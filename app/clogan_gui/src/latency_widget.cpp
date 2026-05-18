#include "latency_widget.hpp"

#include "clog_b_trace_loader.hpp"
#include "filter_parallel_utils.hpp"
#include "gui_format.hpp"

#include <QAbstractItemView>
#include <QBrush>
#include <QComboBox>
#include <QDataStream>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QFontMetrics>
#include <QFont>
#include <QHeaderView>
#include <QHash>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace CHIron::Gui {

namespace {

constexpr std::size_t kMaxRenderedOutliers = 512;
#ifdef CHIRON_GUI_ENABLE_LATENCY_TEST_API
constexpr std::uint64_t kLatencyWorkerChunkRows = 2;
#else
constexpr std::uint64_t kLatencyWorkerChunkRows = 32768;
#endif
constexpr int kClassificationColumn = 0;
constexpr int kBoxPlotColumn = 1;
constexpr int kPlotRole = Qt::UserRole + 1;
constexpr int kLogicalRowRole = Qt::UserRole + 2;
constexpr int kSortValueRole = Qt::UserRole + 3;
constexpr char kPlotTypeProperty[] = "latencyPlotType";
constexpr int kPlotHorizontalPadding = 10;
constexpr int kPlotTopPadding = 5;
constexpr int kPlotMinimumTrackHeight = 24;
constexpr int kLatencyScaleHeight = 34;
constexpr int kLatencySummaryWidth = 210;
constexpr int kPlotGestureActivationDistance = 7;
constexpr int kPlotGestureDirectionDistance = 24;
constexpr int kPlotCursorMagnetRadius = 9;
constexpr int kPlotCursorMagnetCongestionRadius = 18;
constexpr int kPlotCursorMagnetMaxNearbyDots = 6;
constexpr double kMinLatencyPlotZoom = 1.0;
constexpr double kMaxLatencyPlotZoom = 1048576.0;
constexpr double kPi = 3.14159265358979323846;
constexpr char kPlotVisibleMinProperty[] = "latencyPlotVisibleMin";
constexpr char kPlotVisibleMaxProperty[] = "latencyPlotVisibleMax";
constexpr char kPlotCursorVisibleProperty[] = "latencyPlotCursorVisible";
constexpr char kPlotCursorValueProperty[] = "latencyPlotCursorValue";
constexpr char kPlotDragActiveProperty[] = "latencyPlotDragActive";
constexpr char kPlotDragStartXProperty[] = "latencyPlotDragStartX";
constexpr char kPlotDragLastXProperty[] = "latencyPlotDragLastX";
constexpr std::array<char, 8> kLatencyCacheMagic = {'C', 'H', 'L', 'A', 'T', '1', '\0', '\0'};
constexpr std::uint32_t kLatencyCacheVersion = 4;
constexpr std::size_t kMaxRenderedSamples = 768;
constexpr int kPlotDotClickRadius = 10;

enum class LatencyPlotType {
    Box = 0,
    Jitter = 1,
    Violin = 2,
};

struct LatencyStats {
    std::uint64_t count = 0;
    qint64 min = 0;
    qint64 q1 = 0;
    qint64 median = 0;
    qint64 q3 = 0;
    qint64 max = 0;
    qint64 mean = 0;
    qint64 lowerWhisker = 0;
    qint64 upperWhisker = 0;
    std::uint64_t outlierCount = 0;
    std::vector<qint64> outliers;
    std::vector<int> outlierLogicalRows;
    std::vector<qint64> renderedSamples;
    std::vector<int> renderedSampleLogicalRows;
};

struct LatencyBucket {
    QString name;
    std::vector<qint64> samples;
    std::vector<int> sampleLogicalRows;
    int exampleLogicalRow = -1;
};

struct RequestOpcodeBucket {
    QString opcode;
    std::vector<qint64> samples;
    std::vector<int> sampleLogicalRows;
    std::map<QString, LatencyBucket> subsequentBuckets;
};

struct XactionTypeBucket {
    QString type;
    std::vector<qint64> samples;
    std::vector<int> sampleLogicalRows;
    std::map<QString, RequestOpcodeBucket> requestOpcodeBuckets;
};

} // namespace

struct LatencyWidget::BuildResult {
    quint64 generation = 0;
    std::uint64_t totalXactions = 0;
    std::uint64_t processedXactions = 0;
    std::uint64_t sampleCount = 0;
    qint64 typeLoadMs = 0;
    qint64 rowCountLoadMs = 0;
    qint64 eventScanMs = 0;
    qint64 aggregationMs = 0;
    qint64 uiBuildMs = 0;
    std::map<QString, XactionTypeBucket> typeBuckets;
    QString statusText;
    bool cancelled = false;
    bool failed = false;
    bool loadedFromCache = false;
};

namespace {

struct TransactionSummary {
    QString xactionType;
    QString requestOpcode;
    qint64 startTimestamp = 0;
    qint64 endTimestamp = 0;
    int firstLogicalRow = -1;
    std::vector<std::tuple<QString, qint64, int>> subsequentTimestamps;
    bool valid = false;
};

struct StreamingTransactionState {
    QString xactionType = QStringLiteral("Unknown Xaction");
    QString requestOpcode;
    qint64 startTimestamp = 0;
    qint64 endTimestamp = 0;
    int firstLogicalRow = -1;
    std::uint64_t observedRows = 0;
    std::uint64_t expectedRows = 0;
    bool hasStart = false;
    bool sawReqOrSnp = false;
    bool emittedSample = false;
    bool emittedCompletionSample = false;
    QSet<QString> seenDataOpcodes;
};

struct LatencyRowEvent {
    std::uint64_t ordinal = 0;
    std::uint64_t logicalRow = 0;
    qint64 timestamp = 0;
    FlitChannel channel = FlitChannel::Req;
    QString opcode;
    QString opcodeRaw;
};

struct LatencyChunkResult {
    std::uint64_t beginRow = 0;
    std::uint64_t rowCount = 0;
    std::vector<LatencyRowEvent> events;
    CLogBTraceLoadError error;
    bool failed = false;
};

struct LatencyBlockSlice {
    std::size_t blockIndex = 0;
    std::uint64_t beginRow = 0;
    std::uint64_t rowCount = 0;
    std::size_t localBegin = 0;
};

struct LatencyAggregationPartition {
    std::vector<LatencyRowEvent> events;
    std::map<QString, XactionTypeBucket> typeBuckets;
    std::uint64_t sampleCount = 0;
};

struct LatencyCompactKey {
    std::uint32_t typeId = 0;
    std::uint32_t requestId = 0;
    std::uint32_t subsequentId = 0;

    bool operator==(const LatencyCompactKey& other) const noexcept
    {
        return typeId == other.typeId
            && requestId == other.requestId
            && subsequentId == other.subsequentId;
    }
};

struct LatencyCompactKeyHash {
    std::size_t operator()(const LatencyCompactKey& key) const noexcept
    {
        std::size_t seed = static_cast<std::size_t>(key.typeId);
        seed ^= static_cast<std::size_t>(key.requestId) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
        seed ^= static_cast<std::size_t>(key.subsequentId) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

struct LatencyParentCompactKey {
    std::uint32_t typeId = 0;
    std::uint32_t requestId = 0;

    bool operator==(const LatencyParentCompactKey& other) const noexcept
    {
        return typeId == other.typeId && requestId == other.requestId;
    }
};

struct LatencyParentCompactKeyHash {
    std::size_t operator()(const LatencyParentCompactKey& key) const noexcept
    {
        std::size_t seed = static_cast<std::size_t>(key.typeId);
        seed ^= static_cast<std::size_t>(key.requestId) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

struct LatencyCompactBucket {
    std::vector<qint64> samples;
    std::vector<int> sampleLogicalRows;
    int exampleLogicalRow = -1;
};

struct LatencyParentCompactBucket {
    std::vector<qint64> samples;
    std::vector<int> sampleLogicalRows;
};

struct LatencyStringInterner {
    std::vector<QString> values;
    QHash<QString, std::uint32_t> ids;

    std::uint32_t intern(const QString& value)
    {
        const auto found = ids.constFind(value);
        if (found != ids.cend()) {
            return found.value();
        }
        const std::uint32_t id = static_cast<std::uint32_t>(values.size());
        values.push_back(value);
        ids.insert(values.back(), id);
        return id;
    }

    const QString& value(const std::uint32_t id) const
    {
        return values[static_cast<std::size_t>(id)];
    }
};

std::uint64_t fileSizeForCacheKey(const QString& path)
{
    const QFileInfo info(path);
    return info.exists() ? static_cast<std::uint64_t>(info.size()) : 0;
}

std::int64_t fileModifiedMsForCacheKey(const QString& path)
{
    const QFileInfo info(path);
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0;
}

QString latencyCachePathForSession(const TraceSession& session)
{
    return session.filePath() + QStringLiteral(".latencyidx");
}

std::uint64_t parameterSignature(const CLog::Parameters& params)
{
    std::uint64_t signature = static_cast<std::uint64_t>(params.GetIssue());
    const auto mix = [&signature](const std::uint64_t value) {
        signature ^= value + 0x9E3779B97F4A7C15ULL + (signature << 6U) + (signature >> 2U);
    };
    mix(static_cast<std::uint64_t>(params.GetNodeIdWidth()));
    mix(static_cast<std::uint64_t>(params.GetReqAddrWidth()));
    mix(static_cast<std::uint64_t>(params.GetReqRSVDCWidth()));
    mix(static_cast<std::uint64_t>(params.GetDatRSVDCWidth()));
    mix(static_cast<std::uint64_t>(params.GetDataWidth()));
    mix(params.IsDataCheckPresent() ? 1U : 0U);
    mix(params.IsPoisonPresent() ? 1U : 0U);
    mix(params.IsMPAMPresent() ? 1U : 0U);
    return signature;
}

class LatencyTreeItem final : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const QTreeWidget* owner = treeWidget();
        const int column = owner ? owner->sortColumn() : 0;
        if (column == kBoxPlotColumn) {
            return data(column, kSortValueRole).toLongLong()
                < other.data(column, kSortValueRole).toLongLong();
        }
        return QTreeWidgetItem::operator<(other);
    }
};

std::optional<TransactionSummary> summarizeTransaction(
    const std::shared_ptr<TraceSession>& session,
    std::uint64_t ordinal,
    const std::vector<std::uint64_t>& logicalRows,
    const std::vector<FlitRecord>& rows);

std::optional<std::uint64_t> ordinalFromGroupKey(const QString& groupKey)
{
    static const QString prefix = QStringLiteral("xaction|");
    if (!groupKey.startsWith(prefix)) {
        return std::nullopt;
    }

    bool ok = false;
    const std::uint64_t ordinal = groupKey.sliced(prefix.size()).toULongLong(&ok);
    if (!ok || ordinal == 0) {
        return std::nullopt;
    }
    return ordinal;
}

QString displayOpcode(const FlitRecord& record)
{
    if (!record.opcode.isEmpty()) {
        return record.opcode;
    }
    if (!record.opcodeRaw.isEmpty()) {
        return record.opcodeRaw;
    }
    return QStringLiteral("Unknown");
}

QString displayOpcode(const LatencyRowEvent& event)
{
    if (!event.opcode.isEmpty()) {
        return event.opcode;
    }
    if (!event.opcodeRaw.isEmpty()) {
        return event.opcodeRaw;
    }
    return QStringLiteral("Unknown");
}

FlitChannel flitChannelFromTraceChannel(const CLog::Channel channel)
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

bool appendLatencyEventsFromFastRecords(const std::shared_ptr<TraceSession>& session,
                                        const LatencyBlockSlice& slice,
                                        const std::unordered_set<std::uint64_t>& completedOrdinals,
                                        std::vector<LatencyRowEvent>& events,
                                        CLogBTraceLoadError& error,
                                        std::stop_token stopToken)
{
    if (!session) {
        error.summary = QStringLiteral("Open a trace to populate latency.");
        return false;
    }

    std::vector<CLogBTraceFastRecordInfo> fastRecords;
    if (!session->readFastRecordsFromIndex(slice.blockIndex, fastRecords)) {
        std::shared_ptr<CLog::CLogB::TagCHIRecords> block;
        if (!CLogBTraceLoader::loadBlockRecords(session->filePath(),
                                                session->metadata(),
                                                slice.blockIndex,
                                                block,
                                                error,
                                                {},
                                                stopToken)
            || !block) {
            return false;
        }

        if (!CLogBTraceLoader::buildBlockFastRecords(session->metadata(),
                                                     slice.blockIndex,
                                                     *block,
                                                     fastRecords,
                                                     error,
                                                     stopToken)) {
            return false;
        }
    }
    std::unordered_map<std::uint32_t, QString> opcodeCache;

    std::vector<std::uint64_t> ordinals;
    if (!session->xactionOrdinalsByRowRange(slice.beginRow, slice.rowCount, ordinals)
        || ordinals.size() < slice.rowCount) {
        error.summary = QStringLiteral("failed to read Xaction row ordinals");
        return false;
    }

    if (slice.localBegin > fastRecords.size()
        || static_cast<std::size_t>(slice.rowCount) > fastRecords.size() - slice.localBegin) {
        error.summary = QStringLiteral("fast-record block slice is outside the decoded block.");
        return false;
    }

    events.reserve(events.size() + static_cast<std::size_t>(slice.rowCount));
    for (std::size_t index = 0; index < static_cast<std::size_t>(slice.rowCount); ++index) {
        const std::uint64_t ordinal = ordinals[index];
        if (ordinal == 0 || completedOrdinals.find(ordinal) == completedOrdinals.end()) {
            continue;
        }

        const CLogBTraceFastRecordInfo& fastRecord = fastRecords[slice.localBegin + index];
        const std::uint32_t opcodeKey =
            (static_cast<std::uint32_t>(fastRecord.channel) << 24U)
            | (fastRecord.opcode & 0x00FFFFFFU);
        auto opcodeFound = opcodeCache.find(opcodeKey);
        if (opcodeFound == opcodeCache.end()) {
            opcodeFound = opcodeCache.emplace(
                opcodeKey,
                CLogBTraceLoader::opcodeDisplayValue(session->metadata().parameters, fastRecord)).first;
        }
        QString opcode = opcodeFound->second;
        QString opcodeRaw = QStringLiteral("0x%1")
                                .arg(static_cast<qulonglong>(fastRecord.opcode), 0, 16)
                                .toUpper();
        if (opcode == QLatin1String("Unknown")) {
            opcode.clear();
        }

        events.push_back(LatencyRowEvent{
            .ordinal = ordinal,
            .logicalRow = slice.beginRow + index,
            .timestamp = fastRecord.timestamp,
            .channel = flitChannelFromTraceChannel(static_cast<CLog::Channel>(fastRecord.channel)),
            .opcode = std::move(opcode),
            .opcodeRaw = std::move(opcodeRaw),
        });
    }
    return true;
}

QString dataBeatLabel(const FlitRecord& record, QSet<QString>& seenDataOpcodes)
{
    const QString opcode = displayOpcode(record);
    const bool firstBeat = !seenDataOpcodes.contains(opcode);
    seenDataOpcodes.insert(opcode);
    return firstBeat
        ? QStringLiteral("%1 first beat").arg(opcode)
        : QStringLiteral("%1 following beats").arg(opcode);
}

QString subsequentLabel(const FlitRecord& record, QSet<QString>& seenDataOpcodes)
{
    if (record.channel == FlitChannel::Dat) {
        return dataBeatLabel(record, seenDataOpcodes);
    }
    return QStringLiteral("%1 %2").arg(ToString(record.channel), displayOpcode(record));
}

QString dataBeatLabel(const LatencyRowEvent& event, QSet<QString>& seenDataOpcodes)
{
    const QString opcode = displayOpcode(event);
    const bool firstBeat = !seenDataOpcodes.contains(opcode);
    seenDataOpcodes.insert(opcode);
    return firstBeat
        ? QStringLiteral("%1 first beat").arg(opcode)
        : QStringLiteral("%1 following beats").arg(opcode);
}

QString subsequentLabel(const LatencyRowEvent& event, QSet<QString>& seenDataOpcodes)
{
    if (event.channel == FlitChannel::Dat) {
        return dataBeatLabel(event, seenDataOpcodes);
    }
    return QStringLiteral("%1 %2").arg(ToString(event.channel), displayOpcode(event));
}

qint64 percentileOfSorted(const std::vector<qint64>& values, const double percentile)
{
    if (values.empty()) {
        return 0;
    }
    if (values.size() == 1) {
        return values.front();
    }

    const double index = percentile * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(index));
    const auto upper = static_cast<std::size_t>(std::ceil(index));
    if (lower == upper) {
        return values[lower];
    }

    const double fraction = index - static_cast<double>(lower);
    return static_cast<qint64>(std::llround(
        static_cast<double>(values[lower]) * (1.0 - fraction)
        + static_cast<double>(values[upper]) * fraction));
}

LatencyStats calculateStats(std::vector<qint64> samples, std::vector<int> sampleLogicalRows = {})
{
    LatencyStats stats;
    stats.count = static_cast<std::uint64_t>(samples.size());
    if (samples.empty()) {
        return stats;
    }

    if (sampleLogicalRows.size() != samples.size()) {
        sampleLogicalRows.assign(samples.size(), -1);
    }

    std::vector<std::pair<qint64, int>> sortedSamples;
    sortedSamples.reserve(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        sortedSamples.emplace_back(samples[index], sampleLogicalRows[index]);
    }
    std::sort(sortedSamples.begin(), sortedSamples.end(), [](const auto& left, const auto& right) {
        if (left.first != right.first) {
            return left.first < right.first;
        }
        return left.second < right.second;
    });
    samples.clear();
    sampleLogicalRows.clear();
    samples.reserve(sortedSamples.size());
    sampleLogicalRows.reserve(sortedSamples.size());
    for (const auto& [sample, logicalRow] : sortedSamples) {
        samples.push_back(sample);
        sampleLogicalRows.push_back(logicalRow);
    }

    stats.min = samples.front();
    stats.q1 = percentileOfSorted(samples, 0.25);
    stats.median = percentileOfSorted(samples, 0.50);
    stats.q3 = percentileOfSorted(samples, 0.75);
    stats.max = samples.back();
    stats.lowerWhisker = stats.min;
    stats.upperWhisker = stats.max;

    long double sum = 0.0L;
    for (const qint64 sample : samples) {
        sum += static_cast<long double>(sample);
    }
    stats.mean = static_cast<qint64>(std::llround(sum / static_cast<long double>(samples.size())));

    const long double iqr = static_cast<long double>(stats.q3) - static_cast<long double>(stats.q1);
    const long double lowerFence = static_cast<long double>(stats.q1) - 1.5L * iqr;
    const long double upperFence = static_cast<long double>(stats.q3) + 1.5L * iqr;

    const auto isOutlier = [&](const qint64 value) {
        const long double wideValue = static_cast<long double>(value);
        return wideValue < lowerFence || wideValue > upperFence;
    };

    const auto lowerWhisker = std::find_if(samples.begin(), samples.end(), [&](const qint64 value) {
        return !isOutlier(value);
    });
    const auto upperWhisker = std::find_if(samples.rbegin(), samples.rend(), [&](const qint64 value) {
        return !isOutlier(value);
    });
    if (lowerWhisker != samples.end()) {
        stats.lowerWhisker = *lowerWhisker;
    }
    if (upperWhisker != samples.rend()) {
        stats.upperWhisker = *upperWhisker;
    }

    std::vector<qint64> outliers;
    std::vector<int> outlierLogicalRows;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (isOutlier(samples[index])) {
            outliers.push_back(samples[index]);
            outlierLogicalRows.push_back(sampleLogicalRows[index]);
        }
    }
    stats.outlierCount = static_cast<std::uint64_t>(outliers.size());
    if (outliers.size() <= kMaxRenderedOutliers) {
        stats.outliers = std::move(outliers);
        stats.outlierLogicalRows = std::move(outlierLogicalRows);
    } else {
        stats.outliers.reserve(kMaxRenderedOutliers);
        stats.outlierLogicalRows.reserve(kMaxRenderedOutliers);
        for (std::size_t index = 0; index < kMaxRenderedOutliers; ++index) {
            const long double ratio = static_cast<long double>(index)
                / static_cast<long double>(kMaxRenderedOutliers - 1U);
            const std::size_t sourceIndex = static_cast<std::size_t>(
                std::llround(ratio * static_cast<long double>(outliers.size() - 1U)));
            stats.outliers.push_back(outliers[sourceIndex]);
            stats.outlierLogicalRows.push_back(outlierLogicalRows[sourceIndex]);
        }
    }

