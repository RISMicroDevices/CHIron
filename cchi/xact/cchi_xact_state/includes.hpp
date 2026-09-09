#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__INCLUDES
#define __CCHI__CCHI_XACT_STATE__INCLUDES

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>

#include "../../../common/nonstdint.hpp"              // IWYU pragma: keep
#include "../../../common/utility.hpp"               // IWYU pragma: keep
#include "../../../common/eventbus.hpp"              // IWYU pragma: keep

#include "../../spec/cchi_protocol_flits.hpp"         // IWYU pragma: keep
#include "../../spec/cchi_protocol_encoding.hpp"      // IWYU pragma: keep
#include "../../util/cchi_util_decoding.hpp"          // IWYU pragma: keep

#include "../cchi_xact_base/cchi_xact_base_denial.hpp"     // IWYU pragma: keep
#include "../cchi_xact_base/cchi_xact_base_topology.hpp"   // IWYU pragma: keep
#include "../cchi_xact_global.hpp"                        // IWYU pragma: keep
#include "../cchi_xact_flit.hpp"                          // IWYU pragma: keep
#include "../cchi_xactions/cchi_xactions_base.hpp"        // IWYU pragma: keep

#endif // __CCHI__CCHI_XACT_STATE__INCLUDES
