#include "common.hpp"

#include <array>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using ProfileConfig256 = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 256, true, true, false>;

    void CheckLegacyAuxiliaryDefaults(const Options& options)
    {
        LUNATIC_CHECK(options.memoryLatency == 8);
        LUNATIC_CHECK(options.homeHitDataLatency == (options.profile == ProfileKind::Type3 ? 2 : 0));
        LUNATIC_CHECK(options.snoopDataLatency == (options.profile == ProfileKind::Type3 ? 4 : 0));
        LUNATIC_CHECK(options.receiveDelay == 0);
        LUNATIC_CHECK(options.decodeDelay == 0);
        LUNATIC_CHECK(options.admissionDelay == 0);
        LUNATIC_CHECK(options.hazardDelay == 0);
        LUNATIC_CHECK(options.lookupDelay == 0);
        LUNATIC_CHECK(options.resolveDelay == 0);
        LUNATIC_CHECK(options.actionDelay == 0);
        LUNATIC_CHECK(options.returnDelay == 0);
        LUNATIC_CHECK(options.commitDelay == 0);
        LUNATIC_CHECK(options.randomDelayLimit == 0);
        LUNATIC_CHECK(options.seed == 0);
        LUNATIC_CHECK(options.subordinateId == 0);
        LUNATIC_CHECK(options.axiOutstandingLimit == 1);
        LUNATIC_CHECK(options.axiIdWidth == 4);
        LUNATIC_CHECK(!options.randomizeArbitration);
    }

    void CheckClassOptions(const ClassOptions& classes, bool enabled)
    {
        LUNATIC_CHECK(classes.enabled == enabled);
        LUNATIC_CHECK((classes.qosMinimum == std::array<uint8_t, 4>{ 15, 12, 8, 0 }));
        LUNATIC_CHECK((classes.qosMaximum == std::array<uint8_t, 4>{ 15, 14, 11, 7 }));
        LUNATIC_CHECK((classes.dedicated == std::array<size_t, 4>{ 0, 0, 0, 0 }));
        LUNATIC_CHECK((classes.maximum == std::array<size_t, 4>{ 15, 14, 8, 2 }));
        LUNATIC_CHECK((classes.contendedMinimum == std::array<size_t, 4>{ 4, 4, 4, 4 }));
        LUNATIC_CHECK((classes.requestWeights == std::array<uint64_t, 4>{ 0, 0, 0, 0 }));
        LUNATIC_CHECK((classes.retryWeights == std::array<uint64_t, 4>{ 0, 0, 0, 0 }));
        LUNATIC_CHECK(classes.fastResponseWeight == 63);
        LUNATIC_CHECK(classes.retryResponseWeight == 1);
        LUNATIC_CHECK(classes.staticGrantResponseWeight == 1);
        LUNATIC_CHECK(classes.normalResponseWeight == 1);
    }

    void CheckZeroTiming(const OperationTiming& timing)
    {
        LUNATIC_CHECK(timing.inputRegister == 0);
        LUNATIC_CHECK(timing.requestBuffer == 0);
        LUNATIC_CHECK(timing.allocation == 0);
        LUNATIC_CHECK(timing.lookupResult == 0);
        LUNATIC_CHECK(timing.snoopDecision == 0);
        LUNATIC_CHECK(timing.snoopIssue == 0);
        LUNATIC_CHECK(timing.dataReturn == 0);
        LUNATIC_CHECK(timing.fastResponse == 0);
        LUNATIC_CHECK(timing.normalResponse == 0);
        LUNATIC_CHECK(timing.backendRequest == 0);
        LUNATIC_CHECK(timing.backendData == 0);
        LUNATIC_CHECK(timing.retryAck == 0);
        LUNATIC_CHECK(timing.creditGrant == 0);
        LUNATIC_CHECK(timing.backendResend == 0);
        LUNATIC_CHECK(timing.atomicExecution == 0);
        LUNATIC_CHECK(timing.persistence == 0);
        LUNATIC_CHECK(timing.dataBeat == 0);
        LUNATIC_CHECK(timing.output == 0);
        LUNATIC_CHECK(timing.commitSchedule == 0);
    }

    void CheckCustomOptions(const Options& options)
    {
        ValidateOptions(options);
        LUNATIC_CHECK(options.inputQueueLimit == 16);
        LUNATIC_CHECK(options.outputQueueLimit == 16);
        LUNATIC_CHECK(options.transactionLimit == 16);
        LUNATIC_CHECK(options.snoopLimit == 32);
        LUNATIC_CHECK(options.slcSets == 1024);
        LUNATIC_CHECK(options.slcWays == 16);
        LUNATIC_CHECK(options.snoopFilterSets == 4096);
        LUNATIC_CHECK(options.snoopFilterWays == 16);
        LUNATIC_CHECK(options.dataWidth == 256);
        LUNATIC_CHECK(options.tagLatency == 2);
        LUNATIC_CHECK(options.dataLatency == 2);
        CheckLegacyAuxiliaryDefaults(options);
        LUNATIC_CHECK(options.policy == PolicyKind::Exclusive);
        LUNATIC_CHECK(options.backend == BackendKind::Internal);
        LUNATIC_CHECK(options.preciseSnoopFilter);
        LUNATIC_CHECK(!options.doNotGoToSD);
        LUNATIC_CHECK(options.retryEnabled);
        LUNATIC_CHECK(!options.separatedReadResponse);
        LUNATIC_CHECK(!options.separatedBackendRead);
        LUNATIC_CHECK(!options.dmtEnabled);
        LUNATIC_CHECK(!options.dctEnabled);
        LUNATIC_CHECK(!options.dwtEnabled);
        LUNATIC_CHECK(!options.directPersistEnabled);
        LUNATIC_CHECK(!options.poisonEnabled);
        LUNATIC_CHECK(!options.dataCheckEnabled);
        LUNATIC_CHECK(!options.mteEnabled);
        LUNATIC_CHECK(!options.dvmEnabled);
        LUNATIC_CHECK(options.profile == ProfileKind::Custom);

        LUNATIC_CHECK(options.structure.bankCount == 1);
        LUNATIC_CHECK(options.structure.requestBufferLimit == 0);
        LUNATIC_CHECK(options.structure.refillResourceLimit == 0);
        LUNATIC_CHECK(options.structure.responseResourceLimit == 0);
        LUNATIC_CHECK(options.structure.memoryResourceLimit == 0);
        LUNATIC_CHECK(options.structure.snoopResourceLimit == 0);
        LUNATIC_CHECK(options.structure.outgoingSnoopQueueLimit == 0);
        LUNATIC_CHECK(options.structure.reservedTransactionEntries == 0);
        LUNATIC_CHECK(options.structure.retrySourceLimit == 0);
        LUNATIC_CHECK(options.structure.tagPortOccupancy == 1);
        LUNATIC_CHECK(options.structure.dataPortOccupancy == 1);
        CheckClassOptions(options.structure.classes, false);

        LUNATIC_CHECK(options.strategy.homeReplacement == ReplacementKind::Age);
        LUNATIC_CHECK(options.strategy.snoopFilterReplacement == ReplacementKind::Age);
        LUNATIC_CHECK(options.strategy.requestArbitration == RequestArbitrationKind::Age);
        LUNATIC_CHECK(options.strategy.outputArbitration == OutputArbitrationKind::Age);
        LUNATIC_CHECK(options.strategy.allocationStrategy == AllocationStrategyKind::Policy);
        LUNATIC_CHECK(options.strategy.atomicExecution == AtomicExecutionKind::Backend);
        LUNATIC_CHECK(options.strategy.immediateResponse == ImmediateResponseKind::Rotate);
        LUNATIC_CHECK(options.strategy.stashResponse == StashResponseKind::Rotate);
        LUNATIC_CHECK(options.strategy.persistResponse == PersistResponseKind::Downstream);
        LUNATIC_CHECK(options.strategy.upstreamRetrySelection == RetrySelectionKind::Newest);
        LUNATIC_CHECK(options.strategy.downstreamRetrySelection == RetrySelectionKind::Newest);
        LUNATIC_CHECK(options.strategy.providerSelection == ProviderSelectionKind::LowestNodeId);
        LUNATIC_CHECK(options.strategy.forwardDct);
        LUNATIC_CHECK(options.strategy.copybackResponseAtResolve == (options.profile == ProfileKind::Type3));
        LUNATIC_CHECK(options.strategy.snoopIssueAtResolve == (options.profile == ProfileKind::Type3));
        LUNATIC_CHECK(options.strategy.snoopDataThroughDataPort == (options.profile == ProfileKind::Type3));
        CheckZeroTiming(options.timing);
    }

    void CheckType3Options(const Options& options)
    {
        ValidateOptions(options);
        LUNATIC_CHECK(options.profile == ProfileKind::Type3);
        LUNATIC_CHECK(options.inputQueueLimit == 15);
        LUNATIC_CHECK(options.outputQueueLimit == 16);
        LUNATIC_CHECK(options.transactionLimit == 64);
        LUNATIC_CHECK(options.snoopLimit == 32);
        LUNATIC_CHECK(options.slcSets == 128);
        LUNATIC_CHECK(options.slcWays == 4);
        LUNATIC_CHECK(options.snoopFilterSets == 128);
        LUNATIC_CHECK(options.snoopFilterWays == 4);
        LUNATIC_CHECK(options.dataWidth == 256);
        LUNATIC_CHECK(options.tagLatency == 1);
        LUNATIC_CHECK(options.dataLatency == 3);
        CheckLegacyAuxiliaryDefaults(options);
        LUNATIC_CHECK(options.policy == PolicyKind::NINE);
        LUNATIC_CHECK(options.backend == BackendKind::CHI);
        LUNATIC_CHECK(options.preciseSnoopFilter);
        LUNATIC_CHECK(options.doNotGoToSD);
        LUNATIC_CHECK(!options.retryEnabled);
        LUNATIC_CHECK(!options.separatedReadResponse);
        LUNATIC_CHECK(!options.separatedBackendRead);
        LUNATIC_CHECK(!options.dmtEnabled);
        LUNATIC_CHECK(!options.dctEnabled);
        LUNATIC_CHECK(!options.dwtEnabled);
        LUNATIC_CHECK(!options.directPersistEnabled);
        LUNATIC_CHECK(options.poisonEnabled);
        LUNATIC_CHECK(options.dataCheckEnabled);
        LUNATIC_CHECK(!options.mteEnabled);
        LUNATIC_CHECK(!options.dvmEnabled);

        LUNATIC_CHECK(options.structure.bankCount == 4);
        LUNATIC_CHECK(options.structure.requestBufferLimit == 8);
        LUNATIC_CHECK(options.structure.refillResourceLimit == 16);
        LUNATIC_CHECK(options.structure.responseResourceLimit == 16);
        LUNATIC_CHECK(options.structure.memoryResourceLimit == 16);
        LUNATIC_CHECK(options.structure.snoopResourceLimit == 16);
        LUNATIC_CHECK(options.structure.outgoingSnoopQueueLimit == 1);
        LUNATIC_CHECK(options.structure.reservedTransactionEntries == 0);
        LUNATIC_CHECK(options.structure.retrySourceLimit == 0);
        LUNATIC_CHECK(options.structure.tagPortOccupancy == 1);
        LUNATIC_CHECK(options.structure.dataPortOccupancy == 1);
        CheckClassOptions(options.structure.classes, false);

        LUNATIC_CHECK(options.strategy.homeReplacement == ReplacementKind::PLRU);
        LUNATIC_CHECK(options.strategy.snoopFilterReplacement == ReplacementKind::PseudoRandom);
        LUNATIC_CHECK(options.strategy.requestArbitration == RequestArbitrationKind::RefillFirst);
        LUNATIC_CHECK(options.strategy.outputArbitration == OutputArbitrationKind::Age);
        LUNATIC_CHECK(options.strategy.allocationStrategy == AllocationStrategyKind::SelectiveSharedCopyback);
        LUNATIC_CHECK(options.strategy.atomicExecution == AtomicExecutionKind::Backend);
        LUNATIC_CHECK(options.strategy.immediateResponse == ImmediateResponseKind::Rotate);
        LUNATIC_CHECK(options.strategy.stashResponse == StashResponseKind::Rotate);
        LUNATIC_CHECK(options.strategy.persistResponse == PersistResponseKind::Downstream);
        LUNATIC_CHECK(options.strategy.upstreamRetrySelection == RetrySelectionKind::Newest);
        LUNATIC_CHECK(options.strategy.downstreamRetrySelection == RetrySelectionKind::Newest);
        LUNATIC_CHECK(options.strategy.providerSelection == ProviderSelectionKind::LowestNodeId);
        LUNATIC_CHECK(options.strategy.forwardDct);
        LUNATIC_CHECK(options.strategy.copybackResponseAtResolve == (options.profile == ProfileKind::Type3));
        LUNATIC_CHECK(options.strategy.snoopIssueAtResolve == (options.profile == ProfileKind::Type3));
        LUNATIC_CHECK(options.strategy.snoopDataThroughDataPort == (options.profile == ProfileKind::Type3));
        OperationTiming legacyTiming = options.timing;
        legacyTiming.backendData = 0;
        CheckZeroTiming(legacyTiming);
        LUNATIC_CHECK(options.timing.backendData == 4);
    }

    void CheckType7Options(const Options& options)
    {
        ValidateOptions(options);
        LUNATIC_CHECK(options.profile == ProfileKind::Type7);
        LUNATIC_CHECK(options.inputQueueLimit == 16);
        LUNATIC_CHECK(options.outputQueueLimit == 16);
        LUNATIC_CHECK(options.transactionLimit == 16);
        LUNATIC_CHECK(options.snoopLimit == 32);
        LUNATIC_CHECK(options.slcSets == 1024);
        LUNATIC_CHECK(options.slcWays == 16);
        LUNATIC_CHECK(options.snoopFilterSets == 4096);
        LUNATIC_CHECK(options.snoopFilterWays == 16);
        LUNATIC_CHECK(options.dataWidth == 256);
        LUNATIC_CHECK(options.tagLatency == 2);
        LUNATIC_CHECK(options.dataLatency == 2);
        CheckLegacyAuxiliaryDefaults(options);
        LUNATIC_CHECK(options.policy == PolicyKind::Exclusive);
        LUNATIC_CHECK(options.backend == BackendKind::CHI);
        LUNATIC_CHECK(options.preciseSnoopFilter);
        LUNATIC_CHECK(!options.doNotGoToSD);
        LUNATIC_CHECK(options.retryEnabled);
        LUNATIC_CHECK(!options.separatedReadResponse);
        LUNATIC_CHECK(!options.separatedBackendRead);
        LUNATIC_CHECK(options.dmtEnabled);
        LUNATIC_CHECK(options.dctEnabled);
        LUNATIC_CHECK(!options.dwtEnabled);
        LUNATIC_CHECK(!options.directPersistEnabled);
        LUNATIC_CHECK(options.poisonEnabled);
        LUNATIC_CHECK(options.dataCheckEnabled);
        LUNATIC_CHECK(!options.mteEnabled);
        LUNATIC_CHECK(!options.dvmEnabled);

        LUNATIC_CHECK(options.structure.bankCount == 1);
        LUNATIC_CHECK(options.structure.requestBufferLimit == 0);
        LUNATIC_CHECK(options.structure.refillResourceLimit == 0);
        LUNATIC_CHECK(options.structure.responseResourceLimit == 0);
        LUNATIC_CHECK(options.structure.memoryResourceLimit == 0);
        LUNATIC_CHECK(options.structure.snoopResourceLimit == 0);
        LUNATIC_CHECK(options.structure.outgoingSnoopQueueLimit == 2);
        LUNATIC_CHECK(options.structure.reservedTransactionEntries == 1);
        LUNATIC_CHECK(options.structure.retrySourceLimit == 5);
        LUNATIC_CHECK(options.structure.tagPortOccupancy == 1);
        LUNATIC_CHECK(options.structure.dataPortOccupancy == 1);
        CheckClassOptions(options.structure.classes, true);

        LUNATIC_CHECK(options.strategy.homeReplacement == ReplacementKind::PseudoRandom);
        LUNATIC_CHECK(options.strategy.snoopFilterReplacement == ReplacementKind::PseudoRandom);
        LUNATIC_CHECK(options.strategy.requestArbitration == RequestArbitrationKind::OldestClassWeighted);
        LUNATIC_CHECK(options.strategy.outputArbitration == OutputArbitrationKind::OldestClassWeighted);
        LUNATIC_CHECK(options.strategy.allocationStrategy == AllocationStrategyKind::Policy);
        LUNATIC_CHECK(options.strategy.atomicExecution == AtomicExecutionKind::Local);
        LUNATIC_CHECK(options.strategy.immediateResponse == ImmediateResponseKind::EarlyDBID);
        LUNATIC_CHECK(options.strategy.stashResponse == StashResponseKind::Rotate);
        LUNATIC_CHECK(options.strategy.persistResponse == PersistResponseKind::Downstream);
        LUNATIC_CHECK(options.strategy.upstreamRetrySelection == RetrySelectionKind::Newest);
        LUNATIC_CHECK(options.strategy.downstreamRetrySelection == RetrySelectionKind::Newest);
        LUNATIC_CHECK(options.strategy.providerSelection == ProviderSelectionKind::LowestNodeId);
        LUNATIC_CHECK(options.strategy.forwardDct);
        LUNATIC_CHECK(options.strategy.copybackResponseAtResolve == (options.profile == ProfileKind::Type3));
        LUNATIC_CHECK(options.strategy.snoopIssueAtResolve == (options.profile == ProfileKind::Type3));
        LUNATIC_CHECK(options.strategy.snoopDataThroughDataPort == (options.profile == ProfileKind::Type3));
        CheckZeroTiming(options.timing);
    }

    template<typename Change>
    void RejectChange(Change change)
    {
        Options options;
        change(options);
        CheckThrows([&] { ValidateOptions(options); });
    }

    template<typename Change>
    void RejectType7Change(Change change)
    {
        Options options = MakeType7Options();
        change(options);
        CheckThrows([&] { ValidateOptions(options); });
    }

}

