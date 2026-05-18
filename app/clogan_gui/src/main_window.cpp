#include "cache_widget.hpp"
#include "main_window_internal.hpp"

#include <cstdint>

namespace CHIron::Gui {
using namespace MainWindowDetail;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    flitModel_ = new FlitTableModel(this);
    detailModel_ = new FlitDetailsModel(this);
    diagnosticsSnapshotTimer_ = new QTimer(this);
    diagnosticsSnapshotTimer_->setSingleShot(true);
    diagnosticsSnapshotTimer_->setInterval(150);

    buildUi();
    wireSignals();
    connect(diagnosticsSnapshotTimer_, &QTimer::timeout, this, &MainWindow::refreshDiagnosticsSnapshot);
    resetNoSessionState();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmAllDirtySessions(QStringLiteral("closing CloganGUI"))) {
        event->ignore();
        return;
    }

    for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (!session) {
            continue;
        }
        cancelXactionIndexingForSession(*session, true);
        cancelStatisticsComputationForSession(*session);
    }
    if (dockManager_) {
        QSettings settings = MakeLayoutSettings();
        settings.setValue(QLatin1String(kGeometrySettingsKey), saveGeometry());
        settings.setValue(QLatin1String(kLayoutSettingsKey), dockManager_->saveState());
        settings.sync();
        dockManager_->deleteLater();
    }
    QMainWindow::closeEvent(event);
}

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
int MainWindow::testSessionCount() const noexcept
{
    return static_cast<int>(traceSessions_.size());
}

int MainWindow::testActiveSessionIndex() const noexcept
{
    for (std::size_t index = 0; index < traceSessions_.size(); ++index) {
        const TraceViewSession* session = traceSessions_[index].get();
        if (session && session->id == activeTraceSessionId_) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

QString MainWindow::testActiveSessionLabel() const
{
    const TraceViewSession* session = activeTraceViewSession();
    return session ? session->label : QString();
}

QStringList MainWindow::testSessionLabels() const
{
    QStringList labels;
    for (const std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (session) {
            labels.append(session->label);
        }
    }
    return labels;
}

int MainWindow::testVisibleTraceRowCount() const noexcept
{
    return flitModel_ ? flitModel_->visibleCount() : 0;
}

bool MainWindow::testActiveModelIsSessionBacked() const noexcept
{
    return flitModel_ && flitModel_->isSessionBacked();
}

int MainWindow::testActiveModelSourceRowsSnapshotSize() const
{
    return flitModel_ ? static_cast<int>(flitModel_->sourceRowsSnapshot().size()) : 0;
}

int MainWindow::testActiveTraceSessionCachedPageCount() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    return session && session->traceSession
        ? static_cast<int>(session->traceSession->cachedPageCount())
        : 0;
}

int MainWindow::testActiveTraceSessionCachedBlockCount() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    return session && session->traceSession
        ? static_cast<int>(session->traceSession->cachedBlockCount())
        : 0;
}

int MainWindow::testActiveTraceSessionMaxCachedPages() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    return session && session->traceSession
        ? static_cast<int>(session->traceSession->maxCachedPages())
        : 0;
}

int MainWindow::testActiveTraceSessionMaxCachedBlocks() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    return session && session->traceSession
        ? static_cast<int>(session->traceSession->maxCachedBlocks())
        : 0;
}

bool MainWindow::testApplyTraceRows(std::vector<FlitRecord> rows,
                                    const QString& sourceLabel,
                                    const QString& sourcePath)
{
    applyTraceRows(std::move(rows), sourceLabel, sourcePath);
    return activeTraceViewSession() != nullptr;
}

bool MainWindow::testLoadTraceFile(const QString& filePath)
{
    return loadTraceFile(filePath, std::nullopt, 0, false);
}

bool MainWindow::testReloadActiveTrace()
{
    return reloadActiveTrace(false);
}

bool MainWindow::testSwitchToSession(const int index)
{
    return activateTraceSessionAt(index);
}

bool MainWindow::testCloseActiveSession()
{
    if (!activeTraceViewSession()) {
        return false;
    }
    closeActiveTraceSession();
    return true;
}

