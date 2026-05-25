#include "cache_widget.hpp"
#include "main_window_internal.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QSet>
#include <QSpinBox>
#include <QWizard>
#include <QWizardPage>

#include <chrono>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace CHIron::Gui {
using namespace MainWindowDetail;

namespace {

struct FlitTemplateChoice {
    QString label;
    FlitChannel channel = FlitChannel::Req;
    FlitDirection direction = FlitDirection::Tx;
};

struct XactionAddressAnchor {
    std::uint64_t ordinal = 0;
    std::uint64_t requestLogicalRow = 0;
    std::uint64_t lineAddress = 0;
};

struct ClipboardXactionAddressSlice {
    std::size_t blockIndex = 0;
    std::uint64_t beginRow = 0;
    std::uint64_t rowCount = 0;
    std::size_t localBegin = 0;
};

struct ClipboardXactionAddressLogicalSlice {
    std::uint64_t beginRow = 0;
    std::uint64_t rowCount = 0;
};

struct ClipboardXactionAddressRowRef {
    std::uint64_t logicalRow = 0;
    std::uint64_t ordinal = 0;
    std::uint8_t compactFlags = 0;
};

struct ClipboardXactionAddressScanResult {
    std::unordered_map<std::uint64_t, XactionAddressAnchor> anchorsByOrdinal;
    std::unordered_set<std::uint64_t> indexedOrdinals;
    std::uint64_t scannedRows = 0;
};

struct ClipboardXactionAddressBatchRows {
    std::vector<std::pair<int, FlitRecord>> rows;
    int preSkippedDuplicates = 0;
};

constexpr std::uint8_t kClipboardXactionRowProcessed = 1U << 0;
constexpr std::uint8_t kClipboardXactionRowIndexed = 1U << 1;
constexpr std::uint8_t kClipboardXactionRowProcessedByJoint = 1U << 2;

bool ParametersEqual(const CLog::Parameters& lhs, const CLog::Parameters& rhs)
{
    return lhs.GetIssue() == rhs.GetIssue()
        && lhs.GetNodeIdWidth() == rhs.GetNodeIdWidth()
        && lhs.GetReqAddrWidth() == rhs.GetReqAddrWidth()
        && lhs.GetReqRSVDCWidth() == rhs.GetReqRSVDCWidth()
        && lhs.GetDatRSVDCWidth() == rhs.GetDatRSVDCWidth()
        && lhs.GetDataWidth() == rhs.GetDataWidth()
        && lhs.IsDataCheckPresent() == rhs.IsDataCheckPresent()
        && lhs.IsPoisonPresent() == rhs.IsPoisonPresent()
        && lhs.IsMPAMPresent() == rhs.IsMPAMPresent();
}

bool FieldEntriesEqualForClipboard(const FieldEntry& lhs, const FieldEntry& rhs)
{
    return FieldScopeText(lhs) == FieldScopeText(rhs)
        && FieldNameText(lhs) == FieldNameText(rhs)
        && lhs.value == rhs.value
        && lhs.raw == rhs.raw;
}

bool FlitRecordsEqualForClipboard(const FlitRecord& lhs, const FlitRecord& rhs)
{
    if (lhs.timestamp != rhs.timestamp
        || lhs.channel != rhs.channel
        || lhs.direction != rhs.direction
        || lhs.opcode != rhs.opcode
        || lhs.opcodeRaw != rhs.opcodeRaw
        || lhs.source != rhs.source
        || lhs.target != rhs.target
        || lhs.txnId != rhs.txnId
        || lhs.address != rhs.address
        || lhs.dbid != rhs.dbid
        || lhs.dataId != rhs.dataId
        || lhs.resp != rhs.resp
        || lhs.fwdState != rhs.fwdState
        || lhs.respErr != rhs.respErr
        || lhs.summary != rhs.summary
        || lhs.annotation != rhs.annotation
        || lhs.transactionLabel != rhs.transactionLabel
        || lhs.transactionGroupKey != rhs.transactionGroupKey
        || lhs.channelTag != rhs.channelTag
        || lhs.xactionDebugLog != rhs.xactionDebugLog
        || lhs.channelNodeId != rhs.channelNodeId
        || lhs.dimTarget != rhs.dimTarget
        || lhs.xactionIndexChecked != rhs.xactionIndexChecked
        || lhs.xactionIndexed != rhs.xactionIndexed
        || lhs.xactionIndexProcessedByJoint != rhs.xactionIndexProcessedByJoint
        || lhs.xactionIndexState != rhs.xactionIndexState
        || lhs.rawRecordAvailable != rhs.rawRecordAvailable
        || lhs.rawNodeId != rhs.rawNodeId
        || lhs.rawChannel != rhs.rawChannel
        || lhs.rawFlitLength != rhs.rawFlitLength
        || lhs.rawFlitWords != rhs.rawFlitWords
        || lhs.fields.size() != rhs.fields.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.fields.size(); ++index) {
        if (!FieldEntriesEqualForClipboard(lhs.fields[index], rhs.fields[index])) {
            return false;
        }
    }
    return true;
}

QString CsvEscape(QString text)
{
    const bool quote = text.contains(QLatin1Char(','))
        || text.contains(QLatin1Char('"'))
        || text.contains(QLatin1Char('\n'))
        || text.contains(QLatin1Char('\r'));
    if (!quote) {
        return text;
    }
    text.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(text);
}

QString CsvCellForColumn(const FlitRecord& record, const int column)
{
    switch (column) {
    case FlitTableModel::TimeColumn:
        return QString::number(record.timestamp);
    case FlitTableModel::ChannelColumn:
        return ToString(record.channel);
    case FlitTableModel::DirectionColumn:
        return ToString(record.direction);
    case FlitTableModel::OpcodeColumn:
        return record.opcode.isEmpty() ? record.opcodeRaw : record.opcode;
    case FlitTableModel::SourceColumn:
        return record.source;
    case FlitTableModel::TargetColumn:
        return record.target;
    case FlitTableModel::TxnColumn:
        return record.txnId;
    case FlitTableModel::AddressColumn:
        return record.address;
    case FlitTableModel::DataIdColumn:
        return record.dataId;
    case FlitTableModel::RespColumn:
        return record.resp;
    case FlitTableModel::FwdStateColumn:
        return record.fwdState;
    case FlitTableModel::RespErrColumn:
        return record.respErr;
    default:
        return {};
    }
}

QStringList CsvEscapedCells(const QStringList& cells)
{
    QStringList escaped;
    escaped.reserve(cells.size());
    for (const QString& cell : cells) {
        escaped << CsvEscape(cell);
    }
    return escaped;
}

struct ClipboardCsvFieldColumn {
    QString scope;
    QString name;
    QString valueHeader;
    QString rawHeader;
    bool includeRaw = false;
};

QStringList ClipboardCsvFixedHeaders()
{
    return {
        QStringLiteral("Time"),
        QStringLiteral("Channel"),
        QStringLiteral("Direction"),
        QStringLiteral("Opcode"),
        QStringLiteral("SrcID"),
        QStringLiteral("TgtID"),
        QStringLiteral("TxnID"),
        QStringLiteral("Addr"),
        QStringLiteral("DBID"),
        QStringLiteral("DataID"),
        QStringLiteral("Resp"),
        QStringLiteral("FwdState"),
        QStringLiteral("RespErr"),
        QStringLiteral("Summary"),
        QStringLiteral("Annotation"),
    };
}

QString ClipboardCsvFieldBaseHeader(const QString& scope, const QString& name)
{
    return scope.isEmpty() ? name : QStringLiteral("%1.%2").arg(scope, name);
}

QString MakeUniqueCsvHeader(QString header, QSet<QString>& usedHeaders)
{
    if (header.isEmpty()) {
        header = QStringLiteral("Field");
    }
    if (!usedHeaders.contains(header)) {
        usedHeaders.insert(header);
        return header;
    }

    const QString base = header;
    int suffix = 2;
    do {
        header = QStringLiteral("%1 #%2").arg(base).arg(suffix);
        ++suffix;
    } while (usedHeaders.contains(header));

    usedHeaders.insert(header);
    return header;
}

QString ClipboardCsvFieldKey(const QString& scope, const QString& name)
{
    return scope + QChar(0x1F) + name;
}

std::vector<ClipboardCsvFieldColumn> ClipboardCsvFieldColumns(const std::vector<ClipboardEntry>& entries,
                                                              QSet<QString>& usedHeaders)
{
    std::vector<ClipboardCsvFieldColumn> columns;
    QHash<QString, int> columnIndexByKey;

    for (const ClipboardEntry& entry : entries) {
        for (const FieldEntry& field : entry.record.fields) {
            const QString& name = FieldNameText(field);
            if (name.isEmpty()) {
                continue;
            }

            const QString& scope = FieldScopeText(field);
            const QString key = ClipboardCsvFieldKey(scope, name);
            const auto found = columnIndexByKey.constFind(key);
            if (found != columnIndexByKey.cend()) {
                ClipboardCsvFieldColumn& column = columns[static_cast<std::size_t>(found.value())];
                column.includeRaw = column.includeRaw || !field.raw.isEmpty();
                continue;
            }

            ClipboardCsvFieldColumn column;
            column.scope = scope;
            column.name = name;
            column.includeRaw = !field.raw.isEmpty();
            columnIndexByKey.insert(key, static_cast<int>(columns.size()));
            columns.push_back(std::move(column));
        }
    }

    for (ClipboardCsvFieldColumn& column : columns) {
        const QString baseHeader = ClipboardCsvFieldBaseHeader(column.scope, column.name);
        column.valueHeader = MakeUniqueCsvHeader(baseHeader, usedHeaders);
        if (column.includeRaw) {
            column.rawHeader = MakeUniqueCsvHeader(QStringLiteral("%1.Raw").arg(baseHeader), usedHeaders);
        }
    }
    return columns;
}

const FieldEntry* ClipboardCsvFindField(const FlitRecord& record, const ClipboardCsvFieldColumn& column)
{
    for (const FieldEntry& field : record.fields) {
        if (FieldScopeText(field) == column.scope && FieldNameText(field) == column.name) {
            return &field;
        }
    }
    return nullptr;
}

const std::array<FlitTemplateChoice, 8>& FlitTemplateChoices()
{
    static const std::array<FlitTemplateChoice, 8> choices{{
        {QStringLiteral("REQ TX"), FlitChannel::Req, FlitDirection::Tx},
        {QStringLiteral("REQ RX"), FlitChannel::Req, FlitDirection::Rx},
        {QStringLiteral("RSP TX"), FlitChannel::Rsp, FlitDirection::Tx},
        {QStringLiteral("RSP RX"), FlitChannel::Rsp, FlitDirection::Rx},
        {QStringLiteral("DAT TX"), FlitChannel::Dat, FlitDirection::Tx},
        {QStringLiteral("DAT RX"), FlitChannel::Dat, FlitDirection::Rx},
        {QStringLiteral("SNP TX"), FlitChannel::Snp, FlitDirection::Tx},
        {QStringLiteral("SNP RX"), FlitChannel::Snp, FlitDirection::Rx},
    }};
    return choices;
}

bool ClipboardEntryModifiedForOrdering(const ClipboardEntry& entry)
{
    return !FlitRecordsEqualForClipboard(entry.record, entry.originalRecord);
}

bool ClipboardEntriesAreMainTraceSubsetForSession(const std::vector<ClipboardEntry>& entries,
                                                  const quint64 sourceSessionId)
{
    int previousLogicalRow = -1;
    for (const ClipboardEntry& entry : entries) {
        if (entry.source.sessionId != sourceSessionId || entry.source.logicalRow < 0) {
            return false;
        }
        if (entry.source.logicalRow < previousLogicalRow) {
            return false;
        }
        previousLogicalRow = entry.source.logicalRow;
    }
    return true;
}

bool ClipboardEntrySourceRowLess(const ClipboardEntry& lhs, const ClipboardEntry& rhs)
{
    if (lhs.source.logicalRow != rhs.source.logicalRow) {
        return lhs.source.logicalRow < rhs.source.logicalRow;
    }
    return lhs.sequence < rhs.sequence;
}

bool ClipboardEntriesAreSourceSortedForSession(const std::vector<ClipboardEntry>& entries,
                                               const quint64 sourceSessionId,
                                               const std::size_t begin,
                                               const std::size_t end)
{
    if (begin >= end) {
        return true;
    }
    int previousLogicalRow = -1;
    for (std::size_t index = begin; index < end; ++index) {
        const ClipboardEntry& entry = entries[index];
        if (entry.source.sessionId != sourceSessionId || entry.source.logicalRow < 0) {
            return false;
        }
        if (entry.source.logicalRow < previousLogicalRow) {
            return false;
        }
        previousLogicalRow = entry.source.logicalRow;
    }
    return true;
}

bool ClipboardEntriesAreSourceSortedForSession(const std::vector<ClipboardEntry>& entries,
                                               const quint64 sourceSessionId)
{
    return ClipboardEntriesAreSourceSortedForSession(entries, sourceSessionId, 0, entries.size());
}

std::vector<ClipboardEntry> MergeClipboardEntriesBySourceRow(std::vector<ClipboardEntry>& left,
                                                             std::vector<ClipboardEntry>& right)
{
    std::vector<ClipboardEntry> merged;
    merged.reserve(left.size() + right.size());

    std::size_t leftIndex = 0;
    std::size_t rightIndex = 0;
    while (leftIndex < left.size() && rightIndex < right.size()) {
        if (ClipboardEntrySourceRowLess(right[rightIndex], left[leftIndex])) {
            merged.push_back(std::move(right[rightIndex++]));
        } else {
            merged.push_back(std::move(left[leftIndex++]));
        }
    }
    while (leftIndex < left.size()) {
        merged.push_back(std::move(left[leftIndex++]));
    }
    while (rightIndex < right.size()) {
        merged.push_back(std::move(right[rightIndex++]));
    }
    return merged;
}

void SortClipboardEntriesBySourceRow(std::vector<ClipboardEntry>& entries)
{
    std::stable_sort(entries.begin(), entries.end(), ClipboardEntrySourceRowLess);
}

void OrderClipboardEntriesForInsertion(std::vector<ClipboardEntry>& entries,
                                       const quint64 sourceSessionId,
                                       const int ordering,
                                       std::size_t insertionStartIndex)
{
    constexpr int kPreserveInputOrder = 1;
    if (entries.size() <= 1) {
        return;
    }
    insertionStartIndex = std::min(insertionStartIndex, entries.size());

    if (ordering == kPreserveInputOrder
        && ClipboardEntriesAreMainTraceSubsetForSession(entries, sourceSessionId)) {
        return;
    }

    std::vector<std::size_t> modifiedIndexes;
    modifiedIndexes.reserve(entries.size());
    bool hasModifiedEntries = false;
    bool allSourceBackedSameSession = true;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const ClipboardEntry& entry = entries[index];
        if (entry.source.sessionId != sourceSessionId || entry.source.logicalRow < 0) {
            allSourceBackedSameSession = false;
        }
        if (ClipboardEntryModifiedForOrdering(entry)) {
            hasModifiedEntries = true;
            modifiedIndexes.push_back(index);
        }
    }

    if (!hasModifiedEntries && allSourceBackedSameSession) {
        if (ClipboardEntriesAreSourceSortedForSession(entries, sourceSessionId, 0, insertionStartIndex)
            && ClipboardEntriesAreSourceSortedForSession(entries, sourceSessionId, insertionStartIndex, entries.size())) {
            std::vector<ClipboardEntry> existing;
            std::vector<ClipboardEntry> appended;
            existing.reserve(insertionStartIndex);
            appended.reserve(entries.size() - insertionStartIndex);
            for (std::size_t index = 0; index < insertionStartIndex; ++index) {
                existing.push_back(std::move(entries[index]));
            }
            for (std::size_t index = insertionStartIndex; index < entries.size(); ++index) {
                appended.push_back(std::move(entries[index]));
            }
            entries = MergeClipboardEntriesBySourceRow(existing, appended);
        } else {
            SortClipboardEntriesBySourceRow(entries);
        }
        return;
    }

    if (hasModifiedEntries && modifiedIndexes.size() < entries.size()) {
        std::vector<std::size_t> unmodifiedIndexes;
        std::vector<ClipboardEntry> unmodifiedEntries;
        std::vector<ClipboardEntry> existingUnmodifiedEntries;
        std::vector<ClipboardEntry> appendedUnmodifiedEntries;
        unmodifiedIndexes.reserve(entries.size() - modifiedIndexes.size());
        if (allSourceBackedSameSession) {
            existingUnmodifiedEntries.reserve(std::min(insertionStartIndex, entries.size()));
            appendedUnmodifiedEntries.reserve(entries.size() - insertionStartIndex);
        } else {
            unmodifiedEntries.reserve(entries.size() - modifiedIndexes.size());
        }
        std::vector<ClipboardEntry> modifiedEntries;
        modifiedEntries.reserve(modifiedIndexes.size());
        for (std::size_t index = 0; index < entries.size(); ++index) {
            if (ClipboardEntryModifiedForOrdering(entries[index])) {
                modifiedEntries.push_back(std::move(entries[index]));
            } else {
                unmodifiedIndexes.push_back(index);
                if (allSourceBackedSameSession) {
                    if (index < insertionStartIndex) {
                        existingUnmodifiedEntries.push_back(std::move(entries[index]));
                    } else {
                        appendedUnmodifiedEntries.push_back(std::move(entries[index]));
                    }
                } else {
                    unmodifiedEntries.push_back(std::move(entries[index]));
                }
            }
        }
        if (allSourceBackedSameSession) {
            if (ClipboardEntriesAreSourceSortedForSession(existingUnmodifiedEntries, sourceSessionId)
                && ClipboardEntriesAreSourceSortedForSession(appendedUnmodifiedEntries, sourceSessionId)) {
                unmodifiedEntries = MergeClipboardEntriesBySourceRow(existingUnmodifiedEntries,
                                                                     appendedUnmodifiedEntries);
            } else {
                unmodifiedEntries.reserve(existingUnmodifiedEntries.size() + appendedUnmodifiedEntries.size());
                for (ClipboardEntry& entry : existingUnmodifiedEntries) {
                    unmodifiedEntries.push_back(std::move(entry));
                }
                for (ClipboardEntry& entry : appendedUnmodifiedEntries) {
                    unmodifiedEntries.push_back(std::move(entry));
                }
                SortClipboardEntriesBySourceRow(unmodifiedEntries);
            }
        }
        std::stable_sort(modifiedEntries.begin(),
                         modifiedEntries.end(),
                         [](const ClipboardEntry& lhs, const ClipboardEntry& rhs) {
                             if (lhs.record.timestamp != rhs.record.timestamp) {
                                 return lhs.record.timestamp < rhs.record.timestamp;
                             }
                             return lhs.sequence < rhs.sequence;
                         });
        for (std::size_t index = 0; index < unmodifiedIndexes.size(); ++index) {
            entries[unmodifiedIndexes[index]] = std::move(unmodifiedEntries[index]);
        }
        for (std::size_t index = 0; index < modifiedIndexes.size(); ++index) {
            entries[modifiedIndexes[index]] = std::move(modifiedEntries[index]);
        }
        return;
    }

    std::stable_sort(entries.begin(), entries.end(), [](const ClipboardEntry& lhs, const ClipboardEntry& rhs) {
        if (lhs.record.timestamp != rhs.record.timestamp) {
            return lhs.record.timestamp < rhs.record.timestamp;
        }
        return lhs.sequence < rhs.sequence;
    });
}

QString EditResultMessage(const FlitEditResult& result)
{
    if (result.detail.isEmpty()) {
        return result.summary;
    }
    if (result.summary.isEmpty()) {
        return result.detail;
    }
    return result.summary + QStringLiteral("\n") + result.detail;
}

QString FieldDisplayText(const FlitRecord& record, const QString& fieldName)
{
    if (fieldName == QLatin1String("Time")) {
        return QString::number(record.timestamp);
    }
    if (fieldName == QLatin1String("Channel")) {
        return ToString(record.channel);
    }
    if (fieldName == QLatin1String("Direction")) {
        return ToString(record.direction);
    }
    if (fieldName == QLatin1String("Opcode")) {
        return record.opcode.isEmpty() ? record.opcodeRaw : record.opcode;
    }
    if (fieldName == QLatin1String("SrcID")) {
        return record.source;
    }
    if (fieldName == QLatin1String("TgtID")) {
        return record.target;
    }
    if (fieldName == QLatin1String("TxnID")) {
        return record.txnId;
    }
    if (fieldName == QLatin1String("Addr")) {
        return record.address;
    }
    if (fieldName == QLatin1String("DBID")) {
        return record.dbid;
    }
    if (fieldName == QLatin1String("DataID")) {
        return record.dataId;
    }
    if (fieldName == QLatin1String("Resp")) {
        return record.resp;
    }
    if (fieldName == QLatin1String("FwdState")) {
        return record.fwdState;
    }
    if (fieldName == QLatin1String("RespErr")) {
        return record.respErr;
    }

    for (const FieldEntry& field : record.fields) {
        if (FieldNameText(field) == fieldName) {
            return field.value.isEmpty() ? field.raw : field.value;
        }
    }
    return QString();
}

int TimestampInsertLogicalRow(const FlitTableModel* model, const qint64 timestamp)
{
    if (!model) {
        return 0;
    }

    int low = 0;
    int high = model->totalCount();
    while (low < high) {
        const int middle = low + ((high - low) / 2);
        const FlitRecord* record = model->recordForLogicalRow(middle);
        if (!record) {
            high = middle;
            continue;
        }

        if (record->timestamp <= timestamp) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

bool ParseClipboardAddressToken(QString text, std::uint64_t& value)
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

bool RowAddressValueForClipboard(const FlitRecord& record, std::uint64_t& address)
{
    if (ParseClipboardAddressToken(record.address, address)) {
        return true;
    }

    for (const FieldEntry& field : record.fields) {
        if (FieldNameText(field) != QLatin1String("Addr")) {
            continue;
        }
        if (ParseClipboardAddressToken(field.value, address)) {
            return true;
        }
        const QString rawToken = field.raw.section(QChar(' '), 0, 0, QString::SectionSkipEmpty);
        if (ParseClipboardAddressToken(rawToken, address)) {
            return true;
        }
    }

    return false;
}

bool ClipboardFastRecordHasAddress(const CLogBTraceFastRecordInfo& record) noexcept
{
    return record.address != (std::numeric_limits<std::uint64_t>::max)();
}

bool ClipboardFastRecordIsRequestOrigin(const CLogBTraceFastRecordInfo& record) noexcept
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

FlitChannel ClipboardFlitChannelFromTransport(const CLog::Channel channel) noexcept
{
    switch (channel) {
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP:
        return FlitChannel::Rsp;
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT:
        return FlitChannel::Dat;
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP:
        return FlitChannel::Snp;
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
    case CLog::Channel::RXREQ:
        return FlitChannel::Req;
    }
    return FlitChannel::Req;
}

FlitDirection ClipboardFlitDirectionFromTransport(const CLog::Channel channel) noexcept
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::TXRSP:
    case CLog::Channel::TXDAT:
    case CLog::Channel::TXSNP:
        return FlitDirection::Tx;
    case CLog::Channel::RXREQ_BeforeSAM:
    case CLog::Channel::RXREQ:
    case CLog::Channel::RXRSP:
    case CLog::Channel::RXDAT:
    case CLog::Channel::RXSNP:
        return FlitDirection::Rx;
    }
    return FlitDirection::Tx;
}

