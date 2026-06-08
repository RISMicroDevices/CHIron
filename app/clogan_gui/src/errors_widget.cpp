#include "errors_widget.hpp"

#include "gui_format.hpp"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStringList>
#include <QTreeView>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace CHIron::Gui {
namespace {

enum ErrorColumns {
    SeverityColumn = 0,
    CodeColumn,
    DescriptionColumn,
    SourceColumn,
    RowColumn,
    ErrorColumnCount
};

QIcon standardIcon(const QWidget* widget, const QStyle::StandardPixmap pixmap)
{
    QStyle* style = widget ? widget->style() : QApplication::style();
    return style ? style->standardIcon(pixmap) : QIcon();
}

QString issueCodeText(const TraceIssue& issue)
{
    const QString code = issue.denialCode.trimmed();
    if (!code.isEmpty() && code != QStringLiteral("-")) {
        return issue.denialCode;
    }
    return issue.denialName;
}

QString issueDescriptionText(const TraceIssue& issue)
{
    if (!issue.summary.isEmpty()) {
        return issue.summary;
    }
    return QStringLiteral("%1 at row %2")
        .arg(issue.denialName.isEmpty() ? TraceIssueSourceText(issue.source) : issue.denialName)
        .arg(static_cast<qulonglong>(issue.logicalRow + 1));
}

QString issueColumnText(const TraceIssue& issue, const int column)
{
    switch (column) {
    case SeverityColumn:
        return TraceIssueSeverityText(issue.severity);
    case CodeColumn:
        return issueCodeText(issue);
    case DescriptionColumn:
        return issueDescriptionText(issue);
    case SourceColumn:
        return TraceIssueSourceText(issue.source);
    case RowColumn:
        return QString::number(static_cast<qulonglong>(issue.logicalRow + 1));
    default:
        return QString();
    }
}

QString issueHeaderText(const int section)
{
    switch (section) {
    case SeverityColumn:
        return QStringLiteral("Severity");
    case CodeColumn:
        return QStringLiteral("Code");
    case DescriptionColumn:
        return QStringLiteral("Description");
    case SourceColumn:
        return QStringLiteral("Source");
    case RowColumn:
        return QStringLiteral("Row");
    default:
        return QString();
    }
}

quintptr makeIssueIndexId(const int issueRow, const bool detail)
{
    return (static_cast<quintptr>(issueRow) << 1U) | (detail ? 1U : 0U);
}

int issueRowFromIndex(const QModelIndex& index)
{
    if (!index.isValid()) {
        return -1;
    }
    return static_cast<int>(index.internalId() >> 1U);
}

bool issueIndexIsDetail(const QModelIndex& index)
{
    return index.isValid() && (index.internalId() & 1U) != 0U;
}

int sizeToInt(const std::size_t value) noexcept
{
    return value > static_cast<std::size_t>(std::numeric_limits<int>::max())
        ? std::numeric_limits<int>::max()
        : static_cast<int>(value);
}

constexpr int kDefaultDisplayedIssuePage = 4096;

int displayedIssueLimitForCount(const std::size_t issueCount,
                                const int pageSize = kDefaultDisplayedIssuePage) noexcept
{
    const std::size_t boundedPageSize =
        pageSize <= 0 ? static_cast<std::size_t>(kDefaultDisplayedIssuePage)
                      : static_cast<std::size_t>(pageSize);
    return sizeToInt(std::min(issueCount, boundedPageSize));
}

int nextDisplayedIssueLimit(const int displayedIssues,
                            const std::size_t issueCount,
                            const int pageSize = kDefaultDisplayedIssuePage) noexcept
{
    const int total = sizeToInt(issueCount);
    if (displayedIssues >= total) {
        return displayedIssues;
    }
    const int boundedPageSize = pageSize <= 0 ? kDefaultDisplayedIssuePage : pageSize;
    const int remaining = total - displayedIssues;
    return displayedIssues + std::min(remaining, boundedPageSize);
}

TraceIssueDisposition nextDisposition(const TraceIssueDisposition disposition) noexcept
{
    switch (disposition) {
    case TraceIssueDisposition::Error:
        return TraceIssueDisposition::Warning;
    case TraceIssueDisposition::Warning:
        return TraceIssueDisposition::Ignored;
    case TraceIssueDisposition::Ignored:
        return TraceIssueDisposition::Error;
    }
    return TraceIssueDisposition::Error;
}

QIcon dispositionIcon(const QWidget* widget, const TraceIssueDisposition disposition)
{
    switch (disposition) {
    case TraceIssueDisposition::Error:
        return standardIcon(widget, QStyle::SP_MessageBoxCritical);
    case TraceIssueDisposition::Warning:
        return standardIcon(widget, QStyle::SP_MessageBoxWarning);
    case TraceIssueDisposition::Ignored:
        return QIcon();
    }
    return QIcon();
}

QString dispositionButtonText(const TraceIssueSource source, const TraceIssueDisposition disposition)
{
    const QString sourceText = source == TraceIssueSource::CacheStateReplay
        ? QStringLiteral("Cache")
        : QStringLiteral("Xaction");
    return QStringLiteral("%1: %2").arg(sourceText, TraceIssueDispositionText(disposition));
}

QString dispositionTooltip(const TraceIssueSource source, const TraceIssueDisposition disposition)
{
    const QString sourceText = source == TraceIssueSource::CacheStateReplay
        ? QStringLiteral("Cache State")
        : QStringLiteral("Xaction");
    const TraceIssueDisposition next = nextDisposition(disposition);
    switch (disposition) {
    case TraceIssueDisposition::Error:
    case TraceIssueDisposition::Warning:
        return QStringLiteral("%1 denials are classified as %2. Click to switch to %3.")
            .arg(sourceText,
                 TraceIssueDispositionText(disposition).toLower(),
                 TraceIssueDispositionText(next).toLower());
    case TraceIssueDisposition::Ignored:
        return QStringLiteral("%1 denials are ignored. Click to switch to %2.")
            .arg(sourceText, TraceIssueDispositionText(next).toLower());
    default:
        return QString();
    }
}

TraceIssueDisposition dispositionForSource(const TraceIssueSource source,
                                           const TraceIssueDisposition xactionDisposition,
                                           const TraceIssueDisposition cacheStateDisposition) noexcept
{
    return source == TraceIssueSource::CacheStateReplay
        ? cacheStateDisposition
        : xactionDisposition;
}

void setButtonChecked(QToolButton* button, const bool checked)
{
    if (!button) {
        return;
    }
    const QSignalBlocker blocker(button);
    button->setChecked(checked);
}

QToolButton* makeToolbarButton(QWidget* parent, const QString& text)
{
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setCheckable(true);
    button->setAutoRaise(false);
    button->setObjectName(QStringLiteral("errorsToolbarButton"));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIconSize(QSize(14, 14));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

int progressValueForRows(const std::uint64_t completedRows,
                         const std::uint64_t totalRows) noexcept
{
    if (totalRows == 0) {
        return 0;
    }
    const long double boundedCompleted =
        static_cast<long double>(std::min(completedRows, totalRows));
    const long double scaled = (boundedCompleted * 1000.0L) / static_cast<long double>(totalRows);
    return static_cast<int>(std::min<long double>(scaled + 0.5L, 1000.0L));
}

QString errorsToolbarStyleSheet()
{
    return QStringLiteral(
        "QToolButton#errorsToolbarButton {"
        "  min-height: 22px;"
        "  padding: 1px 8px;"
        "  color: #2A3944;"
        "  background: #F6F8FA;"
        "  border: 1px solid #CAD2DA;"
        "  border-radius: 3px;"
        "  font-weight: 600;"
        "}"
        "QToolButton#errorsToolbarButton:hover {"
        "  background: #EEF3F6;"
        "}"
        "QToolButton#errorsToolbarButton:checked {"
        "  color: #17232C;"
        "  background: #E8F1F7;"
        "  border-color: #A9BECE;"
        "}"
        "QToolButton#errorsToolbarButton:!checked {"
        "  color: #5D6872;"
        "}"
        "QTreeView#errorsIssueTree {"
        "  background: #FFFFFF;"
        "  alternate-background-color: #F7F9FC;"
        "  border: 1px solid #C9D2DC;"
        "  color: #1F2933;"
        "  selection-background-color: #DCEBF8;"
        "  selection-color: #111827;"
        "  show-decoration-selected: 1;"
        "}"
        "QTreeView#errorsIssueTree::item {"
        "  min-height: 22px;"
        "  padding: 1px 4px;"
        "  border-bottom: 1px solid #EDF1F5;"
        "}"
        "QTreeView#errorsIssueTree::item:selected {"
        "  background: #DCEBF8;"
        "  color: #111827;"
        "}"
        "QHeaderView::section {"
        "  background: #F3F6FA;"
        "  color: #344452;"
        "  padding: 3px 6px;"
        "  border: 0;"
        "  border-right: 1px solid #D5DEE8;"
        "  border-bottom: 1px solid #C9D2DC;"
        "  font-weight: 600;"
        "}");
}

}  // namespace

class ErrorsModel final : public QAbstractItemModel {
public:
    explicit ErrorsModel(QObject* parent = nullptr)
        : QAbstractItemModel(parent)
    {
    }

