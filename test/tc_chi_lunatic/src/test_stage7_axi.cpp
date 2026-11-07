#include "common.hpp"
#include "axi_model.hpp"

#include <array>
#include <bit>
#include <string>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    class AXIBridgeEndpoint {
    public:
        explicit AXIBridgeEndpoint(Bridge<config>& bridge)
            : bridge(bridge)
        {
        }

        std::optional<AXIAddress> PeekAW() const { return bridge.PeekDownstreamAW(); }
        std::optional<AXIWriteData> PeekW() const { return bridge.PeekDownstreamW(); }
        std::optional<AXIAddress> PeekAR() const { return bridge.PeekDownstreamAR(); }
        std::optional<AXIAddress> PopAW() { return bridge.PopDownstreamAW(); }
        std::optional<AXIWriteData> PopW() { return bridge.PopDownstreamW(); }
        std::optional<AXIAddress> PopAR() { return bridge.PopDownstreamAR(); }
        bool PushB(const AXIWriteResponse& response) { return bridge.PushDownstreamB(response); }
        bool PushR(const AXIReadData& response) { return bridge.PushDownstreamR(response); }

    private:
        Bridge<config>& bridge;
    };

    template<typename config>
    class AXIBridgeDriver {
    public:
        using Node = Bridge<config>;
        using REQ = typename Node::REQFlit;
        using RSP = typename Node::RSPFlit;
        using DAT = typename Node::DATFlit;
        using SNP = typename Node::SNPEmission;

        Node              bridge;
        AXIModel          memory;
        AXIBridgeEndpoint<config> endpoint;
        uint64_t          now = 0;

        explicit AXIBridgeDriver(Options options = {})
            : bridge(2, [&options] {
                options.dataWidth = config::dataWidth;
                options.backend = BackendKind::AXI;
                return options;
            }())
            , memory(config::dataWidth / 8)
            , endpoint(bridge)
        {
        }

        void Tick()
        {
            memory.Tick(endpoint);
            bridge.Tick(++now);
        }

        template<typename Predicate>
        void Until(Predicate&& predicate, size_t limit = 1000)
        {
            for (size_t cycle = 0; cycle < limit; ++cycle)
            {
                if (predicate())
                    return;
                Tick();
            }
            std::string message = "AXI bridge condition timed out at cycle " + std::to_string(now);
            for (const auto& error : bridge.GetNode().GetErrors())
                message += "; " + error.reason;
            throw std::runtime_error(message);
        }

        void Quiesce(bool checkErrors = true)
        {
            Until([&] { return bridge.Drain() && memory.Drain(); });
            LUNATIC_CHECK(memory.Errors().empty());
            if (checkErrors)
                LUNATIC_CHECK(bridge.GetNode().GetErrors().empty());
            LUNATIC_CHECK(bridge.GetNode().ValidateInvariants());
        }

        REQ Request(uint16_t opcode, NodeId source, TransactionId txnId, uint64_t address,
                    bool compAck = false) const
        {
            REQ request{};
            request.SrcID() = source;
            request.TgtID() = 2;
            request.TxnID() = txnId;
            request.Opcode() = opcode;
            request.Addr() = address;
            request.Size() = 6;
            request.AllowRetry() = 1;
            request.ExpCompAck() = compAck;
            request.SnpAttr() = IsCoherentRequest(opcode) || opcode == CHI::Eb::Opcodes::REQ::ReadOnce;
            request.MemAttr() = IsCoherentRequest(opcode) || opcode == CHI::Eb::Opcodes::REQ::ReadOnce
                ? CHI::Eb::MemAttrs::WriteBack_Allocate : CHI::Eb::MemAttrs::NonCacheable_Bufferable;
            return request;
        }

        void PushRequest(const REQ& request)
        {
            LUNATIC_CHECK(bridge.PushUpstreamREQ(request));
        }

        RSP NextRSP(uint16_t opcode)
        {
            Until([&] { return static_cast<bool>(bridge.PeekUpstreamRSP()); });
            const auto first = bridge.PeekUpstreamRSP();
            const auto response = bridge.PopUpstreamRSP();
            LUNATIC_CHECK(response && first);
            LUNATIC_CHECK(response->Opcode() == first->Opcode());
            LUNATIC_CHECK(response->TxnID() == first->TxnID());
            LUNATIC_CHECK(response->Opcode() == opcode);
            return *response;
        }

        DAT NextDAT(uint16_t opcode = CHI::Eb::Opcodes::DAT::CompData)
        {
            Until([&] { return static_cast<bool>(bridge.PeekUpstreamDAT()); });
            const auto first = bridge.PeekUpstreamDAT();
            const auto data = bridge.PopUpstreamDAT();
            LUNATIC_CHECK(data && first);
            LUNATIC_CHECK(data->Opcode() == first->Opcode() && data->DataID() == first->DataID());
            LUNATIC_CHECK(data->Opcode() == opcode);
            return *data;
        }

        SNP NextSNP(uint16_t opcode)
        {
            Until([&] { return static_cast<bool>(bridge.PeekUpstreamSNP()); });
            auto snoop = bridge.PopUpstreamSNP();
            LUNATIC_CHECK(snoop && snoop->flit.Opcode() == opcode);
            return *snoop;
        }

        void CompAck(NodeId source, DataBufferId dbid)
        {
            RSP response{};
            response.SrcID() = source;
            response.TgtID() = 2;
            response.TxnID() = dbid;
            response.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
            LUNATIC_CHECK(bridge.PushUpstreamRSP(response));
            Tick();
        }

        void WriteData(NodeId source, DataBufferId dbid, const LineData& data, const ByteEnables& enables,
                       uint16_t opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData, uint8_t resp = 0,
                       uint64_t beatMask = ~uint64_t(0), uint8_t ccid = 0)
        {
            constexpr size_t bytes = config::dataWidth / 8;
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                if (!(beatMask & (uint64_t(1) << beat)))
                    continue;
                DAT flit{};
                flit.SrcID() = source;
                flit.TgtID() = 2;
                flit.TxnID() = dbid;
                flit.Opcode() = opcode;
                flit.Resp() = resp;
                flit.DataID() = beat * bytes / 16;
                flit.CCID() = ccid;
                for (size_t index = 0; index < bytes; ++index)
                {
                    if (enables[beat * bytes + index])
                    {
                        flit.Data()[index / 8] |= uint64_t(data[beat * bytes + index]) << ((index % 8) * 8);
                        flit.BE() = uint64_t(flit.BE()) | (uint64_t(1) << index);
                    }
                }
                LUNATIC_CHECK(bridge.PushUpstreamDAT(flit));
            }
            Tick();
        }

        void SnoopData(NodeId source, TransactionId txnId, const LineData& data, uint8_t resp,
                       bool partial = false, uint8_t ccid = 0)
        {
            constexpr size_t bytes = config::dataWidth / 8;
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                DAT flit{};
                flit.SrcID() = source;
                flit.TgtID() = 2;
                flit.TxnID() = txnId;
                flit.Opcode() = partial ? CHI::Eb::Opcodes::DAT::SnpRespDataPtl : CHI::Eb::Opcodes::DAT::SnpRespData;
                flit.Resp() = resp;
                flit.DataID() = beat * bytes / 16;
                flit.CCID() = ccid;
                for (size_t index = 0; index < bytes; ++index)
                {
                    if (!partial || index < bytes / 2)
                    {
                        flit.Data()[index / 8] |= uint64_t(data[beat * bytes + index]) << ((index % 8) * 8);
                        flit.BE() = uint64_t(flit.BE()) | (uint64_t(1) << index);
                    }
                }
                LUNATIC_CHECK(bridge.PushUpstreamDAT(flit));
            }
            Tick();
        }

        void Write(uint16_t opcode, TransactionId txnId, uint64_t address, const LineData& data,
                   const ByteEnables& enables)
        {
            PushRequest(Request(opcode, 4, txnId, address));
            const uint16_t first = IsCopybackRequest(opcode) ? CHI::Eb::Opcodes::RSP::CompDBIDResp
                                                             : CHI::Eb::Opcodes::RSP::DBIDResp;
            const DataBufferId dbid = NextRSP(first).DBID();
            const uint16_t dataOpcode = IsCopybackRequest(opcode) ? CHI::Eb::Opcodes::DAT::CopyBackWrData
                                                                  : CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
            const uint8_t resp = IsCopybackRequest(opcode) ? uint8_t(CHI::Eb::Resps::CopyBackWrData_UD_PD) : uint8_t(0);
            WriteData(4, dbid, data, enables, dataOpcode, resp, ~uint64_t(0), (address >> 4) & 3);
            if (!IsCopybackRequest(opcode))
                NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        }

        LineData Read(const REQ& request, size_t beats = LINE_SIZE / (config::dataWidth / 8), uint8_t* error = nullptr)
        {
            PushRequest(request);
            LineData result{};
            DataBufferId dbid = 0;
            for (size_t beat = 0; beat < beats; ++beat)
            {
                const DAT data = NextDAT();
                const size_t offset = size_t(data.DataID()) * 16;
                dbid = data.DBID();
                if (error)
                    *error = data.RespErr();
                for (size_t index = 0; index < config::dataWidth / 8; ++index)
                    result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
            }
            if (request.ExpCompAck())
                CompAck(request.SrcID(), dbid);
            return result;
        }
    };

    LineData AXIPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 11);
        return data;
    }

    ByteEnables FullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    void CheckReadWidth(uint64_t address)
    {
        AXIBridgeDriver<config> driver;
        const LineData data = AXIPattern(uint8_t(config::dataWidth / 8));
        driver.memory.SetLine(address, data);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, address);
        LUNATIC_CHECK(driver.Read(request) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.memory.CompletedReads().size() == 1);
        const auto& read = driver.memory.CompletedReads().front();
        LUNATIC_CHECK(read.address.address == address);
        LUNATIC_CHECK(read.address.len + 1 == LINE_SIZE / (config::dataWidth / 8));
        LUNATIC_CHECK(read.address.size == std::countr_zero(config::dataWidth / 8));
        LUNATIC_CHECK(read.address.prot == 0);
        LUNATIC_CHECK(read.address.cache == AXIReadCache(request.MemAttr()));
        LUNATIC_CHECK(!driver.bridge.PeekDownstreamREQ());
        LUNATIC_CHECK(!driver.bridge.PeekDownstreamDAT());
    }

    template<typename config>
    void CheckDeviceReadGeometry(uint64_t address, uint8_t size)
    {
        AXIBridgeDriver<config> driver;
        const LineData data = AXIPattern(uint8_t(config::dataWidth / 2));
        driver.memory.SetLine(address, data);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 60 + size, address);
        request.Size() = size;
        request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
        request.SnpAttr() = 0;

        const size_t busBytes = config::dataWidth / 8;
        const size_t transferBytes = size_t(1) << size;
        const size_t rawOffset = address % transferBytes;
        const size_t validBytes = transferBytes - rawOffset;
        const size_t axiBytes = std::min(busBytes, transferBytes);
        const size_t axiBeats = (validBytes + axiBytes - 1) / axiBytes;
        const size_t chiBeats = (transferBytes + busBytes - 1) / busBytes;
        const LineData result = driver.Read(request, chiBeats);
        const size_t lineOffset = address % LINE_SIZE;
        for (size_t index = 0; index < LINE_SIZE; ++index)
        {
            const bool valid = index >= lineOffset && index < lineOffset + validBytes;
            LUNATIC_CHECK(result[index] == (valid ? data[index] : 0));
        }
        driver.Quiesce();

        LUNATIC_CHECK(driver.memory.CompletedReads().size() == 1);
        const auto& read = driver.memory.CompletedReads().front();
        LUNATIC_CHECK(driver.memory.AccessedReadAddresses().size() == validBytes);
        LUNATIC_CHECK(read.address.address == address);
        LUNATIC_CHECK(read.address.size == std::countr_zero(axiBytes));
        LUNATIC_CHECK(size_t(read.address.len) + 1 == axiBeats);
        LUNATIC_CHECK(read.beats.size() == axiBeats);
        for (size_t index = 0; index < validBytes; ++index)
            LUNATIC_CHECK(driver.memory.AccessedReadAddresses()[index] == address + index);
    }

    template<typename config>
    void CheckDeviceReadSweep(uint64_t base)
    {
        for (uint8_t size = 0; size <= 6; ++size)
        {
            const size_t transferBytes = size_t(1) << size;
            for (const size_t offset : { size_t(0), transferBytes / 3, transferBytes - 1 })
                CheckDeviceReadGeometry<config>(base + offset, size);
        }
    }

    template<typename config>
    void CheckCoherentWidth(uint64_t address)
    {
        AXIBridgeDriver<config> driver;
        const LineData data = AXIPattern(uint8_t(config::dataWidth / 4));
        driver.memory.SetLine(address, data);
        driver.memory.SetLine(address + LINE_SIZE, data);
        LUNATIC_CHECK(driver.Read(driver.Request(CHI::Eb::Opcodes::REQ::ReadOnce, 4, 20, address, true)) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.Read(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 21, address + LINE_SIZE, true)) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.bridge.GetNode().InspectRequester(LineKey::FromAddress(address + LINE_SIZE, false), 5) == CacheState::UC);
        LUNATIC_CHECK(driver.memory.CompletedReads().size() == 2);
    }

    template<typename config>
    void CheckNarrowWriteUniqueBaseline(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        AXIBridgeDriver<config> driver(options);
        const uint64_t address = 0x90300;
        const LineKey line = LineKey::FromAddress(address, false);
        const LineData initial = AXIPattern(23);
        const LineData update = AXIPattern(181);
        driver.memory.SetLine(address, initial);

        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniquePtl, 4, 1, address);
        request.Size() = 4;
        driver.PushRequest(request);
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        ByteEnables enables{};
        for (size_t index = 0; index < 16; ++index)
            enables[index] = 1;
        driver.WriteData(4, dbid, update, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData, 0, 1);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();

        LineData expected = initial;
        for (size_t index = 0; index < 16; ++index)
            expected[index] = update[index];
        const HomeLineInfo home = driver.bridge.GetNode().InspectHome(line);
        LUNATIC_CHECK(home.valid && home.data == expected);
        LUNATIC_CHECK(std::all_of(home.validBytes.begin(), home.validBytes.end(),
            [](uint8_t valid) { return valid != 0; }));
        LUNATIC_CHECK(driver.memory.ReadLine(address) == initial);
        LUNATIC_CHECK(driver.memory.CompletedReads().size() == 1);
        const auto& baseline = driver.memory.CompletedReads().front();
        LUNATIC_CHECK(baseline.address.address == address);
        LUNATIC_CHECK(baseline.address.len + 1 == LINE_SIZE / (config::dataWidth / 8));
        LUNATIC_CHECK(baseline.address.size == std::countr_zero(config::dataWidth / 8));
        LUNATIC_CHECK(baseline.beats.size() == LINE_SIZE / (config::dataWidth / 8));

        LUNATIC_CHECK(driver.Read(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 2, address, true)) == expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.memory.CompletedReads().size() == 1);
    }

}

