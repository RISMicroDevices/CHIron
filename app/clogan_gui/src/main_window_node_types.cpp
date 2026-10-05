#include "main_window_internal.hpp"

namespace CHIron::Gui {
using namespace MainWindowDetail;

namespace {

constexpr qint64 kNodeTypeGuessBudgetMs = 8;

QString nodeTypeAssignmentColumnName(const int column)
{
    switch (column) {
    case 0:
        return QStringLiteral("Node ID");
    case 1:
        return QStringLiteral("File Type");
    case 2:
        return QStringLiteral("Guessed Type");
    case 3:
        return QStringLiteral("Selected Type");
    }
    return QString();
}

}  // namespace

void MainWindow::showNodeTypeAssignmentDialog()
{
    if (!currentTraceSession_) {
        return;
    }

    if (nodeTypeDialog_) {
        rebuildNodeTypeAssignmentTable();
        if (const TraceViewSession* session = activeTraceViewSession()) {
            updateNodeTypeGuessProgressWidgets(*session);
        }
        nodeTypeDialog_->raise();
        nodeTypeDialog_->activateWindow();
        return;
    }

    nodeTypeDialog_ = new QDialog(this);
    nodeTypeDialog_->setAttribute(Qt::WA_DeleteOnClose);
    nodeTypeDialog_->setWindowTitle(QStringLiteral("Node Type Assignment"));

    QVBoxLayout* layout = new QVBoxLayout(nodeTypeDialog_);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QLabel* titleLabel = new QLabel(QStringLiteral("Assign CHI node types"), nodeTypeDialog_);
    titleLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:700; color:#18212B;"));
    layout->addWidget(titleLabel);

    QLabel* sourceLabel = new QLabel(
        QStringLiteral("Trace: %1").arg(currentTraceLabel_.isEmpty()
                                            ? QStringLiteral("(unnamed)")
                                            : currentTraceLabel_),
        nodeTypeDialog_);
    sourceLabel->setObjectName(QStringLiteral("secondaryLabel"));
    sourceLabel->setWordWrap(true);
    layout->addWidget(sourceLabel);

    QLabel* explanationLabel = new QLabel(
        QStringLiteral("Node IDs observed in the trace that are not listed in the CLog.B topology "
                       "are shown with File Type \"Unmapped\". Run a traffic guess to get type "
                       "suggestions, adjust the selection, then Apply to store the assignments in "
                       "a .nodetypes sidecar next to the trace file."),
        nodeTypeDialog_);
    explanationLabel->setObjectName(QStringLiteral("secondaryLabel"));
    explanationLabel->setWordWrap(true);
    layout->addWidget(explanationLabel);

    QHBoxLayout* guessLayout = new QHBoxLayout();
    guessLayout->setContentsMargins(0, 0, 0, 0);
    guessLayout->setSpacing(8);

    nodeTypeGuessButton_ = new QPushButton(QStringLiteral("Run Guess"), nodeTypeDialog_);
    nodeTypeGuessButton_->setObjectName(QStringLiteral("actionButton"));
    nodeTypeGuessButton_->setFixedHeight(22);
    nodeTypeGuessButton_->setToolTip(
        QStringLiteral("Scan the trace traffic and suggest a node type for every observed node ID."));
    guessLayout->addWidget(nodeTypeGuessButton_);

    nodeTypeProgressBar_ = new QProgressBar(nodeTypeDialog_);
    nodeTypeProgressBar_->setRange(0, 1000);
    nodeTypeProgressBar_->setValue(0);
    nodeTypeProgressBar_->setFixedHeight(14);
    nodeTypeProgressBar_->setVisible(false);
    guessLayout->addWidget(nodeTypeProgressBar_, 1);

    nodeTypeStatusLabel_ = new QLabel(nodeTypeDialog_);
    nodeTypeStatusLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    nodeTypeStatusLabel_->setWordWrap(true);
    guessLayout->addWidget(nodeTypeStatusLabel_, 2);
    layout->addLayout(guessLayout);

    nodeTypeTable_ = new QTableWidget(0, 4, nodeTypeDialog_);
    QStringList headers;
    for (int column = 0; column < 4; ++column) {
        headers.append(nodeTypeAssignmentColumnName(column));
    }
    nodeTypeTable_->setHorizontalHeaderLabels(headers);
    nodeTypeTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    nodeTypeTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    nodeTypeTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    nodeTypeTable_->setAlternatingRowColors(true);
    nodeTypeTable_->verticalHeader()->hide();
    nodeTypeTable_->horizontalHeader()->setStretchLastSection(false);
    nodeTypeTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    nodeTypeTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    nodeTypeTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    nodeTypeTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    nodeTypeTable_->setMinimumHeight(260);
    layout->addWidget(nodeTypeTable_, 1);

    QDialogButtonBox* buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Close, nodeTypeDialog_);
    QPushButton* applyButton = buttonBox->addButton(QStringLiteral("Apply"),
                                                    QDialogButtonBox::AcceptRole);
    applyButton->setObjectName(QStringLiteral("actionButton"));
    applyButton->setFixedHeight(22);
    if (QPushButton* closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setObjectName(QStringLiteral("actionButton"));
        closeButton->setFixedHeight(22);
    }
    layout->addWidget(buttonBox);

