#include "main_window_internal.hpp"

namespace CHIron::Gui {
using namespace MainWindowDetail;

void MainWindow::showTopologyDialog()
{
    if (!topologyDock_ || !topologyAction_->isEnabled()) {
        return;
    }

    refreshTopologyView();
    topologyDock_->toggleView(true);
    topologyDock_->raise();
}


void MainWindow::refreshTopologyView()
{
    if (!topologySummaryLabel_ || !topologyTable_) {
        return;
    }

    topologyTable_->setUpdatesEnabled(false);
    topologyTable_->clearContents();
    topologyTable_->setRowCount(0);

    if (!currentTraceSession_) {
        topologySummaryLabel_->setText(QStringLiteral("No trace file is currently open."));
        topologyTable_->setUpdatesEnabled(true);
        return;
    }

    const CLogBTraceMetadata& metadata = currentTraceSession_->metadata();
    if (metadata.nodeTypes.empty()) {
        topologySummaryLabel_->setText(QStringLiteral("The current CLog.B file does not contain CHI topology information."));
        topologyTable_->setUpdatesEnabled(true);
        return;
    }

    struct TopologyNodeRow {
        quint16 nodeId = 0;
        CLog::NodeType type = CLog::NodeType::RN_F;
        QString annotation;
    };

    std::vector<TopologyNodeRow> rows;
    rows.reserve(metadata.nodeTypes.size());
    for (const auto& [nodeId, nodeType] : metadata.nodeTypes) {
        QString annotation;
        const auto annotationIt = metadata.nodeAnnotations.find(nodeId);
        if (annotationIt != metadata.nodeAnnotations.cend()) {
            annotation = annotationIt->second;
        }
        rows.push_back(TopologyNodeRow{
            .nodeId = nodeId,
            .type = nodeType,
            .annotation = std::move(annotation),
        });
    }
    std::sort(rows.begin(), rows.end(), [](const TopologyNodeRow& lhs, const TopologyNodeRow& rhs) {
        return lhs.nodeId < rhs.nodeId;
    });

    topologySummaryLabel_->setText(
        QStringLiteral("%1 node%2 stored in %3")
            .arg(rows.size())
            .arg(rows.size() == 1 ? QString() : QStringLiteral("s"))
            .arg(currentTraceLabel_.isEmpty() ? QStringLiteral("the current trace") : currentTraceLabel_));

    topologyTable_->setRowCount(static_cast<int>(rows.size()));
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const TopologyNodeRow& topologyRow = rows[static_cast<std::size_t>(row)];
        const QString typeName = QString::fromStdString(CLog::NodeTypeToString(topologyRow.type));
        const QString typeValue = QString::number(static_cast<unsigned int>(topologyRow.type));

        auto* nodeItem = new QTableWidgetItem(QString::number(topologyRow.nodeId));
        nodeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* typeItem = new QTableWidgetItem(typeName.isEmpty() ? QStringLiteral("Unknown") : typeName);
        auto* valueItem = new QTableWidgetItem(typeValue);
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* annotationItem = new QTableWidgetItem(
            topologyRow.annotation.isEmpty() ? QStringLiteral("-") : topologyRow.annotation);

        topologyTable_->setItem(row, 0, nodeItem);
        topologyTable_->setItem(row, 1, typeItem);
        topologyTable_->setItem(row, 2, valueItem);
        topologyTable_->setItem(row, 3, annotationItem);
    }

    const TraceViewSession* session = activeTraceViewSession();
    if (session && session->topologySelectedNodeId) {
        for (int row = 0; row < topologyTable_->rowCount(); ++row) {
            const QTableWidgetItem* item = topologyTable_->item(row, 0);
            if (!item) {
                continue;
            }
            bool ok = false;
            const uint nodeId = item->text().toUInt(&ok);
            if (ok && nodeId == *session->topologySelectedNodeId) {
                topologyTable_->setCurrentCell(row, 0);
                topologyTable_->selectRow(row);
                break;
            }
        }
    }

    topologyTable_->setUpdatesEnabled(true);
}

