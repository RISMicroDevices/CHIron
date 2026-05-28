#pragma once

#include "clog_b_trace_loader.hpp"
#include "trace_statistics.hpp"

#include <QHash>
#include <QSet>
#include <QString>

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CHIron::Gui {

struct TraceSessionOptions {
    std::size_t pageSize = 1024;
    std::size_t maxCachedPages = 8;
    std::size_t maxCachedBlocks = 2;
    std::optional<CLog::Parameters> parameterOverride;
    bool enableFastIndex = true;
};

class TraceSession final {
public:
    struct FastIndexDescriptor {
        std::uint64_t dataOffset = 0;
        std::uint64_t recordCount = 0;
        std::uint64_t dataBytes = 0;
    };

    struct XactionRowsDescriptor {
        std::uint64_t chunkTableOffset = 0;
        std::uint64_t chunkCount = 0;
        std::uint64_t rowCount = 0;
        std::uint64_t chunkTableBytes = 0;
    };

    struct XactionRowsDescriptorByOrdinal {
        std::uint64_t ordinal = 0;
        XactionRowsDescriptor descriptor;
    };

    struct XactionRowMapChunkDescriptor {
        std::uint64_t dataOffset = 0;
        std::uint32_t storedBytes = 0;
        std::uint32_t uncompressedBytes = 0;
        std::uint32_t rowCount = 0;
        std::uint8_t flags = 0;
    };

    struct XactionDebugChunkDescriptor {
        std::uint64_t dataOffset = 0;
        std::uint32_t storedBytes = 0;
        std::uint32_t uncompressedBytes = 0;
        std::uint32_t rowCount = 0;
        std::uint8_t flags = 0;
    };

    struct XactionRowDebugIds {
        std::uint32_t jointType = 0;
        std::uint32_t jointPath = 0;
        std::uint32_t denialName = 0;
        std::uint32_t denialCode = 0;
        std::uint32_t xactionType = 0;
    };

    struct OptionalFieldIndexState {
        QString path;
        std::uint64_t descriptorTableOffset = 0;
        bool readable = false;
        bool writable = false;
        std::vector<FastIndexDescriptor> descriptors;
    };

    struct XactionIndexRowInfo {
        bool processed = false;
        bool indexed = false;
        bool processedByJoint = false;
        bool xactionComplete = false;
        std::uint64_t xactionOrdinal = 0;
        QString transactionGroupKey;
        QString jointType;
        QString jointPath;
        QString denialName;
        QString denialCode;
        QString xactionType;
    };

    struct XactionRowCompactInfo {
        std::uint64_t xactionOrdinal = 0;
        std::uint8_t flags = 0;
    };

    struct TraceSessionSnapshot {
        QString filePath;
        CLogBTraceMetadata metadata;
        std::size_t pageSize = 0;
        std::size_t maxCachedPages = 0;
        std::size_t maxCachedBlocks = 0;
        QString fastIndexPath;
        bool fastIndexReadable = false;
        bool fastIndexWritable = false;
        std::vector<FastIndexDescriptor> fastIndexDescriptors;
        bool xactionIndexEnabled = false;
        bool xactionIndexStarted = false;
        bool xactionIndexComplete = false;
        QString xactionIndexPath;
        std::uint64_t xactionRowTableOffset = 0;
        std::uint64_t xactionGroupTableOffset = 0;
        std::uint64_t xactionGroupCount = 0;
        std::uint64_t xactionStringTableOffset = 0;
        std::uint64_t xactionStringCount = 0;
        std::vector<XactionRowMapChunkDescriptor> xactionRowMapChunkDescriptors;
        std::vector<XactionDebugChunkDescriptor> xactionDebugChunkDescriptors;
        std::vector<XactionRowsDescriptorByOrdinal> xactionRowsByOrdinalDescriptors;
        std::unordered_map<std::uint64_t,
                           std::shared_ptr<const std::vector<CLogBTraceFastRecordInfo>>> cachedFastRecordsByBlock;
    };

    static bool open(const QString& filePath,
                     std::shared_ptr<TraceSession>& session,
                     CLogBTraceLoadError& error,
                     const TraceSessionOptions& options = {},
                     const CLogBTraceLoadCallbacks& callbacks = {},
                     std::stop_token stopToken = {});

public:
    const QString& filePath() const noexcept;
    const CLogBTraceMetadata& metadata() const noexcept;
    std::uint64_t totalRows() const noexcept;
    std::size_t pageSize() const noexcept;
    std::size_t maxCachedPages() const noexcept;
    std::size_t maxCachedBlocks() const noexcept;
    std::size_t cachedPageCount() const noexcept;
    std::size_t cachedBlockCount() const noexcept;
    std::shared_ptr<const TraceSessionSnapshot> snapshot() const noexcept;
    const QString& fastIndexPath() const noexcept;
    bool isFastIndexReadable() const noexcept;
    bool isFastIndexWritable() const noexcept;
    bool readFastRecordsFromIndex(std::size_t blockIndex,
                                  std::vector<CLogBTraceFastRecordInfo>& records) const;
    bool readFastRecordsFromIndex(std::size_t blockIndex,
                                  std::size_t localBegin,
                                  std::size_t rowCount,
                                  std::vector<CLogBTraceFastRecordInfo>& records) const;
    bool readFastRecordsFromSnapshot(const std::shared_ptr<const TraceSessionSnapshot>& snapshot,
                                     std::size_t blockIndex,
                                     std::vector<CLogBTraceFastRecordInfo>& records) const;
    bool readFastRecordsFromSnapshot(const std::shared_ptr<const TraceSessionSnapshot>& snapshot,
                                     std::size_t blockIndex,
                                     std::size_t localBegin,
                                     std::size_t rowCount,
                                     std::vector<CLogBTraceFastRecordInfo>& records) const;
    QString optionalFieldFastIndexPath(const QString& fieldName) const;
    bool hasOptionalFieldFastIndex(const QString& fieldName);
    bool clearOptionalFieldFastIndex(const QString& fieldName,
                                     CLogBTraceLoadError& error);

    void clearCache();
    void refreshCachedXactionIndexRows();
    bool ensureRows(std::uint64_t beginRow,
                    std::uint64_t rowCount,
                    CLogBTraceLoadError& error,
                    const CLogBTraceLoadCallbacks& callbacks = {},
                    std::stop_token stopToken = {});
    bool readRows(std::uint64_t beginRow,
                  std::uint64_t rowCount,
                  std::vector<FlitRecord>& rows,
                  CLogBTraceLoadError& error,
                  const CLogBTraceLoadCallbacks& callbacks = {},
                  std::stop_token stopToken = {});
    bool readRowsDirect(std::uint64_t beginRow,
                        std::uint64_t rowCount,
                        std::vector<FlitRecord>& rows,
                        CLogBTraceLoadError& error,
                        const CLogBTraceLoadCallbacks& callbacks = {},
                        std::stop_token stopToken = {}) const;
    bool readRowsDirect(const std::shared_ptr<const TraceSessionSnapshot>& snapshot,
                        std::uint64_t beginRow,
                        std::uint64_t rowCount,
                        std::vector<FlitRecord>& rows,
                        CLogBTraceLoadError& error,
                        const CLogBTraceLoadCallbacks& callbacks = {},
                        std::stop_token stopToken = {}) const;
    bool collectRowsForTransportMask(std::size_t blockIndex,
                                     CLogBTraceChannelMask enabledMask,
                                     std::vector<int>& logicalRows,
                                     CLogBTraceLoadError& error,
                                     const CLogBTraceLoadCallbacks& callbacks = {},
                                     std::stop_token stopToken = {});
    bool collectRowsMatchingFastFilter(std::size_t blockIndex,
                                       const CLogBTraceFastFilter& filter,
                                       std::vector<int>& logicalRows,
                                       CLogBTraceLoadError& error,
                                       const CLogBTraceLoadCallbacks& callbacks = {},
                                       std::stop_token stopToken = {});
    bool collectRowsMatchingFastFilterRange(std::size_t blockIndex,
                                            const CLogBTraceFastFilter& filter,
                                            std::size_t localBegin,
                                            std::size_t rowCount,
                                            std::vector<int>& logicalRows,
                                            CLogBTraceLoadError& error,
                                            std::stop_token stopToken = {});
    bool collectRowsMatchingFastFilterAndOptionalFieldIndexesRange(
        std::size_t blockIndex,
        const CLogBTraceFastFilter& filter,
        const QHash<QString, QString>& optionalFieldFilters,
        std::size_t localBegin,
        std::size_t rowCount,
        std::vector<int>& logicalRows,
        CLogBTraceLoadError& error,
        std::stop_token stopToken = {});
    bool buildOptionalFieldFastIndex(const QString& fieldName,
                                     CLogBTraceLoadError& error,
                                     const CLogBTraceLoadCallbacks& callbacks = {},
                                     std::stop_token stopToken = {});
    bool supportsXactionIndexing() const noexcept;
    bool isXactionIndexStarted() const noexcept;
    bool isXactionIndexComplete() const noexcept;
    std::vector<std::uint64_t> xactionOrdinals() const;
    bool xactionRowCountsByOrdinal(std::unordered_map<std::uint64_t, std::uint64_t>& rowCounts) const;
    bool xactionTypesByOrdinal(std::unordered_map<std::uint64_t, QString>& types) const;
    bool xactionCompletedOrdinals(std::unordered_set<std::uint64_t>& ordinals) const;
    bool xactionRowInfoRange(std::uint64_t beginRow,
                             std::uint64_t rowCount,
                             std::vector<XactionIndexRowInfo>& infos,
                             bool includeDebugStrings = true) const;
    bool xactionRowCompactInfoRange(std::uint64_t beginRow,
                                    std::uint64_t rowCount,
                                    std::vector<XactionRowCompactInfo>& infos) const;
    bool xactionRowCompactInfoRange(const std::shared_ptr<const TraceSessionSnapshot>& snapshot,
                                    std::uint64_t beginRow,
                                    std::uint64_t rowCount,
                                    std::vector<XactionRowCompactInfo>& infos) const;
    bool xactionRowInfosForRows(const std::vector<std::uint64_t>& logicalRows,
                                std::unordered_map<std::uint64_t, XactionIndexRowInfo>& infos,
                                bool includeDebugStrings = true,
                                const std::function<void(std::size_t completedRows,
                                                         std::size_t totalRows)>& progressCallback = {}) const;
    bool xactionOrdinalsByRowRange(std::uint64_t beginRow,
                                   std::uint64_t rowCount,
                                   std::vector<std::uint64_t>& ordinals) const;
    std::vector<std::uint64_t> xactionRowsForOrdinal(std::uint64_t ordinal) const;
    std::vector<std::uint64_t> xactionRowsForOrdinal(
        const std::shared_ptr<const TraceSessionSnapshot>& snapshot,
        std::uint64_t ordinal) const;
    std::vector<std::uint64_t> xactionRowsForGroup(const QString& groupKey) const;
    bool xactionRowInfo(std::uint64_t logicalRow, XactionIndexRowInfo& info) const;
    QString xactionDebugInfo(std::uint64_t logicalRow) const;
    bool collectRowsMatchingXactionDeniedRange(std::uint64_t beginRow,
                                               std::uint64_t rowCount,
                                               std::vector<int>& logicalRows) const;
    void markXactionIndexStarted() noexcept;
    void clearXactionIndex();
    bool buildXactionIndex(CLogBTraceLoadError& error,
                           const CLogBTraceLoadCallbacks& callbacks = {},
                           std::stop_token stopToken = {});
    bool readFastRecords(std::size_t blockIndex,
                         std::size_t localBegin,
                         std::size_t rowCount,
                         std::vector<CLogBTraceFastRecordInfo>& records,
                         CLogBTraceLoadError& error,
                         std::stop_token stopToken = {});
    bool isRowCached(std::uint64_t row) const noexcept;
    const FlitRecord* tryGetRow(std::uint64_t row) const noexcept;

private:
    struct CachedPage {
        std::uint64_t pageIndex = 0;
        std::uint64_t rowStart = 0;
        std::vector<FlitRecord> rows;
    };

    struct CachedPageEntry {
        CachedPage page;
        std::list<std::uint64_t>::iterator lruIt;
    };

    struct CachedBlockEntry {
        std::shared_ptr<CLog::CLogB::TagCHIRecords> block;
        std::shared_ptr<std::vector<CLogBTraceFastRecordInfo>> fastRecords;
        std::list<std::uint64_t>::iterator lruIt;
    };

    struct CachedOptionalFieldIndexRecord {
        bool present = false;
        QString value;
        QString raw;
        bool numeric = false;
        qulonglong number = 0;
    };

    struct CachedOptionalFieldBlockEntry {
        std::shared_ptr<std::vector<CachedOptionalFieldIndexRecord>> records;
        std::uint64_t approximateBytes = 0;
        std::list<QString>::iterator lruIt;
    };

    struct CachedXactionOrdinalChunkEntry {
        std::shared_ptr<std::vector<std::uint64_t>> ordinals;
        std::uint32_t rowCount = 0;
        std::list<std::uint64_t>::iterator lruIt;
    };

private:
    TraceSession(QString filePath,
                 CLogBTraceMetadata metadata,
                 TraceSessionOptions options,
                 const CLogBTraceLoadCallbacks& callbacks = {},
                 std::stop_token stopToken = {});

private:
    bool areRowsCached(std::uint64_t beginRow, std::uint64_t rowCount) const noexcept;
    bool loadAlignedRangeAndCache(std::uint64_t beginRow,
                                  std::uint64_t rowCount,
                                  CLogBTraceLoadError& error,
                                  const CLogBTraceLoadCallbacks& callbacks = {},
                                  std::stop_token stopToken = {});
    bool ensureBlockLoaded(std::uint64_t blockIndex,
                           CLogBTraceLoadError& error,
                           const CLogBTraceLoadCallbacks& callbacks = {},
                           std::stop_token stopToken = {});
    std::shared_ptr<CLog::CLogB::TagCHIRecords> cachedBlock(std::uint64_t blockIndex) const noexcept;
    bool ensureFastRecordsLoaded(std::uint64_t blockIndex,
                                 CLogBTraceLoadError& error,
                                 std::stop_token stopToken = {});
    std::shared_ptr<const std::vector<CLogBTraceFastRecordInfo>> cachedFastRecords(std::uint64_t blockIndex) const noexcept;
    void initializeFastIndexState() noexcept;
    bool ensureOptionalFieldIndexState(const QString& fieldName) noexcept;
    bool initializeOptionalFieldIndexFile(const QString& fieldName,
                                          OptionalFieldIndexState& state) noexcept;
    bool tryLoadOptionalFieldIndexState(const QString& fieldName,
                                        OptionalFieldIndexState& state) noexcept;
    bool tryLoadFastRecordsFromIndex(std::uint64_t blockIndex,
                                     std::vector<CLogBTraceFastRecordInfo>& records) noexcept;
    void tryPersistFastRecordsToIndex(std::uint64_t blockIndex,
                                      const std::vector<CLogBTraceFastRecordInfo>& records) noexcept;
    bool ensureOptionalFieldRecordsLoaded(std::uint64_t blockIndex,
                                          const QString& fieldName,
                                          const OptionalFieldIndexState& state,
                                          CLogBTraceLoadError& error);
    std::shared_ptr<const std::vector<CachedOptionalFieldIndexRecord>>
    cachedOptionalFieldRecords(std::uint64_t blockIndex,
                               const QString& fieldName) const noexcept;
    void storeOptionalFieldRecords(std::uint64_t blockIndex,
                                   const QString& fieldName,
                                   std::shared_ptr<std::vector<CachedOptionalFieldIndexRecord>> records,
                                   std::uint64_t approximateBytes);
    void touchOptionalFieldBlock(const QString& cacheKey) noexcept;
    void clearOptionalFieldRecordCache(const QString& fieldName = QString());
    void evictOptionalFieldBlocksIfNeeded();
    bool tryLoadXactionIndexState(const CLogBTraceLoadCallbacks& callbacks = {},
                                  std::stop_token stopToken = {});
    void resetXactionIndexFiles();
    void resetXactionIndexFileState();
    bool readXactionRowRecord(std::uint64_t logicalRow,
                              CLogBTraceXactionIndexRecord& record) const;
    bool readXactionRowRecords(std::uint64_t beginRow,
                               std::uint64_t rowCount,
                               std::vector<CLogBTraceXactionIndexRecord>& records,
                               bool includeDebugStrings = true) const;
    bool readXactionRowRecordsFromSnapshot(const TraceSessionSnapshot& snapshot,
                                           std::uint64_t beginRow,
                                           std::uint64_t rowCount,
                                           std::vector<CLogBTraceXactionIndexRecord>& records,
                                           bool includeDebugStrings = true) const;
    bool readXactionOrdinalChunk(const QString& indexPath,
                                 const XactionRowMapChunkDescriptor& descriptor,
                                 std::vector<std::uint64_t>& ordinals) const;
    bool readXactionRowsByOrdinal(std::uint64_t ordinal,
                                  std::vector<std::uint64_t>& rows) const;
    bool readXactionRowsByOrdinal(const TraceSessionSnapshot& snapshot,
                                  std::uint64_t ordinal,
                                  std::vector<std::uint64_t>& rows) const;
    bool xactionGroupDescriptors(std::vector<XactionRowsDescriptorByOrdinal>& descriptors) const;
    bool readXactionDebugStrings(const XactionRowDebugIds& debugIds,
                                 CLogBTraceXactionIndexRecord& record) const;
    bool readXactionDebugIds(std::uint64_t logicalRow,
                             XactionRowDebugIds& debugIds) const;
    QString buildPersistedXactionDebugInfo(std::uint64_t logicalRow,
                                           const CLogBTraceXactionIndexRecord& record) const;
    bool ensurePageLoaded(std::uint64_t pageIndex,
                          CLogBTraceLoadError& error,
                          const CLogBTraceLoadCallbacks& callbacks = {},
                          std::stop_token stopToken = {});
    void cacheRows(std::uint64_t beginRow, std::vector<FlitRecord> rows);
    void storePage(CachedPage page);
    void storeBlock(std::uint64_t blockIndex, std::shared_ptr<CLog::CLogB::TagCHIRecords> block);
    void touchPage(std::uint64_t pageIndex) noexcept;
    void touchBlock(std::uint64_t blockIndex) noexcept;
    void evictPagesIfNeeded();
    void evictBlocksIfNeeded();
    void initializeXactionIndexState(const CLogBTraceLoadCallbacks& callbacks = {},
                                     std::stop_token stopToken = {});
    void applyXactionIndexToRows(std::uint64_t beginRow,
                                 std::vector<FlitRecord>& rows) const;
    void applyXactionIndexToRows(const TraceSessionSnapshot& snapshot,
                                 std::uint64_t beginRow,
                                 std::vector<FlitRecord>& rows) const;
    void publishSnapshot() const;
    void publishSnapshotNoThrow() const noexcept;

private:
    QString filePath_;
    CLogBTraceMetadata metadata_;
    std::size_t pageSize_ = 1024;
    std::size_t maxCachedPages_ = 8;
    std::size_t maxCachedBlocks_ = 2;
    QString fastIndexPath_;
    bool fastIndexEnabled_ = true;
    bool fastIndexReadable_ = false;
    bool fastIndexWritable_ = false;
    bool xactionIndexEnabled_ = false;
    bool xactionIndexStarted_ = false;
    bool xactionIndexComplete_ = false;
    QString xactionIndexPath_;
    std::uint64_t xactionRowTableOffset_ = 0;
    std::uint64_t xactionGroupTableOffset_ = 0;
    std::uint64_t xactionGroupCount_ = 0;
    std::uint64_t xactionStringTableOffset_ = 0;
    std::uint64_t xactionStringCount_ = 0;
    std::vector<XactionRowMapChunkDescriptor> xactionRowMapChunkDescriptors_;
    std::vector<XactionDebugChunkDescriptor> xactionDebugChunkDescriptors_;
    mutable std::vector<XactionRowsDescriptorByOrdinal> xactionRowsByOrdinalDescriptors_;
    mutable bool xactionRowsByOrdinalDescriptorsLoaded_ = false;
    mutable std::optional<std::unordered_map<std::uint64_t, QString>> cachedXactionTypesByOrdinal_;
    mutable std::optional<std::unordered_set<std::uint64_t>> cachedXactionCompletedOrdinals_;
    mutable std::unordered_map<std::uint64_t, CachedXactionOrdinalChunkEntry> cachedXactionOrdinalChunks_;
    mutable std::list<std::uint64_t> lruXactionOrdinalChunkIndexes_;
    std::uint64_t maxCachedOptionalFieldBlockBytes_ = 256ULL << 20;
    std::uint64_t cachedOptionalFieldBlockBytes_ = 0;
    std::vector<FastIndexDescriptor> fastIndexDescriptors_;
    mutable std::mutex xactionIndexMutex_;
    mutable std::mutex optionalFieldIndexesMutex_;
    mutable std::mutex optionalFieldRecordCacheMutex_;
    QHash<QString, OptionalFieldIndexState> optionalFieldIndexes_;
    QHash<QString, CachedOptionalFieldBlockEntry> cachedOptionalFieldBlocks_;
    std::list<QString> lruOptionalFieldBlockKeys_;
    mutable std::recursive_mutex rowBlockCacheMutex_;
    std::unordered_map<std::uint64_t, CachedPageEntry> cachedPages_;
    std::list<std::uint64_t> lruPageIndexes_;
    std::unordered_map<std::uint64_t, CachedBlockEntry> cachedBlocks_;
    std::list<std::uint64_t> lruBlockIndexes_;
    mutable std::shared_ptr<const TraceSessionSnapshot> snapshot_;
};

}  // namespace CHIron::Gui
