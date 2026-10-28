#include "chi_lunatic_protocol.hpp"
#include "../../../chi_eb/spec/chi_eb_protocol_encoding.hpp"

namespace CHI::Lunatic {

RequestClass ClassifyRequest(uint16_t opcode) noexcept
{
    namespace REQ = CHI::Eb::Opcodes::REQ;
    if (opcode == REQ::ReadNoSnp)
        return RequestClass::NonCoherentRead;
    if (opcode == REQ::ReadOnce)
        return RequestClass::ReadOnce;
    if (opcode == REQ::ReadShared || opcode == REQ::ReadUnique)
        return RequestClass::CoherentRead;
    if (opcode == REQ::MakeUnique)
        return RequestClass::MakeUnique;
    if (opcode == REQ::WriteNoSnpPtl || opcode == REQ::WriteNoSnpFull)
        return RequestClass::NonCoherentWrite;
    if (opcode == REQ::WriteNoSnpZero)
        return RequestClass::ZeroWrite;
    if (opcode == REQ::WriteBackPtl || opcode == REQ::WriteBackFull ||
        opcode == REQ::WriteCleanFull || opcode == REQ::WriteEvictFull)
        return RequestClass::CopybackWrite;
    if (opcode == REQ::Evict)
        return RequestClass::Evict;
    if (opcode == REQ::CleanShared || opcode == REQ::CleanInvalid || opcode == REQ::MakeInvalid)
        return RequestClass::CMO;
    return RequestClass::Unsupported;
}

bool IsSupportedRequest(uint16_t opcode) noexcept
{
    return ClassifyRequest(opcode) != RequestClass::Unsupported;
}

bool IsCoherentRequest(uint16_t opcode) noexcept
{
    const auto requestClass = ClassifyRequest(opcode);
    return requestClass == RequestClass::CoherentRead || requestClass == RequestClass::MakeUnique ||
           requestClass == RequestClass::CopybackWrite || requestClass == RequestClass::Evict ||
           requestClass == RequestClass::CMO;
}

bool IsReadRequest(uint16_t opcode) noexcept
{
    const auto requestClass = ClassifyRequest(opcode);
    return requestClass == RequestClass::NonCoherentRead || requestClass == RequestClass::ReadOnce ||
           requestClass == RequestClass::CoherentRead;
}

bool IsWriteRequest(uint16_t opcode) noexcept
{
    const auto requestClass = ClassifyRequest(opcode);
    return requestClass == RequestClass::NonCoherentWrite || requestClass == RequestClass::ZeroWrite ||
           requestClass == RequestClass::CopybackWrite;
}

bool IsCopybackRequest(uint16_t opcode) noexcept
{
    return ClassifyRequest(opcode) == RequestClass::CopybackWrite;
}

bool IsCMORequest(uint16_t opcode) noexcept
{
    return ClassifyRequest(opcode) == RequestClass::CMO;
}

bool RequiresCompAck(const Request& request) noexcept
{
    const auto requestClass = ClassifyRequest(request.opcode);
    if (requestClass == RequestClass::CoherentRead || requestClass == RequestClass::MakeUnique)
        return true;
    return request.expCompAck &&
           (requestClass == RequestClass::NonCoherentRead || requestClass == RequestClass::ReadOnce ||
            requestClass == RequestClass::NonCoherentWrite);
}

}
