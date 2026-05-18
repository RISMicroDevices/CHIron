#pragma once

#include "clog_b_trace_loader.hpp"
#include "flit_record.hpp"
#include "trace_session.hpp"

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVariantMap>
#include <QWidget>

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

class CacheWidget final : public QWidget {
    Q_OBJECT

public:
    explicit CacheWidget(QWidget* parent = nullptr);
    ~CacheWidget() override;

    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setRows(std::vector<FlitRecord> rows);
    void buildView();
    void clear();
    void setSelectedLogicalRow(int logicalRow);
    QVariantMap viewState() const;
    void restoreViewState(const QVariantMap& state);
    QVariantMap diagnosticsState() const;

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
    struct CacheRnBand {
        std::uint32_t rnNodeId = 0;
        QString topologyLabel;
        QString nodeType;
        std::uint64_t spanCount = 0;
    };

    struct CacheLifetimeSpan {
        int rnBandIndex = -1;
        std::uint32_t rnNodeId = 0;
        std::uint64_t lineAddress = 0;
        qint64 startTimestamp = 0;
        qint64 endTimestamp = 0;
        std::uint64_t startLogicalRow = 0;
        std::uint64_t endLogicalRow = 0;
        std::uint8_t startStateMask = 0;
        std::uint8_t endStateMask = 0;
        QString startStateText;
        QString endStateText;
        bool openEnded = false;
    };

#ifdef CHIRON_GUI_ENABLE_CACHE_TEST_API
public:
    int testSpanCount() const noexcept;
    int testBandCount() const noexcept;
    int testVisibleBandCount() const noexcept;
    QVariantMap testSpanAt(int index) const;
    QVariantMap testBandAt(int index) const;
    QString testStatusText() const;
    QVariantMap testLayoutState() const;
    std::pair<qint64, qint64> testVisibleTimestampRange() const;
    double testHorizontalZoom() const noexcept;
    double testVerticalBandScale() const noexcept;
    double testVerticalAddressZoom() const noexcept;
    bool testUsesDensePaint() const noexcept;
    int testDenseTileCacheEntryCount() const noexcept;
    int testSelectedLogicalRow() const noexcept;
    int testSelectedSpanIndex() const noexcept;
    bool testHasCursor() const noexcept;
    qint64 testCursorTimestamp() const noexcept;
    bool testHasMarker() const noexcept;
    int testMarkerLogicalRow() const noexcept;
    QRect testPlotRect() const;
    QRect testBuildPromptButtonRect() const;
    QRect testAddBandButtonRect() const;
    QRect testBandDeleteButtonRect(int visibleBandIndex) const;
    QRect testSpanVisualRect(int spanIndex) const;
    int testSpanIndexAtPoint(const QPoint& position) const;
    int testVisiblePaintCandidateCount() const;
    bool testGestureOverlayVisible() const noexcept;
    QString testDragModeName() const;
    quint64 testBuildGeneration() const noexcept;
    void testInjectSyntheticSpans(int bandCount, int spanCount);
    bool testSetTraceSessionAndWaitForSpans(std::shared_ptr<TraceSession> session, int timeoutMs);
    QVariantMap testBuildSpansFromTraceSession(std::shared_ptr<TraceSession> session);
    void testSetHorizontalZoom(double zoom);
    void testSetHorizontalScroll(int value);
    void testSetVerticalScroll(int value);
    void testSetVerticalZoom(double bandScale, double addressZoom);
    bool testAddVisibleBand(int sourceBandIndex);
    bool testRemoveVisibleBandAt(int visibleBandIndex);
#endif

private:
    enum class SourceMode {
        Empty,
        TraceSession,
        RowSnapshot
    };

    enum class DragMode {
        None,
        PendingGesture,
        RangeZoom,
        ZoomFull,
        ZoomIn2x,
        ZoomOut2x,
        Pan,
        OverviewPan
    };

    struct BuildResult {
        quint64 generation = 0;
        std::uint64_t totalRows = 0;
        std::uint64_t processedRows = 0;
        std::uint64_t minAddress = 0;
        std::uint64_t maxAddress = 0;
        qint64 firstTimestamp = 0;
        qint64 lastTimestamp = 0;
        std::vector<CacheRnBand> bands;
        std::vector<CacheLifetimeSpan> spans;
        QString statusText;
        bool haveAddressRange = false;
        bool haveTimeRange = false;
        bool cancelled = false;
        bool failed = false;
    };

