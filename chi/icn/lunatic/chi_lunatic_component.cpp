#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {

    Core::Core(NodeId nodeId, Options options, std::shared_ptr<EventHub> events)
    {
        ValidateOptions(options);
        if (!events)
            events = std::make_shared<EventHub>();
        impl = std::make_unique<Impl>(nodeId, std::move(options), std::move(events));
    }

    Core::~Core() = default;

    void Core::Reset()
    {
        if (!impl->Reentry())
            impl->Reset();
    }

    void Core::Tick(Time time)
    {
        if (impl->Reentry())
            return;
        if (time < impl->time)
        {
            impl->Error(INVALID_TRANSACTION, "Lunatic virtual time must not decrease");
            return;
        }
        impl->time = time;
        impl->Tick();
    }

    bool Core::Drain()
    {
        if (impl->Reentry())
            return false;
        impl->Tick();
        return !HasInflight();
    }

    bool Core::HasInflight() const noexcept { return impl->HasInflight(); }

    bool Core::PushRXREQ(const Request& flit)
    {
        return !impl->Reentry() && impl->Push(impl->rxreq, flit, EventKind::AcceptedRXREQ);
    }

    bool Core::PushRXRSP(const Response& flit)
    {
        return !impl->Reentry() && impl->Push(impl->rxrsp, flit, EventKind::AcceptedRXRSP);
    }

    bool Core::PushRXDAT(const Data& flit)
    {
        return !impl->Reentry() && impl->Push(impl->rxdat, flit, EventKind::AcceptedRXDAT);
    }

    bool Core::PushRXB(const AXIWriteResponse& response)
    {
        return !impl->Reentry() && impl->axi.PushB(response);
    }

    bool Core::PushRXR(const AXIReadData& response)
    {
        return !impl->Reentry() && impl->axi.PushR(response);
    }

    std::optional<AXIAddress> Core::PeekTXAW() const
    {
        return impl->Reentry() ? std::nullopt : impl->axi.PeekAW();
    }

    std::optional<AXIWriteData> Core::PeekTXW() const
    {
        return impl->Reentry() ? std::nullopt : impl->axi.PeekW();
    }

    std::optional<AXIAddress> Core::PeekTXAR() const
    {
        return impl->Reentry() ? std::nullopt : impl->axi.PeekAR();
    }

    std::optional<AXIAddress> Core::PopTXAW()
    {
        return impl->Reentry() ? std::nullopt : impl->axi.PopAW();
    }

    std::optional<AXIWriteData> Core::PopTXW()
    {
        return impl->Reentry() ? std::nullopt : impl->axi.PopW();
    }

    std::optional<AXIAddress> Core::PopTXAR()
    {
        return impl->Reentry() ? std::nullopt : impl->axi.PopAR();
    }

    std::optional<Request> Core::PeekTXREQ() const
    {
        if (impl->Reentry())
            return std::nullopt;
        const auto* value = impl->Peek(impl->txreq);
        return value ? std::optional<Request>(*value) : std::nullopt;
    }

    std::optional<Response> Core::PeekTXRSP() const
    {
        if (impl->Reentry())
            return std::nullopt;
        const auto* value = impl->Peek(impl->txrsp);
        return value ? std::optional<Response>(*value) : std::nullopt;
    }

    std::optional<Data> Core::PeekTXDAT() const
    {
        if (impl->Reentry())
            return std::nullopt;
        const auto* value = impl->Peek(impl->txdat);
        return value ? std::optional<Data>(*value) : std::nullopt;
    }

    std::optional<Snoop> Core::PeekTXSNP() const
    {
        if (impl->Reentry())
            return std::nullopt;
        const auto* value = impl->Peek(impl->txsnp);
        return value ? std::optional<Snoop>(*value) : std::nullopt;
    }

    std::optional<Request> Core::PopTXREQ()
    {
        return impl->Reentry() ? std::nullopt : impl->Pop(impl->txreq, EventKind::EmittedTXREQ);
    }

    std::optional<Response> Core::PopTXRSP()
    {
        return impl->Reentry() ? std::nullopt : impl->Pop(impl->txrsp, EventKind::EmittedTXRSP);
    }

    std::optional<Data> Core::PopTXDAT()
    {
        return impl->Reentry() ? std::nullopt : impl->Pop(impl->txdat, EventKind::EmittedTXDAT);
    }

    std::optional<Snoop> Core::PopTXSNP()
    {
        return impl->Reentry() ? std::nullopt : impl->Pop(impl->txsnp, EventKind::EmittedTXSNP);
    }

    NodeId Core::GetNodeId() const noexcept { return impl->nodeId; }
    Time Core::GetTime() const noexcept { return impl->time; }
    const Options& Core::GetOptions() const noexcept { return impl->options; }
    std::shared_ptr<EventHub> Core::GetEvents() const noexcept { return impl->events; }
    const std::vector<ErrorInfo>& Core::GetErrors() const noexcept { return impl->errors; }
    SparseMemory& Core::GetMemory() noexcept { return impl->memory; }
    const SparseMemory& Core::GetMemory() const noexcept { return impl->memory; }

    HomeLineInfo Core::InspectHome(const LineKey& line) const noexcept
    {
        HomeLineInfo result;
        if (const HomeLine* home = impl->directory.Find(line))
        {
            result.valid = home->valid;
            result.dirty = home->dirty;
            result.data = home->data;
            result.validBytes = home->validBytes;
            CopyMetadata(result, *home, impl->options);
        }
        return result;
    }

    CacheState Core::InspectRequester(const LineKey& line, NodeId requester) const noexcept
    {
        return impl->snoopFilter.State(line, requester);
    }

    bool Core::ValidateInvariants() const noexcept { return impl->ValidateInvariants(); }

}
