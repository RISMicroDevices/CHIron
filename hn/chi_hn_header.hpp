#pragma once

#ifndef __CHI__HN_HEADER
#define __CHI__HN_HEADER

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>
#include <functional>
#include <cassert>
#include <cstdint>

#ifndef CHI_HN__STANDALONE
#   include "../chi/spec/chi_protocol_flits.hpp"        // IWYU pragma: keep
#   include "../chi/spec/chi_protocol_encoding.hpp"     // IWYU pragma: keep
#   include "../chi/xact/chi_joint.hpp"                 // IWYU pragma: keep
#   include "../chi/xact/chi_xact_state.hpp"            // IWYU pragma: keep
#endif

#endif // __CHI__HN_HEADER