    struct ViewHistoryEntry {
        double horizontalZoom = 1.0;
        std::uint64_t scrollOffset = 0;
        int verticalScrollOffset = 0;
        double verticalBandScale = 1.0;
        double verticalAddressZoom = 1.0;
        long double verticalCenterAddress = 0.0L;
        bool hasCursor = false;
        qint64 cursorTimestamp = 0;
    };

    struct DenseSpanTileKey {
        quint64 generation = 0;
        int sourceBandIndex = -1;
        std::uint64_t tileContentLeft = 0;
        int tileWidth = 0;
        int bandHeight = 0;
        std::uint64_t contentWidth = 0;
        qint64 firstTimestamp = 0;
        qint64 lastTimestamp = 0;
        std::uint64_t minAddress = 0;
        std::uint64_t maxAddress = 0;
        std::uint64_t visibleMinAddress = 0;
        std::uint64_t visibleMaxAddress = 0;
        qint64 visibleStartTimestamp = 0;
        qint64 visibleEndTimestamp = 0;
        int addressRowHeight = 0;
        bool denseAddressMode = false;
        double horizontalZoom = 1.0;
        double verticalBandScale = 1.0;
        double verticalAddressZoom = 1.0;
        long double verticalCenterAddress = 0.0L;
        qreal devicePixelRatio = 1.0;

        bool operator==(const DenseSpanTileKey& other) const noexcept;
    };

    struct DenseSpanTileCacheEntry {
        DenseSpanTileKey key;
        QImage image;
        bool hasContent = false;
        quint64 lastUsed = 0;
    };

    struct BoardLayout {
        QRect headerRect;
        QRect topRulerRect;
        QRect railRect;
        QRect plotRect;
        QRect bottomRulerRect;
        QRect horizontalScrollRect;
        QRect verticalScrollRect;
    };

    struct VisibleAddressRow {
        std::uint64_t address = 0;
        QRect railRect;
        QRect plotRect;
        int spanCount = 0;
    };

    struct VisibleBandRows {
        bool denseMode = false;
        int totalAddressRows = 0;
        std::vector<VisibleAddressRow> rows;
    };

    void stopBuildThread();
    void startSessionBuild(std::shared_ptr<TraceSession> session);
    std::shared_ptr<BuildResult> buildSpansFromSession(const std::shared_ptr<TraceSession>& session,
                                                       quint64 generation,
                                                       std::stop_token stopToken);
    static void finalizeBuildResult(BuildResult& result,
                                    const CLogBTraceMetadata& metadata,
                                    std::vector<CLogBTraceCacheLineLifetime> lifetimes);
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

    void rebuildSpanIndexes();
    void updateScrollBars();
    void updateScrollBarGeometry();
    void normalizeView();
    void resetView();

    BoardLayout boardLayout() const;
    QRect headerRect() const;
    QRect topRulerRect() const;
    QRect addressAxisRect() const;
    QRect plotRect() const;
    QRect fullRulerRect() const;
    QRect horizontalScrollRect() const;
    QRect verticalScrollRect() const;
    QRect addBandButtonRect() const;
    QRect bandDeleteButtonRect(int visibleBandIndex) const;
    int timelineViewportWidth() const;
    int timelineViewportHeight() const;

