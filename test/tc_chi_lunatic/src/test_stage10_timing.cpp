#include "driver.hpp"

#include <array>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Request TimingRequest(uint16_t opcode, NodeId source = 4, TransactionId txnId = 1,
                          uint64_t address = 0xf0000, uint8_t group = 0)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = opcode;
        request.address = address;
        request.size = IsAtomicRequest(opcode) ? 2 : 6;
        request.allowRetry = false;
        request.pGroupId = group;
        request.memAttr = IsAtomicRequest(opcode)
            ? CHI::Eb::MemAttrs::WriteBack_Allocate : CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    Transaction& AddTimingTransaction(Core::Impl& impl, uint16_t opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp,
                                      uint8_t group = 0)
    {
        const Request request = TimingRequest(opcode, 4, 1, 0xf0000, group);
        impl.AdmitRequest(request);
        Transaction& transaction = *impl.tracker.FindRequest(4, 1);
        transaction.dbid = transaction.handle;
        return transaction;
    }

    void TickTo(Core::Impl& impl, Time target)
    {
        while (impl.time < target)
        {
            ++impl.time;
            impl.Tick();
        }
    }

}

LUNATIC_TEST(Stage10TimingOperationDelayInventory)
{
    Options options;
    options.timing = {
        .inputRegister = 1,
        .requestBuffer = 2,
        .allocation = 3,
        .hazardRelease = 4,
        .lookupResult = 5,
        .snoopDecision = 6,
        .snoopIssue = 7,
        .dataReturn = 8,
        .fastResponse = 9,
        .normalResponse = 10,
        .backendRequest = 11,
        .backendData = 12,
        .retryAck = 13,
        .creditGrant = 14,
        .backendResend = 15,
        .atomicExecution = 16,
        .persistence = 17,
        .dataBeat = 18,
        .output = 19,
        .commitSchedule = 20
    };
    VirtualPipeline pipeline(options);
    const std::array<SchedulePoint, 20> points = {
        SchedulePoint::InputRegister, SchedulePoint::RequestBuffer, SchedulePoint::Allocation,
        SchedulePoint::HazardRelease, SchedulePoint::LookupResult,
        SchedulePoint::SnoopDecision, SchedulePoint::SnoopIssue,
        SchedulePoint::DataReturn, SchedulePoint::FastResponse, SchedulePoint::NormalResponse,
        SchedulePoint::BackendRequest, SchedulePoint::BackendData, SchedulePoint::RetryAck,
        SchedulePoint::CreditGrant, SchedulePoint::BackendResend, SchedulePoint::AtomicExecution,
        SchedulePoint::Persistence, SchedulePoint::DataBeat, SchedulePoint::Output,
        SchedulePoint::CommitSchedule
    };
    for (size_t index = 0; index < points.size(); ++index)
        LUNATIC_CHECK(pipeline.OperationDelay(points[index]) == index + 1);
    LUNATIC_CHECK(pipeline.OperationDelay(SchedulePoint::Automatic) == 0);
    LUNATIC_CHECK(VirtualPipeline(Options{}).OperationDelay(SchedulePoint::CommitSchedule) == 0);
}

LUNATIC_TEST(Stage10TimingInputRegister)
{
    Options options;
    options.timing.inputRegister = 3;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    const Request request = TimingRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
    impl.Tick();
    TickTo(impl, 2);
    LUNATIC_CHECK(impl.tracker.Empty());
    LUNATIC_CHECK(impl.rxreq.size() == 1);
    TickTo(impl, 3);
    LUNATIC_CHECK(impl.tracker.Size() == 1);
    LUNATIC_CHECK(impl.rxreqTimes.empty());
}

LUNATIC_TEST(Stage10TimingFullInternalReadTimeline)
{
    Options options;
    options.timing.requestBuffer = 1;
    options.timing.allocation = 2;
    options.timing.lookupResult = 3;
    options.timing.dataReturn = 4;
    options.timing.dataBeat = 2;
    options.timing.output = 1;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddTimingTransaction(impl);
    impl.Tick();
    TickTo(impl, 27);
    LUNATIC_CHECK(transaction.phase == Phase::Return);
    const FlowState& flow = impl.flows.at(transaction.handle);
    LUNATIC_CHECK(flow.scheduleMarks.at(SchedulePoint::RequestBuffer) == 2);
    LUNATIC_CHECK(flow.scheduleMarks.at(SchedulePoint::Allocation) == 6);
    LUNATIC_CHECK(flow.scheduleMarks.at(SchedulePoint::LookupResult) == 11);
    LUNATIC_CHECK(flow.scheduleMarks.at(SchedulePoint::DataReturn) == 26);
    LUNATIC_CHECK(!impl.Peek(impl.txdat));

    TickTo(impl, 32);
    LUNATIC_CHECK(impl.txdat.size() == 2);
    LUNATIC_CHECK(impl.txdat[0].readyTime == 32);
    LUNATIC_CHECK(impl.txdat[1].readyTime == 35);
    LUNATIC_CHECK(flow.dataBeatScheduled == 1);
    const auto first = impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
    LUNATIC_CHECK(first && first->dataId == 0);
    TickTo(impl, 34);
    LUNATIC_CHECK(!impl.Peek(impl.txdat));
    TickTo(impl, 35);
    const auto second = impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
    LUNATIC_CHECK(second && second->dataId == 2);
    TickTo(impl, 40);
    LUNATIC_CHECK(impl.tracker.Empty());
}

