#include "oracle_driver.hpp"

#include <algorithm>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;

    LineData VictimPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 13);
        return data;
    }

    ByteEnables VictimEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void VictimWriteback(TestDriver& driver, const LineKey& line, TransactionId txnId, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, txnId, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(5, txnId, response.DBID(), data, VictimEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    void SeedCopybackHome(TestDriver& driver, const LineKey& target, const LineData& targetLatest,
                          const LineKey& copyback, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, 3, copyback.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(5, 3, response.DBID(), data, VictimEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.flit.Addr() == target.address >> 3);
        driver.SnoopData(5, victim.flit.TxnID(), targetLatest, CHI::Eb::Resps::SnpRespData_I_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(5, 4, target.address, CHI::Eb::Opcodes::REQ::ReadUnique) == targetLatest);
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage9VictimSkipsActiveCopybackHome)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 2;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;
    options.structure.bankCount = 4;
    options.memoryLatency = 1;
    options.strategy.copybackResponseAtResolve = true;
    options.strategy.snoopIssueAtResolve = true;
    options.strategy.snoopDataThroughDataPort = true;

    TestDriver driver(options);
    const LineKey target = LineKey::FromAddress(0x40000, false);
    const LineKey copyback = LineKey::FromAddress(0x40100, false);
    const LineKey incoming = LineKey::FromAddress(0x40200, false);
    const LineData targetBefore = VictimPattern(17);
    const LineData targetLatest = VictimPattern(93);
    const LineData copybackBefore = VictimPattern(151);
    const LineData copybackLatest = VictimPattern(211);

    VictimWriteback(driver, target, 1, targetBefore);
    LUNATIC_CHECK(driver.ReadData(5, 2, target.address, CHI::Eb::Opcodes::REQ::ReadUnique) == targetBefore);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(target, 5) == CacheState::UD);
    SeedCopybackHome(driver, target, targetLatest, copyback, copybackBefore);
    LUNATIC_CHECK(driver.driver.node.InspectHome(target).valid);
    LUNATIC_CHECK(driver.driver.node.InspectHome(copyback).valid);

    std::vector<size_t> directoryCandidates;
    driver.driver.node.GetEvents()->OnVictimSelection.Register(Gravity::MakeListener<VictimSelectionEvent>(
        "active-copyback-test", 0, [&](VictimSelectionEvent& event) {
            if (event.domain == VictimDomain::Directory)
                directoryCandidates.insert(directoryCandidates.end(), event.candidates.begin(), event.candidates.end());
            event.SelectVictim(event.candidates.back());
        }));

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadClean, 6, 10, incoming.address, true));
    const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(victim.target == 5 && victim.flit.Addr() == target.address >> 3);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, 11, copyback.address));
    const auto copybackResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(copybackResponse.TxnID() == 11);
    driver.SnoopData(5, victim.flit.TxnID(), targetLatest, CHI::Eb::Resps::SnpRespData_I_PD);
    driver.WriteData(5, 11, copybackResponse.DBID(), copybackLatest, VictimEnables(),
        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    DataBufferId incomingDBID = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TgtID() == 6 && data.TxnID() == 10);
        LUNATIC_CHECK(data.Data()[0] == 0 && data.Data()[1] == 0 && data.Data()[2] == 0 && data.Data()[3] == 0);
        incomingDBID = data.DBID();
    }
    driver.CompAck(6, incomingDBID);
    const auto copybackVictim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(copybackVictim.target == 6 && copybackVictim.flit.Addr() == incoming.address >> 3);
    driver.SnoopResponse(6, copybackVictim.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    driver.Quiesce();

    LUNATIC_CHECK(!directoryCandidates.empty());
    LUNATIC_CHECK(std::find(directoryCandidates.begin(), directoryCandidates.end(), 1) == directoryCandidates.end());
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(target) == targetLatest);
    LUNATIC_CHECK(driver.driver.node.InspectHome(copyback).data == copybackLatest);
    LUNATIC_CHECK(driver.ReadData(7, 20, target.address, CHI::Eb::Opcodes::REQ::ReadClean) == targetLatest);
    driver.Quiesce();
}


