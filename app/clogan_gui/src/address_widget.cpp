#include "address_widget.hpp"

#include "gui_colors.hpp"
#include "gui_format.hpp"

#include <QCursor>
#include <QFontMetrics>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace CHIron::Gui {

namespace {

constexpr int kHeaderHeight = 28;
constexpr int kTopRulerHeight = 44;
constexpr int kFullRulerHeight = 28;
constexpr int kScrollBarHeight = 16;
constexpr int kAddressAxisWidth = 148;
constexpr int kRightMargin = 10;
constexpr int kPlotPaddingY = 14;
constexpr int kMaxScrollBarRange = 1000000000;
constexpr int kRowsBuildProgressStep = 8192;
constexpr int kSessionReadChunkRows = 65536;
constexpr int kGestureActivationDistance = 7;
constexpr int kGestureDirectionDistance = 24;
constexpr int kEventHitRadius = 10;
constexpr int kEventDrawRadius = 4;
constexpr int kEventTagPaddingX = 5;
constexpr int kEventTagGap = 6;
constexpr int kEventTagMaxCandidatesPerPaint = 2048;
constexpr int kEventTagMaxTextWidth = 190;
constexpr double kEventTagMinimumPixelsPerRow = 3.0;
constexpr std::uint64_t kDenseEventPaintThreshold = 20000;
constexpr int kDenseEventCellShift = 1;
constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 1048576.0;
constexpr double kMinVerticalZoom = 1.0;
constexpr double kMaxVerticalZoom = 1048576.0;
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

double niceStep(const double roughStep)
{
    if (roughStep <= 1.0) {
        return 1.0;
    }
    const double magnitude = std::pow(10.0, std::floor(std::log10(roughStep)));
    const double normalized = roughStep / magnitude;
    if (normalized <= 1.0) {
        return magnitude;
    }
    if (normalized <= 2.0) {
        return 2.0 * magnitude;
    }
    if (normalized <= 5.0) {
        return 5.0 * magnitude;
    }
    return 10.0 * magnitude;
}

int scaledScrollBarValue(const std::uint64_t value, const std::uint64_t maxValue)
{
    if (maxValue == 0) {
        return 0;
    }
    const long double ratio = static_cast<long double>(value)
        / static_cast<long double>(maxValue);
    return qBound(0, static_cast<int>(std::llround(ratio * kMaxScrollBarRange)), kMaxScrollBarRange);
}

std::uint64_t unscaledScrollBarValue(const int value, const std::uint64_t maxValue)
{
    if (maxValue == 0) {
        return 0;
    }
    const long double ratio = static_cast<long double>(qBound(0, value, kMaxScrollBarRange))
        / static_cast<long double>(kMaxScrollBarRange);
    return static_cast<std::uint64_t>(std::llround(ratio * static_cast<long double>(maxValue)));
}

QColor kindColor(const AddressEventKind kind)
{
    switch (kind) {
    case AddressEventKind::Read:
        return QColor(QStringLiteral("#2563EB"));
    case AddressEventKind::Writeback:
        return QColor(QStringLiteral("#D97706"));
    case AddressEventKind::Snoop:
        return QColor(QStringLiteral("#7C3AED"));
    }
    return QColor(QStringLiteral("#64748B"));
}

QString kindLabel(const AddressEventKind kind)
{
    switch (kind) {
    case AddressEventKind::Read:
        return QStringLiteral("Read");
    case AddressEventKind::Writeback:
        return QStringLiteral("Evict / writeback");
    case AddressEventKind::Snoop:
        return QStringLiteral("Snoop");
    }
    return QStringLiteral("Address");
}

std::size_t kindIndex(const AddressEventKind kind) noexcept
{
    return static_cast<std::size_t>(kind);
}

AddressEventKind classifyAddressEvent(const FlitChannel channel, const QString& opcode)
{
    if (channel == FlitChannel::Snp) {
        return AddressEventKind::Snoop;
    }

    const QString folded = opcode.toCaseFolded();
    if (folded.contains(QStringLiteral("read"))
        || folded.contains(QStringLiteral("load"))
        || folded.contains(QStringLiteral("prefetch"))) {
        return AddressEventKind::Read;
    }

    return AddressEventKind::Writeback;
}

bool parseAddressText(QString text, std::uint64_t& value)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return false;
    }
    text.remove(QLatin1Char(','));

    static const QRegularExpression hexPattern(QStringLiteral("0[xX]([0-9a-fA-F]+)"));
    QRegularExpressionMatch hexMatch = hexPattern.match(text);
    if (hexMatch.hasMatch()) {
        bool ok = false;
        value = hexMatch.captured(1).toULongLong(&ok, 16);
        return ok;
    }

    static const QRegularExpression tokenPattern(QStringLiteral("([0-9a-fA-F]+)"));
    QRegularExpressionMatch tokenMatch = tokenPattern.match(text);
    if (!tokenMatch.hasMatch()) {
        return false;
    }

    const QString token = tokenMatch.captured(1);
    bool hasHexLetter = false;
    for (const QChar ch : token) {
        const ushort code = ch.unicode();
        if ((code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F')) {
            hasHexLetter = true;
            break;
        }
    }

    bool ok = false;
    value = token.toULongLong(&ok, hasHexLetter ? 16 : 10);
    return ok;
}

int addressHexDigitsFromMax(const std::uint64_t maxAddress)
{
    if (maxAddress <= 0xFFFFFFFFFFFFULL) {
        return 12;
    }
    return 16;
}

QString addressText(const std::uint64_t address, const int digits)
{
    return QStringLiteral("0x%1")
        .arg(QString::number(static_cast<qulonglong>(address), 16).toUpper()
                 .rightJustified(qBound(1, digits, 16), QLatin1Char('0')));
}

FlitChannel channelFromTransport(const CLog::Channel channel) noexcept
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::RXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
        return FlitChannel::Req;
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP:
        return FlitChannel::Rsp;
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT:
        return FlitChannel::Dat;
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP:
        return FlitChannel::Snp;
    }
    return FlitChannel::Req;
}

bool validTransportChannel(const std::uint8_t value) noexcept
{
    switch (static_cast<CLog::Channel>(value)) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXRSP:
    case CLog::Channel::TXDAT:
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXREQ:
    case CLog::Channel::RXRSP:
    case CLog::Channel::RXDAT:
    case CLog::Channel::RXSNP:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
        return true;
    }
    return false;
}

bool addressTransportChannel(const CLog::Channel channel) noexcept
{
    const FlitChannel flitChannel = channelFromTransport(channel);
    return flitChannel == FlitChannel::Req || flitChannel == FlitChannel::Snp;
}

void includeTimestamp(AddressWidget::BuildResult& result, const qint64 timestamp)
{
    if (!result.haveTimeRange) {
        result.firstTimestamp = timestamp;
        result.lastTimestamp = timestamp;
        result.haveTimeRange = true;
        return;
    }
    result.firstTimestamp = std::min(result.firstTimestamp, timestamp);
    result.lastTimestamp = std::max(result.lastTimestamp, timestamp);
}

std::uint32_t ensureOpcodeLabel(std::vector<QString>& labels,
                                QHash<QString, std::uint32_t>& labelIndexes,
                                const QString& label)
{
    if (label.isEmpty()) {
        return 0;
    }
    if (labels.empty()) {
        labels.push_back(QString());
    }

    const auto found = labelIndexes.constFind(label);
    if (found != labelIndexes.cend()) {
        return found.value();
    }
    if (labels.size() >= std::numeric_limits<std::uint32_t>::max()) {
        return 0;
    }

    labels.push_back(InternDisplayString(label));
    const std::uint32_t index = static_cast<std::uint32_t>(labels.size() - 1);
    labelIndexes.insert(labels.back(), index);
    return index;
}

void addAddressEvent(AddressWidget::BuildResult& result,
                     const std::uint64_t logicalRow,
                     const qint64 timestamp,
                     const std::uint64_t address,
                     const AddressEventKind kind,
                     const std::uint32_t opcodeLabelIndex)
{
    AddressWidget::AddressEvent event;
    event.logicalRow = logicalRow;
    event.timestamp = timestamp;
    event.address = address;
    event.kind = kind;
    event.opcodeLabelIndex = opcodeLabelIndex;
    result.events.push_back(event);
    ++result.kindCounts[kindIndex(kind)];

    if (!result.haveAddressRange) {
        result.minAddress = address;
        result.maxAddress = address;
        result.haveAddressRange = true;
        return;
    }
    result.minAddress = std::min(result.minAddress, address);
    result.maxAddress = std::max(result.maxAddress, address);
}

}  // namespace

AddressWidget::AddressWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("addressWidget"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumHeight(180);

    horizontalScrollBar_ = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar_->setObjectName(QStringLiteral("addressHorizontalScrollBar"));
    connect(horizontalScrollBar_, &QScrollBar::valueChanged, this, [this](const int value) {
        const std::uint64_t maxOffset = horizontalContentRange();
        const std::uint64_t offset = unscaledScrollBarValue(value, maxOffset);
        if (scrollOffset_ != offset) {
            scrollOffset_ = offset;
            update();
        }
    });

    statusText_ = QStringLiteral("Open a trace to populate address view.");
    rebuildContentMetrics();
    updateScrollBar();
}

AddressWidget::~AddressWidget()
{
    stopBuildThread();
}

void AddressWidget::setTraceSession(std::shared_ptr<TraceSession> session)
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_ = std::move(session);
    rowSnapshot_.reset();
    events_.clear();
    opcodeLabels_.clear();
    kindCounts_.fill(0);
    selectedLogicalRow_ = -1;
    markerLogicalRow_ = -1;
    hoveredEventTagIndex_.reset();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalZoom_ = 1.0;
    verticalCenterAddress_ = 0.0L;
    selectedAddress_.reset();
    minAddress_ = 0;
    maxAddress_ = 0;
    firstTimestamp_ = 0;
    lastTimestamp_ = 0;
    haveAddressRange_ = false;
    haveTimeRange_ = false;
    totalRows_ = traceSession_ ? traceSession_->totalRows() : 0;
    processedRows_ = 0;
    building_ = false;
    buildRequested_ = false;
    statusText_ = traceSession_ ? pendingBuildText()
                                : QStringLiteral("Open a trace to populate address view.");
    rebuildContentMetrics();
    updateScrollBar();
    update();
}

void AddressWidget::setRows(std::vector<FlitRecord> rows)
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_.reset();
    rowSnapshot_ = std::make_shared<const std::vector<FlitRecord>>(std::move(rows));
    events_.clear();
    opcodeLabels_.clear();
    kindCounts_.fill(0);
    selectedLogicalRow_ = -1;
    markerLogicalRow_ = -1;
    hoveredEventTagIndex_.reset();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalZoom_ = 1.0;
    verticalCenterAddress_ = 0.0L;
    selectedAddress_.reset();
    minAddress_ = 0;
    maxAddress_ = 0;
    firstTimestamp_ = 0;
    lastTimestamp_ = 0;
    haveAddressRange_ = false;
    haveTimeRange_ = false;
    totalRows_ = rowSnapshot_ ? rowSnapshot_->size() : 0;
    processedRows_ = 0;
    building_ = false;
    buildRequested_ = false;
    statusText_ = totalRows_ == 0 ? QStringLiteral("No flits in the current trace.")
                                  : pendingBuildText();
    rebuildContentMetrics();
    updateScrollBar();
    update();
}

void AddressWidget::buildView()
{
    if (building_) {
        return;
    }

    if (traceSession_) {
        buildRequested_ = true;
        building_ = true;
        statusText_ = QStringLiteral("Address: reading fast trace index...");
        update();
        startSessionBuild(traceSession_);
        return;
    }

    if (rowSnapshot_) {
        buildRequested_ = true;
        totalRows_ = rowSnapshot_->size();
        processedRows_ = 0;
        building_ = totalRows_ > 50000;
        statusText_ = totalRows_ == 0 ? QStringLiteral("No flits in the current trace.")
                                      : QStringLiteral("Address: collecting REQ/SNP addresses...");
        update();
        startRowsBuild(rowSnapshot_, !building_);
        return;
    }

    statusText_ = QStringLiteral("Open a trace to populate address view.");
    update();
}

void AddressWidget::clear()
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_.reset();
    rowSnapshot_.reset();
    events_.clear();
    opcodeLabels_.clear();
    kindCounts_.fill(0);
    viewHistory_.clear();
    totalRows_ = 0;
    processedRows_ = 0;
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalZoom_ = 1.0;
    verticalCenterAddress_ = 0.0L;
    minAddress_ = 0;
    maxAddress_ = 0;
    firstTimestamp_ = 0;
    lastTimestamp_ = 0;
    haveAddressRange_ = false;
    haveTimeRange_ = false;
    selectedLogicalRow_ = -1;
    selectedAddress_.reset();
    markerLogicalRow_ = -1;
    hoveredEventTagIndex_.reset();
    building_ = false;
    buildRequested_ = false;
    statusText_ = QStringLiteral("Open a trace to populate address view.");
    rebuildContentMetrics();
    updateScrollBar();
    update();
}

void AddressWidget::setSelectedLogicalRow(const int logicalRow)
{
    if (logicalRow < 0) {
        selectedLogicalRow_ = -1;
        selectedAddress_.reset();
        hoveredEventTagIndex_.reset();
        update();
        return;
    }
    selectedLogicalRow_ = logicalRow;
    if (hasPendingBuild()) {
        update();
        return;
    }
    setCursorLogicalRow(logicalRow, false);
}

QVariantMap AddressWidget::viewState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("horizontalZoom"), horizontalZoom_);
    state.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(
                     static_cast<qulonglong>(scrollOffset_)));
    state.insert(QStringLiteral("verticalZoom"), verticalZoom_);
    state.insert(QStringLiteral("verticalCenterAddress"), static_cast<double>(verticalCenterAddress_));
    state.insert(QStringLiteral("selectedLogicalRow"), selectedLogicalRow_);
    state.insert(QStringLiteral("markerLogicalRow"), markerLogicalRow_);
    if (selectedAddress_) {
        state.insert(QStringLiteral("selectedAddress"),
                     QVariant::fromValue<qulonglong>(static_cast<qulonglong>(*selectedAddress_)));
    }
    state.insert(QStringLiteral("readVisible"), kindVisible_[0]);
    state.insert(QStringLiteral("writebackVisible"), kindVisible_[1]);
    state.insert(QStringLiteral("snoopVisible"), kindVisible_[2]);
    return state;
}

void AddressWidget::restoreViewState(const QVariantMap& state)
{
    pendingViewState_ = state;
    applyViewState(state);
}

QSize AddressWidget::minimumSizeHint() const
{
    return QSize(440, 180);
}

QSize AddressWidget::sizeHint() const
{
    return QSize(980, 300);
}

void AddressWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildContentMetrics();
    updateScrollBar();
}

void AddressWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), QColor(QStringLiteral("#FFFFFF")));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    paintHeader(painter);
    paintTopRuler(painter);
    paintAddressAxis(painter);
    paintEvents(painter);
    paintRangeZoom(painter);
    paintGestureOverlay(painter);
    paintFullRuler(painter);
    paintStatus(painter);
    paintCursorAddressTag(painter);
    paintBuildPrompt(painter);
}

