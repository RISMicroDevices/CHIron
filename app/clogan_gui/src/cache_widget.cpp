#include "cache_widget.hpp"

#include "gui_format.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QVariantList>
#include <QWheelEvent>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <thread>
#include <unordered_map>
#include <utility>

namespace CHIron::Gui {

namespace {

constexpr int kHeaderHeight = 44;
constexpr int kTopRulerHeight = 22;
constexpr int kAxisWidth = 240;
constexpr int kBottomRulerHeight = 34;
constexpr int kScrollBarExtent = 16;
constexpr int kBandHeight = 156;
constexpr int kBandGap = 8;
constexpr int kBandTitleHeight = 42;
constexpr int kLineRectHeight = 10;
constexpr int kAddressRowHeight = 18;
constexpr int kDenseSpanThreshold = 1800;
constexpr int kDenseTileWidth = 2048;
constexpr int kDenseTileCacheMaxEntries = 160;
constexpr int kGestureActivationDistance = 7;
constexpr int kGestureDirectionDistance = 24;
constexpr int kProgressBatchRows = 4096;
constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 1000000.0;
constexpr double kMinVerticalBandScale = 0.55;
constexpr double kMaxVerticalBandScale = 5.0;
constexpr double kMinVerticalAddressZoom = 1.0;
constexpr double kMaxVerticalAddressZoom = 1048576.0;
constexpr long double kAddressGlobalMarginFraction = 0.08L;
constexpr double kPi = 3.14159265358979323846;

QColor withAlpha(QColor color, const int alpha)
{
    color.setAlpha(alpha);
    return color;
}

int snappedGestureOctant(const QPoint& delta)
{
    if (delta.isNull()) {
        return 0;
    }

    int octant = static_cast<int>(std::lround(std::atan2(static_cast<double>(delta.y()),
                                                         static_cast<double>(delta.x()))
                                              / (kPi / 4.0)));
    if (octant > 4) {
        octant -= 8;
    } else if (octant < -4) {
        octant += 8;
    }
    return octant;
}

bool gestureDirectionReady(const QPoint& delta)
{
    const int dx = delta.x();
    const int dy = delta.y();
    return dx * dx + dy * dy >= kGestureDirectionDistance * kGestureDirectionDistance;
}

QString addressText(const std::uint64_t address)
{
    return QStringLiteral("0x%1").arg(QString::number(static_cast<qulonglong>(address), 16).toUpper(),
                                      12,
                                      QLatin1Char('0'));
}

QString compactAddressText(const std::uint64_t address)
{
    return QStringLiteral("0x%1").arg(QString::number(static_cast<qulonglong>(address), 16).toUpper());
}

double niceStep(const double value)
{
    if (value <= 0.0) {
        return 1.0;
    }
    const double exponent = std::floor(std::log10(value));
    const double base = std::pow(10.0, exponent);
    const double fraction = value / base;
    if (fraction <= 1.0) {
        return base;
    }
    if (fraction <= 2.0) {
        return 2.0 * base;
    }
    if (fraction <= 5.0) {
        return 5.0 * base;
    }
    return 10.0 * base;
}

qint64 clampTimestamp(const qint64 value, const qint64 first, const qint64 last)
{
    return std::clamp(value, std::min(first, last), std::max(first, last));
}

std::uint64_t clampOffsetToRange(const std::uint64_t offset, const std::uint64_t range)
{
    return std::min(offset, range);
}

int intScrollRangeFromU64(const std::uint64_t range)
{
    return static_cast<int>(std::min<std::uint64_t>(
        range,
        static_cast<std::uint64_t>((std::numeric_limits<int>::max)())));
}

std::uint64_t u64FromScrollValue(const int value, const std::uint64_t range)
{
    return std::min<std::uint64_t>(static_cast<std::uint64_t>(std::max(0, value)), range);
}

bool traceSupportsCacheReplay(const std::shared_ptr<TraceSession>& session)
{
    return session && session->metadata().parameters.GetIssue() == CLog::Issue::Eb;
}

QString issueLabel(const CLog::Issue issue)
{
    switch (issue) {
    case CLog::Issue::B:
        return QStringLiteral("CHI B");
    case CLog::Issue::Eb:
        return QStringLiteral("CHI E.b");
    }
    return QStringLiteral("CHI");
}

QColor spanStateColor(const QString& stateText, const bool openEnded)
{
    if (openEnded) {
        return QColor(QStringLiteral("#7C3AED"));
    }
    const QString normalized = stateText.toUpper();
    if (normalized.contains(QStringLiteral("UD"))
        || normalized.contains(QStringLiteral("SD"))) {
        return QColor(QStringLiteral("#D97706"));
    }
    if (normalized.contains(QStringLiteral("UC"))) {
        return QColor(QStringLiteral("#059669"));
    }
    return QColor(QStringLiteral("#2563EB"));
}

}  // namespace

bool CacheWidget::DenseSpanTileKey::operator==(const DenseSpanTileKey& other) const noexcept
{
    return generation == other.generation
        && sourceBandIndex == other.sourceBandIndex
        && tileContentLeft == other.tileContentLeft
        && tileWidth == other.tileWidth
        && bandHeight == other.bandHeight
        && contentWidth == other.contentWidth
        && firstTimestamp == other.firstTimestamp
        && lastTimestamp == other.lastTimestamp
        && minAddress == other.minAddress
        && maxAddress == other.maxAddress
        && visibleMinAddress == other.visibleMinAddress
        && visibleMaxAddress == other.visibleMaxAddress
        && visibleStartTimestamp == other.visibleStartTimestamp
        && visibleEndTimestamp == other.visibleEndTimestamp
        && addressRowHeight == other.addressRowHeight
        && denseAddressMode == other.denseAddressMode
        && std::abs(horizontalZoom - other.horizontalZoom) < 0.0001
        && std::abs(verticalBandScale - other.verticalBandScale) < 0.0001
        && std::abs(verticalAddressZoom - other.verticalAddressZoom) < 0.0001
        && std::abs(verticalCenterAddress - other.verticalCenterAddress) < 0.5L
        && std::abs(devicePixelRatio - other.devicePixelRatio) < 0.0001;
}

CacheWidget::CacheWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("cacheWidget"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumHeight(200);

    horizontalScrollBar_ = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar_->setObjectName(QStringLiteral("cacheHorizontalScrollBar"));
    connect(horizontalScrollBar_, &QScrollBar::valueChanged, this, [this](const int value) {
        const std::uint64_t offset = u64FromScrollValue(value, horizontalContentRange());
        if (scrollOffset_ != offset) {
            scrollOffset_ = offset;
            update();
        }
    });

    verticalScrollBar_ = new QScrollBar(Qt::Vertical, this);
    verticalScrollBar_->setObjectName(QStringLiteral("cacheVerticalScrollBar"));
    connect(verticalScrollBar_, &QScrollBar::valueChanged, this, [this](const int value) {
        const int offset = std::clamp(value, 0, verticalContentRange());
        if (verticalScrollOffset_ != offset) {
            verticalScrollOffset_ = offset;
            update();
        }
    });

    statusText_ = QStringLiteral("Open a trace to populate Cache.");
    updateScrollBars();
}

CacheWidget::~CacheWidget()
{
    stopBuildThread();
}

void CacheWidget::setTraceSession(std::shared_ptr<TraceSession> session)
{
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_ = std::move(session);
    sourceMode_ = traceSession_ ? SourceMode::TraceSession : SourceMode::Empty;
    bands_.clear();
    spans_.clear();
    visibleBandIndices_.clear();
    spanIndicesByBandStart_.clear();
    spanIndicesByBandEnd_.clear();
    viewHistory_.clear();
    clearDenseTileCache();
    resetView();
    totalRows_ = traceSession_ ? traceSession_->totalRows() : 0;
    processedRows_ = 0;
    building_ = false;
    buildRequested_ = false;
    if (!traceSession_) {
        statusText_ = QStringLiteral("Open a trace to populate Cache.");
    } else if (!traceSupportsCacheReplay(traceSession_)) {
        statusText_ = QStringLiteral("Cache unavailable: %1 traces do not expose CHI E.b RN cache-state replay.")
                          .arg(issueLabel(traceSession_->metadata().parameters.GetIssue()));
    } else {
        statusText_ = pendingBuildText();
    }
    updateScrollBars();
    update();
}

void CacheWidget::setRows(std::vector<FlitRecord> rows)
{
    Q_UNUSED(rows);
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_.reset();
    sourceMode_ = SourceMode::RowSnapshot;
    bands_.clear();
    spans_.clear();
    visibleBandIndices_.clear();
    spanIndicesByBandStart_.clear();
    spanIndicesByBandEnd_.clear();
    viewHistory_.clear();
    clearDenseTileCache();
    resetView();
    totalRows_ = 0;
    processedRows_ = 0;
    building_ = false;
    buildRequested_ = false;
    statusText_ = QStringLiteral("Cache unavailable: row snapshots do not include raw CLog.B E.b replay context.");
    updateScrollBars();
    update();
}

void CacheWidget::buildView()
{
    if (building_) {
        return;
    }
    if (!traceSession_) {
        statusText_ = sourceMode_ == SourceMode::RowSnapshot
            ? QStringLiteral("Cache unavailable: row snapshots do not include raw CLog.B E.b replay context.")
            : QStringLiteral("Open a trace to populate Cache.");
        update();
        return;
    }
    if (!traceSupportsCacheReplay(traceSession_)) {
        buildRequested_ = true;
        statusText_ = QStringLiteral("Cache unavailable: %1 traces are not supported; open a CHI E.b trace.")
                          .arg(issueLabel(traceSession_->metadata().parameters.GetIssue()));
        update();
        return;
    }

    buildRequested_ = true;
    building_ = true;
    processedRows_ = 0;
    totalRows_ = traceSession_->totalRows();
    statusText_ = QStringLiteral("Cache: replaying RN coherency state...");
    update();
    startSessionBuild(traceSession_);
}

void CacheWidget::clear()
{
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_.reset();
    sourceMode_ = SourceMode::Empty;
    bands_.clear();
    spans_.clear();
    visibleBandIndices_.clear();
    spanIndicesByBandStart_.clear();
    spanIndicesByBandEnd_.clear();
    viewHistory_.clear();
    clearDenseTileCache();
    resetView();
    totalRows_ = 0;
    processedRows_ = 0;
    building_ = false;
    buildRequested_ = false;
    statusText_ = QStringLiteral("Open a trace to populate Cache.");
    updateScrollBars();
    update();
}

void CacheWidget::setSelectedLogicalRow(const int logicalRow)
{
    selectedLogicalRow_ = logicalRow >= 0 ? logicalRow : -1;
    selectedSpanIndex_ = -1;
    if (selectedLogicalRow_ >= 0 && !spans_.empty()) {
        selectedSpanIndex_ = spanIndexForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
        if (selectedSpanIndex_ >= 0) {
            const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(selectedSpanIndex_)];
            cursorTimestamp_ = timestampForLogicalRowInSpan(span, static_cast<std::uint64_t>(selectedLogicalRow_));
            hasCursor_ = true;
            if (isSourceBandVisible(span.rnBandIndex)) {
                ensureSpanVisible(selectedSpanIndex_);
            }
        }
    }
    update();
}

QVariantMap CacheWidget::viewState() const
{
    QVariantMap state;
    QVariantList visibleBands;
    visibleBands.reserve(static_cast<int>(visibleBandIndices_.size()));
    for (const int sourceBandIndex : visibleBandIndices_) {
        if (sourceBandIndex < 0 || sourceBandIndex >= static_cast<int>(bands_.size())) {
            continue;
        }
        visibleBands.push_back(static_cast<int>(bands_[static_cast<std::size_t>(sourceBandIndex)].rnNodeId));
    }
    state.insert(QStringLiteral("sourceMode"), sourceModeName());
    state.insert(QStringLiteral("horizontalZoom"), horizontalZoom_);
    state.insert(QStringLiteral("scrollOffset"),
                 QVariant::fromValue<qulonglong>(static_cast<qulonglong>(scrollOffset_)));
    state.insert(QStringLiteral("verticalScrollOffset"), verticalScrollOffset_);
    state.insert(QStringLiteral("verticalBandScale"), verticalBandScale_);
    state.insert(QStringLiteral("verticalAddressZoom"), verticalAddressZoom_);
    state.insert(QStringLiteral("verticalCenterAddress"), static_cast<double>(verticalCenterAddress_));
    state.insert(QStringLiteral("visibleRnNodeIds"), visibleBands);
    state.insert(QStringLiteral("selectedLogicalRow"), selectedLogicalRow_);
    state.insert(QStringLiteral("selectedSpanIndex"), selectedSpanIndex_);
    state.insert(QStringLiteral("hasCursor"), hasCursor_);
    state.insert(QStringLiteral("cursorTimestamp"), cursorTimestamp_);
    state.insert(QStringLiteral("hasMarker"), hasMarker_);
    state.insert(QStringLiteral("markerTimestamp"), markerTimestamp_);
    state.insert(QStringLiteral("markerLogicalRow"), markerLogicalRow_);
    state.insert(QStringLiteral("markerSpanIndex"), markerSpanIndex_);
    state.insert(QStringLiteral("spanCount"), static_cast<int>(spans_.size()));
    state.insert(QStringLiteral("bandCount"), static_cast<int>(bands_.size()));
    state.insert(QStringLiteral("visibleBandCount"), static_cast<int>(visibleBandIndices_.size()));
    return state;
}

void CacheWidget::restoreViewState(const QVariantMap& state)
{
    pendingViewState_ = state;
    horizontalZoom_ = std::clamp(state.value(QStringLiteral("horizontalZoom"), 1.0).toDouble(),
                                 kMinZoom,
                                 kMaxZoom);
    scrollOffset_ = state.value(QStringLiteral("scrollOffset"), 0).toULongLong();
    verticalScrollOffset_ = std::max(0, state.value(QStringLiteral("verticalScrollOffset"), 0).toInt());
    verticalBandScale_ = std::clamp(state.value(QStringLiteral("verticalBandScale"), 1.0).toDouble(),
                                    kMinVerticalBandScale,
                                    kMaxVerticalBandScale);
    verticalAddressZoom_ = std::clamp(state.value(QStringLiteral("verticalAddressZoom"), 1.0).toDouble(),
                                      kMinVerticalAddressZoom,
                                      kMaxVerticalAddressZoom);
    verticalCenterAddress_ =
        static_cast<long double>(state.value(QStringLiteral("verticalCenterAddress"),
                                             static_cast<double>(verticalCenterAddress_)).toDouble());
    visibleBandIndices_.clear();
    const QVariantList visibleBands = state.value(QStringLiteral("visibleRnNodeIds")).toList();
    for (const QVariant& value : visibleBands) {
        const int rnNodeId = value.toInt();
        for (int bandIndex = 0; bandIndex < static_cast<int>(bands_.size()); ++bandIndex) {
            const CacheRnBand& band = bands_[static_cast<std::size_t>(bandIndex)];
            if (static_cast<int>(band.rnNodeId) == rnNodeId && !isSourceBandVisible(bandIndex)) {
                visibleBandIndices_.push_back(bandIndex);
                break;
            }
        }
    }
    selectedLogicalRow_ = state.value(QStringLiteral("selectedLogicalRow"), -1).toInt();
    selectedSpanIndex_ = state.value(QStringLiteral("selectedSpanIndex"), -1).toInt();
    hasCursor_ = state.value(QStringLiteral("hasCursor"), false).toBool();
    cursorTimestamp_ = state.value(QStringLiteral("cursorTimestamp"), 0).toLongLong();
    hasMarker_ = state.value(QStringLiteral("hasMarker"), false).toBool();
    markerTimestamp_ = state.value(QStringLiteral("markerTimestamp"), 0).toLongLong();
    markerLogicalRow_ = state.value(QStringLiteral("markerLogicalRow"), -1).toInt();
    markerSpanIndex_ = state.value(QStringLiteral("markerSpanIndex"), -1).toInt();
    normalizeView();
    updateScrollBars();
    update();
}

