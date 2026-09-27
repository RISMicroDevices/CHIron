#include "core2core.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>


// Implementation of: class Core2Core
namespace Cohestra {

    const ConfigurationEntryBack<std::vector<uint32_t>> Core2Core::CONFIG_CORE2CORE_BENCH
        ("CORE2CORE_BENCH", "core2core.bench", nullptr, { 2 }, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::vector<uint32_t>> Core2Core::CONFIG_CORE2CORE_NODES
        ("CORE2CORE_NODES", "core2core.nodes", nullptr, {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<uint64_t> Core2Core::CONFIG_CORE2CORE_SAMPLES
        ("CORE2CORE_SAMPLES", "core2core.samples", nullptr, 300, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> Core2Core::CONFIG_CORE2CORE_ITERATIONS
        ("CORE2CORE_ITERATIONS", "core2core.iterations", nullptr, 1000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> Core2Core::CONFIG_CORE2CORE_WARMUP
        ("CORE2CORE_WARMUP", "core2core.warmup", nullptr, 100, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> Core2Core::CONFIG_CORE2CORE_LINES
        ("CORE2CORE_LINES", "core2core.lines", nullptr, 32, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> Core2Core::CONFIG_CORE2CORE_INFLIGHT
        ("CORE2CORE_INFLIGHT", "core2core.inflight", nullptr, 4, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> Core2Core::CONFIG_CORE2CORE_RANGE_START
        ("CORE2CORE_RANGE_START", "core2core.range.start", nullptr, 0x80000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<std::string> Core2Core::CONFIG_CORE2CORE_REPORT_CSV
        ("CORE2CORE_REPORT_CSV", "core2core.report.csv", nullptr, "core2core.csv", DeserializeString, SerializeString);

    const ConfigurationEntryBack<std::string> Core2Core::CONFIG_CORE2CORE_REPORT_SVG
        ("CORE2CORE_REPORT_SVG", "core2core.report.svg", nullptr, "core2core.svg", DeserializeString, SerializeString);


    Core2Core::Core2Core(Instance&                    instance,
                         size_t                       index,
                         const std::vector<uint32_t>& type1Upstreams) noexcept
        : Stimulator          ("Core2Core", "Core-to-core latency benchmark (replication of nviennot/core-to-core-latency)", instance, index)
        , section             ("stimulator." + std::to_string(index))
        , type1Upstreams      (type1Upstreams)
        , bench               (CONFIG_CORE2CORE_BENCH.defaultValue)
        , nodes               (CONFIG_CORE2CORE_NODES.defaultValue)
        , samples             (CONFIG_CORE2CORE_SAMPLES.defaultValue)
        , iterations          (CONFIG_CORE2CORE_ITERATIONS.defaultValue)
        , warmup              (CONFIG_CORE2CORE_WARMUP.defaultValue)
        , lines               (CONFIG_CORE2CORE_LINES.defaultValue)
        , inFlightLimit       (CONFIG_CORE2CORE_INFLIGHT.defaultValue)
        , rangeStart          (CONFIG_CORE2CORE_RANGE_START.defaultValue)
        , reportCSV           (CONFIG_CORE2CORE_REPORT_CSV.defaultValue)
        , reportSVG           (CONFIG_CORE2CORE_REPORT_SVG.defaultValue)
        , pairs               ()
        , currentPair         (0)
        , runMode3            (false)
        , phase               (Phase::M2_STORE_A)
        , currentOp           (nullptr)
        , issueTime           (0)
        , iterStartTime       (0)
        , iteration           (0)
        , sampleIteration     (0)
        , sampleLatencySum    (0)
        , nextValue           (1)
        , phaseValue          (0)
        , writerNext          (1)
        , readerNext          (1)
        , totalTransfers      (0)
        , writerOps           ()
        , commitTimes         ()
        , pendingCommits      ()
        , begun               (false)
        , finished            (false)
        , reported            (false)
    { }

    template<class T>
    bool Core2Core::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
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

    bool Core2Core::Initialize(const StringLoaderBase& configSource) noexcept
    {
        bool good = LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_BENCH,       bench)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_NODES,       nodes)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_SAMPLES,     samples)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_ITERATIONS,  iterations)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_WARMUP,      warmup)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_LINES,       lines)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_INFLIGHT,    inFlightLimit)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_RANGE_START, rangeStart)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_REPORT_CSV,  reportCSV)
                 && LoadConfigurationEntry(configSource, &CONFIG_CORE2CORE_REPORT_SVG,  reportSVG);

        if (!good)
            return false;

        if (nodes.empty())
            nodes = type1Upstreams;

        if (nodes.size() < 2)
        {
            GetOwner().GetLogger()->error("Core2Core {}: needs at least 2 upstream nodes, got {}.",
                GetIndex(), nodes.size());
            return false;
        }

        if (!iterations)
        {
            GetOwner().GetLogger()->error("Core2Core {}: core2core.iterations must not be zero.", GetIndex());
            return false;
        }

        if (!samples)
        {
            GetOwner().GetLogger()->error("Core2Core {}: core2core.samples must not be zero.", GetIndex());
            return false;
        }

        if (warmup + samples * iterations > 0xFFFFFFFFull)
        {
            GetOwner().GetLogger()->error(
                "Core2Core {}: warmup + samples x iterations exceeds the 32-bit mode-3 stream space.", GetIndex());
            return false;
        }

        lines = std::max<uint64_t>(lines, 2);   // mode 2 needs lines X and Y

        bool wantMode2 = std::find(bench.begin(), bench.end(), 2) != bench.end();
        bool wantMode3 = std::find(bench.begin(), bench.end(), 3) != bench.end();
        bool wantMode1 = std::find(bench.begin(), bench.end(), 1) != bench.end();

        if (wantMode1)
            GetOwner().GetLogger()->warn(
                "Core2Core {}: bench mode 1 (CAS ping-pong) is deferred - it needs atomic-operation "
                "xactions in cchi/xact (see doc/cchi-spec-consistency-report.md); skipping.", GetIndex());

        if (!wantMode2 && !wantMode3)
        {
            GetOwner().GetLogger()->error("Core2Core {}: no runnable bench mode selected (core2core.bench = 2 and/or 3).", GetIndex());
            return false;
        }

        runMode3 = !wantMode2;  // mode 2 runs first when both selected

        // ordered pairs (a, b), a != b; each pair owns a private window of
        // 'lines' cache lines so pairs never interfere
        pairs.clear();

        for (uint32_t a : nodes)
            for (uint32_t b : nodes)
            {
                if (a == b)
                    continue;

                Pair pair;
                pair.a        = a;
                pair.b        = b;
                pair.baseAddr = rangeStart + uint64_t(pairs.size()) * lines * 64;
                pairs.push_back(pair);
            }

        currentPair   = 0;
        finished      = false;
        reported      = false;

        GetOwner().GetLogger()->info(
            "Core2Core {}: {} ordered pair(s) over {} node(s), mode(s) {}{}{}, "
            "{} sample(s) x {} iteration(s), warmup {}, lines {}, range start 0x{:X}.",
            GetIndex(), pairs.size(), nodes.size(),
            wantMode2 ? "2" : "", (wantMode2 && wantMode3) ? "+" : "", wantMode3 ? "3" : "",
            samples, iterations, warmup, lines, rangeStart);

        return true;
    }