    connect(nodeTypeGuessButton_, &QPushButton::clicked, this, &MainWindow::startNodeTypeGuess);
    connect(buttonBox, &QDialogButtonBox::rejected, nodeTypeDialog_, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, nodeTypeDialog_, [this]() {
        if (!nodeTypeTable_ || !currentTraceSession_) {
            return;
        }

        const std::unordered_map<quint16, CLog::NodeType>& fileTypes =
            currentTraceSession_->fileNodeTypes();

        std::unordered_map<quint16, CLog::NodeType> assignments;
        for (int row = 0; row < nodeTypeTable_->rowCount(); ++row) {
            const QTableWidgetItem* idItem = nodeTypeTable_->item(row, 0);
            auto* combo = qobject_cast<QComboBox*>(nodeTypeTable_->cellWidget(row, 3));
            if (!idItem || !combo) {
                continue;
            }
            const auto nodeId = static_cast<quint16>(idItem->data(Qt::UserRole).toUInt());
            const std::optional<CLog::NodeType> selected = NodeTypeFromString(combo->currentText());
            const auto fileIt = fileTypes.find(nodeId);
            if (fileIt != fileTypes.cend()) {
                if (selected.has_value() && *selected != fileIt->second) {
                    assignments[nodeId] = *selected;
                }
            } else if (selected.has_value()) {
                assignments[nodeId] = *selected;
            }
        }

        applyNodeTypeAssignments(assignments);
        rebuildNodeTypeAssignmentTable();
    });
    connect(nodeTypeDialog_, &QDialog::destroyed, this, [this]() {
        nodeTypeDialog_ = nullptr;
        nodeTypeTable_ = nullptr;
        nodeTypeGuessButton_ = nullptr;
        nodeTypeProgressBar_ = nullptr;
        nodeTypeStatusLabel_ = nullptr;
    });

    nodeTypeDialog_->resize(760, 520);
    rebuildNodeTypeAssignmentTable();
    if (const TraceViewSession* session = activeTraceViewSession()) {
        updateNodeTypeGuessProgressWidgets(*session);
    }
    nodeTypeDialog_->show();
}