bool ClipboardTransportIsBeforeSam(const CLog::Channel channel) noexcept
{
    return channel == CLog::Channel::TXREQ_BeforeSAM
        || channel == CLog::Channel::RXREQ_BeforeSAM;
}

QString ClipboardOptionalIdText(const std::uint32_t value)
{
    return value == (std::numeric_limits<std::uint32_t>::max)()
        ? QString()
        : QString::number(value);
}

QString ClipboardOptionalAddressText(const std::uint64_t value, const CLog::Parameters& parameters)
{
    return value == (std::numeric_limits<std::uint64_t>::max)()
        ? QString()
        : QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper()
                                         .rightJustified(
                                             std::max(1, static_cast<int>((parameters.GetReqAddrWidth() + 3U) / 4U)),
                                             QLatin1Char('0')));
}

QString ClipboardAnnotationForFastNode(const CLogBTraceMetadata& metadata, const std::uint32_t nodeId)
{
    if (nodeId > (std::numeric_limits<quint16>::max)()) {
        return QString();
    }
    const auto found = metadata.nodeAnnotations.find(static_cast<quint16>(nodeId));
    if (found == metadata.nodeAnnotations.cend()) {
        return InternDisplayString(QStringLiteral("Captured at node %1.").arg(nodeId));
    }
    return InternDisplayString(QStringLiteral("Captured at node %1 (%2).")
                                   .arg(nodeId)
                                   .arg(found->second));
}

FlitRecord ClipboardRecordFromFastRecord(const CLogBTraceMetadata& metadata,
                                         const CLogBTraceFastRecordInfo& fast,
                                         const ClipboardXactionAddressRowRef& rowRef)
{
    const auto transportChannel = static_cast<CLog::Channel>(fast.channel);
    FlitRecord record;
    record.timestamp = fast.timestamp;
    record.channel = ClipboardFlitChannelFromTransport(transportChannel);
    record.direction = ClipboardFlitDirectionFromTransport(transportChannel);
    record.opcode = InternDisplayString(CLogBTraceLoader::opcodeDisplayValue(metadata.parameters, fast));
    record.opcodeRaw = QStringLiteral("0x%1").arg(QString::number(fast.opcode, 16).toUpper());
    record.source = ClipboardOptionalIdText(fast.sourceId);
    record.target = ClipboardOptionalIdText(fast.targetId);
    record.txnId = ClipboardOptionalIdText(fast.txnId);
    record.address = ClipboardOptionalAddressText(fast.address, metadata.parameters);
    record.dbid = ClipboardOptionalIdText(fast.dbid);
    record.summary = QStringLiteral("%1%2 %3")
                         .arg(ToString(record.direction),
                              ToString(record.channel),
                              record.opcode.isEmpty() ? record.opcodeRaw : record.opcode);
    record.annotation = ClipboardAnnotationForFastNode(metadata, fast.nodeId);
    record.transactionGroupKey = QStringLiteral("xaction|%1").arg(rowRef.ordinal);
    record.channelNodeId =
        fast.nodeId <= (std::numeric_limits<quint16>::max)()
            ? std::optional<quint16>(static_cast<quint16>(fast.nodeId))
            : std::nullopt;
    record.xactionIndexChecked = true;
    record.xactionIndexed = rowRef.ordinal != 0;
    record.xactionIndexProcessedByJoint =
        (rowRef.compactFlags & kClipboardXactionRowProcessedByJoint) != 0;
    record.xactionIndexState =
        (rowRef.compactFlags & kClipboardXactionRowIndexed) != 0
            ? XactionIndexState::Indexed
            : ((rowRef.compactFlags & kClipboardXactionRowProcessed) != 0
                   ? XactionIndexState::Denied
                   : XactionIndexState::NotStarted);
    if (ClipboardTransportIsBeforeSam(transportChannel)) {
        record.channelTag = QStringLiteral("Before SAM");
        record.dimTarget = true;
    }
    return record;
}

bool ClipboardReadFastRecordsForSlice(const std::shared_ptr<TraceSession>& session,
                                      const ClipboardXactionAddressSlice& slice,
                                      std::vector<CLogBTraceFastRecordInfo>& fastRecords,
                                      CLogBTraceLoadError& error,
                                      std::stop_token stopToken)
{
    fastRecords.clear();
    error = {};
    if (!session) {
        error.summary = QStringLiteral("No trace session is available for Clipboard insertion.");
        return false;
    }
    if (slice.blockIndex >= session->metadata().blocks.size()) {
        error.summary = QStringLiteral("Clipboard insertion scan block is out of range.");
        return false;
    }
    if (slice.rowCount > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        error.summary = QStringLiteral("Clipboard insertion scan slice is too large for this platform.");
        return false;
    }

    const std::size_t rowCount = static_cast<std::size_t>(slice.rowCount);
    if (session->readFastRecordsFromIndex(slice.blockIndex,
                                          slice.localBegin,
                                          rowCount,
                                          fastRecords)
        && fastRecords.size() >= rowCount) {
        return true;
    }

    std::shared_ptr<CLog::CLogB::TagCHIRecords> block;
    if (!CLogBTraceLoader::loadBlockRecords(session->filePath(),
                                            session->metadata(),
                                            slice.blockIndex,
                                            block,
                                            error,
                                            {},
                                            stopToken)) {
        return false;
    }
    if (!block) {
        error.summary = QStringLiteral("Trace block could not be loaded for Clipboard insertion.");
        return false;
    }

    std::vector<CLogBTraceFastRecordInfo> blockFastRecords;
    if (!CLogBTraceLoader::buildBlockFastRecords(session->metadata(),
                                                slice.blockIndex,
                                                *block,
                                                blockFastRecords,
                                                error,
                                                stopToken)) {
        return false;
    }

    if (slice.localBegin > blockFastRecords.size()
        || rowCount > blockFastRecords.size() - slice.localBegin) {
        error.summary = QStringLiteral("Trace block returned fewer fast records than requested.");
        return false;
    }

    fastRecords.insert(fastRecords.end(),
                       blockFastRecords.begin() + static_cast<std::ptrdiff_t>(slice.localBegin),
                       blockFastRecords.begin() + static_cast<std::ptrdiff_t>(slice.localBegin + rowCount));
    return true;
}

std::vector<ClipboardXactionAddressSlice> ClipboardXactionAddressSlices(
    const std::shared_ptr<TraceSession>& session)
{
    std::vector<ClipboardXactionAddressSlice> slices;
    if (!session) {
        return slices;
    }

    constexpr std::uint64_t kSliceRows = 262144;
    for (std::size_t blockIndex = 0; blockIndex < session->metadata().blocks.size(); ++blockIndex) {
        const CLogBTraceBlockSummary& block = session->metadata().blocks[blockIndex];
        for (std::uint64_t localBegin = 0; localBegin < block.recordCount;) {
            const std::uint64_t rowCount =
                std::min<std::uint64_t>(kSliceRows, block.recordCount - localBegin);
            slices.push_back(ClipboardXactionAddressSlice{
                .blockIndex = blockIndex,
                .beginRow = block.rowStart + localBegin,
                .rowCount = rowCount,
                .localBegin = static_cast<std::size_t>(localBegin),
            });
            localBegin += rowCount;
        }
    }
    return slices;
}

std::vector<ClipboardXactionAddressLogicalSlice> ClipboardXactionAddressLogicalSlices(
    const std::shared_ptr<TraceSession>& session)
{
    std::vector<ClipboardXactionAddressLogicalSlice> slices;
    if (!session) {
        return slices;
    }

    constexpr std::uint64_t kSliceRows = 262144;
    const std::uint64_t totalRows = session->totalRows();
    for (std::uint64_t beginRow = 0; beginRow < totalRows;) {
        const std::uint64_t rowCount = std::min<std::uint64_t>(kSliceRows, totalRows - beginRow);
        slices.push_back(ClipboardXactionAddressLogicalSlice{
            .beginRow = beginRow,
            .rowCount = rowCount,
        });
        beginRow += rowCount;
    }
    return slices;
}

std::size_t ClipboardXactionAddressWorkerCount(const std::size_t sliceCount) noexcept
{
    if (sliceCount <= 1) {
        return 1;
    }
    const unsigned int hardware = std::max(1U, std::thread::hardware_concurrency());
    const std::size_t uiFriendlyHardware =
        hardware > 2U ? static_cast<std::size_t>(hardware - 1U)
                      : static_cast<std::size_t>(hardware);
    constexpr std::size_t kMaxWorkers = 6;
    return std::min<std::size_t>({sliceCount, kMaxWorkers, uiFriendlyHardware});
}

bool ClipboardCollectFastXactionAnchors(
    const std::shared_ptr<TraceSession>& session,
    const std::vector<ClipboardXactionAddressSlice>& slices,
    std::stop_token stopToken,
    const std::function<void(std::uint64_t completedRows)>& progressCallback,
    ClipboardXactionAddressScanResult& result,
    CLogBTraceLoadError& error)
{
    result = {};
    error = {};
    if (!session) {
        error.summary = QStringLiteral("No trace session is available for Clipboard insertion.");
        return false;
    }
    if (slices.empty()) {
        return true;
    }

    std::atomic<std::size_t> nextSlice = 0;
    std::atomic_bool failed = false;
    std::atomic_bool cancelled = false;
    std::mutex mergeMutex;
    std::mutex errorMutex;
    CLogBTraceLoadError firstError;

    const auto worker = [&]() {
        std::unordered_map<std::uint64_t, XactionAddressAnchor> localAnchors;
        std::unordered_set<std::uint64_t> localOrdinals;
        std::vector<CLogBTraceFastRecordInfo> fastRecords;
        std::vector<TraceSession::XactionRowCompactInfo> compactInfos;
        CLogBTraceLoadError localError;
        std::uint64_t localScannedRows = 0;

        while (!failed.load(std::memory_order_relaxed)) {
            if (stopToken.stop_requested()) {
                cancelled.store(true, std::memory_order_relaxed);
                break;
            }

            const std::size_t sliceIndex = nextSlice.fetch_add(1, std::memory_order_relaxed);
            if (sliceIndex >= slices.size()) {
                break;
            }

            const ClipboardXactionAddressSlice& slice = slices[sliceIndex];
            fastRecords.clear();
            compactInfos.clear();
            localError = {};
            if (!ClipboardReadFastRecordsForSlice(session,
                                                  slice,
                                                  fastRecords,
                                                  localError,
                                                  stopToken)) {
                if (localError.summary.isEmpty()) {
                    localError.summary = QStringLiteral("Could not read fast records for Clipboard insertion.");
                }
            } else if (!session->xactionRowCompactInfoRange(slice.beginRow,
                                                            slice.rowCount,
                                                            compactInfos)) {
                localError.summary =
                    QStringLiteral("Could not read compact xaction rows for Clipboard insertion.");
            } else if (fastRecords.size() < static_cast<std::size_t>(slice.rowCount)
                       || compactInfos.size() < static_cast<std::size_t>(slice.rowCount)) {
                localError.summary =
                    QStringLiteral("Trace index returned fewer rows than requested for Clipboard insertion.");
            }

            if (!localError.summary.isEmpty()) {
                {
                    const std::lock_guard lock(errorMutex);
                    if (firstError.summary.isEmpty()) {
                        firstError = localError;
                    }
                }
                failed.store(true, std::memory_order_relaxed);
                break;
            }

            const std::size_t rowCount = static_cast<std::size_t>(slice.rowCount);
            for (std::size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
                const TraceSession::XactionRowCompactInfo& compact = compactInfos[rowIndex];
                if (compact.xactionOrdinal == 0) {
                    continue;
                }
                const std::uint64_t logicalRow = slice.beginRow + static_cast<std::uint64_t>(rowIndex);
                localOrdinals.insert(compact.xactionOrdinal);

                const CLogBTraceFastRecordInfo& fast = fastRecords[rowIndex];
                if (!ClipboardFastRecordIsRequestOrigin(fast)
                    || !ClipboardFastRecordHasAddress(fast)) {
                    continue;
                }

                XactionAddressAnchor candidate{
                    .ordinal = compact.xactionOrdinal,
                    .requestLogicalRow = logicalRow,
                    .lineAddress = CLogBTraceLoader::normalizeCacheLineAddress(fast.address),
                };
                const auto found = localAnchors.find(candidate.ordinal);
                if (found == localAnchors.end()
                    || candidate.requestLogicalRow < found->second.requestLogicalRow) {
                    localAnchors[candidate.ordinal] = candidate;
                }
            }

            localScannedRows += slice.rowCount;
            if (progressCallback) {
                progressCallback(localScannedRows);
                localScannedRows = 0;
            }
        }

        const std::lock_guard lock(mergeMutex);
        result.scannedRows += localScannedRows;
        for (auto& [ordinal, anchor] : localAnchors) {
            const auto found = result.anchorsByOrdinal.find(ordinal);
            if (found == result.anchorsByOrdinal.end()
                || anchor.requestLogicalRow < found->second.requestLogicalRow) {
                result.anchorsByOrdinal[ordinal] = anchor;
            }
        }
        result.indexedOrdinals.insert(localOrdinals.begin(), localOrdinals.end());
    };

    const std::size_t workerCount = ClipboardXactionAddressWorkerCount(slices.size());
    if (workerCount <= 1) {
        worker();
    } else {
        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (std::size_t index = 0; index < workerCount; ++index) {
            workers.emplace_back(worker);
        }
    }

    if (progressCallback && result.scannedRows > 0) {
        progressCallback(result.scannedRows);
    }
    result.scannedRows = 0;

    if (cancelled.load(std::memory_order_relaxed) || stopToken.stop_requested()) {
        return true;
    }
    if (failed.load(std::memory_order_relaxed)) {
        error = firstError;
        return false;
    }
    return true;
}

