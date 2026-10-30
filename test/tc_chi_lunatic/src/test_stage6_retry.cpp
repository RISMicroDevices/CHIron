#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void CompleteRead(CheckedDriver<config>& driver, uint16_t txnId)
    {
        for (size_t beat = 0; beat < LINE_SIZE / (config::dataWidth / 8); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == txnId);
        }
    }

    template<typename config>
    void RunCreditReturn()
    {
        Options options;
        options.transactionLimit = 1;
        CheckedDriver<config> driver(options);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));
        driver.driver.Tick();
        auto retried = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x20000, false);
        driver.PushRequest(retried);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
        CompleteRead(driver, 1);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

        typename CheckedDriver<config>::REQ request{};
        request.SrcID() = 4;
        request.TgtID() = 2;
        request.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
        request.TxnID() = 0;
        request.PCrdType() = retried.PCrdType();
        driver.PushRequest(request);
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage6RetryChangedTxnIDResend)
{
    Options options;
    options.transactionLimit = 1;
    CheckedDriver<Config256> driver(options);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));
    driver.driver.Tick();
    auto retried = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x20000, false);
    driver.PushRequest(retried);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    CompleteRead(driver, 1);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    retried.TxnID() = 3;
    retried.AllowRetry() = 0;
    driver.PushRequest(retried);
    CompleteRead(driver, 3);
    driver.Quiesce();
}

LUNATIC_TEST(Stage6RetryYoungestCompatibleResend)
{
    Options options;
    options.transactionLimit = 2;
    CheckedDriver<Config256> driver(options);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));
    driver.driver.Tick();
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x18000, false));
    driver.driver.Tick();
    auto older = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 3, 0x20000, false);
    auto younger = older;
    younger.TxnID() = 4;
    driver.PushRequest(older);
    driver.PushRequest(younger);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    CompleteRead(driver, 1);
    CompleteRead(driver, 2);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    younger.TxnID() = 5;
    younger.AllowRetry() = 0;
    driver.PushRequest(younger);
    CompleteRead(driver, 5);
    older.TxnID() = 6;
    older.AllowRetry() = 0;
    driver.PushRequest(older);
    CompleteRead(driver, 6);
    driver.Quiesce();
}

LUNATIC_TEST(Stage6RetryPCrdReturnHasNoResponse)
{
    RunCreditReturn<Config256>();
}

namespace {

    template<typename config>
    void RunBackendRetry(bool grantFirst)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        CheckedDriver<config> driver(options);
        SNOracle<config> oracle(2, 6);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        oracle.Request(driver.driver.now, downstream);

        typename CheckedDriver<config>::RSP retry{};
        retry.SrcID() = 6;
        retry.TgtID() = 2;
        retry.TxnID() = downstream.TxnID();
        retry.Opcode() = CHI::Eb::Opcodes::RSP::RetryAck;
        retry.PCrdType() = downstream.PCrdType();
        typename CheckedDriver<config>::RSP grant{};
        grant.SrcID() = 6;
        grant.TgtID() = 2;
        grant.Opcode() = CHI::Eb::Opcodes::RSP::PCrdGrant;
        grant.PCrdType() = downstream.PCrdType();

        if (grantFirst)
        {
            oracle.Response(driver.driver.now, grant);
            LUNATIC_CHECK(driver.driver.node.PushRXRSP(grant));
            driver.driver.Tick();
        }
        oracle.Response(driver.driver.now, retry);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(retry));
        driver.driver.Tick();
        if (!grantFirst)
        {
            oracle.Response(driver.driver.now, grant);
            LUNATIC_CHECK(driver.driver.node.PushRXRSP(grant));
            driver.driver.Tick();
        }

        const auto resent = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(resent.AllowRetry() == 0);
        LUNATIC_CHECK(resent.TxnID() == downstream.TxnID());
        oracle.Request(driver.driver.now, resent);
        for (size_t beat = 0; beat < LINE_SIZE / (config::dataWidth / 8); ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = resent.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * (config::dataWidth / 8) / 16;
            data.CCID() = (resent.Addr() >> 4) & 3;
            oracle.Data(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        CompleteRead(driver, 1);
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

}

LUNATIC_TEST(Stage6RetryBackendGrantAfterRetryAck)
{
    RunBackendRetry<Config256>(false);
}

LUNATIC_TEST(Stage6RetryBackendGrantBeforeRetryAck)
{
    RunBackendRetry<Config256>(true);
}
