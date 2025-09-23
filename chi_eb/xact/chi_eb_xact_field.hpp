#pragma once

#ifndef __CHI_EB__CHI_XACT_FIELD
#define __CHI_EB__CHI_XACT_FIELD

#include "../../chi/xact/chi_xact_field_header.hpp"     // IWYU pragma: keep
#include "../spec/chi_eb_protocol_encoding.hpp"         // IWYU pragma: keep

#define CHI_XACT_FIELD__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_xact_field_eb.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef CHI_XACT_FIELD__STANDALONE

#endif // __CHI_EB__CHI_XACT_FIELD
