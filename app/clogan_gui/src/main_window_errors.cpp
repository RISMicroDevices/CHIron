#include "main_window_internal.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_set>

namespace CHIron::Gui {
using namespace MainWindowDetail;

namespace {

constexpr auto kErrorsXactionDispositionSettingsKey = "errors/xactionDisposition";
constexpr auto kErrorsCacheStateDispositionSettingsKey = "errors/cacheStateDisposition";
constexpr auto kErrorsShowErrorsSettingsKey = "errors/showErrors";
constexpr auto kErrorsShowWarningsSettingsKey = "errors/showWarnings";

std::shared_ptr<const std::vector<TraceIssue>> EmptyTraceIssues()
{
    static const auto empty = std::make_shared<const std::vector<TraceIssue>>();
    return empty;
}

template <typename Session>
void applyTraceIssuePolicy(Session& session,
                           const TraceIssueDisposition xactionDisposition,
                           const TraceIssueDisposition cacheStateDisposition,
                           const bool showErrors,
                           const bool showWarnings)
{
    std::vector<TraceIssue> classifiedIssues =
        ApplyTraceIssueDispositionPolicy(*session.rawTraceIssues,
                                         xactionDisposition,
                                         cacheStateDisposition);
    session.traceIssueCounts = CountTraceIssues(classifiedIssues);
    std::vector<TraceIssue> visibleIssues = std::move(classifiedIssues);
    if (!showErrors || !showWarnings) {
        visibleIssues.erase(std::remove_if(visibleIssues.begin(),
                                           visibleIssues.end(),
                                           [showErrors, showWarnings](const TraceIssue& issue) {
                                               return issue.severity == TraceIssueSeverity::Error
                                                   ? !showErrors
                                                   : !showWarnings;
                                           }),
                            visibleIssues.end());
    }
    auto sharedIssues = std::make_shared<const std::vector<TraceIssue>>(std::move(visibleIssues));
    session.traceIssues = std::move(sharedIssues);
}

TraceIssueBuildResult makeTraceIssueBuildResult(std::vector<TraceIssue> rawIssues,
                                                const TraceIssueDisposition xactionDisposition,
                                                const TraceIssueDisposition cacheStateDisposition,
                                                const bool showErrors,
                                                const bool showWarnings)
{
    TraceIssueBuildResult result;
    std::vector<TraceIssue> classifiedIssues =
        ApplyTraceIssueDispositionPolicy(rawIssues,
                                         xactionDisposition,
                                         cacheStateDisposition);
    result.counts = CountTraceIssues(classifiedIssues);
    if (!showErrors || !showWarnings) {
        classifiedIssues.erase(std::remove_if(classifiedIssues.begin(),
                                              classifiedIssues.end(),
                                              [showErrors, showWarnings](const TraceIssue& issue) {
                                                  return issue.severity == TraceIssueSeverity::Error
                                                      ? !showErrors
                                                      : !showWarnings;
                                              }),
                               classifiedIssues.end());
    }
    result.rawIssues =
        std::make_shared<const std::vector<TraceIssue>>(std::move(rawIssues));
    result.visibleIssues =
        std::make_shared<const std::vector<TraceIssue>>(std::move(classifiedIssues));
    return result;
}

template <typename Session>
void setRawTraceIssues(Session& session,
                       std::vector<TraceIssue> issues,
                       const TraceIssueDisposition xactionDisposition,
                       const TraceIssueDisposition cacheStateDisposition,
                       const bool showErrors,
                       const bool showWarnings)
{
    session.rawTraceIssues = std::make_shared<const std::vector<TraceIssue>>(std::move(issues));
    applyTraceIssuePolicy(session,
                          xactionDisposition,
                          cacheStateDisposition,
                          showErrors,
                          showWarnings);
    session.traceIssuePersistedTableLoaded = true;
}

template <typename Session>
void setTraceIssueBuildResult(Session& session, TraceIssueBuildResult result)
{
    session.rawTraceIssues = result.rawIssues
        ? std::move(result.rawIssues)
        : EmptyTraceIssues();
    session.traceIssues = result.visibleIssues
        ? std::move(result.visibleIssues)
        : EmptyTraceIssues();
    session.traceIssueCounts = result.counts;
    session.traceIssuePersistedTableLoaded = true;
}

template <typename Session>
void clearTraceIssues(Session& session)
{
    session.rawTraceIssues = EmptyTraceIssues();
    session.traceIssues = EmptyTraceIssues();
    session.traceIssueCounts = {};
    session.traceIssuePersistedTableLoaded = false;
}

std::vector<TraceIssue> finalizePersistedTraceIssues(std::vector<TraceIssue> issues,
                                                     std::stop_token stopToken)
{
    std::unordered_set<QString> seen;
    std::vector<TraceIssue> finalized;
    finalized.reserve(issues.size());
    for (TraceIssue& issue : issues) {
        if (!seen.insert(issue.id).second) {
            continue;
        }
        if (stopToken.stop_requested()) {
            break;
        }
        if (issue.summary.isEmpty()) {
            issue.summary = QStringLiteral("%1 at row %2")
                                .arg(issue.denialName.isEmpty()
                                         ? TraceIssueSourceText(issue.source)
                                         : issue.denialName)
                                .arg(static_cast<qulonglong>(issue.logicalRow + 1));
        }
        if (issue.details.isEmpty()) {
            issue.details = QStringLiteral("%1\n  Logical row: %2")
                                .arg(issue.summary)
                                .arg(static_cast<qulonglong>(issue.logicalRow + 1));
        }
        finalized.push_back(std::move(issue));
    }
    std::sort(finalized.begin(), finalized.end(), [](const TraceIssue& lhs, const TraceIssue& rhs) {
        if (lhs.logicalRow != rhs.logicalRow) {
            return lhs.logicalRow < rhs.logicalRow;
        }
        if (lhs.source != rhs.source) {
            return static_cast<int>(lhs.source) < static_cast<int>(rhs.source);
        }
        return lhs.denialName < rhs.denialName;
    });
    return finalized;
}

}  // namespace

void MainWindow::loadTraceIssueDisplaySettings()
{
    QSettings settings = MakeLayoutSettings();
    xactionIssueDisposition_ = TraceIssueDispositionFromSettingsText(
        settings.value(QLatin1String(kErrorsXactionDispositionSettingsKey)).toString(),
        TraceIssueDisposition::Error);
    cacheStateIssueDisposition_ = TraceIssueDispositionFromSettingsText(
        settings.value(QLatin1String(kErrorsCacheStateDispositionSettingsKey)).toString(),
        TraceIssueDisposition::Error);
    errorIssuesVisible_ = settings.value(QLatin1String(kErrorsShowErrorsSettingsKey), true).toBool();
    warningIssuesVisible_ = settings.value(QLatin1String(kErrorsShowWarningsSettingsKey), true).toBool();
}

void MainWindow::saveTraceIssueDisplaySettings() const
{
    QSettings settings = MakeLayoutSettings();
    settings.setValue(QLatin1String(kErrorsXactionDispositionSettingsKey),
                      TraceIssueDispositionSettingsText(xactionIssueDisposition_));
    settings.setValue(QLatin1String(kErrorsCacheStateDispositionSettingsKey),
                      TraceIssueDispositionSettingsText(cacheStateIssueDisposition_));
    settings.setValue(QLatin1String(kErrorsShowErrorsSettingsKey), errorIssuesVisible_);
    settings.setValue(QLatin1String(kErrorsShowWarningsSettingsKey), warningIssuesVisible_);
    settings.sync();
}

void MainWindow::setTraceIssueDisposition(const TraceIssueSource source,
                                          const TraceIssueDisposition disposition)
{
    TraceIssueDisposition& target =
        source == TraceIssueSource::CacheStateReplay
            ? cacheStateIssueDisposition_
            : xactionIssueDisposition_;
    if (target == disposition) {
        if (errorsWidget_) {
            errorsWidget_->setIssueDisposition(source, disposition);
        }
        return;
    }
    target = disposition;
    if (errorsWidget_) {
        errorsWidget_->setIssueDisposition(source, disposition);
    }
    saveTraceIssueDisplaySettings();
    reapplyTraceIssueDisplayPolicy();
}

void MainWindow::setTraceIssueSeverityVisible(const TraceIssueSeverity severity, const bool visible)
{
    bool& target = severity == TraceIssueSeverity::Warning ? warningIssuesVisible_ : errorIssuesVisible_;
    if (target == visible) {
        if (errorsWidget_) {
            errorsWidget_->setSeverityVisible(severity, visible);
        }
        return;
    }
    target = visible;
    if (errorsWidget_) {
        errorsWidget_->setSeverityVisible(severity, visible);
    }
    saveTraceIssueDisplaySettings();
    reapplyTraceIssueDisplayPolicy();
}

void MainWindow::reapplyTraceIssueDisplayPolicy()
{
    ++traceIssueDisplayGeneration_;
    for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (!session) {
            continue;
        }
        applyTraceIssuePolicy(*session,
                              xactionIssueDisposition_,
                              cacheStateIssueDisposition_,
                              errorIssuesVisible_,
                              warningIssuesVisible_);
    }
    refreshErrorsWidget();
}

