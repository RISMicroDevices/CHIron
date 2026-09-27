#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "../cohestra_instance.hpp"

#include "../../prof/cchi_prof_metrics.hpp"


namespace Cohestra {

    /*
    *   Core-to-core latency benchmark stimulator.
    *
    *   A replication of https://github.com/nviennot/core-to-core-latency inside
    *   cohestra: Type-1 Taurus upstream nodes play the role of pinned cores and
    *   the cache-coherence machinery between them (home + snoops) is the system
    *   under test. Latencies are measured in harness ticks from operation issue
    *   (Stimulator::Tick time) to operation grant (FutureNow completion observed
    *   from Tick; the callbacks themselves only set flags - Taurus ERRATA N3
    *   forbids driving the node from inside its own completion callback).
    *
    *   Benchmark modes (core2core.bench, mirroring the upstream --bench flag):
    *
    *   - Mode 2 (default): single-writer/single-reader ping-pong on two cache
    *     lines. For the ordered pair (a, b): a stores line X / b loads line X
    *     (the a->b transfer), then b stores line Y / a loads line Y (the b->a
    *     transfer), per iteration. The per-iteration one-way latency is
    *     iteration/2; the matrix cell (a,b) is the mean a->b transfer latency
    *     (b's load latency), matching the upstream per-pair matrix semantics.
    *
    *   - Mode 3: one writer / one reader message passing over N cache lines.
    *     The writer streams sequence values through lines in cyclic order
    *     (bounded in-flight lead) and the reader follows in the same order;
    *     per-transfer latency = reader-observation time - writer-commit time.
    *     This catches contention effects that ping-pong hides and yields a
    *     sustained line-migration bandwidth figure (wrBench-style).
    *
    *   - Mode 1 (CAS ping-pong on one cache line): deferred - it needs
    *     atomic-operation xactions in cchi/xact (known model gap, see
    *     doc/cchi-spec-consistency-report.md). Accepted in config, no-op.
    *
    *   Sampling mirrors the upstream defaults: 'samples' x 'iterations' per
    *   pair, first 'warmup' iterations discarded. Per-pair results are
    *   full per-iteration histograms plus the upstream per-sample-mean
    *   headline. Result generation mirrors upstream: a console matrix and a
    *   CSV file ('from,to,mean' long form plus a matrix view; percentiles in
    *   the detailed CSV).
    */
    class Core2Core : public Stimulator {
    public:
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_CORE2CORE_BENCH;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_CORE2CORE_NODES;
        static const ConfigurationEntryBack<uint64_t>               CONFIG_CORE2CORE_SAMPLES;
        static const ConfigurationEntryBack<uint64_t>               CONFIG_CORE2CORE_ITERATIONS;
        static const ConfigurationEntryBack<uint64_t>               CONFIG_CORE2CORE_WARMUP;
        static const ConfigurationEntryBack<uint64_t>               CONFIG_CORE2CORE_LINES;
        static const ConfigurationEntryBack<uint64_t>               CONFIG_CORE2CORE_INFLIGHT;
        static const ConfigurationEntryBack<uint64_t>               CONFIG_CORE2CORE_RANGE_START;
        static const ConfigurationEntryBack<std::string>            CONFIG_CORE2CORE_REPORT_CSV;
        static const ConfigurationEntryBack<std::string>            CONFIG_CORE2CORE_REPORT_SVG;

    protected:
        using TaurusNode    = CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>;
        using GrantedFuture = std::shared_ptr<CCHI::Taurus::FutureNow<typename TaurusNode::GrantedEvent>>;

        // data commit waiting to be applied from Tick (never from callbacks, N3)
        struct PendingCommit {
            uint32_t    upstreamIndex;
            uint64_t    pa;
            uint64_t    value;      // written at offset 0
            uint64_t    tag;        // mode 3: stream sequence t (for commitTime stamping)
            uint64_t    expiryTime;
        };

        struct PairStats {
            CCHI::Prof::Histogram   storeLatency;       // acquiring store: issue -> grant
            CCHI::Prof::Histogram   loadLatency;        // transferring load: issue -> grant (a->b one-way)
            CCHI::Prof::Histogram   iterationLatency;   // full ping-pong iteration (mode 2)
            CCHI::Prof::Histogram   transferLatency;    // mode 3: commit -> reader observation
            std::vector<double>     sampleMeans;        // per-sample mean of one-way latency
            uint64_t                mismatches  = 0;    // data-integrity check failures
            uint64_t                lapped      = 0;    // mode 3: value overwritten before consumed
            uint64_t                startTime   = 0;
            uint64_t                endTime     = 0;
        };

