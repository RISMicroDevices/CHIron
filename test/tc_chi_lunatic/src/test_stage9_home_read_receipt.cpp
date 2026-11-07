#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;
    using REQ = TestDriver::REQ;

    Options BackendOptions(bool separated, bool dmt = false)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.dmtEnabled = dmt;
        options.separatedReadResponse = separated;
        options.separatedBackendRead = separated;
        return options;
    }

    void SendBackendReceipt(TestDriver& driver, SNOracle<Config256>& oracle,
                            TransactionId downstreamTxnId)
    {
        TestDriver::RSP receipt{};
        receipt.SrcID() = 6;
        receipt.TgtID() = 2;
        receipt.TxnID() = downstreamTxnId;
        receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
        oracle.Response(driver.driver.now, receipt);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(receipt));
        driver.driver.Tick();
    }

    void SendBackendData(TestDriver& driver, SNOracle<Config256>& oracle,
                         const REQ& downstream, bool separated)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            TestDriver::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = downstream.TxnID();
            data.Opcode() = separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                      : CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = separated ? CHI::Eb::Resps::DataSepResp_UC
                                    : CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * 2;
            data.CCID() = (downstream.Addr() >> 4) & 3;
            data.BE() = ~uint64_t(0);
            oracle.Data(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
    }

    void SendDirectData(TestDriver& driver, SNOracle<Config256>& oracle,
                        TransactionId originalTxnId, TransactionId downstreamTxnId)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            TestDriver::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 4;
            data.HomeNID() = 2;
            data.TxnID() = originalTxnId;
            data.DBID() = downstreamTxnId;
            data.Opcode() = CHI::Eb::Opcodes::DAT::DataSepResp;
            data.Resp() = CHI::Eb::Resps::DataSepResp_UC;
            data.DataID() = beat * 2;
            data.CCID() = 0;
            data.BE() = ~uint64_t(0);
            oracle.Data(driver.driver.now, data);
            driver.Oracle(4).Data(driver.driver.now, data);
        }
    }

    void ConsumeUpstreamRead(TestDriver& driver, bool separated)
    {
        DataBufferId ackId = 0;
        if (separated)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
            ackId = response.DBID();
        }
        else
            driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);

        const uint16_t opcode = separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                          : CHI::Eb::Opcodes::DAT::CompData;
        for (size_t beat = 0; beat < 2; ++beat)
            driver.NextDAT(opcode);
        if (separated)
            driver.CompAck(4, ackId);
    }

    void CompleteUnorderedRead(TestDriver& driver, SNOracle<Config256>& oracle,
                               TransactionId txnId, uint64_t address)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, txnId, address, false));
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(downstream.Order() == 0);
        oracle.Request(driver.driver.now, downstream);
        SendBackendData(driver, oracle, downstream, false);
        for (size_t beat = 0; beat < 2; ++beat)
            driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    }

    void RunOrderedReadLifetime(bool separated, size_t unrelatedReads)
    {
        TestDriver driver(BackendOptions(separated));
        SNOracle<Config256> oracle(2, 6);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x26000, separated);
        request.Order() = 2;
        driver.PushRequest(request);

        const auto downstream = driver.driver.NextREQ(separated ? CHI::Eb::Opcodes::REQ::ReadNoSnpSep
                                                               : CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(downstream.Order() == 1);
        oracle.Request(driver.driver.now, downstream);
        SendBackendData(driver, oracle, downstream, separated);
        ConsumeUpstreamRead(driver, separated);

        for (size_t index = 0; index < unrelatedReads; ++index)
            CompleteUnorderedRead(driver, oracle, 100 + index, 0x28000 + index * 0x40);

        for (size_t cycle = 0; cycle < 4; ++cycle)
            driver.driver.Tick();
        LUNATIC_CHECK(driver.driver.node.HasInflight());
        SendBackendReceipt(driver, oracle, downstream.TxnID());
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

    void RunDMTCompAckBeforeReceipt(uint8_t order)
    {
        TestDriver driver(BackendOptions(true, true));
        SNOracle<Config256> oracle(2, 6);
        oracle.RegisterRequester(4);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7,
            0x29000 + order * 0x1000, true);
        request.Order() = order;
        driver.PushRequest(request);

        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
        LUNATIC_CHECK(downstream.Order() == 1);
        oracle.Request(driver.driver.now, downstream);
        SendDirectData(driver, oracle, 7, downstream.TxnID());
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
        driver.CompAck(4, response.DBID());

        for (size_t cycle = 0; cycle < 4; ++cycle)
            driver.driver.Tick();
        LUNATIC_CHECK(driver.driver.node.HasInflight());
        SendBackendReceipt(driver, oracle, downstream.TxnID());
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

}

LUNATIC_TEST(Stage9HomeReadOrderedNoSnpWaitsForReceiptAfter65Reads)
{
    RunOrderedReadLifetime(false, 65);
}

LUNATIC_TEST(Stage9HomeReadOrderedSeparatedWaitsForReceiptAfterCompAck)
{
    RunOrderedReadLifetime(true, 0);
}

LUNATIC_TEST(Stage9HomeReadUnorderedDMTWaitsForReceiptAfterCompAck)
{
    RunDMTCompAckBeforeReceipt(0);
}

LUNATIC_TEST(Stage9HomeReadOrderedDMTWaitsForReceiptAfterCompAck)
{
    RunDMTCompAckBeforeReceipt(2);
}

LUNATIC_TEST(Stage9HomeReadUnorderedCompletesWithoutReceipt)
{
    TestDriver driver(BackendOptions(false));
    SNOracle<Config256> oracle(2, 6);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 9, 0x2b000, false));
    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.Order() == 0);
    oracle.Request(driver.driver.now, downstream);
    SendBackendData(driver, oracle, downstream, false);
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage9HomeReadReceiptPredicateHonorsRetryAck)
{
    FlowState flow{};
    flow.backendRequest.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    flow.backendRequest.order = 1;
    LUNATIC_CHECK(RequiresBackendReadReceipt(flow));

    flow.backendRetryAckReceived = true;
    LUNATIC_CHECK(!RequiresBackendReadReceipt(flow));

    flow.backendRetryAckReceived = false;
    flow.backendReadReceiptReceived = true;
    LUNATIC_CHECK(!RequiresBackendReadReceipt(flow));

    flow.backendReadReceiptReceived = false;
    flow.backendRequest.order = 0;
    LUNATIC_CHECK(!RequiresBackendReadReceipt(flow));

    flow.backendRequest.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnpSep;
    flow.backendRequest.order = 1;
    LUNATIC_CHECK(RequiresBackendReadReceipt(flow));
}
