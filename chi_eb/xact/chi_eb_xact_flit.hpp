#pragma once

#ifndef __CHI_EB__CHI_XACT_FLIT
#define __CHI_EB__CHI_XACT_FLIT

#include "../../chi/xact/chi_xact_flit_header.hpp"      // IWYU pragma: keep
#include "chi_eb_xact_base.hpp"                         // IWYU pragma: keep
#include "chi_eb_xact_global.hpp"                       // IWYU pragma: keep

#define CHI_XACT_FLIT__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xact_flit.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef CHI_XACT_FLIT__STANDALONE

#endif // __CHI_EB__CHI_XACT_FLIT
