#pragma once

#include "flit_record.hpp"
#include "trace_session.hpp"

#include <QPoint>
#include <QRect>
#include <QString>
#include <QVariantMap>
#include <QWidget>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

QT_FORWARD_DECLARE_CLASS(QEvent)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QFrame)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QPainter)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QResizeEvent)
QT_FORWARD_DECLARE_CLASS(QTreeWidget)
QT_FORWARD_DECLARE_CLASS(QTreeWidgetItem)

namespace CHIron::Gui {

class LatencyWidget final : public QWidget {
    Q_OBJECT

public:
    explicit LatencyWidget(QWidget* parent = nullptr);
    ~LatencyWidget() override;

    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setRows(std::vector<FlitRecord> rows);
    void buildView();
    void clear();
    QVariantMap viewState() const;
    void restoreViewState(const QVariantMap& state);

Q_SIGNALS:
    void logicalRowActivated(int logicalRow);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    class ScaleWidget;
    class PlotOverlayWidget;
    class BuildPromptOverlayWidget;

#ifdef CHIRON_GUI_ENABLE_LATENCY_TEST_API
public:
    int testColumnCount() const;
    bool testColumnsAreUserResizable() const;
    int testTopLevelCount() const;
    QString testItemText(const std::vector<int>& path, int column) const;
    QString testItemToolTip(const std::vector<int>& path, int column) const;
    int testChildCount(const std::vector<int>& path) const;
    QStringList testItemSummaryLines(const std::vector<int>& path) const;
    QVariantMap testItemStats(const std::vector<int>& path) const;
    void testSelectItem(const std::vector<int>& path);
    QString testSummaryTitle() const;
    QString testSummaryBody() const;
    double testPlotZoom() const noexcept;
    std::pair<double, double> testPlotVisibleRange() const;
    QRect testPlotColumnViewportRect() const;
    QRect testPlotColumnWidgetRect() const;
    QRect testScaleWidgetRect() const;
    QRect testScaleCursorTagWidgetRect() const;
    QRect testItemPlotViewportRect(const std::vector<int>& path) const;
    QPoint testJitterSamplePoint(const std::vector<int>& path, int sampleIndex) const;
    void testSetPlotCursorFromPosition(const QPoint& position);
    std::optional<int> testPlotDotLogicalRowAtPosition(const QPoint& position) const;
    void testBeginPlotLeftDrag(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void testUpdatePlotLeftDrag(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void testFinishPlotLeftDrag(const QPoint& position, Qt::KeyboardModifiers modifiers);
    bool testLastBuildLoadedFromCache() const noexcept;
    QString testPlotType() const;
    void testSetPlotType(const QString& plotType);
#endif

private:
    enum class PlotDragMode {
        None,
        Pending,
        Pan,
        RangeZoom,
        ZoomFull,
        ZoomIn2x,
        ZoomOut2x,
    };

    struct BuildResult;

    void stopBuildThread();
    void refresh();
    void buildRowsFromPendingRows();
    void startSessionBuild(std::shared_ptr<TraceSession> session);
    static bool tryLoadLatencyCache(const TraceSession& session, BuildResult& result);
    static void writeLatencyCache(const TraceSession& session, const BuildResult& result);
    bool buildBucketsFromIndexedRowStream(const std::shared_ptr<TraceSession>& session,
                                          std::stop_token stopToken,
                                          quint64 generation,
                                          BuildResult& result,
                                          QString& errorText);
    void applyBuildResult(std::shared_ptr<BuildResult> result);
    void postBuildProgress(quint64 generation,
                           std::uint64_t processedRows,
                           std::uint64_t totalRows);
    void updateBuildProgress(quint64 generation,
                             std::uint64_t processedXactions,
                             std::uint64_t totalXactions);
    void setIdleText(const QString& text);
    bool hasPendingBuild() const noexcept;
    bool shouldShowBuildPrompt() const noexcept;
    QString pendingBuildText() const;
    QRect buildPromptButtonRect(const QRect& rect) const;
    void updateBuildPromptOverlay();
    void paintBuildPrompt(QPainter& painter, const QRect& rect) const;
    void paintBuildProgress(QPainter& painter, const QRect& bounds) const;
    void clearPlotDataRange();
    void includePlotSampleRange(qint64 minimum, qint64 maximum);
    void fitPlotView();
    void updatePlotViewProperties();
    void clampPlotCenter();
    void zoomPlotByFactor(double factor, const QPoint& focus);
    void zoomPlotToRange(long double firstValue, long double lastValue);
    void panPlotByPixels(int pixels);
    void releasePlotMouseGrab();
    QRect plotColumnViewportRect() const;
    QRect plotTrackViewportRect() const;
    bool positionInPlotColumn(const QPoint& position) const;
    std::pair<long double, long double> effectivePlotDataRange() const;
    std::pair<long double, long double> visiblePlotRange() const;
    void selectPlotItemAt(const QPoint& position);
    void setPlotCursorFromPosition(const QPoint& position);
    std::optional<long double> magneticPlotValueAtPosition(const QPoint& position) const;
    std::optional<int> plotDotLogicalRowAtPosition(const QPoint& position) const;
    bool activatePlotDotAtPosition(const QPoint& position);
    std::optional<long double> plotValueAtX(int x) const;
    PlotDragMode classifyPlotLeftDrag(const QPoint& position, Qt::KeyboardModifiers modifiers) const;
    void beginPlotLeftDrag(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void updatePlotLeftDrag(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void finishPlotLeftDrag(const QPoint& position, Qt::KeyboardModifiers modifiers);
    void cancelPlotLeftDrag();
    void updateSelectionSummary();
    void applyViewState(const QVariantMap& state);
    QRect scaleTrackRect(const QRect& rect) const;
    void paintScale(QPainter& painter, const QRect& rect) const;
    void paintPlotGestureOverlay(QPainter& painter, const QRect& rect) const;

private:
    std::shared_ptr<TraceSession> traceSession_;
    std::vector<FlitRecord> pendingRows_;
    std::jthread buildThread_;
    std::atomic<quint64> buildGeneration_ = 0;
    std::atomic<std::uint64_t> pendingProgressProcessedRows_ = 0;
    std::atomic<std::uint64_t> pendingProgressTotalRows_ = 0;
    std::atomic<quint64> pendingProgressGeneration_ = 0;
    std::atomic_bool progressUpdateQueued_ = false;

    QLabel* statusLabel_ = nullptr;
    QComboBox* plotTypeCombo_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    ScaleWidget* scaleWidget_ = nullptr;
    PlotOverlayWidget* plotOverlay_ = nullptr;
    BuildPromptOverlayWidget* buildPromptOverlay_ = nullptr;
    QFrame* summaryPanel_ = nullptr;
    QLabel* summaryTitleLabel_ = nullptr;
    QLabel* summaryBodyLabel_ = nullptr;

    bool plotHasDataRange_ = false;
    qint64 plotDataMin_ = 0;
    qint64 plotDataMax_ = 1;
    double plotZoom_ = 1.0;
    long double plotCenter_ = 0.5L;
    bool plotHasCursor_ = false;
    bool lastBuildLoadedFromCache_ = false;
    bool buildRequested_ = false;
    bool building_ = false;
    std::uint64_t buildProgressProcessedRows_ = 0;
    std::uint64_t buildProgressTotalRows_ = 0;
    int currentPlotType_ = 0;
    long double plotCursorValue_ = 0.0L;
    PlotDragMode plotDragMode_ = PlotDragMode::None;
    bool plotPanning_ = false;
    QPoint plotPanStart_;
    long double plotPanStartCenter_ = 0.5L;
    QPoint plotDragStart_;
    QPoint plotDragLast_;
    long double plotDragStartCenter_ = 0.5L;
    QVariantMap pendingViewState_;
};

}  // namespace CHIron::Gui
