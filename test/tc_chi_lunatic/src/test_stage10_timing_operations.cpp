#include "timing_matrix.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;
using namespace LunaticTest::Timing;

namespace {

    const std::array<ProfileKind, 2> Profiles = { ProfileKind::Type3, ProfileKind::Type7 };

    Transaction& AddTransaction(const Row& row, CoreDriver& driver, const Request& request)
    {
        Transaction& transaction = driver.Add(request);
        TIMING_CHECK(row, driver.impl.tracker.BindDBID(transaction.handle, transaction.handle));
        return transaction;
    }

    LineData OperationData(uint8_t seed = 5)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 3);
        return data;
    }

    void SetInteger(LineData& data, size_t offset, size_t bytes, uint64_t value)
    {
        for (size_t index = 0; index < bytes; ++index)
            data[offset + index] = index < sizeof(value) ? uint8_t(value >> (index * 8)) : 0;
    }

    void SeedLookup(Core::Impl& impl, Transaction& transaction, bool homeValid, bool homeDirty,
                    const LineData& data)
    {
        const LineKey line = transaction.request.Line();
        if (homeValid)
            impl.InstallHome(line, 0, data, homeDirty, impl.time);
        else
            impl.memory.WriteFull(line, data);
        transaction.homeHit = homeValid;
        transaction.data = data;
        FlowState& flow = impl.flows.at(transaction.handle);
        flow.homeDirty = homeValid && homeDirty;
        flow.validBytes.fill(1);
        flow.snoopLine = line;
    }

    Response SnoopResponse(const Snoop& snoop, uint8_t resp, uint8_t dataPull = 0)
    {
        Response response;
        response.source = snoop.target;
        response.target = 2;
        response.txnId = snoop.txnId;
        response.opcode = CHI::Eb::Opcodes::RSP::SnpResp;
        response.resp = resp;
        response.fwdState = dataPull;
        return response;
    }

    Data SnoopData(const Snoop& snoop, size_t beat, const LineData& data, uint8_t resp,
                   size_t bytes = 32)
    {
        Data flit;
        flit.source = snoop.target;
        flit.target = 2;
        flit.homeNid = 2;
        flit.txnId = snoop.txnId;
        flit.opcode = CHI::Eb::Opcodes::DAT::SnpRespData;
        flit.resp = resp;
        flit.dataId = beat * bytes / 16;
        flit.ccid = (snoop.address >> 4) & 3;
        for (size_t index = 0; index < bytes; ++index)
        {
            flit.bytes[index] = data[beat * bytes + index];
            flit.byteEnables[index] = 1;
        }
        return flit;
    }

    Response BackendResponse(const Transaction& transaction, uint16_t opcode, DataBufferId dbid = 77)
    {
        Response response;
        response.source = 6;
        response.target = 2;
        response.txnId = opcode == CHI::Eb::Opcodes::RSP::Persist ? 0 : transaction.handle;
        response.opcode = opcode;
        response.dbid = dbid;
        response.resp = CHI::Eb::Resps::Comp_UC;
        response.pGroupId = transaction.request.pGroupId;
        return response;
    }

    void CheckLine(const Row& row, Core::Impl& impl, const LineKey& line, const LineData& expected)
    {
        if (const HomeLine* home = impl.Bank(line).directory.Find(line))
            TIMING_CHECK(row, home->data == expected);
        else
            TIMING_CHECK(row, impl.memory.Read(line) == expected);
    }

    void RetireChecked(const Row& row, Core::Impl& impl, TransactionHandle handle)
    {
        impl.Retire(handle);
        TIMING_CHECK(row, impl.ValidateInvariants());
    }

    Options OperationOptions(ProfileKind profile, BackendKind backend = BackendKind::Internal)
    {
        Options options = ProfileOptions(profile);
        options.backend = backend;
        options.subordinateId = 6;
        return options;
    }

    Snoop PopOnlySnoop(const Row& row, Core::Impl& impl, Time visible, uint16_t opcode)
    {
        TIMING_CHECK(row, !impl.txsnp.empty());
        ExpectTime(row, impl.txsnp.front().readyTime, visible, "snoop visibility");
        impl.time = visible - 1;
        TIMING_CHECK(row, !impl.Peek(impl.txsnp));
        impl.time = visible;
        const auto snoop = impl.Pop(impl.txsnp, EventKind::EmittedTXSNP);
        TIMING_CHECK(row, snoop && snoop->opcode == opcode);
        return *snoop;
    }

    Response PopOnlyResponse(const Row& row, Core::Impl& impl, Time visible, uint16_t opcode)
    {
        TIMING_CHECK(row, !impl.txrsp.empty());
        ExpectTime(row, impl.txrsp.front().readyTime, visible, "response visibility");
        impl.time = visible - 1;
        TIMING_CHECK(row, !impl.Peek(impl.txrsp));
        impl.time = visible;
        const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, response && response->opcode == opcode);
        return *response;
    }

    Request PopOnlyRequest(const Row& row, Core::Impl& impl, Time visible, uint16_t opcode)
    {
        TIMING_CHECK(row, !impl.txreq.empty());
        ExpectTime(row, impl.txreq.front().readyTime, visible, "backend request visibility");
        impl.time = visible - 1;
        TIMING_CHECK(row, !impl.Peek(impl.txreq));
        impl.time = visible;
        const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        TIMING_CHECK(row, request && request->opcode == opcode);
        return *request;
    }

    Data PopOnlyData(const Row& row, Core::Impl& impl, Time visible, uint16_t opcode,
                     bool downstream = false)
    {
        TIMING_CHECK(row, !impl.txdat.empty());
        ExpectTime(row, impl.txdat.front().readyTime, visible, "data visibility");
        impl.time = visible - 1;
        TIMING_CHECK(row, !impl.Peek(impl.txdat, downstream));
        impl.time = visible;
        const auto data = impl.Pop(impl.txdat, EventKind::EmittedTXDAT, downstream);
        TIMING_CHECK(row, data && data->opcode == opcode);
        return *data;
    }

}

LUNATIC_TEST(Stage10MatrixC01EvictResponseAndStateTiming)
{
    for (const ProfileKind profile : Profiles)
        for (const bool requesterValid : { false, true })
            for (const bool homeDirty : { false, true })
            {
                const Row row{ profile, "C01" };
                Options options = OperationOptions(profile);
                options.timing.normalResponse = 3;
                options.timing.output = 2;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                Transaction& transaction = AddTransaction(row, driver,
                    Timing::MakeRequest(CHI::Eb::Opcodes::REQ::Evict));
                const LineKey line = transaction.request.Line();
                const LineData data = OperationData(11);
                SeedLookup(impl, transaction, true, homeDirty, data);
                impl.Bank(line).snoopFilter.Track(line, 4,
                    requesterValid ? CacheState::UC : CacheState::I, 0);

                impl.time = 10;
                impl.ActionCoherent(transaction);
                TIMING_CHECK(row, impl.txsnp.empty() && impl.txreq.empty());
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
                TIMING_CHECK(row, impl.flows.at(transaction.handle).responseOwned);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
                TIMING_CHECK(row, impl.Bank(line).directory.Find(line)->dirty == homeDirty);
                const Response comp = PopOnlyResponse(row, impl, 15, CHI::Eb::Opcodes::RSP::Comp);
                TIMING_CHECK(row, comp.resp == CHI::Eb::Resps::Comp_I);
                TIMING_CHECK(row, transaction.phase == Phase::Commit);
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
                RetireChecked(row, impl, transaction.handle);
            }
}

LUNATIC_TEST(Stage10MatrixC02CleanSharedSnoopAndWriteJoin)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C02" };
        {
            Options options = OperationOptions(profile);
            options.policy = PolicyKind::Inclusive;
            options.timing.snoopDecision = 2;
            options.timing.normalResponse = 5;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& transaction = AddTransaction(row, driver,
                Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanShared));
            const LineKey line = transaction.request.Line();
            SeedLookup(impl, transaction, true, false, OperationData(17));
            impl.Bank(line).snoopFilter.Track(line, 4, CacheState::SC, 0);
            impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UC, 0);
            impl.time = 10;
            impl.PlanCMOSnoops(transaction);
            ExpectTime(row, transaction.readyTime, 12, "clean-peer snoop decision");
            TIMING_CHECK(row, transaction.snoopTargets.empty());
            impl.time = 12;
            impl.ActionCoherent(transaction);
            TIMING_CHECK(row, impl.txsnp.empty() && impl.txreq.empty());
            const Response comp = PopOnlyResponse(row, impl, 18, CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, comp.resp == CHI::Eb::Resps::Comp_SC);
            TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::UC);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }

        for (const bool homeDirty : { false, true })
            for (const Time responseTime : { Time(17), Time(20) })
            {
                Options options = OperationOptions(profile);
                options.policy = PolicyKind::Inclusive;
                options.timing.snoopDecision = 2;
                options.timing.snoopIssue = 3;
                options.timing.normalResponse = 5;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                Transaction& transaction = AddTransaction(row, driver,
                    Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanShared));
                const LineKey line = transaction.request.Line();
                const LineData data = OperationData(23);
                SeedLookup(impl, transaction, true, false, data);
                impl.Bank(line).snoopFilter.Track(line, 4, CacheState::SC, 0);
                impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, 0);
                impl.time = 10;
                impl.PlanCMOSnoops(transaction);
                impl.time = 12;
                TIMING_CHECK(row, impl.EmitSnoops(transaction));
                TIMING_CHECK(row, impl.flows.at(transaction.handle).snoopWaveOwned);
                const Snoop snoop = PopOnlySnoop(row, impl, 16,
                    CHI::Eb::Opcodes::SNP::SnpCleanShared);
                impl.time = responseTime;
                TIMING_CHECK(row, impl.txrsp.empty());
                impl.ProcessSnoopDAT(transaction, SnoopData(snoop, 0, data,
                    CHI::Eb::Resps::SnpRespData_UC_PD));
                TIMING_CHECK(row, !transaction.pendingSnoops.empty());
                TIMING_CHECK(row, impl.flows.at(transaction.handle).snoopWaveOwned);
                impl.ProcessSnoopDAT(transaction, SnoopData(snoop, 1, data,
                    CHI::Eb::Resps::SnpRespData_UC_PD));
                TIMING_CHECK(row, transaction.pendingSnoops.empty());
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).snoopWaveOwned);
                impl.ActionCoherent(transaction);
                TIMING_CHECK(row, impl.flows.at(transaction.handle).directoryCommitted);
                TIMING_CHECK(row, impl.memory.Read(line) == data);
                if (homeDirty)
                    TIMING_CHECK(row, !impl.Bank(line).directory.Find(line)->dirty);
                const Response comp = PopOnlyResponse(row, impl, responseTime + 6,
                    CHI::Eb::Opcodes::RSP::Comp);
                TIMING_CHECK(row, comp.resp == CHI::Eb::Resps::Comp_SC);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::UC);
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
                RetireChecked(row, impl, transaction.handle);
            }
    }
}

