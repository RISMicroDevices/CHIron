#include "cohestra_monitor.hpp"

#include <algorithm>
#include <cstdio>
#include <span>
#include <utility>


// File-local helpers
namespace {

    inline uint8_t GetByte(const std::array<uint64_t, 8>& data, size_t byteOffset) noexcept
    {
        return uint8_t(data[byteOffset >> 3] >> ((byteOffset & 7) * 8));
    }

    inline uint8_t GetByte(const std::span<const uint64_t, 8>& data, size_t byteOffset) noexcept
    {
        return uint8_t(data[byteOffset >> 3] >> ((byteOffset & 7) * 8));
    }

    inline void SetByte(std::array<uint64_t, 8>& data, size_t byteOffset, uint8_t value) noexcept
    {
        uint64_t&       lane = data[byteOffset >> 3];
        const uint64_t  mask = uint64_t(0xFF) << ((byteOffset & 7) * 8);

        lane = (lane & ~mask) | ((uint64_t(value) << ((byteOffset & 7) * 8)) & mask);
    }

    inline uint64_t CountSetBits(uint64_t mask) noexcept
    {
        uint64_t count = 0;

        for (; mask; mask &= mask - 1)
            ++count;

        return count;
    }

    // event offsets are element indices, not byte offsets
    inline std::pair<size_t, size_t> DecodeRange(
        CCHI::Taurus::UpstreamNodeCacheLineLoadEventBase<Cohestra::CommonFlitConfigurationType1>::LoadType type,
        size_t offset) noexcept
    {
        using LoadType = CCHI::Taurus::UpstreamNodeCacheLineLoadEventBase<Cohestra::CommonFlitConfigurationType1>::LoadType;

        switch (type)
        {
        case LoadType::LOAD_LINE:   return { 0,         64 };
        case LoadType::LOAD_64:     return { offset * 8, 8  };
        case LoadType::LOAD_32:     return { offset * 4, 4  };
        case LoadType::LOAD_16:     return { offset * 2, 2  };
        case LoadType::LOAD_8:      return { offset,     1  };
        }

        return { 0, 0 };
    }

    inline std::pair<size_t, size_t> DecodeRange(
        CCHI::Taurus::UpstreamNodeCacheLineStoreEventBase<Cohestra::CommonFlitConfigurationType1>::StoreType type,
        size_t offset) noexcept
    {
        using StoreType = CCHI::Taurus::UpstreamNodeCacheLineStoreEventBase<Cohestra::CommonFlitConfigurationType1>::StoreType;

        switch (type)
        {
        case StoreType::STORE_LINE: return { 0,         64 };
        case StoreType::STORE_64:   return { offset * 8, 8  };
        case StoreType::STORE_32:   return { offset * 4, 4  };
        case StoreType::STORE_16:   return { offset * 2, 2  };
        case StoreType::STORE_8:    return { offset,     1  };
        }

        return { 0, 0 };
    }
}


// Implementation of: ToString helpers
namespace Cohestra {

    const char* ToString(MonitorDataSource source) noexcept
    {
        switch (source)
        {
        case MonitorDataSource::STORE:          return "STORE";
        case MonitorDataSource::GRANT_CAPTURE:  return "GRANT_CAPTURE";
        case MonitorDataSource::ASSUMED_ZERO:   return "ASSUMED_ZERO";
        case MonitorDataSource::CMO_INVALIDATE: return "CMO_INVALIDATE";
        case MonitorDataSource::MAKE_UNIQUE:    return "MAKE_UNIQUE";
        case MonitorDataSource::SNP_INVALIDATE: return "SNP_INVALIDATE";
        }

        return "UNKNOWN";
    }

    const char* ToString(MonitorCheckKind kind) noexcept
    {
        switch (kind)
        {
        case MonitorCheckKind::GRANT_DATA:      return "GRANT_DATA";
        case MonitorCheckKind::GRANT_DATALESS:  return "GRANT_DATALESS";
        case MonitorCheckKind::LOAD:            return "LOAD";
        }

        return "UNKNOWN";
    }
}


// Implementation of: class MonitorMismatchEvent
namespace Cohestra {

    MonitorMismatchEvent::MonitorMismatchEvent(CacheLineDataMonitor& monitor, const MonitorMismatch& mismatch) noexcept
        : monitor   (monitor)
        , mismatch  (mismatch)
    { }

    CacheLineDataMonitor& MonitorMismatchEvent::GetMonitor() noexcept
    {
        return monitor;
    }