LUNATIC_TEST(Stage7AXIBridgeWriteUniqueNarrowBaselineAllWidths)
{
    for (PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
    {
        CheckNarrowWriteUniqueBaseline<Config128>(policy);
        CheckNarrowWriteUniqueBaseline<Config256>(policy);
        CheckNarrowWriteUniqueBaseline<Config512>(policy);
    }
}

LUNATIC_TEST(Stage7AXIBridgeReadNoSnpAllWidths)
{
    CheckReadWidth<Config128>(0x90000);
    CheckReadWidth<Config256>(0x90100);
    CheckReadWidth<Config512>(0x90200);
}

LUNATIC_TEST(Stage7AXIBridgeReadOnceAndCoherentMissAllWidths)
{
    CheckCoherentWidth<Config128>(0x90400);
    CheckCoherentWidth<Config256>(0x90500);
    CheckCoherentWidth<Config512>(0x90600);
}

LUNATIC_TEST(Stage7AXIBridgeNarrowReadUsesExactBurst)
{
    AXIBridgeDriver<Config128> driver;
    const uint64_t address = 0x91010;
    const LineData data = AXIPattern(7);
    driver.memory.SetLine(address, data);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, address);
    request.Size() = 5;
    const LineData result = driver.Read(request, 2);
    for (size_t index = 0; index < 32; ++index)
        LUNATIC_CHECK(result[index] == data[index]);
    driver.Quiesce();
    const auto& read = driver.memory.CompletedReads().front();
    LUNATIC_CHECK(read.address.address == 0x91000);
    LUNATIC_CHECK(read.address.len == 1 && read.beats.size() == 2);
}

