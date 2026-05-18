#include "main_window_internal.hpp"

#include <chrono>
#include <exception>
#include <utility>

namespace CHIron::Gui {
using namespace MainWindowDetail;

namespace {

void showXactionIndexErrorDialog(QWidget* parent,
                                 const QString& traceLabel,
                                 const CLogBTraceLoadError& error)
{
    QString informativeText;
    QTextStream stream(&informativeText);
    if (!traceLabel.isEmpty()) {
        stream << "Trace: " << traceLabel << "\n\n";
    }
    if (!error.informativeText.isEmpty()) {
        stream << error.informativeText << "\n\n";
    }
    stream << "The partial Xaction index was cleared. The trace remains loaded.";

    QString detailedText;
    QTextStream details(&detailedText);
    if (!error.detailedText.isEmpty()) {
        details << error.detailedText;
        if (!detailedText.endsWith(QLatin1Char('\n'))) {
            details << '\n';
        }
    }
    if (!error.flitReportText.isEmpty()) {
        if (!detailedText.isEmpty()) {
            details << '\n';
        }
        details << "Current CHI Flit Report\n";
        details << "-----------------------\n";
        details << error.flitReportText;
    }

    QMessageBox dialog(parent);
    dialog.setIcon(QMessageBox::Critical);
    dialog.setWindowTitle(QStringLiteral("Xaction Index Failed"));
    dialog.setText(error.summary.isEmpty()
                       ? QStringLiteral("Xaction indexing failed.")
                       : error.summary);
    dialog.setInformativeText(informativeText.trimmed());
    if (!detailedText.isEmpty()) {
        dialog.setDetailedText(detailedText);
    }
    dialog.addButton(QMessageBox::Close);
    dialog.exec();
}

}  // namespace

void MainWindow::startXactionIndexing(const bool rebuildExisting)
{
    TraceViewSession* targetSession = activeTraceViewSession();
    if (!targetSession || !currentTraceSession_ || !currentTraceSession_->supportsXactionIndexing()) {
        statusBar()->showMessage(QStringLiteral("Xaction indexing is not available for this trace."), 3000);
        updateXactionIndexAction();
        return;
    }

    if (targetSession->xactionIndexActive) {
        return;
    }

    const bool complete = currentTraceSession_->isXactionIndexComplete();
    if (complete && !rebuildExisting) {
        statusBar()->showMessage(QStringLiteral("Xaction index is already ready."), 3000);
        updateXactionIndexAction();
        return;
    }

    const std::shared_ptr<TraceSession> session = currentTraceSession_;
    const quint64 sessionId = targetSession->id;

    auto stopSource = std::make_shared<std::stop_source>();
    targetSession->xactionIndexStopSource = stopSource;
    const quint64 generation = ++targetSession->xactionIndexGeneration;
    targetSession->xactionIndexActive = true;
    xactionIndexStopSource_ = targetSession->xactionIndexStopSource;
    xactionIndexGeneration_ = targetSession->xactionIndexGeneration;
    xactionIndexActive_ = targetSession->xactionIndexActive;
    updateXactionIndexAction();

    updateXactionIndexProgress(true,
                               QStringLiteral("Xaction index"),
                               0,
                               session->totalRows());
    statusBar()->showMessage(rebuildExisting
                                 ? QStringLiteral("Rebuilding Xaction index in background...")
                                 : QStringLiteral("Building Xaction index in background..."),
                             3000);
    session->markXactionIndexStarted();
    session->refreshCachedXactionIndexRows();
    if (flitModel_) {
        flitModel_->clearTransactionHighlightWithoutRefresh();
        scheduleVisibleXactionIndexRowsRefresh();
    }
    refreshTransactionView(*targetSession);

    QPointer<MainWindow> window(this);
    std::thread indexThread([session, sessionId, stopSource, window, generation]() mutable {
        struct XactionIndexProgressState {
            std::atomic<std::uint64_t> completedRecords = 0;
            std::atomic<std::uint64_t> totalRecords = 0;
            std::atomic_bool flushPending = false;
            std::atomic<std::int64_t> lastProgressPostMs = 0;
            std::mutex textMutex;
            QString text = QStringLiteral("Xaction index");
        };

        auto progressState = std::make_shared<XactionIndexProgressState>();
        progressState->totalRecords.store(session->totalRows(), std::memory_order_relaxed);

        CLogBTraceLoadError error;
        bool succeeded = false;

        const auto postProgress = [&](const bool force = false) {
            constexpr std::int64_t kProgressPostIntervalMs = 100;
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            if (!force) {
                std::int64_t previousMs =
                    progressState->lastProgressPostMs.load(std::memory_order_relaxed);
                while (true) {
                    if (nowMs - previousMs < kProgressPostIntervalMs) {
                        return;
                    }
                    if (progressState->lastProgressPostMs.compare_exchange_weak(
                            previousMs,
                            nowMs,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)) {
                        break;
                    }
                }
            } else {
                progressState->lastProgressPostMs.store(nowMs, std::memory_order_relaxed);
            }

            bool expected = false;
            if (!progressState->flushPending.compare_exchange_strong(expected,
                                                                     true,
                                                                     std::memory_order_acq_rel,
                                                                     std::memory_order_relaxed)) {
                return;
            }
            if (window.isNull()) {
                return;
            }

            QMetaObject::invokeMethod(window.data(),
                                      [window,
                                       session,
                                       sessionId,
                                       generation,
                                       progressState]() {
                                          progressState->flushPending.store(false, std::memory_order_release);
                                          if (window.isNull()) {
                                              return;
                                          }

                                          MainWindow* mainWindow = window.data();
                                          auto* target = mainWindow->traceViewSessionById(sessionId);
                                          if (!target
                                              || target->traceSession != session
                                              || generation != target->xactionIndexGeneration
                                              || !target->xactionIndexActive) {
                                              return;
                                          }

                                          const QString text = [&]() {
                                              const std::lock_guard lock(progressState->textMutex);
                                              return progressState->text;
                                          }();
                                          const std::uint64_t completed =
                                              progressState->completedRecords.load(std::memory_order_relaxed);
                                          const std::uint64_t total =
                                              progressState->totalRecords.load(std::memory_order_relaxed);
                                          target->xactionIndexProgressText = text;
                                          target->xactionIndexCompletedRecords = completed;
                                          target->xactionIndexTotalRecords = total;
                                          if (mainWindow->activeTraceSessionId_ == sessionId) {
                                              mainWindow->xactionIndexActive_ = target->xactionIndexActive;
                                              mainWindow->xactionIndexGeneration_ = target->xactionIndexGeneration;
                                              mainWindow->xactionIndexStopSource_ = target->xactionIndexStopSource;
                                              mainWindow->updateXactionIndexProgress(true,
                                                                                    text,
                                                                                    completed,
                                                                                    total);
                                          }
                                      },
                                      Qt::QueuedConnection);
        };

        CLogBTraceLoadCallbacks callbacks;
        callbacks.stage = [&](const CLogBTraceLoadStage stage,
                              const std::size_t workerCount,
                              const std::size_t totalStageRecords) {
            {
                const std::lock_guard lock(progressState->textMutex);
                switch (stage) {
                case CLogBTraceLoadStage::Decoding:
                    progressState->text = workerCount > 1
                        ? QStringLiteral("Xaction index: indexing (%1 workers)").arg(workerCount)
                        : QStringLiteral("Xaction index: indexing");
                    break;
                case CLogBTraceLoadStage::Formatting:
                    progressState->text = workerCount == 0
                        ? QStringLiteral("Xaction index: merging")
                        : QStringLiteral("Xaction index: collecting finalized rows");
                    break;
                case CLogBTraceLoadStage::Opening:
                    progressState->text = QStringLiteral("Xaction index: opening index file");
                    break;
                case CLogBTraceLoadStage::Finalizing:
                    progressState->text = QStringLiteral("Xaction index: finalizing");
                    break;
                case CLogBTraceLoadStage::FinalizingIndexDebug:
                    progressState->text = QStringLiteral("Xaction index: flushing debug data");
                    break;
                case CLogBTraceLoadStage::FinalizingIndexLayout:
                    progressState->text = QStringLiteral("Xaction index: finalizing row-list layout");
                    break;
                case CLogBTraceLoadStage::FinalizingIndexRows:
                    progressState->text = workerCount > 1
                        ? QStringLiteral("Xaction index: flushing streamed row-map chunks (%1 workers)").arg(workerCount)
                        : QStringLiteral("Xaction index: flushing streamed row-map chunks");
                    break;
                case CLogBTraceLoadStage::FinalizingIndexRowDirectory:
                    progressState->text = QStringLiteral("Xaction index: writing directories");
                    break;
                case CLogBTraceLoadStage::Completed:
                    progressState->text = QStringLiteral("Xaction index: complete");
                    break;
                case CLogBTraceLoadStage::Parsing:
                    progressState->text = QStringLiteral("Xaction index: loading");
                    break;
                }
            }
            progressState->totalRecords.store(static_cast<std::uint64_t>(totalStageRecords),
                                              std::memory_order_relaxed);
            progressState->completedRecords.store(
                stage == CLogBTraceLoadStage::Completed
                    ? static_cast<std::uint64_t>(totalStageRecords)
                    : 0,
                std::memory_order_relaxed);
            postProgress(true);
        };
        callbacks.stageProgress = [&](const std::size_t completedStageRecords,
                                      const std::size_t totalStageRecords) {
            progressState->completedRecords.store(static_cast<std::uint64_t>(completedStageRecords),
                                                  std::memory_order_relaxed);
            progressState->totalRecords.store(static_cast<std::uint64_t>(totalStageRecords),
                                              std::memory_order_relaxed);
            postProgress();
        };

        try {
            succeeded = session->buildXactionIndex(error, callbacks, stopSource->get_token());
        } catch (const std::bad_alloc&) {
            succeeded = false;
            error.type = CLogBTraceLoadError::Type::Generic;
            error.summary = QStringLiteral("Xaction indexing ran out of memory.");
            error.informativeText =
                QStringLiteral("CloganGUI could not allocate enough memory to build the Xaction index.");
        } catch (const std::exception& exception) {
            succeeded = false;
            error.type = CLogBTraceLoadError::Type::Generic;
            error.summary = QStringLiteral("Xaction indexing failed unexpectedly.");
            error.informativeText = QString::fromLocal8Bit(exception.what());
        } catch (...) {
            succeeded = false;
            error.type = CLogBTraceLoadError::Type::Generic;
            error.summary = QStringLiteral("Xaction indexing failed unexpectedly.");
            error.informativeText =
                QStringLiteral("A non-standard exception escaped the Xaction index builder.");
        }

        const bool cancelled = stopSource->get_token().stop_requested()
            || error.type == CLogBTraceLoadError::Type::Cancelled;

        if (window.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(window.data(),
                                  [window,
                                   session,
                                   sessionId,
                                   generation,
                                   succeeded,
                                   cancelled,
                                   error = std::move(error)]() mutable {
                                      if (window.isNull()) {
                                          return;
                                      }
                                      window->finishXactionIndexing(session,
                                                                    sessionId,
                                                                    generation,
                                                                    succeeded,
                                                                    cancelled,
                                                                    std::move(error));
                                  },
                                  Qt::QueuedConnection);
    });
    indexThread.detach();
}

void MainWindow::rebuildXactionIndexing()
{
    startXactionIndexing(true);
}

void MainWindow::clearXactionIndex()
{
    if (!currentTraceSession_ || !currentTraceSession_->supportsXactionIndexing()) {
        statusBar()->showMessage(QStringLiteral("Xaction indexing is not available for this trace."), 3000);
        updateXactionIndexAction();
        return;
    }

    const TraceViewSession* targetSession = activeTraceViewSession();
    if (targetSession && targetSession->xactionIndexActive) {
        statusBar()->showMessage(QStringLiteral("Cancel indexing before clearing the Xaction index."), 3000);
        return;
    }

    if (!currentTraceSession_->isXactionIndexComplete()) {
        statusBar()->showMessage(QStringLiteral("No Xaction index is available to clear."), 3000);
        updateXactionIndexAction();
        return;
    }

    currentTraceSession_->clearXactionIndex();
    if (flitModel_) {
        flitModel_->clearTransactionHighlightWithoutRefresh();
        scheduleVisibleXactionIndexRowsRefresh();
    }
    refreshLatencyView();
    if (TraceViewSession* session = activeTraceViewSession()) {
        refreshTransactionView(*session);
    }
    updateXactionIndexAction();
    scheduleDiagnosticsSnapshotRefresh();
    statusBar()->showMessage(QStringLiteral("Xaction index cleared."), 3000);
}

void MainWindow::cancelXactionIndexing()
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        cancelXactionIndexingForSession(*session, true);
    }
}

