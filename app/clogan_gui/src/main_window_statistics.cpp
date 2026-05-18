#include "main_window_internal.hpp"

namespace CHIron::Gui {
using namespace MainWindowDetail;

void MainWindow::resetStatisticsView(const bool cancelActiveComputation)
{
    if (cancelActiveComputation) {
        cancelStatisticsComputation();
        statisticsAccumulator_.reset();
        statisticsComputed_ = false;
    }

    const bool hasTrace = currentTraceSession_
        || flitModel_->totalCount() > 0;

    if (statisticsHintLabel_) {
        statisticsHintLabel_->setText(hasTrace
                                          ? QStringLiteral("Calculate opcode counts and overview metrics for the full loaded trace.")
                                          : QStringLiteral("Open a CHI trace to enable statistics."));
    }
    if (statisticsCalculateButton_) {
        statisticsCalculateButton_->setEnabled(hasTrace);
    }
    if (statisticsRefreshButton_) {
        statisticsRefreshButton_->setEnabled(hasTrace);
    }
    if (statisticsLoadingLabel_) {
        statisticsLoadingFullText_ = QStringLiteral("Preparing the statistics scan.");
        refreshStatisticsLoadingLabel();
    }
    if (statisticsProgressBar_) {
        statisticsProgressBar_->setRange(0, 1000);
        statisticsProgressBar_->setValue(0);
    }
    if (statisticsSummaryLabel_) {
        statisticsSummaryLabel_->setText(hasTrace
                                             ? QStringLiteral("Statistics are calculated from the full loaded trace source.")
                                             : QStringLiteral("No trace is currently loaded."));
    }
    if (statisticsOverviewTable_) {
        statisticsOverviewTable_->clearContents();
        statisticsOverviewTable_->setRowCount(0);
    }
    if (statisticsOpcodeTable_) {
        statisticsOpcodeTable_->clearContents();
        statisticsOpcodeTable_->setRowCount(0);
    }
    if (statisticsStack_ && statisticsIdleState_) {
        statisticsStack_->setCurrentWidget(statisticsIdleState_);
    }
}

void MainWindow::startStatisticsComputation()
{
    TraceViewSession* targetSession = activeTraceViewSession();
    const bool hasTrace = targetSession
        && (targetSession->traceSession
            || (targetSession->flitModel && targetSession->flitModel->totalCount() > 0));
    if (!hasTrace || targetSession->statisticsActive) {
        return;
    }

    cancelStatisticsComputationForSession(*targetSession);
    targetSession->statisticsAccumulator.reset();
    targetSession->statisticsComputed = false;
    targetSession->statisticsResult = {};
    targetSession->statisticsError = {};
    targetSession->statisticsNextBlockIndex = 0;
    targetSession->statisticsNextLocalIndex = 0;
    targetSession->statisticsNextRowIndex = 0;

    if (targetSession->traceSession) {
        targetSession->statisticsTraceSessionSource = targetSession->traceSession;
        targetSession->statisticsAccumulator.setParameters(targetSession->traceSession->metadata().parameters);
        targetSession->statisticsAccumulator.setTimestampRange(targetSession->traceSession->metadata().firstTimestamp,
                                                              targetSession->traceSession->metadata().lastTimestamp);
        targetSession->statisticsChunkRecords = 65536;
    } else {
        targetSession->statisticsRowSource = targetSession->flitModel
            ? targetSession->flitModel->sourceRowsSnapshot()
            : std::vector<FlitRecord>{};
        targetSession->statisticsChunkRecords = 4096;
    }

    targetSession->statisticsActive = true;
    ++targetSession->statisticsGeneration;
    statisticsActive_ = true;
    statisticsComputed_ = false;
    statisticsGeneration_ = targetSession->statisticsGeneration;
    statisticsTraceSessionSource_ = targetSession->statisticsTraceSessionSource;
    updateStatisticsLoadingProgress(QStringLiteral("Scanning trace statistics..."), 0, 1000);
    if (statisticsStack_ && statisticsLoadingState_) {
        statisticsStack_->setCurrentWidget(statisticsLoadingState_);
    }

    const quint64 sessionId = targetSession->id;
    const quint64 generation = targetSession->statisticsGeneration;
    QTimer::singleShot(0, this, [this, sessionId, generation]() {
        processStatisticsComputation(sessionId, generation);
    });
}

void MainWindow::cancelStatisticsComputation()
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        cancelStatisticsComputationForSession(*session);
    } else {
        ++statisticsGeneration_;
        statisticsActive_ = false;
        statisticsTraceSessionSource_.reset();
        statisticsRowSource_.clear();
        statisticsNextBlockIndex_ = 0;
        statisticsNextLocalIndex_ = 0;
        statisticsNextRowIndex_ = 0;
    }
}

