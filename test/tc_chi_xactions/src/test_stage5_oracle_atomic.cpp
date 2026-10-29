#include "sequence.hpp"

using namespace XactTest;

namespace {

    auto AtomicRequest(uint16_t opcode, uint8_t size = 3)
    {
        auto request = Request(opcode);
        request.Size() = size;
        return request;
    }

    void SendAtomicStoreData(RNFixture& fixture, uint16_t dbid)
    {
        auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, dbid, 0);
        data.BE() = 0xff;
        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 5, data) == XactDenial::ACCEPTED);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

}

XACT_TEST(Stage5OracleAtomicStoreSeparateResponsesMayUseDifferentDBIDs)
{
    RNFixture fixture;
    const auto request = AtomicRequest(Opcodes::REQ::AtomicStore::ADD);
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);

    const auto dbid = Response(2, 4, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, dbid) == XactDenial::ACCEPTED);
    const auto comp = Response(2, 4, Opcodes::RSP::Comp, 1, 99);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 3, comp) == XactDenial::ACCEPTED);
    SendAtomicStoreData(fixture, 41);
}

XACT_TEST(Stage5OracleAtomicStoreCompBeforeDBIDMayUseDifferentDBIDs)
{
    RNFixture fixture;
    const auto request = AtomicRequest(Opcodes::REQ::AtomicStore::ADD);
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);

    const auto comp = Response(2, 4, Opcodes::RSP::Comp, 1, 99);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, comp) == XactDenial::ACCEPTED);
    const auto dbid = Response(2, 4, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 3, dbid) == XactDenial::ACCEPTED);
    SendAtomicStoreData(fixture, 41);
}

XACT_TEST(Stage5OracleHomeAtomicStoreCompBeforeDBID)
{
    SNFixture fixture;
    auto request = AtomicRequest(Opcodes::REQ::AtomicStore::ADD);
    request.SrcID() = 2;
    request.TgtID() = 6;
    request.Order() = 0;
    request.SnpAttr() = 0;
    XACT_CHECK(fixture.joint.NextRXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);

    const auto comp = Response(6, 2, Opcodes::RSP::Comp, 1, 99);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2, comp) == XactDenial::ACCEPTED);
    const auto dbid = Response(6, 2, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, dbid) == XactDenial::ACCEPTED);

    auto data = Data(2, 6, Opcodes::DAT::NonCopyBackWrData, 41, 0);
    data.BE() = 0xff;
    XACT_CHECK(fixture.joint.NextRXDAT(fixture.global, 4, data) == XactDenial::ACCEPTED);
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage5OracleAtomicCompare128RequiresBothOperandPackets)
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
    request.Opcode() = Opcodes::REQ::AtomicCompare;
    request.Addr() = 0x10010;
    request.Size() = 5;
    request.AllowRetry() = 1;
    request.SnpAttr() = 1;
    request.MemAttr() = MemAttrs::WriteBack_Allocate;
    XACT_CHECK(joint.NextTXREQ(global, 1, request) == XactDenial::ACCEPTED);

    Flits::RSP<Config128> dbid{};
    dbid.SrcID() = 2;
    dbid.TgtID() = 4;
    dbid.TxnID() = 1;
    dbid.Opcode() = Opcodes::RSP::DBIDResp;
    dbid.DBID() = 41;
    XACT_CHECK(joint.NextRXRSP(global, 2, dbid) == XactDenial::ACCEPTED);

    Flits::DAT<Config128> first{};
    first.SrcID() = 4;
    first.TgtID() = 2;
    first.TxnID() = 41;
    first.Opcode() = Opcodes::DAT::NonCopyBackWrData;
    first.DataID() = 0;
    first.CCID() = 1;
    first.BE() = 0xffff;
    XACT_CHECK(joint.NextTXDAT(global, 3, first) == XactDenial::ACCEPTED);

    Flits::DAT<Config128> result{};
    result.SrcID() = 2;
    result.TgtID() = 4;
    result.HomeNID() = 2;
    result.TxnID() = 1;
    result.Opcode() = Opcodes::DAT::CompData;
    result.Resp() = Resps::CompData_I;
    result.DataID() = 1;
    result.CCID() = 1;
    result.DBID() = 41;
    result.BE() = 0xffff;
    XACT_CHECK(joint.NextRXDAT(global, 4, result) == XactDenial::ACCEPTED);
    XACT_CHECK(joint.HasInflight());

    auto second = first;
    second.DataID() = 1;
    XACT_CHECK(joint.NextTXDAT(global, 5, second) == XactDenial::ACCEPTED);
    XACT_CHECK(!joint.HasInflight());
}