QVariantMap CacheWidget::diagnosticsState() const
{
    QVariantMap state = viewState();
    const BoardLayout layout = boardLayout();
    state.insert(QStringLiteral("status"), statusText_);
    state.insert(QStringLiteral("building"), building_);
    state.insert(QStringLiteral("buildRequested"), buildRequested_);
    state.insert(QStringLiteral("processedRows"), QVariant::fromValue<qulonglong>(processedRows_));
    state.insert(QStringLiteral("totalRows"), QVariant::fromValue<qulonglong>(totalRows_));
    state.insert(QStringLiteral("minAddress"), QVariant::fromValue<qulonglong>(minAddress_));
    state.insert(QStringLiteral("maxAddress"), QVariant::fromValue<qulonglong>(maxAddress_));
    state.insert(QStringLiteral("firstTimestamp"), firstTimestamp_);
    state.insert(QStringLiteral("lastTimestamp"), lastTimestamp_);
    state.insert(QStringLiteral("visibleStart"), visibleTimestampRange().first);
    state.insert(QStringLiteral("visibleEnd"), visibleTimestampRange().second);
    state.insert(QStringLiteral("hoveredSpanIndex"), hoveredSpanIndex_);
    state.insert(QStringLiteral("usesDensePaint"), lastDensePaintUsed_);
    state.insert(QStringLiteral("denseTileCount"), static_cast<int>(denseSpanTileCache_.size()));
    state.insert(QStringLiteral("verticalBandScale"), verticalBandScale_);
    state.insert(QStringLiteral("verticalAddressZoom"), verticalAddressZoom_);
    state.insert(QStringLiteral("verticalCenterAddress"), static_cast<double>(verticalCenterAddress_));
    state.insert(QStringLiteral("visibleBandCount"), static_cast<int>(visibleBandIndices_.size()));
    state.insert(QStringLiteral("totalBandCount"), static_cast<int>(bands_.size()));
    state.insert(QStringLiteral("headerRect"), layout.headerRect);
    state.insert(QStringLiteral("topRulerRect"), layout.topRulerRect);
    state.insert(QStringLiteral("railRect"), layout.railRect);
    state.insert(QStringLiteral("plotRect"), layout.plotRect);
    state.insert(QStringLiteral("bottomRulerRect"), layout.bottomRulerRect);
    state.insert(QStringLiteral("horizontalScrollRect"), layout.horizontalScrollRect);
    state.insert(QStringLiteral("verticalScrollRect"), layout.verticalScrollRect);
    state.insert(QStringLiteral("addBandButtonRect"), addBandButtonRect());
    state.insert(QStringLiteral("plotLeft"), layout.plotRect.left());
    state.insert(QStringLiteral("plotTop"), layout.plotRect.top());
    state.insert(QStringLiteral("plotWidth"), layout.plotRect.width());
    state.insert(QStringLiteral("plotHeight"), layout.plotRect.height());
    state.insert(QStringLiteral("railWidth"), layout.railRect.width());
    state.insert(QStringLiteral("rowHeight"), scaledAddressRowHeight());
    state.insert(QStringLiteral("bandHeight"), scaledBandHeight());
    state.insert(QStringLiteral("bandHeaderHeight"), scaledBandTitleHeight());
    state.insert(QStringLiteral("verticalScrollOffset"), verticalScrollOffset_);
    if (!visibleBandIndices_.empty()) {
        const auto [visibleStart, visibleEnd] = visibleTimestampRange();
        const VisibleBandRows rows = visibleAddressRowsForBand(0, visibleStart, visibleEnd, layout.plotRect);
        state.insert(QStringLiteral("band0Rect"), bandRect(0, layout.plotRect));
        state.insert(QStringLiteral("band0RailRect"), bandRailRect(0, layout.plotRect));
        state.insert(QStringLiteral("band0HeaderRailRect"), bandHeaderRailRect(0, layout.plotRect));
        state.insert(QStringLiteral("band0TitleTextRect"), bandTitleTextRect(0, layout.plotRect));
        state.insert(QStringLiteral("band0DeleteButtonRect"), bandDeleteButtonRect(0));
        state.insert(QStringLiteral("band0AddressRailRect"), bandAddressRailRect(0, layout.plotRect));
        state.insert(QStringLiteral("band0HeaderPlotRect"), bandHeaderPlotRect(0, layout.plotRect));
        state.insert(QStringLiteral("band0AddressPlotRect"), bandAddressPlotRect(0, layout.plotRect));
        state.insert(QStringLiteral("band0DenseAddressMode"), rows.denseMode);
        state.insert(QStringLiteral("band0VisibleAddressRowCount"), static_cast<int>(rows.rows.size()));
        state.insert(QStringLiteral("band0TotalAddressRowCount"), rows.totalAddressRows);
        if (!rows.rows.empty()) {
            state.insert(QStringLiteral("band0FirstAddressRailRect"), rows.rows.front().railRect);
            state.insert(QStringLiteral("band0FirstAddressPlotRect"), rows.rows.front().plotRect);
        }
    }
    state.insert(QStringLiteral("buildGeneration"),
                 QVariant::fromValue<qulonglong>(buildGeneration_.load(std::memory_order_relaxed)));
    return state;
}

QSize CacheWidget::minimumSizeHint() const
{
    return QSize(460, 200);
}

QSize CacheWidget::sizeHint() const
{
    return QSize(980, 320);
}

void CacheWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateScrollBars();
}

void CacheWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), QColor(QStringLiteral("#FFFFFF")));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    paintHeader(painter);
    paintTopRuler(painter);
    paintAddressAxis(painter);
    paintSpans(painter);
    paintRangeZoom(painter);
    paintGestureOverlay(painter);
    paintFullRuler(painter);
    paintStatus(painter);
    paintBuildPrompt(painter);
    paintBuildProgress(painter);
}

void CacheWidget::wheelEvent(QWheelEvent* event)
{
    if (hasPendingBuild()) {
        event->ignore();
        return;
    }

    const QPoint pixelDelta = event->pixelDelta();
    const QPoint angleDelta = event->angleDelta();
    const int verticalDelta = pixelDelta.y() != 0 ? pixelDelta.y() : angleDelta.y();
    const int horizontalDelta = pixelDelta.x() != 0 ? pixelDelta.x() : angleDelta.x();
    const int zoomDelta = verticalDelta != 0 ? verticalDelta : horizontalDelta;

    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        if (zoomDelta == 0) {
            event->ignore();
            return;
        }
        zoomVerticallyByFactor(zoomDelta > 0 ? 1.25 : 0.8, event->position().toPoint());
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        if (zoomDelta == 0) {
            event->ignore();
            return;
        }
        zoomByFactor(zoomDelta > 0 ? 1.25 : 0.8, event->position().toPoint());
        event->accept();
        return;
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        const int delta = verticalDelta == 0 ? horizontalDelta : verticalDelta;
        panHorizontallyByPixels(delta < 0 ? timelineViewportWidth() / 4 : -timelineViewportWidth() / 4);
        event->accept();
        return;
    }
    if (horizontalDelta != 0) {
        panHorizontallyByPixels(horizontalDelta < 0 ? 80 : -80);
        event->accept();
        return;
    }
    if (verticalDelta != 0) {
        scrollVerticallyByPixels(verticalDelta < 0 ? 80 : -80);
        event->accept();
        return;
    }
    event->ignore();
}

void CacheWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    dragStart_ = event->pos();
    dragLast_ = event->pos();
    dragRect_ = QRect();

    if (shouldShowBuildPrompt() && event->button() == Qt::LeftButton) {
        if (buildPromptButtonRect().contains(event->pos())) {
            buildView();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && addBandButtonRect().contains(event->pos())) {
        showAddBandMenu();
        event->accept();
        return;
    }

    if (hasPendingBuild()) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton && !visibleBandIndices_.empty()) {
        for (int visibleBand = 0; visibleBand < static_cast<int>(visibleBandIndices_.size()); ++visibleBand) {
            if (bandDeleteButtonRect(visibleBand).contains(event->pos())) {
                removeVisibleBandAt(visibleBand);
                event->accept();
                return;
            }
        }
    }

    if ((event->button() == Qt::MiddleButton)
        || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))) {
        beginPan(event->pos());
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton
        && (positionInTimeline(event->pos()) || positionInRuler(event->pos()))) {
        beginPendingGesture(event->pos());
        event->accept();
        return;
    }

    event->ignore();
}

void CacheWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton
        && !addBandButtonRect().contains(event->pos())
        && (positionInTimeline(event->pos()) || positionInRuler(event->pos()))) {
        zoomToFit();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CacheWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragMode_ == DragMode::PendingGesture) {
        updatePendingGesture(event->pos(), event->modifiers());
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::RangeZoom) {
        updateRangeZoom(event->pos());
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::ZoomFull
        || dragMode_ == DragMode::ZoomIn2x
        || dragMode_ == DragMode::ZoomOut2x) {
        dragLast_ = event->pos();
        dragRect_ = QRect(dragStart_, dragLast_).normalized();
        update();
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::Pan) {
        updatePan(event->pos());
        event->accept();
        return;
    }

    const int oldHover = hoveredSpanIndex_;
    hoveredSpanIndex_ = spanIndexAtPosition(event->pos()).value_or(-1);
    if (oldHover != hoveredSpanIndex_) {
        setToolTip(hoveredSpanIndex_ >= 0 ? spanTooltipText(hoveredSpanIndex_) : QString());
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CacheWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragMode_ == DragMode::PendingGesture && event->button() == Qt::LeftButton) {
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        if (const std::optional<int> spanIndex = spanIndexAtPosition(event->pos())) {
            setCursorFromSpan(*spanIndex, true);
        } else {
            update();
        }
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::RangeZoom && event->button() == Qt::LeftButton) {
        updateRangeZoom(event->pos());
        finishRangeZoom();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::ZoomFull && event->button() == Qt::LeftButton) {
        dragLast_ = event->pos();
        finishZoomFullGesture();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::ZoomIn2x && event->button() == Qt::LeftButton) {
        dragLast_ = event->pos();
        finishZoomIn2xGesture();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::ZoomOut2x && event->button() == Qt::LeftButton) {
        dragLast_ = event->pos();
        finishZoomOut2xGesture();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        event->accept();
        return;
    }
    if (dragMode_ == DragMode::Pan
        && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        unsetCursor();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void CacheWidget::leaveEvent(QEvent* event)
{
    if (hoveredSpanIndex_ >= 0) {
        hoveredSpanIndex_ = -1;
        setToolTip(QString());
        update();
    }
    QWidget::leaveEvent(event);
}

void CacheWidget::keyPressEvent(QKeyEvent* event)
{
    if (hasPendingBuild()) {
        if (event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Space) {
            buildView();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
        return;
    }

    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    switch (event->key()) {
    case Qt::Key_F:
    case Qt::Key_0:
        zoomToFit();
        event->accept();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomByFactor(1.25, plotRect().center());
        event->accept();
        return;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        zoomByFactor(0.8, plotRect().center());
        event->accept();
        return;
    case Qt::Key_Left:
        if (ctrl) {
            moveCursorToAdjacentSpan(-1);
        } else {
            panHorizontallyByPixels(shift ? -timelineViewportWidth() / 2 : -80);
        }
        event->accept();
        return;
    case Qt::Key_Right:
        if (ctrl) {
            moveCursorToAdjacentSpan(1);
        } else {
            panHorizontallyByPixels(shift ? timelineViewportWidth() / 2 : 80);
        }
        event->accept();
        return;
    case Qt::Key_Up:
        scrollVerticallyByPixels(-80);
        event->accept();
        return;
    case Qt::Key_Down:
        scrollVerticallyByPixels(80);
        event->accept();
        return;
    case Qt::Key_PageUp:
        panHorizontallyByPixels(-qMax(1, timelineViewportWidth() - 40));
        event->accept();
        return;
    case Qt::Key_PageDown:
        panHorizontallyByPixels(qMax(1, timelineViewportWidth() - 40));
        event->accept();
        return;
    case Qt::Key_Home:
        if (ctrl) {
            zoomToFit();
        }
        scrollToHorizontalOffset(0);
        event->accept();
        return;
    case Qt::Key_End:
        if (ctrl) {
            zoomToFit();
        }
        scrollToHorizontalOffset(horizontalContentRange());
        event->accept();
        return;
    case Qt::Key_B:
        if (ctrl) {
            restorePreviousView();
            event->accept();
            return;
        }
        break;
    case Qt::Key_M:
        if (selectedLogicalRow_ >= 0) {
            setMarkerLogicalRow(selectedLogicalRow_);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Escape:
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        hoveredSpanIndex_ = -1;
        update();
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void CacheWidget::stopBuildThread()
{
    ++buildGeneration_;
    if (buildThread_.joinable()) {
        buildThread_.request_stop();
        buildThread_.join();
    }
    building_ = false;
    progressUpdateQueued_.store(false, std::memory_order_release);
}

void CacheWidget::startSessionBuild(std::shared_ptr<TraceSession> session)
{
    const quint64 generation = ++buildGeneration_;
    buildThread_ = std::jthread([this, session = std::move(session), generation](const std::stop_token stopToken) {
        std::shared_ptr<BuildResult> result = buildSpansFromSession(session, generation, stopToken);
        if (stopToken.stop_requested()) {
            return;
        }
        QTimer::singleShot(0, this, [this, result = std::move(result), generation]() {
            if (generation == buildGeneration_.load(std::memory_order_relaxed)) {
                applyBuildResult(result);
            }
        });
    });
}

std::shared_ptr<CacheWidget::BuildResult> CacheWidget::buildSpansFromSession(
    const std::shared_ptr<TraceSession>& session,
    const quint64 generation,
    const std::stop_token stopToken)
{
    auto result = std::make_shared<BuildResult>();
    result->generation = generation;
    result->totalRows = session ? session->totalRows() : 0;

    if (!session) {
        result->statusText = QStringLiteral("Open a trace to populate Cache.");
        return result;
    }
    if (!traceSupportsCacheReplay(session)) {
        result->statusText = QStringLiteral("Cache unavailable: %1 traces are not supported; open a CHI E.b trace.")
                                 .arg(issueLabel(session->metadata().parameters.GetIssue()));
        return result;
    }

    const CLogBTraceMetadata metadata = session->metadata();
    std::vector<CLogBTraceCacheLineLifetime> lifetimes;
    lifetimes.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(metadata.totalRecords / 4U + 16U,
                                                                       2'000'000U)));
    CLogBTraceLoadError error;
    CLogBTraceLoadCallbacks callbacks;
    callbacks.stageProgress = [this, generation](const std::size_t completed, const std::size_t total) {
        postBuildProgress(generation,
                          static_cast<std::uint64_t>(completed),
                          static_cast<std::uint64_t>(total));
    };
    callbacks.progress = [this, generation, totalRows = metadata.totalRecords](
                             const std::uint64_t processedBytes,
                             const std::uint64_t totalBytes) {
        if (totalBytes == 0) {
            return;
        }
        const std::uint64_t processedRows =
            (processedBytes * std::max<std::uint64_t>(1, totalRows)) / totalBytes;
        postBuildProgress(generation, std::min(processedRows, totalRows), totalRows);
    };

    const bool ok = CLogBTraceLoader::buildCacheLineLifetimes(session->filePath(),
                                                              metadata,
                                                              lifetimes,
                                                              error,
                                                              callbacks,
                                                              stopToken);
    if (stopToken.stop_requested()
        || generation != buildGeneration_.load(std::memory_order_relaxed)) {
        result->cancelled = true;
        return result;
    }
    if (!ok) {
        result->failed = true;
        result->statusText = QStringLiteral("Cache unavailable: %1")
                                 .arg(error.summary.isEmpty()
                                          ? QStringLiteral("failed to replay RN cache states")
                                          : error.summary);
        return result;
    }

    finalizeBuildResult(*result, metadata, std::move(lifetimes));
    result->processedRows = result->totalRows;
    return result;
}

void CacheWidget::finalizeBuildResult(BuildResult& result,
                                      const CLogBTraceMetadata& metadata,
                                      std::vector<CLogBTraceCacheLineLifetime> lifetimes)
{
    result.firstTimestamp = metadata.firstTimestamp;
    result.lastTimestamp = metadata.lastTimestamp;
    result.haveTimeRange = metadata.totalRecords > 0;
    result.totalRows = metadata.totalRecords;

    std::map<std::uint32_t, CacheRnBand> bandsByNode;
    for (const auto& lifetime : lifetimes) {
        CacheRnBand& band = bandsByNode[lifetime.rnNodeId];
        band.rnNodeId = lifetime.rnNodeId;
        band.nodeType = lifetime.rnNodeType.trimmed().isEmpty() ? QStringLiteral("RN")
                                                                : lifetime.rnNodeType.trimmed();
        const auto annotation = metadata.nodeAnnotations.find(static_cast<quint16>(lifetime.rnNodeId));
        const QString base = QStringLiteral("RN %1").arg(lifetime.rnNodeId);
        band.topologyLabel = annotation == metadata.nodeAnnotations.end() || annotation->second.trimmed().isEmpty()
            ? base
            : QStringLiteral("%1  %2").arg(base, annotation->second.trimmed());
        ++band.spanCount;

        if (!result.haveAddressRange) {
            result.minAddress = lifetime.lineAddress;
            result.maxAddress = lifetime.lineAddress;
            result.haveAddressRange = true;
        } else {
            result.minAddress = std::min(result.minAddress, lifetime.lineAddress);
            result.maxAddress = std::max(result.maxAddress, lifetime.lineAddress);
        }
    }

    result.bands.reserve(bandsByNode.size());
    std::unordered_map<std::uint32_t, int> bandIndexByNode;
    int bandIndex = 0;
    for (auto& [nodeId, band] : bandsByNode) {
        bandIndexByNode.emplace(nodeId, bandIndex++);
        result.bands.push_back(std::move(band));
    }

    result.spans.reserve(lifetimes.size());
    for (const CLogBTraceCacheLineLifetime& lifetime : lifetimes) {
        const auto bandFound = bandIndexByNode.find(lifetime.rnNodeId);
        if (bandFound == bandIndexByNode.end()) {
            continue;
        }
        CacheLifetimeSpan span;
        span.rnBandIndex = bandFound->second;
        span.rnNodeId = lifetime.rnNodeId;
        span.lineAddress = lifetime.lineAddress;
        span.startTimestamp = lifetime.startTimestamp;
        span.endTimestamp = std::max(lifetime.startTimestamp, lifetime.endTimestamp);
        span.startLogicalRow = lifetime.startLogicalRow;
        span.endLogicalRow = std::max(lifetime.startLogicalRow, lifetime.endLogicalRow);
        span.startStateMask = lifetime.startStateMask;
        span.endStateMask = lifetime.endStateMask;
        span.startStateText = lifetime.startStateText;
        span.endStateText = lifetime.endStateText;
        span.openEnded = lifetime.openEnded;
        result.spans.push_back(std::move(span));
    }

    std::sort(result.spans.begin(), result.spans.end(), [](const CacheLifetimeSpan& lhs,
                                                           const CacheLifetimeSpan& rhs) {
        if (lhs.rnBandIndex != rhs.rnBandIndex) {
            return lhs.rnBandIndex < rhs.rnBandIndex;
        }
        if (lhs.startTimestamp != rhs.startTimestamp) {
            return lhs.startTimestamp < rhs.startTimestamp;
        }
        if (lhs.endTimestamp != rhs.endTimestamp) {
            return lhs.endTimestamp < rhs.endTimestamp;
        }
        return lhs.lineAddress < rhs.lineAddress;
    });

    if (result.spans.empty()) {
        result.statusText = result.totalRows == 0
            ? QStringLiteral("No flits in the current trace.")
            : QStringLiteral("Cache ready: no non-invalid RN cache-line lifetimes were observed.");
    } else {
        result.statusText = QStringLiteral("Cache ready: %1 lifetimes across %2 RN bands.")
                                .arg(FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(result.spans.size())),
                                     FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(result.bands.size())));
    }
}

void CacheWidget::applyBuildResult(std::shared_ptr<BuildResult> result)
{
    progressUpdateQueued_.store(false, std::memory_order_release);
    if (!result || result->generation != buildGeneration_.load(std::memory_order_relaxed) || result->cancelled) {
        return;
    }

    building_ = false;
    totalRows_ = result->totalRows;
    processedRows_ = result->processedRows;
    if (result->failed) {
        bands_.clear();
        spans_.clear();
        spanIndicesByBandStart_.clear();
        spanIndicesByBandEnd_.clear();
        statusText_ = result->statusText;
        updateScrollBars();
        update();
        return;
    }

    bands_ = std::move(result->bands);
    spans_ = std::move(result->spans);
    visibleBandIndices_.clear();
    minAddress_ = result->minAddress;
    maxAddress_ = result->maxAddress;
    firstTimestamp_ = result->firstTimestamp;
    lastTimestamp_ = result->lastTimestamp;
    haveAddressRange_ = result->haveAddressRange;
    haveTimeRange_ = result->haveTimeRange;
    resetVerticalAddressView();
    statusText_ = result->statusText;
    rebuildSpanIndexes();
    normalizeView();
    if (selectedLogicalRow_ >= 0) {
        setSelectedLogicalRow(selectedLogicalRow_);
    }
    updateScrollBars();
    updateGeometry();
    update();

    if (!pendingViewState_.isEmpty()) {
        const QVariantMap state = pendingViewState_;
        pendingViewState_.clear();
        restoreViewState(state);
    }
}

void CacheWidget::postBuildProgress(const quint64 generation,
                                    const std::uint64_t processedRows,
                                    const std::uint64_t totalRows)
{
    if (generation != buildGeneration_.load(std::memory_order_relaxed)) {
        return;
    }
    pendingProgressProcessedRows_.store(processedRows, std::memory_order_relaxed);
    pendingProgressTotalRows_.store(totalRows, std::memory_order_relaxed);
    pendingProgressGeneration_.store(generation, std::memory_order_release);

    bool expected = false;
    if (!progressUpdateQueued_.compare_exchange_strong(expected,
                                                       true,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_relaxed)) {
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        const quint64 generation = pendingProgressGeneration_.load(std::memory_order_acquire);
        const std::uint64_t processed = pendingProgressProcessedRows_.load(std::memory_order_relaxed);
        const std::uint64_t total = pendingProgressTotalRows_.load(std::memory_order_relaxed);
        progressUpdateQueued_.store(false, std::memory_order_release);
        updateBuildProgress(generation, processed, total);
    });
}

