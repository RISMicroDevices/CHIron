#pragma once

#ifndef __CHI_EB__CHI_XACT_STATE
#define __CHI_EB__CHI_XACT_STATE

#include "../../chi/xact/chi_xact_state_header.hpp"         // IWYU pragma: keep
#include "chi_eb_xact_base.hpp"                             // IWYU pragma: keep
#include "chi_eb_xactions.hpp"                              // IWYU pragma: keep
#include "../spec/chi_eb_protocol_encoding.hpp"             // IWYU pragma: keep
#include "../util/chi_eb_util_decoding.hpp"                 // IWYU pragma: keep

#define CHI_XACT_STATE__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xact_state.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef CHI_XACT_STATE__STANDALONE

#endif // __CHI_EB__CHI_XACT_STATE
