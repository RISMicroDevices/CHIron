#include "main_window_internal.hpp"

namespace CHIron::Gui {
using namespace MainWindowDetail;

void MainWindow::showFieldToggleDialog()
{
    if (!fieldToggleDialog_ || !fieldToggleButton_) {
        return;
    }

    if (fieldToggleDialog_->isVisible()) {
        fieldToggleDialog_->hide();
        return;
    }

    rebuildFieldToggleDialog();
    const QPoint anchor = fieldToggleButton_->mapToGlobal(QPoint(0, fieldToggleButton_->height() + 4));
    fieldToggleDialog_->move(anchor);
    fieldToggleDialog_->show();
    fieldToggleDialog_->raise();
    fieldToggleDialog_->activateWindow();
}

void MainWindow::rebuildFieldToggleDialog()
{
    if (!fieldToggleContentLayout_) {
        return;
    }

    fieldToggleCheckboxes_.clear();
    fieldIndexButtons_.clear();
    while (fieldToggleContentLayout_->count() > 0) {
        QLayoutItem* item = fieldToggleContentLayout_->takeAt(0);
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QStringList fields = flitModel_->availableOptionalFields();
    if (fields.isEmpty()) {
        QLabel* emptyLabel = new QLabel(QStringLiteral("No additional CHI fields"), fieldToggleContent_);
        emptyLabel->setObjectName(QStringLiteral("secondaryLabel"));
        fieldToggleContentLayout_->addWidget(emptyLabel);
        fieldToggleContentLayout_->addStretch(1);
        if (fieldToggleClearButton_) {
            fieldToggleClearButton_->setEnabled(false);
        }
        return;
    }

    const auto groupAccent = [](const QString& scope) {
        if (scope == QLatin1String("REQ")) {
            return ChannelAccent(FlitChannel::Req);
        }
        if (scope == QLatin1String("RSP")) {
            return ChannelAccent(FlitChannel::Rsp);
        }
        if (scope == QLatin1String("DAT")) {
            return ChannelAccent(FlitChannel::Dat);
        }
        if (scope == QLatin1String("SNP")) {
            return ChannelAccent(FlitChannel::Snp);
        }
        return QColor(QStringLiteral("#D9D9D9"));
    };

    const QStringList scopeOrder = {
        QStringLiteral("REQ"),
        QStringLiteral("RSP"),
        QStringLiteral("DAT"),
        QStringLiteral("SNP"),
    };

    const int maxColumns = 8;
    const int targetDialogHeight = 720;
    const QFontMetrics checkboxMetrics(fieldToggleContent_->font());
    int widestFieldText = 0;
    int maxGroupFieldCount = 0;
    int visibleGroupCount = 0;

    QVector<QPair<QString, QStringList>> groups;
    groups.reserve(scopeOrder.size());
    for (const QString& scope : scopeOrder) {
        const QStringList scopeFields = flitModel_->availableOptionalFieldsForScope(scope);
        if (scopeFields.isEmpty()) {
            continue;
        }

        groups.push_back({scope, scopeFields});
        ++visibleGroupCount;
        maxGroupFieldCount = qMax(maxGroupFieldCount, scopeFields.size());
        for (const QString& fieldName : scopeFields) {
            widestFieldText = qMax(widestFieldText, checkboxMetrics.horizontalAdvance(fieldName));
        }
    }

    const int columnWidth = qBound(180, widestFieldText + 108, 300);
    int sharedColumnCount = qMin(maxColumns, qMax(2, maxGroupFieldCount));
    for (int candidateColumns = 2; candidateColumns <= qMin(maxColumns, qMax(2, maxGroupFieldCount)); ++candidateColumns) {
        int totalRows = 0;
        for (const auto& group : groups) {
            totalRows += (group.second.size() + candidateColumns - 1) / candidateColumns;
        }

        const int estimatedHeight = 130 + visibleGroupCount * 34 + totalRows * 26;
        if (estimatedHeight <= targetDialogHeight) {
            sharedColumnCount = candidateColumns;
            break;
        }
    }

    const int dialogWidth = qBound(560, 190 + sharedColumnCount * columnWidth, 1900);
    int totalRows = 0;
    for (const auto& group : groups) {
        totalRows += (group.second.size() + sharedColumnCount - 1) / sharedColumnCount;
    }
    const int dialogHeight = qBound(340, 135 + visibleGroupCount * 34 + totalRows * 26, 980);

    fieldToggleDialog_->resize(dialogWidth, dialogHeight);

    if (groups.isEmpty()) {
        if (fieldToggleClearButton_) {
            fieldToggleClearButton_->setEnabled(false);
        }
        return;
    }

    fieldToggleScrollArea_->setMinimumWidth(dialogWidth - 28);

    for (const auto& group : groups) {
        const QString& scope = group.first;
        const QStringList& scopeFields = group.second;

        const int rowCount = (scopeFields.size() + sharedColumnCount - 1) / sharedColumnCount;

        const QColor accent = groupAccent(scope);

        QWidget* groupFrame = new QWidget(fieldToggleContent_);

        QVBoxLayout* groupLayout = new QVBoxLayout(groupFrame);
        groupLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->setSpacing(4);

        QLabel* title = new QLabel(scope, groupFrame);
        title->setStyleSheet(QStringLiteral("color:%1; font-weight:700;").arg(accent.name()));
        groupLayout->addWidget(title);

        QWidget* gridHost = new QWidget(groupFrame);
        QGridLayout* gridLayout = new QGridLayout(gridHost);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->setHorizontalSpacing(16);
        gridLayout->setVerticalSpacing(4);

        for (int index = 0; index < scopeFields.size(); ++index) {
            const QString& fieldName = scopeFields[index];
            QWidget* cell = new QWidget(gridHost);
            QHBoxLayout* cellLayout = new QHBoxLayout(cell);
            cellLayout->setContentsMargins(0, 0, 0, 0);
            cellLayout->setSpacing(4);

            QCheckBox* checkbox = new QCheckBox(fieldName, cell);
            checkbox->setChecked(flitModel_->isFieldColumnVisible(fieldName));
            checkbox->setToolTip(QStringLiteral("Display CHI field %1 as an extra Flits column and enable its search input.")
                                     .arg(fieldName));
            connect(checkbox, &QCheckBox::toggled, this, [this, fieldName, checkbox](const bool checked) {
                const QList<QCheckBox*> peers = fieldToggleCheckboxes_.values(fieldName);
                for (QCheckBox* peer : peers) {
                    if (peer == checkbox) {
                        continue;
                    }
                    QSignalBlocker blocker(peer);
                    peer->setChecked(checked);
                }

                flitModel_->setFieldColumnVisible(fieldName, checked);
                rebuildOptionalFieldSearchRow();
                refreshNodeLabelDelegates();
                if (checked) {
                    const int column = flitModel_->columnForField(fieldName);
                    if (column >= 0) {
                        resizeFlitColumnForCurrentTrace(column);
                    }
                }
                scheduleDiagnosticsSnapshotRefresh();
            });
            fieldToggleCheckboxes_.insert(fieldName, checkbox);

            QToolButton* indexButton = new QToolButton(cell);
            indexButton->setObjectName(QStringLiteral("filterToggle"));
            indexButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
            indexButton->setFixedHeight(22);
            indexButton->setMinimumWidth(64);
            indexButton->setText(QStringLiteral("Index"));
            indexButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            QMenu* indexMenu = new QMenu(indexButton);
            QAction* reindexAction = indexMenu->addAction(QStringLiteral("Re-index"));
            reindexAction->setObjectName(QStringLiteral("reindexAction"));
            QAction* clearIndexAction = indexMenu->addAction(QStringLiteral("Clear"));
            clearIndexAction->setObjectName(QStringLiteral("clearIndexAction"));
            connect(reindexAction, &QAction::triggered, this, [this, fieldName]() {
                createOptionalFieldFastIndex(fieldName, true);
            });
            connect(clearIndexAction, &QAction::triggered, this, [this, fieldName]() {
                clearOptionalFieldFastIndex(fieldName);
            });
            indexButton->setMenu(indexMenu);
            indexButton->setPopupMode(QToolButton::DelayedPopup);
            connect(indexButton, &QToolButton::clicked, this, [this, fieldName]() {
                createOptionalFieldFastIndex(fieldName);
            });
            fieldIndexButtons_.insert(fieldName, indexButton);

            cellLayout->addWidget(checkbox, 1);
            cellLayout->addWidget(indexButton);
            gridLayout->addWidget(cell, index % rowCount, index / rowCount);
        }

        for (int column = 0; column < sharedColumnCount; ++column) {
            gridLayout->setColumnMinimumWidth(column, columnWidth);
            gridLayout->setColumnStretch(column, 1);
        }

        groupLayout->addWidget(gridHost);
        fieldToggleContentLayout_->addWidget(groupFrame);
    }

    fieldToggleContentLayout_->addStretch(1);

    if (fieldToggleClearButton_) {
        fieldToggleClearButton_->setEnabled(true);
    }
    refreshOptionalFieldIndexButtons();
}

void MainWindow::createOptionalFieldFastIndex(const QString& fieldName, const bool rebuildExisting)
{
    TraceViewSession* targetSession = activeTraceViewSession();
    if (fieldName.isEmpty() || !targetSession || !currentTraceSession_) {
        return;
    }

    if (targetSession->optionalFieldIndexingFields.contains(fieldName)) {
        statusBar()->showMessage(QStringLiteral("Fast index creation for %1 is already running.").arg(fieldName),
                                 3000);
        return;
    }

    const std::shared_ptr<TraceSession> session = currentTraceSession_;
    const quint64 sessionId = targetSession->id;
    const bool existingIndex = session->hasOptionalFieldFastIndex(fieldName);
    if (existingIndex && !rebuildExisting) {
        refreshOptionalFieldIndexButtons(fieldName);
        statusBar()->showMessage(QStringLiteral("Fast index for %1 is already available.").arg(fieldName), 3000);
        return;
    }

    if (!session->isFastIndexWritable()) {
        QMessageBox::warning(this,
                             QStringLiteral("Create Field Index"),
                             QStringLiteral("Fast index storage is not writable for this trace."));
        refreshOptionalFieldIndexButtons(fieldName);
        return;
    }

    targetSession->optionalFieldIndexingFields.insert(fieldName);
    fieldIndexingFields_ = targetSession->optionalFieldIndexingFields;
    refreshOptionalFieldIndexButtons(fieldName);

    QDialog progressDialog(this);
    progressDialog.setWindowTitle(rebuildExisting
                                      ? QStringLiteral("Re-index Field")
                                      : QStringLiteral("Create Field Index"));
    progressDialog.setWindowModality(Qt::ApplicationModal);
    progressDialog.resize(460, 150);

    QVBoxLayout* progressLayout = new QVBoxLayout(&progressDialog);
    progressLayout->setContentsMargins(14, 14, 14, 14);
    progressLayout->setSpacing(10);

    QLabel* statusLabel = new QLabel((rebuildExisting
                                          ? QStringLiteral("Re-indexing fast index for %1...")
                                          : QStringLiteral("Creating fast index for %1..."))
                                         .arg(fieldName),
                                     &progressDialog);
    statusLabel->setObjectName(QStringLiteral("sectionLabel"));
    progressLayout->addWidget(statusLabel);

    QLabel* detailLabel = new QLabel(QStringLiteral("Preparing trace blocks..."), &progressDialog);
    detailLabel->setObjectName(QStringLiteral("secondaryLabel"));
    detailLabel->setWordWrap(true);
    progressLayout->addWidget(detailLabel);

    QProgressBar* progressBar = new QProgressBar(&progressDialog);
    progressBar->setTextVisible(false);
    progressBar->setRange(0, 0);
    progressLayout->addWidget(progressBar);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, &progressDialog);
    QPushButton* cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    if (cancelButton) {
        cancelButton->setObjectName(QStringLiteral("actionButton"));
        cancelButton->setFixedHeight(22);
    }
    progressLayout->addWidget(buttonBox);

    struct OptionalFieldIndexOperation {
        std::stop_source stopSource;
        std::atomic_bool cancelRequested = false;
        std::atomic_bool indexFinished = false;
        std::atomic_bool resultHandled = false;
        std::atomic_size_t stageTotalBlocks = 0;
        std::atomic_size_t stageCompletedBlocks = 0;
        std::atomic_size_t activeWorkerCount = 0;
        std::atomic<CLogBTraceLoadStage> indexStage = CLogBTraceLoadStage::Formatting;
        std::mutex resultMutex;
        CLogBTraceLoadError error;
        bool succeeded = false;
    };

    auto operation = std::make_shared<OptionalFieldIndexOperation>();

    const auto handleIndexResult = [this, operation, session, sessionId, fieldName, rebuildExisting]() {
        bool expected = false;
        if (!operation->resultHandled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        CLogBTraceLoadError resultError;
        bool resultSucceeded = false;
        {
            const std::lock_guard lock(operation->resultMutex);
            resultError = operation->error;
            resultSucceeded = operation->succeeded;
        }

        TraceViewSession* target = traceViewSessionById(sessionId);
        if (!target || target->traceSession != session) {
            return;
        }

        target->optionalFieldIndexingFields.remove(fieldName);
        const bool targetIsActive = target->id == activeTraceSessionId_;
        if (targetIsActive) {
            fieldIndexingFields_ = target->optionalFieldIndexingFields;
            refreshOptionalFieldIndexButtons(fieldName);
        } else {
            scheduleDiagnosticsSnapshotRefresh();
            return;
        }

        if (!resultSucceeded) {
            if (resultError.type == CLogBTraceLoadError::Type::Cancelled
                || operation->cancelRequested.load(std::memory_order_relaxed)) {
                statusBar()->showMessage(
                    QStringLiteral("Cancelled fast index creation for %1 and deleted the partial index file.")
                        .arg(fieldName),
                    5000);
                scheduleDiagnosticsSnapshotRefresh();
                return;
            }

            const QString detail = resultError.informativeText.isEmpty()
                ? resultError.summary
                : resultError.summary + QStringLiteral("\n\n") + resultError.informativeText;
            QMessageBox::warning(this,
                                 rebuildExisting
                                     ? QStringLiteral("Re-index Field")
                                     : QStringLiteral("Create Field Index"),
                                 detail.isEmpty()
                                     ? QStringLiteral("Could not create the fast index.")
                                     : detail);
            statusBar()->showMessage((rebuildExisting
                                          ? QStringLiteral("Failed to re-index fast index for %1.")
                                          : QStringLiteral("Failed to create fast index for %1."))
                                         .arg(fieldName),
                                     5000);
            scheduleDiagnosticsSnapshotRefresh();
            return;
        }

        if (target->flitModel) {
            const QString activeFilter = target->flitModel->fieldFilter(fieldName);
            if (!activeFilter.isEmpty()) {
                target->flitModel->setFieldFilter(fieldName, activeFilter);
            }
        }
        statusBar()->showMessage((rebuildExisting
                                      ? QStringLiteral("Re-indexed fast index for %1.")
                                      : QStringLiteral("Created fast index for %1."))
                                     .arg(fieldName),
                                 5000);
        scheduleDiagnosticsSnapshotRefresh();
    };

    QEventLoop waitLoop;

    const auto requestCancel = [&]() {
        bool expected = false;
        if (!operation->cancelRequested.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
            return;
        }

        operation->stopSource.request_stop();
        detailLabel->setText(QStringLiteral("Cancelling index creation..."));
        if (cancelButton) {
            cancelButton->setEnabled(false);
        }
        waitLoop.quit();
    };
    connect(buttonBox, &QDialogButtonBox::rejected, &progressDialog, requestCancel);
    connect(&progressDialog, &QDialog::rejected, &progressDialog, requestCancel);

    const auto updateProgressWidgets = [&](const std::size_t completed,
                                           const std::size_t total,
                                           const std::size_t workerCount,
                                           const CLogBTraceLoadStage stage) {
        if (total == 0) {
            progressBar->setRange(0, operation->indexFinished.load(std::memory_order_acquire) ? 1 : 0);
            progressBar->setValue(operation->indexFinished.load(std::memory_order_acquire) ? 1 : 0);
        } else if (total <= static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            progressBar->setRange(0, static_cast<int>(total));
            progressBar->setValue(static_cast<int>(std::min(completed, total)));
        } else {
            progressBar->setRange(0, 1000);
            const double ratio = static_cast<double>(std::min(completed, total))
                / static_cast<double>(total);
            progressBar->setValue(static_cast<int>(ratio * 1000.0));
        }

        if (operation->cancelRequested.load(std::memory_order_relaxed)) {
            statusLabel->setText(QStringLiteral("Cancelling fast index for %1...").arg(fieldName));
        } else if (stage == CLogBTraceLoadStage::Completed) {
            statusLabel->setText(QStringLiteral("Finalizing fast index for %1...").arg(fieldName));
        } else {
            statusLabel->setText((rebuildExisting
                                      ? QStringLiteral("Re-indexing fast index for %1...")
                                      : QStringLiteral("Creating fast index for %1..."))
                                     .arg(fieldName));
        }

        const QString workerText = workerCount == 1
            ? QStringLiteral("1 worker")
            : QStringLiteral("%1 workers").arg(static_cast<qulonglong>(workerCount));
        detailLabel->setText(QStringLiteral("%1 of %2 records indexed with %3.")
                                 .arg(static_cast<qulonglong>(std::min(completed, total)))
                                 .arg(static_cast<qulonglong>(total))
                                 .arg(workerCount == 0 ? QStringLiteral("0 workers") : workerText));
    };

    CLogBTraceLoadCallbacks callbacks;
    callbacks.stage = [operation](const CLogBTraceLoadStage stage,
                                  const std::size_t workerCount,
                                  const std::size_t totalRecords) {
        operation->indexStage.store(stage, std::memory_order_relaxed);
        operation->activeWorkerCount.store(workerCount, std::memory_order_relaxed);
        operation->stageTotalBlocks.store(totalRecords, std::memory_order_relaxed);
        if (stage != CLogBTraceLoadStage::Completed) {
            operation->stageCompletedBlocks.store(0, std::memory_order_relaxed);
        }
    };
    callbacks.stageProgress = [operation](const std::size_t completedRecords,
                                          const std::size_t totalRecords) {
        operation->stageCompletedBlocks.store(completedRecords, std::memory_order_relaxed);
        operation->stageTotalBlocks.store(totalRecords, std::memory_order_relaxed);
    };

    progressDialog.show();
    progressDialog.raise();

    QPointer<MainWindow> window(this);
    std::thread indexThread([operation, session, sessionId, fieldName, callbacks, window]() mutable {
        CLogBTraceLoadError threadError;
        bool threadSucceeded = false;
        try {
            threadSucceeded = session->buildOptionalFieldFastIndex(fieldName,
                                                                   threadError,
                                                                   callbacks,
                                                                   operation->stopSource.get_token());
        } catch (const std::bad_alloc&) {
            threadError.summary = QStringLiteral("Creating the optional field fast index ran out of memory.");
        } catch (const std::exception& exception) {
            threadError.summary = QStringLiteral("Creating the optional field fast index failed unexpectedly.");
            threadError.informativeText = QString::fromLocal8Bit(exception.what());
        } catch (...) {
            threadError.summary = QStringLiteral("Creating the optional field fast index failed unexpectedly.");
            threadError.informativeText =
                QStringLiteral("A non-standard exception escaped the optional field index builder.");
        }

        {
            const std::lock_guard lock(operation->resultMutex);
            operation->error = std::move(threadError);
            operation->succeeded = threadSucceeded;
        }
        operation->indexFinished.store(true, std::memory_order_release);

        if (operation->cancelRequested.load(std::memory_order_relaxed) && !window.isNull()) {
            QMetaObject::invokeMethod(window.data(),
                                      [operation, session, sessionId, fieldName, window]() {
                                          if (window.isNull()) {
                                              return;
                                          }

                                          MainWindow* mainWindow = window.data();
                                          bool expected = false;
                                          if (!operation->resultHandled.compare_exchange_strong(
                                                  expected,
                                                  true,
                                                  std::memory_order_acq_rel)) {
                                              return;
                                          }

                                          CLogBTraceLoadError resultError;
                                          bool resultSucceeded = false;
                                          {
                                              const std::lock_guard lock(operation->resultMutex);
                                              resultError = operation->error;
                                              resultSucceeded = operation->succeeded;
                                          }

                                          auto* target = mainWindow->traceViewSessionById(sessionId);
                                          if (!target || target->traceSession != session) {
                                              return;
                                          }

                                          target->optionalFieldIndexingFields.remove(fieldName);
                                          const bool targetIsActive =
                                              target->id == mainWindow->activeTraceSessionId_;
                                          if (targetIsActive) {
                                              mainWindow->fieldIndexingFields_ =
                                                  target->optionalFieldIndexingFields;
                                              mainWindow->refreshOptionalFieldIndexButtons(fieldName);
                                          }
                                          if (!resultSucceeded
                                              && (resultError.type == CLogBTraceLoadError::Type::Cancelled
                                                  || operation->cancelRequested.load(std::memory_order_relaxed))) {
                                              if (targetIsActive) {
                                                  mainWindow->statusBar()->showMessage(
                                                      QStringLiteral("Cancelled fast index creation for %1 and deleted the partial index file.")
                                                          .arg(fieldName),
                                                      5000);
                                              }
                                          }
                                          mainWindow->scheduleDiagnosticsSnapshotRefresh();
                                      },
                                      Qt::QueuedConnection);
        }
    });

    QTimer waitTimer(&progressDialog);
    waitTimer.setInterval(100);
    connect(&waitTimer, &QTimer::timeout, &progressDialog, [&]() {
        if (!progressDialog.isVisible()
            && !operation->indexFinished.load(std::memory_order_acquire)
            && !operation->cancelRequested.load(std::memory_order_relaxed)) {
            progressDialog.show();
            progressDialog.raise();
        }

        updateProgressWidgets(operation->stageCompletedBlocks.load(std::memory_order_relaxed),
                              operation->stageTotalBlocks.load(std::memory_order_relaxed),
                              operation->activeWorkerCount.load(std::memory_order_relaxed),
                              operation->indexStage.load(std::memory_order_relaxed));
        if (operation->indexFinished.load(std::memory_order_acquire)
            || operation->cancelRequested.load(std::memory_order_relaxed)) {
            waitLoop.quit();
        }
    });

    waitTimer.start();
    waitLoop.exec();
    waitTimer.stop();

    const bool finishedBeforeReturn = operation->indexFinished.load(std::memory_order_acquire);
    if (finishedBeforeReturn && indexThread.joinable()) {
        indexThread.join();
    } else if (indexThread.joinable()) {
        indexThread.detach();
    }

    progressDialog.hide();

    if (finishedBeforeReturn) {
        handleIndexResult();
        return;
    }

    statusBar()->showMessage(
        QStringLiteral("Cancelled fast index creation for %1. Deleting the partial index file...")
            .arg(fieldName),
        5000);
    refreshOptionalFieldIndexButtons(fieldName);
}

void MainWindow::clearOptionalFieldFastIndex(const QString& fieldName)
{
    TraceViewSession* targetSession = activeTraceViewSession();
    if (fieldName.isEmpty() || !targetSession || !currentTraceSession_) {
        return;
    }

    if (targetSession->optionalFieldIndexingFields.contains(fieldName)) {
        statusBar()->showMessage(QStringLiteral("Fast index creation for %1 is already running.").arg(fieldName),
                                 3000);
        return;
    }

    CLogBTraceLoadError error;
    if (!currentTraceSession_->clearOptionalFieldFastIndex(fieldName, error)) {
        const QString detail = error.informativeText.isEmpty()
            ? error.summary
            : error.summary + QStringLiteral("\n\n") + error.informativeText;
        QMessageBox::warning(this,
                             QStringLiteral("Clear Field Index"),
                             detail.isEmpty()
                                 ? QStringLiteral("Could not clear the fast index.")
                                 : detail);
        refreshOptionalFieldIndexButtons(fieldName);
        return;
    }

    const QString activeFilter = targetSession->flitModel
        ? targetSession->flitModel->fieldFilter(fieldName)
        : QString();
    if (!activeFilter.isEmpty() && targetSession->flitModel) {
        targetSession->flitModel->setFieldFilter(fieldName, activeFilter);
    }
    refreshOptionalFieldIndexButtons(fieldName);
    statusBar()->showMessage(QStringLiteral("Cleared fast index for %1.").arg(fieldName), 5000);
    scheduleDiagnosticsSnapshotRefresh();
}

void MainWindow::refreshOptionalFieldIndexButtons(const QString& fieldName)
{
    const auto updateButton = [this](const QString& name, QToolButton* button) {
        if (!button) {
            return;
        }

        const bool hasSession = currentTraceSession_ != nullptr;
        const bool indexing = fieldIndexingFields_.contains(name);
        const bool writable = hasSession && currentTraceSession_->isFastIndexWritable();
        const bool indexed = hasSession && !indexing && currentTraceSession_->hasOptionalFieldFastIndex(name);
        button->setPopupMode(indexed ? QToolButton::InstantPopup : QToolButton::DelayedPopup);
        button->setText(indexing ? QStringLiteral("Indexing") : (indexed ? QStringLiteral("Indexed") : QStringLiteral("Index")));
        button->setEnabled(hasSession && !indexing && (indexed || writable));
        if (QMenu* menu = button->menu()) {
            for (QAction* action : menu->actions()) {
                if (action->objectName() == QLatin1String("reindexAction")) {
                    action->setEnabled(hasSession && indexed && writable && !indexing);
                } else if (action->objectName() == QLatin1String("clearIndexAction")) {
                    action->setEnabled(hasSession && indexed && !indexing);
                }
            }
        }
        if (!hasSession) {
            button->setToolTip(QStringLiteral("Open a trace before creating optional field indexes."));
        } else if (indexing) {
            button->setToolTip(QStringLiteral("Fast index creation for optional field %1 is running.").arg(name));
        } else if (indexed) {
            button->setToolTip(QStringLiteral("Fast index for optional field %1 is available. Click for clear and re-index options.")
                                   .arg(name));
        } else if (!writable) {
            button->setToolTip(QStringLiteral("Fast index storage is not writable for this trace."));
        } else {
            button->setToolTip(QStringLiteral("Create a fast index for optional field %1 to accelerate filtering.")
                                   .arg(name));
        }
    };

    if (fieldName.isEmpty()) {
        for (auto it = fieldIndexButtons_.cbegin(); it != fieldIndexButtons_.cend(); ++it) {
            updateButton(it.key(), it.value());
        }
        return;
    }

    const QList<QToolButton*> buttons = fieldIndexButtons_.values(fieldName);
    for (QToolButton* button : buttons) {
        updateButton(fieldName, button);
    }
}

void MainWindow::rebuildOptionalFieldSearchRow()
{
    if (!optionalFieldSearchRow_ || !optionalFieldSearchLayout_) {
        return;
    }

    rebuildingOptionalFieldSearchRow_ = true;
    optionalFieldFilterEdits_.clear();
    while (optionalFieldSearchLayout_->count() > 0) {
        QLayoutItem* item = optionalFieldSearchLayout_->takeAt(0);
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QStringList allFields = flitModel_->availableOptionalFields();
    QStringList visibleFields;
    for (const QString& fieldName : allFields) {
        if (flitModel_->isFieldColumnVisible(fieldName)) {
            visibleFields.append(fieldName);
        }
    }

    if (visibleFields.isEmpty()) {
        optionalFieldSearchRow_->hide();
        rebuildingOptionalFieldSearchRow_ = false;
        return;
    }

    const QFontMetrics metrics(optionalFieldSearchRow_->font());
    const int labelWidth = 50;
    const int rowSpacing = 5;
    const int rowWidth = optionalFieldSearchRow_->width() > 0
        ? optionalFieldSearchRow_->width()
        : flitView_->viewport()->width();
    const int availableWidth = qMax(240, rowWidth - labelWidth - 28);

    auto makeRow = [this, labelWidth](const bool firstRow) {
        QWidget* rowWidget = new QWidget(optionalFieldSearchRow_);
        QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(5);

        if (firstRow) {
            QLabel* sectionLabel = new QLabel(QStringLiteral("CHI"), rowWidget);
            sectionLabel->setObjectName(QStringLiteral("sectionLabel"));
            sectionLabel->setMinimumWidth(labelWidth);
            rowLayout->addWidget(sectionLabel);
        } else {
            QWidget* spacer = new QWidget(rowWidget);
            spacer->setFixedWidth(labelWidth);
            rowLayout->addWidget(spacer);
        }

        optionalFieldSearchLayout_->addWidget(rowWidget);
        return rowLayout;
    };

    QHBoxLayout* currentRowLayout = makeRow(true);
    int usedWidth = 0;
    bool firstItemInRow = true;

    for (const QString& fieldName : visibleFields) {
        QWidget* group = new QWidget(optionalFieldSearchRow_);
        QHBoxLayout* groupLayout = new QHBoxLayout(group);
        groupLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->setSpacing(4);

        QLabel* fieldLabel = new QLabel(fieldName, group);
        fieldLabel->setObjectName(QStringLiteral("secondaryLabel"));
        groupLayout->addWidget(fieldLabel);

        QLineEdit* edit = new QLineEdit(group);
        edit->setPlaceholderText(fieldName);
        edit->setClearButtonEnabled(true);
        edit->setFixedHeight(22);
        edit->setMinimumWidth(qBound(72, metrics.horizontalAdvance(fieldName) + 44, 150));
        edit->setMaximumWidth(edit->minimumWidth());
        edit->setText(flitModel_->fieldFilter(fieldName));
        edit->setToolTip(QStringLiteral("Filter rows by CHI field %1. Supports decoded text, decimal, or 0x-prefixed hex when numeric.")
                             .arg(fieldName));
        connect(edit, &QLineEdit::textChanged, this, [this, fieldName](const QString& text) {
            flitModel_->setFieldFilter(fieldName, text);
            scheduleDiagnosticsSnapshotRefresh();
        });
        groupLayout->addWidget(edit);
        optionalFieldFilterEdits_.insert(fieldName, edit);

        const int groupWidth = qMax(group->sizeHint().width(),
                                    metrics.horizontalAdvance(fieldName) + edit->minimumWidth() + 22);
        if (!firstItemInRow && usedWidth + rowSpacing + groupWidth > availableWidth) {
            currentRowLayout->addStretch(1);
            currentRowLayout = makeRow(false);
            usedWidth = 0;
            firstItemInRow = true;
        }

        currentRowLayout->addWidget(group);
        usedWidth += (firstItemInRow ? 0 : rowSpacing) + groupWidth;
        firstItemInRow = false;
    }

    currentRowLayout->addStretch(1);
    optionalFieldSearchRow_->setVisible(true);
    rebuildingOptionalFieldSearchRow_ = false;
}

}  // namespace CHIron::Gui