void CacheWidget::updateBuildProgress(const quint64 generation,
                                      const std::uint64_t processedRows,
                                      const std::uint64_t totalRows)
{
    if (generation != buildGeneration_.load(std::memory_order_relaxed) || !building_) {
        return;
    }
    processedRows_ = totalRows == 0 ? processedRows : std::min(processedRows, totalRows);
    totalRows_ = totalRows;
    statusText_ = QStringLiteral("Cache: replaying RN coherency state... %1 / %2 rows")
                      .arg(FormatUnsignedIntegerWithThousands(processedRows_),
                           FormatUnsignedIntegerWithThousands(std::max<std::uint64_t>(1, totalRows_)));
    update();
}

bool CacheWidget::hasPendingBuild() const noexcept
{
    return shouldShowBuildPrompt();
}

bool CacheWidget::shouldShowBuildPrompt() const noexcept
{
    return sourceMode_ == SourceMode::TraceSession
        && traceSupportsCacheReplay(traceSession_)
        && !building_
        && !buildRequested_
        && spans_.empty();
}

QString CacheWidget::pendingBuildText() const
{
    const QString rows = FormatUnsignedIntegerWithThousands(totalRows_);
    return QStringLiteral("Cache lifetimes are built on demand from CHI E.b RN cache-state replay (%1 rows).")
        .arg(rows);
}

QRect CacheWidget::buildPromptButtonRect() const
{
    const QRect bounds = headerRect();
    const int width = 130;
    const int height = 26;
    return QRect(bounds.right() - width - 14,
                 bounds.top() + (bounds.height() - height) / 2,
                 width,
                 height);
}

void CacheWidget::rebuildSpanIndexes()
{
    spanIndicesByBandStart_.assign(bands_.size(), {});
    spanIndicesByBandEnd_.assign(bands_.size(), {});
    for (int index = 0; index < static_cast<int>(spans_.size()); ++index) {
        const int band = spans_[static_cast<std::size_t>(index)].rnBandIndex;
        if (band < 0 || band >= static_cast<int>(bands_.size())) {
            continue;
        }
        spanIndicesByBandStart_[static_cast<std::size_t>(band)].push_back(index);
        spanIndicesByBandEnd_[static_cast<std::size_t>(band)].push_back(index);
    }
    for (std::size_t band = 0; band < spanIndicesByBandStart_.size(); ++band) {
        auto& byStart = spanIndicesByBandStart_[band];
        std::sort(byStart.begin(), byStart.end(), [this](const int lhs, const int rhs) {
            const CacheLifetimeSpan& a = spans_[static_cast<std::size_t>(lhs)];
            const CacheLifetimeSpan& b = spans_[static_cast<std::size_t>(rhs)];
            if (a.startTimestamp != b.startTimestamp) {
                return a.startTimestamp < b.startTimestamp;
            }
            return a.endTimestamp < b.endTimestamp;
        });
        auto& byEnd = spanIndicesByBandEnd_[band];
        std::sort(byEnd.begin(), byEnd.end(), [this](const int lhs, const int rhs) {
            const CacheLifetimeSpan& a = spans_[static_cast<std::size_t>(lhs)];
            const CacheLifetimeSpan& b = spans_[static_cast<std::size_t>(rhs)];
            if (a.endTimestamp != b.endTimestamp) {
                return a.endTimestamp < b.endTimestamp;
            }
            return a.startTimestamp < b.startTimestamp;
        });
    }
    clearDenseTileCache();
}

void CacheWidget::updateScrollBars()
{
    updateScrollBarGeometry();
    normalizeView();

    const std::uint64_t hRange = horizontalContentRange();
    const int hMax = intScrollRangeFromU64(hRange);
    horizontalScrollBar_->setRange(0, hMax);
    horizontalScrollBar_->setPageStep(std::max(1, timelineViewportWidth()));
    horizontalScrollBar_->setSingleStep(80);
    horizontalScrollBar_->setValue(static_cast<int>(std::min<std::uint64_t>(scrollOffset_, hMax)));
    horizontalScrollBar_->setVisible(hRange > 0);

    const int vRange = verticalContentRange();
    verticalScrollBar_->setRange(0, vRange);
    verticalScrollBar_->setPageStep(std::max(1, timelineViewportHeight()));
    verticalScrollBar_->setSingleStep(48);
    verticalScrollBar_->setValue(std::clamp(verticalScrollOffset_, 0, vRange));
    verticalScrollBar_->setVisible(vRange > 0);
}

void CacheWidget::updateScrollBarGeometry()
{
    if (horizontalScrollBar_) {
        horizontalScrollBar_->setGeometry(horizontalScrollRect());
    }
    if (verticalScrollBar_) {
        verticalScrollBar_->setGeometry(verticalScrollRect());
    }
}

void CacheWidget::normalizeView()
{
    horizontalZoom_ = std::clamp(horizontalZoom_, kMinZoom, kMaxZoom);
    normalizeVerticalView();
    visibleBandIndices_.erase(
        std::remove_if(visibleBandIndices_.begin(), visibleBandIndices_.end(), [this](const int sourceBandIndex) {
            return sourceBandIndex < 0 || sourceBandIndex >= static_cast<int>(bands_.size());
        }),
        visibleBandIndices_.end());
    std::vector<int> uniqueVisible;
    uniqueVisible.reserve(visibleBandIndices_.size());
    for (const int sourceBandIndex : visibleBandIndices_) {
        if (std::find(uniqueVisible.cbegin(), uniqueVisible.cend(), sourceBandIndex) == uniqueVisible.cend()) {
            uniqueVisible.push_back(sourceBandIndex);
        }
    }
    visibleBandIndices_ = std::move(uniqueVisible);
    scrollOffset_ = clampOffsetToRange(scrollOffset_, horizontalContentRange());
    verticalScrollOffset_ = std::clamp(verticalScrollOffset_, 0, verticalContentRange());
    if (selectedSpanIndex_ >= static_cast<int>(spans_.size())) {
        selectedSpanIndex_ = -1;
    }
    if (markerSpanIndex_ >= static_cast<int>(spans_.size())) {
        markerSpanIndex_ = -1;
    }
}

void CacheWidget::resetView()
{
    minAddress_ = 0;
    maxAddress_ = 0;
    firstTimestamp_ = 0;
    lastTimestamp_ = 0;
    haveAddressRange_ = false;
    haveTimeRange_ = false;
    horizontalZoom_ = 1.0;
    verticalBandScale_ = 1.0;
    verticalAddressZoom_ = 1.0;
    verticalCenterAddress_ = 0.0L;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    selectedLogicalRow_ = -1;
    selectedSpanIndex_ = -1;
    hasCursor_ = false;
    cursorTimestamp_ = 0;
    hasMarker_ = false;
    markerTimestamp_ = 0;
    markerLogicalRow_ = -1;
    markerSpanIndex_ = -1;
    hoveredSpanIndex_ = -1;
    dragMode_ = DragMode::None;
    leftDragGestureActive_ = false;
    clearDenseTileCache();
}

CacheWidget::BoardLayout CacheWidget::boardLayout() const
{
    BoardLayout layout;
    layout.headerRect = QRect(0, 0, width(), kHeaderHeight);
    const int railWidth = std::min(kAxisWidth, std::max(150, width() - kScrollBarExtent - 140));
    const int timelineWidth = qMax(0, width() - railWidth - kScrollBarExtent);
    const int top = layout.headerRect.bottom() + 1;
    layout.topRulerRect = QRect(railWidth, top, timelineWidth, kTopRulerHeight);
    const int plotTop = layout.topRulerRect.bottom() + 1;
    const int bottomReserve = kBottomRulerHeight + kScrollBarExtent;
    const int plotHeight = qMax(0, height() - plotTop - bottomReserve);
    layout.railRect = QRect(0, plotTop, railWidth, plotHeight);
    layout.plotRect = QRect(railWidth, plotTop, timelineWidth, plotHeight);
    layout.bottomRulerRect = QRect(railWidth,
                                   layout.plotRect.bottom() + 1,
                                   timelineWidth,
                                   kBottomRulerHeight);
    layout.horizontalScrollRect = QRect(railWidth,
                                        height() - kScrollBarExtent,
                                        timelineWidth,
                                        kScrollBarExtent);
    layout.verticalScrollRect = QRect(width() - kScrollBarExtent,
                                      plotTop,
                                      kScrollBarExtent,
                                      plotHeight);
    return layout;
}

QRect CacheWidget::headerRect() const
{
    return boardLayout().headerRect;
}

QRect CacheWidget::topRulerRect() const
{
    return boardLayout().topRulerRect;
}

QRect CacheWidget::addressAxisRect() const
{
    return boardLayout().railRect;
}

QRect CacheWidget::plotRect() const
{
    return boardLayout().plotRect;
}

QRect CacheWidget::fullRulerRect() const
{
    return boardLayout().bottomRulerRect;
}

QRect CacheWidget::horizontalScrollRect() const
{
    return boardLayout().horizontalScrollRect;
}

QRect CacheWidget::verticalScrollRect() const
{
    return boardLayout().verticalScrollRect;
}

QRect CacheWidget::addBandButtonRect() const
{
    const QRect header = headerRect();
    const int size = 26;
    return QRect(header.right() - size - kScrollBarExtent - 14,
                 header.top() + (header.height() - size) / 2,
                 size,
                 size);
}

QRect CacheWidget::bandDeleteButtonRect(const int visibleBandIndex) const
{
    const QRect plot = plotRect();
    const QRect header = bandHeaderRailRect(visibleBandIndex, plot);
    if (!header.isValid() || !header.intersects(addressAxisRect())) {
        return QRect();
    }
    const int size = 18;
    return QRect(header.right() - size - 8,
                 header.top() + 6,
                 size,
                 size);
}

int CacheWidget::timelineViewportWidth() const
{
    return plotRect().width();
}

int CacheWidget::timelineViewportHeight() const
{
    return plotRect().height();
}

std::uint64_t CacheWidget::timelineContentWidth() const
{
    const int viewport = std::max(1, timelineViewportWidth());
    const long double widthValue =
        static_cast<long double>(viewport) * static_cast<long double>(horizontalZoom_);
    return static_cast<std::uint64_t>(
        std::clamp<long double>(std::llround(widthValue),
                                viewport,
                                static_cast<long double>((std::numeric_limits<int>::max)())));
}

std::uint64_t CacheWidget::horizontalContentRange() const
{
    const std::uint64_t content = timelineContentWidth();
    const std::uint64_t viewport = static_cast<std::uint64_t>(std::max(1, timelineViewportWidth()));
    return content > viewport ? content - viewport : 0;
}

int CacheWidget::verticalContentHeight() const
{
    if (visibleBandIndices_.empty()) {
        return 0;
    }
    return static_cast<int>(visibleBandIndices_.size()) * (scaledBandHeight() + scaledBandGap())
        - scaledBandGap();
}

int CacheWidget::verticalContentRange() const
{
    return std::max(0, verticalContentHeight() - timelineViewportHeight());
}

int CacheWidget::scaledBandHeight() const
{
    return std::max(42, static_cast<int>(std::llround(kBandHeight * verticalBandScale_)));
}

int CacheWidget::scaledBandGap() const
{
    return std::max(4, static_cast<int>(std::llround(kBandGap * std::sqrt(verticalBandScale_))));
}

int CacheWidget::scaledBandTitleHeight() const
{
    return std::clamp(static_cast<int>(std::llround(kBandTitleHeight * std::min(1.35, verticalBandScale_))),
                      34,
                      std::max(34, scaledBandHeight() - 28));
}

int CacheWidget::scaledAddressRowHeight() const
{
    return std::clamp(static_cast<int>(std::llround(kAddressRowHeight * verticalBandScale_)),
                      12,
                      44);
}

std::pair<long double, long double> CacheWidget::globalAddressViewRange() const
{
    if (!haveAddressRange_) {
        return {0.0L, 0.0L};
    }
    if (minAddress_ == maxAddress_) {
        const long double address = static_cast<long double>(minAddress_);
        const long double margin = std::max<long double>(64.0L, std::abs(address) * kAddressGlobalMarginFraction);
        return {std::max<long double>(0.0L, address - margin), address + margin};
    }
    const long double minAddress = static_cast<long double>(minAddress_);
    const long double maxAddress = static_cast<long double>(maxAddress_);
    const long double margin = std::max<long double>(64.0L, (maxAddress - minAddress) * kAddressGlobalMarginFraction);
    return {std::max<long double>(0.0L, minAddress - margin), maxAddress + margin};
}

std::pair<long double, long double> CacheWidget::visibleAddressRange() const
{
    if (!haveAddressRange_) {
        return {0.0L, 0.0L};
    }
    const auto [globalMin, globalMax] = globalAddressViewRange();
    if (globalMax <= globalMin) {
        return {globalMin, globalMax};
    }
    const double zoom = std::clamp(verticalAddressZoom_, kMinVerticalAddressZoom, kMaxVerticalAddressZoom);
    const long double span = (globalMax - globalMin) / static_cast<long double>(zoom);
    const long double center = verticalCenterAddress_ == 0.0L
        ? (globalMin + globalMax) / 2.0L
        : verticalCenterAddress_;
    long double visibleMin = center - span / 2.0L;
    long double visibleMax = center + span / 2.0L;
    if (visibleMin < globalMin) {
        visibleMax += globalMin - visibleMin;
        visibleMin = globalMin;
    }
    if (visibleMax > globalMax) {
        visibleMin -= visibleMax - globalMax;
        visibleMax = globalMax;
    }
    visibleMin = std::max(globalMin, visibleMin);
    visibleMax = std::min(globalMax, visibleMax);
    return {visibleMin, visibleMax};
}

