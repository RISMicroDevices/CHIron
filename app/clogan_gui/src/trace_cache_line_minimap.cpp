#include "trace_cache_line_minimap.hpp"

#include "gui_format.hpp"

#include <QAbstractItemModel>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <utility>

namespace CHIron::Gui {
namespace {

constexpr int kOverlayBaseWidth = 80;
constexpr int kOverlayMaxWidth = 220;
constexpr int kLaneWidth = 14;
constexpr int kLaneGap = 5;
constexpr int kTagHeight = 18;
constexpr int kTagMaxWidth = 120;
constexpr int kTagGap = 2;
constexpr int kOuterMargin = 4;
constexpr int kBuildProgressWidth = 190;
constexpr int kBuildProgressHeight = 14;
constexpr int kBuildProgressMargin = 8;
const QColor kLaneBorderColor(QStringLiteral("#1F2937"));
const QColor kMinimapTextColor(QStringLiteral("#000000"));

struct CacheMapJumpStateSpec {
    TraceCacheLineMinimap::JumpState state;
    std::uint8_t mask = 0;
    QString label;
};

struct CacheMapJumpDirectionSpec {
    TraceCacheLineMinimap::JumpDirection direction;
    QString label;
};

QString addressText(const std::uint64_t address)
{
    return QStringLiteral("0x%1").arg(QString::number(static_cast<qulonglong>(address), 16).toUpper());
}

QColor withAlpha(QColor color, const int alpha)
{
    color.setAlpha(alpha);
    return color;
}

bool parseIntegerToken(QString text, std::uint64_t& value)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return false;
    }

    bool ok = false;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = text.sliced(2).toULongLong(&ok, 16);
        return ok;
    }

    bool hasHexLetter = false;
    for (const QChar ch : text) {
        if (ch.isDigit()) {
            continue;
        }
        const ushort code = ch.unicode();
        if ((code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F')) {
            hasHexLetter = true;
            continue;
        }
        return false;
    }

    value = text.toULongLong(&ok, hasHexLetter ? 16 : 10);
    return ok;
}

bool segmentsCanMerge(const CLogBTraceCacheLineStateSpan& lhs,
                      const CLogBTraceCacheLineStateSpan& rhs)
{
    return lhs.rnNodeId == rhs.rnNodeId
        && lhs.lineAddress == rhs.lineAddress
        && lhs.stateMask == rhs.stateMask
        && lhs.stateText == rhs.stateText
        && lhs.openEnded == rhs.openEnded
        && lhs.endLogicalRow + 1 >= rhs.startLogicalRow;
}

bool isRequesterNodeType(const CLog::NodeType type) noexcept
{
    switch (type) {
    case CLog::NodeType::RN_F:
    case CLog::NodeType::RN_D:
    case CLog::NodeType::RN_I:
        return true;
    default:
        return false;
    }
}

QString nodeTypeText(const CLog::NodeType type)
{
    return QString::fromStdString(CLog::NodeTypeToString(type));
}

int cacheStateBitCount(std::uint8_t value) noexcept
{
    int count = 0;
    while (value != 0) {
        count += value & 1U;
        value >>= 1U;
    }
    return count;
}

QColor cacheStateCombinationColor(const std::uint8_t stateMask)
{
    const std::uint8_t liveMask = stateMask & 0x3fU;
    switch (liveMask) {
    case 0x00:
        return QColor(QStringLiteral("#CBD5E1"));
    case 0x01:  // UC
        return QColor(QStringLiteral("#86EFAC"));
    case 0x02:  // UCE
        return QColor(QStringLiteral("#99F6E4"));
    case 0x04:  // UD
        return QColor(QStringLiteral("#FDBA74"));
    case 0x08:  // UDP
        return QColor(QStringLiteral("#FDE68A"));
    case 0x10:  // SC
        return QColor(QStringLiteral("#93C5FD"));
    case 0x20:  // SD
        return QColor(QStringLiteral("#F0ABFC"));
    default:
        break;
    }

    const int bitCount = cacheStateBitCount(liveMask);
    const int hue = (static_cast<int>(liveMask) * 47 + bitCount * 29) % 360;
    const int saturation = 92 + (static_cast<int>(liveMask) * 11) % 42;
    const int lightness = 214 - qMin(bitCount, 4) * 8;
    return QColor::fromHsl(hue, saturation, lightness);
}

const std::array<CacheMapJumpStateSpec, 6>& cacheMapJumpStates()
{
    static const std::array<CacheMapJumpStateSpec, 6> states{{
        {TraceCacheLineMinimap::JumpState::UC, 0x01, QStringLiteral("UC")},
        {TraceCacheLineMinimap::JumpState::UCE, 0x02, QStringLiteral("UCE")},
        {TraceCacheLineMinimap::JumpState::UD, 0x04, QStringLiteral("UD")},
        {TraceCacheLineMinimap::JumpState::UDP, 0x08, QStringLiteral("UDP")},
        {TraceCacheLineMinimap::JumpState::SC, 0x10, QStringLiteral("SC")},
        {TraceCacheLineMinimap::JumpState::SD, 0x20, QStringLiteral("SD")},
    }};
    return states;
}

const std::array<CacheMapJumpDirectionSpec, 4>& cacheMapJumpDirections()
{
    static const std::array<CacheMapJumpDirectionSpec, 4> directions{{
        {TraceCacheLineMinimap::JumpDirection::First, QStringLiteral("First")},
        {TraceCacheLineMinimap::JumpDirection::Previous, QStringLiteral("Previous")},
        {TraceCacheLineMinimap::JumpDirection::Next, QStringLiteral("Next")},
        {TraceCacheLineMinimap::JumpDirection::Last, QStringLiteral("Last")},
    }};
    return directions;
}

std::uint8_t cacheMapJumpStateMask(const TraceCacheLineMinimap::JumpState state)
{
    for (const CacheMapJumpStateSpec& spec : cacheMapJumpStates()) {
        if (spec.state == state) {
            return spec.mask;
        }
    }
    return 0;
}

}  // namespace

TraceCacheLineMinimap::TraceCacheLineMinimap(QTableView* table, const HostMode mode, QWidget* parent)
    : QWidget(parent ? parent : (table ? table->parentWidget() : nullptr))
    , table_(table)
    , hostMode_(mode)
{
    setObjectName(QStringLiteral("traceCacheLineMinimap"));
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    if (table_) {
        if (!parent && table_->parentWidget()) {
            setParent(table_->parentWidget());
        }
        table_->installEventFilter(this);
        table_->viewport()->installEventFilter(this);
        if (table_->verticalScrollBar()) {
            table_->verticalScrollBar()->installEventFilter(this);
            connect(table_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
                rebuildVisibleSegments();
                update();
            });
            connect(table_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this]() {
                updateOverlayGeometry();
                rebuildVisibleSegments();
                update();
            });
        }
        updateOverlayGeometry();
    }
}

TraceCacheLineMinimap::~TraceCacheLineMinimap()
{
    stopBuildThread();
}

void TraceCacheLineMinimap::setTraceSession(std::shared_ptr<TraceSession> session)
{
    stopBuildThread();
    traceSession_ = std::move(session);
    statusText_.clear();
    for (Lane& lane : lanes_) {
        lane.sourceSpans.clear();
        lane.segments.clear();
        lane.building = false;
        lane.failed = false;
        lane.hasBuildResult = false;
    }
    resetBuildProgress();
    startAllLaneBuilds();
    rebuildVisibleSegments();
    update();
}

void TraceCacheLineMinimap::setFlitModel(FlitTableModel* model)
{
    if (model_ == model) {
        return;
    }
    model_ = model;
    updateModelConnections();
    rebuildVisibleSegments();
    updateOverlayGeometry();
    update();
}

void TraceCacheLineMinimap::setClipboardContext(
    ClipboardWidget* clipboard,
    std::function<std::shared_ptr<TraceSession>(quint64)> sourceSessionResolver)
{
    clipboard_ = clipboard;
    sourceSessionResolver_ = std::move(sourceSessionResolver);
    setFlitModel(clipboard ? clipboard->model() : nullptr);
    startAllClipboardLaneBuilds();
    rebuildVisibleSegments();
    update();
}

void TraceCacheLineMinimap::clearSource()
{
    stopBuildThread();
    traceSession_.reset();
    sourceSessionResolver_ = {};
    for (Lane& lane : lanes_) {
        lane.sourceSpans.clear();
        lane.sourceSpanGroups.clear();
        lane.segments.clear();
        lane.building = false;
        lane.failed = false;
        lane.hasBuildResult = false;
    }
    statusText_.clear();
    resetBuildProgress();
    rebuildVisibleSegments();
    update();
}

void TraceCacheLineMinimap::setModelUpdatesSuspended(const bool suspended)
{
    if (suspended) {
        ++modelUpdatesSuspended_;
        return;
    }

    if (modelUpdatesSuspended_ > 0) {
        --modelUpdatesSuspended_;
    }
    if (modelUpdatesSuspended_ == 0 && pendingModelRefresh_) {
        pendingModelRefresh_ = false;
        rebuildVisibleSegments();
        update();
    }
}

