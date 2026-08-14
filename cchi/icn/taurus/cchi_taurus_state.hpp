#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__STATE
#define __CCHI__CCHI_ICN_TAURUS__STATE


namespace CCHI::Taurus {
    
    class CacheStateEnumBack {
    public:
        const char* name;
        const int   ordinal;

    public:
        inline constexpr CacheStateEnumBack(const char* name, int ordinal) noexcept
            : name(name), ordinal(ordinal) 
        { }

    public:
        inline constexpr operator int() const noexcept 
        { return ordinal; }

        inline constexpr operator const CacheStateEnumBack*() const noexcept 
        { return this; }

        inline constexpr bool operator==(const CacheStateEnumBack& other) const noexcept
        { return this->ordinal == other.ordinal; }

        inline constexpr bool operator!=(const CacheStateEnumBack& other) const noexcept
        { return this->ordinal != other.ordinal; }
    };

    using CacheStateEnum = const CacheStateEnumBack*;

    namespace CacheState {
        inline constexpr CacheStateEnumBack Invalid     ("Invalid"      , 0);
        inline constexpr CacheStateEnumBack Shared      ("Shared"       , 1);
        inline constexpr CacheStateEnumBack UniqueClean ("UniqueClean"  , 2);
        inline constexpr CacheStateEnumBack UniqueDirty ("UniqueDirty"  , 3);
    }
}


#endif // __CCHI__CCHI_ICN_TAURUS__STATE
