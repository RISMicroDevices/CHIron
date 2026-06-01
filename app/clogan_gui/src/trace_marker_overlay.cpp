#include "trace_marker_overlay.hpp"

#include "flit_table_model.hpp"
#include "trace_cache_line_minimap.hpp"

#include <QAbstractItemModel>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRegion>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace CHIron::Gui {
namespace {

constexpr int kOverlayWidth = 16;
constexpr int kExpandedOverlayWidth = 138;
constexpr int kMinimapWidth = 4;
constexpr int kCollapsedTagWidth = 6;
constexpr int kExpandedTagWidth = 122;
constexpr int kCollapsedTagHeight = 7;
constexpr int kExpandedTagHeight = 14;
constexpr int kCollapsedPointerWidth = 5;
constexpr int kExpandedPointerWidth = 10;
constexpr int kMinimumThumbHeight = 8;
constexpr int kRowMarkerOverlayWidth = 16;
constexpr int kRowMarkerTipWidth = 7;
constexpr int kRowMarkerMinimumHeight = 9;
constexpr int kRowMarkerMaximumHeight = 18;
constexpr int kRowMarkerMinimumNameWidth = 28;
constexpr int kRowMarkerNamePadding = 6;
constexpr int kMoveGhostWidth = 22;
constexpr int kMoveGhostHeight = 16;
constexpr int kMoveGhostTipWidth = 10;

QColor withAlpha(QColor color, const int alpha)
{
    color.setAlpha(alpha);
    return color;
}

}  // namespace

class TraceMarkerOverlay::RowMarkerOverlay final : public QWidget {
public:
    explicit RowMarkerOverlay(TraceMarkerOverlay* owner, QWidget* parent)
        : QWidget(parent)
        , owner_(owner)
    {
        setObjectName(QStringLiteral("traceRowMarkerOverlay"));
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMouseTracking(true);
        hide();
    }

    void refreshGeometry()
    {
        if (!owner_) {
            hide();
            return;
        }
        const QRect target = owner_->rowOverlayRectForCurrentTable();
        if (geometry() != target) {
            setGeometry(target);
        }
        setVisible(owner_->hasContent() && target.isValid() && owner_->table_ && owner_->table_->isVisible());
        updateInputMask();
        if (isVisible()) {
            raise();
        }
    }

    void refreshMarkers()
    {
        if (owner_) {
            QSet<QString> liveMarkerIds;
            for (const TraceMarker& marker : owner_->markers_) {
                liveMarkerIds.insert(marker.id);
            }
            for (auto it = expandedMarkerIds_.begin(); it != expandedMarkerIds_.end();) {
                if (liveMarkerIds.contains(*it)) {
                    ++it;
                } else {
                    it = expandedMarkerIds_.erase(it);
                }
            }
            refreshGeometry();
        }
        updateInputMask();
        update();
    }

    QRect markerPolygonRect(const TraceMarker& marker) const
    {
        if (!owner_) {
            return {};
        }
        const QRect rowRect = owner_->markerViewportRowRect(marker);
        if (!rowRect.isValid()) {
            return {};
        }
        const int markerHeight = qBound(kRowMarkerMinimumHeight,
                                        rowRect.height() - 4,
                                        qMin(kRowMarkerMaximumHeight, qMax(kRowMarkerMinimumHeight, height())));
        const int centerY = rowRect.center().y();
        return QRect(0,
                     centerY - (markerHeight - 1) / 2,
                     qMin(kRowMarkerOverlayWidth, width()),
                     markerHeight);
    }

    QRect markerNameRect(const TraceMarker& marker) const
    {
        if (!expandedMarkerIds_.contains(marker.id)) {
            return {};
        }
        const QRect markerRect = markerPolygonRect(marker);
        if (!markerRect.isValid()) {
            return {};
        }
        const QString text = marker.label.isEmpty()
            ? QStringLiteral("Marker %1").arg(marker.logicalRow + 1)
            : marker.label;
        QFont labelFont = font();
        labelFont.setBold(true);
        const QFontMetrics metrics(labelFont);
        const int fullLabelWidth = qMax(kRowMarkerMinimumNameWidth,
                                        metrics.horizontalAdvance(text) + kRowMarkerNamePadding * 2);
        const int availableLabelWidth = qMax(0, width() - markerRect.right() - 1);
        const int labelWidth = qMin(fullLabelWidth, availableLabelWidth);
        const int labelHeight = qMax(markerRect.height(), metrics.height() + 4);
        if (labelWidth <= 0) {
            return {};
        }
        return QRect(markerRect.right(),
                     markerRect.center().y() - (labelHeight - 1) / 2,
                     labelWidth,
                     labelHeight).intersected(rect());
    }

    QRect parentMarkerNameRect(const TraceMarker& marker) const
    {
        const QRect localRect = markerNameRect(marker);
        return localRect.isValid() ? QRect(mapToParent(localRect.topLeft()), localRect.size()) : QRect();
    }

    QRect parentMarkerPolygonRect(const TraceMarker& marker) const
    {
        const QRect localRect = markerPolygonRect(marker);
        return localRect.isValid() ? QRect(mapToParent(localRect.topLeft()), localRect.size()) : QRect();
    }

    const TraceMarker* markerAtIndex(const int markerIndex) const
    {
        if (!owner_ || markerIndex < 0 || markerIndex >= static_cast<int>(owner_->markers_.size())) {
            return nullptr;
        }
        return &owner_->markers_[static_cast<std::size_t>(markerIndex)];
    }

    bool hasExpandedVisibleMarker() const
    {
        if (!owner_ || expandedMarkerIds_.isEmpty()) {
            return false;
        }
        return std::any_of(owner_->markers_.cbegin(), owner_->markers_.cend(), [this](const TraceMarker& marker) {
            return expandedMarkerIds_.contains(marker.id) && markerPolygonRect(marker).isValid();
        });
    }

