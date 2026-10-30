#include "sequence.hpp"

using namespace XactTest;

namespace {

    Flits::REQ<config> HomeReadRequest(uint16_t opcode, bool dmt = false,
                                       uint8_t order = 0, uint16_t txnId = 1)
    {
        auto request = Request(opcode, txnId);
        request.SrcID() = 2;
        request.TgtID() = 6;
        request.Order() = order;
        request.SnpAttr() = 0;
        request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
        if (dmt)
        {
            request.ReturnNID() = 4;
            request.ReturnTxnID() = 0x55;
        }
        return request;
    }

    Flits::DAT<config> SubordinateData(uint16_t opcode, uint8_t dataId, bool dmt = false,
                                       uint16_t txnId = 1)
    {
        auto data = Data(6, dmt ? 4 : 2, opcode, dmt ? 0x55 : txnId, dataId, dmt ? txnId : 0);
        data.HomeNID() = 2;
        data.BE() = 0;
        data.Resp() = opcode == Opcodes::DAT::CompData ? Resps::CompData_I : Resps::DataSepResp_I;
        return data;
    }

    Flits::RSP<config> SubordinateReadReceipt(uint16_t txnId = 1)
    {
        return Response(6, 2, Opcodes::RSP::ReadReceipt, txnId);
    }

    uint16_t HomeReadDataOpcode(uint16_t requestOpcode)
    {
        return requestOpcode == Opcodes::REQ::ReadNoSnp ?
            Opcodes::DAT::CompData : Opcodes::DAT::DataSepResp;
    }

    void StartHomeRead(SNFixture& fixture, uint16_t opcode, bool dmt = false,
                       uint8_t order = 0, uint16_t txnId = 1, uint64_t time = 1)
    {
        if (dmt)
            fixture.global.TOPOLOGY.SetNode(4, Xact::NodeType::RN_F);
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, time,
            HomeReadRequest(opcode, dmt, order, txnId)));
    }

    void SendHomeReadData(SNFixture& fixture, uint16_t requestOpcode,
                          uint16_t txnId = 1, uint64_t time = 2)
    {
        const uint16_t dataOpcode = HomeReadDataOpcode(requestOpcode);
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, time,
            SubordinateData(dataOpcode, 0, false, txnId)));
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, time + 2,
            SubordinateData(dataOpcode, 2, false, txnId)));
    }

    void CompleteOrderedHomeRead(SNFixture& fixture, uint16_t requestOpcode, uint16_t txnId = 1)
    {
        StartHomeRead(fixture, requestOpcode, false, 2, txnId, txnId * 8);
        SendHomeReadData(fixture, requestOpcode, txnId, txnId * 8 + 2);
    }

    void RejectWrongDataForm(uint16_t requestOpcode, uint16_t wrongDataOpcode, uint16_t correctDataOpcode)
    {
        SNFixture fixture;
        StartHomeRead(fixture, requestOpcode);

        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 2,
            SubordinateData(wrongDataOpcode, 0)) == XactDenial::DENIED_DAT_OPCODE);
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3,
            SubordinateData(correctDataOpcode, 0)));
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 4,
            SubordinateData(correctDataOpcode, 2)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    void AcceptDMTDataForm(uint16_t requestOpcode, uint16_t dataOpcode)
    {
        SNFixture fixture;
        StartHomeRead(fixture, requestOpcode, true);

        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 2,
            SubordinateData(dataOpcode, 0, true)));
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3,
            SubordinateData(dataOpcode, 2, true)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

}

XACT_TEST(Stage6OracleHomeReadNoSnpRejectsDataSepResp)
{
    RejectWrongDataForm(Opcodes::REQ::ReadNoSnp,
        Opcodes::DAT::DataSepResp, Opcodes::DAT::CompData);
}

XACT_TEST(Stage6OracleHomeReadNoSnpSepRejectsCompData)
{
    RejectWrongDataForm(Opcodes::REQ::ReadNoSnpSep,
        Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp);
}

XACT_TEST(Stage6OracleHomeReadNoSnpDMTAcceptsCompData)
{
    AcceptDMTDataForm(Opcodes::REQ::ReadNoSnp, Opcodes::DAT::CompData);
}

XACT_TEST(Stage6OracleHomeReadNoSnpSepDMTAcceptsDataSepResp)
{
    AcceptDMTDataForm(Opcodes::REQ::ReadNoSnpSep, Opcodes::DAT::DataSepResp);
}

XACT_TEST(Stage6OracleHomeReadOrderedNoSnpCompletesWithoutReadReceipt)
{
    SNFixture fixture;
    CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnp);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleHomeReadOrderedNoSnpSepCompletesWithoutReadReceipt)
{
    SNFixture fixture;
    CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnpSep);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleHomeReadOrderedReadReceiptBeforeData)
{
    SNFixture fixture;
    StartHomeRead(fixture, Opcodes::REQ::ReadNoSnp, false, 2);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2, SubordinateReadReceipt()));
    SendHomeReadData(fixture, Opcodes::REQ::ReadNoSnp, 1, 3);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleHomeReadOrderedNoSnpAcceptsOneLateReadReceipt)
{
    SNFixture fixture;
    CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnp);
    XACT_CHECK(!fixture.joint.HasInflight());
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 20, SubordinateReadReceipt()));
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 21, SubordinateReadReceipt()) ==
        XactDenial::DENIED_READRECEIPT_TXNID_NOT_EXIST_OR_EVICTED);
}

