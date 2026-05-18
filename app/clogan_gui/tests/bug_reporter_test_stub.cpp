#include "bug_reporter.hpp"

#include <new>

namespace CHIron::Gui {

void BugReporter::initialize()
{
}

void BugReporter::shutdown()
{
}

QString BugReporter::diagnosticsRootPath()
{
    return {};
}

QString BugReporter::sessionLogPath()
{
    return {};
}

QString BugReporter::liveStatePath()
{
    return {};
}

void BugReporter::updateLiveState(const QString&)
{
}

BugReportExportResult BugReporter::exportBugReport(const BugReportExportRequest&)
{
    return {};
}

QString BugReporter::takePendingCrashReportDirectory()
{
    return {};
}

[[noreturn]] void BugReporter::failFastOutOfMemory(const QString&)
{
    throw std::bad_alloc();
}

}  // namespace CHIron::Gui
