#include "driver.hpp"

#include <array>
#include <limits>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Options WeightedOptions()
    {
        Options options = MakeType7Options();
        options.backend = BackendKind::Internal;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        options.policy = PolicyKind::NINE;
        options.inputQueueLimit = 96;
        options.outputQueueLimit = 96;
        options.structure.requestBufferLimit = 0;
        options.structure.responseResourceLimit = 0;
        options.structure.memoryResourceLimit = 0;
        options.structure.refillResourceLimit = 0;
        return options;
    }

    uint8_t ClassQos(size_t classId)
    {
        constexpr std::array<uint8_t, 4> qos = { 15, 12, 8, 0 };
        return qos[classId];
    }

    Request WeightedRequest(NodeId source, TransactionId txnId, size_t classId, size_t alias = 0)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.address = 0xe0000 + alias * 4 * LINE_SIZE;
        request.size = 6;
        request.qos = ClassQos(classId);
        request.pCrdType = classId;
        request.allowRetry = true;
        request.memAttr = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    Transaction* AddTransaction(Core::Impl& impl, NodeId source, TransactionId txnId,
                                size_t classId, size_t alias)
    {
        const Request request = WeightedRequest(source, txnId, classId, alias);
        impl.AdmitRequest(request, classId);
        Transaction* transaction = impl.tracker.FindRequest(source, txnId);
        LUNATIC_CHECK(transaction);
        return transaction;
    }

    void SeedRetry(Core::Impl& impl, size_t index, size_t classId, Time age, size_t alias = 0,
                   NodeId source = std::numeric_limits<NodeId>::max())
    {
        RetryRecord record;
        const NodeId requester = source == std::numeric_limits<NodeId>::max() ? 100 + index : source;
        record.request = WeightedRequest(requester, 1000 + index, classId, alias);
        record.bank = 0;
        record.classId = classId;
        record.age = age;
        record.ackPopped = true;
        impl.retries.emplace(index, record);
        impl.AcquireRetrySource(record.request);
    }

    std::vector<uint8_t> GrantClasses(const Core::Impl& impl)
    {
        std::vector<uint8_t> result;
        for (const auto& pending : impl.txrsp)
            if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant)
                result.push_back(pending.flit.pCrdType);
        return result;
    }

    std::vector<size_t> GrantIndices(const Core::Impl& impl)
    {
        std::vector<size_t> result;
        for (const auto& pending : impl.txrsp)
            if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant)
                result.push_back(pending.retry);
        return result;
    }

    void CheckRound(const WeightedRoundState& actual, const WeightedRoundState& expected)
    {
        LUNATIC_CHECK(actual.index == expected.index);
        LUNATIC_CHECK(actual.remaining == expected.remaining);
    }

    template<size_t N>
    void CheckRounds(const std::array<WeightedRoundState, N>& actual,
                     const std::array<WeightedRoundState, N>& expected)
    {
        for (size_t index = 0; index < N; ++index)
            CheckRound(actual[index], expected[index]);
    }

    void ResetAndCheck(Core::Impl& impl)
    {
        impl.Reset();
        LUNATIC_CHECK(impl.retrySources.empty());
        LUNATIC_CHECK(impl.availableCredits.empty());
        LUNATIC_CHECK(impl.cancellationCredits.empty());
        LUNATIC_CHECK(impl.TotalRetryReservations() == 0);
        LUNATIC_CHECK(!impl.HasInflight());
        LUNATIC_CHECK(impl.ValidateInvariants());
        CheckRounds(impl.retryRounds, {});
        CheckRounds(impl.backendRetryRounds, {});
        CheckRound(impl.responseCategoryRound, {});
        CheckRounds(impl.responseClassRounds, {});
    }

    Response TransactionResponse(const Transaction& transaction, uint16_t opcode)
    {
        Response response;
        response.source = 2;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.opcode = opcode;
        response.qos = transaction.request.qos;
        return response;
    }

    std::vector<NodeId> RunNormalResponses(Core::Impl& impl)
    {
        for (size_t index = 0; index < 6; ++index)
        {
            const size_t classId = index < 4 ? 0 : 1;
            Transaction* transaction = AddTransaction(impl, 10 + index, 100 + index, classId, index);
            if (classId == 0)
                const_cast<Request&>(transaction->request).qos = 8;
            const auto response = TransactionResponse(*transaction, CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(impl.Queue(impl.txrsp, response, transaction->handle, 9));
        }

        std::vector<NodeId> result;
        while (!impl.txrsp.empty())
        {
            const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
            LUNATIC_CHECK(response);
            result.push_back(response->target);
        }
        return result;
    }

    TransactionHandle AddBackendRetry(Core::Impl& impl, NodeId source, TransactionId txnId,
                                      size_t classId, size_t alias, bool changeQos)
    {
        Transaction* transaction = AddTransaction(impl, source, txnId, classId, alias);
        FlowState& flow = impl.flows.at(transaction->handle);
        flow.backendRequest = transaction->request;
        flow.backendRequest.source = 2;
        flow.backendRequest.target = 6;
        flow.backendRequest.txnId = transaction->handle;
        flow.backendRequest.allowRetry = true;
        transaction->memoryRequired = true;
        Response ack;
        ack.source = 6;
        ack.target = 2;
        ack.txnId = transaction->handle;
        ack.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
        ack.pCrdType = 7;
        LUNATIC_CHECK(impl.HandleBackendRetry(*transaction, ack));
        auto& record = impl.backendRetries.at(transaction->handle);
        record.serial = 5;
        if (changeQos)
            record.request.qos = 8;
        return transaction->handle;
    }

}

