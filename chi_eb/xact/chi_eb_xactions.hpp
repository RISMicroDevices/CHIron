#pragma once

#ifndef __CHI_EB__CHI_XACT_XACTIONS
#define __CHI_EB__CHI_XACT_XACTIONS

#include "../../chi/xact/chi_xactions_header.hpp"       // IWYU pragma: keep
#include "chi_eb_xact_base.hpp"                         // IWYU pragma: keep
#include "chi_eb_xact_global.hpp"                       // IWYU pragma: keep
#include "chi_eb_xact_flit.hpp"                         // IWYU pragma: keep

#define CHI_XACT_XACTIONS__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xactions.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef CHI_XACT_XACTIONS__STANDALONE

#endif // __CHI_EB__CHI_XACT_XACTIONS
