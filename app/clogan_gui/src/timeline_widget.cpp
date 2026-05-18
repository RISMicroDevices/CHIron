#include "timeline_widget.hpp"

#include "channel_badge_painter.hpp"
#include "gui_colors.hpp"
#include "gui_format.hpp"

#include <QCursor>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace CHIron::Gui {

namespace {

constexpr int kHeaderHeight = 26;
constexpr int kTopRulerHeight = 44;
constexpr int kFullRulerHeight = 28;
constexpr int kScrollBarHeight = 16;
constexpr int kScrollBarWidth = 16;
constexpr int kRightMargin = 10;
constexpr int kLaneHeight = 19;
constexpr int kLaneGap = 1;
constexpr int kLaneStride = kLaneHeight + kLaneGap;
constexpr int kMinNodeColumnWidth = 48;
constexpr int kMinChannelColumnWidth = 60;
constexpr int kMaxColumnWidth = 640;
constexpr int kResizeGripWidth = 5;
constexpr int kHeaderPadding = 8;
constexpr int kNodeNumberAreaWidth = 40;
constexpr int kMaxScrollBarRange = 1000000000;
constexpr int kRowsBuildProgressStep = 8192;
constexpr int kSessionReadChunkRows = 65536;
constexpr int kMaxEventsPerPixelPerLane = 5;
constexpr int kInlineWaveformRenderEventLimit = 80000;
constexpr int kWaveformProgressivePixelThreshold = 8;
constexpr int kOpcodeTagPaddingX = 5;
constexpr int kOpcodeTagGap = 6;
constexpr int kOpcodeTagMaxTextWidth = 150;
constexpr int kOpcodeTagMaxCandidatesPerPaint = 4096;
constexpr double kOpcodeTagMinimumPixelsPerRow = 2.0;
constexpr int kOpcodeTagFlitClearance = 3;
constexpr int kGestureActivationDistance = 7;
constexpr int kGestureDirectionDistance = 24;
constexpr int kLaneReorderActivationDistance = 5;
constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 1048576.0;
constexpr double kPi = 3.14159265358979323846;
constexpr std::uint32_t kInvalidNodeId = std::numeric_limits<std::uint32_t>::max();

QColor withAlpha(QColor color, const int alpha)
{
    color.setAlpha(alpha);
    return color;
}

qint64 tickStartForStep(const qint64 value, const qint64 step)
{
    if (step <= 0) {
        return value;
    }
    if (value >= 0) {
        return (value / step) * step;
    }
    return -(((-value + step - 1) / step) * step);
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

QColor nodeBadgeColor(const QString& label)
{
    if (label == QLatin1String("RN-F")) {
        return QColor(QStringLiteral("#2F80ED"));
    }
    if (label == QLatin1String("RN-D")) {
        return QColor(QStringLiteral("#56CCF2"));
    }
    if (label == QLatin1String("RN-I")) {
        return QColor(QStringLiteral("#00A878"));
    }
    if (label == QLatin1String("HN-F")) {
        return QColor(QStringLiteral("#9B51E0"));
    }
    if (label == QLatin1String("HN-I")) {
        return QColor(QStringLiteral("#6C63FF"));
    }
    if (label == QLatin1String("SN-F")) {
        return QColor(QStringLiteral("#EB5757"));
    }
    if (label == QLatin1String("SN-I")) {
        return QColor(QStringLiteral("#F2994A"));
    }
    if (label == QLatin1String("MN")) {
        return QColor(QStringLiteral("#219653"));
    }
    if (label == QLatin1String("SAM")) {
        return QColor(QStringLiteral("#D5DAE1"));
    }
    return QColor(QStringLiteral("#8B949E"));
}

QColor eventColor(const FlitChannel channel, const FlitDirection direction)
{
    QColor color = ChannelAccent(channel);
    return direction == FlitDirection::Rx ? color.darker(112) : color.darker(126);
}

QString channelText(const FlitDirection direction, const FlitChannel channel)
{
    return ToString(direction) + ToString(channel);
}

QString nodeIdText(const std::uint32_t nodeId)
{
    if (nodeId == kInvalidNodeId) {
        return QStringLiteral("-");
    }
    return QString::number(nodeId);
}

QString transportNodeType(const std::shared_ptr<TraceSession>& session, const std::uint32_t nodeId)
{
    if (!session || nodeId == kInvalidNodeId || nodeId > std::numeric_limits<quint16>::max()) {
        return QString();
    }
    const auto found = session->metadata().nodeTypes.find(static_cast<quint16>(nodeId));
    if (found == session->metadata().nodeTypes.cend()) {
        return QString();
    }
    return QString::fromStdString(CLog::NodeTypeToString(found->second));
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

FlitDirection directionFromTransport(const CLog::Channel channel) noexcept
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXRSP:
    case CLog::Channel::TXDAT:
    case CLog::Channel::TXSNP:
    case CLog::Channel::TXREQ_BeforeSAM:
        return FlitDirection::Tx;
    case CLog::Channel::RXREQ:
    case CLog::Channel::RXRSP:
    case CLog::Channel::RXDAT:
    case CLog::Channel::RXSNP:
    case CLog::Channel::RXREQ_BeforeSAM:
        return FlitDirection::Rx;
    }
    return FlitDirection::Tx;
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

quint64 laneKey(const std::uint32_t nodeId,
                const FlitDirection direction,
                const FlitChannel channel)
{
    const quint64 node = static_cast<quint64>(nodeId);
    const quint64 dir = direction == FlitDirection::Tx ? 0 : 1;
    const quint64 ch = static_cast<quint64>(channel);
    return (node << 8U) | (dir << 4U) | ch;
}

quint64 opcodeCacheKey(const std::uint8_t channel, const std::uint32_t opcode) noexcept
{
    return (static_cast<quint64>(channel) << 32U) | static_cast<quint64>(opcode);
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

int ensureLane(std::vector<TimelineLane>& lanes,
               QHash<quint64, int>& laneIndexes,
               const std::shared_ptr<TraceSession>& session,
               const std::uint32_t nodeId,
               const FlitDirection direction,
               const FlitChannel channel)
{
    const quint64 key = laneKey(nodeId, direction, channel);
    const auto found = laneIndexes.constFind(key);
    if (found != laneIndexes.cend()) {
        return found.value();
    }

    TimelineLane lane;
    lane.nodeId = nodeId;
    lane.nodeIdLabel = nodeIdText(nodeId);
    lane.nodeTypeLabel = transportNodeType(session, nodeId);
    if (lane.nodeTypeLabel.isEmpty()) {
        lane.nodeTypeLabel = QStringLiteral("Node");
    }
    lane.nodeTypeColor = nodeBadgeColor(lane.nodeTypeLabel);
    lane.channel = channel;
    lane.direction = direction;
    lane.channelLabel = channelText(direction, channel);
    const int laneIndex = static_cast<int>(lanes.size());
    lanes.push_back(std::move(lane));
    laneIndexes.insert(key, laneIndex);
    return laneIndex;
}

bool laneLess(const TimelineLane& a, const TimelineLane& b, const TimelineLaneSortOrder order)
{
    const auto compareNodeThenChannel = [&]() {
        if (a.nodeId != b.nodeId) {
            return a.nodeId < b.nodeId;
        }
        if (a.direction != b.direction) {
            return a.direction == FlitDirection::Tx;
        }
        if (a.channel != b.channel) {
            return static_cast<int>(a.channel) < static_cast<int>(b.channel);
        }
        return a.channelLabel < b.channelLabel;
    };
    const auto compareChannelThenNode = [&]() {
        if (a.direction != b.direction) {
            return a.direction == FlitDirection::Tx;
        }
        if (a.channel != b.channel) {
            return static_cast<int>(a.channel) < static_cast<int>(b.channel);
        }
        if (a.nodeId != b.nodeId) {
            return a.nodeId < b.nodeId;
        }
        return a.nodeTypeLabel < b.nodeTypeLabel;
    };
    return order == TimelineLaneSortOrder::ChannelThenNode
        ? compareChannelThenNode()
        : compareNodeThenChannel();
}

std::vector<int> sortedLaneOrder(const std::vector<TimelineLane>& lanes,
                                 const TimelineLaneSortOrder order)
{
    if (order == TimelineLaneSortOrder::Custom) {
        std::vector<int> indexes(lanes.size());
        for (int index = 0; index < static_cast<int>(indexes.size()); ++index) {
            indexes[static_cast<std::size_t>(index)] = index;
        }
        return indexes;
    }

    std::vector<int> indexes(lanes.size());
    for (int index = 0; index < static_cast<int>(indexes.size()); ++index) {
        indexes[static_cast<std::size_t>(index)] = index;
    }

    std::sort(indexes.begin(), indexes.end(), [&lanes, order](const int lhs, const int rhs) {
        const TimelineLane& a = lanes[static_cast<std::size_t>(lhs)];
        const TimelineLane& b = lanes[static_cast<std::size_t>(rhs)];
        return laneLess(a, b, order);
    });
    return indexes;
}

void applyLaneOrder(std::vector<TimelineLane>& lanes,
                    std::vector<TimelineWidget::TimelineEvent>& events,
                    const std::vector<int>& order)
{
    if (order.size() != lanes.size()) {
        return;
    }

    std::vector<int> remap(lanes.size(), -1);
    std::vector<TimelineLane> sorted;
    sorted.reserve(lanes.size());
    for (int newIndex = 0; newIndex < static_cast<int>(order.size()); ++newIndex) {
        const int oldIndex = order[static_cast<std::size_t>(newIndex)];
        if (oldIndex < 0 || oldIndex >= static_cast<int>(lanes.size())
            || remap[static_cast<std::size_t>(oldIndex)] != -1) {
            return;
        }
        remap[static_cast<std::size_t>(oldIndex)] = newIndex;
        sorted.push_back(std::move(lanes[static_cast<std::size_t>(oldIndex)]));
    }

    for (TimelineWidget::TimelineEvent& event : events) {
        if (event.lane >= 0 && event.lane < static_cast<int>(remap.size())) {
            event.lane = remap[static_cast<std::size_t>(event.lane)];
        }
    }
    std::sort(events.begin(), events.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.logicalRow < rhs.logicalRow;
    });
    lanes = std::move(sorted);
}

void sortLanesAndEvents(std::vector<TimelineLane>& lanes,
                        std::vector<TimelineWidget::TimelineEvent>& events,
                        const TimelineLaneSortOrder order)
{
    if (order == TimelineLaneSortOrder::Custom) {
        return;
    }
    applyLaneOrder(lanes, events, sortedLaneOrder(lanes, order));
}

std::vector<std::vector<std::uint64_t>>
buildLaneRows(const std::vector<TimelineLane>& lanes, const std::vector<TimelineWidget::TimelineEvent>& events)
{
    std::vector<std::vector<std::uint64_t>> laneRows(lanes.size());
    for (const TimelineWidget::TimelineEvent& event : events) {
        if (event.lane >= 0 && event.lane < static_cast<int>(laneRows.size())) {
            laneRows[static_cast<std::size_t>(event.lane)].push_back(event.logicalRow);
        }
    }
    for (std::vector<std::uint64_t>& rows : laneRows) {
        std::sort(rows.begin(), rows.end());
        rows.shrink_to_fit();
    }
    return laneRows;
}

std::vector<TimelineWidget::TimelineEvent>::const_iterator
lowerBoundEventByLogicalRow(const std::vector<TimelineWidget::TimelineEvent>& events,
                            const std::uint64_t logicalRow)
{
    return std::lower_bound(events.cbegin(),
                            events.cend(),
                            logicalRow,
                            [](const TimelineWidget::TimelineEvent& event,
                               const std::uint64_t row) {
                                return event.logicalRow < row;
                            });
}

std::vector<TimelineWidget::TimelineEvent>::const_iterator
findEventByLogicalRow(const std::vector<TimelineWidget::TimelineEvent>& events,
                      const std::uint64_t logicalRow)
{
    const auto found = lowerBoundEventByLogicalRow(events, logicalRow);
    return found != events.cend() && found->logicalRow == logicalRow ? found : events.cend();
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

}  // namespace

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("timelineWidget"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumHeight(160);

    horizontalScrollBar_ = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar_->setObjectName(QStringLiteral("timelineHorizontalScrollBar"));
    connect(horizontalScrollBar_, &QScrollBar::valueChanged, this, [this](const int value) {
        const std::uint64_t maxOffset = horizontalContentRange();
        const std::uint64_t offset = unscaledScrollBarValue(value, maxOffset);
        if (scrollOffset_ != offset) {
            scrollOffset_ = offset;
            update();
        }
    });

    verticalScrollBar_ = new QScrollBar(Qt::Vertical, this);
    verticalScrollBar_->setObjectName(QStringLiteral("timelineVerticalScrollBar"));
    connect(verticalScrollBar_, &QScrollBar::valueChanged, this, [this](const int value) {
        if (verticalScrollOffset_ != value) {
            verticalScrollOffset_ = value;
            update();
        }
    });

    statusText_ = QStringLiteral("Open a trace to populate the timeline.");
    rebuildContentMetrics();
    updateScrollBars();
}

TimelineWidget::~TimelineWidget()
{
    stopWaveformRender();
    stopBuildThread();
}

void TimelineWidget::setTraceSession(std::shared_ptr<TraceSession> session)
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    stopWaveformRender();
    stopBuildThread();
    resetCustomLaneSortOrder();
    pendingViewState_.clear();
    traceSession_ = std::move(session);
    rowSnapshot_.reset();
    lanes_.clear();
    events_.clear();
    opcodeLabels_.clear();
    laneRows_.reset();
    rowTimestamps_.reset();
    rowTimestampsMonotonic_ = true;
    invalidateWaveformCache();
    selectedLogicalRow_ = -1;
    markerLogicalRow_ = -1;
    selectedLane_ = -1;
    highlightedLane_ = -1;
    hoveredOpcodeTagEventIndex_.reset();
    selectedLaneFilter_.reset();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    totalRows_ = traceSession_ ? traceSession_->totalRows() : 0;
    processedRows_ = 0;
    hasTimestampRange_ = traceSession_ && totalRows_ > 0;
    firstTimestamp_ = hasTimestampRange_ ? traceSession_->metadata().firstTimestamp : 0;
    lastTimestamp_ = hasTimestampRange_ ? traceSession_->metadata().lastTimestamp : 0;
    building_ = false;
    buildRequested_ = false;
    statusText_ = traceSession_ ? pendingBuildText()
                                : QStringLiteral("Open a trace to populate the timeline.");
    rebuildContentMetrics();
    updateAutoColumnWidths();
    updateScrollBars();
    update();
}

void TimelineWidget::setRows(std::vector<FlitRecord> rows)
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    stopWaveformRender();
    stopBuildThread();
    resetCustomLaneSortOrder();
    pendingViewState_.clear();
    traceSession_.reset();
    rowSnapshot_ = std::make_shared<const std::vector<FlitRecord>>(std::move(rows));
    lanes_.clear();
    events_.clear();
    opcodeLabels_.clear();
    laneRows_.reset();
    rowTimestamps_.reset();
    rowTimestampsMonotonic_ = true;
    invalidateWaveformCache();
    selectedLogicalRow_ = -1;
    markerLogicalRow_ = -1;
    selectedLane_ = -1;
    highlightedLane_ = -1;
    hoveredOpcodeTagEventIndex_.reset();
    selectedLaneFilter_.reset();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    totalRows_ = rowSnapshot_ ? rowSnapshot_->size() : 0;
    processedRows_ = 0;
    firstTimestamp_ = 0;
    lastTimestamp_ = 0;
    hasTimestampRange_ = false;
    building_ = false;
    buildRequested_ = false;
    statusText_ = totalRows_ == 0 ? QStringLiteral("No flits in the current trace.")
                                  : pendingBuildText();
    rebuildContentMetrics();
    updateScrollBars();
    update();
}

void TimelineWidget::buildView()
{
    if (building_) {
        return;
    }

    if (traceSession_) {
        buildRequested_ = true;
        building_ = true;
        statusText_ = QStringLiteral("Timeline: reading fast trace index...");
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
                                      : QStringLiteral("Timeline: grouping rows...");
        update();
        startRowsBuild(rowSnapshot_, !building_);
        return;
    }

    statusText_ = QStringLiteral("Open a trace to populate the timeline.");
    update();
}

