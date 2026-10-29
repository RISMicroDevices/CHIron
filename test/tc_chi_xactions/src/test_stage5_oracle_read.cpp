#include "sequence.hpp"

using namespace XactTest;

XACT_TEST(Stage5OracleMakeReadUniqueCompThenCompAck)
{
    RNFixture fixture;
    const auto request = Request(Opcodes::REQ::MakeReadUnique);
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    auto comp = Response(2, 4, Opcodes::RSP::Comp, 1, 33);
    comp.Resp() = Resps::Comp_UC;
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, comp) == XactDenial::ACCEPTED);

    const auto ack = Response(4, 2, Opcodes::RSP::CompAck, 33);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, ack) == XactDenial::ACCEPTED);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage5OracleReadOnceInvalidVariantsAcceptCompAck)
{
    for (uint16_t opcode : { Opcodes::REQ::ReadOnceCleanInvalid, Opcodes::REQ::ReadOnceMakeInvalid })
    {
        RNFixture fixture;
        auto request = Request(opcode);
        request.ExpCompAck() = 1;
        request.MemAttr() = MemAttrs::WriteBack_NoAllocate;
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

        for (uint8_t dataId : { uint8_t(0), uint8_t(2) })
        {
            auto data = Data(2, 4, Opcodes::DAT::CompData, 1, dataId, 33);
            data.HomeNID() = 2;
            data.BE() = 0;
            data.Resp() = Resps::CompData_UC;
            XACT_CHECK(fixture.joint.NextRXDAT(fixture.global, 2 + dataId, data) == XactDenial::ACCEPTED);
        }

        const auto ack = Response(4, 2, Opcodes::RSP::CompAck, 33);
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 8, ack) == XactDenial::ACCEPTED);
        XACT_CHECK(!fixture.joint.HasInflight());
    }
}

XACT_TEST(Stage5OracleReadOnceInvalidRejectsUnorderedReadReceipt)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::ReadOnceCleanInvalid);
    request.Order() = 0;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    const auto receipt = Response(2, 4, Opcodes::RSP::ReadReceipt, 1);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, receipt) != XactDenial::ACCEPTED);
}

XACT_TEST(Stage5OracleWriteEvictOrEvictRejectsDataAfterNoDataComp)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::WriteEvictOrEvict);
    request.ExpCompAck() = 1;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    auto comp = Response(2, 4, Opcodes::RSP::Comp, 1, 33);
    comp.Resp() = Resps::Comp_I;
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, comp) == XactDenial::ACCEPTED);

    const auto data = Data(4, 2, Opcodes::DAT::CopyBackWrData, 33, 0);
    XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 3, data) != XactDenial::ACCEPTED);
}

XACT_TEST(Stage5OracleReadNoSnpAcceptsExpectedCompAck)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::ReadNoSnp);
    request.ExpCompAck() = 1;
    request.SnpAttr() = 0;
    request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    for (uint8_t dataId : { uint8_t(0), uint8_t(2) })
    {
        auto data = Data(2, 4, Opcodes::DAT::CompData, 1, dataId, 33);
        data.HomeNID() = 2;
        data.BE() = 0;
        data.Resp() = Resps::CompData_I;
        XACT_CHECK(fixture.joint.NextRXDAT(fixture.global, 2 + dataId, data) == XactDenial::ACCEPTED);
    }

    const auto ack = Response(4, 2, Opcodes::RSP::CompAck, 33);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 8, ack) == XactDenial::ACCEPTED);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage5OracleWriteEvictOrEvictNoDataCompAckCompletes)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::WriteEvictOrEvict);
    request.ExpCompAck() = 1;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    auto comp = Response(2, 4, Opcodes::RSP::Comp, 1, 33);
    comp.Resp() = Resps::Comp_I;
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, comp) == XactDenial::ACCEPTED);

    const auto ack = Response(4, 2, Opcodes::RSP::CompAck, 33);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, ack) == XactDenial::ACCEPTED);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage5OracleReadNoSnpRejectsUnexpectedCompAck)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::ReadNoSnp);
    request.ExpCompAck() = 0;
    request.SnpAttr() = 0;
    request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    auto data = Data(2, 4, Opcodes::DAT::CompData, 1, 0, 33);
    data.HomeNID() = 2;
    data.BE() = 0;
    data.Resp() = Resps::CompData_I;
    XACT_CHECK(fixture.joint.NextRXDAT(fixture.global, 2, data) == XactDenial::ACCEPTED);

    const auto ack = Response(4, 2, Opcodes::RSP::CompAck, 33);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, ack) != XactDenial::ACCEPTED);
}

XACT_TEST(Stage5OracleReadNoSnp128CompletesTwoPackets)
{
    using Config128 = FlitConfiguration<7, 48, 0, 0, 128, false, false, false>;
    Xact::Global<Config128> global;
    Xact::RNFJoint<Config128> joint;
    global.TOPOLOGY.SetNode(2, Xact::NodeType::HN_F);
    global.TOPOLOGY.SetNode(4, Xact::NodeType::RN_F);

    Flits::REQ<Config128> request{};
    request.SrcID() = 4;
    request.TgtID() = 2;
    request.TxnID() = 1;
    request.Opcode() = Opcodes::REQ::ReadNoSnp;
    request.Addr() = 0x10000;
    request.Size() = 5;
    request.AllowRetry() = 1;
    request.SnpAttr() = 0;
    request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
    XACT_CHECK(joint.NextTXREQ(global, 1, request) == XactDenial::ACCEPTED);

    for (uint8_t dataId : { uint8_t(0), uint8_t(1) })
    {
        Flits::DAT<Config128> data{};
        data.SrcID() = 2;
        data.TgtID() = 4;
        data.HomeNID() = 2;
        data.TxnID() = 1;
        data.Opcode() = Opcodes::DAT::CompData;
        data.Resp() = Resps::CompData_I;
        data.DataID() = dataId;
        XACT_CHECK(joint.NextRXDAT(global, 2 + dataId, data) == XactDenial::ACCEPTED);
        if (dataId == 0)
            XACT_CHECK(joint.HasInflight());
    }
    XACT_CHECK(!joint.HasInflight());
}
