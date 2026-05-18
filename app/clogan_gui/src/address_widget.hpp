#pragma once

#include "clog_b_trace_loader.hpp"
#include "flit_record.hpp"
#include "trace_session.hpp"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QVariantMap>
#include <QWidget>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QScrollBar;
class QWheelEvent;

namespace CHIron::Gui {

enum class AddressEventKind {
    Read,
    Writeback,
    Snoop
};

class AddressWidget final : public QWidget {
    Q_OBJECT

public:
    explicit AddressWidget(QWidget* parent = nullptr);
    ~AddressWidget() override;

    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setRows(std::vector<FlitRecord> rows);
    void buildView();
    void clear();
    void setSelectedLogicalRow(int logicalRow);
    QVariantMap viewState() const;
    void restoreViewState(const QVariantMap& state);

Q_SIGNALS:
    void logicalRowActivated(int logicalRow);

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
    struct AddressEvent {
        std::uint64_t logicalRow = 0;
        qint64 timestamp = 0;
        std::uint64_t address = 0;
        AddressEventKind kind = AddressEventKind::Read;
        std::uint32_t opcodeLabelIndex = 0;
    };

    struct BuildResult {
        quint64 generation = 0;
        std::uint64_t totalRows = 0;
        std::uint64_t processedRows = 0;
        std::uint64_t minAddress = 0;
        std::uint64_t maxAddress = 0;
        qint64 firstTimestamp = 0;
        qint64 lastTimestamp = 0;
        std::vector<AddressEvent> events;
        std::vector<QString> opcodeLabels;
        std::array<std::uint64_t, 3> kindCounts{};
        QString statusText;
        bool haveAddressRange = false;
        bool haveTimeRange = false;
        bool cancelled = false;
        bool failed = false;
    };

    enum class DragMode {
        None,
        PendingGesture,
        RangeZoom,
        FullRangeZoom,
        ZoomFull,
        ZoomIn2x,
        ZoomOut2x,
        Pan,
        OverviewPan
    };

private:
    struct LegendItem {
        AddressEventKind kind = AddressEventKind::Read;
        QRect rect;
        QRect markerRect;
        QString text;
    };

    struct CursorPosition {
        int logicalRow = -1;
        std::optional<std::uint64_t> address;
    };

    struct DenseEventLayerCache {
        QImage image;
        QSize plotSize;
        const AddressEvent* eventsData = nullptr;
        std::size_t eventCount = 0;
        qint64 firstTimestamp = 0;
        qint64 lastTimestamp = 0;
        qint64 globalFirstTimestamp = 0;
        qint64 globalLastTimestamp = 0;
        std::uint64_t scrollOffset = 0;
        std::uint64_t contentWidth = 0;
        std::uint64_t totalRows = 0;
        std::uint64_t minAddress = 0;
        std::uint64_t maxAddress = 0;
        double horizontalZoom = 1.0;
        double verticalZoom = 1.0;
        long double verticalCenterAddress = 0.0L;
        std::array<bool, 3> kindVisible = {true, true, true};
        bool valid = false;
    };

    void stopBuildThread();
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
    void updateScrollBarGeometry();
    void updateScrollBar();
    std::vector<LegendItem> legendItems() const;
    bool toggleKindAtPosition(const QPoint& position);
    bool isKindVisible(AddressEventKind kind) const noexcept;
    bool eventVisible(const AddressEvent& event) const noexcept;
    std::uint64_t visibleAddressEventCount() const noexcept;
    void resetVerticalView();
    void normalizeVerticalView();

    QRect headerRect() const;
    QRect topRulerRect() const;
    QRect addressAxisRect() const;
    QRect plotRect() const;
    QRect fullRulerRect() const;
    QRect horizontalScrollRect() const;
    int timelineViewportWidth() const;

    std::uint64_t timelineContentWidth() const;
    std::uint64_t horizontalContentRange() const;
    std::uint64_t contentXForTimestamp(qint64 timestamp) const;
    qint64 timestampForContentX(std::uint64_t contentX) const;
    std::uint64_t contentXForLogicalRow(std::uint64_t logicalRow) const;
    std::uint64_t logicalRowForContentX(std::uint64_t contentX) const;
    std::uint64_t contentXAtTimelineX(int x) const;
    std::uint64_t logicalRowAtTimelineX(int x) const;
    std::optional<int> screenXForTimestamp(qint64 timestamp, const QRect& area) const;
    std::optional<int> screenXForLogicalRow(std::uint64_t logicalRow, const QRect& area) const;
    std::optional<int> screenYForAddress(std::uint64_t address, const QRect& area) const;
    std::uint64_t addressForScreenY(int y, const QRect& area) const;
    QRect addressMappingRect(const QRect& area) const;
    std::pair<long double, long double> globalAddressViewRange() const;
    std::pair<long double, long double> visibleAddressRange() const;
    std::uint64_t addressFromVisibleRatio(long double ratio) const;
    std::uint64_t logicalRowForTimestamp(qint64 timestamp) const;
    std::pair<qint64, qint64> visibleTimestampRange() const;
    std::pair<std::uint64_t, std::uint64_t> visibleLogicalRowRange() const;
    QString visibleRangeText() const;
    QString rowLabel(std::uint64_t logicalRow) const;
    std::optional<qint64> timestampForLogicalRow(std::uint64_t logicalRow) const;
    std::optional<std::uint64_t> addressForLogicalRow(std::uint64_t logicalRow) const;

