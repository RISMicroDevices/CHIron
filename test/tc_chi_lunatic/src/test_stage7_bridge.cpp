#include "common.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename T>
    void CheckDirectionalQueue(Core::Impl& core, std::deque<Pending<T>>& queue, EventKind kind,
                               const std::array<T, 4>& flits, size_t& emitted)
    {
        LUNATIC_CHECK(core.Queue(queue, flits[0], INVALID_TRANSACTION, 1, false, true));
        LUNATIC_CHECK(core.Queue(queue, flits[1], INVALID_TRANSACTION, 2, false, false));
        LUNATIC_CHECK(core.Queue(queue, flits[2], INVALID_TRANSACTION, 1, false, true));
        LUNATIC_CHECK(core.Queue(queue, flits[3], INVALID_TRANSACTION, 2, false, false));

        const size_t queueSize = queue.size();
        const uint64_t nextSerial = core.nextSerial;
        const T* upstream = core.Peek(queue, false);
        const T* downstream = core.Peek(queue, true);
        LUNATIC_CHECK(upstream && upstream->txnId == flits[1].txnId);
        LUNATIC_CHECK(downstream && downstream->txnId == flits[0].txnId);
        LUNATIC_CHECK(core.Peek(queue, false) == upstream);
        LUNATIC_CHECK(core.Peek(queue, true) == downstream);
        LUNATIC_CHECK(core.Peek(queue) == downstream);
        LUNATIC_CHECK(queue.size() == queueSize);
        LUNATIC_CHECK(core.nextSerial == nextSerial);
        LUNATIC_CHECK(emitted == 0);

        auto popped = core.Pop(queue, kind, false);
        LUNATIC_CHECK(popped && popped->txnId == flits[1].txnId);
        LUNATIC_CHECK(queue.size() == 3);
        LUNATIC_CHECK(core.Peek(queue, true)->txnId == flits[0].txnId);
        LUNATIC_CHECK(emitted == 1);

        popped = core.Pop(queue, kind, false);
        LUNATIC_CHECK(popped && popped->txnId == flits[3].txnId);
        LUNATIC_CHECK(!core.Peek(queue, false));
        LUNATIC_CHECK(!core.Pop(queue, kind, false));
        LUNATIC_CHECK(queue.size() == 2);
        LUNATIC_CHECK(core.Peek(queue, true)->txnId == flits[0].txnId);
        LUNATIC_CHECK(emitted == 2);

        popped = core.Pop(queue, kind, true);
        LUNATIC_CHECK(popped && popped->txnId == flits[0].txnId);
        popped = core.Pop(queue, kind, true);
        LUNATIC_CHECK(popped && popped->txnId == flits[2].txnId);
        LUNATIC_CHECK(queue.empty());
        LUNATIC_CHECK(!core.Pop(queue, kind, true));
        LUNATIC_CHECK(emitted == 4);
    }

    using Bridge256 = Bridge<Config256>;
    using REQ = Bridge256::REQFlit;
    using RSP = Bridge256::RSPFlit;
    using DAT = Bridge256::DATFlit;

    constexpr size_t BRIDGE_BYTES = Config256::dataWidth / 8;
    constexpr size_t BRIDGE_BEATS = LINE_SIZE / BRIDGE_BYTES;

    LineData BridgePattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    REQ BridgeRequest(uint16_t opcode, NodeId source, TransactionId txnId, uint64_t address,
                      bool expCompAck = false)
    {
        REQ request{};
        request.SrcID() = source;
        request.TgtID() = 2;
        request.TxnID() = txnId;
        request.Opcode() = opcode;
        request.Addr() = address;
        request.Size() = 6;
        request.AllowRetry() = 1;
        request.ExpCompAck() = expCompAck;
        request.SnpAttr() = 1;
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
        return request;
    }

    DAT BridgeData(NodeId source, NodeId target, TransactionId txnId, DataBufferId dbid,
                   uint16_t opcode, size_t beat, const LineData& payload)
    {
        DAT data{};
        data.SrcID() = source;
        data.TgtID() = target;
        data.HomeNID() = 2;
        data.TxnID() = txnId;
        data.DBID() = dbid;
        data.Opcode() = opcode;
        data.DataID() = beat * BRIDGE_BYTES / 16;
        data.CCID() = 0;
        if (opcode == CHI::Eb::Opcodes::DAT::CompData)
            data.Resp() = CHI::Eb::Resps::CompData_I;
        else
            data.BE() = (uint64_t(1) << BRIDGE_BYTES) - 1;
        for (size_t index = 0; index < BRIDGE_BYTES; ++index)
            data.Data()[index / 8] |= uint64_t(payload[beat * BRIDGE_BYTES + index]) << ((index % 8) * 8);
        return data;
    }

    template<typename Predicate>
    void BridgeUntil(Bridge256& bridge, Time& now, Predicate&& predicate)
    {
        for (size_t cycle = 0; cycle < 500; ++cycle)
        {
            if (predicate())
                return;
            bridge.Tick(++now);
        }
        throw std::runtime_error("Stage 7 Bridge condition timed out");
    }

    void BridgeQuiesce(Bridge256& bridge, Time& now)
    {
        for (size_t cycle = 0; cycle < 500; ++cycle)
        {
            if (bridge.Drain())
                return;
            bridge.Tick(++now);
        }
        throw std::runtime_error("Stage 7 Bridge failed to drain");
    }

    void CheckBridgeData(const DAT& data, const LineData& expected, size_t beat)
    {
        LUNATIC_CHECK(data.DataID() == beat * BRIDGE_BYTES / 16);
        for (size_t index = 0; index < BRIDGE_BYTES; ++index)
            LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) ==
                          expected[beat * BRIDGE_BYTES + index]);
    }

}

