#include "driver.hpp"

#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Request ResponseRequest(uint16_t opcode, NodeId source, TransactionId txnId,
                            uint64_t address = 0xd0000, uint8_t qos = 0)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = opcode;
        request.address = address;
        request.size = 6;
        request.qos = qos;
        request.allowRetry = false;
        request.snpAttr = IsCoherentRequest(opcode);
        const auto requestClass = ClassifyRequest(opcode);
        request.memAttr = requestClass == RequestClass::CoherentRead
            ? CHI::Eb::MemAttrs::WriteBack_NoAllocate : CHI::Eb::MemAttrs::WriteBack_Allocate;
        return request;
    }

    Transaction& AddRequest(Core::Impl& impl, uint16_t opcode, NodeId source, TransactionId txnId,
                            uint64_t address = 0xd0000, uint8_t qos = 0)
    {
        const Request request = ResponseRequest(opcode, source, txnId, address, qos);
        impl.AdmitRequest(request);
        Transaction* transaction = impl.tracker.FindRequest(source, txnId);
        LUNATIC_CHECK(transaction);
        transaction->dbid = transaction->handle;
        return *transaction;
    }

    std::vector<uint16_t> Responses(Core::Impl& impl)
    {
        std::vector<uint16_t> opcodes;
        for (const auto& pending : impl.txrsp)
            opcodes.push_back(pending.flit.opcode);
        return opcodes;
    }

    void CheckImmediateResponse(ImmediateResponseKind strategy, const std::vector<uint16_t>& expected)
    {
        Options options;
        options.strategy.immediateResponse = strategy;
        auto events = std::make_shared<EventHub>();
        Core::Impl impl(2, options, events);
        auto& transaction = AddRequest(impl, CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, 10);
        impl.ResolveImmediateWrite(transaction);
        impl.QueueImmediateResponse(transaction);
        LUNATIC_CHECK(Responses(impl) == expected);
    }

    void CheckStashResponse(StashResponseKind strategy, const std::vector<uint16_t>& expected)
    {
        Options options;
        options.strategy.stashResponse = strategy;
        auto events = std::make_shared<EventHub>();
        Core::Impl impl(2, options, events);
        auto& transaction = AddRequest(impl, CHI::Eb::Opcodes::REQ::StashOnceSepUnique, 4, 11);
        transaction.homeHit = true;
        impl.ResolveStash(transaction);
        impl.ActionStash(transaction);
        LUNATIC_CHECK(Responses(impl) == expected);
    }

    Transaction& PrepareDct(Core::Impl& impl, bool multipleOwners)
    {
        auto& transaction = AddRequest(impl, CHI::Eb::Opcodes::REQ::ReadShared, 6, 20);
        const LineKey line = transaction.request.Line();
        transaction.homeHit = true;
        impl.flows.at(transaction.handle).snoopLine = line;
        impl.Bank(line).snoopFilter.Track(line, 4, CacheState::UD, 0);
        if (multipleOwners)
            impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, 1);
        return transaction;
    }

    void SeedUpstreamRetry(Core::Impl& impl, size_t index, TransactionId txnId, uint8_t traceTag)
    {
        Request request = ResponseRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, txnId, 0xd0000, 15);
        request.traceTag = traceTag;
        RetryRecord record{ request, 0, 0, index, true, false, true, false };
        impl.retries.emplace(index, record);
        ++impl.banks[record.bank].retryReservations[record.classId];
        impl.AcquireRetrySource(request);
    }

    void CheckUpstreamSelection(RetrySelectionKind strategy, size_t removed)
    {
        Options options = MakeType7Options();
        options.backend = BackendKind::Internal;
        options.strategy.upstreamRetrySelection = strategy;
        auto events = std::make_shared<EventHub>();
        Core::Impl impl(2, options, events);
        SeedUpstreamRetry(impl, 0, 1, 0x11);
        SeedUpstreamRetry(impl, 1, 2, 0x22);
        impl.availableCredits[{ 4, 0 }] = 1;
        Request resend = ResponseRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 99, 0xd0000, 15);
        LUNATIC_CHECK(impl.ConsumeRetryRequest(resend, false));
        LUNATIC_CHECK(!impl.retries.contains(removed));
        LUNATIC_CHECK(impl.retries.contains(1 - removed));
        LUNATIC_CHECK(impl.TotalRetryReservations() == 1);
    }

    void CheckDownstreamSelection(RetrySelectionKind strategy, TransactionId expected, bool classes)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.structure.classes.enabled = classes;
        options.strategy.downstreamRetrySelection = strategy;
        auto events = std::make_shared<EventHub>();
        Core::Impl impl(2, options, events);
        for (size_t index = 0; index < 2; ++index)
        {
            auto& transaction = AddRequest(impl, CHI::Eb::Opcodes::REQ::ReadNoSnp,
                4 + index, 100 + index, 0xd0000, 15);
            BackendRetryRecord record;
            record.request = transaction.request;
            record.request.target = 6;
            record.transaction = transaction.handle;
            record.serial = index;
            record.bank = 0;
            record.classId = 0;
            record.retryAckPCrdType = 0;
            record.retryAckSource = 6;
            impl.backendRetries.emplace(transaction.handle, record);
        }
        impl.backendCredits[{ 6, 0 }] = 1;
        impl.AdvanceBackendRetries();
        LUNATIC_CHECK(impl.txreq.size() == 1);
        LUNATIC_CHECK(impl.txreq.front().flit.txnId == expected);
        LUNATIC_CHECK(impl.backendRetries.size() == 1);
    }

}

