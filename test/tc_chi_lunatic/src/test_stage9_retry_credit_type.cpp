#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    typename CheckedDriver<config>::RSP RetryResponse(
        const typename CheckedDriver<config>::REQ& request, uint16_t opcode, uint8_t creditType)
    {
        typename CheckedDriver<config>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        if (opcode == CHI::Eb::Opcodes::RSP::RetryAck)
            response.TxnID() = request.TxnID();
        response.Opcode() = opcode;
        response.PCrdType() = creditType;
        return response;
    }

    template<typename config>
    void SendRetryResponse(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                           const typename CheckedDriver<config>::RSP& response)
    {
        oracle.Response(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

    template<typename config>
    void CompleteBackendRead(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                             const typename CheckedDriver<config>::REQ& request,
                             TransactionId requesterTxnId)
    {
        for (size_t beat = 0; beat < LINE_SIZE / (config::dataWidth / 8); ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = request.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * (config::dataWidth / 8) / 16;
            data.CCID() = (request.Addr() >> 4) & 3;
            oracle.Data(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        for (size_t beat = 0; beat < LINE_SIZE / (config::dataWidth / 8); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == requesterTxnId);
        }
    }

    void RunSelectedCreditType(bool grantFirst)
    {
        constexpr uint8_t creditType = 3;
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        CheckedDriver<Config256> driver(options);
        SNOracle<Config256> oracle(2, 6);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(downstream.AllowRetry() == 1);
        LUNATIC_CHECK(downstream.PCrdType() == 0);
        oracle.Request(driver.driver.now, downstream);

        const auto retry = RetryResponse<Config256>(downstream, CHI::Eb::Opcodes::RSP::RetryAck, creditType);
        const auto grant = RetryResponse<Config256>(downstream, CHI::Eb::Opcodes::RSP::PCrdGrant, creditType);
        if (grantFirst)
            SendRetryResponse(driver, oracle, grant);
        SendRetryResponse(driver, oracle, retry);
        if (!grantFirst)
            SendRetryResponse(driver, oracle, grant);

        const auto resent = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(resent.AllowRetry() == 0);
        LUNATIC_CHECK(resent.PCrdType() == creditType);
        LUNATIC_CHECK(resent.Addr() == downstream.Addr());
        oracle.Request(driver.driver.now, resent);
        CompleteBackendRead(driver, oracle, resent, 1);
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

}

LUNATIC_TEST(Stage9BackendRetrySelectedCreditTypeAfterRetryAck)
{
    RunSelectedCreditType(false);
}

LUNATIC_TEST(Stage9BackendRetrySelectedCreditTypeBeforeRetryAck)
{
    RunSelectedCreditType(true);
}

LUNATIC_TEST(Stage9BackendRetrySeparatesCreditTypes)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.transactionLimit = 4;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x20000, false));
    const auto first = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    oracle.Request(driver.driver.now, first);
    const auto second = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    oracle.Request(driver.driver.now, second);

    SendRetryResponse(driver, oracle,
        RetryResponse<Config256>(first, CHI::Eb::Opcodes::RSP::RetryAck, 1));
    SendRetryResponse(driver, oracle,
        RetryResponse<Config256>(second, CHI::Eb::Opcodes::RSP::RetryAck, 2));
    SendRetryResponse(driver, oracle,
        RetryResponse<Config256>(second, CHI::Eb::Opcodes::RSP::PCrdGrant, 2));

    const auto resentSecond = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(resentSecond.Addr() == second.Addr());
    LUNATIC_CHECK(resentSecond.AllowRetry() == 0);
    LUNATIC_CHECK(resentSecond.PCrdType() == 2);
    LUNATIC_CHECK(!driver.driver.node.PeekTXREQ());
    oracle.Request(driver.driver.now, resentSecond);
    CompleteBackendRead(driver, oracle, resentSecond, 2);

    SendRetryResponse(driver, oracle,
        RetryResponse<Config256>(first, CHI::Eb::Opcodes::RSP::PCrdGrant, 1));
    const auto resentFirst = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(resentFirst.Addr() == first.Addr());
    LUNATIC_CHECK(resentFirst.AllowRetry() == 0);
    LUNATIC_CHECK(resentFirst.PCrdType() == 1);
    oracle.Request(driver.driver.now, resentFirst);
    CompleteBackendRead(driver, oracle, resentFirst, 1);

    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}
