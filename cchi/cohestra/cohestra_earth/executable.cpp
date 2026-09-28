#include <memory>
#include <vector>

#include "arguments.hpp"
#include "earth_interface.hpp"
#include "earth_model.hpp"

#include "../cohestra_logo.hpp"
#include "../cohestra_instance.hpp"
#include "../cohestra_monitor.hpp"
#include "../cohestra_profiler.hpp"
#include "../cohestra_simperf.hpp"
#include "../cohestra_stimulators/fuzz_ari.hpp"
#include "../cohestra_stimulators/core2core.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/details/fmt_helper.h"
#include "spdlog/details/os.h"


namespace
{
#ifndef _WIN32
    // Level tag flag formatter: prints the level name in a darker color with
    // brackets in plain color, and restores the lighter message color afterwards.
    class DeepLevelTagFormatter final : public spdlog::custom_flag_formatter
    {
    public:
        void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
        {
            const char* tagColor = "\033[37m";
            const char* messageColor = "\033[0m";

            switch (msg.level)
            {
            case spdlog::level::warn:
                tagColor = "\033[33m";      // darker yellow
                messageColor = "\033[93m";  // lighter yellow
                break;

            case spdlog::level::err:
                tagColor = "\033[31m";      // darker red
                messageColor = "\033[91m";  // lighter red
                break;

            case spdlog::level::critical:
                tagColor = "\033[1m\033[41m";
                break;

            case spdlog::level::info:
                tagColor = "\033[32m";
                break;

            case spdlog::level::debug:
                tagColor = "\033[36m";
                break;

            default:
                break;
            }

            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t("\033[0m"), dest);
            dest.push_back('[');
            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t(tagColor), dest);
            spdlog::details::fmt_helper::append_string_view(spdlog::level::to_string_view(msg.level), dest);
            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t("\033[0m"), dest);
            dest.push_back(']');
            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t(messageColor), dest);
        }

        std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
        {
            return std::make_unique<DeepLevelTagFormatter>();
        }
    };
#endif

    template<class T>
    void ConfigureColorSink(const std::shared_ptr<T>& sink, FILE* file) noexcept
    {
#ifdef _WIN32
        (void) file;

        sink->set_color(spdlog::level::warn, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        sink->set_color(spdlog::level::err, FOREGROUND_RED | FOREGROUND_INTENSITY);

        // color the whole message, not only the level tag
        sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] %v%$");
#else
        sink->set_color(spdlog::level::warn, "\033[93m"); // light yellow
        sink->set_color(spdlog::level::err, "\033[91m");  // light red

        // color the whole message, with a deeper color for the level tag
        // and the origin color for the timestamp
        if (spdlog::details::os::in_terminal(file) && spdlog::details::os::is_color_terminal())
        {
            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->add_flag<DeepLevelTagFormatter>('*').set_pattern(
                "%^\033[0m[%Y-%m-%d %H:%M:%S.%e] %* %v%$");
            sink->set_formatter(std::move(formatter));
        }
#endif
    }

    void ConfigureLogColors() noexcept
    {
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
            if (!logger)
                return;

            for (const auto& sink : logger->sinks())
            {
                if (auto colorSink = std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(sink))
                {
                    ConfigureColorSink(colorSink, stdout);
                }
                else if (auto colorSink = std::dynamic_pointer_cast<spdlog::sinks::stderr_color_sink_mt>(sink))
                {
                    ConfigureColorSink(colorSink, stderr);
                }
            }
        });
    }

    // System address map directing all upstream traffic to the Earth model
    // downstream node ID.
    class EarthSAM : public CCHI::Taurus::SAM<Cohestra::CommonFlitConfigurationType1> {
    protected:
        CCHI::Flits::dn_nodeid_t<Cohestra::CommonFlitConfigurationType1> nodeID;

    public:
        EarthSAM(CCHI::Flits::dn_nodeid_t<Cohestra::CommonFlitConfigurationType1> nodeID) noexcept
            : nodeID(nodeID)
        { }

    public:
        virtual CCHI::Flits::dn_nodeid_t<Cohestra::CommonFlitConfigurationType1>
        Map(CCHI::Flits::REQ<Cohestra::CommonFlitConfigurationType1>::addr_t PA) const noexcept override
        {
            (void) PA;
            return nodeID;
        }
    };

    // Model pointer captured so OnUpstreamInitialized can install the SAM.
    struct EarthSimulationContext {
        Cohestra::EarthModel*   model = nullptr;
    };

    EarthSimulationContext simulationContext;
}