void MainWindow::cancelXactionIndexingForSession(TraceViewSession& session, const bool clearPartial)
{
    const bool wasActive = session.xactionIndexActive || session.xactionIndexStopSource;
    ++session.xactionIndexGeneration;
    session.xactionIndexActive = false;
    session.xactionIndexProgressText.clear();
    session.xactionIndexCompletedRecords = 0;
    session.xactionIndexTotalRecords = 0;
    if (session.xactionIndexStopSource) {
        session.xactionIndexStopSource->request_stop();
        session.xactionIndexStopSource.reset();
    }
    if (wasActive && clearPartial && session.traceSession) {
        session.traceSession->clearXactionIndex();
    }
    if (wasActive && session.flitModel) {
        session.flitModel->clearTransactionHighlightWithoutRefresh();
        if (session.id == activeTraceSessionId_) {
            scheduleVisibleXactionIndexRowsRefresh();
        }
    }

    if (session.id == activeTraceSessionId_) {
        xactionIndexGeneration_ = session.xactionIndexGeneration;
        xactionIndexActive_ = session.xactionIndexActive;
        xactionIndexStopSource_ = session.xactionIndexStopSource;
        refreshLatencyView();
        refreshTransactionView(session);
        updateXactionIndexProgress(false);
        updateXactionIndexAction();
    }

    if (wasActive) {
        scheduleDiagnosticsSnapshotRefresh();
    }
}