    if (samples.size() <= kMaxRenderedSamples) {
        stats.renderedSamples = std::move(samples);
        stats.renderedSampleLogicalRows = std::move(sampleLogicalRows);
    } else {
        stats.renderedSamples.reserve(kMaxRenderedSamples);
        stats.renderedSampleLogicalRows.reserve(kMaxRenderedSamples);
        for (std::size_t index = 0; index < kMaxRenderedSamples; ++index) {
            const long double ratio = static_cast<long double>(index)
                / static_cast<long double>(kMaxRenderedSamples - 1U);
            const std::size_t sourceIndex = static_cast<std::size_t>(
                std::llround(ratio * static_cast<long double>(samples.size() - 1U)));
            stats.renderedSamples.push_back(samples[sourceIndex]);
            stats.renderedSampleLogicalRows.push_back(sampleLogicalRows[sourceIndex]);
        }
    }
    return stats;
}

QString latencyText(const qint64 value)
{
    return FormatSignedIntegerWithThousands(value);
}

QStringList statsSummaryLines(const LatencyStats& stats)
{
    return {
        QStringLiteral("Count %1").arg(static_cast<qulonglong>(stats.count)),
        QStringLiteral("Min %1").arg(latencyText(stats.min)),
        QStringLiteral("Q1 %1").arg(latencyText(stats.q1)),
        QStringLiteral("Median %1").arg(latencyText(stats.median)),
        QStringLiteral("Q3 %1").arg(latencyText(stats.q3)),
        QStringLiteral("Max %1").arg(latencyText(stats.max)),
        QStringLiteral("Mean %1").arg(latencyText(stats.mean)),
        QStringLiteral("Outliers %1").arg(static_cast<qulonglong>(stats.outlierCount)),
    };
}

QVariant statsVariant(const LatencyStats& stats)
{
    QVariantList outliers;
    outliers.reserve(static_cast<int>(stats.outliers.size()));
    for (const qint64 outlier : stats.outliers) {
        outliers.append(QVariant::fromValue<qlonglong>(outlier));
    }

    QVariantList outlierLogicalRows;
    outlierLogicalRows.reserve(static_cast<int>(stats.outlierLogicalRows.size()));
    for (const int logicalRow : stats.outlierLogicalRows) {
        outlierLogicalRows.append(logicalRow);
    }

    QVariantList renderedSamples;
    renderedSamples.reserve(static_cast<int>(stats.renderedSamples.size()));
    for (const qint64 sample : stats.renderedSamples) {
        renderedSamples.append(QVariant::fromValue<qlonglong>(sample));
    }

    QVariantList renderedSampleLogicalRows;
    renderedSampleLogicalRows.reserve(static_cast<int>(stats.renderedSampleLogicalRows.size()));
    for (const int logicalRow : stats.renderedSampleLogicalRows) {
        renderedSampleLogicalRows.append(logicalRow);
    }

    return QVariantList{
        QVariant::fromValue<qulonglong>(static_cast<qulonglong>(stats.count)),
        QVariant::fromValue<qlonglong>(stats.min),
        QVariant::fromValue<qlonglong>(stats.lowerWhisker),
        QVariant::fromValue<qlonglong>(stats.q1),
        QVariant::fromValue<qlonglong>(stats.median),
        QVariant::fromValue<qlonglong>(stats.q3),
        QVariant::fromValue<qlonglong>(stats.upperWhisker),
        QVariant::fromValue<qlonglong>(stats.max),
        QVariant::fromValue<qlonglong>(stats.mean),
        QVariant::fromValue<qulonglong>(static_cast<qulonglong>(stats.outlierCount)),
        outliers,
        outlierLogicalRows,
        renderedSamples,
        renderedSampleLogicalRows,
    };
}

std::optional<LatencyStats> statsFromVariant(const QVariant& variant)
{
    const QVariantList values = variant.toList();
    if (values.size() < 11 || values.size() > 14) {
        return std::nullopt;
    }

    LatencyStats stats;
    stats.count = values[0].toULongLong();
    stats.min = values[1].toLongLong();
    stats.lowerWhisker = values[2].toLongLong();
    stats.q1 = values[3].toLongLong();
    stats.median = values[4].toLongLong();
    stats.q3 = values[5].toLongLong();
    stats.upperWhisker = values[6].toLongLong();
    stats.max = values[7].toLongLong();
    stats.mean = values[8].toLongLong();
    stats.outlierCount = values[9].toULongLong();
    const QVariantList outliers = values[10].toList();
    stats.outliers.reserve(static_cast<std::size_t>(outliers.size()));
    for (const QVariant& outlier : outliers) {
        stats.outliers.push_back(outlier.toLongLong());
    }
    if (values.size() >= 12) {
        const QVariantList outlierLogicalRows = values[11].toList();
        stats.outlierLogicalRows.reserve(static_cast<std::size_t>(outlierLogicalRows.size()));
        for (const QVariant& logicalRow : outlierLogicalRows) {
            stats.outlierLogicalRows.push_back(logicalRow.toInt());
        }
    }
    if (stats.outlierLogicalRows.size() != stats.outliers.size()) {
        stats.outlierLogicalRows.assign(stats.outliers.size(), -1);
    }
    if (values.size() >= 13) {
        const QVariantList renderedSamples = values[12].toList();
        stats.renderedSamples.reserve(static_cast<std::size_t>(renderedSamples.size()));
        for (const QVariant& sample : renderedSamples) {
            stats.renderedSamples.push_back(sample.toLongLong());
        }
    }
    if (values.size() >= 14) {
        const QVariantList renderedSampleLogicalRows = values[13].toList();
        stats.renderedSampleLogicalRows.reserve(
            static_cast<std::size_t>(renderedSampleLogicalRows.size()));
        for (const QVariant& logicalRow : renderedSampleLogicalRows) {
            stats.renderedSampleLogicalRows.push_back(logicalRow.toInt());
        }
    }
    if (stats.renderedSampleLogicalRows.size() != stats.renderedSamples.size()) {
        stats.renderedSampleLogicalRows.assign(stats.renderedSamples.size(), -1);
    }
    return stats;
}

QTreeWidgetItem* makeItem(const QString& label,
                          const LatencyStats& stats,
                          const int logicalRow,
                          const int depth)
{
    auto* item = new LatencyTreeItem();
    item->setText(kClassificationColumn, label);
    item->setData(kBoxPlotColumn, kPlotRole, statsVariant(stats));
    item->setData(kClassificationColumn, kLogicalRowRole, logicalRow);
    item->setData(kBoxPlotColumn,
                  kSortValueRole,
                  QVariant::fromValue<qlonglong>(static_cast<qlonglong>(stats.count)));
    item->setToolTip(kBoxPlotColumn, QString());

    QFont font = item->font(kClassificationColumn);
    if (depth <= 1) {
        font.setBold(true);
        item->setFont(kClassificationColumn, font);
    }
    if (depth == 0) {
        for (int column = 0; column < 2; ++column) {
            item->setBackground(column, QBrush(QColor(QStringLiteral("#F4F7FA"))));
        }
    }
    return item;
}

LatencyStats collectStats(const RequestOpcodeBucket& bucket)
{
    return calculateStats(bucket.samples, bucket.sampleLogicalRows);
}

LatencyStats collectStats(const XactionTypeBucket& bucket)
{
    return calculateStats(bucket.samples, bucket.sampleLogicalRows);
}

int firstExampleRow(const std::map<QString, LatencyBucket>& buckets)
{
    for (const auto& [_, bucket] : buckets) {
        if (bucket.exampleLogicalRow >= 0) {
            return bucket.exampleLogicalRow;
        }
    }
    return -1;
}

int firstExampleRow(const RequestOpcodeBucket& bucket)
{
    return firstExampleRow(bucket.subsequentBuckets);
}

int firstExampleRow(const XactionTypeBucket& bucket)
{
    for (const auto& [_, requestBucket] : bucket.requestOpcodeBuckets) {
        const int row = firstExampleRow(requestBucket);
        if (row >= 0) {
            return row;
        }
    }
    return -1;
}

void addCompletionLatencySample(std::map<QString, XactionTypeBucket>& buckets,
                                const QString& xactionType,
                                const QString& requestOpcode,
                                const qint64 latency,
                                const int requestLogicalRow)
{
    XactionTypeBucket& typeBucket = buckets[xactionType];
    typeBucket.type = xactionType;
    typeBucket.samples.push_back(latency);
    typeBucket.sampleLogicalRows.push_back(requestLogicalRow);
    RequestOpcodeBucket& requestBucket = typeBucket.requestOpcodeBuckets[requestOpcode];
    requestBucket.opcode = requestOpcode;
    requestBucket.samples.push_back(latency);
    requestBucket.sampleLogicalRows.push_back(requestLogicalRow);
}

void addSubsequentLatencySample(std::map<QString, XactionTypeBucket>& buckets,
                                std::uint64_t& sampleCount,
                                const QString& xactionType,
                                const QString& requestOpcode,
                                const QString& subsequent,
                                const qint64 latency,
                                const int exampleLogicalRow)
{
    XactionTypeBucket& typeBucket = buckets[xactionType];
    typeBucket.type = xactionType;
    RequestOpcodeBucket& requestBucket = typeBucket.requestOpcodeBuckets[requestOpcode];
    requestBucket.opcode = requestOpcode;
    LatencyBucket& bucket = requestBucket.subsequentBuckets[subsequent];
    bucket.name = subsequent;
    bucket.samples.push_back(latency);
    bucket.sampleLogicalRows.push_back(exampleLogicalRow);
    if (bucket.exampleLogicalRow < 0) {
        bucket.exampleLogicalRow = exampleLogicalRow;
    }
    ++sampleCount;
}

void mergeLatencyBuckets(std::map<QString, XactionTypeBucket>& destination,
                         std::map<QString, XactionTypeBucket>&& source)
{
    for (auto& [typeName, sourceType] : source) {
        XactionTypeBucket& destinationType = destination[typeName];
        if (destinationType.type.isEmpty()) {
            destinationType.type = std::move(sourceType.type);
        }
        destinationType.samples.insert(destinationType.samples.end(),
                                       std::make_move_iterator(sourceType.samples.begin()),
                                       std::make_move_iterator(sourceType.samples.end()));
        destinationType.sampleLogicalRows.insert(
            destinationType.sampleLogicalRows.end(),
            std::make_move_iterator(sourceType.sampleLogicalRows.begin()),
            std::make_move_iterator(sourceType.sampleLogicalRows.end()));

        for (auto& [requestName, sourceRequest] : sourceType.requestOpcodeBuckets) {
            RequestOpcodeBucket& destinationRequest =
                destinationType.requestOpcodeBuckets[requestName];
            if (destinationRequest.opcode.isEmpty()) {
                destinationRequest.opcode = std::move(sourceRequest.opcode);
            }
            destinationRequest.samples.insert(
                destinationRequest.samples.end(),
                std::make_move_iterator(sourceRequest.samples.begin()),
                std::make_move_iterator(sourceRequest.samples.end()));
            destinationRequest.sampleLogicalRows.insert(
                destinationRequest.sampleLogicalRows.end(),
                std::make_move_iterator(sourceRequest.sampleLogicalRows.begin()),
                std::make_move_iterator(sourceRequest.sampleLogicalRows.end()));

            for (auto& [subsequentName, sourceSubsequent] : sourceRequest.subsequentBuckets) {
                LatencyBucket& destinationSubsequent =
                    destinationRequest.subsequentBuckets[subsequentName];
                if (destinationSubsequent.name.isEmpty()) {
                    destinationSubsequent.name = std::move(sourceSubsequent.name);
                }
                destinationSubsequent.samples.insert(
                    destinationSubsequent.samples.end(),
                    std::make_move_iterator(sourceSubsequent.samples.begin()),
                    std::make_move_iterator(sourceSubsequent.samples.end()));
                destinationSubsequent.sampleLogicalRows.insert(
                    destinationSubsequent.sampleLogicalRows.end(),
                    std::make_move_iterator(sourceSubsequent.sampleLogicalRows.begin()),
                    std::make_move_iterator(sourceSubsequent.sampleLogicalRows.end()));
                if (destinationSubsequent.exampleLogicalRow < 0
                    || (sourceSubsequent.exampleLogicalRow >= 0
                        && sourceSubsequent.exampleLogicalRow < destinationSubsequent.exampleLogicalRow)) {
                    destinationSubsequent.exampleLogicalRow = sourceSubsequent.exampleLogicalRow;
                }
            }
        }
    }
}

void addCompletionLatencySample(
    std::unordered_map<LatencyParentCompactKey,
                       LatencyParentCompactBucket,
                       LatencyParentCompactKeyHash>& parentBuckets,
    LatencyStringInterner& typeInterner,
    LatencyStringInterner& requestInterner,
    const QString& xactionType,
    const QString& requestOpcode,
    const qint64 latency,
    const int requestLogicalRow)
{
    const LatencyParentCompactKey key{
        .typeId = typeInterner.intern(xactionType),
        .requestId = requestInterner.intern(requestOpcode),
    };
    LatencyParentCompactBucket& bucket = parentBuckets[key];
    bucket.samples.push_back(latency);
    bucket.sampleLogicalRows.push_back(requestLogicalRow);
}

void addCompactLatencySample(
    std::unordered_map<LatencyCompactKey, LatencyCompactBucket, LatencyCompactKeyHash>& buckets,
    LatencyStringInterner& typeInterner,
    LatencyStringInterner& requestInterner,
    LatencyStringInterner& subsequentInterner,
    const QString& xactionType,
    const QString& requestOpcode,
    const QString& subsequent,
    const qint64 latency,
    const int exampleLogicalRow)
{
    const LatencyCompactKey key{
        .typeId = typeInterner.intern(xactionType),
        .requestId = requestInterner.intern(requestOpcode),
        .subsequentId = subsequentInterner.intern(subsequent),
    };
    LatencyCompactBucket& bucket = buckets[key];
    bucket.samples.push_back(latency);
    bucket.sampleLogicalRows.push_back(exampleLogicalRow);
    if (bucket.exampleLogicalRow < 0 || exampleLogicalRow < bucket.exampleLogicalRow) {
        bucket.exampleLogicalRow = exampleLogicalRow;
    }
}

void materializeCompactLatencyBuckets(
    std::map<QString, XactionTypeBucket>& destination,
    std::uint64_t& sampleCount,
    const LatencyStringInterner& typeInterner,
    const LatencyStringInterner& requestInterner,
    const LatencyStringInterner& subsequentInterner,
    std::unordered_map<LatencyParentCompactKey,
                       LatencyParentCompactBucket,
                       LatencyParentCompactKeyHash>&& parentBuckets,
    std::unordered_map<LatencyCompactKey, LatencyCompactBucket, LatencyCompactKeyHash>&& compactBuckets)
{
    for (auto& [key, sourceBucket] : parentBuckets) {
        const QString& xactionType = typeInterner.value(key.typeId);
        const QString& requestOpcode = requestInterner.value(key.requestId);

        XactionTypeBucket& typeBucket = destination[xactionType];
        typeBucket.type = xactionType;
        typeBucket.samples.insert(typeBucket.samples.end(),
                                  std::make_move_iterator(sourceBucket.samples.begin()),
                                  std::make_move_iterator(sourceBucket.samples.end()));
        typeBucket.sampleLogicalRows.insert(
            typeBucket.sampleLogicalRows.end(),
            std::make_move_iterator(sourceBucket.sampleLogicalRows.begin()),
            std::make_move_iterator(sourceBucket.sampleLogicalRows.end()));

        RequestOpcodeBucket& requestBucket = typeBucket.requestOpcodeBuckets[requestOpcode];
        requestBucket.opcode = requestOpcode;
        requestBucket.samples.insert(requestBucket.samples.end(),
                                     std::make_move_iterator(sourceBucket.samples.begin()),
                                     std::make_move_iterator(sourceBucket.samples.end()));
        requestBucket.sampleLogicalRows.insert(
            requestBucket.sampleLogicalRows.end(),
            std::make_move_iterator(sourceBucket.sampleLogicalRows.begin()),
            std::make_move_iterator(sourceBucket.sampleLogicalRows.end()));
    }

    for (auto& [key, sourceBucket] : compactBuckets) {
        const QString& xactionType = typeInterner.value(key.typeId);
        const QString& requestOpcode = requestInterner.value(key.requestId);
        const QString& subsequent = subsequentInterner.value(key.subsequentId);
        const std::uint64_t addedSampleCount =
            static_cast<std::uint64_t>(sourceBucket.samples.size());

        XactionTypeBucket& typeBucket = destination[xactionType];
        typeBucket.type = xactionType;

        RequestOpcodeBucket& requestBucket = typeBucket.requestOpcodeBuckets[requestOpcode];
        requestBucket.opcode = requestOpcode;

        LatencyBucket& latencyBucket = requestBucket.subsequentBuckets[subsequent];
        latencyBucket.name = subsequent;
        latencyBucket.samples.insert(latencyBucket.samples.end(),
                                     std::make_move_iterator(sourceBucket.samples.begin()),
                                     std::make_move_iterator(sourceBucket.samples.end()));
        latencyBucket.sampleLogicalRows.insert(
            latencyBucket.sampleLogicalRows.end(),
            std::make_move_iterator(sourceBucket.sampleLogicalRows.begin()),
            std::make_move_iterator(sourceBucket.sampleLogicalRows.end()));
        if (latencyBucket.exampleLogicalRow < 0
            || (sourceBucket.exampleLogicalRow >= 0
                && sourceBucket.exampleLogicalRow < latencyBucket.exampleLogicalRow)) {
            latencyBucket.exampleLogicalRow = sourceBucket.exampleLogicalRow;
        }
        sampleCount += addedSampleCount;
    }
}

bool readLatencySamples(QDataStream& stream, std::vector<qint64>& samples)
{
    quint64 count = 0;
    stream >> count;
    if (stream.status() != QDataStream::Ok
        || count > static_cast<quint64>((std::numeric_limits<int>::max)())) {
        return false;
    }

    samples.clear();
    samples.reserve(static_cast<std::size_t>(count));
    for (quint64 index = 0; index < count; ++index) {
        qint64 sample = 0;
        stream >> sample;
        if (stream.status() != QDataStream::Ok) {
            samples.clear();
            return false;
        }
        samples.push_back(sample);
    }
    return true;
}

void writeLatencySamples(QDataStream& stream, const std::vector<qint64>& samples)
{
    stream << quint64(samples.size());
    for (const qint64 sample : samples) {
        stream << qint64(sample);
    }
}

bool readLatencyLogicalRows(QDataStream& stream, std::vector<int>& logicalRows)
{
    quint64 count = 0;
    stream >> count;
    if (stream.status() != QDataStream::Ok
        || count > static_cast<quint64>((std::numeric_limits<int>::max)())) {
        return false;
    }

    logicalRows.clear();
    logicalRows.reserve(static_cast<std::size_t>(count));
    for (quint64 index = 0; index < count; ++index) {
        qint32 logicalRow = -1;
        stream >> logicalRow;
        if (stream.status() != QDataStream::Ok) {
            logicalRows.clear();
            return false;
        }
        logicalRows.push_back(static_cast<int>(logicalRow));
    }
    return true;
}

void writeLatencyLogicalRows(QDataStream& stream, const std::vector<int>& logicalRows)
{
    stream << quint64(logicalRows.size());
    for (const int logicalRow : logicalRows) {
        stream << qint32(logicalRow);
    }
}

} // namespace

