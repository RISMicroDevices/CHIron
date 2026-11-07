#include "oracle_driver.hpp"

#include <algorithm>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void SetFlowInteger(LineData& data, size_t offset, size_t bytes, uint64_t value)
    {
        for (size_t index = 0; index < bytes; ++index)
            data[offset + index] = uint8_t(value >> (index * 8));
    }

    Request FlowRequest(uint16_t opcode, uint8_t size, uint64_t address)
    {
        Request request{};
        request.opcode = opcode;
        request.size = size;
        request.address = address;
        return request;
    }

    template<typename config>
    typename Driver<config>::DAT AtomicDAT(const typename Driver<config>::REQ& request, DataBufferId dbid,
                                           const LineData& operands, size_t dataId)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        const Request geometryRequest = FlowRequest(request.Opcode(), request.Size(), request.Addr());
        AtomicGeometry geometry;
        LUNATIC_CHECK(GetAtomicGeometry(geometryRequest, geometry));
        const size_t offset = dataId * 16;
        LUNATIC_CHECK(offset + bytes <= LINE_SIZE && offset % bytes == 0);

        typename Driver<config>::DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.HomeNID() = 0;
        data.TxnID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = dataId;
        data.CCID() = (request.Addr() >> 4) & 3;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            const bool enabled = lineOffset >= geometry.windowOffset &&
                lineOffset < geometry.windowOffset + geometry.windowBytes;
            if (enabled)
            {
                data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
                data.Data()[index / 8] |= uint64_t(operands[lineOffset]) << ((index % 8) * 8);
            }
        }
        return data;
    }

    template<typename config>
    void SendAtomicOperands(CheckedDriver<config>& driver, const typename Driver<config>::REQ& request,
                            DataBufferId dbid, const LineData& operands, bool reverse = false)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        Options options;
        options.dataWidth = config::dataWidth;
        const Request geometryRequest = FlowRequest(request.Opcode(), request.Size(), request.Addr());
        const uint64_t mask = AtomicOperandBeatMask(geometryRequest, options);
        std::vector<size_t> beats;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            if (mask & (uint64_t(1) << beat))
                beats.push_back(beat);
        if (reverse)
            std::reverse(beats.begin(), beats.end());
        for (size_t beat : beats)
        {
            auto data = AtomicDAT<config>(request, dbid, operands, beat * bytes / 16);
            driver.Oracle(4).WriteData(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
    }

    template<typename config>
    void DriveAtomic(CheckedDriver<config>& driver, const typename Driver<config>::REQ& request,
                     const LineData& operands, const LineData& oldData, std::optional<NodeId> peer = std::nullopt,
                     bool dirtyPeer = false, bool reverse = false)
    {
        const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(request.Opcode());
        driver.PushRequest(request);
        if (peer)
        {
            auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
            LUNATIC_CHECK(snoop.target == *peer && snoop.flit.RetToSrc() == 0);
            if (dirtyPeer)
                driver.SnoopData(*peer, snoop.flit.TxnID(), oldData, CHI::Eb::Resps::SnpRespData_I_PD,
                    false, (request.Addr() >> 4) & 3);
            else
                driver.SnoopResponse(*peer, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        }

        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const uint16_t firstOpcode = driver.driver.node.PeekTXRSP()->Opcode();
        bool compComplete = !store;
        DataBufferId dbid = 0;
        if (firstOpcode == CHI::Eb::Opcodes::RSP::Comp)
        {
            LUNATIC_CHECK(store);
            LUNATIC_CHECK(driver.NextRSP(firstOpcode).TxnID() == request.TxnID());
            compComplete = true;
            dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        }
        else if (firstOpcode == CHI::Eb::Opcodes::RSP::DBIDResp)
            dbid = driver.NextRSP(firstOpcode).DBID();
        else
        {
            LUNATIC_CHECK(firstOpcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
            dbid = driver.NextRSP(firstOpcode).DBID();
            compComplete = true;
        }
        SendAtomicOperands(driver, request, dbid, operands, reverse);
        if (store && !compComplete)
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == request.TxnID());

        if (!store)
        {
            auto result = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            const Request geometryRequest = FlowRequest(request.Opcode(), request.Size(), request.Addr());
            AtomicGeometry geometry;
            LUNATIC_CHECK(GetAtomicGeometry(geometryRequest, geometry));
            constexpr size_t bytes = config::dataWidth / 8;
            const size_t address = request.Addr() % LINE_SIZE;
            const size_t offset = address / bytes * bytes;
            LUNATIC_CHECK(result.TgtID() == request.SrcID() && result.TxnID() == request.TxnID());
            LUNATIC_CHECK(result.DBID() == dbid && result.DataID() == offset / 16);
            LUNATIC_CHECK(result.CCID() == ((request.Addr() >> 4) & 3));
            for (size_t index = 0; index < bytes; ++index)
            {
                const size_t lineOffset = offset + index;
                const bool enabled = lineOffset >= address && lineOffset < address + geometry.operandBytes;
                LUNATIC_CHECK(bool((uint64_t(result.BE()) >> index) & 1) == enabled);
                if (enabled)
                    LUNATIC_CHECK(uint8_t(result.Data()[index / 8] >> ((index % 8) * 8)) == oldData[lineOffset]);
            }
        }
        driver.Quiesce();
    }

    template<typename config>
    void CheckAtomicLine(CheckedDriver<config>& driver, const LineKey& line, const LineData& expected)
    {
        const auto home = driver.driver.node.InspectHome(line);
        if (home.valid)
            LUNATIC_CHECK(home.data == expected);
        else
            LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == expected);
    }

    template<typename config>
    void RunAtomicMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.outputQueueLimit = 1;
        CheckedDriver<config> driver(options);
        for (size_t index = 0; index < 18; ++index)
        {
            const bool compare = index == 17;
            const bool swap = index == 16;
            const bool load = !compare && !swap && index >= 8;
            const uint8_t operation = compare || swap ? 0 : index % 8;
            const uint16_t opcode = compare ? uint16_t(CHI::Eb::Opcodes::REQ::AtomicCompare)
                : swap ? uint16_t(CHI::Eb::Opcodes::REQ::AtomicSwap) : uint16_t((load ? 0x30 : 0x28) | operation);
            const uint8_t size = compare ? 4 : 2;
            const LineKey line = LineKey::FromAddress(0x50000 + index * LINE_SIZE, false);
            const uint64_t address = line.address + (compare ? 0x38 : 0x20);
            auto request = driver.Request(opcode, 4, index + 1, address);
            request.Size() = size;

            LineData initial{};
            LineData operands{};
            initial.fill(0x13);
            SetFlowInteger(initial, address % LINE_SIZE, compare ? 8 : 4, 5);
            SetFlowInteger(operands, address % LINE_SIZE, compare ? 8 : 4, compare ? 5 : 7);
            if (compare)
                SetFlowInteger(operands, (address ^ 8) % LINE_SIZE, 8, 9);
            driver.driver.node.GetMemory().WriteFull(line, initial);
            const Request executionRequest = FlowRequest(opcode, size, address);
            AtomicExecution expected = compare ? AtomicExecution{ initial, true }
                : ExecuteAtomic(executionRequest, initial, operands);
            if (compare)
                for (size_t index = 0; index < 8; ++index)
                    expected.data[address % LINE_SIZE + index] = operands[(address ^ 8) % LINE_SIZE + index];
            LUNATIC_CHECK(expected.write);
            DriveAtomic(driver, request, operands, initial);
            CheckAtomicLine(driver, line, expected.data);
        }
    }

    template<typename config>
    void SeedDirtyPeer(CheckedDriver<config>& driver, const LineKey& line, const LineData& data)
    {
        ByteEnables enables{};
        enables.fill(1);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
        auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

    void RunDirtyAtomic(PolicyKind policy, bool mismatch)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(mismatch ? 0x50100 : 0x50200, false);
        LineData initial{};
        for (size_t index = 0; index < initial.size(); ++index)
            initial[index] = uint8_t(17 + index * 3);
        SeedDirtyPeer(driver, line, initial);
        const uint64_t address = line.address + 0x20;
        const uint16_t opcode = mismatch ? CHI::Eb::Opcodes::REQ::AtomicCompare : CHI::Eb::Opcodes::REQ::AtomicLoad::ADD;
        auto request = driver.Request(opcode, 4, 100, address);
        request.Size() = mismatch ? 4 : 2;
        LineData operands = initial;
        if (mismatch)
        {
            SetFlowInteger(operands, 0x20, 8, 7);
            SetFlowInteger(operands, 0x28, 8, 9);
            operands[0x20] ^= 1;
        }
        else
            SetFlowInteger(operands, 0x20, 4, 7);
        const Request executionRequest = FlowRequest(opcode, request.Size(), address);
        const auto expected = ExecuteAtomic(executionRequest, initial, operands);
        LUNATIC_CHECK(expected.write != mismatch);
        DriveAtomic(driver, request, operands, initial, 6, true);
        CheckAtomicLine(driver, line, expected.data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    }

}

