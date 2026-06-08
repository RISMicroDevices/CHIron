#pragma once

#include "trace_issue_store.hpp"

#include <QWidget>

#include <cstdint>
#include <memory>
#include <vector>

class QLabel;
class QModelIndex;
class QPoint;
class QProgressBar;
class QTreeView;
class QToolButton;

namespace CHIron::Gui {

class ErrorsModel;

class ErrorsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ErrorsWidget(QWidget* parent = nullptr);

    void setIssues(const std::vector<TraceIssue>& issues,
                   std::uint64_t sourceId = 0,
                   std::uint64_t generation = 0);
    void setIssues(std::shared_ptr<const std::vector<TraceIssue>> issues,
                   TraceIssueCounts counts,
                   std::uint64_t sourceId = 0,
                   std::uint64_t generation = 0);
    void setBuildStatus(bool active,
                        const QString& text = QString(),
                        std::uint64_t completedRows = 0,
                        std::uint64_t totalRows = 0);
    void clear();
    void setIssueDisposition(TraceIssueSource source, TraceIssueDisposition disposition);
    TraceIssueDisposition issueDisposition(TraceIssueSource source) const noexcept;
    void setSeverityVisible(TraceIssueSeverity severity, bool visible);
    bool severityVisible(TraceIssueSeverity severity) const noexcept;

    int warningCount() const noexcept;
    int errorCount() const noexcept;
    int issueCount() const noexcept;
    bool hasIssuesFor(std::uint64_t sourceId,
                      std::uint64_t generation,
                      int issueCount,
                      TraceIssueCounts counts) const noexcept;

#ifdef CHIRON_GUI_ENABLE_MAIN_WINDOW_TEST_API
    QString testStatusText() const;
    QString testTopLevelSummary(int row) const;
    QString testCodeText(int row) const;
    QString testDescriptionText(int row) const;
    QString testSourceText(int row) const;
    QString testRowText(int row) const;
    bool testTopLevelHasSeverityIcon(int row) const;
    QString testDetailText(int row) const;
    QString testExpandedDetailText(int row) const;
    bool testIssueExpanded(int row) const;
    bool testSetIssueExpanded(int row, bool expanded);
    QString testIssueContextToggleText(int row) const;
    bool testIssueTreeRootDecorated() const noexcept;
    int testIssueTreeIndentation() const noexcept;
    bool testActivateIssue(int row);
    QString testIssueDispositionText(TraceIssueSource source) const;
    bool testSetIssueDisposition(TraceIssueSource source, TraceIssueDisposition disposition);
    bool testSeverityVisible(TraceIssueSeverity severity) const noexcept;
    bool testSetSeverityVisible(TraceIssueSeverity severity, bool visible);
    bool testProgressVisible() const noexcept;
    bool testProgressBusy() const noexcept;
    int testProgressValue() const noexcept;
    int testProgressMaximum() const noexcept;
    QString testSeverityButtonText(TraceIssueSeverity severity) const;
    bool testSeverityButtonHasIcon(TraceIssueSeverity severity) const noexcept;
    QString testDispositionButtonText(TraceIssueSource source) const;
    bool testDispositionButtonHasIcon(TraceIssueSource source) const noexcept;
    bool testDispositionButtonCheckable(TraceIssueSource source) const noexcept;
    bool testDispositionButtonChecked(TraceIssueSource source) const noexcept;
    bool testClickDispositionButton(TraceIssueSource source);
    int testDisplayedIssueRowCount() const noexcept;
    void testSetIssuePageSize(int pageSize);
    bool testFetchMoreIssueRows();
#endif

Q_SIGNALS:
    void logicalRowActivated(int logicalRow);
    void issueDispositionChanged(TraceIssueSource source, TraceIssueDisposition disposition);
    void severityVisibilityChanged(TraceIssueSeverity severity, bool visible);

private:
    void buildUi();
    void refreshSummary();
    void refreshDispositionButtons();
    void refreshSeverityButtons();
    void activateIndex(const QModelIndex& index);
    void handleIssueDoubleClicked(const QModelIndex& index);
    void showIssueContextMenu(const QPoint& position);
    void setIssueExpanded(int row, bool expanded);
    bool issueExpanded(int row) const;
    void spanIssueDetailRow(const QModelIndex& issueIndex);
    QString issueContextToggleText(int row) const;

private:
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QToolButton* errorToggleButton_ = nullptr;
    QToolButton* warningToggleButton_ = nullptr;
    QToolButton* xactionDispositionButton_ = nullptr;
    QToolButton* cacheDispositionButton_ = nullptr;
    QTreeView* tree_ = nullptr;
    ErrorsModel* model_ = nullptr;
    TraceIssueDisposition xactionDisposition_ = TraceIssueDisposition::Error;
    TraceIssueDisposition cacheStateDisposition_ = TraceIssueDisposition::Error;
    bool errorsVisible_ = true;
    bool warningsVisible_ = true;
    bool buildActive_ = false;
    QString buildText_;
    std::uint64_t completedRows_ = 0;
    std::uint64_t totalRows_ = 0;
};

}  // namespace CHIron::Gui