void MainWindow::rebuildNodeTypeAssignmentTable()
{
    if (!nodeTypeTable_) {
        return;
    }

    nodeTypeTable_->setRowCount(0);

    TraceViewSession* session = activeTraceViewSession();
    if (!session || !session->traceSession) {
        return;
    }

    const std::unordered_map<quint16, CLog::NodeType>& fileTypes =
        session->traceSession->fileNodeTypes();
    const std::unordered_map<quint16, CLog::NodeType>& effectiveTypes =
        session->traceSession->metadata().nodeTypes;

    std::unordered_map<quint16, const NodeTypeGuess*> guessesByNode;
    for (const NodeTypeGuess& guess : session->nodeTypeGuessResults) {
        guessesByNode.emplace(guess.nodeId, &guess);
    }

    std::set<quint16> nodeIds;
    for (const auto& [nodeId, unused] : effectiveTypes) {
        (void) unused;
        nodeIds.insert(nodeId);
    }
    for (const auto& [nodeId, unused] : guessesByNode) {
        (void) unused;
        nodeIds.insert(nodeId);
    }

    int row = 0;
    for (const quint16 nodeId : nodeIds) {
        const auto fileIt = fileTypes.find(nodeId);
        const auto effectiveIt = effectiveTypes.find(nodeId);
        const auto guessIt = guessesByNode.find(nodeId);
        const NodeTypeGuess* guess = guessIt != guessesByNode.cend() ? guessIt->second : nullptr;

        nodeTypeTable_->insertRow(row);

        auto* idItem = new QTableWidgetItem(QString::number(nodeId));
        idItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        idItem->setData(Qt::UserRole, static_cast<uint>(nodeId));
        nodeTypeTable_->setItem(row, 0, idItem);

        const QString fileTypeName = fileIt != fileTypes.cend()
            ? QString::fromStdString(CLog::NodeTypeToString(fileIt->second))
            : QStringLiteral("Unmapped");
        auto* fileItem = new QTableWidgetItem(fileTypeName);
        nodeTypeTable_->setItem(row, 1, fileItem);

        QString guessedName = QStringLiteral("-");
        if (guess && guess->type.has_value()) {
            guessedName = QStringLiteral("%1 (%2)")
                .arg(QString::fromStdString(CLog::NodeTypeToString(*guess->type)),
                     NodeTypeGuessConfidenceToString(guess->confidence));
        }
        auto* guessItem = new QTableWidgetItem(guessedName);
        if (guess && !guess->reasons.isEmpty()) {
            guessItem->setToolTip(guess->reasons.join(QLatin1Char('\n')));
        }
        nodeTypeTable_->setItem(row, 2, guessItem);

        auto* combo = new QComboBox(nodeTypeTable_);
        if (fileIt == fileTypes.cend()) {
            combo->addItem(QStringLiteral("Unmapped"));
        }
        for (const CLog::NodeType type : AllNodeTypes()) {
            combo->addItem(QString::fromStdString(CLog::NodeTypeToString(type)));
        }
        QString selectedName;
        if (effectiveIt != effectiveTypes.cend()) {
            selectedName = QString::fromStdString(CLog::NodeTypeToString(effectiveIt->second));
        } else if (guess && guess->type.has_value()) {
            selectedName = QString::fromStdString(CLog::NodeTypeToString(*guess->type));
        } else {
            selectedName = QStringLiteral("Unmapped");
        }
        const int selectedIndex = combo->findText(selectedName);
        combo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
        if (guess && guess->type.has_value()) {
            combo->setToolTip(guess->reasons.join(QLatin1Char('\n')));
        }
        nodeTypeTable_->setCellWidget(row, 3, combo);

        ++row;
    }
}

void MainWindow::startNodeTypeGuess()
{
    TraceViewSession* targetSession = activeTraceViewSession();
    if (!targetSession || !targetSession->traceSession || targetSession->nodeTypeGuessActive) {
        return;
    }

    cancelNodeTypeGuessForSession(*targetSession);
    targetSession->nodeTypeGuesser.clear();
    targetSession->nodeTypeGuessResults.clear();
    targetSession->nodeTypeGuessComputed = false;
    targetSession->nodeTypeGuessError = {};
    targetSession->nodeTypeGuessNextBlockIndex = 0;
    targetSession->nodeTypeGuessNextLocalIndex = 0;
    targetSession->nodeTypeGuessProgressText.clear();
    targetSession->nodeTypeGuessProgressValue = 0;
    targetSession->nodeTypeGuessProgressMaximum = 1000;
    targetSession->nodeTypeGuessTraceSessionSource = targetSession->traceSession;
    targetSession->nodeTypeGuessActive = true;
    ++targetSession->nodeTypeGuessGeneration;

    updateNodeTypeGuessProgressWidgets(*targetSession);

    const quint64 sessionId = targetSession->id;
    const quint64 generation = targetSession->nodeTypeGuessGeneration;
    QTimer::singleShot(0, this, [this, sessionId, generation]() {
        processNodeTypeGuessChunk(sessionId, generation);
    });
}

void MainWindow::cancelNodeTypeGuess()
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        cancelNodeTypeGuessForSession(*session);
    }
}

void MainWindow::cancelNodeTypeGuessForSession(TraceViewSession& session)
{
    ++session.nodeTypeGuessGeneration;
    session.nodeTypeGuessActive = false;
    session.nodeTypeGuessTraceSessionSource.reset();
    session.nodeTypeGuessNextBlockIndex = 0;
    session.nodeTypeGuessNextLocalIndex = 0;
    session.nodeTypeGuessProgressText.clear();
    session.nodeTypeGuessProgressValue = 0;
    session.nodeTypeGuessProgressMaximum = 1000;

    if (session.id == activeTraceSessionId_) {
        updateNodeTypeGuessProgressWidgets(session);
    }
}

