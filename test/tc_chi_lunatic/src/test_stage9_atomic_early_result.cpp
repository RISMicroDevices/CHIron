#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    enum class ResultPoint {
        BeforeDBID,
        BeforeData,
        AfterData
    };

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

    void SendResult(Driver<Config256>& driver, const Driver<Config256>::REQ& request, DataBufferId dbid)
    {
        Driver<Config256>::DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = request.TxnID();
        data.DBID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_I;
        data.DataID() = 0;
        data.CCID() = (request.Addr() >> 4) & 3;
        data.Data()[0] = 1;
        data.BE() = 1;
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
    }

    void SendDBID(Driver<Config256>& driver, const Driver<Config256>::REQ& request, DataBufferId dbid)
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

    Driver<Config256>::REQ AtomicRequest(Driver<Config256>& driver)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicLoad::ADD, 4, 1, 0x64000);
        request.Size() = 0;
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        SendOperand(driver, dbid);
        return driver.NextREQ(request.Opcode());
    }

    void CheckUpstreamResult(Driver<Config256>& driver, DataBufferId dbid)
    {
        const auto result = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(result.TgtID() == 4);
        LUNATIC_CHECK(result.TxnID() == 1);
        LUNATIC_CHECK(result.DBID() == dbid);
        LUNATIC_CHECK(result.DataID() == 0);
        LUNATIC_CHECK(uint8_t(result.Data()[0]) == 1);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void RunResultPoint(ResultPoint point)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        Driver<Config256> driver(options);
        const auto request = AtomicRequest(driver);
        if (point == ResultPoint::BeforeDBID)
        {
            SendResult(driver, request, 77);
            LUNATIC_CHECK(driver.node.GetErrors().empty());
        }
        SendDBID(driver, request, 77);
        if (point == ResultPoint::BeforeData)
        {
            SendResult(driver, request, 77);
            LUNATIC_CHECK(driver.node.GetErrors().empty());
        }
        const auto operand = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(operand.TgtID() == 6 && operand.TxnID() == 77);
        LUNATIC_CHECK(uint8_t(operand.Data()[0]) == 2);
        if (point == ResultPoint::AfterData)
            SendResult(driver, request, 77);
        CheckUpstreamResult(driver, 0);
    }

}

LUNATIC_TEST(Stage9CHIBackendAcceptsAtomicResultBeforeDBID)
{
    RunResultPoint(ResultPoint::BeforeDBID);
}

LUNATIC_TEST(Stage9CHIBackendAcceptsAtomicResultBeforeData)
{
    RunResultPoint(ResultPoint::BeforeData);
}

LUNATIC_TEST(Stage9CHIBackendAcceptsAtomicResultAfterData)
{
    RunResultPoint(ResultPoint::AfterData);
}

LUNATIC_TEST(Stage9CHIBackendRejectsAtomicResultDBIDMismatch)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    const auto request = AtomicRequest(driver);
    SendResult(driver, request, 78);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    SendDBID(driver, request, 77);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.HasInflight());
}
