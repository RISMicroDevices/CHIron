#include "cohestra_lunatic_config.hpp"
#include "common.hpp"

#include <array>
#include <map>
#include <string>

using namespace CHI::Lunatic;
using namespace Cohestra;
using namespace LunaticTest;

namespace {

    class MapLoader final : public StringLoaderBase {
    public:
        std::map<std::string, std::string> values;

        explicit MapLoader(const std::string& profile = {})
        {
            if (!profile.empty())
                values["lunatic.profile"] = profile;
        }

        std::optional<std::string> GetRawString(ConfigurationEntryBase entry,
            const std::string& defaultSection = "global") const noexcept override
        {
            const std::string key = std::string(entry->section ? entry->section : defaultSection.c_str()) +
                "." + entry->key;
            const auto iter = values.find(key);
            return iter == values.end() ? std::nullopt : std::optional<std::string>(iter->second);
        }
    };

    void AllowCompiledMetadata(MapLoader& loader)
    {
#if COHESTRA_CHI_POISON_PRESENT == 0
        loader.values["lunatic.poison.enable"] = "0";
#endif
#if COHESTRA_CHI_DATACHECK_PRESENT == 0
        loader.values["lunatic.datacheck.enable"] = "0";
#endif
    }

    bool Load(MapLoader& loader, LunaticIntegrationConfiguration& configuration)
    {
        return LoadLunaticIntegrationConfiguration(loader, configuration);
    }

}

LUNATIC_TEST(Stage10ProfileConfigCustomResetsPreviousSelection)
{
    MapLoader loader;
    LunaticIntegrationConfiguration configuration;
    configuration.model.options = MakeType7Options();

    LUNATIC_CHECK(Load(loader, configuration));
    const Options& options = configuration.model.options;
    LUNATIC_CHECK(options.profile == ProfileKind::Custom);
    LUNATIC_CHECK(options.structure.bankCount == 1);
    LUNATIC_CHECK(options.backend == BackendKind::Internal);
    LUNATIC_CHECK(options.strategy.requestArbitration == RequestArbitrationKind::Age);
    LUNATIC_CHECK(options.poisonEnabled == (COHESTRA_CHI_POISON_PRESENT != 0));
    LUNATIC_CHECK(options.dataCheckEnabled == (COHESTRA_CHI_DATACHECK_PRESENT != 0));
}

LUNATIC_TEST(Stage10ProfileConfigSelectsType3)
{
    MapLoader loader("type3");
    AllowCompiledMetadata(loader);
    LunaticIntegrationConfiguration configuration;

    LUNATIC_CHECK(Load(loader, configuration));
    const Options& options = configuration.model.options;
    LUNATIC_CHECK(options.profile == ProfileKind::Type3);
    LUNATIC_CHECK(options.structure.bankCount == 4);
    LUNATIC_CHECK(options.structure.requestBufferLimit == 8);
    LUNATIC_CHECK(options.structure.refillResourceLimit == 16);
    LUNATIC_CHECK(options.structure.outgoingSnoopQueueLimit == 1);
    LUNATIC_CHECK(options.transactionLimit == 64);
    LUNATIC_CHECK(options.slcSets == 128 && options.slcWays == 4);
    LUNATIC_CHECK(options.snoopFilterSets == 128 && options.snoopFilterWays == 4);
    LUNATIC_CHECK(options.inputQueueLimit == 15);
    LUNATIC_CHECK(options.backend == BackendKind::CHI);
    LUNATIC_CHECK(options.policy == PolicyKind::NINE);
    LUNATIC_CHECK(!options.retryEnabled);
    LUNATIC_CHECK(options.strategy.homeReplacement == ReplacementKind::PLRU);
    LUNATIC_CHECK(options.strategy.snoopFilterReplacement == ReplacementKind::PseudoRandom);
    LUNATIC_CHECK(options.strategy.requestArbitration == RequestArbitrationKind::RefillFirst);
    LUNATIC_CHECK(options.strategy.allocationStrategy == AllocationStrategyKind::SelectiveSharedCopyback);
    LUNATIC_CHECK(options.tagLatency == 1);
    LUNATIC_CHECK(options.dataLatency == 3);
    LUNATIC_CHECK(options.homeHitDataLatency == 2);
    LUNATIC_CHECK(options.strategy.copybackResponseAtResolve);
    LUNATIC_CHECK(options.strategy.snoopIssueAtResolve);
    LUNATIC_CHECK(options.strategy.snoopDataThroughDataPort);
}

