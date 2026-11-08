#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;
    using DAT        = TestDriver::DAT;

    REQ StartBackendRead(TestDriver& driver, NodeId source, TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, source, txnId, address, true);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        return driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    }

    void SendBackendReadData(TestDriver& driver, const REQ& request, uint8_t respErr)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = request.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.RespErr() = respErr;
            data.DataID() = beat * 2;
            data.CCID() = (request.Addr() >> 4) & 3;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    void ReceiveReadData(TestDriver& driver, NodeId source, TransactionId txnId, uint8_t respErr)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source && data.TxnID() == txnId);
            LUNATIC_CHECK(data.RespErr() == respErr);
            dbid = data.DBID();
        }
        driver.CompAck(source, dbid);
    }

    void SendCleanInvalidSnoopData(TestDriver& driver, const TestDriver::SNP& snoop, uint64_t address)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 4;
            data.TgtID() = 2;
            data.HomeNID() = 0;
            data.TxnID() = snoop.flit.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
            data.BE() = ~uint64_t(0);
            data.DataID() = beat * 2;
            data.CCID() = (address >> 4) & 3;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

}

LUNATIC_TEST(Stage9DERRReadPreservesUniqueOwnership)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::NINE;
    TestDriver driver(options);
    constexpr uint64_t address = 0x69000;

    const auto first = StartBackendRead(driver, 4, 1, address);
    SendBackendReadData(driver, first, CHI::Eb::RespErrs::DERR);
    ReceiveReadData(driver, 4, 1, CHI::Eb::RespErrs::DERR);
    driver.Quiesce();
    const CacheState firstState = driver.node.InspectRequester(LineKey::FromAddress(address, false), 4);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 2, address, true)));
    driver.Until([&] {
        return static_cast<bool>(driver.node.PeekTXSNP()) || static_cast<bool>(driver.node.PeekTXREQ());
    });
    bool secondUsedBackend = false;
    if (driver.node.PeekTXSNP())
    {
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(snoop.target == 4);
        SendCleanInvalidSnoopData(driver, snoop, address);
    }
    else
    {
        secondUsedBackend = true;
        const auto second = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        SendBackendReadData(driver, second, CHI::Eb::RespErrs::OK);
    }
    ReceiveReadData(driver, 5, 2, CHI::Eb::RespErrs::OK);
    driver.Quiesce();

    const LineKey line = LineKey::FromAddress(address, false);
    LUNATIC_CHECK(firstState == CacheState::UC);
    LUNATIC_CHECK(!secondUsedBackend);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 5) == CacheState::UD);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9NDERRReadDoesNotAllocate)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::NINE;
    TestDriver driver(options);
    constexpr uint64_t address = 0x69100;

    const auto request = StartBackendRead(driver, 4, 1, address);
    SendBackendReadData(driver, request, CHI::Eb::RespErrs::NDERR);
    ReceiveReadData(driver, 4, 1, CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();

    const LineKey line = LineKey::FromAddress(address, false);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
    LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
