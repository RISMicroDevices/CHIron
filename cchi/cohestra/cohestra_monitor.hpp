#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cohestra_config.hpp"
#include "cohestra_instance.hpp"

#include "../xact/cchi_xactions.hpp"

#include "../../common/eventbus.hpp"


namespace Cohestra {

    class CacheLineDataMonitor;

    // Data source of an expected scoreboard byte: where the monitor learned the
    // value it checks against. Distinguished so a mismatch report also says how
    // trustworthy the expectation was.
    enum class MonitorDataSource : uint8_t {
        STORE           = 0,    // committed by an observed CacheLine store
        GRANT_CAPTURE   = 1,    // captured from a DUT-delivered fill at a data grant
        ASSUMED_ZERO    = 2,    // expected zero under 'monitor.assume.zero.init'
        CMO_INVALIDATE  = 3,    // expectation dropped on a MakeInvalid (CBOInval) completion:
                                // the CMO architecturally discards dirty data without
                                // writeback, so the memory image is no longer known
        MAKE_UNIQUE     = 4,    // expectation dropped at a MakeUnique-class dataless grant:
                                // MakeUnique transfers no data and discards dirty copies
                                // elsewhere; until the requester's overwrite lands, the
                                // line's value is architecture-undefined (a home may
                                // legitimately drop the dirty data or write it back)
        SNP_INVALIDATE  = 5     // expectation dropped when a SnpMakeInvalid demotes a
                                // UniqueDirty line to Invalid (observed via the Taurus
                                // SNP demotion events): the dirty image dies at this
                                // point, potentially long before the discarding
                                // transaction's grant/CompCMO completes
    };

    const char* ToString(MonitorDataSource source) noexcept;

    // Monitor check that detected a mismatch: the failure source.
    enum class MonitorCheckKind : uint8_t {
        GRANT_DATA      = 0,    // whole-line check at a data-carrying grant
        GRANT_DATALESS  = 1,    // whole-line check at a dataless grant
        LOAD            = 2     // consumer load check
    };

    const char* ToString(MonitorCheckKind kind) noexcept;

    // One scoreboard update record. Per-line histories are bounded by
    // 'monitor.history.length' (0 disables history keeping).
    struct MonitorHistoryEntry {
        uint64_t                    sequence;           // monitor event sequence of the update
        MonitorDataSource           source;             // data source of this update
        size_t                      upstreamIndex;      // upstream node that produced the update
        size_t                      rangeOffset;        // first byte covered by the access
        size_t                      rangeSize;          // bytes covered by the access
        uint64_t                    appliedMask;        // bytes actually written into the scoreboard
        std::array<uint64_t, 8>     image;              // expected line image after the update
    };

    // One detected mismatch. Carries both the failure source (check kind +
    // observing upstream node) and the data source (provenance of the expected
    // byte, resolved from the per-line history when available).
    struct MonitorMismatch {
        MonitorCheckKind            check;              // the check that failed
        uint64_t                    pa;                 // line base address
        size_t                      byteOffset;         // byte offset within the line
        uint8_t                     expected;           // scoreboard byte
        uint8_t                     actual;             // observed byte
        size_t                      upstreamIndex;      // observing upstream node
        MonitorDataSource           source;             // data source of the expected byte
        size_t                      sourceUpstreamIndex;// node that produced the expectation
        uint64_t                    sourceSequence;     // sequence of the producing update (0 = unknown)
        uint64_t                    sequence;           // monitor event sequence at detection
    };

    // Fired on the monitor's EventHub for every detected mismatch, before any
    // configured instance failure is raised.
    class MonitorMismatchEvent : public Gravity::Event<MonitorMismatchEvent> {
    protected:
        CacheLineDataMonitor&       monitor;
        MonitorMismatch             mismatch;

    public:
        MonitorMismatchEvent(CacheLineDataMonitor& monitor, const MonitorMismatch& mismatch) noexcept;

    public:
        CacheLineDataMonitor&       GetMonitor() noexcept;
        const CacheLineDataMonitor& GetMonitor() const noexcept;

        const MonitorMismatch&      GetMismatch() const noexcept;
    };