void MainWindow::cancelStatisticsComputationForSession(TraceViewSession& session)
{
    ++session.statisticsGeneration;
    session.statisticsActive = false;
    session.statisticsTraceSessionSource.reset();
    session.statisticsRowSource.clear();
    session.statisticsNextBlockIndex = 0;
    session.statisticsNextLocalIndex = 0;
    session.statisticsNextRowIndex = 0;
    session.statisticsProgressText.clear();
    session.statisticsProgressValue = 0;
    session.statisticsProgressMaximum = 1000;

    if (session.id == activeTraceSessionId_) {
        statisticsGeneration_ = session.statisticsGeneration;
        statisticsActive_ = false;
        statisticsTraceSessionSource_.reset();
        statisticsRowSource_.clear();
        statisticsNextBlockIndex_ = 0;
        statisticsNextLocalIndex_ = 0;
        statisticsNextRowIndex_ = 0;
    }
}

void MainWindow::processStatisticsComputation(const quint64 sessionId, const quint64 generation)
{
    TraceViewSession* targetSession = traceViewSessionById(sessionId);
    if (!targetSession
        || !targetSession->statisticsActive
        || generation != targetSession->statisticsGeneration) {
        return;
    }

    const bool targetIsActive = targetSession->id == activeTraceSessionId_;
    QElapsedTimer budget;
    budget.start();
    constexpr qint64 kStatisticsBudgetMs = 8;

    const auto postProgress = [&](const QString& text, const int value, const int maximum) {
        targetSession->statisticsProgressText = text;
        targetSession->statisticsProgressValue = value;
        targetSession->statisticsProgressMaximum = maximum;
        if (targetIsActive) {
            statisticsActive_ = targetSession->statisticsActive;
            statisticsComputed_ = targetSession->statisticsComputed;
            statisticsGeneration_ = targetSession->statisticsGeneration;
            updateStatisticsLoadingProgress(text, value, maximum);
        }
    };

    if (targetSession->statisticsTraceSessionSource) {
        const CLogBTraceMetadata& metadata = targetSession->statisticsTraceSessionSource->metadata();
        const std::uint64_t totalRecords = metadata.totalRecords;

        while (targetSession->statisticsNextBlockIndex < metadata.blocks.size()) {
            const CLogBTraceBlockSummary& block = metadata.blocks[targetSession->statisticsNextBlockIndex];
            if (targetSession->statisticsNextLocalIndex >= block.recordCount) {
                ++targetSession->statisticsNextBlockIndex;
                targetSession->statisticsNextLocalIndex = 0;
                continue;
            }

            const std::size_t localCount = std::min<std::size_t>(
                targetSession->statisticsChunkRecords,
                static_cast<std::size_t>(block.recordCount) - targetSession->statisticsNextLocalIndex);
            std::vector<CLogBTraceFastRecordInfo> fastRecords;
            if (!targetSession->statisticsTraceSessionSource->readFastRecords(targetSession->statisticsNextBlockIndex,
                                                                              targetSession->statisticsNextLocalIndex,
                                                                              localCount,
                                                                              fastRecords,
                                                                              targetSession->statisticsError)) {
                targetSession->statisticsActive = false;
                const QString summary = targetSession->statisticsError.summary.isEmpty()
                    ? QStringLiteral("Calculating statistics failed unexpectedly.")
                    : targetSession->statisticsError.summary;
                const QString detail = targetSession->statisticsError.informativeText.isEmpty()
                    ? targetSession->statisticsError.detailedText
                    : targetSession->statisticsError.informativeText;
                if (targetIsActive) {
                    statisticsActive_ = false;
                    resetStatisticsView(false);
                    QMessageBox::warning(this,
                                         QStringLiteral("Statistics"),
                                         detail.isEmpty()
                                             ? summary
                                             : summary + QStringLiteral("\n\n") + detail);
                }
                return;
            }

            for (const CLogBTraceFastRecordInfo& record : fastRecords) {
                targetSession->statisticsAccumulator.addFastRecord(static_cast<CLog::Channel>(record.channel), record);
            }

            targetSession->statisticsNextLocalIndex += localCount;
            const std::uint64_t processedRecords = std::min<std::uint64_t>(
                totalRecords,
                block.rowStart + targetSession->statisticsNextLocalIndex);
            postProgress(
                QStringLiteral("Scanning statistics rows %1 / %2...")
                    .arg(static_cast<qulonglong>(processedRecords))
                    .arg(static_cast<qulonglong>(totalRecords)),
                totalRecords == 0
                    ? 1000
                    : static_cast<int>(std::min<std::uint64_t>(
                        1000,
                        (processedRecords * 1000U) / totalRecords)),
                1000);

            if (targetSession->statisticsNextLocalIndex >= block.recordCount) {
                ++targetSession->statisticsNextBlockIndex;
                targetSession->statisticsNextLocalIndex = 0;
            }

            if (budget.elapsed() >= kStatisticsBudgetMs) {
                break;
            }
        }
    } else {
        const std::size_t totalRows = targetSession->statisticsRowSource.size();
        while (targetSession->statisticsNextRowIndex < totalRows) {
            const std::size_t rowEnd = std::min(totalRows,
                                                targetSession->statisticsNextRowIndex
                                                    + targetSession->statisticsChunkRecords);
            for (std::size_t rowIndex = targetSession->statisticsNextRowIndex; rowIndex < rowEnd; ++rowIndex) {
                targetSession->statisticsAccumulator.addRow(targetSession->statisticsRowSource[rowIndex]);
            }
            targetSession->statisticsNextRowIndex = rowEnd;

            postProgress(
                QStringLiteral("Scanning statistics rows %1 / %2...")
                    .arg(static_cast<qulonglong>(targetSession->statisticsNextRowIndex))
                    .arg(static_cast<qulonglong>(totalRows)),
                totalRows == 0
                    ? 1000
                    : static_cast<int>(std::min<std::size_t>(
                        1000,
                        (targetSession->statisticsNextRowIndex * 1000U) / totalRows)),
                1000);

            if (budget.elapsed() >= kStatisticsBudgetMs) {
                break;
            }
        }
    }

    targetSession = traceViewSessionById(sessionId);
    if (!targetSession
        || !targetSession->statisticsActive
        || generation != targetSession->statisticsGeneration) {
        return;
    }

    const bool finished = targetSession->statisticsTraceSessionSource
        ? targetSession->statisticsNextBlockIndex >= targetSession->statisticsTraceSessionSource->metadata().blocks.size()
        : targetSession->statisticsNextRowIndex >= targetSession->statisticsRowSource.size();
    if (!finished) {
        QTimer::singleShot(0, this, [this, sessionId, generation]() {
            processStatisticsComputation(sessionId, generation);
        });
        return;
    }

    const TraceStatisticsResult result = targetSession->statisticsAccumulator.finalize();
    targetSession->statisticsActive = false;
    targetSession->statisticsTraceSessionSource.reset();
    targetSession->statisticsRowSource.clear();
    targetSession->statisticsComputed = true;
    targetSession->statisticsResult = result;
    targetSession->statisticsProgressText.clear();
    targetSession->statisticsProgressValue = 1000;
    targetSession->statisticsProgressMaximum = 1000;
    if (targetSession->id == activeTraceSessionId_) {
        statisticsActive_ = false;
        statisticsComputed_ = true;
        statisticsTraceSessionSource_.reset();
        statisticsRowSource_.clear();
        statisticsGeneration_ = targetSession->statisticsGeneration;
        applyStatisticsResult(result);
    }
}

