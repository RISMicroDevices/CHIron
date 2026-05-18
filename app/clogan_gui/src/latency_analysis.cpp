#include "latency_analysis.hpp"

#include "clog_b_trace_loader.hpp"
#include "flit_transaction_keys.hpp"
#include "gui_format.hpp"
#include "trace_session.hpp"

#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace CHIron::Gui {

namespace {

struct TransactionSummary {
    QString xactionType = QStringLiteral("Unknown Xaction");
    QString requestOpcode;
    qint64 startTimestamp = 0;
    qint64 endTimestamp = 0;
    int firstLogicalRow = -1;
    std::vector<std::tuple<QString, qint64, int>> subsequentTimestamps;
    bool valid = false;
};

struct RowEvent {
    std::uint64_t ordinal = 0;
    std::uint64_t logicalRow = 0;
    qint64 timestamp = 0;
    FlitChannel channel = FlitChannel::Req;
    QString opcode;
    QString opcodeRaw;
};

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

QString displayOpcode(const RowEvent& event)
{
    if (!event.opcode.isEmpty()) {
        return event.opcode;
    }
    if (!event.opcodeRaw.isEmpty()) {
        return event.opcodeRaw;
    }
    return QStringLiteral("Unknown");
}

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

QString dataBeatLabel(const RowEvent& event, QSet<QString>& seenDataOpcodes)
{
    const QString opcode = displayOpcode(event);
    const bool firstBeat = !seenDataOpcodes.contains(opcode);
    seenDataOpcodes.insert(opcode);
    return firstBeat
        ? QStringLiteral("%1 first beat").arg(opcode)
        : QStringLiteral("%1 following beats").arg(opcode);
}

QString subsequentLabel(const RowEvent& event, QSet<QString>& seenDataOpcodes)
{
    if (event.channel == FlitChannel::Dat) {
        return dataBeatLabel(event, seenDataOpcodes);
    }
    return QStringLiteral("%1 %2").arg(ToString(event.channel), displayOpcode(event));
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

std::optional<TransactionSummary> summarizeTransaction(const std::shared_ptr<TraceSession>& session,
                                                       const std::uint64_t ordinal,
                                                       const std::vector<std::uint64_t>& logicalRows,
                                                       const std::vector<FlitRecord>& rows)
{
    if (ordinal == 0 || logicalRows.empty() || rows.empty() || logicalRows.size() != rows.size()) {
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

    summary.valid = true;
    return summary;
}

double scaledTime(qint64 rawTimestamp,
                  qint64 sessionFirstTimestamp,
                  const LatencyAnalysisOptions& options)
{
    const qint64 anchor = options.anchorMode == LatencyTimeAnchorMode::RelativeSessionStart
        ? sessionFirstTimestamp
        : options.absoluteAnchor;
    return (static_cast<double>(rawTimestamp) - static_cast<double>(anchor)) * options.timeScale;
}

bool includeByRange(qint64 rawStartTimestamp,
                    qint64 sessionFirstTimestamp,
                    const LatencyAnalysisOptions& options)
{
    if (!options.hasRange) {
        return true;
    }
    const double start = scaledTime(rawStartTimestamp, sessionFirstTimestamp, options);
    const double rangeMin = std::min(options.rangeStart, options.rangeEnd);
    const double rangeMax = std::max(options.rangeStart, options.rangeEnd);
    return start >= rangeMin && start <= rangeMax;
}

LatencySample makeSample(qint64 startTimestamp,
                         qint64 endTimestamp,
                         int logicalRow,
                         qint64 sessionFirstTimestamp,
                         const LatencyAnalysisOptions& options)
{
    const double scaledStart = scaledTime(startTimestamp, sessionFirstTimestamp, options);
    const double scaledEnd = scaledTime(endTimestamp, sessionFirstTimestamp, options);
    return LatencySample{
        .rawStartTimestamp = startTimestamp,
        .rawEndTimestamp = endTimestamp,
        .scaledStartTimestamp = scaledStart,
        .scaledEndTimestamp = scaledEnd,
        .latency = scaledEnd - scaledStart,
        .logicalRow = logicalRow,
    };
}

void addCompletionLatencySample(LatencyBucketTree& buckets,
                                const QString& xactionType,
                                const QString& requestOpcode,
                                const LatencySample& sample)
{
    XactionTypeBucket& typeBucket = buckets[xactionType];
    typeBucket.type = xactionType;
    typeBucket.samples.push_back(sample);
    RequestOpcodeBucket& requestBucket = typeBucket.requestOpcodeBuckets[requestOpcode];
    requestBucket.opcode = requestOpcode;
    requestBucket.samples.push_back(sample);
}

void addSubsequentLatencySample(LatencyBucketTree& buckets,
                                std::uint64_t& sampleCount,
                                const QString& xactionType,
                                const QString& requestOpcode,
                                const QString& subsequent,
                                const LatencySample& sample)
{
    XactionTypeBucket& typeBucket = buckets[xactionType];
    typeBucket.type = xactionType;
    RequestOpcodeBucket& requestBucket = typeBucket.requestOpcodeBuckets[requestOpcode];
    requestBucket.opcode = requestOpcode;
    LatencyBucket& bucket = requestBucket.subsequentBuckets[subsequent];
    bucket.name = subsequent;
    bucket.samples.push_back(sample);
    if (bucket.exampleLogicalRow < 0) {
        bucket.exampleLogicalRow = sample.logicalRow;
    }
    ++sampleCount;
}

LatencyBucketTree buildBucketsFromTransactions(
    const std::shared_ptr<TraceSession>& session,
    const std::vector<std::pair<std::uint64_t, std::pair<std::vector<std::uint64_t>, std::vector<FlitRecord>>>>& transactions,
    qint64 sessionFirstTimestamp,
    const LatencyAnalysisOptions& options,
    std::uint64_t& sampleCount)
{
    LatencyBucketTree buckets;
    sampleCount = 0;

    for (const auto& [ordinal, payload] : transactions) {
        const auto& logicalRows = payload.first;
        const auto& rows = payload.second;
        const std::optional<TransactionSummary> summary =
            summarizeTransaction(session, ordinal, logicalRows, rows);
        if (!summary || !summary->valid) {
            continue;
        }
        if (!includeByRange(summary->startTimestamp, sessionFirstTimestamp, options)) {
            continue;
        }

        addCompletionLatencySample(buckets,
                                   summary->xactionType,
                                   summary->requestOpcode,
                                   makeSample(summary->startTimestamp,
                                              summary->endTimestamp,
                                              summary->firstLogicalRow,
                                              sessionFirstTimestamp,
                                              options));
        for (const auto& [subsequent, timestamp, logicalRow] : summary->subsequentTimestamps) {
            addSubsequentLatencySample(buckets,
                                       sampleCount,
                                       summary->xactionType,
                                       summary->requestOpcode,
                                       subsequent,
                                       makeSample(summary->startTimestamp,
                                                  timestamp,
                                                  logicalRow,
                                                  sessionFirstTimestamp,
                                                  options));
        }
    }

    return buckets;
}

qint64 percentileOfSorted(const std::vector<double>& values, double percentile)
{
    if (values.empty()) {
        return 0;
    }
    if (values.size() == 1) {
        return static_cast<qint64>(std::llround(values.front()));
    }

    const double index = percentile * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(index));
    const auto upper = static_cast<std::size_t>(std::ceil(index));
    if (lower == upper) {
        return static_cast<qint64>(std::llround(values[lower]));
    }

    const double fraction = index - static_cast<double>(lower);
    return static_cast<qint64>(std::llround(
        values[lower] * (1.0 - fraction) + values[upper] * fraction));
}

std::vector<double> sampleLatencies(const std::vector<LatencySample>& samples)
{
    std::vector<double> values;
    values.reserve(samples.size());
    for (const LatencySample& sample : samples) {
        values.push_back(sample.latency);
    }
    std::sort(values.begin(), values.end());
    return values;
}

void collectRow(std::vector<LatencyDiffRow>& rows,
                QStringList path,
                const QString& label,
                int depth,
                const std::vector<LatencySample>* aSamples,
                const std::vector<LatencySample>* bSamples)
{
    const LatencyStats aStats = aSamples ? CalculateLatencyStats(*aSamples) : LatencyStats{};
    const LatencyStats bStats = bSamples ? CalculateLatencyStats(*bSamples) : LatencyStats{};
    LatencyDiffRow row;
    row.path = std::move(path);
    row.label = label;
    row.depth = depth;
    row.presentInA = aSamples != nullptr;
    row.presentInB = bSamples != nullptr;

    for (const LatencyDiffMetricKind kind : LatencyDiffMetricKinds()) {
        const double aValue = LatencyMetricValue(aStats, kind);
        const double bValue = LatencyMetricValue(bStats, kind);
        LatencyDiffMetric metric;
        metric.kind = kind;
        metric.name = LatencyDiffMetricName(kind);
        metric.aValue = aValue;
        metric.bValue = bValue;
        metric.delta = bValue - aValue;
        if (aValue == 0.0) {
            metric.percentComparable = bValue == 0.0;
            metric.percent = 0.0;
        } else {
            metric.percentComparable = true;
            metric.percent = metric.delta / aValue;
        }
        row.metrics.push_back(metric);
    }

    rows.push_back(std::move(row));
}

const XactionTypeBucket* findType(const LatencyBucketTree& tree, const QString& key)
{
    const auto found = tree.find(key);
    return found == tree.end() ? nullptr : &found->second;
}

const RequestOpcodeBucket* findRequest(const XactionTypeBucket* bucket, const QString& key)
{
    if (!bucket) {
        return nullptr;
    }
    const auto found = bucket->requestOpcodeBuckets.find(key);
    return found == bucket->requestOpcodeBuckets.end() ? nullptr : &found->second;
}

const LatencyBucket* findSubsequent(const RequestOpcodeBucket* bucket, const QString& key)
{
    if (!bucket) {
        return nullptr;
    }
    const auto found = bucket->subsequentBuckets.find(key);
    return found == bucket->subsequentBuckets.end() ? nullptr : &found->second;
}

}  // namespace

std::vector<LatencyDiffMetricKind> LatencyDiffMetricKinds()
{
    return {
        LatencyDiffMetricKind::Count,
        LatencyDiffMetricKind::Min,
        LatencyDiffMetricKind::Q1,
        LatencyDiffMetricKind::Median,
        LatencyDiffMetricKind::Q3,
        LatencyDiffMetricKind::Max,
        LatencyDiffMetricKind::Mean,
        LatencyDiffMetricKind::LowerWhisker,
        LatencyDiffMetricKind::UpperWhisker,
        LatencyDiffMetricKind::OutlierCount,
    };
}

QString LatencyDiffMetricName(const LatencyDiffMetricKind kind)
{
    switch (kind) {
    case LatencyDiffMetricKind::Count:
        return QStringLiteral("Count");
    case LatencyDiffMetricKind::Min:
        return QStringLiteral("Min");
    case LatencyDiffMetricKind::Q1:
        return QStringLiteral("Q1");
    case LatencyDiffMetricKind::Median:
        return QStringLiteral("Median");
    case LatencyDiffMetricKind::Q3:
        return QStringLiteral("Q3");
    case LatencyDiffMetricKind::Max:
        return QStringLiteral("Max");
    case LatencyDiffMetricKind::Mean:
        return QStringLiteral("Mean");
    case LatencyDiffMetricKind::LowerWhisker:
        return QStringLiteral("Lower Whisker");
    case LatencyDiffMetricKind::UpperWhisker:
        return QStringLiteral("Upper Whisker");
    case LatencyDiffMetricKind::OutlierCount:
        return QStringLiteral("Outliers");
    }
    return QStringLiteral("Unknown");
}

LatencyStats CalculateLatencyStats(const std::vector<LatencySample>& samples)
{
    LatencyStats stats;
    stats.count = static_cast<std::uint64_t>(samples.size());
    if (samples.empty()) {
        return stats;
    }

    const std::vector<double> values = sampleLatencies(samples);
    stats.min = values.front();
    stats.q1 = percentileOfSorted(values, 0.25);
    stats.median = percentileOfSorted(values, 0.50);
    stats.q3 = percentileOfSorted(values, 0.75);
    stats.max = values.back();
    stats.lowerWhisker = stats.min;
    stats.upperWhisker = stats.max;

    long double sum = 0.0L;
    for (const double value : values) {
        sum += static_cast<long double>(value);
    }
    stats.mean = static_cast<double>(
        std::llround(sum / static_cast<long double>(values.size())));

    const long double iqr = static_cast<long double>(stats.q3) - static_cast<long double>(stats.q1);
    const long double lowerFence = static_cast<long double>(stats.q1) - 1.5L * iqr;
    const long double upperFence = static_cast<long double>(stats.q3) + 1.5L * iqr;
    const auto isOutlier = [&](const double value) {
        const long double wideValue = static_cast<long double>(value);
        return wideValue < lowerFence || wideValue > upperFence;
    };

    const auto lowerWhisker = std::find_if(values.begin(), values.end(), [&](const double value) {
        return !isOutlier(value);
    });
    const auto upperWhisker = std::find_if(values.rbegin(), values.rend(), [&](const double value) {
        return !isOutlier(value);
    });
    if (lowerWhisker != values.end()) {
        stats.lowerWhisker = *lowerWhisker;
    }
    if (upperWhisker != values.rend()) {
        stats.upperWhisker = *upperWhisker;
    }

    stats.outlierCount = static_cast<std::uint64_t>(
        std::count_if(values.begin(), values.end(), isOutlier));
    return stats;
}

double LatencyMetricValue(const LatencyStats& stats, const LatencyDiffMetricKind kind)
{
    switch (kind) {
    case LatencyDiffMetricKind::Count:
        return static_cast<double>(stats.count);
    case LatencyDiffMetricKind::Min:
        return stats.min;
    case LatencyDiffMetricKind::Q1:
        return stats.q1;
    case LatencyDiffMetricKind::Median:
        return stats.median;
    case LatencyDiffMetricKind::Q3:
        return stats.q3;
    case LatencyDiffMetricKind::Max:
        return stats.max;
    case LatencyDiffMetricKind::Mean:
        return stats.mean;
    case LatencyDiffMetricKind::LowerWhisker:
        return stats.lowerWhisker;
    case LatencyDiffMetricKind::UpperWhisker:
        return stats.upperWhisker;
    case LatencyDiffMetricKind::OutlierCount:
        return static_cast<double>(stats.outlierCount);
    }
    return 0.0;
}

LatencyAnalysisResult AnalyzeLatencyRows(const std::vector<FlitRecord>& rows,
                                         const LatencyAnalysisOptions& options,
                                         const QString& label,
                                         const QString& sourcePath)
{
    LatencyAnalysisResult result;
    result.label = label;
    result.sourcePath = sourcePath;
    if (rows.empty()) {
        return result;
    }

    result.hasTimestampRange = true;
    result.firstTimestamp = rows.front().timestamp;
    result.lastTimestamp = rows.front().timestamp;
    for (const FlitRecord& row : rows) {
        result.firstTimestamp = std::min(result.firstTimestamp, row.timestamp);
        result.lastTimestamp = std::max(result.lastTimestamp, row.timestamp);
    }

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
    }