    const CacheLineDataMonitor& MonitorMismatchEvent::GetMonitor() const noexcept
    {
        return monitor;
    }

    const MonitorMismatch& MonitorMismatchEvent::GetMismatch() const noexcept
    {
        return mismatch;
    }
}


// Implementation of: class CacheLineDataMonitor
namespace Cohestra {

    const ConfigurationEntryBack<bool> CacheLineDataMonitor::MONITOR_ENABLE
        ("MONITOR_ENABLE", "enable", "monitor", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> CacheLineDataMonitor::MONITOR_FAIL_ON_MISMATCH
        ("MONITOR_FAIL_ON_MISMATCH", "fail.on.mismatch", "monitor", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> CacheLineDataMonitor::MONITOR_ASSUME_ZERO_INIT
        ("MONITOR_ASSUME_ZERO_INIT", "assume.zero.init", "monitor", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> CacheLineDataMonitor::MONITOR_TRACE
        ("MONITOR_TRACE", "trace", "monitor", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> CacheLineDataMonitor::MONITOR_HISTORY_LENGTH
        ("MONITOR_HISTORY_LENGTH", "history.length", "monitor", 4, DeserializeUInt64, SerializeUInt64);


    CacheLineDataMonitor::CacheLineDataMonitor(Instance& instance) noexcept
        : instance  (instance)
    { }

    CacheLineDataMonitor::~CacheLineDataMonitor() noexcept
    {
        Detach();
    }

    bool CacheLineDataMonitor::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        return LoadConfigurationEntry(loader, &MONITOR_ENABLE,            enabled)
            && LoadConfigurationEntry(loader, &MONITOR_FAIL_ON_MISMATCH,  failOnMismatch)
            && LoadConfigurationEntry(loader, &MONITOR_ASSUME_ZERO_INIT,  assumeZeroInit)
            && LoadConfigurationEntry(loader, &MONITOR_TRACE,             trace)
            && LoadConfigurationEntry(loader, &MONITOR_HISTORY_LENGTH,    historyLength);
    }

    void CacheLineDataMonitor::Attach() noexcept
    {
        if (attached)
            return;

        aliasRecords.clear();
        aliasRecords.resize(instance.GetType1UpstreamCount());

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream || !upstream->events)
                continue;

            // a harness may share one event hub between all upstream nodes;
            // register only once per hub
            if (std::find(attachedHubs.begin(), attachedHubs.end(), upstream->events) != attachedHubs.end())
                continue;

            upstream->events->OnCacheLineGranted.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &CacheLineDataMonitor::OnGranted, this));
            upstream->events->OnCacheLinePostLoad.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &CacheLineDataMonitor::OnPostLoad, this));
            upstream->events->OnCacheLinePostStore.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &CacheLineDataMonitor::OnPostStore, this));
            upstream->events->OnAcceptedCompCMO.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &CacheLineDataMonitor::OnAcceptedCompCMO, this));
            upstream->events->OnSNPCacheStatePostDemotion.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &CacheLineDataMonitor::OnSNPDemotion, this));
            upstream->events->OnEVTCacheStatePostDemotion.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &CacheLineDataMonitor::OnEVTDemotion, this));

            attachedHubs.push_back(upstream->events);
        }

        attached = true;
    }

    void CacheLineDataMonitor::Detach() noexcept
    {
        for (auto& hub : attachedHubs)
        {
            hub->OnCacheLineGranted.Unregister(LISTENER_NAME);
            hub->OnCacheLinePostLoad.Unregister(LISTENER_NAME);
            hub->OnCacheLinePostStore.Unregister(LISTENER_NAME);
            hub->OnAcceptedCompCMO.Unregister(LISTENER_NAME);
            hub->OnSNPCacheStatePostDemotion.Unregister(LISTENER_NAME);
            hub->OnEVTCacheStatePostDemotion.Unregister(LISTENER_NAME);
        }

        attachedHubs.clear();
        attached = false;
    }

    bool CacheLineDataMonitor::IsAttached() const noexcept
    {
        return attached;
    }

    bool CacheLineDataMonitor::IsEnabled() const noexcept
    {
        return enabled;
    }

    CacheLineDataMonitor::ScoreboardLine& CacheLineDataMonitor::RequireLine(uint64_t pa) noexcept
    {
        auto [iter, inserted] = scoreboard.try_emplace(pa);

        if (inserted)
        {
            iter->second.sourceUpstreams.fill(NO_UPSTREAM);

            if (assumeZeroInit)
            {
                // every byte starts out expected-zero
                iter->second.validMask = ~uint64_t(0);
                iter->second.sources.fill(uint8_t(MonitorDataSource::ASSUMED_ZERO));
            }
        }

        return iter->second;
    }

    void CacheLineDataMonitor::AppendHistory(ScoreboardLine&   line,
                                             MonitorDataSource source,
                                             size_t            upstreamIndex,
                                             size_t            rangeOffset,
                                             size_t            rangeSize,
                                             uint64_t          appliedMask) noexcept
    {
        if (!historyLength)
            return;

        MonitorHistoryEntry entry;

        entry.sequence      = sequence;
        entry.source        = source;
        entry.upstreamIndex = upstreamIndex;
        entry.rangeOffset   = rangeOffset;
        entry.rangeSize     = rangeSize;
        entry.appliedMask   = appliedMask;
        entry.image         = line.data;

        line.history.push_back(entry);

        while (line.history.size() > historyLength)
            line.history.pop_front();
    }

    void CacheLineDataMonitor::ResolveProvenance(const ScoreboardLine& line,
                                                 size_t              byteOffset,
                                                 MonitorMismatch&    mismatch) noexcept
    {
        mismatch.source               = MonitorDataSource(line.sources[byteOffset]);
        mismatch.sourceUpstreamIndex  = line.sourceUpstreams[byteOffset] == NO_UPSTREAM
                                      ? UNKNOWN_UPSTREAM : size_t(line.sourceUpstreams[byteOffset]);
        mismatch.sourceSequence       = 0;

        // the producing update is the latest history entry that wrote this byte
        for (auto iter = line.history.rbegin(); iter != line.history.rend(); ++iter)
        {
            if (iter->appliedMask & (uint64_t(1) << byteOffset))
            {
                mismatch.sourceSequence = iter->sequence;
                break;
            }
        }
    }

    void CacheLineDataMonitor::RecordMismatch(MonitorCheckKind      check,
                                              uint64_t              pa,
                                              size_t                byteOffset,
                                              uint8_t               expected,
                                              uint8_t               actual,
                                              size_t                upstreamIndex,
                                              const ScoreboardLine& line) noexcept
    {
        MonitorMismatch mismatch;

        mismatch.check          = check;
        mismatch.pa             = pa;
        mismatch.byteOffset     = byteOffset;
        mismatch.expected       = expected;
        mismatch.actual         = actual;
        mismatch.upstreamIndex  = upstreamIndex;
        mismatch.sequence       = sequence;

        ResolveProvenance(line, byteOffset, mismatch);

        ++countMismatches;
        ++countMismatchesByCheck[size_t(check)];

        instance.GetLogger()->error(
            "Cache-line data mismatch [{}]: PA {:#x} byte {}: expected {:#04x} ({} by upstream {} at seq {}), actual {:#04x}, observed by upstream {} (seq {}).",
            ToString(check),
            pa,
            byteOffset,
            unsigned(expected),
            ToString(mismatch.source),
            mismatch.sourceUpstreamIndex == UNKNOWN_UPSTREAM ? "unknown" : std::to_string(mismatch.sourceUpstreamIndex),
            mismatch.sourceSequence,
            unsigned(actual),
            upstreamIndex,
            sequence);

        if (mismatches.size() < MISMATCH_RECORD_LIMIT)
            mismatches.push_back(mismatch);

        if (events)
            events->OnMismatch(*this, mismatch);

        if (failOnMismatch && instance.IsAlive())
        {
            char message[256];

            std::snprintf(message, sizeof(message),
                "Cache-line data mismatch [%s]: PA 0x%llx byte %zu: expected 0x%02x, actual 0x%02x, observed by upstream %zu.",
                ToString(check),
                static_cast<unsigned long long>(pa),
                byteOffset,
                static_cast<unsigned>(expected),
                static_cast<unsigned>(actual),
                upstreamIndex);

            instance.SetFailed({ message });
        }
    }

    void CacheLineDataMonitor::OnGranted(GrantedEvent& event) noexcept
    {
        ++sequence;

        const uint64_t  pa              = event.GetPA();
        const size_t    upstreamIndex   = instance.GetType1UpstreamIndex(&event.GetUpstream())
                                              .value_or(UNKNOWN_UPSTREAM);

        // a grant delivers data only for an allocating read that collected all
        // CompData beats; a dataless (MakeUnique / Comp-only ReadUnique) grant
        // delivers nothing and the persisted local image is re-verified instead
        bool dataCarrying = false;

        if (auto xaction = event.GetXaction())
        {
            if (xaction->GetType() == CCHI::Xact::XactionType::CacheableAllocatingRead)
            {
                auto& read = static_cast<CCHI::Xact::XactionCacheableAllocatingRead<CommonFlitConfigurationType1>&>(
                    *xaction);

                dataCarrying = read.GotAllCompData();
            }
        }

        const auto span = event.GetCacheLine().GetData();

        if (dataCarrying)
        {
            ++countGrantData;

            ScoreboardLine& line = RequireLine(pa);

            uint64_t appliedMask = 0;

            for (size_t byteOffset = 0; byteOffset < 64; ++byteOffset)
            {
                const uint8_t actual = GetByte(span, byteOffset);

                if (line.validMask & (uint64_t(1) << byteOffset))
                {
                    // earliest verification point: the whole DUT-delivered image
                    const uint8_t expected = GetByte(line.data, byteOffset);

                    if (expected != actual)
                    {
                        RecordMismatch(MonitorCheckKind::GRANT_DATA,
                            pa, byteOffset, expected, actual, upstreamIndex, line);

                        // adopt the freshly delivered byte to report the same
                        // divergence only once
                        SetByte(line.data, byteOffset, actual);
                        line.sources[byteOffset]         = uint8_t(MonitorDataSource::GRANT_CAPTURE);
                        line.sourceUpstreams[byteOffset] = upstreamIndex <= NO_UPSTREAM
                                                         ? uint8_t(upstreamIndex) : NO_UPSTREAM;
                        appliedMask |= uint64_t(1) << byteOffset;
                    }
                }
                else
                {
                    // initial capture of a not-yet-known byte
                    SetByte(line.data, byteOffset, actual);
                    line.validMask                  |= uint64_t(1) << byteOffset;
                    line.sources[byteOffset]         = uint8_t(MonitorDataSource::GRANT_CAPTURE);
                    line.sourceUpstreams[byteOffset] = upstreamIndex <= NO_UPSTREAM
                                                     ? uint8_t(upstreamIndex) : NO_UPSTREAM;
                    appliedMask                     |= uint64_t(1) << byteOffset;

                    ++countGrantBytesCaptured;
                }
            }

            if (appliedMask)
                AppendHistory(line, MonitorDataSource::GRANT_CAPTURE, upstreamIndex, 0, 64, appliedMask);

            if (trace)
                instance.GetLogger()->debug(
                    "Cache-line data monitor: grant (data) by upstream {} on PA {:#x}, {} byte(s) captured.",
                    upstreamIndex, pa, CountSetBits(appliedMask));
        }
        else
        {
            ++countGrantDataless;

            // A dataless grant of the MakeUnique class (CacheableDataless, or a
            // store hit carrying no xaction) transfers no data by definition and
            // invalidates every other copy without collecting it: any dirty data
            // elsewhere is architecturally dead (full-overwrite intent) and,
            // until the requester's overwrite lands, the line's value is
            // undefined - one home may drop the dirty data (SnpMakeInvalid),
            // another may write it back and later serve it. The expectation is
            // therefore DROPPED (bytes become not-yet-known and are re-learned
            // on the next defining event), not verified and not adopted. Only an
            // allocating read answered without data (ReadUnique from Shared via
            // Comp) keeps the old expectation: the requester's clean Shared copy
            // is coherence-equal to it by construction, so it is verified.
            bool requesterImageUndefined = true;

            if (auto xaction = event.GetXaction())
                requesterImageUndefined =
                    xaction->GetType() != CCHI::Xact::XactionType::CacheableAllocatingRead;

            if (requesterImageUndefined)
            {
                auto iter = scoreboard.find(pa);

                if (iter != scoreboard.end() && iter->second.validMask)
                    DropExpectation(pa, MonitorDataSource::MAKE_UNIQUE, upstreamIndex);

                if (trace)
                    instance.GetLogger()->debug(
                        "Cache-line data monitor: grant (dataless, expectation dropped) by upstream {} on PA {:#x}.",
                        upstreamIndex, pa);
            }
            else
            {
                auto iter = scoreboard.find(pa);

                if (iter != scoreboard.end())
                {
                    const ScoreboardLine& line = iter->second;

                    for (size_t byteOffset = 0; byteOffset < 64; ++byteOffset)
                    {
                        if (!(line.validMask & (uint64_t(1) << byteOffset)))
                            continue;

                        const uint8_t expected = GetByte(line.data, byteOffset);
                        const uint8_t actual   = GetByte(span, byteOffset);

                        if (expected != actual)
                            RecordMismatch(MonitorCheckKind::GRANT_DATALESS,
                                pa, byteOffset, expected, actual, upstreamIndex, line);
                    }
                }

                if (trace)
                    instance.GetLogger()->debug(
                        "Cache-line data monitor: grant (dataless) by upstream {} on PA {:#x}.",
                        upstreamIndex, pa);
            }
        }

        // TagAlias conflict check: a grant must never land while this node
        // still holds the same PA under a *different* alias (no invalidation
        // observed in between) - that leaves the L1 physically holding two
        // cache lines with the same PA but different TagAlias. A correct DUT
        // snoops the old-alias copy first (observed as a demotion to Invalid,
        // which drops the record), so a conflict here is always a genuine DUT
        // coherence bug, never a race artifact. Grants carrying no xaction are
        // local hit-store completions with no wire traffic: nothing to check.
        if constexpr (CCHI::Flits::REQ<CommonFlitConfigurationType1>::hasTagAlias)
        {
            if (auto xaction = event.GetXaction())
            {
                const uint8_t grantedAlias = static_cast<uint8_t>(xaction->GetFirst().flit.req.TagAlias);

                if (upstreamIndex < aliasRecords.size())
                {
                    auto& records = aliasRecords[upstreamIndex];
                    auto  iter    = records.find(pa);

                    if (iter != records.end() && iter->second != grantedAlias)
                        RecordAliasConflict(pa, upstreamIndex, iter->second, grantedAlias);

                    records[pa] = grantedAlias;
                }
            }
        }
    }

    void CacheLineDataMonitor::OnPostLoad(PostLoadEvent& event) noexcept
    {
        ++sequence;

        const uint64_t  pa              = event.GetPA();
        const size_t    upstreamIndex   = instance.GetType1UpstreamIndex(&event.GetUpstream())
                                              .value_or(UNKNOWN_UPSTREAM);

        const auto [rangeOffset, rangeSize] = DecodeRange(event.GetLoadType(), event.GetLoadOffset());

        ++countLoads;

        auto iter = scoreboard.find(pa);

        if (iter == scoreboard.end())
            return;

        const ScoreboardLine&   line = iter->second;
        const auto              span = event.GetCacheLine().GetData();

        uint64_t bytesChecked = 0;

        for (size_t byteOffset = rangeOffset; byteOffset < rangeOffset + rangeSize; ++byteOffset)
        {
            if (!(line.validMask & (uint64_t(1) << byteOffset)))
                continue;

            const uint8_t expected = GetByte(line.data, byteOffset);
            const uint8_t actual   = GetByte(span, byteOffset);

            if (expected != actual)
                RecordMismatch(MonitorCheckKind::LOAD,
                    pa, byteOffset, expected, actual, upstreamIndex, line);

            ++bytesChecked;
        }

        countLoadBytesChecked += bytesChecked;

        if (trace)
            instance.GetLogger()->debug(
                "Cache-line data monitor: load by upstream {} on PA {:#x} range [{}, {}), {} byte(s) checked.",
                upstreamIndex, pa, rangeOffset, rangeOffset + rangeSize, bytesChecked);
    }

    void CacheLineDataMonitor::OnPostStore(PostStoreEvent& event) noexcept
    {
        ++sequence;

        const uint64_t  pa              = event.GetPA();
        const size_t    upstreamIndex   = instance.GetType1UpstreamIndex(&event.GetUpstream())
                                              .value_or(UNKNOWN_UPSTREAM);

        const auto [rangeOffset, rangeSize] = DecodeRange(event.GetStoreType(), event.GetStoreOffset());

        const auto span = event.GetCacheLine().GetData();

        ScoreboardLine& line = RequireLine(pa);

        uint64_t appliedMask = 0;

        for (size_t byteOffset = rangeOffset; byteOffset < rangeOffset + rangeSize; ++byteOffset)
        {
            SetByte(line.data, byteOffset, GetByte(span, byteOffset));
            line.validMask                  |= uint64_t(1) << byteOffset;
            line.sources[byteOffset]         = uint8_t(MonitorDataSource::STORE);
            line.sourceUpstreams[byteOffset] = upstreamIndex <= NO_UPSTREAM
                                             ? uint8_t(upstreamIndex) : NO_UPSTREAM;
            appliedMask                     |= uint64_t(1) << byteOffset;
        }

        AppendHistory(line, MonitorDataSource::STORE, upstreamIndex, rangeOffset, rangeSize, appliedMask);

        ++countStores;
        countStoreBytes += rangeSize;

        if (trace)
            instance.GetLogger()->debug(
                "Cache-line data monitor: store by upstream {} on PA {:#x} range [{}, {}).",
                upstreamIndex, pa, rangeOffset, rangeOffset + rangeSize);
    }

    void CacheLineDataMonitor::DropExpectation(uint64_t pa,
                                               MonitorDataSource source,
                                               size_t upstreamIndex) noexcept
    {
        auto iter = scoreboard.find(pa);

        if (iter == scoreboard.end() || !iter->second.validMask)
            return;

        ScoreboardLine& line = iter->second;

        line.validMask = 0;

        AppendHistory(line, source, upstreamIndex, 0, 64, 0);

        if (trace)
            instance.GetLogger()->debug(
                "Cache-line data monitor: expectation dropped ({}) on PA {:#x} by upstream {}.",
                ToString(source), pa, upstreamIndex);
    }

    void CacheLineDataMonitor::OnAcceptedCompCMO(AcceptedCompCMOEvent& event) noexcept
    {
        auto xaction = event.GetXaction();

        if (!xaction || xaction->GetType() != CCHI::Xact::XactionType::CMO)
            return;

        // Only MakeInvalid (CBOInval) discards dirty data without a writeback:
        // the memory image is no longer the coherent image, so the line's
        // expectation must be dropped (bytes become not-yet-known again and are
        // re-learned on the next data-carrying grant; loads skip them).
        // CleanShared/CleanInvalid (CBOClean/CBOFlush) write dirty data back, so
        // their expectations stay valid and keep verifying the DUT's CMO
        // writeback path.
        if (xaction->GetFirst().flit.req.Opcode != CCHI::Opcodes::REQ::MakeInvalid)
            return;

        ++sequence;

        // DoCBO* line-aligns the request address; mask again defensively
        const uint64_t  pa              = uint64_t(xaction->GetFirst().flit.req.Addr) & ~uint64_t(0x3F);
        const size_t    upstreamIndex   = instance.GetType1UpstreamIndex(&event.GetUpstream())
                                              .value_or(UNKNOWN_UPSTREAM);

        auto iter = scoreboard.find(pa);

        if (iter == scoreboard.end() || !iter->second.validMask)
            return;

        DropExpectation(pa, MonitorDataSource::CMO_INVALIDATE, upstreamIndex);

        // a completed MakeInvalid also ends this node's hold of the line
        if constexpr (CCHI::Flits::REQ<CommonFlitConfigurationType1>::hasTagAlias)
            DropAliasRecord(upstreamIndex, pa);

        ++countCMOInvalidations;
    }

    void CacheLineDataMonitor::OnSNPDemotion(SNPDemotionEvent& event) noexcept
    {
        // a demotion to Invalid ends this node's hold of the line (any opcode,
        // any previous state): drop the held-alias record, so a later grant
        // under any alias is checked as a fresh hold
        if constexpr (CCHI::Flits::REQ<CommonFlitConfigurationType1>::hasTagAlias)
        {
            if (event.GetNextState() == CCHI::Taurus::CacheState::Invalid)
            {
                const size_t aliasUpstreamIndex = instance.GetType1UpstreamIndex(&event.GetUpstream())
                                                       .value_or(UNKNOWN_UPSTREAM);

                DropAliasRecord(aliasUpstreamIndex, event.GetPA());
            }
        }

        // a SnpMakeInvalid that demotes a UniqueDirty line to Invalid discards
        // the dirty image right here - this is the exact point where dirty data
        // dies for both MakeInvalid CMOs and MakeUnique transactions, possibly
        // long before the discarding transaction's grant/CompCMO reaches this
        // monitor; expecting the old image past this point is a false positive
        if (event.GetPrevState() != CCHI::Taurus::CacheState::UniqueDirty
         || event.GetNextState() != CCHI::Taurus::CacheState::Invalid
         || event.GetSNPFlit().Opcode != CCHI::Opcodes::SNP::SnpMakeInvalid)
            return;

        ++sequence;

        const uint64_t  pa              = event.GetPA();
        const size_t    upstreamIndex   = instance.GetType1UpstreamIndex(&event.GetUpstream())
                                              .value_or(UNKNOWN_UPSTREAM);

        auto iter = scoreboard.find(pa);

        if (iter == scoreboard.end() || !iter->second.validMask)
            return;

        DropExpectation(pa, MonitorDataSource::SNP_INVALIDATE, upstreamIndex);

        ++countSNPInvalidations;
    }

    void CacheLineDataMonitor::OnEVTDemotion(EVTDemotionEvent& event) noexcept
    {
        // an Evict/WriteBack demotion to Invalid ends this node's hold of the
        // line: drop the held-alias record
        if constexpr (CCHI::Flits::REQ<CommonFlitConfigurationType1>::hasTagAlias)
        {
            if (event.GetNextState() == CCHI::Taurus::CacheState::Invalid)
            {
                const size_t upstreamIndex = instance.GetType1UpstreamIndex(&event.GetUpstream())
                                                  .value_or(UNKNOWN_UPSTREAM);

                DropAliasRecord(upstreamIndex, event.GetPA());
            }
        }
    }

    void CacheLineDataMonitor::DropAliasRecord(size_t upstreamIndex, uint64_t pa) noexcept
    {
        if (upstreamIndex >= aliasRecords.size())
            return;

        aliasRecords[upstreamIndex].erase(pa);
    }

    void CacheLineDataMonitor::RecordAliasConflict(uint64_t pa,
                                                   size_t   upstreamIndex,
                                                   uint8_t  heldAlias,
                                                   uint8_t  grantedAlias) noexcept
    {
        ++countAliasConflicts;

        instance.GetLogger()->error(
            "TagAlias conflict: PA {:#x}: upstream {} was granted the line under alias {} while still holding it under alias {} (no intervening invalidation, seq {}).",
            pa,
            upstreamIndex,
            unsigned(grantedAlias),
            unsigned(heldAlias),
            sequence);

        if (aliasConflicts.size() < ALIAS_CONFLICT_RECORD_LIMIT)
            aliasConflicts.push_back({ pa, upstreamIndex, heldAlias, grantedAlias, sequence });

        if (failOnMismatch && instance.IsAlive())
        {
            char message[256];

            std::snprintf(message, sizeof(message),
                "TagAlias conflict: L1 (upstream %zu) holds two cache lines with the same PA 0x%llx but different TagAlias (%u held, %u granted).",
                upstreamIndex,
                static_cast<unsigned long long>(pa),
                unsigned(heldAlias),
                unsigned(grantedAlias));

            instance.SetFailed({ message });
        }
    }
}


