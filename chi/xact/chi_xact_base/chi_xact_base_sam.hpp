//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_BASE_B__SAM)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_BASE_EB__SAM))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_BASE_B__SAM
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_BASE_EB__SAM
#endif


/*
namespace CHI {
*/
    namespace Xact {

        class SAMScopeEnumBack {
        public:
            const char* name;
            const int   value;

        public:
            inline constexpr SAMScopeEnumBack(const char* name, const int value) noexcept
            : name(name), value(value) { }

        public:
            inline constexpr operator int() const noexcept
            { return value; }

            inline constexpr operator const SAMScopeEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const SAMScopeEnumBack& obj) const noexcept
            { return value == obj.value; }

            inline constexpr bool operator!=(const SAMScopeEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using SAMScopeEnum = const SAMScopeEnumBack*;

        namespace SAMScope {
            inline constexpr SAMScopeEnumBack   AfterSAM    ("AfterSAM",    0);
            inline constexpr SAMScopeEnumBack   BeforeSAM   ("BeforeSAM",   1);
        };
    }
/*
}
*/

#endif