void AddressWidget::wheelEvent(QWheelEvent* event)
{
    if (hasPendingBuild()) {
        event->ignore();
        return;
    }

    const QPoint pixelDelta = event->pixelDelta();
    const QPoint angleDelta = event->angleDelta();
    const int verticalDelta = pixelDelta.y() != 0 ? pixelDelta.y() : angleDelta.y();
    const int horizontalDelta = pixelDelta.x() != 0 ? pixelDelta.x() : angleDelta.x();

    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        zoomVerticallyByFactor(verticalDelta > 0 ? 1.25 : 0.8, event->position().toPoint());
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        zoomByFactor(verticalDelta > 0 ? 1.25 : 0.8, event->position().toPoint());
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        const int delta = verticalDelta == 0 ? horizontalDelta : verticalDelta;
        panHorizontallyByPixels(delta < 0 ? timelineViewportWidth() / 4 : -timelineViewportWidth() / 4);
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::AltModifier) {
        const int delta = verticalDelta == 0 ? horizontalDelta : verticalDelta;
        panHorizontallyByPixels(delta < 0 ? 48 : -48);
        event->accept();
        return;
    }

    if (horizontalDelta != 0) {
        panHorizontallyByPixels(horizontalDelta < 0 ? 80 : -80);
        event->accept();
        return;
    }

    if (verticalDelta != 0) {
        scrollVerticallyByPixels(verticalDelta < 0 ? -80 : 80);
        event->accept();
        return;
    }

    event->ignore();
}

void AddressWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    dragStart_ = event->pos();
    dragLast_ = event->pos();

    if (hasPendingBuild()) {
        if (event->button() == Qt::LeftButton) {
            buildView();
            event->accept();
            return;
        }
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton && toggleKindAtPosition(event->pos())) {
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)
        && positionInTimeline(event->pos())) {
        beginRangeZoom(event->pos());
        event->accept();
        return;
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

void AddressWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && (positionInTimeline(event->pos()) || positionInRuler(event->pos()))) {
        zoomToFit();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AddressWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragMode_ == DragMode::PendingGesture) {
        updatePendingGesture(event->pos(), event->modifiers());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::RangeZoom) {
        if (leftDragGestureActive_) {
            updateLeftDragGesture(event->pos(), event->modifiers());
        } else {
            updateRangeZoom(event->pos());
        }
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::FullRangeZoom) {
        if (leftDragGestureActive_) {
            updateLeftDragGesture(event->pos(), event->modifiers());
        } else {
            updateFullRangeZoom(event->pos());
        }
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ZoomFull
        || dragMode_ == DragMode::ZoomIn2x
        || dragMode_ == DragMode::ZoomOut2x) {
        updateLeftDragGesture(event->pos(), event->modifiers());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::Pan) {
        if (leftDragGestureActive_ && (event->buttons() & Qt::LeftButton)) {
            updateLeftDragGesture(event->pos(), event->modifiers());
        } else {
            updatePan(event->pos());
        }
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::OverviewPan) {
        updateOverviewPan(event->pos());
        event->accept();
        return;
    }

    updateHoveredEventTag(event->pos());
    QWidget::mouseMoveEvent(event);
}

void AddressWidget::leaveEvent(QEvent* event)
{
    if (hoveredEventTagIndex_) {
        hoveredEventTagIndex_.reset();
        update(plotRect());
    }
    QWidget::leaveEvent(event);
}

void AddressWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && leftDragGestureActive_
        && (dragMode_ == DragMode::PendingGesture
            || dragMode_ == DragMode::RangeZoom
            || dragMode_ == DragMode::FullRangeZoom
            || dragMode_ == DragMode::ZoomFull
            || dragMode_ == DragMode::ZoomIn2x
            || dragMode_ == DragMode::ZoomOut2x
            || dragMode_ == DragMode::Pan
            || dragMode_ == DragMode::OverviewPan)) {
        updateLeftDragGesture(event->pos(), event->modifiers());
    }

    if (dragMode_ == DragMode::PendingGesture && event->button() == Qt::LeftButton) {
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        if (const std::optional<CursorPosition> position = cursorPositionAtPosition(event->pos())) {
            setCursorPosition(position->logicalRow, position->address, true);
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

    if (dragMode_ == DragMode::FullRangeZoom && event->button() == Qt::LeftButton) {
        updateFullRangeZoom(event->pos());
        finishFullRangeZoom();
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

    if (dragMode_ == DragMode::OverviewPan && event->button() == Qt::LeftButton) {
        updateOverviewPan(event->pos());
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void AddressWidget::keyPressEvent(QKeyEvent* event)
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
            moveCursorToAdjacentEvent(-1);
        } else {
            panHorizontallyByPixels(shift ? -timelineViewportWidth() / 2 : -80);
        }
        event->accept();
        return;
    case Qt::Key_Right:
        if (ctrl) {
            moveCursorToAdjacentEvent(1);
        } else {
            panHorizontallyByPixels(shift ? timelineViewportWidth() / 2 : 80);
        }
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
        if (ctrl) {
            setCursorLogicalRow(0, true);
        }
        event->accept();
        return;
    case Qt::Key_End:
        if (ctrl) {
            zoomToFit();
        }
        scrollToHorizontalOffset(horizontalContentRange());
        if (ctrl && totalRows_ > 0) {
            setCursorLogicalRow(static_cast<int>(totalRows_ - 1), true);
        }
        event->accept();
        return;
    case Qt::Key_C:
        if (ctrl) {
            centerSelectedLogicalRow();
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
    case Qt::Key_B:
        if (ctrl) {
            restorePreviousView();
            event->accept();
            return;
        }
        break;
    case Qt::Key_S:
        if (ctrl) {
            snapToEvents_ = !snapToEvents_;
            update();
            event->accept();
            return;
        }
        break;
    case Qt::Key_D:
        if (ctrl) {
            lockCursorMarkerDelta_ = !lockCursorMarkerDelta_;
            update();
            event->accept();
            return;
        }
        break;
    case Qt::Key_Escape:
        if (dragMode_ == DragMode::RangeZoom || dragMode_ == DragMode::FullRangeZoom) {
            dragMode_ = DragMode::None;
            update();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void AddressWidget::stopBuildThread()
{
    ++buildGeneration_;
    if (buildThread_.joinable()) {
        buildThread_.request_stop();
    }
    building_ = false;
}

void AddressWidget::startRowsBuild(const std::shared_ptr<const std::vector<FlitRecord>> rows,
                                   const bool synchronous)
{
    const quint64 generation = ++buildGeneration_;
    if (!rows) {
        return;
    }

    auto build = [this, rows, generation]() {
        auto result = std::make_shared<BuildResult>();
        result->generation = generation;
        result->totalRows = rows->size();
        result->statusText = result->totalRows == 0
            ? QStringLiteral("No flits in the current trace.")
            : QStringLiteral("Address ready.");
        QHash<QString, std::uint32_t> opcodeLabelIndexes;
        result->opcodeLabels.push_back(QString());
        result->events.reserve(rows->size() / 2);

        for (std::size_t index = 0; index < rows->size(); ++index) {
            if (generation != buildGeneration_) {
                result->cancelled = true;
                return result;
            }

            const FlitRecord& record = rows->at(index);
            includeTimestamp(*result, record.timestamp);
            if (record.channel != FlitChannel::Req && record.channel != FlitChannel::Snp) {
                continue;
            }
            std::uint64_t address = 0;
            if (!parseAddressText(record.address, address)) {
                continue;
            }

            const std::uint32_t opcodeLabelIndex =
                ensureOpcodeLabel(result->opcodeLabels, opcodeLabelIndexes, record.opcode);
            addAddressEvent(*result,
                            static_cast<std::uint64_t>(index),
                            record.timestamp,
                            address,
                            classifyAddressEvent(record.channel, record.opcode),
                            opcodeLabelIndex);

            if (index % kRowsBuildProgressStep == 0) {
                result->processedRows = index;
                postBuildProgress(generation, result->processedRows, result->totalRows);
            }
        }

        result->processedRows = result->totalRows;
        std::sort(result->events.begin(), result->events.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.logicalRow < rhs.logicalRow;
        });
        if (result->events.empty() && result->totalRows > 0) {
            result->statusText = QStringLiteral("No REQ/SNP addresses in the current trace.");
        }
        return result;
    };

    if (synchronous) {
        applyBuildResult(build());
        return;
    }

    buildThread_ = std::jthread([this, generation, build = std::move(build)](const std::stop_token stopToken) mutable {
        if (stopToken.stop_requested()) {
            return;
        }
        auto result = build();
        if (stopToken.stop_requested()) {
            return;
        }
        QTimer::singleShot(0, this, [this, result, generation]() {
            if (generation == buildGeneration_) {
                applyBuildResult(result);
            }
        });
    });
}

void AddressWidget::startSessionBuild(std::shared_ptr<TraceSession> session)
{
    const quint64 generation = ++buildGeneration_;
    buildThread_ = std::jthread([this, session = std::move(session), generation](const std::stop_token stopToken) {
        auto result = std::make_shared<BuildResult>();
        result->generation = generation;
        result->totalRows = session ? session->totalRows() : 0;
        QHash<QString, std::uint32_t> opcodeLabelIndexes;
        QHash<quint64, std::uint32_t> opcodeFastLabelIndexes;
        result->opcodeLabels.push_back(QString());

        if (!session) {
            result->statusText = QStringLiteral("Open a trace to populate address view.");
        } else {
            result->statusText = QStringLiteral("Address ready.");
            const CLogBTraceMetadata& metadata = session->metadata();
            if (metadata.totalRecords > 0) {
                result->firstTimestamp = std::min(metadata.firstTimestamp, metadata.lastTimestamp);
                result->lastTimestamp = std::max(metadata.firstTimestamp, metadata.lastTimestamp);
                result->haveTimeRange = true;
            }
            result->events.reserve(static_cast<std::size_t>(
                std::min<std::uint64_t>(result->totalRows, 2'000'000)));
            CLogBTraceLoadError error;
            const auto& blocks = metadata.blocks;
            for (std::size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
                if (generation != buildGeneration_ || stopToken.stop_requested()) {
                    result->cancelled = true;
                    break;
                }
                const CLogBTraceBlockSummary& block = blocks[blockIndex];
                std::size_t localBegin = 0;
                while (localBegin < block.recordCount) {
                    if (generation != buildGeneration_ || stopToken.stop_requested()) {
                        result->cancelled = true;
                        break;
                    }
                    const std::size_t localCount = std::min<std::size_t>(
                        kSessionReadChunkRows,
                        static_cast<std::size_t>(block.recordCount) - localBegin);
                    std::vector<CLogBTraceFastRecordInfo> records;
                    if (!session->readFastRecords(blockIndex,
                                                  localBegin,
                                                  localCount,
                                                  records,
                                                  error,
                                                  stopToken)) {
                        result->failed = true;
                        result->statusText = QStringLiteral("Address unavailable: %1")
                                                 .arg(error.summary.isEmpty()
                                                          ? QStringLiteral("failed to read fast records")
                                                          : error.summary);
                        break;
                    }
                    for (std::size_t index = 0; index < records.size(); ++index) {
                        const CLogBTraceFastRecordInfo& record = records[index];
                        includeTimestamp(*result, record.timestamp);
                        if (!validTransportChannel(record.channel)) {
                            continue;
                        }
                        const CLog::Channel transportChannel = static_cast<CLog::Channel>(record.channel);
                        if (!addressTransportChannel(transportChannel)
                            || record.address == std::numeric_limits<std::uint64_t>::max()) {
                            continue;
                        }

                        const FlitChannel channel = channelFromTransport(transportChannel);
                        const quint64 fastOpcodeKey =
                            (static_cast<quint64>(record.channel) << 32U)
                            | static_cast<quint64>(record.opcode);
                        QString opcode;
                        std::uint32_t opcodeLabelIndex = 0;
                        const auto cachedOpcode = opcodeFastLabelIndexes.constFind(fastOpcodeKey);
                        if (cachedOpcode != opcodeFastLabelIndexes.cend()) {
                            opcodeLabelIndex = cachedOpcode.value();
                            if (opcodeLabelIndex < result->opcodeLabels.size()) {
                                opcode = result->opcodeLabels[opcodeLabelIndex];
                            }
                        } else {
                            opcode = CLogBTraceLoader::opcodeDisplayValue(
                                session->metadata().parameters,
                                record);
                            opcodeLabelIndex = ensureOpcodeLabel(result->opcodeLabels,
                                                                 opcodeLabelIndexes,
                                                                 opcode);
                            opcodeFastLabelIndexes.insert(fastOpcodeKey, opcodeLabelIndex);
                        }

                        const std::uint64_t logicalRow =
                            block.rowStart + static_cast<std::uint64_t>(localBegin + index);
                        addAddressEvent(*result,
                                        logicalRow,
                                        record.timestamp,
                                        record.address,
                                        classifyAddressEvent(channel, opcode),
                                        opcodeLabelIndex);
                    }
                    localBegin += localCount;
                    result->processedRows =
                        std::min<std::uint64_t>(result->totalRows, block.rowStart + localBegin);
                    const std::uint64_t processed = result->processedRows;
                    const std::uint64_t total = result->totalRows;
                    if (stopToken.stop_requested()) {
                        result->cancelled = true;
                        break;
                    }
                    postBuildProgress(generation, processed, total);
                }
                if (result->cancelled || result->failed) {
                    break;
                }
            }
        }

        if (!result->cancelled && !result->failed) {
            result->processedRows = result->totalRows;
            std::sort(result->events.begin(), result->events.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.logicalRow < rhs.logicalRow;
            });
            if (result->events.empty() && result->totalRows > 0) {
                result->statusText = QStringLiteral("No REQ/SNP addresses in the current trace.");
            }
        }

        if (stopToken.stop_requested()) {
            return;
        }
        QTimer::singleShot(0, this, [this, result, generation]() {
            if (generation == buildGeneration_) {
                applyBuildResult(result);
            }
        });
    });
}

void AddressWidget::applyBuildResult(std::shared_ptr<BuildResult> result)
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    if (!result || result->generation != buildGeneration_ || result->cancelled) {
        return;
    }

    events_ = std::move(result->events);
    opcodeLabels_ = std::move(result->opcodeLabels);
    kindCounts_ = result->kindCounts;
    totalRows_ = result->totalRows;
    processedRows_ = result->processedRows;
    minAddress_ = result->minAddress;
    maxAddress_ = result->maxAddress;
    firstTimestamp_ = result->firstTimestamp;
    lastTimestamp_ = result->lastTimestamp;
    haveAddressRange_ = result->haveAddressRange;
    haveTimeRange_ = result->haveTimeRange;
    statusText_ = result->statusText;
    building_ = false;
    resetVerticalView();
    rebuildContentMetrics();
    updateScrollBar();
    if (selectedLogicalRow_ >= 0) {
        const int selectedLogicalRow = selectedLogicalRow_;
        setCursorLogicalRow(selectedLogicalRow, false);
    }
    updateGeometry();
    update();
    if (!pendingViewState_.isEmpty()) {
        const QVariantMap state = pendingViewState_;
        pendingViewState_.clear();
        applyViewState(state);
    }
}

void AddressWidget::postBuildProgress(const quint64 generation,
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
        const quint64 queuedGeneration =
            pendingProgressGeneration_.load(std::memory_order_acquire);
        const std::uint64_t processedRows =
            pendingProgressProcessedRows_.load(std::memory_order_relaxed);
        const std::uint64_t totalRows =
            pendingProgressTotalRows_.load(std::memory_order_relaxed);

        progressUpdateQueued_.store(false, std::memory_order_release);
        updateBuildProgress(queuedGeneration, processedRows, totalRows);

        const quint64 latestGeneration =
            pendingProgressGeneration_.load(std::memory_order_acquire);
        const std::uint64_t latestProcessedRows =
            pendingProgressProcessedRows_.load(std::memory_order_relaxed);
        const std::uint64_t latestTotalRows =
            pendingProgressTotalRows_.load(std::memory_order_relaxed);
        if (latestGeneration == queuedGeneration
            && latestProcessedRows == processedRows
            && latestTotalRows == totalRows) {
            return;
        }

        postBuildProgress(latestGeneration, latestProcessedRows, latestTotalRows);
    });
}

