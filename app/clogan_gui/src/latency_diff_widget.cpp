#include "latency_diff_widget.hpp"

#include "latency_diff_export.hpp"
#include "trace_session.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWizard>
#include <QWizardPage>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace CHIron::Gui {

namespace {

constexpr int kMetricColumn = 0;
constexpr int kAColumn = 1;
constexpr int kBColumn = 2;
constexpr int kDeltaColumn = 3;
constexpr int kPercentColumn = 4;
constexpr int kPresenceColumn = 5;
constexpr int kPercentValueRole = Qt::UserRole + 1;
constexpr int kPercentComparableRole = Qt::UserRole + 2;
constexpr int kNumericSortRole = Qt::UserRole + 3;

struct DiffWizardResult {
    int sessionAIndex = -1;
    int sessionBIndex = -1;
    LatencyDiffRangeMode rangeMode = LatencyDiffRangeMode::CommonScaledRange;
    LatencyTimeAnchorMode anchorMode = LatencyTimeAnchorMode::RelativeSessionStart;
    LatencyAnalysisOptions optionsA;
    LatencyAnalysisOptions optionsB;
};

class DiffTreeItem final : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const QTreeWidget* owner = treeWidget();
        const int column = owner ? owner->sortColumn() : 0;
        if (column >= kAColumn && column <= kPercentColumn) {
            return data(column, kNumericSortRole).toDouble()
                < other.data(column, kNumericSortRole).toDouble();
        }
        return QTreeWidgetItem::operator<(other);
    }
};

class DataBarDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

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

        const bool comparable = index.data(kPercentComparableRole).toBool();
        bool ok = false;
        const double percent = index.data(kPercentValueRole).toDouble(&ok);
        if (ok && comparable && std::isfinite(percent)) {
            const double magnitude = std::min(1.0, std::abs(percent));
            const QRect track = option.rect.adjusted(7, 5, -7, -5);
            if (track.width() > 12 && track.height() > 6) {
                const int center = track.left() + track.width() / 2;
                const int width = static_cast<int>(std::llround((track.width() / 2.0) * magnitude));
                QRect bar;
                QColor color;
                if (percent >= 0.0) {
                    bar = QRect(center, track.top(), width, track.height());
                    color = QColor(QStringLiteral("#F9C4BA"));
                } else {
                    bar = QRect(center - width, track.top(), width, track.height());
                    color = QColor(QStringLiteral("#B9E6C7"));
                }
                painter->save();
                painter->setPen(Qt::NoPen);
                painter->setBrush(color);
                painter->drawRect(bar);
                painter->setPen(QColor(QStringLiteral("#C9D0D8")));
                painter->drawLine(center, track.top(), center, track.bottom());
                painter->restore();
            }
        }

        painter->save();
        painter->setPen(option.state.testFlag(QStyle::State_Selected)
                            ? option.palette.highlightedText().color()
                            : option.palette.text().color());
        painter->drawText(option.rect.adjusted(6, 0, -6, 0),
                          Qt::AlignRight | Qt::AlignVCenter,
                          text);
        painter->restore();
    }
};

QString presenceText(bool inA, bool inB)
{
    if (inA && inB) {
        return QStringLiteral("A+B");
    }
    if (inA) {
        return QStringLiteral("A only");
    }
    if (inB) {
        return QStringLiteral("B only");
    }
    return QStringLiteral("-");
}

