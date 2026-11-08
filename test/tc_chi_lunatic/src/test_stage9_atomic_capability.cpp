#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using DAT        = TestDriver::DAT;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;

    Options AtomicOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        return options;
    }

    REQ AtomicRequest(TestDriver& driver, uint16_t opcode, uint64_t address, uint8_t memAttr)
    {
        auto request = driver.Request(opcode, 4, 40, address);
        request.Size() = 0;
        request.MemAttr() = memAttr;
        request.SnpAttr() = 0;
        request.AllowRetry() = 0;
        return request;
    }

    DataBufferId StartAtomic(TestDriver& driver, const REQ& request)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        return driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    }

    void SendOperand(TestDriver& driver, DataBufferId dbid, uint8_t value)
    {
        DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.BE() = 1;
        data.Data()[0] = value;
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
    }

    void SendBackendResponse(TestDriver& driver, const REQ& request, uint16_t opcode,
                             uint8_t respErr = CHI::Eb::RespErrs::OK)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.RespErr() = respErr;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SendBackendResult(TestDriver& driver, const REQ& request, uint8_t value)
    {
        DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = request.TxnID();
        data.DBID() = 77;
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_I;
        data.BE() = 1;
        data.Data()[0] = value;
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
    }

    void CompleteBackendWrite(TestDriver& driver, const REQ& request, uint8_t value)
    {
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::DBIDResp);
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(uint8_t(data.Data()[0]) == (beat == 0 ? value : 0));
        }
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
    }

    void CheckError(uint16_t opcode, Options options, uint64_t address)
    {
        TestDriver driver(options);
        const auto request = AtomicRequest(driver, opcode, address, CHI::Eb::MemAttrs::Device_nEWA);
        const DataBufferId dbid = StartAtomic(driver, request);
        SendOperand(driver, dbid, 7);
        if (CHI::Eb::Opcodes::REQ::AtomicStore::Is(opcode))
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
        else
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::NDERR);
        LUNATIC_CHECK(!driver.node.PeekTXREQ());
        LUNATIC_CHECK(driver.node.GetMemory().Read(LineKey::FromAddress(address, false)) == LineData{});
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        driver.Quiesce();
    }

    void CheckLocalNormal()
    {
        Options options = AtomicOptions();
        options.subordinateAtomicEnabled = false;
        TestDriver driver(options);
        const uint64_t address = 0x6b000;
        const auto request = AtomicRequest(driver, CHI::Eb::Opcodes::REQ::AtomicSwap, address,
                                           CHI::Eb::MemAttrs::NonCacheable_Bufferable);
        const DataBufferId dbid = StartAtomic(driver, request);
        SendOperand(driver, dbid, 7);

        const auto downstreamRead = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        SendBackendResult(driver, downstreamRead, 9);
        const auto downstreamWrite = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        CompleteBackendWrite(driver, downstreamWrite, 7);
        const auto original = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(uint8_t(original.Data()[0]) == 9);
        LUNATIC_CHECK(original.RespErr() == CHI::Eb::RespErrs::OK);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckCapableControl()
    {
        Options options = AtomicOptions();
        TestDriver driver(options);
        const uint64_t address = 0x6b100;
        const auto request = AtomicRequest(driver, CHI::Eb::Opcodes::REQ::AtomicSwap, address,
                                           CHI::Eb::MemAttrs::NonCacheable_Bufferable);
        const DataBufferId dbid = StartAtomic(driver, request);
        SendOperand(driver, dbid, 7);
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::AtomicSwap);
        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::DBIDResp);
        driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        SendBackendResult(driver, downstream, 9);
        const auto original = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(uint8_t(original.Data()[0]) == 9);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9AtomicCapabilityPathMatrix)
{
    Request request;
    request.opcode = CHI::Eb::Opcodes::REQ::AtomicSwap;
    request.source = 4;
    request.address = 0x1000;
    request.memAttr = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    Options options;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Local);
    options.backend = BackendKind::CHI;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Forward);

    options.homeAtomicEnabled = false;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Error);
    options.homeAtomicEnabled = true;
    options.atomicRequesters[4] = false;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Error);
    options.atomicRequesters.clear();

    options.atomicRegions = { { 0x1000, 0x2000 } };
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Forward);
    request.address = 0x2000;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Error);
    request.address = 0x1000;

    options.subordinateAtomicEnabled = false;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Local);
    request.memAttr = CHI::Eb::MemAttrs::Device_nEWA;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Error);
    options.subordinateAtomicEnabled = true;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Forward);

    options.atomicRegions = { { 0x2000, 0x2000 } };
    CheckThrows([&] { ValidateOptions(options); });
}

LUNATIC_TEST(Stage9AtomicCapabilityErrorContainment)
{
    Options device = AtomicOptions();
    device.subordinateAtomicEnabled = false;
    CheckError(CHI::Eb::Opcodes::REQ::AtomicSwap, device, 0x6b200);

    Options home = AtomicOptions();
    home.homeAtomicEnabled = false;
    CheckError(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, home, 0x6b300);

    Options requester = AtomicOptions();
    requester.atomicRequesters[4] = false;
    CheckError(CHI::Eb::Opcodes::REQ::AtomicSwap, requester, 0x6b400);

    Options region = AtomicOptions();
    region.subordinateAtomicEnabled = false;
    region.atomicRegions = { { 0x1000, 0x2000 } };
    CheckError(CHI::Eb::Opcodes::REQ::AtomicSwap, region, 0x6b500);
}

LUNATIC_TEST(Stage9AtomicLocalNormalExecution)
{
    CheckLocalNormal();
}

LUNATIC_TEST(Stage9AtomicCapableSubordinateControl)
{
    CheckCapableControl();
}