    QModelIndex index(const int row,
                      const int column,
                      const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid()) {
            if (!parent.isValid()
                || parent.column() != 0
                || issueIndexIsDetail(parent)
                || row != 0
                || column < 0
                || column >= ErrorColumnCount) {
                return QModelIndex();
            }
            const int issueRow = issueRowFromIndex(parent);
            if (issueRow < 0 || issueRow >= displayedIssueCount_) {
                return QModelIndex();
            }
            return createIndex(row, column, makeIssueIndexId(issueRow, true));
        }
        if (row < 0 || column < 0 || column >= ErrorColumnCount) {
            return QModelIndex();
        }
        if (row >= displayedIssueCount_) {
            return QModelIndex();
        }
        return createIndex(row, column, makeIssueIndexId(row, false));
    }

    QModelIndex parent(const QModelIndex& child) const override
    {
        if (!issueIndexIsDetail(child)) {
            return QModelIndex();
        }
        const int issueRow = issueRowFromIndex(child);
        if (issueRow < 0 || issueRow >= displayedIssueCount_) {
            return QModelIndex();
        }
        return createIndex(issueRow, 0, makeIssueIndexId(issueRow, false));
    }

    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override
    {
        if (!parent.isValid()) {
            return displayedIssueCount_ > 0;
        }
        return parent.column() == 0
            && !issueIndexIsDetail(parent)
            && issueRowFromIndex(parent) >= 0
            && issueRowFromIndex(parent) < displayedIssueCount_;
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (!parent.isValid()) {
            return displayedIssueCount_;
        }
        if (parent.column() != 0 || issueIndexIsDetail(parent)) {
            return 0;
        }
        const int issueRow = issueRowFromIndex(parent);
        return issueRow >= 0 && issueRow < displayedIssueCount_ ? 1 : 0;
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        Q_UNUSED(parent);
        return ErrorColumnCount;
    }

    bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override
    {
        return !parent.isValid() && displayedIssueCount_ < sizeToInt(issues_->size());
    }

    void fetchMore(const QModelIndex& parent = QModelIndex()) override
    {
        if (parent.isValid()) {
            return;
        }
        const int oldDisplayedIssues = displayedIssueCount_;
        const int newDisplayedIssues =
            nextDisplayedIssueLimit(displayedIssueCount_, issues_->size(), displayedIssuePageSize_);
        if (newDisplayedIssues <= oldDisplayedIssues) {
            return;
        }
        beginInsertRows(QModelIndex(), oldDisplayedIssues, newDisplayedIssues - 1);
        displayedIssueCount_ = newDisplayedIssues;
        endInsertRows();
    }

    int topLevelRowCount() const noexcept
    {
        return displayedIssueCount_;
    }

    QModelIndex issueIndex(const int row, const int column = SeverityColumn) const
    {
        return index(row, column, QModelIndex());
    }

    QModelIndex detailIndex(const int row) const
    {
        const QModelIndex parentIndex = issueIndex(row, SeverityColumn);
        return index(0, SeverityColumn, parentIndex);
    }

    bool isDetailIndex(const QModelIndex& index) const noexcept
    {
        return issueIndexIsDetail(index);
    }

    QString detailTextAt(const int row) const
    {
        if (row < 0 || row >= sizeToInt(issues_->size())) {
            return QString();
        }
        const TraceIssue& issue = (*issues_)[static_cast<std::size_t>(row)];
        return issue.details.isEmpty() ? issueDescriptionText(issue) : issue.details;
    }

