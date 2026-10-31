#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

#include "../cohestra_instance.hpp"


namespace Cohestra {

    // Fuzzing range element of ARI (Automated Range Iteration),
    // migrated from tl-test-new CFuzzRange.
    struct FuzzARIRange {
        size_t      ordinal;
        uint64_t    maxTag;
        uint64_t    maxSet;
        uint64_t    maxAlias;
    };

    // A granted store acquisition waiting for its data commit. The data plane
    // (CacheLine::Store) must not run inside the node's own completion
    // callback (Taurus ERRATA N3), so grants are queued here and retired from
    // the stimulator's Tick, which is outside the node's call stack. Every
    // store - whether acquired by DoStore or DoStoreLine - commits the whole
    // line with fresh random data, making the full line dirty.
    struct FuzzARIPendingStore {
        uint32_t    upstreamIndex;  // Type-1 upstream index (GetType1Upstream key)
        uint64_t    pa;             // line base address
        uint64_t    data[8];        // full-line store data
        uint64_t    expiryTime;     // drop the commit attempt after this time
    };

    class FuzzARI : public Stimulator {
    public:
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_SEED;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_INTERVAL;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_TARGET;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_STARTUP_CYCLE;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_MAXIMUM_CYCLE;

        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_MEMORY_ENABLE;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_MEMORY_START;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_MEMORY_END;

        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_CMO_ENABLE;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_CMO_RATIO;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_CMO_INVAL_RATIO;

        // Evict (TXEVT) stimulus: with probability 1/evict.ratio per tick the
        // fuzzer evicts a line the upstream is known to hold (DoEvict emits
        // WriteBackFull for UniqueDirty lines, Evict otherwise)
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_EVICT_ENABLE;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_EVICT_RATIO;

        // Prefetch (Stash) stimulus: with probability 1/prefetch.ratio per tick
        // the fuzzer emits a fire-and-forget StashShared (DoPrefetchLoad) or
        // StashUnique (DoPrefetchStore) on a random line of the current range.
        // Disabled by default: when off, no RNG draws are added and the legacy
        // sequence/behavior is preserved bit-identically.
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_PREFETCH_ENABLE;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_PREFETCH_RATIO;
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_PREFETCH_LOAD_ENABLE;
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_PREFETCH_STORE_ENABLE;

        // ReadOnce (DoLoadThrough) stimulus: enabled by default. Explicitly
        // disabling it adds no RNG draws and preserves the legacy sequence.
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_READONCE_ENABLE;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_READONCE_RATIO;

        // Per-source enables: each driven source can be switched off
        // individually. All default true (legacy behavior, legacy RNG draw
        // sequence). A class is available iff its class switch is on AND at
        // least one of its sources is enabled.
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_LOAD_ENABLE;
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_STORE_ENABLE;
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_STORELINE_ENABLE;
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_CMO_CLEAN_ENABLE;
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_CMO_FLUSH_ENABLE;
        static const ConfigurationEntryBack<bool>       CONFIG_FUZZ_ARI_CMO_INVAL_ENABLE;

        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_RANGE_TAG;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_RANGE_SET;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_RANGE_ALIAS;

        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_TAGALIAS_PERMILLIAGE;

        // TagAlias-conflicting read stimulus: permillage rate per tick at which
        // the fuzzer re-reads a line the upstream already holds under a
        // *different* alias (0 = disabled, legacy behavior/RNG sequence). A
        // real VIPT L1 misses in the second set on such an access, so the read
        // reaches the wire and the home must snoop the old-alias copy before
        // granting (nested alias snoop).
        static const ConfigurationEntryBack<uint64_t>   CONFIG_FUZZ_ARI_TAGALIAS_READ_PERMILLIAGE;

    protected:
        std::vector<uint32_t>               type1Upstreams;

        std::string                         section;

        std::mt19937_64                     rng;

        uint64_t                            seed;
        uint64_t                            ariInterval;
        uint64_t                            ariTarget;
        uint64_t                            startupCycle;
        uint64_t                            maximumCycle;

        bool                                memoryEnable;
        uint64_t                            memoryStart;
        uint64_t                            memoryEnd;

        bool                                cmoEnable;
        uint64_t                            cmoRatio;
        uint64_t                            cmoInvalRatio;

        bool                                evictEnable;
        uint64_t                            evictRatio;

        bool                                prefetchEnable;
        uint64_t                            prefetchRatio;
        bool                                prefetchLoadEnable;
        bool                                prefetchStoreEnable;

        bool                                readOnceEnable;
        uint64_t                            readOnceRatio;

        bool                                loadEnable;
        bool                                storeEnable;
        bool                                storeLineEnable;
        bool                                cmoCleanEnable;
        bool                                cmoFlushEnable;
        bool                                cmoInvalEnable;

        uint64_t                            rangeTag;
        uint64_t                            rangeSet;
        uint64_t                            rangeAlias;

