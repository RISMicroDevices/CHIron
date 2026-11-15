#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Request RetryRequest(uint16_t opcode, NodeId source, TransactionId txnId)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = opcode;
        request.address = 0x100000;
        request.size = 6;
        request.allowRetry = false;
        request.memAttr = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    Transaction& AddRetryTransaction(Core::Impl& impl, uint16_t opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp)
    {
        const Request request = RetryRequest(opcode, 4, 1);
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

    Response BackendResponse(const Transaction& transaction, uint16_t opcode)
    {
        Response response;
        response.source = 6;
        response.target = 2;
        response.txnId = transaction.handle;
        response.dbid = 9;
        response.opcode = opcode;
        return response;
    }

}

LUNATIC_TEST(Stage10TimingRetryAckAndCreditGrant)
{
    Options options;
    options.transactionLimit = 1;
    options.timing.retryAck = 3;
    options.timing.creditGrant = 4;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& original = AddRetryTransaction(impl);
    Request retried = RetryRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2);
    retried.allowRetry = true;
    LUNATIC_CHECK(impl.Push(impl.rxreq, retried, EventKind::AcceptedRXREQ));
    impl.Tick();
    LUNATIC_CHECK(impl.txrsp.size() == 1);
    LUNATIC_CHECK(impl.txrsp.front().readyTime == 3);
    LUNATIC_CHECK(impl.retries.size() == 1);
    LUNATIC_CHECK(!impl.retries.begin()->second.ackPopped);
    LUNATIC_CHECK(!impl.Peek(impl.txrsp));
    TickTo(impl, 3);
    const auto ack = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
    LUNATIC_CHECK(ack && ack->opcode == CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(impl.retries.begin()->second.ackPopped);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 0);

    impl.Retire(original.handle);
    impl.QueueRetryCredits();
    LUNATIC_CHECK(impl.txrsp.size() == 1);
    LUNATIC_CHECK(impl.txrsp.front().readyTime == 7);
    LUNATIC_CHECK(!impl.Peek(impl.txrsp));
    TickTo(impl, 7);
    const auto grant = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
    LUNATIC_CHECK(grant && grant->opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
    LUNATIC_CHECK(impl.availableCredits.at({ 5, 0 }) == 1);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 0);
}

LUNATIC_TEST(Stage10TimingBackendResend)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.timing.backendResend = 5;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddRetryTransaction(impl);
    impl.StartMemoryRead(transaction);
    LUNATIC_CHECK(impl.txreq.size() == 1);
    LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
    LUNATIC_CHECK(impl.HandleBackendRetry(transaction,
        BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck)));
    impl.backendCredits[{ 6, 0 }] = 1;
    impl.AdvanceBackendRetries();
    LUNATIC_CHECK(impl.txreq.size() == 1);
    LUNATIC_CHECK(impl.txreq.front().readyTime == 5);
    LUNATIC_CHECK(impl.txreq.front().schedule == SchedulePoint::BackendResend);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).backendOperationOwned);
    LUNATIC_CHECK(!impl.Peek(impl.txreq));
    TickTo(impl, 5);
    LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
    LUNATIC_CHECK(impl.flows.at(transaction.handle).backendOperationOwned);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).backendPhase == BackendPhase::WaitComp);
}

LUNATIC_TEST(Stage10TimingBackendWriteAndDataBeats)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.timing.backendRequest = 2;
    options.timing.backendData = 3;
    options.timing.dataBeat = 2;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddRetryTransaction(impl, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    transaction.byteEnables.fill(1);
    impl.flows.at(transaction.handle).expectedBeatMask = 3;
    impl.StartBackendWrite(transaction);
    LUNATIC_CHECK(impl.txreq.front().readyTime == 2);
    TickTo(impl, 2);
    LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
    LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
        BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
    impl.AdvanceBackendWrite(transaction);
    LUNATIC_CHECK(impl.txdat.front().readyTime == 5);
    TickTo(impl, 5);
    LUNATIC_CHECK(impl.Pop(impl.txdat, EventKind::EmittedTXDAT));
    impl.AdvanceBackendWrite(transaction);
    LUNATIC_CHECK(impl.txdat.empty());
    TickTo(impl, 7);
    impl.AdvanceBackendWrite(transaction);
    LUNATIC_CHECK(impl.txdat.front().readyTime == 10);
    TickTo(impl, 10);
    LUNATIC_CHECK(impl.Pop(impl.txdat, EventKind::EmittedTXDAT));
    LUNATIC_CHECK(impl.flows.at(transaction.handle).backendDataEmitted);
    LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
        BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
    LUNATIC_CHECK(!impl.flows.at(transaction.handle).backendOperationOwned);
    impl.Retire(transaction.handle);
    LUNATIC_CHECK(impl.ValidateInvariants());
}

LUNATIC_TEST(Stage10TimingCommitAbsoluteDeadline)
{
    Options options;
    options.timing.commitSchedule = 3;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddRetryTransaction(impl);
    transaction.phase = Phase::Commit;
    transaction.readyTime = 0;
    impl.time = 5;
    impl.RetireFinished();
    LUNATIC_CHECK(transaction.readyTime == 8);
    LUNATIC_CHECK(!impl.tracker.Empty());
    impl.time = 7;
    impl.RetireFinished();
    LUNATIC_CHECK(!impl.tracker.Empty());
    impl.time = 8;
    impl.RetireFinished();
    LUNATIC_CHECK(impl.tracker.Empty());
}
