#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void RunBackendWriteTrace(bool combined)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        SNOracle<Config256> oracle(2, 6);
        const LineData expected{};
        ByteEnables enables{};
        enables.fill(1);

        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 41, 0x5d000);
        request.TraceTag() = 0;
        driver.PushRequest(request);
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(dbid.TraceTag() == 0);
        driver.WriteData(4, 41, dbid.DBID(), expected, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        oracle.Request(driver.driver.now, downstream);

        CheckedDriver<Config256>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = downstream.TxnID();
        response.DBID() = 77;
        response.Opcode() = combined ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::DBIDResp;
        response.TraceTag() = 1;
        oracle.Response(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(data.TraceTag() == 1);
            oracle.WriteData(driver.driver.now, data);
        }

        if (!combined)
        {
            response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
            response.TraceTag() = 0;
            oracle.Response(driver.driver.now, response);
            LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
            driver.driver.Tick();
        }
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TraceTag() == 0);
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

}

LUNATIC_TEST(Stage9BackendWriteTraceTagDBIDResp)
{
    RunBackendWriteTrace(false);
}

LUNATIC_TEST(Stage9BackendWriteTraceTagCompDBIDResp)
{
    RunBackendWriteTrace(true);
}

LUNATIC_TEST(Stage9BackendAtomicTraceTagDBIDResp)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 1, 0x5d100);
    request.Size() = 0;
    request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_NonBufferable;
    request.TraceTag() = 0;
    driver.PushRequest(request);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(dbid.TraceTag() == 0);

    CheckedDriver<Config256>::DAT operand{};
    operand.SrcID() = 4;
    operand.TgtID() = 2;
    operand.TxnID() = dbid.DBID();
    operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
    operand.DataID() = 0;
    operand.CCID() = 0;
    operand.Data()[0] = 7;
    operand.BE() = 1;
    driver.Oracle(4).WriteData(driver.driver.now, operand);
    LUNATIC_CHECK(driver.driver.node.PushRXDAT(operand));
    driver.driver.Tick();

    const auto downstream = driver.driver.NextREQ(request.Opcode());
    oracle.Request(driver.driver.now, downstream);
    CheckedDriver<Config256>::RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = downstream.TxnID();
    response.DBID() = 77;
    response.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
    response.TraceTag() = 1;
    oracle.Response(driver.driver.now, response);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
    driver.driver.Tick();
    const auto data = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(data.TraceTag() == 1);
    oracle.WriteData(driver.driver.now, data);

    response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    response.TraceTag() = 0;
    oracle.Response(driver.driver.now, response);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
    driver.driver.Tick();
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TraceTag() == 0);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}
