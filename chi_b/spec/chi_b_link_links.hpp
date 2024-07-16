#pragma once

#ifndef __CHI_B__CHI_LINK_LINKS
#define __CHI_B__CHI_LINK_LINKS

#include "../../chi/spec/chi_link_links_header.hpp"     // IWYU pragma: keep
#include "chi_b_link_interfaces.hpp"                    // IWYU pragma: keep

#define CHI_LINK_LINKS__STANDALONE

namespace CHI::B {

    #define CHI_ISSUE_B_ENABLE
    #include "../../chi/spec/chi_link_links.hpp"
    #undef  CHI_ISSUE_B_ENABLE
}


#undef  CHI_LINK_LINKS__STANDALONE

#endif // __CHI_B__CHI_LINK_LINKS
