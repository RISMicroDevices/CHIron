#pragma once

#include "flit_record.hpp"
#include "marker_store.hpp"
#include "trace_session.hpp"

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVariantMap>
#include <QWidget>

#include <atomic>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

class QEvent;
class QCheckBox;
class QComboBox;
class QContextMenuEvent;
class QColor;
class QFrame;
class QFont;
class QFontMetrics;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPushButton;
class QResizeEvent;
class QScrollBar;
class QTableWidget;
class QWheelEvent;

namespace CHIron::Gui {

class TransactionWidget final : public QWidget {
    Q_OBJECT

public:
    explicit TransactionWidget(QWidget* parent = nullptr);
    ~TransactionWidget() override;

    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setRows(std::vector<FlitRecord> rows);
    void buildView();
    void clear();
    void setSelectedLogicalRow(int logicalRow);
    void setMarkers(std::vector<TraceMarker> markers, const QString& selectedMarkerId);
    QVariantMap viewState() const;
    void restoreViewState(const QVariantMap& state);
    QVariantMap diagnosticsState() const;

Q_SIGNALS:
    void logicalRowActivated(int logicalRow);
    void markerSelected(QString markerId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

#ifdef CHIRON_GUI_ENABLE_TRANSACTION_TEST_API
public:
    int testSpanCount() const noexcept;
    int testLaneCount() const noexcept;
    QVariantMap testSpanAt(int index) const;
    QVariantMap testLaneAt(int index) const;
    QString testStatusText() const;
    QVariantMap testLayoutState() const;
    std::pair<qint64, qint64> testVisibleTimestampRange() const;
    double testHorizontalZoom() const noexcept;
    QPoint testSpanCenterPoint(int index) const;
    QString testSpanFillColorName(int index) const;
    QString testSpanEdgeColorName(int index) const;
    QRect testLaneNodeBadgeRect(int laneIndex) const;
    QRect testLaneNodeIdRect(int laneIndex) const;
    QRect testLaneChannelBadgeRect(int laneIndex) const;
    QRect testLaneHeaderCountRect(int laneIndex) const;
    QString testLaneNodeBadgeColorName(int laneIndex) const;
    QString testLaneChannelBadgeColorName(int laneIndex) const;
    int testSpanIndexAtPoint(const QPoint& position) const;
    bool testUsesDensePaint() const;
    int testSelectedLogicalRow() const noexcept;
    int testSelectedSpanIndex() const noexcept;
    bool testHasCursor() const noexcept;
    qint64 testCursorTimestamp() const noexcept;
    bool testHasMarker() const noexcept;
    int testMarkerLogicalRow() const noexcept;
    std::size_t testLastBuildWorkerCount() const noexcept;
    bool testGestureOverlayVisible() const noexcept;
    QString testDragModeName() const;
    QRect testPlotRect() const;
    QRect testRulerRect() const;
    QRect testInfoBarRect() const;
    QRect testBuildProgressRect() const;
    QRect testCursorRulerTagRect() const;
    QRect testSpanVisualRect(int spanIndex) const;
    QRect testLaneHeaderTitleRect(int laneIndex) const;
    bool testHoverCardVisible() const noexcept;
    QRect testHoverCardRect() const;
    int testHoverCardRowCount() const noexcept;
    QString testHoverCardTitle() const;
    QString testHoverCardCellText(int row, int column) const;
    bool testHoverCardDwellActive() const noexcept;
    QString testInlineLabelForSpan(int spanIndex, int availableWidth) const;
    QString testSpanLabelFontFamily() const;
    int testSpanLabelFontPixelSize() const;
    int testLaneHeaderTitleFontPixelSize() const;
    int testLaneHeaderRowFontPixelSize() const;
    int testLaneHeaderCountFontPixelSize(int laneIndex) const;
    int testRulerFontMetricHeight() const;
    int testInfoBarFontMetricHeight() const;
    int testVisiblePaintCandidateCount() const;
    int testFilterMatchCount() const noexcept;
    int testUnfilteredSpanCount() const noexcept;
    QString testArrangeModeName() const;
    QString testFilterModeName() const;
    QRect testArrangeModeSwitchRect() const;
    QRect testFilterToolbarRect() const;
    QRect testFilterModeComboRect() const;
    QRect testOpcodeFilterEditRect() const;
    QRect testAddressFilterEditRect() const;
    QRect testTxnIdFilterEditRect() const;
    QRect testDbidFilterEditRect() const;
    QRect testOpcodeTagsCheckBoxRect() const;
    QRect testHoverCardsCheckBoxRect() const;
    QRect testHeaderSelectionTextRect() const;
    void testSetArrangeModeByName(const QString& modeName);
    void testSetFilterModeByName(const QString& modeName);
    void testSetTransactionFilter(const QString& opcode,
                                  const QString& address,
                                  const QString& txnId,
                                  const QString& dbid);
    QVariantMap testFilterState() const;
    void testSetBuildProgress(std::uint64_t completedWork, std::uint64_t totalWork);
    void testInjectSyntheticSpans(int laneCount, int spanCount);
    void testInjectMixedDensitySyntheticSpans();
    void testInjectOpcodeTagSyntheticSpans();
    void testConfigureOpcodeTagSyntheticSpan(bool snpOrigin, bool congested);
    bool testSetTraceSessionAndWaitForSpans(std::shared_ptr<TraceSession> session, int timeoutMs);
    QVariantMap testBuildSpansFromTraceSession(std::shared_ptr<TraceSession> session);
    void testSetHorizontalZoom(double zoom);
    void testSetHorizontalScroll(int value);
    void testScaleSyntheticTimestamps(qint64 scale);
    void testSetVerticalScroll(int value);
    void testSetRowHeight(int rowHeight);
    void testSetCursorFromPosition(const QPoint& position);
    bool testInvokeContextAction(int spanIndex, const QString& action);
    QString testRelatedRowsTextForSpan(int spanIndex) const;
    QString testDebugTextForSpan(int spanIndex) const;
    quint64 testBuildGeneration() const noexcept;
    void testInjectOverlappingSyntheticSpans();
    void testInjectInterleavedSyntheticSpans();
    int testSelectedOpcodeTagCount() const;
    QVariantMap testSelectedOpcodeTagAt(int index) const;
    int testSelectedOpcodeTagFontPixelSize() const;
    bool testShowOpcodeTags() const noexcept;
    bool testShowHoverCards() const noexcept;
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
        Pan
    };