LUNATIC_TEST(Stage10WeightedRetryFullClassAckAndLatchedResend)
{
    Options options = WeightedOptions();
    options.structure.classes.maximum[0] = 1;
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    Transaction* original = AddTransaction(impl, 4, 10, 0, 0);

    auto request = WeightedRequest(5, 11, 0, 1);
    request.target = 0;
    LUNATIC_CHECK(impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
    impl.AdmitRequests();
    LUNATIC_CHECK(impl.retries.size() == 1);
    LUNATIC_CHECK(impl.rxreq.empty());
    LUNATIC_CHECK(impl.TotalRetryReservations() == 0);
    const auto ack = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
    LUNATIC_CHECK(ack && ack->opcode == CHI::Eb::Opcodes::RSP::RetryAck && ack->pCrdType == 0);

    impl.QueueRetryCredits();
    LUNATIC_CHECK(impl.txrsp.empty());
    LUNATIC_CHECK(!impl.retries.at(0).creditReserved);
    impl.Retire(original->handle);
    impl.QueueRetryCredits();
    LUNATIC_CHECK(impl.txrsp.size() == 1);
    LUNATIC_CHECK(impl.retries.at(0).creditReserved);
    LUNATIC_CHECK(impl.banks[0].retryReservations[0] == 1);
    const auto grant = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
    LUNATIC_CHECK(grant && grant->opcode == CHI::Eb::Opcodes::RSP::PCrdGrant && grant->pCrdType == 0);

    request.target = 2;
    request.txnId = 900;
    request.qos = 8;
    request.allowRetry = false;
    request.pCrdType = grant->pCrdType;
    LUNATIC_CHECK(impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
    impl.AdmitRequests();
    Transaction* resent = impl.tracker.FindRequest(5, 900);
    LUNATIC_CHECK(resent);
    LUNATIC_CHECK(impl.flows.at(resent->handle).assignedClass == 0);
    LUNATIC_CHECK(impl.banks[0].activeClasses[0] == 1);
    LUNATIC_CHECK(impl.banks[0].activeClasses[2] == 0);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 0);
    LUNATIC_CHECK(impl.retrySources.empty());
    impl.Retire(resent->handle);
    LUNATIC_CHECK(impl.banks[0].activeClasses[0] == 0);
    LUNATIC_CHECK(!impl.HasInflight());
    LUNATIC_CHECK(impl.ValidateInvariants());
}

LUNATIC_TEST(Stage10WeightedRetryUnitAndNonzeroWeights)
{
    {
        Options options = WeightedOptions();
        options.structure.classes.maximum.fill(8);
        options.structure.classes.dedicated.fill(0);
        options.structure.classes.contendedMinimum.fill(0);
        Core::Impl impl(2, options, std::make_shared<EventHub>());
        SeedRetry(impl, 0, 0, 5);
        SeedRetry(impl, 1, 0, 5);
        SeedRetry(impl, 2, 1, 5);
        SeedRetry(impl, 3, 1, 5);
        for (size_t count = 0; count < 4; ++count)
            impl.QueueRetryCredits();
        LUNATIC_CHECK((GrantClasses(impl) == std::vector<uint8_t>{ 0, 1, 0, 1 }));
        LUNATIC_CHECK(impl.ValidateInvariants());
        ResetAndCheck(impl);
    }

    {
        Options options = WeightedOptions();
        options.structure.classes.maximum.fill(8);
        options.structure.classes.dedicated.fill(0);
        options.structure.classes.contendedMinimum.fill(0);
        options.structure.retrySourceLimit = 0;
        options.structure.classes.retryWeights[0] = 2;
        Core::Impl impl(2, options, std::make_shared<EventHub>());
        for (size_t index = 0; index < 4; ++index)
            SeedRetry(impl, index, 0, 5, index);
        SeedRetry(impl, 4, 1, 5, 4);
        SeedRetry(impl, 5, 1, 5, 5);
        for (size_t count = 0; count < 6; ++count)
            impl.QueueRetryCredits();
        LUNATIC_CHECK((GrantClasses(impl) == std::vector<uint8_t>{ 0, 0, 1, 0, 0, 1 }));
        LUNATIC_CHECK(impl.TotalRetryReservations() == 6);
        LUNATIC_CHECK(impl.ValidateInvariants());
        ResetAndCheck(impl);
    }
}

LUNATIC_TEST(Stage10WeightedRetryOldestAndCapacityTiers)
{
    {
        Options options = WeightedOptions();
        options.structure.classes.maximum.fill(4);
        options.structure.classes.dedicated[0] = 1;
        options.structure.classes.contendedMinimum[0] = 1;
        options.structure.classes.contendedMinimum[2] = 0;
        Core::Impl impl(2, options, std::make_shared<EventHub>());
        SeedRetry(impl, 0, 2, 1);
        SeedRetry(impl, 1, 2, 1, 1);
        SeedRetry(impl, 2, 0, 2, 2);
        impl.QueueRetryCredits();
        impl.QueueRetryCredits();
        impl.QueueRetryCredits();
        LUNATIC_CHECK((GrantClasses(impl) == std::vector<uint8_t>{ 2, 2, 0 }));
        LUNATIC_CHECK((GrantIndices(impl) == std::vector<size_t>{ 0, 1, 2 }));
        ResetAndCheck(impl);
    }

    {
        Options options = WeightedOptions();
        options.structure.classes.maximum.fill(4);
        options.structure.classes.dedicated[0] = 1;
        options.structure.classes.contendedMinimum[0] = 1;
        options.structure.classes.contendedMinimum[1] = 2;
        options.structure.classes.contendedMinimum[2] = 0;
        Core::Impl impl(2, options, std::make_shared<EventHub>());
        SeedRetry(impl, 0, 0, 4);
        SeedRetry(impl, 1, 1, 4, 1);
        SeedRetry(impl, 2, 2, 4, 2);
        impl.QueueRetryCredits();
        impl.QueueRetryCredits();
        impl.QueueRetryCredits();
        LUNATIC_CHECK((GrantClasses(impl) == std::vector<uint8_t>{ 0, 1, 2 }));
        ResetAndCheck(impl);
    }

    {
        Options options = WeightedOptions();
        options.structure.classes.maximum[3] = 1;
        options.structure.classes.contendedMinimum[3] = 15;
        ValidateOptions(options);
        Core::Impl impl(2, options, std::make_shared<EventHub>());
        SeedRetry(impl, 0, 3, 4);
        SeedRetry(impl, 1, 3, 4, 1);
        impl.QueueRetryCredits();
        impl.QueueRetryCredits();
        LUNATIC_CHECK((GrantClasses(impl) == std::vector<uint8_t>{ 3 }));
        LUNATIC_CHECK(!impl.retries.at(1).creditReserved);
        LUNATIC_CHECK(impl.ValidateInvariants());
        ResetAndCheck(impl);
    }
}

LUNATIC_TEST(Stage10WeightedRetryOutputBackpressureNoDuplicates)
{
    Options options = WeightedOptions();
    options.outputQueueLimit = 2;
    options.structure.classes.maximum.fill(4);
    options.structure.classes.dedicated.fill(0);
    options.structure.classes.contendedMinimum.fill(0);
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    SeedRetry(impl, 0, 0, 3);
    SeedRetry(impl, 1, 1, 3, 1);
    SeedRetry(impl, 2, 1, 3, 2);
    impl.QueueRetryCredits();
    impl.QueueRetryCredits();
    LUNATIC_CHECK(impl.txrsp.size() == 2);
    const auto rounds = impl.retryRounds;
    impl.QueueRetryCredits();
    CheckRounds(impl.retryRounds, rounds);
    LUNATIC_CHECK(impl.txrsp.size() == 2);
    LUNATIC_CHECK(!impl.retries.at(2).creditReserved);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 2);

    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
    impl.QueueRetryCredits();
    LUNATIC_CHECK(impl.retries.at(2).creditReserved);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 3);
    const auto indices = GrantIndices(impl);
    LUNATIC_CHECK(indices.size() == 2);
    LUNATIC_CHECK(indices[0] != indices[1]);
    LUNATIC_CHECK(impl.ValidateInvariants());
    ResetAndCheck(impl);
}

