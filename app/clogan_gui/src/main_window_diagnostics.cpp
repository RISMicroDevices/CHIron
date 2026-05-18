#include "cache_widget.hpp"
#include "main_window_internal.hpp"

namespace CHIron::Gui {
using namespace MainWindowDetail;

void MainWindow::exportBugReport()
{
    const QString layoutFilePath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String(kLayoutSettingsFileName));

    const BugReportExportResult result = BugReporter::exportBugReport({
        .title = QStringLiteral("CloganGUI Bug Report"),
        .summary = QStringLiteral("Manual diagnostics export triggered from the Help menu."),
        .stateText = diagnosticsStateSnapshot(),
        .layoutFilePath = layoutFilePath,
    });

    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("Export Bug Report"),
                             result.errorMessage.isEmpty()
                                 ? QStringLiteral("The bug report could not be exported.")
                                 : result.errorMessage);
        return;
    }

    statusBar()->showMessage(
        QStringLiteral("Exported bug report to %1.")
            .arg(QDir::toNativeSeparators(result.directoryPath)),
        5000);

    QMessageBox notice(this);
    notice.setIcon(QMessageBox::Information);
    notice.setWindowTitle(QStringLiteral("Bug Report Exported"));
    notice.setText(QStringLiteral("CloganGUI exported a diagnostics bundle."));
    notice.setInformativeText(
        QStringLiteral("Report directory:\n%1")
            .arg(QDir::toNativeSeparators(result.directoryPath)));
    QPushButton* openFolderButton = notice.addButton(QStringLiteral("Open Folder"),
                                                     QMessageBox::ActionRole);
    notice.addButton(QMessageBox::Close);
    notice.exec();
    if (notice.clickedButton() == openFolderButton) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(result.directoryPath));
    }
}

void MainWindow::openDiagnosticsFolder()
{
    const QString diagnosticsRootPath = BugReporter::diagnosticsRootPath();
    if (diagnosticsRootPath.isEmpty()
        || !QDesktopServices::openUrl(QUrl::fromLocalFile(diagnosticsRootPath))) {
        QMessageBox::warning(this,
                             QStringLiteral("Open Diagnostics Folder"),
                             QStringLiteral("The diagnostics folder could not be opened."));
        return;
    }

    statusBar()->showMessage(
        QStringLiteral("Opened diagnostics folder: %1")
            .arg(QDir::toNativeSeparators(diagnosticsRootPath)),
        3000);
}


void MainWindow::showAboutDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("About CloganGUI"));
    dialog.resize(640, 220);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(14);

    QLabel* iconLabel = new QLabel(&dialog);
    iconLabel->setPixmap(QApplication::windowIcon().pixmap(80, 80));
    iconLabel->setFixedSize(88, 88);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet(
        QStringLiteral("background:#F4F7FA; border:1px solid #D6DEE5; border-radius:8px;"));
    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(4);

    QLabel* titleLabel = new QLabel(QStringLiteral("CloganGUI"), &dialog);
    titleLabel->setStyleSheet(QStringLiteral("font-size:20px; font-weight:700; color:#18212B;"));
    titleLayout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(
        QStringLiteral("Desktop CHI flit viewer built on CHIron's decode and flit layers."),
        &dialog);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(QStringLiteral("color:#58636F;"));
    titleLayout->addWidget(subtitleLabel);

    QLabel* codenameLabel = new QLabel(
        QStringLiteral("Toolset generation codename: Cyanopica Cyanus"),
        &dialog);
    codenameLabel->setWordWrap(true);
    codenameLabel->setStyleSheet(QStringLiteral("color:#4F5D68; font-weight:600;"));
    titleLayout->addWidget(codenameLabel);

    headerLayout->addLayout(titleLayout, 1);
    layout->addLayout(headerLayout);
    layout->addStretch(1);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    if (QPushButton* closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setObjectName(QStringLiteral("actionButton"));
        closeButton->setFixedHeight(22);
    }
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    dialog.exec();
}


