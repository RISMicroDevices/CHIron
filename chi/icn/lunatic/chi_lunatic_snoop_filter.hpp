#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_state.hpp"
#include "chi_lunatic_types.hpp"

#include <map>
#include <optional>
#include <vector>

namespace CHI::Lunatic {

    struct SnoopFilterEntry {
        LineKey                         key{};
        std::map<NodeId, CacheState>    requesters;
        uint64_t                        age = 0;
        bool                            valid = false;
    };

    class SnoopFilter {
        size_t                          sets;
        size_t                          ways;
        bool                            precise;
        std::vector<SnoopFilterEntry>   entries;

        size_t                          Index(size_t set, size_t way) const noexcept;
        SnoopFilterEntry&               Acquire(const LineKey& key, uint64_t age);

    public:
        SnoopFilter(size_t sets, size_t ways, bool precise = true);
        explicit SnoopFilter(const Options& options);

        size_t                          Sets() const noexcept { return sets; }
        size_t                          Ways() const noexcept { return ways; }
        size_t                          SetIndex(const LineKey& key) const noexcept;
        SnoopFilterEntry*               Find(const LineKey& key) noexcept;
        const SnoopFilterEntry*         Find(const LineKey& key) const noexcept;
        SnoopFilterEntry&               At(size_t set, size_t way) noexcept;
        const SnoopFilterEntry&         At(size_t set, size_t way) const noexcept;

        std::optional<size_t>           FindWay(const LineKey& key) const noexcept;
        std::vector<size_t>             VictimCandidates(const LineKey& key) const;
        size_t                          ChooseVictim(const LineKey& key) const;
        bool                            HasInvalidWay(const LineKey& key) const noexcept;
        void                            Invalidate(size_t set, size_t way) noexcept;

        void                            Track(const LineKey& key, NodeId requester, CacheState state, uint64_t age);
        void                            Remove(const LineKey& key, NodeId requester);
        void                            RemoveRequester(NodeId requester) noexcept;
        CacheState                      State(const LineKey& key, NodeId requester) const noexcept;
        std::vector<NodeId>             Sharers(const LineKey& key) const;
        std::vector<NodeId>             Targets(const LineKey& key) const;
        bool                            HasRequester(const LineKey& key, NodeId requester) const noexcept;

        void                            Clear() noexcept;
        size_t                          ValidCount() const noexcept;
        bool                            IsPrecise() const noexcept { return precise; }
    };

}
