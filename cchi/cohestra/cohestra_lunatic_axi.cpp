#include "cohestra_lunatic_axi.hpp"

namespace Cohestra {
namespace {

    template<typename Channel>
    Channel ConvertAddress(const ::CHI::Lunatic::AXIAddress& address)
    {
        Channel channel{};
        channel.Id = address.id;
        channel.Addr = address.address;
        channel.Len = address.len;
        channel.Size = address.size;
        channel.Burst = static_cast<AXI::Burst>(address.burst);
        return channel;
    }

    std::vector<uint64_t> PackData(const ::CHI::Lunatic::LineData& data, size_t bytes)
    {
        std::vector<uint64_t> lanes(bytes / 8, 0);
        for (size_t index = 0; index < bytes; ++index)
            lanes[index / 8] |= uint64_t(data[index]) << ((index % 8) * 8);
        return lanes;
    }

    std::vector<uint64_t> PackStrobes(const ::CHI::Lunatic::ByteEnables& strobes, size_t bytes)
    {
        std::vector<uint64_t> lanes((bytes + 63) / 64, 0);
        for (size_t index = 0; index < bytes; ++index)
        {
            if (strobes[index])
                lanes[index / 64] |= uint64_t(1) << (index % 64);
        }
        return lanes;
    }

    ::CHI::Lunatic::LineData UnpackData(const std::vector<uint64_t>& lanes, size_t bytes)
    {
        ::CHI::Lunatic::LineData data{};
        for (size_t index = 0; index < bytes; ++index)
            data[index] = AXI::DataByteAt(lanes, index);
        return data;
    }

}

    LunaticAXIAdapter::LunaticAXIAdapter(HomeNode& node, AXIMemory& memory) noexcept
        : node(&node), memory(&memory)
    {
    }

    void LunaticAXIAdapter::Tick() noexcept
    {
        const size_t bytes = memory->GetDataBytes();

        if (const auto response = memory->PeekB())
        {
            ::CHI::Lunatic::AXIWriteResponse converted;
            converted.id = response->Id;
            converted.resp = static_cast<::CHI::Lunatic::AXIResp>(response->Resp);
            if (node->PushRXB(converted))
                (void)memory->PopB();
        }

        if (const auto response = memory->PeekR())
        {
            ::CHI::Lunatic::AXIReadData converted;
            converted.id = response->Id;
            converted.data = UnpackData(response->Data, bytes);
            converted.resp = static_cast<::CHI::Lunatic::AXIResp>(response->Resp);
            converted.last = response->Last != 0;
            if (node->PushRXR(converted))
                (void)memory->PopR();
        }

        if (const auto address = node->PeekTXAW())
        {
            if (memory->PushAW(ConvertAddress<AXI::BundleChannelAW>(*address)))
                (void)node->PopTXAW();
        }

        if (const auto beat = node->PeekTXW())
        {
            AXI::BundleChannelW converted{};
            converted.Data = PackData(beat->data, bytes);
            converted.Strb = PackStrobes(beat->strobes, bytes);
            converted.Last = beat->last;
            if (memory->PushW(converted))
                (void)node->PopTXW();
        }

        if (const auto address = node->PeekTXAR())
        {
            if (memory->PushAR(ConvertAddress<AXI::BundleChannelAR>(*address)))
                (void)node->PopTXAR();
        }
    }

    bool LunaticAXIAdapter::Validate() const noexcept
    {
        return memory->GetProtocolDenialCount() == 0;
    }

}
