#include "chi_lunatic_snoop_filter.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace CHI::Lunatic {

SnoopFilter::SnoopFilter(size_t sets, size_t ways, bool precise)
    : sets(sets), ways(ways), precise(precise)
{
    if (!sets || !ways)
        throw std::invalid_argument("Lunatic snoop filter geometry must be non-zero");
    if (sets > std::numeric_limits<size_t>::max() / ways)
        throw std::invalid_argument("Lunatic snoop filter geometry is too large");

    entries.resize(sets * ways);
}

SnoopFilter::SnoopFilter(const Options& options)
    : SnoopFilter(options.snoopFilterSets, options.snoopFilterWays, options.preciseSnoopFilter)
{
}

size_t SnoopFilter::Index(size_t set, size_t way) const noexcept
{
    return set * ways + way;
}

size_t SnoopFilter::SetIndex(const LineKey& key) const noexcept
{
    return size_t((key.address / LINE_SIZE) % sets);
}

SnoopFilterEntry* SnoopFilter::Find(const LineKey& key) noexcept
{
    const size_t set = SetIndex(key);
    for (size_t way = 0; way < ways; ++way) {
        SnoopFilterEntry& entry = At(set, way);
        if (entry.valid && entry.key == key)
            return &entry;
    }
    return nullptr;
}

const SnoopFilterEntry* SnoopFilter::Find(const LineKey& key) const noexcept
{
    const size_t set = SetIndex(key);
    for (size_t way = 0; way < ways; ++way) {
        const SnoopFilterEntry& entry = At(set, way);
        if (entry.valid && entry.key == key)
            return &entry;
    }
    return nullptr;
}

SnoopFilterEntry& SnoopFilter::At(size_t set, size_t way) noexcept
{
    return entries[Index(set, way)];
}

const SnoopFilterEntry& SnoopFilter::At(size_t set, size_t way) const noexcept
{
    return entries[Index(set, way)];
}

std::optional<size_t> SnoopFilter::FindWay(const LineKey& key) const noexcept
{
    const size_t set = SetIndex(key);
    for (size_t way = 0; way < ways; ++way) {
        const SnoopFilterEntry& entry = At(set, way);
        if (entry.valid && entry.key == key)
            return way;
    }
    return std::nullopt;
}

std::vector<size_t> SnoopFilter::VictimCandidates(const LineKey&) const
{
    std::vector<size_t> candidates;
    candidates.reserve(ways);
    for (size_t way = 0; way < ways; ++way)
        candidates.push_back(way);
    return candidates;
}

size_t SnoopFilter::ChooseVictim(const LineKey& key) const
{
    const size_t set = SetIndex(key);
    for (size_t way = 0; way < ways; ++way) {
        if (!At(set, way).valid)
            return way;
    }
    size_t victim = 0;
    for (size_t way = 1; way < ways; ++way) {
        if (At(set, way).age < At(set, victim).age)
            victim = way;
    }
    return victim;
}

SnoopFilterEntry& SnoopFilter::Acquire(const LineKey& key, uint64_t age)
{
    if (SnoopFilterEntry* current = Find(key)) {
        current->age = age;
        return *current;
    }

    SnoopFilterEntry& entry = At(SetIndex(key), ChooseVictim(key));
    entry.requesters.clear();
    entry.key = key;
    entry.age = age;
    entry.valid = true;
    return entry;
}

void SnoopFilter::Track(const LineKey& key, NodeId requester, CacheState state, uint64_t age)
{
    if (precise && state == CacheState::I) {
        Remove(key, requester);
        return;
    }

    Acquire(key, age).requesters[requester] = state;
}

void SnoopFilter::Remove(const LineKey& key, NodeId requester)
{
    SnoopFilterEntry* entry = Find(key);
    if (!entry)
        return;

    entry->requesters.erase(requester);
    if (entry->requesters.empty()) {
        entry->key = LineKey{};
        entry->age = 0;
        entry->valid = false;
    }
}

void SnoopFilter::RemoveRequester(NodeId requester) noexcept
{
    for (SnoopFilterEntry& entry : entries) {
        if (!entry.valid)
            continue;

        entry.requesters.erase(requester);
        if (entry.requesters.empty()) {
            entry.key = LineKey{};
            entry.age = 0;
            entry.valid = false;
        }
    }
}

CacheState SnoopFilter::State(const LineKey& key, NodeId requester) const noexcept
{
    const SnoopFilterEntry* entry = Find(key);
    if (!entry)
        return CacheState::I;

    const auto requesterIt = entry->requesters.find(requester);
    return requesterIt == entry->requesters.end() ? CacheState::I : requesterIt->second;
}

std::vector<NodeId> SnoopFilter::Sharers(const LineKey& key) const
{
    std::vector<NodeId> sharers;
    const SnoopFilterEntry* entry = Find(key);
    if (!entry)
        return sharers;

    for (const auto& [requester, state] : entry->requesters) {
        if (IsShared(state))
            sharers.push_back(requester);
    }
    return sharers;
}

std::vector<NodeId> SnoopFilter::Targets(const LineKey& key) const
{
    std::vector<NodeId> targets;
    const SnoopFilterEntry* entry = Find(key);
    if (!entry)
        return targets;

    for (const auto& [requester, state] : entry->requesters) {
        if (!precise || IsValid(state))
            targets.push_back(requester);
    }
    return targets;
}

bool SnoopFilter::HasRequester(const LineKey& key, NodeId requester) const noexcept
{
    const SnoopFilterEntry* entry = Find(key);
    return entry && entry->requesters.contains(requester);
}

void SnoopFilter::Clear() noexcept
{
    for (SnoopFilterEntry& entry : entries) {
        entry.requesters.clear();
        entry.key = LineKey{};
        entry.age = 0;
        entry.valid = false;
    }
}

size_t SnoopFilter::ValidCount() const noexcept
{
    return size_t(std::count_if(entries.begin(), entries.end(), [](const SnoopFilterEntry& entry) {
        return entry.valid;
    }));
}

}
