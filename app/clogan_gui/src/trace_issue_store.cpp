#include "trace_issue_store.hpp"

#include <QStringList>

namespace CHIron::Gui {

QString TraceIssueSeverityText(const TraceIssueSeverity severity)
{
    switch (severity) {
    case TraceIssueSeverity::Warning:
        return QStringLiteral("Warning");
    case TraceIssueSeverity::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Error");
}

QString TraceIssueDispositionText(const TraceIssueDisposition disposition)
{
    switch (disposition) {
    case TraceIssueDisposition::Error:
        return QStringLiteral("Error");
    case TraceIssueDisposition::Warning:
        return QStringLiteral("Warning");
    case TraceIssueDisposition::Ignored:
        return QStringLiteral("Ignored");
    }
    return QStringLiteral("Error");
}

QString TraceIssueDispositionSettingsText(const TraceIssueDisposition disposition)
{
    switch (disposition) {
    case TraceIssueDisposition::Error:
        return QStringLiteral("error");
    case TraceIssueDisposition::Warning:
        return QStringLiteral("warning");
    case TraceIssueDisposition::Ignored:
        return QStringLiteral("ignored");
    }
    return QStringLiteral("error");
}

TraceIssueDisposition TraceIssueDispositionFromSettingsText(const QString& text,
                                                            const TraceIssueDisposition fallback)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == QLatin1String("error")) {
        return TraceIssueDisposition::Error;
    }
    if (normalized == QLatin1String("warning")) {
        return TraceIssueDisposition::Warning;
    }
    if (normalized == QLatin1String("ignored")) {
        return TraceIssueDisposition::Ignored;
    }
    return fallback;
}

TraceIssueSeverity TraceIssueSeverityForDisposition(const TraceIssueDisposition disposition)
{
    return disposition == TraceIssueDisposition::Warning
        ? TraceIssueSeverity::Warning
        : TraceIssueSeverity::Error;
}

QString TraceIssueSourceText(const TraceIssueSource source)
{
    switch (source) {
    case TraceIssueSource::XactionIndex:
        return QStringLiteral("Xaction");
    case TraceIssueSource::CacheStateReplay:
        return QStringLiteral("Cache State");
    }
    return QStringLiteral("Issue");
}

bool TraceIssueDenialIsReportable(const QString& denialName)
{
    const QString trimmed = denialName.trimmed();
    return !trimmed.isEmpty()
        && trimmed != QStringLiteral("XACT_ACCEPTED")
        && trimmed != QStringLiteral("ACCEPTED")
        && trimmed != QStringLiteral("not processed");
}

TraceIssueSeverity TraceIssueSeverityForDenial(const QString& denialName)
{
    Q_UNUSED(denialName);
    return TraceIssueSeverity::Error;
}

TraceIssueCounts CountTraceIssues(const std::vector<TraceIssue>& issues)
{
    TraceIssueCounts counts;
    for (const TraceIssue& issue : issues) {
        switch (issue.severity) {
        case TraceIssueSeverity::Warning:
            ++counts.warnings;
            break;
        case TraceIssueSeverity::Error:
            ++counts.errors;
            break;
        }
    }
    return counts;
}

std::vector<TraceIssue> ApplyTraceIssueDispositionPolicy(const std::vector<TraceIssue>& rawIssues,
                                                         const TraceIssueDisposition xactionDisposition,
                                                         const TraceIssueDisposition cacheStateDisposition)
{
    std::vector<TraceIssue> visibleIssues;
    visibleIssues.reserve(rawIssues.size());
    for (TraceIssue issue : rawIssues) {
        const TraceIssueDisposition disposition =
            issue.source == TraceIssueSource::CacheStateReplay
                ? cacheStateDisposition
                : xactionDisposition;
        if (disposition == TraceIssueDisposition::Ignored) {
            continue;
        }
        issue.severity = TraceIssueSeverityForDisposition(disposition);
        visibleIssues.push_back(std::move(issue));
    }
    return visibleIssues;
}

QString TraceIssueAddressText(const std::uint64_t address)
{
    const QString digits = QString::number(static_cast<qulonglong>(address), 16).toUpper();
    return QStringLiteral("0x%1")
        .arg(digits);
}

QString TraceIssueRecordNodeText(const FlitRecord& record)
{
    if (record.channelNodeId) {
        return QStringLiteral("%1 %2")
            .arg(*record.channelNodeId)
            .arg(record.channelTag.trimmed().isEmpty() ? QString() : record.channelTag);
    }
    return record.channelTag;
}

}  // namespace CHIron::Gui
