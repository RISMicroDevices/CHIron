#pragma once

#include <algorithm>
#include <cstddef>
#include <thread>

namespace CHIron::Gui::Detail {

inline std::size_t suggestedFilterWorkerCount(const std::size_t rowCount)
{
    constexpr unsigned int kMaxFilterWorkers = 8;
    const unsigned int hardwareWorkers = std::min(
        kMaxFilterWorkers,
        std::max(1U, std::thread::hardware_concurrency()));
    if (rowCount < 16384 || hardwareWorkers <= 1U) {
        return 1;
    }

    const std::size_t chunkLimitedWorkers = std::max<std::size_t>(1, rowCount / 8192);
    return std::min<std::size_t>(hardwareWorkers, chunkLimitedWorkers);
}

}  // namespace CHIron::Gui::Detail
