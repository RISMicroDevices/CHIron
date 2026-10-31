#include "chi_lunatic_protocol.hpp"
#include "../../../chi_eb/spec/chi_eb_protocol_encoding.hpp"

#include <algorithm>

namespace CHI::Lunatic {

    RequestClass ClassifyRequest(uint16_t opcode) noexcept
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        if (opcode == REQ::ReadNoSnp)
            return RequestClass::NonCoherentRead;
        if (opcode == REQ::ReadOnce || opcode == REQ::ReadOnceCleanInvalid || opcode == REQ::ReadOnceMakeInvalid)
            return RequestClass::ReadOnce;
        if (opcode == REQ::ReadShared || opcode == REQ::ReadUnique || opcode == REQ::ReadClean ||
            opcode == REQ::ReadNotSharedDirty || opcode == REQ::ReadPreferUnique)
            return RequestClass::CoherentRead;
        if (opcode == REQ::MakeUnique)
            return RequestClass::MakeUnique;
        if (opcode == REQ::MakeReadUnique)
            return RequestClass::MakeReadUnique;
        if (opcode == REQ::WriteNoSnpPtl || opcode == REQ::WriteNoSnpFull)
            return RequestClass::NonCoherentWrite;
        if (opcode == REQ::WriteUniquePtl || opcode == REQ::WriteUniqueFull ||
            opcode == REQ::WriteUniquePtlStash || opcode == REQ::WriteUniqueFullStash)
            return RequestClass::ImmediateWrite;
        if (opcode == REQ::WriteNoSnpZero)
            return RequestClass::ZeroWrite;
        if (opcode == REQ::WriteUniqueZero)
            return RequestClass::WriteUniqueZero;
        if (opcode == REQ::WriteBackPtl || opcode == REQ::WriteBackFull ||
            opcode == REQ::WriteCleanFull || opcode == REQ::WriteEvictFull)
            return RequestClass::CopybackWrite;
        if (opcode == REQ::Evict)
            return RequestClass::Evict;
        if (opcode == REQ::WriteEvictOrEvict)
            return RequestClass::WriteEvictOrEvict;
        if (opcode == REQ::CleanShared || opcode == REQ::CleanInvalid || opcode == REQ::MakeInvalid)
            return RequestClass::CMO;
        if (opcode == REQ::CleanUnique)
            return RequestClass::CleanUnique;
        if (opcode == REQ::CleanSharedPersist || opcode == REQ::CleanSharedPersistSep)
            return RequestClass::Persistence;
        if (opcode == REQ::StashOnceUnique || opcode == REQ::StashOnceShared ||
            opcode == REQ::StashOnceSepUnique || opcode == REQ::StashOnceSepShared)
            return RequestClass::Stash;
        if (REQ::AtomicStore::Is(opcode) || REQ::AtomicLoad::Is(opcode) ||
            opcode == REQ::AtomicSwap || opcode == REQ::AtomicCompare)
            return RequestClass::Atomic;
        return RequestClass::Unsupported;
    }

    bool IsSupportedRequest(uint16_t opcode) noexcept
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        if (IsAtomicRequest(opcode))
            return true;
        switch (opcode)
        {
            case REQ::ReadNoSnp:
            case REQ::ReadOnce:
            case REQ::ReadShared:
            case REQ::ReadUnique:
            case REQ::ReadClean:
            case REQ::ReadNotSharedDirty:
            case REQ::ReadPreferUnique:
            case REQ::MakeUnique:
            case REQ::MakeReadUnique:
            case REQ::ReadOnceCleanInvalid:
            case REQ::ReadOnceMakeInvalid:
            case REQ::WriteNoSnpPtl:
            case REQ::WriteNoSnpFull:
            case REQ::WriteNoSnpZero:
            case REQ::WriteUniquePtl:
            case REQ::WriteUniqueFull:
            case REQ::WriteUniquePtlStash:
            case REQ::WriteUniqueFullStash:
            case REQ::WriteUniqueZero:
            case REQ::WriteEvictOrEvict:
            case REQ::WriteBackPtl:
            case REQ::WriteBackFull:
            case REQ::WriteCleanFull:
            case REQ::WriteEvictFull:
            case REQ::Evict:
            case REQ::CleanUnique:
            case REQ::CleanSharedPersist:
            case REQ::CleanSharedPersistSep:
            case REQ::CleanShared:
            case REQ::CleanInvalid:
            case REQ::MakeInvalid:
            case REQ::StashOnceUnique:
            case REQ::StashOnceShared:
            case REQ::StashOnceSepUnique:
            case REQ::StashOnceSepShared:
                return true;
            default:
                return false;
        }
    }

    bool IsCoherentRequest(uint16_t opcode) noexcept
    {
        const auto requestClass = ClassifyRequest(opcode);
        return requestClass == RequestClass::CoherentRead || requestClass == RequestClass::MakeUnique ||
               requestClass == RequestClass::MakeReadUnique || requestClass == RequestClass::ImmediateWrite ||
               requestClass == RequestClass::WriteUniqueZero || requestClass == RequestClass::CopybackWrite ||
               requestClass == RequestClass::Evict || requestClass == RequestClass::WriteEvictOrEvict ||
               requestClass == RequestClass::CMO || requestClass == RequestClass::CleanUnique ||
               requestClass == RequestClass::Persistence || requestClass == RequestClass::Stash ||
               requestClass == RequestClass::Atomic;
    }

    bool IsReadRequest(uint16_t opcode) noexcept
    {
        const auto requestClass = ClassifyRequest(opcode);
        return requestClass == RequestClass::NonCoherentRead || requestClass == RequestClass::ReadOnce ||
               requestClass == RequestClass::CoherentRead || requestClass == RequestClass::MakeReadUnique;
    }

    bool IsWriteRequest(uint16_t opcode) noexcept
    {
        const auto requestClass = ClassifyRequest(opcode);
        return requestClass == RequestClass::NonCoherentWrite || requestClass == RequestClass::ImmediateWrite ||
               requestClass == RequestClass::ZeroWrite || requestClass == RequestClass::WriteUniqueZero ||
               requestClass == RequestClass::CopybackWrite || requestClass == RequestClass::WriteEvictOrEvict ||
               requestClass == RequestClass::Atomic;
    }

    bool IsCopybackRequest(uint16_t opcode) noexcept
    {
        const auto requestClass = ClassifyRequest(opcode);
        return requestClass == RequestClass::CopybackWrite || requestClass == RequestClass::WriteEvictOrEvict;
    }

    bool IsCMORequest(uint16_t opcode) noexcept
    {
        const auto requestClass = ClassifyRequest(opcode);
        return requestClass == RequestClass::CMO || requestClass == RequestClass::CleanUnique ||
               requestClass == RequestClass::Persistence;
    }

    bool IsAtomicRequest(uint16_t opcode) noexcept
    {
        return ClassifyRequest(opcode) == RequestClass::Atomic;
    }

    bool IsStashRequest(uint16_t opcode) noexcept
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        return ClassifyRequest(opcode) == RequestClass::Stash || opcode == REQ::WriteUniquePtlStash ||
               opcode == REQ::WriteUniqueFullStash;
    }

    bool IsSeparatedStashRequest(uint16_t opcode) noexcept
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        return opcode == REQ::StashOnceSepUnique || opcode == REQ::StashOnceSepShared;
    }

    bool IsOrderingSupported(uint16_t opcode) noexcept
    {
        if (IsAtomicRequest(opcode))
            return true;
        namespace REQ = CHI::Eb::Opcodes::REQ;
        switch (opcode)
        {
            case REQ::ReadNoSnp:
            case REQ::ReadOnce:
            case REQ::ReadOnceCleanInvalid:
            case REQ::ReadOnceMakeInvalid:
            case REQ::WriteNoSnpPtl:
            case REQ::WriteNoSnpFull:
            case REQ::WriteNoSnpZero:
            case REQ::WriteUniquePtl:
            case REQ::WriteUniqueFull:
            case REQ::WriteUniqueZero:
                return true;
            default:
                return false;
        }
    }

    bool IsOrderedWriteObservation(const Request& request) noexcept
    {
        const auto requestClass = ClassifyRequest(request.opcode);
        return request.order == 2 && request.expCompAck &&
            (requestClass == RequestClass::NonCoherentWrite || requestClass == RequestClass::ImmediateWrite);
    }

    bool RequiresCompAck(const Request& request) noexcept
    {
        const auto requestClass = ClassifyRequest(request.opcode);
        if (requestClass == RequestClass::CoherentRead || requestClass == RequestClass::MakeUnique ||
            requestClass == RequestClass::MakeReadUnique || requestClass == RequestClass::CleanUnique)
            return true;
        return request.expCompAck &&
               (requestClass == RequestClass::NonCoherentRead || requestClass == RequestClass::ReadOnce ||
                requestClass == RequestClass::NonCoherentWrite || requestClass == RequestClass::ImmediateWrite);
    }

    bool RetryRequestCompatible(const Request& original, const Request& resend, NodeId retrySource) noexcept
    {
        if (resend.target != original.target && resend.target != retrySource)
            return false;

        Request left = original;
        Request right = resend;
        left.target = right.target = 0;
        left.txnId = right.txnId = 0;
        left.qos = right.qos = 0;
        left.traceTag = right.traceTag = 0;
        left.allowRetry = right.allowRetry = false;
        return left == right;
    }

    uint64_t TransferBeatMask(const Request& request, const Options& options) noexcept
    {
        const size_t bytes = BytesPerBeat(options);
        const size_t transfer = std::min<size_t>(LINE_SIZE, size_t(1) << request.size);
        const size_t first = (request.address % LINE_SIZE) / bytes;
        const size_t count = std::max<size_t>(1, (transfer + bytes - 1) / bytes);
        uint64_t mask = 0;
        for (size_t beat = 0; beat < count && first + beat < BeatsPerLine(options); ++beat)
            mask |= uint64_t(1) << (first + beat);
        return mask;
    }

}