LUNATIC_TEST(Stage7AXIBridgeDeviceReadsPreserveAddressAndLanes)
{
    CheckDeviceReadSweep<Config128>(0x91200);
    CheckDeviceReadSweep<Config256>(0x91400);
    CheckDeviceReadSweep<Config512>(0x91600);
}

LUNATIC_TEST(Stage7AXIBridgeSparseWriteAndZero)
{
    AXIBridgeDriver<Config256> driver;
    const uint64_t address = 0x92000;
    const LineData initial = AXIPattern(13);
    const LineData update = AXIPattern(101);
    driver.memory.SetLine(address, initial);
    ByteEnables enables{};
    for (size_t index = 3; index < 9; ++index)
        enables[index] = 1;
    enables[17] = 1;
    for (size_t index = 40; index < 48; ++index)
        enables[index] = 1;
    driver.Write(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 3, address, update, enables);
    driver.Quiesce();
    LineData expected = initial;
    for (size_t index = 0; index < LINE_SIZE; ++index)
        if (enables[index])
            expected[index] = update[index];
    LUNATIC_CHECK(driver.memory.ReadLine(address) == expected);
    LUNATIC_CHECK(driver.memory.CompletedWrites().size() == 1);
    LUNATIC_CHECK(driver.memory.CompletedWrites().front().beats.size() == 2);
    LUNATIC_CHECK(driver.memory.CompletedWrites().front().beats.back().last);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpZero, 4, 4, address));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.ReadLine(address) == LineData{});
    LUNATIC_CHECK(driver.memory.CompletedWrites().size() == 2);

    driver.memory.SetLine(address, initial);
    auto uniqueZero = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueZero, 4, 5, address);
    uniqueZero.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    driver.PushRequest(uniqueZero);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.ReadLine(address) == LineData{});
    LUNATIC_CHECK(driver.memory.CompletedWrites().size() == 3);
}