bool ClipboardCollectMatchingXactionRows(
    const std::shared_ptr<TraceSession>& session,
    const std::vector<ClipboardXactionAddressLogicalSlice>& slices,
    const std::unordered_set<std::uint64_t>& matchingOrdinals,
    const std::unordered_set<std::uint64_t>& existingClipboardRows,
    std::stop_token stopToken,
    const std::function<void(std::uint64_t completedRows)>& progressCallback,
    std::vector<ClipboardXactionAddressRowRef>& includedRows,
    int& preSkippedDuplicates,
    QString& errorText)
{
    includedRows.clear();
    preSkippedDuplicates = 0;
    errorText.clear();
    if (!session || slices.empty() || matchingOrdinals.empty()) {
        return true;
    }

    std::atomic<std::size_t> nextSlice = 0;
    std::atomic_bool failed = false;
    std::atomic_bool cancelled = false;
    std::atomic<int> skippedDuplicates = 0;
    std::mutex errorMutex;
    QString firstError;
    std::vector<std::vector<ClipboardXactionAddressRowRef>> rowsBySlice(slices.size());

    const auto worker = [&]() {
        std::vector<ClipboardXactionAddressRowRef> localRows;
        std::vector<TraceSession::XactionRowCompactInfo> compactInfos;
        std::uint64_t localCompletedRows = 0;

        while (!failed.load(std::memory_order_relaxed)) {
            if (stopToken.stop_requested()) {
                cancelled.store(true, std::memory_order_relaxed);
                break;
            }

            const std::size_t sliceIndex = nextSlice.fetch_add(1, std::memory_order_relaxed);
            if (sliceIndex >= slices.size()) {
                break;
            }

            const ClipboardXactionAddressLogicalSlice& slice = slices[sliceIndex];
            localRows.clear();
            compactInfos.clear();
            if (!session->xactionRowCompactInfoRange(slice.beginRow,
                                                     slice.rowCount,
                                                     compactInfos)
                || compactInfos.size() < static_cast<std::size_t>(slice.rowCount)) {
                {
                    const std::lock_guard lock(errorMutex);
                    if (firstError.isEmpty()) {
                        firstError =
                            QStringLiteral("Could not collect compact xaction rows for Clipboard insertion.");
                    }
                }
                failed.store(true, std::memory_order_relaxed);
                break;
            }

            const std::size_t rowCount = static_cast<std::size_t>(slice.rowCount);
            for (std::size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
                const std::uint64_t ordinal = compactInfos[rowIndex].xactionOrdinal;
                if (ordinal == 0 || matchingOrdinals.find(ordinal) == matchingOrdinals.end()) {
                    continue;
                }
                const std::uint64_t logicalRow = slice.beginRow + static_cast<std::uint64_t>(rowIndex);
                if (logicalRow > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
                    continue;
                }
                if (existingClipboardRows.find(logicalRow) != existingClipboardRows.end()) {
                    skippedDuplicates.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                localRows.push_back(ClipboardXactionAddressRowRef{
                    .logicalRow = logicalRow,
                    .ordinal = ordinal,
                    .compactFlags = compactInfos[rowIndex].flags,
                });
            }

            localCompletedRows += slice.rowCount;
            if (progressCallback) {
                progressCallback(localCompletedRows);
                localCompletedRows = 0;
            }
            rowsBySlice[sliceIndex] = std::move(localRows);
        }
        if (progressCallback && localCompletedRows > 0) {
            progressCallback(localCompletedRows);
        }
    };

    const std::size_t workerCount = ClipboardXactionAddressWorkerCount(slices.size());
    if (workerCount <= 1) {
        worker();
    } else {
        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (std::size_t index = 0; index < workerCount; ++index) {
            workers.emplace_back(worker);
        }
    }

    if (cancelled.load(std::memory_order_relaxed) || stopToken.stop_requested()) {
        preSkippedDuplicates = skippedDuplicates.load(std::memory_order_relaxed);
        return true;
    }
    if (failed.load(std::memory_order_relaxed)) {
        const std::lock_guard lock(errorMutex);
        errorText = firstError;
        preSkippedDuplicates = skippedDuplicates.load(std::memory_order_relaxed);
        return false;
    }

    std::size_t totalRows = 0;
    for (const auto& sliceRows : rowsBySlice) {
        totalRows += sliceRows.size();
    }
    includedRows.reserve(totalRows);
    for (auto& sliceRows : rowsBySlice) {
        includedRows.insert(includedRows.end(),
                            std::make_move_iterator(sliceRows.begin()),
                            std::make_move_iterator(sliceRows.end()));
    }
    preSkippedDuplicates = skippedDuplicates.load(std::memory_order_relaxed);
    return true;
}

bool ClipboardDecodeRowsInChunks(
    const std::shared_ptr<TraceSession>& session,
    const std::vector<std::uint64_t>& includedLogicalRows,
    std::stop_token stopToken,
    const std::function<void(std::uint64_t completedRows)>& progressCallback,
    std::vector<std::pair<int, FlitRecord>>& rows,
    QString& errorText)
{
    rows.clear();
    errorText.clear();
    if (!session || includedLogicalRows.empty()) {
        return true;
    }

    struct Run {
        std::size_t startIndex = 0;
        std::uint64_t beginRow = 0;
        std::uint64_t count = 0;
    };

    std::vector<Run> runs;
    runs.reserve(includedLogicalRows.size());
    for (std::size_t index = 0; index < includedLogicalRows.size();) {
        const std::uint64_t beginRow = includedLogicalRows[index];
        std::uint64_t count = 1;
        while (index + static_cast<std::size_t>(count) < includedLogicalRows.size()
               && includedLogicalRows[index + static_cast<std::size_t>(count)] == beginRow + count) {
            ++count;
        }
        runs.push_back(Run{
            .startIndex = index,
            .beginRow = beginRow,
            .count = count,
        });
        index += static_cast<std::size_t>(count);
    }

    rows.resize(includedLogicalRows.size());

    std::atomic<std::size_t> nextRun = 0;
    std::atomic_bool failed = false;
    std::atomic_bool cancelled = false;
    std::mutex errorMutex;
    QString firstError;

    const auto worker = [&]() {
        CLogBTraceLoadError error;
        std::vector<FlitRecord> decodedRows;
        std::uint64_t localCompletedRows = 0;

        while (!failed.load(std::memory_order_relaxed)) {
            if (stopToken.stop_requested()) {
                cancelled.store(true, std::memory_order_relaxed);
                break;
            }

            const std::size_t runIndex = nextRun.fetch_add(1, std::memory_order_relaxed);
            if (runIndex >= runs.size()) {
                break;
            }

            const Run& run = runs[runIndex];
            decodedRows.clear();
            if (!session->readRowsDirect(run.beginRow,
                                         run.count,
                                         decodedRows,
                                         error,
                                         {},
                                         stopToken)) {
                QString text = error.informativeText.isEmpty()
                    ? error.summary
                    : error.summary + QStringLiteral(" ") + error.informativeText;
                if (text.isEmpty()) {
                    text = QStringLiteral("Could not read Clipboard insertion rows from the trace.");
                }
                {
                    const std::lock_guard lock(errorMutex);
                    if (firstError.isEmpty()) {
                        firstError = std::move(text);
                    }
                }
                failed.store(true, std::memory_order_relaxed);
                break;
            }

            const std::size_t decodedCount =
                std::min<std::size_t>(decodedRows.size(), static_cast<std::size_t>(run.count));
            if (decodedCount != static_cast<std::size_t>(run.count)) {
                QString text = QStringLiteral("Trace row decoder returned fewer rows than requested.");
                {
                    const std::lock_guard lock(errorMutex);
                    if (firstError.isEmpty()) {
                        firstError = std::move(text);
                    }
                }
                failed.store(true, std::memory_order_relaxed);
                break;
            }
            for (std::size_t offset = 0; offset < decodedCount; ++offset) {
                rows[run.startIndex + offset] = {
                    static_cast<int>(run.beginRow + static_cast<std::uint64_t>(offset)),
                    std::move(decodedRows[offset]),
                };
            }

            localCompletedRows += static_cast<std::uint64_t>(decodedCount);
            if (progressCallback) {
                progressCallback(localCompletedRows);
                localCompletedRows = 0;
            }
        }

        if (progressCallback && localCompletedRows > 0) {
            progressCallback(localCompletedRows);
        }
    };

    const std::size_t workerCount = ClipboardXactionAddressWorkerCount(runs.size());
    if (workerCount <= 1) {
        worker();
    } else {
        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (std::size_t index = 0; index < workerCount; ++index) {
            workers.emplace_back(worker);
        }
    }

    if (cancelled.load(std::memory_order_relaxed) || stopToken.stop_requested()) {
        rows.clear();
        return true;
    }
    if (failed.load(std::memory_order_relaxed)) {
        const std::lock_guard lock(errorMutex);
        errorText = firstError;
        rows.clear();
        return false;
    }
    return true;
}

bool ClipboardMaterializeRowsFromFastIndex(
    const std::shared_ptr<TraceSession>& session,
    const std::vector<ClipboardXactionAddressRowRef>& includedRows,
    std::stop_token stopToken,
    const std::function<void(std::uint64_t completedRows)>& progressCallback,
    std::vector<std::pair<int, FlitRecord>>& rows,
    QString& errorText)
{
    rows.clear();
    errorText.clear();
    if (!session || includedRows.empty()) {
        return true;
    }

    rows.resize(includedRows.size());
    const CLogBTraceMetadata& metadata = session->metadata();
    std::size_t rowIndex = 0;
    for (std::size_t blockIndex = 0;
         blockIndex < metadata.blocks.size() && rowIndex < includedRows.size();
         ++blockIndex) {
        if (stopToken.stop_requested()) {
            rows.clear();
            return true;
        }

        const CLogBTraceBlockSummary& block = metadata.blocks[blockIndex];
        const std::uint64_t blockBegin = block.rowStart;
        const std::uint64_t blockEnd = block.rowStart + block.recordCount;
        while (rowIndex < includedRows.size() && includedRows[rowIndex].logicalRow < blockBegin) {
            ++rowIndex;
        }
        if (rowIndex >= includedRows.size()) {
            break;
        }
        if (includedRows[rowIndex].logicalRow >= blockEnd) {
            continue;
        }

        const std::size_t blockRowBeginIndex = rowIndex;
        std::uint64_t firstLogicalRow = includedRows[rowIndex].logicalRow;
        std::uint64_t lastLogicalRow = firstLogicalRow;
        while (rowIndex < includedRows.size()
               && includedRows[rowIndex].logicalRow >= blockBegin
               && includedRows[rowIndex].logicalRow < blockEnd) {
            lastLogicalRow = includedRows[rowIndex].logicalRow;
            ++rowIndex;
        }
        const std::size_t blockRowEndIndex = rowIndex;
        const std::size_t localBegin = static_cast<std::size_t>(firstLogicalRow - blockBegin);
        const std::size_t localCount = static_cast<std::size_t>(lastLogicalRow - firstLogicalRow + 1);

        std::vector<CLogBTraceFastRecordInfo> fastRecords;
        CLogBTraceLoadError fastError;
        bool haveFastRecords =
            session->readFastRecordsFromIndex(blockIndex, localBegin, localCount, fastRecords)
            && fastRecords.size() >= localCount;
        if (!haveFastRecords) {
            fastRecords.clear();
            fastError = {};
            haveFastRecords =
                session->readFastRecords(blockIndex,
                                         localBegin,
                                         localCount,
                                         fastRecords,
                                         fastError,
                                         stopToken)
                && fastRecords.size() >= localCount;
        }

        if (haveFastRecords) {
            for (std::size_t index = blockRowBeginIndex; index < blockRowEndIndex; ++index) {
                if (stopToken.stop_requested()) {
                    rows.clear();
                    return true;
                }
                const ClipboardXactionAddressRowRef& rowRef = includedRows[index];
                const std::size_t localOffset =
                    static_cast<std::size_t>(rowRef.logicalRow - firstLogicalRow);
                rows[index] = {
                    static_cast<int>(rowRef.logicalRow),
                    ClipboardRecordFromFastRecord(metadata, fastRecords[localOffset], rowRef),
                };
            }
            if (progressCallback) {
                progressCallback(static_cast<std::uint64_t>(blockRowEndIndex - blockRowBeginIndex));
            }
            continue;
        }

        std::vector<std::uint64_t> fallbackRows;
        fallbackRows.reserve(blockRowEndIndex - blockRowBeginIndex);
        for (std::size_t index = blockRowBeginIndex; index < blockRowEndIndex; ++index) {
            fallbackRows.push_back(includedRows[index].logicalRow);
        }
        std::vector<std::pair<int, FlitRecord>> decodedRows;
        if (!ClipboardDecodeRowsInChunks(session,
                                         fallbackRows,
                                         stopToken,
                                         progressCallback,
                                         decodedRows,
                                         errorText)) {
            rows.clear();
            return false;
        }
        if (stopToken.stop_requested()) {
            rows.clear();
            return true;
        }
        if (decodedRows.size() != fallbackRows.size()) {
            rows.clear();
            errorText = QStringLiteral("Trace row decoder returned fewer rows than requested.");
            return false;
        }
        for (std::size_t offset = 0; offset < decodedRows.size(); ++offset) {
            rows[blockRowBeginIndex + offset] = std::move(decodedRows[offset]);
        }
    }

    if (rowIndex < includedRows.size()) {
        rows.clear();
        errorText = QStringLiteral("Trace fast index did not cover all Clipboard insertion rows.");
        return false;
    }
    return true;
}

bool ClipboardModeMatchesXactionAddressAnchor(
    const XactionAddressAnchor& candidate,
    const XactionAddressAnchor& selectedAnchor,
    const int mode) noexcept
{
    if (candidate.lineAddress != selectedAnchor.lineAddress) {
        return false;
    }
    switch (mode) {
    case 0:
        return true;
    case 1:
        return candidate.requestLogicalRow > selectedAnchor.requestLogicalRow;
    case 2:
        return candidate.requestLogicalRow >= selectedAnchor.requestLogicalRow;
    }
    return false;
}

bool XactionAddressAnchorFromRows(const std::shared_ptr<TraceSession>& session,
                                  const std::vector<std::uint64_t>& logicalRows,
                                  const std::uint64_t ordinal,
                                  XactionAddressAnchor& anchor,
                                  CLogBTraceLoadError& error,
                                  std::stop_token stopToken)
{
    if (!session || logicalRows.empty()) {
        return false;
    }

    bool haveFallback = false;
    XactionAddressAnchor fallback;
    fallback.ordinal = ordinal;

    std::vector<std::uint64_t> sortedRows = logicalRows;
    std::sort(sortedRows.begin(), sortedRows.end());
    sortedRows.erase(std::unique(sortedRows.begin(), sortedRows.end()), sortedRows.end());

    for (const std::uint64_t logicalRow : sortedRows) {
        if (stopToken.stop_requested()) {
            return false;
        }

        std::vector<FlitRecord> rows;
        if (!session->readRowsDirect(logicalRow, 1, rows, error, {}, stopToken)) {
            return false;
        }
        if (rows.empty()) {
            continue;
        }

        std::uint64_t address = 0;
        if (!RowAddressValueForClipboard(rows.front(), address)) {
            continue;
        }

        XactionAddressAnchor candidate;
        candidate.ordinal = ordinal;
        candidate.requestLogicalRow = logicalRow;
        candidate.lineAddress = CLogBTraceLoader::normalizeCacheLineAddress(address);
        if (!haveFallback) {
            fallback = candidate;
            haveFallback = true;
        }
        if (rows.front().channel == FlitChannel::Req || rows.front().channel == FlitChannel::Snp) {
            anchor = candidate;
            return true;
        }
    }

    if (haveFallback) {
        anchor = fallback;
        return true;
    }
    return false;
}

QString XactionKeyForClipboardRecord(const FlitRecord& record)
{
    if (!record.xactionIndexChecked || !record.xactionIndexed) {
        return QString();
    }
    for (const QString& key : TransactionGroupKeys(record)) {
        if (key.startsWith(QStringLiteral("xaction|"))) {
            return key;
        }
    }
    return QString();
}

QString TimestampPlacementText(const FlitTableModel* model, const qint64 timestamp, const int count)
{
    const int logicalRow = TimestampInsertLogicalRow(model, timestamp);
    const QString countText = count == 1
        ? QStringLiteral("1 row")
        : QStringLiteral("%1 rows").arg(count);
    const bool hasRange = count > 1
        && timestamp <= (std::numeric_limits<qint64>::max)() - static_cast<qint64>(count - 1);
    const QString timestampText = hasRange
        ? QStringLiteral("%1..%2").arg(timestamp).arg(timestamp + static_cast<qint64>(count - 1))
        : QString::number(timestamp);
    return QStringLiteral("Logical row %1, %2 at timestamp %3. Equal timestamps are placed after existing rows.")
        .arg(logicalRow + 1)
        .arg(countText, timestampText);
}

class BuildFlitTemplateWizard final : public QWizard {
public:
    BuildFlitTemplateWizard(const CLogBTraceMetadata* metadata,
                            const qint64 defaultTimestamp,
                            std::function<QString(qint64, int)> placementTextProvider,
                            QWidget* parent)
        : QWizard(parent)
        , metadata_(metadata)
        , placementTextProvider_(std::move(placementTextProvider))
    {
        setWindowTitle(QStringLiteral("Build Flit Template"));
        setWizardStyle(QWizard::ModernStyle);
        setOption(QWizard::NoBackButtonOnStartPage, false);
        setOption(QWizard::NoCancelButtonOnLastPage, false);
        setButtonText(QWizard::FinishButton, QStringLiteral("Insert"));
        setMinimumSize(760, 560);

        addPage(createTemplatePage());
        valuesPageId_ = addPage(createValuesPage(defaultTimestamp));
        reviewPageId_ = addPage(createReviewPage());
        rebuildPreview();
    }

    std::vector<FlitRecord> takeRows()
    {
        return std::move(builtRows_);
    }

protected:
    void reject() override
    {
        closing_ = true;
        rebuildScheduled_ = false;
        QWizard::reject();
    }

    void initializePage(const int id) override
    {
        QWizard::initializePage(id);
        if (id == valuesPageId_) {
            rebuildPreview();
        } else if (id == reviewPageId_) {
            updateReview();
        }
    }

    bool validateCurrentPage() override
    {
        if (currentId() == valuesPageId_) {
            QString errorText;
            if (!buildRows(builtRows_, errorText)) {
                setValidationError(errorText);
                return false;
            }
            setValidationError(QString());
        } else if (currentId() == reviewPageId_) {
            QString errorText;
            if (!buildRows(builtRows_, errorText)) {
                reviewLabel_->setText(errorText);
                setValidationError(errorText);
                return false;
            }
        }
        return QWizard::validateCurrentPage();
    }

private:
    QWizardPage* createTemplatePage()
    {
        QWizardPage* page = new QWizardPage(this);
        page->setTitle(QStringLiteral("Template"));
        page->setSubTitle(QStringLiteral("Choose the channel and direction for the new flit row."));

        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(10);

        QFormLayout* form = new QFormLayout();
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        templateCombo_ = new QComboBox(page);
        for (const FlitTemplateChoice& choice : FlitTemplateChoices()) {
            templateCombo_->addItem(choice.label);
        }
        form->addRow(QStringLiteral("Template"), templateCombo_);

        placementEdit_ = new QLineEdit(page);
        placementEdit_->setReadOnly(true);
        placementEdit_->setObjectName(QStringLiteral("secondaryLineEdit"));
        form->addRow(QStringLiteral("Insert"), placementEdit_);

        layout->addLayout(form);
        layout->addStretch(1);

        connect(templateCombo_, &QComboBox::currentIndexChanged, this, [this]() {
            rebuildPreview();
        });
        return page;
    }

    QWizardPage* createValuesPage(const qint64 defaultTimestamp)
    {
        QWizardPage* page = new QWizardPage(this);
        page->setTitle(QStringLiteral("Values"));
        page->setSubTitle(QStringLiteral("Fill applicable fields before insertion."));

        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(10);

        QFormLayout* form = new QFormLayout();
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        timestampEdit_ = new QLineEdit(page);
        timestampEdit_->setText(QString::number(defaultTimestamp));
        timestampEdit_->setPlaceholderText(QStringLiteral("decimal or hexadecimal"));
        form->addRow(QStringLiteral("Timestamp"), timestampEdit_);

        countSpin_ = new QSpinBox(page);
        countSpin_->setRange(1, 1024);
        countSpin_->setValue(1);
        form->addRow(QStringLiteral("Rows"), countSpin_);
        layout->addLayout(form);

        fieldTable_ = new QTableWidget(page);
        fieldTable_->setColumnCount(3);
        fieldTable_->setHorizontalHeaderLabels({
            QStringLiteral("Field"),
            QStringLiteral("Value"),
            QStringLiteral("State"),
        });
        fieldTable_->verticalHeader()->setVisible(false);
        fieldTable_->horizontalHeader()->setStretchLastSection(false);
        fieldTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        fieldTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        fieldTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        fieldTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        fieldTable_->setAlternatingRowColors(true);
        layout->addWidget(fieldTable_, 1);

        errorLabel_ = new QLabel(page);
        errorLabel_->setWordWrap(true);
        errorLabel_->setStyleSheet(QStringLiteral("color:#9B1C1C; font-weight:600;"));
        errorLabel_->setVisible(false);
        layout->addWidget(errorLabel_);

        connect(timestampEdit_, &QLineEdit::textChanged, this, [this]() {
            rebuildPreview();
        });
        connect(countSpin_, &QSpinBox::valueChanged, this, [this]() {
            updatePlacementLabel();
        });
        return page;
    }

    QWizardPage* createReviewPage()
    {
        QWizardPage* page = new QWizardPage(this);
        page->setTitle(QStringLiteral("Review"));
        page->setSubTitle(QStringLiteral("Confirm the validated rows before insertion."));

        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(10);

        reviewLabel_ = new QLabel(page);
        reviewLabel_->setWordWrap(true);
        reviewLabel_->setFrameShape(QFrame::StyledPanel);
        reviewLabel_->setStyleSheet(QStringLiteral(
            "QLabel { background:#F7F9FB; border:1px solid #DDE3EA; border-radius:4px; padding:10px; color:#314252; }"));
        layout->addWidget(reviewLabel_);
        layout->addStretch(1);
        return page;
    }

    const FlitTemplateChoice& currentChoice() const
    {
        const int index = templateCombo_ ? templateCombo_->currentIndex() : 0;
        return FlitTemplateChoices()[static_cast<std::size_t>(
            qBound(0, index, static_cast<int>(FlitTemplateChoices().size()) - 1))];
    }

    bool makeBaseRow(FlitRecord& row, QString& errorText) const
    {
        if (!metadata_) {
            errorText = QStringLiteral("No CHI metadata is available for template validation.");
            return false;
        }
        if (!timestampEdit_ || timestampEdit_->text().trimmed().isEmpty()) {
            errorText = QStringLiteral("Timestamp is required.");
            return false;
        }

        const FlitTemplateChoice& choice = currentChoice();
        QString templateError;
        if (!FlitEditAdapter::makeTemplate(metadata_,
                                           choice.channel,
                                           choice.direction,
                                           0,
                                           row,
                                           &templateError)) {
            errorText = templateError.isEmpty()
                ? QStringLiteral("The selected flit template could not be created.")
                : templateError;
            return false;
        }

        const FlitEditResult timestampResult =
            FlitEditAdapter::applyEdit(row, metadata_, QStringLiteral("Time"), timestampEdit_->text());
        if (!timestampResult.ok) {
            errorText = EditResultMessage(timestampResult);
            return false;
        }
        return true;
    }

    bool applyEditedFieldValues(FlitRecord& row, QString& errorText) const
    {
        const auto applyOneField = [&](const QString& fieldName, const QLineEdit* editor) {
            if (!editor || !editor->isEnabled()) {
                return true;
            }

            const QString text = editor->text().trimmed();
            if (text.isEmpty()) {
                return true;
            }
            if (text == fieldInputDefaults_.value(fieldName).trimmed()) {
                return true;
            }

            const FlitEditResult result = FlitEditAdapter::applyEdit(row, metadata_, fieldName, text);
            if (!result.ok) {
                errorText = EditResultMessage(result);
                return false;
            }
            return true;
        };

        if (!applyOneField(QStringLiteral("Opcode"),
                           fieldInputs_.value(QStringLiteral("Opcode"), nullptr))) {
            return false;
        }

        for (auto it = fieldInputs_.cbegin(); it != fieldInputs_.cend(); ++it) {
            const QString fieldName = it.key();
            if (fieldName == QLatin1String("Opcode")) {
                continue;
            }
            if (!applyOneField(fieldName, it.value())) {
                return false;
            }
        }
        return true;
    }

    bool buildPreviewRow(FlitRecord& row, QString& errorText) const
    {
        if (!makeBaseRow(row, errorText)) {
            return false;
        }
        return applyEditedFieldValues(row, errorText);
    }

    bool buildRows(std::vector<FlitRecord>& rows, QString& errorText) const
    {
        rows.clear();

        FlitRecord row;
        if (!buildPreviewRow(row, errorText)) {
            return false;
        }

        const int count = countSpin_ ? countSpin_->value() : 1;
        if (count > 1
            && row.timestamp > (std::numeric_limits<qint64>::max)() - static_cast<qint64>(count - 1)) {
            errorText = QStringLiteral("Timestamp range would overflow.");
            return false;
        }

        rows.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            FlitRecord inserted = row;
            inserted.timestamp += index;
            rows.push_back(std::move(inserted));
        }
        return true;
    }

    void setValidationError(const QString& text)
    {
        if (!errorLabel_) {
            return;
        }
        errorLabel_->setText(text);
        errorLabel_->setVisible(!text.isEmpty());
    }

    void updatePlacementLabel()
    {
        if (!placementEdit_ || !placementTextProvider_) {
            return;
        }

        FlitRecord row;
        QString errorText;
        if (!makeBaseRow(row, errorText)) {
            placementEdit_->setText(QStringLiteral("Placement will be computed after the timestamp is valid."));
            return;
        }

        placementEdit_->setText(placementTextProvider_(row.timestamp, countSpin_ ? countSpin_->value() : 1));
    }

    bool canApplyOpcodeText(const QString& text, QString& errorText) const
    {
        if (text.trimmed().isEmpty()) {
            return true;
        }

        FlitRecord row;
        if (!makeBaseRow(row, errorText)) {
            return false;
        }

        const FlitEditResult opcodeResult =
            FlitEditAdapter::applyEdit(row, metadata_, QStringLiteral("Opcode"), text);
        if (!opcodeResult.ok) {
            errorText = EditResultMessage(opcodeResult);
            return false;
        }
        return true;
    }

    void scheduleRebuildPreview(const QString& focusedField)
    {
        if (closing_) {
            return;
        }
        focusedFieldName_ = focusedField;
        if (rebuildScheduled_) {
            return;
        }
        rebuildScheduled_ = true;
        QTimer::singleShot(0, this, [this]() {
            if (closing_) {
                return;
            }
            rebuildScheduled_ = false;
            rebuildPreview();
        });
    }

    void handleOpcodeTextChanged(QLineEdit* editor)
    {
        if (!editor || closing_) {
            return;
        }

        focusedFieldName_ = QStringLiteral("Opcode");
        focusedCursorPosition_ = editor->cursorPosition();

        QString errorText;
        if (!canApplyOpcodeText(editor->text(), errorText)) {
            setValidationError(errorText);
            return;
        }

        scheduleRebuildPreview(QStringLiteral("Opcode"));
    }

    void rebuildPreview()
    {
        if (!fieldTable_ || closing_) {
            return;
        }

        QHash<QString, QString> previousValues;
        for (auto it = fieldInputs_.cbegin(); it != fieldInputs_.cend(); ++it) {
            if (it.value() && it.value()->isEnabled()) {
                previousValues.insert(it.key(), it.value()->text());
            }
        }
        const QString focusedField = focusedFieldName_;

        fieldInputs_.clear();
        fieldInputDefaults_.clear();
        fieldTable_->setUpdatesEnabled(false);
        fieldTable_->clearContents();
        fieldTable_->setRowCount(0);

        FlitRecord row;
        QString errorText;
        if (!makeBaseRow(row, errorText)) {
            setValidationError(errorText);
            updatePlacementLabel();
            fieldTable_->setUpdatesEnabled(true);
            return;
        }
        updatePlacementLabel();

        const QString opcodeText = previousValues.value(QStringLiteral("Opcode")).trimmed();
        if (!opcodeText.isEmpty()) {
            const FlitEditResult opcodeResult =
                FlitEditAdapter::applyEdit(row, metadata_, QStringLiteral("Opcode"), opcodeText);
            if (!opcodeResult.ok) {
                setValidationError(EditResultMessage(opcodeResult));
            }
        }

        const QStringList fieldNames = FlitEditAdapter::templateFieldNames();
        fieldTable_->setRowCount(fieldNames.size());
        for (int rowIndex = 0; rowIndex < fieldNames.size(); ++rowIndex) {
            const QString fieldName = fieldNames[rowIndex];
            const FlitEditCellState state =
                FlitEditAdapter::cellState(row, metadata_, fieldName);

            QTableWidgetItem* nameItem = new QTableWidgetItem(fieldName);
            nameItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            fieldTable_->setItem(rowIndex, 0, nameItem);

            QTableWidgetItem* stateItem = new QTableWidgetItem(
                !state.applicable
                    ? QStringLiteral("Inapplicable")
                    : (state.editable ? QStringLiteral("Editable") : QStringLiteral("Fixed")));
            stateItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            if (!state.reason.isEmpty()) {
                stateItem->setToolTip(state.reason);
                nameItem->setToolTip(state.reason);
            }
            fieldTable_->setItem(rowIndex, 2, stateItem);

            if (state.editable) {
                QLineEdit* editor = new QLineEdit(fieldTable_);
                const QString defaultText = FieldDisplayText(row, fieldName);
                fieldInputDefaults_.insert(fieldName, defaultText);
                editor->setText(previousValues.contains(fieldName)
                                    ? previousValues.value(fieldName)
                                    : defaultText);
                editor->setPlaceholderText(QStringLiteral("decoded, decimal, or hexadecimal"));
                fieldInputs_.insert(fieldName, editor);
                connect(editor, &QLineEdit::editingFinished, this, [this, fieldName, editor]() {
                    focusedCursorPosition_ = editor->cursorPosition();
                    scheduleRebuildPreview(fieldName);
                });
                if (fieldName == QLatin1String("Opcode")) {
                    connect(editor, &QLineEdit::textChanged, this, [this, editor]() {
                        focusedCursorPosition_ = editor->cursorPosition();
                        handleOpcodeTextChanged(editor);
                    });
                }
                fieldTable_->setCellWidget(rowIndex, 1, editor);
            } else {
                QTableWidgetItem* valueItem = new QTableWidgetItem(FieldDisplayText(row, fieldName));
                valueItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                fieldTable_->setItem(rowIndex, 1, valueItem);
            }

            if (!state.applicable) {
                const QColor foreground(QStringLiteral("#8B949E"));
                const QColor background(QStringLiteral("#F1F3F5"));
                for (int column = 0; column < fieldTable_->columnCount(); ++column) {
                    if (QTableWidgetItem* item = fieldTable_->item(rowIndex, column)) {
                        item->setForeground(foreground);
                        item->setBackground(background);
                    }
                }
            } else if (state.fixed) {
                const QColor background(QStringLiteral("#F7F9FB"));
                for (int column = 0; column < fieldTable_->columnCount(); ++column) {
                    if (QTableWidgetItem* item = fieldTable_->item(rowIndex, column)) {
                        item->setBackground(background);
                    }
                }
            }
        }

        fieldTable_->setUpdatesEnabled(true);

        if (!focusedField.isEmpty()) {
            if (QLineEdit* editor = fieldInputs_.value(focusedField, nullptr)) {
                editor->setFocus(Qt::OtherFocusReason);
                editor->setCursorPosition(qBound(0, focusedCursorPosition_, editor->text().size()));
            }
        }
        focusedFieldName_.clear();
        focusedCursorPosition_ = 0;

        FlitRecord validatedRow;
        if (!buildPreviewRow(validatedRow, errorText)) {
            setValidationError(errorText);
            return;
        }
        setValidationError(QString());
    }

    void updateReview()
    {
        QString errorText;
        std::vector<FlitRecord> rows;
        if (!buildRows(rows, errorText) || rows.empty()) {
            reviewLabel_->setText(errorText.isEmpty()
                                      ? QStringLiteral("The flit template is not valid.")
                                      : errorText);
            return;
        }

        const FlitRecord& row = rows.front();
        const int count = countSpin_ ? countSpin_->value() : 1;
        reviewLabel_->setText(QStringLiteral("%1 %2\nTime: %3\nOpcode: %4\nRows: %5\nInsert: %6\n\nReady to insert.")
                                  .arg(ToString(row.channel),
                                       ToString(row.direction),
                                       QString::number(row.timestamp),
                                       FieldDisplayText(row, QStringLiteral("Opcode")),
                                       QString::number(count),
                                       placementTextProvider_
                                           ? placementTextProvider_(row.timestamp, count)
                                           : QStringLiteral("Computed by timestamp.")));
    }

private:
    const CLogBTraceMetadata* metadata_ = nullptr;
    std::function<QString(qint64, int)> placementTextProvider_;
    QComboBox* templateCombo_ = nullptr;
    QLineEdit* placementEdit_ = nullptr;
    QLineEdit* timestampEdit_ = nullptr;
    QSpinBox* countSpin_ = nullptr;
    QTableWidget* fieldTable_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QLabel* reviewLabel_ = nullptr;
    int valuesPageId_ = -1;
    int reviewPageId_ = -1;
    QHash<QString, QLineEdit*> fieldInputs_;
    QHash<QString, QString> fieldInputDefaults_;
    QString focusedFieldName_;
    int focusedCursorPosition_ = 0;
    bool rebuildScheduled_ = false;
    bool closing_ = false;
    std::vector<FlitRecord> builtRows_;
};

}  // namespace

