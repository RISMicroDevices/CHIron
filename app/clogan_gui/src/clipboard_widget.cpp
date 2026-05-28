#include "clipboard_widget.hpp"

#include "channel_badge_painter.hpp"
#include "gui_format.hpp"
#include "trace_cache_line_minimap.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace CHIron::Gui {
namespace {

bool FieldEntriesEqual(const FieldEntry& lhs, const FieldEntry& rhs)
{
    return FieldScopeText(lhs) == FieldScopeText(rhs)
        && FieldNameText(lhs) == FieldNameText(rhs)
        && lhs.value == rhs.value
        && lhs.raw == rhs.raw;
}

bool FlitRecordsEqual(const FlitRecord& lhs, const FlitRecord& rhs)
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

int ClipboardColumnWidth(const int logicalColumn)
{
    switch (logicalColumn) {
    case FlitTableModel::XactionIndexColumn:
        return 48;
    case FlitTableModel::TimeColumn:
        return 92;
    case FlitTableModel::ChannelColumn:
        return 76;
    case FlitTableModel::DirectionColumn:
        return 82;
    case FlitTableModel::OpcodeColumn:
        return 176;
    case FlitTableModel::SourceColumn:
    case FlitTableModel::TargetColumn:
    case FlitTableModel::TxnColumn:
    case FlitTableModel::DataIdColumn:
        return 76;
    case FlitTableModel::AddressColumn:
        return 156;
    case FlitTableModel::RespColumn:
    case FlitTableModel::RespErrColumn:
        return 112;
    case FlitTableModel::FwdStateColumn:
        return 120;
    default:
        return 120;
    }
}

int ClipboardRowHeightForVisibleRows(const int visibleRowCount)
{
    constexpr int kPreferredRowHeight = 22;
    constexpr qint64 kPixelBudget = static_cast<qint64>((std::numeric_limits<int>::max)()) - 65536;

    if (visibleRowCount <= 0) {
        return kPreferredRowHeight;
    }

    const qint64 safeHeight = kPixelBudget / static_cast<qint64>(visibleRowCount);
    return qBound(1, static_cast<int>(safeHeight), kPreferredRowHeight);
}

class ClipboardChannelBadgeDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        const QString text = index.data(Qt::DisplayRole).toString();
        const QString tag = index.data(FlitTableModel::ChannelTagRole).toString();
        QFont badgeFont(option.font);
        const qreal fontScale = index.data(FlitTableModel::BadgeFontScaleRole).toReal();
        badgeFont = ChannelBadgeScaledFont(badgeFont, fontScale);
        const QFontMetrics metrics(badgeFont);
        return ChannelBadgeSizeHint(base, metrics, text, tag);
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem base(option);
        initStyleOption(&base, index);

        const QString text = base.text;
        base.text.clear();
        if (const QWidget* widget = base.widget) {
            widget->style()->drawControl(QStyle::CE_ItemViewItem, &base, painter, widget);
        } else {
            QStyledItemDelegate::paint(painter, base, index);
        }

        const QColor accent = index.data(FlitTableModel::ChannelAccentRole).value<QColor>();
        if (!accent.isValid()) {
            return;
        }
        const QString tag = index.data(FlitTableModel::ChannelTagRole).toString();
        QFont badgeFont(option.font);
        const qreal fontScale = index.data(FlitTableModel::BadgeFontScaleRole).toReal();
        badgeFont = ChannelBadgeScaledFont(badgeFont, fontScale);
        PaintChannelBadge(painter,
                          ChannelBadgeContentRect(option.rect),
                          badgeFont,
                          text,
                          tag,
                          accent,
                          option.state.testFlag(QStyle::State_Selected),
                          index.data(FlitTableModel::TransactionHighlightRole).toBool(),
                          index.data(FlitTableModel::BadgeLeftAlignedRole).toBool());
    }
};

class ClipboardXactionIndexDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        return QSize(qMax(base.width(), 34), base.height());
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem base(option);
        initStyleOption(&base, index);
        base.text.clear();
        if (const QWidget* widget = base.widget) {
            widget->style()->drawControl(QStyle::CE_ItemViewItem, &base, painter, widget);
        } else {
            QStyledItemDelegate::paint(painter, base, index);
        }

        QColor fill(QStringLiteral("#AEB4BE"));
        switch (static_cast<XactionIndexState>(index.data(FlitTableModel::XactionIndexStateRole).toInt())) {
        case XactionIndexState::NotStarted:
            fill = QColor(QStringLiteral("#AEB4BE"));
            break;
        case XactionIndexState::Indexed:
            fill = QColor(QStringLiteral("#35C759"));
            break;
        case XactionIndexState::Denied:
            fill = QColor(QStringLiteral("#E53935"));
            break;
        case XactionIndexState::Pending:
            fill = QColor(QStringLiteral("#F4C542"));
            break;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const int diameter = qMin(8, qMax(5, option.rect.height() - 14));
        const QRect circleRect(option.rect.center().x() - diameter / 2,
                               option.rect.center().y() - diameter / 2,
                               diameter,
                               diameter);
        painter->setPen(QPen(QColor(QStringLiteral("#000000")), 1));
        painter->setBrush(fill);
        painter->drawEllipse(circleRect);
        painter->restore();
    }
};

