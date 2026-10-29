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
        driver.SnoopData(4, victim.flit.TxnID(), dataA, CHI::Eb::Resps::SnpRespData_I);
        SFReceiveUnique(driver, 5, 3);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 4) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectHome(a).valid);
        LUNATIC_CHECK(driver.driver.node.InspectHome(b).valid);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 4, a.address, true));
        victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 5);
        driver.SnoopData(5, victim.flit.TxnID(), dataB, CHI::Eb::Resps::SnpRespData_I);
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
