#include "marker_widget.hpp"

#include "gui_format.hpp"

#include <QAction>
#include <QColorDialog>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QHash>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStringList>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

namespace CHIron::Gui {
namespace {

enum MarkerColumns {
    LabelColumn = 0,
    ChannelColumn,
    OpcodeColumn,
    AddressColumn,
    RowColumn,
    TimeColumn,
    ColorColumn,
    MarkerColumnCount
};

constexpr int kMarkerIdRole = Qt::UserRole + 1;
constexpr const char* kAllStickyGroupId = "__all__";
constexpr qreal kStickyDefaultWidth = 174.0;
constexpr qreal kStickyDefaultHeight = 126.0;
constexpr qreal kStickyMinWidth = 116.0;
constexpr qreal kStickyMinHeight = 82.0;
constexpr qreal kStickyHandleSize = 14.0;
constexpr qreal kStickyMagneticSnapThreshold = 10.0;
constexpr qreal kStickyGridSpacing = 16.0;
constexpr int kStickyColumns = 3;

QTableWidgetItem* makeMarkerItem(const QString& text,
                                 const QString& markerId,
                                 const bool editable)
{
    auto* item = new QTableWidgetItem(text);
    item->setData(kMarkerIdRole, markerId);
    if (editable) {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    } else {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }
    return item;
}

bool isAllStickyGroupId(const QString& groupId)
{
    return groupId == QString::fromLatin1(kAllStickyGroupId);
}

bool containsMarkerId(const std::vector<QString>& markerIds, const QString& markerId)
{
    return std::find(markerIds.cbegin(), markerIds.cend(), markerId) != markerIds.cend();
}

QColor markerChannelAccent(const QString& channel)
{
    if (channel == QStringLiteral("REQ")) {
        return QColor(QStringLiteral("#F3A847"));
    }
    if (channel == QStringLiteral("RSP")) {
        return QColor(QStringLiteral("#5AB9D6"));
    }
    if (channel == QStringLiteral("DAT")) {
        return QColor(QStringLiteral("#7BC96F"));
    }
    if (channel == QStringLiteral("SNP")) {
        return QColor(QStringLiteral("#E7776F"));
    }
    return QColor(QStringLiteral("#D9D9D9"));
}

qreal snapToStickyGrid(const qreal value)
{
    return std::round(value / kStickyGridSpacing) * kStickyGridSpacing;
}

class StickyGraphicsView final : public QGraphicsView {
public:
    explicit StickyGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent)
    {
    }

    void setAlignmentGridVisible(const bool visible)
    {
        if (alignmentGridVisible_ == visible) {
            return;
        }
        alignmentGridVisible_ = visible;
        viewport()->update();
    }