bool TraceCacheLineMinimap::mapVisible() const noexcept
{
    return mapVisible_;
}

int TraceCacheLineMinimap::rightReservedWidth() const noexcept
{
    return rightReservedWidth_;
}

void TraceCacheLineMinimap::setRightReservedWidth(const int width)
{
    const int clamped = qMax(0, width);
    if (rightReservedWidth_ == clamped) {
        return;
    }
    rightReservedWidth_ = clamped;
    updateOverlayGeometry();
    rebuildVisibleSegments();
    update();
}

void TraceCacheLineMinimap::setMapVisible(const bool visible)
{
    if (mapVisible_ == visible) {
        updateOverlayGeometry();
        setVisible(mapVisible_);
        if (mapVisible_) {
            raise();
        }
        update();
        return;
    }
    mapVisible_ = visible;
    updateOverlayGeometry();
    setVisible(mapVisible_);
    if (mapVisible_) {
        raise();
    }
    update();
    Q_EMIT mapVisibilityChanged(mapVisible_);
}

bool TraceCacheLineMinimap::fadeWhenInactive() const noexcept
{
    return fadeWhenInactive_;
}

void TraceCacheLineMinimap::setFadeWhenInactive(const bool enabled)
{
    fadeWhenInactive_ = enabled;
    update();
}

bool TraceCacheLineMinimap::addLane(const std::uint32_t rnNodeId, const std::uint64_t address)
{
    const std::uint64_t lineAddress = CLogBTraceLoader::normalizeCacheLineAddress(address);
    const auto duplicate = std::find_if(lanes_.cbegin(), lanes_.cend(), [&](const Lane& lane) {
        return lane.rnNodeId == rnNodeId && lane.lineAddress == lineAddress;
    });
    if (duplicate != lanes_.cend()) {
        requestStatus(QStringLiteral("Cache Map already contains RN %1 %2.")
                          .arg(rnNodeId)
                          .arg(addressText(lineAddress)));
        return false;
    }

    Lane lane;
    lane.rnNodeId = rnNodeId;
    lane.lineAddress = lineAddress;
    lane.statusText = QStringLiteral("Pending");
    lanes_.push_back(std::move(lane));
    setMapVisible(true);
    updateOverlayGeometry();
    if (hostMode_ == HostMode::Clipboard) {
        startAllClipboardLaneBuilds(true);
    } else {
        startAllLaneBuilds(true);
    }
    rebuildVisibleSegments();
    update();
    return true;
}

std::vector<TraceCacheLineMinimap::RnNodeChoice> TraceCacheLineMinimap::availableRnNodeChoices() const
{
    std::vector<RnNodeChoice> choices;
    QSet<std::uint32_t> seen;
    const auto appendFromMetadata = [&choices, &seen](const CLogBTraceMetadata& metadata) {
        for (const auto& [nodeId, nodeType] : metadata.nodeTypes) {
            if (!isRequesterNodeType(nodeType)) {
                continue;
            }
            const std::uint32_t id = static_cast<std::uint32_t>(nodeId);
            if (seen.contains(id)) {
                continue;
            }
            seen.insert(id);
            const QString type = nodeTypeText(nodeType);
            choices.push_back(RnNodeChoice{
                .nodeId = id,
                .typeText = type,
                .label = QStringLiteral("RN %1  %2").arg(id).arg(type),
            });
        }
    };

    if (traceSession_) {
        appendFromMetadata(traceSession_->metadata());
    }
    if (hostMode_ == HostMode::Clipboard && clipboard_ && sourceSessionResolver_) {
        const std::vector<ClipboardEntry> entries = clipboard_->currentEntriesSnapshot();
        QSet<quint64> sourceIds;
        for (const ClipboardEntry& entry : entries) {
            if (entry.source.sessionId == 0 || sourceIds.contains(entry.source.sessionId)) {
                continue;
            }
            sourceIds.insert(entry.source.sessionId);
            const std::shared_ptr<TraceSession> source = sourceSessionResolver_(entry.source.sessionId);
            if (source) {
                appendFromMetadata(source->metadata());
            }
        }
    }
    std::sort(choices.begin(), choices.end(), [](const RnNodeChoice& lhs, const RnNodeChoice& rhs) {
        return lhs.nodeId < rhs.nodeId;
    });
    return choices;
}

void TraceCacheLineMinimap::clearLanes()
{
    stopBuildThread();
    lanes_.clear();
    statusText_.clear();
    resetBuildProgress();
    updateOverlayGeometry();
    update();
}

int TraceCacheLineMinimap::laneCount() const noexcept
{
    return static_cast<int>(lanes_.size());
}

QString TraceCacheLineMinimap::statusText() const
{
    return statusText_;
}

bool TraceCacheLineMinimap::parseIntegerText(QString text, std::uint64_t& value)
{
    return parseIntegerToken(std::move(text), value);
}

bool TraceCacheLineMinimap::eventFilter(QObject* watched, QEvent* event)
{
    if (!table_) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == table_ || watched == table_->viewport() || watched == table_->verticalScrollBar()) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::LayoutRequest:
            QTimer::singleShot(0, this, [this]() {
                updateOverlayGeometry();
                rebuildVisibleSegments();
                update();
            });
            break;
        case QEvent::Wheel:
        case QEvent::Paint:
            if (watched == table_->viewport()) {
                QTimer::singleShot(0, this, [this]() {
                    rebuildVisibleSegments();
                    update();
                });
            }
            break;
        case QEvent::FocusIn:
            focused_ = true;
            update();
            break;
        case QEvent::FocusOut:
            focused_ = false;
            update();
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void TraceCacheLineMinimap::paintEvent(QPaintEvent*)
{
    if (!mapVisible_) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal opacity = fadeWhenInactive_ && !hovered_ && !focused_ ? 0.42 : 0.88;
    painter.setOpacity(opacity);

    const int total = static_cast<int>(lanes_.size());
    const QFontMetrics metrics(font());
    for (int index = 0; index < total; ++index) {
        const Lane& lane = lanes_[static_cast<std::size_t>(index)];
        const QRect laneRect = lane.laneRect.isValid() ? lane.laneRect : laneRectForIndex(index, total);
        if (!laneRect.isValid()) {
            continue;
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(withAlpha(QColor(QStringLiteral("#FFFFFF")), 110));
        painter.drawRect(laneRect);

        if (lane.building) {
            painter.setPen(kMinimapTextColor);
            painter.drawText(laneRect.adjusted(-18, 0, 18, 0),
                             Qt::AlignCenter,
                             QStringLiteral("..."));
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(withAlpha(kLaneBorderColor, 190), 1));
            painter.drawRect(laneRect.adjusted(0, 0, -1, -1));
            continue;
        }

        for (const Segment& segment : lane.segments) {
            if (!segment.rect.isValid() || !segment.rect.intersects(rect())) {
                continue;
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(segment.color);
            painter.drawRect(segment.rect);
            if (segment.splitBefore) {
                painter.setPen(QPen(withAlpha(kLaneBorderColor, 210), 1));
                painter.drawLine(laneRect.left() + 1,
                                 segment.rect.top(),
                                 laneRect.right() - 1,
                                 segment.rect.top());
            }
            if (segment.rect.height() >= 16 && segment.rect.width() >= 10) {
                painter.setPen(kMinimapTextColor);
                QFont stateFont = font();
                stateFont.setBold(true);
                painter.setFont(stateFont);
                painter.save();
                painter.translate(segment.rect.center());
                painter.rotate(-90.0);
                const QRect textRect(-segment.rect.height() / 2,
                                     -segment.rect.width() / 2,
                                     segment.rect.height(),
                                     segment.rect.width());
                painter.drawText(textRect,
                                 Qt::AlignCenter,
                                 metrics.elidedText(segment.stateText, Qt::ElideRight, textRect.width()));
                painter.restore();
                painter.setFont(font());
            }
        }

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(withAlpha(kLaneBorderColor, 190), 1));
        painter.drawRect(laneRect.adjusted(0, 0, -1, -1));
    }

    for (int index = 0; index < total; ++index) {
        const Lane& lane = lanes_[static_cast<std::size_t>(index)];
        if (!lane.tagRect.isValid()) {
            continue;
        }

        QLinearGradient tagGradient(lane.tagRect.topLeft(), lane.tagRect.topRight());
        const QColor tagBase = QColor::fromHsv((static_cast<int>(lane.rnNodeId) * 47 + index * 23) % 360,
                                               95,
                                               210);
        tagGradient.setColorAt(0.0, withAlpha(tagBase.lighter(130), 230));
        tagGradient.setColorAt(1.0, withAlpha(tagBase.darker(115), 230));
        painter.setPen(QPen(QColor(QStringLiteral("#111827")), 1));
        painter.setBrush(tagGradient);
        painter.drawRoundedRect(lane.tagRect, 4, 4);
        painter.setFont(font());
        painter.setPen(kMinimapTextColor);
        painter.drawText(lane.tagRect.adjusted(4, 0, -4, 0),
                         Qt::AlignCenter,
                         metrics.elidedText(laneTagText(lane), Qt::ElideRight, lane.tagRect.width() - 8));
    }

    paintBuildProgress(painter);
}