void AddressWidget::updateBuildProgress(const quint64 generation,
                                        const std::uint64_t processedRows,
                                        const std::uint64_t totalRows)
{
    if (generation != buildGeneration_ || !building_) {
        return;
    }
    processedRows_ = processedRows;
    totalRows_ = totalRows;
    statusText_ = QStringLiteral("Address: collecting addresses %1/%2")
                      .arg(processedRows_)
                      .arg(totalRows_);
    update();
}

bool AddressWidget::hasPendingBuild() const noexcept
{
    return !building_
        && !buildRequested_
        && events_.empty()
        && ((traceSession_ && totalRows_ > 0) || (rowSnapshot_ && !rowSnapshot_->empty()));
}

bool AddressWidget::shouldShowBuildPrompt() const noexcept
{
    return (hasPendingBuild() || (building_ && buildRequested_ && events_.empty()))
        && ((traceSession_ && totalRows_ > 0) || (rowSnapshot_ && !rowSnapshot_->empty()));
}

QString AddressWidget::pendingBuildText() const
{
    const std::uint64_t rowCount = traceSession_
        ? traceSession_->totalRows()
        : (rowSnapshot_ ? rowSnapshot_->size() : 0);
    if (rowCount == 0) {
        return QStringLiteral("No flits in the current trace.");
    }
    return QStringLiteral("Address view is ready to build. Click here to collect addresses from %1 rows.")
        .arg(FormatUnsignedIntegerWithThousands(rowCount));
}

QRect AddressWidget::buildPromptButtonRect() const
{
    const QRect body = rect().adjusted(24, kHeaderHeight + kTopRulerHeight + 18, -24, -kFullRulerHeight - 24);
    const int buttonWidth = qMin(250, qMax(150, body.width() - 48));
    const int buttonHeight = 34;
    return QRect(body.center().x() - buttonWidth / 2,
                 body.center().y() + 20,
                 buttonWidth,
                 buttonHeight);
}

void AddressWidget::paintBuildPrompt(QPainter& painter) const
{
    if (!shouldShowBuildPrompt()) {
        return;
    }

    painter.save();
    painter.fillRect(rect(), QColor(255, 255, 255, 214));

    const QRect content = rect().adjusted(28, kHeaderHeight + 36, -28, -34);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPixelSize(qMax(13, titleFont.pixelSize() > 0 ? titleFont.pixelSize() + 2 : 15));
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#18212B")));
    painter.drawText(content.adjusted(0, 0, 0, -52),
                     Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                     building_ ? statusText_ : pendingBuildText());

    const QRect button = buildPromptButtonRect();
    if (building_) {
        paintBuildProgress(painter, button);
    } else {
        painter.setPen(QColor(QStringLiteral("#1D5F8F")));
        painter.setBrush(QColor(QStringLiteral("#EAF4FB")));
        painter.drawRect(button.adjusted(0, 0, -1, -1));
        painter.setPen(QColor(QStringLiteral("#174B72")));
        painter.setFont(font());
        painter.drawText(button, Qt::AlignCenter, QStringLiteral("Build Address View"));
    }
    painter.restore();
}

void AddressWidget::paintBuildProgress(QPainter& painter, const QRect& bounds) const
{
    painter.save();
    const QRect track = bounds.adjusted(0, 4, 0, -4);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(QStringLiteral("#B7C8D8")));
    painter.setBrush(QColor(QStringLiteral("#EEF5FA")));
    painter.drawRect(track.adjusted(0, 0, -1, -1));

    const int fillWidth = totalRows_ == 0
        ? 0
        : static_cast<int>(std::llround(
              static_cast<double>(track.width())
              * static_cast<double>(std::min(processedRows_, totalRows_))
              / static_cast<double>(totalRows_)));
    if (fillWidth > 0) {
        const QRect fill(track.left(), track.top(), qMin(track.width(), fillWidth), track.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#2F80ED")));
        painter.drawRect(fill.adjusted(0, 0, -1, -1));
    }

    const QString text = totalRows_ > 0
        ? QStringLiteral("%1 / %2")
              .arg(FormatUnsignedIntegerWithThousands(processedRows_))
              .arg(FormatUnsignedIntegerWithThousands(totalRows_))
        : QStringLiteral("Building...");
    painter.setPen(QColor(QStringLiteral("#174B72")));
    painter.setFont(font());
    painter.drawText(track, Qt::AlignCenter, text);
    painter.restore();
}