LUNATIC_TEST(Stage10MatrixC03CleanInvalidTerminalStateJoin)
{
    for (const ProfileKind profile : Profiles)
        for (const bool dirty : { false, true })
        {
            const Row row{ profile, "C03" };
            Options options = OperationOptions(profile);
            options.policy = PolicyKind::Inclusive;
            options.timing.snoopDecision = 2;
            options.timing.snoopIssue = 3;
            options.timing.normalResponse = 5;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& transaction = AddTransaction(row, driver,
                Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanInvalid));
            const LineKey line = transaction.request.Line();
            const LineData data = OperationData(29);
            SeedLookup(impl, transaction, true, dirty, data);
            impl.Bank(line).snoopFilter.Track(line, 4, CacheState::SC, 0);
            impl.Bank(line).snoopFilter.Track(line, 5, dirty ? CacheState::UC : CacheState::UD, 0);
            impl.time = 10;
            impl.PlanCMOSnoops(transaction);
            impl.time = 12;
            TIMING_CHECK(row, impl.EmitSnoops(transaction));
            const Snoop snoop = PopOnlySnoop(row, impl, 16,
                CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
            impl.time = 18;
            if (dirty)
            {
                impl.ProcessSnoopDAT(transaction, SnoopData(snoop, 0, data,
                    CHI::Eb::Resps::SnpRespData_I_PD));
                TIMING_CHECK(row, !transaction.pendingSnoops.empty());
                TIMING_CHECK(row, impl.txrsp.empty());
                impl.ProcessSnoopDAT(transaction, SnoopData(snoop, 1, data,
                    CHI::Eb::Resps::SnpRespData_I_PD));
            }
            else
                impl.ProcessSnoopRSP(transaction, SnoopResponse(snoop, CHI::Eb::Resps::SnpResp_I));
            TIMING_CHECK(row, transaction.pendingSnoops.empty());
            impl.ActionCoherent(transaction);
            TIMING_CHECK(row, impl.flows.at(transaction.handle).directoryCommitted);
            TIMING_CHECK(row, !impl.Bank(line).directory.Find(line));
            TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
            TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::I);
            if (dirty)
                TIMING_CHECK(row, impl.memory.Read(line) == data);
            PopOnlyResponse(row, impl, 24, CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
}

LUNATIC_TEST(Stage10MatrixC04MakeInvalidMultiplePeersNoDataReturn)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C04" };
        Options options = OperationOptions(profile);
        options.policy = PolicyKind::Inclusive;
        options.timing.snoopDecision = 2;
        options.timing.snoopIssue = 3;
        options.timing.normalResponse = 5;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& transaction = AddTransaction(row, driver,
            Timing::MakeRequest(CHI::Eb::Opcodes::REQ::MakeInvalid));
        const LineKey line = transaction.request.Line();
        SeedLookup(impl, transaction, true, false, OperationData(31));
        impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, 0);
        impl.Bank(line).snoopFilter.Track(line, 6, CacheState::UC, 1);
        impl.time = 10;
        impl.PlanCMOSnoops(transaction);
        TIMING_CHECK(row, transaction.snoopTargets.size() == 2);
        impl.time = 12;
        TIMING_CHECK(row, impl.EmitSnoops(transaction));
        const Snoop first = PopOnlySnoop(row, impl, 16, CHI::Eb::Opcodes::SNP::SnpMakeInvalid);
        impl.time = 17;
        impl.ProcessSnoopRSP(transaction, SnoopResponse(first, CHI::Eb::Resps::SnpResp_I));
        TIMING_CHECK(row, transaction.pendingSnoops.empty());
        TIMING_CHECK(row, impl.txrsp.empty() && impl.txdat.empty() && impl.txreq.empty());
        impl.time = 17;
        TIMING_CHECK(row, impl.EmitSnoops(transaction));
        const Snoop second = PopOnlySnoop(row, impl, 21, CHI::Eb::Opcodes::SNP::SnpMakeInvalid);
        impl.time = 22;
        impl.ProcessSnoopRSP(transaction, SnoopResponse(second, CHI::Eb::Resps::SnpResp_I));
        TIMING_CHECK(row, transaction.pendingSnoops.empty());
        impl.ActionCoherent(transaction);
        TIMING_CHECK(row, !impl.Bank(line).directory.Find(line));
        TIMING_CHECK(row, impl.txdat.empty() && impl.txreq.empty());
        PopOnlyResponse(row, impl, 28, CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::I);
        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 6) == CacheState::I);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC05CleanUniqueCompAckAndErrorFallback)
{
    for (const ProfileKind profile : Profiles)
        for (const bool udpError : { false, true })
        {
            const Row row{ profile, "C05" };
            Options options = OperationOptions(profile);
            options.policy = PolicyKind::Inclusive;
            options.timing.normalResponse = 4;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanUnique);
            request.expCompAck = true;
            Transaction& transaction = AddTransaction(row, driver, request);
            const LineKey line = transaction.request.Line();
            const LineData data = OperationData(37);
            SeedLookup(impl, transaction, true, false, data);
            impl.Bank(line).snoopFilter.Track(line, 4,
                udpError ? CacheState::UDP : CacheState::UC, 0);
            impl.flows.at(transaction.handle).snoopLine = line;
            impl.time = 10;
            impl.PlanCMOSnoops(transaction);
            impl.ActionCoherent(transaction);
            TIMING_CHECK(row, impl.flows.at(transaction.handle).directoryCommitted == !udpError);
            const HomeLine* home = impl.Bank(line).directory.Find(line);
            TIMING_CHECK(row, !home || !home->dirty);
            const Response comp = PopOnlyResponse(row, impl, 15, CHI::Eb::Opcodes::RSP::Comp);
            if (udpError)
            {
                TIMING_CHECK(row, comp.respErr == CHI::Eb::RespErrs::NDERR);
                TIMING_CHECK(row, transaction.canceled && transaction.phase == Phase::Commit);
                TIMING_CHECK(row, impl.errors.size() == 1);
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
            }
            else
            {
                TIMING_CHECK(row, comp.respErr == CHI::Eb::RespErrs::OK);
                TIMING_CHECK(row, transaction.phase == Phase::WaitCompAck);
                TIMING_CHECK(row, impl.flows.at(transaction.handle).responseOwned);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::UC);
                Response ack;
                ack.source = 4;
                ack.target = 2;
                ack.txnId = transaction.dbid;
                ack.opcode = CHI::Eb::Opcodes::RSP::CompAck;
                impl.time = 18;
                impl.ProcessRSP(ack);
                TIMING_CHECK(row, transaction.Complete(COMPLETE_COMPACK));
                TIMING_CHECK(row, transaction.phase == Phase::Commit);
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
                TIMING_CHECK(row, impl.errors.empty());
            }
            RetireChecked(row, impl, transaction.handle);
        }
}