LUNATIC_TEST(Stage10WeightedRetryCrossOriginalCreditReturn)
{
    Options options = WeightedOptions();
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    SeedRetry(impl, 0, 0, 3, 0, 4);
    SeedRetry(impl, 1, 0, 3, 1, 4);
    impl.QueueRetryCredits();
    impl.QueueRetryCredits();
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
    LUNATIC_CHECK(impl.availableCredits.at({ 4, 0 }) == 2);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 2);

    impl.Push(impl.rxreq, Stage10PCrdReturn(4, 2, 0, 0, 0), EventKind::AcceptedRXREQ);
    impl.AdmitRequests();
    LUNATIC_CHECK(impl.availableCredits.at({ 4, 0 }) == 1);
    LUNATIC_CHECK(impl.retries.at(0).creditReserved);
    LUNATIC_CHECK(!impl.retries.at(1).creditReserved);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 1);
    LUNATIC_CHECK(impl.cancellationCredits.at({ 4, 0 }) == 1);
    LUNATIC_CHECK(impl.RetrySourceCount() == 1);
    LUNATIC_CHECK(impl.ValidateInvariants());

    impl.Push(impl.rxreq, Stage10PCrdReturn(4, 2, 0, 0, 0), EventKind::AcceptedRXREQ);
    impl.AdmitRequests();
    LUNATIC_CHECK(impl.retries.empty());
    LUNATIC_CHECK(impl.retrySources.empty());
    LUNATIC_CHECK(impl.availableCredits.empty());
    LUNATIC_CHECK(impl.cancellationCredits.empty());
    LUNATIC_CHECK(impl.TotalRetryReservations() == 0);
    LUNATIC_CHECK(!impl.HasInflight());
    LUNATIC_CHECK(impl.ValidateInvariants());
}

