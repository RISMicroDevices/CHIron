#include "chi_lunatic_hazard.hpp"

#include <algorithm>

namespace CHI::Lunatic {

    bool HazardGraph::Add(TransactionHandle handle, const LineKey& line, Time age)
    {
        if (handle == INVALID_TRANSACTION)
            return false;

        auto [lineIt, inserted] = lines.try_emplace(line);
        (void)inserted;
        auto& entries = lineIt->second;
        for (const Entry& entry : entries) {
            if (entry.handle == handle)
                return false;
        }

        auto position = std::lower_bound(entries.begin(), entries.end(), Entry{ handle, age },
                                         [](const Entry& left, const Entry& right) {
            if (left.age != right.age)
                return left.age < right.age;
            return left.handle < right.handle;
        });
        entries.insert(position, Entry{ handle, age });
        return true;
    }

    bool HazardGraph::Append(TransactionHandle handle, const LineKey& line, Time age)
    {
        if (handle == INVALID_TRANSACTION)
            return false;

        auto [lineIt, inserted] = lines.try_emplace(line);
        (void)inserted;
        auto& entries = lineIt->second;
        for (const Entry& entry : entries)
            if (entry.handle == handle)
                return false;

        entries.push_back(Entry{ handle, age, true });
        return true;
    }

    bool HazardGraph::IsHead(TransactionHandle handle, const LineKey& line) const noexcept
    {
        auto it = lines.find(line);
        return it != lines.end() && !it->second.empty() && it->second.front().handle == handle;
    }

    bool HazardGraph::Has(const LineKey& line) const noexcept
    {
        auto it = lines.find(line);
        return it != lines.end() && !it->second.empty();
    }

    std::optional<TransactionHandle> HazardGraph::Next(const LineKey& line) const
    {
        auto it = lines.find(line);
        if (it == lines.end() || it->second.empty())
            return std::nullopt;
        return it->second.front().handle;
    }

    std::vector<TransactionHandle> HazardGraph::Order(const LineKey& line) const
    {
        std::vector<TransactionHandle> handles;
        auto it = lines.find(line);
        if (it == lines.end())
            return handles;

        handles.reserve(it->second.size());
        for (const Entry& entry : it->second)
            handles.push_back(entry.handle);
        return handles;
    }

    bool HazardGraph::Remove(TransactionHandle handle, const LineKey& line) noexcept
    {
        auto lineIt = lines.find(line);
        if (lineIt == lines.end())
            return false;

        auto& entries = lineIt->second;
        const auto entry = std::find_if(entries.begin(), entries.end(),
            [handle](const Entry& candidate) { return candidate.handle == handle; });
        if (entry == entries.end())
            return false;
        entries.erase(entry);
        if (entries.empty())
            lines.erase(lineIt);
        return true;
    }

    bool HazardGraph::Release(TransactionHandle handle, const LineKey& line) noexcept
    {
        auto lineIt = lines.find(line);
        if (lineIt == lines.end() || lineIt->second.empty() || lineIt->second.front().handle != handle)
            return false;

        auto& entries = lineIt->second;
        entries.erase(entries.begin());
        if (entries.empty())
            lines.erase(lineIt);
        return true;
    }

    bool HazardGraph::Validate() const noexcept
    {
        for (const auto& [line, entries] : lines) {
            (void)line;
            if (entries.empty())
                return false;

            std::optional<Entry> previous;
            for (size_t index = 0; index < entries.size(); ++index) {
                const Entry& current = entries[index];
                if (current.handle == INVALID_TRANSACTION)
                    return false;
                for (size_t earlier = 0; earlier < index; ++earlier)
                    if (entries[earlier].handle == current.handle)
                        return false;
                if (current.victim)
                    continue;
                if (previous && (previous->age > current.age ||
                    (previous->age == current.age && previous->handle >= current.handle)))
                    return false;
                previous = current;
            }
        }
        return true;
    }

}