bool MainWindow::testCloseSession(const int index)
{
    if (index < 0 || index >= static_cast<int>(traceSessions_.size()) || !traceSessions_[static_cast<std::size_t>(index)]) {
        return false;
    }
    closeTraceSession(traceSessions_[static_cast<std::size_t>(index)]->id);
    return true;
}

bool MainWindow::testCloseOtherSessions(const int index)
{
    if (index < 0 || index >= static_cast<int>(traceSessions_.size()) || !traceSessions_[static_cast<std::size_t>(index)]) {
        return false;
    }
    closeOtherTraceSessions(traceSessions_[static_cast<std::size_t>(index)]->id);
    return true;
}

void MainWindow::testCloseAllSessions()
{
    closeAllTraceSessions();
}

void MainWindow::testStartStatisticsComputation()
{
    startStatisticsComputation();
}

bool MainWindow::testSessionStatisticsActive(const int index) const noexcept
{
    if (index < 0 || index >= static_cast<int>(traceSessions_.size())) {
        return false;
    }
    const TraceViewSession* session = traceSessions_[static_cast<std::size_t>(index)].get();
    return session && session->statisticsActive;
}

bool MainWindow::testSessionStatisticsComputed(const int index) const noexcept
{
    if (index < 0 || index >= static_cast<int>(traceSessions_.size())) {
        return false;
    }
    const TraceViewSession* session = traceSessions_[static_cast<std::size_t>(index)].get();
    return session && session->statisticsComputed;
}

bool MainWindow::testActiveSessionStatisticsComputed() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    return session && session->statisticsComputed;
}

void MainWindow::testSetOpcodeFilter(const QString& text)
{
    if (opcodeFilterEdit_) {
        opcodeFilterEdit_->setText(text);
    }
    if (flitModel_) {
        flitModel_->setOpcodeFilter(text);
    }
}

QString MainWindow::testOpcodeFilter() const
{
    return flitModel_ ? flitModel_->opcodeFilter() : QString();
}

void MainWindow::testSetFieldColumnVisible(const QString& fieldName, const bool visible)
{
    if (flitModel_) {
        flitModel_->setFieldColumnVisible(fieldName, visible);
        rebuildOptionalFieldSearchRow();
        refreshNodeLabelDelegates();
    }
}

bool MainWindow::testFieldColumnVisible(const QString& fieldName) const
{
    return flitModel_ && flitModel_->isFieldColumnVisible(fieldName);
}

void MainWindow::testSetFieldFilter(const QString& fieldName, const QString& text)
{
    if (flitModel_) {
        flitModel_->setFieldFilter(fieldName, text);
    }
    if (const auto found = optionalFieldFilterEdits_.constFind(fieldName);
        found != optionalFieldFilterEdits_.cend() && found.value()) {
        const QSignalBlocker blocker(found.value());
        found.value()->setText(text);
    }
}

QString MainWindow::testFieldFilter(const QString& fieldName) const
{
    return flitModel_ ? flitModel_->fieldFilter(fieldName) : QString();
}

int MainWindow::testFieldColumn(const QString& fieldName) const noexcept
{
    return flitModel_ ? flitModel_->columnForField(fieldName) : -1;
}

void MainWindow::testSetSearchMode(const FlitTableModel::SearchMode mode)
{
    if (flitModel_) {
        flitModel_->setSearchMode(mode);
    }
    if (searchModeButton_) {
        const QSignalBlocker blocker(searchModeButton_);
        const bool highlight = mode == FlitTableModel::SearchMode::Highlight;
        searchModeButton_->setChecked(highlight);
        searchModeButton_->setText(highlight ? QStringLiteral("Highlight") : QStringLiteral("Filter"));
    }
}

FlitTableModel::SearchMode MainWindow::testSearchMode() const noexcept
{
    return flitModel_ ? flitModel_->searchMode() : FlitTableModel::SearchMode::Filter;
}

void MainWindow::testSetTraceToolbarFolded(const bool folded)
{
    setTraceToolbarFolded(folded);
}

bool MainWindow::testTraceToolbarFolded() const noexcept
{
    return traceToolbarFolded_;
}