class ClipboardNodeLabelDelegate final : public QStyledItemDelegate {
public:
    explicit ClipboardNodeLabelDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        const QString label = index.data(FlitTableModel::NodeLabelTextRole).toString();
        if (label.isEmpty()) {
            return base;
        }
        QFont badgeFont(option.font);
        const qreal fontScale = index.data(FlitTableModel::BadgeFontScaleRole).toReal();
        if (fontScale > 0.0 && fontScale < 1.0) {
            badgeFont.setPointSizeF(qMax<qreal>(6.0, badgeFont.pointSizeF() * fontScale));
        }
        const QFontMetrics metrics(badgeFont);
        const int labelWidth = metrics.horizontalAdvance(label) + 16;
        return QSize(std::max({base.width(), 52 + 6 + labelWidth + 12, 168}), base.height());
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const QString label = index.data(FlitTableModel::NodeLabelTextRole).toString();
        if (label.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem base(option);
        initStyleOption(&base, index);
        const QString text = base.text;
        base.text.clear();
        if (const QWidget* widget = base.widget) {
            widget->style()->drawControl(QStyle::CE_ItemViewItem, &base, painter, widget);
        } else {
            QStyledItemDelegate::paint(painter, base, index);
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QFont badgeFont(option.font);
        const qreal fontScale = index.data(FlitTableModel::BadgeFontScaleRole).toReal();
        if (fontScale > 0.0 && fontScale < 1.0) {
            badgeFont.setPointSizeF(qMax<qreal>(6.0, badgeFont.pointSizeF() * fontScale));
        }
        painter->setFont(badgeFont);
        const QFontMetrics metrics(badgeFont);
        const QRect contentRect = option.rect.adjusted(6, 4, -6, -4);
        const int labelWidth = metrics.horizontalAdvance(label) + 16;
        const int valueAreaWidth = qMin(52, qMax(0, contentRect.width() - 6 - labelWidth));
        const int labelLeft = contentRect.left() + valueAreaWidth + 6;
        const QRect textRect(contentRect.left(), contentRect.top(), qMax(0, valueAreaWidth), contentRect.height());
        const int remainingWidth = qMax(0, contentRect.right() - labelLeft + 1);
        QRect tagRect;
        if (remainingWidth > metrics.horizontalAdvance(QStringLiteral("MN")) + 18) {
            tagRect = QRect(labelLeft,
                            contentRect.top() + 1,
                            qMin(labelWidth, remainingWidth),
                            qMax(12, contentRect.height() - 2));
        }

        painter->setPen(option.state.testFlag(QStyle::State_Selected)
                            ? QColor(QStringLiteral("#FFFFFF"))
                            : QColor(QStringLiteral("#5F6976")));
        painter->drawText(textRect,
                          Qt::AlignRight | Qt::AlignVCenter,
                          metrics.elidedText(text, Qt::ElideLeft, textRect.width()));
        if (!tagRect.isNull()) {
            painter->setPen(Qt::NoPen);
            const QColor color = index.data(FlitTableModel::NodeLabelColorRole).value<QColor>();
            painter->setBrush(color.isValid() ? color : QColor(QStringLiteral("#8B949E")));
            painter->drawRoundedRect(tagRect, 6.0, 6.0);
            painter->setPen(label == QLatin1String("Before SAM")
                                ? QColor(QStringLiteral("#3F4752"))
                                : QColor(QStringLiteral("#FFFFFF")));
            painter->drawText(tagRect, Qt::AlignCenter, label);
        }
        painter->restore();
    }
};

}  // namespace

ClipboardWidget::ClipboardWidget(QWidget* parent)
    : QWidget(parent)
    , model_(new FlitTableModel(this))
{
    model_->setEditable(true);
    model_->markUndoStackClean();
    buildUi();
    wireSignals();
    setReadOnly(true);
    setToolbarFolded(true);
    updateBadges();
    updateScopeButton();
    updateModeButton();
    updateSearchModeButton();
}

FlitTableModel* ClipboardWidget::model() noexcept
{
    return model_;
}

const FlitTableModel* ClipboardWidget::model() const noexcept
{
    return model_;
}

QTableView* ClipboardWidget::tableView() noexcept
{
    return table_;
}

const QTableView* ClipboardWidget::tableView() const noexcept
{
    return table_;
}

void ClipboardWidget::setEntries(std::vector<ClipboardEntry>* entries, const CLogBTraceMetadata* metadata)
{
    syncEntriesFromModel();
    entries_ = entries;
    if (model_) {
        model_->setTraceMetadataOverride(metadata ? std::optional<CLogBTraceMetadata>(*metadata) : std::nullopt);
    }
    refreshFromEntries();
}

std::vector<ClipboardEntry>* ClipboardWidget::entries() noexcept
{
    return entries_;
}

const std::vector<ClipboardEntry>* ClipboardWidget::entries() const noexcept
{
    return entries_;
}

void ClipboardWidget::refreshFromEntries()
{
    syncingModel_ = true;
    std::vector<FlitRecord> rows;
    if (entries_) {
        rows.reserve(entries_->size());
        for (const ClipboardEntry& entry : *entries_) {
            rows.push_back(entry.record);
        }
    }
    model_->setRows(std::move(rows));
    model_->setEditable(!readOnly_);
    model_->markUndoStackClean();
    syncingModel_ = false;
    modifiedBadgeDirty_ = true;
    updateBadges();
    applyTraceTableRowStyle(model_->visibleCount());
    resizeColumnsToTraceDefaults();
}

