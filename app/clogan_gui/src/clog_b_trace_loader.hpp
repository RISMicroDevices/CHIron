#pragma once

#include "clog/clog_b/clog_b.hpp"
#include "clog/clog.hpp"
#include "flit_record.hpp"

#include <QString>

#include <cstdint>
#include <functional>
#include <limits>
#include <stop_token>
#include <array>
#include <unordered_map>
#include <vector>
#include <optional>

namespace CHIron::Gui {

struct CLogBTraceParameterComparison {
    QString name;
    QString traceValue;
    QString applicationValue;
};

using CLogBTraceFlitLengthMask = std::array<std::uint64_t, 4>;
using CLogBTraceFlitLengthMasks = std::array<CLogBTraceFlitLengthMask, 4>;
using CLogBTraceChannelMask = std::uint16_t;

inline constexpr CLogBTraceChannelMask CLogBChannelBit(const CLog::Channel channel) noexcept
{
    return static_cast<CLogBTraceChannelMask>(
        CLogBTraceChannelMask{1} << static_cast<unsigned int>(channel));
}

inline constexpr CLogBTraceChannelMask CLogBAllChannelMask() noexcept
{
    return CLogBChannelBit(CLog::Channel::TXREQ)
         | CLogBChannelBit(CLog::Channel::TXRSP)
         | CLogBChannelBit(CLog::Channel::TXDAT)
         | CLogBChannelBit(CLog::Channel::TXSNP)
         | CLogBChannelBit(CLog::Channel::RXREQ)
         | CLogBChannelBit(CLog::Channel::RXRSP)
         | CLogBChannelBit(CLog::Channel::RXDAT)
         | CLogBChannelBit(CLog::Channel::RXSNP)
         | CLogBChannelBit(CLog::Channel::TXREQ_BeforeSAM)
         | CLogBChannelBit(CLog::Channel::RXREQ_BeforeSAM);
}

struct CLogBTraceLoadError {
    enum class Type {
        Generic,
        UnsupportedParameters,
        FlitWidthMismatch,
        Cancelled
    };