LUNATIC_TEST(Stage5AtomicInternalMatrix128)
{
    RunAtomicMatrix<Config128>(PolicyKind::NINE);
    RunAtomicMatrix<Config128>(PolicyKind::Exclusive);
    RunAtomicMatrix<Config128>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5AtomicInternalMatrix256)
{
    RunAtomicMatrix<Config256>(PolicyKind::NINE);
    RunAtomicMatrix<Config256>(PolicyKind::Exclusive);
    RunAtomicMatrix<Config256>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5AtomicInternalMatrix512)
{
    RunAtomicMatrix<Config512>(PolicyKind::NINE);
    RunAtomicMatrix<Config512>(PolicyKind::Exclusive);
    RunAtomicMatrix<Config512>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5AtomicDirtyInterventionAndCompareMismatch)
{
    RunDirtyAtomic(PolicyKind::NINE, false);
    RunDirtyAtomic(PolicyKind::NINE, true);
    RunDirtyAtomic(PolicyKind::Exclusive, false);
    RunDirtyAtomic(PolicyKind::Exclusive, true);
    RunDirtyAtomic(PolicyKind::Inclusive, false);
    RunDirtyAtomic(PolicyKind::Inclusive, true);
}

LUNATIC_TEST(Stage5AtomicCompare128AcceptsReversedPackets)
{
    CheckedDriver<Config128> driver;
    const LineKey line = LineKey::FromAddress(0x50300, false);
    const uint64_t address = line.address + 0x30;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicCompare, 4, 1, address);
    request.Size() = 5;
    LineData initial{};
    LineData operands{};
    for (size_t index = 0; index < 16; ++index)
    {
        initial[0x30 + index] = 5;
        operands[0x30 + index] = 5;
        operands[0x20 + index] = 9;
    }
    driver.driver.node.GetMemory().WriteFull(line, initial);
    DriveAtomic(driver, request, operands, initial, std::nullopt, false, true);
    const Request executionRequest = FlowRequest(request.Opcode(), request.Size(), address);
    CheckAtomicLine(driver, line, ExecuteAtomic(executionRequest, initial, operands).data);
}

LUNATIC_TEST(Stage5AtomicRejectsDataBeforeDBIDAndInvalidGeometry)
{
    {
        Driver<Config256> driver;
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicLoad::ADD, 4, 1, 0x50400);
        request.Size() = 2;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });
        LineData operands{};
        auto data = AtomicDAT<Config256>(request, 0, operands, 0);
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
    {
        Driver<Config256> driver;
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 2, 0x50400);
        request.Size() = 4;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.RespErr() == 0b10);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
}
