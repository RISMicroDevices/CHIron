#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_random.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace CHI::Lunatic {

    class ReplacementState {
        ReplacementKind                       kind;
        size_t                                sets;
        size_t                                ways;
        uint64_t                              seed;
        std::shared_ptr<DeterministicRandom> random;
        std::vector<uint8_t>                  plru;

        static size_t BitCount(size_t sets, size_t ways)
        {
            if (!sets || !ways)
                throw std::invalid_argument("Lunatic replacement geometry must be non-zero");
            if (ways > std::numeric_limits<size_t>::max() / 2 ||
                sets > std::numeric_limits<size_t>::max() / (ways * 2))
                throw std::invalid_argument("Lunatic replacement geometry is too large");
            return sets * ways * 2;
        }

        size_t PLRUIndex(size_t set, size_t node) const noexcept
        {
            return set * ways * 2 + node;
        }

    public:
        ReplacementState(ReplacementKind kind, size_t sets, size_t ways, uint64_t seed,
                         std::shared_ptr<DeterministicRandom> random = {})
            : kind(kind), sets(sets), ways(ways), seed(seed), random(std::move(random)),
              plru(BitCount(sets, ways), 0)
        {
            if (!this->random)
                this->random = std::make_shared<DeterministicRandom>(seed);
        }

        void Reset()
        {
            std::fill(plru.begin(), plru.end(), 0);
            random->Reset(seed);
        }

        void Update(size_t set, size_t way)
        {
            if (kind != ReplacementKind::PLRU || way >= ways)
                return;
            size_t node = 0;
            size_t first = 0;
            size_t last = ways;
            while (last - first > 1)
            {
                const size_t middle = first + (last - first) / 2;
                if (way < middle)
                {
                    plru[PLRUIndex(set, node)] = 1;
                    node = node * 2 + 1;
                    last = middle;
                }
                else
                {
                    plru[PLRUIndex(set, node)] = 0;
                    node = node * 2 + 2;
                    first = middle;
                }
            }
        }

        template<typename Entry>
        size_t Choose(size_t set, const std::vector<Entry>& entries,
                      const std::vector<size_t>* candidates = nullptr)
        {
            const auto available = [&](size_t first, size_t last) {
                if (!candidates)
                    return true;
                for (size_t way = first; way < last; ++way)
                    if (std::find(candidates->begin(), candidates->end(), way) != candidates->end())
                        return true;
                return false;
            };
            if (candidates)
            {
                for (size_t way : *candidates)
                    if (!entries[set * ways + way].valid)
                        return way;
                if (candidates->size() == 1)
                    return candidates->front();
            }
            else
            {
                for (size_t way = 0; way < ways; ++way)
                    if (!entries[set * ways + way].valid)
                        return way;
            }
            if (ways == 1)
                return candidates ? candidates->front() : 0;
            if (kind == ReplacementKind::PseudoRandom)
                return candidates ? (*candidates)[random->Bounded(candidates->size() - 1)] : random->Bounded(ways - 1);
            if (kind == ReplacementKind::Age)
            {
                size_t victim = candidates ? candidates->front() : 0;
                if (candidates)
                {
                    for (size_t way : *candidates)
                        if (entries[set * ways + way].age < entries[set * ways + victim].age)
                            victim = way;
                    return victim;
                }
                for (size_t way = 1; way < ways; ++way)
                    if (entries[set * ways + way].age < entries[set * ways + victim].age)
                        victim = way;
                return victim;
            }

            size_t node = 0;
            size_t way = 0;
            size_t remaining = ways;
            while (remaining > 1)
            {
                const size_t middle = remaining / 2;
                const bool lowerAvailable = available(way, way + middle);
                const bool upperAvailable = available(way + middle, way + remaining);
                if ((plru[PLRUIndex(set, node)] && upperAvailable) || !lowerAvailable)
                {
                    way += middle;
                    remaining -= middle;
                    node = node * 2 + 2;
                }
                else
                {
                    remaining = middle;
                    node = node * 2 + 1;
                }
            }
            return way;
        }
    };

}