bool MainWindow::testTraceToolbarFoldedControlsHidden() const noexcept
{
    for (QWidget* row : traceToolbarFoldableRows_) {
        if (row && row->isVisible()) {
            return false;
        }
    }
    for (QWidget* widget : traceToolbarFoldableSearchWidgets_) {
        if (widget && widget->isVisible()) {
            return false;
        }
    }
    return true;
}

bool MainWindow::testTraceToolbarSummaryVisible() const noexcept
{
    return totalBadge_ && totalBadge_->isVisibleTo(traceToolbar_)
        && visibleBadge_ && visibleBadge_->isVisibleTo(traceToolbar_)
        && reqBadge_ && reqBadge_->isVisibleTo(traceToolbar_)
        && rspBadge_ && rspBadge_->isVisibleTo(traceToolbar_)
        && datBadge_ && datBadge_->isVisibleTo(traceToolbar_)
        && snpBadge_ && snpBadge_->isVisibleTo(traceToolbar_);
}

int MainWindow::testTraceToolbarHeight() const noexcept
{
    return traceToolbar_ ? traceToolbar_->height() : 0;
}

QString MainWindow::testTraceToolbarFoldButtonText() const
{
    return traceToolbarFoldButton_ ? traceToolbarFoldButton_->text() : QString();
}

QVariantMap MainWindow::testTraceToolbarFoldButtonGeometry() const
{
    QVariantMap geometry;
    if (!traceToolbar_ || !traceToolbarFoldButton_) {
        return geometry;
    }

    const QPoint origin = traceToolbarFoldButton_->mapTo(traceToolbar_, QPoint(0, 0));
    const QRect rect(origin, traceToolbarFoldButton_->size());
    geometry.insert(QStringLiteral("x"), rect.x());
    geometry.insert(QStringLiteral("y"), rect.y());
    geometry.insert(QStringLiteral("right"), rect.right());
    geometry.insert(QStringLiteral("bottom"), rect.bottom());
    geometry.insert(QStringLiteral("width"), rect.width());
    geometry.insert(QStringLiteral("height"), rect.height());
    geometry.insert(QStringLiteral("toolbarWidth"), traceToolbar_->width());
    geometry.insert(QStringLiteral("toolbarHeight"), traceToolbar_->height());
    return geometry;
}

void MainWindow::testSetDisplayMode(const int column, const FlitTableModel::DisplayMode mode)
{
    if (flitModel_) {
        flitModel_->setDisplayMode(column, mode);
    }
}

FlitTableModel::DisplayMode MainWindow::testDisplayMode(const int column) const noexcept
{
    return flitModel_ ? flitModel_->displayMode(column) : FlitTableModel::DisplayMode::Decoded;
}

void MainWindow::testSelectLogicalRow(const int logicalRow)
{
    if (!flitView_ || !flitModel_ || !flitView_->selectionModel()) {
        return;
    }
    const int visibleRow = flitModel_->visibleRowForLogicalRow(logicalRow);
    if (visibleRow < 0) {
        return;
    }
    const QModelIndex target = flitModel_->index(visibleRow, 0);
    flitView_->selectionModel()->setCurrentIndex(target,
                                                 QItemSelectionModel::ClearAndSelect
                                                     | QItemSelectionModel::Rows);
    flitView_->selectRow(visibleRow);
    updateInspector(flitModel_->recordAt(visibleRow));
    updateTimelineSelection();
    updateAddressSelection();
    updateCacheSelection();
}

int MainWindow::testSelectedLogicalRow() const noexcept
{
    if (!flitView_ || !flitModel_ || !flitView_->selectionModel()) {
        return -1;
    }
    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    return current.isValid() ? flitModel_->logicalRowAt(current.row()) : -1;
}

void MainWindow::testSetTableColumnWidth(const int column, const int width)
{
    if (flitView_ && flitModel_ && column >= 0 && column < flitModel_->columnCount()) {
        flitView_->setColumnWidth(column, width);
    }
}

int MainWindow::testTableColumnWidth(const int column) const noexcept
{
    if (!flitView_ || !flitModel_ || column < 0 || column >= flitModel_->columnCount()) {
        return -1;
    }
    return flitView_->columnWidth(column);
}