MainWindow::TraceViewSession* MainWindow::activeTraceViewSession() noexcept
{
    return traceViewSessionById(activeTraceSessionId_);
}

const MainWindow::TraceViewSession* MainWindow::activeTraceViewSession() const noexcept
{
    return traceViewSessionById(activeTraceSessionId_);
}

MainWindow::TraceViewSession* MainWindow::traceViewSessionById(const quint64 sessionId) noexcept
{
    if (sessionId == 0) {
        return nullptr;
    }

    for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (session && session->id == sessionId) {
            return session.get();
        }
    }
    return nullptr;
}

const MainWindow::TraceViewSession* MainWindow::traceViewSessionById(const quint64 sessionId) const noexcept
{
    if (sessionId == 0) {
        return nullptr;
    }

    for (const std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (session && session->id == sessionId) {
            return session.get();
        }
    }
    return nullptr;
}

QString MainWindow::uniqueSessionLabel(const QString& requestedLabel,
                                       const TraceViewSession* ignoreSession) const
{
    const QString baseLabel = requestedLabel.trimmed().isEmpty()
        ? QStringLiteral("Untitled Trace")
        : requestedLabel.trimmed();

    auto labelTaken = [&](const QString& candidate) {
        for (const std::unique_ptr<TraceViewSession>& session : traceSessions_) {
            if (!session || session.get() == ignoreSession) {
                continue;
            }
            if (session->label == candidate) {
                return true;
            }
        }
        return false;
    };

    if (!labelTaken(baseLabel)) {
        return baseLabel;
    }

    for (int suffix = 2; suffix < 10000; ++suffix) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(baseLabel).arg(suffix);
        if (!labelTaken(candidate)) {
            return candidate;
        }
    }

    return QStringLiteral("%1 (%2)").arg(baseLabel).arg(nextTraceSessionId_);
}

MainWindow::TraceViewSession* MainWindow::addTraceViewSession(
    const QString& sourceLabel,
    const QString& sourcePath,
    std::optional<CLog::Parameters> parameterOverride)
{
    auto session = std::make_unique<TraceViewSession>();
    session->id = nextTraceSessionId_++;
    session->label = uniqueSessionLabel(sourceLabel);
    session->path = sourcePath;
    session->parameterOverride = std::move(parameterOverride);
    session->flitModel = new FlitTableModel(this);
    session->detailModel = new FlitDetailsModel(this);
    wireTraceModelSignals(session->flitModel, session->detailModel);
    createSessionVisualizationWidgets(*session);

    TraceViewSession* rawSession = session.get();
    traceSessions_.push_back(std::move(session));
    refreshSessionTabs();
    refreshLatencyDiffSessions();
    return rawSession;
}

void MainWindow::createSessionVisualizationWidgets(TraceViewSession& session)
{
    if (!session.timelineWidget && timelineStack_) {
        session.timelineWidget = new TimelineWidget(timelineStack_->parentWidget());
        timelineStack_->addWidget(session.timelineWidget);
        connect(session.timelineWidget, &TimelineWidget::logicalRowActivated, this,
                [this](const int logicalRow) {
                    jumpToLogicalTraceRow(logicalRow);
                });
    }
    if (!session.addressWidget && addressStack_) {
        session.addressWidget = new AddressWidget(addressStack_->parentWidget());
        addressStack_->addWidget(session.addressWidget);
        connect(session.addressWidget, &AddressWidget::logicalRowActivated, this,
                [this](const int logicalRow) {
                    jumpToLogicalTraceRow(logicalRow);
                });
    }
    if (!session.cacheWidget && cacheStack_) {
        session.cacheWidget = new CacheWidget(cacheStack_->parentWidget());
        cacheStack_->addWidget(session.cacheWidget);
        connect(session.cacheWidget, &CacheWidget::logicalRowActivated, this,
                [this](const int logicalRow) {
                    jumpToLogicalTraceRow(logicalRow);
                });
    }
    if (!session.latencyWidget && latencyStack_) {
        session.latencyWidget = new LatencyWidget(latencyStack_->parentWidget());
        latencyStack_->addWidget(session.latencyWidget);
        connect(session.latencyWidget, &LatencyWidget::logicalRowActivated, this,
                [this](const int logicalRow) {
                    jumpToLogicalTraceRow(logicalRow);
                });
    }
    if (!session.transactionWidget && transactionStack_) {
        session.transactionWidget = new TransactionWidget(transactionStack_->parentWidget());
        transactionStack_->addWidget(session.transactionWidget);
        connect(session.transactionWidget, &TransactionWidget::logicalRowActivated, this,
                [this](const int logicalRow) {
                    jumpToLogicalTraceRow(logicalRow);
                });
    }
}

void MainWindow::destroySessionVisualizationWidgets(TraceViewSession& session)
{
    if (session.timelineWidget) {
        if (timelineStack_) {
            timelineStack_->removeWidget(session.timelineWidget);
        }
        if (timelineWidget_ == session.timelineWidget) {
            timelineWidget_ = nullptr;
        }
        session.timelineWidget->deleteLater();
        session.timelineWidget = nullptr;
    }
    if (session.addressWidget) {
        if (addressStack_) {
            addressStack_->removeWidget(session.addressWidget);
        }
        if (addressWidget_ == session.addressWidget) {
            addressWidget_ = nullptr;
        }
        session.addressWidget->deleteLater();
        session.addressWidget = nullptr;
    }
    if (session.cacheWidget) {
        if (cacheStack_) {
            cacheStack_->removeWidget(session.cacheWidget);
        }
        if (cacheWidget_ == session.cacheWidget) {
            cacheWidget_ = nullptr;
        }
        session.cacheWidget->deleteLater();
        session.cacheWidget = nullptr;
    }
    if (session.latencyWidget) {
        if (latencyStack_) {
            latencyStack_->removeWidget(session.latencyWidget);
        }
        if (latencyWidget_ == session.latencyWidget) {
            latencyWidget_ = nullptr;
        }
        session.latencyWidget->deleteLater();
        session.latencyWidget = nullptr;
    }
    if (session.transactionWidget) {
        if (transactionStack_) {
            transactionStack_->removeWidget(session.transactionWidget);
        }
        if (transactionWidget_ == session.transactionWidget) {
            transactionWidget_ = nullptr;
        }
        session.transactionWidget->deleteLater();
        session.transactionWidget = nullptr;
    }
}

void MainWindow::populateSessionVisualizationWidgets(TraceViewSession& session)
{
    createSessionVisualizationWidgets(session);
    if (session.derivedViewsOutdated) {
        return;
    }

    if (session.traceSession) {
        if (session.timelineWidget) {
            session.timelineWidget->setTraceSession(session.traceSession);
        }
        if (session.addressWidget) {
            session.addressWidget->setTraceSession(session.traceSession);
        }
        if (session.cacheWidget) {
            session.cacheWidget->setTraceSession(session.traceSession);
        }
        if (session.latencyWidget) {
            session.latencyWidget->setTraceSession(session.traceSession);
        }
        if (session.transactionWidget) {
            session.transactionWidget->setTraceSession(session.traceSession);
        }
        return;
    }

    if (session.flitModel) {
        std::vector<FlitRecord> sourceRows = session.flitModel->sourceRowsSnapshot();
        if (session.timelineWidget) {
            session.timelineWidget->setRows(sourceRows);
        }
        if (session.latencyWidget) {
            session.latencyWidget->setRows(sourceRows);
        }
        if (session.transactionWidget) {
            session.transactionWidget->setRows(sourceRows);
        }
        if (session.cacheWidget) {
            session.cacheWidget->setRows({});
        }
        if (session.addressWidget) {
            session.addressWidget->setRows(std::move(sourceRows));
        }
    } else {
        if (session.timelineWidget) {
            session.timelineWidget->clear();
        }
        if (session.addressWidget) {
            session.addressWidget->clear();
        }
        if (session.cacheWidget) {
            session.cacheWidget->clear();
        }
        if (session.latencyWidget) {
            session.latencyWidget->clear();
        }
        if (session.transactionWidget) {
            session.transactionWidget->clear();
        }
    }
}

void MainWindow::activateSessionVisualizationWidgets(TraceViewSession* session)
{
    timelineWidget_ = session && session->timelineWidget ? session->timelineWidget : timelineEmptyWidget_;
    addressWidget_ = session && session->addressWidget ? session->addressWidget : addressEmptyWidget_;
    cacheWidget_ = session && session->cacheWidget ? session->cacheWidget : cacheEmptyWidget_;
    latencyWidget_ = session && session->latencyWidget ? session->latencyWidget : latencyEmptyWidget_;
    transactionWidget_ = session && session->transactionWidget ? session->transactionWidget : transactionEmptyWidget_;

    if (timelineStack_ && timelineWidget_) {
        timelineStack_->setCurrentWidget(timelineWidget_);
    }
    if (addressStack_ && addressWidget_) {
        addressStack_->setCurrentWidget(addressWidget_);
    }
    if (cacheStack_ && cacheWidget_) {
        cacheStack_->setCurrentWidget(cacheWidget_);
    }
    if (latencyStack_ && latencyWidget_) {
        latencyStack_->setCurrentWidget(latencyWidget_);
    }
    if (transactionStack_ && transactionWidget_) {
        transactionStack_->setCurrentWidget(transactionWidget_);
    }
    updateDerivedWidgetOutdatedTags();
}

void MainWindow::saveActiveSessionUiState()
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return;
    }
    if (clipboardWidget_) {
        clipboardWidget_->syncEntriesFromModel();
    }

    if (flitView_ && flitView_->selectionModel() && flitModel_) {
        const QModelIndex current = flitView_->selectionModel()->currentIndex();
        session->selectedLogicalRow = current.isValid()
            ? flitModel_->logicalRowAt(current.row())
            : -1;
        if (flitView_->verticalScrollBar()) {
            session->tableVerticalScrollValue = flitView_->verticalScrollBar()->value();
        }
        session->tableColumnWidths.clear();
        session->tableColumnWidths.reserve(flitModel_->columnCount());
        for (int column = 0; column < flitModel_->columnCount(); ++column) {
            session->tableColumnWidths.push_back(flitView_->columnWidth(column));
        }
    }
    if (timelineWidget_) {
        session->timelineViewState = timelineWidget_->viewState();
    }
    if (addressWidget_) {
        session->addressViewState = addressWidget_->viewState();
    }
    if (cacheWidget_) {
        session->cacheViewState = cacheWidget_->viewState();
    }
    if (latencyWidget_) {
        session->latencyViewState = latencyWidget_->viewState();
    }
    if (transactionWidget_) {
        session->transactionViewState = transactionWidget_->viewState();
    }
    session->topologySelectedNodeId.reset();
    if (topologyTable_) {
        const int currentRow = topologyTable_->currentRow();
        if (currentRow >= 0) {
            if (const QTableWidgetItem* item = topologyTable_->item(currentRow, 0)) {
                bool ok = false;
                const uint nodeId = item->text().toUInt(&ok);
                if (ok && nodeId <= std::numeric_limits<quint16>::max()) {
                    session->topologySelectedNodeId = static_cast<quint16>(nodeId);
                }
            }
        }
    }
    if (session->statisticsActive) {
        session->statisticsProgressText = statisticsLoadingFullText_;
    }
    if (session->xactionIndexActive) {
        session->xactionIndexProgressText = xactionIndexProgressLabel_ ? xactionIndexProgressLabel_->text() : QString();
    }
    session->optionalFieldIndexingFields = fieldIndexingFields_;
}

bool MainWindow::activateTraceSessionAt(const int index)
{
    if (index < 0 || index >= static_cast<int>(traceSessions_.size())) {
        return false;
    }

    const std::unique_ptr<TraceViewSession>& session = traceSessions_[static_cast<std::size_t>(index)];
    return session ? activateTraceSession(session->id) : false;
}

bool MainWindow::activateTraceSession(const quint64 sessionId)
{
    TraceViewSession* session = traceViewSessionById(sessionId);
    if (!session) {
        return false;
    }

    if (activeTraceSessionId_ == sessionId && flitModel_ == session->flitModel) {
        return true;
    }

    saveActiveSessionUiState();

    activeTraceSessionId_ = sessionId;
    bindActiveSessionUi();
    return true;
}

void MainWindow::bindActiveSessionUi()
{
    TraceViewSession* session = activeTraceViewSession();
    switchingTraceSession_ = true;
    activateSessionVisualizationWidgets(session);

    flitModel_ = session ? session->flitModel : flitModel_;
    detailModel_ = session ? session->detailModel : detailModel_;
    currentTraceSession_ = session ? session->traceSession : nullptr;
    currentTracePath_ = session ? session->path : QString();
    currentTraceLabel_ = session ? session->label : QString();
    currentTraceParameterOverride_ = session ? session->parameterOverride : std::nullopt;
    pendingTraceLoadPath_ = session ? session->pendingLoadPath : QString();
    fieldIndexingFields_ = session ? session->optionalFieldIndexingFields : QSet<QString>();
    statisticsActive_ = session ? session->statisticsActive : false;
    statisticsComputed_ = session ? session->statisticsComputed : false;
    statisticsGeneration_ = session ? session->statisticsGeneration : statisticsGeneration_;
    statisticsTraceSessionSource_ = session ? session->statisticsTraceSessionSource : nullptr;
    statisticsRowSource_.clear();
    statisticsLoadingFullText_ = session ? session->statisticsProgressText : QString();
    statisticsNextBlockIndex_ = session ? session->statisticsNextBlockIndex : 0;
    statisticsNextLocalIndex_ = session ? session->statisticsNextLocalIndex : 0;
    statisticsNextRowIndex_ = session ? session->statisticsNextRowIndex : 0;
    statisticsChunkRecords_ = session ? session->statisticsChunkRecords : 65536;
    xactionIndexActive_ = session ? session->xactionIndexActive : false;
    xactionIndexStopSource_ = session ? session->xactionIndexStopSource : nullptr;
    xactionIndexGeneration_ = session ? session->xactionIndexGeneration : xactionIndexGeneration_;

    if (flitView_ && flitModel_) {
        flitView_->setModel(flitModel_);
        if (traceCacheMinimap_) {
            traceCacheMinimap_->setFlitModel(flitModel_);
            traceCacheMinimap_->setTraceSession(currentTraceSession_);
        }
        wireTraceSelectionModel();
    }
    bindClipboardWidgetToActiveScope();
    if (detailView_ && detailModel_) {
        detailView_->setModel(detailModel_);
    }

    if (!session) {
        if (detailModel_) {
            detailModel_->setTraceSession(nullptr);
            detailModel_->setRecord(nullptr);
        }
        if (timelineEmptyWidget_) {
            timelineEmptyWidget_->clear();
        }
        if (addressEmptyWidget_) {
            addressEmptyWidget_->clear();
        }
        if (cacheEmptyWidget_) {
            cacheEmptyWidget_->clear();
        }
        if (latencyEmptyWidget_) {
            latencyEmptyWidget_->clear();
        }
        if (transactionEmptyWidget_) {
            transactionEmptyWidget_->clear();
        }
    } else if (session->traceSession) {
        if (detailModel_) {
            detailModel_->setTraceSession(session->traceSession);
        }
    } else if (flitModel_) {
        if (detailModel_) {
            detailModel_->setTraceSession(nullptr);
        }
    }

    auto setLineEditText = [](QLineEdit* edit, const QString& text) {
        if (!edit) {
            return;
        }
        const QSignalBlocker blocker(edit);
        edit->setText(text);
    };
    if (flitModel_) {
        setLineEditText(opcodeFilterEdit_, flitModel_->opcodeFilter());
        setLineEditText(sourceIdFilterEdit_, flitModel_->sourceIdFilter());
        setLineEditText(targetIdFilterEdit_, flitModel_->targetIdFilter());
        setLineEditText(txnIdFilterEdit_, flitModel_->txnIdFilter());
        setLineEditText(dbidFilterEdit_, flitModel_->dbidFilter());
        setLineEditText(addressFilterEdit_, flitModel_->addressFilter());
    }

    if (searchModeButton_ && flitModel_) {
        const QSignalBlocker blocker(searchModeButton_);
        const bool highlight = flitModel_->searchMode() == FlitTableModel::SearchMode::Highlight;
        searchModeButton_->setChecked(highlight);
        searchModeButton_->setText(highlight ? QStringLiteral("Highlight") : QStringLiteral("Filter"));
    }

    auto setToolButtonChecked = [](QToolButton* button, const bool checked) {
        if (!button) {
            return;
        }
        const QSignalBlocker blocker(button);
        button->setChecked(checked);
    };
    if (flitModel_) {
        setToolButtonChecked(reqButton_, flitModel_->channelVisible(FlitChannel::Req));
        setToolButtonChecked(rspButton_, flitModel_->channelVisible(FlitChannel::Rsp));
        setToolButtonChecked(datButton_, flitModel_->channelVisible(FlitChannel::Dat));
        setToolButtonChecked(snpButton_, flitModel_->channelVisible(FlitChannel::Snp));
        setToolButtonChecked(txButton_, flitModel_->directionVisible(FlitDirection::Tx));
        setToolButtonChecked(rxButton_, flitModel_->directionVisible(FlitDirection::Rx));
        setToolButtonChecked(deniedFlitsButton_, flitModel_->xactionDeniedOnlyFilter());
    }

    if (nodeLabelsButton_ && flitModel_) {
        setToolButtonChecked(nodeLabelsButton_, flitModel_->nodeLabelsVisible());
    }

    updateWindowCaption();
    rebuildFieldToggleDialog();
    rebuildOptionalFieldSearchRow();
    refreshNodeLabelDelegates();
    applyFlitColumnWidths();
    updateTraceEmptyState();
    updateTopologyAction();
    resetStatisticsView(false);
    if (session && session->statisticsComputed) {
        statisticsComputed_ = true;
        applyStatisticsResult(session->statisticsResult);
    } else if (session && session->statisticsActive) {
        updateStatisticsLoadingProgress(session->statisticsProgressText,
                                        session->statisticsProgressValue,
                                        session->statisticsProgressMaximum);
        if (statisticsStack_ && statisticsLoadingState_) {
            statisticsStack_->setCurrentWidget(statisticsLoadingState_);
        }
    }
    updateXactionIndexAction();
    updateEditActions();
    updateDerivedWidgetOutdatedTags();
    if (session && session->xactionIndexActive) {
        updateXactionIndexProgress(true,
                                   session->xactionIndexProgressText,
                                   session->xactionIndexCompletedRecords,
                                   session->xactionIndexTotalRecords);
    } else {
        updateXactionIndexProgress(false);
    }
    updateMetrics();
    applyTraceTableRowHeight();
    bindClipboardWidgetToActiveScope();
    updateSearchNavigationButtons();
    refreshTopologyView();
    refreshSessionTabs();

    if (flitView_ && flitModel_) {
        if (session && !session->tableColumnWidths.empty()) {
            const int columnCount = std::min<int>(flitModel_->columnCount(), session->tableColumnWidths.size());
            for (int column = 0; column < columnCount; ++column) {
                if (session->tableColumnWidths[column] > 0) {
                    flitView_->setColumnWidth(column, session->tableColumnWidths[column]);
                }
            }
        }
        const int visibleRow = session ? flitModel_->visibleRowForLogicalRow(session->selectedLogicalRow) : -1;
        if (visibleRow >= 0) {
            const QModelIndex target = flitModel_->index(visibleRow, 0);
            flitView_->selectionModel()->setCurrentIndex(target,
                                                         QItemSelectionModel::ClearAndSelect
                                                             | QItemSelectionModel::Rows);
            flitView_->selectRow(visibleRow);
            flitView_->scrollTo(target, QAbstractItemView::PositionAtCenter);
        } else {
            selectFirstRow();
        }
        if (session && flitView_->verticalScrollBar()) {
            flitView_->verticalScrollBar()->setValue(session->tableVerticalScrollValue);
        }
    }

    const QModelIndex current = flitView_ && flitView_->selectionModel()
        ? flitView_->selectionModel()->currentIndex()
        : QModelIndex();
    updateInspector(current.isValid() && flitModel_ ? flitModel_->recordAt(current.row()) : nullptr);
    updateTimelineSelection();
    updateAddressSelection();
    updateCacheSelection();
    updateTransactionSelection();
    if (session && timelineWidget_ && !session->timelineViewState.isEmpty()) {
        timelineWidget_->restoreViewState(session->timelineViewState);
    }
    if (session && addressWidget_ && !session->addressViewState.isEmpty()) {
        addressWidget_->restoreViewState(session->addressViewState);
    }
    if (session && cacheWidget_ && !session->cacheViewState.isEmpty()) {
        cacheWidget_->restoreViewState(session->cacheViewState);
    }
    if (session && latencyWidget_ && !session->latencyViewState.isEmpty()) {
        latencyWidget_->restoreViewState(session->latencyViewState);
    }
    if (session && transactionWidget_ && !session->transactionViewState.isEmpty()) {
        transactionWidget_->restoreViewState(session->transactionViewState);
    }

    switchingTraceSession_ = false;
    scheduleDiagnosticsSnapshotRefresh();
}

void MainWindow::refreshSessionTabs()
{
    if (!sessionTabBar_) {
        return;
    }

    const QSignalBlocker blocker(sessionTabBar_);
    while (sessionTabBar_->count() > 0) {
        sessionTabBar_->removeTab(sessionTabBar_->count() - 1);
    }
    int activeIndex = -1;
    for (std::size_t index = 0; index < traceSessions_.size(); ++index) {
        const TraceViewSession* session = traceSessions_[index].get();
        if (!session) {
            continue;
        }
        QString tabText = session->label;
        if (session->dirty) {
            tabText.append(QStringLiteral(" *"));
        }
        if (session->derivedViewsOutdated) {
            tabText.append(QStringLiteral(" [Outdated]"));
        }
        const int tabIndex = sessionTabBar_->addTab(tabText);
        sessionTabBar_->setTabData(tabIndex, QVariant::fromValue<qulonglong>(session->id));
        QString tooltip = session->path.isEmpty()
            ? session->label
            : QDir::toNativeSeparators(session->path);
        if (session->dirty) {
            tooltip.append(QStringLiteral("\nEdited copy has unsaved staged changes."));
        }
        if (session->derivedViewsOutdated) {
            tooltip.append(QStringLiteral("\nDerived views are outdated until rebuilt or reloaded."));
        }
        sessionTabBar_->setTabToolTip(tabIndex, tooltip);
        if (session->id == activeTraceSessionId_) {
            activeIndex = tabIndex;
        }
    }
    sessionTabBar_->setCurrentIndex(activeIndex);
    sessionTabBar_->setVisible(sessionTabBar_->count() > 1);
}

