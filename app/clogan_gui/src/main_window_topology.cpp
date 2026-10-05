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

    updatingTopologyTable_ = true;
    topologyTable_->setUpdatesEnabled(false);
    topologyTable_->clearContents();
    topologyTable_->setRowCount(0);

    if (!currentTraceSession_) {
        topologySummaryLabel_->setText(QStringLiteral("No trace file is currently open."));
        if (topologyApplyButton_) {
            topologyApplyButton_->setEnabled(false);
        }
        if (topologyRevertButton_) {
            topologyRevertButton_->setEnabled(false);
        }
        updatingTopologyTable_ = false;
        topologyTable_->setUpdatesEnabled(true);
        return;
    }

    const CLogBTraceMetadata& metadata = currentTraceSession_->metadata();

    struct TopologyNodeRow {
        quint16 nodeId = 0;
        bool mapped = true;
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
            .mapped = true,
            .type = nodeType,
            .annotation = std::move(annotation),
        });
    }

    const TraceViewSession* guessSession = activeTraceViewSession();
    if (guessSession) {
        for (const NodeTypeGuess& guess : guessSession->nodeTypeGuessResults) {
            if (metadata.nodeTypes.find(guess.nodeId) != metadata.nodeTypes.cend()) {
                continue;
            }
            const QString annotation = guess.type.has_value()
                ? QStringLiteral("Guessed %1 (%2 confidence): %3")
                      .arg(QString::fromStdString(CLog::NodeTypeToString(*guess.type)),
                           NodeTypeGuessConfidenceToString(guess.confidence),
                           guess.reasons.join(QStringLiteral(" ")))
                : QStringLiteral("Observed in traffic; not stored in the trace topology.");
            rows.push_back(TopologyNodeRow{
                .nodeId = guess.nodeId,
                .mapped = false,
                .type = guess.type.value_or(CLog::NodeType::RN_F),
                .annotation = annotation,
            });
        }
    }

    std::sort(rows.begin(), rows.end(), [](const TopologyNodeRow& lhs, const TopologyNodeRow& rhs) {
        return lhs.nodeId < rhs.nodeId;
    });

    std::size_t unmappedCount = 0;
    for (const TopologyNodeRow& row : rows) {
        if (!row.mapped) {
            ++unmappedCount;
        }
    }

    if (rows.empty()) {
        topologySummaryLabel_->setText(
            QStringLiteral("No topology information is stored in %1. Run a node type guess to "
                           "populate observed node IDs.")
                .arg(currentTraceLabel_.isEmpty() ? QStringLiteral("the current trace")
                                                  : currentTraceLabel_));
    } else if (unmappedCount == 0) {
        topologySummaryLabel_->setText(
            QStringLiteral("%1 node%2 in %3. Double-click a Type cell to edit it.")
                .arg(rows.size())
                .arg(rows.size() == 1 ? QString() : QStringLiteral("s"))
                .arg(currentTraceLabel_.isEmpty() ? QStringLiteral("the current trace")
                                                  : currentTraceLabel_));
    } else {
        topologySummaryLabel_->setText(
            QStringLiteral("%1 node%2 in %3, %4 unmapped. Double-click a Type cell to edit it.")
                .arg(rows.size())
                .arg(rows.size() == 1 ? QString() : QStringLiteral("s"))
                .arg(currentTraceLabel_.isEmpty() ? QStringLiteral("the current trace")
                                                  : currentTraceLabel_)
                .arg(unmappedCount));
    }

    topologyTable_->setRowCount(static_cast<int>(rows.size()));
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const TopologyNodeRow& topologyRow = rows[static_cast<std::size_t>(row)];

        QStringList allowedTypes;
        if (!topologyRow.mapped) {
            allowedTypes.append(QStringLiteral("Unmapped"));
        }
        for (const CLog::NodeType type : AllNodeTypes()) {
            allowedTypes.append(QString::fromStdString(CLog::NodeTypeToString(type)));
        }

        QString displayType = topologyRow.mapped
            ? QString::fromStdString(CLog::NodeTypeToString(topologyRow.type))
            : QStringLiteral("Unmapped");
        if (const auto pendingIt = pendingTopologyAssignments_.find(topologyRow.nodeId);
            pendingIt != pendingTopologyAssignments_.cend()) {
            displayType = pendingIt->second.has_value()
                ? QString::fromStdString(CLog::NodeTypeToString(*pendingIt->second))
                : QStringLiteral("Unmapped");
        }

        auto* nodeItem = new QTableWidgetItem(QString::number(topologyRow.nodeId));
        nodeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        nodeItem->setFlags(nodeItem->flags() & ~Qt::ItemIsEditable);
        nodeItem->setData(Qt::UserRole, static_cast<uint>(topologyRow.nodeId));

        auto* typeItem = new QTableWidgetItem(displayType);
        typeItem->setFlags(typeItem->flags() | Qt::ItemIsEditable);
        typeItem->setData(Qt::UserRole, allowedTypes);

        const std::optional<CLog::NodeType> displayTypeValue = NodeTypeFromString(displayType);
        auto* valueItem = new QTableWidgetItem(
            displayTypeValue.has_value()
                ? QString::number(static_cast<unsigned int>(*displayTypeValue))
                : QStringLiteral("-"));
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        auto* annotationItem = new QTableWidgetItem(
            topologyRow.annotation.isEmpty() ? QStringLiteral("-") : topologyRow.annotation);
        annotationItem->setFlags(annotationItem->flags() & ~Qt::ItemIsEditable);

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

    if (topologyApplyButton_) {
        topologyApplyButton_->setEnabled(!pendingTopologyAssignments_.empty());
    }
    if (topologyRevertButton_) {
        topologyRevertButton_->setEnabled(!pendingTopologyAssignments_.empty());
    }

    updatingTopologyTable_ = false;
    topologyTable_->setUpdatesEnabled(true);
}

void MainWindow::updateTopologyAction()
{
    if (!topologyAction_) {
        return;
    }

    const bool hasSession = currentTraceSession_ != nullptr;
    topologyAction_->setEnabled(hasSession);
    topologyAction_->setToolTip(
        hasSession
            ? QStringLiteral("Display and edit CHI topology information for the current CLog.B file.")
            : QStringLiteral("Open a CLog.B trace to view CHI topology information."));
    topologyAction_->setStatusTip(topologyAction_->toolTip());

    if (topologyDock_) {
        QAction* toggleAction = topologyDock_->toggleViewAction();
        toggleAction->setEnabled(hasSession);
        toggleAction->setToolTip(topologyAction_->toolTip());
        toggleAction->setStatusTip(topologyAction_->statusTip());
        if (!hasSession && !topologyDock_->isClosed()) {
            topologyDock_->closeDockWidget();
        }
    }

    updateNodeTypesAction();
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