void MainWindow::testSetTableScrollValue(const int value)
{
    if (flitView_ && flitView_->verticalScrollBar()) {
        flitView_->verticalScrollBar()->setValue(value);
    }
}

int MainWindow::testTableScrollValue() const noexcept
{
    return flitView_ && flitView_->verticalScrollBar()
        ? flitView_->verticalScrollBar()->value()
        : -1;
}

void MainWindow::testSelectTopologyNode(const int nodeId)
{
    refreshTopologyView();
    if (!topologyTable_ || nodeId < 0) {
        return;
    }
    for (int row = 0; row < topologyTable_->rowCount(); ++row) {
        const QTableWidgetItem* item = topologyTable_->item(row, 0);
        if (!item) {
            continue;
        }
        bool ok = false;
        const int currentNodeId = item->text().toInt(&ok);
        if (ok && currentNodeId == nodeId) {
            topologyTable_->setCurrentCell(row, 0);
            topologyTable_->selectRow(row);
            if (TraceViewSession* session = activeTraceViewSession()) {
                session->topologySelectedNodeId = static_cast<quint16>(nodeId);
            }
            return;
        }
    }
}

int MainWindow::testSelectedTopologyNode() const noexcept
{
    if (!topologyTable_) {
        return -1;
    }
    const int row = topologyTable_->currentRow();
    if (row < 0) {
        return -1;
    }
    const QTableWidgetItem* item = topologyTable_->item(row, 0);
    if (!item) {
        return -1;
    }
    bool ok = false;
    const int nodeId = item->text().toInt(&ok);
    return ok ? nodeId : -1;
}

QVariantMap MainWindow::testTimelineViewState() const
{
    const TraceViewSession* session = activeTraceViewSession();
    TimelineWidget* widget = session && session->timelineWidget ? session->timelineWidget : timelineWidget_;
    return widget ? widget->viewState() : QVariantMap();
}

QVariantMap MainWindow::testAddressViewState() const
{
    const TraceViewSession* session = activeTraceViewSession();
    AddressWidget* widget = session && session->addressWidget ? session->addressWidget : addressWidget_;
    return widget ? widget->viewState() : QVariantMap();
}

QVariantMap MainWindow::testCacheViewState() const
{
    const TraceViewSession* session = activeTraceViewSession();
    CacheWidget* widget = session && session->cacheWidget ? session->cacheWidget : cacheWidget_;
    return widget ? widget->viewState() : QVariantMap();
}

QVariantMap MainWindow::testLatencyViewState() const
{
    const TraceViewSession* session = activeTraceViewSession();
    LatencyWidget* widget = session && session->latencyWidget ? session->latencyWidget : latencyWidget_;
    return widget ? widget->viewState() : QVariantMap();
}

QVariantMap MainWindow::testTransactionViewState() const
{
    const TraceViewSession* session = activeTraceViewSession();
    TransactionWidget* widget =
        session && session->transactionWidget ? session->transactionWidget : transactionWidget_;
    return widget ? widget->viewState() : QVariantMap();
}

QString MainWindow::testDiagnosticsStateSnapshot() const
{
    return diagnosticsStateSnapshot();
}

void MainWindow::testRestoreTimelineViewState(const QVariantMap& state)
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->timelineWidget) {
            session->timelineWidget->restoreViewState(state);
            session->timelineViewState = session->timelineWidget->viewState();
        }
    }
}

void MainWindow::testRestoreAddressViewState(const QVariantMap& state)
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->addressWidget) {
            session->addressWidget->restoreViewState(state);
            session->addressViewState = session->addressWidget->viewState();
        }
    }
}

void MainWindow::testRestoreCacheViewState(const QVariantMap& state)
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->cacheWidget) {
            session->cacheWidget->restoreViewState(state);
            session->cacheViewState = session->cacheWidget->viewState();
        }
    }
}

void MainWindow::testRestoreLatencyViewState(const QVariantMap& state)
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->latencyWidget) {
            session->latencyWidget->restoreViewState(state);
            session->latencyViewState = session->latencyWidget->viewState();
        }
    }
}

