#pragma once

#include "clog_b_trace_loader.hpp"
#include "clipboard_widget.hpp"
#include "flit_details_model.hpp"
#include "flit_table_model.hpp"
#include "marker_store.hpp"
#include "trace_issue_store.hpp"
#include "trace_statistics.hpp"

#include <QByteArray>
#include <QHash>
#include <QMainWindow>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

QT_FORWARD_DECLARE_CLASS(QCloseEvent)
class QAction;
class QCheckBox;
class QDialog;
class QEvent;
class QModelIndex;
class QLabel;
class QHBoxLayout;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QStackedLayout;
class QTabBar;
class QTableView;
class QTableWidget;
class QTextBrowser;
class QTimer;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace CHIron::Gui {

class TimelineWidget;
class AddressWidget;
class CacheWidget;
class LatencyWidget;
class LatencyDiffWidget;
class TransactionWidget;
class ClipboardWidget;
class TraceCacheLineMinimap;
class TraceMarkerOverlay;
class MarkerWidget;
class ErrorsWidget;
struct TraceMarkerDisplaySummary;
class TraceSession;

struct TraceIssueBuildResult {
    std::shared_ptr<const std::vector<TraceIssue>> rawIssues =
        std::make_shared<const std::vector<TraceIssue>>();
    std::shared_ptr<const std::vector<TraceIssue>> visibleIssues =
        std::make_shared<const std::vector<TraceIssue>>();
    TraceIssueCounts counts;
};

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
    int testSessionCount() const noexcept;
    int testActiveSessionIndex() const noexcept;
    QString testActiveSessionLabel() const;
    QStringList testSessionLabels() const;
    int testVisibleTraceRowCount() const noexcept;
    bool testActiveModelIsSessionBacked() const noexcept;
    int testActiveModelSourceRowsSnapshotSize() const;
    int testActiveTraceSessionCachedPageCount() const noexcept;
    int testActiveTraceSessionCachedBlockCount() const noexcept;
    int testActiveTraceSessionMaxCachedPages() const noexcept;
    int testActiveTraceSessionMaxCachedBlocks() const noexcept;
    bool testApplyTraceRows(std::vector<FlitRecord> rows,
                            const QString& sourceLabel,
                            const QString& sourcePath = QString());
    bool testLoadTraceFile(const QString& filePath);
    bool testReloadActiveTrace();
    bool testSwitchToSession(int index);
    bool testCloseActiveSession();
    bool testCloseSession(int index);
    bool testCloseOtherSessions(int index);
    void testCloseAllSessions();
    void testStartStatisticsComputation();
    bool testSessionStatisticsActive(int index) const noexcept;
    bool testSessionStatisticsComputed(int index) const noexcept;
    bool testActiveSessionStatisticsComputed() const noexcept;
    void testSetOpcodeFilter(const QString& text);
    QString testOpcodeFilter() const;
    void testSetFieldColumnVisible(const QString& fieldName, bool visible);
    bool testFieldColumnVisible(const QString& fieldName) const;
    void testSetFieldFilter(const QString& fieldName, const QString& text);
    QString testFieldFilter(const QString& fieldName) const;
    int testFieldColumn(const QString& fieldName) const noexcept;
    void testSetSearchMode(FlitTableModel::SearchMode mode);
    FlitTableModel::SearchMode testSearchMode() const noexcept;
    void testSetTraceToolbarFolded(bool folded);
    bool testTraceToolbarFolded() const noexcept;
    bool testTraceToolbarFoldedControlsHidden() const noexcept;
    bool testTraceToolbarSummaryVisible() const noexcept;
    int testTraceToolbarHeight() const noexcept;
    QString testTraceToolbarFoldButtonText() const;
    QVariantMap testTraceToolbarFoldButtonGeometry() const;
    void testSetDisplayMode(int column, FlitTableModel::DisplayMode mode);
    FlitTableModel::DisplayMode testDisplayMode(int column) const noexcept;
    void testSelectLogicalRow(int logicalRow);
    int testSelectedLogicalRow() const noexcept;
    void testSetTableColumnWidth(int column, int width);
    int testTableColumnWidth(int column) const noexcept;
    void testSetTableScrollValue(int value);
    int testTableScrollValue() const noexcept;
    void testSelectTopologyNode(int nodeId);
    int testSelectedTopologyNode() const noexcept;
    QVariantMap testTimelineViewState() const;
    QVariantMap testAddressViewState() const;
    QVariantMap testCacheViewState() const;
    QVariantMap testLatencyViewState() const;
    QVariantMap testTransactionViewState() const;
    QString testDiagnosticsStateSnapshot() const;
    void testRestoreTimelineViewState(const QVariantMap& state);
    void testRestoreAddressViewState(const QVariantMap& state);
    void testRestoreCacheViewState(const QVariantMap& state);
    void testRestoreLatencyViewState(const QVariantMap& state);
    void testRestoreTransactionViewState(const QVariantMap& state);
    void testBuildVisualizationViews();
    QString testTimelineVisibleRangeText() const;
    QString testAddressVisibleRangeText() const;
    QString testCacheVisibleRangeText() const;
    QString testTransactionVisibleRangeText() const;
    int testTimelineWidgetInstanceId() const noexcept;
    int testAddressWidgetInstanceId() const noexcept;
    int testCacheWidgetInstanceId() const noexcept;
    int testTransactionWidgetInstanceId() const noexcept;
    int testCacheSpanCount() const noexcept;
    int testCacheBandCount() const noexcept;
    quint64 testCacheBuildGeneration() const noexcept;
    void testInjectCacheSpans(int bandCount, int spanCount);
    int testTransactionSpanCount() const noexcept;
    int testTransactionLaneCount() const noexcept;
    quint64 testTransactionBuildGeneration() const noexcept;
    void testInjectTransactionSpans(int laneCount, int spanCount);
    int testMarkerCount() const noexcept;
    bool testAddMarkerAtLogicalRow(int logicalRow);
    bool testEditMarkerLabel(int markerIndex, const QString& label);
    bool testEditMarkerDetails(int markerIndex,
                               const QString& label,
                               const QString& colorName,
                               const QString& memo);
    QString testMarkerLabelAt(int markerIndex) const;
    QString testMarkerColorAt(int markerIndex) const;
    QString testMarkerMemoAt(int markerIndex) const;
    qint64 testMarkerTimestampAt(int markerIndex) const noexcept;
    int testMarkerLogicalRowAt(int markerIndex) const noexcept;
    bool testRemoveMarkerAt(int markerIndex);
    bool testMoveMarkerAt(int markerIndex, int targetLogicalRow);
    int testSelectedMarkerLogicalRow() const noexcept;
    bool testCanUndoUnified() const;
    bool testCanRedoUnified() const;
    QString testUnifiedUndoText() const;
    QString testUnifiedRedoText() const;
    void testUndoUnified();
    void testRedoUnified();
    bool testSetTraceEditable(bool editable);
    bool testEditTraceTimestampAtLogicalRow(int logicalRow, qint64 timestamp);
    qint64 testTraceTimestampAtLogicalRow(int logicalRow) const noexcept;
    bool testSaveMarkersJson(const QString& path);
    bool testLoadMarkersJson(const QString& path);
    bool testTraceMarkerOverlayVisible() const noexcept;
    QRect testTraceMarkerOverlayGeometry() const;
    QRect testTraceMarkerMinimapGeometry() const;
    QRect testTraceMarkerCollapsedTagGeometry(int markerIndex) const;
    QRect testTraceMarkerExpandedTagGeometry(int markerIndex) const;
    QRect testTraceMarkerLeftOverlayGeometry() const;
    QRect testTraceMarkerLeftPolygonGeometry(int markerIndex) const;
    QRect testTraceMarkerLeftNameGeometry(int markerIndex) const;
    QRect testTraceTableLogicalRowViewportGeometry(int logicalRow) const;
    bool testClickTraceMarkerTag(int markerIndex);
    bool testClickTraceMarkerLeftPolygon(int markerIndex);
    bool testDoubleClickTraceMarkerLeftPolygon(int markerIndex);
    bool testStartTraceMarkerMoveFromTag(int markerIndex);
    bool testStartTraceMarkerMoveFromLeftPolygon(int markerIndex);
    bool testDropTraceMarkerMoveOnLogicalRow(int logicalRow);
    void testCancelTraceMarkerMove();
    bool testTraceMarkerMoveActive() const noexcept;
    void testNavigateMarker(bool forward);
    QRect testTraceTableScrollBarGeometry() const;
    QRect testTraceTableViewportGeometry() const;
    void testShowErrorsDock();
    int testActivePersistedTraceIssueCount() const;
    int testErrorIssueCount() const noexcept;
    int testErrorWarningCount() const noexcept;
    int testErrorErrorCount() const noexcept;
    int testErrorWidgetWarningCount() const noexcept;
    int testErrorWidgetErrorCount() const noexcept;
    bool testErrorsBuildActive() const noexcept;
    bool testErrorsBuildComplete() const noexcept;
    QString testErrorsBuildDebugState() const;
    QString testErrorWidgetStatusText() const;
    bool testErrorWidgetProgressVisible() const noexcept;
    QString testErrorSeverityButtonText(TraceIssueSeverity severity) const;
    QString testErrorIssueSummaryAt(int row) const;
    QString testErrorIssueCodeAt(int row) const;
    QString testErrorIssueDescriptionAt(int row) const;
    QString testErrorIssueSourceAt(int row) const;
    QString testErrorIssueRowTextAt(int row) const;
    QString testErrorIssueDetailsAt(int row) const;
    QString testExpandedErrorIssueDetailsAt(int row) const;
    bool testErrorIssueExpanded(int row) const;
    bool testSetErrorIssueExpanded(int row, bool expanded);
    bool testActivateErrorIssueAt(int row);
    QString testErrorIssueDisposition(TraceIssueSource source) const;
    void testSetErrorIssueDisposition(TraceIssueSource source, TraceIssueDisposition disposition);
    bool testErrorSeverityVisible(TraceIssueSeverity severity) const noexcept;
    void testSetErrorSeverityVisible(TraceIssueSeverity severity, bool visible);
    void testStartXactionIndexing(bool rebuildExisting = false);
    bool testSessionXactionIndexActive(int index) const noexcept;
    void testSetClipboardScope(ClipboardScope scope);
    ClipboardScope testClipboardScope() const noexcept;
    bool testClipboardToolbarFolded() const noexcept;
    bool testClipboardReadOnly() const noexcept;
    void testSetClipboardReadOnly(bool readOnly);
    bool testClipboardHasModifiedRows() const;
    bool testClipboardEntryModified(int visibleRow) const;
    int testClipboardRowCount() const noexcept;
    int testClipboardVisibleRowCount() const noexcept;
    qint64 testClipboardTimestampAt(int visibleRow) const noexcept;
    QString testClipboardOpcodeAt(int visibleRow) const;
    QString testClipboardTxnIdAt(int visibleRow) const;
    bool testSetClipboardFieldColumnVisible(const QString& fieldName, bool visible);
    QString testClipboardFieldValueAt(int visibleRow, const QString& fieldName) const;
    bool testSetClipboardFieldValueAt(int visibleRow, const QString& fieldName, const QString& value);
    bool testRemoveClipboardFieldAt(int visibleRow, const QString& fieldName);
    bool testClipboardRowTransactionHighlighted(int visibleRow) const;
    bool testClickClipboardRow(int visibleRow);
    bool testEditClipboardTimestampAt(int visibleRow, qint64 timestamp);
    void testMaterializeClipboardVisiblePage();
    void testActivateClipboardRow(int visibleRow);
    bool testInsertSelectedFlitToClipboard(ClipboardScope scope);
    bool testInsertSelectedXactionToClipboard(ClipboardScope scope);
    bool testInsertAllXactionsWithSelectedAddressToClipboard(ClipboardScope scope);
    bool testInsertLaterXactionsWithSelectedAddressToClipboard(ClipboardScope scope);
    bool testInsertThisAndLaterXactionsWithSelectedAddressToClipboard(ClipboardScope scope);
    bool testClipboardXactionAddressInsertActive() const noexcept;
    bool testClipboardInsertProgressVisible() const noexcept;
    bool testClipboardInsertCancelVisible() const noexcept;
    bool testCancelClipboardXactionAddressInsert();
    bool testDeleteClipboardRow(int visibleRow);
    bool testSaveClipboardCsv(const QString& path);
    bool testSaveClipboardCLogB(const QString& path);
    bool testClipboardCLogBSaveAvailable() const;
    void testSetClipboardOpcodeFilter(const QString& text);
    void testSetClipboardSearchMode(FlitTableModel::SearchMode mode);
    void testSetTraceCacheMinimapVisible(bool visible);
    bool testTraceCacheMinimapVisible() const noexcept;
    QRect testTraceCacheMinimapGeometry() const;
    bool testAddTraceCacheMinimapLane(std::uint32_t rnNodeId, std::uint64_t address);
    int testTraceCacheMinimapLaneCount() const noexcept;
    int testTraceCacheMinimapSegmentCount(int laneIndex) const noexcept;
    QVariantMap testTraceCacheMinimapLaneAt(int laneIndex) const;
    QVariantMap testTraceCacheMinimapSegmentAt(int laneIndex, int segmentIndex) const;
    QRect testTraceCacheMinimapLaneRect(int laneIndex) const;
    QRect testTraceCacheMinimapTagRect(int laneIndex) const;
    bool testTraceCacheMinimapJumpAvailable(int laneIndex,
                                            int state,
                                            int direction,
                                            int referenceRow) const;
    bool testTriggerTraceCacheMinimapJump(int laneIndex,
                                          int state,
                                          int direction,
                                          int referenceRow);
    bool testTraceCacheMinimapChangeJumpAvailable(int laneIndex, int direction, int referenceRow) const;
    bool testTriggerTraceCacheMinimapChangeJump(int laneIndex, int direction, int referenceRow);
    void testRemoveTraceCacheMinimapLane(int laneIndex);
    void testSetClipboardCacheMinimapVisible(bool visible);
    bool testClipboardCacheMinimapVisible() const noexcept;
    bool testAddClipboardCacheMinimapLane(std::uint32_t rnNodeId, std::uint64_t address);
    int testClipboardCacheMinimapLaneCount() const noexcept;
    int testClipboardCacheMinimapSegmentCount(int laneIndex) const noexcept;
    QVariantMap testClipboardCacheMinimapSegmentAt(int laneIndex, int segmentIndex) const;
    bool testClipboardCacheMinimapJumpAvailable(int laneIndex,
                                                int state,
                                                int direction,
                                                int referenceRow) const;
    bool testTriggerClipboardCacheMinimapJump(int laneIndex,
                                              int state,
                                              int direction,
                                              int referenceRow);
    bool testClipboardCacheMinimapChangeJumpAvailable(int laneIndex, int direction, int referenceRow) const;
    bool testTriggerClipboardCacheMinimapChangeJump(int laneIndex, int direction, int referenceRow);
    int testLatencyDiffSessionCount() const noexcept;
    bool testBuildDefaultLatencyDiff();
    bool testLatencyDiffHasReport() const noexcept;
#endif

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
#include "main_window_members.hpp"

private:
#include "main_window_methods.hpp"
};

}  // namespace CHIron::Gui