void AddressWidget::applyViewState(const QVariantMap& state)
{
    if (state.isEmpty()) {
        return;
    }

    horizontalZoom_ = std::clamp(state.value(QStringLiteral("horizontalZoom"), horizontalZoom_).toDouble(),
                                 kMinZoom,
                                 kMaxZoom);
    verticalZoom_ = std::clamp(state.value(QStringLiteral("verticalZoom"), verticalZoom_).toDouble(),
                               kMinVerticalZoom,
                               kMaxVerticalZoom);
    verticalCenterAddress_ =
        static_cast<long double>(state.value(QStringLiteral("verticalCenterAddress"),
                                             static_cast<double>(verticalCenterAddress_)).toDouble());
    if (kindVisible_.size() >= 3) {
        kindVisible_[0] = state.value(QStringLiteral("readVisible"), kindVisible_[0]).toBool();
        kindVisible_[1] = state.value(QStringLiteral("writebackVisible"), kindVisible_[1]).toBool();
        kindVisible_[2] = state.value(QStringLiteral("snoopVisible"), kindVisible_[2]).toBool();
    }

    normalizeVerticalView();
    rebuildContentMetrics();
    scrollToHorizontalOffset(static_cast<std::uint64_t>(
        state.value(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(0)).toULongLong()));

    selectedLogicalRow_ = state.value(QStringLiteral("selectedLogicalRow"), selectedLogicalRow_).toInt();
    markerLogicalRow_ = state.value(QStringLiteral("markerLogicalRow"), markerLogicalRow_).toInt();
    const int maxLogicalRow = totalRows_ == 0
        ? -1
        : static_cast<int>(std::min<std::uint64_t>(
            totalRows_ - 1,
            static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
    selectedLogicalRow_ = qBound(-1, selectedLogicalRow_, maxLogicalRow);
    markerLogicalRow_ = qBound(-1, markerLogicalRow_, maxLogicalRow);
    if (state.contains(QStringLiteral("selectedAddress"))
        && state.value(QStringLiteral("selectedAddress")).isValid()) {
        selectedAddress_ = static_cast<std::uint64_t>(
            state.value(QStringLiteral("selectedAddress")).toULongLong());
    } else if (selectedLogicalRow_ >= 0) {
        selectedAddress_ = addressForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
    } else {
        selectedAddress_.reset();
    }
    hoveredEventTagIndex_.reset();
    updateScrollBar();
    update();
}

void AddressWidget::rebuildContentMetrics()
{
    contentWidth_ = timelineContentWidth();
    if (scrollOffset_ > horizontalContentRange()) {
        scrollOffset_ = horizontalContentRange();
    }
}

void AddressWidget::updateScrollBarGeometry()
{
    if (horizontalScrollBar_) {
        horizontalScrollBar_->setGeometry(horizontalScrollRect());
    }
}

void AddressWidget::updateScrollBar()
{
    updateScrollBarGeometry();
    const std::uint64_t maxHorizontal = horizontalContentRange();
    const QSignalBlocker blocker(horizontalScrollBar_);
    horizontalScrollBar_->setRange(0, maxHorizontal == 0 ? 0 : kMaxScrollBarRange);
    horizontalScrollBar_->setPageStep(qMax(1, scaledScrollBarValue(timelineViewportWidth(), contentWidth_)));
    horizontalScrollBar_->setValue(scaledScrollBarValue(scrollOffset_, maxHorizontal));
    horizontalScrollBar_->setVisible(maxHorizontal > 0);
}

std::vector<AddressWidget::LegendItem> AddressWidget::legendItems() const
{
    const QRect header = headerRect();
    const QRect plotHeader(kAddressAxisWidth, 0, width() - kAddressAxisWidth, header.height());
    const QString title = QStringLiteral("Address");

    QFont legendFont = font();
    legendFont.setPointSizeF(qMax<qreal>(6.0, legendFont.pointSizeF() * 0.9));
    const QFontMetrics metrics(legendFont);

    std::vector<LegendItem> items;
    int legendRight = plotHeader.right() - 8;
    const std::array<AddressEventKind, 3> kinds = {
        AddressEventKind::Snoop,
        AddressEventKind::Writeback,
        AddressEventKind::Read,
    };
    for (const AddressEventKind kind : kinds) {
        const QString text = QStringLiteral("%1 %2")
                                 .arg(kindLabel(kind))
                                 .arg(kindCounts_[kindIndex(kind)]);
        const int itemWidth = metrics.horizontalAdvance(text) + 28;
        QRect itemRect(legendRight - itemWidth + 1,
                       header.top() + 4,
                       itemWidth,
                       header.height() - 8);
        if (itemRect.left() < plotHeader.left() + metrics.horizontalAdvance(title) + 24) {
            break;
        }
        QRect markerRect(itemRect.left() + 3, itemRect.center().y() - 5, 10, 10);
        items.push_back({kind, itemRect, markerRect, text});
        legendRight = itemRect.left() - 10;
    }
    return items;
}

bool AddressWidget::toggleKindAtPosition(const QPoint& position)
{
    if (!headerRect().contains(position)) {
        return false;
    }
    const std::vector<LegendItem> items = legendItems();
    for (const LegendItem& item : items) {
        if (!item.rect.contains(position)) {
            continue;
        }
        const std::size_t index = kindIndex(item.kind);
        if (index >= kindVisible_.size()) {
            return false;
        }
        kindVisible_[index] = !kindVisible_[index];
        hoveredEventTagIndex_.reset();
        update();
        return true;
    }
    return false;
}

bool AddressWidget::isKindVisible(const AddressEventKind kind) const noexcept
{
    const std::size_t index = kindIndex(kind);
    return index < kindVisible_.size() && kindVisible_[index];
}

bool AddressWidget::eventVisible(const AddressEvent& event) const noexcept
{
    return isKindVisible(event.kind);
}

std::uint64_t AddressWidget::visibleAddressEventCount() const noexcept
{
    std::uint64_t count = 0;
    for (std::size_t index = 0; index < kindCounts_.size() && index < kindVisible_.size(); ++index) {
        if (kindVisible_[index]) {
            count += kindCounts_[index];
        }
    }
    return count;
}

void AddressWidget::resetVerticalView()
{
    verticalZoom_ = 1.0;
    if (!haveAddressRange_) {
        verticalCenterAddress_ = 0.0L;
        return;
    }
    const auto [globalMin, globalMax] = globalAddressViewRange();
    verticalCenterAddress_ = (globalMin + globalMax) / 2.0L;
    normalizeVerticalView();
}

void AddressWidget::normalizeVerticalView()
{
    verticalZoom_ = std::clamp(verticalZoom_, kMinVerticalZoom, kMaxVerticalZoom);
    if (!haveAddressRange_) {
        verticalCenterAddress_ = 0.0L;
        return;
    }

    const auto [globalMin, globalMax] = globalAddressViewRange();
    if (globalMax <= globalMin) {
        verticalCenterAddress_ = (globalMin + globalMax) / 2.0L;
        return;
    }

    const long double span = (globalMax - globalMin) / static_cast<long double>(verticalZoom_);
    const long double halfSpan = span / 2.0L;
    verticalCenterAddress_ = std::clamp(verticalCenterAddress_,
                                        globalMin + halfSpan,
                                        globalMax - halfSpan);
}

QRect AddressWidget::headerRect() const
{
    return QRect(0, 0, width(), kHeaderHeight);
}

QRect AddressWidget::topRulerRect() const
{
    return QRect(kAddressAxisWidth,
                 kHeaderHeight,
                 qMax(1, width() - kAddressAxisWidth - kRightMargin),
                 kTopRulerHeight);
}

QRect AddressWidget::addressAxisRect() const
{
    const QRect plot = plotRect();
    return QRect(0, plot.top(), kAddressAxisWidth, plot.height());
}

QRect AddressWidget::plotRect() const
{
    const int top = kHeaderHeight + kTopRulerHeight;
    const int bottomReserved = kFullRulerHeight + kScrollBarHeight;
    return QRect(kAddressAxisWidth,
                 top,
                 qMax(1, width() - kAddressAxisWidth - kRightMargin),
                 qMax(1, height() - top - bottomReserved));
}

QRect AddressWidget::fullRulerRect() const
{
    const QRect plot = plotRect();
    return QRect(plot.left(), plot.bottom() + 1, plot.width(), kFullRulerHeight);
}

QRect AddressWidget::horizontalScrollRect() const
{
    const QRect full = fullRulerRect();
    return QRect(full.left(), full.bottom() + 1, full.width(), kScrollBarHeight);
}

int AddressWidget::timelineViewportWidth() const
{
    return qMax(1, plotRect().width());
}

std::uint64_t AddressWidget::timelineContentWidth() const
{
    const int viewport = timelineViewportWidth();
    if (totalRows_ <= 1) {
        return static_cast<std::uint64_t>(viewport);
    }
    const long double width = static_cast<long double>(viewport) * horizontalZoom_;
    const long double clamped = std::min<long double>(
        width,
        static_cast<long double>(std::numeric_limits<std::uint64_t>::max() / 4U));
    return qMax<std::uint64_t>(static_cast<std::uint64_t>(std::llround(clamped)),
                               static_cast<std::uint64_t>(viewport));
}

std::uint64_t AddressWidget::horizontalContentRange() const
{
    const std::uint64_t contentWidth = timelineContentWidth();
    const std::uint64_t viewport = static_cast<std::uint64_t>(timelineViewportWidth());
    return contentWidth > viewport ? contentWidth - viewport : 0;
}

std::uint64_t AddressWidget::contentXForLogicalRow(const std::uint64_t logicalRow) const
{
    const auto timestamp = timestampForLogicalRow(logicalRow);
    return timestamp ? contentXForTimestamp(*timestamp) : 0;
}

std::uint64_t AddressWidget::contentXForTimestamp(const qint64 timestamp) const
{
    const std::uint64_t contentWidth = timelineContentWidth();
    if (!haveTimeRange_ || contentWidth <= 1 || firstTimestamp_ == lastTimestamp_) {
        return 0;
    }

    const qint64 clampedTimestamp = std::clamp(timestamp, firstTimestamp_, lastTimestamp_);
    const long double ratio =
        static_cast<long double>(clampedTimestamp - firstTimestamp_)
        / static_cast<long double>(lastTimestamp_ - firstTimestamp_);
    return std::min<std::uint64_t>(
        contentWidth - 1,
        static_cast<std::uint64_t>(
            std::llround(ratio * static_cast<long double>(contentWidth - 1))));
}

qint64 AddressWidget::timestampForContentX(const std::uint64_t contentX) const
{
    const std::uint64_t contentWidth = timelineContentWidth();
    if (!haveTimeRange_ || contentWidth <= 1 || firstTimestamp_ == lastTimestamp_) {
        return firstTimestamp_;
    }

    const std::uint64_t x = std::min<std::uint64_t>(contentX, contentWidth - 1);
    const long double ratio = static_cast<long double>(x)
        / static_cast<long double>(contentWidth - 1);
    const long double timestamp = static_cast<long double>(firstTimestamp_)
        + ratio * static_cast<long double>(lastTimestamp_ - firstTimestamp_);
    return qBound(firstTimestamp_, static_cast<qint64>(std::llround(timestamp)), lastTimestamp_);
}

std::uint64_t AddressWidget::logicalRowForContentX(const std::uint64_t contentX) const
{
    return logicalRowForTimestamp(timestampForContentX(contentX));
}

std::uint64_t AddressWidget::contentXAtTimelineX(const int x) const
{
    const QRect plot = plotRect();
    const int clampedX = qBound(plot.left(), x, plot.right());
    return scrollOffset_ + static_cast<std::uint64_t>(clampedX - plot.left());
}

std::uint64_t AddressWidget::logicalRowAtTimelineX(const int x) const
{
    return logicalRowForContentX(contentXAtTimelineX(x));
}

std::optional<int> AddressWidget::screenXForTimestamp(const qint64 timestamp,
                                                      const QRect& area) const
{
    if (!haveTimeRange_ || totalRows_ == 0 || area.width() <= 0) {
        return std::nullopt;
    }
    const std::uint64_t contentX = contentXForTimestamp(timestamp);
    if (contentX < scrollOffset_) {
        return std::nullopt;
    }
    const std::uint64_t local = contentX - scrollOffset_;
    if (local > static_cast<std::uint64_t>(area.width() - 1)) {
        return std::nullopt;
    }
    return area.left() + static_cast<int>(local);
}

std::optional<int> AddressWidget::screenXForLogicalRow(const std::uint64_t logicalRow,
                                                       const QRect& area) const
{
    if (totalRows_ == 0 || area.width() <= 0) {
        return std::nullopt;
    }
    const auto timestamp = timestampForLogicalRow(logicalRow);
    return timestamp ? screenXForTimestamp(*timestamp, area) : std::nullopt;
}

std::optional<int> AddressWidget::screenYForAddress(const std::uint64_t address,
                                                    const QRect& area) const
{
    if (!haveAddressRange_ || area.height() <= 0) {
        return std::nullopt;
    }
    const QRect mapping = addressMappingRect(area);
    if (mapping.height() <= 0) {
        return mapping.center().y();
    }

    const auto [visibleMin, visibleMax] = visibleAddressRange();
    if (visibleMax <= visibleMin) {
        return mapping.center().y();
    }

    const long double clampedAddress = static_cast<long double>(
        std::min<std::uint64_t>(std::max<std::uint64_t>(address, minAddress_), maxAddress_));
    const long double ratio = (clampedAddress - visibleMin) / (visibleMax - visibleMin);
    if (ratio < 0.0L || ratio > 1.0L) {
        return std::nullopt;
    }

    const int y = mapping.bottom() - static_cast<int>(
                                      std::llround(ratio * static_cast<long double>(mapping.height())));
    return qBound(mapping.top(), y, mapping.bottom());
}

std::uint64_t AddressWidget::addressForScreenY(const int y, const QRect& area) const
{
    if (!haveAddressRange_) {
        return minAddress_;
    }
    const QRect mapping = addressMappingRect(area);
    if (mapping.height() <= 0) {
        return minAddress_;
    }

    const int clampedY = qBound(mapping.top(), y, mapping.bottom());
    const long double ratio = static_cast<long double>(mapping.bottom() - clampedY)
        / static_cast<long double>(mapping.height());
    return addressFromVisibleRatio(ratio);
}

QRect AddressWidget::addressMappingRect(const QRect& area) const
{
    if (area.height() <= 0) {
        return area;
    }
    const int verticalMargin = qMin(kPlotPaddingY, qMax(0, area.height() / 4));
    return area.adjusted(0, verticalMargin, 0, -verticalMargin);
}

std::pair<long double, long double> AddressWidget::globalAddressViewRange() const
{
    if (!haveAddressRange_) {
        return {0.0L, 0.0L};
    }

    if (minAddress_ == maxAddress_) {
        const long double address = static_cast<long double>(minAddress_);
        const long double margin = std::max<long double>(1.0L, std::abs(address) * kAddressGlobalMarginFraction);
        return {std::max<long double>(0.0L, address - margin), address + margin};
    }

    const long double minAddress = static_cast<long double>(minAddress_);
    const long double maxAddress = static_cast<long double>(maxAddress_);
    const long double margin = std::max<long double>(1.0L, (maxAddress - minAddress) * kAddressGlobalMarginFraction);
    return {std::max<long double>(0.0L, minAddress - margin), maxAddress + margin};
}

std::pair<long double, long double> AddressWidget::visibleAddressRange() const
{
    if (!haveAddressRange_) {
        return {0.0L, 0.0L};
    }
    const auto [globalMin, globalMax] = globalAddressViewRange();
    if (globalMax <= globalMin) {
        return {globalMin, globalMax};
    }

    const double zoom = std::clamp(verticalZoom_, kMinVerticalZoom, kMaxVerticalZoom);
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

std::uint64_t AddressWidget::addressFromVisibleRatio(const long double ratio) const
{
    if (!haveAddressRange_) {
        return 0;
    }
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    const long double clampedRatio = std::clamp(ratio, 0.0L, 1.0L);
    const long double address = visibleMin + clampedRatio * (visibleMax - visibleMin);
    const long double clampedAddress = std::clamp(address,
                                                  static_cast<long double>(minAddress_),
                                                  static_cast<long double>(maxAddress_));
    return static_cast<std::uint64_t>(std::llround(clampedAddress));
}

std::uint64_t AddressWidget::logicalRowForTimestamp(const qint64 timestamp) const
{
    if (totalRows_ == 0) {
        return 0;
    }
    if (!haveTimeRange_ || firstTimestamp_ == lastTimestamp_ || totalRows_ <= 1) {
        return 0;
    }

    const qint64 clampedTimestamp = std::clamp(timestamp, firstTimestamp_, lastTimestamp_);
    const long double ratio =
        static_cast<long double>(clampedTimestamp - firstTimestamp_)
        / static_cast<long double>(lastTimestamp_ - firstTimestamp_);
    return std::min<std::uint64_t>(
        totalRows_ - 1,
        static_cast<std::uint64_t>(
            std::llround(ratio * static_cast<long double>(totalRows_ - 1))));
}

std::pair<qint64, qint64> AddressWidget::visibleTimestampRange() const
{
    if (!haveTimeRange_ || totalRows_ == 0) {
        return {0, 0};
    }
    const QRect plot = plotRect();
    const qint64 first = timestampForContentX(scrollOffset_);
    const qint64 last = timestampForContentX(scrollOffset_
                                             + static_cast<std::uint64_t>(qMax(0, plot.width() - 1)));
    return {std::min(first, last), std::max(first, last)};
}

std::pair<std::uint64_t, std::uint64_t> AddressWidget::visibleLogicalRowRange() const
{
    if (totalRows_ == 0) {
        return {0, 0};
    }
    const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
    const std::uint64_t first = logicalRowForTimestamp(firstTimestamp);
    const std::uint64_t last = logicalRowForTimestamp(lastTimestamp);
    return {first, std::max(first, last)};
}

QString AddressWidget::visibleRangeText() const
{
    if (totalRows_ == 0) {
        return QStringLiteral("No address view");
    }
    const auto [first, last] = visibleLogicalRowRange();
    const auto [firstTs, lastTs] = visibleTimestampRange();
    const std::uint64_t visibleEvents = visibleAddressEventCount();
    const QString eventCountText = visibleEvents == events_.size()
        ? QString::number(events_.size())
        : QStringLiteral("%1/%2").arg(visibleEvents).arg(events_.size());
    QString text = QStringLiteral("Rows %1 - %2 of %3   Zoom %4x   Addresses %5")
                       .arg(first + 1)
                       .arg(last + 1)
                       .arg(totalRows_)
                       .arg(horizontalZoom_, 0, 'f', horizontalZoom_ < 10.0 ? 2 : 1)
                       .arg(eventCountText);
    if (haveTimeRange_) {
        text += QStringLiteral("   Time %1 - %2")
                    .arg(FormatTimestampForDisplay(firstTs),
                         FormatTimestampForDisplay(lastTs));
    }
    if (markerLogicalRow_ >= 0 && selectedLogicalRow_ >= 0) {
        text += QStringLiteral("   drow %1")
                    .arg(std::abs(markerLogicalRow_ - selectedLogicalRow_));
    }
    return text;
}

QString AddressWidget::rowLabel(const std::uint64_t logicalRow) const
{
    if (const auto timestamp = timestampForLogicalRow(logicalRow)) {
        return QStringLiteral("%1 / %2")
            .arg(logicalRow + 1)
            .arg(FormatTimestampForDisplay(*timestamp));
    }
    return QString::number(logicalRow + 1);
}

std::optional<qint64> AddressWidget::timestampForLogicalRow(const std::uint64_t logicalRow) const
{
    if (!haveTimeRange_) {
        return std::nullopt;
    }
    if (events_.empty()) {
        if (totalRows_ <= 1 || firstTimestamp_ == lastTimestamp_) {
            return firstTimestamp_;
        }
        const std::uint64_t row = std::min<std::uint64_t>(logicalRow, totalRows_ - 1);
        const long double timestamp = static_cast<long double>(firstTimestamp_)
            + static_cast<long double>(row) * static_cast<long double>(lastTimestamp_ - firstTimestamp_)
                / static_cast<long double>(totalRows_ - 1);
        return qBound(firstTimestamp_, static_cast<qint64>(std::llround(timestamp)), lastTimestamp_);
    }
    const AddressEvent probe{logicalRow, 0, 0, AddressEventKind::Read, 0};
    const auto found = std::lower_bound(events_.cbegin(),
                                        events_.cend(),
                                        probe,
                                        [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                            return lhs.logicalRow < rhs.logicalRow;
                                        });
    if (found != events_.cend() && found->logicalRow == logicalRow) {
        return found->timestamp;
    }

    if (totalRows_ <= 1 || firstTimestamp_ == lastTimestamp_) {
        return firstTimestamp_;
    }
    const std::uint64_t row = std::min<std::uint64_t>(logicalRow, totalRows_ - 1);
    const long double timestamp = static_cast<long double>(firstTimestamp_)
        + static_cast<long double>(row) * static_cast<long double>(lastTimestamp_ - firstTimestamp_)
            / static_cast<long double>(totalRows_ - 1);
    return qBound(firstTimestamp_, static_cast<qint64>(std::llround(timestamp)), lastTimestamp_);
}

std::optional<std::uint64_t> AddressWidget::addressForLogicalRow(const std::uint64_t logicalRow) const
{
    if (events_.empty()) {
        return std::nullopt;
    }
    const AddressEvent probe{logicalRow, 0, 0, AddressEventKind::Read, 0};
    const auto found = std::lower_bound(events_.cbegin(),
                                        events_.cend(),
                                        probe,
                                        [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                            return lhs.logicalRow < rhs.logicalRow;
                                        });
    if (found != events_.cend() && found->logicalRow == logicalRow && eventVisible(*found)) {
        return found->address;
    }
    return std::nullopt;
}

void AddressWidget::zoomToFit()
{
    pushViewHistory();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    resetVerticalView();
    rebuildContentMetrics();
    updateScrollBar();
    update();
}

void AddressWidget::zoomByFactor(const double factor, const QPoint& focus)
{
    if (totalRows_ <= 1 || factor <= 0.0) {
        return;
    }

    pushViewHistory();
    const QRect plot = plotRect();
    const int focusX = qBound(plot.left(), focus.x(), plot.right());
    const std::uint64_t focusContentBefore =
        scrollOffset_ + static_cast<std::uint64_t>(focusX - plot.left());
    const qint64 focusTimestamp = timestampForContentX(focusContentBefore);

    horizontalZoom_ = std::clamp(horizontalZoom_ * factor, kMinZoom, kMaxZoom);
    rebuildContentMetrics();
    const std::uint64_t focusContentAfter = contentXForTimestamp(focusTimestamp);
    if (focusContentAfter > static_cast<std::uint64_t>(focusX - plot.left())) {
        scrollToHorizontalOffset(focusContentAfter - static_cast<std::uint64_t>(focusX - plot.left()));
    } else {
        scrollToHorizontalOffset(0);
    }
    updateScrollBar();
    update();
}

void AddressWidget::zoomVerticallyByFactor(const double factor, const QPoint& focus)
{
    if (!haveAddressRange_ || factor <= 0.0) {
        return;
    }

    const QRect plot = plotRect();
    const QRect mapping = addressMappingRect(plot);
    if (mapping.height() <= 0) {
        return;
    }

    pushViewHistory();
    const int focusY = qBound(mapping.top(), focus.y(), mapping.bottom());
    const long double focusRatio = static_cast<long double>(mapping.bottom() - focusY)
        / static_cast<long double>(mapping.height());
    const long double focusAddress = static_cast<long double>(addressFromVisibleRatio(focusRatio));

    verticalZoom_ = std::clamp(verticalZoom_ * factor, kMinVerticalZoom, kMaxVerticalZoom);
    const auto [globalMin, globalMax] = globalAddressViewRange();
    const long double span = (globalMax - globalMin) / static_cast<long double>(verticalZoom_);
    verticalCenterAddress_ = focusAddress + span * (0.5L - focusRatio);
    normalizeVerticalView();
    update();
}

void AddressWidget::zoomToLogicalRange(std::uint64_t firstRow, std::uint64_t lastRow, const bool moveCursor)
{
    if (totalRows_ <= 1) {
        return;
    }
    if (firstRow > lastRow) {
        std::swap(firstRow, lastRow);
    }
    firstRow = std::min<std::uint64_t>(firstRow, totalRows_ - 1);
    lastRow = std::min<std::uint64_t>(lastRow, totalRows_ - 1);
    if (firstRow == lastRow) {
        return;
    }

    const auto firstTimestamp = timestampForLogicalRow(firstRow);
    const auto lastTimestamp = timestampForLogicalRow(lastRow);
    if (!firstTimestamp || !lastTimestamp) {
        return;
    }
    zoomToTimestampRange(*firstTimestamp, *lastTimestamp, moveCursor);
}

void AddressWidget::zoomToTimestampRange(qint64 firstTimestamp, qint64 lastTimestamp, const bool moveCursor)
{
    if (!haveTimeRange_ || firstTimestamp_ == lastTimestamp_) {
        return;
    }
    if (firstTimestamp > lastTimestamp) {
        std::swap(firstTimestamp, lastTimestamp);
    }
    firstTimestamp = std::clamp(firstTimestamp, firstTimestamp_, lastTimestamp_);
    lastTimestamp = std::clamp(lastTimestamp, firstTimestamp_, lastTimestamp_);
    if (firstTimestamp == lastTimestamp) {
        return;
    }

    pushViewHistory();
    const double fullSpan = static_cast<double>(lastTimestamp_ - firstTimestamp_);
    const double span = static_cast<double>(lastTimestamp - firstTimestamp);
    if (fullSpan <= 0.0 || span <= 0.0) {
        return;
    }
    const double requestedZoom = fullSpan / span;
    horizontalZoom_ = std::clamp(requestedZoom, kMinZoom, kMaxZoom);
    rebuildContentMetrics();
    scrollToHorizontalOffset(contentXForTimestamp(firstTimestamp));
    if (moveCursor) {
        setCursorLogicalRow(static_cast<int>(std::min<std::uint64_t>(
                                logicalRowForTimestamp(firstTimestamp),
                                static_cast<std::uint64_t>(std::numeric_limits<int>::max()))),
                            false);
    }
}

void AddressWidget::pushViewHistory()
{
    if (!viewHistory_.empty()) {
        const ViewState& last = viewHistory_.back();
        if (std::abs(last.horizontalZoom - horizontalZoom_) < 0.0001
            && last.horizontalOffset == scrollOffset_
            && std::abs(last.verticalZoom - verticalZoom_) < 0.0001
            && std::abs(last.verticalCenterAddress - verticalCenterAddress_) < 0.5L) {
            return;
        }
    }
    viewHistory_.push_back(ViewState{horizontalZoom_, scrollOffset_, verticalZoom_, verticalCenterAddress_});
    if (viewHistory_.size() > 32) {
        viewHistory_.erase(viewHistory_.begin());
    }
}

void AddressWidget::restorePreviousView()
{
    if (viewHistory_.empty()) {
        return;
    }
    const ViewState state = viewHistory_.back();
    viewHistory_.pop_back();
    horizontalZoom_ = std::clamp(state.horizontalZoom, kMinZoom, kMaxZoom);
    verticalZoom_ = std::clamp(state.verticalZoom, kMinVerticalZoom, kMaxVerticalZoom);
    verticalCenterAddress_ = state.verticalCenterAddress;
    normalizeVerticalView();
    rebuildContentMetrics();
    scrollToHorizontalOffset(state.horizontalOffset);
    update();
}

void AddressWidget::scrollToHorizontalOffset(const std::uint64_t offset)
{
    scrollOffset_ = std::min<std::uint64_t>(offset, horizontalContentRange());
    updateScrollBar();
    update();
}

void AddressWidget::panHorizontallyByPixels(const int pixels)
{
    if (pixels == 0) {
        return;
    }
    const std::uint64_t maxOffset = horizontalContentRange();
    if (pixels < 0) {
        const std::uint64_t amount = static_cast<std::uint64_t>(-pixels);
        scrollOffset_ = amount > scrollOffset_ ? 0 : scrollOffset_ - amount;
    } else {
        scrollOffset_ = std::min<std::uint64_t>(maxOffset,
                                                scrollOffset_ + static_cast<std::uint64_t>(pixels));
    }
    updateScrollBar();
    update();
}

void AddressWidget::scrollVerticallyByPixels(const int pixels)
{
    if (pixels == 0 || !haveAddressRange_) {
        return;
    }
    const QRect mapping = addressMappingRect(plotRect());
    if (mapping.height() <= 0) {
        return;
    }

    const auto [visibleMin, visibleMax] = visibleAddressRange();
    const long double addressPerPixel = (visibleMax - visibleMin)
        / static_cast<long double>(mapping.height());
    verticalCenterAddress_ += static_cast<long double>(pixels) * addressPerPixel;
    normalizeVerticalView();
    update();
}

void AddressWidget::ensureLogicalRowVisible(const std::uint64_t logicalRow)
{
    const QRect plot = plotRect();
    if (plot.width() <= 1 || totalRows_ == 0) {
        return;
    }
    const std::uint64_t contentX = contentXForLogicalRow(logicalRow);
    const std::uint64_t margin = static_cast<std::uint64_t>(std::min(80, plot.width() / 5));
    if (contentX < scrollOffset_ + margin) {
        scrollToHorizontalOffset(contentX > margin ? contentX - margin : 0);
    } else if (contentX >= scrollOffset_ + static_cast<std::uint64_t>(plot.width()) - margin) {
        scrollToHorizontalOffset(contentX + margin > static_cast<std::uint64_t>(plot.width())
                                     ? contentX + margin - static_cast<std::uint64_t>(plot.width())
                                     : 0);
    }
}

void AddressWidget::ensureAddressVisible(const std::uint64_t address)
{
    if (!haveAddressRange_ || verticalZoom_ <= kMinVerticalZoom + 0.0001) {
        return;
    }

    const auto [visibleMin, visibleMax] = visibleAddressRange();
    const long double addressValue = static_cast<long double>(address);
    if (addressValue >= visibleMin && addressValue <= visibleMax) {
        return;
    }

    const long double span = visibleMax - visibleMin;
    if (span <= 0.0L) {
        return;
    }
    const long double margin = span * 0.15L;
    if (addressValue < visibleMin) {
        verticalCenterAddress_ -= visibleMin - addressValue + margin;
    } else {
        verticalCenterAddress_ += addressValue - visibleMax + margin;
    }
    normalizeVerticalView();
}

void AddressWidget::centerSelectedLogicalRow()
{
    if (selectedLogicalRow_ < 0) {
        return;
    }
    const std::uint64_t contentX = contentXForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
    const std::uint64_t half = static_cast<std::uint64_t>(timelineViewportWidth() / 2);
    scrollToHorizontalOffset(contentX > half ? contentX - half : 0);
}

void AddressWidget::moveCursorToAdjacentEvent(const int direction)
{
    if (events_.empty()) {
        return;
    }
    const std::uint64_t current = selectedLogicalRow_ < 0
        ? (direction >= 0 ? 0 : totalRows_ - 1)
        : static_cast<std::uint64_t>(selectedLogicalRow_);
    std::optional<int> next;
    if (direction >= 0) {
        for (const AddressEvent& event : events_) {
            if (eventVisible(event) && event.logicalRow > current) {
                next = static_cast<int>(event.logicalRow);
                break;
            }
        }
    } else {
        for (auto it = events_.crbegin(); it != events_.crend(); ++it) {
            if (eventVisible(*it) && it->logicalRow < current) {
                next = static_cast<int>(it->logicalRow);
                break;
            }
        }
    }
    if (next) {
        setCursorLogicalRow(*next, true);
    }
}

void AddressWidget::setCursorLogicalRow(const int logicalRow, const bool emitActivation)
{
    setCursorPosition(logicalRow, addressForLogicalRow(static_cast<std::uint64_t>(qMax(0, logicalRow))), emitActivation);
}

void AddressWidget::setCursorPosition(const int logicalRow,
                                      std::optional<std::uint64_t> address,
                                      const bool emitActivation)
{
    if (totalRows_ == 0) {
        selectedLogicalRow_ = -1;
        selectedAddress_.reset();
        update();
        return;
    }
    const int clamped = qBound(0,
                               logicalRow,
                               static_cast<int>(std::min<std::uint64_t>(
                                   totalRows_ - 1,
                                   static_cast<std::uint64_t>(std::numeric_limits<int>::max()))));
    const int previous = selectedLogicalRow_;
    const int delta = lockCursorMarkerDelta_ && markerLogicalRow_ >= 0 && previous >= 0
        ? markerLogicalRow_ - previous
        : 0;
    selectedLogicalRow_ = clamped;
    if (!address) {
        address = addressForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
    }
    selectedAddress_ = address;
    if (lockCursorMarkerDelta_ && markerLogicalRow_ >= 0) {
        markerLogicalRow_ = qBound(0,
                                   selectedLogicalRow_ + delta,
                                   static_cast<int>(std::min<std::uint64_t>(
                                       totalRows_ - 1,
                                       static_cast<std::uint64_t>(std::numeric_limits<int>::max()))));
    }
    ensureLogicalRowVisible(static_cast<std::uint64_t>(selectedLogicalRow_));
    if (selectedAddress_) {
        ensureAddressVisible(*selectedAddress_);
    }
    update();
    if (emitActivation) {
        Q_EMIT logicalRowActivated(selectedLogicalRow_);
    }
}

void AddressWidget::setMarkerLogicalRow(const int logicalRow)
{
    if (logicalRow < 0 || totalRows_ == 0) {
        markerLogicalRow_ = -1;
    } else {
        markerLogicalRow_ = qBound(0,
                                   logicalRow,
                                   static_cast<int>(std::min<std::uint64_t>(
                                       totalRows_ - 1,
                                       static_cast<std::uint64_t>(std::numeric_limits<int>::max()))));
    }
    update();
}

void AddressWidget::beginPendingGesture(const QPoint& position)
{
    dragMode_ = DragMode::PendingGesture;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    update();
}

void AddressWidget::updatePendingGesture(const QPoint& position,
                                         const Qt::KeyboardModifiers modifiers)
{
    updateLeftDragGesture(position, modifiers);
}

void AddressWidget::updateLeftDragGesture(const QPoint& position,
                                          const Qt::KeyboardModifiers modifiers)
{
    const DragMode previousMode = dragMode_;
    dragLast_ = position;
    dragStart_ = leftDragAnchor_;
    const DragMode nextMode = classifyLeftDragGesture(position, modifiers);

    if (nextMode != previousMode) {
        if (previousMode == DragMode::Pan || previousMode == DragMode::OverviewPan) {
            unsetCursor();
        }
        if (nextMode == DragMode::Pan) {
            dragStartScrollOffset_ = scrollOffset_;
            setCursor(Qt::ClosedHandCursor);
        } else if (nextMode == DragMode::OverviewPan) {
            setCursor(Qt::SizeHorCursor);
        }
    }

    dragMode_ = nextMode;
    if (dragMode_ == DragMode::Pan) {
        updatePan(position);
    } else if (dragMode_ == DragMode::OverviewPan) {
        updateOverviewPan(position);
    } else {
        update();
    }
}

AddressWidget::DragMode AddressWidget::classifyLeftDragGesture(
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
        && (positionInTopRuler(leftDragAnchor_) || positionInTimeline(leftDragAnchor_))) {
        return DragMode::RangeZoom;
    }

    if (!gestureDirectionReady(delta)) {
        return DragMode::PendingGesture;
    }

    switch (snappedGestureOctant(delta)) {
    case -1:
        return DragMode::ZoomIn2x;
    case 1:
        return DragMode::ZoomOut2x;
    case 2:
    case -2:
        return DragMode::ZoomFull;
    default:
        break;
    }

    if (positionInFullRuler(leftDragAnchor_)) {
        return overviewWindowRect().contains(leftDragAnchor_) && std::abs(delta.x()) >= std::abs(delta.y())
            ? DragMode::OverviewPan
            : DragMode::FullRangeZoom;
    }

    if (positionInTopRuler(leftDragAnchor_) || positionInTimeline(leftDragAnchor_)) {
        return std::abs(delta.x()) >= std::abs(delta.y())
            ? DragMode::RangeZoom
            : DragMode::ZoomFull;
    }

    return DragMode::PendingGesture;
}

void AddressWidget::beginRangeZoom(const QPoint& position)
{
    dragMode_ = DragMode::RangeZoom;
    leftDragGestureActive_ = false;
    dragStart_ = position;
    dragLast_ = position;
    update();
}

void AddressWidget::updateRangeZoom(const QPoint& position)
{
    dragLast_ = position;
    update();
}

bool AddressWidget::finishRangeZoom()
{
    const QRect plot = plotRect();
    const int left = qBound(plot.left(), std::min(dragStart_.x(), dragLast_.x()), plot.right());
    const int right = qBound(plot.left(), std::max(dragStart_.x(), dragLast_.x()), plot.right());
    if (right - left < kGestureActivationDistance) {
        return false;
    }
    const qint64 first = timestampForContentX(contentXAtTimelineX(left));
    const qint64 last = timestampForContentX(contentXAtTimelineX(right));
    zoomToTimestampRange(first, last, false);
    return true;
}

void AddressWidget::beginFullRangeZoom(const QPoint& position)
{
    dragMode_ = DragMode::FullRangeZoom;
    leftDragGestureActive_ = false;
    dragStart_ = position;
    dragLast_ = position;
    update();
}

void AddressWidget::updateFullRangeZoom(const QPoint& position)
{
    dragLast_ = position;
    update();
}

bool AddressWidget::finishFullRangeZoom()
{
    const QRect full = fullRulerRect();
    const int left = qBound(full.left(), std::min(dragStart_.x(), dragLast_.x()), full.right());
    const int right = qBound(full.left(), std::max(dragStart_.x(), dragLast_.x()), full.right());
    if (right - left < kGestureActivationDistance || !haveTimeRange_ || firstTimestamp_ == lastTimestamp_) {
        return false;
    }
    const long double startRatio = static_cast<long double>(left - full.left())
        / static_cast<long double>(qMax(1, full.width() - 1));
    const long double endRatio = static_cast<long double>(right - full.left())
        / static_cast<long double>(qMax(1, full.width() - 1));
    const qint64 first = firstTimestamp_ + static_cast<qint64>(
        std::llround(startRatio * static_cast<long double>(lastTimestamp_ - firstTimestamp_)));
    const qint64 last = firstTimestamp_ + static_cast<qint64>(
        std::llround(endRatio * static_cast<long double>(lastTimestamp_ - firstTimestamp_)));
    zoomToTimestampRange(first, last, false);
    return true;
}

void AddressWidget::beginZoomFullGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomFull;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
    update();
}

bool AddressWidget::finishZoomFullGesture()
{
    zoomToFit();
    return true;
}

void AddressWidget::beginZoomIn2xGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomIn2x;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
    update();
}

