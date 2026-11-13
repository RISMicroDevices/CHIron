#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage9CopybackResponseReleasesTxnID)
{
    CheckedDriver<Config256> driver;
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 7, 0x1a000));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1a040));
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TxnID() == 7);
    }

    LineData line{};
    ByteEnables enables{};
    for (size_t index = 0; index < line.size(); ++index)
    {
        line[index] = uint8_t(index * 13 + 5);
        enables[index] = 1;
    }
    driver.WriteData(4, 7, response.DBID(), line, enables,
        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == 2);
}

LUNATIC_TEST(Stage9EvictOrEvictCompReleasesTxnID)
{
    CheckedDriver<Config256> driver;
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteEvictOrEvict, 4, 9, 0x1a100, true));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 9, 0x1a140));
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TxnID() == 9);
    }

    driver.CompAck(4, response.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == 2);
}
