#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "../cohestra_config.hpp"
#include "../cohestra_interface.hpp"

#include "../../spec/cchi_protocol_encoding.hpp"

#include "spdlog/spdlog.h"


namespace Cohestra {

    // EarthModel: a simple, pure-C++ downstream (home node + memory) CCHI model.
    //
    // One instance serves all Type 1 ports with a shared memory and a shared
    // directory, so cross-port (multi-agent) cache coherency is exercised.
    //
    // Coherency policy:
    //  - directory states per 64B line: I / S (clean, sharer bitmask) / U (possibly
    //    dirty, single owner); S lines are always clean, U lines are always
    //    snooped with data-returning opcodes before ownership transfers.
    //  - ReadShared      : SnpToShared to a unique owner, then CompData (SC).
    //  - ReadUnique      : SnpToInvalid to a unique owner or SnpMakeInvalid to
    //                      other sharers, then CompData (UC) or Comp, honoring
    //                      ExpCompData.
    //  - MakeUnique      : same invalidation flow, then Comp.
    //  - Evict           : directory drop, then Comp.
    //  - WriteBackFull   : CompDBIDResp, then CopyBackWrData beats are merged
    //                      into memory.
    //
    // Channel priority: EVT > SNP > REQ, applied to the per-cycle processing
    // order (TickEVT -> TickSNP -> TickREQ). EVT admissions are additionally
    // allowed to cut ahead of an active REQ transaction on the same line,
    // because Taurus upstream nodes hold snoop responses until their in-flight
    // eviction is completed; completing the eviction first is always coherent
    // and avoids the request/snoop/eviction circular wait.
    //
    // Transactional parallelism is configurable (home transaction tracker size,
    // per-port snoop inflight limit, per-channel queue depths, response
    // latencies). One upstream-initiated transaction (REQ or EVT) is accepted
    // per port at a time: Taurus nodes always drive TxnID 0, so a second
    // concurrent one could not be told apart on the response channels. Excess
    // pushes are denied (backpressured) and reported.
    //
    // Denials are always reported: flow-control (backpressure) denials are
    // counted and logged at debug level, protocol violations are counted and
    // logged at warning/error level; both are readable through counters.
    class EarthModel {
    public:
        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_NODE_ID;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_PARALLELISM;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_INFLIGHT_SNP;

        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_QUEUE_EVT;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_QUEUE_REQ;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_QUEUE_RSP;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_QUEUE_DAT;

        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_LATENCY_RSP;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_EARTH_LATENCY_DAT;

        static const ConfigurationEntryBack<uint64_t>   CONFIG_EARTH_MEMORY_START;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_EARTH_MEMORY_END;

        static const ConfigurationEntryBack<bool>       CONFIG_EARTH_VERBOSE;
        static const ConfigurationEntryBack<bool>       CONFIG_EARTH_VERBOSE_FLIT;
        static const ConfigurationEntryBack<bool>       CONFIG_EARTH_VERBOSE_XACT;
        static const ConfigurationEntryBack<bool>       CONFIG_EARTH_VERBOSE_STATE;
        static const ConfigurationEntryBack<bool>       CONFIG_EARTH_VERBOSE_INTERNAL;
        static const ConfigurationEntryBack<bool>       CONFIG_EARTH_VERBOSE_BACKPRESSURE;

    public:
        using FlitEVT   = CCHI::Flits::EVT<CommonFlitConfigurationType1>;
        using FlitREQ   = CCHI::Flits::REQ<CommonFlitConfigurationType1>;
        using FlitSNP   = CCHI::Flits::SNP<CommonFlitConfigurationType1>;
        using FlitUpRSP = CCHI::Flits::UpRSP<CommonFlitConfigurationType1>;
        using FlitUpDAT = CCHI::Flits::UpDAT<CommonFlitConfigurationType1>;
        using FlitDnRSP = CCHI::Flits::DnRSP<CommonFlitConfigurationType1>;
        using FlitDnDAT = CCHI::Flits::DnDAT<CommonFlitConfigurationType1>;

        static constexpr size_t     LINE_SIZE           = 64;
        static constexpr size_t     LINE_WORDS          = LINE_SIZE / sizeof(uint64_t);
        static constexpr size_t     BEATS_PER_LINE      = 512 / CommonFlitConfigurationType1::dataWidth;
        static constexpr size_t     WORDS_PER_BEAT      = LINE_WORDS / BEATS_PER_LINE;
        static constexpr size_t     TXNID_SPACE         = size_t(1) << CommonFlitConfigurationType1::dbIdWidth;

