#include "transaction_widget.hpp"

#include "channel_badge_painter.hpp"
#include "clog_b_trace_loader.hpp"
#include "flit_transaction_keys.hpp"
#include "gui_format.hpp"

#include <QHash>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFrame>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iterator>
#include <limits>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_set>
#include <unordered_map>

namespace CHIron::Gui {

namespace {

constexpr int kHeaderHeight = 28;
constexpr int kFilterToolbarHeight = 36;
constexpr int kRulerHeight = 30;
constexpr int kInfoBarHeight = 22;
constexpr int kStatusPadding = 16;
constexpr int kLaneRowHeight = 32;
constexpr int kLaneRowMinHeight = 12;
constexpr int kLaneRowMaxHeight = 72;
constexpr int kLaneRowHeightWheelStep = 2;
constexpr int kLaneHeaderMinWidth = 180;
constexpr int kLaneHeaderMaxWidth = 340;
constexpr int kLaneHeaderBadgeGap = 5;
constexpr int kLaneHeaderNodeIdGap = 3;
constexpr int kLaneHeaderBadgeMinHeight = 8;
constexpr int kLaneHeaderBadgeMaxHeight = 16;
constexpr int kLaneHeaderTitleHeight = 32;
constexpr int kScrollBarExtent = 16;
constexpr int kTimeRulerTickHeight = 8;
constexpr int kCursorRulerTagHeight = 16;
constexpr int kCursorRulerTagHorizontalPadding = 6;
constexpr int kDensePaintSpanThreshold = 1800;
constexpr int kDensePaintPixelsPerSpan = 2;
constexpr int kDensePaintBucketCount = 5;
constexpr int kDensePaintMaxSpansPerPixelLane = 4;
constexpr int kDensePaintPreserveRectMinWidth = 5;
constexpr int kMaxScrollBarRange = 1000000000;
constexpr std::size_t kTransactionFilterAsyncSpanThreshold = 8192;
constexpr std::size_t kTransactionFilterMaxWorkers = 8;
constexpr std::size_t kTransactionFilterChunkSize = 16384;
constexpr std::size_t kMaxTransactionSpanBuildWorkers = 8;
constexpr std::size_t kMinXactionsPerTransactionSpanWorker = 256;
constexpr std::uint64_t kTransactionSpanScanSliceRows = 262144;
constexpr std::uint8_t kTransactionXactionRowIndexedFlag = 1U << 1;
constexpr std::uint8_t kTransactionXactionRowProcessedByJointFlag = 1U << 2;
constexpr std::uint8_t kTransactionXactionRowCompleteFlag = 1U << 3;
constexpr std::size_t kTransactionSpanAccumulatorLockCount = 16384;
constexpr int kGestureActivationDistance = 7;
constexpr int kGestureDirectionDistance = 24;
constexpr int kViewHistoryLimit = 32;
constexpr int kBuildProgressBarWidth = 360;
constexpr int kBuildProgressBarHeight = 12;
constexpr int kBuildProgressBarMinWidth = 120;
constexpr int kHeaderControlGap = 12;
constexpr int kHeaderPrimaryRowHeight = 26;
constexpr int kFilterToolbarControlHeight = 22;
constexpr int kHeaderFilterModeWidth = 94;
constexpr int kHeaderFilterEditWidth = 132;
constexpr int kHeaderFilterSmallEditWidth = 78;
constexpr int kHeaderFilterGap = 6;
constexpr int kOpcodeTagToggleWidth = 78;
constexpr int kHoverCardToggleWidth = 88;
constexpr int kFilterToolbarClearWidth = 58;
constexpr int kSpanLabelMinPixelSize = 6;
constexpr int kSpanLabelMaxPixelSize = 11;
constexpr int kOpcodeTagHorizontalPadding = 5;
constexpr int kOpcodeTagVerticalPadding = 2;
constexpr int kOpcodeTagGap = 3;
constexpr int kOpcodeTagMinTextWidth = 8;
constexpr int kOpcodeTagHitSlop = 4;
constexpr int kOpcodeTagTimestampLineHitWidth = 5;
constexpr int kLaneHeaderMinPixelSize = 6;
constexpr int kLaneHeaderMaxPixelSize = 12;
constexpr int kHoverCardMaxRows = 12;
constexpr int kHoverCardWidth = 620;
constexpr int kHoverCardVerticalOffset = 16;
constexpr int kHoverCardHorizontalOffset = 16;
constexpr int kHoverCardDwellMs = 420;
constexpr std::chrono::milliseconds kBuildProgressPublishInterval{33};
constexpr char kTransactionWidgetInteractionHint[] =
    "Transaction spans from the xaction index. Click a span to select it, double-click or press Enter to jump to "
    "the request row, drag left-to-right to zoom, drag vertically to fit, use Ctrl+wheel to zoom, Ctrl+Shift+wheel "
    "to resize rows, Shift+drag or middle-drag to pan.";
constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 1048576.0;
constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kBuildProgressStep = 256;

QRect opcodeTagHitRect(const QRect& tagRect, const QRect& plot)
{
    if (!tagRect.isValid() || !plot.isValid()) {
        return {};
    }
    return tagRect.adjusted(-kOpcodeTagHitSlop,
                            -kOpcodeTagHitSlop,
                            kOpcodeTagHitSlop,
                            kOpcodeTagHitSlop)
        .intersected(plot);
}

QColor withAlpha(QColor color, const int alpha)
{
    color.setAlpha(alpha);
    return color;
}

QString elidedOpcodeLabel(const QString& opcode, const QFontMetrics& metrics, const int availableWidth)
{
    if (opcode.isEmpty() || availableWidth <= 0) {
        return {};
    }

    const QString firstCharacter = opcode.left(1);
    if (metrics.horizontalAdvance(firstCharacter) > availableWidth) {
        return {};
    }
    if (metrics.horizontalAdvance(opcode) <= availableWidth) {
        return opcode;
    }

    const QString elided = metrics.elidedText(opcode, Qt::ElideRight, availableWidth);
    if (!elided.isEmpty()
        && elided.startsWith(firstCharacter)
        && metrics.horizontalAdvance(elided) <= availableWidth) {
        return elided;
    }

    for (int length = opcode.size(); length >= 1; --length) {
        const QString prefix = opcode.left(length);
        if (metrics.horizontalAdvance(prefix) <= availableWidth) {
            return prefix;
        }
    }
    return {};
}

int rowScaledPixelSize(const int rowHeight, const int minPixelSize, const int maxPixelSize)
{
    constexpr int kFontScaleReferenceMinRowHeight = 12;
    constexpr int kFontScaleReferenceMaxRowHeight = 32;
    if (maxPixelSize <= minPixelSize) {
        return minPixelSize;
    }
    const int clampedRowHeight =
        std::clamp(rowHeight, kFontScaleReferenceMinRowHeight, kFontScaleReferenceMaxRowHeight);
    const double normalized =
        static_cast<double>(clampedRowHeight - kFontScaleReferenceMinRowHeight)
        / static_cast<double>(kFontScaleReferenceMaxRowHeight - kFontScaleReferenceMinRowHeight);
    return std::clamp(static_cast<int>(std::lround(static_cast<double>(minPixelSize)
                                                   + normalized * static_cast<double>(maxPixelSize - minPixelSize))),
                      minPixelSize,
                      maxPixelSize);
}

QFont transactionSpanLabelFont(const QFont& baseFont, const int rowHeight)
{
    QFont labelFont(baseFont);
    labelFont.setFamily(QStringLiteral("Consolas"));
    labelFont.setStyleHint(QFont::TypeWriter);
    const int pixelSize = rowScaledPixelSize(rowHeight, kSpanLabelMinPixelSize, kSpanLabelMaxPixelSize);
    labelFont.setPixelSize(pixelSize);
    return labelFont;
}

QFont transactionLaneHeaderFont(const QFont& baseFont, const int rowHeight, const bool bold = false)
{
    QFont labelFont(baseFont);
    const int pixelSize = rowScaledPixelSize(rowHeight, kLaneHeaderMinPixelSize, kLaneHeaderMaxPixelSize);
    labelFont.setPixelSize(pixelSize);
    labelFont.setBold(bold);
    return labelFont;
}

QFont transactionLaneHeaderTitleFont(const QFont& baseFont, const bool bold = false)
{
    return transactionLaneHeaderFont(baseFont, kLaneRowHeight, bold);
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

QString jointFamilyFromInfo(const TraceSession::XactionIndexRowInfo& info)
{
    if (info.jointType == QLatin1String("RNFJoint") || info.jointPath.startsWith(QStringLiteral("RNFJoint::"))) {
        return QStringLiteral("RNFJoint");
    }
    if (info.jointType == QLatin1String("SNFJoint") || info.jointPath.startsWith(QStringLiteral("SNFJoint::"))) {
        return QStringLiteral("SNFJoint");
    }
    return QStringLiteral("Unknown Joint");
}

QColor transactionNodeBadgeColor(const QString& label)
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

QString transportNodeTypeLabel(const std::shared_ptr<TraceSession>& session, const QString& endpointNode)
{
    bool ok = false;
    const qulonglong nodeId = endpointNode.trimmed().toULongLong(&ok, 0);
    if (!ok || !session || nodeId > std::numeric_limits<quint16>::max()) {
        return QStringLiteral("Node");
    }

    const auto found = session->metadata().nodeTypes.find(static_cast<quint16>(nodeId));
    if (found == session->metadata().nodeTypes.cend()) {
        return QStringLiteral("Node");
    }
    const QString label = QString::fromStdString(CLog::NodeTypeToString(found->second)).trimmed();
    return label.isEmpty() ? QStringLiteral("Node") : label;
}

struct SpanClassification {
    QString jointFamily = QStringLiteral("Unknown Joint");
    QString endpointLabel = QStringLiteral("Unknown endpoint");
    QString endpointNode = QStringLiteral("unknown");
    QString endpointNodeType = QStringLiteral("Node");
    QString originKind = QStringLiteral("Unknown");
    QString requestOpcode = QStringLiteral("Unknown");
    QString addressLabel;
    QString confidence = QStringLiteral("unknown");
    std::uint64_t requestLogicalRow = 0;
};

QString laneKeyFor(const QString& jointFamily,
                   const QString& endpointLabel,
                   const QString& originKind,
                   const int stackSlot)
{
    return jointFamily + QChar(0x1F)
        + endpointLabel + QChar(0x1F)
        + originKind + QChar(0x1F)
        + QString::number(stackSlot);
}

QString txnIdLaneKeyFor(const QString& jointFamily,
                        const QString& endpointLabel,
                        const QString& originKind,
                        const QString& txnIdLabel)
{
    return jointFamily + QChar(0x1F)
        + endpointLabel + QChar(0x1F)
        + originKind + QChar(0x1F)
        + QStringLiteral("txn") + QChar(0x1F)
        + txnIdLabel;
}

QString groupKeyFor(const QString& jointFamily,
                    const QString& endpointLabel,
                    const QString& originKind)
{
    return jointFamily + QChar(0x1F)
        + endpointLabel + QChar(0x1F)
        + originKind;
}

int jointSortRank(const QString& jointFamily)
{
    if (jointFamily == QLatin1String("RNFJoint")) {
        return 0;
    }
    if (jointFamily == QLatin1String("SNFJoint")) {
        return 1;
    }
    return 2;
}

int originSortRank(const QString& originKind)
{
    if (originKind == QLatin1String("REQ")) {
        return 0;
    }
    if (originKind == QLatin1String("SNP")) {
        return 1;
    }
    return 2;
}

bool numericLabelLess(const QString& lhs, const QString& rhs)
{
    bool lhsOk = false;
    bool rhsOk = false;
    const qulonglong lhsValue = lhs.toULongLong(&lhsOk);
    const qulonglong rhsValue = rhs.toULongLong(&rhsOk);
    if (lhsOk && rhsOk && lhsValue != rhsValue) {
        return lhsValue < rhsValue;
    }
    if (lhsOk != rhsOk) {
        return lhsOk;
    }
    return lhs < rhs;
}

qint64 clampedRoundToTimestamp(const long double value)
{
    if (value <= static_cast<long double>((std::numeric_limits<qint64>::min)())) {
        return (std::numeric_limits<qint64>::min)();
    }
    if (value >= static_cast<long double>((std::numeric_limits<qint64>::max)())) {
        return (std::numeric_limits<qint64>::max)();
    }
    return static_cast<qint64>(std::llround(value));
}

std::uint64_t boundedRoundToUInt64(const long double value)
{
    if (value <= 0.0L) {
        return 0;
    }
    const long double maxValue = static_cast<long double>((std::numeric_limits<std::uint64_t>::max)());
    return static_cast<std::uint64_t>(std::min(value + 0.5L, maxValue));
}

std::uint64_t transactionHorizontalScrollRange(const qint64 fullStart,
                                               const qint64 fullEnd,
                                               const double horizontalZoom)
{
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom);
    return boundedRoundToUInt64(std::max<long double>(0.0L, fullRange - visibleRange));
}

int scaledScrollBarValue(const std::uint64_t value, const std::uint64_t maxValue)
{
    if (maxValue == 0) {
        return 0;
    }
    const long double ratio =
        std::min(static_cast<long double>(value), static_cast<long double>(maxValue))
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
    return boundedRoundToUInt64(ratio * static_cast<long double>(maxValue));
}

QString compactLaneCount(const std::uint64_t count)
{
    return QStringLiteral("%1 tx").arg(FormatUnsignedIntegerWithThousands(count));
}

std::size_t transactionSpanBuildWorkerCount(const std::size_t xactionCount)
{
    const unsigned int hardwareWorkers = std::min(
        static_cast<unsigned int>(kMaxTransactionSpanBuildWorkers),
        std::max(1U, std::thread::hardware_concurrency()));
    if (hardwareWorkers <= 1U || xactionCount < kMinXactionsPerTransactionSpanWorker * 2U) {
        return 1;
    }

    const std::size_t workLimitedWorkers =
        std::max<std::size_t>(1, xactionCount / kMinXactionsPerTransactionSpanWorker);
    return std::min<std::size_t>(static_cast<std::size_t>(hardwareWorkers), workLimitedWorkers);
}

QString addressText(const std::uint64_t address)
{
    const int digits = address <= 0xFFFFFFFFFFFFULL ? 12 : 16;
    return QStringLiteral("0x%1")
        .arg(QString::number(static_cast<qulonglong>(address), 16).toUpper()
                 .rightJustified(digits, QLatin1Char('0')));
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

QString displayId(const std::uint32_t value)
{
    return value == (std::numeric_limits<std::uint32_t>::max)()
        ? QString()
        : QString::number(static_cast<qulonglong>(value));
}

bool parseSearchInteger(QString text, qulonglong& value)
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

QString normalizedNumericFilter(QString text)
{
    text = text.trimmed();
    return text.compare(QStringLiteral("0x"), Qt::CaseInsensitive) == 0 ? QString() : text;
}

bool numericFilterActive(const QString& text)
{
    return !normalizedNumericFilter(text).isEmpty();
}

void appendUniqueSearchValue(std::vector<QString>& values, QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(std::move(value));
    }
}

QString displayDbid(const std::uint32_t value)
{
    return displayId(value);
}

bool matchesTransactionFilterValue(const QString& value, const QString& filter)
{
    if (filter.isEmpty()) {
        return true;
    }

    qulonglong filterNumber = 0;
    qulonglong valueNumber = 0;
    const bool filterIsNumeric = parseSearchInteger(filter, filterNumber);
    if (filterIsNumeric && parseSearchInteger(value, valueNumber)) {
        return filterNumber == valueNumber;
    }
    return value.contains(filter, Qt::CaseInsensitive);
}

bool matchesAnyTransactionFilterValue(const QString& primary,
                                      const std::vector<QString>& values,
                                      const QString& filter)
{
    if (filter.isEmpty()) {
        return true;
    }
    if (matchesTransactionFilterValue(primary, filter)) {
        return true;
    }
    for (const QString& value : values) {
        if (value != primary && matchesTransactionFilterValue(value, filter)) {
            return true;
        }
    }
    return false;
}

struct BuildRowSummary {
    std::uint64_t logicalRow = 0;
    qint64 timestamp = 0;
    FlitChannel channel = FlitChannel::Req;
    FlitDirection direction = FlitDirection::Tx;
    QString opcode;
    QString opcodeRaw;
    QString source;
    QString target;
    QString txnId;
    QString dbid;
    QString address;
    std::optional<quint16> channelNodeId;
};

struct TransactionBlockSlice {
    std::size_t blockIndex = 0;
    std::uint64_t beginRow = 0;
    std::uint64_t rowCount = 0;
    std::size_t localBegin = 0;
};

struct TransactionSpanAccumulator {
    std::uint64_t ordinal = 0;
    std::uint64_t rowCount = 0;
    BuildRowSummary firstRow;
    BuildRowSummary lastRow;
    std::optional<BuildRowSummary> firstReqRow;
    std::optional<BuildRowSummary> firstSnpRow;
    std::vector<QString> opcodeValues;
    std::vector<QString> txnIdValues;
    std::vector<QString> dbidValues;
    std::optional<std::uint64_t> completionLogicalRow;
    qint64 completionTimestamp = 0;
    bool hasFirstRow = false;
    bool hasLastRow = false;
    bool indexed = false;
    bool processedByJoint = false;
};

struct TransactionSpanAccumulatorLock {
    std::mutex mutex;
};

QString displayOpcode(const BuildRowSummary& row)
{
    if (!row.opcode.isEmpty()) {
        return row.opcode;
    }
    if (!row.opcodeRaw.isEmpty()) {
        return row.opcodeRaw;
    }
    return QStringLiteral("Unknown");
}

QString snpTargetNodeForClassification(const BuildRowSummary& row)
{
    if (row.direction == FlitDirection::Rx && row.channelNodeId) {
        return QString::number(static_cast<qulonglong>(*row.channelNodeId));
    }
    return QString();
}

QString rowChannelOpcodeLabel(const BuildRowSummary& row)
{
    return QStringLiteral("%1 %2").arg(ToString(row.channel), displayOpcode(row));
}

QString displayRowEndpointId(const QString& value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("-") : value.trimmed();
}

QString displayAddress(const BuildRowSummary& row)
{
    return row.address.trimmed();
}

QString opcodeWithoutChannelPrefix(QString opcode)
{
    opcode = opcode.trimmed();
    static constexpr std::array<std::string_view, 4> kPrefixes = {
        "REQ ",
        "SNP ",
        "RSP ",
        "DAT ",
    };
    for (const std::string_view prefix : kPrefixes) {
        const QString prefixText = QString::fromLatin1(prefix.data(), static_cast<qsizetype>(prefix.size()));
        if (opcode.startsWith(prefixText)) {
            return opcode.sliced(prefixText.size()).trimmed();
        }
        if (opcode == QStringView(prefixText).left(3)) {
            return {};
        }
    }
    return opcode;
}

QString channelKindFromOpcodeOrOrigin(QString opcode, QString originKind)
{
    opcode = opcode.trimmed();
    originKind = originKind.trimmed();
    for (const QString& candidate : {originKind, opcode}) {
        if (candidate == QLatin1String("REQ") || candidate.startsWith(QStringLiteral("REQ "))) {
            return QStringLiteral("REQ");
        }
        if (candidate == QLatin1String("SNP") || candidate.startsWith(QStringLiteral("SNP "))) {
            return QStringLiteral("SNP");
        }
        if (candidate == QLatin1String("RSP") || candidate.startsWith(QStringLiteral("RSP "))) {
            return QStringLiteral("RSP");
        }
        if (candidate == QLatin1String("DAT") || candidate.startsWith(QStringLiteral("DAT "))) {
            return QStringLiteral("DAT");
        }
    }
    return QStringLiteral("Unknown");
}

int denseSpanBucketForChannelKind(const QString& channelKind)
{
    if (channelKind == QLatin1String("REQ")) {
        return 0;
    }
    if (channelKind == QLatin1String("SNP")) {
        return 1;
    }
    if (channelKind == QLatin1String("RSP")) {
        return 2;
    }
    if (channelKind == QLatin1String("DAT")) {
        return 3;
    }
    return 4;
}

QColor channelEdgeColor(const QString& channelKind)
{
    if (channelKind == QLatin1String("REQ")) {
        return ChannelAccent(FlitChannel::Req);
    }
    if (channelKind == QLatin1String("SNP")) {
        return ChannelAccent(FlitChannel::Snp);
    }
    if (channelKind == QLatin1String("RSP")) {
        return ChannelAccent(FlitChannel::Rsp);
    }
    if (channelKind == QLatin1String("DAT")) {
        return ChannelAccent(FlitChannel::Dat);
    }
    return QColor(QStringLiteral("#D9D9D9"));
}

QColor lightenedChannelFill(const QColor& accent)
{
    constexpr int kAccentWeight = 58;
    constexpr int kWhiteWeight = 255 - kAccentWeight;
    return QColor((accent.red() * kAccentWeight + 255 * kWhiteWeight) / 255,
                  (accent.green() * kAccentWeight + 255 * kWhiteWeight) / 255,
                  (accent.blue() * kAccentWeight + 255 * kWhiteWeight) / 255);
}

QColor channelFillColor(const QString& channelKind, const bool completed)
{
    if (!completed) {
        return QColor(QStringLiteral("#F7F8FA"));
    }
    return lightenedChannelFill(channelEdgeColor(channelKind));
}

QColor denseSpanBucketColor(const int bucket)
{
    switch (bucket) {
    case 0:
        return channelEdgeColor(QStringLiteral("REQ"));
    case 1:
        return channelEdgeColor(QStringLiteral("SNP"));
    case 2:
        return channelEdgeColor(QStringLiteral("RSP"));
    case 3:
        return channelEdgeColor(QStringLiteral("DAT"));
    default:
        return QColor(QStringLiteral("#75808A"));
    }
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

void addTransactionRowToAccumulator(TransactionSpanAccumulator& accumulator,
                                    const std::uint64_t ordinal,
                                    BuildRowSummary&& summary,
                                    const std::uint8_t flags)
{
    if (accumulator.ordinal == 0) {
        accumulator.ordinal = ordinal;
    }
    ++accumulator.rowCount;
    accumulator.indexed = accumulator.indexed || ((flags & kTransactionXactionRowIndexedFlag) != 0);
    accumulator.processedByJoint =
        accumulator.processedByJoint || ((flags & kTransactionXactionRowProcessedByJointFlag) != 0);

    if (!accumulator.hasFirstRow || summary.logicalRow < accumulator.firstRow.logicalRow) {
        accumulator.firstRow = summary;
        accumulator.hasFirstRow = true;
    }
    if (!accumulator.hasLastRow || summary.logicalRow > accumulator.lastRow.logicalRow) {
        accumulator.lastRow = summary;
        accumulator.hasLastRow = true;
    }
    if (summary.channel == FlitChannel::Req
        && (!accumulator.firstReqRow
            || summary.logicalRow < accumulator.firstReqRow->logicalRow)) {
        accumulator.firstReqRow = summary;
    }
    if (summary.channel == FlitChannel::Snp
        && (!accumulator.firstSnpRow
            || summary.logicalRow < accumulator.firstSnpRow->logicalRow)) {
        accumulator.firstSnpRow = summary;
    }
    appendUniqueSearchValue(accumulator.opcodeValues, summary.opcode);
    appendUniqueSearchValue(accumulator.opcodeValues, summary.opcodeRaw);
    appendUniqueSearchValue(accumulator.opcodeValues, rowChannelOpcodeLabel(summary));
    appendUniqueSearchValue(accumulator.opcodeValues, opcodeWithoutChannelPrefix(rowChannelOpcodeLabel(summary)));
    appendUniqueSearchValue(accumulator.txnIdValues, summary.txnId);
    appendUniqueSearchValue(accumulator.dbidValues, summary.dbid);
    if ((flags & kTransactionXactionRowCompleteFlag) != 0
        && (!accumulator.completionLogicalRow
            || summary.logicalRow > *accumulator.completionLogicalRow)) {
        accumulator.completionLogicalRow = summary.logicalRow;
        accumulator.completionTimestamp = summary.timestamp;
    }
}

BuildRowSummary summaryFromFastRecord(const std::shared_ptr<TraceSession>& session,
                                      const std::uint64_t logicalRow,
                                      const CLogBTraceFastRecordInfo& fastRecord,
                                      std::unordered_map<std::uint32_t, QString>& opcodeCache)
{
    const auto transport = static_cast<CLog::Channel>(fastRecord.channel);
    const std::uint32_t opcodeKey =
        (static_cast<std::uint32_t>(fastRecord.channel) << 24U)
        | (fastRecord.opcode & 0x00FFFFFFU);

    auto opcodeFound = opcodeCache.find(opcodeKey);
    if (opcodeFound == opcodeCache.end()) {
        opcodeFound = opcodeCache.emplace(
            opcodeKey,
            CLogBTraceLoader::opcodeDisplayValue(session->metadata().parameters, fastRecord)).first;
    }

    BuildRowSummary summary;
    summary.logicalRow = logicalRow;
    summary.timestamp = fastRecord.timestamp;
    summary.channel = channelFromTransport(transport);
    summary.direction = directionFromTransport(transport);
    summary.opcode = opcodeFound->second == QLatin1String("Unknown") ? QString() : opcodeFound->second;
    summary.opcodeRaw = QStringLiteral("0x%1")
                            .arg(static_cast<qulonglong>(fastRecord.opcode), 0, 16)
                            .toUpper();
    summary.source = displayId(fastRecord.sourceId);
    summary.target = displayId(fastRecord.targetId);
    summary.txnId = displayId(fastRecord.txnId);
    summary.dbid = displayDbid(fastRecord.dbid);
    if (fastRecord.address != (std::numeric_limits<std::uint64_t>::max)()) {
        summary.address = addressText(fastRecord.address);
    }
    if (fastRecord.nodeId != (std::numeric_limits<std::uint32_t>::max)()
        && fastRecord.nodeId <= (std::numeric_limits<quint16>::max)()) {
        summary.channelNodeId = static_cast<quint16>(fastRecord.nodeId);
    }
    return summary;
}

bool scanTransactionSpanSliceIntoAccumulators(
    const std::shared_ptr<TraceSession>& session,
    const TransactionBlockSlice& slice,
    const std::unordered_map<std::uint64_t, std::size_t>& ordinalIndexByOrdinal,
    std::vector<TransactionSpanAccumulator>& accumulators,
    std::vector<TransactionSpanAccumulatorLock>& accumulatorLocks,
    CLogBTraceLoadError& error,
    std::stop_token stopToken)
{
    if (!session) {
        error.summary = QStringLiteral("No trace session is available.");
        return false;
    }

    std::vector<CLogBTraceFastRecordInfo> fastRecords;
    if (!session->readFastRecords(slice.blockIndex,
                                  slice.localBegin,
                                  static_cast<std::size_t>(slice.rowCount),
                                  fastRecords,
                                  error,
                                  stopToken)) {
        if (error.summary.isEmpty()) {
            error.summary = QStringLiteral("failed to read compact trace-row metadata");
        }
        return false;
    }

    std::vector<TraceSession::XactionRowCompactInfo> rowInfos;
    if (!session->xactionRowCompactInfoRange(slice.beginRow, slice.rowCount, rowInfos)
        || rowInfos.size() < slice.rowCount) {
        error.summary = QStringLiteral("failed to read compact Xaction row metadata");
        return false;
    }

    if (fastRecords.size() < static_cast<std::size_t>(slice.rowCount)) {
        error.summary = QStringLiteral("fast-record block slice is outside the decoded block.");
        return false;
    }

    std::unordered_map<std::uint32_t, QString> opcodeCache;
    for (std::size_t index = 0; index < static_cast<std::size_t>(slice.rowCount); ++index) {
        if (stopToken.stop_requested()) {
            error.type = CLogBTraceLoadError::Type::Cancelled;
            error.summary = QStringLiteral("Transaction span build was cancelled.");
            return false;
        }

        const TraceSession::XactionRowCompactInfo& info = rowInfos[index];
        if (info.xactionOrdinal == 0 || (info.flags & kTransactionXactionRowIndexedFlag) == 0) {
            continue;
        }
        const auto accumulatorIndexFound = ordinalIndexByOrdinal.find(info.xactionOrdinal);
        if (accumulatorIndexFound == ordinalIndexByOrdinal.end()) {
            continue;
        }

        const CLogBTraceFastRecordInfo& fastRecord = fastRecords[index];
        if (!validTransportChannel(fastRecord.channel)) {
            error.summary = QStringLiteral("Compact trace-row metadata contains an invalid transport channel.");
            return false;
        }

        const std::size_t accumulatorIndex = accumulatorIndexFound->second;
        BuildRowSummary summary =
            summaryFromFastRecord(session, slice.beginRow + index, fastRecord, opcodeCache);
        TransactionSpanAccumulatorLock& lock =
            accumulatorLocks[accumulatorIndex % accumulatorLocks.size()];
        const std::lock_guard guard(lock.mutex);
        addTransactionRowToAccumulator(accumulators[accumulatorIndex],
                                       info.xactionOrdinal,
                                       std::move(summary),
                                       info.flags);
    }
    return true;
}

SpanClassification classifySpan(const std::shared_ptr<TraceSession>& session,
                                const TraceSession::XactionIndexRowInfo& firstInfo,
                                const TransactionSpanAccumulator& accumulator)
{
    SpanClassification classification;
    classification.jointFamily = jointFamilyFromInfo(firstInfo);
    if (accumulator.hasFirstRow) {
        classification.requestLogicalRow = accumulator.firstRow.logicalRow;
    }

    const BuildRowSummary* requestRow = accumulator.firstReqRow ? &*accumulator.firstReqRow : nullptr;
    const BuildRowSummary* snoopRow = accumulator.firstSnpRow ? &*accumulator.firstSnpRow : nullptr;
    const BuildRowSummary* originRow = nullptr;
    if (requestRow && snoopRow) {
        originRow = requestRow->logicalRow <= snoopRow->logicalRow ? requestRow : snoopRow;
    } else {
        originRow = requestRow ? requestRow : snoopRow;
    }
    const BuildRowSummary* anchorRow =
        originRow ? originRow : (accumulator.hasFirstRow ? &accumulator.firstRow : nullptr);
    if (anchorRow) {
        classification.requestOpcode = rowChannelOpcodeLabel(*anchorRow);
        classification.requestLogicalRow = anchorRow->logicalRow;
        classification.addressLabel = displayAddress(*anchorRow);
    }

    if (classification.jointFamily == QLatin1String("RNFJoint")) {
        if (originRow && originRow->channel == FlitChannel::Req) {
            classification.originKind = QStringLiteral("REQ");
            classification.endpointNode = originRow->source.trimmed();
            classification.endpointLabel = classification.endpointNode.isEmpty()
                ? QStringLiteral("RN unknown")
                : QStringLiteral("RN %1").arg(classification.endpointNode);
            classification.endpointNodeType = transportNodeTypeLabel(session, classification.endpointNode);
            classification.confidence = classification.endpointNode.isEmpty()
                ? QStringLiteral("malformed REQ SrcID")
                : QStringLiteral("REQ SrcID");
            return classification;
        }
        if (originRow && originRow->channel == FlitChannel::Snp) {
            classification.originKind = QStringLiteral("SNP");
            classification.endpointNode = snpTargetNodeForClassification(*originRow);
            classification.endpointLabel = classification.endpointNode.isEmpty()
                ? QStringLiteral("RN unknown")
                : QStringLiteral("RN %1").arg(classification.endpointNode);
            classification.endpointNodeType = transportNodeTypeLabel(session, classification.endpointNode);
            classification.confidence = classification.endpointNode.isEmpty()
                ? QStringLiteral("missing SNP target context")
                : QStringLiteral("SNP target context");
            return classification;
        }
        classification.originKind = QStringLiteral("Unknown");
        classification.endpointLabel = QStringLiteral("RN unknown");
        classification.endpointNode = QStringLiteral("unknown");
        classification.endpointNodeType = QStringLiteral("Node");
        classification.confidence = QStringLiteral("inferred without REQ/SNP");
        return classification;
    }

    if (classification.jointFamily == QLatin1String("SNFJoint")) {
        if (originRow && originRow->channel == FlitChannel::Req) {
            classification.originKind = QStringLiteral("REQ");
            classification.endpointNode = originRow->target.trimmed();
            classification.endpointLabel = classification.endpointNode.isEmpty()
                ? QStringLiteral("SN unknown")
                : QStringLiteral("SN %1").arg(classification.endpointNode);
            classification.endpointNodeType = transportNodeTypeLabel(session, classification.endpointNode);
            classification.confidence = classification.endpointNode.isEmpty()
                ? QStringLiteral("malformed REQ TgtID")
                : QStringLiteral("REQ TgtID");
            return classification;
        }
        classification.originKind = originRow && originRow->channel == FlitChannel::Snp
            ? QStringLiteral("SNP")
            : QStringLiteral("Unknown");
        classification.endpointLabel = QStringLiteral("SN unknown");
        classification.endpointNode = QStringLiteral("unknown");
        classification.endpointNodeType = QStringLiteral("Node");
        classification.confidence = QStringLiteral("inferred without REQ");
        return classification;
    }

    classification.originKind = originRow && originRow->channel == FlitChannel::Req
        ? QStringLiteral("REQ")
        : (originRow && originRow->channel == FlitChannel::Snp ? QStringLiteral("SNP") : QStringLiteral("Unknown"));
    classification.endpointLabel = QStringLiteral("Unknown endpoint");
    classification.endpointNode = QStringLiteral("unknown");
    classification.endpointNodeType = QStringLiteral("Node");
    classification.confidence = QStringLiteral("unknown joint");
    return classification;
}

}  // namespace

TransactionWidget::TransactionWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("transactionWidget"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    horizontalScrollBar_ = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar_->setSingleStep(16);
    horizontalScrollBar_->setPageStep(100);
    connect(horizontalScrollBar_, &QScrollBar::valueChanged, this, [this](const int value) {
        const auto [fullStart, fullEnd] = fullTimestampRange();
        const std::uint64_t maxOffset =
            transactionHorizontalScrollRange(fullStart, fullEnd, horizontalZoom_);
        const std::uint64_t offset = unscaledScrollBarValue(value, maxOffset);
        if (scrollOffset_ != offset) {
            scrollOffset_ = offset;
            update();
        }
    });

    verticalScrollBar_ = new QScrollBar(Qt::Vertical, this);
    verticalScrollBar_->setSingleStep(1);
    verticalScrollBar_->setPageStep(1);
    connect(verticalScrollBar_, &QScrollBar::valueChanged, this, [this](const int value) {
        verticalScrollOffset_ = std::max(0, value);
        update();
    });

    filterToolbar_ = new QFrame(this);
    filterToolbar_->setObjectName(QStringLiteral("transactionFilterToolbar"));
    filterToolbar_->setFrameShape(QFrame::NoFrame);
    filterToolbar_->setAutoFillBackground(false);

    filterModeCombo_ = new QComboBox(filterToolbar_);
    filterModeCombo_->setObjectName(QStringLiteral("transactionFilterModeCombo"));
    filterModeCombo_->setFocusPolicy(Qt::StrongFocus);
    filterModeCombo_->setFixedHeight(kFilterToolbarControlHeight);
    filterModeCombo_->addItem(QStringLiteral("Highlight"), filterModeName(TransactionFilterMode::Highlight));
    filterModeCombo_->addItem(QStringLiteral("Filter"), filterModeName(TransactionFilterMode::Filter));
    filterModeCombo_->setToolTip(QStringLiteral("Choose whether matching transactions are highlighted or non-matches are hidden."));
    connect(filterModeCombo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](const int index) {
                if (synchronizingFilterControls_) {
                    return;
                }
                setFilterMode(filterModeFromName(filterModeCombo_->itemData(index).toString()));
            });

    const auto createFilterEdit = [this](const QString& objectName,
                                         const QString& placeholder,
                                         const QString& toolTip) {
        auto* edit = new QLineEdit(filterToolbar_);
        edit->setObjectName(objectName);
        edit->setClearButtonEnabled(true);
        edit->setPlaceholderText(placeholder);
        edit->setToolTip(toolTip);
        edit->setFocusPolicy(Qt::StrongFocus);
        edit->setFixedHeight(kFilterToolbarControlHeight);
        return edit;
    };
    opcodeFilterEdit_ = createFilterEdit(QStringLiteral("transactionOpcodeFilterEdit"),
                                         QStringLiteral("Opcode"),
                                         QStringLiteral("Filter transactions by request/snoop opcode."));
    addressFilterEdit_ = createFilterEdit(QStringLiteral("transactionAddressFilterEdit"),
                                          QStringLiteral("Address"),
                                          QStringLiteral("Filter transactions by decoded address."));
    txnIdFilterEdit_ = createFilterEdit(QStringLiteral("transactionTxnIdFilterEdit"),
                                        QStringLiteral("TxnID"),
                                        QStringLiteral("Filter transactions by any TxnID observed in the transaction."));
    dbidFilterEdit_ = createFilterEdit(QStringLiteral("transactionDbidFilterEdit"),
                                       QStringLiteral("DBID"),
                                       QStringLiteral("Filter transactions by any DBID observed in the transaction."));

    const auto connectFilterEdit = [this](QLineEdit* edit) {
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            if (synchronizingFilterControls_) {
                return;
            }
            setFilterCriteria(TransactionFilterCriteria{
                opcodeFilterEdit_ ? opcodeFilterEdit_->text().trimmed() : QString(),
                addressFilterEdit_ ? addressFilterEdit_->text().trimmed() : QString(),
                txnIdFilterEdit_ ? txnIdFilterEdit_->text().trimmed() : QString(),
                dbidFilterEdit_ ? dbidFilterEdit_->text().trimmed() : QString(),
            });
        });
    };
    connectFilterEdit(opcodeFilterEdit_);
    connectFilterEdit(addressFilterEdit_);
    connectFilterEdit(txnIdFilterEdit_);
    connectFilterEdit(dbidFilterEdit_);

    opcodeTagsCheckBox_ = new QCheckBox(QStringLiteral("Tags"), filterToolbar_);
    opcodeTagsCheckBox_->setObjectName(QStringLiteral("transactionOpcodeTagsCheckBox"));
    opcodeTagsCheckBox_->setChecked(showOpcodeTags_);
    opcodeTagsCheckBox_->setFixedHeight(kFilterToolbarControlHeight);
    opcodeTagsCheckBox_->setFocusPolicy(Qt::StrongFocus);
    opcodeTagsCheckBox_->setToolTip(QStringLiteral("Show opcode tags for subsequent flits on the selected transaction."));
    connect(opcodeTagsCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (synchronizingFilterControls_) {
            return;
        }
        if (showOpcodeTags_ == checked) {
            return;
        }
        showOpcodeTags_ = checked;
        update();
    });

    hoverCardsCheckBox_ = new QCheckBox(QStringLiteral("Cards"), filterToolbar_);
    hoverCardsCheckBox_->setObjectName(QStringLiteral("transactionHoverCardsCheckBox"));
    hoverCardsCheckBox_->setChecked(showHoverCards_);
    hoverCardsCheckBox_->setFixedHeight(kFilterToolbarControlHeight);
    hoverCardsCheckBox_->setFocusPolicy(Qt::StrongFocus);
    hoverCardsCheckBox_->setToolTip(QStringLiteral("Show transaction summary cards after hovering over a transaction."));
    connect(hoverCardsCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (synchronizingFilterControls_) {
            return;
        }
        if (showHoverCards_ == checked) {
            return;
        }
        showHoverCards_ = checked;
        if (!showHoverCards_) {
            hideHoverCard();
        } else if (!lastMousePosition_.isNull() && !QApplication::mouseButtons()) {
            hoveredSpanIndex_ = spanIndexAtPosition(lastMousePosition_);
            armHoverCard(hoveredSpanIndex_, lastMousePosition_);
        }
    });

    filterClearButton_ = new QPushButton(QStringLiteral("Clear"), filterToolbar_);
    filterClearButton_->setObjectName(QStringLiteral("transactionFilterClearButton"));
    filterClearButton_->setFixedHeight(kFilterToolbarControlHeight);
    filterClearButton_->setToolTip(QStringLiteral("Clear all Transaction filters."));
    connect(filterClearButton_, &QPushButton::clicked, this, [this]() {
        clearTransactionFilter();
    });

    filterToolbar_->setStyleSheet(QStringLiteral(
        "QFrame#transactionFilterToolbar {"
        "  background: #EEF2F5;"
        "  border-top: 1px solid #FFFFFF;"
        "  border-bottom: 1px solid #C8D0D8;"
        "}"
        "QFrame#transactionFilterToolbar QComboBox,"
        "QFrame#transactionFilterToolbar QLineEdit {"
        "  min-height: 20px;"
        "  padding: 0 6px;"
        "  color: #18212B;"
        "  background: #FFFFFF;"
        "  border: 1px solid #C6CFD8;"
        "  border-radius: 0px;"
        "  selection-background-color: #AFC7DB;"
        "}"
        "QFrame#transactionFilterToolbar QComboBox:focus,"
        "QFrame#transactionFilterToolbar QLineEdit:focus {"
        "  border-color: #7D9CB7;"
        "  background: #FCFDFF;"
        "}"
        "QPushButton#transactionFilterClearButton {"
        "  min-height: 20px;"
        "  padding: 0 9px;"
        "  color: #22303A;"
        "  background: #FFFFFF;"
        "  border: 1px solid #C4CDD6;"
        "  border-radius: 0px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#transactionFilterClearButton:hover {"
        "  background: #F5F8FA;"
        "}"
        "QCheckBox#transactionOpcodeTagsCheckBox,"
        "QCheckBox#transactionHoverCardsCheckBox {"
        "  min-height: 20px;"
        "  color: #22303A;"
        "  font-weight: 600;"
        "  spacing: 5px;"
        "}"
        "QCheckBox#transactionOpcodeTagsCheckBox::indicator,"
        "QCheckBox#transactionHoverCardsCheckBox::indicator {"
        "  width: 13px;"
        "  height: 13px;"
        "}"));
    syncFilterControls();

    hoverCardDwellTimer_ = new QTimer(this);
    hoverCardDwellTimer_->setSingleShot(true);
    hoverCardDwellTimer_->setInterval(kHoverCardDwellMs);
    connect(hoverCardDwellTimer_, &QTimer::timeout, this, [this]() {
        showArmedHoverCard();
    });

    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
}

TransactionWidget::~TransactionWidget()
{
    stopFilterThread();
    stopBuildThread();
}

void TransactionWidget::setTraceSession(std::shared_ptr<TraceSession> session)
{
    stopFilterThread();
    stopBuildThread();
    traceSession_ = std::move(session);
    rows_.clear();
    clearSpanData();
    lastBuildWorkerCount_ = 1;
    sourceMode_ = traceSession_ ? SourceMode::TraceSession : SourceMode::Empty;
    requestedFilterCriteria_ = {};
    requestedFilterMode_ = TransactionFilterMode::Highlight;
    filterCriteria_ = requestedFilterCriteria_;
    filterMode_ = requestedFilterMode_;
    syncFilterControls();
    selectedLogicalRow_ = -1;
    selectedSpanIndex_ = -1;
    hasCursor_ = false;
    hasMarker_ = false;
    markerTimestamp_ = 0;
    markerLogicalRow_ = -1;
    markerSpanIndex_ = -1;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    hoveredSpanIndex_ = -1;
    horizontalZoom_ = 1.0;
    dragMode_ = DragMode::None;
    leftDragGestureActive_ = false;
    opcodeTagPressPending_ = false;
    viewHistory_.clear();
    buildRequested_ = false;
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::setRows(std::vector<FlitRecord> rows)
{
    stopFilterThread();
    stopBuildThread();
    traceSession_.reset();
    rows_ = std::move(rows);
    clearSpanData();
    lastBuildWorkerCount_ = 1;
    sourceMode_ = rows_.empty() ? SourceMode::Empty : SourceMode::RowSnapshot;
    requestedFilterCriteria_ = {};
    requestedFilterMode_ = TransactionFilterMode::Highlight;
    filterCriteria_ = requestedFilterCriteria_;
    filterMode_ = requestedFilterMode_;
    syncFilterControls();
    selectedLogicalRow_ = -1;
    selectedSpanIndex_ = -1;
    hasCursor_ = false;
    hasMarker_ = false;
    markerTimestamp_ = 0;
    markerLogicalRow_ = -1;
    markerSpanIndex_ = -1;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    hoveredSpanIndex_ = -1;
    horizontalZoom_ = 1.0;
    dragMode_ = DragMode::None;
    leftDragGestureActive_ = false;
    viewHistory_.clear();
    buildRequested_ = false;
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::buildView()
{
    buildRequested_ = true;
    refresh();
}

void TransactionWidget::clear()
{
    stopFilterThread();
    stopBuildThread();
    traceSession_.reset();
    rows_.clear();
    clearSpanData();
    lastBuildWorkerCount_ = 1;
    sourceMode_ = SourceMode::Empty;
    requestedFilterCriteria_ = {};
    requestedFilterMode_ = TransactionFilterMode::Highlight;
    filterCriteria_ = requestedFilterCriteria_;
    filterMode_ = requestedFilterMode_;
    syncFilterControls();
    selectedLogicalRow_ = -1;
    selectedSpanIndex_ = -1;
    hasCursor_ = false;
    hasMarker_ = false;
    cursorTimestamp_ = 0;
    markerTimestamp_ = 0;
    markerLogicalRow_ = -1;
    markerSpanIndex_ = -1;
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    hoveredSpanIndex_ = -1;
    building_ = false;
    filtering_ = false;
    filterRestartPending_ = false;
    resetFilterProgress();
    buildRequested_ = false;
    dragMode_ = DragMode::None;
    leftDragGestureActive_ = false;
    opcodeTagPressPending_ = false;
    viewHistory_.clear();
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::setSelectedLogicalRow(const int logicalRow)
{
    const int previousOpcodeTagSpanIndex = selectedOpcodeTagSpanIndex_;
    const std::uint64_t previousOpcodeTagLogicalRow = selectedOpcodeTagLogicalRow_;
    const qint64 previousOpcodeTagTimestamp = selectedOpcodeTagTimestamp_;
    const bool preserveOpcodeTagSelection =
        previousOpcodeTagSpanIndex >= 0
        && logicalRow >= 0
        && previousOpcodeTagLogicalRow == static_cast<std::uint64_t>(logicalRow);

    selectedLogicalRow_ = logicalRow >= 0 ? logicalRow : -1;
    selectedSpanIndex_ = -1;
    if (selectedLogicalRow_ >= 0 && !hasPendingBuild()) {
        selectedSpanIndex_ = spanIndexForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
        if (selectedSpanIndex_ >= 0 && shouldDrawFilteredOutSpan(selectedSpanIndex_)) {
            selectedSpanIndex_ = -1;
        }
        if (selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())) {
            if (shouldDrawFilteredOutSpan(selectedSpanIndex_)) {
                return;
            }
            const TransactionSpan& span = spans_[static_cast<std::size_t>(selectedSpanIndex_)];
            cursorTimestamp_ = timestampForLogicalRowInSpan(span, static_cast<std::uint64_t>(selectedLogicalRow_));
            hasCursor_ = true;
            ensureSpanVisible(selectedSpanIndex_);
        }
    }
    clearSelectedOpcodeTag();
    if (preserveOpcodeTagSelection
        && selectedSpanIndex_ == previousOpcodeTagSpanIndex
        && selectedSpanIndex_ >= 0
        && selectedSpanIndex_ < static_cast<int>(spans_.size())) {
        selectedOpcodeTagSpanIndex_ = previousOpcodeTagSpanIndex;
        selectedOpcodeTagLogicalRow_ = previousOpcodeTagLogicalRow;
        selectedOpcodeTagTimestamp_ = previousOpcodeTagTimestamp;
        stickCursorToSelectedOpcodeTag(true);
    }
    updateWidgetHints();
    update();
}

void TransactionWidget::setMarkers(std::vector<TraceMarker> markers, const QString& selectedMarkerId)
{
    sharedMarkers_ = std::move(markers);
    selectedMarkerId_ = selectedMarkerId;
    if (!selectedMarkerId_.isEmpty()) {
        const auto found = std::find_if(sharedMarkers_.cbegin(), sharedMarkers_.cend(), [&](const TraceMarker& marker) {
            return marker.id == selectedMarkerId_;
        });
        if (found == sharedMarkers_.cend()) {
            selectedMarkerId_.clear();
        }
    }
    update();
}

QVariantMap TransactionWidget::viewState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("sourceMode"), sourceModeName(sourceMode_));
    state.insert(QStringLiteral("arrangeMode"), arrangeModeName(arrangeMode_));
    state.insert(QStringLiteral("filterMode"), filterModeName(requestedFilterMode_));
    state.insert(QStringLiteral("opcodeFilter"), requestedFilterCriteria_.opcode);
    state.insert(QStringLiteral("addressFilter"), requestedFilterCriteria_.address);
    state.insert(QStringLiteral("txnIdFilter"), requestedFilterCriteria_.txnId);
    state.insert(QStringLiteral("dbidFilter"), requestedFilterCriteria_.dbid);
    state.insert(QStringLiteral("filterActive"), requestedTransactionFilterActive());
    state.insert(QStringLiteral("showOpcodeTags"), showOpcodeTags_);
    state.insert(QStringLiteral("showHoverCards"), showHoverCards_);
    state.insert(QStringLiteral("filterMatchCount"), QVariant::fromValue<qulonglong>(filterMatchCount_));
    state.insert(QStringLiteral("unfilteredSpanCount"), static_cast<int>(unfilteredSpans_.size()));
    state.insert(QStringLiteral("displayingFilteredSpans"), displayingFilteredSpans_);
    state.insert(QStringLiteral("selectedLogicalRow"), selectedLogicalRow_);
    state.insert(QStringLiteral("selectedSpanIndex"), selectedSpanIndex_);
    state.insert(QStringLiteral("hasCursor"), hasCursor_);
    state.insert(QStringLiteral("cursorTimestamp"), cursorTimestamp_);
    state.insert(QStringLiteral("hasMarker"), hasMarker_);
    state.insert(QStringLiteral("markerTimestamp"), markerTimestamp_);
    state.insert(QStringLiteral("markerLogicalRow"), markerLogicalRow_);
    state.insert(QStringLiteral("markerSpanIndex"), markerSpanIndex_);
    state.insert(QStringLiteral("horizontalZoom"), horizontalZoom_);
    state.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(scrollOffset_));
    state.insert(QStringLiteral("verticalScrollOffset"), verticalScrollOffset_);
    state.insert(QStringLiteral("rowHeight"), rowHeight());
    state.insert(QStringLiteral("spanCount"), static_cast<int>(spans_.size()));
    state.insert(QStringLiteral("laneCount"), static_cast<int>(lanes_.size()));
    state.insert(QStringLiteral("stackRowCount"), stackRowCount());
    state.insert(QStringLiteral("groupCount"), transactionGroupCount());
    state.insert(QStringLiteral("maxStackDepth"), maxStackDepth());
    return state;
}

QVariantMap TransactionWidget::diagnosticsState() const
{
    QVariantMap state = viewState();
    state.insert(QStringLiteral("status"), statusText_);
    state.insert(QStringLiteral("building"), building_);
    state.insert(QStringLiteral("filtering"), filtering_);
    state.insert(QStringLiteral("filterProgressCompleted"), QVariant::fromValue<qulonglong>(filterProgressCompleted_));
    state.insert(QStringLiteral("filterProgressTotal"), QVariant::fromValue<qulonglong>(filterProgressTotal_));
    state.insert(QStringLiteral("processedXactions"), QVariant::fromValue<qulonglong>(processedXactions_));
    state.insert(QStringLiteral("totalXactions"), QVariant::fromValue<qulonglong>(totalXactions_));
    state.insert(QStringLiteral("buildProgressPhase"), buildProgressPhaseName(buildProgressPhase_));
    state.insert(QStringLiteral("buildProgressCompleted"), QVariant::fromValue<qulonglong>(buildProgressCompleted_));
    state.insert(QStringLiteral("buildProgressTotal"), QVariant::fromValue<qulonglong>(buildProgressTotal_));
    state.insert(QStringLiteral("lastBuildWorkerCount"), QVariant::fromValue<qulonglong>(lastBuildWorkerCount_));
    state.insert(QStringLiteral("lastBuildMetadataMs"), static_cast<double>(lastBuildMetadataNs_) / 1000000.0);
    state.insert(QStringLiteral("lastBuildScanMs"), static_cast<double>(lastBuildScanNs_) / 1000000.0);
    state.insert(QStringLiteral("lastBuildMergeMs"), static_cast<double>(lastBuildMergeNs_) / 1000000.0);
    state.insert(QStringLiteral("lastBuildFirstInfoMs"), static_cast<double>(lastBuildFirstInfoNs_) / 1000000.0);
    state.insert(QStringLiteral("lastBuildSpanMs"), static_cast<double>(lastBuildSpanNs_) / 1000000.0);
    state.insert(QStringLiteral("lastBuildSortMs"), static_cast<double>(lastBuildSortNs_) / 1000000.0);
    state.insert(QStringLiteral("lastBuildLayoutMs"), static_cast<double>(lastBuildLayoutNs_) / 1000000.0);
    state.insert(QStringLiteral("buildGeneration"),
                 QVariant::fromValue<qulonglong>(buildGeneration_.load(std::memory_order_relaxed)));
    state.insert(QStringLiteral("visibleStart"), visibleStartTimestamp());
    state.insert(QStringLiteral("visibleEnd"), visibleEndTimestamp());
    state.insert(QStringLiteral("selectedOrdinal"),
                 selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                     ? QVariant::fromValue<qulonglong>(spans_[static_cast<std::size_t>(selectedSpanIndex_)].ordinal)
                     : QVariant());
    state.insert(QStringLiteral("selectedEndpoint"),
                 selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                     ? QVariant(spans_[static_cast<std::size_t>(selectedSpanIndex_)].endpointLabel)
                     : QVariant());
    state.insert(QStringLiteral("selectedOrigin"),
                 selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                     ? QVariant(spans_[static_cast<std::size_t>(selectedSpanIndex_)].originKind)
                     : QVariant());
    state.insert(QStringLiteral("selectedOpcode"),
                 selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                     ? QVariant(spans_[static_cast<std::size_t>(selectedSpanIndex_)].requestOpcode)
                     : QVariant());
    state.insert(QStringLiteral("selectedAddress"),
                 selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                     ? QVariant(spans_[static_cast<std::size_t>(selectedSpanIndex_)].addressLabel)
                     : QVariant());
    state.insert(QStringLiteral("selectedTxnID"),
                 selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                     ? QVariant(spans_[static_cast<std::size_t>(selectedSpanIndex_)].txnIdLabel)
                     : QVariant());
    state.insert(QStringLiteral("selectedDBID"),
                 selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                     ? QVariant(spans_[static_cast<std::size_t>(selectedSpanIndex_)].dbidLabel)
                     : QVariant());
    state.insert(QStringLiteral("hoveredSpanIndex"), hoveredSpanIndex_);
    return state;
}

void TransactionWidget::restoreViewState(const QVariantMap& state)
{
    stopFilterThread();
    if (state.contains(QStringLiteral("sourceMode"))) {
        sourceMode_ = sourceModeFromName(state.value(QStringLiteral("sourceMode")).toString());
    }
    if (state.contains(QStringLiteral("arrangeMode"))) {
        arrangeMode_ = arrangeModeFromName(state.value(QStringLiteral("arrangeMode")).toString());
    }
    if (state.contains(QStringLiteral("filterMode"))) {
        requestedFilterMode_ = filterModeFromName(state.value(QStringLiteral("filterMode")).toString());
    }
    requestedFilterCriteria_.opcode = state.value(QStringLiteral("opcodeFilter")).toString().trimmed();
    requestedFilterCriteria_.address = state.value(QStringLiteral("addressFilter")).toString().trimmed();
    requestedFilterCriteria_.txnId = state.value(QStringLiteral("txnIdFilter")).toString().trimmed();
    requestedFilterCriteria_.dbid = state.value(QStringLiteral("dbidFilter")).toString().trimmed();
    filterMode_ = requestedFilterMode_;
    filterCriteria_ = requestedFilterCriteria_;
    showOpcodeTags_ = state.value(QStringLiteral("showOpcodeTags"), showOpcodeTags_).toBool();
    showHoverCards_ = state.value(QStringLiteral("showHoverCards"), showHoverCards_).toBool();
    syncFilterControls();
    selectedLogicalRow_ = state.value(QStringLiteral("selectedLogicalRow"), -1).toInt();
    if (selectedLogicalRow_ < 0) {
        selectedLogicalRow_ = -1;
    }
    selectedSpanIndex_ = state.value(QStringLiteral("selectedSpanIndex"), -1).toInt();
    if (selectedSpanIndex_ < -1) {
        selectedSpanIndex_ = -1;
    }
    hasCursor_ = state.value(QStringLiteral("hasCursor"), false).toBool();
    cursorTimestamp_ = state.value(QStringLiteral("cursorTimestamp"), 0).toLongLong();
    hasMarker_ = state.value(QStringLiteral("hasMarker"), false).toBool();
    markerTimestamp_ = state.value(QStringLiteral("markerTimestamp"), 0).toLongLong();
    markerLogicalRow_ = state.value(QStringLiteral("markerLogicalRow"), -1).toInt();
    if (markerLogicalRow_ < 0) {
        markerLogicalRow_ = -1;
    }
    markerSpanIndex_ = state.value(QStringLiteral("markerSpanIndex"), -1).toInt();
    if (markerSpanIndex_ < -1) {
        markerSpanIndex_ = -1;
    }
    horizontalZoom_ = std::clamp(state.value(QStringLiteral("horizontalZoom"), 1.0).toDouble(),
                                 kMinZoom,
                                 kMaxZoom);
    scrollOffset_ = state.value(QStringLiteral("scrollOffset"), 0).toULongLong();
    verticalScrollOffset_ = std::max(0, state.value(QStringLiteral("verticalScrollOffset"), 0).toInt());
    rowHeight_ = std::clamp(state.value(QStringLiteral("rowHeight"), rowHeight()).toInt(),
                            kLaneRowMinHeight,
                            kLaneRowMaxHeight);
    if (!unfilteredSpans_.empty()) {
        arrangeSpans(unfilteredSpans_, unfilteredLanes_, arrangeMode_);
        applyTransactionFilter();
    } else if (!spans_.empty()) {
        rebuildArrangement();
    }
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

QString TransactionWidget::sourceModeName(const SourceMode mode)
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

QString TransactionWidget::buildProgressPhaseName(const BuildProgressPhase phase)
{
    switch (phase) {
    case BuildProgressPhase::Idle:
        return QStringLiteral("idle");
    case BuildProgressPhase::LoadingMetadata:
        return QStringLiteral("loading metadata");
    case BuildProgressPhase::ScanningRows:
        return QStringLiteral("scanning rows");
    case BuildProgressPhase::AggregatingSpans:
        return QStringLiteral("aggregating spans");
    case BuildProgressPhase::LayingOutRows:
        return QStringLiteral("laying out rows");
    case BuildProgressPhase::Finalizing:
        return QStringLiteral("finalizing");
    }
    return QStringLiteral("building");
}

QString TransactionWidget::arrangeModeName(const ArrangeMode mode)
{
    switch (mode) {
    case ArrangeMode::InflightStack:
        return QStringLiteral("stack");
    case ArrangeMode::TxnId:
        return QStringLiteral("txnId");
    }
    return QStringLiteral("stack");
}

TransactionWidget::ArrangeMode TransactionWidget::arrangeModeFromName(const QString& name)
{
    if (name == QLatin1String("txnId") || name == QLatin1String("TxnID")) {
        return ArrangeMode::TxnId;
    }
    return ArrangeMode::InflightStack;
}

QString TransactionWidget::filterModeName(const TransactionFilterMode mode)
{
    switch (mode) {
    case TransactionFilterMode::Highlight:
        return QStringLiteral("highlight");
    case TransactionFilterMode::Filter:
        return QStringLiteral("filter");
    }
    return QStringLiteral("highlight");
}

TransactionWidget::TransactionFilterMode TransactionWidget::filterModeFromName(const QString& name)
{
    if (name == QLatin1String("filter") || name == QLatin1String("Filter")) {
        return TransactionFilterMode::Filter;
    }
    return TransactionFilterMode::Highlight;
}

TransactionWidget::SourceMode TransactionWidget::sourceModeFromName(const QString& name)
{
    if (name == QLatin1String("traceSession")) {
        return SourceMode::TraceSession;
    }
    if (name == QLatin1String("rowSnapshot")) {
        return SourceMode::RowSnapshot;
    }
    return SourceMode::Empty;
}

void TransactionWidget::stopBuildThread()
{
    ++buildGeneration_;
    stopFilterThread();
    if (buildThread_.joinable()) {
        buildThread_.request_stop();
    }
    building_ = false;
    resetBuildProgress();
}

void TransactionWidget::stopFilterThread()
{
    ++filterGeneration_;
    if (filterThread_.joinable()) {
        filterThread_.request_stop();
        filterThread_.join();
    }
    filtering_ = false;
    filterRestartPending_ = false;
    resetFilterProgress();
}

void TransactionWidget::refresh()
{
    stopFilterThread();
    clearSpanData();
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();

    if (!buildRequested_) {
        return;
    }

    if (sourceMode_ == SourceMode::TraceSession
        && traceSession_
        && traceSession_->supportsXactionIndexing()
        && traceSession_->isXactionIndexComplete()) {
        startSessionBuild(traceSession_);
    }
}

void TransactionWidget::startSessionBuild(std::shared_ptr<TraceSession> session)
{
    const quint64 generation = ++buildGeneration_;
    building_ = true;
    processedXactions_ = 0;
    totalXactions_ = 0;
    buildProgressPhase_ = BuildProgressPhase::LoadingMetadata;
    buildProgressCompleted_ = 0;
    buildProgressTotal_ = 0;
    lastBuildWorkerCount_ = 1;
    lastBuildMetadataNs_ = 0;
    lastBuildScanNs_ = 0;
    lastBuildMergeNs_ = 0;
    lastBuildFirstInfoNs_ = 0;
    lastBuildSpanNs_ = 0;
    lastBuildSortNs_ = 0;
    lastBuildLayoutNs_ = 0;
    statusText_ = QStringLiteral("Building transaction spans...");
    updateWidgetHints();
    update();

    buildThread_ = std::jthread([this, session = std::move(session), generation](const std::stop_token stopToken) {
        std::shared_ptr<BuildResult> result = buildSpansFromSession(session, generation, arrangeMode_, stopToken);
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

std::shared_ptr<TransactionWidget::BuildResult> TransactionWidget::buildSpansFromSession(
    const std::shared_ptr<TraceSession>& session,
    const quint64 generation,
    const ArrangeMode arrangeMode,
    const std::stop_token stopToken) const
{
    auto result = std::make_shared<BuildResult>();
    result->generation = generation;
    result->arrangeMode = arrangeMode;

    if (!session) {
        result->statusText = QStringLiteral("No transaction trace is loaded.");
        return result;
    }
    if (!session->supportsXactionIndexing()) {
        result->statusText = QStringLiteral("This trace does not support xaction indexing.");
        return result;
    }
    if (!session->isXactionIndexComplete()) {
        result->statusText = QStringLiteral("Build the xaction index to display transaction timelines.");
        return result;
    }

    const std::vector<std::uint64_t> ordinals = session->xactionOrdinals();
    result->totalXactions = static_cast<std::uint64_t>(ordinals.size());
    QElapsedTimer phaseTimer;
    phaseTimer.start();

    TransactionWidget* widget = const_cast<TransactionWidget*>(this);
    std::atomic<std::int64_t> lastProgressPublishTimeMs = 0;
    const auto publishProgress = [&](const BuildProgressPhase phase,
                                     const std::uint64_t completedWork,
                                     const std::uint64_t totalWork,
                                     const bool force = false) {
        if (stopToken.stop_requested()
            || generation != buildGeneration_.load(std::memory_order_relaxed)) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        std::int64_t previousMs = lastProgressPublishTimeMs.load(std::memory_order_relaxed);
        if (!force && nowMs - previousMs < kBuildProgressPublishInterval.count()) {
            return;
        }
        if (!force
            && !lastProgressPublishTimeMs.compare_exchange_strong(previousMs,
                                                                  nowMs,
                                                                  std::memory_order_relaxed,
                                                                  std::memory_order_relaxed)) {
            return;
        }
        if (force) {
            lastProgressPublishTimeMs.store(nowMs, std::memory_order_relaxed);
        }
        QTimer::singleShot(0, widget, [widget, generation, phase, completedWork, totalWork]() {
            widget->updateBuildProgress(generation, phase, completedWork, totalWork);
        });
    };

    publishProgress(BuildProgressPhase::LoadingMetadata, 0, result->totalXactions, true);

    std::unordered_set<std::uint64_t> completedOrdinals;
    if (!session->xactionCompletedOrdinals(completedOrdinals)) {
        result->failed = true;
        result->statusText = QStringLiteral("Transaction spans unavailable: failed to read completed Xaction metadata.");
        return result;
    }

    std::unordered_map<std::uint64_t, QString> typeByOrdinal;
    if (!session->xactionTypesByOrdinal(typeByOrdinal)) {
        result->failed = true;
        result->statusText = QStringLiteral("Transaction spans unavailable: failed to read Xaction type metadata.");
        return result;
    }

    std::unordered_map<std::uint64_t, std::uint64_t> rowCountsByOrdinal;
    if (!session->xactionRowCountsByOrdinal(rowCountsByOrdinal)) {
        result->failed = true;
        result->statusText = QStringLiteral("Transaction spans unavailable: failed to read Xaction row-count metadata.");
        return result;
    }
    result->metadataNs = phaseTimer.nsecsElapsed();
    phaseTimer.restart();

    const std::uint64_t totalRows = session->totalRows();
    result->workerCount = transactionSpanBuildWorkerCount(static_cast<std::size_t>(
        std::min<std::uint64_t>(totalRows,
                                static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))));
    if (ordinals.empty() || totalRows == 0) {
        result->processedXactions = result->totalXactions;
    } else {
        std::vector<TransactionBlockSlice> slices;
        slices.reserve(session->metadata().blocks.size());
        for (std::size_t blockIndex = 0; blockIndex < session->metadata().blocks.size(); ++blockIndex) {
            const CLogBTraceBlockSummary& block = session->metadata().blocks[blockIndex];
            for (std::uint64_t localBegin = 0; localBegin < block.recordCount; ) {
                const std::uint64_t rowCount =
                    std::min<std::uint64_t>(kTransactionSpanScanSliceRows, block.recordCount - localBegin);
                slices.push_back(TransactionBlockSlice{
                    .blockIndex = blockIndex,
                    .beginRow = block.rowStart + localBegin,
                    .rowCount = rowCount,
                    .localBegin = static_cast<std::size_t>(localBegin),
                });
                localBegin += rowCount;
            }
        }

        if (slices.empty()) {
            result->processedXactions = result->totalXactions;
        } else {
        std::atomic<std::size_t> nextSliceIndex = 0;
        std::atomic<std::uint64_t> scannedRows = 0;
        std::atomic<std::uint64_t> lastProgressRows = 0;
        const std::uint64_t progressRowInterval =
            std::max<std::uint64_t>(kBuildProgressStep, totalRows / 1000U);
        std::atomic_bool failed = false;
        std::atomic_bool cancelled = false;
        std::mutex failureMutex;
        QString firstFailureText;

        const auto recordFailure = [&](const QString& failureText) {
            bool expected = false;
            if (failed.compare_exchange_strong(expected,
                                               true,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed)) {
                const std::lock_guard lock(failureMutex);
                firstFailureText = failureText;
            }
        };

        result->workerCount = std::min<std::size_t>(result->workerCount,
                                                    std::max<std::size_t>(1, slices.size()));
        std::vector<TransactionSpanAccumulator> accumulators(ordinals.size());
        std::unordered_map<std::uint64_t, std::size_t> ordinalIndexByOrdinal;
        ordinalIndexByOrdinal.reserve(ordinals.size());
        for (std::size_t ordinalIndex = 0; ordinalIndex < ordinals.size(); ++ordinalIndex) {
            accumulators[ordinalIndex].ordinal = ordinals[ordinalIndex];
            ordinalIndexByOrdinal.emplace(ordinals[ordinalIndex], ordinalIndex);
        }
        std::vector<TransactionSpanAccumulatorLock> accumulatorLocks(
            std::min<std::size_t>(kTransactionSpanAccumulatorLockCount,
                                  std::max<std::size_t>(1, accumulators.size())));

        const auto workerBody = [&](const std::size_t workerIndex) {
            (void)workerIndex;
            CLogBTraceLoadError error;
            while (!failed.load(std::memory_order_relaxed)
                   && !cancelled.load(std::memory_order_relaxed)
                   && !stopToken.stop_requested()) {
                const std::size_t sliceIndex = nextSliceIndex.fetch_add(1, std::memory_order_relaxed);
                if (sliceIndex >= slices.size()) {
                    return;
                }

                const TransactionBlockSlice& slice = slices[sliceIndex];
                error = CLogBTraceLoadError{};
                if (!scanTransactionSpanSliceIntoAccumulators(session,
                                                              slice,
                                                              ordinalIndexByOrdinal,
                                                              accumulators,
                                                              accumulatorLocks,
                                                              error,
                                                              stopToken)) {
                    if (stopToken.stop_requested() || error.type == CLogBTraceLoadError::Type::Cancelled) {
                        cancelled.store(true, std::memory_order_relaxed);
                    } else {
                        recordFailure(error.summary.isEmpty()
                                          ? QStringLiteral("Transaction spans unavailable: failed to scan indexed rows.")
                                          : QStringLiteral("Transaction spans unavailable: %1").arg(error.summary));
                    }
                    return;
                }

                const std::uint64_t completedRows =
                    scannedRows.fetch_add(slice.rowCount, std::memory_order_relaxed) + slice.rowCount;
                const std::uint64_t previousProgressRows = lastProgressRows.load(std::memory_order_relaxed);
                if (completedRows == totalRows || completedRows - previousProgressRows >= progressRowInterval) {
                    lastProgressRows.store(completedRows, std::memory_order_relaxed);
                    publishProgress(BuildProgressPhase::ScanningRows,
                                    std::min(completedRows, totalRows),
                                    totalRows);
                }
            }
        };

        if (result->workerCount <= 1) {
            workerBody(0);
        } else {
            std::vector<std::jthread> workers;
            workers.reserve(result->workerCount);
            for (std::size_t workerIndex = 0; workerIndex < result->workerCount; ++workerIndex) {
                workers.emplace_back([&, workerIndex]() {
                    try {
                        workerBody(workerIndex);
                    } catch (const std::bad_alloc&) {
                        recordFailure(QStringLiteral("Transaction spans unavailable: build ran out of memory."));
                    } catch (const std::exception& exception) {
                        recordFailure(QStringLiteral("Transaction spans unavailable: row-scan worker failed unexpectedly (%1).")
                                          .arg(QString::fromLocal8Bit(exception.what())));
                    } catch (...) {
                        recordFailure(QStringLiteral("Transaction spans unavailable: row-scan worker failed unexpectedly."));
                    }
                });
            }
        }

        if (stopToken.stop_requested()
            || cancelled.load(std::memory_order_relaxed)
            || generation != buildGeneration_.load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }
        if (failed.load(std::memory_order_relaxed)) {
            result->failed = true;
            {
                const std::lock_guard lock(failureMutex);
                result->statusText = firstFailureText.isEmpty()
                    ? QStringLiteral("Transaction spans unavailable: parallel row scan failed.")
                    : firstFailureText;
            }
            return result;
        }

        if (stopToken.stop_requested()
            || generation != buildGeneration_.load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }
        publishProgress(BuildProgressPhase::ScanningRows, totalRows, totalRows, true);
        result->scanNs = phaseTimer.nsecsElapsed();
        phaseTimer.restart();

        std::size_t estimatedAccumulatorCount = 0;
        for (const TransactionSpanAccumulator& accumulator : accumulators) {
            if (accumulator.rowCount != 0) {
                ++estimatedAccumulatorCount;
            }
        }
        const std::uint64_t aggregationTotal =
            std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::max<std::size_t>(1, estimatedAccumulatorCount)) * 3U);
        std::uint64_t aggregationCompleted = 0;
        const auto publishAggregationProgress = [&](const std::uint64_t completed, const bool force = false) {
            aggregationCompleted = std::min<std::uint64_t>(completed, aggregationTotal);
            publishProgress(BuildProgressPhase::AggregatingSpans,
                            aggregationCompleted,
                            aggregationTotal,
                            force);
        };
        publishAggregationProgress(0, true);

        publishAggregationProgress(estimatedAccumulatorCount, true);
        result->mergeNs = phaseTimer.nsecsElapsed();
        phaseTimer.restart();

        std::vector<std::uint64_t> firstRows;
        firstRows.reserve(estimatedAccumulatorCount);
        std::vector<std::size_t> activeAccumulatorIndexes;
        activeAccumulatorIndexes.reserve(estimatedAccumulatorCount);
        for (std::size_t accumulatorIndex = 0; accumulatorIndex < accumulators.size(); ++accumulatorIndex) {
            TransactionSpanAccumulator& accumulator = accumulators[accumulatorIndex];
            if (accumulator.rowCount == 0) {
                continue;
            }
            if (accumulator.hasFirstRow) {
                firstRows.push_back(accumulator.firstRow.logicalRow);
            }
            activeAccumulatorIndexes.push_back(accumulatorIndex);
        }

        std::unordered_map<std::uint64_t, TraceSession::XactionIndexRowInfo> firstInfoByRow;
        const std::uint64_t firstInfoBaseUnits =
            static_cast<std::uint64_t>(std::max<std::size_t>(1, estimatedAccumulatorCount));
        const bool firstInfoLoaded = session->xactionRowInfosForRows(
            firstRows,
            firstInfoByRow,
            true,
            [&](const std::size_t completedRows, const std::size_t totalRowsForInfo) {
                const std::uint64_t denominator =
                    static_cast<std::uint64_t>(std::max<std::size_t>(1, totalRowsForInfo));
                const std::uint64_t scaled =
                    firstInfoBaseUnits
                    + (static_cast<std::uint64_t>(completedRows) * firstInfoBaseUnits) / denominator;
                publishAggregationProgress(scaled);
            });
        if (!firstInfoLoaded) {
            firstInfoByRow.clear();
            for (std::size_t index = 0; index < firstRows.size(); ++index) {
                if (stopToken.stop_requested()
                    || generation != buildGeneration_.load(std::memory_order_relaxed)) {
                    result->cancelled = true;
                    return result;
                }
                TraceSession::XactionIndexRowInfo info;
                if (session->xactionRowInfo(firstRows[index], info)) {
                    firstInfoByRow.emplace(firstRows[index], std::move(info));
                }
                const std::uint64_t scaled =
                    firstInfoBaseUnits
                    + (static_cast<std::uint64_t>(index + 1) * firstInfoBaseUnits)
                          / static_cast<std::uint64_t>(std::max<std::size_t>(1, firstRows.size()));
                publishAggregationProgress(scaled);
            }
        }
        result->firstInfoNs = phaseTimer.nsecsElapsed();
        phaseTimer.restart();

        if (stopToken.stop_requested()
            || generation != buildGeneration_.load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }

        std::vector<SpanBuildWorkerResult> spanResults(result->workerCount);
        std::atomic_bool aggregationCancelled = false;
        std::atomic<std::size_t> nextAccumulatorIndex = 0;
        std::atomic<std::uint64_t> builtAccumulatorCount = 0;
        const auto buildAccumulatorPartition = [&](const std::size_t workerIndex) {
            SpanBuildWorkerResult& workerResult = spanResults[workerIndex];
            while (!failed.load(std::memory_order_relaxed)
                   && !stopToken.stop_requested()
                   && generation == buildGeneration_.load(std::memory_order_relaxed)) {
                const std::size_t accumulatorIndex =
                    nextAccumulatorIndex.fetch_add(1, std::memory_order_relaxed);
                if (accumulatorIndex >= activeAccumulatorIndexes.size()) {
                    return;
                }

                TransactionSpanAccumulator& accumulator =
                    accumulators[activeAccumulatorIndexes[accumulatorIndex]];
                ++workerResult.processedXactions;
                const auto expectedCountFound = rowCountsByOrdinal.find(accumulator.ordinal);
                const std::uint64_t expectedRowCount = expectedCountFound == rowCountsByOrdinal.end()
                    ? accumulator.rowCount
                    : expectedCountFound->second;
                if (!accumulator.hasFirstRow
                    || !accumulator.hasLastRow
                    || accumulator.rowCount != expectedRowCount) {
                    ++workerResult.skippedXactions;
                    const std::uint64_t completed =
                        builtAccumulatorCount.fetch_add(1, std::memory_order_relaxed) + 1;
                    publishAggregationProgress(firstInfoBaseUnits * 2U + completed);
                    continue;
                }

                const auto firstInfoFound = firstInfoByRow.find(accumulator.firstRow.logicalRow);
                if (firstInfoFound == firstInfoByRow.end()) {
                    ++workerResult.skippedXactions;
                    const std::uint64_t completed =
                        builtAccumulatorCount.fetch_add(1, std::memory_order_relaxed) + 1;
                    publishAggregationProgress(firstInfoBaseUnits * 2U + completed);
                    continue;
                }

                const TraceSession::XactionIndexRowInfo& firstInfo = firstInfoFound->second;
                if (firstInfo.xactionOrdinal != 0 && firstInfo.xactionOrdinal != accumulator.ordinal) {
                    ++workerResult.skippedXactions;
                    const std::uint64_t completed =
                        builtAccumulatorCount.fetch_add(1, std::memory_order_relaxed) + 1;
                    publishAggregationProgress(firstInfoBaseUnits * 2U + completed);
                    continue;
                }

                SpanClassification classification = classifySpan(session, firstInfo, accumulator);
                QString xactionType = firstInfo.xactionType.trimmed();
                if (xactionType.isEmpty() || xactionType == QLatin1String("none")) {
                    const auto typeFound = typeByOrdinal.find(accumulator.ordinal);
                    xactionType = typeFound == typeByOrdinal.end() || typeFound->second.trimmed().isEmpty()
                        ? QStringLiteral("Unknown Xaction")
                        : typeFound->second.trimmed();
                }

                TransactionSpan span;
                span.ordinal = accumulator.ordinal;
                span.transactionGroupKey = firstInfo.transactionGroupKey.trimmed();
                if (span.transactionGroupKey.isEmpty()) {
                    span.transactionGroupKey =
                        QStringLiteral("xaction|%1").arg(static_cast<qulonglong>(accumulator.ordinal));
                }
                span.xactionType = xactionType;
                span.requestOpcode = classification.requestOpcode;
                span.opcodeValues = accumulator.opcodeValues;
                appendUniqueSearchValue(span.opcodeValues, span.requestOpcode);
                appendUniqueSearchValue(span.opcodeValues, opcodeWithoutChannelPrefix(span.requestOpcode));
                span.addressLabel = classification.addressLabel;
                span.jointFamily = classification.jointFamily;
                span.jointPath = firstInfo.jointPath;
                span.endpointLabel = classification.endpointLabel;
                span.endpointNode = classification.endpointNode;
                span.endpointNodeType = classification.endpointNodeType;
                span.originKind = classification.originKind;
                span.txnIdLabel = accumulator.firstRow.txnId.trimmed();
                span.dbidLabel = accumulator.dbidValues.empty()
                    ? QString()
                    : accumulator.dbidValues.front();
                span.classificationConfidence = classification.confidence;
                span.firstLogicalRow = accumulator.firstRow.logicalRow;
                span.lastLogicalRow = accumulator.lastRow.logicalRow;
                span.requestLogicalRow = classification.requestLogicalRow;
                span.startTimestamp = accumulator.firstRow.timestamp;
                span.endTimestamp = accumulator.lastRow.timestamp;
                span.rowCount = accumulator.rowCount;
                span.completed = accumulator.completionLogicalRow.has_value()
                    || firstInfo.xactionComplete
                    || completedOrdinals.find(accumulator.ordinal) != completedOrdinals.end();
                span.indexed = accumulator.indexed || firstInfo.indexed;
                span.processedByJoint = accumulator.processedByJoint || firstInfo.processedByJoint;
                span.txnIdValues = accumulator.txnIdValues;
                span.dbidValues = accumulator.dbidValues;
                if (accumulator.completionLogicalRow) {
                    span.completionLogicalRow = accumulator.completionLogicalRow;
                    span.endTimestamp = accumulator.completionTimestamp;
                }
                span.logicalRows.reserve(4);
                span.logicalRows.push_back(span.firstLogicalRow);
                if (span.requestLogicalRow != span.firstLogicalRow
                    && span.requestLogicalRow != span.lastLogicalRow) {
                    span.logicalRows.push_back(span.requestLogicalRow);
                }
                if (span.completionLogicalRow
                    && *span.completionLogicalRow != span.firstLogicalRow
                    && *span.completionLogicalRow != span.requestLogicalRow
                    && *span.completionLogicalRow != span.lastLogicalRow) {
                    span.logicalRows.push_back(*span.completionLogicalRow);
                }
                if (span.lastLogicalRow != span.firstLogicalRow) {
                    span.logicalRows.push_back(span.lastLogicalRow);
                }
                std::sort(span.logicalRows.begin(), span.logicalRows.end());
                span.logicalRows.erase(std::unique(span.logicalRows.begin(), span.logicalRows.end()),
                                       span.logicalRows.end());
                workerResult.spans.push_back(std::move(span));

                const std::uint64_t completed =
                    builtAccumulatorCount.fetch_add(1, std::memory_order_relaxed) + 1;
                publishAggregationProgress(firstInfoBaseUnits * 2U + completed);
            }
            if (stopToken.stop_requested()
                || generation != buildGeneration_.load(std::memory_order_relaxed)) {
                workerResult.cancelled = true;
                aggregationCancelled.store(true, std::memory_order_release);
            }
        };

        if (result->workerCount <= 1) {
            buildAccumulatorPartition(0);
        } else {
            std::vector<std::jthread> aggregationWorkers;
            aggregationWorkers.reserve(result->workerCount);
            for (std::size_t workerIndex = 0; workerIndex < result->workerCount; ++workerIndex) {
                aggregationWorkers.emplace_back([&, workerIndex]() {
                    try {
                        buildAccumulatorPartition(workerIndex);
                    } catch (const std::bad_alloc&) {
                        recordFailure(QStringLiteral("Transaction spans unavailable: span aggregation ran out of memory."));
                    } catch (const std::exception& exception) {
                        recordFailure(QStringLiteral("Transaction spans unavailable: span aggregation failed unexpectedly (%1).")
                                          .arg(QString::fromLocal8Bit(exception.what())));
                    } catch (...) {
                        recordFailure(QStringLiteral("Transaction spans unavailable: span aggregation failed unexpectedly."));
                    }
                });
            }
        }

        if (stopToken.stop_requested()
            || aggregationCancelled.load(std::memory_order_acquire)
            || generation != buildGeneration_.load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }
        if (failed.load(std::memory_order_relaxed)) {
            result->failed = true;
            {
                const std::lock_guard lock(failureMutex);
                result->statusText = firstFailureText.isEmpty()
                    ? QStringLiteral("Transaction spans unavailable: parallel span aggregation failed.")
                    : firstFailureText;
            }
            return result;
        }

        std::size_t spanCount = 0;
        for (const SpanBuildWorkerResult& workerResult : spanResults) {
            spanCount += workerResult.spans.size();
            result->processedXactions += workerResult.processedXactions;
            result->skippedXactions += workerResult.skippedXactions;
        }
        result->spans.reserve(spanCount);
        for (SpanBuildWorkerResult& workerResult : spanResults) {
            result->spans.insert(result->spans.end(),
                                 std::make_move_iterator(workerResult.spans.begin()),
                                 std::make_move_iterator(workerResult.spans.end()));
        }
        publishAggregationProgress(aggregationTotal, true);
        result->processedXactions = result->totalXactions;
        result->spanBuildNs = phaseTimer.nsecsElapsed();
        }
    }

    phaseTimer.restart();
    std::sort(result->spans.begin(), result->spans.end(), [](const TransactionSpan& lhs,
                                                             const TransactionSpan& rhs) {
        if (lhs.startTimestamp != rhs.startTimestamp) {
            return lhs.startTimestamp < rhs.startTimestamp;
        }
        if (lhs.firstLogicalRow != rhs.firstLogicalRow) {
            return lhs.firstLogicalRow < rhs.firstLogicalRow;
        }
        return lhs.ordinal < rhs.ordinal;
    });
    result->sortNs = phaseTimer.nsecsElapsed();
    phaseTimer.restart();

    if (stopToken.stop_requested()
        || generation != buildGeneration_.load(std::memory_order_relaxed)) {
        result->cancelled = true;
        return result;
    }
    arrangeSpans(result->spans,
                 result->lanes,
                 arrangeMode,
                 [&](const std::uint64_t completedWork, const std::uint64_t totalWork) {
                     publishProgress(BuildProgressPhase::LayingOutRows, completedWork, totalWork);
                 });
    result->layoutNs = phaseTimer.nsecsElapsed();

    publishProgress(BuildProgressPhase::Finalizing, result->spans.size(), result->spans.size(), true);

    if (result->spans.empty()) {
        result->statusText = result->totalXactions == 0
            ? QStringLiteral("No indexed transactions were found.")
            : QStringLiteral("No transaction spans could be built from the xaction index.");
    } else {
        result->statusText = QStringLiteral("Transaction spans ready: %1 spans across %2 stack rows.")
                                 .arg(FormatUnsignedIntegerWithThousands(result->spans.size()),
                                      FormatUnsignedIntegerWithThousands(result->lanes.size()));
    }
    result->processedXactions = result->totalXactions;
    return result;
}

void TransactionWidget::applyBuildResult(std::shared_ptr<BuildResult> result)
{
    if (!result || result->generation != buildGeneration_.load(std::memory_order_relaxed)) {
        return;
    }
    if (result->cancelled) {
        return;
    }

    building_ = false;
    processedXactions_ = result->processedXactions;
    totalXactions_ = result->totalXactions;
    resetBuildProgress();
    lastBuildWorkerCount_ = result->workerCount;
    lastBuildMetadataNs_ = result->metadataNs;
    lastBuildScanNs_ = result->scanNs;
    lastBuildMergeNs_ = result->mergeNs;
    lastBuildFirstInfoNs_ = result->firstInfoNs;
    lastBuildSpanNs_ = result->spanBuildNs;
    lastBuildSortNs_ = result->sortNs;
    lastBuildLayoutNs_ = result->layoutNs;
    if (result->failed) {
        clearSpanData();
        statusText_ = result->statusText;
        updateWidgetHints();
        updateScrollBars();
        update();
        return;
    }

    arrangeMode_ = result->arrangeMode;
    unfilteredLanes_ = std::move(result->lanes);
    unfilteredSpans_ = std::move(result->spans);
    requestedFilterMode_ = filterMode_;
    requestedFilterCriteria_ = filterCriteria_;
    applyTransactionFilter();
    if (!filtering_) {
        statusText_ = result->statusText;
    }
    selectedSpanIndex_ = -1;
    if (markerSpanIndex_ >= static_cast<int>(spans_.size())) {
        markerSpanIndex_ = -1;
    }
    if (selectedLogicalRow_ >= 0) {
        setSelectedLogicalRow(selectedLogicalRow_);
    }
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::updateBuildProgress(const quint64 generation,
                                            const BuildProgressPhase phase,
                                            const std::uint64_t completedWork,
                                            const std::uint64_t totalWork)
{
    if (generation != buildGeneration_.load(std::memory_order_relaxed) || !building_) {
        return;
    }
    buildProgressPhase_ = phase;
    buildProgressCompleted_ = totalWork == 0 ? completedWork : std::min(completedWork, totalWork);
    buildProgressTotal_ = totalWork;
    if (phase == BuildProgressPhase::AggregatingSpans || phase == BuildProgressPhase::LayingOutRows) {
        processedXactions_ = buildProgressCompleted_;
        totalXactions_ = totalWork;
    }
    statusText_ = buildProgressText();
    updateWidgetHints();
    update();
}

void TransactionWidget::updateFilterProgress(const quint64 generation,
                                             const std::uint64_t completedWork,
                                             const std::uint64_t totalWork)
{
    if (generation != filterGeneration_.load(std::memory_order_relaxed) || !filtering_) {
        return;
    }
    filterProgressCompleted_ = totalWork == 0 ? completedWork : std::min(completedWork, totalWork);
    filterProgressTotal_ = totalWork;
    statusText_ = filterProgressText();
    updateWidgetHints();
    update();
}

QString TransactionWidget::buildProgressText() const
{
    if (!building_) {
        return statusText_;
    }

    const QString phaseText = buildProgressPhaseName(buildProgressPhase_);
    if (buildProgressTotal_ == 0) {
        return QStringLiteral("Building transaction spans: %1...").arg(phaseText);
    }
    return QStringLiteral("Building transaction spans: %1 %2 / %3")
        .arg(phaseText,
             FormatUnsignedIntegerWithThousands(buildProgressCompleted_),
             FormatUnsignedIntegerWithThousands(buildProgressTotal_));
}

QString TransactionWidget::filterProgressText() const
{
    if (!filtering_ && !filterRestartPending_) {
        return statusText_;
    }

    if (filterRestartPending_) {
        return QStringLiteral("Cancelling previous Transaction filter...");
    }
    if (filterProgressTotal_ == 0) {
        return QStringLiteral("Filtering transactions...");
    }
    return QStringLiteral("Filtering transactions: %1 / %2")
        .arg(FormatUnsignedIntegerWithThousands(filterProgressCompleted_),
             FormatUnsignedIntegerWithThousands(filterProgressTotal_));
}

bool TransactionWidget::hasPendingBuild() const noexcept
{
    return !building_
        && !filtering_
        && !buildRequested_
        && spans_.empty()
        && sourceMode_ == SourceMode::TraceSession
        && traceSession_
        && traceSession_->supportsXactionIndexing()
        && traceSession_->isXactionIndexComplete();
}

QString TransactionWidget::pendingBuildText() const
{
    if (!traceSession_) {
        return QStringLiteral("No transaction trace is loaded.");
    }
    return QStringLiteral("Transaction view is ready to build. Click here to build spans from %1 indexed rows.")
        .arg(FormatUnsignedIntegerWithThousands(traceSession_->totalRows()));
}

QRect TransactionWidget::buildPromptButtonRect() const
{
    const QRect body = bodyRect().adjusted(24, 24, -24, -24);
    const int buttonWidth = qMin(270, qMax(150, body.width() - 48));
    const int buttonHeight = 34;
    return QRect(body.center().x() - buttonWidth / 2,
                 body.center().y() + 20,
                 buttonWidth,
                 buttonHeight);
}

void TransactionWidget::paintBuildPrompt(QPainter& painter) const
{
    if (!hasPendingBuild()) {
        return;
    }

    painter.save();
    painter.fillRect(bodyRect(), QColor(255, 255, 255, 214));

    const QRect content = bodyRect().adjusted(28, 24, -28, -28);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPixelSize(qMax(13, titleFont.pixelSize() > 0 ? titleFont.pixelSize() + 2 : 15));
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#18212B")));
    painter.drawText(content.adjusted(0, 0, 0, -52),
                     Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                     pendingBuildText());

    const QRect button = buildPromptButtonRect();
    painter.setPen(QColor(QStringLiteral("#1D5F8F")));
    painter.setBrush(QColor(QStringLiteral("#EAF4FB")));
    painter.drawRect(button.adjusted(0, 0, -1, -1));
    painter.setPen(QColor(QStringLiteral("#174B72")));
    painter.setFont(font());
    painter.drawText(button, Qt::AlignCenter, QStringLiteral("Build Transaction View"));
    painter.restore();
}

QRect TransactionWidget::buildProgressRect() const
{
    const QRect header = headerRect();
    const QRect arrangeSwitch = arrangeModeSwitchRect();
    const int right = header.right() - 12;
    const int leftLimit = arrangeSwitch.right() + kHeaderControlGap;
    const int availableWidth = right - leftLimit + 1;
    if (availableWidth < kBuildProgressBarMinWidth) {
        return {};
    }

    const int width = std::min(kBuildProgressBarWidth, availableWidth);
    return QRect(right - width + 1,
                 header.top() + (kHeaderPrimaryRowHeight - kBuildProgressBarHeight) / 2,
                 width,
                 kBuildProgressBarHeight);
}

QRect TransactionWidget::arrangeModeSwitchRect() const
{
    const QRect header = headerRect();
    constexpr int kSwitchWidth = 136;
    constexpr int kSwitchHeight = 20;
    return QRect(header.left() + 116,
                 header.top() + (kHeaderPrimaryRowHeight - kSwitchHeight) / 2,
                 kSwitchWidth,
                 kSwitchHeight);
}

QRect TransactionWidget::filterToolbarRect() const
{
    return QRect(0, headerRect().bottom() + 1, width(), kFilterToolbarHeight);
}

void TransactionWidget::updateFilterToolbarGeometry()
{
    if (!filterToolbar_) {
        return;
    }

    const QRect toolbar = filterToolbarRect();
    filterToolbar_->setGeometry(toolbar);
    filterToolbar_->setVisible(toolbar.width() >= 360 && toolbar.height() >= kFilterToolbarControlHeight);

    const int top = (toolbar.height() - kFilterToolbarControlHeight) / 2;
    int x = 10;
    const int maxRight = toolbar.width() - 10;
    const auto setControl = [&](QWidget* widget, const int requestedWidth) {
        if (!widget) {
            return QRect();
        }
        const int widthToUse = std::min(requestedWidth, std::max(0, maxRight - x + 1));
        QRect rect;
        if (filterToolbar_->isVisible() && widthToUse >= 48) {
            rect = QRect(x, top, widthToUse, kFilterToolbarControlHeight);
        }
        widget->setGeometry(rect);
        widget->setVisible(rect.isValid());
        x += requestedWidth + kHeaderFilterGap;
        return rect;
    };

    setControl(filterModeCombo_, kHeaderFilterModeWidth);
    setControl(opcodeFilterEdit_, kHeaderFilterEditWidth);
    setControl(addressFilterEdit_, kHeaderFilterEditWidth);
    setControl(txnIdFilterEdit_, kHeaderFilterSmallEditWidth);
    setControl(dbidFilterEdit_, kHeaderFilterSmallEditWidth);
    setControl(opcodeTagsCheckBox_, kOpcodeTagToggleWidth);
    setControl(hoverCardsCheckBox_, kHoverCardToggleWidth);
    setControl(filterClearButton_, kFilterToolbarClearWidth);
}

QRect TransactionWidget::headerSelectionTextRect() const
{
    const QRect header = headerRect();
    const int left = arrangeModeSwitchRect().right() + kHeaderControlGap;
    int right = header.right() - 10;
    if (filtering_ || (building_ && buildProgressPhase_ != BuildProgressPhase::Idle)) {
        const QRect progress = buildProgressRect();
        if (progress.isValid()) {
            right = progress.left() - kHeaderControlGap;
        }
    }

    if (right - left + 1 < 24) {
        return {};
    }
    return QRect(left, header.top(), right - left + 1, kHeaderPrimaryRowHeight);
}

void TransactionWidget::resetBuildProgress()
{
    buildProgressPhase_ = BuildProgressPhase::Idle;
    buildProgressCompleted_ = 0;
    buildProgressTotal_ = 0;
}

void TransactionWidget::paintBuildProgress(QPainter& painter) const
{
    if (!building_ && !filtering_) {
        return;
    }

    const QRect bar = buildProgressRect();
    if (!bar.isValid() || bar.width() <= 0 || bar.height() <= 0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QColor(QStringLiteral("#B8C0CA")));
    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    painter.drawRect(bar.adjusted(0, 0, -1, -1));

    int fillWidth = 0;
    const std::uint64_t completed = filtering_ ? filterProgressCompleted_ : buildProgressCompleted_;
    const std::uint64_t total = filtering_ ? filterProgressTotal_ : buildProgressTotal_;
    const QColor fillColor = filtering_ ? QColor(QStringLiteral("#7B8FD8")) : QColor(QStringLiteral("#5AB9D6"));
    if (total > 0) {
        fillWidth = static_cast<int>(
            (static_cast<unsigned long long>(std::min(completed, total))
             * static_cast<unsigned long long>(std::max(0, bar.width() - 2)))
            / static_cast<unsigned long long>(total));
    } else {
        const int span = std::max(16, bar.width() / 5);
        const int offset = static_cast<int>((completed / 11U) % static_cast<std::uint64_t>(std::max(1, bar.width() + span)));
        fillWidth = span;
        painter.setClipRect(bar.adjusted(1, 1, -1, -1));
        painter.fillRect(QRect(bar.left() + 1 + offset - span,
                               bar.top() + 1,
                               fillWidth,
                               std::max(0, bar.height() - 2)),
                         fillColor);
        painter.restore();
        return;
    }

    if (fillWidth > 0) {
        painter.fillRect(QRect(bar.left() + 1,
                               bar.top() + 1,
                               fillWidth,
                               std::max(0, bar.height() - 2)),
                         fillColor);
    }
    painter.restore();
}

void TransactionWidget::clearSpanData()
{
    hideHoverCard();
    invalidateDenseSpanPaintCache();
    unfilteredLanes_.clear();
    unfilteredSpans_.clear();
    lanes_.clear();
    spans_.clear();
    spanIndicesByLane_.clear();
    spanIndicesByLaneEnd_.clear();
    unfilteredSpanFilterMatches_.clear();
    spanFilterMatches_.clear();
    filterMatchCount_ = 0;
    displayingFilteredSpans_ = false;
    ++filterRevision_;
    processedXactions_ = 0;
    totalXactions_ = 0;
    fullTimestampStart_ = 0;
    fullTimestampEnd_ = 1;
    resetBuildProgress();
    resetFilterProgress();
    selectedSpanIndex_ = -1;
    clearSelectedOpcodeTag();
    hoveredSpanIndex_ = -1;
    markerSpanIndex_ = -1;
    dragMode_ = DragMode::None;
    leftDragGestureActive_ = false;
    opcodeTagPressPending_ = false;
}

void TransactionWidget::rebuildSpanPaintIndex()
{
    invalidateDenseSpanPaintCache();
    rebuildSpanPaintIndex(spans_,
                          spanIndicesByLane_,
                          spanIndicesByLaneEnd_,
                          fullTimestampStart_,
                          fullTimestampEnd_,
                          lanes_.size());
    if (spanFilterMatches_.size() != spans_.size()) {
        spanFilterMatches_.assign(spans_.size(), 1);
    }
}

void TransactionWidget::rebuildSpanPaintIndex(std::vector<TransactionSpan>& spans,
                                              std::vector<std::vector<int>>& spanIndicesByLane,
                                              std::vector<std::vector<int>>& spanIndicesByLaneEnd,
                                              qint64& fullTimestampStart,
                                              qint64& fullTimestampEnd,
                                              const std::size_t laneCount)
{
    spanIndicesByLane.clear();
    spanIndicesByLane.resize(laneCount);
    spanIndicesByLaneEnd.clear();
    spanIndicesByLaneEnd.resize(laneCount);
    if (spans.empty()) {
        fullTimestampStart = 0;
        fullTimestampEnd = 1;
        return;
    }

    fullTimestampStart = spans.front().startTimestamp;
    fullTimestampEnd = spans.front().endTimestamp;
    for (int spanIndex = 0; spanIndex < static_cast<int>(spans.size()); ++spanIndex) {
        TransactionSpan& span = spans[static_cast<std::size_t>(spanIndex)];
        fullTimestampStart = std::min(fullTimestampStart, span.startTimestamp);
        fullTimestampEnd = std::max(fullTimestampEnd, span.endTimestamp);
        if (span.laneIndex >= 0 && span.laneIndex < static_cast<int>(spanIndicesByLane.size())) {
            span.paintBucket = static_cast<std::uint8_t>(denseSpanBucketForChannelKind(
                channelKindFromOpcodeOrOrigin(span.requestOpcode, span.originKind)));
            spanIndicesByLane[static_cast<std::size_t>(span.laneIndex)].push_back(spanIndex);
            spanIndicesByLaneEnd[static_cast<std::size_t>(span.laneIndex)].push_back(spanIndex);
        }
    }
    if (fullTimestampEnd <= fullTimestampStart) {
        fullTimestampEnd = fullTimestampStart + 1;
    }

    for (std::vector<int>& laneSpanIndices : spanIndicesByLane) {
        std::sort(laneSpanIndices.begin(), laneSpanIndices.end(), [&spans](const int lhsIndex, const int rhsIndex) {
            const TransactionSpan& lhs = spans[static_cast<std::size_t>(lhsIndex)];
            const TransactionSpan& rhs = spans[static_cast<std::size_t>(rhsIndex)];
            if (lhs.startTimestamp != rhs.startTimestamp) {
                return lhs.startTimestamp < rhs.startTimestamp;
            }
            if (lhs.endTimestamp != rhs.endTimestamp) {
                return lhs.endTimestamp < rhs.endTimestamp;
            }
            return lhs.ordinal < rhs.ordinal;
        });
    }
    for (std::vector<int>& laneSpanIndices : spanIndicesByLaneEnd) {
        std::sort(laneSpanIndices.begin(), laneSpanIndices.end(), [&spans](const int lhsIndex, const int rhsIndex) {
            const TransactionSpan& lhs = spans[static_cast<std::size_t>(lhsIndex)];
            const TransactionSpan& rhs = spans[static_cast<std::size_t>(rhsIndex)];
            if (lhs.endTimestamp != rhs.endTimestamp) {
                return lhs.endTimestamp < rhs.endTimestamp;
            }
            if (lhs.startTimestamp != rhs.startTimestamp) {
                return lhs.startTimestamp < rhs.startTimestamp;
            }
            return lhs.ordinal < rhs.ordinal;
        });
    }
}

TransactionWidget::SpanPaintIndex TransactionWidget::buildSpanPaintIndex(std::vector<TransactionSpan>& spans,
                                                                         const std::size_t laneCount)
{
    SpanPaintIndex index;
    rebuildSpanPaintIndex(spans,
                          index.spanIndicesByLane,
                          index.spanIndicesByLaneEnd,
                          index.fullTimestampStart,
                          index.fullTimestampEnd,
                          laneCount);
    return index;
}

int TransactionWidget::scrollBarExtent() const
{
    return kScrollBarExtent;
}

QRect TransactionWidget::headerRect() const
{
    return QRect(0, 0, width(), kHeaderHeight);
}

int TransactionWidget::contentTop() const
{
    return filterToolbarRect().bottom() + 1;
}

QRect TransactionWidget::rulerRect() const
{
    const int extent = scrollBarExtent();
    const int top = contentTop();
    return QRect(0,
                 std::max(top, height() - kRulerHeight - kInfoBarHeight - extent),
                 std::max(0, width() - extent),
                 kRulerHeight);
}

QRect TransactionWidget::infoBarRect() const
{
    const QRect ruler = rulerRect();
    return QRect(0,
                 ruler.bottom() + 1,
                 std::max(0, width() - scrollBarExtent()),
                 kInfoBarHeight);
}

QRect TransactionWidget::horizontalScrollBarRect() const
{
    const int extent = scrollBarExtent();
    const int top = contentTop();
    return QRect(laneHeaderWidth(),
                 std::max(top, height() - extent),
                 std::max(0, width() - laneHeaderWidth() - extent),
                 extent);
}

QRect TransactionWidget::verticalScrollBarRect() const
{
    const int extent = scrollBarExtent();
    const int top = contentTop();
    return QRect(std::max(0, width() - extent),
                 top,
                 extent,
                 std::max(0, height() - top - extent));
}

QRect TransactionWidget::bodyRect() const
{
    const int extent = scrollBarExtent();
    const int top = contentTop();
    const int bottom = std::max(top, height() - kRulerHeight - kInfoBarHeight - extent);
    return QRect(0, top, std::max(0, width() - extent), std::max(0, bottom - top));
}

int TransactionWidget::laneHeaderWidth() const
{
    return std::clamp(width() / 4, kLaneHeaderMinWidth, kLaneHeaderMaxWidth);
}

QRect TransactionWidget::laneHeaderRect() const
{
    const QRect body = bodyRect();
    return QRect(body.left(), body.top(), std::min(laneHeaderWidth(), body.width()), body.height());
}

QRect TransactionWidget::plotRect() const
{
    const QRect body = bodyRect();
    const int labelWidth = std::min(laneHeaderWidth(), body.width());
    return QRect(body.left() + labelWidth,
                 body.top(),
                 std::max(0, body.width() - labelWidth),
                 body.height());
}

int TransactionWidget::rowHeight() const noexcept
{
    return std::clamp(rowHeight_, kLaneRowMinHeight, kLaneRowMaxHeight);
}

int TransactionWidget::laneHeaderTitleHeight() const noexcept
{
    return std::max(kLaneHeaderTitleHeight, rowHeight());
}

int TransactionWidget::laneHeaderBadgeHeight() const noexcept
{
    return std::clamp(laneHeaderTitleHeight() - 16, kLaneHeaderBadgeMinHeight, kLaneHeaderBadgeMaxHeight);
}

QFont TransactionWidget::rulerFont() const
{
    return font();
}

QFont TransactionWidget::infoBarFont() const
{
    return font();
}

void TransactionWidget::setRowHeight(const int rowHeight)
{
    const int clampedRowHeight = std::clamp(rowHeight, kLaneRowMinHeight, kLaneRowMaxHeight);
    if (rowHeight_ == clampedRowHeight) {
        return;
    }

    hideHoverCard();
    rowHeight_ = clampedRowHeight;
    verticalScrollOffset_ =
        std::clamp(verticalScrollOffset_, 0, std::max(0, stackRowCount() - visibleLaneCapacity()));
    updateScrollBars();
    update();
}

void TransactionWidget::adjustRowHeightFromWheel(const int wheelDelta)
{
    if (wheelDelta == 0) {
        return;
    }
    const int direction = wheelDelta > 0 ? 1 : -1;
    setRowHeight(rowHeight() + direction * kLaneRowHeightWheelStep);
}

int TransactionWidget::visibleLaneCapacity() const
{
    const QRect plot = plotRect();
    return plot.height() <= 0 ? 0 : std::max(1, plot.height() / rowHeight());
}

int TransactionWidget::stackRowCount() const noexcept
{
    return static_cast<int>(lanes_.size());
}

int TransactionWidget::transactionGroupCount() const noexcept
{
    int count = 0;
    int previousGroup = -1;
    for (const TransactionLane& lane : lanes_) {
        if (lane.groupIndex != previousGroup) {
            ++count;
            previousGroup = lane.groupIndex;
        }
    }
    return count;
}

int TransactionWidget::maxStackDepth() const noexcept
{
    int depth = 0;
    for (const TransactionLane& lane : lanes_) {
        depth = std::max(depth, lane.stackDepth);
    }
    return depth;
}

std::pair<qint64, qint64> TransactionWidget::fullTimestampRange() const
{
    return {fullTimestampStart_, fullTimestampEnd_};
}

qint64 TransactionWidget::visibleStartTimestamp() const
{
    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const long double maxOffset = std::max<long double>(0.0L, fullRange - visibleRange);
    const long double clampedOffset =
        std::min<long double>(static_cast<long double>(scrollOffset_), maxOffset);
    return clampedRoundToTimestamp(static_cast<long double>(fullStart) + clampedOffset);
}

qint64 TransactionWidget::visibleEndTimestamp() const
{
    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const qint64 start = visibleStartTimestamp();
    return std::min(fullEnd, clampedRoundToTimestamp(static_cast<long double>(start) + visibleRange));
}

double TransactionWidget::timestampToPlotX(const qint64 timestamp, const QRect& plot) const
{
    if (plot.width() <= 0) {
        return static_cast<double>(plot.left());
    }
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double range =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const long double normalized =
        (static_cast<long double>(timestamp) - static_cast<long double>(visibleStart)) / range;
    return static_cast<double>(static_cast<long double>(plot.left()) + normalized * static_cast<long double>(plot.width()));
}

qint64 TransactionWidget::plotXToTimestamp(const int x, const QRect& plot) const
{
    if (plot.width() <= 0) {
        return visibleStartTimestamp();
    }
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double range =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const long double normalized =
        static_cast<long double>(std::clamp(x, plot.left(), plot.right()) - plot.left())
        / static_cast<long double>(std::max(1, plot.width()));
    return clampedRoundToTimestamp(static_cast<long double>(visibleStart) + normalized * range);
}

int TransactionWidget::laneTopY(const int laneIndex, const QRect& plot) const
{
    return plot.top() + (laneIndex - verticalScrollOffset_) * rowHeight();
}

int TransactionWidget::laneIndexAtY(const int y, const QRect& plot) const
{
    if (!plot.contains(QPoint(plot.left(), y))) {
        return -1;
    }
    const int laneIndex = verticalScrollOffset_ + (y - plot.top()) / rowHeight();
    return laneIndex >= 0 && laneIndex < static_cast<int>(lanes_.size()) ? laneIndex : -1;
}

QRect TransactionWidget::laneHeaderTitleRect(const int laneIndex,
                                             const QRect& laneHeader,
                                             const QRect& plot) const
{
    if (!isFirstClassificationRow(laneIndex)) {
        return {};
    }
    return QRect(laneHeader.left(),
                 laneTopY(laneIndex, plot),
                 laneHeader.width(),
                 laneHeaderTitleHeight());
}

QRect TransactionWidget::spanVisualRect(const int spanIndex, const QRect& plot) const
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size()) || plot.width() <= 0) {
        return {};
    }
    if (shouldDrawFilteredOutSpan(spanIndex)) {
        return {};
    }
    const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    return spanVisualRectForRange(span, plot, visibleStart, visibleEnd, visibleRange);
}

QRect TransactionWidget::spanVisualRectForRange(const TransactionSpan& span,
                                                const QRect& plot,
                                                const qint64 visibleStart,
                                                const qint64 visibleEnd,
                                                const long double visibleRange,
                                                const bool requireVisibleLane) const
{
    if (plot.width() <= 0) {
        return {};
    }
    if (span.laneIndex < 0) {
        return {};
    }
    if (requireVisibleLane) {
        if (span.laneIndex < verticalScrollOffset_
            || span.laneIndex >= verticalScrollOffset_ + visibleLaneCapacity()) {
            return {};
        }
    }
    if (span.endTimestamp < visibleStart || span.startTimestamp > visibleEnd) {
        return {};
    }

    const int rowTop = laneTopY(span.laneIndex, plot);
    const auto toPlotX = [plot, visibleStart, visibleRange](const qint64 timestamp) {
        const long double normalized =
            (static_cast<long double>(timestamp) - static_cast<long double>(visibleStart)) / visibleRange;
        return static_cast<double>(static_cast<long double>(plot.left())
                                   + normalized * static_cast<long double>(plot.width()));
    };
    const int x0 = static_cast<int>(std::floor(toPlotX(span.startTimestamp)));
    const int x1 = static_cast<int>(std::ceil(toPlotX(span.endTimestamp)));
    const int left = std::clamp(std::min(x0, x1), plot.left(), plot.right());
    const int right = std::clamp(std::max(x0, x1), plot.left(), plot.right());
    return QRect(left, rowTop, std::max(5, right - left + 1), rowHeight());
}

bool TransactionWidget::isFirstClassificationRow(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return false;
    }
    return laneIndex == 0
        || lanes_[static_cast<std::size_t>(laneIndex)].groupIndex
               != lanes_[static_cast<std::size_t>(laneIndex - 1)].groupIndex;
}

QRect TransactionWidget::laneNodeBadgeRect(const int laneIndex,
                                           const QRect& laneHeader,
                                           const QRect& plot) const
{
    if (!isFirstClassificationRow(laneIndex)) {
        return {};
    }
    const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    const int y = laneTopY(laneIndex, plot);
    const int badgeHeight = laneHeaderBadgeHeight();
    QFont badgeFont = transactionLaneHeaderTitleFont(font());
    const QFontMetrics badgeMetrics(badgeFont);
    const int width = std::min(ChannelBadgeMainWidth(badgeMetrics, lane.endpointNodeType),
                               std::max(0, laneHeader.width() / 3));
    if (width <= 0) {
        return {};
    }
    return QRect(laneHeader.left() + 8,
                 y + (laneHeaderTitleHeight() - badgeHeight) / 2,
                 width,
                 badgeHeight);
}

QRect TransactionWidget::laneNodeIdRect(const int laneIndex,
                                        const QRect& laneHeader,
                                        const QRect& plot) const
{
    if (!isFirstClassificationRow(laneIndex)) {
        return {};
    }
    const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    const QRect nodeBadge = laneNodeBadgeRect(laneIndex, laneHeader, plot);
    if (!nodeBadge.isValid()) {
        return {};
    }
    const QString nodeId = lane.endpointNode.trimmed().isEmpty()
        ? QStringLiteral("?")
        : lane.endpointNode.trimmed();
    QFont idFont = transactionLaneHeaderTitleFont(font(), true);
    const QFontMetrics idMetrics(idFont);
    const int left = nodeBadge.right() + 1 + kLaneHeaderNodeIdGap;
    const int maxRight = laneHeader.right() - 72;
    const int width = std::min(idMetrics.horizontalAdvance(nodeId) + 4,
                               std::max(0, maxRight - left + 1));
    if (width <= 0) {
        return {};
    }
    return QRect(left,
                 nodeBadge.top(),
                 width,
                 nodeBadge.height());
}

QRect TransactionWidget::laneChannelBadgeRect(const int laneIndex,
                                              const QRect& laneHeader,
                                              const QRect& plot) const
{
    if (!isFirstClassificationRow(laneIndex)) {
        return {};
    }
    const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    const QRect nodeIdRect = laneNodeIdRect(laneIndex, laneHeader, plot);
    const int badgeHeight = laneHeaderBadgeHeight();
    QFont badgeFont = transactionLaneHeaderTitleFont(font());
    const QFontMetrics badgeMetrics(badgeFont);
    const QString channelKind = channelKindFromOpcodeOrOrigin(lane.requestOpcode, lane.originKind);
    const int channelLeft = nodeIdRect.isValid() ? nodeIdRect.right() + 1 + kLaneHeaderBadgeGap
                                                 : laneHeader.left() + 8;
    const int maxRight = laneHeader.right() - 72;
    const int width = std::min(ChannelBadgeMainWidth(badgeMetrics, channelKind),
                               std::max(0, maxRight - channelLeft + 1));
    if (width <= 0) {
        return {};
    }
    return QRect(channelLeft,
                 nodeIdRect.isValid() ? nodeIdRect.top() : laneTopY(laneIndex, plot) + (laneHeaderTitleHeight() - badgeHeight) / 2,
                 width,
                 badgeHeight);
}

QRect TransactionWidget::laneHeaderCountRect(const int laneIndex,
                                             const QRect& laneHeader,
                                             const QRect& plot) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return {};
    }
    const int y = laneTopY(laneIndex, plot);
    return QRect(laneHeader.right() - 64,
                 y,
                 56,
                 isFirstClassificationRow(laneIndex) ? laneHeaderTitleHeight() : rowHeight());
}

QFont TransactionWidget::laneHeaderCountFont(const int laneIndex) const
{
    return isFirstClassificationRow(laneIndex)
        ? transactionLaneHeaderTitleFont(font())
        : transactionLaneHeaderFont(font(), rowHeight());
}

QString TransactionWidget::laneHeaderCountText(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return {};
    }
    const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    if (arrangeMode_ == ArrangeMode::TxnId) {
        return compactLaneCount(lane.spanCount);
    }
    return isFirstClassificationRow(laneIndex)
        ? compactLaneCount(lane.groupSpanCount)
        : QStringLiteral("#%1").arg(lane.stackSlot + 1);
}

int TransactionWidget::visibleSpanCount(const qint64 visibleStart,
                                        const qint64 visibleEnd,
                                        const int maxCount) const
{
    int count = 0;
    const int firstLane = std::max(0, verticalScrollOffset_);
    const int lastLane = std::min(static_cast<int>(spanIndicesByLane_.size()),
                                  firstLane + visibleLaneCapacity() + 1);
    for (int laneIndex = firstLane; laneIndex < lastLane; ++laneIndex) {
        const std::vector<int>& laneSpanIndices = spanIndicesByLane_[static_cast<std::size_t>(laneIndex)];
        const auto firstVisible = std::upper_bound(laneSpanIndices.begin(),
                                                   laneSpanIndices.end(),
                                                   visibleEnd,
                                                   [this](const qint64 timestamp, const int spanIndex) {
                                                       return timestamp < spans_[static_cast<std::size_t>(spanIndex)].startTimestamp;
                                                   });
        const std::size_t startCandidateCount =
            static_cast<std::size_t>(std::distance(laneSpanIndices.begin(), firstVisible));
        const std::vector<int>& laneEndSpanIndices =
            laneIndex < static_cast<int>(spanIndicesByLaneEnd_.size())
                ? spanIndicesByLaneEnd_[static_cast<std::size_t>(laneIndex)]
                : laneSpanIndices;
        const auto firstNotEnded = std::lower_bound(laneEndSpanIndices.begin(),
                                                    laneEndSpanIndices.end(),
                                                    visibleStart,
                                                    [this](const int spanIndex, const qint64 timestamp) {
                                                        return spans_[static_cast<std::size_t>(spanIndex)].endTimestamp < timestamp;
                                                    });
        const std::size_t endCandidateCount =
            static_cast<std::size_t>(std::distance(firstNotEnded, laneEndSpanIndices.end()));

        if (endCandidateCount < startCandidateCount) {
            for (auto it = firstNotEnded; it != laneEndSpanIndices.end(); ++it) {
                const TransactionSpan& span = spans_[static_cast<std::size_t>(*it)];
                if (span.startTimestamp > visibleEnd) {
                    continue;
                }
                ++count;
                if (maxCount > 0 && count >= maxCount) {
                    return count;
                }
            }
            continue;
        }

        for (auto it = laneSpanIndices.begin(); it != firstVisible; ++it) {
            const TransactionSpan& span = spans_[static_cast<std::size_t>(*it)];
            if (span.endTimestamp < visibleStart) {
                continue;
            }
            ++count;
            if (maxCount > 0 && count >= maxCount) {
                return count;
            }
        }
    }
    return count;
}

std::vector<int> TransactionWidget::visibleSpanIndices(const qint64 visibleStart, const qint64 visibleEnd) const
{
    std::vector<int> indices;
    const int firstLane = std::max(0, verticalScrollOffset_);
    const int lastLane = std::min(static_cast<int>(spanIndicesByLane_.size()),
                                  firstLane + visibleLaneCapacity() + 1);
    for (int laneIndex = firstLane; laneIndex < lastLane; ++laneIndex) {
        std::vector<int> laneIndices = visibleSpanIndicesForLane(laneIndex, visibleStart, visibleEnd);
        indices.insert(indices.end(), laneIndices.begin(), laneIndices.end());
    }
    return indices;
}

std::vector<int> TransactionWidget::visibleSpanIndicesForLane(const int laneIndex,
                                                              const qint64 visibleStart,
                                                              const qint64 visibleEnd) const
{
    std::vector<int> indices;
    if (laneIndex < 0 || laneIndex >= static_cast<int>(spanIndicesByLane_.size())) {
        return indices;
    }

    const std::vector<int>& laneSpanIndices = spanIndicesByLane_[static_cast<std::size_t>(laneIndex)];
    const auto firstAfterVisibleEnd = std::upper_bound(laneSpanIndices.begin(),
                                                       laneSpanIndices.end(),
                                                       visibleEnd,
                                                       [this](const qint64 timestamp, const int spanIndex) {
                                                           return timestamp < spans_[static_cast<std::size_t>(spanIndex)].startTimestamp;
                                                       });
    const std::size_t startCandidateCount =
        static_cast<std::size_t>(std::distance(laneSpanIndices.begin(), firstAfterVisibleEnd));
    const std::vector<int>& laneEndSpanIndices =
        laneIndex < static_cast<int>(spanIndicesByLaneEnd_.size())
            ? spanIndicesByLaneEnd_[static_cast<std::size_t>(laneIndex)]
            : laneSpanIndices;
    const auto firstNotEnded = std::lower_bound(laneEndSpanIndices.begin(),
                                                laneEndSpanIndices.end(),
                                                visibleStart,
                                                [this](const int spanIndex, const qint64 timestamp) {
                                                    return spans_[static_cast<std::size_t>(spanIndex)].endTimestamp < timestamp;
                                                });
    const std::size_t endCandidateCount =
        static_cast<std::size_t>(std::distance(firstNotEnded, laneEndSpanIndices.end()));

    if (endCandidateCount < startCandidateCount) {
        indices.reserve(endCandidateCount);
        for (auto it = firstNotEnded; it != laneEndSpanIndices.end(); ++it) {
            const TransactionSpan& span = spans_[static_cast<std::size_t>(*it)];
            if (span.startTimestamp <= visibleEnd) {
                indices.push_back(*it);
            }
        }
        return indices;
    }

    indices.reserve(startCandidateCount);
    for (auto it = laneSpanIndices.begin(); it != firstAfterVisibleEnd; ++it) {
        const TransactionSpan& span = spans_[static_cast<std::size_t>(*it)];
        if (span.endTimestamp < visibleStart) {
            continue;
        }
        indices.push_back(*it);
    }
    return indices;
}

QFont TransactionWidget::opcodeTagFont() const
{
    QFont tagFont(font());
    tagFont.setFamily(QStringLiteral("Consolas"));
    tagFont.setStyleHint(QFont::TypeWriter);
    tagFont.setPixelSize(rowScaledPixelSize(rowHeight(), kSpanLabelMinPixelSize, kSpanLabelMaxPixelSize + 2));
    tagFont.setBold(true);
    return tagFont;
}

std::optional<TransactionWidget::OpcodeTagInfo> TransactionWidget::opcodeTagInfoForLogicalRow(
    const TransactionSpan& span,
    const std::uint64_t logicalRow) const
{
    OpcodeTagInfo info;
    info.logicalRow = logicalRow;
    info.timestamp = timestampForLogicalRowInSpan(span, logicalRow);

    if (logicalRow < rows_.size()) {
        const FlitRecord& record = rows_[static_cast<std::size_t>(logicalRow)];
        info.timestamp = record.timestamp;
        info.channel = record.channel;
        info.opcode = record.opcode.trimmed().isEmpty() ? record.opcodeRaw.trimmed() : record.opcode.trimmed();
        if (info.opcode.isEmpty()) {
            info.opcode = QStringLiteral("Unknown");
        }
        return info;
    }

    if (traceSession_) {
        if (const FlitRecord* record = traceSession_->tryGetRow(logicalRow)) {
            info.timestamp = record->timestamp;
            info.channel = record->channel;
            info.opcode = record->opcode.trimmed().isEmpty() ? record->opcodeRaw.trimmed() : record->opcode.trimmed();
            if (info.opcode.isEmpty()) {
                info.opcode = QStringLiteral("Unknown");
            }
            return info;
        }
    }

    if (traceSession_) {
        const auto& blocks = traceSession_->metadata().blocks;
        auto foundBlock = std::upper_bound(
            blocks.begin(),
            blocks.end(),
            logicalRow,
            [](const std::uint64_t rowValue, const CLogBTraceBlockSummary& block) {
                return rowValue < block.rowStart;
            });
        if (foundBlock != blocks.begin()) {
            --foundBlock;
            const CLogBTraceBlockSummary& block = *foundBlock;
            if (logicalRow >= block.rowStart
                && logicalRow < block.rowStart + block.recordCount) {
                std::vector<CLogBTraceFastRecordInfo> fastRecords;
                const std::size_t blockIndex = static_cast<std::size_t>(
                    std::distance(blocks.begin(), foundBlock));
                const std::size_t localBegin = static_cast<std::size_t>(logicalRow - block.rowStart);
                if (traceSession_->readFastRecordsFromIndex(blockIndex, localBegin, 1, fastRecords)
                    && !fastRecords.empty()
                    && validTransportChannel(fastRecords.front().channel)) {
                    std::unordered_map<std::uint32_t, QString> opcodeCache;
                    const BuildRowSummary summary =
                        summaryFromFastRecord(traceSession_, logicalRow, fastRecords.front(), opcodeCache);
                    info.timestamp = summary.timestamp;
                    info.channel = summary.channel;
                    info.opcode = displayOpcode(summary);
                    if (info.opcode.trimmed().isEmpty()) {
                        info.opcode = QStringLiteral("Unknown");
                    }
                    return info;
                }
            }
        }
    }

    if (spanHasCachedLogicalRow(span, logicalRow)) {
        const QString channelKind = channelKindFromOpcodeOrOrigin(span.requestOpcode, span.originKind);
        if (channelKind == QLatin1String("SNP")) {
            info.channel = FlitChannel::Snp;
        } else if (channelKind == QLatin1String("RSP")) {
            info.channel = FlitChannel::Rsp;
        } else if (channelKind == QLatin1String("DAT")) {
            info.channel = FlitChannel::Dat;
        } else {
            info.channel = FlitChannel::Req;
        }
        info.opcode = logicalRow == span.requestLogicalRow
            ? opcodeWithoutChannelPrefix(span.requestOpcode)
            : (span.completionLogicalRow && logicalRow == *span.completionLogicalRow
                   ? QStringLiteral("Completion")
                   : QStringLiteral("Related flit"));
        if (info.opcode.trimmed().isEmpty()) {
            info.opcode = QStringLiteral("Unknown");
        }
        return info;
    }

    return std::nullopt;
}

std::vector<TransactionWidget::OpcodeTagInfo> TransactionWidget::opcodeTagInfosForSpan(
    const TransactionSpan& span) const
{
    std::vector<std::uint64_t> logicalRows = relatedLogicalRowsForSpan(span);
    std::sort(logicalRows.begin(), logicalRows.end());
    logicalRows.erase(std::unique(logicalRows.begin(), logicalRows.end()), logicalRows.end());

    std::vector<OpcodeTagInfo> infos;
    infos.reserve(logicalRows.size());
    for (const std::uint64_t logicalRow : logicalRows) {
        if (logicalRow < span.requestLogicalRow) {
            continue;
        }
        std::optional<OpcodeTagInfo> info = opcodeTagInfoForLogicalRow(span, logicalRow);
        if (!info) {
            continue;
        }
        if (logicalRow == span.requestLogicalRow
            && info->channel != FlitChannel::Req
            && info->channel != FlitChannel::Snp) {
            const QString originKind = channelKindFromOpcodeOrOrigin(span.requestOpcode, span.originKind);
            if (originKind != QLatin1String("REQ") && originKind != QLatin1String("SNP")) {
                continue;
            }
        }
        info->opcode = opcodeWithoutChannelPrefix(info->opcode).trimmed();
        if (info->opcode.isEmpty()) {
            info->opcode = QStringLiteral("Unknown");
        }
        infos.push_back(std::move(*info));
    }

    std::sort(infos.begin(), infos.end(), [](const OpcodeTagInfo& lhs, const OpcodeTagInfo& rhs) {
        if (lhs.timestamp != rhs.timestamp) {
            return lhs.timestamp < rhs.timestamp;
        }
        return lhs.logicalRow < rhs.logicalRow;
    });
    return infos;
}

QRect TransactionWidget::spanInlineTextBounds(const TransactionSpan& span,
                                              const QRect& spanRect,
                                              const QFontMetrics& opcodeMetrics,
                                              const QFontMetrics& addressMetrics) const
{
    constexpr int kHorizontalPadding = 3;
    if (spanRect.width() <= kHorizontalPadding * 2) {
        return {};
    }

    const int availableWidth = spanRect.width() - kHorizontalPadding * 2;
    const SpanInlineLabelParts labelParts = spanInlineLabelParts(span,
                                                                 availableWidth,
                                                                 opcodeMetrics,
                                                                 addressMetrics);
    if (labelParts.opcode.isEmpty()) {
        return {};
    }

    int textWidth = opcodeMetrics.horizontalAdvance(labelParts.opcode);
    if (!labelParts.address.isEmpty()) {
        constexpr int kOpcodeAddressGap = 8;
        textWidth += kOpcodeAddressGap + addressMetrics.horizontalAdvance(labelParts.address);
    }
    if (textWidth <= 0) {
        return {};
    }

    const int textHeight = std::max(opcodeMetrics.height(), addressMetrics.height());
    const int textTop = spanRect.top() + std::max(0, (spanRect.height() - textHeight) / 2);
    return QRect(spanRect.left() + kHorizontalPadding,
                 textTop,
                 std::min(textWidth, availableWidth),
                 std::min(spanRect.height(), textHeight));
}

std::vector<TransactionWidget::OpcodeTagLayout> TransactionWidget::selectedOpcodeTagLayouts(
    const QRect& plot,
    const qint64 visibleStart,
    const qint64 visibleEnd,
    const long double visibleRange,
    const bool inlineLabelsDrawn) const
{
    std::vector<OpcodeTagLayout> layouts;
    if (!showOpcodeTags_
        || selectedSpanIndex_ < 0
        || selectedSpanIndex_ >= static_cast<int>(spans_.size())
        || plot.width() <= 0
        || plot.height() <= 0) {
        return layouts;
    }

    const TransactionSpan& span = spans_[static_cast<std::size_t>(selectedSpanIndex_)];
    if (shouldDrawFilteredOutSpan(selectedSpanIndex_)) {
        return layouts;
    }

    const QRect barRect = spanVisualRectForRange(span,
                                                 plot,
                                                 visibleStart,
                                                 visibleEnd,
                                                 visibleRange,
                                                 true);
    if (!barRect.isValid()) {
        return layouts;
    }

    const int verticalInset = rowHeight() >= 18 ? 2 : (rowHeight() >= 14 ? 1 : 0);
    const QRect spanRect = barRect.adjusted(0, verticalInset, -1, -verticalInset);
    if (!spanRect.isValid()) {
        return layouts;
    }

    const QFont tagFont = opcodeTagFont();
    const QFontMetrics tagMetrics(tagFont);
    QFont spanAddressFont = transactionSpanLabelFont(font(), rowHeight());
    spanAddressFont.setBold(false);
    QFont spanOpcodeFont = spanAddressFont;
    spanOpcodeFont.setBold(true);
    const QRect inlineTextRect = inlineLabelsDrawn
        ? spanInlineTextBounds(span, spanRect, QFontMetrics(spanOpcodeFont), QFontMetrics(spanAddressFont))
        : QRect();

    const int tagHeight = std::max(tagMetrics.height() + kOpcodeTagVerticalPadding * 2,
                                   std::min(18, std::max(12, rowHeight() - 2)));
    const bool onTopVisibleRow = span.laneIndex == verticalScrollOffset_;
    const bool topFits = !onTopVisibleRow && spanRect.top() - tagHeight >= plot.top();
    const bool bottomFits = spanRect.bottom() + tagHeight <= plot.bottom();
    const bool primaryBottomPlacement = onTopVisibleRow || !topFits;
    if (!topFits && !bottomFits) {
        return layouts;
    }

    const std::vector<OpcodeTagInfo> infos = opcodeTagInfosForSpan(span);
    layouts.reserve(infos.size());
    const auto updateConnectorGeometry = [&](OpcodeTagLayout& layout) {
        const int lineTop = spanRect.top();
        const int lineBottom = spanRect.bottom();
        const QRect lineHitRect(layout.timestampX - kOpcodeTagTimestampLineHitWidth / 2,
                                lineTop,
                                kOpcodeTagTimestampLineHitWidth,
                                std::max(1, lineBottom - lineTop + 1));
        layout.lineRect = QRect(layout.timestampX, lineTop, 1, std::max(1, lineBottom - lineTop + 1));
        layout.lineVisible = !inlineTextRect.isValid() || !lineHitRect.intersects(inlineTextRect);

        const int maxArrowHeight = std::clamp(rowHeight() / 5, 3, 7);
        const int maxArrowHalfWidth = std::max(2, std::min(maxArrowHeight, rowHeight() / 6));
        const int baseY = layout.bottomPlacement ? layout.tagRect.top() : layout.tagRect.bottom();
        layout.triangleVisible = false;
        for (int arrowHeight = maxArrowHeight; arrowHeight >= 2 && !layout.triangleVisible; --arrowHeight) {
            const int maxHalfWidth = std::min(maxArrowHalfWidth, std::max(1, arrowHeight));
            for (int arrowHalfWidth = maxHalfWidth; arrowHalfWidth >= 1; --arrowHalfWidth) {
                const int tipY = layout.bottomPlacement
                    ? std::max(spanRect.top(), baseY - arrowHeight)
                    : std::min(spanRect.bottom(), baseY + arrowHeight);
                std::array<QPoint, 3> points = {
                    QPoint(std::clamp(layout.timestampX - arrowHalfWidth, layout.tagRect.left(), layout.tagRect.right()), baseY),
                    QPoint(std::clamp(layout.timestampX + arrowHalfWidth, layout.tagRect.left(), layout.tagRect.right()), baseY),
                    QPoint(layout.timestampX, tipY),
                };
                QRect bounds = QRect(points[0], points[1]).normalized()
                                   .united(QRect(points[2], QSize(1, 1)));
                if (inlineTextRect.isValid()
                    && bounds.adjusted(-1, -1, 1, 1).intersects(inlineTextRect)) {
                    continue;
                }
                layout.trianglePoints = points;
                layout.triangleBounds = bounds;
                layout.triangleVisible = true;
                break;
            }
        }
    };

    const auto tagTopForPlacement = [&](const bool bottomPlacement) {
        return bottomPlacement ? spanRect.bottom() + 1 : spanRect.top() - tagHeight;
    };
    const auto laneAvailable = [&](const bool bottomPlacement) {
        return bottomPlacement ? bottomFits : topFits;
    };
    const auto subtractInterval = [](std::vector<std::pair<int, int>>& intervals,
                                     const int removeStart,
                                     const int removeEnd) {
        if (removeStart > removeEnd) {
            return;
        }
        std::vector<std::pair<int, int>> next;
        next.reserve(intervals.size() + 1);
        for (const auto& interval : intervals) {
            if (removeEnd < interval.first || removeStart > interval.second) {
                next.push_back(interval);
                continue;
            }
            if (removeStart > interval.first) {
                next.emplace_back(interval.first, removeStart - 1);
            }
            if (removeEnd < interval.second) {
                next.emplace_back(removeEnd + 1, interval.second);
            }
        }
        intervals = std::move(next);
    };
    const auto placementOrder = [&](const bool preferredBottom) {
        std::array<bool, 2> order = {preferredBottom, !preferredBottom};
        if (!laneAvailable(order[0])) {
            std::swap(order[0], order[1]);
        }
        return order;
    };
    struct PendingOpcodeTag {
        OpcodeTagInfo info;
        QString label;
        int timestampX = 0;
        int fullWidth = 0;
    };
    std::vector<PendingOpcodeTag> pendingTags;
    pendingTags.reserve(infos.size());
    for (const OpcodeTagInfo& info : infos) {
        if (info.timestamp < visibleStart || info.timestamp > visibleEnd) {
            continue;
        }

        const long double normalized =
            (static_cast<long double>(info.timestamp) - static_cast<long double>(visibleStart)) / visibleRange;
        const int timestampX = std::clamp(
            static_cast<int>(std::llround(static_cast<long double>(plot.left())
                                          + normalized * static_cast<long double>(plot.width()))),
            plot.left(),
            plot.right());
        QString label = info.opcode.trimmed().isEmpty() ? QStringLiteral("Unknown") : info.opcode.trimmed();
        PendingOpcodeTag pending;
        pending.info = info;
        pending.label = label;
        pending.timestampX = timestampX;
        pending.fullWidth = tagMetrics.horizontalAdvance(label) + kOpcodeTagHorizontalPadding * 2;
        pendingTags.push_back(std::move(pending));
    }

    if (pendingTags.empty()) {
        return layouts;
    }

    const int minimumWidth = kOpcodeTagHorizontalPadding * 2 + kOpcodeTagMinTextWidth;
    enum class TagWidthMode {
        PreferFull,
        MinimumFirst
    };
    const auto buildAttempt = [&](const std::vector<int>& pendingOrder, const TagWidthMode widthMode) {
        std::vector<OpcodeTagLayout> attemptLayouts;
        std::vector<QRect> attemptTopTagRects;
        std::vector<QRect> attemptBottomTagRects;
        attemptLayouts.reserve(pendingTags.size());
        attemptTopTagRects.reserve(pendingTags.size());
        attemptBottomTagRects.reserve(pendingTags.size());

        const auto attemptLaneRects = [&](const bool bottomPlacement) -> const std::vector<QRect>& {
            return bottomPlacement ? attemptBottomTagRects : attemptTopTagRects;
        };
        const auto mutableAttemptLaneRects = [&](const bool bottomPlacement) -> std::vector<QRect>& {
            return bottomPlacement ? attemptBottomTagRects : attemptTopTagRects;
        };
        const auto placeTagWithExistingRects = [&](const int timestampX,
                                                   const int width,
                                                   const bool bottomPlacement,
                                                   const std::vector<QRect>& existingRects) -> std::optional<QRect> {
            if (!laneAvailable(bottomPlacement) || width <= 0 || width > plot.width()) {
                return std::nullopt;
            }
            const int minLeft = std::max(plot.left(), timestampX - width + 1);
            const int maxLeft = std::min(timestampX, plot.right() - width + 1);
            if (minLeft > maxLeft) {
                return std::nullopt;
            }

            std::vector<std::pair<int, int>> intervals = {{minLeft, maxLeft}};
            for (const QRect& existingRect : existingRects) {
                subtractInterval(intervals,
                                 existingRect.left() - kOpcodeTagGap - width + 1,
                                 existingRect.right() + kOpcodeTagGap);
                if (intervals.empty()) {
                    return std::nullopt;
                }
            }

            const int idealLeft = std::clamp(timestampX - width / 2, minLeft, maxLeft);
            int bestLeft = 0;
            int bestDistance = (std::numeric_limits<int>::max)();
            bool found = false;
            for (const auto& interval : intervals) {
                const int candidateLeft = std::clamp(idealLeft, interval.first, interval.second);
                const int distance = std::abs(candidateLeft - idealLeft);
                if (!found || distance < bestDistance) {
                    found = true;
                    bestDistance = distance;
                    bestLeft = candidateLeft;
                }
            }
            if (!found) {
                return std::nullopt;
            }

            return QRect(bestLeft, tagTopForPlacement(bottomPlacement), width, tagHeight);
        };
        const auto placeTagWithWidth = [&](const int timestampX,
                                           const int width,
                                           const bool bottomPlacement) -> std::optional<QRect> {
            return placeTagWithExistingRects(timestampX, width, bottomPlacement, attemptLaneRects(bottomPlacement));
        };
        const auto tagTextForWidth = [&](const QString& label, const int width) {
            const int textWidth = width - kOpcodeTagHorizontalPadding * 2;
            if (textWidth <= 0) {
                return QString();
            }
            QString text = tagMetrics.horizontalAdvance(label) <= textWidth
                ? label
                : tagMetrics.elidedText(label, Qt::ElideRight, textWidth);
            if (text.isEmpty() || tagMetrics.horizontalAdvance(text.left(1)) > textWidth) {
                return QString();
            }
            return text;
        };
        const auto placePendingTag = [&](const PendingOpcodeTag& pending,
                                         QString& placedText,
                                         bool& placedBottom) -> std::optional<QRect> {
            const std::array<bool, 2> order = placementOrder(primaryBottomPlacement);
            const int maximumWidth = std::min(std::max(minimumWidth, pending.fullWidth), plot.width());
            if (widthMode == TagWidthMode::MinimumFirst) {
                const int width = std::min(maximumWidth, minimumWidth);
                for (const bool bottomPlacement : order) {
                    if (std::optional<QRect> rect =
                            placeTagWithWidth(pending.timestampX, width, bottomPlacement)) {
                        placedText = tagTextForWidth(pending.label, width);
                        if (placedText.isEmpty()) {
                            continue;
                        }
                        placedBottom = bottomPlacement;
                        return rect;
                    }
                }
                return std::nullopt;
            }

            if (pending.fullWidth <= plot.width()) {
                for (const bool bottomPlacement : order) {
                    if (std::optional<QRect> rect =
                            placeTagWithWidth(pending.timestampX, pending.fullWidth, bottomPlacement)) {
                        placedText = pending.label;
                        placedBottom = bottomPlacement;
                        return rect;
                    }
                }
            }

            for (int width = maximumWidth; width >= minimumWidth; --width) {
                for (const bool bottomPlacement : order) {
                    std::optional<QRect> rect =
                        placeTagWithWidth(pending.timestampX, width, bottomPlacement);
                    if (!rect) {
                        continue;
                    }
                    QString text = tagTextForWidth(pending.label, width);
                    if (text.isEmpty()) {
                        continue;
                    }
                    placedText = std::move(text);
                    placedBottom = bottomPlacement;
                    return rect;
                }
            }
            return std::nullopt;
        };

        for (const int pendingIndex : pendingOrder) {
            if (pendingIndex < 0 || pendingIndex >= static_cast<int>(pendingTags.size())) {
                continue;
            }
            const PendingOpcodeTag& pending = pendingTags[static_cast<std::size_t>(pendingIndex)];
            QString placedText;
            bool placedBottom = primaryBottomPlacement;
            std::optional<QRect> placedRect = placePendingTag(pending, placedText, placedBottom);
            if (!placedRect) {
                continue;
            }

            OpcodeTagLayout layout;
            layout.info = pending.info;
            layout.text = placedText;
            layout.color = ChannelAccent(pending.info.channel);
            layout.fullWidth = pending.fullWidth;
            layout.timestampX = std::clamp(pending.timestampX, placedRect->left(), placedRect->right());
            layout.bottomPlacement = placedBottom;
            layout.tagRect = *placedRect;
            layout.inlineTextRect = inlineTextRect;
            layout.selected = selectedOpcodeTagSpanIndex_ == selectedSpanIndex_
                && selectedOpcodeTagLogicalRow_ == pending.info.logicalRow
                && selectedOpcodeTagTimestamp_ == pending.info.timestamp;
            updateConnectorGeometry(layout);

            mutableAttemptLaneRects(layout.bottomPlacement).push_back(layout.tagRect);
            attemptLayouts.push_back(layout);
        }
        if (widthMode == TagWidthMode::MinimumFirst) {
            for (std::size_t layoutIndex = 0; layoutIndex < attemptLayouts.size(); ++layoutIndex) {
                OpcodeTagLayout& layout = attemptLayouts[layoutIndex];
                const int maximumWidth = std::min(std::max(minimumWidth, layout.fullWidth), plot.width());
                if (maximumWidth <= layout.tagRect.width()) {
                    continue;
                }

                std::vector<QRect> sameLaneRects;
                sameLaneRects.reserve(attemptLayouts.size());
                for (std::size_t otherIndex = 0; otherIndex < attemptLayouts.size(); ++otherIndex) {
                    if (otherIndex == layoutIndex) {
                        continue;
                    }
                    const OpcodeTagLayout& other = attemptLayouts[otherIndex];
                    if (other.bottomPlacement == layout.bottomPlacement) {
                        sameLaneRects.push_back(other.tagRect);
                    }
                }

                for (int width = maximumWidth; width > layout.tagRect.width(); --width) {
                    std::optional<QRect> expandedRect =
                        placeTagWithExistingRects(layout.timestampX, width, layout.bottomPlacement, sameLaneRects);
                    if (!expandedRect) {
                        continue;
                    }
                    QString expandedText = tagTextForWidth(layout.info.opcode, width);
                    if (expandedText.isEmpty()) {
                        continue;
                    }
                    layout.tagRect = *expandedRect;
                    layout.text = std::move(expandedText);
                    layout.timestampX = std::clamp(layout.timestampX, layout.tagRect.left(), layout.tagRect.right());
                    updateConnectorGeometry(layout);
                    break;
                }
            }
        }
        std::sort(attemptLayouts.begin(), attemptLayouts.end(), [](const OpcodeTagLayout& lhs,
                                                                    const OpcodeTagLayout& rhs) {
            if (lhs.info.timestamp != rhs.info.timestamp) {
                return lhs.info.timestamp < rhs.info.timestamp;
            }
            return lhs.info.logicalRow < rhs.info.logicalRow;
        });
        return attemptLayouts;
    };

    const auto layoutScore = [](const std::vector<OpcodeTagLayout>& scoredLayouts) {
        int fullTextCount = 0;
        int totalWidth = 0;
        for (const OpcodeTagLayout& layout : scoredLayouts) {
            fullTextCount += layout.text == layout.info.opcode ? 1 : 0;
            totalWidth += layout.tagRect.width();
        }
        return std::tuple<int, int, int>(static_cast<int>(scoredLayouts.size()), fullTextCount, totalWidth);
    };

    std::vector<int> timestampOrder(pendingTags.size());
    std::iota(timestampOrder.begin(), timestampOrder.end(), 0);
    std::vector<int> widthOrder = timestampOrder;
    std::sort(widthOrder.begin(), widthOrder.end(), [&](const int lhs, const int rhs) {
        const PendingOpcodeTag& lhsTag = pendingTags[static_cast<std::size_t>(lhs)];
        const PendingOpcodeTag& rhsTag = pendingTags[static_cast<std::size_t>(rhs)];
        if (lhsTag.fullWidth != rhsTag.fullWidth) {
            return lhsTag.fullWidth < rhsTag.fullWidth;
        }
        if (lhsTag.info.timestamp != rhsTag.info.timestamp) {
            return lhsTag.info.timestamp < rhsTag.info.timestamp;
        }
        return lhsTag.info.logicalRow < rhsTag.info.logicalRow;
    });
    std::vector<int> wideDeferredOrder;
    wideDeferredOrder.reserve(pendingTags.size());
    std::vector<int> deferredWideTags;
    deferredWideTags.reserve(pendingTags.size());
    const int wideTagThreshold = std::max(minimumWidth, plot.width() / 4);
    for (const int index : timestampOrder) {
        const PendingOpcodeTag& pending = pendingTags[static_cast<std::size_t>(index)];
        if (pending.fullWidth > wideTagThreshold) {
            deferredWideTags.push_back(index);
        } else {
            wideDeferredOrder.push_back(index);
        }
    }
    wideDeferredOrder.insert(wideDeferredOrder.end(), deferredWideTags.begin(), deferredWideTags.end());

    layouts = buildAttempt(timestampOrder, TagWidthMode::PreferFull);
    std::vector<OpcodeTagLayout> widthAttempt = buildAttempt(widthOrder, TagWidthMode::PreferFull);
    if (layoutScore(widthAttempt) > layoutScore(layouts)) {
        layouts = std::move(widthAttempt);
    }
    std::vector<OpcodeTagLayout> wideDeferredAttempt = buildAttempt(wideDeferredOrder, TagWidthMode::PreferFull);
    if (layoutScore(wideDeferredAttempt) > layoutScore(layouts)) {
        layouts = std::move(wideDeferredAttempt);
    }
    std::vector<OpcodeTagLayout> minimumFirstAttempt =
        buildAttempt(timestampOrder, TagWidthMode::MinimumFirst);
    if (layoutScore(minimumFirstAttempt) > layoutScore(layouts)) {
        layouts = std::move(minimumFirstAttempt);
    }
    std::vector<OpcodeTagLayout> minimumFirstWidthAttempt =
        buildAttempt(widthOrder, TagWidthMode::MinimumFirst);
    if (layoutScore(minimumFirstWidthAttempt) > layoutScore(layouts)) {
        layouts = std::move(minimumFirstWidthAttempt);
    }
    return layouts;
}

void TransactionWidget::paintSelectedOpcodeTags(QPainter& painter,
                                                const QRect& plot,
                                                const qint64 visibleStart,
                                                const qint64 visibleEnd,
                                                const long double visibleRange,
                                                const bool inlineLabelsDrawn) const
{
    const std::vector<OpcodeTagLayout> layouts =
        selectedOpcodeTagLayouts(plot, visibleStart, visibleEnd, visibleRange, inlineLabelsDrawn);
    if (layouts.empty()) {
        return;
    }

    painter.save();
    painter.setClipRect(QRect(plot.left(), plot.top(), plot.width(), plot.height() + 1),
                        Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(opcodeTagFont());
    for (const OpcodeTagLayout& layout : layouts) {
        if (layout.lineVisible) {
            painter.setPen(QPen(layout.color, 1, Qt::DotLine, Qt::RoundCap));
            painter.drawLine(layout.lineRect.topLeft(), layout.lineRect.bottomLeft());
        }
        if (layout.triangleVisible) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(layout.color);
            QPolygon polygon;
            polygon << layout.trianglePoints[0] << layout.trianglePoints[1] << layout.trianglePoints[2];
            painter.drawPolygon(polygon);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(layout.color);
        painter.drawRoundedRect(layout.tagRect, 4.0, 4.0);
        if (layout.selected) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(QStringLiteral("#000000")), 2));
            painter.drawRoundedRect(layout.tagRect.adjusted(1, 1, -1, -1), 3.0, 3.0);
        }
        painter.setPen(QColor(QStringLiteral("#000000")));
        painter.drawText(layout.tagRect.adjusted(kOpcodeTagHorizontalPadding, 0, -kOpcodeTagHorizontalPadding, 0),
                         Qt::AlignCenter,
                         layout.text);
    }
    painter.restore();
}

std::optional<TransactionWidget::OpcodeTagLayout> TransactionWidget::opcodeTagLayoutAtPosition(const QPoint& position) const
{
    const QRect plot = plotRect();
    if (!plot.contains(position)) {
        return std::nullopt;
    }
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const bool dense = densePaintEnabledForVisible(plot, visibleSpanCount(visibleStart, visibleEnd, (std::numeric_limits<int>::max)()));
    const std::vector<OpcodeTagLayout> layouts =
        selectedOpcodeTagLayouts(plot, visibleStart, visibleEnd, visibleRange, !dense);
    for (int index = static_cast<int>(layouts.size()) - 1; index >= 0; --index) {
        const OpcodeTagLayout& layout = layouts[static_cast<std::size_t>(index)];
        if (opcodeTagHitRect(layout.tagRect, plot).contains(position)) {
            return layout;
        }
    }
    return std::nullopt;
}

int TransactionWidget::opcodeTagIndexAtPosition(const QPoint& position) const
{
    const QRect plot = plotRect();
    if (!plot.contains(position)) {
        return -1;
    }
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const bool dense = densePaintEnabledForVisible(plot, visibleSpanCount(visibleStart, visibleEnd, (std::numeric_limits<int>::max)()));
    const std::vector<OpcodeTagLayout> layouts =
        selectedOpcodeTagLayouts(plot, visibleStart, visibleEnd, visibleRange, !dense);
    for (int index = static_cast<int>(layouts.size()) - 1; index >= 0; --index) {
        const OpcodeTagLayout& layout = layouts[static_cast<std::size_t>(index)];
        if (opcodeTagHitRect(layout.tagRect, plot).contains(position)) {
            return index;
        }
    }
    return -1;
}

void TransactionWidget::clearSelectedOpcodeTag()
{
    selectedOpcodeTagSpanIndex_ = -1;
    selectedOpcodeTagLogicalRow_ = 0;
    selectedOpcodeTagTimestamp_ = 0;
}

void TransactionWidget::selectOpcodeTagLayout(const OpcodeTagLayout& layout, const bool moveCursor)
{
    selectedOpcodeTagSpanIndex_ = selectedSpanIndex_;
    selectedOpcodeTagLogicalRow_ = layout.info.logicalRow;
    selectedOpcodeTagTimestamp_ = layout.info.timestamp;
    selectedLogicalRow_ = static_cast<int>(layout.info.logicalRow);
    if (moveCursor) {
        cursorTimestamp_ = layout.info.timestamp;
        hasCursor_ = true;
    }
    updateWidgetHints();
    update();
}

bool TransactionWidget::selectOpcodeTagAtPosition(const QPoint& position)
{
    const std::optional<OpcodeTagLayout> layout = opcodeTagLayoutAtPosition(position);
    if (!layout) {
        return false;
    }
    selectOpcodeTagLayout(*layout, true);
    return true;
}

bool TransactionWidget::activateOpcodeTagAtPosition(const QPoint& position)
{
    const std::optional<OpcodeTagLayout> layout = opcodeTagLayoutAtPosition(position);
    if (!layout) {
        return false;
    }
    const int activatedLogicalRow = static_cast<int>(layout->info.logicalRow);
    const std::uint64_t preservedScrollOffset = scrollOffset_;
    const int preservedVerticalScrollOffset = verticalScrollOffset_;
    selectOpcodeTagLayout(*layout, true);
    Q_EMIT logicalRowActivated(activatedLogicalRow);
    scrollOffset_ = preservedScrollOffset;
    verticalScrollOffset_ = preservedVerticalScrollOffset;
    selectOpcodeTagLayout(*layout, true);
    stickCursorToSelectedOpcodeTag(false);
    updateScrollBars();
    updateWidgetHints();
    update();
    return true;
}

int TransactionWidget::spanIndexAtPosition(const QPoint& position) const
{
    const QRect plot = plotRect();
    if (!plot.contains(position)) {
        return -1;
    }
    const int laneIndex = laneIndexAtY(position.y(), plot);
    if (laneIndex < 0) {
        return -1;
    }

    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const qint64 hitHalfRange =
        std::max<qint64>(1,
                         clampedRoundToTimestamp(visibleRange
                                                 * 8.0L
                                                 / static_cast<long double>(std::max(1, plot.width()))));
    const qint64 timestamp = plotXToTimestamp(position.x(), plot);
    const qint64 hitStart = std::max<qint64>(visibleStart, timestamp - hitHalfRange);
    const qint64 hitEnd = std::min<qint64>(visibleEnd, timestamp + hitHalfRange);
    int bestIndex = -1;
    int bestDistance = (std::numeric_limits<int>::max)();
    const std::vector<int> candidates = visibleSpanIndicesForLane(laneIndex, hitStart, hitEnd);
    for (const int index : candidates) {
        const QRect rect =
            spanVisualRectForRange(spans_[static_cast<std::size_t>(index)],
                                   plot,
                                   visibleStart,
                                   visibleEnd,
                                   visibleRange)
                .adjusted(-3, -5, 3, 5);
        if (!rect.isValid() || !rect.contains(position)) {
            continue;
        }
        if (shouldDrawFilteredOutSpan(index)) {
            continue;
        }
        const int centerDistance = std::abs(rect.center().y() - position.y())
            + std::abs(rect.center().x() - position.x());
        if (centerDistance < bestDistance) {
            bestDistance = centerDistance;
            bestIndex = index;
        }
    }
    return bestIndex;
}

int TransactionWidget::nearestVisibleSpanIndexAtPosition(const QPoint& position) const
{
    const QRect plot = plotRect();
    if (!plot.contains(position)) {
        return -1;
    }

    const int laneIndex = laneIndexAtY(position.y(), plot);
    if (laneIndex < 0) {
        return -1;
    }

    const qint64 timestamp = plotXToTimestamp(position.x(), plot);
    int bestIndex = -1;
    qint64 bestDistance = (std::numeric_limits<qint64>::max)();
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const qint64 searchHalfRange =
        std::max<qint64>(1,
                         clampedRoundToTimestamp(visibleRange
                                                 * 24.0L
                                                 / static_cast<long double>(std::max(1, plot.width()))));
    const std::vector<int> candidates =
        visibleSpanIndicesForLane(laneIndex,
                                  std::max<qint64>(visibleStart, timestamp - searchHalfRange),
                                  std::min<qint64>(visibleEnd, timestamp + searchHalfRange));
    for (const int index : candidates) {
        const TransactionSpan& span = spans_[static_cast<std::size_t>(index)];
        if (shouldDrawFilteredOutSpan(index)) {
            continue;
        }
        const qint64 distance = timestamp < span.startTimestamp
            ? span.startTimestamp - timestamp
            : (timestamp > span.endTimestamp ? timestamp - span.endTimestamp : 0);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }
    return bestIndex;
}

bool TransactionWidget::densePaintEnabled(const QRect& plot) const
{
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const int threshold = plot.width() <= 0
        ? kDensePaintSpanThreshold
        : std::max(kDensePaintSpanThreshold, plot.width() * kDensePaintPixelsPerSpan + 1);
    return densePaintEnabledForVisible(plot, visibleSpanCount(visibleStart, visibleEnd, threshold));
}

bool TransactionWidget::densePaintEnabledForVisible(const QRect& plot, const int visibleSpanCount) const
{
    if (visibleSpanCount < kDensePaintSpanThreshold) {
        return false;
    }
    return plot.width() <= 0 || visibleSpanCount > plot.width() * kDensePaintPixelsPerSpan;
}

TransactionWidget::DenseSpanPaintCacheKey
TransactionWidget::denseSpanPaintCacheKey(const QRect& plot,
                                          const qint64 visibleStart,
                                          const qint64 visibleEnd,
                                          const int firstLane,
                                          const int lastLane) const
{
    DenseSpanPaintCacheKey key;
    key.generation = buildGeneration_.load(std::memory_order_relaxed);
    key.width = plot.width();
    key.height = plot.height();
    key.rowHeight = rowHeight();
    key.visibleStart = visibleStart;
    key.visibleEnd = visibleEnd;
    key.firstLane = firstLane;
    key.lastLane = lastLane;
    key.filterRevision = filterRevision_;
    key.filterMode = static_cast<int>(filterMode_);
    key.filterActive = transactionFilterActive();
    return key;
}

TransactionWidget::DenseSpanLanePaintCacheKey
TransactionWidget::denseSpanLanePaintCacheKey(const QRect& plot,
                                              const qint64 visibleStart,
                                              const qint64 visibleEnd,
                                              const int laneIndex) const
{
    DenseSpanLanePaintCacheKey key;
    key.generation = buildGeneration_.load(std::memory_order_relaxed);
    key.width = plot.width();
    key.rowHeight = rowHeight();
    key.visibleStart = visibleStart;
    key.visibleEnd = visibleEnd;
    key.laneIndex = laneIndex;
    key.filterRevision = filterRevision_;
    key.filterMode = static_cast<int>(filterMode_);
    key.filterActive = transactionFilterActive();
    return key;
}

void TransactionWidget::invalidateDenseSpanPaintCache() const
{
    denseSpanPaintCache_.valid = false;
    denseSpanPaintCache_.image = QImage();
    denseSpanPaintCache_.result = {};
    denseSpanLanePaintCache_.clear();
}

TransactionWidget::DenseSpanPaintResult TransactionWidget::paintDenseSpans(QPainter& painter,
                                                                           const QRect& plot,
                                                                           const qint64 visibleStart,
                                                                           const qint64 visibleEnd,
                                                                           const long double visibleRange,
                                                                           const int firstLane,
                                                                           const int lastLane) const
{
    const auto appendInteractiveSpan = [&](DenseSpanPaintResult& result, const int spanIndex) {
        if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
            return;
        }
        if (std::find(result.preservedSpanIndices.begin(), result.preservedSpanIndices.end(), spanIndex)
            != result.preservedSpanIndices.end()) {
            return;
        }
        const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
        if (span.laneIndex < firstLane || span.laneIndex >= lastLane) {
            return;
        }
        if (shouldDrawFilteredOutSpan(spanIndex)) {
            return;
        }
        if (span.endTimestamp < visibleStart || span.startTimestamp > visibleEnd) {
            return;
        }
        const QRect rect = spanVisualRectForRange(span, plot, visibleStart, visibleEnd, visibleRange);
        if (rect.isValid() && rect.intersects(plot)) {
            result.preservedSpanIndices.push_back(spanIndex);
        }
    };

    const DenseSpanPaintCacheKey key =
        denseSpanPaintCacheKey(plot, visibleStart, visibleEnd, firstLane, lastLane);
    if (denseSpanPaintCache_.valid
        && denseSpanPaintCache_.key == key
        && !denseSpanPaintCache_.image.isNull()) {
        painter.drawImage(plot.topLeft(), denseSpanPaintCache_.image);
        DenseSpanPaintResult result;
        appendInteractiveSpan(result, selectedSpanIndex_);
        appendInteractiveSpan(result, hoveredSpanIndex_);
        return result;
    }

    QImage image(std::max(1, plot.width()), std::max(1, plot.height()), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter imagePainter(&image);
    imagePainter.translate(-plot.left(), -plot.top());
    for (int laneIndex = firstLane; laneIndex < lastLane; ++laneIndex) {
        paintDenseSpanLaneRow(imagePainter, plot, visibleStart, visibleEnd, visibleRange, laneIndex);
    }
    imagePainter.end();

    painter.drawImage(plot.topLeft(), image);
    denseSpanPaintCache_.key = key;
    denseSpanPaintCache_.image = std::move(image);
    denseSpanPaintCache_.result = {};
    denseSpanPaintCache_.valid = true;
    DenseSpanPaintResult result;
    appendInteractiveSpan(result, selectedSpanIndex_);
    appendInteractiveSpan(result, hoveredSpanIndex_);
    return result;
}

TransactionWidget::DenseSpanPaintResult TransactionWidget::paintDenseSpansUncached(QPainter& painter,
                                                                                   const QRect& plot,
                                                                                   const qint64 visibleStart,
                                                                                   const qint64 visibleEnd,
                                                                                   const long double visibleRange,
                                                                                   const int firstLane,
                                                                                   const int lastLane) const
{
    DenseSpanPaintResult result;
    if (plot.width() <= 0 || plot.height() <= 0 || visibleRange <= 0.0L) {
        return result;
    }

    std::vector<std::array<std::vector<QLine>, kDensePaintBucketCount>> rowRuns(
        static_cast<std::size_t>(std::max(0, lastLane - firstLane)));
    const long double pixelsPerTimestamp =
        static_cast<long double>(plot.width()) / std::max<long double>(1.0L, visibleRange);

    for (int laneIndex = firstLane; laneIndex < lastLane; ++laneIndex) {
        if (laneIndex < 0 || laneIndex >= static_cast<int>(spanIndicesByLane_.size())) {
            continue;
        }

        const std::vector<int> laneSpanIndices = visibleSpanIndicesForLane(laneIndex, visibleStart, visibleEnd);
        if (laneSpanIndices.empty()) {
            continue;
        }

        const int rowTop = laneTopY(laneIndex, plot);
        const int lineY = std::clamp(rowTop + rowHeight() / 2, plot.top(), plot.bottom());
        int currentBucket = -1;
        int currentLeft = 0;
        int currentRight = -1;

        const auto flushRun = [&]() {
            if (currentBucket < 0 || currentRight < currentLeft) {
                return;
            }
            const std::size_t rowIndex = static_cast<std::size_t>(laneIndex - firstLane);
            rowRuns[rowIndex][static_cast<std::size_t>(currentBucket)].push_back(
                QLine(currentLeft, lineY, currentRight, lineY));
        };

        std::vector<int> coverageDiff(static_cast<std::size_t>(plot.width() + 1), 0);
        for (const int spanIndex : laneSpanIndices) {
            const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
            if (shouldDrawFilteredOutSpan(spanIndex)) {
                continue;
            }
            const long double startOffset =
                (static_cast<long double>(span.startTimestamp) - static_cast<long double>(visibleStart))
                * pixelsPerTimestamp;
            const long double endOffset =
                (static_cast<long double>(span.endTimestamp) - static_cast<long double>(visibleStart))
                * pixelsPerTimestamp;
            const int x0 = static_cast<int>(std::floor(static_cast<long double>(plot.left()) + startOffset));
            const int x1 = static_cast<int>(std::ceil(static_cast<long double>(plot.left()) + endOffset));
            const int left = std::clamp(std::min(x0, x1), plot.left(), plot.right());
            const int right = std::clamp(std::max(x0, x1), plot.left(), plot.right());
            const int visualWidth = std::max(5, right - left + 1);
            const int leftColumn = std::clamp(left - plot.left(), 0, plot.width() - 1);
            const int rightColumn = std::clamp(left + visualWidth - 1 - plot.left(), 0, plot.width() - 1);
            ++coverageDiff[static_cast<std::size_t>(leftColumn)];
            --coverageDiff[static_cast<std::size_t>(rightColumn + 1)];
            if (visualWidth >= kDensePaintPreserveRectMinWidth) {
                result.preservedSpanIndices.push_back(spanIndex);
            }
        }

        const int bucket = std::clamp(denseSpanBucketForChannelKind(
                                          channelKindFromOpcodeOrOrigin(
                                              lanes_[static_cast<std::size_t>(laneIndex)].requestOpcode,
                                              lanes_[static_cast<std::size_t>(laneIndex)].originKind)),
                                      0,
                                      kDensePaintBucketCount - 1);
        int coverage = 0;
        for (int column = 0; column < plot.width(); ++column) {
            coverage += coverageDiff[static_cast<std::size_t>(column)];
            const int x = plot.left() + column;
            if (coverage <= kDensePaintMaxSpansPerPixelLane) {
                flushRun();
                currentBucket = -1;
                currentLeft = 0;
                currentRight = -1;
                continue;
            }

            if (bucket == currentBucket && x <= currentRight + 1) {
                currentRight = x;
            } else {
                flushRun();
                currentBucket = bucket;
                currentLeft = x;
                currentRight = x;
            }
        }
        flushRun();
    }

    painter.save();
    painter.setClipRect(plot, Qt::IntersectClip);
    for (std::size_t rowIndex = 0; rowIndex < rowRuns.size(); ++rowIndex) {
        const int verticalInset = rowHeight() >= 18 ? 2 : (rowHeight() >= 14 ? 1 : 0);
        const int lineHeight = std::max(1, rowHeight() - verticalInset * 2);
        for (int bucket = 0; bucket < kDensePaintBucketCount; ++bucket) {
            const std::vector<QLine>& runs = rowRuns[rowIndex][static_cast<std::size_t>(bucket)];
            if (runs.empty()) {
                continue;
            }
            painter.setPen(QPen(denseSpanBucketColor(bucket), lineHeight, Qt::SolidLine, Qt::SquareCap));
            painter.drawLines(runs.data(), static_cast<int>(runs.size()));
        }
    }
    painter.restore();
    return result;
}

void TransactionWidget::paintDenseSpanLaneRow(QPainter& painter,
                                              const QRect& plot,
                                              const qint64 visibleStart,
                                              const qint64 visibleEnd,
                                              const long double visibleRange,
                                              const int laneIndex) const
{
    if (plot.width() <= 0 || plot.height() <= 0 || visibleRange <= 0.0L) {
        return;
    }
    if (laneIndex < 0 || laneIndex >= static_cast<int>(spanIndicesByLane_.size())) {
        return;
    }

    const int rowTop = laneTopY(laneIndex, plot);
    const QRect rowTarget(plot.left(), rowTop, plot.width(), rowHeight());
    if (!rowTarget.intersects(plot)) {
        return;
    }

    const DenseSpanLanePaintCacheKey key =
        denseSpanLanePaintCacheKey(plot, visibleStart, visibleEnd, laneIndex);
    if (denseSpanLanePaintCache_.size() != lanes_.size()) {
        denseSpanLanePaintCache_.clear();
        denseSpanLanePaintCache_.resize(lanes_.size());
    }
    DenseSpanLanePaintCache& cache =
        denseSpanLanePaintCache_[static_cast<std::size_t>(laneIndex)];
    if (cache.valid && cache.key == key && !cache.image.isNull()) {
        if (cache.hasContent) {
            painter.drawImage(rowTarget.topLeft(), cache.image);
        }
        return;
    }

    cache.key = key;
    cache.image = QImage(std::max(1, plot.width()),
                         std::max(1, rowHeight()),
                         QImage::Format_ARGB32_Premultiplied);
    cache.image.fill(Qt::transparent);
    cache.hasContent = false;

    const std::vector<int> laneSpanIndices = visibleSpanIndicesForLane(laneIndex, visibleStart, visibleEnd);
    if (!laneSpanIndices.empty()) {
        const long double pixelsPerTimestamp =
            static_cast<long double>(plot.width()) / std::max<long double>(1.0L, visibleRange);
        std::vector<int> coverageDiff(static_cast<std::size_t>(plot.width() + 1), 0);
        std::vector<int> baseSpanIndices;
        baseSpanIndices.reserve(laneSpanIndices.size());

        for (const int spanIndex : laneSpanIndices) {
            const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
            if (shouldDrawFilteredOutSpan(spanIndex)) {
                continue;
            }
            const long double startOffset =
                (static_cast<long double>(span.startTimestamp) - static_cast<long double>(visibleStart))
                * pixelsPerTimestamp;
            const long double endOffset =
                (static_cast<long double>(span.endTimestamp) - static_cast<long double>(visibleStart))
                * pixelsPerTimestamp;
            const int x0 = static_cast<int>(std::floor(startOffset));
            const int x1 = static_cast<int>(std::ceil(endOffset));
            const int left = std::clamp(std::min(x0, x1), 0, plot.width() - 1);
            const int right = std::clamp(std::max(x0, x1), 0, plot.width() - 1);
            const int visualWidth = std::max(5, right - left + 1);
            const int rightColumn = std::clamp(left + visualWidth - 1, 0, plot.width() - 1);
            ++coverageDiff[static_cast<std::size_t>(left)];
            --coverageDiff[static_cast<std::size_t>(rightColumn + 1)];
            if (visualWidth >= kDensePaintPreserveRectMinWidth) {
                baseSpanIndices.push_back(spanIndex);
            }
        }

        QPainter imagePainter(&cache.image);
        imagePainter.setRenderHint(QPainter::Antialiasing, false);
        imagePainter.translate(-plot.left(), -rowTop);
        imagePainter.setClipRect(rowTarget, Qt::IntersectClip);

        const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
        const int bucket = std::clamp(denseSpanBucketForChannelKind(
                                          channelKindFromOpcodeOrOrigin(lane.requestOpcode, lane.originKind)),
                                      0,
                                      kDensePaintBucketCount - 1);
        const int verticalInset = rowHeight() >= 18 ? 2 : (rowHeight() >= 14 ? 1 : 0);
        const int lineHeight = std::max(1, rowHeight() - verticalInset * 2);
        const int lineY = std::clamp(rowTop + rowHeight() / 2, rowTarget.top(), rowTarget.bottom());
        imagePainter.setPen(QPen(denseSpanBucketColor(bucket), lineHeight, Qt::SolidLine, Qt::SquareCap));
        int coverage = 0;
        int currentLeft = 0;
        int currentRight = -1;
        const auto flushRun = [&]() {
            if (currentRight < currentLeft) {
                return;
            }
            imagePainter.drawLine(QLine(currentLeft, lineY, currentRight, lineY));
            cache.hasContent = true;
        };
        for (int column = 0; column < plot.width(); ++column) {
            coverage += coverageDiff[static_cast<std::size_t>(column)];
            const int x = plot.left() + column;
            if (coverage <= kDensePaintMaxSpansPerPixelLane) {
                flushRun();
                currentLeft = 0;
                currentRight = -1;
                continue;
            }
            if (currentRight >= currentLeft && x <= currentRight + 1) {
                currentRight = x;
            } else {
                flushRun();
                currentLeft = x;
                currentRight = x;
            }
        }
        flushRun();

        if (!baseSpanIndices.empty()) {
            paintSpanRectangles(imagePainter,
                                baseSpanIndices,
                                plot,
                                visibleStart,
                                visibleEnd,
                                visibleRange,
                                false,
                                false,
                                false);
            cache.hasContent = true;
        }
        imagePainter.end();
    }

    cache.valid = true;
    if (cache.hasContent) {
        painter.drawImage(rowTarget.topLeft(), cache.image);
    }
}

void TransactionWidget::paintDenseFilteredOutSpans(QPainter& painter,
                                                   const QRect& plot,
                                                   const qint64 visibleStart,
                                                   const qint64 visibleEnd,
                                                   const long double visibleRange,
                                                   const int firstLane,
                                                   const int lastLane) const
{
    if (plot.width() <= 0 || plot.height() <= 0 || visibleRange <= 0.0L) {
        return;
    }

    std::vector<std::vector<QLine>> rowRuns(static_cast<std::size_t>(std::max(0, lastLane - firstLane)));
    const long double pixelsPerTimestamp =
        static_cast<long double>(plot.width()) / std::max<long double>(1.0L, visibleRange);

    for (int laneIndex = firstLane; laneIndex < lastLane; ++laneIndex) {
        if (laneIndex < 0 || laneIndex >= static_cast<int>(spanIndicesByLane_.size())) {
            continue;
        }
        const std::vector<int> laneSpanIndices = visibleSpanIndicesForLane(laneIndex, visibleStart, visibleEnd);
        if (laneSpanIndices.empty()) {
            continue;
        }

        const int rowTop = laneTopY(laneIndex, plot);
        const int lineY = std::clamp(rowTop + rowHeight() / 2, plot.top(), plot.bottom());
        std::vector<int> coverageDiff(static_cast<std::size_t>(plot.width() + 1), 0);
        for (const int spanIndex : laneSpanIndices) {
            const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
            if (!shouldDrawFilteredOutSpan(spanIndex)) {
                continue;
            }
            const long double startOffset =
                (static_cast<long double>(span.startTimestamp) - static_cast<long double>(visibleStart))
                * pixelsPerTimestamp;
            const long double endOffset =
                (static_cast<long double>(span.endTimestamp) - static_cast<long double>(visibleStart))
                * pixelsPerTimestamp;
            const int x0 = static_cast<int>(std::floor(static_cast<long double>(plot.left()) + startOffset));
            const int x1 = static_cast<int>(std::ceil(static_cast<long double>(plot.left()) + endOffset));
            const int left = std::clamp(std::min(x0, x1), plot.left(), plot.right());
            const int right = std::clamp(std::max(x0, x1), plot.left(), plot.right());
            const int visualWidth = std::max(5, right - left + 1);
            const int leftColumn = std::clamp(left - plot.left(), 0, plot.width() - 1);
            const int rightColumn = std::clamp(left + visualWidth - 1 - plot.left(), 0, plot.width() - 1);
            ++coverageDiff[static_cast<std::size_t>(leftColumn)];
            --coverageDiff[static_cast<std::size_t>(rightColumn + 1)];
        }

        int coverage = 0;
        int currentLeft = 0;
        int currentRight = -1;
        const auto flushRun = [&]() {
            if (currentRight < currentLeft) {
                return;
            }
            rowRuns[static_cast<std::size_t>(laneIndex - firstLane)].push_back(
                QLine(currentLeft, lineY, currentRight, lineY));
        };
        for (int column = 0; column < plot.width(); ++column) {
            coverage += coverageDiff[static_cast<std::size_t>(column)];
            const int x = plot.left() + column;
            if (coverage <= 0) {
                flushRun();
                currentLeft = 0;
                currentRight = -1;
                continue;
            }
            if (currentRight >= currentLeft && x <= currentRight + 1) {
                currentRight = x;
            } else {
                flushRun();
                currentLeft = x;
                currentRight = x;
            }
        }
        flushRun();
    }

    painter.save();
    painter.setClipRect(plot, Qt::IntersectClip);
    const int verticalInset = rowHeight() >= 18 ? 2 : (rowHeight() >= 14 ? 1 : 0);
    const int lineHeight = std::max(1, rowHeight() - verticalInset * 2);
    painter.setPen(QPen(QColor(QStringLiteral("#C2C7CE")), lineHeight, Qt::SolidLine, Qt::SquareCap));
    for (const std::vector<QLine>& runs : rowRuns) {
        if (!runs.empty()) {
            painter.drawLines(runs.data(), static_cast<int>(runs.size()));
        }
    }
    painter.restore();
}

void TransactionWidget::paintSpanRectangles(QPainter& painter,
                                            const std::vector<int>& spanIndices,
                                            const QRect& plot,
                                            const qint64 visibleStart,
                                            const qint64 visibleEnd,
                                            const long double visibleRange,
                                            const bool includeInteractionState,
                                            const bool drawInlineLabels,
                                            const bool requireVisibleLane) const
{
    if (spanIndices.empty() || plot.width() <= 0 || plot.height() <= 0) {
        return;
    }

    QFont spanAddressFont = transactionSpanLabelFont(font(), rowHeight());
    spanAddressFont.setBold(false);
    QFont spanOpcodeFont = spanAddressFont;
    spanOpcodeFont.setBold(true);
    const QFontMetrics spanOpcodeMetrics(spanOpcodeFont);
    const QFontMetrics spanAddressMetrics(spanAddressFont);

    const auto drawSpanInlineLabel = [&](const TransactionSpan& span, const QRect& rect, const int horizontalPadding) {
        if (rect.width() <= horizontalPadding * 2) {
            return;
        }
        const int availableWidth = rect.width() - horizontalPadding * 2;
        const SpanInlineLabelParts labelParts = spanInlineLabelParts(span,
                                                                     availableWidth,
                                                                     spanOpcodeMetrics,
                                                                     spanAddressMetrics);
        if (labelParts.opcode.isEmpty()) {
            return;
        }

        painter.save();
        painter.setClipRect(rect.adjusted(1, 0, -1, 0), Qt::IntersectClip);
        painter.setPen(QColor(QStringLiteral("#1D2732")));

        const QRect labelRect = rect.adjusted(horizontalPadding, 0, -horizontalPadding, 0);
        painter.setFont(spanOpcodeFont);
        painter.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, labelParts.opcode);

        if (!labelParts.address.isEmpty()) {
            constexpr int kOpcodeAddressGap = 8;
            const int addressLeft =
                labelRect.left() + spanOpcodeMetrics.horizontalAdvance(labelParts.opcode) + kOpcodeAddressGap;
            painter.setFont(spanAddressFont);
            painter.drawText(QRect(addressLeft,
                                   labelRect.top(),
                                   std::max(0, labelRect.right() - addressLeft + 1),
                                   labelRect.height()),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             labelParts.address);
        }

        painter.restore();
    };

    for (const int spanIndex : spanIndices) {
        if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
            continue;
        }
        const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
        if (span.laneIndex < 0) {
            continue;
        }
        const bool filteredOut = shouldDrawFilteredOutSpan(spanIndex);
        const bool selected = includeInteractionState && spanIndex == selectedSpanIndex_;
        const bool hovered = includeInteractionState && spanIndex == hoveredSpanIndex_;
        const QRect barRect = spanVisualRectForRange(span,
                                                     plot,
                                                     visibleStart,
                                                     visibleEnd,
                                                     visibleRange,
                                                     requireVisibleLane);
        if (!barRect.isValid()) {
            continue;
        }
        const QColor edge = spanEdgeColor(span);
        const int verticalInset = rowHeight() >= 18 ? 2 : (rowHeight() >= 14 ? 1 : 0);
        const QRect spanRect = barRect.adjusted(0, verticalInset, -1, -verticalInset);
        if (spanRect.width() <= 0 || spanRect.height() <= 0) {
            continue;
        }
        if (filteredOut) {
            painter.setPen(QPen(QColor(QStringLiteral("#AEB4BC")), 1));
            painter.setBrush(QColor(QStringLiteral("#E1E4E8")));
            painter.drawRect(spanRect);
            continue;
        }
        const QColor outlineColor = selected ? QColor(QStringLiteral("#111111"))
                                  : (hovered ? QColor(QStringLiteral("#2B333B")) : edge);
        painter.setPen(QPen(outlineColor, selected ? 2 : 1));
        painter.setBrush(spanFillColor(span));
        painter.drawRect(spanRect);
        if (!span.completed) {
            painter.setPen(QPen(edge, 1, Qt::DashLine));
            painter.drawLine(spanRect.topRight(), spanRect.bottomRight());
        }
        if (drawInlineLabels) {
            drawSpanInlineLabel(span, spanRect, 3);
        }
    }
}

QColor TransactionWidget::laneAccentColor(const TransactionLane& lane) const
{
    return channelEdgeColor(channelKindFromOpcodeOrOrigin(lane.requestOpcode, lane.originKind));
}

QColor TransactionWidget::spanFillColor(const TransactionSpan& span) const
{
    return channelFillColor(channelKindFromOpcodeOrOrigin(span.requestOpcode, span.originKind), span.completed);
}

QColor TransactionWidget::spanEdgeColor(const TransactionSpan& span) const
{
    if (!span.completed) {
        return QColor(QStringLiteral("#75808A"));
    }
    return channelEdgeColor(channelKindFromOpcodeOrOrigin(span.requestOpcode, span.originKind));
}

void TransactionWidget::setArrangeMode(const ArrangeMode mode)
{
    if (arrangeMode_ == mode) {
        return;
    }
    stopFilterThread();
    arrangeMode_ = mode;
    const std::optional<std::uint64_t> selectedOrdinal =
        selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
            ? std::optional<std::uint64_t>(spans_[static_cast<std::size_t>(selectedSpanIndex_)].ordinal)
            : std::nullopt;

    if (!unfilteredSpans_.empty()) {
        arrangeSpans(unfilteredSpans_, unfilteredLanes_, arrangeMode_);
    }
    applyTransactionFilter();
    selectedSpanIndex_ = -1;
    if (selectedOrdinal) {
        const auto found = std::find_if(spans_.begin(), spans_.end(), [selectedOrdinal](const TransactionSpan& span) {
            return span.ordinal == *selectedOrdinal;
        });
        if (found != spans_.end()) {
            selectedSpanIndex_ = static_cast<int>(std::distance(spans_.begin(), found));
            ensureSpanVisible(selectedSpanIndex_);
        }
    }
    reconcileSelectedOpcodeTag();
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::rebuildArrangement()
{
    stopFilterThread();
    if (unfilteredSpans_.empty()) {
        arrangeSpans(spans_, lanes_, arrangeMode_);
        unfilteredSpans_ = spans_;
        unfilteredLanes_ = lanes_;
    } else {
        arrangeSpans(unfilteredSpans_, unfilteredLanes_, arrangeMode_);
    }
    applyTransactionFilter();
    verticalScrollOffset_ = std::clamp(verticalScrollOffset_,
                                      0,
                                      std::max(0, static_cast<int>(lanes_.size()) - visibleLaneCapacity()));
}

bool TransactionWidget::transactionFilterActive() const noexcept
{
    return transactionFilterActive(filterCriteria_);
}

bool TransactionWidget::requestedTransactionFilterActive() const noexcept
{
    return transactionFilterActive(requestedFilterCriteria_);
}

bool TransactionWidget::transactionFilterActive(const TransactionFilterCriteria& criteria) noexcept
{
    return !criteria.opcode.isEmpty()
        || numericFilterActive(criteria.address)
        || numericFilterActive(criteria.txnId)
        || numericFilterActive(criteria.dbid);
}

bool TransactionWidget::transactionFilterMatchesSpan(const TransactionSpan& span) const
{
    return transactionFilterMatchesSpan(span, filterCriteria_);
}

bool TransactionWidget::transactionFilterMatchesSpan(const TransactionSpan& span,
                                                     const TransactionFilterCriteria& criteria)
{
    if (!transactionFilterActive(criteria)) {
        return true;
    }

    const QString addressFilter = normalizedNumericFilter(criteria.address);
    const QString txnIdFilter = normalizedNumericFilter(criteria.txnId);
    const QString dbidFilter = normalizedNumericFilter(criteria.dbid);
    const QString opcode = opcodeWithoutChannelPrefix(span.requestOpcode).trimmed().isEmpty()
        ? span.requestOpcode.trimmed()
        : opcodeWithoutChannelPrefix(span.requestOpcode).trimmed();
    if (!matchesTransactionFilterValue(opcode, criteria.opcode)
        && !matchesTransactionFilterValue(span.requestOpcode, criteria.opcode)
        && !matchesAnyTransactionFilterValue(QString(), span.opcodeValues, criteria.opcode)) {
        return false;
    }
    if (!matchesTransactionFilterValue(span.addressLabel, addressFilter)) {
        return false;
    }
    if (!matchesAnyTransactionFilterValue(span.txnIdLabel, span.txnIdValues, txnIdFilter)) {
        return false;
    }
    if (!matchesAnyTransactionFilterValue(span.dbidLabel, span.dbidValues, dbidFilter)) {
        return false;
    }
    return true;
}

bool TransactionWidget::displayedSpanMatchesFilter(const int spanIndex) const noexcept
{
    if (!transactionFilterActive()) {
        return true;
    }
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spanFilterMatches_.size())) {
        return false;
    }
    return spanFilterMatches_[static_cast<std::size_t>(spanIndex)] != 0;
}

bool TransactionWidget::shouldDrawFilteredOutSpan(const int spanIndex) const noexcept
{
    return transactionFilterActive()
        && filterMode_ == TransactionFilterMode::Highlight
        && !displayedSpanMatchesFilter(spanIndex);
}

void TransactionWidget::syncFilterControls()
{
    synchronizingFilterControls_ = true;
    if (filterModeCombo_) {
        const QString modeName = filterModeName(requestedFilterMode_);
        const int index = filterModeCombo_->findData(modeName);
        filterModeCombo_->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (opcodeFilterEdit_) {
        opcodeFilterEdit_->setText(requestedFilterCriteria_.opcode);
    }
    if (addressFilterEdit_) {
        addressFilterEdit_->setText(requestedFilterCriteria_.address);
    }
    if (txnIdFilterEdit_) {
        txnIdFilterEdit_->setText(requestedFilterCriteria_.txnId);
    }
    if (dbidFilterEdit_) {
        dbidFilterEdit_->setText(requestedFilterCriteria_.dbid);
    }
    if (opcodeTagsCheckBox_) {
        opcodeTagsCheckBox_->setChecked(showOpcodeTags_);
    }
    if (hoverCardsCheckBox_) {
        hoverCardsCheckBox_->setChecked(showHoverCards_);
    }
    synchronizingFilterControls_ = false;
}

void TransactionWidget::setFilterMode(const TransactionFilterMode mode)
{
    if (requestedFilterMode_ == mode) {
        return;
    }
    setFilterState(mode, requestedFilterCriteria_);
}

void TransactionWidget::setFilterCriteria(TransactionFilterCriteria criteria)
{
    criteria.opcode = criteria.opcode.trimmed();
    criteria.address = criteria.address.trimmed();
    criteria.txnId = criteria.txnId.trimmed();
    criteria.dbid = criteria.dbid.trimmed();
    if (requestedFilterCriteria_.opcode == criteria.opcode
        && requestedFilterCriteria_.address == criteria.address
        && requestedFilterCriteria_.txnId == criteria.txnId
        && requestedFilterCriteria_.dbid == criteria.dbid) {
        return;
    }

    setFilterState(requestedFilterMode_, std::move(criteria));
}

void TransactionWidget::setFilterState(const TransactionFilterMode mode, TransactionFilterCriteria criteria)
{
    criteria.opcode = criteria.opcode.trimmed();
    criteria.address = criteria.address.trimmed();
    criteria.txnId = criteria.txnId.trimmed();
    criteria.dbid = criteria.dbid.trimmed();
    if (requestedFilterMode_ == mode
        && requestedFilterCriteria_.opcode == criteria.opcode
        && requestedFilterCriteria_.address == criteria.address
        && requestedFilterCriteria_.txnId == criteria.txnId
        && requestedFilterCriteria_.dbid == criteria.dbid) {
        return;
    }

    requestedFilterMode_ = mode;
    requestedFilterCriteria_ = std::move(criteria);
    applyTransactionFilter();
    syncFilterControls();
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

QString TransactionWidget::filterValueForSpanField(const TransactionSpan& span,
                                                   const TransactionFilterField field) const
{
    switch (field) {
    case TransactionFilterField::Opcode: {
        const QString opcode = opcodeWithoutChannelPrefix(span.requestOpcode).trimmed();
        return opcode.isEmpty() ? span.requestOpcode.trimmed() : opcode;
    }
    case TransactionFilterField::Address:
        return span.addressLabel.trimmed();
    case TransactionFilterField::TxnId:
        return span.txnIdLabel.trimmed().isEmpty()
            && !span.txnIdValues.empty()
            ? span.txnIdValues.front().trimmed()
            : span.txnIdLabel.trimmed();
    case TransactionFilterField::Dbid:
        return span.dbidLabel.trimmed().isEmpty()
            && !span.dbidValues.empty()
            ? span.dbidValues.front().trimmed()
            : span.dbidLabel.trimmed();
    }
    return {};
}

bool TransactionWidget::applySpanFilterAction(const int spanIndex,
                                              const TransactionFilterMode mode,
                                              const TransactionFilterField field)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return false;
    }

    const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    const QString value = filterValueForSpanField(span, field);
    if (value.isEmpty()) {
        return false;
    }

    TransactionFilterCriteria criteria;
    switch (field) {
    case TransactionFilterField::Opcode:
        criteria.opcode = value;
        break;
    case TransactionFilterField::Address:
        criteria.address = value;
        break;
    case TransactionFilterField::TxnId:
        criteria.txnId = value;
        break;
    case TransactionFilterField::Dbid:
        criteria.dbid = value;
        break;
    }
    setFilterState(mode, std::move(criteria));
    return true;
}

void TransactionWidget::clearTransactionFilter()
{
    setFilterState(requestedFilterMode_, TransactionFilterCriteria{});
}

void TransactionWidget::applyTransactionFilter()
{
    if (filtering_) {
        startTransactionFilter();
        return;
    }

    const bool shouldRunAsync =
        requestedTransactionFilterActive() || transactionFilterActive() || displayingFilteredSpans_;
    if (shouldRunAsync
        && unfilteredSpans_.size() >= kTransactionFilterAsyncSpanThreshold) {
        startTransactionFilter();
        return;
    }
    applyTransactionFilterSynchronously();
}

void TransactionWidget::applyTransactionFilterSynchronously()
{
    stopFilterThread();
    ++filterRevision_;
    invalidateDenseSpanPaintCache();
    filterMode_ = requestedFilterMode_;
    filterCriteria_ = requestedFilterCriteria_;
    const bool active = transactionFilterActive();

    if (unfilteredSpans_.empty() && !spans_.empty()) {
        unfilteredSpans_ = spans_;
        unfilteredLanes_ = lanes_;
    }

    if (unfilteredSpans_.empty()) {
        unfilteredSpanFilterMatches_.clear();
        spanFilterMatches_.assign(spans_.size(), 1);
        filterMatchCount_ = active ? 0 : static_cast<std::uint64_t>(spans_.size());
        displayingFilteredSpans_ = false;
        rebuildSpanPaintIndex();
        return;
    }

    unfilteredSpanFilterMatches_.assign(unfilteredSpans_.size(), 0);
    filterMatchCount_ = 0;
    for (std::size_t index = 0; index < unfilteredSpans_.size(); ++index) {
        const bool match = !active || transactionFilterMatchesSpan(unfilteredSpans_[index]);
        unfilteredSpanFilterMatches_[index] = match ? 1U : 0U;
        if (match) {
            ++filterMatchCount_;
        }
    }

    if (!active || filterMode_ == TransactionFilterMode::Highlight) {
        lanes_ = unfilteredLanes_;
        spans_ = unfilteredSpans_;
        spanFilterMatches_ = unfilteredSpanFilterMatches_;
        displayingFilteredSpans_ = false;
    } else {
        spans_.clear();
        spans_.reserve(static_cast<std::size_t>(filterMatchCount_));
        for (std::size_t index = 0; index < unfilteredSpans_.size(); ++index) {
            if (unfilteredSpanFilterMatches_[index] != 0) {
                spans_.push_back(unfilteredSpans_[index]);
            }
        }
        lanes_.clear();
        arrangeSpans(spans_, lanes_, arrangeMode_);
        spanFilterMatches_.assign(spans_.size(), 1);
        displayingFilteredSpans_ = true;
    }

    selectedSpanIndex_ = -1;
    if (selectedLogicalRow_ >= 0 && !spans_.empty()) {
        selectedSpanIndex_ = spanIndexForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
    }
    reconcileSelectedOpcodeTag();
    if (markerSpanIndex_ >= static_cast<int>(spans_.size())) {
        markerSpanIndex_ = -1;
    }
    hoveredSpanIndex_ = -1;
    verticalScrollOffset_ = std::clamp(verticalScrollOffset_,
                                      0,
                                      std::max(0, static_cast<int>(lanes_.size()) - visibleLaneCapacity()));
    rebuildSpanPaintIndex();
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::startTransactionFilter()
{
    if (filtering_) {
        filterRestartPending_ = true;
        filterThread_.request_stop();
        statusText_ = filterProgressText();
        updateWidgetHints();
        update();
        return;
    }

    if (unfilteredSpans_.empty() && !spans_.empty()) {
        unfilteredSpans_ = spans_;
        unfilteredLanes_ = lanes_;
    }

    const quint64 generation = ++filterGeneration_;
    ++filterRevision_;
    invalidateDenseSpanPaintCache();
    if (unfilteredSpans_.empty()) {
        filterMode_ = requestedFilterMode_;
        filterCriteria_ = requestedFilterCriteria_;
        unfilteredSpanFilterMatches_.clear();
        spanFilterMatches_.assign(spans_.size(), 1);
        filterMatchCount_ = requestedTransactionFilterActive() ? 0 : static_cast<std::uint64_t>(spans_.size());
        displayingFilteredSpans_ = false;
        rebuildSpanPaintIndex();
        updateStatusText();
        updateWidgetHints();
        updateScrollBars();
        update();
        return;
    }

    const quint64 buildGeneration = buildGeneration_.load(std::memory_order_relaxed);
    const TransactionFilterMode mode = requestedFilterMode_;
    const TransactionFilterCriteria criteria = requestedFilterCriteria_;
    const ArrangeMode arrangeMode = arrangeMode_;
    filtering_ = true;
    filterRestartPending_ = false;
    filterProgressCompleted_ = 0;
    filterProgressTotal_ = static_cast<std::uint64_t>(unfilteredSpans_.size());
    statusText_ = filterProgressText();
    updateWidgetHints();
    update();

    filterThread_ = std::jthread([this,
                                  generation,
                                  buildGeneration,
                                  mode,
                                  criteria,
                                  arrangeMode](const std::stop_token stopToken) {
        std::shared_ptr<FilterResult> result =
            buildTransactionFilterResult(generation,
                                         buildGeneration,
                                         mode,
                                         criteria,
                                         arrangeMode,
                                         stopToken,
                                         true);
        QTimer::singleShot(0, this, [this, result = std::move(result), generation]() {
            if (generation == filterGeneration_.load(std::memory_order_relaxed)) {
                applyTransactionFilterResult(result);
            }
        });
    });
}

std::shared_ptr<TransactionWidget::FilterResult> TransactionWidget::buildTransactionFilterResult(
    const quint64 generation,
    const quint64 buildGeneration,
    const TransactionFilterMode mode,
    TransactionFilterCriteria criteria,
    const ArrangeMode arrangeMode,
    const std::stop_token stopToken,
    const bool publishProgress) const
{
    auto result = std::make_shared<FilterResult>();
    result->generation = generation;
    result->buildGeneration = buildGeneration;
    result->mode = mode;
    result->criteria = std::move(criteria);

    if (buildGeneration != buildGeneration_.load(std::memory_order_relaxed)) {
        result->cancelled = true;
        return result;
    }

    const bool active = transactionFilterActive(result->criteria);
    const std::size_t spanCount = unfilteredSpans_.size();
    result->unfilteredMatches.assign(spanCount, 0);
    const QString opcodeFilter = result->criteria.opcode;
    const QString addressFilter = normalizedNumericFilter(result->criteria.address);
    const QString txnIdFilter = normalizedNumericFilter(result->criteria.txnId);
    const QString dbidFilter = normalizedNumericFilter(result->criteria.dbid);
    const auto matchesSpan = [&](const TransactionSpan& span) {
        if (!active) {
            return true;
        }
        const QString opcodeWithoutPrefix = opcodeWithoutChannelPrefix(span.requestOpcode).trimmed();
        const QString opcode = opcodeWithoutPrefix.isEmpty() ? span.requestOpcode.trimmed() : opcodeWithoutPrefix;
        if (!matchesTransactionFilterValue(opcode, opcodeFilter)
            && !matchesTransactionFilterValue(span.requestOpcode, opcodeFilter)
            && !matchesAnyTransactionFilterValue(QString(), span.opcodeValues, opcodeFilter)) {
            return false;
        }
        if (!matchesTransactionFilterValue(span.addressLabel, addressFilter)) {
            return false;
        }
        if (!matchesAnyTransactionFilterValue(span.txnIdLabel, span.txnIdValues, txnIdFilter)) {
            return false;
        }
        if (!matchesAnyTransactionFilterValue(span.dbidLabel, span.dbidValues, dbidFilter)) {
            return false;
        }
        return true;
    };

    TransactionWidget* widget = const_cast<TransactionWidget*>(this);
    std::atomic<std::int64_t> lastProgressPublishTimeMs = 0;
    const auto publish = [&](const std::uint64_t completedWork,
                             const std::uint64_t totalWork,
                             const bool force = false) {
        if (!publishProgress
            || stopToken.stop_requested()
            || generation != filterGeneration_.load(std::memory_order_relaxed)
            || buildGeneration != buildGeneration_.load(std::memory_order_relaxed)) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        std::int64_t previousMs = lastProgressPublishTimeMs.load(std::memory_order_relaxed);
        if (!force && nowMs - previousMs < kBuildProgressPublishInterval.count()) {
            return;
        }
        if (!force
            && !lastProgressPublishTimeMs.compare_exchange_strong(previousMs,
                                                                  nowMs,
                                                                  std::memory_order_relaxed,
                                                                  std::memory_order_relaxed)) {
            return;
        }
        if (force) {
            lastProgressPublishTimeMs.store(nowMs, std::memory_order_relaxed);
        }
        QTimer::singleShot(0, widget, [widget, generation, completedWork, totalWork]() {
            widget->updateFilterProgress(generation, completedWork, totalWork);
        });
    };

    publish(0, spanCount, true);
    std::atomic<std::size_t> nextChunkBegin = 0;
    std::atomic<std::uint64_t> completedSpans = 0;
    std::atomic<std::uint64_t> matchCount = 0;
    const std::size_t hardwareWorkers = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    const std::size_t chunkCount =
        spanCount == 0 ? 1 : (spanCount + kTransactionFilterChunkSize - 1) / kTransactionFilterChunkSize;
    const std::size_t workerCount = std::min<std::size_t>(
        kTransactionFilterMaxWorkers,
        std::min<std::size_t>(hardwareWorkers, std::max<std::size_t>(1, chunkCount)));

    const auto workerBody = [&]() {
        while (!stopToken.stop_requested()
               && generation == filterGeneration_.load(std::memory_order_relaxed)
               && buildGeneration == buildGeneration_.load(std::memory_order_relaxed)) {
            const std::size_t begin = nextChunkBegin.fetch_add(kTransactionFilterChunkSize,
                                                               std::memory_order_relaxed);
            if (begin >= spanCount) {
                return;
            }
            const std::size_t end = std::min(spanCount, begin + kTransactionFilterChunkSize);
            std::uint64_t localMatches = 0;
            for (std::size_t index = begin; index < end; ++index) {
                const bool match = matchesSpan(unfilteredSpans_[index]);
                result->unfilteredMatches[index] = match ? 1U : 0U;
                if (match) {
                    ++localMatches;
                }
            }
            matchCount.fetch_add(localMatches, std::memory_order_relaxed);
            const std::uint64_t completed =
                completedSpans.fetch_add(static_cast<std::uint64_t>(end - begin), std::memory_order_relaxed)
                + static_cast<std::uint64_t>(end - begin);
            publish(std::min<std::uint64_t>(completed, spanCount), spanCount);
        }
    };

    if (workerCount <= 1) {
        workerBody();
    } else {
        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
            workers.emplace_back(workerBody);
        }
    }

    if (stopToken.stop_requested()
        || generation != filterGeneration_.load(std::memory_order_relaxed)
        || buildGeneration != buildGeneration_.load(std::memory_order_relaxed)) {
        result->cancelled = true;
        return result;
    }

    result->matchCount = matchCount.load(std::memory_order_relaxed);
    publish(spanCount, spanCount, true);

    if (!active || mode == TransactionFilterMode::Highlight) {
        result->lanes = unfilteredLanes_;
        result->spans = unfilteredSpans_;
        result->displayMatches = result->unfilteredMatches;
        result->displayingFilteredSpans = false;
    } else {
        result->spans.reserve(static_cast<std::size_t>(result->matchCount));
        for (std::size_t index = 0; index < spanCount; ++index) {
            if (result->unfilteredMatches[index] != 0) {
                result->spans.push_back(unfilteredSpans_[index]);
            }
        }
        result->lanes.clear();
        arrangeSpans(result->spans,
                     result->lanes,
                     arrangeMode,
                     [&](const std::uint64_t completedWork, const std::uint64_t totalWork) {
                         publish(static_cast<std::uint64_t>(spanCount) + completedWork,
                                 static_cast<std::uint64_t>(spanCount) + totalWork);
                     });
        result->displayMatches.assign(result->spans.size(), 1);
        result->displayingFilteredSpans = true;
    }

    if (stopToken.stop_requested()
        || generation != filterGeneration_.load(std::memory_order_relaxed)
        || buildGeneration != buildGeneration_.load(std::memory_order_relaxed)) {
        result->cancelled = true;
        return result;
    }

    result->paintIndex = buildSpanPaintIndex(result->spans, result->lanes.size());
    return result;
}

void TransactionWidget::applyTransactionFilterResult(std::shared_ptr<FilterResult> result)
{
    if (!result
        || result->generation != filterGeneration_.load(std::memory_order_relaxed)
        || result->buildGeneration != buildGeneration_.load(std::memory_order_relaxed)) {
        return;
    }

    const bool requestedChanged =
        result->mode != requestedFilterMode_
        || result->criteria.opcode != requestedFilterCriteria_.opcode
        || result->criteria.address != requestedFilterCriteria_.address
        || result->criteria.txnId != requestedFilterCriteria_.txnId
        || result->criteria.dbid != requestedFilterCriteria_.dbid;

    if (result->cancelled || filterRestartPending_ || requestedChanged) {
        filtering_ = false;
        resetFilterProgress();
        const bool restart = filterRestartPending_ || requestedChanged;
        filterRestartPending_ = false;
        ++filterGeneration_;
        if (restart) {
            startTransactionFilter();
        } else {
            updateStatusText();
            updateWidgetHints();
            update();
        }
        return;
    }

    filtering_ = false;
    filterRestartPending_ = false;
    resetFilterProgress();
    ++filterRevision_;
    invalidateDenseSpanPaintCache();
    filterMode_ = result->mode;
    filterCriteria_ = result->criteria;
    unfilteredSpanFilterMatches_ = std::move(result->unfilteredMatches);
    spanFilterMatches_ = std::move(result->displayMatches);
    lanes_ = std::move(result->lanes);
    spans_ = std::move(result->spans);
    spanIndicesByLane_ = std::move(result->paintIndex.spanIndicesByLane);
    spanIndicesByLaneEnd_ = std::move(result->paintIndex.spanIndicesByLaneEnd);
    fullTimestampStart_ = result->paintIndex.fullTimestampStart;
    fullTimestampEnd_ = result->paintIndex.fullTimestampEnd;
    filterMatchCount_ = result->matchCount;
    displayingFilteredSpans_ = result->displayingFilteredSpans;

    selectedSpanIndex_ = -1;
    if (selectedLogicalRow_ >= 0 && !spans_.empty()) {
        selectedSpanIndex_ = spanIndexForLogicalRow(static_cast<std::uint64_t>(selectedLogicalRow_));
    }
    reconcileSelectedOpcodeTag();
    if (markerSpanIndex_ >= static_cast<int>(spans_.size())) {
        markerSpanIndex_ = -1;
    }
    hoveredSpanIndex_ = -1;
    verticalScrollOffset_ = std::clamp(verticalScrollOffset_,
                                      0,
                                      std::max(0, static_cast<int>(lanes_.size()) - visibleLaneCapacity()));
    updateStatusText();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::resetFilterProgress()
{
    filterProgressCompleted_ = 0;
    filterProgressTotal_ = 0;
}

void TransactionWidget::arrangeSpans(
    std::vector<TransactionSpan>& spans,
    std::vector<TransactionLane>& lanes,
    const ArrangeMode arrangeMode,
    const std::function<void(std::uint64_t completedWork,
                             std::uint64_t totalWork)>& progressCallback) const
{
    if (progressCallback) {
        progressCallback(0, spans.size());
    }

    std::vector<QString> groupOrder;
    QHash<QString, std::vector<int>> spansByGroup;
    for (int spanIndex = 0; spanIndex < static_cast<int>(spans.size()); ++spanIndex) {
        TransactionSpan& span = spans[static_cast<std::size_t>(spanIndex)];
        span.laneIndex = -1;
        span.groupIndex = -1;
        span.stackSlot = 0;
        span.stackDepth = 1;
        span.laneKey.clear();

        const QString groupKey = groupKeyFor(span.jointFamily, span.endpointLabel, span.originKind);
        if (!spansByGroup.contains(groupKey)) {
            groupOrder.push_back(groupKey);
        }
        spansByGroup[groupKey].push_back(spanIndex);
    }

    std::sort(groupOrder.begin(), groupOrder.end(), [&spansByGroup, &spans](const QString& lhsKey,
                                                                             const QString& rhsKey) {
        const TransactionSpan& lhs =
            spans[static_cast<std::size_t>(spansByGroup.value(lhsKey).front())];
        const TransactionSpan& rhs =
            spans[static_cast<std::size_t>(spansByGroup.value(rhsKey).front())];
        const int lhsJoint = jointSortRank(lhs.jointFamily);
        const int rhsJoint = jointSortRank(rhs.jointFamily);
        if (lhsJoint != rhsJoint) {
            return lhsJoint < rhsJoint;
        }
        if (lhs.endpointNode != rhs.endpointNode) {
            return numericLabelLess(lhs.endpointNode, rhs.endpointNode);
        }
        const int lhsOrigin = originSortRank(lhs.originKind);
        const int rhsOrigin = originSortRank(rhs.originKind);
        if (lhsOrigin != rhsOrigin) {
            return lhsOrigin < rhsOrigin;
        }
        return lhs.endpointLabel < rhs.endpointLabel;
    });

    lanes.clear();
    int groupIndex = 0;
    std::uint64_t layoutProgress = 0;
    for (const QString& groupKey : groupOrder) {
        std::vector<int> groupSpans = spansByGroup.value(groupKey);
        if (groupSpans.empty()) {
            continue;
        }
        std::sort(groupSpans.begin(), groupSpans.end(), [&spans](const int lhsIndex, const int rhsIndex) {
            const TransactionSpan& lhs = spans[static_cast<std::size_t>(lhsIndex)];
            const TransactionSpan& rhs = spans[static_cast<std::size_t>(rhsIndex)];
            if (lhs.startTimestamp != rhs.startTimestamp) {
                return lhs.startTimestamp < rhs.startTimestamp;
            }
            if (lhs.endTimestamp != rhs.endTimestamp) {
                return lhs.endTimestamp < rhs.endTimestamp;
            }
            if (lhs.firstLogicalRow != rhs.firstLogicalRow) {
                return lhs.firstLogicalRow < rhs.firstLogicalRow;
            }
            return lhs.ordinal < rhs.ordinal;
        });

        const TransactionSpan& firstSpan = spans[static_cast<std::size_t>(groupSpans.front())];
        const QString groupLabel = QStringLiteral("%1 / %2").arg(firstSpan.endpointLabel, firstSpan.originKind);

        if (arrangeMode == ArrangeMode::TxnId) {
            std::vector<QString> txnOrder;
            QHash<QString, std::vector<int>> spansByTxnId;
            for (const int spanIndex : groupSpans) {
                const TransactionSpan& span = spans[static_cast<std::size_t>(spanIndex)];
                const QString txnId = span.txnIdLabel.trimmed().isEmpty()
                    ? QStringLiteral("unknown")
                    : span.txnIdLabel.trimmed();
                if (!spansByTxnId.contains(txnId)) {
                    txnOrder.push_back(txnId);
                }
                spansByTxnId[txnId].push_back(spanIndex);
            }
            std::sort(txnOrder.begin(), txnOrder.end(), numericLabelLess);

            const int stackDepth = std::max(1, static_cast<int>(txnOrder.size()));
            for (int slot = 0; slot < static_cast<int>(txnOrder.size()); ++slot) {
                const QString txnId = txnOrder[static_cast<std::size_t>(slot)];
                std::vector<int> txnSpans = spansByTxnId.value(txnId);
                std::sort(txnSpans.begin(), txnSpans.end(), [&spans](const int lhsIndex, const int rhsIndex) {
                    const TransactionSpan& lhs = spans[static_cast<std::size_t>(lhsIndex)];
                    const TransactionSpan& rhs = spans[static_cast<std::size_t>(rhsIndex)];
                    if (lhs.startTimestamp != rhs.startTimestamp) {
                        return lhs.startTimestamp < rhs.startTimestamp;
                    }
                    if (lhs.firstLogicalRow != rhs.firstLogicalRow) {
                        return lhs.firstLogicalRow < rhs.firstLogicalRow;
                    }
                    return lhs.ordinal < rhs.ordinal;
                });

                TransactionLane lane;
                lane.key = txnIdLaneKeyFor(firstSpan.jointFamily, firstSpan.endpointLabel, firstSpan.originKind, txnId);
                lane.groupKey = groupKey;
                lane.groupLabel = groupLabel;
                lane.label = txnId == QLatin1String("unknown")
                    ? QStringLiteral("TxnID unknown")
                    : QStringLiteral("TxnID %1").arg(txnId);
                lane.jointFamily = firstSpan.jointFamily;
                lane.endpointLabel = firstSpan.endpointLabel;
                lane.endpointNode = firstSpan.endpointNode;
                lane.endpointNodeType = firstSpan.endpointNodeType;
                lane.originKind = firstSpan.originKind;
                lane.xactionType = firstSpan.xactionType;
                lane.requestOpcode = firstSpan.requestOpcode;
                lane.txnIdLabel = txnId;
                lane.groupIndex = groupIndex;
                lane.stackSlot = slot;
                lane.stackDepth = stackDepth;
                lane.groupSpanCount = static_cast<std::uint64_t>(groupSpans.size());
                lane.spanCount = static_cast<std::uint64_t>(txnSpans.size());

                const int laneIndex = static_cast<int>(lanes.size());
                for (const int spanIndex : txnSpans) {
                    TransactionSpan& span = spans[static_cast<std::size_t>(spanIndex)];
                    span.groupIndex = groupIndex;
                    span.stackSlot = slot;
                    span.stackDepth = stackDepth;
                    span.laneKey = lane.key;
                    span.laneIndex = laneIndex;
                    ++layoutProgress;
                    if (progressCallback) {
                        progressCallback(std::min<std::uint64_t>(layoutProgress, spans.size()), spans.size());
                    }
                }
                lanes.push_back(std::move(lane));
            }
            ++groupIndex;
            continue;
        }

        struct ActiveSlot {
            qint64 endTimestamp = 0;
            int slot = 0;
        };
        struct ActiveSlotLater {
            bool operator()(const ActiveSlot& lhs, const ActiveSlot& rhs) const noexcept
            {
                if (lhs.endTimestamp != rhs.endTimestamp) {
                    return lhs.endTimestamp > rhs.endTimestamp;
                }
                return lhs.slot > rhs.slot;
            }
        };

        std::vector<qint64> slotEndTimestamps;
        std::vector<std::uint64_t> slotSpanCounts;
        std::priority_queue<ActiveSlot, std::vector<ActiveSlot>, ActiveSlotLater> activeSlots;
        std::priority_queue<int, std::vector<int>, std::greater<int>> availableSlots;
        for (const int spanIndex : groupSpans) {
            TransactionSpan& span = spans[static_cast<std::size_t>(spanIndex)];
            while (!activeSlots.empty() && activeSlots.top().endTimestamp < span.startTimestamp) {
                availableSlots.push(activeSlots.top().slot);
                activeSlots.pop();
            }
            const int slot = availableSlots.empty()
                ? static_cast<int>(slotEndTimestamps.size())
                : availableSlots.top();
            if (!availableSlots.empty()) {
                availableSlots.pop();
            }
            if (slot == static_cast<int>(slotEndTimestamps.size())) {
                slotEndTimestamps.push_back(span.endTimestamp);
                slotSpanCounts.push_back(0);
            } else {
                slotEndTimestamps[static_cast<std::size_t>(slot)] = span.endTimestamp;
            }
            activeSlots.push(ActiveSlot{span.endTimestamp, slot});
            ++slotSpanCounts[static_cast<std::size_t>(slot)];
            span.groupIndex = groupIndex;
            span.stackSlot = slot;
            ++layoutProgress;
            if (progressCallback) {
                progressCallback(std::min<std::uint64_t>(layoutProgress, spans.size()), spans.size());
            }
        }

        const int stackDepth = std::max(1, static_cast<int>(slotEndTimestamps.size()));
        for (const int spanIndex : groupSpans) {
            TransactionSpan& span = spans[static_cast<std::size_t>(spanIndex)];
            span.stackDepth = stackDepth;
            span.laneKey = laneKeyFor(span.jointFamily, span.endpointLabel, span.originKind, span.stackSlot);
            span.laneIndex = static_cast<int>(lanes.size()) + span.stackSlot;
        }

        for (int slot = 0; slot < stackDepth; ++slot) {
            TransactionLane lane;
            lane.key = laneKeyFor(firstSpan.jointFamily, firstSpan.endpointLabel, firstSpan.originKind, slot);
            lane.groupKey = groupKey;
            lane.groupLabel = groupLabel;
            lane.jointFamily = firstSpan.jointFamily;
            lane.endpointLabel = firstSpan.endpointLabel;
            lane.endpointNode = firstSpan.endpointNode;
            lane.endpointNodeType = firstSpan.endpointNodeType;
            lane.originKind = firstSpan.originKind;
            lane.xactionType = firstSpan.xactionType;
            lane.requestOpcode = firstSpan.requestOpcode;
            lane.groupIndex = groupIndex;
            lane.stackSlot = slot;
            lane.stackDepth = stackDepth;
            lane.groupSpanCount = static_cast<std::uint64_t>(groupSpans.size());
            lane.label = slot == 0
                ? QStringLiteral("%1 / %2").arg(lane.endpointLabel, lane.originKind)
                : QStringLiteral("slot %1").arg(slot + 1);
            lane.spanCount = slot < static_cast<int>(slotSpanCounts.size())
                ? slotSpanCounts[static_cast<std::size_t>(slot)]
                : 0;
            lanes.push_back(std::move(lane));
        }
        ++groupIndex;
    }

    if (progressCallback) {
        progressCallback(spans.size(), spans.size());
    }
}

QString TransactionWidget::spanInlineLabel(const TransactionSpan& span,
                                           const int availableWidth,
                                           const QFontMetrics& opcodeMetrics,
                                           const QFontMetrics& addressMetrics) const
{
    const SpanInlineLabelParts parts = spanInlineLabelParts(span,
                                                            availableWidth,
                                                            opcodeMetrics,
                                                            addressMetrics);
    if (parts.opcode.isEmpty()) {
        return {};
    }
    if (parts.address.isEmpty()) {
        return parts.opcode;
    }
    return parts.opcode + QStringLiteral("  ") + parts.address;
}

TransactionWidget::SpanInlineLabelParts TransactionWidget::spanInlineLabelParts(
    const TransactionSpan& span,
    const int availableWidth,
    const QFontMetrics& opcodeMetrics,
    const QFontMetrics& addressMetrics) const
{
    if (availableWidth <= 0) {
        return {};
    }

    const QString displayOpcode = opcodeWithoutChannelPrefix(span.requestOpcode);
    const QString opcode = displayOpcode.isEmpty()
        ? QStringLiteral("Opcode unknown")
        : displayOpcode;
    const QString address = span.addressLabel.trimmed().isEmpty()
        ? QStringLiteral("Address unknown")
        : span.addressLabel.trimmed();

    const int opcodeWidth = opcodeMetrics.horizontalAdvance(opcode);
    if (opcodeWidth > availableWidth) {
        const QString shortenedOpcode = elidedOpcodeLabel(opcode, opcodeMetrics, availableWidth);
        return shortenedOpcode.isEmpty()
            ? SpanInlineLabelParts{}
            : SpanInlineLabelParts{shortenedOpcode, QString()};
    }

    constexpr int kOpcodeAddressGap = 8;
    const int bothWidth = opcodeWidth
        + kOpcodeAddressGap
        + addressMetrics.horizontalAdvance(address);
    if (bothWidth <= availableWidth) {
        return SpanInlineLabelParts{opcode, address};
    }

    return SpanInlineLabelParts{opcode, QString()};
}

QString TransactionWidget::spanStatusText(const TransactionSpan& span) const
{
    const QString displayOpcode = opcodeWithoutChannelPrefix(span.requestOpcode);
    const QString opcode = displayOpcode.isEmpty()
        ? QStringLiteral("Opcode unknown")
        : displayOpcode;
    const QString address = span.addressLabel.trimmed().isEmpty()
        ? QStringLiteral("Address unknown")
        : span.addressLabel.trimmed();
    const QString txnId = span.txnIdLabel.trimmed().isEmpty()
        ? QStringLiteral("TxnID unknown")
        : QStringLiteral("TxnID %1").arg(span.txnIdLabel.trimmed());
    const QString dbid = span.dbidLabel.trimmed().isEmpty()
        ? QStringLiteral("DBID unknown")
        : QStringLiteral("DBID %1").arg(span.dbidLabel.trimmed());
    return QStringLiteral("%1 / %2  |  %3  |  %4  |  %5  |  %6  |  %7 rows  |  %8 -> %9")
        .arg(span.endpointLabel,
             span.originKind,
             txnId,
             dbid,
             opcode,
             address,
             FormatUnsignedIntegerWithThousands(span.rowCount),
             FormatTimestampForDisplay(span.startTimestamp),
             FormatTimestampForDisplay(span.endTimestamp));
}

qint64 TransactionWidget::timestampForLogicalRowInSpan(const TransactionSpan& span,
                                                       const std::uint64_t logicalRow) const
{
    if (logicalRow <= span.firstLogicalRow) {
        return span.startTimestamp;
    }
    if (logicalRow >= span.lastLogicalRow) {
        return span.endTimestamp;
    }
    if (span.lastLogicalRow == span.firstLogicalRow) {
        return span.startTimestamp;
    }

    const long double fraction =
        static_cast<long double>(logicalRow - span.firstLogicalRow)
        / static_cast<long double>(span.lastLogicalRow - span.firstLogicalRow);
    return clampedRoundToTimestamp(static_cast<long double>(span.startTimestamp)
                                   + (static_cast<long double>(span.endTimestamp) - span.startTimestamp) * fraction);
}

bool TransactionWidget::spanHasCachedLogicalRow(const TransactionSpan& span,
                                                const std::uint64_t logicalRow) const
{
    if (span.requestLogicalRow == logicalRow
        || span.firstLogicalRow == logicalRow
        || span.lastLogicalRow == logicalRow
        || (span.completionLogicalRow && *span.completionLogicalRow == logicalRow)) {
        return true;
    }
    return std::find(span.logicalRows.begin(), span.logicalRows.end(), logicalRow) != span.logicalRows.end();
}

int TransactionWidget::spanIndexForLogicalRow(const std::uint64_t logicalRow) const
{
    int rangeMatch = -1;
    std::vector<int> rangeCandidates;
    rangeCandidates.reserve(8);
    for (int index = 0; index < static_cast<int>(spans_.size()); ++index) {
        const TransactionSpan& span = spans_[static_cast<std::size_t>(index)];
        if (shouldDrawFilteredOutSpan(index)) {
            continue;
        }
        if (spanHasCachedLogicalRow(span, logicalRow)) {
            return index;
        }
        if (logicalRow >= span.firstLogicalRow
            && logicalRow <= span.lastLogicalRow) {
            if (rangeMatch < 0) {
                rangeMatch = index;
            }
            if (traceSession_) {
                rangeCandidates.push_back(index);
            }
        }
    }

    if (traceSession_) {
        for (const int index : rangeCandidates) {
            const std::vector<std::uint64_t> rows =
                relatedLogicalRowsForSpan(spans_[static_cast<std::size_t>(index)]);
            if (std::find(rows.begin(), rows.end(), logicalRow) != rows.end()) {
                return index;
            }
        }
    }
    return rangeMatch;
}

qint64 TransactionWidget::snappedCursorTimestampForSpan(const TransactionSpan& span,
                                                        const qint64 requestedTimestamp) const
{
    if (requestedTimestamp <= span.startTimestamp) {
        return span.startTimestamp;
    }
    if (requestedTimestamp >= span.endTimestamp) {
        return span.endTimestamp;
    }
    return requestedTimestamp;
}

bool TransactionWidget::selectSpan(const int spanIndex, const std::optional<qint64> requestedTimestamp)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return false;
    }
    if (shouldDrawFilteredOutSpan(spanIndex)) {
        return false;
    }
    const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    clearSelectedOpcodeTag();
    selectedSpanIndex_ = spanIndex;
    selectedLogicalRow_ = static_cast<int>(span.requestLogicalRow);
    cursorTimestamp_ = snappedCursorTimestampForSpan(
        span,
        requestedTimestamp.value_or(timestampForLogicalRowInSpan(span, span.requestLogicalRow)));
    hasCursor_ = true;
    ensureSpanVisible(spanIndex);
    updateWidgetHints();
    return true;
}

bool TransactionWidget::selectedOpcodeTagAnchorsCursor() const noexcept
{
    return selectedOpcodeTagSpanIndex_ >= 0
        && selectedOpcodeTagSpanIndex_ == selectedSpanIndex_
        && selectedLogicalRow_ >= 0
        && selectedOpcodeTagLogicalRow_ == static_cast<std::uint64_t>(selectedLogicalRow_);
}

bool TransactionWidget::stickCursorToSelectedOpcodeTag(const bool ensureVisible)
{
    if (!selectedOpcodeTagAnchorsCursor()) {
        return false;
    }
    cursorTimestamp_ = selectedOpcodeTagTimestamp_;
    hasCursor_ = true;
    if (ensureVisible) {
        ensureTimestampVisible(cursorTimestamp_);
        if (selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())) {
            const TransactionSpan& span = spans_[static_cast<std::size_t>(selectedSpanIndex_)];
            if (span.laneIndex >= 0) {
                const int visibleRows = visibleLaneCapacity();
                if (span.laneIndex < verticalScrollOffset_) {
                    verticalScrollOffset_ = span.laneIndex;
                } else if (span.laneIndex >= verticalScrollOffset_ + visibleRows) {
                    verticalScrollOffset_ = std::max(0, span.laneIndex - visibleRows + 1);
                }
            }
        }
        updateScrollBars();
    }
    return true;
}

void TransactionWidget::reconcileSelectedOpcodeTag()
{
    if (selectedOpcodeTagSpanIndex_ < 0) {
        return;
    }
    if (selectedSpanIndex_ < 0
        || selectedSpanIndex_ >= static_cast<int>(spans_.size())
        || selectedOpcodeTagSpanIndex_ != selectedSpanIndex_) {
        clearSelectedOpcodeTag();
        return;
    }
    const TransactionSpan& span = spans_[static_cast<std::size_t>(selectedSpanIndex_)];
    const std::optional<OpcodeTagInfo> info =
        opcodeTagInfoForLogicalRow(span, selectedOpcodeTagLogicalRow_);
    if (!info || info->timestamp != selectedOpcodeTagTimestamp_) {
        clearSelectedOpcodeTag();
        return;
    }
    if (selectedOpcodeTagAnchorsCursor()) {
        stickCursorToSelectedOpcodeTag(false);
    }
}

void TransactionWidget::ensureTimestampVisible(const qint64 timestamp)
{
    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    if (timestamp >= visibleStart && timestamp <= visibleEnd) {
        return;
    }

    const long double maxOffset = std::max<long double>(0.0L, fullRange - visibleRange);
    const long double centered =
        static_cast<long double>(timestamp) - static_cast<long double>(fullStart) - visibleRange / 2.0L;
    scrollOffset_ = static_cast<std::uint64_t>(
        std::max<long double>(0.0L, std::min<long double>(maxOffset, centered)));
}

void TransactionWidget::ensureSpanVisible(const int spanIndex)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return;
    }
    const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    if (span.laneIndex >= 0) {
        const int visibleRows = visibleLaneCapacity();
        if (span.laneIndex < verticalScrollOffset_) {
            verticalScrollOffset_ = span.laneIndex;
        } else if (span.laneIndex >= verticalScrollOffset_ + visibleRows) {
            verticalScrollOffset_ = std::max(0, span.laneIndex - visibleRows + 1);
        }
    }

    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    if (span.startTimestamp < visibleStart || span.endTimestamp > visibleEnd) {
        const long double center =
            (static_cast<long double>(span.startTimestamp) + static_cast<long double>(span.endTimestamp)) / 2.0L;
        const long double maxOffset = std::max<long double>(0.0L, fullRange - visibleRange);
        scrollOffset_ = static_cast<std::uint64_t>(
            std::max<long double>(0.0L,
                                  std::min<long double>(maxOffset,
                                                        center - static_cast<long double>(fullStart) - visibleRange / 2.0L)));
    }
    updateScrollBars();
}

bool TransactionWidget::setCursorFromPlotPosition(const QPoint& position, const bool snapToTransaction)
{
    const QRect plot = plotRect();
    lastMousePosition_ = position;
    qint64 requestedTimestamp = plot.contains(position)
        ? plotXToTimestamp(position.x(), plot)
        : plotXToTimestamp(std::clamp(position.x(), plot.left(), plot.right()), plot);
    int snappedSpan = snapToTransaction ? spanIndexAtPosition(position) : -1;
    if (snapToTransaction && snappedSpan < 0) {
        snappedSpan = nearestVisibleSpanIndexAtPosition(position);
    }
    if (snappedSpan >= 0 && snappedSpan < static_cast<int>(spans_.size())) {
        const TransactionSpan& span = spans_[static_cast<std::size_t>(snappedSpan)];
        requestedTimestamp = snappedCursorTimestampForSpan(span, requestedTimestamp);
    }
    cursorTimestamp_ = requestedTimestamp;
    hasCursor_ = true;
    return snappedSpan >= 0;
}

void TransactionWidget::setMarkerFromSelectedSpan()
{
    if (selectedSpanIndex_ < 0 || selectedSpanIndex_ >= static_cast<int>(spans_.size())) {
        return;
    }
    markerSpanIndex_ = selectedSpanIndex_;
    markerLogicalRow_ = selectedLogicalRow_;
    markerTimestamp_ = cursorTimestamp_;
    hasMarker_ = true;
    update();
}

std::optional<QString> TransactionWidget::sharedMarkerAtPosition(const QPoint& position) const
{
    const QRect plot = plotRect();
    const QRect ruler = rulerRect();
    if (!plot.isValid() || !ruler.isValid()) {
        return std::nullopt;
    }
    const QRect activeArea(plot.left(), plot.top(), plot.width(), ruler.bottom() - plot.top() + 1);
    if (!activeArea.adjusted(-6, 0, 6, 0).contains(position)) {
        return std::nullopt;
    }
    for (const TraceMarker& marker : sharedMarkers_) {
        const int x = qBound(plot.left(),
                             static_cast<int>(std::llround(timestampToPlotX(marker.timestamp, plot))),
                             plot.right());
        const QRect hit(x - 6, plot.top(), 12, ruler.bottom() - plot.top() + 1);
        if (hit.contains(position)) {
            return marker.id;
        }
    }
    return std::nullopt;
}

bool TransactionWidget::activateSpanRequestRow(const int spanIndex)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return false;
    }
    if (shouldDrawFilteredOutSpan(spanIndex)) {
        return false;
    }
    selectSpan(spanIndex, {});
    Q_EMIT logicalRowActivated(static_cast<int>(spans_[static_cast<std::size_t>(spanIndex)].requestLogicalRow));
    update();
    return true;
}

bool TransactionWidget::activateSpanCompletionRow(const int spanIndex)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return false;
    }
    if (shouldDrawFilteredOutSpan(spanIndex)) {
        return false;
    }
    const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    const std::uint64_t logicalRow = span.completionLogicalRow.value_or(span.lastLogicalRow);
    clearSelectedOpcodeTag();
    selectedSpanIndex_ = spanIndex;
    selectedLogicalRow_ = static_cast<int>(logicalRow);
    cursorTimestamp_ = timestampForLogicalRowInSpan(span, logicalRow);
    hasCursor_ = true;
    ensureSpanVisible(spanIndex);
    Q_EMIT logicalRowActivated(selectedLogicalRow_);
    update();
    return true;
}

bool TransactionWidget::showSpanRelatedRows(const int spanIndex)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return false;
    }
    if (shouldDrawFilteredOutSpan(spanIndex)) {
        return false;
    }
    const QString text = relatedRowsText(spans_[static_cast<std::size_t>(spanIndex)]);
    if (QApplication::clipboard()) {
        QApplication::clipboard()->setText(text);
    }
    QMessageBox::information(this, QStringLiteral("Transaction Rows"), text);
    return true;
}

bool TransactionWidget::showSpanDebugInfo(const int spanIndex)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return false;
    }
    if (shouldDrawFilteredOutSpan(spanIndex)) {
        return false;
    }
    const QString text = debugText(spans_[static_cast<std::size_t>(spanIndex)]);
    if (QApplication::clipboard()) {
        QApplication::clipboard()->setText(text);
    }
    QMessageBox::information(this, QStringLiteral("Transaction Debug"), text);
    return true;
}

void TransactionWidget::ensureHoverCardCreated()
{
    if (hoverCard_) {
        return;
    }

    hoverCard_ = new QFrame(this);
    hoverCard_->setObjectName(QStringLiteral("transactionHoverCard"));
    hoverCard_->setFrameShape(QFrame::NoFrame);
    hoverCard_->setAutoFillBackground(true);
    hoverCard_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hoverCard_->setStyleSheet(QStringLiteral(
        "QFrame#transactionHoverCard {"
        "  background: #FFFFFF;"
        "  border: 1px solid #AEB7C2;"
        "}"
        "QLabel#transactionHoverTitle {"
        "  color: #16202A;"
        "  font-weight: 700;"
        "}"
        "QLabel#transactionHoverSummary,"
        "QLabel#transactionHoverFooter {"
        "  color: #4B5563;"
        "}"
        "QTableWidget#transactionHoverTable {"
        "  background: #FFFFFF;"
        "  alternate-background-color: #F7F9FB;"
        "  border: 1px solid #D7DDE3;"
        "  color: #1D2732;"
        "  gridline-color: #E1E6EC;"
        "}"
        "QHeaderView::section {"
        "  background: #EEF2F6;"
        "  color: #26313C;"
        "  border: 0;"
        "  border-right: 1px solid #D7DDE3;"
        "  border-bottom: 1px solid #D7DDE3;"
        "  padding: 2px 4px;"
        "}"));

    auto* layout = new QVBoxLayout(hoverCard_);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    hoverCardTitle_ = new QLabel(hoverCard_);
    hoverCardTitle_->setObjectName(QStringLiteral("transactionHoverTitle"));
    hoverCardTitle_->setTextFormat(Qt::PlainText);
    layout->addWidget(hoverCardTitle_);

    hoverCardSummary_ = new QLabel(hoverCard_);
    hoverCardSummary_->setObjectName(QStringLiteral("transactionHoverSummary"));
    hoverCardSummary_->setTextFormat(Qt::PlainText);
    hoverCardSummary_->setWordWrap(true);
    layout->addWidget(hoverCardSummary_);

    hoverCardTable_ = new QTableWidget(hoverCard_);
    hoverCardTable_->setObjectName(QStringLiteral("transactionHoverTable"));
    hoverCardTable_->setColumnCount(9);
    hoverCardTable_->setHorizontalHeaderLabels({
        QStringLiteral("Row"),
        QStringLiteral("Time"),
        QStringLiteral("Ch"),
        QStringLiteral("Dir"),
        QStringLiteral("Opcode"),
        QStringLiteral("Src"),
        QStringLiteral("Tgt"),
        QStringLiteral("TxnID"),
        QStringLiteral("Address"),
    });
    hoverCardTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hoverCardTable_->setSelectionMode(QAbstractItemView::NoSelection);
    hoverCardTable_->setFocusPolicy(Qt::NoFocus);
    hoverCardTable_->setAlternatingRowColors(true);
    hoverCardTable_->verticalHeader()->setVisible(false);
    hoverCardTable_->horizontalHeader()->setStretchLastSection(true);
    hoverCardTable_->horizontalHeader()->setMinimumSectionSize(36);
    hoverCardTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hoverCardTable_->setShowGrid(true);
    hoverCardTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hoverCardTable_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hoverCardTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hoverCardTable_->setColumnWidth(0, 54);
    hoverCardTable_->setColumnWidth(1, 70);
    hoverCardTable_->setColumnWidth(2, 40);
    hoverCardTable_->setColumnWidth(3, 36);
    hoverCardTable_->setColumnWidth(4, 124);
    hoverCardTable_->setColumnWidth(5, 42);
    hoverCardTable_->setColumnWidth(6, 42);
    hoverCardTable_->setColumnWidth(7, 54);
    layout->addWidget(hoverCardTable_);

    hoverCardFooter_ = new QLabel(hoverCard_);
    hoverCardFooter_->setObjectName(QStringLiteral("transactionHoverFooter"));
    hoverCardFooter_->setTextFormat(Qt::PlainText);
    layout->addWidget(hoverCardFooter_);

    hoverCard_->hide();
}

void TransactionWidget::hideHoverCard()
{
    if (hoverCardDwellTimer_) {
        hoverCardDwellTimer_->stop();
    }
    hoverCardPendingSpanIndex_ = -1;
    hoverCardSpanIndex_ = -1;
    if (hoverCard_) {
        hoverCard_->hide();
    }
}

void TransactionWidget::armHoverCard(const int spanIndex, const QPoint& position)
{
    if (!showHoverCards_ || dragMode_ != DragMode::None || leftDragGestureActive_ || spanIndex < 0) {
        hideHoverCard();
        return;
    }

    if (hoverCard_ && hoverCard_->isVisible() && hoverCardSpanIndex_ == spanIndex) {
        return;
    }

    if (hoverCard_ && hoverCard_->isVisible()) {
        hoverCard_->hide();
    }
    hoverCardPendingSpanIndex_ = spanIndex;
    hoverCardPendingPosition_ = position;
    if (hoverCardDwellTimer_) {
        hoverCardDwellTimer_->start();
    }
}

void TransactionWidget::showArmedHoverCard()
{
    if (!showHoverCards_) {
        hideHoverCard();
        return;
    }
    if (hoverCardPendingSpanIndex_ < 0
        || hoverCardPendingSpanIndex_ >= static_cast<int>(spans_.size())) {
        hideHoverCard();
        return;
    }
    if (dragMode_ != DragMode::None
        || leftDragGestureActive_
        || hoveredSpanIndex_ != hoverCardPendingSpanIndex_
        || spanIndexAtPosition(lastMousePosition_) != hoverCardPendingSpanIndex_) {
        hideHoverCard();
        return;
    }

    const int spanIndex = hoverCardPendingSpanIndex_;
    const QPoint position = hoverCardPendingPosition_;
    hoverCardPendingSpanIndex_ = -1;
    updateHoverCardForSpan(spanIndex, position);
}

QRect TransactionWidget::hoverCardGeometryForPosition(const QPoint& position, const QSize& size) const
{
    if (size.isEmpty()) {
        return {};
    }

    const QRect bounds = rect().adjusted(6, headerRect().bottom() + 4, -scrollBarExtent() - 6, -scrollBarExtent() - 6);
    if (!bounds.isValid()) {
        return QRect(position, size);
    }

    QPoint topLeft(position.x() + kHoverCardHorizontalOffset,
                   position.y() + kHoverCardVerticalOffset);
    if (topLeft.x() + size.width() > bounds.right()) {
        topLeft.setX(position.x() - size.width() - kHoverCardHorizontalOffset);
    }
    if (topLeft.y() + size.height() > bounds.bottom()) {
        topLeft.setY(position.y() - size.height() - kHoverCardVerticalOffset);
    }
    topLeft.setX(std::clamp(topLeft.x(), bounds.left(), std::max(bounds.left(), bounds.right() - size.width() + 1)));
    topLeft.setY(std::clamp(topLeft.y(), bounds.top(), std::max(bounds.top(), bounds.bottom() - size.height() + 1)));
    return QRect(topLeft, size);
}

TransactionWidget::HoverCardFlitRow TransactionWidget::hoverCardRowForLogicalRow(
    const TransactionSpan& span,
    const std::uint64_t logicalRow) const
{
    HoverCardFlitRow row;
    row.logicalRow = logicalRow;
    row.timestamp = timestampForLogicalRowInSpan(span, logicalRow);
    row.channel = QStringLiteral("-");
    row.direction = QStringLiteral("-");
    row.opcode = QStringLiteral("-");
    row.source = QStringLiteral("-");
    row.target = QStringLiteral("-");
    row.txnId = QStringLiteral("-");
    row.address = QStringLiteral("-");

    if (logicalRow < rows_.size()) {
        const FlitRecord& record = rows_[static_cast<std::size_t>(logicalRow)];
        row.timestamp = record.timestamp;
        row.channel = ToString(record.channel);
        row.direction = ToString(record.direction);
        row.opcode = record.opcode.trimmed().isEmpty() ? QStringLiteral("Unknown") : record.opcode.trimmed();
        row.source = displayRowEndpointId(record.source);
        row.target = displayRowEndpointId(record.target);
        row.txnId = displayRowEndpointId(record.txnId);
        row.address = record.address.trimmed().isEmpty() ? QStringLiteral("-") : record.address.trimmed();
        row.decoded = true;
        return row;
    }
    if (traceSession_) {
        if (const FlitRecord* record = traceSession_->tryGetRow(logicalRow)) {
            row.timestamp = record->timestamp;
            row.channel = ToString(record->channel);
            row.direction = ToString(record->direction);
            row.opcode = record->opcode.trimmed().isEmpty() ? QStringLiteral("Unknown") : record->opcode.trimmed();
            row.source = displayRowEndpointId(record->source);
            row.target = displayRowEndpointId(record->target);
            row.txnId = displayRowEndpointId(record->txnId);
            row.address = record->address.trimmed().isEmpty() ? QStringLiteral("-") : record->address.trimmed();
            row.decoded = true;
            return row;
        }
    }

    if (traceSession_) {
        const auto& blocks = traceSession_->metadata().blocks;
        auto foundBlock = std::upper_bound(
            blocks.begin(),
            blocks.end(),
            logicalRow,
            [](const std::uint64_t rowValue, const CLogBTraceBlockSummary& block) {
                return rowValue < block.rowStart;
            });
        if (foundBlock != blocks.begin()) {
            --foundBlock;
            const CLogBTraceBlockSummary& block = *foundBlock;
            if (logicalRow >= block.rowStart
                && logicalRow < block.rowStart + block.recordCount) {
                std::vector<CLogBTraceFastRecordInfo> fastRecords;
                const std::size_t blockIndex = static_cast<std::size_t>(
                    std::distance(blocks.begin(), foundBlock));
                const std::size_t localBegin = static_cast<std::size_t>(logicalRow - block.rowStart);
                if (traceSession_->readFastRecordsFromIndex(blockIndex, localBegin, 1, fastRecords)
                    && !fastRecords.empty()
                    && validTransportChannel(fastRecords.front().channel)) {
                    std::unordered_map<std::uint32_t, QString> opcodeCache;
                    const BuildRowSummary summary =
                        summaryFromFastRecord(traceSession_, logicalRow, fastRecords.front(), opcodeCache);
                    row.timestamp = summary.timestamp;
                    row.channel = ToString(summary.channel);
                    row.direction = ToString(summary.direction);
                    row.opcode = displayOpcode(summary);
                    row.source = displayRowEndpointId(summary.source);
                    row.target = displayRowEndpointId(summary.target);
                    row.txnId = displayRowEndpointId(summary.txnId);
                    row.address = summary.address.trimmed().isEmpty() ? QStringLiteral("-") : summary.address.trimmed();
                    row.decoded = true;
                    return row;
                }
            }
        }
    }

    row.opcode = logicalRow == span.requestLogicalRow
        ? opcodeWithoutChannelPrefix(span.requestOpcode)
        : (span.completionLogicalRow && logicalRow == *span.completionLogicalRow
               ? QStringLiteral("Completion")
               : QStringLiteral("Related flit"));
    if (row.opcode.trimmed().isEmpty()) {
        row.opcode = QStringLiteral("Unknown");
    }
    row.address = span.addressLabel.trimmed().isEmpty() ? QStringLiteral("-") : span.addressLabel.trimmed();
    return row;
}

std::vector<TransactionWidget::HoverCardFlitRow> TransactionWidget::hoverCardFlitRowsForSpan(
    const TransactionSpan& span) const
{
    const std::vector<std::uint64_t> logicalRows = relatedLogicalRowsForSpan(span);
    std::vector<HoverCardFlitRow> rows;
    rows.reserve(std::min<std::size_t>(logicalRows.size(), static_cast<std::size_t>(kHoverCardMaxRows)));
    for (const std::uint64_t logicalRow : logicalRows) {
        if (rows.size() >= static_cast<std::size_t>(kHoverCardMaxRows)) {
            break;
        }
        rows.push_back(hoverCardRowForLogicalRow(span, logicalRow));
    }
    return rows;
}

void TransactionWidget::populateHoverCard(const int spanIndex)
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        hideHoverCard();
        return;
    }
    ensureHoverCardCreated();

    const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
    const QString opcode = opcodeWithoutChannelPrefix(span.requestOpcode).trimmed().isEmpty()
        ? QStringLiteral("Opcode unknown")
        : opcodeWithoutChannelPrefix(span.requestOpcode).trimmed();
    const QString address = span.addressLabel.trimmed().isEmpty()
        ? QStringLiteral("Address unknown")
        : span.addressLabel.trimmed();
    const QString txnId = span.txnIdLabel.trimmed().isEmpty()
        ? QStringLiteral("TxnID unknown")
        : QStringLiteral("TxnID %1").arg(span.txnIdLabel.trimmed());

    hoverCardTitle_->setText(QStringLiteral("%1  |  %2  |  %3")
                                 .arg(span.endpointLabel,
                                      span.originKind,
                                      opcode));
    hoverCardSummary_->setText(
        QStringLiteral("Ordinal %1  |  %2  |  %3  |  %4  |  %5 rows  |  %6 -> %7  |  %8")
            .arg(static_cast<qulonglong>(span.ordinal))
            .arg(span.xactionType.trimmed().isEmpty() ? QStringLiteral("Unknown Xaction") : span.xactionType.trimmed())
            .arg(txnId)
            .arg(address)
            .arg(FormatUnsignedIntegerWithThousands(span.rowCount))
            .arg(FormatTimestampForDisplay(span.startTimestamp))
            .arg(FormatTimestampForDisplay(span.endTimestamp))
            .arg(span.completed ? QStringLiteral("completed") : QStringLiteral("incomplete")));

    const std::vector<HoverCardFlitRow> flitRows = hoverCardFlitRowsForSpan(span);
    hoverCardTable_->setRowCount(static_cast<int>(flitRows.size()));
    for (int rowIndex = 0; rowIndex < static_cast<int>(flitRows.size()); ++rowIndex) {
        const HoverCardFlitRow& flitRow = flitRows[static_cast<std::size_t>(rowIndex)];
        const QStringList cells = {
            QString::number(static_cast<qulonglong>(flitRow.logicalRow)),
            FormatTimestampForDisplay(flitRow.timestamp),
            flitRow.channel,
            flitRow.direction,
            flitRow.opcode,
            flitRow.source,
            flitRow.target,
            flitRow.txnId,
            flitRow.address,
        };
        for (int column = 0; column < cells.size(); ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            item->setFlags(Qt::ItemIsEnabled);
            item->setToolTip(cells[column]);
            hoverCardTable_->setItem(rowIndex, column, item);
        }
    }
    hoverCardTable_->resizeRowsToContents();
    const int tableHeight =
        hoverCardTable_->horizontalHeader()->height()
        + std::max(1, hoverCardTable_->rowCount()) * hoverCardTable_->verticalHeader()->defaultSectionSize()
        + 2;
    hoverCardTable_->setFixedHeight(std::min(260, tableHeight));

    const std::vector<std::uint64_t> allRows = relatedLogicalRowsForSpan(span);
    const std::size_t hiddenRows =
        allRows.size() > flitRows.size() ? allRows.size() - flitRows.size() : 0;
    hoverCardFooter_->setText(hiddenRows == 0
        ? QStringLiteral("All related flits are shown.")
        : QStringLiteral("%1 more related flits not shown in the hover card.")
              .arg(FormatUnsignedIntegerWithThousands(hiddenRows)));
    hoverCardSpanIndex_ = spanIndex;
}

void TransactionWidget::updateHoverCardForSpan(const int spanIndex, const QPoint& position)
{
    if (dragMode_ != DragMode::None || leftDragGestureActive_ || spanIndex < 0) {
        hideHoverCard();
        return;
    }

    ensureHoverCardCreated();
    if (hoverCardSpanIndex_ != spanIndex) {
        populateHoverCard(spanIndex);
    }
    const QSize hint(qMin(kHoverCardWidth, std::max(360, width() - scrollBarExtent() - 24)),
                     hoverCard_->sizeHint().height());
    hoverCard_->setGeometry(hoverCardGeometryForPosition(position, hint));
    hoverCard_->show();
    hoverCard_->raise();
}

QString TransactionWidget::relatedRowsText(const TransactionSpan& span) const
{
    const std::vector<std::uint64_t> logicalRows = relatedLogicalRowsForSpan(span);
    QStringList rows;
    rows.reserve(static_cast<int>(logicalRows.size()));
    for (const std::uint64_t logicalRow : logicalRows) {
        rows.append(QString::number(static_cast<qulonglong>(logicalRow)));
    }
    return QStringLiteral("Ordinal: %1\nRequest row: %2\nCompletion row: %3\nRows: %4")
        .arg(static_cast<qulonglong>(span.ordinal))
        .arg(static_cast<qulonglong>(span.requestLogicalRow))
        .arg(span.completionLogicalRow
                 ? QString::number(static_cast<qulonglong>(*span.completionLogicalRow))
                 : QStringLiteral("none"))
        .arg(rows.join(QStringLiteral(", ")));
}

QString TransactionWidget::debugText(const TransactionSpan& span) const
{
    const QString displayOpcode = opcodeWithoutChannelPrefix(span.requestOpcode);
    const QString opcode = displayOpcode.isEmpty()
        ? QStringLiteral("Opcode unknown")
        : displayOpcode;
    const QString address = span.addressLabel.trimmed().isEmpty()
        ? QStringLiteral("Address unknown")
        : span.addressLabel.trimmed();
    return QStringLiteral(
               "Ordinal: %1\n"
               "Group: %2\n"
               "Joint: %3\n"
               "Joint path: %4\n"
               "Endpoint: %5\n"
               "Origin: %6\n"
               "Classification: %7\n"
               "Xaction type: %8\n"
               "Opcode: %9\n"
               "Address: %10\n"
               "Stack: group %11, slot %12/%13\n"
               "Rows: %14\n"
               "Time: %15 -> %16\n"
               "Completed: %17")
        .arg(static_cast<qulonglong>(span.ordinal))
        .arg(span.transactionGroupKey,
             span.jointFamily,
             span.jointPath,
             span.endpointLabel,
             span.originKind,
             span.classificationConfidence,
             span.xactionType,
             opcode,
             address,
             QString::number(span.groupIndex),
             QString::number(span.stackSlot + 1),
             QString::number(span.stackDepth),
             FormatUnsignedIntegerWithThousands(span.rowCount),
             FormatTimestampForDisplay(span.startTimestamp),
             FormatTimestampForDisplay(span.endTimestamp),
             span.completed ? QStringLiteral("yes") : QStringLiteral("no"));
}

std::vector<std::uint64_t> TransactionWidget::relatedLogicalRowsForSpan(const TransactionSpan& span) const
{
    if (traceSession_
        && (span.logicalRows.empty()
            || span.rowCount > static_cast<std::uint64_t>(span.logicalRows.size()))) {
        std::vector<std::uint64_t> rows = traceSession_->xactionRowsForOrdinal(span.ordinal);
        if (!rows.empty()) {
            std::sort(rows.begin(), rows.end());
            return rows;
        }
    }
    if (!span.logicalRows.empty()) {
        return span.logicalRows;
    }
    std::vector<std::uint64_t> rows;
    rows.push_back(span.firstLogicalRow);
    if (span.requestLogicalRow != span.firstLogicalRow
        && span.requestLogicalRow != span.lastLogicalRow) {
        rows.push_back(span.requestLogicalRow);
    }
    if (span.completionLogicalRow
        && *span.completionLogicalRow != span.firstLogicalRow
        && *span.completionLogicalRow != span.requestLogicalRow
        && *span.completionLogicalRow != span.lastLogicalRow) {
        rows.push_back(*span.completionLogicalRow);
    }
    if (span.lastLogicalRow != span.firstLogicalRow) {
        rows.push_back(span.lastLogicalRow);
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
}

void TransactionWidget::pushViewHistory()
{
    ViewHistoryEntry entry;
    entry.horizontalZoom = horizontalZoom_;
    entry.scrollOffset = scrollOffset_;
    entry.verticalScrollOffset = verticalScrollOffset_;
    entry.hasCursor = hasCursor_;
    entry.cursorTimestamp = cursorTimestamp_;
    if (!viewHistory_.empty()) {
        const ViewHistoryEntry& last = viewHistory_.back();
        if (std::abs(last.horizontalZoom - entry.horizontalZoom) < 0.000001
            && last.scrollOffset == entry.scrollOffset
            && last.verticalScrollOffset == entry.verticalScrollOffset
            && last.hasCursor == entry.hasCursor
            && last.cursorTimestamp == entry.cursorTimestamp) {
            return;
        }
    }
    viewHistory_.push_back(entry);
    while (static_cast<int>(viewHistory_.size()) > kViewHistoryLimit) {
        viewHistory_.pop_front();
    }
}

void TransactionWidget::restorePreviousView()
{
    if (viewHistory_.empty()) {
        return;
    }
    const ViewHistoryEntry entry = viewHistory_.back();
    viewHistory_.pop_back();
    horizontalZoom_ = std::clamp(entry.horizontalZoom, kMinZoom, kMaxZoom);
    scrollOffset_ = entry.scrollOffset;
    verticalScrollOffset_ = entry.verticalScrollOffset;
    hasCursor_ = entry.hasCursor;
    cursorTimestamp_ = entry.cursorTimestamp;
    stickCursorToSelectedOpcodeTag(false);
    updateScrollBars();
    update();
}

void TransactionWidget::zoomToFit()
{
    pushViewHistory();
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    stickCursorToSelectedOpcodeTag(false);
    updateScrollBars();
    update();
}

void TransactionWidget::zoomByFactor(const double factor, const QPoint& focus)
{
    if (spans_.empty()) {
        return;
    }
    pushViewHistory();
    const QRect plot = plotRect();
    const qint64 focusTimestamp = plotXToTimestamp(focus.x(), plot);
    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    horizontalZoom_ = std::clamp(horizontalZoom_ * factor, kMinZoom, kMaxZoom);
    const long double newRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const long double focusRatio =
        plot.width() <= 0 ? 0.5L : std::clamp<long double>(
            static_cast<long double>(focus.x() - plot.left()) / std::max(1, plot.width()),
            0.0L,
            1.0L);
    const long double desiredStart = static_cast<long double>(focusTimestamp) - newRange * focusRatio;
    const long double maxOffset = std::max<long double>(0.0L, fullRange - newRange);
    scrollOffset_ = static_cast<std::uint64_t>(
        std::max<long double>(0.0L,
                              std::min<long double>(maxOffset, desiredStart - static_cast<long double>(fullStart))));
    stickCursorToSelectedOpcodeTag(false);
    updateScrollBars();
    update();
}

void TransactionWidget::zoomToTimeRange(const qint64 firstTimestamp, const qint64 lastTimestamp)
{
    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const qint64 left = std::max(fullStart, std::min(firstTimestamp, lastTimestamp));
    const qint64 right = std::min(fullEnd, std::max(firstTimestamp, lastTimestamp));
    if (right <= left) {
        return;
    }
    pushViewHistory();
    const long double requestedRange =
        std::max<long double>(1.0L, static_cast<long double>(right) - static_cast<long double>(left));
    horizontalZoom_ = std::clamp(static_cast<double>(fullRange / requestedRange), kMinZoom, kMaxZoom);
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const long double maxOffset = std::max<long double>(0.0L, fullRange - visibleRange);
    scrollOffset_ = static_cast<std::uint64_t>(
        std::max<long double>(0.0L,
                              std::min<long double>(maxOffset, static_cast<long double>(left) - fullStart)));
    stickCursorToSelectedOpcodeTag(false);
    updateScrollBars();
    update();
}

void TransactionWidget::panHorizontallyByPixels(const int pixels)
{
    const QRect plot = plotRect();
    if (plot.width() <= 0 || spans_.empty()) {
        return;
    }
    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const long double timestampDelta =
        static_cast<long double>(pixels) * visibleRange / static_cast<long double>(std::max(1, plot.width()));
    const long double maxOffset = std::max<long double>(0.0L, fullRange - visibleRange);
    const long double current = static_cast<long double>(scrollOffset_);
    scrollOffset_ = static_cast<std::uint64_t>(
        std::max<long double>(0.0L, std::min<long double>(maxOffset, current + timestampDelta)));
    updateScrollBars();
    update();
}

void TransactionWidget::panVerticallyByRows(const int rows)
{
    if (lanes_.empty()) {
        return;
    }
    const int maxVertical = std::max(0, stackRowCount() - visibleLaneCapacity());
    verticalScrollOffset_ = std::clamp(verticalScrollOffset_ + rows, 0, maxVertical);
    updateScrollBars();
    update();
}

void TransactionWidget::moveSelectionBySpan(const int direction)
{
    if (spans_.empty()) {
        return;
    }
    int next = selectedSpanIndex_ >= 0 ? selectedSpanIndex_ + direction : (direction >= 0 ? 0 : static_cast<int>(spans_.size()) - 1);
    next = std::clamp(next, 0, static_cast<int>(spans_.size()) - 1);
    selectSpan(next, {});
    update();
}

void TransactionWidget::beginPendingGesture(const QPoint& position)
{
    hideHoverCard();
    dragMode_ = DragMode::PendingGesture;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
    leftDragAnchor_ = position;
    update();
}

void TransactionWidget::updateLeftDragGesture(const QPoint& position,
                                              const Qt::KeyboardModifiers modifiers)
{
    const DragMode previousMode = dragMode_;
    dragLast_ = position;
    dragStart_ = leftDragAnchor_;
    const DragMode nextMode = classifyLeftDragGesture(position, modifiers);
    if (nextMode != previousMode && nextMode == DragMode::Pan) {
        dragStartScrollOffset_ = scrollOffset_;
        dragStartVerticalOffset_ = verticalScrollOffset_;
        setCursor(Qt::ClosedHandCursor);
    }
    dragMode_ = nextMode;
    if (dragMode_ == DragMode::Pan) {
        updatePan(position);
    } else {
        update();
    }
}

TransactionWidget::DragMode TransactionWidget::classifyLeftDragGesture(
    const QPoint& position,
    const Qt::KeyboardModifiers modifiers) const
{
    const QPoint delta = position - leftDragAnchor_;
    if (delta.manhattanLength() < kGestureActivationDistance) {
        return DragMode::PendingGesture;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        return DragMode::Pan;
    }
    if (modifiers.testFlag(Qt::ControlModifier) && positionInPlotOrRuler(leftDragAnchor_)) {
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
    return positionInPlotOrRuler(leftDragAnchor_) ? DragMode::RangeZoom : DragMode::PendingGesture;
}

bool TransactionWidget::finishRangeZoom()
{
    const QRect area = plotRect();
    const int left = qBound(area.left(), std::min(dragStart_.x(), dragLast_.x()), area.right());
    const int right = qBound(area.left(), std::max(dragStart_.x(), dragLast_.x()), area.right());
    if (right - left < 8) {
        return false;
    }
    zoomToTimeRange(plotXToTimestamp(left, area), plotXToTimestamp(right, area));
    return true;
}

bool TransactionWidget::finishZoomFullGesture()
{
    zoomToFit();
    return true;
}

bool TransactionWidget::finishZoomIn2xGesture()
{
    zoomByFactor(2.0, dragStart_);
    return true;
}

bool TransactionWidget::finishZoomOut2xGesture()
{
    zoomByFactor(0.5, dragStart_);
    return true;
}

void TransactionWidget::beginPan(const QPoint& position)
{
    hideHoverCard();
    dragMode_ = DragMode::Pan;
    leftDragGestureActive_ = true;
    dragStart_ = position;
    dragLast_ = position;
    leftDragAnchor_ = position;
    dragStartScrollOffset_ = scrollOffset_;
    dragStartVerticalOffset_ = verticalScrollOffset_;
    setCursor(Qt::ClosedHandCursor);
}

void TransactionWidget::updatePan(const QPoint& position)
{
    const int dx = position.x() - dragStart_.x();
    const int dy = position.y() - dragStart_.y();

    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const long double timestampDelta =
        static_cast<long double>(-dx) * visibleRange / static_cast<long double>(std::max(1, plotRect().width()));
    const long double maxOffset = std::max<long double>(0.0L, fullRange - visibleRange);
    scrollOffset_ = static_cast<std::uint64_t>(
        std::max<long double>(0.0L,
                              std::min<long double>(maxOffset,
                                                    static_cast<long double>(dragStartScrollOffset_) + timestampDelta)));

    const int maxVertical = std::max(0, stackRowCount() - visibleLaneCapacity());
    verticalScrollOffset_ = std::clamp(dragStartVerticalOffset_ - dy / rowHeight(), 0, maxVertical);
    updateScrollBars();
    update();
}

bool TransactionWidget::positionInPlotOrRuler(const QPoint& position) const
{
    return plotRect().contains(position) || rulerRect().contains(position);
}

QRect TransactionWidget::cursorRulerTagRect(const qint64 timestamp,
                                            const QRect& plot,
                                            const QRect& ruler,
                                            const QFontMetrics& metrics) const
{
    if (!plot.isValid() || !ruler.isValid()) {
        return {};
    }

    const QString label = FormatTimestampForDisplay(timestamp);
    const int tagWidth = std::min(ruler.width(),
                                  metrics.horizontalAdvance(label) + kCursorRulerTagHorizontalPadding * 2);
    if (tagWidth <= 0) {
        return {};
    }

    const int cursorX = qBound(plot.left(),
                               static_cast<int>(std::llround(timestampToPlotX(timestamp, plot))),
                               plot.right());
    const int minimumLeft = ruler.left() + 2;
    const int maximumLeft = ruler.right() - tagWidth - 1;
    const int tagLeft = maximumLeft < minimumLeft
        ? minimumLeft
        : std::clamp(cursorX - tagWidth / 2, minimumLeft, maximumLeft);
    const int tagTop = std::min(ruler.bottom() - kCursorRulerTagHeight - 2,
                                ruler.top() + kTimeRulerTickHeight + 1);
    return QRect(tagLeft,
                 std::max(ruler.top() + 1, tagTop),
                 tagWidth,
                 std::min(kCursorRulerTagHeight, std::max(0, ruler.height() - 2)));
}

void TransactionWidget::paintCursorRulerTag(QPainter& painter, const QRect& plot, const QRect& ruler) const
{
    if (!hasCursor_ || !plot.isValid() || !ruler.isValid()) {
        return;
    }

    QFont tagFont = rulerFont();
    tagFont.setBold(true);
    if (tagFont.pointSizeF() > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(7.0, tagFont.pointSizeF() * 0.86));
    } else {
        tagFont.setPixelSize(qMax(8, qRound(static_cast<qreal>(tagFont.pixelSize()) * 0.86)));
    }
    const QFontMetrics tagMetrics(tagFont);
    const QString label = FormatTimestampForDisplay(cursorTimestamp_);
    const QRect tag = cursorRulerTagRect(cursorTimestamp_, plot, ruler, tagMetrics);
    if (!tag.isValid()) {
        return;
    }

    const int cursorX = qBound(plot.left(),
                               static_cast<int>(std::llround(timestampToPlotX(cursorTimestamp_, plot))),
                               plot.right());
    painter.save();
    painter.setClipRect(ruler);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(QStringLiteral("#111111")), 1));
    painter.drawLine(QPoint(cursorX, ruler.top() + 1), QPoint(cursorX, tag.top() - 1));
    painter.fillRect(tag, QColor(QStringLiteral("#111111")));
    painter.setPen(QColor(QStringLiteral("#FFFFFF")));
    painter.setFont(tagFont);
    painter.drawText(tag.adjusted(kCursorRulerTagHorizontalPadding, 0, -kCursorRulerTagHorizontalPadding, 0),
                     Qt::AlignCenter,
                     tagMetrics.elidedText(label,
                                           Qt::ElideRight,
                                           std::max(1, tag.width() - kCursorRulerTagHorizontalPadding * 2)));
    painter.restore();
}

void TransactionWidget::paintSharedMarkers(QPainter& painter,
                                           const QRect& plot,
                                           const QRect& ruler,
                                           const QRect& infoBar) const
{
    if (sharedMarkers_.empty() || !plot.isValid() || !ruler.isValid()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const TraceMarker& marker : sharedMarkers_) {
        const int x = qBound(plot.left(),
                             static_cast<int>(std::llround(timestampToPlotX(marker.timestamp, plot))),
                             plot.right());
        const QColor color = marker.color.isValid() ? marker.color : DefaultTraceMarkerColor(marker.logicalRow);
        const bool active = marker.id == selectedMarkerId_ || marker.id == hoveredMarkerId_;
        QColor lineColor = color;
        lineColor.setAlpha(active ? 225 : 86);
        QColor fillColor = color;
        fillColor.setAlpha(active ? 235 : 118);

        painter.setPen(QPen(lineColor, active ? 2 : 1));
        painter.drawLine(QPoint(x, headerRect().bottom() + 1), QPoint(x, ruler.bottom()));
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawRoundedRect(QRect(x - 3, ruler.top() + 2, 7, qMax(6, ruler.height() - 4)), 2, 2);
        if (active && infoBar.isValid()) {
            painter.setPen(color.darker(130));
            painter.setFont(infoBarFont());
            painter.drawText(infoBar.adjusted(8, 0, -8, 0),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             QStringLiteral("%1  row %2")
                                 .arg(marker.label)
                                 .arg(marker.logicalRow + 1));
        }
    }
    painter.restore();
}

void TransactionWidget::paintBottomSpanSnap(QPainter& painter, const QRect& plot, const QRect& ruler) const
{
    if (spans_.empty() || !plot.isValid() || !ruler.isValid()) {
        return;
    }
    const QRect snap(plot.left(), ruler.bottom() - 5, plot.width(), 5);
    if (!snap.isValid()) {
        return;
    }

    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const std::vector<int> visibleSpans = visibleSpanIndices(visibleStart, visibleEnd);

    painter.save();
    painter.setClipRect(snap);
    for (const int spanIndex : visibleSpans) {
        if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
            continue;
        }
        const TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
        const long double startFraction =
            (static_cast<long double>(std::max(span.startTimestamp, visibleStart)) - visibleStart) / visibleRange;
        const long double endFraction =
            (static_cast<long double>(std::min(span.endTimestamp, visibleEnd)) - visibleStart) / visibleRange;
        const int left = plot.left() + static_cast<int>(std::floor(startFraction * plot.width()));
        const int right = plot.left() + static_cast<int>(std::ceil(endFraction * plot.width()));
        QColor color = spanEdgeColor(span);
        const bool active = spanIndex == selectedSpanIndex_ || spanIndex == hoveredSpanIndex_;
        color.setAlpha(active ? 210 : 76);
        painter.fillRect(QRect(left, snap.top(), qMax(1, right - left + 1), snap.height()), color);
    }
    painter.restore();
}

void TransactionWidget::paintRangeZoom(QPainter& painter) const
{
    if (dragMode_ != DragMode::RangeZoom) {
        return;
    }
    const QRect area = plotRect();
    const int left = qBound(area.left(), std::min(dragStart_.x(), dragLast_.x()), area.right());
    const int right = qBound(area.left(), std::max(dragStart_.x(), dragLast_.x()), area.right());
    const QRect selection(left, area.top(), std::max(1, right - left), area.height());

    painter.save();
    painter.fillRect(selection, withAlpha(QColor(QStringLiteral("#2F80ED")), 42));
    painter.setPen(QPen(QColor(QStringLiteral("#2F80ED")), 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selection.adjusted(0, 0, -1, -1));
    painter.restore();
}

void TransactionWidget::paintGestureOverlay(QPainter& painter) const
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

    const double length = std::clamp(std::hypot(static_cast<double>(delta.x()),
                                                static_cast<double>(delta.y())),
                                     28.0,
                                     120.0);
    const double angle = static_cast<double>(octant) * (kPi / 4.0);
    const QPointF start(dragStart_);
    const QPointF end(start.x() + std::cos(angle) * length,
                      start.y() + std::sin(angle) * length);
    const QPointF unit(std::cos(angle), std::sin(angle));
    const QPointF normal(-unit.y(), unit.x());

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#1F5FBF")), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(start, end);
    painter.drawLine(end, end - unit * 10.0 + normal * 5.0);
    painter.drawLine(end, end - unit * 10.0 - normal * 5.0);

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
    painter.setPen(QColor(QStringLiteral("#8BAFE8")));
    painter.setBrush(withAlpha(QColor(QStringLiteral("#FFFFFF")), 235));
    painter.drawRoundedRect(labelRect, 4, 4);
    painter.setPen(QColor(QStringLiteral("#123B72")));
    painter.drawText(labelRect, Qt::AlignCenter, label);
    painter.restore();
}

QString TransactionWidget::dragModeName(const DragMode mode)
{
    switch (mode) {
    case DragMode::None:
        return QStringLiteral("None");
    case DragMode::PendingGesture:
        return QStringLiteral("PendingGesture");
    case DragMode::RangeZoom:
        return QStringLiteral("RangeZoom");
    case DragMode::ZoomFull:
        return QStringLiteral("ZoomFull");
    case DragMode::ZoomIn2x:
        return QStringLiteral("ZoomIn2x");
    case DragMode::ZoomOut2x:
        return QStringLiteral("ZoomOut2x");
    case DragMode::Pan:
        return QStringLiteral("Pan");
    }
    return QStringLiteral("None");
}

void TransactionWidget::updateScrollBars()
{
    if (!horizontalScrollBar_ || !verticalScrollBar_) {
        return;
    }

    updateFilterToolbarGeometry();
    horizontalScrollBar_->setGeometry(horizontalScrollBarRect());
    verticalScrollBar_->setGeometry(verticalScrollBarRect());

    const auto [fullStart, fullEnd] = fullTimestampRange();
    const long double fullRange =
        std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
    const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
    const std::uint64_t maxHorizontal =
        transactionHorizontalScrollRange(fullStart, fullEnd, horizontalZoom_);
    const std::uint64_t horizontalOffset = spans_.empty()
        ? 0
        : std::min<std::uint64_t>(scrollOffset_, maxHorizontal);
    const int horizontalValue = scaledScrollBarValue(horizontalOffset, maxHorizontal);

    horizontalScrollBar_->blockSignals(true);
    horizontalScrollBar_->setRange(0, maxHorizontal == 0 ? 0 : kMaxScrollBarRange);
    horizontalScrollBar_->setPageStep(std::max(1, scaledScrollBarValue(boundedRoundToUInt64(visibleRange), maxHorizontal)));
    horizontalScrollBar_->setSingleStep(std::max(1, horizontalScrollBar_->pageStep() / 20));
    horizontalScrollBar_->setValue(horizontalValue);
    horizontalScrollBar_->setVisible(!spans_.empty() && maxHorizontal > 0 && horizontalScrollBarRect().width() > 0);
    horizontalScrollBar_->blockSignals(false);
    if (!spans_.empty()) {
        scrollOffset_ = horizontalOffset;
    }

    const int visibleRows = visibleLaneCapacity();
    const int maxVertical = std::max(0, stackRowCount() - visibleRows);
    const int verticalValue = lanes_.empty() ? 0 : std::clamp(verticalScrollOffset_, 0, maxVertical);
    verticalScrollBar_->blockSignals(true);
    verticalScrollBar_->setRange(0, maxVertical);
    verticalScrollBar_->setPageStep(std::max(1, visibleRows));
    verticalScrollBar_->setSingleStep(1);
    verticalScrollBar_->setValue(verticalValue);
    verticalScrollBar_->setVisible(!lanes_.empty() && maxVertical > 0 && verticalScrollBarRect().height() > 0);
    verticalScrollBar_->blockSignals(false);
    if (!lanes_.empty()) {
        verticalScrollOffset_ = verticalValue;
    }
}

void TransactionWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(QStringLiteral("#FFFFFF")));

    const QRect header = headerRect();
    painter.fillRect(header, QColor(QStringLiteral("#F4F6F8")));
    painter.setPen(QColor(QStringLiteral("#D7DDE3")));
    painter.drawLine(header.bottomLeft(), header.bottomRight());
    const QRect toolbar = filterToolbarRect();
    painter.fillRect(toolbar, QColor(QStringLiteral("#EEF2F5")));
    painter.drawLine(toolbar.topLeft(), toolbar.topRight());
    painter.drawLine(toolbar.bottomLeft(), toolbar.bottomRight());

    painter.setPen(QColor(QStringLiteral("#18212B")));
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRect(header.left() + 10,
                           header.top(),
                           std::max(0, header.width() - 20),
                           kHeaderPrimaryRowHeight),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Transaction"));
    const QRect arrangeSwitch = arrangeModeSwitchRect();
    painter.setFont(font());
    painter.setPen(QColor(QStringLiteral("#AEB7C2")));
    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    painter.drawRect(arrangeSwitch.adjusted(0, 0, -1, -1));
    const QRect leftModeRect(arrangeSwitch.left() + 1,
                             arrangeSwitch.top() + 1,
                             arrangeSwitch.width() / 2 - 1,
                             arrangeSwitch.height() - 2);
    const QRect rightModeRect(arrangeSwitch.left() + arrangeSwitch.width() / 2,
                              arrangeSwitch.top() + 1,
                              arrangeSwitch.width() / 2 - 1,
                              arrangeSwitch.height() - 2);
    painter.fillRect(arrangeMode_ == ArrangeMode::InflightStack ? leftModeRect : rightModeRect,
                     QColor(QStringLiteral("#26313C")));
    painter.setPen(arrangeMode_ == ArrangeMode::InflightStack
                       ? QColor(QStringLiteral("#FFFFFF"))
                       : QColor(QStringLiteral("#43505C")));
    painter.drawText(leftModeRect, Qt::AlignCenter, QStringLiteral("Stack"));
    painter.setPen(arrangeMode_ == ArrangeMode::TxnId
                       ? QColor(QStringLiteral("#FFFFFF"))
                       : QColor(QStringLiteral("#43505C")));
    painter.drawText(rightModeRect, Qt::AlignCenter, QStringLiteral("TxnID"));
    paintBuildProgress(painter);

    const QRect ruler = rulerRect();
    const QRect infoBar = infoBarRect();
    painter.fillRect(ruler, QColor(QStringLiteral("#FAFBFC")));
    painter.fillRect(infoBar, QColor(QStringLiteral("#F7F9FB")));
    painter.setPen(QColor(QStringLiteral("#D7DDE3")));
    painter.drawLine(ruler.topLeft(), ruler.topRight());
    painter.drawLine(QPoint(ruler.left(), ruler.bottom()), QPoint(ruler.right(), ruler.bottom()));
    painter.drawLine(infoBar.topLeft(), infoBar.topRight());
    painter.drawLine(QPoint(infoBar.left(), infoBar.bottom()), QPoint(infoBar.right(), infoBar.bottom()));

    painter.setFont(font());
    const QRect content = bodyRect().adjusted(kStatusPadding,
                                              kStatusPadding,
                                              -kStatusPadding,
                                              -kStatusPadding);
    if (spans_.empty()) {
        painter.setPen(QColor(QStringLiteral("#5A6673")));
        painter.drawText(content,
                         Qt::AlignCenter | Qt::TextWordWrap,
                         statusText_);
    } else {
        const QRect laneHeader = laneHeaderRect();
        const QRect plot = plotRect();
        const int firstLane = verticalScrollOffset_;
        const int lastLane = std::min(static_cast<int>(lanes_.size()),
                                      firstLane + visibleLaneCapacity() + 1);

        painter.fillRect(laneHeader, QColor(QStringLiteral("#F7F9FB")));
        painter.fillRect(plot, QColor(QStringLiteral("#FFFFFF")));
        painter.setPen(QColor(QStringLiteral("#CDD3DA")));
        painter.drawLine(laneHeader.topRight(), laneHeader.bottomRight());
        QFont laneTitleBadgeFont = transactionLaneHeaderTitleFont(font());
        QFont laneTitleNodeIdFont = transactionLaneHeaderTitleFont(font(), true);
        QFont laneTitleTextFont = transactionLaneHeaderTitleFont(font());
        QFont laneTextFont = transactionLaneHeaderFont(font(), rowHeight());

        const int selectedLaneIndex =
            selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())
                ? spans_[static_cast<std::size_t>(selectedSpanIndex_)].laneIndex
                : -1;
        int previousGroupIndex = -1;
        for (int laneIndex = firstLane; laneIndex < lastLane; ++laneIndex) {
    const int y = laneTopY(laneIndex, plot);
    const QRect laneHeaderRowRect(laneHeader.left(), y, laneHeader.width(), rowHeight());
    const QRect plotRowRect(plot.left(), y, plot.width(), rowHeight());
    const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    const bool firstGroupRow = lane.groupIndex != previousGroupIndex;
            previousGroupIndex = lane.groupIndex;
            const bool laneSelected = laneIndex == selectedLaneIndex;
            if (laneSelected) {
                painter.fillRect(laneHeaderRowRect, QColor(QStringLiteral("#EDF3FA")));
                painter.fillRect(plotRowRect, QColor(QStringLiteral("#EDF3FA")));
            } else if ((laneIndex % 2) == 1) {
                painter.fillRect(laneHeaderRowRect, QColor(QStringLiteral("#F8FAFC")));
                painter.fillRect(plotRowRect, QColor(QStringLiteral("#F8FAFC")));
            }
            painter.setPen(QColor(QStringLiteral("#E1E6EC")));
            painter.drawLine(plot.left(), y + rowHeight() - 1, plot.right(), y + rowHeight() - 1);
            if (firstGroupRow && laneIndex != firstLane) {
                painter.setPen(QColor(QStringLiteral("#B7C0CA")));
                painter.drawLine(laneHeader.left(), y, plot.right(), y);
            }

            const QColor accent = laneAccentColor(lane);
            painter.fillRect(QRect(laneHeader.left(), y, 3, rowHeight()), accent);
        }

        for (int laneIndex = firstLane; laneIndex < lastLane; ++laneIndex) {
            if (isFirstClassificationRow(laneIndex)) {
                continue;
            }
            painter.setFont(laneHeaderCountFont(laneIndex));
            painter.setPen(QColor(QStringLiteral("#7A8490")));
            painter.drawText(laneHeaderCountRect(laneIndex, laneHeader, plot),
                             Qt::AlignVCenter | Qt::AlignRight,
                             laneHeaderCountText(laneIndex));
        }

        painter.save();
        painter.setClipRect(laneHeader);
        const int titleOverlapRows =
            rowHeight() <= 0 ? 0 : static_cast<int>(std::ceil(static_cast<double>(laneHeaderTitleHeight())
                                                              / static_cast<double>(rowHeight())));
        const int titleOverlayFirstLane = std::max(0, firstLane - std::max(0, titleOverlapRows));
        for (int laneIndex = titleOverlayFirstLane; laneIndex < lastLane; ++laneIndex) {
            const int y = laneTopY(laneIndex, plot);
            const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
            const bool firstGroupRow = isFirstClassificationRow(laneIndex);
            const bool laneSelected = laneIndex == selectedLaneIndex;
            if (firstGroupRow) {
                const QRect titleRect = laneHeaderTitleRect(laneIndex, laneHeader, plot);
                painter.fillRect(titleRect,
                                 laneSelected ? QColor(QStringLiteral("#EDF3FA"))
                                              : QColor(QStringLiteral("#F7F9FB")));
                const QColor accent = laneAccentColor(lane);
                painter.fillRect(QRect(laneHeader.left(), titleRect.top(), 3, titleRect.height()), accent);
                painter.setPen(QColor(QStringLiteral("#E1E6EC")));
                painter.drawLine(titleRect.left(), titleRect.bottom(), titleRect.right(), titleRect.bottom());
            }
            const QRect nodeBadge = laneNodeBadgeRect(laneIndex, laneHeader, plot);
            if (nodeBadge.isValid()) {
                PaintChannelBadge(&painter,
                                  nodeBadge,
                                  laneTitleBadgeFont,
                                  lane.endpointNodeType,
                                  QString(),
                                  transactionNodeBadgeColor(lane.endpointNodeType),
                                  laneSelected,
                                  false,
                                  true);
            }

            const QRect nodeIdRect = laneNodeIdRect(laneIndex, laneHeader, plot);
            if (nodeIdRect.isValid()) {
                painter.setFont(laneTitleNodeIdFont);
                painter.setPen(laneSelected ? QColor(QStringLiteral("#18212B"))
                                            : QColor(QStringLiteral("#374151")));
                painter.drawText(nodeIdRect,
                                 Qt::AlignVCenter | Qt::AlignLeft,
                                 painter.fontMetrics().elidedText(lane.endpointNode,
                                                                  Qt::ElideRight,
                                                                  nodeIdRect.width()));
            }

            const QString channelKind = channelKindFromOpcodeOrOrigin(lane.requestOpcode, lane.originKind);
            const QRect channelBadge = laneChannelBadgeRect(laneIndex, laneHeader, plot);
            if (channelBadge.isValid()) {
                PaintChannelBadge(&painter,
                                  channelBadge,
                                  laneTitleBadgeFont,
                                  channelKind,
                                  QString(),
                                  channelEdgeColor(channelKind),
                                  laneSelected,
                                  false,
                                  true);
            }

            const int labelLeft = channelBadge.isValid()
                ? channelBadge.right() + 1 + kLaneHeaderBadgeGap
                : (nodeIdRect.isValid()
                       ? nodeIdRect.right() + 1 + kLaneHeaderBadgeGap
                       : (nodeBadge.isValid() ? nodeBadge.right() + 1 + kLaneHeaderBadgeGap : laneHeader.left() + 8));
            const int labelRight = laneHeader.right() - 66;
            const QRect labelRect(labelLeft,
                                  y,
                                  std::max(0, labelRight - labelLeft + 1),
                                  firstGroupRow ? laneHeaderTitleHeight() : rowHeight());
            const QString rowLabel = arrangeMode_ == ArrangeMode::TxnId
                ? (firstGroupRow ? lane.label : lane.label)
                : (firstGroupRow
                       ? QStringLiteral("depth %1").arg(lane.stackDepth)
                       : QStringLiteral("slot %1").arg(lane.stackSlot + 1));
            if (labelRect.width() > 10) {
                painter.setFont(firstGroupRow ? laneTitleTextFont : laneTextFont);
                painter.setPen(firstGroupRow ? QColor(QStringLiteral("#26313C"))
                                             : QColor(QStringLiteral("#596471")));
                painter.drawText(labelRect,
                                 Qt::AlignVCenter | Qt::AlignLeft,
                                 painter.fontMetrics().elidedText(rowLabel,
                                                                  Qt::ElideRight,
                                                                 labelRect.width()));
            }
            if (firstGroupRow) {
                painter.setFont(laneHeaderCountFont(laneIndex));
                painter.setPen(QColor(QStringLiteral("#7A8490")));
                painter.drawText(laneHeaderCountRect(laneIndex, laneHeader, plot),
                                 Qt::AlignVCenter | Qt::AlignRight,
                                 laneHeaderCountText(laneIndex));
            }
        }
        painter.restore();

        const qint64 visibleStart = visibleStartTimestamp();
        const qint64 visibleEnd = visibleEndTimestamp();
        const long double visibleRange =
            std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
        const int denseThreshold = plot.width() <= 0
            ? kDensePaintSpanThreshold
            : std::max(kDensePaintSpanThreshold, plot.width() * kDensePaintPixelsPerSpan + 1);
        const int visibleSpanTotal = visibleSpanCount(visibleStart, visibleEnd, denseThreshold);
        const bool dense = densePaintEnabledForVisible(plot, visibleSpanTotal);
        bool spansDrawnWithInlineLabels = false;
        painter.setClipRect(plot);
        painter.setRenderHint(QPainter::Antialiasing, false);
        const bool highlightFiltering =
            transactionFilterActive() && filterMode_ == TransactionFilterMode::Highlight;
        if (highlightFiltering && dense) {
            paintDenseFilteredOutSpans(painter,
                                       plot,
                                       visibleStart,
                                       visibleEnd,
                                       visibleRange,
                                       firstLane,
                                       lastLane);
        } else if (highlightFiltering) {
            const std::vector<int> graySpans = visibleSpanIndices(visibleStart, visibleEnd);
            paintSpanRectangles(painter,
                                graySpans,
                                plot,
                                visibleStart,
                                visibleEnd,
                                visibleRange,
                                false,
                                false);
        }
        const DenseSpanPaintResult densePaint =
            dense ? paintDenseSpans(painter, plot, visibleStart, visibleEnd, visibleRange, firstLane, lastLane)
                  : DenseSpanPaintResult{};
        const std::vector<int> visibleSpans =
            dense ? densePaint.preservedSpanIndices : visibleSpanIndices(visibleStart, visibleEnd);
        if (highlightFiltering) {
            std::vector<int> candidateSpans;
            candidateSpans.reserve(visibleSpans.size());
            for (const int spanIndex : visibleSpans) {
                if (!shouldDrawFilteredOutSpan(spanIndex)) {
                    candidateSpans.push_back(spanIndex);
                }
            }
            if (!candidateSpans.empty()) {
                paintSpanRectangles(painter,
                                    candidateSpans,
                                    plot,
                                    visibleStart,
                                    visibleEnd,
                                    visibleRange,
                                    true,
                                    !dense);
                spansDrawnWithInlineLabels = !dense;
            }
        } else if (!dense || !visibleSpans.empty()) {
            paintSpanRectangles(painter,
                                visibleSpans,
                                plot,
                                visibleStart,
                                visibleEnd,
                                visibleRange,
                                true,
                                !dense);
            spansDrawnWithInlineLabels = !dense;
        }
        paintSelectedOpcodeTags(painter,
                                plot,
                                visibleStart,
                                visibleEnd,
                                visibleRange,
                                spansDrawnWithInlineLabels);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setClipping(false);

        painter.setFont(rulerFont());
        const int tickCount = std::clamp(plot.width() / 140, 2, 8);
        painter.setClipRect(ruler);
        painter.setPen(QColor(QStringLiteral("#D7DDE3")));
        painter.drawLine(plot.left(), ruler.top(), plot.left(), ruler.bottom());
        for (int tick = 0; tick <= tickCount; ++tick) {
            const long double fraction = static_cast<long double>(tick) / static_cast<long double>(std::max(1, tickCount));
            const qint64 timestamp = clampedRoundToTimestamp(static_cast<long double>(visibleStart) + visibleRange * fraction);
            const int x = static_cast<int>(std::llround(timestampToPlotX(timestamp, plot)));
            painter.setPen(QColor(QStringLiteral("#9AA4AF")));
            painter.drawLine(QPoint(x, ruler.top() + 1), QPoint(x, ruler.top() + kTimeRulerTickHeight));
            painter.setPen(QColor(QStringLiteral("#3C4651")));
            painter.drawText(QRect(x - 55, ruler.top() + kTimeRulerTickHeight + 1, 110, ruler.height() - kTimeRulerTickHeight),
                             Qt::AlignTop | Qt::AlignHCenter,
                             FormatTimestampForDisplay(timestamp));
        }
        painter.setClipping(false);
        paintCursorRulerTag(painter, plot, ruler);
        paintBottomSpanSnap(painter, plot, ruler);

        painter.setFont(infoBarFont());
        QString detailText = statusText_;
        if (selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())) {
            detailText = spanStatusText(spans_[static_cast<std::size_t>(selectedSpanIndex_)]);
        } else if (hoveredSpanIndex_ >= 0 && hoveredSpanIndex_ < static_cast<int>(spans_.size())) {
            detailText = spanStatusText(spans_[static_cast<std::size_t>(hoveredSpanIndex_)]);
        }
        painter.setPen(QColor(QStringLiteral("#5A6673")));
        painter.drawText(infoBar.adjusted(laneHeaderWidth() + 8, 0, -8, 0),
                         Qt::AlignVCenter | Qt::AlignRight,
                         painter.fontMetrics().elidedText(detailText, Qt::ElideLeft, std::max(20, infoBar.width() - laneHeaderWidth() - 16)));
    }
    painter.setClipping(false);
    painter.setFont(infoBarFont());

    if (hasCursor_) {
        painter.setPen(QPen(QColor(QStringLiteral("#111111")), 1, Qt::DashLine));
        const QRect plot = plotRect();
        const int x = qBound(plot.left(), static_cast<int>(std::llround(timestampToPlotX(cursorTimestamp_, plot))), plot.right());
        painter.drawLine(QPoint(x, header.bottom() + 1), QPoint(x, ruler.top() - 1));
        painter.setPen(QColor(QStringLiteral("#18212B")));
        painter.drawText(infoBar.adjusted(8, 0, -8, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QStringLiteral("Cursor %1").arg(FormatTimestampForDisplay(cursorTimestamp_)));
    }

    if (hasMarker_) {
        painter.setPen(QPen(QColor(QStringLiteral("#2F80ED")), 1, Qt::SolidLine));
        const QRect plot = plotRect();
        const int x = qBound(plot.left(), static_cast<int>(std::llround(timestampToPlotX(markerTimestamp_, plot))), plot.right());
        painter.drawLine(QPoint(x, header.bottom() + 1), QPoint(x, ruler.top() - 1));
        painter.fillRect(QRect(x - 3, header.bottom() + 1, 7, 7), QColor(QStringLiteral("#2F80ED")));
        painter.setPen(QColor(QStringLiteral("#2F80ED")));
        painter.drawText(infoBar.adjusted(8, 0, -8, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QStringLiteral("Marker %1").arg(FormatTimestampForDisplay(markerTimestamp_)));
    }

    paintSharedMarkers(painter, plotRect(), rulerRect(), infoBarRect());

    if (hasSelectedLogicalRow()) {
        const QRect selectionTextRect = headerSelectionTextRect();
        if (selectionTextRect.isValid()) {
            const QString selectionText =
                QStringLiteral("Selected row %1")
                    .arg(FormatSignedIntegerWithThousands(selectedLogicalRow_));
            painter.setPen(QColor(QStringLiteral("#225C8C")));
            painter.drawText(selectionTextRect,
                             Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(selectionText,
                                                              Qt::ElideRight,
                                                              selectionTextRect.width()));
        }
    }

    paintRangeZoom(painter);
    paintGestureOverlay(painter);
    paintBuildPrompt(painter);
}

void TransactionWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    dragStart_ = event->position().toPoint();
    dragLast_ = dragStart_;

    if (hasPendingBuild()) {
        if (event->button() == Qt::LeftButton) {
            buildView();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
        return;
    }

    if ((event->button() == Qt::MiddleButton)
        || (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ShiftModifier))) {
        beginPan(dragStart_);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const QPoint position = event->position().toPoint();
        if (const std::optional<QString> markerId = sharedMarkerAtPosition(position)) {
            selectedMarkerId_ = *markerId;
            Q_EMIT markerSelected(*markerId);
            update();
            event->accept();
            return;
        }
        if (arrangeModeSwitchRect().contains(position)) {
            const QRect switchRect = arrangeModeSwitchRect();
            setArrangeMode(position.x() < switchRect.center().x()
                               ? ArrangeMode::InflightStack
                               : ArrangeMode::TxnId);
            event->accept();
            return;
        }
        if (const std::optional<OpcodeTagLayout> tagLayout = opcodeTagLayoutAtPosition(position)) {
            opcodeTagPressPending_ = true;
            dragMode_ = DragMode::PendingGesture;
            leftDragGestureActive_ = true;
            leftDragAnchor_ = position;
            selectOpcodeTagLayout(*tagLayout, true);
            hideHoverCard();
            updateMouseCursor();
            event->accept();
            return;
        }
        if (positionInPlotOrRuler(position)) {
            beginPendingGesture(position);
        } else {
            const int hitSpan = spanIndexAtPosition(position);
            if (hitSpan >= 0 && !shouldDrawFilteredOutSpan(hitSpan)) {
                setCursorFromPlotPosition(position, true);
                selectSpan(hitSpan, cursorTimestamp_);
            } else {
                setCursorFromPlotPosition(position, false);
            }
            updateMouseCursor();
            update();
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TransactionWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (activateOpcodeTagAtPosition(event->position().toPoint())) {
            event->accept();
            return;
        }
        const int hitSpan = spanIndexAtPosition(event->position().toPoint());
        if (hitSpan >= 0 && hitSpan < static_cast<int>(spans_.size())) {
            activateSpanRequestRow(hitSpan);
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton && hasSelectedLogicalRow()) {
        Q_EMIT logicalRowActivated(selectedLogicalRow_);
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TransactionWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    lastMousePosition_ = position;

    if (dragMode_ == DragMode::PendingGesture
        || dragMode_ == DragMode::RangeZoom
        || dragMode_ == DragMode::ZoomFull
        || dragMode_ == DragMode::ZoomIn2x
        || dragMode_ == DragMode::ZoomOut2x) {
        opcodeTagPressPending_ = false;
        hoveredSpanIndex_ = -1;
        updateLeftDragGesture(position, event->modifiers());
        event->accept();
        return;
    }

    if (dragMode_ == DragMode::Pan) {
        hoveredSpanIndex_ = -1;
        updatePan(position);
        event->accept();
        return;
    }

    if (event->buttons().testFlag(Qt::LeftButton)) {
        if (selectedOpcodeTagAnchorsCursor()) {
            stickCursorToSelectedOpcodeTag(false);
        } else {
            setCursorFromPlotPosition(position, true);
        }
        hideHoverCard();
        update();
    }
    if (!event->buttons()) {
        hoveredSpanIndex_ = spanIndexAtPosition(position);
        const std::optional<QString> markerId = sharedMarkerAtPosition(position);
        const QString nextHoveredMarkerId = markerId ? *markerId : QString();
        if (nextHoveredMarkerId != hoveredMarkerId_) {
            hoveredMarkerId_ = nextHoveredMarkerId;
            update();
        }
        armHoverCard(hoveredSpanIndex_, position);
    }
    updateMouseCursor();
    QWidget::mouseMoveEvent(event);
}

void TransactionWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton
        && (dragMode_ == DragMode::PendingGesture
            || dragMode_ == DragMode::RangeZoom
            || dragMode_ == DragMode::ZoomFull
            || dragMode_ == DragMode::ZoomIn2x
            || dragMode_ == DragMode::ZoomOut2x)) {
        updateLeftDragGesture(event->position().toPoint(), event->modifiers());

        const bool preserveOpcodeTagCursor =
            selectedOpcodeTagAnchorsCursor()
            && (opcodeTagPressPending_
                || (event->position().toPoint() - leftDragAnchor_).manhattanLength() >= kGestureActivationDistance);

        if (dragMode_ == DragMode::PendingGesture && preserveOpcodeTagCursor) {
            stickCursorToSelectedOpcodeTag(false);
        } else if (dragMode_ == DragMode::PendingGesture) {
            const QPoint position = event->position().toPoint();
            const qint64 requested = plotXToTimestamp(position.x(), plotRect());
            setCursorFromPlotPosition(position, true);
            const int hitSpan = spanIndexAtPosition(position);
            if (hitSpan >= 0) {
                selectSpan(hitSpan, requested);
            } else {
                clearSelectedOpcodeTag();
            }
        } else if (dragMode_ == DragMode::RangeZoom) {
            finishRangeZoom();
        } else if (dragMode_ == DragMode::ZoomFull) {
            finishZoomFullGesture();
        } else if (dragMode_ == DragMode::ZoomIn2x) {
            finishZoomIn2xGesture();
        } else if (dragMode_ == DragMode::ZoomOut2x) {
            finishZoomOut2xGesture();
        }

        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        opcodeTagPressPending_ = false;
        hideHoverCard();
        updateMouseCursor();
        update();
        event->accept();
        return;
    }

    if ((event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)
        && dragMode_ == DragMode::Pan) {
        dragMode_ = DragMode::None;
        leftDragGestureActive_ = false;
        opcodeTagPressPending_ = false;
        unsetCursor();
        hideHoverCard();
        updateMouseCursor();
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        lastMousePosition_ = event->position().toPoint();
        opcodeTagPressPending_ = false;
        hoveredSpanIndex_ = spanIndexAtPosition(lastMousePosition_);
        updateMouseCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void TransactionWidget::contextMenuEvent(QContextMenuEvent* event)
{
    hideHoverCard();
    const int hitSpan = spanIndexAtPosition(event->pos());
    if (hitSpan < 0 || hitSpan >= static_cast<int>(spans_.size())) {
        QWidget::contextMenuEvent(event);
        return;
    }

    selectSpan(hitSpan, plotXToTimestamp(event->pos().x(), plotRect()));
    QMenu menu(this);
    QAction* jumpRequest = menu.addAction(QStringLiteral("Jump to request row"));
    QAction* jumpCompletion = menu.addAction(QStringLiteral("Jump to completion row"));
    menu.addSeparator();
    QAction* relatedRows = menu.addAction(QStringLiteral("Show related rows"));
    QAction* debugInfo = menu.addAction(QStringLiteral("Show transaction debug info"));
    menu.addSeparator();
    QMenu* filterMenu = menu.addMenu(QStringLiteral("Transaction filtering"));
    std::vector<std::pair<QAction*, std::pair<TransactionFilterMode, TransactionFilterField>>> filterActions;
    const TransactionSpan& contextSpan = spans_[static_cast<std::size_t>(hitSpan)];
    const auto addFieldActions = [&](const TransactionFilterField field, const QString& label) {
        const QString value = filterValueForSpanField(contextSpan, field);
        if (value.isEmpty()) {
            return;
        }
        QAction* highlightAction =
            filterMenu->addAction(QStringLiteral("Highlight %1: %2").arg(label, value));
        filterActions.push_back(
            {highlightAction, {TransactionFilterMode::Highlight, field}});
        QAction* filterAction =
            filterMenu->addAction(QStringLiteral("Filter %1: %2").arg(label, value));
        filterActions.push_back(
            {filterAction, {TransactionFilterMode::Filter, field}});
    };
    addFieldActions(TransactionFilterField::Opcode, QStringLiteral("Opcode"));
    addFieldActions(TransactionFilterField::Address, QStringLiteral("Address"));
    addFieldActions(TransactionFilterField::TxnId, QStringLiteral("TxnID"));
    addFieldActions(TransactionFilterField::Dbid, QStringLiteral("DBID"));
    QAction* clearFilter = nullptr;
    if (requestedTransactionFilterActive()) {
        filterMenu->addSeparator();
        clearFilter = filterMenu->addAction(QStringLiteral("Clear transaction filter"));
    }
    filterMenu->setEnabled(!filterActions.empty() || clearFilter != nullptr);
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == jumpRequest) {
        activateSpanRequestRow(hitSpan);
    } else if (chosen == jumpCompletion) {
        activateSpanCompletionRow(hitSpan);
    } else if (chosen == relatedRows) {
        showSpanRelatedRows(hitSpan);
    } else if (chosen == debugInfo) {
        showSpanDebugInfo(hitSpan);
    } else if (clearFilter != nullptr && chosen == clearFilter) {
        clearTransactionFilter();
    } else {
        for (const auto& [action, filterAction] : filterActions) {
            if (chosen == action) {
                applySpanFilterAction(hitSpan, filterAction.first, filterAction.second);
                break;
            }
        }
    }
    event->accept();
}

void TransactionWidget::leaveEvent(QEvent* event)
{
    hoveredSpanIndex_ = -1;
    hoveredMarkerId_.clear();
    hideHoverCard();
    update();
    unsetCursor();
    QWidget::leaveEvent(event);
}

void TransactionWidget::keyPressEvent(QKeyEvent* event)
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

    const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);

    if (event->key() == Qt::Key_Escape) {
        if (dragMode_ != DragMode::None) {
            dragMode_ = DragMode::None;
            leftDragGestureActive_ = false;
            opcodeTagPressPending_ = false;
        }
        hasCursor_ = false;
        hoveredSpanIndex_ = -1;
        hideHoverCard();
        update();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_F || event->key() == Qt::Key_0)) {
        zoomToFit();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_T) {
        setArrangeMode(arrangeMode_ == ArrangeMode::TxnId
                           ? ArrangeMode::InflightStack
                           : ArrangeMode::TxnId);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        zoomByFactor(1.25, plotRect().center());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
        zoomByFactor(0.8, plotRect().center());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Left) {
        if (ctrl) {
            moveSelectionBySpan(-1);
        } else {
            panHorizontallyByPixels(shift ? -plotRect().width() / 2 : -80);
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right) {
        if (ctrl) {
            moveSelectionBySpan(1);
        } else {
            panHorizontallyByPixels(shift ? plotRect().width() / 2 : 80);
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Up) {
        panVerticallyByRows(shift ? -6 : -1);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Down) {
        panVerticallyByRows(shift ? 6 : 1);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_PageUp) {
        panHorizontallyByPixels(-std::max(1, plotRect().width() - 40));
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_PageDown) {
        panHorizontallyByPixels(std::max(1, plotRect().width() - 40));
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Home) {
        if (ctrl) {
            zoomToFit();
        }
        scrollOffset_ = 0;
        updateScrollBars();
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_End) {
        const auto [fullStart, fullEnd] = fullTimestampRange();
        const long double fullRange =
            std::max<long double>(1.0L, static_cast<long double>(fullEnd) - static_cast<long double>(fullStart));
        const long double visibleRange = fullRange / std::max(kMinZoom, horizontalZoom_);
        scrollOffset_ = static_cast<std::uint64_t>(std::max<long double>(0.0L, fullRange - visibleRange));
        updateScrollBars();
        update();
        event->accept();
        return;
    }
    if (ctrl && event->key() == Qt::Key_B) {
        restorePreviousView();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_M && selectedSpanIndex_ >= 0) {
        setMarkerFromSelectedSpan();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && (hasSelectedLogicalRow() || selectedSpanIndex_ >= 0)) {
        if (selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())) {
            activateSpanRequestRow(selectedSpanIndex_);
        } else {
            Q_EMIT logicalRowActivated(selectedLogicalRow_);
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void TransactionWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    hideHoverCard();
    updateScrollBars();
    update();
}

void TransactionWidget::wheelEvent(QWheelEvent* event)
{
    if (hasPendingBuild()) {
        event->ignore();
        return;
    }

    const QPoint pixelDelta = event->pixelDelta();
    const QPoint angleDelta = event->angleDelta();
    const int verticalDelta = pixelDelta.y() != 0 ? pixelDelta.y() : angleDelta.y();
    const int horizontalDelta = pixelDelta.x() != 0 ? pixelDelta.x() : angleDelta.x();
    const bool controlPressed = event->modifiers().testFlag(Qt::ControlModifier);
    const bool shiftPressed = event->modifiers().testFlag(Qt::ShiftModifier);

    if (controlPressed && shiftPressed) {
        const int delta = verticalDelta == 0 ? horizontalDelta : verticalDelta;
        if (delta != 0) {
            adjustRowHeightFromWheel(delta);
            event->accept();
            return;
        }
    }

    if (controlPressed) {
        hideHoverCard();
        zoomByFactor(verticalDelta >= 0 ? 1.25 : 0.8, event->position().toPoint());
        event->accept();
        return;
    }

    if (shiftPressed) {
        hideHoverCard();
        const int delta = verticalDelta == 0 ? horizontalDelta : verticalDelta;
        panHorizontallyByPixels(delta < 0 ? plotRect().width() / 4 : -plotRect().width() / 4);
        event->accept();
        return;
    }

    if (event->modifiers().testFlag(Qt::AltModifier)) {
        hideHoverCard();
        const int delta = verticalDelta == 0 ? horizontalDelta : verticalDelta;
        panHorizontallyByPixels(delta < 0 ? 48 : -48);
        event->accept();
        return;
    }

    if (horizontalDelta != 0) {
        hideHoverCard();
        panHorizontallyByPixels(horizontalDelta < 0 ? 80 : -80);
        event->accept();
        return;
    }

    if (verticalDelta != 0) {
        hideHoverCard();
        panVerticallyByRows(verticalDelta < 0 ? 3 : -3);
        event->accept();
        return;
    }

    event->ignore();
}

QSize TransactionWidget::minimumSizeHint() const
{
    return QSize(420, 180);
}

QSize TransactionWidget::sizeHint() const
{
    return QSize(900, 260);
}

void TransactionWidget::updateStatusText()
{
    switch (sourceMode_) {
    case SourceMode::Empty:
        statusText_ = QStringLiteral("No transaction trace is loaded.");
        break;
    case SourceMode::TraceSession:
        if (!traceSession_) {
            statusText_ = QStringLiteral("No transaction trace is loaded.");
        } else if (!traceSession_->supportsXactionIndexing()) {
            statusText_ = QStringLiteral("This trace does not support xaction indexing.");
        } else if (!traceSession_->isXactionIndexStarted()) {
            statusText_ = QStringLiteral("Build the xaction index to display transaction timelines.");
        } else if (!traceSession_->isXactionIndexComplete()) {
            statusText_ = QStringLiteral("Xaction index is building. Transaction timelines will appear when it completes.");
        } else if (filtering_) {
            statusText_ = filterProgressText();
        } else if (building_) {
            statusText_ = totalXactions_ == 0
                ? QStringLiteral("Building transaction spans...")
                : QStringLiteral("Building transaction spans: %1 / %2")
                      .arg(FormatUnsignedIntegerWithThousands(processedXactions_),
                           FormatUnsignedIntegerWithThousands(totalXactions_));
        } else if (!spans_.empty()) {
            if (transactionFilterActive()) {
                const std::size_t totalSpans = unfilteredSpans_.empty() ? spans_.size() : unfilteredSpans_.size();
                statusText_ = QStringLiteral("Transaction filter: %1 of %2 spans matching across %3 stack rows.")
                                  .arg(FormatUnsignedIntegerWithThousands(filterMatchCount_),
                                       FormatUnsignedIntegerWithThousands(totalSpans),
                                       FormatUnsignedIntegerWithThousands(lanes_.size()));
            } else {
                statusText_ = QStringLiteral("Transaction spans ready: %1 spans across %2 stack rows.")
                                  .arg(FormatUnsignedIntegerWithThousands(spans_.size()),
                                       FormatUnsignedIntegerWithThousands(lanes_.size()));
            }
        } else if (!buildRequested_) {
            statusText_ = pendingBuildText();
        } else {
            statusText_ = QStringLiteral("Transaction timeline data is ready for span building.");
        }
        break;
    case SourceMode::RowSnapshot:
        if (filtering_) {
            statusText_ = filterProgressText();
        } else if (!spans_.empty()) {
            if (transactionFilterActive()) {
                const std::size_t totalSpans = unfilteredSpans_.empty() ? spans_.size() : unfilteredSpans_.size();
                statusText_ = QStringLiteral("Transaction filter: %1 of %2 spans matching across %3 stack rows.")
                                  .arg(FormatUnsignedIntegerWithThousands(filterMatchCount_),
                                       FormatUnsignedIntegerWithThousands(totalSpans),
                                       FormatUnsignedIntegerWithThousands(lanes_.size()));
            } else {
                statusText_ = QStringLiteral("Transaction spans ready: %1 spans across %2 stack rows.")
                                  .arg(FormatUnsignedIntegerWithThousands(spans_.size()),
                                       FormatUnsignedIntegerWithThousands(lanes_.size()));
            }
        } else {
            statusText_ = rows_.empty()
                ? QStringLiteral("No transaction rows are loaded.")
                : QStringLiteral("Row snapshot loaded. Xaction span building will be added in the next stage.");
        }
        break;
    }
}

void TransactionWidget::updateWidgetHints()
{
    QString hint = QString::fromLatin1(kTransactionWidgetInteractionHint);
    if (!statusText_.isEmpty()) {
        hint = statusText_ + QStringLiteral("\n\n") + hint;
    }
    if (selectedSpanIndex_ >= 0 && selectedSpanIndex_ < static_cast<int>(spans_.size())) {
        hint += QStringLiteral("\n\nSelected: %1")
                    .arg(spanStatusText(spans_[static_cast<std::size_t>(selectedSpanIndex_)]));
    }
    if (filtering_) {
        hint += QStringLiteral("\n\nFilter: working in background.");
    }
    if (transactionFilterActive()) {
        hint += QStringLiteral("\n\nFilter: %1 mode, %2 / %3 matching transactions.")
                    .arg(filterMode_ == TransactionFilterMode::Filter ? QStringLiteral("Filter")
                                                                       : QStringLiteral("Highlight"),
                         FormatUnsignedIntegerWithThousands(filterMatchCount_),
                         FormatUnsignedIntegerWithThousands(unfilteredSpans_.empty()
                                                                ? spans_.size()
                                                                : unfilteredSpans_.size()));
    }
    setToolTip(QString());
    setStatusTip(hint);
    horizontalScrollBar_->setToolTip(QStringLiteral("Pan the Transaction time range horizontally."));
    horizontalScrollBar_->setStatusTip(horizontalScrollBar_->toolTip());
    verticalScrollBar_->setToolTip(QStringLiteral("Scroll Transaction inflight stack rows by node and REQ/SNP group."));
    if (arrangeMode_ == ArrangeMode::TxnId) {
        verticalScrollBar_->setToolTip(QStringLiteral("Scroll Transaction rows arranged by node, REQ/SNP group, and TxnID."));
    }
    verticalScrollBar_->setStatusTip(verticalScrollBar_->toolTip());
}

void TransactionWidget::updateMouseCursor()
{
    if (dragMode_ == DragMode::Pan) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    const QPoint position = lastMousePosition_;
    if (opcodeTagIndexAtPosition(position) >= 0 || spanIndexAtPosition(position) >= 0) {
        setCursor(Qt::PointingHandCursor);
    } else if (positionInPlotOrRuler(position)) {
        setCursor(Qt::CrossCursor);
    } else {
        unsetCursor();
    }
}

bool TransactionWidget::hasSelectedLogicalRow() const noexcept
{
    return selectedLogicalRow_ >= 0;
}

#ifdef CHIRON_GUI_ENABLE_TRANSACTION_TEST_API
int TransactionWidget::testSpanCount() const noexcept
{
    return static_cast<int>(spans_.size());
}

int TransactionWidget::testLaneCount() const noexcept
{
    return static_cast<int>(lanes_.size());
}

QVariantMap TransactionWidget::testSpanAt(const int index) const
{
    QVariantMap spanMap;
    if (index < 0 || index >= static_cast<int>(spans_.size())) {
        return spanMap;
    }
    const TransactionSpan& span = spans_[static_cast<std::size_t>(index)];
    spanMap.insert(QStringLiteral("ordinal"), QVariant::fromValue<qulonglong>(span.ordinal));
    spanMap.insert(QStringLiteral("transactionGroupKey"), span.transactionGroupKey);
    spanMap.insert(QStringLiteral("xactionType"), span.xactionType);
    spanMap.insert(QStringLiteral("requestOpcode"), span.requestOpcode);
    QStringList opcodeValues;
    opcodeValues.reserve(static_cast<qsizetype>(span.opcodeValues.size()));
    for (const QString& value : span.opcodeValues) {
        opcodeValues.push_back(value);
    }
    spanMap.insert(QStringLiteral("opcodeValues"), opcodeValues);
    spanMap.insert(QStringLiteral("addressLabel"), span.addressLabel);
    spanMap.insert(QStringLiteral("jointFamily"), span.jointFamily);
    spanMap.insert(QStringLiteral("jointPath"), span.jointPath);
    spanMap.insert(QStringLiteral("endpointLabel"), span.endpointLabel);
    spanMap.insert(QStringLiteral("endpointNode"), span.endpointNode);
    spanMap.insert(QStringLiteral("endpointNodeType"), span.endpointNodeType);
    spanMap.insert(QStringLiteral("originKind"), span.originKind);
    spanMap.insert(QStringLiteral("txnIdLabel"), span.txnIdLabel);
    spanMap.insert(QStringLiteral("dbidLabel"), span.dbidLabel);
    spanMap.insert(QStringLiteral("classificationConfidence"), span.classificationConfidence);
    spanMap.insert(QStringLiteral("laneIndex"), span.laneIndex);
    spanMap.insert(QStringLiteral("groupIndex"), span.groupIndex);
    spanMap.insert(QStringLiteral("stackSlot"), span.stackSlot);
    spanMap.insert(QStringLiteral("stackDepth"), span.stackDepth);
    spanMap.insert(QStringLiteral("firstLogicalRow"), QVariant::fromValue<qulonglong>(span.firstLogicalRow));
    spanMap.insert(QStringLiteral("lastLogicalRow"), QVariant::fromValue<qulonglong>(span.lastLogicalRow));
    spanMap.insert(QStringLiteral("requestLogicalRow"), QVariant::fromValue<qulonglong>(span.requestLogicalRow));
    spanMap.insert(QStringLiteral("completionLogicalRow"),
                   span.completionLogicalRow
                       ? QVariant::fromValue<qulonglong>(*span.completionLogicalRow)
                       : QVariant());
    spanMap.insert(QStringLiteral("startTimestamp"), span.startTimestamp);
    spanMap.insert(QStringLiteral("endTimestamp"), span.endTimestamp);
    spanMap.insert(QStringLiteral("rowCount"), QVariant::fromValue<qulonglong>(span.rowCount));
    spanMap.insert(QStringLiteral("completed"), span.completed);
    spanMap.insert(QStringLiteral("indexed"), span.indexed);
    spanMap.insert(QStringLiteral("processedByJoint"), span.processedByJoint);
    spanMap.insert(QStringLiteral("filterMatch"), displayedSpanMatchesFilter(index));
    return spanMap;
}

QVariantMap TransactionWidget::testLaneAt(const int index) const
{
    QVariantMap laneMap;
    if (index < 0 || index >= static_cast<int>(lanes_.size())) {
        return laneMap;
    }
    const TransactionLane& lane = lanes_[static_cast<std::size_t>(index)];
    laneMap.insert(QStringLiteral("key"), lane.key);
    laneMap.insert(QStringLiteral("label"), lane.label);
    laneMap.insert(QStringLiteral("groupKey"), lane.groupKey);
    laneMap.insert(QStringLiteral("groupLabel"), lane.groupLabel);
    laneMap.insert(QStringLiteral("jointFamily"), lane.jointFamily);
    laneMap.insert(QStringLiteral("endpointLabel"), lane.endpointLabel);
    laneMap.insert(QStringLiteral("endpointNode"), lane.endpointNode);
    laneMap.insert(QStringLiteral("endpointNodeType"), lane.endpointNodeType);
    laneMap.insert(QStringLiteral("originKind"), lane.originKind);
    laneMap.insert(QStringLiteral("xactionType"), lane.xactionType);
    laneMap.insert(QStringLiteral("requestOpcode"), lane.requestOpcode);
    laneMap.insert(QStringLiteral("txnIdLabel"), lane.txnIdLabel);
    laneMap.insert(QStringLiteral("spanCount"), QVariant::fromValue<qulonglong>(lane.spanCount));
    laneMap.insert(QStringLiteral("groupIndex"), lane.groupIndex);
    laneMap.insert(QStringLiteral("stackSlot"), lane.stackSlot);
    laneMap.insert(QStringLiteral("stackDepth"), lane.stackDepth);
    laneMap.insert(QStringLiteral("groupSpanCount"), QVariant::fromValue<qulonglong>(lane.groupSpanCount));
    return laneMap;
}

QString TransactionWidget::testStatusText() const
{
    return statusText_;
}

QVariantMap TransactionWidget::testLayoutState() const
{
    QVariantMap state;
    const QRect header = headerRect();
    const QRect laneHeader = laneHeaderRect();
    const QRect plot = plotRect();
    const QRect ruler = rulerRect();
    const QRect infoBar = infoBarRect();
    state.insert(QStringLiteral("headerHeight"), header.height());
    state.insert(QStringLiteral("laneHeaderWidth"), laneHeader.width());
    state.insert(QStringLiteral("plotLeft"), plot.left());
    state.insert(QStringLiteral("plotTop"), plot.top());
    state.insert(QStringLiteral("plotWidth"), plot.width());
    state.insert(QStringLiteral("plotHeight"), plot.height());
    state.insert(QStringLiteral("rulerTop"), ruler.top());
    state.insert(QStringLiteral("rulerHeight"), ruler.height());
    state.insert(QStringLiteral("infoBarTop"), infoBar.top());
    state.insert(QStringLiteral("infoBarHeight"), infoBar.height());
    state.insert(QStringLiteral("rowHeight"), rowHeight());
    state.insert(QStringLiteral("visibleLaneCapacity"), visibleLaneCapacity());
    state.insert(QStringLiteral("verticalScrollOffset"), verticalScrollOffset_);
    state.insert(QStringLiteral("horizontalScrollOffset"), QVariant::fromValue<qulonglong>(scrollOffset_));
    state.insert(QStringLiteral("horizontalZoom"), horizontalZoom_);
    state.insert(QStringLiteral("stackRowCount"), stackRowCount());
    state.insert(QStringLiteral("groupCount"), transactionGroupCount());
    state.insert(QStringLiteral("maxStackDepth"), maxStackDepth());
    state.insert(QStringLiteral("arrangeMode"), arrangeModeName(arrangeMode_));
    state.insert(QStringLiteral("filterMode"), filterModeName(requestedFilterMode_));
    state.insert(QStringLiteral("filterActive"), requestedTransactionFilterActive());
    state.insert(QStringLiteral("filterMatchCount"), QVariant::fromValue<qulonglong>(filterMatchCount_));
    state.insert(QStringLiteral("unfilteredSpanCount"), static_cast<int>(unfilteredSpans_.size()));
    state.insert(QStringLiteral("displayingFilteredSpans"), displayingFilteredSpans_);
    state.insert(QStringLiteral("dragMode"), dragModeName(dragMode_));
    state.insert(QStringLiteral("gestureActive"), leftDragGestureActive_);
    state.insert(QStringLiteral("densePaint"), densePaintEnabled(plot));
    return state;
}

std::pair<qint64, qint64> TransactionWidget::testVisibleTimestampRange() const
{
    return {visibleStartTimestamp(), visibleEndTimestamp()};
}

double TransactionWidget::testHorizontalZoom() const noexcept
{
    return horizontalZoom_;
}

QPoint TransactionWidget::testSpanCenterPoint(const int index) const
{
    const QRect rect = spanVisualRect(index, plotRect());
    return rect.isValid() ? rect.center() : QPoint();
}

QString TransactionWidget::testSpanFillColorName(const int index) const
{
    if (index < 0 || index >= static_cast<int>(spans_.size())) {
        return {};
    }
    return spanFillColor(spans_[static_cast<std::size_t>(index)]).name(QColor::HexRgb).toUpper();
}

QString TransactionWidget::testSpanEdgeColorName(const int index) const
{
    if (index < 0 || index >= static_cast<int>(spans_.size())) {
        return {};
    }
    return spanEdgeColor(spans_[static_cast<std::size_t>(index)]).name(QColor::HexRgb).toUpper();
}

QRect TransactionWidget::testLaneNodeBadgeRect(const int laneIndex) const
{
    return laneNodeBadgeRect(laneIndex, laneHeaderRect(), plotRect());
}

QRect TransactionWidget::testLaneNodeIdRect(const int laneIndex) const
{
    return laneNodeIdRect(laneIndex, laneHeaderRect(), plotRect());
}

QRect TransactionWidget::testLaneChannelBadgeRect(const int laneIndex) const
{
    return laneChannelBadgeRect(laneIndex, laneHeaderRect(), plotRect());
}

QRect TransactionWidget::testLaneHeaderCountRect(const int laneIndex) const
{
    return laneHeaderCountRect(laneIndex, laneHeaderRect(), plotRect());
}

QString TransactionWidget::testLaneNodeBadgeColorName(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return {};
    }
    return transactionNodeBadgeColor(lanes_[static_cast<std::size_t>(laneIndex)].endpointNodeType)
        .name(QColor::HexRgb)
        .toUpper();
}

QString TransactionWidget::testLaneChannelBadgeColorName(const int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes_.size())) {
        return {};
    }
    const TransactionLane& lane = lanes_[static_cast<std::size_t>(laneIndex)];
    const QString channelKind = channelKindFromOpcodeOrOrigin(lane.requestOpcode, lane.originKind);
    return channelEdgeColor(channelKind).name(QColor::HexRgb).toUpper();
}

int TransactionWidget::testSelectedOpcodeTagCount() const
{
    const QRect plot = plotRect();
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    return static_cast<int>(selectedOpcodeTagLayouts(plot, visibleStart, visibleEnd, visibleRange, true).size());
}

QVariantMap TransactionWidget::testSelectedOpcodeTagAt(const int index) const
{
    QVariantMap tagMap;
    const QRect plot = plotRect();
    const qint64 visibleStart = visibleStartTimestamp();
    const qint64 visibleEnd = visibleEndTimestamp();
    const long double visibleRange =
        std::max<long double>(1.0L, static_cast<long double>(visibleEnd) - static_cast<long double>(visibleStart));
    const std::vector<OpcodeTagLayout> layouts =
        selectedOpcodeTagLayouts(plot, visibleStart, visibleEnd, visibleRange, true);
    if (index < 0 || index >= static_cast<int>(layouts.size())) {
        return tagMap;
    }

    const OpcodeTagLayout& layout = layouts[static_cast<std::size_t>(index)];
    tagMap.insert(QStringLiteral("logicalRow"), QVariant::fromValue<qulonglong>(layout.info.logicalRow));
    tagMap.insert(QStringLiteral("timestamp"), layout.info.timestamp);
    tagMap.insert(QStringLiteral("timestampX"), layout.timestampX);
    tagMap.insert(QStringLiteral("text"), layout.text);
    tagMap.insert(QStringLiteral("opcode"), layout.info.opcode);
    tagMap.insert(QStringLiteral("fullWidth"), layout.fullWidth);
    tagMap.insert(QStringLiteral("color"), layout.color.name(QColor::HexRgb).toUpper());
    tagMap.insert(QStringLiteral("rect"), layout.tagRect);
    tagMap.insert(QStringLiteral("selected"), layout.selected);
    tagMap.insert(QStringLiteral("hasBlackEdge"), layout.selected);
    tagMap.insert(QStringLiteral("edgeColor"), layout.selected
                                            ? QColor(QStringLiteral("#000000")).name(QColor::HexRgb).toUpper()
                                            : QString());
    tagMap.insert(QStringLiteral("lineRect"), layout.lineRect);
    tagMap.insert(QStringLiteral("lineVisible"), layout.lineVisible);
    tagMap.insert(QStringLiteral("triangleBounds"), layout.triangleBounds);
    tagMap.insert(QStringLiteral("triangleVisible"), layout.triangleVisible);
    tagMap.insert(QStringLiteral("inlineTextRect"), layout.inlineTextRect);
    tagMap.insert(QStringLiteral("bottomPlacement"), layout.bottomPlacement);
    tagMap.insert(QStringLiteral("triangleX0"), layout.trianglePoints[0].x());
    tagMap.insert(QStringLiteral("triangleY0"), layout.trianglePoints[0].y());
    tagMap.insert(QStringLiteral("triangleX1"), layout.trianglePoints[1].x());
    tagMap.insert(QStringLiteral("triangleY1"), layout.trianglePoints[1].y());
    tagMap.insert(QStringLiteral("triangleX2"), layout.trianglePoints[2].x());
    tagMap.insert(QStringLiteral("triangleY2"), layout.trianglePoints[2].y());
    return tagMap;
}

int TransactionWidget::testSelectedOpcodeTagFontPixelSize() const
{
    return opcodeTagFont().pixelSize();
}

bool TransactionWidget::testShowOpcodeTags() const noexcept
{
    return showOpcodeTags_;
}

bool TransactionWidget::testShowHoverCards() const noexcept
{
    return showHoverCards_;
}

int TransactionWidget::testSpanIndexAtPoint(const QPoint& position) const
{
    return spanIndexAtPosition(position);
}

bool TransactionWidget::testUsesDensePaint() const
{
    return densePaintEnabled(plotRect());
}

int TransactionWidget::testSelectedLogicalRow() const noexcept
{
    return selectedLogicalRow_;
}

int TransactionWidget::testSelectedSpanIndex() const noexcept
{
    return selectedSpanIndex_;
}

bool TransactionWidget::testHasCursor() const noexcept
{
    return hasCursor_;
}

qint64 TransactionWidget::testCursorTimestamp() const noexcept
{
    return cursorTimestamp_;
}

bool TransactionWidget::testHasMarker() const noexcept
{
    return hasMarker_;
}

int TransactionWidget::testMarkerLogicalRow() const noexcept
{
    return markerLogicalRow_;
}

std::size_t TransactionWidget::testLastBuildWorkerCount() const noexcept
{
    return lastBuildWorkerCount_;
}

bool TransactionWidget::testGestureOverlayVisible() const noexcept
{
    return leftDragGestureActive_
        && (dragMode_ == DragMode::PendingGesture
            || dragMode_ == DragMode::ZoomFull
            || dragMode_ == DragMode::ZoomIn2x
            || dragMode_ == DragMode::ZoomOut2x)
        && (dragLast_ - dragStart_).manhattanLength() >= kGestureActivationDistance;
}

QString TransactionWidget::testDragModeName() const
{
    return dragModeName(dragMode_);
}

QRect TransactionWidget::testPlotRect() const
{
    return plotRect();
}

QRect TransactionWidget::testRulerRect() const
{
    return rulerRect();
}

QRect TransactionWidget::testInfoBarRect() const
{
    return infoBarRect();
}

QRect TransactionWidget::testBuildProgressRect() const
{
    return buildProgressRect();
}

QRect TransactionWidget::testCursorRulerTagRect() const
{
    if (!hasCursor_) {
        return {};
    }
    QFont tagFont(rulerFont());
    tagFont.setBold(true);
    if (tagFont.pointSizeF() > 0.0) {
        tagFont.setPointSizeF(qMax<qreal>(7.0, tagFont.pointSizeF() * 0.86));
    } else {
        tagFont.setPixelSize(qMax(8, qRound(static_cast<qreal>(tagFont.pixelSize()) * 0.86)));
    }
    return cursorRulerTagRect(cursorTimestamp_, plotRect(), rulerRect(), QFontMetrics(tagFont));
}

QRect TransactionWidget::testSpanVisualRect(const int spanIndex) const
{
    return spanVisualRect(spanIndex, plotRect());
}

QRect TransactionWidget::testLaneHeaderTitleRect(const int laneIndex) const
{
    return laneHeaderTitleRect(laneIndex, laneHeaderRect(), plotRect());
}

bool TransactionWidget::testHoverCardVisible() const noexcept
{
    return hoverCard_ && hoverCard_->isVisible();
}

QRect TransactionWidget::testHoverCardRect() const
{
    return hoverCard_ ? hoverCard_->geometry() : QRect();
}

int TransactionWidget::testHoverCardRowCount() const noexcept
{
    return hoverCardTable_ ? hoverCardTable_->rowCount() : 0;
}

QString TransactionWidget::testHoverCardTitle() const
{
    return hoverCardTitle_ ? hoverCardTitle_->text() : QString();
}

QString TransactionWidget::testHoverCardCellText(const int row, const int column) const
{
    if (!hoverCardTable_
        || row < 0
        || row >= hoverCardTable_->rowCount()
        || column < 0
        || column >= hoverCardTable_->columnCount()) {
        return {};
    }
    const QTableWidgetItem* item = hoverCardTable_->item(row, column);
    return item ? item->text() : QString();
}

bool TransactionWidget::testHoverCardDwellActive() const noexcept
{
    return hoverCardDwellTimer_ && hoverCardDwellTimer_->isActive();
}

void TransactionWidget::testSetBuildProgress(const std::uint64_t completedWork,
                                             const std::uint64_t totalWork)
{
    building_ = true;
    buildProgressPhase_ = BuildProgressPhase::ScanningRows;
    buildProgressCompleted_ = totalWork == 0 ? completedWork : std::min(completedWork, totalWork);
    buildProgressTotal_ = totalWork;
    statusText_ = buildProgressText();
    updateWidgetHints();
    update();
}

QString TransactionWidget::testInlineLabelForSpan(const int spanIndex, const int availableWidth) const
{
    if (spanIndex < 0 || spanIndex >= static_cast<int>(spans_.size())) {
        return {};
    }
    QFont addressFont = transactionSpanLabelFont(font(), rowHeight());
    addressFont.setBold(false);
    QFont opcodeFont = addressFont;
    opcodeFont.setBold(true);
    return spanInlineLabel(spans_[static_cast<std::size_t>(spanIndex)],
                           availableWidth,
                           QFontMetrics(opcodeFont),
                           QFontMetrics(addressFont));
}

QString TransactionWidget::testSpanLabelFontFamily() const
{
    return transactionSpanLabelFont(font(), rowHeight()).family();
}

int TransactionWidget::testSpanLabelFontPixelSize() const
{
    return transactionSpanLabelFont(font(), rowHeight()).pixelSize();
}

int TransactionWidget::testLaneHeaderTitleFontPixelSize() const
{
    return transactionLaneHeaderTitleFont(font()).pixelSize();
}

int TransactionWidget::testLaneHeaderRowFontPixelSize() const
{
    return transactionLaneHeaderFont(font(), rowHeight()).pixelSize();
}

int TransactionWidget::testLaneHeaderCountFontPixelSize(const int laneIndex) const
{
    return laneHeaderCountFont(laneIndex).pixelSize();
}

int TransactionWidget::testRulerFontMetricHeight() const
{
    return QFontMetrics(rulerFont()).height();
}

int TransactionWidget::testInfoBarFontMetricHeight() const
{
    return QFontMetrics(infoBarFont()).height();
}

QString TransactionWidget::testArrangeModeName() const
{
    return arrangeModeName(arrangeMode_);
}

QRect TransactionWidget::testArrangeModeSwitchRect() const
{
    return arrangeModeSwitchRect();
}

QRect TransactionWidget::testFilterToolbarRect() const
{
    return filterToolbar_ ? filterToolbar_->geometry() : QRect();
}

QRect TransactionWidget::testHeaderSelectionTextRect() const
{
    return headerSelectionTextRect();
}

void TransactionWidget::testSetArrangeModeByName(const QString& modeName)
{
    setArrangeMode(arrangeModeFromName(modeName));
}

int TransactionWidget::testVisiblePaintCandidateCount() const
{
    return static_cast<int>(visibleSpanIndices(visibleStartTimestamp(), visibleEndTimestamp()).size());
}

int TransactionWidget::testFilterMatchCount() const noexcept
{
    return static_cast<int>(filterMatchCount_);
}

int TransactionWidget::testUnfilteredSpanCount() const noexcept
{
    return static_cast<int>(unfilteredSpans_.size());
}

QString TransactionWidget::testFilterModeName() const
{
    return filterModeName(requestedFilterMode_);
}

QRect TransactionWidget::testFilterModeComboRect() const
{
    return filterModeCombo_ ? QRect(filterModeCombo_->mapTo(this, QPoint(0, 0)),
                                    filterModeCombo_->size()) : QRect();
}

QRect TransactionWidget::testOpcodeFilterEditRect() const
{
    return opcodeFilterEdit_ ? QRect(opcodeFilterEdit_->mapTo(this, QPoint(0, 0)),
                                     opcodeFilterEdit_->size()) : QRect();
}

QRect TransactionWidget::testAddressFilterEditRect() const
{
    return addressFilterEdit_ ? QRect(addressFilterEdit_->mapTo(this, QPoint(0, 0)),
                                      addressFilterEdit_->size()) : QRect();
}

QRect TransactionWidget::testTxnIdFilterEditRect() const
{
    return txnIdFilterEdit_ ? QRect(txnIdFilterEdit_->mapTo(this, QPoint(0, 0)),
                                    txnIdFilterEdit_->size()) : QRect();
}

QRect TransactionWidget::testDbidFilterEditRect() const
{
    return dbidFilterEdit_ ? QRect(dbidFilterEdit_->mapTo(this, QPoint(0, 0)),
                                   dbidFilterEdit_->size()) : QRect();
}

QRect TransactionWidget::testOpcodeTagsCheckBoxRect() const
{
    return opcodeTagsCheckBox_ ? QRect(opcodeTagsCheckBox_->mapTo(this, QPoint(0, 0)),
                                       opcodeTagsCheckBox_->size()) : QRect();
}

QRect TransactionWidget::testHoverCardsCheckBoxRect() const
{
    return hoverCardsCheckBox_ ? QRect(hoverCardsCheckBox_->mapTo(this, QPoint(0, 0)),
                                       hoverCardsCheckBox_->size()) : QRect();
}

void TransactionWidget::testSetFilterModeByName(const QString& modeName)
{
    setFilterMode(filterModeFromName(modeName));
}

void TransactionWidget::testSetTransactionFilter(const QString& opcode,
                                                 const QString& address,
                                                 const QString& txnId,
                                                 const QString& dbid)
{
    setFilterCriteria(TransactionFilterCriteria{
        opcode.trimmed(),
        address.trimmed(),
        txnId.trimmed(),
        dbid.trimmed(),
    });
}

QVariantMap TransactionWidget::testFilterState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("mode"), filterModeName(requestedFilterMode_));
    state.insert(QStringLiteral("opcode"), requestedFilterCriteria_.opcode);
    state.insert(QStringLiteral("address"), requestedFilterCriteria_.address);
    state.insert(QStringLiteral("txnId"), requestedFilterCriteria_.txnId);
    state.insert(QStringLiteral("dbid"), requestedFilterCriteria_.dbid);
    state.insert(QStringLiteral("active"), requestedTransactionFilterActive());
    state.insert(QStringLiteral("filtering"), filtering_);
    state.insert(QStringLiteral("matchCount"), QVariant::fromValue<qulonglong>(filterMatchCount_));
    state.insert(QStringLiteral("unfilteredSpanCount"), static_cast<int>(unfilteredSpans_.size()));
    state.insert(QStringLiteral("displayingFilteredSpans"), displayingFilteredSpans_);
    return state;
}

void TransactionWidget::testInjectSyntheticSpans(const int laneCount, const int spanCount)
{
    stopBuildThread();
    traceSession_.reset();
    rows_.clear();
    clearSpanData();
    lastBuildWorkerCount_ = 1;
    sourceMode_ = SourceMode::RowSnapshot;
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    hasCursor_ = false;
    hasMarker_ = false;
    markerTimestamp_ = 0;
    markerLogicalRow_ = -1;
    markerSpanIndex_ = -1;
    selectedLogicalRow_ = -1;
    selectedSpanIndex_ = -1;
    hoveredSpanIndex_ = -1;
    dragMode_ = DragMode::None;
    leftDragGestureActive_ = false;
    opcodeTagPressPending_ = false;
    viewHistory_.clear();

    const int lanesToCreate = std::max(1, laneCount);
    const int spansToCreate = std::max(0, spanCount);
    lanes_.reserve(static_cast<std::size_t>(lanesToCreate));
    for (int laneIndex = 0; laneIndex < lanesToCreate; ++laneIndex) {
        TransactionLane lane;
        lane.key = QStringLiteral("synthetic-lane-%1").arg(laneIndex);
        lane.groupKey = QStringLiteral("synthetic-group-%1").arg(laneIndex);
        lane.groupLabel = QStringLiteral("RN %1 / REQ").arg(laneIndex);
        lane.label = lane.groupLabel;
        lane.jointFamily = QStringLiteral("RNFJoint");
        lane.endpointLabel = QStringLiteral("RN %1").arg(laneIndex);
        lane.endpointNode = QString::number(laneIndex);
        lane.endpointNodeType = QStringLiteral("RN-F");
        lane.originKind = QStringLiteral("REQ");
        lane.xactionType = QStringLiteral("Synthetic");
        lane.requestOpcode = QStringLiteral("REQ ReadNoSnp");
        lane.groupIndex = laneIndex;
        lane.stackSlot = 0;
        lane.stackDepth = 1;
        lane.groupSpanCount = 0;
        lanes_.push_back(std::move(lane));
    }

    spans_.reserve(static_cast<std::size_t>(spansToCreate));
    for (int spanIndex = 0; spanIndex < spansToCreate; ++spanIndex) {
        const int laneIndex = spanIndex % lanesToCreate;
        const qint64 start = static_cast<qint64>(spanIndex) * 8;
        TransactionSpan span;
        span.ordinal = static_cast<std::uint64_t>(spanIndex);
        span.transactionGroupKey = QStringLiteral("synthetic|%1").arg(spanIndex);
        span.xactionType = QStringLiteral("Synthetic");
        span.requestOpcode = QStringLiteral("REQ ReadNoSnp");
        span.opcodeValues = {span.requestOpcode, QStringLiteral("ReadNoSnp")};
        span.addressLabel = QStringLiteral("0x%1")
                                .arg(QString::number(static_cast<qulonglong>(0x1000 + spanIndex * 64),
                                                     16)
                                         .toUpper()
                                         .rightJustified(12, QLatin1Char('0')));
        span.jointFamily = QStringLiteral("RNFJoint");
        span.jointPath = QStringLiteral("RNFJoint::Synthetic");
        span.endpointLabel = lanes_[static_cast<std::size_t>(laneIndex)].endpointLabel;
        span.endpointNode = lanes_[static_cast<std::size_t>(laneIndex)].endpointNode;
        span.endpointNodeType = lanes_[static_cast<std::size_t>(laneIndex)].endpointNodeType;
        span.originKind = QStringLiteral("REQ");
        span.txnIdLabel = QString::number(spanIndex % 16);
        span.dbidLabel = QString::number(32 + (spanIndex % 8));
        span.classificationConfidence = QStringLiteral("synthetic test data");
        span.laneKey = lanes_[static_cast<std::size_t>(laneIndex)].key;
        span.laneIndex = laneIndex;
        span.groupIndex = laneIndex;
        span.stackSlot = 0;
        span.stackDepth = 1;
        span.firstLogicalRow = static_cast<std::uint64_t>(spanIndex * 3);
        span.requestLogicalRow = span.firstLogicalRow;
        span.lastLogicalRow = span.firstLogicalRow + 2;
        span.completionLogicalRow = span.lastLogicalRow;
        span.startTimestamp = start;
        span.endTimestamp = start + 5 + (spanIndex % 7);
        span.rowCount = 3;
        span.completed = true;
        span.indexed = true;
        span.processedByJoint = true;
        span.txnIdValues = {span.txnIdLabel};
        span.dbidValues = {span.dbidLabel};
        span.logicalRows = {span.firstLogicalRow, span.firstLogicalRow + 1, span.lastLogicalRow};
        spans_.push_back(std::move(span));
        ++lanes_[static_cast<std::size_t>(laneIndex)].spanCount;
        ++lanes_[static_cast<std::size_t>(laneIndex)].groupSpanCount;
    }

    statusText_ = QStringLiteral("Transaction spans ready: %1 spans across %2 stack rows.")
                      .arg(FormatUnsignedIntegerWithThousands(spans_.size()),
                           FormatUnsignedIntegerWithThousands(lanes_.size()));
    unfilteredLanes_ = lanes_;
    unfilteredSpans_ = spans_;
    unfilteredSpanFilterMatches_.assign(unfilteredSpans_.size(), 1);
    spanFilterMatches_.assign(spans_.size(), 1);
    filterMatchCount_ = static_cast<std::uint64_t>(spans_.size());
    displayingFilteredSpans_ = false;
    rebuildSpanPaintIndex();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::testInjectMixedDensitySyntheticSpans()
{
    testInjectSyntheticSpans(4, 2500);
    for (int spanIndex = 0; spanIndex < static_cast<int>(spans_.size()); ++spanIndex) {
        TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
        if (spanIndex == 2401 || spanIndex == 2402) {
            span.laneIndex = 1;
            span.groupIndex = 1;
            span.laneKey = lanes_[1].key;
            span.endpointLabel = lanes_[1].endpointLabel;
            span.endpointNode = lanes_[1].endpointNode;
            span.endpointNodeType = lanes_[1].endpointNodeType;
            span.startTimestamp = spanIndex == 2401 ? 100000 : 30000;
            span.endTimestamp = spanIndex == 2401 ? 120000 : 30001;
        } else if (spanIndex == 2403) {
            span.laneIndex = 2;
            span.groupIndex = 2;
            span.laneKey = lanes_[2].key;
            span.endpointLabel = lanes_[2].endpointLabel;
            span.endpointNode = lanes_[2].endpointNode;
            span.endpointNodeType = lanes_[2].endpointNodeType;
            span.startTimestamp = 20000;
            span.endTimestamp = 80000;
        } else if (spanIndex == 2404) {
            span.laneIndex = 3;
            span.groupIndex = 3;
            span.laneKey = lanes_[3].key;
            span.endpointLabel = lanes_[3].endpointLabel;
            span.endpointNode = lanes_[3].endpointNode;
            span.endpointNodeType = lanes_[3].endpointNodeType;
            span.startTimestamp = 52000;
            span.endTimestamp = 54000;
        } else if (spanIndex >= 2410 && spanIndex < 2426) {
            span.laneIndex = 2;
            span.groupIndex = 2;
            span.laneKey = lanes_[2].key;
            span.endpointLabel = lanes_[2].endpointLabel;
            span.endpointNode = lanes_[2].endpointNode;
            span.endpointNodeType = lanes_[2].endpointNodeType;
            span.startTimestamp = 20000 + static_cast<qint64>((spanIndex - 2410) % 4);
            span.endTimestamp = span.startTimestamp + 12;
        } else if (spanIndex >= 2430 && spanIndex < 2446) {
            span.laneIndex = 2;
            span.groupIndex = 2;
            span.laneKey = lanes_[2].key;
            span.endpointLabel = lanes_[2].endpointLabel;
            span.endpointNode = lanes_[2].endpointNode;
            span.endpointNodeType = lanes_[2].endpointNodeType;
            span.startTimestamp = 79980 + static_cast<qint64>((spanIndex - 2430) % 4);
            span.endTimestamp = span.startTimestamp + 12;
        } else if (spanIndex >= 2450 && spanIndex < 2466) {
            span.laneIndex = 3;
            span.groupIndex = 3;
            span.laneKey = lanes_[3].key;
            span.endpointLabel = lanes_[3].endpointLabel;
            span.endpointNode = lanes_[3].endpointNode;
            span.endpointNodeType = lanes_[3].endpointNodeType;
            span.startTimestamp = 52000 + static_cast<qint64>((spanIndex - 2450) % 4);
            span.endTimestamp = span.startTimestamp + 20;
        } else {
            span.laneIndex = 0;
            span.groupIndex = 0;
            span.laneKey = lanes_[0].key;
            span.endpointLabel = lanes_[0].endpointLabel;
            span.endpointNode = lanes_[0].endpointNode;
            span.endpointNodeType = lanes_[0].endpointNodeType;
            span.startTimestamp = static_cast<qint64>(1000 + (spanIndex % 32));
            span.endTimestamp = span.startTimestamp + 8;
        }
    }

    for (TransactionLane& lane : lanes_) {
        lane.spanCount = 0;
        lane.groupSpanCount = 0;
    }
    for (const TransactionSpan& span : spans_) {
        if (span.laneIndex >= 0 && span.laneIndex < static_cast<int>(lanes_.size())) {
            ++lanes_[static_cast<std::size_t>(span.laneIndex)].spanCount;
            ++lanes_[static_cast<std::size_t>(span.laneIndex)].groupSpanCount;
        }
    }

    unfilteredLanes_ = lanes_;
    unfilteredSpans_ = spans_;
    unfilteredSpanFilterMatches_.assign(unfilteredSpans_.size(), 1);
    spanFilterMatches_.assign(spans_.size(), 1);
    filterMatchCount_ = static_cast<std::uint64_t>(spans_.size());
    displayingFilteredSpans_ = false;
    rebuildSpanPaintIndex();
    updateScrollBars();
    update();
}

void TransactionWidget::testInjectOpcodeTagSyntheticSpans()
{
    testInjectSyntheticSpans(12, 12);
    rows_.clear();
    rows_.resize(32);
    if (spans_.empty()) {
        return;
    }

    TransactionSpan& selectedSpan = spans_[0];
    selectedSpan.ordinal = 100;
    selectedSpan.laneIndex = 1;
    selectedSpan.groupIndex = 1;
    selectedSpan.laneKey = lanes_[1].key;
    selectedSpan.endpointLabel = lanes_[1].endpointLabel;
    selectedSpan.endpointNode = lanes_[1].endpointNode;
    selectedSpan.endpointNodeType = lanes_[1].endpointNodeType;
    selectedSpan.firstLogicalRow = 0;
    selectedSpan.requestLogicalRow = 0;
    selectedSpan.lastLogicalRow = 8;
    selectedSpan.completionLogicalRow = 8;
    selectedSpan.startTimestamp = 100;
    selectedSpan.endTimestamp = 220;
    selectedSpan.rowCount = 5;
    selectedSpan.requestOpcode = QStringLiteral("REQ ReadNoSnp");
    selectedSpan.opcodeValues = {selectedSpan.requestOpcode, QStringLiteral("ReadNoSnp")};
    selectedSpan.addressLabel = QStringLiteral("0x00000000C0DE");
    selectedSpan.logicalRows = {0, 2, 4, 6, 8};

    const struct {
        std::uint64_t logicalRow = 0;
        qint64 timestamp = 0;
        FlitChannel channel = FlitChannel::Req;
        QString opcode;
    } records[] = {
        {0, 100, FlitChannel::Req, QStringLiteral("ReadNoSnp")},
        {2, 116, FlitChannel::Rsp, QStringLiteral("CompDBIDRespReallyLongName")},
        {4, 118, FlitChannel::Dat, QStringLiteral("DataSepResp")},
        {6, 170, FlitChannel::Snp, QStringLiteral("SnpOnce")},
        {8, 220, FlitChannel::Dat, QStringLiteral("CompData")},
    };
    for (const auto& recordSpec : records) {
        FlitRecord& record = rows_[static_cast<std::size_t>(recordSpec.logicalRow)];
        record.timestamp = recordSpec.timestamp;
        record.channel = recordSpec.channel;
        record.direction = FlitDirection::Tx;
        record.opcode = recordSpec.opcode;
        record.source = QStringLiteral("0");
        record.target = QStringLiteral("1");
        record.txnId = QStringLiteral("7");
        record.address = selectedSpan.addressLabel;
    }

    for (int spanIndex = 1; spanIndex < static_cast<int>(spans_.size()); ++spanIndex) {
        TransactionSpan& span = spans_[static_cast<std::size_t>(spanIndex)];
        span.laneIndex = 0;
        span.groupIndex = 0;
        span.laneKey = lanes_[0].key;
        span.endpointLabel = lanes_[0].endpointLabel;
        span.endpointNode = lanes_[0].endpointNode;
        span.endpointNodeType = lanes_[0].endpointNodeType;
        span.startTimestamp = 105 + spanIndex * 15;
        span.endTimestamp = span.startTimestamp + 40;
    }

    for (TransactionLane& lane : lanes_) {
        lane.spanCount = 0;
        lane.groupSpanCount = 0;
    }
    for (const TransactionSpan& span : spans_) {
        if (span.laneIndex >= 0 && span.laneIndex < static_cast<int>(lanes_.size())) {
            ++lanes_[static_cast<std::size_t>(span.laneIndex)].spanCount;
            ++lanes_[static_cast<std::size_t>(span.laneIndex)].groupSpanCount;
        }
    }

    unfilteredLanes_ = lanes_;
    unfilteredSpans_ = spans_;
    unfilteredSpanFilterMatches_.assign(unfilteredSpans_.size(), 1);
    spanFilterMatches_.assign(spans_.size(), 1);
    filterMatchCount_ = static_cast<std::uint64_t>(spans_.size());
    displayingFilteredSpans_ = false;
    rebuildSpanPaintIndex();
    updateScrollBars();
    update();
}

void TransactionWidget::testConfigureOpcodeTagSyntheticSpan(const bool snpOrigin, const bool congested)
{
    if (spans_.empty() || rows_.empty()) {
        return;
    }

    TransactionSpan& selectedSpan = spans_[0];
    selectedSpan.originKind = snpOrigin ? QStringLiteral("SNP") : QStringLiteral("REQ");
    selectedSpan.requestOpcode = snpOrigin ? QStringLiteral("SNP SnpMakeInvalid") : QStringLiteral("REQ ReadNoSnp");
    selectedSpan.opcodeValues = {selectedSpan.requestOpcode, opcodeWithoutChannelPrefix(selectedSpan.requestOpcode)};
    if (snpOrigin && lanes_.size() > 2) {
        selectedSpan.laneIndex = 2;
        selectedSpan.groupIndex = lanes_[2].groupIndex;
        selectedSpan.laneKey = lanes_[2].key;
        selectedSpan.endpointLabel = lanes_[2].endpointLabel;
        selectedSpan.endpointNode = lanes_[2].endpointNode;
        selectedSpan.endpointNodeType = lanes_[2].endpointNodeType;
        lanes_[2].originKind = QStringLiteral("SNP");
        lanes_[2].groupLabel = QStringLiteral("RN 2 / SNP");
        lanes_[2].label = lanes_[2].groupLabel;
        lanes_[2].requestOpcode = selectedSpan.requestOpcode;
    }

    if (selectedSpan.requestLogicalRow < rows_.size()) {
        FlitRecord& requestRecord = rows_[static_cast<std::size_t>(selectedSpan.requestLogicalRow)];
        requestRecord.channel = snpOrigin ? FlitChannel::Snp : FlitChannel::Req;
        requestRecord.opcode = snpOrigin ? QStringLiteral("SnpMakeInvalid") : QStringLiteral("ReadNoSnp");
        requestRecord.timestamp = selectedSpan.startTimestamp;
    }

    if (congested) {
        selectedSpan.endTimestamp = selectedSpan.startTimestamp + 36;
        selectedSpan.lastLogicalRow = 10;
        selectedSpan.completionLogicalRow = 10;
        selectedSpan.rowCount = 6;
        selectedSpan.logicalRows = {0, 2, 4, 6, 8, 10};
        rows_.resize(std::max<std::size_t>(rows_.size(), 12));
        const struct {
            std::uint64_t logicalRow = 0;
            qint64 timestampOffset = 0;
            FlitChannel channel = FlitChannel::Req;
            QString opcode;
        } records[] = {
            {0, 0, snpOrigin ? FlitChannel::Snp : FlitChannel::Req, snpOrigin ? QStringLiteral("SnpMakeInvalid") : QStringLiteral("ReadNoSnp")},
            {2, 10, FlitChannel::Rsp, QStringLiteral("Resp")},
            {4, 20, FlitChannel::Dat, QStringLiteral("Data")},
            {6, 30, FlitChannel::Snp, QStringLiteral("SnpOnce")},
            {8,
             40,
             FlitChannel::Dat,
             QStringLiteral("CompDataReallyLongResponseForIndependentOpcodeTagElisionAcrossTheEntireVisibleTransactionPlot")},
            {10, 50, FlitChannel::Rsp, QStringLiteral("RetryAck")},
        };
        for (const auto& recordSpec : records) {
            FlitRecord& record = rows_[static_cast<std::size_t>(recordSpec.logicalRow)];
            record.timestamp = selectedSpan.startTimestamp + recordSpec.timestampOffset;
            record.channel = recordSpec.channel;
            record.direction = FlitDirection::Tx;
            record.opcode = recordSpec.opcode;
            record.source = QStringLiteral("0");
            record.target = QStringLiteral("1");
            record.txnId = QStringLiteral("7");
            record.address = selectedSpan.addressLabel;
        }
    } else if (!snpOrigin) {
        selectedSpan.endTimestamp = selectedSpan.startTimestamp + 1000;
        selectedSpan.lastLogicalRow = 8;
        selectedSpan.completionLogicalRow = 8;
        selectedSpan.rowCount = 5;
        selectedSpan.logicalRows = {0, 2, 4, 6, 8};
        rows_.resize(std::max<std::size_t>(rows_.size(), 10));
        const struct {
            std::uint64_t logicalRow = 0;
            qint64 timestampOffset = 0;
            FlitChannel channel = FlitChannel::Req;
            QString opcode;
        } records[] = {
            {0, 0, FlitChannel::Req, QStringLiteral("ReadNoSnp")},
            {2, 180, FlitChannel::Rsp, QStringLiteral("CompDBIDRespReallyLongName")},
            {4, 360, FlitChannel::Dat, QStringLiteral("DataSepResp")},
            {6, 720, FlitChannel::Snp, QStringLiteral("SnpOnce")},
            {8, 1000, FlitChannel::Dat, QStringLiteral("CompData")},
        };
        for (const auto& recordSpec : records) {
            FlitRecord& record = rows_[static_cast<std::size_t>(recordSpec.logicalRow)];
            record.timestamp = selectedSpan.startTimestamp + recordSpec.timestampOffset;
            record.channel = recordSpec.channel;
            record.direction = FlitDirection::Tx;
            record.opcode = recordSpec.opcode;
            record.source = QStringLiteral("0");
            record.target = QStringLiteral("1");
            record.txnId = QStringLiteral("7");
            record.address = selectedSpan.addressLabel;
        }
    }

    selectedSpan.startTimestamp = rows_[static_cast<std::size_t>(selectedSpan.requestLogicalRow)].timestamp;
    if (selectedSpan.completionLogicalRow
        && *selectedSpan.completionLogicalRow < rows_.size()
        && rows_[static_cast<std::size_t>(*selectedSpan.completionLogicalRow)].timestamp > selectedSpan.startTimestamp) {
        selectedSpan.endTimestamp = rows_[static_cast<std::size_t>(*selectedSpan.completionLogicalRow)].timestamp;
    }

    unfilteredLanes_ = lanes_;
    unfilteredSpans_ = spans_;
    unfilteredSpanFilterMatches_.assign(unfilteredSpans_.size(), 1);
    spanFilterMatches_.assign(spans_.size(), 1);
    filterMatchCount_ = static_cast<std::uint64_t>(spans_.size());
    displayingFilteredSpans_ = false;
    rebuildSpanPaintIndex();
    updateWidgetHints();
    updateScrollBars();
    update();
}

bool TransactionWidget::testSetTraceSessionAndWaitForSpans(std::shared_ptr<TraceSession> session,
                                                           const int timeoutMs)
{
    setTraceSession(std::move(session));
    buildView();

    QElapsedTimer waitTimer;
    waitTimer.start();
    while (building_
           && (timeoutMs <= 0 || waitTimer.elapsed() < timeoutMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return !building_ && !spans_.empty();
}

QVariantMap TransactionWidget::testBuildSpansFromTraceSession(std::shared_ptr<TraceSession> session)
{
    stopBuildThread();
    traceSession_ = session;
    sourceMode_ = session ? SourceMode::TraceSession : SourceMode::Empty;
    clearSpanData();
    lastBuildWorkerCount_ = 1;

    const quint64 generation = ++buildGeneration_;
    std::shared_ptr<BuildResult> result =
        buildSpansFromSession(session, generation, arrangeMode_, std::stop_token{});

    QVariantMap summary;
    if (!result) {
        summary.insert(QStringLiteral("failed"), true);
        summary.insert(QStringLiteral("status"), QStringLiteral("Transaction span build returned no result."));
        return summary;
    }

    summary.insert(QStringLiteral("failed"), result->failed);
    summary.insert(QStringLiteral("cancelled"), result->cancelled);
    summary.insert(QStringLiteral("status"), result->statusText);
    summary.insert(QStringLiteral("spanCount"), static_cast<int>(result->spans.size()));
    summary.insert(QStringLiteral("laneCount"), static_cast<int>(result->lanes.size()));
    summary.insert(QStringLiteral("workerCount"), QVariant::fromValue<qulonglong>(result->workerCount));
    summary.insert(QStringLiteral("metadataMs"), static_cast<double>(result->metadataNs) / 1000000.0);
    summary.insert(QStringLiteral("scanMs"), static_cast<double>(result->scanNs) / 1000000.0);
    summary.insert(QStringLiteral("mergeMs"), static_cast<double>(result->mergeNs) / 1000000.0);
    summary.insert(QStringLiteral("firstInfoMs"), static_cast<double>(result->firstInfoNs) / 1000000.0);
    summary.insert(QStringLiteral("spanMs"), static_cast<double>(result->spanBuildNs) / 1000000.0);
    summary.insert(QStringLiteral("sortMs"), static_cast<double>(result->sortNs) / 1000000.0);
    summary.insert(QStringLiteral("layoutMs"), static_cast<double>(result->layoutNs) / 1000000.0);

    if (!result->failed && !result->cancelled) {
        applyBuildResult(result);
    }
    return summary;
}

void TransactionWidget::testInjectOverlappingSyntheticSpans()
{
    stopBuildThread();
    traceSession_.reset();
    rows_.clear();
    clearSpanData();
    lastBuildWorkerCount_ = 1;
    sourceMode_ = SourceMode::RowSnapshot;
    horizontalZoom_ = 1.0;
    scrollOffset_ = 0;
    verticalScrollOffset_ = 0;
    hasCursor_ = false;
    hasMarker_ = false;
    markerTimestamp_ = 0;
    markerLogicalRow_ = -1;
    markerSpanIndex_ = -1;
    selectedLogicalRow_ = -1;
    selectedSpanIndex_ = -1;
    hoveredSpanIndex_ = -1;
    dragMode_ = DragMode::None;
    leftDragGestureActive_ = false;
    viewHistory_.clear();

    auto addLane = [this](const QString& jointFamily,
                          const QString& groupKey,
                          const QString& groupLabel,
                          const QString& endpointLabel,
                          const QString& endpointNode,
                          const QString& originKind,
                          const int groupIndex,
                          const int stackSlot,
                          const int stackDepth,
                          const std::uint64_t groupSpanCount) {
        TransactionLane lane;
        lane.key = laneKeyFor(jointFamily, endpointLabel, originKind, stackSlot);
        lane.groupKey = groupKey;
        lane.groupLabel = groupLabel;
        lane.label = stackSlot == 0 ? groupLabel : QStringLiteral("slot %1").arg(stackSlot + 1);
        lane.jointFamily = jointFamily;
        lane.endpointLabel = endpointLabel;
        lane.endpointNode = endpointNode;
        lane.endpointNodeType = jointFamily == QLatin1String("SNFJoint")
            ? QStringLiteral("SN-F")
            : (endpointNode == QLatin1String("unknown") ? QStringLiteral("Node") : QStringLiteral("RN-F"));
        lane.originKind = originKind;
        lane.xactionType = QStringLiteral("Synthetic");
        lane.requestOpcode = originKind == QLatin1String("SNP")
            ? QStringLiteral("SNP SnpOnce")
            : QStringLiteral("REQ ReadNoSnp");
        lane.groupIndex = groupIndex;
        lane.stackSlot = stackSlot;
        lane.stackDepth = stackDepth;
        lane.groupSpanCount = groupSpanCount;
        lanes_.push_back(std::move(lane));
    };

    const QString rn0ReqKey = groupKeyFor(QStringLiteral("RNFJoint"), QStringLiteral("RN 0"), QStringLiteral("REQ"));
    const QString rn1SnpKey = groupKeyFor(QStringLiteral("RNFJoint"), QStringLiteral("RN 1"), QStringLiteral("SNP"));
    const QString unknownKey = groupKeyFor(QStringLiteral("Unknown Joint"),
                                           QStringLiteral("Unknown endpoint"),
                                           QStringLiteral("Unknown"));
    addLane(QStringLiteral("RNFJoint"), rn0ReqKey, QStringLiteral("RN 0 / REQ"), QStringLiteral("RN 0"), QStringLiteral("0"), QStringLiteral("REQ"), 0, 0, 2, 3);
    addLane(QStringLiteral("RNFJoint"), rn0ReqKey, QStringLiteral("RN 0 / REQ"), QStringLiteral("RN 0"), QStringLiteral("0"), QStringLiteral("REQ"), 0, 1, 2, 3);
    addLane(QStringLiteral("RNFJoint"), rn1SnpKey, QStringLiteral("RN 1 / SNP"), QStringLiteral("RN 1"), QStringLiteral("1"), QStringLiteral("SNP"), 1, 0, 1, 1);
    addLane(QStringLiteral("Unknown Joint"), unknownKey, QStringLiteral("Unknown endpoint / Unknown"), QStringLiteral("Unknown endpoint"), QStringLiteral("unknown"), QStringLiteral("Unknown"), 2, 0, 1, 1);

    struct SyntheticSpanSpec {
        int laneIndex = 0;
        int groupIndex = 0;
        int stackSlot = 0;
        int stackDepth = 1;
        qint64 start = 0;
        qint64 end = 0;
        QString originKind;
        QString requestOpcode;
        QString address;
    };
    const std::array<SyntheticSpanSpec, 5> specs = {
        SyntheticSpanSpec{0, 0, 0, 2, 0, 20, QStringLiteral("REQ"), QStringLiteral("REQ ReadNoSnp"), QStringLiteral("0x000000001000")},
        SyntheticSpanSpec{1, 0, 1, 2, 5, 12, QStringLiteral("REQ"), QStringLiteral("REQ ReadUnique"), QStringLiteral("0x000000002000")},
        SyntheticSpanSpec{0, 0, 0, 2, 21, 30, QStringLiteral("REQ"), QStringLiteral("REQ WriteBackFull"), QStringLiteral("0x000000003000")},
        SyntheticSpanSpec{2, 1, 0, 1, 8, 24, QStringLiteral("SNP"), QStringLiteral("SNP SnpOnce"), QStringLiteral("0x000000004000")},
        SyntheticSpanSpec{3, 2, 0, 1, 12, 18, QStringLiteral("Unknown"), QStringLiteral("Unknown"), QString()},
    };

    for (int spanIndex = 0; spanIndex < static_cast<int>(specs.size()); ++spanIndex) {
        const SyntheticSpanSpec& spec = specs[static_cast<std::size_t>(spanIndex)];
        TransactionSpan span;
        span.ordinal = static_cast<std::uint64_t>(spanIndex + 1);
        span.transactionGroupKey = QStringLiteral("synthetic-overlap|%1").arg(span.ordinal);
        span.xactionType = QStringLiteral("Synthetic");
        span.requestOpcode = spec.requestOpcode;
        span.opcodeValues = {span.requestOpcode, opcodeWithoutChannelPrefix(span.requestOpcode)};
        span.addressLabel = spec.address;
        span.jointFamily = lanes_[static_cast<std::size_t>(spec.laneIndex)].jointFamily;
        span.jointPath = span.jointFamily + QStringLiteral("::SyntheticOverlap");
        span.endpointLabel = lanes_[static_cast<std::size_t>(spec.laneIndex)].endpointLabel;
        span.endpointNode = lanes_[static_cast<std::size_t>(spec.laneIndex)].endpointNode;
        span.endpointNodeType = lanes_[static_cast<std::size_t>(spec.laneIndex)].endpointNodeType;
        span.originKind = spec.originKind;
        span.txnIdLabel = QString::number(spanIndex + 1);
        span.dbidLabel = spanIndex == 0
            ? QStringLiteral("9")
            : (spanIndex == 1 ? QStringLiteral("13") : QString());
        span.classificationConfidence = QStringLiteral("synthetic overlap layout");
        span.laneKey = lanes_[static_cast<std::size_t>(spec.laneIndex)].key;
        span.laneIndex = spec.laneIndex;
        span.groupIndex = spec.groupIndex;
        span.stackSlot = spec.stackSlot;
        span.stackDepth = spec.stackDepth;
        span.firstLogicalRow = static_cast<std::uint64_t>(spanIndex * 10);
        span.requestLogicalRow = span.firstLogicalRow;
        span.lastLogicalRow = span.firstLogicalRow + 2;
        span.completionLogicalRow = span.lastLogicalRow;
        span.startTimestamp = spec.start;
        span.endTimestamp = spec.end;
        span.rowCount = 3;
        span.completed = true;
        span.indexed = true;
        span.processedByJoint = true;
        span.txnIdValues = {span.txnIdLabel};
        if (!span.dbidLabel.isEmpty()) {
            span.dbidValues = {span.dbidLabel};
        }
        span.logicalRows = {span.firstLogicalRow, span.firstLogicalRow + 1, span.lastLogicalRow};
        spans_.push_back(std::move(span));
        ++lanes_[static_cast<std::size_t>(spec.laneIndex)].spanCount;
    }

    statusText_ = QStringLiteral("Transaction spans ready: %1 spans across %2 stack rows.")
                      .arg(FormatUnsignedIntegerWithThousands(spans_.size()),
                           FormatUnsignedIntegerWithThousands(lanes_.size()));
    unfilteredLanes_ = lanes_;
    unfilteredSpans_ = spans_;
    unfilteredSpanFilterMatches_.assign(unfilteredSpans_.size(), 1);
    spanFilterMatches_.assign(spans_.size(), 1);
    filterMatchCount_ = static_cast<std::uint64_t>(spans_.size());
    displayingFilteredSpans_ = false;
    rebuildSpanPaintIndex();
    updateWidgetHints();
    updateScrollBars();
    update();
}

void TransactionWidget::testInjectInterleavedSyntheticSpans()
{
    testInjectOverlappingSyntheticSpans();
    if (spans_.size() < 2) {
        return;
    }

    TransactionSpan& longSpan = spans_[0];
    longSpan.firstLogicalRow = 0;
    longSpan.requestLogicalRow = 0;
    longSpan.lastLogicalRow = 30;
    longSpan.completionLogicalRow = 30;
    longSpan.startTimestamp = 0;
    longSpan.endTimestamp = 30;
    longSpan.logicalRows = {0, 10, 30};

    TransactionSpan& nestedSpan = spans_[1];
    nestedSpan.firstLogicalRow = 5;
    nestedSpan.requestLogicalRow = 5;
    nestedSpan.lastLogicalRow = 12;
    nestedSpan.completionLogicalRow = 12;
    nestedSpan.startTimestamp = 5;
    nestedSpan.endTimestamp = 12;
    nestedSpan.logicalRows = {5, 6, 12};

    if (unfilteredSpans_.size() >= spans_.size()) {
        unfilteredSpans_ = spans_;
    }
    rebuildSpanPaintIndex();
    updateScrollBars();
    update();
}

void TransactionWidget::testSetHorizontalZoom(const double zoom)
{
    horizontalZoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
    updateScrollBars();
    update();
}

void TransactionWidget::testSetHorizontalScroll(const int value)
{
    const auto [fullStart, fullEnd] = fullTimestampRange();
    const std::uint64_t maxOffset = transactionHorizontalScrollRange(fullStart, fullEnd, horizontalZoom_);
    scrollOffset_ = std::min<std::uint64_t>(static_cast<std::uint64_t>(std::max(0, value)), maxOffset);
    updateScrollBars();
    update();
}

void TransactionWidget::testScaleSyntheticTimestamps(const qint64 scale)
{
    const qint64 clampedScale = std::max<qint64>(1, scale);
    for (TransactionSpan& span : spans_) {
        span.startTimestamp = clampedRoundToTimestamp(static_cast<long double>(span.startTimestamp) * clampedScale);
        span.endTimestamp = clampedRoundToTimestamp(static_cast<long double>(span.endTimestamp) * clampedScale);
    }
    for (TransactionSpan& span : unfilteredSpans_) {
        span.startTimestamp = clampedRoundToTimestamp(static_cast<long double>(span.startTimestamp) * clampedScale);
        span.endTimestamp = clampedRoundToTimestamp(static_cast<long double>(span.endTimestamp) * clampedScale);
    }
    cursorTimestamp_ = clampedRoundToTimestamp(static_cast<long double>(cursorTimestamp_) * clampedScale);
    markerTimestamp_ = clampedRoundToTimestamp(static_cast<long double>(markerTimestamp_) * clampedScale);
    rebuildSpanPaintIndex();
    updateScrollBars();
    update();
}

void TransactionWidget::testSetVerticalScroll(const int value)
{
    if (verticalScrollBar_) {
        verticalScrollBar_->setValue(value);
    } else {
        verticalScrollOffset_ = std::max(0, value);
    }
    update();
}

void TransactionWidget::testSetRowHeight(const int rowHeight)
{
    setRowHeight(rowHeight);
}

void TransactionWidget::testSetCursorFromPosition(const QPoint& position)
{
    setCursorFromPlotPosition(position, true);
    update();
}

bool TransactionWidget::testInvokeContextAction(const int spanIndex, const QString& action)
{
    if (action == QLatin1String("request")) {
        return activateSpanRequestRow(spanIndex);
    }
    if (action == QLatin1String("completion")) {
        return activateSpanCompletionRow(spanIndex);
    }
    if (action == QLatin1String("relatedRows")) {
        return spanIndex >= 0 && spanIndex < static_cast<int>(spans_.size())
            && !relatedRowsText(spans_[static_cast<std::size_t>(spanIndex)]).isEmpty();
    }
    if (action == QLatin1String("debug")) {
        return spanIndex >= 0 && spanIndex < static_cast<int>(spans_.size())
            && !debugText(spans_[static_cast<std::size_t>(spanIndex)]).isEmpty();
    }
    if (action == QLatin1String("highlightOpcode")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Highlight, TransactionFilterField::Opcode);
    }
    if (action == QLatin1String("filterOpcode")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Filter, TransactionFilterField::Opcode);
    }
    if (action == QLatin1String("highlightAddress")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Highlight, TransactionFilterField::Address);
    }
    if (action == QLatin1String("filterAddress")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Filter, TransactionFilterField::Address);
    }
    if (action == QLatin1String("highlightTxnId")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Highlight, TransactionFilterField::TxnId);
    }
    if (action == QLatin1String("filterTxnId")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Filter, TransactionFilterField::TxnId);
    }
    if (action == QLatin1String("highlightDbid")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Highlight, TransactionFilterField::Dbid);
    }
    if (action == QLatin1String("filterDbid")) {
        return applySpanFilterAction(spanIndex, TransactionFilterMode::Filter, TransactionFilterField::Dbid);
    }
    if (action == QLatin1String("clearFilter")) {
        clearTransactionFilter();
        return true;
    }
    return false;
}

QString TransactionWidget::testRelatedRowsTextForSpan(const int spanIndex) const
{
    return spanIndex >= 0 && spanIndex < static_cast<int>(spans_.size())
        ? relatedRowsText(spans_[static_cast<std::size_t>(spanIndex)])
        : QString();
}

QString TransactionWidget::testDebugTextForSpan(const int spanIndex) const
{
    return spanIndex >= 0 && spanIndex < static_cast<int>(spans_.size())
        ? debugText(spans_[static_cast<std::size_t>(spanIndex)])
        : QString();
}

quint64 TransactionWidget::testBuildGeneration() const noexcept
{
    return buildGeneration_.load(std::memory_order_relaxed);
}
#endif

}  // namespace CHIron::Gui