    QVariant data(const QModelIndex& index, const int role = Qt::DisplayRole) const override
    {
        const int issueRow = issueRowFromIndex(index);
        if (!index.isValid() || issueRow < 0 || issueRow >= sizeToInt(issues_->size())) {
            return QVariant();
        }

        const TraceIssue& issue = (*issues_)[static_cast<std::size_t>(issueRow)];
        const bool detail = issueIndexIsDetail(index);
        if (detail) {
            if (role == Qt::DisplayRole) {
                return index.column() == SeverityColumn ? detailTextAt(issueRow) : QVariant();
            }
            if (role == Qt::ToolTipRole) {
                return detailTextAt(issueRow);
            }
            if (role == Qt::BackgroundRole) {
                return QBrush(QColor(QStringLiteral("#F4F8FC")));
            }
            if (role == Qt::ForegroundRole) {
                return QBrush(QColor(QStringLiteral("#475569")));
            }
            if (role == Qt::FontRole) {
                QFont font;
                font.setPointSizeF(std::max<qreal>(7.5, font.pointSizeF() - 1.0));
                return font;
            }
            if (role == Qt::TextAlignmentRole) {
                return QVariant::fromValue(Qt::Alignment(Qt::AlignLeft | Qt::AlignTop));
            }
            if (role == Qt::SizeHintRole) {
                return QSize(-1, 70);
            }
            return QVariant();
        }

        if (role == Qt::DisplayRole) {
            return issueColumnText(issue, index.column());
        }
        if (role == Qt::DecorationRole && index.column() == SeverityColumn) {
            return issue.severity == TraceIssueSeverity::Error ? errorIcon_ : warningIcon_;
        }
        if (role == Qt::ToolTipRole) {
            return issue.details.isEmpty() ? issueDescriptionText(issue) : issue.details;
        }
        if (role == Qt::TextAlignmentRole) {
            return QVariant::fromValue(Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter));
        }
        if (role == Qt::SizeHintRole) {
            return QSize(-1, 22);
        }
        return QVariant();
    }

    QVariant headerData(const int section,
                        const Qt::Orientation orientation,
                        const int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            return issueHeaderText(section);
        }
        return QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid()) {
            return Qt::NoItemFlags;
        }
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    void setIcons(QIcon errorIcon, QIcon warningIcon)
    {
        errorIcon_ = std::move(errorIcon);
        warningIcon_ = std::move(warningIcon);
    }

    void setIssues(std::shared_ptr<const std::vector<TraceIssue>> issues,
                   const TraceIssueCounts counts,
                   const std::uint64_t sourceId,
                   const std::uint64_t generation)
    {
        if (!issues) {
            issues = std::make_shared<const std::vector<TraceIssue>>();
        }
        beginResetModel();
        issues_ = std::move(issues);
        issueCounts_ = counts;
        sourceId_ = sourceId;
        generation_ = generation;
        displayedIssueCount_ = displayedIssueLimitForCount(issues_->size(), displayedIssuePageSize_);
        endResetModel();
    }

    void setDisplayedIssuePageSizeForTest(const int pageSize)
    {
        displayedIssuePageSize_ = pageSize <= 0 ? kDefaultDisplayedIssuePage : pageSize;
        beginResetModel();
        displayedIssueCount_ = displayedIssueLimitForCount(issues_->size(), displayedIssuePageSize_);
        endResetModel();
    }

    void clear()
    {
        setIssues({}, {}, 0, 0);
    }

    int warningCount() const noexcept
    {
        return issueCounts_.warnings;
    }

    int errorCount() const noexcept
    {
        return issueCounts_.errors;
    }

    int issueCount() const noexcept
    {
        return sizeToInt(issues_->size());
    }

    int displayedIssueCount() const noexcept
    {
        return displayedIssueCount_;
    }

    bool fetchMoreIssuesForTest()
    {
        if (!canFetchMore()) {
            return false;
        }
        fetchMore();
        return true;
    }

    bool hasIssuesFor(const std::uint64_t sourceId,
                      const std::uint64_t generation,
                      const int issueCount,
                      const TraceIssueCounts counts) const noexcept
    {
        return sourceId_ == sourceId
            && generation_ == generation
            && issueCount == sizeToInt(issues_->size())
            && counts.warnings == issueCounts_.warnings
            && counts.errors == issueCounts_.errors;
    }

    int logicalRowForIndex(const QModelIndex& index) const noexcept
    {
        const int issueRow = issueRowFromIndex(index);
        if (issueRow < 0 || issueRow >= sizeToInt(issues_->size())) {
            return -1;
        }
        const std::uint64_t logicalRow =
            (*issues_)[static_cast<std::size_t>(issueRow)].logicalRow;
        if (logicalRow > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return -1;
        }
        return static_cast<int>(logicalRow);
    }

    QString codeAt(const int row) const
    {
        if (row < 0 || row >= sizeToInt(issues_->size())) {
            return QString();
        }
        return issueCodeText((*issues_)[static_cast<std::size_t>(row)]);
    }

    QString descriptionAt(const int row) const
    {
        if (row < 0 || row >= sizeToInt(issues_->size())) {
            return QString();
        }
        return issueDescriptionText((*issues_)[static_cast<std::size_t>(row)]);
    }

    QString sourceAt(const int row) const
    {
        if (row < 0 || row >= sizeToInt(issues_->size())) {
            return QString();
        }
        return TraceIssueSourceText((*issues_)[static_cast<std::size_t>(row)].source);
    }

    QString rowTextAt(const int row) const
    {
        if (row < 0 || row >= sizeToInt(issues_->size())) {
            return QString();
        }
        return QString::number(static_cast<qulonglong>(
            (*issues_)[static_cast<std::size_t>(row)].logicalRow + 1));
    }

    QString detailAt(const int row) const
    {
        if (row < 0 || row >= sizeToInt(issues_->size())) {
            return QString();
        }
        return (*issues_)[static_cast<std::size_t>(row)].details;
    }

    bool hasSeverityIconAt(const int row) const
    {
        if (row < 0 || row >= sizeToInt(issues_->size())) {
            return false;
        }
        return !errorIcon_.isNull() || !warningIcon_.isNull();
    }

private:
    std::shared_ptr<const std::vector<TraceIssue>> issues_ =
        std::make_shared<const std::vector<TraceIssue>>();
    TraceIssueCounts issueCounts_;
    std::uint64_t sourceId_ = 0;
    std::uint64_t generation_ = 0;
    QIcon errorIcon_;
    QIcon warningIcon_;
    int displayedIssueCount_ = 0;
    int displayedIssuePageSize_ = kDefaultDisplayedIssuePage;
};