    Type type = Type::Generic;
    QString summary;
    QString informativeText;
    QString flitReportText;
    QString detailedText;
    std::vector<CLogBTraceParameterComparison> parameterDifferences;
};

using CLogBTraceLoadProgressCallback =
    std::function<void(std::uint64_t processedBytes, std::uint64_t totalBytes)>;

enum class CLogBTraceLoadStage {
    Opening,
    Parsing,
    Decoding,
    Formatting,
    Finalizing,
    CollectingCacheStateErrors,
    FinalizingIndexDebug,
    FinalizingIndexLayout,
    FinalizingIndexRows,
    FinalizingIndexRowDirectory,
    Completed
};

using CLogBTraceLoadStageCallback =
    std::function<void(CLogBTraceLoadStage stage, std::size_t workerCount, std::size_t totalRecords)>;

using CLogBTraceLoadStageProgressCallback =
    std::function<void(std::size_t completedRecords, std::size_t totalRecords)>;

struct CLogBTraceLoadWorkerProgress {
    std::size_t workerIndex = 0;
    std::size_t completedRecords = 0;
    std::size_t totalRecords = 0;
};

using CLogBTraceLoadWorkerProgressCallback =
    std::function<void(const CLogBTraceLoadWorkerProgress&)>;

struct CLogBTraceLoadCallbacks {
    CLogBTraceLoadProgressCallback progress;
    CLogBTraceLoadStageCallback stage;
    CLogBTraceLoadStageProgressCallback stageProgress;
    CLogBTraceLoadWorkerProgressCallback workerProgress;
};

struct CLogBTraceBlockSummary {
    std::uint64_t fileOffset = 0;
    std::uint64_t payloadFileOffset = 0;
    std::uint64_t serializedSize = 0;
    std::uint64_t rowStart = 0;
    std::uint32_t recordCount = 0;
    std::uint32_t compressedBytes = 0;
    std::uint32_t uncompressedBytes = 0;
    qint64 firstTimestamp = 0;
    qint64 lastTimestamp = 0;
    CLogBTraceChannelMask channelMask = 0;
    std::array<std::uint32_t, 4> channelCounts{};
    CLogBTraceFlitLengthMasks flitLengthMasks{};
};

struct CLogBTraceMetadata {
    QString filePath;
    CLog::Parameters parameters;
    std::unordered_map<quint16, QString> nodeAnnotations;
    std::unordered_map<quint16, CLog::NodeType> nodeTypes;
    std::vector<CLogBTraceBlockSummary> blocks;
    std::uint64_t totalRecords = 0;
    qint64 firstTimestamp = 0;
    qint64 lastTimestamp = 0;
    std::array<std::uint64_t, 4> channelCounts{};
    CLogBTraceFlitLengthMasks flitLengthMasks{};
};

struct CLogBTraceConfigurationGuess {
    CLog::Parameters parameters;
    std::array<std::uint16_t, 4> expectedBitWidths{};
    std::array<std::uint8_t, 4> expectedByteLengths{};
    std::array<bool, 4> matchedChannels{};
};

struct CLogBTraceConfigurationGuessResult {
    CLog::Parameters declaredParameters;
    std::uint64_t totalRecords = 0;
    std::array<std::vector<std::uint8_t>, 4> observedByteLengths;
    std::vector<CLogBTraceConfigurationGuess> guesses;
};

struct CLogBTraceFastFilter {
    CLogBTraceChannelMask transportMask = CLogBAllChannelMask();
    QString opcodeFilter;
    QString sourceIdFilter;
    QString targetIdFilter;
    QString txnIdFilter;
    QString dbidFilter;
    QString addressFilter;
    std::array<std::vector<std::uint32_t>, 10> opcodeValuesByChannel;
};

struct CLogBTraceFastRecordInfo {
    std::uint8_t channel = 0;
    qint64 timestamp = 0;
    std::uint32_t nodeId = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t opcode = 0;
    std::uint32_t sourceId = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t targetId = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t txnId = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t dbid = std::numeric_limits<std::uint32_t>::max();
    std::uint64_t address = std::numeric_limits<std::uint64_t>::max();
};

struct CLogBTraceOptionalFieldValue {
    bool present = false;
    QString value;
    QString raw;
};

struct CLogBTraceXactionIndexRecord {
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
    std::uint32_t denialValue = 0;
    QString xactionType;
};

using CLogBTraceXactionIndexRecordCallback =
    std::function<bool(std::uint64_t logicalRow, CLogBTraceXactionIndexRecord record)>;

struct CLogBTraceCacheLineLifetime {
    std::uint32_t rnNodeId = (std::numeric_limits<std::uint32_t>::max)();
    QString rnNodeType;
    std::uint64_t lineAddress = 0;
    qint64 startTimestamp = 0;
    qint64 endTimestamp = 0;
    std::uint64_t startLogicalRow = 0;
    std::uint64_t endLogicalRow = 0;
    std::uint8_t startStateMask = 0;
    std::uint8_t endStateMask = 0;
    QString startStateText;
    QString endStateText;
    bool openEnded = false;
};

using CLogBTraceCacheLineLifetimeCallback =
    std::function<bool(CLogBTraceCacheLineLifetime lifetime)>;

struct CLogBTraceCacheLineStateQuery {
    std::uint32_t rnNodeId = (std::numeric_limits<std::uint32_t>::max)();
    std::uint64_t lineAddress = 0;
};

struct CLogBTraceCacheLineStateSpan {
    std::uint32_t rnNodeId = (std::numeric_limits<std::uint32_t>::max)();
    QString rnNodeType;
    std::uint64_t lineAddress = 0;
    qint64 startTimestamp = 0;
    qint64 endTimestamp = 0;
    std::uint64_t startLogicalRow = 0;
    std::uint64_t endLogicalRow = 0;
    std::uint8_t stateMask = 0;
    QString stateText;
    bool openEnded = false;
};

using CLogBTraceCacheLineStateSpanCallback =
    std::function<bool(CLogBTraceCacheLineStateSpan span)>;

struct CLogBTraceCacheReplayIssue {
    std::uint64_t logicalRow = 0;
    qint64 timestamp = 0;
    std::uint32_t rnNodeId = (std::numeric_limits<std::uint32_t>::max)();
    QString rnNodeType;
    std::uint64_t lineAddress = 0;
    QString denialName;
    QString denialCode;
    std::uint32_t denialValue = 0;
    QString channel;
    QString xactionType;
    QString jointType;
    QString jointPath;
};

using CLogBTraceCacheReplayIssueCallback =
    std::function<bool(CLogBTraceCacheReplayIssue issue)>;

class CLogBTraceLoader final {
public:
    static std::uint64_t normalizeCacheLineAddress(std::uint64_t address) noexcept;

