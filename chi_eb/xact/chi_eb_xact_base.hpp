#pragma once

#ifndef __CHI_EB__CHI_XACT_BASE
#define __CHI_EB__CHI_XACT_BASE

#include "../../chi/xact/chi_xact_base_header.hpp"      // IWYU pragma: keep
#include "../spec/chi_eb_protocol_encoding.hpp"         // IWYU pragma: keep

#define CHI_XACT_BASE__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xact_base.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef CHI_XACT_BASE__STANDALONE

#endif // __CHI_EB__CHI_XACT_BASE
