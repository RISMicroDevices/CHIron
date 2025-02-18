#pragma once

#ifndef __CHI_EB__CHI_XACT_CHECKERS_FIELD
#define __CHI_EB__CHI_XACT_CHECKERS_FIELD

#include "../../chi/xact/chi_xact_checkers_field_header.hpp"    // IWYU pragma: keep
#include "../spec/chi_eb_protocol_encoding.hpp"                 // IWYU pragma: keep
#include "chi_eb_xact_base.hpp"                                 // IWYU pragma: keep
#include "chi_eb_xact_field.hpp"                                // IWYU pragma: keep

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xact_checkers_field.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}

#endif // __CHI_EB__CHI_XACT_CHECKERS_FIELD
