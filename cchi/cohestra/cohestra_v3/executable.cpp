#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "arguments.hpp"
#include "interface.hpp"
#include "interface_axi.hpp"
#include "interface_chi.hpp"
#include "logging.hpp"
#include "waves.hpp"
#include "event_upstream.hpp"

#include "../cohestra_logo.hpp"
#include "../cohestra_instance.hpp"
#include "../cohestra_monitor.hpp"
#include "../cohestra_watchdog.hpp"
#include "../cohestra_profiler.hpp"
#include "../cohestra_opcount.hpp"
#include "../cohestra_cchi_flit_logger.hpp"
#include "../cohestra_simperf.hpp"
#include "../cohestra_boot.hpp"
#include "../cohestra_bootings/boot_delay.hpp"
#include "../cohestra_bootings/boot_avocado.hpp"
#include "../cohestra_stimulators/fuzz_ari.hpp"
#include "../cohestra_stimulators/core2core.hpp"
#include "boot_host.hpp"
#include "../../icn/taurus/cchi_taurus_sam.hpp"
#include "spdlog/spdlog.h"

#ifndef COHESTRA_V3_VERILATED_TOP_HEADER
#define COHESTRA_V3_VERILATED_TOP_HEADER "Vtop_dummy.h"
#endif

#ifndef COHESTRA_V3_VERILATED_TOP_CLASS
#define COHESTRA_V3_VERILATED_TOP_CLASS Vtop_dummy
#endif

#ifndef COHESTRA_V3_THREADS
#define COHESTRA_V3_THREADS 0
#endif

#include COHESTRA_V3_VERILATED_TOP_HEADER

using VerilatedTop = COHESTRA_V3_VERILATED_TOP_CLASS;