void TraceCacheLineMinimap::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        const int laneIndex = laneIndexAt(event->pos());
        if (laneIndex >= 0) {
            showLaneContextMenu(laneIndex, event->pos(), event->globalPosition().toPoint());
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    activateSegmentAt(event->pos());
    event->accept();
}

void TraceCacheLineMinimap::mouseMoveEvent(QMouseEvent* event)
{
    hovered_ = true;
    const int row = segmentRowAt(event->pos());
    if (row >= 0) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           QStringLiteral("Jump to row %1").arg(row + 1),
                           this);
    }
    update();
}

void TraceCacheLineMinimap::leaveEvent(QEvent* event)
{
    hovered_ = false;
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(event);
}

void TraceCacheLineMinimap::focusInEvent(QFocusEvent* event)
{
    focused_ = true;
    update();
    QWidget::focusInEvent(event);
}

void TraceCacheLineMinimap::focusOutEvent(QFocusEvent* event)
{
    focused_ = false;
    update();
    QWidget::focusOutEvent(event);
}

void TraceCacheLineMinimap::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildVisibleSegments();
}

void TraceCacheLineMinimap::stopBuildThread()
{
    if (buildThread_.joinable()) {
        buildThread_.request_stop();
        buildThread_.join();
    }
    generation_.fetch_add(1, std::memory_order_relaxed);
    resetBuildProgress();
}

void TraceCacheLineMinimap::updateOverlayGeometry()
{
    if (!table_ || !table_->verticalScrollBar()) {
        hide();
        return;
    }

    const QRect target = overlayRectForCurrentTable();
    if (geometry() != target) {
        setGeometry(target);
    }
    setVisible(mapVisible_);
    if (mapVisible_) {
        raise();
    }
    const int total = static_cast<int>(lanes_.size());
    for (int index = 0; index < total; ++index) {
        Lane& lane = lanes_[static_cast<std::size_t>(index)];
        lane.laneRect = laneRectForIndex(index, total);
        lane.tagRect = tagRectForIndex(index, lane.laneRect);
    }
}

void TraceCacheLineMinimap::updateModelConnections()
{
    QObject::disconnect(modelResetConnection_);
    QObject::disconnect(rowsInsertedConnection_);
    QObject::disconnect(rowsRemovedConnection_);
    QObject::disconnect(dataChangedConnection_);
    if (!model_) {
        return;
    }
    QAbstractItemModel* abstractModel = model_;
    const auto refreshFromModel = [this]() {
        if (modelUpdatesSuspended_ > 0) {
            pendingModelRefresh_ = true;
            return;
        }
        rebuildVisibleSegments();
        update();
    };
    modelResetConnection_ = connect(abstractModel, &QAbstractItemModel::modelReset, this, refreshFromModel);
    rowsInsertedConnection_ = connect(abstractModel, &QAbstractItemModel::rowsInserted, this, refreshFromModel);
    rowsRemovedConnection_ = connect(abstractModel, &QAbstractItemModel::rowsRemoved, this, refreshFromModel);
    dataChangedConnection_ = connect(abstractModel, &QAbstractItemModel::dataChanged, this, refreshFromModel);
}

void TraceCacheLineMinimap::startLaneBuild(const int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return;
    }
    if (hostMode_ == HostMode::Clipboard) {
        startClipboardLaneBuild(laneIndex);
        return;
    }
    if (!traceSession_) {
        Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
        lane.failed = true;
        lane.building = false;
        lane.hasBuildResult = false;
        lane.statusText = QStringLiteral("No trace session");
        rebuildVisibleSegments();
        update();
        return;
    }

    stopBuildThread();
    Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    lane.building = true;
    lane.failed = false;
    lane.sourceSpans.clear();
    lane.segments.clear();
    lane.hasBuildResult = false;
    generation_.fetch_add(1, std::memory_order_relaxed);
    lane.generation = generation_.load(std::memory_order_relaxed);
    const quint64 generation = lane.generation;
    const std::uint32_t rnNodeId = lane.rnNodeId;
    const std::uint64_t lineAddress = lane.lineAddress;
    std::shared_ptr<TraceSession> session = traceSession_;
    beginBuildProgress(generation, session ? session->totalRows() : 0);

    buildThread_ = std::jthread([this, session = std::move(session), generation, rnNodeId, lineAddress](std::stop_token stopToken) {
        const auto progressCallback = [this, generation](const std::size_t completedRows,
                                                         const std::size_t totalRows) {
            postBuildProgress(generation,
                              static_cast<std::uint64_t>(completedRows),
                              static_cast<std::uint64_t>(totalRows));
        };
        std::shared_ptr<BuildResult> result = buildLane(session,
                                                        generation,
                                                        rnNodeId,
                                                        lineAddress,
                                                        stopToken,
                                                        progressCallback);
        QMetaObject::invokeMethod(this, [this, result = std::move(result)]() mutable {
            std::vector<std::shared_ptr<BuildResult>> results;
            results.push_back(std::move(result));
            applyBuildResults(std::move(results));
        }, Qt::QueuedConnection);
    });
}

void TraceCacheLineMinimap::startAllLaneBuilds(const bool onlyPending)
{
    stopBuildThread();
    if (hostMode_ == HostMode::Clipboard) {
        rebuildVisibleSegments();
        return;
    }
    if (!traceSession_) {
        for (Lane& lane : lanes_) {
            lane.failed = true;
            lane.building = false;
            lane.hasBuildResult = false;
            lane.statusText = QStringLiteral("No trace session");
        }
        resetBuildProgress();
        rebuildVisibleSegments();
        update();
        return;
    }
    if (lanes_.empty()) {
        return;
    }

    const quint64 generation = generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    struct LaneBuildQuery {
        std::uint32_t rnNodeId = 0;
        std::uint64_t lineAddress = 0;
    };
    std::vector<LaneBuildQuery> queries;
    queries.reserve(lanes_.size());
    for (int laneIndex = 0; laneIndex < static_cast<int>(lanes_.size()); ++laneIndex) {
        Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
        if (onlyPending && lane.hasBuildResult && !lane.building) {
            continue;
        }
        lane.building = true;
        lane.failed = false;
        lane.sourceSpans.clear();
        lane.segments.clear();
        lane.hasBuildResult = false;
        lane.generation = generation;
        queries.push_back(LaneBuildQuery{
            .rnNodeId = lane.rnNodeId,
            .lineAddress = lane.lineAddress,
        });
    }
    if (queries.empty()) {
        resetBuildProgress();
        rebuildVisibleSegments();
        update();
        return;
    }
    std::shared_ptr<TraceSession> session = traceSession_;
    const std::uint64_t totalBuildRows =
        session ? session->totalRows() * static_cast<std::uint64_t>(queries.size()) : 0;
    beginBuildProgress(generation, totalBuildRows);
    buildThread_ = std::jthread([this,
                                 session = std::move(session),
                                 generation,
                                 totalBuildRows,
                                 queries = std::move(queries)](std::stop_token stopToken) {
        std::vector<std::shared_ptr<BuildResult>> results;
        results.reserve(queries.size());
        const std::uint64_t rowsPerBuild = session ? session->totalRows() : 0;
        std::uint64_t completedBeforeBuild = 0;
        for (const LaneBuildQuery& query : queries) {
            if (stopToken.stop_requested()) {
                break;
            }
            const auto progressCallback = [this, generation, completedBeforeBuild, rowsPerBuild, totalBuildRows](
                                              const std::size_t completedRows,
                                              const std::size_t totalRows) {
                const std::uint64_t currentTotal = rowsPerBuild == 0
                    ? static_cast<std::uint64_t>(totalRows)
                    : rowsPerBuild;
                postBuildProgress(generation,
                                  completedBeforeBuild + static_cast<std::uint64_t>(completedRows),
                                  totalBuildRows == 0
                                      ? completedBeforeBuild + currentTotal
                                      : totalBuildRows);
            };
            results.push_back(buildLane(session,
                                        generation,
                                        query.rnNodeId,
                                        query.lineAddress,
                                        stopToken,
                                        progressCallback));
            completedBeforeBuild += rowsPerBuild;
            postBuildProgress(generation,
                              completedBeforeBuild,
                              totalBuildRows == 0 ? completedBeforeBuild : totalBuildRows);
        }
        QMetaObject::invokeMethod(this, [this, results = std::move(results)]() mutable {
            applyBuildResults(std::move(results));
        }, Qt::QueuedConnection);
    });
}

