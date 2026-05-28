#include "trace_session.hpp"
#include "trace_session_fast_index_utils.hpp"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QThread>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <utility>

namespace CHIron::Gui {

Q_LOGGING_CATEGORY(traceSessionLog, "clogan.trace.session")

namespace {

constexpr std::array<char, 8> kOptionalFieldIndexMagic = {'C', 'H', 'O', 'F', 'I', 'D', '1', '\0'};
constexpr std::uint32_t kOptionalFieldIndexVersion = 1;
constexpr std::array<char, 8> kXactionIndexMagic = {'C', 'H', 'X', 'A', 'C', 'T', '1', '\0'};
constexpr std::uint32_t kXactionIndexVersion = 5;
constexpr std::uint64_t kXactionRowChunkRows = 1024;
constexpr std::uint64_t kXactionRowMapChunksPerWriteSegment = 256;
constexpr std::uint64_t kXactionRowMapChunksPerWriteFlush = 32;
constexpr std::size_t kXactionRowMapChunkPrepareThreadLimit = 8;
constexpr std::uint8_t kXactionRowChunkCompressed = 1U;
constexpr qsizetype kXactionMinCompressBytes = 64 * 1024;
constexpr int kXactionCompressionMinSavingsPercent = 10;
constexpr qsizetype kXactionWriteFlushBytes = 32 * 1024 * 1024;

constexpr std::uint64_t kXactionIndexHeaderSerializedBytes =
    sizeof(char) * kXactionIndexMagic.size()
    + sizeof(std::uint32_t)
    + sizeof(std::uint64_t) * 12;
constexpr std::uint64_t kXactionGroupDescriptorSerializedBytes =
    sizeof(std::uint64_t) * 4;
constexpr std::uint64_t kXactionRowChunkDescriptorSerializedBytes =
    sizeof(std::uint64_t) * 2 + sizeof(std::uint32_t) * 2 + sizeof(std::uint8_t);
constexpr std::uint64_t kXactionRowMapChunkRows = 262144;
constexpr std::uint64_t kXactionRowMapChunkDescriptorSerializedBytes =
    sizeof(std::uint64_t) + sizeof(std::uint32_t) * 3 + sizeof(std::uint8_t);
constexpr std::uint64_t kXactionStringDescriptorSerializedBytes =
    sizeof(std::uint64_t) + sizeof(std::uint32_t);
constexpr std::size_t kMaxCachedXactionOrdinalChunks = 32;
constexpr qint64 kTraceSessionLockWarnMs = 16;

enum XactionRowFlags : std::uint8_t {
    XactionRowProcessed = 1U << 0,
    XactionRowIndexed = 1U << 1,
    XactionRowProcessedByJoint = 1U << 2,
    XactionRowComplete = 1U << 3,
};

void LogTraceSessionLockWait(const char* lockName, const qint64 waitedMs)
{
    if (waitedMs >= kTraceSessionLockWarnMs) {
        qCWarning(traceSessionLog).nospace()
            << "Waited " << waitedMs << " ms for " << lockName
            << " on thread " << QThread::currentThread();
    }
}

struct FastIndexHeader {
    std::array<char, 8> magic = Detail::kFastIndexMagic;
    std::uint32_t version = Detail::kFastIndexVersion;
    std::uint64_t sourceFileSize = 0;
    std::int64_t sourceLastModifiedMs = 0;
    std::uint64_t blockCount = 0;
};

struct XactionIndexHeader {
    std::array<char, 8> magic = kXactionIndexMagic;
    std::uint32_t version = kXactionIndexVersion;
    std::uint64_t sourceFileSize = 0;
    std::int64_t sourceLastModifiedMs = 0;
    std::uint64_t totalRows = 0;
    std::uint64_t rowTableOffset = 0;
    std::uint64_t groupTableOffset = 0;
    std::uint64_t groupCount = 0;
    std::uint64_t debugDescriptorTableOffset = 0;
    std::uint64_t debugChunkDescriptorTableOffset = 0;
    std::uint64_t debugChunkCount = 0;
    std::uint64_t stringTableOffset = 0;
    std::uint64_t stringCount = 0;
    std::uint64_t rowMapChunkCount = 0;
};

struct XactionStringDescriptor {
    std::uint64_t dataOffset = 0;
    std::uint32_t bytes = 0;
};

struct XactionPreparedRowMapEntry {
    std::uint32_t localRow = 0;
    std::uint8_t flags = 0;
    std::uint64_t ordinal = 0;
};

struct XactionPreparedDebugEntry {
    std::uint32_t localRow = 0;
    TraceSession::XactionRowDebugIds debugIds;
};

struct XactionPreparedPayload {
    QByteArray stored;
    std::uint32_t uncompressedBytes = 0;
    std::uint8_t flags = 0;
};

struct XactionRowChunkDescriptor {
    std::uint64_t dataOffset = 0;
    std::uint64_t rowCount = 0;
    std::uint32_t storedBytes = 0;
    std::uint32_t uncompressedBytes = 0;
    std::uint8_t flags = 0;
};

using OptionalFieldIndexRecord = CLogBTraceOptionalFieldValue;

std::uint8_t xactionRowFlagsForRecord(const CLogBTraceXactionIndexRecord& record);
CLogBTraceXactionIndexRecord xactionRecordFromFlags(std::uint8_t flags, std::uint64_t ordinal);

QString fastIndexPathForTrace(const QString& tracePath)
{
    return tracePath.isEmpty() ? QString() : (tracePath + QStringLiteral(".fastidx"));
}

QString xactionIndexPathForTrace(const QString& tracePath)
{
    return tracePath.isEmpty() ? QString() : (tracePath + QStringLiteral(".xactidx"));
}

QString optionalFieldIndexPathForTrace(const QString& tracePath, const QString& fieldName)
{
    if (tracePath.isEmpty() || fieldName.isEmpty()) {
        return QString();
    }

    const QByteArray digest =
        QCryptographicHash::hash(fieldName.toUtf8(), QCryptographicHash::Sha1).toHex();
    return tracePath + QStringLiteral(".fieldidx.") + QString::fromLatin1(digest) + QStringLiteral(".fastidx");
}

bool checkedMultiply(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& result);
bool readFastRecordsFromIndexFile(const QString& fastIndexPath,
                                  const TraceSession::FastIndexDescriptor& descriptor,
                                  std::size_t localBegin,
                                  std::size_t rowCount,
                                  std::vector<CLogBTraceFastRecordInfo>& records);

QString optionalFieldBlockCacheKey(const QString& fieldName, const std::uint64_t blockIndex)
{
    return fieldName + QChar(0x1F) + QString::number(static_cast<qulonglong>(blockIndex));
}

void removeOptionalFieldIndexFile(const QString& indexPath)
{
    if (!indexPath.isEmpty()) {
        QFile::remove(indexPath);
    }
}

void removeXactionIndexFile(const QString& indexPath)
{
    if (!indexPath.isEmpty()) {
        QFile::remove(indexPath);
    }
}

std::uint64_t traceSourceFileSize(const QString& tracePath)
{
    const QFileInfo fileInfo(tracePath);
    return fileInfo.exists() ? static_cast<std::uint64_t>(fileInfo.size()) : 0;
}

std::int64_t traceSourceLastModifiedMs(const QString& tracePath)
{
    const QFileInfo fileInfo(tracePath);
    return fileInfo.exists() ? fileInfo.lastModified().toMSecsSinceEpoch() : 0;
}

std::size_t optionalFieldIndexWorkerCount(const std::uint64_t totalRecords)
{
    if (totalRecords < 2048) {
        return 1;
    }

    constexpr unsigned int kMaxIndexWorkers = 8;
    const unsigned int hardwareWorkers = std::min(
        kMaxIndexWorkers,
        std::max(1U, std::thread::hardware_concurrency()));
    if (hardwareWorkers <= 1U) {
        return 1;
    }

    const std::size_t recordLimitedWorkers = std::max<std::size_t>(
        1,
        static_cast<std::size_t>((totalRecords + 2047) / 2048));
    return std::min<std::size_t>({static_cast<std::size_t>(hardwareWorkers),
                                  recordLimitedWorkers});
}

bool parseSearchInteger(QString text, qulonglong& value)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return false;
    }

    bool ok = false;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = text.sliced(2).toULongLong(&ok, 16);
        return ok;
    }

    bool hasHexLetter = false;
    for (const QChar ch : text) {
        if (ch.isDigit()) {
            continue;
        }

        const ushort code = ch.unicode();
        if ((code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F')) {
            hasHexLetter = true;
            continue;
        }

        return false;
    }

    value = text.toULongLong(&ok, hasHexLetter ? 16 : 10);
    return ok;
}

struct OptionalFieldIndexFilter {
    QString text;
    bool numeric = false;
    qulonglong number = 0;
};

OptionalFieldIndexFilter prepareOptionalFieldIndexFilter(const QString& filter)
{
    OptionalFieldIndexFilter prepared;
    prepared.text = filter;
    prepared.numeric = parseSearchInteger(filter, prepared.number);
    return prepared;
}

template <typename Record>
bool matchesOptionalFieldIndexRecord(const Record& record, const OptionalFieldIndexFilter& filter)
{
    if (filter.text.isEmpty()) {
        return true;
    }

    if (!record.present) {
        return false;
    }

    if (filter.numeric && record.numeric) {
        return filter.number == record.number;
    }

    return record.value.contains(filter.text, Qt::CaseInsensitive)
        || record.raw.contains(filter.text, Qt::CaseInsensitive);
}

bool loadFastIndexDescriptors(const QString& fastIndexPath,
                              const QString& tracePath,
                              const std::size_t blockCount,
                              std::vector<TraceSession::FastIndexDescriptor>& descriptors)
{
    QFile input(fastIndexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDataStream stream(&input);
    stream.setByteOrder(QDataStream::LittleEndian);

    FastIndexHeader header;
    if (stream.readRawData(header.magic.data(), static_cast<int>(header.magic.size()))
            != static_cast<int>(header.magic.size())
        || stream.atEnd()) {
        return false;
    }
    quint32 version = 0;
    quint64 sourceFileSize = 0;
    qint64 sourceLastModifiedMs = 0;
    quint64 storedBlockCount = 0;
    stream >> version >> sourceFileSize >> sourceLastModifiedMs >> storedBlockCount;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    header.version = version;
    header.sourceFileSize = sourceFileSize;
    header.sourceLastModifiedMs = sourceLastModifiedMs;
    header.blockCount = storedBlockCount;

    if (header.magic != Detail::kFastIndexMagic
        || header.version != Detail::kFastIndexVersion
        || header.sourceFileSize != traceSourceFileSize(tracePath)
        || header.sourceLastModifiedMs != traceSourceLastModifiedMs(tracePath)
        || header.blockCount != blockCount) {
        return false;
    }

    descriptors.resize(blockCount);
    if (blockCount == 0) {
        return true;
    }

    for (auto& descriptor : descriptors) {
        quint64 dataOffset = 0;
        quint64 recordCount = 0;
        quint64 dataBytes = 0;
        stream >> dataOffset >> recordCount >> dataBytes;
        if (stream.status() != QDataStream::Ok) {
            descriptors.clear();
            return false;
        }
        descriptor.dataOffset = dataOffset;
        descriptor.recordCount = recordCount;
        descriptor.dataBytes = dataBytes;
    }

    return true;
}

bool loadOptionalFieldIndexDescriptors(const QString& indexPath,
                                       const QString& tracePath,
                                       const QString& fieldName,
                                       const std::size_t blockCount,
                                       TraceSession::OptionalFieldIndexState& state)
{
    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDataStream stream(&input);
    stream.setByteOrder(QDataStream::LittleEndian);

    std::array<char, 8> magic{};
    if (stream.readRawData(magic.data(), static_cast<int>(magic.size()))
            != static_cast<int>(magic.size())
        || stream.atEnd()) {
        return false;
    }

    quint32 version = 0;
    quint64 sourceFileSize = 0;
    qint64 sourceLastModifiedMs = 0;
    quint64 storedBlockCount = 0;
    QString storedFieldName;
    stream >> version >> sourceFileSize >> sourceLastModifiedMs >> storedBlockCount >> storedFieldName;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    if (magic != kOptionalFieldIndexMagic
        || version != kOptionalFieldIndexVersion
        || sourceFileSize != traceSourceFileSize(tracePath)
        || sourceLastModifiedMs != traceSourceLastModifiedMs(tracePath)
        || storedBlockCount != blockCount
        || storedFieldName != fieldName) {
        return false;
    }

    state = {};
    state.path = indexPath;
    state.descriptorTableOffset = static_cast<std::uint64_t>(input.pos());
    state.descriptors.resize(blockCount);

    for (auto& descriptor : state.descriptors) {
        quint64 dataOffset = 0;
        quint64 recordCount = 0;
        quint64 dataBytes = 0;
        stream >> dataOffset >> recordCount >> dataBytes;
        if (stream.status() != QDataStream::Ok) {
            state = {};
            return false;
        }
        descriptor.dataOffset = dataOffset;
        descriptor.recordCount = recordCount;
        descriptor.dataBytes = dataBytes;
    }

    return true;
}

bool validateOptionalFieldIndexDescriptors(const TraceSession::OptionalFieldIndexState& state,
                                           const CLogBTraceMetadata& metadata)
{
    if (state.descriptors.size() != metadata.blocks.size()) {
        return false;
    }

    QFile file(state.path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(file.size());
    const std::uint64_t descriptorTableBytes =
        static_cast<std::uint64_t>(state.descriptors.size() * Detail::kFastIndexDescriptorSerializedBytes);
    const std::uint64_t descriptorTableEnd = state.descriptorTableOffset + descriptorTableBytes;
    if (state.descriptorTableOffset > fileSize
        || descriptorTableBytes > fileSize
        || descriptorTableEnd > fileSize) {
        return false;
    }

    for (std::size_t index = 0; index < state.descriptors.size(); ++index) {
        const TraceSession::FastIndexDescriptor& descriptor = state.descriptors[index];
        const std::uint64_t expectedRecordCount = metadata.blocks[index].recordCount;
        if (descriptor.recordCount != expectedRecordCount) {
            return false;
        }

        if (expectedRecordCount == 0) {
            if (descriptor.dataBytes != 0) {
                return false;
            }
            if (descriptor.dataOffset != 0
                && (descriptor.dataOffset < descriptorTableEnd || descriptor.dataOffset > fileSize)) {
                return false;
            }
            continue;
        }

        if (descriptor.dataOffset == 0
            || descriptor.dataBytes == 0
            || descriptor.dataOffset < descriptorTableEnd
            || descriptor.dataOffset > fileSize
            || descriptor.dataBytes > fileSize
            || descriptor.dataOffset + descriptor.dataBytes > fileSize) {
            return false;
        }
    }

    return true;
}

bool initializeFastIndexFile(const QString& fastIndexPath,
                             const QString& tracePath,
                             const std::size_t blockCount,
                             std::vector<TraceSession::FastIndexDescriptor>& descriptors)
{
    QFile output(fastIndexPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QDataStream stream(&output);
    stream.setByteOrder(QDataStream::LittleEndian);

    FastIndexHeader header;
    header.sourceFileSize = traceSourceFileSize(tracePath);
    header.sourceLastModifiedMs = traceSourceLastModifiedMs(tracePath);
    header.blockCount = blockCount;
    if (stream.writeRawData(header.magic.data(), static_cast<int>(header.magic.size()))
            != static_cast<int>(header.magic.size())) {
        return false;
    }
    stream << quint32(header.version)
           << quint64(header.sourceFileSize)
           << qint64(header.sourceLastModifiedMs)
           << quint64(header.blockCount);
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    descriptors.assign(blockCount, {});
    for (const auto& descriptor : descriptors) {
        stream << quint64(descriptor.dataOffset)
               << quint64(descriptor.recordCount)
               << quint64(descriptor.dataBytes);
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
    }

    return true;
}

bool createOptionalFieldIndexFile(const QString& indexPath,
                                  const QString& tracePath,
                                  const QString& fieldName,
                                  const std::size_t blockCount,
                                  TraceSession::OptionalFieldIndexState& state)
{
    QFile output(indexPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QDataStream stream(&output);
    stream.setByteOrder(QDataStream::LittleEndian);

    if (stream.writeRawData(kOptionalFieldIndexMagic.data(),
                            static_cast<int>(kOptionalFieldIndexMagic.size()))
            != static_cast<int>(kOptionalFieldIndexMagic.size())) {
        return false;
    }

    stream << quint32(kOptionalFieldIndexVersion)
           << quint64(traceSourceFileSize(tracePath))
           << qint64(traceSourceLastModifiedMs(tracePath))
           << quint64(blockCount)
           << fieldName;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    state = {};
    state.path = indexPath;
    state.descriptorTableOffset = static_cast<std::uint64_t>(output.pos());
    state.readable = true;
    state.writable = true;
    state.descriptors.assign(blockCount, {});

    for (const auto& descriptor : state.descriptors) {
        stream << quint64(descriptor.dataOffset)
               << quint64(descriptor.recordCount)
               << quint64(descriptor.dataBytes);
        if (stream.status() != QDataStream::Ok) {
            state = {};
            return false;
        }
    }

    return true;
}

bool readFastRecordsFromIndexFile(const QString& fastIndexPath,
                                  const TraceSession::FastIndexDescriptor& descriptor,
                                  std::vector<CLogBTraceFastRecordInfo>& records)
{
    return readFastRecordsFromIndexFile(fastIndexPath,
                                        descriptor,
                                        0,
                                        static_cast<std::size_t>(std::min<std::uint64_t>(
                                            descriptor.recordCount,
                                            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))),
                                        records);
}

bool readFastRecordsFromIndexFile(const QString& fastIndexPath,
                                  const TraceSession::FastIndexDescriptor& descriptor,
                                  const std::size_t localBegin,
                                  const std::size_t rowCount,
                                  std::vector<CLogBTraceFastRecordInfo>& records)
{
    records.clear();
    if (descriptor.dataOffset == 0 || descriptor.recordCount == 0 || descriptor.dataBytes == 0) {
        return false;
    }
    if (rowCount == 0) {
        return true;
    }

    std::uint64_t expectedDataBytes = 0;
    if (!checkedMultiply(descriptor.recordCount,
                         static_cast<std::uint64_t>(Detail::kFastRecordSerializedBytes),
                         expectedDataBytes)
        || descriptor.dataBytes != expectedDataBytes) {
        return false;
    }

    QFile input(fastIndexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    if (descriptor.recordCount > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())
        || localBegin > descriptor.recordCount
        || rowCount > descriptor.recordCount - localBegin
        || descriptor.dataOffset > fileSize
        || descriptor.dataBytes > fileSize
        || descriptor.dataBytes > fileSize - descriptor.dataOffset) {
        return false;
    }

    std::uint64_t localByteOffset = 0;
    std::uint64_t rangeBytes = 0;
    if (!checkedMultiply(static_cast<std::uint64_t>(localBegin),
                         static_cast<std::uint64_t>(Detail::kFastRecordSerializedBytes),
                         localByteOffset)
        || !checkedMultiply(static_cast<std::uint64_t>(rowCount),
                            static_cast<std::uint64_t>(Detail::kFastRecordSerializedBytes),
                            rangeBytes)
        || localByteOffset > descriptor.dataBytes
        || rangeBytes > descriptor.dataBytes - localByteOffset
        || localByteOffset > fileSize - descriptor.dataOffset) {
        return false;
    }

    const std::uint64_t byteOffset = descriptor.dataOffset + localByteOffset;
    if (rangeBytes > fileSize - byteOffset) {
        return false;
    }

    if (!input.seek(static_cast<qint64>(byteOffset))) {
        return false;
    }

    QDataStream stream(&input);
    stream.setByteOrder(QDataStream::LittleEndian);

    records.resize(rowCount);
    for (auto& record : records) {
        quint8 channel = 0;
        qint64 timestamp = 0;
        quint32 nodeId = 0;
        quint32 opcode = 0;
        quint32 sourceId = 0;
        quint32 targetId = 0;
        quint32 txnId = 0;
        quint32 dbid = 0;
        quint64 address = 0;
        stream >> channel >> timestamp >> nodeId >> opcode >> sourceId >> targetId >> txnId >> dbid >> address;
        if (stream.status() != QDataStream::Ok) {
            records.clear();
            return false;
        }
        record.channel = channel;
        record.timestamp = timestamp;
        record.nodeId = nodeId;
        record.opcode = opcode;
        record.sourceId = sourceId;
        record.targetId = targetId;
        record.txnId = txnId;
        record.dbid = dbid;
        record.address = address;
    }

    return true;
}

bool readOptionalFieldRecordsFromIndexFile(const QString& indexPath,
                                           const TraceSession::FastIndexDescriptor& descriptor,
                                           std::vector<OptionalFieldIndexRecord>& records)
{
    records.clear();
    if (descriptor.dataOffset == 0 || descriptor.recordCount == 0 || descriptor.dataBytes == 0) {
        return false;
    }

    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    if (descriptor.recordCount > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())
        || descriptor.dataOffset > fileSize
        || descriptor.dataBytes > fileSize
        || descriptor.dataOffset + descriptor.dataBytes > fileSize) {
        return false;
    }

    if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
        return false;
    }

    QDataStream stream(&input);
    stream.setByteOrder(QDataStream::LittleEndian);

    records.resize(static_cast<std::size_t>(descriptor.recordCount));
    for (auto& record : records) {
        quint8 present = 0;
        stream >> present;
        if (stream.status() != QDataStream::Ok) {
            records.clear();
            return false;
        }

        record.present = present != 0;
        if (record.present) {
            stream >> record.value >> record.raw;
            if (stream.status() != QDataStream::Ok) {
                records.clear();
                return false;
            }
        }
    }

    return true;
}

bool writeFastRecords(const QString& fastIndexPath,
                      const std::size_t blockIndex,
                      std::vector<TraceSession::FastIndexDescriptor>& descriptors,
                      const std::vector<CLogBTraceFastRecordInfo>& records)
{
    if (blockIndex >= descriptors.size()) {
        return false;
    }

    if (records.empty()) {
        return true;
    }

    QFile file(fastIndexPath);
    if (!file.open(QIODevice::ReadWrite)) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    if (!file.seek(file.size())) {
        return false;
    }
    const std::uint64_t dataOffset = static_cast<std::uint64_t>(file.pos());
    for (const auto& record : records) {
        stream << quint8(record.channel)
               << qint64(record.timestamp)
               << quint32(record.nodeId)
               << quint32(record.opcode)
               << quint32(record.sourceId)
               << quint32(record.targetId)
               << quint32(record.txnId)
               << quint32(record.dbid)
               << quint64(record.address);
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
    }

    TraceSession::FastIndexDescriptor descriptor;
    descriptor.dataOffset = dataOffset;
    descriptor.recordCount = static_cast<std::uint64_t>(records.size());
    descriptor.dataBytes = static_cast<std::uint64_t>(records.size() * Detail::kFastRecordSerializedBytes);
    descriptors[blockIndex] = descriptor;

    if (!file.seek(Detail::fastIndexDescriptorOffset(blockIndex))) {
        return false;
    }
    stream << quint64(descriptor.dataOffset)
           << quint64(descriptor.recordCount)
           << quint64(descriptor.dataBytes);
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    return true;
}

bool writeOptionalFieldRecords(const std::size_t blockIndex,
                               TraceSession::OptionalFieldIndexState& state,
                               const std::vector<OptionalFieldIndexRecord>& records,
                               std::stop_token stopToken = {})
{
    if (blockIndex >= state.descriptors.size()) {
        return false;
    }

    QFile file(state.path);
    if (!file.open(QIODevice::ReadWrite)) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    if (!file.seek(file.size())) {
        return false;
    }
    const std::uint64_t dataOffset = static_cast<std::uint64_t>(file.pos());
    for (const OptionalFieldIndexRecord& record : records) {
        if (stopToken.stop_requested()) {
            return false;
        }

        stream << quint8(record.present ? 1 : 0);
        if (record.present) {
            stream << record.value << record.raw;
        }
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
    }

    const std::uint64_t dataBytes = static_cast<std::uint64_t>(file.pos()) - dataOffset;
    TraceSession::FastIndexDescriptor descriptor;
    descriptor.dataOffset = dataOffset;
    descriptor.recordCount = static_cast<std::uint64_t>(records.size());
    descriptor.dataBytes = dataBytes;
    state.descriptors[blockIndex] = descriptor;

    const std::uint64_t descriptorOffset =
        state.descriptorTableOffset
        + static_cast<std::uint64_t>(blockIndex * Detail::kFastIndexDescriptorSerializedBytes);
    if (!file.seek(static_cast<qint64>(descriptorOffset))) {
        return false;
    }
    stream << quint64(descriptor.dataOffset)
           << quint64(descriptor.recordCount)
           << quint64(descriptor.dataBytes);
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    return true;
}

std::uint8_t xactionRowFlagsForRecord(const CLogBTraceXactionIndexRecord& record)
{
    std::uint8_t flags = 0;
    if (record.processed) {
        flags |= XactionRowProcessed;
    }
    if (record.indexed) {
        flags |= XactionRowIndexed;
    }
    if (record.processedByJoint) {
        flags |= XactionRowProcessedByJoint;
    }
    if (record.xactionComplete) {
        flags |= XactionRowComplete;
    }
    return flags;
}

CLogBTraceXactionIndexRecord xactionRecordFromFlags(const std::uint8_t flags,
                                                    const std::uint64_t ordinal)
{
    CLogBTraceXactionIndexRecord record;
    record.processed = (flags & XactionRowProcessed) != 0;
    record.indexed = (flags & XactionRowIndexed) != 0;
    record.processedByJoint = (flags & XactionRowProcessedByJoint) != 0;
    record.xactionComplete = record.indexed && (flags & XactionRowComplete) != 0;
    record.xactionOrdinal = record.indexed ? ordinal : 0;
    if (record.indexed) {
        record.transactionGroupKey =
            QStringLiteral("xaction|%1").arg(static_cast<qulonglong>(record.xactionOrdinal));
    }
    return record;
}

std::uint32_t readLe32(const char* cursor) noexcept
{
    const auto bytes = reinterpret_cast<const unsigned char*>(cursor);
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8U)
        | (static_cast<std::uint32_t>(bytes[2]) << 16U)
        | (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::uint64_t readLe64(const char* cursor) noexcept
{
    const auto bytes = reinterpret_cast<const unsigned char*>(cursor);
    return static_cast<std::uint64_t>(bytes[0])
        | (static_cast<std::uint64_t>(bytes[1]) << 8U)
        | (static_cast<std::uint64_t>(bytes[2]) << 16U)
        | (static_cast<std::uint64_t>(bytes[3]) << 24U)
        | (static_cast<std::uint64_t>(bytes[4]) << 32U)
        | (static_cast<std::uint64_t>(bytes[5]) << 40U)
        | (static_cast<std::uint64_t>(bytes[6]) << 48U)
        | (static_cast<std::uint64_t>(bytes[7]) << 56U);
}

bool writeXactionHeader(QDataStream& stream, const XactionIndexHeader& header)
{
    if (stream.writeRawData(header.magic.data(), static_cast<int>(header.magic.size()))
            != static_cast<int>(header.magic.size())) {
        return false;
    }
    stream << quint32(header.version)
           << quint64(header.sourceFileSize)
           << quint64(static_cast<std::uint64_t>(header.sourceLastModifiedMs))
           << quint64(header.totalRows)
           << quint64(header.rowTableOffset)
           << quint64(header.groupTableOffset)
           << quint64(header.groupCount)
           << quint64(header.debugDescriptorTableOffset)
           << quint64(header.debugChunkDescriptorTableOffset)
           << quint64(header.debugChunkCount)
           << quint64(header.stringTableOffset)
           << quint64(header.stringCount)
           << quint64(header.rowMapChunkCount);
    return stream.status() == QDataStream::Ok;
}

bool readXactionHeader(QDataStream& stream, XactionIndexHeader& header)
{
    header = {};
    if (stream.readRawData(header.magic.data(), static_cast<int>(header.magic.size()))
            != static_cast<int>(header.magic.size())) {
        return false;
    }

    quint32 version = 0;
    quint64 sourceFileSize = 0;
    quint64 sourceLastModifiedMs = 0;
    quint64 totalRows = 0;
    quint64 rowTableOffset = 0;
    quint64 groupTableOffset = 0;
    quint64 groupCount = 0;
    quint64 debugDescriptorTableOffset = 0;
    quint64 debugChunkDescriptorTableOffset = 0;
    quint64 debugChunkCount = 0;
    quint64 stringTableOffset = 0;
    quint64 stringCount = 0;
    quint64 rowMapChunkCount = 0;
    stream >> version
           >> sourceFileSize
           >> sourceLastModifiedMs
           >> totalRows
           >> rowTableOffset
           >> groupTableOffset
           >> groupCount
           >> debugDescriptorTableOffset
           >> debugChunkDescriptorTableOffset
           >> debugChunkCount
           >> stringTableOffset
           >> stringCount
           >> rowMapChunkCount;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    header.version = version;
    header.sourceFileSize = sourceFileSize;
    header.sourceLastModifiedMs = static_cast<std::int64_t>(sourceLastModifiedMs);
    header.totalRows = totalRows;
    header.rowTableOffset = rowTableOffset;
    header.groupTableOffset = groupTableOffset;
    header.groupCount = groupCount;
    header.debugDescriptorTableOffset = debugDescriptorTableOffset;
    header.debugChunkDescriptorTableOffset = debugChunkDescriptorTableOffset;
    header.debugChunkCount = debugChunkCount;
    header.stringTableOffset = stringTableOffset;
    header.stringCount = stringCount;
    header.rowMapChunkCount = rowMapChunkCount;
    return true;
}

bool validateXactionIndexHeader(const XactionIndexHeader& header,
                                const QString& tracePath,
                                const CLogBTraceMetadata& metadata,
                                const std::uint64_t fileSize)
{
    if (header.magic != kXactionIndexMagic
        || header.version != kXactionIndexVersion
        || header.sourceFileSize != traceSourceFileSize(tracePath)
        || header.sourceLastModifiedMs != traceSourceLastModifiedMs(tracePath)
        || header.totalRows != metadata.totalRecords
        || header.rowTableOffset < kXactionIndexHeaderSerializedBytes) {
        return false;
    }

    const std::uint64_t expectedRowMapChunkCount =
        metadata.totalRecords == 0
            ? 0
            : ((metadata.totalRecords + kXactionRowMapChunkRows - 1) / kXactionRowMapChunkRows);
    if (header.rowMapChunkCount != expectedRowMapChunkCount) {
        return false;
    }

    if (header.rowMapChunkCount != 0
        && kXactionRowMapChunkDescriptorSerializedBytes
            > (std::numeric_limits<std::uint64_t>::max)() / header.rowMapChunkCount) {
        return false;
    }
    const std::uint64_t rowTableBytes =
        header.rowMapChunkCount * kXactionRowMapChunkDescriptorSerializedBytes;
    if (header.rowTableOffset > fileSize
        || rowTableBytes > fileSize
        || header.rowTableOffset + rowTableBytes > fileSize) {
        return false;
    }

    if (header.groupCount != 0
        && kXactionGroupDescriptorSerializedBytes
            > (std::numeric_limits<std::uint64_t>::max)() / header.groupCount) {
        return false;
    }
    const std::uint64_t groupTableBytes =
        header.groupCount * kXactionGroupDescriptorSerializedBytes;
    if (header.groupTableOffset > fileSize
        || groupTableBytes > fileSize
        || header.groupTableOffset + groupTableBytes > fileSize) {
        return false;
    }

    if (header.debugDescriptorTableOffset != 0) {
        return false;
    }
    if (header.debugChunkCount != expectedRowMapChunkCount) {
        return false;
    }
    if (header.debugChunkCount != 0
        && kXactionRowMapChunkDescriptorSerializedBytes
            > (std::numeric_limits<std::uint64_t>::max)() / header.debugChunkCount) {
        return false;
    }
    const std::uint64_t debugChunkTableBytes =
        header.debugChunkCount * kXactionRowMapChunkDescriptorSerializedBytes;
    if (header.debugChunkDescriptorTableOffset > fileSize
        || debugChunkTableBytes > fileSize
        || header.debugChunkDescriptorTableOffset + debugChunkTableBytes > fileSize) {
        return false;
    }

    if (header.stringCount != 0
        && kXactionStringDescriptorSerializedBytes
            > (std::numeric_limits<std::uint64_t>::max)() / header.stringCount) {
        return false;
    }
    const std::uint64_t stringTableBytes =
        header.stringCount * kXactionStringDescriptorSerializedBytes;
    if (header.stringTableOffset > fileSize
        || stringTableBytes > fileSize
        || header.stringTableOffset + stringTableBytes > fileSize) {
        return false;
    }

    return true;
}

bool readXactionRowChunkDescriptor(QFile& file,
                                   const std::uint64_t descriptorTableOffset,
                                   const std::uint64_t chunkIndex,
                                   XactionRowChunkDescriptor& descriptor)
{
    const std::uint64_t offset =
        descriptorTableOffset + chunkIndex * kXactionRowChunkDescriptorSerializedBytes;
    if (!file.seek(static_cast<qint64>(offset))) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint64 dataOffset = 0;
    quint64 rowCount = 0;
    quint32 storedBytes = 0;
    quint32 uncompressedBytes = 0;
    quint8 flags = 0;
    stream >> dataOffset >> rowCount >> storedBytes >> uncompressedBytes >> flags;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    descriptor.dataOffset = dataOffset;
    descriptor.rowCount = rowCount;
    descriptor.storedBytes = storedBytes;
    descriptor.uncompressedBytes = uncompressedBytes;
    descriptor.flags = flags;
    return true;
}

bool readXactionGroupDescriptorTable(QFile& file,
                                     const std::uint64_t descriptorTableOffset,
                                     const std::uint64_t groupCount,
                                     QByteArray& bytes)
{
    bytes.clear();
    if (groupCount == 0) {
        return true;
    }
    if (groupCount > (std::numeric_limits<std::uint64_t>::max)()
            / kXactionGroupDescriptorSerializedBytes
        || !file.seek(static_cast<qint64>(descriptorTableOffset))) {
        return false;
    }
    const std::uint64_t tableBytes = groupCount * kXactionGroupDescriptorSerializedBytes;
    if (tableBytes > static_cast<std::uint64_t>((std::numeric_limits<qsizetype>::max)())) {
        return false;
    }
    bytes = file.read(static_cast<qint64>(tableBytes));
    return bytes.size() == static_cast<qsizetype>(tableBytes);
}

bool readXactionGroupDescriptorAt(QFile& file,
                                  const std::uint64_t descriptorTableOffset,
                                  const std::uint64_t groupIndex,
                                  TraceSession::XactionRowsDescriptorByOrdinal& descriptor)
{
    std::uint64_t offset = 0;
    std::uint64_t byteOffset = 0;
    if (!checkedMultiply(groupIndex, kXactionGroupDescriptorSerializedBytes, byteOffset)
        || descriptorTableOffset > (std::numeric_limits<std::uint64_t>::max)() - byteOffset) {
        return false;
    }
    offset = descriptorTableOffset + byteOffset;
    if (!file.seek(static_cast<qint64>(offset))) {
        return false;
    }

    char bytes[static_cast<std::size_t>(kXactionGroupDescriptorSerializedBytes)] = {};
    if (file.read(bytes, static_cast<qint64>(sizeof(bytes)))
        != static_cast<qint64>(sizeof(bytes))) {
        return false;
    }

    const char* cursor = bytes;
    descriptor.ordinal = readLe64(cursor);
    cursor += sizeof(std::uint64_t);
    descriptor.descriptor.chunkTableOffset = readLe64(cursor);
    cursor += sizeof(std::uint64_t);
    descriptor.descriptor.chunkCount = readLe64(cursor);
    cursor += sizeof(std::uint64_t);
    descriptor.descriptor.rowCount = readLe64(cursor);
    cursor += sizeof(std::uint64_t);
    descriptor.descriptor.chunkTableBytes = 0;
    return true;
}

bool validateXactionRowsDescriptor(const TraceSession::XactionRowsDescriptorByOrdinal& entry,
                                   const std::uint64_t fileSize)
{
    if (entry.ordinal == 0
        || entry.descriptor.chunkCount == 0
        || entry.descriptor.rowCount == 0) {
        return false;
    }

    std::uint64_t chunkTableBytes = 0;
    if (!checkedMultiply(entry.descriptor.chunkCount,
                         kXactionRowChunkDescriptorSerializedBytes,
                         chunkTableBytes)
        || entry.descriptor.chunkTableOffset > fileSize
        || chunkTableBytes > fileSize
        || entry.descriptor.chunkTableOffset + chunkTableBytes > fileSize) {
        return false;
    }

    return true;
}

TraceSession::XactionRowsDescriptor withChunkTableBytes(
    TraceSession::XactionRowsDescriptor descriptor)
{
    std::uint64_t chunkTableBytes = 0;
    if (checkedMultiply(descriptor.chunkCount,
                        kXactionRowChunkDescriptorSerializedBytes,
                        chunkTableBytes)) {
        descriptor.chunkTableBytes = chunkTableBytes;
    }
    return descriptor;
}

bool findXactionRowsDescriptorByOrdinalOnDisk(
    QFile& file,
    const std::uint64_t descriptorTableOffset,
    const std::uint64_t groupCount,
    const std::uint64_t ordinal,
    const std::uint64_t fileSize,
    TraceSession::XactionRowsDescriptor& descriptor)
{
    descriptor = {};
    if (ordinal == 0 || groupCount == 0) {
        return false;
    }

    std::uint64_t begin = 0;
    std::uint64_t end = groupCount;
    while (begin < end) {
        const std::uint64_t mid = begin + (end - begin) / 2;
        TraceSession::XactionRowsDescriptorByOrdinal candidate;
        if (!readXactionGroupDescriptorAt(file, descriptorTableOffset, mid, candidate)
            || !validateXactionRowsDescriptor(candidate, fileSize)) {
            return false;
        }

        if (candidate.ordinal < ordinal) {
            begin = mid + 1;
        } else {
            end = mid;
        }
    }

    if (begin >= groupCount) {
        return false;
    }

    TraceSession::XactionRowsDescriptorByOrdinal candidate;
    if (!readXactionGroupDescriptorAt(file, descriptorTableOffset, begin, candidate)
        || !validateXactionRowsDescriptor(candidate, fileSize)
        || candidate.ordinal != ordinal) {
        return false;
    }
    descriptor = withChunkTableBytes(candidate.descriptor);
    return true;
}

bool readXactionRowMapChunkDescriptorTable(
    QFile& file,
    const std::uint64_t descriptorTableOffset,
    const std::uint64_t chunkCount,
    std::vector<TraceSession::XactionRowMapChunkDescriptor>& descriptors)
{
    descriptors.clear();
    if (chunkCount == 0) {
        return true;
    }
    if (chunkCount > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())
        || !file.seek(static_cast<qint64>(descriptorTableOffset))) {
        return false;
    }
    const std::uint64_t tableBytes = chunkCount * kXactionRowMapChunkDescriptorSerializedBytes;
    if (tableBytes > static_cast<std::uint64_t>((std::numeric_limits<qsizetype>::max)())) {
        return false;
    }
    const QByteArray bytes = file.read(static_cast<qint64>(tableBytes));
    if (bytes.size() != static_cast<qsizetype>(tableBytes)) {
        return false;
    }

    descriptors.resize(static_cast<std::size_t>(chunkCount));
    const char* cursor = bytes.constData();
    for (std::uint64_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
        auto& descriptor = descriptors[static_cast<std::size_t>(chunkIndex)];
        descriptor.dataOffset = readLe64(cursor);
        cursor += sizeof(std::uint64_t);
        descriptor.storedBytes = readLe32(cursor);
        cursor += sizeof(std::uint32_t);
        descriptor.uncompressedBytes = readLe32(cursor);
        cursor += sizeof(std::uint32_t);
        descriptor.rowCount = readLe32(cursor);
        cursor += sizeof(std::uint32_t);
        descriptor.flags = static_cast<std::uint8_t>(*cursor++);
    }
    return true;
}

bool readXactionDebugChunkDescriptorTable(
    QFile& file,
    const std::uint64_t descriptorTableOffset,
    const std::uint64_t chunkCount,
    std::vector<TraceSession::XactionDebugChunkDescriptor>& descriptors)
{
    descriptors.clear();
    if (chunkCount == 0) {
        return true;
    }
    if (chunkCount > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())
        || !file.seek(static_cast<qint64>(descriptorTableOffset))) {
        return false;
    }
    const std::uint64_t tableBytes = chunkCount * kXactionRowMapChunkDescriptorSerializedBytes;
    if (tableBytes > static_cast<std::uint64_t>((std::numeric_limits<qsizetype>::max)())) {
        return false;
    }
    const QByteArray bytes = file.read(static_cast<qint64>(tableBytes));
    if (bytes.size() != static_cast<qsizetype>(tableBytes)) {
        return false;
    }

    descriptors.resize(static_cast<std::size_t>(chunkCount));
    const char* cursor = bytes.constData();
    for (std::uint64_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
        auto& descriptor = descriptors[static_cast<std::size_t>(chunkIndex)];
        descriptor.dataOffset = readLe64(cursor);
        cursor += sizeof(std::uint64_t);
        descriptor.storedBytes = readLe32(cursor);
        cursor += sizeof(std::uint32_t);
        descriptor.uncompressedBytes = readLe32(cursor);
        cursor += sizeof(std::uint32_t);
        descriptor.rowCount = readLe32(cursor);
        cursor += sizeof(std::uint32_t);
        descriptor.flags = static_cast<std::uint8_t>(*cursor++);
    }
    return true;
}