void MainWindow::testRestoreTransactionViewState(const QVariantMap& state)
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->transactionWidget) {
            session->transactionWidget->restoreViewState(state);
            session->transactionViewState = session->transactionWidget->viewState();
        }
    }
}

void MainWindow::testBuildVisualizationViews()
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->timelineWidget) {
            session->timelineWidget->buildView();
        }
        if (session->addressWidget) {
            session->addressWidget->buildView();
        }
        if (session->cacheWidget) {
            session->cacheWidget->buildView();
        }
        if (session->latencyWidget) {
            session->latencyWidget->buildView();
        }
        if (session->transactionWidget) {
            session->transactionWidget->buildView();
        }
    }
}

QString MainWindow::testTimelineVisibleRangeText() const
{
    const TraceViewSession* session = activeTraceViewSession();
    TimelineWidget* widget = session && session->timelineWidget ? session->timelineWidget : timelineWidget_;
    return widget ? widget->testVisibleRangeText() : QString();
}

QString MainWindow::testAddressVisibleRangeText() const
{
    const TraceViewSession* session = activeTraceViewSession();
    AddressWidget* widget = session && session->addressWidget ? session->addressWidget : addressWidget_;
    return widget ? widget->testVisibleRangeText() : QString();
}

QString MainWindow::testCacheVisibleRangeText() const
{
    const TraceViewSession* session = activeTraceViewSession();
    CacheWidget* widget = session && session->cacheWidget ? session->cacheWidget : cacheWidget_;
    if (!widget) {
        return QString();
    }
    const std::pair<qint64, qint64> range = widget->testVisibleTimestampRange();
    return QStringLiteral("%1..%2").arg(range.first).arg(range.second);
}

QString MainWindow::testTransactionVisibleRangeText() const
{
    const TraceViewSession* session = activeTraceViewSession();
    TransactionWidget* widget =
        session && session->transactionWidget ? session->transactionWidget : transactionWidget_;
    if (!widget) {
        return QString();
    }

    const std::pair<qint64, qint64> range = widget->testVisibleTimestampRange();
    return QStringLiteral("%1..%2").arg(range.first).arg(range.second);
}

int MainWindow::testTimelineWidgetInstanceId() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const TimelineWidget* widget = session && session->timelineWidget ? session->timelineWidget : timelineWidget_;
    return static_cast<int>(reinterpret_cast<std::uintptr_t>(widget) & 0x7FFFFFFF);
}

int MainWindow::testAddressWidgetInstanceId() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const AddressWidget* widget = session && session->addressWidget ? session->addressWidget : addressWidget_;
    return static_cast<int>(reinterpret_cast<std::uintptr_t>(widget) & 0x7FFFFFFF);
}

int MainWindow::testCacheWidgetInstanceId() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const CacheWidget* widget = session && session->cacheWidget ? session->cacheWidget : cacheWidget_;
    return static_cast<int>(reinterpret_cast<std::uintptr_t>(widget) & 0x7FFFFFFF);
}

int MainWindow::testTransactionWidgetInstanceId() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const TransactionWidget* widget =
        session && session->transactionWidget ? session->transactionWidget : transactionWidget_;
    return static_cast<int>(reinterpret_cast<std::uintptr_t>(widget) & 0x7FFFFFFF);
}

int MainWindow::testCacheSpanCount() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const CacheWidget* widget = session && session->cacheWidget ? session->cacheWidget : cacheWidget_;
    return widget ? widget->testSpanCount() : 0;
}

int MainWindow::testCacheBandCount() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const CacheWidget* widget = session && session->cacheWidget ? session->cacheWidget : cacheWidget_;
    return widget ? widget->testBandCount() : 0;
}

quint64 MainWindow::testCacheBuildGeneration() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const CacheWidget* widget = session && session->cacheWidget ? session->cacheWidget : cacheWidget_;
    return widget ? widget->testBuildGeneration() : 0;
}

void MainWindow::testInjectCacheSpans(const int bandCount, const int spanCount)
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->cacheWidget) {
            session->cacheWidget->testInjectSyntheticSpans(bandCount, spanCount);
            session->cacheViewState = session->cacheWidget->viewState();
            cacheWidget_ = session->cacheWidget;
            if (cacheStack_) {
                cacheStack_->setCurrentWidget(cacheWidget_);
            }
        }
    }
}

