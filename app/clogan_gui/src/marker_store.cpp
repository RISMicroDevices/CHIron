#include "marker_store.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <array>
#include <algorithm>

namespace CHIron::Gui {
namespace {

constexpr int kMarkerJsonVersion = 1;

const std::array<const char*, 8>& markerPalette()
{
    static constexpr std::array<const char*, 8> colors{{
        "#2F80ED",
        "#00A878",
        "#F2994A",
        "#EB5757",
        "#9B51E0",
        "#0EADBD",
        "#C48A00",
        "#4F6BED",
    }};
    return colors;
}

QString markerDateTimeToString(const QDateTime& value)
{
    return value.isValid() ? value.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QDateTime markerDateTimeFromString(const QString& value)
{
    const QDateTime parsed = QDateTime::fromString(value, Qt::ISODateWithMs);
    return parsed.isValid() ? parsed.toUTC() : QDateTime::currentDateTimeUtc();
}

QJsonObject markerToJson(const TraceMarker& marker)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), marker.id);
    object.insert(QStringLiteral("logicalRow"), marker.logicalRow);
    object.insert(QStringLiteral("timestamp"), QString::number(marker.timestamp));
    object.insert(QStringLiteral("label"), marker.label);
    object.insert(QStringLiteral("color"), TraceMarkerColorName(marker.color));
    object.insert(QStringLiteral("memo"), marker.memo);
    object.insert(QStringLiteral("createdAt"), markerDateTimeToString(marker.createdAt));
    object.insert(QStringLiteral("updatedAt"), markerDateTimeToString(marker.updatedAt));
    return object;
}

QJsonObject stickyNoteToJson(const MarkerStickyNoteLayout& layout)
{
    QJsonObject object;
    object.insert(QStringLiteral("markerId"), layout.markerId);
    object.insert(QStringLiteral("x"), layout.x);
    object.insert(QStringLiteral("y"), layout.y);
    object.insert(QStringLiteral("width"), layout.width);
    object.insert(QStringLiteral("height"), layout.height);
    return object;
}

QJsonObject stickyGroupToJson(const MarkerStickyGroup& group)
{
    QJsonArray markerArray;
    for (const QString& markerId : group.markerIds) {
        markerArray.push_back(markerId);
    }

    QJsonArray layoutArray;
    for (const MarkerStickyNoteLayout& layout : group.noteLayouts) {
        layoutArray.push_back(stickyNoteToJson(layout));
    }

    QJsonObject object;
    object.insert(QStringLiteral("id"), group.id);
    object.insert(QStringLiteral("name"), group.name);
    object.insert(QStringLiteral("markerIds"), markerArray);
    object.insert(QStringLiteral("noteLayouts"), layoutArray);
    return object;
}

QJsonObject stickyStateToJson(const MarkerStickyState& state)
{
    QJsonArray groupArray;
    for (const MarkerStickyGroup& group : state.groups) {
        groupArray.push_back(stickyGroupToJson(group));
    }

    QJsonObject object;
    object.insert(QStringLiteral("activeGroupId"), state.activeGroupId);
    object.insert(QStringLiteral("groups"), groupArray);
    return object;
}