void MainWindow::refreshVisibleXactionIndexRows()
{
    if (!flitModel_) {
        return;
    }

    const int rows = flitModel_->rowCount();
    if (rows <= 0) {
        return;
    }

    int firstRow = 0;
    int lastRow = std::min(rows - 1, 255);
    if (flitView_ && flitView_->viewport()) {
        const int topRow = flitView_->rowAt(0);
        const int bottomRow = flitView_->rowAt(std::max(0, flitView_->viewport()->height() - 1));
        if (topRow >= 0) {
            firstRow = topRow;
            lastRow = bottomRow >= topRow ? bottomRow : std::min(rows - 1, topRow + 255);
        }
    }

    flitModel_->refreshTraceRowRange(firstRow, lastRow);
}

void MainWindow::scheduleVisibleXactionIndexRowsRefresh()
{
    QPointer<MainWindow> window(this);
    QTimer::singleShot(0, this, [window]() {
        if (window.isNull()) {
            return;
        }
        window->refreshVisibleXactionIndexRows();
    });
}

void MainWindow::finishXactionIndexing(std::shared_ptr<TraceSession> session,
                                       const quint64 sessionId,
                                       const quint64 generation,
                                       const bool succeeded,
                                       const bool cancelled,
                                       CLogBTraceLoadError error)
{
    TraceViewSession* targetSession = traceViewSessionById(sessionId);
    if (!targetSession
        || targetSession->traceSession != session
        || generation != targetSession->xactionIndexGeneration) {
        return;
    }

    targetSession->xactionIndexActive = false;
    targetSession->xactionIndexStopSource.reset();
    targetSession->xactionIndexProgressText.clear();
    targetSession->xactionIndexCompletedRecords = 0;
    targetSession->xactionIndexTotalRecords = 0;
    const bool targetIsActive = targetSession->id == activeTraceSessionId_;
    if (targetIsActive) {
        xactionIndexActive_ = false;
        xactionIndexStopSource_.reset();
        xactionIndexGeneration_ = targetSession->xactionIndexGeneration;
        updateXactionIndexProgress(false);
        updateXactionIndexAction();
    }
    if (!succeeded || cancelled) {
        session->clearXactionIndex();
        if (targetSession->flitModel) {
            targetSession->flitModel->clearTransactionHighlightWithoutRefresh();
            if (targetIsActive) {
                scheduleVisibleXactionIndexRowsRefresh();
            }
        }
        if (targetSession->latencyWidget) {
            targetSession->latencyWidget->setTraceSession(targetSession->traceSession);
        }
        refreshTransactionView(*targetSession);
        if (targetIsActive) {
            latencyWidget_ = targetSession->latencyWidget;
        }
        scheduleDiagnosticsSnapshotRefresh();
        if (targetIsActive) {
            updateXactionIndexAction();
        }

        if (cancelled) {
            if (targetIsActive) {
                statusBar()->showMessage(QStringLiteral("Xaction indexing cancelled."), 3000);
            }
            return;
        }

        if (error.summary.isEmpty()) {
            error.summary = QStringLiteral("Xaction indexing failed.");
        }
        if (targetIsActive) {
            statusBar()->showMessage(QStringLiteral("Xaction indexing failed."), 5000);
            showXactionIndexErrorDialog(
                this,
                targetSession->label.isEmpty() ? QDir::toNativeSeparators(session->filePath())
                                               : targetSession->label,
                error);
        }
        return;
    }

    session->refreshCachedXactionIndexRows();
    if (targetSession->flitModel) {
        targetSession->flitModel->clearTransactionHighlightWithoutRefresh();
        if (targetIsActive) {
            scheduleVisibleXactionIndexRowsRefresh();
        }
    }
    if (targetSession->latencyWidget) {
        targetSession->latencyWidget->setTraceSession(targetSession->traceSession);
    }
    refreshTransactionView(*targetSession);
    if (targetIsActive) {
        latencyWidget_ = targetSession->latencyWidget;
        transactionWidget_ = targetSession->transactionWidget;
        statusBar()->showMessage(QStringLiteral("Xaction index ready."), 3000);
        updateXactionIndexAction();
    }
    scheduleDiagnosticsSnapshotRefresh();
}

}  // namespace CHIron::Gui
