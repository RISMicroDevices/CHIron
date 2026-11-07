#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void SendDirectDMTData(CheckedDriver<Config256>& driver, SNOracle<Config256>& oracle,
                           TransactionId originalTxnId, TransactionId downstreamTxnId)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            CheckedDriver<Config256>::DAT data{};
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

    void SendBackendReceipt(CheckedDriver<Config256>& driver, SNOracle<Config256>& oracle,
                            TransactionId downstreamTxnId)
    {
        CheckedDriver<Config256>::RSP receipt{};
        receipt.SrcID() = 6;
        receipt.TgtID() = 2;
        receipt.TxnID() = downstreamTxnId;
        receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
        oracle.Response(driver.driver.now, receipt);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(receipt));
        driver.driver.Tick();
    }

    void CompleteCopyback(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId, uint64_t address)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, source, txnId, address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        ByteEnables enables{};
        enables.fill(1);
        driver.WriteData(source, txnId, response.DBID(), LineData{}, enables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    }

}

LUNATIC_TEST(Stage9SeparatedDMTReleasesTxnIDBeforeBackendReceipt)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dmtEnabled = true;
    options.separatedReadResponse = true;
    options.separatedBackendRead = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x72000, true);
    request.Order() = 2;
    driver.PushRequest(request);
    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
    LUNATIC_CHECK(downstream.ReturnNID() == 4 && downstream.ReturnTxnID() == 7);
    oracle.Request(driver.driver.now, downstream);
    SendDirectDMTData(driver, oracle, 7, downstream.TxnID());

    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    CompleteCopyback(driver, 4, 7, 0x72040);
    driver.CompAck(4, response.DBID());
    SendBackendReceipt(driver, oracle, downstream.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
    LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9RetryAckAllowsImmediateOriginalTxnIDReuse)
{
    Options options;
    CheckedDriver<Config256> driver(options);
    bool retry = true;
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("direct-id-retry", 0, [&](RequestAdmissionEvent& event) {
            if (retry && event.request.allowRetry && event.request.txnId == 7)
            {
                retry = false;
                event.RequestRetry();
            }
        }));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x72100);
    driver.PushRequest(request);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck).TxnID() == 7);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    CompleteCopyback(driver, 4, 7, 0x72140);


    request.TxnID() = 9;
    request.AllowRetry() = 0;
    driver.PushRequest(request);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).TxnID() == 9);
    driver.Quiesce();
    LUNATIC_CHECK(!retry);
    LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9DWTReleasesTxnIDBeforeBackendComp)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dwtEnabled = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);
    driver.Oracle(4).RegisterSubordinate(6);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, 0, 0x72200));
    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(downstream.DoDWT() == 1 && downstream.ReturnNID() == 4 && downstream.ReturnTxnID() == 0);
    oracle.Request(driver.driver.now, downstream);

    CheckedDriver<Config256>::RSP directDBID{};
    directDBID.SrcID() = 6;
    directDBID.TgtID() = 4;
    directDBID.TxnID() = 0;
    directDBID.DBID() = 77;
    directDBID.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
    driver.Oracle(4).Response(driver.driver.now, directDBID);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        CheckedDriver<Config256>::DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 6;
        data.HomeNID() = 0;
        data.TxnID() = 77;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = beat * 2;
        data.CCID() = 0;
        data.BE() = ~uint64_t(0);
        driver.Oracle(4).WriteData(driver.driver.now, data);
    }

    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 0);
    CompleteCopyback(driver, 4, 0, 0x72240);

    CheckedDriver<Config256>::RSP backendComp{};
    backendComp.SrcID() = 6;
    backendComp.TgtID() = 2;
    backendComp.TxnID() = downstream.TxnID();
    backendComp.DBID() = 99;
    backendComp.Resp() = CHI::Eb::Resps::Comp_I;
    backendComp.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    oracle.Response(driver.driver.now, backendComp);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(backendComp));
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
}