LUNATIC_TEST(Stage9VictimWaitsForActiveCopyback)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;
    options.structure.bankCount = 4;

    Core::Impl impl(2, options, std::make_shared<EventHub>());
    const LineKey copyback = LineKey::FromAddress(0x50100, false);
    const LineKey incoming = LineKey::FromAddress(0x50200, false);
    const LineData copybackBefore = VictimPattern(151);
    const LineData copybackLatest = VictimPattern(211);
    const auto makeRequest = [](uint16_t opcode, NodeId source, TransactionId txnId, uint64_t address) {
        Request request{};
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.address = address;
        request.opcode = opcode;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.snpAttr = 1;
        return request;
    };

    Transaction* older = impl.tracker.Allocate(
        makeRequest(CHI::Eb::Opcodes::REQ::ReadClean, 6, 10, incoming.address), 10);
    LUNATIC_CHECK(older);
    older->phase = Phase::Action;
    older->Mark(COMPLETE_MEMORY);
    impl.flows.emplace(older->handle, FlowState{});
    ++impl.Bank(incoming).activeOriginals;
    LUNATIC_CHECK(impl.Bank(incoming).hazards.Add(older->handle, incoming, older->actionAge));

    Transaction* active = impl.tracker.Allocate(
        makeRequest(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, 11, copyback.address), 11);
    LUNATIC_CHECK(active);
    active->phase = Phase::WaitWriteData;
    active->homeHit = true;
    impl.flows.emplace(active->handle, FlowState{});
    ++impl.Bank(copyback).activeOriginals;
    LUNATIC_CHECK(impl.Bank(copyback).hazards.Add(active->handle, copyback, active->actionAge));

    HomeLine& home = impl.Bank(copyback).directory.Install(copyback, 0, 1);
    home.data = copybackLatest;
    home.validBytes = VictimEnables();
    home.dirty = true;
    impl.memory.WriteFull(copyback, copybackBefore);

    LUNATIC_CHECK(!impl.PrepareHomeCommit(*older));
    LUNATIC_CHECK(!impl.flows.at(older->handle).victimPending);
    LUNATIC_CHECK(impl.Bank(copyback).directory.Find(copyback));

    LUNATIC_CHECK(impl.Bank(copyback).hazards.Release(active->handle, copyback));
    LUNATIC_CHECK(impl.PrepareHomeCommit(*older));
    LUNATIC_CHECK(impl.memory.Read(copyback) == copybackLatest);

    impl.Retire(active->handle);
    impl.Retire(older->handle);
    LUNATIC_CHECK(impl.ValidateInvariants());
    LUNATIC_CHECK(impl.errors.empty());
}

