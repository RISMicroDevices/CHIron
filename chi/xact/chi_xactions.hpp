//#pragma once

//#ifndef __CHI__CHI_XACT_XACTIONS
//#define __CHI__CHI_XACT_XACTIONS

#include "chi_xactions/chi_xactions_base.hpp"       // IWYU pragma: keep
#include "chi_xactions/chi_xactions_impl.hpp"       // IWYU pragma: keep


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {

    }
/*
}
*/


#endif // __CHI__CHI_XACT_XACTIONS_*

//#endif // __CHI__CHI_XACT_XACTIONS