    protected:
        enum class LineState {
            I = 0,
            S,
            U
        };

        struct DirectoryEntry {
            LineState   state           = LineState::I;
            uint32_t    sharers         = 0;    // port bitmask, valid when S
            size_t      owner           = 0;    // port index, valid when U
        };

        enum class XactKind {
            ReadShared = 0,
            ReadUnique,
            MakeUnique,
            Evict,
            WriteBackFull
        };

        enum class XactPhase {
            Snoop = 0,      // issuing snoops and/or waiting for snoop responses
            Respond,        // waiting for readyTime to emit Comp/CompData/CompDBIDResp
            WaitCompAck,    // waiting for CompAck of the emitted DBID
            WaitWrData,     // waiting for CopyBackWrData beats of the emitted DBID
            WaitPop         // waiting for the Evict Comp to be popped downstream
        };

        struct Snoop {
            size_t      port            = 0;
            uint8_t     opcode          = 0;
            int         txnID           = -1;
            bool        issued          = false;
            bool        done            = false;
            bool        respNotI        = false;
        };

        struct Xaction {
            bool                        active          = false;
            XactKind                    kind            = XactKind::ReadShared;
            size_t                      port            = 0;
            uint64_t                    line            = 0;      // PA >> 6
            uint64_t                    addr            = 0;
            uint8_t                     txnID           = 0;
            uint32_t                    srcNodeID       = 0;
            bool                        expCompData     = false;
            uint8_t                     traceTag        = 0;

            XactPhase                   phase           = XactPhase::Snoop;
            uint64_t                    readyTime       = 0;

            int                         dbid            = -1;
            bool                        ackReceived     = false;
            size_t                      compDataBeats   = 0;

            bool                        snoopPlanBuilt  = false;
            std::vector<Snoop>          snoops;

            uint32_t                    wrBeatMask      = 0;
        };

        enum class DBIDKind {
            CompAckWait = 0,
            WriteBackData
        };

        struct DBIDEntry {
            size_t      slot;
            DBIDKind    kind;
        };

        struct SnoopEntry {
            size_t      slot;
            size_t      snoop;
            uint64_t    line;
            uint32_t    beatMask        = 0;
            bool        gotData         = false;
            bool        gotResp         = false;
            uint8_t     resp            = 0;
        };

        template<class TFlit>
        struct StampedFlit {
            TFlit       flit;
            uint64_t    stamp;
            bool        registered;
        };

        struct Port {
            std::deque<StampedFlit<FlitEVT>>    evtFIFO;
            std::deque<StampedFlit<FlitREQ>>    reqFIFO;
            std::deque<FlitUpRSP>               uprspFIFO;
            std::deque<FlitUpDAT>               updatFIFO;

            std::deque<FlitSNP>                 snpQueue;
            std::deque<FlitDnRSP>               dnrspQueue;
            std::deque<FlitDnDAT>               dndatQueue;

            // one allocator per port shared by snoop TxnIDs and DBIDs, so an
            // UpRSP/UpDAT flit always resolves to exactly one transaction
            std::array<bool, TXNID_SPACE>       idUsed          = {};

            std::unordered_map<uint8_t, SnoopEntry>
                                                snoops;
            std::unordered_map<uint8_t, DBIDEntry>
                                                dbids;
        };

    protected:
        std::shared_ptr<spdlog::logger>
                                logger              = spdlog::default_logger();

        const size_t            portCount;

        uint64_t                time                = 0;

        uint32_t                nodeID;
        uint32_t                parallelism;
        uint32_t                inflightSNP;

        uint32_t                queueDepthEVT;
        uint32_t                queueDepthREQ;
        uint32_t                queueDepthRSP;
        uint32_t                queueDepthDAT;

        uint32_t                latencyRSP;
        uint32_t                latencyDAT;

        uint64_t                memoryStart;
        uint64_t                memoryEnd;

        // verbose switches: 'verbose' is the total switch of all categories
        bool                    verbose;
        bool                    verboseFlit;
        bool                    verboseXact;
        bool                    verboseState;
        bool                    verboseInternal;
        bool                    verboseBackpressure;

        std::vector<uint32_t>   upstreamNodeIDs;

        std::vector<Port>       ports;

