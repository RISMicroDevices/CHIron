#pragma once

#ifndef __CHI_EB__CHI_XACT_GLOBAL
#define __CHI_EB__CHI_XACT_GLOBAL

#include "../../chi/xact/chi_xact_global_header.hpp"            // IWYU pragma: keep
#include "chi_eb_xact_base.hpp"                                 // IWYU pragma: keep
#include "chi_eb_xact_checkers_field.hpp"                       // IWYU pragma: keep

#define CHI_XACT_GLOBAL__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xact_global.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef CHI_XACT_GLOBAL__STANDALONE

#endif // __CHI_EB__CHI_XACT_GLOBAL