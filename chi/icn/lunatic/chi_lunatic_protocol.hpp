#pragma once

#include "chi_lunatic_config.hpp"

namespace CHI::Lunatic {

    enum class RequestClass : uint8_t {
        Unsupported,
        NonCoherentRead,
        ReadOnce,
        CoherentRead,
        MakeUnique,
        MakeReadUnique,
        NonCoherentWrite,
        ImmediateWrite,
        ZeroWrite,
        WriteUniqueZero,
        CopybackWrite,
        Evict,
        WriteEvictOrEvict,
        CMO,
        CleanUnique,
        Persistence,
        Stash,
        Atomic
    };

    RequestClass ClassifyRequest(uint16_t opcode) noexcept;
    bool         IsSupportedRequest(uint16_t opcode) noexcept;
    bool         IsCoherentRequest(uint16_t opcode) noexcept;
    bool         IsReadRequest(uint16_t opcode) noexcept;
    bool         IsWriteRequest(uint16_t opcode) noexcept;
    bool         IsCopybackRequest(uint16_t opcode) noexcept;
    bool         IsCMORequest(uint16_t opcode) noexcept;
    bool         IsAtomicRequest(uint16_t opcode) noexcept;
    bool         IsStashRequest(uint16_t opcode) noexcept;
    bool         IsSeparatedStashRequest(uint16_t opcode) noexcept;
    bool         IsOrderingSupported(uint16_t opcode) noexcept;
    bool         IsOrderedWriteObservation(const Request& request) noexcept;
    bool         RequiresCompAck(const Request& request) noexcept;
    bool         AllowsCompAck(const Request& request) noexcept;
    bool         RetryRequestCompatible(const Request& original, const Request& resend,
                                        NodeId retrySource) noexcept;
    uint64_t     TransferBeatMask(const Request& request, const Options& options) noexcept;

}
