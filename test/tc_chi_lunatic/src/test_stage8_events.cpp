#include "common.hpp"

#include <limits>

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage8EventDecisionPayloads)
{
    auto events = std::make_shared<EventHub>();
    Request request;
    request.source = 4;
    request.target = 2;
    request.txnId = 7;
    request.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    request.address = 0x1040;

    events->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("backpressure", -1, [&](RequestAdmissionEvent& event) {
            LUNATIC_CHECK(event.request == request);
            event.Backpressure();
        }));
    events->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("retry", 1, [&](RequestAdmissionEvent& event) {
            event.RequestRetry();
        }));
    const auto admission = events->Decide(RequestAdmissionEvent(3, 2, request));
    LUNATIC_CHECK(admission.IsBackpressured());
    LUNATIC_CHECK(admission.IsRetryRequested());
    LUNATIC_CHECK(admission.request == request);

    Transaction transaction(9, request, 1);
    transaction.phase = Phase::Lookup;
    transaction.readyTime = 8;
    events->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("delay", 0, [&](TransactionScheduleEvent& event) {
            LUNATIC_CHECK(event.transaction == transaction.handle);
            LUNATIC_CHECK(event.phase == Phase::Lookup);
            LUNATIC_CHECK(event.readyTime == 8);
            event.AddDelay(4);
            event.AddDelay(std::numeric_limits<Time>::max());
            event.Backpressure();
        }));
    const auto schedule = events->Decide(TransactionScheduleEvent(8, 2, transaction));
    LUNATIC_CHECK(schedule.GetAdditionalDelay() == std::numeric_limits<Time>::max());
    LUNATIC_CHECK(schedule.IsBackpressured());

    events->OnVictimSelection.Register(
        Gravity::MakeListener<VictimSelectionEvent>("victim", 0, [&](VictimSelectionEvent& event) {
            LUNATIC_CHECK(event.domain == VictimDomain::Directory);
            LUNATIC_CHECK((event.candidates == std::vector<size_t>{ 0, 1, 3 }));
            event.SelectVictim(3);
        }));
    const auto victim = events->Decide(
        VictimSelectionEvent(3, 2, 9, VictimDomain::Directory, request.Line(), { 0, 1, 3 }));
    LUNATIC_CHECK(victim.GetSelectedVictim() == 3);

    events->Clear();
    LUNATIC_CHECK(events->OnRequestAdmission.empty());
    LUNATIC_CHECK(events->OnTransactionSchedule.empty());
    LUNATIC_CHECK(events->OnVictimSelection.empty());
}

LUNATIC_TEST(Stage8EventObservationReentryRejected)
{
    auto events = std::make_shared<EventHub>();
    HomeNode<Config256> node(2, {}, events);
    HomeNode<Config256>::REQFlit request{};
    request.SrcID() = 4;
    request.TgtID() = 2;
    request.TxnID() = 7;
    request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    request.Addr() = 0x1040;
    request.Size() = 6;

    size_t deferredErrors = 0;
    bool inAccepted = false;
    events->OnError.Register(Gravity::MakeListener<ErrorEvent>("error", 0, [&](ErrorEvent&) {
        LUNATIC_CHECK(!inAccepted);
        ++deferredErrors;
    }));
    events->OnAcceptedRXREQ.Register(
        Gravity::MakeListener<AcceptedRXREQEvent>("reentry", 0, [&](AcceptedRXREQEvent&) {
            inAccepted = true;
            LUNATIC_CHECK(!node.PushRXREQ(request));
            LUNATIC_CHECK(!node.PushRXRSP({}));
            LUNATIC_CHECK(!node.PushRXDAT({}));
            LUNATIC_CHECK(!node.PushRXB({}));
            LUNATIC_CHECK(!node.PushRXR({}));
            LUNATIC_CHECK(!node.PeekTXREQ());
            LUNATIC_CHECK(!node.PeekTXRSP());
            LUNATIC_CHECK(!node.PeekTXRSP(false));
            LUNATIC_CHECK(!node.PeekTXDAT());
            LUNATIC_CHECK(!node.PeekTXDAT(false));
            LUNATIC_CHECK(!node.PeekTXSNP());
            LUNATIC_CHECK(!node.PeekTXAW());
            LUNATIC_CHECK(!node.PeekTXW());
            LUNATIC_CHECK(!node.PeekTXAR());
            LUNATIC_CHECK(!node.PopTXREQ());
            LUNATIC_CHECK(!node.PopTXRSP());
            LUNATIC_CHECK(!node.PopTXRSP(false));
            LUNATIC_CHECK(!node.PopTXDAT());
            LUNATIC_CHECK(!node.PopTXDAT(false));
            LUNATIC_CHECK(!node.PopTXSNP());
            LUNATIC_CHECK(!node.PopTXAW());
            LUNATIC_CHECK(!node.PopTXW());
            LUNATIC_CHECK(!node.PopTXAR());
            node.Tick(99);
            LUNATIC_CHECK(!node.Drain());
            node.Reset();
            LUNATIC_CHECK(node.GetTime() == 0);
            LUNATIC_CHECK(node.HasInflight());
            inAccepted = false;
        }));

    LUNATIC_CHECK(node.PushRXREQ(request));
    constexpr size_t rejectedOperations = 26;
    LUNATIC_CHECK(node.GetErrors().size() == rejectedOperations);
    LUNATIC_CHECK(deferredErrors == rejectedOperations);
    LUNATIC_CHECK(node.HasInflight());
    node.Reset();
    LUNATIC_CHECK(node.GetErrors().empty());
    LUNATIC_CHECK(!node.HasInflight());
}

LUNATIC_TEST(Stage8EventResetReentryDefersError)
{
    auto events = std::make_shared<EventHub>();
    HomeNode<Config256> node(2, {}, events);
    std::vector<int> order;
    bool inReset = false;
    events->OnReset.Register(Gravity::MakeListener<ResetEvent>("reset", 0, [&](ResetEvent&) {
        inReset = true;
        order.push_back(1);
        node.Reset();
        inReset = false;
    }));
    events->OnError.Register(Gravity::MakeListener<ErrorEvent>("error", 0, [&](ErrorEvent&) {
        LUNATIC_CHECK(!inReset);
        order.push_back(2);
    }));

    node.Reset();
    LUNATIC_CHECK((order == std::vector<int>{ 1, 2 }));
    LUNATIC_CHECK(node.GetErrors().size() == 1);
}

LUNATIC_TEST(Stage8EventRecursivePublicationIsDeferred)
{
    auto events = std::make_shared<EventHub>();
    std::vector<int> order;
    events->OnAcceptedRXREQ.Register(
        Gravity::MakeListener<AcceptedRXREQEvent>("accepted", 0, [&](AcceptedRXREQEvent&) {
            order.push_back(1);
            Event event;
            event.kind = EventKind::Reset;
            events->Publish(std::move(event));
            order.push_back(2);
        }));
    events->OnReset.Register(Gravity::MakeListener<ResetEvent>("reset", 0, [&](ResetEvent&) {
        order.push_back(3);
    }));

    Event event;
    event.kind = EventKind::AcceptedRXREQ;
    events->Publish(std::move(event));
    LUNATIC_CHECK((order == std::vector<int>{ 1, 2, 3 }));
}
