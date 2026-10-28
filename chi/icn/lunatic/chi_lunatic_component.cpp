#include "chi_lunatic_component.hpp"

#include <deque>
#include <utility>

namespace CHI::Lunatic {

struct Core::Impl {
    NodeId nodeId;
    Options options;
    std::shared_ptr<EventHub> events;
    Time time = 0;
    std::deque<Request> rxreq;
    std::deque<Response> rxrsp;
    std::deque<Data> rxdat;
    std::deque<Request> txreq;
    std::deque<Response> txrsp;
    std::deque<Data> txdat;
    std::deque<Snoop> txsnp;
    std::vector<ErrorInfo> errors;

    Impl(NodeId nodeId, Options options, std::shared_ptr<EventHub> events)
        : nodeId(nodeId), options(std::move(options)), events(std::move(events))
    {
    }

    template<typename T>
    bool Push(std::deque<T>& queue, const T& flit, EventKind kind)
    {
        if (queue.size() >= options.inputQueueLimit)
            return false;

        queue.push_back(flit);
        Event event;
        event.kind = kind;
        event.time = time;
        event.home = nodeId;
        event.source = flit.source;
        event.target = flit.target;
        event.txnId = flit.txnId;
        event.opcode = flit.opcode;
        if constexpr (requires { flit.Line(); })
            event.line = flit.Line();
        events->Publish(std::move(event));
        return true;
    }

    template<typename T>
    std::optional<T> Pop(std::deque<T>& queue, EventKind kind)
    {
        if (queue.empty())
            return std::nullopt;

        std::optional<T> result(std::move(queue.front()));
        queue.pop_front();
        Event event;
        event.kind = kind;
        event.time = time;
        event.home = nodeId;
        event.source = result->source;
        event.target = result->target;
        event.txnId = result->txnId;
        event.opcode = result->opcode;
        if constexpr (requires { result->Line(); })
            event.line = result->Line();
        events->Publish(std::move(event));
        return result;
    }
};

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
    impl->rxreq.clear();
    impl->rxrsp.clear();
    impl->rxdat.clear();
    impl->txreq.clear();
    impl->txrsp.clear();
    impl->txdat.clear();
    impl->txsnp.clear();
    impl->errors.clear();
    impl->time = 0;
    impl->events->Clear();

    Event event;
    event.kind = EventKind::Reset;
    event.home = impl->nodeId;
    impl->events->Publish(std::move(event));
}

void Core::Tick(Time time)
{
    impl->time = time;
    impl->events->Dispatch();
}

bool Core::Drain()
{
    impl->events->Dispatch();
    return !HasInflight() && impl->events->Pending() == 0;
}

bool Core::DispatchEvents()
{
    return impl->events->Dispatch();
}

bool Core::HasInflight() const noexcept
{
    return !impl->rxreq.empty() || !impl->rxrsp.empty() || !impl->rxdat.empty() ||
           !impl->txreq.empty() || !impl->txrsp.empty() || !impl->txdat.empty() || !impl->txsnp.empty();
}

bool Core::PushRXREQ(const Request& flit) { return impl->Push(impl->rxreq, flit, EventKind::AcceptedRXREQ); }
bool Core::PushRXRSP(const Response& flit) { return impl->Push(impl->rxrsp, flit, EventKind::AcceptedRXRSP); }
bool Core::PushRXDAT(const Data& flit) { return impl->Push(impl->rxdat, flit, EventKind::AcceptedRXDAT); }

std::optional<Request> Core::PeekTXREQ() const { return impl->txreq.empty() ? std::nullopt : std::optional<Request>(impl->txreq.front()); }
std::optional<Response> Core::PeekTXRSP() const { return impl->txrsp.empty() ? std::nullopt : std::optional<Response>(impl->txrsp.front()); }
std::optional<Data> Core::PeekTXDAT() const { return impl->txdat.empty() ? std::nullopt : std::optional<Data>(impl->txdat.front()); }
std::optional<Snoop> Core::PeekTXSNP() const { return impl->txsnp.empty() ? std::nullopt : std::optional<Snoop>(impl->txsnp.front()); }

std::optional<Request> Core::PopTXREQ() { return impl->Pop(impl->txreq, EventKind::EmittedTXREQ); }
std::optional<Response> Core::PopTXRSP() { return impl->Pop(impl->txrsp, EventKind::EmittedTXRSP); }
std::optional<Data> Core::PopTXDAT() { return impl->Pop(impl->txdat, EventKind::EmittedTXDAT); }
std::optional<Snoop> Core::PopTXSNP() { return impl->Pop(impl->txsnp, EventKind::EmittedTXSNP); }

NodeId Core::GetNodeId() const noexcept { return impl->nodeId; }
Time Core::GetTime() const noexcept { return impl->time; }
const Options& Core::GetOptions() const noexcept { return impl->options; }
std::shared_ptr<EventHub> Core::GetEvents() const noexcept { return impl->events; }
const std::vector<ErrorInfo>& Core::GetErrors() const noexcept { return impl->errors; }

}
