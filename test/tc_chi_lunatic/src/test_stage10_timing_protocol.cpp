#include "timing_matrix.hpp"

#include <array>
#include <limits>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;
using namespace LunaticTest::Timing;

namespace {

    const std::array<ProfileKind, 2> Profiles = { ProfileKind::Type3, ProfileKind::Type7 };

    Request RetryRequest(NodeId source, TransactionId txnId, uint64_t address,
                         uint8_t qos = 0, uint8_t pCrdType = 0)
    {
        Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, source, txnId, address);
        request.allowRetry = true;
        request.qos = qos;
        request.pCrdType = pCrdType;
        return request;
    }

    uint8_t CreditType(Core::Impl& impl)
    {
        return impl.options.structure.classes.enabled ? impl.ClassForQos(0) : 0;
    }

    void SeedRetry(Core::Impl& impl, size_t index, const Request& request,
                   size_t classId, Time age, bool ackPopped = true)
    {
        RetryRecord record;
        record.request = request;
        record.bank = impl.BankIndex(request.Line());
        record.classId = impl.options.structure.classes.enabled
            ? impl.ClassForQos(request.qos) : classId;
        record.age = age;
        record.ackPopped = ackPopped;
        record.request.pCrdType = impl.options.structure.classes.enabled
            ? record.classId : request.pCrdType;
        impl.retries.emplace(index, record);
        impl.AcquireRetrySource(record.request);
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

    struct OutputKey {
        uint16_t opcode;
        NodeId   target;

        bool operator==(const OutputKey&) const = default;
    };

}

LUNATIC_TEST(Stage10MatrixP01ForcedRequesterRetryAbsoluteSchedule)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P01" };
        Options options = ProfileOptions(profile);
        options.timing.retryAck = 3;
        options.timing.creditGrant = 4;
        options.timing.output = 2;
        if (profile == ProfileKind::Type3)
        {
            CoreDriver disabled(options);
            Core::Impl& rejected = disabled.impl;
            rejected.events->OnRequestAdmission.Register(
                Gravity::MakeListener<RequestAdmissionEvent>("matrix-p01-disabled", 0,
                    [&](RequestAdmissionEvent& event) {
                        if (event.request.allowRetry)
                            event.RequestRetry();
                    }));
            Request request = RetryRequest(4, 20, 0x100000);
            rejected.time = 10;
            TIMING_CHECK(row, rejected.Push(rejected.rxreq, request, EventKind::AcceptedRXREQ));
            rejected.Tick();
            TIMING_CHECK(row, rejected.tracker.Size() == 1);
            TIMING_CHECK(row, rejected.rxreq.empty());
            TIMING_CHECK(row, rejected.retries.empty());
            TIMING_CHECK(row, rejected.txrsp.empty());
            TIMING_CHECK(row, rejected.errors.size() == 1);
            rejected.Reset();
        }
        options.retryEnabled = true;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.events->OnRequestAdmission.Register(
            Gravity::MakeListener<RequestAdmissionEvent>("matrix-p01-force", 0,
                [&](RequestAdmissionEvent& event) {
                    if (event.request.allowRetry)
                        event.RequestRetry();
                }));

        Request request = RetryRequest(4, 20, 0x100000, profile == ProfileKind::Type7 ? 12 : 0,
            profile == ProfileKind::Type7 ? 1 : 0);
        impl.time = 10;
        TIMING_CHECK(row, impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
        impl.Tick();
        TIMING_CHECK(row, impl.tracker.Empty());
        TIMING_CHECK(row, impl.rxreq.empty());
        TIMING_CHECK(row, impl.retries.size() == 1);
        TIMING_CHECK(row, impl.RetrySourceCount() == 1);
        TIMING_CHECK(row, impl.txrsp.size() == 1);
        TIMING_CHECK(row, impl.txrsp.front().flit.opcode == CHI::Eb::Opcodes::RSP::RetryAck);
        ExpectTime(row, impl.txrsp.front().readyTime, 15, "forced RetryAck visibility");

        driver.TickTo(14);
        TIMING_CHECK(row, !impl.Peek(impl.txrsp));
        TIMING_CHECK(row, !impl.retries.at(0).ackPopped);
        driver.TickTo(15);
        const auto ack = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, ack && ack->opcode == CHI::Eb::Opcodes::RSP::RetryAck);
        TIMING_CHECK(row, impl.retries.at(0).ackPopped);
        TIMING_CHECK(row, impl.availableCredits.empty());
        TIMING_CHECK(row, impl.txrsp.empty());

        driver.TickTo(16);
        TIMING_CHECK(row, impl.txrsp.size() == 1);
        TIMING_CHECK(row, impl.txrsp.front().flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
        ExpectTime(row, impl.txrsp.front().readyTime, 22, "later credit grant visibility");
        TIMING_CHECK(row, impl.availableCredits.empty());
        TIMING_CHECK(row, impl.ValidateInvariants());
        impl.Reset();
    }
}

LUNATIC_TEST(Stage10MatrixP02ResourceRetryAndAdmissionHold)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P02" };
        for (size_t scenario = 0; scenario < 2; ++scenario)
        {
            Options options = ProfileOptions(profile);
            options.structure.bankCount = 1;
            options.transactionLimit = scenario == 0
                ? (profile == ProfileKind::Type3 ? 1 : 2) : 4;
            options.timing.retryAck = 2;
            options.timing.output = 1;
            if (profile == ProfileKind::Type3)
                options.structure.requestBufferLimit = 1;
            else
                options.structure.classes.maximum.fill(1);

            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request original = RetryRequest(4, 1, 0x100000, 8, 2);
            original.allowRetry = false;
            Transaction& allocated = driver.Add(original);
            Request waiting = RetryRequest(5, 2, 0x100040, 8, 2);
            impl.time = 5;
            TIMING_CHECK(row, impl.Push(impl.rxreq, waiting, EventKind::AcceptedRXREQ));
            impl.Tick();
            TIMING_CHECK(row, impl.tracker.Size() == 1);
            TIMING_CHECK(row, impl.tracker.FindRequest(4, 1) == &allocated);
            TIMING_CHECK(row, impl.tracker.FindRequest(5, 2) == nullptr);

            if (profile == ProfileKind::Type3)
            {
                TIMING_CHECK(row, impl.rxreq.size() == 1);
                TIMING_CHECK(row, impl.retries.empty());
                TIMING_CHECK(row, impl.txrsp.empty());
                TIMING_CHECK(row, impl.errors.empty());
            }
            else
            {
                TIMING_CHECK(row, impl.rxreq.empty());
                TIMING_CHECK(row, impl.retries.size() == 1);
                TIMING_CHECK(row, impl.retries.at(0).classId == 2);
                TIMING_CHECK(row, impl.txrsp.size() == 1);
                TIMING_CHECK(row, impl.txrsp.front().flit.opcode == CHI::Eb::Opcodes::RSP::RetryAck);
                TIMING_CHECK(row, impl.txrsp.front().flit.pCrdType == 2);
                ExpectTime(row, impl.txrsp.front().readyTime, 8, "resource RetryAck visibility");
                driver.TickTo(8);
                const auto ack = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
                TIMING_CHECK(row, ack && ack->pCrdType == 2);
            }
            TIMING_CHECK(row, impl.ValidateInvariants());
            impl.Reset();
        }
    }
}

