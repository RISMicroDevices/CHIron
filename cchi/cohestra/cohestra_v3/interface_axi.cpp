#include "interface_axi.hpp"

#include "spdlog/spdlog.h"


// Implementation of: AXIMonitor listener registration
namespace Cohestra {

    void RegisterAXIMonitorListeners(
        std::shared_ptr<Cohestra::AXIMonitor> monitor, size_t index)
    {
        monitor->events->OnWriteAccept.Register(
            Gravity::MakeListener<Cohestra::AXIWriteAcceptEvent>("interface_axi", 0, [index](Cohestra::AXIWriteAcceptEvent& event) {
                const auto& channel = event.GetChannel();
                spdlog::debug("AXI monitor [{}]: [write accept] id: {:#x}, addr: {:#x}, size: {:#x}, burst: {}, len: {:#x} at time {}.",
                    index, uint64_t(channel.Bundle.Id), channel.Bundle.Addr, uint64_t(channel.Bundle.Size),
                    Cohestra::AXI::ToString(channel.Bundle.Burst), uint64_t(channel.Bundle.Len), event.GetTime());
            }));

        monitor->events->OnWriteData.Register(
            Gravity::MakeListener<Cohestra::AXIWriteDataEvent>("interface_axi", 0, [index](Cohestra::AXIWriteDataEvent& event) {
                spdlog::debug("AXI monitor [{}]: [write data] last: {} at time {}.",
                    index, uint64_t(event.GetChannel().Bundle.Last), event.GetTime());
            }));

        monitor->events->OnWriteResponse.Register(
            Gravity::MakeListener<Cohestra::AXIWriteResponseEvent>("interface_axi", 0, [index](Cohestra::AXIWriteResponseEvent& event) {
                const auto& channel = event.GetChannel();
                spdlog::debug("AXI monitor [{}]: [write response] id: {:#x}, resp: {} at time {}.",
                    index, uint64_t(channel.Bundle.Id), Cohestra::AXI::ToString(channel.Bundle.Resp), event.GetTime());
            }));

        monitor->events->OnReadAccept.Register(
            Gravity::MakeListener<Cohestra::AXIReadAcceptEvent>("interface_axi", 0, [index](Cohestra::AXIReadAcceptEvent& event) {
                const auto& channel = event.GetChannel();
                spdlog::debug("AXI monitor [{}]: [read accept] id: {:#x}, addr: {:#x}, size: {:#x}, burst: {}, len: {:#x} at time {}.",
                    index, uint64_t(channel.Bundle.Id), channel.Bundle.Addr, uint64_t(channel.Bundle.Size),
                    Cohestra::AXI::ToString(channel.Bundle.Burst), uint64_t(channel.Bundle.Len), event.GetTime());
            }));

        monitor->events->OnReadData.Register(
            Gravity::MakeListener<Cohestra::AXIReadDataEvent>("interface_axi", 0, [index](Cohestra::AXIReadDataEvent& event) {
                const auto& channel = event.GetChannel();
                spdlog::debug("AXI monitor [{}]: [read data] id: {:#x}, resp: {}, last: {} at time {}.",
                    index, uint64_t(channel.Bundle.Id), Cohestra::AXI::ToString(channel.Bundle.Resp),
                    uint64_t(channel.Bundle.Last), event.GetTime());
            }));

        monitor->events->OnReadResponse.Register(
            Gravity::MakeListener<Cohestra::AXIReadResponseEvent>("interface_axi", 0, [index](Cohestra::AXIReadResponseEvent& event) {
                spdlog::debug("AXI monitor [{}]: [read response] id: {:#x} at time {}.",
                    index, uint64_t(event.GetChannel().Bundle.Id), event.GetTime());
            }));

        monitor->events->OnWriteComplete.Register(
            Gravity::MakeListener<Cohestra::AXIWriteCompleteEvent>("interface_axi", 0, [index](Cohestra::AXIWriteCompleteEvent& event) {
                const auto& transaction = event.GetTransaction();
                spdlog::debug("AXI monitor [{}]: [write complete] id: {:#x}, addr: {:#x}, data beats: {}, resp: {} at time {}.",
                    index, uint64_t(transaction.request.Bundle.Id), transaction.request.Bundle.Addr,
                    transaction.data.size(), Cohestra::AXI::ToString(transaction.response.Bundle.Resp), event.GetTime());
            }));

        monitor->events->OnReadComplete.Register(
            Gravity::MakeListener<Cohestra::AXIReadCompleteEvent>("interface_axi", 0, [index](Cohestra::AXIReadCompleteEvent& event) {
                const auto& transaction = event.GetTransaction();
                spdlog::debug("AXI monitor [{}]: [read complete] id: {:#x}, addr: {:#x}, data beats: {} at time {}.",
                    index, uint64_t(transaction.request.Bundle.Id), transaction.request.Bundle.Addr,
                    transaction.data.size(), event.GetTime());
            }));
    }
}