bool readXactionStringDescriptorTable(QFile& file,
                                      const std::uint64_t descriptorTableOffset,
                                      const std::uint64_t stringCount,
                                      QByteArray& bytes)
{
    bytes.clear();
    if (stringCount == 0) {
        return true;
    }
    if (stringCount > (std::numeric_limits<std::uint64_t>::max)()
            / kXactionStringDescriptorSerializedBytes
        || !file.seek(static_cast<qint64>(descriptorTableOffset))) {
        return false;
    }
    const std::uint64_t tableBytes = stringCount * kXactionStringDescriptorSerializedBytes;
    if (tableBytes > static_cast<std::uint64_t>((std::numeric_limits<qsizetype>::max)())) {
        return false;
    }
    bytes = file.read(static_cast<qint64>(tableBytes));
    return bytes.size() == static_cast<qsizetype>(tableBytes);
}

std::vector<TraceSession::XactionRowsDescriptorByOrdinal>::const_iterator findXactionRowsDescriptorByOrdinal(
    const std::vector<TraceSession::XactionRowsDescriptorByOrdinal>& descriptors,
    const std::uint64_t ordinal)
{
    return std::lower_bound(descriptors.cbegin(),
                            descriptors.cend(),
                            ordinal,
                            [](const TraceSession::XactionRowsDescriptorByOrdinal& entry,
                               const std::uint64_t value) {
                                return entry.ordinal < value;
                            });
}

bool readAndValidateXactionGroupDescriptorTable(
    QFile& file,
    const std::uint64_t descriptorTableOffset,
    const std::uint64_t groupCount,
    const std::uint64_t fileSize,
    std::vector<TraceSession::XactionRowsDescriptorByOrdinal>& descriptors,
    std::stop_token stopToken = {},
    const std::function<void(std::uint64_t completed)>& progressCallback = {})
{
    descriptors.clear();
    descriptors.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
        groupCount,
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))));

    QByteArray groupDescriptorTable;
    if (!readXactionGroupDescriptorTable(file,
                                         descriptorTableOffset,
                                         groupCount,
                                         groupDescriptorTable)) {
        return false;
    }

    constexpr std::uint64_t kValidationProgressBatch = 512;
    const char* groupDescriptorCursor = groupDescriptorTable.constData();
    for (std::uint64_t index = 0; index < groupCount; ++index) {
        if (stopToken.stop_requested()) {
            return false;
        }

        TraceSession::XactionRowsDescriptorByOrdinal entry;
        entry.ordinal = readLe64(groupDescriptorCursor);
        groupDescriptorCursor += sizeof(std::uint64_t);
        entry.descriptor.chunkTableOffset = readLe64(groupDescriptorCursor);
        groupDescriptorCursor += sizeof(std::uint64_t);
        entry.descriptor.chunkCount = readLe64(groupDescriptorCursor);
        groupDescriptorCursor += sizeof(std::uint64_t);
        entry.descriptor.rowCount = readLe64(groupDescriptorCursor);
        groupDescriptorCursor += sizeof(std::uint64_t);

        if (!validateXactionRowsDescriptor(entry, fileSize)
            || (!descriptors.empty() && entry.ordinal <= descriptors.back().ordinal)) {
            descriptors.clear();
            return false;
        }

        entry.descriptor = withChunkTableBytes(entry.descriptor);
        descriptors.push_back(entry);
        if (progressCallback
            && (((index + 1) % kValidationProgressBatch) == 0 || index + 1 == groupCount)) {
            progressCallback(index + 1);
        }
    }
    return true;
}

bool readXactionStringDescriptor(QFile& file,
                                 const std::uint64_t descriptorTableOffset,
                                 const std::uint64_t stringIndex,
                                 XactionStringDescriptor& descriptor)
{
    const std::uint64_t offset =
        descriptorTableOffset + stringIndex * kXactionStringDescriptorSerializedBytes;
    if (!file.seek(static_cast<qint64>(offset))) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint64 dataOffset = 0;
    quint32 bytes = 0;
    stream >> dataOffset >> bytes;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    descriptor.dataOffset = dataOffset;
    descriptor.bytes = bytes;
    return true;
}

bool writeZeroBytes(QFile& file,
                    std::uint64_t bytes,
                    const std::function<bool(std::uint64_t writtenBytes)>& progressCallback = {})
{
    const QByteArray zeros(1 << 20, '\0');
    std::uint64_t writtenBytes = 0;
    while (bytes > 0) {
        const qint64 chunk = static_cast<qint64>(
            std::min<std::uint64_t>(bytes, static_cast<std::uint64_t>(zeros.size())));
        if (file.write(zeros.constData(), chunk) != chunk) {
            return false;
        }
        bytes -= static_cast<std::uint64_t>(chunk);
        writtenBytes += static_cast<std::uint64_t>(chunk);
        if (progressCallback && !progressCallback(writtenBytes)) {
            return false;
        }
    }
    return true;
}

bool checkedMultiply(const std::uint64_t lhs,
                     const std::uint64_t rhs,
                     std::uint64_t& result)
{
    if (lhs != 0 && rhs > (std::numeric_limits<std::uint64_t>::max)() / lhs) {
        return false;
    }
    result = lhs * rhs;
    return true;
}

class XactionIndexFileWriter final {
public:
    XactionIndexFileWriter(QString indexPath,
                           QString tracePath,
                           const CLogBTraceMetadata& metadata)
        : indexPath_(std::move(indexPath))
        , tempPath_(indexPath_ + QStringLiteral(".tmp"))
        , tracePath_(std::move(tracePath))
        , metadata_(metadata)
    {
    }

    bool open()
    {
        if (indexPath_.isEmpty()) {
            return false;
        }

        const std::uint64_t rowMapChunkCount =
            metadata_.totalRecords == 0
                ? 0
                : ((metadata_.totalRecords + kXactionRowMapChunkRows - 1) / kXactionRowMapChunkRows);
        std::uint64_t rowMapDescriptorBytes = 0;
        if (!checkedMultiply(rowMapChunkCount,
                             kXactionRowMapChunkDescriptorSerializedBytes,
                             rowMapDescriptorBytes)) {
            return false;
        }

        QFile::remove(tempPath_);
        file_.setFileName(tempPath_);
        if (!file_.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
            return false;
        }

        header_ = {};
        header_.sourceFileSize = traceSourceFileSize(tracePath_);
        header_.sourceLastModifiedMs = traceSourceLastModifiedMs(tracePath_);
        header_.totalRows = metadata_.totalRecords;
        header_.rowTableOffset = kXactionIndexHeaderSerializedBytes;
        header_.rowMapChunkCount = rowMapChunkCount;

        {
            QDataStream stream(&file_);
            stream.setByteOrder(QDataStream::LittleEndian);
            if (!writeXactionHeader(stream, header_)) {
                return false;
            }
        }
        if (static_cast<std::uint64_t>(file_.pos()) != kXactionIndexHeaderSerializedBytes) {
            return false;
        }
        if (!writeZeroBytes(file_, rowMapDescriptorBytes)) {
            return false;
        }

        rowMapDescriptors_.clear();
        rowMapDescriptors_.resize(static_cast<std::size_t>(rowMapChunkCount));
        debugChunkDescriptors_.clear();
        debugChunkDescriptors_.resize(static_cast<std::size_t>(rowMapChunkCount));
        currentChunkRecords_.clear();
        pendingChunkRecords_.clear();
        currentChunkIndex_ = 0;
        nextExpectedRow_ = 0;
        flushedRowMapChunkCount_ = 0;
        rowGroupsByOrdinal_.clear();
        strings_.clear();
        stringIds_.clear();
        lastError_.clear();
        return true;
    }

    bool writeRecord(const std::uint64_t logicalRow,
                     CLogBTraceXactionIndexRecord record)
    {
        lastError_.clear();
        if (!file_.isOpen()
            || logicalRow >= metadata_.totalRecords
            || logicalRow != nextExpectedRow_) {
            setError(QStringLiteral("Xaction index writer rejected row %1 of %2.")
                         .arg(static_cast<qulonglong>(logicalRow + 1))
                         .arg(static_cast<qulonglong>(metadata_.totalRecords)));
            return false;
        }

        if (logicalRow / kXactionRowMapChunkRows != currentChunkIndex_) {
            setError(QStringLiteral("Xaction index writer received row %1 outside the active chunk.")
                         .arg(static_cast<qulonglong>(logicalRow + 1)));
            return false;
        }

        currentChunkRecords_.push_back(std::move(record));
        ++nextExpectedRow_;
        if (currentChunkRecords_.size() == kXactionRowMapChunkRows) {
            pendingChunkRecords_.push_back(std::move(currentChunkRecords_));
            currentChunkRecords_.clear();
            ++currentChunkIndex_;
        }
        return true;
    }

    QString errorString() const
    {
        return lastError_;
    }

    bool finalize(std::stop_token stopToken,
                  const CLogBTraceLoadStageCallback& stageCallback,
                  const CLogBTraceLoadStageProgressCallback& progressCallback)
    {
        if (!prepareAndWriteChunks(stopToken, stageCallback, progressCallback)
            || stopToken.stop_requested()) {
            discard();
            return false;
        }

        if (!file_.seek(0)) {
            discard();
            return false;
        }
        {
            QDataStream stream(&file_);
            stream.setByteOrder(QDataStream::LittleEndian);
            if (!writeXactionHeader(stream, header_)) {
                discard();
                return false;
            }
        }

        file_.close();
        QFile::remove(indexPath_);
        if (!QFile::rename(tempPath_, indexPath_)) {
            QFile::remove(tempPath_);
            return false;
        }
        return true;
    }

    void discard()
    {
        file_.close();
        QFile::remove(tempPath_);
    }

private:
    struct GroupWriteState {
        std::vector<XactionRowChunkDescriptor> chunks;
        std::uint64_t rowCount = 0;
    };

    struct PreparedRowListChunk {
        std::uint64_t ordinal = 0;
        XactionPreparedPayload payload;
        XactionRowChunkDescriptor descriptor;
    };

    struct PreparedRowMapChunk {
        std::uint64_t chunkIndex = 0;
        TraceSession::XactionRowMapChunkDescriptor rowMapDescriptor;
        TraceSession::XactionDebugChunkDescriptor debugDescriptor;
        XactionPreparedPayload rowMapPayload;
        XactionPreparedPayload debugPayload;
        std::vector<PreparedRowListChunk> rowListChunks;
        QString error;
    };

    void setError(QString error)
    {
        lastError_ = std::move(error);
    }

    std::uint32_t internString(const QString& value)
    {
        if (value.isEmpty()) {
            return 0;
        }

        const std::lock_guard lock(stringsMutex_);
        const auto found = stringIds_.constFind(value);
        if (found != stringIds_.cend()) {
            return found.value();
        }

        if (strings_.size() >= (std::numeric_limits<std::uint32_t>::max)()) {
            return 0;
        }
        const std::uint32_t id = static_cast<std::uint32_t>(strings_.size() + 1);
        strings_.push_back(value);
        stringIds_.insert(value, id);
        return id;
    }

    std::uint32_t internStringCached(const QString& value,
                                     QHash<QString, std::uint32_t>& localStringIds)
    {
        if (value.isEmpty()) {
            return 0;
        }

        const auto localFound = localStringIds.constFind(value);
        if (localFound != localStringIds.cend()) {
            return localFound.value();
        }

        const std::uint32_t id = internString(value);
        if (id != 0) {
            localStringIds.insert(value, id);
        }
        return id;
    }

    TraceSession::XactionRowDebugIds internDebugIds(
        const CLogBTraceXactionIndexRecord& record,
        QHash<QString, std::uint32_t>& localStringIds)
    {
        return TraceSession::XactionRowDebugIds{
            .jointType = internStringCached(record.jointType, localStringIds),
            .jointPath = internStringCached(record.jointPath, localStringIds),
            .denialName = internStringCached(record.denialName, localStringIds),
            .denialCode = internStringCached(record.denialCode, localStringIds),
            .xactionType = internStringCached(record.xactionType, localStringIds),
        };
    }

    static void writeLe32(char*& cursor, const std::uint32_t value) noexcept
    {
        *cursor++ = static_cast<char>(value & 0xffU);
        *cursor++ = static_cast<char>((value >> 8U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 16U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 24U) & 0xffU);
    }

    static void writeLe64(char*& cursor, const std::uint64_t value) noexcept
    {
        *cursor++ = static_cast<char>(value & 0xffU);
        *cursor++ = static_cast<char>((value >> 8U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 16U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 24U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 32U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 40U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 48U) & 0xffU);
        *cursor++ = static_cast<char>((value >> 56U) & 0xffU);
    }