void ClipboardWidget::beginIncrementalRefreshFromEntries(std::vector<ClipboardEntry>* entries,
                                                         const CLogBTraceMetadata* metadata,
                                                         const bool entriesAlreadySynced)
{
    const bool preserveModifiedCache = entriesAlreadySynced && !modifiedBadgeDirty_;
    if (!entriesAlreadySynced) {
        syncEntriesFromModel();
    }
    entries_ = entries;
    syncingModel_ = true;
    if (table_) {
        table_->setUpdatesEnabled(false);
    }
    if (cacheMinimap_) {
        cacheMinimap_->setModelUpdatesSuspended(true);
    }
    if (model_) {
        model_->setTraceMetadataOverride(metadata ? std::optional<CLogBTraceMetadata>(*metadata) : std::nullopt);
        model_->setRows({});
        model_->setEditable(!readOnly_);
        model_->markUndoStackClean();
    }
    modifiedBadgeDirty_ = !preserveModifiedCache;
}

void ClipboardWidget::beginAppendRefreshFromEntries(std::vector<ClipboardEntry>* entries,
                                                    const CLogBTraceMetadata* metadata,
                                                    const std::size_t appendBeginIndex)
{
    if (!entries || !model_ || entries_ != entries) {
        beginIncrementalRefreshFromEntries(entries, metadata, true);
        return;
    }
    syncDirtyEntriesFromModel();
    const int modelRows = model_->totalCount();
    if (appendBeginIndex != static_cast<std::size_t>(modelRows)
        || appendBeginIndex > entries->size()) {
        beginIncrementalRefreshFromEntries(entries, metadata, true);
        return;
    }
    entries_ = entries;
    syncingModel_ = true;
    if (table_) {
        table_->setUpdatesEnabled(false);
    }
    if (cacheMinimap_) {
        cacheMinimap_->setModelUpdatesSuspended(true);
    }
    model_->setTraceMetadataOverride(metadata ? std::optional<CLogBTraceMetadata>(*metadata) : std::nullopt);
    model_->setEditable(!readOnly_);
    model_->markUndoStackClean();
    modifiedBadgeDirty_ = false;
}

bool ClipboardWidget::appendEntryRowsIncrementally(const std::size_t beginIndex,
                                                   const std::size_t count)
{
    if (!entries_ || !model_ || count == 0 || beginIndex >= entries_->size()) {
        return false;
    }

    const std::size_t endIndex = std::min(entries_->size(), beginIndex + count);
    std::vector<FlitRecord> rows;
    rows.reserve(endIndex - beginIndex);
    for (std::size_t index = beginIndex; index < endIndex; ++index) {
        rows.push_back((*entries_)[index].record);
    }
    if (!model_->appendRowsIncrementally(rows)) {
        return false;
    }
    return true;
}

void ClipboardWidget::abortIncrementalRefreshFromEntries()
{
    syncingModel_ = false;
    if (cacheMinimap_) {
        cacheMinimap_->setModelUpdatesSuspended(false);
    }
    if (table_) {
        table_->setUpdatesEnabled(true);
        table_->viewport()->update();
    }
}

void ClipboardWidget::finishIncrementalRefreshFromEntries()
{
    if (model_) {
        model_->setEditable(!readOnly_);
        model_->markUndoStackClean();
    }
    syncingModel_ = false;
    updateBadges();
    applyTraceTableRowStyle(model_ ? model_->visibleCount() : 0);
    resizeColumnsToTraceDefaults();
    if (cacheMinimap_) {
        cacheMinimap_->setModelUpdatesSuspended(false);
    }
    if (table_) {
        table_->setUpdatesEnabled(true);
        table_->viewport()->update();
    }
}

void ClipboardWidget::syncEntriesFromModel()
{
    if (!entries_ || syncingModel_) {
        return;
    }
    const std::vector<FlitRecord> rows = model_->sourceRowsSnapshot();
    if (rows.size() == entries_->size()) {
        for (std::size_t index = 0; index < rows.size(); ++index) {
            (*entries_)[index].record = rows[index];
        }
    }
    modifiedBadgeDirty_ = true;
    updateBadges();
}

void ClipboardWidget::syncDirtyEntriesFromModel()
{
    if (!entries_ || syncingModel_ || !model_) {
        return;
    }
    if (modifiedBadgeDirty_) {
        (void)hasModifiedRows();
    }
    const std::vector<std::pair<int, FlitRecord>> rows = model_->takeDirtySourceRowsSnapshot();
    if (rows.empty()) {
        return;
    }
    bool shouldRescanModifiedRows = hasModifiedRowsCache_;
    for (const auto& [logicalRow, row] : rows) {
        if (logicalRow >= 0 && logicalRow < static_cast<int>(entries_->size())) {
            (*entries_)[static_cast<std::size_t>(logicalRow)].record = row;
            const bool modified = entryModified(logicalRow);
            hasModifiedRowsCache_ = hasModifiedRowsCache_ || modified;
            shouldRescanModifiedRows = shouldRescanModifiedRows && !modified;
        }
    }
    if (shouldRescanModifiedRows && entries_->size() > rows.size()) {
        hasModifiedRowsCache_ = true;
        shouldRescanModifiedRows = false;
    }
    modifiedBadgeDirty_ = shouldRescanModifiedRows;
    updateBadges();
}