std::optional<DiffWizardResult> runDiffWizard(const std::vector<LatencyDiffSessionSource>& sessions,
                                              QWidget* parent)
{
    if (sessions.size() < 2) {
        QMessageBox::information(parent,
                                 QStringLiteral("Latency Diff"),
                                 QStringLiteral("Open at least two sessions before building a Latency Diff report."));
        return std::nullopt;
    }

    QWizard wizard(parent);
    wizard.setWindowTitle(QStringLiteral("Build Latency Diff"));
    wizard.setWizardStyle(QWizard::ModernStyle);
    wizard.setButtonText(QWizard::FinishButton, QStringLiteral("Build"));
    wizard.resize(640, 460);

    QWizardPage* page = new QWizardPage(&wizard);
    page->setTitle(QStringLiteral("Latency Diff"));
    page->setSubTitle(QStringLiteral("Choose the two sessions, time scaling, and transaction-start range."));
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    QFormLayout* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* sessionACombo = new QComboBox(page);
    auto* sessionBCombo = new QComboBox(page);
    for (const LatencyDiffSessionSource& source : sessions) {
        const QString label = source.path.isEmpty()
            ? source.label
            : QStringLiteral("%1  (%2)").arg(source.label, source.path);
        sessionACombo->addItem(label);
        sessionBCombo->addItem(label);
    }
    sessionBCombo->setCurrentIndex(1);
    form->addRow(QStringLiteral("Session A"), sessionACombo);
    form->addRow(QStringLiteral("Session B"), sessionBCombo);

    auto* scaleA = new QDoubleSpinBox(page);
    auto* scaleB = new QDoubleSpinBox(page);
    for (QDoubleSpinBox* spin : {scaleA, scaleB}) {
        spin->setDecimals(9);
        spin->setRange(0.000000001, 1000000000.0);
        spin->setValue(1.0);
    }
    form->addRow(QStringLiteral("Scale A"), scaleA);
    form->addRow(QStringLiteral("Scale B"), scaleB);

    auto* anchorCombo = new QComboBox(page);
    anchorCombo->addItem(QStringLiteral("Relative to each session start"));
    anchorCombo->addItem(QStringLiteral("Absolute timestamp"));
    form->addRow(QStringLiteral("Anchor"), anchorCombo);

    auto* rangeModeCombo = new QComboBox(page);
    rangeModeCombo->addItem(QStringLiteral("Common scaled range"));
    rangeModeCombo->addItem(QStringLiteral("Separate session ranges"));
    form->addRow(QStringLiteral("Range mode"), rangeModeCombo);

    auto* rangeEnabled = new QCheckBox(QStringLiteral("Limit by transaction start time"), page);
    form->addRow(QString(), rangeEnabled);

    auto* commonStart = new QDoubleSpinBox(page);
    auto* commonEnd = new QDoubleSpinBox(page);
    auto* startA = new QDoubleSpinBox(page);
    auto* endA = new QDoubleSpinBox(page);
    auto* startB = new QDoubleSpinBox(page);
    auto* endB = new QDoubleSpinBox(page);
    for (QDoubleSpinBox* spin : {commonStart, commonEnd, startA, endA, startB, endB}) {
        spin->setDecimals(3);
        spin->setRange(-1.0e18, 1.0e18);
        spin->setEnabled(false);
    }
    commonEnd->setValue(1.0e9);
    endA->setValue(1.0e9);
    endB->setValue(1.0e9);
    form->addRow(QStringLiteral("Common start"), commonStart);
    form->addRow(QStringLiteral("Common end"), commonEnd);
    form->addRow(QStringLiteral("A start"), startA);
    form->addRow(QStringLiteral("A end"), endA);
    form->addRow(QStringLiteral("B start"), startB);
    form->addRow(QStringLiteral("B end"), endB);

    const auto updateRangeControls = [&]() {
        const bool enabled = rangeEnabled->isChecked();
        const bool separate = rangeModeCombo->currentIndex() == 1;
        commonStart->setEnabled(enabled && !separate);
        commonEnd->setEnabled(enabled && !separate);
        startA->setEnabled(enabled && separate);
        endA->setEnabled(enabled && separate);
        startB->setEnabled(enabled && separate);
        endB->setEnabled(enabled && separate);
    };
    QObject::connect(rangeEnabled, &QCheckBox::toggled, page, updateRangeControls);
    QObject::connect(rangeModeCombo, &QComboBox::currentIndexChanged, page, updateRangeControls);

    QLabel* hint = new QLabel(
        QStringLiteral("Session A is the baseline. Percentage diff is (B - A) / A. "
                       "The selected range includes latency samples whose transaction start time is inside the range."),
        page);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("secondaryLabel"));

    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addStretch(1);
    wizard.addPage(page);

    for (;;) {
        if (wizard.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        if (sessionACombo->currentIndex() != sessionBCombo->currentIndex()) {
            break;
        }
        QMessageBox::warning(&wizard,
                             QStringLiteral("Latency Diff"),
                             QStringLiteral("Choose two different sessions."));
    }

    DiffWizardResult result;
    result.sessionAIndex = sessionACombo->currentIndex();
    result.sessionBIndex = sessionBCombo->currentIndex();
    result.rangeMode = rangeModeCombo->currentIndex() == 1
        ? LatencyDiffRangeMode::SeparateSessionRanges
        : LatencyDiffRangeMode::CommonScaledRange;
    result.anchorMode = anchorCombo->currentIndex() == 1
        ? LatencyTimeAnchorMode::AbsoluteTimestamp
        : LatencyTimeAnchorMode::RelativeSessionStart;
    result.optionsA.timeScale = scaleA->value();
    result.optionsB.timeScale = scaleB->value();
    result.optionsA.anchorMode = result.anchorMode;
    result.optionsB.anchorMode = result.anchorMode;
    result.optionsA.hasRange = rangeEnabled->isChecked();
    result.optionsB.hasRange = rangeEnabled->isChecked();
    if (result.rangeMode == LatencyDiffRangeMode::CommonScaledRange) {
        result.optionsA.rangeStart = commonStart->value();
        result.optionsA.rangeEnd = commonEnd->value();
        result.optionsB.rangeStart = commonStart->value();
        result.optionsB.rangeEnd = commonEnd->value();
    } else {
        result.optionsA.rangeStart = startA->value();
        result.optionsA.rangeEnd = endA->value();
        result.optionsB.rangeStart = startB->value();
        result.optionsB.rangeEnd = endB->value();
    }
    return result;
}

}  // namespace