    static bool serializeRowMapChunk(const std::vector<XactionPreparedRowMapEntry>& entries,
                                     QByteArray& payload)
    {
        payload.clear();
        if (entries.size() > (std::numeric_limits<std::uint32_t>::max)()) {
            return false;
        }
        const qsizetype entryBytes = sizeof(std::uint32_t)
            + sizeof(std::uint8_t)
            + sizeof(std::uint64_t);
        const qsizetype totalBytes = sizeof(std::uint32_t)
            + static_cast<qsizetype>(entries.size()) * entryBytes;
        if (totalBytes < 0) {
            return false;
        }
        payload.resize(totalBytes);
        char* cursor = payload.data();
        writeLe32(cursor, static_cast<std::uint32_t>(entries.size()));
        for (const XactionPreparedRowMapEntry& entry : entries) {
            writeLe32(cursor, entry.localRow);
            *cursor++ = static_cast<char>(entry.flags);
            writeLe64(cursor, entry.ordinal);
        }
        return true;
    }

    static bool serializeDebugChunk(const std::vector<XactionPreparedDebugEntry>& entries,
                                    QByteArray& payload)
    {
        payload.clear();
        if (entries.size() > (std::numeric_limits<std::uint32_t>::max)()) {
            return false;
        }
        const qsizetype entryBytes = sizeof(std::uint32_t) * 6;
        const qsizetype totalBytes = sizeof(std::uint32_t)
            + static_cast<qsizetype>(entries.size()) * entryBytes;
        if (totalBytes < 0) {
            return false;
        }
        payload.resize(totalBytes);
        char* cursor = payload.data();
        writeLe32(cursor, static_cast<std::uint32_t>(entries.size()));
        for (const XactionPreparedDebugEntry& entry : entries) {
            writeLe32(cursor, entry.localRow);
            writeLe32(cursor, entry.debugIds.jointType);
            writeLe32(cursor, entry.debugIds.jointPath);
            writeLe32(cursor, entry.debugIds.denialName);
            writeLe32(cursor, entry.debugIds.denialCode);
            writeLe32(cursor, entry.debugIds.xactionType);
        }
        return true;
    }

    static bool serializeRows(const std::vector<std::uint64_t>& rows,
                              QByteArray& payload)
    {
        payload.clear();
        const qsizetype totalBytes =
            static_cast<qsizetype>(rows.size()) * static_cast<qsizetype>(sizeof(std::uint64_t));
        if (totalBytes < 0) {
            return false;
        }
        payload.resize(totalBytes);
        char* cursor = payload.data();
        for (const std::uint64_t row : rows) {
            writeLe64(cursor, row);
        }
        return true;
    }

    static XactionPreparedPayload preparePayload(const QByteArray& payload)
    {
        XactionPreparedPayload prepared;
        prepared.uncompressedBytes = static_cast<std::uint32_t>(payload.size());
        if (payload.size() >= kXactionMinCompressBytes) {
            const QByteArray compressed = qCompress(payload, 1);
            const qsizetype requiredSavings =
                payload.size() * kXactionCompressionMinSavingsPercent / 100;
            if (!compressed.isEmpty()
                && compressed.size() + requiredSavings < payload.size()) {
                prepared.stored = compressed;
                prepared.flags = kXactionRowChunkCompressed;
                return prepared;
            }
        }
        prepared.stored = payload;
        return prepared;
    }

    bool appendPreparedPayload(const XactionPreparedPayload& payload,
                               std::uint64_t& dataOffset)
    {
        dataOffset = static_cast<std::uint64_t>(file_.size());
        if (payload.stored.isEmpty()) {
            return true;
        }
        if (!file_.seek(static_cast<qint64>(dataOffset))) {
            setError(QStringLiteral("Could not seek to the end of the Xaction index file."));
            return false;
        }
        if (file_.write(payload.stored.constData(), payload.stored.size())
            != payload.stored.size()) {
            setError(QStringLiteral("Could not write Xaction index chunk data."));
            return false;
        }
        return true;
    }

    bool prepareRowMapChunk(const std::uint64_t chunkIndex,
                            const std::vector<CLogBTraceXactionIndexRecord>& records,
                            PreparedRowMapChunk& prepared,
                            std::stop_token stopToken)
    {
        prepared = {};
        prepared.chunkIndex = chunkIndex;
        if (stopToken.stop_requested()) {
            prepared.error = QStringLiteral("Xaction index chunk writing was cancelled.");
            return false;
        }
        if (chunkIndex >= rowMapDescriptors_.size()
            || chunkIndex >= debugChunkDescriptors_.size()) {
            prepared.error = QStringLiteral("Xaction index writer exceeded the expected chunk count.");
            return false;
        }

        const std::uint64_t chunkStartRow = chunkIndex * kXactionRowMapChunkRows;
        const std::uint32_t rowCount = static_cast<std::uint32_t>(records.size());
        std::vector<XactionPreparedRowMapEntry> rowMapEntries;
        std::vector<XactionPreparedDebugEntry> debugEntries;
        std::map<std::uint64_t, std::vector<std::uint64_t>> rowsByOrdinal;
        QHash<QString, std::uint32_t> localStringIds;
        rowMapEntries.reserve(records.size());

        for (std::size_t localRow = 0; localRow < records.size(); ++localRow) {
            if (stopToken.stop_requested()) {
                prepared.error = QStringLiteral("Xaction index chunk writing was cancelled.");
                return false;
            }

            const CLogBTraceXactionIndexRecord& record = records[localRow];
            const std::uint8_t flags = xactionRowFlagsForRecord(record);
            if (flags != 0) {
                rowMapEntries.push_back(XactionPreparedRowMapEntry{
                    .localRow = static_cast<std::uint32_t>(localRow),
                    .flags = flags,
                    .ordinal = record.indexed ? record.xactionOrdinal : 0,
                });
            }

            const bool hasDebug = !record.jointType.isEmpty()
                || !record.jointPath.isEmpty()
                || !record.denialName.isEmpty()
                || !record.denialCode.isEmpty()
                || !record.xactionType.isEmpty();
            if (hasDebug) {
                debugEntries.push_back(XactionPreparedDebugEntry{
                    .localRow = static_cast<std::uint32_t>(localRow),
                    .debugIds = internDebugIds(record, localStringIds),
                });
            }

            if (record.indexed && record.xactionOrdinal != 0) {
                rowsByOrdinal[record.xactionOrdinal].push_back(chunkStartRow + localRow);
            }
        }

        QByteArray uncompressedRowMap;
        if (!serializeRowMapChunk(rowMapEntries, uncompressedRowMap)) {
            prepared.error = QStringLiteral("Could not serialize an Xaction row-map chunk.");
            return false;
        }
        prepared.rowMapPayload = preparePayload(uncompressedRowMap);
        prepared.rowMapDescriptor.rowCount = rowCount;
        prepared.rowMapDescriptor.uncompressedBytes = prepared.rowMapPayload.uncompressedBytes;
        prepared.rowMapDescriptor.flags = prepared.rowMapPayload.flags;
        prepared.rowMapDescriptor.storedBytes =
            static_cast<std::uint32_t>(prepared.rowMapPayload.stored.size());

        prepared.debugDescriptor.rowCount = rowCount;
        if (!debugEntries.empty()) {
            QByteArray uncompressedDebug;
            if (!serializeDebugChunk(debugEntries, uncompressedDebug)) {
                prepared.error = QStringLiteral("Could not serialize an Xaction debug chunk.");
                return false;
            }
            prepared.debugPayload = preparePayload(uncompressedDebug);
            prepared.debugDescriptor.uncompressedBytes = prepared.debugPayload.uncompressedBytes;
            prepared.debugDescriptor.flags = prepared.debugPayload.flags;
            prepared.debugDescriptor.storedBytes =
                static_cast<std::uint32_t>(prepared.debugPayload.stored.size());
        }

        prepared.rowListChunks.reserve(rowsByOrdinal.size());
        for (const auto& [ordinal, rows] : rowsByOrdinal) {
            QByteArray uncompressedRows;
            if (!serializeRows(rows, uncompressedRows)) {
                prepared.error = QStringLiteral("Could not serialize an Xaction row-list chunk.");
                return false;
            }
            PreparedRowListChunk rowListChunk;
            rowListChunk.ordinal = ordinal;
            rowListChunk.payload = preparePayload(uncompressedRows);
            rowListChunk.descriptor.rowCount = static_cast<std::uint64_t>(rows.size());
            rowListChunk.descriptor.uncompressedBytes = rowListChunk.payload.uncompressedBytes;
            rowListChunk.descriptor.flags = rowListChunk.payload.flags;
            rowListChunk.descriptor.storedBytes =
                static_cast<std::uint32_t>(rowListChunk.payload.stored.size());
            prepared.rowListChunks.push_back(std::move(rowListChunk));
        }

        return true;
    }

    bool appendPreparedRowMapChunk(PreparedRowMapChunk& prepared)
    {
        if (prepared.chunkIndex >= rowMapDescriptors_.size()
            || prepared.chunkIndex >= debugChunkDescriptors_.size()) {
            setError(QStringLiteral("Xaction index writer exceeded the expected chunk count."));
            return false;
        }

        if (!appendPreparedPayload(prepared.rowMapPayload,
                                   prepared.rowMapDescriptor.dataOffset)) {
            return false;
        }
        rowMapDescriptors_[static_cast<std::size_t>(prepared.chunkIndex)] =
            prepared.rowMapDescriptor;

        if (prepared.debugDescriptor.storedBytes != 0
            && !appendPreparedPayload(prepared.debugPayload,
                                      prepared.debugDescriptor.dataOffset)) {
            return false;
        }
        debugChunkDescriptors_[static_cast<std::size_t>(prepared.chunkIndex)] =
            prepared.debugDescriptor;

        for (PreparedRowListChunk& rowListChunk : prepared.rowListChunks) {
            if (!appendPreparedPayload(rowListChunk.payload,
                                       rowListChunk.descriptor.dataOffset)) {
                return false;
            }

            GroupWriteState& group = rowGroupsByOrdinal_[rowListChunk.ordinal];
            group.chunks.push_back(rowListChunk.descriptor);
            group.rowCount += rowListChunk.descriptor.rowCount;
        }

        flushedRowMapChunkCount_ = std::max(flushedRowMapChunkCount_,
                                            prepared.chunkIndex + 1);
        return true;
    }

    bool flushCurrentRowMapChunk(std::stop_token stopToken)
    {
        if (currentChunkRecords_.empty()) {
            return true;
        }

        PreparedRowMapChunk prepared;
        if (!prepareRowMapChunk(currentChunkIndex_,
                                currentChunkRecords_,
                                prepared,
                                stopToken)) {
            setError(std::move(prepared.error));
            return false;
        }
        if (!appendPreparedRowMapChunk(prepared)) {
            return false;
        }
        currentChunkRecords_.clear();
        ++currentChunkIndex_;
        return true;
    }

    bool writeStringTable(const std::function<bool(std::uint64_t itemCount)>& progressCallback)
    {
        header_.stringCount = static_cast<std::uint64_t>(strings_.size());
        header_.stringTableOffset = static_cast<std::uint64_t>(file_.size());
        std::uint64_t descriptorBytes = 0;
        if (!checkedMultiply(header_.stringCount,
                             kXactionStringDescriptorSerializedBytes,
                             descriptorBytes)) {
            return false;
        }

        if (!file_.seek(static_cast<qint64>(header_.stringTableOffset + descriptorBytes))) {
            return false;
        }

        std::vector<XactionStringDescriptor> descriptors;
        descriptors.reserve(strings_.size());
        for (const QString& string : strings_) {
            const QByteArray bytes = string.toUtf8();
            if (bytes.size() < 0
                || static_cast<std::uint64_t>(bytes.size())
                    > (std::numeric_limits<std::uint32_t>::max)()) {
                setError(QStringLiteral("An Xaction debug string is too large."));
                return false;
            }

            XactionStringDescriptor descriptor;
            descriptor.dataOffset = static_cast<std::uint64_t>(file_.pos());
            descriptor.bytes = static_cast<std::uint32_t>(bytes.size());
            if (!bytes.isEmpty()
                && file_.write(bytes.constData(), bytes.size()) != bytes.size()) {
                setError(QStringLiteral("Could not write an Xaction debug string."));
                return false;
            }
            descriptors.push_back(descriptor);
            if (progressCallback && !progressCallback(1)) {
                return false;
            }
        }

        if (!file_.seek(static_cast<qint64>(header_.stringTableOffset))) {
            return false;
        }
        QDataStream stream(&file_);
        stream.setByteOrder(QDataStream::LittleEndian);
        for (const XactionStringDescriptor& descriptor : descriptors) {
            stream << quint64(descriptor.dataOffset)
                   << quint32(descriptor.bytes);
            if (stream.status() != QDataStream::Ok) {
                setError(QStringLiteral("Could not serialize the Xaction string table."));
                return false;
            }
        }
        return true;
    }

    bool writeGroupDirectory(const std::function<bool(std::uint64_t itemCount)>& progressCallback)
    {
        std::vector<std::uint64_t> ordinals;
        ordinals.reserve(static_cast<std::size_t>(rowGroupsByOrdinal_.size()));
        for (auto it = rowGroupsByOrdinal_.cbegin(); it != rowGroupsByOrdinal_.cend(); ++it) {
            ordinals.push_back(it.key());
        }
        std::sort(ordinals.begin(), ordinals.end());

        header_.groupCount = static_cast<std::uint64_t>(ordinals.size());
        header_.groupTableOffset = static_cast<std::uint64_t>(file_.size());

        std::uint64_t totalChunkCount = 0;
        for (const std::uint64_t ordinal : ordinals) {
            const auto found = rowGroupsByOrdinal_.constFind(ordinal);
            if (found == rowGroupsByOrdinal_.cend()) {
                return false;
            }
            totalChunkCount += static_cast<std::uint64_t>(found.value().chunks.size());
        }

        std::uint64_t groupTableBytes = 0;
        std::uint64_t chunkDescriptorBytes = 0;
        if (!checkedMultiply(header_.groupCount,
                             kXactionGroupDescriptorSerializedBytes,
                             groupTableBytes)
            || !checkedMultiply(totalChunkCount,
                                kXactionRowChunkDescriptorSerializedBytes,
                                chunkDescriptorBytes)) {
            return false;
        }

        if (!file_.seek(static_cast<qint64>(header_.groupTableOffset))) {
            return false;
        }

        std::uint64_t chunkTableOffset = header_.groupTableOffset + groupTableBytes;
        QDataStream stream(&file_);
        stream.setByteOrder(QDataStream::LittleEndian);
        for (const std::uint64_t ordinal : ordinals) {
            const GroupWriteState& group = rowGroupsByOrdinal_.value(ordinal);
            stream << quint64(ordinal)
                   << quint64(chunkTableOffset)
                   << quint64(group.chunks.size())
                   << quint64(group.rowCount);
            if (stream.status() != QDataStream::Ok) {
                setError(QStringLiteral("Could not serialize an Xaction row-list descriptor."));
                return false;
            }
            chunkTableOffset +=
                static_cast<std::uint64_t>(group.chunks.size())
                * kXactionRowChunkDescriptorSerializedBytes;
            if (progressCallback && !progressCallback(1)) {
                return false;
            }
        }
        for (const std::uint64_t ordinal : ordinals) {
            const GroupWriteState& group = rowGroupsByOrdinal_.value(ordinal);
            for (const XactionRowChunkDescriptor& chunk : group.chunks) {
                stream << quint64(chunk.dataOffset)
                       << quint64(chunk.rowCount)
                       << quint32(chunk.storedBytes)
                       << quint32(chunk.uncompressedBytes)
                       << quint8(chunk.flags);
                if (stream.status() != QDataStream::Ok) {
                    setError(QStringLiteral("Could not serialize an Xaction row-list chunk descriptor."));
                    return false;
                }
                if (progressCallback && !progressCallback(1)) {
                    return false;
                }
            }
        }

        return chunkTableOffset == header_.groupTableOffset + groupTableBytes + chunkDescriptorBytes;
    }

    bool writeRowMapDescriptorTable(
        const std::vector<TraceSession::XactionRowMapChunkDescriptor>& descriptors,
        const std::function<bool(std::uint64_t itemCount)>& progressCallback)
    {
        if (descriptors.size() != header_.rowMapChunkCount) {
            return false;
        }

        if (!file_.seek(static_cast<qint64>(header_.rowTableOffset))) {
            return false;
        }

        QDataStream stream(&file_);
        stream.setByteOrder(QDataStream::LittleEndian);
        for (const TraceSession::XactionRowMapChunkDescriptor& descriptor : descriptors) {
            stream << quint64(descriptor.dataOffset)
                   << quint32(descriptor.storedBytes)
                   << quint32(descriptor.uncompressedBytes)
                   << quint32(descriptor.rowCount)
                   << quint8(descriptor.flags);
            if (stream.status() != QDataStream::Ok) {
                setError(QStringLiteral("Could not serialize an Xaction row-map chunk descriptor."));
                return false;
            }
            if (progressCallback && !progressCallback(1)) {
                return false;
            }
        }
        return true;
    }

    bool writeDebugChunkDescriptorTable(
        const std::vector<TraceSession::XactionDebugChunkDescriptor>& descriptors,
        const std::function<bool(std::uint64_t itemCount)>& progressCallback)
    {
        if (descriptors.size() != header_.rowMapChunkCount) {
            return false;
        }

        header_.debugChunkCount = static_cast<std::uint64_t>(descriptors.size());
        header_.debugChunkDescriptorTableOffset = static_cast<std::uint64_t>(file_.size());
        if (!file_.seek(static_cast<qint64>(header_.debugChunkDescriptorTableOffset))) {
            return false;
        }

        QDataStream stream(&file_);
        stream.setByteOrder(QDataStream::LittleEndian);
        for (const TraceSession::XactionDebugChunkDescriptor& descriptor : descriptors) {
            stream << quint64(descriptor.dataOffset)
                   << quint32(descriptor.storedBytes)
                   << quint32(descriptor.uncompressedBytes)
                   << quint32(descriptor.rowCount)
                   << quint8(descriptor.flags);
            if (stream.status() != QDataStream::Ok) {
                setError(QStringLiteral("Could not serialize an Xaction debug chunk descriptor."));
                return false;
            }
            if (progressCallback && !progressCallback(1)) {
                return false;
            }
        }
        return true;
    }

    bool preparePendingRowMapChunksParallel(std::stop_token stopToken,
                                            const std::uint64_t chunkCount,
                                            const std::size_t workerCount,
                                            const std::function<void(std::uint64_t completed)>& progressCallback)
    {
        if (!currentChunkRecords_.empty()) {
            pendingChunkRecords_.push_back(std::move(currentChunkRecords_));
            currentChunkRecords_.clear();
            ++currentChunkIndex_;
        }

        if (pendingChunkRecords_.size() != chunkCount) {
            setError(QStringLiteral("Xaction index writer did not receive every trace row."));
            return false;
        }

        std::atomic<std::size_t> nextChunkIndex = 0;
        std::atomic_bool failed = false;
        std::mutex errorMutex;
        QString workerError;
        std::mutex preparedMutex;
        std::condition_variable preparedCondition;
        std::vector<std::optional<PreparedRowMapChunk>> preparedChunks(pendingChunkRecords_.size());

        const auto storeFailure = [&](QString error) {
            bool expected = false;
            if (failed.compare_exchange_strong(expected,
                                               true,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
                const std::lock_guard lock(errorMutex);
                workerError = std::move(error);
            }
        };

        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
            workers.emplace_back([&]() {
                try {
                    for (;;) {
                        if (stopToken.stop_requested()
                            || failed.load(std::memory_order_acquire)) {
                            return;
                        }

                        const std::size_t chunkIndex =
                            nextChunkIndex.fetch_add(1, std::memory_order_relaxed);
                        if (chunkIndex >= pendingChunkRecords_.size()) {
                            return;
                        }

                        PreparedRowMapChunk prepared;
                        if (!prepareRowMapChunk(static_cast<std::uint64_t>(chunkIndex),
                                                pendingChunkRecords_[chunkIndex],
                                                prepared,
                                                stopToken)) {
                            storeFailure(prepared.error);
                            return;
                        }
                        {
                            const std::lock_guard lock(preparedMutex);
                            preparedChunks[chunkIndex] = std::move(prepared);
                        }
                        preparedCondition.notify_all();
                    }
                } catch (const std::bad_alloc&) {
                    storeFailure(QStringLiteral("Out of memory while preparing Xaction row-map chunks."));
                } catch (...) {
                    storeFailure(QStringLiteral("Unexpected failure while preparing Xaction row-map chunks."));
                }
            });
        }
        for (std::size_t chunkIndex = 0; chunkIndex < preparedChunks.size(); ++chunkIndex) {
            if (stopToken.stop_requested()) {
                setError(QStringLiteral("Xaction index chunk writing was cancelled."));
                return false;
            }
            std::unique_lock lock(preparedMutex);
            preparedCondition.wait(lock, [&]() {
                return preparedChunks[chunkIndex].has_value()
                    || failed.load(std::memory_order_acquire)
                    || stopToken.stop_requested();
            });
            if (stopToken.stop_requested()) {
                setError(QStringLiteral("Xaction index chunk writing was cancelled."));
                return false;
            }
            if (failed.load(std::memory_order_acquire)
                && !preparedChunks[chunkIndex].has_value()) {
                const std::lock_guard errorLock(errorMutex);
                setError(workerError.isEmpty()
                             ? QStringLiteral("Could not prepare Xaction row-map chunks.")
                             : workerError);
                return false;
            }
            PreparedRowMapChunk prepared = std::move(*preparedChunks[chunkIndex]);
            preparedChunks[chunkIndex].reset();
            lock.unlock();
            if (!appendPreparedRowMapChunk(prepared)) {
                return false;
            }
            if (progressCallback) {
                progressCallback(prepared.chunkIndex + 1);
            }
        }
        workers.clear();

        pendingChunkRecords_.clear();
        return true;
    }

    bool prepareAndWriteChunks(std::stop_token stopToken,
                               const CLogBTraceLoadStageCallback& stageCallback,
                               const CLogBTraceLoadStageProgressCallback& progressCallback)
    {
        const auto toProgressCount = [](const std::uint64_t value) {
            return static_cast<std::size_t>(std::min<std::uint64_t>(
                value,
                static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
        };
        const auto reportStage = [&](const CLogBTraceLoadStage stage,
                                     const std::uint64_t totalRecords,
                                     const std::size_t workerCount = 0) {
            if (stageCallback) {
                stageCallback(stage, workerCount, toProgressCount(totalRecords));
            }
            if (progressCallback) {
                progressCallback(0, toProgressCount(totalRecords));
            }
        };
        const auto reportProgress = [&](const std::uint64_t completed,
                                        const std::uint64_t total) {
            if (progressCallback) {
                progressCallback(toProgressCount(std::min(completed, total)),
                                 toProgressCount(total));
            }
        };

        const std::uint64_t chunkCount = header_.rowMapChunkCount;
        const std::size_t rowMapPrepareWorkerCount = static_cast<std::size_t>(
            std::max<std::uint64_t>(
                1,
                std::min<std::uint64_t>(
                    static_cast<std::uint64_t>(kXactionRowMapChunkPrepareThreadLimit),
                    std::min<std::uint64_t>(
                        std::max<std::uint64_t>(chunkCount, 1),
                        std::max<std::uint64_t>(
                            1,
                            static_cast<std::uint64_t>(
                                std::thread::hardware_concurrency()))))));
        reportStage(CLogBTraceLoadStage::FinalizingIndexRows,
                    std::max<std::uint64_t>(chunkCount, 1),
                    rowMapPrepareWorkerCount);
        if (!preparePendingRowMapChunksParallel(
                stopToken,
                chunkCount,
                rowMapPrepareWorkerCount,
                [&](const std::uint64_t completed) {
                    reportProgress(completed,
                                   std::max<std::uint64_t>(chunkCount, 1));
                })) {
            return false;
        }
        if (flushedRowMapChunkCount_ != chunkCount
            || nextExpectedRow_ != metadata_.totalRecords) {
            setError(QStringLiteral("Xaction index writer did not receive every trace row."));
            return false;
        }
        reportProgress(std::max<std::uint64_t>(chunkCount, 1),
                       std::max<std::uint64_t>(chunkCount, 1));

        std::uint64_t rowListChunkCount = 0;
        for (auto it = rowGroupsByOrdinal_.cbegin(); it != rowGroupsByOrdinal_.cend(); ++it) {
            rowListChunkCount += static_cast<std::uint64_t>(it.value().chunks.size());
        }
        const std::uint64_t directoryTotal = std::max<std::uint64_t>(
            static_cast<std::uint64_t>(strings_.size())
                + static_cast<std::uint64_t>(rowGroupsByOrdinal_.size())
                + rowListChunkCount
                + header_.rowMapChunkCount
                + debugChunkDescriptors_.size(),
            1);
        reportStage(CLogBTraceLoadStage::FinalizingIndexRowDirectory, directoryTotal, 1);
        std::uint64_t completedDirectoryItems = 0;
        const auto reportDirectoryProgress = [&](const std::uint64_t itemCount) {
            if (stopToken.stop_requested()) {
                return false;
            }
            completedDirectoryItems = std::min<std::uint64_t>(
                directoryTotal,
                completedDirectoryItems + itemCount);
            reportProgress(completedDirectoryItems, directoryTotal);
            return true;
        };
        if (!writeStringTable(reportDirectoryProgress)
            || !writeGroupDirectory(reportDirectoryProgress)
            || !writeDebugChunkDescriptorTable(debugChunkDescriptors_, reportDirectoryProgress)
            || !writeRowMapDescriptorTable(rowMapDescriptors_, reportDirectoryProgress)) {
            return false;
        }
        reportProgress(directoryTotal, directoryTotal);
        return true;
    }

    QString indexPath_;
    QString tempPath_;
    QString tracePath_;
    const CLogBTraceMetadata& metadata_;
    QFile file_;
    XactionIndexHeader header_;
    std::vector<CLogBTraceXactionIndexRecord> currentChunkRecords_;
    std::vector<std::vector<CLogBTraceXactionIndexRecord>> pendingChunkRecords_;
    std::vector<TraceSession::XactionRowMapChunkDescriptor> rowMapDescriptors_;
    std::vector<TraceSession::XactionDebugChunkDescriptor> debugChunkDescriptors_;
    std::uint64_t currentChunkIndex_ = 0;
    std::uint64_t nextExpectedRow_ = 0;
    std::uint64_t flushedRowMapChunkCount_ = 0;
    QStringList strings_;
    QHash<QString, std::uint32_t> stringIds_;
    std::mutex stringsMutex_;
    QHash<std::uint64_t, GroupWriteState> rowGroupsByOrdinal_;
    QString lastError_;
};

}  // namespace

bool TraceSession::open(const QString& filePath,
                        std::shared_ptr<TraceSession>& session,
                        CLogBTraceLoadError& error,
                        const TraceSessionOptions& options,
                        const CLogBTraceLoadCallbacks& callbacks,
                        std::stop_token stopToken)
{
    session.reset();

    CLogBTraceMetadata metadata;
    if (!CLogBTraceLoader::scanFile(filePath,
                                    metadata,
                                    error,
                                    callbacks,
                                    stopToken)) {
        return false;
    }
    if (options.parameterOverride.has_value()) {
        metadata.parameters = *options.parameterOverride;
    }

    const QString resolvedFilePath = metadata.filePath;
    session = std::shared_ptr<TraceSession>(
        new TraceSession(resolvedFilePath, std::move(metadata), options, callbacks, stopToken));
    if (stopToken.stop_requested()) {
        session.reset();
        error.type = CLogBTraceLoadError::Type::Cancelled;
        error.summary = QStringLiteral("Loading was cancelled.");
        return false;
    }
    return true;
}

TraceSession::TraceSession(QString filePath,
                           CLogBTraceMetadata metadata,
                           const TraceSessionOptions options,
                           const CLogBTraceLoadCallbacks& callbacks,
                           std::stop_token stopToken)
    : filePath_(std::move(filePath))
    , metadata_(std::move(metadata))
    , pageSize_(std::max<std::size_t>(1, options.pageSize))
    , maxCachedPages_(std::max<std::size_t>(1, options.maxCachedPages))
    , maxCachedBlocks_(std::max<std::size_t>(1, options.maxCachedBlocks))
    , fastIndexEnabled_(options.enableFastIndex && !options.parameterOverride.has_value())
{
    initializeFastIndexState();
    initializeXactionIndexState(callbacks, stopToken);
    publishSnapshot();
}

const QString& TraceSession::filePath() const noexcept
{
    return filePath_;
}

const CLogBTraceMetadata& TraceSession::metadata() const noexcept
{
    return metadata_;
}

std::uint64_t TraceSession::totalRows() const noexcept
{
    return metadata_.totalRecords;
}

std::size_t TraceSession::pageSize() const noexcept
{
    return pageSize_;
}

std::size_t TraceSession::maxCachedPages() const noexcept
{
    return maxCachedPages_;
}

std::size_t TraceSession::maxCachedBlocks() const noexcept
{
    return maxCachedBlocks_;
}

std::size_t TraceSession::cachedPageCount() const noexcept
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    return cachedPages_.size();
}