void ClipboardWidget::applyTraceTableRowStyle(const int referenceVisibleRowCount)
{
    if (!table_ || !table_->verticalHeader()) {
        return;
    }
    const int rowHeight = ClipboardRowHeightForVisibleRows(referenceVisibleRowCount);
    table_->verticalHeader()->setDefaultSectionSize(rowHeight);
    table_->verticalHeader()->setMinimumSectionSize(rowHeight);
}

void ClipboardWidget::resizeColumnsToTraceDefaults()
{
    if (!table_ || !model_) {
        return;
    }
    const QFontMetrics metrics(table_->font());
    for (int column = 0; column < model_->columnCount(); ++column) {
        const QString headerText = model_->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        table_->setColumnWidth(column, qMax(ClipboardColumnWidth(column), metrics.horizontalAdvance(headerText) + 26));
    }
}

void ClipboardWidget::setReadOnly(const bool readOnly)
{
    readOnly_ = readOnly;
    if (model_) {
        model_->setEditable(!readOnly_);
        model_->markUndoStackClean();
    }
    updateModeButton();
}

bool ClipboardWidget::readOnly() const noexcept
{
    return readOnly_;
}

bool ClipboardWidget::hasModifiedRows() const
{
    if (!entries_) {
        return false;
    }
    if (!modifiedBadgeDirty_) {
        return hasModifiedRowsCache_;
    }
    bool modified = false;
    for (std::size_t index = 0; index < entries_->size(); ++index) {
        if (entryModified(static_cast<int>(index))) {
            modified = true;
            break;
        }
    }
    auto* self = const_cast<ClipboardWidget*>(this);
    self->hasModifiedRowsCache_ = modified;
    self->modifiedBadgeDirty_ = false;
    return modified;
}

bool ClipboardWidget::entryModified(const int logicalRow) const
{
    if (!entries_ || logicalRow < 0 || logicalRow >= static_cast<int>(entries_->size())) {
        return false;
    }
    const ClipboardEntry& entry = (*entries_)[static_cast<std::size_t>(logicalRow)];
    return !FlitRecordsEqual(entry.record, entry.originalRecord);
}

std::vector<ClipboardEntry> ClipboardWidget::currentEntriesSnapshot() const
{
    return entries_ ? *entries_ : std::vector<ClipboardEntry>{};
}

std::optional<ClipboardEntry> ClipboardWidget::entryForVisibleRow(const int visibleRow) const
{
    if (!entries_ || !model_) {
        return std::nullopt;
    }
    const int logicalRow = model_->logicalRowAt(visibleRow);
    if (logicalRow < 0 || logicalRow >= static_cast<int>(entries_->size())) {
        return std::nullopt;
    }
    return (*entries_)[static_cast<std::size_t>(logicalRow)];
}

void ClipboardWidget::setCacheMinimap(TraceCacheLineMinimap* minimap)
{
    cacheMinimap_ = minimap;
    if (cacheMapButton_) {
        cacheMapButton_->setEnabled(cacheMinimap_ != nullptr);
    }
    if (cacheMapAddButton_) {
        cacheMapAddButton_->setEnabled(cacheMinimap_ != nullptr);
    }
    if (cacheMapFadeButton_) {
        cacheMapFadeButton_->setEnabled(cacheMinimap_ != nullptr);
    }
    if (cacheMinimap_) {
        connect(cacheMinimap_, &TraceCacheLineMinimap::mapVisibilityChanged,
                this, [this](const bool visible) {
                    if (!cacheMapButton_) {
                        return;
                    }
                    const QSignalBlocker blocker(cacheMapButton_);
                    cacheMapButton_->setChecked(visible);
                    cacheMapButton_->setText(visible ? QStringLiteral("Hide")
                                                     : QStringLiteral("Show"));
                });
    }
}

void ClipboardWidget::setScope(const ClipboardScope scope)
{
    if (scope_ == scope) {
        return;
    }
    syncEntriesFromModel();
    scope_ = scope;
    updateScopeButton();
    Q_EMIT scopeChanged(scope_);
}

ClipboardScope ClipboardWidget::scope() const noexcept
{
    return scope_;
}

bool ClipboardWidget::toolbarFolded() const noexcept
{
    return toolbarFolded_;
}

void ClipboardWidget::setToolbarFolded(const bool folded)
{
    toolbarFolded_ = folded;
    if (toolbarContent_) {
        toolbarContent_->setVisible(!folded);
    }
    if (foldButton_) {
        const QSignalBlocker blocker(foldButton_);
        foldButton_->setChecked(folded);
        foldButton_->setArrowType(folded ? Qt::DownArrow : Qt::UpArrow);
        const QString hint = folded
            ? QStringLiteral("Unfold the Clipboard toolbar.")
            : QStringLiteral("Fold the Clipboard toolbar.");
        foldButton_->setToolTip(hint);
        foldButton_->setStatusTip(hint);
    }
}

