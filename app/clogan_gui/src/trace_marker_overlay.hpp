#pragma once

#include "marker_store.hpp"

#include <QColor>
#include <QPointer>
#include <QPolygon>
#include <QRect>
#include <QString>
#include <QWidget>

#include <vector>

class QAbstractItemModel;
class QEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QTableView;

namespace CHIron::Gui {

class FlitTableModel;
class TraceCacheLineMinimap;

class TraceMarkerOverlay final : public QWidget {
    Q_OBJECT

public:
    explicit TraceMarkerOverlay(QTableView* table, QWidget* parent = nullptr);
    ~TraceMarkerOverlay() override;

    void setFlitModel(FlitTableModel* model);
    void setCacheMinimap(TraceCacheLineMinimap* minimap);
    void setMarkers(std::vector<TraceMarker> markers, const QString& selectedMarkerId);
    void clear();
    void ensureOnTop();
    void cancelMarkerMove();
    void finishMarkerMove();

Q_SIGNALS:
    void logicalRowActivated(int logicalRow);
    void markerSelected(QString markerId);
    void markerSelectRequested(QString markerId);
    void markerEditRequested(QString markerId);
    void markerRemoveRequested(QString markerId);
    void markerMoveStarted(QString markerId);
    void markerMoveDropped(QString markerId, int logicalRow);
    void markerMoveCancelled(QString markerId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
public:
    bool testOverlayVisible() const noexcept { return isVisible(); }
    QRect testOverlayGeometry() const noexcept { return geometry(); }
    QRect testMinimapGeometry() const;
    QRect testCollapsedMarkerTagGeometry(int markerIndex) const;
    QRect testExpandedMarkerTagGeometry(int markerIndex) const;
    QRect testLeftMarkerOverlayGeometry() const;
    QRect testLeftMarkerPolygonGeometry(int markerIndex) const;
    QRect testLeftMarkerNameGeometry(int markerIndex) const;
    bool testClickLeftMarkerPolygon(int markerIndex);
    bool testDoubleClickLeftMarkerPolygon(int markerIndex);
    bool testStartMarkerMoveFromTag(int markerIndex);
    bool testStartMarkerMoveFromLeftPolygon(int markerIndex);
    bool testDropMarkerMoveOnLogicalRow(int logicalRow);
    void testCancelMarkerMove();
    bool testMarkerMoveActive() const noexcept { return markerMoveActive_; }
    int testMarkerCount() const noexcept { return static_cast<int>(markers_.size()); }
#endif

private:
    class RowMarkerOverlay;
    class MoveOverlay;

    struct PaintMarker {
        const TraceMarker* marker = nullptr;
        QRect bodyRect;
        bool active = false;
    };

    void updateOverlayGeometry();
    void updateModelConnections();
    void refresh();
    bool hasContent() const noexcept;
    bool hasExpandedMarker() const;
    void showMarkerContextMenu(const QString& markerId, const QPoint& globalPosition);
    void startMarkerMove(const QString& markerId);
    bool dropMarkerMoveAtViewportPosition(const QPoint& viewportPosition);
    void cancelMarkerMoveInternal(bool emitSignal);
    QRect moveOverlayRectForCurrentTable() const;
    QRect moveGhostRect(const QPoint& position) const;
    QPolygon moveGhostPolygon(const QRect& ghostRect) const;
    QRect overlayRectForCurrentTable() const;
    QRect rowOverlayRectForCurrentTable() const;
    QRect markerViewportRowRect(const TraceMarker& marker) const;
    QRect minimapRect() const;
    QRect viewportThumbRect() const;
    int visibleRowForY(int y) const;
    int globalYForVisibleRow(int visibleRow) const;
    int visibleRowForLogicalRow(int logicalRow) const;
    int markerCenterY(const TraceMarker& marker) const;
    QRect markerTagBodyRect(const TraceMarker& marker, bool expanded) const;
    QRect markerTagRect(const TraceMarker& marker, bool expanded) const;
    QRect markerHitRect(const TraceMarker& marker) const;
    QPolygon markerTagPolygon(const QRect& bodyRect) const;
    const TraceMarker* markerAtIndex(int markerIndex) const;
    const TraceMarker* markerAt(const QPoint& position) const;
    QRect parentRect(const QRect& localRect) const;

    QPointer<QTableView> table_;
    QPointer<FlitTableModel> model_;
    QPointer<TraceCacheLineMinimap> cacheMinimap_;
    RowMarkerOverlay* rowOverlay_ = nullptr;
    MoveOverlay* moveOverlay_ = nullptr;
    std::vector<TraceMarker> markers_;
    QString selectedMarkerId_;
    QString hoveredMarkerId_;
    QString movingMarkerId_;
    QPoint movingCursorPos_;
    bool markerMoveActive_ = false;
    bool moveEventFilterInstalled_ = false;
    QMetaObject::Connection modelResetConnection_;
    QMetaObject::Connection rowsInsertedConnection_;
    QMetaObject::Connection rowsRemovedConnection_;
    QMetaObject::Connection dataChangedConnection_;
};

}  // namespace CHIron::Gui
