#include "flit_table_model.hpp"
#include "bug_reporter.hpp"
#include "filter_parallel_utils.hpp"
#include "flit_edit_adapter.hpp"
#include "flit_transaction_keys.hpp"
#include "gui_colors.hpp"
#include "gui_format.hpp"

#include <QElapsedTimer>
#include <QFont>
#include <QMetaObject>
#include <QSignalBlocker>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>

#include <algorithm>
#include <atomic>
#include <exception>
#include <iterator>
#include <limits>
#include <mutex>
#include <new>
#include <numeric>
#include <thread>
#include <utility>

namespace CHIron::Gui {

namespace {

QColor RowBackgroundForChannel(const FlitChannel channel)
{
    switch (channel) {
    case FlitChannel::Req:
        return QColor(QStringLiteral("#FFF4DD"));
    case FlitChannel::Snp:
        return QColor(QStringLiteral("#FDE8E5"));
    case FlitChannel::Rsp:
    case FlitChannel::Dat:
        return QColor();
    }

    return QColor();
}

QColor TransactionHighlightBackground()
{
    return QColor(QStringLiteral("#FF8A00"));
}

QColor OpcodeForegroundForChannel(const FlitRecord& record)
{
    const QColor accent = ChannelAccent(record.channel);
    switch (record.channel) {
    case FlitChannel::Req:
    case FlitChannel::Snp:
        return QColor(QStringLiteral("#000000"));
    case FlitChannel::Rsp:
    case FlitChannel::Dat:
        return accent.darker(145);
    }

    return accent;
}

QColor NodeTypeLabelColor(const QString& label)
{
    if (label == QLatin1String("RN-F")) {
        return QColor(QStringLiteral("#2F80ED"));
    }
    if (label == QLatin1String("RN-D")) {
        return QColor(QStringLiteral("#56CCF2"));
    }
    if (label == QLatin1String("RN-I")) {
        return QColor(QStringLiteral("#00A878"));
    }
    if (label == QLatin1String("HN-F")) {
        return QColor(QStringLiteral("#9B51E0"));
    }
    if (label == QLatin1String("HN-I")) {
        return QColor(QStringLiteral("#6C63FF"));
    }
    if (label == QLatin1String("SN-F")) {
        return QColor(QStringLiteral("#EB5757"));
    }
    if (label == QLatin1String("SN-I")) {
        return QColor(QStringLiteral("#F2994A"));
    }
    if (label == QLatin1String("MN")) {
        return QColor(QStringLiteral("#219653"));
    }
    if (label == QLatin1String("Before SAM")) {
        return QColor(QStringLiteral("#D5DAE1"));
    }
    return QColor(QStringLiteral("#8B949E"));
}

bool ParseSearchInteger(QString text, qulonglong& value)
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

bool IsIncompleteHexPrefix(QString text)
{
    text = text.trimmed();
    return text.compare(QStringLiteral("0x"), Qt::CaseInsensitive) == 0;
}

QString NormalizeNumericFilterInput(QString text)
{
    text = text.trimmed();
    return IsIncompleteHexPrefix(text)
        ? QString()
        : text;
}

template<typename TmatchFn>
void CollectMatchingLogicalRows(const std::uint64_t rowStart,
                                const std::vector<FlitRecord>& rows,
                                TmatchFn&& matchFn,
                                std::vector<int>& matchedRows)
{
    matchedRows.clear();
    const std::size_t rowCount = rows.size();
    if (rowCount == 0) {
        return;
    }

    const std::size_t workerCount = Detail::suggestedFilterWorkerCount(rowCount);
    if (workerCount <= 1) {
        matchedRows.reserve(rowCount);
        for (std::size_t localIndex = 0; localIndex < rowCount; ++localIndex) {
            if (matchFn(rows[localIndex])) {
                matchedRows.push_back(static_cast<int>(rowStart + localIndex));
            }
        }
        return;
    }

    std::vector<std::vector<int>> workerMatches(workerCount);
    std::vector<std::jthread> workers;
    workers.reserve(workerCount);

    std::atomic_bool failed = false;
    std::exception_ptr workerException;
    std::mutex workerExceptionMutex;

    const std::size_t rowsPerWorker = (rowCount + workerCount - 1) / workerCount;
    std::size_t activeWorkers = 0;
    for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
        const std::size_t workerBegin = workerIndex * rowsPerWorker;
        if (workerBegin >= rowCount) {
            break;
        }

        const std::size_t workerEnd = std::min(rowCount, workerBegin + rowsPerWorker);
        workers.emplace_back([&, workerIndex, workerBegin, workerEnd]() {
            try {
                std::vector<int>& matches = workerMatches[workerIndex];
                matches.reserve(workerEnd - workerBegin);
                for (std::size_t localIndex = workerBegin; localIndex < workerEnd; ++localIndex) {
                    if (failed.load(std::memory_order_relaxed)) {
                        return;
                    }

                    if (matchFn(rows[localIndex])) {
                        matches.push_back(static_cast<int>(rowStart + localIndex));
                    }
                }
            } catch (...) {
                failed.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(workerExceptionMutex);
                if (!workerException) {
                    workerException = std::current_exception();
                }
            }
        });
        ++activeWorkers;
    }

    workers.clear();
    if (workerException) {
        std::rethrow_exception(workerException);
    }

    std::size_t totalMatchedRows = 0;
    for (std::size_t workerIndex = 0; workerIndex < activeWorkers; ++workerIndex) {
        totalMatchedRows += workerMatches[workerIndex].size();
    }

    matchedRows.reserve(totalMatchedRows);
    for (std::size_t workerIndex = 0; workerIndex < activeWorkers; ++workerIndex) {
        matchedRows.insert(matchedRows.end(),
                           workerMatches[workerIndex].begin(),
                           workerMatches[workerIndex].end());
    }
}

std::vector<int> IntersectSortedLogicalRows(const std::vector<int>& lhs,
                                            const std::vector<int>& rhs)
{
    std::vector<int> result;
    result.reserve(std::min(lhs.size(), rhs.size()));
    std::set_intersection(lhs.begin(),
                          lhs.end(),
                          rhs.begin(),
                          rhs.end(),
                          std::back_inserter(result));
    return result;
}

bool MatchesFilter(const QString& value, const QString& filter)
{
    if (filter.isEmpty()) {
        return true;
    }

    if (IsIncompleteHexPrefix(filter)) {
        return false;
    }

    qulonglong filterNumber = 0;
    qulonglong valueNumber = 0;
    const bool filterIsNumeric = ParseSearchInteger(filter, filterNumber);
    if (filterIsNumeric && ParseSearchInteger(value, valueNumber)) {
        return filterNumber == valueNumber;
    }

    return value.contains(filter, Qt::CaseInsensitive);
}

bool IsDefaultFieldName(const QString& fieldName)
{
    return fieldName == QLatin1String("Opcode")
        || fieldName == QLatin1String("SrcID")
        || fieldName == QLatin1String("TgtID")
        || fieldName == QLatin1String("TxnID")
        || fieldName == QLatin1String("Addr")
        || fieldName == QLatin1String("DBID")
        || fieldName == QLatin1String("DataID")
        || fieldName == QLatin1String("Resp")
        || fieldName == QLatin1String("FwdState")
        || fieldName == QLatin1String("RespErr");
}

bool FieldEntriesEqual(const FieldEntry& lhs, const FieldEntry& rhs)
{
    return FieldScopeText(lhs) == FieldScopeText(rhs)
        && FieldNameText(lhs) == FieldNameText(rhs)
        && lhs.value == rhs.value
        && lhs.raw == rhs.raw;
}

bool FlitRecordsEqualForUndo(const FlitRecord& lhs, const FlitRecord& rhs)
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
        if (!FieldEntriesEqual(lhs.fields[index], rhs.fields[index])) {
            return false;
        }
    }
    return true;
}

bool ParseFieldRawIntegral(const FlitRecord& record, const char* fieldName, qulonglong& value)
{
    const QLatin1String lookup(fieldName);
    for (const FieldEntry& field : record.fields) {
        if (FieldNameText(field) != lookup) {
            continue;
        }

        const QString rawToken = field.raw.section(QChar(' '), 0, 0, QString::SectionSkipEmpty);
        return ParseSearchInteger(rawToken, value);
    }

    return false;
}

const FieldEntry* FindField(const FlitRecord& record, const QString& fieldName)
{
    for (const FieldEntry& field : record.fields) {
        if (FieldNameText(field) == fieldName) {
            return &field;
        }
    }

    return nullptr;
}

bool IsNodeIdFieldName(const QString& fieldName)
{
    return fieldName == QLatin1String("SrcID")
        || fieldName == QLatin1String("TgtID")
        || fieldName == QLatin1String("ReturnNID")
        || fieldName == QLatin1String("HomeNID")
        || fieldName == QLatin1String("FwdNID");
}

bool ParseFieldValueIntegral(const FieldEntry& field, qulonglong& value)
{
    if (ParseSearchInteger(field.value, value)) {
        return true;
    }

    const QString rawToken = field.raw.section(QChar(' '), 0, 0, QString::SectionSkipEmpty);
    return ParseSearchInteger(rawToken, value);
}

bool NumericValueForColumn(const FlitRecord& record, const int column, qulonglong& value)
{
    switch (column) {
    case FlitTableModel::TimeColumn:
        value = static_cast<qulonglong>(record.timestamp);
        return true;
    case FlitTableModel::OpcodeColumn:
        return ParseSearchInteger(record.opcodeRaw, value);
    case FlitTableModel::SourceColumn:
        return ParseSearchInteger(record.source, value);
    case FlitTableModel::TargetColumn:
        return ParseSearchInteger(record.target, value);
    case FlitTableModel::TxnColumn:
        return ParseSearchInteger(record.txnId, value);
    case FlitTableModel::AddressColumn:
        return ParseSearchInteger(record.address.isEmpty() ? record.dbid : record.address, value);
    case FlitTableModel::DataIdColumn:
        return ParseSearchInteger(record.dataId, value);
    case FlitTableModel::RespColumn:
        return ParseFieldRawIntegral(record, "Resp", value);
    case FlitTableModel::FwdStateColumn:
        return ParseFieldRawIntegral(record, "FwdState", value);
    case FlitTableModel::RespErrColumn:
        return ParseFieldRawIntegral(record, "RespErr", value);
    default:
        return false;
    }
}

QString DecodedValueForColumn(const FlitRecord& record, const int column)
{
    switch (column) {
    case FlitTableModel::TimeColumn:
        return FormatTimestampForDisplay(record.timestamp);
    case FlitTableModel::ChannelColumn:
        return ToString(record.channel);
    case FlitTableModel::DirectionColumn:
        return ToString(record.direction);
    case FlitTableModel::OpcodeColumn:
        return record.opcode;
    case FlitTableModel::SourceColumn:
        return record.source;
    case FlitTableModel::TargetColumn:
        return record.target;
    case FlitTableModel::TxnColumn:
        return record.txnId;
    case FlitTableModel::AddressColumn:
        return record.address.isEmpty() ? record.dbid : record.address;
    case FlitTableModel::DataIdColumn:
        return record.dataId;
    case FlitTableModel::RespColumn:
        return record.resp;
    case FlitTableModel::FwdStateColumn:
        return record.fwdState;
    case FlitTableModel::RespErrColumn:
        return record.respErr;
    default:
        return QString();
    }
}

QString DisplayValueForField(const FlitRecord& record,
                             const QString& fieldName,
                             const FlitTableModel::DisplayMode mode)
{
    const FieldEntry* field = FindField(record, fieldName);
    if (!field) {
        return QString();
    }

    if (mode == FlitTableModel::DisplayMode::Decoded) {
        return field->value;
    }

    qulonglong value = 0;
    if (!ParseFieldValueIntegral(*field, value)) {
        return QString();
    }

    if (mode == FlitTableModel::DisplayMode::Decimal) {
        return QString::number(value);
    }

    return QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper());
}

QString DisplayValueForColumn(const FlitRecord& record,
                              const int column,
                              const FlitTableModel::DisplayMode mode)
{
    if (column == FlitTableModel::TimeColumn && mode != FlitTableModel::DisplayMode::Hexadecimal) {
        return FormatTimestampForDisplay(record.timestamp);
    }

    if (mode == FlitTableModel::DisplayMode::Decoded) {
        return DecodedValueForColumn(record, column);
    }

    qulonglong value = 0;
    if (!NumericValueForColumn(record, column, value)) {
        return QString();
    }

    if (mode == FlitTableModel::DisplayMode::Decimal) {
        return QString::number(value);
    }

    return QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper());
}

bool MatchesFieldFilter(const FlitRecord& record, const QString& fieldName, const QString& filter)
{
    if (filter.isEmpty()) {
        return true;
    }

    const FieldEntry* field = FindField(record, fieldName);
    if (!field) {
        return false;
    }

    qulonglong filterNumber = 0;
    qulonglong valueNumber = 0;
    const bool filterIsNumeric = ParseSearchInteger(filter, filterNumber);
    if (filterIsNumeric && ParseFieldValueIntegral(*field, valueNumber)) {
        return filterNumber == valueNumber;
    }

    return field->value.contains(filter, Qt::CaseInsensitive)
        || field->raw.contains(filter, Qt::CaseInsensitive);
}

CLogBTraceChannelMask EnabledTransportMask(const bool showReq,
                                           const bool showRsp,
                                           const bool showDat,
                                           const bool showSnp,
                                           const bool showTx,
                                           const bool showRx)
{
    CLogBTraceChannelMask mask = 0;
    if (showReq && showTx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::TXREQ));
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::TXREQ_BeforeSAM));
    }
    if (showRsp && showTx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::TXRSP));
    }
    if (showDat && showTx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::TXDAT));
    }
    if (showSnp && showTx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::TXSNP));
    }
    if (showReq && showRx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::RXREQ));
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::RXREQ_BeforeSAM));
    }
    if (showRsp && showRx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::RXRSP));
    }
    if (showDat && showRx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::RXDAT));
    }
    if (showSnp && showRx) {
        mask = static_cast<CLogBTraceChannelMask>(mask | CLogBChannelBit(CLog::Channel::RXSNP));
    }
    return mask;
}

bool IsNumericCompatibleFilter(const QString& filter)
{
    if (filter.trimmed().isEmpty()) {
        return true;
    }

    for (const QChar ch : filter) {
        if (ch.isDigit()) {
            continue;
        }

        const ushort code = ch.unicode();
        if (code == 'x' || code == 'X'
            || (code >= 'a' && code <= 'f')
            || (code >= 'A' && code <= 'F')) {
            continue;
        }

        return false;
    }

    return true;
}

bool CanUseFastRawFiltersForDefaultFields(const QString& opcodeFilter,
                                          const QString& sourceIdFilter,
                                          const QString& targetIdFilter,
                                          const QString& txnIdFilter,
                                          const QString& dbidFilter,
                                          const QString& addressFilter)
{
    Q_UNUSED(opcodeFilter)
    return true
        && IsNumericCompatibleFilter(sourceIdFilter)
        && IsNumericCompatibleFilter(targetIdFilter)
        && IsNumericCompatibleFilter(txnIdFilter)
        && IsNumericCompatibleFilter(dbidFilter)
        && IsNumericCompatibleFilter(addressFilter);
}

int ClampModelCount(const std::uint64_t value)
{
    return value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())
        ? std::numeric_limits<int>::max()
        : static_cast<int>(value);
}

QString EditFieldNameForIndex(const FlitRecord& record,
                              const int column,
                              const QString& optionalFieldName)
{
    return FlitEditAdapter::fieldNameForColumn(column, optionalFieldName, record);
}

template<typename RowContainer>
void CollectOptionalFieldsFromRows(const RowContainer& rows,
                                   QStringList& rebuilt,
                                   QHash<QString, QStringList>& rebuiltByScope)
{
    QSet<QString> seenFields;
    for (const QString& fieldName : rebuilt) {
        seenFields.insert(fieldName);
    }
    QHash<QString, QSet<QString>> seenFieldsByScope;
    for (auto it = rebuiltByScope.cbegin(); it != rebuiltByScope.cend(); ++it) {
        QSet<QString> scopeFields;
        for (const QString& fieldName : it.value()) {
            scopeFields.insert(fieldName);
        }
        seenFieldsByScope.insert(it.key(), std::move(scopeFields));
    }

    for (const FlitRecord& row : rows) {
        for (const FieldEntry& field : row.fields) {
            const QString& fieldName = FieldNameText(field);
            const QString& fieldScope = FieldScopeText(field);

            if (IsDefaultFieldName(fieldName)) {
                continue;
            }

            if (!seenFields.contains(fieldName)) {
                seenFields.insert(fieldName);
                rebuilt.append(fieldName);
            }

            QSet<QString>& seenScopeFields = seenFieldsByScope[fieldScope];
            QStringList& scopeFields = rebuiltByScope[fieldScope];
            if (!seenScopeFields.contains(fieldName)) {
                seenScopeFields.insert(fieldName);
                scopeFields.append(fieldName);
            }
        }
    }
}

}  // namespace

