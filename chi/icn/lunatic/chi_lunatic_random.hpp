#pragma once

#include <cstdint>
#include <limits>

namespace CHI::Lunatic {

    class DeterministicRandom {
        uint64_t state;

        static uint64_t Mix(uint64_t value) noexcept
        {
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

    public:
        explicit DeterministicRandom(uint64_t seed = 0) noexcept : state(seed) {}

        void Reset(uint64_t seed) noexcept { state = seed; }

        uint64_t Next() noexcept
        {
            state += 0x9E3779B97F4A7C15ull;
            return Mix(state);
        }

        uint64_t Bounded(uint64_t limit) noexcept
        {
            if (!limit)
                return 0;
            if (limit == std::numeric_limits<uint64_t>::max())
                return Next();
            return Next() % (limit + 1);
        }
    };

}