    static bool scanFile(const QString& filePath,
                         CLogBTraceMetadata& metadata,
                         CLogBTraceLoadError& error,
                         const CLogBTraceLoadCallbacks& callbacks = {},
                         std::stop_token stopToken = {});

    static bool loadRows(const QString& filePath,
                         const CLogBTraceMetadata& metadata,
                         std::uint64_t beginRow,
                         std::uint64_t rowCount,
                         std::vector<FlitRecord>& records,
                         CLogBTraceLoadError& error,
                         const CLogBTraceLoadCallbacks& callbacks = {},
                         std::stop_token stopToken = {});

    static bool loadBlockRecords(const QString& filePath,
                                 const CLogBTraceMetadata& metadata,
                                 std::size_t blockIndex,
                                 std::shared_ptr<CLog::CLogB::TagCHIRecords>& block,
                                 CLogBTraceLoadError& error,
                                 const CLogBTraceLoadCallbacks& callbacks = {},
                                 std::stop_token stopToken = {});

    static bool decodeBlockRows(const CLogBTraceMetadata& metadata,
                                std::size_t blockIndex,
                                const CLog::CLogB::TagCHIRecords& block,
                                std::size_t localBegin,
                                std::size_t rowCount,
                                std::vector<FlitRecord>& records,
                                CLogBTraceLoadError& error,
                                const CLogBTraceLoadCallbacks& callbacks = {},
                                std::stop_token stopToken = {},
                                bool allowParallelDecode = true);

    static bool decodeRawRecord(const CLogBTraceMetadata& metadata,
                                qint64 timestamp,
                                CLog::Channel channel,
                                std::uint16_t nodeId,
                                std::uint8_t flitLength,
                                const std::vector<std::uint32_t>& flitWords,
                                FlitRecord& record,
                                CLogBTraceLoadError& error);

    static bool collectBlockRowsMatchingFilter(const CLogBTraceMetadata& metadata,
                                               std::size_t blockIndex,
                                               const CLog::CLogB::TagCHIRecords& block,
                                               const CLogBTraceFastFilter& filter,
                                               std::vector<std::size_t>& localRows,
                                               CLogBTraceLoadError& error,
                                               std::stop_token stopToken = {});

    static bool collectBlockRowsMatchingFilterAndBuildRecords(const CLogBTraceMetadata& metadata,
                                                              std::size_t blockIndex,
                                                              const CLog::CLogB::TagCHIRecords& block,
                                                              const CLogBTraceFastFilter& filter,
                                                              std::vector<std::size_t>& localRows,
                                                              std::vector<CLogBTraceFastRecordInfo>& fastRecords,
                                                              CLogBTraceLoadError& error,
                                                              std::stop_token stopToken = {});

    static void prepareFastFilter(CLogBTraceFastFilter& filter);
    static QString opcodeDisplayValue(const CLog::Parameters& params,
                                      const CLogBTraceFastRecordInfo& info);

    static bool buildBlockFastRecords(const CLogBTraceMetadata& metadata,
                                      std::size_t blockIndex,
                                      const CLog::CLogB::TagCHIRecords& block,
                                      std::vector<CLogBTraceFastRecordInfo>& records,
                                      CLogBTraceLoadError& error,
                                      std::stop_token stopToken = {});

    static bool buildBlockOptionalFieldRecords(const CLogBTraceMetadata& metadata,
                                               std::size_t blockIndex,
                                               const CLog::CLogB::TagCHIRecords& block,
                                               const QString& fieldName,
                                               std::vector<CLogBTraceOptionalFieldValue>& records,
                                               CLogBTraceLoadError& error,
                                               std::size_t maxWorkerCount = 1,
                                               const CLogBTraceLoadStageProgressCallback& progressCallback = {},
                                               std::stop_token stopToken = {});

