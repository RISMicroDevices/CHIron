#include "cohestra_lunatic_config.hpp"

#include "spdlog/spdlog.h"

#include <array>
#include <exception>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>

namespace Cohestra {
namespace {

    template<typename T>
    void Read(const StringLoaderBase& loader, const ConfigurationEntryBack<T>& entry,
        T& destination, bool& ok) noexcept
    {
        auto raw = loader.GetRawString(&entry);
        if (!raw)
            return;
        auto value = entry.deserializer(*raw);
        if (!value)
        {
            spdlog::error("Lunatic configuration: invalid [lunatic] {} value: {}.", entry.key, *raw);
            ok = false;
            return;
        }
        destination = *value;
    }

    void ReadUInt32(const StringLoaderBase& loader, const char* key, uint32_t& value, bool& ok) noexcept
    {
        ConfigurationEntryBack<uint32_t> entry("LUNATIC", key, "lunatic", 0,
            DeserializeUInt32, SerializeUInt32);
        Read(loader, entry, value, ok);
    }

    void ReadUInt64(const StringLoaderBase& loader, const char* key, uint64_t& value, bool& ok) noexcept
    {
        ConfigurationEntryBack<uint64_t> entry("LUNATIC", key, "lunatic", 0,
            DeserializeUInt64, SerializeUInt64);
        Read(loader, entry, value, ok);
    }

    void ReadBool(const StringLoaderBase& loader, const char* key, bool& value, bool& ok) noexcept
    {
        ConfigurationEntryBack<bool> entry("LUNATIC", key, "lunatic", false,
            DeserializeBool, SerializeBool);
        Read(loader, entry, value, ok);
    }

    void ReadString(const StringLoaderBase& loader, const char* key, std::string& value, bool& ok) noexcept
    {
        ConfigurationEntryBack<std::string> entry("LUNATIC", key, "lunatic", std::string(),
            DeserializeString, SerializeString);
        Read(loader, entry, value, ok);
    }

    void ReadSize(const StringLoaderBase& loader, const char* key, size_t& value, bool& ok) noexcept
    {
        uint32_t parsed = static_cast<uint32_t>(value);
        ReadUInt32(loader, key, parsed, ok);
        value = parsed;
    }

    ::CHI::Lunatic::NodeId CheckNodeId(uint32_t value, const char* key, bool& ok) noexcept;

    template<typename T>
    void ReadClassArray(const StringLoaderBase& loader, const char* key,
        std::array<T, 4>& values, bool& ok,
        uint64_t maximum = std::numeric_limits<T>::max()) noexcept
    {
        for (size_t index = 0; index < values.size(); ++index)
        {
            const std::string indexedKey = std::string(key) + std::to_string(index);
            uint64_t parsed = values[index];
            ReadUInt64(loader, indexedKey.c_str(), parsed, ok);
            if (parsed > maximum)
            {
                spdlog::error("Lunatic configuration: [lunatic] {} exceeds {}.", indexedKey, maximum);
                ok = false;
            }
            values[index] = static_cast<T>(parsed);
        }
    }

    template<typename T>
    void ReadChoice(const StringLoaderBase& loader, const char* key, T& destination, bool& ok,
        std::initializer_list<std::pair<const char*, T>> choices) noexcept
    {
        std::string value;
        ReadString(loader, key, value, ok);
        if (value.empty())
            return;
        for (const auto& choice : choices)
        {
            if (value == choice.first)
            {
                destination = choice.second;
                return;
            }
        }
        spdlog::error("Lunatic configuration: unknown [lunatic] {} value: {}.", key, value);
        ok = false;
    }

