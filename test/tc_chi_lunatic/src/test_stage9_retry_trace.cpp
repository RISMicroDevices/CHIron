#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void CompleteTraceRead(CheckedDriver<config>& driver, TransactionId txnId, uint8_t traceTag)
    {
        for (size_t beat = 0; beat < BeatsPerLine(driver.driver.node.GetOptions()); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == txnId);
            LUNATIC_CHECK(data.TraceTag() == traceTag);
        }
    }

    template<typename config>
    void ReturnTraceCredit(CheckedDriver<config>& driver, NodeId source, uint8_t creditType)
    {
        typename CheckedDriver<config>::REQ request{};
        request.SrcID() = source;
        request.TgtID() = driver.home;
        request.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
        request.PCrdType() = creditType;
        driver.PushRequest(request);
    }

}

LUNATIC_TEST(Stage9RetryGrantTraceTagDelayed)
{
    Options options;
    options.outputQueueLimit = 1;
    CheckedDriver<Config256> driver(options);
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("trace-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry)
                event.RequestRetry();
        }));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x5b000, false);
    request.TraceTag() = 1;
    driver.PushRequest(request);
    driver.driver.Tick();
    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        const auto pending = driver.driver.node.PeekTXRSP();
        LUNATIC_CHECK(pending);
        LUNATIC_CHECK(pending->Opcode() == CHI::Eb::Opcodes::RSP::RetryAck);
        LUNATIC_CHECK(pending->TraceTag() == 1);
        driver.driver.Tick();
    }
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck).TraceTag() == 1);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant).TraceTag() == 1);

    request.TxnID() = 9;
    request.AllowRetry() = 0;
    request.TraceTag() = 0;
    driver.PushRequest(request);
    CompleteTraceRead(driver, 9, 0);
    driver.Quiesce();
}

LUNATIC_TEST(Stage9RetryGrantTraceTagMultipleOriginals)
{
    Options options;
    options.transactionLimit = 2;
    CheckedDriver<Config256> driver(options);
    driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("trace-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry)
                event.RequestRetry();
        }));

    auto older = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 3, 0x5c000, false);
    auto younger = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 4, 0x5c040, false);
    older.TraceTag() = 0;
    younger.TraceTag() = 1;
    driver.PushRequest(older);
    driver.driver.Tick();
    driver.PushRequest(younger);
    driver.driver.Tick();
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck).TxnID() == 3);
    const auto taggedAck = driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(taggedAck.TxnID() == 4);
    LUNATIC_CHECK(taggedAck.TraceTag() == 1);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant).TraceTag() == 1);

    younger.TxnID() = 5;
    younger.AllowRetry() = 0;
    younger.TraceTag() = 0;
    driver.PushRequest(younger);
    CompleteTraceRead(driver, 5, 0);
    ReturnTraceCredit(driver, 4, older.PCrdType());
    driver.Quiesce();
}