bool AddressWidget::finishZoomIn2xGesture()
{
    zoomByFactor(2.0, dragStart_);
    return true;
}

void AddressWidget::beginZoomOut2xGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomOut2x;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
    update();
}

bool AddressWidget::finishZoomOut2xGesture()
{
    zoomByFactor(0.5, dragStart_);
    return true;
}

void AddressWidget::beginPan(const QPoint& position)
{
    dragMode_ = DragMode::Pan;
    leftDragGestureActive_ = false;
    dragStart_ = position;
    dragLast_ = position;
    dragStartScrollOffset_ = scrollOffset_;
    setCursor(Qt::ClosedHandCursor);
}

void AddressWidget::updatePan(const QPoint& position)
{
    const int dx = position.x() - dragStart_.x();
    if (dx < 0) {
        scrollOffset_ = std::min<std::uint64_t>(horizontalContentRange(),
                                                dragStartScrollOffset_ + static_cast<std::uint64_t>(-dx));
    } else {
        const std::uint64_t amount = static_cast<std::uint64_t>(dx);
        scrollOffset_ = amount > dragStartScrollOffset_ ? 0 : dragStartScrollOffset_ - amount;
    }
    updateScrollBar();
    update();
}

void AddressWidget::beginOverviewPan(const QPoint& position)
{
    dragMode_ = DragMode::OverviewPan;
    leftDragGestureActive_ = false;
    dragStart_ = position;
    dragLast_ = position;
    setCursor(Qt::SizeHorCursor);
}

