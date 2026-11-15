#include "driver.hpp"

#include <array>
#include <map>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using MetadataConfig256 = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 256, true, true, false>;

    Options RetryClassOptions(size_t banks = 1)
    {
        Options options = MakeType7Options();
        options.backend = BackendKind::Internal;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        options.policy = PolicyKind::NINE;
        options.inputQueueLimit = 96;
        options.outputQueueLimit = 96;
        options.structure.bankCount = banks;
        options.structure.requestBufferLimit = 0;
        options.structure.responseResourceLimit = 0;
        options.structure.memoryResourceLimit = 0;
        options.structure.refillResourceLimit = 0;
        return options;
    }

    LineKey ClassLine(size_t bank, size_t alias = 0)
    {
        return LineKey::FromAddress(0xb0000 + bank * LINE_SIZE + alias * 4 * LINE_SIZE, false);
    }

    Request ClassRequest(NodeId source, TransactionId txnId, const LineKey& line, uint8_t qos,
                         uint8_t pCrdType = 0)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.address = line.address;
        request.size = 6;
        request.qos = qos;
        request.pCrdType = pCrdType;
        request.allowRetry = true;
        request.memAttr = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    class ClassHarness {
    public:
        std::shared_ptr<EventHub> events = std::make_shared<EventHub>();
        Core::Impl impl;
        bool forceRetry = false;

        explicit ClassHarness(const Options& options)
            : impl(2, options, events)
        {
            events->OnRequestAdmission.Register(
                Gravity::MakeListener<RequestAdmissionEvent>("class-retry-force", 0,
                    [this](RequestAdmissionEvent& event) {
                        if (forceRetry && event.request.allowRetry)
                            event.RequestRetry();
                    }));
        }

        void Push(const Request& request)
        {
            LUNATIC_CHECK(impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
        }

        void Admit()
        {
            impl.AdmitRequests();
        }

        void Retry(const Request& request)
        {
            Push(request);
            Admit();
        }

        std::optional<Response> PopRSP()
        {
            return impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        }

        std::optional<Response> Grant(size_t index)
        {
            if (!impl.retries.at(index).ackPopped)
            {
                const auto ack = PopRSP();
                LUNATIC_CHECK(ack && ack->opcode == CHI::Eb::Opcodes::RSP::RetryAck);
            }
            impl.QueueRetryCredit(index);
            std::optional<Response> grant;
            do
                grant = PopRSP();
            while (grant && grant->opcode == CHI::Eb::Opcodes::RSP::RetryAck);
            LUNATIC_CHECK(grant && grant->opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
            return grant;
        }

        void Resend(const Request& request)
        {
            Request resend = request;
            resend.allowRetry = false;
            Push(resend);
            Admit();
        }

        void Retire(TransactionHandle handle)
        {
            impl.Retire(handle);
        }

        void Check()
        {
            LUNATIC_CHECK(impl.ValidateInvariants());
        }
    };

    template<typename config>
    void SendBackendReadData(Driver<config>& driver, TransactionId txnId, const LineKey& line)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename Driver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = driver.node.GetNodeId();
            data.HomeNID() = driver.node.GetNodeId();
            data.TxnID() = txnId;
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (line.address >> 4) & 3;
            for (size_t index = 0; index < bytes; ++index)
                data.Data()[index / 8] |= uint64_t(index + beat * bytes) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

}

LUNATIC_TEST(Stage10RetryClassesQosBoundariesAndContendedReset)
{
    Options options = RetryClassOptions();
    options.structure.classes.contendedMinimum[3] = 15;
    ValidateOptions(options);
    ClassHarness harness(options);
    constexpr std::array<size_t, 16> expected = { 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 0 };
    for (uint8_t qos = 0; qos < 16; ++qos)
        LUNATIC_CHECK(harness.impl.ClassForQos(qos) == expected[qos]);
}

LUNATIC_TEST(Stage10RetryClassesMaximumExhaustionAndRestore)
{
    Options options = RetryClassOptions();
    options.structure.classes.maximum[3] = 1;
    options.structure.classes.maximum[2] = 2;
    ClassHarness harness(options);
    const auto first = ClassRequest(4, 10, ClassLine(0, 0), 0);
    const auto second = ClassRequest(5, 11, ClassLine(0, 1), 0);
    const auto other = ClassRequest(6, 12, ClassLine(0, 2), 8);
    harness.Retry(first);
    harness.Retry(other);
    harness.Retry(second);
    LUNATIC_CHECK(harness.impl.tracker.Size() == 2);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 1);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[2] == 1);
    LUNATIC_CHECK(harness.impl.banks[0].activeOriginals == 2);
    harness.Retire(0);
    harness.Retry(second);
    LUNATIC_CHECK(harness.impl.tracker.Size() == 2);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 1);
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesRawAndBankCapacityDominate)
{
    Options options = RetryClassOptions(2);
    options.structure.reservedTransactionEntries = 0;
    options.transactionLimit = 4;
    ClassHarness harness(options);
    harness.Retry(ClassRequest(4, 20, ClassLine(0, 0), 0));
    harness.Retry(ClassRequest(5, 21, ClassLine(0, 1), 0));
    harness.Retry(ClassRequest(7, 23, ClassLine(1, 0), 8));
    harness.Retry(ClassRequest(6, 22, ClassLine(0, 2), 8));
    LUNATIC_CHECK(harness.impl.tracker.Size() == 3);
    LUNATIC_CHECK(harness.impl.banks[0].activeOriginals == 2);
    LUNATIC_CHECK(harness.impl.banks[1].activeOriginals == 1);
    harness.Check();

    Options rawOptions = RetryClassOptions();
    rawOptions.transactionLimit = 3;
    ClassHarness raw(rawOptions);
    raw.Retry(ClassRequest(4, 30, ClassLine(0, 0), 0));
    raw.Retry(ClassRequest(5, 31, ClassLine(0, 1), 8));
    raw.Retry(ClassRequest(6, 32, ClassLine(0, 2), 12));
    LUNATIC_CHECK(raw.impl.tracker.Size() == 2);
    LUNATIC_CHECK(raw.impl.banks[0].activeOriginals == 2);
    raw.Check();
}

