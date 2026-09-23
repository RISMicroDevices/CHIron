#pragma once

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cohestra_config.hpp"
#include "cohestra_instance.hpp"
#include "cohestra_cchi_flit_format.hpp"

#include "../xact/cchi_xactions.hpp"

#include "../../common/eventbus.hpp"


namespace Cohestra {

    class TransactionWatchdog;

    // Transaction class of a watched transaction: the birth channel on which
    // the transaction became DUT-outstanding.
    enum class WatchdogXactClass : uint8_t {
        REQ         = 0,    // request-channel transaction (OnAcceptedREQ)
        EVT         = 1,    // eviction-hint transaction (OnAcceptedEVT)
        CMO         = 2,    // cache-maintenance transaction (OnAcceptedCMO)
        PREFETCH    = 3     // prefetch transaction (OnAcceptedPrefetch)
    };

    const char* ToString(WatchdogXactClass xactClass) noexcept;

    // TransactionWatchdog: a deadlock/livelock watchdog for DUT-outstanding
    // transactions.
    //
    // Subscribes to the transaction lifecycle events of every Type 1 Taurus
    // upstream node and supervises transactions from the moment they become
    // DUT-outstanding (birth: OnAcceptedREQ / OnAcceptedEVT / OnAcceptedCMO /
    // OnAcceptedPrefetch) until their first response (death:
    // OnCacheLineGranted / OnAcceptedCompCMO / OnAcceptedDnRSP /
    // OnAcceptedDnDAT). The outstanding registry is keyed by the xaction
    // shared_ptr identity; the entry holds the shared_ptr, so the object
    // cannot be recycled while registered (no pointer-reuse ABA). The first
    // matching death event retires the entry: supervision is birth -> first
    // response (e.g. a WriteBack's DBIDResp retires its entry early; the data
    // phase is still covered by the global tier below).
    //
    // Two timeout tiers are checked once per harness cycle from Tick():
    //
    //  - livelock (global no-progress): transactions are outstanding but no
    //    death/progress event (including OnAcceptedSNP as DUT-driven
    //    activity) has fired for 'watchdog.progress.timeout' time units.
    //    The timer is re-based whenever the registry transitions from empty
    //    to non-empty, so a quiet start never measures against simulation
    //    time zero;
    //  - deadlock (per-transaction age): the oldest outstanding transaction
    //    (front of the birth-ordered registry) has waited longer than
    //    'watchdog.xact.timeout' time units for its first response; the
    //    trigger names that transaction (upstream, class, TxnID, opcode,
    //    address, age).
    //
    // Time values are harness 'time' units (2 per clock, the same scale as
    // the --cycles command line argument). The simulation is
    // single-threaded and the events fire synchronously inside
    // instance.Tick, so stamping the watchdog clock with the last Tick time
    // is exact enough.
    //
    // On a trigger the oldest 'watchdog.dump.limit' outstanding transactions
    // are dumped at error level; then the instance is marked FAILED (first
    // trigger wins, exit code 2 via the normal FAILED path) unless disabled
    // via 'watchdog.fail.on.timeout', in which case each trigger is logged
    // once (per transaction, per no-progress epoch) and the run continues.
    //
    // Transaction denials already fail the instance via the harness denial
    // listeners (event_upstream.cpp), so denied transactions are ignored
    // here. A transaction wedged upstream before its flit is
    // channel-accepted is not DUT-outstanding and is only bounded by the
    // global tier while other work is outstanding.
    class TransactionWatchdog {
    public:
        static const ConfigurationEntryBack<bool>       WATCHDOG_ENABLE;
        static const ConfigurationEntryBack<uint64_t>   WATCHDOG_PROGRESS_TIMEOUT;
        static const ConfigurationEntryBack<uint64_t>   WATCHDOG_XACT_TIMEOUT;
        static const ConfigurationEntryBack<bool>       WATCHDOG_FAIL_ON_TIMEOUT;
        static const ConfigurationEntryBack<uint64_t>   WATCHDOG_DUMP_LIMIT;
        static const ConfigurationEntryBack<bool>       WATCHDOG_DUMP_FLITS;

