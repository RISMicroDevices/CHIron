#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void CompleteRead(CheckedDriver<config>& driver, TransactionId txnId)
    {
        for (size_t beat = 0; beat < BeatsPerLine(driver.driver.node.GetOptions()); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == txnId);
        }
    }

    template<typename config>
    void ReturnCredit(CheckedDriver<config>& driver, NodeId source, uint8_t creditType)
    {
        typename CheckedDriver<config>::REQ request{};
        request.SrcID() = source;
        request.TgtID() = driver.home;
        request.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
        request.PCrdType() = creditType;
        driver.PushRequest(request);
    }

}

LUNATIC_TEST(Stage8RetryForcedWithoutResourceExhaustion)
{
    CheckedDriver<Config256> driver;
    bool retry = true;
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("force-retry", 0, [&](RequestAdmissionEvent& event) {
            if (retry && event.request.allowRetry && event.request.txnId == 2)
            {
                retry = false;
                event.RequestRetry();
            }
        }));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x12000, false);
    driver.PushRequest(request);
    driver.driver.Tick();
    LUNATIC_CHECK(!retry);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    request.TxnID() = 9;
    request.AllowRetry() = 0;
    driver.PushRequest(request);
    CompleteRead(driver, 9);
    driver.Quiesce();
}

LUNATIC_TEST(Stage8RetryAckBackpressureAndCreditReturn)
{
    Options options;
    options.outputQueueLimit = 1;
    CheckedDriver<Config256> driver(options);
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("force-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry)
                event.RequestRetry();
        }));

    const auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x12000, false);
    driver.PushRequest(request);
    driver.driver.Tick();
    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        const auto pending = driver.driver.node.PeekTXRSP();
        LUNATIC_CHECK(pending);
        LUNATIC_CHECK(pending->Opcode() == CHI::Eb::Opcodes::RSP::RetryAck);
        driver.driver.Tick();
    }

    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    ReturnCredit(driver, 4, request.PCrdType());
    driver.Quiesce();
}

LUNATIC_TEST(Stage8RetryRejectsCreditedResendDecision)
{
    CheckedDriver<Config256> driver;
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("force-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.txnId == 2 || event.request.txnId == 9)
                event.RequestRetry();
        }));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x12000, false);
    driver.PushRequest(request);
    driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    request.TxnID() = 9;
    request.AllowRetry() = 0;
    driver.PushRequest(request);
    CompleteRead(driver, 9);
    driver.driver.Quiesce();
    for (const auto& [requester, oracle] : driver.oracles)
    {
        (void)requester;
        LUNATIC_CHECK(!oracle->HasInflight());
    }
    LUNATIC_CHECK(driver.driver.node.ValidateInvariants());
    LUNATIC_CHECK(driver.driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.driver.node.GetErrors().front().reason.find("retry decision") != std::string::npos);
}

LUNATIC_TEST(Stage8RetryMultipleForcedCredits)
{
    Options options;
    options.transactionLimit = 2;
    CheckedDriver<Config256> driver(options);
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("force-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry)
                event.RequestRetry();
        }));

    auto older = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 3, 0x14000, false);
    auto younger = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 4, 0x18000, false);
    driver.PushRequest(older);
    driver.driver.Tick();
    driver.PushRequest(younger);
    driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    younger.TxnID() = 5;
    younger.AllowRetry() = 0;
    driver.PushRequest(younger);
    CompleteRead(driver, 5);
    ReturnCredit(driver, 4, older.PCrdType());
    driver.Quiesce();
}

LUNATIC_TEST(Stage8RetryBackendWithFlowDecisions)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.randomDelayLimit = 1;
    options.randomizeArbitration = true;
    options.seed = 0x42;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    bool block = true;
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("backend-backpressure", 0, [&](RequestAdmissionEvent& event) {
            if (block)
            {
                block = false;
                event.Backpressure();
            }
        }));
    driver.driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("backend-delay", 0, [&](TransactionScheduleEvent& event) {
            if (event.phase == Phase::Action)
                event.AddDelay(1);
        }));

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x1c000, false));
    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    oracle.Request(driver.driver.now, downstream);
    typename CheckedDriver<Config256>::RSP retry{};
    retry.SrcID() = 6;
    retry.TgtID() = 2;
    retry.TxnID() = downstream.TxnID();
    retry.Opcode() = CHI::Eb::Opcodes::RSP::RetryAck;
    retry.PCrdType() = downstream.PCrdType();
    typename CheckedDriver<Config256>::RSP grant{};
    grant.SrcID() = 6;
    grant.TgtID() = 2;
    grant.Opcode() = CHI::Eb::Opcodes::RSP::PCrdGrant;
    grant.PCrdType() = downstream.PCrdType();

    oracle.Response(driver.driver.now, retry);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(retry));
    driver.driver.Tick();
    oracle.Response(driver.driver.now, grant);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(grant));
    driver.driver.Tick();
    const auto resent = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(resent.AllowRetry() == 0);
    LUNATIC_CHECK(resent.TxnID() == downstream.TxnID());
    oracle.Request(driver.driver.now, resent);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        typename CheckedDriver<Config256>::DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = resent.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_I;
        data.DataID() = beat * 2;
        data.CCID() = (resent.Addr() >> 4) & 3;
        oracle.Data(driver.driver.now, data);
        LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
    }
    CompleteRead(driver, 1);
    driver.Quiesce();
    LUNATIC_CHECK(!block);
    LUNATIC_CHECK(!oracle.HasInflight());
}