LUNATIC_TEST(Stage10RetryClassesDedicatedOwnerAndNonOwner)
{
    Options options = RetryClassOptions();
    options.structure.reservedTransactionEntries = 0;
    options.transactionLimit = 3;
    options.structure.classes.dedicated[3] = 2;
    ClassHarness harness(options);
    harness.Retry(ClassRequest(4, 40, ClassLine(0, 0), 8));
    harness.Retry(ClassRequest(6, 42, ClassLine(0, 2), 0));
    harness.Retry(ClassRequest(7, 43, ClassLine(0, 3), 0));
    harness.Retry(ClassRequest(5, 41, ClassLine(0, 1), 8));
    harness.Retry(ClassRequest(8, 44, ClassLine(0, 4), 12));
    LUNATIC_CHECK(harness.impl.tracker.Size() == 3);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[2] == 1);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 2);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[1] == 0);
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesContendedMinimumDoesNotClamp)
{
    Options options = RetryClassOptions();
    options.structure.classes.maximum[3] = 2;
    options.structure.classes.contendedMinimum[3] = 15;
    ValidateOptions(options);
    ClassHarness harness(options);
    harness.Retry(ClassRequest(4, 50, ClassLine(0, 0), 0));
    harness.Retry(ClassRequest(5, 51, ClassLine(0, 1), 0));
    harness.Retry(ClassRequest(6, 52, ClassLine(0, 2), 0));
    LUNATIC_CHECK(harness.impl.tracker.Size() == 2);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 2);
    LUNATIC_CHECK(harness.impl.banks[0].activeOriginals == 2);
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesRetryAckAndChangedResendPreserveClass)
{
    Options options = RetryClassOptions();
    for (size_t& maximum : options.structure.classes.maximum)
        maximum = 16;
    ClassHarness harness(options);
    for (size_t index = 0; index < 15; ++index)
        harness.Retry(ClassRequest(4 + index, 100 + index, ClassLine(0, index), 0));
    harness.forceRetry = true;
    auto initial = ClassRequest(4, 115, ClassLine(0, 15), 7);
    initial.target = 0;
    harness.Retry(initial);
    const auto ack = harness.PopRSP();
    LUNATIC_CHECK(ack && ack->opcode == CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(ack->pCrdType == 3);
    LUNATIC_CHECK(ack->qos == initial.qos);
    const auto grant = harness.Grant(0);
    LUNATIC_CHECK(grant->pCrdType == 3);
    LUNATIC_CHECK(grant->qos == initial.qos);

    auto resend = initial;
    resend.target = 2;
    resend.txnId = 180;
    resend.qos = 8;
    resend.traceTag = 0x5a;
    resend.allowRetry = false;
    resend.pCrdType = grant->pCrdType;
    harness.Resend(resend);
    Transaction* transaction = harness.impl.tracker.FindRequest(4, 180);
    LUNATIC_CHECK(transaction);
    LUNATIC_CHECK(harness.impl.flows.at(transaction->handle).assignedClass == 3);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 16);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[2] == 0);
    LUNATIC_CHECK(harness.impl.retrySources.empty());
    LUNATIC_CHECK(harness.impl.availableCredits.empty());
    LUNATIC_CHECK(harness.impl.retries.empty());
    const auto handle = transaction->handle;
    harness.Retire(0);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 15);
    harness.Retire(handle);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 14);
    harness.Retire(handle);
    LUNATIC_CHECK(harness.impl.banks[0].activeClasses[3] == 14);
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesResendRefusalPreservesCredit)
{
    Options options = RetryClassOptions();
    options.structure.requestBufferLimit = 1;
    ClassHarness harness(options);
    harness.forceRetry = true;
    const auto original = ClassRequest(4, 175, ClassLine(0, 0), 0);
    harness.Retry(original);
    const auto grant = harness.Grant(0);
    LUNATIC_CHECK(grant);
    harness.forceRetry = false;
    harness.Retry(ClassRequest(5, 176, ClassLine(0, 1), 8));
    LUNATIC_CHECK(harness.impl.tracker.Size() == 1);
    LUNATIC_CHECK(harness.impl.banks[0].requestBufferOccupants == 1);

    harness.Resend(ClassRequest(4, 175, ClassLine(0, 0), 0, grant->pCrdType));
    LUNATIC_CHECK(harness.impl.rxreq.size() == 1);
    LUNATIC_CHECK(harness.impl.tracker.Size() == 1);
    LUNATIC_CHECK(harness.impl.availableCredits.size() == 1);
    LUNATIC_CHECK(harness.impl.retries.size() == 1);
    LUNATIC_CHECK(harness.impl.RetrySourceCount() == 1);
    LUNATIC_CHECK(harness.impl.banks[0].retryReservations[3] == 1);
    harness.Check();

    harness.Retire(0);
    harness.Admit();
    Transaction* transaction = harness.impl.tracker.FindRequest(4, 175);
    LUNATIC_CHECK(transaction);
    LUNATIC_CHECK(harness.impl.availableCredits.empty());
    LUNATIC_CHECK(harness.impl.retries.empty());
    LUNATIC_CHECK(harness.impl.retrySources.empty());
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 0);
    harness.Retire(transaction->handle);
    LUNATIC_CHECK(!harness.impl.HasInflight());
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesRetryReservationMaximumAndRecovery)
{
    Options options = RetryClassOptions();
    options.structure.classes.maximum[3] = 1;
    ClassHarness harness(options);
    harness.forceRetry = true;
    harness.Retry(ClassRequest(4, 190, ClassLine(0, 0), 0));
    harness.Retry(ClassRequest(5, 191, ClassLine(0, 1), 0));
    LUNATIC_CHECK(harness.impl.retries.size() == 2);
    LUNATIC_CHECK(harness.impl.rxreq.empty());
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 0);
    harness.Check();

    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.PopRSP());
    harness.impl.QueueRetryCredit(0);
    harness.impl.QueueRetryCredit(1);
    LUNATIC_CHECK(harness.impl.txrsp.size() == 1);
    LUNATIC_CHECK(harness.impl.retries.at(0).creditReserved);
    LUNATIC_CHECK(!harness.impl.retries.at(1).creditReserved);
    LUNATIC_CHECK(harness.impl.banks[0].retryReservations[3] == 1);
    const auto firstGrant = harness.PopRSP();
    LUNATIC_CHECK(firstGrant && firstGrant->opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
    harness.Push(Stage10PCrdReturn(4, 2, firstGrant->pCrdType, 0, 0));
    harness.Admit();
    LUNATIC_CHECK(harness.impl.retries.size() == 1);
    LUNATIC_CHECK(harness.impl.RetrySourceCount() == 1);
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 0);

    harness.impl.QueueRetryCredit(1);
    const auto secondGrant = harness.PopRSP();
    LUNATIC_CHECK(secondGrant && secondGrant->opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
    harness.Push(Stage10PCrdReturn(5, 2, secondGrant->pCrdType, 0, 0));
    harness.Admit();
    LUNATIC_CHECK(!harness.impl.HasInflight());
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesSourceSharingAcrossClasses)
{
    Options options = RetryClassOptions();
    options.structure.retrySourceLimit = 1;
    ClassHarness harness(options);
    harness.forceRetry = true;
    harness.Retry(ClassRequest(4, 200, ClassLine(0, 0), 0));
    harness.Retry(ClassRequest(4, 201, ClassLine(0, 1), 8));
    LUNATIC_CHECK(harness.impl.RetrySourceCount() == 1);
    LUNATIC_CHECK(!harness.impl.RetrySourceAvailable(5));
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 0);
    LUNATIC_CHECK(harness.impl.banks[0].retryReservations[3] == 0);
    LUNATIC_CHECK(harness.impl.banks[0].retryReservations[2] == 0);
    const auto firstGrant = harness.Grant(0);
    const auto secondGrant = harness.Grant(1);
    LUNATIC_CHECK(firstGrant && secondGrant);
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 2);
    LUNATIC_CHECK(harness.impl.banks[0].retryReservations[3] == 1);
    LUNATIC_CHECK(harness.impl.banks[0].retryReservations[2] == 1);

    harness.Resend(ClassRequest(4, 200, ClassLine(0, 0), 0, firstGrant->pCrdType));
    LUNATIC_CHECK(harness.impl.RetrySourceCount() == 1);
    harness.Resend(ClassRequest(4, 201, ClassLine(0, 1), 8, secondGrant->pCrdType));
    LUNATIC_CHECK(harness.impl.retrySources.empty());
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 0);
    LUNATIC_CHECK(harness.impl.RetrySourceAvailable(5));
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesDistinctSourceExhaustionAndRecovery)
{
    Options options = RetryClassOptions();
    options.structure.retrySourceLimit = 1;
    ClassHarness harness(options);
    harness.forceRetry = true;
    harness.Retry(ClassRequest(4, 300, ClassLine(0, 0), 0));
    harness.Retry(ClassRequest(5, 301, ClassLine(0, 1), 8));
    LUNATIC_CHECK(harness.impl.RetrySourceCount() == 1);
    LUNATIC_CHECK(harness.impl.retries.size() == 1);
    LUNATIC_CHECK(harness.impl.rxreq.size() == 1);
    const auto firstGrant = harness.Grant(0);
    LUNATIC_CHECK(firstGrant);
    auto firstResend = ClassRequest(4, 300, ClassLine(0, 0), 0, firstGrant->pCrdType);
    firstResend.allowRetry = false;
    harness.impl.rxreqTimes.push_front(harness.impl.time);
    harness.impl.rxreq.push_front(firstResend);
    harness.Admit();
    LUNATIC_CHECK(harness.impl.retrySources.empty());
    harness.Admit();
    LUNATIC_CHECK(harness.impl.RetrySourceCount() == 1);
    const auto secondGrant = harness.Grant(1);
    LUNATIC_CHECK(secondGrant);
    harness.Resend(ClassRequest(5, 301, ClassLine(0, 1), 8, secondGrant->pCrdType));
    LUNATIC_CHECK(harness.impl.retrySources.empty());
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesResendAndCreditReturnCleanup)
{
    Options options = RetryClassOptions();
    ClassHarness harness(options);
    harness.forceRetry = true;
    harness.Retry(ClassRequest(4, 400, ClassLine(0, 0), 0));
    harness.Retry(ClassRequest(4, 401, ClassLine(0, 1), 0));
    const auto firstGrant = harness.Grant(0);
    const auto secondGrant = harness.Grant(1);
    LUNATIC_CHECK(firstGrant && secondGrant);
    harness.Resend(ClassRequest(4, 400, ClassLine(0, 0), 0, firstGrant->pCrdType));
    LUNATIC_CHECK(harness.impl.RetrySourceCount() == 1);
    LUNATIC_CHECK(harness.impl.availableCredits.size() == 1);
    const auto creditReturn = Stage10PCrdReturn(4, 2, secondGrant->pCrdType, 0, 0);
    harness.Push(creditReturn);
    harness.Admit();
    LUNATIC_CHECK(harness.impl.retrySources.empty());
    LUNATIC_CHECK(harness.impl.availableCredits.empty());
    LUNATIC_CHECK(harness.impl.cancellationCredits.empty());
    LUNATIC_CHECK(harness.impl.retries.empty());
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 0);
    Transaction* transaction = harness.impl.tracker.FindRequest(4, 400);
    LUNATIC_CHECK(transaction);
    harness.Retire(transaction->handle);
    LUNATIC_CHECK(!harness.impl.HasInflight());
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesResetClearsEveryLedger)
{
    Options options = RetryClassOptions();
    ClassHarness harness(options);
    harness.forceRetry = true;
    harness.Retry(ClassRequest(4, 500, ClassLine(0, 0), 0));
    harness.Retry(ClassRequest(4, 501, ClassLine(0, 1), 0));
    const auto firstGrant = harness.Grant(0);
    const auto secondGrant = harness.Grant(1);
    LUNATIC_CHECK(firstGrant && secondGrant);
    const auto creditReturn = Stage10PCrdReturn(4, 2, firstGrant->pCrdType, 0, 0);
    harness.Push(creditReturn);
    harness.Admit();
    LUNATIC_CHECK(!harness.impl.cancellationCredits.empty());
    LUNATIC_CHECK(!harness.impl.availableCredits.empty());
    LUNATIC_CHECK(!harness.impl.retrySources.empty());
    harness.Check();
    harness.impl.Reset();
    LUNATIC_CHECK(harness.impl.retrySources.empty());
    LUNATIC_CHECK(harness.impl.availableCredits.empty());
    LUNATIC_CHECK(harness.impl.cancellationCredits.empty());
    LUNATIC_CHECK(harness.impl.retries.empty());
    LUNATIC_CHECK(harness.impl.TotalRetryReservations() == 0);
    LUNATIC_CHECK(!harness.impl.HasInflight());
    harness.Check();
}

