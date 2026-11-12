#include "driver.hpp"

#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Request OperationRequest(uint16_t opcode, NodeId source, TransactionId txnId, uint64_t address = 0xe0000)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = opcode;
        request.address = address;
        request.size = IsAtomicRequest(opcode) ? 2 : 6;
        request.allowRetry = false;
        request.snpAttr = IsCoherentRequest(opcode);
        request.memAttr = ClassifyRequest(opcode) == RequestClass::Atomic
            ? CHI::Eb::MemAttrs::WriteBack_Allocate : CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    Transaction& AddOperation(Core::Impl& impl, uint16_t opcode, NodeId source, TransactionId txnId,
                              uint64_t address = 0xe0000, uint8_t pGroupId = 0)
    {
        Request request = OperationRequest(opcode, source, txnId, address);
        request.pGroupId = pGroupId;
        impl.AdmitRequest(request);
        Transaction& transaction = *impl.tracker.FindRequest(source, txnId);
        transaction.dbid = transaction.handle;
        return transaction;
    }

    Options OperationOptions(PersistResponseKind strategy, bool direct = false)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.directPersistEnabled = direct;
        options.strategy.persistResponse = strategy;
        return options;
    }

    Transaction& StartPersistence(Core::Impl& impl, PersistResponseKind, bool direct = false,
                                  uint16_t opcode = CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
    {
        auto& transaction = AddOperation(impl, opcode, 4, 30, 0xe0000,
            opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep ? 0x66 : 0);
        impl.flows.at(transaction.handle).directPersistActive = direct;
        impl.ActionPersistence(transaction);
        return transaction;
    }

    Response PersistenceResponse(const Transaction& transaction, uint16_t opcode)
    {
        Response response;
        response.source = 6;
        response.target = 2;
        response.txnId = opcode == CHI::Eb::Opcodes::RSP::Persist ? 0 : transaction.handle;
        response.opcode = opcode;
        response.resp = CHI::Eb::Resps::Comp_UC;
        response.pGroupId = transaction.request.pGroupId;
        return response;
    }

    std::vector<uint16_t> FinishPersistence(Core::Impl& impl, Transaction& transaction)
    {
        std::vector<uint16_t> opcodes;
        for (size_t count = 0; count < 2; ++count)
        {
            impl.ActionPersistence(transaction);
            const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
            if (!response)
                break;
            opcodes.push_back(response->opcode);
        }
        return opcodes;
    }

    void CheckSeparatePersistence(PersistResponseKind strategy, const std::vector<uint16_t>& expected,
                                  bool persistFirst)
    {
        auto events = std::make_shared<EventHub>();
        Core::Impl impl(2, OperationOptions(strategy), events);
        auto& transaction = StartPersistence(impl, strategy);
        LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
        if (persistFirst)
        {
            LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
                PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Persist)));
            LUNATIC_CHECK(impl.flows.at(transaction.handle).backendOperationOwned);
            LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
                PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
        }
        else
        {
            LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
                PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            LUNATIC_CHECK(impl.flows.at(transaction.handle).backendOperationOwned);
            LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
                PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Persist)));
        }
        LUNATIC_CHECK(!impl.flows.at(transaction.handle).backendOperationOwned);
        LUNATIC_CHECK(FinishPersistence(impl, transaction) == expected);
        LUNATIC_CHECK(!impl.flows.at(transaction.handle).responseOwned);
        impl.Retire(transaction.handle);
        LUNATIC_CHECK(impl.ValidateInvariants());
    }

    void CheckCombinedPersistence(PersistResponseKind strategy)
    {
        auto events = std::make_shared<EventHub>();
        Core::Impl impl(2, OperationOptions(strategy), events);
        auto& transaction = StartPersistence(impl, strategy);
        LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
        LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
            PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::CompPersist)));
        LUNATIC_CHECK(FinishPersistence(impl, transaction) ==
            std::vector<uint16_t>{ CHI::Eb::Opcodes::RSP::CompPersist });
        LUNATIC_CHECK(!impl.flows.at(transaction.handle).responseOwned);
        impl.Retire(transaction.handle);
    }

    Transaction& PrepareAtomic(Core::Impl& impl, uint16_t opcode)
    {
        auto& transaction = AddOperation(impl, opcode, 4, 40);
        impl.ResolveAtomic(transaction);
        FlowState& flow = impl.flows.at(transaction.handle);
        flow.dbidSent = true;
        flow.dbidEmitted = true;
        flow.compSent = true;
        flow.compEmitted = true;
        flow.atomic.operands.fill(0);
        flow.atomic.operands[0] = 0x29;
        transaction.data.fill(0);
        transaction.data[0] = 0x11;
        transaction.Mark(COMPLETE_WRITE_DATA);
        transaction.phase = Phase::Action;
        return transaction;
    }

    Data AtomicBackendData(const Transaction& transaction, size_t beat)
    {
        Data data;
        data.source = 6;
        data.target = 2;
        data.homeNid = 2;
        data.txnId = transaction.handle;
        data.opcode = CHI::Eb::Opcodes::DAT::CompData;
        data.resp = CHI::Eb::Resps::CompData_I;
        data.dataId = beat * 2;
        data.ccid = 0;
        data.byteEnables.fill(1);
        for (size_t index = 0; index < 32; ++index)
            data.bytes[index] = beat == 0 && index == 0 ? 0x11 : uint8_t(0x80 + beat + index);
        return data;
    }

    void ReadAtomicLine(Core::Impl& impl, Transaction& transaction)
    {
        const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        LUNATIC_CHECK(request);
        LUNATIC_CHECK(request->opcode == CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(request->size == 6);
        for (size_t beat = 0; beat < 2; ++beat)
            LUNATIC_CHECK(impl.HandleBackendDAT(transaction, AtomicBackendData(transaction, beat)));
    }

}

