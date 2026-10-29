#pragma once

#include "chi_lunatic_types.hpp"
#include "../../../common/eventbus.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace CHI::Lunatic {

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

    class EventHub {
    public:
        Gravity::EventBus<AcceptedRXREQEvent> OnAcceptedRXREQ;
        Gravity::EventBus<AcceptedRXRSPEvent> OnAcceptedRXRSP;
        Gravity::EventBus<AcceptedRXDATEvent> OnAcceptedRXDAT;
        Gravity::EventBus<EmittedTXREQEvent>  OnEmittedTXREQ;
        Gravity::EventBus<EmittedTXRSPEvent>  OnEmittedTXRSP;
        Gravity::EventBus<EmittedTXDATEvent>  OnEmittedTXDAT;
        Gravity::EventBus<EmittedTXSNPEvent>  OnEmittedTXSNP;
        Gravity::EventBus<DeniedEvent>        OnDenied;
        Gravity::EventBus<RetiredEvent>       OnRetired;
        Gravity::EventBus<ResetEvent>         OnReset;
        Gravity::EventBus<ErrorEvent>         OnError;

    private:
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

    private:
        template<EventKind kind>
        void Fire(Event event)
        {
            EventRecord<kind> record(std::move(event));
            Bus<kind>().Fire(record);
        }

    public:
        // Listeners run synchronously while node containers may be changing.
        // A listener must not call back into the same node during publication.
        void Publish(Event event)
        {
            switch (event.kind)
            {
                case EventKind::AcceptedRXREQ: Fire<EventKind::AcceptedRXREQ>(std::move(event)); return;
                case EventKind::AcceptedRXRSP: Fire<EventKind::AcceptedRXRSP>(std::move(event)); return;
                case EventKind::AcceptedRXDAT: Fire<EventKind::AcceptedRXDAT>(std::move(event)); return;
                case EventKind::EmittedTXREQ: Fire<EventKind::EmittedTXREQ>(std::move(event)); return;
                case EventKind::EmittedTXRSP: Fire<EventKind::EmittedTXRSP>(std::move(event)); return;
                case EventKind::EmittedTXDAT: Fire<EventKind::EmittedTXDAT>(std::move(event)); return;
                case EventKind::EmittedTXSNP: Fire<EventKind::EmittedTXSNP>(std::move(event)); return;
                case EventKind::Denied: Fire<EventKind::Denied>(std::move(event)); return;
                case EventKind::Retired: Fire<EventKind::Retired>(std::move(event)); return;
                case EventKind::Reset: Fire<EventKind::Reset>(std::move(event)); return;
                case EventKind::Error: Fire<EventKind::Error>(std::move(event)); return;
                default: Fire<EventKind::Error>(std::move(event)); return;
            }
        }

        void Clear() noexcept;
    };

}
