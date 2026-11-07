#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void RunOrdinaryReadTxnIDRelease(uint16_t opcode, uint8_t order = 0, bool separated = false)
    {
        Options options;
        options.separatedReadResponse = separated;
        CheckedDriver<Config256> driver(options);
        auto request = driver.Request(opcode, 4, 7, 0x1b000, true);
        request.Order() = order;
        driver.PushRequest(request);

        DataBufferId dbid = 0;
        if (separated)
            dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData).DBID();
        else if (order != 0)
            driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);

        const auto dataOpcode = separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                          : CHI::Eb::Opcodes::DAT::CompData;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(dataOpcode);
            LUNATIC_CHECK(data.TxnID() == 7);
            dbid = data.DBID();
        }

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1b040));
        if (separated)
            driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(dataOpcode);
            LUNATIC_CHECK(data.TxnID() == 7);
            LUNATIC_CHECK(data.DBID() != dbid);
        }

        driver.CompAck(4, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == 2);
    }

}

LUNATIC_TEST(Stage9OrdinaryReadNoSnpReleasesTxnIDBeforeCompAck)
{
    RunOrdinaryReadTxnIDRelease(CHI::Eb::Opcodes::REQ::ReadNoSnp);
}

LUNATIC_TEST(Stage9OrdinaryReadSharedReleasesTxnIDBeforeCompAck)
{
    RunOrdinaryReadTxnIDRelease(CHI::Eb::Opcodes::REQ::ReadShared);
}

LUNATIC_TEST(Stage9OrdinaryReadOrderedReleasesTxnIDBeforeCompAck)
{
    RunOrdinaryReadTxnIDRelease(CHI::Eb::Opcodes::REQ::ReadNoSnp, 2);
}

LUNATIC_TEST(Stage9OrdinaryReadSeparatedReleasesTxnIDBeforeCompAck)
{
    RunOrdinaryReadTxnIDRelease(CHI::Eb::Opcodes::REQ::ReadNoSnp, 0, true);
}

LUNATIC_TEST(Stage9OrdinaryReadPrematureTxnIDReuseIsRejected)
{
    Driver<Config256> driver;
    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1b100, true));
    const auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);

    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1b140));
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 1);
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.GetErrors().front().reason.find("duplicate live requester transaction ID") !=
                  std::string::npos);

    const auto last = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(last.DBID() == first.DBID());
    driver.CompAck(4, first.DBID());
    driver.Quiesce();
}