int MainWindow::testTransactionSpanCount() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const TransactionWidget* widget =
        session && session->transactionWidget ? session->transactionWidget : transactionWidget_;
    return widget ? widget->testSpanCount() : 0;
}

int MainWindow::testTransactionLaneCount() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const TransactionWidget* widget =
        session && session->transactionWidget ? session->transactionWidget : transactionWidget_;
    return widget ? widget->testLaneCount() : 0;
}

quint64 MainWindow::testTransactionBuildGeneration() const noexcept
{
    const TraceViewSession* session = activeTraceViewSession();
    const TransactionWidget* widget =
        session && session->transactionWidget ? session->transactionWidget : transactionWidget_;
    return widget ? widget->testBuildGeneration() : 0;
}

void MainWindow::testInjectTransactionSpans(const int laneCount, const int spanCount)
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->transactionWidget) {
            session->transactionWidget->testInjectSyntheticSpans(laneCount, spanCount);
            session->transactionViewState = session->transactionWidget->viewState();
            transactionWidget_ = session->transactionWidget;
            if (transactionStack_) {
                transactionStack_->setCurrentWidget(transactionWidget_);
            }
        }
    }
}

void MainWindow::testStartXactionIndexing(const bool rebuildExisting)
{
    startXactionIndexing(rebuildExisting);
}

bool MainWindow::testSessionXactionIndexActive(const int index) const noexcept
{
    if (index < 0 || index >= static_cast<int>(traceSessions_.size())) {
        return false;
    }
    const TraceViewSession* session = traceSessions_[static_cast<std::size_t>(index)].get();
    return session && session->xactionIndexActive;
}

void MainWindow::testSetClipboardScope(const ClipboardScope scope)
{
    if (clipboardWidget_) {
        clipboardWidget_->setScope(scope);
        bindClipboardWidgetToActiveScope();
    }
}

ClipboardScope MainWindow::testClipboardScope() const noexcept
{
    return clipboardWidget_ ? clipboardWidget_->scope() : ClipboardScope::Session;
}

bool MainWindow::testClipboardToolbarFolded() const noexcept
{
    return clipboardWidget_ && clipboardWidget_->toolbarFolded();
}

bool MainWindow::testClipboardReadOnly() const noexcept
{
    return clipboardWidget_ && clipboardWidget_->readOnly();
}

void MainWindow::testSetClipboardReadOnly(const bool readOnly)
{
    if (clipboardWidget_) {
        clipboardWidget_->setReadOnly(readOnly);
    }
}

bool MainWindow::testClipboardHasModifiedRows() const
{
    return clipboardWidget_ && clipboardWidget_->hasModifiedRows();
}

bool MainWindow::testClipboardEntryModified(const int visibleRow) const
{
    if (!clipboardWidget_ || !clipboardWidget_->model()) {
        return false;
    }
    const int logicalRow = clipboardWidget_->model()->logicalRowAt(visibleRow);
    return clipboardWidget_->entryModified(logicalRow);
}

int MainWindow::testClipboardRowCount() const noexcept
{
    return clipboardWidget_ && clipboardWidget_->model()
        ? clipboardWidget_->model()->totalCount()
        : 0;
}

int MainWindow::testClipboardVisibleRowCount() const noexcept
{
    return clipboardWidget_ && clipboardWidget_->model()
        ? clipboardWidget_->model()->visibleCount()
        : 0;
}

qint64 MainWindow::testClipboardTimestampAt(const int visibleRow) const noexcept
{
    if (!clipboardWidget_ || !clipboardWidget_->model()) {
        return 0;
    }
    const FlitRecord* record = clipboardWidget_->model()->recordAt(visibleRow);
    return record ? record->timestamp : -1;
}

QString MainWindow::testClipboardOpcodeAt(const int visibleRow) const
{
    if (!clipboardWidget_ || !clipboardWidget_->model()) {
        return {};
    }
    const FlitRecord* record = clipboardWidget_->model()->recordAt(visibleRow);
    return record ? record->opcode : QString();
}

