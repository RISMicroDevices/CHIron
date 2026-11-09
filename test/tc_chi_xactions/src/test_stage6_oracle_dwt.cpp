#include "sequence.hpp"

using namespace XactTest;

namespace {

    Flits::REQ<config> DWTRequest(uint16_t opcode, bool dwt,
                                  uint16_t txnId = 0, uint16_t returnTxnId = 0)
    {
        auto request = Request(opcode, txnId);
        request.SrcID() = 2;
        request.TgtID() = 6;
        request.SnpAttr() = dwt ? 1 : 0;
        request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
        request.DoDWT() = dwt ? 1 : 0;
        request.ReturnNID() = dwt ? 4 : 2;
        request.ReturnTxnID() = returnTxnId;
        return request;
    }

    void RegisterDWTNodes(SNFixture& fixture)
    {
        fixture.global.TOPOLOGY.SetNode(4, Xact::NodeType::RN_F);
        fixture.global.TOPOLOGY.SetNode(7, Xact::NodeType::SN_F);
        fixture.global.TOPOLOGY.SetNode(12, Xact::NodeType::HN_F);
    }

    Flits::RSP<config> DirectDBIDResp(uint16_t dbid = 77, uint16_t source = 6)
    {
        return Response(source, 4, Opcodes::RSP::DBIDResp, 0x55, dbid);
    }

    Flits::RSP<config> HomeResponse(uint16_t opcode, uint16_t txnId,
                                    uint16_t dbid = 77, uint16_t home = 2)
    {
        return Response(6, home, opcode, txnId, dbid);
    }

    void AcceptWriteData(SNFixture& fixture, uint16_t source, uint16_t dbid,
                         uint64_t& time, uint16_t opcode = Opcodes::DAT::NonCopyBackWrData)
    {
        for (uint8_t dataId = 0; dataId < 4; dataId += 2)
        {
            auto data = Data(source, 6, opcode, dbid, dataId);
            if (opcode == Opcodes::DAT::WriteDataCancel)
                data.BE() = 0;
            XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, time++, data));
        }
    }

    void AcceptDWT(uint16_t opcode)
    {
        SNFixture fixture;
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, DWTRequest(opcode, true)));
    }

    void RetryAndGrant(SNFixture& fixture, const Flits::REQ<config>& request)
    {
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 20,
            Response(6, 2, Opcodes::RSP::RetryAck, request.TxnID())));
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 21,
            Response(6, 2, Opcodes::RSP::PCrdGrant, 0)));
    }

    Flits::REQ<config> CreditedResend(Flits::REQ<config> request)
    {
        request.AllowRetry() = 0;
        return request;
    }

    Flits::REQ<config> HomeReadRequest(uint16_t txnId, uint16_t returnTxnId)
    {
        auto request = Request(Opcodes::REQ::ReadNoSnp, txnId);
        request.SrcID() = 2;
        request.TgtID() = 6;
        request.SnpAttr() = 0;
        request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
        request.Order() = 0;
        request.ReturnNID() = 4;
        request.ReturnTxnID() = returnTxnId;
        return request;
    }

    Flits::REQ<config> HomeAtomicRequest(uint16_t opcode, uint16_t txnId, uint16_t returnTxnId)
    {
        auto request = Request(opcode, txnId);
        request.SrcID() = 2;
        request.TgtID() = 6;
        request.Size() = 3;
        request.Order() = 0;
        request.SnpAttr() = 0;
        request.ReturnNID() = 2;
        request.ReturnTxnID() = returnTxnId;
        return request;
    }

    void CompleteHomeAtomicLoad(SNFixture& fixture, uint16_t txnId, uint16_t returnTxnId)
    {
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 30,
            Response(6, 2, Opcodes::RSP::DBIDResp, txnId, 41)));

        auto result = Data(6, 2, Opcodes::DAT::CompData, returnTxnId, 0, 41);
        result.HomeNID() = 2;
        result.Resp() = Resps::CompData_I;
        result.BE() = 0xff;
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 31, result));

        auto operands = Data(2, 6, Opcodes::DAT::NonCopyBackWrData, 41, 0);
        operands.BE() = 0xff;
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 32, operands));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    void CheckDWTResendChangeRejected(bool changeReturnNID, bool changeReturnTxnID)
    {
        SNFixture fixture;
        RegisterDWTNodes(fixture);
        auto original = DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55);
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, original));
        RetryAndGrant(fixture, original);

        auto resend = DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x20, 0x55);
        if (changeReturnNID)
            resend.ReturnNID() = 10;
        if (changeReturnTxnID)
            resend.ReturnTxnID() = 0x56;
        XACT_CHECK(fixture.joint.NextRXREQ(fixture.global, 23, CreditedResend(resend)) ==
            XactDenial::DENIED_NO_MATCHING_RETRY);

        auto correct = DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x20, 0x55);
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 24, CreditedResend(correct)));
    }

    void CheckAtomicLoadReturnTxnIDChangeRejected(uint16_t changedReturnTxnID)
    {
        SNFixture fixture;
        RegisterDWTNodes(fixture);
        auto original = HomeAtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 0x10, 0x10);
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, original));
        RetryAndGrant(fixture, original);

        auto resend = HomeAtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 0x10, changedReturnTxnID);
        XACT_CHECK(fixture.joint.NextRXREQ(fixture.global, 23, CreditedResend(resend)) ==
            XactDenial::DENIED_NO_MATCHING_RETRY);

        auto correct = HomeAtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 0x10, 0x10);
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 24, CreditedResend(correct)));
        CompleteHomeAtomicLoad(fixture, 0x10, 0x10);
    }

}

