#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

LineData RemedData(uint8_t seed)
{
    LineData data{};
    for (size_t index = 0; index < data.size(); ++index)
        data[index] = uint8_t(seed + index * 3);
    return data;
}

ByteEnables RemedEnables()
{
    ByteEnables enables{};
    enables.fill(1);
    return enables;
}

void RemedWriteback(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId,
                    const LineKey& line, const LineData& data, uint8_t resp = CHI::Eb::Resps::CopyBackWrData_UD_PD,
                    uint16_t opcode = CHI::Eb::Opcodes::REQ::WriteBackFull)
{
    driver.PushRequest(driver.Request(opcode, source, txnId, line.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(source, txnId, dbid.DBID(), data, RemedEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData, resp);
    driver.Quiesce();
}

LineData RemedReceive(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId,
                      uint8_t resp, bool acknowledge = false)
{
    LineData result{};
    DatabaseId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.Resp() == resp);
        dbid = data.DBID();
        const size_t offset = size_t(data.DataID()) * 16;
        for (size_t index = 0; index < 32; ++index)
            result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    if (acknowledge)
        driver.CompAck(source, dbid);
    return result;
}

void ReadOnceDirtyPolicy(PolicyKind policy)
{
    Options options;
    options.policy = policy;
    options.slcSets = 1;
    options.slcWays = 1;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x15000, false);
    const LineKey other = LineKey::FromAddress(0x15040, false);
    const LineData data = RemedData(71);
    RemedWriteback(driver, 4, 91, line, data);
    LUNATIC_CHECK(driver.ReadData(5, 92, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
    driver.Quiesce();

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadOnce, 6, 93, line.address));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpOnce);
    driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I_PD);
    LUNATIC_CHECK(RemedReceive(driver, 6, 93, CHI::Eb::Resps::CompData_UC) == data);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).dirty);

    RemedWriteback(driver, 7, 94, other, RemedData(99));
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == data);
}

template<typename config>
void SublineReadGeometry()
{
    Driver<config> driver;
    const LineKey line = LineKey::FromAddress(0x16000, false);
    const LineData data = RemedData(103);
    driver.node.GetMemory().WriteFull(line, data);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 100, line.address + 0x30);
    request.Size() = 4;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(response.DataID() == (config::dataWidth == 512 ? 0 : 3));
    LUNATIC_CHECK(response.CCID() == 3);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.PeekTXDAT());
}

}

LUNATIC_TEST(Stage4ReadOncePassDirtySurvivesVictim)
{
    ReadOnceDirtyPolicy(PolicyKind::NINE);
    ReadOnceDirtyPolicy(PolicyKind::Exclusive);
    ReadOnceDirtyPolicy(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage4WriteCleanFullSDPDBecomesSC)
{
    for (PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x17000, false);
        const LineData data = RemedData(107);
        RemedWriteback(driver, 4, 110, line, data);
        LUNATIC_CHECK(driver.ReadData(5, 111, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        driver.Quiesce();
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 6, 112, line.address, true));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_SC_PD);
        LUNATIC_CHECK(RemedReceive(driver, 6, 112, CHI::Eb::Resps::CompData_SD_PD, true) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SD);

        RemedWriteback(driver, 6, 113, line, data, CHI::Eb::Resps::CopyBackWrData_SD_PD,
                       CHI::Eb::Opcodes::REQ::WriteCleanFull);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
    }
}

LUNATIC_TEST(Stage4SingleWriteDataCancelCompletes)
{
    CheckedDriver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x18000, false);
    const LineData original = RemedData(113);
    driver.driver.node.GetMemory().WriteFull(line, original);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 120, line.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    Driver<Config256>::DAT cancel{};
    cancel.SrcID() = 4;
    cancel.TgtID() = 2;
    cancel.TxnID() = dbid.DBID();
    cancel.DBID() = dbid.DBID();
    cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
    driver.Oracle(4).WriteData(driver.driver.now, cancel);
    LUNATIC_CHECK(driver.driver.node.PushRXDAT(cancel));
    driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == original);
}

LUNATIC_TEST(Stage4SublineReadAndCCID128)
{
    SublineReadGeometry<Config128>();
}

LUNATIC_TEST(Stage4SublineReadAndCCID512)
{
    SublineReadGeometry<Config512>();
}

LUNATIC_TEST(Stage4SublineCHIBackendRead)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config128> driver(options);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 121, 0x16030);
    request.Size() = 4;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.Size() == 4);
    Driver<Config128>::DAT data{};
    data.SrcID() = 6;
    data.TgtID() = 2;
    data.TxnID() = downstream.TxnID();
    data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    data.DataID() = 3;
    data.CCID() = 3;
    LUNATIC_CHECK(driver.node.PushRXDAT(data));
    driver.Tick();
    auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(response.DataID() == 3 && response.CCID() == 3);
    driver.Quiesce();
}

LUNATIC_TEST(Stage4ReadReceiptPrecedesDataEmission)
{
    Driver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 122, 0x19000);
    request.Order() = 2;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });
    for (size_t cycle = 0; cycle < 20; ++cycle)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
    }
    auto receipt = driver.node.PopTXRSP();
    LUNATIC_CHECK(receipt && receipt->Opcode() == CHI::Eb::Opcodes::RSP::ReadReceipt);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
}

LUNATIC_TEST(Stage4EarlyCompAckDoesNotRetire)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 123, 0x1a000, true)));
    driver.Until([&] { return driver.node.GetTime() >= 4; });
    Driver<Config256>::RSP ack{};
    ack.SrcID() = 4;
    ack.TgtID() = 2;
    ack.TxnID() = 0;
    ack.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
    LUNATIC_CHECK(driver.node.PushRXRSP(ack));
    driver.Tick();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.CompAck(4, first.DBID());
    driver.Quiesce();
}

LUNATIC_TEST(Stage4UnsupportedRequestGetsErrorCompletion)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 124, 0x1b000)));
    auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(response.RespErr() == 0b10);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    driver.Quiesce();
}