LUNATIC_TEST(Stage10ProfilesLegacyOptionsUnchanged)
{
    CheckCustomOptions(Options{});
}

LUNATIC_TEST(Stage10ProfilesType3Factory)
{
    CheckType3Options(MakeType3Options());
}

LUNATIC_TEST(Stage10ProfilesType7Factory)
{
    CheckType7Options(MakeType7Options());
}

LUNATIC_TEST(Stage10ProfilesRejectInvalidEnums)
{
    RejectChange([](Options& options) { options.profile = static_cast<ProfileKind>(255); });
    RejectChange([](Options& options) {
        options.strategy.homeReplacement = static_cast<ReplacementKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.snoopFilterReplacement = static_cast<ReplacementKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.requestArbitration = static_cast<RequestArbitrationKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.outputArbitration = static_cast<OutputArbitrationKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.allocationStrategy = static_cast<AllocationStrategyKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.atomicExecution = static_cast<AtomicExecutionKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.immediateResponse = static_cast<ImmediateResponseKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.stashResponse = static_cast<StashResponseKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.persistResponse = static_cast<PersistResponseKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.upstreamRetrySelection = static_cast<RetrySelectionKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.downstreamRetrySelection = static_cast<RetrySelectionKind>(255);
    });
    RejectChange([](Options& options) {
        options.strategy.providerSelection = static_cast<ProviderSelectionKind>(255);
    });
}

