#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Request OrderedRead(NodeId source, uint64_t address, bool nonSecure = false)
    {
        Request request{};
        request.source = source;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.address = address;
        request.nonSecure = nonSecure;
        request.order = 2;
        return request;
    }

    Request OrderedWrite(NodeId source, uint64_t address)
    {
        Request request{};
        request.source = source;
        request.opcode = CHI::Eb::Opcodes::REQ::WriteNoSnpFull;
        request.address = address;
        request.order = 2;
        request.expCompAck = true;
        return request;
    }

}

LUNATIC_TEST(Stage9EndpointOrderTrackerUsesLineAndNamespace)
{
    OrderingTracker tracker;
    Transaction first(0, OrderedRead(4, 0x10000), 0);
    Transaction sameLine(1, OrderedRead(4, 0x10020), 0);
    Transaction otherLine(2, OrderedRead(4, 0x10040), 0);
    Transaction otherNamespace(3, OrderedRead(4, 0x10000, true), 0);
    Transaction otherSource(4, OrderedRead(5, 0x10000), 0);

    LUNATIC_CHECK(tracker.Add(first));
    LUNATIC_CHECK(!tracker.CanAdmit(sameLine.request));
    LUNATIC_CHECK(tracker.CanAdmit(otherLine.request));
    LUNATIC_CHECK(tracker.CanAdmit(otherNamespace.request));
    LUNATIC_CHECK(tracker.CanAdmit(otherSource.request));
    LUNATIC_CHECK(tracker.Add(otherLine));
    LUNATIC_CHECK(tracker.Validate());

    LUNATIC_CHECK(tracker.Release(first.handle));
    LUNATIC_CHECK(tracker.CanAdmit(sameLine.request));
    LUNATIC_CHECK(tracker.Add(sameLine));
    LUNATIC_CHECK(tracker.Release(otherLine.handle));
    LUNATIC_CHECK(tracker.Release(sameLine.handle));
    LUNATIC_CHECK(tracker.Empty());

    LUNATIC_CHECK(tracker.Add(first));
    tracker.Clear();
    LUNATIC_CHECK(tracker.Empty());
}

LUNATIC_TEST(Stage9EndpointOrderTrackerKeepsOWOSourceWide)
{
    OrderingTracker tracker;
    Transaction first(0, OrderedWrite(4, 0x10000), 0);
    Transaction otherLine(1, OrderedWrite(4, 0x20000), 0);

    LUNATIC_CHECK(tracker.Add(first));
    LUNATIC_CHECK(!tracker.CanAdmit(otherLine.request));
    LUNATIC_CHECK(tracker.Release(first.handle));
    LUNATIC_CHECK(tracker.CanAdmit(otherLine.request));
    LUNATIC_CHECK(tracker.Add(otherLine));
    LUNATIC_CHECK(tracker.Release(otherLine.handle));
    LUNATIC_CHECK(tracker.Empty());
}

LUNATIC_TEST(Stage9EndpointOrderDMTUsesLineAndNamespace)
{
    OrderingTracker tracker;
    Transaction first(0, OrderedRead(4, 0x10000), 0);
    Transaction sameLine(1, OrderedRead(4, 0x10020), 0);
    Transaction otherLine(2, OrderedRead(4, 0x10040), 0);
    Transaction otherNamespace(3, OrderedRead(4, 0x10000, true), 0);
    Transaction otherSource(4, OrderedRead(5, 0x10000), 0);

    tracker.HoldDMT(first);
    LUNATIC_CHECK(!tracker.CanIssueDMT(sameLine));
    LUNATIC_CHECK(tracker.CanIssueDMT(otherLine));
    LUNATIC_CHECK(tracker.CanIssueDMT(otherNamespace));
    LUNATIC_CHECK(tracker.CanIssueDMT(otherSource));
    LUNATIC_CHECK(tracker.Validate());

    tracker.ReleaseDMT(first.handle);
    LUNATIC_CHECK(tracker.CanIssueDMT(sameLine));
    LUNATIC_CHECK(tracker.Empty());
}

LUNATIC_TEST(Stage9EndpointOrderIndependentReadLinesQueueTogether)
{
    Options options;
    options.outputQueueLimit = 4;
    CheckedDriver<Config256> driver(options);
    for (uint16_t txnId = 1; txnId <= 2; ++txnId)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, txnId,
            0x10000 + txnId * 0x40, false);
        request.Order() = 2;
        driver.PushRequest(request);
    }

    driver.driver.Until([&] {
        return driver.driver.node.GetPerformanceCounters().admittedTransactions == 2;
    });
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
    const auto first = driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);
    LUNATIC_CHECK(first.TxnID() == 1);
    const auto second = driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);
    LUNATIC_CHECK(second.TxnID() == 2);

    for (size_t index = 0; index < 4; ++index)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
}
