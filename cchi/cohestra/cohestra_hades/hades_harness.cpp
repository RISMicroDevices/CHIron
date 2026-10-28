#if defined(COHESTRA_HADES)
#  if defined(__has_include) && !__has_include(<hades/transport.hpp>)
#    error "cohestra_hades: COHESTRA_HADES defined but HADES-Fabric headers not on the include path (set HADES_FABRIC_ROOT)"
#  endif
#endif

// Self-neutralizing guard: outside this layer's own opt-in build the whole
// file compiles to an empty translation unit. Content is enabled either by
// the opt-in macro (cohestra_hades build, consumers linking the CMake target)
// or simply by having the HADES-Fabric headers on the include path (the
// HADES rehearsal, which links the archive by file path and therefore does
// not inherit the target's PUBLIC defines).
#if defined(COHESTRA_HADES) || __has_include(<hades/transport.hpp>)

//*****************************************************************************
// hades_harness.cpp — the cohestra_hades harness: the cohestra_v3 executable
// flow (INI config, Instance + stimulators + monitors + watchdog + profiler,
// AXI memory, reset stage, run loop, reports, verdict) bound to a HADES-Fabric
// link through HadesCCHIInterface / HadesAXISlaveInterface instead of a
// Verilated top.
//
// Structural deltas from cohestra_v3/executable.cpp (all intentional, see
// README.md):
//   * no Verilated top, no waves, no CHI operational interfaces, no AXI
//     master/monitor models — the PACKING.md boundary is exactly t1p0 (CCHI)
//     + axi_m0 (AXI slave);
//   * the runCycle eval pair (clock=0/clock=1) is replaced by the link tail:
//     HadesLink::DriveCycle stages one target cycle's h2c tokens, flushes
//     granule frames, extends credit and rings the doorbell;
//   * [root] reset_preamble/reset_cycles/reset_repeat are accepted for
//     v3-compatible configs but are NO-OPS here: the FPGA-side wrapper owns
//     the DUT reset stretch (on loopback the mock resets instantly);
//   * --cycles bounds TARGET cycles (v3 compares its half-cycle time base,
//     effectively stopping at maximumCycles/2 — reads like a v3 quirk; this
//     harness counts staged link cycles 1:1).
//*****************************************************************************
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "hades_harness.hpp"
#include "hades_interface.hpp"
#include "hades_report.hpp"
#include "hades_transport.hpp"

#include "../cohestra_logo.hpp"
#include "../cohestra_instance.hpp"
#include "../cohestra_monitor.hpp"
#include "../cohestra_watchdog.hpp"
#include "../cohestra_profiler.hpp"
#include "../cohestra_opcount.hpp"
#include "../cohestra_cchi_flit_logger.hpp"
#include "../cohestra_simperf.hpp"
#include "../cohestra_clogb.hpp"
#include "../cohestra_boot.hpp"
#include "../cohestra_bootings/boot_delay.hpp"
#include "../cohestra_stimulators/fuzz_ari.hpp"
#include "../cohestra_stimulators/core2core.hpp"

#include "logging.hpp"
#include "event_upstream.hpp"

#include "../../icn/taurus/cchi_taurus_sam.hpp"

#include "spdlog/spdlog.h"


// [hades] section (cohestra_config typed entries; consumed only by this layer)
const Cohestra::ConfigurationEntryBack<std::string> CONFIG_HADES_MANIFEST
    ("HADES_MANIFEST", "manifest", "hades", "", Cohestra::DeserializeString, Cohestra::SerializeString);

const Cohestra::ConfigurationEntryBack<std::string> CONFIG_HADES_BACKEND
    ("HADES_BACKEND", "backend", "hades", "", Cohestra::DeserializeString, Cohestra::SerializeString);