        // TagAlias (VIPT aliasing) stimulus: permillage rate per access at which a
        // line's alias is remapped (0 = disabled, legacy behavior). Each upstream
        // keeps its own persistent line -> current-alias map, so same-alias
        // sequences (incl. S->U store-after-load upgrades) dominate by design.
        uint64_t                            tagaliasPermillage;

        uint64_t                            tagaliasReadPermillage;

        std::vector<FuzzARIRange>           ranges;
        std::vector<size_t>                 rangeOrdinal;

        size_t                              rangeIndex;
        uint64_t                            rangeIterationTime;
        size_t                              rangeIterationCount;

        size_t                              startupInterval;

        bool                                finished;

        uint64_t                            countIssued;
        uint64_t                            countDenied;

        uint64_t                            countIssuedCMO;
        uint64_t                            countDeniedCMO;
        uint64_t                            countIssuedCMOClean;
        uint64_t                            countIssuedCMOFlush;
        uint64_t                            countIssuedCMOInval;

        uint64_t                            countIssuedEVT;
        uint64_t                            countDeniedEVT;

        uint64_t                            countIssuedPF;
        uint64_t                            countDeniedPF;
        uint64_t                            countIssuedPFLoad;
        uint64_t                            countIssuedPFStore;

        uint64_t                            countIssuedTAR;
        uint64_t                            countDeniedTAR;

        uint64_t                            countIssuedRO;
        uint64_t                            countCompletedRO;
        uint64_t                            countDeniedRO;

        uint64_t                            countStoreCommitted;
        uint64_t                            countStoreDropped;

        std::vector<FuzzARIPendingStore>    pendingStores;

        // per-upstream current TagAlias per line (line base >> 6); consulted only
        // when tagaliasPermillage > 0, so the disabled case never adds RNG draws
        std::array<std::unordered_map<uint64_t, uint8_t>, 8>
                                            aliasMap;

        // per-upstream line-base addresses recently acquired by this fuzzer's
        // memory ops; probed for Evict targets so DoEvict virtually always hits
        // a present line. Consulted only when evictEnable, so the disabled case
        // never adds RNG draws
        std::array<std::vector<uint64_t>, 8>
                                            evictableLines;

    public:
        static constexpr size_t             PENDING_STORE_LIMIT   = 64;
        static constexpr size_t             ALIAS_MAP_LIMIT       = 4096;
        static constexpr size_t             EVICTABLE_LINE_LIMIT  = 256;
        static constexpr size_t             EVICT_PROBE_ATTEMPTS  = 4;

    public:
        FuzzARI(Instance&                    instance,
                size_t                       index,
                const std::vector<uint32_t>& type1Upstreams) noexcept;

        virtual ~FuzzARI() noexcept = default;

    public:
        virtual bool        Initialize(const StringLoaderBase& configSource) noexcept override;

    public:
        virtual void        Tick(uint64_t time) noexcept override;

    public:
        bool                IsFinished() const noexcept override;

    protected:
        uint64_t            Random64() noexcept;

        uint64_t            GenerateAddress(const FuzzARIRange& range, bool doAlias, uint64_t& aliasOut) noexcept;

        void                RandomTest(uint64_t time) noexcept;

        void                QueuePendingStore(uint32_t upstreamIndex, uint64_t pa, uint64_t time) noexcept;

        void                RetirePendingStores(uint64_t time) noexcept;

        void                TrackEvictableLine(uint32_t upstreamIndex, uint64_t addr) noexcept;

        bool                TryDoEvict(uint32_t upstreamIndex) noexcept;

        bool                TryDoPrefetch(uint32_t upstreamIndex) noexcept;

        bool                TryDoReadOnce(uint32_t upstreamIndex) noexcept;

        bool                TryDoAliasRead(uint32_t upstreamIndex) noexcept;

        template<class TEvent>
        void                CountResult(const std::shared_ptr<CCHI::Taurus::FutureNow<TEvent>>& result) noexcept;

        template<class T>
        bool                LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

        static size_t       Factorial(size_t n) noexcept;
    };

    class FuzzARIFactory : public StimulatorFactory {
    public:
        FuzzARIFactory() noexcept;

        virtual ~FuzzARIFactory() noexcept = default;

    public:
        virtual std::unique_ptr<Stimulator> Create(Instance&                    instance,
                                                   size_t                       index,
                                                   const std::vector<uint32_t>& type1Upstreams) noexcept override;
    };
}


// Inline implementation of: class FuzzARI
namespace Cohestra {

    template<class TEvent>
    inline void FuzzARI::CountResult(const std::shared_ptr<CCHI::Taurus::FutureNow<TEvent>>& result) noexcept
    {
        if (result && !result->IsRejected())
            countIssued++;
        else
            countDenied++;
    }

    template<class T>
    inline bool FuzzARI::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry, section);

        if (!rawValue)
        {
            dst = entry->defaultValue;
            return true;
        }

        std::optional<T> deserializedValue = entry->deserializer(*rawValue);

        if (!deserializedValue)
        {
            GetOwner().GetLogger()->error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }
}