LUNATIC_TEST(Stage9VictimResponseDoesNotReleaseSnoopFilterWayEarly)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;

    Core::Impl impl(2, options, std::make_shared<EventHub>());
    const LineKey victim = LineKey::FromAddress(0x40000, false);
    const LineKey incoming = LineKey::FromAddress(0x40100, false);
    const LineKey replacement = LineKey::FromAddress(0x40200, false);
    const LineData latest = VictimPattern(93);
    impl.Bank(victim).snoopFilter.Track(victim, 5, CacheState::UD, 0);

    const auto makeRequest = [](NodeId source, TransactionId txnId, uint64_t address) {
        Request request{};
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.address = address;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadClean;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.snpAttr = 1;
        return request;
    };

    Transaction* blocker = impl.tracker.Allocate(makeRequest(7, 1, victim.address), 10);
    Transaction* operation = impl.tracker.Allocate(makeRequest(6, 2, incoming.address), 20);
    LUNATIC_CHECK(blocker && operation);
    impl.flows.emplace(blocker->handle, FlowState{});
    impl.flows.emplace(operation->handle, FlowState{});
    operation->phase = Phase::Action;
    operation->Mark(COMPLETE_MEMORY);
    LUNATIC_CHECK(impl.Bank(incoming).hazards.Add(operation->handle, incoming, operation->actionAge));

    LUNATIC_CHECK(!impl.PrepareHomeCommit(*operation));
    LUNATIC_CHECK(impl.flows.at(operation->handle).victimSnoopFilterOnly);
    LUNATIC_CHECK(impl.flows.at(operation->handle).victimPending);
    LUNATIC_CHECK(impl.EmitSnoops(*operation));
    const Snoop snoop = impl.txsnp.front().flit;
    LUNATIC_CHECK(impl.Bank(victim).hazards.Add(blocker->handle, victim, blocker->actionAge));

    for (size_t beat = 0; beat < 2; ++beat)
    {
        Data response{};
        response.source = 5;
        response.target = 2;
        response.homeNid = 2;
        response.txnId = snoop.txnId;
        response.opcode = CHI::Eb::Opcodes::DAT::SnpRespData;
        response.resp = CHI::Eb::Resps::SnpRespData_I_PD;
        response.dataId = beat == 0 ? 0 : 2;
        for (size_t index = 0; index < 32; ++index)
            response.bytes[index] = latest[beat * 32 + index];
        response.byteEnables.fill(1);
        impl.ProcessSnoopDAT(*operation, response);
    }
    LUNATIC_CHECK(impl.flows.at(operation->handle).victimPending);
    LUNATIC_CHECK(impl.flows.at(operation->handle).victimResponseUpdated);

    Transaction* competing = impl.tracker.Allocate(makeRequest(8, 3, replacement.address), 30);
    LUNATIC_CHECK(competing);
    impl.flows.emplace(competing->handle, FlowState{});
    competing->phase = Phase::Action;
    competing->Mark(COMPLETE_MEMORY);
    LUNATIC_CHECK(impl.Bank(replacement).hazards.Add(competing->handle, replacement, competing->actionAge));
    LUNATIC_CHECK(!impl.PrepareHomeCommit(*competing));

    LUNATIC_CHECK(impl.Bank(victim).hazards.Release(blocker->handle, victim));
    LUNATIC_CHECK(impl.CompleteVictim(*operation));
    LUNATIC_CHECK(impl.memory.Read(victim) == latest);
    LUNATIC_CHECK(impl.PrepareHomeCommit(*competing));
    LUNATIC_CHECK(impl.errors.empty());
}

