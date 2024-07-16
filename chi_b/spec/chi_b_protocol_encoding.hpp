#pragma once

#ifndef __CHI_B__CHI_PROTOCOL_ENCODING
#define __CHI_B__CHI_PROTOCOL_ENCODING

#include "../../chi/spec/chi_protocol_encoding_header.hpp"  // IWYU pragma: keep
#include "chi_b_protocol_flits.hpp"                           // IWYU pragma: keep

#define CHI_PROTOCOL_ENCODING__STANDALONE

namespace CHI::B {

    #define CHI_ISSUE_B_ENABLE
    #include "../../chi/spec/chi_protocol_encoding.hpp"
    #undef  CHI_ISSUE_B_ENABLE
}


#undef  CHI_PROTOCOL_ENCODING__STANDALONE

#endif // __CHI_B__CHI_PROTOCOL_ENCODING