void ClipboardWidget::deleteSelectedRows()
{
    if (!table_ || !model_ || !table_->selectionModel()) {
        return;
    }
    QModelIndexList selectedRows = table_->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        const QModelIndex current = table_->selectionModel()->currentIndex();
        if (current.isValid()) {
            selectedRows.push_back(current.siblingAtColumn(0));
        }
    }
    QVector<int> logicalRows;
    logicalRows.reserve(selectedRows.size());
    for (const QModelIndex& selected : selectedRows) {
        const int logicalRow = model_->logicalRowAt(selected.row());
        if (logicalRow >= 0) {
            logicalRows.append(logicalRow);
        }
    }
    std::sort(selectedRows.begin(), selectedRows.end(), [](const QModelIndex& lhs, const QModelIndex& rhs) {
        return lhs.row() > rhs.row();
    });
    std::sort(logicalRows.begin(), logicalRows.end(), std::greater<int>());

    syncingModel_ = true;
    QVector<int> visibleRows;
    visibleRows.reserve(selectedRows.size());
    for (const QModelIndex& selected : selectedRows) {
        visibleRows.append(selected.row());
    }
    model_->removeRowsAt(visibleRows);
    syncingModel_ = false;

    if (entries_) {
        int previous = -1;
        for (const int logicalRow : std::as_const(logicalRows)) {
            if (logicalRow == previous) {
                continue;
            }
            previous = logicalRow;
            if (logicalRow >= 0 && logicalRow < static_cast<int>(entries_->size())) {
                entries_->erase(entries_->begin() + logicalRow);
            }
        }
    }
    modifiedBadgeDirty_ = true;
    syncEntriesFromModel();
    Q_EMIT entriesEdited();
}