bool markerFromJson(const QJsonObject& object, const int totalRows, TraceMarker& marker)
{
    const int logicalRow = object.value(QStringLiteral("logicalRow")).toInt(-1);
    if (logicalRow < 0 || (totalRows >= 0 && logicalRow >= totalRows)) {
        return false;
    }

    marker.id = object.value(QStringLiteral("id")).toString();
    if (marker.id.isEmpty()) {
        marker.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    marker.logicalRow = logicalRow;

    bool timestampOk = false;
    const QString timestampText = object.value(QStringLiteral("timestamp")).toVariant().toString();
    const qlonglong parsedTimestamp = timestampText.toLongLong(&timestampOk);
    marker.timestamp = timestampOk
        ? static_cast<qint64>(parsedTimestamp)
        : static_cast<qint64>(object.value(QStringLiteral("timestamp")).toDouble(0.0));

    marker.label = object.value(QStringLiteral("label")).toString();
    if (marker.label.isEmpty()) {
        marker.label = QStringLiteral("Marker %1").arg(logicalRow + 1);
    }
    marker.color = QColor(object.value(QStringLiteral("color")).toString());
    if (!marker.color.isValid()) {
        marker.color = DefaultTraceMarkerColor(logicalRow);
    }
    marker.memo = object.value(QStringLiteral("memo")).toString();
    marker.createdAt = markerDateTimeFromString(object.value(QStringLiteral("createdAt")).toString());
    marker.updatedAt = markerDateTimeFromString(object.value(QStringLiteral("updatedAt")).toString());
    return true;
}

MarkerStickyNoteLayout stickyNoteFromJson(const QJsonObject& object)
{
    MarkerStickyNoteLayout layout;
    layout.markerId = object.value(QStringLiteral("markerId")).toString();
    layout.x = object.value(QStringLiteral("x")).toDouble(0.0);
    layout.y = object.value(QStringLiteral("y")).toDouble(0.0);
    layout.width = std::max(80.0, object.value(QStringLiteral("width")).toDouble(160.0));
    layout.height = std::max(72.0, object.value(QStringLiteral("height")).toDouble(120.0));
    return layout;
}

std::optional<MarkerStickyState> stickyStateFromJson(const QJsonObject& root,
                                                     const QSet<QString>& validMarkerIds)
{
    const QJsonValue stickyValue = root.value(QStringLiteral("sticky"));
    if (!stickyValue.isObject()) {
        return std::nullopt;
    }

    MarkerStickyState state;
    const QJsonObject stickyObject = stickyValue.toObject();
    state.activeGroupId = stickyObject.value(QStringLiteral("activeGroupId")).toString();

    const QJsonArray groupArray = stickyObject.value(QStringLiteral("groups")).toArray();
    state.groups.reserve(static_cast<std::size_t>(groupArray.size()));
    for (const QJsonValue& value : groupArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject groupObject = value.toObject();
        MarkerStickyGroup group;
        group.id = groupObject.value(QStringLiteral("id")).toString();
        if (group.id.isEmpty()) {
            group.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        group.name = groupObject.value(QStringLiteral("name")).toString();
        if (group.name.trimmed().isEmpty()) {
            group.name = QStringLiteral("Group %1").arg(state.groups.size() + 1);
        }

        QSet<QString> groupMarkerIds;
        const QJsonArray markerArray = groupObject.value(QStringLiteral("markerIds")).toArray();
        for (const QJsonValue& markerValue : markerArray) {
            const QString markerId = markerValue.toString();
            if (!markerId.isEmpty() && validMarkerIds.contains(markerId) && !groupMarkerIds.contains(markerId)) {
                group.markerIds.push_back(markerId);
                groupMarkerIds.insert(markerId);
            }
        }

        const QJsonArray layoutArray = groupObject.value(QStringLiteral("noteLayouts")).toArray();
        for (const QJsonValue& layoutValue : layoutArray) {
            if (!layoutValue.isObject()) {
                continue;
            }
            MarkerStickyNoteLayout layout = stickyNoteFromJson(layoutValue.toObject());
            if (!layout.markerId.isEmpty() && validMarkerIds.contains(layout.markerId)) {
                group.noteLayouts.push_back(std::move(layout));
            }
        }
        state.groups.push_back(std::move(group));
    }

    if (!state.activeGroupId.isEmpty()) {
        const auto found = std::find_if(state.groups.cbegin(), state.groups.cend(), [&](const MarkerStickyGroup& group) {
            return group.id == state.activeGroupId;
        });
        if (found == state.groups.cend()) {
            state.activeGroupId.clear();
        }
    }
    return state;
}

}  // namespace

QColor DefaultTraceMarkerColor(const int index)
{
    const auto& palette = markerPalette();
    const int colorIndex = index < 0 ? 0 : index % static_cast<int>(palette.size());
    return QColor(QString::fromLatin1(palette[static_cast<std::size_t>(colorIndex)]));
}

QString TraceMarkerColorName(const QColor& color)
{
    return color.isValid() ? color.name(QColor::HexRgb) : DefaultTraceMarkerColor(0).name(QColor::HexRgb);
}

TraceMarker MakeTraceMarker(const int logicalRow,
                            const qint64 timestamp,
                            const QString& label,
                            const int colorIndex)
{
    TraceMarker marker;
    marker.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    marker.logicalRow = logicalRow;
    marker.timestamp = timestamp;
    marker.label = label.isEmpty()
        ? QStringLiteral("Marker %1").arg(logicalRow + 1)
        : label;
    marker.color = DefaultTraceMarkerColor(colorIndex);
    marker.createdAt = QDateTime::currentDateTimeUtc();
    marker.updatedAt = marker.createdAt;
    return marker;
}

bool SaveTraceMarkersToJson(const QString& path,
                            const std::vector<TraceMarker>& markers,
                            const QString& tracePath,
                            const QString& traceLabel,
                            QString& errorText)
{
    return SaveTraceMarkersToJson(path, markers, MarkerStickyState{}, tracePath, traceLabel, errorText);
}

bool SaveTraceMarkersToJson(const QString& path,
                            const std::vector<TraceMarker>& markers,
                            const MarkerStickyState& stickyState,
                            const QString& tracePath,
                            const QString& traceLabel,
                            QString& errorText)
{
    errorText.clear();
    QJsonArray markerArray;
    for (const TraceMarker& marker : markers) {
        markerArray.push_back(markerToJson(marker));
    }

    QJsonObject root;
    root.insert(QStringLiteral("app"), QStringLiteral("CloganGUI"));
    root.insert(QStringLiteral("kind"), QStringLiteral("trace-markers"));
    root.insert(QStringLiteral("version"), kMarkerJsonVersion);
    root.insert(QStringLiteral("tracePath"), tracePath);
    root.insert(QStringLiteral("traceLabel"), traceLabel);
    root.insert(QStringLiteral("savedAt"), markerDateTimeToString(QDateTime::currentDateTimeUtc()));
    root.insert(QStringLiteral("markers"), markerArray);
    root.insert(QStringLiteral("sticky"), stickyStateToJson(stickyState));

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorText = output.errorString();
        return false;
    }
    output.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!output.commit()) {
        errorText = output.errorString();
        return false;
    }
    return true;
}

bool LoadTraceMarkersFromJson(const QString& path,
                              const int totalRows,
                              TraceMarkerLoadResult& result,
                              QString& errorText)
{
    errorText.clear();
    result = {};

    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) {
        errorText = input.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorText = parseError.errorString();
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonArray markerArray = root.value(QStringLiteral("markers")).toArray();
    result.markers.reserve(static_cast<std::size_t>(markerArray.size()));
    for (const QJsonValue& value : markerArray) {
        if (!value.isObject()) {
            ++result.skippedInvalidRows;
            continue;
        }
        TraceMarker marker;
        if (!markerFromJson(value.toObject(), totalRows, marker)) {
            ++result.skippedInvalidRows;
            continue;
        }
        result.markers.push_back(std::move(marker));
    }
    QSet<QString> markerIds;
    for (const TraceMarker& marker : result.markers) {
        markerIds.insert(marker.id);
    }
    result.stickyState = stickyStateFromJson(root, markerIds);
    return true;
}

}  // namespace CHIron::Gui