LUNATIC_TEST(Stage10MatrixP03CreditGrantCapacityAndSingleReservation)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P03" };
        Options options = ProfileOptions(profile);
        options.structure.bankCount = 1;
        options.transactionLimit = profile == ProfileKind::Type3 ? 1 : 2;
        options.timing.creditGrant = 4;
        options.timing.output = 2;
        if (profile == ProfileKind::Type7)
            options.structure.classes.maximum.fill(1);
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Request occupying = RetryRequest(4, 1, 0x100000, 15, 0);
        occupying.allowRetry = false;
        Transaction& original = driver.Add(occupying);
        SeedRetry(impl, 0, RetryRequest(5, 2, 0x100040, 15, 0), 0, 3);

        impl.time = 10;
        impl.QueueRetryCredits();
        if (profile == ProfileKind::Type7)
        {
            TIMING_CHECK(row, impl.txrsp.empty());
            TIMING_CHECK(row, !impl.retries.at(0).creditReserved);
            impl.Retire(original.handle);
            impl.QueueRetryCredits();
        }
        else
        {
            TIMING_CHECK(row, impl.txrsp.size() == 1);
            TIMING_CHECK(row, impl.retries.at(0).grantQueued);
            TIMING_CHECK(row, impl.retries.at(0).creditReserved);
            impl.Retire(original.handle);
            impl.QueueRetryCredits();
        }
        TIMING_CHECK(row, impl.txrsp.size() == 1);
        TIMING_CHECK(row, impl.retries.at(0).grantQueued);
        TIMING_CHECK(row, impl.retries.at(0).creditReserved);
        TIMING_CHECK(row, impl.TotalRetryReservations() == (profile == ProfileKind::Type7 ? 1 : 0));
        ExpectTime(row, impl.txrsp.front().readyTime, 16, "credit grant visibility");

        driver.TickTo(15);
        TIMING_CHECK(row, !impl.Peek(impl.txrsp));
        driver.TickTo(16);
        TIMING_CHECK(row, impl.Peek(impl.txrsp));
        driver.TickTo(19);
        TIMING_CHECK(row, impl.txrsp.size() == 1);
        ExpectTime(row, impl.txrsp.front().readyTime, 16, "held grant readiness");
        const auto grant = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, grant && grant->opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
        TIMING_CHECK(row, impl.availableCredits.at({ 5, 0 }) == 1);
        TIMING_CHECK(row, !impl.retries.at(0).grantQueued);
        TIMING_CHECK(row, impl.TotalRetryReservations() == (profile == ProfileKind::Type7 ? 1 : 0));

        TIMING_CHECK(row, impl.Push(impl.rxreq, Stage10PCrdReturn(5, 2, 0, 0, 0),
            EventKind::AcceptedRXREQ));
        impl.AdmitRequests();
        TIMING_CHECK(row, impl.retries.empty());
        TIMING_CHECK(row, impl.retrySources.empty());
        TIMING_CHECK(row, impl.availableCredits.empty());
        TIMING_CHECK(row, impl.cancellationCredits.empty());
        TIMING_CHECK(row, impl.txrsp.empty());
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP04CompatibleCreditedResendAbsoluteAdmission)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P04" };
        Options options = ProfileOptions(profile);
        options.timing.inputRegister = 2;
        options.timing.requestBuffer = 3;
        options.timing.allocation = 4;
        options.timing.creditGrant = 2;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const uint8_t qos = profile == ProfileKind::Type7 ? 12 : 0;
        const size_t classId = profile == ProfileKind::Type7 ? 1 : 0;
        SeedRetry(impl, 0, RetryRequest(4, 20, 0x100000, qos, classId), classId, 2);

        impl.time = 4;
        TIMING_CHECK(row, impl.QueueRetryCredit(0));
        ExpectTime(row, impl.txrsp.front().readyTime, 7, "resend donor grant visibility");
        driver.TickTo(6);
        TIMING_CHECK(row, !impl.Peek(impl.txrsp));
        driver.TickTo(7);
        TIMING_CHECK(row, impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
        TIMING_CHECK(row, impl.availableCredits.at({ 4, uint8_t(classId) }) == 1);

        Request resend = impl.retries.at(0).request;
        resend.txnId = 99;
        resend.allowRetry = false;
        impl.time = 10;
        TIMING_CHECK(row, impl.Push(impl.rxreq, resend, EventKind::AcceptedRXREQ));
        impl.Tick();
        driver.TickTo(11);
        TIMING_CHECK(row, impl.tracker.Empty());
        driver.TickTo(12);
        Transaction* transaction = impl.tracker.FindRequest(4, 99);
        TIMING_CHECK(row, transaction != nullptr);
        TIMING_CHECK(row, impl.tracker.FindRequest(4, 20) == nullptr);
        TIMING_CHECK(row, impl.tracker.Size() == 1);
        TIMING_CHECK(row, impl.retries.empty());
        TIMING_CHECK(row, impl.retrySources.empty());
        TIMING_CHECK(row, impl.availableCredits.empty());
        TIMING_CHECK(row, impl.TotalRetryReservations() == 0);
        TIMING_CHECK(row, impl.pmu.creditedResends == 1);
        TIMING_CHECK(row, impl.flows.at(transaction->handle).assignedClass == classId);
        ExpectTime(row, impl.flows.at(transaction->handle).scheduleMarks.at(SchedulePoint::RequestBuffer),
            16, "credited resend request buffer");
        driver.TickTo(16);
        TIMING_CHECK(row, transaction->phase == Phase::Decode);
        driver.TickTo(18);
        ExpectTime(row, impl.flows.at(transaction->handle).scheduleMarks.at(SchedulePoint::Allocation),
            22, "credited resend allocation");
        driver.TickTo(22);
        TIMING_CHECK(row, transaction->phase == Phase::Lookup);
        TIMING_CHECK(row, impl.ValidateInvariants());
        impl.Retire(transaction->handle);
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP05CreditReturnAndCancellationAllowance)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P05" };
        Options options = ProfileOptions(profile);
        options.timing.creditGrant = 2;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        SeedRetry(impl, 0, RetryRequest(4, 10, 0x100000), 0, 1);
        SeedRetry(impl, 1, RetryRequest(4, 11, 0x100040), 0, 2);
        impl.time = 5;
        impl.QueueRetryCredits();
        impl.QueueRetryCredits();
        TIMING_CHECK(row, impl.txrsp.size() == 2);
        for (const Pending<Response>& pending : impl.txrsp)
            ExpectTime(row, pending.readyTime, 8, "returnable credit visibility");
        driver.TickTo(8);
        TIMING_CHECK(row, impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
        driver.TickTo(9);
        TIMING_CHECK(row, impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
        TIMING_CHECK(row, impl.availableCredits.at({ 4, CreditType(impl) }) == 2);

        impl.time = 10;
        impl.QueueRetryCredits();
        TIMING_CHECK(row, impl.txrsp.empty());
        impl.time = 11;
        TIMING_CHECK(row, impl.Push(impl.rxreq, Stage10PCrdReturn(4, 2, CreditType(impl), 0, 0),
            EventKind::AcceptedRXREQ));
        impl.AdmitRequests();
        TIMING_CHECK(row, impl.availableCredits.at({ 4, CreditType(impl) }) == 1);
        TIMING_CHECK(row, impl.retries.size() == 2);
        TIMING_CHECK(row, impl.retries.contains(0));
        TIMING_CHECK(row, impl.retries.contains(1));
        TIMING_CHECK(row, impl.cancellationCredits.at({ 4, CreditType(impl) }) == 1);
        TIMING_CHECK(row, !impl.RetryCreditEligible(impl.retries.at(1)));
        TIMING_CHECK(row, impl.TotalRetryReservations() == (profile == ProfileKind::Type7 ? 1 : 0));
        TIMING_CHECK(row, impl.txrsp.empty());
        TIMING_CHECK(row, impl.ValidateInvariants());

        Request resend = impl.retries.at(0).request;
        resend.txnId = 100;
        resend.allowRetry = false;
        impl.time = 12;
        TIMING_CHECK(row, impl.Push(impl.rxreq, resend, EventKind::AcceptedRXREQ));
        impl.AdmitRequests();
        Transaction* transaction = impl.tracker.FindRequest(4, 100);
        TIMING_CHECK(row, transaction != nullptr);
        TIMING_CHECK(row, impl.retries.empty());
        TIMING_CHECK(row, impl.retrySources.empty());
        TIMING_CHECK(row, impl.availableCredits.empty());
        TIMING_CHECK(row, impl.cancellationCredits.empty());
        TIMING_CHECK(row, impl.TotalRetryReservations() == 0);
        TIMING_CHECK(row, impl.pmu.returnedCredits == 1);
        TIMING_CHECK(row, impl.pmu.creditedResends == 1);
        impl.Retire(transaction->handle);
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP06OldestNewestOriginalDonorAndDownstreamSelection)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P06" };
        for (const RetrySelectionKind selection : { RetrySelectionKind::Oldest, RetrySelectionKind::Newest })
        {
            Options options = ProfileOptions(profile);
            options.strategy.upstreamRetrySelection = selection;
            options.timing.creditGrant = 1;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            SeedRetry(impl, 0, RetryRequest(4, 10, 0x100000), 0, 1);
            SeedRetry(impl, 1, RetryRequest(4, 11, 0x100000), 0, 2);
            SeedRetry(impl, 2, RetryRequest(4, 12, 0x100000), 0, 3);
            const size_t originalIndex = selection == RetrySelectionKind::Oldest ? 0 : 2;
            const size_t donorIndex = 1;
            const size_t remainingIndex = selection == RetrySelectionKind::Oldest ? 2 : 0;
            impl.time = 4;
            for (const size_t index : { size_t(0), size_t(1), size_t(2) })
                if (index != originalIndex)
                    TIMING_CHECK(row, impl.QueueRetryCredit(index));
            TIMING_CHECK(row, impl.txrsp.size() == 2);
            impl.time = 6;
            TIMING_CHECK(row, impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
            impl.time = 7;
            TIMING_CHECK(row, impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
            TIMING_CHECK(row, impl.availableCredits.at({ 4, CreditType(impl) }) == 2);

            Request resend = impl.retries.at(originalIndex).request;
            resend.txnId = 20;
            resend.allowRetry = false;
            impl.time = 8;
            TIMING_CHECK(row, impl.Push(impl.rxreq, resend, EventKind::AcceptedRXREQ));
            impl.AdmitRequests();
            Transaction* transaction = impl.tracker.FindRequest(4, 20);
            TIMING_CHECK(row, transaction != nullptr);
            TIMING_CHECK(row, !impl.retries.contains(originalIndex));
            TIMING_CHECK(row, impl.retries.contains(donorIndex));
            TIMING_CHECK(row, impl.retries.contains(remainingIndex));
            TIMING_CHECK(row, !impl.retries.at(donorIndex).creditReserved);
            TIMING_CHECK(row, impl.retries.at(remainingIndex).creditReserved);
            TIMING_CHECK(row, impl.availableCredits.at({ 4, CreditType(impl) }) == 1);
            TIMING_CHECK(row, impl.retrySources.begin()->second == 2);
            TIMING_CHECK(row, impl.TotalRetryReservations() == (profile == ProfileKind::Type7 ? 1 : 0));
            TIMING_CHECK(row, impl.pmu.creditedResends == 1);
            TIMING_CHECK(row, impl.ValidateInvariants());
            impl.Retire(transaction->handle);
            TIMING_CHECK(row, impl.ValidateInvariants());
            impl.Reset();
        }

        for (const RetrySelectionKind selection : { RetrySelectionKind::Oldest, RetrySelectionKind::Newest })
        {
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::CHI;
            options.subordinateId = 6;
            options.strategy.downstreamRetrySelection = selection;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 3;
            Transaction& older = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp,
                4, 30, 0x100000));
            Transaction& newer = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp,
                5, 31, 0x100040));
            TIMING_CHECK(row, impl.tracker.BindDBID(older.handle, older.handle));
            TIMING_CHECK(row, impl.tracker.BindDBID(newer.handle, newer.handle));
            impl.StartMemoryRead(older);
            impl.StartMemoryRead(newer);
            TIMING_CHECK(row, impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
            impl.time = 4;
            TIMING_CHECK(row, impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
            TIMING_CHECK(row, impl.HandleBackendRetry(older,
                BackendResponse(older, CHI::Eb::Opcodes::RSP::RetryAck)));
            TIMING_CHECK(row, impl.HandleBackendRetry(newer,
                BackendResponse(newer, CHI::Eb::Opcodes::RSP::RetryAck)));
            impl.backendCredits[{ 6, 0 }] = 1;
            impl.time = 5;
            impl.AdvanceBackendRetries();
            const TransactionHandle expected = selection == RetrySelectionKind::Oldest
                ? older.handle : newer.handle;
            const TransactionHandle waiting = selection == RetrySelectionKind::Oldest
                ? newer.handle : older.handle;
            TIMING_CHECK(row, impl.txreq.size() == 1);
            TIMING_CHECK(row, impl.txreq.front().transaction == expected);
            TIMING_CHECK(row, impl.backendRetries.contains(waiting));
            TIMING_CHECK(row, !impl.backendRetries.at(waiting).creditReceived);
            TIMING_CHECK(row, !impl.backendRetries.contains(expected));
            TIMING_CHECK(row, impl.tracker.Size() == 2);
            TIMING_CHECK(row, impl.ValidateInvariants());
            impl.Reset();
        }
    }
}