void MainWindow::applyStatisticsResult(const TraceStatisticsResult& statistics)
{
    statisticsComputed_ = true;
    if (TraceViewSession* session = activeTraceViewSession()) {
        session->statisticsComputed = true;
        session->statisticsActive = false;
        session->statisticsResult = statistics;
        session->statisticsTraceSessionSource.reset();
        session->statisticsRowSource.clear();
        session->statisticsProgressText.clear();
        session->statisticsProgressValue = 1000;
        session->statisticsProgressMaximum = 1000;
    }

    if (statisticsSummaryLabel_) {
        statisticsSummaryLabel_->setText(
            QStringLiteral("Calculated from the full loaded trace. %1 opcode entries were found.")
                .arg(static_cast<qulonglong>(statistics.opcodeCounts.size())));
    }

    if (statisticsOverviewTable_) {
        const QString timestampRange = statistics.totalFlits == 0
            ? QStringLiteral("-")
            : QStringLiteral("%1 .. %2")
                  .arg(FormatTimestampForDisplay(statistics.firstTimestamp),
                       FormatTimestampForDisplay(statistics.lastTimestamp));

        std::vector<std::pair<QString, QString>> rows = {
            {QStringLiteral("Total Flits"), StatisticsCountText(statistics.totalFlits)},
            {QStringLiteral("TX Flits"), StatisticsCountText(statistics.txFlits)},
            {QStringLiteral("RX Flits"), StatisticsCountText(statistics.rxFlits)},
            {QStringLiteral("REQ Flits"), StatisticsCountText(statistics.channelCounts[0])},
            {QStringLiteral("RSP Flits"), StatisticsCountText(statistics.channelCounts[1])},
            {QStringLiteral("DAT Flits"), StatisticsCountText(statistics.channelCounts[2])},
            {QStringLiteral("SNP Flits"), StatisticsCountText(statistics.channelCounts[3])},
            {QStringLiteral("Addressed Flits"), StatisticsCountText(statistics.addressedFlits)},
            {QStringLiteral("DBID Flits"), StatisticsCountText(statistics.dbidFlits)},
            {QStringLiteral("Unique Source IDs"), StatisticsCountText(statistics.uniqueSourceCount)},
            {QStringLiteral("Unique Target IDs"), StatisticsCountText(statistics.uniqueTargetCount)},
            {QStringLiteral("Unique Opcode Entries"), StatisticsCountText(statistics.uniqueOpcodeCount)},
            {QStringLiteral("Timestamp Range"), timestampRange},
        };

        if (!statistics.issue.isEmpty()) {
            rows.emplace_back(QStringLiteral("CHI Issue"), statistics.issue);
            rows.emplace_back(QStringLiteral("NodeIDWidth"), QString::number(static_cast<qulonglong>(statistics.nodeIdWidth)));
            rows.emplace_back(QStringLiteral("ReqAddrWidth"), QString::number(static_cast<qulonglong>(statistics.reqAddrWidth)));
            rows.emplace_back(QStringLiteral("DataWidth"), QString::number(static_cast<qulonglong>(statistics.dataWidth)));
            rows.emplace_back(QStringLiteral("DataCheck"), StatisticsBoolText(statistics.dataCheckPresent));
            rows.emplace_back(QStringLiteral("Poison"), StatisticsBoolText(statistics.poisonPresent));
            rows.emplace_back(QStringLiteral("MPAM"), StatisticsBoolText(statistics.mpamPresent));
        }

        statisticsOverviewTable_->setRowCount(static_cast<int>(rows.size()));
        for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
            QTableWidgetItem* labelItem = new QTableWidgetItem(rows[static_cast<std::size_t>(row)].first);
            QTableWidgetItem* valueItem = new QTableWidgetItem(rows[static_cast<std::size_t>(row)].second);
            labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            statisticsOverviewTable_->setItem(row, 0, labelItem);
            statisticsOverviewTable_->setItem(row, 1, valueItem);
        }
        statisticsOverviewTable_->resizeRowsToContents();
    }

    if (statisticsOpcodeTable_) {
        statisticsOpcodeTable_->setRowCount(static_cast<int>(statistics.opcodeCounts.size()));
        for (int row = 0; row < static_cast<int>(statistics.opcodeCounts.size()); ++row) {
            const TraceStatisticsOpcodeCount& count = statistics.opcodeCounts[static_cast<std::size_t>(row)];
            auto* directionItem = new QTableWidgetItem(count.direction);
            auto* channelItem = new QTableWidgetItem(count.channel);
            auto* opcodeItem = new QTableWidgetItem(count.opcode);
            auto* countItem = new QTableWidgetItem(StatisticsCountText(count.count));
            directionItem->setFlags(directionItem->flags() & ~Qt::ItemIsEditable);
            channelItem->setFlags(channelItem->flags() & ~Qt::ItemIsEditable);
            opcodeItem->setFlags(opcodeItem->flags() & ~Qt::ItemIsEditable);
            countItem->setFlags(countItem->flags() & ~Qt::ItemIsEditable);
            countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            statisticsOpcodeTable_->setItem(row, 0, directionItem);
            statisticsOpcodeTable_->setItem(row, 1, channelItem);
            statisticsOpcodeTable_->setItem(row, 2, opcodeItem);
            statisticsOpcodeTable_->setItem(row, 3, countItem);
        }
        statisticsOpcodeTable_->resizeRowsToContents();
    }

    if (statisticsStack_ && statisticsResultsState_) {
        statisticsStack_->setCurrentWidget(statisticsResultsState_);
    }
}