std::uint64_t CacheWidget::addressFromVisibleRatio(const long double ratio) const
{
    if (!haveAddressRange_) {
        return minAddress_;
    }
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    const long double clampedRatio = std::clamp<long double>(ratio, 0.0L, 1.0L);
    const long double address = visibleMin + clampedRatio * (visibleMax - visibleMin);
    return static_cast<std::uint64_t>(std::llround(std::max<long double>(0.0L, address)));
}

void CacheWidget::resetVerticalAddressView()
{
    verticalAddressZoom_ = 1.0;
    if (!haveAddressRange_) {
        verticalCenterAddress_ = 0.0L;
        return;
    }
    const auto [globalMin, globalMax] = globalAddressViewRange();
    verticalCenterAddress_ = (globalMin + globalMax) / 2.0L;
    normalizeVerticalView();
}

void CacheWidget::normalizeVerticalView()
{
    verticalBandScale_ = std::clamp(verticalBandScale_, kMinVerticalBandScale, kMaxVerticalBandScale);
    verticalAddressZoom_ = std::clamp(verticalAddressZoom_, kMinVerticalAddressZoom, kMaxVerticalAddressZoom);
    if (!haveAddressRange_) {
        verticalCenterAddress_ = 0.0L;
        return;
    }
    const auto [globalMin, globalMax] = globalAddressViewRange();
    if (globalMax <= globalMin) {
        verticalCenterAddress_ = (globalMin + globalMax) / 2.0L;
        return;
    }
    const long double span = (globalMax - globalMin) / static_cast<long double>(verticalAddressZoom_);
    const long double halfSpan = span / 2.0L;
    verticalCenterAddress_ = std::clamp(verticalCenterAddress_,
                                        globalMin + halfSpan,
                                        globalMax - halfSpan);
}

std::uint64_t CacheWidget::contentXForTimestamp(const qint64 timestamp) const
{
    if (!haveTimeRange_ || firstTimestamp_ == lastTimestamp_) {
        return 0;
    }
    const qint64 clamped = clampTimestamp(timestamp, firstTimestamp_, lastTimestamp_);
    const long double span = static_cast<long double>(lastTimestamp_ - firstTimestamp_);
    const long double ratio = static_cast<long double>(clamped - firstTimestamp_) / span;
    return static_cast<std::uint64_t>(
        std::llround(ratio * static_cast<long double>(std::max<std::uint64_t>(1, timelineContentWidth() - 1))));
}

qint64 CacheWidget::timestampForContentX(const std::uint64_t contentX) const
{
    if (!haveTimeRange_ || firstTimestamp_ == lastTimestamp_) {
        return firstTimestamp_;
    }
    const long double denominator = static_cast<long double>(std::max<std::uint64_t>(1, timelineContentWidth() - 1));
    const long double ratio = static_cast<long double>(std::min(contentX, timelineContentWidth() - 1)) / denominator;
    return firstTimestamp_ + static_cast<qint64>(
        std::llround(ratio * static_cast<long double>(lastTimestamp_ - firstTimestamp_)));
}

std::uint64_t CacheWidget::contentXAtTimelineX(const int x) const
{
    const QRect plot = plotRect();
    const int localX = std::clamp(x - plot.left(), 0, std::max(0, plot.width() - 1));
    return scrollOffset_ + static_cast<std::uint64_t>(localX);
}

std::optional<int> CacheWidget::screenXForTimestamp(const qint64 timestamp, const QRect& area) const
{
    const std::uint64_t contentX = contentXForTimestamp(timestamp);
    if (contentX + 1 < scrollOffset_) {
        return std::nullopt;
    }
    const std::uint64_t local = contentX >= scrollOffset_ ? contentX - scrollOffset_ : 0;
    if (local > static_cast<std::uint64_t>(std::max(0, area.width() + 1))) {
        return std::nullopt;
    }
    return area.left() + static_cast<int>(std::min<std::uint64_t>(local, std::numeric_limits<int>::max()));
}

qint64 CacheWidget::timestampAtTimelineX(const int x) const
{
    return timestampForContentX(contentXAtTimelineX(x));
}

std::pair<qint64, qint64> CacheWidget::visibleTimestampRange() const
{
    if (!haveTimeRange_) {
        return {0, 0};
    }
    const QRect plot = plotRect();
    const qint64 first = timestampForContentX(scrollOffset_);
    const qint64 last = timestampForContentX(scrollOffset_ + static_cast<std::uint64_t>(std::max(0, plot.width() - 1)));
    return {std::min(first, last), std::max(first, last)};
}

QRect CacheWidget::bandRect(const int visibleBandIndex, const QRect& plot) const
{
    const int y = plot.top() + visibleBandIndex * (scaledBandHeight() + scaledBandGap()) - verticalScrollOffset_;
    return QRect(plot.left(), y, plot.width(), scaledBandHeight());
}

QRect CacheWidget::bandAddressBodyRect(const int visibleBandIndex, const QRect& plot) const
{
    return bandAddressPlotRect(visibleBandIndex, plot);
}

QRect CacheWidget::bandRailRect(const int visibleBandIndex, const QRect& plot) const
{
    const QRect axis = addressAxisRect();
    const QRect band = bandRect(visibleBandIndex, plot);
    return QRect(axis.left(), band.top(), axis.width(), band.height());
}

QRect CacheWidget::bandHeaderRailRect(const int visibleBandIndex, const QRect& plot) const
{
    const QRect rail = bandRailRect(visibleBandIndex, plot);
    return QRect(rail.left(), rail.top(), rail.width(), std::min(scaledBandTitleHeight(), rail.height()));
}

QRect CacheWidget::bandTitleTextRect(const int visibleBandIndex, const QRect& plot) const
{
    const QRect header = bandHeaderRailRect(visibleBandIndex, plot);
    const QRect deleteButton = bandDeleteButtonRect(visibleBandIndex);
    const int right = deleteButton.isValid() ? deleteButton.left() - 8 : header.right() - 8;
    return QRect(header.left() + 12,
                 header.top() + 4,
                 std::max(0, right - header.left() - 12),
                 18);
}

QRect CacheWidget::bandAddressRailRect(const int visibleBandIndex, const QRect& plot) const
{
    const QRect rail = bandRailRect(visibleBandIndex, plot);
    return rail.adjusted(0, scaledBandTitleHeight(), 0, -6);
}

QRect CacheWidget::bandHeaderPlotRect(const int visibleBandIndex, const QRect& plot) const
{
    const QRect band = bandRect(visibleBandIndex, plot);
    return QRect(band.left(), band.top(), band.width(), std::min(scaledBandTitleHeight(), band.height()));
}

QRect CacheWidget::bandAddressPlotRect(const int visibleBandIndex, const QRect& plot) const
{
    const QRect band = bandRect(visibleBandIndex, plot);
    return band.adjusted(0, scaledBandTitleHeight(), 0, -6);
}

CacheWidget::VisibleBandRows CacheWidget::visibleAddressRowsForBand(const int visibleBandIndex,
                                                                     const qint64 visibleStart,
                                                                     const qint64 visibleEnd,
                                                                     const QRect& plot) const
{
    VisibleBandRows result;
    const int sourceBandIndex = sourceBandForVisibleSlot(visibleBandIndex);
    if (sourceBandIndex < 0) {
        return result;
    }
    const QRect bodyPlot = bandAddressPlotRect(visibleBandIndex, plot);
    const QRect bodyRail = bandAddressRailRect(visibleBandIndex, plot);
    if (bodyPlot.height() <= 0 || bodyRail.height() <= 0) {
        return result;
    }

    std::map<std::uint64_t, int> addressCounts;
    const std::vector<int> candidates =
        visibleSpanIndicesForSourceBand(visibleStart, visibleEnd, sourceBandIndex);
    for (const int spanIndex : candidates) {
        if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
            continue;
        }
        const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
        ++addressCounts[span.lineAddress];
    }
    result.totalAddressRows = static_cast<int>(addressCounts.size());
    if (addressCounts.empty()) {
        return result;
    }

    const int rowHeight = scaledAddressRowHeight();
    const int rowCapacity = std::max(1, bodyPlot.height() / std::max(1, rowHeight));
    result.denseMode = result.totalAddressRows > rowCapacity;
    if (result.denseMode) {
        return result;
    }

    result.rows.reserve(static_cast<std::size_t>(result.totalAddressRows));
    int y = bodyPlot.top();
    for (const auto& [address, count] : addressCounts) {
        if (y > bodyPlot.bottom()) {
            break;
        }
        const int height = std::min(rowHeight, bodyPlot.bottom() - y + 1);
        if (height <= 0) {
            break;
        }
        VisibleAddressRow row;
        row.address = address;
        row.spanCount = count;
        row.railRect = QRect(bodyRail.left(), y, bodyRail.width(), height);
        row.plotRect = QRect(bodyPlot.left(), y, bodyPlot.width(), height);
        result.rows.push_back(row);
        y += rowHeight;
    }
    return result;
}

std::optional<int> CacheWidget::screenYForAddressInBand(const int visibleBandIndex,
                                                        const std::uint64_t address,
                                                        const QRect& plot) const
{
    if (!haveAddressRange_ || sourceBandForVisibleSlot(visibleBandIndex) < 0) {
        return std::nullopt;
    }
    const QRect body = bandAddressBodyRect(visibleBandIndex, plot);
    if (!body.intersects(plot)) {
        return std::nullopt;
    }
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();
    const VisibleBandRows rows = visibleAddressRowsForBand(visibleBandIndex, visibleStart, visibleEnd, plot);
    if (!rows.denseMode) {
        const auto found = std::find_if(rows.rows.cbegin(), rows.rows.cend(), [address](const VisibleAddressRow& row) {
            return row.address == address;
        });
        if (found == rows.rows.cend()) {
            return std::nullopt;
        }
        return found->plotRect.center().y();
    }
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    if (visibleMax <= visibleMin) {
        return body.center().y();
    }
    const long double addressValue = static_cast<long double>(address);
    const long double ratio = (addressValue - visibleMin) / (visibleMax - visibleMin);
    if (ratio < 0.0L || ratio > 1.0L) {
        return std::nullopt;
    }
    const int y = body.bottom() - static_cast<int>(std::llround(ratio * std::max(1, body.height() - 1)));
    return std::clamp(y, body.top(), body.bottom());
}

std::uint64_t CacheWidget::addressForScreenYInBand(const int visibleBandIndex, const int y, const QRect& plot) const
{
    if (!haveAddressRange_) {
        return minAddress_;
    }
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();
    const VisibleBandRows rows = visibleAddressRowsForBand(visibleBandIndex, visibleStart, visibleEnd, plot);
    if (!rows.denseMode) {
        for (const VisibleAddressRow& row : rows.rows) {
            if (row.plotRect.contains(row.plotRect.center().x(), y)) {
                return row.address;
            }
        }
    }
    const QRect body = bandAddressBodyRect(visibleBandIndex, plot);
    const int clampedY = std::clamp(y, body.top(), body.bottom());
    const long double ratio =
        static_cast<long double>(body.bottom() - clampedY) / static_cast<long double>(std::max(1, body.height() - 1));
    return addressFromVisibleRatio(ratio);
}

std::pair<int, int> CacheWidget::visibleBandRange(const QRect& plot) const
{
    if (visibleBandIndices_.empty() || plot.height() <= 0) {
        return {0, -1};
    }
    const int pitch = scaledBandHeight() + scaledBandGap();
    const int first = std::clamp((verticalScrollOffset_) / pitch, 0, static_cast<int>(visibleBandIndices_.size()) - 1);
    const int last = std::clamp((verticalScrollOffset_ + plot.height() + pitch - 1) / pitch,
                                0,
                                static_cast<int>(visibleBandIndices_.size()) - 1);
    return {first, last};
}

std::vector<std::pair<std::uint64_t, int>> CacheWidget::addressTicksForBand(const int visibleBandIndex,
                                                                            const QRect& plot) const
{
    std::vector<std::pair<std::uint64_t, int>> ticks;
    if (!haveAddressRange_ || sourceBandForVisibleSlot(visibleBandIndex) < 0) {
        return ticks;
    }
    const QRect body = bandAddressBodyRect(visibleBandIndex, plot);
    if (body.height() <= 0) {
        return ticks;
    }
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();
    const VisibleBandRows rows = visibleAddressRowsForBand(visibleBandIndex, visibleStart, visibleEnd, plot);
    if (!rows.denseMode) {
        ticks.reserve(rows.rows.size());
        for (const VisibleAddressRow& row : rows.rows) {
            ticks.emplace_back(row.address, row.plotRect.center().y());
        }
        return ticks;
    }
    const int tickCount = std::clamp(body.height() / 36, 2, 8);
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    for (int index = 0; index <= tickCount; ++index) {
        const long double ratio = static_cast<long double>(index) / static_cast<long double>(tickCount);
        const long double address = visibleMin + ratio * (visibleMax - visibleMin);
        const std::uint64_t tickAddress = static_cast<std::uint64_t>(std::llround(
            std::clamp(address,
                       static_cast<long double>(minAddress_),
                       static_cast<long double>(maxAddress_))));
        if (const auto y = screenYForAddressInBand(visibleBandIndex, tickAddress, plot)) {
            ticks.emplace_back(tickAddress, *y);
        }
    }
    return ticks;
}

QString CacheWidget::visibleRangeText() const
{
    if (!haveTimeRange_) {
        return QStringLiteral("empty");
    }
    const auto [first, last] = visibleTimestampRange();
    return QStringLiteral("%1 to %2").arg(first).arg(last);
}

QString CacheWidget::sourceModeName() const
{
    return sourceModeName(sourceMode_);
}

QString CacheWidget::sourceModeName(const SourceMode mode)
{
    switch (mode) {
    case SourceMode::Empty:
        return QStringLiteral("empty");
    case SourceMode::TraceSession:
        return QStringLiteral("traceSession");
    case SourceMode::RowSnapshot:
        return QStringLiteral("rowSnapshot");
    }
    return QStringLiteral("empty");
}

QString CacheWidget::dragModeName(const DragMode mode)
{
    switch (mode) {
    case DragMode::None:
        return QStringLiteral("none");
    case DragMode::PendingGesture:
        return QStringLiteral("pending");
    case DragMode::RangeZoom:
        return QStringLiteral("rangeZoom");
    case DragMode::ZoomFull:
        return QStringLiteral("zoomFull");
    case DragMode::ZoomIn2x:
        return QStringLiteral("zoomIn2x");
    case DragMode::ZoomOut2x:
        return QStringLiteral("zoomOut2x");
    case DragMode::Pan:
        return QStringLiteral("pan");
    case DragMode::OverviewPan:
        return QStringLiteral("overviewPan");
    }
    return QStringLiteral("none");
}

void CacheWidget::zoomToFit()
{
    pushViewHistory();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    verticalBandScale_ = 1.0;
    resetVerticalAddressView();
    clearDenseTileCache();
    normalizeView();
    updateScrollBars();
    update();
}

void CacheWidget::zoomByFactor(const double factor, const QPoint& focus)
{
    if (!haveTimeRange_ || factor <= 0.0) {
        return;
    }
    pushViewHistory();
    const QRect plot = plotRect();
    const std::uint64_t focusContent = contentXAtTimelineX(focus.x());
    const double localRatio = plot.width() > 0
        ? static_cast<double>(std::clamp(focus.x() - plot.left(), 0, plot.width())) / plot.width()
        : 0.5;
    horizontalZoom_ = std::clamp(horizontalZoom_ * factor, kMinZoom, kMaxZoom);
    const std::uint64_t newFocusContent = contentXForTimestamp(timestampForContentX(focusContent));
    const std::uint64_t desired =
        newFocusContent > static_cast<std::uint64_t>(std::llround(localRatio * plot.width()))
            ? newFocusContent - static_cast<std::uint64_t>(std::llround(localRatio * plot.width()))
            : 0;
    scrollOffset_ = desired;
    normalizeView();
    updateScrollBars();
    update();
}

void CacheWidget::zoomVerticallyByFactor(const double factor, const QPoint& focus)
{
    if (factor <= 0.0 || visibleBandIndices_.empty()) {
        return;
    }
    const QRect plot = plotRect();
    if (plot.height() <= 0) {
        return;
    }
    pushViewHistory();
    const int oldBandHeight = scaledBandHeight();
    const int oldGap = scaledBandGap();
    const int oldPitch = oldBandHeight + oldGap;
    const int focusContentY = verticalScrollOffset_ + std::clamp(focus.y() - plot.top(), 0, plot.height());
    const long double contentRatio = oldPitch > 0
        ? static_cast<long double>(focusContentY) / static_cast<long double>(oldPitch)
        : 0.0L;

    std::optional<std::uint64_t> focusAddress;
    const auto [firstBand, lastBand] = visibleBandRange(plot);
    for (int visibleBand = firstBand; visibleBand <= lastBand; ++visibleBand) {
        const QRect body = bandAddressBodyRect(visibleBand, plot);
        if (body.contains(focus)) {
            focusAddress = addressForScreenYInBand(visibleBand, focus.y(), plot);
            break;
        }
    }
    long double focusRatio = 0.5L;
    if (focusAddress) {
        const auto [visibleMin, visibleMax] = visibleAddressRange();
        if (visibleMax > visibleMin) {
            focusRatio =
                (static_cast<long double>(*focusAddress) - visibleMin) / (visibleMax - visibleMin);
        }
    }

    verticalBandScale_ = std::clamp(verticalBandScale_ * factor,
                                    kMinVerticalBandScale,
                                    kMaxVerticalBandScale);
    verticalAddressZoom_ = std::clamp(verticalAddressZoom_ * factor,
                                      kMinVerticalAddressZoom,
                                      kMaxVerticalAddressZoom);
    if (focusAddress && haveAddressRange_) {
        const auto [globalMin, globalMax] = globalAddressViewRange();
        const long double span = (globalMax - globalMin) / static_cast<long double>(verticalAddressZoom_);
        verticalCenterAddress_ = static_cast<long double>(*focusAddress) + span * (0.5L - focusRatio);
    }
    normalizeVerticalView();

    const int newPitch = scaledBandHeight() + scaledBandGap();
    verticalScrollOffset_ =
        std::max(0,
                 static_cast<int>(std::llround(contentRatio * static_cast<long double>(newPitch)))
                     - std::clamp(focus.y() - plot.top(), 0, plot.height()));
    clearDenseTileCache();
    normalizeView();
    updateScrollBars();
    update();
}

