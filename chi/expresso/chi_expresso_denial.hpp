//#pragma once

//#ifndef __CHI_CHI_EXPRESSO_DENIAL
//#define __CHI_CHI_EXPRESSO_DENIAL

#ifndef CHI_EXPRESSO_DENIAL__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_expresso_denial_header.hpp"    // IWYU pragma: keep
#   include "chi_expresso_xaction.hpp"          // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_EXPRESSO_DENIAL_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_EXPRESSO_DENIAL_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_EXPRESSO_DENIAL_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_EXPRESSO_DENIAL_EB
#endif


/*
namespace CHI {
*/
    namespace Expresso::Denial {

        class SourceEnumBack {
        public:
            const char* name;
            const int   ordinal;

        public:
            inline constexpr SourceEnumBack(const char* name, const int ordinal) noexcept
            : name(name), ordinal(ordinal) { }

        public:
            inline constexpr operator int() const noexcept
            { return ordinal; }

            inline constexpr operator const SourceEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const SourceEnumBack& obj) const noexcept
            { return ordinal == obj.ordinal; }
            
            inline constexpr bool operator!=(const SourceEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using SourceEnum = const SourceEnumBack*;

        namespace Source {
            inline constexpr SourceEnumBack ENVIRONMENT     ("ENVIRONMENT"  , 0); // Framework environment & outer components
            inline constexpr SourceEnumBack XACTION         ("XACTION"      , 1); // Transaction-level
            inline constexpr SourceEnumBack JOINT           ("JOINT"        , 2); // Interface-level or Node-level
        };
    }
/*
}
*/

#endif //

//#endif
