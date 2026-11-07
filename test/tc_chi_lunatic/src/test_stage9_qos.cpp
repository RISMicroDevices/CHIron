#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using RSP        = TestDriver::RSP;
    using DAT        = TestDriver::DAT;

    Options CHIBackendOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        return options;
    }

    void SendBackendResponse(TestDriver& driver, const TestDriver::REQ& request, uint16_t opcode, uint8_t creditType = 0)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.PCrdType() = creditType;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SendBackendReadData(TestDriver& driver, const TestDriver::REQ& request)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = request.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * 2;
            data.CCID() = 0;
            data.BE() = 0xffffffff;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

}

LUNATIC_TEST(Stage9QoSChangedRetryPropagates)
{
    TestDriver driver(CHIBackendOptions());
    bool retry = true;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("qos-retry", 0, [&](RequestAdmissionEvent& event) {
            if (retry && event.request.txnId == 3 && event.request.allowRetry)
            {
                retry = false;
                event.RequestRetry();
            }
        }));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 3, 0x6f000);
    request.QoS() = 3;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto retryAck = driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(retryAck.QoS() == 3);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    request.TxnID() = 4;
    request.AllowRetry() = 0;
    request.PCrdType() = retryAck.PCrdType();
    request.QoS() = 12;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.QoS() == 12);

    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::RetryAck, 2);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::PCrdGrant, 2);
    const auto resent = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(resent.QoS() == 12 && resent.PCrdType() == 2 && !resent.AllowRetry());
    SendBackendReadData(driver, resent);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.QoS() == 12 && data.TxnID() == 4);
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(!retry);
}

LUNATIC_TEST(Stage9QoSWriteAndBackendPropagation)
{
    TestDriver driver(CHIBackendOptions());
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 9, 0x6f100);
    request.QoS() = 15;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto dbidResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(dbidResponse.QoS() == 15);
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 9, dbidResponse.DBID(), LineData{}, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);

    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(downstream.QoS() == 15);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::DBIDResp);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData).QoS() == 15);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).QoS() == 15);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9QoSIndependentRepetitionAndCreditReturn)
{
    Options options;
    options.memoryLatency = 16;
    TestDriver driver(options);
    bool retry = true;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("qos-repetition-retry", 0, [&](RequestAdmissionEvent& event) {
            if (retry && event.request.txnId == 21 && event.request.allowRetry)
            {
                retry = false;
                event.RequestRetry();
            }
        }));

    auto original = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 20, 0x71000);
    original.QoS() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(original));
    driver.Tick();

    auto retried = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 21, 0x71000);
    retried.QoS() = 15;
    LUNATIC_CHECK(driver.node.PushRXREQ(retried));
    const auto retryAck = driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(retryAck.TxnID() == 21 && retryAck.QoS() == 15);
    LUNATIC_CHECK(!retry);

    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TxnID() == 20 && data.QoS() == 0);
    }

    const auto grant = driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    LUNATIC_CHECK(grant.TxnID() == 0 && grant.PCrdType() == retryAck.PCrdType());
    TestDriver::REQ creditReturn{};
    creditReturn.SrcID() = 4;
    creditReturn.TgtID() = 2;
    creditReturn.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
    creditReturn.PCrdType() = retryAck.PCrdType();
    creditReturn.QoS() = 7;
    LUNATIC_CHECK(driver.node.PushRXREQ(creditReturn));
    driver.Quiesce();

    const auto& counters = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(counters.emittedRetryAcks == 1);
    LUNATIC_CHECK(counters.returnedCredits == 1);
    LUNATIC_CHECK(counters.admittedTransactions == 1);
    LUNATIC_CHECK(counters.retiredTransactions == 1);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
