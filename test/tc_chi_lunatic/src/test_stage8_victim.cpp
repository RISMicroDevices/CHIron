#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData VictimData(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    ByteEnables VictimEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void Writeback(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId,
                   const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, source, txnId, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(source, txnId, response.DBID(), data, VictimEnables(),
                         CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    void ReceiveUnique(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_UC);
            dbid = data.DBID();
        }
        driver.CompAck(source, dbid);
    }

    void DirectorySelection(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.slcSets = 1;
        options.slcWays = 2;
        CheckedDriver<Config256> driver(options);
        const LineKey a = LineKey::FromAddress(0x40000, false);
        const LineKey b = LineKey::FromAddress(0x40040, false);
        const LineKey c = LineKey::FromAddress(0x40080, false);
        const LineData dataA = VictimData(13);
        const LineData dataB = VictimData(79);
        const LineData dataC = VictimData(151);
        size_t decisions = 0;
        driver.driver.node.GetEvents()->OnVictimSelection.Register(
            Gravity::MakeListener<VictimSelectionEvent>("directory", 0, [&](VictimSelectionEvent& event) {
                if (event.domain != VictimDomain::Directory)
                    return;
                LUNATIC_CHECK(event.candidates.size() == 2);
                ++decisions;
                if (event.line == a)
                    event.SelectVictim(1);
                else if (event.line == b || event.line == c)
                    event.SelectVictim(0);
            }));

        Writeback(driver, 4, 1, a, dataA);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).valid);
        Writeback(driver, 4, 2, b, dataB);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).valid);
        LUNATIC_CHECK(driver.driver.node.InspectHome(b).valid);
        Writeback(driver, 4, 3, c, dataC);

        LUNATIC_CHECK(decisions == 3);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).valid);
        LUNATIC_CHECK(!driver.driver.node.InspectHome(b).valid);
        LUNATIC_CHECK(driver.driver.node.InspectHome(c).valid);
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(b) == dataB);
        LUNATIC_CHECK(driver.driver.node.ValidateInvariants());
    }

    void SnoopFilterSelection(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.snoopFilterSets = 1;
        options.snoopFilterWays = 2;
        CheckedDriver<Config256> driver(options);
        const LineKey a = LineKey::FromAddress(0x50000, false);
        const LineKey b = LineKey::FromAddress(0x50040, false);
        const LineKey c = LineKey::FromAddress(0x50080, false);
        const LineData dataA = VictimData(17);
        const LineData dataB = VictimData(83);
        const LineData dataC = VictimData(157);
        driver.driver.node.GetMemory().WriteFull(a, dataA);
        driver.driver.node.GetMemory().WriteFull(b, dataB);
        driver.driver.node.GetMemory().WriteFull(c, dataC);
        size_t decisions = 0;
        driver.driver.node.GetEvents()->OnVictimSelection.Register(
            Gravity::MakeListener<VictimSelectionEvent>("snoop-filter", 0, [&](VictimSelectionEvent& event) {
                if (event.domain != VictimDomain::SnoopFilter)
                    return;
                LUNATIC_CHECK(event.candidates.size() == 2);
                ++decisions;
                event.SelectVictim(1);
            }));

        LUNATIC_CHECK(driver.ReadData(4, 1, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(5, 2, b.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataB);
        driver.Quiesce();

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 3, c.address, true));
        const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 5);
        driver.SnoopResponse(5, victim.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        ReceiveUnique(driver, 6, 3);
        driver.Quiesce();

        LUNATIC_CHECK(decisions == 1);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 4) == CacheState::UC);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(b, 5) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(c, 6) == CacheState::UC);
        LUNATIC_CHECK(driver.driver.node.ValidateInvariants());
    }

}

LUNATIC_TEST(Stage8VictimDirectorySelectionAllPolicies)
{
    DirectorySelection(PolicyKind::NINE);
    DirectorySelection(PolicyKind::Exclusive);
    DirectorySelection(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage8VictimSnoopFilterSelectionAllPolicies)
{
    SnoopFilterSelection(PolicyKind::NINE);
    SnoopFilterSelection(PolicyKind::Exclusive);
    SnoopFilterSelection(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage8VictimInvalidChoiceFallsBack)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    Driver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x60000, false);
    const LineKey b = LineKey::FromAddress(0x60040, false);
    const LineData dataA = VictimData(23);
    const LineData dataB = VictimData(97);

    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 1, a.address));
    auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(4, 1, response.DBID(), dataA, VictimEnables(),
                     CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();

    driver.node.GetEvents()->OnVictimSelection.Register(
        Gravity::MakeListener<VictimSelectionEvent>("invalid", 0, [&](VictimSelectionEvent& event) {
            if (event.domain == VictimDomain::Directory)
                event.SelectVictim(99);
        }));
    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 2, b.address));
    response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(4, 2, response.DBID(), dataB, VictimEnables(),
                     CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();

    LUNATIC_CHECK(!driver.node.InspectHome(a).valid);
    LUNATIC_CHECK(driver.node.InspectHome(b).valid);
    LUNATIC_CHECK(driver.node.GetMemory().Read(a) == dataA);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.GetErrors().front().reason.find("candidate set") != std::string::npos);
}