XACT_TEST(Stage5OracleAtomicRejectsInvalidSizeAndAlignment)
{
    {
        RNFixture fixture;
        const auto request = AtomicRequest(Opcodes::REQ::AtomicSwap, 4);
        XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) != XactDenial::ACCEPTED);
    }
    {
        RNFixture fixture;
        auto request = AtomicRequest(Opcodes::REQ::AtomicCompare, 5);
        request.Addr() = 0x10008;
        XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) != XactDenial::ACCEPTED);
    }
}

XACT_TEST(Stage5OracleAtomicRejectsIncompleteByteEnables)
{
    RNFixture fixture;
    auto request = AtomicRequest(Opcodes::REQ::AtomicStore::ADD, 2);
    request.Addr() = 0x10004;
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);
    const auto dbid = Response(2, 4, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, dbid) == XactDenial::ACCEPTED);

    auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 41, 0);
    data.BE() = 0x3;
    XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 3, data) != XactDenial::ACCEPTED);
}

XACT_TEST(Stage5OracleAtomicRejectsWrongDataIDAndCCID)
{
    RNFixture fixture;
    auto request = AtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 3);
    request.Addr() = 0x10018;
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);
    const auto dbid = Response(2, 4, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, dbid) == XactDenial::ACCEPTED);

    auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 41, 0);
    data.CCID() = 0;
    XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 3, data) != XactDenial::ACCEPTED);
}

XACT_TEST(Stage5OracleAtomicOpcodePredicatesUseFullWidth)
{
    XACT_CHECK(!Opcodes::REQ::AtomicStore::Is(0x68));
    XACT_CHECK(!Opcodes::REQ::AtomicStore::Is(0x6f));
    XACT_CHECK(!Opcodes::REQ::AtomicLoad::Is(0x70));
    XACT_CHECK(!Opcodes::REQ::AtomicLoad::Is(0x77));
    XACT_CHECK(Opcodes::REQ::AtomicStore::Is(Opcodes::REQ::AtomicStore::ADD));
    XACT_CHECK(Opcodes::REQ::AtomicLoad::Is(Opcodes::REQ::AtomicLoad::UMIN));
}

XACT_TEST(Stage5OracleAtomicRejectsDataOutsideByteEnables)
{
    RNFixture fixture;
    auto request = AtomicRequest(Opcodes::REQ::AtomicStore::ADD, 2);
    request.Addr() = 0x10004;
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);
    const auto dbid = Response(2, 4, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, dbid) == XactDenial::ACCEPTED);

    auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 41, 0);
    data.BE() = 0xf;
    data.Data()[0] = uint64_t{1} << 32;
    XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 3, data) != XactDenial::ACCEPTED);
}

XACT_TEST(Stage5OracleAtomicRejectsInvalidReturnByteEnables)
{
    RNFixture fixture;
    auto request = AtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 3);
    request.Addr() = 0x10018;
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);
    const auto dbid = Response(2, 4, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, dbid) == XactDenial::ACCEPTED);

    auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 41, 1);
    data.CCID() = 1;
    data.BE() = 0xff00;
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3, data));

    auto result = Data(2, 4, Opcodes::DAT::CompData, 1, 1, 41);
    result.HomeNID() = 2;
    result.CCID() = 1;
    result.Resp() = Resps::CompData_I;
    result.BE() = 0;
    XACT_CHECK(fixture.joint.NextRXDAT(fixture.global, 4, result) != XactDenial::ACCEPTED);
}

XACT_TEST(Stage5OracleAtomicAcceptsValidReturnByteEnables)
{
    RNFixture fixture;
    auto request = AtomicRequest(Opcodes::REQ::AtomicLoad::ADD, 3);
    request.Addr() = 0x10018;
    XACT_CHECK(fixture.joint.NextTXREQ(fixture.global, 1, request) == XactDenial::ACCEPTED);
    const auto dbid = Response(2, 4, Opcodes::RSP::DBIDResp, 1, 41);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, dbid) == XactDenial::ACCEPTED);

    auto data = Data(4, 2, Opcodes::DAT::NonCopyBackWrData, 41, 1);
    data.CCID() = 1;
    data.BE() = 0xff00;
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3, data));

    auto result = Data(2, 4, Opcodes::DAT::CompData, 1, 1, 41);
    result.HomeNID() = 2;
    result.CCID() = 1;
    result.Resp() = Resps::CompData_I;
    result.BE() = 0xff00;
    XACT_CHECK(fixture.joint.NextRXDAT(fixture.global, 4, result) == XactDenial::ACCEPTED);
    XACT_CHECK(!fixture.joint.HasInflight());
}
