#pragma once

#include "flit_record.hpp"

#include <QString>

#include <cstdint>
#include <vector>

namespace CHIron::Gui {

enum class TraceIssueSeverity {
    Warning = 0,
    Error
};

enum class TraceIssueDisposition {
    Error = 0,
    Warning,
    Ignored
};

enum class TraceIssueSource {
    XactionIndex = 0,
    CacheStateReplay
};

struct TraceIssue {
    QString id;
    std::uint64_t logicalRow = 0;
    qint64 timestamp = 0;
    TraceIssueSeverity severity = TraceIssueSeverity::Error;
    TraceIssueSource source = TraceIssueSource::XactionIndex;
    QString denialName;
    QString denialCode;
    std::uint32_t denialValue = 0;
    QString channel;
    QString opcode;
    QString node;
    QString address;
    QString summary;
    QString details;
};

struct TraceIssueCounts {
    int warnings = 0;
    int errors = 0;
};

QString TraceIssueSeverityText(TraceIssueSeverity severity);
QString TraceIssueDispositionText(TraceIssueDisposition disposition);
QString TraceIssueDispositionSettingsText(TraceIssueDisposition disposition);
TraceIssueDisposition TraceIssueDispositionFromSettingsText(const QString& text,
                                                            TraceIssueDisposition fallback);
TraceIssueSeverity TraceIssueSeverityForDisposition(TraceIssueDisposition disposition);
QString TraceIssueSourceText(TraceIssueSource source);
bool TraceIssueDenialIsReportable(const QString& denialName);
TraceIssueSeverity TraceIssueSeverityForDenial(const QString& denialName);
TraceIssueCounts CountTraceIssues(const std::vector<TraceIssue>& issues);
std::vector<TraceIssue> ApplyTraceIssueDispositionPolicy(const std::vector<TraceIssue>& rawIssues,
                                                         TraceIssueDisposition xactionDisposition,
                                                         TraceIssueDisposition cacheStateDisposition);
QString TraceIssueAddressText(std::uint64_t address);
QString TraceIssueRecordNodeText(const FlitRecord& record);

}  // namespace CHIron::Gui