    // CacheLineDataMonitor: a global data monitor for cache lines.
    //
    // Subscribes to the CacheLine events of every Type 1 Taurus upstream node
    // (OnCacheLineGranted / OnCacheLinePostLoad / OnCacheLinePostStore) and
    // maintains a global scoreboard of expected line data:
    //
    //  - data-carrying grants verify the whole DUT-delivered image against the
    //    scoreboard and capture not-yet-known bytes (initial data capture);
    //  - dataless grants of the MakeUnique class drop the line's expectation:
    //    MakeUnique transfers no data and discards dirty copies elsewhere, so
    //    until the requester's overwrite lands the line's value is
    //    architecture-undefined and re-learned at the next defining event.
    //    Only a dataless-answered allocating read (ReadUnique from Shared)
    //    verifies the persisted image, which is coherence-equal by
    //    construction;
    //  - stores commit their byte range into the scoreboard;
    //  - loads verify their byte range against the scoreboard;
    //  - a completed MakeInvalid (CBOInval, observed via OnAcceptedCompCMO)
    //    drops the line's expectation: the CMO architecturally discards dirty
    //    data without writeback, so the memory image is no longer known and
    //    the bytes become not-yet-known again. The same drop happens at the
    //    moment a SnpMakeInvalid demotes a UniqueDirty line to Invalid
    //    (observed via SNP demotion events): that is the exact point where
    //    dirty data dies for both MakeInvalid and MakeUnique, potentially long
    //    before the discarding transaction's grant/CompCMO completes.
    //    CleanShared/CleanInvalid (CBOClean/CBOFlush) write dirty data back, so
    //    their expectations stay valid and keep verifying the DUT's CMO
    //    writeback path.
    //
    // The simulation is single-threaded and the events fire synchronously
    // inside the CacheLine accessors, so event order coincides with the
    // per-line coherence order of data updates: stores execute only in Unique
    // state (all other copies invalidated before the grant) and a load or a
    // data-carrying grant can only observe data that passed through the DUT
    // after the last committed store. A mismatch is therefore always a genuine
    // data-integrity error, never a race artifact.
    //
    // Every mismatch is logged, recorded and fired on events->OnMismatch, and
    // marks the instance FAILED (first mismatch wins) unless disabled via
    // 'monitor.fail.on.mismatch' (default true).
    class CacheLineDataMonitor {
    public:
        static const ConfigurationEntryBack<bool>       MONITOR_ENABLE;
        static const ConfigurationEntryBack<bool>       MONITOR_FAIL_ON_MISMATCH;
        static const ConfigurationEntryBack<bool>       MONITOR_ASSUME_ZERO_INIT;
        static const ConfigurationEntryBack<bool>       MONITOR_TRACE;
        static const ConfigurationEntryBack<uint64_t>   MONITOR_HISTORY_LENGTH;

    public:
        static constexpr size_t     MISMATCH_RECORD_LIMIT   = 64;
        static constexpr size_t     REPORT_MISMATCH_LIMIT   = 8;
        static constexpr uint8_t    NO_UPSTREAM             = 0xFF;

        // Sentinel for MonitorMismatch::sourceUpstreamIndex when the producing
        // node is unknown (history disabled or evicted).
        static constexpr size_t     UNKNOWN_UPSTREAM        = ~size_t(0);

    public:
        // Expected-image state of one tracked cache line.
        struct ScoreboardLine {
            std::array<uint64_t, 8>     data                        = {};
            uint64_t                    validMask                   = 0;
            std::array<uint8_t, 64>     sources                     = {};   // MonitorDataSource per byte
            std::array<uint8_t, 64>     sourceUpstreams             = {};   // producing node per byte
            std::deque<MonitorHistoryEntry>
                                        history;
        };

        class EventHub {
        public:
            Gravity::EventBus<MonitorMismatchEvent> OnMismatch;

        public:
            EventHub() noexcept = default;
        };

        std::shared_ptr<EventHub>   events          = std::make_shared<EventHub>();

    protected:
        using TaurusNode            = CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>;
        using GrantedEvent          = CCHI::Taurus::UpstreamNodeCacheLineGrantedEvent<CommonFlitConfigurationType1>;
        using PostLoadEvent         = CCHI::Taurus::UpstreamNodeCacheLinePostLoadEvent<CommonFlitConfigurationType1>;
        using PostStoreEvent        = CCHI::Taurus::UpstreamNodeCacheLinePostStoreEvent<CommonFlitConfigurationType1>;
        using AcceptedCompCMOEvent  = CCHI::Taurus::UpstreamNodeXactAcceptedCompCMOEvent<CommonFlitConfigurationType1>;
        using SNPDemotionEvent      = CCHI::Taurus::UpstreamNodeSNPCacheStatePostDemotionEvent<CommonFlitConfigurationType1>;

        static constexpr const char* LISTENER_NAME  = "Cohestra.CacheLineDataMonitor";

    protected:
        Instance&                   instance;

        bool                        enabled         = MONITOR_ENABLE.defaultValue;
        bool                        failOnMismatch  = MONITOR_FAIL_ON_MISMATCH.defaultValue;
        bool                        assumeZeroInit  = MONITOR_ASSUME_ZERO_INIT.defaultValue;
        bool                        trace           = MONITOR_TRACE.defaultValue;
        uint64_t                    historyLength   = MONITOR_HISTORY_LENGTH.defaultValue;