bool LatencyWidget::tryLoadLatencyCache(const TraceSession& session, BuildResult& result)
{
    QFile input(latencyCachePathForSession(session));
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDataStream stream(&input);
    stream.setByteOrder(QDataStream::LittleEndian);

    std::array<char, kLatencyCacheMagic.size()> magic{};
    if (stream.readRawData(magic.data(), static_cast<int>(magic.size()))
        != static_cast<int>(magic.size())) {
        return false;
    }

    quint32 version = 0;
    quint64 traceSize = 0;
    qint64 traceModifiedMs = 0;
    quint64 xactionIndexSize = 0;
    qint64 xactionIndexModifiedMs = 0;
    quint64 totalRows = 0;
    quint64 blockCount = 0;
    quint64 paramsSignature = 0;
    quint64 totalXactions = 0;
    quint64 sampleCount = 0;
    quint64 typeCount = 0;
    stream >> version
           >> traceSize
           >> traceModifiedMs
           >> xactionIndexSize
           >> xactionIndexModifiedMs
           >> totalRows
           >> blockCount
           >> paramsSignature
           >> totalXactions
           >> sampleCount
           >> typeCount;
    if (stream.status() != QDataStream::Ok
        || magic != kLatencyCacheMagic
        || version != kLatencyCacheVersion
        || traceSize != fileSizeForCacheKey(session.filePath())
        || traceModifiedMs != fileModifiedMsForCacheKey(session.filePath())
        || xactionIndexSize != fileSizeForCacheKey(session.filePath() + QStringLiteral(".xactidx"))
        || xactionIndexModifiedMs != fileModifiedMsForCacheKey(session.filePath() + QStringLiteral(".xactidx"))
        || totalRows != session.totalRows()
        || blockCount != session.metadata().blocks.size()
        || paramsSignature != parameterSignature(session.metadata().parameters)
        || typeCount > static_cast<quint64>((std::numeric_limits<int>::max)())) {
        return false;
    }

    std::map<QString, XactionTypeBucket> typeBuckets;
    for (quint64 typeIndex = 0; typeIndex < typeCount; ++typeIndex) {
        QString typeName;
        quint64 requestCount = 0;
        XactionTypeBucket typeBucket;
        stream >> typeName;
        if (!readLatencySamples(stream, typeBucket.samples)) {
            return false;
        }
        if (!readLatencyLogicalRows(stream, typeBucket.sampleLogicalRows)
            || typeBucket.sampleLogicalRows.size() != typeBucket.samples.size()) {
            return false;
        }
        stream >> requestCount;
        if (stream.status() != QDataStream::Ok
            || requestCount > static_cast<quint64>((std::numeric_limits<int>::max)())) {
            return false;
        }
        typeBucket.type = typeName;

        for (quint64 requestIndex = 0; requestIndex < requestCount; ++requestIndex) {
            QString requestName;
            quint64 subsequentCount = 0;
            RequestOpcodeBucket requestBucket;
            stream >> requestName;
            if (!readLatencySamples(stream, requestBucket.samples)) {
                return false;
            }
            if (!readLatencyLogicalRows(stream, requestBucket.sampleLogicalRows)
                || requestBucket.sampleLogicalRows.size() != requestBucket.samples.size()) {
                return false;
            }
            stream >> subsequentCount;
            if (stream.status() != QDataStream::Ok
                || subsequentCount > static_cast<quint64>((std::numeric_limits<int>::max)())) {
                return false;
            }
            requestBucket.opcode = requestName;

            for (quint64 subsequentIndex = 0; subsequentIndex < subsequentCount; ++subsequentIndex) {
                QString subsequentName;
                qint32 exampleLogicalRow = -1;
                LatencyBucket latencyBucket;
                stream >> subsequentName >> exampleLogicalRow;
                if (!readLatencySamples(stream, latencyBucket.samples)) {
                    return false;
                }
                if (!readLatencyLogicalRows(stream, latencyBucket.sampleLogicalRows)
                    || latencyBucket.sampleLogicalRows.size() != latencyBucket.samples.size()) {
                    return false;
                }
                if (stream.status() != QDataStream::Ok) {
                    return false;
                }
                latencyBucket.name = subsequentName;
                latencyBucket.exampleLogicalRow = static_cast<int>(exampleLogicalRow);
                requestBucket.subsequentBuckets.emplace(subsequentName, std::move(latencyBucket));
            }

            typeBucket.requestOpcodeBuckets.emplace(requestName, std::move(requestBucket));
        }

        typeBuckets.emplace(typeName, std::move(typeBucket));
    }

    result.typeBuckets = std::move(typeBuckets);
    result.totalXactions = static_cast<std::uint64_t>(totalXactions);
    result.processedXactions = result.totalXactions;
    result.sampleCount = static_cast<std::uint64_t>(sampleCount);
    result.loadedFromCache = true;
    return true;
}

void LatencyWidget::writeLatencyCache(const TraceSession& session, const BuildResult& result)
{
    const QString cachePath = latencyCachePathForSession(session);
    QFile output(cachePath + QStringLiteral(".tmp"));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    QDataStream stream(&output);
    stream.setByteOrder(QDataStream::LittleEndian);
    if (stream.writeRawData(kLatencyCacheMagic.data(), static_cast<int>(kLatencyCacheMagic.size()))
        != static_cast<int>(kLatencyCacheMagic.size())) {
        output.remove();
        return;
    }

    const QString xactionIndexPath = session.filePath() + QStringLiteral(".xactidx");
    stream << quint32(kLatencyCacheVersion)
           << quint64(fileSizeForCacheKey(session.filePath()))
           << qint64(fileModifiedMsForCacheKey(session.filePath()))
           << quint64(fileSizeForCacheKey(xactionIndexPath))
           << qint64(fileModifiedMsForCacheKey(xactionIndexPath))
           << quint64(session.totalRows())
           << quint64(session.metadata().blocks.size())
           << quint64(parameterSignature(session.metadata().parameters))
           << quint64(result.totalXactions)
           << quint64(result.sampleCount)
           << quint64(result.typeBuckets.size());

    for (const auto& [_, typeBucket] : result.typeBuckets) {
        stream << typeBucket.type;
        writeLatencySamples(stream, typeBucket.samples);
        writeLatencyLogicalRows(stream, typeBucket.sampleLogicalRows);
        stream << quint64(typeBucket.requestOpcodeBuckets.size());

        for (const auto& [__, requestBucket] : typeBucket.requestOpcodeBuckets) {
            stream << requestBucket.opcode;
            writeLatencySamples(stream, requestBucket.samples);
            writeLatencyLogicalRows(stream, requestBucket.sampleLogicalRows);
            stream << quint64(requestBucket.subsequentBuckets.size());

            for (const auto& [___, latencyBucket] : requestBucket.subsequentBuckets) {
                stream << latencyBucket.name
                       << qint32(latencyBucket.exampleLogicalRow);
                writeLatencySamples(stream, latencyBucket.samples);
                writeLatencyLogicalRows(stream, latencyBucket.sampleLogicalRows);
            }
        }
    }

    if (stream.status() != QDataStream::Ok) {
        output.remove();
        return;
    }
    output.close();
    QFile::remove(cachePath);
    QFile::rename(output.fileName(), cachePath);
}

namespace {

std::optional<std::uint64_t> xactionOrdinalForRecord(const FlitRecord& row)
{
    for (const QString& key : row.transactionGroupKey.split(QChar(0x1F), Qt::SkipEmptyParts)) {
        const std::optional<std::uint64_t> ordinal = ordinalFromGroupKey(key);
        if (ordinal) {
            return ordinal;
        }
    }
    return std::nullopt;
}

QString xactionTypeForRows(const std::shared_ptr<TraceSession>& session,
                           const std::vector<FlitRecord>& rows,
                           const std::vector<std::uint64_t>& logicalRows)
{
    if (session && !logicalRows.empty()) {
        TraceSession::XactionIndexRowInfo info;
        if (session->xactionRowInfo(logicalRows.front(), info)
            && !info.xactionType.trimmed().isEmpty()
            && info.xactionType != QLatin1String("none")) {
            return info.xactionType;
        }
    }

    for (const FlitRecord& row : rows) {
        if (!row.xactionDebugLog.isEmpty()) {
            const QString marker = QStringLiteral("Xaction type:");
            const int markerIndex = row.xactionDebugLog.indexOf(marker);
            if (markerIndex >= 0) {
                const int start = markerIndex + marker.size();
                const int end = row.xactionDebugLog.indexOf(QLatin1Char('\n'), start);
                const QString value = row.xactionDebugLog.mid(start, end >= 0 ? end - start : -1).trimmed();
                if (!value.isEmpty() && value != QLatin1String("none")) {
                    return value;
                }
            }
        }
    }

    return QStringLiteral("Unknown Xaction");
}

std::map<QString, XactionTypeBucket> buildBucketsFromTransactions(
    const std::shared_ptr<TraceSession>& session,
    const std::vector<std::pair<std::uint64_t, std::pair<std::vector<std::uint64_t>, std::vector<FlitRecord>>>>& transactions,
    std::uint64_t& sampleCount)
{
    std::map<QString, XactionTypeBucket> buckets;
    sampleCount = 0;

    for (const auto& [ordinal, payload] : transactions) {
        const auto& logicalRows = payload.first;
        const auto& rows = payload.second;
        const std::optional<TransactionSummary> summary =
            summarizeTransaction(session, ordinal, logicalRows, rows);
        if (!summary || !summary->valid) {
            continue;
        }

        addCompletionLatencySample(buckets,
                                   summary->xactionType,
                                   summary->requestOpcode,
                                   summary->endTimestamp - summary->startTimestamp,
                                   summary->firstLogicalRow);
        for (const auto& [subsequent, timestamp, logicalRow] : summary->subsequentTimestamps) {
            addSubsequentLatencySample(buckets,
                                       sampleCount,
                                       summary->xactionType,
                                       summary->requestOpcode,
                                       subsequent,
                                       timestamp - summary->startTimestamp,
                                       logicalRow);
        }
    }

    return buckets;
}

std::optional<TransactionSummary> summarizeTransaction(const std::shared_ptr<TraceSession>& session,
                                                       const std::uint64_t ordinal,
                                                       const std::vector<std::uint64_t>& logicalRows,
                                                       const std::vector<FlitRecord>& rows)
{
    if (logicalRows.empty() || rows.empty() || logicalRows.size() != rows.size()) {
        return std::nullopt;
    }

    TransactionSummary summary;
    summary.xactionType = xactionTypeForRows(session, rows, logicalRows);
    summary.firstLogicalRow = static_cast<int>(logicalRows.front());
    summary.startTimestamp = rows.front().timestamp;
    summary.endTimestamp = rows.back().timestamp;

    std::size_t firstIndex = 0;
    for (; firstIndex < rows.size(); ++firstIndex) {
        if (rows[firstIndex].channel == FlitChannel::Req
            || rows[firstIndex].channel == FlitChannel::Snp) {
            break;
        }
    }
    if (firstIndex >= rows.size()) {
        firstIndex = 0;
    }

    const FlitRecord& first = rows[firstIndex];
    summary.requestOpcode = QStringLiteral("%1 %2")
                                .arg(ToString(first.channel), displayOpcode(first));
    summary.startTimestamp = first.timestamp;
    summary.firstLogicalRow = static_cast<int>(logicalRows[firstIndex]);

    QSet<QString> seenDataOpcodes;
    for (std::size_t index = firstIndex + 1; index < rows.size(); ++index) {
        const FlitRecord& row = rows[index];
        if (row.channel == FlitChannel::Req || row.channel == FlitChannel::Snp) {
            continue;
        }

        const QString label = subsequentLabel(row, seenDataOpcodes);
        summary.subsequentTimestamps.emplace_back(label,
                                                  row.timestamp,
                                                  static_cast<int>(logicalRows[index]));
        summary.endTimestamp = std::max(summary.endTimestamp, row.timestamp);
    }

    if (summary.subsequentTimestamps.empty()) {
        summary.subsequentTimestamps.emplace_back(QStringLiteral("No subsequent flits"),
                                                  summary.endTimestamp,
                                                  summary.firstLogicalRow);
    }

    if (ordinal == 0) {
        return std::nullopt;
    }

    summary.valid = true;
    return summary;
}

double niceLatencyStep(const double roughStep)
{
    if (!std::isfinite(roughStep) || roughStep <= 0.0) {
        return 1.0;
    }

    const double exponent = std::floor(std::log10(roughStep));
    const double magnitude = std::pow(10.0, exponent);
    const double normalized = roughStep / magnitude;
    double nice = 10.0;
    if (normalized <= 1.0) {
        nice = 1.0;
    } else if (normalized <= 2.0) {
        nice = 2.0;
    } else if (normalized <= 5.0) {
        nice = 5.0;
    }
    return nice * magnitude;
}

std::pair<long double, long double> visibleLatencyRangeFor(
    const QStyleOptionViewItem& option,
    const LatencyStats& stats)
{
    const QObject* object = option.widget;
    while (object) {
        if (const auto* tree = qobject_cast<const QTreeWidget*>(object)) {
            bool minOk = false;
            bool maxOk = false;
            const double minValue = tree->property(kPlotVisibleMinProperty).toDouble(&minOk);
            const double maxValue = tree->property(kPlotVisibleMaxProperty).toDouble(&maxOk);
            if (minOk && maxOk && std::isfinite(minValue) && std::isfinite(maxValue)
                && maxValue > minValue) {
                return {static_cast<long double>(minValue), static_cast<long double>(maxValue)};
            }
            break;
        }
        object = object->parent();
    }

    long double fallbackMin = static_cast<long double>(stats.min);
    long double fallbackMax = static_cast<long double>(stats.max);
    if (fallbackMax <= fallbackMin) {
        fallbackMax = fallbackMin + 1.0L;
    }
    return {fallbackMin, fallbackMax};
}

bool boolTreeProperty(const QStyleOptionViewItem& option, const char* propertyName)
{
    const QObject* object = option.widget;
    while (object) {
        if (const auto* tree = qobject_cast<const QTreeWidget*>(object)) {
            return tree->property(propertyName).toBool();
        }
        object = object->parent();
    }
    return false;
}

std::optional<double> doubleTreeProperty(const QStyleOptionViewItem& option, const char* propertyName)
{
    const QObject* object = option.widget;
    while (object) {
        if (const auto* tree = qobject_cast<const QTreeWidget*>(object)) {
            bool ok = false;
            const double value = tree->property(propertyName).toDouble(&ok);
            if (ok && std::isfinite(value)) {
                return value;
            }
            break;
        }
        object = object->parent();
    }
    return std::nullopt;
}

LatencyPlotType plotTypeForOption(const QStyleOptionViewItem& option)
{
    const QObject* object = option.widget;
    while (object) {
        if (const auto* tree = qobject_cast<const QTreeWidget*>(object)) {
            const int value = tree->property(kPlotTypeProperty).toInt();
            if (value == static_cast<int>(LatencyPlotType::Jitter)) {
                return LatencyPlotType::Jitter;
            }
            if (value == static_cast<int>(LatencyPlotType::Violin)) {
                return LatencyPlotType::Violin;
            }
            return LatencyPlotType::Box;
        }
        object = object->parent();
    }
    return LatencyPlotType::Box;
}

QRect plotTrackRectForCell(const QRect& cell)
{
    QRect track = cell.adjusted(kPlotHorizontalPadding,
                                kPlotTopPadding,
                                -kPlotHorizontalPadding,
                                -kPlotTopPadding);
    if (track.height() < kPlotMinimumTrackHeight) {
        const int centerY = cell.center().y();
        track.setTop(centerY - kPlotMinimumTrackHeight / 2);
        track.setBottom(centerY + kPlotMinimumTrackHeight / 2);
    }
    return track;
}

QPoint jitterDotPointForSample(const LatencyStats& stats,
                               const QRect& track,
                               const qint64 sample,
                               const std::size_t index,
                               const std::function<int(long double)>& xFor)
{
    const int radius = stats.count > 300 ? 2 : 3;
    const int verticalSpan = qMax(1, track.height() - 14);
    const int midY = track.center().y();
    const int x = xFor(static_cast<long double>(sample));
    const std::uint32_t hash = static_cast<std::uint32_t>(
        (index + 1U) * 2654435761U
        ^ static_cast<std::uint32_t>(static_cast<std::uint64_t>(sample) >> 3U));
    const int offset = static_cast<int>(hash % static_cast<std::uint32_t>(verticalSpan))
        - verticalSpan / 2;
    const int y = qBound(track.top() + radius + 2, midY + offset, track.bottom() - radius - 2);
    return QPoint(x, y);
}

void paintBoxPlotBody(QPainter& painter,
                      const LatencyStats& stats,
                      const QRect& track,
                      const std::function<int(long double)>& xFor,
                      const std::function<bool(int, int)>& xVisible)
{
    const int lowerWhiskerX = xFor(static_cast<long double>(stats.lowerWhisker));
    const int q1X = xFor(static_cast<long double>(stats.q1));
    const int medianX = xFor(static_cast<long double>(stats.median));
    const int q3X = xFor(static_cast<long double>(stats.q3));
    const int upperWhiskerX = xFor(static_cast<long double>(stats.upperWhisker));
    const int midY = track.center().y();
    const int boxHeight = qBound(16, (track.height() * 3) / 4, track.height() - 4);
    QRect boxRect(QPoint(qMin(q1X, q3X), midY - boxHeight / 2),
                  QPoint(qMax(q1X, q3X), midY + boxHeight / 2));
    if (boxRect.width() < 3) {
        boxRect.adjust(-1, 0, 1, 0);
    }

    painter.setPen(QPen(QColor(QStringLiteral("#4B5563")), 1));
    painter.drawLine(lowerWhiskerX, midY, qMin(q1X, q3X), midY);
    painter.drawLine(qMax(q1X, q3X), midY, upperWhiskerX, midY);
    painter.drawLine(lowerWhiskerX, track.top() + 5, lowerWhiskerX, track.bottom() - 5);
    painter.drawLine(upperWhiskerX, track.top() + 5, upperWhiskerX, track.bottom() - 5);
    painter.setBrush(QColor(QStringLiteral("#F3F4F6")));
    painter.setPen(QPen(QColor(QStringLiteral("#111111")), 1));
    painter.drawRect(boxRect);
    painter.setPen(QPen(QColor(QStringLiteral("#000000")), 2));
    painter.drawLine(medianX, boxRect.top() + 1, medianX, boxRect.bottom() - 1);

    if (!stats.outliers.empty()) {
        const int radius = stats.count > 200 ? 3 : 4;
        for (const qint64 outlier : stats.outliers) {
            const int x = xFor(static_cast<long double>(outlier));
            if (!xVisible(x, radius)) {
                continue;
            }
            const bool leftOutlier = outlier < stats.lowerWhisker;
            const bool rightOutlier = outlier > stats.upperWhisker;
            const QColor color = leftOutlier
                ? QColor(QStringLiteral("#36B66D"))
                : (rightOutlier ? QColor(QStringLiteral("#D94D4D"))
                                : QColor(QStringLiteral("#B9BEC5")));
            painter.setBrush(color);
            painter.setPen(QPen(color.darker(135), 1));
            painter.drawEllipse(QPoint(x, midY), radius, radius);
        }
    }
}

void paintJitterPlotBody(QPainter& painter,
                         const LatencyStats& stats,
                         const QRect& track,
                         const std::function<int(long double)>& xFor,
                         const std::function<bool(int, int)>& xVisible)
{
    const int midY = track.center().y();
    painter.setPen(QPen(QColor(QStringLiteral("#CBD1D9")), 1));
    painter.drawLine(track.left(), midY, track.right(), midY);
    painter.setPen(QPen(QColor(QStringLiteral("#111827")), 2));
    const int medianX = xFor(static_cast<long double>(stats.median));
    if (xVisible(medianX, 0)) {
        painter.drawLine(medianX, track.top() + 4, medianX, track.bottom() - 4);
    }

    const int radius = stats.count > 300 ? 2 : 3;
    painter.setBrush(QColor(QStringLiteral("#3B414A")));
    painter.setPen(QPen(QColor(QStringLiteral("#1F242B")), 1));
    const std::vector<qint64>& samples =
        stats.renderedSamples.empty() ? stats.outliers : stats.renderedSamples;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const QPoint point = jitterDotPointForSample(stats, track, samples[index], index, xFor);
        const int x = point.x();
        if (!xVisible(x, radius)) {
            continue;
        }
        painter.drawEllipse(point, radius, radius);
    }
}

