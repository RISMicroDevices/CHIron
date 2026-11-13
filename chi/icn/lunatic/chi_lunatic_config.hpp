#pragma once

#include "chi_lunatic_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CHI::Lunatic {

    enum class PolicyKind : uint8_t {
        NINE,
        Exclusive,
        Inclusive
    };

    enum class BackendKind : uint8_t {
        Internal,
        CHI,
        AXI
    };

    enum class ProfileKind : uint8_t {
        Custom,
        Type3,
        Type7
    };

    enum class ReplacementKind : uint8_t {
        Age,
        PLRU,
        PseudoRandom
    };

    enum class RequestArbitrationKind : uint8_t {
        Age,
        RefillFirst,
        OldestClassWeighted
    };

    enum class OutputArbitrationKind : uint8_t {
        Age,
        OldestClassWeighted
    };

    enum class AllocationStrategyKind : uint8_t {
        Policy,
        SelectiveSharedCopyback
    };

    enum class AtomicExecutionKind : uint8_t {
        Backend,
        Local
    };

    enum class ImmediateResponseKind : uint8_t {
        Rotate,
        EarlyDBID,
        LateComp,
        Combined
    };

    enum class StashResponseKind : uint8_t {
        Rotate,
        CompFirst,
        StashDoneFirst,
        Combined
    };

    enum class PersistResponseKind : uint8_t {
        Downstream,
        Combined,
        SeparateCompFirst,
        SeparatePersistFirst
    };

    enum class RetrySelectionKind : uint8_t {
        Oldest,
        Newest
    };

    enum class ProviderSelectionKind : uint8_t {
        LowestNodeId,
        RoundRobin
    };

    struct ClassOptions {
        bool                    enabled = false;
        std::array<uint8_t, 4>  qosMinimum = { 15, 12, 8, 0 };
        std::array<uint8_t, 4>  qosMaximum = { 15, 14, 11, 7 };
        std::array<size_t, 4>   dedicated = { 0, 0, 0, 0 };
        std::array<size_t, 4>   maximum = { 15, 14, 8, 2 };
        std::array<size_t, 4>   contendedMinimum = { 4, 4, 4, 4 };
        std::array<uint64_t, 4> requestWeights = { 0, 0, 0, 0 };
        std::array<uint64_t, 4> retryWeights = { 0, 0, 0, 0 };
        uint64_t                fastResponseWeight = 63;
        uint64_t                retryResponseWeight = 1;
        uint64_t                staticGrantResponseWeight = 1;
        uint64_t                normalResponseWeight = 1;
    };

    struct StructureOptions {
        size_t       bankCount = 1;
        // Zero buffer, resource, queue, and retry-source limits preserve legacy behavior without independent limits.
        size_t       requestBufferLimit = 0;
        size_t       refillResourceLimit = 0;
        size_t       responseResourceLimit = 0;
        size_t       memoryResourceLimit = 0;
        size_t       snoopResourceLimit = 0;
        size_t       outgoingSnoopQueueLimit = 0;
        size_t       reservedTransactionEntries = 0;
        size_t       retrySourceLimit = 0;
        size_t       tagPortOccupancy = 1;
        size_t       dataPortOccupancy = 1;
        ClassOptions classes{};
    };

    struct StrategyOptions {
        ReplacementKind        homeReplacement = ReplacementKind::Age;
        ReplacementKind        snoopFilterReplacement = ReplacementKind::Age;
        RequestArbitrationKind requestArbitration = RequestArbitrationKind::Age;
        OutputArbitrationKind  outputArbitration = OutputArbitrationKind::Age;
        AllocationStrategyKind allocationStrategy = AllocationStrategyKind::Policy;
        AtomicExecutionKind    atomicExecution = AtomicExecutionKind::Backend;
        ImmediateResponseKind  immediateResponse = ImmediateResponseKind::Rotate;
        StashResponseKind      stashResponse = StashResponseKind::Rotate;
        PersistResponseKind    persistResponse = PersistResponseKind::Downstream;
        RetrySelectionKind     upstreamRetrySelection = RetrySelectionKind::Newest;
        RetrySelectionKind     downstreamRetrySelection = RetrySelectionKind::Newest;
        ProviderSelectionKind  providerSelection = ProviderSelectionKind::LowestNodeId;
        bool                   forwardDct = true;
        bool                   copybackResponseAtResolve = false;
        bool                   snoopIssueAtResolve = false;
        bool                   snoopDataThroughDataPort = false;
    };

    enum class SchedulePoint : uint8_t {
        Automatic,
        InputRegister,
        RequestBuffer,
        Allocation,
        LookupResult,
        SnoopDecision,
        SnoopIssue,
        DataReturn,
        FastResponse,
        NormalResponse,
        BackendRequest,
        BackendData,
        RetryAck,
        CreditGrant,
        BackendResend,
        AtomicExecution,
        Persistence,
        DataBeat,
        Output,
        CommitSchedule
    };

    struct OperationTiming {
        uint64_t    inputRegister = 0;
        uint64_t    requestBuffer = 0;
        uint64_t    allocation = 0;
        uint64_t    lookupResult = 0;
        uint64_t    snoopDecision = 0;
        uint64_t    snoopIssue = 0;
        uint64_t    dataReturn = 0;
        uint64_t    fastResponse = 0;
        uint64_t    normalResponse = 0;
        uint64_t    backendRequest = 0;
        uint64_t    backendData = 0;
        uint64_t    retryAck = 0;
        uint64_t    creditGrant = 0;
        uint64_t    backendResend = 0;
        uint64_t    atomicExecution = 0;
        uint64_t    persistence = 0;
        uint64_t    dataBeat = 0;
        uint64_t    output = 0;
        uint64_t    commitSchedule = 0;
    };

    struct Options {
        size_t      inputQueueLimit = 16;
        size_t      outputQueueLimit = 16;
        size_t      transactionLimit = 16;
        size_t      snoopLimit = 32;
        size_t      slcSets = 1024;
        size_t      slcWays = 16;
        size_t      snoopFilterSets = 4096;
        size_t      snoopFilterWays = 16;
        size_t      dataWidth = 256;
        uint64_t    tagLatency = 2;
        uint64_t    dataLatency = 2;
        uint64_t    homeHitDataLatency = 0;
        uint64_t    snoopDataLatency = 0;
        uint64_t    memoryLatency = 8;
        uint64_t    receiveDelay = 0;
        uint64_t    decodeDelay = 0;
        uint64_t    admissionDelay = 0;
        uint64_t    hazardDelay = 0;
        uint64_t    lookupDelay = 0;
        uint64_t    resolveDelay = 0;
        uint64_t    actionDelay = 0;
        uint64_t    returnDelay = 0;
        uint64_t    commitDelay = 0;
        uint64_t    randomDelayLimit = 0;
        uint64_t    seed = 0;
        NodeId      subordinateId = 0;
        PolicyKind  policy = PolicyKind::Exclusive;
        BackendKind backend = BackendKind::Internal;
        size_t      axiOutstandingLimit = 1;
        uint8_t     axiIdWidth = 4;
        bool        randomizeArbitration = false;
        bool        preciseSnoopFilter = true;
        bool        doNotGoToSD = false;
        bool        retryEnabled = true;
        bool        separatedReadResponse = false;
        bool        separatedBackendRead = false;
        bool        dmtEnabled = false;
        bool        dctEnabled = false;
        bool        dwtEnabled = false;
        bool        directPersistEnabled = false;
        bool        poisonEnabled = false;
        bool        dataCheckEnabled = false;
        bool        mteEnabled = false;
        bool        dvmEnabled = false;
        ProfileKind profile = ProfileKind::Custom;
        StructureOptions structure{};
        StrategyOptions strategy{};
        OperationTiming timing{};
    };

    Options MakeType3Options();
    Options MakeType7Options();

    void   ValidateOptions(const Options& options);
    size_t BytesPerBeat(const Options& options) noexcept;
    size_t BeatsPerLine(const Options& options) noexcept;

}