    void ReadOptions(const StringLoaderBase& loader, LunaticModel::Config& options, bool& ok) noexcept
    {
        using namespace ::CHI::Lunatic;

        ReadSize(loader, "queue.input", options.inputQueueLimit, ok);
        ReadSize(loader, "queue.output", options.outputQueueLimit, ok);
        ReadSize(loader, "transaction.limit", options.transactionLimit, ok);
        ReadSize(loader, "snoop.limit", options.snoopLimit, ok);
        ReadSize(loader, "slc.sets", options.slcSets, ok);
        ReadSize(loader, "slc.ways", options.slcWays, ok);
        ReadSize(loader, "snoopfilter.sets", options.snoopFilterSets, ok);
        ReadSize(loader, "snoopfilter.ways", options.snoopFilterWays, ok);
        ReadUInt64(loader, "latency.tag", options.tagLatency, ok);
        ReadUInt64(loader, "latency.data", options.dataLatency, ok);
        ReadUInt64(loader, "latency.homehit", options.homeHitDataLatency, ok);
        ReadUInt64(loader, "latency.snoopdata", options.snoopDataLatency, ok);
        ReadUInt64(loader, "latency.cmowrite", options.cmoWriteLatency, ok);
        ReadUInt64(loader, "latency.memory", options.memoryLatency, ok);
        ReadUInt64(loader, "pipeline.receive", options.receiveDelay, ok);
        ReadUInt64(loader, "pipeline.decode", options.decodeDelay, ok);
        ReadUInt64(loader, "pipeline.admission", options.admissionDelay, ok);
        ReadUInt64(loader, "pipeline.hazard", options.hazardDelay, ok);
        ReadUInt64(loader, "pipeline.lookup", options.lookupDelay, ok);
        ReadUInt64(loader, "pipeline.resolve", options.resolveDelay, ok);
        ReadUInt64(loader, "pipeline.action", options.actionDelay, ok);
        ReadUInt64(loader, "pipeline.return", options.returnDelay, ok);
        ReadUInt64(loader, "pipeline.commit", options.commitDelay, ok);
        ReadUInt64(loader, "random.delay.limit", options.randomDelayLimit, ok);
        ReadUInt64(loader, "random.seed", options.seed, ok);

        uint32_t subordinateId = options.subordinateId;
        ReadUInt32(loader, "subordinate.nodeid", subordinateId, ok);
        options.subordinateId = CheckNodeId(subordinateId, "subordinate.nodeid", ok);
        ReadSize(loader, "axi.outstanding.limit", options.axiOutstandingLimit, ok);
        uint32_t axiIdWidth = options.axiIdWidth;
        ReadUInt32(loader, "axi.id.width", axiIdWidth, ok);
        if (axiIdWidth > 16)
        {
            spdlog::error("Lunatic configuration: [lunatic] axi.id.width exceeds 16.");
            ok = false;
        }
        options.axiIdWidth = static_cast<uint8_t>(axiIdWidth);

        ReadBool(loader, "random.arbitration", options.randomizeArbitration, ok);
        ReadBool(loader, "snoopfilter.precise", options.preciseSnoopFilter, ok);
        ReadBool(loader, "donotgotosd", options.doNotGoToSD, ok);
        ReadBool(loader, "retry.enable", options.retryEnabled, ok);
        ReadBool(loader, "read.separated", options.separatedReadResponse, ok);
        ReadBool(loader, "backend.read.separated", options.separatedBackendRead, ok);
        ReadBool(loader, "dmt.enable", options.dmtEnabled, ok);
        ReadBool(loader, "dct.enable", options.dctEnabled, ok);
        ReadBool(loader, "dwt.enable", options.dwtEnabled, ok);
        ReadBool(loader, "persistence.direct.enable", options.directPersistEnabled, ok);
        ReadBool(loader, "poison.enable", options.poisonEnabled, ok);
        ReadBool(loader, "datacheck.enable", options.dataCheckEnabled, ok);

        ReadChoice(loader, "policy", options.policy, ok, {
            { "exclusive", PolicyKind::Exclusive },
            { "inclusive", PolicyKind::Inclusive },
            { "nine", PolicyKind::NINE }
        });

        ReadSize(loader, "structure.banks", options.structure.bankCount, ok);
        ReadSize(loader, "structure.requestbuffer", options.structure.requestBufferLimit, ok);
        ReadSize(loader, "structure.resource.refill", options.structure.refillResourceLimit, ok);
        ReadSize(loader, "structure.resource.response", options.structure.responseResourceLimit, ok);
        ReadSize(loader, "structure.resource.memory", options.structure.memoryResourceLimit, ok);
        ReadSize(loader, "structure.resource.snoop", options.structure.snoopResourceLimit, ok);
        ReadSize(loader, "structure.snoopqueue", options.structure.outgoingSnoopQueueLimit, ok);
        ReadSize(loader, "structure.transaction.reserved", options.structure.reservedTransactionEntries, ok);
        ReadSize(loader, "structure.retry.sources", options.structure.retrySourceLimit, ok);
        ReadSize(loader, "structure.port.tag", options.structure.tagPortOccupancy, ok);
        ReadSize(loader, "structure.port.data", options.structure.dataPortOccupancy, ok);
        ReadBool(loader, "class.enable", options.structure.classes.enabled, ok);
        ReadClassArray(loader, "class.qos.minimum.", options.structure.classes.qosMinimum, ok, 15);
        ReadClassArray(loader, "class.qos.maximum.", options.structure.classes.qosMaximum, ok, 15);
        ReadClassArray(loader, "class.dedicated.", options.structure.classes.dedicated, ok);
        ReadClassArray(loader, "class.maximum.", options.structure.classes.maximum, ok);
        ReadClassArray(loader, "class.contended.", options.structure.classes.contendedMinimum, ok);
        ReadClassArray(loader, "class.weight.request.", options.structure.classes.requestWeights, ok);
        ReadClassArray(loader, "class.weight.retry.", options.structure.classes.retryWeights, ok);
        ReadUInt64(loader, "class.response.fast", options.structure.classes.fastResponseWeight, ok);
        ReadUInt64(loader, "class.response.retry", options.structure.classes.retryResponseWeight, ok);
        ReadUInt64(loader, "class.response.static", options.structure.classes.staticGrantResponseWeight, ok);
        ReadUInt64(loader, "class.response.normal", options.structure.classes.normalResponseWeight, ok);

        ReadChoice(loader, "strategy.home.replacement", options.strategy.homeReplacement, ok, {
            { "age", ReplacementKind::Age },
            { "plru", ReplacementKind::PLRU },
            { "random", ReplacementKind::PseudoRandom }
        });
        ReadChoice(loader, "strategy.snoopfilter.replacement", options.strategy.snoopFilterReplacement, ok, {
            { "age", ReplacementKind::Age },
            { "plru", ReplacementKind::PLRU },
            { "random", ReplacementKind::PseudoRandom }
        });
        ReadChoice(loader, "strategy.request.arbitration", options.strategy.requestArbitration, ok, {
            { "age", RequestArbitrationKind::Age },
            { "refillfirst", RequestArbitrationKind::RefillFirst },
            { "classweighted", RequestArbitrationKind::OldestClassWeighted }
        });
        ReadChoice(loader, "strategy.output.arbitration", options.strategy.outputArbitration, ok, {
            { "age", OutputArbitrationKind::Age },
            { "classweighted", OutputArbitrationKind::OldestClassWeighted }
        });
        ReadChoice(loader, "strategy.allocation", options.strategy.allocationStrategy, ok, {
            { "policy", AllocationStrategyKind::Policy },
            { "selective", AllocationStrategyKind::SelectiveSharedCopyback }
        });
        ReadChoice(loader, "strategy.atomic", options.strategy.atomicExecution, ok, {
            { "backend", AtomicExecutionKind::Backend },
            { "local", AtomicExecutionKind::Local }
        });
        ReadChoice(loader, "strategy.immediate.response", options.strategy.immediateResponse, ok, {
            { "rotate", ImmediateResponseKind::Rotate },
            { "earlydbid", ImmediateResponseKind::EarlyDBID },
            { "latecomp", ImmediateResponseKind::LateComp },
            { "combined", ImmediateResponseKind::Combined }
        });
        ReadBool(loader, "strategy.copyback.resolve", options.strategy.copybackResponseAtResolve, ok);
        ReadBool(loader, "strategy.cmo.whenrequired", options.strategy.cmoBackendWhenRequired, ok);
        ReadBool(loader, "strategy.cmo.localcompi", options.strategy.cmoLocalCompI, ok);
        ReadBool(loader, "strategy.evict.resolve", options.strategy.evictResponseAtResolve, ok);
        ReadBool(loader, "strategy.snoop.resolve", options.strategy.snoopIssueAtResolve, ok);
        ReadBool(loader, "strategy.snoop.dataport", options.strategy.snoopDataThroughDataPort, ok);
        ReadChoice(loader, "strategy.stash.response", options.strategy.stashResponse, ok, {
            { "rotate", StashResponseKind::Rotate },
            { "compfirst", StashResponseKind::CompFirst },
            { "stashdonefirst", StashResponseKind::StashDoneFirst },
            { "combined", StashResponseKind::Combined }
        });
        ReadChoice(loader, "strategy.persist.response", options.strategy.persistResponse, ok, {
            { "downstream", PersistResponseKind::Downstream },
            { "combined", PersistResponseKind::Combined },
            { "compfirst", PersistResponseKind::SeparateCompFirst },
            { "persistfirst", PersistResponseKind::SeparatePersistFirst }
        });
        ReadChoice(loader, "strategy.retry.upstream", options.strategy.upstreamRetrySelection, ok, {
            { "oldest", RetrySelectionKind::Oldest },
            { "newest", RetrySelectionKind::Newest }
        });
        ReadChoice(loader, "strategy.retry.downstream", options.strategy.downstreamRetrySelection, ok, {
            { "oldest", RetrySelectionKind::Oldest },
            { "newest", RetrySelectionKind::Newest }
        });
        ReadChoice(loader, "strategy.provider", options.strategy.providerSelection, ok, {
            { "lowestnode", ProviderSelectionKind::LowestNodeId },
            { "roundrobin", ProviderSelectionKind::RoundRobin }
        });
        ReadBool(loader, "strategy.dct.forward", options.strategy.forwardDct, ok);

        ReadUInt64(loader, "timing.input", options.timing.inputRegister, ok);
        ReadUInt64(loader, "timing.requestbuffer", options.timing.requestBuffer, ok);
        ReadUInt64(loader, "timing.allocation", options.timing.allocation, ok);
        ReadUInt64(loader, "timing.hazard.release", options.timing.hazardRelease, ok);
        ReadUInt64(loader, "timing.lookup", options.timing.lookupResult, ok);
        ReadUInt64(loader, "timing.snoop.decision", options.timing.snoopDecision, ok);
        ReadUInt64(loader, "timing.snoop.issue", options.timing.snoopIssue, ok);
        ReadUInt64(loader, "timing.data.return", options.timing.dataReturn, ok);
        ReadUInt64(loader, "timing.response.fast", options.timing.fastResponse, ok);
        ReadUInt64(loader, "timing.response.normal", options.timing.normalResponse, ok);
        ReadUInt64(loader, "timing.backend.request", options.timing.backendRequest, ok);
        ReadUInt64(loader, "timing.backend.data", options.timing.backendData, ok);
        ReadUInt64(loader, "timing.retry.ack", options.timing.retryAck, ok);
        ReadUInt64(loader, "timing.credit.grant", options.timing.creditGrant, ok);
        ReadUInt64(loader, "timing.backend.resend", options.timing.backendResend, ok);
        ReadUInt64(loader, "timing.atomic", options.timing.atomicExecution, ok);
        ReadUInt64(loader, "timing.persistence", options.timing.persistence, ok);
        ReadUInt64(loader, "timing.data.beat", options.timing.dataBeat, ok);
        ReadUInt64(loader, "timing.output", options.timing.output, ok);
        ReadUInt64(loader, "timing.commit", options.timing.commitSchedule, ok);
    }