LUNATIC_TEST(Stage10MatrixP07ClassTiersAndWeightedCreditGrant)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P07" };
        Options options = ProfileOptions(profile);
        options.structure.bankCount = 1;
        options.structure.classes.enabled = true;
        options.structure.classes.maximum.fill(8);
        options.structure.classes.dedicated = { 1, 0, 0, 0 };
        options.structure.classes.contendedMinimum = { 2, 0, 0, 0 };
        options.structure.classes.retryWeights = { 2, 0, 1, 1 };
        options.timing.creditGrant = 2;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        SeedRetry(impl, 0, RetryRequest(4, 10, 0x100000, 8, 2), 2, 1);
        for (size_t index = 0; index < 4; ++index)
            SeedRetry(impl, 1 + index, RetryRequest(4, 11 + index,
                0x100040 + index * LINE_SIZE, 15, 0), 0, 5);
        for (size_t index = 0; index < 2; ++index)
            SeedRetry(impl, 5 + index, RetryRequest(4, 15 + index,
                0x100140 + index * LINE_SIZE, 12, 1), 1, 5);

        impl.time = 10;
        TIMING_CHECK(row, impl.RetryTier(0, 0) == 0);
        for (size_t count = 0; count < 7; ++count)
        {
            impl.QueueRetryCredits();
            if (count == 1)
                TIMING_CHECK(row, impl.RetryTier(0, 0) == 1);
            if (count == 2)
                TIMING_CHECK(row, impl.RetryTier(0, 0) == 2);
            if (count == 3)
                TIMING_CHECK(row, impl.RetryTier(0, 0) == 2);
        }
        TIMING_CHECK(row, impl.RetryTier(0, 0) == 2);
        const std::vector<uint8_t> expectedClasses = { 2, 0, 0, 0, 0, 1, 1 };
        std::vector<uint8_t> actualClasses;
        for (const Pending<Response>& pending : impl.txrsp)
        {
            TIMING_CHECK(row, pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
            actualClasses.push_back(pending.flit.pCrdType);
            ExpectTime(row, pending.readyTime, 13, "weighted grant visibility");
        }
        TIMING_CHECK(row, actualClasses == expectedClasses);
        TIMING_CHECK(row, impl.TotalRetryReservations() == 7);
        TIMING_CHECK(row, impl.ValidateInvariants());
        impl.Reset();

        Options limitedOptions = ProfileOptions(profile);
        limitedOptions.structure.bankCount = 1;
        limitedOptions.structure.classes.enabled = true;
        limitedOptions.structure.classes.maximum.fill(8);
        limitedOptions.structure.classes.maximum[3] = 1;
        limitedOptions.timing.creditGrant = 2;
        limitedOptions.timing.output = 1;
        CoreDriver limited(limitedOptions);
        Core::Impl& full = limited.impl;
        Request occupying = RetryRequest(7, 30, 0x100000, 0, 3);
        occupying.allowRetry = false;
        Transaction& active = limited.Add(occupying);
        SeedRetry(full, 0, RetryRequest(4, 31, 0x100040, 0, 3), 3, 1);
        full.time = 20;
        full.QueueRetryCredits();
        TIMING_CHECK(row, full.retries.size() == 1);
        TIMING_CHECK(row, full.txrsp.empty());
        TIMING_CHECK(row, !full.retries.at(0).creditReserved);
        full.Retire(active.handle);
        full.QueueRetryCredits();
        TIMING_CHECK(row, full.txrsp.size() == 1);
        TIMING_CHECK(row, full.txrsp.front().flit.pCrdType == 3);
        ExpectTime(row, full.txrsp.front().readyTime, 23, "post-maximum grant visibility");
        TIMING_CHECK(row, full.ValidateInvariants());
        full.Reset();
    }
}