void CacheWidget::zoomToTimestampRange(const qint64 firstTimestamp,
                                       const qint64 lastTimestamp,
                                       const bool moveCursor)
{
    if (!haveTimeRange_ || firstTimestamp == lastTimestamp) {
        return;
    }
    pushViewHistory();
    const qint64 first = std::min(firstTimestamp, lastTimestamp);
    const qint64 last = std::max(firstTimestamp, lastTimestamp);
    const long double total = std::max<long double>(1.0L, static_cast<long double>(lastTimestamp_ - firstTimestamp_));
    const long double selected = std::max<long double>(1.0L, static_cast<long double>(last - first));
    horizontalZoom_ = std::clamp(static_cast<double>(total / selected), kMinZoom, kMaxZoom);
    scrollOffset_ = contentXForTimestamp(first);
    if (moveCursor) {
        cursorTimestamp_ = first;
        hasCursor_ = true;
    }
    normalizeView();
    updateScrollBars();
    update();
}

void CacheWidget::pushViewHistory()
{
    ViewHistoryEntry entry;
    entry.horizontalZoom = horizontalZoom_;
    entry.scrollOffset = scrollOffset_;
    entry.verticalScrollOffset = verticalScrollOffset_;
    entry.verticalBandScale = verticalBandScale_;
    entry.verticalAddressZoom = verticalAddressZoom_;
    entry.verticalCenterAddress = verticalCenterAddress_;
    entry.hasCursor = hasCursor_;
    entry.cursorTimestamp = cursorTimestamp_;
    viewHistory_.push_back(entry);
    if (viewHistory_.size() > 32) {
        viewHistory_.erase(viewHistory_.begin());
    }
}

void CacheWidget::restorePreviousView()
{
    if (viewHistory_.empty()) {
        return;
    }
    const ViewHistoryEntry entry = viewHistory_.back();
    viewHistory_.pop_back();
    horizontalZoom_ = entry.horizontalZoom;
    scrollOffset_ = entry.scrollOffset;
    verticalScrollOffset_ = entry.verticalScrollOffset;
    verticalBandScale_ = entry.verticalBandScale;
    verticalAddressZoom_ = entry.verticalAddressZoom;
    verticalCenterAddress_ = entry.verticalCenterAddress;
    hasCursor_ = entry.hasCursor;
    cursorTimestamp_ = entry.cursorTimestamp;
    clearDenseTileCache();
    normalizeView();
    updateScrollBars();
    update();
}

void CacheWidget::scrollToHorizontalOffset(const std::uint64_t offset)
{
    scrollOffset_ = clampOffsetToRange(offset, horizontalContentRange());
    updateScrollBars();
    update();
}

void CacheWidget::panHorizontallyByPixels(const int pixels)
{
    const qint64 desired = static_cast<qint64>(scrollOffset_) + pixels;
    scrollToHorizontalOffset(static_cast<std::uint64_t>(std::max<qint64>(0, desired)));
}

void CacheWidget::scrollVerticallyByPixels(const int pixels)
{
    verticalScrollOffset_ = std::clamp(verticalScrollOffset_ + pixels, 0, verticalContentRange());
    updateScrollBars();
    update();
}

void CacheWidget::ensureSpanVisible(const int spanIndex)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return;
    }
    const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    const QRect plot = plotRect();
    const std::uint64_t start = contentXForTimestamp(span.startTimestamp);
    const std::uint64_t end = contentXForTimestamp(span.endTimestamp);
    if (start < scrollOffset_) {
        scrollOffset_ = start;
    } else if (end > scrollOffset_ + static_cast<std::uint64_t>(std::max(1, plot.width()))) {
        scrollOffset_ = end - static_cast<std::uint64_t>(std::max(1, plot.width()));
    }
    const int visibleBand = visibleSlotForSourceBand(span.rnBandIndex);
    if (visibleBand < 0) {
        addVisibleBand(span.rnBandIndex);
    }
    const int effectiveVisibleBand = visibleSlotForSourceBand(span.rnBandIndex);
    if (effectiveVisibleBand < 0) {
        return;
    }
    const QRect band = bandRect(effectiveVisibleBand, plot);
    if (band.top() < plot.top()) {
        verticalScrollOffset_ = effectiveVisibleBand * (scaledBandHeight() + scaledBandGap());
    } else if (band.bottom() > plot.bottom()) {
        verticalScrollOffset_ =
            effectiveVisibleBand * (scaledBandHeight() + scaledBandGap())
            - std::max(0, plot.height() - scaledBandHeight());
    }
    normalizeView();
    updateScrollBars();
}

void CacheWidget::setCursorFromSpan(const int spanIndex, const bool emitActivation)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return;
    }
    const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    selectedSpanIndex_ = spanIndex;
    selectedLogicalRow_ = static_cast<int>(std::min<std::uint64_t>(
        span.startLogicalRow,
        static_cast<std::uint64_t>((std::numeric_limits<int>::max)())));
    cursorTimestamp_ = span.startTimestamp;
    hasCursor_ = true;
    ensureSpanVisible(spanIndex);
    update();
    if (emitActivation) {
        Q_EMIT logicalRowActivated(selectedLogicalRow_);
    }
}

void CacheWidget::setMarkerLogicalRow(const int logicalRow)
{
    markerLogicalRow_ = logicalRow >= 0 ? logicalRow : -1;
    markerSpanIndex_ = markerLogicalRow_ >= 0
        ? spanIndexForLogicalRow(static_cast<std::uint64_t>(markerLogicalRow_))
        : -1;
    if (markerSpanIndex_ >= 0) {
        markerTimestamp_ = timestampForLogicalRowInSpan(
            spans_[static_cast<std::size_t>(markerSpanIndex_)],
            static_cast<std::uint64_t>(markerLogicalRow_));
        hasMarker_ = true;
    } else {
        hasMarker_ = false;
        markerTimestamp_ = 0;
    }
    update();
}

void CacheWidget::moveCursorToAdjacentSpan(const int direction)
{
    if (spans_.empty() || visibleBandIndices_.empty()) {
        return;
    }
    int index = selectedSpanIndex_;
    if (index < 0) {
        const int firstVisible = visibleSpanIndices(firstTimestamp_,
                                                   lastTimestamp_,
                                                   0,
                                                   static_cast<int>(visibleBandIndices_.size()) - 1).empty()
            ? -1
            : visibleSpanIndices(firstTimestamp_,
                                 lastTimestamp_,
                                 0,
                                 static_cast<int>(visibleBandIndices_.size()) - 1).front();
        if (firstVisible < 0) {
            return;
        }
        index = firstVisible;
    } else {
        std::vector<int> visible = visibleSpanIndices(firstTimestamp_,
                                                      lastTimestamp_,
                                                      0,
                                                      static_cast<int>(visibleBandIndices_.size()) - 1);
        if (visible.empty()) {
            return;
        }
        const auto found = std::find(visible.begin(), visible.end(), index);
        if (found == visible.end()) {
            index = direction >= 0 ? visible.front() : visible.back();
        } else {
            const int position = static_cast<int>(std::distance(visible.begin(), found));
            index = visible[static_cast<std::size_t>(
                std::clamp(position + (direction >= 0 ? 1 : -1), 0, static_cast<int>(visible.size()) - 1))];
        }
    }
    setCursorFromSpan(index, true);
}

void CacheWidget::beginPendingGesture(const QPoint& position)
{
    dragMode_ = DragMode::PendingGesture;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    update();
}

void CacheWidget::updatePendingGesture(const QPoint& position, const Qt::KeyboardModifiers modifiers)
{
    dragLast_ = position;
    if ((position - leftDragAnchor_).manhattanLength() < kGestureActivationDistance) {
        return;
    }
    const DragMode mode = classifyLeftDragGesture(position, modifiers);
    if (mode == DragMode::PendingGesture) {
        update();
        return;
    }
    if (mode == DragMode::Pan) {
        beginPan(leftDragAnchor_);
        updatePan(position);
        return;
    }
    if (mode == DragMode::ZoomFull) {
        beginZoomFullGesture(leftDragAnchor_);
        dragLast_ = position;
        dragRect_ = QRect(leftDragAnchor_, dragLast_).normalized();
        update();
        return;
    }
    if (mode == DragMode::ZoomIn2x) {
        beginZoomIn2xGesture(leftDragAnchor_);
        dragLast_ = position;
        dragRect_ = QRect(leftDragAnchor_, dragLast_).normalized();
        update();
        return;
    }
    if (mode == DragMode::ZoomOut2x) {
        beginZoomOut2xGesture(leftDragAnchor_);
        dragLast_ = position;
        dragRect_ = QRect(leftDragAnchor_, dragLast_).normalized();
        update();
        return;
    }
    beginRangeZoom(leftDragAnchor_);
    updateRangeZoom(position);
}

void CacheWidget::beginRangeZoom(const QPoint& position)
{
    dragMode_ = DragMode::RangeZoom;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
    dragRect_ = QRect(position, QSize(1, 1));
}

void CacheWidget::updateRangeZoom(const QPoint& position)
{
    dragLast_ = position;
    const QRect plot = plotRect();
    const int x1 = std::clamp(dragStart_.x(), plot.left(), plot.right());
    const int x2 = std::clamp(position.x(), plot.left(), plot.right());
    dragRect_ = QRect(QPoint(std::min(x1, x2), plot.top()),
                      QPoint(std::max(x1, x2), plot.bottom())).normalized();
    update();
}

bool CacheWidget::finishRangeZoom()
{
    if (dragRect_.width() < 6) {
        update();
        return false;
    }
    const qint64 first = timestampAtTimelineX(dragRect_.left());
    const qint64 last = timestampAtTimelineX(dragRect_.right());
    zoomToTimestampRange(first, last, true);
    return true;
}

void CacheWidget::beginZoomFullGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomFull;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
}

bool CacheWidget::finishZoomFullGesture()
{
    zoomToFit();
    return true;
}

void CacheWidget::beginZoomIn2xGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomIn2x;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
}

bool CacheWidget::finishZoomIn2xGesture()
{
    zoomByFactor(2.0, dragStart_);
    return true;
}

void CacheWidget::beginZoomOut2xGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomOut2x;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
}

bool CacheWidget::finishZoomOut2xGesture()
{
    zoomByFactor(0.5, dragStart_);
    return true;
}

void CacheWidget::beginPan(const QPoint& position)
{
    dragMode_ = DragMode::Pan;
    dragStart_ = position;
    dragLast_ = position;
    setCursor(Qt::ClosedHandCursor);
}

void CacheWidget::updatePan(const QPoint& position)
{
    const QPoint delta = position - dragLast_;
    dragLast_ = position;
    panHorizontallyByPixels(-delta.x());
    scrollVerticallyByPixels(-delta.y());
}

bool CacheWidget::hasHiddenBands() const
{
    return visibleBandIndices_.size() < bands_.size();
}

bool CacheWidget::isSourceBandVisible(const int sourceBandIndex) const
{
    return std::find(visibleBandIndices_.cbegin(), visibleBandIndices_.cend(), sourceBandIndex)
        != visibleBandIndices_.cend();
}

int CacheWidget::visibleSlotForSourceBand(const int sourceBandIndex) const
{
    const auto found = std::find(visibleBandIndices_.cbegin(), visibleBandIndices_.cend(), sourceBandIndex);
    return found == visibleBandIndices_.cend()
        ? -1
        : static_cast<int>(std::distance(visibleBandIndices_.cbegin(), found));
}

int CacheWidget::sourceBandForVisibleSlot(const int visibleBandIndex) const
{
    if (visibleBandIndex < 0 || visibleBandIndex >= static_cast<int>(visibleBandIndices_.size())) {
        return -1;
    }
    const int sourceBandIndex = visibleBandIndices_[static_cast<std::size_t>(visibleBandIndex)];
    return sourceBandIndex >= 0 && sourceBandIndex < static_cast<int>(bands_.size()) ? sourceBandIndex : -1;
}

bool CacheWidget::addVisibleBand(const int sourceBandIndex)
{
    if (sourceBandIndex < 0 || sourceBandIndex >= static_cast<int>(bands_.size())
        || isSourceBandVisible(sourceBandIndex)) {
        return false;
    }
    visibleBandIndices_.push_back(sourceBandIndex);
    clearDenseTileCache();
    normalizeView();
    updateScrollBars();
    update();
    return true;
}

bool CacheWidget::removeVisibleBandAt(const int visibleBandIndex)
{
    if (visibleBandIndex < 0 || visibleBandIndex >= static_cast<int>(visibleBandIndices_.size())) {
        return false;
    }
    const int removedSourceBand = visibleBandIndices_[static_cast<std::size_t>(visibleBandIndex)];
    visibleBandIndices_.erase(visibleBandIndices_.begin() + visibleBandIndex);
    if (selectedSpanIndex_ >= 0
        && spans_[static_cast<std::size_t>(selectedSpanIndex_)].rnBandIndex == removedSourceBand) {
        selectedSpanIndex_ = -1;
    }
    if (hoveredSpanIndex_ >= 0
        && spans_[static_cast<std::size_t>(hoveredSpanIndex_)].rnBandIndex == removedSourceBand) {
        hoveredSpanIndex_ = -1;
    }
    if (markerSpanIndex_ >= 0
        && spans_[static_cast<std::size_t>(markerSpanIndex_)].rnBandIndex == removedSourceBand) {
        markerSpanIndex_ = -1;
        hasMarker_ = false;
    }
    clearDenseTileCache();
    normalizeView();
    updateScrollBars();
    update();
    return true;
}

void CacheWidget::showAddBandMenu()
{
    if (!hasHiddenBands()) {
        return;
    }
    QMenu menu(this);
    for (int bandIndex = 0; bandIndex < static_cast<int>(bands_.size()); ++bandIndex) {
        if (isSourceBandVisible(bandIndex)) {
            continue;
        }
        const CacheRnBand& band = bands_[static_cast<std::size_t>(bandIndex)];
        const QString label = QStringLiteral("%1  |  %2 spans")
                                  .arg(band.topologyLabel,
                                       FormatUnsignedIntegerWithThousands(band.spanCount));
        QAction* action = menu.addAction(label);
        action->setData(bandIndex);
    }
    const QPoint menuPosition = mapToGlobal(addBandButtonRect().bottomLeft());
    QAction* selected = menu.exec(menuPosition);
    if (selected) {
        addVisibleBand(selected->data().toInt());
    }
}

void CacheWidget::clearDenseTileCache() const
{
    denseSpanTileCache_.clear();
}

void CacheWidget::pruneDenseTileCache() const
{
    if (denseSpanTileCache_.size() <= static_cast<std::size_t>(kDenseTileCacheMaxEntries)) {
        return;
    }
    std::sort(denseSpanTileCache_.begin(), denseSpanTileCache_.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.lastUsed < rhs.lastUsed;
    });
    denseSpanTileCache_.erase(denseSpanTileCache_.begin(),
                              denseSpanTileCache_.begin()
                                  + static_cast<std::ptrdiff_t>(denseSpanTileCache_.size()
                                                                - kDenseTileCacheMaxEntries));
}

CacheWidget::DragMode CacheWidget::classifyLeftDragGesture(
    const QPoint& position,
    const Qt::KeyboardModifiers modifiers) const
{
    const QPoint delta = position - leftDragAnchor_;
    if (delta.manhattanLength() < kGestureActivationDistance) {
        return DragMode::PendingGesture;
    }
    if (modifiers & Qt::ShiftModifier) {
        return DragMode::Pan;
    }
    if ((modifiers & Qt::ControlModifier)
        && (positionInTimeline(leftDragAnchor_) || positionInRuler(leftDragAnchor_))) {
        return DragMode::RangeZoom;
    }
    if (!gestureDirectionReady(delta)) {
        return DragMode::PendingGesture;
    }
    switch (snappedGestureOctant(delta)) {
    case 1:
        return DragMode::ZoomIn2x;
    case 3:
    case -3:
        return DragMode::ZoomOut2x;
    case -1:
        return DragMode::ZoomOut2x;
    case 2:
    case -2:
        return DragMode::ZoomFull;
    default:
        break;
    }
    if (positionInTimeline(leftDragAnchor_) || positionInRuler(leftDragAnchor_)) {
        return std::abs(delta.x()) >= std::abs(delta.y()) ? DragMode::RangeZoom : DragMode::ZoomFull;
    }
    return DragMode::PendingGesture;
}

bool CacheWidget::positionInTimeline(const QPoint& position) const
{
    return plotRect().contains(position);
}

