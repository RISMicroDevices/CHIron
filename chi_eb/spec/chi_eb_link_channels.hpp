#pragma once

#ifndef __CHI_EB__CHI_LINK_CHANNELS
#define __CHI_EB__CHI_LINK_CHANNELS

#include "../../chi/spec/chi_link_channels_header.hpp"      // IWYU pragma: keep
#include "chi_eb_protocol_flits.hpp"                        // IWYU pragma: keep

#define CHI_LINK_CHANNELS__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/spec/chi_link_channels.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}


#undef  CHI_LINK_CHANNELS__STANDALONE

#endif // __CHI_EB__CHI_LINK_CHANNELS
