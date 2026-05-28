#pragma once

#include "flit_record.hpp"
#include "trace_session.hpp"

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QUndoStack;

namespace CHIron::Gui {

class FlitTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum class DisplayMode {
        Decoded = 0,
        Decimal,
        Hexadecimal
    };

    enum class SearchMode {
        Filter = 0,
        Highlight
    };

    struct SearchHighlightNavigationIndex {
        int offset = 0;
        int total = 0;
    };

public:
    enum Column {
        XactionIndexColumn = 0,
        TimeColumn,
        ChannelColumn,
        DirectionColumn,
        OpcodeColumn,
        SourceColumn,
        TargetColumn,
        TxnColumn,
        AddressColumn,
        DataIdColumn,
        RespColumn,
        FwdStateColumn,
        RespErrColumn,
        ColumnCount
    };

public:
    enum Role {
        ChannelAccentRole = Qt::UserRole + 1,
        TransactionHighlightRole,
        ChannelTagRole,
        NodeLabelTextRole,
        NodeLabelColorRole,
        XactionIndexStateRole,
        BadgeFontScaleRole,
        BadgeLeftAlignedRole
    };

public:
    explicit FlitTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    void clear();
    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setTraceMetadataOverride(std::optional<CLogBTraceMetadata> metadata);
    void refreshTraceRows();
    void refreshTraceRowRange(int firstRow, int lastRow);
    void setRows(std::vector<FlitRecord> rows);
    void appendRow(const FlitRecord& row);
    void appendRows(const std::vector<FlitRecord>& rows);
    bool canAppendRowsIncrementally(const std::vector<FlitRecord>& rows) const;
    bool appendRowsIncrementally(const std::vector<FlitRecord>& rows);
    bool insertRowsAt(int visibleRow, std::vector<FlitRecord> rows);
    bool insertRowsAtLogicalRow(int logicalRow, std::vector<FlitRecord> rows);
    bool insertRowsByTimestamp(std::vector<FlitRecord> rows);
    bool cloneRowsAt(int visibleRow, int count);
    bool deleteRowAt(int visibleRow);
    bool deleteRowsAt(QVector<int> visibleRows);
    bool removeRowsAt(QVector<int> visibleRows);
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    void undoEdit();
    void redoEdit();
    void markUndoStackClean();
    QString undoText() const;
    QString redoText() const;

    void setOpcodeFilter(const QString& text);
    void setSourceIdFilter(const QString& text);
    void setTargetIdFilter(const QString& text);
    void setTxnIdFilter(const QString& text);
    void setDbidFilter(const QString& text);
    void setAddressFilter(const QString& text);
    QString opcodeFilter() const;
    QString sourceIdFilter() const;
    QString targetIdFilter() const;
    QString txnIdFilter() const;
    QString dbidFilter() const;
    QString addressFilter() const;
    void cancelPendingFilterBuild();
    SearchMode searchMode() const noexcept;
    void setSearchMode(SearchMode mode);
    QStringList availableOptionalFields() const;
    QStringList availableOptionalFieldsForScope(const QString& scope) const;
    bool isFieldColumnVisible(const QString& fieldName) const noexcept;
    void setFieldColumnVisible(const QString& fieldName, bool visible);
    void setFieldFilter(const QString& fieldName, const QString& text);
    QString fieldFilter(const QString& fieldName) const;
    int columnForField(const QString& fieldName) const noexcept;
    void setChannelVisible(FlitChannel channel, bool visible);
    void setDirectionVisible(FlitDirection direction, bool visible);
    bool channelVisible(FlitChannel channel) const noexcept;
    bool directionVisible(FlitDirection direction) const noexcept;
    void setXactionDeniedOnlyFilter(bool enabled);
    bool xactionDeniedOnlyFilter() const noexcept;
    DisplayMode displayMode(int column) const noexcept;
    bool supportsDisplayMode(int column) const noexcept;
    void setDisplayMode(int column, DisplayMode mode);
    bool nodeLabelsVisible() const noexcept;
    void setNodeLabelsVisible(bool visible);

