#pragma once

#ifndef __CHI__CHI_XACT_FIELD_HEADER
#define __CHI__CHI_XACT_FIELD_HEADER

#include <optional>                                 // IWYU pragma: export
#include <cstring>                                  // IWYU pragma: export

#include "../spec/chi_protocol_encoding_header.hpp" // IWYU pragma: export
#include "../spec/chi_protocol_encoding.hpp"        // IWYU pragma: export

/*
namespace CHI {
*/
    namespace Xact {

        enum class FieldConvention {
            A0      = 0,        // Applicable. Must be zero.
            A1      = 1,        // Applicable. Must be one.
            I0,                 // Inapplicable. Must be zero.
            X,                  // Inapplicable. Can take any value.
            Y,                  // Applicable. Specification constrained.
            B8,                 // Size field must be 8-byte encoding.
            B64,                // Size field must be 64-byte encoding.
            S,                  // Assigned to another shared field.
            D                   // Inapplicable. Must be default value of MPAM field.
        };
    }
/*
}
*/

#endif // __CHI__CHI_XACT_FIELD_HEADER