    ::CHI::Lunatic::NodeId CheckNodeId(uint32_t value, const char* key, bool& ok) noexcept
    {
        if (value >= (1u << COHESTRA_CHI_NODEID_WIDTH))
        {
            spdlog::error("Lunatic configuration: [lunatic] {} node ID {} exceeds the compiled field width.",
                key, value);
            ok = false;
        }
        return static_cast<::CHI::Lunatic::NodeId>(value);
    }

}

    bool LoadLunaticIntegrationConfiguration(const StringLoaderBase& loader,
        LunaticIntegrationConfiguration& configuration) noexcept
    {
        using namespace ::CHI::Lunatic;

        bool ok = true;
        configuration.model.rnPorts.clear();
        configuration.model.options = {};
        ReadBool(loader, "enable", configuration.enabled, ok);
        ReadBool(loader, "require.activity", configuration.requireActivity, ok);
        ReadUInt32(loader, "drain.limit.cycles", configuration.drainCycleLimit, ok);

        uint32_t homeNodeId = configuration.model.homeNodeId;
        ReadUInt32(loader, "home.nodeid", homeNodeId, ok);
        configuration.model.homeNodeId = CheckNodeId(homeNodeId, "home.nodeid", ok);
        ReadSize(loader, "service.quantum", configuration.model.serviceQuantum, ok);

        std::string profile;
        ReadString(loader, "profile", profile, ok);
        if (profile.empty() || profile == "custom")
        {
            configuration.model.options.poisonEnabled = COHESTRA_CHI_POISON_PRESENT != 0;
            configuration.model.options.dataCheckEnabled = COHESTRA_CHI_DATACHECK_PRESENT != 0;
        }
        else if (profile == "type3")
            configuration.model.options = MakeType3Options();
        else if (profile == "type7")
            configuration.model.options = MakeType7Options();
        else
        {
            spdlog::error("Lunatic configuration: unknown [lunatic] profile: {}.", profile);
            configuration.model.options.poisonEnabled = COHESTRA_CHI_POISON_PRESENT != 0;
            configuration.model.options.dataCheckEnabled = COHESTRA_CHI_DATACHECK_PRESENT != 0;
            ok = false;
        }
        configuration.model.options.dataWidth = COHESTRA_CHI_DATA_WIDTH;

        std::string backend;
        ReadString(loader, "backend", backend, ok);
        if (backend == "internal")
            configuration.model.options.backend = BackendKind::Internal;
        else if (backend == "axi")
            configuration.model.options.backend = BackendKind::AXI;
        else if (backend == "chi")
            configuration.model.options.backend = BackendKind::CHI;
        else if (!backend.empty())
        {
            spdlog::error("Lunatic configuration: unknown [lunatic] backend: {}.", backend);
            ok = false;
        }

        ReadOptions(loader, configuration.model.options, ok);

        ReadBool(loader, "retry.force", configuration.retryForceOnce, ok);
        if (configuration.retryForceOnce && !configuration.model.options.retryEnabled)
        {
            spdlog::error("Lunatic configuration: retry.force requires retry.enable=1.");
            ok = false;
        }

        std::string dctChoice;
        ReadString(loader, "dct.choice", dctChoice, ok);
        if (dctChoice.empty() || dctChoice == "default")
            configuration.dctChoice = LunaticDCTChoice::Default;
        else if (dctChoice == "normal")
            configuration.dctChoice = LunaticDCTChoice::Normal;
        else if (dctChoice == "forward")
            configuration.dctChoice = LunaticDCTChoice::Forward;
        else
        {
            spdlog::error("Lunatic configuration: unknown [lunatic] dct.choice: {}.", dctChoice);
            ok = false;
        }
        if (configuration.dctChoice != LunaticDCTChoice::Default && !configuration.model.options.dctEnabled)
        {
            spdlog::error("Lunatic configuration: a non-default dct.choice requires dct.enable=1.");
            ok = false;
        }

        bool mte = false;
        bool dvm = false;
        ReadBool(loader, "mte.enable", mte, ok);
        ReadBool(loader, "dvm.enable", dvm, ok);
        configuration.model.options.mteEnabled = mte;
        configuration.model.options.dvmEnabled = dvm;
        if (mte || dvm)
        {
            spdlog::error("Lunatic configuration: MTE and DVM are prohibited.");
            ok = false;
        }

        for (size_t port = 0; port < 8; ++port)
        {
            const std::string key = "rnf.nodeid." + std::to_string(port);
            uint32_t nodeId = 0;
            ConfigurationEntryBack<uint32_t> entry("LUNATIC_RN_NODE_ID", key.c_str(), "lunatic", 0,
                DeserializeUInt32, SerializeUInt32);
            if (!loader.GetRawString(&entry))
                continue;
            Read(loader, entry, nodeId, ok);
            configuration.model.rnPorts.push_back({ port, CheckNodeId(nodeId, key.c_str(), ok) });
        }

        if (configuration.enabled && configuration.model.rnPorts.empty())
        {
            spdlog::error("Lunatic configuration: at least one [lunatic] rnf.nodeid.<port> entry is required.");
            ok = false;
        }
        if (!configuration.drainCycleLimit)
        {
            spdlog::error("Lunatic configuration: drain.limit.cycles must be non-zero.");
            ok = false;
        }
        if (configuration.model.options.poisonEnabled && COHESTRA_CHI_POISON_PRESENT == 0)
        {
            spdlog::error("Lunatic configuration: poison.enable requires a typed Poison field.");
            ok = false;
        }
        if (configuration.model.options.dataCheckEnabled && COHESTRA_CHI_DATACHECK_PRESENT == 0)
        {
            spdlog::error("Lunatic configuration: datacheck.enable requires a typed DataCheck field.");
            ok = false;
        }
        try
        {
            ValidateOptions(configuration.model.options);
        }
        catch (const std::exception& error)
        {
            spdlog::error("Lunatic configuration: {}.", error.what());
            ok = false;
        }
        return ok;
    }

}