LUNATIC_TEST(Stage10WeightedRetryReturnedOriginalNeverRegranted)
{
    Options options = WeightedOptions();
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    SeedRetry(impl, 0, 0, 1, 0, 4);
    SeedRetry(impl, 1, 0, 2, 1, 4);
    SeedRetry(impl, 2, 0, 3, 2, 4);
    impl.retries.at(2).ackPopped = false;
    impl.QueueRetryCredits();
    impl.QueueRetryCredits();
    LUNATIC_CHECK((GrantIndices(impl) == std::vector<size_t>{ 0, 1 }));
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));

    impl.Push(impl.rxreq, Stage10PCrdReturn(4, 2, 0, 0, 0), EventKind::AcceptedRXREQ);
    impl.AdmitRequests();
    LUNATIC_CHECK(impl.retries.at(0).creditReturned);
    LUNATIC_CHECK(!impl.retries.at(0).creditReserved);
    LUNATIC_CHECK(!impl.retries.at(0).grantQueued);
    LUNATIC_CHECK(!impl.RetryCreditEligible(impl.retries.at(0)));
    LUNATIC_CHECK(impl.cancellationCredits.at({ 4, 0 }) == 1);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 1);
    LUNATIC_CHECK(impl.ValidateInvariants());

    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
    auto resend = impl.retries.at(1).request;
    resend.allowRetry = false;
    LUNATIC_CHECK(impl.Push(impl.rxreq, resend, EventKind::AcceptedRXREQ));
    impl.AdmitRequests();
    Transaction* transaction = impl.tracker.FindRequest(4, 1001);
    LUNATIC_CHECK(transaction);
    impl.Retire(transaction->handle);
    LUNATIC_CHECK(impl.retries.contains(0));
    LUNATIC_CHECK(!impl.retries.contains(1));
    LUNATIC_CHECK(impl.retries.at(0).creditReturned);

    SeedRetry(impl, 3, 0, 4, 3, 4);
    impl.QueueRetryCredits();
    LUNATIC_CHECK((GrantIndices(impl) == std::vector<size_t>{ 3 }));
    LUNATIC_CHECK(impl.retries.at(3).creditReserved);
    LUNATIC_CHECK(!impl.retries.at(0).creditReserved);
    LUNATIC_CHECK(impl.TotalRetryReservations() == 1);
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));

    resend = impl.retries.at(3).request;
    resend.allowRetry = false;
    LUNATIC_CHECK(impl.Push(impl.rxreq, resend, EventKind::AcceptedRXREQ));
    impl.AdmitRequests();
    transaction = impl.tracker.FindRequest(4, 1003);
    LUNATIC_CHECK(transaction);
    impl.Retire(transaction->handle);
    LUNATIC_CHECK(impl.retries.contains(0));
    LUNATIC_CHECK(!impl.retries.contains(3));
    LUNATIC_CHECK(impl.TotalRetryReservations() == 0);

    impl.retries.at(2).ackPopped = true;
    impl.QueueRetryCredits();
    LUNATIC_CHECK((GrantIndices(impl) == std::vector<size_t>{ 2 }));
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
    impl.Push(impl.rxreq, Stage10PCrdReturn(4, 2, 0, 0, 0), EventKind::AcceptedRXREQ);
    impl.AdmitRequests();
    LUNATIC_CHECK(impl.retries.empty());
    LUNATIC_CHECK(impl.retrySources.empty());
    LUNATIC_CHECK(impl.availableCredits.empty());
    LUNATIC_CHECK(impl.cancellationCredits.empty());
    LUNATIC_CHECK(impl.TotalRetryReservations() == 0);
    impl.Tick();
    LUNATIC_CHECK(!impl.HasInflight());
    LUNATIC_CHECK(impl.ValidateInvariants());
}