const Cohestra::ConfigurationEntryBack<std::string> CONFIG_HADES_JTAG_URL
    ("HADES_JTAG_URL", "jtag_url", "hades", "tcp://127.0.0.1:3122", Cohestra::DeserializeString, Cohestra::SerializeString);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_WINDOW_K
    ("HADES_WINDOW_K", "window_k", "hades", 32, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_SKID_DEPTH
    ("HADES_SKID_DEPTH", "skid_depth", "hades", 0, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_RX_QUEUE
    ("HADES_RX_QUEUE", "rx_queue", "hades", 1024, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint64_t> CONFIG_HADES_MAX_QUIET_POLLS
    ("HADES_MAX_QUIET_POLLS", "max_quiet_polls", "hades", 2000000, Cohestra::DeserializeUInt64, Cohestra::SerializeUInt64);

// Transport segment cap for h2c staging (1 = Phase J-0 single-beat JTAG bridge)
const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_MAX_BATCH_FRAMES
    ("HADES_MAX_BATCH_FRAMES", "max_batch_frames", "hades", 8, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

// DUT-cycle anchoring (strict replay-determinism under host congestion)
const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_VIEW_LATENCY
    ("HADES_VIEW_LATENCY", "view_latency", "hades", 40, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_ANCHOR_L_RSP
    ("HADES_ANCHOR_L_RSP", "anchor_l_rsp", "hades", 0, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_ANCHOR_L_DAT
    ("HADES_ANCHOR_L_DAT", "anchor_l_dat", "hades", 0, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_ANCHOR_L_B
    ("HADES_ANCHOR_L_B", "anchor_l_b", "hades", 0, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

const Cohestra::ConfigurationEntryBack<uint32_t> CONFIG_HADES_ANCHOR_L_R
    ("HADES_ANCHOR_L_R", "anchor_l_r", "hades", 0, Cohestra::DeserializeUInt32, Cohestra::SerializeUInt32);

// v3-compatible reset keys ([root]); NO-OPS over HADES — the FPGA wrapper owns
// the DUT reset stretch. Parsed so v3-shaped configs load cleanly, logged once.
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


namespace {

    using namespace Cohestra;

    // ---- CLI -------------------------------------------------------------------

    bool StartsWith(const std::string& value, const char* prefix) noexcept
    {
        return value.rfind(prefix, 0) == 0;
    }

    std::string StripWrappedQuotes(const std::string& value) noexcept
    {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            return value.substr(1, value.size() - 2);

        return value;
    }

    // ---- --seed override: intercepts fuzz.ari.seed on any section ---------------

    class SeedOverrideLoader : public Cohestra::StringLoaderBase {
    public:
        SeedOverrideLoader(const Cohestra::StringLoaderBase& base, uint64_t seed) noexcept
            : base(base), seed(seed) { }

    public:
        std::optional<std::string> GetRawString(
            Cohestra::ConfigurationEntryBase entry,
            const std::string& defaultSection = "global") const noexcept override
        {
            if (entry && entry->key && std::strcmp(entry->key, "fuzz.ari.seed") == 0)
                return std::to_string(seed);

            return base.GetRawString(entry, defaultSection);
        }

    private:
        const Cohestra::StringLoaderBase& base;
        uint64_t seed;
    };

    // ---- boot host (the Boot Stage over this boundary) ---------------------------

    struct HadesBootHost {
        Instance*                                   instance            = nullptr;
        std::shared_ptr<Hades::HadesCCHIInterface>  cchiInterface       = nullptr;
        std::shared_ptr<Hades::HadesAXISlaveInterface> axiSlaveInterface = nullptr;
        std::shared_ptr<Cohestra::CHI::CHIMonitorInterface> chiMonitorInterface = nullptr;
        std::vector<std::shared_ptr<AXIMemory>>*    axiMemories         = nullptr;

        Instance*   GetInstance() const noexcept { return instance; }

        size_t      GetCCHIUpstreamCount() const noexcept
        { return instance ? instance->GetType1UpstreamCount() : 0; }

        CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*
                    GetCCHIUpstream(size_t index) const noexcept
        { return instance ? instance->GetType1Upstream(index) : nullptr; }

        // no AXI master models on this boundary
        size_t      GetAXIMasterCount() const noexcept { return 0; }
        AXIMaster*  GetAXIMaster(size_t) const noexcept { return nullptr; }
    };

    static_assert(BootHostContract<HadesBootHost>);

    // ---- instance event loggers (as v3's executable.cpp) -------------------------

    void OnUpstreamInitialized(InstanceUpstreamInitializedEvent& event)
    {
        spdlog::info(" - Taurus CCHI upstream node initialized: index {}, node ID {}",
            event.GetIndex(), size_t(event.GetUpstream().nodeID));
    }

    void OnUpstreamAbsent(InstanceUpstreamAbsentEvent& event)
    {
        spdlog::info(" - Taurus CCHI upstream node absent: index {}",
            event.GetIndex());
    }

    void OnStimulatorInstantiated(InstanceStimulatorInstantiatedEvent& event)
    {
        spdlog::info(" - Cohestra stimulator instantiated: index {}, name {}",
            event.GetIndex(), event.GetStimulator().GetName());
    }

    void OnStimulatorInitialized(InstanceStimulatorInitializedEvent& event)
    {
        spdlog::info(" - Cohestra stimulator initialized: index {}, name {}",
            event.GetIndex(), event.GetStimulator().GetName());
    }

    // [hades] raw-key reader with v3's readResetKey shape
    template<class T>
    bool ReadHadesKey(const INIConfigurationLoader& loader,
                      const ConfigurationEntryBack<T>* entry, T& dst)
    {
        std::optional<std::string> raw = loader.GetRawString(entry);

        if (!raw)
            return true;

        if (auto parsed = entry->deserializer(*raw))
        {
            dst = *parsed;
            return true;
        }

        spdlog::error("Failed to parse [{}] {} value: {}", entry->section, entry->key, *raw);
        return false;
    }

    // manifest path resolution: as given (CWD-relative), then INI-file-relative
    std::string ResolveManifestPath(const std::string& path, const std::string& iniPath)
    {
        auto exists = [] (const std::string& p)
        {
            std::ifstream f(p);
            return f.good();
        };

        if (exists(path))
            return path;

        const std::string::size_type slash = iniPath.find_last_of("/\\");

        if (slash != std::string::npos)
        {
            std::string candidate = iniPath.substr(0, slash + 1) + path;

            if (exists(candidate))
                return candidate;
        }

        return path;    // let the YAML load throw the canonical error
    }
}


namespace Cohestra::Hades {

    int RunHarnessImpl(int argc, const char* argv[]);   // internal, may throw

    HadesArgumentsContext ParseHadesArguments(int argc, const char* argv[]) noexcept
    {
        HadesArgumentsContext context;

        if (argc > 0 && argv && argv[0])
            context.executableName = argv[0];
        else
            context.executableName = "cohestra_hades";

        bool positionalOnly = false;

        for (int i = 1; i < argc; ++i)
        {
            const char* rawArgument = argv[i];

            if (!rawArgument)
            {
                context.valid = false;
                context.errorMessage = "Null argument encountered.";
                return context;
            }

            std::string argument(rawArgument);

            if (!positionalOnly && argument == "--")
            {
                positionalOnly = true;
                continue;
            }

            if (!positionalOnly && (argument == "-h" || argument == "--help"))
            {
                context.showHelp = true;
                continue;
            }

            if (!positionalOnly && (argument == "-c" || argument == "--config"))
            {
                if (i + 1 >= argc || !argv[i + 1])
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for " + argument + ".";
                    return context;
                }

                context.configurationPath = StripWrappedQuotes(std::string(argv[++i]));
                continue;
            }

            if (!positionalOnly && StartsWith(argument, "--config="))
            {
                std::string value = argument.substr(9);

                if (value.empty())
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --config.";
                    return context;
                }

                context.configurationPath = StripWrappedQuotes(value);
                continue;
            }

            if (!positionalOnly && (argument == "--cycles" || argument == "-n"))
            {
                if (i + 1 >= argc || !argv[i + 1])
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --cycles.";
                    return context;
                }

                try
                {
                    context.maximumCycles = std::stoull(argv[++i]);
                }
                catch (...)
                {
                    context.valid = false;
                    context.errorMessage = "Invalid value for --cycles.";
                    return context;
                }
                continue;
            }

            if (!positionalOnly && argument == "--seed")
            {
                if (i + 1 >= argc || !argv[i + 1])
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --seed.";
                    return context;
                }

                try
                {
                    context.seedOverride = std::stoull(argv[++i]);
                }
                catch (...)
                {
                    context.valid = false;
                    context.errorMessage = "Invalid value for --seed.";
                    return context;
                }
                continue;
            }

            if (!positionalOnly && argument == "--backend")
            {
                if (i + 1 >= argc || !argv[i + 1])
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --backend.";
                    return context;
                }

                context.backendOverride = StripWrappedQuotes(std::string(argv[++i]));
                continue;
            }

            if (!positionalOnly && argument == "--log-file")
            {
                context.logToFile = true;
                continue;
            }

            if (!positionalOnly && argument == "--no-log-file")
            {
                context.logToFile = false;
                continue;
            }

            if (!positionalOnly && StartsWith(argument, "--log-file="))
            {
                std::string value = argument.substr(11);

                if (value.empty())
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --log-file.";
                    return context;
                }

                context.logToFile = true;
                context.logFilePath = StripWrappedQuotes(value);
                continue;
            }

            if (!positionalOnly && !argument.empty() && argument[0] == '-')
            {
                context.valid = false;
                context.errorMessage = "Unknown argument: " + argument;
                return context;
            }

            context.positionalArguments.push_back(StripWrappedQuotes(argument));
        }

        return context;
    }

    int RunHarness(int argc, const char* argv[]) noexcept
    {
        try
        {
            return RunHarnessImpl(argc, argv);
        }
        catch (const std::exception& e)
        {
            spdlog::error("cohestra_hades: fatal setup error: {}", e.what());
            return 1;
        }
        catch (...)
        {
            spdlog::error("cohestra_hades: unknown fatal setup error.");
            return 1;
        }
    }

    // split out so RunHarness can stay a noexcept firewall
    int RunHarnessImpl(int argc, const char* argv[])
    {
        Cohestra::ConfigureLogColors();

        auto arguments = ParseHadesArguments(argc, argv);

        if (!arguments.valid)
        {
            spdlog::error("{}", arguments.errorMessage);
            spdlog::info("Use {} --help for usage.", arguments.executableName);
            return 1;
        }

        if (arguments.showHelp)
        {
            spdlog::info("Usage: {} [--config <path>] [--cycles <n>] [--seed <n>] [--backend <loopback|jtag>] [--log-file[=<path>]] [--no-log-file] [--help]",
                arguments.executableName);
            return 0;
        }

        Cohestra::ConfigureFileLogging(arguments);

        spdlog::info("Cohestra Framework with executable built for HADES-Fabric");
        spdlog::info("");
        Cohestra::Logo::PrintToLog();
        spdlog::info("");

        // The [hades] manifest key is mandatory: it defines the channel map
        if (!arguments.configurationPath)
        {
            spdlog::error("cohestra_hades requires --config <path> (the [hades] manifest key is mandatory).");
            return 1;
        }

        Cohestra::INIConfigurationLoader configurationLoader(*arguments.configurationPath);

        if (!configurationLoader.LoadFile())
        {
            spdlog::error("Failed to load configuration file: {}.", *arguments.configurationPath);
            return 1;
        }

        // ---- [hades] section -----------------------------------------------------

        std::string manifestPath  = CONFIG_HADES_MANIFEST.defaultValue;
        std::string backend       = CONFIG_HADES_BACKEND.defaultValue;
        std::string jtagUrl       = CONFIG_HADES_JTAG_URL.defaultValue;
        uint32_t    windowK       = CONFIG_HADES_WINDOW_K.defaultValue;
        uint32_t    skidDepth     = CONFIG_HADES_SKID_DEPTH.defaultValue;
        uint32_t    rxQueue       = CONFIG_HADES_RX_QUEUE.defaultValue;
        uint64_t    maxQuietPolls = CONFIG_HADES_MAX_QUIET_POLLS.defaultValue;
        uint32_t    viewLatency   = CONFIG_HADES_VIEW_LATENCY.defaultValue;
        uint32_t    anchorLRsp    = CONFIG_HADES_ANCHOR_L_RSP.defaultValue;
        uint32_t    anchorLDat    = CONFIG_HADES_ANCHOR_L_DAT.defaultValue;
        uint32_t    anchorLB      = CONFIG_HADES_ANCHOR_L_B.defaultValue;
        uint32_t    anchorLR      = CONFIG_HADES_ANCHOR_L_R.defaultValue;
        uint32_t    maxBatchFrames = CONFIG_HADES_MAX_BATCH_FRAMES.defaultValue;

        if (!ReadHadesKey(configurationLoader, &CONFIG_HADES_MANIFEST, manifestPath) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_BACKEND, backend) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_JTAG_URL, jtagUrl) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_WINDOW_K, windowK) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_SKID_DEPTH, skidDepth) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_RX_QUEUE, rxQueue) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_MAX_QUIET_POLLS, maxQuietPolls) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_VIEW_LATENCY, viewLatency) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_ANCHOR_L_RSP, anchorLRsp) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_ANCHOR_L_DAT, anchorLDat) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_ANCHOR_L_B, anchorLB) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_ANCHOR_L_R, anchorLR) ||
            !ReadHadesKey(configurationLoader, &CONFIG_HADES_MAX_BATCH_FRAMES, maxBatchFrames))
            return 1;

        if (manifestPath.empty())
        {
            spdlog::error("[hades] manifest is empty in {}.", *arguments.configurationPath);
            return 1;
        }

        manifestPath = ResolveManifestPath(manifestPath, *arguments.configurationPath);

        Hades::ManifestInfo manifest = Hades::LoadManifestSubset(manifestPath);

        if (backend.empty())
            backend = manifest.backend.empty() ? "loopback" : manifest.backend;

        if (arguments.backendOverride)
            backend = *arguments.backendOverride;

        if (windowK == 0)
        {
            spdlog::error("[hades] window_k = 0 is invalid (1 = lockstep debug).");
            return 1;
        }

        if (skidDepth == 0)
            skidDepth = windowK;    // the demo wrapper's skid depths track window_k

        // the wrapper's skids are exactly 2*WINDOW_K deep (WINDOW_K ==
        // manifest transport.tick_batch; hades_oceanus_h2c_skid $fatal on
        // overflow): never stage more unaccepted flits than that
        if (skidDepth > 2 * manifest.tickBatch)
        {
            spdlog::warn("[hades] skid_depth = {} exceeds the wrapper's 2*tick_batch ({}); clamping.",
                skidDepth, 2 * manifest.tickBatch);
            skidDepth = 2 * manifest.tickBatch;
        }

        spdlog::info("HADES-Fabric link: design '{}', session '{}', backend '{}', window_k={}, skid_depth={}, rx_queue={}, view_latency={}.",
            manifest.designName, manifest.sessionName, backend, windowK, skidDepth, rxQueue, viewLatency);

        for (const auto& chan : manifest.channels)
            spdlog::info(" - chan {} {:14s} {} {}b token, fifo {}, granule {}.",
                uint32_t(chan.id), chan.name, chan.h2c ? "h2c" : "c2h",
                chan.widthBits, chan.fifoDepth, chan.granule);

        // ---- transport + link ----------------------------------------------------

        // the loopback mock's wire randomness seed; independent of the traffic seed
        const uint64_t linkSeed = 0x48414453ULL;    // "HADS", as LoopbackBackend::kDefaultSeed

        auto transport = Hades::MakeTransport(backend, jtagUrl, manifest, linkSeed);

        auto store = std::make_shared<Hades::HadesFlitStore>(
            Hades::BoundaryConfig { .skidDepth = skidDepth, .rxQueue = rxQueue,
                                    .viewLatency = viewLatency,
                                    .anchorLRsp = anchorLRsp, .anchorLDat = anchorLDat,
                                    .anchorLB = anchorLB, .anchorLR = anchorLR });

        Hades::LinkConfig linkConfig;
        linkConfig.windowK       = windowK;
        linkConfig.maxInflight   = manifest.maxInflight;
        linkConfig.maxQuietPolls = maxQuietPolls;
        linkConfig.viewLatency   = viewLatency;
        // The JTAG bridge's mailbox accumulates the unconfigured fabric's
        // vacuous-tick flood before the session starts; flush it after CTRL.run
        // (hades_link.cpp Reset). In-process backends have no bridge window.
        linkConfig.flushBridgeOnReset = (backend == "jtag");
        // Transport segment cap for h2c staging granules: the Phase J-0 JTAG
        // bridge maps one 64B frame to one AXIS beat, so wide batches must be
        // re-sliced to single-segment frames on this backend (hades_link.cpp
        // ctor). In-process backends keep the full A3 budget.
        if (backend == "jtag")
            linkConfig.maxBatchFrames = maxBatchFrames;

        auto link = std::make_shared<Hades::HadesLink>(
            std::move(transport), manifest, linkConfig, store.get());

        // CCHI + AXI interfaces over the link
        auto cchiInterface = std::make_shared<Hades::HadesCCHIInterface>(link, store);

        spdlog::info("CCHI interface instantiated (HADES link):");
        spdlog::info(" - Type 1 Port 0: Present (the PACKING.md boundary is exactly t1p0)");

        auto axiSlaveInterface = std::make_shared<Hades::HadesAXISlaveInterface>(link, store);

        spdlog::info("AXI slave interface instantiated (HADES link):");
        spdlog::info(" - AXI Slave Port 0: Present (axi_m0, 256-bit)");

        auto chiMonitorInterface = std::make_shared<Hades::HadesCHIMonitorNull>();

        spdlog::info("CHI monitor interface instantiated: no taps on this boundary (null).");

        // ---- instance + stimulators (unchanged above-Instance machinery) ----------

        auto stimulatorRegistry = std::make_shared<Cohestra::StimulatorRegistry>();

        stimulatorRegistry->Register(std::make_shared<Cohestra::FuzzARIFactory>());
        stimulatorRegistry->Register(std::make_shared<Cohestra::Core2CoreFactory>());

        spdlog::info("{} stimulator(s) registered.", stimulatorRegistry->Size());

        Cohestra::Instance instance;

        instance.events->OnUpstreamInitialized.Register(
            Gravity::MakeListener("executable", 0, &OnUpstreamInitialized));
        instance.events->OnUpstreamAbsent.Register(
            Gravity::MakeListener("executable", 0, &OnUpstreamAbsent));
        instance.events->OnStimulatorInstantiated.Register(
            Gravity::MakeListener("executable", 0, &OnStimulatorInstantiated));
        instance.events->OnStimulatorInitialized.Register(
            Gravity::MakeListener("executable", 0, &OnStimulatorInitialized));

        if (!instance.LoadPreInitConfiguration(configurationLoader))
        {
            spdlog::error("Failed to load pre-init configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        instance.Initialize(cchiInterface, stimulatorRegistry);

        if (instance.GetState() != Cohestra::InstanceState::ALIVE)
        {
            spdlog::error("CCHI instance initialization failed, stopped with state {}.",
                instance.GetState() == Cohestra::InstanceState::NOT_INITIALIZED ? "NOT_INITIALIZED" :
                instance.GetState() == Cohestra::InstanceState::FINISHED ? "FINISHED" :
                instance.GetState() == Cohestra::InstanceState::FAILED ? "FAILED" : "UNKNOWN");

            return 1;
        }

        // CLog.B writer: drains the (null) CHI monitor interface; inert unless
        // [clogb] enable = 1, in which case it writes parameters/topology only
        Cohestra::CLogBWriter clogbWriter;

        // Post-init configuration; --seed decorates the loader (fuzz.ari.seed)
        {
            SeedOverrideLoader seedLoader(configurationLoader,
                arguments.seedOverride.value_or(0));

            const Cohestra::StringLoaderBase& postInitLoader =
                arguments.seedOverride ? static_cast<const Cohestra::StringLoaderBase&>(seedLoader)
                                       : static_cast<const Cohestra::StringLoaderBase&>(configurationLoader);

            if (!instance.LoadPostInitConfiguration(postInitLoader))
            {
                spdlog::error("Failed to load post-init configuration from {}.", *arguments.configurationPath);
                return 1;
            }
        }

        if (arguments.seedOverride)
            spdlog::info("FuzzARI seed overridden by --seed: {}.", *arguments.seedOverride);

        if (!clogbWriter.LoadConfiguration(configurationLoader, *arguments.configurationPath))
        {
            spdlog::error("Failed to load CLog.B writer configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        spdlog::info("CCHI instance initialized.");

        // ---- AXI memory (port 0) ---------------------------------------------------

        std::vector<std::shared_ptr<Cohestra::AXIMemory>> axiMemories;

        for (size_t index : axiSlaveInterface->GetPortIndices())
        {
            auto memory = std::make_shared<Cohestra::AXIMemory>(
                axiSlaveInterface, index, static_cast<unsigned int>(index));

            if (!memory->LoadConfiguration(configurationLoader))
            {
                spdlog::error("Failed to load AXI memory configuration from {}.", *arguments.configurationPath);
                return 1;
            }

            // deep B/R FIFOs: the retire echo lags by a link window
            axiSlaveInterface->SetPortBFIFOSize(index, 64);
            axiSlaveInterface->SetPortRFIFOSize(index, 64);

            axiMemories.push_back(memory);

            spdlog::info(" - AXI Slave Port {}: memory model attached ({}-bit data).", index, memory->GetDataBits());
        }

        // ---- upstream event hub + monitors/watchdog/profiler (as v3) ----------------

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

        Cohestra::CacheLineDataMonitor cacheLineDataMonitor(instance);

        if (!cacheLineDataMonitor.LoadConfiguration(configurationLoader))
        {
            spdlog::error("Failed to load cache-line data monitor configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (cacheLineDataMonitor.IsEnabled())
        {
            cacheLineDataMonitor.Attach();
            spdlog::info("Cache-line data monitor attached.");
        }
        else
            spdlog::info("Cache-line data monitor disabled by configuration.");

        Cohestra::TransactionWatchdog watchdog(instance);

        if (!watchdog.LoadConfiguration(configurationLoader))
        {
            spdlog::error("Failed to load transaction watchdog configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (watchdog.IsEnabled())
        {
            watchdog.Attach();
            spdlog::info("Transaction watchdog attached.");
        }
        else
            spdlog::info("Transaction watchdog disabled by configuration.");

        Cohestra::Profiler profiler(instance);

        if (!profiler.LoadConfiguration(configurationLoader))
        {
            spdlog::error("Failed to load profiler configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (profiler.IsEnabled())
        {
            profiler.SetTicksPerCycle(2);   // v3-compatible half-cycle time base
            profiler.Attach();
            spdlog::info("Performance profiler attached.");
        }
        else
            spdlog::info("Performance profiler disabled by configuration.");

        Cohestra::OpCount opCount(instance);

        if (!opCount.LoadConfiguration(configurationLoader))
        {
            spdlog::error("Failed to load opcode counter configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (opCount.IsEnabled())
        {
            opCount.Attach();
            spdlog::info("Upstream opcode counter attached.");
        }
        else
            spdlog::info("Upstream opcode counter disabled by configuration.");

        Cohestra::CCHIFlitLogger cchiFlitLogger(instance);

        if (!cchiFlitLogger.LoadConfiguration(configurationLoader))
        {
            spdlog::error("Failed to load CCHI flit logger configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (cchiFlitLogger.IsEnabled())
        {
            cchiFlitLogger.Attach();
            spdlog::info("CCHI flit logger attached.");
        }
        else
            spdlog::info("CCHI flit logger disabled by configuration.");

        Cohestra::SimPerfMeter simPerfMeter(instance);

        if (!simPerfMeter.LoadConfiguration(configurationLoader))
        {
            spdlog::error("Failed to load simulation performance meter configuration from {}.", *arguments.configurationPath);
            return 1;
        }

        if (simPerfMeter.IsEnabled())
        {
            simPerfMeter.SetTicksPerCycle(2);
            spdlog::info("Simulation performance meter enabled.");
        }
        else
            spdlog::info("Simulation performance meter disabled by configuration.");

        Cohestra::Hades::HadesCompletionReporter completionReporter(instance, *store);
        completionReporter.Attach();

        // CLog.B log open (parameters/topology tags only — the null monitor
        // has no ports; every [clogb] rnf.nodeid.*/snf.nodeid.* key must
        // therefore be absent, which the writer checks)
        if (!clogbWriter.Open(chiMonitorInterface.get()))
        {
            spdlog::error("Failed to open the CLog.B log file.");
            return 1;
        }

        // [log] verbose (raw loader, as v3)
        {
            std::optional<std::string> verboseRaw = configurationLoader.GetRawString(&CONFIG_LOG_VERBOSE);

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

        // ---- boot sequencer (delay only; the Venus-specific sequencers drive ------
        //      v3 pin interfaces that do not exist over this boundary)
        HadesBootHost bootHost {
            .instance            = &instance,
            .cchiInterface       = cchiInterface,
            .axiSlaveInterface   = axiSlaveInterface,
            .chiMonitorInterface = chiMonitorInterface,
            .axiMemories         = &axiMemories
        };

        Cohestra::BootSequencerRegistry<HadesBootHost> bootSequencerRegistry;

        bootSequencerRegistry.Register(
            std::make_shared<Cohestra::DelayBootSequencerFactory<HadesBootHost>>());

        std::unique_ptr<Cohestra::BootSequencer> bootSequencer;

        {
            std::optional<std::string> bootNameRaw = configurationLoader.GetRawString(&CONFIG_BOOT_SEQUENCER);

            if (bootNameRaw && !bootNameRaw->empty())
            {
                auto bootFactory = bootSequencerRegistry.Get(*bootNameRaw);

                if (!bootFactory)
                {
                    spdlog::error("Boot sequencer \"{}\" is not found in the registry "
                        "(cohestra_hades registers only \"delay\").", *bootNameRaw);
                    return 1;
                }

                bootSequencer = bootFactory->Create(bootHost);

                if (!bootSequencer->Initialize(configurationLoader))
                {
                    spdlog::error("Failed to initialize boot sequencer \"{}\".", *bootNameRaw);
                    return 1;
                }

                spdlog::info("Boot sequencer \"{}\" ({}) selected.",
                    bootSequencer->GetName(), bootSequencer->GetDescription());
            }
        }

        // ---- reset stage ------------------------------------------------------------
        // [root] reset_preamble/reset_cycles/reset_repeat (v3-compatible):
        //   * reset_preamble IS meaningful over HADES: it is the post-reset
        //     bubble window — the host drives idle tokens only while the
        //     wrapper's reset stretch (RST_CYCLES=128) runs, because flits
        //     staged during the stretch are flushed by the wrapper without
        //     echo and would leak. Must be >= 128.
        //   * reset_cycles/reset_repeat are NO-OPS: the wrapper's stretcher
        //     owns the DUT reset assertion and re-arms on CTRL.reset.
        uint32_t resetCycles   = CONFIG_RESET_CYCLES.defaultValue;
        uint32_t resetRepeat   = CONFIG_RESET_REPEAT.defaultValue;
        uint32_t resetPreamble = CONFIG_RESET_PREAMBLE.defaultValue;

        if (!ReadHadesKey(configurationLoader, &CONFIG_RESET_CYCLES, resetCycles) ||
            !ReadHadesKey(configurationLoader, &CONFIG_RESET_REPEAT, resetRepeat) ||
            !ReadHadesKey(configurationLoader, &CONFIG_RESET_PREAMBLE, resetPreamble))
            return 1;

        if (resetPreamble < 128)
        {
            spdlog::warn("[root] reset_preamble = {} is below the wrapper's reset stretch "
                "(RST_CYCLES=128): flits staged during the stretch would be flushed without "
                "echo. Raising the bubble window to 128.", resetPreamble);
            resetPreamble = 128;
        }

        if (resetCycles != CONFIG_RESET_CYCLES.defaultValue ||
            resetRepeat != CONFIG_RESET_REPEAT.defaultValue)
            spdlog::warn("[root] reset_cycles/reset_repeat are no-ops over HADES (the FPGA "
                "wrapper owns the DUT reset stretch): cycles={}, repeat={} accepted but unused.",
                resetCycles, resetRepeat);

        spdlog::info("Resetting the HADES link (descriptors -> CTRL.reset -> CTRL.run; "
            "post-reset bubble window {} cycle(s)).", resetPreamble);

        store->Clear();
        store->SetBubbleCycles(resetPreamble);

        if (!link->Reset())
        {
            spdlog::error("HADES link reset failed: {}.", link->LastError());
            return 1;
        }

        // clamp credit to the intended run length (0 = unlimited): the fabric
        // then completes exactly the staged cycles, never runs past them
        link->SetFinalTarget(arguments.maximumCycles);

        uint64_t time  = 0;     // v3-compatible half-cycle log/profiler time base
        uint64_t cycle = 0;     // target cycles since reset (staged 1:1)

        simPerfMeter.Start(time);

        // One simulation cycle. tickInstance differs between the boot and the
        // stimulation stage; the link tail (DriveCycle) stages this cycle's
        // tokens after every push of this tick has landed.
        auto runCycle = [&] (auto&& tickInstance) -> bool
        {
            Cohestra::SetCurrentLogTime(time);

            if (!tickInstance())
            {
                // Tick rejects only when the instance has left ALIVE (FAILED or
                // FINISHED): stop ticking and fall through to the report path
                spdlog::error("CCHI instance stopped ticking at time {}: state is {}.", time,
                    instance.GetState() == Cohestra::InstanceState::FINISHED ? "FINISHED" :
                    instance.GetState() == Cohestra::InstanceState::FAILED ? "FAILED" : "UNKNOWN");
                return false;
            }

            watchdog.Tick(time);

            simPerfMeter.Tick(time);

            for (auto& memory : axiMemories)
                memory->TickPreHandshake(time);

            for (auto& memory : axiMemories)
                memory->TickPostHandshake(time);

            instance.TickPostHandshake(time);

            clogbWriter.DrainAndWrite(chiMonitorInterface.get(), time);

            // link tail: stage + flush + grant + doorbell this target cycle
            if (!link->DriveCycle(cycle))
            {
                spdlog::error("HADES link failed at cycle {}: {}.", cycle, link->LastError());
                instance.SetFailed({ "HADES link failure: " + link->LastError() });
                return false;
            }

            ++cycle;
            time += 2;

            if (arguments.maximumCycles > 0 && cycle >= arguments.maximumCycles)
            {
                instance.SetFinished({"Simulation reached maximum cycle count."});
                return false;
            }

            return true;
        };

        // Boot stage
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
                spdlog::info("Simulation started, maximum {} target cycle(s).", arguments.maximumCycles);
            else
                spdlog::info("Simulation started, no maximum cycle limit.");
        }

        while (instance.IsAlive())
        {
            if (!runCycle([&] () { return instance.Tick(time); }))
                break;
        }

        // let the fabric run out every staged cycle (bounded; c2h tails inside
        // an unfilled emission granule stay behind per the phase-1 contract)
        if (!link->FinalDrain())
        {
            spdlog::error("HADES link final drain failed: {}.", link->LastError());

            if (!instance.IsFailed())
                instance.SetFailed({ "HADES link failure: " + link->LastError() });
        }

        spdlog::info("HADES link totals: staged={} granted={} completed={} unpacked={} "
            "(h2c frames {}, c2h frames {}).",
            link->StagedCycles(), link->GrantedCycles(), link->CompletedCycles(),
            link->UnpackedCycles(), link->H2CFramesPushed(), link->C2HFramesDrained());

        if (store->GetFieldTruncationCount())
            spdlog::warn("HADES boundary protocol warnings: {} field truncation(s).",
                store->GetFieldTruncationCount());

        spdlog::info("HADES boundary: {} benign idle ready echo(es) fence-blocked.",
            store->GetFenceBlockedCount());

        // CLog.B teardown
        clogbWriter.Close();

        // Reports (unchanged above-Instance machinery)
        cacheLineDataMonitor.Report();
        watchdog.Report();
        profiler.Report(time);
        simPerfMeter.Report(time);
        opCount.Report();
        completionReporter.Report();

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
}

#endif
