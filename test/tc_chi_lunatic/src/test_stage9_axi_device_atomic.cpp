#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using DAT        = TestDriver::DAT;

    void CheckAXIDeviceError(uint16_t opcode, uint64_t address)
    {
        Options options;
        options.backend = BackendKind::AXI;
        options.outputQueueLimit = 1;
        TestDriver driver(options);
        auto request = driver.Request(opcode, 4, 40, address);
        request.Size() = 0;
        request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
        request.SnpAttr() = 0;
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();

        DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.BE() = 1;
        data.Data()[0] = 7;
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();

        if (CHI::Eb::Opcodes::REQ::AtomicStore::Is(opcode))
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
        else
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::NDERR);
        LUNATIC_CHECK(!driver.node.PeekTXAR());
        LUNATIC_CHECK(!driver.node.PeekTXAW());
        LUNATIC_CHECK(driver.node.GetMemory().Read(LineKey::FromAddress(address, false)) == LineData{});
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage9AXIDeviceAtomicPathAndErrorContainment)
{
    Request request;
    request.opcode = CHI::Eb::Opcodes::REQ::AtomicSwap;
    request.memAttr = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    Options options;
    options.backend = BackendKind::AXI;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Local);
    request.memAttr = CHI::Eb::MemAttrs::Device_nEWA;
    LUNATIC_CHECK(ClassifyAtomicPath(request, options) == AtomicPath::Error);

    CheckAXIDeviceError(CHI::Eb::Opcodes::REQ::AtomicSwap, 0x6c000);
    CheckAXIDeviceError(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 0x6c100);
}