void TraceCacheLineMinimap::startClipboardLaneBuild(const int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return;
    }
    stopBuildThread();
    Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    lane.building = true;
    lane.failed = false;
    lane.sourceSpanGroups.clear();
    lane.segments.clear();
    lane.hasBuildResult = false;
    generation_.fetch_add(1, std::memory_order_relaxed);
    const quint64 generation = generation_.load(std::memory_order_relaxed);
    lane.generation = generation;
    const std::uint32_t rnNodeId = lane.rnNodeId;
    const std::uint64_t lineAddress = lane.lineAddress;

    std::vector<std::pair<quint64, std::shared_ptr<TraceSession>>> sources;
    if (clipboard_ && sourceSessionResolver_) {
        const std::vector<ClipboardEntry> entries = clipboard_->currentEntriesSnapshot();
        QSet<quint64> seen;
        for (const ClipboardEntry& entry : entries) {
            if (entry.source.sessionId == 0 || seen.contains(entry.source.sessionId)) {
                continue;
            }
            std::shared_ptr<TraceSession> source = sourceSessionResolver_(entry.source.sessionId);
            if (!source) {
                continue;
            }
            seen.insert(entry.source.sessionId);
            sources.push_back({entry.source.sessionId, std::move(source)});
        }
    }
    if (sources.empty()) {
        lane.building = false;
        lane.failed = true;
        lane.hasBuildResult = false;
        lane.statusText = QStringLiteral("No source trace session");
        resetBuildProgress();
        rebuildVisibleSegments();
        update();
        return;
    }

    std::uint64_t totalRows = 0;
    for (const auto& [sessionId, session] : sources) {
        Q_UNUSED(sessionId);
        if (session) {
            totalRows += session->totalRows();
        }
    }
    beginBuildProgress(generation, totalRows);
    const std::uint64_t totalBuildRows = totalRows;
    buildThread_ = std::jthread([this,
                                 generation,
                                 rnNodeId,
                                 lineAddress,
                                 totalBuildRows,
                                 sources = std::move(sources)](std::stop_token stopToken) {
        std::vector<std::shared_ptr<BuildResult>> results;
        results.reserve(sources.size());
        std::uint64_t completedBeforeBuild = 0;
        for (const auto& [sessionId, session] : sources) {
            if (stopToken.stop_requested()) {
                break;
            }
            const std::uint64_t rowsPerBuild = session ? session->totalRows() : 0;
            const auto progressCallback = [this, generation, completedBeforeBuild, rowsPerBuild, totalBuildRows](
                                              const std::size_t completedRows,
                                              const std::size_t totalRows) {
                const std::uint64_t currentTotal = rowsPerBuild == 0
                    ? static_cast<std::uint64_t>(totalRows)
                    : rowsPerBuild;
                postBuildProgress(generation,
                                  completedBeforeBuild + static_cast<std::uint64_t>(completedRows),
                                  totalBuildRows == 0
                                      ? completedBeforeBuild + currentTotal
                                      : totalBuildRows);
            };
            std::shared_ptr<BuildResult> result = buildLane(session,
                                                            generation,
                                                            rnNodeId,
                                                            lineAddress,
                                                            stopToken,
                                                            progressCallback);
            result->sourceSessionId = sessionId;
            results.push_back(std::move(result));
            completedBeforeBuild += rowsPerBuild;
            postBuildProgress(generation,
                              completedBeforeBuild,
                              totalBuildRows == 0 ? completedBeforeBuild : totalBuildRows);
        }
        QMetaObject::invokeMethod(this, [this, results = std::move(results)]() mutable {
            applyBuildResults(std::move(results));
        }, Qt::QueuedConnection);
    });
}

void TraceCacheLineMinimap::startAllClipboardLaneBuilds(const bool onlyPending)
{
    if (hostMode_ != HostMode::Clipboard) {
        return;
    }
    stopBuildThread();
    if (lanes_.empty()) {
        return;
    }

    const quint64 generation = generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    struct LaneBuildQuery {
        std::uint32_t rnNodeId = 0;
        std::uint64_t lineAddress = 0;
    };
    std::vector<LaneBuildQuery> queries;
    queries.reserve(lanes_.size());
    for (int laneIndex = 0; laneIndex < static_cast<int>(lanes_.size()); ++laneIndex) {
        Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
        if (onlyPending && lane.hasBuildResult && !lane.building) {
            continue;
        }
        lane.building = true;
        lane.failed = false;
        lane.sourceSpanGroups.clear();
        lane.segments.clear();
        lane.hasBuildResult = false;
        lane.generation = generation;
        queries.push_back(LaneBuildQuery{
            .rnNodeId = lane.rnNodeId,
            .lineAddress = lane.lineAddress,
        });
    }
    if (queries.empty()) {
        resetBuildProgress();
        rebuildVisibleSegments();
        update();
        return;
    }

    std::vector<std::pair<quint64, std::shared_ptr<TraceSession>>> sources;
    if (clipboard_ && sourceSessionResolver_) {
        const std::vector<ClipboardEntry> entries = clipboard_->currentEntriesSnapshot();
        QSet<quint64> seen;
        for (const ClipboardEntry& entry : entries) {
            if (entry.source.sessionId == 0 || seen.contains(entry.source.sessionId)) {
                continue;
            }
            std::shared_ptr<TraceSession> source = sourceSessionResolver_(entry.source.sessionId);
            if (!source) {
                continue;
            }
            seen.insert(entry.source.sessionId);
            sources.push_back({entry.source.sessionId, std::move(source)});
        }
    }
    if (sources.empty()) {
        for (Lane& lane : lanes_) {
            lane.building = false;
            lane.failed = true;
            lane.hasBuildResult = false;
            lane.statusText = QStringLiteral("No source trace session");
        }
        resetBuildProgress();
        rebuildVisibleSegments();
        update();
        return;
    }

    std::uint64_t totalRows = 0;
    for (const auto& [sessionId, session] : sources) {
        Q_UNUSED(sessionId);
        if (session) {
            totalRows += session->totalRows();
        }
    }
    beginBuildProgress(generation, totalRows * static_cast<std::uint64_t>(queries.size()));
    buildThread_ = std::jthread([this, generation, queries = std::move(queries), sources = std::move(sources)](std::stop_token stopToken) {
        std::vector<std::shared_ptr<BuildResult>> results;
        results.reserve(queries.size() * std::max<std::size_t>(1, sources.size()));
        std::uint64_t completedBeforeBuild = 0;
        std::uint64_t totalBuildRows = 0;
        for (const auto& [sessionId, session] : sources) {
            Q_UNUSED(sessionId);
            if (session) {
                totalBuildRows += session->totalRows();
            }
        }
        totalBuildRows *= static_cast<std::uint64_t>(queries.size());
        for (const LaneBuildQuery& query : queries) {
            for (const auto& [sessionId, session] : sources) {
                if (stopToken.stop_requested()) {
                    return;
                }
                const std::uint64_t rowsPerBuild = session ? session->totalRows() : 0;
                const auto progressCallback = [this, generation, completedBeforeBuild, rowsPerBuild, totalBuildRows](
                                                  const std::size_t completedRows,
                                                  const std::size_t totalRows) {
                    const std::uint64_t currentTotal = rowsPerBuild == 0
                        ? static_cast<std::uint64_t>(totalRows)
                        : rowsPerBuild;
                    postBuildProgress(generation,
                                      completedBeforeBuild + static_cast<std::uint64_t>(completedRows),
                                      totalBuildRows == 0
                                          ? completedBeforeBuild + currentTotal
                                          : totalBuildRows);
                };
                std::shared_ptr<BuildResult> result = buildLane(session,
                                                                generation,
                                                                query.rnNodeId,
                                                                query.lineAddress,
                                                                stopToken,
                                                                progressCallback);
                result->sourceSessionId = sessionId;
                results.push_back(std::move(result));
                completedBeforeBuild += rowsPerBuild;
                postBuildProgress(generation, completedBeforeBuild, totalBuildRows);
            }
        }
        QMetaObject::invokeMethod(this, [this, results = std::move(results)]() mutable {
            applyBuildResults(std::move(results));
        }, Qt::QueuedConnection);
    });
}

void TraceCacheLineMinimap::applyBuildResult(std::shared_ptr<BuildResult> result)
{
    if (!result || result->generation != generation_.load(std::memory_order_relaxed)) {
        return;
    }

    for (Lane& lane : lanes_) {
        if (lane.rnNodeId != result->rnNodeId || lane.lineAddress != result->lineAddress) {
            continue;
        }
        lane.building = false;
        lane.failed = result->failed;
        lane.hasBuildResult = !result->cancelled;
        if (hostMode_ == HostMode::Clipboard) {
            auto found = std::find_if(lane.sourceSpanGroups.begin(),
                                      lane.sourceSpanGroups.end(),
                                      [&](const SourceSpanGroup& group) {
                                          return group.sessionId == result->sourceSessionId;
                                      });
            if (found == lane.sourceSpanGroups.end()) {
                lane.sourceSpanGroups.push_back(SourceSpanGroup{
                    .sessionId = result->sourceSessionId,
                    .spans = std::move(result->spans),
                });
            } else {
                found->spans = std::move(result->spans);
            }
        } else {
            lane.sourceSpans = std::move(result->spans);
        }
        lane.statusText = result->statusText;
        if (!lane.sourceSpans.empty()) {
            lane.rnNodeType = lane.sourceSpans.front().rnNodeType;
        } else if (!lane.sourceSpanGroups.empty() && !lane.sourceSpanGroups.front().spans.empty()) {
            lane.rnNodeType = lane.sourceSpanGroups.front().spans.front().rnNodeType;
        }
        break;
    }
    statusText_ = result->statusText;
    rebuildVisibleSegments();
    update();
}