    protected:
        using Cfg                   = CommonFlitConfigurationType1;
        using TaurusNode            = CCHI::Taurus::UpstreamNode<Cfg>;
        using AcceptedREQEvent      = CCHI::Taurus::UpstreamNodeXactAcceptedREQEvent<CommonFlitConfigurationType1>;
        using AcceptedEVTEvent      = CCHI::Taurus::UpstreamNodeXactAcceptedEVTEvent<CommonFlitConfigurationType1>;
        using AcceptedCMOEvent      = CCHI::Taurus::UpstreamNodeXactAcceptedCMOEvent<CommonFlitConfigurationType1>;
        using AcceptedPrefetchEvent = CCHI::Taurus::UpstreamNodeXactAcceptedPrefetchEvent<CommonFlitConfigurationType1>;
        using GrantedEvent          = CCHI::Taurus::UpstreamNodeCacheLineGrantedEvent<CommonFlitConfigurationType1>;
        using AcceptedCompCMOEvent  = CCHI::Taurus::UpstreamNodeXactAcceptedCompCMOEvent<CommonFlitConfigurationType1>;
        using AcceptedDnRSPEvent    = CCHI::Taurus::UpstreamNodeXactAcceptedDnRSPEvent<CommonFlitConfigurationType1>;
        using AcceptedDnDATEvent    = CCHI::Taurus::UpstreamNodeXactAcceptedDnDATEvent<CommonFlitConfigurationType1>;
        using AcceptedSNPEvent      = CCHI::Taurus::UpstreamNodeXactAcceptedSNPEvent<CommonFlitConfigurationType1>;

        static constexpr const char* LISTENER_NAME  = "Cohestra.TransactionWatchdog";

        // Sentinel for the upstream index of an entry whose producing node
        // could not be resolved.
        static constexpr size_t     UNKNOWN_UPSTREAM    = ~size_t(0);

        static constexpr size_t     XACT_CLASS_COUNT    = 4;

    public:
        // One watched (DUT-outstanding) transaction. All diagnostics are
        // captured at birth from the typed flit getter, so no flit access is
        // needed at death. Holding the xaction shared_ptr keeps the object
        // alive for the whole registration, so the registry key (its raw
        // pointer identity) cannot be recycled by a later transaction.
        struct OutstandingEntry {
            std::shared_ptr<CCHI::Xact::Xaction<CommonFlitConfigurationType1>>
                                xaction;
            size_t              upstreamIndex;
            WatchdogXactClass   xactClass;
            uint64_t            txnID;
            uint64_t            opcode;
            uint64_t            address;
            uint64_t            birthTime;
            bool                reported        = false;  // warn mode: trigger already logged
        };

    protected:
        Instance&                   instance;

        bool                        enabled         = WATCHDOG_ENABLE.defaultValue;
        uint64_t                    progressTimeout = WATCHDOG_PROGRESS_TIMEOUT.defaultValue;
        uint64_t                    xactTimeout     = WATCHDOG_XACT_TIMEOUT.defaultValue;
        bool                        failOnTimeout   = WATCHDOG_FAIL_ON_TIMEOUT.defaultValue;
        uint64_t                    dumpLimit       = WATCHDOG_DUMP_LIMIT.defaultValue;
        bool                        dumpFlits       = WATCHDOG_DUMP_FLITS.defaultValue;

        bool                        attached        = false;

        // birth-ordered outstanding registry (front = oldest) plus a lookup
        // from xaction pointer identity to the entry
        std::list<OutstandingEntry> outstanding;
        std::unordered_map<const void*, std::list<OutstandingEntry>::iterator>
                                    outstandingByKey;

        // hubs the listeners are registered on; deduplicated because a harness
        // may share one hub between all upstream nodes
        std::vector<std::shared_ptr<TaurusNode::EventHub>>
                                    attachedHubs;

