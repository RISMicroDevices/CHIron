#pragma once

#include "flit_record.hpp"

#include <QString>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace CHIron::Gui {

class TraceSession;

enum class LatencyTimeAnchorMode {
    RelativeSessionStart,
    AbsoluteTimestamp,
};

enum class LatencyDiffRangeMode {
    CommonScaledRange,
    SeparateSessionRanges,
};

struct LatencyAnalysisOptions {
    double timeScale = 1.0;
    LatencyTimeAnchorMode anchorMode = LatencyTimeAnchorMode::RelativeSessionStart;
    qint64 absoluteAnchor = 0;
    bool hasRange = false;
    double rangeStart = 0.0;
    double rangeEnd = 0.0;
};

struct LatencySample {
    qint64 rawStartTimestamp = 0;
    qint64 rawEndTimestamp = 0;
    double scaledStartTimestamp = 0.0;
    double scaledEndTimestamp = 0.0;
    double latency = 0.0;
    int logicalRow = -1;
};

struct LatencyStats {
    std::uint64_t count = 0;
    double min = 0.0;
    double q1 = 0.0;
    double median = 0.0;
    double q3 = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double lowerWhisker = 0.0;
    double upperWhisker = 0.0;
    std::uint64_t outlierCount = 0;
};

struct LatencyBucket {
    QString name;
    std::vector<LatencySample> samples;
    int exampleLogicalRow = -1;
};

struct RequestOpcodeBucket {
    QString opcode;
    std::vector<LatencySample> samples;
    std::map<QString, LatencyBucket> subsequentBuckets;
};

struct XactionTypeBucket {
    QString type;
    std::vector<LatencySample> samples;
    std::map<QString, RequestOpcodeBucket> requestOpcodeBuckets;
};

using LatencyBucketTree = std::map<QString, XactionTypeBucket>;

struct LatencyAnalysisResult {
    QString label;
    QString sourcePath;
    std::uint64_t totalXactions = 0;
    std::uint64_t sampleCount = 0;
    qint64 firstTimestamp = 0;
    qint64 lastTimestamp = 0;
    bool hasTimestampRange = false;
    bool failed = false;
    QString errorText;
    LatencyBucketTree typeBuckets;
};

enum class LatencyDiffMetricKind {
    Count,
    Min,
    Q1,
    Median,
    Q3,
    Max,
    Mean,
    LowerWhisker,
    UpperWhisker,
    OutlierCount,
};

struct LatencyDiffMetric {
    LatencyDiffMetricKind kind = LatencyDiffMetricKind::Count;
    QString name;
    double aValue = 0.0;
    double bValue = 0.0;
    double delta = 0.0;
    double percent = 0.0;
    bool percentComparable = true;
};

struct LatencyDiffRow {
    QStringList path;
    QString label;
    int depth = 0;
    bool presentInA = false;
    bool presentInB = false;
    std::vector<LatencyDiffMetric> metrics;
};

struct LatencyDiffOptions {
    QString sessionALabel;
    QString sessionBLabel;
    LatencyAnalysisOptions sessionA;
    LatencyAnalysisOptions sessionB;
    LatencyDiffRangeMode rangeMode = LatencyDiffRangeMode::CommonScaledRange;
};

struct LatencyDiffReport {
    QString sessionALabel;
    QString sessionBLabel;
    LatencyDiffOptions options;
    LatencyAnalysisResult analysisA;
    LatencyAnalysisResult analysisB;
    std::vector<LatencyDiffRow> rows;
    QString errorText;
    bool failed = false;
};

std::vector<LatencyDiffMetricKind> LatencyDiffMetricKinds();
QString LatencyDiffMetricName(LatencyDiffMetricKind kind);

LatencyStats CalculateLatencyStats(const std::vector<LatencySample>& samples);
double LatencyMetricValue(const LatencyStats& stats, LatencyDiffMetricKind kind);

LatencyAnalysisResult AnalyzeLatencyRows(const std::vector<FlitRecord>& rows,
                                         const LatencyAnalysisOptions& options = {},
                                         const QString& label = QString(),
                                         const QString& sourcePath = QString());

LatencyAnalysisResult AnalyzeLatencySession(const std::shared_ptr<TraceSession>& session,
                                            const LatencyAnalysisOptions& options = {},
                                            const QString& label = QString(),
                                            std::stop_token stopToken = {});

LatencyDiffReport BuildLatencyDiffReport(LatencyAnalysisResult analysisA,
                                         LatencyAnalysisResult analysisB,
                                         LatencyDiffOptions options = {});

QString FormatLatencyNumber(double value);
QString FormatLatencyPercent(double value, bool comparable = true);

}  // namespace CHIron::Gui