void paintViolinPlotBody(QPainter& painter,
                         const LatencyStats& stats,
                         const QRect& track,
                         const std::function<int(long double)>& xFor,
                         long double visibleMin,
                         long double visibleMax)
{
    if (stats.renderedSamples.empty()) {
        paintBoxPlotBody(painter,
                         stats,
                         track,
                         xFor,
                         [](int, int) {
                             return true;
                         });
        return;
    }

    constexpr int kBins = 48;
    std::array<double, kBins> bins{};
    const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
    for (const qint64 sample : stats.renderedSamples) {
        if (static_cast<long double>(sample) < visibleMin
            || static_cast<long double>(sample) > visibleMax) {
            continue;
        }
        const long double ratio =
            (static_cast<long double>(sample) - visibleMin) / visibleSpan;
        const int rawIndex = static_cast<int>(std::floor(ratio * kBins));
        const int index = qBound(0, rawIndex, kBins - 1);
        bins[static_cast<std::size_t>(index)] += 1.0;
    }

    std::array<double, kBins> smoothed{};
    double maxDensity = 0.0;
    for (int index = 0; index < kBins; ++index) {
        const double left = index > 0 ? bins[static_cast<std::size_t>(index - 1)] : 0.0;
        const double center = bins[static_cast<std::size_t>(index)];
        const double right = index + 1 < kBins ? bins[static_cast<std::size_t>(index + 1)] : 0.0;
        smoothed[static_cast<std::size_t>(index)] = left * 0.25 + center * 0.5 + right * 0.25;
        maxDensity = std::max(maxDensity, smoothed[static_cast<std::size_t>(index)]);
    }
    if (maxDensity <= 0.0) {
        return;
    }

    const int midY = track.center().y();
    const int maxHalfHeight = qMax(2, (track.height() - 8) / 2);
    QPainterPath upperPath;
    QPainterPath lowerPath;
    for (int index = 0; index < kBins; ++index) {
        const long double value = visibleMin
            + (static_cast<long double>(index) + 0.5L) * visibleSpan / static_cast<long double>(kBins);
        const int x = xFor(value);
        const int halfHeight = static_cast<int>(std::llround(
            smoothed[static_cast<std::size_t>(index)] / maxDensity * maxHalfHeight));
        const QPointF upper(x, midY - halfHeight);
        const QPointF lower(x, midY + halfHeight);
        if (index == 0) {
            upperPath.moveTo(upper);
            lowerPath.moveTo(lower);
        } else {
            upperPath.lineTo(upper);
            lowerPath.lineTo(lower);
        }
    }

    QPainterPath violinPath = upperPath;
    for (int index = kBins - 1; index >= 0; --index) {
        const long double value = visibleMin
            + (static_cast<long double>(index) + 0.5L) * visibleSpan / static_cast<long double>(kBins);
        const int x = xFor(value);
        const int halfHeight = static_cast<int>(std::llround(
            smoothed[static_cast<std::size_t>(index)] / maxDensity * maxHalfHeight));
        violinPath.lineTo(QPointF(x, midY + halfHeight));
    }
    violinPath.closeSubpath();

    painter.setPen(QPen(QColor(QStringLiteral("#111827")), 1));
    painter.setBrush(QColor(QStringLiteral("#E7EAEE")));
    painter.drawPath(violinPath);
    painter.setPen(QPen(QColor(QStringLiteral("#6B7280")), 1, Qt::DashLine));
    painter.drawLine(track.left(), midY, track.right(), midY);

    const int medianX = xFor(static_cast<long double>(stats.median));
    const int q1X = xFor(static_cast<long double>(stats.q1));
    const int q3X = xFor(static_cast<long double>(stats.q3));
    painter.setPen(QPen(QColor(QStringLiteral("#111111")), 2));
    painter.drawLine(medianX, track.top() + 5, medianX, track.bottom() - 5);
    painter.setPen(QPen(QColor(QStringLiteral("#4B5563")), 2));
    painter.drawLine(q1X, midY, q3X, midY);
}

int snappedPlotGestureOctant(const QPoint& delta)
{
    if (delta.isNull()) {
        return 0;
    }

    int octant = static_cast<int>(std::lround(std::atan2(static_cast<double>(delta.y()),
                                                         static_cast<double>(delta.x()))
                                              / (3.14159265358979323846 / 4.0)));
    if (octant > 4) {
        octant -= 8;
    } else if (octant < -4) {
        octant += 8;
    }
    return octant;
}

bool plotGestureDirectionReady(const QPoint& delta)
{
    const int dx = delta.x();
    const int dy = delta.y();
    return dx * dx + dy * dy >= kPlotGestureDirectionDistance * kPlotGestureDirectionDistance;
}

QColor withAlpha(QColor color, const int alpha)
{
    color.setAlpha(alpha);
    return color;
}

class BoxPlotDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const QVariant plotData = index.data(kPlotRole);
        if (!plotData.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem base(option);
        initStyleOption(&base, index);
        base.text.clear();
        if (const QWidget* widget = base.widget) {
            widget->style()->drawControl(QStyle::CE_ItemViewItem, &base, painter, widget);
        } else {
            QStyledItemDelegate::paint(painter, base, index);
        }

        const std::optional<LatencyStats> decodedStats = statsFromVariant(plotData);
        if (!decodedStats) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const LatencyStats stats = *decodedStats;
        if (stats.count == 0) {
            return;
        }

        const QRect track = plotTrackRectForCell(option.rect);
        if (track.width() <= 16 || track.height() <= 10) {
            return;
        }

        const auto [visibleMin, visibleMax] = visibleLatencyRangeFor(option, stats);
        const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
        const auto xFor = [&](const long double value) {
            const long double ratio =
                (value - visibleMin) / visibleSpan;
            return track.left() + static_cast<int>(std::llround(ratio * static_cast<long double>(track.width() - 1)));
        };
        const auto xVisible = [&](const int x, const int margin = 4) {
            return x >= track.left() - margin && x <= track.right() + margin;
        };

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        painter->fillRect(track, QColor(QStringLiteral("#FFFFFF")));
        painter->setPen(QPen(QColor(QStringLiteral("#D7DCE2")), 1));
        painter->drawRect(track.adjusted(0, 0, -1, -1));

        const double majorStep = niceLatencyStep(static_cast<double>(visibleSpan / 5.0L));
        const long double firstTick =
            std::ceil(visibleMin / static_cast<long double>(majorStep))
            * static_cast<long double>(majorStep);
        for (long double tick = firstTick;
             tick <= visibleMax + static_cast<long double>(majorStep) * 0.5L;
             tick += static_cast<long double>(majorStep)) {
            const int x = xFor(tick);
            if (!xVisible(x, 0)) {
                continue;
            }

            painter->setPen(QPen(QColor(QStringLiteral("#ECEFF3")), 1));
            painter->drawLine(x, track.top() + 1, x, track.bottom() - 1);

            if (tick > static_cast<long double>((std::numeric_limits<qint64>::max)())
                    - static_cast<long double>(majorStep)
                || majorStep <= 0.0) {
                break;
            }
        }

        if (const std::optional<double> cursorValue = doubleTreeProperty(option, kPlotCursorValueProperty);
            cursorValue && boolTreeProperty(option, kPlotCursorVisibleProperty)) {
            const int cursorX = xFor(static_cast<long double>(*cursorValue));
            if (xVisible(cursorX, 0)) {
                painter->setPen(QPen(QColor(QStringLiteral("#111827")), 1, Qt::DashLine));
                painter->drawLine(cursorX, track.top(), cursorX, track.bottom());
                QFont scaleFont = painter->font();
                scaleFont.setPointSizeF(qMax<qreal>(6.5, scaleFont.pointSizeF() * 0.76));
                painter->setFont(scaleFont);
                const QFontMetrics scaleMetrics(scaleFont);
                const QString label = latencyText(static_cast<qint64>(std::llround(*cursorValue)));
                const int textWidth = scaleMetrics.horizontalAdvance(label) + 8;
                const QRect labelRect(qBound(track.left(),
                                             cursorX - textWidth / 2,
                                             qMax(track.left(), track.right() - textWidth + 1)),
                                      track.top() + 2,
                                      textWidth,
                                      15);
                painter->fillRect(labelRect, QColor(QStringLiteral("#111827")));
                painter->setPen(QColor(QStringLiteral("#FFFFFF")));
                painter->drawText(labelRect, Qt::AlignCenter, label);
            }
        }

        if (boolTreeProperty(option, kPlotDragActiveProperty)) {
            const std::optional<double> startX = doubleTreeProperty(option, kPlotDragStartXProperty);
            const std::optional<double> lastX = doubleTreeProperty(option, kPlotDragLastXProperty);
            if (startX && lastX) {
                const int left = qBound(track.left(),
                                        static_cast<int>(std::llround(std::min(*startX, *lastX))),
                                        track.right());
                const int right = qBound(track.left(),
                                         static_cast<int>(std::llround(std::max(*startX, *lastX))),
                                         track.right());
                if (right - left >= 2) {
                    const QRect selection(left, track.top(), right - left + 1, track.height());
                    painter->fillRect(selection, QColor(47, 128, 237, 42));
                    painter->setPen(QPen(QColor(QStringLiteral("#2F80ED")), 1));
                    painter->drawRect(selection.adjusted(0, 0, -1, -1));
                }
            }
        }

        painter->setClipRect(track.adjusted(0, 0, 1, 1));
        switch (plotTypeForOption(option)) {
        case LatencyPlotType::Jitter:
            paintJitterPlotBody(*painter, stats, track, xFor, xVisible);
            break;
        case LatencyPlotType::Violin:
            paintViolinPlotBody(*painter, stats, track, xFor, visibleMin, visibleMax);
            break;
        case LatencyPlotType::Box:
        default:
            paintBoxPlotBody(*painter, stats, track, xFor, xVisible);
            break;
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        if (index.data(kPlotRole).isValid()) {
            return QSize(qMax(base.width(), 420), qMax(base.height(), 58));
        }
        return base;
    }
};

}  // namespace

class LatencyWidget::ScaleWidget final : public QWidget {
public:
    explicit ScaleWidget(LatencyWidget* owner)
        : QWidget(owner)
        , owner_(owner)
    {
        setFixedHeight(kLatencyScaleHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        if (owner_) {
            owner_->paintScale(painter, rect());
        }
    }

private:
    LatencyWidget* owner_ = nullptr;
};

class LatencyWidget::PlotOverlayWidget final : public QWidget {
public:
    explicit PlotOverlayWidget(LatencyWidget* owner, QWidget* parent)
        : QWidget(parent)
        , owner_(owner)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!owner_) {
            return;
        }

        QPainter painter(this);
        owner_->paintPlotGestureOverlay(painter, rect());
    }

private:
    LatencyWidget* owner_ = nullptr;
};

class LatencyWidget::BuildPromptOverlayWidget final : public QWidget {
public:
    explicit BuildPromptOverlayWidget(LatencyWidget* owner)
        : QWidget(owner)
        , owner_(owner)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!owner_) {
            return;
        }

        QPainter painter(this);
        owner_->paintBuildPrompt(painter, rect());
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (owner_ && event->button() == Qt::LeftButton) {
            if (owner_->hasPendingBuild()) {
                owner_->buildView();
            }
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (owner_
            && (event->key() == Qt::Key_Return
                || event->key() == Qt::Key_Enter
                || event->key() == Qt::Key_Space)) {
            if (owner_->hasPendingBuild()) {
                owner_->buildView();
            }
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    LatencyWidget* owner_ = nullptr;
};

LatencyWidget::LatencyWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    statusLabel_->setWordWrap(true);
    headerLayout->addWidget(statusLabel_, 1);

    auto* plotTypeLabel = new QLabel(QStringLiteral("Plot"), this);
    plotTypeLabel->setObjectName(QStringLiteral("secondaryLabel"));
    headerLayout->addWidget(plotTypeLabel);

    plotTypeCombo_ = new QComboBox(this);
    plotTypeCombo_->addItem(QStringLiteral("Box"), static_cast<int>(LatencyPlotType::Box));
    plotTypeCombo_->addItem(QStringLiteral("Jitter"), static_cast<int>(LatencyPlotType::Jitter));
    plotTypeCombo_->addItem(QStringLiteral("Violin"), static_cast<int>(LatencyPlotType::Violin));
    plotTypeCombo_->setCurrentIndex(0);
    plotTypeCombo_->setFixedHeight(22);
    plotTypeCombo_->setMinimumWidth(86);
    headerLayout->addWidget(plotTypeCombo_);

    refreshButton_ = new QPushButton(QStringLiteral("Refresh"), this);
    refreshButton_->setObjectName(QStringLiteral("actionButton"));
    refreshButton_->setFixedHeight(22);
    headerLayout->addWidget(refreshButton_);

    layout->addLayout(headerLayout);

    progressBar_ = new QProgressBar(this);
    progressBar_->setTextVisible(false);
    progressBar_->setRange(0, 1000);
    progressBar_->setValue(0);
    progressBar_->hide();
    layout->addWidget(progressBar_);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({
        QStringLiteral("Classification"),
        QStringLiteral("Box Plot"),
    });
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(false);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setMinimumSectionSize(120);
    tree_->header()->setSectionResizeMode(kClassificationColumn, QHeaderView::Interactive);
    tree_->header()->setSectionResizeMode(kBoxPlotColumn, QHeaderView::Stretch);
    tree_->setColumnWidth(kClassificationColumn, 360);
    tree_->setColumnWidth(kBoxPlotColumn, 520);
    tree_->setFocusPolicy(Qt::StrongFocus);
    tree_->viewport()->setMouseTracking(true);
    tree_->viewport()->installEventFilter(this);
    tree_->installEventFilter(this);
    tree_->setItemDelegateForColumn(kBoxPlotColumn, new BoxPlotDelegate(tree_));
    plotOverlay_ = new PlotOverlayWidget(this, tree_->viewport());

    scaleWidget_ = new ScaleWidget(this);

    summaryPanel_ = new QFrame(this);
    summaryPanel_->setObjectName(QStringLiteral("latencySummaryPanel"));
    summaryPanel_->setFrameShape(QFrame::StyledPanel);
    summaryPanel_->setFixedWidth(kLatencySummaryWidth);
    summaryPanel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    summaryPanel_->setStyleSheet(QStringLiteral(
        "QFrame#latencySummaryPanel { background:#FFFFFF; border:1px solid #D7DCE2; border-radius:4px; }"));
    auto* summaryLayout = new QVBoxLayout(summaryPanel_);
    summaryLayout->setContentsMargins(10, 8, 10, 8);
    summaryLayout->setSpacing(6);
    summaryTitleLabel_ = new QLabel(QStringLiteral("Summary"), summaryPanel_);
    QFont summaryTitleFont = summaryTitleLabel_->font();
    summaryTitleFont.setBold(true);
    summaryTitleLabel_->setFont(summaryTitleFont);
    summaryTitleLabel_->setWordWrap(true);
    summaryLayout->addWidget(summaryTitleLabel_);
    summaryBodyLabel_ = new QLabel(summaryPanel_);
    summaryBodyLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    summaryBodyLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    summaryBodyLabel_->setWordWrap(true);
    summaryLayout->addWidget(summaryBodyLabel_, 1);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    auto* plotLayout = new QVBoxLayout();
    plotLayout->setContentsMargins(0, 0, 0, 0);
    plotLayout->setSpacing(0);
    plotLayout->addWidget(tree_, 1);
    plotLayout->addWidget(scaleWidget_);
    contentLayout->addLayout(plotLayout, 1);
    contentLayout->addWidget(summaryPanel_);
    layout->addLayout(contentLayout, 1);

    buildPromptOverlay_ = new BuildPromptOverlayWidget(this);

    connect(refreshButton_, &QPushButton::clicked, this, &LatencyWidget::refresh);
    connect(plotTypeCombo_, &QComboBox::currentIndexChanged, this, [this](const int index) {
        const int value = plotTypeCombo_->itemData(index).toInt();
        currentPlotType_ = value;
        if (tree_ && tree_->headerItem()) {
            tree_->headerItem()->setText(kBoxPlotColumn,
                                         plotTypeCombo_->currentText() + QStringLiteral(" Plot"));
        }
        updatePlotViewProperties();
    });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item) {
            return;
        }
        const int logicalRow = item->data(0, kLogicalRowRole).toInt();
        if (logicalRow >= 0) {
            Q_EMIT logicalRowActivated(logicalRow);
        }
    });
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this]() {
        if (tree_ && tree_->viewport()) {
            tree_->viewport()->update();
        }
        updateSelectionSummary();
    });

    updatePlotViewProperties();
    updateSelectionSummary();
    setIdleText(QStringLiteral("Open a CHI E.b trace and build the Xaction index to populate latency box plots."));
}

LatencyWidget::~LatencyWidget()
{
    stopBuildThread();
}

void LatencyWidget::setTraceSession(std::shared_ptr<TraceSession> session)
{
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_ = std::move(session);
    pendingRows_.clear();
    lastBuildLoadedFromCache_ = false;
    buildRequested_ = false;
    building_ = false;
    buildProgressProcessedRows_ = 0;
    buildProgressTotalRows_ = traceSession_ ? traceSession_->totalRows() : 0;
    tree_->clear();
    clearPlotDataRange();
    progressBar_->hide();
    setIdleText(traceSession_ ? pendingBuildText()
                              : QStringLiteral("Open a CHI E.b trace and build the Xaction index to populate latency box plots."));
    updateBuildPromptOverlay();
}

void LatencyWidget::setRows(std::vector<FlitRecord> rows)
{
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_.reset();
    pendingRows_ = std::move(rows);
    lastBuildLoadedFromCache_ = false;
    building_ = false;
    buildProgressProcessedRows_ = 0;
    buildProgressTotalRows_ = pendingRows_.size();
    tree_->clear();
    clearPlotDataRange();
    progressBar_->hide();
    buildRequested_ = false;
    setIdleText(pendingRows_.empty() ? QStringLiteral("No flits in the current trace.")
                                     : pendingBuildText());
    updateBuildPromptOverlay();
}

void LatencyWidget::buildView()
{
    stopBuildThread();
    buildRequested_ = true;
    updateBuildPromptOverlay();
    refresh();
}

QVariantMap LatencyWidget::viewState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("plotZoom"), plotZoom_);
    state.insert(QStringLiteral("plotCenter"), static_cast<double>(plotCenter_));
    state.insert(QStringLiteral("plotHasCursor"), plotHasCursor_);
    state.insert(QStringLiteral("plotCursorValue"), static_cast<double>(plotCursorValue_));
    state.insert(QStringLiteral("plotType"), plotTypeCombo_ ? plotTypeCombo_->currentText() : QString());
    state.insert(QStringLiteral("classificationColumnWidth"),
                 tree_ ? tree_->columnWidth(kClassificationColumn) : 0);
    state.insert(QStringLiteral("plotColumnWidth"),
                 tree_ ? tree_->columnWidth(kBoxPlotColumn) : 0);

    QStringList selectedPath;
    if (tree_ && tree_->currentItem()) {
        for (QTreeWidgetItem* item = tree_->currentItem(); item; item = item->parent()) {
            selectedPath.prepend(item->text(kClassificationColumn));
        }
    }
    state.insert(QStringLiteral("selectedPath"), selectedPath);

    QStringList expandedPaths;
    if (tree_) {
        const std::function<void(QTreeWidgetItem*, QStringList)> collectExpanded =
            [&](QTreeWidgetItem* item, QStringList path) {
                if (!item) {
                    return;
                }
                path.append(item->text(kClassificationColumn));
                if (item->isExpanded()) {
                    expandedPaths.append(path.join(QLatin1Char('/')));
                }
                for (int index = 0; index < item->childCount(); ++index) {
                    collectExpanded(item->child(index), path);
                }
            };
        for (int index = 0; index < tree_->topLevelItemCount(); ++index) {
            collectExpanded(tree_->topLevelItem(index), {});
        }
    }
    state.insert(QStringLiteral("expandedPaths"), expandedPaths);
    return state;
}