// Implementation of: class CacheLineDataMonitor (getters and reporting)
namespace Cohestra {

    uint64_t CacheLineDataMonitor::GetLineCount() const noexcept
    {
        return scoreboard.size();
    }

    uint64_t CacheLineDataMonitor::GetGrantDataCount() const noexcept
    {
        return countGrantData;
    }

    uint64_t CacheLineDataMonitor::GetGrantDatalessCount() const noexcept
    {
        return countGrantDataless;
    }

    uint64_t CacheLineDataMonitor::GetGrantBytesCaptured() const noexcept
    {
        return countGrantBytesCaptured;
    }

    uint64_t CacheLineDataMonitor::GetLoadCount() const noexcept
    {
        return countLoads;
    }

    uint64_t CacheLineDataMonitor::GetLoadBytesChecked() const noexcept
    {
        return countLoadBytesChecked;
    }

    uint64_t CacheLineDataMonitor::GetStoreCount() const noexcept
    {
        return countStores;
    }

    uint64_t CacheLineDataMonitor::GetStoreBytes() const noexcept
    {
        return countStoreBytes;
    }

    uint64_t CacheLineDataMonitor::GetCMOInvalidationCount() const noexcept
    {
        return countCMOInvalidations;
    }

    uint64_t CacheLineDataMonitor::GetSNPInvalidationCount() const noexcept
    {
        return countSNPInvalidations;
    }