LUNATIC_TEST(Stage10MatrixC06DownstreamCMOCompletionPositions)
{
    for (const ProfileKind profile : Profiles)
        for (size_t position = 0; position < 3; ++position)
        {
            const Row row{ profile, "C06" };
            Options options = OperationOptions(profile, BackendKind::CHI);
            options.timing.backendRequest = 4;
            options.timing.normalResponse = 3;
            options.timing.output = 2;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& transaction = AddTransaction(row, driver,
                Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanShared));
            SeedLookup(impl, transaction, true, false, OperationData(41));
            impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
            impl.time = 10;
            impl.PlanCMOSnoops(transaction);
            impl.ActionCoherent(transaction);
            TIMING_CHECK(row, impl.flows.at(transaction.handle).directoryCommitted);
            TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
            TIMING_CHECK(row, impl.txrsp.empty());
            PopOnlyRequest(row, impl, 16, CHI::Eb::Opcodes::REQ::CleanShared);
            const Time responseTime = position == 0 ? 16 : position == 1 ? 17 : 20;
            if (position != 0)
            {
                impl.time = 17;
                impl.Advance(transaction);
                TIMING_CHECK(row, transaction.phase == Phase::WaitMemory);
            }
            impl.time = responseTime;
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
            TIMING_CHECK(row, impl.flows.at(transaction.handle).backendPhase == BackendPhase::Done);
            impl.ActionCoherent(transaction);
            PopOnlyResponse(row, impl, responseTime + 5, CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
}

LUNATIC_TEST(Stage10MatrixC07DownstreamCMORetryResendTiming)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C07" };
        Options options = OperationOptions(profile, BackendKind::CHI);
        options.timing.backendRequest = 2;
        options.timing.backendResend = 5;
        options.timing.normalResponse = 4;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& transaction = AddTransaction(row, driver,
            Timing::MakeRequest(CHI::Eb::Opcodes::REQ::MakeInvalid));
        SeedLookup(impl, transaction, false, false, OperationData(43));
        impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
        impl.time = 10;
        impl.PlanCMOSnoops(transaction);
        impl.ActionCoherent(transaction);
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
        PopOnlyRequest(row, impl, 13, CHI::Eb::Opcodes::REQ::MakeInvalid);
        impl.time = 14;
        TIMING_CHECK(row, impl.HandleBackendRetry(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck)));
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
        TIMING_CHECK(row, impl.backendRetries.contains(transaction.handle));
        TIMING_CHECK(row, impl.txrsp.empty());
        impl.backendCredits[{ 6, 0 }] = 1;
        impl.time = 15;
        impl.AdvanceBackendRetries();
        TIMING_CHECK(row, impl.backendRetries.empty());
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
        const Request resent = PopOnlyRequest(row, impl, 21, CHI::Eb::Opcodes::REQ::MakeInvalid);
        TIMING_CHECK(row, !resent.allowRetry);
        TIMING_CHECK(row, impl.txrsp.empty());
        impl.time = 22;
        TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
        impl.ActionCoherent(transaction);
        PopOnlyResponse(row, impl, 27, CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, impl.txreq.empty() && impl.txdat.empty());
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC08NonSeparatedPersistenceFallback)
{
    for (const ProfileKind profile : Profiles)
        for (const BackendKind backend : { BackendKind::Internal, BackendKind::CHI })
        {
            const Row row{ profile, "C08" };
            Options options = OperationOptions(profile, backend);
            options.timing.persistence = 4;
            options.timing.backendRequest = 2;
            options.timing.normalResponse = 3;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& transaction = AddTransaction(row, driver,
                Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanSharedPersist));
            SeedLookup(impl, transaction, true, false, OperationData(47));
            impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
            impl.time = 10;
            transaction.phase = Phase::Action;
            transaction.readyTime = 10;
            impl.ActionPersistence(transaction);
            Time completion = 10;
            if (backend == BackendKind::CHI)
            {
                TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
                const Request downstream = PopOnlyRequest(row, impl, 13,
                    CHI::Eb::Opcodes::REQ::CleanSharedPersist);
                TIMING_CHECK(row, downstream.pGroupId == 0);
                impl.time = 15;
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
                completion = 15;
                impl.ActionPersistence(transaction);
            }
            else
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);

            ExpectTime(row, transaction.readyTime, completion + 4, "persistence gate");
            TIMING_CHECK(row, impl.txrsp.empty());
            driver.TickTo(completion + 3);
            TIMING_CHECK(row, impl.txrsp.empty());
            driver.TickTo(completion + 4);
            const Response comp = PopOnlyResponse(row, impl, completion + 8,
                CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, comp.pGroupId == 0);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).persistSent);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
}

LUNATIC_TEST(Stage10MatrixC09CombinedPersistenceJoinAndResponse)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C09" };
        Options options = OperationOptions(profile, BackendKind::CHI);
        options.timing.persistence = 3;
        options.timing.backendRequest = 2;
        options.timing.normalResponse = 4;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        request.pGroupId = 0x66;
        Transaction& transaction = AddTransaction(row, driver, request);
        SeedLookup(impl, transaction, false, false, OperationData(53));
        impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
        impl.time = 10;
        impl.ActionPersistence(transaction);
        TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
        PopOnlyRequest(row, impl, 13, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        impl.time = 14;
        TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::CompPersist)));
        FlowState& flow = impl.flows.at(transaction.handle);
        TIMING_CHECK(row, flow.backendCompReceived && flow.backendPersistReceived);
        TIMING_CHECK(row, !flow.backendOperationOwned && impl.txrsp.empty());
        impl.ActionPersistence(transaction);
        ExpectTime(row, transaction.readyTime, 17, "combined persistence gate");
        TIMING_CHECK(row, impl.txrsp.empty());
        impl.time = 17;
        impl.ActionPersistence(transaction);
        TIMING_CHECK(row, flow.responseOwned);
        impl.time = 20;
        TIMING_CHECK(row, !impl.Peek(impl.txrsp));
        impl.time = 22;
        const Response response = PopOnlyResponse(row, impl, 22, CHI::Eb::Opcodes::RSP::CompPersist);
        TIMING_CHECK(row, response.txnId == transaction.request.txnId);
        TIMING_CHECK(row, response.pGroupId == 0x66);
        TIMING_CHECK(row, flow.compEmitted && !flow.persistSent);
        TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
        TIMING_CHECK(row, !flow.responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC10SeparatePersistenceEitherOrder)
{
    for (const ProfileKind profile : Profiles)
        for (const bool persistFirst : { false, true })
        {
            const Row row{ profile, "C10" };
            Options options = OperationOptions(profile, BackendKind::CHI);
            options.timing.persistence = 3;
            options.timing.normalResponse = 4;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
            request.pGroupId = 0x67;
            Transaction& transaction = AddTransaction(row, driver, request);
            SeedLookup(impl, transaction, false, false, OperationData(59));
            impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
            impl.time = 10;
            impl.ActionPersistence(transaction);
            PopOnlyRequest(row, impl, 11, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
            FlowState& flow = impl.flows.at(transaction.handle);
            const uint16_t first = persistFirst ? CHI::Eb::Opcodes::RSP::Persist
                                                : CHI::Eb::Opcodes::RSP::Comp;
            const uint16_t second = persistFirst ? CHI::Eb::Opcodes::RSP::Comp
                                                 : CHI::Eb::Opcodes::RSP::Persist;
            impl.time = 14;
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction, BackendResponse(transaction, first)));
            TIMING_CHECK(row, flow.backendOperationOwned);
            TIMING_CHECK(row, impl.txrsp.empty());
            impl.ActionPersistence(transaction);
            TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::Persistence));
            impl.time = 16;
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction, BackendResponse(transaction, second)));
            TIMING_CHECK(row, !flow.backendOperationOwned);
            TIMING_CHECK(row, flow.backendPersistFirst == persistFirst);
            impl.ActionPersistence(transaction);
            ExpectTime(row, transaction.readyTime, 19, "separate persistence join");
            impl.time = 19;
            impl.ActionPersistence(transaction);
            const Response upstreamFirst = PopOnlyResponse(row, impl, 24, first);
            TIMING_CHECK(row, upstreamFirst.txnId == (persistFirst ? 0 : transaction.request.txnId));
            TIMING_CHECK(row, flow.responseOwned);
            TIMING_CHECK(row, !transaction.Complete(COMPLETE_RESPONSE));
            impl.ActionPersistence(transaction);
            const Response upstreamSecond = PopOnlyResponse(row, impl, 29, second);
            TIMING_CHECK(row, upstreamSecond.txnId == (persistFirst ? transaction.request.txnId : 0));
            TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
            TIMING_CHECK(row, !flow.responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
}

