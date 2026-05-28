#include "clog_b_trace_loader.hpp"
#include "flit_table_model.hpp"
#include "trace_session.hpp"
#include "transaction_widget.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QImage>
#include <QPainter>
#include <QTableView>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace {

struct BenchmarkOptions {
    QString filePath;
    int iterations = 5;
    int warmupIterations = 1;
    int transactionRenderRepeats = 1;
    int transactionRenderVerticalSteps = 0;
    int repeatFilters = 1;
    quint64 rowLimit = 0;
    QString opcodeFilter;
    QString sourceIdFilter;
    QString targetIdFilter;
    QString txnIdFilter;
    QString dbidFilter;
    QString addressFilter;
    bool reqOnly = false;
    bool txOnly = false;
    bool includeModel = true;
    bool includeView = true;
    bool scanOnly = false;
    bool xactionIndexOnly = false;
    bool transactionSpansOnly = false;
    bool transactionSpanScanOnly = false;
    bool clipboardBatchScanOnly = false;
    bool clipboardInsertBench = false;
    QString clipboardStoreAlgorithm = QStringLiteral("double-buffer-swap");
    bool transactionRenderOnly = false;
    bool editableModeOnly = false;
};

struct BenchmarkSample {
    qsizetype rowCount = 0;
    qint64 scanNs = 0;
    qint64 rowLoadNs = 0;
    qint64 setRowsNs = 0;
    qint64 filterNs = 0;
    qint64 resizeColumnsNs = 0;
    qint64 xactionIndexNs = 0;
    qint64 xactionIndexingNs = 0;
    qint64 xactionMergingNs = 0;
    qint64 xactionCollectingNs = 0;
    qint64 xactionFinalizingNs = 0;
    qint64 xactionFlushingRowsNs = 0;
    qint64 xactionWritingDirectoriesNs = 0;
    qint64 transactionSpanBuildNs = 0;
    int transactionSpanCount = 0;
    int transactionLaneCount = 0;
    std::size_t transactionSpanWorkerCount = 0;
    std::uint64_t transactionSpanIndexedRowCount = 0;
    double transactionSpanMetadataMs = 0.0;
    double transactionSpanScanMs = 0.0;
    double transactionSpanMergeMs = 0.0;
    double transactionSpanFirstInfoMs = 0.0;
    double transactionSpanBuildOnlyMs = 0.0;
    double transactionSpanSortMs = 0.0;
    double transactionSpanLayoutMs = 0.0;
    qint64 clipboardBatchScanNs = 0;
    qint64 clipboardBatchCollectNs = 0;
    qint64 clipboardBatchDecodeNs = 0;
    std::uint64_t clipboardBatchAnchorCount = 0;
    std::uint64_t clipboardBatchMatchingOrdinalCount = 0;
    std::uint64_t clipboardBatchCollectedRows = 0;
    std::uint64_t clipboardBatchDecodedRows = 0;
    qint64 clipboardInsertPrepareNs = 0;
    qint64 clipboardInsertPublishNs = 0;
    qint64 clipboardInsertRefreshNs = 0;
    qint64 clipboardInsertHeartbeatMaxNs = 0;
    qint64 clipboardInsertCancelNs = 0;
    std::uint64_t clipboardInsertExistingRows = 0;
    std::uint64_t clipboardInsertRows = 0;
    QString clipboardInsertAlgorithm;
    qint64 transactionRenderNs = 0;
    qint64 editablePrepareNs = 0;
    qint64 editableFirstCellNs = 0;
    qint64 transactionRenderFirstNs = 0;
    qint64 transactionRenderRepeatNs = 0;
    int transactionRenderRepeats = 1;
    int transactionRenderVerticalSteps = 0;
    int transactionRenderCandidateCount = 0;
    bool transactionRenderDense = false;
};

struct BenchmarkSeries {
    std::vector<BenchmarkSample> samples;
};

struct ClipboardBenchEntry {
    std::uint64_t logicalRow = 0;
    std::uint64_t sequence = 0;
    qint64 timestamp = 0;
};

struct ClipboardBenchSnapshot {
    std::vector<ClipboardBenchEntry> entries;
};

constexpr std::uint64_t kClipboardBenchExistingRows = 100000;
constexpr std::uint64_t kClipboardBenchInsertRows = 10000;

std::vector<ClipboardBenchEntry> makeClipboardBenchEntries(const std::uint64_t beginRow,
                                                           const std::uint64_t count,
                                                           const std::uint64_t sequenceBase)
{
    std::vector<ClipboardBenchEntry> entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        entries.push_back(ClipboardBenchEntry{
            .logicalRow = beginRow + index,
            .sequence = sequenceBase + index,
            .timestamp = static_cast<qint64>((beginRow + index) * 3U),
        });
    }
    return entries;
}

qint64 runClipboardBenchHeartbeat(QCoreApplication& app,
                                  const std::function<void()>& work)
{
    QElapsedTimer timer;
    timer.start();
    qint64 lastNs = timer.nsecsElapsed();
    qint64 maxGapNs = 0;
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, [&]() {
        const qint64 nowNs = timer.nsecsElapsed();
        maxGapNs = std::max(maxGapNs, nowNs - lastNs);
        lastNs = nowNs;
    });
    heartbeat.start(1);
    work();
    for (int spin = 0; spin < 8; ++spin) {
        app.processEvents();
    }
    heartbeat.stop();
    return maxGapNs;
}

std::vector<ClipboardBenchEntry> prepareClipboardBenchVector(
    std::vector<ClipboardBenchEntry> baseEntries,
    const std::vector<ClipboardBenchEntry>& insertRows)
{
    std::unordered_set<std::uint64_t> existingRows;
    existingRows.reserve(baseEntries.size() + insertRows.size());
    std::uint64_t nextSequence = 0;
    for (const ClipboardBenchEntry& entry : baseEntries) {
        existingRows.insert(entry.logicalRow);
        nextSequence = std::max(nextSequence, entry.sequence + 1);
    }
    baseEntries.reserve(baseEntries.size() + insertRows.size());
    for (ClipboardBenchEntry entry : insertRows) {
        if (!existingRows.insert(entry.logicalRow).second) {
            continue;
        }
        entry.sequence = nextSequence++;
        baseEntries.push_back(entry);
    }
    std::stable_sort(baseEntries.begin(), baseEntries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.logicalRow < rhs.logicalRow;
    });
    return baseEntries;
}

qint64 refreshClipboardBenchModel(QCoreApplication& app, const std::size_t entryCount)
{
    QElapsedTimer timer;
    timer.start();
    CHIron::Gui::FlitTableModel model;
    constexpr std::size_t kRefreshBudgetRows = 2048;
    const std::size_t refreshRows = std::min<std::size_t>(entryCount, 50000);
    for (std::size_t index = 0; index < refreshRows; index += kRefreshBudgetRows) {
        std::vector<CHIron::Gui::FlitRecord> chunk;
        chunk.resize(std::min<std::size_t>(kRefreshBudgetRows, refreshRows - index));
        if (index == 0) {
            model.setRows(chunk);
        } else {
            model.appendRowsIncrementally(chunk);
        }
        app.processEvents();
    }
    return timer.nsecsElapsed();
}

