#pragma once

#ifndef __CHI_EB__CHI_PROTOCOL_FLITS
#define __CHI_EB__CHI_PROTOCOL_FLITS

#include "../../chi/spec/chi_protocol_flits_header.hpp"     // IWYU pragma: keep

#define CHI_PROTOCOL_FLITS__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/spec/chi_protocol_flits.hpp"
    #undef  CHI_ISSUE_EB_ENABLE    
}


#undef CHI_PROTOCOL_FLITS__STANDALONE

#endif // __CHI_EB__CHI_PROTOCOL_FLITS