LUNATIC_TEST(Stage10WeightedRetryResponseCategoriesAndPeekPurity)
{
    Options options = WeightedOptions();
    options.outputQueueLimit = 6;
    options.structure.classes.fastResponseWeight = 2;
    options.structure.classes.retryResponseWeight = 1;
    options.structure.classes.staticGrantResponseWeight = 1;
    options.structure.classes.normalResponseWeight = 1;
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    SeedRetry(impl, 0, 0, 1);
    SeedRetry(impl, 1, 1, 1, 1);

    std::array<TransactionHandle, 4> handles{};
    for (size_t index = 0; index < handles.size(); ++index)
        handles[index] = AddTransaction(impl, 20 + index, 200 + index,
            index == 0 || index == 3 ? 3 : 2, index)->handle;
    LUNATIC_CHECK(impl.Queue(impl.txrsp,
        TransactionResponse(*impl.tracker.Find(handles[0]), CHI::Eb::Opcodes::RSP::Comp), handles[0], 6));
    LUNATIC_CHECK(impl.Queue(impl.txrsp,
        TransactionResponse(*impl.tracker.Find(handles[1]), CHI::Eb::Opcodes::RSP::DBIDResp), handles[1], 7));
    LUNATIC_CHECK(impl.Queue(impl.txrsp,
        TransactionResponse(*impl.tracker.Find(handles[2]), CHI::Eb::Opcodes::RSP::CompDBIDResp), handles[2], 7));
    Response ack;
    ack.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, ack, INVALID_TRANSACTION, 7, false, false, 0));
    Response grant;
    grant.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, grant, INVALID_TRANSACTION, 7, false, false, 1));
    LUNATIC_CHECK(impl.Queue(impl.txrsp,
        TransactionResponse(*impl.tracker.Find(handles[3]), CHI::Eb::Opcodes::RSP::Comp), handles[3], 7));
    LUNATIC_CHECK(!impl.Queue(impl.txrsp, grant, INVALID_TRANSACTION, 7, false, false, 1));
    for (const auto& pending : impl.txrsp)
    {
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::RetryAck)
            LUNATIC_CHECK(impl.ResponseClass(pending) == 0);
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant)
            LUNATIC_CHECK(impl.ResponseClass(pending) == 1);
    }

    const auto categoryRound = impl.responseCategoryRound;
    const auto classRounds = impl.responseClassRounds;
    const auto first = impl.Peek(impl.txrsp);
    LUNATIC_CHECK(first && first->opcode == CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(impl.Peek(impl.txrsp) && impl.Peek(impl.txrsp)->opcode == first->opcode);
    CheckRound(impl.responseCategoryRound, categoryRound);
    CheckRounds(impl.responseClassRounds, classRounds);
    LUNATIC_CHECK(impl.txrsp.size() == 6);
    LUNATIC_CHECK(impl.pmu.emittedTXRSP == 0);

    const std::array<uint16_t, 6> expected = {
        CHI::Eb::Opcodes::RSP::Comp,
        CHI::Eb::Opcodes::RSP::DBIDResp,
        CHI::Eb::Opcodes::RSP::CompDBIDResp,
        CHI::Eb::Opcodes::RSP::RetryAck,
        CHI::Eb::Opcodes::RSP::PCrdGrant,
        CHI::Eb::Opcodes::RSP::Comp
    };
    for (const uint16_t opcode : expected)
    {
        const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        LUNATIC_CHECK(response && response->opcode == opcode);
    }
    ResetAndCheck(impl);
}