LUNATIC_TEST(Stage7AXIBridgeCopybackDirtyVictimAndAtomic)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    AXIBridgeDriver<Config512> driver(options);
    const uint64_t a = 0x93000;
    const uint64_t b = 0x93040;
    const LineData dataA = AXIPattern(19);
    const LineData dataB = AXIPattern(83);
    driver.Write(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, a, dataA, FullEnables());
    driver.Quiesce();
    driver.Write(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, b, dataB, FullEnables());
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.ReadLine(a) == dataA);
    LUNATIC_CHECK(driver.memory.ReadLine(b) == LineData{});
    LUNATIC_CHECK(driver.bridge.GetNode().InspectHome(LineKey::FromAddress(b, false)).valid);

    driver.memory.SetLine(a, dataA);
    auto atomic = driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 7, a + 8);
    atomic.Size() = 3;
    driver.PushRequest(atomic);
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    LineData operands{};
    ByteEnables enables{};
    for (size_t index = 8; index < 16; ++index)
    {
        operands[index] = uint8_t(0xd0 + index);
        enables[index] = 1;
    }
    driver.WriteData(4, dbid, operands, enables);
    const auto old = driver.NextDAT();
    LUNATIC_CHECK(!driver.memory.CompletedReads().empty());
    LUNATIC_CHECK(driver.memory.CompletedReads().back().address.address == a);
    for (size_t index = 8; index < 16; ++index)
        LUNATIC_CHECK(uint8_t(old.Data()[index / 8] >> ((index % 8) * 8)) == dataA[index]);
    driver.Quiesce();
    LineData expected = dataA;
    for (size_t index = 8; index < 16; ++index)
        expected[index] = operands[index];
    const HomeLineInfo atomicHome = driver.bridge.GetNode().InspectHome(LineKey::FromAddress(a, false));
    LUNATIC_CHECK(atomicHome.valid && atomicHome.data == expected);
    LUNATIC_CHECK(driver.memory.ReadLine(b) == dataB);

    const LineData dataC = AXIPattern(151);
    driver.Write(CHI::Eb::Opcodes::REQ::WriteBackFull, 18, a + 0x80, dataC, FullEnables());
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.ReadLine(a) == expected);
    LUNATIC_CHECK(driver.bridge.GetNode().InspectHome(LineKey::FromAddress(a + 0x80, false)).valid);
}