LUNATIC_TEST(Stage10MatrixC11SeparateCompFirstOverride)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C11" };
        Options options = OperationOptions(profile, BackendKind::CHI);
        options.strategy.persistResponse = PersistResponseKind::SeparateCompFirst;
        options.timing.persistence = 3;
        options.timing.normalResponse = 4;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        request.pGroupId = 0x68;
        Transaction& transaction = AddTransaction(row, driver, request);
        SeedLookup(impl, transaction, false, false, OperationData(61));
        impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
        impl.time = 10;
        impl.ActionPersistence(transaction);
        PopOnlyRequest(row, impl, 11, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        impl.time = 14;
        TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::CompPersist)));
        impl.ActionPersistence(transaction);
        impl.time = 17;
        impl.ActionPersistence(transaction);
        FlowState& flow = impl.flows.at(transaction.handle);
        const Response comp = PopOnlyResponse(row, impl, 22, CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, comp.txnId == transaction.request.txnId);
        TIMING_CHECK(row, flow.responseOwned && !flow.persistEmitted);
        TIMING_CHECK(row, !transaction.Complete(COMPLETE_RESPONSE));
        impl.ActionPersistence(transaction);
        const Response persist = PopOnlyResponse(row, impl, 27, CHI::Eb::Opcodes::RSP::Persist);
        TIMING_CHECK(row, persist.txnId == 0 && persist.pGroupId == 0x68);
        TIMING_CHECK(row, flow.compEmitted && flow.persistEmitted);
        TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
        TIMING_CHECK(row, !flow.responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC12SeparatePersistFirstBackpressure)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C12" };
        Options options = OperationOptions(profile, BackendKind::CHI);
        options.strategy.persistResponse = PersistResponseKind::SeparatePersistFirst;
        options.outputQueueLimit = 2;
        options.timing.persistence = 3;
        options.timing.normalResponse = 4;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        request.pGroupId = 0x69;
        Transaction& transaction = AddTransaction(row, driver, request);
        SeedLookup(impl, transaction, false, false, OperationData(67));
        impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
        impl.time = 10;
        impl.ActionPersistence(transaction);
        PopOnlyRequest(row, impl, 11, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        impl.time = 14;
        TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::CompPersist)));
        impl.ActionPersistence(transaction);
        impl.time = 17;
        impl.ActionPersistence(transaction);
        FlowState& flow = impl.flows.at(transaction.handle);
        ExpectTime(row, impl.txrsp.front().readyTime, 22, "persist-first visibility");
        impl.ActionPersistence(transaction);
        TIMING_CHECK(row, impl.txrsp.size() == 2);
        TIMING_CHECK(row, flow.compSent && flow.persistSent);
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::Persistence), 17,
            "single persistence schedule");
        ExpectTime(row, impl.txrsp.front().readyTime, 22, "persist held before pop");
        impl.time = 25;
        const auto persistValue = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, persistValue && persistValue->opcode == CHI::Eb::Opcodes::RSP::Persist);
        const Response persist = *persistValue;
        TIMING_CHECK(row, persist.txnId == 0);
        TIMING_CHECK(row, flow.responseOwned && !flow.compEmitted);
        TIMING_CHECK(row, !transaction.Complete(COMPLETE_RESPONSE));
        ExpectTime(row, impl.txrsp.front().readyTime, 22, "comp held behind persist");
        impl.time = 28;
        const auto compValue = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, compValue && compValue->opcode == CHI::Eb::Opcodes::RSP::Comp);
        const Response comp = *compValue;
        TIMING_CHECK(row, comp.txnId == transaction.request.txnId);
        TIMING_CHECK(row, flow.compEmitted && flow.persistEmitted);
        TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
        TIMING_CHECK(row, !flow.responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC13DirectPersistCompOnlyFallback)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C13" };
        {
            Options options = OperationOptions(profile, BackendKind::CHI);
            CoreDriver driver(options);
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
            request.pGroupId = 0x6a;
            Transaction& transaction = AddTransaction(row, driver, request);
            TIMING_CHECK(row, !options.directPersistEnabled);
            TIMING_CHECK(row, !driver.impl.flows.at(transaction.handle).directPersistActive);
            RetireChecked(row, driver.impl, transaction.handle);
        }
        {
            Options options = OperationOptions(profile, BackendKind::CHI);
            options.directPersistEnabled = true;
            options.strategy.persistResponse = PersistResponseKind::SeparatePersistFirst;
            options.timing.persistence = 3;
            options.timing.normalResponse = 4;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
            request.pGroupId = 0x6a;
            Transaction& transaction = AddTransaction(row, driver, request);
            SeedLookup(impl, transaction, false, false, OperationData(71));
            impl.flows.at(transaction.handle).snoopLine = transaction.request.Line();
            FlowState& flow = impl.flows.at(transaction.handle);
            TIMING_CHECK(row, flow.directPersistActive);
            impl.time = 10;
            impl.ActionPersistence(transaction);
            const Request downstream = PopOnlyRequest(row, impl, 11,
                CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
            TIMING_CHECK(row, downstream.returnNid == transaction.request.source);
            impl.time = 14;
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            TIMING_CHECK(row, !flow.backendOperationOwned);
            impl.ActionPersistence(transaction);
            impl.time = 17;
            impl.ActionPersistence(transaction);
            const Response comp = PopOnlyResponse(row, impl, 22, CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, comp.txnId == transaction.request.txnId);
            TIMING_CHECK(row, !flow.persistSent && !flow.persistEmitted);
            TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
            TIMING_CHECK(row, !flow.responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
    }
}

LUNATIC_TEST(Stage10MatrixC14AXIPersistenceExplicitRejection)
{
    for (const ProfileKind profile : Profiles)
        for (const bool separated : { false, true })
        {
            const Row row{ profile, "C14" };
            Options options = OperationOptions(profile, BackendKind::AXI);
            options.timing.normalResponse = 3;
            options.timing.output = 2;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request request = Timing::MakeRequest(separated
                ? CHI::Eb::Opcodes::REQ::CleanSharedPersistSep
                : CHI::Eb::Opcodes::REQ::CleanSharedPersist);
            request.pGroupId = separated ? 0x6b : 0;
            Transaction& transaction = AddTransaction(row, driver, request);
            transaction.phase = Phase::Decode;
            transaction.readyTime = 10;
            impl.time = 10;
            impl.Advance(transaction);
            TIMING_CHECK(row, transaction.canceled);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
            TIMING_CHECK(row, !impl.axi.HasInflight());
            TIMING_CHECK(row, impl.txreq.empty() && impl.txdat.empty());
            TIMING_CHECK(row, impl.errors.size() == 1);
            const Response response = PopOnlyResponse(row, impl, 15, separated
                ? CHI::Eb::Opcodes::RSP::CompPersist : CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, response.respErr == CHI::Eb::RespErrs::NDERR);
            TIMING_CHECK(row, response.pGroupId == request.pGroupId);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendCompReceived);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendPersistReceived);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).persistSent);
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
}

namespace {

    Response PopHeldResponse(const Row& row, Core::Impl& impl, Time visible, Time popped,
                             uint16_t opcode)
    {
        TIMING_CHECK(row, !impl.txrsp.empty());
        ExpectTime(row, impl.txrsp.front().readyTime, visible, "held response visibility");
        impl.time = popped;
        TIMING_CHECK(row, impl.Peek(impl.txrsp));
        const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, response && response->opcode == opcode);
        return *response;
    }

    Data PopHeldData(const Row& row, Core::Impl& impl, Time visible, Time popped, uint16_t opcode)
    {
        TIMING_CHECK(row, !impl.txdat.empty());
        ExpectTime(row, impl.txdat.front().readyTime, visible, "held data visibility");
        impl.time = popped;
        TIMING_CHECK(row, impl.Peek(impl.txdat));
        const auto data = impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
        TIMING_CHECK(row, data && data->opcode == opcode);
        return *data;
    }

    Request StashRequest(uint16_t opcode, bool targetPresent)
    {
        Request request = Timing::MakeRequest(opcode);
        request.returnNid = targetPresent ? 5 : request.source;
        request.stashNidValid = true;
        request.stashLpidValid = true;
        request.stashLpid = 3;
        request.stashGroupId = 0x72;
        return request;
    }

}

LUNATIC_TEST(Stage10MatrixC15NonSeparatedStashTargetTiming)
{
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::StashOnceUnique,
                 CHI::Eb::Opcodes::REQ::StashOnceShared })
            for (const bool targetPresent : { false, true })
            {
                const Row row{ profile, "C15" };
                Options options = OperationOptions(profile);
                options.timing.snoopDecision = 2;
                options.timing.snoopIssue = 3;
                options.timing.normalResponse = 4;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                Transaction& transaction = AddTransaction(row, driver,
                    StashRequest(opcode, targetPresent));
                const LineKey line = transaction.request.Line();
                const CacheState initial = opcode == CHI::Eb::Opcodes::REQ::StashOnceUnique
                    ? CacheState::UC : CacheState::SC;
                if (targetPresent)
                    impl.Bank(line).snoopFilter.Track(line, 5, initial, 0);
                impl.time = 10;
                impl.ResolveStash(transaction);
                ExpectTime(row, transaction.readyTime, 12, "stash snoop decision");
                TIMING_CHECK(row, transaction.snoopsRequired == targetPresent);
                impl.time = 12;
                Time completion = 12;
                if (targetPresent)
                {
                    TIMING_CHECK(row, impl.EmitSnoops(transaction));
                    const uint16_t snoopOpcode = opcode == CHI::Eb::Opcodes::REQ::StashOnceUnique
                        ? CHI::Eb::Opcodes::SNP::SnpStashUnique : CHI::Eb::Opcodes::SNP::SnpStashShared;
                    const Snoop snoop = PopOnlySnoop(row, impl, 16, snoopOpcode);
                    TIMING_CHECK(row, snoop.stashLpidValid && snoop.stashLpid == 3);
                    impl.time = 18;
                    impl.ProcessSnoopRSP(transaction, SnoopResponse(snoop,
                        initial == CacheState::UC ? CHI::Eb::Resps::SnpResp_UC
                                                  : CHI::Eb::Resps::SnpResp_SC));
                    completion = 18;
                    TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == initial);
                }
                impl.time = completion;
                impl.ActionCoherent(transaction);
                PopOnlyResponse(row, impl, completion + 5, CHI::Eb::Opcodes::RSP::Comp);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
                TIMING_CHECK(row, !impl.flows.at(transaction.handle).snoopWaveOwned);
                RetireChecked(row, impl, transaction.handle);
            }
}

