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
    bool         IsRequestAttributeValid(const Request& request) noexcept;
    uint8_t      MergeResponseErrors(uint8_t first, uint8_t second) noexcept;
    bool         IsTagOperationValid(const Request& request) noexcept;
    bool         IsLPIDApplicable(const Request& request) noexcept;
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
    bool         IsPCrdReturnValid(const Request& request) noexcept;
    Request      MakePCrdReturnRequest(NodeId source, NodeId target, uint8_t pCrdType,
                                       uint8_t qos = 0, uint8_t traceTag = 0) noexcept;

    struct TransferGeometry {
        uint64_t  alignedAddress = 0;
        size_t    windowOffset = 0;
        size_t    rawOffset = 0;
        size_t    transferBytes = 0;
        size_t    validOffset = 0;
        size_t    validBytes = 0;
        uint64_t  beatMask = 0;
        uint8_t   ccid = 0;
    };

    TransferGeometry GetTransferGeometry(const Request& request, const Options& options) noexcept;
    uint64_t         TransferBeatMask(const Request& request, const Options& options) noexcept;

}