LUNATIC_TEST(Stage7AXIBridgeAtomicPartialDirtyPreservesBaseline)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    AXIBridgeDriver<Config256> driver(options);
    const uint64_t a = 0x93100;
    const uint64_t b = 0x93140;
    const LineData initial = AXIPattern(37);
    const LineData dirty = AXIPattern(167);
    const LineData replacement = AXIPattern(213);
    ByteEnables partialEnables{};
    LineData merged = initial;
    for (size_t index = 0; index < LINE_SIZE; ++index)
        if (index % 32 < 16)
        {
            partialEnables[index] = 1;
            merged[index] = dirty[index];
        }
    driver.memory.SetLine(a, initial);

    driver.Write(CHI::Eb::Opcodes::REQ::WriteBackPtl, 90, a, dirty, partialEnables);
    driver.Quiesce();
    LUNATIC_CHECK(driver.Read(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 91, a, true)) == merged);
    driver.Quiesce();
    const LineKey line = LineKey::FromAddress(a, false);
    LUNATIC_CHECK(driver.bridge.GetNode().InspectRequester(line, 4) == CacheState::UDP);

    driver.Write(CHI::Eb::Opcodes::REQ::WriteBackFull, 92, b, replacement, FullEnables());
    driver.Quiesce();
    LUNATIC_CHECK(!driver.bridge.GetNode().InspectHome(line).valid);
    LUNATIC_CHECK(driver.bridge.GetNode().InspectRequester(line, 4) == CacheState::UDP);
    LUNATIC_CHECK(driver.memory.ReadLine(a) == initial);

    auto atomic = driver.Request(CHI::Eb::Opcodes::REQ::AtomicLoad::ADD, 5, 100, a + 0x20);
    atomic.Size() = 2;
    driver.PushRequest(atomic);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 4);
    driver.SnoopData(4, snoop.flit.TxnID(), merged, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true,
        ((a + 0x20) >> 4) & 3);
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    LineData operands{};
    ByteEnables enables{};
    for (size_t index = 0x20; index < 0x24; ++index)
    {
        operands[index] = uint8_t(7 >> ((index - 0x20) * 8));
        enables[index] = 1;
    }
    driver.WriteData(5, dbid, operands, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData, 0, 2, 2);
    const auto old = driver.NextDAT();
    LUNATIC_CHECK(old.DataID() == 2);
    for (size_t index = 0; index < 4; ++index)
        LUNATIC_CHECK(uint8_t(old.Data()[index / 8] >> ((index % 8) * 8)) == merged[0x20 + index]);
    driver.Quiesce();

    uint64_t oldValue = 0;
    for (size_t index = 0; index < 4; ++index)
        oldValue |= uint64_t(merged[0x20 + index]) << (index * 8);
    LineData expected = merged;
    for (size_t index = 0; index < 4; ++index)
        expected[0x20 + index] = uint8_t((oldValue + 7) >> (index * 8));
    const HomeLineInfo home = driver.bridge.GetNode().InspectHome(line);
    if (home.valid)
        LUNATIC_CHECK(home.data == expected);
    else
        LUNATIC_CHECK(driver.memory.ReadLine(a) == expected);
    LUNATIC_CHECK(!driver.memory.CompletedReads().empty());
    const auto& baseline = driver.memory.CompletedReads().back();
    LUNATIC_CHECK(baseline.address.address == a);
    LUNATIC_CHECK(baseline.address.len == 1 && baseline.beats.size() == 2);
}