LUNATIC_TEST(Stage10MatrixC16SeparatedStashFormsAndBackpressure)
{
    for (const ProfileKind profile : Profiles)
        for (const StashResponseKind strategy : { StashResponseKind::Rotate,
                 StashResponseKind::CompFirst, StashResponseKind::StashDoneFirst,
                 StashResponseKind::Combined })
        {
            const Row row{ profile, "C16" };
            Options options = OperationOptions(profile);
            options.strategy.stashResponse = strategy;
            options.outputQueueLimit = 2;
            options.timing.snoopDecision = 1;
            options.timing.normalResponse = 4;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& transaction = AddTransaction(row, driver,
                StashRequest(CHI::Eb::Opcodes::REQ::StashOnceSepUnique, false));
            impl.time = 10;
            impl.ResolveStash(transaction);
            impl.time = 11;
            impl.ActionCoherent(transaction);
            FlowState& flow = impl.flows.at(transaction.handle);
            const bool combined = strategy == StashResponseKind::Combined;
            const bool doneFirst = strategy == StashResponseKind::StashDoneFirst;
            TIMING_CHECK(row, impl.txrsp.size() == (combined ? 1 : 2));
            TIMING_CHECK(row, flow.responseOwned);
            const uint16_t firstOpcode = combined ? CHI::Eb::Opcodes::RSP::CompStashDone
                : doneFirst ? CHI::Eb::Opcodes::RSP::StashDone : CHI::Eb::Opcodes::RSP::Comp;
            const Response first = PopHeldResponse(row, impl, 16, 19, firstOpcode);
            TIMING_CHECK(row, first.stashGroupId == (combined || doneFirst ? 0x72 : 0));
            if (combined)
            {
                TIMING_CHECK(row, first.txnId == transaction.request.txnId);
                TIMING_CHECK(row, flow.compEmitted && flow.stashDoneEmitted);
                TIMING_CHECK(row, !flow.responseOwned);
            }
            else
            {
                TIMING_CHECK(row, first.txnId == (doneFirst ? 0 : transaction.request.txnId));
                TIMING_CHECK(row, flow.responseOwned);
                TIMING_CHECK(row, !transaction.Complete(COMPLETE_RESPONSE));
                const uint16_t secondOpcode = doneFirst ? CHI::Eb::Opcodes::RSP::Comp
                                                        : CHI::Eb::Opcodes::RSP::StashDone;
                const Response second = PopHeldResponse(row, impl, 16, 22, secondOpcode);
                TIMING_CHECK(row, second.txnId == (doneFirst ? transaction.request.txnId : 0));
                TIMING_CHECK(row, secondOpcode == CHI::Eb::Opcodes::RSP::StashDone
                    ? second.stashGroupId == 0x72 : true);
                TIMING_CHECK(row, flow.compEmitted && flow.stashDoneEmitted);
                TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
                TIMING_CHECK(row, !flow.responseOwned);
            }
            RetireChecked(row, impl, transaction.handle);
        }

    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C16" };
        Options options = OperationOptions(profile);
        options.strategy.stashResponse = StashResponseKind::Combined;
        options.timing.normalResponse = 4;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Transaction& transaction = AddTransaction(row, driver,
            StashRequest(CHI::Eb::Opcodes::REQ::StashOnceShared, false));
        impl.time = 10;
        impl.ResolveStash(transaction);
        impl.ActionCoherent(transaction);
        TIMING_CHECK(row, impl.txrsp.size() == 1);
        const Response comp = PopOnlyResponse(row, impl, 15, CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, comp.txnId == transaction.request.txnId);
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).stashDoneSent);
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC17StashDataAndDataPullFallback)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C17" };
        {
            Options options = OperationOptions(profile);
            options.timing.snoopIssue = 2;
            options.timing.normalResponse = 3;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& transaction = AddTransaction(row, driver,
                StashRequest(CHI::Eb::Opcodes::REQ::StashOnceUnique, true));
            impl.time = 10;
            impl.ResolveStash(transaction);
            TIMING_CHECK(row, impl.EmitSnoops(transaction));
            const Snoop snoop = PopOnlySnoop(row, impl, 13,
                CHI::Eb::Opcodes::SNP::SnpStashUnique);
            impl.time = 15;
            impl.ProcessSnoopRSP(transaction, SnoopResponse(snoop, CHI::Eb::Resps::SnpResp_I, 1));
            TIMING_CHECK(row, transaction.pendingSnoops.empty());
            TIMING_CHECK(row, impl.errors.size() == 1);
            impl.ActionCoherent(transaction);
            PopOnlyResponse(row, impl, 19, CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
            TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
        {
            Options options = OperationOptions(profile);
            options.timing.snoopDecision = 1;
            options.timing.snoopIssue = 2;
            options.timing.fastResponse = 2;
            options.timing.normalResponse = 3;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::WriteUniquePtlStash);
            request.returnNid = 5;
            request.stashNidValid = true;
            request.stashLpidValid = true;
            request.stashLpid = 3;
            Transaction& transaction = AddTransaction(row, driver, request);
            const LineKey line = transaction.request.Line();
            const LineData initial = OperationData(73);
            const LineData update = OperationData(131);
            SeedLookup(impl, transaction, false, false, initial);
            impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, 0);
            impl.time = 10;
            impl.ResolveImmediateWrite(transaction);
            impl.time = 11;
            TIMING_CHECK(row, impl.EmitSnoops(transaction));
            const Snoop snoop = PopOnlySnoop(row, impl, 14,
                CHI::Eb::Opcodes::SNP::SnpUniqueStash);
            impl.time = 15;
            impl.ProcessSnoopDAT(transaction, SnoopData(snoop, 0, initial,
                CHI::Eb::Resps::SnpRespData_I_PD));
            TIMING_CHECK(row, !transaction.pendingSnoops.empty());
            TIMING_CHECK(row, impl.txrsp.empty());
            impl.ProcessSnoopDAT(transaction, SnoopData(snoop, 1, initial,
                CHI::Eb::Resps::SnpRespData_I_PD));
            FlowState& flow = impl.flows.at(transaction.handle);
            TIMING_CHECK(row, flow.snoopDataValid && flow.dataDirty);
            impl.ActionCoherent(transaction);
            TIMING_CHECK(row, flow.responseOwned);
            PopOnlyResponse(row, impl, 18, CHI::Eb::Opcodes::RSP::DBIDResp);
            TIMING_CHECK(row, flow.responseOwned && !flow.directoryCommitted);

            ByteEnables enables{};
            for (size_t index = 0; index < enables.size(); ++index)
                enables[index] = index >= 7 && index < 51;
            impl.time = 19;
            for (size_t beat = 0; beat < 2; ++beat)
            {
                Data operand;
                operand.source = 4;
                operand.target = 2;
                operand.txnId = transaction.dbid;
                operand.opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
                operand.dataId = beat * 2;
                operand.ccid = (transaction.request.address >> 4) & 3;
                for (size_t index = 0; index < 32; ++index)
                {
                    const size_t lineOffset = beat * 32 + index;
                    operand.bytes[index] = update[lineOffset];
                    operand.byteEnables[index] = enables[lineOffset];
                }
                TIMING_CHECK(row, impl.HandleWriteDAT(transaction, operand));
            }
            TIMING_CHECK(row, transaction.Complete(COMPLETE_WRITE_DATA));
            impl.ActionCoherent(transaction);
            TIMING_CHECK(row, flow.directoryCommitted);
            LineData expected = initial;
            for (size_t index = 0; index < expected.size(); ++index)
                if (enables[index])
                    expected[index] = update[index];
            CheckLine(row, impl, line, expected);
            PopOnlyResponse(row, impl, 19, CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
            TIMING_CHECK(row, !flow.responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
    }
}

namespace {

    Options AtomicOptions(ProfileKind profile, bool local, BackendKind backend = BackendKind::CHI)
    {
        Options options = OperationOptions(profile, backend);
        const bool packagedLocal = profile == ProfileKind::Type7;
        if (local != packagedLocal)
            options.strategy.atomicExecution = local ? AtomicExecutionKind::Local
                                                     : AtomicExecutionKind::Backend;
        options.timing.fastResponse = 2;
        options.timing.normalResponse = 4;
        options.timing.backendRequest = 3;
        options.timing.backendData = 2;
        options.timing.backendResend = 5;
        options.timing.dataReturn = 2;
        options.timing.output = 1;
        return options;
    }

    Transaction& PrepareAtomic(const Row& row, CoreDriver& driver, uint16_t opcode, uint8_t size,
                               uint64_t address, const LineData& initial, bool memoryReady)
    {
        Request request = Timing::MakeRequest(opcode, 4, 1, address);
        request.size = size;
        Transaction& transaction = AddTransaction(row, driver, request);
        if (memoryReady)
        {
            transaction.data = initial;
            transaction.Mark(COMPLETE_MEMORY);
        }
        driver.impl.time = 10;
        driver.impl.ResolveAtomic(transaction);
        TIMING_CHECK(row, transaction.phase == Phase::Action);
        TIMING_CHECK(row, driver.impl.flows.at(transaction.handle).expectedBeatMask ==
            AtomicOperandBeatMask(transaction.request, driver.impl.options));
        TIMING_CHECK(row, driver.impl.flows.at(transaction.handle).atomic.resultBeatMask ==
            AtomicResultBeatMask(transaction.request, driver.impl.options));
        driver.impl.ActionAtomic(transaction);
        PopOnlyResponse(row, driver.impl, 13, CHI::Eb::Opcodes::RSP::DBIDResp);
        TIMING_CHECK(row, driver.impl.flows.at(transaction.handle).responseOwned);
        return transaction;
    }

    Data AtomicOperand(const Transaction& transaction, const Options& options,
                       const LineData& operands, size_t beat)
    {
        const size_t bytes = BytesPerBeat(options);
        AtomicGeometry geometry;
        LUNATIC_CHECK(GetAtomicGeometry(transaction.request, geometry));
        Data data;
        data.source = transaction.request.source;
        data.target = 2;
        data.txnId = transaction.dbid;
        data.opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.dataId = beat * bytes / 16;
        data.ccid = (transaction.request.address >> 4) & 3;
        const size_t offset = beat * bytes;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            const bool enabled = lineOffset >= geometry.windowOffset &&
                lineOffset < geometry.windowOffset + geometry.windowBytes;
            if (enabled)
            {
                data.bytes[index] = operands[lineOffset];
                data.byteEnables[index] = 1;
            }
        }
        return data;
    }

    void AcceptAtomicOperands(const Row& row, Core::Impl& impl, Transaction& transaction,
                              Time accepted, const LineData& operands, bool reverse = false)
    {
        const uint64_t mask = impl.flows.at(transaction.handle).expectedBeatMask;
        std::vector<size_t> beats;
        for (size_t beat = 0; beat < BeatsPerLine(impl.options); ++beat)
            if (mask & (uint64_t(1) << beat))
                beats.push_back(beat);
        if (reverse)
            std::reverse(beats.begin(), beats.end());
        impl.time = accepted;
        for (const size_t beat : beats)
            TIMING_CHECK(row, impl.HandleAtomicDAT(transaction,
                AtomicOperand(transaction, impl.options, operands, beat)));
        TIMING_CHECK(row, transaction.Complete(COMPLETE_WRITE_DATA));
        TIMING_CHECK(row, (transaction.receivedBeats & mask) == mask);
    }

    Data AtomicBackendResult(const Transaction& transaction, const Options& options,
                             const LineData& oldData)
    {
        const size_t bytes = BytesPerBeat(options);
        const size_t address = transaction.request.address % LINE_SIZE;
        const size_t offset = address / bytes * bytes;
        AtomicGeometry geometry;
        LUNATIC_CHECK(GetAtomicGeometry(transaction.request, geometry));
        Data data;
        data.source = 6;
        data.target = 2;
        data.homeNid = 2;
        data.txnId = transaction.handle;
        data.dbid = 77;
        data.opcode = CHI::Eb::Opcodes::DAT::CompData;
        data.resp = CHI::Eb::Resps::CompData_I;
        data.dataId = offset / 16;
        data.ccid = (transaction.request.address >> 4) & 3;
        for (size_t index = 0; index < geometry.operandBytes; ++index)
        {
            const size_t packetIndex = address - offset + index;
            data.bytes[packetIndex] = oldData[address + index];
            data.byteEnables[packetIndex] = 1;
        }
        return data;
    }

    void CheckAtomicResult(const Row& row, const Transaction& transaction, const Options& options,
                           const Data& result, const LineData& oldData, Time visible)
    {
        const size_t bytes = BytesPerBeat(options);
        const size_t address = transaction.request.address % LINE_SIZE;
        const size_t offset = address / bytes * bytes;
        AtomicGeometry geometry;
        LUNATIC_CHECK(GetAtomicGeometry(transaction.request, geometry));
        TIMING_CHECK(row, result.txnId == transaction.request.txnId);
        TIMING_CHECK(row, result.dbid == transaction.dbid);
        TIMING_CHECK(row, result.dataId == offset / 16);
        TIMING_CHECK(row, result.ccid == ((transaction.request.address >> 4) & 3));
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            const bool enabled = lineOffset >= address && lineOffset < address + geometry.operandBytes;
            TIMING_CHECK(row, bool(result.byteEnables[index]) == enabled);
            if (enabled)
                TIMING_CHECK(row, result.bytes[index] == oldData[lineOffset]);
        }
        (void)visible;
    }

    void RunAtomicStoreRoute(const Row& row, ProfileKind profile, bool local, Time operandTime)
    {
        Options options = AtomicOptions(profile, local);
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const uint64_t address = 0x100020;
        LineData initial{};
        LineData operands{};
        SetInteger(initial, address % LINE_SIZE, 4, 5);
        SetInteger(operands, address % LINE_SIZE, 4, 7);
        Transaction& transaction = PrepareAtomic(row, driver, CHI::Eb::Opcodes::REQ::AtomicStore::ADD,
            2, address, initial, local);
        FlowState& flow = impl.flows.at(transaction.handle);
        if (operandTime <= 15)
        {
            AcceptAtomicOperands(row, impl, transaction, operandTime, operands);
            impl.ActionAtomic(transaction);
            PopOnlyResponse(row, impl, 15, CHI::Eb::Opcodes::RSP::Comp);
        }
        else
        {
            PopOnlyResponse(row, impl, 15, CHI::Eb::Opcodes::RSP::Comp);
            AcceptAtomicOperands(row, impl, transaction, operandTime, operands);
            impl.ActionAtomic(transaction);
        }
        TIMING_CHECK(row, flow.responseOwned != local);
        if (local)
        {
            TIMING_CHECK(row, flow.directoryCommitted);
            TIMING_CHECK(row, !flow.backendOperationOwned && impl.txreq.empty() && impl.txdat.empty());
            ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::AtomicExecution), operandTime,
                "local store execution");
            const AtomicExecution expected = ExecuteAtomic(transaction.request, initial, operands);
            TIMING_CHECK(row, expected.write);
            CheckLine(row, impl, transaction.request.Line(), expected.data);
            TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
            TIMING_CHECK(row, !flow.responseOwned);
        }
        else
        {
            TIMING_CHECK(row, !flow.directoryCommitted);
            TIMING_CHECK(row, flow.backendOperationOwned);
            TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::AtomicExecution));
            const Time requestVisible = operandTime + 4;
            PopOnlyRequest(row, impl, requestVisible, CHI::Eb::Opcodes::REQ::AtomicStore::ADD);
            impl.time = requestVisible + 1;
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            TIMING_CHECK(row, flow.backendCompReceived && flow.backendOperationOwned);
            impl.ActionAtomic(transaction);
            const Time dataVisible = requestVisible + 4;
            const Data operand = PopOnlyData(row, impl, dataVisible,
                CHI::Eb::Opcodes::DAT::NonCopyBackWrData, true);
            TIMING_CHECK(row, operand.txnId == 77);
            TIMING_CHECK(row, !flow.backendOperationOwned);
            TIMING_CHECK(row, flow.backendDataEmitted);
            impl.ActionAtomic(transaction);
            TIMING_CHECK(row, flow.directoryCommitted);
            TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
            TIMING_CHECK(row, !flow.responseOwned);
            TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::AtomicExecution));
        }
        RetireChecked(row, impl, transaction.handle);
    }

    void RunAtomicResultRoute(const Row& row, ProfileKind profile, uint16_t opcode, uint8_t size,
                              bool local)
    {
        Options options = AtomicOptions(profile, local);
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const uint64_t address = 0x100020;
        const size_t operandBytes = size_t(1) << size;
        LineData initial{};
        LineData operands{};
        SetInteger(initial, address % LINE_SIZE, operandBytes, 5);
        SetInteger(operands, address % LINE_SIZE, operandBytes, 7);
        Transaction& transaction = PrepareAtomic(row, driver, opcode, size, address, initial, local);
        FlowState& flow = impl.flows.at(transaction.handle);
        AcceptAtomicOperands(row, impl, transaction, 15, operands);
        impl.ActionAtomic(transaction);
        Time dataDone = 17 + options.dataLatency;
        if (local)
        {
            TIMING_CHECK(row, flow.directoryCommitted);
            TIMING_CHECK(row, !flow.backendOperationOwned && impl.txreq.empty());
            ExpectTime(row, transaction.readyTime, dataDone, "local atomic result data");
            const AtomicExecution expected = ExecuteAtomic(transaction.request, initial, operands);
            TIMING_CHECK(row, expected.write);
            CheckLine(row, impl, transaction.request.Line(), expected.data);
        }
        else
        {
            TIMING_CHECK(row, flow.backendOperationOwned && !flow.directoryCommitted);
            PopOnlyRequest(row, impl, 19, opcode);
            impl.time = 20;
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
            impl.ActionAtomic(transaction);
            TIMING_CHECK(row, impl.txdat.size() == 1);
            impl.time = 21;
            TIMING_CHECK(row, impl.HandleBackendDAT(transaction,
                AtomicBackendResult(transaction, options, initial)));
            TIMING_CHECK(row, flow.atomic.backendResultReceived && flow.backendOperationOwned);
            PopOnlyData(row, impl, 23, CHI::Eb::Opcodes::DAT::NonCopyBackWrData, true);
            TIMING_CHECK(row, !flow.backendOperationOwned);
            impl.ActionAtomic(transaction);
            dataDone = 25 + options.dataLatency;
            ExpectTime(row, transaction.readyTime, dataDone, "remote atomic result data");
            TIMING_CHECK(row, flow.directoryCommitted);
            TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::AtomicExecution));
        }
        impl.time = dataDone;
        impl.ReturnAtomic(transaction);
        const Time resultVisible = dataDone + 3;
        const Data result = PopOnlyData(row, impl, resultVisible, CHI::Eb::Opcodes::DAT::CompData);
        CheckAtomicResult(row, transaction, options, result, initial, resultVisible);
        TIMING_CHECK(row, flow.atomic.returnEmitted);
        TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
        TIMING_CHECK(row, !flow.responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }

}

