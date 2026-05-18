#pragma once

#ifndef CHI_ISSUE_EB_ENABLE
#define CHI_ISSUE_EB_ENABLE
#endif

#include "chi_eb/spec/chi_eb_protocol.hpp"
#include "chi_eb/util/chi_eb_util_decoding.hpp"
#include "chi/expresso/chi_expresso_flit_header.hpp"

#ifndef __CHI__CHI_EXPRESSO_FLIT_EB
#define CHI_EXPRESSO_FLIT__STANDALONE
namespace CHI::Eb {
#include "chi/expresso/chi_expresso_flit.hpp"
}
#undef CHI_EXPRESSO_FLIT__STANDALONE
#endif