void TraceCacheLineMinimap::applyBuildResults(std::vector<std::shared_ptr<BuildResult>> results)
{
    quint64 completedGeneration = 0;
    for (std::shared_ptr<BuildResult>& result : results) {
        if (result && !result->cancelled) {
            completedGeneration = result->generation;
        }
        applyBuildResult(std::move(result));
    }
    if (completedGeneration != 0) {
        finishBuildProgress(completedGeneration);
    }
}

std::shared_ptr<TraceCacheLineMinimap::BuildResult> TraceCacheLineMinimap::buildLane(
    std::shared_ptr<TraceSession> session,
    const quint64 generation,
    const std::uint32_t rnNodeId,
    const std::uint64_t lineAddress,
    std::stop_token stopToken,
    const std::function<void(std::size_t completedRows,
                             std::size_t totalRows)>& progressCallback) const
{
    auto result = std::make_shared<BuildResult>();
    result->generation = generation;
    result->rnNodeId = rnNodeId;
    result->lineAddress = lineAddress;
    if (!session) {
        result->failed = true;
        result->statusText = QStringLiteral("Cache Map unavailable: no trace session.");
        return result;
    }
    if (session->metadata().parameters.GetIssue() != CLog::Issue::Eb) {
        result->failed = true;
        result->statusText = QStringLiteral("Cache Map unavailable: only CHI E.b traces expose RN cache-state replay.");
        return result;
    }

    CLogBTraceLoadError error;
    CLogBTraceLoadCallbacks callbacks;
    callbacks.stageProgress = [progressCallback](const std::size_t completed, const std::size_t total) {
        if (progressCallback) {
            progressCallback(completed, total);
        }
    };

    const bool ok = CLogBTraceLoader::buildCacheLineStateSpans(
        session->filePath(),
        session->metadata(),
        CLogBTraceCacheLineStateQuery{.rnNodeId = rnNodeId, .lineAddress = lineAddress},
        result->spans,
        error,
        callbacks,
        stopToken);
    if (stopToken.stop_requested()) {
        result->cancelled = true;
        return result;
    }
    if (!ok) {
        result->failed = true;
        result->statusText = error.summary.isEmpty()
            ? QStringLiteral("Cache Map failed to replay cache-line state.")
            : error.summary;
        return result;
    }
    result->statusText = result->spans.empty()
        ? QStringLiteral("Cache Map: no state spans for RN %1 %2.")
              .arg(rnNodeId)
              .arg(addressText(lineAddress))
        : QStringLiteral("Cache Map: %1 state span%2 for RN %3 %4.")
              .arg(result->spans.size())
              .arg(result->spans.size() == 1 ? QString() : QStringLiteral("s"))
              .arg(rnNodeId)
              .arg(addressText(lineAddress));
    return result;
}

void TraceCacheLineMinimap::rebuildVisibleSegments()
{
    updateOverlayGeometry();
    for (Lane& lane : lanes_) {
        lane.segments.clear();
        if (hostMode_ == HostMode::Clipboard) {
            rebuildClipboardLane(lane);
        } else {
            rebuildMainTraceLane(lane);
        }
    }
}

std::vector<CLogBTraceCacheLineStateSpan> TraceCacheLineMinimap::coalescedSourceSpans(
    const std::vector<CLogBTraceCacheLineStateSpan>& spans) const
{
    std::vector<CLogBTraceCacheLineStateSpan> sorted = spans;
    std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.startLogicalRow != rhs.startLogicalRow) {
            return lhs.startLogicalRow < rhs.startLogicalRow;
        }
        if (lhs.endLogicalRow != rhs.endLogicalRow) {
            return lhs.endLogicalRow < rhs.endLogicalRow;
        }
        return lhs.startTimestamp < rhs.startTimestamp;
    });

    std::vector<CLogBTraceCacheLineStateSpan> coalesced;
    coalesced.reserve(sorted.size());
    for (const CLogBTraceCacheLineStateSpan& span : sorted) {
        if (!coalesced.empty() && segmentsCanMerge(coalesced.back(), span)) {
            CLogBTraceCacheLineStateSpan& pending = coalesced.back();
            pending.endLogicalRow = std::max(pending.endLogicalRow, span.endLogicalRow);
            pending.endTimestamp = std::max(pending.endTimestamp, span.endTimestamp);
            pending.openEnded = pending.openEnded || span.openEnded;
            continue;
        }
        coalesced.push_back(span);
    }
    return coalesced;
}

void TraceCacheLineMinimap::rebuildMainTraceLane(Lane& lane) const
{
    if (!model_ || lane.sourceSpans.empty()) {
        return;
    }

    for (const CLogBTraceCacheLineStateSpan& span : coalescedSourceSpans(lane.sourceSpans)) {
        const auto startVisible = visibleRowForSourceRow(lane, span.startLogicalRow);
        const auto endVisible = visibleRowForSourceRow(lane, span.endLogicalRow);
        if (startVisible && endVisible) {
            Segment segment;
            segment.sourceStartLogicalRow = span.startLogicalRow;
            segment.sourceEndLogicalRow = span.endLogicalRow;
            segment.startVisibleRow = *startVisible;
            segment.endVisibleRow = *endVisible;
            segment.startTimestamp = span.startTimestamp;
            segment.endTimestamp = span.endTimestamp;
            segment.stateMask = span.stateMask;
            segment.stateText = span.stateText;
            segment.openEnded = span.openEnded;
            segment.color = colorForState(segment.stateMask, segment.stateText, segment.openEnded);
            const int top = rowY(segment.startVisibleRow);
            const int bottom = rowBottomY(segment.endVisibleRow);
            segment.rect = QRect(lane.laneRect.left() + 2,
                                 top,
                                 qMax(1, lane.laneRect.width() - 4),
                                 qMax(1, bottom - top + 1)).intersected(lane.laneRect);
            lane.segments.push_back(std::move(segment));
        }
    }
}

void TraceCacheLineMinimap::rebuildClipboardLane(Lane& lane) const
{
    if (!clipboard_ || !model_) {
        return;
    }
    struct SpanLookup {
        std::vector<CLogBTraceCacheLineStateSpan> spans;
        std::size_t cursor = 0;
    };

    std::unordered_map<quint64, SpanLookup> spanLookups;
    spanLookups.reserve(lane.sourceSpanGroups.size());
    for (const SourceSpanGroup& group : lane.sourceSpanGroups) {
        SpanLookup lookup;
        lookup.spans = coalescedSourceSpans(group.spans);
        if (!lookup.spans.empty()) {
            spanLookups.emplace(group.sessionId, std::move(lookup));
        }
    }

    auto findSpan = [](SpanLookup& lookup,
                       const std::uint64_t sourceRow) -> std::pair<const CLogBTraceCacheLineStateSpan*, std::size_t> {
        while (lookup.cursor < lookup.spans.size()
               && lookup.spans[lookup.cursor].endLogicalRow < sourceRow) {
            ++lookup.cursor;
        }
        if (lookup.cursor < lookup.spans.size()
            && lookup.spans[lookup.cursor].startLogicalRow <= sourceRow
            && sourceRow <= lookup.spans[lookup.cursor].endLogicalRow) {
            return {&lookup.spans[lookup.cursor], lookup.cursor};
        }

        const auto found = std::lower_bound(lookup.spans.cbegin(),
                                            lookup.spans.cend(),
                                            sourceRow,
                                            [](const auto& span, const std::uint64_t row) {
                                                return span.endLogicalRow < row;
                                            });
        if (found != lookup.spans.cend()
            && found->startLogicalRow <= sourceRow
            && sourceRow <= found->endLogicalRow) {
            const std::size_t index = static_cast<std::size_t>(std::distance(lookup.spans.cbegin(), found));
            lookup.cursor = index;
            return {&lookup.spans[index], index};
        }
        return {nullptr, 0};
    };

    struct ClipboardProjectedRow {
        quint64 sessionId = 0;
        std::uint64_t sourceLogicalRow = 0;
        const CLogBTraceCacheLineStateSpan* span = nullptr;
        std::size_t spanIndex = 0;
        int visibleRow = -1;
    };

    auto flushProjected = [&](const ClipboardProjectedRow& first,
                              const ClipboardProjectedRow& last,
                              const bool splitBefore) {
        if (!first.span) {
            return;
        }
        Segment segment;
        segment.sourceSessionId = first.sessionId;
        segment.sourceStartLogicalRow = first.span->startLogicalRow;
        segment.sourceEndLogicalRow = first.span->endLogicalRow;
        segment.startVisibleRow = first.visibleRow;
        segment.endVisibleRow = last.visibleRow;
        segment.startTimestamp = first.span->startTimestamp;
        segment.endTimestamp = first.span->endTimestamp;
        segment.stateMask = first.span->stateMask;
        segment.stateText = first.span->stateText;
        segment.openEnded = first.span->openEnded;
        segment.splitBefore = splitBefore;
        segment.color = colorForState(segment.stateMask, segment.stateText, segment.openEnded);
        const int top = rowY(segment.startVisibleRow);
        const int bottom = rowBottomY(segment.endVisibleRow);
        segment.rect = QRect(lane.laneRect.left() + 2,
                             top,
                             qMax(1, lane.laneRect.width() - 4),
                             qMax(1, bottom - top + 1)).intersected(lane.laneRect);
        lane.segments.push_back(std::move(segment));
    };

    const int visibleCount = model_->visibleCount();
    ClipboardProjectedRow firstPending;
    ClipboardProjectedRow lastPending;
    bool havePending = false;
    bool nextSplitBefore = false;
    for (int visibleRow = 0; visibleRow < visibleCount; ++visibleRow) {
        const std::optional<ClipboardEntry> entry = clipboard_->entryForVisibleRow(visibleRow);
        if (!entry || entry->source.sessionId == 0 || entry->source.logicalRow < 0) {
            continue;
        }
        auto lookupFound = spanLookups.find(entry->source.sessionId);
        if (lookupFound == spanLookups.end()) {
            continue;
        }
        const std::uint64_t sourceRow = static_cast<std::uint64_t>(entry->source.logicalRow);
        const auto [span, spanIndex] = findSpan(lookupFound->second, sourceRow);
        if (!span) {
            continue;
        }
        const ClipboardProjectedRow current{
            .sessionId = entry->source.sessionId,
            .sourceLogicalRow = sourceRow,
            .span = span,
            .spanIndex = spanIndex,
            .visibleRow = visibleRow,
        };
        if (!havePending) {
            firstPending = current;
            lastPending = current;
            havePending = true;
            nextSplitBefore = false;
            continue;
        }

        const bool sameSpan = current.sessionId == lastPending.sessionId
            && current.spanIndex == lastPending.spanIndex
            && current.span == lastPending.span
            && current.visibleRow == lastPending.visibleRow + 1;
        if (sameSpan) {
            lastPending = current;
            continue;
        }

        flushProjected(firstPending, lastPending, nextSplitBefore);
        nextSplitBefore = current.sessionId == lastPending.sessionId;
        firstPending = current;
        lastPending = current;
    }
    if (havePending) {
        flushProjected(firstPending, lastPending, nextSplitBefore);
    }
}

