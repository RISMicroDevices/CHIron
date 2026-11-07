#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ = TestDriver::REQ;
    using RSP = TestDriver::RSP;
    using DAT = TestDriver::DAT;

    void CheckTagMatch(TestDriver& driver, uint8_t group)
    {
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::TagMatch);
        LUNATIC_CHECK(response.Resp() == CHI::Eb::Resps::Fail);
        LUNATIC_CHECK(response.TagGroupID() == group);
        LUNATIC_CHECK(response.TagOp() == CHI::Eb::TagOp::Invalid);
    }

    void SendOperand(TestDriver& driver, DataBufferId dbid)
    {
        DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = 0;
        data.CCID() = 0;
        data.Data()[0] = 7;
        data.BE() = 1;
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
    }

    auto MatchRequest(TestDriver& driver, uint16_t opcode, uint8_t group)
    {
        auto request = driver.Request(opcode, 4, 1, 0x74000);
        request.TagOp() = CHI::Eb::TagOp::MatchFetch;
        request.TagGroupID() = group;
        return request;
    }

    void PushRequest(TestDriver& driver, const REQ& request)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
    }

}

LUNATIC_TEST(Stage9MTEUnsupportedMatchWriteReturnsFail)
{
    TestDriver driver;
    PushRequest(driver, MatchRequest(driver, CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 0xa7));
    CheckTagMatch(driver, 0xa7);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 1, dbid, LineData{}, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
}

LUNATIC_TEST(Stage9MTEUnsupportedCancelStillReturnsFail)
{
    TestDriver driver;
    PushRequest(driver, MatchRequest(driver, CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 0x5a));
    CheckTagMatch(driver, 0x5a);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT cancel{};
        cancel.SrcID() = 4;
        cancel.TgtID() = 2;
        cancel.TxnID() = dbid;
        cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        cancel.DataID() = beat * 2;
        cancel.CCID() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(cancel));
    }
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
}

LUNATIC_TEST(Stage9MTEUnsupportedAtomicStoreReturnsFail)
{
    TestDriver driver;
    auto request = MatchRequest(driver, CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 0x91);
    request.Size() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    CheckTagMatch(driver, 0x91);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    SendOperand(driver, dbid);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
}

LUNATIC_TEST(Stage9MTEUnsupportedAtomicSwapReturnsFail)
{
    TestDriver driver;
    auto request = MatchRequest(driver, CHI::Eb::Opcodes::REQ::AtomicSwap, 0x92);
    request.Size() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    CheckTagMatch(driver, 0x92);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    SendOperand(driver, dbid);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
}

LUNATIC_TEST(Stage9MTEUnsupportedMatchRejectsInapplicableRequest)
{
    TestDriver driver;
    PushRequest(driver, MatchRequest(driver, CHI::Eb::Opcodes::REQ::WriteBackPtl, 0xa7));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 1, response.DBID(), LineData{}, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
}

LUNATIC_TEST(Stage9MTEUnsupportedReadUsesZeroTagResponse)
{
    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, 0x74000, true);
    request.TagOp() = CHI::Eb::TagOp::MatchFetch;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TagOp() == CHI::Eb::TagOp::Invalid);
        LUNATIC_CHECK(data.Tag() == 0);
        LUNATIC_CHECK(data.TU() == 0);
    }
    driver.CompAck(4, 0);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
