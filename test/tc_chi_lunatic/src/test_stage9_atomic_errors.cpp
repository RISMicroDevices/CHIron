#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;
    using DAT        = TestDriver::DAT;

    struct AtomicFlow {
        REQ          downstream;
        DataBufferId upstreamDBID;
    };

    Options AtomicOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        return options;
    }

    AtomicFlow BeginAtomic(TestDriver& driver, uint16_t opcode, TransactionId txnId,
                           uint64_t address, uint8_t operandErr)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.Size() = 0;
        request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
        request.SnpAttr() = 0;
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();

        DAT operand{};
        operand.SrcID() = 4;
        operand.TgtID() = 2;
        operand.TxnID() = dbid;
        operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        operand.RespErr() = operandErr;
        operand.DataID() = (address % LINE_SIZE) / 16;
        operand.CCID() = (address >> 4) & 3;
        operand.BE() = 1;
        operand.Data()[0] = 7;
        LUNATIC_CHECK(driver.node.PushRXDAT(operand));
        driver.Tick();
        return { driver.NextREQ(opcode), dbid };
    }

    DAT ReceiveBackendOperand(TestDriver& driver, const REQ& downstream)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = downstream.TxnID();
        response.DBID() = 77;
        response.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
        return driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    }

    void SendBackendComp(TestDriver& driver, const REQ& downstream, uint8_t respErr)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = downstream.TxnID();
        response.DBID() = 77;
        response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.RespErr() = respErr;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SendBackendResult(TestDriver& driver, const REQ& downstream, uint8_t respErr)
    {
        DAT result{};
        result.SrcID() = 6;
        result.TgtID() = 2;
        result.HomeNID() = 2;
        result.TxnID() = downstream.TxnID();
        result.DBID() = 77;
        result.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        result.Resp() = CHI::Eb::Resps::CompData_I;
        result.RespErr() = respErr;
        result.DataID() = (downstream.Addr() % LINE_SIZE) / 16;
        result.CCID() = (downstream.Addr() >> 4) & 3;
        result.BE() = 1;
        result.Data()[0] = 9;
        LUNATIC_CHECK(driver.node.PushRXDAT(result));
        driver.Tick();
    }

    void CheckSwapBackendError(uint8_t respErr, uint64_t address)
    {
        TestDriver driver(AtomicOptions());
        const auto flow = BeginAtomic(driver, CHI::Eb::Opcodes::REQ::AtomicSwap, 2, address,
                                      CHI::Eb::RespErrs::OK);
        LUNATIC_CHECK(ReceiveBackendOperand(driver, flow.downstream).RespErr() == CHI::Eb::RespErrs::OK);
        SendBackendResult(driver, flow.downstream, respErr);
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == respErr);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9AtomicErrorStoreBackendCompletion)
{
    TestDriver driver(AtomicOptions());
    const auto flow = BeginAtomic(driver, CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 1, 0x6a000,
                                  CHI::Eb::RespErrs::OK);
    LUNATIC_CHECK(ReceiveBackendOperand(driver, flow.downstream).RespErr() == CHI::Eb::RespErrs::OK);
    SendBackendComp(driver, flow.downstream, CHI::Eb::RespErrs::NDERR);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9AtomicErrorSwapBackendData)
{
    CheckSwapBackendError(CHI::Eb::RespErrs::DERR, 0x6a100);
    CheckSwapBackendError(CHI::Eb::RespErrs::NDERR, 0x6a200);
}

LUNATIC_TEST(Stage9AtomicErrorStoreOperand)
{
    TestDriver driver(AtomicOptions());
    const auto flow = BeginAtomic(driver, CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 3, 0x6a300,
                                  CHI::Eb::RespErrs::DERR);
    LUNATIC_CHECK(ReceiveBackendOperand(driver, flow.downstream).RespErr() == CHI::Eb::RespErrs::DERR);
    SendBackendComp(driver, flow.downstream, CHI::Eb::RespErrs::OK);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::DERR);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9AtomicErrorSwapOperand)
{
    TestDriver driver(AtomicOptions());
    const auto flow = BeginAtomic(driver, CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 0x6a400,
                                  CHI::Eb::RespErrs::DERR);
    LUNATIC_CHECK(ReceiveBackendOperand(driver, flow.downstream).RespErr() == CHI::Eb::RespErrs::DERR);
    SendBackendResult(driver, flow.downstream, CHI::Eb::RespErrs::OK);
    LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::DERR);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

namespace {

    void CheckEarlyStore(BackendKind backend, uint64_t address)
    {
        Options options = AtomicOptions();
        options.backend = backend;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 5, address);
        request.Size() = 0;
        request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        LUNATIC_CHECK(!driver.node.PeekTXRSP());

        DAT operand{};
        operand.SrcID() = 4;
        operand.TgtID() = 2;
        operand.TxnID() = dbid;
        operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        operand.RespErr() = CHI::Eb::RespErrs::DERR;
        operand.DataID() = (address % LINE_SIZE) / 16;
        operand.CCID() = (address >> 4) & 3;
        operand.BE() = 1;
        operand.Data()[0] = 7;
        LUNATIC_CHECK(driver.node.PushRXDAT(operand));
        driver.Tick();

        if (backend == BackendKind::CHI)
        {
            const auto downstream = driver.NextREQ(request.Opcode());
            LUNATIC_CHECK(ReceiveBackendOperand(driver, downstream).RespErr() == CHI::Eb::RespErrs::DERR);
            SendBackendComp(driver, downstream, CHI::Eb::RespErrs::OK);
        }
        else
            LUNATIC_CHECK(!driver.node.PeekTXREQ());
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::DERR);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9AtomicErrorEarlyStoreCHI)
{
    CheckEarlyStore(BackendKind::CHI, 0x6a500);
}

LUNATIC_TEST(Stage9AtomicErrorEarlyStoreInternal)
{
    CheckEarlyStore(BackendKind::Internal, 0x6a600);
}
