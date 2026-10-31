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
    typename CheckedDriver<config>::SNP StartDCT(CheckedDriver<config>& driver, uint16_t opcode,
                                                 NodeId source, TransactionId txnId, const LineKey& line,
                                                 uint16_t expectedSnoop)
    {
        auto request = driver.Request(opcode, source, txnId, line.address, true);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        driver.PushRequest(request);
        return driver.NextSNP(expectedSnoop);
    }

    template<typename config>
    void SendDirectData(CheckedDriver<config>& driver, NodeId provider, NodeId requester,
                        TransactionId requestTxn, const typename CheckedDriver<config>::SNP& snoop,
                        const LineData& line, uint8_t resp)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        driver.Oracle(provider).RegisterRequester(requester);
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = provider;
            data.TgtID() = requester;
            data.HomeNID() = driver.home;
            data.TxnID() = requestTxn;
            data.DBID() = snoop.flit.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = resp;
            data.DataID() = beat * bytes / 16;
            data.CCID() = 0;
            data.BE() = ~uint64_t(0);
            for (size_t index = 0; index < bytes; ++index)
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
            driver.Oracle(provider).SnoopData(driver.driver.now, data);
            driver.Oracle(requester).Data(driver.driver.now, data);
        }
    }

    template<typename config>
    void SendForwardedResponse(CheckedDriver<config>& driver, NodeId provider,
                               const typename CheckedDriver<config>::SNP& snoop, uint8_t resp, uint8_t fwdState)
    {
        typename CheckedDriver<config>::RSP response{};
        response.SrcID() = provider;
        response.TgtID() = driver.home;
        response.TxnID() = snoop.flit.TxnID();
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
        response.Resp() = resp;
        response.FwdState() = fwdState;
        driver.Oracle(provider).SnoopResponse(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

    template<typename config>
    void SendForwardedHomeData(CheckedDriver<config>& driver, NodeId provider,
                               const typename CheckedDriver<config>::SNP& snoop, const LineData& line,
                               uint8_t resp, uint8_t fwdState)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = provider;
            data.TgtID() = driver.home;
            data.TxnID() = snoop.flit.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespDataFwded;
            data.Resp() = resp;
            data.FwdState() = fwdState;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (beat * bytes / 16) % 2;
            data.BE() = ~uint64_t(0);
            for (size_t index = 0; index < bytes; ++index)
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
            driver.Oracle(provider).SnoopData(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
    }

    template<typename config>
    void ReceiveHomeData(CheckedDriver<config>& driver, NodeId requester)
    {
        DataBufferId dataBufferId = 0;
        for (size_t beat = 0; beat < LINE_SIZE / (config::dataWidth / 8); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == requester);
            dataBufferId = data.DBID();
        }
        driver.CompAck(requester, dataBufferId);
    }

    template<typename config>
    void SeedSharedDirtyPair(CheckedDriver<config>& driver, const LineKey& line, const LineData& data)
    {
        SeedDirtyProvider(driver, line, data);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 8, 92, line.address, true));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        driver.SnoopData(6, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_SC_PD);
        ReceiveHomeData(driver, 8);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::SD);
    }

}

LUNATIC_TEST(Stage8DCTUniqueForwardsSingleOwner)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x51000, false);
    const LineData expected{};
    driver.ReadData(6, 90, line.address, CHI::Eb::Opcodes::REQ::ReadUnique);
    driver.Quiesce();

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpUniqueFwd);
    LUNATIC_CHECK(snoop.flit.DoNotGoToSD());
    SendDirectData(driver, 6, 4, 1, snoop, expected, CHI::Eb::Resps::CompData_UC);
    SendForwardedResponse(driver, 6, snoop, CHI::Eb::Resps::SnpResp_I, CHI::Eb::FwdStates::UC);
    driver.CompAck(4, snoop.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
}