LUNATIC_TEST(Stage10MatrixC18AtomicStoreLocalRemoteAndCancellation)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C18" };
        const Options packaged = ProfileOptions(profile);
        TIMING_CHECK(row, packaged.strategy.atomicExecution == (profile == ProfileKind::Type3
            ? AtomicExecutionKind::Backend : AtomicExecutionKind::Local));
        for (const bool local : { false, true })
            for (const Time operandTime : { Time(13), Time(15), Time(19) })
                RunAtomicStoreRoute(row, profile, local, operandTime);

        Options options = AtomicOptions(profile, true);
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const LineData initial = OperationData(79);
        Transaction& transaction = PrepareAtomic(row, driver, CHI::Eb::Opcodes::REQ::AtomicStore::ADD,
            2, 0x100020, initial, true);
        Data cancel;
        cancel.source = 4;
        cancel.target = 2;
        cancel.txnId = transaction.dbid;
        cancel.opcode = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        cancel.ccid = (transaction.request.address >> 4) & 3;
        impl.time = 13;
        TIMING_CHECK(row, impl.HandleWriteDAT(transaction, cancel));
        TIMING_CHECK(row, transaction.canceled && transaction.Complete(COMPLETE_WRITE_DATA));
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).directoryCommitted);
        PopOnlyResponse(row, impl, 15, CHI::Eb::Opcodes::RSP::Comp);
        TIMING_CHECK(row, impl.txreq.empty() && impl.txdat.empty());
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC19AtomicLoadSwapSizesAndResultTiming)
{
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::AtomicLoad::ADD,
                 CHI::Eb::Opcodes::REQ::AtomicSwap })
        {
            const Row row{ profile, "C19" };
            for (uint8_t size = 0; size <= 3; ++size)
                for (const bool local : { false, true })
                    RunAtomicResultRoute(row, profile, opcode, size, local);
        }
}

