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

LUNATIC_TEST(Stage8RetryCreditFromOtherOriginal)
{
    Options options;
    options.transactionLimit = 1;
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

    younger.AllowRetry() = 0;
    driver.PushRequest(younger);
    CompleteRead(driver, 4);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    older.AllowRetry() = 0;
    driver.PushRequest(older);
    CompleteRead(driver, 3);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().creditedResends == 2);
}

LUNATIC_TEST(Stage8RetryCreditFromOriginalWithQueuedGrant)
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

    younger.AllowRetry() = 0;
    driver.PushRequest(younger);
    CompleteRead(driver, 4);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    older.AllowRetry() = 0;
    driver.PushRequest(older);
    CompleteRead(driver, 3);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().emittedCreditGrants == 2);
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().creditedResends == 2);
}

LUNATIC_TEST(Stage8RetryGrantAndResendProgressPastFullTracker)
{
    Options options;
    options.transactionLimit = 1;
    options.inputQueueLimit = 4;
    options.outputQueueLimit = 4;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x1c000, false);
    const LineData data{};

    driver.ReadData(5, 1, line.address, CHI::Eb::Opcodes::REQ::ReadUnique);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 4, 2, line.address, true));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 5);

    auto retried = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 3, 0x20000, false);
    driver.PushRequest(retried);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    retried.AllowRetry() = 0;
    driver.PushRequest(retried);
    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        driver.driver.Tick();
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
        LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
    }

    driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I);
    const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.TgtID() == 4);
    driver.CompAck(4, comp.DBID());
    CompleteRead(driver, 3);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().creditedResends == 1);
}

LUNATIC_TEST(Stage8RetryCreditReturnPreservesOtherOriginal)
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

    ReturnCredit(driver, 4, older.PCrdType());
    younger.AllowRetry() = 0;
    driver.PushRequest(younger);
    CompleteRead(driver, 4);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().returnedCredits == 1);
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().creditedResends == 1);
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

LUNATIC_TEST(Stage8RetryChangedTraceTagAndTargetResend)
{
    Driver<Config256> driver;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("force-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry && event.request.txnId == 2)
                event.RequestRetry();
        }));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x20000, false);
    request.TgtID() = 3;
    request.TraceTag() = 0;
    driver.node.PushRXREQ(request);
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    request.TxnID() = 9;
    request.AllowRetry() = 0;
    request.TraceTag() = 1;
    request.TgtID() = 2;
    driver.node.PushRXREQ(request);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TxnID() == 9);
        LUNATIC_CHECK(data.TraceTag() == 1);
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().creditedResends == 1);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage8RetryArbitraryTargetDoesNotConsumeCredit)
{
    Driver<Config256> driver;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("force-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry && event.request.txnId == 2)
                event.RequestRetry();
        }));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x20000, false);
    driver.node.PushRXREQ(request);
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    request.TxnID() = 9;
    request.AllowRetry() = 0;
    request.TgtID() = 3;
    driver.node.PushRXREQ(request);
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().creditedResends == 0);

    Driver<Config256>::REQ creditReturn{};
    creditReturn.SrcID() = 4;
    creditReturn.TgtID() = 2;
    creditReturn.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
    creditReturn.PCrdType() = 0;
    driver.node.PushRXREQ(creditReturn);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().returnedCredits == 1);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