LUNATIC_TEST(Stage7AXIBridgeResponseBackpressureAndReordering)
{
    Options options;
    options.axiOutstandingLimit = 2;
    options.axiIdWidth = 2;
    AXIBridgeDriver<Config256> driver(options);
    const LineData a = AXIPattern(23);
    const LineData b = AXIPattern(97);
    driver.memory.SetLine(0x94000, a);
    driver.memory.SetLine(0x94040, b);
    driver.memory.SetReadResponseOrder({ 1, 0 });
    driver.memory.Stall(AXIChannel::AR, 3);
    driver.memory.Stall(AXIChannel::R, 20);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 8, 0x94000));
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 9, 0x94040));
    LineData first{};
    LineData second{};
    size_t firstBeats = 0;
    size_t secondBeats = 0;
    for (size_t beat = 0; beat < 4; ++beat)
    {
        const auto data = driver.NextDAT();
        LineData& destination = data.TxnID() == 8 ? first : second;
        LUNATIC_CHECK(data.TxnID() == 8 || data.TxnID() == 9);
        for (size_t index = 0; index < 32; ++index)
            destination[size_t(data.DataID()) * 16 + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        firstBeats += data.TxnID() == 8;
        secondBeats += data.TxnID() == 9;
    }
    LUNATIC_CHECK(firstBeats == 2 && secondBeats == 2);
    LUNATIC_CHECK(first == a && second == b);
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.CompletedReads().size() == 2);
    LUNATIC_CHECK(driver.memory.CompletedReads()[0].address.id == 1);
}