LUNATIC_TEST(Stage10OperationStrategiesSeparateCompFirst)
{
    CheckSeparatePersistence(PersistResponseKind::SeparateCompFirst,
        { CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::Opcodes::RSP::Persist }, false);
}

LUNATIC_TEST(Stage10OperationStrategiesSeparatePersistFirst)
{
    CheckSeparatePersistence(PersistResponseKind::SeparatePersistFirst,
        { CHI::Eb::Opcodes::RSP::Persist, CHI::Eb::Opcodes::RSP::Comp }, true);
}

LUNATIC_TEST(Stage10OperationStrategiesDownstreamCombined)
{
    CheckCombinedPersistence(PersistResponseKind::Downstream);
    CheckCombinedPersistence(PersistResponseKind::Combined);
}

LUNATIC_TEST(Stage10OperationStrategiesDownstreamSeparateOrder)
{
    CheckSeparatePersistence(PersistResponseKind::Downstream,
        { CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::Opcodes::RSP::Persist }, false);
    CheckSeparatePersistence(PersistResponseKind::Downstream,
        { CHI::Eb::Opcodes::RSP::Persist, CHI::Eb::Opcodes::RSP::Comp }, true);
}

LUNATIC_TEST(Stage10OperationStrategiesCombinedOverridesSeparateDownstream)
{
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, OperationOptions(PersistResponseKind::Combined), events);
    auto& transaction = StartPersistence(impl, PersistResponseKind::Combined);
    LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
    LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
        PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
    LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
        PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Persist)));
    LUNATIC_CHECK(FinishPersistence(impl, transaction) ==
        std::vector<uint16_t>{ CHI::Eb::Opcodes::RSP::CompPersist });
    impl.Retire(transaction.handle);
}

