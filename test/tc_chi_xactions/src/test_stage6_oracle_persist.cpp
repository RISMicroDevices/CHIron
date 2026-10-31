#include "sequence.hpp"

using namespace XactTest;

namespace {

    Flits::REQ<config> PersistRequest(uint16_t home, uint16_t txnId, uint8_t group)
    {
        auto request = Request(Opcodes::REQ::CleanSharedPersistSep, txnId);
        request.TgtID() = home;
        request.PGroupID() = group;
        return request;
    }

    Flits::RSP<config> PersistResponse(uint16_t source, uint8_t group)
    {
        auto response = Response(source, 4, Opcodes::RSP::Persist, 0);
        response.PGroupID() = group;
        return response;
    }

    Flits::RSP<config> CompResponse(uint16_t source, uint16_t txnId)
    {
        auto response = Response(source, 4, Opcodes::RSP::Comp, txnId);
        response.Resp() = Resps::Comp_UC;
        return response;
    }

    void RegisterSecondHome(RNFixture& fixture)
    {
        fixture.global.TOPOLOGY.SetNode(12, Xact::NodeType::HN_F);
    }

}

XACT_TEST(Stage6OraclePersistRejectsWrongHomeAfterSAM)
{
    RNFixture fixture;
    RegisterSecondHome(fixture);
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, PersistRequest(12, 7, 0x42)));

    const auto wrong = PersistResponse(2, 0x42);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, wrong) ==
        XactDenial::DENIED_RSP_TXNID_NOT_EXIST);
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3, PersistResponse(12, 0x42)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 4, CompResponse(12, 7)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OraclePersistSelectsMatchingHome)
{
    RNFixture fixture;
    RegisterSecondHome(fixture);
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, PersistRequest(12, 1, 0x55)));
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 2, PersistRequest(2, 2, 0x55)));

    std::shared_ptr<Xact::Xaction<config>> matched;
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3, PersistResponse(2, 0x55), &matched));
    XACT_CHECK(matched->GetFirst().flit.req.TgtID() == 2);
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 4, PersistResponse(12, 0x55)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 5, CompResponse(12, 1)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 6, CompResponse(2, 2)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OraclePersistAcceptsBeforeSAMRemapping)
{
    RNFixture fixture;
    RegisterSecondHome(fixture);
    fixture.global.SAM_SCOPE.Set(4, Xact::SAMScope::BeforeSAM);
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, PersistRequest(2, 3, 0x31)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2, CompResponse(12, 3)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3, PersistResponse(12, 0x31)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OraclePersistUsesCompHomeBeforeSAM)
{
    RNFixture fixture;
    RegisterSecondHome(fixture);
    fixture.global.SAM_SCOPE.Set(4, Xact::SAMScope::BeforeSAM);
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, PersistRequest(2, 4, 0x32)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2, CompResponse(2, 4)));

    const auto wrong = PersistResponse(12, 0x32);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 3, wrong) ==
        XactDenial::DENIED_RSP_TXNID_NOT_EXIST);
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 4, PersistResponse(2, 0x32)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleDirectPersistBeforeComp)
{
    RNFixture fixture;
    fixture.global.TOPOLOGY.SetNode(6, Xact::NodeType::SN_F);
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, PersistRequest(2, 5, 0x33)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2, PersistResponse(6, 0x33)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3, CompResponse(2, 5)));
    XACT_CHECK(!fixture.joint.HasInflight());
}