        struct Pair {
            uint32_t    a;          // writer/instigator upstream index
            uint32_t    b;          // reader/responder upstream index
            uint64_t    baseAddr;
            PairStats   stats;
        };

        enum class Phase {
            M2_STORE_A,             // issue a: store X
            M2_STORE_A_COMMIT,      // commit data, then issue b: load X
            M2_LOAD_B,              // wait b: load X grant
            M2_STORE_B,             // issue b: store Y
            M2_STORE_B_COMMIT,      // commit data, then issue a: load Y
            M2_LOAD_A,              // wait a: load Y grant -> iteration done
            M3_WRITE,               // stream writer (bounded in-flight)
            M3_DONE
        };

    protected:
        std::string                 section;

        std::vector<uint32_t>       type1Upstreams;

        std::vector<uint32_t>       bench;
        std::vector<uint32_t>       nodes;
        uint64_t                    samples;
        uint64_t                    iterations;
        uint64_t                    warmup;
        uint64_t                    lines;
        uint64_t                    inFlightLimit;
        uint64_t                    rangeStart;
        std::string                 reportCSV;
        std::string                 reportSVG;

        std::vector<Pair>           pairs;
        size_t                      currentPair;
        bool                        runMode3;       // false: mode 2 phase machine

        // current-pair mode-2 state
        Phase                       phase;
        GrantedFuture               currentOp;      // polled with IsNow()/IsRejected() from Tick
        uint64_t                    issueTime;
        uint64_t                    iterStartTime;
        uint64_t                    iteration;
        uint64_t                    sampleIteration;
        uint64_t                    sampleLatencySum;
        uint64_t                    nextValue;      // global store-value sequence (never 0)
        uint64_t                    phaseValue;     // value committed by the current phase's store

        // current-pair mode-3 state
        uint64_t                    writerNext;     // next stream value t to write (1-based)
        uint64_t                    readerNext;     // next stream value k to consume (1-based)
        uint64_t                    totalTransfers;
        std::deque<std::pair<uint64_t, GrantedFuture>>
                                    writerOps;      // in-flight writer stores: (t, future)
        std::unordered_map<uint64_t, uint64_t>
                                    commitTimes;    // stream t -> data-commit time (bounded by lines)

        std::deque<PendingCommit>   pendingCommits;

        bool                        begun;          // BeginPair ran for the current pair
        bool                        finished;
        bool                        reported;

    public:
        static constexpr size_t     PENDING_COMMIT_LIMIT = 64;

        // Mode-3 stream values are tagged in the upper 32 bits so that stale
        // line data (zero-initialized memory, leftovers from other phases or
        // earlier rounds) can never alias a stream value: the reader only
        // accepts values carrying the tag. Stream sequence numbers occupy the
        // lower 32 bits.
        static constexpr uint64_t   STREAM_TAG = 0xC2C3ull << 32;

    public:
        Core2Core(Instance&                     instance,
                  size_t                        index,
                  const std::vector<uint32_t>&  type1Upstreams) noexcept;

        virtual ~Core2Core() noexcept = default;

    public:
        virtual bool        Initialize(const StringLoaderBase& configSource) noexcept override;

    public:
        virtual void        Tick(uint64_t time) noexcept override;

    public:
        virtual bool        IsFinished() const noexcept override;

    public:
        // Emits the console matrix and the CSV result files. Idempotent; also
        // invoked automatically when the last pair completes.
        void                ReportResults() noexcept;

    protected:
        void                BeginPair(uint64_t time) noexcept;
        void                EndPair(uint64_t time) noexcept;

        void                TickMode2(uint64_t time) noexcept;
        void                TickMode3(uint64_t time) noexcept;

        void                RetireCommits(uint64_t time) noexcept;

        GrantedFuture       IssueStore(uint32_t upstreamIndex, uint64_t pa, uint64_t time) noexcept;
        GrantedFuture       IssueLoad(uint32_t upstreamIndex, uint64_t pa, uint64_t time) noexcept;

        std::optional<uint64_t>
                            ReadLineValue(uint32_t upstreamIndex, uint64_t pa) const noexcept;

        // Renders the mode-2 one-way latency matrix as an SVG heatmap
        // (same cell semantics as the CSV matrix). Called from ReportResults.
        void                WriteSVGHeatmap(const std::string& path) const noexcept;

        template<class T>
        bool                LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;
    };

    class Core2CoreFactory : public StimulatorFactory {
    public:
        Core2CoreFactory() noexcept;

        virtual ~Core2CoreFactory() noexcept = default;

    public:
        virtual std::unique_ptr<Stimulator> Create(Instance&                    instance,
                                                   size_t                       index,
                                                   const std::vector<uint32_t>& type1Upstreams) noexcept override;
    };
}