FlitTableModel::RowMutationSnapshot MakeSnapshot(std::vector<FlitTableModel::RowMutationEntry> entries)
{
    FlitTableModel::RowMutationSnapshot snapshot;
    snapshot.entries = std::move(entries);
    std::sort(snapshot.entries.begin(),
              snapshot.entries.end(),
              [](const FlitTableModel::RowMutationEntry& lhs,
                 const FlitTableModel::RowMutationEntry& rhs) {
                  return lhs.logicalRow < rhs.logicalRow;
              });
    return snapshot;
}

class FlitTableModelSetRowCommand final : public QUndoCommand {
public:
    FlitTableModelSetRowCommand(FlitTableModel* model,
                                const int logicalRow,
                                FlitRecord before,
                                FlitRecord after)
        : model_(model)
        , logicalRow_(logicalRow)
        , before_(std::move(before))
        , after_(std::move(after))
    {
        setText(QStringLiteral("Edit flit row"));
    }

    void undo() override
    {
        if (model_) {
            model_->applySetLogicalRowDirect(logicalRow_, before_);
        }
    }

    void redo() override
    {
        if (model_) {
            model_->applySetLogicalRowDirect(logicalRow_, after_);
        }
    }

private:
    FlitTableModel* model_ = nullptr;
    int logicalRow_ = -1;
    FlitRecord before_;
    FlitRecord after_;
};

class FlitTableModelInsertRowsCommand final : public QUndoCommand {
public:
    enum class Mode {
        LogicalRow,
        Timestamp
    };

    FlitTableModelInsertRowsCommand(FlitTableModel* model,
                                    const int logicalRow,
                                    std::vector<FlitRecord> rows)
        : model_(model)
        , mode_(Mode::LogicalRow)
        , logicalRow_(logicalRow)
        , rows_(std::move(rows))
    {
        setText(QStringLiteral("Insert flit row"));
    }

    FlitTableModelInsertRowsCommand(FlitTableModel* model,
                                    std::vector<FlitRecord> rows)
        : model_(model)
        , mode_(Mode::Timestamp)
        , rows_(std::move(rows))
    {
        setText(QStringLiteral("Insert flit row"));
    }

    void undo() override
    {
        if (model_) {
            model_->applyRemoveRowsDirect(snapshot_);
        }
    }

    void redo() override
    {
        if (!model_) {
            return;
        }

        snapshot_ = {};
        if (mode_ == Mode::LogicalRow) {
            model_->applyInsertRowsAtLogicalRowDirect(logicalRow_, rows_, &snapshot_);
        } else {
            model_->applyInsertRowsByTimestampDirect(rows_, &snapshot_);
        }
    }

private:
    FlitTableModel* model_ = nullptr;
    Mode mode_ = Mode::LogicalRow;
    int logicalRow_ = -1;
    std::vector<FlitRecord> rows_;
    FlitTableModel::RowMutationSnapshot snapshot_;
};

class FlitTableModelDeleteRowCommand final : public QUndoCommand {
public:
    FlitTableModelDeleteRowCommand(FlitTableModel* model, const int logicalRow)
        : model_(model)
        , logicalRow_(logicalRow)
    {
        setText(QStringLiteral("Delete flit row"));
    }

    void undo() override
    {
        if (model_) {
            model_->applyRestoreRowsDirect(snapshot_);
        }
    }

    void redo() override
    {
        if (model_) {
            snapshot_ = {};
            model_->applyDeleteLogicalRowDirect(logicalRow_, &snapshot_);
        }
    }

private:
    FlitTableModel* model_ = nullptr;
    int logicalRow_ = -1;
    FlitTableModel::RowMutationSnapshot snapshot_;
};

FlitTableModel::FlitTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    undoStack_ = new QUndoStack(this);
    connect(undoStack_, &QUndoStack::canUndoChanged, this, [this]() {
        emitUndoRedoAvailability();
    });
    connect(undoStack_, &QUndoStack::canRedoChanged, this, [this]() {
        emitUndoRedoAvailability();
    });
    connect(undoStack_, &QUndoStack::cleanChanged, this, [this]() {
        updateDirtyFromUndoStack();
    });
}

int FlitTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return visibleRowCountInternal();
}

int FlitTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount + visibleOptionalFields_.size();
}

QVariant FlitTableModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const FlitRecord* record = recordAt(index.row());
    if (!record) {
        return {};
    }

    const bool isOptionalFieldColumn = index.column() >= ColumnCount;
    const QString optionalFieldName = isOptionalFieldColumn
        ? visibleOptionalFields_.value(index.column() - ColumnCount)
        : QString();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (isOptionalFieldColumn) {
            return DisplayValueForField(*record, optionalFieldName, displayMode(index.column()));
        }
        if (index.column() == XactionIndexColumn) {
            return QString();
        }

        return DisplayValueForColumn(*record,
                                     index.column(),
                                     displayModes_[static_cast<std::size_t>(index.column())]);
    }

    if (role == Qt::TextAlignmentRole) {
        if (isOptionalFieldColumn) {
            return displayMode(index.column()) == DisplayMode::Decoded
                ? static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter)
                : static_cast<int>(Qt::AlignCenter);
        }

        switch (index.column()) {
        case XactionIndexColumn:
        case TimeColumn:
        case ChannelColumn:
        case DirectionColumn:
        case SourceColumn:
        case TargetColumn:
        case TxnColumn:
        case AddressColumn:
        case DataIdColumn:
        case RespColumn:
        case FwdStateColumn:
        case RespErrColumn:
            return static_cast<int>(Qt::AlignCenter);
        default:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (role == Qt::ForegroundRole) {
        const QString fieldName = EditFieldNameForIndex(*record, index.column(), optionalFieldName);
        if (!fieldName.isEmpty() && fieldName != QLatin1String("Xact")
            && traceMetadata() && record->rawRecordAvailable) {
            const FlitEditCellState state = FlitEditAdapter::cellState(*record, traceMetadata(), fieldName);
            if (!state.applicable) {
                return QColor(QStringLiteral("#8B949E"));
            }
            if (editable_ && state.fixed) {
                return QColor(QStringLiteral("#586575"));
            }
        }

        if (recordMatchesTransactionHighlight(*record, logicalRowAt(index.row()))) {
            return QColor(QStringLiteral("#000000"));
        }

        if (index.column() == TargetColumn && record->dimTarget) {
            return QColor(QStringLiteral("#8B949E"));
        }

        if (index.column() == OpcodeColumn) {
            return OpcodeForegroundForChannel(*record);
        }
    }

    if (role == Qt::FontRole && index.column() == OpcodeColumn) {
        QFont font;
        font.setBold(true);
        return font;
    }

    if (role == ChannelAccentRole && index.column() == ChannelColumn) {
        return ChannelAccent(record->channel);
    }

    if (role == TransactionHighlightRole) {
        return recordMatchesTransactionHighlight(*record, logicalRowAt(index.row()));
    }

    if (role == XactionIndexStateRole && index.column() == XactionIndexColumn) {
        return static_cast<int>(record->xactionIndexState);
    }

    if (role == ChannelTagRole && index.column() == TargetColumn) {
        return record->dimTarget ? QString() : record->channelTag;
    }

    if (role == NodeLabelTextRole || role == NodeLabelColorRole) {
        QString label;
        QColor color;
        if (nodeLabelForIndex(*record, index.column(), label, color)) {
            return role == NodeLabelTextRole ? QVariant(label) : QVariant(color);
        }
        return {};
    }

    if (role == Qt::BackgroundRole) {
        if (recordMatchesTransactionHighlight(*record, logicalRowAt(index.row()))) {
            return TransactionHighlightBackground();
        }

        const QString fieldName = EditFieldNameForIndex(*record, index.column(), optionalFieldName);
        if (!fieldName.isEmpty() && fieldName != QLatin1String("Xact")
            && traceMetadata() && record->rawRecordAvailable) {
            const FlitEditCellState state = FlitEditAdapter::cellState(*record, traceMetadata(), fieldName);
            if (!state.applicable) {
                return QColor(QStringLiteral("#F1F3F5"));
            }
            if (editable_ && state.fixed) {
                return QColor(QStringLiteral("#F7F9FB"));
            }
        }

        if (index.column() == TargetColumn && record->dimTarget) {
            return QColor(QStringLiteral("#EEF1F4"));
        }

        if (searchMode_ == SearchMode::Highlight && hasActiveSearchFilters()) {
            if (passesFilters(*record)) {
                return SearchHighlightBackground();
            }
            return {};
        }

        const QColor background = RowBackgroundForChannel(record->channel);
        if (background.isValid()) {
            return background;
        }
    }

    if (role == Qt::ToolTipRole) {
        const QString fieldName = EditFieldNameForIndex(*record, index.column(), optionalFieldName);
        if (!fieldName.isEmpty() && fieldName != QLatin1String("Xact")
            && traceMetadata() && record->rawRecordAvailable) {
            const FlitEditCellState state = FlitEditAdapter::cellState(*record, traceMetadata(), fieldName);
            if (!state.applicable && !state.reason.isEmpty()) {
                return state.reason;
            }
            if (editable_ && !state.reason.isEmpty()) {
                return state.reason;
            }
        }

        return QStringLiteral("%1\n%2")
            .arg(record->summary,
                 record->annotation.isEmpty() ? record->transactionLabel : record->annotation);
    }

    return {};
}

QVariant FlitTableModel::headerData(const int section,
                                    const Qt::Orientation orientation,
                                    const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    if (section >= ColumnCount) {
        return visibleOptionalFields_.value(section - ColumnCount);
    }

    switch (section) {
    case XactionIndexColumn:
        return QStringLiteral("Xact");
    case TimeColumn:
        return QStringLiteral("Time");
    case ChannelColumn:
        return QStringLiteral("Channel");
    case DirectionColumn:
        return QStringLiteral("Direction");
    case OpcodeColumn:
        return QStringLiteral("Opcode");
    case SourceColumn:
        return QStringLiteral("SrcID");
    case TargetColumn:
        return QStringLiteral("TgtID");
    case TxnColumn:
        return QStringLiteral("TxnID");
    case AddressColumn:
        return QStringLiteral("Address/DBID");
    case DataIdColumn:
        return QStringLiteral("DataID");
    case RespColumn:
        return QStringLiteral("Resp");
    case FwdStateColumn:
        return QStringLiteral("FwdState");
    case RespErrColumn:
        return QStringLiteral("RespErr");
    default:
        return {};
    }
}

Qt::ItemFlags FlitTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (!index.isValid() || !editable_) {
        return result;
    }

    const FlitRecord* record = recordAt(index.row());
    if (!record) {
        return result;
    }

    const bool isOptionalFieldColumn = index.column() >= ColumnCount;
    const QString optionalFieldName = isOptionalFieldColumn
        ? visibleOptionalFields_.value(index.column() - ColumnCount)
        : QString();
    const QString fieldName = EditFieldNameForIndex(*record, index.column(), optionalFieldName);
    const FlitEditCellState state = FlitEditAdapter::cellState(*record, traceMetadata(), fieldName);
    if (state.editable) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool FlitTableModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
    if (role != Qt::EditRole || !index.isValid() || !editable_) {
        return false;
    }

    const int logicalRow = logicalRowAt(index.row());
    if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
        return false;
    }

    const FlitRecord* sourceRecord = recordForLogicalRow(logicalRow);
    if (!sourceRecord) {
        return false;
    }

    FlitRecord editedRecord = *sourceRecord;
    const bool isOptionalFieldColumn = index.column() >= ColumnCount;
    const QString optionalFieldName = isOptionalFieldColumn
        ? visibleOptionalFields_.value(index.column() - ColumnCount)
        : QString();
    const QString fieldName = EditFieldNameForIndex(editedRecord, index.column(), optionalFieldName);
    FlitEditResult editResult = FlitEditAdapter::applyEdit(editedRecord, traceMetadata(), fieldName, value.toString());
    if (!editResult.ok) {
        Q_EMIT editRejected(editResult.summary, editResult.detail);
        return false;
    }
    if (FlitRecordsEqualForUndo(*sourceRecord, editedRecord)) {
        return true;
    }

    undoStack_->push(new FlitTableModelSetRowCommand(this, logicalRow, *sourceRecord, std::move(editedRecord)));
    return true;
}

void FlitTableModel::resetUndoStack()
{
    if (undoStack_) {
        undoStack_->clear();
        undoStack_->setClean();
    }
    emitUndoRedoAvailability();
}

void FlitTableModel::clear()
{
    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    traceSession_.reset();
    traceMetadataOverride_.reset();
    rows_.clear();
    editedRows_.clear();
    insertedRows_.clear();
    sparseRowRefs_.clear();
    rows_.shrink_to_fit();
    visibleRows_.clear();
    visibleRows_.shrink_to_fit();
    identityVisibleRows_ = false;
    identityVisibleRowCount_ = 0;
    channelCounts_.fill(0);
    availableOptionalFields_.clear();
    availableOptionalFieldsByScope_.clear();
    visibleOptionalFields_.clear();
    optionalFieldDisplayModes_.clear();
    optionalFieldFilters_.clear();
    editable_ = false;
    resetUndoStack();
    setDirty(false);
    endResetModel();
}

void FlitTableModel::setTraceSession(std::shared_ptr<TraceSession> session)
{
    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    traceSession_ = std::move(session);
    traceMetadataOverride_.reset();
    rows_.clear();
    editedRows_.clear();
    insertedRows_.clear();
    sparseRowRefs_.clear();
    rows_.shrink_to_fit();
    visibleRows_.clear();
    visibleRows_.shrink_to_fit();
    identityVisibleRows_ = false;
    identityVisibleRowCount_ = 0;
    recountChannels();
    rebuildAvailableOptionalFields();
    rebuildVisibleRowsNoReset();
    editable_ = false;
    resetUndoStack();
    setDirty(false);
    endResetModel();
    if (searchMode_ == SearchMode::Highlight) {
        rebuildSearchHighlightIndex();
    }
}

void FlitTableModel::setTraceMetadataOverride(std::optional<CLogBTraceMetadata> metadata)
{
    traceMetadataOverride_ = std::move(metadata);
    if (rowCount() > 0 && columnCount() > 0) {
        Q_EMIT dataChanged(index(0, 0),
                           index(rowCount() - 1, columnCount() - 1),
                           {Qt::ForegroundRole, Qt::BackgroundRole, Qt::ToolTipRole});
    }
}

void FlitTableModel::refreshTraceRows()
{
    const int rows = rowCount();
    const int columns = columnCount();
    if (rows <= 0 || columns <= 0) {
        return;
    }

    Q_EMIT dataChanged(index(0, 0),
                       index(rows - 1, columns - 1),
                       {Qt::DisplayRole,
                        Qt::ToolTipRole,
                        Qt::BackgroundRole,
                       Qt::ForegroundRole,
                       TransactionHighlightRole,
                       XactionIndexStateRole,
                       ChannelTagRole,
                       NodeLabelTextRole,
                       NodeLabelColorRole});
}

void FlitTableModel::refreshTraceRowRange(const int firstRow, const int lastRow)
{
    const int rows = rowCount();
    const int columns = columnCount();
    if (rows <= 0 || columns <= 0 || lastRow < firstRow) {
        return;
    }

    const int clampedFirstRow = qBound(0, firstRow, rows - 1);
    const int clampedLastRow = qBound(0, lastRow, rows - 1);
    if (clampedLastRow < clampedFirstRow) {
        return;
    }

    Q_EMIT dataChanged(index(clampedFirstRow, 0),
                       index(clampedLastRow, columns - 1),
                       {Qt::DisplayRole,
                        Qt::ToolTipRole,
                        Qt::BackgroundRole,
                        Qt::ForegroundRole,
                        TransactionHighlightRole,
                        XactionIndexStateRole,
                        ChannelTagRole,
                        NodeLabelTextRole,
                        NodeLabelColorRole});
}

void FlitTableModel::setRows(std::vector<FlitRecord> rows)
{
    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    traceSession_.reset();
    rows_ = std::move(rows);
    editedRows_.clear();
    insertedRows_.clear();
    sparseRowRefs_.clear();
    identityVisibleRows_ = false;
    identityVisibleRowCount_ = 0;
    recountChannels();
    rebuildAvailableOptionalFields();
    rebuildVisibleRowsNoReset();
    editable_ = false;
    resetUndoStack();
    setDirty(false);
    endResetModel();
    if (searchMode_ == SearchMode::Highlight) {
        rebuildSearchHighlightIndex();
    }
}