std::size_t TraceSession::cachedBlockCount() const noexcept
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    return cachedBlocks_.size();
}

std::shared_ptr<const TraceSession::TraceSessionSnapshot> TraceSession::snapshot() const noexcept
{
    return std::atomic_load_explicit(&snapshot_, std::memory_order_acquire);
}

const QString& TraceSession::fastIndexPath() const noexcept
{
    return fastIndexPath_;
}

bool TraceSession::isFastIndexReadable() const noexcept
{
    return fastIndexReadable_;
}

bool TraceSession::isFastIndexWritable() const noexcept
{
    return fastIndexWritable_;
}

bool TraceSession::readFastRecordsFromIndex(const std::size_t blockIndex,
                                            std::vector<CLogBTraceFastRecordInfo>& records) const
{
    return readFastRecordsFromSnapshot(snapshot(), blockIndex, records);
}

bool TraceSession::readFastRecordsFromIndex(const std::size_t blockIndex,
                                            const std::size_t localBegin,
                                            const std::size_t rowCount,
                                            std::vector<CLogBTraceFastRecordInfo>& records) const
{
    return readFastRecordsFromSnapshot(snapshot(), blockIndex, localBegin, rowCount, records);
}

bool TraceSession::readFastRecordsFromSnapshot(
    const std::shared_ptr<const TraceSessionSnapshot>& sessionSnapshot,
    const std::size_t blockIndex,
    std::vector<CLogBTraceFastRecordInfo>& records) const
{
    if (!sessionSnapshot
        || !sessionSnapshot->fastIndexReadable
        || blockIndex >= sessionSnapshot->fastIndexDescriptors.size()) {
        records.clear();
        return false;
    }

    if (const auto found = sessionSnapshot->cachedFastRecordsByBlock.find(blockIndex);
        found != sessionSnapshot->cachedFastRecordsByBlock.end() && found->second) {
        records = *found->second;
        return true;
    }

    return readFastRecordsFromIndexFile(sessionSnapshot->fastIndexPath,
                                        sessionSnapshot->fastIndexDescriptors[blockIndex],
                                        records);
}

bool TraceSession::readFastRecordsFromSnapshot(
    const std::shared_ptr<const TraceSessionSnapshot>& sessionSnapshot,
    const std::size_t blockIndex,
    const std::size_t localBegin,
    const std::size_t rowCount,
    std::vector<CLogBTraceFastRecordInfo>& records) const
{
    if (!sessionSnapshot
        || !sessionSnapshot->fastIndexReadable
        || blockIndex >= sessionSnapshot->fastIndexDescriptors.size()) {
        records.clear();
        return false;
    }

    if (const auto found = sessionSnapshot->cachedFastRecordsByBlock.find(blockIndex);
        found != sessionSnapshot->cachedFastRecordsByBlock.end() && found->second) {
        const auto& cached = *found->second;
        if (localBegin > cached.size()) {
            records.clear();
            return false;
        }
        const std::size_t localEnd = std::min<std::size_t>(cached.size(), localBegin + rowCount);
        records.assign(cached.begin() + static_cast<std::ptrdiff_t>(localBegin),
                       cached.begin() + static_cast<std::ptrdiff_t>(localEnd));
        return true;
    }

    return readFastRecordsFromIndexFile(sessionSnapshot->fastIndexPath,
                                        sessionSnapshot->fastIndexDescriptors[blockIndex],
                                        localBegin,
                                        rowCount,
                                        records);
}

QString TraceSession::optionalFieldFastIndexPath(const QString& fieldName) const
{
    return optionalFieldIndexPathForTrace(filePath_, fieldName);
}

bool TraceSession::hasOptionalFieldFastIndex(const QString& fieldName)
{
    if (!ensureOptionalFieldIndexState(fieldName)) {
        return false;
    }

    OptionalFieldIndexState state;
    {
        const std::lock_guard lock(optionalFieldIndexesMutex_);
        const auto found = optionalFieldIndexes_.constFind(fieldName);
        if (found == optionalFieldIndexes_.cend()) {
            return false;
        }
        state = found.value();
    }
    if (!state.readable || state.descriptors.size() != metadata_.blocks.size()) {
        return false;
    }

    for (std::size_t index = 0; index < state.descriptors.size(); ++index) {
        const CLogBTraceBlockSummary& block = metadata_.blocks[index];
        const FastIndexDescriptor& descriptor = state.descriptors[index];
        if (block.recordCount > 0 && descriptor.recordCount != block.recordCount) {
            return false;
        }
    }

    return true;
}

bool TraceSession::clearOptionalFieldFastIndex(const QString& fieldName,
                                               CLogBTraceLoadError& error)
{
    error = {};
    if (!fastIndexEnabled_) {
        error.summary = QStringLiteral("Fast indexes are disabled for this trace session.");
        return false;
    }

    if (fieldName.isEmpty()) {
        error.summary = QStringLiteral("Optional field name is empty.");
        return false;
    }

    const QString indexPath = optionalFieldFastIndexPath(fieldName);
    if (indexPath.isEmpty()) {
        error.summary = QStringLiteral("Optional field fast index path is empty.");
        return false;
    }

    if (QFileInfo::exists(indexPath) && !QFile::remove(indexPath)) {
        error.summary = QStringLiteral("Could not delete fast index for optional field %1.").arg(fieldName);
        error.informativeText = indexPath;
        return false;
    }

    {
        const std::lock_guard lock(optionalFieldIndexesMutex_);
        optionalFieldIndexes_.remove(fieldName);
    }
    clearOptionalFieldRecordCache(fieldName);
    return true;
}

void TraceSession::clearCache()
{
    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(rowBlockCacheMutex_);
        LogTraceSessionLockWait("rowBlockCacheMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        cachedPages_.clear();
        lruPageIndexes_.clear();
        cachedBlocks_.clear();
        lruBlockIndexes_.clear();
    }
    clearOptionalFieldRecordCache();
    publishSnapshotNoThrow();
}

void TraceSession::refreshCachedXactionIndexRows()
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    if (!xactionIndexEnabled_ || cachedPages_.empty()) {
        return;
    }

    for (auto& [_, entry] : cachedPages_) {
        applyXactionIndexToRows(entry.page.rowStart, entry.page.rows);
    }
}

bool TraceSession::ensureRows(const std::uint64_t beginRow,
                              const std::uint64_t rowCount,
                              CLogBTraceLoadError& error,
                              const CLogBTraceLoadCallbacks& callbacks,
                              std::stop_token stopToken)
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    error = {};
    if (beginRow > metadata_.totalRecords) {
        error.summary = QStringLiteral("Requested row range starts beyond the end of the trace.");
        return false;
    }

    const std::uint64_t clampedRowCount = std::min<std::uint64_t>(
        rowCount,
        metadata_.totalRecords - beginRow);
    if (clampedRowCount == 0) {
        return true;
    }

    const std::uint64_t cacheCapacityRows = static_cast<std::uint64_t>(pageSize_ * maxCachedPages_);
    if (clampedRowCount <= cacheCapacityRows
        && !areRowsCached(beginRow, clampedRowCount)) {
        return loadAlignedRangeAndCache(beginRow, clampedRowCount, error, callbacks, stopToken);
    }

    const std::uint64_t endRow = beginRow + clampedRowCount;
    const std::uint64_t firstPage = beginRow / pageSize_;
    const std::uint64_t lastPage = (endRow - 1) / pageSize_;
    for (std::uint64_t pageIndex = firstPage; pageIndex <= lastPage; ++pageIndex) {
        if (!ensurePageLoaded(pageIndex, error, callbacks, stopToken)) {
            return false;
        }
    }

    return true;
}

bool TraceSession::readRows(const std::uint64_t beginRow,
                            const std::uint64_t rowCount,
                            std::vector<FlitRecord>& rows,
                            CLogBTraceLoadError& error,
                            const CLogBTraceLoadCallbacks& callbacks,
                            std::stop_token stopToken)
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    rows.clear();
    error = {};

    if (beginRow > metadata_.totalRecords) {
        error.summary = QStringLiteral("Requested row range starts beyond the end of the trace.");
        return false;
    }

    const std::uint64_t clampedRowCount = std::min<std::uint64_t>(
        rowCount,
        metadata_.totalRecords - beginRow);
    if (clampedRowCount == 0) {
        return true;
    }

    const std::uint64_t cacheCapacityRows = static_cast<std::uint64_t>(pageSize_ * maxCachedPages_);
    if (clampedRowCount <= cacheCapacityRows) {
        if (!areRowsCached(beginRow, clampedRowCount)
            && !loadAlignedRangeAndCache(beginRow, clampedRowCount, error, callbacks, stopToken)) {
            return false;
        }
    }

    if (areRowsCached(beginRow, clampedRowCount)) {
        rows.reserve(static_cast<std::size_t>(clampedRowCount));
        const std::uint64_t endRow = beginRow + clampedRowCount;
        const std::uint64_t firstPage = beginRow / pageSize_;
        const std::uint64_t lastPage = (endRow - 1) / pageSize_;
        for (std::uint64_t pageIndex = firstPage; pageIndex <= lastPage; ++pageIndex) {
            const auto found = cachedPages_.find(pageIndex);
            if (found == cachedPages_.end()) {
                error.summary = QStringLiteral("TraceSession cache lost a requested page.");
                rows.clear();
                return false;
            }

            const CachedPage& page = found->second.page;
            const std::uint64_t pageEnd = page.rowStart + page.rows.size();
            const std::uint64_t copyBegin = std::max(beginRow, page.rowStart);
            const std::uint64_t copyEnd = std::min(endRow, pageEnd);
            const std::size_t localBegin = static_cast<std::size_t>(copyBegin - page.rowStart);
            const std::size_t localEnd = static_cast<std::size_t>(copyEnd - page.rowStart);

            rows.insert(rows.end(),
                        page.rows.begin() + static_cast<std::ptrdiff_t>(localBegin),
                        page.rows.begin() + static_cast<std::ptrdiff_t>(localEnd));
            touchPage(pageIndex);
        }

        return true;
    }

    if (!CLogBTraceLoader::loadRows(filePath_,
                                    metadata_,
                                    beginRow,
                                    clampedRowCount,
                                    rows,
                                    error,
                                    callbacks,
                                    stopToken)) {
        return false;
    }

    applyXactionIndexToRows(beginRow, rows);
    return true;
}

bool TraceSession::readRowsDirect(const std::uint64_t beginRow,
                                  const std::uint64_t rowCount,
                                  std::vector<FlitRecord>& rows,
                                  CLogBTraceLoadError& error,
                                  const CLogBTraceLoadCallbacks& callbacks,
                                  std::stop_token stopToken) const
{
    return readRowsDirect(snapshot(), beginRow, rowCount, rows, error, callbacks, stopToken);
}

bool TraceSession::readRowsDirect(
    const std::shared_ptr<const TraceSessionSnapshot>& sessionSnapshot,
    const std::uint64_t beginRow,
    const std::uint64_t rowCount,
    std::vector<FlitRecord>& rows,
    CLogBTraceLoadError& error,
    const CLogBTraceLoadCallbacks& callbacks,
    std::stop_token stopToken) const
{
    rows.clear();
    error = {};

    if (!sessionSnapshot) {
        error.summary = QStringLiteral("Trace session snapshot is not available.");
        return false;
    }

    if (beginRow > sessionSnapshot->metadata.totalRecords) {
        error.summary = QStringLiteral("Requested row range starts beyond the end of the trace.");
        return false;
    }

    const std::uint64_t clampedRowCount = std::min<std::uint64_t>(
        rowCount,
        sessionSnapshot->metadata.totalRecords - beginRow);
    if (clampedRowCount == 0) {
        return true;
    }

    if (!CLogBTraceLoader::loadRows(sessionSnapshot->filePath,
                                    sessionSnapshot->metadata,
                                    beginRow,
                                    clampedRowCount,
                                    rows,
                                    error,
                                    callbacks,
                                    stopToken)) {
        return false;
    }

    applyXactionIndexToRows(*sessionSnapshot, beginRow, rows);
    return true;
}

bool TraceSession::collectRowsForTransportMask(const std::size_t blockIndex,
                                               const CLogBTraceChannelMask enabledMask,
                                               std::vector<int>& logicalRows,
                                               CLogBTraceLoadError& error,
                                               const CLogBTraceLoadCallbacks& callbacks,
                                               std::stop_token stopToken)
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    error = {};

    if (blockIndex >= metadata_.blocks.size()) {
        error.summary = QStringLiteral("Requested block index is out of range.");
        return false;
    }

    if (!ensureBlockLoaded(blockIndex, error, callbacks, stopToken)) {
        return false;
    }

    const std::shared_ptr<CLog::CLogB::TagCHIRecords> block = cachedBlock(blockIndex);
    if (!block) {
        error.summary = QStringLiteral("TraceSession block cache lost a requested CHI record block.");
        return false;
    }

    const CLogBTraceBlockSummary& blockSummary = metadata_.blocks[blockIndex];
    logicalRows.reserve(logicalRows.size() + block->records.size());
    for (std::size_t localIndex = 0; localIndex < block->records.size(); ++localIndex) {
        const auto channelIndex = static_cast<unsigned int>(block->records[localIndex].channel);
        if ((enabledMask & (CLogBTraceChannelMask{1} << channelIndex)) == 0) {
            continue;
        }

        const std::uint64_t logicalRow = blockSummary.rowStart + localIndex;
        if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            break;
        }

        logicalRows.push_back(static_cast<int>(logicalRow));
    }

    return true;
}

bool TraceSession::collectRowsMatchingFastFilter(const std::size_t blockIndex,
                                                 const CLogBTraceFastFilter& filter,
                                                 std::vector<int>& logicalRows,
                                                 CLogBTraceLoadError& error,
                                                 const CLogBTraceLoadCallbacks& callbacks,
                                                 std::stop_token stopToken)
{
    const std::lock_guard lock(rowBlockCacheMutex_);
    error = {};

    if (blockIndex >= metadata_.blocks.size()) {
        error.summary = QStringLiteral("Requested block index is out of range.");
        return false;
    }

    if (!ensureBlockLoaded(blockIndex, error, callbacks, stopToken)) {
        return false;
    }

    std::vector<std::size_t> localRows;
    const auto found = cachedBlocks_.find(blockIndex);
    if (found == cachedBlocks_.end() || !found->second.block) {
        error.summary = QStringLiteral("TraceSession block cache lost a requested CHI record block.");
        return false;
    }

    if (found->second.fastRecords) {
        if (!CLogBTraceLoader::collectFastRecordRowsMatchingFilter(metadata_,
                                                                   filter,
                                                                   *found->second.fastRecords,
                                                                   localRows,
                                                                   error)) {
            return false;
        }
        touchBlock(blockIndex);
    } else {
        std::vector<CLogBTraceFastRecordInfo> fastRecords;
        if (!CLogBTraceLoader::collectBlockRowsMatchingFilterAndBuildRecords(metadata_,
                                                                             blockIndex,
                                                                             *found->second.block,
                                                                             filter,
                                                                             localRows,
                                                                             fastRecords,
                                                                             error,
                                                                             stopToken)) {
            return false;
        }
        tryPersistFastRecordsToIndex(blockIndex, fastRecords);
        found->second.fastRecords =
            std::make_shared<std::vector<CLogBTraceFastRecordInfo>>(std::move(fastRecords));
        touchBlock(blockIndex);
    }

    const CLogBTraceBlockSummary& blockSummary = metadata_.blocks[blockIndex];
    logicalRows.reserve(logicalRows.size() + localRows.size());
    for (const std::size_t localIndex : localRows) {
        const std::uint64_t logicalRow = blockSummary.rowStart + localIndex;
        if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            break;
        }

        logicalRows.push_back(static_cast<int>(logicalRow));
    }

    return true;
}

bool TraceSession::collectRowsMatchingFastFilterRange(const std::size_t blockIndex,
                                                      const CLogBTraceFastFilter& filter,
                                                      const std::size_t localBegin,
                                                      const std::size_t rowCount,
                                                      std::vector<int>& logicalRows,
                                                      CLogBTraceLoadError& error,
                                                      std::stop_token stopToken)
{
    const std::lock_guard lock(rowBlockCacheMutex_);
    error = {};

    if (blockIndex >= metadata_.blocks.size()) {
        error.summary = QStringLiteral("Requested block index is out of range.");
        return false;
    }

    if (!ensureFastRecordsLoaded(blockIndex, error, stopToken)) {
        return false;
    }

    const std::shared_ptr<const std::vector<CLogBTraceFastRecordInfo>> fastRecords =
        cachedFastRecords(blockIndex);
    if (!fastRecords) {
        error.summary = QStringLiteral("TraceSession fast-record cache lost a requested block.");
        return false;
    }

    std::vector<std::size_t> localRows;
    if (!CLogBTraceLoader::collectFastRecordRowsMatchingFilterRange(metadata_,
                                                                    filter,
                                                                    *fastRecords,
                                                                    localBegin,
                                                                    rowCount,
                                                                    localRows,
                                                                    error)) {
        return false;
    }

    const CLogBTraceBlockSummary& blockSummary = metadata_.blocks[blockIndex];
    logicalRows.reserve(logicalRows.size() + localRows.size());
    for (const std::size_t localIndex : localRows) {
        const std::uint64_t logicalRow = blockSummary.rowStart + localIndex;
        if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            break;
        }

        logicalRows.push_back(static_cast<int>(logicalRow));
    }

    return true;
}

bool TraceSession::collectRowsMatchingFastFilterAndOptionalFieldIndexesRange(
    const std::size_t blockIndex,
    const CLogBTraceFastFilter& filter,
    const QHash<QString, QString>& optionalFieldFilters,
    const std::size_t localBegin,
    const std::size_t rowCount,
    std::vector<int>& logicalRows,
    CLogBTraceLoadError& error,
    std::stop_token stopToken)
{
    const std::lock_guard lock(rowBlockCacheMutex_);
    logicalRows.clear();
    error = {};

    if (blockIndex >= metadata_.blocks.size()) {
        error.summary = QStringLiteral("Requested block index is out of range.");
        return false;
    }

    if (optionalFieldFilters.isEmpty()) {
        return collectRowsMatchingFastFilterRange(blockIndex,
                                                  filter,
                                                  localBegin,
                                                  rowCount,
                                                  logicalRows,
                                                  error,
                                                  stopToken);
    }

    const CLogBTraceBlockSummary& blockSummary = metadata_.blocks[blockIndex];
    if (localBegin >= blockSummary.recordCount || rowCount == 0) {
        return true;
    }

    const std::size_t localEnd = std::min<std::size_t>(
        static_cast<std::size_t>(blockSummary.recordCount),
        localBegin + rowCount);

    struct OptionalFilterContext {
        QString fieldName;
        OptionalFieldIndexFilter filter;
        std::shared_ptr<const std::vector<CachedOptionalFieldIndexRecord>> records;
    };
    std::vector<OptionalFilterContext> optionalFilters;
    optionalFilters.reserve(static_cast<std::size_t>(optionalFieldFilters.size()));

    for (auto it = optionalFieldFilters.cbegin(); it != optionalFieldFilters.cend(); ++it) {
        if (stopToken.stop_requested()) {
            error.type = CLogBTraceLoadError::Type::Cancelled;
            error.summary = QStringLiteral("Optional field index filtering was cancelled.");
            return false;
        }

        if (!ensureOptionalFieldIndexState(it.key())) {
            error.summary = QStringLiteral("Fast index for optional field %1 is not available.").arg(it.key());
            return false;
        }

        OptionalFieldIndexState state;
        {
            const std::lock_guard lock(optionalFieldIndexesMutex_);
            const auto found = optionalFieldIndexes_.constFind(it.key());
            if (found == optionalFieldIndexes_.cend()) {
                error.summary = QStringLiteral("Fast index for optional field %1 is not available.").arg(it.key());
                return false;
            }
            state = found.value();
        }
        if (!state.readable || blockIndex >= state.descriptors.size()) {
            error.summary = QStringLiteral("Fast index for optional field %1 is not readable.").arg(it.key());
            return false;
        }

        if (!ensureOptionalFieldRecordsLoaded(blockIndex, it.key(), state, error)) {
            if (error.summary.isEmpty()) {
                error.summary = QStringLiteral("Fast index for optional field %1 could not be read.").arg(it.key());
            }
            return false;
        }

        std::shared_ptr<const std::vector<CachedOptionalFieldIndexRecord>> fieldRecords =
            cachedOptionalFieldRecords(blockIndex, it.key());
        if (!fieldRecords) {
            error.summary = QStringLiteral("Fast index for optional field %1 could not be read.").arg(it.key());
            return false;
        }

        if (fieldRecords->size() != blockSummary.recordCount) {
            error.summary = QStringLiteral("Fast index for optional field %1 has an invalid record count.").arg(it.key());
            return false;
        }

        optionalFilters.push_back(OptionalFilterContext{
            .fieldName = it.key(),
            .filter = prepareOptionalFieldIndexFilter(it.value()),
            .records = std::move(fieldRecords),
        });
    }

    const bool fastFilterIsIdentity = filter.transportMask == CLogBAllChannelMask()
        && filter.opcodeFilter.isEmpty()
        && filter.sourceIdFilter.isEmpty()
        && filter.targetIdFilter.isEmpty()
        && filter.txnIdFilter.isEmpty()
        && filter.dbidFilter.isEmpty()
        && filter.addressFilter.isEmpty();

    const auto appendLogicalRowsFromLocalRows = [&](const std::vector<std::size_t>& localRows) {
        logicalRows.reserve(logicalRows.size() + localRows.size());
        for (const std::size_t localIndex : localRows) {
            const std::uint64_t logicalRow = blockSummary.rowStart + localIndex;
            if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
                break;
            }
            logicalRows.push_back(static_cast<int>(logicalRow));
        }
    };

    if (fastFilterIsIdentity && optionalFilters.size() == 1) {
        const OptionalFilterContext& optionalFilter = optionalFilters.front();
        const std::vector<CachedOptionalFieldIndexRecord>& fieldRecords = *optionalFilter.records;
        logicalRows.reserve(localEnd - localBegin);
        for (std::size_t localIndex = localBegin; localIndex < localEnd; ++localIndex) {
            if (matchesOptionalFieldIndexRecord(fieldRecords[localIndex], optionalFilter.filter)) {
                const std::uint64_t logicalRow = blockSummary.rowStart + localIndex;
                if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
                    break;
                }
                logicalRows.push_back(static_cast<int>(logicalRow));
            }
        }
        return true;
    }

    std::vector<std::size_t> candidateRows;
    if (fastFilterIsIdentity) {
        candidateRows.resize(localEnd - localBegin);
        std::iota(candidateRows.begin(), candidateRows.end(), localBegin);
    } else {
        std::vector<int> fastLogicalRows;
        if (!collectRowsMatchingFastFilterRange(blockIndex,
                                                filter,
                                                localBegin,
                                                localEnd - localBegin,
                                                fastLogicalRows,
                                                error,
                                                stopToken)) {
            return false;
        }

        candidateRows.reserve(fastLogicalRows.size());
        for (const int logicalRow : fastLogicalRows) {
            if (logicalRow >= 0 && static_cast<std::uint64_t>(logicalRow) >= blockSummary.rowStart) {
                const std::size_t localIndex = static_cast<std::size_t>(
                    static_cast<std::uint64_t>(logicalRow) - blockSummary.rowStart);
                if (localIndex >= localBegin && localIndex < localEnd) {
                    candidateRows.push_back(localIndex);
                }
            }
        }
    }

    for (const OptionalFilterContext& optionalFilter : optionalFilters) {
        if (candidateRows.empty()) {
            return true;
        }

        const std::vector<CachedOptionalFieldIndexRecord>& fieldRecords = *optionalFilter.records;
        auto writeIt = candidateRows.begin();
        for (const std::size_t localIndex : candidateRows) {
            if (matchesOptionalFieldIndexRecord(fieldRecords[localIndex], optionalFilter.filter)) {
                *writeIt = localIndex;
                ++writeIt;
            }
        }
        candidateRows.erase(writeIt, candidateRows.end());
    }

    appendLogicalRowsFromLocalRows(candidateRows);
    return true;
}