XACT_TEST(Stage6OracleHomeReadOrderedNoSnpSepAcceptsOneLateReadReceipt)
{
    SNFixture fixture;
    CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnpSep);
    XACT_CHECK(!fixture.joint.HasInflight());
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 20, SubordinateReadReceipt()));
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 21, SubordinateReadReceipt()) ==
        XactDenial::DENIED_READRECEIPT_TXNID_NOT_EXIST_OR_EVICTED);
}

XACT_TEST(Stage6OracleHomeReadMissingReadReceiptReportsPossibleEviction)
{
    SNFixture fixture;
    std::string message;
    XACT_CHECK(fixture.joint.events != nullptr);
    fixture.joint.events->OnDeniedResponse.Register(Gravity::MakeListener<Xact::JointDeniedResponseEvent<config>>(
        "stage6-read-receipt", 0, [&](Xact::JointDeniedResponseEvent<config>& event) {
            message = event.GetMessage();
        }));

    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 1, SubordinateReadReceipt(0x55)) ==
        XactDenial::DENIED_READRECEIPT_TXNID_NOT_EXIST_OR_EVICTED);
    XACT_CHECK(message.find("evicted") != std::string::npos);
}

XACT_TEST(Stage6OracleHomeReadUnorderedReadReceiptRemainsRejected)
{
    SNFixture fixture;
    StartHomeRead(fixture, Opcodes::REQ::ReadNoSnp);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2, SubordinateReadReceipt()) ==
        XactDenial::DENIED_READRECEIPT_ON_NO_ORDER);
}

XACT_TEST(Stage6OracleHomeReadInvalidLateReadReceiptPreservesHistory)
{
    SNFixture fixture;
    CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnp);
    XACT_CHECK(!fixture.joint.HasInflight());

    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 20, SubordinateReadReceipt(2)) ==
        XactDenial::DENIED_READRECEIPT_TXNID_NOT_EXIST_OR_EVICTED);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 21, Response(6, 3, Opcodes::RSP::ReadReceipt, 1)) ==
        XactDenial::DENIED_READRECEIPT_TXNID_NOT_EXIST_OR_EVICTED);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 23, SubordinateReadReceipt()));
}

XACT_TEST(Stage6OracleHomeReadCompletedTxnIDCanBeReused)
{
    SNFixture fixture;
    CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnp);
    XACT_CHECK(!fixture.joint.HasInflight());

    StartHomeRead(fixture, Opcodes::REQ::ReadNoSnpSep, false, 2, 1, 20);
    SendHomeReadData(fixture, Opcodes::REQ::ReadNoSnpSep, 1, 22);
    XACT_CHECK(!fixture.joint.HasInflight());
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 25, SubordinateReadReceipt()));
}

XACT_TEST(Stage6OracleHomeReadCompletedHistoryEvictsOldest)
{
    SNFixture fixture;
    for (uint16_t txnId = 1; txnId <= 65; ++txnId)
    {
        CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnp, txnId);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 1001, SubordinateReadReceipt(1)) ==
        XactDenial::DENIED_READRECEIPT_TXNID_NOT_EXIST_OR_EVICTED);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 1002, SubordinateReadReceipt(2)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 1065, SubordinateReadReceipt(65)));
}

XACT_TEST(Stage6OracleHomeReadCompletedHistoryCopyForkAndClear)
{
    SNFixture source;
    CompleteOrderedHomeRead(source, Opcodes::REQ::ReadNoSnp);
    XACT_CHECK(!source.joint.HasInflight());

    Xact::SNFJoint<config> copied(source.joint);
    Xact::SNFJoint<config> assigned;
    assigned = source.joint;
    source.joint.Fork();

    XACT_ACCEPT(copied.NextTXRSP(source.global, 20, SubordinateReadReceipt()));
    XACT_ACCEPT(assigned.NextTXRSP(source.global, 20, SubordinateReadReceipt()));
    XACT_ACCEPT(source.joint.NextTXRSP(source.global, 20, SubordinateReadReceipt()));

    source.joint.Clear();
    XACT_CHECK(source.joint.NextTXRSP(source.global, 21, SubordinateReadReceipt()) ==
        XactDenial::DENIED_READRECEIPT_TXNID_NOT_EXIST_OR_EVICTED);
}

XACT_TEST(Stage6OracleHomeReadLateReadReceiptDoesNotRepeatEvents)
{
    SNFixture fixture;
    size_t completeCount = 0;
    size_t txnIdFreeCount = 0;
    XACT_CHECK(fixture.joint.events != nullptr);
    fixture.joint.events->OnComplete.Register(Gravity::MakeListener<Xact::JointXactionCompleteEvent<config>>(
        "stage6-complete", 0, [&](Xact::JointXactionCompleteEvent<config>&) {
            ++completeCount;
        }));
    fixture.joint.events->OnTxnIDFree.Register(Gravity::MakeListener<Xact::JointXactionTxnIDFreeEvent<config>>(
        "stage6-txnid-free", 0, [&](Xact::JointXactionTxnIDFreeEvent<config>&) {
            ++txnIdFreeCount;
        }));

    CompleteOrderedHomeRead(fixture, Opcodes::REQ::ReadNoSnp);
    XACT_CHECK(completeCount == 1);
    XACT_CHECK(txnIdFreeCount == 1);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 20, SubordinateReadReceipt()));
    XACT_CHECK(completeCount == 1);
    XACT_CHECK(txnIdFreeCount == 1);
}
