#pragma once

#ifndef __CHI_B__CHI_LINK_PORTS
#define __CHI_B__CHI_LINK_PORTS

#include "../../chi/spec/chi_link_ports_header.hpp"         // IWYU pragma: keep
#include "chi_b_link_links.hpp"                             // IWYU pragma: keep

#define CHI_LINK_PORTS__STANDALONE

namespace CHI::B {

    #define CHI_ISSUE_B_ENABLE
    #include "../../chi/spec/chi_link_ports.hpp"
    #undef  CHI_ISSUE_B_ENABLE
}


#undef  CHI_LINK_PORTS__STANDALONE

#endif // __CHI_B__CHI_LINK_PORTS