    enum class BuildProgressPhase {
        Idle,
        LoadingMetadata,
        ScanningRows,
        AggregatingSpans,
        LayingOutRows,
        Finalizing
    };

    enum class ArrangeMode {
        InflightStack,
        TxnId
    };

    enum class TransactionFilterMode {
        Highlight,
        Filter
    };

    struct TransactionFilterCriteria {
        QString opcode;
        QString address;
        QString txnId;
        QString dbid;
    };

    enum class TransactionFilterField {
        Opcode,
        Address,
        TxnId,
        Dbid
    };

    struct ViewHistoryEntry {
        double horizontalZoom = 1.0;
        std::uint64_t scrollOffset = 0;
        int verticalScrollOffset = 0;
        bool hasCursor = false;
        qint64 cursorTimestamp = 0;
    };

    struct TransactionLane {
        QString key;
        QString label;
        QString groupKey;
        QString groupLabel;
        QString jointFamily;
        QString endpointLabel;
        QString endpointNode;
        QString endpointNodeType;
        QString originKind;
        QString xactionType;
        QString requestOpcode;
        QString txnIdLabel;
        std::uint64_t spanCount = 0;
        int groupIndex = -1;
        int stackSlot = 0;
        int stackDepth = 1;
        std::uint64_t groupSpanCount = 0;
    };

    struct TransactionSpan {
        std::uint64_t ordinal = 0;
        QString transactionGroupKey;
        QString xactionType;
        QString requestOpcode;
        std::vector<QString> opcodeValues;
        QString addressLabel;
        QString jointFamily;
        QString jointPath;
        QString endpointLabel;
        QString endpointNode;
        QString endpointNodeType;
        QString originKind;
        QString txnIdLabel;
        QString dbidLabel;
        QString classificationConfidence;
        QString laneKey;
        int laneIndex = -1;
        int groupIndex = -1;
        int stackSlot = 0;
        int stackDepth = 1;
        std::uint64_t firstLogicalRow = 0;
        std::uint64_t lastLogicalRow = 0;
        std::uint64_t requestLogicalRow = 0;
        std::optional<std::uint64_t> completionLogicalRow;
        qint64 startTimestamp = 0;
        qint64 endTimestamp = 0;
        std::uint64_t rowCount = 0;
        bool completed = false;
        bool indexed = false;
        bool processedByJoint = false;
        std::uint8_t paintBucket = 4;
        std::vector<QString> txnIdValues;
        std::vector<QString> dbidValues;
        std::vector<std::uint64_t> logicalRows;
    };

