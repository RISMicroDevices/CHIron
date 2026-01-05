//#pragma once

//#ifndef __CHI__CHI_XACT_BASE
//#define __CHI__CHI_XACT_BASE

#ifndef CHI_XACT_BASE__STANDALONE
#   include "chi_xact_base_header.hpp"                  // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding_header.hpp"  // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding.hpp"         // IWYU pragma: keep
#endif

#include "chi_xact_base/chi_xact_base_denial.hpp"       // IWYU pragma: keep
#include "chi_xact_base/chi_xact_base_topology.hpp"     // IWYU pragma: keep
#include "chi_xact_base/chi_xact_base_sam.hpp"          // IWYU pragma: keep


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_BASE_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_BASE_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_BASE_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_BASE_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {

    }
/*
}
*/


#endif // __CHI__CHI_XACT_BASE_*

//#endif // __CHI__CHI_XACT_BASE
