#include "chi_lunatic_config.hpp"
#include "chi_lunatic_types.hpp"

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace CHI::Lunatic {
    namespace {

        template<typename Enum>
        void ValidateEnum(Enum value, Enum maximum)
        {
            using Underlying = std::underlying_type_t<Enum>;
            if (static_cast<Underlying>(value) > static_cast<Underlying>(maximum))
                throw std::invalid_argument("Lunatic option contains an invalid enumeration value");
        }

    }

    void ValidateOptions(const Options& options)
    {
        if (!options.inputQueueLimit || !options.outputQueueLimit)
            throw std::invalid_argument("Lunatic channel queues must be non-empty");

        if (!options.transactionLimit || !options.snoopLimit)
            throw std::invalid_argument("Lunatic tracker resources must be non-empty");

        if (options.transactionLimit > 4096 || options.snoopLimit > 4096 ||
            options.transactionLimit > 4096 / options.snoopLimit)
            throw std::invalid_argument("Lunatic transaction or snoop IDs exceed the Issue E.b width");

        if (!options.slcSets || !options.slcWays || !options.snoopFilterSets || !options.snoopFilterWays)
            throw std::invalid_argument("Lunatic array geometry must be non-zero");

        if (options.slcSets > std::numeric_limits<uint32_t>::max() / options.slcWays ||
            options.snoopFilterSets > std::numeric_limits<uint32_t>::max() / options.snoopFilterWays)
            throw std::invalid_argument("Lunatic array geometry is too large");

        if (options.dataWidth != 128 && options.dataWidth != 256 && options.dataWidth != 512)
            throw std::invalid_argument("Lunatic data width must be 128, 256, or 512 bits");

        if (!options.tagLatency || !options.dataLatency)
            throw std::invalid_argument("Lunatic tag and data latencies must be non-zero");

        if (!options.axiOutstandingLimit || options.axiOutstandingLimit > 256 ||
            !options.axiIdWidth || options.axiIdWidth > 16 ||
            options.axiOutstandingLimit > (size_t(1) << options.axiIdWidth))
            throw std::invalid_argument("Lunatic AXI outstanding capacity exceeds its ID space");

        if (options.backend == BackendKind::AXI &&
            (options.separatedBackendRead || options.dmtEnabled || options.dwtEnabled ||
             options.directPersistEnabled || options.poisonEnabled || options.dataCheckEnabled))
            throw std::invalid_argument("Lunatic AXI does not support CHI-only backend features or metadata");

        if (options.mteEnabled || options.dvmEnabled)
            throw std::invalid_argument("Lunatic MTE and DVM are prohibited");

        ValidateEnum(options.profile, ProfileKind::Type7);
        ValidateEnum(options.strategy.homeReplacement, ReplacementKind::PseudoRandom);
        ValidateEnum(options.strategy.snoopFilterReplacement, ReplacementKind::PseudoRandom);
        ValidateEnum(options.strategy.requestArbitration, RequestArbitrationKind::OldestClassWeighted);
        ValidateEnum(options.strategy.outputArbitration, OutputArbitrationKind::OldestClassWeighted);
        ValidateEnum(options.strategy.allocationStrategy, AllocationStrategyKind::SelectiveSharedCopyback);
        ValidateEnum(options.strategy.atomicExecution, AtomicExecutionKind::Local);
        ValidateEnum(options.strategy.immediateResponse, ImmediateResponseKind::Combined);
        ValidateEnum(options.strategy.stashResponse, StashResponseKind::Combined);
        ValidateEnum(options.strategy.persistResponse, PersistResponseKind::SeparatePersistFirst);
        ValidateEnum(options.strategy.upstreamRetrySelection, RetrySelectionKind::Newest);
        ValidateEnum(options.strategy.downstreamRetrySelection, RetrySelectionKind::Newest);
        ValidateEnum(options.strategy.providerSelection, ProviderSelectionKind::RoundRobin);

        const size_t bankCount = options.structure.bankCount;
        if (!bankCount || (bankCount & (bankCount - 1)) != 0 ||
            bankCount > options.transactionLimit || options.transactionLimit % bankCount != 0)
            throw std::invalid_argument("Lunatic banking must be a power of two dividing the transaction capacity");

        if (!options.structure.tagPortOccupancy || !options.structure.dataPortOccupancy)
            throw std::invalid_argument("Lunatic port occupancy must be non-zero");

        if (options.structure.reservedTransactionEntries >= options.transactionLimit)
            throw std::invalid_argument("Lunatic reserved transaction entries must leave unreserved capacity");

        const bool classRequestArbitration =
            options.strategy.requestArbitration == RequestArbitrationKind::OldestClassWeighted;
        const bool classOutputArbitration =
            options.strategy.outputArbitration == OutputArbitrationKind::OldestClassWeighted;
        if ((classRequestArbitration || classOutputArbitration) && !options.structure.classes.enabled)
            throw std::invalid_argument("Lunatic class-weighted arbitration requires classes");

        const auto& classes = options.structure.classes;
        if (classes.enabled)
        {
            for (size_t index = 0; index < classes.maximum.size(); ++index)
            {
                if (classes.qosMinimum[index] > classes.qosMaximum[index])
                    throw std::invalid_argument("Lunatic class QoS minimum exceeds its maximum");
                if (!classes.maximum[index] || classes.maximum[index] > options.transactionLimit)
                    throw std::invalid_argument("Lunatic class maximum exceeds the transaction capacity");
                if (classes.dedicated[index] > classes.maximum[index])
                    throw std::invalid_argument("Lunatic class dedicated capacity exceeds its maximum");

                for (size_t other = index + 1; other < classes.maximum.size(); ++other)
                {
                    if (classes.qosMinimum[index] <= classes.qosMaximum[other] &&
                        classes.qosMinimum[other] <= classes.qosMaximum[index])
                        throw std::invalid_argument("Lunatic class QoS ranges must not overlap");
                }
            }
        }
    }

    Options MakeType3Options()
    {
        Options options;
        options.profile = ProfileKind::Type3;
        options.inputQueueLimit = 15;
        options.transactionLimit = 64;
        options.slcSets = 128;
        options.slcWays = 4;
        options.snoopFilterSets = 128;
        options.snoopFilterWays = 4;
        options.backend = BackendKind::CHI;
        options.doNotGoToSD = true;
        options.retryEnabled = false;
        options.poisonEnabled = true;
        options.dataCheckEnabled = true;
        options.tagLatency = 1;
        options.dataLatency = 3;
        options.timing.backendData = 4;

        options.structure.bankCount = 4;
        options.structure.requestBufferLimit = 8;
        options.structure.refillResourceLimit = 16;
        options.structure.responseResourceLimit = 16;
        options.structure.memoryResourceLimit = 16;
        options.structure.snoopResourceLimit = 16;
        options.structure.outgoingSnoopQueueLimit = 1;

        options.strategy.homeReplacement = ReplacementKind::PLRU;
        options.strategy.snoopFilterReplacement = ReplacementKind::PseudoRandom;
        options.strategy.requestArbitration = RequestArbitrationKind::RefillFirst;
        options.strategy.allocationStrategy = AllocationStrategyKind::SelectiveSharedCopyback;
        options.strategy.copybackResponseAtResolve = true;

        ValidateOptions(options);
        return options;
    }

    Options MakeType7Options()
    {
        Options options;
        options.profile = ProfileKind::Type7;
        options.backend = BackendKind::CHI;
        options.dmtEnabled = true;
        options.dctEnabled = true;
        options.poisonEnabled = true;
        options.dataCheckEnabled = true;

        options.structure.outgoingSnoopQueueLimit = 2;
        options.structure.reservedTransactionEntries = 1;
        options.structure.retrySourceLimit = 5;
        options.structure.classes.enabled = true;

        options.strategy.homeReplacement = ReplacementKind::PseudoRandom;
        options.strategy.snoopFilterReplacement = ReplacementKind::PseudoRandom;
        options.strategy.requestArbitration = RequestArbitrationKind::OldestClassWeighted;
        options.strategy.outputArbitration = OutputArbitrationKind::OldestClassWeighted;
        options.strategy.atomicExecution = AtomicExecutionKind::Local;
        options.strategy.immediateResponse = ImmediateResponseKind::EarlyDBID;

        ValidateOptions(options);
        return options;
    }

    size_t BytesPerBeat(const Options& options) noexcept
    {
        return options.dataWidth / 8;
    }

    size_t BeatsPerLine(const Options& options) noexcept
    {
        return LINE_SIZE / BytesPerBeat(options);
    }

}