LUNATIC_TEST(Stage10MatrixP08DownstreamRetryGrantOrdering)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P08" };
        for (const bool grantFirst : { false, true })
        {
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::CHI;
            options.subordinateId = 6;
            options.timing.backendRequest = 2;
            options.timing.backendResend = 4;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 10;
            Transaction& transaction = driver.Add(Timing::MakeRequest(
                CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 40));
            impl.StartMemoryRead(transaction);
            TIMING_CHECK(row, impl.txreq.size() == 1);
            ExpectTime(row, impl.txreq.front().readyTime, 13, "initial downstream request");
            driver.TickTo(12);
            TIMING_CHECK(row, !impl.Peek(impl.txreq));
            driver.TickTo(13);
            TIMING_CHECK(row, impl.Pop(impl.txreq, EventKind::EmittedTXREQ));

            const Response retry = BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck);
            Response grant = BackendResponse(transaction, CHI::Eb::Opcodes::RSP::PCrdGrant);
            grant.txnId = 0;
            const Response first = grantFirst ? grant : retry;
            const Response second = grantFirst ? retry : grant;
            driver.TickTo(14);
            TIMING_CHECK(row, impl.Push(impl.rxrsp, first, EventKind::AcceptedRXRSP));
            impl.Tick();
            TIMING_CHECK(row, impl.txreq.empty());
            TIMING_CHECK(row, impl.backendRetries.size() == (grantFirst ? 0 : 1));
            driver.TickTo(15);
            TIMING_CHECK(row, impl.Push(impl.rxrsp, second, EventKind::AcceptedRXRSP));
            impl.Tick();
            TIMING_CHECK(row, impl.txreq.size() == 1);
            TIMING_CHECK(row, impl.backendRetries.empty());
            TIMING_CHECK(row, impl.txreq.front().schedule == SchedulePoint::BackendResend);
            TIMING_CHECK(row, !impl.txreq.front().flit.allowRetry);
            TIMING_CHECK(row, impl.txreq.front().flit.txnId == transaction.handle);
            ExpectTime(row, impl.txreq.front().readyTime, 20, "ordered downstream resend");
            TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
            TIMING_CHECK(row, impl.tracker.Size() == 1);
            TIMING_CHECK(row, impl.tracker.FindMemory(6, transaction.handle) == &transaction);
            TIMING_CHECK(row, impl.txrsp.empty());
            TIMING_CHECK(row, impl.txdat.empty());
            driver.TickTo(19);
            TIMING_CHECK(row, !impl.Peek(impl.txreq));
            driver.TickTo(20);
            TIMING_CHECK(row, impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
            TIMING_CHECK(row, impl.flows.at(transaction.handle).backendPhase == BackendPhase::WaitComp);
            TIMING_CHECK(row, impl.ValidateInvariants());
            impl.Reset();
        }
    }
}

LUNATIC_TEST(Stage10MatrixP09DownstreamResendOutputHold)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P09" };
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.timing.backendRequest = 1;
        options.timing.backendResend = 5;
        options.timing.output = 2;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.time = 6;
        Transaction& transaction = driver.Add(Timing::MakeRequest(
            CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 50));
        impl.StartMemoryRead(transaction);
        ExpectTime(row, impl.txreq.front().readyTime, 9, "initial request before retry");
        driver.TickTo(9);
        TIMING_CHECK(row, impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
        TIMING_CHECK(row, impl.HandleBackendRetry(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck)));
        impl.backendCredits[{ 6, 0 }] = 1;
        impl.time = 10;
        impl.AdvanceBackendRetries();
        TIMING_CHECK(row, impl.txreq.size() == 1);
        TIMING_CHECK(row, impl.txreq.front().schedule == SchedulePoint::BackendResend);
        ExpectTime(row, impl.txreq.front().readyTime, 17, "resend visibility");
        TIMING_CHECK(row, impl.backendRetries.empty());
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
        TIMING_CHECK(row, impl.tracker.FindMemory(6, transaction.handle) == &transaction);

        driver.TickTo(16);
        TIMING_CHECK(row, !impl.Peek(impl.txreq));
        driver.TickTo(17);
        const Request* visible = impl.Peek(impl.txreq);
        TIMING_CHECK(row, visible && !visible->allowRetry);
        for (Time held = 18; held <= 20; ++held)
        {
            driver.TickTo(held);
            TIMING_CHECK(row, impl.Peek(impl.txreq));
            TIMING_CHECK(row, impl.txreq.size() == 1);
            ExpectTime(row, impl.txreq.front().readyTime, 17, "held resend readiness");
            TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
        }
        const auto resent = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        TIMING_CHECK(row, resent && resent->txnId == transaction.handle);
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendPhase == BackendPhase::WaitComp);
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
        TIMING_CHECK(row, impl.tracker.Size() == 1);
        TIMING_CHECK(row, impl.ValidateInvariants());
        impl.Reset();
    }
}

LUNATIC_TEST(Stage10MatrixP10DownstreamCreditReturnAfterCancellation)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P10" };
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.timing.backendRequest = 1;
        options.timing.backendResend = 3;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.time = 5;
        Transaction& transaction = driver.Add(Timing::MakeRequest(
            CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 60));
        transaction.byteEnables.fill(1);
        impl.flows.at(transaction.handle).expectedBeatMask = 3;
        impl.StartBackendWrite(transaction);
        ExpectTime(row, impl.txreq.front().readyTime, 7, "downstream write request");
        driver.TickTo(7);
        TIMING_CHECK(row, impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendPhase == BackendPhase::WaitDBID);
        FlowState& flow = impl.flows.at(transaction.handle);
        flow.dbidSent = true;
        flow.dbidEmitted = true;
        transaction.phase = Phase::WaitWriteData;
        transaction.readyTime = 8;
        impl.time = 8;
        Data cancel;
        cancel.source = 4;
        cancel.target = 2;
        cancel.txnId = transaction.dbid;
        cancel.opcode = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        TIMING_CHECK(row, impl.HandleWriteDAT(transaction, cancel));
        TIMING_CHECK(row, transaction.canceled);
        TIMING_CHECK(row, transaction.Complete(COMPLETE_WRITE_DATA));
        TIMING_CHECK(row, impl.HandleBackendRetry(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck)));
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);

        impl.backendCredits[{ 6, 0 }] = 1;
        impl.time = 9;
        impl.AdvanceBackendRetries();
        TIMING_CHECK(row, impl.txreq.size() == 1);
        TIMING_CHECK(row, impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::PCrdReturn);
        TIMING_CHECK(row, impl.txreq.front().flit.pCrdType == 0);
        TIMING_CHECK(row, impl.txreq.front().schedule == SchedulePoint::BackendResend);
        ExpectTime(row, impl.txreq.front().readyTime, 13, "downstream credit return");
        TIMING_CHECK(row, impl.backendRetries.empty());
        TIMING_CHECK(row, impl.backendCredits.empty());
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendPhase == BackendPhase::Done);
        TIMING_CHECK(row, impl.tracker.FindMemory(6, transaction.handle) == nullptr);
        impl.time = 12;
        TIMING_CHECK(row, !impl.Peek(impl.txreq));
        impl.time = 13;
        const auto returned = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        TIMING_CHECK(row, returned && returned->opcode == CHI::Eb::Opcodes::REQ::PCrdReturn);
        TIMING_CHECK(row, impl.txreq.empty());
        impl.Retire(transaction.handle);
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP11PrefetchTargetForwardTimeline)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P11" };
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.timing.inputRegister = 1;
        options.timing.requestBuffer = 2;
        options.timing.allocation = 3;
        options.timing.lookupResult = 4;
        options.timing.backendRequest = 5;
        options.timing.output = 6;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::PrefetchTgt, 4, 70);
        impl.time = 10;
        TIMING_CHECK(row, impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
        impl.Tick();
        TIMING_CHECK(row, impl.tracker.Empty());
        driver.TickTo(11);
        Transaction* transaction = impl.tracker.FindRequest(4, 70);
        TIMING_CHECK(row, transaction != nullptr);
        const FlowState& flow = impl.flows.at(transaction->handle);
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::RequestBuffer), 14,
            "prefetch request buffer");
        driver.TickTo(16);
        TIMING_CHECK(row, flow.scheduleMarks.contains(SchedulePoint::Allocation));
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::Allocation), 19,
            "prefetch allocation");
        driver.TickTo(19);
        TIMING_CHECK(row, transaction->phase == Phase::Lookup);
        TIMING_CHECK(row, flow.scheduleMarks.contains(SchedulePoint::LookupResult));
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::LookupResult), 23 + options.tagLatency,
            "prefetch lookup result");
        TIMING_CHECK(row, !flow.requestBufferOwned);
        driver.TickTo(23 + options.tagLatency);
        TIMING_CHECK(row, transaction->phase == Phase::Resolve);
        driver.TickTo(28);
        TIMING_CHECK(row, impl.txreq.size() == 1);
        TIMING_CHECK(row, impl.txdat.empty());
        TIMING_CHECK(row, impl.txsnp.empty());
        TIMING_CHECK(row, impl.txrsp.empty());
        TIMING_CHECK(row, impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::PrefetchTgt);
        TIMING_CHECK(row, impl.txreq.front().flit.target == 6);
        TIMING_CHECK(row, !impl.txreq.front().flit.allowRetry);
        ExpectTime(row, impl.txreq.front().readyTime, 39, "prefetch downstream visibility");
        TIMING_CHECK(row, impl.errors.empty());
        driver.TickTo(38);
        TIMING_CHECK(row, !impl.Peek(impl.txreq));
        driver.TickTo(39);
        const auto downstream = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        TIMING_CHECK(row, downstream && downstream->opcode == CHI::Eb::Opcodes::REQ::PrefetchTgt);
        impl.RetireFinished();
        TIMING_CHECK(row, impl.tracker.Empty());
        TIMING_CHECK(row, impl.txrsp.empty() && impl.txdat.empty());
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP12DecodeErrorsNormalOutputAndRetirement)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P12" };
        for (size_t scenario = 0; scenario < 2; ++scenario)
        {
            Options options = ProfileOptions(profile);
            options.timing.normalResponse = 3;
            options.timing.output = 2;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request request = scenario == 0
                ? Timing::MakeRequest(0xffff, 4, 80)
                : Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 81);
            if (scenario == 1)
                request.size = 7;
            impl.time = 10;
            TIMING_CHECK(row, impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
            impl.Tick();
            driver.TickTo(12);
            TIMING_CHECK(row, impl.errors.size() == 1);
            TIMING_CHECK(row, impl.txreq.empty());
            TIMING_CHECK(row, impl.txdat.empty());
            TIMING_CHECK(row, impl.txsnp.empty());
            TIMING_CHECK(row, impl.txrsp.size() == 1);
            TIMING_CHECK(row, impl.txrsp.front().flit.opcode == CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, impl.txrsp.front().flit.respErr == CHI::Eb::RespErrs::NDERR);
            ExpectTime(row, impl.txrsp.front().readyTime, 17, "decode error visibility");
            driver.TickTo(16);
            TIMING_CHECK(row, !impl.Peek(impl.txrsp));
            TIMING_CHECK(row, impl.txdat.empty());
            driver.TickTo(17);
            const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
            TIMING_CHECK(row, response && response->respErr == CHI::Eb::RespErrs::NDERR);
            TIMING_CHECK(row, impl.txrsp.empty());
            TIMING_CHECK(row, impl.txdat.empty());
            impl.RetireFinished();
            TIMING_CHECK(row, impl.tracker.Empty());
            TIMING_CHECK(row, !impl.HasInflight());
            TIMING_CHECK(row, impl.ValidateInvariants());
        }
    }
}