void ClipboardWidget::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* toolbarFrame = new QFrame(this);
    toolbar_ = toolbarFrame;
    toolbarFrame->setObjectName(QStringLiteral("traceToolbar"));
    auto* toolbarLayout = new QVBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(8, 4, 8, 6);
    toolbarLayout->setSpacing(4);

    auto* summaryRow = new QWidget(toolbarFrame);
    auto* summaryLayout = new QHBoxLayout(summaryRow);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(5);
    auto* summaryLabel = new QLabel(QStringLiteral("Clipboard"), summaryRow);
    summaryLabel->setObjectName(QStringLiteral("sectionLabel"));
    summaryLabel->setMinimumWidth(68);
    summaryLayout->addWidget(summaryLabel);

    totalBadge_ = new QLabel(summaryRow);
    totalBadge_->setObjectName(QStringLiteral("metricBadge"));
    visibleBadge_ = new QLabel(summaryRow);
    visibleBadge_->setObjectName(QStringLiteral("metricBadge"));
    modifiedBadge_ = new QLabel(QStringLiteral("Modified"), summaryRow);
    modifiedBadge_->setObjectName(QStringLiteral("metricBadge"));
    modifiedBadge_->setStyleSheet(QStringLiteral(
        "QLabel#metricBadge { background:#FEE2E2; color:#B91C1C; border-color:#FCA5A5; font-weight:700; }"));
    modifiedBadge_->setVisible(false);
    summaryLayout->addWidget(totalBadge_);
    summaryLayout->addWidget(visibleBadge_);
    summaryLayout->addWidget(modifiedBadge_);
    summaryLayout->addStretch(1);

    scopeButton_ = makeToggle(QStringLiteral("Session"), true);
    scopeButton_->setMinimumWidth(76);
    summaryLayout->addWidget(scopeButton_);
    modeButton_ = makeToggle(QStringLiteral("Read-only"), false);
    modeButton_->setMinimumWidth(84);
    summaryLayout->addWidget(modeButton_);
    saveButton_ = new QToolButton(summaryRow);
    saveButton_->setText(QStringLiteral("Save..."));
    saveButton_->setObjectName(QStringLiteral("filterToggle"));
    saveButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    saveButton_->setFixedHeight(22);
    summaryLayout->addWidget(saveButton_);
    clearButton_ = new QToolButton(summaryRow);
    clearButton_->setText(QStringLiteral("Clear"));
    clearButton_->setObjectName(QStringLiteral("filterToggle"));
    clearButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    clearButton_->setFixedHeight(22);
    summaryLayout->addWidget(clearButton_);
    foldButton_ = new QToolButton(summaryRow);
    foldButton_->setObjectName(QStringLiteral("traceToolbarFoldButton"));
    foldButton_->setCheckable(true);
    foldButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    foldButton_->setFixedSize(22, 22);
    summaryLayout->addWidget(foldButton_);
    toolbarLayout->addWidget(summaryRow);

    toolbarContent_ = new QWidget(toolbarFrame);
    auto* contentLayout = new QVBoxLayout(toolbarContent_);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);

    auto addRow = [contentLayout](const QString& title) {
        auto* row = new QWidget();
        row->setObjectName(QStringLiteral("traceToolbarRow"));
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(5);
        auto* label = new QLabel(title, row);
        label->setObjectName(QStringLiteral("sectionLabel"));
        label->setMinimumWidth(68);
        rowLayout->addWidget(label);
        contentLayout->addWidget(row);
        return rowLayout;
    };
    auto addGroup = [](QHBoxLayout* rowLayout, const QString& caption) {
        auto* group = new QFrame();
        group->setObjectName(QStringLiteral("traceToolbarGroup"));
        auto* layout = new QHBoxLayout(group);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(4);
        if (!caption.isEmpty()) {
            auto* label = new QLabel(caption, group);
            label->setObjectName(QStringLiteral("groupCaption"));
            layout->addWidget(label);
        }
        rowLayout->addWidget(group);
        return layout;
    };

    QHBoxLayout* filtersRow = addRow(QStringLiteral("Filters"));
    QHBoxLayout* channelGroup = addGroup(filtersRow, QStringLiteral("Channel"));
    reqButton_ = makeToggle(QStringLiteral("REQ"));
    rspButton_ = makeToggle(QStringLiteral("RSP"));
    datButton_ = makeToggle(QStringLiteral("DAT"));
    snpButton_ = makeToggle(QStringLiteral("SNP"));
    channelGroup->addWidget(reqButton_);
    channelGroup->addWidget(rspButton_);
    channelGroup->addWidget(datButton_);
    channelGroup->addWidget(snpButton_);
    QHBoxLayout* directionGroup = addGroup(filtersRow, QStringLiteral("Direction"));
    txButton_ = makeToggle(QStringLiteral("TX"));
    rxButton_ = makeToggle(QStringLiteral("RX"));
    directionGroup->addWidget(txButton_);
    directionGroup->addWidget(rxButton_);
    QHBoxLayout* viewGroup = addGroup(filtersRow, QStringLiteral("View"));
    nodeLabelsButton_ = makeToggle(QStringLiteral("Node Labels"));
    viewGroup->addWidget(nodeLabelsButton_);
    QHBoxLayout* cacheMapGroup = addGroup(filtersRow, QStringLiteral("Cache Map"));
    cacheMapButton_ = makeToggle(QStringLiteral("Show"), false);
    cacheMapButton_->setEnabled(false);
    cacheMapGroup->addWidget(cacheMapButton_);
    cacheMapAddButton_ = makeToggle(QStringLiteral("Add"), false);
    cacheMapAddButton_->setCheckable(false);
    cacheMapAddButton_->setEnabled(false);
    cacheMapGroup->addWidget(cacheMapAddButton_);
    cacheMapFadeButton_ = makeToggle(QStringLiteral("Fade"), true);
    cacheMapFadeButton_->setEnabled(false);
    cacheMapGroup->addWidget(cacheMapFadeButton_);
    filtersRow->addStretch(1);

    QHBoxLayout* searchRow = addRow(QStringLiteral("Search"));
    QHBoxLayout* modeGroup = addGroup(searchRow, QStringLiteral("Mode"));
    searchModeButton_ = makeToggle(QStringLiteral("Filter"), false);
    searchModeButton_->setMinimumWidth(92);
    modeGroup->addWidget(searchModeButton_);
    QHBoxLayout* fieldGroup = addGroup(searchRow, QStringLiteral("Fields"));
    opcodeFilterEdit_ = addFilterField(fieldGroup->parentWidget(), fieldGroup, QStringLiteral("Op"), QStringLiteral("Opcode"), 96);
    sourceIdFilterEdit_ = addFilterField(fieldGroup->parentWidget(), fieldGroup, QStringLiteral("Src"), QStringLiteral("SrcID"), 60);
    targetIdFilterEdit_ = addFilterField(fieldGroup->parentWidget(), fieldGroup, QStringLiteral("Tgt"), QStringLiteral("TgtID"), 60);
    txnIdFilterEdit_ = addFilterField(fieldGroup->parentWidget(), fieldGroup, QStringLiteral("Txn"), QStringLiteral("TxnID"), 60);
    dbidFilterEdit_ = addFilterField(fieldGroup->parentWidget(), fieldGroup, QStringLiteral("DB"), QStringLiteral("DBID"), 60);
    addressFilterEdit_ = addFilterField(fieldGroup->parentWidget(), fieldGroup, QStringLiteral("Addr"), QStringLiteral("Address"), 96);
    searchRow->addStretch(1);

    toolbarLayout->addWidget(toolbarContent_);
    rootLayout->addWidget(toolbarFrame);

    table_ = new QTableView(this);
    table_->setObjectName(QStringLiteral("clipboardTable"));
    table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setAlternatingRowColors(true);
    table_->setTextElideMode(Qt::ElideRight);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setMinimumSectionSize(1);
    table_->verticalHeader()->setDefaultSectionSize(22);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setResizeContentsPrecision(64);
    table_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    table_->setItemDelegateForColumn(FlitTableModel::XactionIndexColumn,
                                     new ClipboardXactionIndexDelegate(table_));
    table_->setItemDelegateForColumn(FlitTableModel::ChannelColumn,
                                     new ClipboardChannelBadgeDelegate(table_));
    auto* nodeDelegate = new ClipboardNodeLabelDelegate(table_);
    table_->setItemDelegateForColumn(FlitTableModel::SourceColumn, nodeDelegate);
    table_->setItemDelegateForColumn(FlitTableModel::TargetColumn, nodeDelegate);
    rootLayout->addWidget(table_, 1);
}

