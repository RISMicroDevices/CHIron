#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void SendDMTData(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                     TransactionId originalTxnId, TransactionId downstreamTxnId, uint16_t opcode)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 4;
            data.HomeNID() = 2;
            data.TxnID() = originalTxnId;
            data.DBID() = downstreamTxnId;
            data.Opcode() = opcode;
            data.Resp() = opcode == CHI::Eb::Opcodes::DAT::CompData
                ? CHI::Eb::Resps::CompData_I : CHI::Eb::Resps::DataSepResp_UC;
            data.DataID() = beat * bytes / 16;
            data.CCID() = 0;
            oracle.Data(driver.driver.now, data);
            driver.Oracle(4).Data(driver.driver.now, data);
        }
    }

    template<typename config>
    void SendBackendReceipt(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                            TransactionId downstreamTxnId)
    {
        typename CheckedDriver<config>::RSP receipt{};
        receipt.SrcID() = 6;
        receipt.TgtID() = 2;
        receipt.TxnID() = downstreamTxnId;
        receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
        oracle.Response(driver.driver.now, receipt);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(receipt));
        driver.driver.Tick();
    }

}

LUNATIC_TEST(Stage6DMTUnorderedEarlyDeallocation)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dmtEnabled = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.ReturnNID() == 4);
    LUNATIC_CHECK(downstream.ReturnTxnID() == 1);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, downstream);
    SendBackendReceipt(driver, oracle, downstream.TxnID());
    SendDMTData(driver, oracle, 1, downstream.TxnID(), CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
    LUNATIC_CHECK(!driver.driver.node.InspectHome(LineKey::FromAddress(0x10000, false)).valid);
}

LUNATIC_TEST(Stage6DMTSeparatedCompAckAndLateReceipt)
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
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, true);
    request.Order() = 2;
    driver.PushRequest(request);

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
    LUNATIC_CHECK(downstream.ReturnNID() == 4);
    LUNATIC_CHECK(downstream.ReturnTxnID() == 1);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, downstream);
    SendDMTData(driver, oracle, 1, downstream.TxnID(), CHI::Eb::Opcodes::DAT::DataSepResp);

    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    LUNATIC_CHECK(response.Resp() == 0);
    driver.CompAck(4, response.DBID());
    SendBackendReceipt(driver, oracle, downstream.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
    LUNATIC_CHECK(!driver.driver.node.InspectHome(LineKey::FromAddress(0x10000, false)).valid);
}
