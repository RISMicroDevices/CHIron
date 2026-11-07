#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Driver<Config256>::REQ StartPersistence(Driver<Config256>& driver)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 50, 0x65000);
        request.PGroupID() = 0x68;
        request.Deep() = 1;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        LUNATIC_CHECK(downstream.ReturnNID() == 2);
        LUNATIC_CHECK(downstream.ReturnTxnID() == 0);
        LUNATIC_CHECK(downstream.PGroupID() == 0x68 && downstream.Deep() == 1);
        return downstream;
    }

    void SendPersistenceResponse(Driver<Config256>& driver, const Driver<Config256>::REQ& request,
        uint16_t opcode, uint8_t group = 0x68)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = opcode == CHI::Eb::Opcodes::RSP::Persist ? uint16_t(0) : uint16_t(request.TxnID());
        response.Opcode() = opcode;
        response.PGroupID() = group;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void CheckSeparateCompletion(Driver<Config256>& driver)
    {
        const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.TgtID() == 4 && comp.TxnID() == 50);
        const auto persist = driver.NextRSP(CHI::Eb::Opcodes::RSP::Persist);
        LUNATIC_CHECK(persist.TgtID() == 4 && persist.TxnID() == 0 && persist.PGroupID() == 0x68);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void RunSeparate(bool persistFirst)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        Driver<Config256> driver(options);
        const auto request = StartPersistence(driver);
        if (persistFirst)
            SendPersistenceResponse(driver, request, CHI::Eb::Opcodes::RSP::Persist);
        else
            SendPersistenceResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
        LUNATIC_CHECK(driver.node.GetErrors().empty());
        if (persistFirst)
            SendPersistenceResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
        else
            SendPersistenceResponse(driver, request, CHI::Eb::Opcodes::RSP::Persist);
        CheckSeparateCompletion(driver);
    }

}

LUNATIC_TEST(Stage9PersistenceAcceptsCompThenPersist)
{
    RunSeparate(false);
}

LUNATIC_TEST(Stage9PersistenceAcceptsPersistThenComp)
{
    RunSeparate(true);
}

LUNATIC_TEST(Stage9PersistenceCombinedResponseControl)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    const auto request = StartPersistence(driver);
    SendPersistenceResponse(driver, request, CHI::Eb::Opcodes::RSP::CompPersist);
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(response.TgtID() == 4 && response.TxnID() == 50 && response.PGroupID() == 0x68);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9PersistenceRejectsWrongSeparateGroup)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    const auto request = StartPersistence(driver);
    SendPersistenceResponse(driver, request, CHI::Eb::Opcodes::RSP::Persist, 0x69);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.HasInflight());
}
