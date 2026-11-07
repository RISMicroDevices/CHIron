#pragma once

#include "chi_lunatic_state.hpp"

#include <vector>

namespace CHI::Lunatic {

    struct OrdinarySnoopResponse {
        uint16_t opcode = 0;
        uint8_t  resp = 0;
        uint8_t  dataPull = 0;
        bool     withData = false;
        bool     partial = false;
        bool     retToSrc = false;
        bool     doNotGoToSD = false;
    };

    struct ForwardedSnoopResponse {
        uint16_t opcode = 0;
        uint8_t  resp = 0;
        uint8_t  fwdState = 0;
        bool     withData = false;
        bool     retToSrc = false;
        bool     doNotGoToSD = false;
    };

    std::vector<CacheState> PossibleSnoopStates(CacheState tracked);
    bool IsOrdinarySnoopResponseLegal(const std::vector<CacheState>& possibleInitial,
        const OrdinarySnoopResponse& response) noexcept;
    bool IsForwardedSnoopResponseLegal(const std::vector<CacheState>& possibleInitial,
        const ForwardedSnoopResponse& response) noexcept;

}
