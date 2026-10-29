#pragma once

#include "chi_lunatic_config.hpp"

namespace CHI::Lunatic {

    enum class RequestClass : uint8_t {
        Unsupported,
        NonCoherentRead,
        ReadOnce,
        CoherentRead,
        MakeUnique,
        NonCoherentWrite,
        ZeroWrite,
        CopybackWrite,
        Evict,
        CMO
    };

    RequestClass                    ClassifyRequest(uint16_t opcode) noexcept;
    bool                            IsSupportedRequest(uint16_t opcode) noexcept;
    bool                            IsCoherentRequest(uint16_t opcode) noexcept;
    bool                            IsReadRequest(uint16_t opcode) noexcept;
    bool                            IsWriteRequest(uint16_t opcode) noexcept;
    bool                            IsCopybackRequest(uint16_t opcode) noexcept;
    bool                            IsCMORequest(uint16_t opcode) noexcept;
    bool                            RequiresCompAck(const Request& request) noexcept;
    uint64_t                        TransferBeatMask(const Request& request, const Options& options) noexcept;

}