bool CacheWidget::positionInRuler(const QPoint& position) const
{
    return topRulerRect().contains(position) || fullRulerRect().contains(position);
}

QRect CacheWidget::overviewWindowRect() const
{
    const QRect ruler = fullRulerRect().adjusted(10, 9, -10, -9);
    if (!haveTimeRange_ || horizontalContentRange() == 0 || ruler.width() <= 0) {
        return ruler;
    }
    const long double contentWidth = static_cast<long double>(timelineContentWidth());
    const int left = ruler.left()
        + static_cast<int>(std::llround((static_cast<long double>(scrollOffset_) / contentWidth) * ruler.width()));
    const int width = std::max(8,
                               static_cast<int>(std::llround(
                                   (static_cast<long double>(timelineViewportWidth()) / contentWidth) * ruler.width())));
    return QRect(left, ruler.top(), std::min(width, ruler.right() - left + 1), ruler.height());
}

std::optional<int> CacheWidget::spanIndexAtPosition(const QPoint& position) const
{
    const QRect plot = plotRect();
    if (!plot.contains(position)) {
        return std::nullopt;
    }
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();
    const auto [firstBand, lastBand] = visibleBandRange(plot);
    std::vector<int> candidates = visibleSpanIndices(visibleStart, visibleEnd, firstBand, lastBand);
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
        const QRect rect = spanVisualRect(*it).adjusted(-2, -3, 2, 3);
        if (rect.contains(position)) {
            return *it;
        }
    }
    int nearest = -1;
    int nearestDistance = 9;
    for (const int index : candidates) {
        const QRect rect = spanVisualRect(index).adjusted(-4, -5, 4, 5);
        const int dx = position.x() < rect.left() ? rect.left() - position.x()
            : position.x() > rect.right() ? position.x() - rect.right()
                                          : 0;
        const int dy = position.y() < rect.top() ? rect.top() - position.y()
            : position.y() > rect.bottom() ? position.y() - rect.bottom()
                                           : 0;
        const int distance = std::max(dx, dy);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = index;
        }
    }
    if (nearest >= 0) {
        return nearest;
    }
    return std::nullopt;
}

int CacheWidget::spanIndexForLogicalRow(const std::uint64_t logicalRow) const
{
    int nearest = -1;
    std::uint64_t nearestDistance = (std::numeric_limits<std::uint64_t>::max)();
    for (int index = 0; index < static_cast<int>(spans_.size()); ++index) {
        const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(index)];
        if (logicalRow >= span.startLogicalRow && logicalRow <= span.endLogicalRow) {
            return index;
        }
        const std::uint64_t distance = logicalRow < span.startLogicalRow
            ? span.startLogicalRow - logicalRow
            : logicalRow - span.endLogicalRow;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = index;
        }
    }
    return nearest;
}

QRect CacheWidget::spanVisualRect(const int spanIndex) const
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return QRect();
    }
    const QRect plot = plotRect();
    const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    const int visibleBand = visibleSlotForSourceBand(span.rnBandIndex);
    if (visibleBand < 0) {
        return QRect();
    }
    const auto startX = screenXForTimestamp(span.startTimestamp, plot);
    const auto endX = screenXForTimestamp(span.endTimestamp, plot);
    if (!startX && !endX) {
        const std::uint64_t start = contentXForTimestamp(span.startTimestamp);
        const std::uint64_t end = contentXForTimestamp(span.endTimestamp);
        if (end < scrollOffset_ || start > scrollOffset_ + static_cast<std::uint64_t>(plot.width())) {
            return QRect();
        }
    }
    const int x1 = startX.value_or(plot.left());
    const int x2 = endX.value_or(plot.right());
    const auto y = screenYForAddressInBand(visibleBand, span.lineAddress, plot);
    if (!y) {
        return QRect();
    }
    const int left = std::clamp(std::min(x1, x2), plot.left() - 200000, plot.right() + 200000);
    const int right = std::clamp(std::max(x1, x2), plot.left() - 200000, plot.right() + 200000);
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();
    const VisibleBandRows rows = visibleAddressRowsForBand(visibleBand,
                                                           visibleStart,
                                                           visibleEnd,
                                                           plot);
    const int lineHeight = rows.denseMode
        ? kLineRectHeight
        : std::max(4, std::min(kLineRectHeight, scaledAddressRowHeight() - 4));
    return QRect(QPoint(left, *y - lineHeight / 2),
                 QPoint(std::max(left + 2, right), *y + lineHeight / 2)).normalized();
}

QRect CacheWidget::spanVisualRectInBandTile(const int spanIndex,
                                            const int sourceBandIndex,
                                            const int visibleBandIndex,
                                            const std::uint64_t tileContentLeft,
                                            const int tileWidth,
                                            const QRect& plot,
                                            const VisibleBandRows& rows,
                                            const int bandHeight) const
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size()) || tileWidth <= 0) {
        return QRect();
    }
    const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    if (span.rnBandIndex != sourceBandIndex || !haveTimeRange_ || !haveAddressRange_) {
        return QRect();
    }
    const std::uint64_t startContent = contentXForTimestamp(span.startTimestamp);
    const std::uint64_t endContent = contentXForTimestamp(span.endTimestamp);
    if (endContent + 1 < tileContentLeft
        || startContent > tileContentLeft + static_cast<std::uint64_t>(tileWidth + 1)) {
        return QRect();
    }
    const int x1 = static_cast<int>(std::clamp<qint64>(
        static_cast<qint64>(startContent) - static_cast<qint64>(tileContentLeft),
        -200000,
        tileWidth + 200000));
    const int x2 = static_cast<int>(std::clamp<qint64>(
        static_cast<qint64>(endContent) - static_cast<qint64>(tileContentLeft),
        -200000,
        tileWidth + 200000));
    const QRect body(0, scaledBandTitleHeight(), tileWidth, std::max(1, bandHeight - scaledBandTitleHeight() - 6));
    if (!rows.denseMode) {
        const auto found = std::find_if(rows.rows.cbegin(), rows.rows.cend(), [&span](const VisibleAddressRow& row) {
            return row.address == span.lineAddress;
        });
        if (found == rows.rows.cend()) {
            return QRect();
        }
        const int localY = std::clamp(found->plotRect.center().y() - bandRect(visibleBandIndex, plot).top(),
                                      body.top(),
                                      body.bottom());
        const int left = std::min(x1, x2);
        const int right = std::max(x1, x2);
        const int height = std::max(4, std::min(kLineRectHeight, scaledAddressRowHeight() - 4));
        return QRect(QPoint(left, localY - height / 2),
                     QPoint(std::max(left + 2, right), localY + height / 2)).normalized();
    }
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    if (visibleMax <= visibleMin) {
        return QRect();
    }
    const long double ratio = (static_cast<long double>(span.lineAddress) - visibleMin) / (visibleMax - visibleMin);
    if (ratio < 0.0L || ratio > 1.0L) {
        return QRect();
    }
    const int y = body.bottom() - static_cast<int>(std::llround(ratio * std::max(1, body.height() - 1)));
    const int left = std::min(x1, x2);
    const int right = std::max(x1, x2);
    return QRect(QPoint(left, y - kLineRectHeight / 2),
                 QPoint(std::max(left + 2, right), y + kLineRectHeight / 2)).normalized();
}

std::vector<int> CacheWidget::visibleSpanIndices(const qint64 visibleStart,
                                                 const qint64 visibleEnd,
                                                 const int firstVisibleBand,
                                                 const int lastVisibleBand) const
{
    std::vector<int> indices;
    if (firstVisibleBand > lastVisibleBand || spans_.empty() || visibleBandIndices_.empty()) {
        return indices;
    }
    for (int visibleBand = firstVisibleBand;
         visibleBand <= lastVisibleBand && visibleBand < static_cast<int>(visibleBandIndices_.size());
         ++visibleBand) {
        const int band = sourceBandForVisibleSlot(visibleBand);
        if (band < 0 || band >= static_cast<int>(spanIndicesByBandStart_.size())) {
            continue;
        }
        std::vector<int> bandIndices = visibleSpanIndicesForSourceBand(visibleStart, visibleEnd, band);
        indices.insert(indices.end(), bandIndices.begin(), bandIndices.end());
    }
    return indices;
}

std::vector<int> CacheWidget::visibleSpanIndicesForSourceBand(const qint64 visibleStart,
                                                              const qint64 visibleEnd,
                                                              const int sourceBandIndex) const
{
    std::vector<int> indices;
    if (sourceBandIndex < 0 || sourceBandIndex >= static_cast<int>(spanIndicesByBandStart_.size())) {
        return indices;
    }
    const auto& byStart = spanIndicesByBandStart_[static_cast<std::size_t>(sourceBandIndex)];
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    for (const int index : byStart) {
        const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(index)];
        if (span.startTimestamp > visibleEnd) {
            break;
        }
        if (span.endTimestamp < visibleStart) {
            continue;
        }
        const long double address = static_cast<long double>(span.lineAddress);
        if (haveAddressRange_ && (address < visibleMin || address > visibleMax)) {
            continue;
        }
        indices.push_back(index);
    }
    return indices;
}

qint64 CacheWidget::timestampForLogicalRowInSpan(const CacheLifetimeSpan& span,
                                                 const std::uint64_t logicalRow) const
{
    if (span.endLogicalRow <= span.startLogicalRow || span.endTimestamp <= span.startTimestamp) {
        return span.startTimestamp;
    }
    const std::uint64_t clamped = std::clamp(logicalRow, span.startLogicalRow, span.endLogicalRow);
    const long double ratio = static_cast<long double>(clamped - span.startLogicalRow)
        / static_cast<long double>(span.endLogicalRow - span.startLogicalRow);
    return span.startTimestamp + static_cast<qint64>(
        std::llround(ratio * static_cast<long double>(span.endTimestamp - span.startTimestamp)));
}

QString CacheWidget::spanTooltipText(const int spanIndex) const
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return QString();
    }
    const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    const QString rnLabel = span.rnBandIndex >= 0 && span.rnBandIndex < static_cast<int>(bands_.size())
        ? bands_[static_cast<std::size_t>(span.rnBandIndex)].topologyLabel
        : QStringLiteral("RN %1").arg(span.rnNodeId);
    return QStringLiteral("%1\nAddress %2\nTime %3 - %4%5\nRows %6 - %7\nState %8 -> %9")
        .arg(rnLabel,
             addressText(span.lineAddress),
             QString::number(span.startTimestamp),
             QString::number(span.endTimestamp),
             span.openEnded ? QStringLiteral(" (open)") : QString(),
             QString::number(static_cast<qulonglong>(span.startLogicalRow)),
             QString::number(static_cast<qulonglong>(span.endLogicalRow)),
             span.startStateText.isEmpty() ? QStringLiteral("?") : span.startStateText,
             span.endStateText.isEmpty() ? QStringLiteral("?") : span.endStateText);
}

void CacheWidget::paintHeader(QPainter& painter) const
{
    const QRect header = headerRect();
    painter.fillRect(header, QColor(QStringLiteral("#F8FAFC")));
    painter.setPen(QColor(QStringLiteral("#D6DEE8")));
    painter.drawLine(header.bottomLeft(), header.bottomRight());

    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#111827")));
    const QRect add = addBandButtonRect();
    const QRect titleRect(header.left() + 14,
                          header.top() + 4,
                          std::max(0, add.left() - header.left() - 30),
                          18);
    const QFontMetrics titleMetrics(titleFont);
    painter.drawText(titleRect,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(QStringLiteral("RN Cache Line Board"), Qt::ElideRight, titleRect.width()));

    QFont infoFont = painter.font();
    infoFont.setBold(false);
    infoFont.setPointSizeF(std::max<qreal>(7.0, infoFont.pointSizeF() - 1.5));
    painter.setFont(infoFont);
    painter.setPen(QColor(QStringLiteral("#4B5563")));
    const QString summary = spans_.empty()
        ? statusText_
        : QStringLiteral("%1 spans  |  %2/%3 RN bands  |  Y %4x/%5x  |  %6")
              .arg(FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(spans_.size())),
                   FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(visibleBandIndices_.size())),
                   FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(bands_.size())))
              .arg(verticalBandScale_, 0, 'f', verticalBandScale_ < 10.0 ? 2 : 1)
              .arg(verticalAddressZoom_, 0, 'f', verticalAddressZoom_ < 10.0 ? 2 : 1)
              .arg(
                   visibleRangeText());
    const QRect summaryRect(header.left() + 14,
                            header.top() + 22,
                            std::max(0, add.left() - header.left() - 30),
                            18);
    const QFontMetrics summaryMetrics(infoFont);
    painter.drawText(summaryRect,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     summaryMetrics.elidedText(summary, Qt::ElideRight, summaryRect.width()));

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(hasHiddenBands() ? QColor(QStringLiteral("#1D4ED8")) : QColor(QStringLiteral("#CBD5E1")), 1));
    painter.setBrush(hasHiddenBands() ? QColor(QStringLiteral("#FFFFFF")) : QColor(QStringLiteral("#F1F5F9")));
    painter.drawRoundedRect(add, 4, 4);
    QFont buttonFont = painter.font();
    buttonFont.setBold(true);
    buttonFont.setPointSizeF(buttonFont.pointSizeF() + 2.0);
    painter.setFont(buttonFont);
    painter.setPen(hasHiddenBands() ? QColor(QStringLiteral("#1D4ED8")) : QColor(QStringLiteral("#94A3B8")));
    painter.drawText(add, Qt::AlignCenter, QStringLiteral("+"));
}

void CacheWidget::paintTopRuler(QPainter& painter) const
{
    const QRect ruler = topRulerRect();
    painter.fillRect(ruler, QColor(QStringLiteral("#FFFFFF")));
    painter.setPen(QColor(QStringLiteral("#D6DEE8")));
    painter.drawLine(ruler.bottomLeft(), ruler.bottomRight());
    if (!haveTimeRange_ || ruler.width() <= 0) {
        return;
    }
    const auto [first, last] = visibleTimestampRange();
    const qint64 span = std::max<qint64>(1, last - first);
    const qint64 major = std::max<qint64>(1, static_cast<qint64>(niceStep(static_cast<double>(span) / 8.0)));
    QFont font = painter.font();
    font.setPointSizeF(std::max<qreal>(7.0, font.pointSizeF() - 1.0));
    painter.setFont(font);
    for (qint64 tick = (first / major) * major; tick <= last; tick += major) {
        const auto x = screenXForTimestamp(tick, ruler);
        if (!x) {
            if (tick > last - major) {
                break;
            }
            continue;
        }
        painter.setPen(QColor(QStringLiteral("#CBD5E1")));
        painter.drawLine(*x, ruler.bottom() - 9, *x, ruler.bottom());
        painter.setPen(QColor(QStringLiteral("#475569")));
        painter.drawText(QRect(*x + 4, ruler.top(), 100, ruler.height() - 7),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::number(tick));
        if (tick > std::numeric_limits<qint64>::max() - major) {
            break;
        }
    }
}