    bool Core2Core::IsFinished() const noexcept
    {
        return finished;
    }

    Core2Core::GrantedFuture Core2Core::IssueStore(uint32_t upstreamIndex, uint64_t pa, uint64_t time) noexcept
    {
        auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

        if (!upstream)
            return nullptr;

        issueTime = time;
        return upstream->DoStoreLine(pa);
    }

    Core2Core::GrantedFuture Core2Core::IssueLoad(uint32_t upstreamIndex, uint64_t pa, uint64_t time) noexcept
    {
        auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

        if (!upstream)
            return nullptr;

        issueTime = time;
        return upstream->DoLoad(pa);
    }

    std::optional<uint64_t> Core2Core::ReadLineValue(uint32_t upstreamIndex, uint64_t pa) const noexcept
    {
        auto* upstream = GetOwner().GetType1Upstream(upstreamIndex);

        if (!upstream)
            return std::nullopt;

        auto line = upstream->GetCacheLine(pa);

        if (!line)
            return std::nullopt;

        return line->Load64(0);
    }

    void Core2Core::RetireCommits(uint64_t time) noexcept
    {
        for (auto it = pendingCommits.begin(); it != pendingCommits.end();)
        {
            auto* upstream = GetOwner().GetType1Upstream(it->upstreamIndex);

            auto line = upstream ? upstream->GetCacheLine(it->pa) : nullptr;

            if (!line)
            {
                // line reaped before the commit landed; count as a mismatch-level anomaly
                if (!pairs.empty())
                    pairs[currentPair].stats.mismatches++;

                it = pendingCommits.erase(it);
                continue;
            }

            if (line->Store64(0, it->value))
            {
                if (runMode3 && it->tag)
                    commitTimes[it->tag] = time;

                it = pendingCommits.erase(it);
                continue;
            }

            if (time >= it->expiryTime)
            {
                pairs[currentPair].stats.mismatches++;
                it = pendingCommits.erase(it);
                continue;
            }

            ++it;
        }
    }

