#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData SFData(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    ByteEnables SFEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void SFReceiveUnique(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_UC);
            dbid = data.DBID();
        }
        driver.CompAck(source, dbid);
    }

    void SFWriteback(CheckedDriver<Config256>& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 1, line.address));
        auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 1, dbid.DBID(), data, SFEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    void CleanCapacity(bool precise)
    {
        Options options;
        options.policy = PolicyKind::NINE;
        options.snoopFilterSets = 1;
        options.snoopFilterWays = 1;
        options.preciseSnoopFilter = precise;
        CheckedDriver<Config256> driver(options);
        const LineKey a = LineKey::FromAddress(0x1f000, false);
        const LineKey b = LineKey::FromAddress(0x1f040, false);
        const LineData dataA = SFData(13);
        const LineData dataB = SFData(107);
        driver.driver.node.GetMemory().WriteFull(a, dataA);
        driver.driver.node.GetMemory().WriteFull(b, dataB);

        LUNATIC_CHECK(driver.ReadData(4, 2, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
        driver.Quiesce();
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 3, b.address, true));
        auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 4);
        driver.SnoopResponse(4, victim.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        SFReceiveUnique(driver, 5, 3);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 4) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).valid);
        LUNATIC_CHECK(driver.driver.node.InspectHome(b).valid);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 4, a.address, true));
        victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 5);
        driver.SnoopResponse(5, victim.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        SFReceiveUnique(driver, 6, 4);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(b, 5) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).data == dataA);
    }

    void DirtyCapacity(bool precise)
    {
        Options options;
        options.policy = PolicyKind::NINE;
        options.snoopFilterSets = 1;
        options.snoopFilterWays = 1;
        options.preciseSnoopFilter = precise;
        CheckedDriver<Config256> driver(options);
        const LineKey a = LineKey::FromAddress(0x20000, false);
        const LineKey b = LineKey::FromAddress(0x20040, false);
        const LineData dataA = SFData(29);
        const LineData dataB = SFData(131);
        SFWriteback(driver, a, dataA);
        LUNATIC_CHECK(driver.ReadData(5, 5, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 5) == CacheState::UD);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 6, b.address, true));
        auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 5);
        driver.SnoopData(5, victim.flit.TxnID(), dataA, CHI::Eb::Resps::SnpRespData_I_PD);
        SFReceiveUnique(driver, 6, 6);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 5) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).dirty);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).data == dataA);
    }

}

LUNATIC_TEST(Stage4SnoopFilterCleanCapacityPreciseAndLossy)
{
    CleanCapacity(true);
    CleanCapacity(false);
}

LUNATIC_TEST(Stage4SnoopFilterDirtyCapacityPreciseAndLossy)
{
    DirtyCapacity(true);
    DirtyCapacity(false);
}

namespace {

    void RunVictimActiveLine(bool directoryVictim)
    {
        Options options;
        if (directoryVictim)
        {
            options.policy = PolicyKind::Inclusive;
            options.slcSets = 1;
            options.slcWays = 1;
        }
        else
        {
            options.policy = PolicyKind::NINE;
            options.snoopFilterSets = 1;
            options.snoopFilterWays = 1;
        }
        CheckedDriver<Config256> driver(options);
        const LineKey a = LineKey::FromAddress(0x20100, false);
        const LineKey b = LineKey::FromAddress(0x20140, false);
        const LineData dataA = SFData(41);
        const LineData dataB = SFData(149);
        driver.driver.node.GetMemory().WriteFull(a, dataA);
        driver.driver.node.GetMemory().WriteFull(b, dataB);

        bool holdOlder = true;
        bool olderAction = false;
        driver.driver.node.GetEvents()->OnTransactionSchedule.Register(
            Gravity::MakeListener<TransactionScheduleEvent>("victim-active-line", 0,
                [&](TransactionScheduleEvent& event) {
                    if (event.transaction == 0 && event.phase == Phase::Action && holdOlder)
                    {
                        olderAction = true;
                        event.Backpressure();
                    }
                }));

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, a.address, true));
        driver.driver.Until([&] { return olderAction; });
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 2, b.address, true));
        DataBufferId youngerDbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
            youngerDbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();

        holdOlder = false;
        for (size_t cycle = 0; cycle < 12; ++cycle)
        {
            driver.driver.Tick();
            LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
        }
        driver.CompAck(5, youngerDbid);

        const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 5);
        driver.SnoopResponse(5, victim.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        SFReceiveUnique(driver, 4, 1);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(b, 5) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).data == dataA);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 3, b.address, true));
        const auto later = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(later.target == 4);
        driver.SnoopResponse(4, later.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        SFReceiveUnique(driver, 6, 3);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(b).data == dataB);
    }

}

LUNATIC_TEST(Stage4SnoopFilterVictimWaitsForActiveLine)
{
    RunVictimActiveLine(false);
}

LUNATIC_TEST(Stage4DirectoryVictimWaitsForActiveLine)
{
    RunVictimActiveLine(true);
}