LUNATIC_TEST(Stage7BridgeRSPIsolatesDirections)
{
    size_t emitted = 0;
    auto events = std::make_shared<EventHub>();
    events->OnEmittedTXRSP.Register(
        Gravity::MakeListener<EmittedTXRSPEvent>("stage7-rsp", 0, [&](EmittedTXRSPEvent&) { ++emitted; }));
    Core::Impl core(2, Options{}, events);
    std::array<Response, 4> flits{};
    for (size_t index = 0; index < flits.size(); ++index)
    {
        flits[index].source = 2;
        flits[index].target = 4;
        flits[index].txnId = 10 + index;
        flits[index].opcode = CHI::Eb::Opcodes::RSP::Comp;
    }
    CheckDirectionalQueue(core, core.txrsp, EventKind::EmittedTXRSP, flits, emitted);
}

LUNATIC_TEST(Stage7BridgeDATIsolatesDirections)
{
    size_t emitted = 0;
    auto events = std::make_shared<EventHub>();
    events->OnEmittedTXDAT.Register(
        Gravity::MakeListener<EmittedTXDATEvent>("stage7-dat", 0, [&](EmittedTXDATEvent&) { ++emitted; }));
    Core::Impl core(2, Options{}, events);
    std::array<Data, 4> flits{};
    for (size_t index = 0; index < flits.size(); ++index)
    {
        flits[index].source = 2;
        flits[index].target = 4;
        flits[index].txnId = 20 + index;
        flits[index].opcode = CHI::Eb::Opcodes::DAT::CompData;
    }
    CheckDirectionalQueue(core, core.txdat, EventKind::EmittedTXDAT, flits, emitted);
}

