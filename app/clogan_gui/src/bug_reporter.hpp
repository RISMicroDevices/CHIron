#pragma once

#include <QString>

namespace CHIron::Gui {

struct BugReportExportRequest {
    QString title;
    QString summary;
    QString stateText;
    QString layoutFilePath;
};

struct BugReportExportResult {
    bool ok = false;
    QString directoryPath;
    QString reportPath;
    QString errorMessage;
};

class BugReporter final {
public:
    static void initialize();
    static void shutdown();

    static QString diagnosticsRootPath();
    static QString sessionLogPath();
    static QString liveStatePath();

    static void updateLiveState(const QString& stateText);
    static BugReportExportResult exportBugReport(const BugReportExportRequest& request);
    static QString takePendingCrashReportDirectory();
    [[noreturn]] static void failFastOutOfMemory(const QString& context);

private:
    BugReporter() = delete;
};

}  // namespace CHIron::Gui