XACT_TEST(Stage6OracleDWTWriteNoSnpFullSharedFieldAccepted)
{
    AcceptDWT(Opcodes::REQ::WriteNoSnpFull);
}

XACT_TEST(Stage6OracleDWTWriteNoSnpPtlSharedFieldAccepted)
{
    AcceptDWT(Opcodes::REQ::WriteNoSnpPtl);
}

XACT_TEST(Stage6OracleDWTNonDWTControlAccepted)
{
    SNFixture fixture;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, DWTRequest(Opcodes::REQ::WriteNoSnpFull, false)));
}

XACT_TEST(Stage6OracleDWTDirectDBIDCompAndData)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2, DirectDBIDResp()));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 3,
        HomeResponse(Opcodes::RSP::Comp, 0x10)));

    uint64_t time = 4;
    AcceptWriteData(fixture, 4, 77, time);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTDirectCompBeforeDBIDAndData)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        HomeResponse(Opcodes::RSP::Comp, 0x10)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 3, DirectDBIDResp()));

    uint64_t time = 4;
    AcceptWriteData(fixture, 4, 77, time);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTDirectDataBeforeComp)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2, DirectDBIDResp()));

    uint64_t time = 3;
    AcceptWriteData(fixture, 4, 77, time);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, time,
        HomeResponse(Opcodes::RSP::Comp, 0x10)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTDirectWriteDataCancel)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        HomeResponse(Opcodes::RSP::Comp, 0x10)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 3, DirectDBIDResp()));

    uint64_t time = 4;
    AcceptWriteData(fixture, 4, 77, time, Opcodes::DAT::WriteDataCancel);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTDirectCompDBIDRespRejected)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2,
        HomeResponse(Opcodes::RSP::CompDBIDResp, 0x10)) ==
        XactDenial::DENIED_DWT_COMPDBIDRESP_DIRECT);
}

XACT_TEST(Stage6OracleDWTIndependentCompDBIDRespAccepted)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    auto request = DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x10);
    request.ReturnNID() = 2;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, request));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        HomeResponse(Opcodes::RSP::CompDBIDResp, 0x10)));

    uint64_t time = 3;
    AcceptWriteData(fixture, 2, 77, time);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTNonDWTEarlyCompAccepted)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, false, 0x10)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        HomeResponse(Opcodes::RSP::Comp, 0x10)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 3,
        HomeResponse(Opcodes::RSP::DBIDResp, 0x10)));

    uint64_t time = 4;
    AcceptWriteData(fixture, 2, 77, time);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTMismatchedDBIDRejected)
{
    {
        SNFixture fixture;
        RegisterDWTNodes(fixture);
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
            DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
            HomeResponse(Opcodes::RSP::Comp, 0x10, 99)));
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, DirectDBIDResp()) ==
            XactDenial::DENIED_RSP_DBID_MISMATCH);
    }
    {
        SNFixture fixture;
        RegisterDWTNodes(fixture);
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
            DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2, DirectDBIDResp()));
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3,
            HomeResponse(Opcodes::RSP::Comp, 0x10, 99)) ==
            XactDenial::DENIED_RSP_DBID_MISMATCH);
    }
}

XACT_TEST(Stage6OracleDWTDuplicateDBIDRespRejected)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2, DirectDBIDResp()));
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, DirectDBIDResp()) ==
        XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP);
}

XACT_TEST(Stage6OracleDWTPendingGenerationSelected)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2, DirectDBIDResp()));

    auto request = DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x20, 0x55);
    request.SrcID() = 12;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 3, request));

    std::shared_ptr<Xact::Xaction<config>> matched;
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 4, DirectDBIDResp(78), &matched));
    XACT_CHECK(matched->GetFirst().flit.req.SrcID() == 12);

    uint64_t time = 5;
    AcceptWriteData(fixture, 4, 77, time);
    AcceptWriteData(fixture, 4, 78, time);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, time++,
        HomeResponse(Opcodes::RSP::Comp, 0x10)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, time,
        HomeResponse(Opcodes::RSP::Comp, 0x20, 78, 12)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTAmbiguousPendingGenerationsRejected)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));

    auto request = DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x20, 0x55);
    request.SrcID() = 12;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 2, request));
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, DirectDBIDResp()) ==
        XactDenial::DENIED_DWT_RSP_AMBIGUOUS);
}

