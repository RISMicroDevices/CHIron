#pragma once

#include "chi_lunatic_protocol.hpp"

namespace CHI::Lunatic {

    enum class AtomicPath : uint8_t {
        Forward,
        Local,
        Error
    };

    enum class AtomicOperation : uint8_t {
        Add = 0,
        Clr = 1,
        Eor = 2,
        Set = 3,
        SMax = 4,
        SMin = 5,
        UMax = 6,
        UMin = 7,
        Swap = 8,
        Compare = 9
    };

    struct AtomicGeometry {
        size_t windowBytes = 0;
        size_t operandBytes = 0;
        size_t windowOffset = 0;
    };

    struct AtomicExecution {
        LineData data{};
        bool     write = false;
    };

    bool            GetAtomicGeometry(const Request& request, AtomicGeometry& geometry) noexcept;
    AtomicOperation GetAtomicOperation(uint16_t opcode) noexcept;
    AtomicPath      ClassifyAtomicPath(const Request& request, const Options& options) noexcept;
    uint64_t        AtomicOperandBeatMask(const Request& request, const Options& options) noexcept;
    uint64_t        AtomicResultBeatMask(const Request& request, const Options& options) noexcept;
    AtomicExecution ExecuteAtomic(const Request& request, const LineData& initial,
                                  const LineData& operands) noexcept;

}