        uint64_t                    now             = 0;    // stamped by Tick
        uint64_t                    lastProgress    = 0;    // stamped by every death/progress event
        bool                        livelockWarned  = false;

        uint64_t                    maxOutstanding  = 0;
        std::array<uint64_t, XACT_CLASS_COUNT>
                                    countBirthsByClass      = {};
        std::array<uint64_t, XACT_CLASS_COUNT>
                                    countDeathsByClass      = {};

        uint64_t                    countLivelockTriggers   = 0;
        uint64_t                    countXactTriggers       = 0;

    public:
        explicit                    TransactionWatchdog(Instance& instance) noexcept;
                                    ~TransactionWatchdog() noexcept;

    public:
        bool                        LoadConfiguration(const StringLoaderBase& loader) noexcept;

    public:
        void                        Attach() noexcept;
        void                        Detach() noexcept;
        bool                        IsAttached() const noexcept;

        bool                        IsEnabled() const noexcept;

    public:
        // Per-cycle timeout check; called once per harness cycle with the
        // current harness time
        void                        Tick(uint64_t time) noexcept;

    protected:
        template<class T>
        bool                        LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

    protected:
        void                        OnAcceptedREQ(AcceptedREQEvent& event) noexcept;
        void                        OnAcceptedEVT(AcceptedEVTEvent& event) noexcept;
        void                        OnAcceptedCMO(AcceptedCMOEvent& event) noexcept;
        void                        OnAcceptedPrefetch(AcceptedPrefetchEvent& event) noexcept;

        void                        OnCacheLineGranted(GrantedEvent& event) noexcept;
        void                        OnAcceptedCompCMO(AcceptedCompCMOEvent& event) noexcept;
        void                        OnAcceptedDnRSP(AcceptedDnRSPEvent& event) noexcept;
        void                        OnAcceptedDnDAT(AcceptedDnDATEvent& event) noexcept;
        void                        OnAcceptedSNP(AcceptedSNPEvent& event) noexcept;

        void                        Birth(WatchdogXactClass xactClass,
                                         std::shared_ptr<CCHI::Xact::Xaction<CommonFlitConfigurationType1>> xaction,
                                         uint64_t txnID,
                                         uint64_t opcode,
                                         uint64_t address,
                                         size_t upstreamIndex) noexcept;

        void                        Retire(const std::shared_ptr<CCHI::Xact::Xaction<CommonFlitConfigurationType1>>& xaction) noexcept;

        void                        DumpOutstanding() const noexcept;

        void                        DumpXactFlits(const OutstandingEntry& entry) const noexcept;

        static const char*          GetRequestChannelLabel(const CCHI::Xact::FiredRequestFlit<Cfg>& flit) noexcept;
        static const char*          GetResponseChannelLabel(const CCHI::Xact::FiredResponseFlit<Cfg>& flit) noexcept;

        static std::string          FormatFirstFlit(const CCHI::Xact::FiredRequestFlit<Cfg>& flit) noexcept;
        static std::string          FormatResponseFlit(const CCHI::Xact::FiredResponseFlit<Cfg>& flit) noexcept;

        static std::optional<uint64_t>
                                    GetXactHashTag(const std::shared_ptr<CCHI::Xact::Xaction<Cfg>>& xaction) noexcept;

    public:
        uint64_t                    GetOutstandingCount() const noexcept;
        uint64_t                    GetMaxOutstandingCount() const noexcept;

        uint64_t                    GetBirthCount() const noexcept;
        uint64_t                    GetBirthCount(WatchdogXactClass xactClass) const noexcept;
        uint64_t                    GetDeathCount() const noexcept;
        uint64_t                    GetDeathCount(WatchdogXactClass xactClass) const noexcept;

        uint64_t                    GetLivelockTriggerCount() const noexcept;
        uint64_t                    GetXactTriggerCount() const noexcept;

    public:
        void                        Report() const noexcept;
    };
}


// Inline implementation of: class TransactionWatchdog
namespace Cohestra {

    template<class T>
    inline bool TransactionWatchdog::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
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