LUNATIC_TEST(Stage10MatrixP13PackageValidationPrecedesTraffic)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P13" };
        const Options valid = ProfileOptions(profile);
        std::array<Options, 4> invalid = { valid, valid, valid, valid };
        invalid[0].mteEnabled = true;
        invalid[1].dvmEnabled = true;
        invalid[2].structure.bankCount = 3;
        invalid[3].slcWays = 0;
        for (const Options& options : invalid)
        {
            bool rejected = false;
            try
            {
                ValidateOptions(options);
            }
            catch (const std::invalid_argument&)
            {
                rejected = true;
            }
            TIMING_CHECK(row, rejected);
        }

        bool constructionRejected = false;
        try
        {
            Options options = valid;
            options.mteEnabled = true;
            HomeNode<Config256> node(2, options);
            TIMING_CHECK(row, !node.HasInflight());
        }
        catch (const std::invalid_argument&)
        {
            constructionRejected = true;
        }
        TIMING_CHECK(row, constructionRejected);

        Core core(2, valid, std::make_shared<EventHub>());
        TIMING_CHECK(row, !core.HasInflight());
        TIMING_CHECK(row, core.GetOptions().profile == profile);
        TIMING_CHECK(row, core.Drain());
        TIMING_CHECK(row, core.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP14WeightedOutputCommittedArbitration)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P14" };
        Options options = ProfileOptions(profile);
        options.timing.output = 4;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        SeedRetry(impl, 0, RetryRequest(4, 40, 0x100000, 15, 0), 0, 40, false);
        SeedRetry(impl, 1, RetryRequest(5, 41, 0x100040, 15, 0), 0, 40);
        impl.time = 10;

        const auto response = [](NodeId target, uint16_t opcode, uint8_t qos) {
            Response value;
            value.source = 2;
            value.target = target;
            value.txnId = target;
            value.opcode = opcode;
            value.qos = qos;
            return value;
        };
        TIMING_CHECK(row, impl.Queue(impl.txrsp,
            response(20, CHI::Eb::Opcodes::RSP::Comp, 12), INVALID_TRANSACTION, 40));
        Response retry = response(4, CHI::Eb::Opcodes::RSP::RetryAck, 15);
        retry.txnId = 40;
        TIMING_CHECK(row, impl.Queue(impl.txrsp, retry, INVALID_TRANSACTION, 40, false, false, 0));
        TIMING_CHECK(row, impl.Queue(impl.txrsp,
            response(21, CHI::Eb::Opcodes::RSP::DBIDResp, 15), INVALID_TRANSACTION, 40));
        impl.nextAge = 40;
        TIMING_CHECK(row, impl.QueueRetryCredit(1));
        TIMING_CHECK(row, impl.Queue(impl.txrsp,
            response(23, CHI::Eb::Opcodes::RSP::CompDBIDResp, 12), INVALID_TRANSACTION, 40));
        TIMING_CHECK(row, impl.Queue(impl.txrsp,
            response(22, CHI::Eb::Opcodes::RSP::Comp, 15), INVALID_TRANSACTION, 40));
        TIMING_CHECK(row, impl.Queue(impl.txrsp,
            response(24, CHI::Eb::Opcodes::RSP::DBIDRespOrd, 15), INVALID_TRANSACTION, 40));
        TIMING_CHECK(row, impl.txrsp.size() == 7);
        for (const Pending<Response>& pending : impl.txrsp)
        {
            TIMING_CHECK(row, pending.age == 40);
            ExpectTime(row, pending.readyTime, 14, "equal-age output visibility");
        }

        const std::vector<OutputKey> expected = profile == ProfileKind::Type3
            ? std::vector<OutputKey>{
                { CHI::Eb::Opcodes::RSP::Comp, 20 },
                { CHI::Eb::Opcodes::RSP::RetryAck, 4 },
                { CHI::Eb::Opcodes::RSP::DBIDResp, 21 },
                { CHI::Eb::Opcodes::RSP::PCrdGrant, 5 },
                { CHI::Eb::Opcodes::RSP::CompDBIDResp, 23 },
                { CHI::Eb::Opcodes::RSP::Comp, 22 },
                { CHI::Eb::Opcodes::RSP::DBIDRespOrd, 24 } }
            : std::vector<OutputKey>{
                { CHI::Eb::Opcodes::RSP::DBIDResp, 21 },
                { CHI::Eb::Opcodes::RSP::CompDBIDResp, 23 },
                { CHI::Eb::Opcodes::RSP::DBIDRespOrd, 24 },
                { CHI::Eb::Opcodes::RSP::RetryAck, 4 },
                { CHI::Eb::Opcodes::RSP::PCrdGrant, 5 },
                { CHI::Eb::Opcodes::RSP::Comp, 22 },
                { CHI::Eb::Opcodes::RSP::Comp, 20 } };
        impl.time = 14;
        const auto categoryRound = impl.responseCategoryRound;
        const auto classRounds = impl.responseClassRounds;
        const Response* first = impl.Peek(impl.txrsp);
        TIMING_CHECK(row, first && first->opcode == expected.front().opcode &&
            first->target == expected.front().target);
        TIMING_CHECK(row, impl.Peek(impl.txrsp) == first);
        TIMING_CHECK(row, impl.responseCategoryRound.index == categoryRound.index);
        TIMING_CHECK(row, impl.responseCategoryRound.remaining == categoryRound.remaining);
        for (size_t index = 0; index < classRounds.size(); ++index)
        {
            TIMING_CHECK(row, impl.responseClassRounds[index].index == classRounds[index].index);
            TIMING_CHECK(row, impl.responseClassRounds[index].remaining == classRounds[index].remaining);
        }
        TIMING_CHECK(row, impl.pmu.emittedTXRSP == 0);

        for (size_t index = 0; index < expected.size(); ++index)
        {
            impl.time = 14 + index;
            const auto emitted = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
            TIMING_CHECK(row, emitted.has_value());
            if (emitted)
            {
                TIMING_CHECK(row, emitted->opcode == expected[index].opcode);
                TIMING_CHECK(row, emitted->target == expected[index].target);
            }
        }
        TIMING_CHECK(row, impl.txrsp.empty());
        TIMING_CHECK(row, impl.pmu.emittedTXRSP == expected.size());
        impl.Reset();
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP15IndependentOutputBackpressureAndEligibility)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P15" };
        Options options = ProfileOptions(profile);
        options.outputQueueLimit = 1;
        options.structure.outgoingSnoopQueueLimit = 1;
        options.timing.backendRequest = 4;
        options.timing.normalResponse = 2;
        options.timing.dataReturn = 3;
        options.timing.snoopIssue = 5;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& transaction = driver.Add(Timing::MakeRequest(
            CHI::Eb::Opcodes::REQ::ReadUnique, 4, 100));
        const LineKey line = transaction.request.Line();
        impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, 0);
        impl.Bank(line).snoopFilter.Track(line, 6, CacheState::SC, 0);
        transaction.homeHit = true;
        impl.flows.at(transaction.handle).snoopLine = line;
        impl.time = 10;
        impl.PlanReadSnoops(transaction);
        TIMING_CHECK(row, transaction.snoopTargets.size() == 2);

        Request downstream = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 2, 1);
        downstream.target = 6;
        Response normal;
        normal.source = 2;
        normal.target = 4;
        normal.opcode = CHI::Eb::Opcodes::RSP::Comp;
        Data data;
        data.source = 2;
        data.target = 4;
        data.opcode = CHI::Eb::Opcodes::DAT::CompData;
        TIMING_CHECK(row, impl.Queue(impl.txreq, downstream, INVALID_TRANSACTION, 1, false, true));
        TIMING_CHECK(row, impl.Queue(impl.txrsp, normal, INVALID_TRANSACTION, 1));
        TIMING_CHECK(row, impl.Queue(impl.txdat, data, INVALID_TRANSACTION, 1));
        TIMING_CHECK(row, impl.EmitSnoops(transaction));
        ExpectTime(row, impl.txreq.front().readyTime, 15, "full request queue visibility");
        ExpectTime(row, impl.txrsp.front().readyTime, 13, "full response queue visibility");
        ExpectTime(row, impl.txdat.front().readyTime, 14, "full data queue visibility");
        ExpectTime(row, impl.txsnp.front().readyTime, 16, "full snoop queue visibility");
        TIMING_CHECK(row, !impl.Queue(impl.txreq, downstream, INVALID_TRANSACTION, 2, false, true));
        TIMING_CHECK(row, !impl.Queue(impl.txrsp, normal, INVALID_TRANSACTION, 2));
        TIMING_CHECK(row, !impl.Queue(impl.txdat, data, INVALID_TRANSACTION, 2));
        TIMING_CHECK(row, !impl.EmitSnoops(transaction));
        TIMING_CHECK(row, impl.txreq.size() == 1 && impl.txrsp.size() == 1 &&
            impl.txdat.size() == 1 && impl.txsnp.size() == 1);
        TIMING_CHECK(row, impl.flows.at(transaction.handle).nextSnoop == 1);
        TIMING_CHECK(row, impl.Bank(line).outgoingSnoopQueue == 1);
        TIMING_CHECK(row, impl.pmu.GetStalls(WaitReason::OutputQueue) >= 4);

        driver.TickTo(13);
        TIMING_CHECK(row, impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
        TIMING_CHECK(row, impl.txreq.size() == 1 && impl.txdat.size() == 1 && impl.txsnp.size() == 1);
        driver.TickTo(14);
        TIMING_CHECK(row, impl.Pop(impl.txdat, EventKind::EmittedTXDAT));
        TIMING_CHECK(row, impl.txreq.size() == 1 && impl.txsnp.size() == 1);
        driver.TickTo(15);
        TIMING_CHECK(row, impl.Pop(impl.txreq, EventKind::EmittedTXREQ));
        TIMING_CHECK(row, impl.txsnp.size() == 1);
        driver.TickTo(16);
        TIMING_CHECK(row, impl.Pop(impl.txsnp, EventKind::EmittedTXSNP));
        TIMING_CHECK(row, impl.Bank(line).outgoingSnoopQueue == 0);
        TIMING_CHECK(row, impl.EmitSnoops(transaction));
        ExpectTime(row, impl.txsnp.front().readyTime, 22, "released snoop queue visibility");
        TIMING_CHECK(row, impl.Bank(line).activeSnoopWaves == 1);
        driver.TickTo(22);
        TIMING_CHECK(row, impl.Pop(impl.txsnp, EventKind::EmittedTXSNP));
        impl.Retire(transaction.handle);
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());

        Options eligibleOptions = ProfileOptions(profile);
        eligibleOptions.outputQueueLimit = 2;
        eligibleOptions.timing.retryAck = 8;
        eligibleOptions.timing.normalResponse = 2;
        eligibleOptions.timing.output = 1;
        CoreDriver eligibleDriver(eligibleOptions);
        Core::Impl& eligible = eligibleDriver.impl;
        eligible.time = 10;
        Response older;
        older.source = 2;
        older.target = 4;
        older.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
        TIMING_CHECK(row, eligible.Queue(eligible.txrsp, older, INVALID_TRANSACTION, 5));
        eligible.time = 11;
        Response newer = older;
        newer.opcode = CHI::Eb::Opcodes::RSP::Comp;
        TIMING_CHECK(row, eligible.Queue(eligible.txrsp, newer, INVALID_TRANSACTION, 6));
        ExpectTime(row, eligible.txrsp[0].readyTime, 19, "invisible older response");
        ExpectTime(row, eligible.txrsp[1].readyTime, 14, "eligible newer response");
        eligible.time = 14;
        const Response* selected = eligible.Peek(eligible.txrsp);
        TIMING_CHECK(row, selected && selected->opcode == CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, eligible.Peek(eligible.txrsp) == selected);
        ExpectTime(row, eligible.txrsp[0].readyTime, 19, "pure older peek");
        ExpectTime(row, eligible.txrsp[1].readyTime, 14, "pure newer peek");
        const auto first = eligible.Pop(eligible.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, first && first->opcode == CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, !eligible.Peek(eligible.txrsp));
        eligible.time = 19;
        const auto second = eligible.Pop(eligible.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, second && second->opcode == CHI::Eb::Opcodes::RSP::RetryAck);
        TIMING_CHECK(row, !eligible.HasInflight());
        TIMING_CHECK(row, eligible.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP16SaturatingScheduleAndCommit)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P16" };
        constexpr Time maximum = std::numeric_limits<Time>::max();
        Options options = ProfileOptions(profile);
        options.timing = {
            .inputRegister = maximum,
            .requestBuffer = maximum,
            .allocation = maximum,
            .lookupResult = maximum,
            .snoopDecision = maximum,
            .snoopIssue = maximum,
            .dataReturn = maximum,
            .fastResponse = maximum,
            .normalResponse = maximum,
            .backendRequest = maximum,
            .backendData = maximum,
            .retryAck = maximum,
            .creditGrant = maximum,
            .backendResend = maximum,
            .atomicExecution = maximum,
            .persistence = maximum,
            .dataBeat = maximum,
            .output = maximum,
            .commitSchedule = maximum
        };
        VirtualPipeline pipeline(options);
        const std::array<SchedulePoint, 19> points = {
            SchedulePoint::InputRegister, SchedulePoint::RequestBuffer, SchedulePoint::Allocation,
            SchedulePoint::LookupResult, SchedulePoint::SnoopDecision, SchedulePoint::SnoopIssue,
            SchedulePoint::DataReturn, SchedulePoint::FastResponse, SchedulePoint::NormalResponse,
            SchedulePoint::BackendRequest, SchedulePoint::BackendData, SchedulePoint::RetryAck,
            SchedulePoint::CreditGrant, SchedulePoint::BackendResend, SchedulePoint::AtomicExecution,
            SchedulePoint::Persistence, SchedulePoint::DataBeat, SchedulePoint::Output,
            SchedulePoint::CommitSchedule
        };
        for (const SchedulePoint point : points)
            ExpectTime(row, pipeline.SaturatingAdd(maximum - 3, pipeline.OperationDelay(point)),
                maximum, "saturating schedule point");

        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& transaction = driver.Add(Timing::MakeRequest(
            CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 110));
        transaction.phase = Phase::Commit;
        transaction.readyTime = 0;
        impl.time = maximum - 3;
        Response response;
        response.source = 2;
        response.target = 4;
        response.opcode = CHI::Eb::Opcodes::RSP::Comp;
        TIMING_CHECK(row, impl.Queue(impl.txrsp, response, INVALID_TRANSACTION, 0));
        ExpectTime(row, impl.txrsp.front().readyTime, maximum, "saturating output");
        impl.RetireFinished();
        ExpectTime(row, transaction.readyTime, maximum, "saturating commit");
        TIMING_CHECK(row, impl.tracker.Size() == 1);
        impl.time = maximum - 1;
        TIMING_CHECK(row, !impl.Peek(impl.txrsp));
        impl.RetireFinished();
        TIMING_CHECK(row, impl.tracker.Size() == 1);
        impl.time = maximum;
        TIMING_CHECK(row, impl.Peek(impl.txrsp));
        TIMING_CHECK(row, impl.Pop(impl.txrsp, EventKind::EmittedTXRSP));
        impl.RetireFinished();
        TIMING_CHECK(row, impl.tracker.Empty());
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());

        CoreDriver inputDriver(options);
        Core::Impl& input = inputDriver.impl;
        input.time = maximum - 3;
        TIMING_CHECK(row, input.Push(input.rxreq,
            Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 111),
            EventKind::AcceptedRXREQ));
        ExpectTime(row, input.rxreqTimes.front(), maximum, "saturating input register");
        input.time = maximum - 1;
        input.AdmitRequests();
        TIMING_CHECK(row, input.tracker.Empty());
        TIMING_CHECK(row, input.rxreq.size() == 1);
        input.time = maximum;
        input.AdmitRequests();
        TIMING_CHECK(row, input.tracker.Size() == 1);
        const Transaction& admitted = *input.tracker.FindRequest(4, 111);
        ExpectTime(row, input.flows.at(admitted.handle).scheduleMarks.at(SchedulePoint::RequestBuffer),
            maximum, "saturating request buffer");
        input.Reset();
        TIMING_CHECK(row, input.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixP17ResetDrainAndSeededReplay)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P17" };
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.seed = 0x5a17;
        options.timing.backendRequest = 1;
        options.timing.creditGrant = 1;
        options.timing.dataReturn = 1;
        options.timing.snoopIssue = 1;
        options.timing.output = 2;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;

        const auto exercise = [&] {
            impl.time = 4;
            Transaction& backend = driver.Add(Timing::MakeRequest(
                CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 120, 0x100000));
            TIMING_CHECK(row, impl.tracker.BindDBID(backend.handle, backend.handle));
            impl.StartMemoryRead(backend);
            Transaction& snoop = driver.Add(Timing::MakeRequest(
                CHI::Eb::Opcodes::REQ::ReadUnique, 5, 121, 0x100040));
            TIMING_CHECK(row, impl.tracker.BindDBID(snoop.handle, snoop.handle));
            const LineKey line = snoop.request.Line();
            impl.Bank(line).snoopFilter.Track(line, 6, CacheState::UD, 0);
            snoop.homeHit = true;
            impl.flows.at(snoop.handle).snoopLine = line;
            impl.PlanReadSnoops(snoop);
            TIMING_CHECK(row, impl.EmitSnoops(snoop));

            SeedRetry(impl, 0, RetryRequest(8, 122, 0x100080, 15, 0), 0, 2, false);
            Response ack;
            ack.source = 2;
            ack.target = 8;
            ack.txnId = 122;
            ack.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
            ack.qos = 15;
            TIMING_CHECK(row, impl.Queue(impl.txrsp, ack, INVALID_TRANSACTION, 2, false, false, 0));
            SeedRetry(impl, 1, RetryRequest(9, 123, 0x1000c0, 15, 0), 0, 3);
            TIMING_CHECK(row, impl.QueueRetryCredit(1));
            Data data;
            data.source = 2;
            data.target = 4;
            data.opcode = CHI::Eb::Opcodes::DAT::CompData;
            TIMING_CHECK(row, impl.Queue(impl.txdat, data, INVALID_TRANSACTION, impl.nextAge++));
        };
        const auto snapshot = [&] {
            std::vector<uint64_t> trace;
            const auto appendQueue = [&](uint64_t channel, const auto& queue) {
                trace.push_back(channel);
                for (const Pending<Request>& pending : queue)
                {
                    trace.push_back(pending.flit.txnId);
                    trace.push_back(pending.flit.opcode);
                    trace.push_back(pending.readyTime);
                    trace.push_back(pending.age);
                    trace.push_back(pending.serial);
                    trace.push_back(pending.transaction);
                    trace.push_back(static_cast<uint64_t>(pending.schedule));
                }
            };
            appendQueue(1, impl.txreq);
            const auto appendResponse = [&] {
                trace.push_back(2);
                for (const Pending<Response>& pending : impl.txrsp)
                {
                    trace.push_back(pending.flit.txnId);
                    trace.push_back(pending.flit.opcode);
                    trace.push_back(pending.readyTime);
                    trace.push_back(pending.age);
                    trace.push_back(pending.serial);
                    trace.push_back(pending.retry);
                    trace.push_back(static_cast<uint64_t>(pending.schedule));
                }
            };
            appendResponse();
            trace.push_back(3);
            for (const Pending<Data>& pending : impl.txdat)
            {
                trace.push_back(pending.flit.txnId);
                trace.push_back(pending.flit.opcode);
                trace.push_back(pending.readyTime);
                trace.push_back(pending.age);
                trace.push_back(pending.serial);
            }
            trace.push_back(4);
            for (const Pending<Snoop>& pending : impl.txsnp)
            {
                trace.push_back(pending.flit.txnId);
                trace.push_back(pending.flit.opcode);
                trace.push_back(pending.readyTime);
                trace.push_back(pending.age);
                trace.push_back(pending.serial);
                trace.push_back(pending.transaction);
            }
            for (const TransactionHandle handle : impl.tracker.Handles())
            {
                const Transaction& transaction = *impl.tracker.Find(handle);
                trace.push_back(handle);
                trace.push_back(transaction.actionAge);
                trace.push_back(transaction.readyTime);
                trace.push_back(static_cast<uint64_t>(transaction.phase));
                for (const auto& [point, mark] : impl.flows.at(handle).scheduleMarks)
                {
                    trace.push_back(static_cast<uint64_t>(point));
                    trace.push_back(mark);
                }
            }
            trace.push_back(impl.nextAge);
            trace.push_back(impl.nextSerial);
            trace.push_back(impl.nextRetry);
            trace.push_back(impl.nextBackendRetry);
            trace.push_back(impl.nextBackendSelection);
            for (const WeightedRoundState& round : impl.retryRounds)
            {
                trace.push_back(round.index);
                trace.push_back(round.remaining);
            }
            return trace;
        };

        exercise();
        TIMING_CHECK(row, impl.HasInflight());
        TIMING_CHECK(row, impl.flows.at(impl.tracker.FindRequest(4, 120)->handle).backendOperationOwned);
        TIMING_CHECK(row, !impl.retries.empty());
        TIMING_CHECK(row, !impl.txreq.empty() && !impl.txrsp.empty() &&
            !impl.txdat.empty() && !impl.txsnp.empty());
        TIMING_CHECK(row, impl.ValidateInvariants());
        const std::vector<uint64_t> baseline = snapshot();

        Core drainProbe(2, options, std::make_shared<EventHub>());
        TIMING_CHECK(row, drainProbe.PushRXREQ(Timing::MakeRequest(
            CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 124)));
        drainProbe.Tick(4);
        TIMING_CHECK(row, !drainProbe.Drain());
        TIMING_CHECK(row, drainProbe.HasInflight());
        drainProbe.Reset();
        TIMING_CHECK(row, drainProbe.Drain());
        TIMING_CHECK(row, drainProbe.ValidateInvariants());

        impl.Reset();
        TIMING_CHECK(row, impl.time == 0);
        TIMING_CHECK(row, impl.tracker.Empty() && impl.flows.empty());
        TIMING_CHECK(row, impl.rxreq.empty() && impl.rxrsp.empty() && impl.rxdat.empty());
        TIMING_CHECK(row, impl.txreq.empty() && impl.txrsp.empty() &&
            impl.txdat.empty() && impl.txsnp.empty());
        TIMING_CHECK(row, impl.retries.empty() && impl.retrySources.empty());
        TIMING_CHECK(row, impl.availableCredits.empty() && impl.cancellationCredits.empty());
        TIMING_CHECK(row, impl.backendRetries.empty() && impl.backendCredits.empty());
        TIMING_CHECK(row, impl.nextAge == 0 && impl.nextSerial == 0 && impl.nextRetry == 0);
        TIMING_CHECK(row, impl.nextBackendRetry == 0 && impl.nextBackendSelection == 0);
        TIMING_CHECK(row, impl.options.profile == profile && impl.options.seed == options.seed);
        for (const BankState& bank : impl.banks)
        {
            TIMING_CHECK(row, !bank.pipeline.TagBusy(0));
            TIMING_CHECK(row, !bank.pipeline.DataBusy(0));
            TIMING_CHECK(row, bank.activeOriginals == 0 && bank.requestBufferOccupants == 0);
            TIMING_CHECK(row, bank.activeSnoopWaves == 0 && bank.outgoingSnoopQueue == 0);
            TIMING_CHECK(row, bank.responseUnits == 0 && bank.backendOperations == 0);
        }
        for (const WeightedRoundState& round : impl.retryRounds)
            TIMING_CHECK(row, round.index == 0 && round.remaining == 0);
        for (const WeightedRoundState& round : impl.backendRetryRounds)
            TIMING_CHECK(row, round.index == 0 && round.remaining == 0);
        for (const WeightedRoundState& round : impl.responseClassRounds)
            TIMING_CHECK(row, round.index == 0 && round.remaining == 0);
        TIMING_CHECK(row, impl.requestClassRound.index == 0 && impl.requestClassRound.remaining == 0);
        TIMING_CHECK(row, impl.responseCategoryRound.index == 0 &&
            impl.responseCategoryRound.remaining == 0);
        TIMING_CHECK(row, !impl.HasInflight());
        TIMING_CHECK(row, impl.ValidateInvariants());

        exercise();
        TIMING_CHECK(row, snapshot() == baseline);
        TIMING_CHECK(row, impl.ValidateInvariants());
        impl.Reset();
        TIMING_CHECK(row, !impl.HasInflight());
    }
}