LUNATIC_TEST(Stage10MatrixC20AtomicCompareSuccessFailureSchedule)
{
    for (const ProfileKind profile : Profiles)
        for (const bool success : { false, true })
        {
            const Row row{ profile, "C20" };
            Options options = AtomicOptions(profile, true);
            options.timing.atomicExecution = 2;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const uint64_t address = 0x100038;
            LineData initial{};
            LineData operands{};
            SetInteger(initial, address % LINE_SIZE, 8, 5);
            SetInteger(operands, address % LINE_SIZE, 8, success ? 5 : 7);
            SetInteger(operands, (address ^ 8) % LINE_SIZE, 8, 9);
            Transaction& transaction = PrepareAtomic(row, driver,
                CHI::Eb::Opcodes::REQ::AtomicCompare, 4, address, initial, true);
            impl.memory.WriteFull(transaction.request.Line(), initial);
            FlowState& flow = impl.flows.at(transaction.handle);
            AcceptAtomicOperands(row, impl, transaction, 15, operands);
            impl.ActionAtomic(transaction);
            ExpectTime(row, transaction.readyTime, 17, "compare execution gate");
            TIMING_CHECK(row, !flow.directoryCommitted && impl.txdat.empty());
            driver.TickTo(16);
            TIMING_CHECK(row, !flow.directoryCommitted);
            driver.TickTo(17);
            TIMING_CHECK(row, flow.directoryCommitted);
            TIMING_CHECK(row, flow.atomic.execution.write == success);
            const AtomicExecution expected = ExecuteAtomic(transaction.request, initial, operands);
            TIMING_CHECK(row, expected.write == success);
            CheckLine(row, impl, transaction.request.Line(), expected.data);
            ExpectTime(row, transaction.readyTime, 19 + options.dataLatency, "compare result data");
            driver.TickTo(19 + options.dataLatency);
            const Time resultVisible = 22 + options.dataLatency;
            const Data result = PopHeldData(row, impl, resultVisible, success ? resultVisible : resultVisible + 3,
                CHI::Eb::Opcodes::DAT::CompData);
            CheckAtomicResult(row, transaction, options, result, initial, resultVisible);
            ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::AtomicExecution), 17,
                "compare execution applied once");
            TIMING_CHECK(row, !flow.responseOwned);
            RetireChecked(row, impl, transaction.handle);
        }
}

LUNATIC_TEST(Stage10MatrixC21MultiBeatAtomicOperandPlacement)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C21" };
        Options options = AtomicOptions(profile, true);
        options.dataWidth = 128;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const uint64_t address = 0x100030;
        LineData initial{};
        LineData operands{};
        SetInteger(initial, 0x30, 16, 5);
        SetInteger(operands, 0x30, 16, 5);
        SetInteger(operands, 0x20, 16, 9);
        Transaction& transaction = PrepareAtomic(row, driver,
            CHI::Eb::Opcodes::REQ::AtomicCompare, 5, address, initial, true);
        FlowState& flow = impl.flows.at(transaction.handle);
        TIMING_CHECK(row, flow.expectedBeatMask == 0b1100);
        TIMING_CHECK(row, flow.atomic.resultBeatMask == 0b1000);
        impl.time = 13;
        TIMING_CHECK(row, !impl.HandleAtomicDAT(transaction,
            AtomicOperand(transaction, options, operands, 0)));
        TIMING_CHECK(row, transaction.receivedBeats == 0);
        TIMING_CHECK(row, impl.HandleAtomicDAT(transaction,
            AtomicOperand(transaction, options, operands, 2)));
        TIMING_CHECK(row, transaction.receivedBeats == 0b0100);
        TIMING_CHECK(row, !transaction.Complete(COMPLETE_WRITE_DATA));
        TIMING_CHECK(row, !flow.directoryCommitted && impl.txdat.empty());
        TIMING_CHECK(row, impl.HandleAtomicDAT(transaction,
            AtomicOperand(transaction, options, operands, 3)));
        TIMING_CHECK(row, transaction.receivedBeats == 0b1100);
        TIMING_CHECK(row, transaction.Complete(COMPLETE_WRITE_DATA));
        impl.ActionAtomic(transaction);
        TIMING_CHECK(row, flow.directoryCommitted);
        ExpectTime(row, transaction.readyTime, 15 + options.dataLatency, "multi-beat operand result data");
        impl.time = 23;
        impl.ReturnAtomic(transaction);
        const Data result = PopOnlyData(row, impl, 26, CHI::Eb::Opcodes::DAT::CompData);
        CheckAtomicResult(row, transaction, options, result, initial, 26);
        TIMING_CHECK(row, result.dataId == 3);
        for (size_t index = 0; index < 16; ++index)
            TIMING_CHECK(row, result.byteEnables[index]);
        TIMING_CHECK(row, !flow.responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC22LocalAtomicExecutionDelayAppliedOnce)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C22" };
        Options options = AtomicOptions(profile, true);
        options.timing.snoopDecision = 1;
        options.timing.snoopIssue = 2;
        options.timing.atomicExecution = 5;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const uint64_t address = 0x100020;
        Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 1, address);
        request.size = 3;
        Transaction& transaction = AddTransaction(row, driver, request);
        LineData initial{};
        LineData operands{};
        SetInteger(initial, address % LINE_SIZE, 8, 5);
        SetInteger(operands, address % LINE_SIZE, 8, 7);
        SeedLookup(impl, transaction, true, false, initial);
        transaction.Mark(COMPLETE_MEMORY);
        impl.Bank(transaction.request.Line()).snoopFilter.Track(transaction.request.Line(), 5,
            CacheState::UC, 0);
        impl.time = 10;
        impl.ResolveAtomic(transaction);
        ExpectTime(row, transaction.readyTime, 11, "atomic snoop decision");
        impl.time = 11;
        impl.ActionCoherent(transaction);
        const Snoop snoop = PopOnlySnoop(row, impl, 14, CHI::Eb::Opcodes::SNP::SnpUnique);
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).scheduleMarks.contains(
            SchedulePoint::AtomicExecution));
        impl.time = 15;
        impl.ProcessSnoopRSP(transaction, SnoopResponse(snoop, CHI::Eb::Resps::SnpResp_I));
        impl.ActionCoherent(transaction);
        PopOnlyResponse(row, impl, 18, CHI::Eb::Opcodes::RSP::DBIDResp);
        AcceptAtomicOperands(row, impl, transaction, 22, operands);
        FlowState& flow = impl.flows.at(transaction.handle);
        TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::AtomicExecution));
        impl.ActionAtomic(transaction);
        ExpectTime(row, transaction.readyTime, 27, "atomic execution delay");
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::AtomicExecution), 27,
            "atomic execution mark");
        TIMING_CHECK(row, !flow.directoryCommitted);
        driver.TickTo(26);
        TIMING_CHECK(row, !flow.directoryCommitted && impl.txdat.empty());
        driver.TickTo(27);
        TIMING_CHECK(row, flow.directoryCommitted);
        ExpectTime(row, transaction.readyTime, 29 + options.dataLatency, "atomic result after execution");
        driver.TickTo(29 + options.dataLatency);
        TIMING_CHECK(row, impl.txdat.size() == 1);
        const Time resultVisible = transaction.readyTime + 3;
        ExpectTime(row, impl.txdat.front().readyTime, resultVisible, "held atomic result visibility");
        impl.time = resultVisible;
        TIMING_CHECK(row, impl.Peek(impl.txdat));
        impl.time = resultVisible + 1;
        impl.ReturnAtomic(transaction);
        TIMING_CHECK(row, impl.txdat.size() == 1);
        impl.time = resultVisible + 3;
        impl.ReturnAtomic(transaction);
        TIMING_CHECK(row, impl.txdat.size() == 1);
        ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::AtomicExecution), 27,
            "atomic execution not reapplied");
        const Data result = PopHeldData(row, impl, resultVisible, resultVisible + 3,
            CHI::Eb::Opcodes::DAT::CompData);
        CheckAtomicResult(row, transaction, options, result, initial, resultVisible);
        TIMING_CHECK(row, flow.atomic.returnEmitted);
        TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
        TIMING_CHECK(row, !flow.responseOwned);
        const AtomicExecution expected = ExecuteAtomic(transaction.request, initial, operands);
        CheckLine(row, impl, transaction.request.Line(), expected.data);
        RetireChecked(row, impl, transaction.handle);
    }
}

