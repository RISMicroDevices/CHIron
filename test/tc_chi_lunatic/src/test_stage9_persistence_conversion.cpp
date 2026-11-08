#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;

    Options PersistenceOptions(bool separated)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        options.subordinatePersistSepEnabled = separated;
        return options;
    }

    LineData PersistencePattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(43 + index * 5);
        return data;
    }

    ByteEnables FullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    REQ StartPersistence(TestDriver& driver, uint8_t group, uint64_t address)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 50, address);
        request.PGroupID() = group;
        request.Deep() = 1;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        return driver.NextREQ(driver.node.GetOptions().subordinatePersistSepEnabled
            ? CHI::Eb::Opcodes::REQ::CleanSharedPersistSep : CHI::Eb::Opcodes::REQ::CleanSharedPersist);
    }

    void SendBackendResponse(TestDriver& driver, const REQ& request, uint16_t opcode,
                             uint8_t respErr = CHI::Eb::RespErrs::OK)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.RespErr() = respErr;
        if (opcode == CHI::Eb::Opcodes::RSP::CompPersist || opcode == CHI::Eb::Opcodes::RSP::Persist)
            response.PGroupID() = request.PGroupID();
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void CompleteBackendWrite(TestDriver& driver, const REQ& request)
    {
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::DBIDResp);
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(data.TgtID() == 6 && data.TxnID() == 77);
        }
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
    }

    void CheckConvertedGroup(uint8_t group, uint64_t address)
    {
        TestDriver driver(PersistenceOptions(false));
        const auto downstream = StartPersistence(driver, group, address);
        LUNATIC_CHECK(downstream.Opcode() == CHI::Eb::Opcodes::REQ::CleanSharedPersist);
        LUNATIC_CHECK(downstream.PGroupID() == 0);
        LUNATIC_CHECK(downstream.ReturnNID() == 2);
        LUNATIC_CHECK(downstream.ReturnTxnID() == 0);
        LUNATIC_CHECK(downstream.Deep() == 1);
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp);
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
        LUNATIC_CHECK(response.TgtID() == 4 && response.TxnID() == 50);
        LUNATIC_CHECK(response.PGroupID() == group);
        LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::OK);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void SeedDirtyRequester(TestDriver& driver, uint64_t address, const LineData& data)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, address)));
        const auto copyback = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, copyback.DBID(), data, FullEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 91, address, true)));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        CompleteBackendWrite(driver, downstream);
        DataBufferId readDBID = 0;
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
            readDBID = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
        driver.CompAck(6, readDBID);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(LineKey::FromAddress(address, false), 6) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage9PersistenceConversionRetainsOriginalGroups)
{
    CheckConvertedGroup(0x68, 0x6a000);
    CheckConvertedGroup(0x69, 0x6a100);
}

LUNATIC_TEST(Stage9PersistenceConversionPropagatesBackendError)
{
    TestDriver driver(PersistenceOptions(false));
    const auto downstream = StartPersistence(driver, 0x70, 0x6a200);
    LUNATIC_CHECK(downstream.Opcode() == CHI::Eb::Opcodes::REQ::CleanSharedPersist);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::RespErrs::NDERR);
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(response.TgtID() == 4 && response.TxnID() == 50);
    LUNATIC_CHECK(response.PGroupID() == 0x70);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9PersistenceConversionCompletesDirtyWriteback)
{
    TestDriver driver(PersistenceOptions(false));
    const uint64_t address = 0x6a300;
    const LineData data = PersistencePattern();
    SeedDirtyRequester(driver, address, data);

    auto persist = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 50, address);
    persist.PGroupID() = 0x68;
    persist.Deep() = 1;
    LUNATIC_CHECK(driver.node.PushRXREQ(persist));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
    LUNATIC_CHECK(snoop.target == 6);
    driver.WriteData(6, 50, snoop.flit.TxnID(), data, FullEnables(),
        CHI::Eb::Opcodes::DAT::SnpRespData, CHI::Eb::Resps::SnpRespData_UC_PD);

    auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    CompleteBackendWrite(driver, downstream);
    downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersist);
    LUNATIC_CHECK(downstream.PGroupID() == 0 && downstream.Deep() == 1);
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp);
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(response.TgtID() == 4 && response.TxnID() == 50);
    LUNATIC_CHECK(response.PGroupID() == 0x68);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::OK);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9PersistenceCapableSubordinateControl)
{
    TestDriver driver(PersistenceOptions(true));
    const auto downstream = StartPersistence(driver, 0x68, 0x6a400);
    LUNATIC_CHECK(downstream.Opcode() == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    LUNATIC_CHECK(downstream.PGroupID() == 0x68 && downstream.Deep() == 1);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::CompPersist);
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(response.TgtID() == 4 && response.TxnID() == 50);
    LUNATIC_CHECK(response.PGroupID() == 0x68);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
