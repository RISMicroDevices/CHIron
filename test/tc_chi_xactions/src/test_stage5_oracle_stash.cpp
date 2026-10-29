#include "sequence.hpp"

using namespace XactTest;

namespace {

    void CheckStashGroupResponse(uint16_t opcode)
    {
        RNFixture fixture;
        auto request = Request(Opcodes::REQ::StashOnceSepUnique);
        request.StashNIDValid() = 1;
        request.TxnID() = 0;
        request.StashNID() = 4;
        request.StashGroupID() = 0x12;
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

        auto response = Response(2, 4, opcode, 0);
        response.StashGroupID() = 0x13;
        XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, response) != XactDenial::ACCEPTED);
    }

}

XACT_TEST(Stage5OracleStashDoneChecksGroup)
{
    CheckStashGroupResponse(Opcodes::RSP::StashDone);
}

XACT_TEST(Stage5OracleCompStashDoneChecksGroup)
{
    CheckStashGroupResponse(Opcodes::RSP::CompStashDone);
}

XACT_TEST(Stage5OracleSeparatedStashResponsesComplete)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::StashOnceSepShared);
    request.StashNIDValid() = 1;
    request.TxnID() = 0;
    request.StashNID() = 4;
    request.StashGroupID() = 0x24;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    auto done = Response(2, 4, Opcodes::RSP::StashDone, 0);
    done.StashGroupID() = 0x24;
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2, done));
    XACT_CHECK(fixture.joint.HasInflight());

    const auto comp = Response(2, 4, Opcodes::RSP::Comp, 0);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 3, comp) == XactDenial::ACCEPTED);
    XACT_CHECK(!fixture.joint.HasInflight());
}