LUNATIC_TEST(Stage7AXIBridgeErrorsBecomeNDERR)
{
    AXIBridgeDriver<Config512> driver;
    driver.memory.ScriptReadResponse(AXIResp::SLVERR);
    uint8_t error = 0;
    driver.Read(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 10, 0x95000), 1, &error);
    LUNATIC_CHECK(error == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();

    driver.memory.ScriptWriteResponse(AXIResp::DECERR);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 11, 0x95040));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    driver.WriteData(4, dbid, AXIPattern(31), FullEnables());
    const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
}

LUNATIC_TEST(Stage7AXIBridgePersistenceIsExplicitlyRejected)
{
    AXIBridgeDriver<Config256> driver;
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersist, 4, 12, 0x96000));
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    auto separated = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 13, 0x96040);
    separated.PGroupID() = 0x25;
    driver.PushRequest(separated);
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
    LUNATIC_CHECK(response.PGroupID() == 0x25);
    driver.Quiesce(false);
    LUNATIC_CHECK(driver.memory.CompletedReads().empty());
    LUNATIC_CHECK(driver.memory.CompletedWrites().empty());
    LUNATIC_CHECK(!driver.bridge.GetNode().GetErrors().empty());
}

LUNATIC_TEST(Stage7AXIBridgeCleanSharedWritesDirtyHome)
{
    Options options;
    options.policy = PolicyKind::NINE;
    AXIBridgeDriver<Config256> driver(options);
    const uint64_t address = 0x97000;
    const LineData data = AXIPattern(41);
    driver.Write(CHI::Eb::Opcodes::REQ::WriteBackFull, 30, address, data, FullEnables());
    driver.Quiesce();
    LUNATIC_CHECK(driver.bridge.GetNode().InspectHome(LineKey::FromAddress(address, false)).dirty);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 4, 31, address));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.ReadLine(address) == data);
    const HomeLineInfo home = driver.bridge.GetNode().InspectHome(LineKey::FromAddress(address, false));
    LUNATIC_CHECK(home.valid && !home.dirty);
}

LUNATIC_TEST(Stage7AXIProfileValidation)
{
    Options options;
    options.backend = BackendKind::AXI;
    ValidateOptions(options);
    bool Options::* const features[] = {
        &Options::separatedBackendRead,
        &Options::dmtEnabled,
        &Options::dwtEnabled,
        &Options::directPersistEnabled,
        &Options::poisonEnabled,
        &Options::dataCheckEnabled
    };
    for (const auto feature : features)
    {
        Options invalid = options;
        invalid.*feature = true;
        CheckThrows([&] { ValidateOptions(invalid); });
    }
    Options invalid = options;
    invalid.axiIdWidth = 1;
    invalid.axiOutstandingLimit = 3;
    CheckThrows([&] { ValidateOptions(invalid); });
}