void MainWindow::showSessionTabContextMenu(const QPoint& position)
{
    if (!sessionTabBar_) {
        return;
    }

    const int tabIndex = sessionTabBar_->tabAt(position);
    if (tabIndex < 0 || tabIndex >= sessionTabBar_->count()) {
        return;
    }
    const quint64 sessionId =
        sessionTabBar_->tabData(tabIndex).value<qulonglong>();
    TraceViewSession* session = traceViewSessionById(sessionId);
    if (!session) {
        return;
    }

    QMenu menu(this);
    QAction* activateAction = menu.addAction(QStringLiteral("Activate Session"));
    activateAction->setEnabled(sessionId != activeTraceSessionId_);
    QAction* reloadAction = menu.addAction(QStringLiteral("Reload Session"));
    reloadAction->setEnabled(!session->path.isEmpty());
    menu.addSeparator();
    QAction* closeAction = menu.addAction(QStringLiteral("Close Session"));
    QAction* closeOthersAction = menu.addAction(QStringLiteral("Close Other Sessions"));
    closeOthersAction->setEnabled(traceSessions_.size() > 1);
    QAction* closeAllAction = menu.addAction(QStringLiteral("Close All Sessions"));

    QAction* selectedAction = menu.exec(sessionTabBar_->mapToGlobal(position));
    if (!selectedAction) {
        return;
    }
    if (selectedAction == activateAction) {
        activateTraceSession(sessionId);
    } else if (selectedAction == reloadAction) {
        saveActiveSessionUiState();
        loadTraceFile(session->path, session->parameterOverride, sessionId);
    } else if (selectedAction == closeAction) {
        closeTraceSession(sessionId);
    } else if (selectedAction == closeOthersAction) {
        closeOtherTraceSessions(sessionId);
    } else if (selectedAction == closeAllAction) {
        closeAllTraceSessions();
    }
}

void MainWindow::wireTraceModelSignals(FlitTableModel* model, FlitDetailsModel* detailModel)
{
    if (!model || !detailModel) {
        return;
    }

    connect(model, &QAbstractItemModel::modelReset, this, [this, model]() {
        if (model != flitModel_) {
            return;
        }
        refreshNodeLabelDelegates();
        updateMetrics();
        applyTraceTableRowHeight();
        selectFirstRow();
        const QModelIndex current = flitView_ && flitView_->selectionModel()
            ? flitView_->selectionModel()->currentIndex()
            : QModelIndex();
        updateXactionRowsTable(current.isValid() ? flitModel_->recordAt(current.row()) : nullptr);
        updateSearchNavigationButtons();
        updateTimelineSelection();
        updateAddressSelection();
        updateCacheSelection();
        updateTransactionSelection();
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(model, &QAbstractItemModel::dataChanged, this,
            [this, model](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                if (model != flitModel_) {
                    return;
                }
                if (roles.isEmpty() || roles.contains(Qt::BackgroundRole)) {
                    updateSearchNavigationButtons();
                }
                if (roles.isEmpty()
                    || roles.contains(Qt::DisplayRole)
                    || roles.contains(FlitTableModel::NodeLabelTextRole)
                    || roles.contains(FlitTableModel::NodeLabelColorRole)
                    || roles.contains(FlitTableModel::XactionIndexStateRole)) {
                    const QModelIndex current = flitView_ && flitView_->selectionModel()
                        ? flitView_->selectionModel()->currentIndex()
                        : QModelIndex();
                    updateXactionRowsTable(current.isValid() ? flitModel_->recordAt(current.row()) : nullptr);
                }
            });
    connect(detailModel, &QAbstractItemModel::modelReset, this, [this, detailModel]() {
        if (detailModel == detailModel_ && detailView_) {
            detailView_->resizeColumnsToContents();
        }
    });
    connect(model, &FlitTableModel::filteringProgressChanged, this,
            [this, model](const bool active, const QString& text, const int value, const int maximum) {
                if (model == flitModel_) {
                    updateFilterProgress(active, text, value, maximum);
                }
            });
    connect(model, &FlitTableModel::filteringFailed, this,
            [this, model](const QString& summary, const QString& detail) {
                if (model != flitModel_) {
                    return;
                }
                statusBar()->showMessage(summary, 8000);
                if (!detail.isEmpty()) {
                    QMessageBox::warning(this, QStringLiteral("Filtering Failed"), summary + QStringLiteral("\n\n") + detail);
                } else {
                    QMessageBox::warning(this, QStringLiteral("Filtering Failed"), summary);
                }
                scheduleDiagnosticsSnapshotRefresh();
            });
    connect(model, &FlitTableModel::searchHighlightNavigationChanged, this, [this, model]() {
        if (model == flitModel_) {
            updateSearchNavigationButtons();
        }
    });
    connect(model, &FlitTableModel::editRejected, this,
            [this, model](const QString& summary, const QString& detail) {
                if (model != flitModel_) {
                    return;
                }
                const QString message = detail.isEmpty()
                    ? summary
                    : summary + QStringLiteral("\n\n") + detail;
                QMessageBox::warning(this, QStringLiteral("Edit Rejected"), message);
            });
    connect(model, &FlitTableModel::dirtyChanged, this, [this, model](const bool dirty) {
        for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
            if (!session || session->flitModel != model) {
                continue;
            }
            session->dirty = dirty;
            refreshSessionTabs();
            updateMetrics();
            if (model == flitModel_) {
                updateEditActions();
            }
            scheduleDiagnosticsSnapshotRefresh();
            break;
        }
    });
    connect(model, &FlitTableModel::rowsEdited, this, [this, model]() {
        for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
            if (!session || session->flitModel != model) {
                continue;
            }
            markDerivedViewsOutdated(*session);
            if (model == flitModel_) {
                const QModelIndex current = flitView_ && flitView_->selectionModel()
                    ? flitView_->selectionModel()->currentIndex()
                    : QModelIndex();
                updateInspector(current.isValid() ? flitModel_->recordAt(current.row()) : nullptr);
            }
            break;
        }
    });
    connect(model, &FlitTableModel::undoRedoAvailabilityChanged, this, [this, model](bool, bool) {
        if (model == flitModel_) {
            scheduleDiagnosticsSnapshotRefresh();
        }
    });
}

void MainWindow::wireTraceSelectionModel()
{
    if (!flitView_ || !flitView_->selectionModel()) {
        return;
    }

    connect(flitView_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (!flitModel_ || !detailModel_) {
                    return;
                }
                if (!current.isValid()) {
                    detailModel_->setRecord(nullptr);
                    updateInspector(nullptr);
                    updateTimelineSelection();
                    updateAddressSelection();
                    updateCacheSelection();
                    updateTransactionSelection();
                    updateSearchNavigationButtons();
                    return;
                }
                if (currentTraceSession_) {
                    detailModel_->setLogicalRow(flitModel_->logicalRowAt(current.row()));
                } else {
                    detailModel_->setRecord(flitModel_->recordAt(current.row()));
                }
                if (TraceViewSession* session = activeTraceViewSession()) {
                    session->selectedLogicalRow = flitModel_->logicalRowAt(current.row());
                }
                updateInspector(flitModel_->recordAt(current.row()));
                updateTimelineSelection();
                updateAddressSelection();
                updateCacheSelection();
                updateTransactionSelection();
                updateSearchNavigationButtons();
                scheduleDiagnosticsSnapshotRefresh();
            });
}

void MainWindow::updateEditActions()
{
    const TraceViewSession* session = activeTraceViewSession();
    const bool hasSession = session != nullptr;
    if (readOnlyModeAction_) {
        const QSignalBlocker blocker(readOnlyModeAction_);
        readOnlyModeAction_->setEnabled(hasSession);
        readOnlyModeAction_->setChecked(!session || !session->editModeEnabled);
        readOnlyModeAction_->setText(session && session->editModeEnabled
                                         ? QStringLiteral("Editable")
                                         : QStringLiteral("Read-only"));
    }
    if (saveEditedCopyAction_) {
        saveEditedCopyAction_->setEnabled(hasSession && session->editModeEnabled && session->dirty);
    }
    if (insertBlankFlitAction_) {
        insertBlankFlitAction_->setEnabled(hasSession && session->editModeEnabled);
    }
    if (cloneFlitAction_) {
        cloneFlitAction_->setEnabled(hasSession && session->editModeEnabled && flitModel_ && flitModel_->visibleCount() > 0);
    }
    if (rebuildDerivedViewsAction_) {
        rebuildDerivedViewsAction_->setEnabled(hasSession && session->derivedViewsOutdated);
    }
}

void MainWindow::setActiveTraceReadOnly(const bool readOnly)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        updateEditActions();
        return;
    }

    if (readOnly) {
        session->editModeEnabled = false;
        if (session->flitModel) {
            session->flitModel->setEditable(false);
        }
        statusBar()->showMessage(QStringLiteral("Trace editing disabled."), 2500);
        updateMetrics();
        refreshSessionTabs();
        return;
    }

    if (!enableEditingForSession(*session)) {
        updateEditActions();
        return;
    }
    statusBar()->showMessage(QStringLiteral("Trace editing enabled. Edits are staged in memory."), 4000);
    updateMetrics();
    refreshSessionTabs();
}

bool MainWindow::enableEditingForSession(TraceViewSession& session)
{
    QMessageBox warning(this);
    warning.setIcon(QMessageBox::Warning);
    warning.setWindowTitle(QStringLiteral("Enable Trace Editing"));
    warning.setText(QStringLiteral("Turn read-only mode off for this trace?"));
    warning.setInformativeText(
        QStringLiteral("Edits are staged in the GUI. Derived views may become outdated. "
                       "Original trace files are never overwritten. Save edited data through "
                       "\"Save Edited Copy...\"."));
    warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    warning.setDefaultButton(QMessageBox::Cancel);
    if (warning.exec() != QMessageBox::Ok) {
        session.editModeEnabled = false;
        if (session.flitModel) {
            session.flitModel->setEditable(false);
        }
        return false;
    }

    if (!materializeSessionRowsForEditing(session)) {
        session.editModeEnabled = false;
        if (session.flitModel) {
            session.flitModel->setEditable(false);
        }
        return false;
    }

    session.editModeEnabled = true;
    if (session.flitModel) {
        session.flitModel->setEditable(true);
    }
    return true;
}

bool MainWindow::materializeSessionRowsForEditing(TraceViewSession& session)
{
    if (!session.flitModel) {
        return false;
    }

    if (session.traceSession) {
        session.editableMetadata = session.traceSession->metadata();
        session.flitModel->setTraceMetadataOverride(session.editableMetadata);
        return true;
    }

    if (session.editableMetadata) {
        session.flitModel->setTraceMetadataOverride(session.editableMetadata);
        return true;
    }

    QMessageBox::warning(this,
                         QStringLiteral("Editing Unavailable"),
                         QStringLiteral("This row-backed session has no CHI metadata for validation."));
    return false;
}

bool MainWindow::saveEditedCopy(TraceViewSession& session)
{
    if (!session.flitModel || !session.editableMetadata) {
        QMessageBox::warning(this,
                             QStringLiteral("Save Edited Copy"),
                             QStringLiteral("No editable trace data is available to save."));
        return false;
    }

    const QFileInfo originalInfo(session.path);
    const QString baseName = originalInfo.completeBaseName().isEmpty()
        ? session.label
        : originalInfo.completeBaseName();
    const QString defaultName = QStringLiteral("%1_edited.cldb").arg(baseName);
    const QString defaultDir = originalInfo.absolutePath().isEmpty()
        ? QDir::homePath()
        : originalInfo.absolutePath();
    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Edited Copy"),
        QDir(defaultDir).filePath(defaultName),
        QStringLiteral("CLog.B database (*.cldb);;CLog.B trace (*.clogb *.clog.b);;All files (*)"));
    if (selectedPath.isEmpty()) {
        return false;
    }

    const QFileInfo selectedInfo(selectedPath);
    const QString selectedCanonical = selectedInfo.exists()
        ? selectedInfo.canonicalFilePath()
        : selectedInfo.absoluteFilePath();
    const QString originalCanonical = originalInfo.exists()
        ? originalInfo.canonicalFilePath()
        : originalInfo.absoluteFilePath();
    if (!originalCanonical.isEmpty()
        && QString::compare(selectedCanonical, originalCanonical, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this,
                             QStringLiteral("Save Edited Copy"),
                             QStringLiteral("Choose a different path. The original trace is never overwritten."));
        return false;
    }

    CLogBTraceLoadError error;
    std::vector<FlitRecord> snapshotRows;
    std::uint64_t snapshotRowIndex = 0;
    std::uint64_t streamCursor = 0;
    const bool saved = session.flitModel->isSessionBacked()
        ? CLogBTraceLoader::saveRowsAsCLogB(
              selectedPath,
              *session.editableMetadata,
              static_cast<std::uint64_t>(session.flitModel->totalCount()),
              [model = session.flitModel, &streamCursor](const std::uint64_t rowIndex,
                                                         FlitRecord& record,
                                                         CLogBTraceLoadError& providerError) {
                  if (rowIndex != streamCursor) {
                      providerError.summary =
                          QStringLiteral("Edited rows must be saved sequentially.");
                      return false;
                  }
                  const FlitRecord* row = model->recordForLogicalRow(static_cast<int>(rowIndex));
                  if (!row) {
                      providerError.summary = QStringLiteral("Edited row %1 could not be found for saving.")
                                                  .arg(static_cast<qulonglong>(rowIndex + 1));
                      return false;
                  }
                  record = *row;
                  ++streamCursor;
                  return true;
              },
              error)
        : ([&]() {
              snapshotRows = session.flitModel->sourceRowsSnapshot();
              return CLogBTraceLoader::saveRowsAsCLogB(
                  selectedPath,
                  *session.editableMetadata,
                  static_cast<std::uint64_t>(snapshotRows.size()),
                  [&snapshotRows, &snapshotRowIndex](const std::uint64_t rowIndex,
                                                     FlitRecord& record,
                                                     CLogBTraceLoadError&) {
                      if (rowIndex != snapshotRowIndex || rowIndex >= snapshotRows.size()) {
                          return false;
                      }
                      record = snapshotRows[static_cast<std::size_t>(rowIndex)];
                      ++snapshotRowIndex;
                      return true;
                  },
                  error);
          })();
    if (!saved) {
        const QString message = error.informativeText.isEmpty()
            ? error.summary
            : error.summary + QStringLiteral("\n\n") + error.informativeText;
        QMessageBox::warning(this,
                             QStringLiteral("Save Edited Copy Failed"),
                             message.isEmpty()
                                 ? QStringLiteral("The edited trace copy could not be saved.")
                                 : message);
        return false;
    }

    session.flitModel->setDirty(false);
    session.flitModel->markUndoStackClean();
    session.dirty = false;
    statusBar()->showMessage(QStringLiteral("Saved edited trace copy to %1.")
                                 .arg(QDir::toNativeSeparators(selectedPath)),
                             6000);
    refreshSessionTabs();
    updateMetrics();
    updateEditActions();
    return true;
}

bool MainWindow::confirmDirtySessionAction(TraceViewSession& session, const QString& actionText)
{
    if (!session.dirty) {
        return true;
    }

    QMessageBox prompt(this);
    prompt.setIcon(QMessageBox::Warning);
    prompt.setWindowTitle(QStringLiteral("Unsaved Edited Trace"));
    prompt.setText(QStringLiteral("%1 has unsaved staged edits.").arg(session.label));
    prompt.setInformativeText(QStringLiteral("Save an edited copy before %1?").arg(actionText));
    QPushButton* saveButton = prompt.addButton(QStringLiteral("Save Edited Copy..."), QMessageBox::AcceptRole);
    QPushButton* discardButton = prompt.addButton(QStringLiteral("Discard"), QMessageBox::DestructiveRole);
    prompt.addButton(QMessageBox::Cancel);
    prompt.setDefaultButton(saveButton);
    prompt.exec();

    if (prompt.clickedButton() == saveButton) {
        return saveEditedCopy(session) && !session.dirty;
    }
    if (prompt.clickedButton() == discardButton) {
        session.dirty = false;
        if (session.flitModel) {
            session.flitModel->setDirty(false);
        }
        return true;
    }
    return false;
}

bool MainWindow::confirmAllDirtySessions(const QString& actionText)
{
    for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (!session) {
            continue;
        }
        if (!confirmDirtySessionAction(*session, actionText)) {
            return false;
        }
    }
    return true;
}

void MainWindow::markDerivedViewsOutdated(TraceViewSession& session)
{
    setDerivedViewsOutdated(session, true);
}

void MainWindow::setDerivedViewsOutdated(TraceViewSession& session, const bool outdated)
{
    if (session.derivedViewsOutdated == outdated) {
        return;
    }
    session.derivedViewsOutdated = outdated;
    refreshSessionTabs();
    updateMetrics();
    updateDerivedWidgetOutdatedTags();
    updateEditActions();
    scheduleDiagnosticsSnapshotRefresh();
}

void MainWindow::rebuildDerivedViewsForActiveSession()
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return;
    }
    session->derivedViewsOutdated = false;
    populateSessionVisualizationWidgets(*session);
    activateSessionVisualizationWidgets(session);
    refreshLatencyView();
    if (session->cacheWidget) {
        session->cacheWidget->buildView();
    }
    refreshTransactionView(*session);
    refreshSessionTabs();
    updateMetrics();
    updateDerivedWidgetOutdatedTags();
    statusBar()->showMessage(QStringLiteral("Rebuilt derived views from staged trace rows."), 3000);
}

void MainWindow::showFlitRowContextMenu(const QPoint& position)
{
    if (!flitView_ || !flitModel_) {
        return;
    }

    const QModelIndex index = flitView_->indexAt(position);
    if (index.isValid() && flitView_->selectionModel()) {
        flitView_->selectionModel()->setCurrentIndex(index,
                                                     QItemSelectionModel::ClearAndSelect
                                                         | QItemSelectionModel::Rows);
        flitView_->selectRow(index.row());
    }

    TraceViewSession* session = activeTraceViewSession();
    const bool editable = session && session->editModeEnabled;
    const FlitRecord* selectedRecord = index.isValid() ? flitModel_->recordAt(index.row()) : nullptr;
    const QString selectedXactionKey = selectedRecord
        ? XactionKeyForClipboardRecord(*selectedRecord)
        : QString();
    const bool canInsertXaction = index.isValid()
        && currentTraceSession_
        && currentTraceSession_->isXactionIndexComplete()
        && !selectedXactionKey.isEmpty();
    const bool canInsertXactionsByAddress = canInsertXaction;

    QMenu menu(this);
    QMenu* clipboardMenu = menu.addMenu(QStringLiteral("Clipboard"));
    QMenu* clipboardSingleMenu = clipboardMenu->addMenu(QStringLiteral("Insert Single Flit"));
    QAction* clipboardSingleSessionAction = clipboardSingleMenu->addAction(QStringLiteral("Session Clipboard"));
    QAction* clipboardSingleGlobalAction = clipboardSingleMenu->addAction(QStringLiteral("Global Clipboard"));
    clipboardSingleSessionAction->setEnabled(index.isValid() && session);
    clipboardSingleGlobalAction->setEnabled(index.isValid() && session);
    QMenu* clipboardXactionMenu = clipboardMenu->addMenu(QStringLiteral("Insert Transaction"));
    QAction* clipboardXactionSessionAction = clipboardXactionMenu->addAction(QStringLiteral("Session Clipboard"));
    QAction* clipboardXactionGlobalAction = clipboardXactionMenu->addAction(QStringLiteral("Global Clipboard"));
    clipboardXactionSessionAction->setEnabled(canInsertXaction);
    clipboardXactionGlobalAction->setEnabled(canInsertXaction);
    QMenu* clipboardAllAddressMenu =
        clipboardMenu->addMenu(QStringLiteral("Insert all Transactions with same Address"));
    QAction* clipboardAllAddressSessionAction =
        clipboardAllAddressMenu->addAction(QStringLiteral("Session Clipboard"));
    QAction* clipboardAllAddressGlobalAction =
        clipboardAllAddressMenu->addAction(QStringLiteral("Global Clipboard"));
    clipboardAllAddressSessionAction->setEnabled(canInsertXactionsByAddress && !clipboardXactionAddressInsertActive_);
    clipboardAllAddressGlobalAction->setEnabled(canInsertXactionsByAddress && !clipboardXactionAddressInsertActive_);
    QMenu* clipboardLaterAddressMenu =
        clipboardMenu->addMenu(QStringLiteral("Insert all later Transactions with same Address"));
    QAction* clipboardLaterAddressSessionAction =
        clipboardLaterAddressMenu->addAction(QStringLiteral("Session Clipboard"));
    QAction* clipboardLaterAddressGlobalAction =
        clipboardLaterAddressMenu->addAction(QStringLiteral("Global Clipboard"));
    clipboardLaterAddressSessionAction->setEnabled(canInsertXactionsByAddress && !clipboardXactionAddressInsertActive_);
    clipboardLaterAddressGlobalAction->setEnabled(canInsertXactionsByAddress && !clipboardXactionAddressInsertActive_);
    QMenu* clipboardThisAndLaterAddressMenu =
        clipboardMenu->addMenu(QStringLiteral("Insert this and all later Transactions with same Address"));
    QAction* clipboardThisAndLaterAddressSessionAction =
        clipboardThisAndLaterAddressMenu->addAction(QStringLiteral("Session Clipboard"));
    QAction* clipboardThisAndLaterAddressGlobalAction =
        clipboardThisAndLaterAddressMenu->addAction(QStringLiteral("Global Clipboard"));
    clipboardThisAndLaterAddressSessionAction->setEnabled(canInsertXactionsByAddress && !clipboardXactionAddressInsertActive_);
    clipboardThisAndLaterAddressGlobalAction->setEnabled(canInsertXactionsByAddress && !clipboardXactionAddressInsertActive_);
    menu.addSeparator();
    QMenu* insertMenu = menu.addMenu(QStringLiteral("Insert"));
    QAction* insertAfterAction = insertMenu->addAction(QStringLiteral("Insert after"));
    insertAfterAction->setEnabled(editable && index.isValid());
    QAction* deleteRowAction = menu.addAction(QStringLiteral("Delete row"));
    deleteRowAction->setShortcut(QKeySequence::Delete);
    deleteRowAction->setEnabled(editable && index.isValid());
    menu.addSeparator();
    QAction* undoAction = menu.addAction(flitModel_->undoText().isEmpty()
                                             ? QStringLiteral("Undo")
                                             : QStringLiteral("Undo %1").arg(flitModel_->undoText()));
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(editable && flitModel_->canUndo());
    QAction* redoAction = menu.addAction(flitModel_->redoText().isEmpty()
                                             ? QStringLiteral("Redo")
                                             : QStringLiteral("Redo %1").arg(flitModel_->redoText()));
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(editable && flitModel_->canRedo());

    QAction* selected = menu.exec(flitView_->viewport()->mapToGlobal(position));
    if (selected == clipboardSingleSessionAction) {
        insertSelectedFlitToClipboard(ClipboardScope::Session);
    } else if (selected == clipboardSingleGlobalAction) {
        insertSelectedFlitToClipboard(ClipboardScope::Global);
    } else if (selected == clipboardXactionSessionAction) {
        insertSelectedXactionToClipboard(ClipboardScope::Session);
    } else if (selected == clipboardXactionGlobalAction) {
        insertSelectedXactionToClipboard(ClipboardScope::Global);
    } else if (selected == clipboardAllAddressSessionAction) {
        insertXactionsWithSelectedAddressToClipboard(ClipboardScope::Session,
                                                     ClipboardXactionAddressInsertMode::All);
    } else if (selected == clipboardAllAddressGlobalAction) {
        insertXactionsWithSelectedAddressToClipboard(ClipboardScope::Global,
                                                     ClipboardXactionAddressInsertMode::All);
    } else if (selected == clipboardLaterAddressSessionAction) {
        insertXactionsWithSelectedAddressToClipboard(ClipboardScope::Session,
                                                     ClipboardXactionAddressInsertMode::Later);
    } else if (selected == clipboardLaterAddressGlobalAction) {
        insertXactionsWithSelectedAddressToClipboard(ClipboardScope::Global,
                                                     ClipboardXactionAddressInsertMode::Later);
    } else if (selected == clipboardThisAndLaterAddressSessionAction) {
        insertXactionsWithSelectedAddressToClipboard(ClipboardScope::Session,
                                                     ClipboardXactionAddressInsertMode::ThisAndLater);
    } else if (selected == clipboardThisAndLaterAddressGlobalAction) {
        insertXactionsWithSelectedAddressToClipboard(ClipboardScope::Global,
                                                     ClipboardXactionAddressInsertMode::ThisAndLater);
    } else if (selected == insertAfterAction) {
        insertBlankFlitTemplate();
    } else if (selected == deleteRowAction) {
        deleteSelectedFlitRow();
    } else if (selected == undoAction) {
        undoFlitTableEdit();
    } else if (selected == redoAction) {
        redoFlitTableEdit();
    }
}

