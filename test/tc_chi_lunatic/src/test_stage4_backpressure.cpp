#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

LineData BackpressureData(uint8_t seed)
{
    LineData data{};
    for (size_t index = 0; index < data.size(); ++index)
        data[index] = uint8_t(seed + index * 5);
    return data;
}

ByteEnables BackpressureEnables()
{
    ByteEnables enables{};
    enables.fill(1);
    return enables;
}

void BackpressureWriteback(CheckedDriver<Config256>& driver, const LineKey& line, const LineData& data)
{
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 1, line.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(4, 1, dbid.DBID(), data, BackpressureEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
}

template<typename config>
void CoherentGeometry()
{
    CheckedDriver<config> driver;
    const LineKey line = LineKey::FromAddress(0x25000, false);
    const LineData expected = BackpressureData(47);
    driver.driver.node.GetMemory().WriteFull(line, expected);
    LUNATIC_CHECK(driver.ReadData(4, 1, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
    driver.Quiesce();
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 2, line.address, true));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    driver.SnoopData(4, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SC);

    LineData actual{};
    DatabaseId dbid = 0;
    for (size_t beat = 0; beat < BeatsPerLine(driver.driver.node.GetOptions()); ++beat)
    {
        auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_SC);
        dbid = data.DBID();
        const size_t offset = size_t(data.DataID()) * 16;
        for (size_t index = 0; index < BytesPerBeat(driver.driver.node.GetOptions()); ++index)
            actual[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    driver.CompAck(5, dbid);
    driver.Quiesce();
    LUNATIC_CHECK(actual == expected);
}

}

LUNATIC_TEST(Stage4BackpressuredMakeUniqueCommitIsIdempotent)
{
    Options options;
    options.outputQueueLimit = 1;
    options.policy = PolicyKind::Exclusive;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x26000, false);
    BackpressureWriteback(driver, line, BackpressureData(53));

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpZero, 4, 2, line.address));
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 5, 3, line.address, true));
    for (size_t cycle = 0; cycle < 20; ++cycle)
        driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.CompAck(5, comp.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);
}

LUNATIC_TEST(Stage4BackpressuredCMOCommitIsIdempotent)
{
    Options options;
    options.outputQueueLimit = 1;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x27000, false);
    const LineData dirty = BackpressureData(59);
    BackpressureWriteback(driver, line, dirty);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpZero, 4, 4, line.address));
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 5, 5, line.address));
    for (size_t cycle = 0; cycle < 20; ++cycle)
        driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == dirty);
}

LUNATIC_TEST(Stage4CoherentReadGeometry128)
{
    CoherentGeometry<Config128>();
}

LUNATIC_TEST(Stage4CoherentReadGeometry512)
{
    CoherentGeometry<Config512>();
}
