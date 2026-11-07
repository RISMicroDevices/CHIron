#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;
    using DAT        = TestDriver::DAT;

    REQ StartWrite(TestDriver& driver)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 1, 0x69700)));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 4;
            data.TgtID() = 2;
            data.TxnID() = dbid;
            data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
            data.RespErr() = beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK;
            data.DataID() = beat * 2;
            data.CCID() = 0;
            data.BE() = 0xffffffff;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        return driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    }

    void SendBackendResponse(TestDriver& driver, const REQ& request, uint16_t opcode, uint8_t respErr)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 88;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.RespErr() = respErr;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

}

LUNATIC_TEST(Stage9WriteDataErrorsPreserveEveryBeat)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    TestDriver driver(options);
    const auto downstream = StartWrite(driver);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::RespErrs::OK);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(data.RespErr() == (beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK));
    }
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::RespErrs::OK);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9WriteCompletionErrorsPropagate)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    TestDriver driver(options);
    const auto downstream = StartWrite(driver);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::RespErrs::OK);
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::RespErrs::NDERR);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