bool runClipboardInsertAlgorithmBench(const QString& algorithm,
                                      BenchmarkSample& sample,
                                      QTextStream& err)
{
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        err << "Clipboard insert benchmark requires a QCoreApplication.\n";
        return false;
    }
    const std::vector<ClipboardBenchEntry> baseEntries =
        makeClipboardBenchEntries(0, kClipboardBenchExistingRows, 1);
    const std::vector<ClipboardBenchEntry> insertRows =
        makeClipboardBenchEntries(kClipboardBenchExistingRows,
                                  kClipboardBenchInsertRows,
                                  kClipboardBenchExistingRows + 1);
    sample.clipboardInsertAlgorithm = algorithm;
    sample.clipboardInsertExistingRows = kClipboardBenchExistingRows;
    sample.clipboardInsertRows = kClipboardBenchInsertRows;

    QElapsedTimer timer;
    if (algorithm == QLatin1String("baseline")) {
        std::vector<ClipboardBenchEntry> target = baseEntries;
        sample.clipboardInsertHeartbeatMaxNs = runClipboardBenchHeartbeat(*app, [&]() {
            timer.restart();
            target = prepareClipboardBenchVector(std::move(target), insertRows);
            sample.clipboardInsertPrepareNs = timer.nsecsElapsed();
            timer.restart();
            sample.clipboardInsertPublishNs = timer.nsecsElapsed();
            sample.clipboardInsertRefreshNs = refreshClipboardBenchModel(*app, target.size());
        });
        return true;
    }

    if (algorithm == QLatin1String("double-buffer-swap")
        || algorithm == QLatin1String("segmented-snapshot")
        || algorithm == QLatin1String("delta-overlay")) {
        std::vector<ClipboardBenchEntry> published = baseEntries;
        std::shared_ptr<const ClipboardBenchSnapshot> snapshot =
            std::make_shared<ClipboardBenchSnapshot>(ClipboardBenchSnapshot{published});
        sample.clipboardInsertHeartbeatMaxNs = runClipboardBenchHeartbeat(*app, [&]() {
            std::vector<ClipboardBenchEntry> prepared;
            std::atomic_bool done = false;
            timer.restart();
            std::thread worker([&]() {
                prepared = prepareClipboardBenchVector(baseEntries, insertRows);
                done.store(true, std::memory_order_release);
            });
            while (!done.load(std::memory_order_acquire)) {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(1ms);
                app->processEvents();
                if (timer.elapsed() > 60000) {
                    break;
                }
            }
            if (worker.joinable()) {
                worker.join();
            }
            sample.clipboardInsertPrepareNs = timer.nsecsElapsed();
            timer.restart();
            if (algorithm == QLatin1String("double-buffer-swap")) {
                published.swap(prepared);
            } else {
                snapshot = std::make_shared<ClipboardBenchSnapshot>(
                    ClipboardBenchSnapshot{std::move(prepared)});
            }
            sample.clipboardInsertPublishNs = timer.nsecsElapsed();
            sample.clipboardInsertRefreshNs =
                refreshClipboardBenchModel(*app, snapshot ? snapshot->entries.size() : published.size());
        });
        return true;
    }

    if (algorithm == QLatin1String("single-writer-actor")
        || algorithm == QLatin1String("pipeline-parallel")
        || algorithm == QLatin1String("striped-lock-store")) {
        std::vector<ClipboardBenchEntry> published = baseEntries;
        std::mutex mutex;
        std::condition_variable cv;
        bool ready = false;
        std::vector<ClipboardBenchEntry> prepared;
        sample.clipboardInsertHeartbeatMaxNs = runClipboardBenchHeartbeat(*app, [&]() {
            timer.restart();
            std::thread worker([&]() {
                std::vector<ClipboardBenchEntry> localPrepared =
                    prepareClipboardBenchVector(baseEntries, insertRows);
                {
                    const std::lock_guard lock(mutex);
                    prepared = std::move(localPrepared);
                    ready = true;
                }
                cv.notify_one();
            });
            while (true) {
                {
                    std::unique_lock lock(mutex);
                    if (ready) {
                        break;
                    }
                }
                app->processEvents();
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(1ms);
            }
            worker.join();
            sample.clipboardInsertPrepareNs = timer.nsecsElapsed();
            timer.restart();
            {
                const std::lock_guard lock(mutex);
                published = std::move(prepared);
            }
            sample.clipboardInsertPublishNs = timer.nsecsElapsed();
            sample.clipboardInsertRefreshNs = refreshClipboardBenchModel(*app, published.size());
        });
        return true;
    }

    err << "Unknown clipboard store algorithm: " << algorithm << "\n";
    return false;
}

QStringList argumentsFromRaw(int argc, char* argv[])
{
    QStringList args;
    args.reserve(argc);
    for (int index = 0; index < argc; ++index) {
        args.append(QString::fromLocal8Bit(argv[index]));
    }
    return args;
}

QString formatMilliseconds(const qint64 nanoseconds)
{
    return QString::number(static_cast<double>(nanoseconds) / 1000000.0, 'f', 2);
}

QString formatRecordsPerSecond(const qsizetype rowCount, const qint64 nanoseconds)
{
    if (nanoseconds <= 0) {
        return QStringLiteral("0");
    }

    const double seconds = static_cast<double>(nanoseconds) / 1000000000.0;
    return QString::number(static_cast<double>(rowCount) / seconds, 'f', rowCount >= 100000 ? 0 : 1);
}

QString xactionStageKey(const CHIron::Gui::CLogBTraceLoadStage stage,
                        const std::size_t workerCount)
{
    using CHIron::Gui::CLogBTraceLoadStage;
    switch (stage) {
    case CLogBTraceLoadStage::Decoding:
        return QStringLiteral("indexing");
    case CLogBTraceLoadStage::Formatting:
        return workerCount == 0 ? QStringLiteral("merging")
                                : QStringLiteral("collecting");
    case CLogBTraceLoadStage::Finalizing:
        return QStringLiteral("finalizing");
    case CLogBTraceLoadStage::FinalizingIndexRows:
        return QStringLiteral("flushing_rows");
    case CLogBTraceLoadStage::FinalizingIndexRowDirectory:
        return QStringLiteral("writing_directories");
    default:
        return {};
    }
}

std::size_t transactionSpanBenchWorkerCount(const std::uint64_t rowCount)
{
    constexpr std::size_t kMaxWorkers = 8;
    constexpr std::uint64_t kMinRowsPerWorker = 262144;
    const unsigned int hardware = std::thread::hardware_concurrency();
    if (hardware == 0 || rowCount < kMinRowsPerWorker) {
        return 1;
    }
    const std::uint64_t workLimited = std::max<std::uint64_t>(1, rowCount / kMinRowsPerWorker);
    return static_cast<std::size_t>(std::min<std::uint64_t>(
        std::min<std::uint64_t>(kMaxWorkers, static_cast<std::uint64_t>(hardware)),
        workLimited));
}

bool fastRecordHasClipboardAddress(const CHIron::Gui::CLogBTraceFastRecordInfo& record) noexcept
{
    return record.address != (std::numeric_limits<std::uint64_t>::max)();
}

bool fastRecordIsClipboardRequestOrigin(const CHIron::Gui::CLogBTraceFastRecordInfo& record) noexcept
{
    switch (static_cast<CLog::Channel>(record.channel)) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
    case CLog::Channel::RXREQ:
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP:
        return true;
    case CLog::Channel::TXRSP:
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXRSP:
    case CLog::Channel::RXDAT:
        return false;
    }
    return false;
}

bool runClipboardBatchScanMicrobench(const std::shared_ptr<CHIron::Gui::TraceSession>& session,
                                     BenchmarkSample& sample,
                                     QTextStream& err)
{
    if (!session) {
        err << "No session for Clipboard batch scan benchmark.\n";
        err.flush();
        return false;
    }
    if (!session->isXactionIndexComplete()) {
        err << "Xaction index is not complete for Clipboard batch scan benchmark.\n";
        err.flush();
        return false;
    }

    struct Slice {
        std::size_t blockIndex = 0;
        std::uint64_t beginRow = 0;
        std::uint64_t rowCount = 0;
        std::size_t localBegin = 0;
    };

    constexpr std::uint64_t kSliceRows = 262144;
    std::vector<Slice> slices;
    for (std::size_t blockIndex = 0; blockIndex < session->metadata().blocks.size(); ++blockIndex) {
        const CHIron::Gui::CLogBTraceBlockSummary& block = session->metadata().blocks[blockIndex];
        for (std::uint64_t localBegin = 0; localBegin < block.recordCount;) {
            const std::uint64_t rowCount = std::min<std::uint64_t>(kSliceRows, block.recordCount - localBegin);
            slices.push_back(Slice{blockIndex,
                                   block.rowStart + localBegin,
                                   rowCount,
                                   static_cast<std::size_t>(localBegin)});
            localBegin += rowCount;
        }
    }

    std::unordered_map<std::uint64_t, std::uint64_t> anchorRowByOrdinal;
    std::unordered_map<std::uint64_t, std::uint64_t> lineByOrdinal;
    CHIron::Gui::CLogBTraceLoadError readError;
    QElapsedTimer timer;
    timer.start();
    for (const Slice& slice : slices) {
        std::vector<CHIron::Gui::CLogBTraceFastRecordInfo> fastRecords;
        std::vector<CHIron::Gui::TraceSession::XactionRowCompactInfo> compactInfos;
        if (!session->readFastRecords(slice.blockIndex,
                                      slice.localBegin,
                                      static_cast<std::size_t>(slice.rowCount),
                                      fastRecords,
                                      readError)
            || fastRecords.size() < static_cast<std::size_t>(slice.rowCount)) {
            err << "Clipboard batch scan fast-record read failed: " << readError.summary << "\n";
            err.flush();
            return false;
        }
        if (!session->xactionRowCompactInfoRange(slice.beginRow, slice.rowCount, compactInfos)
            || compactInfos.size() < static_cast<std::size_t>(slice.rowCount)) {
            err << "Clipboard batch scan compact xaction read failed.\n";
            err.flush();
            return false;
        }
        for (std::size_t rowIndex = 0; rowIndex < static_cast<std::size_t>(slice.rowCount); ++rowIndex) {
            const std::uint64_t ordinal = compactInfos[rowIndex].xactionOrdinal;
            if (ordinal == 0) {
                continue;
            }
            const CHIron::Gui::CLogBTraceFastRecordInfo& fast = fastRecords[rowIndex];
            if (!fastRecordIsClipboardRequestOrigin(fast) || !fastRecordHasClipboardAddress(fast)) {
                continue;
            }
            const std::uint64_t logicalRow = slice.beginRow + static_cast<std::uint64_t>(rowIndex);
            const auto found = anchorRowByOrdinal.find(ordinal);
            if (found == anchorRowByOrdinal.end() || logicalRow < found->second) {
                anchorRowByOrdinal[ordinal] = logicalRow;
                lineByOrdinal[ordinal] =
                    CHIron::Gui::CLogBTraceLoader::normalizeCacheLineAddress(fast.address);
            }
        }
    }
    sample.clipboardBatchScanNs = timer.nsecsElapsed();
    sample.clipboardBatchAnchorCount = static_cast<std::uint64_t>(anchorRowByOrdinal.size());

    if (lineByOrdinal.empty()) {
        sample.rowCount = static_cast<qsizetype>(session->totalRows());
        return true;
    }

    const std::uint64_t selectedLine = lineByOrdinal.begin()->second;
    std::unordered_set<std::uint64_t> matchingOrdinals;
    for (const auto& [ordinal, line] : lineByOrdinal) {
        if (line == selectedLine) {
            matchingOrdinals.insert(ordinal);
        }
    }
    sample.clipboardBatchMatchingOrdinalCount = static_cast<std::uint64_t>(matchingOrdinals.size());

    std::vector<std::uint64_t> logicalRows;
    timer.restart();
    for (const Slice& slice : slices) {
        std::vector<CHIron::Gui::TraceSession::XactionRowCompactInfo> compactInfos;
        if (!session->xactionRowCompactInfoRange(slice.beginRow, slice.rowCount, compactInfos)
            || compactInfos.size() < static_cast<std::size_t>(slice.rowCount)) {
            err << "Clipboard batch collect compact xaction read failed.\n";
            err.flush();
            return false;
        }
        for (std::size_t rowIndex = 0; rowIndex < static_cast<std::size_t>(slice.rowCount); ++rowIndex) {
            if (matchingOrdinals.find(compactInfos[rowIndex].xactionOrdinal) != matchingOrdinals.end()) {
                logicalRows.push_back(slice.beginRow + static_cast<std::uint64_t>(rowIndex));
            }
        }
    }
    sample.clipboardBatchCollectNs = timer.nsecsElapsed();
    std::sort(logicalRows.begin(), logicalRows.end());
    logicalRows.erase(std::unique(logicalRows.begin(), logicalRows.end()), logicalRows.end());
    sample.clipboardBatchCollectedRows = static_cast<std::uint64_t>(logicalRows.size());

    timer.restart();
    std::uint64_t decodedRows = 0;
    for (std::size_t index = 0; index < logicalRows.size();) {
        const std::uint64_t beginRow = logicalRows[index];
        std::uint64_t count = 1;
        while (index + static_cast<std::size_t>(count) < logicalRows.size()
               && logicalRows[index + static_cast<std::size_t>(count)] == beginRow + count) {
            ++count;
        }
        std::vector<CHIron::Gui::FlitRecord> rows;
        if (!session->readRows(beginRow, count, rows, readError)) {
            err << "Clipboard batch decode failed: " << readError.summary << "\n";
            err.flush();
            return false;
        }
        decodedRows += static_cast<std::uint64_t>(rows.size());
        index += static_cast<std::size_t>(count);
    }
    sample.clipboardBatchDecodeNs = timer.nsecsElapsed();
    sample.clipboardBatchDecodedRows = decodedRows;
    sample.rowCount = static_cast<qsizetype>(session->totalRows());
    return true;
}

