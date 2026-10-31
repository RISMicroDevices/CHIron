#include "fuzz_ari.hpp"

#include <algorithm>
#include <span>


// Implementation of: class FuzzARI
namespace Cohestra {

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_SEED
        ("FUZZ_ARI_SEED", "fuzz.ari.seed", nullptr, 1000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_INTERVAL
        ("FUZZ_ARI_INTERVAL", "fuzz.ari.interval", nullptr, 5000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_TARGET
        ("FUZZ_ARI_TARGET", "fuzz.ari.target", nullptr, 24, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_STARTUP_CYCLE
        ("FUZZ_ARI_STARTUP_CYCLE", "fuzz.ari.startup", nullptr, 0, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_MAXIMUM_CYCLE
        ("FUZZ_ARI_MAXIMUM_CYCLE", "fuzz.ari.maximum.cycle", nullptr, 0, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_MEMORY_ENABLE
        ("FUZZ_ARI_MEMORY_ENABLE", "fuzz.ari.memory.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_MEMORY_START
        ("FUZZ_ARI_MEMORY_START", "fuzz.ari.memory.start", nullptr, 0x80000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_MEMORY_END
        ("FUZZ_ARI_MEMORY_END", "fuzz.ari.memory.end", nullptr, 0xA0000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_CMO_ENABLE
        ("FUZZ_ARI_CMO_ENABLE", "fuzz.ari.cmo.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_CMO_RATIO
        ("FUZZ_ARI_CMO_RATIO", "fuzz.ari.cmo.ratio", nullptr, 8, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_CMO_INVAL_RATIO
        ("FUZZ_ARI_CMO_INVAL_RATIO", "fuzz.ari.cmo.inval.ratio", nullptr, 4, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_EVICT_ENABLE
        ("FUZZ_ARI_EVICT_ENABLE", "fuzz.ari.evict.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_EVICT_RATIO
        ("FUZZ_ARI_EVICT_RATIO", "fuzz.ari.evict.ratio", nullptr, 16, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_PREFETCH_ENABLE
        ("FUZZ_ARI_PREFETCH_ENABLE", "fuzz.ari.prefetch.enable", nullptr, false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_PREFETCH_RATIO
        ("FUZZ_ARI_PREFETCH_RATIO", "fuzz.ari.prefetch.ratio", nullptr, 8, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_PREFETCH_LOAD_ENABLE
        ("FUZZ_ARI_PREFETCH_LOAD_ENABLE", "fuzz.ari.prefetch.load.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_PREFETCH_STORE_ENABLE
        ("FUZZ_ARI_PREFETCH_STORE_ENABLE", "fuzz.ari.prefetch.store.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_READONCE_ENABLE
        ("FUZZ_ARI_READONCE_ENABLE", "fuzz.ari.readonce.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_READONCE_RATIO
        ("FUZZ_ARI_READONCE_RATIO", "fuzz.ari.readonce.ratio", nullptr, 8, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_LOAD_ENABLE
        ("FUZZ_ARI_LOAD_ENABLE", "fuzz.ari.load.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_STORE_ENABLE
        ("FUZZ_ARI_STORE_ENABLE", "fuzz.ari.store.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_STORELINE_ENABLE
        ("FUZZ_ARI_STORELINE_ENABLE", "fuzz.ari.storeline.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_CMO_CLEAN_ENABLE
        ("FUZZ_ARI_CMO_CLEAN_ENABLE", "fuzz.ari.cmo.clean.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_CMO_FLUSH_ENABLE
        ("FUZZ_ARI_CMO_FLUSH_ENABLE", "fuzz.ari.cmo.flush.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> FuzzARI::CONFIG_FUZZ_ARI_CMO_INVAL_ENABLE
        ("FUZZ_ARI_CMO_INVAL_ENABLE", "fuzz.ari.cmo.inval.enable", nullptr, true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_RANGE_TAG
        ("FUZZ_ARI_RANGE_TAG", "fuzz.ari.range.tag", nullptr, 0x80, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_RANGE_SET
        ("FUZZ_ARI_RANGE_SET", "fuzz.ari.range.set", nullptr, 0x80, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_RANGE_ALIAS
        ("FUZZ_ARI_RANGE_ALIAS", "fuzz.ari.range.alias", nullptr, 0x4, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_TAGALIAS_PERMILLIAGE
        ("FUZZ_ARI_TAGALIAS_PERMILLIAGE", "fuzz.ari.tagalias.permillage", nullptr, 0, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> FuzzARI::CONFIG_FUZZ_ARI_TAGALIAS_READ_PERMILLIAGE
        ("FUZZ_ARI_TAGALIAS_READ_PERMILLIAGE", "fuzz.ari.tagalias.read.permillage", nullptr, 0, DeserializeUInt64, SerializeUInt64);
}

// Implementation of: class FuzzARI
namespace Cohestra {

    FuzzARI::FuzzARI(Instance&                    instance,
                     size_t                       index,
                     const std::vector<uint32_t>& type1Upstreams) noexcept
        : Stimulator          ("FuzzARI", "Automated Range Iteration fuzzer (migrated from tl-test-new)", instance, index)
        , type1Upstreams      (type1Upstreams)
        , section             ("stimulator." + std::to_string(index))
        , rng                 ()
        , seed                (CONFIG_FUZZ_ARI_SEED.defaultValue)
        , ariInterval         (CONFIG_FUZZ_ARI_INTERVAL.defaultValue)
        , ariTarget           (CONFIG_FUZZ_ARI_TARGET.defaultValue)
        , startupCycle        (CONFIG_FUZZ_ARI_STARTUP_CYCLE.defaultValue)
        , maximumCycle        (CONFIG_FUZZ_ARI_MAXIMUM_CYCLE.defaultValue)
        , memoryEnable        (CONFIG_FUZZ_ARI_MEMORY_ENABLE.defaultValue)
        , memoryStart         (CONFIG_FUZZ_ARI_MEMORY_START.defaultValue)
        , memoryEnd           (CONFIG_FUZZ_ARI_MEMORY_END.defaultValue)
        , cmoEnable           (CONFIG_FUZZ_ARI_CMO_ENABLE.defaultValue)
        , cmoRatio            (CONFIG_FUZZ_ARI_CMO_RATIO.defaultValue)
        , cmoInvalRatio       (CONFIG_FUZZ_ARI_CMO_INVAL_RATIO.defaultValue)
        , evictEnable         (CONFIG_FUZZ_ARI_EVICT_ENABLE.defaultValue)
        , evictRatio          (CONFIG_FUZZ_ARI_EVICT_RATIO.defaultValue)
        , prefetchEnable      (CONFIG_FUZZ_ARI_PREFETCH_ENABLE.defaultValue)
        , prefetchRatio       (CONFIG_FUZZ_ARI_PREFETCH_RATIO.defaultValue)
        , prefetchLoadEnable  (CONFIG_FUZZ_ARI_PREFETCH_LOAD_ENABLE.defaultValue)
        , prefetchStoreEnable (CONFIG_FUZZ_ARI_PREFETCH_STORE_ENABLE.defaultValue)
        , readOnceEnable      (CONFIG_FUZZ_ARI_READONCE_ENABLE.defaultValue)
        , readOnceRatio       (CONFIG_FUZZ_ARI_READONCE_RATIO.defaultValue)
        , loadEnable          (CONFIG_FUZZ_ARI_LOAD_ENABLE.defaultValue)
        , storeEnable         (CONFIG_FUZZ_ARI_STORE_ENABLE.defaultValue)
        , storeLineEnable     (CONFIG_FUZZ_ARI_STORELINE_ENABLE.defaultValue)
        , cmoCleanEnable      (CONFIG_FUZZ_ARI_CMO_CLEAN_ENABLE.defaultValue)
        , cmoFlushEnable      (CONFIG_FUZZ_ARI_CMO_FLUSH_ENABLE.defaultValue)
        , cmoInvalEnable      (CONFIG_FUZZ_ARI_CMO_INVAL_ENABLE.defaultValue)
        , rangeTag            (CONFIG_FUZZ_ARI_RANGE_TAG.defaultValue)
        , rangeSet            (CONFIG_FUZZ_ARI_RANGE_SET.defaultValue)
        , rangeAlias          (CONFIG_FUZZ_ARI_RANGE_ALIAS.defaultValue)
        , tagaliasPermillage  (CONFIG_FUZZ_ARI_TAGALIAS_PERMILLIAGE.defaultValue)
        , tagaliasReadPermillage (CONFIG_FUZZ_ARI_TAGALIAS_READ_PERMILLIAGE.defaultValue)
        , ranges              ()
        , rangeOrdinal        ()
        , rangeIndex          (0)
        , rangeIterationTime  (0)
        , rangeIterationCount (0)
        , startupInterval     (0)
        , finished            (false)
        , countIssued         (0)
        , countDenied         (0)
        , countIssuedCMO      (0)
        , countDeniedCMO      (0)
        , countIssuedCMOClean (0)
        , countIssuedCMOFlush (0)
        , countIssuedCMOInval (0)
        , countIssuedEVT      (0)
        , countDeniedEVT      (0)
        , countIssuedPF       (0)
        , countDeniedPF       (0)
        , countIssuedPFLoad   (0)
        , countIssuedPFStore  (0)
        , countIssuedTAR      (0)
        , countDeniedTAR      (0)
        , countIssuedRO       (0)
        , countCompletedRO    (0)
        , countDeniedRO       (0)
        , countStoreCommitted (0)
        , countStoreDropped   (0)
        , pendingStores       ()
        , aliasMap            ()
        , evictableLines      ()
    { }

    bool FuzzARI::Initialize(const StringLoaderBase& configSource) noexcept
    {
        bool good = LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_SEED,          seed)          &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_INTERVAL,      ariInterval)   &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_TARGET,        ariTarget)     &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_STARTUP_CYCLE, startupCycle)  &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_MAXIMUM_CYCLE, maximumCycle)  &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_MEMORY_ENABLE, memoryEnable)  &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_MEMORY_START,  memoryStart)   &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_MEMORY_END,    memoryEnd)     &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_CMO_ENABLE,    cmoEnable)     &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_CMO_RATIO,     cmoRatio)      &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_CMO_INVAL_RATIO, cmoInvalRatio) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_EVICT_ENABLE, evictEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_EVICT_RATIO, evictRatio) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_PREFETCH_ENABLE, prefetchEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_PREFETCH_RATIO, prefetchRatio) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_PREFETCH_LOAD_ENABLE, prefetchLoadEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_PREFETCH_STORE_ENABLE, prefetchStoreEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_READONCE_ENABLE, readOnceEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_READONCE_RATIO, readOnceRatio) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_LOAD_ENABLE,   loadEnable)    &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_STORE_ENABLE,  storeEnable)   &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_STORELINE_ENABLE, storeLineEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_CMO_CLEAN_ENABLE, cmoCleanEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_CMO_FLUSH_ENABLE, cmoFlushEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_CMO_INVAL_ENABLE, cmoInvalEnable) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_RANGE_TAG,     rangeTag)      &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_RANGE_SET,     rangeSet)      &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_RANGE_ALIAS,   rangeAlias)    &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_TAGALIAS_PERMILLIAGE, tagaliasPermillage) &&
                    LoadConfigurationEntry(configSource, &CONFIG_FUZZ_ARI_TAGALIAS_READ_PERMILLIAGE, tagaliasReadPermillage);

        if (!good)
            return false;

        if (!ariInterval)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.interval must not be zero.", GetIndex());
            return false;
        }

        if (!cmoRatio)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.cmo.ratio must not be zero.", GetIndex());
            return false;
        }

        if (!cmoInvalRatio)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.cmo.inval.ratio must not be zero.", GetIndex());
            return false;
        }

        if (evictEnable && !evictRatio)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.evict.ratio must not be zero when evict is enabled.", GetIndex());
            return false;
        }

        if (prefetchEnable && !prefetchRatio)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.prefetch.ratio must not be zero when prefetch is enabled.", GetIndex());
            return false;
        }

        if (readOnceEnable && !readOnceRatio)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.readonce.ratio must not be zero when ReadOnce is enabled.", GetIndex());
            return false;
        }

        if (memoryEnd <= memoryStart)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.memory.end ({}) must be greater than fuzz.ari.memory.start ({}).",
                GetIndex(), memoryEnd, memoryStart);
            return false;
        }

        if (!rangeTag || !rangeSet || !rangeAlias)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.range dimensions must not be zero.", GetIndex());
            return false;
        }