LUNATIC_TEST(Stage10TimingOutputScheduleMapping)
{
    Options options;
    options.timing.dataReturn = 1;
    options.timing.fastResponse = 2;
    options.timing.normalResponse = 3;
    options.timing.backendRequest = 4;
    options.timing.backendData = 5;
    options.timing.retryAck = 6;
    options.timing.creditGrant = 7;
    options.timing.backendResend = 8;
    options.timing.snoopIssue = 9;
    options.timing.output = 10;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddTimingTransaction(impl);
    impl.time = 10;

    Data upstream;
    upstream.source = 2;
    upstream.target = 4;
    upstream.opcode = CHI::Eb::Opcodes::DAT::CompData;
    LUNATIC_CHECK(impl.Queue(impl.txdat, upstream, transaction.handle, 0));
    Data downstream = upstream;
    downstream.opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
    LUNATIC_CHECK(impl.Queue(impl.txdat, downstream, transaction.handle, 0, true, true));

    Response fast;
    fast.source = 2;
    fast.target = 4;
    fast.opcode = CHI::Eb::Opcodes::RSP::DBIDResp;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, fast, transaction.handle, 0));
    Response normal = fast;
    normal.opcode = CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, normal, transaction.handle, 0));
    Response retry = fast;
    retry.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, retry, INVALID_TRANSACTION, 0, false, false, 0));
    Response grant = fast;
    grant.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, grant, INVALID_TRANSACTION, 0, false, false, 1));

    Request backend;
    backend.source = 2;
    backend.target = 6;
    backend.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    LUNATIC_CHECK(impl.Queue(impl.txreq, backend, transaction.handle, 0, false, true));
    LUNATIC_CHECK(impl.Queue(impl.txreq, backend, transaction.handle, 0, false, true,
        std::numeric_limits<size_t>::max(), SchedulePoint::BackendResend));
    Snoop snoop;
    snoop.source = 2;
    snoop.target = 4;
    snoop.opcode = CHI::Eb::Opcodes::SNP::SnpUnique;
    LUNATIC_CHECK(impl.Queue(impl.txsnp, snoop, transaction.handle, 0));

    LUNATIC_CHECK(impl.txdat[0].readyTime == 21);
    LUNATIC_CHECK(impl.txdat[1].readyTime == 25);
    LUNATIC_CHECK(impl.txrsp[0].readyTime == 22);
    LUNATIC_CHECK(impl.txrsp[1].readyTime == 23);
    LUNATIC_CHECK(impl.txrsp[2].readyTime == 26);
    LUNATIC_CHECK(impl.txrsp[3].readyTime == 27);
    LUNATIC_CHECK(impl.txreq[0].readyTime == 24);
    LUNATIC_CHECK(impl.txreq[1].readyTime == 28);
    LUNATIC_CHECK(impl.txreq[1].schedule == SchedulePoint::BackendResend);
    LUNATIC_CHECK(impl.txsnp[0].readyTime == 29);
    LUNATIC_CHECK(!impl.Peek(impl.txdat));
    LUNATIC_CHECK(!impl.Peek(impl.txrsp));
    LUNATIC_CHECK(!impl.Peek(impl.txreq));
    LUNATIC_CHECK(!impl.Peek(impl.txsnp));

    TickTo(impl, 21);
    LUNATIC_CHECK(impl.Pop(impl.txdat, EventKind::EmittedTXDAT));
    TickTo(impl, 22);
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP)->opcode == fast.opcode);
    TickTo(impl, 23);
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP)->opcode == normal.opcode);
    TickTo(impl, 24);
    LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
    TickTo(impl, 25);
    LUNATIC_CHECK(impl.Pop(impl.txdat, EventKind::EmittedTXDAT));
    TickTo(impl, 26);
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP)->opcode == retry.opcode);
    TickTo(impl, 27);
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP)->opcode == grant.opcode);
    TickTo(impl, 28);
    LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
    TickTo(impl, 29);
    LUNATIC_CHECK(impl.Pop(impl.txsnp, EventKind::EmittedTXSNP));
}