LUNATIC_TEST(Stage10ProfilesStructureValidation)
{
    Options options;
    options.structure.bankCount = 2;
    options.structure.reservedTransactionEntries = 1;
    options.structure.tagPortOccupancy = 3;
    options.structure.dataPortOccupancy = 2;
    ValidateOptions(options);

    RejectChange([](Options& value) { value.structure.bankCount = 0; });
    RejectChange([](Options& value) { value.structure.bankCount = 3; });
    RejectChange([](Options& value) { value.structure.bankCount = 32; });
    RejectChange([](Options& value) {
        value.transactionLimit = 15;
        value.structure.bankCount = 4;
    });
    RejectChange([](Options& value) { value.structure.tagPortOccupancy = 0; });
    RejectChange([](Options& value) { value.structure.dataPortOccupancy = 0; });
    RejectChange([](Options& value) { value.structure.reservedTransactionEntries = 16; });
    RejectChange([](Options& value) { value.structure.reservedTransactionEntries = 17; });
}

LUNATIC_TEST(Stage10ProfilesNeutralZeroResources)
{
    Options options;
    options.structure.requestBufferLimit = 0;
    options.structure.refillResourceLimit = 0;
    options.structure.responseResourceLimit = 0;
    options.structure.memoryResourceLimit = 0;
    options.structure.snoopResourceLimit = 0;
    options.structure.outgoingSnoopQueueLimit = 0;
    options.structure.reservedTransactionEntries = 0;
    options.structure.retrySourceLimit = 0;
    ValidateOptions(options);
}