bool MainWindow::testEditClipboardTimestampAt(const int visibleRow, const qint64 timestamp)
{
    if (!clipboardWidget_ || !clipboardWidget_->model()) {
        return false;
    }
    FlitTableModel* model = clipboardWidget_->model();
    if (visibleRow < 0 || visibleRow >= model->visibleCount()) {
        return false;
    }
    const QModelIndex target = model->index(visibleRow, FlitTableModel::TimeColumn);
    const bool edited = model->setData(target, QString::number(timestamp), Qt::EditRole);
    clipboardWidget_->syncEntriesFromModel();
    return edited;
}

void MainWindow::testActivateClipboardRow(const int visibleRow)
{
    if (!clipboardWidget_) {
        return;
    }
    const std::optional<ClipboardEntry> entry = clipboardWidget_->entryForVisibleRow(visibleRow);
    if (entry) {
        handleClipboardRowActivated(*entry);
    }
}

bool MainWindow::testInsertSelectedFlitToClipboard(const ClipboardScope scope)
{
    const int before = testClipboardScope() == scope
        ? testClipboardRowCount()
        : (clipboardEntriesForScope(scope) ? static_cast<int>(clipboardEntriesForScope(scope)->size()) : 0);
    insertSelectedFlitToClipboard(scope);
    const int after = testClipboardScope() == scope
        ? testClipboardRowCount()
        : (clipboardEntriesForScope(scope) ? static_cast<int>(clipboardEntriesForScope(scope)->size()) : 0);
    return after > before;
}

bool MainWindow::testDeleteClipboardRow(const int visibleRow)
{
    if (!clipboardWidget_ || !clipboardWidget_->tableView() || !clipboardWidget_->model()) {
        return false;
    }
    if (visibleRow < 0 || visibleRow >= clipboardWidget_->model()->visibleCount()) {
        return false;
    }
    QTableView* table = clipboardWidget_->tableView();
    const QModelIndex target = clipboardWidget_->model()->index(visibleRow, 0);
    table->selectionModel()->setCurrentIndex(target,
                                             QItemSelectionModel::ClearAndSelect
                                                 | QItemSelectionModel::Rows);
    table->selectRow(visibleRow);
    const int before = testClipboardRowCount();
    clipboardWidget_->deleteSelectedRows();
    return testClipboardRowCount() < before;
}

bool MainWindow::testSaveClipboardCsv(const QString& path)
{
    if (!clipboardWidget_) {
        return false;
    }
    clipboardWidget_->syncEntriesFromModel();
    return saveClipboardAsCsv(path, clipboardWidget_->currentEntriesSnapshot());
}

bool MainWindow::testSaveClipboardCLogB(const QString& path)
{
    if (!clipboardWidget_) {
        return false;
    }
    clipboardWidget_->syncEntriesFromModel();
    return saveClipboardAsCLogB(path, clipboardWidget_->currentEntriesSnapshot());
}

bool MainWindow::testClipboardCLogBSaveAvailable() const
{
    return clipboardWidget_
        && metadataForClipboardCLogBSave(clipboardWidget_->currentEntriesSnapshot()).has_value();
}

void MainWindow::testSetClipboardOpcodeFilter(const QString& text)
{
    if (clipboardWidget_) {
        clipboardWidget_->testSetOpcodeFilter(text);
    }
}

void MainWindow::testSetClipboardSearchMode(const FlitTableModel::SearchMode mode)
{
    if (clipboardWidget_) {
        clipboardWidget_->testSetSearchMode(mode);
    }
}

int MainWindow::testLatencyDiffSessionCount() const noexcept
{
    return latencyDiffWidget_ ? latencyDiffWidget_->testSessionSourceCount() : 0;
}

bool MainWindow::testBuildDefaultLatencyDiff()
{
    refreshLatencyDiffSessions();
    return latencyDiffWidget_ && latencyDiffWidget_->testBuildDefaultDiff();
}

bool MainWindow::testLatencyDiffHasReport() const noexcept
{
    return latencyDiffWidget_ && latencyDiffWidget_->hasReport();
}
#endif

}  // namespace CHIron::Gui