void MainWindow::refreshStatisticsLoadingLabel()
{
    if (!statisticsLoadingLabel_) {
        return;
    }

    const QString text = statisticsLoadingFullText_.isEmpty()
        ? QStringLiteral("Preparing the statistics scan.")
        : statisticsLoadingFullText_;
    const int availableWidth = qMax(0, statisticsLoadingLabel_->contentsRect().width());
    const QString elided = availableWidth > 0
        ? statisticsLoadingLabel_->fontMetrics().elidedText(text, Qt::ElideRight, availableWidth)
        : text;
    statisticsLoadingLabel_->setText(elided);
    statisticsLoadingLabel_->setToolTip(text);
    statisticsLoadingLabel_->setStatusTip(text);
}

void MainWindow::updateStatisticsLoadingProgress(const QString& text,
                                                 const int value,
                                                 const int maximum)
{
    statisticsLoadingFullText_ = text;
    if (TraceViewSession* session = activeTraceViewSession()) {
        statisticsActive_ = session->statisticsActive;
        statisticsComputed_ = session->statisticsComputed;
        session->statisticsProgressText = text;
        session->statisticsProgressValue = value;
        session->statisticsProgressMaximum = maximum;
    }
    refreshStatisticsLoadingLabel();
    if (statisticsProgressBar_) {
        statisticsProgressBar_->setRange(0, qMax(0, maximum));
        statisticsProgressBar_->setValue(qBound(0, value, qMax(0, maximum)));
    }
}

}  // namespace CHIron::Gui

