#pragma once

#ifndef __CHI_EB__CHI_LINK_INTERFACES
#define __CHI_EB__CHI_LINK_INTERFACES

#include "../../chi/spec/chi_link_interfaces_header.hpp"    // IWYU pragma: keep
#include "chi_eb_link_channels.hpp"                         // IWYU pragma: keep

#define CHI_LINK_INTERFACES__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/spec/chi_link_interfaces.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef  CHI_LINK_INTERFACES__STANDALONE

#endif // __CHI_EB__CHI_LINK_INTERFACES