LUNATIC_TEST(Stage10TimingBeatIntervalDoesNotRepeatOnBackpressure)
{
    Options options;
    options.timing.dataBeat = 2;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddTimingTransaction(impl);
    transaction.sentBeats = 1;
    transaction.readyTime = 0;
    impl.time = 5;
    LUNATIC_CHECK(!impl.ScheduleTransition(transaction, SchedulePoint::DataBeat));
    LUNATIC_CHECK(transaction.readyTime == 7);
    impl.time = 7;
    LUNATIC_CHECK(impl.ScheduleTransition(transaction, SchedulePoint::DataBeat));
    LUNATIC_CHECK(transaction.readyTime == 7);
    transaction.sentBeats = 2;
    LUNATIC_CHECK(!impl.ScheduleTransition(transaction, SchedulePoint::DataBeat));
    LUNATIC_CHECK(transaction.readyTime == 9);
    impl.time = 9;
    LUNATIC_CHECK(impl.ScheduleTransition(transaction, SchedulePoint::DataBeat));
    LUNATIC_CHECK(transaction.readyTime == 9);
}

LUNATIC_TEST(Stage10TimingAtomicExecution)
{
    Options options;
    options.timing.atomicExecution = 2;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddTimingTransaction(impl, CHI::Eb::Opcodes::REQ::AtomicStore::ADD);
    impl.ResolveAtomic(transaction);
    FlowState& flow = impl.flows.at(transaction.handle);
    flow.dbidSent = true;
    flow.dbidEmitted = true;
    flow.compSent = true;
    flow.compEmitted = true;
    flow.atomic.operands[0] = 1;
    transaction.data[0] = 2;
    transaction.Mark(COMPLETE_MEMORY);
    transaction.Mark(COMPLETE_WRITE_DATA);
    transaction.phase = Phase::Action;
    impl.ActionAtomic(transaction);
    LUNATIC_CHECK(transaction.readyTime == 2);
    LUNATIC_CHECK(!flow.directoryCommitted);
    TickTo(impl, 2);
    LUNATIC_CHECK(flow.directoryCommitted);
    LUNATIC_CHECK(impl.Bank(transaction.request.Line()).directory.Find(transaction.request.Line()));
    LUNATIC_CHECK(impl.Bank(transaction.request.Line()).directory.Find(transaction.request.Line())->data[0] == 3);
    impl.Retire(transaction.handle);
}

LUNATIC_TEST(Stage10TimingPersistenceCompletion)
{
    Options options;
    options.timing.persistence = 2;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddTimingTransaction(impl, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 0x66);
    impl.ActionPersistence(transaction);
    LUNATIC_CHECK(transaction.readyTime == 2);
    LUNATIC_CHECK(impl.txrsp.empty());
    TickTo(impl, 9);
    LUNATIC_CHECK(impl.txrsp.size() == 1);
    LUNATIC_CHECK(impl.txrsp.front().flit.opcode == CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(impl.txrsp.front().flit.pGroupId == 0x66);
    impl.Retire(transaction.handle);
}

LUNATIC_TEST(Stage10TimingCommitSchedule)
{
    Options options;
    options.timing.commitSchedule = 4;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddTimingTransaction(impl);
    transaction.phase = Phase::Commit;
    transaction.readyTime = 0;
    impl.time = 3;
    impl.RetireFinished();
    LUNATIC_CHECK(transaction.readyTime == 7);
    LUNATIC_CHECK(!impl.tracker.Empty());
    impl.time = 7;
    impl.RetireFinished();
    LUNATIC_CHECK(impl.tracker.Empty());
}

LUNATIC_TEST(Stage10TimingSnoopDecision)
{
    Options options;
    options.timing.snoopDecision = 3;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    const Request request = TimingRequest(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 2);
    impl.AdmitRequest(request);
    Transaction& transaction = *impl.tracker.FindRequest(5, 2);
    const LineKey line = transaction.request.Line();
    impl.Bank(line).snoopFilter.Track(line, 4, CacheState::UD, 0);
    transaction.homeHit = true;
    impl.flows.at(transaction.handle).snoopLine = line;
    impl.PlanReadSnoops(transaction);
    LUNATIC_CHECK(transaction.snoopTargets.size() == 1);
    LUNATIC_CHECK(transaction.readyTime == 3);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).scheduleMarks.at(SchedulePoint::SnoopDecision) == 3);
}