    std::uint64_t timelineContentWidth() const;
    std::uint64_t horizontalContentRange() const;
    int verticalContentHeight() const;
    int verticalContentRange() const;
    int scaledBandHeight() const;
    int scaledBandGap() const;
    int scaledBandTitleHeight() const;
    int scaledAddressRowHeight() const;
    std::pair<long double, long double> globalAddressViewRange() const;
    std::pair<long double, long double> visibleAddressRange() const;
    std::uint64_t addressFromVisibleRatio(long double ratio) const;
    void resetVerticalAddressView();
    void normalizeVerticalView();
    std::uint64_t contentXForTimestamp(qint64 timestamp) const;
    qint64 timestampForContentX(std::uint64_t contentX) const;
    std::uint64_t contentXAtTimelineX(int x) const;
    std::optional<int> screenXForTimestamp(qint64 timestamp, const QRect& area) const;
    qint64 timestampAtTimelineX(int x) const;
    std::pair<qint64, qint64> visibleTimestampRange() const;
    QRect bandRect(int visibleBandIndex, const QRect& plot) const;
    QRect bandAddressBodyRect(int visibleBandIndex, const QRect& plot) const;
    QRect bandRailRect(int visibleBandIndex, const QRect& plot) const;
    QRect bandHeaderRailRect(int visibleBandIndex, const QRect& plot) const;
    QRect bandTitleTextRect(int visibleBandIndex, const QRect& plot) const;
    QRect bandAddressRailRect(int visibleBandIndex, const QRect& plot) const;
    QRect bandHeaderPlotRect(int visibleBandIndex, const QRect& plot) const;
    QRect bandAddressPlotRect(int visibleBandIndex, const QRect& plot) const;
    VisibleBandRows visibleAddressRowsForBand(int visibleBandIndex,
                                              qint64 visibleStart,
                                              qint64 visibleEnd,
                                              const QRect& plot) const;
    std::optional<int> screenYForAddressInBand(int visibleBandIndex,
                                               std::uint64_t address,
                                               const QRect& plot) const;
    std::uint64_t addressForScreenYInBand(int visibleBandIndex, int y, const QRect& plot) const;
    std::pair<int, int> visibleBandRange(const QRect& plot) const;
    std::vector<std::pair<std::uint64_t, int>> addressTicksForBand(int visibleBandIndex,
                                                                    const QRect& plot) const;
    QString visibleRangeText() const;
    QString sourceModeName() const;
    static QString sourceModeName(SourceMode mode);
    static QString dragModeName(DragMode mode);

    void zoomToFit();
    void zoomByFactor(double factor, const QPoint& focus);
    void zoomVerticallyByFactor(double factor, const QPoint& focus);
    void zoomToTimestampRange(qint64 firstTimestamp, qint64 lastTimestamp, bool moveCursor);
    void pushViewHistory();
    void restorePreviousView();
    void scrollToHorizontalOffset(std::uint64_t offset);
    void panHorizontallyByPixels(int pixels);
    void scrollVerticallyByPixels(int pixels);
    void ensureSpanVisible(int spanIndex);
    void setCursorFromSpan(int spanIndex, bool emitActivation);
    void setMarkerLogicalRow(int logicalRow);
    void moveCursorToAdjacentSpan(int direction);