LUNATIC_TEST(Stage9VictimRejectsUnsafeHazard)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;

    Core::Impl impl(2, options, std::make_shared<EventHub>());
    const LineKey incoming = LineKey::FromAddress(0x60100, false);
    const LineKey victim = LineKey::FromAddress(0x60200, false);
    const LineKey otherIncoming = LineKey::FromAddress(0x60300, false);
    const auto makeRequest = [](NodeId source, TransactionId txnId, uint64_t address) {
        Request request{};
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.address = address;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadClean;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.snpAttr = 1;
        return request;
    };

    Transaction* blocker = impl.tracker.Allocate(makeRequest(5, 1, victim.address), 10);
    Transaction* requester = impl.tracker.Allocate(makeRequest(6, 2, incoming.address), 20);
    Transaction* younger = impl.tracker.Allocate(makeRequest(7, 3, victim.address), 30);
    Transaction* otherRequester = impl.tracker.Allocate(makeRequest(8, 4, otherIncoming.address), 40);
    Transaction* olderVictim = impl.tracker.Allocate(makeRequest(9, 5, otherIncoming.address), 5);
    LUNATIC_CHECK(blocker && requester && younger && otherRequester && olderVictim);
    LUNATIC_CHECK(impl.Bank(victim).hazards.Add(blocker->handle, victim, blocker->actionAge));
    LUNATIC_CHECK(impl.Bank(victim).hazards.Add(younger->handle, victim, younger->actionAge));
    LUNATIC_CHECK(impl.Bank(victim).hazards.Next(victim) == blocker->handle);

    impl.Bank(victim).directory.Install(victim, 0, 1);
    impl.Bank(victim).snoopFilter.Track(victim, 5, CacheState::UD, 0);

    LUNATIC_CHECK(impl.ChooseVictim(requester->handle, VictimDomain::Directory, incoming) ==
        impl.Bank(incoming).directory.Ways());
    LUNATIC_CHECK(impl.ChooseVictim(requester->handle, VictimDomain::SnoopFilter, incoming) ==
        impl.Bank(incoming).snoopFilter.Ways());
    LUNATIC_CHECK(impl.ChooseVictim(otherRequester->handle, VictimDomain::Directory, otherIncoming) == 0);
    LUNATIC_CHECK(impl.ChooseVictim(otherRequester->handle, VictimDomain::SnoopFilter, otherIncoming) == 0);

    LUNATIC_CHECK(impl.Bank(victim).hazards.Append(olderVictim->handle, victim, olderVictim->actionAge));
    LUNATIC_CHECK(impl.Bank(victim).hazards.HasVictim(victim));
    LUNATIC_CHECK(impl.ChooseVictim(otherRequester->handle, VictimDomain::Directory, otherIncoming) ==
        impl.Bank(otherIncoming).directory.Ways());
    LUNATIC_CHECK(impl.ChooseVictim(otherRequester->handle, VictimDomain::SnoopFilter, otherIncoming) ==
        impl.Bank(otherIncoming).snoopFilter.Ways());
    LUNATIC_CHECK(impl.errors.empty());
}

LUNATIC_TEST(Stage9VictimResponseAcceptsSameKeyGeneration)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;

    Core::Impl impl(2, options, std::make_shared<EventHub>());
    const LineKey victim = LineKey::FromAddress(0x70100, false);
    const LineKey incoming = LineKey::FromAddress(0x70200, false);
    const LineData latest = VictimPattern(93);
    impl.Bank(victim).snoopFilter.Track(victim, 5, CacheState::UD, 0);

    Request request{};
    request.source = 6;
    request.target = 2;
    request.txnId = 2;
    request.address = incoming.address;
    request.opcode = CHI::Eb::Opcodes::REQ::ReadClean;
    request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
    request.snpAttr = 1;
    Transaction* operation = impl.tracker.Allocate(request, 20);
    LUNATIC_CHECK(operation);
    impl.flows.emplace(operation->handle, FlowState{});
    operation->phase = Phase::Action;
    operation->Mark(COMPLETE_MEMORY);
    LUNATIC_CHECK(impl.Bank(incoming).hazards.Add(operation->handle, incoming, operation->actionAge));

    LUNATIC_CHECK(!impl.PrepareHomeCommit(*operation));
    FlowState& flow = impl.flows.at(operation->handle);
    LUNATIC_CHECK(flow.victimSnoopFilterOnly && flow.victimPending);
    LUNATIC_CHECK(flow.victimLine == victim);
    LUNATIC_CHECK(impl.EmitSnoops(*operation));
    const Snoop snoop = impl.txsnp.front().flit;

    impl.Bank(victim).snoopFilter.Track(victim, 5, CacheState::UD, impl.time + 1);
    const SnoopFilterEntry& current = impl.Bank(victim).snoopFilter.At(
        impl.Bank(victim).snoopFilter.SetIndex(victim), flow.victimWay);
    LUNATIC_CHECK(current.valid && current.key == victim);
    LUNATIC_CHECK(current.generation != flow.victimGeneration);

    for (size_t beat = 0; beat < 2; ++beat)
    {
        Data response{};
        response.source = 5;
        response.target = 2;
        response.homeNid = 2;
        response.txnId = snoop.txnId;
        response.opcode = CHI::Eb::Opcodes::DAT::SnpRespData;
        response.resp = CHI::Eb::Resps::SnpRespData_I_PD;
        response.dataId = beat == 0 ? 0 : 2;
        for (size_t index = 0; index < 32; ++index)
            response.bytes[index] = latest[beat * 32 + index];
        response.byteEnables.fill(1);
        impl.ProcessSnoopDAT(*operation, response);
    }

    LUNATIC_CHECK(flow.victimResponseUpdated);
    LUNATIC_CHECK(impl.CompleteVictim(*operation));
    LUNATIC_CHECK(!flow.victimPending);
    LUNATIC_CHECK(impl.memory.Read(victim) == latest);
    LUNATIC_CHECK(impl.Bank(victim).snoopFilter.State(victim, 5) == CacheState::I);
    LUNATIC_CHECK(impl.errors.empty());
}