    bool alignmentGridVisible() const
    {
        return alignmentGridVisible_;
    }

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override
    {
        QGraphicsView::drawBackground(painter, rect);
        if (!alignmentGridVisible_) {
            return;
        }

        painter->save();
        QPen pen(QColor(62, 72, 86, 46), 1.0);
        pen.setCosmetic(true);
        painter->setPen(pen);
        const qreal left = std::floor(rect.left() / kStickyGridSpacing) * kStickyGridSpacing;
        const qreal top = std::floor(rect.top() / kStickyGridSpacing) * kStickyGridSpacing;
        for (qreal x = left; x <= rect.right(); x += kStickyGridSpacing) {
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (qreal y = top; y <= rect.bottom(); y += kStickyGridSpacing) {
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
        painter->restore();
    }

private:
    bool alignmentGridVisible_ = false;
};

class StickyNoteItem final : public QGraphicsObject {
public:
    StickyNoteItem(MarkerWidget* owner,
                   TraceMarker marker,
                   TraceMarkerDisplaySummary summary,
                   QRectF rect,
                   QGraphicsItem* parent = nullptr)
        : QGraphicsObject(parent)
        , owner_(owner)
        , marker_(std::move(marker))
        , summary_(std::move(summary))
        , rect_(0.0, 0.0, std::max(kStickyMinWidth, rect.width()), std::max(kStickyMinHeight, rect.height()))
    {
        setFlag(QGraphicsItem::ItemIsMovable, false);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setAcceptHoverEvents(true);
        setCursor(Qt::OpenHandCursor);
        setPos(rect.topLeft());

        labelItem_ = new QGraphicsTextItem(this);
        labelItem_->setTextInteractionFlags(Qt::NoTextInteraction);
        labelItem_->setTabChangesFocus(true);
        labelItem_->setDefaultTextColor(QColor(38, 42, 48));

        summaryItem_ = new QGraphicsTextItem(this);
        summaryItem_->setTextInteractionFlags(Qt::NoTextInteraction);
        summaryItem_->setDefaultTextColor(QColor(76, 82, 92));

        memoItem_ = new QGraphicsTextItem(this);
        memoItem_->setTextInteractionFlags(Qt::NoTextInteraction);
        memoItem_->setTabChangesFocus(true);
        memoItem_->setDefaultTextColor(QColor(68, 72, 80));

        updateText();
        updateChildGeometry();
    }

    QRectF boundingRect() const override
    {
        return rect_.adjusted(-1.0, -1.0, 1.0, 1.0);
    }

    QString markerId() const
    {
        return marker_.id;
    }

    QString displayedSummaryText() const
    {
        return summaryItem_ ? summaryItem_->toPlainText() : QString();
    }

    bool summaryFontIsSmallerThanLabel() const
    {
        return summaryItem_ && labelItem_
            && summaryItem_->font().pointSize() < labelItem_->font().pointSize();
    }

    bool memoStartsBelowSummary() const
    {
        if (!summaryItem_ || !memoItem_ || !summaryItem_->isVisible()) {
            return true;
        }
        return memoItem_->pos().y() >= summaryItem_->pos().y() + summaryItem_->boundingRect().height() + 1.0;
    }

    QSizeF noteSize() const
    {
        return rect_.size();
    }

    QRectF sceneNoteRect() const
    {
        return QRectF(pos(), rect_.size());
    }

    void installTextEventFilters()
    {
        if (textEventFiltersInstalled_ || !scene()) {
            return;
        }
        labelItem_->installSceneEventFilter(this);
        memoItem_->installSceneEventFilter(this);
        textEventFiltersInstalled_ = true;
    }

    void setSelectedFromOwner(const bool selected)
    {
        applyingOwnerSelection_ = true;
        setSelected(selected);
        applyingOwnerSelection_ = false;
        update();
    }

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
    void testDoubleClick()
    {
        QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseDoubleClick);
        event.setButton(Qt::LeftButton);
        event.setButtons(Qt::LeftButton);
        event.setPos(rect_.center());
        event.setScenePos(mapToScene(rect_.center()));
        mouseDoubleClickEvent(&event);
    }

    void testCommitMemoText(const QString& memo)
    {
        memoItem_->setPlainText(memo);
        commitTextEdits();
    }

    void testSetSceneRect(const QRectF& sceneRect)
    {
        applySceneRect(sceneRect);
    }

    void testSetSize(const QSizeF& size, const Qt::KeyboardModifiers modifiers)
    {
        applySize(size, modifiers);
    }
#endif

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        QColor fill = marker_.color.isValid() ? marker_.color.lighter(176) : QColor(255, 245, 178);
        fill.setAlpha(isSelected() || hovered_ ? 245 : 218);
        QColor border = marker_.color.isValid() ? marker_.color.darker(125) : QColor(164, 136, 48);
        border.setAlpha(isSelected() || hovered_ ? 230 : 145);

        painter->setBrush(fill);
        painter->setPen(QPen(border, isSelected() ? 2.0 : 1.0));
        painter->drawRect(rect_);

        const QRectF handle = resizeHandleRect();
        painter->setPen(Qt::NoPen);
        QColor handleColor = border;
        handleColor.setAlpha(hoveredHandle_ ? 230 : 135);
        painter->setBrush(handleColor);
        painter->drawRect(handle);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && resizeHandleRect().contains(event->pos())) {
            resizing_ = true;
            interactionMoved_ = false;
            resizeStartScenePos_ = event->scenePos();
            resizeStartSize_ = rect_.size();
            updateAlignmentGrid(event->modifiers());
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            dragging_ = true;
            interactionMoved_ = false;
            dragStartScenePos_ = event->scenePos();
            dragStartItemPos_ = pos();
            updateAlignmentGrid(event->modifiers());
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        setCursor(Qt::ClosedHandCursor);
        QGraphicsObject::mousePressEvent(event);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (resizing_) {
            const QPointF delta = event->scenePos() - resizeStartScenePos_;
            interactionMoved_ = interactionMoved_ || std::abs(delta.x()) + std::abs(delta.y()) > 1.5;
            const QSizeF newSize(std::max(kStickyMinWidth, resizeStartSize_.width() + delta.x()),
                                 std::max(kStickyMinHeight, resizeStartSize_.height() + delta.y()));
            applySize(newSize, event->modifiers());
            event->accept();
            return;
        }
        if (dragging_) {
            const QPointF delta = event->scenePos() - dragStartScenePos_;
            interactionMoved_ = interactionMoved_ || std::abs(delta.x()) + std::abs(delta.y()) > 1.5;
            applySceneRect(QRectF(dragStartItemPos_ + delta, rect_.size()), event->modifiers());
            event->accept();
            return;
        }
        QGraphicsObject::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        const bool wasResizing = resizing_;
        const bool wasDragging = dragging_;
        const bool wasMoved = interactionMoved_;
        resizing_ = false;
        dragging_ = false;
        interactionMoved_ = false;
        updateAlignmentGrid(Qt::NoModifier);
        setCursor(Qt::OpenHandCursor);
        if (!wasDragging && !wasResizing) {
            QGraphicsObject::mouseReleaseEvent(event);
        }
        if (owner_ && (wasResizing || (wasDragging && wasMoved))) {
            owner_->stickyItemUpdateLayout(marker_.id, pos(), rect_.size());
        }
        if (wasDragging && !wasMoved && event->button() == Qt::LeftButton) {
            queueOwnerSelection(false);
        }
        event->accept();
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            queueOwnerSelection(true);
            event->accept();
            return;
        }
        QGraphicsObject::mouseDoubleClickEvent(event);
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
    {
        if (owner_) {
            owner_->stickyItemShowContextMenu(marker_.id, event->screenPos());
            event->accept();
            return;
        }
        QGraphicsObject::contextMenuEvent(event);
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
    {
        hovered_ = true;
        hoveredHandle_ = resizeHandleRect().contains(event->pos());
        update();
        QGraphicsObject::hoverMoveEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
    {
        hovered_ = false;
        hoveredHandle_ = false;
        update();
        QGraphicsObject::hoverLeaveEvent(event);
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == QGraphicsItem::ItemPositionHasChanged && owner_ && !resizing_ && !dragging_) {
            owner_->stickyItemUpdateLayout(marker_.id, value.toPointF(), rect_.size());
        }
        return QGraphicsObject::itemChange(change, value);
    }

    bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override
    {
        if ((watched == labelItem_ || watched == memoItem_) && event->type() == QEvent::FocusOut) {
            commitTextEdits();
        }
        return QGraphicsObject::sceneEventFilter(watched, event);
    }

private:
    QRectF resizeHandleRect() const
    {
        return QRectF(rect_.right() - kStickyHandleSize,
                      rect_.bottom() - kStickyHandleSize,
                      kStickyHandleSize,
                      kStickyHandleSize);
    }

    void updateText()
    {
        QFont labelFont;
        labelFont.setBold(true);
        labelItem_->setFont(labelFont);
        labelItem_->setPlainText(marker_.label);
        labelItem_->setTextInteractionFlags(Qt::TextEditorInteraction);

        QFont summaryFont;
        summaryFont.setPointSize(std::max(7, summaryFont.pointSize() - 2));
        summaryFont.setBold(true);
        summaryItem_->setFont(summaryFont);
        summaryItem_->setPlainText(summaryText());
        summaryItem_->setVisible(!summaryItem_->toPlainText().isEmpty());

        QFont memoFont;
        memoFont.setPointSize(std::max(7, memoFont.pointSize() - 1));
        memoItem_->setFont(memoFont);
        memoItem_->setPlainText(marker_.memo);
        memoItem_->setTextInteractionFlags(Qt::TextEditorInteraction);
    }

    void queueOwnerSelection(const bool activate)
    {
        if (!owner_) {
            return;
        }
        QPointer<MarkerWidget> owner(owner_);
        const QString markerId = marker_.id;
        QTimer::singleShot(0, owner_, [owner, markerId, activate]() {
            if (owner) {
                owner->stickyItemSelectMarker(markerId, activate);
            }
        });
    }

    void updateChildGeometry()
    {
        constexpr qreal margin = 8.0;
        constexpr qreal labelTop = 4.0;
        constexpr qreal minimumSummaryTop = 27.0;
        constexpr qreal memoTopWithoutSummary = 46.0;
        constexpr qreal labelSummaryGap = 2.0;
        constexpr qreal summaryMemoGap = 5.0;
        constexpr qreal memoMinimumHeight = 28.0;
        const bool hasSummary = summaryItem_ && summaryItem_->isVisible();
        const qreal contentWidth = std::max<qreal>(20.0, rect_.width() - margin * 2.0);

        labelItem_->setPos(margin, labelTop);
        labelItem_->setTextWidth(contentWidth);
        const qreal labelBottom = labelTop + labelItem_->boundingRect().height();
        const qreal summaryTop = std::max(minimumSummaryTop, labelBottom + labelSummaryGap);
        summaryItem_->setPos(margin, summaryTop);
        summaryItem_->setTextWidth(contentWidth);

        const qreal summaryHeight = hasSummary ? summaryItem_->boundingRect().height() : 0.0;
        const qreal memoTop = hasSummary
            ? std::max(memoTopWithoutSummary, summaryTop + summaryHeight + summaryMemoGap)
            : std::max(memoTopWithoutSummary, labelBottom + summaryMemoGap);
        const qreal requiredHeight = std::max<qreal>(kStickyMinHeight, memoTop + memoMinimumHeight + margin);
        if (rect_.height() < requiredHeight) {
            prepareGeometryChange();
            rect_.setHeight(requiredHeight);
        }

        rowRect_ = QRectF(margin,
                          hasSummary ? summaryTop - 1.0 : 28.0,
                          contentWidth,
                          hasSummary ? std::max<qreal>(18.0, summaryHeight) : 18.0);
        memoItem_->setPos(margin, memoTop);
        memoItem_->setTextWidth(contentWidth);
    }

    void applySceneRect(const QRectF& candidateRect, const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QRectF adjustedRect = owner_
            ? owner_->stickyItemAdjustedRect(marker_.id, candidateRect, modifiers)
            : candidateRect;
        adjustedRect.setWidth(std::max(kStickyMinWidth, adjustedRect.width()));
        adjustedRect.setHeight(std::max(kStickyMinHeight, adjustedRect.height()));
        setPos(adjustedRect.topLeft());
        if (rect_.size() != adjustedRect.size()) {
            prepareGeometryChange();
            rect_.setSize(adjustedRect.size());
            updateChildGeometry();
        }
        updateAlignmentGrid(modifiers);
        update();
    }

    void applySize(const QSizeF& candidateSize, const Qt::KeyboardModifiers modifiers)
    {
        QRectF adjustedRect = owner_
            ? owner_->stickyItemAdjustedRect(marker_.id,
                                             QRectF(pos(),
                                                    QSizeF(std::max(kStickyMinWidth, candidateSize.width()),
                                                           std::max(kStickyMinHeight, candidateSize.height()))),
                                             modifiers,
                                             true)
            : QRectF(pos(), candidateSize);
        adjustedRect.setWidth(std::max(kStickyMinWidth, adjustedRect.width()));
        adjustedRect.setHeight(std::max(kStickyMinHeight, adjustedRect.height()));
        prepareGeometryChange();
        rect_.setSize(adjustedRect.size());
        updateChildGeometry();
        updateAlignmentGrid(modifiers);
        update();
    }

    void updateAlignmentGrid(const Qt::KeyboardModifiers modifiers)
    {
        if (owner_) {
            owner_->stickyItemSetAlignmentGridVisible((dragging_ || resizing_) && modifiers.testFlag(Qt::AltModifier));
        }
    }

    void commitTextEdits()
    {
        if (!owner_) {
            return;
        }
        QPointer<MarkerWidget> owner(owner_);
        const QString markerId = marker_.id;
        const QString label = labelItem_->toPlainText().trimmed();
        const QString memo = memoItem_->toPlainText();
        QTimer::singleShot(0, owner_, [owner, markerId, label, memo]() {
            if (owner) {
                owner->stickyItemEditMarker(markerId, label, memo);
            }
        });
    }

    QString summaryText() const
    {
        QStringList parts;
        if (!summary_.channel.isEmpty()) {
            parts.push_back(summary_.channel);
        }
        if (!summary_.opcode.isEmpty()) {
            parts.push_back(summary_.opcode);
        }
        if (!summary_.address.isEmpty()) {
            parts.push_back(summary_.address);
        }
        return parts.join(QStringLiteral("  "));
    }

    MarkerWidget* owner_ = nullptr;
    TraceMarker marker_;
    TraceMarkerDisplaySummary summary_;
    QRectF rect_;
    QRectF rowRect_;
    QGraphicsTextItem* labelItem_ = nullptr;
    QGraphicsTextItem* summaryItem_ = nullptr;
    QGraphicsTextItem* memoItem_ = nullptr;
    bool hovered_ = false;
    bool hoveredHandle_ = false;
    bool resizing_ = false;
    bool dragging_ = false;
    bool interactionMoved_ = false;
    bool applyingOwnerSelection_ = false;
    bool textEventFiltersInstalled_ = false;
    QPointF resizeStartScenePos_;
    QSizeF resizeStartSize_;
    QPointF dragStartScenePos_;
    QPointF dragStartItemPos_;
};

}  // namespace

MarkerWidget::MarkerWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    wireSignals();
    refreshTable();
    refreshMemo();
}

MarkerWidget::~MarkerWidget()
{
    commitMemoEdit();
}

void MarkerWidget::setMarkers(const std::vector<TraceMarker>& markers, const QString& selectedMarkerId)
{
    setMarkers(markers, selectedMarkerId, stickyState_);
}

void MarkerWidget::setMarkers(const std::vector<TraceMarker>& markers,
                              const QString& selectedMarkerId,
                              const MarkerStickyState& stickyState)
{
    setMarkers(markers, selectedMarkerId, stickyState, {});
}

void MarkerWidget::setMarkers(const std::vector<TraceMarker>& markers,
                              const QString& selectedMarkerId,
                              const MarkerStickyState& stickyState,
                              const std::vector<TraceMarkerDisplaySummary>& summaries)
{
    commitMemoEdit();
    markers_ = markers;
    selectedMarkerId_ = selectedMarkerId;
    stickyState_ = stickyState;
    markerSummaries_.clear();
    markerSummaries_.reserve(static_cast<int>(summaries.size()));
    for (const TraceMarkerDisplaySummary& summary : summaries) {
        if (!summary.markerId.isEmpty()) {
            markerSummaries_.insert(summary.markerId, summary);
        }
    }
    memoEditDirty_ = false;
    if (!selectedMarkerId_.isEmpty() && !markerById(selectedMarkerId_)) {
        selectedMarkerId_.clear();
    }
    ensureStickyStateForMarkers(false);
    refreshTable();
    refreshMemo();
    refreshStickyTabs();
    refreshSticky();
}