        std::vector<Xaction>    tracker;

        std::unordered_map<uint64_t, std::array<uint64_t, LINE_WORDS>>
                                memory;

        std::unordered_map<uint64_t, DirectoryEntry>
                                directory;

        std::unordered_map<uint64_t, size_t>
                                lineREQ;

        std::unordered_map<uint64_t, size_t>
                                lineEVT;

        std::unordered_map<uint64_t, std::deque<std::pair<uint64_t, size_t>>>
                                lineWaiters;

        uint64_t                stampCounter        = 0;

        uint64_t                countDeniedBackpressure = 0;
        uint64_t                countDeniedProtocol     = 0;

        uint64_t                countREQ            = 0;
        uint64_t                countEVT            = 0;
        uint64_t                countSNP            = 0;

    public:
        EarthModel(size_t portCount) noexcept;

        virtual ~EarthModel() noexcept = default;

    public:
        bool                                LoadConfiguration(const StringLoaderBase& loader) noexcept;

        void                                SetUpstreamNodeIDs(const std::vector<uint32_t>& nodeIDs) noexcept;

    public:
        void                                Tick(uint64_t time) noexcept;

    protected:
        void                                TickEVT() noexcept;
        void                                TickSNP() noexcept;
        void                                TickREQ() noexcept;
        void                                TickEmit() noexcept;

    public:
        bool                                PushEVT(size_t port, const FlitEVT& flit) noexcept;
        bool                                PushREQ(size_t port, const FlitREQ& flit) noexcept;
        bool                                PushUpRSP(size_t port, const FlitUpRSP& flit) noexcept;
        bool                                PushUpDAT(size_t port, const FlitUpDAT& flit) noexcept;

        bool                                HasSNP(size_t port) const noexcept;
        std::optional<FlitSNP>              PeekSNP(size_t port) const noexcept;
        std::optional<FlitSNP>              PopSNP(size_t port) noexcept;

        bool                                HasDnRSP(size_t port) const noexcept;
        std::optional<FlitDnRSP>            PeekDnRSP(size_t port) const noexcept;
        std::optional<FlitDnRSP>            PopDnRSP(size_t port) noexcept;

        bool                                HasDnDAT(size_t port) const noexcept;
        std::optional<FlitDnDAT>            PeekDnDAT(size_t port) const noexcept;
        std::optional<FlitDnDAT>            PopDnDAT(size_t port) noexcept;

    public:
        size_t                              GetPortCount() const noexcept;
        uint32_t                            GetNodeID() const noexcept;

        bool                                IsIdle() const noexcept;

        uint64_t                            GetBackpressureDenialCount() const noexcept;
        uint64_t                            GetProtocolDenialCount() const noexcept;

        uint64_t                            GetServicedREQCount() const noexcept;
        uint64_t                            GetServicedEVTCount() const noexcept;
        uint64_t                            GetServicedSNPCount() const noexcept;

        std::shared_ptr<spdlog::logger>     GetLogger() const noexcept;
        void                                SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept;

        void                                DumpState() const noexcept;

    protected:
        std::optional<int>                  AllocateID(size_t port) noexcept;
        void                                FreeID(size_t port, int id) noexcept;

        std::optional<size_t>               AllocateSlot() noexcept;

        void                                BuildSnoopPlan(size_t slot) noexcept;
        bool                                CheckSnoopsDone(size_t slot) noexcept;

        void                                Retire(size_t slot) noexcept;

        DirectoryEntry                      DirLookup(uint64_t line) const noexcept;
        void                                DirRemove(size_t port, uint64_t line) noexcept;
        void                                DirGrant(size_t slot) noexcept;

        void                                MergeBeat(uint64_t line, size_t dataID, const uint64_t* data, uint32_t BE) noexcept;

        void                                ReportBackpressure(const char* what, size_t port) noexcept;
        void                                ReportViolation(const char* what, size_t port) noexcept;

        bool                                CheckAddressWindow(uint64_t addr, const char* what, size_t port) noexcept;

        template<class T>
        bool                                LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;
    };
}


// Inline implementation of: class EarthModel
namespace Cohestra {

    template<class T>
    inline bool EarthModel::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry, "earth");

        if (!rawValue)
        {
            dst = entry->defaultValue;
            return true;
        }

        std::optional<T> deserializedValue = entry->deserializer(*rawValue);

        if (!deserializedValue)
        {
            logger->error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }
}