std::optional<int> TraceCacheLineMinimap::visibleRowForSourceRow(const Lane&, const std::uint64_t sourceLogicalRow) const
{
    if (!model_ || sourceLogicalRow > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
        return std::nullopt;
    }
    const int visibleRow = model_->visibleRowForLogicalRow(static_cast<int>(sourceLogicalRow));
    return visibleRow >= 0 ? std::optional<int>(visibleRow) : std::nullopt;
}

int TraceCacheLineMinimap::sourceLogicalRowForClipboardVisibleRow(const int visibleRow) const
{
    if (!clipboard_) {
        return -1;
    }
    const std::optional<ClipboardEntry> entry = clipboard_->entryForVisibleRow(visibleRow);
    return entry ? entry->source.logicalRow : -1;
}

int TraceCacheLineMinimap::laneIndexAt(const QPoint& position) const
{
    for (int index = 0; index < static_cast<int>(lanes_.size()); ++index) {
        const Lane& lane = lanes_[static_cast<std::size_t>(index)];
        if (lane.laneRect.adjusted(-2, 0, 2, 0).contains(position)
            || lane.tagRect.contains(position)) {
            return index;
        }
    }
    return -1;
}

int TraceCacheLineMinimap::tagIndexAt(const QPoint& position) const
{
    for (int index = static_cast<int>(lanes_.size()) - 1; index >= 0; --index) {
        const Lane& lane = lanes_[static_cast<std::size_t>(index)];
        if (lane.tagRect.contains(position)) {
            return index;
        }
    }
    return -1;
}

int TraceCacheLineMinimap::rowAtPositionForJump(const QPoint& position) const
{
    if (!model_ || model_->visibleCount() <= 0) {
        return 0;
    }
    if (!table_) {
        return qBound(0, position.y(), model_->visibleCount() - 1);
    }

    for (int visibleRow = 0; visibleRow < model_->visibleCount(); ++visibleRow) {
        const int top = rowY(visibleRow);
        const int bottom = rowBottomY(visibleRow);
        if (position.y() >= top && position.y() <= bottom) {
            return visibleRow;
        }
        if (position.y() < top) {
            return qMax(0, visibleRow - 1);
        }
    }
    return model_->visibleCount() - 1;
}

int TraceCacheLineMinimap::segmentRowAt(const QPoint& position) const
{
    for (const Lane& lane : lanes_) {
        for (const Segment& segment : lane.segments) {
            if (segment.rect.adjusted(-4, -1, 4, 1).contains(position)) {
                return hostMode_ == HostMode::Clipboard
                    ? segment.startVisibleRow
                    : static_cast<int>(segment.sourceStartLogicalRow);
            }
        }
    }
    return -1;
}

void TraceCacheLineMinimap::removeLaneAt(const int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return;
    }

    const bool removedLaneWasBuilding = lanes_[static_cast<std::size_t>(laneIndex)].building;
    lanes_.erase(lanes_.begin() + laneIndex);
    statusText_.clear();
    updateOverlayGeometry();
    if (lanes_.empty()) {
        stopBuildThread();
        setMapVisible(false);
    } else if (removedLaneWasBuilding) {
        if (hostMode_ == HostMode::Clipboard) {
            startAllClipboardLaneBuilds(true);
        } else {
            startAllLaneBuilds(true);
        }
    }
    rebuildVisibleSegments();
    update();
}

void TraceCacheLineMinimap::showLaneContextMenu(const int laneIndex,
                                                const QPoint& position,
                                                const QPoint& globalPosition)
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return;
    }

    const int referenceRow = rowAtPositionForJump(position);
    QMenu menu(this);
    QMenu* jumpMenu = menu.addMenu(QStringLiteral("Jump To ..."));
    const std::array<std::pair<ChangeJumpDirection, QString>, 4> changeDirections{{
        {ChangeJumpDirection::First, QStringLiteral("First change")},
        {ChangeJumpDirection::Previous, QStringLiteral("Previous change")},
        {ChangeJumpDirection::Next, QStringLiteral("Next change")},
        {ChangeJumpDirection::Last, QStringLiteral("Last change")},
    }};
    for (const auto& [direction, label] : changeDirections) {
        QAction* action = jumpMenu->addAction(label);
        action->setEnabled(findChangeJumpTarget(laneIndex, direction, referenceRow).has_value());
        connect(action, &QAction::triggered, this, [this, laneIndex, direction, referenceRow]() {
            const std::optional<ChangeJumpTarget> target =
                findChangeJumpTarget(laneIndex, direction, referenceRow);
            if (target) {
                jumpToChangeTarget(*target);
            }
        });
    }
    jumpMenu->addSeparator();

    for (const CacheMapJumpStateSpec& stateSpec : cacheMapJumpStates()) {
        for (const CacheMapJumpDirectionSpec& directionSpec : cacheMapJumpDirections()) {
            QAction* action = jumpMenu->addAction(QStringLiteral("%1 %2")
                                                      .arg(directionSpec.label, stateSpec.label));
            action->setEnabled(findJumpSegment(laneIndex,
                                               stateSpec.state,
                                               directionSpec.direction,
                                               referenceRow)
                                   != nullptr);
            connect(action,
                    &QAction::triggered,
                    this,
                    [this, laneIndex, state = stateSpec.state, direction = directionSpec.direction, referenceRow]() {
                        const Segment* segment = findJumpSegment(laneIndex, state, direction, referenceRow);
                        if (segment) {
                            jumpToSegment(*segment);
                        }
                    });
        }
        if (stateSpec.state != cacheMapJumpStates().back().state) {
            jumpMenu->addSeparator();
        }
    }

    menu.addSeparator();
    QAction* deleteAction = menu.addAction(QStringLiteral("Delete"));
    QAction* selectedAction = menu.exec(globalPosition);
    if (selectedAction == deleteAction) {
        removeLaneAt(laneIndex);
    }
}

const TraceCacheLineMinimap::Segment* TraceCacheLineMinimap::findJumpSegment(
    const int laneIndex,
    const JumpState state,
    const JumpDirection direction,
    const int referenceRow) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return nullptr;
    }

    const std::uint8_t stateMask = cacheMapJumpStateMask(state);
    if (stateMask == 0) {
        return nullptr;
    }

    const Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    const Segment* result = nullptr;
    for (const Segment& segment : lane.segments) {
        if ((segment.stateMask & stateMask) == 0) {
            continue;
        }

        switch (direction) {
        case JumpDirection::First:
            return &segment;
        case JumpDirection::Previous:
            if (segment.endVisibleRow < referenceRow) {
                result = &segment;
            } else {
                return result;
            }
            break;
        case JumpDirection::Next:
            if (segment.startVisibleRow > referenceRow) {
                return &segment;
            }
            break;
        case JumpDirection::Last:
            result = &segment;
            break;
        }
    }
    return result;
}