void ClipboardWidget::wireSignals()
{
    connect(scopeButton_, &QToolButton::clicked, this, [this]() {
        setScope(scope_ == ClipboardScope::Session ? ClipboardScope::Global : ClipboardScope::Session);
    });
    connect(modeButton_, &QToolButton::clicked, this, [this]() {
        setReadOnly(!readOnly_);
    });
    connect(saveButton_, &QToolButton::clicked, this, [this]() {
        syncEntriesFromModel();
        Q_EMIT saveRequested();
    });
    connect(foldButton_, &QToolButton::toggled, this, &ClipboardWidget::setToolbarFolded);
    connect(clearButton_, &QToolButton::clicked, this, [this]() {
        if (!entries_) {
            return;
        }
        entries_->clear();
        refreshFromEntries();
        Q_EMIT entriesEdited();
    });

    connect(searchModeButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setSearchMode(checked ? FlitTableModel::SearchMode::Highlight
                                      : FlitTableModel::SearchMode::Filter);
        updateSearchModeButton();
    });

    auto bindFilter = [](QLineEdit* edit, auto applyFilter) {
        QObject::connect(edit, &QLineEdit::textChanged, edit, [edit, applyFilter]() {
            applyFilter(edit->text());
        });
    };
    bindFilter(opcodeFilterEdit_, [this](const QString& text) { model_->setOpcodeFilter(text); });
    bindFilter(sourceIdFilterEdit_, [this](const QString& text) { model_->setSourceIdFilter(text); });
    bindFilter(targetIdFilterEdit_, [this](const QString& text) { model_->setTargetIdFilter(text); });
    bindFilter(txnIdFilterEdit_, [this](const QString& text) { model_->setTxnIdFilter(text); });
    bindFilter(dbidFilterEdit_, [this](const QString& text) { model_->setDbidFilter(text); });
    bindFilter(addressFilterEdit_, [this](const QString& text) { model_->setAddressFilter(text); });

    connect(reqButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setChannelVisible(FlitChannel::Req, checked);
    });
    connect(rspButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setChannelVisible(FlitChannel::Rsp, checked);
    });
    connect(datButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setChannelVisible(FlitChannel::Dat, checked);
    });
    connect(snpButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setChannelVisible(FlitChannel::Snp, checked);
    });
    connect(txButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setDirectionVisible(FlitDirection::Tx, checked);
    });
    connect(rxButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setDirectionVisible(FlitDirection::Rx, checked);
    });
    connect(nodeLabelsButton_, &QToolButton::toggled, this, [this](const bool checked) {
        model_->setNodeLabelsVisible(checked);
        resizeColumnsToTraceDefaults();
    });
    connect(cacheMapButton_, &QToolButton::toggled, this, [this](const bool checked) {
        cacheMapButton_->setText(checked ? QStringLiteral("Hide") : QStringLiteral("Show"));
        if (cacheMinimap_) {
            cacheMinimap_->setMapVisible(checked);
        }
    });
    connect(cacheMapFadeButton_, &QToolButton::toggled, this, [this](const bool checked) {
        if (cacheMinimap_) {
            cacheMinimap_->setFadeWhenInactive(checked);
        }
    });
    connect(cacheMapAddButton_, &QToolButton::clicked, this, &ClipboardWidget::showAddCacheMapLineDialog);
    connect(model_, &FlitTableModel::rowsEdited, this, [this]() {
        if (syncingModel_) {
            return;
        }
        syncDirtyEntriesFromModel();
        updateBadges();
        Q_EMIT entriesEdited();
    });
    connect(model_, &FlitTableModel::modelReset, this, [this]() {
        updateBadges();
    });
    connect(table_, &QTableView::clicked, this, [this](const QModelIndex& index) {
        if (!index.isValid()) {
            model_->clearTransactionHighlight();
            return;
        }
        model_->setTransactionHighlightFromVisibleRow(index.row());
    });
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!readOnly_ || !index.isValid()) {
            return;
        }
        syncEntriesFromModel();
        const std::optional<ClipboardEntry> entry = entryForVisibleRow(index.row());
        if (entry) {
            Q_EMIT rowActivated(*entry);
        }
    });
    connect(table_, &QWidget::customContextMenuRequested, this, &ClipboardWidget::showRowContextMenu);

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, table_);
    deleteShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, &ClipboardWidget::deleteSelectedRows);
    auto* viewportDeleteShortcut = new QShortcut(QKeySequence::Delete, table_->viewport());
    viewportDeleteShortcut->setContext(Qt::WidgetShortcut);
    connect(viewportDeleteShortcut, &QShortcut::activated, this, &ClipboardWidget::deleteSelectedRows);
}

void ClipboardWidget::updateBadges()
{
    if (totalBadge_) {
        totalBadge_->setText(QStringLiteral("Total %1").arg(model_ ? model_->totalCount() : 0));
    }
    if (visibleBadge_) {
        visibleBadge_->setText(QStringLiteral("Visible %1").arg(model_ ? model_->visibleCount() : 0));
    }
    if (modifiedBadge_) {
        modifiedBadge_->setVisible(hasModifiedRows());
    }
}

void ClipboardWidget::updateScopeButton()
{
    if (!scopeButton_) {
        return;
    }
    const QSignalBlocker blocker(scopeButton_);
    scopeButton_->setChecked(scope_ == ClipboardScope::Global);
    scopeButton_->setText(scope_ == ClipboardScope::Global ? QStringLiteral("Global") : QStringLiteral("Session"));
    const QString hint = scope_ == ClipboardScope::Global
        ? QStringLiteral("Showing the shared global Clipboard.")
        : QStringLiteral("Showing the active session Clipboard.");
    scopeButton_->setToolTip(hint);
    scopeButton_->setStatusTip(hint);
}