void LatencyWidget::restoreViewState(const QVariantMap& state)
{
    pendingViewState_ = state;
    applyViewState(state);
}

void LatencyWidget::clear()
{
    stopBuildThread();
    pendingViewState_.clear();
    traceSession_.reset();
    pendingRows_.clear();
    lastBuildLoadedFromCache_ = false;
    buildRequested_ = false;
    building_ = false;
    buildProgressProcessedRows_ = 0;
    buildProgressTotalRows_ = 0;
    tree_->clear();
    clearPlotDataRange();
    progressBar_->hide();
    setIdleText(QStringLiteral("Open a CHI E.b trace and build the Xaction index to populate latency box plots."));
    updateBuildPromptOverlay();
}

bool LatencyWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (!tree_ || !event) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == tree_->viewport()) {
        switch (event->type()) {
        case QEvent::Wheel: {
            auto* wheel = static_cast<QWheelEvent*>(event);
            const QPoint position = wheel->position().toPoint();
            if (!positionInPlotColumn(position) || !plotHasDataRange_) {
                break;
            }

            const QPoint pixelDelta = wheel->pixelDelta();
            const QPoint angleDelta = wheel->angleDelta();
            const int verticalDelta = pixelDelta.y() != 0 ? pixelDelta.y() : angleDelta.y();
            const int horizontalDelta = pixelDelta.x() != 0 ? pixelDelta.x() : angleDelta.x();

            if ((wheel->modifiers() & Qt::ControlModifier) && verticalDelta != 0) {
                zoomPlotByFactor(verticalDelta > 0 ? 1.25 : 0.8, position);
                event->accept();
                return true;
            }

            if (wheel->modifiers() & Qt::ShiftModifier) {
                const int delta = verticalDelta == 0 ? horizontalDelta : verticalDelta;
                if (delta != 0) {
                    const QRect track = plotTrackViewportRect();
                    panPlotByPixels(delta < 0 ? track.width() / 4 : -track.width() / 4);
                    event->accept();
                    return true;
                }
            }

            if (horizontalDelta != 0) {
                panPlotByPixels(horizontalDelta < 0 ? 80 : -80);
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonPress: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton
                && positionInPlotColumn(mouse->pos())
                && plotHasDataRange_) {
                tree_->setFocus(Qt::MouseFocusReason);
                selectPlotItemAt(mouse->pos());
                setPlotCursorFromPosition(mouse->pos());
                beginPlotLeftDrag(mouse->pos(), mouse->modifiers());
                event->accept();
                return true;
            }

            if (((mouse->button() == Qt::MiddleButton)
                    || (mouse->button() == Qt::LeftButton
                        && (mouse->modifiers() & Qt::ShiftModifier)))
                && positionInPlotColumn(mouse->pos())
                && plotHasDataRange_) {
                tree_->setFocus(Qt::MouseFocusReason);
                plotPanning_ = true;
                plotPanStart_ = mouse->pos();
                plotPanStartCenter_ = plotCenter_;
                tree_->viewport()->setCursor(Qt::ClosedHandCursor);
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (plotDragMode_ != PlotDragMode::None) {
                setPlotCursorFromPosition(mouse->pos());
                updatePlotLeftDrag(mouse->pos(), mouse->modifiers());
                event->accept();
                return true;
            }

            if (positionInPlotColumn(mouse->pos()) && plotHasDataRange_) {
                setPlotCursorFromPosition(mouse->pos());
            }

            if (!plotPanning_) {
                break;
            }
            const QRect track = plotTrackViewportRect();
            const auto [visibleMin, visibleMax] = visiblePlotRange();
            const long double unitsPerPixel =
                (visibleMax - visibleMin) / static_cast<long double>(qMax(1, track.width()));
            plotCenter_ = plotPanStartCenter_
                - static_cast<long double>(mouse->pos().x() - plotPanStart_.x()) * unitsPerPixel;
            clampPlotCenter();
            updatePlotViewProperties();
            event->accept();
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (plotDragMode_ != PlotDragMode::None && mouse->button() == Qt::LeftButton) {
                finishPlotLeftDrag(mouse->pos(), mouse->modifiers());
                event->accept();
                return true;
            }

            if (plotPanning_
                && (mouse->button() == Qt::MiddleButton || mouse->button() == Qt::LeftButton)) {
                plotPanning_ = false;
                tree_->viewport()->unsetCursor();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton
                && positionInPlotColumn(mouse->pos())
                && plotHasDataRange_) {
                fitPlotView();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::Leave:
            if (!plotPanning_ && plotDragMode_ == PlotDragMode::None) {
                plotHasCursor_ = false;
                updatePlotViewProperties();
            }
            break;
        default:
            break;
        }
    } else if (watched == tree_ && event->type() == QEvent::KeyRelease) {
        if (plotDragMode_ != PlotDragMode::None) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape) {
                cancelPlotLeftDrag();
                event->accept();
                return true;
            }
        }
    } else if (watched == tree_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && plotDragMode_ != PlotDragMode::None) {
            cancelPlotLeftDrag();
            event->accept();
            return true;
        }

        const bool control = key->modifiers() & Qt::ControlModifier;
        if ((control && key->key() == Qt::Key_0) || key->key() == Qt::Key_F) {
            fitPlotView();
            event->accept();
            return true;
        }
        if (control && (key->key() == Qt::Key_Plus || key->key() == Qt::Key_Equal)) {
            zoomPlotByFactor(1.25, plotTrackViewportRect().center());
            event->accept();
            return true;
        }
        if (control && (key->key() == Qt::Key_Minus || key->key() == Qt::Key_Underscore)) {
            zoomPlotByFactor(0.8, plotTrackViewportRect().center());
            event->accept();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void LatencyWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateBuildPromptOverlay();
}

void LatencyWidget::clearPlotDataRange()
{
    plotHasDataRange_ = false;
    plotDataMin_ = 0;
    plotDataMax_ = 1;
    plotZoom_ = 1.0;
    plotCenter_ = 0.5L;
    plotHasCursor_ = false;
    plotCursorValue_ = 0.0L;
    plotDragMode_ = PlotDragMode::None;
    plotPanning_ = false;
    releasePlotMouseGrab();
    updatePlotViewProperties();
}

void LatencyWidget::includePlotSampleRange(const qint64 minimum, const qint64 maximum)
{
    if (!plotHasDataRange_) {
        plotDataMin_ = minimum;
        plotDataMax_ = maximum;
        plotHasDataRange_ = true;
        return;
    }
    plotDataMin_ = std::min(plotDataMin_, minimum);
    plotDataMax_ = std::max(plotDataMax_, maximum);
}

void LatencyWidget::fitPlotView()
{
    const auto [minimum, maximum] = effectivePlotDataRange();
    plotZoom_ = 1.0;
    plotCenter_ = minimum + (maximum - minimum) / 2.0L;
    clampPlotCenter();
    updatePlotViewProperties();
}

void LatencyWidget::updatePlotViewProperties()
{
    if (!tree_) {
        return;
    }

    const bool rangeZoomActive = plotDragMode_ == PlotDragMode::RangeZoom
        || plotDragMode_ == PlotDragMode::ZoomFull
        || plotDragMode_ == PlotDragMode::ZoomIn2x
        || plotDragMode_ == PlotDragMode::ZoomOut2x;

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    tree_->setProperty(kPlotVisibleMinProperty, static_cast<double>(visibleMin));
    tree_->setProperty(kPlotVisibleMaxProperty, static_cast<double>(visibleMax));
    tree_->setProperty(kPlotCursorVisibleProperty, plotHasCursor_);
    tree_->setProperty(kPlotCursorValueProperty, static_cast<double>(plotCursorValue_));
    tree_->setProperty(kPlotDragActiveProperty, rangeZoomActive);
    tree_->setProperty(kPlotDragStartXProperty, plotDragStart_.x());
    tree_->setProperty(kPlotDragLastXProperty, plotDragLast_.x());
    tree_->setProperty(kPlotTypeProperty, currentPlotType_);
    if (tree_->viewport()) {
        tree_->viewport()->update();
    }
    if (plotOverlay_) {
        const bool overlayVisible = plotDragMode_ != PlotDragMode::None
            && plotDragMode_ != PlotDragMode::Pan;
        plotOverlay_->setGeometry(tree_->viewport()->rect());
        plotOverlay_->setVisible(overlayVisible);
        if (overlayVisible) {
            plotOverlay_->raise();
            plotOverlay_->update();
        }
    }
    if (scaleWidget_) {
        scaleWidget_->update();
    }
}

void LatencyWidget::clampPlotCenter()
{
    const auto [minimum, maximum] = effectivePlotDataRange();
    const long double dataSpan = std::max<long double>(1.0L, maximum - minimum);
    const long double visibleSpan =
        dataSpan / static_cast<long double>(std::clamp(plotZoom_, kMinLatencyPlotZoom, kMaxLatencyPlotZoom));
    if (visibleSpan >= dataSpan) {
        plotCenter_ = minimum + dataSpan / 2.0L;
        return;
    }

    const long double halfSpan = visibleSpan / 2.0L;
    plotCenter_ = std::clamp(plotCenter_, minimum + halfSpan, maximum - halfSpan);
}

void LatencyWidget::zoomPlotByFactor(const double factor, const QPoint& focus)
{
    if (!plotHasDataRange_ || factor <= 0.0) {
        return;
    }

    const QRect track = plotTrackViewportRect();
    if (track.width() <= 1) {
        return;
    }

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
    const int focusX = qBound(track.left(), focus.x(), track.right());
    const long double focusRatio =
        static_cast<long double>(focusX - track.left())
        / static_cast<long double>(qMax(1, track.width() - 1));
    const long double focusValue = visibleMin + visibleSpan * focusRatio;

    plotZoom_ = std::clamp(plotZoom_ * factor, kMinLatencyPlotZoom, kMaxLatencyPlotZoom);
    const auto [minimum, maximum] = effectivePlotDataRange();
    const long double dataSpan = std::max<long double>(1.0L, maximum - minimum);
    const long double nextSpan = dataSpan / static_cast<long double>(plotZoom_);
    plotCenter_ = focusValue + nextSpan * (0.5L - focusRatio);
    clampPlotCenter();
    updatePlotViewProperties();
}

void LatencyWidget::zoomPlotToRange(long double firstValue, long double lastValue)
{
    if (!plotHasDataRange_) {
        return;
    }

    if (firstValue > lastValue) {
        std::swap(firstValue, lastValue);
    }

    const auto [minimum, maximum] = effectivePlotDataRange();
    firstValue = std::clamp(firstValue, minimum, maximum);
    lastValue = std::clamp(lastValue, minimum, maximum);
    if (lastValue - firstValue <= 0.0L) {
        return;
    }

    const long double dataSpan = std::max<long double>(1.0L, maximum - minimum);
    const long double requestedSpan = std::max<long double>(1.0L, lastValue - firstValue);
    plotZoom_ = std::clamp(static_cast<double>(dataSpan / requestedSpan),
                           kMinLatencyPlotZoom,
                           kMaxLatencyPlotZoom);
    plotCenter_ = firstValue + requestedSpan / 2.0L;
    clampPlotCenter();
    updatePlotViewProperties();
}

void LatencyWidget::panPlotByPixels(const int pixels)
{
    if (!plotHasDataRange_ || pixels == 0) {
        return;
    }

    const QRect track = plotTrackViewportRect();
    if (track.width() <= 1) {
        return;
    }

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    const long double unitsPerPixel =
        (visibleMax - visibleMin) / static_cast<long double>(qMax(1, track.width()));
    plotCenter_ += static_cast<long double>(pixels) * unitsPerPixel;
    clampPlotCenter();
    updatePlotViewProperties();
}

void LatencyWidget::releasePlotMouseGrab()
{
    if (!tree_ || !tree_->viewport()) {
        return;
    }

    tree_->viewport()->releaseMouse();
    tree_->viewport()->unsetCursor();
}

QRect LatencyWidget::plotColumnViewportRect() const
{
    if (!tree_ || !tree_->header() || !tree_->viewport()) {
        return QRect();
    }

    return QRect(tree_->header()->sectionViewportPosition(kBoxPlotColumn),
                 0,
                 tree_->header()->sectionSize(kBoxPlotColumn),
                 tree_->viewport()->height());
}

QRect LatencyWidget::plotTrackViewportRect() const
{
    return plotColumnViewportRect().adjusted(kPlotHorizontalPadding,
                                             0,
                                             -kPlotHorizontalPadding,
                                             0);
}

bool LatencyWidget::positionInPlotColumn(const QPoint& position) const
{
    return plotColumnViewportRect().contains(position);
}

void LatencyWidget::selectPlotItemAt(const QPoint& position)
{
    if (!tree_) {
        return;
    }

    QTreeWidgetItem* item = tree_->itemAt(position);
    if (!item) {
        return;
    }
    tree_->setCurrentItem(item, kBoxPlotColumn, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void LatencyWidget::setPlotCursorFromPosition(const QPoint& position)
{
    const std::optional<long double> value = plotValueAtX(position.x());
    if (!value) {
        plotHasCursor_ = false;
        updatePlotViewProperties();
        return;
    }

    plotHasCursor_ = true;
    const std::optional<long double> magneticValue = magneticPlotValueAtPosition(position);
    plotCursorValue_ = magneticValue.value_or(*value);
    updatePlotViewProperties();
}

std::optional<long double> LatencyWidget::magneticPlotValueAtPosition(const QPoint& position) const
{
    if (!tree_ || !plotHasDataRange_) {
        return std::nullopt;
    }

    QTreeWidgetItem* item = tree_->itemAt(position);
    if (!item) {
        return std::nullopt;
    }

    const std::optional<LatencyStats> stats =
        statsFromVariant(item->data(kBoxPlotColumn, kPlotRole));
    if (!stats || stats->count == 0) {
        return std::nullopt;
    }

    const QRect rowRect = tree_->visualItemRect(item);
    const QRect column = plotColumnViewportRect();
    const QRect cell(column.left(), rowRect.top(), column.width(), rowRect.height());
    const QRect track = plotTrackRectForCell(cell);
    if (track.width() <= 1 || track.height() <= 1) {
        return std::nullopt;
    }

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
    const auto xFor = [&](const long double value) {
        const long double ratio = (value - visibleMin) / visibleSpan;
        return track.left() + static_cast<int>(
            std::llround(ratio * static_cast<long double>(track.width() - 1)));
    };

    struct DotCandidate {
        qint64 value = 0;
        QPoint point;
    };

    std::vector<DotCandidate> candidates;
    const auto addCandidate = [&](const qint64 value, const QPoint& point) {
        if (point.x() < track.left() - kPlotCursorMagnetRadius
            || point.x() > track.right() + kPlotCursorMagnetRadius) {
            return;
        }
        candidates.push_back({value, point});
    };

    const auto plotType = static_cast<LatencyPlotType>(currentPlotType_);
    if (plotType == LatencyPlotType::Jitter) {
        const std::vector<qint64>& samples =
            stats->renderedSamples.empty() ? stats->outliers : stats->renderedSamples;
        candidates.reserve(samples.size());
        for (std::size_t index = 0; index < samples.size(); ++index) {
            addCandidate(samples[index],
                         jitterDotPointForSample(*stats, track, samples[index], index, xFor));
        }
    } else if (plotType == LatencyPlotType::Box) {
        candidates.reserve(stats->outliers.size());
        const int midY = track.center().y();
        for (const qint64 outlier : stats->outliers) {
            addCandidate(outlier, QPoint(xFor(static_cast<long double>(outlier)), midY));
        }
    } else {
        return std::nullopt;
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    const int magnetRadiusSquared = kPlotCursorMagnetRadius * kPlotCursorMagnetRadius;
    const auto distanceSquared = [](const QPoint& first, const QPoint& second) {
        const int dx = first.x() - second.x();
        const int dy = first.y() - second.y();
        return dx * dx + dy * dy;
    };

    const DotCandidate* bestCandidate = nullptr;
    int bestDistanceSquared = magnetRadiusSquared + 1;
    for (const DotCandidate& candidate : candidates) {
        const int candidateDistanceSquared = distanceSquared(candidate.point, position);
        if (candidateDistanceSquared <= magnetRadiusSquared
            && candidateDistanceSquared < bestDistanceSquared) {
            bestCandidate = &candidate;
            bestDistanceSquared = candidateDistanceSquared;
        }
    }

    if (!bestCandidate) {
        return std::nullopt;
    }

    const int congestionRadiusSquared =
        kPlotCursorMagnetCongestionRadius * kPlotCursorMagnetCongestionRadius;
    int nearbyDots = 0;
    for (const DotCandidate& candidate : candidates) {
        if (distanceSquared(candidate.point, bestCandidate->point) <= congestionRadiusSquared) {
            ++nearbyDots;
            if (nearbyDots > kPlotCursorMagnetMaxNearbyDots) {
                return std::nullopt;
            }
        }
    }

    return static_cast<long double>(bestCandidate->value);
}

std::optional<int> LatencyWidget::plotDotLogicalRowAtPosition(const QPoint& position) const
{
    if (!tree_ || !plotHasDataRange_) {
        return std::nullopt;
    }

    QTreeWidgetItem* item = tree_->itemAt(position);
    if (!item) {
        return std::nullopt;
    }

    const std::optional<LatencyStats> stats =
        statsFromVariant(item->data(kBoxPlotColumn, kPlotRole));
    if (!stats || stats->count == 0) {
        return std::nullopt;
    }

    const QRect rowRect = tree_->visualItemRect(item);
    const QRect column = plotColumnViewportRect();
    const QRect cell(column.left(), rowRect.top(), column.width(), rowRect.height());
    const QRect track = plotTrackRectForCell(cell);
    if (track.width() <= 1 || track.height() <= 1) {
        return std::nullopt;
    }

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
    const auto xFor = [&](const long double value) {
        const long double ratio = (value - visibleMin) / visibleSpan;
        return track.left() + static_cast<int>(
            std::llround(ratio * static_cast<long double>(track.width() - 1)));
    };
    const auto distanceSquared = [](const QPoint& first, const QPoint& second) {
        const int dx = first.x() - second.x();
        const int dy = first.y() - second.y();
        return dx * dx + dy * dy;
    };

    struct DotCandidate {
        int logicalRow = -1;
        QPoint point;
    };
    std::vector<DotCandidate> candidates;
    const auto addCandidate = [&](const int logicalRow, const QPoint& point) {
        if (logicalRow < 0
            || point.x() < track.left() - kPlotDotClickRadius
            || point.x() > track.right() + kPlotDotClickRadius) {
            return;
        }
        candidates.push_back({logicalRow, point});
    };

    const auto plotType = static_cast<LatencyPlotType>(currentPlotType_);
    if (plotType == LatencyPlotType::Jitter) {
        const std::vector<qint64>& samples =
            stats->renderedSamples.empty() ? stats->outliers : stats->renderedSamples;
        const std::vector<int>& logicalRows =
            stats->renderedSamples.empty() ? stats->outlierLogicalRows
                                           : stats->renderedSampleLogicalRows;
        candidates.reserve(samples.size());
        for (std::size_t index = 0; index < samples.size() && index < logicalRows.size(); ++index) {
            addCandidate(logicalRows[index],
                         jitterDotPointForSample(*stats, track, samples[index], index, xFor));
        }
    } else if (plotType == LatencyPlotType::Box) {
        candidates.reserve(stats->outliers.size());
        const int midY = track.center().y();
        for (std::size_t index = 0;
             index < stats->outliers.size() && index < stats->outlierLogicalRows.size();
             ++index) {
            addCandidate(stats->outlierLogicalRows[index],
                         QPoint(xFor(static_cast<long double>(stats->outliers[index])), midY));
        }
    } else {
        return std::nullopt;
    }

    const int clickRadiusSquared = kPlotDotClickRadius * kPlotDotClickRadius;
    const DotCandidate* bestCandidate = nullptr;
    int bestDistanceSquared = clickRadiusSquared + 1;
    for (const DotCandidate& candidate : candidates) {
        const int candidateDistanceSquared = distanceSquared(candidate.point, position);
        if (candidateDistanceSquared <= clickRadiusSquared
            && candidateDistanceSquared < bestDistanceSquared) {
            bestCandidate = &candidate;
            bestDistanceSquared = candidateDistanceSquared;
        }
    }

    if (!bestCandidate) {
        return std::nullopt;
    }
    return bestCandidate->logicalRow;
}

bool LatencyWidget::activatePlotDotAtPosition(const QPoint& position)
{
    const std::optional<int> logicalRow = plotDotLogicalRowAtPosition(position);
    if (!logicalRow) {
        return false;
    }

    Q_EMIT logicalRowActivated(*logicalRow);
    return true;
}

std::optional<long double> LatencyWidget::plotValueAtX(const int x) const
{
    if (!plotHasDataRange_) {
        return std::nullopt;
    }

    const QRect track = plotTrackViewportRect();
    if (track.width() <= 1) {
        return std::nullopt;
    }

    const int clampedX = qBound(track.left(), x, track.right());
    const auto [visibleMin, visibleMax] = visiblePlotRange();
    const long double ratio =
        static_cast<long double>(clampedX - track.left())
        / static_cast<long double>(qMax(1, track.width() - 1));
    return visibleMin + (visibleMax - visibleMin) * ratio;
}

LatencyWidget::PlotDragMode LatencyWidget::classifyPlotLeftDrag(
    const QPoint& position,
    const Qt::KeyboardModifiers modifiers) const
{
    const QPoint delta = position - plotDragStart_;
    if (delta.manhattanLength() < kPlotGestureActivationDistance) {
        return PlotDragMode::Pending;
    }

    if (modifiers & Qt::ShiftModifier) {
        return PlotDragMode::Pan;
    }

    if (modifiers & Qt::ControlModifier) {
        return PlotDragMode::RangeZoom;
    }

    const int absDx = std::abs(delta.x());
    const int absDy = std::abs(delta.y());
    if (plotGestureDirectionReady(delta)) {
        switch (snappedPlotGestureOctant(delta)) {
        case -1:
        case -3:
            return PlotDragMode::ZoomIn2x;
        case 1:
        case 3:
            return PlotDragMode::ZoomOut2x;
        case 2:
        case -2:
            return PlotDragMode::ZoomFull;
        default:
            return PlotDragMode::RangeZoom;
        }
    }

    if (absDx >= kPlotGestureActivationDistance && absDx >= absDy * 2) {
        return PlotDragMode::RangeZoom;
    }
    return PlotDragMode::Pending;
}

void LatencyWidget::beginPlotLeftDrag(const QPoint& position, const Qt::KeyboardModifiers modifiers)
{
    plotDragMode_ = modifiers & Qt::ShiftModifier ? PlotDragMode::Pan : PlotDragMode::Pending;
    plotDragStart_ = position;
    plotDragLast_ = position;
    plotDragStartCenter_ = plotCenter_;
    if (tree_ && tree_->viewport()) {
        tree_->viewport()->grabMouse();
    }
    if (plotDragMode_ == PlotDragMode::Pan && tree_ && tree_->viewport()) {
        tree_->viewport()->setCursor(Qt::ClosedHandCursor);
    }
    updatePlotViewProperties();
}

void LatencyWidget::updatePlotLeftDrag(const QPoint& position, const Qt::KeyboardModifiers modifiers)
{
    plotDragLast_ = position;

    if (plotDragMode_ != PlotDragMode::Pan) {
        const PlotDragMode nextMode = classifyPlotLeftDrag(position, modifiers);
        if (nextMode != PlotDragMode::Pending) {
            if (nextMode == PlotDragMode::Pan) {
                plotDragStart_ = position;
                plotDragStartCenter_ = plotCenter_;
                plotDragLast_ = position;
                if (tree_ && tree_->viewport()) {
                    tree_->viewport()->setCursor(Qt::ClosedHandCursor);
                }
            } else if (plotDragMode_ == PlotDragMode::Pan && tree_ && tree_->viewport()) {
                tree_->viewport()->unsetCursor();
            }
            plotDragMode_ = nextMode;
        }
    }

    if (plotDragMode_ == PlotDragMode::Pan) {
        const QRect track = plotTrackViewportRect();
        const auto [visibleMin, visibleMax] = visiblePlotRange();
        const long double unitsPerPixel =
            (visibleMax - visibleMin) / static_cast<long double>(qMax(1, track.width()));
        plotCenter_ = plotDragStartCenter_
            - static_cast<long double>(position.x() - plotDragStart_.x()) * unitsPerPixel;
        clampPlotCenter();
    }

    updatePlotViewProperties();
}

void LatencyWidget::finishPlotLeftDrag(const QPoint& position, const Qt::KeyboardModifiers modifiers)
{
    plotDragLast_ = position;

    if (plotDragMode_ == PlotDragMode::Pending) {
        plotDragMode_ = classifyPlotLeftDrag(position, modifiers);
    }

    const PlotDragMode mode = plotDragMode_;
    plotDragMode_ = PlotDragMode::None;
    releasePlotMouseGrab();

    if (mode == PlotDragMode::RangeZoom) {
        const QRect track = plotTrackViewportRect();
        const int left = qBound(track.left(), std::min(plotDragStart_.x(), plotDragLast_.x()), track.right());
        const int right = qBound(track.left(), std::max(plotDragStart_.x(), plotDragLast_.x()), track.right());
        if (right - left >= 8) {
            const std::optional<long double> first = plotValueAtX(left);
            const std::optional<long double> last = plotValueAtX(right);
            if (first && last) {
                zoomPlotToRange(*first, *last);
                return;
            }
        }
    } else if (mode == PlotDragMode::ZoomIn2x) {
        zoomPlotByFactor(2.0, plotDragStart_);
        return;
    } else if (mode == PlotDragMode::ZoomOut2x) {
        zoomPlotByFactor(0.5, plotDragStart_);
        return;
    } else if (mode == PlotDragMode::ZoomFull) {
        fitPlotView();
        return;
    }

    if (mode == PlotDragMode::Pending) {
        activatePlotDotAtPosition(position);
    }

    updatePlotViewProperties();
}

void LatencyWidget::cancelPlotLeftDrag()
{
    plotDragMode_ = PlotDragMode::None;
    releasePlotMouseGrab();
    updatePlotViewProperties();
}

void LatencyWidget::updateSelectionSummary()
{
    if (!summaryTitleLabel_ || !summaryBodyLabel_ || !tree_) {
        return;
    }

    QTreeWidgetItem* item = tree_->currentItem();
    if (!item) {
        summaryTitleLabel_->setText(QStringLiteral("Summary"));
        summaryBodyLabel_->setText(QStringLiteral("Select a latency row."));
        return;
    }

    const std::optional<LatencyStats> stats =
        statsFromVariant(item->data(kBoxPlotColumn, kPlotRole));
    summaryTitleLabel_->setText(item->text(kClassificationColumn));
    if (!stats || stats->count == 0) {
        summaryBodyLabel_->setText(QStringLiteral("No samples."));
        return;
    }

    summaryBodyLabel_->setText(statsSummaryLines(*stats).join(QLatin1Char('\n')));
}

void LatencyWidget::applyViewState(const QVariantMap& state)
{
    if (state.isEmpty()) {
        return;
    }

    if (plotTypeCombo_ && state.contains(QStringLiteral("plotType"))) {
        const int index = plotTypeCombo_->findText(state.value(QStringLiteral("plotType")).toString(),
                                                   Qt::MatchFixedString);
        if (index >= 0) {
            const QSignalBlocker blocker(plotTypeCombo_);
            plotTypeCombo_->setCurrentIndex(index);
            currentPlotType_ = plotTypeCombo_->itemData(index).toInt();
            if (tree_ && tree_->headerItem()) {
                tree_->headerItem()->setText(kBoxPlotColumn,
                                             plotTypeCombo_->currentText() + QStringLiteral(" Plot"));
            }
        }
    }

    if (tree_) {
        const int classificationWidth = state.value(QStringLiteral("classificationColumnWidth")).toInt();
        if (classificationWidth > 0) {
            tree_->setColumnWidth(kClassificationColumn, classificationWidth);
        }
        const int plotWidth = state.value(QStringLiteral("plotColumnWidth")).toInt();
        if (plotWidth > 0) {
            tree_->setColumnWidth(kBoxPlotColumn, plotWidth);
        }

        QSet<QString> expandedPaths;
        const QStringList expandedPathList = state.value(QStringLiteral("expandedPaths")).toStringList();
        for (const QString& path : expandedPathList) {
            expandedPaths.insert(path);
        }
        const auto restoreExpanded = [&](auto&& self, QTreeWidgetItem* item, QStringList path) -> void {
            if (!item) {
                return;
            }
            path.append(item->text(kClassificationColumn));
            item->setExpanded(expandedPaths.contains(path.join(QLatin1Char('/'))));
            for (int index = 0; index < item->childCount(); ++index) {
                self(self, item->child(index), path);
            }
        };
        for (int index = 0; index < tree_->topLevelItemCount(); ++index) {
            restoreExpanded(restoreExpanded, tree_->topLevelItem(index), {});
        }

        const QStringList selectedPath = state.value(QStringLiteral("selectedPath")).toStringList();
        QTreeWidgetItem* selectedItem = nullptr;
        if (!selectedPath.isEmpty()) {
            auto findByPath = [&](auto&& self,
                                  QTreeWidgetItem* parent,
                                  const QStringList& path,
                                  int depth) -> QTreeWidgetItem* {
                if (depth >= path.size()) {
                    return parent;
                }
                const int childCount = parent ? parent->childCount() : tree_->topLevelItemCount();
                for (int index = 0; index < childCount; ++index) {
                    QTreeWidgetItem* child = parent ? parent->child(index) : tree_->topLevelItem(index);
                    if (child && child->text(kClassificationColumn) == path[depth]) {
                        return self(self, child, path, depth + 1);
                    }
                }
                return nullptr;
            };
            selectedItem = findByPath(findByPath, nullptr, selectedPath, 0);
        }
        if (selectedItem) {
            tree_->setCurrentItem(selectedItem);
        } else {
            tree_->clearSelection();
            tree_->setCurrentItem(nullptr);
        }
    }

    plotZoom_ = std::clamp(state.value(QStringLiteral("plotZoom"), plotZoom_).toDouble(),
                           kMinLatencyPlotZoom,
                           kMaxLatencyPlotZoom);
    plotCenter_ = static_cast<long double>(
        state.value(QStringLiteral("plotCenter"), static_cast<double>(plotCenter_)).toDouble());
    plotHasCursor_ = state.value(QStringLiteral("plotHasCursor"), plotHasCursor_).toBool();
    plotCursorValue_ = static_cast<long double>(
        state.value(QStringLiteral("plotCursorValue"), static_cast<double>(plotCursorValue_)).toDouble());
    clampPlotCenter();
    updatePlotViewProperties();
    updateSelectionSummary();
}

QRect LatencyWidget::scaleTrackRect(const QRect& rect) const
{
    if (!tree_ || !plotHasDataRange_) {
        return QRect();
    }

    const QRect column = plotColumnViewportRect();
    QRect track(column.left() + kPlotHorizontalPadding,
                rect.top() + 2,
                qMax(1, column.width() - 2 * kPlotHorizontalPadding),
                qMax(1, rect.height() - 4));
    return track.intersected(rect.adjusted(0, 0, -1, -1));
}

void LatencyWidget::paintScale(QPainter& painter, const QRect& rect) const
{
    painter.fillRect(rect, QColor(QStringLiteral("#FFFFFF")));
    painter.setPen(QPen(QColor(QStringLiteral("#D7DCE2")), 1));
    painter.drawLine(rect.left(), rect.top(), rect.right(), rect.top());

    if (!tree_ || !plotHasDataRange_) {
        return;
    }

    const QRect track = scaleTrackRect(rect);
    if (track.width() <= 16) {
        return;
    }

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
    const auto xFor = [&](const long double value) {
        const long double ratio = (value - visibleMin) / visibleSpan;
        return track.left() + static_cast<int>(
            std::llround(ratio * static_cast<long double>(track.width() - 1)));
    };

    const double majorStep = niceLatencyStep(static_cast<double>(visibleSpan / 5.0L));
    const long double firstTick =
        std::ceil(visibleMin / static_cast<long double>(majorStep))
        * static_cast<long double>(majorStep);
    QFont scaleFont = painter.font();
    scaleFont.setPointSizeF(qMax<qreal>(6.5, scaleFont.pointSizeF() * 0.82));
    painter.setFont(scaleFont);
    const QFontMetrics metrics(scaleFont);

    int lastLabelRight = track.left() - 12;
    for (long double tick = firstTick;
         tick <= visibleMax + static_cast<long double>(majorStep) * 0.5L;
         tick += static_cast<long double>(majorStep)) {
        const int x = xFor(tick);
        if (x < track.left() || x > track.right()) {
            continue;
        }

        painter.setPen(QPen(QColor(QStringLiteral("#AEB6C1")), 1));
        painter.drawLine(x, track.top(), x, track.top() + 7);
        const QString label = latencyText(static_cast<qint64>(std::llround(tick)));
        const int textWidth = metrics.horizontalAdvance(label);
        const int left = qBound(track.left(),
                                x - textWidth / 2,
                                qMax(track.left(), track.right() - textWidth + 1));
        if (left > lastLabelRight + 8) {
            painter.setPen(QColor(QStringLiteral("#4B5563")));
            painter.drawText(QRect(left, track.top() + 9, textWidth, track.height() - 9),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             label);
            lastLabelRight = left + textWidth;
        }

        if (tick > static_cast<long double>((std::numeric_limits<qint64>::max)())
                - static_cast<long double>(majorStep)
            || majorStep <= 0.0) {
            break;
        }
    }

    if (plotHasCursor_
        && plotCursorValue_ >= visibleMin
        && plotCursorValue_ <= visibleMax) {
        const int cursorX = xFor(plotCursorValue_);
        if (cursorX >= track.left() && cursorX <= track.right()) {
            const QString cursorLabel = latencyText(static_cast<qint64>(std::llround(plotCursorValue_)));
            const int labelWidth = metrics.horizontalAdvance(cursorLabel) + 12;
            const int labelHeight = qMin(track.height() - 4, metrics.height() + 6);
            const int labelLeft = qBound(track.left(),
                                         cursorX - labelWidth / 2,
                                         qMax(track.left(), track.right() - labelWidth + 1));
            const QRect labelRect(labelLeft,
                                  qMax(track.top() + 10, track.bottom() - labelHeight),
                                  labelWidth,
                                  labelHeight);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(QStringLiteral("#111827")), 1));
            painter.drawLine(cursorX, track.top(), cursorX, labelRect.top());
            painter.setBrush(QColor(QStringLiteral("#111827")));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(labelRect, 3, 3);

            QPolygon pointer;
            pointer << QPoint(cursorX, labelRect.top() - 4)
                    << QPoint(cursorX - 4, labelRect.top() + 1)
                    << QPoint(cursorX + 4, labelRect.top() + 1);
            painter.drawPolygon(pointer);

            painter.setPen(QColor(QStringLiteral("#FFFFFF")));
            painter.drawText(labelRect.adjusted(6, 0, -6, 0),
                             Qt::AlignCenter,
                             cursorLabel);
        }
    }
}

void LatencyWidget::paintPlotGestureOverlay(QPainter& painter, const QRect& rect) const
{
    if (plotDragMode_ == PlotDragMode::None || plotDragMode_ == PlotDragMode::Pan) {
        return;
    }

    const QPoint delta = plotDragLast_ - plotDragStart_;
    if (delta.manhattanLength() < kPlotGestureActivationDistance) {
        return;
    }

    const QRect column = plotColumnViewportRect().intersected(rect);
    if (column.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (plotDragMode_ == PlotDragMode::RangeZoom) {
        const QRect track = plotTrackViewportRect().intersected(rect);
        if (!track.isEmpty()) {
            const int left = qBound(track.left(),
                                    std::min(plotDragStart_.x(), plotDragLast_.x()),
                                    track.right());
            const int right = qBound(track.left(),
                                     std::max(plotDragStart_.x(), plotDragLast_.x()),
                                     track.right());
            if (right - left >= 1) {
                const QRect selection(left, column.top(), qMax(1, right - left), column.height());
                painter.fillRect(selection, withAlpha(QColor(QStringLiteral("#2F80ED")), 34));
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(QColor(QStringLiteral("#2F80ED")), 1, Qt::DashLine));
                painter.drawRect(selection.adjusted(0, 0, -1, -1));
            }

            const QString label = QStringLiteral("Range zoom");
            const QFontMetrics metrics(painter.font());
            QRect labelRect(QPoint(qBound(track.left(),
                                          (left + right) / 2 - metrics.horizontalAdvance(label) / 2 - 8,
                                          qMax(track.left(), track.right() - metrics.horizontalAdvance(label) - 16)),
                                   column.top() + 8),
                            QSize(metrics.horizontalAdvance(label) + 16, metrics.height() + 8));
            labelRect = labelRect.intersected(rect.adjusted(4, 4, -4, -4));
            if (!labelRect.isEmpty()) {
                painter.setPen(QPen(QColor(QStringLiteral("#8BAFE8")), 1));
                painter.setBrush(withAlpha(QColor(QStringLiteral("#FFFFFF")), 235));
                painter.drawRoundedRect(labelRect, 4, 4);
                painter.setPen(QColor(QStringLiteral("#123B72")));
                painter.drawText(labelRect, Qt::AlignCenter, label);
            }
        }

        painter.restore();
        return;
    }

    const int octant = snappedPlotGestureOctant(delta);
    QString label;
    if (plotDragMode_ == PlotDragMode::ZoomFull || octant == 2 || octant == -2) {
        label = QStringLiteral("Zoom full");
    } else if (plotDragMode_ == PlotDragMode::ZoomIn2x || octant == -1 || octant == -3) {
        label = QStringLiteral("Zoom in 2x");
    } else if (plotDragMode_ == PlotDragMode::ZoomOut2x || octant == 1 || octant == 3) {
        label = QStringLiteral("Zoom out 2x");
    } else {
        label = QStringLiteral("Gesture");
    }

    const double length = qBound(28.0,
                                 std::hypot(static_cast<double>(delta.x()),
                                            static_cast<double>(delta.y())),
                                 120.0);
    const double angle = static_cast<double>(octant) * (kPi / 4.0);
    const QPointF start(plotDragStart_);
    const QPointF end(start.x() + std::cos(angle) * length,
                      start.y() + std::sin(angle) * length);
    const QPointF unit(std::cos(angle), std::sin(angle));
    const QPointF normal(-unit.y(), unit.x());
    const QPointF headLeft = end - unit * 10.0 + normal * 5.0;
    const QPointF headRight = end - unit * 10.0 - normal * 5.0;

    painter.setPen(QPen(QColor(QStringLiteral("#1F5FBF")), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(start, end);
    painter.drawLine(end, headLeft);
    painter.drawLine(end, headRight);

    const QFontMetrics metrics(painter.font());
    QRect labelRect(QPoint(qRound(end.x()) + 10, qRound(end.y()) - metrics.height() / 2 - 4),
                    QSize(metrics.horizontalAdvance(label) + 16, metrics.height() + 8));
    const QRect bounds = rect.adjusted(4, 4, -4, -4);
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

std::pair<long double, long double> LatencyWidget::effectivePlotDataRange() const
{
    if (!plotHasDataRange_) {
        return {0.0L, 1.0L};
    }

    long double minimum = static_cast<long double>(plotDataMin_);
    long double maximum = static_cast<long double>(plotDataMax_);
    if (maximum <= minimum) {
        minimum -= 0.5L;
        maximum += 0.5L;
    } else {
        const long double span = maximum - minimum;
        const long double margin = std::max<long double>(1.0L, span * 0.04L);
        minimum -= margin;
        maximum += margin;
    }
    return {minimum, maximum};
}

std::pair<long double, long double> LatencyWidget::visiblePlotRange() const
{
    const auto [minimum, maximum] = effectivePlotDataRange();
    const long double dataSpan = std::max<long double>(1.0L, maximum - minimum);
    const long double zoom =
        static_cast<long double>(std::clamp(plotZoom_, kMinLatencyPlotZoom, kMaxLatencyPlotZoom));
    const long double visibleSpan = dataSpan / zoom;
    if (visibleSpan >= dataSpan) {
        return {minimum, maximum};
    }

    const long double halfSpan = visibleSpan / 2.0L;
    const long double center = std::clamp(plotCenter_, minimum + halfSpan, maximum - halfSpan);
    return {center - halfSpan, center + halfSpan};
}

void LatencyWidget::stopBuildThread()
{
    ++buildGeneration_;
    if (buildThread_.joinable()) {
        buildThread_.request_stop();
        buildThread_.join();
    }
}

void LatencyWidget::refresh()
{
    buildRequested_ = true;
    stopBuildThread();
    building_ = false;
    buildProgressProcessedRows_ = 0;
    buildProgressTotalRows_ = traceSession_ ? traceSession_->totalRows() : pendingRows_.size();
    updateBuildPromptOverlay();
    tree_->clear();
    clearPlotDataRange();
    progressBar_->hide();

    if (!traceSession_) {
        buildRowsFromPendingRows();
        return;
    }

    if (!traceSession_->supportsXactionIndexing()) {
        setIdleText(QStringLiteral("Latency is available for CHI E.b traces with Xaction indexing."));
        return;
    }

    if (!traceSession_->isXactionIndexComplete()) {
        setIdleText(QStringLiteral("Build the Xaction index first, then refresh Latency."));
        return;
    }

    startSessionBuild(traceSession_);
}

void LatencyWidget::buildRowsFromPendingRows()
{
    if (pendingRows_.empty()) {
        setIdleText(QStringLiteral("No flits in the current trace."));
        return;
    }

    building_ = true;
    buildProgressProcessedRows_ = 0;
    buildProgressTotalRows_ = pendingRows_.size();
    updateBuildPromptOverlay();
    std::vector<FlitRecord> rows = pendingRows_;
    statusLabel_->setText(QStringLiteral("Aggregating latency classification 0 / %1 rows...")
                              .arg(static_cast<qulonglong>(buildProgressTotalRows_)));
    std::unordered_map<std::uint64_t, std::pair<std::vector<std::uint64_t>, std::vector<FlitRecord>>> groupedRows;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const FlitRecord& row = rows[index];
        const std::optional<std::uint64_t> ordinal = xactionOrdinalForRecord(row);
        if (!ordinal) {
            continue;
        }
        auto& group = groupedRows[*ordinal];
        group.first.push_back(static_cast<std::uint64_t>(index));
        group.second.push_back(row);
        if (index % kLatencyWorkerChunkRows == 0) {
            buildProgressProcessedRows_ = index;
            statusLabel_->setText(QStringLiteral("Aggregating latency classification %1 / %2 rows...")
                                      .arg(static_cast<qulonglong>(buildProgressProcessedRows_))
                                      .arg(static_cast<qulonglong>(buildProgressTotalRows_)));
            updateBuildPromptOverlay();
        }
    }
    buildProgressProcessedRows_ = buildProgressTotalRows_;
    statusLabel_->setText(QStringLiteral("Aggregating latency classification %1 / %2 rows...")
                              .arg(static_cast<qulonglong>(buildProgressProcessedRows_))
                              .arg(static_cast<qulonglong>(buildProgressTotalRows_)));

    std::vector<std::pair<std::uint64_t, std::pair<std::vector<std::uint64_t>, std::vector<FlitRecord>>>> transactions;
    transactions.reserve(groupedRows.size());
    for (auto& [ordinal, payload] : groupedRows) {
        transactions.emplace_back(ordinal, std::move(payload));
    }
    std::sort(transactions.begin(), transactions.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    auto result = std::make_shared<BuildResult>();
    result->generation = buildGeneration_.load(std::memory_order_relaxed);
    result->totalXactions = static_cast<std::uint64_t>(transactions.size());
    result->processedXactions = result->totalXactions;
    result->typeBuckets = buildBucketsFromTransactions(nullptr, transactions, result->sampleCount);
    applyBuildResult(result);
    updateBuildPromptOverlay();
}

QString LatencyWidget::pendingBuildText() const
{
    if (traceSession_) {
        const std::uint64_t rowCount = traceSession_->totalRows();
        return QStringLiteral("Latency view is ready to build. Click here to scan %1 indexed rows.")
            .arg(FormatUnsignedIntegerWithThousands(rowCount));
    }
    if (!pendingRows_.empty()) {
        return QStringLiteral("Latency view is ready to build. Click here to aggregate %1 rows.")
            .arg(FormatUnsignedIntegerWithThousands(pendingRows_.size()));
    }
    return QStringLiteral("Open a CHI E.b trace and build the Xaction index to populate latency box plots.");
}

bool LatencyWidget::hasPendingBuild() const noexcept
{
    return !buildRequested_
        && ((traceSession_ && traceSession_->isXactionIndexComplete())
            || (!traceSession_ && !pendingRows_.empty()));
}

bool LatencyWidget::shouldShowBuildPrompt() const noexcept
{
    return hasPendingBuild()
        || (building_
            && buildRequested_
            && tree_
            && tree_->topLevelItemCount() == 0
            && ((traceSession_ && traceSession_->isXactionIndexComplete())
                || (!traceSession_ && !pendingRows_.empty())));
}

QRect LatencyWidget::buildPromptButtonRect(const QRect& rect) const
{
    const QRect body = rect.adjusted(24, 24, -24, -24);
    const int buttonWidth = qMin(260, qMax(150, body.width() - 48));
    const int buttonHeight = 34;
    return QRect(body.center().x() - buttonWidth / 2,
                 body.center().y() + 20,
                 buttonWidth,
                 buttonHeight);
}

void LatencyWidget::updateBuildPromptOverlay()
{
    if (!buildPromptOverlay_) {
        return;
    }

    buildPromptOverlay_->setGeometry(rect());
    const bool visible = shouldShowBuildPrompt();
    buildPromptOverlay_->setCursor(hasPendingBuild() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    buildPromptOverlay_->setVisible(visible);
    if (visible) {
        buildPromptOverlay_->raise();
        buildPromptOverlay_->update();
    }
}

void LatencyWidget::paintBuildPrompt(QPainter& painter, const QRect& rect) const
{
    if (!shouldShowBuildPrompt()) {
        return;
    }

    painter.save();
    painter.fillRect(rect, QColor(255, 255, 255, 214));

    const QRect content = rect.adjusted(28, 36, -28, -34);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPixelSize(qMax(13, titleFont.pixelSize() > 0 ? titleFont.pixelSize() + 2 : 15));
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#18212B")));
    painter.drawText(content.adjusted(0, 0, 0, -52),
                     Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                     building_
                         ? QStringLiteral("Scanning latency classification...")
                         : pendingBuildText());

    const QRect button = buildPromptButtonRect(rect);
    if (building_) {
        paintBuildProgress(painter, button);
    } else {
        painter.setPen(QColor(QStringLiteral("#1D5F8F")));
        painter.setBrush(QColor(QStringLiteral("#EAF4FB")));
        painter.drawRect(button.adjusted(0, 0, -1, -1));
        painter.setPen(QColor(QStringLiteral("#174B72")));
        painter.setFont(font());
        painter.drawText(button, Qt::AlignCenter, QStringLiteral("Build Latency View"));
    }
    painter.restore();
}

void LatencyWidget::paintBuildProgress(QPainter& painter, const QRect& bounds) const
{
    painter.save();
    const QRect track = bounds.adjusted(0, 4, 0, -4);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(QStringLiteral("#B7C8D8")));
    painter.setBrush(QColor(QStringLiteral("#EEF5FA")));
    painter.drawRect(track.adjusted(0, 0, -1, -1));

    const int fillWidth = buildProgressTotalRows_ == 0
        ? 0
        : static_cast<int>(std::llround(
              static_cast<double>(track.width())
              * static_cast<double>(std::min(buildProgressProcessedRows_, buildProgressTotalRows_))
              / static_cast<double>(buildProgressTotalRows_)));
    if (fillWidth > 0) {
        const QRect fill(track.left(), track.top(), qMin(track.width(), fillWidth), track.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#2F80ED")));
        painter.drawRect(fill.adjusted(0, 0, -1, -1));
    }

    const QString text = buildProgressTotalRows_ > 0
        ? QStringLiteral("%1 / %2")
              .arg(FormatUnsignedIntegerWithThousands(buildProgressProcessedRows_))
              .arg(FormatUnsignedIntegerWithThousands(buildProgressTotalRows_))
        : QStringLiteral("Building...");
    painter.setPen(QColor(QStringLiteral("#174B72")));
    painter.setFont(font());
    painter.drawText(track, Qt::AlignCenter, text);
    painter.restore();
}

void LatencyWidget::startSessionBuild(std::shared_ptr<TraceSession> session)
{
    const quint64 generation = ++buildGeneration_;
    building_ = true;
    buildProgressProcessedRows_ = 0;
    buildProgressTotalRows_ = session ? session->totalRows() : 0;
    updateBuildPromptOverlay();
    statusLabel_->setText(QStringLiteral("Scanning indexed rows for latency classification..."));
    progressBar_->setRange(0, 0);
    progressBar_->show();
    refreshButton_->setEnabled(false);
    postBuildProgress(generation, 0, session ? session->totalRows() : 0);

    buildThread_ = std::jthread([this, session = std::move(session), generation](const std::stop_token stopToken) {
        auto result = std::make_shared<BuildResult>();
        result->generation = generation;
        if (!session) {
            result->statusText = QStringLiteral("Open a trace to populate latency.");
            result->failed = true;
        } else {
            result->statusText = QStringLiteral("Latency ready.");
            QString errorText;
            if (tryLoadLatencyCache(*session, *result)) {
                result->statusText = QStringLiteral("Latency ready from cache.");
            } else if (!buildBucketsFromIndexedRowStream(session,
                                                         stopToken,
                                                         generation,
                                                         *result,
                                                         errorText)) {
                result->failed = true;
                result->statusText = QStringLiteral("Latency unavailable: %1").arg(errorText);
            } else if (!result->cancelled && !result->failed) {
                writeLatencyCache(*session, *result);
            }
        }

        if (stopToken.stop_requested()) {
            return;
        }
        QMetaObject::invokeMethod(this,
                                  [this, result, generation]() {
                                      if (generation == buildGeneration_) {
                                          applyBuildResult(result);
                                      }
                                  },
                                  Qt::QueuedConnection);
    });
}

bool LatencyWidget::buildBucketsFromIndexedRowStream(const std::shared_ptr<TraceSession>& session,
                                                     const std::stop_token stopToken,
                                                     const quint64 generation,
                                                     BuildResult& result,
                                                     QString& errorText)
{
    if (!session) {
        errorText = QStringLiteral("Open a trace to populate latency.");
        return false;
    }

    QElapsedTimer phaseTimer;
    std::unordered_map<std::uint64_t, QString> xactionTypes;
    phaseTimer.start();
    if (!session->xactionTypesByOrdinal(xactionTypes)) {
        errorText = QStringLiteral("failed to read Xaction type metadata");
        return false;
    }
    result.typeLoadMs = phaseTimer.elapsed();

    std::unordered_map<std::uint64_t, std::uint64_t> rowCounts;
    phaseTimer.restart();
    if (!session->xactionRowCountsByOrdinal(rowCounts)) {
        errorText = QStringLiteral("failed to read Xaction row-count metadata");
        return false;
    }
    result.rowCountLoadMs = phaseTimer.elapsed();

    std::unordered_set<std::uint64_t> completedOrdinals;
    if (!session->xactionCompletedOrdinals(completedOrdinals)) {
        errorText = QStringLiteral("failed to read completed Xaction metadata");
        return false;
    }

    result.totalXactions = static_cast<std::uint64_t>(completedOrdinals.size());

    const std::uint64_t totalRows = session->totalRows();
    if (totalRows == 0) {
        result.processedXactions = result.totalXactions;
        return true;
    }

    postBuildProgress(generation, 0, totalRows);

    std::vector<LatencyBlockSlice> slices;
    slices.reserve(session->metadata().blocks.size());
    for (std::size_t blockIndex = 0; blockIndex < session->metadata().blocks.size(); ++blockIndex) {
        const CLogBTraceBlockSummary& block = session->metadata().blocks[blockIndex];
        const std::uint64_t blockEndRow = block.rowStart + block.recordCount;
        slices.push_back(LatencyBlockSlice{
            .blockIndex = blockIndex,
            .beginRow = block.rowStart,
            .rowCount = blockEndRow - block.rowStart,
            .localBegin = 0,
        });
    }
    if (slices.empty()) {
        result.processedXactions = result.totalXactions;
        return true;
    }

    phaseTimer.restart();
    std::atomic<std::uint64_t> nextChunkIndex = 0;
    std::atomic<std::uint64_t> scannedRows = 0;
    std::atomic<std::uint64_t> lastProgressRows = 0;
    const std::uint64_t progressRowInterval =
        std::max<std::uint64_t>(kLatencyWorkerChunkRows, totalRows / 1000U);
    std::atomic_bool failed = false;
    std::atomic_bool cancelled = false;
    QString firstErrorText;
    std::mutex firstErrorMutex;
    const std::size_t workerCount = Detail::suggestedFilterWorkerCount(
        static_cast<std::size_t>(std::min<std::uint64_t>(
            totalRows,
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))));
    const std::size_t effectiveWorkerCount = std::min<std::size_t>(
#ifdef CHIRON_GUI_ENABLE_LATENCY_TEST_API
        std::max<std::size_t>(workerCount, 2),
#else
        workerCount,
#endif
        std::max<std::size_t>(1, slices.size()));

    std::vector<LatencyChunkResult> chunkResults(slices.size());
    for (std::size_t index = 0; index < slices.size(); ++index) {
        chunkResults[index].beginRow = slices[index].beginRow;
        chunkResults[index].rowCount = slices[index].rowCount;
    }

    const auto workerBody = [&](std::stop_token) {
        while (!failed.load(std::memory_order_acquire)) {
            if (stopToken.stop_requested()
                || generation != buildGeneration_.load(std::memory_order_relaxed)) {
                cancelled.store(true, std::memory_order_release);
                return;
            }

            const std::uint64_t chunkIndex64 = nextChunkIndex.fetch_add(1, std::memory_order_relaxed);
            if (chunkIndex64 >= slices.size()) {
                return;
            }
            const std::size_t chunkIndex = static_cast<std::size_t>(chunkIndex64);
            const LatencyBlockSlice& slice = slices[chunkIndex];
            LatencyChunkResult& chunk = chunkResults[chunkIndex];

            CLogBTraceLoadError readError;
            if (!appendLatencyEventsFromFastRecords(session,
                                                    slice,
                                                    completedOrdinals,
                                                    chunk.events,
                                                    readError,
                                                    stopToken)) {
                std::vector<FlitRecord> rows;
                if (!CLogBTraceLoader::loadRows(session->filePath(),
                                                session->metadata(),
                                                chunk.beginRow,
                                                chunk.rowCount,
                                                rows,
                                                readError,
                                                {},
                                                stopToken)) {
                    chunk.failed = true;
                    chunk.error = readError;
                    failed.store(true, std::memory_order_release);
                    const std::lock_guard lock(firstErrorMutex);
                    if (firstErrorText.isEmpty()) {
                        firstErrorText = readError.summary.isEmpty()
                            ? QStringLiteral("failed to read trace rows")
                            : readError.summary;
                    }
                    return;
                }

                std::vector<std::uint64_t> ordinals;
                if (!session->xactionOrdinalsByRowRange(chunk.beginRow, chunk.rowCount, ordinals)
                    || ordinals.size() < rows.size()) {
                    chunk.failed = true;
                    failed.store(true, std::memory_order_release);
                    const std::lock_guard lock(firstErrorMutex);
                    if (firstErrorText.isEmpty()) {
                        firstErrorText = QStringLiteral("failed to read Xaction row ordinals");
                    }
                    return;
                }

                chunk.events.reserve(rows.size());
                for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
                    const std::uint64_t ordinal = ordinals[rowIndex];
                    if (ordinal == 0 || completedOrdinals.find(ordinal) == completedOrdinals.end()) {
                        continue;
                    }

                    const FlitRecord& row = rows[rowIndex];
                    chunk.events.push_back(LatencyRowEvent{
                        .ordinal = ordinal,
                        .logicalRow = chunk.beginRow + rowIndex,
                        .timestamp = row.timestamp,
                        .channel = row.channel,
                        .opcode = row.opcode,
                        .opcodeRaw = row.opcodeRaw,
                    });
                }
            }

            const std::uint64_t completedRows =
                scannedRows.fetch_add(chunk.rowCount, std::memory_order_relaxed) + chunk.rowCount;
            const std::uint64_t previousProgressRows =
                lastProgressRows.load(std::memory_order_relaxed);
            if (completedRows == totalRows
                || completedRows - previousProgressRows >= progressRowInterval) {
                lastProgressRows.store(completedRows, std::memory_order_relaxed);
                postBuildProgress(generation, std::min(completedRows, totalRows), totalRows);
            }
        }
    };

    std::vector<std::jthread> workers;
    workers.reserve(effectiveWorkerCount);
    for (std::size_t workerIndex = 0; workerIndex < effectiveWorkerCount; ++workerIndex) {
        workers.emplace_back(workerBody);
    }
    workers.clear();

    if (stopToken.stop_requested()
        || generation != buildGeneration_.load(std::memory_order_relaxed)
        || cancelled.load(std::memory_order_acquire)) {
        result.cancelled = true;
        return true;
    }

    if (failed.load(std::memory_order_acquire)) {
        errorText = firstErrorText.isEmpty()
            ? QStringLiteral("failed to scan latency classification")
            : firstErrorText;
        return false;
    }

    postBuildProgress(generation, totalRows, totalRows);
    result.eventScanMs = phaseTimer.elapsed();

    phaseTimer.restart();
    const std::size_t aggregationWorkerCount = std::min<std::size_t>(
#ifdef CHIRON_GUI_ENABLE_LATENCY_TEST_API
        std::max<std::size_t>(workerCount, 2),
#else
        workerCount,
#endif
        std::max<std::size_t>(1, rowCounts.size()));
    std::vector<LatencyAggregationPartition> partitions(aggregationWorkerCount);

    for (LatencyChunkResult& chunk : chunkResults) {
        for (LatencyRowEvent& event : chunk.events) {
            const std::size_t partitionIndex =
                static_cast<std::size_t>(event.ordinal % aggregationWorkerCount);
            partitions[partitionIndex].events.push_back(std::move(event));
        }
        chunk.events.clear();
        chunk.events.shrink_to_fit();
    }

    std::atomic_bool aggregationCancelled = false;
    const auto aggregatePartition = [&](std::stop_token, LatencyAggregationPartition& partition) {
        std::sort(partition.events.begin(), partition.events.end(), [](const auto& left, const auto& right) {
            if (left.ordinal != right.ordinal) {
                return left.ordinal < right.ordinal;
            }
            return left.logicalRow < right.logicalRow;
        });

        LatencyStringInterner typeInterner;
        LatencyStringInterner requestInterner;
        LatencyStringInterner subsequentInterner;
        std::unordered_map<LatencyParentCompactKey,
                           LatencyParentCompactBucket,
                           LatencyParentCompactKeyHash> parentBuckets;
        parentBuckets.reserve(std::min<std::size_t>(partition.events.size(), 4096));
        std::unordered_map<LatencyCompactKey, LatencyCompactBucket, LatencyCompactKeyHash> compactBuckets;
        compactBuckets.reserve(std::min<std::size_t>(partition.events.size(), 4096));
        std::unordered_map<std::uint64_t, StreamingTransactionState> states;
        states.reserve(std::min<std::size_t>(partition.events.size(), 65536));

        const auto finishIfComplete = [&](const std::uint64_t ordinal,
                                          StreamingTransactionState& state) -> bool {
            if (state.expectedRows == 0 || state.observedRows < state.expectedRows) {
                return false;
            }
            if (state.hasStart && !state.emittedCompletionSample) {
                addCompletionLatencySample(parentBuckets,
                                           typeInterner,
                                           requestInterner,
                                           state.xactionType,
                                           state.requestOpcode,
                                           state.endTimestamp - state.startTimestamp,
                                           state.firstLogicalRow);
                state.emittedCompletionSample = true;
            }
            if (state.hasStart && !state.emittedSample) {
                addCompactLatencySample(compactBuckets,
                                        typeInterner,
                                        requestInterner,
                                        subsequentInterner,
                                        state.xactionType,
                                        state.requestOpcode,
                                        QStringLiteral("No subsequent flits"),
                                        state.endTimestamp - state.startTimestamp,
                                        state.firstLogicalRow);
                state.emittedSample = true;
            }
            states.erase(ordinal);
            return true;
        };

        for (const LatencyRowEvent& event : partition.events) {
            if (stopToken.stop_requested()
                || generation != buildGeneration_.load(std::memory_order_relaxed)) {
                aggregationCancelled.store(true, std::memory_order_release);
                return;
            }

            StreamingTransactionState& state = states[event.ordinal];
            if (state.expectedRows == 0) {
                const auto countFound = rowCounts.find(event.ordinal);
                state.expectedRows = countFound == rowCounts.end() ? 0 : countFound->second;
            }

            if (state.xactionType == QLatin1String("Unknown Xaction")) {
                const auto typeFound = xactionTypes.find(event.ordinal);
                if (typeFound != xactionTypes.end()
                    && !typeFound->second.trimmed().isEmpty()
                    && typeFound->second != QLatin1String("none")) {
                    state.xactionType = typeFound->second;
                }
            }

            bool anchoredThisRow = false;
            ++state.observedRows;

            if (!state.hasStart) {
                state.hasStart = true;
                state.startTimestamp = event.timestamp;
                state.endTimestamp = event.timestamp;
                state.firstLogicalRow = static_cast<int>(event.logicalRow);
                state.requestOpcode = QStringLiteral("%1 %2")
                                          .arg(ToString(event.channel), displayOpcode(event));
                anchoredThisRow = true;
            } else {
                state.endTimestamp = std::max(state.endTimestamp, event.timestamp);
            }

            if ((event.channel == FlitChannel::Req || event.channel == FlitChannel::Snp)
                && !state.sawReqOrSnp) {
                state.sawReqOrSnp = true;
                state.startTimestamp = event.timestamp;
                state.firstLogicalRow = static_cast<int>(event.logicalRow);
                state.requestOpcode = QStringLiteral("%1 %2")
                                          .arg(ToString(event.channel), displayOpcode(event));
                finishIfComplete(event.ordinal, state);
                continue;
            }

            if (anchoredThisRow) {
                finishIfComplete(event.ordinal, state);
                continue;
            }

            if (event.channel == FlitChannel::Req || event.channel == FlitChannel::Snp) {
                finishIfComplete(event.ordinal, state);
                continue;
            }

            addCompactLatencySample(compactBuckets,
                                    typeInterner,
                                    requestInterner,
                                    subsequentInterner,
                                    state.xactionType,
                                    state.requestOpcode,
                                    subsequentLabel(event, state.seenDataOpcodes),
                                    event.timestamp - state.startTimestamp,
                                    static_cast<int>(event.logicalRow));
            state.emittedSample = true;

            finishIfComplete(event.ordinal, state);
        }
        materializeCompactLatencyBuckets(partition.typeBuckets,
                                         partition.sampleCount,
                                         typeInterner,
                                         requestInterner,
                                         subsequentInterner,
                                         std::move(parentBuckets),
                                         std::move(compactBuckets));
        partition.events.clear();
        partition.events.shrink_to_fit();
    };

    std::vector<std::jthread> aggregationWorkers;
    aggregationWorkers.reserve(partitions.size());
    for (LatencyAggregationPartition& partition : partitions) {
        aggregationWorkers.emplace_back(aggregatePartition, std::ref(partition));
    }
    aggregationWorkers.clear();

    if (stopToken.stop_requested()
        || generation != buildGeneration_.load(std::memory_order_relaxed)
        || aggregationCancelled.load(std::memory_order_acquire)) {
        result.cancelled = true;
        return true;
    }

    for (LatencyAggregationPartition& partition : partitions) {
        result.sampleCount += partition.sampleCount;
        mergeLatencyBuckets(result.typeBuckets, std::move(partition.typeBuckets));
    }
    result.processedXactions = result.totalXactions;
    result.aggregationMs = phaseTimer.elapsed();
    return true;
}

void LatencyWidget::applyBuildResult(std::shared_ptr<BuildResult> result)
{
    progressUpdateQueued_.store(false, std::memory_order_relaxed);
    if (!result || result->generation != buildGeneration_.load(std::memory_order_relaxed)) {
        return;
    }
    building_ = false;
    refreshButton_->setEnabled(buildRequested_
                               && ((traceSession_ && traceSession_->isXactionIndexComplete())
                                   || (!traceSession_ && !pendingRows_.empty())));
    updateBuildPromptOverlay();
    progressBar_->hide();
    tree_->clear();
    clearPlotDataRange();

    if (result->cancelled) {
        setIdleText(QStringLiteral("Latency build cancelled."));
        return;
    }

    if (result->failed) {
        setIdleText(result->statusText);
        return;
    }
    lastBuildLoadedFromCache_ = result->loadedFromCache;

    if (result->sampleCount == 0) {
        setIdleText(QStringLiteral("No latency samples were found in indexed transactions."));
        return;
    }

    QElapsedTimer uiTimer;
    uiTimer.start();
    statusLabel_->setText(QStringLiteral("Latency box plots ready. Select a row to inspect its summary."));

    const auto includeStats = [this](const LatencyStats& stats) {
        if (stats.count > 0) {
            includePlotSampleRange(stats.min, stats.max);
        }
    };

    for (const auto& [_, typeBucket] : result->typeBuckets) {
        const LatencyStats typeStats = collectStats(typeBucket);
        includeStats(typeStats);
        QTreeWidgetItem* typeItem =
            makeItem(typeBucket.type, typeStats, firstExampleRow(typeBucket), 0);
        tree_->addTopLevelItem(typeItem);

        for (const auto& [__, requestBucket] : typeBucket.requestOpcodeBuckets) {
            const LatencyStats requestStats = collectStats(requestBucket);
            includeStats(requestStats);
            QTreeWidgetItem* requestItem =
                makeItem(requestBucket.opcode, requestStats, firstExampleRow(requestBucket), 1);
            typeItem->addChild(requestItem);

            for (const auto& [___, subsequentBucket] : requestBucket.subsequentBuckets) {
                const LatencyStats subsequentStats =
                    calculateStats(subsequentBucket.samples, subsequentBucket.sampleLogicalRows);
                includeStats(subsequentStats);
                QTreeWidgetItem* subsequentItem =
                    makeItem(subsequentBucket.name,
                             subsequentStats,
                             subsequentBucket.exampleLogicalRow,
                             2);
                requestItem->addChild(subsequentItem);
            }
        }
    }

    tree_->sortItems(1, Qt::DescendingOrder);
    for (int index = 0; index < tree_->topLevelItemCount(); ++index) {
        tree_->topLevelItem(index)->setExpanded(true);
    }
    fitPlotView();
    updateSelectionSummary();
    if (!pendingViewState_.isEmpty()) {
        const QVariantMap state = pendingViewState_;
        pendingViewState_.clear();
        applyViewState(state);
    }
    result->uiBuildMs = uiTimer.elapsed();
    qInfo().noquote()
        << QStringLiteral("Latency build timings: type=%1 ms, row-counts=%2 ms, event-scan=%3 ms, aggregation=%4 ms, ui=%5 ms, samples=%6, xactions=%7, cache=%8")
               .arg(result->typeLoadMs)
               .arg(result->rowCountLoadMs)
               .arg(result->eventScanMs)
               .arg(result->aggregationMs)
               .arg(result->uiBuildMs)
               .arg(static_cast<qulonglong>(result->sampleCount))
               .arg(static_cast<qulonglong>(result->totalXactions))
               .arg(result->loadedFromCache ? QStringLiteral("hit") : QStringLiteral("miss"));
}

void LatencyWidget::postBuildProgress(const quint64 generation,
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

    QMetaObject::invokeMethod(this,
                              [this]() {
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
                              },
                              Qt::QueuedConnection);
}

void LatencyWidget::updateBuildProgress(const quint64 generation,
                                        const std::uint64_t processedRows,
                                        const std::uint64_t totalRows)
{
    if (generation != buildGeneration_) {
        return;
    }

    buildProgressProcessedRows_ = processedRows;
    buildProgressTotalRows_ = totalRows;
    statusLabel_->setText(QStringLiteral("Scanning latency classification %1 / %2 rows...")
                              .arg(static_cast<qulonglong>(processedRows))
                              .arg(static_cast<qulonglong>(totalRows)));
    if (totalRows == 0) {
        progressBar_->setRange(0, 0);
        return;
    }

    progressBar_->setRange(0, 1000);
    progressBar_->setValue(static_cast<int>(
        std::min<std::uint64_t>(1000, (processedRows * 1000U) / totalRows)));
    updateBuildPromptOverlay();
}

void LatencyWidget::setIdleText(const QString& text)
{
    statusLabel_->setText(text);
    refreshButton_->setEnabled(buildRequested_
                               && ((traceSession_ && traceSession_->isXactionIndexComplete())
                                   || (!traceSession_ && !pendingRows_.empty())));
    updateBuildPromptOverlay();
}

#ifdef CHIRON_GUI_ENABLE_LATENCY_TEST_API
namespace {

QTreeWidgetItem* itemAtPath(QTreeWidget* tree, const std::vector<int>& path)
{
    if (!tree || path.empty()) {
        return nullptr;
    }

    QTreeWidgetItem* item = tree->topLevelItem(path.front());
    for (std::size_t index = 1; item && index < path.size(); ++index) {
        item = item->child(path[index]);
    }
    return item;
}

}  // namespace

int LatencyWidget::testColumnCount() const
{
    return tree_ ? tree_->columnCount() : 0;
}

bool LatencyWidget::testColumnsAreUserResizable() const
{
    if (!tree_ || !tree_->header()) {
        return false;
    }
    return tree_->header()->sectionResizeMode(kClassificationColumn) == QHeaderView::Interactive
        && tree_->header()->sectionResizeMode(kBoxPlotColumn) == QHeaderView::Stretch;
}

int LatencyWidget::testTopLevelCount() const
{
    return tree_ ? tree_->topLevelItemCount() : 0;
}

QString LatencyWidget::testItemText(const std::vector<int>& path, const int column) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    return item ? item->text(column) : QString();
}

QString LatencyWidget::testItemToolTip(const std::vector<int>& path, const int column) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    return item ? item->toolTip(column) : QString();
}

int LatencyWidget::testChildCount(const std::vector<int>& path) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    return item ? item->childCount() : 0;
}

QStringList LatencyWidget::testItemSummaryLines(const std::vector<int>& path) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    if (!item) {
        return {};
    }
    const std::optional<LatencyStats> stats =
        statsFromVariant(item->data(kBoxPlotColumn, kPlotRole));
    return stats ? statsSummaryLines(*stats) : QStringList{};
}