void AddressWidget::updateOverviewPan(const QPoint& position)
{
    if (totalRows_ <= 1) {
        return;
    }
    const QRect full = fullRulerRect();
    const QRect window = overviewWindowRect();
    const int dx = position.x() - dragLast_.x();
    dragLast_ = position;
    const int newCenter = qBound(full.left() + window.width() / 2,
                                 window.center().x() + dx,
                                 full.right() - window.width() / 2);
    const long double ratio = static_cast<long double>(newCenter - full.left())
        / static_cast<long double>(qMax(1, full.width() - 1));
    const std::uint64_t centerContent =
        static_cast<std::uint64_t>(std::llround(ratio * static_cast<long double>(timelineContentWidth() - 1)));
    const std::uint64_t half = static_cast<std::uint64_t>(timelineViewportWidth() / 2);
    scrollToHorizontalOffset(centerContent > half ? centerContent - half : 0);
}

bool AddressWidget::positionInTimeline(const QPoint& position) const
{
    return plotRect().contains(position);
}

bool AddressWidget::positionInTopRuler(const QPoint& position) const
{
    return topRulerRect().contains(position);
}

bool AddressWidget::positionInFullRuler(const QPoint& position) const
{
    return fullRulerRect().contains(position);
}

bool AddressWidget::positionInRuler(const QPoint& position) const
{
    return positionInTopRuler(position) || positionInFullRuler(position);
}

QRect AddressWidget::overviewWindowRect() const
{
    const QRect full = fullRulerRect();
    const std::uint64_t contentWidth = timelineContentWidth();
    if (contentWidth <= static_cast<std::uint64_t>(timelineViewportWidth())) {
        return full.adjusted(0, 4, -1, -5);
    }
    const long double leftRatio = static_cast<long double>(scrollOffset_)
        / static_cast<long double>(contentWidth);
    const long double rightRatio = static_cast<long double>(scrollOffset_ + timelineViewportWidth())
        / static_cast<long double>(contentWidth);
    const int left = full.left() + static_cast<int>(std::floor(leftRatio * full.width()));
    const int right = full.left() + static_cast<int>(std::ceil(rightRatio * full.width()));
    return QRect(left, full.top() + 4, qMax(4, right - left), qMax(1, full.height() - 8));
}

std::optional<int> AddressWidget::logicalRowAtPosition(const QPoint& position) const
{
    if (totalRows_ == 0 || (!positionInTimeline(position) && !positionInRuler(position))) {
        return std::nullopt;
    }
    if (positionInTimeline(position) && snapToEvents_) {
        if (const auto eventIndex = nearestEventIndexAtPosition(position)) {
            return static_cast<int>(events_[*eventIndex].logicalRow);
        }
    }
    const std::uint64_t row = logicalRowAtTimelineX(position.x());
    return static_cast<int>(std::min<std::uint64_t>(
        row,
        static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
}

std::optional<AddressWidget::CursorPosition> AddressWidget::cursorPositionAtPosition(const QPoint& position) const
{
    if (totalRows_ == 0 || (!positionInTimeline(position) && !positionInRuler(position))) {
        return std::nullopt;
    }

    if (positionInTimeline(position) && snapToEvents_) {
        if (const auto eventIndex = nearestEventIndexAtPosition(position)) {
            const AddressEvent& event = events_[*eventIndex];
            return CursorPosition{static_cast<int>(event.logicalRow), event.address};
        }
    }

    const std::optional<int> row = logicalRowAtPosition(position);
    if (!row) {
        return std::nullopt;
    }
    std::optional<std::uint64_t> address;
    if (positionInTimeline(position) && haveAddressRange_) {
        address = addressForScreenY(position.y(), plotRect());
    } else {
        address = addressForLogicalRow(static_cast<std::uint64_t>(*row));
    }
    return CursorPosition{*row, address};
}

std::optional<std::size_t> AddressWidget::nearestEventIndexAtPosition(const QPoint& position) const
{
    if (events_.empty() || !plotRect().contains(position)) {
        return std::nullopt;
    }

    const QRect plot = plotRect();
    const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
    const AddressEvent probe{0, firstTimestamp, 0, AddressEventKind::Read, 0};
    auto it = std::lower_bound(events_.cbegin(),
                               events_.cend(),
                               probe,
                               [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                   return lhs.timestamp < rhs.timestamp;
                               });

    std::optional<std::size_t> bestIndex;
    int bestScore = std::numeric_limits<int>::max();
    for (; it != events_.cend() && it->timestamp <= lastTimestamp; ++it) {
        if (!eventVisible(*it)) {
            continue;
        }
        const auto x = screenXForTimestamp(it->timestamp, plot);
        const auto y = screenYForAddress(it->address, plot);
        if (!x || !y) {
            continue;
        }
        const int dx = std::abs(*x - position.x());
        const int dy = std::abs(*y - position.y());
        if (dx > kEventHitRadius || dy > kEventHitRadius) {
            continue;
        }
        const int score = dx * dx + dy * dy;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = static_cast<std::size_t>(std::distance(events_.cbegin(), it));
        }
    }
    return bestIndex;
}

std::optional<std::size_t> AddressWidget::eventTagAtPosition(const QPoint& position) const
{
    if (!plotRect().contains(position) || events_.empty()) {
        return std::nullopt;
    }

    QFont tagFont = font();
    const qreal pointSize = tagFont.pointSizeF();
    if (pointSize > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(6.5, pointSize * 0.78));
    }
    const QFontMetrics metrics(tagFont);
    const QRect plot = plotRect();
    const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
    const int digits = addressHexDigitsFromMax(maxAddress_);

    std::vector<QRect> occupied;
    const AddressEvent probe{0, firstTimestamp, 0, AddressEventKind::Read, 0};
    auto it = std::lower_bound(events_.cbegin(),
                               events_.cend(),
                               probe,
                               [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                   return lhs.timestamp < rhs.timestamp;
    });
    int candidates = 0;
    for (; it != events_.cend() && it->timestamp <= lastTimestamp; ++it) {
        if (!eventVisible(*it)) {
            continue;
        }
        if (++candidates > kEventTagMaxCandidatesPerPaint) {
            break;
        }
        const auto x = screenXForTimestamp(it->timestamp, plot);
        const auto y = screenYForAddress(it->address, plot);
        if (!x || !y) {
            continue;
        }
        QString label = addressText(it->address, digits);
        if (it->opcodeLabelIndex > 0 && it->opcodeLabelIndex < opcodeLabels_.size()) {
            label += QStringLiteral(" ");
            label += opcodeLabels_[it->opcodeLabelIndex];
        }
        const int textWidth = metrics.horizontalAdvance(label);
        if (textWidth <= 0 || textWidth > kEventTagMaxTextWidth) {
            continue;
        }
        const QRect tagRect(*x + 7,
                            *y - (metrics.height() + 4) / 2,
                            textWidth + kEventTagPaddingX * 2,
                            metrics.height() + 4);
        if (tagRect.right() > plot.right() - 2 || tagRect.top() < plot.top() || tagRect.bottom() > plot.bottom()) {
            continue;
        }
        const QRect padded = tagRect.adjusted(-kEventTagGap, -2, kEventTagGap, 2);
        const bool overlaps = std::any_of(occupied.cbegin(), occupied.cend(), [&padded](const QRect& rect) {
            return padded.intersects(rect);
        });
        if (overlaps) {
            continue;
        }
        if (tagRect.contains(position)) {
            return static_cast<std::size_t>(std::distance(events_.cbegin(), it));
        }
        occupied.push_back(tagRect);
    }
    return std::nullopt;
}

void AddressWidget::updateHoveredEventTag(const QPoint& position)
{
    const std::optional<std::size_t> hovered = eventTagAtPosition(position);
    if (hoveredEventTagIndex_ == hovered) {
        return;
    }
    hoveredEventTagIndex_ = hovered;
    update(plotRect());
}

QRect AddressWidget::cursorAddressTagRect() const
{
    if (!selectedAddress_) {
        return QRect();
    }

    const QRect plot = plotRect();
    const auto y = screenYForAddress(*selectedAddress_, plot);
    if (!y) {
        return QRect();
    }

    const QRect axis = addressAxisRect();
    const int digits = addressHexDigitsFromMax(maxAddress_);
    const QString label = addressText(*selectedAddress_, digits);
    QFont tagFont = font();
    tagFont.setPointSizeF(qMax<qreal>(6.5, tagFont.pointSizeF() * 0.86));
    const QFontMetrics metrics(tagFont);
    QRect tagRect(axis.left() + 6,
                  *y - metrics.height() / 2 - 3,
                  qMin(axis.width() - 14, metrics.horizontalAdvance(label) + 12),
                  metrics.height() + 6);
    tagRect.moveRight(axis.right() - 6);
    if (tagRect.top() < plot.top() + 2) {
        tagRect.moveTop(plot.top() + 2);
    }
    if (tagRect.bottom() > plot.bottom() - 2) {
        tagRect.moveBottom(plot.bottom() - 2);
    }
    return tagRect;
}