void FlitTableModel::appendRow(const FlitRecord& row)
{
    appendRows(std::vector<FlitRecord>{row});
}

void FlitTableModel::appendRows(const std::vector<FlitRecord>& rows)
{
    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    traceSession_.reset();
    editedRows_.clear();
    insertedRows_.clear();
    sparseRowRefs_.clear();
    rows_.insert(rows_.end(), rows.begin(), rows.end());
    identityVisibleRows_ = false;
    identityVisibleRowCount_ = 0;
    recountChannels();
    rebuildAvailableOptionalFields();
    rebuildVisibleRowsNoReset();
    setDirty(editable_ || dirty_);
    endResetModel();
    if (searchMode_ == SearchMode::Highlight) {
        rebuildSearchHighlightIndex();
    }
}

bool FlitTableModel::insertRowsAt(const int visibleRow, std::vector<FlitRecord> rows)
{
    if (!editable_ || rows.empty()) {
        return false;
    }

    int logicalInsertRow = sourceRowCount();
    if (visibleRow >= 0 && visibleRow < visibleRowCountInternal()) {
        logicalInsertRow = logicalRowAt(visibleRow);
        if (logicalInsertRow < 0) {
            logicalInsertRow = sourceRowCount();
        }
    }
    logicalInsertRow = qBound(0, logicalInsertRow, sourceRowCount());
    return insertRowsAtLogicalRow(logicalInsertRow, std::move(rows));
}

bool FlitTableModel::insertRowsAtLogicalRow(const int logicalRow, std::vector<FlitRecord> rows)
{
    if (!editable_ || rows.empty()) {
        return false;
    }

    undoStack_->push(new FlitTableModelInsertRowsCommand(this,
                                                         qBound(0, logicalRow, sourceRowCount()),
                                                         std::move(rows)));
    return true;
}

bool FlitTableModel::insertRowsByTimestamp(std::vector<FlitRecord> rows)
{
    if (!editable_ || rows.empty()) {
        return false;
    }

    undoStack_->push(new FlitTableModelInsertRowsCommand(this, std::move(rows)));
    return true;
}

bool FlitTableModel::cloneRowsAt(const int visibleRow, const int count)
{
    if (!editable_ || count <= 0) {
        return false;
    }

    const FlitRecord* source = recordAt(visibleRow);
    if (!source) {
        return false;
    }

    std::vector<FlitRecord> rows;
    rows.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        FlitRecord clone = *source;
        clone.timestamp += index + 1;
        clone.transactionGroupKey.clear();
        clone.xactionDebugLog.clear();
        clone.xactionIndexChecked = false;
        clone.xactionIndexed = false;
        clone.xactionIndexProcessedByJoint = false;
        clone.xactionIndexState = XactionIndexState::NotStarted;
        rows.push_back(std::move(clone));
    }
    return insertRowsAt(visibleRow + 1, std::move(rows));
}

bool FlitTableModel::deleteRowAt(const int visibleRow)
{
    if (!editable_) {
        return false;
    }

    const int logicalRow = logicalRowAt(visibleRow);
    if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
        return false;
    }
    if (!recordForLogicalRow(logicalRow)) {
        return false;
    }

    undoStack_->push(new FlitTableModelDeleteRowCommand(this, logicalRow));
    return true;
}

bool FlitTableModel::deleteRowsAt(QVector<int> visibleRows)
{
    if (!editable_ || visibleRows.isEmpty()) {
        return false;
    }

    std::sort(visibleRows.begin(), visibleRows.end(), std::greater<int>());
    int previous = -1;
    bool deletedAny = false;
    for (const int visibleRow : std::as_const(visibleRows)) {
        if (visibleRow == previous) {
            continue;
        }
        previous = visibleRow;
        deletedAny = deleteRowAt(visibleRow) || deletedAny;
    }
    return deletedAny;
}

bool FlitTableModel::removeRowsAt(QVector<int> visibleRows)
{
    if (traceSession_ || visibleRows.isEmpty()) {
        return false;
    }

    std::sort(visibleRows.begin(), visibleRows.end(), std::greater<int>());
    bool removedAny = false;
    int previous = -1;
    for (const int visibleRow : std::as_const(visibleRows)) {
        if (visibleRow == previous) {
            continue;
        }
        previous = visibleRow;
        const int logicalRow = logicalRowAt(visibleRow);
        if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
            continue;
        }
        RowMutationSnapshot ignored;
        removedAny = applyDeleteLogicalRowDirect(logicalRow, &ignored) || removedAny;
    }
    if (removedAny) {
        setDirty(false);
        markUndoStackClean();
    }
    return removedAny;
}

bool FlitTableModel::canUndo() const noexcept
{
    return editable_ && undoStack_ && undoStack_->canUndo();
}

bool FlitTableModel::canRedo() const noexcept
{
    return editable_ && undoStack_ && undoStack_->canRedo();
}

void FlitTableModel::undoEdit()
{
    if (canUndo()) {
        undoStack_->undo();
    }
}

void FlitTableModel::redoEdit()
{
    if (canRedo()) {
        undoStack_->redo();
    }
}

void FlitTableModel::markUndoStackClean()
{
    if (!undoStack_) {
        return;
    }
    undoStack_->setClean();
    updateDirtyFromUndoStack();
}

QString FlitTableModel::undoText() const
{
    return undoStack_ ? undoStack_->undoText() : QString();
}

QString FlitTableModel::redoText() const
{
    return undoStack_ ? undoStack_->redoText() : QString();
}

void FlitTableModel::setOpcodeFilter(const QString& text)
{
    opcodeFilter_ = text;
    applySearchFilterChange();
}

void FlitTableModel::setSourceIdFilter(const QString& text)
{
    sourceIdFilter_ = NormalizeNumericFilterInput(text);
    applySearchFilterChange();
}

void FlitTableModel::setTargetIdFilter(const QString& text)
{
    targetIdFilter_ = NormalizeNumericFilterInput(text);
    applySearchFilterChange();
}

void FlitTableModel::setTxnIdFilter(const QString& text)
{
    txnIdFilter_ = NormalizeNumericFilterInput(text);
    applySearchFilterChange();
}

void FlitTableModel::setDbidFilter(const QString& text)
{
    dbidFilter_ = NormalizeNumericFilterInput(text);
    applySearchFilterChange();
}

void FlitTableModel::setAddressFilter(const QString& text)
{
    addressFilter_ = NormalizeNumericFilterInput(text);
    applySearchFilterChange();
}

QString FlitTableModel::opcodeFilter() const
{
    return opcodeFilter_;
}

QString FlitTableModel::sourceIdFilter() const
{
    return sourceIdFilter_;
}

QString FlitTableModel::targetIdFilter() const
{
    return targetIdFilter_;
}

QString FlitTableModel::txnIdFilter() const
{
    return txnIdFilter_;
}

QString FlitTableModel::dbidFilter() const
{
    return dbidFilter_;
}

QString FlitTableModel::addressFilter() const
{
    return addressFilter_;
}

void FlitTableModel::cancelPendingFilterBuild()
{
    cancelAsyncFilterBuild();
}

FlitTableModel::SearchMode FlitTableModel::searchMode() const noexcept
{
    return searchMode_;
}

void FlitTableModel::setSearchMode(const SearchMode mode)
{
    if (searchMode_ == mode) {
        return;
    }

    searchMode_ = mode;
    rebuildVisibleRows();
}

QStringList FlitTableModel::availableOptionalFields() const
{
    return availableOptionalFields_;
}

QStringList FlitTableModel::availableOptionalFieldsForScope(const QString& scope) const
{
    return availableOptionalFieldsByScope_.value(scope);
}

bool FlitTableModel::isFieldColumnVisible(const QString& fieldName) const noexcept
{
    return visibleOptionalFields_.contains(fieldName);
}

void FlitTableModel::setFieldColumnVisible(const QString& fieldName, const bool visible)
{
    if (!availableOptionalFields_.contains(fieldName)) {
        return;
    }

    const bool currentlyVisible = visibleOptionalFields_.contains(fieldName);
    if (currentlyVisible == visible) {
        return;
    }

    const bool filterRemoved = !visible && optionalFieldFilters_.contains(fieldName);

    beginResetModel();
    if (visible) {
        const int availableIndex = availableOptionalFields_.indexOf(fieldName);
        int insertIndex = visibleOptionalFields_.size();
        for (int index = 0; index < visibleOptionalFields_.size(); ++index) {
            if (availableOptionalFields_.indexOf(visibleOptionalFields_[index]) > availableIndex) {
                insertIndex = index;
                break;
            }
        }
        visibleOptionalFields_.insert(insertIndex, fieldName);
    } else {
        visibleOptionalFields_.removeAll(fieldName);
        optionalFieldFilters_.remove(fieldName);
    }
    if (!filterRemoved) {
        rebuildVisibleRowsNoReset();
    }
    endResetModel();

    if (filterRemoved) {
        rebuildVisibleRows();
    }
}

void FlitTableModel::setFieldFilter(const QString& fieldName, const QString& text)
{
    if (!visibleOptionalFields_.contains(fieldName)) {
        return;
    }

    if (text.isEmpty()) {
        optionalFieldFilters_.remove(fieldName);
    } else {
        optionalFieldFilters_.insert(fieldName, text);
    }
    applySearchFilterChange();
}

QString FlitTableModel::fieldFilter(const QString& fieldName) const
{
    return optionalFieldFilters_.value(fieldName);
}

int FlitTableModel::columnForField(const QString& fieldName) const noexcept
{
    const int index = visibleOptionalFields_.indexOf(fieldName);
    return index < 0 ? -1 : ColumnCount + index;
}

void FlitTableModel::setChannelVisible(const FlitChannel channel, const bool visible)
{
    switch (channel) {
    case FlitChannel::Req:
        showReq_ = visible;
        break;
    case FlitChannel::Rsp:
        showRsp_ = visible;
        break;
    case FlitChannel::Dat:
        showDat_ = visible;
        break;
    case FlitChannel::Snp:
        showSnp_ = visible;
        break;
    }

    rebuildVisibleRows();
}

void FlitTableModel::setDirectionVisible(const FlitDirection direction, const bool visible)
{
    if (direction == FlitDirection::Tx) {
        showTx_ = visible;
    } else {
        showRx_ = visible;
    }

    rebuildVisibleRows();
}

bool FlitTableModel::channelVisible(const FlitChannel channel) const noexcept
{
    return channelEnabled(channel);
}

bool FlitTableModel::directionVisible(const FlitDirection direction) const noexcept
{
    return directionEnabled(direction);
}

void FlitTableModel::setXactionDeniedOnlyFilter(const bool enabled)
{
    if (showXactionDeniedOnly_ == enabled) {
        return;
    }

    showXactionDeniedOnly_ = enabled;
    rebuildVisibleRows();
}

bool FlitTableModel::xactionDeniedOnlyFilter() const noexcept
{
    return showXactionDeniedOnly_;
}

FlitTableModel::DisplayMode FlitTableModel::displayMode(const int column) const noexcept
{
    if (column < 0) {
        return DisplayMode::Decoded;
    }

    if (column < ColumnCount) {
        return displayModes_[static_cast<std::size_t>(column)];
    }

    return optionalFieldDisplayModes_.value(visibleOptionalFields_.value(column - ColumnCount),
                                            DisplayMode::Decoded);
}

bool FlitTableModel::supportsDisplayMode(const int column) const noexcept
{
    if (column < 0 || column >= columnCount()) {
        return false;
    }

    return column != XactionIndexColumn && column != ChannelColumn && column != DirectionColumn;
}

void FlitTableModel::setDisplayMode(const int column, const DisplayMode mode)
{
    if (!supportsDisplayMode(column)) {
        return;
    }

    if (column < ColumnCount) {
        const std::size_t index = static_cast<std::size_t>(column);
        if (displayModes_[index] == mode) {
            return;
        }

        displayModes_[index] = mode;
    } else {
        const QString fieldName = visibleOptionalFields_.value(column - ColumnCount);
        if (fieldName.isEmpty() || optionalFieldDisplayModes_.value(fieldName, DisplayMode::Decoded) == mode) {
            return;
        }

        optionalFieldDisplayModes_.insert(fieldName, mode);
    }

    if (rowCount() > 0) {
        const QModelIndex topLeft = this->index(0, column);
        const QModelIndex bottomRight = this->index(rowCount() - 1, column);
        Q_EMIT dataChanged(topLeft, bottomRight, {Qt::DisplayRole});
    }
    Q_EMIT headerDataChanged(Qt::Horizontal, column, column);
}

bool FlitTableModel::nodeLabelsVisible() const noexcept
{
    return nodeLabelsVisible_;
}

void FlitTableModel::setNodeLabelsVisible(const bool visible)
{
    if (nodeLabelsVisible_ == visible) {
        return;
    }

    nodeLabelsVisible_ = visible;
    const int rows = rowCount();
    const int columns = columnCount();
    if (rows <= 0 || columns <= 0) {
        return;
    }

    Q_EMIT dataChanged(index(0, 0),
                       index(rows - 1, columns - 1),
                       {Qt::DisplayRole,
                        Qt::SizeHintRole,
                        NodeLabelTextRole,
                        NodeLabelColorRole,
                        ChannelTagRole});
}

int FlitTableModel::totalCount() const noexcept
{
    return sourceRowCount();
}

int FlitTableModel::visibleCount() const noexcept
{
    return visibleRowCountInternal();
}

int FlitTableModel::channelCountAll(const FlitChannel channel) const noexcept
{
    switch (channel) {
    case FlitChannel::Req:
        return channelCounts_[0];
    case FlitChannel::Rsp:
        return channelCounts_[1];
    case FlitChannel::Dat:
        return channelCounts_[2];
    case FlitChannel::Snp:
        return channelCounts_[3];
    }

    return 0;
}

bool FlitTableModel::isSessionBacked() const noexcept
{
    return static_cast<bool>(traceSession_);
}

bool FlitTableModel::editable() const noexcept
{
    return editable_;
}

void FlitTableModel::setEditable(const bool editable)
{
    if (editable_ == editable) {
        return;
    }

    editable_ = editable;
    if (!editable_ && traceSession_) {
        beginResetModel();
        editedRows_.clear();
        insertedRows_.clear();
        sparseRowRefs_.clear();
        resetUndoStack();
        setDirty(false);
        recountChannels();
        rebuildAvailableOptionalFields();
        rebuildVisibleRowsNoReset();
        endResetModel();
    }
    if (rowCount() > 0 && columnCount() > 0) {
        Q_EMIT dataChanged(index(0, 0),
                           index(rowCount() - 1, columnCount() - 1),
                           {Qt::ForegroundRole, Qt::BackgroundRole, Qt::ToolTipRole});
    }
    emitUndoRedoAvailability();
}

bool FlitTableModel::isDirty() const noexcept
{
    return dirty_;
}

void FlitTableModel::setDirty(const bool dirty)
{
    if (dirty_ == dirty) {
        return;
    }

    dirty_ = dirty;
    Q_EMIT dirtyChanged(dirty_);
}

bool FlitTableModel::isIdentityVisibleRows() const noexcept
{
    return identityVisibleRows_;
}

const CLogBTraceMetadata* FlitTableModel::traceMetadata() const noexcept
{
    if (traceSession_) {
        return &traceSession_->metadata();
    }
    return traceMetadataOverride_ ? &*traceMetadataOverride_ : nullptr;
}

QVector<const FlitRecord*> FlitTableModel::visibleRows() const
{
    QVector<const FlitRecord*> rows;
    rows.reserve(static_cast<qsizetype>(visibleRowCountInternal()));
    for (int visibleRow = 0; visibleRow < visibleRowCountInternal(); ++visibleRow) {
        rows.push_back(recordAt(visibleRow));
    }
    return rows;
}

std::vector<FlitRecord> FlitTableModel::sourceRowsSnapshot() const
{
    if (!traceSession_) {
        return rows_;
    }

    return {};
}

std::vector<FlitRecord> FlitTableModel::sparseEditedRowsSnapshot() const
{
    std::vector<FlitRecord> rows;
    rows.reserve(static_cast<std::size_t>(editedRows_.size()) + insertedRows_.size());
    for (auto it = editedRows_.cbegin(); it != editedRows_.cend(); ++it) {
        rows.push_back(it.value());
    }
    if (hasSparseRowMap()) {
        QSet<int> liveInsertedRows;
        for (const SourceRowRef& ref : sparseRowRefs_) {
            if (ref.insertedRow >= 0) {
                liveInsertedRows.insert(ref.insertedRow);
            }
        }
        for (const int insertedRow : liveInsertedRows) {
            if (insertedRow < static_cast<int>(insertedRows_.size())) {
                rows.push_back(insertedRows_[static_cast<std::size_t>(insertedRow)]);
            }
        }
    } else {
        rows.insert(rows.end(), insertedRows_.begin(), insertedRows_.end());
    }
    return rows;
}

