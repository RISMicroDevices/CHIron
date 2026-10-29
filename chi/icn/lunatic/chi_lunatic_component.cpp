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

    void                                Core::Reset() { impl->Reset(); }

    void                                Core::Tick(Time time)
    {
        impl->time = time;
        impl->Tick();
    }

    bool                                Core::Drain()
    {
        impl->Tick();
        impl->events->Dispatch();
        return !HasInflight() && impl->events->Pending() == 0;
    }

    bool                                Core::DispatchEvents() { return impl->events->Dispatch(); }
    bool                                Core::HasInflight() const noexcept { return impl->HasInflight(); }

    bool                                Core::PushRXREQ(const Request& flit) { return impl->Push(impl->rxreq, flit, EventKind::AcceptedRXREQ); }
    bool                                Core::PushRXRSP(const Response& flit) { return impl->Push(impl->rxrsp, flit, EventKind::AcceptedRXRSP); }
    bool                                Core::PushRXDAT(const Data& flit) { return impl->Push(impl->rxdat, flit, EventKind::AcceptedRXDAT); }

    std::optional<Request>              Core::PeekTXREQ() const
    {
        const auto* value = impl->Peek(impl->txreq);
        return value ? std::optional<Request>(*value) : std::nullopt;
    }

    std::optional<Response>             Core::PeekTXRSP() const
    {
        const auto* value = impl->Peek(impl->txrsp);
        return value ? std::optional<Response>(*value) : std::nullopt;
    }

    std::optional<Data>                 Core::PeekTXDAT() const
    {
        const auto* value = impl->Peek(impl->txdat);
        return value ? std::optional<Data>(*value) : std::nullopt;
    }

    std::optional<Snoop>                Core::PeekTXSNP() const
    {
        const auto* value = impl->Peek(impl->txsnp);
        return value ? std::optional<Snoop>(*value) : std::nullopt;
    }

    std::optional<Request>              Core::PopTXREQ() { return impl->Pop(impl->txreq, EventKind::EmittedTXREQ); }
    std::optional<Response>             Core::PopTXRSP() { return impl->Pop(impl->txrsp, EventKind::EmittedTXRSP); }
    std::optional<Data>                 Core::PopTXDAT() { return impl->Pop(impl->txdat, EventKind::EmittedTXDAT); }
    std::optional<Snoop>                Core::PopTXSNP() { return impl->Pop(impl->txsnp, EventKind::EmittedTXSNP); }

    NodeId                              Core::GetNodeId() const noexcept { return impl->nodeId; }
    Time                                Core::GetTime() const noexcept { return impl->time; }
    const Options&                      Core::GetOptions() const noexcept { return impl->options; }
    std::shared_ptr<EventHub>           Core::GetEvents() const noexcept { return impl->events; }
    const std::vector<ErrorInfo>&       Core::GetErrors() const noexcept { return impl->errors; }
    SparseMemory&                       Core::GetMemory() noexcept { return impl->memory; }
    const SparseMemory&                 Core::GetMemory() const noexcept { return impl->memory; }

    HomeLineInfo                        Core::InspectHome(const LineKey& line) const noexcept
    {
        HomeLineInfo result;
        if (const HomeLine* home = impl->directory.Find(line))
        {
            result.valid = home->valid;
            result.dirty = home->dirty;
            result.data = home->data;
        }
        return result;
    }

    CacheState                          Core::InspectRequester(const LineKey& line, NodeId requester) const noexcept
    {
        return impl->snoopFilter.State(line, requester);
    }

    bool                                Core::ValidateInvariants() const noexcept { return impl->ValidateInvariants(); }

}
