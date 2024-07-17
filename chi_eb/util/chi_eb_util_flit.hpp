#pragma once

#ifndef __CHI_EB__CHI_UTIL_FLIT
#define __CHI_EB__CHI_UTIL_FLIT

#include "../../chi/util/chi_util_flit_header.hpp"      // IWYU pragma: keep
#include "../spec/chi_eb_protocol_flits.hpp"            // IWYU pragma: keep

#define CHI_UTIL_FLIT__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/util/chi_util_flit.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef CHI_UTIL_FLIT__STANDALONE

#endif // __CHI_EB__CHI_UTIL_FLIT
