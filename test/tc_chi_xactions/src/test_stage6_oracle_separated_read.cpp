#include "sequence.hpp"

using namespace XactTest;

namespace {

    constexpr uint16_t dataBufferId = 0x40;

    Flits::REQ<config> ReadRequest(uint16_t opcode, uint8_t order, bool expCompAck = true)
    {
        auto request = Request(opcode, 1);
        request.Order() = order;
        request.ExpCompAck() = expCompAck;
        if (opcode == Opcodes::REQ::ReadNoSnp)
        {
            request.SnpAttr() = 0;
            request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
        }
        else
            request.MemAttr() = MemAttrs::WriteBack_NoAllocate;
        return request;
    }

    Flits::RSP<config> HomeResponse(uint16_t opcode)
    {
        return Response(2, 4, opcode, 1, dataBufferId);
    }

    Flits::RSP<config> RequesterCompAck()
    {
        return Response(4, 2, Opcodes::RSP::CompAck, dataBufferId);
    }

    Flits::DAT<config> HomeData(uint16_t opcode, uint8_t dataId)
    {
        auto data = Data(2, 4, opcode, 1, dataId, dataBufferId);
        data.HomeNID() = 2;
        data.Resp() = opcode == Opcodes::DAT::CompData ? Resps::CompData_I : Resps::DataSepResp_UC;
        return data;
    }

    void StartSeparatedRead(RNFixture& fixture, uint16_t opcode, uint8_t order, bool expCompAck = true)
    {
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, ReadRequest(opcode, order, expCompAck)));
        XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2, HomeResponse(Opcodes::RSP::RespSepData)));
    }

    void OrderedCompAck(uint16_t opcode)
    {
        RNFixture fixture;
        StartSeparatedRead(fixture, opcode, 2);

        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, RequesterCompAck()) ==
            XactDenial::DENIED_COMPACK_BEFORE_COMPDATA_OR_RESPSEPDATA);
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 4,
            HomeData(Opcodes::DAT::DataSepResp, 0)));
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 5, RequesterCompAck()));
    }

    void UnorderedCompAckAfterRespSepData()
    {
        RNFixture fixture;
        StartSeparatedRead(fixture, Opcodes::REQ::ReadNoSnp, 0);
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 3, RequesterCompAck()));
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 4,
            HomeData(Opcodes::DAT::DataSepResp, 0)));
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 5,
            HomeData(Opcodes::DAT::DataSepResp, 2)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    void OrderedCompAckAfterCompData()
    {
        RNFixture fixture;
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1,
            ReadRequest(Opcodes::REQ::ReadOnce, 2)));
        auto data = HomeData(Opcodes::DAT::CompData, 0);
        data.Resp() = Resps::CompData_UC;
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 2, data));
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 3, RequesterCompAck()));
    }

    void RejectOrderedNoCompAckRespSepData(RNFixture& fixture, uint16_t opcode)
    {
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, ReadRequest(opcode, 2, false)));
        XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2,
            HomeResponse(Opcodes::RSP::RespSepData)) ==
                XactDenial::DENIED_RESPSEPDATA_ON_ORDER_WITHOUT_EXPCOMPACK);
    }

    void UnorderedNoCompAckSeparatedRead()
    {
        RNFixture fixture;
        StartSeparatedRead(fixture, Opcodes::REQ::ReadNoSnp, 0, false);
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 3,
            HomeData(Opcodes::DAT::DataSepResp, 0)));
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 4,
            HomeData(Opcodes::DAT::DataSepResp, 2)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

}

XACT_TEST(Stage6OracleSeparatedOrderedReadNoSnpCompAck) { OrderedCompAck(Opcodes::REQ::ReadNoSnp); }
XACT_TEST(Stage6OracleSeparatedOrderedReadOnceCompAck) { OrderedCompAck(Opcodes::REQ::ReadOnce); }
XACT_TEST(Stage6OracleSeparatedOrderedReadOnceCleanInvalidCompAck) { OrderedCompAck(Opcodes::REQ::ReadOnceCleanInvalid); }
XACT_TEST(Stage6OracleSeparatedOrderedReadOnceMakeInvalidCompAck) { OrderedCompAck(Opcodes::REQ::ReadOnceMakeInvalid); }
XACT_TEST(Stage6OracleSeparatedUnorderedCompAckAfterRespSepData) { UnorderedCompAckAfterRespSepData(); }
XACT_TEST(Stage6OracleSeparatedOrderedCompAckAfterCompData) { OrderedCompAckAfterCompData(); }

XACT_TEST(Stage6OracleSeparatedOrderedCompletionWithoutReadReceipt)
{
    RNFixture fixture;
    StartSeparatedRead(fixture, Opcodes::REQ::ReadNoSnp, 2);

    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 3,
        HomeData(Opcodes::DAT::DataSepResp, 0)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 4, RequesterCompAck()));
    XACT_CHECK(fixture.joint.HasInflight());

    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 5,
        HomeData(Opcodes::DAT::DataSepResp, 2)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleSeparatedOrderedNoCompAckReadNoSnpFallback)
{
    RNFixture fixture;
    RejectOrderedNoCompAckRespSepData(fixture, Opcodes::REQ::ReadNoSnp);

    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3,
        Response(2, 4, Opcodes::RSP::ReadReceipt, 1)));
    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 4,
        HomeData(Opcodes::DAT::CompData, 0)));
    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 5,
        HomeData(Opcodes::DAT::CompData, 2)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleSeparatedOrderedNoCompAckReadOnceRejectsRespSepData)
{
    RNFixture fixture;
    RejectOrderedNoCompAckRespSepData(fixture, Opcodes::REQ::ReadOnce);
}

XACT_TEST(Stage6OracleSeparatedOrderedNoCompAckReadOnceCleanInvalidRejectsRespSepData)
{
    RNFixture fixture;
    RejectOrderedNoCompAckRespSepData(fixture, Opcodes::REQ::ReadOnceCleanInvalid);
}

XACT_TEST(Stage6OracleSeparatedOrderedNoCompAckReadOnceMakeInvalidRejectsRespSepData)
{
    RNFixture fixture;
    RejectOrderedNoCompAckRespSepData(fixture, Opcodes::REQ::ReadOnceMakeInvalid);
}

XACT_TEST(Stage6OracleSeparatedUnorderedNoCompAckCompletion)
{
    UnorderedNoCompAckSeparatedRead();
}