MarkerStickyState MarkerWidget::stickyState() const
{
    return stickyState_;
}

void MarkerWidget::stickyItemSelectMarker(QString markerId, const bool activate)
{
    if (!markerById(markerId)) {
        return;
    }
    selectedMarkerId_ = std::move(markerId);
    refreshMemo();
    refreshTable();
    deleteButton_->setEnabled(true);
    Q_EMIT markerSelected(selectedMarkerId_);
    if (activate) {
        Q_EMIT markerActivated(selectedMarkerId_);
    }
}

void MarkerWidget::stickyItemEditMarker(const QString& markerId, const QString& label, const QString& memo)
{
    applyStickyMarkerEdit(markerId, label, memo);
}

void MarkerWidget::stickyItemUpdateLayout(const QString& markerId, const QPointF& position, const QSizeF& size)
{
    updateStickyMarkerLayout(markerId, position, size);
}

QRectF MarkerWidget::stickyItemAdjustedRect(const QString& markerId,
                                            QRectF candidateRect,
                                            const Qt::KeyboardModifiers modifiers,
                                            const bool resizeOnly) const
{
    candidateRect.setWidth(std::max(kStickyMinWidth, candidateRect.width()));
    candidateRect.setHeight(std::max(kStickyMinHeight, candidateRect.height()));
    if (modifiers.testFlag(Qt::AltModifier)) {
        return snapStickyRectToGrid(candidateRect, resizeOnly);
    }
    if (resizeOnly) {
        return snapStickyResizeRectToNeighbors(markerId, candidateRect);
    }
    return snapStickyRectToNeighbors(markerId, candidateRect);
}

void MarkerWidget::stickyItemSetAlignmentGridVisible(const bool visible)
{
    if (auto* view = dynamic_cast<StickyGraphicsView*>(stickyView_)) {
        view->setAlignmentGridVisible(visible);
    }
}

void MarkerWidget::stickyItemShowContextMenu(const QString& markerId, const QPoint& globalPos)
{
    if (!markerById(markerId)) {
        return;
    }

    QMenu menu(this);
    QMenu* deleteMenu = menu.addMenu(QStringLiteral("Delete"));
    QAction* deleteMarkerAction = deleteMenu->addAction(QStringLiteral("Delete marker"));
    QAction* deleteFromGroupAction = deleteMenu->addAction(QStringLiteral("Delete from the group"));
    deleteFromGroupAction->setEnabled(canRemoveStickyMarkerFromActiveGroup(markerId));

    QAction* jumpAction = menu.addAction(QStringLiteral("Jump to row"));

    std::vector<QAction*> copyActions;
    std::vector<QAction*> moveActions;
    for (const MarkerStickyGroup& group : stickyState_.groups) {
        if (isAllStickyGroupId(group.id)) {
            continue;
        }
        QAction* copyAction = menu.addAction(QStringLiteral("Copy into group [%1]").arg(group.name));
        copyAction->setData(group.id);
        copyActions.push_back(copyAction);

        QAction* moveAction = menu.addAction(QStringLiteral("Move into group [%1]").arg(group.name));
        moveAction->setData(group.id);
        moveActions.push_back(moveAction);
    }
    QAction* copyToNewGroupAction = menu.addAction(QStringLiteral("Copy into new group"));
    QAction* moveToNewGroupAction = menu.addAction(QStringLiteral("Move into new group"));

    QAction* chosen = menu.exec(globalPos);
    if (!chosen || !chosen->isEnabled()) {
        return;
    }
    if (chosen == deleteMarkerAction) {
        commitMemoEdit();
        Q_EMIT markerRemoved(markerId);
        return;
    }
    if (chosen == deleteFromGroupAction) {
        removeStickyMarkerFromActiveGroup(markerId);
        return;
    }
    if (chosen == jumpAction) {
        Q_EMIT markerActivated(markerId);
        return;
    }
    if (chosen == copyToNewGroupAction) {
        copyOrMoveStickyMarkerToNewGroup(markerId, false);
        return;
    }
    if (chosen == moveToNewGroupAction) {
        copyOrMoveStickyMarkerToNewGroup(markerId, true);
        return;
    }
    const auto copyFound = std::find(copyActions.cbegin(), copyActions.cend(), chosen);
    if (copyFound != copyActions.cend()) {
        copyStickyMarkerToGroup(markerId, chosen->data().toString());
        return;
    }
    const auto moveFound = std::find(moveActions.cbegin(), moveActions.cend(), chosen);
    if (moveFound != moveActions.cend()) {
        moveStickyMarkerToGroup(markerId, chosen->data().toString());
    }
}

void MarkerWidget::clear()
{
    commitMemoEdit();
    markers_.clear();
    selectedMarkerId_.clear();
    memoEditMarkerId_.clear();
    memoEditOriginalText_.clear();
    stickyState_ = {};
    markerSummaries_.clear();
    memoEditDirty_ = false;
    refreshTable();
    refreshMemo();
    refreshStickyTabs();
    refreshSticky();
}

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
void MarkerWidget::testSetStickyMode(const bool sticky)
{
    modeCombo_->setCurrentIndex(sticky ? 1 : 0);
}

int MarkerWidget::testStickyNoteCount() const
{
    if (!stickyScene_) {
        return 0;
    }
    int count = 0;
    for (QGraphicsItem* item : stickyScene_->items()) {
        if (dynamic_cast<StickyNoteItem*>(item)) {
            ++count;
        }
    }
    return count;
}

bool MarkerWidget::testMoveStickyNote(const QString& markerId, const QPointF position, const QSizeF size)
{
    if (!markerById(markerId)) {
        return false;
    }
    updateStickyMarkerLayout(markerId, position, size);
    refreshSticky();
    return true;
}

bool MarkerWidget::testDragStickyNote(const QString& markerId,
                                      const QPointF position,
                                      const Qt::KeyboardModifiers modifiers)
{
    const MarkerStickyNoteLayout* currentLayout = stickyLayoutForActiveMarker(markerId, false);
    if (!markerById(markerId) || !currentLayout) {
        return false;
    }
    const QRectF adjusted = stickyItemAdjustedRect(markerId,
                                                  QRectF(position, QSizeF(currentLayout->width, currentLayout->height)),
                                                  modifiers,
                                                  false);
    updateStickyMarkerLayout(markerId, adjusted.topLeft(), adjusted.size());
    stickyItemSetAlignmentGridVisible(false);
    refreshSticky();
    return true;
}

bool MarkerWidget::testPreviewStickyNoteDrag(const QString& markerId,
                                             const QPointF position,
                                             const Qt::KeyboardModifiers modifiers)
{
    const MarkerStickyNoteLayout* currentLayout = stickyLayoutForActiveMarker(markerId, false);
    if (!markerById(markerId) || !currentLayout) {
        return false;
    }
    const QRectF adjusted = stickyItemAdjustedRect(markerId,
                                                  QRectF(position, QSizeF(currentLayout->width, currentLayout->height)),
                                                  modifiers,
                                                  false);
    updateStickyMarkerLayout(markerId, adjusted.topLeft(), adjusted.size());
    stickyItemSetAlignmentGridVisible(modifiers.testFlag(Qt::AltModifier));
    refreshSticky();
    return true;
}

void MarkerWidget::testFinishStickyInteraction()
{
    stickyItemSetAlignmentGridVisible(false);
}

bool MarkerWidget::testResizeStickyNote(const QString& markerId,
                                        const QSizeF size,
                                        const Qt::KeyboardModifiers modifiers)
{
    const MarkerStickyNoteLayout* currentLayout = stickyLayoutForActiveMarker(markerId, false);
    if (!markerById(markerId) || !currentLayout) {
        return false;
    }
    const QRectF adjusted = stickyItemAdjustedRect(markerId,
                                                  QRectF(QPointF(currentLayout->x, currentLayout->y), size),
                                                  modifiers,
                                                  true);
    updateStickyMarkerLayout(markerId, adjusted.topLeft(), adjusted.size());
    stickyItemSetAlignmentGridVisible(false);
    refreshSticky();
    return true;
}

bool MarkerWidget::testStickyGridVisible() const
{
    if (const auto* view = dynamic_cast<const StickyGraphicsView*>(stickyView_)) {
        return view->alignmentGridVisible();
    }
    return false;
}

QString MarkerWidget::testSelectedMarkerId() const
{
    return selectedMarkerId_;
}

bool MarkerWidget::testStickyJumpToRow(const QString& markerId)
{
    if (!markerById(markerId)) {
        return false;
    }
    Q_EMIT markerActivated(markerId);
    return true;
}

bool MarkerWidget::testStickyDeleteMarker(const QString& markerId)
{
    if (!markerById(markerId)) {
        return false;
    }
    Q_EMIT markerRemoved(markerId);
    return true;
}

bool MarkerWidget::testStickyCanDeleteFromGroup(const QString& markerId) const
{
    return canRemoveStickyMarkerFromActiveGroup(markerId);
}

bool MarkerWidget::testStickyDeleteFromGroup(const QString& markerId)
{
    return removeStickyMarkerFromActiveGroup(markerId);
}

bool MarkerWidget::testStickyCopyToGroup(const QString& markerId, const QString& groupId)
{
    return copyStickyMarkerToGroup(markerId, groupId);
}

bool MarkerWidget::testStickyMoveToGroup(const QString& markerId, const QString& groupId)
{
    return moveStickyMarkerToGroup(markerId, groupId);
}

bool MarkerWidget::testStickyCopyToNewGroup(const QString& markerId, const QString& groupName)
{
    return copyOrMoveStickyMarkerToNewGroup(markerId, false, groupName);
}