void MainWindow::processNodeTypeGuessChunk(const quint64 sessionId, const quint64 generation)
{
    TraceViewSession* targetSession = traceViewSessionById(sessionId);
    if (!targetSession
        || !targetSession->nodeTypeGuessActive
        || generation != targetSession->nodeTypeGuessGeneration
        || !targetSession->nodeTypeGuessTraceSessionSource) {
        return;
    }

    QElapsedTimer budget;
    budget.start();

    const std::shared_ptr<TraceSession>& source = targetSession->nodeTypeGuessTraceSessionSource;
    const CLogBTraceMetadata& metadata = source->metadata();
    const std::uint64_t totalRecords = metadata.totalRecords;

    while (targetSession->nodeTypeGuessNextBlockIndex < metadata.blocks.size()) {
        const CLogBTraceBlockSummary& block =
            metadata.blocks[targetSession->nodeTypeGuessNextBlockIndex];
        if (targetSession->nodeTypeGuessNextLocalIndex >= block.recordCount) {
            ++targetSession->nodeTypeGuessNextBlockIndex;
            targetSession->nodeTypeGuessNextLocalIndex = 0;
            continue;
        }

        const std::size_t localCount = std::min<std::size_t>(
            targetSession->nodeTypeGuessChunkRecords,
            static_cast<std::size_t>(block.recordCount) - targetSession->nodeTypeGuessNextLocalIndex);
        std::vector<CLogBTraceFastRecordInfo> fastRecords;
        if (!source->readFastRecords(targetSession->nodeTypeGuessNextBlockIndex,
                                     targetSession->nodeTypeGuessNextLocalIndex,
                                     localCount,
                                     fastRecords,
                                     targetSession->nodeTypeGuessError)) {
            targetSession->nodeTypeGuessActive = false;
            updateNodeTypeGuessProgressWidgets(*targetSession);
            const QString summary = targetSession->nodeTypeGuessError.summary.isEmpty()
                ? QStringLiteral("Guessing node types failed unexpectedly.")
                : targetSession->nodeTypeGuessError.summary;
            QMessageBox::warning(this, QStringLiteral("Node Type Guess"), summary);
            return;
        }

        for (const CLogBTraceFastRecordInfo& record : fastRecords) {
            targetSession->nodeTypeGuesser.observe(record);
        }

        targetSession->nodeTypeGuessNextLocalIndex += localCount;
        const std::uint64_t processedRecords = std::min<std::uint64_t>(
            totalRecords,
            block.rowStart + targetSession->nodeTypeGuessNextLocalIndex);
        targetSession->nodeTypeGuessProgressText =
            QStringLiteral("Scanning trace rows %1 / %2...")
                .arg(static_cast<qulonglong>(processedRecords))
                .arg(static_cast<qulonglong>(totalRecords));
        targetSession->nodeTypeGuessProgressValue = totalRecords == 0
            ? 1000
            : static_cast<int>(std::min<std::uint64_t>(
                  1000,
                  (processedRecords * 1000U) / totalRecords));
        targetSession->nodeTypeGuessProgressMaximum = 1000;

        if (targetSession->nodeTypeGuessNextLocalIndex >= block.recordCount) {
            ++targetSession->nodeTypeGuessNextBlockIndex;
            targetSession->nodeTypeGuessNextLocalIndex = 0;
        }

        if (targetSession->id == activeTraceSessionId_) {
            updateNodeTypeGuessProgressWidgets(*targetSession);
        }

        if (budget.elapsed() >= kNodeTypeGuessBudgetMs) {
            break;
        }
    }

    targetSession = traceViewSessionById(sessionId);
    if (!targetSession
        || !targetSession->nodeTypeGuessActive
        || generation != targetSession->nodeTypeGuessGeneration) {
        return;
    }

    const bool finished =
        targetSession->nodeTypeGuessNextBlockIndex >= metadata.blocks.size();
    if (!finished) {
        QTimer::singleShot(0, this, [this, sessionId, generation]() {
            processNodeTypeGuessChunk(sessionId, generation);
        });
        return;
    }

    targetSession->nodeTypeGuessResults =
        targetSession->nodeTypeGuesser.results(metadata);
    targetSession->nodeTypeGuessActive = false;
    targetSession->nodeTypeGuessComputed = true;
    targetSession->nodeTypeGuessTraceSessionSource.reset();
    targetSession->nodeTypeGuessProgressText.clear();
    targetSession->nodeTypeGuessProgressValue = 1000;
    targetSession->nodeTypeGuessProgressMaximum = 1000;

    if (targetSession->id == activeTraceSessionId_) {
        updateNodeTypeGuessProgressWidgets(*targetSession);
        rebuildNodeTypeAssignmentTable();
        refreshTopologyView();
    }
}

