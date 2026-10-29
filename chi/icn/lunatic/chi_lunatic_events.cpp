#include "chi_lunatic_events.hpp"

#include <algorithm>
#include <utility>

namespace CHI::Lunatic {

    uint64_t                            EventHub::Register(std::string name, int priority, Listener listener)
    {
        if (!listener)
            return 0;

        const uint64_t id = nextListenerId++;
        listeners.push_back({ id, std::move(name), priority, nextOrder++, std::move(listener) });
        std::stable_sort(listeners.begin(), listeners.end(), [](const auto& a, const auto& b) {
            if (a.priority != b.priority)
                return a.priority < b.priority;
            return a.order < b.order;
        });
        return id;
    }

    bool                                EventHub::Unregister(uint64_t id) noexcept
    {
        if (dispatching)
            return false;

        const auto it = std::find_if(listeners.begin(), listeners.end(), [id](const auto& entry) {
            return entry.id == id;
        });
        if (it == listeners.end())
            return false;

        listeners.erase(it);
        return true;
    }

    void                                EventHub::Publish(Event event)
    {
        pending.push_back(std::move(event));
    }

    bool                                EventHub::Dispatch()
    {
        if (dispatching)
        {
            reentryRejected = true;
            return false;
        }

        dispatching = true;
        while (!pending.empty())
        {
            Event event = std::move(pending.front());
            pending.pop_front();
            for (const auto& entry : listeners)
                entry.listener(event);
        }
        dispatching = false;
        return true;
    }

    void                                EventHub::Clear() noexcept
    {
        pending.clear();
        reentryRejected = false;
    }

}
