#pragma once

#ifndef __CHI_EB__CHI_XACT_FIELD
#define __CHI_EB__CHI_XACT_FIELD

#include "../../chi/xact/chi_xact_field_header.hpp"     // IWYU pragma: keep

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xact_field_eb.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}

#endif // __CHI_EB__CHI_XACT_FIELD