void MainWindow::updateNodeTypeGuessProgressWidgets(const TraceViewSession& session)
{
    if (!nodeTypeDialog_) {
        return;
    }

    if (nodeTypeGuessButton_) {
        nodeTypeGuessButton_->setEnabled(!session.nodeTypeGuessActive
                                         && session.traceSession != nullptr);
        nodeTypeGuessButton_->setText(session.nodeTypeGuessActive
                                          ? QStringLiteral("Scanning...")
                                          : (session.nodeTypeGuessComputed
                                                 ? QStringLiteral("Re-run Guess")
                                                 : QStringLiteral("Run Guess")));
    }
    if (nodeTypeProgressBar_) {
        nodeTypeProgressBar_->setVisible(session.nodeTypeGuessActive);
        nodeTypeProgressBar_->setMaximum(session.nodeTypeGuessProgressMaximum);
        nodeTypeProgressBar_->setValue(session.nodeTypeGuessProgressValue);
    }
    if (nodeTypeStatusLabel_) {
        if (session.nodeTypeGuessActive) {
            nodeTypeStatusLabel_->setText(session.nodeTypeGuessProgressText);
        } else if (session.nodeTypeGuessComputed) {
            std::size_t unmappedCount = 0;
            for (const NodeTypeGuess& guess : session.nodeTypeGuessResults) {
                if (!guess.mappedInFile) {
                    ++unmappedCount;
                }
            }
            nodeTypeStatusLabel_->setText(
                QStringLiteral("Guess complete: %1 observed node%2, %3 unmapped.")
                    .arg(static_cast<qulonglong>(session.nodeTypeGuessResults.size()))
                    .arg(session.nodeTypeGuessResults.size() == 1 ? QString() : QStringLiteral("s"))
                    .arg(static_cast<qulonglong>(unmappedCount)));
        } else {
            nodeTypeStatusLabel_->setText(
                QStringLiteral("No traffic guess has been run for this trace."));
        }
    }
}

void MainWindow::applyNodeTypeAssignments(
    const std::unordered_map<quint16, CLog::NodeType>& assignments)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || !session->traceSession) {
        return;
    }

    session->traceSession->applyNodeTypeAssignments(assignments);

    if (session->editableMetadata) {
        session->editableMetadata->nodeTypes = session->traceSession->metadata().nodeTypes;
    }

    QString saveError;
    if (!session->traceSession->saveNodeTypeOverrides(&saveError)) {
        QMessageBox::warning(this,
                             QStringLiteral("Node Types"),
                             saveError.isEmpty()
                                 ? QStringLiteral("The node type assignments could not be saved.")
                                 : saveError);
    }

    pendingTopologyAssignments_.clear();

    if (session->flitModel) {
        session->flitModel->refreshNodeLabels();
    }
    refreshTopologyView();
    updateTopologyAction();
    markDerivedViewsOutdated(*session);
    scheduleDiagnosticsSnapshotRefresh();

    statusBar()->showMessage(
        QStringLiteral("Applied %1 node type assignment%2.")
            .arg(static_cast<qulonglong>(assignments.size()))
            .arg(assignments.size() == 1 ? QString() : QStringLiteral("s")),
        3000);

    if (session->traceSession->isXactionIndexComplete()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            QStringLiteral("Node Types"),
            QStringLiteral("Node types changed. The existing Xaction index was built with the "
                           "previous topology.\n\nRebuild the Xaction index now?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            startXactionIndexing(true);
        }
    }
}

void MainWindow::updateNodeTypesAction()
{
    if (!nodeTypesAction_) {
        return;
    }

    const bool hasSession = currentTraceSession_ != nullptr;
    nodeTypesAction_->setEnabled(hasSession);
    nodeTypesAction_->setToolTip(
        hasSession
            ? QStringLiteral("Guess or assign CHI node types for observed node IDs.")
            : QStringLiteral("Open a CLog.B trace to guess or assign CHI node types."));
    nodeTypesAction_->setStatusTip(nodeTypesAction_->toolTip());
}

void MainWindow::applyTopologyPendingAssignments()
{
    if (!currentTraceSession_ || pendingTopologyAssignments_.empty()) {
        return;
    }

    std::unordered_map<quint16, CLog::NodeType> assignments =
        currentTraceSession_->nodeTypeOverrides();
    for (const auto& [nodeId, pending] : pendingTopologyAssignments_) {
        if (pending.has_value()) {
            assignments[nodeId] = *pending;
        } else {
            assignments.erase(nodeId);
        }
    }

    applyNodeTypeAssignments(assignments);
}

void MainWindow::revertTopologyPendingAssignments()
{
    if (pendingTopologyAssignments_.empty()) {
        return;
    }
    pendingTopologyAssignments_.clear();
    refreshTopologyView();
}

}  // namespace CHIron::Gui