    struct SpanInlineLabelParts {
        QString opcode;
        QString address;
    };

    struct DenseSpanPaintResult {
        std::vector<int> preservedSpanIndices;
    };

    struct DenseSpanPaintCacheKey {
        quint64 generation = 0;
        int width = 0;
        int height = 0;
        int rowHeight = 0;
        qint64 visibleStart = 0;
        qint64 visibleEnd = 0;
        int firstLane = 0;
        int lastLane = 0;
        quint64 filterRevision = 0;
        int filterMode = 0;
        bool filterActive = false;

        bool operator==(const DenseSpanPaintCacheKey& other) const noexcept = default;
    };

    struct DenseSpanPaintCache {
        DenseSpanPaintCacheKey key;
        QImage image;
        DenseSpanPaintResult result;
        bool valid = false;
    };

    struct DenseSpanLanePaintCacheKey {
        quint64 generation = 0;
        int width = 0;
        int rowHeight = 0;
        qint64 visibleStart = 0;
        qint64 visibleEnd = 0;
        int laneIndex = 0;
        quint64 filterRevision = 0;
        int filterMode = 0;
        bool filterActive = false;

        bool operator==(const DenseSpanLanePaintCacheKey& other) const noexcept = default;
    };

    struct DenseSpanLanePaintCache {
        DenseSpanLanePaintCacheKey key;
        QImage image;
        bool hasContent = false;
        bool valid = false;
    };

    struct SpanPaintIndex {
        std::vector<std::vector<int>> spanIndicesByLane;
        std::vector<std::vector<int>> spanIndicesByLaneEnd;
        qint64 fullTimestampStart = 0;
        qint64 fullTimestampEnd = 1;
    };

    struct HoverCardFlitRow {
        std::uint64_t logicalRow = 0;
        qint64 timestamp = 0;
        QString channel;
        QString direction;
        QString opcode;
        QString source;
        QString target;
        QString txnId;
        QString address;
        bool decoded = false;
    };

    struct OpcodeTagInfo {
        std::uint64_t logicalRow = 0;
        qint64 timestamp = 0;
        FlitChannel channel = FlitChannel::Req;
        QString opcode;
    };

    struct OpcodeTagLayout {
        OpcodeTagInfo info;
        QRect tagRect;
        QRect lineRect;
        QRect triangleBounds;
        QRect inlineTextRect;
        std::array<QPoint, 3> trianglePoints{};
        QColor color;
        QString text;
        int fullWidth = 0;
        int timestampX = 0;
        bool bottomPlacement = false;
        bool lineVisible = false;
        bool triangleVisible = false;
        bool selected = false;
    };

    struct BuildResult {
        quint64 generation = 0;
        ArrangeMode arrangeMode = ArrangeMode::InflightStack;
        std::uint64_t totalXactions = 0;
        std::uint64_t processedXactions = 0;
        std::uint64_t skippedXactions = 0;
        qint64 metadataNs = 0;
        qint64 scanNs = 0;
        qint64 mergeNs = 0;
        qint64 firstInfoNs = 0;
        qint64 spanBuildNs = 0;
        qint64 sortNs = 0;
        qint64 layoutNs = 0;
        std::size_t workerCount = 1;
        std::vector<TransactionLane> lanes;
        std::vector<TransactionSpan> spans;
        QString statusText;
        bool cancelled = false;
        bool failed = false;
    };

    struct FilterResult {
        quint64 generation = 0;
        quint64 buildGeneration = 0;
        TransactionFilterMode mode = TransactionFilterMode::Highlight;
        TransactionFilterCriteria criteria;
        std::uint64_t matchCount = 0;
        std::vector<unsigned char> unfilteredMatches;
        std::vector<unsigned char> displayMatches;
        std::vector<TransactionLane> lanes;
        std::vector<TransactionSpan> spans;
        SpanPaintIndex paintIndex;
        bool displayingFilteredSpans = false;
        bool cancelled = false;
    };

