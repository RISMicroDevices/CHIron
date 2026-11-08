#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    AXIOperation MakeOperation(const LineKey& line, uint8_t memAttr, bool write)
    {
        AXIOperation operation;
        operation.transaction = 0;
        operation.line = line;
        operation.memAttr = memAttr;
        operation.qos = 0xa;
        operation.write = write;
        operation.beatMask = 1;
        operation.geometry.rawOffset = 0;
        operation.geometry.alignedAddress = line.address;
        operation.geometry.transferBytes = LINE_SIZE;
        operation.geometry.validBytes = LINE_SIZE;
        operation.data.fill(0x5a);
        operation.enables.fill(1);
        return operation;
    }

    void CheckAddress(uint8_t memAttr, bool nonSecure, bool write)
    {
        Options options;
        options.backend = BackendKind::AXI;
        AXIBackend backend(options);
        const LineKey line = LineKey::FromAddress(0x8000, nonSecure);
        LUNATIC_CHECK(backend.Submit(MakeOperation(line, memAttr, write)));
        const auto address = write ? backend.PopAW() : backend.PopAR();
        LUNATIC_CHECK(address);
        LUNATIC_CHECK(address->address == line.address);
        LUNATIC_CHECK(address->len == (!write && CHI::Eb::MemAttrs::ExtractDevice(memAttr) ? 1 : 0));
        LUNATIC_CHECK(address->size == 5);
        LUNATIC_CHECK(address->burst == AXIBurst::INCR);
        LUNATIC_CHECK(address->cache == (write ? AXIWriteCache(memAttr) : AXIReadCache(memAttr)));
        LUNATIC_CHECK(address->qos == 0xa);
        LUNATIC_CHECK(address->prot == (nonSecure ? 0b010 : 0));
    }

}

LUNATIC_TEST(Stage9AXIReducedInterfaceLiteralProfile)
{
    LUNATIC_CHECK(AXIReadCache(CHI::Eb::MemAttrs::Device_nEWA) == 0b0000);
    LUNATIC_CHECK(AXIReadCache(CHI::Eb::MemAttrs::Device_EWA) == 0b0001);
    LUNATIC_CHECK(AXIReadCache(CHI::Eb::MemAttrs::NonCacheable_NonBufferable) == 0b0010);
    LUNATIC_CHECK(AXIReadCache(CHI::Eb::MemAttrs::NonCacheable_Bufferable) == 0b0011);
    LUNATIC_CHECK(AXIReadCache(CHI::Eb::MemAttrs::WriteBack_NoAllocate) == 0b1011);
    LUNATIC_CHECK(AXIReadCache(CHI::Eb::MemAttrs::WriteBack_Allocate) == 0b1111);

    LUNATIC_CHECK(AXIWriteCache(CHI::Eb::MemAttrs::Device_nEWA) == 0b0000);
    LUNATIC_CHECK(AXIWriteCache(CHI::Eb::MemAttrs::Device_EWA) == 0b0001);
    LUNATIC_CHECK(AXIWriteCache(CHI::Eb::MemAttrs::NonCacheable_NonBufferable) == 0b0010);
    LUNATIC_CHECK(AXIWriteCache(CHI::Eb::MemAttrs::NonCacheable_Bufferable) == 0b0011);
    LUNATIC_CHECK(AXIWriteCache(CHI::Eb::MemAttrs::WriteBack_NoAllocate) == 0b0111);
    LUNATIC_CHECK(AXIWriteCache(CHI::Eb::MemAttrs::WriteBack_Allocate) == 0b1111);

    CheckAddress(CHI::Eb::MemAttrs::WriteBack_Allocate, false, false);
    CheckAddress(CHI::Eb::MemAttrs::WriteBack_Allocate, true, false);
    CheckAddress(CHI::Eb::MemAttrs::WriteBack_Allocate, false, true);
    CheckAddress(CHI::Eb::MemAttrs::WriteBack_Allocate, true, true);
}

LUNATIC_TEST(Stage9AXIReducedInterfaceAttributeSignals)
{
    CheckAddress(CHI::Eb::MemAttrs::Device_EWA, true, false);
    CheckAddress(CHI::Eb::MemAttrs::Device_EWA, true, true);
    CheckAddress(CHI::Eb::MemAttrs::NonCacheable_Bufferable, false, false);
    CheckAddress(CHI::Eb::MemAttrs::NonCacheable_Bufferable, false, true);
}
