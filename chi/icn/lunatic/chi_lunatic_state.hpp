#pragma once

#include "chi_lunatic_types.hpp"

namespace CHI::Lunatic {

    enum class CacheState : uint8_t {
        I,
        SC,
        UC,
        SD,
        UD,
        UDP
    };

    bool IsValid(CacheState state) noexcept;
    bool IsShared(CacheState state) noexcept;
    bool IsUnique(CacheState state) noexcept;
    bool IsDirty(CacheState state) noexcept;
    bool IsPartialDirty(CacheState state) noexcept;

}