    struct SpanBuildWorkerResult {
        std::vector<TransactionSpan> spans;
        std::uint64_t processedXactions = 0;
        std::uint64_t skippedXactions = 0;
        bool failed = false;
        bool cancelled = false;
        QString statusText;
    };

    static QString sourceModeName(SourceMode mode);
    static SourceMode sourceModeFromName(const QString& name);
    static bool transactionFilterActive(const TransactionFilterCriteria& criteria) noexcept;
    static bool transactionFilterMatchesSpan(const TransactionSpan& span, const TransactionFilterCriteria& criteria);
    static SpanPaintIndex buildSpanPaintIndex(std::vector<TransactionSpan>& spans, std::size_t laneCount);
    static void rebuildSpanPaintIndex(std::vector<TransactionSpan>& spans,
                                      std::vector<std::vector<int>>& spanIndicesByLane,
                                      std::vector<std::vector<int>>& spanIndicesByLaneEnd,
                                      qint64& fullTimestampStart,
                                      qint64& fullTimestampEnd,
                                      std::size_t laneCount);

    void stopBuildThread();
    void stopFilterThread();
    void refresh();
    void startSessionBuild(std::shared_ptr<TraceSession> session);
    std::shared_ptr<BuildResult> buildSpansFromSession(const std::shared_ptr<TraceSession>& session,
                                                       quint64 generation,
                                                       ArrangeMode arrangeMode,
                                                       std::stop_token stopToken) const;
    void applyBuildResult(std::shared_ptr<BuildResult> result);
    void updateBuildProgress(quint64 generation,
                             BuildProgressPhase phase,
                             std::uint64_t completedWork,
                             std::uint64_t totalWork);
    void updateFilterProgress(quint64 generation, std::uint64_t completedWork, std::uint64_t totalWork);
    QString buildProgressText() const;
    QString filterProgressText() const;
    bool hasPendingBuild() const noexcept;
    QString pendingBuildText() const;
    QRect buildPromptButtonRect() const;
    void paintBuildPrompt(QPainter& painter) const;
    QRect buildProgressRect() const;
    QRect arrangeModeSwitchRect() const;
    QRect filterToolbarRect() const;
    void updateFilterToolbarGeometry();
    QRect headerSelectionTextRect() const;
    void resetBuildProgress();
    void paintBuildProgress(QPainter& painter) const;
    void clearSpanData();
    void rebuildSpanPaintIndex();
    void updateStatusText();
    void updateWidgetHints();
    void updateMouseCursor();
    bool hasSelectedLogicalRow() const noexcept;
    void updateScrollBars();
    QRect headerRect() const;
    int contentTop() const;
    QRect bodyRect() const;
    QRect laneHeaderRect() const;
    QRect plotRect() const;
    QRect rulerRect() const;
    QRect infoBarRect() const;
    QRect horizontalScrollBarRect() const;
    QRect verticalScrollBarRect() const;
    int scrollBarExtent() const;
    int laneHeaderWidth() const;
    int rowHeight() const noexcept;
    int laneHeaderTitleHeight() const noexcept;
    int laneHeaderBadgeHeight() const noexcept;
    QFont rulerFont() const;
    QFont infoBarFont() const;
    void setRowHeight(int rowHeight);
    void adjustRowHeightFromWheel(int wheelDelta);
    int visibleLaneCapacity() const;
    int stackRowCount() const noexcept;
    int transactionGroupCount() const noexcept;
    int maxStackDepth() const noexcept;
    std::pair<qint64, qint64> fullTimestampRange() const;
    qint64 visibleStartTimestamp() const;
    qint64 visibleEndTimestamp() const;
    double timestampToPlotX(qint64 timestamp, const QRect& plot) const;
    qint64 plotXToTimestamp(int x, const QRect& plot) const;
    int laneTopY(int laneIndex, const QRect& plot) const;
    int laneIndexAtY(int y, const QRect& plot) const;
    QRect laneHeaderTitleRect(int laneIndex, const QRect& laneHeader, const QRect& plot) const;
    QRect spanVisualRect(int spanIndex, const QRect& plot) const;
    QRect spanVisualRectForRange(const TransactionSpan& span,
                                 const QRect& plot,
                                 qint64 visibleStart,
                                 qint64 visibleEnd,
                                 long double visibleRange,
                                 bool requireVisibleLane = true) const;
    bool isFirstClassificationRow(int laneIndex) const;
    QRect laneNodeBadgeRect(int laneIndex, const QRect& laneHeader, const QRect& plot) const;
    QRect laneNodeIdRect(int laneIndex, const QRect& laneHeader, const QRect& plot) const;
    QRect laneChannelBadgeRect(int laneIndex, const QRect& laneHeader, const QRect& plot) const;
    QRect laneHeaderCountRect(int laneIndex, const QRect& laneHeader, const QRect& plot) const;
    QFont laneHeaderCountFont(int laneIndex) const;
    QString laneHeaderCountText(int laneIndex) const;
    int visibleSpanCount(qint64 visibleStart, qint64 visibleEnd, int maxCount) const;
    std::vector<int> visibleSpanIndices(qint64 visibleStart, qint64 visibleEnd) const;
    std::vector<int> visibleSpanIndicesForLane(int laneIndex, qint64 visibleStart, qint64 visibleEnd) const;
    int spanIndexAtPosition(const QPoint& position) const;
    int nearestVisibleSpanIndexAtPosition(const QPoint& position) const;
    bool densePaintEnabled(const QRect& plot) const;
    bool densePaintEnabledForVisible(const QRect& plot, int visibleSpanCount) const;
    DenseSpanPaintCacheKey denseSpanPaintCacheKey(const QRect& plot,
                                                  qint64 visibleStart,
                                                  qint64 visibleEnd,
                                                  int firstLane,
                                                  int lastLane) const;
    DenseSpanLanePaintCacheKey denseSpanLanePaintCacheKey(const QRect& plot,
                                                          qint64 visibleStart,
                                                          qint64 visibleEnd,
                                                          int laneIndex) const;
    void invalidateDenseSpanPaintCache() const;
    DenseSpanPaintResult paintDenseSpans(QPainter& painter,
                                         const QRect& plot,
                                         qint64 visibleStart,
                                         qint64 visibleEnd,
                                         long double visibleRange,
                                         int firstLane,
                                         int lastLane) const;
    DenseSpanPaintResult paintDenseSpansUncached(QPainter& painter,
                                                 const QRect& plot,
                                                 qint64 visibleStart,
                                                 qint64 visibleEnd,
                                                 long double visibleRange,
                                                 int firstLane,
                                                 int lastLane) const;
    void paintDenseSpanLaneRow(QPainter& painter,
                               const QRect& plot,
                               qint64 visibleStart,
                               qint64 visibleEnd,
                               long double visibleRange,
                               int laneIndex) const;
    void paintDenseFilteredOutSpans(QPainter& painter,
                                    const QRect& plot,
                                    qint64 visibleStart,
                                    qint64 visibleEnd,
                                    long double visibleRange,
                                    int firstLane,
                                    int lastLane) const;
    void paintSpanRectangles(QPainter& painter,
                             const std::vector<int>& spanIndices,
                             const QRect& plot,
                             qint64 visibleStart,
                             qint64 visibleEnd,
                             long double visibleRange,
                             bool includeInteractionState,
                             bool drawInlineLabels,
                             bool requireVisibleLane = true) const;
    QFont opcodeTagFont() const;
    std::optional<OpcodeTagInfo> opcodeTagInfoForLogicalRow(const TransactionSpan& span,
                                                            std::uint64_t logicalRow) const;
    std::vector<OpcodeTagInfo> opcodeTagInfosForSpan(const TransactionSpan& span) const;
    QRect spanInlineTextBounds(const TransactionSpan& span,
                               const QRect& spanRect,
                               const QFontMetrics& opcodeMetrics,
                               const QFontMetrics& addressMetrics) const;
    std::vector<OpcodeTagLayout> selectedOpcodeTagLayouts(const QRect& plot,
                                                          qint64 visibleStart,
                                                          qint64 visibleEnd,
                                                          long double visibleRange,
                                                          bool inlineLabelsDrawn) const;
    void paintSelectedOpcodeTags(QPainter& painter,
                                 const QRect& plot,
                                 qint64 visibleStart,
                                 qint64 visibleEnd,
                                 long double visibleRange,
                                 bool inlineLabelsDrawn) const;
    std::optional<OpcodeTagLayout> opcodeTagLayoutAtPosition(const QPoint& position) const;
    int opcodeTagIndexAtPosition(const QPoint& position) const;
    void clearSelectedOpcodeTag();
    void selectOpcodeTagLayout(const OpcodeTagLayout& layout, bool moveCursor);
    bool selectOpcodeTagAtPosition(const QPoint& position);
    bool activateOpcodeTagAtPosition(const QPoint& position);
    QColor laneAccentColor(const TransactionLane& lane) const;
    QColor spanFillColor(const TransactionSpan& span) const;
    QColor spanEdgeColor(const TransactionSpan& span) const;
    void setArrangeMode(ArrangeMode mode);
    void rebuildArrangement();
    void arrangeSpans(std::vector<TransactionSpan>& spans,
                      std::vector<TransactionLane>& lanes,
                      ArrangeMode arrangeMode,
                      const std::function<void(std::uint64_t completedWork,
                                               std::uint64_t totalWork)>& progressCallback = {}) const;
    SpanInlineLabelParts spanInlineLabelParts(const TransactionSpan& span,
                                              int availableWidth,
                                              const QFontMetrics& opcodeMetrics,
                                              const QFontMetrics& addressMetrics) const;
    QString spanInlineLabel(const TransactionSpan& span,
                            int availableWidth,
                            const QFontMetrics& opcodeMetrics,
                            const QFontMetrics& addressMetrics) const;
    QString spanStatusText(const TransactionSpan& span) const;
    qint64 timestampForLogicalRowInSpan(const TransactionSpan& span, std::uint64_t logicalRow) const;
    bool spanHasCachedLogicalRow(const TransactionSpan& span, std::uint64_t logicalRow) const;
    int spanIndexForLogicalRow(std::uint64_t logicalRow) const;
    qint64 snappedCursorTimestampForSpan(const TransactionSpan& span, qint64 requestedTimestamp) const;
    bool selectSpan(int spanIndex, std::optional<qint64> requestedTimestamp);
    bool selectedOpcodeTagAnchorsCursor() const noexcept;
    bool stickCursorToSelectedOpcodeTag(bool ensureVisible);
    void reconcileSelectedOpcodeTag();
    void ensureTimestampVisible(qint64 timestamp);
    void ensureSpanVisible(int spanIndex);
    bool setCursorFromPlotPosition(const QPoint& position, bool snapToTransaction);
    void setMarkerFromSelectedSpan();
    std::optional<QString> sharedMarkerAtPosition(const QPoint& position) const;
    bool activateSpanRequestRow(int spanIndex);
    bool activateSpanCompletionRow(int spanIndex);
    bool showSpanRelatedRows(int spanIndex);
    bool showSpanDebugInfo(int spanIndex);
    QString relatedRowsText(const TransactionSpan& span) const;
    QString debugText(const TransactionSpan& span) const;
    std::vector<std::uint64_t> relatedLogicalRowsForSpan(const TransactionSpan& span) const;
    void ensureHoverCardCreated();
    void hideHoverCard();
    void armHoverCard(int spanIndex, const QPoint& position);
    void showArmedHoverCard();
    void updateHoverCardForSpan(int spanIndex, const QPoint& position);
    void populateHoverCard(int spanIndex);
    QRect hoverCardGeometryForPosition(const QPoint& position, const QSize& size) const;
    std::vector<HoverCardFlitRow> hoverCardFlitRowsForSpan(const TransactionSpan& span) const;
    HoverCardFlitRow hoverCardRowForLogicalRow(const TransactionSpan& span, std::uint64_t logicalRow) const;
    void pushViewHistory();
    void restorePreviousView();
    void zoomToFit();
    void zoomByFactor(double factor, const QPoint& focus);
    void zoomToTimeRange(qint64 firstTimestamp, qint64 lastTimestamp);
    void panHorizontallyByPixels(int pixels);
    void panVerticallyByRows(int rows);
    void moveSelectionBySpan(int direction);
    void beginPendingGesture(const QPoint& position);
    void updateLeftDragGesture(const QPoint& position, Qt::KeyboardModifiers modifiers);
    DragMode classifyLeftDragGesture(const QPoint& position, Qt::KeyboardModifiers modifiers) const;
    bool finishRangeZoom();
    bool finishZoomFullGesture();
    bool finishZoomIn2xGesture();
    bool finishZoomOut2xGesture();
    void beginPan(const QPoint& position);
    void updatePan(const QPoint& position);
    bool positionInPlotOrRuler(const QPoint& position) const;
    QRect cursorRulerTagRect(qint64 timestamp,
                             const QRect& plot,
                             const QRect& ruler,
                             const QFontMetrics& metrics) const;
    void paintCursorRulerTag(QPainter& painter, const QRect& plot, const QRect& ruler) const;
    void paintSharedMarkers(QPainter& painter, const QRect& plot, const QRect& ruler, const QRect& infoBar) const;
    void paintBottomSpanSnap(QPainter& painter, const QRect& plot, const QRect& ruler) const;
    void paintRangeZoom(QPainter& painter) const;
    void paintGestureOverlay(QPainter& painter) const;
    static QString dragModeName(DragMode mode);
    static QString buildProgressPhaseName(BuildProgressPhase phase);
    static QString arrangeModeName(ArrangeMode mode);
    static ArrangeMode arrangeModeFromName(const QString& name);
    static QString filterModeName(TransactionFilterMode mode);
    static TransactionFilterMode filterModeFromName(const QString& name);
    bool transactionFilterActive() const noexcept;
    bool requestedTransactionFilterActive() const noexcept;
    bool transactionFilterMatchesSpan(const TransactionSpan& span) const;
    bool displayedSpanMatchesFilter(int spanIndex) const noexcept;
    bool shouldDrawFilteredOutSpan(int spanIndex) const noexcept;
    void syncFilterControls();
    void setFilterMode(TransactionFilterMode mode);
    void setFilterCriteria(TransactionFilterCriteria criteria);
    void setFilterState(TransactionFilterMode mode, TransactionFilterCriteria criteria);
    void applyTransactionFilter();
    void applyTransactionFilterSynchronously();
    void startTransactionFilter();
    std::shared_ptr<FilterResult> buildTransactionFilterResult(quint64 generation,
                                                               quint64 buildGeneration,
                                                               TransactionFilterMode mode,
                                                               TransactionFilterCriteria criteria,
                                                               ArrangeMode arrangeMode,
                                                               std::stop_token stopToken,
                                                               bool publishProgress) const;
    void applyTransactionFilterResult(std::shared_ptr<FilterResult> result);
    void resetFilterProgress();
    QString filterValueForSpanField(const TransactionSpan& span, TransactionFilterField field) const;
    bool applySpanFilterAction(int spanIndex, TransactionFilterMode mode, TransactionFilterField field);
    void clearTransactionFilter();