QVariantMap LatencyWidget::testItemStats(const std::vector<int>& path) const
{
    QVariantMap result;
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    if (!item) {
        return result;
    }
    const std::optional<LatencyStats> stats =
        statsFromVariant(item->data(kBoxPlotColumn, kPlotRole));
    if (!stats) {
        return result;
    }

    QVariantList outliers;
    outliers.reserve(static_cast<int>(stats->outliers.size()));
    for (const qint64 outlier : stats->outliers) {
        outliers.append(QVariant::fromValue<qlonglong>(outlier));
    }

    result.insert(QStringLiteral("count"),
                  QVariant::fromValue<qulonglong>(static_cast<qulonglong>(stats->count)));
    result.insert(QStringLiteral("min"), QVariant::fromValue<qlonglong>(stats->min));
    result.insert(QStringLiteral("lowerWhisker"), QVariant::fromValue<qlonglong>(stats->lowerWhisker));
    result.insert(QStringLiteral("q1"), QVariant::fromValue<qlonglong>(stats->q1));
    result.insert(QStringLiteral("median"), QVariant::fromValue<qlonglong>(stats->median));
    result.insert(QStringLiteral("q3"), QVariant::fromValue<qlonglong>(stats->q3));
    result.insert(QStringLiteral("upperWhisker"), QVariant::fromValue<qlonglong>(stats->upperWhisker));
    result.insert(QStringLiteral("max"), QVariant::fromValue<qlonglong>(stats->max));
    result.insert(QStringLiteral("mean"), QVariant::fromValue<qlonglong>(stats->mean));
    result.insert(QStringLiteral("outlierCount"),
                  QVariant::fromValue<qulonglong>(static_cast<qulonglong>(stats->outlierCount)));
    result.insert(QStringLiteral("outliers"), outliers);
    return result;
}

