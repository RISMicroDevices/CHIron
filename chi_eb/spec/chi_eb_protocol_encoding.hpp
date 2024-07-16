#pragma once

#ifndef __CHI_EB__CHI_PROTOCOL_ENCODING
#define __CHI_EB__CHI_PROTOCOL_ENCODING

#include "../../chi/spec/chi_protocol_encoding_header.hpp"  // IWYU pragma: keep
#include "chi_eb_protocol_flits.hpp"                           // IWYU pragma: keep

#define CHI_PROTOCOL_ENCODING__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/spec/chi_protocol_encoding.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef  CHI_PROTOCOL_ENCODING__STANDALONE

#endif // __CHI_EB__CHI_PROTOCOL_ENCODING