bool FlitTableModel::hasSparseEdits() const noexcept
{
    return !editedRows_.isEmpty() || (traceSession_ && hasSparseRowMap())
        || (!traceSession_ && !insertedRows_.empty());
}

bool FlitTableModel::forEachSourceRowInOrder(
    const std::function<bool(const FlitRecord&, int)>& callback,
    CLogBTraceLoadError& error) const
{
    error = {};
    if (!callback) {
        return true;
    }

    if (!traceSession_) {
        for (int logicalRow = 0; logicalRow < static_cast<int>(rows_.size()); ++logicalRow) {
            if (!callback(rows_[static_cast<std::size_t>(logicalRow)], logicalRow)) {
                return true;
            }
        }
        return true;
    }

    const int totalRows = sourceRowCount();
    std::uint64_t originalReadStart = 0;
    std::vector<FlitRecord> originalRows;

    for (int logicalRow = 0; logicalRow < totalRows; ++logicalRow) {
        const SourceRowRef rowRef = sourceRowRefAt(logicalRow);
        if (rowRef.insertedRow >= 0) {
            if (rowRef.insertedRow >= static_cast<int>(insertedRows_.size())) {
                error.summary = QStringLiteral("Sparse edit row map references an invalid inserted row.");
                return false;
            }
            if (!callback(insertedRows_[static_cast<std::size_t>(rowRef.insertedRow)], logicalRow)) {
                return true;
            }
            continue;
        }

        if (rowRef.originalRow < 0) {
            error.summary = QStringLiteral("Sparse edit row map references an invalid source row.");
            return false;
        }

        const auto edited = editedRows_.constFind(rowRef.originalRow);
        if (edited != editedRows_.cend()) {
            if (!callback(edited.value(), logicalRow)) {
                return true;
            }
            continue;
        }

        if (originalRows.empty()
            || static_cast<std::uint64_t>(rowRef.originalRow) < originalReadStart
            || static_cast<std::uint64_t>(rowRef.originalRow) >= originalReadStart + originalRows.size()) {
            originalReadStart = static_cast<std::uint64_t>(rowRef.originalRow);
            const std::uint64_t readCount = std::min<std::uint64_t>(
                traceSession_->pageSize(),
                traceSession_->totalRows() - originalReadStart);
            if (!traceSession_->readRows(originalReadStart, readCount, originalRows, error)) {
                return false;
            }
        }

        const std::size_t localIndex =
            static_cast<std::size_t>(static_cast<std::uint64_t>(rowRef.originalRow) - originalReadStart);
        if (localIndex >= originalRows.size()) {
            error.summary = QStringLiteral("Trace row streaming lost a requested source row.");
            return false;
        }
        if (!callback(originalRows[localIndex], logicalRow)) {
            return true;
        }
    }

    return true;
}

int FlitTableModel::logicalRowAt(const int visibleRow) const noexcept
{
    if (visibleRow < 0 || visibleRow >= visibleRowCountInternal()) {
        return -1;
    }

    return identityVisibleRows_
        ? visibleRow
        : visibleRows_[static_cast<std::size_t>(visibleRow)];
}

const FlitRecord* FlitTableModel::tryRecordAt(const int visibleRow) const noexcept
{
    if (visibleRow < 0 || visibleRow >= visibleRowCountInternal()) {
        return nullptr;
    }

    return tryRecordForLogicalRow(logicalRowAt(visibleRow));
}

const FlitRecord* FlitTableModel::recordAt(const int visibleRow) const
{
    if (visibleRow < 0 || visibleRow >= visibleRowCountInternal()) {
        return nullptr;
    }

    return recordForLogicalRow(logicalRowAt(visibleRow));
}

void FlitTableModel::setTransactionHighlightFromVisibleRow(const int visibleRow)
{
    const int logicalRow = logicalRowAt(visibleRow);
    const FlitRecord* record = recordAt(visibleRow);
    if (logicalRow < 0 || !record) {
        clearTransactionHighlight();
        return;
    }

    QSet<QString> keys;
    for (const QString& key : TransactionGroupKeys(*record)) {
        if (!key.trimmed().isEmpty()) {
            keys.insert(key);
        }
    }

    QSet<QString> xactionKeys;
    for (const QString& key : keys) {
        if (key.startsWith(QStringLiteral("xaction|"))) {
            xactionKeys.insert(key);
        }
    }
    if (!xactionKeys.isEmpty()) {
        keys = std::move(xactionKeys);
    }

    if (keys.isEmpty()) {
        keys.insert(QStringLiteral("logical-row|%1").arg(logicalRow));
    }

    if (transactionHighlightAnchorLogicalRow_ == logicalRow
        && transactionHighlightKeys_ == keys) {
        return;
    }

    transactionHighlightAnchorLogicalRow_ = logicalRow;
    transactionHighlightKeys_ = std::move(keys);
    refreshTransactionHighlights();
}

bool FlitTableModel::toggleTransactionHighlightFromVisibleRow(const int visibleRow)
{
    if (isTransactionHighlightAnchorRow(visibleRow)) {
        clearTransactionHighlight();
        return false;
    }

    setTransactionHighlightFromVisibleRow(visibleRow);
    return isTransactionHighlightedRow(visibleRow);
}

void FlitTableModel::clearTransactionHighlight()
{
    if (!clearTransactionHighlightWithoutRefresh()) {
        return;
    }

    refreshTransactionHighlights();
}

bool FlitTableModel::clearTransactionHighlightWithoutRefresh()
{
    if (transactionHighlightAnchorLogicalRow_ < 0 && transactionHighlightKeys_.isEmpty()) {
        return false;
    }

    clearTransactionHighlightState();
    return true;
}

bool FlitTableModel::isTransactionHighlightAnchorRow(const int visibleRow) const
{
    const int logicalRow = logicalRowAt(visibleRow);
    return logicalRow >= 0 && logicalRow == transactionHighlightAnchorLogicalRow_;
}

bool FlitTableModel::isTransactionHighlightedRow(const int visibleRow) const
{
    const int logicalRow = logicalRowAt(visibleRow);
    const FlitRecord* record = tryRecordAt(visibleRow);
    return record && recordMatchesTransactionHighlight(*record, logicalRow);
}

bool FlitTableModel::isSearchHighlightedRow(const int visibleRow) const
{
    if (searchMode_ != SearchMode::Highlight || !hasActiveSearchFilters()) {
        return false;
    }

    const FlitRecord* record = recordAt(visibleRow);
    return record && passesFilters(*record);
}

int FlitTableModel::findSearchHighlightedRow(const int currentVisibleRow, const bool forward) const
{
    if (searchMode_ != SearchMode::Highlight || !hasActiveSearchFilters()) {
        return -1;
    }

    if (highlightedRows_.empty()) {
        return -1;
    }

    if (forward) {
        const auto found = std::upper_bound(highlightedRows_.begin(),
                                            highlightedRows_.end(),
                                            currentVisibleRow);
        return found == highlightedRows_.end() ? highlightedRows_.front() : *found;
    }

    const auto found = std::lower_bound(highlightedRows_.begin(),
                                        highlightedRows_.end(),
                                        currentVisibleRow);
    if (found == highlightedRows_.begin()) {
        return highlightedRows_.back();
    }
    return *(found - 1);
}

FlitTableModel::SearchHighlightNavigationIndex
FlitTableModel::searchHighlightNavigationIndex(const int currentVisibleRow) const
{
    SearchHighlightNavigationIndex index;
    if (searchMode_ != SearchMode::Highlight || !hasActiveSearchFilters()) {
        return index;
    }

    index.total = static_cast<int>(std::min<std::size_t>(
        highlightedRows_.size(),
        static_cast<std::size_t>(std::numeric_limits<int>::max())));

    const auto found = std::lower_bound(highlightedRows_.begin(),
                                        highlightedRows_.end(),
                                        currentVisibleRow);
    if (found != highlightedRows_.end() && *found == currentVisibleRow) {
        index.offset = static_cast<int>(std::distance(highlightedRows_.begin(), found)) + 1;
    }

    return index;
}

bool FlitTableModel::isSearchHighlightIndexBuilding() const noexcept
{
    return highlightSearchIndexBuild_ != nullptr && !highlightIndexComplete_;
}

void FlitTableModel::emitUndoRedoAvailability()
{
    Q_EMIT undoRedoAvailabilityChanged(canUndo(), canRedo());
}

void FlitTableModel::updateDirtyFromUndoStack()
{
    setDirty(undoStack_ ? !undoStack_->isClean() : dirty_);
}

void FlitTableModel::finishRowMutationReset()
{
    recountChannels();
    rebuildAvailableOptionalFields();
    rebuildVisibleRowsNoReset();
    endResetModel();
    if (searchMode_ == SearchMode::Highlight) {
        rebuildSearchHighlightIndex();
    }
    updateDirtyFromUndoStack();
    Q_EMIT rowsEdited();
    emitUndoRedoAvailability();
}

bool FlitTableModel::applySetLogicalRowDirect(const int logicalRow, const FlitRecord& row)
{
    if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
        return false;
    }

    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    if (traceSession_) {
        const SourceRowRef rowRef = sourceRowRefAt(logicalRow);
        if (rowRef.insertedRow >= 0) {
            if (rowRef.insertedRow >= static_cast<int>(insertedRows_.size())) {
                endResetModel();
                return false;
            }
            insertedRows_[static_cast<std::size_t>(rowRef.insertedRow)] = row;
        } else if (rowRef.originalRow >= 0) {
            editedRows_.insert(rowRef.originalRow, row);
        } else {
            endResetModel();
            return false;
        }
    } else {
        if (logicalRow >= static_cast<int>(rows_.size())) {
            endResetModel();
            return false;
        }
        rows_[static_cast<std::size_t>(logicalRow)] = row;
    }
    finishRowMutationReset();
    return true;
}

bool FlitTableModel::applyInsertRowsAtLogicalRowDirect(const int logicalRow,
                                                       const std::vector<FlitRecord>& rows,
                                                       RowMutationSnapshot* snapshot)
{
    if (rows.empty()) {
        return false;
    }

    const int logicalInsertRow = qBound(0, logicalRow, sourceRowCount());
    std::vector<RowMutationEntry> entries;
    entries.reserve(rows.size());

    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    if (traceSession_) {
        if (!hasSparseRowMap()) {
            rebuildSparseRowRefs();
        }
        for (std::size_t index = 0; index < rows.size(); ++index) {
            const int insertedIndex = static_cast<int>(insertedRows_.size());
            insertedRows_.push_back(rows[index]);
            const int targetLogicalRow = logicalInsertRow + static_cast<int>(index);
            const SourceRowRef ref{.originalRow = -1, .insertedRow = insertedIndex};
            sparseRowRefs_.insert(sparseRowRefs_.begin() + static_cast<std::ptrdiff_t>(targetLogicalRow), ref);
            entries.push_back(RowMutationEntry{.logicalRow = targetLogicalRow, .ref = ref, .row = rows[index]});
        }
    } else {
        rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(logicalInsertRow),
                     rows.begin(),
                     rows.end());
        for (std::size_t index = 0; index < rows.size(); ++index) {
            const int targetLogicalRow = logicalInsertRow + static_cast<int>(index);
            entries.push_back(RowMutationEntry{
                .logicalRow = targetLogicalRow,
                .ref = SourceRowRef{.originalRow = targetLogicalRow, .insertedRow = -1},
                .row = rows[index],
            });
        }
    }
    if (snapshot) {
        *snapshot = MakeSnapshot(std::move(entries));
    }
    finishRowMutationReset();
    return true;
}

bool FlitTableModel::applyInsertRowsByTimestampDirect(const std::vector<FlitRecord>& inputRows,
                                                      RowMutationSnapshot* snapshot)
{
    if (inputRows.empty()) {
        return false;
    }

    std::vector<FlitRecord> rows = inputRows;
    std::stable_sort(rows.begin(), rows.end(), [](const FlitRecord& lhs, const FlitRecord& rhs) {
        return lhs.timestamp < rhs.timestamp;
    });

    std::vector<RowMutationEntry> entries;
    entries.reserve(rows.size());
    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    if (traceSession_) {
        if (!hasSparseRowMap()) {
            rebuildSparseRowRefs();
        }

        for (const FlitRecord& row : rows) {
            const int insertedIndex = static_cast<int>(insertedRows_.size());
            insertedRows_.push_back(row);

            int low = 0;
            int high = static_cast<int>(sparseRowRefs_.size());
            while (low < high) {
                const int middle = low + ((high - low) / 2);
                const FlitRecord* middleRow = recordForLogicalRow(middle);
                if (!middleRow) {
                    high = middle;
                    continue;
                }
                if (middleRow->timestamp <= row.timestamp) {
                    low = middle + 1;
                } else {
                    high = middle;
                }
            }

            const SourceRowRef ref{.originalRow = -1, .insertedRow = insertedIndex};
            sparseRowRefs_.insert(sparseRowRefs_.begin() + static_cast<std::ptrdiff_t>(low), ref);
            entries.push_back(RowMutationEntry{.logicalRow = low, .ref = ref, .row = row});
        }
    } else {
        for (const FlitRecord& row : rows) {
            const auto insertIt = std::upper_bound(
                rows_.begin(),
                rows_.end(),
                row.timestamp,
                [](const qint64 timestamp, const FlitRecord& existing) {
                    return timestamp < existing.timestamp;
                });
            const int logicalRow = static_cast<int>(std::distance(rows_.begin(), insertIt));
            rows_.insert(insertIt, row);
            entries.push_back(RowMutationEntry{
                .logicalRow = logicalRow,
                .ref = SourceRowRef{.originalRow = logicalRow, .insertedRow = -1},
                .row = row,
            });
        }
    }
    if (snapshot) {
        *snapshot = MakeSnapshot(std::move(entries));
    }
    finishRowMutationReset();
    return true;
}

bool FlitTableModel::applyDeleteLogicalRowDirect(const int logicalRow, RowMutationSnapshot* snapshot)
{
    if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
        return false;
    }

    const FlitRecord* row = recordForLogicalRow(logicalRow);
    if (!row) {
        return false;
    }

    RowMutationEntry entry;
    entry.logicalRow = logicalRow;
    entry.ref = sourceRowRefAt(logicalRow);
    entry.row = *row;

    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    if (traceSession_) {
        if (!hasSparseRowMap()) {
            rebuildSparseRowRefs();
        }
        if (logicalRow < 0 || logicalRow >= static_cast<int>(sparseRowRefs_.size())) {
            endResetModel();
            return false;
        }
        sparseRowRefs_.erase(sparseRowRefs_.begin() + logicalRow);
        if (entry.ref.originalRow >= 0) {
            editedRows_.remove(entry.ref.originalRow);
        }
    } else {
        if (logicalRow >= static_cast<int>(rows_.size())) {
            endResetModel();
            return false;
        }
        rows_.erase(rows_.begin() + logicalRow);
    }
    if (snapshot) {
        *snapshot = MakeSnapshot(std::vector<RowMutationEntry>{std::move(entry)});
    }
    finishRowMutationReset();
    return true;
}

bool FlitTableModel::applyRestoreRowsDirect(const RowMutationSnapshot& snapshot)
{
    if (snapshot.entries.empty()) {
        return false;
    }

    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    if (traceSession_) {
        if (!hasSparseRowMap()) {
            rebuildSparseRowRefs();
        }
        for (const RowMutationEntry& entry : snapshot.entries) {
            const int logicalRow = qBound(0, entry.logicalRow, static_cast<int>(sparseRowRefs_.size()));
            sparseRowRefs_.insert(sparseRowRefs_.begin() + static_cast<std::ptrdiff_t>(logicalRow), entry.ref);
            if (entry.ref.originalRow >= 0) {
                editedRows_.insert(entry.ref.originalRow, entry.row);
            } else if (entry.ref.insertedRow >= 0
                       && entry.ref.insertedRow < static_cast<int>(insertedRows_.size())) {
                insertedRows_[static_cast<std::size_t>(entry.ref.insertedRow)] = entry.row;
            }
        }
    } else {
        for (const RowMutationEntry& entry : snapshot.entries) {
            const int logicalRow = qBound(0, entry.logicalRow, static_cast<int>(rows_.size()));
            rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(logicalRow), entry.row);
        }
    }
    finishRowMutationReset();
    return true;
}