LUNATIC_TEST(Stage10MatrixC23RemoteAtomicRetryAndEarlyResult)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "C23" };
        Options options = AtomicOptions(profile, false);
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const uint64_t address = 0x100020;
        LineData initial{};
        LineData operands{};
        SetInteger(initial, address % LINE_SIZE, 8, 5);
        SetInteger(operands, address % LINE_SIZE, 8, 7);
        Transaction& transaction = PrepareAtomic(row, driver, CHI::Eb::Opcodes::REQ::AtomicSwap,
            3, address, initial, false);
        FlowState& flow = impl.flows.at(transaction.handle);
        AcceptAtomicOperands(row, impl, transaction, 15, operands);
        impl.ActionAtomic(transaction);
        TIMING_CHECK(row, flow.backendOperationOwned);
        PopOnlyRequest(row, impl, 19, CHI::Eb::Opcodes::REQ::AtomicSwap);
        impl.time = 20;
        TIMING_CHECK(row, impl.HandleBackendRetry(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck)));
        TIMING_CHECK(row, flow.backendOperationOwned);
        TIMING_CHECK(row, impl.backendRetries.contains(transaction.handle));
        TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::AtomicExecution));
        Response grant;
        grant.source = 6;
        grant.target = 2;
        grant.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
        impl.time = 21;
        impl.ProcessRSP(grant);
        impl.AdvanceBackendRetries();
        TIMING_CHECK(row, impl.backendRetries.empty());
        TIMING_CHECK(row, flow.backendOperationOwned);
        const Request resent = PopOnlyRequest(row, impl, 27, CHI::Eb::Opcodes::REQ::AtomicSwap);
        TIMING_CHECK(row, !resent.allowRetry);
        TIMING_CHECK(row, impl.txrsp.empty());
        impl.time = 28;
        TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
        impl.ActionAtomic(transaction);
        TIMING_CHECK(row, impl.txdat.size() == 1);
        ExpectTime(row, impl.txdat.front().readyTime, 31, "retried atomic operand visibility");
        impl.time = 29;
        TIMING_CHECK(row, impl.HandleBackendDAT(transaction,
            AtomicBackendResult(transaction, options, initial)));
        TIMING_CHECK(row, flow.atomic.backendResultReceived);
        TIMING_CHECK(row, flow.backendOperationOwned);
        TIMING_CHECK(row, flow.backendPhase == BackendPhase::WaitDataEmission);
        PopOnlyData(row, impl, 31, CHI::Eb::Opcodes::DAT::NonCopyBackWrData, true);
        TIMING_CHECK(row, !flow.backendOperationOwned);
        TIMING_CHECK(row, flow.backendDataEmitted);
        impl.ActionAtomic(transaction);
        TIMING_CHECK(row, flow.directoryCommitted);
        ExpectTime(row, transaction.readyTime, 33 + options.dataLatency, "retried atomic result data");
        TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::AtomicExecution));
        impl.time = 33 + options.dataLatency;
        const Time resultVisible = transaction.readyTime + 3;
        impl.ReturnAtomic(transaction);
        const Data result = PopOnlyData(row, impl, resultVisible, CHI::Eb::Opcodes::DAT::CompData);
        CheckAtomicResult(row, transaction, options, result, initial, resultVisible);
        TIMING_CHECK(row, impl.txrsp.empty() && impl.txreq.empty());
        TIMING_CHECK(row, !flow.responseOwned);
        RetireChecked(row, impl, transaction.handle);
    }
}

namespace {

    void CompleteAXIRead(const Row& row, Core::Impl& impl, Transaction& transaction,
                         const AXIAddress& address, const LineData& data, bool failed)
    {
        impl.time = 18;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            AXIReadData read;
            read.id = address.id;
            read.resp = failed ? AXIResp::SLVERR : AXIResp::OKAY;
            read.last = beat == 1;
            for (size_t index = 0; index < 32; ++index)
                read.data[index] = data[beat * 32 + index];
            TIMING_CHECK(row, impl.axi.PushR(read));
            impl.axi.Tick();
            while (const auto completion = impl.axi.PeekCompletion())
            {
                impl.HandleAXICompletion(*completion);
                impl.axi.PopCompletion();
            }
            if (beat == 0)
            {
                TIMING_CHECK(row, !transaction.Complete(COMPLETE_MEMORY));
                TIMING_CHECK(row, impl.flows.at(transaction.handle).backendOperationOwned);
            }
        }
        TIMING_CHECK(row, transaction.Complete(COMPLETE_MEMORY));
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
        ExpectTime(row, transaction.readyTime, 20 + impl.options.dataLatency, "AXI atomic read completion");
        TIMING_CHECK(row, transaction.responseRespErr == (failed
            ? CHI::Eb::RespErrs::NDERR : CHI::Eb::RespErrs::OK));
    }

    void CompleteAXIWrite(const Row& row, Core::Impl& impl, Transaction& transaction,
                          const LineData& expected)
    {
        const auto address = impl.axi.PopAW();
        TIMING_CHECK(row, address.has_value());
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = impl.axi.PopW();
            TIMING_CHECK(row, data.has_value());
            TIMING_CHECK(row, data->last == (beat == 1));
            for (size_t index = 0; index < 32; ++index)
            {
                TIMING_CHECK(row, data->data[index] == expected[beat * 32 + index]);
                TIMING_CHECK(row, data->strobes[index]);
            }
        }
        impl.time = 28;
        TIMING_CHECK(row, impl.axi.PushB({ address->id, AXIResp::OKAY }));
        impl.axi.Tick();
        while (const auto completion = impl.axi.PeekCompletion())
        {
            impl.HandleAXICompletion(*completion);
            impl.axi.PopCompletion();
        }
        TIMING_CHECK(row, !impl.flows.at(transaction.handle).backendOperationOwned);
        TIMING_CHECK(row, transaction.phase == Phase::Action);
    }

    void RunAXIAtomic(const Row& row, ProfileKind profile, uint16_t opcode, bool failed)
    {
        Options options = AtomicOptions(profile, true, BackendKind::AXI);
        options.timing.atomicExecution = 3;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(opcode);
        const uint64_t address = 0x100020;
        Request request = Timing::MakeRequest(opcode, 4, 1, address);
        request.size = 2;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        Transaction& transaction = AddTransaction(row, driver, request);
        LineData initial{};
        LineData operands{};
        SetInteger(initial, address % LINE_SIZE, 4, 5);
        SetInteger(operands, address % LINE_SIZE, 4, 7);
        impl.memory.WriteFull(transaction.request.Line(), initial);
        transaction.data = initial;
        impl.time = 10;
        impl.ResolveAtomic(transaction);
        impl.ActionAtomic(transaction);
        PopOnlyResponse(row, impl, 13, CHI::Eb::Opcodes::RSP::DBIDResp);
        TIMING_CHECK(row, impl.txrsp.empty());
        AcceptAtomicOperands(row, impl, transaction, 14, operands);
        impl.ActionAtomic(transaction);
        FlowState& flow = impl.flows.at(transaction.handle);
        TIMING_CHECK(row, flow.backendOperationOwned);
        TIMING_CHECK(row, transaction.memoryRequired);
        const auto ar = impl.axi.PopAR();
        TIMING_CHECK(row, ar.has_value());
        TIMING_CHECK(row, ar->address == transaction.request.Line().address && ar->len == 1);
        CompleteAXIRead(row, impl, transaction, *ar, initial, failed);
        TIMING_CHECK(row, !flow.directoryCommitted);
        TIMING_CHECK(row, !impl.axi.PeekAW());
        const AtomicExecution expected = ExecuteAtomic(transaction.request, initial, operands);
        Time finalResponse = 0;
        if (!failed)
        {
            impl.time = 22;
            impl.ActionAtomic(transaction);
            ExpectTime(row, transaction.readyTime, 25, "AXI local atomic execution");
            TIMING_CHECK(row, !flow.directoryCommitted && !impl.axi.PeekAW());
            driver.TickTo(24);
            TIMING_CHECK(row, !flow.directoryCommitted && !impl.axi.PeekAW());
            driver.TickTo(25);
            TIMING_CHECK(row, flow.spillPending && flow.backendOperationOwned);
            CompleteAXIWrite(row, impl, transaction, expected.data);
            impl.ActionAtomic(transaction);
            TIMING_CHECK(row, flow.directoryCommitted && !flow.spillPending);
            ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::AtomicExecution), 25,
                "AXI execution applied once");
            TIMING_CHECK(row, impl.memory.Read(transaction.request.Line()) == expected.data);
            finalResponse = store ? 33 : 35 + options.dataLatency - 2;
        }
        else
        {
            impl.time = 22;
            impl.ActionAtomic(transaction);
            TIMING_CHECK(row, flow.directoryCommitted);
            TIMING_CHECK(row, !flow.scheduleMarks.contains(SchedulePoint::AtomicExecution));
            TIMING_CHECK(row, !flow.backendOperationOwned && !impl.axi.PeekAW());
            TIMING_CHECK(row, impl.memory.Read(transaction.request.Line()) == initial);
            finalResponse = store ? 27 : 29 + options.dataLatency - 2;
        }

        if (store)
        {
            const Response comp = PopOnlyResponse(row, impl, finalResponse,
                CHI::Eb::Opcodes::RSP::Comp);
            TIMING_CHECK(row, comp.respErr == (failed ? CHI::Eb::RespErrs::NDERR
                                                      : CHI::Eb::RespErrs::OK));
            TIMING_CHECK(row, impl.txdat.empty());
        }
        else
        {
            const Time dataDone = finalResponse - 3;
            ExpectTime(row, transaction.readyTime, dataDone, "AXI atomic result data");
            impl.time = dataDone;
            impl.ReturnAtomic(transaction);
            const Data result = PopOnlyData(row, impl, finalResponse,
                CHI::Eb::Opcodes::DAT::CompData);
            CheckAtomicResult(row, transaction, options, result, initial, finalResponse);
            TIMING_CHECK(row, result.respErr == (failed ? CHI::Eb::RespErrs::NDERR
                                                        : CHI::Eb::RespErrs::OK));
        }
        TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
        TIMING_CHECK(row, !flow.responseOwned);
        TIMING_CHECK(row, !impl.axi.HasInflight());
        RetireChecked(row, impl, transaction.handle);
    }

}

LUNATIC_TEST(Stage10MatrixC24AXIAtomicExecutionAndErrors)
{
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::AtomicStore::ADD,
                 CHI::Eb::Opcodes::REQ::AtomicSwap })
            for (const bool failed : { false, true })
            {
                const Row row{ profile, "C24" };
                RunAXIAtomic(row, profile, opcode, failed);
            }
}