    uint64_t CacheLineDataMonitor::GetMismatchCount() const noexcept
    {
        return countMismatches;
    }

    uint64_t CacheLineDataMonitor::GetMismatchCount(MonitorCheckKind check) const noexcept
    {
        return countMismatchesByCheck[size_t(check)];
    }

    uint64_t CacheLineDataMonitor::GetAliasConflictCount() const noexcept
    {
        return countAliasConflicts;
    }

    const std::vector<MonitorMismatch>& CacheLineDataMonitor::GetMismatchRecords() const noexcept
    {
        return mismatches;
    }

    const std::vector<MonitorAliasConflict>& CacheLineDataMonitor::GetAliasConflictRecords() const noexcept
    {
        return aliasConflicts;
    }

    std::optional<std::reference_wrapper<const CacheLineDataMonitor::ScoreboardLine>>
        CacheLineDataMonitor::GetLine(uint64_t pa) const noexcept
    {
        auto iter = scoreboard.find(pa);

        if (iter == scoreboard.end())
            return std::nullopt;

        return { std::cref(iter->second) };
    }

    std::optional<std::reference_wrapper<const std::deque<MonitorHistoryEntry>>>
        CacheLineDataMonitor::GetLineHistory(uint64_t pa) const noexcept
    {
        auto iter = scoreboard.find(pa);

        if (iter == scoreboard.end())
            return std::nullopt;

        return { std::cref(iter->second.history) };
    }

