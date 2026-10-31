#pragma once

#include "chi_lunatic_types.hpp"

#include <array>

namespace CHI::Lunatic {

    inline constexpr size_t WAIT_REASON_COUNT = size_t(WaitReason::Decision) + 1;

    struct PerformanceCounters {
        uint64_t acceptedRXREQ = 0;
        uint64_t acceptedRXRSP = 0;
        uint64_t acceptedRXDAT = 0;
        uint64_t emittedTXREQ = 0;
        uint64_t emittedTXRSP = 0;
        uint64_t emittedTXDAT = 0;
        uint64_t emittedTXSNP = 0;
        uint64_t admittedTransactions = 0;
        uint64_t retiredTransactions = 0;
        uint64_t emittedRetryAcks = 0;
        uint64_t emittedCreditGrants = 0;
        uint64_t creditedResends = 0;
        uint64_t returnedCredits = 0;
        uint64_t completedSnoopTargets = 0;
        uint64_t acceptedRXB = 0;
        uint64_t acceptedRXR = 0;
        uint64_t emittedTXAW = 0;
        uint64_t emittedTXW = 0;
        uint64_t emittedTXAR = 0;

    private:
        std::array<uint64_t, WAIT_REASON_COUNT> stalls{};

    public:
        void Reset() noexcept { *this = PerformanceCounters{}; }

        void RecordStall(WaitReason reason) noexcept
        {
            const size_t index = size_t(reason);
            if (index < stalls.size())
                ++stalls[index];
        }

        uint64_t GetStalls(WaitReason reason) const noexcept
        {
            const size_t index = size_t(reason);
            return index < stalls.size() ? stalls[index] : 0;
        }

        bool operator==(const PerformanceCounters&) const = default;
    };

}