LUNATIC_TEST(Stage8VictimDirectoryRevalidatesReplacement)
{
    Options options;
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    CheckedDriver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x61000, false);
    const LineKey b = LineKey::FromAddress(0x61040, false);
    const LineKey c = LineKey::FromAddress(0x61080, false);
    const LineData dataB = VictimData(31);
    const LineData dataA = VictimData(97);
    const LineData dataC = VictimData(163);
    driver.driver.node.GetMemory().WriteFull(b, dataB);
    LUNATIC_CHECK(driver.ReadData(4, 1, b.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataB);
    driver.Quiesce();

    bool holdOlder = true;
    bool holdVictim = true;
    bool olderAction = false;
    bool victimSelected = false;
    driver.driver.node.GetEvents()->OnVictimSelection.Register(
        Gravity::MakeListener<VictimSelectionEvent>("capture-replacement", 0, [&](VictimSelectionEvent& event) {
            if (event.transaction == 2 && event.domain == VictimDomain::Directory)
                victimSelected = true;
        }));
    driver.driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("delay-replacement", 0,
            [&](TransactionScheduleEvent& event) {
                if (event.transaction == 1 && event.phase == Phase::Action && holdOlder)
                {
                    olderAction = true;
                    event.Backpressure();
                }
                if (event.transaction == 2 && event.phase == Phase::Action && victimSelected && holdVictim)
                    event.Backpressure();
            }));

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanInvalid, 4, 2, b.address));
    driver.driver.Until([&] { return olderAction; });
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, 3, a.address));
    const auto allocation = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(allocation.TxnID() == 3);
    driver.WriteData(5, 3, allocation.DBID(), dataA, VictimEnables(),
                     CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.driver.Until([&] { return victimSelected; });

    holdOlder = false;
    const auto olderComp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(olderComp.TxnID() == 2);
    const auto captured = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(captured.target == 4);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 4, c.address));
    const auto replacement = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(replacement.TxnID() == 4);
    driver.WriteData(6, 4, replacement.DBID(), dataC, VictimEnables(),
                     CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.driver.Until([&] { return driver.driver.node.InspectHome(c).valid; });
    LUNATIC_CHECK(driver.driver.node.InspectHome(c).data == dataC);
    LUNATIC_CHECK(driver.driver.node.InspectHome(c).dirty);
    driver.SnoopResponse(4, captured.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);

    holdVictim = false;
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(c) == dataC);
}

LUNATIC_TEST(Stage8VictimDirectoryRevalidatesSameKeyGeneration)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    CheckedDriver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x62000, false);
    const LineKey b = LineKey::FromAddress(0x62040, false);
    const LineData oldB = VictimData(37);
    const LineData newB = VictimData(103);
    const LineData dataA = VictimData(169);
    Writeback(driver, 4, 1, b, oldB);

    bool holdVictim = true;
    bool victimSelected = false;
    driver.driver.node.GetEvents()->OnVictimSelection.Register(
        Gravity::MakeListener<VictimSelectionEvent>("capture-generation", 0, [&](VictimSelectionEvent& event) {
            if (event.transaction == 2 && event.domain == VictimDomain::Directory)
                victimSelected = true;
        }));
    driver.driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("delay-generation", 0,
            [&](TransactionScheduleEvent& event) {
                if (event.transaction == 2 && event.phase == Phase::Action && victimSelected && holdVictim)
                    event.Backpressure();
            }));

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 2, b.address));
    const auto older = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, 3, a.address));
    const auto allocation = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(5, 3, allocation.DBID(), dataA, VictimEnables(),
                     CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.driver.Until([&] { return victimSelected; });

    driver.WriteData(4, 2, older.DBID(), newB, VictimEnables(),
                     CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.driver.Until([&] { return driver.driver.node.InspectHome(b).data == newB; });
    holdVictim = false;
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(b) == newB);
}

LUNATIC_TEST(Stage8VictimSnoopFilterRevalidatesReplacement)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;
    CheckedDriver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x63000, false);
    const LineKey b = LineKey::FromAddress(0x63040, false);
    const LineKey c = LineKey::FromAddress(0x63080, false);
    const LineData dataA = VictimData(43);
    const LineData dataB = VictimData(109);
    const LineData dataC = VictimData(175);
    driver.driver.node.GetMemory().WriteFull(a, dataA);
    driver.driver.node.GetMemory().WriteFull(b, dataB);
    driver.driver.node.GetMemory().WriteFull(c, dataC);
    LUNATIC_CHECK(driver.ReadData(4, 1, b.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataB);
    driver.Quiesce();

    bool holdOlder = true;
    bool holdVictim = true;
    bool olderAction = false;
    bool victimSelected = false;
    driver.driver.node.GetEvents()->OnVictimSelection.Register(
        Gravity::MakeListener<VictimSelectionEvent>("capture-sf-replacement", 0, [&](VictimSelectionEvent& event) {
            if (event.transaction == 2 && event.domain == VictimDomain::SnoopFilter)
                victimSelected = true;
        }));
    driver.driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("delay-sf-replacement", 0,
            [&](TransactionScheduleEvent& event) {
                if (event.transaction == 1 && event.phase == Phase::Action && holdOlder)
                {
                    olderAction = true;
                    event.Backpressure();
                }
                if (event.transaction == 2 && event.phase == Phase::Action && victimSelected && holdVictim)
                    event.Backpressure();
            }));

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanInvalid, 4, 2, b.address));
    driver.driver.Until([&] { return olderAction; });
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 3, a.address, true));
    driver.driver.Until([&] { return victimSelected; });

    holdOlder = false;
    const auto olderComp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(olderComp.TxnID() == 2);
    const auto captured = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(captured.target == 4);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 4, c.address, true));
    DataBufferId replacementDbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
        replacementDbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
    driver.driver.Until([&] { return driver.driver.node.InspectRequester(c, 6) == CacheState::UC; });

    holdVictim = false;
    driver.SnoopResponse(4, captured.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    driver.CompAck(6, replacementDbid);
    const auto replacement = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(replacement.target == 6);
    driver.SnoopResponse(6, replacement.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);

    DataBufferId allocationDbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
        allocationDbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
    driver.CompAck(5, allocationDbid);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 5) == CacheState::UC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(c, 6) == CacheState::I);
}
