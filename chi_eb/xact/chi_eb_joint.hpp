#pragma once

#ifndef __CHI_EB__CHI_JOINT
#define __CHI_EB__CHI_JOINT

#include "../../chi/xact/chi_joint_header.hpp"          // IWYU pragma: keep
#include "../util/chi_eb_util_decoding.hpp"             // IWYU pragma: keep
#include "chi_eb_xactions.hpp"                          // IWYU pragma: keep

#define CHI_XACT_XACTIONS__STANDALONE

namespace CHI::Eb {

    #define CHI_ISSUE_EB_ENABLE
    #include "../../chi/xact/chi_joint.hpp"
    #undef  CHI_ISSUE_EB_ENABLE
}

#undef CHI_XACT_XACTIONS__STANDALONE

#endif // __CHI_EB__CHI_JOINT