void MainWindow::scheduleDiagnosticsSnapshotRefresh()
{
    if (!diagnosticsSnapshotTimer_) {
        return;
    }

    diagnosticsSnapshotTimer_->start();
}

void MainWindow::refreshDiagnosticsSnapshot()
{
    BugReporter::updateLiveState(diagnosticsStateSnapshot());
}

namespace {

QString DiagnosticsVariantMapSummary(const QVariantMap& state)
{
    if (state.isEmpty()) {
        return QStringLiteral("(empty)");
    }

    QStringList entries;
    entries.reserve(state.size());
    for (auto it = state.cbegin(); it != state.cend(); ++it) {
        const QVariant& value = it.value();
        QString valueText;
        if (value.typeId() == QMetaType::Bool) {
            valueText = BoolText(value.toBool());
        } else if (value.canConvert<QString>()) {
            valueText = value.toString();
        } else {
            valueText = QString::fromUtf8(value.typeName() ? value.typeName() : "value");
        }
        if (valueText.size() > 48) {
            valueText = valueText.left(45) + QStringLiteral("...");
        }
        entries.append(QStringLiteral("%1=%2").arg(it.key(), valueText));
    }
    return entries.join(QStringLiteral(", "));
}

}  // namespace

QString MainWindow::diagnosticsStateSnapshot() const
{
    QString text;
    QTextStream stream(&text);

    const ProcessMemorySample memory = QueryProcessMemory();
    const QModelIndex currentIndex = flitView_ && flitView_->selectionModel()
        ? flitView_->selectionModel()->currentIndex()
        : QModelIndex();
    const FlitRecord* selectedRecord = currentIndex.isValid()
        ? flitModel_->recordAt(currentIndex.row())
        : nullptr;

    QStringList visibleOptionalFields;
    QStringList optionalFieldFilters;
    QStringList indexedOptionalFields;
    const QStringList optionalFields = flitModel_->availableOptionalFields();
    for (const QString& fieldName : optionalFields) {
        if (flitModel_->isFieldColumnVisible(fieldName)) {
            visibleOptionalFields.append(fieldName);
        }
        const QString filterText = flitModel_->fieldFilter(fieldName);
        if (!filterText.isEmpty()) {
            optionalFieldFilters.append(QStringLiteral("%1=%2").arg(fieldName, filterText));
        }
        if (currentTraceSession_ && currentTraceSession_->hasOptionalFieldFastIndex(fieldName)) {
            indexedOptionalFields.append(fieldName);
        }
    }

    stream << "Timestamp: " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << '\n';
    stream << "Window Title: " << windowTitle() << '\n';
    stream << "Window Size: " << width() << 'x' << height() << '\n';
    stream << "Window State: maximized=" << BoolText(isMaximized())
           << ", minimized=" << BoolText(isMinimized())
           << ", fullScreen=" << BoolText(isFullScreen()) << '\n';
    stream << "Current Trace Label: " << currentTraceLabel_ << '\n';
    stream << "Current Trace Path: " << QDir::toNativeSeparators(currentTracePath_) << '\n';
    stream << "Pending Trace Load: " << QDir::toNativeSeparators(pendingTraceLoadPath_) << '\n';
    stream << "Active Session ID: " << static_cast<qulonglong>(activeTraceSessionId_) << '\n';
    stream << "Open Session Count: " << traceSessions_.size() << '\n';
    stream << "Session Backed: " << BoolText(currentTraceSession_ != nullptr) << '\n';
    stream << "Rows: total=" << flitModel_->totalCount()
           << ", visible=" << flitModel_->visibleCount()
           << ", identityVisibleRows=" << BoolText(flitModel_->isIdentityVisibleRows()) << '\n';
    stream << "Memory: working=" << FormatBytes(memory.workingSetBytes)
           << ", private=" << FormatBytes(memory.privateBytes) << '\n';
    stream << "Selection: visibleRow=" << (currentIndex.isValid() ? QString::number(currentIndex.row()) : QStringLiteral("-1"));
    if (currentTraceSession_ && currentIndex.isValid()) {
        stream << ", logicalRow=" << flitModel_->logicalRowAt(currentIndex.row());
    }
    stream << '\n';

    stream << '\n';
    stream << "Open Trace Sessions" << '\n';
    stream << "-------------------" << '\n';
    if (traceSessions_.empty()) {
        stream << "(none)" << '\n';
    }
    for (std::size_t index = 0; index < traceSessions_.size(); ++index) {
        const std::unique_ptr<TraceViewSession>& session = traceSessions_[index];
        if (!session) {
            continue;
        }

        const bool active = session->id == activeTraceSessionId_;
        stream << "Session[" << index << "]: id=" << static_cast<qulonglong>(session->id)
               << ", active=" << BoolText(active)
               << ", label=\"" << session->label << '"'
               << ", path=\"" << QDir::toNativeSeparators(session->path) << '"'
               << ", rowBacked=" << BoolText(session->rowBacked)
               << ", sessionBacked=" << BoolText(session->traceSession != nullptr)
               << '\n';
        const qulonglong sessionTotalRows = session->traceSession
            ? static_cast<qulonglong>(session->traceSession->totalRows())
            : (session->flitModel ? static_cast<qulonglong>(session->flitModel->totalCount()) : 0ULL);
        const int sessionVisibleRows = session->flitModel ? session->flitModel->visibleCount() : 0;
        stream << "  Rows: total=" << sessionTotalRows
               << ", visible=" << sessionVisibleRows
               << ", selectedLogicalRow=" << session->selectedLogicalRow << '\n';
        stream << "  Background: statisticsActive=" << BoolText(session->statisticsActive)
               << ", statisticsComputed=" << BoolText(session->statisticsComputed)
               << ", statisticsProgress=\"" << session->statisticsProgressText << '"'
               << ", xactionIndexActive=" << BoolText(session->xactionIndexActive)
               << ", xactionIndexProgress=\"" << session->xactionIndexProgressText << '"'
               << ", optionalFieldIndexing="
               << (session->optionalFieldIndexingFields.isEmpty()
                       ? QStringLiteral("(none)")
                       : QStringList(session->optionalFieldIndexingFields.begin(),
                                     session->optionalFieldIndexingFields.end()).join(QStringLiteral(", ")))
               << '\n';
        stream << "  View State: timeline="
               << DiagnosticsVariantMapSummary(active && session->timelineWidget
                                                 ? session->timelineWidget->viewState()
                                                 : session->timelineViewState)
               << '\n';
        stream << "  View State: address="
               << DiagnosticsVariantMapSummary(active && session->addressWidget
                                                 ? session->addressWidget->viewState()
                                                 : session->addressViewState)
               << '\n';
        stream << "  View State: cache="
               << DiagnosticsVariantMapSummary(active && session->cacheWidget
                                                 ? session->cacheWidget->diagnosticsState()
                                                 : session->cacheViewState)
               << '\n';
        stream << "  View State: latency="
               << DiagnosticsVariantMapSummary(active && session->latencyWidget
                                                 ? session->latencyWidget->viewState()
                                                 : session->latencyViewState)
               << '\n';
        stream << "  View State: transaction="
               << DiagnosticsVariantMapSummary(active && session->transactionWidget
                                                 ? session->transactionWidget->diagnosticsState()
                                                 : session->transactionViewState)
               << '\n';
        stream << "  Transaction Widget: instanceId="
               << static_cast<qulonglong>(
                      reinterpret_cast<std::uintptr_t>(session->transactionWidget) & 0x7FFFFFFFULL)
               << ", activeWidget=" << BoolText(active && session->transactionWidget == transactionWidget_)
               << ", hasWidget=" << BoolText(session->transactionWidget != nullptr)
               << '\n';
        stream << "  Cache Widget: instanceId="
               << static_cast<qulonglong>(
                      reinterpret_cast<std::uintptr_t>(session->cacheWidget) & 0x7FFFFFFFULL)
               << ", activeWidget=" << BoolText(active && session->cacheWidget == cacheWidget_)
               << ", hasWidget=" << BoolText(session->cacheWidget != nullptr)
               << '\n';
        if (session->traceSession) {
            const CLogBTraceMetadata& metadata = session->traceSession->metadata();
            QStringList sessionIndexedOptionalFields;
            if (session->flitModel) {
                const QStringList fields = session->flitModel->availableOptionalFields();
                for (const QString& fieldName : fields) {
                    if (session->traceSession->hasOptionalFieldFastIndex(fieldName)) {
                        sessionIndexedOptionalFields.append(fieldName);
                    }
                }
            }
            stream << "  Sidecar Indexes: fast=\"" << QDir::toNativeSeparators(session->traceSession->fastIndexPath())
                   << "\", xaction=\"" << QDir::toNativeSeparators(session->traceSession->filePath() + QStringLiteral(".xactidx"))
                   << "\", optionalFields="
                   << (sessionIndexedOptionalFields.isEmpty()
                           ? QStringLiteral("(none)")
                           : sessionIndexedOptionalFields.join(QStringLiteral(", ")))
                   << '\n';
            stream << "  Trace Summary: blocks=" << metadata.blocks.size()
                   << ", records=" << static_cast<qulonglong>(metadata.totalRecords)
                   << ", timestampRange=" << metadata.firstTimestamp << ".." << metadata.lastTimestamp
                   << '\n';
        }
    }

    stream << '\n';
    stream << "Default Filters" << '\n';
    stream << "---------------" << '\n';
    stream << "SearchMode=" << (flitModel_->searchMode() == FlitTableModel::SearchMode::Highlight
                                  ? QStringLiteral("Highlight")
                                  : QStringLiteral("Filter")) << '\n';
    stream << "XactionDebugMode=" << BoolText(xactionDebugMode_) << '\n';
    stream << "Opcode=" << opcodeFilterEdit_->text() << '\n';
    stream << "SrcID=" << sourceIdFilterEdit_->text() << '\n';
    stream << "TgtID=" << targetIdFilterEdit_->text() << '\n';
    stream << "TxnID=" << txnIdFilterEdit_->text() << '\n';
    stream << "DBID=" << dbidFilterEdit_->text() << '\n';
    stream << "Addr=" << addressFilterEdit_->text() << '\n';
    stream << "Channels=REQ:" << BoolText(reqButton_->isChecked())
           << ", RSP:" << BoolText(rspButton_->isChecked())
           << ", DAT:" << BoolText(datButton_->isChecked())
           << ", SNP:" << BoolText(snpButton_->isChecked()) << '\n';
    stream << "Directions=TX:" << BoolText(txButton_->isChecked())
           << ", RX:" << BoolText(rxButton_->isChecked()) << '\n';

    stream << '\n';
    stream << "Columns" << '\n';
    stream << "-------" << '\n';
    for (int column = 0; column < FlitTableModel::ColumnCount; ++column) {
        const QString headerText = flitModel_->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        stream << headerText << ": " << DisplayModeText(flitModel_->displayMode(column)) << '\n';
    }
    stream << "Visible Optional Fields: "
           << (visibleOptionalFields.isEmpty() ? QStringLiteral("(none)") : visibleOptionalFields.join(QStringLiteral(", ")))
           << '\n';
    stream << "Optional Field Filters: "
           << (optionalFieldFilters.isEmpty() ? QStringLiteral("(none)") : optionalFieldFilters.join(QStringLiteral(", ")))
           << '\n';
    stream << "Indexed Optional Fields: "
           << (indexedOptionalFields.isEmpty() ? QStringLiteral("(none)") : indexedOptionalFields.join(QStringLiteral(", ")))
           << '\n';

    if (selectedRecord) {
        stream << '\n';
        stream << "Selected Record" << '\n';
        stream << "---------------" << '\n';
        stream << "Timestamp=" << selectedRecord->timestamp << '\n';
        stream << "Channel=" << ToString(selectedRecord->channel) << '\n';
        stream << "Direction=" << ToString(selectedRecord->direction) << '\n';
        stream << "Opcode=" << selectedRecord->opcode << '\n';
        stream << "Source=" << selectedRecord->source << '\n';
        stream << "Target=" << selectedRecord->target << '\n';
        stream << "TxnID=" << selectedRecord->txnId << '\n';
        stream << "Address=" << selectedRecord->address << '\n';
        stream << "DBID=" << selectedRecord->dbid << '\n';
        stream << "Summary=" << selectedRecord->summary << '\n';
    }

    if (currentTraceSession_) {
        const CLogBTraceMetadata& metadata = currentTraceSession_->metadata();
        stream << '\n';
        stream << "Trace Session" << '\n';
        stream << "-------------" << '\n';
        stream << "File Path: " << QDir::toNativeSeparators(currentTraceSession_->filePath()) << '\n';
        stream << "Fast Index Path: " << QDir::toNativeSeparators(currentTraceSession_->fastIndexPath()) << '\n';
        stream << "Fast Index Readable: " << BoolText(currentTraceSession_->isFastIndexReadable()) << '\n';
        stream << "Fast Index Writable: " << BoolText(currentTraceSession_->isFastIndexWritable()) << '\n';
        stream << "Blocks: " << metadata.blocks.size() << '\n';
        stream << "Records: " << static_cast<qulonglong>(metadata.totalRecords) << '\n';
        stream << "Timestamp Range: " << metadata.firstTimestamp << " .. " << metadata.lastTimestamp << '\n';
        stream << "Channel Counts: REQ=" << static_cast<qulonglong>(metadata.channelCounts[0])
               << ", RSP=" << static_cast<qulonglong>(metadata.channelCounts[1])
               << ", DAT=" << static_cast<qulonglong>(metadata.channelCounts[2])
               << ", SNP=" << static_cast<qulonglong>(metadata.channelCounts[3]) << '\n';
        stream << "Page Cache: size=" << currentTraceSession_->pageSize()
               << ", cached=" << currentTraceSession_->cachedPageCount()
               << ", max=" << currentTraceSession_->maxCachedPages() << '\n';
        stream << "Block Cache: cached=" << currentTraceSession_->cachedBlockCount()
               << ", max=" << currentTraceSession_->maxCachedBlocks() << '\n';
        stream << "Node Annotations: " << metadata.nodeAnnotations.size() << '\n';
        stream << '\n';
        stream << "Trace Parameters" << '\n';
        stream << "----------------" << '\n';
        stream << "Issue=" << IssueText(metadata.parameters.GetIssue()) << '\n';
        stream << "NodeIDWidth=" << static_cast<qulonglong>(metadata.parameters.GetNodeIdWidth()) << '\n';
        stream << "ReqAddrWidth=" << static_cast<qulonglong>(metadata.parameters.GetReqAddrWidth()) << '\n';
        stream << "ReqRSVDCWidth=" << static_cast<qulonglong>(metadata.parameters.GetReqRSVDCWidth()) << '\n';
        stream << "DatRSVDCWidth=" << static_cast<qulonglong>(metadata.parameters.GetDatRSVDCWidth()) << '\n';
        stream << "DataWidth=" << static_cast<qulonglong>(metadata.parameters.GetDataWidth()) << '\n';
        stream << "DataCheckPresent=" << BoolText(metadata.parameters.IsDataCheckPresent()) << '\n';
        stream << "PoisonPresent=" << BoolText(metadata.parameters.IsPoisonPresent()) << '\n';
        stream << "MPAMPresent=" << BoolText(metadata.parameters.IsMPAMPresent()) << '\n';
    }

    if (dockManager_) {
        stream << '\n';
        stream << "Dock Layout Bytes: " << dockManager_->saveState().size() << '\n';
    }

    return text;
}


}  // namespace CHIron::Gui