void LatencyWidget::testSelectItem(const std::vector<int>& path)
{
    if (!tree_) {
        return;
    }
    tree_->setCurrentItem(itemAtPath(tree_, path));
    updateSelectionSummary();
}

QString LatencyWidget::testSummaryTitle() const
{
    return summaryTitleLabel_ ? summaryTitleLabel_->text() : QString();
}

QString LatencyWidget::testSummaryBody() const
{
    return summaryBodyLabel_ ? summaryBodyLabel_->text() : QString();
}

double LatencyWidget::testPlotZoom() const noexcept
{
    return plotZoom_;
}

std::pair<double, double> LatencyWidget::testPlotVisibleRange() const
{
    const auto [minimum, maximum] = visiblePlotRange();
    return {static_cast<double>(minimum), static_cast<double>(maximum)};
}

QRect LatencyWidget::testPlotColumnViewportRect() const
{
    return plotColumnViewportRect();
}

QRect LatencyWidget::testPlotColumnWidgetRect() const
{
    if (!tree_ || !tree_->viewport()) {
        return QRect();
    }
    const QRect viewportRect = plotColumnViewportRect();
    return QRect(tree_->viewport()->mapTo(this, viewportRect.topLeft()), viewportRect.size());
}

QRect LatencyWidget::testScaleWidgetRect() const
{
    return scaleWidget_ ? QRect(scaleWidget_->mapTo(this, QPoint(0, 0)), scaleWidget_->size()) : QRect();
}