LUNATIC_TEST(Stage10OperationStrategiesDirectPersistFallback)
{
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, OperationOptions(PersistResponseKind::SeparatePersistFirst, true), events);
    auto& transaction = StartPersistence(impl, PersistResponseKind::SeparatePersistFirst, true);
    const auto downstream = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
    LUNATIC_CHECK(downstream && downstream->returnNid == 4);
    LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
        PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
    LUNATIC_CHECK(FinishPersistence(impl, transaction) ==
        std::vector<uint16_t>{ CHI::Eb::Opcodes::RSP::Comp });
    LUNATIC_CHECK(!impl.flows.at(transaction.handle).persistSent);
    impl.Retire(transaction.handle);
}

LUNATIC_TEST(Stage10OperationStrategiesNonSeparatedPersistFallback)
{
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, OperationOptions(PersistResponseKind::SeparatePersistFirst), events);
    auto& transaction = StartPersistence(impl, PersistResponseKind::SeparatePersistFirst, false,
        CHI::Eb::Opcodes::REQ::CleanSharedPersist);
    LUNATIC_CHECK(impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
    LUNATIC_CHECK(impl.HandleBackendRSP(transaction,
        PersistenceResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
    LUNATIC_CHECK(FinishPersistence(impl, transaction) ==
        std::vector<uint16_t>{ CHI::Eb::Opcodes::RSP::Comp });
    LUNATIC_CHECK(!impl.flows.at(transaction.handle).persistSent);
    impl.Retire(transaction.handle);
}

LUNATIC_TEST(Stage10OperationStrategiesLocalAtomicSwap)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.strategy.atomicExecution = AtomicExecutionKind::Local;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = PrepareAtomic(impl, CHI::Eb::Opcodes::REQ::AtomicSwap);
    impl.ActionAtomic(transaction);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).backendExpectedBeatMask == 3);
    ReadAtomicLine(impl, transaction);
    LUNATIC_CHECK(transaction.Complete(COMPLETE_MEMORY));
    LUNATIC_CHECK(!impl.flows.at(transaction.handle).backendOperationOwned);

    impl.ActionAtomic(transaction);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).directoryCommitted);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).atomic.execution.write);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).atomic.execution.data[0] == 0x29);
    LUNATIC_CHECK(transaction.data[0] == 0x11);
    impl.ReturnAtomic(transaction);
    const auto result = impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
    LUNATIC_CHECK(result && result->bytes[0] == 0x11);
    LUNATIC_CHECK(impl.Bank(transaction.request.Line()).directory.Find(transaction.request.Line()));
    LUNATIC_CHECK(impl.Bank(transaction.request.Line()).directory.Find(transaction.request.Line())->data[0] == 0x29);
    impl.Retire(transaction.handle);
}

LUNATIC_TEST(Stage10OperationStrategiesLocalAtomicStore)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.strategy.atomicExecution = AtomicExecutionKind::Local;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = PrepareAtomic(impl, CHI::Eb::Opcodes::REQ::AtomicStore::ADD);
    impl.ActionAtomic(transaction);
    ReadAtomicLine(impl, transaction);
    impl.ActionAtomic(transaction);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).directoryCommitted);
    LUNATIC_CHECK(transaction.data[0] == 0x11);
    LUNATIC_CHECK(impl.Bank(transaction.request.Line()).directory.Find(transaction.request.Line()));
    LUNATIC_CHECK(impl.Bank(transaction.request.Line()).directory.Find(transaction.request.Line())->data[0] == 0x3a);
    LUNATIC_CHECK(!impl.flows.at(transaction.handle).atomic.returnSent);
    impl.Retire(transaction.handle);
}

LUNATIC_TEST(Stage10OperationStrategiesBackendAtomicDefault)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.strategy.atomicExecution = AtomicExecutionKind::Backend;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& transaction = PrepareAtomic(impl, CHI::Eb::Opcodes::REQ::AtomicSwap);
    impl.ActionAtomic(transaction);
    LUNATIC_CHECK(!impl.txreq.empty());
    LUNATIC_CHECK(impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::AtomicSwap);
    LUNATIC_CHECK(impl.flows.at(transaction.handle).backendOperationOwned);
    impl.Retire(transaction.handle);
}