LUNATIC_TEST(Stage7BridgeCoherentCHIBackendCrossDirectionBackpressure)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Bridge256 bridge(2, options);
    Time now = 0;
    const LineData writeData = BridgePattern(31);
    const LineData readData = BridgePattern(147);
    const uint64_t writeAddress = 0x68000;
    const uint64_t readAddress = 0x69000;

    auto write = BridgeRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, 40, writeAddress);
    write.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    LUNATIC_CHECK(bridge.PushUpstreamREQ(write));
    BridgeUntil(bridge, now, [&] { return static_cast<bool>(bridge.PeekUpstreamRSP()); });
    auto dbid = bridge.PopUpstreamRSP();
    LUNATIC_CHECK(dbid && dbid->Opcode() == CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(bridge.PeekUpstreamRSP());
    LUNATIC_CHECK(!bridge.PeekDownstreamRSP());

    for (size_t beat = 0; beat < BRIDGE_BEATS; ++beat)
    {
        auto data = BridgeData(4, 2, dbid->DBID(), 0, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
                               beat, writeData);
        LUNATIC_CHECK(bridge.PushUpstreamDAT(data));
    }
    bridge.Tick(++now);
    BridgeUntil(bridge, now, [&] { return static_cast<bool>(bridge.PeekDownstreamREQ()); });
    auto backendWrite = bridge.PopDownstreamREQ();
    LUNATIC_CHECK(backendWrite && backendWrite->Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);

    RSP backendDBID{};
    backendDBID.SrcID() = 6;
    backendDBID.TgtID() = 2;
    backendDBID.TxnID() = backendWrite->TxnID();
    backendDBID.DBID() = 77;
    backendDBID.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
    LUNATIC_CHECK(bridge.PushDownstreamRSP(backendDBID));
    bridge.Tick(++now);
    BridgeUntil(bridge, now, [&] { return static_cast<bool>(bridge.PeekDownstreamDAT()); });

    auto read = BridgeRequest(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 41, readAddress, true);
    LUNATIC_CHECK(bridge.PushUpstreamREQ(read));
    BridgeUntil(bridge, now, [&] { return static_cast<bool>(bridge.PeekDownstreamREQ()); });
    auto backendRead = bridge.PopDownstreamREQ();
    LUNATIC_CHECK(backendRead && backendRead->Opcode() == CHI::Eb::Opcodes::REQ::ReadNoSnp);
    for (size_t beat = 0; beat < BRIDGE_BEATS; ++beat)
    {
        auto data = BridgeData(6, 2, backendRead->TxnID(), 88, CHI::Eb::Opcodes::DAT::CompData,
                               beat, readData);
        LUNATIC_CHECK(bridge.PushDownstreamDAT(data));
    }
    bridge.Tick(++now);
    BridgeUntil(bridge, now, [&] { return static_cast<bool>(bridge.PeekUpstreamDAT()); });

    const auto upstreamPeek = bridge.PeekUpstreamDAT();
    const auto downstreamPeek = bridge.PeekDownstreamDAT();
    LUNATIC_CHECK(upstreamPeek && upstreamPeek->Opcode() == CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(downstreamPeek && downstreamPeek->Opcode() == CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(bridge.PeekUpstreamDAT()->TxnID() == upstreamPeek->TxnID());
    LUNATIC_CHECK(bridge.PeekUpstreamDAT()->DataID() == upstreamPeek->DataID());
    LUNATIC_CHECK(bridge.PeekDownstreamDAT()->TxnID() == downstreamPeek->TxnID());
    LUNATIC_CHECK(bridge.PeekDownstreamDAT()->DataID() == downstreamPeek->DataID());
    LUNATIC_CHECK(bridge.PeekUpstreamRSP()->TxnID() == 40);
    LUNATIC_CHECK(!bridge.PeekDownstreamRSP());

    DataBufferId readDBID = 0;
    for (size_t beat = 0; beat < BRIDGE_BEATS; ++beat)
    {
        BridgeUntil(bridge, now, [&] { return static_cast<bool>(bridge.PeekUpstreamDAT()); });
        auto data = bridge.PopUpstreamDAT();
        LUNATIC_CHECK(data && data->Opcode() == CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data->TgtID() == 5 && data->TxnID() == 41);
        CheckBridgeData(*data, readData, beat);
        readDBID = data->DBID();
        const auto heldDownstream = bridge.PeekDownstreamDAT();
        LUNATIC_CHECK(heldDownstream && heldDownstream->TxnID() == downstreamPeek->TxnID());
        LUNATIC_CHECK(heldDownstream->DataID() == downstreamPeek->DataID());
    }
    LUNATIC_CHECK(!bridge.PeekUpstreamDAT());

    for (size_t beat = 0; beat < BRIDGE_BEATS; ++beat)
    {
        auto data = bridge.PopDownstreamDAT();
        LUNATIC_CHECK(data && data->Opcode() == CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(data->TgtID() == 6 && data->TxnID() == 77);
        CheckBridgeData(*data, writeData, beat);
    }
    LUNATIC_CHECK(!bridge.PeekDownstreamDAT());

    RSP compAck{};
    compAck.SrcID() = 5;
    compAck.TgtID() = 2;
    compAck.TxnID() = readDBID;
    compAck.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
    LUNATIC_CHECK(bridge.PushUpstreamRSP(compAck));

    RSP backendComp{};
    backendComp.SrcID() = 6;
    backendComp.TgtID() = 2;
    backendComp.TxnID() = backendWrite->TxnID();
    backendComp.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    backendComp.Resp() = CHI::Eb::Resps::Comp_I;
    LUNATIC_CHECK(bridge.PushDownstreamRSP(backendComp));
    bridge.Tick(++now);

    auto writeComp = bridge.PopUpstreamRSP();
    LUNATIC_CHECK(writeComp && writeComp->Opcode() == CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(writeComp->TxnID() == 40);
    LUNATIC_CHECK(!bridge.PeekUpstreamRSP());
    BridgeQuiesce(bridge, now);
    LUNATIC_CHECK(bridge.GetNode().GetErrors().empty());
}