    static bool buildXactionIndex(const QString& filePath,
                                  const CLogBTraceMetadata& metadata,
                                  std::vector<CLogBTraceXactionIndexRecord>& records,
                                  CLogBTraceLoadError& error,
                                  const CLogBTraceLoadCallbacks& callbacks = {},
                                  std::stop_token stopToken = {});
    static bool buildXactionIndex(const QString& filePath,
                                  const CLogBTraceMetadata& metadata,
                                  const CLogBTraceXactionIndexRecordCallback& recordCallback,
                                  CLogBTraceLoadError& error,
                                  const CLogBTraceLoadCallbacks& callbacks = {},
                                  std::stop_token stopToken = {});

    static bool buildCacheLineLifetimes(const QString& filePath,
                                        const CLogBTraceMetadata& metadata,
                                        std::vector<CLogBTraceCacheLineLifetime>& lifetimes,
                                        CLogBTraceLoadError& error,
                                        const CLogBTraceLoadCallbacks& callbacks = {},
                                        std::stop_token stopToken = {});
    static bool buildCacheLineLifetimes(const QString& filePath,
                                        const CLogBTraceMetadata& metadata,
                                        const CLogBTraceCacheLineLifetimeCallback& lifetimeCallback,
                                        CLogBTraceLoadError& error,
                                        const CLogBTraceLoadCallbacks& callbacks = {},
                                        std::stop_token stopToken = {});

    static bool buildCacheLineStateSpans(const QString& filePath,
                                         const CLogBTraceMetadata& metadata,
                                         CLogBTraceCacheLineStateQuery query,
                                         std::vector<CLogBTraceCacheLineStateSpan>& spans,
                                         CLogBTraceLoadError& error,
                                         const CLogBTraceLoadCallbacks& callbacks = {},
                                         std::stop_token stopToken = {});
    static bool buildCacheLineStateSpans(const QString& filePath,
                                         const CLogBTraceMetadata& metadata,
                                         CLogBTraceCacheLineStateQuery query,
                                         const CLogBTraceCacheLineStateSpanCallback& spanCallback,
                                         CLogBTraceLoadError& error,
                                         const CLogBTraceLoadCallbacks& callbacks = {},
                                         std::stop_token stopToken = {});
    static bool collectCacheStateReplayIssues(const QString& filePath,
                                              const CLogBTraceMetadata& metadata,
                                              const CLogBTraceCacheReplayIssueCallback& issueCallback,
                                              CLogBTraceLoadError& error,
                                              const CLogBTraceLoadCallbacks& callbacks = {},
                                              std::stop_token stopToken = {});

    static bool collectFastRecordRowsMatchingFilter(const CLogBTraceMetadata& metadata,
                                                    const CLogBTraceFastFilter& filter,
                                                    const std::vector<CLogBTraceFastRecordInfo>& records,
                                                    std::vector<std::size_t>& localRows,
                                                    CLogBTraceLoadError& error);
    static bool collectFastRecordRowsMatchingFilterRange(const CLogBTraceMetadata& metadata,
                                                         const CLogBTraceFastFilter& filter,
                                                         const std::vector<CLogBTraceFastRecordInfo>& records,
                                                         std::size_t localBegin,
                                                         std::size_t rowCount,
                                                         std::vector<std::size_t>& localRows,
                                                         CLogBTraceLoadError& error);

    static bool loadFile(const QString& filePath,
                         std::vector<FlitRecord>& records,
                         CLogBTraceLoadError& error,
                         const CLogBTraceLoadCallbacks& callbacks = {},
                         std::stop_token stopToken = {});

    static bool saveRowsAsCLogB(const QString& filePath,
                                const CLogBTraceMetadata& metadata,
                                const std::vector<FlitRecord>& records,
                                CLogBTraceLoadError& error);

    static bool saveRowsAsCLogB(const QString& filePath,
                                const CLogBTraceMetadata& metadata,
                                std::uint64_t rowCount,
                                const std::function<bool(std::uint64_t rowIndex, FlitRecord& record, CLogBTraceLoadError& error)>& rowProvider,
                                CLogBTraceLoadError& error);

    static bool guessFlitConfigurations(const QString& filePath,
                                        CLogBTraceConfigurationGuessResult& result,
                                        CLogBTraceLoadError& error,
                                        const CLogBTraceLoadCallbacks& callbacks = {},
                                        std::stop_token stopToken = {});

    static QStringList supportedOpcodeNames();
    static QString supportedConfigurationSummary();

private:
    CLogBTraceLoader() = delete;
};

}  // namespace CHIron::Gui
