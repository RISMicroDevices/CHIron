#include "timing_matrix.hpp"

#include <array>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;
using namespace LunaticTest::Timing;

namespace {

    const std::array<ProfileKind, 2> Profiles = { ProfileKind::Type3, ProfileKind::Type7 };

}

LUNATIC_TEST(Stage10MatrixF01FrontEndAbsoluteSchedule)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F01" };
        Options options = ProfileOptions(profile);
        options.timing.inputRegister = 2;
        options.timing.requestBuffer = 3;
        options.timing.allocation = 4;
        options.timing.lookupResult = 5;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        TIMING_CHECK(row, impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ));
        impl.Tick();
        driver.TickTo(1);
        TIMING_CHECK(row, impl.tracker.Empty());
        driver.TickTo(2);
        TIMING_CHECK(row, impl.tracker.Size() == 1);
        const Transaction& transaction = *impl.tracker.FindRequest(4, 1);
        const FlowState& flow = impl.flows.at(transaction.handle);
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::RequestBuffer), 6, "request buffer");
        driver.TickTo(12);
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::Allocation), 12, "allocation");
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::LookupResult), 17 + options.tagLatency, "lookup result");
        TIMING_CHECK(row, !flow.requestBufferOwned);
        TIMING_CHECK(row, transaction.phase == Phase::Lookup);
        driver.TickTo(17 + options.tagLatency);
        TIMING_CHECK(row, transaction.phase == Phase::Resolve);
        ExpectTime(row, transaction.readyTime, 18 + options.tagLatency, "resolve eligibility");
    }
}

LUNATIC_TEST(Stage10MatrixF02IndependentInputReadiness)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F02" };
        Options options = ProfileOptions(profile);
        options.timing.inputRegister = 2;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& write = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 10));
        write.phase = Phase::WaitWriteData;
        write.readyTime = 0;
        TIMING_CHECK(row, impl.tracker.BindDBID(write.handle, write.handle));
        FlowState& writeFlow = impl.flows.at(write.handle);
        writeFlow.expectedBeatMask = 3;
        writeFlow.dbidEmitted = true;

        const Request read = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 1, 0x100100);
        TIMING_CHECK(row, impl.Push(impl.rxreq, read, EventKind::AcceptedRXREQ));
        driver.TickTo(1);
        Response credit;
        credit.source = 6;
        credit.target = 2;
        credit.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
        TIMING_CHECK(row, impl.Push(impl.rxrsp, credit, EventKind::AcceptedRXRSP));
        driver.TickTo(2);
        TIMING_CHECK(row, impl.tracker.FindRequest(5, 1) != nullptr);
        TIMING_CHECK(row, impl.rxrsp.size() == 1);

        Data data;
        data.source = 4;
        data.target = 2;
        data.txnId = write.dbid;
        data.opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.byteEnables.fill(1);
        TIMING_CHECK(row, impl.Push(impl.rxdat, data, EventKind::AcceptedRXDAT));
        ExpectTime(row, impl.rxreqTimes.empty() ? 0 : impl.rxreqTimes.front(), 0, "request consumed");
        ExpectTime(row, impl.rxrspTimes.front(), 3, "response readiness");
        ExpectTime(row, impl.rxdatTimes.front(), 4, "data readiness");
        driver.TickTo(3);
        TIMING_CHECK(row, impl.rxrsp.empty());
        TIMING_CHECK(row, impl.rxdat.size() == 1);
        TIMING_CHECK(row, write.receivedBeats == 0);
        TIMING_CHECK(row, impl.backendCredits.at({ 6, 0 }) == 1);
        driver.TickTo(4);
        TIMING_CHECK(row, impl.rxdat.empty());
        TIMING_CHECK(row, write.receivedBeats == 1);
        TIMING_CHECK(row, impl.errors.empty());
        impl.Retire(write.handle);
        impl.Retire(impl.tracker.FindRequest(5, 1)->handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixF03RequestBufferReleaseAdmission)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F03" };
        Options options = ProfileOptions(profile);
        options.structure.requestBufferLimit = 1;
        options.timing.allocation = 10;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const Request first = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1);
        const Request second = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2, 0x100100);
        TIMING_CHECK(row, impl.Push(impl.rxreq, first, EventKind::AcceptedRXREQ));
        impl.Tick();
        driver.TickTo(1);
        TIMING_CHECK(row, impl.Push(impl.rxreq, second, EventKind::AcceptedRXREQ));
        impl.Tick();
        TIMING_CHECK(row, impl.tracker.Size() == 1);
        TIMING_CHECK(row, impl.rxreq.size() == 1);
        TIMING_CHECK(row, impl.txrsp.empty());
        const Transaction& allocated = *impl.tracker.FindRequest(4, 1);
        TIMING_CHECK(row, impl.flows.at(allocated.handle).requestBufferOwned);
        driver.TickTo(13);
        TIMING_CHECK(row, !impl.flows.at(allocated.handle).requestBufferOwned);
        TIMING_CHECK(row, impl.tracker.Size() == 1);
        TIMING_CHECK(row, impl.rxreq.size() == 1);
        driver.TickTo(14);
        TIMING_CHECK(row, impl.tracker.Size() == 2);
        TIMING_CHECK(row, impl.rxreq.empty());
        const Transaction& admitted = *impl.tracker.FindRequest(5, 2);
        ExpectTime(row, impl.flows.at(admitted.handle).scheduleMarks.at(SchedulePoint::RequestBuffer),
            15, "post-stall request buffer");
        impl.Retire(allocated.handle);
        impl.Retire(admitted.handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixF04IdleTagLatencyBoundary)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F04" };
        Options options = ProfileOptions(profile);
        options.tagLatency = 3;
        VirtualPipeline pipeline(options);
        ExpectTime(row, pipeline.ScheduleTag(5), 8, "idle tag result");
        TIMING_CHECK(row, pipeline.TagBusy(5));
        TIMING_CHECK(row, !pipeline.TagBusy(6));
        ExpectTime(row, pipeline.ScheduleData(7), 7 + options.dataLatency, "idle data result");
        TIMING_CHECK(row, pipeline.DataBusy(7));
        TIMING_CHECK(row, !pipeline.DataBusy(6 + options.dataLatency));
    }
}