void MainWindow::bindClipboardWidgetToActiveScope()
{
    if (!clipboardWidget_) {
        return;
    }
    const ClipboardScope scope = clipboardWidget_->scope();
    std::vector<ClipboardEntry>* entries = clipboardEntriesForScope(scope);
    std::optional<CLogBTraceMetadata> globalMetadata;
    const CLogBTraceMetadata* metadata = nullptr;
    if (scope == ClipboardScope::Session) {
        metadata = flitModel_ ? flitModel_->traceMetadata() : nullptr;
    } else if (entries) {
        globalMetadata = metadataForClipboardCLogBSave(*entries);
        metadata = globalMetadata ? &*globalMetadata : nullptr;
    }
    clipboardWidget_->setEntries(entries, metadata);
    clipboardWidget_->applyTraceTableRowStyle(flitModel_ ? flitModel_->visibleCount() : clipboardWidget_->model()->visibleCount());
    if (clipboardCacheMinimap_) {
        clipboardCacheMinimap_->setClipboardContext(
            clipboardWidget_,
            [this](const quint64 sessionId) -> std::shared_ptr<TraceSession> {
                const TraceViewSession* sourceSession = traceViewSessionById(sessionId);
                return sourceSession ? sourceSession->traceSession : nullptr;
            });
    }
}

std::vector<ClipboardEntry>* MainWindow::clipboardEntriesForScope(const ClipboardScope scope)
{
    if (scope == ClipboardScope::Global) {
        return &globalClipboardEntries_;
    }
    TraceViewSession* session = activeTraceViewSession();
    return session ? &session->clipboardEntries : nullptr;
}

const std::vector<ClipboardEntry>* MainWindow::clipboardEntriesForScope(const ClipboardScope scope) const
{
    if (scope == ClipboardScope::Global) {
        return &globalClipboardEntries_;
    }
    const TraceViewSession* session = activeTraceViewSession();
    return session ? &session->clipboardEntries : nullptr;
}

bool MainWindow::insertClipboardRows(const ClipboardScope scope,
                                     const std::vector<std::pair<int, FlitRecord>>& rows,
                                     QString* message,
                                     const ClipboardInsertOrdering ordering)
{
    return insertClipboardRowsDetailed(scope, rows, message, ordering).insertedAny();
}

MainWindow::ClipboardInsertResult MainWindow::insertClipboardRowsDetailed(
    const ClipboardScope scope,
    const std::vector<std::pair<int, FlitRecord>>& rows,
    QString* message,
    const ClipboardInsertOrdering ordering,
    const int preSkippedDuplicates)
{
    TraceViewSession* session = activeTraceViewSession();
    return session ? insertClipboardRowsFromSessionDetailed(session->id,
                                                           scope,
                                                           rows,
                                                           message,
                                                           ordering,
                                                           preSkippedDuplicates)
                   : ClipboardInsertResult{};
}

bool MainWindow::insertClipboardRowsFromSession(const quint64 sourceSessionId,
                                                const ClipboardScope scope,
                                                const std::vector<std::pair<int, FlitRecord>>& rows,
                                                QString* message,
                                                const ClipboardInsertOrdering ordering)
{
    return insertClipboardRowsFromSessionDetailed(sourceSessionId, scope, rows, message, ordering).insertedAny();
}

MainWindow::ClipboardInsertResult MainWindow::insertClipboardRowsFromSessionDetailed(
    const quint64 sourceSessionId,
    const ClipboardScope scope,
    const std::vector<std::pair<int, FlitRecord>>& rows,
    QString* message,
    const ClipboardInsertOrdering ordering,
    const int preSkippedDuplicates)
{
    TraceViewSession* sourceSession = traceViewSessionById(sourceSessionId);
    std::vector<ClipboardEntry>* target =
        scope == ClipboardScope::Global
            ? &globalClipboardEntries_
            : (sourceSession ? &sourceSession->clipboardEntries : nullptr);
    if (!sourceSession || !target || rows.empty()) {
        if (message) {
            if (preSkippedDuplicates > 0) {
                const QString scopeText = scope == ClipboardScope::Global
                    ? QStringLiteral("global Clipboard")
                    : QStringLiteral("session Clipboard");
                *message = QStringLiteral("Skipped %1 duplicate flit row%2 for the %3.")
                               .arg(preSkippedDuplicates)
                               .arg(preSkippedDuplicates == 1 ? QString() : QStringLiteral("s"))
                               .arg(scopeText);
            } else {
                *message = QStringLiteral("No flit rows are available for Clipboard insertion.");
            }
        }
        return ClipboardInsertResult{.inserted = 0, .skipped = preSkippedDuplicates};
    }

    if (clipboardWidget_ && clipboardWidget_->entries() == target) {
        clipboardWidget_->syncEntriesFromModel();
    }

    QSet<qulonglong> existingRows;
    existingRows.reserve(static_cast<int>(target->size() + rows.size()));
    for (const ClipboardEntry& entry : *target) {
        if (entry.source.sessionId == sourceSessionId && entry.source.logicalRow >= 0) {
            existingRows.insert(static_cast<qulonglong>(entry.source.logicalRow));
        }
    }

    ClipboardInsertResult result;
    result.skipped = std::max(0, preSkippedDuplicates);
    const std::size_t insertionStartIndex = target->size();
    for (const auto& [logicalRow, record] : rows) {
        const ClipboardSourceKey source{
            .sessionId = sourceSessionId,
            .logicalRow = logicalRow,
        };
        const qulonglong logicalRowKey = static_cast<qulonglong>(logicalRow);
        if (existingRows.contains(logicalRowKey)) {
            ++result.skipped;
            continue;
        }
        existingRows.insert(logicalRowKey);
        target->push_back(ClipboardEntry{
            .source = source,
            .sequence = nextClipboardSequence_++,
            .record = record,
            .originalRecord = record,
        });
        ++result.inserted;
    }

    OrderClipboardEntriesForInsertion(*target,
                                      sourceSessionId,
                                      static_cast<int>(ordering),
                                      insertionStartIndex);

    if (clipboardWidget_ && clipboardWidget_->scope() == scope) {
        std::optional<CLogBTraceMetadata> globalMetadata;
        const CLogBTraceMetadata* metadata = nullptr;
        if (scope == ClipboardScope::Session) {
            metadata = sourceSession->flitModel ? sourceSession->flitModel->traceMetadata() : nullptr;
        } else {
            globalMetadata = metadataForClipboardCLogBSave(*target);
            metadata = globalMetadata ? &*globalMetadata : nullptr;
        }
        clipboardWidget_->setEntries(target, metadata);
    }

    if (message) {
        const QString scopeText = scope == ClipboardScope::Global
            ? QStringLiteral("global Clipboard")
            : QStringLiteral("session Clipboard");
        if (result.inserted > 0 && result.skipped > 0) {
            *message = QStringLiteral("Inserted %1 flit row%2 into the %3; skipped %4 duplicate%5.")
                           .arg(result.inserted)
                           .arg(result.inserted == 1 ? QString() : QStringLiteral("s"))
                           .arg(scopeText)
                           .arg(result.skipped)
                           .arg(result.skipped == 1 ? QString() : QStringLiteral("s"));
        } else if (result.inserted > 0) {
            *message = QStringLiteral("Inserted %1 flit row%2 into the %3.")
                           .arg(result.inserted)
                           .arg(result.inserted == 1 ? QString() : QStringLiteral("s"))
                           .arg(scopeText);
        } else {
            *message = QStringLiteral("Skipped %1 duplicate flit row%2 for the %3.")
                           .arg(result.skipped)
                           .arg(result.skipped == 1 ? QString() : QStringLiteral("s"))
                           .arg(scopeText);
        }
    }
    return result;
}

void MainWindow::insertSelectedFlitToClipboard(const ClipboardScope scope)
{
    if (!flitView_ || !flitModel_ || !flitView_->selectionModel()) {
        return;
    }
    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    if (!current.isValid()) {
        return;
    }
    const int logicalRow = flitModel_->logicalRowAt(current.row());
    const FlitRecord* record = flitModel_->recordAt(current.row());
    if (logicalRow < 0 || !record) {
        return;
    }
    QString message;
    insertClipboardRows(scope, {{logicalRow, *record}}, &message);
    showClipboardDock();
    statusBar()->showMessage(message, 3000);
}

void MainWindow::insertSelectedXactionToClipboard(const ClipboardScope scope)
{
    if (!flitView_ || !flitModel_ || !flitView_->selectionModel() || !currentTraceSession_) {
        return;
    }
    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    const FlitRecord* record = current.isValid() ? flitModel_->recordAt(current.row()) : nullptr;
    if (!record || !currentTraceSession_->isXactionIndexComplete()) {
        return;
    }

    QString xactionKey;
    for (const QString& key : TransactionGroupKeys(*record)) {
        if (key.startsWith(QStringLiteral("xaction|"))) {
            xactionKey = key;
            break;
        }
    }
    if (xactionKey.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("The selected flit is not part of an indexed xaction."), 3000);
        return;
    }

    const std::vector<std::uint64_t> logicalRows = currentTraceSession_->xactionRowsForGroup(xactionKey);
    std::vector<std::pair<int, FlitRecord>> rows;
    rows.reserve(logicalRows.size());
    for (const std::uint64_t logicalRow64 : logicalRows) {
        if (logicalRow64 > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
            continue;
        }
        const int logicalRow = static_cast<int>(logicalRow64);
        if (const FlitRecord* relatedRecord = flitModel_->recordForLogicalRow(logicalRow)) {
            rows.push_back({logicalRow, *relatedRecord});
        }
    }

    QString message;
    insertClipboardRows(scope, rows, &message);
    showClipboardDock();
    statusBar()->showMessage(message, 3000);
}

bool MainWindow::insertXactionsWithSelectedAddressToClipboard(
    const ClipboardScope scope,
    const ClipboardXactionAddressInsertMode mode)
{
    if (!flitView_ || !flitModel_ || !flitView_->selectionModel() || !currentTraceSession_) {
        return false;
    }
    if (clipboardXactionAddressInsertActive_) {
        statusBar()->showMessage(QStringLiteral("A Clipboard transaction insertion is already running."),
                                 3000);
        return false;
    }
    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    const FlitRecord* record = current.isValid() ? flitModel_->recordAt(current.row()) : nullptr;
    if (!record || !currentTraceSession_->isXactionIndexComplete()) {
        statusBar()->showMessage(QStringLiteral("Build the xaction index before inserting transactions by address."),
                                 3000);
        return false;
    }

    const QString selectedXactionKey = XactionKeyForClipboardRecord(*record);
    if (selectedXactionKey.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("The selected flit is not part of an indexed xaction."), 3000);
        return false;
    }
    bool selectedOrdinalOk = false;
    const std::uint64_t selectedOrdinal =
        selectedXactionKey.sliced(QStringLiteral("xaction|").size()).toULongLong(&selectedOrdinalOk);

    if (!selectedOrdinalOk || selectedOrdinal == 0) {
        statusBar()->showMessage(QStringLiteral("The selected flit is not part of an indexed xaction."), 3000);
        return false;
    }

    TraceViewSession* sourceViewSession = activeTraceViewSession();
    if (!sourceViewSession) {
        return false;
    }
    const std::shared_ptr<TraceSession> session = currentTraceSession_;
    const quint64 sourceSessionId = sourceViewSession->id;
    std::unordered_set<std::uint64_t> existingClipboardRows;
    if (std::vector<ClipboardEntry>* targetEntries =
            scope == ClipboardScope::Global
                ? &globalClipboardEntries_
                : &sourceViewSession->clipboardEntries) {
        if (clipboardWidget_ && clipboardWidget_->entries() == targetEntries) {
            clipboardWidget_->syncEntriesFromModel();
        }
        existingClipboardRows.reserve(targetEntries->size());
        for (const ClipboardEntry& entry : *targetEntries) {
            if (entry.source.sessionId == sourceSessionId && entry.source.logicalRow >= 0) {
                existingClipboardRows.insert(static_cast<std::uint64_t>(entry.source.logicalRow));
            }
        }
    }
    auto stopSource = std::make_shared<std::stop_source>();
    const quint64 generation = ++clipboardXactionAddressInsertGeneration_;
    clipboardXactionAddressInsertActive_ = true;
    clipboardXactionAddressInsertSessionId_ = sourceSessionId;
    clipboardXactionAddressInsertScope_ = scope;
    clipboardXactionAddressInsertStopSource_ = stopSource;
    updateClipboardInsertProgress(true,
                                  QStringLiteral("Clipboard insert"),
                                  0,
                                  0);
    statusBar()->showMessage(QStringLiteral("Collecting same-address transactions for Clipboard..."),
                             3000);

    QPointer<MainWindow> window(this);
    std::thread worker([window,
                        session,
                        sourceSessionId,
                        scope,
                        mode,
                        selectedOrdinal,
                        existingClipboardRows = std::move(existingClipboardRows),
                        generation,
                        stopSource]() mutable {
        struct ProgressState {
            std::atomic<std::uint64_t> completed = 0;
            std::atomic<std::uint64_t> total = 0;
            std::atomic_bool flushPending = false;
            std::atomic<std::int64_t> lastPostMs = 0;
            std::mutex textMutex;
            QString text = QStringLiteral("Clipboard insert");
        };

        auto progressState = std::make_shared<ProgressState>();
        ClipboardXactionAddressBatchRows batchRows;
        QString errorText;
        bool cancelled = false;

        const auto postProgress = [&](const bool force = false) {
            constexpr std::int64_t kProgressPostIntervalMs = 100;
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            if (!force) {
                std::int64_t previousMs = progressState->lastPostMs.load(std::memory_order_relaxed);
                while (true) {
                    if (nowMs - previousMs < kProgressPostIntervalMs) {
                        return;
                    }
                    if (progressState->lastPostMs.compare_exchange_weak(previousMs,
                                                                        nowMs,
                                                                        std::memory_order_relaxed,
                                                                        std::memory_order_relaxed)) {
                        break;
                    }
                }
            } else {
                progressState->lastPostMs.store(nowMs, std::memory_order_relaxed);
            }

            bool expected = false;
            if (!progressState->flushPending.compare_exchange_strong(expected,
                                                                     true,
                                                                     std::memory_order_acq_rel,
                                                                     std::memory_order_relaxed)) {
                return;
            }
            if (window.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(window.data(),
                                      [window,
                                       generation,
                                       progressState]() {
                                          progressState->flushPending.store(false, std::memory_order_release);
                                          if (window.isNull()) {
                                              return;
                                          }
                                          MainWindow* mainWindow = window.data();
                                          if (!mainWindow->clipboardXactionAddressInsertActive_
                                              || generation != mainWindow->clipboardXactionAddressInsertGeneration_) {
                                              return;
                                          }
                                          const QString text = [&]() {
                                              const std::lock_guard lock(progressState->textMutex);
                                              return progressState->text;
                                          }();
                                          mainWindow->updateClipboardInsertProgress(
                                              true,
                                              text,
                                              progressState->completed.load(std::memory_order_relaxed),
                                              progressState->total.load(std::memory_order_relaxed));
                                      },
                                      Qt::QueuedConnection);
        };

        try {
            const std::vector<ClipboardXactionAddressSlice> slices =
                ClipboardXactionAddressSlices(session);
            const std::vector<ClipboardXactionAddressLogicalSlice> logicalSlices =
                ClipboardXactionAddressLogicalSlices(session);
            const std::uint64_t totalScanRows =
                static_cast<std::uint64_t>(std::min<std::uint64_t>(
                    session->totalRows(),
                    (std::numeric_limits<std::uint64_t>::max)()));
            progressState->total.store(totalScanRows,
                                       std::memory_order_relaxed);
            {
                const std::lock_guard lock(progressState->textMutex);
                progressState->text = QStringLiteral("Clipboard insert: scanning");
            }
            postProgress(true);

            CLogBTraceLoadError error;
            const std::stop_token stopToken = stopSource->get_token();
            ClipboardXactionAddressScanResult scanResult;
            std::atomic<std::uint64_t> scanCompletedRows = 0;
            const auto scanProgress = [&](const std::uint64_t completedRows) {
                const std::uint64_t total =
                    scanCompletedRows.fetch_add(completedRows, std::memory_order_relaxed)
                    + completedRows;
                progressState->completed.store(total, std::memory_order_relaxed);
                postProgress();
            };
            if (!ClipboardCollectFastXactionAnchors(session,
                                                    slices,
                                                    stopToken,
                                                    scanProgress,
                                                    scanResult,
                                                    error)) {
                errorText = error.informativeText.isEmpty()
                    ? error.summary
                    : error.summary + QStringLiteral(" ") + error.informativeText;
            }
            cancelled = stopToken.stop_requested();

            std::unordered_set<std::uint64_t> matchingOrdinals;
            if (!cancelled && errorText.isEmpty()) {
                XactionAddressAnchor selectedAnchor;
                const auto selectedFastAnchor = scanResult.anchorsByOrdinal.find(selectedOrdinal);
                if (selectedFastAnchor != scanResult.anchorsByOrdinal.end()) {
                    selectedAnchor = selectedFastAnchor->second;
                } else {
                    const std::vector<std::uint64_t> selectedRows =
                        session->xactionRowsForOrdinal(selectedOrdinal);
                    if (!XactionAddressAnchorFromRows(session,
                                                     selectedRows,
                                                     selectedOrdinal,
                                                     selectedAnchor,
                                                     error,
                                                     stopToken)) {
                        if (error.summary.isEmpty()) {
                            errorText = QStringLiteral("The selected transaction has no parseable request address.");
                        } else {
                            errorText = error.informativeText.isEmpty()
                                ? error.summary
                                : error.summary + QStringLiteral(" ") + error.informativeText;
                        }
                    }
                }

                if (!errorText.isEmpty()) {
                    cancelled = stopToken.stop_requested();
                }

                if (!cancelled && errorText.isEmpty()) {
                    for (const auto& [ordinal, anchor] : scanResult.anchorsByOrdinal) {
                        if (ClipboardModeMatchesXactionAddressAnchor(anchor,
                                                                     selectedAnchor,
                                                                     static_cast<int>(mode))) {
                            matchingOrdinals.insert(ordinal);
                        }
                    }

                    if (scanResult.indexedOrdinals.size() > scanResult.anchorsByOrdinal.size()) {
                        {
                            const std::lock_guard lock(progressState->textMutex);
                            progressState->text = QStringLiteral("Clipboard insert: fallback anchors");
                        }
                        progressState->completed.store(0, std::memory_order_relaxed);
                        progressState->total.store(
                            static_cast<std::uint64_t>(scanResult.indexedOrdinals.size()
                                                       - scanResult.anchorsByOrdinal.size()),
                            std::memory_order_relaxed);
                        postProgress(true);

                        std::uint64_t fallbackCompleted = 0;
                        for (const std::uint64_t ordinal : scanResult.indexedOrdinals) {
                            if (stopToken.stop_requested()) {
                                cancelled = true;
                                break;
                            }
                            if (scanResult.anchorsByOrdinal.find(ordinal)
                                != scanResult.anchorsByOrdinal.end()) {
                                continue;
                            }
                            const std::vector<std::uint64_t> xactionRows =
                                session->xactionRowsForOrdinal(ordinal);
                            XactionAddressAnchor candidate;
                            error = {};
                            if (XactionAddressAnchorFromRows(session,
                                                             xactionRows,
                                                             ordinal,
                                                             candidate,
                                                             error,
                                                             stopToken)) {
                                if (ClipboardModeMatchesXactionAddressAnchor(candidate,
                                                                             selectedAnchor,
                                                                             static_cast<int>(mode))) {
                                    matchingOrdinals.insert(ordinal);
                                }
                            } else if (!error.summary.isEmpty()) {
                                errorText = error.informativeText.isEmpty()
                                    ? error.summary
                                    : error.summary + QStringLiteral(" ") + error.informativeText;
                                break;
                            }
                            ++fallbackCompleted;
                            progressState->completed.store(fallbackCompleted, std::memory_order_relaxed);
                            postProgress();
                        }
                    }
                }
            }

            if (!cancelled && errorText.isEmpty()) {
                std::vector<ClipboardXactionAddressRowRef> includedRows;
                {
                    const std::lock_guard lock(progressState->textMutex);
                    progressState->text = QStringLiteral("Clipboard insert: collecting rows");
                }
                progressState->completed.store(0, std::memory_order_relaxed);
                progressState->total.store(totalScanRows, std::memory_order_relaxed);
                postProgress(true);

                std::atomic<std::uint64_t> collectCompletedRows = 0;
                const auto collectProgress = [&](const std::uint64_t completedRows) {
                    const std::uint64_t total =
                        collectCompletedRows.fetch_add(completedRows, std::memory_order_relaxed)
                        + completedRows;
                    progressState->completed.store(total, std::memory_order_relaxed);
                    postProgress();
                };
                if (!ClipboardCollectMatchingXactionRows(session,
                                                         logicalSlices,
                                                         matchingOrdinals,
                                                         existingClipboardRows,
                                                         stopToken,
                                                         collectProgress,
                                                         includedRows,
                                                         batchRows.preSkippedDuplicates,
                                                         errorText)) {
                    cancelled = stopToken.stop_requested();
                }

                if (!cancelled && errorText.isEmpty()) {
                    {
                        const std::lock_guard lock(progressState->textMutex);
                        progressState->text = QStringLiteral("Clipboard insert: materializing rows");
                    }
                    progressState->completed.store(0, std::memory_order_relaxed);
                    progressState->total.store(static_cast<std::uint64_t>(includedRows.size()),
                                               std::memory_order_relaxed);
                    postProgress(true);

                    std::atomic<std::uint64_t> readCompleted = 0;
                    const auto readProgress = [&](const std::uint64_t completedRows) {
                        const std::uint64_t total =
                            readCompleted.fetch_add(completedRows, std::memory_order_relaxed)
                            + completedRows;
                        progressState->completed.store(total, std::memory_order_relaxed);
                        postProgress();
                    };
                    if (!ClipboardMaterializeRowsFromFastIndex(session,
                                                               includedRows,
                                                               stopToken,
                                                               readProgress,
                                                               batchRows.rows,
                                                               errorText)) {
                        cancelled = stopToken.stop_requested();
                    }
                }
            }
        } catch (const std::bad_alloc&) {
            errorText = QStringLiteral("Clipboard transaction insertion ran out of memory.");
        } catch (const std::exception& exception) {
            errorText = QStringLiteral("Clipboard transaction insertion failed: %1")
                            .arg(QString::fromLocal8Bit(exception.what()));
        } catch (...) {
            errorText = QStringLiteral("Clipboard transaction insertion failed unexpectedly.");
        }

        cancelled = cancelled || stopSource->get_token().stop_requested();
        if (window.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(window.data(),
                                  [window,
                                   generation,
                                   sourceSessionId,
                                   scope,
                                   rows = std::move(batchRows.rows),
                                   preSkippedDuplicates = batchRows.preSkippedDuplicates,
                                   cancelled,
                                   errorText = std::move(errorText)]() mutable {
                                      if (window.isNull()) {
                                          return;
                                      }
                                      window->finishClipboardXactionAddressInsert(generation,
                                                                                  sourceSessionId,
                                                                                  scope,
                                                                                  std::move(rows),
                                                                                  preSkippedDuplicates,
                                                                                  cancelled,
                                                                                  std::move(errorText));
                                  },
                                  Qt::QueuedConnection);
    });
    worker.detach();
    return true;
}

void MainWindow::finishClipboardXactionAddressInsert(const quint64 generation,
                                                     const quint64 sourceSessionId,
                                                     const ClipboardScope scope,
                                                     std::vector<std::pair<int, FlitRecord>> rows,
                                                     const int preSkippedDuplicates,
                                                     const bool cancelled,
                                                     QString errorText)
{
    if (generation != clipboardXactionAddressInsertGeneration_
        || sourceSessionId != clipboardXactionAddressInsertSessionId_) {
        return;
    }

    const bool stopAlreadyRequested =
        clipboardXactionAddressInsertStopSource_
        && clipboardXactionAddressInsertStopSource_->stop_requested();
    clipboardXactionAddressInsertActive_ = false;
    clipboardXactionAddressInsertSessionId_ = 0;
    clipboardXactionAddressInsertStopSource_.reset();
    updateClipboardInsertProgress(false);

    if (cancelled || stopAlreadyRequested) {
        statusBar()->showMessage(QStringLiteral("Clipboard transaction insertion cancelled."), 3000);
        return;
    }
    if (!errorText.isEmpty()) {
        statusBar()->showMessage(errorText, 6000);
        return;
    }

    TraceViewSession* sourceSession = traceViewSessionById(sourceSessionId);
    if (!sourceSession) {
        statusBar()->showMessage(QStringLiteral("The source trace session for this Clipboard insertion is no longer open."),
                                 4500);
        return;
    }

    QString message;
    insertClipboardRowsFromSessionDetailed(sourceSessionId,
                                           scope,
                                           rows,
                                           &message,
                                           ClipboardInsertOrdering::PreserveInputOrder,
                                           preSkippedDuplicates);
    showClipboardDock();
    statusBar()->showMessage(message, 3000);
}

void MainWindow::cancelClipboardXactionAddressInsert()
{
    if (clipboardXactionAddressInsertStopSource_) {
        clipboardXactionAddressInsertStopSource_->request_stop();
    }
    if (clipboardXactionAddressInsertActive_) {
        if (clipboardInsertCancelButton_) {
            clipboardInsertCancelButton_->setEnabled(false);
        }
        updateClipboardInsertProgress(true,
                                      QStringLiteral("Clipboard insert: cancelling"),
                                      0,
                                      0);
        statusBar()->showMessage(QStringLiteral("Cancelling Clipboard transaction insertion..."), 2000);
    }
}

void MainWindow::cancelClipboardXactionAddressInsertForSession(const quint64 sessionId)
{
    if (clipboardXactionAddressInsertActive_
        && clipboardXactionAddressInsertSessionId_ == sessionId) {
        cancelClipboardXactionAddressInsert();
    }
}

void MainWindow::showClipboardDock()
{
    if (!clipboardDock_) {
        return;
    }
    clipboardDock_->toggleView(true);
    clipboardDock_->raise();
    clipboardDock_->setAsCurrentTab();
}

void MainWindow::handleClipboardRowActivated(const ClipboardEntry& entry)
{
    if (!clipboardWidget_ || !clipboardWidget_->readOnly()) {
        return;
    }
    if (!FlitRecordsEqualForClipboard(entry.record, entry.originalRecord)) {
        statusBar()->showMessage(QStringLiteral("Modified Clipboard rows cannot jump to the original trace row."), 3500);
        return;
    }
    if (entry.source.sessionId == 0 || entry.source.logicalRow < 0) {
        statusBar()->showMessage(QStringLiteral("This Clipboard row has no source trace row."), 3500);
        return;
    }

    TraceViewSession* sourceSession = traceViewSessionById(entry.source.sessionId);
    if (!sourceSession) {
        statusBar()->showMessage(QStringLiteral("The source trace session for this Clipboard row is no longer open."), 3500);
        return;
    }

    if (activeTraceSessionId_ != sourceSession->id && !activateTraceSession(sourceSession->id)) {
        statusBar()->showMessage(QStringLiteral("The source trace session could not be activated."), 3500);
        return;
    }
    jumpToLogicalTraceRow(entry.source.logicalRow);
}

bool MainWindow::saveClipboardAsCsv(const QString& path, const std::vector<ClipboardEntry>& entries)
{
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             QStringLiteral("Save Clipboard"),
                             QStringLiteral("Unable to create %1.").arg(QFileInfo(path).fileName()));
        return false;
    }

    QTextStream stream(&output);
    QSet<QString> usedHeaders;
    QStringList headers = ClipboardCsvFixedHeaders();
    for (const QString& header : headers) {
        usedHeaders.insert(header);
    }
    const std::vector<ClipboardCsvFieldColumn> fieldColumns = ClipboardCsvFieldColumns(entries, usedHeaders);
    for (const ClipboardCsvFieldColumn& column : fieldColumns) {
        headers << column.valueHeader;
        if (column.includeRaw) {
            headers << column.rawHeader;
        }
    }

    stream << CsvEscapedCells(headers).join(QLatin1Char(',')) << '\n';
    for (const ClipboardEntry& entry : entries) {
        QStringList cells;
        cells.reserve(15 + static_cast<int>(fieldColumns.size() * 2));
        cells << CsvCellForColumn(entry.record, FlitTableModel::TimeColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::ChannelColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::DirectionColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::OpcodeColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::SourceColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::TargetColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::TxnColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::AddressColumn);
        cells << entry.record.dbid;
        cells << CsvCellForColumn(entry.record, FlitTableModel::DataIdColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::RespColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::FwdStateColumn);
        cells << CsvCellForColumn(entry.record, FlitTableModel::RespErrColumn);
        cells << entry.record.summary;
        cells << entry.record.annotation;

        for (const ClipboardCsvFieldColumn& column : fieldColumns) {
            const FieldEntry* field = ClipboardCsvFindField(entry.record, column);
            cells << (field ? field->value : QString());
            if (column.includeRaw) {
                cells << (field ? field->raw : QString());
            }
        }

        stream << CsvEscapedCells(cells).join(QLatin1Char(',')) << '\n';
    }

    if (!output.commit()) {
        QMessageBox::warning(this,
                             QStringLiteral("Save Clipboard"),
                             QStringLiteral("Writing %1 failed.").arg(QFileInfo(path).fileName()));
        return false;
    }
    statusBar()->showMessage(QStringLiteral("Saved Clipboard CSV to %1.")
                                 .arg(QDir::toNativeSeparators(path)),
                             6000);
    return true;
}