        if (tagaliasPermillage > 1000)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.tagalias.permillage ({}) must not exceed 1000.",
                GetIndex(), tagaliasPermillage);
            return false;
        }

        if (tagaliasReadPermillage > 1000)
        {
            GetOwner().GetLogger()->error("FuzzARI {}: fuzz.ari.tagalias.read.permillage ({}) must not exceed 1000.",
                GetIndex(), tagaliasReadPermillage);
            return false;
        }

        // Fuzzing ranges of ARI: element 0 configurable, elements 1 & 2 fixed as tl-test-new
        ranges.clear();
        ranges.push_back({ 0, rangeTag, rangeSet, rangeAlias });
        ranges.push_back({ 1, 0x1,      0x10,     0x4        });
        ranges.push_back({ 2, 0x10,     0x1,      0x4        });

        // seed-dependent initial permutation of the range set
        rangeOrdinal.clear();
        for (size_t i = 0; i < ranges.size(); ++i)
            rangeOrdinal.push_back(i);

        rng.seed(seed + GetIndex());

        size_t loop = static_cast<size_t>(seed % Factorial(rangeOrdinal.size()));
        for (size_t i = 0; i < loop; ++i)
            std::next_permutation(rangeOrdinal.begin(), rangeOrdinal.end());

        rangeIndex          = 0;
        rangeIterationCount = 0;
        rangeIterationTime  = startupCycle + ariInterval;
        startupInterval     = 0;
        finished            = false;

        GetOwner().GetLogger()->info(
            "FuzzARI {}: initialized with seed {}, interval {}, target {}, memory [{}, {}), startup {}, maximum {}.",
            GetIndex(), seed, ariInterval, ariTarget, memoryStart, memoryEnd, startupCycle, maximumCycle);

        // Report disabled sources only (default all-enabled keeps the log quiet
        // so default-config runs stay bit-identical with legacy logs)
        if (!loadEnable || !storeEnable || !storeLineEnable
         || !cmoCleanEnable || !cmoFlushEnable || !cmoInvalEnable
         || !evictEnable
         || (prefetchEnable && (!prefetchLoadEnable || !prefetchStoreEnable)))
        {
            GetOwner().GetLogger()->info(
                "FuzzARI {}: sources disabled: {}{}{}{}{}{}{}{}{}",
                GetIndex(),
                loadEnable      ? "" : "DoLoad(ReadShared) ",
                storeEnable     ? "" : "DoStore(ReadUnique) ",
                storeLineEnable ? "" : "DoStoreLine(MakeUnique) ",
                cmoCleanEnable  ? "" : "DoCBOClean(CleanShared) ",
                cmoFlushEnable  ? "" : "DoCBOFlush(CleanInvalid) ",
                cmoInvalEnable  ? "" : "DoCBOInval(MakeInvalid) ",
                evictEnable     ? "" : "DoEvict(Evict/WriteBackFull) ",
                (prefetchEnable && !prefetchLoadEnable)  ? "DoPrefetchLoad(StashShared) " : "",
                (prefetchEnable && !prefetchStoreEnable) ? "DoPrefetchStore(StashUnique) " : "");
        }

        return true;
    }

    void FuzzARI::Tick(uint64_t time) noexcept
    {
        if (finished)
            return;

        if (startupInterval < startupCycle)
        {
            startupInterval++;
            return;
        }

        if (maximumCycle && time > maximumCycle)
        {
            finished = true;

            GetOwner().GetLogger()->info("FuzzARI {}: maximum cycle {} reached, stopping at time {}. Issued {} (CMO {}: clean {}, flush {}, inval {}; EVT {}; PF {}: load {}, store {}; TAR {}; RO {}, completed {}, denied {}), denied {} (CMO {}, EVT {}, PF {}, TAR {}), stores committed {}, dropped {}.",
                GetIndex(), maximumCycle, time,
                countIssued, countIssuedCMO, countIssuedCMOClean, countIssuedCMOFlush, countIssuedCMOInval, countIssuedEVT,
                countIssuedPF, countIssuedPFLoad, countIssuedPFStore, countIssuedTAR, countIssuedRO, countCompletedRO, countDeniedRO,
                countDenied, countDeniedCMO, countDeniedEVT, countDeniedPF, countDeniedTAR, countStoreCommitted, countStoreDropped);

            return;
        }

        // retire granted store acquisitions: commit their data into the local
        // cache line (dirties lines for writeback/CMO/back-invalidation paths)
        RetirePendingStores(time);

        // one randomized transaction per tick
        RandomTest(time);

        // ARI range iteration
        if (time >= rangeIterationTime)
        {
            rangeIterationTime += ariInterval;
            rangeIndex++;

            if (rangeIndex == rangeOrdinal.size())
            {
                rangeIndex = 0;
                rangeIterationCount++;
                std::next_permutation(rangeOrdinal.begin(), rangeOrdinal.end());
            }

            GetOwner().GetLogger()->info("FuzzARI {}: Fuzz Set switched to range {} (pass {}/{}).",
                GetIndex(), rangeOrdinal[rangeIndex], rangeIterationCount, ariTarget);
        }

        if (rangeIterationCount >= ariTarget)
        {
            finished = true;

            GetOwner().GetLogger()->info("FuzzARI {}: ARI target {} reached at time {}, finished. Issued {} (CMO {}: clean {}, flush {}, inval {}; EVT {}; PF {}: load {}, store {}; TAR {}; RO {}, completed {}, denied {}), denied {} (CMO {}, EVT {}, PF {}, TAR {}), stores committed {}, dropped {}.",
                GetIndex(), ariTarget, time,
                countIssued, countIssuedCMO, countIssuedCMOClean, countIssuedCMOFlush, countIssuedCMOInval, countIssuedEVT,
                countIssuedPF, countIssuedPFLoad, countIssuedPFStore, countIssuedTAR, countIssuedRO, countCompletedRO, countDeniedRO,
                countDenied, countDeniedCMO, countDeniedEVT, countDeniedPF, countDeniedTAR, countStoreCommitted, countStoreDropped);
        }
    }

    bool FuzzARI::IsFinished() const noexcept
    {
        return finished;
    }

    uint64_t FuzzARI::Random64() noexcept
    {
        return rng();
    }

    uint64_t FuzzARI::GenerateAddress(const FuzzARIRange& range, bool doAlias, uint64_t& aliasOut) noexcept
    {
        // Tag + Set + Offset (cache line = 64B, tag stride = 8KB), as tl-test-new CFuzzer
        uint64_t addr = ((Random64() % range.maxTag) << 13)
                      + ((Random64() % range.maxSet) << 6);

        // tl-test-new draws a cache-line alias here; the draw is kept in the RNG
        // sequence for replay fidelity and now also seeds the TagAlias model:
        // RandomTest adopts it as a line's initial alias on first touch
        aliasOut = 0;
        if (doAlias)
            aliasOut = Random64() % range.maxAlias;

        // fold into [memoryStart, memoryEnd)
        addr = addr % (memoryEnd - memoryStart) + memoryStart;

        // align to cache line
        addr &= ~uint64_t(0x3F);

        return addr;
    }

    void FuzzARI::RandomTest(uint64_t time) noexcept
    {
        if (type1Upstreams.empty())
            return;

        const uint32_t upstreamIndex = type1Upstreams[Random64() % type1Upstreams.size()];
        auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

        if (!upstream)
            return;

        // Class availability: a class needs its class switch AND at least one
        // enabled source. With all sources enabled (default) these reduce to
        // the legacy expressions, preserving behavior and the RNG sequence.
        const bool memAvail = memoryEnable
                           && (loadEnable || storeEnable || storeLineEnable);
        const bool cmoAvail = cmoEnable
                           && (cmoCleanEnable || cmoFlushEnable || cmoInvalEnable);
        const bool evtAvail = evictEnable;
        const bool pfAvail  = prefetchEnable
                           && (prefetchLoadEnable || prefetchStoreEnable);
        const bool roAvail  = readOnceEnable;

        if (!memAvail && !cmoAvail && !evtAvail && !pfAvail && !roAvail)
            return;

        // Evict (TXEVT) class with probability 1/evictRatio: evict a line this
        // upstream is known to hold (probe-verified, so the Evict virtually
        // always hits). On no live target, fall through to the regular
        // memory/CMO path - nothing issued, nothing counted. The draw is
        // skipped entirely when evict is disabled, keeping the legacy RNG
        // sequence bit-identical.
        if (evtAvail && (Random64() % evictRatio) == 0 && TryDoEvict(upstreamIndex))
            return;

        // Prefetch (Stash) class with probability 1/prefetchRatio: emit a
        // fire-and-forget StashShared/StashUnique on a random line of the
        // current range (no upstream completion is ever expected). The draw
        // is skipped entirely when prefetch is disabled, keeping the legacy
        // RNG sequence bit-identical.
        if (pfAvail && (Random64() % prefetchRatio) == 0 && TryDoPrefetch(upstreamIndex))
            return;

        // ReadOnce class with probability 1/readOnceRatio. The draw is skipped
        // only when explicitly disabled, preserving opt-out legacy sequences.
        if (roAvail && (Random64() % readOnceRatio) == 0 && TryDoReadOnce(upstreamIndex))
            return;

        // Evict-only configuration (memory and CMO both off) with a missed
        // draw or no live target: nothing else to issue this tick
        if (!memAvail && !cmoAvail)
            return;

        // TagAlias-conflicting read with permillage probability: re-read a line
        // this upstream already holds under a *different* alias - a real VIPT
        // L1 misses in the second set on such an access, so the read reaches
        // the wire and the home must snoop the old-alias copy before granting
        // (nested alias snoop). On no live target, fall through to the regular
        // memory/CMO path - nothing issued, nothing counted. The draw is
        // skipped entirely when the knob is 0, keeping the legacy RNG sequence
        // bit-identical.
        if (tagaliasReadPermillage && memAvail
         && (Random64() % 1000) < tagaliasReadPermillage
         && TryDoAliasRead(upstreamIndex))
            return;

        const FuzzARIRange& range = ranges[rangeOrdinal[rangeIndex]];

        uint64_t aliasDraw;
        uint64_t addr = GenerateAddress(range, true, aliasDraw);

        // CMO operation with probability 1/cmoRatio when both classes are enabled
        bool doMemory;

        if (memAvail && cmoAvail)
            doMemory = (Random64() % cmoRatio) != 0;
        else
            doMemory = memAvail;

        if (doMemory)
        {
            // Resolve the TagAlias for this access (VIPT alias of the VA that
            // produced it). A line keeps its alias once this upstream assigned
            // it; a remap to a *different* alias happens only at the configured
            // permillage rate, so alias snooping stays a rare event and
            // same-alias sequences (e.g. S->U store-after-load) dominate.
            // Disabled at permillage 0: no map access, no extra RNG draws.
            uint64_t tagAlias = 0;

            if (tagaliasPermillage && upstreamIndex < aliasMap.size())
            {
                auto&          aliases = aliasMap[upstreamIndex];
                const uint64_t line    = addr >> 6;
                auto           it      = aliases.find(line);

                if (it == aliases.end())
                {
                    // first touch of this line by this upstream: adopt the draw
                    if (aliases.size() >= ALIAS_MAP_LIMIT)
                        aliases.clear(); // memory safety valve; aliases re-seed afterwards

                    aliases.emplace(line, static_cast<uint8_t>(aliasDraw));
                    tagAlias = aliasDraw;
                }
                else
                {
                    tagAlias = it->second;

                    const uint64_t r = Random64();
                    if (range.maxAlias > 1 && (r % 1000) < tagaliasPermillage)
                    {
                        tagAlias   = (it->second + 1 + ((r >> 32) % (range.maxAlias - 1))) % range.maxAlias;
                        it->second = static_cast<uint8_t>(tagAlias);
                    }
                }
            }

            // Pick load vs store with the legacy draw, then fall back to the
            // other kind when the picked one is disabled (memAvail guarantees
            // at least one is enabled). Default config: identical selection.
            bool wantLoad = (Random64() % 2) != 0;

            if (wantLoad && !loadEnable)
                wantLoad = false;
            else if (!wantLoad && !storeEnable && !storeLineEnable)
                wantLoad = true;

            if (wantLoad)
            {
                // Acquire -> Load
                auto result = upstream->DoLoad(addr, tagAlias);

                CountResult(result);

                if (result && !result->IsRejected())
                    TrackEvictableLine(upstreamIndex, addr);
            }
            else
            {
                // Release -> Store (2/3 DoStoreLine, 1/3 DoStore for opcode
                // variety); the data commit into the local cache line is
                // deferred to RetirePendingStores and always dirties the
                // whole line with fresh random data. Disabled store kinds
                // fall back to the enabled one.
                bool partial = (Random64() % 3) == 0;

                if (partial && !storeEnable)
                    partial = false;
                else if (!partial && !storeLineEnable)
                    partial = true;

                auto result = partial ? upstream->DoStore(addr, tagAlias)
                                      : upstream->DoStoreLine(addr, tagAlias);

                CountResult(result);

                if (result && !result->IsRejected())
                {
                    QueuePendingStore(upstreamIndex, addr, time);
                    TrackEvictableLine(upstreamIndex, addr);
                }
            }
        }
        else
        {
            // CMO: real CBO transactions (CleanShared/CleanInvalid/MakeInvalid -> CompCMO).
            // MakeInvalid (CBOInval) is down-weighted to 1/cmoInvalRatio of CMOs -
            // together with CleanInvalid it removes the directory entry at the home,
            // so keeping both rare lets the directory fill and exercises the
            // back-invalidation (victim) flow.
            auto countCMO = [this](const auto& result, uint64_t& issuedKind) noexcept {
                if (result && !result->IsRejected())
                {
                    countIssued++;
                    countIssuedCMO++;
                    issuedKind++;
                }
                else
                {
                    countDenied++;
                    countDeniedCMO++;
                }
            };

            // CMO kind selection with per-source fallbacks (cmoAvail guarantees
            // at least one CMO source is enabled). Default config: identical
            // draws and selection.
            const bool pickInval = (Random64() % cmoInvalRatio) == 0;

            if ((pickInval && cmoInvalEnable)
             || (!cmoCleanEnable && !cmoFlushEnable))
            {
                // inval picked and enabled, or inval is the only enabled CMO
                countCMO(upstream->DoCBOInval(addr), countIssuedCMOInval);
            }
            else
            {
                // clean/flush split; reaching here guarantees at least one of
                // the two is enabled, so the single-step fallback always
                // lands on an enabled source
                bool pickClean = (Random64() % 2) != 0;

                if (pickClean && !cmoCleanEnable)
                    pickClean = false;
                else if (!pickClean && !cmoFlushEnable)
                    pickClean = true;

                if (pickClean)
                    countCMO(upstream->DoCBOClean(addr), countIssuedCMOClean);
                else
                    countCMO(upstream->DoCBOFlush(addr), countIssuedCMOFlush);
            }
        }
    }

    void FuzzARI::QueuePendingStore(uint32_t upstreamIndex, uint64_t pa, uint64_t time) noexcept
    {
        if (pendingStores.size() >= PENDING_STORE_LIMIT)
        {
            // the acquisition still happened; only the data commit is skipped
            countStoreDropped++;
            return;
        }

        FuzzARIPendingStore& slot = pendingStores.emplace_back();

        slot.upstreamIndex = upstreamIndex;
        slot.pa            = pa;
        slot.expiryTime    = time + 10000;

        for (size_t i = 0; i < 8; ++i)
            slot.data[i] = Random64();
    }

    void FuzzARI::RetirePendingStores(uint64_t time) noexcept
    {
        for (auto it = pendingStores.begin(); it != pendingStores.end();)
        {
            auto* upstream = GetOwner().GetType1Upstream(it->upstreamIndex);

            auto line = upstream ? upstream->GetCacheLine(it->pa) : nullptr;

            if (!line)
            {
                // line reaped (invalidated) before the commit landed
                countStoreDropped++;
                it = pendingStores.erase(it);
                continue;
            }

            if (line->Store(std::span<const uint64_t, 8>(it->data)))
            {
                countStoreCommitted++;
                it = pendingStores.erase(it);
                continue;
            }

            // commit refused: retry while the line is still owned (a trailing
            // fill beat may still be in flight); give up once ownership was
            // lost (snooped away) or the slot expired
            const auto state = line->GetState();

            if ((state != CCHI::Taurus::CacheState::UniqueClean
              && state != CCHI::Taurus::CacheState::UniqueDirty)
             || time >= it->expiryTime)
            {
                countStoreDropped++;
                it = pendingStores.erase(it);
                continue;
            }

            ++it;
        }
    }

    void FuzzARI::TrackEvictableLine(uint32_t upstreamIndex, uint64_t addr) noexcept
    {
        if (!evictEnable || upstreamIndex >= evictableLines.size())
            return;

        auto& lines = evictableLines[upstreamIndex];

        if (lines.size() >= EVICTABLE_LINE_LIMIT)
            lines[Random64() % lines.size()] = addr; // random replacement keeps the set fresh
        else
            lines.push_back(addr);
    }

    bool FuzzARI::TryDoEvict(uint32_t upstreamIndex) noexcept
    {
        if (upstreamIndex >= evictableLines.size())
            return false;

        auto& lines = evictableLines[upstreamIndex];

        if (lines.empty())
            return false;

        auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

        if (!upstream)
            return false;

        // Probe a few tracked lines and evict the first one the upstream still
        // holds - targeting known-present lines converts the would-be
        // REJECTED_TAURUS_EVICT_MISS denial into a guaranteed hit. Stale
        // entries (snooped away or reaped since tracking) are just skipped.
        for (size_t attempt = 0; attempt < EVICT_PROBE_ATTEMPTS; ++attempt)
        {
            const size_t   pick = Random64() % lines.size();
            const uint64_t pa   = lines[pick];

            auto line = upstream->GetCacheLine(pa);

            if (!line || line->GetState() == CCHI::Taurus::CacheState::Invalid)
                continue;

            auto result = upstream->DoEvict(pa);

            if (result && !result->IsRejected())
            {
                countIssued++;
                countIssuedEVT++;

                // the accepted evict demoted the line to Invalid: drop it
                lines[pick] = lines.back();
                lines.pop_back();
            }
            else
            {
                countDenied++;
                countDeniedEVT++;
            }

            return true;
        }

        return false;
    }

    bool FuzzARI::TryDoPrefetch(uint32_t upstreamIndex) noexcept
    {
        auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

        if (!upstream)
            return false;

        const FuzzARIRange& range = ranges[rangeOrdinal[rangeIndex]];

        // Own address draw (tag/set/alias, same shape as the memory-op draw).
        // The Taurus prefetch API carries no TagAlias, so the alias draw is
        // discarded; it is kept for draw-shape uniformity only.
        uint64_t aliasDummy;
        const uint64_t addr = GenerateAddress(range, true, aliasDummy);

        // 50/50 StashShared (prefetch load) vs StashUnique (prefetch store)
        // with per-source fallback; pfAvail guarantees at least one enabled.
        bool wantLoad = (Random64() % 2) != 0;

        if (wantLoad && !prefetchLoadEnable)
            wantLoad = false;
        else if (!wantLoad && !prefetchStoreEnable)
            wantLoad = true;

        auto result = wantLoad ? upstream->DoPrefetchLoad(addr)
                               : upstream->DoPrefetchStore(addr);

        if (result && !result->IsRejected())
        {
            countIssued++;
            countIssuedPF++;
            (wantLoad ? countIssuedPFLoad : countIssuedPFStore)++;
        }
        else
        {
            countDenied++;
            countDeniedPF++;
        }

        return true;
    }

    bool FuzzARI::TryDoReadOnce(uint32_t upstreamIndex) noexcept
    {
        auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

        if (!upstream)
            return false;

        const FuzzARIRange& range = ranges[rangeOrdinal[rangeIndex]];

        uint64_t aliasDummy;
        const uint64_t addr = GenerateAddress(range, true, aliasDummy);

        // ReadOnce must start from Invalid. Peer upstreams may still hold the
        // line, so this does not suppress the required coherence snoops.
        if (upstream->GetState(addr) != CCHI::Taurus::CacheState::Invalid)
            return false;

        auto result = upstream->DoLoadThrough(addr);

        if (result && !result->IsRejected())
        {
            countIssued++;
            countIssuedRO++;

            result->BindFuture([this](const auto& event) noexcept {
                if (event.GetData())
                    countCompletedRO++;
                else
                    countDeniedRO++;
            });
        }
        else
        {
            countDenied++;
            countDeniedRO++;
        }

        return true;
    }

    bool FuzzARI::TryDoAliasRead(uint32_t upstreamIndex) noexcept
    {
        // no TagAlias on the wire: a different-alias access is a plain local
        // hit, so the whole stimulus is a no-op in width-0 builds
        if constexpr (!CCHI::Flits::REQ<CommonFlitConfigurationType1>::hasTagAlias)
        {
            return false;
        }
        else
        {
            if (upstreamIndex >= evictableLines.size() || upstreamIndex >= aliasMap.size())
                return false;

            auto& lines = evictableLines[upstreamIndex];

            if (lines.empty())
                return false;

            auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

            if (!upstream)
                return false;

            const FuzzARIRange& range = ranges[rangeOrdinal[rangeIndex]];

            if (range.maxAlias < 2)
                return false;

            auto& aliases = aliasMap[upstreamIndex];

            // Probe a few tracked lines and re-read the first one the upstream
            // still holds in Shared state under a *different* alias. Shared-only:
            // the Taurus alias-miss path issues ReadShared, a legal issue from
            // Shared (initials I_SC in the xact state model) but not from
            // Unique, so Unique-held lines keep their local-hit fast path.
            // Stale pool entries (snooped away or reaped since tracking) are
            // just skipped.
            for (size_t attempt = 0; attempt < EVICT_PROBE_ATTEMPTS; ++attempt)
            {
                const uint64_t pa = lines[Random64() % lines.size()];

                auto line = upstream->GetCacheLine(pa);

                if (!line || line->GetState() != CCHI::Taurus::CacheState::Shared)
                    continue;

                auto it = aliases.find(pa >> 6);

                if (it == aliases.end())
                    continue;   // current alias unknown (map safety valve fired)

                const uint64_t newAlias = (it->second + 1 + (Random64() % (range.maxAlias - 1)))
                                        % range.maxAlias;

                auto result = upstream->DoLoad(pa, newAlias);

                if (result && !result->IsRejected())
                {
                    countIssued++;
                    countIssuedTAR++;

                    it->second = static_cast<uint8_t>(newAlias);
                }
                else
                {
                    countDenied++;
                    countDeniedTAR++;
                }

                return true;
            }

            return false;
        }
    }

    size_t FuzzARI::Factorial(size_t n) noexcept
    {
        size_t result = 1;

        for (size_t i = 2; i <= n; ++i)
            result *= i;

        return result;
    }
}

// Implementation of: class FuzzARIFactory
namespace Cohestra {

    FuzzARIFactory::FuzzARIFactory() noexcept
        : StimulatorFactory("FuzzARI", "Automated Range Iteration fuzzer (migrated from tl-test-new)")
    { }

    std::unique_ptr<Stimulator> FuzzARIFactory::Create(Instance&                    instance,
                                                       size_t                       index,
                                                       const std::vector<uint32_t>& type1Upstreams) noexcept
    {
        return std::make_unique<FuzzARI>(instance, index, type1Upstreams);
    }
}