bool FlitTableModel::applyRemoveRowsDirect(const RowMutationSnapshot& snapshot)
{
    if (snapshot.entries.empty()) {
        return false;
    }

    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    clearTransactionHighlightState();
    beginResetModel();
    if (traceSession_) {
        if (!hasSparseRowMap()) {
            rebuildSparseRowRefs();
        }
        for (auto it = snapshot.entries.rbegin(); it != snapshot.entries.rend(); ++it) {
            const int logicalRow = sourceRowForInsertedRow(it->ref.insertedRow);
            if (logicalRow >= 0 && logicalRow < static_cast<int>(sparseRowRefs_.size())) {
                sparseRowRefs_.erase(sparseRowRefs_.begin() + logicalRow);
            }
        }
    } else {
        for (auto it = snapshot.entries.rbegin(); it != snapshot.entries.rend(); ++it) {
            if (it->logicalRow >= 0 && it->logicalRow < static_cast<int>(rows_.size())) {
                rows_.erase(rows_.begin() + it->logicalRow);
            }
        }
    }
    finishRowMutationReset();
    return true;
}

int FlitTableModel::sourceRowCount() const noexcept
{
    if (!traceSession_) {
        return static_cast<int>(rows_.size());
    }

    if (hasSparseRowMap()) {
        return static_cast<int>(sparseRowRefs_.size());
    }

    const std::uint64_t total =
        traceSession_->totalRows() + static_cast<std::uint64_t>(insertedRows_.size());
    return ClampModelCount(total);
}

bool FlitTableModel::hasSparseRowMap() const noexcept
{
    return traceSession_ && !sparseRowRefs_.empty();
}

bool FlitTableModel::sourceRowIsInserted(const int logicalRow) const noexcept
{
    return sourceRowRefAt(logicalRow).insertedRow >= 0;
}

int FlitTableModel::originalRowForSourceRow(const int logicalRow) const noexcept
{
    return sourceRowRefAt(logicalRow).originalRow;
}

int FlitTableModel::sourceRowForOriginalRow(const int originalRow) const noexcept
{
    if (originalRow < 0 || !traceSession_) {
        return originalRow;
    }

    if (!hasSparseRowMap()) {
        return originalRow < sourceRowCount() ? originalRow : -1;
    }

    const auto found = std::lower_bound(
        sparseRowRefs_.begin(),
        sparseRowRefs_.end(),
        originalRow,
        [](const SourceRowRef& ref, const int row) {
            return ref.originalRow >= 0 && ref.originalRow < row;
        });
    for (auto it = found; it != sparseRowRefs_.end(); ++it) {
        if (it->originalRow == originalRow) {
            return static_cast<int>(std::distance(sparseRowRefs_.begin(), it));
        }
        if (it->originalRow > originalRow) {
            break;
        }
    }
    return -1;
}

int FlitTableModel::sourceRowForInsertedRow(const int insertedRow) const noexcept
{
    if (insertedRow < 0 || insertedRow >= static_cast<int>(insertedRows_.size())) {
        return -1;
    }

    if (!hasSparseRowMap()) {
        return -1;
    }

    for (std::size_t index = 0; index < sparseRowRefs_.size(); ++index) {
        if (sparseRowRefs_[index].insertedRow == insertedRow) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

FlitTableModel::SourceRowRef FlitTableModel::sourceRowRefAt(const int logicalRow) const noexcept
{
    if (!traceSession_) {
        return SourceRowRef{.originalRow = logicalRow, .insertedRow = -1};
    }

    if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
        return {};
    }

    if (hasSparseRowMap()) {
        return sparseRowRefs_[static_cast<std::size_t>(logicalRow)];
    }

    return SourceRowRef{.originalRow = logicalRow, .insertedRow = -1};
}

int FlitTableModel::visibleRowCountInternal() const noexcept
{
    return identityVisibleRows_
        ? identityVisibleRowCount_
        : static_cast<int>(visibleRows_.size());
}

void FlitTableModel::rebuildSparseRowRefs()
{
    sparseRowRefs_.clear();
    if (!traceSession_) {
        return;
    }

    const int originalRows = ClampModelCount(traceSession_->totalRows());
    sparseRowRefs_.reserve(static_cast<std::size_t>(originalRows + insertedRows_.size()));
    for (int originalRow = 0; originalRow < originalRows; ++originalRow) {
        sparseRowRefs_.push_back(SourceRowRef{.originalRow = originalRow, .insertedRow = -1});
    }
}

int FlitTableModel::visibleRowForLogicalRow(const int logicalRow) const noexcept
{
    if (logicalRow < 0) {
        return -1;
    }

    if (identityVisibleRows_) {
        return logicalRow < identityVisibleRowCount_ ? logicalRow : -1;
    }

    const auto found = std::lower_bound(visibleRows_.begin(), visibleRows_.end(), logicalRow);
    if (found == visibleRows_.end() || *found != logicalRow) {
        return -1;
    }

    return static_cast<int>(std::distance(visibleRows_.begin(), found));
}

bool FlitTableModel::hasActiveSearchFilters() const noexcept
{
    return !opcodeFilter_.isEmpty()
        || !sourceIdFilter_.isEmpty()
        || !targetIdFilter_.isEmpty()
        || !txnIdFilter_.isEmpty()
        || !dbidFilter_.isEmpty()
        || !addressFilter_.isEmpty()
        || !optionalFieldFilters_.isEmpty();
}

bool FlitTableModel::hasActiveRowFilters() const noexcept
{
    return !showReq_
        || !showRsp_
        || !showDat_
        || !showSnp_
        || !showTx_
        || !showRx_
        || showXactionDeniedOnly_
        || (searchMode_ == SearchMode::Filter && hasActiveSearchFilters());
}

bool FlitTableModel::hasActiveFilters() const noexcept
{
    return hasActiveRowFilters();
}

bool FlitTableModel::canUseIdentityVisibleRows() const noexcept
{
    return !hasActiveFilters();
}

FlitTableModel::FilterCriteria FlitTableModel::currentFilterCriteria() const
{
    return FilterCriteria{
        .opcodeFilter = opcodeFilter_,
        .sourceIdFilter = sourceIdFilter_,
        .targetIdFilter = targetIdFilter_,
        .txnIdFilter = txnIdFilter_,
        .dbidFilter = dbidFilter_,
        .addressFilter = addressFilter_,
        .optionalFieldFilters = optionalFieldFilters_,
        .showReq = showReq_,
        .showRsp = showRsp_,
        .showDat = showDat_,
        .showSnp = showSnp_,
        .showTx = showTx_,
        .showRx = showRx_,
        .showXactionDeniedOnly = showXactionDeniedOnly_,
    };
}

FlitTableModel::FilterCriteria FlitTableModel::currentRowFilterCriteria() const
{
    FilterCriteria criteria = currentFilterCriteria();
    if (searchMode_ == SearchMode::Highlight) {
        criteria.opcodeFilter.clear();
        criteria.sourceIdFilter.clear();
        criteria.targetIdFilter.clear();
        criteria.txnIdFilter.clear();
        criteria.dbidFilter.clear();
        criteria.addressFilter.clear();
        criteria.optionalFieldFilters.clear();
    }
    return criteria;
}

bool FlitTableModel::optionalFieldFiltersHaveFastIndexes(const QHash<QString, QString>& filters)
{
    if (!traceSession_ || filters.isEmpty()) {
        return false;
    }

    for (auto it = filters.cbegin(); it != filters.cend(); ++it) {
        if (!traceSession_->hasOptionalFieldFastIndex(it.key())) {
            return false;
        }
    }

    return true;
}

const FlitRecord* FlitTableModel::tryRecordForLogicalRow(const int logicalRow) const noexcept
{
    if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
        return nullptr;
    }

    if (!traceSession_) {
        return &rows_[static_cast<std::size_t>(logicalRow)];
    }

    const SourceRowRef rowRef = sourceRowRefAt(logicalRow);
    if (rowRef.insertedRow >= 0) {
        return rowRef.insertedRow < static_cast<int>(insertedRows_.size())
            ? &insertedRows_[static_cast<std::size_t>(rowRef.insertedRow)]
            : nullptr;
    }

    const auto edited = editedRows_.constFind(rowRef.originalRow);
    if (edited != editedRows_.cend()) {
        return &edited.value();
    }

    return rowRef.originalRow >= 0
        ? traceSession_->tryGetRow(static_cast<std::uint64_t>(rowRef.originalRow))
        : nullptr;
}

const FlitRecord* FlitTableModel::recordForLogicalRow(const int logicalRow) const
{
    if (logicalRow < 0 || logicalRow >= sourceRowCount()) {
        return nullptr;
    }

    if (!traceSession_) {
        return &rows_[static_cast<std::size_t>(logicalRow)];
    }

    const SourceRowRef rowRef = sourceRowRefAt(logicalRow);
    if (rowRef.insertedRow >= 0) {
        return rowRef.insertedRow < static_cast<int>(insertedRows_.size())
            ? &insertedRows_[static_cast<std::size_t>(rowRef.insertedRow)]
            : nullptr;
    }

    const auto edited = editedRows_.constFind(rowRef.originalRow);
    if (edited != editedRows_.cend()) {
        return &edited.value();
    }

    if (rowRef.originalRow < 0) {
        return nullptr;
    }

    if (const FlitRecord* cachedRow = traceSession_->tryGetRow(static_cast<std::uint64_t>(rowRef.originalRow))) {
        return cachedRow;
    }

    CLogBTraceLoadError error;
    try {
        if (!traceSession_->ensureRows(static_cast<std::uint64_t>(rowRef.originalRow), 1, error)) {
            return nullptr;
        }
    } catch (const std::bad_alloc&) {
        BugReporter::failFastOutOfMemory(
            QStringLiteral("fetching a trace row from the session-backed table model"));
    } catch (const std::exception& exception) {
        const QString summary = QStringLiteral("Loading a trace row failed unexpectedly.");
        const QString detail = QString::fromLocal8Bit(exception.what());
        auto* self = const_cast<FlitTableModel*>(this);
        QMetaObject::invokeMethod(self,
                                  [self, summary, detail]() {
                                      self->emitFilteringFailure(summary, detail);
                                  },
                                  Qt::QueuedConnection);
        return nullptr;
    } catch (...) {
        const QString summary = QStringLiteral("Loading a trace row failed unexpectedly.");
        const QString detail = QStringLiteral("A non-standard exception escaped the row loader.");
        auto* self = const_cast<FlitTableModel*>(this);
        QMetaObject::invokeMethod(self,
                                  [self, summary, detail]() {
                                      self->emitFilteringFailure(summary, detail);
                                  },
                                  Qt::QueuedConnection);
        return nullptr;
    }

    return traceSession_->tryGetRow(static_cast<std::uint64_t>(rowRef.originalRow));
}

bool FlitTableModel::nodeLabelForIndex(const FlitRecord& record,
                                       const int column,
                                       QString& label,
                                       QColor& color) const
{
    label.clear();
    color = QColor();

    if (!nodeLabelsVisible_) {
        return false;
    }

    if (column == TargetColumn && record.dimTarget) {
        label = record.channelTag;
        color = NodeTypeLabelColor(label);
        return !label.isEmpty();
    }

    QString valueText;
    if (column == SourceColumn) {
        valueText = record.source;
    } else if (column == TargetColumn) {
        valueText = record.target;
    } else if (column >= ColumnCount) {
        const QString fieldName = visibleOptionalFields_.value(column - ColumnCount);
        if (!IsNodeIdFieldName(fieldName)) {
            return false;
        }
        const FieldEntry* field = FindField(record, fieldName);
        if (!field) {
            return false;
        }
        valueText = field->value;
    } else {
        return false;
    }

    qulonglong nodeId = 0;
    if (!ParseSearchInteger(valueText, nodeId)) {
        return false;
    }

    const CLogBTraceMetadata* metadata = traceMetadata();
    if (metadata) {
        const auto found = metadata->nodeTypes.find(static_cast<quint16>(nodeId));
        if (found != metadata->nodeTypes.cend()) {
            label = QString::fromStdString(CLog::NodeTypeToString(found->second));
        }
    }
    if (label.isEmpty()) {
        label = QStringLiteral("unknown");
    }
    color = NodeTypeLabelColor(label);
    return true;
}

void FlitTableModel::recountChannels()
{
    channelCounts_.fill(0);

    if (traceSession_) {
        const auto& metadataCounts = traceSession_->metadata().channelCounts;
        for (std::size_t index = 0; index < channelCounts_.size(); ++index) {
            channelCounts_[index] = ClampModelCount(metadataCounts[index]);
        }
        for (auto it = editedRows_.cbegin(); it != editedRows_.cend(); ++it) {
            const FlitRecord* original = traceSession_->tryGetRow(static_cast<std::uint64_t>(it.key()));
            if (original) {
                switch (original->channel) {
                case FlitChannel::Req: --channelCounts_[0]; break;
                case FlitChannel::Rsp: --channelCounts_[1]; break;
                case FlitChannel::Dat: --channelCounts_[2]; break;
                case FlitChannel::Snp: --channelCounts_[3]; break;
                }
            }
            switch (it.value().channel) {
            case FlitChannel::Req: ++channelCounts_[0]; break;
            case FlitChannel::Rsp: ++channelCounts_[1]; break;
            case FlitChannel::Dat: ++channelCounts_[2]; break;
            case FlitChannel::Snp: ++channelCounts_[3]; break;
            }
        }
        for (const FlitRecord& row : insertedRows_) {
            switch (row.channel) {
            case FlitChannel::Req:
                ++channelCounts_[0];
                break;
            case FlitChannel::Rsp:
                ++channelCounts_[1];
                break;
            case FlitChannel::Dat:
                ++channelCounts_[2];
                break;
            case FlitChannel::Snp:
                ++channelCounts_[3];
                break;
            }
        }
        return;
    }

    for (const FlitRecord& row : rows_) {
        switch (row.channel) {
        case FlitChannel::Req:
            ++channelCounts_[0];
            break;
        case FlitChannel::Rsp:
            ++channelCounts_[1];
            break;
        case FlitChannel::Dat:
            ++channelCounts_[2];
            break;
        case FlitChannel::Snp:
            ++channelCounts_[3];
            break;
        }
    }
}

bool FlitTableModel::channelEnabled(const FlitChannel channel) const noexcept
{
    switch (channel) {
    case FlitChannel::Req:
        return showReq_;
    case FlitChannel::Rsp:
        return showRsp_;
    case FlitChannel::Dat:
        return showDat_;
    case FlitChannel::Snp:
        return showSnp_;
    }
    return true;
}

bool FlitTableModel::channelEnabled(const FlitChannel channel,
                                    const FilterCriteria& criteria) const noexcept
{
    switch (channel) {
    case FlitChannel::Req:
        return criteria.showReq;
    case FlitChannel::Rsp:
        return criteria.showRsp;
    case FlitChannel::Dat:
        return criteria.showDat;
    case FlitChannel::Snp:
        return criteria.showSnp;
    }
    return true;
}

bool FlitTableModel::directionEnabled(const FlitDirection direction) const noexcept
{
    return direction == FlitDirection::Tx ? showTx_ : showRx_;
}

bool FlitTableModel::directionEnabled(const FlitDirection direction,
                                      const FilterCriteria& criteria) const noexcept
{
    return direction == FlitDirection::Tx ? criteria.showTx : criteria.showRx;
}

bool FlitTableModel::passesFilters(const FlitRecord& row) const
{
    return passesFilters(row, currentFilterCriteria());
}

bool FlitTableModel::passesFilters(const FlitRecord& row, const FilterCriteria& criteria) const
{
    if (criteria.showXactionDeniedOnly
        && row.xactionIndexState != XactionIndexState::Denied) {
        return false;
    }

    if (!MatchesFilter(row.opcode, criteria.opcodeFilter)
        && !MatchesFilter(row.opcodeRaw, criteria.opcodeFilter)) {
        return false;
    }

    if (!MatchesFilter(row.source, criteria.sourceIdFilter)) {
        return false;
    }

    if (!MatchesFilter(row.target, criteria.targetIdFilter)) {
        return false;
    }

    if (!MatchesFilter(row.txnId, criteria.txnIdFilter)) {
        return false;
    }

    if (!MatchesFilter(row.dbid, criteria.dbidFilter)) {
        return false;
    }

    if (!MatchesFilter(row.address, criteria.addressFilter)) {
        return false;
    }

    for (auto it = criteria.optionalFieldFilters.cbegin();
         it != criteria.optionalFieldFilters.cend();
         ++it) {
        if (!MatchesFieldFilter(row, it.key(), it.value())) {
            return false;
        }
    }

    return true;
}

