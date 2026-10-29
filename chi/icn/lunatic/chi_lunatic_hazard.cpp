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

            for (size_t index = 0; index < entries.size(); ++index) {
                if (entries[index].handle == INVALID_TRANSACTION)
                    return false;
                if (index == 0)
                    continue;

                const Entry& previous = entries[index - 1];
                const Entry& current = entries[index];
                if (previous.age > current.age ||
                    (previous.age == current.age && previous.handle >= current.handle))
                    return false;
            }
        }
        return true;
    }

}