QRect LatencyWidget::testScaleCursorTagWidgetRect() const
{
    if (!scaleWidget_ || !plotHasDataRange_ || !plotHasCursor_) {
        return QRect();
    }

    const QRect track = scaleTrackRect(scaleWidget_->rect());
    if (track.width() <= 16) {
        return QRect();
    }

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    if (plotCursorValue_ < visibleMin || plotCursorValue_ > visibleMax) {
        return QRect();
    }

    const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
    const long double ratio = (plotCursorValue_ - visibleMin) / visibleSpan;
    const int cursorX = track.left() + static_cast<int>(
        std::llround(ratio * static_cast<long double>(track.width() - 1)));

    const QFontMetrics metrics(scaleWidget_->font());
    const QString cursorLabel = latencyText(static_cast<qint64>(std::llround(plotCursorValue_)));
    const int labelWidth = metrics.horizontalAdvance(cursorLabel) + 12;
    const int labelHeight = qMin(track.height() - 4, metrics.height() + 6);
    const int labelLeft = qBound(track.left(),
                                 cursorX - labelWidth / 2,
                                 qMax(track.left(), track.right() - labelWidth + 1));
    const QRect localRect(labelLeft,
                          qMax(track.top() + 10, track.bottom() - labelHeight),
                          labelWidth,
                          labelHeight);
    return QRect(scaleWidget_->mapTo(this, localRect.topLeft()), localRect.size());
}

QRect LatencyWidget::testItemPlotViewportRect(const std::vector<int>& path) const
{
    if (!tree_) {
        return QRect();
    }

    QTreeWidgetItem* item = itemAtPath(tree_, path);
    if (!item) {
        return QRect();
    }

    const QRect rowRect = tree_->visualItemRect(item);
    const QRect column = plotColumnViewportRect();
    if (!rowRect.isValid() || !column.isValid()) {
        return QRect();
    }
    return QRect(column.left(), rowRect.top(), column.width(), rowRect.height());
}

QPoint LatencyWidget::testJitterSamplePoint(const std::vector<int>& path, const int sampleIndex) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    if (!item || sampleIndex < 0) {
        return QPoint();
    }

    const std::optional<LatencyStats> stats =
        statsFromVariant(item->data(kBoxPlotColumn, kPlotRole));
    if (!stats) {
        return QPoint();
    }

    const std::vector<qint64>& samples =
        stats->renderedSamples.empty() ? stats->outliers : stats->renderedSamples;
    if (sampleIndex >= static_cast<int>(samples.size())) {
        return QPoint();
    }

    const QRect cell = testItemPlotViewportRect(path);
    const QRect track = plotTrackRectForCell(cell);
    if (track.width() <= 1) {
        return QPoint();
    }

    const auto [visibleMin, visibleMax] = visiblePlotRange();
    const long double visibleSpan = std::max<long double>(1.0L, visibleMax - visibleMin);
    const auto xFor = [&](const long double value) {
        const long double ratio = (value - visibleMin) / visibleSpan;
        return track.left() + static_cast<int>(
            std::llround(ratio * static_cast<long double>(track.width() - 1)));
    };

    return jitterDotPointForSample(*stats,
                                   track,
                                   samples[static_cast<std::size_t>(sampleIndex)],
                                   static_cast<std::size_t>(sampleIndex),
                                   xFor);
}

void LatencyWidget::testSetPlotCursorFromPosition(const QPoint& position)
{
    setPlotCursorFromPosition(position);
}

std::optional<int> LatencyWidget::testPlotDotLogicalRowAtPosition(const QPoint& position) const
{
    return plotDotLogicalRowAtPosition(position);
}

void LatencyWidget::testBeginPlotLeftDrag(const QPoint& position, const Qt::KeyboardModifiers modifiers)
{
    beginPlotLeftDrag(position, modifiers);
}

void LatencyWidget::testUpdatePlotLeftDrag(const QPoint& position, const Qt::KeyboardModifiers modifiers)
{
    updatePlotLeftDrag(position, modifiers);
}

void LatencyWidget::testFinishPlotLeftDrag(const QPoint& position, const Qt::KeyboardModifiers modifiers)
{
    finishPlotLeftDrag(position, modifiers);
}

bool LatencyWidget::testLastBuildLoadedFromCache() const noexcept
{
    return lastBuildLoadedFromCache_;
}

QString LatencyWidget::testPlotType() const
{
    return plotTypeCombo_ ? plotTypeCombo_->currentText() : QString();
}

void LatencyWidget::testSetPlotType(const QString& plotType)
{
    if (!plotTypeCombo_) {
        return;
    }
    const int index = plotTypeCombo_->findText(plotType, Qt::MatchFixedString);
    if (index >= 0) {
        plotTypeCombo_->setCurrentIndex(index);
    }
}
#endif

}  // namespace CHIron::Gui