const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_RESET_CYCLES
    ("RESET_CYCLES", "reset_cycles", "root", 100, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_RESET_REPEAT
    ("RESET_REPEAT", "reset_repeat", "root", 1, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_RESET_PREAMBLE
    ("RESET_PREAMBLE", "reset_preamble", "root", 1024, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<bool> CONFIG_LOG_VERBOSE
    ("LOG_VERBOSE", "verbose", "log", false, Cohestra::DeserializeBool, Cohestra::SerializeBool);

const Cohestra::ConfigurationEntryBack<std::string> CONFIG_BOOT_SEQUENCER
    ("BOOT_SEQUENCER", "sequencer", "boot", "", Cohestra::DeserializeString, Cohestra::SerializeString);


void OnUpstreamInitialized(Cohestra::InstanceUpstreamInitializedEvent& event);
void OnUpstreamAbsent(Cohestra::InstanceUpstreamAbsentEvent& event);
void OnStimulatorInstantiated(Cohestra::InstanceStimulatorInstantiatedEvent& event);
void OnStimulatorInitialized(Cohestra::InstanceStimulatorInitializedEvent& event);


int main(const int argc, const char* argv[])
{
    Cohestra::ConfigureLogColors();

    auto arguments = Cohestra::ParseArguments(argc, argv);

    if (!arguments.valid)
    {
        spdlog::error("{}", arguments.errorMessage);
        spdlog::info("Use {} --help for usage.", arguments.executableName);
        return 1;
    }

    if (arguments.showHelp)
    {
        spdlog::info("Usage: {} [--config <path>] [--cycles <n>] [--log-file[=<path>]] [--no-log-file] [--help]", arguments.executableName);
        return 0;
    }

    Cohestra::ConfigureFileLogging(arguments);

    spdlog::info("Cohestra Framework with executable built for Verilator");
    spdlog::info("");
    Cohestra::Logo::PrintToLog();
    spdlog::info("");

    // Verilated module instantiation
    // Allow waveform tracing to be enabled later (must precede model construction)
    Verilated::traceEverOn(true);

#if COHESTRA_V3_THREADS > 0
    // Multithreaded model (--threads N): create the context explicitly and
    // size the worker pool to the verilated thread count. The context default
    // is the host hardware parallelism, which would spawn a large pool of
    // mostly-idle workers behind the model's N mtask lanes.
    std::unique_ptr<VerilatedContext> verilatedContext = std::make_unique<VerilatedContext>();
    verilatedContext->threads(COHESTRA_V3_THREADS);
    // Let Verilator consume +verilator+* plusargs (e.g. +verilator+prof+exec+*)
    verilatedContext->commandArgs(argc, argv);

    VerilatedTop top{verilatedContext.get()};
#else
    VerilatedTop top;
#endif

    spdlog::info("Verilated top module instantiated: {}", typeid(VerilatedTop).name());

    // CCHI interface instantiation
    auto v3interface = std::make_shared<Cohestra::V3CCHIInterface<VerilatedTop>>(&top);

    spdlog::info("CCHI interface instantiated.");
    spdlog::info(" - Type 1 Port 0: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port0() ? "Present" : "Absent");
    spdlog::info(" - Type 1 Port 1: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port1() ? "Present" : "Absent");
    spdlog::info(" - Type 1 Port 2: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port2() ? "Present" : "Absent");
    spdlog::info(" - Type 1 Port 3: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port3() ? "Present" : "Absent");
    spdlog::info(" - Type 1 Port 4: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port4() ? "Present" : "Absent");
    spdlog::info(" - Type 1 Port 5: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port5() ? "Present" : "Absent");
    spdlog::info(" - Type 1 Port 6: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port6() ? "Present" : "Absent");
    spdlog::info(" - Type 1 Port 7: {}", Cohestra::V3CCHIInterface<VerilatedTop>::HasType1Port7() ? "Present" : "Absent");

    // AXI slave interface instantiation
    auto axiSlaveInterface = std::make_shared<Cohestra::V3AXISlaveInterface<VerilatedTop>>(&top);

    spdlog::info("AXI slave interface instantiated.");
    spdlog::info(" - AXI Slave Port 0: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort0() ? "Present" : "Absent");
    spdlog::info(" - AXI Slave Port 1: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort1() ? "Present" : "Absent");
    spdlog::info(" - AXI Slave Port 2: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort2() ? "Present" : "Absent");
    spdlog::info(" - AXI Slave Port 3: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort3() ? "Present" : "Absent");
    spdlog::info(" - AXI Slave Port 4: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort4() ? "Present" : "Absent");
    spdlog::info(" - AXI Slave Port 5: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort5() ? "Present" : "Absent");
    spdlog::info(" - AXI Slave Port 6: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort6() ? "Present" : "Absent");
    spdlog::info(" - AXI Slave Port 7: {}", Cohestra::V3AXISlaveInterface<VerilatedTop>::HasPort7() ? "Present" : "Absent");

    // AXI master interface instantiation
    auto axiMasterInterface = std::make_shared<Cohestra::V3AXIMasterInterface<VerilatedTop>>(&top);

    spdlog::info("AXI master interface instantiated.");
    spdlog::info(" - AXI Master Port 0: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort0() ? "Present" : "Absent");
    spdlog::info(" - AXI Master Port 1: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort1() ? "Present" : "Absent");
    spdlog::info(" - AXI Master Port 2: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort2() ? "Present" : "Absent");
    spdlog::info(" - AXI Master Port 3: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort3() ? "Present" : "Absent");
    spdlog::info(" - AXI Master Port 4: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort4() ? "Present" : "Absent");
    spdlog::info(" - AXI Master Port 5: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort5() ? "Present" : "Absent");
    spdlog::info(" - AXI Master Port 6: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort6() ? "Present" : "Absent");
    spdlog::info(" - AXI Master Port 7: {}", Cohestra::V3AXIMasterInterface<VerilatedTop>::HasPort7() ? "Present" : "Absent");

    // AXI monitor interface instantiation
    auto axiMonitorInterface = std::make_shared<Cohestra::V3AXIMonitorInterface<VerilatedTop>>(&top);

    spdlog::info("AXI monitor interface instantiated.");
    spdlog::info(" - AXI Monitor Port 0: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort0() ? "Present" : "Absent");
    spdlog::info(" - AXI Monitor Port 1: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort1() ? "Present" : "Absent");
    spdlog::info(" - AXI Monitor Port 2: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort2() ? "Present" : "Absent");
    spdlog::info(" - AXI Monitor Port 3: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort3() ? "Present" : "Absent");
    spdlog::info(" - AXI Monitor Port 4: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort4() ? "Present" : "Absent");
    spdlog::info(" - AXI Monitor Port 5: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort5() ? "Present" : "Absent");
    spdlog::info(" - AXI Monitor Port 6: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort6() ? "Present" : "Absent");
    spdlog::info(" - AXI Monitor Port 7: {}", Cohestra::V3AXIMonitorInterface<VerilatedTop>::HasPort7() ? "Present" : "Absent");

    // CHI interface instantiation
    auto chiFromRNInterface = std::make_shared<Cohestra::V3CHIFromRNInterface<VerilatedTop>>(&top);
    auto chiToRNInterface   = std::make_shared<Cohestra::V3CHIToRNInterface<VerilatedTop>>(&top);
    auto chiFromSNInterface = std::make_shared<Cohestra::V3CHIFromSNInterface<VerilatedTop>>(&top);
    auto chiToSNInterface   = std::make_shared<Cohestra::V3CHIToSNInterface<VerilatedTop>>(&top);
    auto chiMonitorInterface = std::make_shared<Cohestra::V3CHIMonitorInterface<VerilatedTop>>(&top);

    spdlog::info("CHI interface instantiated.");

    spdlog::info(" - CHI From-RN Port 0: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort0() ? "Present" : "Absent");
    spdlog::info(" - CHI From-RN Port 1: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort1() ? "Present" : "Absent");
    spdlog::info(" - CHI From-RN Port 2: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort2() ? "Present" : "Absent");
    spdlog::info(" - CHI From-RN Port 3: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort3() ? "Present" : "Absent");
    spdlog::info(" - CHI From-RN Port 4: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort4() ? "Present" : "Absent");
    spdlog::info(" - CHI From-RN Port 5: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort5() ? "Present" : "Absent");
    spdlog::info(" - CHI From-RN Port 6: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort6() ? "Present" : "Absent");
    spdlog::info(" - CHI From-RN Port 7: {}", Cohestra::V3CHIFromRNInterface<VerilatedTop>::HasPort7() ? "Present" : "Absent");

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
    }

    instance.Initialize(v3interface, stimulatorRegistry);

    if (instance.GetState() != Cohestra::InstanceState::ALIVE)
    {
        spdlog::error("CCHI instance initialization failed, stopped with state {}.", 
            instance.GetState() == Cohestra::InstanceState::NOT_INITIALIZED ? "NOT_INITIALIZED" :
            instance.GetState() == Cohestra::InstanceState::FINISHED ? "FINISHED" :
            instance.GetState() == Cohestra::InstanceState::FAILED ? "FAILED" : "UNKNOWN");
        
        return 1;
    }

    // Read INI for stimualtors and other components if specified
    if (configurationLoader)
    {
        if (!instance.LoadPostInitConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load post-init configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (!Cohestra::Waves::LoadConfiguration(*configurationLoader, instance.GetConfiguration()))
        {
            spdlog::error("Failed to load waveform configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }
    else
    {
        spdlog::warn("No external configuration file specified, skipping post-init configuration.");
        spdlog::warn("Stimulators may not be initialized properly without external configuration.");
    }

    spdlog::info("CCHI instance initialized.");

    // AXI memory & master model & monitor instantiation (one per present
    // port; each instance owns its interface port and completes the channel
    // interactions inside its own Tick)
    std::vector<std::shared_ptr<Cohestra::AXIMemory>>
        axiMemories;

    std::vector<std::shared_ptr<Cohestra::AXIMaster>>
        axiMasters;

    std::vector<size_t> axiMasterIndices;

    std::vector<std::shared_ptr<Cohestra::AXIMonitor>>
        axiMonitors;

    std::vector<size_t> axiMonitorIndices;

    for (size_t index : axiSlaveInterface->GetPortIndices())
    {
        auto memory = std::make_shared<Cohestra::AXIMemory>(
            axiSlaveInterface, index, static_cast<unsigned int>(index));

        if (configurationLoader)
        {
            if (!memory->LoadConfiguration(*configurationLoader))
            {
                spdlog::error("Failed to load AXI memory configuration from {}.", *arguments.configurationPath);
                return 1;
            }
        }

        axiSlaveInterface->SetPortBFIFOSize(index, 2);
        axiSlaveInterface->SetPortRFIFOSize(index, 2);

        axiMemories.push_back(memory);

        spdlog::info(" - AXI Slave Port {}: memory model attached ({}-bit data).", index, memory->GetDataBits());
    }

    for (size_t index : axiMasterInterface->GetPortIndices())
    {
        auto master = std::make_shared<Cohestra::AXIMaster>(
            axiMasterInterface, index, static_cast<unsigned int>(index));

        if (configurationLoader)
        {
            if (!master->LoadConfiguration(*configurationLoader))
            {
                spdlog::error("Failed to load AXI master configuration from {}.", *arguments.configurationPath);
                return 1;
            }
        }

        axiMasterInterface->SetPortAWFIFOSize(index, 2);
        axiMasterInterface->SetPortWFIFOSize(index, 2);
        axiMasterInterface->SetPortARFIFOSize(index, 2);

        axiMasterIndices.push_back(index);
        axiMasters.push_back(master);

        spdlog::info(" - AXI Master Port {}: master model attached ({}-bit data).", index, master->GetDataBits());
    }

    for (size_t index : axiMonitorInterface->GetPortIndices())
    {
        auto monitor = std::make_shared<Cohestra::AXIMonitor>(axiMonitorInterface, index);

        Cohestra::RegisterAXIMonitorListeners(monitor, index);

        axiMonitorIndices.push_back(index);
        axiMonitors.push_back(monitor);

        spdlog::info(" - AXI Monitor Port {}: monitor attached ({}-bit data).", index, axiMonitorInterface->GetPortDataBits(index));
    }

    // Upstream event hub registration
    std::shared_ptr<CCHI::Taurus::UpstreamNode<Cohestra::CommonFlitConfigurationType1>::EventHub> upstreamNodeEventHub 
        = std::make_shared<CCHI::Taurus::UpstreamNode<Cohestra::CommonFlitConfigurationType1>::EventHub>();

    RegisterUpstreamXactDeniedEventListeners(instance, *upstreamNodeEventHub);

    for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
    {
        auto* upstream = instance.GetType1Upstream(index);

        if (upstream)
            upstream->events = upstreamNodeEventHub;

        spdlog::info("Applied upstream node event hub for Type 1 Upstream index {}.", index);
    }

    // Cache-line data monitor instantiation. Attaches after the harness
    // replaced the per-node event hubs above, so its listeners land on the
    // shared hub actually in use.
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

    // Transaction watchdog instantiation. Attaches after the harness
    // replaced the per-node event hubs above, so its listeners land on the
    // shared hub actually in use.
    Cohestra::TransactionWatchdog watchdog(instance);

    if (configurationLoader)
    {
        if (!watchdog.LoadConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load transaction watchdog configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }

    if (watchdog.IsEnabled())
    {
        watchdog.Attach();
        spdlog::info("Transaction watchdog attached.");
    }
    else
        spdlog::info("Transaction watchdog disabled by configuration.");

    // Performance profiler instantiation. Attaches after the harness replaced
    // the per-node event hubs above, so its listeners land on the shared hub
    // actually in use.
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
        profiler.SetTicksPerCycle(2);   // v3 harness: half-cycle ticks, 2 per cycle
        profiler.Attach();
        spdlog::info("Performance profiler attached.");
    }
    else
        spdlog::info("Performance profiler disabled by configuration.");

    // Upstream opcode counter. Attaches after the harness replaced the
    // per-node event hubs above, so its listeners land on the shared hub
    // actually in use.
    Cohestra::OpCount opCount(instance);

    if (configurationLoader)
    {
        if (!opCount.LoadConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load opcode counter configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }

    if (opCount.IsEnabled())
    {
        opCount.Attach();
        spdlog::info("Upstream opcode counter attached.");
    }
    else
        spdlog::info("Upstream opcode counter disabled by configuration.");

    // CCHI flit logger. Attaches after the harness replaced the per-node
    // event hubs above, so its listeners land on the shared hub actually in
    // use.
    Cohestra::CCHIFlitLogger cchiFlitLogger(instance);

    if (configurationLoader)
    {
        if (!cchiFlitLogger.LoadConfiguration(*configurationLoader))
        {
            spdlog::error("Failed to load CCHI flit logger configuration from {}.", *arguments.configurationPath);
            return 1;
        }
    }

    if (cchiFlitLogger.IsEnabled())
    {
        cchiFlitLogger.Attach();
        spdlog::info("CCHI flit logger attached.");
    }
    else
        spdlog::info("CCHI flit logger disabled by configuration.");

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
        simPerfMeter.SetTicksPerCycle(2);   // v3 harness: half-cycle ticks, 2 per cycle
        spdlog::info("Simulation performance meter enabled.");
    }
    else
        spdlog::info("Simulation performance meter disabled by configuration.");

    // Waveform tracer setup
    Cohestra::Waves::Tracer waveTracer;

    if (!waveTracer.Configure(instance.GetConfiguration()))
    {
        spdlog::error("Failed to configure waveform tracer.");
        return 1;
    }

    if (waveTracer.IsConfigured())
    {
        if (!waveTracer.Open(*top.contextp()))
        {
            spdlog::error("Failed to open waveform trace file.");
            return 1;
        }
    }
    else
        spdlog::info("Waveform dumping disabled.");

    uint64_t time = 0;

    // [log] verbose = 1 enables debug-level logging (e.g. monitor trace);
    // read from the raw loader - the instance Configuration only holds
    // entries explicitly Load()ed into it
    if (configurationLoader)
    {
        std::optional<std::string> verboseRaw = configurationLoader->GetRawString(&CONFIG_LOG_VERBOSE);

        bool verbose = false;

        if (verboseRaw)
        {
            if (auto parsed = Cohestra::DeserializeBool(*verboseRaw))
                verbose = *parsed;
            else
            {
                spdlog::error("Failed to parse [log] verbose value: {}", *verboseRaw);
                return 1;
            }
        }

        if (verbose)
        {
            spdlog::set_level(spdlog::level::debug);
            spdlog::info("Verbose (debug-level) logging enabled by [log] verbose.");
        }
    }

    // Boot sequencer (harness-level): owns the Boot Stage between the reset
    // stage and the test stimulation stage, and is free to sequence through
    // any driver the harness exposes (CCHI master side, AXI models, CHI
    // interfaces, ...). Selected by [boot] sequencer; when empty, no boot
    // sequencer is registered and the Boot Stage is skipped.
    Cohestra::V3BootHost<VerilatedTop> bootHost {
        .instance            = &instance,
        .cchiInterface       = v3interface,
        .axiSlaveInterface   = axiSlaveInterface,
        .axiMasterInterface  = axiMasterInterface,
        .axiMonitorInterface = axiMonitorInterface,
        .chiFromRNInterface  = chiFromRNInterface,
        .chiToRNInterface    = chiToRNInterface,
        .chiFromSNInterface  = chiFromSNInterface,
        .chiToSNInterface    = chiToSNInterface,
        .chiMonitorInterface = chiMonitorInterface,
        .axiMemories         = &axiMemories,
        .axiMasters          = &axiMasters
    };

    Cohestra::BootSequencerRegistry<Cohestra::V3BootHost<VerilatedTop>> bootSequencerRegistry;

    bootSequencerRegistry.Register(
        std::make_shared<Cohestra::DelayBootSequencerFactory<Cohestra::V3BootHost<VerilatedTop>>>());

    bootSequencerRegistry.Register(
        std::make_shared<Cohestra::AvocadoBootSequencerFactory<Cohestra::V3BootHost<VerilatedTop>>>());

    spdlog::info("{} boot sequencer(s) registered.", bootSequencerRegistry.Size());

    std::unique_ptr<Cohestra::BootSequencer> bootSequencer;

    if (configurationLoader)
    {
        std::optional<std::string> bootNameRaw = configurationLoader->GetRawString(&CONFIG_BOOT_SEQUENCER);

        if (bootNameRaw && !bootNameRaw->empty())
        {
            auto bootFactory = bootSequencerRegistry.Get(*bootNameRaw);

            if (!bootFactory)
            {
                spdlog::error("Boot sequencer \"{}\" is not found in the registry.", *bootNameRaw);
                return 1;
            }

            bootSequencer = bootFactory->Create(bootHost);

            if (!bootSequencer->Initialize(*configurationLoader))
            {
                spdlog::error("Failed to initialize boot sequencer \"{}\".", *bootNameRaw);
                return 1;
            }

            spdlog::info("Boot sequencer \"{}\" ({}) selected.",
                bootSequencer->GetName(), bootSequencer->GetDescription());
        }
    }

    // Reset (high-active), optionally repeated: each pulse is reset_cycles
    // cycles of reset active followed by reset_cycles cycles of reset inactive.
    // Both keys are executable-local: they are read straight from the INI
    // loader, not from the instance Configuration (which never Load()s them).
    uint32_t resetCycles = CONFIG_RESET_CYCLES.defaultValue;
    uint32_t resetRepeat = CONFIG_RESET_REPEAT.defaultValue;
    uint32_t resetPreamble = CONFIG_RESET_PREAMBLE.defaultValue;

    if (configurationLoader)
    {
        auto readResetKey = [&configurationLoader] (
            const Cohestra::ConfigurationEntryBack<uint32_t>* entry, uint32_t& dst) -> bool
        {
            std::optional<std::string> raw = configurationLoader->GetRawString(entry);

            if (!raw)
                return true;

            if (auto parsed = Cohestra::DeserializeUInt32(*raw))
            {
                dst = *parsed;
                return true;
            }

            spdlog::error("Failed to parse [{}] {} value: {}", entry->section, entry->key, *raw);
            return false;
        };

        if (!readResetKey(&CONFIG_RESET_CYCLES, resetCycles) ||
            !readResetKey(&CONFIG_RESET_REPEAT, resetRepeat) ||
            !readResetKey(&CONFIG_RESET_PREAMBLE, resetPreamble))
            return 1;
    }

    if (resetRepeat == 0)
    {
        spdlog::warn("reset_repeat = 0 is invalid, using 1.");
        resetRepeat = 1;
    }

    spdlog::info("Resetting: {} cycle(s) of preamble (reset deasserted), then {} reset pulse(s) of {} cycle(s) active + {} cycle(s) inactive.",
        resetPreamble, resetRepeat, resetCycles, resetCycles);

    auto runResetCycles = [&time, &top, &waveTracer] (uint32_t cycles)
    {
        for (uint32_t i = 0; i < cycles; ++i)
        {
            Cohestra::SetCurrentLogTime(time);

            top.clock = 0;
            top.eval();
            waveTracer.Dump(time++);

            top.clock = 1;
            top.eval();
            waveTracer.Dump(time++);
        }
    };

    // Power-on preamble: run with reset deasserted before the first
    // assertion. Targets that keep power-on state behind asynchronous-reset
    // flops (e.g. the credit-initialization counters in the Avocado mesh's
    // PUB fabric) are only loaded by the reset *assertion* edge; starting the
    // simulation with reset already asserted never produces that edge in a
    // 2-state (Verilator) model and leaves such state wedged.
    if (resetPreamble)
    {
        top.reset = 0;
        runResetCycles(resetPreamble);
    }

    for (uint32_t r = 0; r < resetRepeat; ++r)
    {
        spdlog::info("Reset pulse {}/{}: active [{},{}), inactive [{},{}).",
            r + 1, resetRepeat,
            time, time + 2 * resetCycles,
            time + 2 * resetCycles, time + 4 * resetCycles);

        top.reset = 1;
        runResetCycles(resetCycles);

        top.reset = 0;
        runResetCycles(resetCycles);
    }

    spdlog::info("Reset released.");

    // arm the throughput meter after reset, so reset cycles are not measured
    simPerfMeter.Start(time);

    // One simulation cycle, shared by the boot stage and the test stimulation
    // stage below; only the instance tick differs between the stages. Returns
    // false when the run must stop (instance left ALIVE, or maximum cycles).
    auto runCycle = [&] (auto&& tickInstance) -> bool
    {
        Cohestra::SetCurrentLogTime(time);

        if (!tickInstance())
        {
            // Tick rejects only when the instance has left ALIVE (FAILED, e.g.
            // via the cache-line data monitor, or FINISHED): stop ticking and
            // fall through to the normal report/verdict path below
            spdlog::error("CCHI instance stopped ticking at time {}: state is {}.", time,
                instance.GetState() == Cohestra::InstanceState::FINISHED ? "FINISHED" :
                instance.GetState() == Cohestra::InstanceState::FAILED ? "FAILED" : "UNKNOWN");
            return false;
        }

        watchdog.Tick(time);

        simPerfMeter.Tick(time);

        for (auto& memory : axiMemories)
            memory->TickPreHandshake(time);

        for (auto& master : axiMasters)
            master->TickPreHandshake(time);

        chiFromRNInterface->TickPreHandshake(time);
        chiToRNInterface->TickPreHandshake(time);
        chiFromSNInterface->TickPreHandshake(time);
        chiToSNInterface->TickPreHandshake(time);

        top.clock = 0;
        top.eval();

        // retire phase: sample the settled valid/ready pairs (exactly what the
        // DUT registers at the upcoming posedge) and pop the transferred flits
        instance.TickPostHandshake(time);

        for (auto& memory : axiMemories)
            memory->TickPostHandshake(time);

        for (auto& master : axiMasters)
            master->TickPostHandshake(time);

        // CHI interfaces sample/retire after the clock=0 eval.
        chiFromRNInterface->TickPostHandshake(time);
        chiToRNInterface->TickPostHandshake(time);
        chiFromSNInterface->TickPostHandshake(time);
        chiToSNInterface->TickPostHandshake(time);
        chiMonitorInterface->TickPostHandshake(time);

        // monitors sample the settled combinational state of the current
        // cycle (after the clock=0 eval, so that the C++-driven 'ready'
        // pins have propagated to the monitor taps), before the posedge
        for (auto& monitor : axiMonitors)
            monitor->TickPostHandshake(time);

        waveTracer.Dump(time++);

        top.clock = 1;
        top.eval();
        waveTracer.Dump(time++);

        if (arguments.maximumCycles > 0 && time >= arguments.maximumCycles)
        {
            instance.SetFinished({"Simulation reached maximum cycle count."});
            return false;
        }

        return true;
    };

    // Boot stage: skipped when no boot sequencer is registered; otherwise runs
    // until the boot sequencer itself signals completion via IsBootComplete().
    // Stimulators stay frozen (Instance::TickBoot); the boot sequencer ticks
    // in their place.
    if (!bootSequencer)
        spdlog::info("No boot sequencer registered; boot stage skipped.");
    else
    {
        spdlog::info("Boot stage started: sequencer \"{}\" ({}) at time {}.",
            bootSequencer->GetName(), bootSequencer->GetDescription(), time);

        while (instance.IsAlive() && !bootSequencer->IsBootComplete())
        {
            if (!runCycle([&] () {
                bootSequencer->Tick(time);
                return instance.TickBoot(time);
            }))
                break;
        }

        if (bootSequencer->HasBootFailed())
        {
            spdlog::error("Boot stage failed: sequencer \"{}\" reported an error.",
                bootSequencer->GetName());
            return 1;
        }

        if (bootSequencer->IsBootComplete())
            spdlog::info("Boot stage completed at time {}.", time);
        else
            spdlog::error("Boot stage did not complete: instance left ALIVE (see above).");
    }

    // Test stimulation stage
    if (instance.IsAlive())
    {
        if (arguments.maximumCycles > 0)
            spdlog::info("Simulation started, maximum {} cycle(s).", arguments.maximumCycles);
        else
            spdlog::info("Simulation started, no maximum cycle limit.");
    }

    while (instance.IsAlive())
    {
        if (!runCycle([&] () { return instance.Tick(time); }))
            break;
    }

    // Per-port AXI master summary: transactions completed during the run
    for (size_t k = 0; k < axiMasters.size(); ++k)
    {
        spdlog::info(" - AXI Master Port {}: {} write transaction(s), {} read transaction(s) completed.",
            axiMasterIndices[k],
            axiMasters[k]->GetCompletedWriteCount(),
            axiMasters[k]->GetCompletedReadCount());
    }

    // Per-port AXI monitor summary: transactions observed during the run
    for (size_t k = 0; k < axiMonitors.size(); ++k)
    {
        spdlog::info(" - AXI Monitor Port {}: {} write transaction(s), {} read transaction(s) observed.",
            axiMonitorIndices[k],
            axiMonitors[k]->GetCompletedWriteCount(),
            axiMonitors[k]->GetCompletedReadCount());
    }

    // Cache-line data monitor summary: mismatches observed during the run
    cacheLineDataMonitor.Report();

    // Transaction watchdog summary: transaction lifecycles and triggers
    // observed during the run
    watchdog.Report();

    // Performance profiler summary (text/JSON/CSV per [profiler] configuration)
    profiler.Report(time);

    // Simulation-throughput summary (wall-clock cycles/second, [simperf])
    simPerfMeter.Report(time);

    // Per-upstream-node opcode counts observed during the run
    opCount.Report();

    if (instance.IsFinished())
    {
        spdlog::info("\"\033[1;32m                                                           \033[0m\"");
        spdlog::info("\"\033[1;32m ███████ ██ ███   ██ ██ ███████ ██   ██ ███████ ██████  ██ \033[0m\"");
        spdlog::info("\"\033[1;32m ██      ██ ████  ██ ██ ██      ██   ██ ██      ██   ██ ██ \033[0m\"");
        spdlog::info("\"\033[1;32m █████   ██ ██ ██ ██ ██ ███████ ███████ █████   ██   ██ ██ \033[0m\"");
        spdlog::info("\"\033[1;32m ██      ██ ██  ████ ██      ██ ██   ██ ██      ██   ██    \033[0m\"");
        spdlog::info("\"\033[1;32m ██      ██ ██   ███ ██ ███████ ██   ██ ███████ ██████  ██ \033[0m\"");
        spdlog::info("\"\033[1;32m                                                           \033[0m\"");

        if (instance.GetFinishMessage().empty())
            spdlog::info("Simulation finished.");
        else
        {
            spdlog::info("Simulation finished:");
            for (const auto& message : instance.GetFinishMessage())
                spdlog::info(" - {}", message);
        }
    }
    else if (instance.IsFailed())
    {
        if (instance.GetFailMessage().empty())
            spdlog::error("Simulation failed.");
        else
        {
            spdlog::info("Simulation failed:");
            for (const auto& message : instance.GetFailMessage())
                spdlog::error(" - {}", message);
        }
    }

    spdlog::info("Simulation exited with state {}.", 
        instance.GetState() == Cohestra::InstanceState::ALIVE ? "ALIVE" :
        instance.GetState() == Cohestra::InstanceState::FINISHED ? "FINISHED" :
        instance.GetState() == Cohestra::InstanceState::FAILED ? "FAILED" : "UNKNOWN");

    if (cacheLineDataMonitor.GetMismatchCount())
    {
        spdlog::error("Simulation verdict: FAILED, {} cache-line data mismatch(es) reported by the monitor.",
            cacheLineDataMonitor.GetMismatchCount());
    }

    if (instance.IsFailed() || cacheLineDataMonitor.GetMismatchCount())
        return 2;

    return 0;
}


void OnUpstreamInitialized(Cohestra::InstanceUpstreamInitializedEvent& event)
{
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