XACT_TEST(Stage6OracleDWTSubordinateMismatchRejected)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        DWTRequest(Opcodes::REQ::WriteNoSnpFull, true, 0x10, 0x55)));
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2, DirectDBIDResp(77, 7)) ==
        XactDenial::DENIED_RSP_TXNID_NOT_EXIST);
}

XACT_TEST(Stage6OracleDWTSharedFieldTraitIsInapplicable)
{
    XACT_CHECK(!Xact::FieldTrait::IsApplicable(Xact::FieldConvention::S));
    XACT_CHECK(Xact::FieldTrait::IsApplicable(Xact::FieldConvention::Y));
}

XACT_TEST(Stage6OracleDWTRequesterReturnTxnIDMustBeZero)
{
    RNFixture fixture;
    auto request = DWTRequest(Opcodes::REQ::WriteNoSnpFull, false, 0x10, 0x55);
    request.SrcID() = 4;
    request.TgtID() = 2;
    request.ReturnNID() = 0;
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) ==
        XactDenial::DENIED_REQ_FIELD_RETURNTXNID);

    request.ReturnTxnID() = 0;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 2, request));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3,
        Response(2, 4, Opcodes::RSP::CompDBIDResp, 0x10, 77)));
    for (uint8_t dataId = 0; dataId < 4; dataId += 2)
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 4 + dataId,
            Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 77, dataId)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTStashAliasControlAccepted)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::WriteUniqueFullStash, 0x10);
    request.StashNIDValid() = 1;
    request.StashNID() = 4;
    request.StashLPIDValid() = 1;
    request.StashLPID() = 0x15;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));
}

XACT_TEST(Stage6OracleDWTResendRejectsChangedReturnNID)
{
    CheckDWTResendChangeRejected(true, false);
}

XACT_TEST(Stage6OracleDWTResendRejectsChangedReturnTxnID)
{
    CheckDWTResendChangeRejected(false, true);
}

XACT_TEST(Stage6OracleDWTNonDWTResendAllowsChangedReturnIDs)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    auto original = DWTRequest(Opcodes::REQ::WriteNoSnpFull, false, 0x10, 0x55);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, original));
    RetryAndGrant(fixture, original);

    auto resend = DWTRequest(Opcodes::REQ::WriteNoSnpFull, false, 0x20, 0xaa);
    resend.ReturnNID() = 10;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 23, CreditedResend(resend)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 24,
        HomeResponse(Opcodes::RSP::CompDBIDResp, 0x20)));

    uint64_t time = 25;
    AcceptWriteData(fixture, 2, 77, time);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTReadResendAllowsChangedReturnTxnID)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    auto original = HomeReadRequest(0x10, 0x55);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, original));
    RetryAndGrant(fixture, original);

    auto resend = HomeReadRequest(0x20, 0x56);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 23, CreditedResend(resend)));
    for (uint8_t dataId = 0; dataId < 4; dataId += 2)
    {
        auto data = Data(6, 4, Opcodes::DAT::CompData, 0x56, dataId, 0x20);
        data.HomeNID() = 2;
        data.Resp() = Resps::CompData_I;
        data.BE() = 0;
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 24 + dataId, data));
    }
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDWTAtomicLoadResendRejectsLowReturnTxnIDChange)
{
    CheckAtomicLoadReturnTxnIDChangeRejected(0x11);
}

XACT_TEST(Stage6OracleDWTAtomicLoadResendRejectsUpperReturnTxnIDChange)
{
    CheckAtomicLoadReturnTxnIDChangeRejected(0x110);
}

XACT_TEST(Stage6OracleDWTAtomicLoadResendAcceptsUnchangedReturnTxnID)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    auto original = HomeAtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 0x10, 0x10);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, original));
    RetryAndGrant(fixture, original);

    auto resend = HomeAtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 0x10, 0x10);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 23, CreditedResend(resend)));
    CompleteHomeAtomicLoad(fixture, 0x10, 0x10);
}

XACT_TEST(Stage6OracleDWTAtomicStoreResendAllowsChangedReturnTxnID)
{
    SNFixture fixture;
    RegisterDWTNodes(fixture);
    auto original = HomeAtomicRequest(Opcodes::REQ::AtomicStore::ADD, 0x10, 0x10);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, original));
    RetryAndGrant(fixture, original);

    auto resend = HomeAtomicRequest(Opcodes::REQ::AtomicStore::ADD, 0x10, 0xaa);
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 23, CreditedResend(resend)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 24,
        Response(6, 2, Opcodes::RSP::DBIDResp, 0x10, 41)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 25,
        Response(6, 2, Opcodes::RSP::Comp, 0x10, 41)));

    auto operands = Data(2, 6, Opcodes::DAT::NonCopyBackWrData, 41, 0);
    operands.BE() = 0xff;
    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 26, operands));
    XACT_CHECK(!fixture.joint.HasInflight());
}