    void zoomToFit();
    void zoomByFactor(double factor, const QPoint& focus);
    void zoomVerticallyByFactor(double factor, const QPoint& focus);
    void zoomToLogicalRange(std::uint64_t firstRow, std::uint64_t lastRow, bool moveCursor);
    void zoomToTimestampRange(qint64 firstTimestamp, qint64 lastTimestamp, bool moveCursor);
    void pushViewHistory();
    void restorePreviousView();
    void scrollToHorizontalOffset(std::uint64_t offset);
    void panHorizontallyByPixels(int pixels);
    void scrollVerticallyByPixels(int pixels);
    void ensureLogicalRowVisible(std::uint64_t logicalRow);
    void ensureAddressVisible(std::uint64_t address);
    void centerSelectedLogicalRow();
    void moveCursorToAdjacentEvent(int direction);
    void setCursorLogicalRow(int logicalRow, bool emitActivation);
    void setCursorPosition(int logicalRow, std::optional<std::uint64_t> address, bool emitActivation);
    void setMarkerLogicalRow(int logicalRow);

    void beginPendingGesture(const QPoint& position);
    void updatePendingGesture(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void updateLeftDragGesture(const QPoint& position, Qt::KeyboardModifiers modifiers);
    DragMode classifyLeftDragGesture(const QPoint& position, Qt::KeyboardModifiers modifiers) const;
    void beginRangeZoom(const QPoint& position);
    void updateRangeZoom(const QPoint& position);
    bool finishRangeZoom();
    void beginFullRangeZoom(const QPoint& position);
    void updateFullRangeZoom(const QPoint& position);
    bool finishFullRangeZoom();
    void beginZoomFullGesture(const QPoint& position);
    bool finishZoomFullGesture();
    void beginZoomIn2xGesture(const QPoint& position);
    bool finishZoomIn2xGesture();
    void beginZoomOut2xGesture(const QPoint& position);
    bool finishZoomOut2xGesture();
    void beginPan(const QPoint& position);
    void updatePan(const QPoint& position);
    void beginOverviewPan(const QPoint& position);
    void updateOverviewPan(const QPoint& position);

    bool positionInTimeline(const QPoint& position) const;
    bool positionInTopRuler(const QPoint& position) const;
    bool positionInFullRuler(const QPoint& position) const;
    bool positionInRuler(const QPoint& position) const;
    QRect overviewWindowRect() const;
    std::optional<int> logicalRowAtPosition(const QPoint& position) const;
    std::optional<CursorPosition> cursorPositionAtPosition(const QPoint& position) const;
    std::optional<std::size_t> nearestEventIndexAtPosition(const QPoint& position) const;
    std::optional<std::size_t> eventTagAtPosition(const QPoint& position) const;
    void updateHoveredEventTag(const QPoint& position);
    QRect cursorAddressTagRect() const;

    void paintHeader(QPainter& painter) const;
    void paintTopRuler(QPainter& painter) const;
    void paintAddressAxis(QPainter& painter) const;
    void paintEvents(QPainter& painter) const;
    bool useDenseEventPaint(qint64 firstTimestamp, qint64 lastTimestamp, const QRect& plot) const;
    void paintDenseEvents(QPainter& painter,
                          const QRect& plot,
                          qint64 firstTimestamp,
                          qint64 lastTimestamp) const;
    void paintEventTags(QPainter& painter) const;
    void paintCursorAndMarker(QPainter& painter) const;
    void paintCursorAddressTag(QPainter& painter) const;
    void paintFullRuler(QPainter& painter) const;
    void paintRangeZoom(QPainter& painter) const;
    void paintGestureOverlay(QPainter& painter) const;
    void paintStatus(QPainter& painter) const;
    void applyViewState(const QVariantMap& state);

#ifdef CHIRON_GUI_ENABLE_ADDRESS_TEST_API
public:
    int testEventCount() const noexcept { return static_cast<int>(events_.size()); }
    int testVisibleEventCount() const noexcept { return static_cast<int>(visibleAddressEventCount()); }
    bool testKindVisible(AddressEventKind kind) const noexcept { return isKindVisible(kind); }
    double testHorizontalZoom() const noexcept { return horizontalZoom_; }
    double testVerticalZoom() const noexcept { return verticalZoom_; }
    std::uint64_t testScrollOffset() const noexcept { return scrollOffset_; }
    int testSelectedLogicalRow() const noexcept { return selectedLogicalRow_; }
    std::optional<std::uint64_t> testSelectedAddress() const { return selectedAddress_; }
    QRect testPlotRect() const { return plotRect(); }
    QRect testAddressAxisRect() const { return addressAxisRect(); }
    QRect testCursorAddressTagRect() const { return cursorAddressTagRect(); }
    QRect testFullRulerRect() const { return fullRulerRect(); }
    QRect testOverviewWindowRect() const { return overviewWindowRect(); }
    void testBeginRangeZoom(const QPoint& position) { beginRangeZoom(position); }
    void testUpdateRangeZoom(const QPoint& position) { updateRangeZoom(position); }
    void testZoomByFactor(double factor, const QPoint& focus) { zoomByFactor(factor, focus); }
    bool testUsesDenseEventPaint() const
    {
        const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
        return useDenseEventPaint(firstTimestamp, lastTimestamp, plotRect());
    }
    std::pair<std::uint64_t, std::uint64_t> testVisibleLogicalRowRange() const
    {
        return visibleLogicalRowRange();
    }
    QString testVisibleRangeText() const { return visibleRangeText(); }
    std::pair<std::uint64_t, std::uint64_t> testVisibleAddressRange() const;
    std::optional<int> testScreenXForLogicalRow(const int logicalRow) const
    {
        if (logicalRow < 0) {
            return std::nullopt;
        }
        return screenXForLogicalRow(static_cast<std::uint64_t>(logicalRow), plotRect());
    }
    std::optional<int> testScreenYForAddress(const std::uint64_t address) const
    {
        return screenYForAddress(address, plotRect());
    }
    std::uint64_t testAddressAtPlotY(const int y) const
    {
        return addressForScreenY(y, plotRect());
    }
    AddressEventKind testKindForLogicalRow(int logicalRow) const;
    std::pair<std::uint64_t, std::uint64_t> testAddressRange() const
    {
        return {minAddress_, maxAddress_};
    }
    QRect testLegendToggleRect(AddressEventKind kind) const;
#endif

private:
    std::shared_ptr<TraceSession> traceSession_;
    std::shared_ptr<const std::vector<FlitRecord>> rowSnapshot_;
    std::vector<AddressEvent> events_;
    std::vector<QString> opcodeLabels_;
    std::array<std::uint64_t, 3> kindCounts_{};
    std::array<bool, 3> kindVisible_ = {true, true, true};
    std::jthread buildThread_;
    std::atomic<quint64> buildGeneration_ = 0;
    std::atomic<std::uint64_t> pendingProgressProcessedRows_ = 0;
    std::atomic<std::uint64_t> pendingProgressTotalRows_ = 0;
    std::atomic<quint64> pendingProgressGeneration_ = 0;
    std::atomic_bool progressUpdateQueued_ = false;

    QScrollBar* horizontalScrollBar_ = nullptr;

    std::uint64_t totalRows_ = 0;
    std::uint64_t processedRows_ = 0;
    std::uint64_t contentWidth_ = 1;
    std::uint64_t scrollOffset_ = 0;
    std::uint64_t minAddress_ = 0;
    std::uint64_t maxAddress_ = 0;
    qint64 firstTimestamp_ = 0;
    qint64 lastTimestamp_ = 0;
    bool haveAddressRange_ = false;
    bool haveTimeRange_ = false;
    double horizontalZoom_ = 1.0;
    double verticalZoom_ = 1.0;
    long double verticalCenterAddress_ = 0.0L;

    int selectedLogicalRow_ = -1;
    std::optional<std::uint64_t> selectedAddress_;
    int markerLogicalRow_ = -1;
    std::optional<std::size_t> hoveredEventTagIndex_;

    DragMode dragMode_ = DragMode::None;
    QPoint dragStart_;
    QPoint dragLast_;
    QPoint leftDragAnchor_;
    std::uint64_t dragStartScrollOffset_ = 0;
    bool leftDragGestureActive_ = false;

    struct ViewState {
        double horizontalZoom = 1.0;
        std::uint64_t horizontalOffset = 0;
        double verticalZoom = 1.0;
        long double verticalCenterAddress = 0.0L;
    };
    std::vector<ViewState> viewHistory_;
    bool snapToEvents_ = true;
    bool lockCursorMarkerDelta_ = false;

    bool building_ = false;
    bool buildRequested_ = false;
    QString statusText_;
    QVariantMap pendingViewState_;
    mutable DenseEventLayerCache denseEventLayerCache_;
};

}  // namespace CHIron::Gui