LUNATIC_TEST(Stage10ResponseStrategiesImmediateForms)
{
    CheckImmediateResponse(ImmediateResponseKind::EarlyDBID,
        { CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::Opcodes::RSP::Comp });
    CheckImmediateResponse(ImmediateResponseKind::LateComp,
        { CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::Opcodes::RSP::DBIDResp });
    CheckImmediateResponse(ImmediateResponseKind::Combined,
        { CHI::Eb::Opcodes::RSP::CompDBIDResp });
    CheckImmediateResponse(ImmediateResponseKind::Rotate,
        { CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::Opcodes::RSP::Comp });
}

LUNATIC_TEST(Stage10ResponseStrategiesStashFormsAndFallback)
{
    CheckStashResponse(StashResponseKind::CompFirst,
        { CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::Opcodes::RSP::StashDone });
    CheckStashResponse(StashResponseKind::StashDoneFirst,
        { CHI::Eb::Opcodes::RSP::StashDone, CHI::Eb::Opcodes::RSP::Comp });
    CheckStashResponse(StashResponseKind::Combined,
        { CHI::Eb::Opcodes::RSP::CompStashDone });

    Options options;
    options.strategy.stashResponse = StashResponseKind::Combined;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = AddRequest(impl, CHI::Eb::Opcodes::REQ::StashOnceUnique, 4, 12);
    transaction.homeHit = true;
    impl.ResolveStash(transaction);
    impl.ActionStash(transaction);
    LUNATIC_CHECK((Responses(impl) == std::vector<uint16_t>{ CHI::Eb::Opcodes::RSP::Comp }));
}

LUNATIC_TEST(Stage10ResponseStrategiesDctDefaultAndEventOverride)
{
    Options options;
    options.dctEnabled = true;
    options.policy = PolicyKind::NINE;
    options.strategy.forwardDct = false;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = PrepareDct(impl, false);
    impl.PlanReadSnoops(transaction);
    FlowState& flow = impl.flows.at(transaction.handle);
    LUNATIC_CHECK(!flow.dctActive);
    LUNATIC_CHECK(impl.SnoopOpcode(transaction, flow, flow.dctTarget) == CHI::Eb::Opcodes::SNP::SnpShared);

    size_t calls = 0;
    events->OnSnoopForwarding.Register(
        Gravity::MakeListener<SnoopForwardingEvent>("strategy-dct-override", 0,
            [&](SnoopForwardingEvent& event) {
                LUNATIC_CHECK(!event.IsForwarding());
                event.UseForwarding();
                ++calls;
            }));
    flow.dctActive = true;
    impl.PlanReadSnoops(transaction);
    LUNATIC_CHECK(calls == 1);
    LUNATIC_CHECK(flow.dctActive);
    LUNATIC_CHECK(impl.SnoopOpcode(transaction, flow, flow.dctTarget) == CHI::Eb::Opcodes::SNP::SnpSharedFwd);
}

LUNATIC_TEST(Stage10ResponseStrategiesDctProviderRoundRobin)
{
    Options options;
    options.dctEnabled = true;
    options.policy = PolicyKind::NINE;
    options.strategy.providerSelection = ProviderSelectionKind::RoundRobin;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = PrepareDct(impl, true);
    FlowState& flow = impl.flows.at(transaction.handle);
    impl.PlanReadSnoops(transaction);
    LUNATIC_CHECK(flow.dctTarget == 4);
    impl.PlanReadSnoops(transaction);
    LUNATIC_CHECK(flow.dctTarget == 5);
    LUNATIC_CHECK(impl.providerNext.at(transaction.request.Line()) == 2);
    impl.Reset();
    LUNATIC_CHECK(impl.providerNext.empty());
}

LUNATIC_TEST(Stage10ResponseStrategiesDctProviderLowest)
{
    Options options;
    options.dctEnabled = true;
    options.policy = PolicyKind::NINE;
    options.strategy.providerSelection = ProviderSelectionKind::LowestNodeId;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = PrepareDct(impl, true);
    FlowState& flow = impl.flows.at(transaction.handle);
    impl.PlanReadSnoops(transaction);
    LUNATIC_CHECK(flow.dctTarget == 4);
    impl.PlanReadSnoops(transaction);
    LUNATIC_CHECK(flow.dctTarget == 4);
}

LUNATIC_TEST(Stage10ResponseStrategiesUpstreamRetrySelection)
{
    CheckUpstreamSelection(RetrySelectionKind::Oldest, 0);
    CheckUpstreamSelection(RetrySelectionKind::Newest, 1);
}

LUNATIC_TEST(Stage10ResponseStrategiesDownstreamRetrySelection)
{
    CheckDownstreamSelection(RetrySelectionKind::Oldest, 100, false);
    CheckDownstreamSelection(RetrySelectionKind::Newest, 101, false);
    CheckDownstreamSelection(RetrySelectionKind::Oldest, 100, true);
    CheckDownstreamSelection(RetrySelectionKind::Newest, 101, true);
}