LUNATIC_TEST(Stage7AXIBridgeSeededTraffic)
{
    AXIBridgeDriver<Config256> driver;
    constexpr size_t lines = 8;
    std::array<LineData, lines> expected{};
    for (size_t line = 0; line < lines; ++line)
    {
        expected[line] = AXIPattern(uint8_t(51 + line * 19));
        driver.memory.SetLine(0x98000 + line * LINE_SIZE, expected[line]);
    }

    uint32_t seed = 0xA571u;
    const auto next = [&] {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };
    TransactionId txnId = 40;
    for (size_t step = 0; step < 40; ++step)
    {
        if (step % 7 == 0)
        {
            driver.memory.Stall(AXIChannel::AW, step % 3);
            driver.memory.Stall(AXIChannel::AR, (step + 1) % 3);
            driver.memory.Stall(AXIChannel::B, (step + 2) % 3);
            driver.memory.Stall(AXIChannel::R, step % 2);
        }
        const size_t line = next() % lines;
        const uint64_t address = 0x98000 + line * LINE_SIZE;
        if (next() & 1)
        {
            LUNATIC_CHECK(driver.Read(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, txnId++, address)) == expected[line]);
        }
        else
        {
            LineData update{};
            ByteEnables enables{};
            for (size_t index = 0; index < LINE_SIZE; ++index)
            {
                update[index] = uint8_t(next());
                enables[index] = next() & 1;
            }
            enables[next() % LINE_SIZE] = 1;
            driver.Write(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, txnId++, address, update, enables);
            for (size_t index = 0; index < LINE_SIZE; ++index)
                if (enables[index])
                    expected[line][index] = update[index];
        }
        driver.Quiesce();
    }
    for (size_t line = 0; line < lines; ++line)
        LUNATIC_CHECK(driver.memory.ReadLine(0x98000 + line * LINE_SIZE) == expected[line]);
}

LUNATIC_TEST(Stage7AXIBridgeReadSubmissionRefusalRetries)
{
    Options options;
    options.axiOutstandingLimit = 1;
    AXIBridgeDriver<Config256> driver(options);
    const uint64_t a = 0x60000;
    const uint64_t b = 0x60040;
    const auto dataA = AXIPattern(31);
    const auto dataB = AXIPattern(107);
    driver.memory.SetLine(a, dataA);
    driver.memory.SetLine(b, dataB);
    driver.memory.Stall(AXIChannel::R, 20);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, a));
    driver.Until([&] { return !driver.memory.AccessedReadAddresses().empty(); });
    const size_t acceptedBytes = driver.memory.AccessedReadAddresses().size();
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2, b));
    for (size_t cycle = 0; cycle < 4; ++cycle)
        driver.Tick();
    LUNATIC_CHECK(driver.memory.AccessedReadAddresses().size() == acceptedBytes);

    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT().TgtID() == 4);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT().TgtID() == 5);
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.CompletedReads().size() == 2);
}

LUNATIC_TEST(Stage7AXIBridgeSpillSubmissionRefusalDoesNotUseReadDone)
{
    Options options;
    options.axiOutstandingLimit = 1;
    AXIBridgeDriver<Config256> driver(options);
    const uint64_t blocker = 0x61000;
    const uint64_t atomicAddress = 0x61100;
    const auto blockerData = AXIPattern(17);
    LineData initial{};
    initial[0] = 1;
    driver.memory.SetLine(blocker, blockerData);
    driver.memory.SetLine(atomicAddress, initial);
    driver.memory.Stall(AXIChannel::B, 100);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 5, 50, blocker));
    const auto blockerDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    driver.WriteData(5, blockerDBID, blockerData, FullEnables(), CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
        0, ~uint64_t(0), 0);
    driver.Until([&] { return driver.memory.ReadLine(blocker) == blockerData; });
    LUNATIC_CHECK(driver.memory.CompletedWrites().empty());

    auto atomic = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 2, atomicAddress);
    atomic.Size() = 0;
    atomic.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    driver.PushRequest(atomic);
    const auto atomicDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    LineData operands{};
    operands[0] = 2;
    ByteEnables enables{};
    enables[0] = 1;
    driver.WriteData(4, atomicDBID, operands, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData, 0, 1, 0);

    for (size_t cycle = 0; cycle < 30; ++cycle)
        driver.Tick();
    LUNATIC_CHECK(!driver.bridge.PeekUpstreamRSP());
    LUNATIC_CHECK(driver.memory.ReadLine(atomicAddress) == initial);
    LUNATIC_CHECK(driver.memory.CompletedWrites().empty());

    driver.memory.Stall(AXIChannel::B, 0);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 50);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 2);
    driver.Quiesce();
    LUNATIC_CHECK(driver.memory.CompletedWrites().size() == 2);
    LUNATIC_CHECK(driver.memory.ReadLine(atomicAddress)[0] == 3);
}
