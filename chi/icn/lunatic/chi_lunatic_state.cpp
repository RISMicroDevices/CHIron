#include "chi_lunatic_state.hpp"

namespace CHI::Lunatic {

    bool                                IsValid(CacheState state) noexcept { return state != CacheState::I; }
    bool                                IsShared(CacheState state) noexcept { return state == CacheState::SC || state == CacheState::SD; }
    bool                                IsUnique(CacheState state) noexcept { return state == CacheState::UC || state == CacheState::UD || state == CacheState::UDP; }
    bool                                IsDirty(CacheState state) noexcept { return state == CacheState::SD || state == CacheState::UD || state == CacheState::UDP; }
    bool                                IsPartialDirty(CacheState state) noexcept { return state == CacheState::UDP; }

}