LUNATIC_TEST(Stage10RetryClassesType3LegacyAndType7MetadataSmoke)
{
    Options legacy = MakeType3Options();
    legacy.backend = BackendKind::Internal;
    legacy.poisonEnabled = false;
    legacy.dataCheckEnabled = false;
    Driver<Config256> driver(legacy);
    LUNATIC_CHECK(driver.ReadData(4, 600, 0xc0000, CHI::Eb::Opcodes::REQ::ReadNoSnp) == LineData{});
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());

    Options options = MakeType7Options();
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dmtEnabled = false;
    for (size_t& maximum : options.structure.classes.maximum)
        maximum = 15;
    Driver<MetadataConfig256> metadata(options);
    auto request = metadata.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 700, 0xc0040, false);
    metadata.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("metadata-force-retry", 0,
            [&](RequestAdmissionEvent& event) {
                if (event.request.allowRetry)
                    event.RequestRetry();
            }));
    LUNATIC_CHECK(metadata.node.PushRXREQ(request));
    const auto retry = metadata.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(retry.PCrdType() == 3);
    const auto grant = metadata.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    LUNATIC_CHECK(grant.PCrdType() == 3);
    auto resend = request;
    resend.AllowRetry() = 0;
    resend.PCrdType() = grant.PCrdType();
    LUNATIC_CHECK(metadata.node.PushRXREQ(resend));
    const auto downstream = metadata.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    SendBackendReadData(metadata, downstream.TxnID(), ClassLine(0, 0));
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(metadata.NextDAT(CHI::Eb::Opcodes::DAT::CompData).TxnID() == 700);
    metadata.Quiesce();
    LUNATIC_CHECK(metadata.node.GetErrors().empty());
    LUNATIC_CHECK(metadata.node.ValidateInvariants());
}