    void CacheLineDataMonitor::Report() const noexcept
    {
        auto logger = instance.GetLogger();

        logger->info("Cache-line data monitor report:");
        logger->info(" - lines tracked: {}.", scoreboard.size());
        logger->info(" - grants: {} data-carrying ({} byte(s) captured), {} dataless.",
            countGrantData, countGrantBytesCaptured, countGrantDataless);
        logger->info(" - loads: {} ({} byte(s) checked).", countLoads, countLoadBytesChecked);
        logger->info(" - stores: {} ({} byte(s) committed).", countStores, countStoreBytes);
        logger->info(" - CMO invalidations: {} MakeInvalid (CBOInval) completion(s) dropped line expectations.",
            countCMOInvalidations);
        logger->info(" - SNP invalidations: {} SnpMakeInvalid demotion(s) of dirty lines dropped line expectations.",
            countSNPInvalidations);

        if (!countMismatches)
        {
            logger->info(" - mismatches: 0.");
        }
        else
        {
            logger->error(" - mismatches: {} (grant-data {}, grant-dataless {}, load {}).",
                countMismatches,
                countMismatchesByCheck[size_t(MonitorCheckKind::GRANT_DATA)],
                countMismatchesByCheck[size_t(MonitorCheckKind::GRANT_DATALESS)],
                countMismatchesByCheck[size_t(MonitorCheckKind::LOAD)]);

            size_t shown = 0;

            for (const auto& mismatch : mismatches)
            {
                if (shown >= REPORT_MISMATCH_LIMIT)
                    break;

                logger->error(
                    "   [{}] PA {:#x} byte {}: expected {:#04x} ({} by upstream {} at seq {}), actual {:#04x}, observed by upstream {} (seq {}).",
                    ToString(mismatch.check),
                    mismatch.pa,
                    mismatch.byteOffset,
                    unsigned(mismatch.expected),
                    ToString(mismatch.source),
                    mismatch.sourceUpstreamIndex == UNKNOWN_UPSTREAM ? "unknown" : std::to_string(mismatch.sourceUpstreamIndex),
                    mismatch.sourceSequence,
                    unsigned(mismatch.actual),
                    mismatch.upstreamIndex,
                    mismatch.sequence);

                ++shown;
            }

            if (countMismatches > shown)
                logger->error("   ... and {} more mismatch(es).", countMismatches - shown);
        }

        if (!countAliasConflicts)
        {
            logger->info(" - TagAlias conflicts: 0.");
            return;
        }

        logger->error(" - TagAlias conflicts: {} (L1 granted a line under a new alias while still holding it under the old one).",
            countAliasConflicts);

        size_t shownConflicts = 0;

        for (const auto& conflict : aliasConflicts)
        {
            if (shownConflicts >= REPORT_MISMATCH_LIMIT)
                break;

            logger->error(
                "   PA {:#x}: upstream {} granted under alias {} while holding alias {} (seq {}).",
                conflict.pa,
                conflict.upstreamIndex,
                unsigned(conflict.grantedAlias),
                unsigned(conflict.heldAlias),
                conflict.sequence);

            ++shownConflicts;
        }

        if (countAliasConflicts > shownConflicts)
            logger->error("   ... and {} more TagAlias conflict(s).", countAliasConflicts - shownConflicts);
    }
}
