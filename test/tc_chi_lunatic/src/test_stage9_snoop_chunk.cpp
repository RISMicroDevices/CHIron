#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void SeedUniqueProvider(CheckedDriver<config>& driver, const LineKey& line)
    {
        driver.ReadData(6, 90, line.address, CHI::Eb::Opcodes::REQ::ReadUnique);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UC);
    }

    template<typename config>
    void SeedDirtyProvider(CheckedDriver<config>& driver, const LineKey& line)
    {
        ByteEnables enables{};
        enables.fill(1);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), LineData{}, enables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

    template<typename config>
    void ReturnOrdinarySnoopData(CheckedDriver<config>& driver,
                                 const typename CheckedDriver<config>::SNP& snoop, uint8_t criticalChunk)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = driver.home;
            data.TxnID() = snoop.flit.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = CHI::Eb::Resps::SnpRespData_SC;
            data.DataID() = beat * bytes / 16;
            data.CCID() = criticalChunk;
            data.BE() = ~uint64_t(0);
            LUNATIC_CHECK(data.CCID() == criticalChunk);
            driver.Oracle(6).SnoopData(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
    }

    template<typename config>
    void ReceiveHomeData(CheckedDriver<config>& driver, NodeId source, TransactionId txnId, uint8_t criticalChunk)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        DataBufferId dataBufferId = 0;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source);
            LUNATIC_CHECK(data.TxnID() == txnId);
            LUNATIC_CHECK(data.DataID() == beat * bytes / 16);
            LUNATIC_CHECK(data.CCID() == criticalChunk);
            dataBufferId = data.DBID();
        }
        driver.CompAck(source, dataBufferId);
    }

    template<typename config>
    void ReturnForwardedData(CheckedDriver<config>& driver, const typename CheckedDriver<config>::SNP& snoop,
                             NodeId requester, TransactionId requestTxn, uint8_t criticalChunk)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        driver.Oracle(6).RegisterRequester(requester);
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = requester;
            data.HomeNID() = driver.home;
            data.TxnID() = requestTxn;
            data.DBID() = snoop.flit.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_SD_PD;
            data.DataID() = beat * bytes / 16;
            data.CCID() = criticalChunk;
            data.BE() = ~uint64_t(0);
            LUNATIC_CHECK(data.CCID() == criticalChunk);
            driver.Oracle(6).SnoopData(driver.driver.now, data);
            driver.Oracle(requester).Data(driver.driver.now, data);
        }

        typename CheckedDriver<config>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = driver.home;
        response.TxnID() = snoop.flit.TxnID();
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
        response.Resp() = CHI::Eb::Resps::SnpResp_I;
        response.FwdState() = CHI::Eb::FwdStates::SD_PD;
        driver.Oracle(6).SnoopResponse(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

}

LUNATIC_TEST(Stage9SnoopCriticalChunkOrdinary)
{
    for (uint8_t chunk = 0; chunk < 4; ++chunk)
    {
        CheckedDriver<Config256> driver;
        const LineKey line = LineKey::FromAddress(0x10000 + chunk * 0x1000, false);
        const uint64_t address = line.address + chunk * 16;
        SeedUniqueProvider(driver, line);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, address, true));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        LUNATIC_CHECK((uint64_t(snoop.flit.Addr()) << 3) == address);
        ReturnOrdinarySnoopData(driver, snoop, (address >> 4) & 3);
        ReceiveHomeData(driver, 4, 1, (address >> 4) & 3);
        driver.Quiesce();
    }
}

LUNATIC_TEST(Stage9SnoopCriticalChunkDCT)
{
    for (uint8_t chunk = 0; chunk < 4; ++chunk)
    {
        Options options;
        options.dctEnabled = true;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x20000 + chunk * 0x1000, false);
        const uint64_t address = line.address + chunk * 16;
        SeedDirtyProvider(driver, line);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, address, true);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        driver.PushRequest(request);
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
        LUNATIC_CHECK((uint64_t(snoop.flit.Addr()) << 3) == address);
        ReturnForwardedData(driver, snoop, 4, 1, (address >> 4) & 3);
        driver.CompAck(4, snoop.flit.TxnID());
        driver.Quiesce();
    }
}

LUNATIC_TEST(Stage9SnoopCriticalChunkVictimControl)
{
    Options options;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 2;
    CheckedDriver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x30000, false);
    const LineKey b = LineKey::FromAddress(0x30040, false);
    const LineKey c = LineKey::FromAddress(0x30080, false);
    driver.driver.node.GetMemory().WriteFull(a, LineData{});
    driver.driver.node.GetMemory().WriteFull(b, LineData{});
    driver.driver.node.GetMemory().WriteFull(c, LineData{});
    driver.driver.node.GetEvents()->OnVictimSelection.Register(
        Gravity::MakeListener<VictimSelectionEvent>("chunk-victim", 0, [&](VictimSelectionEvent& event) {
            if (event.domain == VictimDomain::SnoopFilter)
                event.SelectVictim(1);
        }));
    driver.ReadData(4, 1, a.address, CHI::Eb::Opcodes::REQ::ReadUnique);
    driver.Quiesce();
    driver.ReadData(5, 2, b.address, CHI::Eb::Opcodes::REQ::ReadUnique);
    driver.Quiesce();

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 3, c.address + 0x30, true));
    const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(victim.target == 5);
    LUNATIC_CHECK((uint64_t(victim.flit.Addr()) << 3) == b.address);
    driver.SnoopResponse(5, victim.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    ReceiveHomeData(driver, 6, 3, 3);
    driver.Quiesce();
}