ErrorsWidget::ErrorsWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    refreshSummary();
}

void ErrorsWidget::setIssues(const std::vector<TraceIssue>& issues,
                             const std::uint64_t sourceId,
                             const std::uint64_t generation)
{
    setIssues(std::make_shared<const std::vector<TraceIssue>>(issues),
              CountTraceIssues(issues),
              sourceId,
              generation);
}

void ErrorsWidget::setIssues(std::shared_ptr<const std::vector<TraceIssue>> issues,
                             const TraceIssueCounts counts,
                             const std::uint64_t sourceId,
                             const std::uint64_t generation)
{
    model_->setIssues(std::move(issues), counts, sourceId, generation);
    buildActive_ = false;
    buildText_.clear();
    completedRows_ = 0;
    totalRows_ = 0;
    refreshSummary();
}

void ErrorsWidget::setBuildStatus(const bool active,
                                  const QString& text,
                                  const std::uint64_t completedRows,
                                  const std::uint64_t totalRows)
{
    buildActive_ = active;
    buildText_ = text;
    completedRows_ = completedRows;
    totalRows_ = totalRows;
    refreshSummary();
}

void ErrorsWidget::setIssueDisposition(const TraceIssueSource source,
                                       const TraceIssueDisposition disposition)
{
    TraceIssueDisposition& target =
        source == TraceIssueSource::CacheStateReplay ? cacheStateDisposition_ : xactionDisposition_;
    if (target == disposition) {
        refreshDispositionButtons();
        return;
    }
    target = disposition;
    refreshDispositionButtons();
}

TraceIssueDisposition ErrorsWidget::issueDisposition(const TraceIssueSource source) const noexcept
{
    return dispositionForSource(source, xactionDisposition_, cacheStateDisposition_);
}

void ErrorsWidget::setSeverityVisible(const TraceIssueSeverity severity, const bool visible)
{
    bool& target = severity == TraceIssueSeverity::Warning ? warningsVisible_ : errorsVisible_;
    if (target == visible) {
        refreshSeverityButtons();
        return;
    }
    target = visible;
    refreshSeverityButtons();
}

bool ErrorsWidget::severityVisible(const TraceIssueSeverity severity) const noexcept
{
    return severity == TraceIssueSeverity::Warning ? warningsVisible_ : errorsVisible_;
}

void ErrorsWidget::clear()
{
    model_->clear();
    buildActive_ = false;
    buildText_.clear();
    completedRows_ = 0;
    totalRows_ = 0;
    refreshSummary();
}

int ErrorsWidget::warningCount() const noexcept
{
    return model_ ? model_->warningCount() : 0;
}

int ErrorsWidget::errorCount() const noexcept
{
    return model_ ? model_->errorCount() : 0;
}

int ErrorsWidget::issueCount() const noexcept
{
    return model_ ? model_->issueCount() : 0;
}

bool ErrorsWidget::hasIssuesFor(const std::uint64_t sourceId,
                                const std::uint64_t generation,
                                const int issueCount,
                                const TraceIssueCounts counts) const noexcept
{
    return model_ && model_->hasIssuesFor(sourceId, generation, issueCount, counts);
}

void ErrorsWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);
    setStyleSheet(errorsToolbarStyleSheet());

    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(10);

    errorToggleButton_ = makeToolbarButton(this, QString());
    errorToggleButton_->setIcon(standardIcon(this, QStyle::SP_MessageBoxCritical));
    errorToggleButton_->setToolTip(QStringLiteral("Show or hide errors."));
    toolbar->addWidget(errorToggleButton_);

    warningToggleButton_ = makeToolbarButton(this, QString());
    warningToggleButton_->setIcon(standardIcon(this, QStyle::SP_MessageBoxWarning));
    warningToggleButton_->setToolTip(QStringLiteral("Show or hide warnings."));
    toolbar->addWidget(warningToggleButton_);

    toolbar->addSpacing(8);

    const auto addDispositionButton =
        [this, toolbar](const TraceIssueSource source,
                        QToolButton*& button) {
            button = makeToolbarButton(this, QString());
            button->setCheckable(false);
            toolbar->addWidget(button);
            connect(button, &QToolButton::clicked, this, [this, source]() {
                const TraceIssueDisposition disposition =
                    nextDisposition(issueDisposition(source));
                setIssueDisposition(source, disposition);
                Q_EMIT issueDispositionChanged(source, disposition);
            });
        };

    addDispositionButton(TraceIssueSource::XactionIndex, xactionDispositionButton_);
    toolbar->addSpacing(6);
    addDispositionButton(TraceIssueSource::CacheStateReplay, cacheDispositionButton_);
    toolbar->addSpacing(8);
    statusLabel_ = new QLabel(this);
    statusLabel_->setTextFormat(Qt::PlainText);
    toolbar->addWidget(statusLabel_, 1);

    progressBar_ = new QProgressBar(this);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedWidth(140);
    progressBar_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    progressBar_->hide();
    toolbar->addWidget(progressBar_);
    root->addLayout(toolbar);

    model_ = new ErrorsModel(this);
    model_->setIcons(standardIcon(this, QStyle::SP_MessageBoxCritical),
                     standardIcon(this, QStyle::SP_MessageBoxWarning));

    tree_ = new QTreeView(this);
    tree_->setObjectName(QStringLiteral("errorsIssueTree"));
    tree_->setModel(model_);
    tree_->setAlternatingRowColors(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setWordWrap(true);
    tree_->setRootIsDecorated(true);
    tree_->setIndentation(18);
    tree_->setItemsExpandable(true);
    tree_->setExpandsOnDoubleClick(false);
    tree_->setAllColumnsShowFocus(true);
    tree_->setUniformRowHeights(false);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(QHeaderView::Interactive);
    tree_->header()->setSectionResizeMode(DescriptionColumn, QHeaderView::Stretch);
    tree_->setColumnWidth(SeverityColumn, 96);
    tree_->setColumnWidth(CodeColumn, 180);
    tree_->setColumnWidth(SourceColumn, 112);
    tree_->setColumnWidth(RowColumn, 78);
    root->addWidget(tree_, 1);

    connect(tree_, &QTreeView::doubleClicked, this, &ErrorsWidget::handleIssueDoubleClicked);
    connect(tree_, &QTreeView::expanded, this, &ErrorsWidget::spanIssueDetailRow);
    connect(tree_, &QTreeView::customContextMenuRequested, this, &ErrorsWidget::showIssueContextMenu);
    connect(errorToggleButton_, &QToolButton::toggled, this, [this](const bool checked) {
        errorsVisible_ = checked;
        refreshSummary();
        Q_EMIT severityVisibilityChanged(TraceIssueSeverity::Error, checked);
    });
    connect(warningToggleButton_, &QToolButton::toggled, this, [this](const bool checked) {
        warningsVisible_ = checked;
        refreshSummary();
        Q_EMIT severityVisibilityChanged(TraceIssueSeverity::Warning, checked);
    });
    refreshDispositionButtons();
    refreshSeverityButtons();
}

void ErrorsWidget::refreshSummary()
{
    const int errors = errorCount();
    const int warnings = warningCount();
    const int issues = issueCount();

    if (errorToggleButton_) {
        errorToggleButton_->setText(QStringLiteral("%1 error%2")
                                        .arg(errors)
                                        .arg(errors == 1 ? QString() : QStringLiteral("s")));
    }
    if (warningToggleButton_) {
        warningToggleButton_->setText(QStringLiteral("%1 warning%2")
                                          .arg(warnings)
                                          .arg(warnings == 1 ? QString() : QStringLiteral("s")));
    }

    if (buildActive_) {
        const QString text = buildText_.isEmpty() ? QStringLiteral("Building diagnostics") : buildText_;
        if (totalRows_ > 0) {
            statusLabel_->setText(QStringLiteral("%1: %2 / %3 issues")
                                      .arg(text,
                                           FormatUnsignedIntegerWithThousands(completedRows_),
                                           FormatUnsignedIntegerWithThousands(totalRows_)));
        } else {
            statusLabel_->setText(text);
        }
    } else if (!buildText_.isEmpty()) {
        statusLabel_->setText(buildText_);
    } else if (issues == 0 && errors + warnings > 0) {
        statusLabel_->setText(QStringLiteral("No visible issues."));
    } else if (issues == 0) {
        statusLabel_->setText(QStringLiteral("No xaction or cache-state errors."));
    } else {
        statusLabel_->setText(QStringLiteral("%1 issue%2")
                                  .arg(issues)
                                  .arg(issues == 1 ? QString() : QStringLiteral("s")));
    }

    if (!progressBar_) {
        return;
    }
    if (!buildActive_) {
        progressBar_->hide();
        progressBar_->setRange(0, 1000);
        progressBar_->setValue(0);
        progressBar_->setToolTip(QString());
        return;
    }
    progressBar_->show();
    if (totalRows_ > 0) {
        progressBar_->setRange(0, 1000);
        progressBar_->setValue(progressValueForRows(completedRows_, totalRows_));
        progressBar_->setToolTip(QStringLiteral("%1 / %2 persisted issues")
                                     .arg(FormatUnsignedIntegerWithThousands(completedRows_),
                                          FormatUnsignedIntegerWithThousands(totalRows_)));
    } else {
        progressBar_->setRange(0, 0);
        progressBar_->setToolTip(buildText_);
    }
}

