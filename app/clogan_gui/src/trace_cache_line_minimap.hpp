#pragma once

#include "clipboard_widget.hpp"
#include "clog_b_trace_loader.hpp"
#include "flit_table_model.hpp"
#include "trace_session.hpp"

#include <QColor>
#include <QHash>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVariantMap>
#include <QWidget>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

class QAbstractItemModel;
class QEvent;
class QFocusEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QTableView;

namespace CHIron::Gui {

class TraceCacheLineMinimap final : public QWidget {
    Q_OBJECT

public:
    enum class HostMode {
        MainTrace,
        Clipboard
    };

    enum class JumpState {
        UC = 0,
        UCE,
        UD,
        UDP,
        SC,
        SD
    };

    enum class JumpDirection {
        First = 0,
        Previous,
        Next,
        Last
    };

    struct RnNodeChoice {
        std::uint32_t nodeId = 0;
        QString typeText;
        QString label;
    };

    explicit TraceCacheLineMinimap(QTableView* table, HostMode mode, QWidget* parent = nullptr);
    ~TraceCacheLineMinimap() override;

    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setFlitModel(FlitTableModel* model);
    void setClipboardContext(ClipboardWidget* clipboard,
                             std::function<std::shared_ptr<TraceSession>(quint64)> sourceSessionResolver);
    void clearSource();
    void setModelUpdatesSuspended(bool suspended);

    bool mapVisible() const noexcept;
    void setMapVisible(bool visible);
    int rightReservedWidth() const noexcept;
    void setRightReservedWidth(int width);
    QRect overlayGeometry() const noexcept { return geometry(); }
    bool fadeWhenInactive() const noexcept;
    void setFadeWhenInactive(bool enabled);

    bool addLane(std::uint32_t rnNodeId, std::uint64_t address);
    std::vector<RnNodeChoice> availableRnNodeChoices() const;
    void clearLanes();
    int laneCount() const noexcept;
    QString statusText() const;

    static bool parseIntegerText(QString text, std::uint64_t& value);

Q_SIGNALS:
    void logicalRowActivated(int logicalRow);
    void clipboardRowActivated(int visibleRow);
    void statusMessageRequested(const QString& text, int timeoutMs);
    void mapVisibilityChanged(bool visible);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

#ifdef CHIRON_GUI_ENABLE_TRACE_CACHE_MINIMAP_TEST_API
public:
    QRect testOverlayGeometry() const noexcept { return geometry(); }
    bool testMapVisible() const noexcept { return mapVisible_; }
    bool testFadeWhenInactive() const noexcept { return fadeWhenInactive_; }
    bool testHovered() const noexcept { return hovered_; }
    int testLaneCount() const noexcept { return laneCount(); }
    int testSegmentCount(int laneIndex) const noexcept;
    QVariantMap testLaneAt(int laneIndex) const;
    QVariantMap testSegmentAt(int laneIndex, int segmentIndex) const;
    QRect testLaneRect(int laneIndex) const;
    QRect testTagRect(int laneIndex) const;
    int testHitRowAt(const QPoint& position) const;
    bool testJumpActionAvailable(int laneIndex,
                                 JumpState state,
                                 JumpDirection direction,
                                 int referenceRow) const;
    bool testTriggerJumpAction(int laneIndex,
                               JumpState state,
                               JumpDirection direction,
                               int referenceRow);
    bool testChangeJumpActionAvailable(int laneIndex, int direction, int referenceRow) const;
    bool testTriggerChangeJumpAction(int laneIndex, int direction, int referenceRow);
    void testRemoveLaneAt(int laneIndex) { removeLaneAt(laneIndex); }
#endif

private:
    enum class ChangeJumpDirection {
        First = 0,
        Previous,
        Next,
        Last
    };

    struct Segment {
        quint64 sourceSessionId = 0;
        std::uint64_t sourceStartLogicalRow = 0;
        std::uint64_t sourceEndLogicalRow = 0;
        int startVisibleRow = -1;
        int endVisibleRow = -1;
        qint64 startTimestamp = 0;
        qint64 endTimestamp = 0;
        std::uint8_t stateMask = 0;
        QString stateText;
        QColor color;
        bool openEnded = false;
        bool splitBefore = false;
        QRect rect;
    };

    struct ChangeJumpTarget {
        const Segment* segment = nullptr;
        int visibleRow = -1;
        std::uint64_t sourceLogicalRow = 0;
    };

    struct SourceSpanGroup {
        quint64 sessionId = 0;
        std::vector<CLogBTraceCacheLineStateSpan> spans;
    };

    struct Lane {
        std::uint32_t rnNodeId = 0;
        std::uint64_t lineAddress = 0;
        QString rnNodeType;
        QString statusText;
        bool building = false;
        bool failed = false;
        bool hasBuildResult = false;
        quint64 generation = 0;
        std::vector<CLogBTraceCacheLineStateSpan> sourceSpans;
        std::vector<SourceSpanGroup> sourceSpanGroups;
        std::vector<Segment> segments;
        QRect laneRect;
        QRect tagRect;
    };

