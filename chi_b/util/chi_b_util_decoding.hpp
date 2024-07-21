#pragma once

#ifndef __CHI_B__CHI_UTIL_DECODING
#define __CHI_B__CHI_UTIL_DECODING

#include "../../chi/util/chi_util_decoding_header.hpp"      // IWYU pragma: keep
#include "../spec/chi_b_protocol_encoding.hpp"              // IWYU pragma: keep

#define CHI_UTIL_DECODING__STANDALONE

namespace CHI::B {

    #define CHI_ISSUE_B_ENABLE
    #include "../../chi/util/chi_util_decoding.hpp"
    #undef  CHI_ISSUE_B_ENABLE
}


#undef CHI_UTIL_DECODING__STANDALONE

#endif // __CHI_B__CHI_UTIL_DECODING