void CacheWidget::paintAddressAxis(QPainter& painter) const
{
    const QRect axis = addressAxisRect();
    const QRect plot = plotRect();
    painter.fillRect(axis, QColor(QStringLiteral("#F8FAFC")));
    painter.setPen(QColor(QStringLiteral("#D6DEE8")));
    painter.drawLine(axis.topRight(), axis.bottomRight());
    if (visibleBandIndices_.empty()) {
        return;
    }

    const auto [firstBand, lastBand] = visibleBandRange(plot);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    QFont smallFont = painter.font();
    smallFont.setPointSizeF(std::max<qreal>(6.5, smallFont.pointSizeF() - 1.5));
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();

    for (int visibleBandIndex = firstBand; visibleBandIndex <= lastBand; ++visibleBandIndex) {
        const int sourceBandIndex = sourceBandForVisibleSlot(visibleBandIndex);
        if (sourceBandIndex < 0) {
            continue;
        }
        const QRect band = bandRect(visibleBandIndex, plot);
        if (!band.intersects(plot)) {
            continue;
        }
        const CacheRnBand& bandInfo = bands_[static_cast<std::size_t>(sourceBandIndex)];
        const QRect axisBand(axis.left(), band.top(), axis.width(), band.height());
        const QRect header = bandHeaderRailRect(visibleBandIndex, plot).intersected(axis);
        const QRect addressRail = bandAddressRailRect(visibleBandIndex, plot).intersected(axis);
        painter.fillRect(axisBand.adjusted(0, 0, 0, -1),
                         visibleBandIndex % 2 == 0 ? QColor(QStringLiteral("#F8FAFC"))
                                                    : QColor(QStringLiteral("#F3F6FA")));
        painter.setPen(QColor(QStringLiteral("#D8E0EA")));
        painter.drawLine(axisBand.bottomLeft(), axisBand.bottomRight());
        painter.setFont(titleFont);
        painter.setPen(QColor(QStringLiteral("#111827")));
        const QRect titleRect = bandTitleTextRect(visibleBandIndex, plot).intersected(axis);
        const QFontMetrics titleMetrics(titleFont);
        painter.drawText(titleRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         titleMetrics.elidedText(bandInfo.topologyLabel, Qt::ElideRight, titleRect.width()));
        const QRect deleteButton = bandDeleteButtonRect(visibleBandIndex);
        if (deleteButton.isValid()) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(QStringLiteral("#CBD5E1")), 1));
            painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
            painter.drawRoundedRect(deleteButton, 3, 3);
            painter.setPen(QColor(QStringLiteral("#64748B")));
            painter.drawText(deleteButton, Qt::AlignCenter, QStringLiteral("x"));
        }
        painter.setFont(smallFont);
        painter.setPen(QColor(QStringLiteral("#64748B")));
        const QString summary = QStringLiteral("%1  |  %2 spans")
                                    .arg(bandInfo.nodeType,
                                         FormatUnsignedIntegerWithThousands(bandInfo.spanCount));
        const QRect summaryRect(header.left() + 12,
                                header.top() + 22,
                                std::max(0, header.width() - 22),
                                16);
        const QFontMetrics smallMetrics(smallFont);
        painter.drawText(summaryRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         smallMetrics.elidedText(summary, Qt::ElideRight, summaryRect.width()));
        if (!haveAddressRange_ || addressRail.isEmpty()) {
            continue;
        }

        painter.setFont(smallFont);
        const VisibleBandRows rows = visibleAddressRowsForBand(visibleBandIndex, visibleStart, visibleEnd, plot);
        if (rows.denseMode) {
            const QString denseText = QStringLiteral("%1 visible lines - zoom in for rows")
                                          .arg(FormatUnsignedIntegerWithThousands(
                                              static_cast<std::uint64_t>(rows.totalAddressRows)));
            painter.setPen(QColor(QStringLiteral("#64748B")));
            painter.drawText(addressRail.adjusted(10, 0, -10, 0),
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                             denseText);
            for (const auto& [address, y] : addressTicksForBand(visibleBandIndex, plot)) {
                if (y < addressRail.top() || y > addressRail.bottom()) {
                    continue;
                }
                painter.setPen(QColor(QStringLiteral("#CAD2DD")));
                painter.drawLine(axis.right() - 6, y, axis.right(), y);
                painter.setPen(QColor(QStringLiteral("#4B5563")));
                painter.drawText(QRect(axis.left() + 8, y - 9, axis.width() - 20, 18),
                                 Qt::AlignRight | Qt::AlignVCenter,
                                 addressText(address));
            }
            continue;
        }

        for (const VisibleAddressRow& row : rows.rows) {
            if (!row.railRect.intersects(axis)) {
                continue;
            }
            painter.fillRect(row.railRect.adjusted(0, 0, 0, -1),
                             rows.rows.size() > 1 && (&row - rows.rows.data()) % 2 == 0
                                 ? QColor(QStringLiteral("#F9FBFE"))
                                 : QColor(QStringLiteral("#F4F7FB")));
            painter.setPen(QColor(QStringLiteral("#E1E7F0")));
            painter.drawLine(row.railRect.bottomLeft(), row.railRect.bottomRight());
            painter.setPen(QColor(QStringLiteral("#4B5563")));
            painter.drawText(row.railRect.adjusted(8, 0, -10, 0),
                             Qt::AlignRight | Qt::AlignVCenter,
                             smallMetrics.elidedText(addressText(row.address),
                                                     Qt::ElideLeft,
                                                     std::max(0, row.railRect.width() - 18)));
        }
    }
}

void CacheWidget::paintSpans(QPainter& painter) const
{
    const QRect plot = plotRect();
    painter.save();
    painter.setClipRect(plot);
    painter.fillRect(plot, QColor(QStringLiteral("#FFFFFF")));
    lastDensePaintUsed_ = false;

    if (visibleBandIndices_.empty()) {
        painter.restore();
        return;
    }

    const auto [firstBand, lastBand] = visibleBandRange(plot);
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();
    for (int visibleBandIndex = firstBand; visibleBandIndex <= lastBand; ++visibleBandIndex) {
        const QRect band = bandRect(visibleBandIndex, plot);
        if (!band.intersects(plot)) {
            continue;
        }
        const QRect header = bandHeaderPlotRect(visibleBandIndex, plot).intersected(plot);
        const QRect body = bandAddressPlotRect(visibleBandIndex, plot).intersected(plot);
        painter.fillRect(band.intersected(plot),
                         visibleBandIndex % 2 == 0 ? QColor(QStringLiteral("#FFFFFF"))
                                                   : QColor(QStringLiteral("#FBFCFE")));
        painter.setPen(QColor(QStringLiteral("#E6ECF3")));
        painter.drawLine(plot.left(), band.top(), plot.right(), band.top());
        painter.drawLine(plot.left(), band.bottom(), plot.right(), band.bottom());
        if (!header.isEmpty()) {
            painter.fillRect(header, QColor(QStringLiteral("#F8FAFC")));
            painter.setPen(QColor(QStringLiteral("#E2E8F0")));
            painter.drawLine(plot.left(), header.bottom(), plot.right(), header.bottom());
        }
        if (!body.isEmpty()) {
            painter.setPen(QColor(QStringLiteral("#EEF2F7")));
            const VisibleBandRows rows = visibleAddressRowsForBand(visibleBandIndex, visibleStart, visibleEnd, plot);
            if (rows.denseMode) {
                painter.fillRect(body,
                                 visibleBandIndex % 2 == 0 ? QColor(QStringLiteral("#FFFFFF"))
                                                           : QColor(QStringLiteral("#FBFCFE")));
                for (const auto& tick : addressTicksForBand(visibleBandIndex, plot)) {
                    const int y = tick.second;
                    painter.drawLine(plot.left(), y, plot.right(), y);
                }
            } else {
                for (std::size_t rowIndex = 0; rowIndex < rows.rows.size(); ++rowIndex) {
                    const VisibleAddressRow& row = rows.rows[rowIndex];
                    if (!row.plotRect.intersects(plot)) {
                        continue;
                    }
                    painter.fillRect(row.plotRect.intersected(plot),
                                     rowIndex % 2 == 0 ? QColor(QStringLiteral("#FFFFFF"))
                                                       : QColor(QStringLiteral("#FBFCFE")));
                    painter.drawLine(plot.left(), row.plotRect.bottom(), plot.right(), row.plotRect.bottom());
                }
            }
        }
    }

    if (haveTimeRange_) {
        const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
        const double roughMajor = std::max<double>(1.0, static_cast<double>(lastTimestamp - firstTimestamp + 1) / 8.0);
        const qint64 major = std::max<qint64>(1, static_cast<qint64>(niceStep(roughMajor)));
        const qint64 minor = std::max<qint64>(1, major / 5);
        for (qint64 timestamp = (firstTimestamp / minor) * minor;
             timestamp <= lastTimestamp;
             timestamp += minor) {
            const auto x = screenXForTimestamp(timestamp, plot);
            if (!x) {
                if (timestamp > lastTimestamp - minor) {
                    break;
                }
                continue;
            }
            painter.setPen(timestamp % major == 0 ? QColor(QStringLiteral("#DCE3ED"))
                                                  : QColor(QStringLiteral("#F2F5F9")));
            painter.drawLine(*x, plot.top(), *x, plot.bottom());
            if (timestamp > std::numeric_limits<qint64>::max() - minor) {
                break;
            }
        }
    }

    std::vector<int> candidates = visibleSpanIndices(visibleStart, visibleEnd, firstBand, lastBand);
    if (useDenseSpanPaint(candidates, plot)) {
        paintDenseSpans(painter, plot, visibleStart, visibleEnd, firstBand, lastBand, candidates);
        lastDensePaintUsed_ = true;
    } else {
        for (const int index : candidates) {
            paintSpan(painter, index, index == selectedSpanIndex_, index == hoveredSpanIndex_);
        }
    }

    if (lastDensePaintUsed_) {
        if (selectedSpanIndex_ >= 0) {
            paintSpan(painter, selectedSpanIndex_, true, false);
        }
        if (hoveredSpanIndex_ >= 0 && hoveredSpanIndex_ != selectedSpanIndex_) {
            paintSpan(painter, hoveredSpanIndex_, false, true);
        }
    }

    paintCursorAndMarker(painter);

    painter.restore();
}

void CacheWidget::paintDenseSpans(QPainter& painter,
                                  const QRect& plot,
                                  const qint64 visibleStart,
                                  const qint64 visibleEnd,
                                  const int firstBand,
                                  const int lastBand,
                                  const std::vector<int>& candidates) const
{
    Q_UNUSED(candidates);
    Q_UNUSED(visibleStart);
    Q_UNUSED(visibleEnd);
    if (firstBand > lastBand || plot.width() <= 0 || plot.height() <= 0) {
        return;
    }

    const qreal dpr = devicePixelRatioF();
    const std::uint64_t contentWidth = timelineContentWidth();
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    const std::uint64_t visibleMinAddress =
        static_cast<std::uint64_t>(std::llround(std::max<long double>(0.0L, visibleMin)));
    const std::uint64_t visibleMaxAddress =
        static_cast<std::uint64_t>(std::llround(std::max<long double>(0.0L, visibleMax)));
    const std::uint64_t firstTile =
        (scrollOffset_ / static_cast<std::uint64_t>(kDenseTileWidth)) * static_cast<std::uint64_t>(kDenseTileWidth);
    const std::uint64_t lastVisibleContent =
        scrollOffset_ + static_cast<std::uint64_t>(std::max(0, plot.width() - 1));

    for (int visibleBand = firstBand; visibleBand <= lastBand; ++visibleBand) {
        const int sourceBandIndex = sourceBandForVisibleSlot(visibleBand);
        if (sourceBandIndex < 0) {
            continue;
        }
        const QRect band = bandRect(visibleBand, plot);
        if (!band.intersects(plot)) {
            continue;
        }
        const VisibleBandRows tileRows =
            visibleAddressRowsForBand(visibleBand, visibleStart, visibleEnd, plot);

        for (std::uint64_t tileLeft = firstTile;
             tileLeft <= lastVisibleContent;
             tileLeft += static_cast<std::uint64_t>(kDenseTileWidth)) {
            const int tileWidth = static_cast<int>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(kDenseTileWidth),
                contentWidth > tileLeft ? contentWidth - tileLeft : 1));
            DenseSpanTileKey key;
            key.generation = buildGeneration_.load(std::memory_order_relaxed);
            key.sourceBandIndex = sourceBandIndex;
            key.tileContentLeft = tileLeft;
            key.tileWidth = tileWidth;
            key.bandHeight = band.height();
            key.contentWidth = contentWidth;
            key.firstTimestamp = firstTimestamp_;
            key.lastTimestamp = lastTimestamp_;
            key.minAddress = minAddress_;
            key.maxAddress = maxAddress_;
            key.visibleMinAddress = visibleMinAddress;
            key.visibleMaxAddress = visibleMaxAddress;
            key.addressRowHeight = scaledAddressRowHeight();
            key.denseAddressMode = tileRows.denseMode;
            key.visibleStartTimestamp = key.denseAddressMode ? 0 : visibleStart;
            key.visibleEndTimestamp = key.denseAddressMode ? 0 : visibleEnd;
            key.horizontalZoom = horizontalZoom_;
            key.verticalBandScale = verticalBandScale_;
            key.verticalAddressZoom = verticalAddressZoom_;
            key.verticalCenterAddress = verticalCenterAddress_;
            key.devicePixelRatio = dpr;

            auto found = std::find_if(denseSpanTileCache_.begin(), denseSpanTileCache_.end(), [&key](const auto& entry) {
                return entry.key == key;
            });
            if (found == denseSpanTileCache_.end()) {
                DenseSpanTileCacheEntry entry;
                entry.key = key;
                entry.image = QImage(QSize(std::max(1, tileWidth), std::max(1, band.height())) * dpr,
                                     QImage::Format_ARGB32_Premultiplied);
                entry.image.setDevicePixelRatio(dpr);
                entry.image.fill(Qt::transparent);

                const qint64 tileStart = timestampForContentX(tileLeft);
                const qint64 tileEnd = timestampForContentX(tileLeft + static_cast<std::uint64_t>(tileWidth));
                const std::vector<int> tileSpans = visibleSpanIndicesForSourceBand(
                    std::min(tileStart, tileEnd),
                    std::max(tileStart, tileEnd),
                    sourceBandIndex);
                if (!tileSpans.empty()) {
                    QPainter imagePainter(&entry.image);
                    imagePainter.setRenderHint(QPainter::Antialiasing, false);
                    for (const int spanIndex : tileSpans) {
                        if (spanIndex == selectedSpanIndex_ || spanIndex == hoveredSpanIndex_) {
                            continue;
                        }
                        QRect rect = spanVisualRectInBandTile(spanIndex,
                                                              sourceBandIndex,
                                                              visibleBand,
                                                              tileLeft,
                                                              tileWidth,
                                                              plot,
                                                              tileRows,
                                                              band.height());
                        if (!rect.isValid() || !rect.intersects(QRect(0, 0, tileWidth, band.height()))) {
                            continue;
                        }
                        rect = rect.intersected(QRect(-2, 0, tileWidth + 4, band.height()));
                        paintSpanRect(imagePainter,
                                      rect,
                                      spans_[static_cast<std::size_t>(spanIndex)],
                                      false,
                                      false);
                        entry.hasContent = true;
                    }
                    imagePainter.end();
                }
                entry.lastUsed = ++denseSpanTileClock_;
                denseSpanTileCache_.push_back(std::move(entry));
                pruneDenseTileCache();
                found = std::find_if(denseSpanTileCache_.begin(), denseSpanTileCache_.end(), [&key](const auto& item) {
                    return item.key == key;
                });
            } else {
                found->lastUsed = ++denseSpanTileClock_;
            }

            if (found != denseSpanTileCache_.end() && found->hasContent) {
                const int targetX =
                    plot.left() + static_cast<int>(static_cast<qint64>(tileLeft) - static_cast<qint64>(scrollOffset_));
                painter.drawImage(QPoint(targetX, band.top()), found->image);
            }
            if (tileLeft > (std::numeric_limits<std::uint64_t>::max)() - static_cast<std::uint64_t>(kDenseTileWidth)) {
                break;
            }
        }
    }
}

void CacheWidget::paintSpan(QPainter& painter,
                            const int spanIndex,
                            const bool selected,
                            const bool hovered) const
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return;
    }
    QRect visual = spanVisualRect(spanIndex);
    const QRect plot = plotRect();
    if (!visual.isValid() || !visual.intersects(plot)) {
        return;
    }
    visual = visual.intersected(plot.adjusted(-2, -4, 2, 4));
    const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    paintSpanRect(painter, visual, span, selected, hovered);
}

void CacheWidget::paintSpanRect(QPainter& painter,
                                const QRect& visual,
                                const CacheLifetimeSpan& span,
                                const bool selected,
                                const bool hovered) const
{
    const QColor base = spanStateColor(span.endStateText.isEmpty() ? span.startStateText : span.endStateText,
                                       span.openEnded);
    const QColor fill = withAlpha(base, selected ? 210 : hovered ? 170 : 118);
    const QColor edge = selected ? base.darker(135) : base.darker(110);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(edge, selected ? 2 : 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(visual, 2, 2);

    if (selected || hovered) {
        QFont font = painter.font();
        font.setPointSizeF(std::max<qreal>(6.5, font.pointSizeF() - 1.4));
        painter.setFont(font);
        const QString label = compactAddressText(span.lineAddress);
        const QFontMetrics metrics(font);
        if (metrics.horizontalAdvance(label) + 10 > visual.width()) {
            return;
        }
        painter.setPen(selected ? QColor(QStringLiteral("#FFFFFF")) : QColor(QStringLiteral("#0F172A")));
        painter.drawText(visual.adjusted(5, 0, -5, 0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         label);
    }
}

bool CacheWidget::useDenseSpanPaint(const std::vector<int>& candidates, const QRect& plot) const
{
    return plot.width() > 0 && plot.height() > 0
        && candidates.size() >= static_cast<std::size_t>(kDenseSpanThreshold);
}

void CacheWidget::paintCursorAndMarker(QPainter& painter) const
{
    const QRect plot = plotRect();
    const QRect ruler = fullRulerRect();
    const QRect combined(plot.left(), plot.top(), plot.width(), plot.height() + ruler.height() + 1);
    if (hasMarker_) {
        if (const auto x = screenXForTimestamp(markerTimestamp_, plot)) {
            painter.fillRect(QRect(*x - 1, plot.top(), 3, plot.height()), withAlpha(QColor(QStringLiteral("#D97706")), 32));
            painter.setPen(QPen(QColor(QStringLiteral("#D97706")), 1, Qt::DashLine));
            painter.drawLine(*x, combined.top(), *x, combined.bottom());
        }
    }
    if (hasCursor_) {
        if (const auto x = screenXForTimestamp(cursorTimestamp_, plot)) {
            painter.fillRect(QRect(*x - 1, plot.top(), 3, plot.height()), withAlpha(QColor(QStringLiteral("#111827")), 24));
            painter.setPen(QPen(QColor(QStringLiteral("#111827")), 1));
            painter.drawLine(*x, combined.top(), *x, combined.bottom());
        }
    }
}

void CacheWidget::paintFullRuler(QPainter& painter) const
{
    const QRect ruler = fullRulerRect();
    painter.fillRect(ruler, QColor(QStringLiteral("#F8FAFC")));
    painter.setPen(QColor(QStringLiteral("#D6DEE8")));
    painter.drawLine(ruler.topLeft(), ruler.topRight());
    painter.drawLine(ruler.bottomLeft(), ruler.bottomRight());
    if (!haveTimeRange_) {
        return;
    }
    const auto [first, last] = visibleTimestampRange();
    const qint64 span = std::max<qint64>(1, last - first);
    const qint64 major = std::max<qint64>(1, static_cast<qint64>(niceStep(static_cast<double>(span) / 8.0)));
    const qint64 minor = std::max<qint64>(1, major / 5);
    QFont font = painter.font();
    font.setPointSizeF(std::max<qreal>(7.0, font.pointSizeF() - 1.0));
    painter.setFont(font);

    for (qint64 tick = (first / minor) * minor; tick <= last; tick += minor) {
        const auto x = screenXForTimestamp(tick, ruler);
        if (!x) {
            if (tick > last - minor) {
                break;
            }
            continue;
        }
        const bool isMajor = tick % major == 0;
        painter.setPen(isMajor ? QColor(QStringLiteral("#94A3B8")) : QColor(QStringLiteral("#CBD5E1")));
        const int tickHeight = isMajor ? 14 : 7;
        painter.drawLine(*x, ruler.top(), *x, ruler.top() + tickHeight);
        if (isMajor) {
            painter.setPen(QColor(QStringLiteral("#475569")));
            painter.drawText(QRect(*x + 4, ruler.top() + 12, 110, ruler.height() - 13),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QString::number(tick));
        }
        if (tick > std::numeric_limits<qint64>::max() - minor) {
            break;
        }
    }

    const QRect overview = overviewWindowRect().adjusted(0, 9, 0, -7);
    painter.fillRect(overview, withAlpha(QColor(QStringLiteral("#2563EB")), 52));
    painter.setPen(withAlpha(QColor(QStringLiteral("#2563EB")), 150));
    painter.drawRect(overview.adjusted(0, 0, -1, -1));

    if (hasMarker_) {
        if (const auto x = screenXForTimestamp(markerTimestamp_, ruler)) {
            painter.setPen(QPen(QColor(QStringLiteral("#D97706")), 1, Qt::DashLine));
            painter.drawLine(*x, ruler.top(), *x, ruler.bottom());
        }
    }

    if (hasCursor_) {
        if (const auto x = screenXForTimestamp(cursorTimestamp_, ruler)) {
            painter.setPen(QPen(QColor(QStringLiteral("#111827")), 1));
            painter.drawLine(*x, ruler.top(), *x, ruler.bottom());
            const QString label = QString::number(cursorTimestamp_);
            const QFontMetrics metrics(font);
            const int textWidth = metrics.horizontalAdvance(label) + 12;
            QRect tag(*x + 6, ruler.bottom() - metrics.height() - 5, textWidth, metrics.height() + 4);
            if (tag.right() > ruler.right() - 2) {
                tag.moveRight(*x - 6);
            }
            if (tag.left() < ruler.left() + 2) {
                tag.moveLeft(ruler.left() + 2);
            }
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(QStringLiteral("#111827")), 1));
            painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
            painter.drawRoundedRect(tag, 3, 3);
            painter.setPen(QColor(QStringLiteral("#111827")));
            painter.drawText(tag.adjusted(6, 0, -6, 0), Qt::AlignCenter, label);
        }
    }
}

