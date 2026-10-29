#include "chi_lunatic_arbiter.hpp"

#include <algorithm>

namespace CHI::Lunatic {

    bool                                Arbiter::Submit(TransactionHandle handle, Time age, uint8_t qos)
    {
        if (handle == INVALID_TRANSACTION)
            return false;

        auto duplicate = std::find_if(entries.begin(), entries.end(),
                                      [handle](const Entry& entry) { return entry.handle == handle; });
        if (duplicate != entries.end())
            return false;

        entries.push_back({ handle, age, qos });
        return true;
    }

    bool                                Arbiter::Remove(TransactionHandle handle) noexcept
    {
        auto it = std::find_if(entries.begin(), entries.end(),
                               [handle](const Entry& entry) { return entry.handle == handle; });
        if (it == entries.end())
            return false;

        entries.erase(it);
        return true;
    }

    std::optional<TransactionHandle>    Arbiter::Peek() const noexcept
    {
        const Entry* selected = nullptr;
        for (const Entry& entry : entries) {
            if (!selected || entry.age < selected->age ||
                (entry.age == selected->age && entry.handle < selected->handle))
                selected = &entry;
        }

        if (!selected)
            return std::nullopt;
        return selected->handle;
    }

    std::optional<TransactionHandle>    Arbiter::PeekQoS() const noexcept
    {
        const Entry* selected = nullptr;
        for (const Entry& entry : entries) {
            if (!selected || entry.qos > selected->qos ||
                (entry.qos == selected->qos &&
                 (entry.age < selected->age ||
                  (entry.age == selected->age && entry.handle < selected->handle))))
                selected = &entry;
        }

        if (!selected)
            return std::nullopt;
        return selected->handle;
    }

}