LUNATIC_TEST(Stage10ProfileConfigSelectsType7)
{
    MapLoader loader("type7");
    AllowCompiledMetadata(loader);
    LunaticIntegrationConfiguration configuration;

    LUNATIC_CHECK(Load(loader, configuration));
    const Options& options = configuration.model.options;
    LUNATIC_CHECK(options.profile == ProfileKind::Type7);
    LUNATIC_CHECK(options.structure.bankCount == 1);
    LUNATIC_CHECK(options.structure.outgoingSnoopQueueLimit == 2);
    LUNATIC_CHECK(options.structure.reservedTransactionEntries == 1);
    LUNATIC_CHECK(options.structure.retrySourceLimit == 5);
    LUNATIC_CHECK(options.structure.classes.enabled);
    LUNATIC_CHECK(options.structure.classes.maximum == (std::array<size_t, 4>{ 15, 14, 8, 2 }));
    LUNATIC_CHECK(options.structure.classes.contendedMinimum == (std::array<size_t, 4>{ 4, 4, 4, 4 }));
    LUNATIC_CHECK(options.structure.classes.fastResponseWeight == 63);
    LUNATIC_CHECK(options.slcSets == 1024 && options.slcWays == 16);
    LUNATIC_CHECK(options.snoopFilterSets == 4096 && options.snoopFilterWays == 16);
    LUNATIC_CHECK(options.dmtEnabled && options.dctEnabled);
    LUNATIC_CHECK(options.strategy.atomicExecution == AtomicExecutionKind::Local);
    LUNATIC_CHECK(options.strategy.immediateResponse == ImmediateResponseKind::EarlyDBID);
    LUNATIC_CHECK(options.strategy.requestArbitration == RequestArbitrationKind::OldestClassWeighted);
}

