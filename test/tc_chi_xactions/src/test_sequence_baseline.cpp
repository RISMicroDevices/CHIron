#include "sequence.hpp"

using namespace XactTest;

XACT_TEST(SequenceRNReadSharedDataThenCompAck)
{
    RNFixture fixture;
    const auto request = Request(Opcodes::REQ::ReadShared);
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);

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

XACT_TEST(SequenceRNWriteUniqueCombinedResponseAndData)
{
    RNFixture fixture;
    const auto request = Request(Opcodes::REQ::WriteUniqueFull);
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);

    const auto response = Response(2, 4, Opcodes::RSP::CompDBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, response) == XactDenial::ACCEPTED);

    for (uint8_t dataId : { uint8_t(0), uint8_t(2) })
    {
        const auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 41, dataId);
        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 3 + dataId, data) == XactDenial::ACCEPTED);
    }
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(SequenceRNWriteUniqueRejectsDataBeforeDBID)
{
    RNFixture fixture;
    const auto request = Request(Opcodes::REQ::WriteUniqueFull);
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);

    const auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 41, 0);
    XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 2, data) != XactDenial::ACCEPTED);
    XACT_CHECK(fixture.joint.HasInflight());
}

XACT_TEST(SequenceSNReadNoSnpData)
{
    SNFixture fixture;
    auto request = Request(Opcodes::REQ::ReadNoSnp);
    request.SrcID() = 2;
    request.TgtID() = 6;
    request.AllowRetry() = 1;
    request.SnpAttr() = 0;
    request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, request));

    for (uint8_t dataId : { uint8_t(0), uint8_t(2) })
    {
        auto data = Data(6, 2, Opcodes::DAT::CompData, 1, dataId);
        data.BE() = 0;
        data.Resp() = Resps::CompData_I;
        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 2 + dataId, data) == XactDenial::ACCEPTED);
    }
    XACT_CHECK(!fixture.joint.HasInflight());
}