bool TraceSession::buildOptionalFieldFastIndex(const QString& fieldName,
                                               CLogBTraceLoadError& error,
                                               const CLogBTraceLoadCallbacks& callbacks,
                                               std::stop_token stopToken)
{
    error = {};
    if (!fastIndexEnabled_) {
        error.summary = QStringLiteral("Fast indexes are disabled for this trace session.");
        return false;
    }

    if (fieldName.isEmpty()) {
        error.summary = QStringLiteral("Optional field name is empty.");
        return false;
    }

    {
        const std::lock_guard lock(optionalFieldIndexesMutex_);
        optionalFieldIndexes_.remove(fieldName);
    }
    clearOptionalFieldRecordCache(fieldName);

    OptionalFieldIndexState state;
    if (!initializeOptionalFieldIndexFile(fieldName, state)) {
        error.summary = QStringLiteral("Could not create fast index for optional field %1.").arg(fieldName);
        error.informativeText = optionalFieldFastIndexPath(fieldName);
        return false;
    }

    const auto discardIndexFile = [&state]() {
        removeOptionalFieldIndexFile(state.path);
    };
    const auto setCancelledError = [](CLogBTraceLoadError& cancelledError) {
        cancelledError.type = CLogBTraceLoadError::Type::Cancelled;
        cancelledError.summary = QStringLiteral("Optional field fast index creation was cancelled.");
    };

    const std::size_t blockCount = metadata_.blocks.size();
    const std::size_t totalRecordCount = static_cast<std::size_t>(metadata_.totalRecords);
    const std::size_t totalWorkerCount = optionalFieldIndexWorkerCount(metadata_.totalRecords);
    const std::size_t blockWorkerCount = blockCount >= totalWorkerCount ? totalWorkerCount : 1;
    const std::size_t recordWorkerCount = blockWorkerCount > 1 ? 1 : totalWorkerCount;
    if (callbacks.stage) {
        callbacks.stage(CLogBTraceLoadStage::Formatting, totalWorkerCount, totalRecordCount);
    }
    if (callbacks.stageProgress) {
        callbacks.stageProgress(0, totalRecordCount);
    }

    std::atomic<std::size_t> nextBlockIndex = 0;
    std::atomic<std::size_t> completedRecords = 0;
    std::atomic_bool failed = false;
    std::mutex writeMutex;
    std::mutex errorMutex;
    CLogBTraceLoadError firstError;

    const auto recordFailure = [&](CLogBTraceLoadError localError) {
        if (localError.summary.isEmpty()) {
            if (stopToken.stop_requested()) {
                setCancelledError(localError);
            } else {
                localError.summary = QStringLiteral("Optional field fast index creation failed.");
            }
        }

        bool expected = false;
        if (failed.compare_exchange_strong(expected,
                                           true,
                                           std::memory_order_relaxed,
                                           std::memory_order_relaxed)) {
            const std::lock_guard lock(errorMutex);
            firstError = std::move(localError);
        }
    };

    const auto indexBlock = [&](const std::size_t blockIndex,
                                CLogBTraceLoadError& localError) -> bool {
        if (stopToken.stop_requested()) {
            setCancelledError(localError);
            return false;
        }

        std::shared_ptr<CLog::CLogB::TagCHIRecords> block;
        if (!CLogBTraceLoader::loadBlockRecords(filePath_,
                                                metadata_,
                                                blockIndex,
                                                block,
                                                localError,
                                                {},
                                               stopToken)) {
            return false;
        }

        if (stopToken.stop_requested()) {
            setCancelledError(localError);
            return false;
        }

        if (!block) {
            localError.summary = QStringLiteral("The CHI record block loaded for optional field indexing was empty.");
            return false;
        }

        std::vector<OptionalFieldIndexRecord> fieldRecords;
        std::mutex blockProgressMutex;
        std::size_t blockReportedRecords = 0;
        const auto publishProgressDelta = [&](const std::size_t delta) {
            if (delta == 0) {
                return;
            }

            const std::size_t completed =
                completedRecords.fetch_add(delta, std::memory_order_relaxed) + delta;
            if (callbacks.stageProgress) {
                callbacks.stageProgress(std::min(completed, totalRecordCount), totalRecordCount);
            }
        };
        const CLogBTraceLoadStageProgressCallback blockProgress =
            [&](const std::size_t completed, const std::size_t total) {
                (void)total;
                std::size_t delta = 0;
                {
                    const std::lock_guard lock(blockProgressMutex);
                    if (completed <= blockReportedRecords) {
                        return;
                    }
                    delta = completed - blockReportedRecords;
                    blockReportedRecords = completed;
                }
                publishProgressDelta(delta);
            };

        if (!CLogBTraceLoader::buildBlockOptionalFieldRecords(metadata_,
                                                             blockIndex,
                                                             *block,
                                                             fieldName,
                                                             fieldRecords,
                                                             localError,
                                                             recordWorkerCount,
                                                             blockProgress,
                                                             stopToken)) {
            return false;
        }

        {
            const std::lock_guard lock(blockProgressMutex);
            if (blockReportedRecords < block->records.size()) {
                publishProgressDelta(block->records.size() - blockReportedRecords);
                blockReportedRecords = block->records.size();
            }
        }

        {
            const std::lock_guard lock(writeMutex);
            if (!writeOptionalFieldRecords(blockIndex, state, fieldRecords, stopToken)) {
                if (stopToken.stop_requested()) {
                    setCancelledError(localError);
                } else {
                    localError.summary = QStringLiteral("Could not write fast index data for optional field %1.")
                                             .arg(fieldName);
                    localError.informativeText = state.path;
                }
                return false;
            }
        }

        return true;
    };

    if (blockWorkerCount <= 1) {
        for (std::size_t blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
            if (stopToken.stop_requested()) {
                CLogBTraceLoadError cancelled;
                setCancelledError(cancelled);
                recordFailure(std::move(cancelled));
                break;
            }

            CLogBTraceLoadError localError;
            if (!indexBlock(blockIndex, localError)) {
                recordFailure(std::move(localError));
                break;
            }
        }
    } else {
        std::vector<std::jthread> workers;
        workers.reserve(blockWorkerCount);
        for (std::size_t workerIndex = 0; workerIndex < blockWorkerCount; ++workerIndex) {
            workers.emplace_back([&]() {
                try {
                    while (!failed.load(std::memory_order_relaxed) && !stopToken.stop_requested()) {
                        const std::size_t blockIndex = nextBlockIndex.fetch_add(1, std::memory_order_relaxed);
                        if (blockIndex >= blockCount) {
                            return;
                        }

                        CLogBTraceLoadError localError;
                        if (!indexBlock(blockIndex, localError)) {
                            recordFailure(std::move(localError));
                            return;
                        }
                    }
                } catch (const std::bad_alloc&) {
                    CLogBTraceLoadError localError;
                    localError.summary = QStringLiteral("Optional field fast index creation ran out of memory.");
                    recordFailure(std::move(localError));
                } catch (const std::exception& exception) {
                    CLogBTraceLoadError localError;
                    localError.summary = QStringLiteral("Optional field fast index creation failed unexpectedly.");
                    localError.informativeText = QString::fromLocal8Bit(exception.what());
                    recordFailure(std::move(localError));
                } catch (...) {
                    CLogBTraceLoadError localError;
                    localError.summary = QStringLiteral("Optional field fast index creation failed unexpectedly.");
                    localError.informativeText =
                        QStringLiteral("A non-standard exception escaped an index worker.");
                    recordFailure(std::move(localError));
                }
            });
        }
    }

    if (failed.load(std::memory_order_relaxed)) {
        const std::lock_guard lock(errorMutex);
        error = std::move(firstError);
        discardIndexFile();
        return false;
    }

    if (stopToken.stop_requested()) {
        setCancelledError(error);
        discardIndexFile();
        return false;
    }

    state.readable = true;
    state.writable = true;
    {
        const std::lock_guard lock(optionalFieldIndexesMutex_);
        optionalFieldIndexes_.insert(fieldName, std::move(state));
    }
    if (callbacks.stage) {
        callbacks.stage(CLogBTraceLoadStage::Completed, totalWorkerCount, totalRecordCount);
    }
    return true;
}

bool TraceSession::supportsXactionIndexing() const noexcept
{
    return xactionIndexEnabled_;
}

bool TraceSession::isXactionIndexStarted() const noexcept
{
    const std::lock_guard lock(xactionIndexMutex_);
    return xactionIndexStarted_;
}

bool TraceSession::isXactionIndexComplete() const noexcept
{
    const std::lock_guard lock(xactionIndexMutex_);
    return xactionIndexComplete_;
}

std::vector<std::uint64_t> TraceSession::xactionOrdinals() const
{
    std::vector<std::uint64_t> ordinals;
    std::vector<XactionRowsDescriptorByOrdinal> descriptors;
    if (!xactionGroupDescriptors(descriptors)) {
        return ordinals;
    }

    ordinals.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
        descriptors.size(),
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))));
    for (const XactionRowsDescriptorByOrdinal& entry : descriptors) {
        ordinals.push_back(entry.ordinal);
    }
    return ordinals;
}

bool TraceSession::xactionRowCountsByOrdinal(std::unordered_map<std::uint64_t, std::uint64_t>& rowCounts) const
{
    rowCounts.clear();
    std::vector<XactionRowsDescriptorByOrdinal> descriptors;
    if (!xactionGroupDescriptors(descriptors)) {
        return false;
    }

    rowCounts.reserve(descriptors.size());
    for (const XactionRowsDescriptorByOrdinal& entry : descriptors) {
        rowCounts.emplace(entry.ordinal, entry.descriptor.rowCount);
    }
    return true;
}

bool TraceSession::xactionTypesByOrdinal(std::unordered_map<std::uint64_t, QString>& types) const
{
    types.clear();

    QString indexPath;
    std::uint64_t stringTableOffset = 0;
    std::uint64_t stringCount = 0;
    std::uint64_t indexedOrdinalCount = 0;
    std::vector<XactionRowMapChunkDescriptor> rowMapDescriptors;
    std::vector<XactionDebugChunkDescriptor> debugDescriptors;
    {
        const std::lock_guard lock(xactionIndexMutex_);
        if (!xactionIndexComplete_ || xactionIndexPath_.isEmpty()) {
            return false;
        }
        if (cachedXactionTypesByOrdinal_) {
            types = *cachedXactionTypesByOrdinal_;
            return true;
        }
        indexPath = xactionIndexPath_;
        stringTableOffset = xactionStringTableOffset_;
        stringCount = xactionStringCount_;
        indexedOrdinalCount = xactionGroupCount_;
        rowMapDescriptors = xactionRowMapChunkDescriptors_;
        debugDescriptors = xactionDebugChunkDescriptors_;
    }

    if (rowMapDescriptors.size() != debugDescriptors.size()) {
        return false;
    }
    types.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
        indexedOrdinalCount,
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))));

    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }
    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());

    std::unordered_map<std::uint32_t, QString> stringCache;
    const auto readString = [&](const std::uint32_t oneBasedIndex, QString& value) -> bool {
        value.clear();
        if (oneBasedIndex == 0) {
            return true;
        }
        if (oneBasedIndex > stringCount) {
            return false;
        }

        const auto found = stringCache.find(oneBasedIndex);
        if (found != stringCache.end()) {
            value = found->second;
            return true;
        }

        XactionStringDescriptor descriptor;
        if (!readXactionStringDescriptor(input,
                                         stringTableOffset,
                                         static_cast<std::uint64_t>(oneBasedIndex - 1),
                                         descriptor)) {
            return false;
        }
        if (descriptor.dataOffset > fileSize
            || descriptor.bytes > fileSize
            || descriptor.dataOffset + descriptor.bytes > fileSize) {
            return false;
        }
        if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
            return false;
        }

        const QByteArray bytes = input.read(static_cast<qint64>(descriptor.bytes));
        if (bytes.size() != static_cast<int>(descriptor.bytes)) {
            return false;
        }
        value = QString::fromUtf8(bytes);
        stringCache.emplace(oneBasedIndex, value);
        return true;
    };

    for (std::size_t chunkIndex = 0; chunkIndex < rowMapDescriptors.size(); ++chunkIndex) {
        const XactionRowMapChunkDescriptor& rowDescriptor = rowMapDescriptors[chunkIndex];
        const XactionDebugChunkDescriptor& debugDescriptor = debugDescriptors[chunkIndex];
        if (rowDescriptor.storedBytes == 0 || debugDescriptor.storedBytes == 0) {
            continue;
        }
        if (rowDescriptor.dataOffset > fileSize
            || rowDescriptor.storedBytes > fileSize
            || rowDescriptor.dataOffset + rowDescriptor.storedBytes > fileSize
            || debugDescriptor.dataOffset > fileSize
            || debugDescriptor.storedBytes > fileSize
            || debugDescriptor.dataOffset + debugDescriptor.storedBytes > fileSize) {
            types.clear();
            return false;
        }

        if (!input.seek(static_cast<qint64>(rowDescriptor.dataOffset))) {
            types.clear();
            return false;
        }
        const QByteArray storedRowBytes = input.read(static_cast<qint64>(rowDescriptor.storedBytes));
        if (storedRowBytes.size() != static_cast<int>(rowDescriptor.storedBytes)) {
            types.clear();
            return false;
        }
        const QByteArray rowPayload = (rowDescriptor.flags & kXactionRowChunkCompressed) != 0
            ? qUncompress(storedRowBytes)
            : storedRowBytes;
        if (rowPayload.size() != static_cast<int>(rowDescriptor.uncompressedBytes)) {
            types.clear();
            return false;
        }

        if (!input.seek(static_cast<qint64>(debugDescriptor.dataOffset))) {
            types.clear();
            return false;
        }
        const QByteArray storedDebugBytes = input.read(static_cast<qint64>(debugDescriptor.storedBytes));
        if (storedDebugBytes.size() != static_cast<int>(debugDescriptor.storedBytes)) {
            types.clear();
            return false;
        }
        const QByteArray debugPayload = (debugDescriptor.flags & kXactionRowChunkCompressed) != 0
            ? qUncompress(storedDebugBytes)
            : storedDebugBytes;
        if (debugPayload.size() != static_cast<int>(debugDescriptor.uncompressedBytes)) {
            types.clear();
            return false;
        }

        QDataStream rowStream(rowPayload);
        rowStream.setByteOrder(QDataStream::LittleEndian);
        quint32 rowEntryCount = 0;
        rowStream >> rowEntryCount;
        if (rowStream.status() != QDataStream::Ok || rowEntryCount > rowDescriptor.rowCount) {
            types.clear();
            return false;
        }

        std::unordered_map<std::uint32_t, std::uint64_t> ordinalByLocalRow;
        ordinalByLocalRow.reserve(rowEntryCount);
        for (std::uint32_t entryIndex = 0; entryIndex < rowEntryCount; ++entryIndex) {
            quint32 localRow = 0;
            quint8 flags = 0;
            quint64 ordinal = 0;
            rowStream >> localRow
                      >> flags
                      >> ordinal;
            if (rowStream.status() != QDataStream::Ok || localRow >= rowDescriptor.rowCount) {
                types.clear();
                return false;
            }
            if ((flags & XactionRowIndexed) != 0 && ordinal != 0) {
                ordinalByLocalRow[static_cast<std::uint32_t>(localRow)] =
                    static_cast<std::uint64_t>(ordinal);
            }
        }

        if (ordinalByLocalRow.empty()) {
            continue;
        }

        QDataStream debugStream(debugPayload);
        debugStream.setByteOrder(QDataStream::LittleEndian);
        quint32 debugEntryCount = 0;
        debugStream >> debugEntryCount;
        if (debugStream.status() != QDataStream::Ok || debugEntryCount > debugDescriptor.rowCount) {
            types.clear();
            return false;
        }

        for (std::uint32_t entryIndex = 0; entryIndex < debugEntryCount; ++entryIndex) {
            quint32 localRow = 0;
            XactionRowDebugIds debugIds;
            debugStream >> localRow
                        >> debugIds.jointType
                        >> debugIds.jointPath
                        >> debugIds.denialName
                        >> debugIds.denialCode
                        >> debugIds.xactionType;
            if (debugStream.status() != QDataStream::Ok || localRow >= debugDescriptor.rowCount) {
                types.clear();
                return false;
            }
            const auto ordinalFound = ordinalByLocalRow.find(static_cast<std::uint32_t>(localRow));
            if (ordinalFound == ordinalByLocalRow.end()
                || debugIds.xactionType == 0
                || types.find(ordinalFound->second) != types.end()) {
                continue;
            }

            QString xactionType;
            if (!readString(debugIds.xactionType, xactionType)) {
                types.clear();
                return false;
            }
            if (!xactionType.trimmed().isEmpty() && xactionType != QLatin1String("none")) {
                types.emplace(ordinalFound->second, std::move(xactionType));
            }
        }
    }

    {
        const std::lock_guard lock(xactionIndexMutex_);
        if (xactionIndexComplete_) {
            cachedXactionTypesByOrdinal_ = types;
        }
    }
    return true;
}

bool TraceSession::xactionCompletedOrdinals(std::unordered_set<std::uint64_t>& ordinals) const
{
    ordinals.clear();

    QString indexPath;
    std::vector<XactionRowMapChunkDescriptor> rowMapDescriptors;
    std::uint64_t indexedOrdinalCount = 0;
    {
        const std::lock_guard lock(xactionIndexMutex_);
        if (!xactionIndexComplete_ || xactionIndexPath_.isEmpty()) {
            return false;
        }
        if (cachedXactionCompletedOrdinals_) {
            ordinals = *cachedXactionCompletedOrdinals_;
            return true;
        }
        indexPath = xactionIndexPath_;
        rowMapDescriptors = xactionRowMapChunkDescriptors_;
        indexedOrdinalCount = xactionGroupCount_;
    }

    ordinals.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
        indexedOrdinalCount,
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))));
    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }
    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());

    for (const XactionRowMapChunkDescriptor& descriptor : rowMapDescriptors) {
        if (descriptor.storedBytes == 0) {
            continue;
        }
        if (descriptor.dataOffset > fileSize
            || descriptor.storedBytes > fileSize
            || descriptor.dataOffset + descriptor.storedBytes > fileSize) {
            ordinals.clear();
            return false;
        }

        if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
            ordinals.clear();
            return false;
        }
        const QByteArray storedBytes = input.read(static_cast<qint64>(descriptor.storedBytes));
        if (storedBytes.size() != static_cast<int>(descriptor.storedBytes)) {
            ordinals.clear();
            return false;
        }
        const QByteArray payload = (descriptor.flags & kXactionRowChunkCompressed) != 0
            ? qUncompress(storedBytes)
            : storedBytes;
        if (payload.size() != static_cast<int>(descriptor.uncompressedBytes)) {
            ordinals.clear();
            return false;
        }

        QDataStream stream(payload);
        stream.setByteOrder(QDataStream::LittleEndian);
        quint32 entryCount = 0;
        stream >> entryCount;
        if (stream.status() != QDataStream::Ok || entryCount > descriptor.rowCount) {
            ordinals.clear();
            return false;
        }

        for (std::uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
            quint32 localRow = 0;
            quint8 flags = 0;
            quint64 ordinal = 0;
            stream >> localRow
                   >> flags
                   >> ordinal;
            if (stream.status() != QDataStream::Ok || localRow >= descriptor.rowCount) {
                ordinals.clear();
                return false;
            }
            if ((flags & XactionRowIndexed) != 0
                && (flags & XactionRowComplete) != 0
                && ordinal != 0) {
                ordinals.insert(static_cast<std::uint64_t>(ordinal));
            }
        }
    }

    {
        const std::lock_guard lock(xactionIndexMutex_);
        if (xactionIndexComplete_) {
            cachedXactionCompletedOrdinals_ = ordinals;
        }
    }
    return true;
}

bool TraceSession::xactionRowInfoRange(const std::uint64_t beginRow,
                                       const std::uint64_t rowCount,
                                       std::vector<XactionIndexRowInfo>& infos,
                                       const bool includeDebugStrings) const
{
    infos.clear();

    std::vector<CLogBTraceXactionIndexRecord> records;
    if (!readXactionRowRecords(beginRow, rowCount, records, includeDebugStrings)) {
        return false;
    }

    infos.reserve(records.size());
    for (const CLogBTraceXactionIndexRecord& record : records) {
        XactionIndexRowInfo info;
        info.processed = record.processed;
        info.indexed = record.indexed;
        info.processedByJoint = record.processedByJoint;
        info.xactionComplete = record.xactionComplete;
        info.xactionOrdinal = record.xactionOrdinal;
        info.transactionGroupKey = record.transactionGroupKey;
        info.jointType = record.jointType;
        info.jointPath = record.jointPath;
        info.denialName = record.denialName;
        info.denialCode = record.denialCode;
        info.xactionType = record.xactionType;
        infos.push_back(std::move(info));
    }
    return true;
}

bool TraceSession::xactionRowCompactInfoRange(const std::uint64_t beginRow,
                                              const std::uint64_t requestedRowCount,
                                              std::vector<XactionRowCompactInfo>& infos) const
{
    return xactionRowCompactInfoRange(snapshot(), beginRow, requestedRowCount, infos);
}

bool TraceSession::xactionRowCompactInfoRange(
    const std::shared_ptr<const TraceSessionSnapshot>& sessionSnapshot,
    const std::uint64_t beginRow,
    const std::uint64_t requestedRowCount,
    std::vector<XactionRowCompactInfo>& infos) const
{
    infos.clear();
    if (!sessionSnapshot) {
        return false;
    }
    if (beginRow >= sessionSnapshot->metadata.totalRecords || requestedRowCount == 0) {
        return true;
    }

    const std::uint64_t rowCount =
        std::min<std::uint64_t>(requestedRowCount, sessionSnapshot->metadata.totalRecords - beginRow);
    if (rowCount == 0) {
        return true;
    }
    if (rowCount > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        return false;
    }
    infos.resize(static_cast<std::size_t>(rowCount));

    const std::uint64_t endRow = beginRow + rowCount;
    const std::uint64_t firstChunkIndex = beginRow / kXactionRowMapChunkRows;
    const std::uint64_t lastChunkIndex = (endRow - 1) / kXactionRowMapChunkRows;

    if (!sessionSnapshot->xactionIndexComplete
        || sessionSnapshot->xactionIndexPath.isEmpty()
        || lastChunkIndex >= sessionSnapshot->xactionRowMapChunkDescriptors.size()) {
        infos.clear();
        return false;
    }
    std::vector<XactionRowMapChunkDescriptor> descriptors;
    descriptors.insert(descriptors.end(),
                       sessionSnapshot->xactionRowMapChunkDescriptors.begin()
                           + static_cast<std::ptrdiff_t>(firstChunkIndex),
                       sessionSnapshot->xactionRowMapChunkDescriptors.begin()
                           + static_cast<std::ptrdiff_t>(lastChunkIndex + 1));

    QFile input(sessionSnapshot->xactionIndexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        infos.clear();
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    constexpr qsizetype kEntryBytes =
        static_cast<qsizetype>(sizeof(std::uint32_t) + sizeof(std::uint8_t) + sizeof(std::uint64_t));
    for (std::uint64_t chunkIndex = firstChunkIndex; chunkIndex <= lastChunkIndex; ++chunkIndex) {
        const XactionRowMapChunkDescriptor& descriptor =
            descriptors[static_cast<std::size_t>(chunkIndex - firstChunkIndex)];
        if (descriptor.dataOffset > fileSize
            || descriptor.storedBytes > fileSize
            || descriptor.dataOffset + descriptor.storedBytes > fileSize) {
            infos.clear();
            return false;
        }
        if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
            infos.clear();
            return false;
        }

        const QByteArray storedBytes = input.read(static_cast<qint64>(descriptor.storedBytes));
        if (storedBytes.size() != static_cast<int>(descriptor.storedBytes)) {
            infos.clear();
            return false;
        }

        const QByteArray payload = (descriptor.flags & kXactionRowChunkCompressed) != 0
            ? qUncompress(storedBytes)
            : storedBytes;
        if (payload.size() != static_cast<int>(descriptor.uncompressedBytes)
            || payload.size() < static_cast<int>(sizeof(std::uint32_t))) {
            infos.clear();
            return false;
        }

        const char* cursor = payload.constData();
        const char* const payloadEnd = cursor + payload.size();
        const std::uint32_t entryCount = readLe32(cursor);
        cursor += sizeof(std::uint32_t);
        if (entryCount > descriptor.rowCount
            || payloadEnd - cursor < static_cast<std::ptrdiff_t>(entryCount) * kEntryBytes) {
            infos.clear();
            return false;
        }

        const std::uint64_t chunkStartRow = chunkIndex * kXactionRowMapChunkRows;
        const std::uint64_t overlapBegin = std::max(beginRow, chunkStartRow);
        const std::uint64_t overlapEnd =
            std::min<std::uint64_t>(endRow, chunkStartRow + descriptor.rowCount);
        for (std::uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
            const std::uint32_t localRow = readLe32(cursor);
            cursor += sizeof(std::uint32_t);
            const std::uint8_t flags = static_cast<std::uint8_t>(*cursor++);
            const std::uint64_t ordinal = readLe64(cursor);
            cursor += sizeof(std::uint64_t);

            if (localRow >= descriptor.rowCount) {
                infos.clear();
                return false;
            }
            const std::uint64_t logicalRow = chunkStartRow + localRow;
            if (logicalRow < overlapBegin || logicalRow >= overlapEnd) {
                continue;
            }
            XactionRowCompactInfo& info = infos[static_cast<std::size_t>(logicalRow - beginRow)];
            info.xactionOrdinal = ((flags & XactionRowIndexed) != 0) ? ordinal : 0;
            info.flags = flags;
        }
    }

    return true;
}