LUNATIC_TEST(Stage10WeightedRetryNormalClassWeightsAndResetReplay)
{
    Options options = WeightedOptions();
    options.structure.classes.requestWeights[0] = 2;
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    const std::vector<NodeId> expected = { 10, 11, 14, 12, 13, 15 };
    LUNATIC_CHECK(RunNormalResponses(impl) == expected);
    ResetAndCheck(impl);
    LUNATIC_CHECK(RunNormalResponses(impl) == expected);
    ResetAndCheck(impl);
}

LUNATIC_TEST(Stage10WeightedRetryBackendOriginalClassAndOutputRefusal)
{
    Options options = WeightedOptions();
    options.outputQueueLimit = 1;
    options.structure.classes.dedicated.fill(0);
    options.structure.classes.contendedMinimum.fill(0);
    options.structure.classes.retryWeights[0] = 2;
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    const auto first = AddBackendRetry(impl, 4, 300, 0, 0, true);
    const auto second = AddBackendRetry(impl, 5, 301, 0, 1, true);
    const auto third = AddBackendRetry(impl, 6, 302, 1, 2, false);
    impl.backendCredits[{ 6, 7 }] = 3;
    impl.AdvanceBackendRetries();
    LUNATIC_CHECK(impl.backendCredits.empty());
    LUNATIC_CHECK(impl.nextBackendSelection == 3);
    LUNATIC_CHECK(!impl.backendRetries.contains(first));
    LUNATIC_CHECK(impl.backendRetries.at(second).selection == 1);
    LUNATIC_CHECK(impl.backendRetries.at(third).selection == 2);
    LUNATIC_CHECK(impl.txreq.size() == 1);

    impl.AdvanceBackendRetries();
    LUNATIC_CHECK(impl.nextBackendSelection == 3);
    LUNATIC_CHECK(impl.txreq.size() == 1);
    LUNATIC_CHECK(impl.backendRetries.at(second).creditReceived);
    LUNATIC_CHECK(impl.backendRetries.at(third).creditReceived);
    LUNATIC_CHECK(impl.ValidateInvariants());

    for (const TransactionHandle expected : { first, second, third })
    {
        const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        LUNATIC_CHECK(request && request->txnId == expected);
        impl.AdvanceBackendRetries();
    }
    LUNATIC_CHECK(impl.backendRetries.empty());
    LUNATIC_CHECK(impl.nextBackendSelection == 3);
    LUNATIC_CHECK(impl.ValidateInvariants());
    ResetAndCheck(impl);
}

