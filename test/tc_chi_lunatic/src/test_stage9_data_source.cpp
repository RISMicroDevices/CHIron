#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData DataSourcePattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    ByteEnables DataSourceFullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    void SeedDirtyProvider(CheckedDriver<config>& driver, NodeId provider, const LineKey& line,
                           const LineData& data, TransactionId txnId)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, provider, txnId, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(provider, txnId, response.DBID(), data, DataSourceFullEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(provider, txnId + 1, line.address,
            CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, provider) == CacheState::UD);
    }

    template<typename config>
    void SendDataSourcePackets(CheckedDriver<config>& driver, NodeId provider,
                               const typename CheckedDriver<config>::SNP& snoop,
                               const LineData& line, uint8_t resp, bool partial = false)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = provider;
            data.TgtID() = driver.home;
            data.TxnID() = snoop.flit.TxnID();
            data.Opcode() = partial ? CHI::Eb::Opcodes::DAT::SnpRespDataPtl
                : CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = resp;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (snoop.flit.Addr() >> 1) & 3;
            data.DataSource() = beat == 0 ? 1 : 8;
            for (size_t index = 0; index < bytes; ++index)
                if (!partial || index < bytes / 2)
                {
                    data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
                    data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
                }
            driver.Oracle(provider).SnoopData(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    }

    template<typename config>
    void ReceiveDataSourceRead(CheckedDriver<config>& driver, NodeId source, TransactionId txnId,
                               const LineData& expected, uint8_t criticalChunk)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        DataBufferId dataBufferId = 0;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source && data.TxnID() == txnId);
            LUNATIC_CHECK(data.CCID() == criticalChunk);
            for (size_t index = 0; index < bytes; ++index)
                LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) ==
                    expected[beat * bytes + index]);
            dataBufferId = data.DBID();
        }
        driver.CompAck(source, dataBufferId);
    }

    void RunOrdinaryDataSource(bool partial)
    {
        CheckedDriver<Config256> driver;
        const LineKey line = LineKey::FromAddress(partial ? 0x61040 : 0x61000, false);
        const LineData baseline = DataSourcePattern(19);
        const LineData dirty = DataSourcePattern(113);
        LineData expected = baseline;
        if (partial)
        {
            ByteEnables enables{};
            for (size_t index = 0; index < enables.size(); ++index)
                enables[index] = index % 32 < 16;
            for (size_t index = 0; index < expected.size(); ++index)
                if (enables[index])
                    expected[index] = dirty[index];
            driver.driver.node.GetMemory().WriteFull(line, baseline);
            driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 6, 90, line.address));
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
            driver.WriteData(6, 90, response.DBID(), dirty, enables,
                CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
            driver.Quiesce();
            LUNATIC_CHECK(driver.ReadData(6, 91, line.address,
                CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
            driver.Quiesce();
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UDP);
        }
        else
        {
            expected = dirty;
            SeedDirtyProvider(driver, 6, line, dirty, 90);
        }

        const uint64_t address = line.address + 0x30;
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 100, address, true));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        SendDataSourcePackets(driver, 6, snoop, dirty,
            partial ? CHI::Eb::Resps::SnpRespDataPtl_I_PD : CHI::Eb::Resps::SnpRespData_I_PD, partial);
        ReceiveDataSourceRead(driver, 4, 100, expected, 3);
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage9DataSourceOrdinaryFull)
{
    RunOrdinaryDataSource(false);
}

LUNATIC_TEST(Stage9DataSourceOrdinaryPartial)
{
    RunOrdinaryDataSource(true);
}

LUNATIC_TEST(Stage9DataSourceDCTFallback)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x61100, false);
    const LineData expected = DataSourcePattern(47);
    SeedDirtyProvider(driver, 6, line, expected, 90);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 100, line.address + 0x10, true);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    driver.PushRequest(request);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    SendDataSourcePackets(driver, 6, snoop, expected, CHI::Eb::Resps::SnpRespData_SC_PD);
    ReceiveDataSourceRead(driver, 4, 100, expected, 1);
    driver.Quiesce();
}

LUNATIC_TEST(Stage9DataSourceVictim)
{
    Options options;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 2;
    CheckedDriver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x61200, false);
    const LineKey b = LineKey::FromAddress(0x61240, false);
    const LineKey c = LineKey::FromAddress(0x61280, false);
    const LineData dataA = DataSourcePattern(17);
    const LineData dataB = DataSourcePattern(83);
    const LineData dataC = DataSourcePattern(157);
    driver.driver.node.GetMemory().WriteFull(a, dataA);
    driver.driver.node.GetMemory().WriteFull(b, dataB);
    driver.driver.node.GetMemory().WriteFull(c, dataC);
    driver.driver.node.GetEvents()->OnVictimSelection.Register(
        Gravity::MakeListener<VictimSelectionEvent>("data-source-victim", 0, [&](VictimSelectionEvent& event) {
            if (event.domain == VictimDomain::SnoopFilter)
                event.SelectVictim(1);
        }));
    LUNATIC_CHECK(driver.ReadData(4, 1, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
    driver.Quiesce();
    SeedDirtyProvider(driver, 5, b, dataB, 90);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 3, c.address, true));
    const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(victim.target == 5);
    SendDataSourcePackets(driver, 5, victim, dataB, CHI::Eb::Resps::SnpRespData_I_PD);
    ReceiveDataSourceRead(driver, 6, 3, dataC, 0);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(b) == dataB);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(b, 5) == CacheState::I);
}
