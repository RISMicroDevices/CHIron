#include "cohestra_lunatic_config.hpp"

#include "spdlog/spdlog.h"

#include <string>

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

    void ReadOptions(const StringLoaderBase& loader, LunaticModel::Config& options, bool& ok) noexcept
    {
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

        std::string policy;
        ReadString(loader, "policy", policy, ok);
        if (policy.empty() || policy == "exclusive")
            options.policy = ::CHI::Lunatic::PolicyKind::Exclusive;
        else if (policy == "inclusive")
            options.policy = ::CHI::Lunatic::PolicyKind::Inclusive;
        else if (policy == "nine")
            options.policy = ::CHI::Lunatic::PolicyKind::NINE;
        else
        {
            spdlog::error("Lunatic configuration: unknown [lunatic] policy: {}.", policy);
            ok = false;
        }
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
        bool ok = true;
        configuration.model.rnPorts.clear();
        ReadBool(loader, "enable", configuration.enabled, ok);
        ReadBool(loader, "require.activity", configuration.requireActivity, ok);
        ReadUInt32(loader, "drain.limit.cycles", configuration.drainCycleLimit, ok);

        uint32_t homeNodeId = configuration.model.homeNodeId;
        ReadUInt32(loader, "home.nodeid", homeNodeId, ok);
        configuration.model.homeNodeId = CheckNodeId(homeNodeId, "home.nodeid", ok);
        ReadSize(loader, "service.quantum", configuration.model.serviceQuantum, ok);

        configuration.model.options.dataWidth = COHESTRA_CHI_DATA_WIDTH;
        configuration.model.options.poisonEnabled = COHESTRA_CHI_POISON_PRESENT != 0;
        configuration.model.options.dataCheckEnabled = COHESTRA_CHI_DATACHECK_PRESENT != 0;

        std::string backend;
        ReadString(loader, "backend", backend, ok);
        if (backend.empty() || backend == "internal")
            configuration.model.options.backend = ::CHI::Lunatic::BackendKind::Internal;
        else if (backend == "axi")
            configuration.model.options.backend = ::CHI::Lunatic::BackendKind::AXI;
        else if (backend == "chi")
            configuration.model.options.backend = ::CHI::Lunatic::BackendKind::CHI;
        else
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
        return ok;
    }

}
