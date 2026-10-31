#include "chi_lunatic_metadata.hpp"

#include <bit>

namespace CHI::Lunatic {

    uint8_t OddByteParity(uint8_t data) noexcept
    {
        return uint8_t(!bool(std::popcount(unsigned(data)) & 1));
    }

    void GenerateMetadata(LinePoison& poison, LineDataCheck& dataCheck, const LineData& data,
                          const ByteEnables& enables, const Options& options) noexcept
    {
        poison.fill(0);
        dataCheck.fill(0);
        GenerateDataCheck(dataCheck, data, enables, options);
    }

    void GenerateDataCheck(LineDataCheck& dataCheck, const LineData& data,
                           const ByteEnables& enables, const Options& options) noexcept
    {
        if (!options.dataCheckEnabled)
            return;
        for (size_t index = 0; index < LINE_SIZE; ++index)
            if (enables[index])
                dataCheck[index] = OddByteParity(data[index]);
    }

    void MergeMetadata(LinePoison& poison, LineDataCheck& dataCheck,
                       const LinePoison& sourcePoison, const LineDataCheck& sourceDataCheck,
                       const ByteEnables& enables, size_t targetOffset, size_t sourceOffset,
                       size_t bytes, const Options& options) noexcept
    {
        if (targetOffset + bytes > LINE_SIZE || sourceOffset + bytes > LINE_SIZE ||
            (targetOffset % 8) != (sourceOffset % 8))
            return;

        if (options.dataCheckEnabled)
        {
            for (size_t index = 0; index < bytes; ++index)
            {
                const size_t target = targetOffset + index;
                if (enables[target])
                    dataCheck[target] = sourceDataCheck[sourceOffset + index];
            }
        }

        if (!options.poisonEnabled)
            return;
        for (size_t chunk = targetOffset / 8; chunk < (targetOffset + bytes + 7) / 8; ++chunk)
        {
            const size_t begin = chunk * 8;
            bool any = false;
            bool all = true;
            for (size_t index = begin; index < begin + 8; ++index)
            {
                const bool enabled = index >= targetOffset && index < targetOffset + bytes && enables[index];
                any |= enabled;
                all &= enabled;
            }
            if (!any)
                continue;
            const size_t sourceChunk = (sourceOffset + begin - targetOffset) / 8;
            if (all)
                poison[chunk] = sourcePoison[sourceChunk];
            else
                poison[chunk] |= sourcePoison[sourceChunk];
        }
    }

    ByteEnables ExpandedEnables(const ByteEnables& enables, size_t targetOffset, size_t bytes)
    {
        ByteEnables result{};
        if (targetOffset + bytes > LINE_SIZE)
            return result;
        for (size_t index = 0; index < bytes; ++index)
            result[targetOffset + index] = enables[index];
        return result;
    }

}