LUNATIC_TEST(Stage9DirectoryVictimClearsSnoopFilterOnly)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;

    Core::Impl impl(2, options, std::make_shared<EventHub>());
    const LineKey incoming = LineKey::FromAddress(0x70800, false);
    const LineKey blockedSF = LineKey::FromAddress(0x70900, false);
    const LineKey victim = LineKey::FromAddress(0x70a00, false);
    const LineData latest = VictimPattern(121);
    const LineData stale = VictimPattern(7);
    const auto makeRequest = [](NodeId source, TransactionId txnId, uint64_t address) {
        Request request{};
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.address = address;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadClean;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.snpAttr = 1;
        return request;
    };

    Transaction* blocker = impl.tracker.Allocate(makeRequest(5, 1, blockedSF.address), 30);
    Transaction* operation = impl.tracker.Allocate(makeRequest(6, 2, incoming.address), 20);
    LUNATIC_CHECK(blocker && operation);
    impl.flows.emplace(operation->handle, FlowState{});
    operation->phase = Phase::Action;
    operation->Mark(COMPLETE_MEMORY);
    LUNATIC_CHECK(impl.Bank(incoming).hazards.Add(operation->handle, incoming, operation->actionAge));
    LUNATIC_CHECK(impl.Bank(blockedSF).hazards.Add(blocker->handle, blockedSF, blocker->actionAge));
    impl.Bank(blockedSF).snoopFilter.Track(blockedSF, 5, CacheState::UD, 0);

    LUNATIC_CHECK(!impl.PrepareSnoopFilterCapacity(*operation));
    FlowState& flow = impl.flows.at(operation->handle);
    LUNATIC_CHECK(flow.victimSnoopFilterOnly);
    LUNATIC_CHECK(!flow.victimPending);

    LUNATIC_CHECK(impl.Bank(blockedSF).hazards.Remove(blocker->handle, blockedSF));
    const size_t sfWay = *impl.Bank(blockedSF).snoopFilter.FindWay(blockedSF);
    impl.Bank(blockedSF).snoopFilter.Invalidate(impl.Bank(blockedSF).snoopFilter.SetIndex(blockedSF), sfWay);
    LUNATIC_CHECK(impl.PrepareSnoopFilterCapacity(*operation));
    LUNATIC_CHECK(flow.victimSnoopFilterOnly);
    LUNATIC_CHECK(!flow.victimPending);

    HomeLine& home = impl.Bank(victim).directory.Install(victim, 0, 1);
    home.data = latest;
    home.validBytes = VictimEnables();
    home.dirty = true;
    impl.memory.WriteFull(victim, stale);

    LUNATIC_CHECK(impl.PrepareVictim(*operation));
    LUNATIC_CHECK(!flow.victimPending);
    LUNATIC_CHECK(impl.memory.Read(victim) == latest);
    LUNATIC_CHECK(!impl.Bank(victim).directory.Find(victim));
    LUNATIC_CHECK(impl.errors.empty());
}