    void Core2Core::BeginPair(uint64_t time) noexcept
    {
        auto& stats = pairs[currentPair].stats;

        begun            = true;
        phase            = Phase::M2_STORE_A;
        currentOp        = nullptr;
        iteration        = 0;
        sampleIteration  = 0;
        sampleLatencySum = 0;

        writerNext     = 1;
        readerNext     = 1;
        totalTransfers = warmup + samples * iterations;
        writerOps.clear();
        commitTimes.clear();
        pendingCommits.clear();

        stats.startTime = time;
    }

    void Core2Core::EndPair(uint64_t time) noexcept
    {
        auto& stats = pairs[currentPair].stats;

        // close a partially-filled final sample
        if (sampleIteration)
        {
            stats.sampleMeans.push_back(double(sampleLatencySum) / double(sampleIteration));
            sampleIteration  = 0;
            sampleLatencySum = 0;
        }

        stats.endTime = time;

        if (++currentPair >= pairs.size())
        {
            bool wantMode3 = std::find(bench.begin(), bench.end(), 3) != bench.end();

            if (!runMode3 && wantMode3)
            {
                // rerun all pairs in mode 3
                runMode3    = true;
                currentPair = 0;
            }
            else
            {
                finished = true;
                ReportResults();
                return;
            }
        }

        BeginPair(time);
    }

    void Core2Core::Tick(uint64_t time) noexcept
    {
        if (finished || pairs.empty())
            return;

        if (!begun)
            BeginPair(time);

        RetireCommits(time);

        if (runMode3)
            TickMode3(time);
        else
            TickMode2(time);
    }