    void beginPendingGesture(const QPoint& position);
    void updatePendingGesture(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void beginRangeZoom(const QPoint& position);
    void updateRangeZoom(const QPoint& position);
    bool finishRangeZoom();
    void beginZoomFullGesture(const QPoint& position);
    bool finishZoomFullGesture();
    void beginZoomIn2xGesture(const QPoint& position);
    bool finishZoomIn2xGesture();
    void beginZoomOut2xGesture(const QPoint& position);
    bool finishZoomOut2xGesture();
    void beginPan(const QPoint& position);
    void updatePan(const QPoint& position);
    DragMode classifyLeftDragGesture(const QPoint& position, Qt::KeyboardModifiers modifiers) const;

    bool hasHiddenBands() const;
    bool isSourceBandVisible(int sourceBandIndex) const;
    int visibleSlotForSourceBand(int sourceBandIndex) const;
    int sourceBandForVisibleSlot(int visibleBandIndex) const;
    bool addVisibleBand(int sourceBandIndex);
    bool removeVisibleBandAt(int visibleBandIndex);
    void showAddBandMenu();
    void clearDenseTileCache() const;
    void pruneDenseTileCache() const;
    bool positionInTimeline(const QPoint& position) const;
    bool positionInRuler(const QPoint& position) const;
    QRect overviewWindowRect() const;
    std::optional<int> spanIndexAtPosition(const QPoint& position) const;
    int spanIndexForLogicalRow(std::uint64_t logicalRow) const;
    QRect spanVisualRect(int spanIndex) const;
    QRect spanVisualRectInBandTile(int spanIndex,
                                   int sourceBandIndex,
                                   int visibleBandIndex,
                                   std::uint64_t tileContentLeft,
                                   int tileWidth,
                                   const QRect& plot,
                                   const VisibleBandRows& rows,
                                   int bandHeight) const;
    std::vector<int> visibleSpanIndices(qint64 visibleStart,
                                        qint64 visibleEnd,
                                        int firstVisibleBand,
                                        int lastVisibleBand) const;
    std::vector<int> visibleSpanIndicesForSourceBand(qint64 visibleStart,
                                                     qint64 visibleEnd,
                                                     int sourceBandIndex) const;
    qint64 timestampForLogicalRowInSpan(const CacheLifetimeSpan& span, std::uint64_t logicalRow) const;
    QString spanTooltipText(int spanIndex) const;

    void paintHeader(QPainter& painter) const;
    void paintTopRuler(QPainter& painter) const;
    void paintAddressAxis(QPainter& painter) const;
    void paintSpans(QPainter& painter) const;
    void paintDenseSpans(QPainter& painter,
                         const QRect& plot,
                         qint64 visibleStart,
                         qint64 visibleEnd,
                         int firstBand,
                         int lastBand,
                         const std::vector<int>& candidates) const;
    void paintSpan(QPainter& painter, int spanIndex, bool selected, bool hovered) const;
    void paintSpanRect(QPainter& painter,
                       const QRect& visual,
                       const CacheLifetimeSpan& span,
                       bool selected,
                       bool hovered) const;
    bool useDenseSpanPaint(const std::vector<int>& candidates, const QRect& plot) const;
    void paintCursorAndMarker(QPainter& painter) const;
    void paintFullRuler(QPainter& painter) const;
    void paintStatus(QPainter& painter) const;
    void paintBuildPrompt(QPainter& painter) const;
    void paintBuildProgress(QPainter& painter) const;
    void paintGestureOverlay(QPainter& painter) const;
    void paintRangeZoom(QPainter& painter) const;

private:
    std::shared_ptr<TraceSession> traceSession_;
    SourceMode sourceMode_ = SourceMode::Empty;
    std::vector<CacheRnBand> bands_;
    std::vector<CacheLifetimeSpan> spans_;
    std::vector<int> visibleBandIndices_;
    std::vector<std::vector<int>> spanIndicesByBandStart_;
    std::vector<std::vector<int>> spanIndicesByBandEnd_;
    std::vector<ViewHistoryEntry> viewHistory_;
    QVariantMap pendingViewState_;

    QScrollBar* horizontalScrollBar_ = nullptr;
    QScrollBar* verticalScrollBar_ = nullptr;
    std::jthread buildThread_;
    std::atomic<quint64> buildGeneration_{0};
    std::atomic_bool progressUpdateQueued_{false};
    std::atomic<quint64> pendingProgressGeneration_{0};
    std::atomic<std::uint64_t> pendingProgressProcessedRows_{0};
    std::atomic<std::uint64_t> pendingProgressTotalRows_{0};

    std::uint64_t totalRows_ = 0;
    std::uint64_t processedRows_ = 0;
    std::uint64_t minAddress_ = 0;
    std::uint64_t maxAddress_ = 0;
    qint64 firstTimestamp_ = 0;
    qint64 lastTimestamp_ = 0;
    bool haveAddressRange_ = false;
    bool haveTimeRange_ = false;
    bool building_ = false;
    bool buildRequested_ = false;
    QString statusText_;

    double horizontalZoom_ = 1.0;
    double verticalBandScale_ = 1.0;
    double verticalAddressZoom_ = 1.0;
    long double verticalCenterAddress_ = 0.0L;
    std::uint64_t scrollOffset_ = 0;
    int verticalScrollOffset_ = 0;
    int selectedLogicalRow_ = -1;
    int selectedSpanIndex_ = -1;
    bool hasCursor_ = false;
    qint64 cursorTimestamp_ = 0;
    bool hasMarker_ = false;
    qint64 markerTimestamp_ = 0;
    int markerLogicalRow_ = -1;
    int markerSpanIndex_ = -1;
    int hoveredSpanIndex_ = -1;

    DragMode dragMode_ = DragMode::None;
    bool leftDragGestureActive_ = false;
    QPoint dragStart_;
    QPoint dragLast_;
    QPoint leftDragAnchor_;
    QRect dragRect_;
    mutable std::vector<DenseSpanTileCacheEntry> denseSpanTileCache_;
    mutable quint64 denseSpanTileClock_ = 0;
    mutable bool lastDensePaintUsed_ = false;
};

}  // namespace CHIron::Gui
