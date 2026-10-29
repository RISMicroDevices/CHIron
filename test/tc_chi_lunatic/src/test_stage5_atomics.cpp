#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void SetInteger(LineData& data, size_t offset, size_t bytes, bool bigEndian, uint64_t value)
    {
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t shift = (bigEndian ? bytes - 1 - index : index) * 8;
            data[offset + index] = uint8_t(value >> shift);
        }
    }

    uint64_t GetInteger(const LineData& data, size_t offset, size_t bytes, bool bigEndian)
    {
        uint64_t value = 0;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t shift = (bigEndian ? bytes - 1 - index : index) * 8;
            value |= uint64_t(data[offset + index]) << shift;
        }
        return value;
    }

    Request AtomicRequest(uint16_t opcode, uint8_t size, uint64_t address, bool bigEndian = false)
    {
        Request request{};
        request.opcode = opcode;
        request.size = size;
        request.address = address;
        request.endian = bigEndian;
        return request;
    }

    void RunALUOperation(uint8_t operation, bool load, size_t bytes, bool bigEndian)
    {
        const uint16_t opcode = (load ? 0x30 : 0x28) | operation;
        const size_t size = bytes == 1 ? 0 : bytes == 2 ? 1 : bytes == 4 ? 2 : 3;
        const size_t address = 0x28;
        Request request = AtomicRequest(opcode, size, address, bigEndian);
        LineData initial{};
        LineData operands{};
        initial.fill(0x11);
        const unsigned bits = unsigned(bytes * 8);
        const uint64_t sign = uint64_t(1) << (bits - 1);
        uint64_t memory = sign;
        uint64_t transmitted = 1;
        uint64_t expected = 0;
        switch (operation)
        {
            case 0:
                memory = bits == 64 ? ~uint64_t(0) : (uint64_t(1) << bits) - 1;
                expected = 0;
                break;
            case 1:
                memory = 0x55;
                transmitted = 0x0f;
                expected = 0x50;
                break;
            case 2:
                memory = 0x55;
                transmitted = 0x0f;
                expected = 0x5a;
                break;
            case 3:
                memory = 0x55;
                transmitted = 0x0f;
                expected = 0x5f;
                break;
            case 4: expected = 1; break;
            case 5: expected = sign; break;
            case 6: expected = sign; break;
            case 7: expected = 1; break;
            default: LUNATIC_CHECK(false);
        }
        SetInteger(initial, address, bytes, bigEndian, memory);
        SetInteger(operands, address, bytes, bigEndian, transmitted);
        const auto result = ExecuteAtomic(request, initial, operands);
        LUNATIC_CHECK(result.write);
        LUNATIC_CHECK(GetInteger(result.data, address, bytes, bigEndian) == expected);
        LUNATIC_CHECK(result.data[0] == initial[0]);
        LUNATIC_CHECK(result.data[LINE_SIZE - 1] == initial[LINE_SIZE - 1]);
    }

}

LUNATIC_TEST(Stage5AtomicALUAllOperationsAndWidths)
{
    for (uint8_t operation = 0; operation < 8; ++operation)
        for (bool load : { false, true })
            for (size_t bytes : { size_t(1), size_t(2), size_t(4), size_t(8) })
                for (bool bigEndian : { false, true })
                    RunALUOperation(operation, load, bytes, bigEndian);

    for (size_t bytes : { size_t(1), size_t(2), size_t(4), size_t(8) })
    {
        const uint8_t size = bytes == 1 ? 0 : bytes == 2 ? 1 : bytes == 4 ? 2 : 3;
        const uint64_t expected = bytes == 8 ? 0xfeed : 0xfeed & ((uint64_t(1) << (bytes * 8)) - 1);
        Request request = AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicSwap, size, 0x28);
        LineData initial{};
        LineData operands{};
        initial.fill(0x11);
        SetInteger(operands, 0x28, bytes, false, 0xfeed);
        const auto result = ExecuteAtomic(request, initial, operands);
        LUNATIC_CHECK(result.write);
        LUNATIC_CHECK(GetInteger(result.data, 0x28, bytes, false) == expected);
    }
}

LUNATIC_TEST(Stage5AtomicComparePlacementMatchAndMismatch)
{
    for (size_t bytes : { size_t(1), size_t(2), size_t(4), size_t(8), size_t(16) })
    {
        const uint8_t size = bytes == 1 ? 1 : bytes == 2 ? 2 : bytes == 4 ? 3 : bytes == 8 ? 4 : 5;
        const size_t address = 0x20 + bytes;
        const size_t swapAddress = address ^ bytes;
        Request request = AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicCompare, size, address);
        LineData initial{};
        LineData operands{};
        initial.fill(0x22);
        for (size_t index = 0; index < bytes; ++index)
        {
            initial[address + index] = 0x66;
            initial[swapAddress + index] = 0x44;
            operands[address + index] = 0x66;
            operands[swapAddress + index] = 0x99;
        }
        const auto match = ExecuteAtomic(request, initial, operands);
        LUNATIC_CHECK(match.write);
        for (size_t index = 0; index < bytes; ++index)
        {
            LUNATIC_CHECK(match.data[swapAddress + index] == 0x99);
            LUNATIC_CHECK(match.data[address + index] == 0x66);
        }
        LUNATIC_CHECK(match.data[0] == initial[0]);

        operands[address] ^= 1;
        const auto mismatch = ExecuteAtomic(request, initial, operands);
        LUNATIC_CHECK(!mismatch.write);
        LUNATIC_CHECK(mismatch.data == initial);
    }
}

LUNATIC_TEST(Stage5AtomicGeometryAndBeatMasks)
{
    AtomicGeometry geometry;
    LUNATIC_CHECK(!GetAtomicGeometry(AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 0x20), geometry));
    LUNATIC_CHECK(!GetAtomicGeometry(AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicCompare, 0, 0x20), geometry));
    LUNATIC_CHECK(!GetAtomicGeometry(AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicCompare, 6, 0x20), geometry));
    LUNATIC_CHECK(!GetAtomicGeometry(AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicSwap, 2, 0x21), geometry));
    LUNATIC_CHECK(GetAtomicGeometry(AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicCompare, 5, 0x30), geometry));
    LUNATIC_CHECK(geometry.windowBytes == 32 && geometry.operandBytes == 16 && geometry.windowOffset == 0x20);

    const Request request = AtomicRequest(CHI::Eb::Opcodes::REQ::AtomicCompare, 5, 0x30);
    Options options;
    options.dataWidth = 128;
    LUNATIC_CHECK(AtomicOperandBeatMask(request, options) == 0b1100);
    LUNATIC_CHECK(AtomicResultBeatMask(request, options) == 0b1000);
    options.dataWidth = 256;
    LUNATIC_CHECK(AtomicOperandBeatMask(request, options) == 0b0010);
    LUNATIC_CHECK(AtomicResultBeatMask(request, options) == 0b0010);
    options.dataWidth = 512;
    LUNATIC_CHECK(AtomicOperandBeatMask(request, options) == 0b0001);
    LUNATIC_CHECK(AtomicResultBeatMask(request, options) == 0b0001);
}