std::optional<CLogBTraceMetadata> MainWindow::metadataForClipboardCLogBSave(const std::vector<ClipboardEntry>& entries) const
{
    if (entries.empty()) {
        return std::nullopt;
    }

    std::optional<CLogBTraceMetadata> merged;
    for (const ClipboardEntry& entry : entries) {
        if (!entry.record.rawRecordAvailable
            || entry.record.rawFlitLength == 0
            || entry.record.rawFlitWords.empty()) {
            return std::nullopt;
        }

        const TraceViewSession* session = traceViewSessionById(entry.source.sessionId);
        if (!session) {
            return std::nullopt;
        }
        const CLogBTraceMetadata* metadata = nullptr;
        if (session->flitModel && session->flitModel->traceMetadata()) {
            metadata = session->flitModel->traceMetadata();
        } else if (session->traceSession) {
            metadata = &session->traceSession->metadata();
        } else if (session->editableMetadata) {
            metadata = &*session->editableMetadata;
        }
        if (!metadata) {
            return std::nullopt;
        }

        if (!merged) {
            merged = *metadata;
            merged->blocks.clear();
            merged->totalRecords = static_cast<std::uint64_t>(entries.size());
            merged->firstTimestamp = entries.front().record.timestamp;
            merged->lastTimestamp = entries.back().record.timestamp;
            merged->channelCounts = {};
        } else {
            if (!ParametersEqual(merged->parameters, metadata->parameters)) {
                return std::nullopt;
            }
            for (const auto& [nodeId, nodeType] : metadata->nodeTypes) {
                const auto found = merged->nodeTypes.find(nodeId);
                if (found != merged->nodeTypes.end() && found->second != nodeType) {
                    return std::nullopt;
                }
                merged->nodeTypes.insert_or_assign(nodeId, nodeType);
            }
            for (const auto& [nodeId, annotation] : metadata->nodeAnnotations) {
                merged->nodeAnnotations.emplace(nodeId, annotation);
            }
        }
    }
    return merged;
}

bool MainWindow::saveClipboardAsCLogB(const QString& path, const std::vector<ClipboardEntry>& entries)
{
    std::optional<CLogBTraceMetadata> metadata = metadataForClipboardCLogBSave(entries);
    if (!metadata) {
        QMessageBox::warning(this,
                             QStringLiteral("Save Clipboard"),
                             QStringLiteral("CLog.B save is available only when all Clipboard rows have compatible live source metadata and raw CLog.B payloads."));
        return false;
    }

    CLogBTraceLoadError error;
    std::uint64_t rowIndex = 0;
    const bool saved = CLogBTraceLoader::saveRowsAsCLogB(
        path,
        *metadata,
        static_cast<std::uint64_t>(entries.size()),
        [&entries, &rowIndex](const std::uint64_t requestedRow,
                              FlitRecord& record,
                              CLogBTraceLoadError&) {
            if (requestedRow != rowIndex || requestedRow >= entries.size()) {
                return false;
            }
            record = entries[static_cast<std::size_t>(requestedRow)].record;
            ++rowIndex;
            return true;
        },
        error);
    if (!saved) {
        const QString message = error.informativeText.isEmpty()
            ? error.summary
            : error.summary + QStringLiteral("\n\n") + error.informativeText;
        QMessageBox::warning(this,
                             QStringLiteral("Save Clipboard Failed"),
                             message.isEmpty()
                                 ? QStringLiteral("The Clipboard CLog.B file could not be saved.")
                                 : message);
        return false;
    }
    statusBar()->showMessage(QStringLiteral("Saved Clipboard CLog.B to %1.")
                                 .arg(QDir::toNativeSeparators(path)),
                             6000);
    return true;
}

void MainWindow::saveClipboard()
{
    if (!clipboardWidget_) {
        return;
    }
    clipboardWidget_->syncEntriesFromModel();
    const std::vector<ClipboardEntry> entries = clipboardWidget_->currentEntriesSnapshot();
    if (entries.empty()) {
        QMessageBox::information(this,
                                 QStringLiteral("Save Clipboard"),
                                 QStringLiteral("The current Clipboard is empty."));
        return;
    }

    const bool canSaveCLogB = metadataForClipboardCLogBSave(entries).has_value();
    const QString filter = canSaveCLogB
        ? QStringLiteral("CSV table (*.csv);;CLog.B database (*.cldb);;CLog.B trace (*.clogb *.clog.b);;All files (*)")
        : QStringLiteral("CSV table (*.csv);;All files (*)");
    QString selectedFilter = QStringLiteral("CSV table (*.csv)");
    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Clipboard"),
        QDir(QDir::homePath()).filePath(QStringLiteral("clipboard.csv")),
        filter,
        &selectedFilter);
    if (selectedPath.isEmpty()) {
        return;
    }

    const bool wantsCLogB = selectedFilter.contains(QStringLiteral("CLog.B"), Qt::CaseInsensitive)
        || selectedPath.endsWith(QStringLiteral(".cldb"), Qt::CaseInsensitive)
        || selectedPath.endsWith(QStringLiteral(".clogb"), Qt::CaseInsensitive)
        || selectedPath.endsWith(QStringLiteral(".clog.b"), Qt::CaseInsensitive);
    if (wantsCLogB && canSaveCLogB) {
        saveClipboardAsCLogB(selectedPath, entries);
    } else {
        saveClipboardAsCsv(selectedPath, entries);
    }
}

void MainWindow::insertBlankFlitTemplate()
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || !session->editModeEnabled || !flitModel_) {
        return;
    }
    const CLogBTraceMetadata* metadata = flitModel_->traceMetadata();
    if (!metadata) {
        QMessageBox::warning(this,
                             QStringLiteral("Insert Flit"),
                             QStringLiteral("No CHI metadata is available for template validation."));
        return;
    }

    const QModelIndex current = flitView_ && flitView_->selectionModel()
        ? flitView_->selectionModel()->currentIndex()
        : QModelIndex();
    qint64 timestamp = 0;
    if (current.isValid()) {
        if (const FlitRecord* row = flitModel_->recordAt(current.row())) {
            timestamp = row->timestamp + 1;
        }
    }

    const auto placementTextProvider = [model = flitModel_](const qint64 candidateTimestamp, const int count) {
        return TimestampPlacementText(model, candidateTimestamp, count);
    };

    BuildFlitTemplateWizard wizard(metadata, timestamp, placementTextProvider, this);
    if (wizard.exec() != QDialog::Accepted) {
        return;
    }

    std::vector<FlitRecord> rows = wizard.takeRows();
    if (rows.empty()) {
        return;
    }

    flitModel_->insertRowsByTimestamp(std::move(rows));
}

void MainWindow::cloneSelectedFlitRows()
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || !session->editModeEnabled || !flitModel_ || !flitView_ || !flitView_->selectionModel()) {
        return;
    }

    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    if (!current.isValid()) {
        return;
    }

    bool ok = false;
    const int count = QInputDialog::getInt(this,
                                           QStringLiteral("Clone Selected Flit"),
                                           QStringLiteral("Rows"),
                                           1,
                                           1,
                                           1024,
                                           1,
                                           &ok);
    if (!ok) {
        return;
    }
    flitModel_->cloneRowsAt(current.row(), count);
}

void MainWindow::deleteSelectedFlitRow()
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || !session->editModeEnabled || !flitModel_ || !flitView_ || !flitView_->selectionModel()) {
        return;
    }

    const QModelIndex current = flitView_->selectionModel()->currentIndex();
    if (!current.isValid()) {
        return;
    }

    flitModel_->deleteRowAt(current.row());
}

void MainWindow::undoFlitTableEdit()
{
    const QWidget* focusWidget = QApplication::focusWidget();
    const TraceViewSession* session = activeTraceViewSession();
    if (!session || !session->editModeEnabled || !flitModel_ || !flitView_
        || (focusWidget != flitView_ && focusWidget != flitView_->viewport())) {
        return;
    }
    flitModel_->undoEdit();
}

void MainWindow::redoFlitTableEdit()
{
    const QWidget* focusWidget = QApplication::focusWidget();
    const TraceViewSession* session = activeTraceViewSession();
    if (!session || !session->editModeEnabled || !flitModel_ || !flitView_
        || (focusWidget != flitView_ && focusWidget != flitView_->viewport())) {
        return;
    }
    flitModel_->redoEdit();
}

void MainWindow::resetNoSessionState()
{
    updateXactionIndexProgress(false);
    cancelClipboardXactionAddressInsert();
    activeTraceSessionId_ = 0;
    activateSessionVisualizationWidgets(nullptr);
    refreshSessionTabs();
    pendingTraceLoadPath_.clear();
    pendingTraceLoadPaths_.clear();
    currentTraceSession_.reset();
    detailModel_->setTraceSession(nullptr);
    if (timelineEmptyWidget_) {
        timelineEmptyWidget_->clear();
    }
    if (addressEmptyWidget_) {
        addressEmptyWidget_->clear();
    }
    if (latencyEmptyWidget_) {
        latencyEmptyWidget_->clear();
    }
    if (transactionEmptyWidget_) {
        transactionEmptyWidget_->clear();
    }
    refreshLatencyDiffSessions();
    currentTracePath_.clear();
    currentTraceLabel_.clear();
    currentTraceParameterOverride_.reset();
    if (flitModel_) {
        flitModel_->clear();
    }
    bindClipboardWidgetToActiveScope();
    if (detailModel_) {
        detailModel_->setTraceSession(nullptr);
        detailModel_->setRecord(nullptr);
    }
    updateWindowCaption();
    rebuildFieldToggleDialog();
    rebuildOptionalFieldSearchRow();
    refreshNodeLabelDelegates();
    updateTraceEmptyState();
    updateTopologyAction();
    resetStatisticsView(false);
    updateXactionIndexAction();
    statusBar()->showMessage(QStringLiteral("No trace file is currently open."), 3000);
    refreshDiagnosticsSnapshot();
}

void MainWindow::closeActiveTraceSession()
{
    const TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return;
    }
    closeTraceSession(session->id);
}

void MainWindow::closeTraceSession(const quint64 sessionId)
{
    const auto found = std::find_if(traceSessions_.begin(),
                                    traceSessions_.end(),
                                    [sessionId](const std::unique_ptr<TraceViewSession>& session) {
                                        return session && session->id == sessionId;
                                    });
    if (found == traceSessions_.end()) {
        return;
    }
    if (!confirmDirtySessionAction(**found, QStringLiteral("closing the session"))) {
        return;
    }

    const std::size_t removedIndex = static_cast<std::size_t>(std::distance(traceSessions_.begin(), found));
    const bool closingActive = activeTraceSessionId_ == sessionId;
    if (closingActive) {
        saveActiveSessionUiState();
    }
    cancelClipboardXactionAddressInsertForSession((*found)->id);
    cancelXactionIndexingForSession(**found, true);
    cancelStatisticsComputationForSession(**found);
    if ((*found)->flitModel) {
        (*found)->flitModel->cancelPendingFilterBuild();
    }
    destroySessionVisualizationWidgets(**found);
    traceSessions_.erase(found);
    refreshLatencyDiffSessions();

    if (traceSessions_.empty()) {
        resetNoSessionState();
        return;
    }

    if (closingActive) {
        const std::size_t nextIndex = std::min<std::size_t>(removedIndex, traceSessions_.size() - 1);
        activeTraceSessionId_ = 0;
        activateTraceSession(traceSessions_[nextIndex]->id);
    } else {
        refreshSessionTabs();
        updateWindowCaption();
        scheduleDiagnosticsSnapshotRefresh();
    }
}

void MainWindow::closeOtherTraceSessions(const quint64 keepSessionId)
{
    if (!traceViewSessionById(keepSessionId)) {
        return;
    }

    for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (!session || session->id == keepSessionId) {
            continue;
        }
        if (!confirmDirtySessionAction(*session, QStringLiteral("closing other sessions"))) {
            return;
        }
    }

    activateTraceSession(keepSessionId);
    saveActiveSessionUiState();
    for (auto it = traceSessions_.begin(); it != traceSessions_.end();) {
        if (*it && (*it)->id == keepSessionId) {
            ++it;
            continue;
        }
        cancelClipboardXactionAddressInsertForSession((*it)->id);
        cancelXactionIndexingForSession(**it, true);
        cancelStatisticsComputationForSession(**it);
        if ((*it)->flitModel) {
            (*it)->flitModel->cancelPendingFilterBuild();
        }
        destroySessionVisualizationWidgets(**it);
        it = traceSessions_.erase(it);
    }
    activeTraceSessionId_ = keepSessionId;
    bindActiveSessionUi();
    refreshLatencyDiffSessions();
}

void MainWindow::closeAllTraceSessions()
{
    if (!confirmAllDirtySessions(QStringLiteral("closing all sessions"))) {
        return;
    }

    for (std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (!session) {
            continue;
        }
        cancelClipboardXactionAddressInsertForSession(session->id);
        cancelXactionIndexingForSession(*session, true);
        cancelStatisticsComputationForSession(*session);
        if (session->flitModel) {
            session->flitModel->cancelPendingFilterBuild();
            session->flitModel->clear();
        }
        if (session->detailModel) {
            session->detailModel->setTraceSession(nullptr);
            session->detailModel->setRecord(nullptr);
        }
        destroySessionVisualizationWidgets(*session);
    }
    traceSessions_.clear();
    resetNoSessionState();
}

void MainWindow::openTraceFile()
{
    const QString startDirectory = currentTracePath_.isEmpty()
        ? QDir::homePath()
        : QFileInfo(currentTracePath_).absolutePath();

    const QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Open CLog.B Traces"),
        startDirectory,
        QStringLiteral("Trace files (*.clog *.cldb *.clogb *.clog.b);;CLog.B files (*.clogb *.clog.b);;All files (*)"));

    for (const QString& filePath : filePaths) {
        if (filePath.isEmpty()) {
            continue;
        }
        loadTraceFile(filePath);
    }
}

