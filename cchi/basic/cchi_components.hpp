#pragma once

#ifndef __CCHI__CCHI_COMPONENTS
#define __CCHI__CCHI_COMPONENTS

#include <cstddef>


namespace CCHI {

    class ComponentTypeEnumBack {
    public:
        const char*     name;
        const int       value;
        const char*     description;
        const size_t    opcodeWidthEVT;
        const size_t    opcodeWidthREQ;
        const size_t    opcodeWidthSNP;
        const size_t    opcodeWidthDnRSP;
        const size_t    opcodeWidthUpRSP;
        const size_t    opcodeWidthDnDAT;
        const size_t    opcodeWidthUpDAT;
        const bool      hasEVT;
        const bool      hasREQ;
        const bool      hasSNP;
        const bool      hasDnRSP;
        const bool      hasUpRSP;
        const bool      hasDnDAT;
        const bool      hasUpDAT;
        
    public:
        inline constexpr ComponentTypeEnumBack(
            const char*     name,
            const int       value,
            const char*     description,
            const size_t    opcodeWidthEVT,
            const size_t    opcodeWidthREQ,
            const size_t    opcodeWidthSNP,
            const size_t    opcodeWidthDnRSP,
            const size_t    opcodeWidthUpRSP,
            const size_t    opcodeWidthDnDAT,
            const size_t    opcodeWidthUpDAT,
            const bool      hasEVT,
            const bool      hasREQ,
            const bool      hasSNP,
            const bool      hasDnRSP,
            const bool      hasUpRSP,
            const bool      hasDnDAT,
            const bool      hasUpDAT)
        : name              (name)
        , value             (value)
        , description       (description)
        , opcodeWidthEVT    (opcodeWidthEVT)
        , opcodeWidthREQ    (opcodeWidthREQ)
        , opcodeWidthSNP    (opcodeWidthSNP)
        , opcodeWidthDnRSP  (opcodeWidthDnRSP)
        , opcodeWidthUpRSP  (opcodeWidthUpRSP)
        , opcodeWidthDnDAT  (opcodeWidthDnDAT)
        , opcodeWidthUpDAT  (opcodeWidthUpDAT)
        , hasEVT            (hasEVT)
        , hasREQ            (hasREQ)
        , hasSNP            (hasSNP)
        , hasDnRSP          (hasDnRSP)
        , hasUpRSP          (hasUpRSP)
        , hasDnDAT          (hasDnDAT)
        , hasUpDAT          (hasUpDAT)
        { }

    public:
        inline constexpr operator int() const noexcept
        { return value; }

        inline constexpr operator const ComponentTypeEnumBack*() const noexcept
        { return this; }

        inline constexpr bool operator==(const ComponentTypeEnumBack& obj) const noexcept
        { return value == obj.value; }

        inline constexpr bool operator!=(const ComponentTypeEnumBack& obj) const noexcept
        { return !(*this == obj); }
    };

    using ComponentTypeEnum = const ComponentTypeEnumBack*;

    namespace ComponentType {
        inline constexpr ComponentTypeEnumBack TYPE_1   ("Type 1", 1, "Fully coherent"          , 1, 6, 2, 3, 1, 0, 2, true , true , true , true , true , true , true );
        inline constexpr ComponentTypeEnumBack TYPE_2   ("Type 2", 2, "Read-only coherent"      , 0, 3, 0, 0, 1, 0, 0, true , true , true , false, true , true , false);
        inline constexpr ComponentTypeEnumBack TYPE_3   ("Type 3", 3, "Non-coherent"            , 0, 4, 0, 3, 0, 0, 1, false, true , false, true , true , true , true );
        inline constexpr ComponentTypeEnumBack TYPE_4   ("Type 4", 4, "Read-only non-coherent"  , 0, 2, 0, 0, 0, 0, 0, false, true , false, false, false, true , false);
        inline constexpr ComponentTypeEnumBack TYPE_5   ("Type 5", 5, "Lower-level Prefetcher"  , 0, 1, 0, 0, 0, 0, 0, false, true , false, true , false, false, false);
    }


    template<ComponentTypeEnum ComponentType>
    concept HasEVT = ComponentType->hasEVT;

    template<ComponentTypeEnum ComponentType>
    concept HasREQ = ComponentType->hasREQ;

    template<ComponentTypeEnum ComponentType>
    concept HasSNP = ComponentType->hasSNP;

    template<ComponentTypeEnum ComponentType>
    concept HasDnRSP = ComponentType->hasDnRSP;

    template<ComponentTypeEnum ComponentType>
    concept HasUpRSP = ComponentType->hasUpRSP;

    template<ComponentTypeEnum ComponentType>
    concept HasDnDAT = ComponentType->hasDnDAT;

    template<ComponentTypeEnum ComponentType>
    concept HasUpDAT = ComponentType->hasUpDAT;
}


#endif // __CCHI__CCHI_COMPONENTS