    std::vector<std::pair<std::uint64_t, std::pair<std::vector<std::uint64_t>, std::vector<FlitRecord>>>> transactions;
    transactions.reserve(groupedRows.size());
    for (auto& [ordinal, payload] : groupedRows) {
        transactions.emplace_back(ordinal, std::move(payload));
    }
    std::sort(transactions.begin(), transactions.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    result.totalXactions = static_cast<std::uint64_t>(transactions.size());
    result.typeBuckets = buildBucketsFromTransactions(nullptr,
                                                      transactions,
                                                      result.firstTimestamp,
                                                      options,
                                                      result.sampleCount);
    return result;
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

LatencyAnalysisResult AnalyzeLatencySession(const std::shared_ptr<TraceSession>& session,
                                            const LatencyAnalysisOptions& options,
                                            const QString& label,
                                            std::stop_token stopToken)
{
    LatencyAnalysisResult result;
    result.label = label;
    if (!session) {
        result.failed = true;
        result.errorText = QStringLiteral("No trace session is selected.");
        return result;
    }
    result.sourcePath = session->filePath();
    result.hasTimestampRange = true;
    result.firstTimestamp = session->metadata().firstTimestamp;
    result.lastTimestamp = session->metadata().lastTimestamp;

    if (!session->supportsXactionIndexing()) {
        result.failed = true;
        result.errorText = QStringLiteral("Latency Diff requires CHI E.b traces with Xaction indexing.");
        return result;
    }
    if (!session->isXactionIndexComplete()) {
        result.failed = true;
        result.errorText = QStringLiteral("Build the Xaction index before running Latency Diff.");
        return result;
    }

    std::unordered_set<std::uint64_t> completedOrdinals;
    if (!session->xactionCompletedOrdinals(completedOrdinals)) {
        result.failed = true;
        result.errorText = QStringLiteral("Failed to read completed Xaction metadata.");
        return result;
    }
    result.totalXactions = static_cast<std::uint64_t>(completedOrdinals.size());

    std::unordered_map<std::uint64_t, std::uint64_t> rowCounts;
    if (!session->xactionRowCountsByOrdinal(rowCounts)) {
        result.failed = true;
        result.errorText = QStringLiteral("Failed to read Xaction row-count metadata.");
        return result;
    }

    std::unordered_map<std::uint64_t, QString> xactionTypes;
    if (!session->xactionTypesByOrdinal(xactionTypes)) {
        result.failed = true;
        result.errorText = QStringLiteral("Failed to read Xaction type metadata.");
        return result;
    }

    std::vector<RowEvent> events;
    CLogBTraceLoadError readError;
    for (std::size_t blockIndex = 0; blockIndex < session->metadata().blocks.size(); ++blockIndex) {
        if (stopToken.stop_requested()) {
            result.failed = true;
            result.errorText = QStringLiteral("Latency Diff was cancelled.");
            return result;
        }

        const CLogBTraceBlockSummary& block = session->metadata().blocks[blockIndex];
        std::vector<CLogBTraceFastRecordInfo> fastRecords;
        if (!session->readFastRecords(blockIndex, 0, block.recordCount, fastRecords, readError, stopToken)) {
            result.failed = true;
            result.errorText = readError.summary.isEmpty()
                ? QStringLiteral("Failed to read trace fast records.")
                : readError.summary;
            return result;
        }

        std::vector<std::uint64_t> ordinals;
        if (!session->xactionOrdinalsByRowRange(block.rowStart, block.recordCount, ordinals)
            || ordinals.size() < fastRecords.size()) {
            result.failed = true;
            result.errorText = QStringLiteral("Failed to read Xaction row ordinals.");
            return result;
        }

        events.reserve(events.size() + fastRecords.size());
        std::unordered_map<std::uint32_t, QString> opcodeCache;
        for (std::size_t index = 0; index < fastRecords.size(); ++index) {
            const std::uint64_t ordinal = ordinals[index];
            if (ordinal == 0 || completedOrdinals.find(ordinal) == completedOrdinals.end()) {
                continue;
            }
            const CLogBTraceFastRecordInfo& fastRecord = fastRecords[index];
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

            events.push_back(RowEvent{
                .ordinal = ordinal,
                .logicalRow = block.rowStart + index,
                .timestamp = fastRecord.timestamp,
                .channel = flitChannelFromTraceChannel(static_cast<CLog::Channel>(fastRecord.channel)),
                .opcode = std::move(opcode),
                .opcodeRaw = std::move(opcodeRaw),
            });
        }
    }

    std::sort(events.begin(), events.end(), [](const RowEvent& left, const RowEvent& right) {
        if (left.ordinal != right.ordinal) {
            return left.ordinal < right.ordinal;
        }
        return left.logicalRow < right.logicalRow;
    });

    struct StreamingState {
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

    std::unordered_map<std::uint64_t, StreamingState> states;
    states.reserve(std::min<std::size_t>(events.size(), 65536));

    const auto finishIfComplete = [&](const std::uint64_t ordinal,
                                      StreamingState& state) -> bool {
        if (state.expectedRows == 0 || state.observedRows < state.expectedRows) {
            return false;
        }
        if (state.hasStart
            && includeByRange(state.startTimestamp, result.firstTimestamp, options)
            && !state.emittedCompletionSample) {
            addCompletionLatencySample(result.typeBuckets,
                                       state.xactionType,
                                       state.requestOpcode,
                                       makeSample(state.startTimestamp,
                                                  state.endTimestamp,
                                                  state.firstLogicalRow,
                                                  result.firstTimestamp,
                                                  options));
            state.emittedCompletionSample = true;
        }
        if (state.hasStart
            && includeByRange(state.startTimestamp, result.firstTimestamp, options)
            && !state.emittedSample) {
            addSubsequentLatencySample(result.typeBuckets,
                                       result.sampleCount,
                                       state.xactionType,
                                       state.requestOpcode,
                                       QStringLiteral("No subsequent flits"),
                                       makeSample(state.startTimestamp,
                                                  state.endTimestamp,
                                                  state.firstLogicalRow,
                                                  result.firstTimestamp,
                                                  options));
            state.emittedSample = true;
        }
        states.erase(ordinal);
        return true;
    };

    for (const RowEvent& event : events) {
        if (stopToken.stop_requested()) {
            result.failed = true;
            result.errorText = QStringLiteral("Latency Diff was cancelled.");
            return result;
        }

        StreamingState& state = states[event.ordinal];
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

        if (includeByRange(state.startTimestamp, result.firstTimestamp, options)) {
            addSubsequentLatencySample(result.typeBuckets,
                                       result.sampleCount,
                                       state.xactionType,
                                       state.requestOpcode,
                                       subsequentLabel(event, state.seenDataOpcodes),
                                       makeSample(state.startTimestamp,
                                                  event.timestamp,
                                                  static_cast<int>(event.logicalRow),
                                                  result.firstTimestamp,
                                                  options));
            state.emittedSample = true;
        }

        finishIfComplete(event.ordinal, state);
    }

    return result;
}

LatencyDiffReport BuildLatencyDiffReport(LatencyAnalysisResult analysisA,
                                         LatencyAnalysisResult analysisB,
                                         LatencyDiffOptions options)
{
    LatencyDiffReport report;
    report.sessionALabel = options.sessionALabel.isEmpty() ? analysisA.label : options.sessionALabel;
    report.sessionBLabel = options.sessionBLabel.isEmpty() ? analysisB.label : options.sessionBLabel;
    report.options = options;
    report.analysisA = std::move(analysisA);
    report.analysisB = std::move(analysisB);
    if (report.analysisA.failed) {
        report.failed = true;
        report.errorText = report.analysisA.errorText;
        return report;
    }
    if (report.analysisB.failed) {
        report.failed = true;
        report.errorText = report.analysisB.errorText;
        return report;
    }

    std::set<QString> typeKeys;
    for (const auto& [key, _] : report.analysisA.typeBuckets) {
        typeKeys.insert(key);
    }
    for (const auto& [key, _] : report.analysisB.typeBuckets) {
        typeKeys.insert(key);
    }

    for (const QString& typeKey : typeKeys) {
        const XactionTypeBucket* aType = findType(report.analysisA.typeBuckets, typeKey);
        const XactionTypeBucket* bType = findType(report.analysisB.typeBuckets, typeKey);
        collectRow(report.rows,
                   QStringList{typeKey},
                   typeKey,
                   0,
                   aType ? &aType->samples : nullptr,
                   bType ? &bType->samples : nullptr);

        std::set<QString> requestKeys;
        if (aType) {
            for (const auto& [key, _] : aType->requestOpcodeBuckets) {
                requestKeys.insert(key);
            }
        }
        if (bType) {
            for (const auto& [key, _] : bType->requestOpcodeBuckets) {
                requestKeys.insert(key);
            }
        }

        for (const QString& requestKey : requestKeys) {
            const RequestOpcodeBucket* aRequest = findRequest(aType, requestKey);
            const RequestOpcodeBucket* bRequest = findRequest(bType, requestKey);
            collectRow(report.rows,
                       QStringList{typeKey, requestKey},
                       requestKey,
                       1,
                       aRequest ? &aRequest->samples : nullptr,
                       bRequest ? &bRequest->samples : nullptr);

            std::set<QString> subsequentKeys;
            if (aRequest) {
                for (const auto& [key, _] : aRequest->subsequentBuckets) {
                    subsequentKeys.insert(key);
                }
            }
            if (bRequest) {
                for (const auto& [key, _] : bRequest->subsequentBuckets) {
                    subsequentKeys.insert(key);
                }
            }

            for (const QString& subsequentKey : subsequentKeys) {
                const LatencyBucket* aSubsequent = findSubsequent(aRequest, subsequentKey);
                const LatencyBucket* bSubsequent = findSubsequent(bRequest, subsequentKey);
                collectRow(report.rows,
                           QStringList{typeKey, requestKey, subsequentKey},
                           subsequentKey,
                           2,
                           aSubsequent ? &aSubsequent->samples : nullptr,
                           bSubsequent ? &bSubsequent->samples : nullptr);
            }
        }
    }

    return report;
}

QString FormatLatencyNumber(const double value)
{
    if (!std::isfinite(value)) {
        return QStringLiteral("N/A");
    }
    return FormatSignedIntegerWithThousands(static_cast<qint64>(std::llround(value)));
}

QString FormatLatencyPercent(const double value, const bool comparable)
{
    if (!comparable || !std::isfinite(value)) {
        return QStringLiteral("N/A");
    }
    return QStringLiteral("%1%").arg(QString::number(value * 100.0, 'f', 1));
}

}  // namespace CHIron::Gui
