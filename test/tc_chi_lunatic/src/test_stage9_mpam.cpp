#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using ConfigMPAM = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 256, false, false, true>;
    using TestDriver = CheckedDriver<ConfigMPAM>;
    using REQ = TestDriver::REQ;
    using RSP = TestDriver::RSP;
    using DAT = TestDriver::DAT;

    constexpr uint16_t NonDefaultMPAM = 0x655;
    constexpr uint16_t OtherMPAM = 0x2ab;

    Options BackendOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        return options;
    }

    void CompleteBackendRead(TestDriver& driver, const REQ& downstream)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = downstream.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * 2;
            data.CCID() = 0;
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
        for (size_t beat = 0; beat < 2; ++beat)
            driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.Quiesce();
    }

    void SendResponse(TestDriver& driver, const REQ& downstream, uint16_t opcode, uint8_t creditType = 0)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = downstream.TxnID();
        response.Opcode() = opcode;
        response.PCrdType() = creditType;
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

}

LUNATIC_TEST(Stage9MPAMDownstreamAndRetryPreserveRawValue)
{
    TestDriver driver(BackendOptions());
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x73000);
    request.NS() = 1;
    request.MPAM() = NonDefaultMPAM;
    driver.PushRequest(request);
    const auto first = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(first.MPAM() == NonDefaultMPAM);
    LUNATIC_CHECK(first.NS() == 1);
    SendResponse(driver, first, CHI::Eb::Opcodes::RSP::RetryAck, 1);
    SendResponse(driver, first, CHI::Eb::Opcodes::RSP::PCrdGrant, 1);
    const auto resent = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(resent.MPAM() == NonDefaultMPAM);
    LUNATIC_CHECK(resent.PCrdType() == 1 && resent.AllowRetry() == 0);
    CompleteBackendRead(driver, resent);
}

LUNATIC_TEST(Stage9MPAMStashSnoopPreservesRawValue)
{
    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::StashOnceShared, 4, 1, 0x73100);
    request.ReturnNID() = 5;
    request.StashNIDValid() = 1;
    request.NS() = 1;
    request.MPAM() = OtherMPAM;
    driver.PushRequest(request);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashShared);
    LUNATIC_CHECK(snoop.flit.MPAM() == OtherMPAM);
    LUNATIC_CHECK(snoop.flit.NS() == 1);
    driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
}

LUNATIC_TEST(Stage9MPAMNonStashSnoopUsesDefaults)
{
    TestDriver driver;
    auto seed = driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 90, 0x73200, true);
    seed.NS() = 1;
    seed.MPAM() = OtherMPAM;
    driver.PushRequest(seed);
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.CompAck(6, 0);
    driver.Quiesce();

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, 0x73200, true);
    request.NS() = 1;
    request.MPAM() = NonDefaultMPAM;
    driver.PushRequest(request);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    LUNATIC_CHECK(snoop.flit.MPAM() == 0x400);
    LUNATIC_CHECK(snoop.flit.NS() == 1);
    driver.SnoopData(6, snoop.flit.TxnID(), LineData{}, CHI::Eb::Resps::SnpRespData_SC);
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.CompAck(4, 1);
    driver.Quiesce();
}