    int preferredOverlayWidth(const int maximumWidth) const
    {
        const int boundedMaximum = qMax(kRowMarkerOverlayWidth, maximumWidth);
        if (!owner_ || expandedMarkerIds_.isEmpty()) {
            return qMin(kRowMarkerOverlayWidth, boundedMaximum);
        }

        QFont labelFont = font();
        labelFont.setBold(true);
        const QFontMetrics metrics(labelFont);
        int preferredWidth = kRowMarkerOverlayWidth;
        for (const TraceMarker& marker : owner_->markers_) {
            if (!expandedMarkerIds_.contains(marker.id) || !owner_->markerViewportRowRect(marker).isValid()) {
                continue;
            }
            const QString text = marker.label.isEmpty()
                ? QStringLiteral("Marker %1").arg(marker.logicalRow + 1)
                : marker.label;
            const int labelWidth = qMax(kRowMarkerMinimumNameWidth,
                                        metrics.horizontalAdvance(text) + kRowMarkerNamePadding * 2);
            preferredWidth = qMax(preferredWidth, kRowMarkerOverlayWidth + labelWidth);
        }
        return qBound(kRowMarkerOverlayWidth, preferredWidth, boundedMaximum);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!owner_ || !owner_->hasContent()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        std::vector<const TraceMarker*> activeMarkers;
        activeMarkers.reserve(owner_->markers_.size());

        auto drawMarker = [&](const TraceMarker& marker) {
            const QRect markerRect = markerPolygonRect(marker);
            if (!markerRect.isValid()) {
                return;
            }
            const bool hovered = marker.id == hoveredMarkerId_;
            const bool selected = marker.id == owner_->selectedMarkerId_;
            const QColor markerColor = marker.color.isValid() ? marker.color : DefaultTraceMarkerColor(marker.logicalRow);
            const QColor fill = withAlpha(markerColor, hovered ? 230 : (selected ? 174 : 112));
            const QColor border = withAlpha(markerColor.darker(120), hovered ? 242 : (selected ? 198 : 132));

            const QRect labelRect = markerNameRect(marker);
            if (labelRect.isValid()) {
                QFont labelFont = font();
                labelFont.setBold(true);
                painter.setFont(labelFont);
                const QFontMetrics metrics(labelFont);
                painter.setPen(QPen(border, 1));
                painter.setBrush(fill);
                painter.drawPolygon(expandedMarkerPolygon(markerRect, labelRect));
                const QString text = marker.label.isEmpty()
                    ? QStringLiteral("Marker %1").arg(marker.logicalRow + 1)
                    : marker.label;
                painter.setPen(withAlpha(QColor(QStringLiteral("#0F172A")), hovered ? 244 : 216));
                painter.drawText(labelRect.adjusted(kRowMarkerNamePadding, 0, -kRowMarkerNamePadding, 0),
                                 Qt::AlignVCenter | Qt::AlignLeft,
                                 metrics.elidedText(text,
                                                    Qt::ElideRight,
                                                    qMax(1, labelRect.width() - kRowMarkerNamePadding * 2)));
            } else {
                painter.setPen(QPen(border, 1));
                painter.setBrush(fill);
                painter.drawPolygon(markerPolygon(markerRect));
            }
        };

        for (const TraceMarker& marker : owner_->markers_) {
            if (marker.id == hoveredMarkerId_ || marker.id == owner_->selectedMarkerId_) {
                activeMarkers.push_back(&marker);
            } else {
                drawMarker(marker);
            }
        }
        for (const TraceMarker* marker : activeMarkers) {
            if (marker) {
                drawMarker(*marker);
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (!owner_) {
            QWidget::mousePressEvent(event);
            return;
        }
        const TraceMarker* marker = markerAt(event->pos());
        if (!marker) {
            QWidget::mousePressEvent(event);
            return;
        }

        if (event->button() == Qt::LeftButton) {
            const QString markerId = marker->id;
            owner_->selectedMarkerId_ = markerId;
            Q_EMIT owner_->markerSelectRequested(markerId);
            update();
            event->accept();
            return;
        }

        if (event->button() == Qt::RightButton) {
            const QString markerId = marker->id;
            owner_->selectedMarkerId_ = markerId;
            Q_EMIT owner_->markerSelected(markerId);
            owner_->showMarkerContextMenu(markerId, event->globalPosition().toPoint());
            update();
            event->accept();
            return;
        }

        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        const TraceMarker* marker = markerAt(event->pos());
        if (!owner_ || !marker || event->button() != Qt::LeftButton) {
            QWidget::mouseDoubleClickEvent(event);
            return;
        }

        if (expandedMarkerIds_.contains(marker->id)) {
            expandedMarkerIds_.remove(marker->id);
        } else {
            expandedMarkerIds_.insert(marker->id);
        }
        refreshGeometry();
        update();
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        QString hoveredMarker;
        if (const TraceMarker* marker = markerAt(event->pos())) {
            hoveredMarker = marker->id;
            QToolTip::showText(event->globalPosition().toPoint(),
                               QStringLiteral("%1 - row %2")
                                   .arg(marker->label)
                                   .arg(marker->logicalRow + 1),
                               this);
        } else {
            QToolTip::hideText();
        }
        if (hoveredMarker != hoveredMarkerId_) {
            hoveredMarkerId_ = hoveredMarker;
            update();
        }
    }

    void leaveEvent(QEvent* event) override
    {
        hoveredMarkerId_.clear();
        QToolTip::hideText();
        update();
        QWidget::leaveEvent(event);
    }

private:
    QPolygon markerPolygon(const QRect& markerRect) const
    {
        if (!markerRect.isValid()) {
            return {};
        }
        const int left = markerRect.left();
        const int right = markerRect.right();
        const int tipY = markerRect.center().y();
        const int shoulderX = qMin(right, left + kRowMarkerTipWidth);
        const int bodyTop = markerRect.top() + 1;
        const int bodyBottom = markerRect.bottom() - 1;
        const int chamfer = qMin(3, qMax(1, markerRect.height() / 4));
        return QPolygon({
            QPoint(left, tipY),
            QPoint(shoulderX, bodyTop),
            QPoint(right - chamfer, bodyTop),
            QPoint(right, bodyTop + chamfer),
            QPoint(right, bodyBottom - chamfer),
            QPoint(right - chamfer, bodyBottom),
            QPoint(shoulderX, bodyBottom),
        });
    }

    QPolygon expandedMarkerPolygon(const QRect& markerRect, const QRect& labelRect) const
    {
        if (!markerRect.isValid() || !labelRect.isValid()) {
            return markerPolygon(markerRect);
        }
        const int left = markerRect.left();
        const int tipY = markerRect.center().y();
        const int shoulderX = qMin(markerRect.right(), left + kRowMarkerTipWidth);
        const int top = qMin(markerRect.top() + 1, labelRect.top());
        const int bottom = qMax(markerRect.bottom() - 1, labelRect.bottom());
        const int right = labelRect.right();
        const int chamfer = qMin(3, qMax(1, (bottom - top + 1) / 5));
        return QPolygon({
            QPoint(left, tipY),
            QPoint(shoulderX, top),
            QPoint(right - chamfer, top),
            QPoint(right, top + chamfer),
            QPoint(right, bottom - chamfer),
            QPoint(right - chamfer, bottom),
            QPoint(shoulderX, bottom),
        });
    }

    const TraceMarker* markerAt(const QPoint& position) const
    {
        if (!owner_) {
            return nullptr;
        }
        for (auto it = owner_->markers_.crbegin(); it != owner_->markers_.crend(); ++it) {
            const QRect hitRect = (markerPolygonRect(*it) | markerNameRect(*it)).adjusted(-2, -3, 2, 3).intersected(rect());
            if (hitRect.contains(position)) {
                return &*it;
            }
        }
        return nullptr;
    }

    void updateInputMask()
    {
        if (!owner_ || !owner_->hasContent()) {
            clearMask();
            return;
        }
        QRegion mask;
        for (const TraceMarker& marker : owner_->markers_) {
            const QRect hitRect = (markerPolygonRect(marker) | markerNameRect(marker)).adjusted(-2, -3, 2, 3).intersected(rect());
            if (hitRect.isValid()) {
                mask += QRegion(hitRect);
            }
        }
        setMask(mask);
    }

    TraceMarkerOverlay* owner_ = nullptr;
    QString hoveredMarkerId_;
    QSet<QString> expandedMarkerIds_;
};

class TraceMarkerOverlay::MoveOverlay final : public QWidget {
public:
    explicit MoveOverlay(TraceMarkerOverlay* owner, QWidget* parent)
        : QWidget(parent)
        , owner_(owner)
    {
        setObjectName(QStringLiteral("traceMarkerMoveOverlay"));
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        hide();
    }

    void refreshGeometry()
    {
        if (!owner_) {
            hide();
            return;
        }
        const QRect target = owner_->moveOverlayRectForCurrentTable();
        if (geometry() != target) {
            setGeometry(target);
        }
        setVisible(owner_->markerMoveActive_ && target.isValid());
        if (isVisible()) {
            raise();
            setFocus(Qt::OtherFocusReason);
        }
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!owner_ || !owner_->markerMoveActive_) {
            return;
        }
        const TraceMarker* marker = nullptr;
        for (const TraceMarker& candidate : owner_->markers_) {
            if (candidate.id == owner_->movingMarkerId_) {
                marker = &candidate;
                break;
            }
        }
        if (!marker) {
            return;
        }
        const QRect ghostRect = owner_->moveGhostRect(owner_->movingCursorPos_);
        if (!ghostRect.isValid()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor markerColor = marker->color.isValid()
            ? marker->color
            : DefaultTraceMarkerColor(marker->logicalRow);
        painter.setPen(QPen(withAlpha(markerColor.darker(125), 190), 1));
        painter.setBrush(withAlpha(markerColor, 146));
        painter.drawPolygon(owner_->moveGhostPolygon(ghostRect));
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (owner_) {
            owner_->movingCursorPos_ = event->pos();
            update();
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (!owner_) {
            QWidget::mousePressEvent(event);
            return;
        }
        if (event->button() == Qt::LeftButton) {
            owner_->movingCursorPos_ = event->pos();
            if (owner_->dropMarkerMoveAtViewportPosition(event->pos())) {
                event->accept();
                return;
            }
            update();
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton) {
            owner_->cancelMarkerMove();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (owner_ && event->key() == Qt::Key_Escape) {
            owner_->cancelMarkerMove();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        if (owner_ && owner_->markerMoveActive_) {
            QWidget* focusWidget = QApplication::focusWidget();
            const bool stillInTable = focusWidget == this
                || focusWidget == owner_->table_
                || (owner_->table_ && focusWidget == owner_->table_->viewport());
            if (!stillInTable) {
                owner_->cancelMarkerMove();
            }
        }
        QWidget::focusOutEvent(event);
    }

private:
    TraceMarkerOverlay* owner_ = nullptr;
};

TraceMarkerOverlay::TraceMarkerOverlay(QTableView* table, QWidget* parent)
    : QWidget(parent ? parent : (table ? table->parentWidget() : nullptr))
    , table_(table)
{
    setObjectName(QStringLiteral("traceMarkerOverlay"));
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);
    hide();

    rowOverlay_ = new RowMarkerOverlay(this, parentWidget());
    moveOverlay_ = new MoveOverlay(this, parentWidget());
    connect(rowOverlay_, &QObject::destroyed, this, [this]() {
        rowOverlay_ = nullptr;
    });
    connect(moveOverlay_, &QObject::destroyed, this, [this]() {
        moveOverlay_ = nullptr;
    });
    if (table_) {
        if (!parent && table_->parentWidget()) {
            setParent(table_->parentWidget());
            rowOverlay_->setParent(table_->parentWidget());
            moveOverlay_->setParent(table_->parentWidget());
        }
        table_->installEventFilter(this);
        table_->viewport()->installEventFilter(this);
        if (table_->verticalScrollBar()) {
            table_->verticalScrollBar()->installEventFilter(this);
            connect(table_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
                if (rowOverlay_) {
                    rowOverlay_->refreshMarkers();
                }
                update();
            });
            connect(table_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this]() {
                updateOverlayGeometry();
                update();
            });
        }
        updateOverlayGeometry();
    }
}

TraceMarkerOverlay::~TraceMarkerOverlay()
{
    if (moveEventFilterInstalled_) {
        qApp->removeEventFilter(this);
        moveEventFilterInstalled_ = false;
    }
    delete moveOverlay_;
    delete rowOverlay_;
}

void TraceMarkerOverlay::setFlitModel(FlitTableModel* model)
{
    if (model_ == model) {
        return;
    }
    model_ = model;
    updateModelConnections();
    refresh();
}

void TraceMarkerOverlay::setCacheMinimap(TraceCacheLineMinimap* minimap)
{
    if (cacheMinimap_ == minimap) {
        return;
    }
    if (cacheMinimap_) {
        cacheMinimap_->removeEventFilter(this);
    }
    cacheMinimap_ = minimap;
    if (cacheMinimap_) {
        cacheMinimap_->installEventFilter(this);
    }
    refresh();
}

void TraceMarkerOverlay::setMarkers(std::vector<TraceMarker> markers, const QString& selectedMarkerId)
{
    markers_ = std::move(markers);
    selectedMarkerId_ = selectedMarkerId;
    if (!selectedMarkerId_.isEmpty()) {
        const auto found = std::find_if(markers_.cbegin(), markers_.cend(), [&](const TraceMarker& marker) {
            return marker.id == selectedMarkerId_;
        });
        if (found == markers_.cend()) {
            selectedMarkerId_.clear();
        }
    }
    if (!hoveredMarkerId_.isEmpty()) {
        const auto found = std::find_if(markers_.cbegin(), markers_.cend(), [&](const TraceMarker& marker) {
            return marker.id == hoveredMarkerId_;
        });
        if (found == markers_.cend()) {
            hoveredMarkerId_.clear();
        }
    }
    if (markerMoveActive_) {
        const auto found = std::find_if(markers_.cbegin(), markers_.cend(), [&](const TraceMarker& marker) {
            return marker.id == movingMarkerId_;
        });
        if (found == markers_.cend()) {
            cancelMarkerMoveInternal(true);
        }
    }
    refresh();
}

void TraceMarkerOverlay::clear()
{
    cancelMarkerMoveInternal(false);
    markers_.clear();
    selectedMarkerId_.clear();
    hoveredMarkerId_.clear();
    refresh();
}

void TraceMarkerOverlay::ensureOnTop()
{
    if (rowOverlay_ && rowOverlay_->isVisible()) {
        rowOverlay_->raise();
    }
    if (isVisible()) {
        raise();
    }
    if (moveOverlay_ && moveOverlay_->isVisible()) {
        moveOverlay_->raise();
    }
}

bool TraceMarkerOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (!table_) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == table_ || watched == table_->viewport() || watched == table_->verticalScrollBar()
        || watched == cacheMinimap_) {
        switch (event->type()) {
        case QEvent::Hide:
            cancelMarkerMove();
            [[fallthrough]];
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::LayoutRequest:
            updateOverlayGeometry();
            update();
            QTimer::singleShot(0, this, [this]() {
                ensureOnTop();
            });
            break;
        default:
            break;
        }
    }
    if (markerMoveActive_
        && (event->type() == QEvent::WindowDeactivate
            || event->type() == QEvent::ApplicationDeactivate
            || event->type() == QEvent::FocusOut)) {
        if (watched == qApp || watched == table_ || watched == table_->viewport()) {
            cancelMarkerMove();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TraceMarkerOverlay::paintEvent(QPaintEvent*)
{
    if (!hasContent()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect mapRect = minimapRect();
    painter.fillRect(mapRect, withAlpha(QColor(QStringLiteral("#64748B")), 64));
    painter.fillRect(viewportThumbRect(), withAlpha(QColor(QStringLiteral("#1F2937")), 132));

    std::vector<PaintMarker> activeMarkers;
    activeMarkers.reserve(markers_.size());
    auto drawMarker = [&](const PaintMarker& paintMarker) {
        if (!paintMarker.marker || !paintMarker.bodyRect.isValid()) {
            return;
        }
        const TraceMarker& marker = *paintMarker.marker;
        const QColor color = marker.color.isValid() ? marker.color : DefaultTraceMarkerColor(marker.logicalRow);
        const QColor bodyColor = withAlpha(color, paintMarker.active ? 236 : 138);
        const QColor borderColor = withAlpha(color.darker(paintMarker.active ? 125 : 110),
                                             paintMarker.active ? 245 : 150);
        const QPolygon tagShape = markerTagPolygon(paintMarker.bodyRect);
        if (!tagShape.isEmpty()) {
            painter.setPen(QPen(borderColor, 1));
            painter.setBrush(bodyColor);
            painter.drawPolygon(tagShape);
        }

        if (paintMarker.active && paintMarker.bodyRect.width() > kCollapsedTagWidth + 12) {
            painter.setPen(withAlpha(QColor(QStringLiteral("#0F172A")), 238));
            QFont tagFont = font();
            tagFont.setBold(true);
            painter.setFont(tagFont);
            const QFontMetrics metrics(tagFont);
            const QString text = marker.label.isEmpty()
                ? QStringLiteral("Marker %1").arg(marker.logicalRow + 1)
                : marker.label;
            painter.drawText(paintMarker.bodyRect.adjusted(5, 0, -5, 0),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             metrics.elidedText(text, Qt::ElideRight, qMax(1, paintMarker.bodyRect.width() - 10)));
        }
    };

    for (const TraceMarker& marker : markers_) {
        const bool hovered = marker.id == hoveredMarkerId_;
        const bool active = marker.id == selectedMarkerId_ || hovered;
        PaintMarker markerPaint{&marker, markerTagBodyRect(marker, active), active};
        if (!markerPaint.bodyRect.isValid()) {
            continue;
        }
        if (active) {
            activeMarkers.push_back(markerPaint);
        } else {
            drawMarker(markerPaint);
        }
    }

    for (const PaintMarker& markerPaint : activeMarkers) {
        drawMarker(markerPaint);
    }
}

void TraceMarkerOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (const TraceMarker* marker = markerAt(event->pos())) {
            const QString markerId = marker->id;
            const int logicalRow = marker->logicalRow;
            selectedMarkerId_ = markerId;
            Q_EMIT markerSelectRequested(markerId);
            Q_EMIT logicalRowActivated(logicalRow);
            update();
            event->accept();
            return;
        }
        if (minimapRect().adjusted(-2, 0, 2, 0).contains(event->pos())) {
            const int visibleRow = visibleRowForY(event->pos().y());
            if (visibleRow >= 0) {
                Q_EMIT logicalRowActivated(model_ ? model_->logicalRowAt(visibleRow) : -1);
            }
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::RightButton) {
        if (const TraceMarker* marker = markerAt(event->pos())) {
            const QString markerId = marker->id;
            selectedMarkerId_ = markerId;
            Q_EMIT markerSelected(markerId);
            showMarkerContextMenu(markerId, event->globalPosition().toPoint());
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TraceMarkerOverlay::mouseMoveEvent(QMouseEvent* event)
{
    QString hoveredMarker;
    if (const TraceMarker* marker = markerAt(event->pos())) {
        hoveredMarker = marker->id;
        QToolTip::showText(event->globalPosition().toPoint(),
                           QStringLiteral("%1 - row %2")
                               .arg(marker->label)
                               .arg(marker->logicalRow + 1),
                           this);
    } else if (minimapRect().adjusted(-2, 0, 2, 0).contains(event->pos())) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           QStringLiteral("Trace viewport"),
                           this);
    } else {
        QToolTip::hideText();
    }
    if (hoveredMarker != hoveredMarkerId_) {
        const bool hadExpanded = hasExpandedMarker();
        hoveredMarkerId_ = hoveredMarker;
        if (hadExpanded != hasExpandedMarker()) {
            updateOverlayGeometry();
        }
        update();
    }
}

void TraceMarkerOverlay::leaveEvent(QEvent* event)
{
    const bool hadExpanded = hasExpandedMarker();
    hoveredMarkerId_.clear();
    QToolTip::hideText();
    if (hadExpanded) {
        updateOverlayGeometry();
    }
    update();
    QWidget::leaveEvent(event);
}

void TraceMarkerOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void TraceMarkerOverlay::updateOverlayGeometry()
{
    if (!table_ || !model_) {
        hide();
        if (rowOverlay_) {
            rowOverlay_->hide();
        }
        if (moveOverlay_) {
            moveOverlay_->hide();
        }
        return;
    }
    const QRect target = overlayRectForCurrentTable();
    if (geometry() != target) {
        setGeometry(target);
    }
    setVisible(hasContent() && target.isValid() && table_->isVisible());
    if (isVisible()) {
        ensureOnTop();
    }
    if (rowOverlay_) {
        rowOverlay_->refreshGeometry();
    }
    if (moveOverlay_) {
        moveOverlay_->refreshGeometry();
    }
}

void TraceMarkerOverlay::updateModelConnections()
{
    QObject::disconnect(modelResetConnection_);
    QObject::disconnect(rowsInsertedConnection_);
    QObject::disconnect(rowsRemovedConnection_);
    QObject::disconnect(dataChangedConnection_);
    if (!model_) {
        return;
    }
    QAbstractItemModel* abstractModel = model_;
    const auto refreshFromModel = [this]() {
        refresh();
    };
    modelResetConnection_ = connect(abstractModel, &QAbstractItemModel::modelReset, this, refreshFromModel);
    rowsInsertedConnection_ = connect(abstractModel, &QAbstractItemModel::rowsInserted, this, refreshFromModel);
    rowsRemovedConnection_ = connect(abstractModel, &QAbstractItemModel::rowsRemoved, this, refreshFromModel);
    dataChangedConnection_ = connect(abstractModel, &QAbstractItemModel::dataChanged, this, refreshFromModel);
}

void TraceMarkerOverlay::refresh()
{
    updateOverlayGeometry();
    if (rowOverlay_) {
        rowOverlay_->refreshMarkers();
    }
    update();
}

void TraceMarkerOverlay::cancelMarkerMove()
{
    cancelMarkerMoveInternal(true);
}

void TraceMarkerOverlay::finishMarkerMove()
{
    cancelMarkerMoveInternal(false);
}

bool TraceMarkerOverlay::hasContent() const noexcept
{
    return model_ && model_->visibleCount() > 0;
}

bool TraceMarkerOverlay::hasExpandedMarker() const
{
    if (hoveredMarkerId_.isEmpty() && selectedMarkerId_.isEmpty()) {
        return false;
    }
    return std::any_of(markers_.cbegin(), markers_.cend(), [&](const TraceMarker& marker) {
        return (marker.id == hoveredMarkerId_ || marker.id == selectedMarkerId_)
            && visibleRowForLogicalRow(marker.logicalRow) >= 0;
    });
}

QRect TraceMarkerOverlay::overlayRectForCurrentTable() const
{
    if (!table_ || !table_->viewport()) {
        return {};
    }
    const QRect viewport = table_->viewport()->geometry();
    if (viewport.height() <= 0) {
        return {};
    }
    const QPoint tableOrigin = table_->mapTo(parentWidget(), QPoint(0, 0));

    QRect scroll = table_->verticalScrollBar() ? table_->verticalScrollBar()->geometry() : QRect();
    if (scroll.left() <= 0 || scroll.height() <= 0) {
        scroll = QRect(viewport.right() + 1,
                       viewport.top(),
                       qMax(1, table_->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, table_)),
                       viewport.height());
    }
    const int requestedWidth = hasExpandedMarker() ? kExpandedOverlayWidth : kOverlayWidth;
    const int width = qMin(requestedWidth, qMax(1, scroll.left()));
    const int right = tableOrigin.x() + scroll.left() - 1;
    const int x = right - width + 1;
    return QRect(qMax(tableOrigin.x(), x),
                 tableOrigin.y() + scroll.top(),
                 width,
                 qMax(1, scroll.height()));
}

void TraceMarkerOverlay::showMarkerContextMenu(const QString& markerId, const QPoint& globalPosition)
{
    if (markerId.isEmpty()) {
        return;
    }
    selectedMarkerId_ = markerId;
    Q_EMIT markerSelected(markerId);

    QMenu menu(this);
    QAction* editAction = menu.addAction(QStringLiteral("Edit"));
    QAction* deleteAction = menu.addAction(QStringLiteral("Delete"));
    QAction* moveAction = menu.addAction(QStringLiteral("Move"));
    QAction* selectedAction = menu.exec(globalPosition);
    if (selectedAction == editAction) {
        Q_EMIT markerEditRequested(markerId);
    } else if (selectedAction == deleteAction) {
        Q_EMIT markerRemoveRequested(markerId);
    } else if (selectedAction == moveAction) {
        startMarkerMove(markerId);
    }
}

void TraceMarkerOverlay::startMarkerMove(const QString& markerId)
{
    const auto found = std::find_if(markers_.cbegin(), markers_.cend(), [&](const TraceMarker& marker) {
        return marker.id == markerId;
    });
    if (found == markers_.cend()) {
        return;
    }
    markerMoveActive_ = true;
    movingMarkerId_ = markerId;
    selectedMarkerId_ = markerId;
    if (!moveEventFilterInstalled_) {
        qApp->installEventFilter(this);
        moveEventFilterInstalled_ = true;
    }
    if (table_ && table_->viewport()) {
        movingCursorPos_ = table_->viewport()->mapFromGlobal(QCursor::pos());
        if (!table_->viewport()->rect().contains(movingCursorPos_)) {
            const int visibleRow = visibleRowForLogicalRow(found->logicalRow);
            if (visibleRow >= 0) {
                movingCursorPos_ = QPoint(0,
                                          table_->rowViewportPosition(visibleRow)
                                              + table_->rowHeight(visibleRow) / 2);
            } else {
                movingCursorPos_ = table_->viewport()->rect().center();
            }
        }
    }
    if (moveOverlay_) {
        moveOverlay_->refreshGeometry();
        moveOverlay_->update();
    }
    ensureOnTop();
    Q_EMIT markerMoveStarted(markerId);
}

bool TraceMarkerOverlay::dropMarkerMoveAtViewportPosition(const QPoint& viewportPosition)
{
    if (!markerMoveActive_ || !table_ || !model_) {
        return false;
    }
    const int visibleRow = table_->rowAt(viewportPosition.y());
    if (visibleRow < 0 || visibleRow >= model_->visibleCount()) {
        return false;
    }
    const int logicalRow = model_->logicalRowAt(visibleRow);
    if (logicalRow < 0) {
        return false;
    }
    Q_EMIT markerMoveDropped(movingMarkerId_, logicalRow);
    return true;
}

void TraceMarkerOverlay::cancelMarkerMoveInternal(const bool emitSignal)
{
    if (!markerMoveActive_) {
        return;
    }
    const QString markerId = movingMarkerId_;
    markerMoveActive_ = false;
    movingMarkerId_.clear();
    if (moveEventFilterInstalled_) {
        qApp->removeEventFilter(this);
        moveEventFilterInstalled_ = false;
    }
    if (moveOverlay_) {
        moveOverlay_->hide();
        moveOverlay_->update();
    }
    if (emitSignal) {
        Q_EMIT markerMoveCancelled(markerId);
    }
}

QRect TraceMarkerOverlay::moveOverlayRectForCurrentTable() const
{
    if (!table_ || !table_->viewport()) {
        return {};
    }
    const QRect viewport = table_->viewport()->geometry();
    if (viewport.width() <= 0 || viewport.height() <= 0) {
        return {};
    }
    const QPoint tableOrigin = table_->mapTo(parentWidget(), QPoint(0, 0));
    return QRect(tableOrigin.x() + viewport.left(),
                 tableOrigin.y() + viewport.top(),
                 viewport.width(),
                 viewport.height());
}

QRect TraceMarkerOverlay::moveGhostRect(const QPoint& position) const
{
    if (!table_ || !table_->viewport()) {
        return {};
    }
    const int height = qMin(kMoveGhostHeight, qMax(8, table_->viewport()->height()));
    return QRect(position.x(),
                 position.y() - height / 2,
                 kMoveGhostWidth,
                 height);
}

QPolygon TraceMarkerOverlay::moveGhostPolygon(const QRect& ghostRect) const
{
    if (!ghostRect.isValid()) {
        return {};
    }
    const int left = ghostRect.left();
    const int right = ghostRect.right();
    const int tipY = ghostRect.center().y();
    const int shoulderX = qMin(right, left + kMoveGhostTipWidth);
    const int top = ghostRect.top() + 1;
    const int bottom = ghostRect.bottom() - 1;
    const int chamfer = qMin(3, qMax(1, ghostRect.height() / 4));
    return QPolygon({
        QPoint(left, tipY),
        QPoint(shoulderX, top),
        QPoint(right - chamfer, top),
        QPoint(right, top + chamfer),
        QPoint(right, bottom - chamfer),
        QPoint(right - chamfer, bottom),
        QPoint(shoulderX, bottom),
    });
}

QRect TraceMarkerOverlay::rowOverlayRectForCurrentTable() const
{
    if (!table_ || !table_->viewport()) {
        return {};
    }
    const QRect viewport = table_->viewport()->geometry();
    if (viewport.height() <= 0) {
        return {};
    }
    const QPoint tableOrigin = table_->mapTo(parentWidget(), QPoint(0, 0));
    const int overlayWidth = rowOverlay_
        ? rowOverlay_->preferredOverlayWidth(qMax(kRowMarkerOverlayWidth, viewport.width()))
        : kRowMarkerOverlayWidth;
    return QRect(tableOrigin.x() + viewport.left(),
                 tableOrigin.y() + viewport.top(),
                 overlayWidth,
                 qMax(1, viewport.height()));
}

QRect TraceMarkerOverlay::markerViewportRowRect(const TraceMarker& marker) const
{
    if (!table_ || !model_ || !table_->viewport()) {
        return {};
    }
    const int visibleRow = visibleRowForLogicalRow(marker.logicalRow);
    if (visibleRow < 0) {
        return {};
    }
    const int rowY = table_->rowViewportPosition(visibleRow);
    const int rowHeight = table_->rowHeight(visibleRow);
    if (rowHeight <= 0 || rowY + rowHeight <= 0 || rowY >= table_->viewport()->height()) {
        return {};
    }
    return QRect(0, rowY, kRowMarkerOverlayWidth, rowHeight);
}

QRect TraceMarkerOverlay::minimapRect() const
{
    if (width() <= 0) {
        return {};
    }
    const int mapWidth = qMin(kMinimapWidth, width());
    return QRect(width() - mapWidth, 0, mapWidth, height());
}

QRect TraceMarkerOverlay::viewportThumbRect() const
{
    const QRect mapRect = minimapRect();
    if (!table_ || !table_->verticalScrollBar() || !model_ || model_->visibleCount() <= 0 || !mapRect.isValid()) {
        return {};
    }

    const int rowCount = model_->visibleCount();
    const int topVisibleRow = qBound(0, table_->verticalScrollBar()->value(), rowCount - 1);
    int bottomVisibleRow = topVisibleRow;
    if (table_->viewport()) {
        const int bottomY = qMax(0, table_->viewport()->height() - 1);
        bottomVisibleRow = table_->rowAt(bottomY);
        if (bottomVisibleRow < 0) {
            bottomVisibleRow = qMin(rowCount - 1,
                                    topVisibleRow + qMax(0, table_->verticalScrollBar()->pageStep() - 1));
        }
    }
    bottomVisibleRow = qBound(topVisibleRow, bottomVisibleRow, rowCount - 1);

    const int top = globalYForVisibleRow(topVisibleRow);
    const int bottom = globalYForVisibleRow(bottomVisibleRow + 1);
    const int thumbHeight = qMax(kMinimumThumbHeight, bottom - top);
    return QRect(mapRect.left(),
                 qBound(mapRect.top(), top, qMax(mapRect.top(), mapRect.bottom() - thumbHeight + 1)),
                 mapRect.width(),
                 qMin(thumbHeight, mapRect.height()));
}

int TraceMarkerOverlay::visibleRowForY(const int y) const
{
    if (!model_ || model_->visibleCount() <= 0 || height() <= 0) {
        return -1;
    }
    const double fraction = qBound(0.0, static_cast<double>(y) / static_cast<double>(qMax(1, height() - 1)), 1.0);
    return qBound(0,
                  static_cast<int>(std::llround(fraction * static_cast<double>(model_->visibleCount() - 1))),
                  model_->visibleCount() - 1);
}

int TraceMarkerOverlay::globalYForVisibleRow(const int visibleRow) const
{
    if (!model_ || model_->visibleCount() <= 0 || height() <= 0) {
        return 0;
    }
    const int clamped = qBound(0, visibleRow, model_->visibleCount());
    if (model_->visibleCount() <= 1) {
        return 0;
    }
    const double fraction = static_cast<double>(clamped) / static_cast<double>(model_->visibleCount());
    return qBound(0, static_cast<int>(std::floor(fraction * height())), qMax(0, height() - 1));
}

int TraceMarkerOverlay::visibleRowForLogicalRow(const int logicalRow) const
{
    return model_ ? model_->visibleRowForLogicalRow(logicalRow) : -1;
}

int TraceMarkerOverlay::markerCenterY(const TraceMarker& marker) const
{
    const int visibleRow = visibleRowForLogicalRow(marker.logicalRow);
    if (visibleRow < 0) {
        return -1;
    }
    return globalYForVisibleRow(visibleRow);
}

QRect TraceMarkerOverlay::markerTagBodyRect(const TraceMarker& marker, const bool expanded) const
{
    const int centerY = markerCenterY(marker);
    if (centerY < 0) {
        return {};
    }
    const QRect mapRect = minimapRect();
    if (!mapRect.isValid()) {
        return {};
    }

    const int pointerWidth = expanded ? kExpandedPointerWidth : kCollapsedPointerWidth;
    const int tagHeight = expanded ? kExpandedTagHeight : kCollapsedTagHeight;
    const int tagWidth = expanded ? qMin(kExpandedTagWidth, qMax(1, mapRect.left() - pointerWidth))
                                  : qMin(kCollapsedTagWidth, qMax(1, mapRect.left() - pointerWidth));
    const int right = mapRect.left() - pointerWidth;
    const int left = right - tagWidth + 1;
    const int boundedTagHeight = qMin(tagHeight, height());
    const int top = qBound(0, centerY - boundedTagHeight / 2, qMax(0, height() - boundedTagHeight));
    return QRect(left, top, tagWidth, boundedTagHeight);
}

QRect TraceMarkerOverlay::markerTagRect(const TraceMarker& marker, const bool expanded) const
{
    const QRect body = markerTagBodyRect(marker, expanded);
    const QRect mapRect = minimapRect();
    if (!body.isValid() || !mapRect.isValid()) {
        return {};
    }
    const int right = qMin(width() - 1, mapRect.right());
    return QRect(body.left(),
                 body.top(),
                 qMax(1, right - body.left() + 1),
                 body.height());
}

QRect TraceMarkerOverlay::markerHitRect(const TraceMarker& marker) const
{
    const bool expanded = marker.id == hoveredMarkerId_ || marker.id == selectedMarkerId_;
    return markerTagRect(marker, expanded).adjusted(-3, -4, 3, 4).intersected(rect());
}

QPolygon TraceMarkerOverlay::markerTagPolygon(const QRect& bodyRect) const
{
    const QRect mapRect = minimapRect();
    if (!bodyRect.isValid() || !mapRect.isValid()) {
        return {};
    }
    const int centerY = bodyRect.center().y();
    const int pointX = mapRect.left();
    const int baseX = bodyRect.right();
    return QPolygon({
        QPoint(bodyRect.left(), bodyRect.top()),
        QPoint(baseX, bodyRect.top()),
        QPoint(pointX, centerY),
        QPoint(baseX, bodyRect.bottom()),
        QPoint(bodyRect.left(), bodyRect.bottom()),
    });
}

const TraceMarker* TraceMarkerOverlay::markerAtIndex(const int markerIndex) const
{
    if (markerIndex < 0 || markerIndex >= static_cast<int>(markers_.size())) {
        return nullptr;
    }
    return &markers_[static_cast<std::size_t>(markerIndex)];
}

const TraceMarker* TraceMarkerOverlay::markerAt(const QPoint& position) const
{
    for (auto it = markers_.crbegin(); it != markers_.crend(); ++it) {
        const TraceMarker& marker = *it;
        if (markerHitRect(marker).contains(position)) {
            return &marker;
        }
    }
    return nullptr;
}

QRect TraceMarkerOverlay::parentRect(const QRect& localRect) const
{
    if (!localRect.isValid()) {
        return {};
    }
    return QRect(mapToParent(localRect.topLeft()), localRect.size());
}

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
QRect TraceMarkerOverlay::testMinimapGeometry() const
{
    return parentRect(minimapRect());
}

QRect TraceMarkerOverlay::testCollapsedMarkerTagGeometry(const int markerIndex) const
{
    const TraceMarker* marker = markerAtIndex(markerIndex);
    return marker ? parentRect(markerTagRect(*marker, false)) : QRect();
}

QRect TraceMarkerOverlay::testExpandedMarkerTagGeometry(const int markerIndex) const
{
    const TraceMarker* marker = markerAtIndex(markerIndex);
    return marker ? parentRect(markerTagRect(*marker, true)) : QRect();
}

QRect TraceMarkerOverlay::testLeftMarkerOverlayGeometry() const
{
    return rowOverlay_ ? rowOverlay_->geometry() : QRect();
}

QRect TraceMarkerOverlay::testLeftMarkerPolygonGeometry(const int markerIndex) const
{
    const TraceMarker* marker = rowOverlay_ ? rowOverlay_->markerAtIndex(markerIndex) : nullptr;
    return marker && rowOverlay_ ? rowOverlay_->parentMarkerPolygonRect(*marker) : QRect();
}

QRect TraceMarkerOverlay::testLeftMarkerNameGeometry(const int markerIndex) const
{
    const TraceMarker* marker = rowOverlay_ ? rowOverlay_->markerAtIndex(markerIndex) : nullptr;
    return marker && rowOverlay_ ? rowOverlay_->parentMarkerNameRect(*marker) : QRect();
}

bool TraceMarkerOverlay::testClickLeftMarkerPolygon(const int markerIndex)
{
    if (!rowOverlay_) {
        return false;
    }
    const TraceMarker* marker = rowOverlay_->markerAtIndex(markerIndex);
    const QRect polygon = marker ? rowOverlay_->parentMarkerPolygonRect(*marker) : QRect();
    if (!polygon.isValid()) {
        return false;
    }
    const QPoint parentCenter = polygon.center();
    const QPoint localCenter = rowOverlay_->mapFromParent(parentCenter);
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(localCenter),
                      QPointF(rowOverlay_->mapToGlobal(localCenter)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(rowOverlay_, &press);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(localCenter),
                        QPointF(rowOverlay_->mapToGlobal(localCenter)),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(rowOverlay_, &release);
    QApplication::processEvents();
    return press.isAccepted();
}

bool TraceMarkerOverlay::testDoubleClickLeftMarkerPolygon(const int markerIndex)
{
    if (!rowOverlay_) {
        return false;
    }
    const TraceMarker* marker = rowOverlay_->markerAtIndex(markerIndex);
    const QRect polygon = marker ? rowOverlay_->parentMarkerPolygonRect(*marker) : QRect();
    if (!polygon.isValid()) {
        return false;
    }
    const QPoint parentCenter = polygon.center();
    const QPoint localCenter = rowOverlay_->mapFromParent(parentCenter);
    QMouseEvent doubleClick(QEvent::MouseButtonDblClick,
                            QPointF(localCenter),
                            QPointF(rowOverlay_->mapToGlobal(localCenter)),
                            Qt::LeftButton,
                            Qt::LeftButton,
                            Qt::NoModifier);
    QApplication::sendEvent(rowOverlay_, &doubleClick);
    QApplication::processEvents();
    return doubleClick.isAccepted();
}

bool TraceMarkerOverlay::testStartMarkerMoveFromTag(const int markerIndex)
{
    const TraceMarker* marker = markerAtIndex(markerIndex);
    if (!marker) {
        return false;
    }
    startMarkerMove(marker->id);
    QApplication::processEvents();
    return markerMoveActive_ && movingMarkerId_ == marker->id;
}

bool TraceMarkerOverlay::testStartMarkerMoveFromLeftPolygon(const int markerIndex)
{
    const TraceMarker* marker = rowOverlay_ ? rowOverlay_->markerAtIndex(markerIndex) : nullptr;
    if (!marker) {
        return false;
    }
    startMarkerMove(marker->id);
    QApplication::processEvents();
    return markerMoveActive_ && movingMarkerId_ == marker->id;
}

bool TraceMarkerOverlay::testDropMarkerMoveOnLogicalRow(const int logicalRow)
{
    if (!markerMoveActive_ || !table_ || !model_) {
        return false;
    }
    const int visibleRow = model_->visibleRowForLogicalRow(logicalRow);
    if (visibleRow < 0) {
        return false;
    }
    const int rowY = table_->rowViewportPosition(visibleRow);
    const int rowHeight = table_->rowHeight(visibleRow);
    if (rowHeight <= 0) {
        return false;
    }
    const QPoint point(0, rowY + rowHeight / 2);
    const bool dropped = dropMarkerMoveAtViewportPosition(point);
    QApplication::processEvents();
    return dropped;
}

void TraceMarkerOverlay::testCancelMarkerMove()
{
    cancelMarkerMove();
    QApplication::processEvents();
}
#endif

}  // namespace CHIron::Gui