        bool                        attached        = false;

        std::unordered_map<uint64_t, ScoreboardLine>
                                    scoreboard;

        // hubs the listeners are registered on; deduplicated because a harness
        // may share one hub between all upstream nodes
        std::vector<std::shared_ptr<TaurusNode::EventHub>>
                                    attachedHubs;

        uint64_t                    sequence        = 0;

        uint64_t                    countGrantData          = 0;
        uint64_t                    countGrantDataless      = 0;
        uint64_t                    countGrantBytesCaptured = 0;
        uint64_t                    countLoads              = 0;
        uint64_t                    countLoadBytesChecked   = 0;
        uint64_t                    countStores             = 0;
        uint64_t                    countStoreBytes         = 0;
        uint64_t                    countCMOInvalidations   = 0;
        uint64_t                    countSNPInvalidations   = 0;

        uint64_t                    countMismatches         = 0;
        std::array<uint64_t, 3>     countMismatchesByCheck  = {};

        std::vector<MonitorMismatch>
                                    mismatches;

    public:
        explicit                    CacheLineDataMonitor(Instance& instance) noexcept;
                                    ~CacheLineDataMonitor() noexcept;

    public:
        bool                        LoadConfiguration(const StringLoaderBase& loader) noexcept;

    public:
        void                        Attach() noexcept;
        void                        Detach() noexcept;
        bool                        IsAttached() const noexcept;

        bool                        IsEnabled() const noexcept;

    protected:
        template<class T>
        bool                        LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

    protected:
        void                        OnGranted(GrantedEvent& event) noexcept;
        void                        OnPostLoad(PostLoadEvent& event) noexcept;
        void                        OnPostStore(PostStoreEvent& event) noexcept;
        void                        OnAcceptedCompCMO(AcceptedCompCMOEvent& event) noexcept;
        void                        OnSNPDemotion(SNPDemotionEvent& event) noexcept;

        // drops a line's expectation (validMask := 0): bytes become not-yet-known
        // and are re-learned on the next data-carrying grant; loads skip them
        void                        DropExpectation(uint64_t pa,
                                                    MonitorDataSource source,
                                                    size_t upstreamIndex) noexcept;

    protected:
        ScoreboardLine&             RequireLine(uint64_t pa) noexcept;

        void                        AppendHistory(ScoreboardLine& line,
                                                MonitorDataSource source,
                                                size_t upstreamIndex,
                                                size_t rangeOffset,
                                                size_t rangeSize,
                                                uint64_t appliedMask) noexcept;

        void                        ResolveProvenance(const ScoreboardLine& line,
                                                size_t byteOffset,
                                                MonitorMismatch& mismatch) noexcept;

        void                        RecordMismatch(MonitorCheckKind check,
                                                uint64_t pa,
                                                size_t byteOffset,
                                                uint8_t expected,
                                                uint8_t actual,
                                                size_t upstreamIndex,
                                                const ScoreboardLine& line) noexcept;

    public:
        uint64_t                    GetLineCount() const noexcept;

        uint64_t                    GetGrantDataCount() const noexcept;
        uint64_t                    GetGrantDatalessCount() const noexcept;
        uint64_t                    GetGrantBytesCaptured() const noexcept;
        uint64_t                    GetLoadCount() const noexcept;
        uint64_t                    GetLoadBytesChecked() const noexcept;
        uint64_t                    GetStoreCount() const noexcept;
        uint64_t                    GetStoreBytes() const noexcept;
        uint64_t                    GetCMOInvalidationCount() const noexcept;
        uint64_t                    GetSNPInvalidationCount() const noexcept;

        uint64_t                    GetMismatchCount() const noexcept;
        uint64_t                    GetMismatchCount(MonitorCheckKind check) const noexcept;

        const std::vector<MonitorMismatch>&
                                    GetMismatchRecords() const noexcept;

        std::optional<std::reference_wrapper<const ScoreboardLine>>
                                    GetLine(uint64_t pa) const noexcept;

        std::optional<std::reference_wrapper<const std::deque<MonitorHistoryEntry>>>
                                    GetLineHistory(uint64_t pa) const noexcept;

    public:
        void                        Report() const noexcept;
    };
}


// Inline implementation of: class CacheLineDataMonitor
namespace Cohestra {

    template<class T>
    inline bool CacheLineDataMonitor::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry);

        if (!rawValue)
        {
            dst = entry->defaultValue;
            return true;
        }

        std::optional<T> deserializedValue = entry->deserializer(*rawValue);

        if (!deserializedValue)
        {
            instance.GetLogger()->error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }
}