LUNATIC_TEST(Stage10ProfilesClassAndArbitrationValidation)
{
    Options options = MakeType7Options();
    options.structure.classes.contendedMinimum[3] = 15;
    options.structure.classes.requestWeights.fill(0);
    options.structure.classes.retryWeights.fill(0);
    ValidateOptions(options);

    options.structure.classes.enabled = false;
    options.strategy.requestArbitration = RequestArbitrationKind::Age;
    options.strategy.outputArbitration = OutputArbitrationKind::Age;
    ValidateOptions(options);

    RejectType7Change([](Options& value) { value.structure.classes.qosMinimum[0] = 16; });
    RejectType7Change([](Options& value) { value.structure.classes.qosMinimum[0] = 14; });
    RejectType7Change([](Options& value) { value.structure.classes.maximum[0] = 0; });
    RejectType7Change([](Options& value) { value.structure.classes.maximum[0] = 17; });
    RejectType7Change([](Options& value) { value.structure.classes.dedicated[2] = 9; });
    RejectType7Change([](Options& value) { value.structure.classes.enabled = false; });
    RejectChange([](Options& value) {
        value.strategy.requestArbitration = RequestArbitrationKind::OldestClassWeighted;
    });
    RejectChange([](Options& value) {
        value.strategy.outputArbitration = OutputArbitrationKind::OldestClassWeighted;
    });
}

