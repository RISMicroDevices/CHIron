#pragma once

#include <QColor>
#include <QDateTime>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace CHIron::Gui {

struct TraceMarker {
    QString id;
    int logicalRow = -1;
    qint64 timestamp = 0;
    QString label;
    QColor color;
    QString memo;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct MarkerStickyNoteLayout {
    QString markerId;
    double x = 0.0;
    double y = 0.0;
    double width = 160.0;
    double height = 120.0;
};

struct MarkerStickyGroup {
    QString id;
    QString name;
    std::vector<QString> markerIds;
    std::vector<MarkerStickyNoteLayout> noteLayouts;
};

struct MarkerStickyState {
    QString activeGroupId;
    std::vector<MarkerStickyGroup> groups;
};

struct TraceMarkerLoadResult {
    std::vector<TraceMarker> markers;
    std::optional<MarkerStickyState> stickyState;
    int skippedInvalidRows = 0;
};

QColor DefaultTraceMarkerColor(int index);
QString TraceMarkerColorName(const QColor& color);
TraceMarker MakeTraceMarker(int logicalRow,
                            qint64 timestamp,
                            const QString& label,
                            int colorIndex);

bool SaveTraceMarkersToJson(const QString& path,
                            const std::vector<TraceMarker>& markers,
                            const QString& tracePath,
                            const QString& traceLabel,
                            QString& errorText);

bool SaveTraceMarkersToJson(const QString& path,
                            const std::vector<TraceMarker>& markers,
                            const MarkerStickyState& stickyState,
                            const QString& tracePath,
                            const QString& traceLabel,
                            QString& errorText);

bool LoadTraceMarkersFromJson(const QString& path,
                              int totalRows,
                              TraceMarkerLoadResult& result,
                              QString& errorText);

}  // namespace CHIron::Gui