bool MarkerWidget::testStickyMoveToNewGroup(const QString& markerId, const QString& groupName)
{
    return copyOrMoveStickyMarkerToNewGroup(markerId, true, groupName);
}

bool MarkerWidget::testStickyMarkerInGroup(const QString& groupId, const QString& markerId) const
{
    const MarkerStickyGroup* group = stickyGroupById(groupId);
    return group && stickyGroupContainsMarker(*group, markerId);
}

int MarkerWidget::testStickyCustomGroupMembershipCount(const QString& markerId) const
{
    return customStickyGroupMembershipCount(markerId);
}

bool MarkerWidget::testTriggerStickyDeleteShortcut()
{
    return deleteSelectedStickyNote();
}

void MarkerWidget::testSetStickyTextEditing(const bool editing)
{
    stickyTextEditingForTest_ = editing;
}

bool MarkerWidget::testEditStickyNote(const QString& markerId, const QString& label, const QString& memo)
{
    if (!markerById(markerId)) {
        return false;
    }
    applyStickyMarkerEdit(markerId, label, memo);
    return true;
}

bool MarkerWidget::testDoubleClickStickyNote(const QString& markerId)
{
    if (!stickyScene_) {
        return false;
    }
    for (QGraphicsItem* item : stickyScene_->items()) {
        auto* note = dynamic_cast<StickyNoteItem*>(item);
        if (note && note->markerId() == markerId) {
            note->testDoubleClick();
            return true;
        }
    }
    return false;
}

bool MarkerWidget::testAddStickyGroup(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    MarkerStickyGroup group;
    group.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    group.name = trimmed;
    stickyState_.groups.push_back(std::move(group));
    stickyState_.activeGroupId = stickyState_.groups.back().id;
    refreshStickyTabs();
    refreshSticky();
    emitStickyStateChanged();
    return true;
}

bool MarkerWidget::testSelectStickyGroup(const QString& groupId)
{
    if (!stickyGroupById(groupId)) {
        return false;
    }
    stickyState_.activeGroupId = groupId;
    refreshStickyTabs();
    refreshSticky();
    emitStickyStateChanged();
    return true;
}

bool MarkerWidget::testAddMarkerToActiveStickyGroup(const QString& markerId)
{
    MarkerStickyGroup* group = activeStickyGroup();
    if (!group || activeStickyTabIsAll() || !markerById(markerId)) {
        return false;
    }
    if (!stickyGroupContainsMarker(*group, markerId)) {
        group->markerIds.push_back(markerId);
        stickyLayoutForMarker(*group, markerId, true);
    }
    refreshSticky();
    emitStickyStateChanged();
    return true;
}

bool MarkerWidget::testRemoveMarkerFromActiveStickyGroup(const QString& markerId)
{
    return removeStickyMarkerFromActiveGroup(markerId);
}

QString MarkerWidget::testActiveStickyGroupId() const
{
    return stickyState_.activeGroupId;
}

QString MarkerWidget::testStickySummaryText(const QString& markerId) const
{
    if (!stickyScene_) {
        return QString();
    }
    for (QGraphicsItem* item : stickyScene_->items()) {
        if (const auto* note = dynamic_cast<StickyNoteItem*>(item);
            note && note->markerId() == markerId) {
            return note->displayedSummaryText();
        }
    }
    return QString();
}

bool MarkerWidget::testStickySummaryFontIsSmaller(const QString& markerId) const
{
    if (!stickyScene_) {
        return false;
    }
    for (QGraphicsItem* item : stickyScene_->items()) {
        if (const auto* note = dynamic_cast<StickyNoteItem*>(item);
            note && note->markerId() == markerId) {
            return note->summaryFontIsSmallerThanLabel();
        }
    }
    return false;
}

bool MarkerWidget::testStickyMemoStartsBelowSummary(const QString& markerId) const
{
    if (!stickyScene_) {
        return false;
    }
    for (QGraphicsItem* item : stickyScene_->items()) {
        if (const auto* note = dynamic_cast<StickyNoteItem*>(item);
            note && note->markerId() == markerId) {
            return note->memoStartsBelowSummary();
        }
    }
    return false;
}

bool MarkerWidget::testCommitStickyMemoText(const QString& markerId, const QString& memo)
{
    if (!stickyScene_) {
        return false;
    }
    for (QGraphicsItem* item : stickyScene_->items()) {
        auto* note = dynamic_cast<StickyNoteItem*>(item);
        if (note && note->markerId() == markerId) {
            note->testCommitMemoText(memo);
            return true;
        }
    }
    return false;
}
#endif

void MarkerWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* toolbar = new QWidget(this);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(5);

    summaryLabel_ = new QLabel(toolbar);
    summaryLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    toolbarLayout->addWidget(summaryLabel_, 1);

    modeCombo_ = new QComboBox(toolbar);
    modeCombo_->setObjectName(QStringLiteral("markerModeCombo"));
    modeCombo_->addItem(QStringLiteral("List"));
    modeCombo_->addItem(QStringLiteral("Sticky"));
    modeCombo_->setFixedHeight(22);
    toolbarLayout->addWidget(modeCombo_);

    loadButton_ = new QToolButton(toolbar);
    loadButton_->setText(QStringLiteral("Load"));
    loadButton_->setObjectName(QStringLiteral("actionButton"));
    loadButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    loadButton_->setFixedHeight(22);
    toolbarLayout->addWidget(loadButton_);

    saveButton_ = new QToolButton(toolbar);
    saveButton_->setText(QStringLiteral("Save"));
    saveButton_->setObjectName(QStringLiteral("actionButton"));
    saveButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    saveButton_->setFixedHeight(22);
    toolbarLayout->addWidget(saveButton_);

    deleteButton_ = new QToolButton(toolbar);
    deleteButton_->setText(QStringLiteral("Delete"));
    deleteButton_->setObjectName(QStringLiteral("actionButton"));
    deleteButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    deleteButton_->setFixedHeight(22);
    toolbarLayout->addWidget(deleteButton_);
    root->addWidget(toolbar);

    auto* searchBar = new QWidget(this);
    auto* searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(5);

    searchEdit_ = new QLineEdit(searchBar);
    searchEdit_->setObjectName(QStringLiteral("markerSearchEdit"));
    searchEdit_->setPlaceholderText(QStringLiteral("Search name or memo"));
    searchEdit_->setClearButtonEnabled(false);
    searchEdit_->setMinimumHeight(22);
    searchLayout->addWidget(searchEdit_, 1);

    clearSearchButton_ = new QToolButton(searchBar);
    clearSearchButton_->setText(QStringLiteral("Clear"));
    clearSearchButton_->setObjectName(QStringLiteral("actionButton"));
    clearSearchButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    clearSearchButton_->setFixedHeight(22);
    clearSearchButton_->setEnabled(false);
    searchLayout->addWidget(clearSearchButton_);
    root->addWidget(searchBar);

    viewStack_ = new QStackedWidget(this);
    root->addWidget(viewStack_, 1);

    listPage_ = new QWidget(viewStack_);
    auto* listLayout = new QVBoxLayout(listPage_);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(6);

    table_ = new QTableWidget(0, MarkerColumnCount, listPage_);
    table_->setObjectName(QStringLiteral("markerTable"));
    table_->setHorizontalHeaderLabels({QStringLiteral("Label"),
                                       QStringLiteral("Channel"),
                                       QStringLiteral("Opcode"),
                                       QStringLiteral("Address"),
                                       QStringLiteral("Row"),
                                       QStringLiteral("Time"),
                                       QStringLiteral("Color")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setAlternatingRowColors(true);
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideRight);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setMinimumSectionSize(20);
    table_->verticalHeader()->setDefaultSectionSize(22);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(LabelColumn, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(ChannelColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(OpcodeColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(AddressColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(RowColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(TimeColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(ColorColumn, QHeaderView::ResizeToContents);
    table_->setMinimumHeight(160);
    listLayout->addWidget(table_, 2);

    memoEdit_ = new QPlainTextEdit(listPage_);
    memoEdit_->setPlaceholderText(QStringLiteral("Marker memo"));
    memoEdit_->setMinimumHeight(96);
    memoEdit_->installEventFilter(this);
    listLayout->addWidget(memoEdit_, 1);
    viewStack_->addWidget(listPage_);

    stickyPage_ = new QWidget(viewStack_);
    auto* stickyLayout = new QVBoxLayout(stickyPage_);
    stickyLayout->setContentsMargins(0, 0, 0, 0);
    stickyLayout->setSpacing(5);

    auto* stickyToolbar = new QWidget(stickyPage_);
    auto* stickyToolbarLayout = new QHBoxLayout(stickyToolbar);
    stickyToolbarLayout->setContentsMargins(0, 0, 0, 0);
    stickyToolbarLayout->setSpacing(5);

    stickyTabBar_ = new QTabBar(stickyToolbar);
    stickyTabBar_->setObjectName(QStringLiteral("markerStickyTabBar"));
    stickyTabBar_->setExpanding(false);
    stickyTabBar_->setMovable(false);
    stickyToolbarLayout->addWidget(stickyTabBar_, 1);

    stickyAddGroupButton_ = new QToolButton(stickyToolbar);
    stickyAddGroupButton_->setText(QStringLiteral("+"));
    stickyAddGroupButton_->setObjectName(QStringLiteral("actionButton"));
    stickyAddGroupButton_->setFixedSize(24, 22);
    stickyToolbarLayout->addWidget(stickyAddGroupButton_);

    stickyRenameGroupButton_ = new QToolButton(stickyToolbar);
    stickyRenameGroupButton_->setText(QStringLiteral("Rename"));
    stickyRenameGroupButton_->setObjectName(QStringLiteral("actionButton"));
    stickyRenameGroupButton_->setFixedHeight(22);
    stickyToolbarLayout->addWidget(stickyRenameGroupButton_);

    stickyDeleteGroupButton_ = new QToolButton(stickyToolbar);
    stickyDeleteGroupButton_->setText(QStringLiteral("Delete"));
    stickyDeleteGroupButton_->setObjectName(QStringLiteral("actionButton"));
    stickyDeleteGroupButton_->setFixedHeight(22);
    stickyToolbarLayout->addWidget(stickyDeleteGroupButton_);

    stickyAddMarkerButton_ = new QToolButton(stickyToolbar);
    stickyAddMarkerButton_->setText(QStringLiteral("Add Marker"));
    stickyAddMarkerButton_->setObjectName(QStringLiteral("actionButton"));
    stickyAddMarkerButton_->setFixedHeight(22);
    stickyToolbarLayout->addWidget(stickyAddMarkerButton_);

    stickyRemoveMarkerButton_ = new QToolButton(stickyToolbar);
    stickyRemoveMarkerButton_->setText(QStringLiteral("Remove"));
    stickyRemoveMarkerButton_->setObjectName(QStringLiteral("actionButton"));
    stickyRemoveMarkerButton_->setFixedHeight(22);
    stickyToolbarLayout->addWidget(stickyRemoveMarkerButton_);
    stickyLayout->addWidget(stickyToolbar);

    stickyScene_ = new QGraphicsScene(stickyPage_);
    stickyView_ = new StickyGraphicsView(stickyScene_, stickyPage_);
    stickyView_->setObjectName(QStringLiteral("markerStickyView"));
    stickyView_->setRenderHint(QPainter::Antialiasing, true);
    stickyView_->setDragMode(QGraphicsView::RubberBandDrag);
    stickyView_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    stickyView_->setFocusPolicy(Qt::StrongFocus);
    stickyView_->setMinimumHeight(240);
    stickyView_->installEventFilter(this);
    stickyView_->viewport()->installEventFilter(this);
    stickyLayout->addWidget(stickyView_, 1);
    viewStack_->addWidget(stickyPage_);
}

void MarkerWidget::wireSignals()
{
    connect(modeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        commitMemoEdit();
        viewStack_->setCurrentIndex(index == 1 ? 1 : 0);
        refreshSticky();
    });
    connect(loadButton_, &QToolButton::clicked, this, [this]() {
        Q_EMIT loadRequested();
    });
    connect(saveButton_, &QToolButton::clicked, this, [this]() {
        Q_EMIT saveRequested();
    });
    connect(deleteButton_, &QToolButton::clicked, this, [this]() {
        if (!selectedMarkerId_.isEmpty()) {
            commitMemoEdit();
            Q_EMIT markerRemoved(selectedMarkerId_);
        }
    });
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (rebuilding_) {
            return;
        }
        commitMemoEdit();
        clearSearchButton_->setEnabled(!text.isEmpty());
        refreshTable();
        refreshSticky();
    });
    connect(clearSearchButton_, &QToolButton::clicked, searchEdit_, &QLineEdit::clear);
    connect(stickyTabBar_, &QTabBar::currentChanged, this, [this](const int index) {
        if (rebuilding_) {
            return;
        }
        const QString groupId = stickyTabBar_->tabData(index).toString();
        if (!groupId.isEmpty() && stickyState_.activeGroupId != groupId) {
            stickyState_.activeGroupId = groupId;
            refreshSticky();
            emitStickyStateChanged();
        }
    });
    connect(stickyAddGroupButton_, &QToolButton::clicked, this, &MarkerWidget::addStickyGroup);
    connect(stickyRenameGroupButton_, &QToolButton::clicked, this, &MarkerWidget::renameActiveStickyGroup);
    connect(stickyDeleteGroupButton_, &QToolButton::clicked, this, &MarkerWidget::deleteActiveStickyGroup);
    connect(stickyAddMarkerButton_, &QToolButton::clicked, this, &MarkerWidget::showAddMarkerToStickyGroupMenu);
    connect(stickyRemoveMarkerButton_, &QToolButton::clicked, this, &MarkerWidget::removeSelectedMarkerFromStickyGroup);
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (rebuilding_) {
            return;
        }
        updateSelectedMarkerFromTable();
    });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](const int row, const int column) {
        commitMemoEdit();
        if (row < 0 || row >= table_->rowCount()) {
            return;
        }
        const QTableWidgetItem* item = table_->item(row, LabelColumn);
        const QString markerId = item ? item->data(kMarkerIdRole).toString() : QString();
        TraceMarker* marker = markerById(markerId);
        if (!marker) {
            return;
        }
        if (column == ColorColumn) {
            const QColor chosen = QColorDialog::getColor(marker->color.isValid() ? marker->color : DefaultTraceMarkerColor(row),
                                                         this,
                                                         QStringLiteral("Marker Color"));
            if (!chosen.isValid()) {
                return;
            }
            marker->color = chosen;
            marker->updatedAt = QDateTime::currentDateTimeUtc();
            if (QTableWidgetItem* colorItem = table_->item(row, ColorColumn)) {
                const QSignalBlocker blocker(table_);
                colorItem->setText(TraceMarkerColorName(marker->color));
                colorItem->setBackground(marker->color);
                colorItem->setForeground(marker->color.lightness() < 128 ? Qt::white : Qt::black);
            }
            Q_EMIT markerEdited(*marker);
            return;
        }
        if (!markerId.isEmpty()) {
            Q_EMIT markerActivated(markerId);
        }
    });
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!rebuilding_) {
            applyItemEdit(item);
        }
    });
    connect(memoEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        if (rebuilding_) {
            return;
        }
        memoEditDirty_ = true;
    });
}