    void Core2Core::TickMode2(uint64_t time) noexcept
    {
        Pair& pair   = pairs[currentPair];
        auto& stats  = pair.stats;

        const uint64_t lineX = pair.baseAddr;
        const uint64_t lineY = pair.baseAddr + 64;

        const bool recording = iteration >= warmup;

        // 1. complete the in-flight operation, if any
        if (currentOp)
        {
            if (currentOp->IsRejected())
            {
                // momentarily blocked (hazard/inflight limits): re-issue next tick
                currentOp = nullptr;
                return;
            }

            if (!currentOp->IsNow())
                return;

            const uint64_t latency = time - issueTime;

            switch (phase)
            {
                case Phase::M2_STORE_A:
                case Phase::M2_STORE_B:
                    if (recording)
                        stats.storeLatency.Record(latency);

                    if (pendingCommits.size() >= PENDING_COMMIT_LIMIT)
                    {
                        stats.mismatches++;
                    }
                    else
                    {
                        PendingCommit& commit = pendingCommits.emplace_back();
                        commit.upstreamIndex = phase == Phase::M2_STORE_A ? pair.a : pair.b;
                        commit.pa            = phase == Phase::M2_STORE_A ? lineX : lineY;
                        commit.value         = phaseValue;
                        commit.tag           = 0;
                        commit.expiryTime    = time + 10000;
                    }

                    phase = phase == Phase::M2_STORE_A ? Phase::M2_STORE_A_COMMIT
                                                       : Phase::M2_STORE_B_COMMIT;
                    break;

                case Phase::M2_LOAD_B:
                {
                    if (recording)
                    {
                        stats.loadLatency.Record(latency);
                        sampleLatencySum += latency;
                    }

                    // data-integrity spot check: b must observe a's committed value
                    auto observed = ReadLineValue(pair.b, lineX);
                    if (!observed || *observed != phaseValue)
                        stats.mismatches++;

                    phase = Phase::M2_STORE_B;
                    break;
                }

                case Phase::M2_LOAD_A:
                {
                    if (recording)
                        stats.iterationLatency.Record(time - iterStartTime);

                    auto observed = ReadLineValue(pair.a, lineY);
                    if (!observed || *observed != phaseValue)
                        stats.mismatches++;

                    iteration++;

                    if (recording)
                    {
                        sampleIteration++;

                        if (sampleIteration >= iterations)
                        {
                            stats.sampleMeans.push_back(double(sampleLatencySum) / double(sampleIteration));
                            sampleIteration  = 0;
                            sampleLatencySum = 0;
                        }
                    }

                    phase = Phase::M2_STORE_A;

                    if (iteration >= warmup + samples * iterations)
                    {
                        EndPair(time);
                        return;
                    }
                    break;
                }

                default:
                    break;
            }

            currentOp = nullptr;
        }

        // 2. issue the next operation
        switch (phase)
        {
            case Phase::M2_STORE_A:
                phaseValue = nextValue++;
                iterStartTime = time;
                currentOp = IssueStore(pair.a, lineX, time);
                break;

            case Phase::M2_STORE_A_COMMIT:
                // wait until the data commit of store A has actually landed
                if (!pendingCommits.empty())
                    return;
                currentOp = IssueLoad(pair.b, lineX, time);
                phase     = Phase::M2_LOAD_B;
                break;

            case Phase::M2_STORE_B:
                phaseValue = nextValue++;
                currentOp = IssueStore(pair.b, lineY, time);
                break;

            case Phase::M2_STORE_B_COMMIT:
                if (!pendingCommits.empty())
                    return;
                currentOp = IssueLoad(pair.a, lineY, time);
                phase     = Phase::M2_LOAD_A;
                break;

            default:
                break;
        }
    }

