#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

LineData DataPattern(uint8_t seed)
{
    LineData data{};
    for (size_t index = 0; index < data.size(); ++index)
        data[index] = uint8_t(seed + index * 7);
    return data;
}

ByteEnables AllBytes()
{
    ByteEnables enables{};
    enables.fill(1);
    return enables;
}

template<typename config>
LineData ReceiveData(CheckedDriver<config>& driver, NodeId source, TransactionId txnId, uint8_t expectedResp)
{
    LineData result{};
    DatabaseId dbid = 0;
    for (size_t beat = 0; beat < BeatsPerLine(driver.driver.node.GetOptions()); ++beat)
    {
        auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TgtID() == source);
        LUNATIC_CHECK(data.TxnID() == txnId);
        LUNATIC_CHECK(data.Resp() == expectedResp);
        dbid = data.DBID();
        const size_t offset = size_t(data.DataID()) * 16;
        for (size_t index = 0; index < BytesPerBeat(driver.driver.node.GetOptions()); ++index)
            result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    driver.CompAck(source, dbid);
    return result;
}

template<typename config>
void CheckResidency(CheckedDriver<config>& driver, const LineKey& line, PolicyKind policy, bool expectedHome)
{
    const HomeLineInfo home = driver.driver.node.InspectHome(line);
    LUNATIC_CHECK(home.valid == expectedHome);
    if (policy == PolicyKind::Exclusive)
        LUNATIC_CHECK(!expectedHome || driver.driver.node.InspectRequester(line, 4) == CacheState::I);
}

void CoherentReadMatrix(PolicyKind policy)
{
    Options options;
    options.policy = policy;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x9000, false);
    const LineData expected = DataPattern(19);
    driver.driver.node.GetMemory().WriteFull(line, expected);

    LUNATIC_CHECK(driver.ReadData(4, 1, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UC);
    CheckResidency(driver, line, policy, policy != PolicyKind::Exclusive);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 2, line.address, true));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    LUNATIC_CHECK(snoop.target == 4);
    LUNATIC_CHECK(snoop.flit.RetToSrc() == 1);
    driver.SnoopData(4, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SC);
    LUNATIC_CHECK(ReceiveData(driver, 5, 2, CHI::Eb::Resps::CompData_SC) == expected);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::SC);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 3, line.address, true));
    auto snoop4 = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    auto snoop5 = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop4.target == 4 && snoop5.target == 5);
    driver.SnoopResponse(5, snoop5.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    driver.SnoopData(4, snoop4.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_I);
    LUNATIC_CHECK(ReceiveData(driver, 6, 3, CHI::Eb::Resps::CompData_UC) == expected);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UC);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 4, 4, line.address, true));
    auto make = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(make.target == 6);
    driver.SnoopData(6, make.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_I);
    auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.Resp() == CHI::Eb::Resps::Comp_UC);
    driver.CompAck(4, comp.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UD);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
}

void Writeback(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId, const LineKey& line,
               uint16_t opcode, const LineData& data, const ByteEnables& enables, uint8_t resp)
{
    driver.PushRequest(driver.Request(opcode, source, txnId, line.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(source, txnId, dbid.DBID(), data, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData, resp);
    driver.Quiesce();
}

void DirtyAndCopybackMatrix(PolicyKind policy)
{
    Options options;
    options.policy = policy;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0xa000, false);
    const LineData initial = DataPattern(31);
    const LineData dirty = DataPattern(101);
    driver.driver.node.GetMemory().WriteFull(line, initial);

    Writeback(driver, 4, 10, line, CHI::Eb::Opcodes::REQ::WriteBackFull, dirty, AllBytes(),
              CHI::Eb::Resps::CopyBackWrData_UD_PD);
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).valid);
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).dirty);

    LUNATIC_CHECK(driver.ReadData(5, 11, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);
    LUNATIC_CHECK(!driver.driver.node.InspectHome(line).dirty);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 6, 12, line.address, true));
    auto shared = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    LUNATIC_CHECK(shared.target == 5);
    driver.SnoopData(5, shared.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_SC_PD);
    LUNATIC_CHECK(ReceiveData(driver, 6, 12, CHI::Eb::Resps::CompData_SD_PD) == dirty);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::SC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SD);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 7, 13, line.address, true));
    auto clean = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    auto dirtySnoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(clean.target == 5 && dirtySnoop.target == 6);
    driver.SnoopData(6, dirtySnoop.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_I_PD);
    driver.SnoopResponse(5, clean.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    LUNATIC_CHECK(ReceiveData(driver, 7, 13, CHI::Eb::Resps::CompData_UD_PD) == dirty);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 7) == CacheState::UD);

    ByteEnables partial{};
    for (size_t index = 3; index < 19; ++index)
        partial[index] = 1;
    LineData partialData = DataPattern(173);
    Writeback(driver, 7, 14, line, CHI::Eb::Opcodes::REQ::WriteBackPtl, partialData, partial,
              CHI::Eb::Resps::CopyBackWrData_UD_PD);
    LineData merged = dirty;
    for (size_t index = 3; index < 19; ++index)
        merged[index] = partialData[index];
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == merged);

    LUNATIC_CHECK(driver.ReadData(8, 15, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == merged);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::UDP);
    Writeback(driver, 8, 16, line, CHI::Eb::Opcodes::REQ::WriteCleanFull, merged, AllBytes(),
              CHI::Eb::Resps::CopyBackWrData_UD_PD);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::UC);
    if (policy == PolicyKind::Exclusive)
    {
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == merged);
    }
    else
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).dirty);

    Writeback(driver, 8, 17, line, CHI::Eb::Opcodes::REQ::WriteEvictFull, merged, AllBytes(),
              CHI::Eb::Resps::CopyBackWrData_UC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::I);
    LUNATIC_CHECK(!driver.driver.node.InspectHome(line).dirty);

    LUNATIC_CHECK(driver.ReadData(9, 18, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == merged);
    driver.Quiesce();
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::Evict, 9, 19, line.address));
    auto evict = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(evict.Resp() == CHI::Eb::Resps::Comp_I);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 9) == CacheState::I);
}

}

LUNATIC_TEST(Stage4CoherentReadsAndPlainSnoops)
{
    CoherentReadMatrix(PolicyKind::NINE);
    CoherentReadMatrix(PolicyKind::Exclusive);
    CoherentReadMatrix(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage4DirtyInterventionCopybacksAndEvictionsNINE)
{
    DirtyAndCopybackMatrix(PolicyKind::NINE);
}

LUNATIC_TEST(Stage4DirtyInterventionCopybacksAndEvictionsExclusive)
{
    DirtyAndCopybackMatrix(PolicyKind::Exclusive);
}

LUNATIC_TEST(Stage4DirtyInterventionCopybacksAndEvictionsInclusive)
{
    DirtyAndCopybackMatrix(PolicyKind::Inclusive);
}