void ClipboardWidget::updateModeButton()
{
    if (!modeButton_) {
        return;
    }
    const QSignalBlocker blocker(modeButton_);
    modeButton_->setChecked(!readOnly_);
    modeButton_->setText(readOnly_ ? QStringLiteral("Read-only") : QStringLiteral("Editable"));
    const QString hint = readOnly_
        ? QStringLiteral("Clipboard rows are read-only; double-click clean rows to jump to their source trace row.")
        : QStringLiteral("Clipboard rows are editable; double-click source jumping is disabled.");
    modeButton_->setToolTip(hint);
    modeButton_->setStatusTip(hint);
}

void ClipboardWidget::updateSearchModeButton()
{
    if (!searchModeButton_ || !model_) {
        return;
    }
    const bool highlight = model_->searchMode() == FlitTableModel::SearchMode::Highlight;
    const QSignalBlocker blocker(searchModeButton_);
    searchModeButton_->setChecked(highlight);
    searchModeButton_->setText(highlight ? QStringLiteral("Highlight") : QStringLiteral("Filter"));
}

void ClipboardWidget::showRowContextMenu(const QPoint& position)
{
    if (!table_ || !model_) {
        return;
    }
    const QModelIndex index = table_->indexAt(position);
    if (index.isValid() && table_->selectionModel()) {
        table_->selectionModel()->setCurrentIndex(index,
                                                  QItemSelectionModel::ClearAndSelect
                                                      | QItemSelectionModel::Rows);
        table_->selectRow(index.row());
    }

    QMenu menu(this);
    QAction* saveAction = menu.addAction(QStringLiteral("Save..."));
    menu.addSeparator();
    QAction* deleteAction = menu.addAction(QStringLiteral("Delete row"));
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setEnabled(index.isValid());
    QAction* selected = menu.exec(table_->viewport()->mapToGlobal(position));
    if (selected == saveAction) {
        syncEntriesFromModel();
        Q_EMIT saveRequested();
    } else if (selected == deleteAction) {
        deleteSelectedRows();
    }
}

void ClipboardWidget::showAddCacheMapLineDialog()
{
    if (!cacheMinimap_) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Add Cache Map Line"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFrame(&dialog);
    auto* formLayout = new QHBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    auto* rnCombo = new QComboBox(form);
    rnCombo->setEditable(true);
    rnCombo->lineEdit()->setPlaceholderText(QStringLiteral("RN id"));
    const std::vector<TraceCacheLineMinimap::RnNodeChoice> rnChoices =
        cacheMinimap_->availableRnNodeChoices();
    for (const TraceCacheLineMinimap::RnNodeChoice& choice : rnChoices) {
        rnCombo->addItem(choice.label, static_cast<qulonglong>(choice.nodeId));
    }
    auto* addressEdit = new QLineEdit(form);
    addressEdit->setPlaceholderText(QStringLiteral("Address"));
    formLayout->addWidget(new QLabel(QStringLiteral("RN"), form));
    formLayout->addWidget(rnCombo);
    formLayout->addWidget(new QLabel(QStringLiteral("Address"), form));
    formLayout->addWidget(addressEdit);
    layout->addWidget(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::uint64_t rn = 0;
    std::uint64_t address = 0;
    bool rnOk = false;
    if (rnCombo->currentIndex() >= 0
        && rnCombo->currentText() == rnCombo->itemText(rnCombo->currentIndex())) {
        const QVariant selectedRn = rnCombo->currentData();
        if (selectedRn.isValid()) {
            rn = selectedRn.toULongLong(&rnOk);
        }
    }
    if (!rnOk) {
        rnOk = TraceCacheLineMinimap::parseIntegerText(rnCombo->currentText(), rn);
    }
    if (!rnOk
        || rn > static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())
        || !TraceCacheLineMinimap::parseIntegerText(addressEdit->text(), address)) {
        return;
    }
    cacheMinimap_->addLane(static_cast<std::uint32_t>(rn), address);
    if (cacheMapButton_) {
        const QSignalBlocker blocker(cacheMapButton_);
        cacheMapButton_->setChecked(true);
        cacheMapButton_->setText(QStringLiteral("Hide"));
    }
}

QToolButton* ClipboardWidget::makeToggle(const QString& label, const bool checked) const
{
    auto* button = new QToolButton();
    button->setText(label);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setObjectName(QStringLiteral("filterToggle"));
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setFixedHeight(22);
    return button;
}

QLineEdit* ClipboardWidget::addFilterField(QWidget* parent,
                                           QHBoxLayout* rowLayout,
                                           const QString& label,
                                           const QString& placeholder,
                                           const int width)
{
    auto* labelWidget = new QLabel(label, parent);
    labelWidget->setObjectName(QStringLiteral("secondaryLabel"));
    rowLayout->addWidget(labelWidget);

    auto* edit = new QLineEdit(parent);
    edit->setPlaceholderText(placeholder);
    edit->setClearButtonEnabled(true);
    edit->setMinimumWidth(width);
    edit->setMaximumWidth(width);
    edit->setFixedHeight(22);
    rowLayout->addWidget(edit);
    return edit;
}

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
void ClipboardWidget::testSetOpcodeFilter(const QString& text)
{
    if (opcodeFilterEdit_) {
        opcodeFilterEdit_->setText(text);
    }
    if (model_) {
        model_->setOpcodeFilter(text);
    }
}

void ClipboardWidget::testSetSearchMode(const FlitTableModel::SearchMode mode)
{
    if (model_) {
        model_->setSearchMode(mode);
    }
    updateSearchModeButton();
}
#endif

}  // namespace CHIron::Gui