LUNATIC_TEST(Stage10MatrixF05ConfiguredTagOccupancy)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F05" };
        Options options = ProfileOptions(profile);
        options.structure.tagPortOccupancy = 3;
        VirtualPipeline pipeline(options);
        ExpectTime(row, pipeline.ScheduleTag(5), 5 + options.tagLatency, "first tag result");
        ExpectTime(row, pipeline.ScheduleTag(5), 8 + options.tagLatency, "second tag result");
        ExpectTime(row, pipeline.ScheduleTag(5), 11 + options.tagLatency, "third tag result");
    }
}

LUNATIC_TEST(Stage10MatrixF06ConfiguredDataOccupancy)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F06" };
        Options options = ProfileOptions(profile);
        options.structure.dataPortOccupancy = 4;
        VirtualPipeline pipeline(options);
        ExpectTime(row, pipeline.ScheduleData(5), 5 + options.dataLatency, "first data result");
        ExpectTime(row, pipeline.ScheduleData(5), 9 + options.dataLatency, "second data result");
        ExpectTime(row, pipeline.ScheduleData(5), 13 + options.dataLatency, "third data result");
    }
}

LUNATIC_TEST(Stage10MatrixF07ParallelBankPorts)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F07" };
        Options options = ProfileOptions(profile);
        CoreDriver driver(options);
        for (size_t bank = 0; bank < 4; ++bank)
        {
            const LineKey line = LineKey::FromAddress(0x100000 + bank * LINE_SIZE, false);
            if (profile == ProfileKind::Type3)
                TIMING_CHECK(row, driver.impl.BankIndex(line) == bank);
            else
                TIMING_CHECK(row, driver.impl.BankIndex(line) == 0);
            ExpectTime(row, driver.impl.Bank(line).pipeline.ScheduleTag(0),
                profile == ProfileKind::Type3 ? options.tagLatency : options.tagLatency + bank, "bank tag result");
        }
    }
}

