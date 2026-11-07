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

    uint8_t MergeResponseErrors(uint8_t first, uint8_t second) noexcept
    {
        if (first == CHI::Eb::RespErrs::OK)
            return second;
        if (second == CHI::Eb::RespErrs::OK)
            return first;
        if (first == CHI::Eb::RespErrs::NDERR || second == CHI::Eb::RespErrs::NDERR)
            return CHI::Eb::RespErrs::NDERR;
        if (first == CHI::Eb::RespErrs::DERR || second == CHI::Eb::RespErrs::DERR)
            return CHI::Eb::RespErrs::DERR;
        return first;
    }

    bool IsTagOperationValid(const Request& request) noexcept
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        if (request.tagOp == CHI::Eb::TagOp::Invalid)
            return true;
        if (IsReadRequest(request.opcode))
            return request.tagOp == CHI::Eb::TagOp::Transfer ||
                (request.tagOp == CHI::Eb::TagOp::MatchFetch &&
                 (request.opcode == REQ::ReadUnique || request.opcode == REQ::ReadNoSnp));
        if (IsAtomicRequest(request.opcode))
            return request.tagOp == CHI::Eb::TagOp::MatchFetch;
        switch (request.opcode)
        {
            case REQ::WriteNoSnpFull:
            case REQ::WriteNoSnpPtl:
            case REQ::WriteUniqueFull:
            case REQ::WriteUniqueFullStash:
            case REQ::WriteUniquePtl:
            case REQ::WriteUniquePtlStash:
            case REQ::WriteBackFull:
            case REQ::WriteCleanFull:
                return request.tagOp == CHI::Eb::TagOp::MatchFetch;
            default:
                return false;
        }
    }

    bool IsLPIDApplicable(const Request& request) noexcept
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        const bool nonCacheableOrDevice = request.memAttr == CHI::Eb::MemAttrs::Device_nEWA ||
            request.memAttr == CHI::Eb::MemAttrs::Device_EWA ||
            request.memAttr == CHI::Eb::MemAttrs::NonCacheable_NonBufferable ||
            request.memAttr == CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        switch (request.opcode)
        {
            case REQ::ReadNoSnp:
            case REQ::WriteNoSnpPtl:
            case REQ::WriteNoSnpFull:
            case REQ::WriteNoSnpZero:
                return request.snoopMe || nonCacheableOrDevice;
            case REQ::ReadClean:
            case REQ::ReadShared:
            case REQ::ReadNotSharedDirty:
            case REQ::ReadPreferUnique:
            case REQ::MakeReadUnique:
            case REQ::CleanUnique:
                return request.snoopMe;
            default:
                return false;
        }
    }

    bool IsRequestAttributeValid(const Request& request) noexcept
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        const auto requestClass = ClassifyRequest(request.opcode);
        const bool nonCacheable = request.memAttr == CHI::Eb::MemAttrs::Device_nEWA ||
            request.memAttr == CHI::Eb::MemAttrs::Device_EWA ||
            request.memAttr == CHI::Eb::MemAttrs::NonCacheable_NonBufferable ||
            request.memAttr == CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        const bool writeBack = request.memAttr == CHI::Eb::MemAttrs::WriteBack_NoAllocate ||
            request.memAttr == CHI::Eb::MemAttrs::WriteBack_Allocate;
        const bool normalAttributes = nonCacheable || writeBack;
        const bool device = request.memAttr == CHI::Eb::MemAttrs::Device_nEWA ||
            request.memAttr == CHI::Eb::MemAttrs::Device_EWA;
        const bool orderedClass = requestClass == RequestClass::NonCoherentRead ||
            requestClass == RequestClass::ReadOnce || requestClass == RequestClass::NonCoherentWrite ||
            requestClass == RequestClass::ImmediateWrite || requestClass == RequestClass::ZeroWrite ||
            requestClass == RequestClass::WriteUniqueZero || requestClass == RequestClass::Atomic;
        if (request.order == 1 || (request.order == 2 && !orderedClass) ||
            (request.order == 3 && (!device ||
             (requestClass != RequestClass::NonCoherentRead && requestClass != RequestClass::NonCoherentWrite &&
              requestClass != RequestClass::ZeroWrite && requestClass != RequestClass::Atomic))))
            return false;

        switch (requestClass)
        {
            case RequestClass::NonCoherentRead:
                return !request.snpAttr && normalAttributes && (request.order != 3 || !request.expCompAck);
            case RequestClass::ReadOnce:
                return request.snpAttr && writeBack &&
                    (request.opcode != REQ::ReadOnceMakeInvalid ||
                     request.memAttr == CHI::Eb::MemAttrs::WriteBack_NoAllocate);
            case RequestClass::CoherentRead:
            case RequestClass::MakeUnique:
            case RequestClass::MakeReadUnique:
                return request.snpAttr && writeBack && request.size == 6 && request.expCompAck &&
                    (request.opcode != REQ::ReadUnique || !request.likelyShared);
            case RequestClass::NonCoherentWrite:
                return !request.snpAttr && normalAttributes &&
                    (request.opcode != REQ::WriteNoSnpFull || request.size == 6) &&
                    (!request.expCompAck || request.order == 2);
            case RequestClass::ImmediateWrite:
                return request.snpAttr && writeBack &&
                    ((request.opcode != REQ::WriteUniqueFull && request.opcode != REQ::WriteUniqueFullStash) ||
                     request.size == 6) && (!request.expCompAck || request.order == 2);
            case RequestClass::ZeroWrite:
                return !request.snpAttr && normalAttributes && request.size == 6 && !request.expCompAck;
            case RequestClass::WriteUniqueZero:
                return request.snpAttr && writeBack && request.size == 6 && !request.expCompAck;
            case RequestClass::CopybackWrite:
                if (!request.snpAttr || request.size != 6 || request.expCompAck)
                    return false;
                if (request.opcode == REQ::WriteEvictFull)
                    return request.memAttr == CHI::Eb::MemAttrs::WriteBack_Allocate;
                return writeBack;
            case RequestClass::Evict:
                return request.snpAttr && request.memAttr == CHI::Eb::MemAttrs::WriteBack_NoAllocate &&
                    request.size == 6 && !request.expCompAck;
            case RequestClass::WriteEvictOrEvict:
                return request.snpAttr && request.memAttr == CHI::Eb::MemAttrs::WriteBack_Allocate &&
                    request.size == 6 && request.expCompAck;
            case RequestClass::CMO:
            case RequestClass::Persistence:
                return normalAttributes && (writeBack || !request.snpAttr) && request.size == 6 &&
                    !request.expCompAck && request.order == 0;
            case RequestClass::CleanUnique:
                return request.snpAttr && writeBack && request.size == 6 && request.expCompAck;
            case RequestClass::Stash:
                return request.snpAttr && writeBack && request.size == 6 && !request.expCompAck;
            case RequestClass::Atomic:
                return (request.snpAttr ? writeBack : normalAttributes && !request.snoopMe) &&
                    !request.expCompAck;
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
            case REQ::WriteUniquePtlStash:
            case REQ::WriteUniqueFullStash:
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

    bool AllowsCompAck(const Request& request) noexcept
    {
        return RequiresCompAck(request);
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

    bool IsPCrdReturnValid(const Request& request) noexcept
    {
        return request.opcode == CHI::Eb::Opcodes::REQ::PCrdReturn && request.txnId == 0 &&
            request.returnNid == 0 && request.returnTxnId == 0 && request.address == 0 &&
            request.size == 0 && request.order == 0 && request.memAttr == 0 && request.snpAttr == 0 &&
            request.pGroupId == 0 && request.stashGroupId == 0 && request.stashLpid == 0 &&
            !request.nonSecure && !request.allowRetry && !request.expCompAck && !request.likelyShared &&
            !request.stashNidValid && !request.stashLpidValid && !request.endian && !request.deep &&
            !request.doDwt && !request.snoopMe && request.mpam == 0 &&
            request.tagOp == CHI::Eb::TagOp::Invalid && request.tagGroupId == 0;
    }

    Request MakePCrdReturnRequest(NodeId source, NodeId target, uint8_t pCrdType,
                                  uint8_t qos, uint8_t traceTag) noexcept
    {
        Request request;
        request.source = source;
        request.target = target;
        request.opcode = CHI::Eb::Opcodes::REQ::PCrdReturn;
        request.size = 0;
        request.allowRetry = false;
        request.pCrdType = pCrdType;
        request.qos = qos;
        request.traceTag = traceTag;
        return request;
    }

    TransferGeometry GetTransferGeometry(const Request& request, const Options& options) noexcept
    {
        TransferGeometry geometry;
        if (request.size > 6)
            return geometry;

        geometry.transferBytes = size_t(1) << request.size;
        geometry.alignedAddress = request.address & ~(geometry.transferBytes - 1);
        geometry.windowOffset = geometry.alignedAddress % LINE_SIZE;
        geometry.rawOffset = request.address % LINE_SIZE;
        const bool device = CHI::Eb::MemAttrs::ExtractDevice(request.memAttr) != 0;
        geometry.validOffset = device ? geometry.rawOffset : geometry.windowOffset;
        geometry.validBytes = device
            ? geometry.windowOffset + geometry.transferBytes - geometry.rawOffset : geometry.transferBytes;
        geometry.ccid = (request.address >> 4) & 3;

        const size_t bytes = BytesPerBeat(options);
        const size_t first = geometry.windowOffset / bytes;
        const size_t count = std::max<size_t>(1, (geometry.transferBytes + bytes - 1) / bytes);
        for (size_t beat = 0; beat < count && first + beat < BeatsPerLine(options); ++beat)
            geometry.beatMask |= uint64_t(1) << (first + beat);
        return geometry;
    }

    uint64_t TransferBeatMask(const Request& request, const Options& options) noexcept
    {
        return GetTransferGeometry(request, options).beatMask;
    }

}