bool MarkerWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == memoEdit_ && event->type() == QEvent::FocusOut) {
        commitMemoEdit();
    }
    if ((watched == stickyView_ || (stickyView_ && watched == stickyView_->viewport()))
        && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Delete && keyEvent->modifiers() == Qt::NoModifier) {
            return deleteSelectedStickyNote();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MarkerWidget::commitMemoEdit()
{
    if (rebuilding_ || !memoEditDirty_ || memoEditMarkerId_.isEmpty()) {
        return;
    }
    TraceMarker* marker = markerById(memoEditMarkerId_);
    if (!marker) {
        memoEditDirty_ = false;
        memoEditMarkerId_.clear();
        memoEditOriginalText_.clear();
        return;
    }
    const QString memo = memoEdit_->toPlainText();
    if (memo == memoEditOriginalText_) {
        memoEditDirty_ = false;
        return;
    }
    marker->memo = memo;
    marker->updatedAt = QDateTime::currentDateTimeUtc();
    memoEditOriginalText_ = memo;
    memoEditDirty_ = false;
    Q_EMIT markerEdited(*marker);
}

void MarkerWidget::refreshTable()
{
    rebuildFilteredMarkerIndices();
    rebuilding_ = true;
    const QSignalBlocker tableBlocker(table_);
    table_->clearContents();
    table_->setRowCount(static_cast<int>(filteredMarkerIndices_.size()));

    for (int row = 0; row < static_cast<int>(filteredMarkerIndices_.size()); ++row) {
        const TraceMarker& marker = markers_[markerIndexForTableRow(row)];
        const TraceMarkerDisplaySummary* summary = markerSummaryById(marker.id);
        QTableWidgetItem* labelItem = makeMarkerItem(marker.label, marker.id, true);
        QTableWidgetItem* channelItem = makeMarkerItem(summary ? summary->channel : QString(), marker.id, false);
        QTableWidgetItem* opcodeItem = makeMarkerItem(summary ? summary->opcode : QString(), marker.id, false);
        QTableWidgetItem* addressItem = makeMarkerItem(summary ? summary->address : QString(), marker.id, false);
        QTableWidgetItem* rowItem = makeMarkerItem(QString::number(marker.logicalRow + 1), marker.id, false);
        QTableWidgetItem* timeItem = makeMarkerItem(FormatTimestampForDisplay(marker.timestamp), marker.id, false);
        QTableWidgetItem* colorItem = makeMarkerItem(TraceMarkerColorName(marker.color), marker.id, false);
        channelItem->setTextAlignment(Qt::AlignCenter);
        if (!channelItem->text().isEmpty()) {
            const QColor channelAccent = markerChannelAccent(channelItem->text());
            channelItem->setBackground(channelAccent.lighter(180));
            channelItem->setForeground(channelAccent.lightness() < 96 ? Qt::white : QColor(38, 42, 48));
        }
        colorItem->setBackground(marker.color);
        colorItem->setForeground(marker.color.lightness() < 128 ? Qt::white : Qt::black);
        table_->setItem(row, LabelColumn, labelItem);
        table_->setItem(row, ChannelColumn, channelItem);
        table_->setItem(row, OpcodeColumn, opcodeItem);
        table_->setItem(row, AddressColumn, addressItem);
        table_->setItem(row, RowColumn, rowItem);
        table_->setItem(row, TimeColumn, timeItem);
        table_->setItem(row, ColorColumn, colorItem);
        table_->setRowHeight(row, 22);
    }
    const bool filtered = searchEdit_ && !searchEdit_->text().trimmed().isEmpty();
    if (filtered) {
        summaryLabel_->setText(QStringLiteral("%1 / %2 markers")
                                   .arg(filteredMarkerIndices_.size())
                                   .arg(markers_.size()));
    } else {
        summaryLabel_->setText(QStringLiteral("%1 marker%2")
                                   .arg(markers_.size())
                                   .arg(markers_.size() == 1 ? QString() : QStringLiteral("s")));
    }
    const int selectedRow = rowForMarkerId(selectedMarkerId_);
    if (selectedRow >= 0) {
        table_->selectRow(selectedRow);
    } else {
        table_->clearSelection();
    }
    deleteButton_->setEnabled(!selectedMarkerId_.isEmpty() && markerById(selectedMarkerId_));
    saveButton_->setEnabled(!markers_.empty());
    rebuilding_ = false;
}

void MarkerWidget::refreshMemo()
{
    rebuilding_ = true;
    const QSignalBlocker memoBlocker(memoEdit_);
    if (const TraceMarker* marker = markerById(selectedMarkerId_)) {
        memoEdit_->setPlainText(marker->memo);
        memoEdit_->setEnabled(true);
        memoEditMarkerId_ = marker->id;
        memoEditOriginalText_ = marker->memo;
    } else {
        memoEdit_->clear();
        memoEdit_->setEnabled(false);
        memoEditMarkerId_.clear();
        memoEditOriginalText_.clear();
    }
    memoEditDirty_ = false;
    rebuilding_ = false;
}

void MarkerWidget::refreshSticky()
{
    if (!stickyScene_) {
        return;
    }
    ensureStickyStateForMarkers(false);
    stickyScene_->clear();

    const MarkerStickyGroup* group = activeStickyGroup();
    if (!group) {
        stickyScene_->setSceneRect(QRectF(0.0, 0.0, 640.0, 360.0));
        return;
    }

    qreal maxRight = 640.0;
    qreal maxBottom = 360.0;
    int visibleIndex = 0;
    for (const TraceMarker& marker : markers_) {
        if (!markerMatchesSearch(marker)) {
            continue;
        }
        if (!activeStickyTabIsAll() && !stickyGroupContainsMarker(*group, marker.id)) {
            continue;
        }
        MarkerStickyGroup* mutableGroup = activeStickyGroup();
        MarkerStickyNoteLayout* layout = mutableGroup
            ? stickyLayoutForMarker(*mutableGroup, marker.id, true)
            : nullptr;
        if (!layout) {
            continue;
        }
        if (layout->width <= 0.0 || layout->height <= 0.0) {
            layout->width = kStickyDefaultWidth;
            layout->height = kStickyDefaultHeight;
        }
        if (layout->x == 0.0 && layout->y == 0.0 && visibleIndex > 0) {
            layout->x = 16.0 + (visibleIndex % kStickyColumns) * (kStickyDefaultWidth + 18.0);
            layout->y = 16.0 + (visibleIndex / kStickyColumns) * (kStickyDefaultHeight + 18.0);
        }
        auto* item = new StickyNoteItem(this,
                                        marker,
                                        markerSummaryById(marker.id) ? *markerSummaryById(marker.id) : TraceMarkerDisplaySummary{},
                                        QRectF(layout->x, layout->y, layout->width, layout->height));
        item->setSelectedFromOwner(marker.id == selectedMarkerId_);
        stickyScene_->addItem(item);
        item->installTextEventFilters();
        maxRight = std::max(maxRight, layout->x + layout->width + 96.0);
        maxBottom = std::max(maxBottom, layout->y + layout->height + 96.0);
        ++visibleIndex;
    }
    stickyScene_->setSceneRect(QRectF(0.0, 0.0, maxRight, maxBottom));

    const bool customGroup = !activeStickyTabIsAll();
    stickyRenameGroupButton_->setEnabled(customGroup);
    stickyDeleteGroupButton_->setEnabled(customGroup);
    stickyAddMarkerButton_->setEnabled(customGroup && !markers_.empty());
    stickyRemoveMarkerButton_->setEnabled(canRemoveStickyMarkerFromActiveGroup(selectedMarkerId_));
}

void MarkerWidget::refreshStickyTabs()
{
    if (!stickyTabBar_) {
        return;
    }
    ensureStickyStateForMarkers(false);

    rebuilding_ = true;
    while (stickyTabBar_->count() > 0) {
        stickyTabBar_->removeTab(0);
    }
    for (const MarkerStickyGroup& group : stickyState_.groups) {
        stickyTabBar_->addTab(group.name);
        stickyTabBar_->setTabData(stickyTabBar_->count() - 1, group.id);
    }
    const int activeIndex = [&]() {
        for (int index = 0; index < stickyTabBar_->count(); ++index) {
            if (stickyTabBar_->tabData(index).toString() == stickyState_.activeGroupId) {
                return index;
            }
        }
        return 0;
    }();
    stickyTabBar_->setCurrentIndex(activeIndex);
    rebuilding_ = false;
}

void MarkerWidget::ensureStickyStateForMarkers(const bool addNewMarkersToActiveGroup)
{
    if (stickyState_.groups.empty() || !isAllStickyGroupId(stickyState_.groups.front().id)) {
        MarkerStickyGroup allGroup;
        allGroup.id = QString::fromLatin1(kAllStickyGroupId);
        allGroup.name = QStringLiteral("All");
        stickyState_.groups.insert(stickyState_.groups.begin(), std::move(allGroup));
    }
    stickyState_.groups.front().id = QString::fromLatin1(kAllStickyGroupId);
    stickyState_.groups.front().name = QStringLiteral("All");
    if (stickyState_.activeGroupId.isEmpty()) {
        stickyState_.activeGroupId = QString::fromLatin1(kAllStickyGroupId);
    }

    std::vector<QString> markerIds;
    markerIds.reserve(markers_.size());
    for (const TraceMarker& marker : markers_) {
        markerIds.push_back(marker.id);
    }

    auto markerExists = [&](const QString& markerId) {
        return containsMarkerId(markerIds, markerId);
    };

    MarkerStickyGroup& allGroup = stickyState_.groups.front();
    allGroup.markerIds = markerIds;

    for (MarkerStickyGroup& group : stickyState_.groups) {
        group.markerIds.erase(std::remove_if(group.markerIds.begin(),
                                             group.markerIds.end(),
                                             [&](const QString& markerId) {
                                                 return !markerExists(markerId);
                                             }),
                              group.markerIds.end());
        std::vector<QString> uniqueIds;
        for (const QString& markerId : group.markerIds) {
            if (!containsMarkerId(uniqueIds, markerId)) {
                uniqueIds.push_back(markerId);
            }
        }
        group.markerIds = std::move(uniqueIds);
        group.noteLayouts.erase(std::remove_if(group.noteLayouts.begin(),
                                               group.noteLayouts.end(),
                                               [&](const MarkerStickyNoteLayout& layout) {
                                                   return !markerExists(layout.markerId);
                                               }),
                                group.noteLayouts.end());
    }

    const bool activeGroupExists = std::any_of(stickyState_.groups.cbegin(),
                                               stickyState_.groups.cend(),
                                               [&](const MarkerStickyGroup& group) {
                                                   return group.id == stickyState_.activeGroupId;
                                               });
    if (!activeGroupExists) {
        stickyState_.activeGroupId = QString::fromLatin1(kAllStickyGroupId);
    }

    if (addNewMarkersToActiveGroup && !activeStickyTabIsAll()) {
        if (MarkerStickyGroup* group = activeStickyGroup()) {
            for (const QString& markerId : markerIds) {
                if (!containsMarkerId(group->markerIds, markerId)) {
                    group->markerIds.push_back(markerId);
                }
            }
        }
    }

    for (MarkerStickyGroup& group : stickyState_.groups) {
        int layoutIndex = 0;
        const std::vector<QString> targetIds = isAllStickyGroupId(group.id) ? markerIds : group.markerIds;
        for (const QString& markerId : targetIds) {
            MarkerStickyNoteLayout* layout = stickyLayoutForMarker(group, markerId, true);
            if (!layout) {
                continue;
            }
            if (layout->width <= 0.0) {
                layout->width = kStickyDefaultWidth;
            }
            if (layout->height <= 0.0) {
                layout->height = kStickyDefaultHeight;
            }
            if (layout->x == 0.0 && layout->y == 0.0 && layoutIndex > 0) {
                layout->x = 16.0 + (layoutIndex % kStickyColumns) * (kStickyDefaultWidth + 18.0);
                layout->y = 16.0 + (layoutIndex / kStickyColumns) * (kStickyDefaultHeight + 18.0);
            }
            ++layoutIndex;
        }
    }
}

void MarkerWidget::emitStickyStateChanged()
{
    Q_EMIT stickyStateChanged(stickyState_);
}

void MarkerWidget::rebuildFilteredMarkerIndices()
{
    filteredMarkerIndices_.clear();
    filteredMarkerIndices_.reserve(markers_.size());
    for (std::size_t index = 0; index < markers_.size(); ++index) {
        if (markerMatchesSearch(markers_[index])) {
            filteredMarkerIndices_.push_back(index);
        }
    }
}

bool MarkerWidget::markerMatchesSearch(const TraceMarker& marker) const
{
    if (!searchEdit_) {
        return true;
    }
    const QString query = searchEdit_->text().trimmed();
    if (query.isEmpty()) {
        return true;
    }
    return marker.label.contains(query, Qt::CaseInsensitive)
        || marker.memo.contains(query, Qt::CaseInsensitive);
}

const TraceMarkerDisplaySummary* MarkerWidget::markerSummaryById(const QString& markerId) const
{
    const auto found = markerSummaries_.constFind(markerId);
    return found == markerSummaries_.cend() ? nullptr : &found.value();
}

int MarkerWidget::rowForMarkerId(const QString& markerId) const
{
    if (markerId.isEmpty()) {
        return -1;
    }
    for (int row = 0; row < static_cast<int>(filteredMarkerIndices_.size()); ++row) {
        if (markers_[markerIndexForTableRow(row)].id == markerId) {
            return row;
        }
    }
    return -1;
}

std::size_t MarkerWidget::markerIndexForTableRow(const int row) const
{
    if (row < 0 || row >= static_cast<int>(filteredMarkerIndices_.size())) {
        return std::numeric_limits<std::size_t>::max();
    }
    return filteredMarkerIndices_[static_cast<std::size_t>(row)];
}

TraceMarker* MarkerWidget::markerById(const QString& markerId)
{
    const auto found = std::find_if(markers_.begin(), markers_.end(), [&](const TraceMarker& marker) {
        return marker.id == markerId;
    });
    return found == markers_.end() ? nullptr : &*found;
}

const TraceMarker* MarkerWidget::markerById(const QString& markerId) const
{
    const auto found = std::find_if(markers_.cbegin(), markers_.cend(), [&](const TraceMarker& marker) {
        return marker.id == markerId;
    });
    return found == markers_.cend() ? nullptr : &*found;
}

MarkerStickyGroup* MarkerWidget::activeStickyGroup()
{
    return stickyGroupById(stickyState_.activeGroupId);
}

const MarkerStickyGroup* MarkerWidget::activeStickyGroup() const
{
    return stickyGroupById(stickyState_.activeGroupId);
}

MarkerStickyGroup* MarkerWidget::stickyGroupById(const QString& groupId)
{
    const auto found = std::find_if(stickyState_.groups.begin(), stickyState_.groups.end(), [&](const MarkerStickyGroup& group) {
        return group.id == groupId;
    });
    return found == stickyState_.groups.end() ? nullptr : &*found;
}

const MarkerStickyGroup* MarkerWidget::stickyGroupById(const QString& groupId) const
{
    const auto found = std::find_if(stickyState_.groups.cbegin(), stickyState_.groups.cend(), [&](const MarkerStickyGroup& group) {
        return group.id == groupId;
    });
    return found == stickyState_.groups.cend() ? nullptr : &*found;
}

MarkerStickyNoteLayout* MarkerWidget::stickyLayoutForMarker(MarkerStickyGroup& group,
                                                            const QString& markerId,
                                                            const bool createIfMissing)
{
    const auto found = std::find_if(group.noteLayouts.begin(), group.noteLayouts.end(), [&](const MarkerStickyNoteLayout& layout) {
        return layout.markerId == markerId;
    });
    if (found != group.noteLayouts.end()) {
        return &*found;
    }
    if (!createIfMissing) {
        return nullptr;
    }
    const int index = static_cast<int>(group.noteLayouts.size());
    MarkerStickyNoteLayout layout;
    layout.markerId = markerId;
    layout.x = 16.0 + (index % kStickyColumns) * (kStickyDefaultWidth + 18.0);
    layout.y = 16.0 + (index / kStickyColumns) * (kStickyDefaultHeight + 18.0);
    layout.width = kStickyDefaultWidth;
    layout.height = kStickyDefaultHeight;
    group.noteLayouts.push_back(std::move(layout));
    return &group.noteLayouts.back();
}

MarkerStickyNoteLayout* MarkerWidget::stickyLayoutForActiveMarker(const QString& markerId, const bool createIfMissing)
{
    MarkerStickyGroup* group = activeStickyGroup();
    return group ? stickyLayoutForMarker(*group, markerId, createIfMissing) : nullptr;
}

bool MarkerWidget::activeStickyTabIsAll() const
{
    return isAllStickyGroupId(stickyState_.activeGroupId);
}

bool MarkerWidget::stickyGroupContainsMarker(const MarkerStickyGroup& group, const QString& markerId) const
{
    return containsMarkerId(group.markerIds, markerId);
}

void MarkerWidget::setMarkerSelection(QString markerId, const bool activate)
{
    if (!markerById(markerId)) {
        return;
    }
    selectedMarkerId_ = std::move(markerId);
    refreshMemo();
    refreshTable();
    refreshSticky();
    deleteButton_->setEnabled(true);
    Q_EMIT markerSelected(selectedMarkerId_);
    if (activate) {
        Q_EMIT markerActivated(selectedMarkerId_);
    }
}

void MarkerWidget::applyStickyMarkerEdit(const QString& markerId, const QString& label, const QString& memo)
{
    TraceMarker* marker = markerById(markerId);
    if (!marker) {
        return;
    }
    const QString normalizedLabel = label.trimmed().isEmpty()
        ? QStringLiteral("Marker %1").arg(marker->logicalRow + 1)
        : label.trimmed();
    if (marker->label == normalizedLabel && marker->memo == memo) {
        return;
    }
    marker->label = normalizedLabel;
    marker->memo = memo;
    marker->updatedAt = QDateTime::currentDateTimeUtc();
    Q_EMIT markerEdited(*marker);
}

QRectF MarkerWidget::snapStickyRectToGrid(const QRectF& candidateRect, const bool resizeOnly) const
{
    QPointF topLeft = candidateRect.topLeft();
    QSizeF size(std::max(kStickyMinWidth, candidateRect.width()),
                std::max(kStickyMinHeight, candidateRect.height()));
    if (!resizeOnly) {
        topLeft.setX(snapToStickyGrid(topLeft.x()));
        topLeft.setY(snapToStickyGrid(topLeft.y()));
    }
    size.setWidth(std::max(kStickyMinWidth, snapToStickyGrid(size.width())));
    size.setHeight(std::max(kStickyMinHeight, snapToStickyGrid(size.height())));
    return QRectF(topLeft, size);
}

QRectF MarkerWidget::snapStickyRectToNeighbors(const QString& markerId, const QRectF& candidateRect) const
{
    if (!stickyScene_) {
        return candidateRect;
    }

    qreal bestDx = 0.0;
    qreal bestDy = 0.0;
    qreal bestAbsDx = kStickyMagneticSnapThreshold + 1.0;
    qreal bestAbsDy = kStickyMagneticSnapThreshold + 1.0;

    const auto considerDx = [&](const qreal delta) {
        const qreal distance = std::abs(delta);
        if (distance <= kStickyMagneticSnapThreshold && distance < bestAbsDx) {
            bestDx = delta;
            bestAbsDx = distance;
        }
    };
    const auto considerDy = [&](const qreal delta) {
        const qreal distance = std::abs(delta);
        if (distance <= kStickyMagneticSnapThreshold && distance < bestAbsDy) {
            bestDy = delta;
            bestAbsDy = distance;
        }
    };

    for (QGraphicsItem* item : stickyScene_->items()) {
        const auto* note = dynamic_cast<const StickyNoteItem*>(item);
        if (!note || note->markerId() == markerId) {
            continue;
        }
        const QRectF other = note->sceneNoteRect();
        const qreal candidateX[] = {
            candidateRect.left(),
            candidateRect.right(),
            candidateRect.center().x()
        };
        const qreal otherX[] = {
            other.left(),
            other.right(),
            other.center().x()
        };
        for (const qreal source : candidateX) {
            for (const qreal target : otherX) {
                considerDx(target - source);
            }
        }

        const qreal candidateY[] = {
            candidateRect.top(),
            candidateRect.bottom(),
            candidateRect.center().y()
        };
        const qreal otherY[] = {
            other.top(),
            other.bottom(),
            other.center().y()
        };
        for (const qreal source : candidateY) {
            for (const qreal target : otherY) {
                considerDy(target - source);
            }
        }
    }

    return candidateRect.translated(bestDx, bestDy);
}

QRectF MarkerWidget::snapStickyResizeRectToNeighbors(const QString& markerId, const QRectF& candidateRect) const
{
    if (!stickyScene_) {
        return candidateRect;
    }

    qreal bestWidth = candidateRect.width();
    qreal bestHeight = candidateRect.height();
    qreal bestAbsDw = kStickyMagneticSnapThreshold + 1.0;
    qreal bestAbsDh = kStickyMagneticSnapThreshold + 1.0;

    const auto considerWidthForTarget = [&](const qreal targetX) {
        const qreal newWidth = targetX - candidateRect.left();
        if (newWidth < kStickyMinWidth) {
            return;
        }
        const qreal distance = std::abs(newWidth - candidateRect.width());
        if (distance <= kStickyMagneticSnapThreshold && distance < bestAbsDw) {
            bestWidth = newWidth;
            bestAbsDw = distance;
        }
    };
    const auto considerHeightForTarget = [&](const qreal targetY) {
        const qreal newHeight = targetY - candidateRect.top();
        if (newHeight < kStickyMinHeight) {
            return;
        }
        const qreal distance = std::abs(newHeight - candidateRect.height());
        if (distance <= kStickyMagneticSnapThreshold && distance < bestAbsDh) {
            bestHeight = newHeight;
            bestAbsDh = distance;
        }
    };

    for (QGraphicsItem* item : stickyScene_->items()) {
        const auto* note = dynamic_cast<const StickyNoteItem*>(item);
        if (!note || note->markerId() == markerId) {
            continue;
        }
        const QRectF other = note->sceneNoteRect();
        considerWidthForTarget(other.left());
        considerWidthForTarget(other.right());
        considerWidthForTarget(other.center().x());
        considerHeightForTarget(other.top());
        considerHeightForTarget(other.bottom());
        considerHeightForTarget(other.center().y());
    }

    return QRectF(candidateRect.topLeft(), QSizeF(bestWidth, bestHeight));
}

void MarkerWidget::updateStickyMarkerLayout(const QString& markerId, const QPointF& position, const QSizeF& size)
{
    MarkerStickyNoteLayout* layout = stickyLayoutForActiveMarker(markerId, true);
    if (!layout) {
        return;
    }
    layout->x = position.x();
    layout->y = position.y();
    layout->width = std::max(kStickyMinWidth, size.width());
    layout->height = std::max(kStickyMinHeight, size.height());
    emitStickyStateChanged();
}

QString MarkerWidget::createStickyGroup(QString name)
{
    name = name.trimmed();
    if (name.isEmpty()) {
        return QString();
    }
    MarkerStickyGroup group;
    group.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    group.name = std::move(name);
    stickyState_.groups.push_back(std::move(group));
    stickyState_.activeGroupId = stickyState_.groups.back().id;
    refreshStickyTabs();
    refreshSticky();
    emitStickyStateChanged();
    return stickyState_.activeGroupId;
}

int MarkerWidget::customStickyGroupMembershipCount(const QString& markerId) const
{
    int count = 0;
    for (const MarkerStickyGroup& group : stickyState_.groups) {
        if (!isAllStickyGroupId(group.id) && stickyGroupContainsMarker(group, markerId)) {
            ++count;
        }
    }
    return count;
}

bool MarkerWidget::canRemoveStickyMarkerFromActiveGroup(const QString& markerId) const
{
    const MarkerStickyGroup* group = activeStickyGroup();
    return group
        && !isAllStickyGroupId(group->id)
        && stickyGroupContainsMarker(*group, markerId);
}

void MarkerWidget::removeStickyMarkerFromGroup(MarkerStickyGroup& group, const QString& markerId)
{
    group.markerIds.erase(std::remove(group.markerIds.begin(), group.markerIds.end(), markerId),
                          group.markerIds.end());
    group.noteLayouts.erase(std::remove_if(group.noteLayouts.begin(),
                                           group.noteLayouts.end(),
                                           [&](const MarkerStickyNoteLayout& layout) {
                                               return layout.markerId == markerId;
                                           }),
                            group.noteLayouts.end());
}

bool MarkerWidget::removeStickyMarkerFromActiveGroup(const QString& markerId)
{
    if (!canRemoveStickyMarkerFromActiveGroup(markerId)) {
        return false;
    }
    MarkerStickyGroup* group = activeStickyGroup();
    if (!group) {
        return false;
    }
    removeStickyMarkerFromGroup(*group, markerId);
    refreshSticky();
    emitStickyStateChanged();
    return true;
}

bool MarkerWidget::copyStickyMarkerToGroup(const QString& markerId, const QString& groupId)
{
    if (!markerById(markerId)) {
        return false;
    }
    MarkerStickyGroup* group = stickyGroupById(groupId);
    if (!group || isAllStickyGroupId(group->id)) {
        return false;
    }
    if (!stickyGroupContainsMarker(*group, markerId)) {
        group->markerIds.push_back(markerId);
    }
    stickyLayoutForMarker(*group, markerId, true);
    refreshSticky();
    emitStickyStateChanged();
    return true;
}

bool MarkerWidget::moveStickyMarkerToGroup(const QString& markerId, const QString& groupId)
{
    if (!markerById(markerId)) {
        return false;
    }
    const QString sourceGroupId = stickyState_.activeGroupId;
    MarkerStickyGroup* targetGroup = stickyGroupById(groupId);
    if (!targetGroup || isAllStickyGroupId(targetGroup->id)) {
        return false;
    }
    if (!stickyGroupContainsMarker(*targetGroup, markerId)) {
        targetGroup->markerIds.push_back(markerId);
    }
    stickyLayoutForMarker(*targetGroup, markerId, true);
    if (!isAllStickyGroupId(sourceGroupId) && sourceGroupId != groupId) {
        if (MarkerStickyGroup* sourceGroup = stickyGroupById(sourceGroupId)) {
            removeStickyMarkerFromGroup(*sourceGroup, markerId);
        }
    }
    refreshSticky();
    emitStickyStateChanged();
    return true;
}

bool MarkerWidget::copyOrMoveStickyMarkerToNewGroup(const QString& markerId,
                                                    const bool move,
                                                    const QString& requestedName)
{
    if (!markerById(markerId)) {
        return false;
    }
    QString name = requestedName.trimmed();
    if (name.isEmpty()) {
        bool ok = false;
        name = QInputDialog::getText(this,
                                     QStringLiteral("New Marker Group"),
                                     QStringLiteral("Group name"),
                                     QLineEdit::Normal,
                                     QStringLiteral("Group %1").arg(stickyState_.groups.size()),
                                     &ok)
                   .trimmed();
        if (!ok || name.isEmpty()) {
            return false;
        }
    }

    const QString sourceGroupId = stickyState_.activeGroupId;
    const QString newGroupId = createStickyGroup(name);
    if (newGroupId.isEmpty()) {
        return false;
    }
    MarkerStickyGroup* newGroup = stickyGroupById(newGroupId);
    if (!newGroup) {
        return false;
    }
    if (!stickyGroupContainsMarker(*newGroup, markerId)) {
        newGroup->markerIds.push_back(markerId);
    }
    stickyLayoutForMarker(*newGroup, markerId, true);
    if (move && !isAllStickyGroupId(sourceGroupId) && sourceGroupId != newGroupId) {
        if (MarkerStickyGroup* sourceGroup = stickyGroupById(sourceGroupId)) {
            removeStickyMarkerFromGroup(*sourceGroup, markerId);
        }
    }
    stickyState_.activeGroupId = newGroupId;
    refreshStickyTabs();
    refreshSticky();
    emitStickyStateChanged();
    return true;
}

bool MarkerWidget::stickyNoteVisibleInActiveView(const QString& markerId) const
{
    const TraceMarker* marker = markerById(markerId);
    const MarkerStickyGroup* group = activeStickyGroup();
    if (!marker || !group || !markerMatchesSearch(*marker)) {
        return false;
    }
    return activeStickyTabIsAll() || stickyGroupContainsMarker(*group, markerId);
}

bool MarkerWidget::stickyTextEditorHasFocus() const
{
#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
    if (stickyTextEditingForTest_) {
        return true;
    }
#endif
    return stickyScene_ && dynamic_cast<QGraphicsTextItem*>(stickyScene_->focusItem()) != nullptr;
}

bool MarkerWidget::deleteSelectedStickyNote()
{
    if (selectedMarkerId_.isEmpty()
        || stickyTextEditorHasFocus()
        || !stickyNoteVisibleInActiveView(selectedMarkerId_)) {
        return false;
    }
    commitMemoEdit();
    Q_EMIT markerRemoved(selectedMarkerId_);
    return true;
}

void MarkerWidget::addStickyGroup()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("New Marker Group"),
                                               QStringLiteral("Group name"),
                                               QLineEdit::Normal,
                                               QStringLiteral("Group %1").arg(stickyState_.groups.size()),
                                               &ok)
                             .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    createStickyGroup(name);
}

