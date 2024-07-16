#pragma once

#ifndef __CHI_B__CHI_LINK_INTERFACES
#define __CHI_B__CHI_LINK_INTERFACES

#include "../../chi/spec/chi_link_interfaces_header.hpp"     // IWYU pragma: keep
#include "chi_b_link_channels.hpp"                           // IWYU pragma: keep

#define CHI_LINK_INTERFACES__STANDALONE

namespace CHI::B {

    #define CHI_ISSUE_B_ENABLE
    #include "../../chi/spec/chi_link_interfaces.hpp"
    #undef  CHI_ISSUE_B_ENABLE
}


#undef  CHI_LINK_INTERFACES__STANDALONE

#endif // __CHI_B__CHI_LINK_INTERFACES