LUNATIC_TEST(Stage10MatrixF08SameLineReleaseDoesNotReallocate)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F08" };
        Options options = ProfileOptions(profile);
        options.memoryLatency = 2;
        options.timing.lookupResult = 3;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const Request olderRequest = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1);
        const Request youngerRequest = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2);
        Transaction& older = driver.Add(olderRequest);
        Transaction& younger = driver.Add(youngerRequest);
        const TransactionHandle olderHandle = older.handle;
        driver.TickTo(20);
        TIMING_CHECK(row, older.phase == Phase::Return);
        TIMING_CHECK(row, younger.phase == Phase::Hazard);
        ExpectTime(row, impl.flows.at(younger.handle).scheduleMarks.at(SchedulePoint::Allocation),
            3, "younger allocation before stall");
        TIMING_CHECK(row, impl.txdat.size() == 2);
        TIMING_CHECK(row, impl.Pop(impl.txdat, EventKind::EmittedTXDAT));
        TIMING_CHECK(row, impl.Pop(impl.txdat, EventKind::EmittedTXDAT));
        impl.RetireFinished();
        TIMING_CHECK(row, impl.tracker.Find(olderHandle) == nullptr);
        TIMING_CHECK(row, younger.phase == Phase::Hazard);
        driver.TickTo(21);
        ExpectTime(row, impl.flows.at(younger.handle).scheduleMarks.at(SchedulePoint::Allocation),
            3, "younger allocation after release");
        ExpectTime(row, impl.flows.at(younger.handle).scheduleMarks.at(SchedulePoint::LookupResult),
            24 + options.tagLatency, "younger lookup after release");
        impl.Retire(younger.handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixF09LinkedVictimReleaseBoundary)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F09" };
        Options options = ProfileOptions(profile);
        options.timing.lookupResult = 4;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp));
        const LineKey original = transaction.request.Line();
        const LineKey victim = LineKey::FromAddress(original.address + LINE_SIZE, false);
        FlowState& flow = impl.flows.at(transaction.handle);
        flow.victimLinked = true;
        flow.victimLine = victim;
        transaction.phase = Phase::Hazard;
        transaction.readyTime = 0;
        constexpr TransactionHandle olderOriginal = 900;
        constexpr TransactionHandle olderVictim = 901;
        impl.Bank(original).hazards.Add(olderOriginal, original, 0);
        impl.Bank(original).hazards.Add(transaction.handle, original, 1);
        impl.Bank(victim).hazards.Add(olderVictim, victim, 0);
        impl.Bank(victim).hazards.Add(transaction.handle, victim, 1);
        impl.Advance(transaction);
        TIMING_CHECK(row, transaction.phase == Phase::Hazard);
        impl.Bank(original).hazards.Release(olderOriginal, original);
        impl.Advance(transaction);
        TIMING_CHECK(row, transaction.phase == Phase::Hazard);
        TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::LookupResult));
        impl.Bank(victim).hazards.Release(olderVictim, victim);
        impl.Advance(transaction);
        TIMING_CHECK(row, transaction.phase == Phase::Lookup);
        ExpectTime(row, transaction.readyTime, 4 + options.tagLatency, "linked victim lookup release");
        impl.Retire(transaction.handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixF10SameSetDifferentLinesPipeline)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F10" };
        Options options = ProfileOptions(profile);
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const uint64_t alias = options.structure.bankCount * options.slcSets * LINE_SIZE;
        Transaction& first = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1));
        Transaction& second = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2,
            0x100000 + alias));
        TIMING_CHECK(row, impl.BankIndex(first.request.Line()) == impl.BankIndex(second.request.Line()));
        TIMING_CHECK(row, impl.Bank(first).directory.SetIndex(first.request.Line()) ==
            impl.Bank(second).directory.SetIndex(second.request.Line()));
        for (Transaction* transaction : { &first, &second })
        {
            transaction->phase = Phase::Hazard;
            transaction->readyTime = 0;
            impl.Bank(*transaction).hazards.Add(transaction->handle, transaction->request.Line(),
                transaction->actionAge);
        }
        impl.Advance(first);
        impl.Advance(second);
        TIMING_CHECK(row, first.phase == Phase::Lookup);
        TIMING_CHECK(row, second.phase == Phase::Lookup);
        ExpectTime(row, first.readyTime, options.tagLatency, "first same-set lookup");
        ExpectTime(row, second.readyTime, options.tagLatency + 1, "second same-set lookup");
        impl.Retire(first.handle);
        impl.Retire(second.handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixF11RefillEligibilityAndPriority)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F11" };
        Options options = ProfileOptions(profile);
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const auto add = [&](NodeId source, TransactionId txnId, uint64_t address, uint8_t qos) {
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, source, txnId, address);
            request.qos = qos;
            return &driver.Add(request);
        };
        Transaction& older = *add(4, 1, 0x100000, 15);
        Transaction& refill = *add(5, 2, 0x100100, 12);
        Transaction& younger = *add(6, 3, 0x100080, 8);
        Transaction& waiting = *add(7, 4, 0x1000c0, 0);
        for (Transaction* transaction : { &older, &refill, &younger, &waiting })
        {
            transaction->phase = Phase::Action;
            transaction->readyTime = 7;
        }
        refill.Mark(COMPLETE_MEMORY);
        impl.flows.at(refill.handle).backendPhase = BackendPhase::Done;
        waiting.memoryRequired = true;
        impl.time = 7;
        TIMING_CHECK(row, impl.IsRefillTransaction(refill));
        TIMING_CHECK(row, !impl.IsRefillTransaction(waiting));
        std::vector<TransactionHandle> order = impl.tracker.Handles();
        impl.OrderTransactions(order);
        TIMING_CHECK(row, order.size() == 4);
        if (profile == ProfileKind::Type3)
            TIMING_CHECK(row, (order == std::vector<TransactionHandle>{
                refill.handle, older.handle, younger.handle, waiting.handle }));
        else
            TIMING_CHECK(row, (order == std::vector<TransactionHandle>{
                older.handle, refill.handle, younger.handle, waiting.handle }));
        for (const TransactionHandle handle : order)
            ExpectTime(row, impl.tracker.Find(handle)->readyTime, 7, "arbitration preserves readiness");
        impl.Retire(older.handle);
        impl.Retire(refill.handle);
        impl.Retire(younger.handle);
        impl.Retire(waiting.handle);
    }
}

