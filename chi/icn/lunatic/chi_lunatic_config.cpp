#include "chi_lunatic_config.hpp"
#include "chi_lunatic_types.hpp"

#include <limits>
#include <stdexcept>

namespace CHI::Lunatic {

    void                                ValidateOptions(const Options& options)
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

        if (options.backend == BackendKind::AXI)
            throw std::invalid_argument("Lunatic AXI backend is not enabled in this profile");

        if (options.mteEnabled || options.dvmEnabled)
            throw std::invalid_argument("Lunatic MTE and DVM are not enabled in this profile");
    }

    size_t                              BytesPerBeat(const Options& options) noexcept
    {
        return options.dataWidth / 8;
    }

    size_t                              BeatsPerLine(const Options& options) noexcept
    {
        return LINE_SIZE / BytesPerBeat(options);
    }

}