bool TraceSession::xactionRowInfosForRows(
    const std::vector<std::uint64_t>& logicalRows,
    std::unordered_map<std::uint64_t, XactionIndexRowInfo>& infos,
    const bool includeDebugStrings,
    const std::function<void(std::size_t completedRows,
                             std::size_t totalRows)>& progressCallback) const
{
    infos.clear();
    if (logicalRows.empty()) {
        return true;
    }

    std::vector<std::uint64_t> sortedRows;
    sortedRows.reserve(logicalRows.size());
    for (const std::uint64_t row : logicalRows) {
        if (row < metadata_.totalRecords) {
            sortedRows.push_back(row);
        }
    }
    if (sortedRows.empty()) {
        return true;
    }
    std::sort(sortedRows.begin(), sortedRows.end());
    sortedRows.erase(std::unique(sortedRows.begin(), sortedRows.end()), sortedRows.end());
    const std::size_t totalRequestedRows = sortedRows.size();
    std::size_t completedRequestedRows = 0;

    QString indexPath;
    std::vector<XactionRowMapChunkDescriptor> rowMapDescriptors;
    std::vector<XactionDebugChunkDescriptor> debugDescriptors;
    std::uint64_t stringTableOffset = 0;
    std::uint64_t stringCount = 0;
    {
        const std::lock_guard lock(xactionIndexMutex_);
        if (!xactionIndexComplete_ || xactionIndexPath_.isEmpty()) {
            return false;
        }
        indexPath = xactionIndexPath_;
        rowMapDescriptors = xactionRowMapChunkDescriptors_;
        debugDescriptors = xactionDebugChunkDescriptors_;
        stringTableOffset = xactionStringTableOffset_;
        stringCount = xactionStringCount_;
    }

    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }
    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());

    const auto readString = [&](const std::uint32_t oneBasedIndex, QString& value) -> bool {
        value.clear();
        if (oneBasedIndex == 0) {
            return true;
        }
        if (oneBasedIndex > stringCount) {
            return false;
        }

        XactionStringDescriptor descriptor;
        if (!readXactionStringDescriptor(input,
                                         stringTableOffset,
                                         static_cast<std::uint64_t>(oneBasedIndex - 1),
                                         descriptor)) {
            return false;
        }
        if (descriptor.dataOffset > fileSize
            || descriptor.bytes > fileSize
            || descriptor.dataOffset + descriptor.bytes > fileSize) {
            return false;
        }
        if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
            return false;
        }
        const QByteArray bytes = input.read(static_cast<qint64>(descriptor.bytes));
        if (bytes.size() != static_cast<int>(descriptor.bytes)) {
            return false;
        }
        value = QString::fromUtf8(bytes);
        return true;
    };

    std::unordered_map<std::uint32_t, QString> stringCache;
    const auto resolveDebugStrings = [&](const XactionRowDebugIds& debugIds,
                                         CLogBTraceXactionIndexRecord& record) -> bool {
        const auto resolve = [&](const std::uint32_t stringId, QString& value) -> bool {
            value.clear();
            if (stringId == 0) {
                return true;
            }
            const auto found = stringCache.find(stringId);
            if (found != stringCache.end()) {
                value = found->second;
                return true;
            }
            QString loaded;
            if (!readString(stringId, loaded)) {
                return false;
            }
            const auto inserted = stringCache.emplace(stringId, loaded);
            value = inserted.first->second;
            return true;
        };

        return resolve(debugIds.jointType, record.jointType)
            && resolve(debugIds.jointPath, record.jointPath)
            && resolve(debugIds.denialName, record.denialName)
            && resolve(debugIds.denialCode, record.denialCode)
            && resolve(debugIds.xactionType, record.xactionType);
    };

    std::size_t rowCursor = 0;
    infos.reserve(sortedRows.size());
    while (rowCursor < sortedRows.size()) {
        const std::uint64_t chunkIndex = sortedRows[rowCursor] / kXactionRowMapChunkRows;
        if (chunkIndex >= rowMapDescriptors.size()
            || (includeDebugStrings && chunkIndex >= debugDescriptors.size())) {
            infos.clear();
            return false;
        }

        std::vector<std::uint64_t> rowsInChunk;
        do {
            rowsInChunk.push_back(sortedRows[rowCursor]);
            ++rowCursor;
        } while (rowCursor < sortedRows.size()
                 && sortedRows[rowCursor] / kXactionRowMapChunkRows == chunkIndex);

        const std::uint64_t chunkStartRow = chunkIndex * kXactionRowMapChunkRows;
        std::unordered_map<std::uint32_t, std::size_t> wantedLocalRows;
        wantedLocalRows.reserve(rowsInChunk.size());
        for (std::size_t index = 0; index < rowsInChunk.size(); ++index) {
            wantedLocalRows.emplace(static_cast<std::uint32_t>(rowsInChunk[index] - chunkStartRow), index);
            infos.emplace(rowsInChunk[index], XactionIndexRowInfo{});
        }

        const XactionRowMapChunkDescriptor& rowDescriptor =
            rowMapDescriptors[static_cast<std::size_t>(chunkIndex)];
        if (rowDescriptor.dataOffset > fileSize
            || rowDescriptor.storedBytes > fileSize
            || rowDescriptor.dataOffset + rowDescriptor.storedBytes > fileSize) {
            infos.clear();
            return false;
        }
        if (!input.seek(static_cast<qint64>(rowDescriptor.dataOffset))) {
            infos.clear();
            return false;
        }

        const QByteArray rowStoredBytes = input.read(static_cast<qint64>(rowDescriptor.storedBytes));
        if (rowStoredBytes.size() != static_cast<int>(rowDescriptor.storedBytes)) {
            infos.clear();
            return false;
        }
        QByteArray rowPayload = (rowDescriptor.flags & kXactionRowChunkCompressed) != 0
            ? qUncompress(rowStoredBytes)
            : rowStoredBytes;
        if (rowPayload.size() != static_cast<int>(rowDescriptor.uncompressedBytes)) {
            infos.clear();
            return false;
        }

        QDataStream rowStream(rowPayload);
        rowStream.setByteOrder(QDataStream::LittleEndian);
        quint32 rowEntryCount = 0;
        rowStream >> rowEntryCount;
        if (rowStream.status() != QDataStream::Ok || rowEntryCount > rowDescriptor.rowCount) {
            infos.clear();
            return false;
        }

        std::size_t resolvedRows = 0;
        for (std::uint32_t entryIndex = 0; entryIndex < rowEntryCount; ++entryIndex) {
            quint32 localRow = 0;
            quint8 flags = 0;
            quint64 ordinal = 0;
            rowStream >> localRow
                      >> flags
                      >> ordinal;
            if (rowStream.status() != QDataStream::Ok || localRow >= rowDescriptor.rowCount) {
                infos.clear();
                return false;
            }
            const auto wanted = wantedLocalRows.find(localRow);
            if (wanted == wantedLocalRows.end()) {
                continue;
            }

            CLogBTraceXactionIndexRecord record =
                xactionRecordFromFlags(flags, static_cast<std::uint64_t>(ordinal));
            const std::uint64_t logicalRow = chunkStartRow + localRow;
            XactionIndexRowInfo& info = infos[logicalRow];
            info.processed = record.processed;
            info.indexed = record.indexed;
            info.processedByJoint = record.processedByJoint;
            info.xactionComplete = record.xactionComplete;
            info.xactionOrdinal = record.xactionOrdinal;
            info.transactionGroupKey = record.transactionGroupKey;
            ++resolvedRows;
        }

        if (includeDebugStrings) {
            const XactionDebugChunkDescriptor& debugDescriptor =
                debugDescriptors[static_cast<std::size_t>(chunkIndex)];
            if (debugDescriptor.storedBytes != 0) {
                if (debugDescriptor.dataOffset > fileSize
                    || debugDescriptor.storedBytes > fileSize
                    || debugDescriptor.dataOffset + debugDescriptor.storedBytes > fileSize) {
                    infos.clear();
                    return false;
                }
                if (!input.seek(static_cast<qint64>(debugDescriptor.dataOffset))) {
                    infos.clear();
                    return false;
                }

                const QByteArray debugStoredBytes = input.read(static_cast<qint64>(debugDescriptor.storedBytes));
                if (debugStoredBytes.size() != static_cast<int>(debugDescriptor.storedBytes)) {
                    infos.clear();
                    return false;
                }
                QByteArray debugPayload = (debugDescriptor.flags & kXactionRowChunkCompressed) != 0
                    ? qUncompress(debugStoredBytes)
                    : debugStoredBytes;
                if (debugPayload.size() != static_cast<int>(debugDescriptor.uncompressedBytes)) {
                    infos.clear();
                    return false;
                }

                QDataStream debugStream(debugPayload);
                debugStream.setByteOrder(QDataStream::LittleEndian);
                quint32 debugEntryCount = 0;
                debugStream >> debugEntryCount;
                if (debugStream.status() != QDataStream::Ok
                    || debugEntryCount > debugDescriptor.rowCount) {
                    infos.clear();
                    return false;
                }

                for (std::uint32_t entryIndex = 0; entryIndex < debugEntryCount; ++entryIndex) {
                    quint32 localRow = 0;
                    XactionRowDebugIds debugIds;
                    debugStream >> localRow
                                >> debugIds.jointType
                                >> debugIds.jointPath
                                >> debugIds.denialName
                                >> debugIds.denialCode
                                >> debugIds.xactionType;
                    if (debugStream.status() != QDataStream::Ok
                        || localRow >= debugDescriptor.rowCount) {
                        infos.clear();
                        return false;
                    }
                    if (wantedLocalRows.find(localRow) == wantedLocalRows.end()) {
                        continue;
                    }

                    CLogBTraceXactionIndexRecord debugRecord;
                    if (!resolveDebugStrings(debugIds, debugRecord)) {
                        infos.clear();
                        return false;
                    }
                    const std::uint64_t logicalRow = chunkStartRow + localRow;
                    XactionIndexRowInfo& info = infos[logicalRow];
                    info.jointType = std::move(debugRecord.jointType);
                    info.jointPath = std::move(debugRecord.jointPath);
                    info.denialName = std::move(debugRecord.denialName);
                    info.denialCode = std::move(debugRecord.denialCode);
                    info.xactionType = std::move(debugRecord.xactionType);
                }
            }
        }

        if (resolvedRows < rowsInChunk.size()) {
            for (const std::uint64_t logicalRow : rowsInChunk) {
                const auto found = infos.find(logicalRow);
                if (found == infos.end() || !found->second.indexed) {
                    infos.erase(logicalRow);
                }
            }
        }
        completedRequestedRows += rowsInChunk.size();
        if (progressCallback) {
            progressCallback(completedRequestedRows, totalRequestedRows);
        }
    }

    return true;
}

bool TraceSession::xactionOrdinalsByRowRange(const std::uint64_t beginRow,
                                             const std::uint64_t rowCount,
                                             std::vector<std::uint64_t>& ordinals) const
{
    ordinals.clear();
    if (beginRow >= metadata_.totalRecords || rowCount == 0) {
        return true;
    }

    const std::uint64_t clampedRowCount =
        std::min<std::uint64_t>(rowCount, metadata_.totalRecords - beginRow);
    if (clampedRowCount > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        return false;
    }
    ordinals.reserve(static_cast<std::size_t>(clampedRowCount));

    const std::uint64_t endRow = beginRow + clampedRowCount;
    const std::uint64_t firstChunkIndex = beginRow / kXactionRowMapChunkRows;
    const std::uint64_t lastChunkIndex = (endRow - 1) / kXactionRowMapChunkRows;

    QString indexPath;
    std::vector<XactionRowMapChunkDescriptor> descriptors;
    {
        const std::lock_guard lock(xactionIndexMutex_);
        if (!xactionIndexComplete_
            || xactionIndexPath_.isEmpty()
            || lastChunkIndex >= xactionRowMapChunkDescriptors_.size()) {
            return false;
        }
        indexPath = xactionIndexPath_;
        descriptors.insert(descriptors.end(),
                           xactionRowMapChunkDescriptors_.begin()
                               + static_cast<std::ptrdiff_t>(firstChunkIndex),
                           xactionRowMapChunkDescriptors_.begin()
                               + static_cast<std::ptrdiff_t>(lastChunkIndex + 1));
    }

    for (std::uint64_t chunkIndex = firstChunkIndex; chunkIndex <= lastChunkIndex; ++chunkIndex) {
        std::shared_ptr<const std::vector<std::uint64_t>> cachedOrdinals;
        std::uint32_t cachedRowCount = 0;
        {
            const std::lock_guard lock(xactionIndexMutex_);
            const auto found = cachedXactionOrdinalChunks_.find(chunkIndex);
            if (found != cachedXactionOrdinalChunks_.end()) {
                cachedOrdinals = found->second.ordinals;
                cachedRowCount = found->second.rowCount;
                lruXactionOrdinalChunkIndexes_.erase(found->second.lruIt);
                lruXactionOrdinalChunkIndexes_.push_front(chunkIndex);
                found->second.lruIt = lruXactionOrdinalChunkIndexes_.begin();
            }
        }

        std::vector<std::uint64_t> loadedOrdinals;
        if (!cachedOrdinals) {
            const XactionRowMapChunkDescriptor& descriptor =
                descriptors[static_cast<std::size_t>(chunkIndex - firstChunkIndex)];
            if (!readXactionOrdinalChunk(indexPath, descriptor, loadedOrdinals)) {
                ordinals.clear();
                return false;
            }
            auto sharedOrdinals =
                std::make_shared<std::vector<std::uint64_t>>(std::move(loadedOrdinals));
            {
                const std::lock_guard lock(xactionIndexMutex_);
                const auto found = cachedXactionOrdinalChunks_.find(chunkIndex);
                if (found != cachedXactionOrdinalChunks_.end()) {
                    cachedOrdinals = found->second.ordinals;
                    cachedRowCount = found->second.rowCount;
                    lruXactionOrdinalChunkIndexes_.erase(found->second.lruIt);
                    lruXactionOrdinalChunkIndexes_.push_front(chunkIndex);
                    found->second.lruIt = lruXactionOrdinalChunkIndexes_.begin();
                } else {
                    lruXactionOrdinalChunkIndexes_.push_front(chunkIndex);
                    cachedXactionOrdinalChunks_.emplace(
                        chunkIndex,
                        CachedXactionOrdinalChunkEntry{
                            .ordinals = sharedOrdinals,
                            .rowCount = static_cast<std::uint32_t>(sharedOrdinals->size()),
                            .lruIt = lruXactionOrdinalChunkIndexes_.begin(),
                        });
                    while (cachedXactionOrdinalChunks_.size() > kMaxCachedXactionOrdinalChunks) {
                        const std::uint64_t evictChunk = lruXactionOrdinalChunkIndexes_.back();
                        lruXactionOrdinalChunkIndexes_.pop_back();
                        cachedXactionOrdinalChunks_.erase(evictChunk);
                    }
                    cachedOrdinals = std::move(sharedOrdinals);
                    cachedRowCount = static_cast<std::uint32_t>(cachedOrdinals->size());
                }
            }
        }

        const std::uint64_t chunkStartRow = chunkIndex * kXactionRowMapChunkRows;
        const std::uint64_t overlapBegin = std::max(beginRow, chunkStartRow);
        const std::uint64_t overlapEnd =
            std::min<std::uint64_t>(endRow, chunkStartRow + cachedRowCount);
        if (overlapBegin > overlapEnd
            || overlapEnd - chunkStartRow > cachedOrdinals->size()) {
            ordinals.clear();
            return false;
        }

        const auto first = cachedOrdinals->begin()
            + static_cast<std::ptrdiff_t>(overlapBegin - chunkStartRow);
        const auto last = cachedOrdinals->begin()
            + static_cast<std::ptrdiff_t>(overlapEnd - chunkStartRow);
        ordinals.insert(ordinals.end(), first, last);
    }
    return ordinals.size() == static_cast<std::size_t>(clampedRowCount);
}

bool TraceSession::readXactionOrdinalChunk(const QString& indexPath,
                                           const XactionRowMapChunkDescriptor& descriptor,
                                           std::vector<std::uint64_t>& ordinals) const
{
    ordinals.clear();
    ordinals.resize(descriptor.rowCount);

    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        ordinals.clear();
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    if (descriptor.dataOffset > fileSize
        || descriptor.storedBytes > fileSize
        || descriptor.dataOffset + descriptor.storedBytes > fileSize) {
        ordinals.clear();
        return false;
    }
    if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
        ordinals.clear();
        return false;
    }

    const QByteArray storedBytes = input.read(static_cast<qint64>(descriptor.storedBytes));
    if (storedBytes.size() != static_cast<int>(descriptor.storedBytes)) {
        ordinals.clear();
        return false;
    }

    const QByteArray payload = (descriptor.flags & kXactionRowChunkCompressed) != 0
        ? qUncompress(storedBytes)
        : storedBytes;
    if (payload.size() != static_cast<int>(descriptor.uncompressedBytes)) {
        ordinals.clear();
        return false;
    }

    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 entryCount = 0;
    stream >> entryCount;
    if (stream.status() != QDataStream::Ok || entryCount > descriptor.rowCount) {
        ordinals.clear();
        return false;
    }

    for (std::uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        quint32 localRow = 0;
        quint8 flags = 0;
        quint64 ordinal = 0;
        stream >> localRow
               >> flags
               >> ordinal;
        if (stream.status() != QDataStream::Ok || localRow >= descriptor.rowCount) {
            ordinals.clear();
            return false;
        }
        if ((flags & XactionRowIndexed) != 0) {
            ordinals[static_cast<std::size_t>(localRow)] =
                static_cast<std::uint64_t>(ordinal);
        }
    }
    return true;
}

std::vector<std::uint64_t> TraceSession::xactionRowsForOrdinal(const std::uint64_t ordinal) const
{
    return xactionRowsForOrdinal(snapshot(), ordinal);
}

std::vector<std::uint64_t> TraceSession::xactionRowsForOrdinal(
    const std::shared_ptr<const TraceSessionSnapshot>& sessionSnapshot,
    const std::uint64_t ordinal) const
{
    if (ordinal == 0) {
        return {};
    }

    std::vector<std::uint64_t> rows;
    if (!sessionSnapshot || !readXactionRowsByOrdinal(*sessionSnapshot, ordinal, rows)) {
        return {};
    }
    return rows;
}

std::vector<std::uint64_t> TraceSession::xactionRowsForGroup(const QString& groupKey) const
{
    if (groupKey.isEmpty()) {
        return {};
    }

    static const QString prefix = QStringLiteral("xaction|");
    if (!groupKey.startsWith(prefix)) {
        return {};
    }

    bool ok = false;
    const std::uint64_t ordinal = groupKey.sliced(prefix.size()).toULongLong(&ok);
    if (!ok || ordinal == 0) {
        return {};
    }

    return xactionRowsForOrdinal(ordinal);
}

bool TraceSession::xactionRowInfo(const std::uint64_t logicalRow,
                                  XactionIndexRowInfo& info) const
{
    info = {};
    std::vector<CLogBTraceXactionIndexRecord> records;
    if (!readXactionRowRecords(logicalRow, 1, records, true) || records.empty()) {
        records.clear();
        if (!readXactionRowRecords(logicalRow, 1, records, false) || records.empty()) {
            return false;
        }
    }

    if (records.empty()) {
        return false;
    }

    const CLogBTraceXactionIndexRecord& record = records.front();
    info.processed = record.processed;
    info.indexed = record.indexed;
    info.processedByJoint = record.processedByJoint;
    info.xactionComplete = record.xactionComplete;
    info.xactionOrdinal = record.xactionOrdinal;
    info.transactionGroupKey = record.transactionGroupKey;
    info.jointType = record.jointType;
    info.jointPath = record.jointPath;
    info.denialName = record.denialName;
    info.denialCode = record.denialCode;
    info.xactionType = record.xactionType;
    return true;
}

QString TraceSession::xactionDebugInfo(const std::uint64_t logicalRow) const
{
    CLogBTraceXactionIndexRecord record;
    std::vector<CLogBTraceXactionIndexRecord> records;
    if (!readXactionRowRecords(logicalRow, 1, records, false) || records.empty()) {
        return QStringLiteral("No persisted Xaction index information is available for this row.");
    }

    record = std::move(records.front());
    XactionRowDebugIds debugIds;
    if (!readXactionDebugIds(logicalRow, debugIds)
        || !readXactionDebugStrings(debugIds, record)) {
        return QStringLiteral("No persisted Xaction index information is available for this row.");
    }
    return buildPersistedXactionDebugInfo(logicalRow, record);
}

bool TraceSession::collectRowsMatchingXactionDeniedRange(const std::uint64_t beginRow,
                                                         const std::uint64_t requestedRowCount,
                                                         std::vector<int>& logicalRows) const
{
    logicalRows.clear();
    if (!xactionIndexEnabled_ || requestedRowCount == 0 || beginRow >= metadata_.totalRecords) {
        return false;
    }

    std::vector<CLogBTraceXactionIndexRecord> records;
    if (!readXactionRowRecords(beginRow, requestedRowCount, records, false)) {
        return false;
    }

    logicalRows.reserve(records.size());
    for (std::size_t index = 0; index < records.size(); ++index) {
        const CLogBTraceXactionIndexRecord& record = records[index];
        if (record.indexed) {
            continue;
        }

        const std::uint64_t logicalRow = beginRow + index;
        if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            break;
        }
        logicalRows.push_back(static_cast<int>(logicalRow));
    }
    return true;
}

void TraceSession::markXactionIndexStarted() noexcept
{
    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(xactionIndexMutex_);
        LogTraceSessionLockWait("xactionIndexMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        xactionIndexStarted_ = true;
        xactionIndexComplete_ = false;
        xactionRowTableOffset_ = 0;
        xactionGroupTableOffset_ = 0;
        xactionGroupCount_ = 0;
        xactionStringTableOffset_ = 0;
        xactionStringCount_ = 0;
        xactionRowMapChunkDescriptors_.clear();
        xactionDebugChunkDescriptors_.clear();
        xactionRowsByOrdinalDescriptors_.clear();
        xactionRowsByOrdinalDescriptorsLoaded_ = false;
        cachedXactionTypesByOrdinal_.reset();
        cachedXactionCompletedOrdinals_.reset();
        cachedXactionOrdinalChunks_.clear();
        lruXactionOrdinalChunkIndexes_.clear();
    }
    publishSnapshotNoThrow();
}

void TraceSession::clearXactionIndex()
{
    resetXactionIndexFiles();
    refreshCachedXactionIndexRows();
}

