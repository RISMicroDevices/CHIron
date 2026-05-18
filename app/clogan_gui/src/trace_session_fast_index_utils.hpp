#pragma once

#include "trace_session.hpp"

#include <QFileInfo>

#include <array>
#include <cstdint>
#include <ios>
#include <vector>

namespace CHIron::Gui::Detail {

inline constexpr std::array<char, 8> kFastIndexMagic = {'C', 'H', 'F', 'I', 'D', 'X', '1', '\0'};
inline constexpr std::uint32_t kFastIndexVersion = 3;
inline constexpr std::size_t kFastRecordSerializedBytes =
    sizeof(std::uint8_t) + sizeof(std::int64_t) + sizeof(std::uint32_t) * 6 + sizeof(std::uint64_t);
inline constexpr std::streamoff kFastIndexHeaderSerializedBytes =
    static_cast<std::streamoff>(sizeof(char) * kFastIndexMagic.size()
        + sizeof(std::uint32_t)
        + sizeof(std::uint64_t)
        + sizeof(std::int64_t)
        + sizeof(std::uint64_t));
inline constexpr std::streamoff kFastIndexDescriptorSerializedBytes =
    static_cast<std::streamoff>(sizeof(std::uint64_t) * 3);

inline std::streamoff fastIndexDescriptorOffset(const std::size_t blockIndex)
{
    return kFastIndexHeaderSerializedBytes
        + static_cast<std::streamoff>(blockIndex) * kFastIndexDescriptorSerializedBytes;
}

inline bool validateFastIndexDescriptors(
    const QString& fastIndexPath,
    const CLogBTraceMetadata& metadata,
    const std::vector<TraceSession::FastIndexDescriptor>& descriptors)
{
    if (descriptors.size() != metadata.blocks.size()) {
        return false;
    }

    const QFileInfo fileInfo(fastIndexPath);
    const std::uint64_t fileSize = fileInfo.exists()
        ? static_cast<std::uint64_t>(fileInfo.size())
        : 0;
    const std::uint64_t descriptorTableEnd = static_cast<std::uint64_t>(
        kFastIndexHeaderSerializedBytes
        + static_cast<std::streamoff>(descriptors.size()) * kFastIndexDescriptorSerializedBytes);

    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& descriptor = descriptors[index];
        const std::uint64_t expectedRecordCount = metadata.blocks[index].recordCount;
        const bool emptyDescriptor = descriptor.dataOffset == 0
            && descriptor.recordCount == 0
            && descriptor.dataBytes == 0;
        if (emptyDescriptor) {
            continue;
        }

        if (descriptor.dataOffset == 0
            || descriptor.recordCount != expectedRecordCount
            || descriptor.dataBytes != descriptor.recordCount * kFastRecordSerializedBytes
            || descriptor.dataOffset < descriptorTableEnd
            || descriptor.dataOffset > fileSize
            || descriptor.dataBytes > fileSize
            || descriptor.dataOffset + descriptor.dataBytes > fileSize) {
            return false;
        }
    }

    return true;
}

}  // namespace CHIron::Gui::Detail
