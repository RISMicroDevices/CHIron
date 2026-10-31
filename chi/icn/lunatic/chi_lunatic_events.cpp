#include "chi_lunatic_events.hpp"
#include "chi_lunatic_transaction.hpp"

#include <limits>

namespace CHI::Lunatic {

    TransactionScheduleEvent::TransactionScheduleEvent(Time time, NodeId home, const Transaction& transaction)
        : time(time), home(home), transaction(transaction.handle), phase(transaction.phase),
          readyTime(transaction.readyTime), line(transaction.request.Line())
    {
    }

    void TransactionScheduleEvent::AddDelay(Time delay) noexcept
    {
        if (delay > std::numeric_limits<Time>::max() - additionalDelay)
            additionalDelay = std::numeric_limits<Time>::max();
        else
            additionalDelay += delay;
    }

    void EventHub::FlushDeferred()
    {
        while (!Dispatching() && !deferred.empty())
        {
            Event event = std::move(deferred.front());
            deferred.pop_front();
            Publish(std::move(event), false);
        }
    }

    void EventHub::Defer(Event event)
    {
        if (Dispatching())
            deferred.push_back(std::move(event));
        else
            Publish(std::move(event));
    }

    void EventHub::Publish(Event event, bool flush)
    {
        if (Dispatching())
        {
            deferred.push_back(std::move(event));
            return;
        }

        {
            DispatchScope scope(*this, event.home);
            switch (event.kind)
            {
                case EventKind::AcceptedRXREQ: Fire<EventKind::AcceptedRXREQ>(std::move(event)); break;
                case EventKind::AcceptedRXRSP: Fire<EventKind::AcceptedRXRSP>(std::move(event)); break;
                case EventKind::AcceptedRXDAT: Fire<EventKind::AcceptedRXDAT>(std::move(event)); break;
                case EventKind::EmittedTXREQ: Fire<EventKind::EmittedTXREQ>(std::move(event)); break;
                case EventKind::EmittedTXRSP: Fire<EventKind::EmittedTXRSP>(std::move(event)); break;
                case EventKind::EmittedTXDAT: Fire<EventKind::EmittedTXDAT>(std::move(event)); break;
                case EventKind::EmittedTXSNP: Fire<EventKind::EmittedTXSNP>(std::move(event)); break;
                case EventKind::Denied: Fire<EventKind::Denied>(std::move(event)); break;
                case EventKind::Retired: Fire<EventKind::Retired>(std::move(event)); break;
                case EventKind::Reset: Fire<EventKind::Reset>(std::move(event)); break;
                case EventKind::Error: Fire<EventKind::Error>(std::move(event)); break;
                default: Fire<EventKind::Error>(std::move(event)); break;
            }
        }
        if (flush)
            FlushDeferred();
    }

    RequestAdmissionEvent EventHub::Decide(RequestAdmissionEvent event)
    {
        {
            DispatchScope scope(*this, event.home);
            OnRequestAdmission.Fire(event);
        }
        FlushDeferred();
        return event;
    }

    TransactionScheduleEvent EventHub::Decide(TransactionScheduleEvent event)
    {
        {
            DispatchScope scope(*this, event.home);
            OnTransactionSchedule.Fire(event);
        }
        FlushDeferred();
        return event;
    }

    VictimSelectionEvent EventHub::Decide(VictimSelectionEvent event)
    {
        {
            DispatchScope scope(*this, event.home);
            OnVictimSelection.Fire(event);
        }
        FlushDeferred();
        return event;
    }

    void EventHub::Clear() noexcept
    {
        OnAcceptedRXREQ.UnregisterAll();
        OnAcceptedRXRSP.UnregisterAll();
        OnAcceptedRXDAT.UnregisterAll();
        OnEmittedTXREQ.UnregisterAll();
        OnEmittedTXRSP.UnregisterAll();
        OnEmittedTXDAT.UnregisterAll();
        OnEmittedTXSNP.UnregisterAll();
        OnDenied.UnregisterAll();
        OnRetired.UnregisterAll();
        OnReset.UnregisterAll();
        OnError.UnregisterAll();
        OnRequestAdmission.UnregisterAll();
        OnTransactionSchedule.UnregisterAll();
        OnVictimSelection.UnregisterAll();
        deferred.clear();
    }

}