std::vector<TraceCacheLineMinimap::ChangeJumpTarget>
TraceCacheLineMinimap::changeJumpTargetsForLane(const int laneIndex) const
{
    std::vector<ChangeJumpTarget> targets;
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return targets;
    }

    const Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    if (lane.segments.empty()) {
        return targets;
    }

    auto addTarget = [&targets](const Segment& segment,
                                const int visibleRow,
                                const std::uint64_t sourceLogicalRow) {
        if (visibleRow < 0) {
            return;
        }
        if (!targets.empty() && targets.back().visibleRow == visibleRow) {
            targets.back() = ChangeJumpTarget{&segment, visibleRow, sourceLogicalRow};
            return;
        }
        targets.push_back(ChangeJumpTarget{&segment, visibleRow, sourceLogicalRow});
    };

    for (std::size_t index = 0; index < lane.segments.size(); ++index) {
        const Segment& segment = lane.segments[index];
        addTarget(segment, segment.startVisibleRow, segment.sourceStartLogicalRow);

        if (segment.openEnded) {
            continue;
        }

        const bool nextSegmentStartsAtSameRow = index + 1 < lane.segments.size()
            && lane.segments[index + 1].startVisibleRow <= segment.endVisibleRow;
        if (!nextSegmentStartsAtSameRow) {
            addTarget(segment, segment.endVisibleRow, segment.sourceEndLogicalRow);
        }
    }
    return targets;
}

std::optional<TraceCacheLineMinimap::ChangeJumpTarget> TraceCacheLineMinimap::findChangeJumpTarget(
    const int laneIndex,
    const ChangeJumpDirection direction,
    const int referenceRow) const
{
    const std::vector<ChangeJumpTarget> targets = changeJumpTargetsForLane(laneIndex);
    if (targets.empty()) {
        return std::nullopt;
    }

    switch (direction) {
    case ChangeJumpDirection::First:
        return targets.front();
    case ChangeJumpDirection::Previous: {
        std::optional<ChangeJumpTarget> result;
        for (const ChangeJumpTarget& target : targets) {
            if (target.visibleRow < referenceRow) {
                result = target;
            } else {
                return result;
            }
        }
        return result;
    }
    case ChangeJumpDirection::Next:
        for (const ChangeJumpTarget& target : targets) {
            if (target.visibleRow > referenceRow) {
                return target;
            }
        }
        return std::nullopt;
    case ChangeJumpDirection::Last:
        return targets.back();
    }
    return std::nullopt;
}

bool TraceCacheLineMinimap::jumpToSegment(const Segment& segment)
{
    if (hostMode_ == HostMode::Clipboard) {
        if (segment.startVisibleRow >= 0) {
            Q_EMIT clipboardRowActivated(segment.startVisibleRow);
            return true;
        }
        return false;
    }

    if (segment.sourceStartLogicalRow <= static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
        Q_EMIT logicalRowActivated(static_cast<int>(segment.sourceStartLogicalRow));
        return true;
    }
    return false;
}

bool TraceCacheLineMinimap::jumpToChangeTarget(const ChangeJumpTarget& target)
{
    if (!target.segment) {
        return false;
    }

    if (hostMode_ == HostMode::Clipboard) {
        if (target.visibleRow >= 0) {
            Q_EMIT clipboardRowActivated(target.visibleRow);
            return true;
        }
        return false;
    }

    if (target.sourceLogicalRow <= static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
        Q_EMIT logicalRowActivated(static_cast<int>(target.sourceLogicalRow));
        return true;
    }
    return false;
}

void TraceCacheLineMinimap::activateSegmentAt(const QPoint& position)
{
    for (const Lane& lane : lanes_) {
        for (const Segment& segment : lane.segments) {
            if (!segment.rect.adjusted(-4, -1, 4, 1).contains(position)) {
                continue;
            }
            jumpToSegment(segment);
            return;
        }
    }
}

QRect TraceCacheLineMinimap::overlayRectForCurrentTable() const
{
    if (!table_ || !table_->verticalScrollBar()) {
        return QRect();
    }

    QRect scroll = table_->verticalScrollBar()->geometry();
    if (scroll.left() <= 0 || scroll.height() <= 0) {
        const QRect viewport = table_->viewport() ? table_->viewport()->geometry() : table_->rect();
        scroll = QRect(viewport.right() + 1,
                       viewport.top(),
                       qMax(1, table_->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, table_)),
                       viewport.height());
    }
    const QPoint tableOrigin = table_->mapTo(parentWidget(), QPoint(0, 0));
    const int laneAreaWidth =
        static_cast<int>(lanes_.size()) * kLaneWidth
        + qMax(0, static_cast<int>(lanes_.size()) - 1) * kLaneGap
        + 2 * kOuterMargin;
    const int tagAnchorWidth =
        kTagMaxWidth
        + 2 * kOuterMargin
        + qMax(0, static_cast<int>(lanes_.size()) - 1) * (kLaneWidth + kLaneGap);
    const int width = qBound(kOverlayBaseWidth,
                             qMax(laneAreaWidth + 22, tagAnchorWidth) + rightReservedWidth_,
                             kOverlayMaxWidth + rightReservedWidth_);
    const int overlayWidth = qMin(width, qMax(1, scroll.left()));
    const int x = tableOrigin.x() + scroll.left() - overlayWidth;
    const int y = tableOrigin.y() + scroll.top();
    return QRect(qMax(tableOrigin.x(), x), y, overlayWidth, qMax(1, scroll.height()));
}

QRect TraceCacheLineMinimap::laneRectForIndex(const int laneIndex, const int totalLanes) const
{
    if (totalLanes <= 0) {
        return QRect();
    }
    const int totalLaneWidth = totalLanes * kLaneWidth + (totalLanes - 1) * kLaneGap;
    const int contentRight = width() - rightReservedWidth_;
    const int left = contentRight - kOuterMargin - totalLaneWidth
        + laneIndex * (kLaneWidth + kLaneGap);
    return QRect(left,
                 0,
                 kLaneWidth,
                 qMax(1, height()));
}

QRect TraceCacheLineMinimap::tagRectForIndex(const int laneIndex, const QRect& laneRect) const
{
    const int maxWidthForLane = qMax(1, laneRect.right() - kOuterMargin + 1);
    const int desiredWidth = qMin(kTagMaxWidth, qMax(1, width() - 2 * kOuterMargin));
    const int tagWidth = qMax(1, qMin(desiredWidth, maxWidthForLane));
    const int right = laneRect.right();
    const int left = right - tagWidth + 1;
    return QRect(left,
                 kOuterMargin + laneIndex * (kTagHeight + kTagGap),
                 tagWidth,
                 kTagHeight);
}

QRect TraceCacheLineMinimap::buildProgressRect() const
{
    if (!buildProgressActive_ || width() <= 0 || height() <= 0) {
        return {};
    }

    const int progressWidth = qMin(kBuildProgressWidth, qMax(1, width() - 2 * kBuildProgressMargin));
    if (progressWidth < 48 || height() < kBuildProgressHeight + 2 * kBuildProgressMargin) {
        return {};
    }
    return QRect(width() - kBuildProgressMargin - progressWidth,
                 height() - kBuildProgressMargin - kBuildProgressHeight,
                 progressWidth,
                 kBuildProgressHeight);
}

int TraceCacheLineMinimap::rowY(const int visibleRow) const
{
    if (!table_ || visibleRow < 0) {
        return 0;
    }
    const int viewportY = table_->rowViewportPosition(visibleRow);
    const QPoint viewportOrigin = table_->viewport()->mapTo(parentWidget(), QPoint(0, 0));
    const QPoint selfOrigin = mapTo(parentWidget(), QPoint(0, 0));
    return viewportOrigin.y() - selfOrigin.y() + viewportY;
}

int TraceCacheLineMinimap::rowBottomY(const int visibleRow) const
{
    if (!table_ || visibleRow < 0) {
        return rowY(visibleRow);
    }
    return rowY(visibleRow) + qMax(1, table_->rowHeight(visibleRow)) - 1;
}

QColor TraceCacheLineMinimap::colorForState(const std::uint8_t stateMask,
                                            const QString& stateText,
                                            const bool openEnded) const
{
    Q_UNUSED(stateText);
    Q_UNUSED(openEnded);
    return cacheStateCombinationColor(stateMask);
}

QString TraceCacheLineMinimap::laneTagText(const Lane& lane) const
{
    return QStringLiteral("RN %1 %2").arg(lane.rnNodeId).arg(addressText(lane.lineAddress));
}

void TraceCacheLineMinimap::requestStatus(const QString& text, const int timeoutMs)
{
    statusText_ = text;
    Q_EMIT statusMessageRequested(text, timeoutMs);
}

void TraceCacheLineMinimap::beginBuildProgress(const quint64 generation, const std::uint64_t totalRows)
{
    buildProgressActive_ = true;
    buildProgressGeneration_ = generation;
    buildProgressCompletedRows_ = 0;
    buildProgressTotalRows_ = totalRows;
    pendingProgressGeneration_.store(generation, std::memory_order_release);
    pendingProgressCompletedRows_.store(0, std::memory_order_relaxed);
    pendingProgressTotalRows_.store(totalRows, std::memory_order_relaxed);
    progressUpdateQueued_.store(false, std::memory_order_release);
    statusText_ = totalRows == 0
        ? QStringLiteral("Cache Map: replaying cache-line state...")
        : QStringLiteral("Cache Map: replaying 0 / %1 rows")
              .arg(FormatUnsignedIntegerWithThousands(totalRows));
    update();
}

