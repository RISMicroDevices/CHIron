#pragma once

#include "marker_store.hpp"

#include <QHash>
#include <QWidget>

#include <vector>

class QComboBox;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;
class QTabBar;
class QTableWidget;
class QTableWidgetItem;
class QToolButton;
class QEvent;

namespace CHIron::Gui {

struct TraceMarkerDisplaySummary {
    QString markerId;
    QString channel;
    QString opcode;
    QString address;
};

class MarkerWidget final : public QWidget {
    Q_OBJECT

public:
    explicit MarkerWidget(QWidget* parent = nullptr);
    ~MarkerWidget() override;

    void setMarkers(const std::vector<TraceMarker>& markers, const QString& selectedMarkerId);
    void setMarkers(const std::vector<TraceMarker>& markers,
                    const QString& selectedMarkerId,
                    const MarkerStickyState& stickyState);
    void setMarkers(const std::vector<TraceMarker>& markers,
                    const QString& selectedMarkerId,
                    const MarkerStickyState& stickyState,
                    const std::vector<TraceMarkerDisplaySummary>& summaries);
    MarkerStickyState stickyState() const;
    void clear();

    void stickyItemSelectMarker(QString markerId, bool activate);
    void stickyItemEditMarker(const QString& markerId, const QString& label, const QString& memo);
    void stickyItemUpdateLayout(const QString& markerId, const QPointF& position, const QSizeF& size);
    QRectF stickyItemAdjustedRect(const QString& markerId,
                                  QRectF candidateRect,
                                  Qt::KeyboardModifiers modifiers,
                                  bool resizeOnly = false) const;
    void stickyItemSetAlignmentGridVisible(bool visible);
    void stickyItemShowContextMenu(const QString& markerId, const QPoint& globalPos);

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
    void testSetStickyMode(bool sticky);
    int testStickyNoteCount() const;
    bool testMoveStickyNote(const QString& markerId, QPointF position, QSizeF size);
    bool testEditStickyNote(const QString& markerId, const QString& label, const QString& memo);
    bool testDoubleClickStickyNote(const QString& markerId);
    bool testAddStickyGroup(const QString& name);
    bool testSelectStickyGroup(const QString& groupId);
    bool testAddMarkerToActiveStickyGroup(const QString& markerId);
    bool testRemoveMarkerFromActiveStickyGroup(const QString& markerId);
    QString testActiveStickyGroupId() const;
    QString testStickySummaryText(const QString& markerId) const;
    bool testStickySummaryFontIsSmaller(const QString& markerId) const;
    bool testStickyMemoStartsBelowSummary(const QString& markerId) const;
    bool testCommitStickyMemoText(const QString& markerId, const QString& memo);
    bool testDragStickyNote(const QString& markerId, QPointF position, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    bool testPreviewStickyNoteDrag(const QString& markerId, QPointF position, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void testFinishStickyInteraction();
    bool testResizeStickyNote(const QString& markerId, QSizeF size, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    bool testStickyGridVisible() const;
    QString testSelectedMarkerId() const;
    bool testStickyJumpToRow(const QString& markerId);
    bool testStickyDeleteMarker(const QString& markerId);
    bool testStickyCanDeleteFromGroup(const QString& markerId) const;
    bool testStickyDeleteFromGroup(const QString& markerId);
    bool testStickyCopyToGroup(const QString& markerId, const QString& groupId);
    bool testStickyMoveToGroup(const QString& markerId, const QString& groupId);
    bool testStickyCopyToNewGroup(const QString& markerId, const QString& groupName);
    bool testStickyMoveToNewGroup(const QString& markerId, const QString& groupName);
    bool testStickyMarkerInGroup(const QString& groupId, const QString& markerId) const;
    int testStickyCustomGroupMembershipCount(const QString& markerId) const;
    bool testTriggerStickyDeleteShortcut();
    void testSetStickyTextEditing(bool editing);
#endif

Q_SIGNALS:
    void markerActivated(QString markerId);
    void markerSelected(QString markerId);
    void markerEdited(TraceMarker marker);
    void markerRemoved(QString markerId);
    void stickyStateChanged(MarkerStickyState state);
    void saveRequested();
    void loadRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void wireSignals();
    void commitMemoEdit();
    void refreshTable();
    void refreshMemo();
    void refreshSticky();
    void refreshStickyTabs();
    void ensureStickyStateForMarkers(bool addNewMarkersToActiveGroup);
    void emitStickyStateChanged();
    void rebuildFilteredMarkerIndices();
    bool markerMatchesSearch(const TraceMarker& marker) const;
    const TraceMarkerDisplaySummary* markerSummaryById(const QString& markerId) const;
    int rowForMarkerId(const QString& markerId) const;
    std::size_t markerIndexForTableRow(int row) const;
    TraceMarker* markerById(const QString& markerId);
    const TraceMarker* markerById(const QString& markerId) const;
    MarkerStickyGroup* activeStickyGroup();
    const MarkerStickyGroup* activeStickyGroup() const;
    MarkerStickyGroup* stickyGroupById(const QString& groupId);
    const MarkerStickyGroup* stickyGroupById(const QString& groupId) const;
    MarkerStickyNoteLayout* stickyLayoutForMarker(MarkerStickyGroup& group,
                                                  const QString& markerId,
                                                  bool createIfMissing);
    MarkerStickyNoteLayout* stickyLayoutForActiveMarker(const QString& markerId, bool createIfMissing);
    bool activeStickyTabIsAll() const;
    bool stickyGroupContainsMarker(const MarkerStickyGroup& group, const QString& markerId) const;
    void setMarkerSelection(QString markerId, bool activate);
    void applyStickyMarkerEdit(const QString& markerId, const QString& label, const QString& memo);
    void updateStickyMarkerLayout(const QString& markerId, const QPointF& position, const QSizeF& size);
    QRectF snapStickyRectToNeighbors(const QString& markerId, const QRectF& candidateRect) const;
    QRectF snapStickyResizeRectToNeighbors(const QString& markerId, const QRectF& candidateRect) const;
    QRectF snapStickyRectToGrid(const QRectF& candidateRect, bool resizeOnly) const;
    QString createStickyGroup(QString name);
    bool canRemoveStickyMarkerFromActiveGroup(const QString& markerId) const;
    int customStickyGroupMembershipCount(const QString& markerId) const;
    void removeStickyMarkerFromGroup(MarkerStickyGroup& group, const QString& markerId);
    bool removeStickyMarkerFromActiveGroup(const QString& markerId);
    bool copyStickyMarkerToGroup(const QString& markerId, const QString& groupId);
    bool moveStickyMarkerToGroup(const QString& markerId, const QString& groupId);
    bool copyOrMoveStickyMarkerToNewGroup(const QString& markerId, bool move, const QString& requestedName = QString());
    bool stickyNoteVisibleInActiveView(const QString& markerId) const;
    bool stickyTextEditorHasFocus() const;
    bool deleteSelectedStickyNote();
    void addStickyGroup();
    void renameActiveStickyGroup();
    void deleteActiveStickyGroup();
    void showAddMarkerToStickyGroupMenu();
    void removeSelectedMarkerFromStickyGroup();
    void updateSelectedMarkerFromTable();
    void applyItemEdit(QTableWidgetItem* item);

    QToolButton* saveButton_ = nullptr;
    QToolButton* loadButton_ = nullptr;
    QToolButton* deleteButton_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QToolButton* clearSearchButton_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QStackedWidget* viewStack_ = nullptr;
    QWidget* listPage_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPlainTextEdit* memoEdit_ = nullptr;
    QWidget* stickyPage_ = nullptr;
    QTabBar* stickyTabBar_ = nullptr;
    QToolButton* stickyAddGroupButton_ = nullptr;
    QToolButton* stickyRenameGroupButton_ = nullptr;
    QToolButton* stickyDeleteGroupButton_ = nullptr;
    QToolButton* stickyAddMarkerButton_ = nullptr;
    QToolButton* stickyRemoveMarkerButton_ = nullptr;
    QGraphicsView* stickyView_ = nullptr;
    QGraphicsScene* stickyScene_ = nullptr;
    std::vector<TraceMarker> markers_;
    QHash<QString, TraceMarkerDisplaySummary> markerSummaries_;
    std::vector<std::size_t> filteredMarkerIndices_;
    MarkerStickyState stickyState_;
    QString selectedMarkerId_;
    QString memoEditMarkerId_;
    QString memoEditOriginalText_;
    bool memoEditDirty_ = false;
    bool rebuilding_ = false;
#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
    bool stickyTextEditingForTest_ = false;
#endif
};

}  // namespace CHIron::Gui
