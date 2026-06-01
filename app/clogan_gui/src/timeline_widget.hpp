#pragma once

#include "clog_b_trace_loader.hpp"
#include "flit_record.hpp"
#include "marker_store.hpp"
#include "trace_session.hpp"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

#include <cstdint>
#include <memory>
#include <optional>
#include <atomic>
#include <stop_token>
#include <thread>
#include <vector>

class QKeyEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QScrollBar;
class QWheelEvent;

namespace CHIron::Gui {

struct TimelineLane {
    QString nodeIdLabel;
    QString nodeTypeLabel;
    QColor nodeTypeColor;
    QString channelLabel;
    FlitChannel channel = FlitChannel::Req;
    FlitDirection direction = FlitDirection::Tx;
    std::uint32_t nodeId = 0;
    std::uint64_t count = 0;
};

enum class TimelineLaneSortOrder {
    NodeThenChannel,
    ChannelThenNode,
    Custom
};

class TimelineWidget final : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    ~TimelineWidget() override;

    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setRows(std::vector<FlitRecord> rows);
    void buildView();
    void clear();
    void setSelectedLogicalRow(int logicalRow);
    void setSelectedRow(FlitDirection direction,
                        FlitChannel channel,
                        std::optional<std::uint32_t> nodeId);
    void clearSelectedRow();
    void setMarkers(std::vector<TraceMarker> markers, const QString& selectedMarkerId);
    QVariantMap viewState() const;
    void restoreViewState(const QVariantMap& state);

Q_SIGNALS:
    void logicalRowActivated(int logicalRow);
    void markerSelected(QString markerId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

public:
    struct TimelineEvent {
        std::uint64_t logicalRow = 0;
        qint64 timestamp = 0;
        int lane = -1;
        std::uint32_t opcodeLabelIndex = 0;
    };

    struct LaneSelector {
        FlitDirection direction = FlitDirection::Tx;
        FlitChannel channel = FlitChannel::Req;
        std::optional<std::uint32_t> nodeId;
    };

    struct BuildResult {
        quint64 generation = 0;
        std::uint64_t totalRows = 0;
        std::uint64_t processedRows = 0;
        std::vector<TimelineLane> lanes;
        std::vector<TimelineEvent> events;
        std::vector<QString> opcodeLabels;
        std::vector<std::vector<std::uint64_t>> laneRows;
        std::vector<qint64> rowTimestamps;
        qint64 firstTimestamp = 0;
        qint64 lastTimestamp = 0;
        bool hasTimestampRange = false;
        bool rowTimestampsMonotonic = true;
        QString statusText;
        bool cancelled = false;
        bool failed = false;
    };

    enum class DragMode {
        None,
        PendingGesture,
        ColumnResize,
        RangeZoom,
        FullRangeZoom,
        ZoomFull,
        ZoomIn2x,
        ZoomOut2x,
        Pan,
        OverviewPan,
        LaneReorder
    };

    enum class ResizeColumn {
        None,
        Node,
        Channel
    };

    struct WaveformCacheKey {
        quint64 dataGeneration = 0;
        int width = 0;
        int height = 0;
        int firstLane = 0;
        int lastLane = -1;
        int verticalScrollOffset = 0;
        std::uint64_t totalRows = 0;
        std::uint64_t contentWidth = 0;
        std::uint64_t scrollOffset = 0;
        std::uint64_t firstRow = 0;
        std::uint64_t lastRow = 0;
        qint64 firstTimestamp = 0;
        qint64 lastTimestamp = 0;

        bool operator==(const WaveformCacheKey& other) const noexcept
        {
            return dataGeneration == other.dataGeneration
                && width == other.width
                && height == other.height
                && firstLane == other.firstLane
                && lastLane == other.lastLane
                && verticalScrollOffset == other.verticalScrollOffset
                && totalRows == other.totalRows
                && contentWidth == other.contentWidth
                && scrollOffset == other.scrollOffset
                && firstRow == other.firstRow
                && lastRow == other.lastRow
                && firstTimestamp == other.firstTimestamp
                && lastTimestamp == other.lastTimestamp;
        }

        bool operator!=(const WaveformCacheKey& other) const noexcept
        {
            return !(*this == other);
        }
    };

    struct WaveformRenderResult {
        quint64 renderGeneration = 0;
        WaveformCacheKey key;
        QImage image;
        bool cancelled = false;
    };

    struct OpcodeTagPlacement {
        std::size_t eventIndex = 0;
        QRect rect;
        bool overlapsLaneFlits = false;
    };

private:
    void stopBuildThread();
    void stopWaveformRender();
    void invalidateWaveformCache();
    void requestWaveformRerender();
    void startRowsBuild(std::shared_ptr<const std::vector<FlitRecord>> rows, bool synchronous);
    void startSessionBuild(std::shared_ptr<TraceSession> session);
    void applyBuildResult(std::shared_ptr<BuildResult> result);
    void postBuildProgress(quint64 generation,
                           std::uint64_t processedRows,
                           std::uint64_t totalRows);
    void updateBuildProgress(quint64 generation,
                             std::uint64_t processedRows,
                             std::uint64_t totalRows);
    bool hasPendingBuild() const noexcept;
    bool shouldShowBuildPrompt() const noexcept;
    QString pendingBuildText() const;
    QRect buildPromptButtonRect() const;
    void paintBuildPrompt(QPainter& painter) const;
    void paintBuildProgress(QPainter& painter, const QRect& bounds) const;

    void rebuildContentMetrics();
    void updateScrollBars();
    void updateScrollBarGeometry();
    bool updateAutoColumnWidths();
    int requiredNodeColumnWidth() const;
    int requiredChannelColumnWidth() const;
    void setColumnWidth(ResizeColumn column, int width, bool userSized);
    ResizeColumn columnResizeHandleAt(const QPoint& position) const;
    void updateResizeCursor(const QPoint& position);

    QRect headerRect() const;
    QRect topRulerRect() const;
    QRect plotRect() const;
    QRect fullRulerRect() const;
    QRect horizontalScrollRect() const;
    QRect verticalScrollRect() const;
    int timelineLeft() const;
    int timelineViewportWidth() const;
    int laneStride() const;
    int timelineContentHeight() const;
    QRect nodeHeaderRect() const;
    QRect channelHeaderRect() const;

    std::uint64_t timelineContentWidth() const;
    std::uint64_t horizontalContentRange() const;
    std::uint64_t contentXForTimestamp(qint64 timestamp) const;
    qint64 timestampForContentX(std::uint64_t contentX) const;
    std::uint64_t contentXForLogicalRow(std::uint64_t logicalRow) const;
    std::uint64_t logicalRowForTimestamp(qint64 timestamp) const;
    std::uint64_t firstLogicalRowAtOrAfterTimestamp(qint64 timestamp) const;
    std::uint64_t lastLogicalRowAtOrBeforeTimestamp(qint64 timestamp) const;
    std::uint64_t logicalRowForContentX(std::uint64_t contentX) const;
    std::uint64_t contentXAtTimelineX(int x) const;
    qint64 timestampAtTimelineX(int x) const;
    qint64 timestampAtFullRulerX(int x) const;
    std::uint64_t logicalRowAtTimelineX(int x) const;
    std::optional<int> screenXForTimestamp(qint64 timestamp, const QRect& area) const;
    std::optional<int> screenXForLogicalRow(std::uint64_t logicalRow, const QRect& area) const;
    std::pair<qint64, qint64> visibleTimeRange() const;
    std::pair<std::uint64_t, std::uint64_t> visibleLogicalRowRange() const;
    QString visibleRangeText() const;
    QString rowLabel(std::uint64_t logicalRow) const;
    std::optional<qint64> timestampForLogicalRow(std::uint64_t logicalRow) const;

    void zoomToFit();
    void zoomByFactor(double factor, const QPoint& focus);
    void zoomToTimeRange(qint64 firstTimestamp, qint64 lastTimestamp, bool moveCursor);
    void zoomToLogicalRange(std::uint64_t firstRow, std::uint64_t lastRow, bool moveCursor);
    bool restoreVisibleTimeRange(qint64 firstTimestamp, qint64 lastTimestamp);
    bool restoreVisibleLogicalRange(std::uint64_t firstRow, std::uint64_t lastRow);
    void pushViewHistory();
    void restorePreviousView();
    void scrollToHorizontalOffset(std::uint64_t offset);
    void panHorizontallyByPixels(int pixels);
    void panVerticallyByPixels(int pixels);
    void ensureLogicalRowVisible(std::uint64_t logicalRow);
    void centerSelectedLogicalRow();
    void moveCursorToAdjacentEvent(int direction);
    void setCursorLogicalRow(int logicalRow, bool emitActivation);
    void setMarkerLogicalRow(int logicalRow);
    std::optional<QString> sharedMarkerAtPosition(const QPoint& position) const;
    void updateLaneSelectionFromFilter();
    void resetCustomLaneSortOrder();
    void setLaneSortOrder(TimelineLaneSortOrder order);
    void applyLaneSortOrder();
    void moveLane(int sourceIndex, int targetIndex);
    std::optional<LaneSelector> laneSelectorForIndex(int laneIndex) const;
    static QString laneSelectorStateKey(const LaneSelector& selector);
    QStringList laneStateOrder() const;
    void updateHoveredOpcodeTag(const QPoint& position);
    std::optional<std::size_t> opcodeTagAtPosition(const QPoint& position) const;

    void beginLaneReorder(const QPoint& position);
    void updateLaneReorder(const QPoint& position);
    void finishLaneReorder(const QPoint& position);
    void cancelLaneReorder();
    void beginPendingGesture(const QPoint& position);
    void updatePendingGesture(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void updateLeftDragGesture(const QPoint& position, Qt::KeyboardModifiers modifiers);
    DragMode classifyLeftDragGesture(const QPoint& position, Qt::KeyboardModifiers modifiers) const;
    void beginRangeZoom(const QPoint& position);
    void updateRangeZoom(const QPoint& position);
    bool finishRangeZoom();
    void beginZoomFullGesture(const QPoint& position);
    bool finishZoomFullGesture();
    void beginZoomIn2xGesture(const QPoint& position);
    bool finishZoomIn2xGesture();
    void beginZoomOut2xGesture(const QPoint& position);
    bool finishZoomOut2xGesture();
    void beginFullRangeZoom(const QPoint& position);
    void updateFullRangeZoom(const QPoint& position);
    bool finishFullRangeZoom();
    void beginPan(const QPoint& position);
    void updatePan(const QPoint& position);
    void beginOverviewPan(const QPoint& position);
    void updateOverviewPan(const QPoint& position);

    bool positionInTimeline(const QPoint& position) const;
    bool positionInLaneTable(const QPoint& position) const;
    bool positionInTopRuler(const QPoint& position) const;
    bool positionInFullRuler(const QPoint& position) const;
    bool positionInRuler(const QPoint& position) const;
    QRect overviewWindowRect() const;
    std::optional<int> laneAtPosition(const QPoint& position) const;
    int laneReorderTargetIndex(const QPoint& position) const;
    std::optional<int> logicalRowAtPosition(const QPoint& position) const;
    std::optional<int> nearestEventRow(qint64 timestamp, int lane) const;
    int laneIndexFor(const LaneSelector& selector) const;
    WaveformCacheKey currentWaveformCacheKey(const QRect& plot) const;
    void scheduleWaveformRender(const WaveformCacheKey& key);
    void applyWaveformRenderResult(std::shared_ptr<WaveformRenderResult> result);
    void applyViewState(const QVariantMap& state);

    void paintHeader(QPainter& painter) const;
    void paintTopRuler(QPainter& painter) const;
    void paintFullRuler(QPainter& painter) const;
    void paintLaneTable(QPainter& painter) const;
    void paintWaveforms(QPainter& painter);
    void paintOpcodeTags(QPainter& painter, const QRect& plot, int firstLane, int lastLane) const;
    std::vector<OpcodeTagPlacement> visibleOpcodeTagPlacements(const QFontMetrics& metrics,
                                                               const QRect& plot,
                                                               int firstLane,
                                                               int lastLane) const;
    bool opcodeTagOverlapsLaneFlits(const QRect& tagRect,
                                    int laneIndex,
                                    std::uint64_t ownerRow,
                                    const QRect& plot) const;
    void paintCursorAndMarker(QPainter& painter) const;
    void paintSharedMarkers(QPainter& painter) const;
    void paintRangeZoom(QPainter& painter) const;
    void paintGestureOverlay(QPainter& painter) const;
    void paintStatus(QPainter& painter) const;

#ifdef CHIRON_GUI_ENABLE_TIMELINE_TEST_API
public:
    double testHorizontalZoom() const noexcept { return horizontalZoom_; }
    std::uint64_t testScrollOffset() const noexcept { return scrollOffset_; }
    int testSelectedLogicalRow() const noexcept { return selectedLogicalRow_; }
    int testMarkerLogicalRow() const noexcept { return markerLogicalRow_; }
    int testSelectedLane() const noexcept { return selectedLane_; }
    int testVerticalScrollOffset() const noexcept { return verticalScrollOffset_; }
    QRect testPlotRect() const { return plotRect(); }
    std::pair<std::uint64_t, std::uint64_t> testVisibleLogicalRowRange() const
    {
        return visibleLogicalRowRange();
    }
    std::optional<int> testScreenXForLogicalRow(const int logicalRow) const
    {
        if (logicalRow < 0) {
            return std::nullopt;
        }
        return screenXForLogicalRow(static_cast<std::uint64_t>(logicalRow), plotRect());
    }
    QString testVisibleRangeText() const { return visibleRangeText(); }
    QString testOpcodeLabelForLogicalRow(int logicalRow) const;
    int testVisibleOpcodeTagCount() const;
    bool testVisibleOpcodeTagsOverlapFlits() const;
    TimelineLaneSortOrder testLaneSortOrder() const noexcept { return laneSortOrder_; }
    QRect testNodeHeaderRect() const { return nodeHeaderRect(); }
    QRect testChannelHeaderRect() const { return channelHeaderRect(); }
    QPoint testLaneTablePoint(int laneIndex) const;
    QString testLaneKey(int laneIndex) const;
    QStringList testLaneStateOrder() const { return laneStateOrder(); }
    void testMoveLane(int sourceIndex, int targetIndex) { moveLane(sourceIndex, targetIndex); }
#endif

private:
    std::shared_ptr<TraceSession> traceSession_;
    std::shared_ptr<const std::vector<FlitRecord>> rowSnapshot_;
    std::vector<TimelineLane> lanes_;
    std::vector<TimelineEvent> events_;
    std::vector<QString> opcodeLabels_;
    std::shared_ptr<const std::vector<std::vector<std::uint64_t>>> laneRows_;
    std::shared_ptr<const std::vector<qint64>> rowTimestamps_;
    bool rowTimestampsMonotonic_ = true;
    std::jthread buildThread_;
    std::atomic<quint64> buildGeneration_ = 0;
    std::atomic<std::uint64_t> pendingProgressProcessedRows_ = 0;
    std::atomic<std::uint64_t> pendingProgressTotalRows_ = 0;
    std::atomic<quint64> pendingProgressGeneration_ = 0;
    std::atomic_bool progressUpdateQueued_ = false;
    std::jthread waveformRenderThread_;
    std::atomic<quint64> waveformRenderGeneration_ = 0;

    QScrollBar* horizontalScrollBar_ = nullptr;
    QScrollBar* verticalScrollBar_ = nullptr;

    std::uint64_t totalRows_ = 0;
    std::uint64_t processedRows_ = 0;
    std::uint64_t contentWidth_ = 1;
    int contentHeight_ = 1;
    std::uint64_t scrollOffset_ = 0;
    int verticalScrollOffset_ = 0;
    double horizontalZoom_ = 1.0;
    qint64 firstTimestamp_ = 0;
    qint64 lastTimestamp_ = 0;
    bool hasTimestampRange_ = false;

    int nodeColumnWidth_ = 92;
    int channelColumnWidth_ = 86;
    bool nodeColumnUserSized_ = false;
    bool channelColumnUserSized_ = false;

    int selectedLogicalRow_ = -1;
    int markerLogicalRow_ = -1;
    int selectedLane_ = -1;
    int highlightedLane_ = -1;
    TimelineLaneSortOrder laneSortOrder_ = TimelineLaneSortOrder::NodeThenChannel;
    std::optional<std::size_t> hoveredOpcodeTagEventIndex_;
    std::optional<LaneSelector> selectedLaneFilter_;
    std::vector<TraceMarker> sharedMarkers_;
    QString selectedMarkerId_;
    QString hoveredMarkerId_;

    DragMode dragMode_ = DragMode::None;
    ResizeColumn resizeColumn_ = ResizeColumn::None;
    QPoint dragStart_;
    QPoint dragLast_;
    QPoint leftDragAnchor_;
    int resizeStartNodeColumnWidth_ = 0;
    int resizeStartChannelColumnWidth_ = 0;
    std::uint64_t dragStartScrollOffset_ = 0;
    int dragStartVerticalOffset_ = 0;
    bool leftDragGestureActive_ = false;
    int laneDragSourceIndex_ = -1;
    int laneDragTargetIndex_ = -1;
    bool laneDragActive_ = false;

    struct ViewState {
        double zoom = 1.0;
        std::uint64_t offset = 0;
    };
    std::vector<ViewState> viewHistory_;
    bool snapToEvents_ = true;
    bool lockCursorMarkerDelta_ = false;

    WaveformCacheKey waveformCacheKey_;
    QImage waveformCacheImage_;
    bool waveformCacheValid_ = false;
    WaveformCacheKey waveformPendingKey_;
    bool waveformRenderPending_ = false;

    bool building_ = false;
    bool buildRequested_ = false;
    QString statusText_;
    QVariantMap pendingViewState_;
};

}  // namespace CHIron::Gui