void TraceCacheLineMinimap::postBuildProgress(const quint64 generation,
                                              const std::uint64_t completedRows,
                                              const std::uint64_t totalRows)
{
    if (generation != generation_.load(std::memory_order_relaxed)) {
        return;
    }

    pendingProgressGeneration_.store(generation, std::memory_order_release);
    pendingProgressCompletedRows_.store(completedRows, std::memory_order_relaxed);
    pendingProgressTotalRows_.store(totalRows, std::memory_order_relaxed);

    bool expected = false;
    if (!progressUpdateQueued_.compare_exchange_strong(expected,
                                                       true,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_relaxed)) {
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        const quint64 queuedGeneration = pendingProgressGeneration_.load(std::memory_order_acquire);
        const std::uint64_t queuedCompleted = pendingProgressCompletedRows_.load(std::memory_order_relaxed);
        const std::uint64_t queuedTotal = pendingProgressTotalRows_.load(std::memory_order_relaxed);
        progressUpdateQueued_.store(false, std::memory_order_release);
        updateBuildProgress(queuedGeneration, queuedCompleted, queuedTotal);

        const quint64 latestGeneration = pendingProgressGeneration_.load(std::memory_order_acquire);
        const std::uint64_t latestCompleted = pendingProgressCompletedRows_.load(std::memory_order_relaxed);
        const std::uint64_t latestTotal = pendingProgressTotalRows_.load(std::memory_order_relaxed);
        if (latestGeneration == queuedGeneration
            && latestCompleted == queuedCompleted
            && latestTotal == queuedTotal) {
            return;
        }
        postBuildProgress(latestGeneration, latestCompleted, latestTotal);
    });
}

void TraceCacheLineMinimap::updateBuildProgress(const quint64 generation,
                                                const std::uint64_t completedRows,
                                                const std::uint64_t totalRows)
{
    if (generation != generation_.load(std::memory_order_relaxed)
        || !buildProgressActive_
        || generation != buildProgressGeneration_) {
        return;
    }

    buildProgressTotalRows_ = totalRows;
    buildProgressCompletedRows_ = totalRows == 0 ? completedRows : std::min(completedRows, totalRows);
    statusText_ = totalRows == 0
        ? QStringLiteral("Cache Map: replaying cache-line state...")
        : QStringLiteral("Cache Map: replaying %1 / %2 rows")
              .arg(FormatUnsignedIntegerWithThousands(buildProgressCompletedRows_),
                   FormatUnsignedIntegerWithThousands(totalRows));
    update(buildProgressRect().adjusted(-2, -2, 2, 2));
}

void TraceCacheLineMinimap::finishBuildProgress(const quint64 generation)
{
    if (generation != buildProgressGeneration_) {
        return;
    }
    resetBuildProgress();
    update();
}

void TraceCacheLineMinimap::resetBuildProgress()
{
    buildProgressActive_ = false;
    buildProgressGeneration_ = 0;
    buildProgressCompletedRows_ = 0;
    buildProgressTotalRows_ = 0;
    progressUpdateQueued_.store(false, std::memory_order_release);
}

void TraceCacheLineMinimap::paintBuildProgress(QPainter& painter) const
{
    const QRect bar = buildProgressRect();
    if (!bar.isValid()) {
        return;
    }

    painter.save();
    painter.setOpacity(0.96);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(QStringLiteral("#111827")), 1));
    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    painter.drawRect(bar.adjusted(0, 0, -1, -1));

    const QRect fillArea = bar.adjusted(1, 1, -1, -1);
    if (fillArea.isValid()) {
        int fillWidth = 0;
        if (buildProgressTotalRows_ > 0) {
            fillWidth = static_cast<int>(
                (static_cast<unsigned long long>(std::min(buildProgressCompletedRows_, buildProgressTotalRows_))
                 * static_cast<unsigned long long>(fillArea.width()))
                / static_cast<unsigned long long>(buildProgressTotalRows_));
        } else {
            fillWidth = qMax(12, fillArea.width() / 4);
        }
        painter.fillRect(QRect(fillArea.left(),
                               fillArea.top(),
                               qBound(0, fillWidth, fillArea.width()),
                               fillArea.height()),
                         QColor(QStringLiteral("#38BDF8")));
    }

    if (bar.width() >= 92 && bar.height() >= 12) {
        const QString text = buildProgressTotalRows_ == 0
            ? QStringLiteral("Building")
            : QStringLiteral("%1%").arg(static_cast<unsigned long long>(
                  (std::min(buildProgressCompletedRows_, buildProgressTotalRows_) * 100ULL)
                  / std::max<std::uint64_t>(1, buildProgressTotalRows_)));
        painter.setPen(kMinimapTextColor);
        QFont progressFont = font();
        progressFont.setBold(true);
        painter.setFont(progressFont);
        painter.drawText(bar, Qt::AlignCenter, text);
    }
    painter.restore();
}

#ifdef CHIRON_GUI_ENABLE_TRACE_CACHE_MINIMAP_TEST_API
int TraceCacheLineMinimap::testSegmentCount(const int laneIndex) const noexcept
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return 0;
    }
    return static_cast<int>(lanes_[static_cast<std::size_t>(laneIndex)].segments.size());
}

QVariantMap TraceCacheLineMinimap::testLaneAt(const int laneIndex) const
{
    QVariantMap map;
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return map;
    }
    const Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    map.insert(QStringLiteral("rnNodeId"), static_cast<int>(lane.rnNodeId));
    map.insert(QStringLiteral("lineAddress"), QVariant::fromValue<qulonglong>(lane.lineAddress));
    map.insert(QStringLiteral("tag"), laneTagText(lane));
    map.insert(QStringLiteral("building"), lane.building);
    map.insert(QStringLiteral("failed"), lane.failed);
    map.insert(QStringLiteral("hasBuildResult"), lane.hasBuildResult);
    map.insert(QStringLiteral("status"), lane.statusText);
    return map;
}

QVariantMap TraceCacheLineMinimap::testSegmentAt(const int laneIndex, const int segmentIndex) const
{
    QVariantMap map;
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return map;
    }
    const Lane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    if (segmentIndex < 0 || segmentIndex >= static_cast<int>(lane.segments.size())) {
        return map;
    }
    const Segment& segment = lane.segments[static_cast<std::size_t>(segmentIndex)];
    map.insert(QStringLiteral("startLogicalRow"), QVariant::fromValue<qulonglong>(segment.sourceStartLogicalRow));
    map.insert(QStringLiteral("endLogicalRow"), QVariant::fromValue<qulonglong>(segment.sourceEndLogicalRow));
    map.insert(QStringLiteral("startVisibleRow"), segment.startVisibleRow);
    map.insert(QStringLiteral("endVisibleRow"), segment.endVisibleRow);
    map.insert(QStringLiteral("stateText"), segment.stateText);
    map.insert(QStringLiteral("stateMask"), static_cast<int>(segment.stateMask));
    map.insert(QStringLiteral("sourceSessionId"), QVariant::fromValue<qulonglong>(segment.sourceSessionId));
    map.insert(QStringLiteral("openEnded"), segment.openEnded);
    map.insert(QStringLiteral("splitBefore"), segment.splitBefore);
    map.insert(QStringLiteral("color"), segment.color);
    map.insert(QStringLiteral("x"), segment.rect.x());
    map.insert(QStringLiteral("y"), segment.rect.y());
    map.insert(QStringLiteral("width"), segment.rect.width());
    map.insert(QStringLiteral("height"), segment.rect.height());
    return map;
}

QRect TraceCacheLineMinimap::testLaneRect(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return QRect();
    }
    return lanes_[static_cast<std::size_t>(laneIndex)].laneRect;
}

QRect TraceCacheLineMinimap::testTagRect(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return QRect();
    }
    return lanes_[static_cast<std::size_t>(laneIndex)].tagRect;
}

int TraceCacheLineMinimap::testHitRowAt(const QPoint& position) const
{
    return segmentRowAt(position);
}

bool TraceCacheLineMinimap::testJumpActionAvailable(const int laneIndex,
                                                    const JumpState state,
                                                    const JumpDirection direction,
                                                    const int referenceRow) const
{
    return findJumpSegment(laneIndex, state, direction, referenceRow) != nullptr;
}

bool TraceCacheLineMinimap::testTriggerJumpAction(const int laneIndex,
                                                  const JumpState state,
                                                  const JumpDirection direction,
                                                  const int referenceRow)
{
    const Segment* segment = findJumpSegment(laneIndex, state, direction, referenceRow);
    return segment && jumpToSegment(*segment);
}

bool TraceCacheLineMinimap::testChangeJumpActionAvailable(const int laneIndex,
                                                          const int direction,
                                                          const int referenceRow) const
{
    return findChangeJumpTarget(laneIndex,
                                static_cast<ChangeJumpDirection>(direction),
                                referenceRow)
        .has_value();
}

bool TraceCacheLineMinimap::testTriggerChangeJumpAction(const int laneIndex,
                                                        const int direction,
                                                        const int referenceRow)
{
    const std::optional<ChangeJumpTarget> target = findChangeJumpTarget(
        laneIndex,
        static_cast<ChangeJumpDirection>(direction),
        referenceRow);
    return target && jumpToChangeTarget(*target);
}
#endif

}  // namespace CHIron::Gui