bool TraceSession::buildXactionIndex(CLogBTraceLoadError& error,
                                     const CLogBTraceLoadCallbacks& callbacks,
                                     std::stop_token stopToken)
{
    error = {};
    if (!supportsXactionIndexing()) {
        return true;
    }

    resetXactionIndexFiles();
    markXactionIndexStarted();
    XactionIndexFileWriter writer(xactionIndexPath_, filePath_, metadata_);
    if (!writer.open()) {
        error.type = CLogBTraceLoadError::Type::Generic;
        error.summary = QStringLiteral("Could not create Xaction index file.");
        error.informativeText = xactionIndexPath_;
        resetXactionIndexFileState();
        return false;
    }

    QString writerError;
    const auto recordCallback =
        [&writer](const std::uint64_t logicalRow,
                  CLogBTraceXactionIndexRecord record) {
            return writer.writeRecord(logicalRow, std::move(record));
        };

    if (!CLogBTraceLoader::buildXactionIndex(filePath_,
                                             metadata_,
                                             recordCallback,
                                             error,
                                             callbacks,
                                             stopToken)) {
        writerError = writer.errorString();
        writer.discard();
        resetXactionIndexFileState();
        if (!writerError.isEmpty()) {
            error.summary = QStringLiteral("Could not write Xaction index data: %1").arg(writerError);
            error.detailedText = writerError;
            error.informativeText = writerError;
        }
        return false;
    }

    if (stopToken.stop_requested()) {
        writer.discard();
        resetXactionIndexFileState();
        error.type = CLogBTraceLoadError::Type::Cancelled;
        error.summary = QStringLiteral("Xaction indexing was cancelled.");
        return false;
    }

    const std::size_t finalizingProgressTotal =
        static_cast<std::size_t>(std::min<std::uint64_t>(
            metadata_.totalRecords,
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
    if (callbacks.stage) {
        callbacks.stage(CLogBTraceLoadStage::Finalizing, 0, finalizingProgressTotal);
    }
    if (callbacks.stageProgress) {
        callbacks.stageProgress(0, finalizingProgressTotal);
    }

    if (!writer.finalize(stopToken, callbacks.stage, callbacks.stageProgress)) {
        writer.discard();
        resetXactionIndexFileState();
        if (stopToken.stop_requested()) {
            error.type = CLogBTraceLoadError::Type::Cancelled;
            error.summary = QStringLiteral("Xaction indexing was cancelled.");
            return false;
        }
        error.type = CLogBTraceLoadError::Type::Generic;
        error.summary = QStringLiteral("Could not finalize Xaction index file.");
        error.informativeText = xactionIndexPath_;
        writerError = writer.errorString();
        if (!writerError.isEmpty()) {
            error.detailedText = writerError;
            error.informativeText = writerError;
        }
        return false;
    }

    if (stopToken.stop_requested()) {
        resetXactionIndexFileState();
        error.type = CLogBTraceLoadError::Type::Cancelled;
        error.summary = QStringLiteral("Xaction indexing was cancelled.");
        return false;
    }

    if (!tryLoadXactionIndexState(callbacks, stopToken)) {
        if (stopToken.stop_requested()) {
            error.type = CLogBTraceLoadError::Type::Cancelled;
            error.summary = QStringLiteral("Xaction indexing was cancelled.");
            return false;
        }
        resetXactionIndexFileState();
        error.type = CLogBTraceLoadError::Type::Generic;
        error.summary = QStringLiteral("Could not load finalized Xaction index file.");
        error.informativeText = xactionIndexPath_;
        return false;
    }

    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(xactionIndexMutex_);
        LogTraceSessionLockWait("xactionIndexMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        xactionIndexComplete_ = true;
        xactionIndexStarted_ = true;
    }
    publishSnapshotNoThrow();

    if (callbacks.stageProgress) {
        callbacks.stageProgress(finalizingProgressTotal, finalizingProgressTotal);
    }
    if (callbacks.stage) {
        callbacks.stage(CLogBTraceLoadStage::Completed, 0, finalizingProgressTotal);
    }

    return true;
}

bool TraceSession::tryLoadXactionIndexState(const CLogBTraceLoadCallbacks& callbacks,
                                            std::stop_token stopToken)
{
    if (!xactionIndexEnabled_ || xactionIndexPath_.isEmpty()) {
        return false;
    }

    QFile input(xactionIndexPath_);
    if (!input.open(QIODevice::ReadOnly)) {
        resetXactionIndexFileState();
        return false;
    }

    QDataStream stream(&input);
    stream.setByteOrder(QDataStream::LittleEndian);
    XactionIndexHeader header;
    if (!readXactionHeader(stream, header)) {
        resetXactionIndexFileState();
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    if (!validateXactionIndexHeader(header, filePath_, metadata_, fileSize)) {
        resetXactionIndexFileState();
        return false;
    }

    const auto toProgressCount = [](const std::uint64_t value) {
        return static_cast<std::size_t>(std::min<std::uint64_t>(
            value,
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
    };
    const std::uint64_t layoutProgressTotal =
        std::max<std::uint64_t>(std::min<std::uint64_t>(header.groupCount, 2), 1);
    if (callbacks.stage) {
        callbacks.stage(CLogBTraceLoadStage::FinalizingIndexLayout,
                        0,
                        toProgressCount(layoutProgressTotal));
    }
    if (callbacks.stageProgress) {
        callbacks.stageProgress(0, toProgressCount(layoutProgressTotal));
    }
    const auto reportLayoutProgress = [&](const std::uint64_t completed) {
        if (callbacks.stageProgress) {
            callbacks.stageProgress(toProgressCount(std::min(completed, layoutProgressTotal)),
                                    toProgressCount(layoutProgressTotal));
        }
    };

    constexpr std::uint64_t kValidationProgressBatch = 512;
    if (header.groupCount > 0) {
        if (stopToken.stop_requested()) {
            resetXactionIndexFileState();
            return false;
        }
        XactionRowsDescriptorByOrdinal firstGroup;
        if (!readXactionGroupDescriptorAt(input,
                                          header.groupTableOffset,
                                          0,
                                          firstGroup)
            || !validateXactionRowsDescriptor(firstGroup, fileSize)) {
            resetXactionIndexFileState();
            return false;
        }
        reportLayoutProgress(1);

        if (header.groupCount > 1) {
            if (stopToken.stop_requested()) {
                resetXactionIndexFileState();
                return false;
            }
            XactionRowsDescriptorByOrdinal lastGroup;
            if (!readXactionGroupDescriptorAt(input,
                                              header.groupTableOffset,
                                              header.groupCount - 1,
                                              lastGroup)
                || !validateXactionRowsDescriptor(lastGroup, fileSize)
                || firstGroup.ordinal >= lastGroup.ordinal) {
                resetXactionIndexFileState();
                return false;
            }
            reportLayoutProgress(layoutProgressTotal);
        }
    }
    reportLayoutProgress(layoutProgressTotal);

    // Keep open fast by validating fixed-size directory bounds plus the edge descriptors only.
    // The sorted group table is searched and individual row-list chunks are bounds-checked lazily
    // when a specific xaction is read; callers that need all groups materialize this table on demand.

    std::vector<XactionRowMapChunkDescriptor> rowMapChunkDescriptors;
    rowMapChunkDescriptors.resize(static_cast<std::size_t>(header.rowMapChunkCount));
    std::vector<XactionDebugChunkDescriptor> debugChunkDescriptors;
    debugChunkDescriptors.resize(static_cast<std::size_t>(header.debugChunkCount));
    const std::uint64_t descriptorProgressTotal = std::max<std::uint64_t>(
        header.rowMapChunkCount + header.debugChunkCount + header.stringCount,
        1);
    if (callbacks.stage) {
        callbacks.stage(CLogBTraceLoadStage::FinalizingIndexRowDirectory,
                        0,
                        toProgressCount(descriptorProgressTotal));
    }
    if (callbacks.stageProgress) {
        callbacks.stageProgress(0, toProgressCount(descriptorProgressTotal));
    }
    std::uint64_t completedDescriptors = 0;
    const auto reportDescriptorProgress = [&](const std::uint64_t completed) {
        if (callbacks.stageProgress) {
            callbacks.stageProgress(toProgressCount(std::min(completed, descriptorProgressTotal)),
                                    toProgressCount(descriptorProgressTotal));
        }
    };
    if (!readXactionRowMapChunkDescriptorTable(input,
                                               header.rowTableOffset,
                                               header.rowMapChunkCount,
                                               rowMapChunkDescriptors)) {
        resetXactionIndexFileState();
        return false;
    }

    std::uint64_t validatedRowMapRows = 0;
    for (std::uint64_t chunkIndex = 0; chunkIndex < header.rowMapChunkCount; ++chunkIndex) {
        if (stopToken.stop_requested()) {
            resetXactionIndexFileState();
            return false;
        }
        const XactionRowMapChunkDescriptor& descriptor =
            rowMapChunkDescriptors[static_cast<std::size_t>(chunkIndex)];
        if (descriptor.rowCount == 0
            || descriptor.dataOffset > fileSize
            || descriptor.storedBytes > fileSize
            || descriptor.dataOffset + descriptor.storedBytes > fileSize
            || validatedRowMapRows > (std::numeric_limits<std::uint64_t>::max)() - descriptor.rowCount) {
            resetXactionIndexFileState();
            return false;
        }
        validatedRowMapRows += descriptor.rowCount;
        ++completedDescriptors;
        if ((completedDescriptors % kValidationProgressBatch) == 0
            || completedDescriptors == descriptorProgressTotal) {
            reportDescriptorProgress(completedDescriptors);
        }
    }
    if (validatedRowMapRows != metadata_.totalRecords) {
        resetXactionIndexFileState();
        return false;
    }

    if (!readXactionDebugChunkDescriptorTable(input,
                                              header.debugChunkDescriptorTableOffset,
                                              header.debugChunkCount,
                                              debugChunkDescriptors)) {
        resetXactionIndexFileState();
        return false;
    }

    std::uint64_t validatedDebugRows = 0;
    for (std::uint64_t chunkIndex = 0; chunkIndex < header.debugChunkCount; ++chunkIndex) {
        if (stopToken.stop_requested()) {
            resetXactionIndexFileState();
            return false;
        }
        const XactionDebugChunkDescriptor& descriptor =
            debugChunkDescriptors[static_cast<std::size_t>(chunkIndex)];
        if (descriptor.rowCount == 0
            || descriptor.storedBytes > fileSize
            || (descriptor.storedBytes != 0 && descriptor.dataOffset > fileSize)
            || (descriptor.storedBytes != 0
                && descriptor.dataOffset + descriptor.storedBytes > fileSize)
            || validatedDebugRows > (std::numeric_limits<std::uint64_t>::max)() - descriptor.rowCount) {
            resetXactionIndexFileState();
            return false;
        }
        if (descriptor.storedBytes == 0 && descriptor.uncompressedBytes != 0) {
            resetXactionIndexFileState();
            return false;
        }
        validatedDebugRows += descriptor.rowCount;
        ++completedDescriptors;
        if ((completedDescriptors % kValidationProgressBatch) == 0
            || completedDescriptors == descriptorProgressTotal) {
            reportDescriptorProgress(completedDescriptors);
        }
    }
    if (validatedDebugRows != metadata_.totalRecords) {
        resetXactionIndexFileState();
        return false;
    }

    QByteArray stringDescriptorTable;
    if (!readXactionStringDescriptorTable(input,
                                          header.stringTableOffset,
                                          header.stringCount,
                                          stringDescriptorTable)) {
        resetXactionIndexFileState();
        return false;
    }
    const char* stringDescriptorCursor = stringDescriptorTable.constData();
    for (std::uint64_t stringIndex = 0; stringIndex < header.stringCount; ++stringIndex) {
        if (stopToken.stop_requested()) {
            resetXactionIndexFileState();
            return false;
        }
        XactionStringDescriptor descriptor;
        descriptor.dataOffset = readLe64(stringDescriptorCursor);
        stringDescriptorCursor += sizeof(std::uint64_t);
        descriptor.bytes = readLe32(stringDescriptorCursor);
        stringDescriptorCursor += sizeof(std::uint32_t);
        if (descriptor.dataOffset > fileSize
            || descriptor.bytes > fileSize
            || descriptor.dataOffset + descriptor.bytes > fileSize) {
            resetXactionIndexFileState();
            return false;
        }
        ++completedDescriptors;
        if ((completedDescriptors % kValidationProgressBatch) == 0
            || completedDescriptors == descriptorProgressTotal) {
            reportDescriptorProgress(completedDescriptors);
        }
    }
    reportDescriptorProgress(descriptorProgressTotal);

    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(xactionIndexMutex_);
        LogTraceSessionLockWait("xactionIndexMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        xactionRowTableOffset_ = header.rowTableOffset;
        xactionGroupTableOffset_ = header.groupTableOffset;
        xactionGroupCount_ = header.groupCount;
        xactionStringTableOffset_ = header.stringTableOffset;
        xactionStringCount_ = header.stringCount;
        xactionRowMapChunkDescriptors_ = std::move(rowMapChunkDescriptors);
        xactionDebugChunkDescriptors_ = std::move(debugChunkDescriptors);
        xactionRowsByOrdinalDescriptors_.clear();
        xactionRowsByOrdinalDescriptorsLoaded_ = false;
        cachedXactionTypesByOrdinal_.reset();
        cachedXactionCompletedOrdinals_.reset();
        cachedXactionOrdinalChunks_.clear();
        lruXactionOrdinalChunkIndexes_.clear();
    }
    publishSnapshotNoThrow();
    return true;
}

void TraceSession::resetXactionIndexFiles()
{
    removeXactionIndexFile(xactionIndexPath_);
    removeXactionIndexFile(xactionIndexPath_ + QStringLiteral(".tmp"));
    resetXactionIndexFileState();
}

void TraceSession::resetXactionIndexFileState()
{
    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(xactionIndexMutex_);
        LogTraceSessionLockWait("xactionIndexMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        xactionIndexStarted_ = false;
        xactionIndexComplete_ = false;
        xactionRowTableOffset_ = 0;
        xactionGroupTableOffset_ = 0;
        xactionGroupCount_ = 0;
        xactionStringTableOffset_ = 0;
        xactionStringCount_ = 0;
        xactionRowMapChunkDescriptors_.clear();
        xactionDebugChunkDescriptors_.clear();
        xactionRowsByOrdinalDescriptors_.clear();
        xactionRowsByOrdinalDescriptorsLoaded_ = false;
        cachedXactionTypesByOrdinal_.reset();
        cachedXactionCompletedOrdinals_.reset();
        cachedXactionOrdinalChunks_.clear();
        lruXactionOrdinalChunkIndexes_.clear();
    }
    publishSnapshotNoThrow();
}

bool TraceSession::readXactionRowRecord(const std::uint64_t logicalRow,
                                        CLogBTraceXactionIndexRecord& record) const
{
    record = {};
    std::vector<CLogBTraceXactionIndexRecord> records;
    if (!readXactionRowRecords(logicalRow, 1, records)) {
        return false;
    }
    if (records.empty()) {
        return false;
    }
    record = std::move(records.front());
    return true;
}

bool TraceSession::readXactionRowRecords(
    const std::uint64_t beginRow,
    const std::uint64_t requestedRowCount,
    std::vector<CLogBTraceXactionIndexRecord>& records,
    const bool includeDebugStrings) const
{
    const auto sessionSnapshot = snapshot();
    if (!sessionSnapshot) {
        records.clear();
        return false;
    }
    return readXactionRowRecordsFromSnapshot(*sessionSnapshot,
                                             beginRow,
                                             requestedRowCount,
                                             records,
                                             includeDebugStrings);
}

bool TraceSession::readXactionRowRecordsFromSnapshot(
    const TraceSessionSnapshot& sessionSnapshot,
    const std::uint64_t beginRow,
    const std::uint64_t requestedRowCount,
    std::vector<CLogBTraceXactionIndexRecord>& records,
    const bool includeDebugStrings) const
{
    records.clear();
    if (beginRow >= sessionSnapshot.metadata.totalRecords || requestedRowCount == 0) {
        return true;
    }

    const std::uint64_t rowCount = std::min<std::uint64_t>(
        requestedRowCount,
        sessionSnapshot.metadata.totalRecords - beginRow);
    if (rowCount == 0) {
        return true;
    }

    if (rowCount > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        return false;
    }
    records.resize(static_cast<std::size_t>(rowCount));

    const std::uint64_t endRow = beginRow + rowCount;
    const std::uint64_t firstChunkIndex = beginRow / kXactionRowMapChunkRows;
    const std::uint64_t lastChunkIndex = (endRow - 1) / kXactionRowMapChunkRows;

    if (!sessionSnapshot.xactionIndexComplete
        || sessionSnapshot.xactionIndexPath.isEmpty()
        || lastChunkIndex >= sessionSnapshot.xactionRowMapChunkDescriptors.size()) {
        records.clear();
        return false;
    }
    std::vector<XactionRowMapChunkDescriptor> descriptors;
    descriptors.insert(descriptors.end(),
                       sessionSnapshot.xactionRowMapChunkDescriptors.begin()
                           + static_cast<std::ptrdiff_t>(firstChunkIndex),
                       sessionSnapshot.xactionRowMapChunkDescriptors.begin()
                           + static_cast<std::ptrdiff_t>(lastChunkIndex + 1));

    QFile input(sessionSnapshot.xactionIndexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        records.clear();
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    for (std::uint64_t chunkIndex = firstChunkIndex; chunkIndex <= lastChunkIndex; ++chunkIndex) {
        const XactionRowMapChunkDescriptor& descriptor =
            descriptors[static_cast<std::size_t>(chunkIndex - firstChunkIndex)];
        if (descriptor.dataOffset > fileSize
            || descriptor.storedBytes > fileSize
            || descriptor.dataOffset + descriptor.storedBytes > fileSize) {
            records.clear();
            return false;
        }
        if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
            records.clear();
            return false;
        }

        const QByteArray storedBytes = input.read(static_cast<qint64>(descriptor.storedBytes));
        if (storedBytes.size() != static_cast<int>(descriptor.storedBytes)) {
            records.clear();
            return false;
        }

        QByteArray payload;
        if ((descriptor.flags & kXactionRowChunkCompressed) != 0) {
            payload = qUncompress(storedBytes);
        } else {
            payload = storedBytes;
        }
        if (payload.size() != static_cast<int>(descriptor.uncompressedBytes)) {
            records.clear();
            return false;
        }

        QDataStream stream(payload);
        stream.setByteOrder(QDataStream::LittleEndian);
        quint32 entryCount = 0;
        stream >> entryCount;
        if (stream.status() != QDataStream::Ok || entryCount > descriptor.rowCount) {
            records.clear();
            return false;
        }

        const std::uint64_t chunkStartRow = chunkIndex * kXactionRowMapChunkRows;
        const std::uint64_t overlapBegin = std::max(beginRow, chunkStartRow);
        const std::uint64_t overlapEnd =
            std::min<std::uint64_t>(endRow, chunkStartRow + descriptor.rowCount);
        for (std::uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
            quint32 localRow = 0;
            quint8 flags = 0;
            quint64 ordinal = 0;
            stream >> localRow
                   >> flags
                   >> ordinal;
            if (stream.status() != QDataStream::Ok) {
                records.clear();
                return false;
            }
            if (localRow >= descriptor.rowCount) {
                records.clear();
                return false;
            }

            const std::uint64_t logicalRow = chunkStartRow + localRow;
            if (logicalRow < overlapBegin || logicalRow >= overlapEnd) {
                continue;
            }

            CLogBTraceXactionIndexRecord record = xactionRecordFromFlags(flags, ordinal);
            if (includeDebugStrings) {
                XactionRowDebugIds debugIds;
                if (!readXactionDebugIds(logicalRow, debugIds)
                    || !readXactionDebugStrings(debugIds, record)) {
                    records.clear();
                    return false;
                }
            }
            records[static_cast<std::size_t>(logicalRow - beginRow)] = std::move(record);
        }
    }

    return true;
}

bool TraceSession::readXactionDebugIds(const std::uint64_t logicalRow,
                                       XactionRowDebugIds& debugIds) const
{
    debugIds = {};
    if (logicalRow >= metadata_.totalRecords) {
        return false;
    }

    const std::uint64_t chunkIndex = logicalRow / kXactionRowMapChunkRows;
    QString indexPath;
    XactionDebugChunkDescriptor descriptor;
    {
        const std::lock_guard lock(xactionIndexMutex_);
        if (!xactionIndexComplete_
            || xactionIndexPath_.isEmpty()
            || chunkIndex >= xactionDebugChunkDescriptors_.size()) {
            return false;
        }
        indexPath = xactionIndexPath_;
        descriptor = xactionDebugChunkDescriptors_[static_cast<std::size_t>(chunkIndex)];
    }

    if (descriptor.storedBytes == 0) {
        return true;
    }

    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    if (descriptor.storedBytes > fileSize
        || (descriptor.storedBytes != 0 && descriptor.dataOffset > fileSize)
        || (descriptor.storedBytes != 0
            && descriptor.dataOffset + descriptor.storedBytes > fileSize)) {
        return false;
    }
    if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
        return false;
    }

    const QByteArray storedBytes = input.read(static_cast<qint64>(descriptor.storedBytes));
    if (storedBytes.size() != static_cast<int>(descriptor.storedBytes)) {
        return false;
    }

    QByteArray payload;
    if ((descriptor.flags & kXactionRowChunkCompressed) != 0) {
        payload = qUncompress(storedBytes);
    } else {
        payload = storedBytes;
    }
    if (payload.size() != static_cast<int>(descriptor.uncompressedBytes)) {
        return false;
    }

    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 entryCount = 0;
    stream >> entryCount;
    if (stream.status() != QDataStream::Ok || entryCount > descriptor.rowCount) {
        return false;
    }

    const std::uint32_t targetLocalRow =
        static_cast<std::uint32_t>(logicalRow - chunkIndex * kXactionRowMapChunkRows);
    for (std::uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        quint32 localRow = 0;
        XactionRowDebugIds candidate;
        stream >> localRow
               >> candidate.jointType
               >> candidate.jointPath
               >> candidate.denialName
               >> candidate.denialCode
               >> candidate.xactionType;
        if (stream.status() != QDataStream::Ok || localRow >= descriptor.rowCount) {
            return false;
        }
        if (localRow == targetLocalRow) {
            debugIds = candidate;
            return true;
        }
        if (localRow > targetLocalRow) {
            break;
        }
    }
    return true;
}

bool TraceSession::readXactionRowsByOrdinal(const std::uint64_t ordinal,
                                            std::vector<std::uint64_t>& rows) const
{
    const auto sessionSnapshot = snapshot();
    if (!sessionSnapshot) {
        rows.clear();
        return false;
    }
    return readXactionRowsByOrdinal(*sessionSnapshot, ordinal, rows);
}

bool TraceSession::readXactionRowsByOrdinal(const TraceSessionSnapshot& sessionSnapshot,
                                            const std::uint64_t ordinal,
                                            std::vector<std::uint64_t>& rows) const
{
    rows.clear();

    if (!sessionSnapshot.xactionIndexComplete || sessionSnapshot.xactionIndexPath.isEmpty()) {
        return false;
    }
    QFile input(sessionSnapshot.xactionIndexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    XactionRowsDescriptor descriptor;
    const auto found = findXactionRowsDescriptorByOrdinal(
        sessionSnapshot.xactionRowsByOrdinalDescriptors,
        ordinal);
    if (found != sessionSnapshot.xactionRowsByOrdinalDescriptors.cend()
        && found->ordinal == ordinal) {
        descriptor = found->descriptor;
    } else if (!findXactionRowsDescriptorByOrdinalOnDisk(input,
                                                         sessionSnapshot.xactionGroupTableOffset,
                                                         sessionSnapshot.xactionGroupCount,
                                                         ordinal,
                                                         fileSize,
                                                         descriptor)) {
        return false;
    }

    if (descriptor.rowCount == 0) {
        return false;
    }
    if (descriptor.rowCount > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())
        || descriptor.chunkTableOffset > fileSize
        || descriptor.chunkTableBytes > fileSize
        || descriptor.chunkTableOffset + descriptor.chunkTableBytes > fileSize) {
        return false;
    }

    rows.reserve(static_cast<std::size_t>(descriptor.rowCount));
    for (std::uint64_t chunkIndex = 0; chunkIndex < descriptor.chunkCount; ++chunkIndex) {
        XactionRowChunkDescriptor chunk;
        if (!readXactionRowChunkDescriptor(input,
                                           descriptor.chunkTableOffset,
                                           chunkIndex,
                                           chunk)) {
            rows.clear();
            return false;
        }

        std::uint64_t dataBytes = 0;
        if (chunk.rowCount == 0
            || !checkedMultiply(chunk.rowCount, sizeof(std::uint64_t), dataBytes)
            || chunk.dataOffset > fileSize
            || chunk.storedBytes > fileSize
            || chunk.dataOffset + chunk.storedBytes > fileSize
            || chunk.uncompressedBytes != dataBytes) {
            rows.clear();
            return false;
        }

        if (!input.seek(static_cast<qint64>(chunk.dataOffset))) {
            rows.clear();
            return false;
        }

        const QByteArray storedBytes = input.read(static_cast<qint64>(chunk.storedBytes));
        if (storedBytes.size() != static_cast<int>(chunk.storedBytes)) {
            rows.clear();
            return false;
        }

        QByteArray payload;
        if ((chunk.flags & kXactionRowChunkCompressed) != 0) {
            payload = qUncompress(storedBytes);
        } else {
            payload = storedBytes;
        }
        if (payload.size() != static_cast<int>(chunk.uncompressedBytes)) {
            rows.clear();
            return false;
        }

        QDataStream stream(payload);
        stream.setByteOrder(QDataStream::LittleEndian);
        for (std::uint64_t rowIndex = 0; rowIndex < chunk.rowCount; ++rowIndex) {
            quint64 storedRow = 0;
            stream >> storedRow;
            if (stream.status() != QDataStream::Ok
                || storedRow >= sessionSnapshot.metadata.totalRecords) {
                rows.clear();
                return false;
            }
            rows.push_back(static_cast<std::uint64_t>(storedRow));
        }
    }

    if (rows.size() != static_cast<std::size_t>(descriptor.rowCount)) {
        rows.clear();
        return false;
    }
    return true;
}

bool TraceSession::xactionGroupDescriptors(
    std::vector<XactionRowsDescriptorByOrdinal>& descriptors) const
{
    descriptors.clear();

    QString indexPath;
    std::uint64_t groupTableOffset = 0;
    std::uint64_t groupCount = 0;
    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(xactionIndexMutex_);
        LogTraceSessionLockWait("xactionIndexMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        if (!xactionIndexComplete_ || xactionIndexPath_.isEmpty()) {
            return false;
        }
        if (xactionRowsByOrdinalDescriptorsLoaded_) {
            descriptors = xactionRowsByOrdinalDescriptors_;
            return true;
        }
        indexPath = xactionIndexPath_;
        groupTableOffset = xactionGroupTableOffset_;
        groupCount = xactionGroupCount_;
    }

    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    std::vector<XactionRowsDescriptorByOrdinal> loadedDescriptors;
    if (!readAndValidateXactionGroupDescriptorTable(input,
                                                    groupTableOffset,
                                                    groupCount,
                                                    static_cast<std::uint64_t>(input.size()),
                                                    loadedDescriptors)) {
        return false;
    }

    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(xactionIndexMutex_);
        LogTraceSessionLockWait("xactionIndexMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        if (!xactionIndexComplete_
            || xactionIndexPath_ != indexPath
            || xactionGroupTableOffset_ != groupTableOffset
            || xactionGroupCount_ != groupCount) {
            return false;
        }
        xactionRowsByOrdinalDescriptors_ = std::move(loadedDescriptors);
        xactionRowsByOrdinalDescriptorsLoaded_ = true;
        descriptors = xactionRowsByOrdinalDescriptors_;
    }
    publishSnapshotNoThrow();
    return true;
}

bool TraceSession::readXactionDebugStrings(const XactionRowDebugIds& debugIds,
                                           CLogBTraceXactionIndexRecord& record) const
{
    QString indexPath;
    std::uint64_t stringTableOffset = 0;
    std::uint64_t stringCount = 0;
    {
        const std::lock_guard lock(xactionIndexMutex_);
        indexPath = xactionIndexPath_;
        stringTableOffset = xactionStringTableOffset_;
        stringCount = xactionStringCount_;
    }

    const auto maxStringId = std::max({debugIds.jointType,
                                       debugIds.jointPath,
                                       debugIds.denialName,
                                       debugIds.denialCode,
                                       debugIds.xactionType});
    if (maxStringId == 0) {
        return true;
    }
    if (indexPath.isEmpty() || maxStringId > stringCount) {
        return false;
    }

    QFile input(indexPath);
    if (!input.open(QIODevice::ReadOnly)) {
        return false;
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(input.size());
    const auto readString = [&](const std::uint32_t oneBasedIndex, QString& value) -> bool {
        value.clear();
        if (oneBasedIndex == 0) {
            return true;
        }
        if (oneBasedIndex > stringCount) {
            return false;
        }

        XactionStringDescriptor descriptor;
        if (!readXactionStringDescriptor(input,
                                         stringTableOffset,
                                         static_cast<std::uint64_t>(oneBasedIndex - 1),
                                         descriptor)) {
            return false;
        }
        if (descriptor.dataOffset > fileSize
            || descriptor.bytes > fileSize
            || descriptor.dataOffset + descriptor.bytes > fileSize) {
            return false;
        }
        if (!input.seek(static_cast<qint64>(descriptor.dataOffset))) {
            return false;
        }
        const QByteArray bytes = input.read(static_cast<qint64>(descriptor.bytes));
        if (bytes.size() != static_cast<int>(descriptor.bytes)) {
            return false;
        }
        value = QString::fromUtf8(bytes);
        return true;
    };

    return readString(debugIds.jointType, record.jointType)
        && readString(debugIds.jointPath, record.jointPath)
        && readString(debugIds.denialName, record.denialName)
        && readString(debugIds.denialCode, record.denialCode)
        && readString(debugIds.xactionType, record.xactionType);
}

QString TraceSession::buildPersistedXactionDebugInfo(
    const std::uint64_t logicalRow,
    const CLogBTraceXactionIndexRecord& record) const
{
    QStringList lines;
    lines << QStringLiteral("Persisted Xaction index information");
    lines << QStringLiteral("  Logical row: %1").arg(static_cast<qulonglong>(logicalRow + 1));
    lines << QStringLiteral("  Processed: %1").arg(record.processed ? QStringLiteral("yes")
                                                                     : QStringLiteral("no"));
    lines << QStringLiteral("  Indexed: %1").arg(record.indexed ? QStringLiteral("yes")
                                                                 : QStringLiteral("no"));
    lines << QStringLiteral("  Processed by joint: %1")
                 .arg(record.processedByJoint ? QStringLiteral("yes") : QStringLiteral("no"));
    lines << QStringLiteral("  Complete after flit: %1")
                 .arg(record.xactionComplete ? QStringLiteral("yes") : QStringLiteral("no"));
    lines << QStringLiteral("  Xaction key: %1")
                 .arg(record.transactionGroupKey.isEmpty() ? QStringLiteral("none")
                                                            : record.transactionGroupKey);
    lines << QStringLiteral("  Joint type: %1")
                 .arg(record.jointType.isEmpty() ? QStringLiteral("none") : record.jointType);
    lines << QStringLiteral("  Joint path: %1")
                 .arg(record.jointPath.isEmpty() ? QStringLiteral("none") : record.jointPath);
    lines << QStringLiteral("  Denial: %1 (%2)")
                 .arg(record.denialName.isEmpty() ? QStringLiteral("not processed")
                                                  : record.denialName,
                      record.denialCode.isEmpty() ? QStringLiteral("-") : record.denialCode);
    lines << QStringLiteral("  Xaction type: %1")
                 .arg(record.xactionType.isEmpty() ? QStringLiteral("none") : record.xactionType);
    return lines.join(QLatin1Char('\n'));
}

bool TraceSession::readFastRecords(const std::size_t blockIndex,
                                   const std::size_t localBegin,
                                   const std::size_t rowCount,
                                   std::vector<CLogBTraceFastRecordInfo>& records,
                                   CLogBTraceLoadError& error,
                                   std::stop_token stopToken)
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    records.clear();
    error = {};

    if (blockIndex >= metadata_.blocks.size()) {
        error.summary = QStringLiteral("Requested block index is out of range.");
        return false;
    }

    if (!ensureFastRecordsLoaded(blockIndex, error, stopToken)) {
        return false;
    }

    const std::shared_ptr<const std::vector<CLogBTraceFastRecordInfo>> fastRecords =
        cachedFastRecords(blockIndex);
    if (!fastRecords) {
        error.summary = QStringLiteral("TraceSession fast-record cache lost a requested block.");
        return false;
    }

    if (localBegin >= fastRecords->size() || rowCount == 0) {
        return true;
    }

    const std::size_t localEnd = std::min<std::size_t>(fastRecords->size(), localBegin + rowCount);
    records.insert(records.end(),
                   fastRecords->begin() + static_cast<std::ptrdiff_t>(localBegin),
                   fastRecords->begin() + static_cast<std::ptrdiff_t>(localEnd));
    return true;
}

bool TraceSession::isRowCached(const std::uint64_t row) const noexcept
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    if (row >= metadata_.totalRecords) {
        return false;
    }

    const std::uint64_t pageIndex = row / pageSize_;
    const auto found = cachedPages_.find(pageIndex);
    if (found == cachedPages_.end()) {
        return false;
    }

    const CachedPage& page = found->second.page;
    const std::uint64_t offset = row - page.rowStart;
    return offset < page.rows.size();
}

const FlitRecord* TraceSession::tryGetRow(const std::uint64_t row) const noexcept
{
    const auto lockWaitStart = std::chrono::steady_clock::now();
    const std::lock_guard lock(rowBlockCacheMutex_);
    LogTraceSessionLockWait("rowBlockCacheMutex_",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - lockWaitStart).count());
    if (row >= metadata_.totalRecords) {
        return nullptr;
    }

    const std::uint64_t pageIndex = row / pageSize_;
    const auto found = cachedPages_.find(pageIndex);
    if (found == cachedPages_.end()) {
        return nullptr;
    }

    const CachedPage& page = found->second.page;
    const std::uint64_t offset = row - page.rowStart;
    if (offset >= page.rows.size()) {
        return nullptr;
    }

    return &page.rows[static_cast<std::size_t>(offset)];
}