LUNATIC_TEST(Stage10MatrixP18CommitScheduleIndependentOfResponsePop)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "P18" };
        Options options = ProfileOptions(profile);
        options.timing.fastResponse = 2;
        options.timing.normalResponse = 2;
        options.timing.output = 1;
        options.timing.commitSchedule = 3;

        CoreDriver responseDriver(options);
        Core::Impl& responseImpl = responseDriver.impl;
        responseImpl.time = 10;
        Request evict = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::Evict, 4, 130);
        evict.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        Transaction& responseTransaction = responseDriver.Add(evict);
        responseTransaction.phase = Phase::Action;
        responseTransaction.readyTime = 10;
        responseImpl.ActionCoherent(responseTransaction);
        TIMING_CHECK(row, responseImpl.txrsp.size() == 1);
        ExpectTime(row, responseImpl.txrsp.front().readyTime, 13, "response-driven Comp");
        TIMING_CHECK(row, responseImpl.flows.at(responseTransaction.handle).responseOwned);
        responseDriver.TickTo(12);
        TIMING_CHECK(row, !responseImpl.Peek(responseImpl.txrsp));
        responseImpl.RetireFinished();
        TIMING_CHECK(row, responseImpl.tracker.Size() == 1);
        responseDriver.TickTo(13);
        responseImpl.RetireFinished();
        TIMING_CHECK(row, responseImpl.tracker.Size() == 1);
        const auto comp = responseImpl.Pop(responseImpl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, comp && comp->opcode == CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, responseTransaction.phase == Phase::Commit);
        responseImpl.RetireFinished();
        ExpectTime(row, responseTransaction.readyTime, 16, "response-driven commit deadline");
        TIMING_CHECK(row, responseImpl.tracker.Size() == 1);
        responseDriver.TickTo(15);
        TIMING_CHECK(row, responseImpl.tracker.Size() == 1);
        responseDriver.TickTo(16);
        TIMING_CHECK(row, responseImpl.tracker.Empty());
        TIMING_CHECK(row, !responseImpl.HasInflight());
        TIMING_CHECK(row, responseImpl.ValidateInvariants());

        CoreDriver copybackDriver(options);
        Core::Impl& copybackImpl = copybackDriver.impl;
        copybackImpl.time = 20;
        Transaction& copyback = copybackDriver.Add(Timing::MakeRequest(
            CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 131, 0x100040));
        TIMING_CHECK(row, copybackImpl.tracker.BindDBID(copyback.handle, copyback.handle));
        copybackImpl.flows.at(copyback.handle).expectedBeatMask = 3;
        copyback.phase = Phase::Action;
        copyback.readyTime = 20;
        copybackImpl.ActionCoherent(copyback);
        TIMING_CHECK(row, copybackImpl.txrsp.size() == 1);
        TIMING_CHECK(row, copybackImpl.txrsp.front().flit.opcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
        ExpectTime(row, copybackImpl.txrsp.front().readyTime, 23, "CopyBack immediate response");
        copybackDriver.TickTo(22);
        copybackImpl.RetireFinished();
        TIMING_CHECK(row, copybackImpl.tracker.Size() == 1);
        copybackDriver.TickTo(23);
        const auto immediate = copybackImpl.Pop(copybackImpl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, immediate && immediate->opcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
        TIMING_CHECK(row, copyback.phase == Phase::WaitWriteData);
        TIMING_CHECK(row, copybackImpl.flows.at(copyback.handle).responseOwned);
        const DataBufferId dataBufferId = copyback.dbid;

        Data data;
        data.source = 4;
        data.target = 2;
        data.txnId = dataBufferId;
        data.opcode = CHI::Eb::Opcodes::DAT::CopyBackWrData;
        data.resp = CHI::Eb::Resps::CopyBackWrData_UD_PD;
        data.byteEnables.fill(1);
        data.dataId = 0;
        copybackImpl.time = 24;
        copybackImpl.ProcessDAT(data);
        TIMING_CHECK(row, !copyback.Complete(COMPLETE_WRITE_DATA));
        data.dataId = 2;
        copybackImpl.time = 25;
        copybackImpl.ProcessDAT(data);
        TIMING_CHECK(row, copyback.Complete(COMPLETE_WRITE_DATA));
        TIMING_CHECK(row, copyback.phase == Phase::Action);
        copybackImpl.ActionCoherent(copyback);
        TIMING_CHECK(row, copyback.phase == Phase::Commit);
        TIMING_CHECK(row, copybackImpl.flows.at(copyback.handle).directoryCommitted);
        TIMING_CHECK(row, copybackImpl.flows.at(copyback.handle).responseOwned);
        copybackImpl.RetireFinished();
        ExpectTime(row, copyback.readyTime, 28, "CopyBack commit deadline");
        TIMING_CHECK(row, copybackImpl.tracker.Size() == 1);
        copybackDriver.TickTo(27);
        TIMING_CHECK(row, copybackImpl.tracker.Size() == 1);
        TIMING_CHECK(row, copybackImpl.Bank(copyback).responseUnits == 1);
        copybackDriver.TickTo(28);
        TIMING_CHECK(row, copybackImpl.tracker.Empty());
        TIMING_CHECK(row, copybackImpl.Bank(LineKey::FromAddress(0x100040, false)).responseUnits == 0);
        TIMING_CHECK(row, !copybackImpl.HasInflight());
        TIMING_CHECK(row, copybackImpl.ValidateInvariants());
    }
}
