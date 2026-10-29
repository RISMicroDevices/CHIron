#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_types.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace CHI::Lunatic {

    struct HomeLine {
        LineKey                     key{};
        LineData                    data{};
        ByteEnables                 validBytes{};
        uint64_t                    age = 0;
        bool                        valid = false;
        bool                        dirty = false;
    };

    class Directory {
        size_t                      sets;
        size_t                      ways;
        std::vector<HomeLine>       lines;

        size_t                      Index(size_t set, size_t way) const noexcept;

    public:
        Directory(size_t sets, size_t ways);
        explicit Directory(const Options& options);

        size_t                      Sets() const noexcept { return sets; }
        size_t                      Ways() const noexcept { return ways; }
        size_t                      SetIndex(const LineKey& key) const noexcept;

        HomeLine*                   Find(const LineKey& key) noexcept;
        const HomeLine*             Find(const LineKey& key) const noexcept;
        HomeLine&                   At(size_t set, size_t way) noexcept;
        const HomeLine&             At(size_t set, size_t way) const noexcept;

        std::optional<size_t>       FindWay(const LineKey& key) const noexcept;
        std::vector<size_t>         VictimCandidates(const LineKey& key) const;
        size_t                      ChooseVictim(const LineKey& key) const;

        HomeLine&                   Install(const LineKey& key, size_t way, uint64_t age);
        void                        Invalidate(size_t set, size_t way) noexcept;
        void                        Clear() noexcept;
        size_t                      ValidCount() const noexcept;
    };

}