void OnUpstreamInitialized(Cohestra::InstanceUpstreamInitializedEvent& event);
void OnUpstreamAbsent(Cohestra::InstanceUpstreamAbsentEvent& event);
void OnStimulatorInstantiated(Cohestra::InstanceStimulatorInstantiatedEvent& event);
void OnStimulatorInitialized(Cohestra::InstanceStimulatorInitializedEvent& event);


int main(const int argc, const char* argv[])
{
    ConfigureLogColors();

    auto arguments = Cohestra::ParseEarthArguments(argc, argv);

    if (!arguments.valid)
    {
        spdlog::error("{}", arguments.errorMessage);
        spdlog::info("Use {} --help for usage.", arguments.executableName);
        return 1;
    }

    if (arguments.showHelp)
    {
        spdlog::info("Usage: {} [--config <path>] [--cycles <n>] [--ports <n>] [--verbose] [--help]", arguments.executableName);
        return 0;
    }

    if (arguments.verbose)
        spdlog::set_level(spdlog::level::debug);

    spdlog::info("Cohestra Framework with executable built for C++ models");
    spdlog::info("");
    Cohestra::Logo::PrintToLog();
    spdlog::info("");

    // C++ model and CCHI interface instantiation
    auto interface = std::make_shared<Cohestra::EarthCCHIInterface>(arguments.portCount);

    simulationContext.model = &interface->GetModel();

    spdlog::info("C++ CCHI model (EarthModel) instantiated with {} Type 1 port(s).", arguments.portCount);

    for (size_t i = 0; i < 8; ++i)
    {
        spdlog::info(" - Type 1 Port {}: {}", i,
            i < arguments.portCount ? "Present" : "Absent");
    }

    // Stimulator registry instantiation
    auto stimulatorRegistry = std::make_shared<Cohestra::StimulatorRegistry>();

    spdlog::info("Stimulator registry instantiated.");

    stimulatorRegistry->Register(std::make_shared<Cohestra::FuzzARIFactory>());
    stimulatorRegistry->Register(std::make_shared<Cohestra::Core2CoreFactory>());

    spdlog::info("{} stimulator(s) registered.", stimulatorRegistry->Size());

    // CCHI instance instantiation
    Cohestra::Instance instance;

    spdlog::info("CCHI instance instantiated, initializing with CCHI interface and stimulator registry.");

    instance.events->OnUpstreamInitialized.Register(
        Gravity::MakeListener("executable", 0, &OnUpstreamInitialized));
    instance.events->OnUpstreamAbsent.Register(
        Gravity::MakeListener("executable", 0, &OnUpstreamAbsent));
    instance.events->OnStimulatorInstantiated.Register(
        Gravity::MakeListener("executable", 0, &OnStimulatorInstantiated));
    instance.events->OnStimulatorInitialized.Register(
        Gravity::MakeListener("executable", 0, &OnStimulatorInitialized));

    // Read INI for instance initialization if specified
    std::unique_ptr<Cohestra::INIConfigurationLoader> configurationLoader;

    if (arguments.configurationPath)
    {
        configurationLoader = std::make_unique<Cohestra::INIConfigurationLoader>(*arguments.configurationPath);

        if (!configurationLoader->LoadFile())
        {
            spdlog::error("Failed to load configuration file: {}.", *arguments.configurationPath);
            return 1;
        }

        if (!instance.LoadPreInitConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load pre-init configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (!interface->GetModel().LoadConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load model configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }
    else
    {
        spdlog::warn("No external configuration file specified, running with built-in defaults.");
    }

    // direct the upstream node address maps and the model snoop targets
    interface->GetModel().SetUpstreamNodeIDs(
        instance.GetConfiguration().GetOrDefault(&Cohestra::Instance::CONFIG_T1_UPSTREAM_NODE_ID));

    instance.Initialize(interface, stimulatorRegistry);

    if (instance.GetState() != Cohestra::InstanceState::ALIVE)
    {
        spdlog::error("CCHI instance initialization failed, stopped with state {}.",
            instance.GetState() == Cohestra::InstanceState::NOT_INITIALIZED ? "NOT_INITIALIZED" :
            instance.GetState() == Cohestra::InstanceState::FINISHED ? "FINISHED" :
            instance.GetState() == Cohestra::InstanceState::FAILED ? "FAILED" : "UNKNOWN");

        return 1;
    }

    // Read INI for stimulators and other components if specified
    if (configurationLoader)
    {
        if (!instance.LoadPostInitConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load post-init configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }
    else
    {
        spdlog::warn("No external configuration file specified, skipping post-init configuration.");
        spdlog::warn("Stimulators may not be initialized properly without external configuration.");
    }

    spdlog::info("CCHI instance initialized.");

    // Cache-line data monitor instantiation (nodes keep their own per-node
    // event hubs in this harness, so the monitor attaches to each in turn)
    Cohestra::CacheLineDataMonitor cacheLineDataMonitor(instance);

    if (configurationLoader)
    {
        if (!cacheLineDataMonitor.LoadConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load cache-line data monitor configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }

    if (cacheLineDataMonitor.IsEnabled())
    {
        cacheLineDataMonitor.Attach();
        spdlog::info("Cache-line data monitor attached.");
    }
    else
        spdlog::info("Cache-line data monitor disabled by configuration.");

    // Performance profiler instantiation (joint-level latency/occupancy/denial
    // metrics plus Taurus channel/coherency counters; inert unless enabled)
    Cohestra::Profiler profiler(instance);

    if (configurationLoader)
    {
        if (!profiler.LoadConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load profiler configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }

    if (profiler.IsEnabled())
    {
        profiler.SetTicksPerCycle(1);   // earth harness: 1 tick per cycle
        profiler.Attach();
        spdlog::info("Performance profiler attached.");
    }
    else
        spdlog::info("Performance profiler disabled by configuration.");

    // Simulation-throughput meter (wall-clock cycles/second of the simulation
    // itself; on by default, in-flight display throttled to one line per
    // [simperf] interval.cycles simulated cycles)
    Cohestra::SimPerfMeter simPerfMeter(instance);

    if (configurationLoader)
    {
        if (!simPerfMeter.LoadConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load simulation performance meter configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }

    if (simPerfMeter.IsEnabled())
    {
        simPerfMeter.SetTicksPerCycle(1);   // earth harness: 1 tick per cycle
        spdlog::info("Simulation performance meter enabled.");
    }
    else
        spdlog::info("Simulation performance meter disabled by configuration.");

    spdlog::info("Simulation started, maximum {} cycle(s).", arguments.maximumCycles);

    // Simulation loop: the framework moves every flit between the Taurus
    // upstream nodes and the model interface in channel priority order
    // (EVT > SNP > REQ), then ticks all participants.
    uint64_t time = 0;

    bool anyStimulator  = false;
    bool allFinished    = false;
    bool drained        = false;

    simPerfMeter.Start(time);

    for (; time < arguments.maximumCycles; ++time)
    {
        simPerfMeter.Tick(time);

        // 1. stimulators issue transactions into the upstream nodes
        for (size_t i = 0; i < instance.GetStimulatorCount(); ++i)
        {
            if (auto* stimulator = instance.GetStimulator(i))
                stimulator->Tick(time);
        }

        // 2. pump upstream node -> model, EVT > responses > REQ
        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            if (auto flit = upstream->PeekTXEVT())
            {
                if (interface->PushType1EVT(index, *flit))
                    upstream->PopTXEVT();
            }

            if (auto flit = upstream->PeekTXRSP())
            {
                if (interface->PushType1UpRSP(index, *flit))
                    upstream->PopTXRSP();
            }

            if (auto flit = upstream->PeekTXDAT())
            {
                if (interface->PushType1UpDAT(index, *flit))
                    upstream->PopTXDAT();
            }

            if (auto flit = upstream->PeekTXREQ())
            {
                if (interface->PushType1REQ(index, *flit))
                    upstream->PopTXREQ();
            }
        }

        // 3. model tick (EVT > SNP > REQ internally)
        interface->TickPreHandshake(time);

        // 4. pump model -> upstream node
        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            if (auto flit = interface->PeekType1SNP(index))
            {
                if (upstream->PushRXSNP(*flit))
                    interface->PopType1SNP(index);
            }

            if (auto flit = interface->PopType1DnRSP(index))
                upstream->PushRXRSP(*flit);

            if (auto flit = interface->PopType1DnDAT(index))
                upstream->PushRXDAT(*flit);
        }

        // 5. upstream node ticks
        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            if (auto* upstream = instance.GetType1Upstream(index))
                upstream->Tick(time);
        }

        // 6. completion tracking: all stimulators finished and traffic drained.
        //    The simulation always runs the full cycle count and never stops
        //    early on drain, so later protocol violations are still observed.
        anyStimulator = false;
        allFinished   = true;

        for (size_t i = 0; i < instance.GetStimulatorCount(); ++i)
        {
            Cohestra::Stimulator* stimulator = instance.GetStimulator(i);

            if (!stimulator)
                continue;

            anyStimulator = true;

            if (!stimulator->IsFinished())
                allFinished = false;
        }

        if (!anyStimulator)
            continue;

        if (allFinished)
        {
            drained = interface->GetModel().IsIdle();

            for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
            {
                auto* upstream = instance.GetType1Upstream(index);

                if (!upstream)
                    continue;

                if (upstream->PeekTXEVT() || upstream->PeekTXREQ()
                 || upstream->PeekTXRSP() || upstream->PeekTXDAT())
                    drained = false;
            }
        }
    }

    if (anyStimulator && allFinished && drained)
        spdlog::info("Simulation finished: all tests completed and traffic drained, ran full {} cycle(s).", arguments.maximumCycles);
    else
        spdlog::warn("Simulation stopped: maximum cycle count {} reached.", arguments.maximumCycles);

    // final report
    for (size_t i = 0; i < instance.GetStimulatorCount(); ++i)
    {
        Cohestra::Stimulator* stimulator = instance.GetStimulator(i);

        if (!stimulator)
            continue;

        spdlog::info(" - stimulator {} ({}): {}.", stimulator->GetIndex(), stimulator->GetName(),
            stimulator->IsFinished() ? "finished" : "not finished");
    }

    const Cohestra::EarthModel& model = interface->GetModel();

    spdlog::info(" - model serviced: {} REQ(s), {} EVT(s), {} SNP(s).",
        model.GetServicedREQCount(), model.GetServicedEVTCount(), model.GetServicedSNPCount());
    spdlog::info(" - model denials: {} backpressure, {} protocol violation.",
        model.GetBackpressureDenialCount(), model.GetProtocolDenialCount());
    spdlog::info(" - model idle: {}.", model.IsIdle() ? "yes" : "no");

    // cache-line data monitor summary
    cacheLineDataMonitor.Report();

    // performance profiler summary (text/JSON/CSV per [profiler] configuration)
    profiler.Report(time);

    // simulation-throughput summary (wall-clock cycles/second, [simperf])
    simPerfMeter.Report(time);

    if (model.GetProtocolDenialCount())
    {
        spdlog::error("Simulation verdict: FAILED, {} protocol violation denial(s) reported.",
            model.GetProtocolDenialCount());
        return 2;
    }

    if (anyStimulator && (!allFinished || !drained))
    {
        spdlog::error("Simulation verdict: FAILED, stimulators or transactions did not complete.");

        interface->GetModel().DumpState();

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            spdlog::info(" - upstream {}: pending TXEVT {}, TXREQ {}, TXRSP {}, TXDAT {}",
                index,
                upstream->PeekTXEVT().has_value(),
                upstream->PeekTXREQ().has_value(),
                upstream->PeekTXRSP().has_value(),
                upstream->PeekTXDAT().has_value());
        }

        return 2;
    }

    if (cacheLineDataMonitor.GetMismatchCount())
    {
        spdlog::error("Simulation verdict: FAILED, {} cache-line data mismatch(es) reported by the monitor.",
            cacheLineDataMonitor.GetMismatchCount());
        return 2;
    }

    spdlog::info("Simulation verdict: PASSED.");

    return 0;
}


void OnUpstreamInitialized(Cohestra::InstanceUpstreamInitializedEvent& event)
{
    // the framework constructs upstream nodes without a SAM; direct all of
    // their traffic to the model's downstream node ID
    event.GetUpstream().sam = std::make_shared<EarthSAM>(
        static_cast<CCHI::Flits::dn_nodeid_t<Cohestra::CommonFlitConfigurationType1>>(
            simulationContext.model->GetNodeID()));

    spdlog::info(" - Taurus CCHI upstream node initialized: index {}, node ID {}",
        event.GetIndex(), size_t(event.GetUpstream().nodeID));
}

void OnUpstreamAbsent(Cohestra::InstanceUpstreamAbsentEvent& event)
{
    spdlog::info(" - Taurus CCHI upstream node absent: index {}",
        event.GetIndex());
}

void OnStimulatorInstantiated(Cohestra::InstanceStimulatorInstantiatedEvent& event)
{
    spdlog::info(" - Cohestra stimulator instantiated: index {}, name {}",
        event.GetIndex(), event.GetStimulator().GetName());
}

void OnStimulatorInitialized(Cohestra::InstanceStimulatorInitializedEvent& event)
{
    spdlog::info(" - Cohestra stimulator initialized: index {}, name {}",
        event.GetIndex(), event.GetStimulator().GetName());
}