    struct BuildResult {
        quint64 generation = 0;
        quint64 sourceSessionId = 0;
        std::uint32_t rnNodeId = 0;
        std::uint64_t lineAddress = 0;
        std::vector<CLogBTraceCacheLineStateSpan> spans;
        QString statusText;
        bool failed = false;
        bool cancelled = false;
    };

    void stopBuildThread();
    void updateOverlayGeometry();
    void updateModelConnections();
    void startLaneBuild(int laneIndex);
    void startAllLaneBuilds(bool onlyPending = false);
    void startClipboardLaneBuild(int laneIndex);
    void startAllClipboardLaneBuilds(bool onlyPending = false);
    void applyBuildResult(std::shared_ptr<BuildResult> result);
    void applyBuildResults(std::vector<std::shared_ptr<BuildResult>> results);
    std::shared_ptr<BuildResult> buildLane(std::shared_ptr<TraceSession> session,
                                           quint64 generation,
                                           std::uint32_t rnNodeId,
                                           std::uint64_t lineAddress,
                                           std::stop_token stopToken,
                                           const std::function<void(std::size_t completedRows,
                                                                    std::size_t totalRows)>& progressCallback) const;

    void rebuildVisibleSegments();
    std::vector<CLogBTraceCacheLineStateSpan> coalescedSourceSpans(
        const std::vector<CLogBTraceCacheLineStateSpan>& spans) const;
    void rebuildMainTraceLane(Lane& lane) const;
    void rebuildClipboardLane(Lane& lane) const;
    std::optional<int> visibleRowForSourceRow(const Lane& lane, std::uint64_t sourceLogicalRow) const;
    int sourceLogicalRowForClipboardVisibleRow(int visibleRow) const;
    int laneIndexAt(const QPoint& position) const;
    int tagIndexAt(const QPoint& position) const;
    int rowAtPositionForJump(const QPoint& position) const;
    int segmentRowAt(const QPoint& position) const;
    void removeLaneAt(int laneIndex);
    void activateSegmentAt(const QPoint& position);
    void showLaneContextMenu(int laneIndex, const QPoint& position, const QPoint& globalPosition);
    const Segment* findJumpSegment(int laneIndex,
                                   JumpState state,
                                   JumpDirection direction,
                                   int referenceRow) const;
    std::vector<ChangeJumpTarget> changeJumpTargetsForLane(int laneIndex) const;
    std::optional<ChangeJumpTarget> findChangeJumpTarget(int laneIndex,
                                                         ChangeJumpDirection direction,
                                                         int referenceRow) const;
    bool jumpToSegment(const Segment& segment);
    bool jumpToChangeTarget(const ChangeJumpTarget& target);
    QRect overlayRectForCurrentTable() const;
    QRect laneRectForIndex(int laneIndex, int totalLanes) const;
    QRect tagRectForIndex(int laneIndex, const QRect& laneRect) const;
    QRect buildProgressRect() const;
    int rowY(int visibleRow) const;
    int rowBottomY(int visibleRow) const;
    QColor colorForState(std::uint8_t stateMask, const QString& stateText, bool openEnded) const;
    QString laneTagText(const Lane& lane) const;
    void requestStatus(const QString& text, int timeoutMs = 3000);
    void beginBuildProgress(quint64 generation, std::uint64_t totalRows);
    void postBuildProgress(quint64 generation, std::uint64_t completedRows, std::uint64_t totalRows);
    void updateBuildProgress(quint64 generation, std::uint64_t completedRows, std::uint64_t totalRows);
    void finishBuildProgress(quint64 generation);
    void resetBuildProgress();
    void paintBuildProgress(QPainter& painter) const;

private:
    QPointer<QTableView> table_;
    HostMode hostMode_ = HostMode::MainTrace;
    QPointer<FlitTableModel> model_;
    QPointer<ClipboardWidget> clipboard_;
    std::function<std::shared_ptr<TraceSession>(quint64)> sourceSessionResolver_;
    std::shared_ptr<TraceSession> traceSession_;
    std::vector<Lane> lanes_;
    std::jthread buildThread_;
    std::atomic<quint64> generation_{0};
    QMetaObject::Connection modelResetConnection_;
    QMetaObject::Connection rowsInsertedConnection_;
    QMetaObject::Connection rowsRemovedConnection_;
    QMetaObject::Connection dataChangedConnection_;
    int modelUpdatesSuspended_ = 0;
    bool pendingModelRefresh_ = false;
    bool mapVisible_ = false;
    bool fadeWhenInactive_ = true;
    int rightReservedWidth_ = 0;
    bool hovered_ = false;
    bool focused_ = false;
    QString statusText_;
    bool buildProgressActive_ = false;
    quint64 buildProgressGeneration_ = 0;
    std::uint64_t buildProgressCompletedRows_ = 0;
    std::uint64_t buildProgressTotalRows_ = 0;
    std::atomic_bool progressUpdateQueued_{false};
    std::atomic<quint64> pendingProgressGeneration_{0};
    std::atomic<std::uint64_t> pendingProgressCompletedRows_{0};
    std::atomic<std::uint64_t> pendingProgressTotalRows_{0};
};

}  // namespace CHIron::Gui
