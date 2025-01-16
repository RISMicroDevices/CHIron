//#pragma once

//#ifndef __CHI__CHI_XACT_CHECKERS
//#define __CHI__CHI_XACT_CHECKERS

#ifndef CHI_XACT_CHECKERS__STANDALONE
#   include "chi_xact_checkers_header.hpp"              // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding_header.hpp"  // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding.hpp"         // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_CHECKERS_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_CHECKERS_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_CHECKERS_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_CHECKERS_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {
        
    }
/*
}
*/


#endif // __CHI__CHI_XACT_CHECKERS_*

//#endif // __CHI__CHI_XACT_CHECKERS
