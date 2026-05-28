#include "main_window_internal.hpp"

#include <DockWidget.h>
#include <DockWidgetTab.h>
#include <QLabel>
#include <QBoxLayout>

namespace CHIron::Gui {
using namespace MainWindowDetail;

void MainWindow::updateWindowCaption()
{
    if (currentTraceLabel_.isEmpty()) {
        setWindowTitle(QStringLiteral("CloganGUI"));
        return;
    }

    setWindowTitle(traceSessions_.size() > 1
                       ? QStringLiteral("CloganGUI - %1 (%2 sessions)")
                             .arg(currentTraceLabel_)
                             .arg(traceSessions_.size())
                       : QStringLiteral("CloganGUI - %1").arg(currentTraceLabel_));
}

void MainWindow::restoreDefaultLayout()
{
    if (dockManager_ && !defaultLayoutState_.isEmpty()) {
        dockManager_->restoreState(defaultLayoutState_);
        updateDerivedWidgetOutdatedTags();
        statusBar()->showMessage(QStringLiteral("Restored default layout."), 3000);
        scheduleDiagnosticsSnapshotRefresh();
    }
}

void MainWindow::selectFirstRow()
{
    if (flitModel_->rowCount() <= 0) {
        detailModel_->setRecord(nullptr);
        updateInspector(nullptr);
        return;
    }

    flitView_->selectRow(0);
    const QModelIndex first = flitModel_->index(0, 0);
    flitView_->selectionModel()->setCurrentIndex(first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void MainWindow::updateMetrics()
{
    totalBadge_->setText(QStringLiteral("All %1").arg(flitModel_->totalCount()));
    visibleBadge_->setText(QStringLiteral("Shown %1").arg(flitModel_->visibleCount()));
    reqBadge_->setText(QStringLiteral("REQ %1").arg(flitModel_->channelCountAll(FlitChannel::Req)));
    rspBadge_->setText(QStringLiteral("RSP %1").arg(flitModel_->channelCountAll(FlitChannel::Rsp)));
    datBadge_->setText(QStringLiteral("DAT %1").arg(flitModel_->channelCountAll(FlitChannel::Dat)));
    snpBadge_->setText(QStringLiteral("SNP %1").arg(flitModel_->channelCountAll(FlitChannel::Snp)));
    if (editStateBadge_) {
        const TraceViewSession* session = activeTraceViewSession();
        editStateBadge_->setVisible(session != nullptr);
        if (session) {
            editStateBadge_->setText(session->editModeEnabled
                                         ? (session->dirty
                                                ? QStringLiteral("Editable *")
                                                : QStringLiteral("Editable"))
                                         : QStringLiteral("Read-only"));
        }
    }
    if (outdatedViewsBadge_) {
        const TraceViewSession* session = activeTraceViewSession();
        outdatedViewsBadge_->setVisible(session && session->derivedViewsOutdated);
        outdatedViewsBadge_->setText(QStringLiteral("Outdated"));
    }
    updateEditActions();
}

void MainWindow::updateDerivedWidgetOutdatedTags()
{
    const TraceViewSession* session = activeTraceViewSession();
    const bool outdated = session && session->derivedViewsOutdated;

    const auto plainTitleFor = [outdated](const QString& baseTitle) {
        return outdated
            ? QStringLiteral("%1 [Outdated]").arg(baseTitle)
            : baseTitle;
    };

    const auto updateDockTitle = [&](ads::CDockWidget* dock, const QString& baseTitle) {
        if (!dock) {
            return;
        }
        const QString plainTitle = plainTitleFor(baseTitle);
        dock->setWindowTitle(plainTitle);
        if (ads::CDockWidgetTab* tab = dock->tabWidget()) {
            tab->setText(baseTitle);
            QLabel* marker = tab->findChild<QLabel*>(QStringLiteral("dockWidgetOutdatedMarker"));
            if (!marker && tab->layout()) {
                marker = new QLabel(QStringLiteral("[Outdated]"), tab);
                marker->setObjectName(QStringLiteral("dockWidgetOutdatedMarker"));
                marker->setStyleSheet(QStringLiteral("color: #B42318; font-weight: 700; padding-left: 4px;"));
                marker->setAlignment(Qt::AlignCenter);
                marker->setVisible(false);
                if (auto* boxLayout = qobject_cast<QBoxLayout*>(tab->layout())) {
                    const int insertIndex = std::max(1, boxLayout->count() - 3);
                    boxLayout->insertWidget(insertIndex, marker, 0);
                }
            }
            if (marker) {
                marker->setVisible(outdated);
            }
        }
    };

    updateDockTitle(timelineDock_, QStringLiteral("Timeline"));
    updateDockTitle(addressDock_, QStringLiteral("Address"));
    updateDockTitle(cacheDock_, QStringLiteral("Cache"));
    updateDockTitle(latencyDock_, QStringLiteral("Latency"));
    if (latencyDiffDock_) {
        latencyDiffDock_->setWindowTitle(QStringLiteral("Latency Diff"));
        if (ads::CDockWidgetTab* tab = latencyDiffDock_->tabWidget()) {
            tab->setText(QStringLiteral("Latency Diff"));
        }
    }
    updateDockTitle(transactionDock_, QStringLiteral("Transaction"));
    updateDockTitle(statisticsDock_, QStringLiteral("Statistics"));
}

void MainWindow::updateInspector(const FlitRecord* record)
{
    inspector_->setHtml(inspectorHtml(record));
    updateXactionRowsTable(record);
}

void MainWindow::updateXactionRowsTable(const FlitRecord* record)
{
    if (!xactionRowsCaption_ || !xactionRowsTable_) {
        return;
    }

    xactionRowsTable_->clearContents();
    xactionRowsTable_->setRowCount(0);

    QString xactionKey;
    if (record && record->xactionIndexChecked && record->xactionIndexed) {
        for (const QString& key : TransactionGroupKeys(*record)) {
            if (key.startsWith(QStringLiteral("xaction|"))) {
                xactionKey = key;
                break;
            }
        }
    }

    if (!record) {
        xactionRowsCaption_->setText(QStringLiteral("Xaction Rows"));
        xactionRowsTable_->setEnabled(false);
        return;
    }

    if (xactionKey.isEmpty() || !currentTraceSession_ || !currentTraceSession_->isXactionIndexComplete()) {
        xactionRowsCaption_->setText(QStringLiteral("Xaction Rows: not available"));
        xactionRowsTable_->setEnabled(false);
        return;
    }

    const std::vector<std::uint64_t> logicalRows =
        currentTraceSession_->xactionRowsForGroup(xactionKey);
    xactionRowsCaption_->setText(QStringLiteral("Xaction Rows: %1 (%2)")
                                     .arg(xactionKey)
                                     .arg(logicalRows.size()));
    xactionRowsTable_->setEnabled(!logicalRows.empty());

    const int rowCount = qMin<int>(static_cast<int>(logicalRows.size()), 512);
    xactionRowsTable_->setRowCount(rowCount);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const int rowHeight = flitModel_
        ? TraceTableRowHeightForVisibleRows(flitModel_->visibleCount())
        : 22;
    xactionRowsTable_->verticalHeader()->setDefaultSectionSize(rowHeight);
    xactionRowsTable_->verticalHeader()->setMinimumSectionSize(rowHeight);

    const bool showNodeLabels = flitModel_ && flitModel_->nodeLabelsVisible();
    const CLogBTraceMetadata* metadata = currentTraceSession_
        ? &currentTraceSession_->metadata()
        : nullptr;

    for (int tableRow = 0; tableRow < rowCount; ++tableRow) {
        const int logicalRow = static_cast<int>(logicalRows[static_cast<std::size_t>(tableRow)]);
        const int visibleRow = flitModel_ ? flitModel_->visibleRowForLogicalRow(logicalRow) : -1;
        const FlitRecord* rowRecord = flitModel_ ? flitModel_->recordForLogicalRow(logicalRow) : nullptr;

        auto makeItem = [logicalRow, visibleRow, &mono](const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setData(Qt::UserRole, logicalRow);
            item->setData(Qt::UserRole + 1, visibleRow);
            item->setFont(mono);
            return item;
        };

        auto makeNodeItem = [makeItem, showNodeLabels, metadata](const FlitRecord* rowRecord,
                                                                 const bool targetColumn) {
            QTableWidgetItem* item = makeItem(rowRecord
                                                  ? (targetColumn ? rowRecord->target : rowRecord->source)
                                                  : QStringLiteral("-"));
            if (rowRecord) {
                QString label;
                QColor color;
                if (XactionNodeLabelForRecord(*rowRecord, targetColumn, showNodeLabels, metadata, label, color)) {
                    item->setData(FlitTableModel::NodeLabelTextRole, label);
                    item->setData(FlitTableModel::NodeLabelColorRole, color);
                    item->setData(FlitTableModel::BadgeFontScaleRole, 0.82);
                }
            }
            return item;
        };

        xactionRowsTable_->setItem(tableRow, 0, makeItem(rowRecord
                                                             ? FormatTimestampForDisplay(rowRecord->timestamp)
                                                             : QStringLiteral("-")));

        QTableWidgetItem* channelItem = makeItem(rowRecord
                                                     ? ToString(rowRecord->direction) + ToString(rowRecord->channel)
                                                     : QStringLiteral("-"));
        if (rowRecord) {
            channelItem->setData(FlitTableModel::ChannelAccentRole, ChannelAccent(rowRecord->channel));
            channelItem->setData(FlitTableModel::TransactionHighlightRole, true);
            channelItem->setData(FlitTableModel::ChannelTagRole, rowRecord->channelTag);
            channelItem->setData(FlitTableModel::BadgeFontScaleRole, 0.82);
            channelItem->setData(FlitTableModel::BadgeLeftAlignedRole, true);
        }
        xactionRowsTable_->setItem(tableRow, 1, channelItem);

        xactionRowsTable_->setItem(tableRow, 2, makeItem(rowRecord ? rowRecord->opcode : QStringLiteral("-")));
        xactionRowsTable_->setItem(tableRow, 3, makeNodeItem(rowRecord, false));
        xactionRowsTable_->setItem(tableRow, 4, makeNodeItem(rowRecord, true));
        xactionRowsTable_->setItem(tableRow, 5, makeItem(rowRecord && !rowRecord->txnId.isEmpty()
                                                             ? rowRecord->txnId
                                                             : QStringLiteral("-")));
        xactionRowsTable_->setItem(tableRow, 6, makeItem(rowRecord && !rowRecord->dbid.isEmpty()
                                                             ? rowRecord->dbid
                                                             : QStringLiteral("-")));

        if (visibleRow < 0) {
            for (int column = 0; column < xactionRowsTable_->columnCount(); ++column) {
                if (QTableWidgetItem* item = xactionRowsTable_->item(tableRow, column)) {
                    item->setForeground(QColor(QStringLiteral("#6B7280")));
                }
            }
        }
        xactionRowsTable_->setRowHeight(tableRow, rowHeight);
    }

    if (logicalRows.size() > static_cast<std::size_t>(rowCount)) {
        xactionRowsCaption_->setText(xactionRowsCaption_->text()
                                     + QStringLiteral(" - first %1 shown").arg(rowCount));
    }
}

void MainWindow::updateTimelineSelection()
{
    if (!timelineWidget_ || !flitModel_ || !flitView_ || !flitView_->selectionModel()) {
        return;
    }

    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    if (!current.isValid()) {
        timelineWidget_->setSelectedLogicalRow(-1);
        timelineWidget_->clearSelectedRow();
        return;
    }

    timelineWidget_->setSelectedLogicalRow(flitModel_->logicalRowAt(current.row()));
    const FlitRecord* record = flitModel_->recordAt(current.row());
    if (record) {
        timelineWidget_->setSelectedRow(record->direction,
                                        record->channel,
                                        record->channelNodeId
                                            ? std::optional<std::uint32_t>(
                                                  static_cast<std::uint32_t>(*record->channelNodeId))
                                            : std::nullopt);
    } else {
        timelineWidget_->clearSelectedRow();
    }
}

void MainWindow::updateAddressSelection()
{
    if (!addressWidget_ || !flitModel_ || !flitView_ || !flitView_->selectionModel()) {
        return;
    }

    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    if (!current.isValid()) {
        addressWidget_->setSelectedLogicalRow(-1);
        return;
    }

    addressWidget_->setSelectedLogicalRow(flitModel_->logicalRowAt(current.row()));
}

void MainWindow::updateTransactionSelection()
{
    if (!transactionWidget_ || !flitModel_ || !flitView_ || !flitView_->selectionModel()) {
        return;
    }

    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    if (!current.isValid()) {
        transactionWidget_->setSelectedLogicalRow(-1);
        return;
    }

    transactionWidget_->setSelectedLogicalRow(flitModel_->logicalRowAt(current.row()));
}

void MainWindow::updateCacheSelection()
{
    if (!cacheWidget_ || !flitModel_ || !flitView_ || !flitView_->selectionModel()) {
        return;
    }

    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    if (!current.isValid()) {
        cacheWidget_->setSelectedLogicalRow(-1);
        return;
    }

    cacheWidget_->setSelectedLogicalRow(flitModel_->logicalRowAt(current.row()));
}

void MainWindow::jumpToLogicalTraceRow(const int logicalRow)
{
    if (!flitModel_ || !flitView_ || logicalRow < 0) {
        return;
    }

    const int visibleRow = flitModel_->visibleRowForLogicalRow(logicalRow);
    if (visibleRow < 0) {
        statusBar()->showMessage(QStringLiteral("Trace row %1 is hidden by the current filters.")
                                     .arg(logicalRow + 1),
                                 3500);
        return;
    }

    const QModelIndex target = flitModel_->index(visibleRow, 0);
    flitView_->selectionModel()->setCurrentIndex(target,
                                                 QItemSelectionModel::ClearAndSelect
                                                     | QItemSelectionModel::Rows);
    flitView_->selectRow(visibleRow);
    flitView_->scrollTo(target, QAbstractItemView::PositionAtCenter);
    updateTimelineSelection();
    updateAddressSelection();
    updateCacheSelection();
    updateTransactionSelection();
    statusBar()->showMessage(QStringLiteral("Selected trace row %1.").arg(logicalRow + 1),
                             1500);
}

void MainWindow::updateTraceEmptyState()
{
    if (!traceContentStack_ || !traceEmptyState_ || !flitView_) {
        return;
    }

    const bool hasTrace =
        currentTraceSession_
        || !currentTracePath_.isEmpty()
        || !currentTraceLabel_.isEmpty()
        || flitModel_->totalCount() > 0;
    traceContentStack_->setCurrentWidget(hasTrace ? static_cast<QWidget*>(flitView_)
                                                  : traceEmptyState_);
}

void MainWindow::updateFilterProgress(const bool active,
                                      const QString& text,
                                      const int value,
                                      const int maximum)
{
    if (!filterProgressLabel_ || !filterProgressBar_) {
        return;
    }

    if (!active) {
        const bool wasVisible = filterProgressBar_->isVisible() || filterProgressLabel_->isVisible();
        filterProgressLabel_->hide();
        filterProgressBar_->hide();
        filterProgressBar_->setRange(0, 1000);
        filterProgressBar_->setValue(0);
        if (wasVisible) {
            statusBar()->showMessage(
                QStringLiteral("Filtering complete. Showing %1 rows.")
                    .arg(flitModel_->visibleCount()),
                3000);
        }
        return;
    }

    filterProgressLabel_->setText(text.isEmpty()
                                      ? QStringLiteral("Filtering trace rows...")
                                      : text);
    if (maximum > 0) {
        filterProgressBar_->setRange(0, maximum);
        filterProgressBar_->setValue(qBound(0, value, maximum));
    } else {
        filterProgressBar_->setRange(0, 0);
    }
    filterProgressLabel_->show();
    filterProgressBar_->show();
}

void MainWindow::updateXactionIndexProgress(const bool active,
                                            const QString& text,
                                            const std::uint64_t completedRecords,
                                            const std::uint64_t totalRecords)
{
    if (!xactionIndexProgressLabel_ || !xactionIndexProgressBar_) {
        return;
    }

    if (!active) {
        if (TraceViewSession* session = activeTraceViewSession()) {
            session->xactionIndexActive = false;
            session->xactionIndexProgressText.clear();
            session->xactionIndexCompletedRecords = 0;
            session->xactionIndexTotalRecords = 0;
        }
        xactionIndexProgressLabel_->hide();
        xactionIndexProgressBar_->hide();
        xactionIndexProgressBar_->setRange(0, 1000);
        xactionIndexProgressBar_->setValue(0);
        return;
    }

    if (TraceViewSession* session = activeTraceViewSession()) {
        session->xactionIndexActive = true;
        session->xactionIndexProgressText = text;
        session->xactionIndexCompletedRecords = completedRecords;
        session->xactionIndexTotalRecords = totalRecords;
    }

    xactionIndexProgressLabel_->setText(text.isEmpty()
                                            ? QStringLiteral("Xaction index")
                                            : text);
    if (totalRecords == 0) {
        xactionIndexProgressBar_->setRange(0, 0);
    } else if (totalRecords <= static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
        const int maximum = static_cast<int>(totalRecords);
        xactionIndexProgressBar_->setRange(0, maximum);
        xactionIndexProgressBar_->setValue(static_cast<int>(
            std::min<std::uint64_t>(completedRecords, totalRecords)));
    } else {
        constexpr int kScaledMaximum = 1000;
        const std::uint64_t clampedCompleted = std::min(completedRecords, totalRecords);
        xactionIndexProgressBar_->setRange(0, kScaledMaximum);
        xactionIndexProgressBar_->setValue(static_cast<int>(
            (clampedCompleted * static_cast<std::uint64_t>(kScaledMaximum)) / totalRecords));
    }

    xactionIndexProgressLabel_->show();
    xactionIndexProgressBar_->show();
}

void MainWindow::updateClipboardInsertProgress(const bool active,
                                               const QString& text,
                                               const std::uint64_t completedRecords,
                                               const std::uint64_t totalRecords)
{
    if (!clipboardInsertProgressLabel_ || !clipboardInsertProgressBar_) {
        return;
    }

    if (!active) {
        clipboardInsertProgressLabel_->hide();
        clipboardInsertProgressBar_->hide();
        if (clipboardInsertCancelButton_) {
            clipboardInsertCancelButton_->hide();
            clipboardInsertCancelButton_->setEnabled(true);
        }
        clipboardInsertProgressBar_->setRange(0, 1000);
        clipboardInsertProgressBar_->setValue(0);
        return;
    }

    clipboardInsertProgressLabel_->setText(text.isEmpty()
                                               ? QStringLiteral("Clipboard insert")
                                               : text);
    if (totalRecords == 0) {
        clipboardInsertProgressBar_->setRange(0, 0);
    } else if (totalRecords <= static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
        const int maximum = static_cast<int>(totalRecords);
        clipboardInsertProgressBar_->setRange(0, maximum);
        clipboardInsertProgressBar_->setValue(static_cast<int>(
            std::min<std::uint64_t>(completedRecords, totalRecords)));
    } else {
        constexpr int kScaledMaximum = 1000;
        const std::uint64_t clampedCompleted = std::min(completedRecords, totalRecords);
        clipboardInsertProgressBar_->setRange(0, kScaledMaximum);
        clipboardInsertProgressBar_->setValue(static_cast<int>(
            (clampedCompleted * static_cast<std::uint64_t>(kScaledMaximum)) / totalRecords));
    }

    clipboardInsertProgressLabel_->show();
    clipboardInsertProgressBar_->show();
    if (clipboardInsertCancelButton_) {
        clipboardInsertCancelButton_->show();
        clipboardInsertCancelButton_->setEnabled(clipboardXactionAddressInsertStopSource_
                                                 && !clipboardXactionAddressInsertStopSource_->stop_requested()
                                                 && !clipboardInsertApplyState_);
    }
}

void MainWindow::setTraceToolbarFolded(const bool folded)
{
    const bool stateChanged = folded != traceToolbarFolded_;
    traceToolbarFolded_ = folded;

    placeTraceToolbarFoldButton(folded);

    for (QWidget* row : std::as_const(traceToolbarFoldableRows_)) {
        if (row) {
            row->setVisible(!folded);
        }
    }
    for (QWidget* widget : std::as_const(traceToolbarFoldableSearchWidgets_)) {
        if (widget) {
            widget->setVisible(!folded);
        }
    }
    if (optionalFieldSearchRow_) {
        if (folded) {
            if (stateChanged) {
                optionalFieldSearchRowVisibleBeforeToolbarFold_ = optionalFieldSearchRow_->isVisible();
            }
            optionalFieldSearchRow_->hide();
        } else if (optionalFieldSearchRowVisibleBeforeToolbarFold_) {
            optionalFieldSearchRow_->show();
        }
    }
    if (traceToolbarFoldButton_) {
        const QSignalBlocker blocker(traceToolbarFoldButton_);
        traceToolbarFoldButton_->setChecked(folded);
        traceToolbarFoldButton_->setArrowType(folded ? Qt::DownArrow : Qt::UpArrow);
        const QString hint = folded
            ? QStringLiteral("Unfold the Flits toolbar.")
            : QStringLiteral("Fold the Flits toolbar.");
        traceToolbarFoldButton_->setToolTip(hint);
        traceToolbarFoldButton_->setStatusTip(hint);
    }
    if (traceToolbar_) {
        if (!folded) {
            traceToolbar_->setMaximumHeight(QWIDGETSIZE_MAX);
        }
        if (QLayout* contentLayout = traceToolbarContent_ ? traceToolbarContent_->layout() : nullptr) {
            contentLayout->invalidate();
            contentLayout->activate();
        }
        if (QLayout* toolbarLayout = traceToolbar_->layout()) {
            toolbarLayout->invalidate();
            toolbarLayout->activate();
        }
        if (folded) {
            traceToolbar_->setMaximumHeight(traceToolbar_->sizeHint().height());
        }
        traceToolbar_->updateGeometry();
        if (QWidget* parent = traceToolbar_->parentWidget(); parent && parent->layout()) {
            parent->layout()->invalidate();
            parent->layout()->activate();
        }
    }

    if (stateChanged) {
        scheduleDiagnosticsSnapshotRefresh();
    }
}

void MainWindow::placeTraceToolbarFoldButton(const bool folded)
{
    if (!traceToolbarFoldButton_ || !traceToolbarSummaryLayout_ || !traceToolbarSearchLayout_) {
        return;
    }

    QHBoxLayout* targetLayout = folded ? traceToolbarSummaryLayout_ : traceToolbarSearchLayout_;
    traceToolbarSummaryLayout_->removeWidget(traceToolbarFoldButton_);
    traceToolbarSearchLayout_->removeWidget(traceToolbarFoldButton_);
    if (traceToolbarFoldButton_->parentWidget() != targetLayout->parentWidget()) {
        traceToolbarFoldButton_->setParent(targetLayout->parentWidget());
    }
    if (traceToolbarFoldButton_->parentWidget() && !traceToolbarFoldButton_->parentWidget()->isVisible()) {
        traceToolbarFoldButton_->parentWidget()->show();
    }
    targetLayout->addWidget(traceToolbarFoldButton_);

    if (QWidget* searchRow = qobject_cast<QWidget*>(traceToolbarSearchLayout_->parent())) {
        searchRow->setVisible(!folded);
    }
}

void MainWindow::updateSearchNavigationButtons()
{
    const bool highlighting = flitModel_
        && flitModel_->searchMode() == FlitTableModel::SearchMode::Highlight;
    FlitTableModel::SearchHighlightNavigationIndex navigationIndex;
    bool buildingIndex = false;
    if (highlighting && flitModel_->rowCount() > 0) {
        const QModelIndex current = flitView_ && flitView_->selectionModel()
            ? flitView_->selectionModel()->currentIndex()
            : QModelIndex();
        navigationIndex = flitModel_->searchHighlightNavigationIndex(current.isValid() ? current.row() : -1);
        buildingIndex = flitModel_->isSearchHighlightIndexBuilding();
    }
    const bool enabled = highlighting && navigationIndex.total > 0;

    if (searchPreviousButton_) {
        searchPreviousButton_->setVisible(highlighting);
        searchPreviousButton_->setEnabled(enabled);
    }
    if (searchNextButton_) {
        searchNextButton_->setVisible(highlighting);
        searchNextButton_->setEnabled(enabled);
    }
    if (searchNavigationLabel_) {
        searchNavigationLabel_->setVisible(highlighting);
        QString labelText = QStringLiteral("%1/%2")
                                .arg(navigationIndex.offset)
                                .arg(navigationIndex.total);
        if (buildingIndex) {
            labelText.append(QLatin1Char('+'));
        }
        searchNavigationLabel_->setText(labelText);
        searchNavigationLabel_->setToolTip(
            buildingIndex
                ? QStringLiteral("Highlight matches are still being indexed.")
                : QStringLiteral("Current highlighted flit / total highlighted flits."));
    }
}

void MainWindow::navigateSearchHighlight(const bool forward)
{
    if (!flitModel_
        || flitModel_->searchMode() != FlitTableModel::SearchMode::Highlight
        || !flitView_
        || !flitView_->selectionModel()) {
        return;
    }

    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    const int currentRow = current.isValid() ? current.row() : (forward ? -1 : flitModel_->rowCount());
    const int targetRow = flitModel_->findSearchHighlightedRow(currentRow, forward);
    if (targetRow < 0) {
        statusBar()->showMessage(
            flitModel_->isSearchHighlightIndexBuilding()
                ? QStringLiteral("Highlight search is still indexing rows.")
                : QStringLiteral("No highlighted flits found."),
            2500);
        return;
    }

    const QModelIndex target = flitModel_->index(targetRow, 0);
    flitView_->selectionModel()->setCurrentIndex(target,
                                                 QItemSelectionModel::ClearAndSelect
                                                     | QItemSelectionModel::Rows);
    flitView_->selectRow(targetRow);
    flitView_->scrollTo(target, QAbstractItemView::PositionAtCenter);
    updateSearchNavigationButtons();
    statusBar()->showMessage(QStringLiteral("Selected highlighted flit %1.")
                                 .arg(targetRow + 1),
                             1500);
}

bool MainWindow::handleTableToolTip(QTableView* view, QEvent* event)
{
    if (!view || event->type() != QEvent::ToolTip) {
        return false;
    }

    const auto* helpEvent = static_cast<QHelpEvent*>(event);
    const QModelIndex index = view->indexAt(helpEvent->pos());
    if (!index.isValid()) {
        QToolTip::hideText();
        return true;
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty() || !isTableCellElided(view, index)) {
        QToolTip::hideText();
        return true;
    }

    QToolTip::showText(helpEvent->globalPos(), text, view);
    return true;
}

bool MainWindow::isTableCellElided(QTableView* view, const QModelIndex& index) const
{
    if (!view || !index.isValid()) {
        return false;
    }

    const QRect rect = view->visualRect(index);
    if (!rect.isValid()) {
        return false;
    }

    QStyleOptionViewItem option;
    option.initFrom(view);
    option.rect = rect;
    const QVariant fontData = index.data(Qt::FontRole);
    option.font = fontData.value<QFont>();
    if (!fontData.isValid()) {
        option.font = view->font();
    }

    QAbstractItemDelegate* delegate = view->itemDelegateForColumn(index.column());
    if (!delegate) {
        delegate = view->itemDelegate();
    }

    if (!delegate) {
        return false;
    }

    return delegate->sizeHint(option, index).width() > rect.width();
}

void MainWindow::showTableCellDialog(QTableView* view, const QModelIndex& index)
{
    if (!view || !index.isValid()) {
        return;
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) {
        return;
    }

    const QString tableTitle = (view == flitView_) ? QStringLiteral("Flits")
                                                   : QStringLiteral("Fields");
    const QString columnTitle = view->model()->headerData(index.column(), Qt::Horizontal, Qt::DisplayRole).toString();

    QDialog* dialog = new QDialog(this, Qt::Tool);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(QStringLiteral("%1 - %2").arg(tableTitle, columnTitle));
    dialog->resize(560, 220);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    QLabel* contextLabel = new QLabel(
        QStringLiteral("Column: %1    Row: %2").arg(columnTitle).arg(index.row() + 1),
        dialog);
    contextLabel->setObjectName(QStringLiteral("secondaryLabel"));
    layout->addWidget(contextLabel);

    QPlainTextEdit* textView = new QPlainTextEdit(dialog);
    textView->setReadOnly(true);
    textView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    textView->setPlainText(text);
    layout->addWidget(textView, 1);

    QPushButton* closeButton = new QPushButton(QStringLiteral("Close"), dialog);
    closeButton->setObjectName(QStringLiteral("actionButton"));
    closeButton->setFixedHeight(22);
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);

    QHBoxLayout* actionsLayout = new QHBoxLayout();
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->addStretch(1);
    actionsLayout->addWidget(closeButton);
    layout->addLayout(actionsLayout);

    dialog->show();
}

void MainWindow::showXactionDebugDialog(const int visibleRow, const bool highlighted)
{
    if (!xactionDebugMode_) {
        return;
    }

    const FlitRecord* sourceRecord = flitModel_ ? flitModel_->recordAt(visibleRow) : nullptr;
    if (!sourceRecord) {
        return;
    }
    const FlitRecord record = *sourceRecord;

    struct DebugRow {
        int visibleRow = -1;
        int logicalRow = -1;
        FlitRecord record;
        bool anchor = false;
    };

    std::vector<DebugRow> debugRows;
    int omittedRows = 0;
    if (highlighted && flitModel_) {
        const int visibleRows = flitModel_->visibleCount();
        debugRows.reserve(static_cast<std::size_t>(qMin(visibleRows, kMaxXactionDebugLoggedRows)));
        for (int row = 0; row < visibleRows; ++row) {
            if (!flitModel_->isTransactionHighlightedRow(row)) {
                continue;
            }
            if (static_cast<int>(debugRows.size()) >= kMaxXactionDebugLoggedRows) {
                ++omittedRows;
                continue;
            }
            if (const FlitRecord* highlightedRecord = flitModel_->recordAt(row)) {
                debugRows.push_back(DebugRow{
                    .visibleRow = row,
                    .logicalRow = flitModel_->logicalRowAt(row),
                    .record = *highlightedRecord,
                    .anchor = row == visibleRow,
                });
            }
        }
    }
    if (debugRows.empty()) {
        debugRows.push_back(DebugRow{
            .visibleRow = visibleRow,
            .logicalRow = flitModel_ ? flitModel_->logicalRowAt(visibleRow) : -1,
            .record = record,
            .anchor = true,
        });
    }

    QString text;
    QTextStream stream(&text);
    stream << "Double-click action: "
           << (highlighted ? QStringLiteral("enabled transactional highlight")
                           : QStringLiteral("cleared transactional highlight"))
           << '\n';
    stream << "Logged rows: " << debugRows.size();
    if (omittedRows > 0) {
        stream << " (" << omittedRows << " additional highlighted rows omitted)";
    }
    stream << "\n\n";

    for (std::size_t index = 0; index < debugRows.size(); ++index) {
        const DebugRow& debugRow = debugRows[index];
        if (index > 0) {
            stream << "\n\n";
        }
        stream << "==== Highlighted row " << (debugRow.visibleRow + 1);
        if (debugRow.anchor) {
            stream << " (double-clicked)";
        }
        stream << " ====\n";
        stream << ToString(debugRow.record.direction) << ToString(debugRow.record.channel)
               << ' ' << debugRow.record.opcode
               << "    Txn: "
               << (debugRow.record.txnId.isEmpty() ? QStringLiteral("-") : debugRow.record.txnId)
               << "\n\n";
        stream << "Logical row: " << (debugRow.logicalRow >= 0 ? debugRow.logicalRow + 1 : -1) << '\n';
        stream << "Visible index state: " << XactionIndexStateText(debugRow.record) << '\n';
        stream << "Visible group key: " << XactionGroupKeyText(debugRow.record) << "\n\n";

        if (currentTraceSession_ && debugRow.logicalRow >= 0) {
            stream << currentTraceSession_->xactionDebugInfo(
                static_cast<std::uint64_t>(debugRow.logicalRow));
        } else {
            stream << "No persisted Xaction index information is available for this row.";
        }
    }

    QDialog* dialog = new QDialog(this, Qt::Tool);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(QStringLiteral("Xaction Highlight Debug"));
    dialog->resize(780, 440);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    QLabel* contextLabel = new QLabel(
        QStringLiteral("Double-clicked row: %1    %2%3 %4    Txn: %5")
            .arg(visibleRow + 1)
            .arg(ToString(record.direction), ToString(record.channel), record.opcode)
            .arg(record.txnId.isEmpty() ? QStringLiteral("-") : record.txnId),
        dialog);
    contextLabel->setObjectName(QStringLiteral("secondaryLabel"));
    layout->addWidget(contextLabel);

    QPlainTextEdit* textView = new QPlainTextEdit(dialog);
    textView->setReadOnly(true);
    textView->setLineWrapMode(QPlainTextEdit::NoWrap);
    textView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    textView->setPlainText(text);
    layout->addWidget(textView, 1);

    QPushButton* closeButton = new QPushButton(QStringLiteral("Close"), dialog);
    closeButton->setObjectName(QStringLiteral("actionButton"));
    closeButton->setFixedHeight(22);
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);

    QHBoxLayout* actionsLayout = new QHBoxLayout();
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->addStretch(1);
    actionsLayout->addWidget(closeButton);
    layout->addLayout(actionsLayout);

    dialog->show();
}

QString MainWindow::inspectorHtml(const FlitRecord* record) const
{
    if (!record) {
        return QStringLiteral(
            "<div style='color:#6B7280;font-family:Segoe UI;'>"
            "Select a flit to inspect its decoded fields."
            "</div>");
    }

    const QString addressOrDbid = !record->address.isEmpty() ? record->address : record->dbid;

    return QStringLiteral(
               "<div style='font-family:Segoe UI;color:#18212B;'>"
               "<div style='font-size:22px;font-weight:700;color:%1;'>%2</div>"
               "<div style='margin-top:4px;color:#6B7280;'>%3</div>"
               "<div style='margin-top:14px;padding:12px;background:#F4F6FA;border:1px solid #D8E1EA;'>"
               "<div><b>Timestamp:</b> %4</div>"
               "<div><b>Channel:</b> %5</div>"
               "<div><b>Direction:</b> %6</div>"
               "<div><b>Txn:</b> %7</div>"
               "<div><b>Addr/DBID:</b> %8</div>"
               "</div>"
               "<div style='margin-top:14px;'><b>Summary</b><br/>%9</div>"
               "<div style='margin-top:12px;'><b>Interpretation</b><br/>%10</div>"
               "<div style='margin-top:12px;color:#6B7280;'>"
               "Reading path: packet row -> decoded field table."
               "</div>"
               "</div>")
        .arg(ChannelAccent(record->channel).name())
        .arg(record->opcode)
        .arg(record->transactionLabel.isEmpty() ? QStringLiteral("No transaction label") : record->transactionLabel)
        .arg(FormatTimestampForDisplay(record->timestamp))
        .arg(ToString(record->channel))
        .arg(ToString(record->direction))
        .arg(record->txnId.isEmpty() ? QStringLiteral("-") : record->txnId)
        .arg(addressOrDbid.isEmpty() ? QStringLiteral("-") : addressOrDbid)
        .arg(record->summary.isEmpty() ? QStringLiteral("-") : record->summary)
        .arg(record->annotation.isEmpty() ? QStringLiteral("No annotation provided.") : record->annotation);
}

}  // namespace CHIron::Gui

