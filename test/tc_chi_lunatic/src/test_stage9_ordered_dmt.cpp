#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void SendDMTData(CheckedDriver<Config256>& driver, SNOracle<Config256>& oracle,
                     TransactionId originalTxnId, TransactionId downstreamTxnId)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 4;
            data.HomeNID() = 2;
            data.TxnID() = originalTxnId;
            data.DBID() = downstreamTxnId;
            data.Opcode() = CHI::Eb::Opcodes::DAT::DataSepResp;
            data.Resp() = CHI::Eb::Resps::DataSepResp_UC;
            data.DataID() = beat * 2;
            data.CCID() = 0;
            oracle.Data(driver.driver.now, data);
            driver.Oracle(4).Data(driver.driver.now, data);
        }
    }

    void SendBackendReceipt(CheckedDriver<Config256>& driver, SNOracle<Config256>& oracle,
                            TransactionId downstreamTxnId)
    {
        Driver<Config256>::RSP receipt{};
        receipt.SrcID() = 6;
        receipt.TgtID() = 2;
        receipt.TxnID() = downstreamTxnId;
        receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
        oracle.Response(driver.driver.now, receipt);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(receipt));
        driver.driver.Tick();
    }

    void RunOrderedSeparatedDMTGate(bool receiptBeforeAck)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.dmtEnabled = true;
        options.separatedReadResponse = true;
        options.separatedBackendRead = true;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        SNOracle<Config256> oracle(2, 6);
        oracle.RegisterRequester(4);

        auto first = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x1f000, true);
        first.Order() = 2;
        driver.PushRequest(first);
        const auto downstreamFirst = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
        LUNATIC_CHECK(downstreamFirst.ReturnNID() == 4);
        LUNATIC_CHECK(downstreamFirst.ReturnTxnID() == 1);
        LUNATIC_CHECK(downstreamFirst.Order() == 1);
        oracle.Request(driver.driver.now, downstreamFirst);
        SendDMTData(driver, oracle, 1, downstreamFirst.TxnID());
        const auto firstResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
        if (receiptBeforeAck)
            SendBackendReceipt(driver, oracle, downstreamFirst.TxnID());

        auto second = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x1f040, true);
        second.Order() = 2;
        driver.PushRequest(second);
        driver.driver.Until([&] {
            return driver.driver.node.GetPerformanceCounters().admittedTransactions == 2;
        });
        for (size_t cycle = 0; cycle < 8; ++cycle)
        {
            LUNATIC_CHECK(!driver.driver.node.PeekTXREQ());
            driver.driver.Tick();
        }

        driver.CompAck(4, firstResponse.DBID());
        if (!receiptBeforeAck)
            SendBackendReceipt(driver, oracle, downstreamFirst.TxnID());
        const auto downstreamSecond = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
        LUNATIC_CHECK(downstreamSecond.ReturnNID() == 4);
        LUNATIC_CHECK(downstreamSecond.ReturnTxnID() == 2);
        LUNATIC_CHECK(downstreamSecond.Order() == 1);
        oracle.Request(driver.driver.now, downstreamSecond);
        SendDMTData(driver, oracle, 2, downstreamSecond.TxnID());
        const auto secondResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
        driver.CompAck(4, secondResponse.DBID());
        SendBackendReceipt(driver, oracle, downstreamSecond.TxnID());
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

}

LUNATIC_TEST(Stage9OrderedSeparatedDMTWaitsForCompAckBeforeReceipt)
{
    RunOrderedSeparatedDMTGate(false);
}

LUNATIC_TEST(Stage9OrderedSeparatedDMTWaitsForCompAckAfterReceipt)
{
    RunOrderedSeparatedDMTGate(true);
}