    int totalCount() const noexcept;
    int visibleCount() const noexcept;
    int channelCountAll(FlitChannel channel) const noexcept;
    QVector<const FlitRecord*> visibleRows() const;
    std::vector<FlitRecord> sourceRowsSnapshot() const;
    std::vector<std::pair<int, FlitRecord>> takeDirtySourceRowsSnapshot();
    void clearDirtySourceRowsSnapshot();
    bool isSessionBacked() const noexcept;
    bool editable() const noexcept;
    void setEditable(bool editable);
    bool isDirty() const noexcept;
    void setDirty(bool dirty);
    bool isIdentityVisibleRows() const noexcept;
    const CLogBTraceMetadata* traceMetadata() const noexcept;
    int logicalRowAt(int visibleRow) const noexcept;
    int visibleRowForLogicalRow(int logicalRow) const noexcept;
    const FlitRecord* tryRecordAt(int visibleRow) const noexcept;
    const FlitRecord* recordAt(int visibleRow) const;
    const FlitRecord* recordForLogicalRow(int logicalRow) const;
    std::vector<FlitRecord> sparseEditedRowsSnapshot() const;
    bool hasSparseEdits() const noexcept;
    bool forEachSourceRowInOrder(const std::function<bool(const FlitRecord&, int)>& callback,
                                 CLogBTraceLoadError& error) const;
    void setTransactionHighlightFromVisibleRow(int visibleRow);
    bool toggleTransactionHighlightFromVisibleRow(int visibleRow);
    void clearTransactionHighlight();
    bool clearTransactionHighlightWithoutRefresh();
    bool isTransactionHighlightAnchorRow(int visibleRow) const;
    bool isTransactionHighlightedRow(int visibleRow) const;
    bool isSearchHighlightedRow(int visibleRow) const;
    int findSearchHighlightedRow(int currentVisibleRow, bool forward) const;
    SearchHighlightNavigationIndex searchHighlightNavigationIndex(int currentVisibleRow) const;
    bool isSearchHighlightIndexBuilding() const noexcept;

Q_SIGNALS:
    void filteringProgressChanged(bool active, const QString& text, int value, int maximum);
    void filteringFailed(const QString& summary, const QString& detail);
    void searchHighlightNavigationChanged();
    void editRejected(const QString& summary, const QString& detail);
    void dirtyChanged(bool dirty);
    void rowsEdited();
    void undoRedoAvailabilityChanged(bool canUndo, bool canRedo);

private:
    struct FilterCriteria {
        QString opcodeFilter;
        QString sourceIdFilter;
        QString targetIdFilter;
        QString txnIdFilter;
        QString dbidFilter;
        QString addressFilter;
        QHash<QString, QString> optionalFieldFilters;
        bool showReq = true;
        bool showRsp = true;
        bool showDat = true;
        bool showSnp = true;
        bool showTx = true;
        bool showRx = true;
        bool showXactionDeniedOnly = false;
    };

    struct AsyncFilterBuildState {
        quint64 generation = 0;
        FilterCriteria criteria;
        CLogBTraceChannelMask enabledMask = 0;
        bool identityCandidate = true;
        bool hasTextFilters = false;
        bool canUseFastRawFilters = false;
        bool canUseOptionalFieldIndexes = false;
        CLogBTraceFastFilter fastFilter;
        std::size_t nextBlockIndex = 0;
        std::size_t nextLocalIndex = 0;
        std::uint64_t nextRowStart = 0;
        int nextExpectedLogicalRow = 0;
        std::uint64_t totalRows = 0;
        std::uint64_t processedRows = 0;
        std::uint64_t scanChunkRows = 0;
        std::size_t fastScanChunkRecords = 0;
        int logicalLimit = 0;
        bool scanFailed = false;
        CLogBTraceLoadError error;
        std::vector<int> visibleRows;
        QStringList rebuiltFields;
        QHash<QString, QStringList> rebuiltFieldsByScope;
    };

    struct HighlightSearchIndexBuildState {
        quint64 generation = 0;
        FilterCriteria criteria;
        CLogBTraceChannelMask enabledMask = 0;
        bool hasTextFilters = false;
        bool canUseFastRawFilters = false;
        bool canUseOptionalFieldIndexes = false;
        CLogBTraceFastFilter fastFilter;
        std::size_t nextBlockIndex = 0;
        std::size_t nextLocalIndex = 0;
        std::uint64_t nextRowStart = 0;
        std::uint64_t totalRows = 0;
        std::uint64_t processedRows = 0;
        std::uint64_t scanChunkRows = 0;
        std::size_t fastScanChunkRecords = 0;
        int logicalLimit = 0;
        bool scanFailed = false;
        CLogBTraceLoadError error;
    };

    struct SourceRowRef {
        int originalRow = -1;
        int insertedRow = -1;
    };

    struct RowMutationEntry {
        int logicalRow = -1;
        SourceRowRef ref;
        FlitRecord row;
    };

    struct RowMutationSnapshot {
        std::vector<RowMutationEntry> entries;
    };

