#pragma once

#ifndef __CHI_B__CHI_PROTOCOL_FLITS
#define __CHI_B__CHI_PROTOCOL_FLITS

#include "../../chi/spec/chi_protocol_flits_header.hpp"     // IWYU pragma: keep


namespace CHI::B {

    #define CHI_ISSUE_B_ENABLE
    #include "../../chi/spec/chi_protocol_flits.hpp"
    #undef  CHI_ISSUE_B_ENABLE
}


#endif // __CHI_B__CHI_PROTOCOL_FLITS