LUNATIC_TEST(Stage10MatrixF12ClassWeightedCommittedSelection)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F12" };
        Options options = ProfileOptions(profile);
        options.structure.classes.enabled = true;
        options.structure.classes.requestWeights = { 2, 1, 1, 1 };
        options.strategy.requestArbitration = RequestArbitrationKind::OldestClassWeighted;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        std::array<Transaction*, 3> transactions{};
        const std::array<uint8_t, 3> qos = { 15, 12, 8 };
        for (size_t index = 0; index < transactions.size(); ++index)
        {
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp,
                4 + index, 1 + index, 0x100000 + index * LINE_SIZE);
            request.qos = qos[index];
            impl.nextAge = 8;
            transactions[index] = &driver.Add(request);
            transactions[index]->phase = Phase::Action;
            transactions[index]->readyTime = 0;
        }
        const std::vector<TransactionHandle> expected = {
            transactions[0]->handle, transactions[1]->handle, transactions[2]->handle
        };
        for (size_t iteration = 0; iteration < 2; ++iteration)
        {
            std::vector<TransactionHandle> order = impl.tracker.Handles();
            impl.OrderTransactions(order);
            TIMING_CHECK(row, order == expected);
            for (const TransactionHandle handle : order)
                ExpectTime(row, impl.tracker.Find(handle)->readyTime, 0, "weighted selection readiness");
        }
        for (Transaction* transaction : transactions)
            impl.Retire(transaction->handle);
    }
}