LUNATIC_TEST(Stage8DCTUniqueMultipleOwnersUseNormalSnoops)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x52000, false);
    driver.ReadData(6, 90, line.address, CHI::Eb::Opcodes::REQ::ReadShared);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 8, 91, line.address, true));
    const auto seedSnoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    driver.SnoopData(6, seedSnoop.flit.TxnID(), LineData{}, CHI::Eb::Resps::SnpRespData_SC);
    ReceiveHomeData(driver, 8);
    driver.Quiesce();

    const auto first = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpUnique);
    const auto second = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(first.target == 6);
    LUNATIC_CHECK(second.target == 8);
    LUNATIC_CHECK(first.flit.RetToSrc());
    LUNATIC_CHECK(!second.flit.RetToSrc());
    driver.SnoopResponse(6, first.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    driver.SnoopResponse(8, second.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    ReceiveHomeData(driver, 4);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::I);
}

LUNATIC_TEST(Stage8DCTPreferUniqueMultipleOwnersUseNormalSnoops)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x53000, false);
    const LineData expected{};
    SeedSharedDirtyPair(driver, line, expected);

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadPreferUnique, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpPreferUnique);
    LUNATIC_CHECK(snoop.target == 8);
    driver.SnoopData(8, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SD);
    ReceiveHomeData(driver, 4);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::SD);
}

LUNATIC_TEST(Stage8DCTDoubleDataCommitsRequesterAndHome)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x54000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    SendDirectData(driver, 6, 4, 1, snoop, expected, CHI::Eb::Resps::CompData_SC);
    SendForwardedHomeData(driver, 6, snoop, expected, CHI::Eb::Resps::SnpRespData_I_PD,
        CHI::Eb::FwdStates::SC);
    driver.CompAck(4, snoop.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).valid);
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).dirty);
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);
    LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
}

LUNATIC_TEST(Stage8DCTEventForcesNormalSnoop)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x55000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    size_t decisions = 0;
    driver.driver.node.GetEvents()->OnSnoopForwarding.Register(
        Gravity::MakeListener<SnoopForwardingEvent>("normal", 0, [&](SnoopForwardingEvent& event) {
            ++decisions;
            LUNATIC_CHECK(event.home == 2);
            LUNATIC_CHECK(event.request.opcode == CHI::Eb::Opcodes::REQ::ReadShared);
            LUNATIC_CHECK(event.request.source == 4);
            LUNATIC_CHECK(event.provider == 6);
            LUNATIC_CHECK(event.providerState == CacheState::UD);
            LUNATIC_CHECK(event.trackedOwners == 1);
            LUNATIC_CHECK(!event.homeHit);
            LUNATIC_CHECK(!event.homeDirty);
            event.UseNonForwarding();
        }));
    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpShared);
    LUNATIC_CHECK(snoop.flit.FwdNID() == 0);
    LUNATIC_CHECK(snoop.flit.FwdTxnID() == 0);
    driver.SnoopData(6, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SC_PD);
    ReceiveHomeData(driver, 4);
    driver.Quiesce();
    LUNATIC_CHECK(decisions == 1);
}

LUNATIC_TEST(Stage8DCTEventSelectsForwarding)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x56000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    size_t decisions = 0;
    driver.driver.node.GetEvents()->OnSnoopForwarding.Register(
        Gravity::MakeListener<SnoopForwardingEvent>("forward", 0, [&](SnoopForwardingEvent& event) {
            ++decisions;
            LUNATIC_CHECK(event.IsForwarding());
            event.UseForwarding();
        }));
    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    SendDirectData(driver, 6, 4, 1, snoop, expected, CHI::Eb::Resps::CompData_SD_PD);
    SendForwardedResponse(driver, 6, snoop, CHI::Eb::Resps::SnpResp_I, CHI::Eb::FwdStates::SD_PD);
    driver.CompAck(4, snoop.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(decisions == 1);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SD);
    LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
    LUNATIC_CHECK(decisions == 1);
}

LUNATIC_TEST(Stage8DCTEventDisabledOffersNoChoice)
{
    Options options;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x57000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    size_t decisions = 0;
    driver.driver.node.GetEvents()->OnSnoopForwarding.Register(
        Gravity::MakeListener<SnoopForwardingEvent>("disabled", 0, [&](SnoopForwardingEvent& event) {
            ++decisions;
            event.UseForwarding();
        }));
    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpShared);
    driver.SnoopData(6, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SC_PD);
    ReceiveHomeData(driver, 4);
    driver.Quiesce();
    LUNATIC_CHECK(decisions == 0);
}

