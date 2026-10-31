#pragma once

#include "chi_lunatic_types.hpp"
#include "chi_lunatic_state.hpp"
#include "../../../common/eventbus.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace CHI::Lunatic {

    struct Transaction;

    enum class EventKind : uint8_t {
        AcceptedRXREQ,
        AcceptedRXRSP,
        AcceptedRXDAT,
        EmittedTXREQ,
        EmittedTXRSP,
        EmittedTXDAT,
        EmittedTXSNP,
        Denied,
        Retired,
        Reset,
        Error
    };

    struct Event {
        EventKind         kind = EventKind::Error;
        Time              time = 0;
        NodeId            home = 0;
        NodeId            source = 0;
        NodeId            target = 0;
        TransactionId     txnId = 0;
        TransactionHandle transaction = INVALID_TRANSACTION;
        uint16_t          opcode = 0;
        LineKey           line{};
        std::string       message;
    };

    template<EventKind kind>
    struct EventRecord : Event, Gravity::Event<EventRecord<kind>> {
        EventRecord()
        {
            this->kind = kind;
        }

        explicit EventRecord(Event event)
            : Event(std::move(event))
        {
            this->kind = kind;
        }
    };

    using AcceptedRXREQEvent = EventRecord<EventKind::AcceptedRXREQ>;
    using AcceptedRXRSPEvent = EventRecord<EventKind::AcceptedRXRSP>;
    using AcceptedRXDATEvent = EventRecord<EventKind::AcceptedRXDAT>;
    using EmittedTXREQEvent = EventRecord<EventKind::EmittedTXREQ>;
    using EmittedTXRSPEvent = EventRecord<EventKind::EmittedTXRSP>;
    using EmittedTXDATEvent = EventRecord<EventKind::EmittedTXDAT>;
    using EmittedTXSNPEvent = EventRecord<EventKind::EmittedTXSNP>;
    using DeniedEvent = EventRecord<EventKind::Denied>;
    using RetiredEvent = EventRecord<EventKind::Retired>;
    using ResetEvent = EventRecord<EventKind::Reset>;
    using ErrorEvent = EventRecord<EventKind::Error>;

    class RequestAdmissionEvent : public Gravity::Event<RequestAdmissionEvent> {
        bool backpressure = false;
        bool retry = false;

    public:
        const Time     time;
        const NodeId   home;
        const Request  request;

        RequestAdmissionEvent(Time time, NodeId home, const Request& request)
            : time(time), home(home), request(request)
        {
        }

        void Backpressure() noexcept { backpressure = true; }
        void RequestRetry() noexcept { retry = true; }
        bool IsBackpressured() const noexcept { return backpressure; }
        bool IsRetryRequested() const noexcept { return retry; }
    };

    class TransactionScheduleEvent : public Gravity::Event<TransactionScheduleEvent> {
        Time additionalDelay = 0;
        bool backpressure = false;

    public:
        const Time              time;
        const NodeId            home;
        const TransactionHandle transaction;
        const Phase             phase;
        const Time              readyTime;
        const LineKey           line;

        TransactionScheduleEvent(Time time, NodeId home, const Transaction& transaction);
        void Backpressure() noexcept { backpressure = true; }
        void AddDelay(Time delay) noexcept;
        Time GetAdditionalDelay() const noexcept { return additionalDelay; }
        bool IsBackpressured() const noexcept { return backpressure; }
    };

    enum class VictimDomain : uint8_t {
        Directory,
        SnoopFilter
    };

    class VictimSelectionEvent : public Gravity::Event<VictimSelectionEvent> {
        std::optional<size_t> selectedWay;

    public:
        const Time                   time;
        const NodeId                 home;
        const TransactionHandle      transaction;
        const VictimDomain           domain;
        const LineKey                line;
        const std::vector<size_t>    candidates;

        VictimSelectionEvent(Time time, NodeId home, TransactionHandle transaction, VictimDomain domain,
                             const LineKey& line, std::vector<size_t> candidates)
            : time(time), home(home), transaction(transaction), domain(domain), line(line),
              candidates(std::move(candidates))
        {
        }

        void SelectVictim(size_t way) noexcept { selectedWay = way; }
        std::optional<size_t> GetSelectedVictim() const noexcept { return selectedWay; }
    };

    class SnoopForwardingEvent : public Gravity::Event<SnoopForwardingEvent> {
        bool forwarding = true;

    public:
        const Time              time;
        const NodeId            home;
        const TransactionHandle transaction;
        const Request           request;
        const NodeId            provider;
        const CacheState        providerState;
        const size_t            trackedOwners;
        const bool              homeHit;
        const bool              homeDirty;

        SnoopForwardingEvent(Time time, NodeId home, const Transaction& transaction, NodeId provider,
                             CacheState providerState, size_t trackedOwners, bool homeDirty);
        void UseForwarding() noexcept { forwarding = true; }
        void UseNonForwarding() noexcept { forwarding = false; }
        bool IsForwarding() const noexcept { return forwarding; }
    };

    class EventHub {
        size_t             dispatchDepth = 0;
        std::deque<Event>   deferred;
        std::vector<NodeId> dispatchHomes;

        class DispatchScope {
            EventHub& hub;

        public:
            DispatchScope(EventHub& hub, NodeId home) : hub(hub)
            {
                hub.dispatchHomes.push_back(home);
                ++hub.dispatchDepth;
            }
            ~DispatchScope()
            {
                hub.dispatchHomes.pop_back();
                --hub.dispatchDepth;
            }
        };

        template<EventKind kind>
        Gravity::EventBus<EventRecord<kind>>& Bus() noexcept
        {
            if constexpr (kind == EventKind::AcceptedRXREQ)
                return OnAcceptedRXREQ;
            else if constexpr (kind == EventKind::AcceptedRXRSP)
                return OnAcceptedRXRSP;
            else if constexpr (kind == EventKind::AcceptedRXDAT)
                return OnAcceptedRXDAT;
            else if constexpr (kind == EventKind::EmittedTXREQ)
                return OnEmittedTXREQ;
            else if constexpr (kind == EventKind::EmittedTXRSP)
                return OnEmittedTXRSP;
            else if constexpr (kind == EventKind::EmittedTXDAT)
                return OnEmittedTXDAT;
            else if constexpr (kind == EventKind::EmittedTXSNP)
                return OnEmittedTXSNP;
            else if constexpr (kind == EventKind::Denied)
                return OnDenied;
            else if constexpr (kind == EventKind::Retired)
                return OnRetired;
            else if constexpr (kind == EventKind::Reset)
                return OnReset;
            else
            {
                static_assert(kind == EventKind::Error, "unsupported Lunatic event kind");
                return OnError;
            }
        }

        template<EventKind kind>
        void Fire(Event event)
        {
            EventRecord<kind> record(std::move(event));
            Bus<kind>().Fire(record);
        }

        void Dispatch(Event event);
        void FlushDeferred(bool force = false);

    public:
        Gravity::EventBus<AcceptedRXREQEvent>       OnAcceptedRXREQ;
        Gravity::EventBus<AcceptedRXRSPEvent>       OnAcceptedRXRSP;
        Gravity::EventBus<AcceptedRXDATEvent>       OnAcceptedRXDAT;
        Gravity::EventBus<EmittedTXREQEvent>        OnEmittedTXREQ;
        Gravity::EventBus<EmittedTXRSPEvent>        OnEmittedTXRSP;
        Gravity::EventBus<EmittedTXDATEvent>        OnEmittedTXDAT;
        Gravity::EventBus<EmittedTXSNPEvent>        OnEmittedTXSNP;
        Gravity::EventBus<DeniedEvent>              OnDenied;
        Gravity::EventBus<RetiredEvent>             OnRetired;
        Gravity::EventBus<ResetEvent>               OnReset;
        Gravity::EventBus<ErrorEvent>               OnError;
        Gravity::EventBus<RequestAdmissionEvent>    OnRequestAdmission;
        Gravity::EventBus<TransactionScheduleEvent> OnTransactionSchedule;
        Gravity::EventBus<VictimSelectionEvent>     OnVictimSelection;
        Gravity::EventBus<SnoopForwardingEvent>     OnSnoopForwarding;

        bool Dispatching() const noexcept { return dispatchDepth != 0; }
        bool Dispatching(NodeId home) const noexcept
        {
            for (NodeId active : dispatchHomes)
                if (active == home)
                    return true;
            return false;
        }
        void Defer(Event event);
        void DispatchDeferred() { FlushDeferred(); }
        void Publish(Event event, bool flush = true);
        RequestAdmissionEvent Decide(RequestAdmissionEvent event);
        TransactionScheduleEvent Decide(TransactionScheduleEvent event);
        VictimSelectionEvent Decide(VictimSelectionEvent event);
        SnoopForwardingEvent Decide(SnoopForwardingEvent event);
        void Clear() noexcept;
    };

}
