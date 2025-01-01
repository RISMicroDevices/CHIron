#pragma once

#ifndef __CHI__CHI_XACT_FIELD_HEADER
#define __CHI__CHI_XACT_FIELD_HEADER

#include "../spec/chi_protocol_encoding_header.hpp" // IWYU pragma: export
#include "../spec/chi_protocol_encoding.hpp"        // IWYU pragma: export

/*
namespace CHI {
*/
    namespace Xact {

        enum class FieldConvention {
            A0      = 0,        // 
            A1      = 1,
            I0,
            X,
            Y,
            B8,
            B64,
            S,
            D
        };
    }
/*
}
*/

#endif // __CHI__CHI_XACT_FIELD_HEADER