LUNATIC_TEST(Stage10ProfileConfigOverridesEveryOptionGroup)
{
    MapLoader loader("type7");
    AllowCompiledMetadata(loader);
    loader.values["lunatic.structure.requestbuffer"] = "9";
    loader.values["lunatic.structure.resource.refill"] = "10";
    loader.values["lunatic.structure.resource.response"] = "11";
    loader.values["lunatic.structure.resource.memory"] = "12";
    loader.values["lunatic.structure.resource.snoop"] = "13";
    loader.values["lunatic.structure.snoopqueue"] = "14";
    loader.values["lunatic.structure.transaction.reserved"] = "2";
    loader.values["lunatic.structure.retry.sources"] = "7";
    loader.values["lunatic.structure.port.tag"] = "2";
    loader.values["lunatic.structure.port.data"] = "3";
    loader.values["lunatic.class.qos.minimum.1"] = "13";
    loader.values["lunatic.class.maximum.3"] = "3";
    loader.values["lunatic.class.dedicated.3"] = "1";
    loader.values["lunatic.class.contended.3"] = "4";
    loader.values["lunatic.class.weight.request.0"] = "5";
    loader.values["lunatic.class.weight.retry.0"] = "6";
    loader.values["lunatic.class.response.fast"] = "7";
    loader.values["lunatic.class.response.retry"] = "8";
    loader.values["lunatic.class.response.static"] = "9";
    loader.values["lunatic.class.response.normal"] = "10";
    loader.values["lunatic.policy"] = "nine";
    loader.values["lunatic.strategy.home.replacement"] = "plru";
    loader.values["lunatic.strategy.snoopfilter.replacement"] = "age";
    loader.values["lunatic.strategy.request.arbitration"] = "classweighted";
    loader.values["lunatic.strategy.output.arbitration"] = "age";
    loader.values["lunatic.strategy.allocation"] = "selective";
    loader.values["lunatic.strategy.atomic"] = "backend";
    loader.values["lunatic.strategy.immediate.response"] = "latecomp";
    loader.values["lunatic.strategy.copyback.resolve"] = "1";
    loader.values["lunatic.strategy.snoop.resolve"] = "1";
    loader.values["lunatic.strategy.snoop.dataport"] = "1";
    loader.values["lunatic.strategy.stash.response"] = "combined";
    loader.values["lunatic.strategy.persist.response"] = "compfirst";
    loader.values["lunatic.strategy.retry.upstream"] = "oldest";
    loader.values["lunatic.strategy.retry.downstream"] = "oldest";
    loader.values["lunatic.strategy.provider"] = "roundrobin";
    loader.values["lunatic.strategy.dct.forward"] = "0";
    loader.values["lunatic.latency.homehit"] = "2";
    loader.values["lunatic.latency.snoopdata"] = "4";
    loader.values["lunatic.timing.input"] = "1";
    loader.values["lunatic.timing.requestbuffer"] = "2";
    loader.values["lunatic.timing.allocation"] = "3";
    loader.values["lunatic.timing.lookup"] = "4";
    loader.values["lunatic.timing.snoop.decision"] = "5";
    loader.values["lunatic.timing.snoop.issue"] = "6";
    loader.values["lunatic.timing.data.return"] = "7";
    loader.values["lunatic.timing.response.fast"] = "8";
    loader.values["lunatic.timing.response.normal"] = "9";
    loader.values["lunatic.timing.backend.request"] = "10";
    loader.values["lunatic.timing.backend.data"] = "11";
    loader.values["lunatic.timing.retry.ack"] = "12";
    loader.values["lunatic.timing.credit.grant"] = "13";
    loader.values["lunatic.timing.backend.resend"] = "14";
    loader.values["lunatic.timing.atomic"] = "15";
    loader.values["lunatic.timing.persistence"] = "16";
    loader.values["lunatic.timing.data.beat"] = "17";
    loader.values["lunatic.timing.output"] = "18";
    loader.values["lunatic.timing.commit"] = "19";

    LunaticIntegrationConfiguration configuration;
    LUNATIC_CHECK(Load(loader, configuration));
    const Options& options = configuration.model.options;
    LUNATIC_CHECK(options.backend == BackendKind::CHI);
    LUNATIC_CHECK(options.policy == PolicyKind::NINE);
    LUNATIC_CHECK(options.structure.requestBufferLimit == 9);
    LUNATIC_CHECK(options.structure.refillResourceLimit == 10);
    LUNATIC_CHECK(options.structure.responseResourceLimit == 11);
    LUNATIC_CHECK(options.structure.memoryResourceLimit == 12);
    LUNATIC_CHECK(options.structure.snoopResourceLimit == 13);
    LUNATIC_CHECK(options.structure.outgoingSnoopQueueLimit == 14);
    LUNATIC_CHECK(options.structure.reservedTransactionEntries == 2);
    LUNATIC_CHECK(options.structure.retrySourceLimit == 7);
    LUNATIC_CHECK(options.structure.tagPortOccupancy == 2);
    LUNATIC_CHECK(options.structure.dataPortOccupancy == 3);
    LUNATIC_CHECK(options.structure.classes.qosMinimum[1] == 13);
    LUNATIC_CHECK(options.structure.classes.maximum[3] == 3);
    LUNATIC_CHECK(options.structure.classes.dedicated[3] == 1);
    LUNATIC_CHECK(options.structure.classes.contendedMinimum[3] == 4);
    LUNATIC_CHECK(options.structure.classes.requestWeights[0] == 5);
    LUNATIC_CHECK(options.structure.classes.retryWeights[0] == 6);
    LUNATIC_CHECK(options.structure.classes.fastResponseWeight == 7);
    LUNATIC_CHECK(options.structure.classes.retryResponseWeight == 8);
    LUNATIC_CHECK(options.structure.classes.staticGrantResponseWeight == 9);
    LUNATIC_CHECK(options.structure.classes.normalResponseWeight == 10);
    LUNATIC_CHECK(options.strategy.homeReplacement == ReplacementKind::PLRU);
    LUNATIC_CHECK(options.strategy.snoopFilterReplacement == ReplacementKind::Age);
    LUNATIC_CHECK(options.strategy.outputArbitration == OutputArbitrationKind::Age);
    LUNATIC_CHECK(options.strategy.allocationStrategy == AllocationStrategyKind::SelectiveSharedCopyback);
    LUNATIC_CHECK(options.strategy.atomicExecution == AtomicExecutionKind::Backend);
    LUNATIC_CHECK(options.strategy.immediateResponse == ImmediateResponseKind::LateComp);
    LUNATIC_CHECK(options.strategy.copybackResponseAtResolve);
    LUNATIC_CHECK(options.strategy.snoopIssueAtResolve);
    LUNATIC_CHECK(options.strategy.snoopDataThroughDataPort);
    LUNATIC_CHECK(options.strategy.stashResponse == StashResponseKind::Combined);
    LUNATIC_CHECK(options.strategy.persistResponse == PersistResponseKind::SeparateCompFirst);
    LUNATIC_CHECK(options.strategy.upstreamRetrySelection == RetrySelectionKind::Oldest);
    LUNATIC_CHECK(options.strategy.downstreamRetrySelection == RetrySelectionKind::Oldest);
    LUNATIC_CHECK(options.strategy.providerSelection == ProviderSelectionKind::RoundRobin);
    LUNATIC_CHECK(!options.strategy.forwardDct);
    LUNATIC_CHECK(options.homeHitDataLatency == 2);
    LUNATIC_CHECK(options.snoopDataLatency == 4);
    LUNATIC_CHECK(options.timing.inputRegister == 1);
    LUNATIC_CHECK(options.timing.requestBuffer == 2);
    LUNATIC_CHECK(options.timing.allocation == 3);
    LUNATIC_CHECK(options.timing.lookupResult == 4);
    LUNATIC_CHECK(options.timing.snoopDecision == 5);
    LUNATIC_CHECK(options.timing.snoopIssue == 6);
    LUNATIC_CHECK(options.timing.dataReturn == 7);
    LUNATIC_CHECK(options.timing.fastResponse == 8);
    LUNATIC_CHECK(options.timing.normalResponse == 9);
    LUNATIC_CHECK(options.timing.backendRequest == 10);
    LUNATIC_CHECK(options.timing.backendData == 11);
    LUNATIC_CHECK(options.timing.retryAck == 12);
    LUNATIC_CHECK(options.timing.creditGrant == 13);
    LUNATIC_CHECK(options.timing.backendResend == 14);
    LUNATIC_CHECK(options.timing.atomicExecution == 15);
    LUNATIC_CHECK(options.timing.persistence == 16);
    LUNATIC_CHECK(options.timing.dataBeat == 17);
    LUNATIC_CHECK(options.timing.output == 18);
    LUNATIC_CHECK(options.timing.commitSchedule == 19);
}