void MainWindow::showErrorsDock()
{
    if (errorsDock_) {
        errorsDock_->toggleView(true);
        errorsDock_->raise();
    }
    if (TraceViewSession* session = activeTraceViewSession()) {
        const bool forceReload = session->traceIssueBuildComplete
            && !session->traceIssueBuildActive
            && !session->traceIssuePersistedTableLoaded;
        startTraceIssueBuild(*session, forceReload);
    }
    refreshErrorsWidget();
}

void MainWindow::refreshErrorsWidget()
{
    if (!errorsWidget_) {
        return;
    }
    errorsWidget_->setIssueDisposition(TraceIssueSource::XactionIndex, xactionIssueDisposition_);
    errorsWidget_->setIssueDisposition(TraceIssueSource::CacheStateReplay, cacheStateIssueDisposition_);
    errorsWidget_->setSeverityVisible(TraceIssueSeverity::Error, errorIssuesVisible_);
    errorsWidget_->setSeverityVisible(TraceIssueSeverity::Warning, warningIssuesVisible_);
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        errorsWidget_->clear();
        return;
    }

    const int issueCount =
        session->traceIssues->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(session->traceIssues->size());
    if (!errorsWidget_->hasIssuesFor(session->id,
                                     session->traceIssueBuildGeneration + traceIssueDisplayGeneration_,
                                     issueCount,
                                     session->traceIssueCounts)) {
        errorsWidget_->setIssues(session->traceIssues,
                                 session->traceIssueCounts,
                                 session->id,
                                 session->traceIssueBuildGeneration + traceIssueDisplayGeneration_);
    }
    if (session->traceIssueBuildActive) {
        errorsWidget_->setBuildStatus(true,
                                      session->traceIssueBuildProgressText,
                                      session->traceIssueBuildCompletedRows,
                                      session->traceIssueBuildTotalRows);
    } else {
        errorsWidget_->setBuildStatus(false,
                                      session->traceIssueBuildProgressText,
                                      session->traceIssueBuildCompletedRows,
                                      session->traceIssueBuildTotalRows);
    }
}