void MarkerWidget::renameActiveStickyGroup()
{
    MarkerStickyGroup* group = activeStickyGroup();
    if (!group || isAllStickyGroupId(group->id)) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("Rename Marker Group"),
                                               QStringLiteral("Group name"),
                                               QLineEdit::Normal,
                                               group->name,
                                               &ok)
                             .trimmed();
    if (!ok || name.isEmpty() || name == group->name) {
        return;
    }
    group->name = name;
    refreshStickyTabs();
    emitStickyStateChanged();
}

void MarkerWidget::deleteActiveStickyGroup()
{
    if (activeStickyTabIsAll()) {
        return;
    }
    stickyState_.groups.erase(std::remove_if(stickyState_.groups.begin(),
                                             stickyState_.groups.end(),
                                             [&](const MarkerStickyGroup& group) {
                                                 return group.id == stickyState_.activeGroupId;
                                             }),
                              stickyState_.groups.end());
    stickyState_.activeGroupId = QString::fromLatin1(kAllStickyGroupId);
    refreshStickyTabs();
    refreshSticky();
    emitStickyStateChanged();
}

void MarkerWidget::showAddMarkerToStickyGroupMenu()
{
    MarkerStickyGroup* group = activeStickyGroup();
    if (!group || activeStickyTabIsAll()) {
        return;
    }
    QMenu menu(this);
    for (const TraceMarker& marker : markers_) {
        if (stickyGroupContainsMarker(*group, marker.id)) {
            continue;
        }
        QAction* action = menu.addAction(marker.label);
        action->setData(marker.id);
    }
    if (menu.isEmpty()) {
        QAction* emptyAction = menu.addAction(QStringLiteral("No markers to add"));
        emptyAction->setEnabled(false);
    }
    QAction* chosen = menu.exec(stickyAddMarkerButton_->mapToGlobal(QPoint(0, stickyAddMarkerButton_->height())));
    if (!chosen || !chosen->isEnabled()) {
        return;
    }
    const QString markerId = chosen->data().toString();
    if (!markerId.isEmpty() && !stickyGroupContainsMarker(*group, markerId)) {
        group->markerIds.push_back(markerId);
        stickyLayoutForMarker(*group, markerId, true);
        refreshSticky();
        emitStickyStateChanged();
    }
}