bool runTransactionSpanScanMicrobench(const std::shared_ptr<CHIron::Gui::TraceSession>& session,
                                      BenchmarkSample& sample,
                                      QTextStream& err)
{
    if (!session) {
        err << "No session for transaction span scan benchmark.\n";
        err.flush();
        return false;
    }
    if (!session->isXactionIndexComplete()) {
        err << "Xaction index is not complete for transaction span scan benchmark.\n";
        err.flush();
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    const std::vector<std::uint64_t> ordinals = session->xactionOrdinals();
    std::unordered_set<std::uint64_t> completedOrdinals;
    std::unordered_map<std::uint64_t, QString> typeByOrdinal;
    std::unordered_map<std::uint64_t, std::uint64_t> rowCountsByOrdinal;
    if (!session->xactionCompletedOrdinals(completedOrdinals)) {
        err << "Failed to read completed xaction ordinals.\n";
        err.flush();
        return false;
    }
    if (!session->xactionTypesByOrdinal(typeByOrdinal)) {
        err << "Failed to read xaction types by ordinal.\n";
        err.flush();
        return false;
    }
    if (!session->xactionRowCountsByOrdinal(rowCountsByOrdinal)) {
        err << "Failed to read xaction row counts by ordinal.\n";
        err.flush();
        return false;
    }
    sample.transactionSpanMetadataMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;

    constexpr std::uint64_t kSliceRows = 262144;
    struct Slice {
        std::size_t blockIndex = 0;
        std::uint64_t beginRow = 0;
        std::uint64_t rowCount = 0;
        std::size_t localBegin = 0;
    };
    std::vector<Slice> slices;
    for (std::size_t blockIndex = 0; blockIndex < session->metadata().blocks.size(); ++blockIndex) {
        const CHIron::Gui::CLogBTraceBlockSummary& block = session->metadata().blocks[blockIndex];
        for (std::uint64_t localBegin = 0; localBegin < block.recordCount; ) {
            const std::uint64_t rowCount = std::min<std::uint64_t>(kSliceRows, block.recordCount - localBegin);
            slices.push_back(Slice{blockIndex,
                                   block.rowStart + localBegin,
                                   rowCount,
                                   static_cast<std::size_t>(localBegin)});
            localBegin += rowCount;
        }
    }

    timer.restart();
    std::atomic<std::size_t> nextSlice = 0;
    std::atomic<std::uint64_t> indexedRows = 0;
    std::atomic_bool failed = false;
    std::atomic<std::uint64_t> failedSliceBegin = 0;
    std::atomic<std::size_t> failedBlockIndex = 0;
    std::atomic<int> failedStep = 0;
    const std::size_t workerCount =
        std::min<std::size_t>(transactionSpanBenchWorkerCount(session->totalRows()),
                              std::max<std::size_t>(1, slices.size()));
    sample.transactionSpanWorkerCount = workerCount;

    const auto worker = [&]() {
        std::vector<CHIron::Gui::CLogBTraceFastRecordInfo> fastRecords;
        std::vector<CHIron::Gui::TraceSession::XactionRowCompactInfo> compactInfos;
        CHIron::Gui::CLogBTraceLoadError readError;
        while (!failed.load(std::memory_order_relaxed)) {
            const std::size_t sliceIndex = nextSlice.fetch_add(1, std::memory_order_relaxed);
            if (sliceIndex >= slices.size()) {
                break;
            }
            const Slice& slice = slices[sliceIndex];
            fastRecords.clear();
            compactInfos.clear();
            readError = {};
            if (!session->readFastRecords(slice.blockIndex,
                                          slice.localBegin,
                                          static_cast<std::size_t>(slice.rowCount),
                                          fastRecords,
                                          readError)
                || fastRecords.size() < static_cast<std::size_t>(slice.rowCount)) {
                failedBlockIndex.store(slice.blockIndex, std::memory_order_relaxed);
                failedSliceBegin.store(slice.beginRow, std::memory_order_relaxed);
                failedStep.store(1, std::memory_order_relaxed);
                failed.store(true, std::memory_order_relaxed);
                break;
            }
            if (!session->xactionRowCompactInfoRange(slice.beginRow, slice.rowCount, compactInfos)
                || compactInfos.size() < static_cast<std::size_t>(slice.rowCount)) {
                failedBlockIndex.store(slice.blockIndex, std::memory_order_relaxed);
                failedSliceBegin.store(slice.beginRow, std::memory_order_relaxed);
                failedStep.store(2, std::memory_order_relaxed);
                failed.store(true, std::memory_order_relaxed);
                break;
            }
            std::uint64_t localIndexedRows = 0;
            for (std::size_t rowIndex = 0; rowIndex < static_cast<std::size_t>(slice.rowCount); ++rowIndex) {
                if (compactInfos[rowIndex].xactionOrdinal != 0) {
                    ++localIndexedRows;
                }
            }
            indexedRows.fetch_add(localIndexedRows, std::memory_order_relaxed);
        }
    };

    if (workerCount <= 1) {
        worker();
    } else {
        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
            workers.emplace_back(worker);
        }
    }

    if (failed.load(std::memory_order_relaxed)) {
        const int step = failedStep.load(std::memory_order_relaxed);
        err << "Transaction span scan microbenchmark failed in "
            << (step == 1 ? QStringLiteral("fast-record range read")
                          : (step == 2 ? QStringLiteral("compact xaction row read")
                                       : QStringLiteral("unknown step")))
            << " for block " << failedBlockIndex.load(std::memory_order_relaxed)
            << ", begin row " << failedSliceBegin.load(std::memory_order_relaxed)
            << ".\n";
        err.flush();
        return false;
    }
    sample.transactionSpanScanMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
    sample.transactionSpanIndexedRowCount = indexedRows.load(std::memory_order_relaxed);
    sample.transactionSpanCount = static_cast<int>(std::min<std::uint64_t>(
        ordinals.size(), static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
    sample.rowCount = static_cast<qsizetype>(session->totalRows());
    return true;
}

bool runTransactionRenderMicrobench(const std::shared_ptr<CHIron::Gui::TraceSession>& session,
                                    int renderRepeats,
                                    int verticalSteps,
                                    BenchmarkSample& sample,
                                    QTextStream& err)
{
#ifdef CHIRON_GUI_ENABLE_TRANSACTION_TEST_API
    if (!session) {
        err << "No session for transaction render benchmark.\n";
        err.flush();
        return false;
    }
    if (!session->isXactionIndexComplete()) {
        err << "Xaction index is not complete for transaction render benchmark.\n";
        err.flush();
        return false;
    }

    CHIron::Gui::TransactionWidget widget;
    widget.resize(1600, 900);
    const QVariantMap spanSummary = widget.testBuildSpansFromTraceSession(session);
    if (spanSummary.value(QStringLiteral("failed")).toBool()
        || spanSummary.value(QStringLiteral("cancelled")).toBool()
        || spanSummary.value(QStringLiteral("spanCount")).toInt() <= 0) {
        err << "Transaction span build failed before render benchmark.\n";
        err << "Status: " << spanSummary.value(QStringLiteral("status")).toString() << "\n";
        err.flush();
        return false;
    }

    widget.testSetHorizontalZoom(1.0);
    widget.testSetHorizontalScroll(0);
    widget.testSetVerticalScroll(0);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);

    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    QElapsedTimer totalTimer;
    QElapsedTimer renderTimer;
    verticalSteps = std::max(0, verticalSteps);
    renderRepeats = verticalSteps > 0 ? verticalSteps : std::max(1, renderRepeats);
    qint64 repeatNs = 0;
    totalTimer.start();
    for (int renderIndex = 0; renderIndex < renderRepeats; ++renderIndex) {
        if (verticalSteps > 0) {
            widget.testSetVerticalScroll(renderIndex);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderTimer.restart();
        widget.render(&painter);
        painter.end();
        const qint64 elapsedNs = renderTimer.nsecsElapsed();
        if (renderIndex == 0) {
            sample.transactionRenderFirstNs = elapsedNs;
        } else {
            repeatNs += elapsedNs;
        }
    }

    sample.transactionRenderNs = totalTimer.nsecsElapsed();
    sample.transactionRenderRepeatNs =
        renderRepeats > 1 ? repeatNs / static_cast<qint64>(renderRepeats - 1) : 0;
    sample.transactionRenderRepeats = renderRepeats;
    sample.transactionRenderVerticalSteps = verticalSteps;
    sample.transactionRenderCandidateCount = widget.testVisiblePaintCandidateCount();
    sample.transactionRenderDense = widget.testUsesDensePaint();
    sample.transactionSpanCount = spanSummary.value(QStringLiteral("spanCount")).toInt();
    sample.transactionLaneCount = spanSummary.value(QStringLiteral("laneCount")).toInt();
    sample.transactionSpanWorkerCount =
        static_cast<std::size_t>(spanSummary.value(QStringLiteral("workerCount")).toULongLong());
    sample.rowCount = static_cast<qsizetype>(session->totalRows());
    return true;
#else
    Q_UNUSED(session);
    Q_UNUSED(renderRepeats);
    Q_UNUSED(verticalSteps);
    Q_UNUSED(sample);
    err << "This benchmark target was built without Transaction test API support.\n";
    err.flush();
    return false;
#endif
}

void printUsage(QTextStream& stream, const QString& executableName)
{
    stream << "Usage: " << executableName
           << " <trace-file> [--iterations N] [--warmup N] [--repeat-filters N] [--rows N] [--req-only] [--tx-only]"
              " [--opcode-filter TEXT] [--src-filter TEXT] [--tgt-filter TEXT]"
              " [--txn-filter TEXT] [--dbid-filter TEXT] [--addr-filter TEXT]"
              " [--no-view] [--skip-model] [--scan-only] [--xaction-index-only]"
              " [--editable-mode-only] [--transaction-spans-only] [--transaction-span-scan-only] [--clipboard-batch-scan-only]"
              " [--clipboard-insert-bench] [--clipboard-store-algorithm NAME] [--transaction-render-only]"
              " [--transaction-render-repeats N] [--transaction-render-vertical-steps N]\n";
    stream << "Measures metadata scan time, row decode time, model/filter population time, trace-table sizing time, optional Xaction indexing time, and Transaction span build time.\n";
}

std::optional<BenchmarkOptions> parseArguments(const QStringList& args, QTextStream& err)
{
    if (args.size() <= 1) {
        printUsage(err, QFileInfo(args.value(0)).fileName());
        return std::nullopt;
    }

    BenchmarkOptions options;
    for (int index = 1; index < args.size(); ++index) {
        const QString& argument = args[index];
        if (argument == QLatin1String("--help") || argument == QLatin1String("-h")) {
            printUsage(err, QFileInfo(args.value(0)).fileName());
            return std::nullopt;
        }

        if (argument == QLatin1String("--no-view")) {
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--skip-model")) {
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--scan-only")) {
            options.scanOnly = true;
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--xaction-index-only")) {
            options.xactionIndexOnly = true;
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--editable-mode-only")) {
            options.editableModeOnly = true;
            options.includeModel = true;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--transaction-spans-only")) {
            options.transactionSpansOnly = true;
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--transaction-span-scan-only")) {
            options.transactionSpanScanOnly = true;
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--clipboard-batch-scan-only")) {
            options.clipboardBatchScanOnly = true;
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--clipboard-insert-bench")) {
            options.clipboardInsertBench = true;
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--clipboard-store-algorithm")) {
            if (index + 1 >= args.size()) {
                err << "Missing value for " << argument << ".\n";
                return std::nullopt;
            }
            options.clipboardStoreAlgorithm = args[index + 1];
            ++index;
            continue;
        }

        if (argument == QLatin1String("--transaction-render-only")) {
            options.transactionRenderOnly = true;
            options.includeModel = false;
            options.includeView = false;
            continue;
        }

        if (argument == QLatin1String("--transaction-render-repeats")) {
            if (index + 1 >= args.size()) {
                err << "Missing value for " << argument << ".\n";
                return std::nullopt;
            }

            bool ok = false;
            const int value = args[index + 1].toInt(&ok);
            if (!ok || value <= 0) {
                err << "Invalid numeric value for " << argument << ": " << args[index + 1] << "\n";
                return std::nullopt;
            }
            options.transactionRenderRepeats = value;
            ++index;
            continue;
        }

        if (argument == QLatin1String("--transaction-render-vertical-steps")) {
            if (index + 1 >= args.size()) {
                err << "Missing value for " << argument << ".\n";
                return std::nullopt;
            }

            bool ok = false;
            const int value = args[index + 1].toInt(&ok);
            if (!ok || value <= 0) {
                err << "Invalid numeric value for " << argument << ": " << args[index + 1] << "\n";
                return std::nullopt;
            }
            options.transactionRenderVerticalSteps = value;
            options.transactionRenderRepeats = value;
            ++index;
            continue;
        }

        if (argument == QLatin1String("--req-only")) {
            options.reqOnly = true;
            continue;
        }

        if (argument == QLatin1String("--tx-only")) {
            options.txOnly = true;
            continue;
        }

        if (argument == QLatin1String("--opcode-filter")
            || argument == QLatin1String("--src-filter")
            || argument == QLatin1String("--tgt-filter")
            || argument == QLatin1String("--txn-filter")
            || argument == QLatin1String("--dbid-filter")
            || argument == QLatin1String("--addr-filter")) {
            if (index + 1 >= args.size()) {
                err << "Missing value for " << argument << ".\n";
                return std::nullopt;
            }

            const QString value = args[index + 1];
            if (argument == QLatin1String("--opcode-filter")) {
                options.opcodeFilter = value;
            } else if (argument == QLatin1String("--src-filter")) {
                options.sourceIdFilter = value;
            } else if (argument == QLatin1String("--tgt-filter")) {
                options.targetIdFilter = value;
            } else if (argument == QLatin1String("--txn-filter")) {
                options.txnIdFilter = value;
            } else if (argument == QLatin1String("--dbid-filter")) {
                options.dbidFilter = value;
            } else {
                options.addressFilter = value;
            }
            ++index;
            continue;
        }

        if (argument == QLatin1String("--iterations")
            || argument == QLatin1String("--warmup")
            || argument == QLatin1String("--repeat-filters")
            || argument == QLatin1String("--rows")) {
            if (index + 1 >= args.size()) {
                err << "Missing value for " << argument << ".\n";
                return std::nullopt;
            }

            bool ok = false;
            const quint64 value = args[index + 1].toULongLong(&ok);
            if (!ok) {
                err << "Invalid numeric value for " << argument << ": " << args[index + 1] << "\n";
                return std::nullopt;
            }

            if (argument == QLatin1String("--iterations")) {
                options.iterations = qMax(1, static_cast<int>(value));
            } else if (argument == QLatin1String("--warmup")) {
                options.warmupIterations = static_cast<int>(value);
            } else if (argument == QLatin1String("--repeat-filters")) {
                options.repeatFilters = qMax(1, static_cast<int>(value));
            } else {
                options.rowLimit = value;
            }
            ++index;
            continue;
        }

        if (options.filePath.isEmpty()) {
            options.filePath = argument;
            continue;
        }

        err << "Unexpected argument: " << argument << "\n";
        return std::nullopt;
    }

    if (options.filePath.isEmpty()) {
        err << "Trace file path is required.\n";
        return std::nullopt;
    }

    return options;
}

BenchmarkSample runSample(const BenchmarkOptions& options)
{
    BenchmarkSample sample;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    std::vector<CHIron::Gui::FlitRecord> rows;
    CHIron::Gui::CLogBTraceLoadError error;

    QElapsedTimer timer;
    timer.start();
    if (!CHIron::Gui::TraceSession::open(options.filePath, session, error)) {
        QTextStream err(stderr);
        err << "Open failed: " << error.summary << "\n";
        if (!error.informativeText.isEmpty()) {
            err << error.informativeText << "\n";
        }
        std::exit(2);
    }
    sample.scanNs = timer.nsecsElapsed();

    if (options.scanOnly) {
        sample.rowCount = static_cast<qsizetype>(session->totalRows());
        return sample;
    }

    if (options.xactionIndexOnly) {
        std::map<QString, qint64> stageNsByKey;
        QString activeStageKey;
        QElapsedTimer stageTimer;
        CHIron::Gui::CLogBTraceLoadCallbacks callbacks;
        callbacks.stage =
            [&](const CHIron::Gui::CLogBTraceLoadStage stage,
                const std::size_t workerCount,
                const std::size_t) {
                if (!activeStageKey.isEmpty() && stageTimer.isValid()) {
                    stageNsByKey[activeStageKey] += stageTimer.nsecsElapsed();
                }
                activeStageKey = xactionStageKey(stage, workerCount);
                if (!activeStageKey.isEmpty()) {
                    stageTimer.restart();
                } else {
                    stageTimer.invalidate();
                }
            };

        timer.restart();
        if (!session->buildXactionIndex(error, callbacks)) {
            QTextStream err(stderr);
            err << "Xaction index build failed: " << error.summary << "\n";
            if (!error.informativeText.isEmpty()) {
                err << error.informativeText << "\n";
            }
            if (!error.detailedText.isEmpty()) {
                err << error.detailedText << "\n";
            }
            std::exit(2);
        }
        if (!activeStageKey.isEmpty() && stageTimer.isValid()) {
            stageNsByKey[activeStageKey] += stageTimer.nsecsElapsed();
        }
        sample.xactionIndexNs = timer.nsecsElapsed();
        sample.xactionIndexingNs = stageNsByKey[QStringLiteral("indexing")];
        sample.xactionMergingNs = stageNsByKey[QStringLiteral("merging")];
        sample.xactionCollectingNs = stageNsByKey[QStringLiteral("collecting")];
        sample.xactionFinalizingNs = stageNsByKey[QStringLiteral("finalizing")];
        sample.xactionFlushingRowsNs = stageNsByKey[QStringLiteral("flushing_rows")];
        sample.xactionWritingDirectoriesNs = stageNsByKey[QStringLiteral("writing_directories")];
        sample.rowCount = static_cast<qsizetype>(session->totalRows());
        return sample;
    }

    if (options.editableModeOnly) {
        sample.rowCount = static_cast<qsizetype>(session->totalRows());
        CHIron::Gui::FlitTableModel model;
        timer.restart();
        model.setTraceSession(session);
        model.setEditable(true);
        sample.editablePrepareNs = timer.nsecsElapsed();
        sample.setRowsNs = sample.editablePrepareNs;

        timer.restart();
        if (model.rowCount() > 0 && model.columnCount() > 0) {
            (void)model.data(model.index(0, CHIron::Gui::FlitTableModel::OpcodeColumn), Qt::DisplayRole);
        }
        sample.editableFirstCellNs = timer.nsecsElapsed();
        return sample;
    }

    if (options.transactionSpansOnly) {
#ifdef CHIRON_GUI_ENABLE_TRANSACTION_TEST_API
        if (!session->isXactionIndexComplete()) {
            if (!session->buildXactionIndex(error)) {
                QTextStream err(stderr);
                err << "Xaction index build failed: " << error.summary << "\n";
                if (!error.informativeText.isEmpty()) {
                    err << error.informativeText << "\n";
                }
                if (!error.detailedText.isEmpty()) {
                    err << error.detailedText << "\n";
                }
                std::exit(2);
            }
        }

        CHIron::Gui::TransactionWidget widget;
        widget.resize(1600, 900);
        timer.restart();
        const QVariantMap spanSummary = widget.testBuildSpansFromTraceSession(session);
        if (spanSummary.value(QStringLiteral("failed")).toBool()
            || spanSummary.value(QStringLiteral("cancelled")).toBool()
            || spanSummary.value(QStringLiteral("spanCount")).toInt() <= 0) {
            QTextStream err(stderr);
            err << "Transaction span build failed.\n";
            err << "Status: " << spanSummary.value(QStringLiteral("status")).toString() << "\n";
            std::exit(2);
        }
        sample.transactionSpanBuildNs = timer.nsecsElapsed();
        sample.transactionSpanCount = spanSummary.value(QStringLiteral("spanCount")).toInt();
        sample.transactionLaneCount = spanSummary.value(QStringLiteral("laneCount")).toInt();
        sample.transactionSpanWorkerCount =
            static_cast<std::size_t>(spanSummary.value(QStringLiteral("workerCount")).toULongLong());
        sample.transactionSpanMetadataMs = spanSummary.value(QStringLiteral("metadataMs")).toDouble();
        sample.transactionSpanScanMs = spanSummary.value(QStringLiteral("scanMs")).toDouble();
        sample.transactionSpanMergeMs = spanSummary.value(QStringLiteral("mergeMs")).toDouble();
        sample.transactionSpanFirstInfoMs = spanSummary.value(QStringLiteral("firstInfoMs")).toDouble();
        sample.transactionSpanBuildOnlyMs = spanSummary.value(QStringLiteral("spanMs")).toDouble();
        sample.transactionSpanSortMs = spanSummary.value(QStringLiteral("sortMs")).toDouble();
        sample.transactionSpanLayoutMs = spanSummary.value(QStringLiteral("layoutMs")).toDouble();
        sample.rowCount = static_cast<qsizetype>(session->totalRows());
        return sample;
#else
        QTextStream err(stderr);
        err << "This benchmark target was built without Transaction test API support.\n";
        std::exit(2);
#endif
    }

    if (options.transactionSpanScanOnly) {
        if (!session->isXactionIndexComplete()) {
            if (!session->buildXactionIndex(error)) {
                QTextStream err(stderr);
                err << "Xaction index build failed: " << error.summary << "\n";
                if (!error.informativeText.isEmpty()) {
                    err << error.informativeText << "\n";
                }
                if (!error.detailedText.isEmpty()) {
                    err << error.detailedText << "\n";
                }
                std::exit(2);
            }
        }
        QTextStream err(stderr);
        if (!runTransactionSpanScanMicrobench(session, sample, err)) {
            std::exit(2);
        }
        return sample;
    }

    if (options.clipboardBatchScanOnly) {
        if (!session->isXactionIndexComplete()) {
            if (!session->buildXactionIndex(error)) {
                QTextStream err(stderr);
                err << "Xaction index build failed: " << error.summary << "\n";
                if (!error.informativeText.isEmpty()) {
                    err << error.informativeText << "\n";
                }
                if (!error.detailedText.isEmpty()) {
                    err << error.detailedText << "\n";
                }
                std::exit(2);
            }
        }
        QTextStream err(stderr);
        if (!runClipboardBatchScanMicrobench(session, sample, err)) {
            std::exit(2);
        }
        return sample;
    }

    if (options.clipboardInsertBench) {
        QTextStream err(stderr);
        if (!runClipboardInsertAlgorithmBench(options.clipboardStoreAlgorithm, sample, err)) {
            std::exit(2);
        }
        sample.rowCount = static_cast<qsizetype>(session->totalRows());
        return sample;
    }

    if (options.transactionRenderOnly) {
        if (!session->isXactionIndexComplete()) {
            if (!session->buildXactionIndex(error)) {
                QTextStream err(stderr);
                err << "Xaction index build failed: " << error.summary << "\n";
                if (!error.informativeText.isEmpty()) {
                    err << error.informativeText << "\n";
                }
                if (!error.detailedText.isEmpty()) {
                    err << error.detailedText << "\n";
                }
                std::exit(2);
            }
        }
        QTextStream err(stderr);
        if (!runTransactionRenderMicrobench(session,
                                            options.transactionRenderRepeats,
                                            options.transactionRenderVerticalSteps,
                                            sample,
                                            err)) {
            std::exit(2);
        }
        return sample;
    }

    if (!options.includeModel) {
        timer.restart();
        const quint64 requestedRows = options.rowLimit == 0
            ? session->totalRows()
            : std::min<quint64>(session->totalRows(), options.rowLimit);
        if (!session->readRows(0, requestedRows, rows, error)) {
            QTextStream err(stderr);
            err << "Row load failed: " << error.summary << "\n";
            if (!error.informativeText.isEmpty()) {
                err << error.informativeText << "\n";
            }
            std::exit(2);
        }
        sample.rowLoadNs = timer.nsecsElapsed();
        sample.rowCount = static_cast<qsizetype>(rows.size());
        return sample;
    }

    sample.rowCount = static_cast<qsizetype>(session->totalRows());
    CHIron::Gui::FlitTableModel model;
    std::unique_ptr<QTableView> tableView;
    if (options.includeView) {
        tableView = std::make_unique<QTableView>();
        tableView->setModel(&model);
        tableView->setAlternatingRowColors(true);
        tableView->verticalHeader()->hide();
        tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        tableView->verticalHeader()->setMinimumSectionSize(18);
        tableView->verticalHeader()->setDefaultSectionSize(22);
        tableView->horizontalHeader()->setStretchLastSection(false);
        tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        tableView->horizontalHeader()->setResizeContentsPrecision(256);
        tableView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        tableView->resize(1600, 900);
    }

    timer.restart();
    model.setTraceSession(session);
    sample.setRowsNs = timer.nsecsElapsed();

    if (options.reqOnly || options.txOnly) {
        timer.restart();
        if (options.reqOnly) {
            model.setChannelVisible(CHIron::Gui::FlitChannel::Rsp, false);
            model.setChannelVisible(CHIron::Gui::FlitChannel::Dat, false);
            model.setChannelVisible(CHIron::Gui::FlitChannel::Snp, false);
        }
        if (options.txOnly) {
            model.setDirectionVisible(CHIron::Gui::FlitDirection::Rx, false);
        }
        sample.filterNs = timer.nsecsElapsed();
        sample.rowCount = model.visibleCount();
    }

    if (!options.opcodeFilter.isEmpty()
        || !options.sourceIdFilter.isEmpty()
        || !options.targetIdFilter.isEmpty()
        || !options.txnIdFilter.isEmpty()
        || !options.dbidFilter.isEmpty()
        || !options.addressFilter.isEmpty()) {
        timer.restart();
        for (int iteration = 0; iteration < options.repeatFilters; ++iteration) {
            if (!options.opcodeFilter.isEmpty()) {
                model.setOpcodeFilter(options.opcodeFilter);
            }
            if (!options.sourceIdFilter.isEmpty()) {
                model.setSourceIdFilter(options.sourceIdFilter);
            }
            if (!options.targetIdFilter.isEmpty()) {
                model.setTargetIdFilter(options.targetIdFilter);
            }
            if (!options.txnIdFilter.isEmpty()) {
                model.setTxnIdFilter(options.txnIdFilter);
            }
            if (!options.dbidFilter.isEmpty()) {
                model.setDbidFilter(options.dbidFilter);
            }
            if (!options.addressFilter.isEmpty()) {
                model.setAddressFilter(options.addressFilter);
            }
        }
        sample.filterNs += timer.nsecsElapsed();
        sample.rowCount = model.visibleCount();
    }

    if (tableView) {
        tableView->show();
        QCoreApplication::processEvents();

        timer.restart();
        tableView->resizeColumnsToContents();
        QCoreApplication::processEvents();
        sample.resizeColumnsNs = timer.nsecsElapsed();
        tableView->hide();
        QCoreApplication::processEvents();
    }

    return sample;
}

qint64 averageNanoseconds(const BenchmarkSeries& series, qint64 BenchmarkSample::*member)
{
    if (series.samples.empty()) {
        return 0;
    }

    qint64 total = 0;
    for (const BenchmarkSample& sample : series.samples) {
        total += sample.*member;
    }
    return total / static_cast<qint64>(series.samples.size());
}

void printXactionStageSummary(QTextStream& out, const BenchmarkSample& sample)
{
    if (sample.xactionIndexNs == 0) {
        return;
    }

    out << "    Xaction stages: indexing "
        << formatMilliseconds(sample.xactionIndexingNs)
        << " ms, merging "
        << formatMilliseconds(sample.xactionMergingNs)
        << " ms, collecting "
        << formatMilliseconds(sample.xactionCollectingNs)
        << " ms, finalizing "
        << formatMilliseconds(sample.xactionFinalizingNs)
        << " ms, flushing rows "
        << formatMilliseconds(sample.xactionFlushingRowsNs)
        << " ms, writing directories "
        << formatMilliseconds(sample.xactionWritingDirectoriesNs)
        << " ms\n";
}

void printTransactionSpanSummary(QTextStream& out, const BenchmarkSample& sample)
{
    if (sample.transactionSpanBuildNs == 0
        && sample.transactionSpanMetadataMs == 0.0
        && sample.transactionSpanScanMs == 0.0
        && sample.transactionRenderNs == 0) {
        return;
    }

    const QString buildText = sample.transactionSpanBuildNs == 0
        ? QStringLiteral("0.00")
        : formatMilliseconds(sample.transactionSpanBuildNs);
    out << "    Transaction spans: build "
        << buildText
        << " ms, spans "
        << sample.transactionSpanCount
        << ", lanes "
        << sample.transactionLaneCount
        << ", workers "
        << sample.transactionSpanWorkerCount
        << "\n";
    out << "    Transaction span stages: metadata "
        << QString::number(sample.transactionSpanMetadataMs, 'f', 2)
        << " ms, scan "
        << QString::number(sample.transactionSpanScanMs, 'f', 2)
        << " ms, merge "
        << QString::number(sample.transactionSpanMergeMs, 'f', 2)
        << " ms, first-info "
        << QString::number(sample.transactionSpanFirstInfoMs, 'f', 2)
        << " ms, span-build "
        << QString::number(sample.transactionSpanBuildOnlyMs, 'f', 2)
        << " ms, sort "
        << QString::number(sample.transactionSpanSortMs, 'f', 2)
        << " ms, layout "
        << QString::number(sample.transactionSpanLayoutMs, 'f', 2)
        << " ms\n";
    if (sample.transactionSpanIndexedRowCount != 0) {
        out << "    Transaction span scan rows: indexed "
            << sample.transactionSpanIndexedRowCount
            << "\n";
    }
    if (sample.transactionRenderNs != 0) {
        out << "    Transaction render: paint "
            << formatMilliseconds(sample.transactionRenderNs)
            << " ms, candidates "
            << sample.transactionRenderCandidateCount
            << ", dense "
            << (sample.transactionRenderDense ? QStringLiteral("yes") : QStringLiteral("no"));
        if (sample.transactionRenderVerticalSteps > 0) {
            out << ", vertical steps "
                << sample.transactionRenderVerticalSteps
                << ", first "
                << formatMilliseconds(sample.transactionRenderFirstNs)
                << " ms, scroll avg "
                << formatMilliseconds(sample.transactionRenderRepeatNs)
                << " ms";
        }
        if (sample.transactionRenderVerticalSteps == 0 && sample.transactionRenderRepeats > 1) {
            out << ", repeats "
                << sample.transactionRenderRepeats
                << ", first "
                << formatMilliseconds(sample.transactionRenderFirstNs)
                << " ms, cached avg "
                << formatMilliseconds(sample.transactionRenderRepeatNs)
                << " ms";
        }
        out << "\n";
    }
}

void printClipboardBatchSummary(QTextStream& out, const BenchmarkSample& sample)
{
    if (sample.clipboardBatchScanNs == 0
        && sample.clipboardBatchCollectNs == 0
        && sample.clipboardBatchDecodeNs == 0) {
        return;
    }

    out << "    Clipboard batch scan: scan "
        << formatMilliseconds(sample.clipboardBatchScanNs)
        << " ms, collect "
        << formatMilliseconds(sample.clipboardBatchCollectNs)
        << " ms, decode "
        << formatMilliseconds(sample.clipboardBatchDecodeNs)
        << " ms, anchors "
        << sample.clipboardBatchAnchorCount
        << ", matching ordinals "
        << sample.clipboardBatchMatchingOrdinalCount
        << ", rows "
        << sample.clipboardBatchCollectedRows
        << ", decoded "
        << sample.clipboardBatchDecodedRows
        << "\n";
}

void printClipboardInsertSummary(QTextStream& out, const BenchmarkSample& sample)
{
    if (sample.clipboardInsertAlgorithm.isEmpty()) {
        return;
    }

    out << "    Clipboard insert [" << sample.clipboardInsertAlgorithm << "]: prepare "
        << formatMilliseconds(sample.clipboardInsertPrepareNs)
        << " ms, publish "
        << formatMilliseconds(sample.clipboardInsertPublishNs)
        << " ms, refresh "
        << formatMilliseconds(sample.clipboardInsertRefreshNs)
        << " ms, heartbeat max "
        << formatMilliseconds(sample.clipboardInsertHeartbeatMaxNs)
        << " ms, existing "
        << sample.clipboardInsertExistingRows
        << ", inserted "
        << sample.clipboardInsertRows
        << "\n";
}

void printEditableSummary(QTextStream& out, const BenchmarkSample& sample)
{
    if (sample.editablePrepareNs == 0 && sample.editableFirstCellNs == 0) {
        return;
    }

    out << "    Editable: prepare "
        << formatMilliseconds(sample.editablePrepareNs)
        << " ms, first cell "
        << formatMilliseconds(sample.editableFirstCellNs)
        << " ms\n";
}

}  // namespace

int main(int argc, char* argv[])
{
    const QStringList rawArguments = argumentsFromRaw(argc, argv);
    const bool transactionSpansMode = rawArguments.contains(QStringLiteral("--transaction-spans-only"));
    const bool transactionRenderMode = rawArguments.contains(QStringLiteral("--transaction-render-only"));
    const bool editableMode = rawArguments.contains(QStringLiteral("--editable-mode-only"));
    const bool needsGuiApplication =
        transactionSpansMode
        || transactionRenderMode
        || (!rawArguments.contains(QStringLiteral("--scan-only"))
        && !rawArguments.contains(QStringLiteral("--skip-model"))
        && !rawArguments.contains(QStringLiteral("--xaction-index-only"))
        && !editableMode
        && !rawArguments.contains(QStringLiteral("--transaction-span-scan-only"))
        && !rawArguments.contains(QStringLiteral("--transaction-render-only"))
        && !rawArguments.contains(QStringLiteral("--no-view")));

    std::unique_ptr<QCoreApplication> app;
    if (needsGuiApplication) {
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            if (transactionSpansMode || transactionRenderMode) {
#if defined(Q_OS_WIN)
                qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("windows"));
#else
                qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
#endif
            } else {
#if defined(Q_OS_WIN)
                qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("windows"));
#else
            qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
#endif
            }
        }
        app = std::make_unique<QApplication>(argc, argv);
    } else {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }

    QApplication* guiApp = qobject_cast<QApplication*>(app.get());
    if (guiApp) {
        guiApp->setQuitOnLastWindowClosed(false);
    }

    QTextStream out(stdout);
    QTextStream err(stderr);

    const std::optional<BenchmarkOptions> options = parseArguments(app->arguments(), err);
    if (!options.has_value()) {
        return 1;
    }

    const QFileInfo traceInfo(options->filePath);
    if (!traceInfo.exists() || !traceInfo.isFile()) {
        err << "Trace file not found: " << options->filePath << "\n";
        return 1;
    }

    out << "Trace: " << traceInfo.absoluteFilePath() << "\n";
    out << "Size:  " << traceInfo.size() << " bytes\n";
    const QString mode = options->scanOnly
               ? QStringLiteral("metadata scan only")
               : (options->xactionIndexOnly
                      ? QStringLiteral("Xaction index only")
                      : (options->editableModeOnly
                             ? QStringLiteral("editable mode only")
                             : (options->transactionSpansOnly
                                    ? QStringLiteral("Transaction spans only")
                                    : (options->transactionSpanScanOnly
                                            ? QStringLiteral("Transaction span scan only")
                                            : (options->clipboardBatchScanOnly
                                                   ? QStringLiteral("Clipboard batch scan only")
                                                   : (options->clipboardInsertBench
                                                          ? QStringLiteral("Clipboard insert benchmark")
                                                          : (options->transactionRenderOnly
                                                                 ? QStringLiteral("Transaction render only")
                                                                 : (options->includeModel
                                                                        ? (options->includeView
                                                                               ? QStringLiteral("loader + table attach")
                                                                               : QStringLiteral("loader + model"))
                                                                        : QStringLiteral("loader only")))))))));
    out << "Mode:  " << mode << "\n";
    if (options->reqOnly || options->txOnly) {
        QStringList filterModes;
        if (options->reqOnly) {
            filterModes.append(QStringLiteral("REQ only"));
        }
        if (options->txOnly) {
            filterModes.append(QStringLiteral("TX only"));
        }
        out << "Post-load filter: " << filterModes.join(QStringLiteral(", ")) << "\n";
    }
    if (!options->opcodeFilter.isEmpty()
        || !options->sourceIdFilter.isEmpty()
        || !options->targetIdFilter.isEmpty()
        || !options->txnIdFilter.isEmpty()
        || !options->dbidFilter.isEmpty()
        || !options->addressFilter.isEmpty()) {
        QStringList filterModes;
        if (!options->opcodeFilter.isEmpty()) {
            filterModes.append(QStringLiteral("Opcode=%1").arg(options->opcodeFilter));
        }
        if (!options->sourceIdFilter.isEmpty()) {
            filterModes.append(QStringLiteral("SrcID=%1").arg(options->sourceIdFilter));
        }
        if (!options->targetIdFilter.isEmpty()) {
            filterModes.append(QStringLiteral("TgtID=%1").arg(options->targetIdFilter));
        }
        if (!options->txnIdFilter.isEmpty()) {
            filterModes.append(QStringLiteral("TxnID=%1").arg(options->txnIdFilter));
        }
        if (!options->dbidFilter.isEmpty()) {
            filterModes.append(QStringLiteral("DBID=%1").arg(options->dbidFilter));
        }
        if (!options->addressFilter.isEmpty()) {
            filterModes.append(QStringLiteral("Addr=%1").arg(options->addressFilter));
        }
        out << "Value filter: " << filterModes.join(QStringLiteral(", ")) << "\n";
    }
    if (options->rowLimit != 0) {
        out << "Rows requested: " << options->rowLimit << "\n";
    }
    if (options->repeatFilters > 1) {
        out << "Repeated filter passes: " << options->repeatFilters << "\n";
    }
    if (options->transactionRenderOnly && options->transactionRenderVerticalSteps > 0) {
        out << "Vertical render steps per sample: " << options->transactionRenderVerticalSteps << "\n";
    } else if (options->transactionRenderOnly && options->transactionRenderRepeats > 1) {
        out << "Render repeats per sample: " << options->transactionRenderRepeats << "\n";
    }
    if (options->clipboardInsertBench) {
        out << "Clipboard store algorithm: " << options->clipboardStoreAlgorithm << "\n";
    }
    out << "Warmup iterations: " << options->warmupIterations << "\n";
    out << "Measured iterations: " << options->iterations << "\n\n";

    for (int index = 0; index < options->warmupIterations; ++index) {
        const BenchmarkSample warmup = runSample(*options);
        out << "Warmup " << (index + 1) << ": "
            << warmup.rowCount << " rows, scan "
            << formatMilliseconds(warmup.scanNs) << " ms, xaction "
            << formatMilliseconds(warmup.xactionIndexNs) << " ms, rows "
            << formatMilliseconds(warmup.rowLoadNs) << " ms\n";
        printXactionStageSummary(out, warmup);
        printTransactionSpanSummary(out, warmup);
        printClipboardBatchSummary(out, warmup);
    }

    BenchmarkSeries series;
    series.samples.reserve(static_cast<std::size_t>(options->iterations));

    out << "\nIteration  Rows      Scan(ms)  Rows(ms)  Xact(ms)  Spans(ms)  Render(ms)  SetRows(ms)  Filter(ms)  Resize(ms)  Total(ms)  Rows rec/s\n";
    out << "---------  --------  --------  --------  --------  ---------  ----------  -----------  ----------  ----------  ---------  ----------\n";
    for (int index = 0; index < options->iterations; ++index) {
        const BenchmarkSample sample = runSample(*options);
        series.samples.push_back(sample);

        const qint64 totalNs = sample.scanNs + sample.rowLoadNs + sample.xactionIndexNs
            + sample.transactionSpanBuildNs + sample.transactionRenderNs
            + sample.setRowsNs + sample.filterNs + sample.resizeColumnsNs
            + sample.clipboardInsertPrepareNs + sample.clipboardInsertPublishNs
            + sample.clipboardInsertRefreshNs;
        out << QString::number(index + 1).rightJustified(9, QLatin1Char(' ')) << "  "
            << QString::number(sample.rowCount).rightJustified(8, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.scanNs).rightJustified(8, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.rowLoadNs).rightJustified(8, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.xactionIndexNs).rightJustified(8, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.transactionSpanBuildNs).rightJustified(9, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.transactionRenderNs).rightJustified(10, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.setRowsNs).rightJustified(11, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.filterNs).rightJustified(10, QLatin1Char(' ')) << "  "
            << formatMilliseconds(sample.resizeColumnsNs).rightJustified(10, QLatin1Char(' ')) << "  "
            << formatMilliseconds(totalNs).rightJustified(9, QLatin1Char(' ')) << "  "
            << formatRecordsPerSecond(sample.rowCount, sample.rowLoadNs).rightJustified(10, QLatin1Char(' ')) << "\n";
        printXactionStageSummary(out, sample);
        printTransactionSpanSummary(out, sample);
        printClipboardBatchSummary(out, sample);
        printClipboardInsertSummary(out, sample);
        printEditableSummary(out, sample);
    }

    const qint64 averageScanNs = averageNanoseconds(series, &BenchmarkSample::scanNs);
    const qint64 averageRowLoadNs = averageNanoseconds(series, &BenchmarkSample::rowLoadNs);
    const qint64 averageXactionIndexNs = averageNanoseconds(series, &BenchmarkSample::xactionIndexNs);
    const qint64 averageXactionIndexingNs = averageNanoseconds(series, &BenchmarkSample::xactionIndexingNs);
    const qint64 averageXactionMergingNs = averageNanoseconds(series, &BenchmarkSample::xactionMergingNs);
    const qint64 averageXactionCollectingNs = averageNanoseconds(series, &BenchmarkSample::xactionCollectingNs);
    const qint64 averageXactionFinalizingNs = averageNanoseconds(series, &BenchmarkSample::xactionFinalizingNs);
    const qint64 averageXactionFlushingRowsNs = averageNanoseconds(series, &BenchmarkSample::xactionFlushingRowsNs);
    const qint64 averageXactionWritingDirectoriesNs =
        averageNanoseconds(series, &BenchmarkSample::xactionWritingDirectoriesNs);
    const qint64 averageTransactionSpanBuildNs =
        averageNanoseconds(series, &BenchmarkSample::transactionSpanBuildNs);
    const qint64 averageTransactionRenderNs =
        averageNanoseconds(series, &BenchmarkSample::transactionRenderNs);
    const qint64 averageClipboardBatchScanNs =
        averageNanoseconds(series, &BenchmarkSample::clipboardBatchScanNs);
    const qint64 averageClipboardBatchCollectNs =
        averageNanoseconds(series, &BenchmarkSample::clipboardBatchCollectNs);
    const qint64 averageClipboardBatchDecodeNs =
        averageNanoseconds(series, &BenchmarkSample::clipboardBatchDecodeNs);
    const qint64 averageClipboardInsertPrepareNs =
        averageNanoseconds(series, &BenchmarkSample::clipboardInsertPrepareNs);
    const qint64 averageClipboardInsertPublishNs =
        averageNanoseconds(series, &BenchmarkSample::clipboardInsertPublishNs);
    const qint64 averageClipboardInsertRefreshNs =
        averageNanoseconds(series, &BenchmarkSample::clipboardInsertRefreshNs);
    const qint64 averageClipboardInsertHeartbeatMaxNs =
        averageNanoseconds(series, &BenchmarkSample::clipboardInsertHeartbeatMaxNs);
    const qint64 averageSetRowsNs = averageNanoseconds(series, &BenchmarkSample::setRowsNs);
    const qint64 averageFilterNs = averageNanoseconds(series, &BenchmarkSample::filterNs);
    const qint64 averageResizeNs = averageNanoseconds(series, &BenchmarkSample::resizeColumnsNs);
    const qint64 averageEditablePrepareNs = averageNanoseconds(series, &BenchmarkSample::editablePrepareNs);
    const qint64 averageEditableFirstCellNs = averageNanoseconds(series, &BenchmarkSample::editableFirstCellNs);
    const qint64 averageTotalNs = averageScanNs + averageRowLoadNs + averageXactionIndexNs
        + averageTransactionSpanBuildNs + averageTransactionRenderNs
        + averageSetRowsNs + averageFilterNs + averageResizeNs
        + averageClipboardInsertPrepareNs + averageClipboardInsertPublishNs
        + averageClipboardInsertRefreshNs;
    const qsizetype averageRows = series.samples.empty()
        ? 0
        : series.samples.front().rowCount;

    out << "\nAverage    "
        << QString::number(averageRows).rightJustified(8, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageScanNs).rightJustified(8, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageRowLoadNs).rightJustified(8, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageXactionIndexNs).rightJustified(8, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageTransactionSpanBuildNs).rightJustified(9, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageTransactionRenderNs).rightJustified(10, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageSetRowsNs).rightJustified(11, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageFilterNs).rightJustified(10, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageResizeNs).rightJustified(10, QLatin1Char(' ')) << "  "
        << formatMilliseconds(averageTotalNs).rightJustified(9, QLatin1Char(' ')) << "  "
        << formatRecordsPerSecond(averageRows, averageRowLoadNs).rightJustified(10, QLatin1Char(' ')) << "\n";
    BenchmarkSample averageSample;
    averageSample.rowCount = averageRows;
    averageSample.xactionIndexNs = averageXactionIndexNs;
    averageSample.xactionIndexingNs = averageXactionIndexingNs;
    averageSample.xactionMergingNs = averageXactionMergingNs;
    averageSample.xactionCollectingNs = averageXactionCollectingNs;
    averageSample.xactionFinalizingNs = averageXactionFinalizingNs;
    averageSample.xactionFlushingRowsNs = averageXactionFlushingRowsNs;
    averageSample.xactionWritingDirectoriesNs = averageXactionWritingDirectoriesNs;
    averageSample.transactionSpanBuildNs = averageTransactionSpanBuildNs;
    averageSample.transactionRenderNs = averageTransactionRenderNs;
    averageSample.clipboardBatchScanNs = averageClipboardBatchScanNs;
    averageSample.clipboardBatchCollectNs = averageClipboardBatchCollectNs;
    averageSample.clipboardBatchDecodeNs = averageClipboardBatchDecodeNs;
    averageSample.clipboardInsertPrepareNs = averageClipboardInsertPrepareNs;
    averageSample.clipboardInsertPublishNs = averageClipboardInsertPublishNs;
    averageSample.clipboardInsertRefreshNs = averageClipboardInsertRefreshNs;
    averageSample.clipboardInsertHeartbeatMaxNs = averageClipboardInsertHeartbeatMaxNs;
    averageSample.editablePrepareNs = averageEditablePrepareNs;
    averageSample.editableFirstCellNs = averageEditableFirstCellNs;
    if (!series.samples.empty()) {
        averageSample.transactionSpanCount = series.samples.front().transactionSpanCount;
        averageSample.transactionLaneCount = series.samples.front().transactionLaneCount;
        averageSample.transactionSpanWorkerCount = series.samples.front().transactionSpanWorkerCount;
        averageSample.transactionRenderCandidateCount = series.samples.front().transactionRenderCandidateCount;
        averageSample.transactionRenderDense = series.samples.front().transactionRenderDense;
        averageSample.transactionRenderRepeats = series.samples.front().transactionRenderRepeats;
        averageSample.transactionRenderVerticalSteps = series.samples.front().transactionRenderVerticalSteps;
        averageSample.clipboardBatchAnchorCount = series.samples.front().clipboardBatchAnchorCount;
        averageSample.clipboardBatchMatchingOrdinalCount =
            series.samples.front().clipboardBatchMatchingOrdinalCount;
        averageSample.clipboardBatchCollectedRows = series.samples.front().clipboardBatchCollectedRows;
        averageSample.clipboardBatchDecodedRows = series.samples.front().clipboardBatchDecodedRows;
        averageSample.clipboardInsertAlgorithm = series.samples.front().clipboardInsertAlgorithm;
        averageSample.clipboardInsertExistingRows = series.samples.front().clipboardInsertExistingRows;
        averageSample.clipboardInsertRows = series.samples.front().clipboardInsertRows;
        for (const BenchmarkSample& sample : series.samples) {
            averageSample.transactionSpanMetadataMs += sample.transactionSpanMetadataMs;
            averageSample.transactionSpanScanMs += sample.transactionSpanScanMs;
            averageSample.transactionSpanMergeMs += sample.transactionSpanMergeMs;
            averageSample.transactionSpanFirstInfoMs += sample.transactionSpanFirstInfoMs;
            averageSample.transactionSpanBuildOnlyMs += sample.transactionSpanBuildOnlyMs;
            averageSample.transactionSpanSortMs += sample.transactionSpanSortMs;
            averageSample.transactionSpanLayoutMs += sample.transactionSpanLayoutMs;
            averageSample.transactionRenderFirstNs += sample.transactionRenderFirstNs;
            averageSample.transactionRenderRepeatNs += sample.transactionRenderRepeatNs;
        }
        const double divisor = static_cast<double>(series.samples.size());
        averageSample.transactionSpanMetadataMs /= divisor;
        averageSample.transactionSpanScanMs /= divisor;
        averageSample.transactionSpanMergeMs /= divisor;
        averageSample.transactionSpanFirstInfoMs /= divisor;
        averageSample.transactionSpanBuildOnlyMs /= divisor;
        averageSample.transactionSpanSortMs /= divisor;
        averageSample.transactionSpanLayoutMs /= divisor;
        averageSample.transactionRenderFirstNs /= static_cast<qint64>(series.samples.size());
        averageSample.transactionRenderRepeatNs /= static_cast<qint64>(series.samples.size());
    }
    printXactionStageSummary(out, averageSample);
    printTransactionSpanSummary(out, averageSample);
    printClipboardBatchSummary(out, averageSample);
    printClipboardInsertSummary(out, averageSample);
    printEditableSummary(out, averageSample);

    return 0;
}
