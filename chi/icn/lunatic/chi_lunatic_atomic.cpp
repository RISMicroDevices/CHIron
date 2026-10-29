#include "chi_lunatic_atomic.hpp"
#include "../../../chi_eb/spec/chi_eb_protocol_encoding.hpp"

namespace CHI::Lunatic {
    namespace {

        uint64_t ReadInteger(const LineData& data, size_t offset, size_t bytes, bool bigEndian)
        {
            uint64_t value = 0;
            for (size_t index = 0; index < bytes; ++index)
            {
                const size_t shift = (bigEndian ? bytes - 1 - index : index) * 8;
                value |= uint64_t(data[offset + index]) << shift;
            }
            return value;
        }

        void WriteInteger(LineData& data, size_t offset, size_t bytes, bool bigEndian, uint64_t value)
        {
            for (size_t index = 0; index < bytes; ++index)
            {
                const size_t shift = (bigEndian ? bytes - 1 - index : index) * 8;
                data[offset + index] = uint8_t(value >> shift);
            }
        }

        uint64_t BeatMask(size_t offset, size_t bytes, const Options& options)
        {
            const size_t beatBytes = BytesPerBeat(options);
            const size_t first = offset / beatBytes;
            const size_t last = (offset + bytes - 1) / beatBytes;
            uint64_t mask = 0;
            for (size_t beat = first; beat <= last && beat < BeatsPerLine(options); ++beat)
                mask |= uint64_t(1) << beat;
            return mask;
        }

    }

    bool GetAtomicGeometry(const Request& request, AtomicGeometry& geometry) noexcept
    {
        const bool compare = request.opcode == CHI::Eb::Opcodes::REQ::AtomicCompare;
        if ((!compare && request.size > 3) || (compare && (request.size == 0 || request.size > 5)))
            return false;
        geometry.windowBytes = size_t(1) << request.size;
        geometry.operandBytes = compare ? geometry.windowBytes / 2 : geometry.windowBytes;
        if (request.address % geometry.operandBytes != 0)
            return false;
        geometry.windowOffset = (request.address & ~(geometry.windowBytes - 1)) % LINE_SIZE;
        return geometry.windowOffset + geometry.windowBytes <= LINE_SIZE;
    }

    AtomicOperation GetAtomicOperation(uint16_t opcode) noexcept
    {
        if (opcode == CHI::Eb::Opcodes::REQ::AtomicSwap)
            return AtomicOperation::Swap;
        if (opcode == CHI::Eb::Opcodes::REQ::AtomicCompare)
            return AtomicOperation::Compare;
        return static_cast<AtomicOperation>(opcode & 7);
    }

    uint64_t AtomicOperandBeatMask(const Request& request, const Options& options) noexcept
    {
        AtomicGeometry geometry;
        if (!GetAtomicGeometry(request, geometry))
            return 0;
        return BeatMask(geometry.windowOffset, geometry.windowBytes, options);
    }

    uint64_t AtomicResultBeatMask(const Request& request, const Options& options) noexcept
    {
        AtomicGeometry geometry;
        if (!GetAtomicGeometry(request, geometry))
            return 0;
        return BeatMask(request.address % LINE_SIZE, geometry.operandBytes, options);
    }

    AtomicExecution ExecuteAtomic(const Request& request, const LineData& initial,
                                  const LineData& operands) noexcept
    {
        AtomicGeometry geometry;
        AtomicExecution result{ initial, false };
        if (!GetAtomicGeometry(request, geometry))
            return result;

        const size_t address = request.address % LINE_SIZE;
        const size_t bytes = geometry.operandBytes;
        const AtomicOperation operation = GetAtomicOperation(request.opcode);
        if (operation == AtomicOperation::Compare)
        {
            const size_t swapAddress = (request.address ^ bytes) % LINE_SIZE;
            bool equal = true;
            for (size_t index = 0; index < bytes; ++index)
                equal &= initial[address + index] == operands[address + index];
            if (equal)
            {
                for (size_t index = 0; index < bytes; ++index)
                    result.data[swapAddress + index] = operands[swapAddress + index];
                result.write = true;
            }
            return result;
        }
        if (operation == AtomicOperation::Swap)
        {
            for (size_t index = 0; index < bytes; ++index)
                result.data[address + index] = operands[address + index];
            result.write = true;
            return result;
        }

        const uint64_t memory = ReadInteger(initial, address, bytes, request.endian);
        const uint64_t transmitted = ReadInteger(operands, address, bytes, request.endian);
        const uint64_t sign = uint64_t(1) << (bytes * 8 - 1);
        uint64_t updated = memory;
        switch (operation)
        {
            case AtomicOperation::Add:  updated = memory + transmitted; break;
            case AtomicOperation::Clr:  updated = memory & ~transmitted; break;
            case AtomicOperation::Eor:  updated = memory ^ transmitted; break;
            case AtomicOperation::Set:  updated = memory | transmitted; break;
            case AtomicOperation::SMax: updated = (memory ^ sign) < (transmitted ^ sign) ? transmitted : memory; break;
            case AtomicOperation::SMin: updated = (memory ^ sign) < (transmitted ^ sign) ? memory : transmitted; break;
            case AtomicOperation::UMax: updated = memory < transmitted ? transmitted : memory; break;
            case AtomicOperation::UMin: updated = memory < transmitted ? memory : transmitted; break;
            default: return AtomicExecution{ initial, false };
        }
        WriteInteger(result.data, address, bytes, request.endian, updated);
        result.write = true;
        return result;
    }

}