void ErrorsWidget::refreshDispositionButtons()
{
    const auto updateButton = [this](QToolButton* button,
                                     const TraceIssueSource source,
                                     const TraceIssueDisposition disposition) {
        if (!button) {
            return;
        }
        button->setText(dispositionButtonText(source, disposition));
        button->setIcon(dispositionIcon(this, disposition));
        button->setToolTip(dispositionTooltip(source, disposition));
    };
    updateButton(xactionDispositionButton_,
                 TraceIssueSource::XactionIndex,
                 xactionDisposition_);
    updateButton(cacheDispositionButton_,
                 TraceIssueSource::CacheStateReplay,
                 cacheStateDisposition_);
}

void ErrorsWidget::refreshSeverityButtons()
{
    setButtonChecked(errorToggleButton_, errorsVisible_);
    setButtonChecked(warningToggleButton_, warningsVisible_);
    refreshSummary();
}

void ErrorsWidget::activateIndex(const QModelIndex& index)
{
    const int logicalRow = model_ ? model_->logicalRowForIndex(index) : -1;
    if (logicalRow < 0) {
        return;
    }
    Q_EMIT logicalRowActivated(logicalRow);
}

void ErrorsWidget::handleIssueDoubleClicked(const QModelIndex& index)
{
    if (!model_ || !index.isValid()) {
        return;
    }
    if (!model_->isDetailIndex(index)) {
        const int row = issueRowFromIndex(index);
        if (row >= 0) {
            setIssueExpanded(row, !issueExpanded(row));
        }
    }
    activateIndex(index);
}

void ErrorsWidget::showIssueContextMenu(const QPoint& position)
{
    if (!tree_ || !model_) {
        return;
    }
    const QModelIndex hitIndex = tree_->indexAt(position);
    const int row = issueRowFromIndex(hitIndex);
    if (!hitIndex.isValid() || row < 0 || row >= model_->topLevelRowCount()) {
        return;
    }

    QMenu menu(this);
    QAction* jumpAction = menu.addAction(QStringLiteral("Jump to row %1")
                                             .arg(model_->rowTextAt(row)));
    QAction* toggleAction = menu.addAction(issueContextToggleText(row));
    QAction* chosen = menu.exec(tree_->viewport()->mapToGlobal(position));
    if (chosen == jumpAction) {
        activateIndex(hitIndex);
    } else if (chosen == toggleAction) {
        setIssueExpanded(row, !issueExpanded(row));
    }
}

void ErrorsWidget::setIssueExpanded(const int row, const bool expanded)
{
    if (!tree_ || !model_ || row < 0 || row >= model_->topLevelRowCount()) {
        return;
    }
    const QModelIndex issueIndex = model_->issueIndex(row, SeverityColumn);
    if (!issueIndex.isValid()) {
        return;
    }
    if (expanded) {
        tree_->expand(issueIndex);
        spanIssueDetailRow(issueIndex);
    } else {
        tree_->collapse(issueIndex);
    }
}

bool ErrorsWidget::issueExpanded(const int row) const
{
    if (!tree_ || !model_ || row < 0 || row >= model_->topLevelRowCount()) {
        return false;
    }
    const QModelIndex index = model_->issueIndex(row, SeverityColumn);
    return index.isValid() && tree_->isExpanded(index);
}

void ErrorsWidget::spanIssueDetailRow(const QModelIndex& issueIndex)
{
    if (!tree_ || !model_ || !issueIndex.isValid() || model_->isDetailIndex(issueIndex)) {
        return;
    }
    const int row = issueRowFromIndex(issueIndex);
    if (row < 0 || row >= model_->topLevelRowCount()) {
        return;
    }
    const QModelIndex detailIndex = model_->detailIndex(row);
    if (detailIndex.isValid()) {
        tree_->setFirstColumnSpanned(0, issueIndex, true);
    }
}

QString ErrorsWidget::issueContextToggleText(const int row) const
{
    return issueExpanded(row) ? QStringLiteral("Fold") : QStringLiteral("Expand");
}

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
QString ErrorsWidget::testStatusText() const
{
    return statusLabel_ ? statusLabel_->text() : QString();
}

QString ErrorsWidget::testTopLevelSummary(const int row) const
{
    return testDescriptionText(row);
}

QString ErrorsWidget::testCodeText(const int row) const
{
    return model_ ? model_->codeAt(row) : QString();
}

QString ErrorsWidget::testDescriptionText(const int row) const
{
    return model_ ? model_->descriptionAt(row) : QString();
}

QString ErrorsWidget::testSourceText(const int row) const
{
    return model_ ? model_->sourceAt(row) : QString();
}

QString ErrorsWidget::testRowText(const int row) const
{
    return model_ ? model_->rowTextAt(row) : QString();
}

bool ErrorsWidget::testTopLevelHasSeverityIcon(const int row) const
{
    return model_ && model_->hasSeverityIconAt(row);
}

QString ErrorsWidget::testDetailText(const int row) const
{
    return model_ ? model_->detailAt(row) : QString();
}

QString ErrorsWidget::testExpandedDetailText(const int row) const
{
    return model_ ? model_->detailTextAt(row) : QString();
}

bool ErrorsWidget::testIssueExpanded(const int row) const
{
    return issueExpanded(row);
}

