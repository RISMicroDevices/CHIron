#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData                            RolloverData()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(index * 9 + 5);
        return data;
    }

    void                                RolloverReceive(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId,
                                                        uint8_t expectedResp = CHI::Eb::Resps::CompData_UC)
    {
        DatabaseId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == expectedResp);
            dbid = data.DBID();
        }
        driver.CompAck(source, dbid);
    }

}

LUNATIC_TEST(Stage4SequentialDBIDRollover)
{
    Options options;
    options.memoryLatency = 0;
    Driver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x1c000, false);
    const LineData expected = RolloverData();
    driver.node.GetMemory().WriteFull(line, expected);
    for (size_t index = 0; index < 4097; ++index)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, index & 0xFFF, line.address);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        auto second = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(first.DBID() < 16 && second.DBID() < 16);
        driver.Quiesce();
    }
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage4CoherentSnoopIDRollover)
{
    CheckedDriver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x1d000, false);
    const LineData expected = RolloverData();
    driver.driver.node.GetMemory().WriteFull(line, expected);
    LUNATIC_CHECK(driver.ReadData(4, 0, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
    driver.Quiesce();

    NodeId previous = 4;
    for (size_t index = 1; index < 140; ++index)
    {
        const NodeId source = index % 2 ? 5 : 4;
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, source, index & 0xFFF, line.address, true));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(snoop.target == previous);
        driver.SnoopData(previous, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_I);
        RolloverReceive(driver, source, index & 0xFFF);
        driver.Quiesce();
        previous = source;
    }
}

LUNATIC_TEST(Stage4SnoopFanoutUsesBoundedBatches)
{
    Options options;
    options.snoopLimit = 2;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x1e000, false);
    const LineData expected = RolloverData();
    driver.driver.node.GetMemory().WriteFull(line, expected);
    LUNATIC_CHECK(driver.ReadData(4, 1, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
    driver.Quiesce();

    for (NodeId source = 5; source <= 7; ++source)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, source, source, line.address, true));
        driver.driver.Until([&] {
            return static_cast<bool>(driver.driver.node.PeekTXSNP() || driver.driver.node.PeekTXDAT());
        });
        while (driver.driver.node.PeekTXSNP())
        {
            auto emission = driver.driver.node.PopTXSNP();
            driver.Oracle(emission->target).Snoop(driver.driver.now, emission->flit);
            driver.SnoopData(emission->target, emission->flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SC);
            driver.driver.Until([&] {
                return static_cast<bool>(driver.driver.node.PeekTXSNP() || driver.driver.node.PeekTXDAT());
            });
        }
        RolloverReceive(driver, source, source, CHI::Eb::Resps::CompData_SC);
        driver.Quiesce();
    }

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 8, 8, line.address, true));
    for (size_t count = 0; count < 4; ++count)
    {
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        driver.SnoopResponse(snoop.target, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    }
    RolloverReceive(driver, 8, 8);
    driver.Quiesce();
    for (NodeId source = 4; source <= 7; ++source)
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, source) == CacheState::I);
}