LUNATIC_TEST(Stage10WeightedRetryDisabledLegacyBehavior)
{
    Options options;
    Core::Impl impl(2, options, std::make_shared<EventHub>());
    Response retry;
    retry.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, retry, INVALID_TRANSACTION, 5));
    Response dbid;
    dbid.opcode = CHI::Eb::Opcodes::RSP::DBIDResp;
    LUNATIC_CHECK(impl.Queue(impl.txrsp, dbid, INVALID_TRANSACTION, 4));
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP)->opcode == CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(impl.Pop(impl.txrsp, EventKind::EmittedTXRSP)->opcode == CHI::Eb::Opcodes::RSP::RetryAck);

    Transaction* older = AddTransaction(impl, 4, 400, 0, 0);
    Transaction* newer = AddTransaction(impl, 5, 401, 0, 1);
    for (Transaction* transaction : { older, newer })
    {
        FlowState& flow = impl.flows.at(transaction->handle);
        flow.backendRequest = transaction->request;
        flow.backendRequest.source = 2;
        flow.backendRequest.target = 6;
        flow.backendRequest.txnId = transaction->handle;
        flow.backendRequest.allowRetry = true;
        transaction->memoryRequired = true;
        Response ack;
        ack.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
        ack.pCrdType = 0;
        LUNATIC_CHECK(impl.HandleBackendRetry(*transaction, ack));
    }
    impl.backendCredits[{ 6, 0 }] = 1;
    impl.AdvanceBackendRetries();
    LUNATIC_CHECK(!impl.backendRetries.at(older->handle).creditReceived);
    LUNATIC_CHECK(!impl.backendRetries.contains(newer->handle));
    LUNATIC_CHECK(impl.txreq.size() == 1);
    ResetAndCheck(impl);

    Driver<Config256> driver;
    LUNATIC_CHECK(driver.ReadData(4, 500, 0xf0000, CHI::Eb::Opcodes::REQ::ReadNoSnp) == LineData{});
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}