void CacheWidget::paintStatus(QPainter& painter) const
{
    if (statusText_.isEmpty() || shouldShowBuildPrompt() || building_) {
        return;
    }
    if (!spans_.empty() && !visibleBandIndices_.empty()) {
        return;
    }
    const QRect plot = plotRect();
    painter.save();
    painter.setPen(QColor(QStringLiteral("#64748B")));
    const QString text = !spans_.empty() && visibleBandIndices_.empty()
        ? QStringLiteral("Cache ready. Use + to add RN bands to this view.")
        : statusText_;
    painter.drawText(plot.adjusted(16, 16, -16, -16),
                     Qt::AlignTop | Qt::TextWordWrap,
                     text);
    painter.restore();
}

void CacheWidget::paintBuildPrompt(QPainter& painter) const
{
    if (!shouldShowBuildPrompt()) {
        return;
    }
    const QRect plot = plotRect();
    painter.save();
    painter.setPen(QColor(QStringLiteral("#334155")));
    painter.drawText(plot.adjusted(18, 18, -18, -18),
                     Qt::AlignTop | Qt::TextWordWrap,
                     pendingBuildText());

    const QRect button = buildPromptButtonRect();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#1D4ED8")), 1));
    painter.setBrush(QColor(QStringLiteral("#2563EB")));
    painter.drawRoundedRect(button, 4, 4);
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(button, Qt::AlignCenter, QStringLiteral("Build Cache"));
    painter.restore();
}

void CacheWidget::paintBuildProgress(QPainter& painter) const
{
    if (!building_) {
        return;
    }
    const QRect plot = plotRect();
    const QRect bar(plot.left() + 18, plot.top() + 18, std::min(360, plot.width() - 36), 18);
    if (bar.width() <= 0) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(QStringLiteral("#CBD5E1")));
    painter.setBrush(QColor(QStringLiteral("#F8FAFC")));
    painter.drawRoundedRect(bar, 4, 4);
    const double ratio = totalRows_ == 0 ? 0.0 : static_cast<double>(processedRows_) / static_cast<double>(totalRows_);
    const QRect fill = bar.adjusted(2, 2, -2, -2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#2563EB")));
    painter.drawRoundedRect(QRect(fill.left(), fill.top(),
                                  static_cast<int>(std::llround(fill.width() * std::clamp(ratio, 0.0, 1.0))),
                                  fill.height()),
                            3,
                            3);
    painter.setPen(QColor(QStringLiteral("#334155")));
    painter.drawText(bar.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, statusText_);
    painter.restore();
}

void CacheWidget::paintGestureOverlay(QPainter& painter) const
{
    if (!leftDragGestureActive_
        || !(dragMode_ == DragMode::ZoomFull
             || dragMode_ == DragMode::ZoomIn2x
             || dragMode_ == DragMode::ZoomOut2x)) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#2563EB")), 1, Qt::DashLine));
    painter.setBrush(withAlpha(QColor(QStringLiteral("#2563EB")), 36));
    painter.drawRect(dragRect_.normalized());
    painter.setPen(QColor(QStringLiteral("#1E3A8A")));
    const QString text = dragMode_ == DragMode::ZoomFull ? QStringLiteral("Fit")
        : dragMode_ == DragMode::ZoomIn2x ? QStringLiteral("2x in")
                                          : QStringLiteral("2x out");
    painter.drawText(dragRect_.normalized().adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft, text);
    painter.restore();
}

void CacheWidget::paintRangeZoom(QPainter& painter) const
{
    if (dragMode_ != DragMode::RangeZoom || !dragRect_.isValid()) {
        return;
    }
    painter.save();
    painter.setPen(QPen(QColor(QStringLiteral("#2563EB")), 1, Qt::DashLine));
    painter.setBrush(withAlpha(QColor(QStringLiteral("#2563EB")), 42));
    painter.drawRect(dragRect_.normalized());
    painter.restore();
}

#ifdef CHIRON_GUI_ENABLE_CACHE_TEST_API

int CacheWidget::testSpanCount() const noexcept
{
    return static_cast<int>(spans_.size());
}

int CacheWidget::testBandCount() const noexcept
{
    return static_cast<int>(bands_.size());
}

int CacheWidget::testVisibleBandCount() const noexcept
{
    return static_cast<int>(visibleBandIndices_.size());
}

QVariantMap CacheWidget::testSpanAt(const int index) const
{
    QVariantMap map;
    if (index < 0 || index >= static_cast<int>(spans_.size())) {
        return map;
    }
    const CacheLifetimeSpan& span = spans_[static_cast<std::size_t>(index)];
    map.insert(QStringLiteral("rnBandIndex"), span.rnBandIndex);
    map.insert(QStringLiteral("rnNodeId"), static_cast<int>(span.rnNodeId));
    map.insert(QStringLiteral("lineAddress"), QVariant::fromValue<qulonglong>(span.lineAddress));
    map.insert(QStringLiteral("startTimestamp"), span.startTimestamp);
    map.insert(QStringLiteral("endTimestamp"), span.endTimestamp);
    map.insert(QStringLiteral("startLogicalRow"), QVariant::fromValue<qulonglong>(span.startLogicalRow));
    map.insert(QStringLiteral("endLogicalRow"), QVariant::fromValue<qulonglong>(span.endLogicalRow));
    map.insert(QStringLiteral("startStateText"), span.startStateText);
    map.insert(QStringLiteral("endStateText"), span.endStateText);
    map.insert(QStringLiteral("openEnded"), span.openEnded);
    return map;
}

QVariantMap CacheWidget::testBandAt(const int index) const
{
    QVariantMap map;
    if (index < 0 || index >= static_cast<int>(bands_.size())) {
        return map;
    }
    const CacheRnBand& band = bands_[static_cast<std::size_t>(index)];
    map.insert(QStringLiteral("rnNodeId"), static_cast<int>(band.rnNodeId));
    map.insert(QStringLiteral("topologyLabel"), band.topologyLabel);
    map.insert(QStringLiteral("nodeType"), band.nodeType);
    map.insert(QStringLiteral("spanCount"), QVariant::fromValue<qulonglong>(band.spanCount));
    return map;
}

QString CacheWidget::testStatusText() const
{
    return statusText_;
}

QVariantMap CacheWidget::testLayoutState() const
{
    return diagnosticsState();
}

std::pair<qint64, qint64> CacheWidget::testVisibleTimestampRange() const
{
    return visibleTimestampRange();
}

double CacheWidget::testHorizontalZoom() const noexcept
{
    return horizontalZoom_;
}

double CacheWidget::testVerticalBandScale() const noexcept
{
    return verticalBandScale_;
}

double CacheWidget::testVerticalAddressZoom() const noexcept
{
    return verticalAddressZoom_;
}

bool CacheWidget::testUsesDensePaint() const noexcept
{
    return lastDensePaintUsed_;
}

int CacheWidget::testDenseTileCacheEntryCount() const noexcept
{
    return static_cast<int>(denseSpanTileCache_.size());
}

int CacheWidget::testSelectedLogicalRow() const noexcept
{
    return selectedLogicalRow_;
}

int CacheWidget::testSelectedSpanIndex() const noexcept
{
    return selectedSpanIndex_;
}

bool CacheWidget::testHasCursor() const noexcept
{
    return hasCursor_;
}

qint64 CacheWidget::testCursorTimestamp() const noexcept
{
    return cursorTimestamp_;
}

bool CacheWidget::testHasMarker() const noexcept
{
    return hasMarker_;
}

int CacheWidget::testMarkerLogicalRow() const noexcept
{
    return markerLogicalRow_;
}

QRect CacheWidget::testPlotRect() const
{
    return plotRect();
}

QRect CacheWidget::testBuildPromptButtonRect() const
{
    return buildPromptButtonRect();
}

QRect CacheWidget::testAddBandButtonRect() const
{
    return addBandButtonRect();
}

QRect CacheWidget::testBandDeleteButtonRect(const int visibleBandIndex) const
{
    return bandDeleteButtonRect(visibleBandIndex);
}

QRect CacheWidget::testSpanVisualRect(const int spanIndex) const
{
    return spanVisualRect(spanIndex);
}

int CacheWidget::testSpanIndexAtPoint(const QPoint& position) const
{
    return spanIndexAtPosition(position).value_or(-1);
}

int CacheWidget::testVisiblePaintCandidateCount() const
{
    const QRect plot = plotRect();
    const auto [visibleStart, visibleEnd] = visibleTimestampRange();
    const auto [firstBand, lastBand] = visibleBandRange(plot);
    return static_cast<int>(visibleSpanIndices(visibleStart, visibleEnd, firstBand, lastBand).size());
}

bool CacheWidget::testGestureOverlayVisible() const noexcept
{
    return leftDragGestureActive_;
}

QString CacheWidget::testDragModeName() const
{
    return dragModeName(dragMode_);
}

quint64 CacheWidget::testBuildGeneration() const noexcept
{
    return buildGeneration_.load(std::memory_order_relaxed);
}

void CacheWidget::testInjectSyntheticSpans(const int bandCount, const int spanCount)
{
    stopBuildThread();
    traceSession_.reset();
    sourceMode_ = SourceMode::RowSnapshot;
    bands_.clear();
    spans_.clear();
    visibleBandIndices_.clear();
    resetView();
    buildRequested_ = true;
    totalRows_ = static_cast<std::uint64_t>(std::max(0, spanCount) * 3);
    firstTimestamp_ = 0;
    lastTimestamp_ = std::max<qint64>(100, static_cast<qint64>(std::max(1, spanCount)) * 10);
    haveTimeRange_ = true;
    minAddress_ = 0x1000;
    maxAddress_ = 0x1000 + static_cast<std::uint64_t>(std::max(1, spanCount)) * 64U;
    haveAddressRange_ = true;

    const int bandsToCreate = std::max(1, bandCount);
    bands_.reserve(static_cast<std::size_t>(bandsToCreate));
    for (int bandIndex = 0; bandIndex < bandsToCreate; ++bandIndex) {
        CacheRnBand band;
        band.rnNodeId = static_cast<std::uint32_t>(bandIndex);
        band.topologyLabel = QStringLiteral("RN %1").arg(bandIndex);
        band.nodeType = QStringLiteral("RN-F");
        bands_.push_back(std::move(band));
    }

    const int spansToCreate = std::max(0, spanCount);
    spans_.reserve(static_cast<std::size_t>(spansToCreate));
    for (int spanIndex = 0; spanIndex < spansToCreate; ++spanIndex) {
        const int bandIndex = spanIndex % bandsToCreate;
        CacheLifetimeSpan span;
        span.rnBandIndex = bandIndex;
        span.rnNodeId = bands_[static_cast<std::size_t>(bandIndex)].rnNodeId;
        span.lineAddress = 0x1000 + static_cast<std::uint64_t>(spanIndex) * 64U;
        span.startTimestamp = static_cast<qint64>(spanIndex) * 8;
        span.endTimestamp = span.startTimestamp + 6 + (spanIndex % 9);
        span.startLogicalRow = static_cast<std::uint64_t>(spanIndex) * 3U;
        span.endLogicalRow = span.startLogicalRow + 2U;
        span.startStateMask = 0x10;
        span.endStateMask = 0x40;
        span.startStateText = QStringLiteral("SC");
        span.endStateText = spanIndex % 7 == 0 ? QStringLiteral("SC, I") : QStringLiteral("I");
        span.openEnded = spanIndex % 11 == 0;
        spans_.push_back(std::move(span));
        ++bands_[static_cast<std::size_t>(bandIndex)].spanCount;
    }
    statusText_ = QStringLiteral("Cache ready: %1 lifetimes across %2 RN bands.")
                      .arg(FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(spans_.size())),
                           FormatUnsignedIntegerWithThousands(static_cast<std::uint64_t>(bands_.size())));
    rebuildSpanIndexes();
    updateScrollBars();
    update();
}

bool CacheWidget::testSetTraceSessionAndWaitForSpans(std::shared_ptr<TraceSession> session,
                                                     const int timeoutMs)
{
    setTraceSession(std::move(session));
    buildView();
    QElapsedTimer timer;
    timer.start();
    while (building_ && (timeoutMs <= 0 || timer.elapsed() < timeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return !building_ && !spans_.empty();
}

QVariantMap CacheWidget::testBuildSpansFromTraceSession(std::shared_ptr<TraceSession> session)
{
    stopBuildThread();
    const quint64 generation = ++buildGeneration_;
    std::shared_ptr<BuildResult> result = buildSpansFromSession(session, generation, std::stop_token{});
    QVariantMap summary;
    if (!result) {
        summary.insert(QStringLiteral("failed"), true);
        summary.insert(QStringLiteral("status"), QStringLiteral("Cache build returned no result."));
        return summary;
    }
    summary.insert(QStringLiteral("failed"), result->failed);
    summary.insert(QStringLiteral("cancelled"), result->cancelled);
    summary.insert(QStringLiteral("status"), result->statusText);
    summary.insert(QStringLiteral("spanCount"), static_cast<int>(result->spans.size()));
    summary.insert(QStringLiteral("bandCount"), static_cast<int>(result->bands.size()));
    if (!result->failed && !result->cancelled) {
        applyBuildResult(result);
    }
    return summary;
}

void CacheWidget::testSetHorizontalZoom(const double zoom)
{
    horizontalZoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
    clearDenseTileCache();
    normalizeView();
    updateScrollBars();
    update();
}

void CacheWidget::testSetHorizontalScroll(const int value)
{
    scrollToHorizontalOffset(static_cast<std::uint64_t>(std::max(0, value)));
}

void CacheWidget::testSetVerticalScroll(const int value)
{
    verticalScrollOffset_ = std::clamp(value, 0, verticalContentRange());
    updateScrollBars();
    update();
}

void CacheWidget::testSetVerticalZoom(const double bandScale, const double addressZoom)
{
    verticalBandScale_ = std::clamp(bandScale, kMinVerticalBandScale, kMaxVerticalBandScale);
    verticalAddressZoom_ = std::clamp(addressZoom, kMinVerticalAddressZoom, kMaxVerticalAddressZoom);
    normalizeVerticalView();
    clearDenseTileCache();
    normalizeView();
    updateScrollBars();
    update();
}

bool CacheWidget::testAddVisibleBand(const int sourceBandIndex)
{
    return addVisibleBand(sourceBandIndex);
}

bool CacheWidget::testRemoveVisibleBandAt(const int visibleBandIndex)
{
    return removeVisibleBandAt(visibleBandIndex);
}

#endif

}  // namespace CHIron::Gui