#ifdef CHIRON_GUI_ENABLE_LATENCY_DIFF_TEST_API
namespace {

QTreeWidgetItem* itemAtPath(QTreeWidget* tree, const std::vector<int>& path)
{
    if (!tree || path.empty()) {
        return nullptr;
    }
    QTreeWidgetItem* item = tree->topLevelItem(path.front());
    for (std::size_t index = 1; item && index < path.size(); ++index) {
        item = item->child(path[index]);
    }
    return item;
}

}  // namespace
#endif

struct LatencyDiffWidget::BuildRequest {
    LatencyDiffSessionSource sessionA;
    LatencyDiffSessionSource sessionB;
    LatencyDiffOptions options;
};

LatencyDiffWidget::LatencyDiffWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(6);
    buildButton_ = new QPushButton(QStringLiteral("Build..."), this);
    exportButton_ = new QPushButton(QStringLiteral("Export Excel..."), this);
    exportButton_->setEnabled(false);
    statusLabel_ = new QLabel(QStringLiteral("Open two sessions, then build a Latency Diff report."), this);
    statusLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    toolbar->addWidget(buildButton_);
    toolbar->addWidget(exportButton_);
    toolbar->addWidget(statusLabel_, 1);
    root->addLayout(toolbar);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->hide();
    root->addWidget(progressBar_);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(6);
    tree_->setHeaderLabels({
        QStringLiteral("Bucket / Metric"),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("Delta"),
        QStringLiteral("% Diff"),
        QStringLiteral("Seen"),
    });
    tree_->setAlternatingRowColors(true);
    tree_->setSortingEnabled(true);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setItemDelegateForColumn(kPercentColumn, new DataBarDelegate(tree_));
    tree_->header()->setSectionResizeMode(kMetricColumn, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(kAColumn, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(kBColumn, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(kDeltaColumn, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(kPercentColumn, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(kPresenceColumn, QHeaderView::ResizeToContents);
    root->addWidget(tree_, 1);

    connect(buildButton_, &QPushButton::clicked, this, &LatencyDiffWidget::runWizard);
    connect(exportButton_, &QPushButton::clicked, this, &LatencyDiffWidget::exportExcel);
    updateButtonStates();
}

LatencyDiffWidget::~LatencyDiffWidget()
{
    stopBuildThread();
}

void LatencyDiffWidget::stopBuildThread()
{
    ++buildGeneration_;
    if (buildThread_.joinable()) {
        buildThread_.request_stop();
        buildThread_.join();
    }
}

void LatencyDiffWidget::setSessions(std::vector<LatencyDiffSessionSource> sessions)
{
    sessions_ = std::move(sessions);
    if (report_) {
        const auto hasLabel = [this](const QString& label) {
            return std::any_of(sessions_.begin(), sessions_.end(), [&](const LatencyDiffSessionSource& source) {
                return source.label == label;
            });
        };
        if (sessions_.size() < 2
            || !hasLabel(report_->sessionALabel)
            || !hasLabel(report_->sessionBLabel)) {
            report_.reset();
            if (tree_) {
                tree_->clear();
            }
            setStatusText(QStringLiteral("Open two sessions, then build a Latency Diff report."));
        }
    }
    updateButtonStates();
}

void LatencyDiffWidget::clear()
{
    stopBuildThread();
    report_.reset();
    if (tree_) {
        tree_->clear();
    }
    setStatusText(QStringLiteral("Open two sessions, then build a Latency Diff report."));
    updateButtonStates();
}

bool LatencyDiffWidget::hasReport() const noexcept
{
    return report_ && !report_->failed;
}

void LatencyDiffWidget::runWizard()
{
    const std::optional<DiffWizardResult> wizard = runDiffWizard(sessions_, this);
    if (!wizard) {
        return;
    }
    BuildRequest request;
    request.sessionA = sessions_[static_cast<std::size_t>(wizard->sessionAIndex)];
    request.sessionB = sessions_[static_cast<std::size_t>(wizard->sessionBIndex)];
    request.options.sessionALabel = request.sessionA.label;
    request.options.sessionBLabel = request.sessionB.label;
    request.options.sessionA = wizard->optionsA;
    request.options.sessionB = wizard->optionsB;
    request.options.rangeMode = wizard->rangeMode;
    startBuild(std::move(request));
}

bool LatencyDiffWidget::buildReportFromRequest(const BuildRequest& request,
                                               LatencyDiffReport& report,
                                               QString& errorText,
                                               std::stop_token stopToken)
{
    LatencyAnalysisResult analysisA;
    LatencyAnalysisResult analysisB;
    if (request.sessionA.traceSession) {
        analysisA = AnalyzeLatencySession(request.sessionA.traceSession,
                                          request.options.sessionA,
                                          request.sessionA.label,
                                          stopToken);
    } else {
        analysisA = AnalyzeLatencyRows(request.sessionA.rows,
                                       request.options.sessionA,
                                       request.sessionA.label,
                                       request.sessionA.path);
    }
    if (analysisA.failed) {
        errorText = analysisA.errorText;
        return false;
    }
    if (stopToken.stop_requested()) {
        errorText = QStringLiteral("Latency Diff was cancelled.");
        return false;
    }

    if (request.sessionB.traceSession) {
        analysisB = AnalyzeLatencySession(request.sessionB.traceSession,
                                          request.options.sessionB,
                                          request.sessionB.label,
                                          stopToken);
    } else {
        analysisB = AnalyzeLatencyRows(request.sessionB.rows,
                                       request.options.sessionB,
                                       request.sessionB.label,
                                       request.sessionB.path);
    }
    if (analysisB.failed) {
        errorText = analysisB.errorText;
        return false;
    }

    report = BuildLatencyDiffReport(std::move(analysisA),
                                    std::move(analysisB),
                                    request.options);
    if (report.failed) {
        errorText = report.errorText;
        return false;
    }
    return true;
}

void LatencyDiffWidget::startBuild(BuildRequest request)
{
    stopBuildThread();
    const quint64 generation = ++buildGeneration_;
    tree_->clear();
    report_.reset();
    progressBar_->show();
    buildButton_->setEnabled(false);
    exportButton_->setEnabled(false);
    setStatusText(QStringLiteral("Building Latency Diff report..."));

    buildThread_ = std::jthread([this, request = std::move(request), generation](std::stop_token stopToken) {
        auto report = std::make_shared<LatencyDiffReport>();
        QString errorText;
        const bool ok = buildReportFromRequest(request, *report, errorText, stopToken);
        if (!ok) {
            report->failed = true;
            report->errorText = errorText;
        }
        if (stopToken.stop_requested()) {
            return;
        }
        QMetaObject::invokeMethod(this,
                                  [this, report, generation]() {
                                      applyReport(report, generation);
                                  },
                                  Qt::QueuedConnection);
    });
}

void LatencyDiffWidget::applyReport(std::shared_ptr<LatencyDiffReport> report, const quint64 generation)
{
    if (generation != buildGeneration_) {
        return;
    }
    progressBar_->hide();
    buildButton_->setEnabled(sessions_.size() >= 2);
    report_ = std::move(report);
    if (!report_ || report_->failed) {
        tree_->clear();
        setStatusText(report_ && !report_->errorText.isEmpty()
                          ? report_->errorText
                          : QStringLiteral("Latency Diff failed."));
        exportButton_->setEnabled(false);
        return;
    }
    populateTree();
    setStatusText(QStringLiteral("Latency Diff ready: %1 rows, %2 vs %3.")
                      .arg(static_cast<qulonglong>(report_->rows.size()))
                      .arg(report_->sessionALabel, report_->sessionBLabel));
    exportButton_->setEnabled(true);
}

void LatencyDiffWidget::populateTree()
{
    tree_->setSortingEnabled(false);
    tree_->clear();
    if (!report_) {
        tree_->setSortingEnabled(true);
        return;
    }

    QHash<QString, QTreeWidgetItem*> groupItems;
    for (const LatencyDiffRow& row : report_->rows) {
        QTreeWidgetItem* parent = nullptr;
        QString currentPath;
        for (int depth = 0; depth < row.path.size(); ++depth) {
            if (depth > 0) {
                currentPath += QLatin1Char('/');
            }
            currentPath += row.path[depth];
            if (QTreeWidgetItem* existing = groupItems.value(currentPath, nullptr)) {
                parent = existing;
                continue;
            }
            auto* item = new DiffTreeItem();
            item->setText(kMetricColumn, row.path[depth]);
            item->setText(kPresenceColumn, depth == row.path.size() - 1
                                           ? presenceText(row.presentInA, row.presentInB)
                                           : QString());
            QFont font = item->font(kMetricColumn);
            font.setBold(depth <= 1);
            item->setFont(kMetricColumn, font);
            if (parent) {
                parent->addChild(item);
            } else {
                tree_->addTopLevelItem(item);
            }
            groupItems.insert(currentPath, item);
            parent = item;
        }

        if (!parent) {
            continue;
        }
        parent->setText(kPresenceColumn, presenceText(row.presentInA, row.presentInB));
        for (const LatencyDiffMetric& metric : row.metrics) {
            auto* item = new DiffTreeItem();
            item->setText(kMetricColumn, QStringLiteral("  %1").arg(metric.name));
            item->setText(kAColumn, FormatLatencyNumber(metric.aValue));
            item->setText(kBColumn, FormatLatencyNumber(metric.bValue));
            item->setText(kDeltaColumn, FormatLatencyNumber(metric.delta));
            item->setText(kPercentColumn, FormatLatencyPercent(metric.percent, metric.percentComparable));
            item->setText(kPresenceColumn, presenceText(row.presentInA, row.presentInB));
            item->setData(kAColumn, kNumericSortRole, metric.aValue);
            item->setData(kBColumn, kNumericSortRole, metric.bValue);
            item->setData(kDeltaColumn, kNumericSortRole, metric.delta);
            item->setData(kPercentColumn, kNumericSortRole, metric.percentComparable ? metric.percent : -1.0e100);
            item->setData(kPercentColumn, kPercentValueRole, metric.percent);
            item->setData(kPercentColumn, kPercentComparableRole, metric.percentComparable);
            parent->addChild(item);
        }
    }

    for (int index = 0; index < tree_->topLevelItemCount(); ++index) {
        tree_->topLevelItem(index)->setExpanded(true);
        for (int child = 0; child < tree_->topLevelItem(index)->childCount(); ++child) {
            tree_->topLevelItem(index)->child(child)->setExpanded(true);
        }
    }
    tree_->setSortingEnabled(true);
}

void LatencyDiffWidget::exportExcel()
{
    if (!report_ || report_->failed) {
        return;
    }
    QString path = QFileDialog::getSaveFileName(this,
                                                QStringLiteral("Export Latency Diff"),
                                                QStringLiteral("latency-diff.xlsx"),
                                                QStringLiteral("Excel Workbook (*.xlsx)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".xlsx");
    }
    QString errorText;
    if (!ExportLatencyDiffReportXlsx(*report_, path, errorText)) {
        QMessageBox::warning(this,
                             QStringLiteral("Export Latency Diff"),
                             errorText.isEmpty()
                                 ? QStringLiteral("Failed to export the workbook.")
                                 : errorText);
        return;
    }
    setStatusText(QStringLiteral("Exported Latency Diff to %1.").arg(path));
}

void LatencyDiffWidget::updateButtonStates()
{
    if (buildButton_) {
        buildButton_->setEnabled(sessions_.size() >= 2);
    }
    if (exportButton_) {
        exportButton_->setEnabled(hasReport());
    }
}

void LatencyDiffWidget::setStatusText(const QString& text)
{
    if (statusLabel_) {
        statusLabel_->setText(text);
        statusLabel_->setToolTip(text);
    }
}

#ifdef CHIRON_GUI_ENABLE_LATENCY_DIFF_TEST_API
bool LatencyDiffWidget::testBuildDefaultDiff()
{
    if (sessions_.size() < 2) {
        return false;
    }
    BuildRequest request;
    request.sessionA = sessions_[0];
    request.sessionB = sessions_[1];
    request.options.sessionALabel = request.sessionA.label;
    request.options.sessionBLabel = request.sessionB.label;
    request.options.sessionA = {};
    request.options.sessionB = {};
    LatencyDiffReport report;
    QString errorText;
    if (!buildReportFromRequest(request, report, errorText, {})) {
        report.failed = true;
        report.errorText = errorText;
    }
    applyReport(std::make_shared<LatencyDiffReport>(std::move(report)), ++buildGeneration_);
    return hasReport();
}

int LatencyDiffWidget::testTopLevelCount() const
{
    return tree_ ? tree_->topLevelItemCount() : 0;
}

int LatencyDiffWidget::testSessionSourceCount() const noexcept
{
    return static_cast<int>(sessions_.size());
}

int LatencyDiffWidget::testChildCount(const std::vector<int>& path) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    return item ? item->childCount() : 0;
}

QString LatencyDiffWidget::testItemText(const std::vector<int>& path, const int column) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    return item ? item->text(column) : QString();
}

QVariant LatencyDiffWidget::testItemData(const std::vector<int>& path, const int column, const int role) const
{
    QTreeWidgetItem* item = itemAtPath(tree_, path);
    return item ? item->data(column, role) : QVariant();
}

bool LatencyDiffWidget::testExportXlsx(const QString& path, QString* errorText) const
{
    if (!report_) {
        if (errorText) {
            *errorText = QStringLiteral("No report.");
        }
        return false;
    }
    QString localError;
    const bool ok = ExportLatencyDiffReportXlsx(*report_, path, localError);
    if (errorText) {
        *errorText = localError;
    }
    return ok;
}
#endif

}  // namespace CHIron::Gui