    void Core2Core::TickMode3(uint64_t time) noexcept
    {
        Pair& pair  = pairs[currentPair];
        auto& stats = pair.stats;

        const bool recording = readerNext > warmup;

        // 1. retire completed writer stores (grant -> queue data commit)
        while (!writerOps.empty() && writerOps.front().second->IsNow())
        {
            const uint64_t t = writerOps.front().first;

            if (pendingCommits.size() >= PENDING_COMMIT_LIMIT)
            {
                stats.mismatches++;
            }
            else
            {
                PendingCommit& commit = pendingCommits.emplace_back();
                commit.upstreamIndex = pair.a;
                commit.pa            = pair.baseAddr + (t % lines) * 64;
                commit.value         = STREAM_TAG | t;
                commit.tag           = t;
                commit.expiryTime    = time + 10000;
            }

            writerOps.pop_front();
        }

        // 2. issue writer stores within the in-flight and lead bounds
        while (writerOps.size() < inFlightLimit
            && writerNext < readerNext + lines          // never lap the reader
            && writerNext <= totalTransfers)
        {
            const uint64_t t = writerNext;

            auto* upstream = GetOwner().GetType1Upstream(pair.a);
            if (!upstream)
                return;

            auto future = upstream->DoStoreLine(pair.baseAddr + (t % lines) * 64);

            if (!future || future->IsRejected())
                break;  // blocked; retry next tick

            if (future->IsNow())
            {
                // granted immediately: queue the commit right away
                PendingCommit& commit = pendingCommits.emplace_back();
                commit.upstreamIndex = pair.a;
                commit.pa            = pair.baseAddr + (t % lines) * 64;
                commit.value         = STREAM_TAG | t;
                commit.tag           = t;
                commit.expiryTime    = time + 10000;

                if (pendingCommits.size() > PENDING_COMMIT_LIMIT)
                {
                    pendingCommits.pop_front();
                    stats.mismatches++;
                }
            }
            else
            {
                writerOps.emplace_back(t, std::move(future));
            }

            writerNext++;
        }

        // 3. drive the reader
        if (currentOp)
        {
            if (currentOp->IsRejected())
            {
                currentOp = nullptr;
            }
            else if (currentOp->IsNow())
            {
                const uint64_t k = readerNext;

                auto observed = ReadLineValue(pair.b, pair.baseAddr + (k % lines) * 64);

                currentOp = nullptr;

                // accept only tagged stream values; anything else is stale data
                // (zero-init memory, leftovers from other phases/rounds)
                const uint64_t seen = (observed && (*observed >> 32) == (STREAM_TAG >> 32))
                    ? uint32_t(*observed) : 0;

                if (seen >= k)
                {
                    if (seen > k)
                    {
                        // writer overwrote a value before the reader consumed it
                        stats.lapped++;
                        readerNext = seen;
                    }

                    auto commitIt = commitTimes.find(readerNext);

                    if (commitIt != commitTimes.end())
                    {
                        if (recording)
                            stats.transferLatency.Record(time - commitIt->second);

                        commitTimes.erase(commitIt);
                    }
                    else if (recording)
                    {
                        // commit time entry lost (should not happen): count as anomaly
                        stats.mismatches++;
                    }

                    readerNext++;

                    if (readerNext > totalTransfers)
                    {
                        EndPair(time);
                        return;
                    }
                }
                // else: stale data (writer's invalidation not visible yet): re-poll next tick
            }
        }
        else if (readerNext <= totalTransfers)
        {
            currentOp = IssueLoad(pair.b, pair.baseAddr + (readerNext % lines) * 64, time);

            if (currentOp && currentOp->IsRejected())
                currentOp = nullptr;
        }
    }

