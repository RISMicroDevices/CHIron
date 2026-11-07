#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    struct ReferenceResult {
        LineData data{};
        bool     write = false;
    };

    void SetInteger(LineData& data, size_t offset, size_t bytes, uint64_t value)
    {
        for (size_t index = 0; index < bytes; ++index)
            data[offset + index] = uint8_t(value >> (index * 8));
    }

    uint64_t ReadInteger(const LineData& data, size_t offset, size_t bytes, bool bigEndian)
    {
        uint64_t value = 0;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t shift = (bigEndian ? bytes - 1 - index : index) * 8;
            value |= uint64_t(data[offset + index]) << shift;
        }
        return value;
    }

    void WriteInteger(LineData& data, size_t offset, size_t bytes, uint64_t value, bool bigEndian)
    {
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t shift = (bigEndian ? bytes - 1 - index : index) * 8;
            data[offset + index] = uint8_t(value >> shift);
        }
    }

    uint64_t OperandMask(size_t bytes)
    {
        return bytes == 8 ? ~uint64_t(0) : (uint64_t(1) << (bytes * 8)) - 1;
    }

    int64_t SignedInteger(uint64_t value, size_t bytes)
    {
        const uint64_t sign = uint64_t(1) << (bytes * 8 - 1);
        if (!(value & sign))
            return int64_t(value);
        return -1 - int64_t((~value) & OperandMask(bytes));
    }

    ReferenceResult EvaluateAtomic(uint16_t opcode, const LineData& initial, const LineData& operands,
                                   uint64_t address, uint8_t size, bool bigEndian)
    {
        const bool compare = opcode == CHI::Eb::Opcodes::REQ::AtomicCompare;
        const size_t windowBytes = size_t(1) << size;
        const size_t operandBytes = compare ? windowBytes / 2 : windowBytes;
        const size_t swapAddress = (address ^ operandBytes) % LINE_SIZE;
        const uint64_t first = ReadInteger(initial, address % LINE_SIZE, operandBytes, bigEndian);
        const uint64_t second = ReadInteger(operands, address % LINE_SIZE, operandBytes, bigEndian);
        const bool swap = opcode == CHI::Eb::Opcodes::REQ::AtomicSwap;
        const bool load = CHI::Eb::Opcodes::REQ::AtomicLoad::Is(opcode);
        const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(opcode);
        uint64_t result = compare ? ReadInteger(operands, swapAddress, operandBytes, bigEndian) : second;
        if (store || load)
        {
            switch (opcode & 7)
            {
                case 0: result = first + second; break;
                case 1: result = first & ~second; break;
                case 2: result = first ^ second; break;
                case 3: result = first | second; break;
                case 4: result = SignedInteger(first, operandBytes) > SignedInteger(second, operandBytes) ? first : second; break;
                case 5: result = SignedInteger(first, operandBytes) < SignedInteger(second, operandBytes) ? first : second; break;
                case 6: result = first > second ? first : second; break;
                case 7: result = first < second ? first : second; break;
                default: LUNATIC_CHECK(false);
            }
        }
        if (compare && first != second)
            return { initial, false };

        ReferenceResult outcome{ initial, store || load || swap || compare };
        if (outcome.write)
        {
            const size_t writeAddress = address % LINE_SIZE;
            WriteInteger(outcome.data, writeAddress, operandBytes, result, bigEndian);
        }
        return outcome;
    }

    Request GeometryRequest(uint16_t opcode, uint8_t size, uint64_t address, bool bigEndian)
    {
        Request request{};
        request.opcode = opcode;
        request.size = size;
        request.address = address;
        request.endian = bigEndian;
        return request;
    }

    template<typename config>
    typename Driver<config>::DAT OperandPacket(const typename Driver<config>::REQ& request,
                                                const LineData& operands, DataBufferId dbid, size_t beat)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        const auto geometryRequest = GeometryRequest(request.Opcode(), request.Size(), request.Addr(), request.Endian());
        AtomicGeometry geometry;
        LUNATIC_CHECK(GetAtomicGeometry(geometryRequest, geometry));
        const size_t offset = beat * bytes;
        typename Driver<config>::DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = offset / 16;
        data.CCID() = (request.Addr() >> 4) & 3;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            if (lineOffset >= geometry.windowOffset && lineOffset < geometry.windowOffset + geometry.windowBytes)
            {
                data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
                data.Data()[index / 8] |= uint64_t(operands[lineOffset]) << ((index % 8) * 8);
            }
        }
        return data;
    }

    template<typename config>
    void RunIndependentVector(Driver<config>& driver, const LineKey& line, uint16_t opcode, size_t serial,
                              uint64_t address, uint8_t size, bool bigEndian,
                              const LineData& initial, const LineData& operands)
    {
        const bool compare = opcode == CHI::Eb::Opcodes::REQ::AtomicCompare;
        const size_t operandBytes = (size_t(1) << size) / (compare ? 2 : 1);
        const auto expected = EvaluateAtomic(opcode, initial, operands, address, size, bigEndian);
        if (!compare)
            LUNATIC_CHECK(expected.write);
        auto request = driver.Request(opcode, 4, serial % 200 + 1, address);
        request.Size() = size;
        request.Endian() = bigEndian;
        request.AllowRetry() = 0;
        driver.node.GetMemory().WriteFull(line, initial);

        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        Options options;
        options.dataWidth = config::dataWidth;
        const auto geometryRequest = GeometryRequest(opcode, size, address, bigEndian);
        const uint64_t mask = AtomicOperandBeatMask(geometryRequest, options);
        for (size_t beat = 0; beat < LINE_SIZE / (config::dataWidth / 8); ++beat)
        {
            if (mask & (uint64_t(1) << beat))
                LUNATIC_CHECK(driver.node.PushRXDAT(OperandPacket<config>(request, operands, dbid, beat)));
        }
        driver.Tick();
        if (CHI::Eb::Opcodes::REQ::AtomicStore::Is(opcode))
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        else
        {
            const auto result = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            const size_t offset = (address % LINE_SIZE) / (config::dataWidth / 8) * (config::dataWidth / 8);
            LUNATIC_CHECK(result.DataID() == offset / 16 && result.DBID() == dbid);
            for (size_t index = 0; index < config::dataWidth / 8; ++index)
            {
                const size_t lineOffset = offset + index;
                const bool enabled = lineOffset >= address % LINE_SIZE &&
                    lineOffset < address % LINE_SIZE + operandBytes;
                LUNATIC_CHECK(bool((uint64_t(result.BE()) >> index) & 1) == enabled);
                if (enabled)
                    LUNATIC_CHECK(uint8_t(result.Data()[index / 8] >> ((index % 8) * 8)) == initial[lineOffset]);
            }
        }
        driver.Quiesce();
        const auto home = driver.node.InspectHome(line);
        const auto actual = home.valid ? home.data : driver.node.GetMemory().Read(line);
        if (actual != expected.data)
        {
            for (size_t index = 0; index < LINE_SIZE; ++index)
                if (actual[index] != expected.data[index])
                {
                    std::cerr << "independent mismatch opcode=" << opcode << " serial=" << serial << " byte=" << index
                              << " actual=" << unsigned(actual[index]) << " expected=" << unsigned(expected.data[index])
                              << " endian=" << bigEndian << " home=" << home.valid << "\n";
                    break;
                }
        }
        LUNATIC_CHECK(actual == expected.data);
        LUNATIC_CHECK(driver.node.GetErrors().empty());
        if (serial == 1)
        {
            LineData mutation = expected.data;
            mutation[address % LINE_SIZE] ^= 1;
            LUNATIC_CHECK(actual != mutation);
        }
    }

    template<typename config>
    void RunIndependentMatrix(bool bigEndian)
    {
        Driver<config> driver;
        for (size_t operation = 0; operation < 18; ++operation)
        {
            const bool compare = operation == 17;
            const bool swap = operation == 16;
            const bool load = !compare && !swap && operation >= 8;
            const uint16_t opcode = compare ? uint16_t(CHI::Eb::Opcodes::REQ::AtomicCompare)
                : swap ? uint16_t(CHI::Eb::Opcodes::REQ::AtomicSwap)
                : uint16_t((load ? 0x30 : 0x28) | (operation % 8));
            const LineKey line = LineKey::FromAddress(0x6f200 + operation * LINE_SIZE, false);
            const uint64_t address = line.address + (compare ? 0x38 : 0x20);
            const uint8_t size = compare ? 4 : 2;
            LineData initial{};
            LineData operands{};
            initial.fill(0x11);
            SetInteger(initial, address % LINE_SIZE, compare ? 8 : 4, 5);
            SetInteger(operands, address % LINE_SIZE, compare ? 8 : 4, compare ? 5 : 7);
            if (compare)
                SetInteger(operands, (address ^ 8) % LINE_SIZE, 8, 9);
            RunIndependentVector(driver, line, opcode, operation + 1, address, size, bigEndian, initial, operands);
        }
    }

    template<typename config>
    void RunIndependentArithmeticVectors(bool bigEndian)
    {
        Driver<config> driver;
        size_t serial = 0;
        for (size_t operation = 0; operation < 17; ++operation)
            for (size_t bytes : { size_t(1), size_t(2), size_t(4), size_t(8) })
                for (bool upperHalf : { false, true })
                {
                    ++serial;
                    const LineKey line = LineKey::FromAddress(0x71000 + serial * LINE_SIZE, false);
                    const uint64_t address = line.address + 0x20 + (upperHalf ? bytes : 0);
                    const uint8_t size = bytes == 1 ? 0 : bytes == 2 ? 1 : bytes == 4 ? 2 : 3;
                    const uint64_t sign = uint64_t(1) << (bytes * 8 - 1);
                    uint64_t memory = sign;
                    uint64_t transmitted = 1;
                    switch (operation % 8)
                    {
                        case 0:
                            memory = OperandMask(bytes);
                            break;
                        case 1:
                        case 2:
                        case 3:
                            memory = 0x55;
                            transmitted = 0x0f;
                            break;
                        default:
                            break;
                    }
                    if (operation == 16)
                    {
                        memory = 0x55;
                        transmitted = OperandMask(bytes) & uint64_t(0xfeed);
                    }
                    LineData initial{};
                    LineData operands{};
                    initial.fill(0x11);
                    operands.fill(0x19);
                    WriteInteger(initial, address % LINE_SIZE, bytes, memory, bigEndian);
                    WriteInteger(operands, address % LINE_SIZE, bytes, transmitted, bigEndian);
                    const bool load = operation >= 8;
                    const uint16_t opcode = operation == 16 ? uint16_t(CHI::Eb::Opcodes::REQ::AtomicSwap)
                        : uint16_t((load ? 0x30 : 0x28) | (operation % 8));
                    RunIndependentVector(driver, line, opcode, serial, address, size, bigEndian, initial, operands);
                }

        for (size_t bytes : { size_t(1), size_t(2), size_t(4), size_t(8), size_t(16) })
            for (bool upperHalf : { false, true })
                for (bool match : { false, true })
                {
                    ++serial;
                    const LineKey line = LineKey::FromAddress(0x71000 + serial * LINE_SIZE, false);
                    const size_t offset = 0x20 + (upperHalf ? bytes : 0);
                    const uint64_t address = line.address + offset;
                    const size_t swapAddress = (address ^ bytes) % LINE_SIZE;
                    const uint8_t size = bytes == 1 ? 1 : bytes == 2 ? 2 : bytes == 4 ? 3 : bytes == 8 ? 4 : 5;
                    const uint64_t compareValue = uint64_t(0x66) & OperandMask(bytes);
                    LineData initial{};
                    LineData operands{};
                    initial.fill(0x11);
                    operands.fill(0x19);
                    WriteInteger(initial, offset, bytes, compareValue, bigEndian);
                    WriteInteger(operands, offset, bytes, match ? compareValue : compareValue ^ 1, bigEndian);
                    WriteInteger(initial, swapAddress, bytes, uint64_t(0x44), bigEndian);
                    WriteInteger(operands, swapAddress, bytes, uint64_t(0x99), bigEndian);
                    RunIndependentVector(driver, line, CHI::Eb::Opcodes::REQ::AtomicCompare, serial,
                                         address, size, bigEndian, initial, operands);
                }
    }

}

LUNATIC_TEST(Stage9AtomicIndependentMatrix128)
{
    RunIndependentMatrix<Config128>(false);
    RunIndependentMatrix<Config128>(true);
    RunIndependentArithmeticVectors<Config128>(false);
    RunIndependentArithmeticVectors<Config128>(true);
}

LUNATIC_TEST(Stage9AtomicIndependentMatrix256)
{
    RunIndependentMatrix<Config256>(false);
    RunIndependentMatrix<Config256>(true);
    RunIndependentArithmeticVectors<Config256>(false);
    RunIndependentArithmeticVectors<Config256>(true);
}

LUNATIC_TEST(Stage9AtomicIndependentMatrix512)
{
    RunIndependentMatrix<Config512>(false);
    RunIndependentMatrix<Config512>(true);
    RunIndependentArithmeticVectors<Config512>(false);
    RunIndependentArithmeticVectors<Config512>(true);
}