LUNATIC_TEST(Stage10ProfilesLegacyValidationRemains)
{
    Options options;
    options.transactionLimit = 2;
    ValidateOptions(options);
    options.transactionLimit = 128;
    ValidateOptions(options);
    options.transactionLimit = 256;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.mteEnabled = true;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.dvmEnabled = true;
    CheckThrows([&] { ValidateOptions(options); });
}

LUNATIC_TEST(Stage10ProfilesTypedConstructionAndReset)
{
    HomeNode<ProfileConfig256> type3(2, MakeType3Options());
    CheckType3Options(type3.GetOptions());
    type3.Reset();
    CheckType3Options(type3.GetOptions());
    LUNATIC_CHECK(type3.Drain());

    HomeNode<ProfileConfig256> type7(2, MakeType7Options());
    CheckType7Options(type7.GetOptions());
    type7.Reset();
    CheckType7Options(type7.GetOptions());
    LUNATIC_CHECK(type7.Drain());
}

LUNATIC_TEST(Stage10ProfilesTypedCapabilityMismatch)
{
    CheckThrows([] { HomeNode<Config256> node(2, MakeType3Options()); });

    Options poison = MakeType7Options();
    poison.dataCheckEnabled = false;
    CheckThrows([&] { HomeNode<Config256> node(2, poison); });

    Options dataCheck = MakeType7Options();
    dataCheck.poisonEnabled = false;
    CheckThrows([&] { HomeNode<Config256> node(2, dataCheck); });

    Options width = MakeType7Options();
    width.dataWidth = 512;
    CheckThrows([&] { HomeNode<ProfileConfig256> node(2, width); });
}