bool MainWindow::loadTraceFile(const QString& filePath,
                               std::optional<CLog::Parameters> parameterOverride,
                               const quint64 replaceSessionId,
                               const bool showErrorDialog)
{
    if (TraceViewSession* session = traceViewSessionById(replaceSessionId)) {
        if (!confirmDirtySessionAction(*session, QStringLiteral("reloading the session"))) {
            return false;
        }
    }

    pendingTraceLoadPath_ = filePath;
    pendingTraceLoadPaths_.append(filePath);
    if (TraceViewSession* session = traceViewSessionById(replaceSessionId)) {
        session->pendingLoadPath = filePath;
        cancelXactionIndexingForSession(*session, true);
        cancelStatisticsComputationForSession(*session);
    }
    refreshDiagnosticsSnapshot();

    const QFileInfo sourceInfo(filePath);
    const QString sourceName = sourceInfo.fileName().isEmpty()
        ? QDir::toNativeSeparators(filePath)
        : sourceInfo.fileName();

    statusBar()->showMessage(QStringLiteral("Loading %1...").arg(sourceName));

    QDialog progressDialog(this);
    progressDialog.setWindowTitle(QStringLiteral("Open CLog.B"));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    progressDialog.setWindowFlag(Qt::WindowCloseButtonHint, false);
    progressDialog.resize(760, 360);

    QVBoxLayout* progressLayout = new QVBoxLayout(&progressDialog);
    progressLayout->setContentsMargins(14, 14, 14, 14);
    progressLayout->setSpacing(10);

    QLabel* statusLabel = new QLabel(QStringLiteral("Opening trace file..."), &progressDialog);
    statusLabel->setWordWrap(true);
    progressLayout->addWidget(statusLabel);

    QLabel* detailLabel = new QLabel(QStringLiteral("Preparing to read %1").arg(sourceName), &progressDialog);
    detailLabel->setObjectName(QStringLiteral("secondaryLabel"));
    detailLabel->setWordWrap(true);
    progressLayout->addWidget(detailLabel);

    QProgressBar* progressBar = new QProgressBar(&progressDialog);
    progressBar->setRange(0, 1);
    progressBar->setValue(0);
    progressLayout->addWidget(progressBar);

    QGridLayout* metricsLayout = new QGridLayout();
    metricsLayout->setContentsMargins(0, 0, 0, 0);
    metricsLayout->setHorizontalSpacing(14);
    metricsLayout->setVerticalSpacing(6);

    QLabel* throughputTitle = new QLabel(QStringLiteral("Overall Throughput"), &progressDialog);
    throughputTitle->setObjectName(QStringLiteral("sectionLabel"));
    QLabel* throughputValue = new QLabel(QStringLiteral("0 B/s"), &progressDialog);
    QLabel* growthTitle = new QLabel(QStringLiteral("Memory Growth"), &progressDialog);
    growthTitle->setObjectName(QStringLiteral("sectionLabel"));
    QLabel* growthValue = new QLabel(QStringLiteral("Working +0 B, Private +0 B"), &progressDialog);
    QLabel* footprintTitle = new QLabel(QStringLiteral("Memory Footprint"), &progressDialog);
    footprintTitle->setObjectName(QStringLiteral("sectionLabel"));
    QLabel* footprintValue = new QLabel(QStringLiteral("Working 0 B, Private 0 B"), &progressDialog);
    QLabel* workerCountTitle = new QLabel(QStringLiteral("Worker Threads"), &progressDialog);
    workerCountTitle->setObjectName(QStringLiteral("sectionLabel"));
    QLabel* workerCountValue = new QLabel(QStringLiteral("0"), &progressDialog);

    metricsLayout->addWidget(throughputTitle, 0, 0);
    metricsLayout->addWidget(throughputValue, 0, 1);
    metricsLayout->addWidget(growthTitle, 1, 0);
    metricsLayout->addWidget(growthValue, 1, 1);
    metricsLayout->addWidget(footprintTitle, 2, 0);
    metricsLayout->addWidget(footprintValue, 2, 1);
    metricsLayout->addWidget(workerCountTitle, 3, 0);
    metricsLayout->addWidget(workerCountValue, 3, 1);
    metricsLayout->setColumnStretch(1, 1);
    progressLayout->addLayout(metricsLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, &progressDialog);
    QPushButton* cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    if (cancelButton) {
        cancelButton->setObjectName(QStringLiteral("actionButton"));
        cancelButton->setFixedHeight(22);
    }
    progressLayout->addWidget(buttonBox);
    progressDialog.show();
    progressDialog.raise();

    std::shared_ptr<TraceSession> loadedTraceSession;
    CLogBTraceLoadError error;
    bool loadSucceeded = false;
    bool loadCancelled = false;
    std::atomic_bool loadFinished = false;
    std::atomic_bool cancelRequested = false;
    std::atomic<std::uint64_t> progressProcessedBytes = 0;
    std::atomic<std::uint64_t> progressTotalBytes = 0;
    std::atomic<CLogBTraceLoadStage> loadStage = CLogBTraceLoadStage::Opening;
    std::atomic<std::size_t> activeWorkerCount = 0;
    std::atomic<std::size_t> stageTotalRecords = 0;
    std::atomic<std::size_t> stageCompletedRecords = 0;
    std::stop_source loadStopSource;
    const bool wasEnabled = isEnabled();
    setEnabled(false);

    {
        WaitCursorGuard cursorGuard;
        std::jthread loadThread([&]() {
            CLogBTraceLoadCallbacks callbacks;
            callbacks.progress =
                [&progressProcessedBytes, &progressTotalBytes](const std::uint64_t processedBytes,
                                                               const std::uint64_t totalBytes) {
                    progressProcessedBytes.store(processedBytes, std::memory_order_relaxed);
                    progressTotalBytes.store(totalBytes, std::memory_order_relaxed);
                };
            callbacks.stage =
                [&loadStage,
                 &activeWorkerCount,
                 &stageTotalRecords,
                 &stageCompletedRecords](
                    const CLogBTraceLoadStage stage,
                    const std::size_t workerCount,
                    const std::size_t totalRecords) {
                    loadStage.store(stage, std::memory_order_relaxed);
                    if (workerCount > 0 || stage == CLogBTraceLoadStage::Opening
                        || stage == CLogBTraceLoadStage::Parsing
                        || stage == CLogBTraceLoadStage::Finalizing
                        || stage == CLogBTraceLoadStage::FinalizingIndexDebug
                        || stage == CLogBTraceLoadStage::FinalizingIndexLayout
                        || stage == CLogBTraceLoadStage::FinalizingIndexRows
                        || stage == CLogBTraceLoadStage::FinalizingIndexRowDirectory
                        || stage == CLogBTraceLoadStage::Completed) {
                        activeWorkerCount.store(workerCount, std::memory_order_relaxed);
                    }
                    stageTotalRecords.store(totalRecords, std::memory_order_relaxed);
                    stageCompletedRecords.store(0, std::memory_order_relaxed);
                };
            callbacks.stageProgress =
                [&stageCompletedRecords, &stageTotalRecords](const std::size_t completedRecords,
                                                             const std::size_t totalRecords) {
                    stageCompletedRecords.store(completedRecords, std::memory_order_relaxed);
                    stageTotalRecords.store(totalRecords, std::memory_order_relaxed);
                };

            TraceSessionOptions sessionOptions;
            sessionOptions.parameterOverride = parameterOverride;
            sessionOptions.enableFastIndex = !parameterOverride.has_value();

            std::shared_ptr<TraceSession> session;
            if (!TraceSession::open(filePath,
                                    session,
                                    error,
                                    sessionOptions,
                                    callbacks,
                                    loadStopSource.get_token())) {
                loadFinished.store(true, std::memory_order_release);
                return;
            }

            const std::uint64_t warmRowCount = std::min<std::uint64_t>(
                session->pageSize(),
                session->totalRows());
            if (warmRowCount > 0
                && !session->ensureRows(0,
                                        warmRowCount,
                                        error,
                                        callbacks,
                                        loadStopSource.get_token())) {
                loadFinished.store(true, std::memory_order_release);
                return;
            }

            loadedTraceSession = std::move(session);
            loadSucceeded = true;
            loadFinished.store(true, std::memory_order_release);
        });

        QEventLoop waitLoop;
        QElapsedTimer metricsTimer;
        metricsTimer.start();
        ProcessMemorySample baselineMemory = QueryProcessMemory();
        std::uint64_t previousProcessedBytes = 0;
        std::size_t previousStageCompletedRecords = 0;
        qint64 previousSampleTimeMs = 0;
        CLogBTraceLoadStage previousStage = CLogBTraceLoadStage::Opening;
        QTimer waitTimer(&progressDialog);
        waitTimer.setInterval(200);
        const auto requestCancel = [&]() {
            if (cancelRequested.exchange(true, std::memory_order_relaxed)) {
                return;
            }

            loadStopSource.request_stop();
            statusLabel->setText(QStringLiteral("Cancelling trace load..."));
            detailLabel->setText(QStringLiteral("Waiting for worker threads to stop safely."));
            if (cancelButton) {
                cancelButton->setEnabled(false);
            }
        };
        connect(buttonBox, &QDialogButtonBox::rejected, &progressDialog, requestCancel);
        connect(&progressDialog, &QDialog::rejected, &progressDialog, requestCancel);
        connect(&waitTimer, &QTimer::timeout, &progressDialog, [&]() {
            if (!progressDialog.isVisible() && !loadFinished.load(std::memory_order_acquire)) {
                progressDialog.show();
                progressDialog.raise();
            }

            const std::uint64_t totalBytes = progressTotalBytes.load(std::memory_order_relaxed);
            const std::uint64_t processedBytes = progressProcessedBytes.load(std::memory_order_relaxed);
            const CLogBTraceLoadStage stage = loadStage.load(std::memory_order_relaxed);
            const std::size_t workerCount = activeWorkerCount.load(std::memory_order_relaxed);
            const std::size_t totalStageRecords = stageTotalRecords.load(std::memory_order_relaxed);
            const std::size_t completedStageRecords = stageCompletedRecords.load(std::memory_order_relaxed);

            if (totalBytes == 0) {
                progressBar->setRange(0, 0);
            } else {
                const std::uint64_t clampedMaximum = std::min<std::uint64_t>(
                    totalBytes,
                    static_cast<std::uint64_t>(std::numeric_limits<int>::max()));
                if (progressBar->maximum() != static_cast<int>(clampedMaximum)) {
                    progressBar->setRange(0, static_cast<int>(clampedMaximum));
                }
                const int progressValue = totalBytes <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())
                    ? static_cast<int>(std::min<std::uint64_t>(processedBytes, clampedMaximum))
                    : static_cast<int>(
                        std::min<std::uint64_t>(
                            (processedBytes * clampedMaximum) / totalBytes,
                            clampedMaximum));
                progressBar->setValue(progressValue);
            }

            const qint64 currentSampleTimeMs = metricsTimer.elapsed();
            const double sampleSeconds = previousSampleTimeMs < currentSampleTimeMs
                ? static_cast<double>(currentSampleTimeMs - previousSampleTimeMs) / 1000.0
                : 0.0;
            if (stage != previousStage) {
                previousProcessedBytes = processedBytes;
                previousStageCompletedRecords = completedStageRecords;
            }
            const double throughputBytesPerSecond = sampleSeconds > 0.0
                ? static_cast<double>(processedBytes >= previousProcessedBytes
                                          ? (processedBytes - previousProcessedBytes)
                                          : 0U)
                    / sampleSeconds
                : 0.0;
            const double throughputRecordsPerSecond = sampleSeconds > 0.0
                ? static_cast<double>(completedStageRecords >= previousStageCompletedRecords
                                          ? (completedStageRecords - previousStageCompletedRecords)
                                          : completedStageRecords)
                    / sampleSeconds
                : 0.0;
            previousProcessedBytes = processedBytes;
            previousStageCompletedRecords = completedStageRecords;
            previousSampleTimeMs = currentSampleTimeMs;
            previousStage = stage;

            const QString stageText = cancelRequested.load(std::memory_order_relaxed)
                ? QStringLiteral("Cancelling trace load...")
                : LoadStageText(stage, workerCount);
            statusLabel->setText(stageText);
            detailLabel->setText(totalBytes == 0
                                     ? QStringLiteral("%1 / %2 records in current stage")
                                           .arg(completedStageRecords)
                                           .arg(totalStageRecords)
                                     : QStringLiteral("%1 of %2 read")
                                           .arg(FormatBytes(processedBytes), FormatBytes(totalBytes)));
            if (stage == CLogBTraceLoadStage::Decoding || stage == CLogBTraceLoadStage::Formatting
                || stage == CLogBTraceLoadStage::Finalizing
                || stage == CLogBTraceLoadStage::FinalizingIndexDebug
                || stage == CLogBTraceLoadStage::FinalizingIndexLayout
                || stage == CLogBTraceLoadStage::FinalizingIndexRows
                || stage == CLogBTraceLoadStage::FinalizingIndexRowDirectory) {
                throughputValue->setText(
                    QStringLiteral("%1, %2")
                        .arg(FormatRecordRate(throughputRecordsPerSecond),
                             FormatTransferRate(throughputBytesPerSecond)));
            } else {
                throughputValue->setText(FormatTransferRate(throughputBytesPerSecond));
            }

            const ProcessMemorySample currentMemory = QueryProcessMemory();
            growthValue->setText(
                QStringLiteral("Working +%1, Private +%2")
                    .arg(FormatBytes(currentMemory.workingSetBytes >= baselineMemory.workingSetBytes
                                         ? currentMemory.workingSetBytes - baselineMemory.workingSetBytes
                                         : 0U),
                         FormatBytes(currentMemory.privateBytes >= baselineMemory.privateBytes
                                         ? currentMemory.privateBytes - baselineMemory.privateBytes
                                         : 0U)));
            footprintValue->setText(
                QStringLiteral("Working %1, Private %2")
                    .arg(FormatBytes(currentMemory.workingSetBytes),
                         FormatBytes(currentMemory.privateBytes)));
            workerCountValue->setText(QString::number(workerCount));

            if (loadFinished.load(std::memory_order_acquire)) {
                waitLoop.quit();
            }
        });

        waitTimer.start();
        waitLoop.exec();
        waitTimer.stop();

        if (loadThread.joinable()) {
            loadThread.join();
        }
    }
    progressDialog.hide();
    setEnabled(wasEnabled);
    loadCancelled = error.type == CLogBTraceLoadError::Type::Cancelled;

    if (!loadSucceeded) {
        pendingTraceLoadPath_.clear();
        pendingTraceLoadPaths_.removeAll(filePath);
        if (TraceViewSession* session = traceViewSessionById(replaceSessionId)) {
            session->pendingLoadPath.clear();
        }
        refreshDiagnosticsSnapshot();
        if (loadCancelled) {
            statusBar()->showMessage(QStringLiteral("Cancelled loading %1.").arg(sourceName), 5000);
            return false;
        }
        if (showErrorDialog) {
            const std::optional<CLog::Parameters> selectedParameters =
                showTraceLoadErrorDialog(this, sourceName, filePath, error);
            if (selectedParameters.has_value()) {
                statusBar()->showMessage(QStringLiteral("Retrying %1 with selected CHI parameters.").arg(sourceName),
                                         5000);
                return loadTraceFile(filePath, selectedParameters, replaceSessionId, showErrorDialog);
            }
        }
        statusBar()->showMessage(QStringLiteral("Failed to load %1.").arg(sourceName), 5000);
        return false;
    }

    const QString resolvedPath = sourceInfo.canonicalFilePath().isEmpty()
        ? sourceInfo.absoluteFilePath()
        : sourceInfo.canonicalFilePath();
    const QFileInfo resolvedInfo(resolvedPath);
    const QString resolvedName = resolvedInfo.fileName().isEmpty()
        ? sourceName
        : resolvedInfo.fileName();

    pendingTraceLoadPath_.clear();
    pendingTraceLoadPaths_.removeAll(filePath);
    if (replaceSessionId != 0) {
        TraceViewSession* session = traceViewSessionById(replaceSessionId);
        if (!session) {
            statusBar()->showMessage(QStringLiteral("The session selected for reload is no longer open."), 5000);
            pendingTraceLoadPaths_.removeAll(filePath);
            return false;
        }
        saveActiveSessionUiState();
        session->statisticsAccumulator.reset();
        session->statisticsComputed = false;
        session->statisticsResult = {};
        session->statisticsProgressText.clear();
        session->statisticsProgressValue = 0;
        session->statisticsProgressMaximum = 1000;
        session->optionalFieldIndexingFields.clear();
        session->clipboardEntries.clear();
        session->traceSession = std::move(loadedTraceSession);
        session->rowBacked = false;
        session->editableMetadata.reset();
        session->editModeEnabled = false;
        session->dirty = false;
        session->derivedViewsOutdated = false;
        session->path = resolvedPath;
        session->label = uniqueSessionLabel(resolvedName, session);
        session->parameterOverride = parameterOverride;
        session->pendingLoadPath.clear();
        session->selectedLogicalRow = -1;
        session->detailModel->setTraceSession(session->traceSession);
        session->flitModel->setTraceSession(session->traceSession);
        session->flitModel->setEditable(false);
        session->flitModel->setDirty(false);
        session->timelineViewState.clear();
        session->addressViewState.clear();
        session->cacheViewState.clear();
        session->latencyViewState.clear();
        session->transactionViewState.clear();
        populateSessionVisualizationWidgets(*session);
        refreshLatencyDiffSessions();
        if (activeTraceSessionId_ == session->id) {
            bindActiveSessionUi();
        } else {
            activateTraceSession(session->id);
        }
    } else {
        currentTraceParameterOverride_ = parameterOverride;
        applyTraceSession(std::move(loadedTraceSession), resolvedName, resolvedPath);
    }
    if (TraceViewSession* session = activeTraceViewSession()) {
        session->parameterOverride = parameterOverride;
        currentTraceParameterOverride_ = session->parameterOverride;
    }
    statusBar()->showMessage(
        QStringLiteral("Loaded %1 flits from %2.")
            .arg(currentTraceSession_
                     ? static_cast<qulonglong>(currentTraceSession_->totalRows())
                     : static_cast<qulonglong>(flitModel_->totalCount()))
            .arg(resolvedName),
        5000);
    return true;
}

void MainWindow::refreshLatencyDiffSessions()
{
    if (!latencyDiffWidget_) {
        return;
    }

    std::vector<LatencyDiffSessionSource> sources;
    sources.reserve(traceSessions_.size());
    for (const std::unique_ptr<TraceViewSession>& session : traceSessions_) {
        if (!session) {
            continue;
        }
        LatencyDiffSessionSource source;
        source.id = session->id;
        source.label = session->label;
        source.path = session->path;
        source.traceSession = session->traceSession;
        if (!source.traceSession && session->flitModel) {
            source.rows = session->flitModel->sourceRowsSnapshot();
        }
        sources.push_back(std::move(source));
    }
    latencyDiffWidget_->setSessions(std::move(sources));
}

void MainWindow::showLatencyDiffWizard()
{
    refreshLatencyDiffSessions();
    if (latencyDiffDock_) {
        latencyDiffDock_->toggleView(true);
        latencyDiffDock_->raise();
    }
    if (latencyDiffWidget_) {
        latencyDiffWidget_->runWizard();
    }
}

void MainWindow::reloadTrace()
{
    const TraceViewSession* session = activeTraceViewSession();
    if (session && !session->path.isEmpty()) {
        reloadActiveTrace(true);
        return;
    }

    openTraceFile();
}

bool MainWindow::reloadActiveTrace(const bool showErrorDialog)
{
    const TraceViewSession* session = activeTraceViewSession();
    if (!session || session->path.isEmpty()) {
        return false;
    }
    return loadTraceFile(session->path, session->parameterOverride, session->id, showErrorDialog);
}

void MainWindow::applyTraceSession(std::shared_ptr<TraceSession> session,
                                   const QString& sourceLabel,
                                   const QString& sourcePath)
{
    TraceViewSession* traceViewSession = addTraceViewSession(sourceLabel, sourcePath, currentTraceParameterOverride_);
    traceViewSession->traceSession = std::move(session);
    traceViewSession->rowBacked = false;
    traceViewSession->editModeEnabled = false;
    traceViewSession->dirty = false;
    traceViewSession->derivedViewsOutdated = false;
    traceViewSession->detailModel->setTraceSession(traceViewSession->traceSession);
    traceViewSession->flitModel->setTraceSession(traceViewSession->traceSession);
    populateSessionVisualizationWidgets(*traceViewSession);
    refreshLatencyDiffSessions();
    activateTraceSession(traceViewSession->id);
}

void MainWindow::applyTraceRows(std::vector<FlitRecord> rows,
                                const QString& sourceLabel,
                                const QString& sourcePath)
{
    currentTraceParameterOverride_.reset();
    TraceViewSession* traceViewSession = addTraceViewSession(sourceLabel, sourcePath, std::nullopt);
    traceViewSession->traceSession.reset();
    traceViewSession->rowBacked = true;
    traceViewSession->editModeEnabled = false;
    traceViewSession->dirty = false;
    traceViewSession->derivedViewsOutdated = false;
    traceViewSession->detailModel->setTraceSession(nullptr);
    traceViewSession->flitModel->setRows(std::move(rows));
    populateSessionVisualizationWidgets(*traceViewSession);
    refreshLatencyDiffSessions();
    activateTraceSession(traceViewSession->id);
}


void MainWindow::applyTraceTableRowHeight()
{
    if (!flitView_ || !flitView_->verticalHeader() || !flitModel_) {
        return;
    }

    const int rowHeight = TraceTableRowHeightForVisibleRows(flitModel_->visibleCount());
    flitView_->verticalHeader()->setDefaultSectionSize(rowHeight);
    if (xactionRowsTable_ && xactionRowsTable_->verticalHeader()) {
        xactionRowsTable_->verticalHeader()->setDefaultSectionSize(rowHeight);
        xactionRowsTable_->verticalHeader()->setMinimumSectionSize(rowHeight);
        for (int row = 0; row < xactionRowsTable_->rowCount(); ++row) {
            xactionRowsTable_->setRowHeight(row, rowHeight);
        }
    }
    if (clipboardWidget_) {
        clipboardWidget_->applyTraceTableRowStyle(flitModel_->visibleCount());
    }
}

void MainWindow::applyFlitColumnWidths()
{
    if (!flitView_ || !flitModel_) {
        return;
    }

    if (!currentTraceSession_) {
        flitView_->resizeColumnsToContents();
        return;
    }

    const QFontMetrics metrics(flitView_->font());
    for (int logicalColumn = 0; logicalColumn < flitModel_->columnCount(); ++logicalColumn) {
        const QString headerText =
            flitModel_->headerData(logicalColumn, Qt::Horizontal, Qt::DisplayRole).toString();
        int width = qMax(SessionBackedColumnWidth(logicalColumn),
                         metrics.horizontalAdvance(headerText) + 26);
        if (flitModel_->nodeLabelsVisible() && IsNodeLabelColumnName(headerText)) {
            width = qMax(width, NodeLabelDelegate::kMinimumColumnWidth);
        }
        flitView_->setColumnWidth(logicalColumn, width);
    }
}

void MainWindow::resizeFlitColumnForCurrentTrace(const int logicalColumn)
{
    if (!flitView_ || !flitModel_ || logicalColumn < 0 || logicalColumn >= flitModel_->columnCount()) {
        return;
    }

    if (!currentTraceSession_) {
        flitView_->resizeColumnToContents(logicalColumn);
        return;
    }

    const QFontMetrics metrics(flitView_->font());
    const QString headerText =
        flitModel_->headerData(logicalColumn, Qt::Horizontal, Qt::DisplayRole).toString();
    int width = qMax(SessionBackedColumnWidth(logicalColumn),
                     metrics.horizontalAdvance(headerText) + 26);
    if (flitModel_->nodeLabelsVisible() && IsNodeLabelColumnName(headerText)) {
        width = qMax(width, NodeLabelDelegate::kMinimumColumnWidth);
    }
    flitView_->setColumnWidth(logicalColumn, width);
}

void MainWindow::refreshLatencyView()
{
    if (TraceViewSession* session = activeTraceViewSession()) {
        createSessionVisualizationWidgets(*session);
        if (session->latencyWidget) {
            if (session->traceSession) {
                session->latencyWidget->setTraceSession(session->traceSession);
            } else if (session->flitModel) {
                session->latencyWidget->setRows(session->flitModel->sourceRowsSnapshot());
            } else {
                session->latencyWidget->clear();
            }
            latencyWidget_ = session->latencyWidget;
            if (latencyStack_) {
                latencyStack_->setCurrentWidget(latencyWidget_);
            }
        }
    }
}

void MainWindow::refreshTransactionView(TraceViewSession& session)
{
    createSessionVisualizationWidgets(session);
    if (session.cacheWidget) {
        if (session.traceSession) {
            session.cacheWidget->setTraceSession(session.traceSession);
        } else {
            session.cacheWidget->setRows({});
        }
        if (session.id == activeTraceSessionId_) {
            cacheWidget_ = session.cacheWidget;
            if (cacheStack_) {
                cacheStack_->setCurrentWidget(cacheWidget_);
            }
        }
    }

    if (!session.transactionWidget) {
        return;
    }

    if (session.traceSession) {
        session.transactionWidget->setTraceSession(session.traceSession);
    } else if (session.flitModel) {
        session.transactionWidget->setRows(session.flitModel->sourceRowsSnapshot());
    } else {
        session.transactionWidget->clear();
    }

    if (session.id == activeTraceSessionId_) {
        transactionWidget_ = session.transactionWidget;
        if (transactionStack_) {
            transactionStack_->setCurrentWidget(transactionWidget_);
        }
    }
}


}  // namespace CHIron::Gui
