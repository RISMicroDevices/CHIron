#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_transaction.hpp"

namespace CHI::Lunatic {

    class VirtualPipeline {
        Options options;
        Time    tagPortFree = 0;
        Time    dataPortFree = 0;

    public:
        explicit VirtualPipeline(Options options) : options(std::move(options)) {}

        bool Eligible(const Transaction& transaction, Time time) const noexcept;
        Time StageLatency(Phase phase) const noexcept;
        Time Advance(Transaction& transaction, Phase next, Time time) const noexcept;
        Time ScheduleTag(Time earliest) noexcept;
        Time ScheduleData(Time earliest) noexcept;
        Time SaturatingAdd(Time time, Time delay) const noexcept;
        void Reset() noexcept;
    };

}