LUNATIC_TEST(Stage8DCTPreferUniqueForwardsSingleOwner)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x58000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadPreferUnique, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpPreferUniqueFwd);
    SendDirectData(driver, 6, 4, 1, snoop, expected, CHI::Eb::Resps::CompData_UD_PD);
    SendForwardedResponse(driver, 6, snoop, CHI::Eb::Resps::SnpResp_I, CHI::Eb::FwdStates::UD_PD);
    driver.CompAck(4, snoop.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UD);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
}

LUNATIC_TEST(Stage8DCTRejectsSharedUniqueForward)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x59000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    SendForwardedResponse(driver, 6, snoop, CHI::Eb::Resps::SnpResp_I, CHI::Eb::FwdStates::UD_PD);
    LUNATIC_CHECK(!driver.driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.driver.node.HasInflight());
}

LUNATIC_TEST(Stage8DCTRejectsPreferUniqueInvalidCorrelation)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x5a000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadPreferUnique, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpPreferUniqueFwd);
    SendForwardedResponse(driver, 6, snoop, CHI::Eb::Resps::SnpResp_I, CHI::Eb::FwdStates::SC);
    LUNATIC_CHECK(!driver.driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.driver.node.HasInflight());
}

LUNATIC_TEST(Stage8DCTForwardedReadAllPoliciesWithEarlyCompAck)
{
    for (PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
    {
        Options options;
        options.policy = policy;
        options.dctEnabled = true;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x5b000, false);
        const LineData expected{};
        SeedDirtyProvider(driver, line, expected);

        const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
            CHI::Eb::Opcodes::SNP::SnpSharedFwd);
        SendDirectData(driver, 6, 4, 1, snoop, expected, CHI::Eb::Resps::CompData_SD_PD);
        driver.CompAck(4, snoop.flit.TxnID());
        SendForwardedResponse(driver, 6, snoop, CHI::Eb::Resps::SnpResp_I, CHI::Eb::FwdStates::SD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SD);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    }
}

LUNATIC_TEST(Stage8DCTMixedForwardAndOrdinaryInvalidation)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x5c000, false);
    const LineData expected{};
    SeedSharedDirtyPair(driver, line, expected);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid, 4, 1, line.address, true);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    request.SnpAttr() = 1;
    driver.PushRequest(request);
    const auto forwarded = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpOnceFwd);
    const auto invalidated = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(forwarded.target == 6);
    LUNATIC_CHECK(invalidated.target == 8);
    LUNATIC_CHECK(!forwarded.flit.RetToSrc());
    LUNATIC_CHECK(invalidated.flit.RetToSrc());
    SendDirectData(driver, 6, 4, 1, forwarded, expected, CHI::Eb::Resps::CompData_I);
    SendForwardedResponse(driver, 6, forwarded, CHI::Eb::Resps::SnpResp_SC, CHI::Eb::FwdStates::I);
    driver.SnoopData(8, invalidated.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_I_PD);
    driver.CompAck(4, forwarded.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == expected);
}

LUNATIC_TEST(Stage8DCTRejectsCleanHomeDataReturn)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x5d000, false);
    const LineData expected{};
    driver.ReadData(6, 90, line.address, CHI::Eb::Opcodes::REQ::ReadUnique);
    driver.Quiesce();

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    SendForwardedHomeData(driver, 6, snoop, expected, CHI::Eb::Resps::SnpRespData_SC,
        CHI::Eb::FwdStates::SC);
    LUNATIC_CHECK(!driver.driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.driver.node.HasInflight());
}

LUNATIC_TEST(Stage8DCTRejectsDirtyHomeCopyWithoutPassDirty)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x5e000, false);
    const LineData expected{};
    SeedDirtyProvider(driver, line, expected);

    const auto snoop = StartDCT(driver, CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line,
        CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    SendForwardedHomeData(driver, 6, snoop, expected, CHI::Eb::Resps::SnpRespData_I,
        CHI::Eb::FwdStates::SD_PD);
    LUNATIC_CHECK(!driver.driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.driver.node.HasInflight());
}
