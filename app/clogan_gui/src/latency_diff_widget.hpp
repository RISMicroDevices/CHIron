#pragma once

#include "flit_record.hpp"
#include "latency_analysis.hpp"

#include <QString>
#include <QWidget>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QEvent)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QTreeWidget)

namespace CHIron::Gui {

class TraceSession;

struct LatencyDiffSessionSource {
    quint64 id = 0;
    QString label;
    QString path;
    std::shared_ptr<TraceSession> traceSession;
    std::vector<FlitRecord> rows;
};

class LatencyDiffWidget final : public QWidget {
    Q_OBJECT

public:
    explicit LatencyDiffWidget(QWidget* parent = nullptr);
    ~LatencyDiffWidget() override;

    void setSessions(std::vector<LatencyDiffSessionSource> sessions);
    void runWizard();
    void clear();
    bool hasReport() const noexcept;

#ifdef CHIRON_GUI_ENABLE_LATENCY_DIFF_TEST_API
    bool testBuildDefaultDiff();
    int testSessionSourceCount() const noexcept;
    int testTopLevelCount() const;
    int testChildCount(const std::vector<int>& path) const;
    QString testItemText(const std::vector<int>& path, int column) const;
    QVariant testItemData(const std::vector<int>& path, int column, int role) const;
    bool testExportXlsx(const QString& path, QString* errorText = nullptr) const;
#endif

private:
    struct BuildRequest;

    void stopBuildThread();
    bool buildReportFromRequest(const BuildRequest& request,
                                LatencyDiffReport& report,
                                QString& errorText,
                                std::stop_token stopToken);
    void startBuild(BuildRequest request);
    void applyReport(std::shared_ptr<LatencyDiffReport> report, quint64 generation);
    void populateTree();
    void exportExcel();
    void updateButtonStates();
    void setStatusText(const QString& text);

private:
    std::vector<LatencyDiffSessionSource> sessions_;
    std::shared_ptr<LatencyDiffReport> report_;
    std::jthread buildThread_;
    std::atomic<quint64> buildGeneration_ = 0;

    QLabel* statusLabel_ = nullptr;
    QPushButton* buildButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QTreeWidget* tree_ = nullptr;
};

}  // namespace CHIron::Gui
