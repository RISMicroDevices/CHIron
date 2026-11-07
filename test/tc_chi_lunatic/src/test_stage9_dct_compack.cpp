#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    const LineKey DCTLine = LineKey::FromAddress(0x20000, false);

    void SeedProvider(CheckedDriver<Config256>& driver)
    {
        const LineData data{};
        ByteEnables enables{};
        enables.fill(1);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, DCTLine.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, enables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, DCTLine.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(DCTLine, 6) == CacheState::UD);
    }

    void StartDCT(CheckedDriver<Config256>& driver)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, DCTLine.address, true);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        driver.PushRequest(request);
    }

    void FinishFallbackAfterRejectedAck(CheckedDriver<Config256>& driver)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == 4);
            dbid = data.DBID();
        }
        driver.CompAck(4, dbid);
        driver.driver.Quiesce();
        for (const auto& [requester, oracle] : driver.oracles)
        {
            (void)requester;
            LUNATIC_CHECK(!oracle->HasInflight());
        }
        LUNATIC_CHECK(driver.driver.node.ValidateInvariants());
        LUNATIC_CHECK(driver.driver.node.GetErrors().size() == 1);
    }

    void RejectFallbackAck(bool beforeSnoopEmission)
    {
        Options options;
        options.dctEnabled = true;
        CheckedDriver<Config256> driver(options);
        SeedProvider(driver);
        StartDCT(driver);

        TransactionId txnId = 0;
        if (beforeSnoopEmission)
        {
            driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXSNP()); });
            txnId = driver.driver.node.PeekTXSNP()->flit.TxnID();
            driver.driver.CompAck(4, txnId);
            LUNATIC_CHECK(driver.driver.node.GetErrors().size() == 1);
            const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
            LUNATIC_CHECK(snoop.flit.TxnID() == txnId);
        }
        else
        {
            const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
            txnId = snoop.flit.TxnID();
        }

        const LineData data{};
        driver.SnoopData(6, txnId, data, CHI::Eb::Resps::SnpRespData_SC_PD);
        if (!beforeSnoopEmission)
        {
            driver.driver.CompAck(4, txnId);
            LUNATIC_CHECK(driver.driver.node.GetErrors().size() == 1);
        }
        FinishFallbackAfterRejectedAck(driver);
    }

}

LUNATIC_TEST(Stage9DCTCompAckRejectedBeforeSnoopEmission)
{
    RejectFallbackAck(true);
}

LUNATIC_TEST(Stage9DCTCompAckRejectedBeforeFallbackData)
{
    RejectFallbackAck(false);
}

LUNATIC_TEST(Stage9DCTCompAckAfterEmissionBeforeProviderResponse)
{
    Options options;
    options.dctEnabled = true;
    CheckedDriver<Config256> driver(options);
    SeedProvider(driver);
    StartDCT(driver);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    driver.Oracle(6).RegisterRequester(4);

    const LineData data{};
    for (size_t beat = 0; beat < 2; ++beat)
    {
        Driver<Config256>::DAT direct{};
        direct.SrcID() = 6;
        direct.TgtID() = 4;
        direct.HomeNID() = 2;
        direct.TxnID() = 1;
        direct.DBID() = snoop.flit.TxnID();
        direct.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        direct.Resp() = CHI::Eb::Resps::CompData_SD_PD;
        direct.DataID() = beat * 2;
        direct.CCID() = 0;
        driver.Oracle(6).SnoopData(driver.driver.now, direct);
        driver.Oracle(4).Data(driver.driver.now, direct);
    }
    driver.CompAck(4, snoop.flit.TxnID());

    Driver<Config256>::RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = snoop.flit.TxnID();
    response.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
    response.Resp() = CHI::Eb::Resps::SnpResp_I;
    response.FwdState() = CHI::Eb::Resps::CompData_SD_PD;
    driver.Oracle(6).SnoopResponse(driver.driver.now, response);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
    driver.driver.Tick();
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(DCTLine, 4) == CacheState::SD);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(DCTLine, 6) == CacheState::I);
}