    std::shared_ptr<TraceSession> traceSession_;
    std::jthread buildThread_;
    std::jthread filterThread_;
    std::atomic<quint64> buildGeneration_ = 0;
    std::atomic<quint64> filterGeneration_ = 0;
    std::vector<FlitRecord> rows_;
    std::vector<TransactionSpan> unfilteredSpans_;
    std::vector<TransactionLane> unfilteredLanes_;
    std::vector<TransactionLane> lanes_;
    std::vector<TransactionSpan> spans_;
    std::vector<std::vector<int>> spanIndicesByLane_;
    std::vector<std::vector<int>> spanIndicesByLaneEnd_;
    std::vector<unsigned char> unfilteredSpanFilterMatches_;
    std::vector<unsigned char> spanFilterMatches_;
    SourceMode sourceMode_ = SourceMode::Empty;
    QString statusText_;
    int selectedLogicalRow_ = -1;
    int selectedSpanIndex_ = -1;
    int selectedOpcodeTagSpanIndex_ = -1;
    std::uint64_t selectedOpcodeTagLogicalRow_ = 0;
    qint64 selectedOpcodeTagTimestamp_ = 0;
    bool hasCursor_ = false;
    bool hasMarker_ = false;
    bool building_ = false;
    bool filtering_ = false;
    bool filterRestartPending_ = false;
    bool buildRequested_ = false;
    qint64 cursorTimestamp_ = 0;
    qint64 markerTimestamp_ = 0;
    int markerLogicalRow_ = -1;
    int markerSpanIndex_ = -1;
    double horizontalZoom_ = 1.0;
    std::uint64_t scrollOffset_ = 0;
    int verticalScrollOffset_ = 0;
    int rowHeight_ = 32;
    int hoveredSpanIndex_ = -1;
    std::vector<TraceMarker> sharedMarkers_;
    QString selectedMarkerId_;
    QString hoveredMarkerId_;
    DragMode dragMode_ = DragMode::None;
    bool leftDragGestureActive_ = false;
    bool opcodeTagPressPending_ = false;
    QPoint dragStart_;
    QPoint dragLast_;
    QPoint leftDragAnchor_;
    std::uint64_t dragStartScrollOffset_ = 0;
    int dragStartVerticalOffset_ = 0;
    std::deque<ViewHistoryEntry> viewHistory_;
    std::uint64_t processedXactions_ = 0;
    std::uint64_t totalXactions_ = 0;
    qint64 fullTimestampStart_ = 0;
    qint64 fullTimestampEnd_ = 1;
    BuildProgressPhase buildProgressPhase_ = BuildProgressPhase::Idle;
    std::uint64_t buildProgressCompleted_ = 0;
    std::uint64_t buildProgressTotal_ = 0;
    std::uint64_t filterProgressCompleted_ = 0;
    std::uint64_t filterProgressTotal_ = 0;
    std::size_t lastBuildWorkerCount_ = 1;
    qint64 lastBuildMetadataNs_ = 0;
    qint64 lastBuildScanNs_ = 0;
    qint64 lastBuildMergeNs_ = 0;
    qint64 lastBuildFirstInfoNs_ = 0;
    qint64 lastBuildSpanNs_ = 0;
    qint64 lastBuildSortNs_ = 0;
    qint64 lastBuildLayoutNs_ = 0;
    ArrangeMode arrangeMode_ = ArrangeMode::InflightStack;
    TransactionFilterMode requestedFilterMode_ = TransactionFilterMode::Highlight;
    TransactionFilterCriteria requestedFilterCriteria_;
    TransactionFilterMode filterMode_ = TransactionFilterMode::Highlight;
    TransactionFilterCriteria filterCriteria_;
    std::uint64_t filterMatchCount_ = 0;
    bool displayingFilteredSpans_ = false;
    bool synchronizingFilterControls_ = false;
    quint64 filterRevision_ = 0;
    mutable DenseSpanPaintCache denseSpanPaintCache_;
    mutable std::vector<DenseSpanLanePaintCache> denseSpanLanePaintCache_;
    QPoint lastMousePosition_;
    QFrame* hoverCard_ = nullptr;
    QLabel* hoverCardTitle_ = nullptr;
    QLabel* hoverCardSummary_ = nullptr;
    QTableWidget* hoverCardTable_ = nullptr;
    QLabel* hoverCardFooter_ = nullptr;
    int hoverCardSpanIndex_ = -1;
    QTimer* hoverCardDwellTimer_ = nullptr;
    int hoverCardPendingSpanIndex_ = -1;
    QPoint hoverCardPendingPosition_;
    QScrollBar* horizontalScrollBar_ = nullptr;
    QScrollBar* verticalScrollBar_ = nullptr;
    QFrame* filterToolbar_ = nullptr;
    QComboBox* filterModeCombo_ = nullptr;
    QLineEdit* opcodeFilterEdit_ = nullptr;
    QLineEdit* addressFilterEdit_ = nullptr;
    QLineEdit* txnIdFilterEdit_ = nullptr;
    QLineEdit* dbidFilterEdit_ = nullptr;
    QCheckBox* opcodeTagsCheckBox_ = nullptr;
    QCheckBox* hoverCardsCheckBox_ = nullptr;
    QPushButton* filterClearButton_ = nullptr;
    bool showOpcodeTags_ = true;
    bool showHoverCards_ = true;
};

}  // namespace CHIron::Gui
