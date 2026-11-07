#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;
    using REQ = TestDriver::REQ;
    using RSP = TestDriver::RSP;
    using DAT = TestDriver::DAT;

    Options RemapOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        return options;
    }

    REQ Remapped(const REQ& original)
    {
        LUNATIC_CHECK(original.TgtID() == 6);
        auto request = original;
        request.TgtID() = 9;
        return request;
    }

    void SendResponse(TestDriver& driver, SNOracle<Config256>& oracle, const RSP& response)
    {
        oracle.Response(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

    void SendReadData(TestDriver& driver, SNOracle<Config256>& oracle, const REQ& downstream)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 9;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = downstream.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * 2;
            data.CCID() = (downstream.Addr() >> 4) & 3;
            data.BE() = ~uint64_t(0);
            oracle.Data(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
    }

    RSP MakeResponse(uint16_t opcode, TransactionId txnId, DataBufferId dbid = 0, uint8_t creditType = 0)
    {
        RSP response{};
        response.SrcID() = 9;
        response.TgtID() = 2;
        response.TxnID() = txnId;
        response.DBID() = dbid;
        response.Opcode() = opcode;
        response.Resp() = opcode == CHI::Eb::Opcodes::RSP::Comp ? uint8_t(CHI::Eb::Resps::Comp_I) : uint8_t(0);
        response.PCrdType() = creditType;
        return response;
    }

    void RunRemappedRetry(bool grantFirst)
    {
        TestDriver driver(RemapOptions());
        SNOracle<Config256> oracle(2, 9);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x2c000, false));
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        oracle.Request(driver.driver.now, Remapped(downstream));

        const auto retry = MakeResponse(CHI::Eb::Opcodes::RSP::RetryAck, downstream.TxnID(), 0, 3);
        const auto grant = MakeResponse(CHI::Eb::Opcodes::RSP::PCrdGrant, 0, 0, 3);
        if (grantFirst)
            SendResponse(driver, oracle, grant);
        SendResponse(driver, oracle, retry);
        if (!grantFirst)
            SendResponse(driver, oracle, grant);

        const auto resent = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(!resent.AllowRetry() && resent.PCrdType() == 3);
        LUNATIC_CHECK(resent.TgtID() == 6);
        oracle.Request(driver.driver.now, Remapped(resent));
        SendReadData(driver, oracle, resent);
        for (size_t beat = 0; beat < 2; ++beat)
            driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

}

LUNATIC_TEST(Stage9BackendRemapOrderedReadAndReceipt)
{
    TestDriver driver(RemapOptions());
    SNOracle<Config256> oracle(2, 9);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x2c100, false);
    request.Order() = 2;
    driver.PushRequest(request);
    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, Remapped(downstream));

    driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);
    SendReadData(driver, oracle, downstream);
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    for (size_t cycle = 0; cycle < 4; ++cycle)
        driver.driver.Tick();
    LUNATIC_CHECK(driver.driver.node.HasInflight());

    SendResponse(driver, oracle,
        MakeResponse(CHI::Eb::Opcodes::RSP::ReadReceipt, downstream.TxnID()));
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage9BackendRemapWriteUsesDBIDResponder)
{
    TestDriver driver(RemapOptions());
    SNOracle<Config256> oracle(2, 9);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 2, 0x2c200));
    const auto requesterDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 2, requesterDBID.DBID(), LineData{}, enables,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    oracle.Request(driver.driver.now, Remapped(downstream));

    SendResponse(driver, oracle,
        MakeResponse(CHI::Eb::Opcodes::RSP::DBIDResp, downstream.TxnID(), 77));
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(data.TgtID() == 9 && data.TxnID() == 77 && data.DBID() == 0);
        oracle.WriteData(driver.driver.now, data);
    }
    SendResponse(driver, oracle,
        MakeResponse(CHI::Eb::Opcodes::RSP::Comp, downstream.TxnID(), 77));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage9BackendRemapAtomicStoreUsesDBIDResponder)
{
    TestDriver driver(RemapOptions());
    SNOracle<Config256> oracle(2, 9);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 4, 0x2c400);
    request.Size() = 2;
    driver.PushRequest(request);
    const auto requesterDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);

    LineData operands{};
    operands[0] = 4;
    operands[1] = 3;
    operands[2] = 2;
    operands[3] = 1;
    ByteEnables enables{};
    for (size_t index = 0; index < 4; ++index)
        enables[index] = 1;
    driver.WriteData(4, 4, requesterDBID.DBID(), operands, enables,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData, 0, 1);

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::AtomicStore::ADD);
    oracle.Request(driver.driver.now, Remapped(downstream));
    SendResponse(driver, oracle,
        MakeResponse(CHI::Eb::Opcodes::RSP::DBIDResp, downstream.TxnID(), 77));
    const auto operandData = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(operandData.TgtID() == 9 && operandData.TxnID() == 77);
    oracle.WriteData(driver.driver.now, operandData);

    SendResponse(driver, oracle,
        MakeResponse(CHI::Eb::Opcodes::RSP::Comp, downstream.TxnID(), 77));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage9BackendRemapDMTDirectDataAndLateReceipt)
{
    auto options = RemapOptions();
    options.dmtEnabled = true;
    options.separatedReadResponse = true;
    options.separatedBackendRead = true;
    TestDriver driver(options);
    SNOracle<Config256> oracle(2, 9);
    oracle.RegisterRequester(4);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x2c500, true);
    request.Order() = 2;
    driver.PushRequest(request);
    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, Remapped(downstream));

    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT data{};
        data.SrcID() = 9;
        data.TgtID() = 4;
        data.HomeNID() = 2;
        data.TxnID() = 7;
        data.DBID() = downstream.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::DataSepResp;
        data.Resp() = CHI::Eb::Resps::DataSepResp_UC;
        data.DataID() = beat * 2;
        data.CCID() = 0;
        data.BE() = ~uint64_t(0);
        oracle.Data(driver.driver.now, data);
        driver.Oracle(4).Data(driver.driver.now, data);
    }
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    driver.CompAck(4, response.DBID());
    for (size_t cycle = 0; cycle < 4; ++cycle)
        driver.driver.Tick();
    LUNATIC_CHECK(driver.driver.node.HasInflight());

    SendResponse(driver, oracle,
        MakeResponse(CHI::Eb::Opcodes::RSP::ReadReceipt, downstream.TxnID()));
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage9BackendCreditReturnUsesRecordedResponder)
{
    BackendRetryRecord retry{};
    retry.creditSource = 9;
    retry.retryAckPCrdType = 3;
    retry.request.qos = 12;
    retry.request.traceTag = 1;

    const auto request = MakeBackendCreditReturnRequest(retry, 2);
    LUNATIC_CHECK(request.source == 2 && request.target == 9);
    LUNATIC_CHECK(request.opcode == CHI::Eb::Opcodes::REQ::PCrdReturn);
    LUNATIC_CHECK(request.txnId == 0 && request.pCrdType == 3);
    LUNATIC_CHECK(request.qos == 12 && request.traceTag == 1);
}

LUNATIC_TEST(Stage9BackendRemapRetryGrantAfterRetryAck)
{
    RunRemappedRetry(false);
}

LUNATIC_TEST(Stage9BackendRemapRetryGrantBeforeRetryAck)
{
    RunRemappedRetry(true);
}