    friend RowMutationSnapshot MakeSnapshot(std::vector<RowMutationEntry> entries);
    friend class FlitTableModelSetRowCommand;
    friend class FlitTableModelInsertRowsCommand;
    friend class FlitTableModelDeleteRowCommand;

private:
    std::shared_ptr<TraceSession> traceSession_;
    std::optional<CLogBTraceMetadata> traceMetadataOverride_;
    std::vector<FlitRecord> rows_;
    QSet<int> dirtySourceRows_;
    QHash<int, FlitRecord> editedRows_;
    std::vector<FlitRecord> insertedRows_;
    std::vector<SourceRowRef> sparseRowRefs_;
    std::vector<int> visibleRows_;
    bool identityVisibleRows_ = false;
    int identityVisibleRowCount_ = 0;
    QString opcodeFilter_;
    QString sourceIdFilter_;
    QString targetIdFilter_;
    QString txnIdFilter_;
    QString dbidFilter_;
    QString addressFilter_;
    std::array<DisplayMode, ColumnCount> displayModes_{};
    std::array<int, 4> channelCounts_{};
    QStringList availableOptionalFields_;
    QHash<QString, QStringList> availableOptionalFieldsByScope_;
    QStringList visibleOptionalFields_;
    QHash<QString, DisplayMode> optionalFieldDisplayModes_;
    QHash<QString, QString> optionalFieldFilters_;
    SearchMode searchMode_ = SearchMode::Filter;
    bool showReq_ = true;
    bool showRsp_ = true;
    bool showDat_ = true;
    bool showSnp_ = true;
    bool showTx_ = true;
    bool showRx_ = true;
    bool showXactionDeniedOnly_ = false;
    quint64 filterBuildGeneration_ = 0;
    bool filteringActive_ = false;
    std::unique_ptr<AsyncFilterBuildState> asyncFilterBuild_;
    quint64 highlightBuildGeneration_ = 0;
    bool highlightIndexComplete_ = true;
    std::vector<int> highlightedRows_;
    std::unique_ptr<HighlightSearchIndexBuildState> highlightSearchIndexBuild_;
    QSet<QString> transactionHighlightKeys_;
    int transactionHighlightAnchorLogicalRow_ = -1;
    bool nodeLabelsVisible_ = true;
    bool editable_ = false;
    bool dirty_ = false;
    QUndoStack* undoStack_ = nullptr;

private:
    int sourceRowCount() const noexcept;
    bool hasSparseRowMap() const noexcept;
    bool sourceRowIsInserted(int logicalRow) const noexcept;
    int originalRowForSourceRow(int logicalRow) const noexcept;
    int sourceRowForOriginalRow(int originalRow) const noexcept;
    int sourceRowForInsertedRow(int insertedRow) const noexcept;
    SourceRowRef sourceRowRefAt(int logicalRow) const noexcept;
    int visibleRowCountInternal() const noexcept;
    void rebuildSparseRowRefs();
    bool hasActiveSearchFilters() const noexcept;
    bool hasActiveRowFilters() const noexcept;
    bool hasActiveFilters() const noexcept;
    bool canUseIdentityVisibleRows() const noexcept;
    FilterCriteria currentFilterCriteria() const;
    FilterCriteria currentRowFilterCriteria() const;
    bool optionalFieldFiltersHaveFastIndexes(const QHash<QString, QString>& filters);
    const FlitRecord* tryRecordForLogicalRow(int logicalRow) const noexcept;
    bool nodeLabelForIndex(const FlitRecord& record, int column, QString& label, QColor& color) const;
    bool channelEnabled(FlitChannel channel) const noexcept;
    bool channelEnabled(FlitChannel channel, const FilterCriteria& criteria) const noexcept;
    bool directionEnabled(FlitDirection direction) const noexcept;
    bool directionEnabled(FlitDirection direction, const FilterCriteria& criteria) const noexcept;
    bool passesFilters(const FlitRecord& row) const;
    bool passesFilters(const FlitRecord& row, const FilterCriteria& criteria) const;
    void recountChannels();
    void rebuildAvailableOptionalFields();
    void rebuildVisibleRowsNoReset();
    void cancelAsyncFilterBuild(bool notifyProgress = true);
    void startAsyncFilterBuild();
    void processAsyncFilterBuild(quint64 generation);
    void applyAsyncFilterBuild(quint64 generation);
    void clearSearchHighlightIndex(bool notify = true);
    void clearTransactionHighlightState() noexcept;
    bool recordMatchesTransactionHighlight(const FlitRecord& record, int logicalRow) const;
    void refreshTransactionHighlights();
    void rebuildSearchHighlightIndex();
    void processSearchHighlightIndexBuild(quint64 generation);
    void appendHighlightedLogicalRows(const std::vector<int>& logicalRows);
    void appendHighlightedLogicalRange(std::uint64_t rowStart, std::uint64_t rowCount);
    void emitFilteringProgress(const QString& text, int value, int maximum);
    void emitFilteringFailure(const QString& summary, const QString& detail);
    void applySearchFilterChange();
    void refreshSearchHighlights();
    void rebuildVisibleRows();
    void resetUndoStack();
    void emitUndoRedoAvailability();
    void updateDirtyFromUndoStack();
    bool applySetLogicalRowDirect(int logicalRow, const FlitRecord& row);
    bool applyInsertRowsAtLogicalRowDirect(int logicalRow,
                                           const std::vector<FlitRecord>& rows,
                                           RowMutationSnapshot* snapshot);
    bool applyInsertRowsByTimestampDirect(const std::vector<FlitRecord>& rows,
                                          RowMutationSnapshot* snapshot);
    bool applyDeleteLogicalRowDirect(int logicalRow, RowMutationSnapshot* snapshot);
    bool applyRestoreRowsDirect(const RowMutationSnapshot& snapshot);
    bool applyRemoveRowsDirect(const RowMutationSnapshot& snapshot);
    void finishRowMutationReset();
};

}  // namespace CHIron::Gui