LUNATIC_TEST(Stage10MatrixF13ResourceReleaseEnablesWaitingOperation)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F13" };
        Options options = ProfileOptions(profile);
        options.structure.responseResourceLimit = 1;
        options.structure.memoryResourceLimit = 1;
        options.structure.refillResourceLimit = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& first = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1));
        Transaction& second = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2, 0x100100));
        impl.time = 5;
        TIMING_CHECK(row, impl.AcquireResponse(first, impl.txrsp));
        TIMING_CHECK(row, !impl.AcquireResponse(second, impl.txrsp));
        TIMING_CHECK(row, impl.AcquireBackendOperation(first));
        TIMING_CHECK(row, !impl.AcquireBackendOperation(second));
        impl.flows.at(first.handle).baselineLine = first.request.Line();
        impl.flows.at(second.handle).baselineLine = second.request.Line();
        TIMING_CHECK(row, impl.AcquireRefill(first));
        TIMING_CHECK(row, !impl.AcquireRefill(second));
        impl.time = 6;
        impl.ReleaseResponse(first);
        impl.ReleaseBackendOperation(first);
        impl.ReleaseRefill(first);
        TIMING_CHECK(row, impl.AcquireResponse(second, impl.txrsp));
        TIMING_CHECK(row, impl.AcquireBackendOperation(second));
        TIMING_CHECK(row, impl.AcquireRefill(second));
        size_t responseUnits = 0;
        size_t backendOperations = 0;
        size_t refillOperations = 0;
        for (const BankState& bank : impl.banks)
        {
            responseUnits += bank.responseUnits;
            backendOperations += bank.backendOperations;
            refillOperations += bank.refillOperations;
        }
        TIMING_CHECK(row, responseUnits == 1);
        TIMING_CHECK(row, backendOperations == 1);
        TIMING_CHECK(row, refillOperations == 1);
        impl.Retire(first.handle);
        impl.Retire(second.handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixF14SnoopQueueBackpressureAbsoluteTime)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F14" };
        Options options = ProfileOptions(profile);
        options.structure.outgoingSnoopQueueLimit = 1;
        options.structure.snoopResourceLimit = 1;
        options.timing.snoopIssue = 3;
        options.timing.output = 2;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadUnique));
        const LineKey line = transaction.request.Line();
        impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, 0);
        impl.Bank(line).snoopFilter.Track(line, 6, CacheState::SC, 0);
        transaction.homeHit = true;
        impl.flows.at(transaction.handle).snoopLine = line;
        impl.PlanReadSnoops(transaction);
        TIMING_CHECK(row, transaction.snoopTargets.size() == 2);
        TIMING_CHECK(row, impl.EmitSnoops(transaction));
        TIMING_CHECK(row, impl.txsnp.size() == 1);
        ExpectTime(row, impl.txsnp.front().readyTime, 5, "first snoop visibility");
        TIMING_CHECK(row, !impl.EmitSnoops(transaction));
        TIMING_CHECK(row, impl.flows.at(transaction.handle).nextSnoop == 1);
        TIMING_CHECK(row, impl.Bank(line).activeSnoopWaves == 1);
        TIMING_CHECK(row, impl.Bank(line).outgoingSnoopQueue == 1);
        driver.TickTo(5);
        TIMING_CHECK(row, impl.Peek(impl.txsnp) != nullptr);
        driver.TickTo(6);
        TIMING_CHECK(row, !impl.EmitSnoops(transaction));
        TIMING_CHECK(row, impl.Pop(impl.txsnp, EventKind::EmittedTXSNP));
        TIMING_CHECK(row, impl.Bank(line).outgoingSnoopQueue == 0);
        TIMING_CHECK(row, impl.EmitSnoops(transaction));
        ExpectTime(row, impl.txsnp.front().readyTime, 11, "second snoop visibility");
        TIMING_CHECK(row, impl.Bank(line).activeSnoopWaves == 1);
        driver.TickTo(11);
        TIMING_CHECK(row, impl.Pop(impl.txsnp, EventKind::EmittedTXSNP));
        impl.Retire(transaction.handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixF15EventBackpressureAndDelayLatching)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F15" };
        Options options = ProfileOptions(profile);
        options.receiveDelay = 2;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        size_t blocked = 2;
        size_t delayed = 0;
        impl.events->OnTransactionSchedule.Register(
            Gravity::MakeListener<TransactionScheduleEvent>("matrix-f15", 0,
                [&](TransactionScheduleEvent& event) {
                    if (event.phase != Phase::Receive)
                        return;
                    if (blocked)
                    {
                        --blocked;
                        event.Backpressure();
                        return;
                    }
                    ++delayed;
                    event.AddDelay(3);
                }));
        Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp));
        driver.TickTo(1);
        TIMING_CHECK(row, transaction.phase == Phase::Receive);
        ExpectTime(row, transaction.readyTime, 3, "static receive delay");
        TIMING_CHECK(row, blocked == 1);
        driver.TickTo(3);
        TIMING_CHECK(row, blocked == 0);
        TIMING_CHECK(row, delayed == 0);
        driver.TickTo(4);
        TIMING_CHECK(row, delayed == 1);
        ExpectTime(row, transaction.readyTime, 6, "event receive delay");
        driver.TickTo(6);
        TIMING_CHECK(row, delayed == 1);
        TIMING_CHECK(row, transaction.phase == Phase::Decode);
        ExpectTime(row, transaction.readyTime, 7, "decode after latched delay");
        ExpectTime(row, impl.flows.at(transaction.handle).scheduleMarks.at(SchedulePoint::RequestBuffer),
            1, "request buffer unchanged by event");
        impl.Retire(transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixF16SeededResetReplay)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "F16" };
        Options options = ProfileOptions(profile);
        options.seed = 0x51;
        options.randomDelayLimit = 3;
        options.randomizeArbitration = true;
        options.strategy.requestArbitration = RequestArbitrationKind::Age;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const auto run = [&] {
            driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x100000));
            driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2, 0x100100));
            driver.TickTo(20);
            std::vector<Time> trace;
            for (const TransactionHandle handle : impl.tracker.Handles())
            {
                const Transaction& transaction = *impl.tracker.Find(handle);
                const FlowState& flow = impl.flows.at(handle);
                trace.push_back(transaction.actionAge);
                trace.push_back(static_cast<Time>(transaction.phase));
                trace.push_back(transaction.readyTime);
                for (const SchedulePoint point : { SchedulePoint::RequestBuffer, SchedulePoint::Allocation,
                    SchedulePoint::LookupResult })
                {
                    trace.push_back(flow.scheduleMarks.contains(point)
                        ? flow.scheduleMarks.at(point) : std::numeric_limits<Time>::max());
                }
            }
            for (const Pending<Data>& pending : impl.txdat)
                trace.push_back(pending.readyTime);
            return trace;
        };
        const std::vector<Time> expected = run();
        TIMING_CHECK(row, impl.ValidateInvariants());
        impl.Reset();
        TIMING_CHECK(row, impl.tracker.Empty());
        TIMING_CHECK(row, impl.flows.empty());
        TIMING_CHECK(row, impl.txdat.empty());
        TIMING_CHECK(row, impl.rxreqTimes.empty());
        for (const BankState& bank : impl.banks)
        {
            TIMING_CHECK(row, !bank.pipeline.TagBusy(0));
            TIMING_CHECK(row, !bank.pipeline.DataBusy(0));
        }
        const std::vector<Time> replayed = run();
        TIMING_CHECK(row, replayed == expected);
        TIMING_CHECK(row, impl.ValidateInvariants());
        impl.Reset();
    }
}