void TimelineWidget::clear()
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    stopWaveformRender();
    stopBuildThread();
    resetCustomLaneSortOrder();
    pendingViewState_.clear();
    traceSession_.reset();
    rowSnapshot_.reset();
    lanes_.clear();
    events_.clear();
    opcodeLabels_.clear();
    laneRows_.reset();
    rowTimestamps_.reset();
    rowTimestampsMonotonic_ = true;
    invalidateWaveformCache();
    selectedLaneFilter_.reset();
    viewHistory_.clear();
    totalRows_ = 0;
    processedRows_ = 0;
    firstTimestamp_ = 0;
    lastTimestamp_ = 0;
    hasTimestampRange_ = false;
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    selectedLogicalRow_ = -1;
    markerLogicalRow_ = -1;
    selectedLane_ = -1;
    highlightedLane_ = -1;
    hoveredOpcodeTagEventIndex_.reset();
    building_ = false;
    buildRequested_ = false;
    statusText_ = QStringLiteral("Open a trace to populate the timeline.");
    rebuildContentMetrics();
    updateAutoColumnWidths();
    updateScrollBars();
    update();
}

void TimelineWidget::setSelectedLogicalRow(const int logicalRow)
{
    if (logicalRow < 0) {
        selectedLogicalRow_ = -1;
        highlightedLane_ = -1;
        hoveredOpcodeTagEventIndex_.reset();
        update();
        return;
    }

    selectedLogicalRow_ = logicalRow;
    if (hasPendingBuild()) {
        update();
        return;
    }

    setCursorLogicalRow(logicalRow, false);
    updateLaneSelectionFromFilter();
}

QVariantMap TimelineWidget::viewState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("horizontalZoom"), horizontalZoom_);
    state.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(
                     static_cast<qulonglong>(scrollOffset_)));
    const auto [visibleFirstTimestamp, visibleLastTimestamp] = visibleTimeRange();
    state.insert(QStringLiteral("visibleFirstTimestamp"), visibleFirstTimestamp);
    state.insert(QStringLiteral("visibleLastTimestamp"), visibleLastTimestamp);
    const auto [visibleFirstLogicalRow, visibleLastLogicalRow] = visibleLogicalRowRange();
    state.insert(QStringLiteral("visibleFirstLogicalRow"),
                 QVariant::fromValue<qulonglong>(static_cast<qulonglong>(visibleFirstLogicalRow)));
    state.insert(QStringLiteral("visibleLastLogicalRow"),
                 QVariant::fromValue<qulonglong>(static_cast<qulonglong>(visibleLastLogicalRow)));
    state.insert(QStringLiteral("hasTimestampRange"), hasTimestampRange_);
    state.insert(QStringLiteral("verticalScrollOffset"), verticalScrollOffset_);
    state.insert(QStringLiteral("selectedLogicalRow"), selectedLogicalRow_);
    state.insert(QStringLiteral("markerLogicalRow"), markerLogicalRow_);
    state.insert(QStringLiteral("selectedLane"), selectedLane_);
    state.insert(QStringLiteral("laneSortOrder"), static_cast<int>(laneSortOrder_));
    if (laneSortOrder_ == TimelineLaneSortOrder::Custom) {
        state.insert(QStringLiteral("customLaneOrder"), laneStateOrder());
    }
    state.insert(QStringLiteral("nodeColumnWidth"), nodeColumnWidth_);
    state.insert(QStringLiteral("channelColumnWidth"), channelColumnWidth_);
    state.insert(QStringLiteral("nodeColumnUserSized"), nodeColumnUserSized_);
    state.insert(QStringLiteral("channelColumnUserSized"), channelColumnUserSized_);
    return state;
}

void TimelineWidget::restoreViewState(const QVariantMap& state)
{
    pendingViewState_ = state;
    applyViewState(state);
}

void TimelineWidget::setSelectedRow(const FlitDirection direction,
                                    const FlitChannel channel,
                                    const std::optional<std::uint32_t> nodeId)
{
    LaneSelector selector;
    selector.direction = direction;
    selector.channel = channel;
    selector.nodeId = nodeId;
    selectedLaneFilter_ = selector;
    updateLaneSelectionFromFilter();
    update();
}

void TimelineWidget::clearSelectedRow()
{
    selectedLaneFilter_.reset();
    highlightedLane_ = -1;
    update();
}

QSize TimelineWidget::minimumSizeHint() const
{
    return QSize(420, 170);
}

QSize TimelineWidget::sizeHint() const
{
    return QSize(980, 260);
}

void TimelineWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    requestWaveformRerender();
    rebuildContentMetrics();
    updateScrollBars();
}

void TimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), QColor(QStringLiteral("#FFFFFF")));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    paintHeader(painter);
    paintTopRuler(painter);
    paintLaneTable(painter);
    paintWaveforms(painter);
    paintCursorAndMarker(painter);
    paintRangeZoom(painter);
    paintGestureOverlay(painter);
    paintFullRuler(painter);
    paintStatus(painter);
    paintBuildPrompt(painter);
}

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
    if (hasPendingBuild()) {
        event->ignore();
        return;
    }

    const QPoint pixelDelta = event->pixelDelta();
    const QPoint angleDelta = event->angleDelta();
    const int verticalDelta = pixelDelta.y() != 0 ? pixelDelta.y() : angleDelta.y();
    const int horizontalDelta = pixelDelta.x() != 0 ? pixelDelta.x() : angleDelta.x();

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
        panVerticallyByPixels(verticalDelta < 0 ? kLaneStride * 3 : -kLaneStride * 3);
        event->accept();
        return;
    }

    event->ignore();
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
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
    if (event->button() == Qt::LeftButton && headerRect().contains(event->pos())) {
        if (nodeHeaderRect().contains(event->pos())) {
            setLaneSortOrder(TimelineLaneSortOrder::NodeThenChannel);
            event->accept();
            return;
        }
        if (channelHeaderRect().contains(event->pos())) {
            setLaneSortOrder(TimelineLaneSortOrder::ChannelThenNode);
            event->accept();
            return;
        }
    }

    const ResizeColumn column = columnResizeHandleAt(event->pos());
    if (event->button() == Qt::LeftButton && column != ResizeColumn::None) {
        dragMode_ = DragMode::ColumnResize;
        resizeColumn_ = column;
        resizeStartNodeColumnWidth_ = nodeColumnWidth_;
        resizeStartChannelColumnWidth_ = channelColumnWidth_;
        setCursor(Qt::SplitHCursor);
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

    if (event->button() == Qt::LeftButton && positionInLaneTable(event->pos())) {
        beginLaneReorder(event->pos());
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton
        && (positionInTimeline(event->pos()) || positionInRuler(event->pos()))) {
        beginPendingGesture(event->pos());
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const std::optional<int> lane = laneAtPosition(event->pos());
        if (lane) {
            selectedLane_ = *lane;
        }

        if (const std::optional<int> row = logicalRowAtPosition(event->pos())) {
            setCursorLogicalRow(*row, true);
        } else {
            update();
        }
        event->accept();
        return;
    }

    event->ignore();
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && (positionInTimeline(event->pos()) || positionInRuler(event->pos()))) {
        zoomToFit();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragMode_ == DragMode::LaneReorder) {
        updateLaneReorder(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ColumnResize) {
        const int dx = event->pos().x() - dragStart_.x();
        if (resizeColumn_ == ResizeColumn::Node) {
            setColumnWidth(ResizeColumn::Node, resizeStartNodeColumnWidth_ + dx, true);
        } else if (resizeColumn_ == ResizeColumn::Channel) {
            setColumnWidth(ResizeColumn::Channel, resizeStartChannelColumnWidth_ + dx, true);
        }
        event->accept();
        return;
    }

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

    if (dragMode_ == DragMode::ZoomFull) {
        updateLeftDragGesture(event->pos(), event->modifiers());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ZoomIn2x) {
        updateLeftDragGesture(event->pos(), event->modifiers());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ZoomOut2x) {
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

    updateHoveredOpcodeTag(event->pos());
    updateResizeCursor(event->pos());
    QWidget::mouseMoveEvent(event);
}

void TimelineWidget::leaveEvent(QEvent* event)
{
    if (hoveredOpcodeTagEventIndex_) {
        hoveredOpcodeTagEventIndex_.reset();
        update(plotRect());
    }
    QWidget::leaveEvent(event);
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragMode_ == DragMode::LaneReorder && event->button() == Qt::LeftButton) {
        finishLaneReorder(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ColumnResize && event->button() == Qt::LeftButton) {
        dragMode_ = DragMode::None;
        resizeColumn_ = ResizeColumn::None;
        leftDragGestureActive_ = false;
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

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
        if (const std::optional<int> lane = laneAtPosition(event->pos())) {
            selectedLane_ = *lane;
        }
        if (const std::optional<int> row = logicalRowAtPosition(event->pos())) {
            setCursorLogicalRow(*row, true);
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
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::FullRangeZoom && event->button() == Qt::LeftButton) {
        updateFullRangeZoom(event->pos());
        finishFullRangeZoom();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ZoomFull && event->button() == Qt::LeftButton) {
        dragLast_ = event->pos();
        finishZoomFullGesture();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ZoomIn2x && event->button() == Qt::LeftButton) {
        dragLast_ = event->pos();
        finishZoomIn2xGesture();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::ZoomOut2x && event->button() == Qt::LeftButton) {
        dragLast_ = event->pos();
        finishZoomOut2xGesture();
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::Pan
        && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        unsetCursor();
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::OverviewPan && event->button() == Qt::LeftButton) {
        updateOverviewPan(event->pos());
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        updateResizeCursor(event->pos());
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void TimelineWidget::keyPressEvent(QKeyEvent* event)
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
    case Qt::Key_Up:
        panVerticallyByPixels(shift ? -kLaneStride * 6 : -kLaneStride);
        event->accept();
        return;
    case Qt::Key_Down:
        panVerticallyByPixels(shift ? kLaneStride * 6 : kLaneStride);
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
        if (dragMode_ == DragMode::LaneReorder) {
            cancelLaneReorder();
            event->accept();
            return;
        }
        if (dragMode_ == DragMode::RangeZoom) {
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

void TimelineWidget::stopBuildThread()
{
    ++buildGeneration_;
    if (buildThread_.joinable()) {
        buildThread_.request_stop();
    }
    building_ = false;
}

void TimelineWidget::stopWaveformRender()
{
    ++waveformRenderGeneration_;
    if (waveformRenderThread_.joinable()) {
        waveformRenderThread_.request_stop();
    }
    waveformRenderPending_ = false;
}

void TimelineWidget::invalidateWaveformCache()
{
    ++waveformRenderGeneration_;
    waveformCacheValid_ = false;
    waveformCacheImage_ = QImage();
    waveformPendingKey_ = WaveformCacheKey{};
    waveformRenderPending_ = false;
}

void TimelineWidget::requestWaveformRerender()
{
    ++waveformRenderGeneration_;
    waveformRenderPending_ = false;
    waveformPendingKey_ = WaveformCacheKey{};
}

void TimelineWidget::startRowsBuild(const std::shared_ptr<const std::vector<FlitRecord>> rows,
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
            : QStringLiteral("Timeline ready.");
        QHash<quint64, int> laneIndexes;
        QHash<QString, std::uint32_t> opcodeLabelIndexes;
        result->opcodeLabels.push_back(QString());
        result->events.reserve(rows->size());
        result->rowTimestamps.reserve(rows->size());
        bool monotonicTimestamps = true;

        for (std::size_t index = 0; index < rows->size(); ++index) {
            if (generation != buildGeneration_) {
                result->cancelled = true;
                return result;
            }
            const FlitRecord& record = rows->at(index);
            if (!result->rowTimestamps.empty() && record.timestamp < result->rowTimestamps.back()) {
                monotonicTimestamps = false;
            }
            result->rowTimestamps.push_back(record.timestamp);
            if (!result->hasTimestampRange) {
                result->firstTimestamp = record.timestamp;
                result->lastTimestamp = record.timestamp;
                result->hasTimestampRange = true;
            } else {
                result->firstTimestamp = std::min(result->firstTimestamp, record.timestamp);
                result->lastTimestamp = std::max(result->lastTimestamp, record.timestamp);
            }
            const std::uint32_t nodeId = record.channelNodeId
                ? static_cast<std::uint32_t>(*record.channelNodeId)
                : kInvalidNodeId;
            const int lane = ensureLane(result->lanes,
                                        laneIndexes,
                                        nullptr,
                                        nodeId,
                                        record.direction,
                                        record.channel);
            ++result->lanes[static_cast<std::size_t>(lane)].count;
            const std::uint32_t opcodeLabelIndex =
                ensureOpcodeLabel(result->opcodeLabels,
                                  opcodeLabelIndexes,
                                  record.opcode);
            result->events.push_back(TimelineEvent{static_cast<std::uint64_t>(index),
                                                   record.timestamp,
                                                   lane,
                                                   opcodeLabelIndex});
            if (index % kRowsBuildProgressStep == 0) {
                result->processedRows = index;
                postBuildProgress(generation, result->processedRows, result->totalRows);
            }
        }

        result->processedRows = result->totalRows;
        result->rowTimestampsMonotonic = monotonicTimestamps;
        sortLanesAndEvents(result->lanes, result->events, laneSortOrder_);
        result->laneRows = buildLaneRows(result->lanes, result->events);
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

void TimelineWidget::startSessionBuild(std::shared_ptr<TraceSession> session)
{
    const quint64 generation = ++buildGeneration_;
    buildThread_ = std::jthread([this, session = std::move(session), generation](const std::stop_token stopToken) {
        auto result = std::make_shared<BuildResult>();
        result->generation = generation;
        result->totalRows = session ? session->totalRows() : 0;
        QHash<quint64, int> laneIndexes;
        QHash<QString, std::uint32_t> opcodeLabelIndexes;
        QHash<quint64, std::uint32_t> opcodeFastLabelIndexes;
        result->opcodeLabels.push_back(QString());
        if (session && result->totalRows > 0) {
            result->rowTimestamps.reserve(static_cast<std::size_t>(
                std::min<std::uint64_t>(result->totalRows, static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))));
            result->firstTimestamp = session->metadata().firstTimestamp;
            result->lastTimestamp = session->metadata().lastTimestamp;
            result->hasTimestampRange = true;
        }

        if (!session) {
            result->statusText = QStringLiteral("Open a trace to populate the timeline.");
        } else {
            result->statusText = QStringLiteral("Timeline ready.");
            result->events.reserve(static_cast<std::size_t>(
                std::min<std::uint64_t>(result->totalRows, 2'000'000)));
            CLogBTraceLoadError error;
            const auto& blocks = session->metadata().blocks;
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
                        result->statusText = QStringLiteral("Timeline unavailable: %1")
                                                 .arg(error.summary.isEmpty()
                                                          ? QStringLiteral("failed to read fast records")
                                                          : error.summary);
                        break;
                    }
                    for (std::size_t index = 0; index < records.size(); ++index) {
                        const CLogBTraceFastRecordInfo& record = records[index];
                        const qint64 timestamp = record.timestamp;
                        result->rowTimestamps.push_back(timestamp);
                        if (!validTransportChannel(record.channel)) {
                            continue;
                        }
                        const CLog::Channel transportChannel = static_cast<CLog::Channel>(record.channel);
                        const FlitDirection direction = directionFromTransport(transportChannel);
                        const FlitChannel channel = channelFromTransport(transportChannel);
                        const int lane = ensureLane(result->lanes,
                                                    laneIndexes,
                                                    session,
                                                    record.nodeId,
                                                    direction,
                                                    channel);
                        ++result->lanes[static_cast<std::size_t>(lane)].count;
                        const quint64 fastOpcodeKey = opcodeCacheKey(record.channel, record.opcode);
                        std::uint32_t opcodeLabelIndex = 0;
                        const auto cachedOpcode = opcodeFastLabelIndexes.constFind(fastOpcodeKey);
                        if (cachedOpcode != opcodeFastLabelIndexes.cend()) {
                            opcodeLabelIndex = cachedOpcode.value();
                        } else {
                            opcodeLabelIndex =
                                ensureOpcodeLabel(result->opcodeLabels,
                                                  opcodeLabelIndexes,
                                                  CLogBTraceLoader::opcodeDisplayValue(
                                                      session->metadata().parameters,
                                                      record));
                            opcodeFastLabelIndexes.insert(fastOpcodeKey, opcodeLabelIndex);
                        }
                        const std::uint64_t logicalRow =
                            block.rowStart + static_cast<std::uint64_t>(localBegin + index);
                        result->events.push_back(TimelineEvent{logicalRow,
                                                               timestamp,
                                                               lane,
                                                               opcodeLabelIndex});
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
            sortLanesAndEvents(result->lanes, result->events, laneSortOrder_);
            result->laneRows = buildLaneRows(result->lanes, result->events);
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

void TimelineWidget::applyBuildResult(std::shared_ptr<BuildResult> result)
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    if (!result || result->generation != buildGeneration_ || result->cancelled) {
        return;
    }

    lanes_ = std::move(result->lanes);
    events_ = std::move(result->events);
    opcodeLabels_ = std::move(result->opcodeLabels);
    laneRows_ = std::make_shared<const std::vector<std::vector<std::uint64_t>>>(
        std::move(result->laneRows));
    rowTimestamps_ = std::make_shared<const std::vector<qint64>>(std::move(result->rowTimestamps));
    rowTimestampsMonotonic_ = result->rowTimestampsMonotonic
        && rowTimestamps_
        && !rowTimestamps_->empty();
    invalidateWaveformCache();
    totalRows_ = result->totalRows;
    processedRows_ = result->processedRows;
    hasTimestampRange_ = result->hasTimestampRange;
    firstTimestamp_ = result->firstTimestamp;
    lastTimestamp_ = result->lastTimestamp;
    if (!hasTimestampRange_ || totalRows_ == 0) {
        firstTimestamp_ = 0;
        lastTimestamp_ = 0;
        hasTimestampRange_ = false;
    }
    statusText_ = result->statusText;
    building_ = false;
    rebuildContentMetrics();
    updateAutoColumnWidths();
    updateScrollBars();
    updateLaneSelectionFromFilter();
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

void TimelineWidget::postBuildProgress(const quint64 generation,
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

void TimelineWidget::updateBuildProgress(const quint64 generation,
                                         const std::uint64_t processedRows,
                                         const std::uint64_t totalRows)
{
    if (generation != buildGeneration_ || !building_) {
        return;
    }
    processedRows_ = processedRows;
    totalRows_ = totalRows;
    statusText_ = QStringLiteral("Timeline: grouping flits %1/%2")
                      .arg(processedRows_)
                      .arg(totalRows_);
    update();
}

bool TimelineWidget::hasPendingBuild() const noexcept
{
    return !building_
        && !buildRequested_
        && events_.empty()
        && ((traceSession_ && totalRows_ > 0) || (rowSnapshot_ && !rowSnapshot_->empty()));
}

bool TimelineWidget::shouldShowBuildPrompt() const noexcept
{
    return (hasPendingBuild() || (building_ && buildRequested_ && events_.empty()))
        && ((traceSession_ && totalRows_ > 0) || (rowSnapshot_ && !rowSnapshot_->empty()));
}

QString TimelineWidget::pendingBuildText() const
{
    const std::uint64_t rowCount = traceSession_
        ? traceSession_->totalRows()
        : (rowSnapshot_ ? rowSnapshot_->size() : 0);
    if (rowCount == 0) {
        return QStringLiteral("No flits in the current trace.");
    }
    return QStringLiteral("Timeline view is ready to build. Click here to group %1 rows.")
        .arg(FormatUnsignedIntegerWithThousands(rowCount));
}

QRect TimelineWidget::buildPromptButtonRect() const
{
    const QRect body = rect().adjusted(24, kHeaderHeight + kTopRulerHeight + 18, -24, -kFullRulerHeight - 24);
    const int buttonWidth = qMin(260, qMax(150, body.width() - 48));
    const int buttonHeight = 34;
    return QRect(body.center().x() - buttonWidth / 2,
                 body.center().y() + 20,
                 buttonWidth,
                 buttonHeight);
}

void TimelineWidget::paintBuildPrompt(QPainter& painter) const
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
        painter.drawText(button, Qt::AlignCenter, QStringLiteral("Build Timeline View"));
    }
    painter.restore();
}

void TimelineWidget::paintBuildProgress(QPainter& painter, const QRect& bounds) const
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

TimelineWidget::WaveformCacheKey TimelineWidget::currentWaveformCacheKey(const QRect& plot) const
{
    WaveformCacheKey key;
    key.dataGeneration = buildGeneration_.load();
    key.width = plot.width();
    key.height = plot.height();
    key.verticalScrollOffset = verticalScrollOffset_;
    key.totalRows = totalRows_;
    key.contentWidth = timelineContentWidth();
    key.scrollOffset = scrollOffset_;
    const auto [firstRow, lastRow] = visibleLogicalRowRange();
    key.firstRow = firstRow;
    key.lastRow = lastRow;
    const auto [firstTimestamp, lastTimestamp] = visibleTimeRange();
    key.firstTimestamp = firstTimestamp;
    key.lastTimestamp = lastTimestamp;
    key.firstLane = qMax(0, verticalScrollOffset_ / laneStride());
    key.lastLane = qMin(static_cast<int>(lanes_.size()) - 1,
                        (verticalScrollOffset_ + plot.height()) / laneStride() + 1);
    return key;
}

void TimelineWidget::scheduleWaveformRender(const WaveformCacheKey& key)
{
    if (waveformRenderPending_ && waveformPendingKey_ == key) {
        return;
    }

    if (waveformRenderThread_.joinable()) {
        waveformRenderThread_.request_stop();
    }

    waveformPendingKey_ = key;
    waveformRenderPending_ = true;
    const quint64 renderGeneration = ++waveformRenderGeneration_;
    const auto lanes = std::make_shared<const std::vector<TimelineLane>>(lanes_);
    const auto laneRows = laneRows_;
    const auto rowTimestamps = rowTimestamps_;
    const qint64 globalFirstTimestamp = firstTimestamp_;
    const qint64 globalLastTimestamp = lastTimestamp_;

    waveformRenderThread_ = std::jthread(
        [this,
         renderGeneration,
         key,
         lanes,
         laneRows,
         rowTimestamps,
         globalFirstTimestamp,
         globalLastTimestamp](const std::stop_token stopToken) {
            auto result = std::make_shared<WaveformRenderResult>();
            result->renderGeneration = renderGeneration;
            result->key = key;
            if (!laneRows || !rowTimestamps || key.width <= 0 || key.height <= 0 || key.lastLane < key.firstLane) {
                result->image = QImage(qMax(1, key.width), qMax(1, key.height), QImage::Format_ARGB32_Premultiplied);
                result->image.fill(Qt::transparent);
            } else {
                QImage image(key.width, key.height, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter imagePainter(&image);
                imagePainter.setRenderHint(QPainter::Antialiasing, false);
                imagePainter.setCompositionMode(QPainter::CompositionMode_Source);

                const std::uint64_t contentWidth = std::max<std::uint64_t>(1, key.contentWidth);
                const auto xForRow = [&](const std::uint64_t row) -> int {
                    if (row >= rowTimestamps->size()
                        || globalFirstTimestamp == globalLastTimestamp
                        || contentWidth <= 1) {
                        return 0;
                    }
                    const qint64 timestamp = (*rowTimestamps)[static_cast<std::size_t>(row)];
                    const qint64 clampedTimestamp = qBound(globalFirstTimestamp, timestamp, globalLastTimestamp);
                    const long double contentX =
                        ((static_cast<long double>(clampedTimestamp)
                          - static_cast<long double>(globalFirstTimestamp))
                         * static_cast<long double>(contentWidth - 1))
                        / (static_cast<long double>(globalLastTimestamp)
                           - static_cast<long double>(globalFirstTimestamp));
                    const auto rounded = static_cast<std::int64_t>(std::llround(contentX));
                    return static_cast<int>(rounded - static_cast<std::int64_t>(key.scrollOffset));
                };

                for (int laneIndex = key.firstLane; laneIndex <= key.lastLane; ++laneIndex) {
                    if (stopToken.stop_requested()) {
                        result->cancelled = true;
                        break;
                    }
                    if (laneIndex < 0 || laneIndex >= static_cast<int>(laneRows->size())
                        || laneIndex >= static_cast<int>(lanes->size())) {
                        continue;
                    }
                    const std::vector<std::uint64_t>& rows = (*laneRows)[static_cast<std::size_t>(laneIndex)];
                    if (rows.empty()) {
                        continue;
                    }

                    const int centerY = laneIndex * laneStride() - key.verticalScrollOffset + kLaneHeight / 2;
                    if (centerY < -kLaneHeight || centerY > key.height + kLaneHeight) {
                        continue;
                    }
                    const QColor color = eventColor((*lanes)[static_cast<std::size_t>(laneIndex)].channel,
                                                    (*lanes)[static_cast<std::size_t>(laneIndex)].direction);
                    imagePainter.setPen(QPen(color, 1));
                    imagePainter.setBrush(color);
                    int lastX = std::numeric_limits<int>::min();
                    int duplicateCount = 0;
                    auto lower = std::lower_bound(rows.cbegin(), rows.cend(), key.firstRow);
                    auto upper = std::upper_bound(rows.cbegin(), rows.cend(), key.lastRow);
                    for (auto it = lower; it != upper; ++it) {
                        if (*it >= rowTimestamps->size()) {
                            continue;
                        }
                        const qint64 timestamp = (*rowTimestamps)[static_cast<std::size_t>(*it)];
                        if (timestamp < key.firstTimestamp) {
                            continue;
                        }
                        if (timestamp > key.lastTimestamp) {
                            break;
                        }
                        const int x = xForRow(*it);
                        if (x < -2) {
                            continue;
                        }
                        if (x > key.width + 2) {
                            break;
                        }
                        if (x == lastX) {
                            ++duplicateCount;
                            if (duplicateCount > kMaxEventsPerPixelPerLane) {
                                continue;
                            }
                        } else {
                            lastX = x;
                            duplicateCount = 0;
                        }

                        const long double timeSpan =
                            static_cast<long double>(key.lastTimestamp)
                            - static_cast<long double>(key.firstTimestamp);
                        if (timeSpan > static_cast<long double>(key.width)
                            * kWaveformProgressivePixelThreshold) {
                            imagePainter.drawLine(x, centerY - 5, x, centerY + 5);
                        } else {
                            const QRect pulse(x - 1, centerY - 4, 3, 9);
                            imagePainter.drawRect(pulse);
                            imagePainter.drawLine(x, centerY - 6, x, centerY + 6);
                        }
                    }
                }
                imagePainter.end();
                result->image = std::move(image);
            }

            if (stopToken.stop_requested()) {
                return;
            }
            QTimer::singleShot(0, this, [this, result]() {
                applyWaveformRenderResult(result);
            });
        });
}

void TimelineWidget::applyWaveformRenderResult(std::shared_ptr<WaveformRenderResult> result)
{
    if (!result || result->cancelled
        || result->renderGeneration != waveformRenderGeneration_
        || result->key != waveformPendingKey_) {
        return;
    }
    waveformCacheImage_ = std::move(result->image);
    waveformCacheKey_ = result->key;
    waveformCacheValid_ = true;
    waveformRenderPending_ = false;
    update(plotRect());
}

void TimelineWidget::applyViewState(const QVariantMap& state)
{
    if (state.isEmpty()) {
        return;
    }

    if (state.contains(QStringLiteral("laneSortOrder"))) {
        const int rawOrder = state.value(QStringLiteral("laneSortOrder")).toInt();
        if (rawOrder >= static_cast<int>(TimelineLaneSortOrder::NodeThenChannel)
            && rawOrder <= static_cast<int>(TimelineLaneSortOrder::Custom)) {
            laneSortOrder_ = static_cast<TimelineLaneSortOrder>(rawOrder);
            applyLaneSortOrder();
        }
    }
    if (laneSortOrder_ == TimelineLaneSortOrder::Custom
        && state.contains(QStringLiteral("customLaneOrder"))
        && !lanes_.empty()) {
        const QStringList savedOrder = state.value(QStringLiteral("customLaneOrder")).toStringList();
        if (!savedOrder.isEmpty()) {
            QHash<QString, int> indexByKey;
            indexByKey.reserve(static_cast<int>(lanes_.size()));
            for (int index = 0; index < static_cast<int>(lanes_.size()); ++index) {
                if (const std::optional<LaneSelector> selector = laneSelectorForIndex(index)) {
                    indexByKey.insert(laneSelectorStateKey(*selector), index);
                }
            }

            std::vector<int> order;
            order.reserve(lanes_.size());
            std::vector<bool> used(lanes_.size(), false);
            for (const QString& key : savedOrder) {
                const auto found = indexByKey.constFind(key);
                if (found == indexByKey.cend()) {
                    continue;
                }
                const int index = found.value();
                if (index < 0 || index >= static_cast<int>(used.size()) || used[static_cast<std::size_t>(index)]) {
                    continue;
                }
                used[static_cast<std::size_t>(index)] = true;
                order.push_back(index);
            }
            for (int index = 0; index < static_cast<int>(lanes_.size()); ++index) {
                if (!used[static_cast<std::size_t>(index)]) {
                    order.push_back(index);
                }
            }
            if (order.size() == lanes_.size()) {
                applyLaneOrder(lanes_, events_, order);
                laneRows_ = std::make_shared<const std::vector<std::vector<std::uint64_t>>>(
                    buildLaneRows(lanes_, events_));
                invalidateWaveformCache();
            }
        }
    }

    if (state.contains(QStringLiteral("nodeColumnWidth"))) {
        nodeColumnWidth_ = std::clamp(state.value(QStringLiteral("nodeColumnWidth")).toInt(),
                                      kMinNodeColumnWidth,
                                      kMaxColumnWidth);
        nodeColumnUserSized_ = state.value(QStringLiteral("nodeColumnUserSized"), nodeColumnUserSized_).toBool();
    }
    if (state.contains(QStringLiteral("channelColumnWidth"))) {
        channelColumnWidth_ = std::clamp(state.value(QStringLiteral("channelColumnWidth")).toInt(),
                                         kMinChannelColumnWidth,
                                         kMaxColumnWidth);
        channelColumnUserSized_ =
            state.value(QStringLiteral("channelColumnUserSized"), channelColumnUserSized_).toBool();
    }

    horizontalZoom_ = std::clamp(state.value(QStringLiteral("horizontalZoom"), horizontalZoom_).toDouble(),
                                 kMinZoom,
                                 kMaxZoom);
    verticalScrollOffset_ = state.value(QStringLiteral("verticalScrollOffset"), verticalScrollOffset_).toInt();
    rebuildContentMetrics();
    bool restoredHorizontalRange = false;
    if (state.contains(QStringLiteral("visibleFirstTimestamp"))
        && state.contains(QStringLiteral("visibleLastTimestamp"))
        && state.value(QStringLiteral("hasTimestampRange"), true).toBool()) {
        restoredHorizontalRange =
            restoreVisibleTimeRange(state.value(QStringLiteral("visibleFirstTimestamp")).toLongLong(),
                                    state.value(QStringLiteral("visibleLastTimestamp")).toLongLong());
    }
    if (!restoredHorizontalRange
        && state.contains(QStringLiteral("visibleFirstLogicalRow"))
        && state.contains(QStringLiteral("visibleLastLogicalRow"))) {
        restoredHorizontalRange = restoreVisibleLogicalRange(
            static_cast<std::uint64_t>(state.value(QStringLiteral("visibleFirstLogicalRow")).toULongLong()),
            static_cast<std::uint64_t>(state.value(QStringLiteral("visibleLastLogicalRow")).toULongLong()));
    }
    if (!restoredHorizontalRange) {
        scrollToHorizontalOffset(static_cast<std::uint64_t>(
            state.value(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(0)).toULongLong()));
    }

    selectedLogicalRow_ = state.value(QStringLiteral("selectedLogicalRow"), selectedLogicalRow_).toInt();
    markerLogicalRow_ = state.value(QStringLiteral("markerLogicalRow"), markerLogicalRow_).toInt();
    selectedLane_ = state.value(QStringLiteral("selectedLane"), selectedLane_).toInt();
    const int maxLogicalRow = totalRows_ == 0
        ? -1
        : static_cast<int>(std::min<std::uint64_t>(
            totalRows_ - 1,
            static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
    selectedLogicalRow_ = qBound(-1, selectedLogicalRow_, maxLogicalRow);
    markerLogicalRow_ = qBound(-1, markerLogicalRow_, maxLogicalRow);
    selectedLane_ = qBound(-1, selectedLane_, lanes_.empty() ? -1 : static_cast<int>(lanes_.size()) - 1);
    updateLaneSelectionFromFilter();
    requestWaveformRerender();
    updateScrollBars();
    update();
}

void TimelineWidget::rebuildContentMetrics()
{
    contentWidth_ = timelineContentWidth();
    contentHeight_ = timelineContentHeight();
    if (scrollOffset_ > horizontalContentRange()) {
        scrollOffset_ = horizontalContentRange();
    }
    const int maxVertical = qMax(0, contentHeight_ - plotRect().height());
    verticalScrollOffset_ = qBound(0, verticalScrollOffset_, maxVertical);
}

void TimelineWidget::updateScrollBars()
{
    updateScrollBarGeometry();
    const std::uint64_t maxHorizontal = horizontalContentRange();
    {
        const QSignalBlocker blocker(horizontalScrollBar_);
        horizontalScrollBar_->setRange(0, maxHorizontal == 0 ? 0 : kMaxScrollBarRange);
        horizontalScrollBar_->setPageStep(qMax(1, scaledScrollBarValue(timelineViewportWidth(), contentWidth_)));
        horizontalScrollBar_->setValue(scaledScrollBarValue(scrollOffset_, maxHorizontal));
        horizontalScrollBar_->setVisible(maxHorizontal > 0);
    }

    const int maxVertical = qMax(0, contentHeight_ - plotRect().height());
    {
        const QSignalBlocker blocker(verticalScrollBar_);
        verticalScrollBar_->setRange(0, maxVertical);
        verticalScrollBar_->setPageStep(qMax(1, plotRect().height()));
        verticalScrollBar_->setSingleStep(kLaneStride);
        verticalScrollBar_->setValue(qBound(0, verticalScrollOffset_, maxVertical));
        verticalScrollBar_->setVisible(maxVertical > 0);
    }
}

void TimelineWidget::updateScrollBarGeometry()
{
    if (horizontalScrollBar_) {
        horizontalScrollBar_->setGeometry(horizontalScrollRect());
    }
    if (verticalScrollBar_) {
        verticalScrollBar_->setGeometry(verticalScrollRect());
    }
}

bool TimelineWidget::updateAutoColumnWidths()
{
    bool changed = false;
    if (!nodeColumnUserSized_) {
        const int width = requiredNodeColumnWidth();
        if (nodeColumnWidth_ != width) {
            nodeColumnWidth_ = width;
            changed = true;
        }
    }
    if (!channelColumnUserSized_) {
        const int width = requiredChannelColumnWidth();
        if (channelColumnWidth_ != width) {
            channelColumnWidth_ = width;
            changed = true;
        }
    }
    if (changed) {
        rebuildContentMetrics();
        updateScrollBars();
    }
    return changed;
}

int TimelineWidget::requiredNodeColumnWidth() const
{
    QFontMetrics metrics(font());
    int width = metrics.horizontalAdvance(QStringLiteral("Node")) + 2 * kHeaderPadding;
    for (const TimelineLane& lane : lanes_) {
        const int badgeWidth = ChannelBadgePreferredCellWidth(metrics,
                                                              lane.nodeTypeLabel,
                                                              QString());
        width = qMax(width, kHeaderPadding + kNodeNumberAreaWidth + 6 + badgeWidth);
    }
    return qBound(kMinNodeColumnWidth, width, kMaxColumnWidth);
}

int TimelineWidget::requiredChannelColumnWidth() const
{
    QFont badgeFont = ChannelBadgeScaledFont(font(), 0.92);
    QFontMetrics metrics(badgeFont);
    int width = metrics.horizontalAdvance(QStringLiteral("Channel")) + 2 * kHeaderPadding;
    for (const TimelineLane& lane : lanes_) {
        width = qMax(width,
                     ChannelBadgePreferredCellWidth(metrics, lane.channelLabel, QString()));
    }
    return qBound(kMinChannelColumnWidth, width, kMaxColumnWidth);
}

void TimelineWidget::setColumnWidth(const ResizeColumn column, const int width, const bool userSized)
{
    if (column == ResizeColumn::Node) {
        nodeColumnWidth_ = qBound(kMinNodeColumnWidth, width, kMaxColumnWidth);
        nodeColumnUserSized_ = userSized;
    } else if (column == ResizeColumn::Channel) {
        channelColumnWidth_ = qBound(kMinChannelColumnWidth, width, kMaxColumnWidth);
        channelColumnUserSized_ = userSized;
    }
    rebuildContentMetrics();
    updateScrollBars();
    update();
}

TimelineWidget::ResizeColumn TimelineWidget::columnResizeHandleAt(const QPoint& position) const
{
    if (position.y() > plotRect().bottom()) {
        return ResizeColumn::None;
    }
    const int nodeEdge = nodeColumnWidth_;
    if (std::abs(position.x() - nodeEdge) <= kResizeGripWidth) {
        return ResizeColumn::Node;
    }
    const int channelEdge = nodeColumnWidth_ + channelColumnWidth_;
    if (std::abs(position.x() - channelEdge) <= kResizeGripWidth) {
        return ResizeColumn::Channel;
    }
    return ResizeColumn::None;
}

void TimelineWidget::updateResizeCursor(const QPoint& position)
{
    if (dragMode_ == DragMode::None && columnResizeHandleAt(position) != ResizeColumn::None) {
        setCursor(Qt::SplitHCursor);
    } else if (dragMode_ == DragMode::None) {
        unsetCursor();
    }
}

QRect TimelineWidget::headerRect() const
{
    return QRect(0, 0, width(), kHeaderHeight);
}

QRect TimelineWidget::topRulerRect() const
{
    return QRect(timelineLeft(),
                 kHeaderHeight,
                 qMax(1, width() - timelineLeft() - kRightMargin - kScrollBarWidth),
                 kTopRulerHeight);
}

QRect TimelineWidget::plotRect() const
{
    const int left = timelineLeft();
    const int top = kHeaderHeight + kTopRulerHeight;
    const int bottomReserved = kFullRulerHeight + kScrollBarHeight;
    return QRect(left,
                 top,
                 qMax(1, width() - left - kRightMargin - kScrollBarWidth),
                 qMax(1, height() - top - bottomReserved));
}

QRect TimelineWidget::fullRulerRect() const
{
    const QRect plot = plotRect();
    return QRect(plot.left(), plot.bottom() + 1, plot.width(), kFullRulerHeight);
}

QRect TimelineWidget::horizontalScrollRect() const
{
    const QRect full = fullRulerRect();
    return QRect(full.left(), full.bottom() + 1, full.width(), kScrollBarHeight);
}

QRect TimelineWidget::verticalScrollRect() const
{
    const QRect plot = plotRect();
    return QRect(plot.right() + 1,
                 plot.top(),
                 kScrollBarWidth,
                 plot.height() + kFullRulerHeight);
}

int TimelineWidget::timelineLeft() const
{
    return nodeColumnWidth_ + channelColumnWidth_;
}

int TimelineWidget::timelineViewportWidth() const
{
    return qMax(1, plotRect().width());
}

int TimelineWidget::laneStride() const
{
    return kLaneStride;
}

int TimelineWidget::timelineContentHeight() const
{
    return qMax(1, static_cast<int>(lanes_.size()) * laneStride());
}

QRect TimelineWidget::nodeHeaderRect() const
{
    return QRect(0, 0, nodeColumnWidth_, headerRect().height());
}

QRect TimelineWidget::channelHeaderRect() const
{
    return QRect(nodeColumnWidth_, 0, channelColumnWidth_, headerRect().height());
}

std::uint64_t TimelineWidget::timelineContentWidth() const
{
    const int viewport = timelineViewportWidth();
    if (!hasTimestampRange_ || firstTimestamp_ == lastTimestamp_) {
        return static_cast<std::uint64_t>(viewport);
    }
    const long double width = static_cast<long double>(viewport) * horizontalZoom_;
    const long double clamped = std::min<long double>(
        width,
        static_cast<long double>(std::numeric_limits<std::uint64_t>::max() / 4U));
    return qMax<std::uint64_t>(static_cast<std::uint64_t>(std::llround(clamped)),
                               static_cast<std::uint64_t>(viewport));
}

std::uint64_t TimelineWidget::horizontalContentRange() const
{
    const std::uint64_t contentWidth = timelineContentWidth();
    const std::uint64_t viewport = static_cast<std::uint64_t>(timelineViewportWidth());
    return contentWidth > viewport ? contentWidth - viewport : 0;
}

std::uint64_t TimelineWidget::contentXForTimestamp(const qint64 timestamp) const
{
    const std::uint64_t contentWidth = timelineContentWidth();
    if (!hasTimestampRange_ || firstTimestamp_ == lastTimestamp_ || contentWidth <= 1) {
        return 0;
    }

    const qint64 clampedTimestamp = qBound(firstTimestamp_, timestamp, lastTimestamp_);
    const long double numerator = static_cast<long double>(clampedTimestamp)
        - static_cast<long double>(firstTimestamp_);
    const long double denominator = static_cast<long double>(lastTimestamp_)
        - static_cast<long double>(firstTimestamp_);
    return static_cast<std::uint64_t>(std::llround(
        std::clamp<long double>(numerator / denominator, 0.0L, 1.0L)
        * static_cast<long double>(contentWidth - 1)));
}

qint64 TimelineWidget::timestampForContentX(const std::uint64_t contentX) const
{
    const std::uint64_t contentWidth = timelineContentWidth();
    if (!hasTimestampRange_ || firstTimestamp_ == lastTimestamp_ || contentWidth <= 1) {
        return firstTimestamp_;
    }

    const std::uint64_t x = std::min<std::uint64_t>(contentX, contentWidth - 1);
    const long double ratio = static_cast<long double>(x)
        / static_cast<long double>(contentWidth - 1);
    const long double timestamp = static_cast<long double>(firstTimestamp_)
        + ratio * (static_cast<long double>(lastTimestamp_) - static_cast<long double>(firstTimestamp_));
    const long double clamped = std::clamp<long double>(
        timestamp,
        static_cast<long double>((std::numeric_limits<qint64>::min)()),
        static_cast<long double>((std::numeric_limits<qint64>::max)()));
    return static_cast<qint64>(std::llround(clamped));
}

std::uint64_t TimelineWidget::contentXForLogicalRow(const std::uint64_t logicalRow) const
{
    if (const auto timestamp = timestampForLogicalRow(logicalRow)) {
        return contentXForTimestamp(*timestamp);
    }

    const std::uint64_t contentWidth = timelineContentWidth();
    if (totalRows_ <= 1 || contentWidth <= 1) {
        return 0;
    }
    const std::uint64_t row = std::min<std::uint64_t>(logicalRow, totalRows_ - 1);
    const long double numerator = static_cast<long double>(row)
        * static_cast<long double>(contentWidth - 1);
    const long double denominator = static_cast<long double>(totalRows_ - 1);
    return static_cast<std::uint64_t>(std::llround(numerator / denominator));
}

std::uint64_t TimelineWidget::logicalRowForTimestamp(const qint64 timestamp) const
{
    if (totalRows_ == 0) {
        return 0;
    }
    if (rowTimestampsMonotonic_ && rowTimestamps_ && !rowTimestamps_->empty()) {
        const auto begin = rowTimestamps_->cbegin();
        const auto end = rowTimestamps_->cend();
        const auto found = std::lower_bound(begin, end, timestamp);
        if (found == begin) {
            return 0;
        }
        if (found == end) {
            return std::min<std::uint64_t>(
                totalRows_ - 1,
                static_cast<std::uint64_t>(rowTimestamps_->size() - 1));
        }

        const auto previous = std::prev(found);
        const long double previousDistance = std::abs(static_cast<long double>(timestamp)
                                                      - static_cast<long double>(*previous));
        const long double nextDistance = std::abs(static_cast<long double>(*found)
                                                  - static_cast<long double>(timestamp));
        const auto chosen = nextDistance < previousDistance ? found : previous;
        return std::min<std::uint64_t>(
            totalRows_ - 1,
            static_cast<std::uint64_t>(std::distance(begin, chosen)));
    }

    if (events_.empty()) {
        return 0;
    }

    const TimelineEvent* best = nullptr;
    long double bestDistance = (std::numeric_limits<long double>::max)();
    for (const TimelineEvent& event : events_) {
        const long double distance = std::abs(static_cast<long double>(event.timestamp)
                                              - static_cast<long double>(timestamp));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = &event;
        }
    }
    return best ? best->logicalRow : 0;
}

std::uint64_t TimelineWidget::firstLogicalRowAtOrAfterTimestamp(const qint64 timestamp) const
{
    if (totalRows_ == 0) {
        return 0;
    }
    if (rowTimestampsMonotonic_ && rowTimestamps_ && !rowTimestamps_->empty()) {
        const auto begin = rowTimestamps_->cbegin();
        const auto found = std::lower_bound(begin, rowTimestamps_->cend(), timestamp);
        if (found == rowTimestamps_->cend()) {
            return std::min<std::uint64_t>(
                totalRows_ - 1,
                static_cast<std::uint64_t>(rowTimestamps_->size() - 1));
        }
        return std::min<std::uint64_t>(
            totalRows_ - 1,
            static_cast<std::uint64_t>(std::distance(begin, found)));
    }
    std::uint64_t bestRow = 0;
    qint64 bestTimestamp = (std::numeric_limits<qint64>::max)();
    bool found = false;
    for (const TimelineEvent& event : events_) {
        if (event.timestamp >= timestamp
            && (!found || event.timestamp < bestTimestamp
                || (event.timestamp == bestTimestamp && event.logicalRow < bestRow))) {
            bestTimestamp = event.timestamp;
            bestRow = event.logicalRow;
            found = true;
        }
    }
    if (found) {
        return bestRow;
    }
    return events_.empty() ? 0 : events_.back().logicalRow;
}

std::uint64_t TimelineWidget::lastLogicalRowAtOrBeforeTimestamp(const qint64 timestamp) const
{
    if (totalRows_ == 0) {
        return 0;
    }
    if (rowTimestampsMonotonic_ && rowTimestamps_ && !rowTimestamps_->empty()) {
        const auto begin = rowTimestamps_->cbegin();
        const auto found = std::upper_bound(begin, rowTimestamps_->cend(), timestamp);
        if (found == begin) {
            return 0;
        }
        return std::min<std::uint64_t>(
            totalRows_ - 1,
            static_cast<std::uint64_t>(std::distance(begin, std::prev(found))));
    }
    std::uint64_t bestRow = 0;
    qint64 bestTimestamp = (std::numeric_limits<qint64>::min)();
    bool found = false;
    for (const TimelineEvent& event : events_) {
        if (event.timestamp <= timestamp
            && (!found || event.timestamp > bestTimestamp
                || (event.timestamp == bestTimestamp && event.logicalRow > bestRow))) {
            bestTimestamp = event.timestamp;
            bestRow = event.logicalRow;
            found = true;
        }
    }
    if (found) {
        return bestRow;
    }
    return 0;
}

std::uint64_t TimelineWidget::logicalRowForContentX(const std::uint64_t contentX) const
{
    if (hasTimestampRange_) {
        return logicalRowForTimestamp(timestampForContentX(contentX));
    }

    const std::uint64_t contentWidth = timelineContentWidth();
    if (totalRows_ <= 1 || contentWidth <= 1) {
        return 0;
    }
    const std::uint64_t x = std::min<std::uint64_t>(contentX, contentWidth - 1);
    const long double numerator = static_cast<long double>(x)
        * static_cast<long double>(totalRows_ - 1);
    const long double denominator = static_cast<long double>(contentWidth - 1);
    return std::min<std::uint64_t>(
        totalRows_ - 1,
        static_cast<std::uint64_t>(std::llround(numerator / denominator)));
}

std::uint64_t TimelineWidget::contentXAtTimelineX(const int x) const
{
    const QRect plot = plotRect();
    const int clampedX = qBound(plot.left(), x, plot.right());
    return scrollOffset_ + static_cast<std::uint64_t>(clampedX - plot.left());
}

qint64 TimelineWidget::timestampAtTimelineX(const int x) const
{
    return timestampForContentX(contentXAtTimelineX(x));
}

qint64 TimelineWidget::timestampAtFullRulerX(const int x) const
{
    const QRect full = fullRulerRect();
    if (!hasTimestampRange_ || firstTimestamp_ == lastTimestamp_) {
        return firstTimestamp_;
    }
    const int clampedX = qBound(full.left(), x, full.right());
    const long double ratio = static_cast<long double>(clampedX - full.left())
        / static_cast<long double>(qMax(1, full.width() - 1));
    const long double timestamp = static_cast<long double>(firstTimestamp_)
        + std::clamp<long double>(ratio, 0.0L, 1.0L)
            * (static_cast<long double>(lastTimestamp_) - static_cast<long double>(firstTimestamp_));
    return static_cast<qint64>(std::llround(timestamp));
}

std::uint64_t TimelineWidget::logicalRowAtTimelineX(const int x) const
{
    return logicalRowForContentX(contentXAtTimelineX(x));
}

std::optional<int> TimelineWidget::screenXForTimestamp(const qint64 timestamp,
                                                       const QRect& area) const
{
    if (!hasTimestampRange_ || area.width() <= 0) {
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

std::optional<int> TimelineWidget::screenXForLogicalRow(const std::uint64_t logicalRow,
                                                        const QRect& area) const
{
    if (totalRows_ == 0 || area.width() <= 0) {
        return std::nullopt;
    }
    const std::uint64_t contentX = contentXForLogicalRow(logicalRow);
    if (contentX < scrollOffset_) {
        return std::nullopt;
    }
    const std::uint64_t local = contentX - scrollOffset_;
    if (local > static_cast<std::uint64_t>(area.width() - 1)) {
        return std::nullopt;
    }
    return area.left() + static_cast<int>(local);
}

std::pair<qint64, qint64> TimelineWidget::visibleTimeRange() const
{
    if (!hasTimestampRange_) {
        return {0, 0};
    }
    const QRect plot = plotRect();
    const qint64 first = timestampForContentX(scrollOffset_);
    const qint64 last = timestampForContentX(scrollOffset_
                                             + static_cast<std::uint64_t>(qMax(0, plot.width() - 1)));
    return {std::min(first, last), std::max(first, last)};
}

std::pair<std::uint64_t, std::uint64_t> TimelineWidget::visibleLogicalRowRange() const
{
    if (totalRows_ == 0) {
        return {0, 0};
    }
    if (hasTimestampRange_ && rowTimestampsMonotonic_ && rowTimestamps_ && !rowTimestamps_->empty()) {
        const auto [firstTimestamp, lastTimestamp] = visibleTimeRange();
        const auto begin = rowTimestamps_->cbegin();
        const auto firstIt = std::lower_bound(begin, rowTimestamps_->cend(), firstTimestamp);
        const auto lastIt = std::upper_bound(begin, rowTimestamps_->cend(), lastTimestamp);
        if (firstIt != rowTimestamps_->cend() && firstIt < lastIt) {
            return {
                std::min<std::uint64_t>(totalRows_ - 1,
                                        static_cast<std::uint64_t>(std::distance(begin, firstIt))),
                std::min<std::uint64_t>(totalRows_ - 1,
                                        static_cast<std::uint64_t>(std::distance(begin, std::prev(lastIt))))
            };
        }
        const std::uint64_t nearest = logicalRowForTimestamp(
            firstTimestamp + static_cast<qint64>(
                (static_cast<long double>(lastTimestamp) - static_cast<long double>(firstTimestamp)) / 2.0L));
        return {nearest, nearest};
    }

    const QRect plot = plotRect();
    const std::uint64_t first = logicalRowForContentX(scrollOffset_);
    const std::uint64_t last = logicalRowForContentX(scrollOffset_
                                                     + static_cast<std::uint64_t>(qMax(0, plot.width() - 1)));
    return {first, std::max(first, last)};
}

QString TimelineWidget::visibleRangeText() const
{
    if (totalRows_ == 0) {
        return QStringLiteral("No timeline");
    }
    const auto [first, last] = visibleLogicalRowRange();
    QString text = QStringLiteral("Rows %1 - %2 of %3   Zoom %4x")
                       .arg(first + 1)
                       .arg(last + 1)
                       .arg(totalRows_)
                       .arg(horizontalZoom_, 0, 'f', horizontalZoom_ < 10.0 ? 2 : 1);
    if (hasTimestampRange_) {
        const auto [firstTs, lastTs] = visibleTimeRange();
        text += QStringLiteral("   Time %1 - %2")
                    .arg(FormatTimestampForDisplay(firstTs),
                         FormatTimestampForDisplay(lastTs));
    }
    if (markerLogicalRow_ >= 0 && selectedLogicalRow_ >= 0) {
        text += QStringLiteral("   Δrow %1")
                    .arg(std::abs(markerLogicalRow_ - selectedLogicalRow_));
    }
    return text;
}

QString TimelineWidget::rowLabel(const std::uint64_t logicalRow) const
{
    if (const auto timestamp = timestampForLogicalRow(logicalRow)) {
        return QStringLiteral("%1 / %2")
            .arg(logicalRow + 1)
            .arg(FormatTimestampForDisplay(*timestamp));
    }
    return QString::number(logicalRow + 1);
}

QString rulerRowLabel(const std::uint64_t logicalRow)
{
    return QStringLiteral("R%1").arg(logicalRow + 1);
}

QString rulerTimeLabel(const qint64 timestamp)
{
    return QStringLiteral("T%1").arg(FormatTimestampForDisplay(timestamp));
}

std::optional<qint64> TimelineWidget::timestampForLogicalRow(const std::uint64_t logicalRow) const
{
    if (rowTimestamps_ && logicalRow < rowTimestamps_->size()) {
        return (*rowTimestamps_)[static_cast<std::size_t>(logicalRow)];
    }
    if (events_.empty()) {
        return std::nullopt;
    }
    const auto found = lowerBoundEventByLogicalRow(events_, logicalRow);
    if (found != events_.cend() && found->logicalRow == logicalRow) {
        return found->timestamp;
    }
    if (found == events_.cbegin()) {
        return found != events_.cend() ? std::optional<qint64>(found->timestamp) : std::nullopt;
    }
    const auto previous = std::prev(found);
    return previous->timestamp;
}

void TimelineWidget::zoomToFit()
{
    pushViewHistory();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    requestWaveformRerender();
    rebuildContentMetrics();
    updateScrollBars();
    update();
}

void TimelineWidget::zoomByFactor(const double factor, const QPoint& focus)
{
    if (!hasTimestampRange_ || firstTimestamp_ == lastTimestamp_ || factor <= 0.0) {
        return;
    }

    pushViewHistory();
    const QRect plot = plotRect();
    const int focusX = qBound(plot.left(), focus.x(), plot.right());
    const std::uint64_t focusContentBefore =
        scrollOffset_ + static_cast<std::uint64_t>(focusX - plot.left());
    const qint64 focusTimestamp = timestampForContentX(focusContentBefore);

    horizontalZoom_ = std::clamp(horizontalZoom_ * factor, kMinZoom, kMaxZoom);
    requestWaveformRerender();
    rebuildContentMetrics();
    const std::uint64_t focusContentAfter = contentXForTimestamp(focusTimestamp);
    if (focusContentAfter > static_cast<std::uint64_t>(focusX - plot.left())) {
        scrollToHorizontalOffset(focusContentAfter - static_cast<std::uint64_t>(focusX - plot.left()));
    } else {
        scrollToHorizontalOffset(0);
    }
    updateScrollBars();
    update();
}

void TimelineWidget::zoomToTimeRange(qint64 firstTimestamp, qint64 lastTimestamp, const bool moveCursor)
{
    if (!hasTimestampRange_ || firstTimestamp_ == lastTimestamp_) {
        return;
    }
    if (firstTimestamp > lastTimestamp) {
        std::swap(firstTimestamp, lastTimestamp);
    }
    firstTimestamp = qBound(firstTimestamp_, firstTimestamp, lastTimestamp_);
    lastTimestamp = qBound(firstTimestamp_, lastTimestamp, lastTimestamp_);
    if (firstTimestamp == lastTimestamp) {
        return;
    }

    pushViewHistory();
    const long double totalSpan = static_cast<long double>(lastTimestamp_)
        - static_cast<long double>(firstTimestamp_);
    const long double requestedSpan = static_cast<long double>(lastTimestamp)
        - static_cast<long double>(firstTimestamp);
    const double requestedZoom = requestedSpan <= 0.0L
        ? kMaxZoom
        : static_cast<double>(totalSpan / requestedSpan);
    horizontalZoom_ = std::clamp(requestedZoom, kMinZoom, kMaxZoom);
    requestWaveformRerender();
    rebuildContentMetrics();
    scrollToHorizontalOffset(contentXForTimestamp(firstTimestamp));
    if (moveCursor) {
        setCursorLogicalRow(static_cast<int>(firstLogicalRowAtOrAfterTimestamp(firstTimestamp)), false);
    }
}

void TimelineWidget::zoomToLogicalRange(std::uint64_t firstRow, std::uint64_t lastRow, const bool moveCursor)
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
    if (firstTimestamp && lastTimestamp && *firstTimestamp != *lastTimestamp) {
        zoomToTimeRange(*firstTimestamp, *lastTimestamp, moveCursor);
        return;
    }

    pushViewHistory();
    const double spanRows = static_cast<double>(lastRow - firstRow + 1);
    const double requestedZoom = static_cast<double>(totalRows_) / spanRows;
    horizontalZoom_ = std::clamp(requestedZoom, kMinZoom, kMaxZoom);
    requestWaveformRerender();
    rebuildContentMetrics();
    scrollToHorizontalOffset(contentXForLogicalRow(firstRow));
    if (moveCursor) {
        setCursorLogicalRow(static_cast<int>(firstRow), false);
    }
}

bool TimelineWidget::restoreVisibleTimeRange(qint64 firstTimestamp, qint64 lastTimestamp)
{
    if (!hasTimestampRange_ || firstTimestamp_ == lastTimestamp_) {
        return false;
    }
    if (firstTimestamp > lastTimestamp) {
        std::swap(firstTimestamp, lastTimestamp);
    }
    if (firstTimestamp < firstTimestamp_ || lastTimestamp > lastTimestamp_) {
        return false;
    }
    firstTimestamp = qBound(firstTimestamp_, firstTimestamp, lastTimestamp_);
    lastTimestamp = qBound(firstTimestamp_, lastTimestamp, lastTimestamp_);
    if (firstTimestamp >= lastTimestamp) {
        return false;
    }

    const long double totalSpan = static_cast<long double>(lastTimestamp_)
        - static_cast<long double>(firstTimestamp_);
    const long double requestedSpan = static_cast<long double>(lastTimestamp)
        - static_cast<long double>(firstTimestamp);
    if (totalSpan <= 0.0L || requestedSpan <= 0.0L) {
        return false;
    }

    horizontalZoom_ = std::clamp(static_cast<double>(totalSpan / requestedSpan),
                                 kMinZoom,
                                 kMaxZoom);
    requestWaveformRerender();
    rebuildContentMetrics();
    scrollToHorizontalOffset(contentXForTimestamp(firstTimestamp));
    return true;
}

bool TimelineWidget::restoreVisibleLogicalRange(std::uint64_t firstRow, std::uint64_t lastRow)
{
    if (totalRows_ <= 1) {
        return false;
    }
    if (firstRow > lastRow) {
        std::swap(firstRow, lastRow);
    }
    firstRow = std::min<std::uint64_t>(firstRow, totalRows_ - 1);
    lastRow = std::min<std::uint64_t>(lastRow, totalRows_ - 1);
    if (firstRow >= lastRow) {
        return false;
    }

    const auto firstTimestamp = timestampForLogicalRow(firstRow);
    const auto lastTimestamp = timestampForLogicalRow(lastRow);
    if (firstTimestamp && lastTimestamp && *firstTimestamp != *lastTimestamp) {
        return restoreVisibleTimeRange(*firstTimestamp, *lastTimestamp);
    }

    const double spanRows = static_cast<double>(lastRow - firstRow + 1);
    if (spanRows <= 0.0) {
        return false;
    }
    horizontalZoom_ = std::clamp(static_cast<double>(totalRows_) / spanRows,
                                 kMinZoom,
                                 kMaxZoom);
    requestWaveformRerender();
    rebuildContentMetrics();
    scrollToHorizontalOffset(contentXForLogicalRow(firstRow));
    return true;
}

void TimelineWidget::pushViewHistory()
{
    if (!viewHistory_.empty()) {
        const ViewState& last = viewHistory_.back();
        if (std::abs(last.zoom - horizontalZoom_) < 0.0001 && last.offset == scrollOffset_) {
            return;
        }
    }
    viewHistory_.push_back(ViewState{horizontalZoom_, scrollOffset_});
    if (viewHistory_.size() > 32) {
        viewHistory_.erase(viewHistory_.begin());
    }
}

void TimelineWidget::restorePreviousView()
{
    if (viewHistory_.empty()) {
        return;
    }
    const ViewState state = viewHistory_.back();
    viewHistory_.pop_back();
    horizontalZoom_ = std::clamp(state.zoom, kMinZoom, kMaxZoom);
    requestWaveformRerender();
    rebuildContentMetrics();
    scrollToHorizontalOffset(state.offset);
    update();
}

void TimelineWidget::scrollToHorizontalOffset(const std::uint64_t offset)
{
    scrollOffset_ = std::min<std::uint64_t>(offset, horizontalContentRange());
    requestWaveformRerender();
    updateScrollBars();
    update();
}

void TimelineWidget::panHorizontallyByPixels(const int pixels)
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
    requestWaveformRerender();
    updateScrollBars();
    update();
}

void TimelineWidget::panVerticallyByPixels(const int pixels)
{
    const int maxVertical = qMax(0, contentHeight_ - plotRect().height());
    verticalScrollOffset_ = qBound(0, verticalScrollOffset_ + pixels, maxVertical);
    requestWaveformRerender();
    updateScrollBars();
    update();
}

void TimelineWidget::ensureLogicalRowVisible(const std::uint64_t logicalRow)
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

void TimelineWidget::centerSelectedLogicalRow()
{
    if (selectedLogicalRow_ < 0) {
        return;
    }
    const std::uint64_t contentX = contentXForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
    const std::uint64_t half = static_cast<std::uint64_t>(timelineViewportWidth() / 2);
    scrollToHorizontalOffset(contentX > half ? contentX - half : 0);
}

void TimelineWidget::moveCursorToAdjacentEvent(const int direction)
{
    if (events_.empty()) {
        return;
    }
    const std::uint64_t current = selectedLogicalRow_ < 0
        ? (direction >= 0 ? 0 : totalRows_ - 1)
        : static_cast<std::uint64_t>(selectedLogicalRow_);
    std::optional<int> next;
    if (direction >= 0) {
        for (const TimelineEvent& event : events_) {
            if (event.logicalRow > current) {
                next = static_cast<int>(event.logicalRow);
                break;
            }
        }
    } else {
        for (auto it = events_.crbegin(); it != events_.crend(); ++it) {
            if (it->logicalRow < current) {
                next = static_cast<int>(it->logicalRow);
                break;
            }
        }
    }
    if (next) {
        setCursorLogicalRow(*next, true);
    }
}

void TimelineWidget::setCursorLogicalRow(const int logicalRow, const bool emitActivation)
{
    if (totalRows_ == 0) {
        selectedLogicalRow_ = -1;
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
    if (lockCursorMarkerDelta_ && markerLogicalRow_ >= 0) {
        markerLogicalRow_ = qBound(0,
                                   selectedLogicalRow_ + delta,
                                   static_cast<int>(std::min<std::uint64_t>(
                                       totalRows_ - 1,
                                       static_cast<std::uint64_t>(std::numeric_limits<int>::max()))));
    }
    ensureLogicalRowVisible(static_cast<std::uint64_t>(selectedLogicalRow_));
    updateLaneSelectionFromFilter();
    update();
    if (emitActivation) {
        Q_EMIT logicalRowActivated(selectedLogicalRow_);
    }
}

void TimelineWidget::setMarkerLogicalRow(const int logicalRow)
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

void TimelineWidget::updateLaneSelectionFromFilter()
{
    highlightedLane_ = selectedLaneFilter_ ? laneIndexFor(*selectedLaneFilter_) : -1;
    if (highlightedLane_ >= 0) {
        selectedLane_ = highlightedLane_;
        const QRect plot = plotRect();
        const int laneTop = highlightedLane_ * laneStride();
        if (laneTop < verticalScrollOffset_) {
            verticalScrollOffset_ = laneTop;
        } else if (laneTop + kLaneHeight > verticalScrollOffset_ + plot.height()) {
            verticalScrollOffset_ = qMax(0, laneTop + kLaneHeight - plot.height());
        }
        updateScrollBars();
    }
}

std::optional<TimelineWidget::LaneSelector> TimelineWidget::laneSelectorForIndex(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return std::nullopt;
    }
    const TimelineLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    LaneSelector selector;
    selector.direction = lane.direction;
    selector.channel = lane.channel;
    if (lane.nodeId != kInvalidNodeId) {
        selector.nodeId = lane.nodeId;
    }
    return selector;
}

QString TimelineWidget::laneSelectorStateKey(const LaneSelector& selector)
{
    return QStringLiteral("%1|%2|%3")
        .arg(static_cast<int>(selector.direction))
        .arg(static_cast<int>(selector.channel))
        .arg(selector.nodeId ? QString::number(*selector.nodeId) : QStringLiteral("-"));
}

QStringList TimelineWidget::laneStateOrder() const
{
    QStringList order;
    order.reserve(static_cast<int>(lanes_.size()));
    for (int index = 0; index < static_cast<int>(lanes_.size()); ++index) {
        if (const std::optional<LaneSelector> selector = laneSelectorForIndex(index)) {
            order.append(laneSelectorStateKey(*selector));
        }
    }
    return order;
}

void TimelineWidget::resetCustomLaneSortOrder()
{
    if (laneSortOrder_ == TimelineLaneSortOrder::Custom) {
        laneSortOrder_ = TimelineLaneSortOrder::NodeThenChannel;
    }
}

void TimelineWidget::setLaneSortOrder(const TimelineLaneSortOrder order)
{
    if (laneSortOrder_ == order) {
        return;
    }
    laneSortOrder_ = order;
    applyLaneSortOrder();
}

void TimelineWidget::applyLaneSortOrder()
{
    if (lanes_.empty()) {
        update();
        return;
    }

    const std::optional<LaneSelector> selectedSelector = laneSelectorForIndex(selectedLane_);
    const std::optional<LaneSelector> highlightedSelector = laneSelectorForIndex(highlightedLane_);
    applyLaneOrder(lanes_, events_, sortedLaneOrder(lanes_, laneSortOrder_));
    laneRows_ = std::make_shared<const std::vector<std::vector<std::uint64_t>>>(
        buildLaneRows(lanes_, events_));
    selectedLane_ = selectedSelector ? laneIndexFor(*selectedSelector) : -1;
    highlightedLane_ = highlightedSelector ? laneIndexFor(*highlightedSelector) : -1;
    invalidateWaveformCache();
    rebuildContentMetrics();
    updateScrollBars();
    update();
}

void TimelineWidget::moveLane(const int sourceIndex, const int targetIndex)
{
    const int laneCount = static_cast<int>(lanes_.size());
    if (laneCount <= 1
        || sourceIndex < 0
        || sourceIndex >= laneCount
        || targetIndex < 0
        || targetIndex >= laneCount
        || sourceIndex == targetIndex) {
        return;
    }

    const std::optional<LaneSelector> selectedSelector = laneSelectorForIndex(selectedLane_);
    const std::optional<LaneSelector> highlightedSelector = laneSelectorForIndex(highlightedLane_);
    std::vector<int> order;
    order.reserve(lanes_.size());
    for (int index = 0; index < laneCount; ++index) {
        if (index != sourceIndex) {
            order.push_back(index);
        }
    }
    order.insert(order.begin() + targetIndex, sourceIndex);

    applyLaneOrder(lanes_, events_, order);
    laneRows_ = std::make_shared<const std::vector<std::vector<std::uint64_t>>>(
        buildLaneRows(lanes_, events_));
    selectedLane_ = selectedSelector ? laneIndexFor(*selectedSelector) : targetIndex;
    highlightedLane_ = highlightedSelector ? laneIndexFor(*highlightedSelector) : -1;
    laneSortOrder_ = TimelineLaneSortOrder::Custom;
    invalidateWaveformCache();
    rebuildContentMetrics();
    updateScrollBars();
    update();
}

std::optional<std::size_t> TimelineWidget::opcodeTagAtPosition(const QPoint& position) const
{
    if (!plotRect().contains(position) || events_.empty()) {
        return std::nullopt;
    }

    QFont tagFont = font();
    const qreal pointSize = tagFont.pointSizeF();
    if (pointSize > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(6.5, pointSize * 0.78));
    } else {
        tagFont.setPixelSize(qMax(7, static_cast<int>(std::lround(tagFont.pixelSize() * 0.78))));
    }
    const QFontMetrics metrics(tagFont);
    const QRect plot = plotRect();
    const int firstLane = qMax(0, verticalScrollOffset_ / laneStride());
    const int lastLane = qMin(static_cast<int>(lanes_.size()) - 1,
                              (verticalScrollOffset_ + plot.height()) / laneStride() + 1);
    const std::vector<OpcodeTagPlacement> placements =
        visibleOpcodeTagPlacements(metrics, plot, firstLane, lastLane);
    for (auto it = placements.crbegin(); it != placements.crend(); ++it) {
        if (it->rect.contains(position)) {
            return it->eventIndex;
        }
    }
    return std::nullopt;
}

void TimelineWidget::beginLaneReorder(const QPoint& position)
{
    const std::optional<int> lane = laneAtPosition(position);
    if (!lane) {
        return;
    }

    dragMode_ = DragMode::LaneReorder;
    laneDragSourceIndex_ = *lane;
    laneDragTargetIndex_ = *lane;
    laneDragActive_ = false;
    selectedLane_ = *lane;
    dragStart_ = position;
    dragLast_ = position;
    setCursor(Qt::ClosedHandCursor);
    update();
}

void TimelineWidget::updateLaneReorder(const QPoint& position)
{
    dragLast_ = position;
    if (!laneDragActive_
        && (position - dragStart_).manhattanLength() >= kLaneReorderActivationDistance) {
        laneDragActive_ = true;
    }
    if (laneDragActive_) {
        laneDragTargetIndex_ = laneReorderTargetIndex(position);
    }
    update();
}

void TimelineWidget::finishLaneReorder(const QPoint& position)
{
    updateLaneReorder(position);
    const int sourceIndex = laneDragSourceIndex_;
    const int targetIndex = laneDragTargetIndex_;
    const bool moved = laneDragActive_ && sourceIndex != targetIndex;

    dragMode_ = DragMode::None;
    laneDragSourceIndex_ = -1;
    laneDragTargetIndex_ = -1;
    laneDragActive_ = false;
    unsetCursor();
    updateResizeCursor(position);

    if (moved) {
        moveLane(sourceIndex, targetIndex);
        return;
    }

    if (const std::optional<int> lane = laneAtPosition(position)) {
        selectedLane_ = *lane;
    }
    update();
}

void TimelineWidget::cancelLaneReorder()
{
    dragMode_ = DragMode::None;
    laneDragSourceIndex_ = -1;
    laneDragTargetIndex_ = -1;
    laneDragActive_ = false;
    unsetCursor();
    updateResizeCursor(dragLast_);
    update();
}

void TimelineWidget::updateHoveredOpcodeTag(const QPoint& position)
{
    const std::optional<std::size_t> hovered = opcodeTagAtPosition(position);
    if (hoveredOpcodeTagEventIndex_ == hovered) {
        return;
    }
    hoveredOpcodeTagEventIndex_ = hovered;
    update(plotRect());
}

void TimelineWidget::beginPendingGesture(const QPoint& position)
{
    dragMode_ = DragMode::PendingGesture;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    if (positionInTimeline(position)) {
        if (const std::optional<int> lane = laneAtPosition(position)) {
            selectedLane_ = *lane;
        }
    }
    update();
}

void TimelineWidget::updatePendingGesture(const QPoint& position,
                                          const Qt::KeyboardModifiers modifiers)
{
    updateLeftDragGesture(position, modifiers);
}

void TimelineWidget::updateLeftDragGesture(const QPoint& position,
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
            dragStartVerticalOffset_ = verticalScrollOffset_;
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

TimelineWidget::DragMode TimelineWidget::classifyLeftDragGesture(
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
        return DragMode::RangeZoom;
    }
    return DragMode::PendingGesture;
}

void TimelineWidget::beginRangeZoom(const QPoint& position)
{
    dragMode_ = DragMode::RangeZoom;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    update();
}

void TimelineWidget::updateRangeZoom(const QPoint& position)
{
    dragLast_ = position;
    update();
}

bool TimelineWidget::finishRangeZoom()
{
    const QRect plot = plotRect();
    const int left = qBound(plot.left(), std::min(dragStart_.x(), dragLast_.x()), plot.right());
    const int right = qBound(plot.left(), std::max(dragStart_.x(), dragLast_.x()), plot.right());
    if (right - left < 8) {
        update();
        return false;
    }
    zoomToTimeRange(timestampAtTimelineX(left), timestampAtTimelineX(right), false);
    return true;
}

void TimelineWidget::beginZoomFullGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomFull;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    update();
}

bool TimelineWidget::finishZoomFullGesture()
{
    zoomToFit();
    return true;
}

void TimelineWidget::beginZoomIn2xGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomIn2x;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    update();
}

bool TimelineWidget::finishZoomIn2xGesture()
{
    zoomByFactor(2.0, dragStart_);
    return true;
}

void TimelineWidget::beginZoomOut2xGesture(const QPoint& position)
{
    dragMode_ = DragMode::ZoomOut2x;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    update();
}

bool TimelineWidget::finishZoomOut2xGesture()
{
    zoomByFactor(0.5, dragStart_);
    return true;
}

void TimelineWidget::beginFullRangeZoom(const QPoint& position)
{
    dragMode_ = DragMode::FullRangeZoom;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    update();
}

void TimelineWidget::updateFullRangeZoom(const QPoint& position)
{
    dragLast_ = position;
    update();
}

bool TimelineWidget::finishFullRangeZoom()
{
    const QRect full = fullRulerRect();
    const int left = qBound(full.left(), std::min(dragStart_.x(), dragLast_.x()), full.right());
    const int right = qBound(full.left(), std::max(dragStart_.x(), dragLast_.x()), full.right());
    if (totalRows_ == 0 || right - left < 8) {
        update();
        return false;
    }
    zoomToTimeRange(timestampAtFullRulerX(left), timestampAtFullRulerX(right), false);
    return true;
}

void TimelineWidget::beginPan(const QPoint& position)
{
    dragMode_ = DragMode::Pan;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    dragStartScrollOffset_ = scrollOffset_;
    dragStartVerticalOffset_ = verticalScrollOffset_;
    setCursor(Qt::ClosedHandCursor);
}

void TimelineWidget::updatePan(const QPoint& position)
{
    const int dx = position.x() - dragStart_.x();
    const int dy = position.y() - dragStart_.y();
    const std::uint64_t base = dragStartScrollOffset_;
    if (dx > 0) {
        scrollOffset_ = static_cast<std::uint64_t>(dx) > base ? 0 : base - static_cast<std::uint64_t>(dx);
    } else {
        scrollOffset_ = std::min<std::uint64_t>(horizontalContentRange(),
                                                base + static_cast<std::uint64_t>(-dx));
    }
    verticalScrollOffset_ = qBound(0,
                                   dragStartVerticalOffset_ - dy,
                                   qMax(0, contentHeight_ - plotRect().height()));
    requestWaveformRerender();
    updateScrollBars();
    update();
}

void TimelineWidget::beginOverviewPan(const QPoint& position)
{
    dragMode_ = DragMode::OverviewPan;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    leftDragAnchor_ = position;
    dragLast_ = position;
    setCursor(Qt::SizeHorCursor);
    updateOverviewPan(position);
}

void TimelineWidget::updateOverviewPan(const QPoint& position)
{
    const QRect full = fullRulerRect();
    if (!full.contains(position) || totalRows_ == 0) {
        return;
    }
    const std::uint64_t targetContent = contentXForTimestamp(timestampAtFullRulerX(position.x()));
    const std::uint64_t half = static_cast<std::uint64_t>(timelineViewportWidth() / 2);
    scrollToHorizontalOffset(targetContent > half ? targetContent - half : 0);
}

bool TimelineWidget::positionInTimeline(const QPoint& position) const
{
    return plotRect().contains(position);
}

bool TimelineWidget::positionInLaneTable(const QPoint& position) const
{
    const QRect plot = plotRect();
    return position.x() >= 0
        && position.x() < timelineLeft()
        && position.y() >= plot.top()
        && position.y() <= plot.bottom();
}

bool TimelineWidget::positionInTopRuler(const QPoint& position) const
{
    return topRulerRect().contains(position);
}

bool TimelineWidget::positionInFullRuler(const QPoint& position) const
{
    return fullRulerRect().contains(position);
}

bool TimelineWidget::positionInRuler(const QPoint& position) const
{
    return positionInTopRuler(position) || positionInFullRuler(position);
}

QRect TimelineWidget::overviewWindowRect() const
{
    const QRect full = fullRulerRect();
    const std::uint64_t contentWidth = timelineContentWidth();
    const long double leftRatio = contentWidth <= 1 ? 0.0L
        : static_cast<long double>(scrollOffset_) / static_cast<long double>(contentWidth);
    const long double rightRatio = contentWidth <= 1 ? 1.0L
        : static_cast<long double>(scrollOffset_ + timelineViewportWidth())
            / static_cast<long double>(contentWidth);
    const int viewLeft = full.left() + static_cast<int>(std::floor(leftRatio * full.width()));
    const int viewRight = full.left() + static_cast<int>(std::ceil(rightRatio * full.width()));
    return QRect(qBound(full.left(), viewLeft, full.right()),
                 full.top() + 5,
                 qMax(4, qBound(full.left(), viewRight, full.right()) - viewLeft),
                 full.height() - 10);
}

std::optional<int> TimelineWidget::laneAtPosition(const QPoint& position) const
{
    const QRect plot = plotRect();
    if (position.y() < plot.top() || position.y() > plot.bottom()) {
        return std::nullopt;
    }
    if (position.x() < 0 || position.x() > plot.right()) {
        return std::nullopt;
    }
    const int contentY = verticalScrollOffset_ + position.y() - plot.top();
    const int lane = contentY / laneStride();
    if (lane < 0 || lane >= static_cast<int>(lanes_.size())) {
        return std::nullopt;
    }
    return lane;
}

int TimelineWidget::laneReorderTargetIndex(const QPoint& position) const
{
    if (lanes_.empty()) {
        return -1;
    }
    const QRect plot = plotRect();
    const int contentY = qBound(0,
                                verticalScrollOffset_ + position.y() - plot.top(),
                                qMax(0, timelineContentHeight() - 1));
    return qBound(0,
                  contentY / laneStride(),
                  static_cast<int>(lanes_.size()) - 1);
}

std::optional<int> TimelineWidget::logicalRowAtPosition(const QPoint& position) const
{
    if (totalRows_ == 0 || !positionInTimeline(position)) {
        return std::nullopt;
    }
    const qint64 timestamp = timestampAtTimelineX(position.x());
    const std::uint64_t row = logicalRowForTimestamp(timestamp);
    const int lane = laneAtPosition(position).value_or(-1);
    if (snapToEvents_) {
        if (const std::optional<int> nearest = nearestEventRow(timestamp, lane)) {
            return nearest;
        }
    }
    return static_cast<int>(std::min<std::uint64_t>(
        row,
        static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
}

std::optional<int> TimelineWidget::nearestEventRow(const qint64 timestamp, const int lane) const
{
    if (events_.empty()) {
        return std::nullopt;
    }
    const qint64 toleranceTicks = std::max<qint64>(
        1,
        std::llabs(timestampForContentX(scrollOffset_ + 8) - timestampForContentX(scrollOffset_)));
    std::optional<std::uint64_t> bestRow;
    long double bestDistance = (std::numeric_limits<long double>::max)();

    const auto considerRow = [&](const std::uint64_t row) {
        const auto found = findEventByLogicalRow(events_, row);
        if (found == events_.cend()) {
            return;
        }
        if (lane >= 0 && found->lane != lane) {
            return;
        }
        const long double distance = std::abs(static_cast<long double>(found->timestamp)
                                              - static_cast<long double>(timestamp));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestRow = found->logicalRow;
        }
    };

    if (rowTimestampsMonotonic_ && rowTimestamps_ && !rowTimestamps_->empty()) {
        const auto begin = rowTimestamps_->cbegin();
        const auto found = std::lower_bound(begin, rowTimestamps_->cend(), timestamp);
        for (auto it = found; it != rowTimestamps_->cend(); ++it) {
            if (*it > timestamp && *it - timestamp > toleranceTicks) {
                break;
            }
            considerRow(static_cast<std::uint64_t>(std::distance(begin, it)));
        }
        for (auto it = std::make_reverse_iterator(found); it != rowTimestamps_->crend(); ++it) {
            if (*it < timestamp && timestamp - *it > toleranceTicks) {
                break;
            }
            const auto baseIt = std::prev(it.base());
            considerRow(static_cast<std::uint64_t>(std::distance(begin, baseIt)));
        }
    } else {
        for (const TimelineEvent& event : events_) {
            if (lane >= 0 && event.lane != lane) {
                continue;
            }
            const long double distance = std::abs(static_cast<long double>(event.timestamp)
                                                  - static_cast<long double>(timestamp));
            if (distance < bestDistance) {
                bestDistance = distance;
                bestRow = event.logicalRow;
            }
        }
    }

    if (!bestRow || bestDistance > static_cast<long double>(toleranceTicks)) {
        return std::nullopt;
    }
    return static_cast<int>(std::min<std::uint64_t>(
        *bestRow,
        static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
}

#ifdef CHIRON_GUI_ENABLE_TIMELINE_TEST_API
QPoint TimelineWidget::testLaneTablePoint(const int laneIndex) const
{
    const QRect plot = plotRect();
    const int x = qMax(1, timelineLeft() / 2);
    const int y = plot.top()
        + laneIndex * laneStride()
        - verticalScrollOffset_
        + kLaneHeight / 2;
    return QPoint(x, y);
}

QString TimelineWidget::testOpcodeLabelForLogicalRow(const int logicalRow) const
{
    if (logicalRow < 0) {
        return QString();
    }
    const auto found = lowerBoundEventByLogicalRow(events_, static_cast<std::uint64_t>(logicalRow));
    if (found == events_.cend() || found->logicalRow != static_cast<std::uint64_t>(logicalRow)
        || found->opcodeLabelIndex >= opcodeLabels_.size()) {
        return QString();
    }
    return opcodeLabels_[found->opcodeLabelIndex];
}

int TimelineWidget::testVisibleOpcodeTagCount() const
{
    QFont tagFont = font();
    const qreal pointSize = tagFont.pointSizeF();
    if (pointSize > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(6.5, pointSize * 0.78));
    } else {
        tagFont.setPixelSize(qMax(7, static_cast<int>(std::lround(tagFont.pixelSize() * 0.78))));
    }
    const QFontMetrics metrics(tagFont);
    const QRect plot = plotRect();
    const int firstLane = qMax(0, verticalScrollOffset_ / laneStride());
    const int lastLane = qMin(static_cast<int>(lanes_.size()) - 1,
                              (verticalScrollOffset_ + plot.height()) / laneStride() + 1);
    return static_cast<int>(visibleOpcodeTagPlacements(metrics, plot, firstLane, lastLane).size());
}

bool TimelineWidget::testVisibleOpcodeTagsOverlapFlits() const
{
    QFont tagFont = font();
    const qreal pointSize = tagFont.pointSizeF();
    if (pointSize > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(6.5, pointSize * 0.78));
    } else {
        tagFont.setPixelSize(qMax(7, static_cast<int>(std::lround(tagFont.pixelSize() * 0.78))));
    }
    const QFontMetrics metrics(tagFont);
    const QRect plot = plotRect();
    const int firstLane = qMax(0, verticalScrollOffset_ / laneStride());
    const int lastLane = qMin(static_cast<int>(lanes_.size()) - 1,
                              (verticalScrollOffset_ + plot.height()) / laneStride() + 1);
    const std::vector<OpcodeTagPlacement> placements =
        visibleOpcodeTagPlacements(metrics, plot, firstLane, lastLane);
    return std::any_of(placements.cbegin(),
                       placements.cend(),
                       [](const OpcodeTagPlacement& placement) {
                           return placement.overlapsLaneFlits;
                       });
}

QString TimelineWidget::testLaneKey(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return QString();
    }
    const TimelineLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    return QStringLiteral("%1:%2").arg(lane.nodeIdLabel, lane.channelLabel);
}
#endif

int TimelineWidget::laneIndexFor(const LaneSelector& selector) const
{
    for (int index = 0; index < static_cast<int>(lanes_.size()); ++index) {
        const TimelineLane& lane = lanes_[static_cast<std::size_t>(index)];
        if (lane.direction != selector.direction || lane.channel != selector.channel) {
            continue;
        }
        if (selector.nodeId && lane.nodeId != *selector.nodeId) {
            continue;
        }
        return index;
    }
    return -1;
}

void TimelineWidget::paintHeader(QPainter& painter) const
{
    const QRect header = headerRect();
    painter.fillRect(header, QColor(QStringLiteral("#F3F6FA")));
    painter.setPen(QColor(QStringLiteral("#D8DEE8")));
    painter.drawLine(0, header.bottom(), width(), header.bottom());

    const QRect nodeHeader = nodeHeaderRect();
    const QRect channelHeader = channelHeaderRect();
    const QRect timelineHeader(timelineLeft(), 0, width() - timelineLeft(), header.height());

    const auto sortSuffix = [](const bool active) {
        return active ? QStringLiteral("  ^") : QString();
    };
    const bool sortableOrder = laneSortOrder_ != TimelineLaneSortOrder::Custom;
    painter.setPen(QColor(QStringLiteral("#5C6675")));
    painter.drawText(nodeHeader.adjusted(kHeaderPadding, 0, -kHeaderPadding, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Node") + sortSuffix(sortableOrder
                                                         && laneSortOrder_ == TimelineLaneSortOrder::NodeThenChannel));
    painter.drawText(channelHeader.adjusted(kHeaderPadding, 0, -kHeaderPadding, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Channel") + sortSuffix(sortableOrder
                                                            && laneSortOrder_ == TimelineLaneSortOrder::ChannelThenNode));
    const QRect timelineHeaderContent =
        timelineHeader.adjusted(kHeaderPadding, 0, -kHeaderPadding, 0);
    const QString timelineTitle = QStringLiteral("Timeline");
    painter.drawText(timelineHeaderContent,
                     Qt::AlignVCenter | Qt::AlignLeft,
                     timelineTitle);

    const QFont headerFont = painter.font();
    const QFontMetrics titleMetrics(headerFont);
    QFont infoFont = painter.font();
    infoFont.setPointSizeF(qMax<qreal>(6.0, infoFont.pointSizeF() * 0.92));
    painter.setFont(infoFont);
    painter.setPen(QColor(QStringLiteral("#6B7280")));
    const QFontMetrics infoMetrics(infoFont);
    const int titleWidth = titleMetrics.horizontalAdvance(timelineTitle) + 18;
    const QRect infoRect = timelineHeaderContent.adjusted(titleWidth, 0, 0, 0);
    if (infoRect.width() > 24) {
        painter.drawText(infoRect,
                         Qt::AlignVCenter | Qt::AlignRight,
                         infoMetrics.elidedText(visibleRangeText(),
                                                Qt::ElideMiddle,
                                                infoRect.width()));
    }
    painter.setFont(headerFont);

    painter.setPen(QColor(QStringLiteral("#C6CEDA")));
    painter.drawLine(nodeColumnWidth_, 0, nodeColumnWidth_, height());
    painter.drawLine(timelineLeft(), 0, timelineLeft(), height());
}

void TimelineWidget::paintTopRuler(QPainter& painter) const
{
    const QRect ruler = topRulerRect();
    painter.fillRect(QRect(0, ruler.top(), width(), ruler.height()), QColor(QStringLiteral("#FBFCFE")));
    painter.fillRect(ruler, QColor(QStringLiteral("#FFFFFF")));
    painter.setPen(QColor(QStringLiteral("#DDE3EC")));
    painter.drawLine(0, ruler.bottom(), width(), ruler.bottom());

    if (totalRows_ == 0) {
        return;
    }

    const auto [firstTimestamp, lastTimestamp] = visibleTimeRange();
    const long double visibleSpan = std::max<long double>(
        1.0L,
        static_cast<long double>(lastTimestamp) - static_cast<long double>(firstTimestamp));
    const qint64 major = std::max<qint64>(1, static_cast<qint64>(niceStep(static_cast<double>(visibleSpan / 8.0L))));
    const qint64 medium = std::max<qint64>(1, major / 5);
    const qint64 minor = std::max<qint64>(1, medium / 2);

    QFont baseFont = painter.font();
    QFont timeFont = baseFont;
    timeFont.setPointSizeF(qMax<qreal>(6.5, timeFont.pointSizeF() * 0.9));
    QFont mediumFont = baseFont;
    mediumFont.setPointSizeF(qMax<qreal>(6.5, mediumFont.pointSizeF() * 0.82));
    QFont minorFont = baseFont;
    minorFont.setPointSizeF(qMax<qreal>(6.0, minorFont.pointSizeF() * 0.72));

    struct RulerTick {
        qint64 timestamp = 0;
        int x = 0;
        bool major = false;
        bool medium = false;
    };
    std::vector<RulerTick> ticks;

    const qint64 begin = tickStartForStep(firstTimestamp, minor);
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
        ticks.push_back(RulerTick{timestamp, *x, isMajor, isMedium});

        if (timestamp > (std::numeric_limits<qint64>::max)() - minor || minor == 0) {
            break;
        }
    }

    const QRect rowBand(ruler.left() + 2, ruler.top() + 2, qMax(1, ruler.width() - 4), 16);
    const QRect timeBand(ruler.left() + 2, ruler.top() + 19, qMax(1, ruler.width() - 4), 15);
    std::vector<QRect> occupiedRowLabels;
    std::vector<QRect> occupiedTimeLabels;

    const auto labelRect = [](const int x, const int textWidth, const QRect& band) -> QRect {
        const int width = textWidth + 8;
        if (width > band.width()) {
            return QRect();
        }
        const int maxLeft = band.right() - width + 1;
        const int left = qBound(band.left(), x - width / 2, maxLeft);
        return QRect(left, band.top(), width, band.height());
    };
    const auto canPlaceLabel = [](const QRect& candidate, const std::vector<QRect>& occupied) {
        if (!candidate.isValid() || candidate.isEmpty()) {
            return false;
        }
        const QRect padded = candidate.adjusted(-4, 0, 4, 0);
        return std::none_of(occupied.cbegin(),
                            occupied.cend(),
                            [&padded](const QRect& rect) {
                                return padded.intersects(rect);
                            });
    };
    const auto drawLabel = [&](const RulerTick& tick,
                               const QString& text,
                               const QRect& band,
                               const QFont& font,
                               const QColor& color,
                               std::vector<QRect>& occupied) {
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const QRect rect = labelRect(tick.x, metrics.horizontalAdvance(text), band);
        if (!canPlaceLabel(rect, occupied)) {
            return false;
        }
        painter.setPen(color);
        painter.drawText(rect, Qt::AlignCenter, text);
        occupied.push_back(rect);
        return true;
    };

    for (const RulerTick& tick : ticks) {
        if (!tick.major) {
            continue;
        }
        drawLabel(tick,
                  rulerTimeLabel(tick.timestamp),
                  timeBand,
                  timeFont,
                  QColor(QStringLiteral("#334155")),
                  occupiedTimeLabels);
        drawLabel(tick,
                  rulerRowLabel(logicalRowForTimestamp(tick.timestamp)),
                  rowBand,
                  baseFont,
                  QColor(QStringLiteral("#475569")),
                  occupiedRowLabels);
    }

    if (horizontalZoom_ >= 8.0) {
        for (const RulerTick& tick : ticks) {
            if (!tick.medium || tick.major) {
                continue;
            }
            drawLabel(tick,
                      rulerRowLabel(logicalRowForTimestamp(tick.timestamp)),
                      rowBand,
                      mediumFont,
                      QColor(QStringLiteral("#64748B")),
                      occupiedRowLabels);
            if (horizontalZoom_ >= 32.0) {
                drawLabel(tick,
                          rulerTimeLabel(tick.timestamp),
                          timeBand,
                          mediumFont,
                          QColor(QStringLiteral("#64748B")),
                          occupiedTimeLabels);
            }
        }
    }

    if (horizontalZoom_ >= 64.0) {
        for (const RulerTick& tick : ticks) {
            if (tick.medium) {
                continue;
            }
            drawLabel(tick,
                      QString::number(logicalRowForTimestamp(tick.timestamp) + 1),
                      rowBand,
                      minorFont,
                      QColor(QStringLiteral("#94A3B8")),
                      occupiedRowLabels);
        }
    }
    painter.setFont(baseFont);
}

void TimelineWidget::paintFullRuler(QPainter& painter) const
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
    painter.setPen(QColor(QStringLiteral("#3879D9")));
    painter.drawRect(viewRect.adjusted(0, 0, -1, -1));

    painter.setPen(QColor(QStringLiteral("#596579")));
    painter.drawText(full.adjusted(8, 0, -8, 0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Full trace"));
}

void TimelineWidget::paintLaneTable(QPainter& painter) const
{
    const QRect plot = plotRect();
    const QRect table(0, plot.top(), timelineLeft(), plot.height());
    painter.fillRect(table, QColor(QStringLiteral("#FFFFFF")));

    QFont badgeFont = ChannelBadgeScaledFont(font(), 0.9);
    QFontMetrics badgeMetrics(badgeFont);
    QFontMetrics metrics(font());

    const int firstLane = qMax(0, verticalScrollOffset_ / laneStride());
    const int lastLane = qMin(static_cast<int>(lanes_.size()) - 1,
                              (verticalScrollOffset_ + plot.height()) / laneStride() + 1);

    for (int laneIndex = firstLane; laneIndex <= lastLane; ++laneIndex) {
        const TimelineLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
        const int y = plot.top() + laneIndex * laneStride() - verticalScrollOffset_;
        const QRect rowRect(0, y, timelineLeft(), kLaneHeight);
        const bool draggingSource = laneDragActive_ && laneIndex == laneDragSourceIndex_;
        const bool draggingTarget = dragMode_ == DragMode::LaneReorder && laneIndex == laneDragTargetIndex_;
        if (draggingSource) {
            painter.fillRect(rowRect, QColor(QStringLiteral("#EEF2F7")));
        } else if (draggingTarget) {
            painter.fillRect(rowRect, QColor(QStringLiteral("#DCEBFF")));
        } else if (laneIndex == selectedLane_ || laneIndex == highlightedLane_) {
            painter.fillRect(rowRect, laneIndex == highlightedLane_
                                          ? SearchHighlightBackground()
                                          : QColor(QStringLiteral("#EAF2FF")));
        } else if (laneIndex % 2 == 1) {
            painter.fillRect(rowRect, QColor(QStringLiteral("#FBFCFE")));
        }

        const QRect nodeCell(0, y, nodeColumnWidth_, kLaneHeight);
        const QRect channelCell(nodeColumnWidth_, y, channelColumnWidth_, kLaneHeight);
        QRect numberRect = nodeCell.adjusted(4, 0, -(nodeColumnWidth_ - kNodeNumberAreaWidth), 0);
        numberRect.setWidth(qMin(kNodeNumberAreaWidth, nodeCell.width() - 4));
        painter.setPen(QColor(QStringLiteral("#374151")));
        painter.drawText(numberRect, Qt::AlignRight | Qt::AlignVCenter, lane.nodeIdLabel);

        const int badgeAvailableLeft = nodeCell.left() + qMin(nodeCell.width(), kNodeNumberAreaWidth + 8);
        QRect nodeBadgeRect(badgeAvailableLeft,
                            y + 3,
                            nodeCell.right() - badgeAvailableLeft - 4,
                            qMax(12, kLaneHeight - 6));
        if (nodeBadgeRect.width() < ChannelBadgeMainWidth(badgeMetrics, lane.nodeTypeLabel) / 2) {
            nodeBadgeRect = nodeCell.adjusted(4, 3, -4, -3);
        }
        PaintChannelBadge(&painter,
                          nodeBadgeRect,
                          badgeFont,
                          lane.nodeTypeLabel,
                          QString(),
                          lane.nodeTypeColor,
                          laneIndex == selectedLane_,
                          laneIndex == highlightedLane_,
                          true);

        PaintChannelBadge(&painter,
                          channelCell.adjusted(6, 3, -6, -3),
                          badgeFont,
                          lane.channelLabel,
                          QString(),
                          ChannelAccent(lane.channel),
                          laneIndex == selectedLane_,
                          laneIndex == highlightedLane_,
                          true);

        painter.setPen(QColor(QStringLiteral("#E3E8F0")));
        painter.drawLine(rowRect.left(), rowRect.bottom(), rowRect.right(), rowRect.bottom());
    }

    if (dragMode_ == DragMode::LaneReorder
        && laneDragActive_
        && laneDragTargetIndex_ >= firstLane
        && laneDragTargetIndex_ <= lastLane) {
        const int y = plot.top() + laneDragTargetIndex_ * laneStride() - verticalScrollOffset_;
        const QRect targetRect(0, y, timelineLeft(), kLaneHeight);
        painter.setPen(QPen(QColor(QStringLiteral("#2F80ED")), 2));
        painter.drawRect(targetRect.adjusted(1, 1, -2, -2));
    }

    painter.setPen(QColor(QStringLiteral("#D8DEE8")));
    painter.drawLine(nodeColumnWidth_, plot.top(), nodeColumnWidth_, plot.bottom());
    painter.drawLine(timelineLeft(), plot.top(), timelineLeft(), plot.bottom());
}

void TimelineWidget::paintWaveforms(QPainter& painter)
{
    const QRect plot = plotRect();
    painter.save();
    painter.setClipRect(plot);
    painter.fillRect(plot, QColor(QStringLiteral("#FFFFFF")));

    const int firstLane = qMax(0, verticalScrollOffset_ / laneStride());
    const int lastLane = qMin(static_cast<int>(lanes_.size()) - 1,
                              (verticalScrollOffset_ + plot.height()) / laneStride() + 1);

    for (int laneIndex = firstLane; laneIndex <= lastLane; ++laneIndex) {
        const int y = plot.top() + laneIndex * laneStride() - verticalScrollOffset_;
        const QRect laneRect(plot.left(), y, plot.width(), kLaneHeight);
        const bool draggingSource = laneDragActive_ && laneIndex == laneDragSourceIndex_;
        const bool draggingTarget = dragMode_ == DragMode::LaneReorder && laneIndex == laneDragTargetIndex_;
        if (draggingSource) {
            painter.fillRect(laneRect, QColor(QStringLiteral("#F3F6FA")));
        } else if (draggingTarget) {
            painter.fillRect(laneRect, QColor(QStringLiteral("#EEF6FF")));
        } else if (laneIndex == selectedLane_ || laneIndex == highlightedLane_) {
            painter.fillRect(laneRect, laneIndex == highlightedLane_
                                           ? withAlpha(SearchHighlightBackground(), 160)
                                           : QColor(QStringLiteral("#F0F6FF")));
        } else if (laneIndex % 2 == 1) {
            painter.fillRect(laneRect, QColor(QStringLiteral("#FBFCFE")));
        }
        painter.setPen(QColor(QStringLiteral("#E4EAF2")));
        painter.drawLine(plot.left(), y + kLaneHeight / 2, plot.right(), y + kLaneHeight / 2);
        painter.setPen(QColor(QStringLiteral("#EEF2F7")));
        painter.drawLine(plot.left(), laneRect.bottom(), plot.right(), laneRect.bottom());
    }

    if (totalRows_ > 0) {
        const auto [firstTimestamp, lastTimestamp] = visibleTimeRange();
        const long double visibleSpan = std::max<long double>(
            1.0L,
            static_cast<long double>(lastTimestamp) - static_cast<long double>(firstTimestamp));
        const qint64 major = std::max<qint64>(1, static_cast<qint64>(niceStep(static_cast<double>(visibleSpan / 8.0L))));
        const qint64 minor = std::max<qint64>(1, major / 5);
        const qint64 begin = tickStartForStep(firstTimestamp, minor);
        for (qint64 timestamp = begin; timestamp <= lastTimestamp; timestamp += minor) {
            const auto x = screenXForTimestamp(timestamp, plot);
            if (!x) {
                if (timestamp > lastTimestamp - minor) {
                    break;
                }
                continue;
            }
            const bool isMajor = timestamp % major == 0;
            painter.setPen(isMajor ? QColor(QStringLiteral("#DCE3ED"))
                                   : QColor(QStringLiteral("#EEF2F7")));
            painter.drawLine(*x, plot.top(), *x, plot.bottom());
            if (timestamp > (std::numeric_limits<qint64>::max)() - minor || minor == 0) {
                break;
            }
        }
    }

    if (!laneRows_ || totalRows_ == 0 || lanes_.empty()) {
        painter.restore();
        return;
    }

    const WaveformCacheKey key = currentWaveformCacheKey(plot);
    if (waveformCacheValid_ && waveformCacheKey_ == key && !waveformCacheImage_.isNull()) {
        painter.drawImage(plot.topLeft(), waveformCacheImage_);
    } else {
        if (waveformCacheValid_ && !waveformCacheImage_.isNull()) {
            painter.save();
            painter.setClipRect(plot);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

            const bool sameTrace = waveformCacheKey_.dataGeneration == key.dataGeneration
                && waveformCacheKey_.totalRows == key.totalRows;
            const long double oldSpan = waveformCacheKey_.contentWidth > 1
                ? static_cast<long double>(waveformCacheKey_.contentWidth - 1)
                : 1.0L;
            const long double newSpan = key.contentWidth > 1
                ? static_cast<long double>(key.contentWidth - 1)
                : oldSpan;
            const long double scale = sameTrace ? newSpan / oldSpan : 1.0L;
            const long double mappedLeft = sameTrace
                ? static_cast<long double>(waveformCacheKey_.scrollOffset) * scale
                    - static_cast<long double>(key.scrollOffset)
                : 0.0L;
            const double dy = sameTrace
                ? static_cast<double>(waveformCacheKey_.verticalScrollOffset - key.verticalScrollOffset)
                : 0.0;
            const QRectF target(plot.left() + static_cast<double>(mappedLeft),
                                plot.top() + dy,
                                qMax(1.0, static_cast<double>(
                                                 static_cast<long double>(waveformCacheImage_.width())
                                                 * scale)),
                                static_cast<double>(waveformCacheImage_.height()));
            painter.drawImage(target,
                              waveformCacheImage_,
                              QRectF(waveformCacheImage_.rect()));
            painter.restore();
        }
        scheduleWaveformRender(key);
        if (events_.size() <= kInlineWaveformRenderEventLimit) {
            for (int laneIndex = firstLane; laneIndex <= lastLane; ++laneIndex) {
                if (laneIndex < 0 || laneIndex >= static_cast<int>(laneRows_->size())
                    || laneIndex >= static_cast<int>(lanes_.size())) {
                    continue;
                }
                const std::vector<std::uint64_t>& rows =
                    (*laneRows_)[static_cast<std::size_t>(laneIndex)];
                const auto [firstTimestamp, lastTimestamp] = visibleTimeRange();
                const std::uint64_t firstRow = firstLogicalRowAtOrAfterTimestamp(firstTimestamp);
                const std::uint64_t lastRow = lastLogicalRowAtOrBeforeTimestamp(lastTimestamp);
                if (firstRow > lastRow) {
                    continue;
                }
                auto lower = std::lower_bound(rows.cbegin(), rows.cend(), firstRow);
                auto upper = std::upper_bound(rows.cbegin(), rows.cend(), lastRow);
                const int centerY = plot.top() + laneIndex * laneStride()
                    - verticalScrollOffset_ + kLaneHeight / 2;
                const QColor color = eventColor(lanes_[static_cast<std::size_t>(laneIndex)].channel,
                                                lanes_[static_cast<std::size_t>(laneIndex)].direction);
                painter.setPen(QPen(color, 1));
                painter.setBrush(color);
                int lastX = std::numeric_limits<int>::min();
                int duplicateCount = 0;
                for (auto it = lower; it != upper; ++it) {
                    const auto timestamp = timestampForLogicalRow(*it);
                    if (!timestamp || *timestamp < firstTimestamp || *timestamp > lastTimestamp) {
                        continue;
                    }
                    const auto x = screenXForLogicalRow(*it, plot);
                    if (!x) {
                        continue;
                    }
                    if (*x == lastX) {
                        ++duplicateCount;
                        if (duplicateCount > kMaxEventsPerPixelPerLane) {
                            continue;
                        }
                    } else {
                        lastX = *x;
                        duplicateCount = 0;
                    }
                    const QRect pulse(*x - 1, centerY - 4, 3, 9);
                    painter.drawRect(pulse);
                    painter.drawLine(*x, centerY - 6, *x, centerY + 6);
                }
            }
        }
    }

    paintOpcodeTags(painter, plot, firstLane, lastLane);

    painter.restore();
}

void TimelineWidget::paintOpcodeTags(QPainter& painter,
                                     const QRect& plot,
                                     const int firstLane,
                                     const int lastLane) const
{
    if (events_.empty() || opcodeLabels_.size() <= 1 || totalRows_ <= 1
        || plot.width() <= 0 || lanes_.empty()) {
        return;
    }

    const std::uint64_t contentWidth = timelineContentWidth();
    const long double totalTimeSpan = std::max<long double>(
        1.0L,
        static_cast<long double>(lastTimestamp_) - static_cast<long double>(firstTimestamp_));
    const double pixelsPerTimeUnit = static_cast<double>(
        static_cast<long double>(contentWidth - 1) / totalTimeSpan);
    if (pixelsPerTimeUnit < kOpcodeTagMinimumPixelsPerRow && horizontalZoom_ < 8.0) {
        return;
    }

    QFont tagFont = painter.font();
    const qreal pointSize = tagFont.pointSizeF();
    if (pointSize > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(6.5, pointSize * 0.78));
    } else {
        tagFont.setPixelSize(qMax(7, static_cast<int>(std::lround(tagFont.pixelSize() * 0.78))));
    }
    const QFontMetrics metrics(tagFont);
    const int tagHeight = qMin(kLaneHeight - 3, metrics.height() + 4);
    if (tagHeight < 8) {
        return;
    }

    const std::vector<OpcodeTagPlacement> placements =
        visibleOpcodeTagPlacements(metrics, plot, firstLane, lastLane);
    if (placements.empty()) {
        return;
    }
    const QPoint mousePosition = mapFromGlobal(QCursor::pos());

    painter.save();
    painter.setFont(tagFont);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const OpcodeTagPlacement& placement : placements) {
        if (placement.eventIndex >= events_.size()) {
            continue;
        }
        const TimelineEvent& event = events_[placement.eventIndex];

        const QString& label = opcodeLabels_[event.opcodeLabelIndex];
        if (label.isEmpty()) {
            continue;
        }

        const bool selected = selectedLogicalRow_ >= 0
            && event.logicalRow == static_cast<std::uint64_t>(selectedLogicalRow_);
        const bool hovered = hoveredOpcodeTagEventIndex_ == placement.eventIndex
            || placement.rect.contains(mousePosition);
        const int fillAlpha = placement.overlapsLaneFlits && !selected && !hovered ? 72 : 236;
        const int penAlpha = placement.overlapsLaneFlits && !selected && !hovered ? 92 : 255;
        const int textAlpha = placement.overlapsLaneFlits && !selected && !hovered ? 112 : 255;

        const int laneIndex = event.lane;
        const TimelineLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
        const QColor accent = eventColor(lane.channel, lane.direction);
        painter.setPen(QPen(withAlpha(accent.darker(115), penAlpha), 1));
        painter.setBrush(withAlpha(QColor(QStringLiteral("#FFFFFF")), fillAlpha));
        painter.drawRoundedRect(placement.rect, 3, 3);
        painter.setPen(withAlpha(accent.darker(170), textAlpha));
        painter.drawText(placement.rect.adjusted(kOpcodeTagPaddingX, 0, -kOpcodeTagPaddingX, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         label);
    }

    painter.restore();
}

std::vector<TimelineWidget::OpcodeTagPlacement>
TimelineWidget::visibleOpcodeTagPlacements(const QFontMetrics& metrics,
                                           const QRect& plot,
                                           const int firstLane,
                                           const int lastLane) const
{
    std::vector<OpcodeTagPlacement> placements;
    if (events_.empty() || opcodeLabels_.size() <= 1 || totalRows_ <= 1
        || plot.width() <= 0 || lanes_.empty()) {
        return placements;
    }

    const std::uint64_t contentWidth = timelineContentWidth();
    const long double totalTimeSpan = std::max<long double>(
        1.0L,
        static_cast<long double>(lastTimestamp_) - static_cast<long double>(firstTimestamp_));
    const double pixelsPerTimeUnit = static_cast<double>(
        static_cast<long double>(contentWidth - 1) / totalTimeSpan);
    if (pixelsPerTimeUnit < kOpcodeTagMinimumPixelsPerRow && horizontalZoom_ < 8.0) {
        return placements;
    }

    const int tagHeight = qMin(kLaneHeight - 3, metrics.height() + 4);
    if (tagHeight < 8) {
        return placements;
    }

    const auto [firstTimestamp, lastTimestamp] = visibleTimeRange();
    const std::uint64_t firstRow = firstLogicalRowAtOrAfterTimestamp(firstTimestamp);
    const std::uint64_t lastRow = lastLogicalRowAtOrBeforeTimestamp(lastTimestamp);
    if (firstRow > lastRow) {
        return placements;
    }
    auto it = lowerBoundEventByLogicalRow(events_, firstRow);

    std::vector<int> nextAllowedX(lanes_.size(), plot.left());
    int candidates = 0;
    for (; it != events_.cend() && it->logicalRow <= lastRow; ++it) {
        if (++candidates > kOpcodeTagMaxCandidatesPerPaint) {
            break;
        }
        if (it->timestamp < firstTimestamp || it->timestamp > lastTimestamp) {
            continue;
        }
        if (it->lane < firstLane || it->lane > lastLane
            || it->lane < 0 || it->lane >= static_cast<int>(lanes_.size())
            || it->opcodeLabelIndex == 0
            || it->opcodeLabelIndex >= opcodeLabels_.size()) {
            continue;
        }

        const QString& label = opcodeLabels_[it->opcodeLabelIndex];
        const int textWidth = metrics.horizontalAdvance(label);
        if (label.isEmpty() || textWidth <= 0 || textWidth > kOpcodeTagMaxTextWidth) {
            continue;
        }

        const auto x = screenXForLogicalRow(it->logicalRow, plot);
        if (!x) {
            continue;
        }
        const int laneIndex = it->lane;
        const int centerY = plot.top() + laneIndex * laneStride()
            - verticalScrollOffset_ + kLaneHeight / 2;
        const int tagWidth = textWidth + kOpcodeTagPaddingX * 2;
        const int left = *x + 5;
        const QRect tagRect(left,
                            centerY - tagHeight / 2,
                            tagWidth,
                            tagHeight);
        if (tagRect.left() < nextAllowedX[static_cast<std::size_t>(laneIndex)]
            || tagRect.right() > plot.right() - 2) {
            continue;
        }

        placements.push_back(OpcodeTagPlacement{
            static_cast<std::size_t>(std::distance(events_.cbegin(), it)),
            tagRect,
            opcodeTagOverlapsLaneFlits(tagRect, laneIndex, it->logicalRow, plot)});
        nextAllowedX[static_cast<std::size_t>(laneIndex)] = tagRect.right() + kOpcodeTagGap;
    }
    return placements;
}

bool TimelineWidget::opcodeTagOverlapsLaneFlits(const QRect& tagRect,
                                                const int laneIndex,
                                                const std::uint64_t ownerRow,
                                                const QRect& plot) const
{
    if (!laneRows_ || laneIndex < 0 || laneIndex >= static_cast<int>(laneRows_->size())) {
        return false;
    }
    const QRect protectedRect = tagRect.adjusted(-kOpcodeTagFlitClearance,
                                                 -kOpcodeTagFlitClearance,
                                                 kOpcodeTagFlitClearance,
                                                 kOpcodeTagFlitClearance);
    const qint64 firstTimestamp = timestampAtTimelineX(protectedRect.left());
    const qint64 lastTimestamp = timestampAtTimelineX(protectedRect.right());
    const std::uint64_t firstRow = firstLogicalRowAtOrAfterTimestamp(std::min(firstTimestamp, lastTimestamp));
    const std::uint64_t lastRow = lastLogicalRowAtOrBeforeTimestamp(std::max(firstTimestamp, lastTimestamp));
    if (firstRow > lastRow) {
        return false;
    }
    const std::vector<std::uint64_t>& rows = (*laneRows_)[static_cast<std::size_t>(laneIndex)];
    auto lower = std::lower_bound(rows.cbegin(), rows.cend(), std::min(firstRow, lastRow));
    auto upper = std::upper_bound(rows.cbegin(), rows.cend(), std::max(firstRow, lastRow));
    for (auto it = lower; it != upper; ++it) {
        if (*it == ownerRow) {
            continue;
        }
        const auto timestamp = timestampForLogicalRow(*it);
        if (!timestamp || *timestamp < std::min(firstTimestamp, lastTimestamp)
            || *timestamp > std::max(firstTimestamp, lastTimestamp)) {
            continue;
        }
        const auto x = screenXForLogicalRow(*it, plot);
        if (!x) {
            continue;
        }
        if (*x >= protectedRect.left() && *x <= protectedRect.right()) {
            return true;
        }
    }
    return false;
}

void TimelineWidget::paintCursorAndMarker(QPainter& painter) const
{
    const QRect plot = plotRect();
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
        painter.fillRect(QRect(*x - 1, plot.top(), 3, plot.height()), color);
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

    drawMarker(markerLogicalRow_, QColor(QStringLiteral("#2F80ED")),
               markerLogicalRow_ >= 0 ? QStringLiteral("M %1").arg(markerLogicalRow_ + 1) : QString(),
               Qt::DashLine);
    drawMarker(selectedLogicalRow_, QColor(QStringLiteral("#C48A00")),
               selectedLogicalRow_ >= 0 ? QStringLiteral("C %1").arg(selectedLogicalRow_ + 1) : QString(),
               Qt::SolidLine);
}

void TimelineWidget::paintRangeZoom(QPainter& painter) const
{
    if (dragMode_ != DragMode::RangeZoom && dragMode_ != DragMode::FullRangeZoom) {
        return;
    }
    const QRect area = dragMode_ == DragMode::FullRangeZoom ? fullRulerRect() : plotRect();
    const int left = qBound(area.left(), std::min(dragStart_.x(), dragLast_.x()), area.right());
    const int right = qBound(area.left(), std::max(dragStart_.x(), dragLast_.x()), area.right());
    QRect selection(left, area.top(), qMax(1, right - left), area.height());
    painter.fillRect(selection, withAlpha(QColor(QStringLiteral("#2F80ED")), 42));
    painter.setPen(QPen(QColor(QStringLiteral("#2F80ED")), 1, Qt::DashLine));
    painter.drawRect(selection.adjusted(0, 0, -1, -1));
}

void TimelineWidget::paintGestureOverlay(QPainter& painter) const
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

void TimelineWidget::paintStatus(QPainter& painter) const
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

    painter.setPen(QColor(QStringLiteral("#6B7280")));
    QString mode = snapToEvents_ ? QStringLiteral("Snap") : QStringLiteral("Free");
    if (lockCursorMarkerDelta_) {
        mode += QStringLiteral(" | Delta lock");
    }
    painter.drawText(plot.adjusted(8, 0, -8, -4),
                     Qt::AlignRight | Qt::AlignBottom,
                     mode);
}

}  // namespace CHIron::Gui
