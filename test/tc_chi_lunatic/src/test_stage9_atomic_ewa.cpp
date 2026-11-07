#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void SendOperand(Driver<Config256>& driver, DataBufferId dbid)
    {
        Driver<Config256>::DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = 0;
        data.CCID() = 0;
        data.Data()[0] = 2;
        data.BE() = 1;
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
    }

    Driver<Config256>::REQ StartAtomic(Driver<Config256>& driver, bool earlyWriteAck)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 1, 0x66000);
        request.Size() = 0;
        request.MemAttr() = earlyWriteAck ? CHI::Eb::MemAttrs::WriteBack_NoAllocate
            : CHI::Eb::MemAttrs::NonCacheable_NonBufferable;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        SendOperand(driver, dbid);
        return driver.NextREQ(request.Opcode());
    }

    void SendComp(Driver<Config256>& driver, const Driver<Config256>::REQ& request, NodeId source)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = source;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SendDBIDResp(Driver<Config256>& driver, const Driver<Config256>::REQ& request, DataBufferId dbid)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = dbid;
        response.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

}

LUNATIC_TEST(Stage9AtomicStoreEWAZeroWaitsForEndpoint)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    const auto request = StartAtomic(driver, false);
    LUNATIC_CHECK(request.MemAttr() == CHI::Eb::MemAttrs::NonCacheable_NonBufferable);
    LUNATIC_CHECK(CHI::Eb::MemAttrs::ExtractEWA(request.MemAttr()) == 0);
    LUNATIC_CHECK(!driver.node.PeekTXRSP());

    SendComp(driver, request, 6);
    SendDBIDResp(driver, request, 0);
    const auto operand = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(operand.TgtID() == 6 && operand.TxnID() == 0);
    LUNATIC_CHECK(uint8_t(operand.Data()[0]) == 2);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9AtomicStoreRejectsIntermediateComp)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    const auto request = StartAtomic(driver, false);
    SendComp(driver, request, 2);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.HasInflight());
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
}