void AddressWidget::paintHeader(QPainter& painter) const
{
    const QRect header = headerRect();
    painter.fillRect(header, QColor(QStringLiteral("#F7F9FC")));
    painter.setPen(QColor(QStringLiteral("#D8DEE8")));
    painter.drawLine(0, header.bottom(), width(), header.bottom());
    painter.drawLine(kAddressAxisWidth, 0, kAddressAxisWidth, height());

    const QRect axisHeader(0, 0, kAddressAxisWidth, header.height());
    const QRect plotHeader(kAddressAxisWidth, 0, width() - kAddressAxisWidth, header.height());
    painter.setPen(QColor(QStringLiteral("#5C6675")));
    painter.drawText(axisHeader.adjusted(8, 0, -8, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Address"));
    const QString title = QStringLiteral("Address");
    painter.drawText(plotHeader.adjusted(8, 0, -8, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     title);

    QFont legendFont = painter.font();
    legendFont.setPointSizeF(qMax<qreal>(6.0, legendFont.pointSizeF() * 0.9));
    painter.setFont(legendFont);
    const QFontMetrics metrics(legendFont);
    const std::vector<LegendItem> items = legendItems();
    int legendLeft = plotHeader.right() - 8;
    for (const LegendItem& item : items) {
        legendLeft = std::min(legendLeft, item.rect.left());
        const bool visible = isKindVisible(item.kind);
        const QColor accent = kindColor(item.kind);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(visible ? accent.darker(115) : QColor(QStringLiteral("#B8C0CC")), 1));
        painter.setBrush(visible ? accent : QColor(QStringLiteral("#FFFFFF")));
        painter.drawRoundedRect(item.markerRect, 2, 2);
        if (visible) {
            painter.setPen(QPen(QColor(QStringLiteral("#FFFFFF")), 1));
            painter.drawLine(item.markerRect.left() + 2,
                             item.markerRect.center().y(),
                             item.markerRect.center().x() - 1,
                             item.markerRect.bottom() - 2);
            painter.drawLine(item.markerRect.center().x() - 1,
                             item.markerRect.bottom() - 2,
                             item.markerRect.right() - 2,
                             item.markerRect.top() + 2);
        }
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(visible ? QColor(QStringLiteral("#4B5563")) : QColor(QStringLiteral("#9AA4B2")));
        painter.drawText(item.rect.adjusted(17, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, item.text);
    }

    const QRect infoRect(plotHeader.left() + metrics.horizontalAdvance(title) + 20,
                         plotHeader.top(),
                         qMax(0, legendLeft - plotHeader.left() - metrics.horizontalAdvance(title) - 30),
                         plotHeader.height());
    if (infoRect.width() > 24) {
        painter.setPen(QColor(QStringLiteral("#6B7280")));
        painter.drawText(infoRect,
                         Qt::AlignVCenter | Qt::AlignRight,
                         metrics.elidedText(visibleRangeText(), Qt::ElideMiddle, infoRect.width()));
    }
}

void AddressWidget::paintTopRuler(QPainter& painter) const
{
    const QRect ruler = topRulerRect();
    painter.fillRect(QRect(0, ruler.top(), width(), ruler.height()), QColor(QStringLiteral("#FBFCFE")));
    painter.fillRect(ruler, QColor(QStringLiteral("#FFFFFF")));
    painter.setPen(QColor(QStringLiteral("#DDE3EC")));
    painter.drawLine(0, ruler.bottom(), width(), ruler.bottom());

    if (totalRows_ == 0) {
        return;
    }

    const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
    const double roughMajor = std::max<double>(
        1.0,
        static_cast<double>(lastTimestamp - firstTimestamp + 1) / 8.0);
    const qint64 major = std::max<qint64>(1, static_cast<qint64>(niceStep(roughMajor)));
    const qint64 medium = std::max<qint64>(1, major / 5);
    const qint64 minor = std::max<qint64>(1, medium / 2);

    QFont baseFont = painter.font();
    QFont timeFont = baseFont;
    timeFont.setPointSizeF(qMax<qreal>(6.5, timeFont.pointSizeF() * 0.9));
    QFont minorFont = baseFont;
    minorFont.setPointSizeF(qMax<qreal>(6.0, minorFont.pointSizeF() * 0.74));

    struct RulerTick {
        qint64 timestamp = 0;
        std::uint64_t row = 0;
        int x = 0;
        bool major = false;
        bool medium = false;
    };
    std::vector<RulerTick> ticks;

    const qint64 begin = (firstTimestamp / minor) * minor;
    for (qint64 timestamp = begin; timestamp <= lastTimestamp; timestamp += minor) {
        const auto x = screenXForTimestamp(timestamp, ruler);
        if (!x) {
            if (timestamp > lastTimestamp - minor) {
                break;
            }
            continue;
        }

        const bool isMajor = timestamp % major == 0;
        const bool isMedium = timestamp % medium == 0;
        const int tickHeight = isMajor ? 15 : (isMedium ? 10 : 6);
        painter.setPen(isMajor ? QColor(QStringLiteral("#556273"))
                               : (isMedium ? QColor(QStringLiteral("#9AA5B5"))
                                           : QColor(QStringLiteral("#C5CCD8"))));
        painter.drawLine(*x, ruler.bottom(), *x, ruler.bottom() - tickHeight);
        ticks.push_back(RulerTick{timestamp, logicalRowForTimestamp(timestamp), *x, isMajor, isMedium});

        if (timestamp > std::numeric_limits<qint64>::max() - minor || minor == 0) {
            break;
        }
    }

    const QRect rowBand(ruler.left() + 2, ruler.top() + 2, qMax(1, ruler.width() - 4), 16);
    const QRect timeBand(ruler.left() + 2, ruler.top() + 19, qMax(1, ruler.width() - 4), 15);
    std::vector<QRect> occupiedLabels;

    const auto placeLabel = [&](const RulerTick& tick,
                                const QString& text,
                                const QRect& band,
                                const QFont& font,
                                const QColor& color) {
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const int width = metrics.horizontalAdvance(text) + 8;
        if (width > band.width()) {
            return;
        }
        const int left = qBound(band.left(), tick.x - width / 2, band.right() - width + 1);
        const QRect rect(left, band.top(), width, band.height());
        const QRect padded = rect.adjusted(-4, 0, 4, 0);
        const bool overlaps = std::any_of(occupiedLabels.cbegin(), occupiedLabels.cend(), [&padded](const QRect& other) {
            return padded.intersects(other);
        });
        if (overlaps) {
            return;
        }
        painter.setPen(color);
        painter.drawText(rect, Qt::AlignCenter, text);
        occupiedLabels.push_back(rect);
    };

    for (const RulerTick& tick : ticks) {
        if (!tick.major) {
            continue;
        }
        placeLabel(tick,
                   QStringLiteral("T%1").arg(FormatTimestampForDisplay(tick.timestamp)),
                   timeBand,
                   timeFont,
                   QColor(QStringLiteral("#334155")));
        placeLabel(tick,
                   QStringLiteral("R%1").arg(tick.row + 1),
                   rowBand,
                   baseFont,
                   QColor(QStringLiteral("#475569")));
    }

    if (horizontalZoom_ >= 16.0) {
        for (const RulerTick& tick : ticks) {
            if (!tick.medium || tick.major) {
                continue;
            }
            placeLabel(tick,
                       QString::number(tick.row + 1),
                       rowBand,
                       minorFont,
                       QColor(QStringLiteral("#94A3B8")));
        }
    }
    painter.setFont(baseFont);
}

void AddressWidget::paintAddressAxis(QPainter& painter) const
{
    const QRect axis = addressAxisRect();
    const QRect plot = plotRect();
    painter.fillRect(axis, QColor(QStringLiteral("#FFFFFF")));
    painter.setPen(QColor(QStringLiteral("#D8DEE8")));
    painter.drawLine(axis.right(), axis.top(), axis.right(), axis.bottom());

    if (!haveAddressRange_) {
        painter.setPen(QColor(QStringLiteral("#7A8494")));
        painter.drawText(axis.adjusted(8, 0, -8, 0), Qt::AlignCenter, QStringLiteral("No addresses"));
        return;
    }

    QFont axisFont = painter.font();
    axisFont.setPointSizeF(qMax<qreal>(6.5, axisFont.pointSizeF() * 0.86));
    painter.setFont(axisFont);
    const int digits = addressHexDigitsFromMax(maxAddress_);
    const int tickCount = qBound(2, plot.height() / 46, 8);
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    for (int index = 0; index <= tickCount; ++index) {
        const long double ratio = static_cast<long double>(index) / static_cast<long double>(tickCount);
        const long double tickAddress = visibleMin + ratio * (visibleMax - visibleMin);
        const std::uint64_t address = static_cast<std::uint64_t>(
            std::llround(std::clamp(tickAddress,
                                    static_cast<long double>(minAddress_),
                                    static_cast<long double>(maxAddress_))));
        const auto y = screenYForAddress(address, plot);
        if (!y) {
            continue;
        }
        painter.setPen(QColor(QStringLiteral("#CAD2DD")));
        painter.drawLine(axis.right() - 6, *y, axis.right(), *y);
        painter.setPen(QColor(QStringLiteral("#4B5563")));
        painter.drawText(axis.adjusted(8, 0, -12, 0).translated(0, *y - axis.center().y()),
                         Qt::AlignRight | Qt::AlignVCenter,
                         addressText(address, digits));
    }
}

void AddressWidget::paintEvents(QPainter& painter) const
{
    const QRect plot = plotRect();
    painter.save();
    painter.setClipRect(plot);
    painter.fillRect(plot, QColor(QStringLiteral("#FFFFFF")));

    if (haveAddressRange_) {
        const int gridCount = qBound(2, plot.height() / 46, 8);
        const auto [visibleMin, visibleMax] = visibleAddressRange();
        for (int index = 0; index <= gridCount; ++index) {
            const long double ratio = static_cast<long double>(index) / static_cast<long double>(gridCount);
            const long double gridAddress = visibleMin + ratio * (visibleMax - visibleMin);
            const std::uint64_t address = static_cast<std::uint64_t>(
                std::llround(std::clamp(gridAddress,
                                        static_cast<long double>(minAddress_),
                                        static_cast<long double>(maxAddress_))));
            const auto y = screenYForAddress(address, plot);
            if (!y) {
                continue;
            }
            painter.setPen(QColor(QStringLiteral("#EEF2F7")));
            painter.drawLine(plot.left(), *y, plot.right(), *y);
        }
    }

    if (haveTimeRange_) {
        const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
        const double roughMajor = std::max<double>(
            1.0,
            static_cast<double>(lastTimestamp - firstTimestamp + 1) / 8.0);
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
            const bool isMajor = timestamp % major == 0;
            painter.setPen(isMajor ? QColor(QStringLiteral("#DCE3ED"))
                                   : QColor(QStringLiteral("#F2F5F9")));
            painter.drawLine(*x, plot.top(), *x, plot.bottom());
            if (timestamp > std::numeric_limits<qint64>::max() - minor || minor == 0) {
                break;
            }
        }
    }

    if (events_.empty()) {
        painter.restore();
        return;
    }

    paintCursorAndMarker(painter);

    const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();
    if (useDenseEventPaint(firstTimestamp, lastTimestamp, plot)) {
        paintDenseEvents(painter, plot, firstTimestamp, lastTimestamp);
        painter.restore();
        return;
    }

    const AddressEvent probe{0, firstTimestamp, 0, AddressEventKind::Read, 0};
    auto it = std::lower_bound(events_.cbegin(),
                               events_.cend(),
                               probe,
                               [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                   return lhs.timestamp < rhs.timestamp;
    });
    for (; it != events_.cend() && it->timestamp <= lastTimestamp; ++it) {
        if (!eventVisible(*it)) {
            continue;
        }
        const auto x = screenXForTimestamp(it->timestamp, plot);
        const auto y = screenYForAddress(it->address, plot);
        if (!x || !y) {
            continue;
        }

        const bool selected = selectedLogicalRow_ >= 0
            && it->logicalRow == static_cast<std::uint64_t>(selectedLogicalRow_)
            && (!selectedAddress_ || *selectedAddress_ == it->address);
        QColor color = kindColor(it->kind);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(selected ? color.darker(135) : color.darker(115), selected ? 2 : 1));
        painter.setBrush(withAlpha(color, selected ? 235 : 205));

        const QPoint center(*x, *y);
        switch (it->kind) {
        case AddressEventKind::Read:
            painter.drawEllipse(center, selected ? kEventDrawRadius + 2 : kEventDrawRadius,
                                selected ? kEventDrawRadius + 2 : kEventDrawRadius);
            break;
        case AddressEventKind::Writeback: {
            const int radius = selected ? kEventDrawRadius + 2 : kEventDrawRadius;
            painter.drawRect(QRect(center.x() - radius, center.y() - radius, radius * 2, radius * 2));
            break;
        }
        case AddressEventKind::Snoop: {
            const int radius = selected ? kEventDrawRadius + 3 : kEventDrawRadius + 1;
            const QPolygon triangle({
                QPoint(center.x(), center.y() - radius),
                QPoint(center.x() - radius, center.y() + radius),
                QPoint(center.x() + radius, center.y() + radius),
            });
            painter.drawPolygon(triangle);
            break;
        }
        }
    }

    paintEventTags(painter);
    painter.restore();
}

bool AddressWidget::useDenseEventPaint(const qint64 firstTimestamp,
                                       const qint64 lastTimestamp,
                                       const QRect& plot) const
{
    if (events_.empty() || !haveAddressRange_ || !haveTimeRange_ || plot.width() <= 0 || plot.height() <= 0) {
        return false;
    }

    const AddressEvent probe{0, firstTimestamp, 0, AddressEventKind::Read, 0};
    const auto first = std::lower_bound(events_.cbegin(),
                                        events_.cend(),
                                        probe,
                                        [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                            return lhs.timestamp < rhs.timestamp;
                                        });
    const auto last = std::upper_bound(events_.cbegin(),
                                       events_.cend(),
                                       lastTimestamp,
                                       [](const qint64 timestamp, const AddressEvent& event) {
                                           return timestamp < event.timestamp;
                                       });
    return std::distance(first, last) >= static_cast<std::ptrdiff_t>(kDenseEventPaintThreshold);
}

void AddressWidget::paintDenseEvents(QPainter& painter,
                                     const QRect& plot,
                                     const qint64 firstTimestamp,
                                     const qint64 lastTimestamp) const
{
    const QSize layerSize(qMax(1, (plot.width() + (1 << kDenseEventCellShift) - 1) >> kDenseEventCellShift),
                          qMax(1, (plot.height() + (1 << kDenseEventCellShift) - 1) >> kDenseEventCellShift));
    const bool cacheValid = denseEventLayerCache_.valid
        && denseEventLayerCache_.plotSize == plot.size()
        && denseEventLayerCache_.eventsData == events_.data()
        && denseEventLayerCache_.eventCount == events_.size()
        && denseEventLayerCache_.firstTimestamp == firstTimestamp
        && denseEventLayerCache_.lastTimestamp == lastTimestamp
        && denseEventLayerCache_.globalFirstTimestamp == firstTimestamp_
        && denseEventLayerCache_.globalLastTimestamp == lastTimestamp_
        && denseEventLayerCache_.scrollOffset == scrollOffset_
        && denseEventLayerCache_.contentWidth == contentWidth_
        && denseEventLayerCache_.totalRows == totalRows_
        && denseEventLayerCache_.minAddress == minAddress_
        && denseEventLayerCache_.maxAddress == maxAddress_
        && std::abs(denseEventLayerCache_.horizontalZoom - horizontalZoom_) < 0.0001
        && std::abs(denseEventLayerCache_.verticalZoom - verticalZoom_) < 0.0001
        && std::abs(denseEventLayerCache_.verticalCenterAddress - verticalCenterAddress_) < 0.5L
        && denseEventLayerCache_.kindVisible == kindVisible_
        && denseEventLayerCache_.image.size() == layerSize;

    if (!cacheValid) {
        QImage layer(layerSize, QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);

        const auto [visibleMin, visibleMax] = visibleAddressRange();
        const long double visibleSpan = visibleMax - visibleMin;
        if (visibleSpan > 0.0L && haveTimeRange_ && firstTimestamp_ != lastTimestamp_ && contentWidth_ > 1) {
            const AddressEvent probe{0, firstTimestamp, 0, AddressEventKind::Read, 0};
            auto it = std::lower_bound(events_.cbegin(),
                                       events_.cend(),
                                       probe,
                                       [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                           return lhs.timestamp < rhs.timestamp;
                                       });

            QRgb* pixels = reinterpret_cast<QRgb*>(layer.bits());
            const int stride = layer.bytesPerLine() / static_cast<int>(sizeof(QRgb));
            const long double timeScale = static_cast<long double>(contentWidth_ - 1)
                / static_cast<long double>(lastTimestamp_ - firstTimestamp_);
            const int xMax = layer.width() - 1;
            const int yMax = layer.height() - 1;
            const int cellSize = 1 << kDenseEventCellShift;
            const QRgb colors[3] = {
                QColor(QStringLiteral("#2563EB")).rgba(),
                QColor(QStringLiteral("#D97706")).rgba(),
                QColor(QStringLiteral("#7C3AED")).rgba(),
            };
            for (; it != events_.cend() && it->timestamp <= lastTimestamp; ++it) {
                const std::size_t kind = kindIndex(it->kind);
                if (kind >= kindVisible_.size() || !kindVisible_[kind]) {
                    continue;
                }
                const long double address = static_cast<long double>(it->address);
                if (address < visibleMin || address > visibleMax) {
                    continue;
                }
                const qint64 clampedTimestamp = std::clamp(it->timestamp, firstTimestamp_, lastTimestamp_);
                const std::uint64_t contentX = static_cast<std::uint64_t>(
                    std::llround(static_cast<long double>(clampedTimestamp - firstTimestamp_) * timeScale));
                if (contentX < scrollOffset_) {
                    continue;
                }
                const std::uint64_t localX = contentX - scrollOffset_;
                if (localX >= static_cast<std::uint64_t>(plot.width())) {
                    continue;
                }
                const long double ratio = (address - visibleMin) / visibleSpan;
                const int x = qBound(0, static_cast<int>(localX / static_cast<std::uint64_t>(cellSize)), xMax);
                const int y = qBound(0,
                                     (plot.height() - 1
                                      - static_cast<int>(std::llround(ratio * static_cast<long double>(plot.height() - 1))))
                                         / cellSize,
                                     yMax);
                pixels[y * stride + x] = colors[kind];
            }
        }

        denseEventLayerCache_.image = std::move(layer);
        denseEventLayerCache_.plotSize = plot.size();
        denseEventLayerCache_.eventsData = events_.data();
        denseEventLayerCache_.eventCount = events_.size();
        denseEventLayerCache_.firstTimestamp = firstTimestamp;
        denseEventLayerCache_.lastTimestamp = lastTimestamp;
        denseEventLayerCache_.globalFirstTimestamp = firstTimestamp_;
        denseEventLayerCache_.globalLastTimestamp = lastTimestamp_;
        denseEventLayerCache_.scrollOffset = scrollOffset_;
        denseEventLayerCache_.contentWidth = contentWidth_;
        denseEventLayerCache_.totalRows = totalRows_;
        denseEventLayerCache_.minAddress = minAddress_;
        denseEventLayerCache_.maxAddress = maxAddress_;
        denseEventLayerCache_.horizontalZoom = horizontalZoom_;
        denseEventLayerCache_.verticalZoom = verticalZoom_;
        denseEventLayerCache_.verticalCenterAddress = verticalCenterAddress_;
        denseEventLayerCache_.kindVisible = kindVisible_;
        denseEventLayerCache_.valid = true;
    }

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(plot, denseEventLayerCache_.image);
    painter.restore();
}

void AddressWidget::paintEventTags(QPainter& painter) const
{
    if (events_.empty() || totalRows_ <= 1 || !haveAddressRange_) {
        return;
    }

    const QRect plot = plotRect();
    const double pixelsPerEvent = events_.empty()
        ? static_cast<double>(plot.width())
        : static_cast<double>(timelineContentWidth()) / static_cast<double>(events_.size());
    if (pixelsPerEvent < kEventTagMinimumPixelsPerRow && !hoveredEventTagIndex_) {
        return;
    }

    QFont tagFont = painter.font();
    const qreal pointSize = tagFont.pointSizeF();
    if (pointSize > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(6.5, pointSize * 0.78));
    }
    const QFontMetrics metrics(tagFont);
    const int tagHeight = metrics.height() + 4;
    const int digits = addressHexDigitsFromMax(maxAddress_);
    const auto [firstTimestamp, lastTimestamp] = visibleTimestampRange();

    painter.save();
    painter.setFont(tagFont);
    painter.setRenderHint(QPainter::Antialiasing, true);

    std::vector<QRect> occupied;
    const AddressEvent probe{0, firstTimestamp, 0, AddressEventKind::Read, 0};
    auto it = std::lower_bound(events_.cbegin(),
                               events_.cend(),
                               probe,
                               [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                   return lhs.timestamp < rhs.timestamp;
    });
    int candidates = 0;
    for (; it != events_.cend() && it->timestamp <= lastTimestamp; ++it) {
        const std::size_t eventIndex = static_cast<std::size_t>(std::distance(events_.cbegin(), it));
        const bool hovered = hoveredEventTagIndex_ == eventIndex;
        if (!eventVisible(*it)) {
            continue;
        }
        if (++candidates > kEventTagMaxCandidatesPerPaint && !hovered) {
            break;
        }

        const auto x = screenXForTimestamp(it->timestamp, plot);
        const auto y = screenYForAddress(it->address, plot);
        if (!x || !y) {
            continue;
        }
        QString label = addressText(it->address, digits);
        if (it->opcodeLabelIndex > 0 && it->opcodeLabelIndex < opcodeLabels_.size()) {
            label += QStringLiteral(" ");
            label += opcodeLabels_[it->opcodeLabelIndex];
        }
        const int textWidth = metrics.horizontalAdvance(label);
        if (textWidth <= 0 || textWidth > kEventTagMaxTextWidth) {
            continue;
        }
        QRect tagRect(*x + 7, *y - tagHeight / 2, textWidth + kEventTagPaddingX * 2, tagHeight);
        if (tagRect.right() > plot.right() - 2) {
            tagRect.moveRight(*x - 7);
        }
        if (tagRect.left() < plot.left() + 2 || tagRect.top() < plot.top() || tagRect.bottom() > plot.bottom()) {
            continue;
        }
        const QRect padded = tagRect.adjusted(-kEventTagGap, -2, kEventTagGap, 2);
        const bool overlaps = std::any_of(occupied.cbegin(), occupied.cend(), [&padded](const QRect& rect) {
            return padded.intersects(rect);
        });
        if (overlaps && !hovered) {
            continue;
        }
        const bool selected = selectedLogicalRow_ >= 0
            && it->logicalRow == static_cast<std::uint64_t>(selectedLogicalRow_)
            && (!selectedAddress_ || *selectedAddress_ == it->address);
        const QColor accent = kindColor(it->kind);
        painter.setPen(QPen(withAlpha(accent.darker(120), overlaps && !selected && !hovered ? 130 : 255), 1));
        painter.setBrush(withAlpha(QColor(QStringLiteral("#FFFFFF")), overlaps && !selected && !hovered ? 120 : 236));
        painter.drawRoundedRect(tagRect, 3, 3);
        painter.setPen(withAlpha(accent.darker(160), overlaps && !selected && !hovered ? 140 : 255));
        painter.drawText(tagRect.adjusted(kEventTagPaddingX, 0, -kEventTagPaddingX, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         label);
        occupied.push_back(tagRect);
    }

    painter.restore();
}

void AddressWidget::paintCursorAndMarker(QPainter& painter) const
{
    const QRect plot = plotRect();
    const QRect axis = addressAxisRect();
    const QRect ruler = topRulerRect();
    const auto drawMarker = [&](const int logicalRow,
                                const QColor& color,
                                const QString& label,
                                const Qt::PenStyle style) {
        if (logicalRow < 0) {
            return;
        }
        const auto x = screenXForLogicalRow(static_cast<std::uint64_t>(logicalRow), plot);
        if (!x) {
            return;
        }
        painter.save();
        painter.setClipRect(QRect(plot.left(), ruler.top(), plot.width(), plot.height() + ruler.height()));
        painter.fillRect(QRect(*x - 1, plot.top(), 3, plot.height()), withAlpha(color, 42));
        painter.setPen(QPen(color, 2, style));
        painter.drawLine(*x, ruler.top(), *x, plot.bottom());
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        const QPolygon triangle({QPoint(*x - 5, ruler.top() + 1),
                                 QPoint(*x + 5, ruler.top() + 1),
                                 QPoint(*x, ruler.top() + 9)});
        painter.drawPolygon(triangle);
        painter.setPen(color.darker(130));
        painter.drawText(QRect(*x + 6, ruler.top() + 2, 180, 16),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         label);
        painter.restore();
    };

    if (selectedAddress_) {
        if (const auto y = screenYForAddress(*selectedAddress_, plot)) {
            const QColor color(QStringLiteral("#C48A00"));
            painter.save();
            painter.setClipRect(QRect(axis.left(), plot.top(), axis.width() + plot.width(), plot.height()));
            painter.fillRect(QRect(plot.left(), *y - 1, plot.width(), 3), withAlpha(color, 36));
            painter.setPen(QPen(color, 1, Qt::SolidLine));
            painter.drawLine(axis.left(), *y, plot.right(), *y);
            painter.restore();
        }
    }

    drawMarker(markerLogicalRow_, QColor(QStringLiteral("#2F80ED")),
               markerLogicalRow_ >= 0 ? QStringLiteral("M %1").arg(markerLogicalRow_ + 1) : QString(),
               Qt::DashLine);
    drawMarker(selectedLogicalRow_, QColor(QStringLiteral("#C48A00")),
               selectedLogicalRow_ >= 0 ? QStringLiteral("C %1").arg(selectedLogicalRow_ + 1) : QString(),
               Qt::SolidLine);
}

void AddressWidget::paintCursorAddressTag(QPainter& painter) const
{
    if (!selectedAddress_) {
        return;
    }

    const QRect tagRect = cursorAddressTagRect();
    if (tagRect.isEmpty()) {
        return;
    }

    const QColor color(QStringLiteral("#C48A00"));
    const int digits = addressHexDigitsFromMax(maxAddress_);
    const QString label = addressText(*selectedAddress_, digits);
    QFont tagFont = painter.font();
    tagFont.setPointSizeF(qMax<qreal>(6.5, tagFont.pointSizeF() * 0.86));

    painter.save();
    painter.setFont(tagFont);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color.darker(115), 1));
    painter.setBrush(withAlpha(QColor(QStringLiteral("#FFF7D6")), 245));
    painter.drawRoundedRect(tagRect, 3, 3);
    painter.setPen(color.darker(150));
    const QFontMetrics metrics(tagFont);
    painter.drawText(tagRect.adjusted(6, 0, -6, 0),
                     Qt::AlignVCenter | Qt::AlignRight,
                     metrics.elidedText(label, Qt::ElideLeft, tagRect.width() - 12));
    painter.restore();
}

