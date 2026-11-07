#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;
    using DAT        = TestDriver::DAT;

    ByteEnables FullMemAttrEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void CheckRequestAttributes(const REQ& request, uint8_t memAttr)
    {
        LUNATIC_CHECK(uint8_t(request.MemAttr()) == memAttr);
        LUNATIC_CHECK(request.SnpAttr() == 0);
    }

    void SendBackendResponse(TestDriver& driver, const REQ& request, uint16_t opcode)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.Opcode() = opcode;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void CompleteBackendRead(TestDriver& driver, const REQ& request)
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
            data.DataID() = beat * 2;
            data.CCID() = (request.Addr() >> 4) & 3;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        for (size_t beat = 0; beat < 2; ++beat)
            driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CompleteBackendWrite(TestDriver& driver, const REQ& request)
    {
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::DBIDResp);
        for (size_t beat = 0; beat < 2; ++beat)
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData).TxnID() == 77);
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
    }

    void StartCopyback(TestDriver& driver, uint64_t address, TransactionId txnId,
                       uint8_t memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, txnId, address);
        request.MemAttr() = memAttr;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, txnId, response.DBID(), LineData{}, FullMemAttrEnables(),
                         CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    }

    void CheckPropagatedRead()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x68000);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        request.SnpAttr() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        CheckRequestAttributes(downstream, CHI::Eb::MemAttrs::WriteBack_NoAllocate);
        CompleteBackendRead(driver, downstream);
    }

    void CheckPropagatedAtomic()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 1, 0x68100);
        request.Size() = 0;
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });
        bool compReceived = driver.node.PeekTXRSP()->Opcode() == CHI::Eb::Opcodes::RSP::Comp;
        if (compReceived)
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        DAT operand{};
        operand.SrcID() = 4;
        operand.TgtID() = 2;
        operand.TxnID() = dbid;
        operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        operand.DataID() = 0;
        operand.CCID() = 0;
        operand.Data()[0] = 7;
        operand.BE() = 1;
        LUNATIC_CHECK(driver.node.PushRXDAT(operand));
        driver.Tick();

        const auto downstream = driver.NextREQ(request.Opcode());
        CheckRequestAttributes(downstream, CHI::Eb::MemAttrs::WriteBack_NoAllocate);
        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData).TxnID() == 77);
        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp);
        if (!compReceived)
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckPropagatedCMO()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 4, 1, 0x68200);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanShared);
        CheckRequestAttributes(downstream, CHI::Eb::MemAttrs::WriteBack_NoAllocate);
        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckOriginalWrite()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 1, 0x68300);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        driver.WriteData(4, 1, response.DBID(), LineData{}, FullMemAttrEnables(),
                         CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        CheckRequestAttributes(downstream, CHI::Eb::MemAttrs::WriteBack_NoAllocate);
        CompleteBackendWrite(driver, downstream);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckSameLineSpill()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniquePtl, 4, 1, 0x68400);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        driver.WriteData(4, 1, response.DBID(), LineData{}, FullMemAttrEnables(),
                         CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl);
        CheckRequestAttributes(downstream, CHI::Eb::MemAttrs::WriteBack_NoAllocate);
        CompleteBackendWrite(driver, downstream);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void RunAXIWrite(uint16_t opcode, uint8_t memAttr, uint8_t expectedCache)
    {
        Options options;
        options.backend = BackendKind::AXI;
        options.policy = PolicyKind::Inclusive;
        TestDriver driver(options);
        auto request = driver.Request(opcode, 4, 1, 0x68500);
        request.MemAttr() = memAttr;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(IsCopybackRequest(opcode)
            ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::DBIDResp);
        driver.WriteData(4, 1, response.DBID(), LineData{}, FullMemAttrEnables(),
            IsCopybackRequest(opcode) ? CHI::Eb::Opcodes::DAT::CopyBackWrData
                                      : CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
            IsCopybackRequest(opcode) ? uint8_t(CHI::Eb::Resps::CopyBackWrData_UD_PD) : uint8_t(0));
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXAW()); });
        const auto address = driver.node.PeekTXAW();
        LUNATIC_CHECK(address->cache == expectedCache);
        LUNATIC_CHECK(driver.node.PopTXAW());
        for (size_t beat = 0; beat < 2; ++beat)
        {
            driver.Until([&] { return static_cast<bool>(driver.node.PeekTXW()); });
            LUNATIC_CHECK(driver.node.PopTXW());
            driver.Tick();
        }
        LUNATIC_CHECK(driver.node.PushRXB(AXIWriteResponse{ address->id, AXIResp::OKAY }));
        driver.Tick();
        if (!IsCopybackRequest(opcode))
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9MemAttrPropagatesCHIRequests)
{
    CheckPropagatedRead();
    CheckPropagatedAtomic();
    CheckPropagatedCMO();
}

LUNATIC_TEST(Stage9MemAttrDistinguishesCHIWrites)
{
    CheckOriginalWrite();
    CheckSameLineSpill();
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    TestDriver driver(options);
    StartCopyback(driver, 0x68600, 1);
    driver.Quiesce();
    StartCopyback(driver, 0x68640, 2);
    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(downstream.Addr() == 0x68600);
    CheckRequestAttributes(downstream, CHI::Eb::MemAttrs::WriteBack_Allocate);
    CompleteBackendWrite(driver, downstream);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9MemAttrDistinguishesAXIWrites)
{
    RunAXIWrite(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, CHI::Eb::MemAttrs::WriteBack_NoAllocate,
                AXIWriteCache(CHI::Eb::MemAttrs::WriteBack_NoAllocate));
    RunAXIWrite(CHI::Eb::Opcodes::REQ::WriteUniquePtl, CHI::Eb::MemAttrs::WriteBack_NoAllocate,
                AXIWriteCache(CHI::Eb::MemAttrs::WriteBack_NoAllocate));

    Options options;
    options.backend = BackendKind::AXI;
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    TestDriver driver(options);
    StartCopyback(driver, 0x68600, 1);
    driver.Quiesce();
    StartCopyback(driver, 0x68640, 2);
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXAW()); });
    const auto address = driver.node.PeekTXAW();
    LUNATIC_CHECK(address->address == 0x68600);
    LUNATIC_CHECK(address->cache == AXIWriteCache(CHI::Eb::MemAttrs::WriteBack_Allocate));
    LUNATIC_CHECK(driver.node.PopTXAW());
    for (size_t beat = 0; beat < 2; ++beat)
    {
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXW()); });
        LUNATIC_CHECK(driver.node.PopTXW());
        driver.Tick();
    }
    LUNATIC_CHECK(driver.node.PushRXB(AXIWriteResponse{ address->id, AXIResp::OKAY }));
    driver.Tick();
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