    void Core2Core::ReportResults() noexcept
    {
        if (reported)
            return;

        reported = true;

        auto logger = GetOwner().GetLogger();

        const bool ranMode2 = std::find(bench.begin(), bench.end(), 2) != bench.end();
        const bool ranMode3 = std::find(bench.begin(), bench.end(), 3) != bench.end();

        // ---- console matrix (mode 2 one-way latencies, upstream style) ----
        if (ranMode2)
        {
            std::ostringstream oss;

            oss << "Core-to-core one-way latency matrix (ticks; cell (from,to) = mean of load latency):\n";
            oss << std::setw(8) << "from\\to";
            for (uint32_t node : nodes)
                oss << std::setw(10) << node;
            oss << "\n";

            for (uint32_t a : nodes)
            {
                oss << std::setw(8) << a;

                for (uint32_t b : nodes)
                {
                    if (a == b)
                    {
                        oss << std::setw(10) << "-";
                        continue;
                    }

                    auto it = std::find_if(pairs.begin(), pairs.end(),
                        [a, b](const Pair& p) { return p.a == a && p.b == b; });

                    if (it == pairs.end() || !it->stats.loadLatency.GetCount())
                        oss << std::setw(10) << "?";
                    else
                    {
                        oss << std::setw(10) << std::fixed << std::setprecision(1)
                            << it->stats.loadLatency.GetMean();
                    }
                }

                oss << "\n";
            }

            logger->info("\n{}", oss.str());
        }

        // ---- per-pair details to console ----
        for (const auto& pair : pairs)
        {
            const auto& stats = pair.stats;

            if (ranMode2 && stats.loadLatency.GetCount())
            {
                double sampleMedian = 0.0;

                if (!stats.sampleMeans.empty())
                {
                    auto sorted = stats.sampleMeans;
                    std::sort(sorted.begin(), sorted.end());
                    sampleMedian = sorted[sorted.size() / 2];
                }

                logger->info(
                    "Core2Core mode 2 pair {}->{}: one-way(load) p50={} p95={} min={} max={} mean={:.2f} ticks "
                    "(sample-mean median {:.2f}), store p50={} p95={}, iteration p50={} p95={}, "
                    "{} iteration(s), {} mismatch(es).",
                    pair.a, pair.b,
                    stats.loadLatency.GetPercentile(50.0), stats.loadLatency.GetPercentile(95.0),
                    stats.loadLatency.GetMin(), stats.loadLatency.GetMax(), stats.loadLatency.GetMean(),
                    sampleMedian,
                    stats.storeLatency.GetPercentile(50.0), stats.storeLatency.GetPercentile(95.0),
                    stats.iterationLatency.GetPercentile(50.0), stats.iterationLatency.GetPercentile(95.0),
                    stats.loadLatency.GetCount(), stats.mismatches);
            }

            if (ranMode3 && stats.transferLatency.GetCount())
            {
                const uint64_t span = stats.endTime > stats.startTime ? stats.endTime - stats.startTime : 1;
                const double   bytes = double(stats.transferLatency.GetCount()) * 64.0;

                logger->info(
                    "Core2Core mode 3 pair {}->{}: transfer p50={} p95={} min={} max={} mean={:.2f} ticks, "
                    "{} transfer(s), {:.2f} bytes/tick sustained, {} lapped, {} mismatch(es).",
                    pair.a, pair.b,
                    stats.transferLatency.GetPercentile(50.0), stats.transferLatency.GetPercentile(95.0),
                    stats.transferLatency.GetMin(), stats.transferLatency.GetMax(), stats.transferLatency.GetMean(),
                    stats.transferLatency.GetCount(), bytes / double(span),
                    stats.lapped, stats.mismatches);
            }
        }

        // ---- CSV result files ----
        if (!reportCSV.empty())
        {
            // matrix CSV (mode 2), mirroring the upstream --csv layout:
            // first column = source, one column per destination, cells = mean latency
            if (ranMode2)
            {
                std::ofstream file(reportCSV);

                if (file)
                {
                    file << "from\\to";
                    for (uint32_t node : nodes)
                        file << "," << node;
                    file << "\n";

                    for (uint32_t a : nodes)
                    {
                        file << a;

                        for (uint32_t b : nodes)
                        {
                            file << ",";

                            if (a == b)
                                continue;

                            auto it = std::find_if(pairs.begin(), pairs.end(),
                                [a, b](const Pair& p) { return p.a == a && p.b == b; });

                            if (it != pairs.end() && it->stats.loadLatency.GetCount())
                                file << std::fixed << std::setprecision(2) << it->stats.loadLatency.GetMean();
                        }

                        file << "\n";
                    }
                }
                else
                    logger->error("Core2Core: failed to write CSV file: {}", reportCSV);
            }

            // detailed long-form CSV (both modes, full percentiles)
            const std::string detailPath = [this]() {
                const std::string suffix = ".csv";
                if (reportCSV.size() > suffix.size()
                 && reportCSV.compare(reportCSV.size() - suffix.size(), suffix.size(), suffix) == 0)
                    return reportCSV.substr(0, reportCSV.size() - suffix.size()) + ".detailed.csv";
                return reportCSV + ".detailed.csv";
            }();

            std::ofstream detail(detailPath);

            if (detail)
            {
                detail << "mode,from,to,metric,count,min,p25,p50,p75,p95,p99,max,mean\n";

                auto writeHist = [&](uint32_t mode, const Pair& pair,
                                     const char* metric, const CCHI::Prof::Histogram& hist) {
                    if (!hist.GetCount())
                        return;

                    detail << mode << "," << pair.a << "," << pair.b << "," << metric << ","
                           << hist.GetCount() << ","
                           << hist.GetMin() << ","
                           << hist.GetPercentile(25.0) << ","
                           << hist.GetPercentile(50.0) << ","
                           << hist.GetPercentile(75.0) << ","
                           << hist.GetPercentile(95.0) << ","
                           << hist.GetPercentile(99.0) << ","
                           << hist.GetMax() << ","
                           << std::fixed << std::setprecision(4) << hist.GetMean() << "\n";
                };

                for (const auto& pair : pairs)
                {
                    if (ranMode2)
                    {
                        writeHist(2, pair, "oneway_load",  pair.stats.loadLatency);
                        writeHist(2, pair, "store",        pair.stats.storeLatency);
                        writeHist(2, pair, "iteration",    pair.stats.iterationLatency);
                    }

                    if (ranMode3)
                        writeHist(3, pair, "transfer",     pair.stats.transferLatency);
                }
            }
            else
                logger->error("Core2Core: failed to write CSV file: {}", detailPath);
        }

        // ---- SVG heatmap (mode 2 matrix, same cell semantics as the CSV) ----
        if (ranMode2 && !reportSVG.empty())
            WriteSVGHeatmap(reportSVG);
    }

