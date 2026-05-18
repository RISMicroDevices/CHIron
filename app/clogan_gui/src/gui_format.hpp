#pragma once

#include <QString>

#include <cstdint>

namespace CHIron::Gui {

inline QString FormatUnsignedIntegerWithThousands(const std::uint64_t value)
{
    QString digits = QString::number(static_cast<qulonglong>(value));
    for (int index = digits.size() - 3; index > 0; index -= 3) {
        digits.insert(index, QLatin1Char(','));
    }
    return digits;
}

inline QString FormatSignedIntegerWithThousands(const qint64 value)
{
    if (value < 0) {
        const std::uint64_t magnitude =
            static_cast<std::uint64_t>(-(value + 1)) + 1ULL;
        return QStringLiteral("-") + FormatUnsignedIntegerWithThousands(magnitude);
    }
    return FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(value));
}

inline QString FormatTimestampForDisplay(const qint64 timestamp)
{
    return FormatSignedIntegerWithThousands(timestamp);
}

}  // namespace CHIron::Gui