void FlitTableModel::rebuildAvailableOptionalFields()
{
    QStringList rebuilt;
    QHash<QString, QStringList> rebuiltByScope;

    if (traceSession_) {
        std::vector<FlitRecord> seedRows;
        CLogBTraceLoadError error;
        const std::uint64_t seedCount = std::min<std::uint64_t>(
            traceSession_->pageSize(),
            traceSession_->totalRows());
        if (seedCount > 0
            && traceSession_->readRows(0, seedCount, seedRows, error)) {
            CollectOptionalFieldsFromRows(seedRows, rebuilt, rebuiltByScope);
        }
        CollectOptionalFieldsFromRows(sparseEditedRowsSnapshot(), rebuilt, rebuiltByScope);
    } else {
        CollectOptionalFieldsFromRows(rows_, rebuilt, rebuiltByScope);
    }

    QStringList filteredVisible;
    for (const QString& fieldName : visibleOptionalFields_) {
        if (rebuilt.contains(fieldName)) {
            filteredVisible.append(fieldName);
        }
    }

    for (auto it = optionalFieldFilters_.begin(); it != optionalFieldFilters_.end();) {
        if (!rebuilt.contains(it.key())) {
            it = optionalFieldFilters_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = optionalFieldDisplayModes_.begin(); it != optionalFieldDisplayModes_.end();) {
        if (!rebuilt.contains(it.key())) {
            it = optionalFieldDisplayModes_.erase(it);
        } else {
            ++it;
        }
    }

    availableOptionalFields_ = rebuilt;
    availableOptionalFieldsByScope_ = rebuiltByScope;
    visibleOptionalFields_ = filteredVisible;
}

void FlitTableModel::rebuildVisibleRowsNoReset()
{
    visibleRows_.clear();
    identityVisibleRows_ = false;
    identityVisibleRowCount_ = 0;

    if (canUseIdentityVisibleRows()) {
        identityVisibleRows_ = true;
        identityVisibleRowCount_ = sourceRowCount();
        return;
    }

    const FilterCriteria criteria = currentRowFilterCriteria();

    if (traceSession_ && hasSparseEdits()) {
        const int logicalLimit = sourceRowCount();
        visibleRows_.reserve(static_cast<std::size_t>(qMin(65536, qMax(0, logicalLimit))));
        for (int logicalRow = 0; logicalRow < logicalLimit; ++logicalRow) {
            const FlitRecord* row = recordForLogicalRow(logicalRow);
            if (row
                && channelEnabled(row->channel, criteria)
                && directionEnabled(row->direction, criteria)
                && passesFilters(*row, criteria)) {
                visibleRows_.push_back(logicalRow);
            }
        }
        return;
    }

    if (traceSession_) {
        const int logicalLimit = sourceRowCount();
        visibleRows_.reserve(static_cast<std::size_t>(qMin(65536, qMax(0, logicalLimit))));
        bool identityCandidate = true;
        int nextExpectedLogicalRow = 0;
        const auto materializeIdentityPrefix = [this, &identityCandidate, &nextExpectedLogicalRow]() {
            if (!identityCandidate) {
                return;
            }

            visibleRows_.reserve(visibleRows_.size() + static_cast<std::size_t>(qMax(0, nextExpectedLogicalRow)));
            for (int logicalRow = 0; logicalRow < nextExpectedLogicalRow; ++logicalRow) {
                visibleRows_.push_back(logicalRow);
            }
            identityCandidate = false;
        };
        const auto noteSkippedRange = [&identityCandidate,
                                       &nextExpectedLogicalRow,
                                       &materializeIdentityPrefix](const std::uint64_t rowStart,
                                                                   const std::uint64_t rowCount) {
            if (!identityCandidate || rowCount == 0) {
                return;
            }

            const std::uint64_t expected = static_cast<std::uint64_t>(nextExpectedLogicalRow);
            if (rowStart <= expected && expected < rowStart + rowCount) {
                materializeIdentityPrefix();
            }
        };
        const bool hasTextFilters = !criteria.opcodeFilter.isEmpty()
            || !criteria.sourceIdFilter.isEmpty()
            || !criteria.targetIdFilter.isEmpty()
            || !criteria.txnIdFilter.isEmpty()
            || !criteria.dbidFilter.isEmpty()
            || !criteria.addressFilter.isEmpty()
            || !criteria.optionalFieldFilters.isEmpty();
        const bool canUseDefaultFastFilters = CanUseFastRawFiltersForDefaultFields(criteria.opcodeFilter,
                                                                                   criteria.sourceIdFilter,
                                                                                   criteria.targetIdFilter,
                                                                                   criteria.txnIdFilter,
                                                                                   criteria.dbidFilter,
                                                                                   criteria.addressFilter);
        const bool canUseFastRawFilters = !criteria.showXactionDeniedOnly
            && criteria.optionalFieldFilters.isEmpty()
            && canUseDefaultFastFilters;
        const bool canUseOptionalFieldIndexes = !criteria.showXactionDeniedOnly
            && !criteria.optionalFieldFilters.isEmpty()
            && canUseDefaultFastFilters
            && optionalFieldFiltersHaveFastIndexes(criteria.optionalFieldFilters);
        CLogBTraceFastFilter fastFilter;
        if (canUseFastRawFilters || canUseOptionalFieldIndexes) {
            fastFilter = {};
            fastFilter.transportMask = EnabledTransportMask(criteria.showReq,
                                                            criteria.showRsp,
                                                            criteria.showDat,
                                                            criteria.showSnp,
                                                            criteria.showTx,
                                                            criteria.showRx);
            fastFilter.opcodeFilter = criteria.opcodeFilter;
            fastFilter.sourceIdFilter = criteria.sourceIdFilter;
            fastFilter.targetIdFilter = criteria.targetIdFilter;
            fastFilter.txnIdFilter = criteria.txnIdFilter;
            fastFilter.dbidFilter = criteria.dbidFilter;
            fastFilter.addressFilter = criteria.addressFilter;
            CLogBTraceLoader::prepareFastFilter(fastFilter);
        }
        const CLogBTraceChannelMask enabledMask = EnabledTransportMask(criteria.showReq,
                                                                       criteria.showRsp,
                                                                       criteria.showDat,
                                                                       criteria.showSnp,
                                                                       criteria.showTx,
                                                                       criteria.showRx);
        const auto appendLogicalRange = [this,
                                         logicalLimit,
                                         &identityCandidate,
                                         &nextExpectedLogicalRow,
                                         &materializeIdentityPrefix](const std::uint64_t rowStart,
                                                                     const std::uint64_t rowCount) {
            if (logicalLimit <= 0 || rowStart >= static_cast<std::uint64_t>(logicalLimit)) {
                return;
            }

            const int begin = static_cast<int>(rowStart);
            const std::uint64_t endRow = std::min<std::uint64_t>(
                rowStart + rowCount,
                static_cast<std::uint64_t>(logicalLimit));
            if (identityCandidate && begin == nextExpectedLogicalRow) {
                nextExpectedLogicalRow = static_cast<int>(endRow);
                return;
            }

            if (identityCandidate) {
                materializeIdentityPrefix();
            }

            for (int logicalRow = begin; logicalRow < static_cast<int>(endRow); ++logicalRow) {
                visibleRows_.push_back(logicalRow);
            }
        };
        const auto appendLogicalRows = [this,
                                        logicalLimit,
                                        &identityCandidate,
                                        &nextExpectedLogicalRow,
                                        &materializeIdentityPrefix](const std::uint64_t rowStart,
                                                                    const std::uint64_t rowCount,
                                                                    const std::vector<int>& logicalRows) {
            if (logicalLimit <= 0 || rowCount == 0 || rowStart >= static_cast<std::uint64_t>(logicalLimit)) {
                return;
            }

            const int begin = static_cast<int>(rowStart);
            const int clampedCount = static_cast<int>(std::min<std::uint64_t>(
                rowCount,
                static_cast<std::uint64_t>(logicalLimit - begin)));
            bool matchesFullRange = logicalRows.size() == static_cast<std::size_t>(clampedCount);
            if (matchesFullRange) {
                for (int index = 0; index < clampedCount; ++index) {
                    if (logicalRows[static_cast<std::size_t>(index)] != begin + index) {
                        matchesFullRange = false;
                        break;
                    }
                }
            }

            if (identityCandidate && matchesFullRange && begin == nextExpectedLogicalRow) {
                nextExpectedLogicalRow += clampedCount;
                return;
            }

            if (identityCandidate) {
                materializeIdentityPrefix();
            }

            visibleRows_.insert(visibleRows_.end(), logicalRows.begin(), logicalRows.end());
        };
        QStringList rebuiltFields;
        QHash<QString, QStringList> rebuiltFieldsByScope;
        CLogBTraceLoadError error;
        bool scanFailed = false;
        const std::uint64_t pageSize = traceSession_->pageSize();
        const std::uint64_t scanChunkRows = std::max<std::uint64_t>(
            pageSize,
            static_cast<std::uint64_t>(traceSession_->pageSize() * traceSession_->maxCachedPages()));
        for (std::size_t blockIndex = 0;
             blockIndex < traceSession_->metadata().blocks.size();
             ++blockIndex) {
            const CLogBTraceBlockSummary& block = traceSession_->metadata().blocks[blockIndex];
            if (logicalLimit <= 0 || block.rowStart >= static_cast<std::uint64_t>(logicalLimit)) {
                break;
            }

            if ((block.channelMask & enabledMask) == 0) {
                noteSkippedRange(block.rowStart, block.recordCount);
                continue;
            }

            if (!hasTextFilters
                && !criteria.showXactionDeniedOnly
                && (block.channelMask & static_cast<CLogBTraceChannelMask>(
                                         CLogBAllChannelMask() & ~enabledMask)) == 0) {
                appendLogicalRange(block.rowStart, block.recordCount);
                continue;
            }

            if (criteria.showXactionDeniedOnly && !hasTextFilters) {
                std::vector<int> deniedRows;
                if (!traceSession_->collectRowsMatchingXactionDeniedRange(block.rowStart,
                                                                          block.recordCount,
                                                                          deniedRows)) {
                    scanFailed = true;
                }
                if (scanFailed) {
                    break;
                }

                if ((block.channelMask & static_cast<CLogBTraceChannelMask>(
                                         CLogBAllChannelMask() & ~enabledMask)) == 0) {
                    appendLogicalRows(block.rowStart, block.recordCount, deniedRows);
                    continue;
                }

                std::vector<int> transportRows;
                if (!traceSession_->collectRowsForTransportMask(blockIndex,
                                                                enabledMask,
                                                                transportRows,
                                                                error)) {
                    scanFailed = true;
                }
                if (scanFailed) {
                    break;
                }

                appendLogicalRows(block.rowStart,
                                  block.recordCount,
                                  IntersectSortedLogicalRows(deniedRows, transportRows));
                continue;
            }

            if (!hasTextFilters) {
                std::vector<int> matchedRows;
                if (!traceSession_->collectRowsForTransportMask(blockIndex,
                                                                enabledMask,
                                                                matchedRows,
                                                                error)) {
                    scanFailed = true;
                }
                if (scanFailed) {
                    break;
                }
                appendLogicalRows(block.rowStart, block.recordCount, matchedRows);
                continue;
            }

            if (canUseFastRawFilters) {
                std::vector<int> matchedRows;
                if (!traceSession_->collectRowsMatchingFastFilter(blockIndex,
                                                                  fastFilter,
                                                                  matchedRows,
                                                                  error)) {
                    scanFailed = true;
                }
                if (scanFailed) {
                    break;
                }
                appendLogicalRows(block.rowStart, block.recordCount, matchedRows);
                continue;
            }

            if (canUseOptionalFieldIndexes) {
                std::vector<int> matchedRows;
                if (!traceSession_->collectRowsMatchingFastFilterAndOptionalFieldIndexesRange(
                        blockIndex,
                        fastFilter,
                        criteria.optionalFieldFilters,
                        0,
                        static_cast<std::size_t>(block.recordCount),
                        matchedRows,
                        error)) {
                    scanFailed = true;
                }
                if (scanFailed) {
                    break;
                }
                appendLogicalRows(block.rowStart, block.recordCount, matchedRows);
                continue;
            }

            const std::uint64_t blockEnd = block.rowStart + block.recordCount;
            for (std::uint64_t rowStart = block.rowStart; rowStart < blockEnd; rowStart += scanChunkRows) {
                const std::uint64_t rowCount = std::min<std::uint64_t>(scanChunkRows, blockEnd - rowStart);
                std::vector<FlitRecord> pageRows;
                if (!traceSession_->readRows(rowStart, rowCount, pageRows, error)) {
                    scanFailed = true;
                    break;
                }

                CollectOptionalFieldsFromRows(pageRows, rebuiltFields, rebuiltFieldsByScope);
                for (std::size_t localIndex = 0; localIndex < pageRows.size(); ++localIndex) {
                    const FlitRecord& row = pageRows[localIndex];
                    const int logicalRow = static_cast<int>(rowStart + localIndex);
                    if (channelEnabled(row.channel, criteria)
                        && directionEnabled(row.direction, criteria)
                        && passesFilters(row, criteria)) {
                        if (identityCandidate && logicalRow == nextExpectedLogicalRow) {
                            ++nextExpectedLogicalRow;
                        } else {
                            if (identityCandidate) {
                                materializeIdentityPrefix();
                            }
                            visibleRows_.push_back(logicalRow);
                        }
                    } else {
                        noteSkippedRange(rowStart + localIndex, 1);
                    }
                }
            }

            if (scanFailed) {
                break;
            }
        }

        if (scanFailed) {
            visibleRows_.clear();
            identityVisibleRows_ = true;
            identityVisibleRowCount_ = sourceRowCount();
            return;
        }

        if (identityCandidate && nextExpectedLogicalRow >= logicalLimit) {
            visibleRows_.clear();
            identityVisibleRows_ = true;
            identityVisibleRowCount_ = logicalLimit;
            return;
        }

        if (!rebuiltFields.isEmpty() || !rebuiltFieldsByScope.isEmpty()) {
            availableOptionalFields_ = rebuiltFields;
            availableOptionalFieldsByScope_ = rebuiltFieldsByScope;
            QStringList filteredVisible;
            for (const QString& fieldName : visibleOptionalFields_) {
                if (availableOptionalFields_.contains(fieldName)) {
                    filteredVisible.append(fieldName);
                }
            }
            visibleOptionalFields_ = filteredVisible;
        }
        return;
    }

    visibleRows_.reserve(rows_.size());
    for (int index = 0; index < static_cast<int>(rows_.size()); ++index) {
        const FlitRecord& row = rows_[static_cast<std::size_t>(index)];
        if (channelEnabled(row.channel, criteria)
            && directionEnabled(row.direction, criteria)
            && passesFilters(row, criteria)) {
            visibleRows_.push_back(index);
        }
    }
}

void FlitTableModel::cancelAsyncFilterBuild(const bool notifyProgress)
{
    ++filterBuildGeneration_;
    asyncFilterBuild_.reset();
    if (notifyProgress && filteringActive_) {
        filteringActive_ = false;
        Q_EMIT filteringProgressChanged(false, QString(), 0, 0);
    }
}

void FlitTableModel::startAsyncFilterBuild()
{
    if (!traceSession_) {
        rebuildVisibleRows();
        return;
    }

    try {
        auto state = std::make_unique<AsyncFilterBuildState>();
        state->generation = filterBuildGeneration_;
        state->criteria = currentRowFilterCriteria();
        state->enabledMask = EnabledTransportMask(state->criteria.showReq,
                                                  state->criteria.showRsp,
                                                  state->criteria.showDat,
                                                  state->criteria.showSnp,
                                                  state->criteria.showTx,
                                                  state->criteria.showRx);
        state->hasTextFilters = !state->criteria.opcodeFilter.isEmpty()
            || !state->criteria.sourceIdFilter.isEmpty()
            || !state->criteria.targetIdFilter.isEmpty()
            || !state->criteria.txnIdFilter.isEmpty()
            || !state->criteria.dbidFilter.isEmpty()
            || !state->criteria.addressFilter.isEmpty()
            || !state->criteria.optionalFieldFilters.isEmpty();
        const bool canUseDefaultFastFilters = CanUseFastRawFiltersForDefaultFields(
            state->criteria.opcodeFilter,
            state->criteria.sourceIdFilter,
            state->criteria.targetIdFilter,
            state->criteria.txnIdFilter,
            state->criteria.dbidFilter,
            state->criteria.addressFilter);
        state->canUseFastRawFilters =
            !state->criteria.showXactionDeniedOnly
            && state->criteria.optionalFieldFilters.isEmpty()
            && canUseDefaultFastFilters;
        state->canUseOptionalFieldIndexes =
            !state->criteria.showXactionDeniedOnly
            && !state->criteria.optionalFieldFilters.isEmpty()
            && canUseDefaultFastFilters
            && optionalFieldFiltersHaveFastIndexes(state->criteria.optionalFieldFilters);
        if (state->canUseFastRawFilters || state->canUseOptionalFieldIndexes) {
            state->fastFilter = {};
            state->fastFilter.transportMask = state->enabledMask;
            state->fastFilter.opcodeFilter = state->criteria.opcodeFilter;
            state->fastFilter.sourceIdFilter = state->criteria.sourceIdFilter;
            state->fastFilter.targetIdFilter = state->criteria.targetIdFilter;
            state->fastFilter.txnIdFilter = state->criteria.txnIdFilter;
            state->fastFilter.dbidFilter = state->criteria.dbidFilter;
            state->fastFilter.addressFilter = state->criteria.addressFilter;
            CLogBTraceLoader::prepareFastFilter(state->fastFilter);
        }

        state->logicalLimit = sourceRowCount();
        state->totalRows = std::min<std::uint64_t>(
            traceSession_->totalRows(),
            state->logicalLimit > 0
                ? static_cast<std::uint64_t>(state->logicalLimit)
                : 0U);
        state->scanChunkRows = traceSession_->pageSize();
        state->fastScanChunkRecords = std::max<std::size_t>(
            65536U,
            traceSession_->pageSize() * 64U);
        state->visibleRows.reserve(static_cast<std::size_t>(qMin(65536, qMax(0, state->logicalLimit))));
        asyncFilterBuild_ = std::move(state);
    } catch (const std::bad_alloc&) {
        BugReporter::failFastOutOfMemory(
            QStringLiteral("initializing the asynchronous trace filter state"));
    } catch (const std::exception& exception) {
        emitFilteringFailure(QStringLiteral("Starting trace filtering failed unexpectedly."),
                             QString::fromLocal8Bit(exception.what()));
        return;
    } catch (...) {
        emitFilteringFailure(QStringLiteral("Starting trace filtering failed unexpectedly."),
                             QStringLiteral("A non-standard exception escaped while preparing the filter build."));
        return;
    }

    emitFilteringProgress(QStringLiteral("Filtering trace rows..."), 0, 1000);
    const quint64 generation = filterBuildGeneration_;
    QTimer::singleShot(0, this, [this, generation]() {
        processAsyncFilterBuild(generation);
    });
}

void FlitTableModel::processAsyncFilterBuild(const quint64 generation)
{
    if (!asyncFilterBuild_ || generation != filterBuildGeneration_ || !traceSession_) {
        return;
    }

    try {
        AsyncFilterBuildState& state = *asyncFilterBuild_;
        const CLogBTraceMetadata& metadata = traceSession_->metadata();
        const auto materializeIdentityPrefix = [this, &state]() {
            if (!state.identityCandidate) {
                return;
            }

            state.visibleRows.reserve(
                state.visibleRows.size() + static_cast<std::size_t>(qMax(0, state.nextExpectedLogicalRow)));
            for (int logicalRow = 0; logicalRow < state.nextExpectedLogicalRow; ++logicalRow) {
                state.visibleRows.push_back(logicalRow);
            }
            state.identityCandidate = false;
        };
        const auto noteSkippedRange = [&state,
                                       &materializeIdentityPrefix](const std::uint64_t rowStart,
                                                                   const std::uint64_t rowCount) {
            if (!state.identityCandidate || rowCount == 0) {
                return;
            }

            const std::uint64_t expected = static_cast<std::uint64_t>(state.nextExpectedLogicalRow);
            if (rowStart <= expected && expected < rowStart + rowCount) {
                materializeIdentityPrefix();
            }
        };
        const auto appendLogicalRange = [this, &state, &materializeIdentityPrefix](const std::uint64_t rowStart,
                                                                                    const std::uint64_t rowCount) {
            if (state.logicalLimit <= 0 || rowStart >= static_cast<std::uint64_t>(state.logicalLimit)) {
                return;
            }

            const int begin = static_cast<int>(rowStart);
            const std::uint64_t endRow = std::min<std::uint64_t>(
                rowStart + rowCount,
                static_cast<std::uint64_t>(state.logicalLimit));
            if (state.identityCandidate && begin == state.nextExpectedLogicalRow) {
                state.nextExpectedLogicalRow = static_cast<int>(endRow);
                return;
            }

            if (state.identityCandidate) {
                materializeIdentityPrefix();
            }

            for (int logicalRow = begin; logicalRow < static_cast<int>(endRow); ++logicalRow) {
                state.visibleRows.push_back(logicalRow);
            }
        };
        const auto appendLogicalRows = [this,
                                        &state,
                                        &materializeIdentityPrefix](const std::uint64_t rowStart,
                                                                    const std::uint64_t rowCount,
                                                                    const std::vector<int>& logicalRows) {
            if (state.logicalLimit <= 0 || rowCount == 0 || rowStart >= static_cast<std::uint64_t>(state.logicalLimit)) {
                return;
            }

            const int begin = static_cast<int>(rowStart);
            const int clampedCount = static_cast<int>(std::min<std::uint64_t>(
                rowCount,
                static_cast<std::uint64_t>(state.logicalLimit - begin)));
            bool matchesFullRange = logicalRows.size() == static_cast<std::size_t>(clampedCount);
            if (matchesFullRange) {
                for (int index = 0; index < clampedCount; ++index) {
                    if (logicalRows[static_cast<std::size_t>(index)] != begin + index) {
                        matchesFullRange = false;
                        break;
                    }
                }
            }

            if (state.identityCandidate && matchesFullRange && begin == state.nextExpectedLogicalRow) {
                state.nextExpectedLogicalRow += clampedCount;
                return;
            }

            if (state.identityCandidate) {
                materializeIdentityPrefix();
            }

            state.visibleRows.insert(state.visibleRows.end(), logicalRows.begin(), logicalRows.end());
        };
        const auto updateProgress = [this, &state]() {
            const int maximum = 1000;
            const int value = state.totalRows == 0
                ? maximum
                : static_cast<int>(std::min<std::uint64_t>(
                    maximum,
                    (state.processedRows * static_cast<std::uint64_t>(maximum)) / state.totalRows));
            const QString text = QStringLiteral("Filtering rows %1 / %2...")
                .arg(static_cast<qulonglong>(state.processedRows))
                .arg(static_cast<qulonglong>(state.totalRows));
            emitFilteringProgress(text, value, maximum);
        };

        QElapsedTimer budget;
        budget.start();
        constexpr qint64 kFilterBudgetMs = 8;

        while (state.nextBlockIndex < metadata.blocks.size()) {
            const CLogBTraceBlockSummary& block = metadata.blocks[state.nextBlockIndex];
            const std::uint64_t blockEnd = block.rowStart + block.recordCount;
            if (state.logicalLimit <= 0 || block.rowStart >= static_cast<std::uint64_t>(state.logicalLimit)) {
                state.processedRows = state.totalRows;
                break;
            }

            if (state.nextRowStart == 0) {
                state.nextRowStart = block.rowStart;
            }

            if ((block.channelMask & state.enabledMask) == 0) {
                noteSkippedRange(block.rowStart, block.recordCount);
                state.processedRows = std::min<std::uint64_t>(state.totalRows, blockEnd);
                ++state.nextBlockIndex;
                state.nextRowStart = 0;
                updateProgress();
                if (budget.elapsed() >= kFilterBudgetMs) {
                    break;
                }
                continue;
            }

            if (!state.hasTextFilters
                && !state.criteria.showXactionDeniedOnly
                && (block.channelMask & static_cast<CLogBTraceChannelMask>(
                                         CLogBAllChannelMask() & ~state.enabledMask)) == 0) {
                appendLogicalRange(block.rowStart, block.recordCount);
                state.processedRows = std::min<std::uint64_t>(state.totalRows, blockEnd);
                ++state.nextBlockIndex;
                state.nextLocalIndex = 0;
                state.nextRowStart = 0;
                updateProgress();
                if (budget.elapsed() >= kFilterBudgetMs) {
                    break;
                }
                continue;
            }

            if (state.criteria.showXactionDeniedOnly && !state.hasTextFilters) {
                const std::size_t localBegin = state.nextLocalIndex;
                if (localBegin >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                    continue;
                }

                const std::size_t localCount = std::min<std::size_t>(
                    state.fastScanChunkRecords,
                    static_cast<std::size_t>(block.recordCount) - localBegin);
                const std::uint64_t chunkRowStart = block.rowStart + localBegin;
                std::vector<int> deniedRows;
                if (!traceSession_->collectRowsMatchingXactionDeniedRange(chunkRowStart,
                                                                          localCount,
                                                                          deniedRows)) {
                    state.scanFailed = true;
                    break;
                }

                if ((block.channelMask & static_cast<CLogBTraceChannelMask>(
                                         CLogBAllChannelMask() & ~state.enabledMask)) == 0) {
                    appendLogicalRows(chunkRowStart, localCount, deniedRows);
                } else {
                    std::vector<FlitRecord> pageRows;
                    if (!traceSession_->readRows(chunkRowStart,
                                                 localCount,
                                                 pageRows,
                                                 state.error)) {
                        state.scanFailed = true;
                        break;
                    }

                    std::vector<int> matchedRows;
                    CollectMatchingLogicalRows(chunkRowStart,
                                               pageRows,
                                               [this, &state](const FlitRecord& row) {
                                                   return channelEnabled(row.channel, state.criteria)
                                                       && directionEnabled(row.direction, state.criteria);
                                               },
                                               matchedRows);
                    appendLogicalRows(chunkRowStart,
                                      localCount,
                                      IntersectSortedLogicalRows(deniedRows, matchedRows));
                }

                state.nextLocalIndex += localCount;
                state.processedRows = std::min<std::uint64_t>(
                    state.totalRows,
                    block.rowStart + state.nextLocalIndex);
                if (state.nextLocalIndex >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                }
                updateProgress();
                if (budget.elapsed() >= kFilterBudgetMs) {
                    break;
                }
                continue;
            }

            if (state.canUseFastRawFilters) {
                const std::size_t localBegin = state.nextLocalIndex;
                const std::size_t localCount = std::min<std::size_t>(
                    state.fastScanChunkRecords,
                    static_cast<std::size_t>(block.recordCount) - localBegin);
                std::vector<int> matchedRows;
                if (!traceSession_->collectRowsMatchingFastFilterRange(state.nextBlockIndex,
                                                                       state.fastFilter,
                                                                       localBegin,
                                                                       localCount,
                                                                       matchedRows,
                                                                       state.error)) {
                    state.scanFailed = true;
                    break;
                }
                appendLogicalRows(block.rowStart + localBegin, localCount, matchedRows);

                state.nextLocalIndex += localCount;
                state.processedRows = std::min<std::uint64_t>(
                    state.totalRows,
                    block.rowStart + state.nextLocalIndex);
                if (state.nextLocalIndex >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                }
                updateProgress();
                if (budget.elapsed() >= kFilterBudgetMs) {
                    break;
                }
                continue;
            }

            if (state.canUseOptionalFieldIndexes) {
                const std::size_t localBegin = state.nextLocalIndex;
                const std::size_t localCount = std::min<std::size_t>(
                    state.fastScanChunkRecords,
                    static_cast<std::size_t>(block.recordCount) - localBegin);
                std::vector<int> matchedRows;
                if (!traceSession_->collectRowsMatchingFastFilterAndOptionalFieldIndexesRange(
                        state.nextBlockIndex,
                        state.fastFilter,
                        state.criteria.optionalFieldFilters,
                        localBegin,
                        localCount,
                        matchedRows,
                        state.error)) {
                    state.scanFailed = true;
                    break;
                }
                appendLogicalRows(block.rowStart + localBegin, localCount, matchedRows);

                state.nextLocalIndex += localCount;
                state.processedRows = std::min<std::uint64_t>(
                    state.totalRows,
                    block.rowStart + state.nextLocalIndex);
                if (state.nextLocalIndex >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                }
                updateProgress();
                if (budget.elapsed() >= kFilterBudgetMs) {
                    break;
                }
                continue;
            }

            const std::uint64_t rowStart = state.nextRowStart;
            const std::uint64_t rowCount = std::min<std::uint64_t>(state.scanChunkRows, blockEnd - rowStart);
            std::vector<FlitRecord> pageRows;
            if (!traceSession_->readRows(rowStart, rowCount, pageRows, state.error)) {
                state.scanFailed = true;
                break;
            }

            if (state.hasTextFilters) {
                CollectOptionalFieldsFromRows(pageRows, state.rebuiltFields, state.rebuiltFieldsByScope);
            }
            std::vector<int> matchedRows;
            CollectMatchingLogicalRows(rowStart,
                                       pageRows,
                                       [this, &state](const FlitRecord& row) {
                                           return channelEnabled(row.channel, state.criteria)
                                               && directionEnabled(row.direction, state.criteria)
                                               && ((!state.hasTextFilters
                                                    && !state.criteria.showXactionDeniedOnly)
                                                   || passesFilters(row, state.criteria));
                                       },
                                       matchedRows);
            appendLogicalRows(rowStart, rowCount, matchedRows);

            state.nextRowStart += rowCount;
            state.processedRows = std::min<std::uint64_t>(state.totalRows, state.nextRowStart);
            if (state.nextRowStart >= blockEnd) {
                ++state.nextBlockIndex;
                state.nextRowStart = 0;
            }
            updateProgress();
            if (budget.elapsed() >= kFilterBudgetMs) {
                break;
            }
        }

        if (generation != filterBuildGeneration_) {
            return;
        }

        if (state.scanFailed || state.nextBlockIndex >= metadata.blocks.size()) {
            applyAsyncFilterBuild(generation);
            return;
        }

        QTimer::singleShot(0, this, [this, generation]() {
            processAsyncFilterBuild(generation);
        });
    } catch (const std::bad_alloc&) {
        cancelAsyncFilterBuild();
        BugReporter::failFastOutOfMemory(
            QStringLiteral("building the asynchronous filtered row set"));
    } catch (const std::exception& exception) {
        cancelAsyncFilterBuild();
        emitFilteringFailure(QStringLiteral("Filtering trace rows failed unexpectedly."),
                             QString::fromLocal8Bit(exception.what()));
    } catch (...) {
        cancelAsyncFilterBuild();
        emitFilteringFailure(QStringLiteral("Filtering trace rows failed unexpectedly."),
                             QStringLiteral("A non-standard exception escaped the asynchronous filter builder."));
    }
}

void FlitTableModel::applyAsyncFilterBuild(const quint64 generation)
{
    if (!asyncFilterBuild_ || generation != filterBuildGeneration_) {
        return;
    }

    std::unique_ptr<AsyncFilterBuildState> state = std::move(asyncFilterBuild_);

    beginResetModel();
    visibleRows_ = std::move(state->visibleRows);
    identityVisibleRows_ = false;
    identityVisibleRowCount_ = 0;

    if (state->scanFailed) {
        visibleRows_.clear();
        identityVisibleRows_ = true;
        identityVisibleRowCount_ = sourceRowCount();
    } else if (state->identityCandidate && state->nextExpectedLogicalRow >= state->logicalLimit) {
        visibleRows_.clear();
        identityVisibleRows_ = true;
        identityVisibleRowCount_ = state->logicalLimit;
    } else if (!state->rebuiltFields.isEmpty() || !state->rebuiltFieldsByScope.isEmpty()) {
        availableOptionalFields_ = std::move(state->rebuiltFields);
        availableOptionalFieldsByScope_ = std::move(state->rebuiltFieldsByScope);
        QStringList filteredVisible;
        for (const QString& fieldName : visibleOptionalFields_) {
            if (availableOptionalFields_.contains(fieldName)) {
                filteredVisible.append(fieldName);
            }
        }
        visibleOptionalFields_ = filteredVisible;
    }
    endResetModel();

    if (filteringActive_) {
        filteringActive_ = false;
        Q_EMIT filteringProgressChanged(false, QString(), 0, 0);
    }

    if (searchMode_ == SearchMode::Highlight) {
        rebuildSearchHighlightIndex();
    } else {
        clearSearchHighlightIndex();
    }
}

void FlitTableModel::clearSearchHighlightIndex(const bool notify)
{
    ++highlightBuildGeneration_;
    highlightSearchIndexBuild_.reset();
    highlightedRows_.clear();
    highlightIndexComplete_ = true;
    if (notify) {
        Q_EMIT searchHighlightNavigationChanged();
    }
}

void FlitTableModel::clearTransactionHighlightState() noexcept
{
    transactionHighlightKeys_.clear();
    transactionHighlightAnchorLogicalRow_ = -1;
}

bool FlitTableModel::recordMatchesTransactionHighlight(const FlitRecord& record,
                                                       const int logicalRow) const
{
    if (transactionHighlightAnchorLogicalRow_ < 0 && transactionHighlightKeys_.isEmpty()) {
        return false;
    }

    if (logicalRow >= 0 && logicalRow == transactionHighlightAnchorLogicalRow_) {
        return true;
    }

    for (const QString& key : TransactionGroupKeys(record)) {
        if (transactionHighlightKeys_.contains(key)) {
            return true;
        }
    }

    return false;
}

void FlitTableModel::refreshTransactionHighlights()
{
    const int rows = rowCount();
    const int columns = columnCount();
    if (rows <= 0 || columns <= 0) {
        return;
    }

    Q_EMIT dataChanged(index(0, 0),
                       index(rows - 1, columns - 1),
                       {Qt::BackgroundRole, Qt::ForegroundRole, TransactionHighlightRole});
}

void FlitTableModel::rebuildSearchHighlightIndex()
{
    const quint64 generation = ++highlightBuildGeneration_;
    highlightSearchIndexBuild_.reset();
    highlightedRows_.clear();
    highlightIndexComplete_ = false;

    if (searchMode_ != SearchMode::Highlight
        || !hasActiveSearchFilters()
        || visibleRowCountInternal() <= 0) {
        highlightIndexComplete_ = true;
        Q_EMIT searchHighlightNavigationChanged();
        return;
    }

    if (!traceSession_) {
        const int rows = visibleRowCountInternal();
        highlightedRows_.reserve(static_cast<std::size_t>(rows));
        for (int visibleRow = 0; visibleRow < rows; ++visibleRow) {
            const FlitRecord* record = recordAt(visibleRow);
            if (record && passesFilters(*record)) {
                highlightedRows_.push_back(visibleRow);
            }
        }
        highlightIndexComplete_ = true;
        Q_EMIT searchHighlightNavigationChanged();
        return;
    }

    try {
        auto state = std::make_unique<HighlightSearchIndexBuildState>();
        state->generation = generation;
        state->criteria = currentFilterCriteria();
        state->enabledMask = EnabledTransportMask(state->criteria.showReq,
                                                  state->criteria.showRsp,
                                                  state->criteria.showDat,
                                                  state->criteria.showSnp,
                                                  state->criteria.showTx,
                                                  state->criteria.showRx);
        state->hasTextFilters = !state->criteria.opcodeFilter.isEmpty()
            || !state->criteria.sourceIdFilter.isEmpty()
            || !state->criteria.targetIdFilter.isEmpty()
            || !state->criteria.txnIdFilter.isEmpty()
            || !state->criteria.dbidFilter.isEmpty()
            || !state->criteria.addressFilter.isEmpty()
            || !state->criteria.optionalFieldFilters.isEmpty();
        const bool canUseDefaultFastFilters = CanUseFastRawFiltersForDefaultFields(
            state->criteria.opcodeFilter,
            state->criteria.sourceIdFilter,
            state->criteria.targetIdFilter,
            state->criteria.txnIdFilter,
            state->criteria.dbidFilter,
            state->criteria.addressFilter);
        state->canUseFastRawFilters =
            state->criteria.optionalFieldFilters.isEmpty() && canUseDefaultFastFilters;
        state->canUseOptionalFieldIndexes =
            !state->criteria.optionalFieldFilters.isEmpty()
            && canUseDefaultFastFilters
            && optionalFieldFiltersHaveFastIndexes(state->criteria.optionalFieldFilters);
        if (state->canUseFastRawFilters || state->canUseOptionalFieldIndexes) {
            state->fastFilter = {};
            state->fastFilter.transportMask = state->enabledMask;
            state->fastFilter.opcodeFilter = state->criteria.opcodeFilter;
            state->fastFilter.sourceIdFilter = state->criteria.sourceIdFilter;
            state->fastFilter.targetIdFilter = state->criteria.targetIdFilter;
            state->fastFilter.txnIdFilter = state->criteria.txnIdFilter;
            state->fastFilter.dbidFilter = state->criteria.dbidFilter;
            state->fastFilter.addressFilter = state->criteria.addressFilter;
            CLogBTraceLoader::prepareFastFilter(state->fastFilter);
        }

        state->logicalLimit = sourceRowCount();
        state->totalRows = std::min<std::uint64_t>(
            traceSession_->totalRows(),
            state->logicalLimit > 0
                ? static_cast<std::uint64_t>(state->logicalLimit)
                : 0U);
        state->scanChunkRows = traceSession_->pageSize();
        state->fastScanChunkRecords = std::max<std::size_t>(
            65536U,
            traceSession_->pageSize() * 64U);
        highlightSearchIndexBuild_ = std::move(state);
    } catch (const std::bad_alloc&) {
        BugReporter::failFastOutOfMemory(
            QStringLiteral("initializing the highlighted row navigation index"));
    } catch (const std::exception& exception) {
        clearSearchHighlightIndex(false);
        emitFilteringFailure(QStringLiteral("Starting search highlight indexing failed unexpectedly."),
                             QString::fromLocal8Bit(exception.what()));
        Q_EMIT searchHighlightNavigationChanged();
        return;
    } catch (...) {
        clearSearchHighlightIndex(false);
        emitFilteringFailure(QStringLiteral("Starting search highlight indexing failed unexpectedly."),
                             QStringLiteral("A non-standard exception escaped while preparing the highlighted row index."));
        Q_EMIT searchHighlightNavigationChanged();
        return;
    }

    Q_EMIT searchHighlightNavigationChanged();
    QTimer::singleShot(0, this, [this, generation]() {
        processSearchHighlightIndexBuild(generation);
    });
}

void FlitTableModel::appendHighlightedLogicalRows(const std::vector<int>& logicalRows)
{
    highlightedRows_.reserve(highlightedRows_.size() + logicalRows.size());
    for (const int logicalRow : logicalRows) {
        const int visibleRow = visibleRowForLogicalRow(logicalRow);
        if (visibleRow < 0) {
            continue;
        }
        if (highlightedRows_.empty() || highlightedRows_.back() != visibleRow) {
            highlightedRows_.push_back(visibleRow);
        }
    }
}

void FlitTableModel::appendHighlightedLogicalRange(const std::uint64_t rowStart,
                                                   const std::uint64_t rowCount)
{
    const std::uint64_t rowEnd = std::min<std::uint64_t>(
        rowStart + rowCount,
        static_cast<std::uint64_t>(std::numeric_limits<int>::max()));
    for (std::uint64_t logicalRow = rowStart; logicalRow < rowEnd; ++logicalRow) {
        const int visibleRow = visibleRowForLogicalRow(static_cast<int>(logicalRow));
        if (visibleRow < 0) {
            continue;
        }
        if (highlightedRows_.empty() || highlightedRows_.back() != visibleRow) {
            highlightedRows_.push_back(visibleRow);
        }
    }
}

void FlitTableModel::processSearchHighlightIndexBuild(const quint64 generation)
{
    if (!highlightSearchIndexBuild_ || generation != highlightBuildGeneration_ || !traceSession_) {
        return;
    }

    try {
        HighlightSearchIndexBuildState& state = *highlightSearchIndexBuild_;
        const CLogBTraceMetadata& metadata = traceSession_->metadata();

        QElapsedTimer budget;
        budget.start();
        constexpr qint64 kHighlightIndexBudgetMs = 8;

        while (state.nextBlockIndex < metadata.blocks.size()) {
            const CLogBTraceBlockSummary& block = metadata.blocks[state.nextBlockIndex];
            const std::uint64_t blockEnd = block.rowStart + block.recordCount;
            if (state.logicalLimit <= 0 || block.rowStart >= static_cast<std::uint64_t>(state.logicalLimit)) {
                state.processedRows = state.totalRows;
                break;
            }

            if (state.nextRowStart == 0) {
                state.nextRowStart = block.rowStart;
            }

            if ((block.channelMask & state.enabledMask) == 0) {
                state.processedRows = std::min<std::uint64_t>(state.totalRows, blockEnd);
                ++state.nextBlockIndex;
                state.nextLocalIndex = 0;
                state.nextRowStart = 0;
                if (budget.elapsed() >= kHighlightIndexBudgetMs) {
                    break;
                }
                continue;
            }

            if (!state.hasTextFilters
                && (block.channelMask & static_cast<CLogBTraceChannelMask>(
                                         CLogBAllChannelMask() & ~state.enabledMask)) == 0) {
                appendHighlightedLogicalRange(block.rowStart, block.recordCount);
                state.processedRows = std::min<std::uint64_t>(state.totalRows, blockEnd);
                ++state.nextBlockIndex;
                state.nextLocalIndex = 0;
                state.nextRowStart = 0;
                if (budget.elapsed() >= kHighlightIndexBudgetMs) {
                    break;
                }
                continue;
            }

            if (state.canUseFastRawFilters) {
                const std::size_t localBegin = state.nextLocalIndex;
                if (localBegin >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                    continue;
                }

                const std::size_t localCount = std::min<std::size_t>(
                    state.fastScanChunkRecords,
                    static_cast<std::size_t>(block.recordCount) - localBegin);
                std::vector<int> matchedRows;
                if (!traceSession_->collectRowsMatchingFastFilterRange(state.nextBlockIndex,
                                                                       state.fastFilter,
                                                                       localBegin,
                                                                       localCount,
                                                                       matchedRows,
                                                                       state.error)) {
                    state.scanFailed = true;
                    break;
                }
                appendHighlightedLogicalRows(matchedRows);

                state.nextLocalIndex += localCount;
                state.processedRows = std::min<std::uint64_t>(
                    state.totalRows,
                    block.rowStart + state.nextLocalIndex);
                if (state.nextLocalIndex >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                }
                if (budget.elapsed() >= kHighlightIndexBudgetMs) {
                    break;
                }
                continue;
            }

            if (state.canUseOptionalFieldIndexes) {
                const std::size_t localBegin = state.nextLocalIndex;
                if (localBegin >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                    continue;
                }

                const std::size_t localCount = std::min<std::size_t>(
                    state.fastScanChunkRecords,
                    static_cast<std::size_t>(block.recordCount) - localBegin);
                std::vector<int> matchedRows;
                if (!traceSession_->collectRowsMatchingFastFilterAndOptionalFieldIndexesRange(
                        state.nextBlockIndex,
                        state.fastFilter,
                        state.criteria.optionalFieldFilters,
                        localBegin,
                        localCount,
                        matchedRows,
                        state.error)) {
                    state.scanFailed = true;
                    break;
                }
                appendHighlightedLogicalRows(matchedRows);

                state.nextLocalIndex += localCount;
                state.processedRows = std::min<std::uint64_t>(
                    state.totalRows,
                    block.rowStart + state.nextLocalIndex);
                if (state.nextLocalIndex >= static_cast<std::size_t>(block.recordCount)) {
                    ++state.nextBlockIndex;
                    state.nextLocalIndex = 0;
                    state.nextRowStart = 0;
                }
                if (budget.elapsed() >= kHighlightIndexBudgetMs) {
                    break;
                }
                continue;
            }

            const std::uint64_t rowStart = state.nextRowStart;
            const std::uint64_t logicalEnd = std::min<std::uint64_t>(
                blockEnd,
                static_cast<std::uint64_t>(state.logicalLimit));
            if (rowStart >= logicalEnd) {
                ++state.nextBlockIndex;
                state.nextLocalIndex = 0;
                state.nextRowStart = 0;
                continue;
            }

            const std::uint64_t rowCount = std::min<std::uint64_t>(
                state.scanChunkRows,
                logicalEnd - rowStart);
            std::vector<FlitRecord> pageRows;
            if (!traceSession_->readRows(rowStart, rowCount, pageRows, state.error)) {
                state.scanFailed = true;
                break;
            }

            for (std::size_t localIndex = 0; localIndex < pageRows.size(); ++localIndex) {
                const FlitRecord& row = pageRows[localIndex];
                if (!channelEnabled(row.channel, state.criteria)
                    || !directionEnabled(row.direction, state.criteria)
                    || !passesFilters(row, state.criteria)) {
                    continue;
                }

                const std::uint64_t logicalRow = rowStart + localIndex;
                if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
                    break;
                }
                const int visibleRow = visibleRowForLogicalRow(static_cast<int>(logicalRow));
                if (visibleRow >= 0
                    && (highlightedRows_.empty() || highlightedRows_.back() != visibleRow)) {
                    highlightedRows_.push_back(visibleRow);
                }
            }

            state.nextRowStart += rowCount;
            state.processedRows = std::min<std::uint64_t>(state.totalRows, state.nextRowStart);
            if (state.nextRowStart >= blockEnd) {
                ++state.nextBlockIndex;
                state.nextRowStart = 0;
            }
            if (budget.elapsed() >= kHighlightIndexBudgetMs) {
                break;
            }
        }

        if (generation != highlightBuildGeneration_) {
            return;
        }

        if (state.scanFailed) {
            const QString summary = state.error.summary.isEmpty()
                ? QStringLiteral("Search highlight indexing failed.")
                : state.error.summary;
            const QString detail = state.error.detailedText.isEmpty()
                ? state.error.informativeText
                : state.error.detailedText;
            highlightSearchIndexBuild_.reset();
            highlightedRows_.clear();
            highlightIndexComplete_ = true;
            Q_EMIT searchHighlightNavigationChanged();
            emitFilteringFailure(summary, detail);
            return;
        }

        if (state.nextBlockIndex >= metadata.blocks.size()
            || state.processedRows >= state.totalRows) {
            highlightSearchIndexBuild_.reset();
            highlightIndexComplete_ = true;
            Q_EMIT searchHighlightNavigationChanged();
            return;
        }

        Q_EMIT searchHighlightNavigationChanged();
        QTimer::singleShot(0, this, [this, generation]() {
            processSearchHighlightIndexBuild(generation);
        });
    } catch (const std::bad_alloc&) {
        clearSearchHighlightIndex(false);
        BugReporter::failFastOutOfMemory(
            QStringLiteral("building the highlighted row navigation index"));
    } catch (const std::exception& exception) {
        clearSearchHighlightIndex(false);
        emitFilteringFailure(QStringLiteral("Search highlight indexing failed unexpectedly."),
                             QString::fromLocal8Bit(exception.what()));
        Q_EMIT searchHighlightNavigationChanged();
    } catch (...) {
        clearSearchHighlightIndex(false);
        emitFilteringFailure(QStringLiteral("Search highlight indexing failed unexpectedly."),
                             QStringLiteral("A non-standard exception escaped the highlighted row index builder."));
        Q_EMIT searchHighlightNavigationChanged();
    }
}