    void Core2Core::WriteSVGHeatmap(const std::string& path) const noexcept
    {
        auto logger = GetOwner().GetLogger();

        // collect matrix values (mean one-way load latency per ordered pair)
        double minV = 0.0, maxV = 0.0;
        bool   any  = false;

        for (const auto& pair : pairs)
        {
            if (!pair.stats.loadLatency.GetCount())
                continue;

            const double v = pair.stats.loadLatency.GetMean();

            if (!any)
            {
                minV = maxV = v;
                any   = true;
            }
            else
            {
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }
        }

        const size_t n      = nodes.size();
        const int    cell   = 46;
        const int    left   = 90;
        const int    top    = 76;
        const int    gridW  = int(n) * cell;
        const int    legendH = 54;
        const int    width  = std::max(left + gridW + 24, 760);   // keep title/subtitle inside
        const int    height = top + gridW + legendH + 16;

        // ColorBrewer "Blues" anchors: light -> dark
        auto colorFor = [&](double t, int& r, int& g, int& b) {
            // 9-stop Blues ramp sampled at t in [0,1]
            static constexpr int stops[9][3] = {
                {247, 251, 255}, {222, 235, 247}, {198, 219, 239},
                {158, 202, 225}, {107, 174, 214}, { 66, 146, 198},
                { 33, 113, 181}, {  8,  81, 156}, {  8,  48, 107}
            };

            const double pos = t * 8.0;
            const size_t i   = std::min<size_t>(size_t(pos), 7);
            const double f   = pos - double(i);

            r = int(stops[i][0] + (stops[i + 1][0] - stops[i][0]) * f);
            g = int(stops[i][1] + (stops[i + 1][1] - stops[i][1]) * f);
            b = int(stops[i][2] + (stops[i + 1][2] - stops[i][2]) * f);
        };

        std::ostringstream svg;

        svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
            << "\" height=\"" << height << "\" font-family=\"monospace\">\n";

        // title
        svg << "<text x=\"" << left << "\" y=\"24\" font-size=\"15\" font-weight=\"bold\">"
            << "Core-to-core one-way latency (ticks) - mean of load latency</text>\n";
        svg << "<text x=\"" << left << "\" y=\"42\" font-size=\"11\" fill=\"#555\">"
            << "rows: from (writer), columns: to (reader); nviennot/core-to-core-latency matrix semantics</text>\n";

        // column headers
        for (size_t j = 0; j < n; j++)
            svg << "<text x=\"" << left + int(j) * cell + cell / 2 - 4 << "\" y=\"" << top - 8
                << "\" font-size=\"12\" text-anchor=\"middle\">" << nodes[j] << "</text>\n";

        // grid
        for (size_t i = 0; i < n; i++)
        {
            // row header
            svg << "<text x=\"" << left - 10 << "\" y=\"" << top + int(i) * cell + cell / 2 + 4
                << "\" font-size=\"12\" text-anchor=\"end\">" << nodes[i] << "</text>\n";

            for (size_t j = 0; j < n; j++)
            {
                const int x = left + int(j) * cell;
                const int y = top + int(i) * cell;

                if (i == j)
                {
                    svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << cell
                        << "\" height=\"" << cell << "\" fill=\"#e8e8e8\" stroke=\"#ffffff\"/>\n";
                    svg << "<text x=\"" << x + cell / 2 << "\" y=\"" << y + cell / 2 + 4
                        << "\" font-size=\"12\" text-anchor=\"middle\" fill=\"#999\">-</text>\n";
                    continue;
                }

                auto it = std::find_if(pairs.begin(), pairs.end(),
                    [&](const Pair& p) { return p.a == nodes[i] && p.b == nodes[j]; });

                if (it == pairs.end() || !it->stats.loadLatency.GetCount())
                {
                    svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << cell
                        << "\" height=\"" << cell << "\" fill=\"#f4f4f4\" stroke=\"#ffffff\"/>\n";
                    svg << "<text x=\"" << x + cell / 2 << "\" y=\"" << y + cell / 2 + 4
                        << "\" font-size=\"12\" text-anchor=\"middle\" fill=\"#aaa\">?</text>\n";
                    continue;
                }

                const double v = it->stats.loadLatency.GetMean();
                const double t = (any && maxV > minV) ? (v - minV) / (maxV - minV) : 0.5;

                int r, g, b;
                colorFor(t, r, g, b);

                char fill[8];
                std::snprintf(fill, sizeof(fill), "#%02x%02x%02x", r, g, b);

                svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << cell
                    << "\" height=\"" << cell << "\" fill=\"" << fill << "\" stroke=\"#ffffff\"/>\n";

                svg << "<text x=\"" << x + cell / 2 << "\" y=\"" << y + cell / 2 + 4
                    << "\" font-size=\"11\" text-anchor=\"middle\" fill=\""
                    << (t > 0.55 ? "#ffffff" : "#1a1a1a") << "\">"
                    << std::fixed << std::setprecision(1) << v << "</text>\n";
            }
        }

        // legend: gradient bar + min/max labels
        {
            const int lx = left;
            const int ly = top + gridW + 18;
            const int lw = gridW;
            const int lh = 12;
            const int steps = 64;

            for (int s = 0; s < steps; s++)
            {
                int r, g, b;
                colorFor(double(s) / double(steps - 1), r, g, b);

                char fill[8];
                std::snprintf(fill, sizeof(fill), "#%02x%02x%02x", r, g, b);

                svg << "<rect x=\"" << lx + s * lw / steps << "\" y=\"" << ly
                    << "\" width=\"" << lw / steps + 1 << "\" height=\"" << lh
                    << "\" fill=\"" << fill << "\"/>\n";
            }

            svg << "<text x=\"" << lx << "\" y=\"" << ly + lh + 14 << "\" font-size=\"11\">"
                << std::fixed << std::setprecision(1) << minV << "</text>\n";
            svg << "<text x=\"" << lx + lw << "\" y=\"" << ly + lh + 14
                << "\" font-size=\"11\" text-anchor=\"end\">"
                << std::fixed << std::setprecision(1) << maxV << "</text>\n";
        }

        svg << "</svg>\n";

        std::ofstream file(path);
        if (file)
            file << svg.str();
        else
            logger->error("Core2Core: failed to write SVG heatmap: {}", path);
    }
}


// Implementation of: class Core2CoreFactory
namespace Cohestra {

    Core2CoreFactory::Core2CoreFactory() noexcept
        : StimulatorFactory("Core2Core", "Core-to-core latency benchmark (replication of nviennot/core-to-core-latency)")
    { }

    std::unique_ptr<Stimulator> Core2CoreFactory::Create(Instance&                    instance,
                                                         size_t                       index,
                                                         const std::vector<uint32_t>& type1Upstreams) noexcept
    {
        return std::make_unique<Core2Core>(instance, index, type1Upstreams);
    }
}