LUNATIC_TEST(Stage10ProfileConfigRejectsInvalidSelections)
{
    LunaticIntegrationConfiguration configuration;
    MapLoader unknownProfile("invalid");
    LUNATIC_CHECK(!Load(unknownProfile, configuration));

    MapLoader unknownChoice("type3");
    AllowCompiledMetadata(unknownChoice);
    unknownChoice.values["lunatic.strategy.home.replacement"] = "invalid";
    LUNATIC_CHECK(!Load(unknownChoice, configuration));

    MapLoader overlappingClasses("type7");
    AllowCompiledMetadata(overlappingClasses);
    overlappingClasses.values["lunatic.class.qos.minimum.0"] = "14";
    LUNATIC_CHECK(!Load(overlappingClasses, configuration));

    MapLoader missingClasses("type3");
    AllowCompiledMetadata(missingClasses);
    missingClasses.values["lunatic.strategy.request.arbitration"] = "classweighted";
    LUNATIC_CHECK(!Load(missingClasses, configuration));
}

LUNATIC_TEST(Stage10ProfileConfigChecksCompiledMetadata)
{
    MapLoader loader("type3");
    LunaticIntegrationConfiguration configuration;
#if COHESTRA_CHI_POISON_PRESENT == 0 || COHESTRA_CHI_DATACHECK_PRESENT == 0
    LUNATIC_CHECK(!Load(loader, configuration));
    AllowCompiledMetadata(loader);
    LUNATIC_CHECK(Load(loader, configuration));
#else
    LUNATIC_CHECK(Load(loader, configuration));
    LUNATIC_CHECK(configuration.model.options.poisonEnabled);
    LUNATIC_CHECK(configuration.model.options.dataCheckEnabled);
#endif
}