void MarkerWidget::removeSelectedMarkerFromStickyGroup()
{
    removeStickyMarkerFromActiveGroup(selectedMarkerId_);
}

void MarkerWidget::updateSelectedMarkerFromTable()
{
    const QList<QTableWidgetItem*> items = table_->selectedItems();
    const QString markerId = items.isEmpty() ? QString() : items.front()->data(kMarkerIdRole).toString();
    if (markerId == selectedMarkerId_) {
        return;
    }
    commitMemoEdit();
    selectedMarkerId_ = markerId;
    refreshMemo();
    deleteButton_->setEnabled(!selectedMarkerId_.isEmpty());
    if (!selectedMarkerId_.isEmpty()) {
        Q_EMIT markerSelected(selectedMarkerId_);
    }
}

void MarkerWidget::applyItemEdit(QTableWidgetItem* item)
{
    if (!item || item->column() != LabelColumn) {
        return;
    }
    commitMemoEdit();
    TraceMarker* marker = markerById(item->data(kMarkerIdRole).toString());
    if (!marker) {
        return;
    }
    const QString label = item->text().trimmed();
    marker->label = label.isEmpty() ? QStringLiteral("Marker %1").arg(marker->logicalRow + 1) : label;
    marker->updatedAt = QDateTime::currentDateTimeUtc();
    if (marker->label != item->text()) {
        const QSignalBlocker blocker(table_);
        item->setText(marker->label);
    }
    Q_EMIT markerEdited(*marker);
}

}  // namespace CHIron::Gui
