#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;

    Options ErrorBackendOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        return options;
    }

    void SendResponse(TestDriver& driver, const REQ& request, uint16_t opcode, uint8_t respErr)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = opcode == CHI::Eb::Opcodes::RSP::Persist ? uint16_t(0) : uint16_t(request.TxnID());
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_UC;
        response.RespErr() = respErr;
        response.PGroupID() = 0x68;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    REQ StartPersistence(TestDriver& driver)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 50, 0x69500);
        request.PGroupID() = 0x68;
        request.Deep() = 1;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        return driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    }

    void CheckSeparateErrors(bool persistFirst, uint8_t compErr, uint8_t persistErr)
    {
        TestDriver driver(ErrorBackendOptions());
        const auto request = StartPersistence(driver);
        SendResponse(driver, request, persistFirst ? CHI::Eb::Opcodes::RSP::Persist : CHI::Eb::Opcodes::RSP::Comp,
                     persistFirst ? persistErr : compErr);
        SendResponse(driver, request, persistFirst ? CHI::Eb::Opcodes::RSP::Comp : CHI::Eb::Opcodes::RSP::Persist,
                     persistFirst ? compErr : persistErr);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == compErr);
        const auto persist = driver.NextRSP(CHI::Eb::Opcodes::RSP::Persist);
        LUNATIC_CHECK(persist.RespErr() == persistErr);
        LUNATIC_CHECK(persist.PGroupID() == 0x68);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9CMOErrorsPropagateFromBackend)
{
    for (uint8_t respErr : { uint8_t(CHI::Eb::RespErrs::DERR), uint8_t(CHI::Eb::RespErrs::NDERR) })
    {
        TestDriver driver(ErrorBackendOptions());
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 4, 1, 0x69600)));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanShared);
        SendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp, respErr);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == respErr);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }
}

LUNATIC_TEST(Stage9PersistenceErrorsPreserveResponseLegs)
{
    CheckSeparateErrors(false, CHI::Eb::RespErrs::DERR, CHI::Eb::RespErrs::OK);
    CheckSeparateErrors(true, CHI::Eb::RespErrs::OK, CHI::Eb::RespErrs::DERR);

    TestDriver driver(ErrorBackendOptions());
    const auto request = StartPersistence(driver);
    SendResponse(driver, request, CHI::Eb::Opcodes::RSP::CompPersist, CHI::Eb::RespErrs::NDERR);
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
    LUNATIC_CHECK(response.PGroupID() == 0x68);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
