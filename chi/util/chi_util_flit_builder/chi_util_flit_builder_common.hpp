//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_B__COMMON)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_EB__COMMON))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_B__COMMON
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_EB__COMMON
#endif


/*
namespace CHI {
*/
    namespace Flits {

        // concepts
        template<Xact::FieldConvention conv>
        concept is_field_applicable = Xact::FieldTrait::IsApplicable(conv);

        template<class T>
        concept has_mpam = T::hasMPAM;

        template<class T>
        concept has_rsvdc = T::hasRSVDC;

        template<class T>
        concept has_datacheck = T::hasDataCheck;

        template<class T>
        concept has_poison = T::hasPoison;

        
        // constevals
        template<Xact::FieldConvention conv>
        consteval uint64_t GetDefaultFieldValue() noexcept;

        template<Xact::FieldConvention conv>
        consteval bool HasDefaultFieldValue() noexcept;

        template<Xact::FieldConvention conv>
        consteval bool IsFieldValueStableOrInapplicable() noexcept;
    }
/*
}
*/

// Implementation of constevals
namespace /*CHI::*/Flits {

    template<Xact::FieldConvention conv>
    inline consteval uint64_t GetDefaultFieldValue() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::A0)
            return 0;
        else if constexpr (conv == Xact::FieldConvention::A1)
            return 1;
        else if constexpr (conv == Xact::FieldConvention::I0)
            return 0;
        else if constexpr (conv == Xact::FieldConvention::X)
            return 0; // can take any value here, leave it 0 as default
        else if constexpr (conv == Xact::FieldConvention::Y)
            return 0; // applicable, leave to 0 as default
        else if constexpr (conv == Xact::FieldConvention::B8)
            return Sizes::B8;
        else if constexpr (conv == Xact::FieldConvention::B64)
            return Sizes::B64;
        else if constexpr (conv == Xact::FieldConvention::S)
            return 0; // do not set default value
        else if constexpr (conv == Xact::FieldConvention::D)
            return 0; // associated with NS field, leave it 0 as default
        else
            return 0;
    }

    template<Xact::FieldConvention conv>
    inline consteval bool HasDefaultFieldValue() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::X)
            return false;
        else if constexpr (conv == Xact::FieldConvention::Y)
            return false;
        else if constexpr (conv == Xact::FieldConvention::S)
            return false;
        else
            return true;
    }

    template<Xact::FieldConvention conv>
    inline consteval bool IsFieldValueStableOrInapplicable() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::A0)
            return true;
        else if constexpr (conv == Xact::FieldConvention::A1)
            return true;
        else if constexpr (conv == Xact::FieldConvention::I0)
            return true;
        else if constexpr (conv == Xact::FieldConvention::X)
            return true;
        else if constexpr (conv == Xact::FieldConvention::B8)
            return true;
        else if constexpr (conv == Xact::FieldConvention::B64)
            return true;
        else if constexpr (conv == Xact::FieldConvention::S)
            return true;
        else if constexpr (conv == Xact::FieldConvention::D)
            return true;
        else
            return false;
    }
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_*__COMMON