void MainWindow::updateTopologyAction()
{
    if (!topologyAction_) {
        return;
    }

    const bool hasTopology = currentTraceSession_
        && !currentTraceSession_->metadata().nodeTypes.empty();
    topologyAction_->setEnabled(hasTopology);
    topologyAction_->setToolTip(
        hasTopology
            ? QStringLiteral("Display CHI topology information stored in the current CLog.B file.")
            : QStringLiteral("No CLog.B topology information is available for the current trace."));
    topologyAction_->setStatusTip(topologyAction_->toolTip());

    if (topologyDock_) {
        QAction* toggleAction = topologyDock_->toggleViewAction();
        toggleAction->setEnabled(hasTopology);
        toggleAction->setToolTip(topologyAction_->toolTip());
        toggleAction->setStatusTip(topologyAction_->statusTip());
        if (!hasTopology && !topologyDock_->isClosed()) {
            topologyDock_->closeDockWidget();
        }
    }

    refreshTopologyView();
}

void MainWindow::updateXactionIndexAction()
{
    if (!xactionIndexAction_) {
        return;
    }

    const bool supported = currentTraceSession_
        && currentTraceSession_->supportsXactionIndexing();
    const bool complete = supported
        && currentTraceSession_->isXactionIndexComplete();
    const bool idle = !xactionIndexActive_;
    xactionIndexAction_->setEnabled(supported && idle);
    xactionIndexAction_->setText(xactionIndexActive_
                                     ? QStringLiteral("Building Xaction Index")
                                     : (complete
                                            ? QStringLiteral("Rebuild Xaction Index")
                                            : QStringLiteral("Build Xaction Index")));
    xactionIndexAction_->setToolTip(
        !supported
            ? QStringLiteral("Xaction indexing is available only for supported CHI-E.b CLog.B traces.")
            : (xactionIndexActive_
                   ? QStringLiteral("Xaction indexing is running.")
                   : (complete
                          ? QStringLiteral("Rebuild Xaction indexing for transactional highlighting.")
                          : QStringLiteral("Build Xaction indexing for transactional highlighting."))));
    xactionIndexAction_->setStatusTip(xactionIndexAction_->toolTip());

    if (rebuildXactionIndexAction_) {
        rebuildXactionIndexAction_->setEnabled(supported && complete && idle);
        rebuildXactionIndexAction_->setToolTip(
            !supported
                ? QStringLiteral("Xaction indexing is available only for supported CHI-E.b CLog.B traces.")
                : (complete
                       ? QStringLiteral("Delete and rebuild the Xaction index for this trace.")
                       : QStringLiteral("No completed Xaction index is available to rebuild.")));
        rebuildXactionIndexAction_->setStatusTip(rebuildXactionIndexAction_->toolTip());
    }

    if (clearXactionIndexAction_) {
        clearXactionIndexAction_->setEnabled(supported && complete && idle);
        clearXactionIndexAction_->setToolTip(
            !supported
                ? QStringLiteral("Xaction indexing is available only for supported CHI-E.b CLog.B traces.")
                : (complete
                       ? QStringLiteral("Delete the persisted Xaction index for this trace.")
                       : QStringLiteral("No completed Xaction index is available to clear.")));
        clearXactionIndexAction_->setStatusTip(clearXactionIndexAction_->toolTip());
    }

    if (deniedFlitsButton_) {
        const bool available = supported && complete && idle;
        if (!available && deniedFlitsButton_->isChecked()) {
            QSignalBlocker blocker(deniedFlitsButton_);
            deniedFlitsButton_->setChecked(false);
            if (flitModel_) {
                flitModel_->setXactionDeniedOnlyFilter(false);
            }
        }
        deniedFlitsButton_->setEnabled(available);
        const QString hint =
            !supported
                ? QStringLiteral("Denied flit filtering is available only for supported CHI-E.b CLog.B traces.")
                : (complete && idle
                       ? QStringLiteral("Show only flits denied by the completed Xaction index.")
                       : QStringLiteral("Build the Xaction index before filtering denied flits."));
        deniedFlitsButton_->setToolTip(hint);
        deniedFlitsButton_->setStatusTip(hint);
    }
}

void MainWindow::refreshNodeLabelDelegates()
{
    if (!flitView_ || !flitModel_) {
        return;
    }

    for (int column = 0; column < flitModel_->columnCount(); ++column) {
        const QString headerText =
            flitModel_->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        if (!IsNodeLabelColumnName(headerText)) {
            continue;
        }

        if (!dynamic_cast<NodeLabelDelegate*>(flitView_->itemDelegateForColumn(column))) {
            flitView_->setItemDelegateForColumn(column, new NodeLabelDelegate(flitView_));
        }
    }
}

}  // namespace CHIron::Gui