void AddressWidget::paintFullRuler(QPainter& painter) const
{
    const QRect full = fullRulerRect();
    painter.fillRect(full, QColor(QStringLiteral("#F6F8FB")));
    painter.setPen(QColor(QStringLiteral("#D8DEE8")));
    painter.drawLine(full.left(), full.top(), full.right(), full.top());
    painter.drawLine(full.left(), full.bottom(), full.right(), full.bottom());

    if (totalRows_ == 0) {
        return;
    }

    painter.setPen(QColor(QStringLiteral("#CAD2DD")));
    const int ticks = 20;
    for (int index = 0; index <= ticks; ++index) {
        const int x = full.left() + static_cast<int>(
            std::llround(static_cast<double>(index) * full.width() / ticks));
        const int tickHeight = index % 5 == 0 ? 13 : 7;
        painter.drawLine(x, full.bottom(), x, full.bottom() - tickHeight);
    }

    const QRect viewRect = overviewWindowRect();
    painter.fillRect(viewRect, withAlpha(QColor(QStringLiteral("#6BA7FF")), 75));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(QStringLiteral("#3879D9")));
    painter.drawRect(viewRect.adjusted(0, 0, -1, -1));

    painter.setPen(QColor(QStringLiteral("#596579")));
    painter.drawText(full.adjusted(8, 0, -8, 0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Full trace"));
}

void AddressWidget::paintRangeZoom(QPainter& painter) const
{
    if (dragMode_ != DragMode::RangeZoom && dragMode_ != DragMode::FullRangeZoom) {
        return;
    }
    const QRect area = dragMode_ == DragMode::FullRangeZoom ? fullRulerRect() : plotRect();
    const int left = qBound(area.left(), std::min(dragStart_.x(), dragLast_.x()), area.right());
    const int right = qBound(area.left(), std::max(dragStart_.x(), dragLast_.x()), area.right());
    QRect selection(left, area.top(), qMax(1, right - left), area.height());
    painter.fillRect(selection, withAlpha(QColor(QStringLiteral("#2F80ED")), 42));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(QStringLiteral("#2F80ED")), 1, Qt::DashLine));
    painter.drawRect(selection.adjusted(0, 0, -1, -1));
}

void AddressWidget::paintGestureOverlay(QPainter& painter) const
{
    if (!leftDragGestureActive_) {
        return;
    }
    if (dragMode_ != DragMode::PendingGesture
        && dragMode_ != DragMode::ZoomFull
        && dragMode_ != DragMode::ZoomIn2x
        && dragMode_ != DragMode::ZoomOut2x) {
        return;
    }

    const QPoint delta = dragLast_ - dragStart_;
    if (delta.manhattanLength() < kGestureActivationDistance) {
        return;
    }

    const int octant = snappedGestureOctant(delta);
    QString label;
    if (dragMode_ == DragMode::ZoomFull || octant == 2 || octant == -2) {
        label = QStringLiteral("Zoom full");
    } else if (dragMode_ == DragMode::ZoomIn2x || octant == -1) {
        label = QStringLiteral("Zoom in 2x");
    } else if (dragMode_ == DragMode::ZoomOut2x || octant == 1) {
        label = QStringLiteral("Zoom out 2x");
    } else {
        return;
    }

    const double length = qBound(28.0,
                                 std::hypot(static_cast<double>(delta.x()),
                                            static_cast<double>(delta.y())),
                                 120.0);
    const double angle = static_cast<double>(octant) * (kPi / 4.0);
    const QPointF start(dragStart_);
    const QPointF end(start.x() + std::cos(angle) * length,
                      start.y() + std::sin(angle) * length);
    const QPointF unit(std::cos(angle), std::sin(angle));
    const QPointF normal(-unit.y(), unit.x());
    const QPointF headLeft = end - unit * 10.0 + normal * 5.0;
    const QPointF headRight = end - unit * 10.0 - normal * 5.0;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#1F5FBF")), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(start, end);
    painter.drawLine(end, headLeft);
    painter.drawLine(end, headRight);

    const QFontMetrics fm(painter.font());
    QRect labelRect(QPoint(qRound(end.x()) + 10, qRound(end.y()) - fm.height() / 2 - 4),
                    QSize(fm.horizontalAdvance(label) + 16, fm.height() + 8));
    const QRect bounds = rect().adjusted(4, 4, -4, -4);
    if (labelRect.right() > bounds.right()) {
        labelRect.moveRight(qRound(end.x()) - 10);
    }
    if (labelRect.left() < bounds.left()) {
        labelRect.moveLeft(bounds.left());
    }
    if (labelRect.top() < bounds.top()) {
        labelRect.moveTop(bounds.top());
    }
    if (labelRect.bottom() > bounds.bottom()) {
        labelRect.moveBottom(bounds.bottom());
    }

    painter.setPen(QPen(QColor(QStringLiteral("#8BAFE8")), 1));
    painter.setBrush(withAlpha(QColor(QStringLiteral("#FFFFFF")), 235));
    painter.drawRoundedRect(labelRect, 4, 4);
    painter.setPen(QColor(QStringLiteral("#123B72")));
    painter.drawText(labelRect, Qt::AlignCenter, label);
    painter.restore();
}

void AddressWidget::paintStatus(QPainter& painter) const
{
    const QRect plot = plotRect();
    if (building_) {
        painter.fillRect(plot.adjusted(12, 12, -12, -(plot.height() - 34)),
                         withAlpha(QColor(QStringLiteral("#FFFFFF")), 220));
        painter.setPen(QColor(QStringLiteral("#4B5563")));
        const QString text = totalRows_ > 0
            ? QStringLiteral("%1 (%2/%3)").arg(statusText_).arg(processedRows_).arg(totalRows_)
            : statusText_;
        painter.drawText(plot.adjusted(20, 12, -20, -12),
                         Qt::AlignTop | Qt::AlignLeft,
                         text);
        return;
    }

    if (!statusText_.isEmpty() && events_.empty()) {
        painter.setPen(QColor(QStringLiteral("#7A8494")));
        painter.drawText(plot, Qt::AlignCenter, statusText_);
    }

    if (!events_.empty() && visibleAddressEventCount() == 0) {
        painter.setPen(QColor(QStringLiteral("#7A8494")));
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("All address categories are hidden."));
    }

    painter.setPen(QColor(QStringLiteral("#6B7280")));
    QString mode = snapToEvents_ ? QStringLiteral("Snap") : QStringLiteral("Free");
    if (lockCursorMarkerDelta_) {
        mode += QStringLiteral(" | Delta lock");
    }
    if (haveAddressRange_ && verticalZoom_ > kMinVerticalZoom + 0.0001) {
        mode += QStringLiteral(" | Y %1x").arg(verticalZoom_, 0, 'f', verticalZoom_ < 10.0 ? 2 : 1);
    }
    painter.drawText(plot.adjusted(8, 0, -8, -4),
                     Qt::AlignRight | Qt::AlignBottom,
                     mode);
}

#ifdef CHIRON_GUI_ENABLE_ADDRESS_TEST_API
AddressEventKind AddressWidget::testKindForLogicalRow(const int logicalRow) const
{
    const AddressEvent probe{static_cast<std::uint64_t>(qMax(0, logicalRow)),
                             0,
                             0,
                             AddressEventKind::Read,
                             0};
    const auto found = std::lower_bound(events_.cbegin(),
                                        events_.cend(),
                                        probe,
                                        [](const AddressEvent& lhs, const AddressEvent& rhs) {
                                            return lhs.logicalRow < rhs.logicalRow;
                                        });
    if (found != events_.cend()
        && logicalRow >= 0
        && found->logicalRow == static_cast<std::uint64_t>(logicalRow)) {
        return found->kind;
    }
    return AddressEventKind::Read;
}

QRect AddressWidget::testLegendToggleRect(const AddressEventKind kind) const
{
    const std::vector<LegendItem> items = legendItems();
    for (const LegendItem& item : items) {
        if (item.kind == kind) {
            return item.rect;
        }
    }
    return QRect();
}

std::pair<std::uint64_t, std::uint64_t> AddressWidget::testVisibleAddressRange() const
{
    const auto [visibleMin, visibleMax] = visibleAddressRange();
    return {static_cast<std::uint64_t>(std::llround(std::max<long double>(0.0L, visibleMin))),
            static_cast<std::uint64_t>(std::llround(std::max<long double>(0.0L, visibleMax)))};
}
#endif

}  // namespace CHIron::Gui