bool TraceSession::areRowsCached(const std::uint64_t beginRow,
                                 const std::uint64_t rowCount) const noexcept
{
    if (rowCount == 0) {
        return true;
    }

    const std::uint64_t endRow = beginRow + rowCount;
    const std::uint64_t firstPage = beginRow / pageSize_;
    const std::uint64_t lastPage = (endRow - 1) / pageSize_;
    for (std::uint64_t pageIndex = firstPage; pageIndex <= lastPage; ++pageIndex) {
        const auto found = cachedPages_.find(pageIndex);
        if (found == cachedPages_.end()) {
            return false;
        }

        const std::uint64_t pageRowStart = pageIndex * pageSize_;
        const std::uint64_t pageRowEnd = std::min<std::uint64_t>(
            pageRowStart + pageSize_,
            metadata_.totalRecords);
        if (found->second.page.rowStart != pageRowStart
            || found->second.page.rows.size() != static_cast<std::size_t>(pageRowEnd - pageRowStart)) {
            return false;
        }
    }

    return true;
}

bool TraceSession::loadAlignedRangeAndCache(const std::uint64_t beginRow,
                                            const std::uint64_t rowCount,
                                            CLogBTraceLoadError& error,
                                            const CLogBTraceLoadCallbacks& callbacks,
                                            std::stop_token stopToken)
{
    if (rowCount == 0 || beginRow >= metadata_.totalRecords) {
        return true;
    }

    const std::uint64_t endRow = std::min<std::uint64_t>(beginRow + rowCount, metadata_.totalRecords);
    const std::uint64_t alignedBegin = (beginRow / pageSize_) * pageSize_;
    const std::uint64_t alignedEnd = std::min<std::uint64_t>(
        ((endRow + pageSize_ - 1) / pageSize_) * pageSize_,
        metadata_.totalRecords);
    std::vector<FlitRecord> alignedRows;
    alignedRows.reserve(static_cast<std::size_t>(alignedEnd - alignedBegin));

    for (std::size_t blockIndex = 0; blockIndex < metadata_.blocks.size(); ++blockIndex) {
        const CLogBTraceBlockSummary& blockSummary = metadata_.blocks[blockIndex];
        const std::uint64_t blockEnd = blockSummary.rowStart + blockSummary.recordCount;
        if (blockEnd <= alignedBegin) {
            continue;
        }
        if (blockSummary.rowStart >= alignedEnd) {
            break;
        }

        if (!ensureBlockLoaded(blockIndex, error, callbacks, stopToken)) {
            return false;
        }

        const std::shared_ptr<CLog::CLogB::TagCHIRecords> block = cachedBlock(blockIndex);
        if (!block) {
            error.summary = QStringLiteral("TraceSession block cache lost a requested CHI record block.");
            return false;
        }

        const std::uint64_t overlapBegin = std::max(alignedBegin, blockSummary.rowStart);
        const std::uint64_t overlapEnd = std::min(alignedEnd, blockEnd);
        std::vector<FlitRecord> blockRows;
        if (!CLogBTraceLoader::decodeBlockRows(metadata_,
                                              blockIndex,
                                              *block,
                                              static_cast<std::size_t>(overlapBegin - blockSummary.rowStart),
                                              static_cast<std::size_t>(overlapEnd - overlapBegin),
                                              blockRows,
                                              error,
                                              callbacks,
                                              stopToken)) {
            return false;
        }

        applyXactionIndexToRows(overlapBegin, blockRows);
        alignedRows.insert(alignedRows.end(),
                           std::make_move_iterator(blockRows.begin()),
                           std::make_move_iterator(blockRows.end()));
    }

    cacheRows(alignedBegin, std::move(alignedRows));
    return true;
}

bool TraceSession::ensureBlockLoaded(const std::uint64_t blockIndex,
                                     CLogBTraceLoadError& error,
                                     const CLogBTraceLoadCallbacks& callbacks,
                                     std::stop_token stopToken)
{
    if (const std::shared_ptr<CLog::CLogB::TagCHIRecords> block = cachedBlock(blockIndex)) {
        touchBlock(blockIndex);
        return true;
    }

    std::shared_ptr<CLog::CLogB::TagCHIRecords> block;
    if (!CLogBTraceLoader::loadBlockRecords(filePath_,
                                            metadata_,
                                            static_cast<std::size_t>(blockIndex),
                                            block,
                                            error,
                                            callbacks,
                                            stopToken)) {
        return false;
    }

    storeBlock(blockIndex, std::move(block));
    return true;
}

std::shared_ptr<CLog::CLogB::TagCHIRecords> TraceSession::cachedBlock(const std::uint64_t blockIndex) const noexcept
{
    const auto found = cachedBlocks_.find(blockIndex);
    return found == cachedBlocks_.end() ? nullptr : found->second.block;
}

bool TraceSession::ensureFastRecordsLoaded(const std::uint64_t blockIndex,
                                           CLogBTraceLoadError& error,
                                           std::stop_token stopToken)
{
    if (!ensureBlockLoaded(blockIndex, error, {}, stopToken)) {
        return false;
    }

    const auto found = cachedBlocks_.find(blockIndex);
    if (found == cachedBlocks_.end()) {
        error.summary = QStringLiteral("TraceSession block cache lost a requested CHI record block.");
        return false;
    }

    if (found->second.fastRecords) {
        touchBlock(blockIndex);
        return true;
    }

    std::vector<CLogBTraceFastRecordInfo> fastRecords;
    if (!tryLoadFastRecordsFromIndex(blockIndex, fastRecords)) {
        if (!CLogBTraceLoader::buildBlockFastRecords(metadata_,
                                                     static_cast<std::size_t>(blockIndex),
                                                     *found->second.block,
                                                     fastRecords,
                                                     error,
                                                     stopToken)) {
            return false;
        }
        tryPersistFastRecordsToIndex(blockIndex, fastRecords);
    }

    found->second.fastRecords =
        std::make_shared<std::vector<CLogBTraceFastRecordInfo>>(std::move(fastRecords));
    touchBlock(blockIndex);
    return true;
}

std::shared_ptr<const std::vector<CLogBTraceFastRecordInfo>>
TraceSession::cachedFastRecords(const std::uint64_t blockIndex) const noexcept
{
    const auto found = cachedBlocks_.find(blockIndex);
    return (found == cachedBlocks_.end() || !found->second.fastRecords)
        ? nullptr
        : found->second.fastRecords;
}

void TraceSession::initializeFastIndexState() noexcept
{
    if (!fastIndexEnabled_) {
        return;
    }

    fastIndexPath_ = fastIndexPathForTrace(filePath_);
    if (fastIndexPath_.isEmpty()) {
        return;
    }

    if (loadFastIndexDescriptors(fastIndexPath_,
                                 filePath_,
                                 metadata_.blocks.size(),
                                 fastIndexDescriptors_)
        && Detail::validateFastIndexDescriptors(fastIndexPath_, metadata_, fastIndexDescriptors_)) {
        fastIndexReadable_ = true;
        fastIndexWritable_ = true;
        return;
    }

    fastIndexDescriptors_.clear();
    if (initializeFastIndexFile(fastIndexPath_,
                                filePath_,
                                metadata_.blocks.size(),
                                fastIndexDescriptors_)) {
        fastIndexReadable_ = true;
        fastIndexWritable_ = true;
    }
}

void TraceSession::initializeXactionIndexState(const CLogBTraceLoadCallbacks& callbacks,
                                               std::stop_token stopToken)
{
    if (metadata_.parameters.GetIssue() != CLog::Issue::Eb) {
        return;
    }

    xactionIndexEnabled_ = true;
    xactionIndexPath_ = xactionIndexPathForTrace(filePath_);
    if (tryLoadXactionIndexState(callbacks, stopToken)) {
        const std::lock_guard lock(xactionIndexMutex_);
        xactionIndexStarted_ = true;
        xactionIndexComplete_ = true;
    }
}

void TraceSession::applyXactionIndexToRows(const std::uint64_t beginRow,
                                           std::vector<FlitRecord>& rows) const
{
    const auto sessionSnapshot = snapshot();
    if (!sessionSnapshot) {
        return;
    }
    applyXactionIndexToRows(*sessionSnapshot, beginRow, rows);
}

void TraceSession::applyXactionIndexToRows(const TraceSessionSnapshot& sessionSnapshot,
                                           const std::uint64_t beginRow,
                                           std::vector<FlitRecord>& rows) const
{
    if (!sessionSnapshot.xactionIndexEnabled || rows.empty()) {
        return;
    }

    const bool started = sessionSnapshot.xactionIndexStarted;
    const bool complete = sessionSnapshot.xactionIndexComplete;

    if (!started) {
        for (FlitRecord& row : rows) {
            row.xactionIndexChecked = false;
            row.xactionIndexed = false;
            row.xactionIndexProcessedByJoint = false;
            row.xactionIndexState = XactionIndexState::NotStarted;
            row.xactionDebugLog.clear();
        }
        return;
    }

    std::vector<CLogBTraceXactionIndexRecord> completeRecords;
    const bool haveCompleteRecords = complete
        && readXactionRowRecordsFromSnapshot(sessionSnapshot,
                                             beginRow,
                                             static_cast<std::uint64_t>(rows.size()),
                                             completeRecords,
                                             false);

    for (std::size_t index = 0; index < rows.size(); ++index) {
        FlitRecord& row = rows[index];

        CLogBTraceXactionIndexRecord indexRecord;
        if (complete) {
            if (haveCompleteRecords && index < completeRecords.size()) {
                indexRecord = completeRecords[index];
            } else {
                row.xactionIndexChecked = true;
                row.xactionIndexed = false;
                row.xactionIndexProcessedByJoint = false;
                row.xactionIndexState = XactionIndexState::Denied;
                row.transactionGroupKey.clear();
                row.xactionDebugLog.clear();
                continue;
            }
        } else {
            indexRecord.processed = false;
            indexRecord.indexed = false;
            indexRecord.processedByJoint = false;
        }

        row.xactionIndexChecked = true;
        row.xactionIndexed = indexRecord.indexed;
        row.xactionIndexProcessedByJoint = indexRecord.processedByJoint;
        row.xactionIndexState = indexRecord.indexed
            ? XactionIndexState::Indexed
            : (indexRecord.processed || complete
                   ? XactionIndexState::Denied
                   : XactionIndexState::Pending);
        row.transactionGroupKey = indexRecord.indexed ? indexRecord.transactionGroupKey
                                                      : QString();
        row.xactionDebugLog.clear();
    }
}

void TraceSession::publishSnapshot() const
{
    auto next = std::make_shared<TraceSessionSnapshot>();
    next->filePath = filePath_;
    next->metadata = metadata_;
    next->pageSize = pageSize_;
    next->maxCachedPages = maxCachedPages_;
    next->maxCachedBlocks = maxCachedBlocks_;
    next->fastIndexPath = fastIndexPath_;
    next->fastIndexReadable = fastIndexReadable_;
    next->fastIndexWritable = fastIndexWritable_;
    next->fastIndexDescriptors = fastIndexDescriptors_;
    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(xactionIndexMutex_);
        LogTraceSessionLockWait("xactionIndexMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        next->xactionIndexEnabled = xactionIndexEnabled_;
        next->xactionIndexStarted = xactionIndexStarted_;
        next->xactionIndexComplete = xactionIndexComplete_;
        next->xactionIndexPath = xactionIndexPath_;
        next->xactionRowTableOffset = xactionRowTableOffset_;
        next->xactionGroupTableOffset = xactionGroupTableOffset_;
        next->xactionGroupCount = xactionGroupCount_;
        next->xactionStringTableOffset = xactionStringTableOffset_;
        next->xactionStringCount = xactionStringCount_;
        next->xactionRowMapChunkDescriptors = xactionRowMapChunkDescriptors_;
        next->xactionDebugChunkDescriptors = xactionDebugChunkDescriptors_;
        if (xactionRowsByOrdinalDescriptorsLoaded_) {
            next->xactionRowsByOrdinalDescriptors = xactionRowsByOrdinalDescriptors_;
        }
    }
    {
        const auto lockWaitStart = std::chrono::steady_clock::now();
        const std::lock_guard lock(rowBlockCacheMutex_);
        LogTraceSessionLockWait("rowBlockCacheMutex_",
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - lockWaitStart).count());
        next->cachedFastRecordsByBlock.reserve(cachedBlocks_.size());
        for (const auto& [blockIndex, entry] : cachedBlocks_) {
            if (entry.fastRecords) {
                next->cachedFastRecordsByBlock.emplace(blockIndex, entry.fastRecords);
            }
        }
    }
    std::atomic_store_explicit(&snapshot_,
                               std::shared_ptr<const TraceSessionSnapshot>(std::move(next)),
                               std::memory_order_release);
}

void TraceSession::publishSnapshotNoThrow() const noexcept
{
    try {
        publishSnapshot();
    } catch (const std::exception& exception) {
        qCWarning(traceSessionLog).noquote()
            << QStringLiteral("Could not publish TraceSession snapshot: %1")
                   .arg(QString::fromLocal8Bit(exception.what()));
    } catch (...) {
        qCWarning(traceSessionLog)
            << "Could not publish TraceSession snapshot due to an unknown error.";
    }
}

bool TraceSession::ensureOptionalFieldIndexState(const QString& fieldName) noexcept
{
    if (!fastIndexEnabled_ || fieldName.isEmpty()) {
        return false;
    }

    {
        const std::lock_guard lock(optionalFieldIndexesMutex_);
        const auto found = optionalFieldIndexes_.constFind(fieldName);
        if (found != optionalFieldIndexes_.cend()) {
            return found->readable;
        }
    }

    OptionalFieldIndexState state;
    if (!tryLoadOptionalFieldIndexState(fieldName, state)) {
        return false;
    }

    state.readable = true;
    state.writable = true;
    {
        const std::lock_guard lock(optionalFieldIndexesMutex_);
        const auto found = optionalFieldIndexes_.constFind(fieldName);
        if (found != optionalFieldIndexes_.cend()) {
            return found->readable;
        }
        optionalFieldIndexes_.insert(fieldName, std::move(state));
    }
    return true;
}

bool TraceSession::initializeOptionalFieldIndexFile(const QString& fieldName,
                                                    OptionalFieldIndexState& state) noexcept
{
    state = {};
    const QString indexPath = optionalFieldFastIndexPath(fieldName);
    if (indexPath.isEmpty()) {
        return false;
    }

    return createOptionalFieldIndexFile(indexPath,
                                        filePath_,
                                        fieldName,
                                        metadata_.blocks.size(),
                                        state);
}

bool TraceSession::tryLoadOptionalFieldIndexState(const QString& fieldName,
                                                  OptionalFieldIndexState& state) noexcept
{
    state = {};
    const QString indexPath = optionalFieldFastIndexPath(fieldName);
    if (indexPath.isEmpty()) {
        return false;
    }

    if (!loadOptionalFieldIndexDescriptors(indexPath,
                                           filePath_,
                                           fieldName,
                                           metadata_.blocks.size(),
                                           state)) {
        return false;
    }

    if (!validateOptionalFieldIndexDescriptors(state, metadata_)) {
        state = {};
        return false;
    }

    return true;
}

bool TraceSession::tryLoadFastRecordsFromIndex(const std::uint64_t blockIndex,
                                               std::vector<CLogBTraceFastRecordInfo>& records) noexcept
{
    if (!fastIndexReadable_ || blockIndex >= fastIndexDescriptors_.size()) {
        return false;
    }

    return readFastRecordsFromIndexFile(fastIndexPath_, fastIndexDescriptors_[blockIndex], records);
}

void TraceSession::tryPersistFastRecordsToIndex(const std::uint64_t blockIndex,
                                                const std::vector<CLogBTraceFastRecordInfo>& records) noexcept
{
    if (!fastIndexWritable_ || blockIndex >= fastIndexDescriptors_.size()) {
        return;
    }

    if (!writeFastRecords(fastIndexPath_, static_cast<std::size_t>(blockIndex), fastIndexDescriptors_, records)) {
        fastIndexWritable_ = false;
        fastIndexReadable_ = false;
        publishSnapshotNoThrow();
    }
}

bool TraceSession::ensureOptionalFieldRecordsLoaded(const std::uint64_t blockIndex,
                                                    const QString& fieldName,
                                                    const OptionalFieldIndexState& state,
                                                    CLogBTraceLoadError& error)
{
    const QString cacheKey = optionalFieldBlockCacheKey(fieldName, blockIndex);
    if (cachedOptionalFieldRecords(blockIndex, fieldName)) {
        touchOptionalFieldBlock(cacheKey);
        return true;
    }

    if (!state.readable || blockIndex >= state.descriptors.size() || blockIndex >= metadata_.blocks.size()) {
        error.summary = QStringLiteral("Fast index for optional field %1 is not readable.").arg(fieldName);
        return false;
    }

    const FastIndexDescriptor& descriptor = state.descriptors[static_cast<std::size_t>(blockIndex)];
    std::vector<OptionalFieldIndexRecord> diskRecords;
    if (!readOptionalFieldRecordsFromIndexFile(state.path, descriptor, diskRecords)) {
        error.summary = QStringLiteral("Fast index for optional field %1 could not be read.").arg(fieldName);
        error.informativeText = state.path;
        return false;
    }

    if (diskRecords.size() != metadata_.blocks[static_cast<std::size_t>(blockIndex)].recordCount) {
        error.summary = QStringLiteral("Fast index for optional field %1 has an invalid record count.").arg(fieldName);
        return false;
    }

    auto records = std::make_shared<std::vector<CachedOptionalFieldIndexRecord>>();
    records->reserve(diskRecords.size());
    for (OptionalFieldIndexRecord& diskRecord : diskRecords) {
        CachedOptionalFieldIndexRecord cachedRecord;
        cachedRecord.present = diskRecord.present;
        cachedRecord.value = std::move(diskRecord.value);
        cachedRecord.raw = std::move(diskRecord.raw);
        if (cachedRecord.present) {
            const QString rawToken = cachedRecord.raw.section(QChar(' '), 0, 0, QString::SectionSkipEmpty);
            cachedRecord.numeric = parseSearchInteger(cachedRecord.value, cachedRecord.number)
                || parseSearchInteger(rawToken, cachedRecord.number);
        }
        records->push_back(std::move(cachedRecord));
    }

    const std::uint64_t approximateBytes =
        descriptor.dataBytes
        + static_cast<std::uint64_t>(records->size() * sizeof(CachedOptionalFieldIndexRecord));
    storeOptionalFieldRecords(blockIndex, fieldName, std::move(records), approximateBytes);
    return true;
}

std::shared_ptr<const std::vector<TraceSession::CachedOptionalFieldIndexRecord>>
TraceSession::cachedOptionalFieldRecords(const std::uint64_t blockIndex,
                                         const QString& fieldName) const noexcept
{
    const QString cacheKey = optionalFieldBlockCacheKey(fieldName, blockIndex);
    const std::lock_guard lock(optionalFieldRecordCacheMutex_);
    const auto found = cachedOptionalFieldBlocks_.constFind(cacheKey);
    return found == cachedOptionalFieldBlocks_.cend() ? nullptr : found->records;
}

void TraceSession::storeOptionalFieldRecords(
    const std::uint64_t blockIndex,
    const QString& fieldName,
    std::shared_ptr<std::vector<TraceSession::CachedOptionalFieldIndexRecord>> records,
    const std::uint64_t approximateBytes)
{
    const QString cacheKey = optionalFieldBlockCacheKey(fieldName, blockIndex);
    const std::lock_guard lock(optionalFieldRecordCacheMutex_);
    const auto found = cachedOptionalFieldBlocks_.find(cacheKey);
    if (found != cachedOptionalFieldBlocks_.end()) {
        cachedOptionalFieldBlockBytes_ -= found->approximateBytes;
        lruOptionalFieldBlockKeys_.erase(found->lruIt);
        cachedOptionalFieldBlocks_.erase(found);
    }

    lruOptionalFieldBlockKeys_.push_front(cacheKey);
    cachedOptionalFieldBlockBytes_ += approximateBytes;
    cachedOptionalFieldBlocks_.insert(cacheKey,
                                      CachedOptionalFieldBlockEntry{
                                          .records = std::move(records),
                                          .approximateBytes = approximateBytes,
                                          .lruIt = lruOptionalFieldBlockKeys_.begin(),
                                      });
    evictOptionalFieldBlocksIfNeeded();
}

void TraceSession::touchOptionalFieldBlock(const QString& cacheKey) noexcept
{
    const std::lock_guard lock(optionalFieldRecordCacheMutex_);
    const auto found = cachedOptionalFieldBlocks_.find(cacheKey);
    if (found == cachedOptionalFieldBlocks_.end()
        || found->lruIt == lruOptionalFieldBlockKeys_.begin()) {
        return;
    }

    lruOptionalFieldBlockKeys_.splice(lruOptionalFieldBlockKeys_.begin(),
                                      lruOptionalFieldBlockKeys_,
                                      found->lruIt);
    found->lruIt = lruOptionalFieldBlockKeys_.begin();
}

void TraceSession::clearOptionalFieldRecordCache(const QString& fieldName)
{
    const std::lock_guard lock(optionalFieldRecordCacheMutex_);
    if (fieldName.isEmpty()) {
        cachedOptionalFieldBlocks_.clear();
        lruOptionalFieldBlockKeys_.clear();
        cachedOptionalFieldBlockBytes_ = 0;
        return;
    }

    const QString prefix = fieldName + QChar(0x1F);
    const QList<QString> keys = cachedOptionalFieldBlocks_.keys();
    for (const QString& key : keys) {
        if (!key.startsWith(prefix)) {
            continue;
        }

        const auto found = cachedOptionalFieldBlocks_.find(key);
        if (found == cachedOptionalFieldBlocks_.end()) {
            continue;
        }

        cachedOptionalFieldBlockBytes_ -= found->approximateBytes;
        lruOptionalFieldBlockKeys_.erase(found->lruIt);
        cachedOptionalFieldBlocks_.erase(found);
    }
}

bool TraceSession::ensurePageLoaded(const std::uint64_t pageIndex,
                                    CLogBTraceLoadError& error,
                                    const CLogBTraceLoadCallbacks& callbacks,
                                    std::stop_token stopToken)
{
    const auto found = cachedPages_.find(pageIndex);
    const std::uint64_t rowStart = pageIndex * pageSize_;
    if (rowStart >= metadata_.totalRecords) {
        return true;
    }

    if (found != cachedPages_.end()) {
        const std::uint64_t expectedRowCount = std::min<std::uint64_t>(
            pageSize_,
            metadata_.totalRecords - rowStart);
        if (found->second.page.rowStart == rowStart
            && found->second.page.rows.size() == static_cast<std::size_t>(expectedRowCount)) {
            touchPage(pageIndex);
            return true;
        }
    }

    const std::uint64_t rowCount = std::min<std::uint64_t>(
        pageSize_,
        metadata_.totalRecords - rowStart);
    return loadAlignedRangeAndCache(rowStart, rowCount, error, callbacks, stopToken);
}

void TraceSession::cacheRows(const std::uint64_t beginRow, std::vector<FlitRecord> rows)
{
    if (rows.empty()) {
        return;
    }

    std::size_t offset = 0;
    std::uint64_t rowStart = beginRow;
    while (offset < rows.size()) {
        const std::uint64_t pageIndex = rowStart / pageSize_;
        const std::uint64_t pageRowStart = pageIndex * pageSize_;
        const std::size_t pageOffset = static_cast<std::size_t>(rowStart - pageRowStart);
        const std::size_t pageRowCount = static_cast<std::size_t>(std::min<std::uint64_t>(
            pageSize_,
            metadata_.totalRecords - pageRowStart));
        const std::size_t sliceCount = std::min<std::size_t>(
            pageRowCount - pageOffset,
            rows.size() - offset);

        std::vector<FlitRecord> pageRows;
        pageRows.reserve(sliceCount);
        std::move(rows.begin() + static_cast<std::ptrdiff_t>(offset),
                  rows.begin() + static_cast<std::ptrdiff_t>(offset + sliceCount),
                  std::back_inserter(pageRows));
        storePage(CachedPage{
            .pageIndex = pageIndex,
            .rowStart = rowStart,
            .rows = std::move(pageRows),
        });

        rowStart += sliceCount;
        offset += sliceCount;
    }
}

void TraceSession::storePage(CachedPage page)
{
    const auto found = cachedPages_.find(page.pageIndex);
    if (found != cachedPages_.end()) {
        lruPageIndexes_.erase(found->second.lruIt);
        cachedPages_.erase(found);
    }

    lruPageIndexes_.push_front(page.pageIndex);
    cachedPages_.emplace(page.pageIndex,
                         CachedPageEntry{
                             .page = std::move(page),
                             .lruIt = lruPageIndexes_.begin(),
                         });
    evictPagesIfNeeded();
}

void TraceSession::storeBlock(const std::uint64_t blockIndex,
                              std::shared_ptr<CLog::CLogB::TagCHIRecords> block)
{
    const auto found = cachedBlocks_.find(blockIndex);
    if (found != cachedBlocks_.end()) {
        lruBlockIndexes_.erase(found->second.lruIt);
        cachedBlocks_.erase(found);
    }

    lruBlockIndexes_.push_front(blockIndex);
    cachedBlocks_.emplace(blockIndex,
                          CachedBlockEntry{
                              .block = std::move(block),
                              .fastRecords = nullptr,
                              .lruIt = lruBlockIndexes_.begin(),
                          });
    evictBlocksIfNeeded();
}

void TraceSession::touchPage(const std::uint64_t pageIndex) noexcept
{
    const auto found = cachedPages_.find(pageIndex);
    if (found == cachedPages_.end() || found->second.lruIt == lruPageIndexes_.begin()) {
        return;
    }

    lruPageIndexes_.splice(lruPageIndexes_.begin(), lruPageIndexes_, found->second.lruIt);
    found->second.lruIt = lruPageIndexes_.begin();
}

void TraceSession::touchBlock(const std::uint64_t blockIndex) noexcept
{
    const auto found = cachedBlocks_.find(blockIndex);
    if (found == cachedBlocks_.end() || found->second.lruIt == lruBlockIndexes_.begin()) {
        return;
    }

    lruBlockIndexes_.splice(lruBlockIndexes_.begin(), lruBlockIndexes_, found->second.lruIt);
    found->second.lruIt = lruBlockIndexes_.begin();
}

void TraceSession::evictPagesIfNeeded()
{
    while (cachedPages_.size() > maxCachedPages_ && !lruPageIndexes_.empty()) {
        const std::uint64_t pageIndex = lruPageIndexes_.back();
        lruPageIndexes_.pop_back();
        cachedPages_.erase(pageIndex);
    }
}

void TraceSession::evictBlocksIfNeeded()
{
    while (cachedBlocks_.size() > maxCachedBlocks_ && !lruBlockIndexes_.empty()) {
        const std::uint64_t blockIndex = lruBlockIndexes_.back();
        lruBlockIndexes_.pop_back();
        cachedBlocks_.erase(blockIndex);
    }
}

void TraceSession::evictOptionalFieldBlocksIfNeeded()
{
    while (cachedOptionalFieldBlockBytes_ > maxCachedOptionalFieldBlockBytes_
           && cachedOptionalFieldBlocks_.size() > 1
           && !lruOptionalFieldBlockKeys_.empty()) {
        const QString cacheKey = lruOptionalFieldBlockKeys_.back();
        lruOptionalFieldBlockKeys_.pop_back();
        const auto found = cachedOptionalFieldBlocks_.find(cacheKey);
        if (found == cachedOptionalFieldBlocks_.end()) {
            continue;
        }

        cachedOptionalFieldBlockBytes_ -= found->approximateBytes;
        cachedOptionalFieldBlocks_.erase(found);
    }
}

}  // namespace CHIron::Gui
