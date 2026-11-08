#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void SeedDirtyProvider(CheckedDriver<config>& driver, const LineKey& line, const LineData& data)
    {
        ByteEnables enables{};
        enables.fill(1);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
            CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

    template<typename config>
    typename CheckedDriver<config>::SNP StartDCT(CheckedDriver<config>& driver, const LineKey& line)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line.address, true);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        driver.PushRequest(request);
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
        LUNATIC_CHECK(snoop.flit.FwdNID() == 4);
        LUNATIC_CHECK(snoop.flit.FwdTxnID() == 1);
        driver.Oracle(6).RegisterRequester(4);
        return snoop;
    }

}

LUNATIC_TEST(Stage6DCTForwardedRead)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x10000, false);
    LineData expected{};
    for (size_t index = 0; index < expected.size(); ++index)
        expected[index] = uint8_t(0x5a + index * 13);
    SeedDirtyProvider(driver, line, expected);
    const auto snoop = StartDCT(driver, line);

    constexpr size_t bytes = Config256::dataWidth / 8;
    LineData observed{};
    for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
    {
        typename CheckedDriver<Config256>::DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 4;
        data.HomeNID() = 2;
        data.TxnID() = 1;
        data.DBID() = snoop.flit.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_SD_PD;
        data.DataID() = beat * bytes / 16;
        data.CCID() = 0;
        data.BE() = ~uint64_t(0);
        for (size_t index = 0; index < bytes; ++index)
            data.Data()[index / 8] |= uint64_t(expected[beat * bytes + index]) << ((index % 8) * 8);
        driver.Oracle(6).SnoopData(driver.driver.now, data);
        driver.Oracle(4).Data(driver.driver.now, data);
        for (size_t index = 0; index < bytes; ++index)
            observed[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    LUNATIC_CHECK(observed == expected);

    typename CheckedDriver<Config256>::RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = snoop.flit.TxnID();
    response.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
    response.Resp() = CHI::Eb::Resps::SnpResp_I;
    response.FwdState() = CHI::Eb::Resps::CompData_SD_PD;
    driver.Oracle(6).SnoopResponse(driver.driver.now, response);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
    driver.driver.Tick();
    LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
    driver.CompAck(4, snoop.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SD);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage6DCTOrdinaryFallback)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x10000, false);
    LineData expected{};
    for (size_t index = 0; index < expected.size(); ++index)
        expected[index] = uint8_t(0xa5 ^ (index * 29));
    SeedDirtyProvider(driver, line, expected);
    const auto snoop = StartDCT(driver, line);

    driver.SnoopData(6, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SC_PD);
    constexpr size_t bytes = Config256::dataWidth / 8;
    LineData observed{};
    DataBufferId dataBufferId = 0;
    for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.DBID() == snoop.flit.TxnID());
        dataBufferId = data.DBID();
        for (size_t index = 0; index < bytes; ++index)
            observed[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    LUNATIC_CHECK(observed == expected);
    driver.CompAck(4, dataBufferId);
    driver.Quiesce();
}