void FlitTableModel::emitFilteringProgress(const QString& text,
                                           const int value,
                                           const int maximum)
{
    filteringActive_ = true;
    Q_EMIT filteringProgressChanged(true, text, value, maximum);
}

void FlitTableModel::emitFilteringFailure(const QString& summary,
                                          const QString& detail)
{
    if (filteringActive_) {
        filteringActive_ = false;
        Q_EMIT filteringProgressChanged(false, QString(), 0, 0);
    }
    Q_EMIT filteringFailed(summary, detail);
}

void FlitTableModel::applySearchFilterChange()
{
    if (searchMode_ == SearchMode::Highlight) {
        rebuildSearchHighlightIndex();
        refreshSearchHighlights();
        return;
    }

    rebuildVisibleRows();
}

void FlitTableModel::refreshSearchHighlights()
{
    const int rows = rowCount();
    const int columns = columnCount();
    if (rows <= 0 || columns <= 0) {
        return;
    }

    Q_EMIT dataChanged(index(0, 0), index(rows - 1, columns - 1), {Qt::BackgroundRole});
}

void FlitTableModel::rebuildVisibleRows()
{
    if (traceSession_ && hasActiveFilters()) {
        ++filterBuildGeneration_;
        clearSearchHighlightIndex();
        startAsyncFilterBuild();
        return;
    }

    cancelAsyncFilterBuild();
    clearSearchHighlightIndex(false);
    beginResetModel();
    try {
        rebuildVisibleRowsNoReset();
    } catch (const std::bad_alloc&) {
        endResetModel();
        BugReporter::failFastOutOfMemory(
            QStringLiteral("building the synchronous filtered row set"));
    } catch (const std::exception& exception) {
        visibleRows_.clear();
        identityVisibleRows_ = true;
        identityVisibleRowCount_ = sourceRowCount();
        endResetModel();
        emitFilteringFailure(QStringLiteral("Filtering trace rows failed unexpectedly."),
                             QString::fromLocal8Bit(exception.what()));
        return;
    } catch (...) {
        visibleRows_.clear();
        identityVisibleRows_ = true;
        identityVisibleRowCount_ = sourceRowCount();
        endResetModel();
        emitFilteringFailure(QStringLiteral("Filtering trace rows failed unexpectedly."),
                             QStringLiteral("A non-standard exception escaped the filter engine. Showing the full trace instead."));
        return;
    }
    endResetModel();
    if (searchMode_ == SearchMode::Highlight) {
        rebuildSearchHighlightIndex();
    } else {
        clearSearchHighlightIndex();
    }
}

}  // namespace CHIron::Gui
