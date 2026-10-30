#include "sequence.hpp"

using namespace XactTest;

namespace {

    Flits::REQ<config> DWTRequest(uint16_t opcode, bool dwt)
    {
        auto request = Request(opcode, 0);
        request.SrcID() = 2;
        request.TgtID() = 6;
        request.SnpAttr() = dwt ? 1 : 0;
        request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
        request.DoDWT() = dwt ? 1 : 0;
        request.ReturnNID() = dwt ? 4 : 2;
        request.ReturnTxnID() = 0;
        return request;
    }

    void AcceptDWT(uint16_t opcode)
    {
        SNFixture fixture;
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, DWTRequest(opcode, true)));
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
