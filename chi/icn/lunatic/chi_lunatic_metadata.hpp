#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_types.hpp"

namespace CHI::Lunatic {

    struct LineImage {
        LineData      data{};
        ByteEnables   validBytes{};
        LinePoison    poison{};
        LineDataCheck dataCheck{};
    };

    uint8_t OddByteParity(uint8_t data) noexcept;
    bool    HasUnsupportedMetadataError(const Data& flit, const ByteEnables& enables,
                                        size_t targetOffset, size_t sourceOffset, size_t bytes,
                                        const Options& options) noexcept;

    void GenerateMetadata(LinePoison& poison, LineDataCheck& dataCheck, const LineData& data,
                          const ByteEnables& enables, const Options& options) noexcept;
    void GenerateDataCheck(LineDataCheck& dataCheck, const LineData& data,
                           const ByteEnables& enables, const Options& options) noexcept;
    void MergeMetadata(LinePoison& poison, LineDataCheck& dataCheck,
                       const LinePoison& sourcePoison, const LineDataCheck& sourceDataCheck,
                       const ByteEnables& enables, size_t targetOffset, size_t sourceOffset,
                       size_t bytes, const Options& options) noexcept;
    ByteEnables ExpandedEnables(const ByteEnables& enables, size_t targetOffset, size_t bytes);

    template<typename Destination, typename Source>
    void CopyMetadata(Destination& destination, const Source& source, const Options& options)
    {
        if (options.poisonEnabled)
            destination.poison = source.poison;
        else
            destination.poison.fill(0);
        if (options.dataCheckEnabled)
            destination.dataCheck = source.dataCheck;
        else
            destination.dataCheck.fill(0);
    }

    template<typename Source>
    LineImage MakeLineImage(const Source& source, const Options& options)
    {
        LineImage result;
        result.data = source.data;
        result.validBytes.fill(1);
        CopyMetadata(result, source, options);
        return result;
    }

    template<typename Source>
    void MergeLineImage(LineImage& destination, const Source& source, const ByteEnables& enables,
                        const Options& options)
    {
        for (size_t index = 0; index < LINE_SIZE; ++index)
        {
            if (!enables[index])
                continue;
            destination.data[index] = source.data[index];
            destination.validBytes[index] = 1;
        }
        MergeMetadata(destination.poison, destination.dataCheck, source.poison, source.dataCheck,
                      enables, 0, 0, LINE_SIZE, options);
    }

}
