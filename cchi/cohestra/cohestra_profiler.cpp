#include "cohestra_profiler.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "cohestra_instance.hpp"


// Implementation of: class Profiler
namespace Cohestra {

    const ConfigurationEntryBack<bool> Profiler::PROFILER_ENABLE
        ("PROFILER_ENABLE", "enable", "profiler", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> Profiler::PROFILER_WINDOW
        ("PROFILER_WINDOW", "window", "profiler", 1024, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<std::string> Profiler::PROFILER_REPORT_JSON
        ("PROFILER_REPORT_JSON", "report.json", "profiler", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<std::string> Profiler::PROFILER_REPORT_CSV_DIR
        ("PROFILER_REPORT_CSV_DIR", "report.csv.dir", "profiler", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Profiler::PROFILER_REPORT_TEXT
        ("PROFILER_REPORT_TEXT", "report.text", "profiler", true, DeserializeBool, SerializeBool);


    Profiler::Profiler(Instance& instance) noexcept
        : instance  (instance)
    { }

    Profiler::~Profiler() noexcept
    {
        Detach();
    }

    template<class T>
    bool Profiler::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
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

    bool Profiler::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        return LoadConfigurationEntry(loader, &PROFILER_ENABLE,         enabled)
            && LoadConfigurationEntry(loader, &PROFILER_WINDOW,         window)
            && LoadConfigurationEntry(loader, &PROFILER_REPORT_JSON,    reportJSON)
            && LoadConfigurationEntry(loader, &PROFILER_REPORT_CSV_DIR, reportCSVDir)
            && LoadConfigurationEntry(loader, &PROFILER_REPORT_TEXT,    reportText);
    }

    void Profiler::SetTicksPerCycle(unsigned ticksPerCycle) noexcept
    {
        this->ticksPerCycle = ticksPerCycle ? ticksPerCycle : 1;
    }

    Profiler::NodeContext* Profiler::ContextOf(const TaurusNode& node) const noexcept
    {
        auto it = nodeLookup.find(&node);
        return it != nodeLookup.end() ? it->second : nullptr;
    }

    Profiler::NodeContext& Profiler::RequireNode(size_t index)
    {
        if (index >= nodes.size())
            nodes.resize(index + 1);

        auto& slot = nodes[index];

        if (!slot)
        {
            slot = std::make_unique<NodeContext>();
            slot->index = index;
        }

        return *slot;
    }

    void Profiler::Attach() noexcept
    {
        if (attached || !enabled)
            return;

        JointProfiler::Config jointConfig;
        jointConfig.seriesWindowTicks = window;

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            // joint-level profiler (families A/C/D), one per node
            NodeContext& ctx = RequireNode(index);
            ctx.joint = std::make_unique<JointProfiler>(jointConfig);
            ctx.joint->Attach(upstream->GetJoint());

            nodeLookup[upstream] = &ctx;

            // taurus-level listeners (families B/D/E); a harness may share one
            // event hub between all upstream nodes - register only once per hub
            if (!upstream->events)
                continue;

            if (std::find(attachedHubs.begin(), attachedHubs.end(), upstream->events) != attachedHubs.end())
                continue;

            auto hub = upstream->events;

            hub->OnAcceptedEVT  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedEVT, this));
            hub->OnAcceptedSNP  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedSNP, this));
            hub->OnAcceptedREQ  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedREQ, this));
            hub->OnAcceptedPrefetch.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedPrefetch, this));
            hub->OnAcceptedDnRSP.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedDnRSP, this));
            hub->OnAcceptedUpRSP.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedUpRSP, this));
            hub->OnAcceptedDnDAT.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedDnDAT, this));
            hub->OnAcceptedUpDAT.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnAcceptedUpDAT, this));

            hub->OnDeniedEVT    .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedEVT, this));
            hub->OnDeniedSNP    .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedSNP, this));
            hub->OnDeniedREQ    .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedREQ, this));
            hub->OnDeniedDnRSP  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedDnRSP, this));
            hub->OnDeniedUpRSP  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedUpRSP, this));
            hub->OnDeniedDnDAT  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedDnDAT, this));
            hub->OnDeniedUpDAT  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedUpDAT, this));
            hub->OnDeniedPrefetch.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedPrefetch, this));
            hub->OnDeniedCMO    .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedCMO, this));
            hub->OnDeniedCompCMO.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnDeniedCompCMO, this));

            hub->OnCacheStateMapDeniedRequest .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnCacheStateMapDeniedRequest, this));
            hub->OnCacheStateMapDeniedResponse.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnCacheStateMapDeniedResponse, this));

            hub->OnEVTCacheStatePostDemotion.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnEVTDemotion, this));
            hub->OnSNPCacheStatePostDemotion.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnSNPDemotion, this));

            hub->OnCacheLineGranted  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnCacheLineGranted, this));
            hub->OnCacheLinePostLoad .Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnCacheLinePostLoad, this));
            hub->OnCacheLinePostStore.Register(Gravity::MakeListener(LISTENER_NAME, 0, &Profiler::OnCacheLinePostStore, this));

            attachedHubs.push_back(upstream->events);
        }

        attached = true;
    }

    void Profiler::Detach() noexcept
    {
        for (auto& ctx : nodes)
            if (ctx && ctx->joint)
                ctx->joint->Detach();

        for (auto& hub : attachedHubs)
        {
            hub->OnAcceptedEVT  .Unregister(LISTENER_NAME);
            hub->OnAcceptedSNP  .Unregister(LISTENER_NAME);
            hub->OnAcceptedREQ  .Unregister(LISTENER_NAME);
            hub->OnAcceptedPrefetch.Unregister(LISTENER_NAME);
            hub->OnAcceptedDnRSP.Unregister(LISTENER_NAME);
            hub->OnAcceptedUpRSP.Unregister(LISTENER_NAME);
            hub->OnAcceptedDnDAT.Unregister(LISTENER_NAME);
            hub->OnAcceptedUpDAT.Unregister(LISTENER_NAME);

            hub->OnDeniedEVT    .Unregister(LISTENER_NAME);
            hub->OnDeniedSNP    .Unregister(LISTENER_NAME);
            hub->OnDeniedREQ    .Unregister(LISTENER_NAME);
            hub->OnDeniedDnRSP  .Unregister(LISTENER_NAME);
            hub->OnDeniedUpRSP  .Unregister(LISTENER_NAME);
            hub->OnDeniedDnDAT  .Unregister(LISTENER_NAME);
            hub->OnDeniedUpDAT  .Unregister(LISTENER_NAME);
            hub->OnDeniedPrefetch.Unregister(LISTENER_NAME);
            hub->OnDeniedCMO    .Unregister(LISTENER_NAME);
            hub->OnDeniedCompCMO.Unregister(LISTENER_NAME);

            hub->OnCacheStateMapDeniedRequest .Unregister(LISTENER_NAME);
            hub->OnCacheStateMapDeniedResponse.Unregister(LISTENER_NAME);

            hub->OnEVTCacheStatePostDemotion.Unregister(LISTENER_NAME);
            hub->OnSNPCacheStatePostDemotion.Unregister(LISTENER_NAME);

            hub->OnCacheLineGranted  .Unregister(LISTENER_NAME);
            hub->OnCacheLinePostLoad .Unregister(LISTENER_NAME);
            hub->OnCacheLinePostStore.Unregister(LISTENER_NAME);
        }

        attachedHubs.clear();
        nodeLookup.clear();
        attached = false;
    }

    bool Profiler::IsAttached() const noexcept
    {
        return attached;
    }

    bool Profiler::IsEnabled() const noexcept
    {
        return enabled;
    }

    void Profiler::CountChannel(NodeContext& ctx, const char* channel, uint64_t time, bool data, bool tx) noexcept
    {
        static constexpr uint64_t DATA_BYTES = CommonFlitConfigurationType1::dataWidth / 8;

        ctx.agent.C(std::string("chan.") + channel + ".flits").Inc();
        ctx.agent.S(std::string("chan.") + channel + ".flits.ts", window).Add(time, 1);

        if (data)
        {
            ctx.agent.C(std::string("chan.") + channel + ".bytes").Add(DATA_BYTES);
            ctx.agent.S(std::string("chan.") + channel + ".bytes.ts", window).Add(time, DATA_BYTES);
        }

        // traffic-mix split: upstream-originated requests vs downstream responses
        ctx.agent.C(tx ? "chan.tx.flits" : "chan.rx.flits").Inc();
    }

    void Profiler::OnAcceptedEVT(CCHI::Taurus::UpstreamNodeXactAcceptedEVTEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "evt", event.GetUpstream().time, false, true);
    }

    void Profiler::OnAcceptedSNP(CCHI::Taurus::UpstreamNodeXactAcceptedSNPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "snp", event.GetUpstream().time, false, false);
    }

    void Profiler::OnAcceptedREQ(CCHI::Taurus::UpstreamNodeXactAcceptedREQEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "req", event.GetUpstream().time, false, true);
    }

    // Prefetch (StashShared/StashUnique) rides a dedicated emission queue and
    // does not fire OnAcceptedREQ; it is still a REQ-channel flit on the wire.
    void Profiler::OnAcceptedPrefetch(CCHI::Taurus::UpstreamNodeXactAcceptedPrefetchEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "req", event.GetUpstream().time, false, true);
    }

    void Profiler::OnAcceptedDnRSP(CCHI::Taurus::UpstreamNodeXactAcceptedDnRSPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "dnrsp", event.GetUpstream().time, false, false);
    }

    void Profiler::OnAcceptedUpRSP(CCHI::Taurus::UpstreamNodeXactAcceptedUpRSPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "uprsp", event.GetUpstream().time, false, true);
    }

    void Profiler::OnAcceptedDnDAT(CCHI::Taurus::UpstreamNodeXactAcceptedDnDATEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "dndat", event.GetUpstream().time, true, false);
    }

    void Profiler::OnAcceptedUpDAT(CCHI::Taurus::UpstreamNodeXactAcceptedUpDATEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            CountChannel(*ctx, "updat", event.GetUpstream().time, true, true);
    }

    void Profiler::OnDeniedEVT(CCHI::Taurus::UpstreamNodeXactDeniedEVTEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.evt").Inc();
    }

    void Profiler::OnDeniedSNP(CCHI::Taurus::UpstreamNodeXactDeniedSNPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.snp").Inc();
    }

    void Profiler::OnDeniedREQ(CCHI::Taurus::UpstreamNodeXactDeniedREQEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.req").Inc();
    }

    void Profiler::OnDeniedDnRSP(CCHI::Taurus::UpstreamNodeXactDeniedDnRSPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.dnrsp").Inc();
    }

    void Profiler::OnDeniedUpRSP(CCHI::Taurus::UpstreamNodeXactDeniedUpRSPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.uprsp").Inc();
    }

    void Profiler::OnDeniedDnDAT(CCHI::Taurus::UpstreamNodeXactDeniedDnDATEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.dndat").Inc();
    }

    void Profiler::OnDeniedUpDAT(CCHI::Taurus::UpstreamNodeXactDeniedUpDATEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.updat").Inc();
    }

    void Profiler::OnDeniedPrefetch(CCHI::Taurus::UpstreamNodeXactDeniedPrefetchEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.prefetch").Inc();
    }

    void Profiler::OnDeniedCMO(CCHI::Taurus::UpstreamNodeXactDeniedCMOEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.cmo").Inc();
    }

    void Profiler::OnDeniedCompCMO(CCHI::Taurus::UpstreamNodeXactDeniedCompCMOEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("denied.compcmo").Inc();
    }

    void Profiler::OnCacheStateMapDeniedRequest(CCHI::Taurus::UpstreamNodeCacheStateMapDeniedRequestEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("csm.denied.request").Inc();
    }

    void Profiler::OnCacheStateMapDeniedResponse(CCHI::Taurus::UpstreamNodeCacheStateMapDeniedResponseEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("csm.denied.response").Inc();
    }

    void Profiler::OnEVTDemotion(CCHI::Taurus::UpstreamNodeEVTCacheStatePostDemotionEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
        {
            ctx->agent.C(std::string("demote.evt.")
                + (event.GetPrevState() ? event.GetPrevState()->name : "?")
                + "2"
                + (event.GetNextState() ? event.GetNextState()->name : "?")).Inc();
        }
    }

    void Profiler::OnSNPDemotion(CCHI::Taurus::UpstreamNodeSNPCacheStatePostDemotionEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
        {
            ctx->agent.C(std::string("demote.snp.")
                + (event.GetPrevState() ? event.GetPrevState()->name : "?")
                + "2"
                + (event.GetNextState() ? event.GetNextState()->name : "?")).Inc();
        }
    }

    void Profiler::OnCacheLineGranted(CCHI::Taurus::UpstreamNodeCacheLineGrantedEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("cacheline.grants").Inc();
    }

    void Profiler::OnCacheLinePostLoad(CCHI::Taurus::UpstreamNodeCacheLinePostLoadEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("cacheline.loads").Inc();
    }

    void Profiler::OnCacheLinePostStore(CCHI::Taurus::UpstreamNodeCacheLinePostStoreEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        if (auto* ctx = ContextOf(event.GetUpstream()))
            ctx->agent.C("cacheline.stores").Inc();
    }

    void Profiler::Report(uint64_t elapsedTicks) const noexcept
    {
        if (!enabled || !attached)
            return;

        auto logger = instance.GetLogger();

        // merged system-wide view (unprefixed names = aggregated over all nodes)
        CCHI::Prof::MetricSet global;

        for (const auto& ctx : nodes)
        {
            if (!ctx)
                continue;

            if (ctx->joint)
                global.MergeFrom(ctx->joint->Metrics());

            global.MergeFrom(ctx->agent);
        }

        CCHI::Prof::ReportContext context;
        context.ticksPerCycle = ticksPerCycle;
        context.elapsedTicks  = elapsedTicks;

        // ---- text report ----
        if (reportText)
        {
            context.title = "CCHI Profiler - global (all nodes merged)";
            logger->info("\n{}", CCHI::Prof::Report::ToText(global, context));

            for (const auto& ctx : nodes)
            {
                if (!ctx)
                    continue;

                context.title = "CCHI Profiler - node " + std::to_string(ctx->index) + " (joint)";
                if (ctx->joint)
                    logger->info("\n{}", CCHI::Prof::Report::ToText(ctx->joint->Metrics(), context));

                context.title = "CCHI Profiler - node " + std::to_string(ctx->index) + " (agent)";
                logger->info("\n{}", CCHI::Prof::Report::ToText(ctx->agent, context));
            }

            // derived bandwidth headline (ticks_per_cycle converts to cycles)
            if (elapsedTicks)
            {
                const double cycles = double(elapsedTicks) / double(ticksPerCycle);

                for (const auto& ctx : nodes)
                {
                    if (!ctx)
                        continue;

                    uint64_t flits = 0, bytes = 0;

                    for (const auto& name : ctx->agent.ListCounters())
                    {
                        const CCHI::Prof::Counter* c = ctx->agent.GetCounter(name);

                        if (!c)
                            continue;

                        if (name.rfind("chan.", 0) == 0 && name.size() > 6
                         && name.compare(name.size() - 6, 6, ".flits") == 0)
                            flits += c->Get();

                        if (name.rfind("chan.", 0) == 0 && name.size() > 6
                         && name.compare(name.size() - 6, 6, ".bytes") == 0)
                            bytes += c->Get();
                    }

                    logger->info("CCHI Profiler - node {} bandwidth: {} flit(s), {} byte(s) over {} tick(s) "
                        "-> {:.4f} flits/cycle, {:.4f} bytes/cycle ({} ticks/cycle)",
                        ctx->index, flits, bytes, elapsedTicks,
                        cycles ? double(flits) / cycles : 0.0,
                        cycles ? double(bytes) / cycles : 0.0,
                        ticksPerCycle);
                }
            }
        }

        // ---- JSON report ----
        if (!reportJSON.empty())
        {
            std::ostringstream oss;

            oss << "{\n";
            oss << "  \"ticks_per_cycle\": " << ticksPerCycle << ",\n";
            oss << "  \"elapsed_ticks\": " << elapsedTicks << ",\n";

            context.title = "global";
            oss << "  \"global\": " << CCHI::Prof::Report::ToJSON(global, context);

            oss << ",\n  \"nodes\": [";

            bool firstNode = true;
            for (const auto& ctx : nodes)
            {
                if (!ctx)
                    continue;

                oss << (firstNode ? "\n" : ",\n");
                oss << "    {\"index\": " << ctx->index;

                context.title = "joint";
                oss << ", \"joint\": " << (ctx->joint
                    ? CCHI::Prof::Report::ToJSON(ctx->joint->Metrics(), context)
                    : std::string("{}"));

                context.title = "agent";
                oss << ", \"agent\": " << CCHI::Prof::Report::ToJSON(ctx->agent, context);

                oss << "}";
                firstNode = false;
            }

            oss << (firstNode ? "" : "\n  ") << "]\n}\n";

            std::ofstream file(reportJSON);
            if (file)
                file << oss.str();
            else
                logger->error("CCHI Profiler: failed to open JSON report file: {}", reportJSON);
        }

        // ---- CSV dumps ----
        if (!reportCSVDir.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(reportCSVDir, ec);

            if (ec)
            {
                logger->error("CCHI Profiler: failed to create CSV directory {}: {}",
                    reportCSVDir, ec.message());
            }
            else
            {
                auto writeFile = [&](const std::string& name, const std::string& content) {
                    std::ofstream file(std::filesystem::path(reportCSVDir) / name);
                    if (file)
                        file << content;
                    else
                        logger->error("CCHI Profiler: failed to write CSV file: {}", name);
                };

                writeFile("global.counters.csv",   CCHI::Prof::Report::CountersToCSV(global));
                writeFile("global.histograms.csv", CCHI::Prof::Report::HistogramsToCSV(global));
                writeFile("global.series.csv",     CCHI::Prof::Report::SeriesToCSV(global));

                for (const auto& ctx : nodes)
                {
                    if (!ctx)
                        continue;

                    const std::string prefix = "node" + std::to_string(ctx->index);

                    if (ctx->joint)
                    {
                        writeFile(prefix + ".joint.counters.csv",   CCHI::Prof::Report::CountersToCSV(ctx->joint->Metrics()));
                        writeFile(prefix + ".joint.histograms.csv", CCHI::Prof::Report::HistogramsToCSV(ctx->joint->Metrics()));
                        writeFile(prefix + ".joint.series.csv",     CCHI::Prof::Report::SeriesToCSV(ctx->joint->Metrics()));
                    }

                    writeFile(prefix + ".agent.counters.csv", CCHI::Prof::Report::CountersToCSV(ctx->agent));
                    writeFile(prefix + ".agent.series.csv",   CCHI::Prof::Report::SeriesToCSV(ctx->agent));
                }
            }
        }
    }
}