void MainWindow::startTraceIssueBuild(TraceViewSession& session, const bool forceRebuild)
{
    session.traceIssueBuildRequested = true;
    if (!session.traceSession) {
        clearTraceIssues(session);
        session.traceIssueBuildActive = false;
        session.traceIssueBuildComplete = true;
        refreshErrorsWidget();
        return;
    }
    if (session.traceIssueBuildActive) {
        refreshErrorsWidget();
        return;
    }
    const std::shared_ptr<TraceSession> traceSession = session.traceSession;
    if (!forceRebuild
        && session.traceIssueBuildComplete
        && session.traceIssuePersistedTableLoaded) {
        refreshErrorsWidget();
        return;
    }

    const std::shared_ptr<const TraceSession::TraceSessionSnapshot> snapshot = traceSession->snapshot();
    if (!snapshot || !traceSession->isXactionIndexComplete()) {
        clearTraceIssues(session);
        session.traceIssueBuildActive = false;
        session.traceIssueBuildProgressText = traceSession->supportsXactionIndexing()
            ? QStringLiteral("Build Xaction index to collect protocol errors.")
            : QStringLiteral("Errors are unavailable for this trace.");
        session.traceIssueBuildComplete = !traceSession->supportsXactionIndexing();
        refreshErrorsWidget();
        if (traceSession->supportsXactionIndexing()) {
            const quint64 sessionId = session.id;
            QPointer<MainWindow> window(this);
            QTimer::singleShot(50, this, [window, traceSession, sessionId]() {
                if (window.isNull()) {
                    return;
                }
                MainWindow* mainWindow = window.data();
                TraceViewSession* target = mainWindow->traceViewSessionById(sessionId);
                if (!target
                    || target->traceSession != traceSession
                    || target->traceIssueBuildActive
                    || target->traceIssueBuildComplete) {
                    return;
                }
                if (traceSession->isXactionIndexComplete()) {
                    mainWindow->startTraceIssueBuild(*target, true);
                } else if (target->id == mainWindow->activeTraceSessionId_) {
                    mainWindow->refreshErrorsWidget();
                }
            });
        }
        return;
    }

    std::uint64_t persistedIssueCount = 0;
    CLogBTraceLoadError countError;
    if (traceSession->xactionIndexIssueCount(persistedIssueCount, countError)
        && persistedIssueCount == 0) {
        setRawTraceIssues(session,
                          {},
                          xactionIssueDisposition_,
                          cacheStateIssueDisposition_,
                          errorIssuesVisible_,
                          warningIssuesVisible_);
        session.traceIssueBuildActive = false;
        session.traceIssueBuildComplete = true;
        session.traceIssueBuildProgressText.clear();
        session.traceIssueBuildCompletedRows = 0;
        session.traceIssueBuildTotalRows = 0;
        refreshErrorsWidget();
        return;
    }

    auto stopSource = std::make_shared<std::stop_source>();
    session.traceIssueBuildStopSource = stopSource;
    const quint64 generation = ++session.traceIssueBuildGeneration;
    const quint64 sessionId = session.id;
    session.traceIssueBuildActive = true;
    session.traceIssueBuildComplete = false;
    session.traceIssueBuildProgressText = QStringLiteral("Loading persisted errors");
    session.traceIssueBuildCompletedRows = 0;
    session.traceIssueBuildTotalRows = persistedIssueCount;
    refreshErrorsWidget();

    QPointer<MainWindow> window(this);
    std::thread worker([window,
                        traceSession,
                        sessionId,
                        generation,
                        stopSource,
                        xactionDisposition = xactionIssueDisposition_,
                        cacheStateDisposition = cacheStateIssueDisposition_,
                        showErrors = errorIssuesVisible_,
                        showWarnings = warningIssuesVisible_]() mutable {
        const auto token = stopSource->get_token();
        std::vector<TraceIssue> issues;
        TraceIssueBuildResult result;
        QString errorText;
        bool succeeded = true;
        std::uint64_t lastProgressUpdate = 0;
        auto lastProgressPost = std::chrono::steady_clock::time_point{};

        const auto reportProgress = [&](const std::uint64_t completedIssues,
                                        const std::uint64_t totalIssues) {
            if (window.isNull()) {
                return;
            }
            const bool terminalProgress = completedIssues == 0 || completedIssues == totalIssues;
            if (!terminalProgress) {
                constexpr std::uint64_t kMinProgressIssueStep = 8192ULL;
                constexpr auto kMinProgressInterval = std::chrono::milliseconds(100);
                const auto now = std::chrono::steady_clock::now();
                if (completedIssues - lastProgressUpdate < kMinProgressIssueStep
                    || (lastProgressPost != std::chrono::steady_clock::time_point{}
                        && now - lastProgressPost < kMinProgressInterval)) {
                    return;
                }
                lastProgressPost = now;
            }
            lastProgressUpdate = completedIssues;
            QMetaObject::invokeMethod(window.data(),
                                      [window,
                                       traceSession,
                                       sessionId,
                                       generation,
                                       completedIssues,
                                       totalIssues]() {
                                          if (window.isNull()) {
                                              return;
                                          }
                                          MainWindow* mainWindow = window.data();
                                          TraceViewSession* target =
                                              mainWindow->traceViewSessionById(sessionId);
                                          if (!target
                                              || target->traceSession != traceSession
                                              || target->traceIssueBuildGeneration != generation
                                              || !target->traceIssueBuildActive) {
                                              return;
                                          }
                                          target->traceIssueBuildCompletedRows = completedIssues;
                                          target->traceIssueBuildTotalRows = totalIssues;
                                          if (target->id == mainWindow->activeTraceSessionId_) {
                                              mainWindow->refreshErrorsWidget();
                                          }
                                      },
                                      Qt::QueuedConnection);
        };

        try {
            auto loadIssues = [&]() {
                CLogBTraceLoadError error;
                issues.clear();
                if (traceSession->xactionIndexIssues(issues, error, token, reportProgress)) {
                    errorText.clear();
                    return true;
                }
                errorText = error.summary.isEmpty()
                    ? QStringLiteral("Failed to load persisted errors.")
                    : error.summary;
                return false;
            };

            succeeded = loadIssues();
            if (!succeeded && errorText.isEmpty()) {
                errorText = QStringLiteral("Failed to load persisted errors.");
            }

            if (succeeded && !token.stop_requested()) {
                issues = finalizePersistedTraceIssues(std::move(issues), token);
            }
            if (succeeded && !token.stop_requested()) {
                result = makeTraceIssueBuildResult(std::move(issues),
                                                   xactionDisposition,
                                                   cacheStateDisposition,
                                                   showErrors,
                                                   showWarnings);
            }
        } catch (const std::bad_alloc&) {
            succeeded = false;
            errorText = QStringLiteral("Error loading ran out of memory.");
        } catch (const std::exception& exception) {
            succeeded = false;
            errorText = QStringLiteral("Error loading failed unexpectedly: %1")
                            .arg(QString::fromLocal8Bit(exception.what()));
        } catch (...) {
            succeeded = false;
            errorText = QStringLiteral("Error loading failed unexpectedly.");
        }

        const bool cancelled = token.stop_requested();
        if (window.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(window.data(),
                                  [window,
                                   traceSession,
                                   sessionId,
                                   generation,
                                   result = std::move(result),
                                   succeeded,
                                   cancelled,
                                   errorText = std::move(errorText)]() mutable {
                                      if (window.isNull()) {
                                          return;
                                      }
                                      window->finishTraceIssueBuild(traceSession,
                                                                    sessionId,
                                                                    generation,
                                                                    std::move(result),
                                                                    succeeded,
                                                                    cancelled,
                                                                    std::move(errorText));
                                  },
                                  Qt::QueuedConnection);
    });
    worker.detach();
}

void MainWindow::cancelTraceIssueBuildForSession(TraceViewSession& session)
{
    ++session.traceIssueBuildGeneration;
    session.traceIssueBuildActive = false;
    session.traceIssuePersistedTableLoaded = false;
    if (session.traceIssueBuildStopSource) {
        session.traceIssueBuildStopSource->request_stop();
        session.traceIssueBuildStopSource.reset();
    }
    session.traceIssueBuildProgressText.clear();
    session.traceIssueBuildCompletedRows = 0;
    session.traceIssueBuildTotalRows = 0;
    if (session.id == activeTraceSessionId_) {
        refreshErrorsWidget();
    }
}

void MainWindow::finishTraceIssueBuild(std::shared_ptr<TraceSession> session,
                                       const quint64 sessionId,
                                       const quint64 generation,
                                       TraceIssueBuildResult result,
                                       const bool succeeded,
                                       const bool cancelled,
                                       QString errorText)
{
    TraceViewSession* target = traceViewSessionById(sessionId);
    if (!target
        || target->traceSession != session
        || target->traceIssueBuildGeneration != generation) {
        return;
    }

    target->traceIssueBuildActive = false;
    target->traceIssueBuildStopSource.reset();
    target->traceIssueBuildCompletedRows = 0;
    target->traceIssueBuildTotalRows = 0;
    target->traceIssueBuildProgressText.clear();
    if (cancelled) {
        if (target->id == activeTraceSessionId_) {
            refreshErrorsWidget();
        }
        return;
    }

    if (succeeded) {
        setTraceIssueBuildResult(*target, std::move(result));
        target->traceIssueBuildComplete = true;
        if (target->id == activeTraceSessionId_) {
            const std::size_t rawIssueCount = target->rawTraceIssues ? target->rawTraceIssues->size() : 0;
            statusBar()->showMessage(
                rawIssueCount == 0
                    ? QStringLiteral("No xaction or cache-state errors found.")
                    : QStringLiteral("Loaded %1 issue%2.")
                          .arg(rawIssueCount)
                          .arg(rawIssueCount == 1 ? QString() : QStringLiteral("s")),
                2500);
        }
    } else {
        target->traceIssueBuildProgressText = errorText;
        if (target->id == activeTraceSessionId_) {
            statusBar()->showMessage(errorText.isEmpty()
                                         ? QStringLiteral("Error loading failed.")
                                         : errorText,
                                     4000);
        }
        target->traceIssueBuildComplete = true;
    }

    if (target->id == activeTraceSessionId_) {
        refreshErrorsWidget();
    }
}

}  // namespace CHIron::Gui