bool ErrorsWidget::testSetIssueExpanded(const int row, const bool expanded)
{
    if (!model_ || row < 0 || row >= model_->topLevelRowCount()) {
        return false;
    }
    setIssueExpanded(row, expanded);
    return testIssueExpanded(row) == expanded;
}

QString ErrorsWidget::testIssueContextToggleText(const int row) const
{
    return issueContextToggleText(row);
}

bool ErrorsWidget::testIssueTreeRootDecorated() const noexcept
{
    return tree_ && tree_->rootIsDecorated();
}

int ErrorsWidget::testIssueTreeIndentation() const noexcept
{
    return tree_ ? tree_->indentation() : 0;
}

bool ErrorsWidget::testActivateIssue(const int row)
{
    if (!model_ || row < 0 || row >= model_->issueCount()) {
        return false;
    }
    handleIssueDoubleClicked(model_->issueIndex(row, SeverityColumn));
    return true;
}

QString ErrorsWidget::testIssueDispositionText(const TraceIssueSource source) const
{
    return TraceIssueDispositionText(issueDisposition(source));
}

bool ErrorsWidget::testSetIssueDisposition(const TraceIssueSource source,
                                           const TraceIssueDisposition disposition)
{
    setIssueDisposition(source, disposition);
    Q_EMIT issueDispositionChanged(source, disposition);
    return true;
}

bool ErrorsWidget::testSeverityVisible(const TraceIssueSeverity severity) const noexcept
{
    return severityVisible(severity);
}

bool ErrorsWidget::testSetSeverityVisible(const TraceIssueSeverity severity, const bool visible)
{
    setSeverityVisible(severity, visible);
    Q_EMIT severityVisibilityChanged(severity, visible);
    return true;
}

bool ErrorsWidget::testProgressVisible() const noexcept
{
    return progressBar_ && progressBar_->isVisible();
}

bool ErrorsWidget::testProgressBusy() const noexcept
{
    return progressBar_ && progressBar_->minimum() == 0 && progressBar_->maximum() == 0;
}

int ErrorsWidget::testProgressValue() const noexcept
{
    return progressBar_ ? progressBar_->value() : 0;
}

int ErrorsWidget::testProgressMaximum() const noexcept
{
    return progressBar_ ? progressBar_->maximum() : 0;
}

QString ErrorsWidget::testSeverityButtonText(const TraceIssueSeverity severity) const
{
    const QToolButton* button =
        severity == TraceIssueSeverity::Warning ? warningToggleButton_ : errorToggleButton_;
    return button ? button->text() : QString();
}

bool ErrorsWidget::testSeverityButtonHasIcon(const TraceIssueSeverity severity) const noexcept
{
    const QToolButton* button =
        severity == TraceIssueSeverity::Warning ? warningToggleButton_ : errorToggleButton_;
    return button && !button->icon().isNull();
}

QString ErrorsWidget::testDispositionButtonText(const TraceIssueSource source) const
{
    const QToolButton* button =
        source == TraceIssueSource::CacheStateReplay
            ? cacheDispositionButton_
            : xactionDispositionButton_;
    return button ? button->text() : QString();
}

bool ErrorsWidget::testDispositionButtonHasIcon(const TraceIssueSource source) const noexcept
{
    const QToolButton* button =
        source == TraceIssueSource::CacheStateReplay
            ? cacheDispositionButton_
            : xactionDispositionButton_;
    return button && !button->icon().isNull();
}

bool ErrorsWidget::testDispositionButtonCheckable(const TraceIssueSource source) const noexcept
{
    const QToolButton* button =
        source == TraceIssueSource::CacheStateReplay
            ? cacheDispositionButton_
            : xactionDispositionButton_;
    return button && button->isCheckable();
}

bool ErrorsWidget::testDispositionButtonChecked(const TraceIssueSource source) const noexcept
{
    const QToolButton* button =
        source == TraceIssueSource::CacheStateReplay
            ? cacheDispositionButton_
            : xactionDispositionButton_;
    return button && button->isChecked();
}

bool ErrorsWidget::testClickDispositionButton(const TraceIssueSource source)
{
    QToolButton* button =
        source == TraceIssueSource::CacheStateReplay
            ? cacheDispositionButton_
            : xactionDispositionButton_;
    if (!button) {
        return false;
    }
    button->click();
    return true;
}

int ErrorsWidget::testDisplayedIssueRowCount() const noexcept
{
    return model_ ? model_->displayedIssueCount() : 0;
}

void ErrorsWidget::testSetIssuePageSize(const int pageSize)
{
    if (model_) {
        model_->setDisplayedIssuePageSizeForTest(pageSize);
    }
}

bool ErrorsWidget::testFetchMoreIssueRows()
{
    return model_ && model_->fetchMoreIssuesForTest();
}
#endif

}  // namespace CHIron::Gui
