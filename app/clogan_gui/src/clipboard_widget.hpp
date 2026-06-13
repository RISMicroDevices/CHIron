#pragma once

#include "flit_record.hpp"
#include "flit_table_model.hpp"

#include <QWidget>

#include <cstdint>
#include <optional>
#include <vector>

class QLabel;
class QLineEdit;
class QHBoxLayout;
class QTableView;
class QToolButton;

namespace CHIron::Gui {

class TraceCacheLineMinimap;

struct ClipboardSourceKey {
    quint64 sessionId = 0;
    int logicalRow = -1;
};

struct ClipboardEntry {
    ClipboardSourceKey source;
    std::uint64_t sequence = 0;
    FlitRecord record;
    FlitRecord originalRecord;
};

enum class ClipboardScope {
    Session = 0,
    Global
};

class ClipboardWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ClipboardWidget(QWidget* parent = nullptr);

    FlitTableModel* model() noexcept;
    const FlitTableModel* model() const noexcept;
    QTableView* tableView() noexcept;
    const QTableView* tableView() const noexcept;

    void setEntries(std::vector<ClipboardEntry>* entries, const CLogBTraceMetadata* metadata);
    std::vector<ClipboardEntry>* entries() noexcept;
    const std::vector<ClipboardEntry>* entries() const noexcept;
    void refreshFromEntries();
    void beginIncrementalRefreshFromEntries(std::vector<ClipboardEntry>* entries,
                                            const CLogBTraceMetadata* metadata,
                                            bool entriesAlreadySynced = false);
    void beginAppendRefreshFromEntries(std::vector<ClipboardEntry>* entries,
                                       const CLogBTraceMetadata* metadata,
                                       std::size_t appendBeginIndex);
    bool appendEntryRowsIncrementally(std::size_t beginIndex, std::size_t count);
    void abortIncrementalRefreshFromEntries();
    void finishIncrementalRefreshFromEntries();
    void syncEntriesFromModel();
    void syncDirtyEntriesFromModel();
    QStringList availableOptionalFields() const;
    QStringList visibleOptionalFields() const;
    QStringList materializedVisibleFieldNames() const;
    void setOptionalFieldColumnsVisible(const QStringList& fieldNames);
    bool replaceEntryRowsForStorage(const std::vector<std::pair<int, FlitRecord>>& rows);
    void refreshRows(int firstVisibleRow, int lastVisibleRow);
    void applyTraceTableRowStyle(int referenceVisibleRowCount);
    void resizeColumnsToTraceDefaults();
    void setScope(ClipboardScope scope);
    ClipboardScope scope() const noexcept;
    bool toolbarFolded() const noexcept;
    void setToolbarFolded(bool folded);
    void deleteSelectedRows();
    void setReadOnly(bool readOnly);
    bool readOnly() const noexcept;
    bool hasModifiedRows() const;
    bool entryModified(int logicalRow) const;
    std::vector<ClipboardEntry> currentEntriesSnapshot() const;
    std::optional<ClipboardEntry> entryForVisibleRow(int visibleRow) const;
    void setCacheMinimap(TraceCacheLineMinimap* minimap);

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
    void testSetOpcodeFilter(const QString& text);
    void testSetSearchMode(FlitTableModel::SearchMode mode);
#endif

Q_SIGNALS:
    void scopeChanged(ClipboardScope scope);
    void entriesEdited();
    void rowActivated(ClipboardEntry entry);
    void saveRequested();

private:
    QWidget* toolbar_ = nullptr;
    QWidget* toolbarContent_ = nullptr;
    QToolButton* foldButton_ = nullptr;
    QToolButton* scopeButton_ = nullptr;
    QToolButton* searchModeButton_ = nullptr;
    QToolButton* reqButton_ = nullptr;
    QToolButton* rspButton_ = nullptr;
    QToolButton* datButton_ = nullptr;
    QToolButton* snpButton_ = nullptr;
    QToolButton* txButton_ = nullptr;
    QToolButton* rxButton_ = nullptr;
    QToolButton* nodeLabelsButton_ = nullptr;
    QToolButton* cacheMapButton_ = nullptr;
    QToolButton* cacheMapAddButton_ = nullptr;
    QToolButton* cacheMapFadeButton_ = nullptr;
    QToolButton* clearButton_ = nullptr;
    QToolButton* saveButton_ = nullptr;
    QToolButton* modeButton_ = nullptr;
    QLabel* totalBadge_ = nullptr;
    QLabel* visibleBadge_ = nullptr;
    QLabel* modifiedBadge_ = nullptr;
    QLineEdit* opcodeFilterEdit_ = nullptr;
    QLineEdit* sourceIdFilterEdit_ = nullptr;
    QLineEdit* targetIdFilterEdit_ = nullptr;
    QLineEdit* txnIdFilterEdit_ = nullptr;
    QLineEdit* dbidFilterEdit_ = nullptr;
    QLineEdit* addressFilterEdit_ = nullptr;
    QTableView* table_ = nullptr;
    FlitTableModel* model_ = nullptr;
    std::vector<ClipboardEntry>* entries_ = nullptr;
    ClipboardScope scope_ = ClipboardScope::Session;
    bool toolbarFolded_ = true;
    bool readOnly_ = true;
    bool syncingModel_ = false;
    bool modifiedBadgeDirty_ = true;
    bool hasModifiedRowsCache_ = false;
    TraceCacheLineMinimap* cacheMinimap_ = nullptr;

private:
    void buildUi();
    void wireSignals();
    void updateBadges();
    void updateScopeButton();
    void updateModeButton();
    void updateSearchModeButton();
    void showRowContextMenu(const QPoint& position);
    QToolButton* makeToggle(const QString& label, bool checked = true) const;
    QLineEdit* addFilterField(QWidget* parent,
                              QHBoxLayout* rowLayout,
                              const QString& label,
                              const QString& placeholder,
                              int width);
    void showAddCacheMapLineDialog();
};

}  // namespace CHIron::Gui
