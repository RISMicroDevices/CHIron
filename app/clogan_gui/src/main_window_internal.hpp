#pragma once
#include "main_window.hpp"

#include "bug_reporter.hpp"
#include "address_widget.hpp"
#include "cache_widget.hpp"
#include "channel_badge_painter.hpp"
#include "clipboard_widget.hpp"
#include "clog_b_trace_loader.hpp"
#include "config.hpp"
#include "errors_widget.hpp"
#include "flit_edit_adapter.hpp"
#include "flit_transaction_keys.hpp"
#include "gui_format.hpp"
#include "latency_diff_widget.hpp"
#include "latency_widget.hpp"
#include "marker_store.hpp"
#include "marker_widget.hpp"
#include "timeline_widget.hpp"
#include "trace_cache_line_minimap.hpp"
#include "trace_marker_overlay.hpp"
#include "transaction_widget.hpp"
#include "trace_session.hpp"

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

#include <QAbstractTableModel>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QHelpEvent>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMetaObject>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStackedLayout>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTableWidget>
#include <QTimer>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextStream>
#include <QScreen>
#include <QTabBar>
#include <QToolButton>
#include <QToolBar>
#include <QToolTip>
#include <QShortcut>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>

#include <array>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace CHIron::Gui::MainWindowDetail {



constexpr auto kGeometrySettingsKey = "windowGeometry";
constexpr auto kLayoutSettingsKey = "dockLayout";
constexpr auto kLayoutSettingsFileName = "clogan_gui_layout.ini";
constexpr int kMaxXactionDebugLoggedRows = 512;

inline bool IsNodeLabelColumnName(const QString& columnName)
{
    return columnName == QLatin1String("SrcID")
        || columnName == QLatin1String("TgtID")
        || columnName == QLatin1String("ReturnNID")
        || columnName == QLatin1String("HomeNID")
        || columnName == QLatin1String("FwdNID");
}

inline QColor NodeTypeLabelColor(const QString& label)
{
    if (label == QLatin1String("RN-F")) {
        return QColor(QStringLiteral("#2F80ED"));
    }
    if (label == QLatin1String("RN-D")) {
        return QColor(QStringLiteral("#56CCF2"));
    }
    if (label == QLatin1String("RN-I")) {
        return QColor(QStringLiteral("#00A878"));
    }
    if (label == QLatin1String("HN-F")) {
        return QColor(QStringLiteral("#9B51E0"));
    }
    if (label == QLatin1String("HN-I")) {
        return QColor(QStringLiteral("#6C63FF"));
    }
    if (label == QLatin1String("SN-F")) {
        return QColor(QStringLiteral("#EB5757"));
    }
    if (label == QLatin1String("SN-I")) {
        return QColor(QStringLiteral("#F2994A"));
    }
    if (label == QLatin1String("MN")) {
        return QColor(QStringLiteral("#219653"));
    }
    if (label == QLatin1String("Before SAM")) {
        return QColor(QStringLiteral("#D5DAE1"));
    }
    return QColor(QStringLiteral("#8B949E"));
}

inline bool ParseNodeIdText(QString text, qulonglong& value)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return false;
    }

    bool ok = false;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = text.sliced(2).toULongLong(&ok, 16);
        return ok;
    }

    bool hasHexLetter = false;
    for (const QChar ch : text) {
        if (ch.isDigit()) {
            continue;
        }

        const ushort code = ch.unicode();
        if ((code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F')) {
            hasHexLetter = true;
            continue;
        }

        return false;
    }

    value = text.toULongLong(&ok, hasHexLetter ? 16 : 10);
    return ok;
}

inline bool XactionNodeLabelForRecord(const FlitRecord& record,
                               const bool targetColumn,
                               const bool labelsVisible,
                               const CLogBTraceMetadata* metadata,
                               QString& label,
                               QColor& color)
{
    label.clear();
    color = QColor();

    if (!labelsVisible) {
        return false;
    }

    if (targetColumn && record.dimTarget && !record.channelTag.isEmpty()) {
        label = record.channelTag;
        color = NodeTypeLabelColor(label);
        return !label.isEmpty();
    }

    qulonglong nodeId = 0;
    if (!ParseNodeIdText(targetColumn ? record.target : record.source, nodeId)) {
        return false;
    }

    if (metadata) {
        const auto found = metadata->nodeTypes.find(static_cast<quint16>(nodeId));
        if (found != metadata->nodeTypes.cend()) {
            label = QString::fromStdString(CLog::NodeTypeToString(found->second));
        }
    }
    if (label.isEmpty()) {
        label = QStringLiteral("unknown");
    }
    color = NodeTypeLabelColor(label);
    return true;
}

inline QSettings MakeLayoutSettings()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    return QSettings(appDir.filePath(QLatin1String(kLayoutSettingsFileName)), QSettings::IniFormat);
}

struct ProcessMemorySample {
    quint64 workingSetBytes = 0;
    quint64 privateBytes = 0;
};

inline ProcessMemorySample QueryProcessMemory()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters))) {
        return ProcessMemorySample{
            .workingSetBytes = static_cast<quint64>(counters.WorkingSetSize),
            .privateBytes = static_cast<quint64>(counters.PrivateUsage),
        };
    }
#endif

    return {};
}

inline QString FormatBytes(const quint64 bytes)
{
    static constexpr const char* kUnits[] = {"B", "KiB", "MiB", "GiB", "TiB"};

    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }

    if (unitIndex == 0) {
        return QStringLiteral("%1 %2")
            .arg(static_cast<qulonglong>(bytes))
            .arg(QLatin1String(kUnits[unitIndex]));
    }

    return QStringLiteral("%1 %2")
        .arg(QString::number(value, 'f', value >= 100.0 ? 0 : 1))
        .arg(QLatin1String(kUnits[unitIndex]));
}

inline QString FormatTransferRate(const double bytesPerSecond)
{
    if (bytesPerSecond <= 0.0) {
        return QStringLiteral("0 B/s");
    }

    return QStringLiteral("%1/s").arg(FormatBytes(static_cast<quint64>(bytesPerSecond)));
}

inline QString FormatRecordRate(const double recordsPerSecond)
{
    if (recordsPerSecond <= 0.0) {
        return QStringLiteral("0 rec/s");
    }

    double value = recordsPerSecond;
    QString suffix = QStringLiteral("rec/s");
    if (value >= 1000000.0) {
        value /= 1000000.0;
        suffix = QStringLiteral("M rec/s");
    } else if (value >= 1000.0) {
        value /= 1000.0;
        suffix = QStringLiteral("K rec/s");
    }

    return QStringLiteral("%1 %2")
        .arg(QString::number(value, 'f', value >= 100.0 ? 0 : 1))
        .arg(suffix);
}

inline QString BoolText(const bool value)
{
    return value ? QStringLiteral("true")
                 : QStringLiteral("false");
}

inline QString XactionIndexStateText(const FlitRecord& record)
{
    if (!record.xactionIndexChecked
        && record.xactionIndexState != XactionIndexState::NotStarted) {
        return QStringLiteral("Not indexed");
    }

    switch (record.xactionIndexState) {
    case XactionIndexState::NotStarted:
        return QStringLiteral("Not started");
    case XactionIndexState::Pending:
        return QStringLiteral("Pending");
    case XactionIndexState::Indexed:
        return QStringLiteral("Indexed");
    case XactionIndexState::Denied:
        return QStringLiteral("Denied");
    }

    return QStringLiteral("Unknown");
}

inline QString XactionGroupKeyText(const FlitRecord& record)
{
    return record.transactionGroupKey.isEmpty() ? QStringLiteral("-")
                                                : record.transactionGroupKey;
}

inline QString XactionChannelNodeIdText(const FlitRecord& record)
{
    return record.channelNodeId
        ? QString::number(static_cast<qulonglong>(*record.channelNodeId))
        : QStringLiteral("-");
}

inline QString IssueText(const CLog::Issue issue)
{
    switch (issue) {
    case CLog::Issue::B:
        return QStringLiteral("B");
    case CLog::Issue::Eb:
        return QStringLiteral("E.b");
    }

    return QStringLiteral("Unknown");
}

inline QString DisplayModeText(const FlitTableModel::DisplayMode mode)
{
    switch (mode) {
    case FlitTableModel::DisplayMode::Decoded:
        return QStringLiteral("Decoded");
    case FlitTableModel::DisplayMode::Decimal:
        return QStringLiteral("Decimal");
    case FlitTableModel::DisplayMode::Hexadecimal:
        return QStringLiteral("Hexadecimal");
    }

    return QStringLiteral("Unknown");
}

inline QString LoadStageText(const CLogBTraceLoadStage stage, const std::size_t workerCount)
{
    switch (stage) {
    case CLogBTraceLoadStage::Opening:
        return QStringLiteral("Opening trace file...");
    case CLogBTraceLoadStage::Parsing:
        return QStringLiteral("Reading CLog.B stream structure...");
    case CLogBTraceLoadStage::Decoding:
        return workerCount > 1
            ? QStringLiteral("Decoding flits with %1 workers...").arg(workerCount)
            : QStringLiteral("Decoding flits...");
    case CLogBTraceLoadStage::Formatting:
        return workerCount > 1
            ? QStringLiteral("Formatting decoded rows with %1 workers...").arg(workerCount)
            : QStringLiteral("Formatting decoded rows...");
    case CLogBTraceLoadStage::Finalizing:
        return QStringLiteral("Finalizing trace indexes...");
    case CLogBTraceLoadStage::CollectingCacheStateErrors:
        return QStringLiteral("Collecting cache-state errors...");
    case CLogBTraceLoadStage::FinalizingIndexDebug:
        return QStringLiteral("Validating xaction debug index...");
    case CLogBTraceLoadStage::FinalizingIndexLayout:
        return QStringLiteral("Checking xaction group directory...");
    case CLogBTraceLoadStage::FinalizingIndexRows:
        return workerCount > 0
            ? QStringLiteral("Preparing xaction row maps with %1 workers...").arg(workerCount)
            : QStringLiteral("Validating compact xaction row maps...");
    case CLogBTraceLoadStage::FinalizingIndexRowDirectory:
        return workerCount > 0
            ? QStringLiteral("Writing xaction index directories...")
            : QStringLiteral("Validating compact xaction index directories...");
    case CLogBTraceLoadStage::Completed:
        return QStringLiteral("Completing load...");
    }

    return QStringLiteral("Loading...");
}

inline bool LoadStageUsesStageProgress(const CLogBTraceLoadStage stage)
{
    switch (stage) {
    case CLogBTraceLoadStage::Decoding:
    case CLogBTraceLoadStage::Formatting:
    case CLogBTraceLoadStage::Finalizing:
    case CLogBTraceLoadStage::CollectingCacheStateErrors:
    case CLogBTraceLoadStage::FinalizingIndexDebug:
    case CLogBTraceLoadStage::FinalizingIndexLayout:
    case CLogBTraceLoadStage::FinalizingIndexRows:
    case CLogBTraceLoadStage::FinalizingIndexRowDirectory:
        return true;
    case CLogBTraceLoadStage::Opening:
    case CLogBTraceLoadStage::Parsing:
    case CLogBTraceLoadStage::Completed:
        return false;
    }

    return false;
}

inline QString LoadStageProgressUnit(const CLogBTraceLoadStage stage, const std::size_t count)
{
    const auto plural = [count](const char* singular, const char* pluralText) {
        return QLatin1String(count == 1 ? singular : pluralText);
    };

    switch (stage) {
    case CLogBTraceLoadStage::FinalizingIndexDebug:
        return plural("debug chunk", "debug chunks");
    case CLogBTraceLoadStage::FinalizingIndexLayout:
        return plural("directory check", "directory checks");
    case CLogBTraceLoadStage::FinalizingIndexRows:
        return plural("row-map chunk", "row-map chunks");
    case CLogBTraceLoadStage::FinalizingIndexRowDirectory:
        return plural("compact directory item", "compact directory items");
    case CLogBTraceLoadStage::Decoding:
    case CLogBTraceLoadStage::Formatting:
    case CLogBTraceLoadStage::Finalizing:
    case CLogBTraceLoadStage::CollectingCacheStateErrors:
        return plural("record", "records");
    case CLogBTraceLoadStage::Opening:
    case CLogBTraceLoadStage::Parsing:
    case CLogBTraceLoadStage::Completed:
        return plural("item", "items");
    }

    return plural("item", "items");
}

inline int SessionBackedColumnWidth(const int logicalColumn)
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

inline int TraceTableRowHeightForVisibleRows(const int visibleRowCount)
{
    constexpr int kPreferredRowHeight = 22;
    constexpr qint64 kPixelBudget = static_cast<qint64>(std::numeric_limits<int>::max()) - 65536;

    if (visibleRowCount <= 0) {
        return kPreferredRowHeight;
    }

    const qint64 safeHeight = kPixelBudget / static_cast<qint64>(visibleRowCount);
    return qBound(1, static_cast<int>(safeHeight), kPreferredRowHeight);
}

inline QString StatisticsBoolText(const bool value)
{
    return value ? QStringLiteral("Yes")
                 : QStringLiteral("No");
}

inline QString StatisticsCountText(const std::uint64_t value)
{
    return QString::number(static_cast<qulonglong>(value));
}

class WaitCursorGuard final {
public:
    WaitCursorGuard()
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    }

    ~WaitCursorGuard()
    {
        QApplication::restoreOverrideCursor();
    }
};

inline QString GuiIssueName(const CLog::Issue issue)
{
    switch (issue) {
    case CLog::Issue::B:
        return QStringLiteral("B");
    case CLog::Issue::Eb:
        return QStringLiteral("E.b");
    }

    return QStringLiteral("Unknown");
}

inline QString GuiBoolText(const bool value)
{
    return value ? QStringLiteral("on")
                 : QStringLiteral("off");
}

inline QString GuessFlitKindName(const std::size_t index)
{
    switch (index) {
    case 0:
        return QStringLiteral("REQ");
    case 1:
        return QStringLiteral("RSP");
    case 2:
        return QStringLiteral("DAT");
    case 3:
        return QStringLiteral("SNP");
    }

    return QStringLiteral("Unknown");
}

inline QString ObservedLengthText(const std::vector<std::uint8_t>& byteLengths)
{
    if (byteLengths.empty()) {
        return QStringLiteral("not present");
    }

    QStringList values;
    for (const std::uint8_t byteLength : byteLengths) {
        values.append(QStringLiteral("%1 B").arg(static_cast<unsigned int>(byteLength)));
    }
    return values.join(QStringLiteral(", "));
}

inline QString ObservedLengthsSummary(const CLogBTraceConfigurationGuessResult& result)
{
    QStringList parts;
    for (std::size_t index = 0; index < result.observedByteLengths.size(); ++index) {
        if (!result.observedByteLengths[index].empty()) {
            parts.append(QStringLiteral("%1 %2")
                             .arg(GuessFlitKindName(index),
                                  ObservedLengthText(result.observedByteLengths[index])));
        }
    }

    return parts.isEmpty() ? QStringLiteral("No flit records found.")
                           : parts.join(QStringLiteral("  |  "));
}

constexpr int kGuessParameterColumnCount = 9;
constexpr int kGuessColumnCount = 13;
constexpr int kGuessWidthColumnOffset = kGuessParameterColumnCount;

inline QString GuessColumnName(const int section)
{
    switch (section) {
    case 0:
        return QStringLiteral("Issue");
    case 1:
        return QStringLiteral("NodeID");
    case 2:
        return QStringLiteral("ReqAddr");
    case 3:
        return QStringLiteral("ReqRSVDC");
    case 4:
        return QStringLiteral("DatRSVDC");
    case 5:
        return QStringLiteral("Data");
    case 6:
        return QStringLiteral("DataCheck");
    case 7:
        return QStringLiteral("Poison");
    case 8:
        return QStringLiteral("MPAM");
    case 9:
        return QStringLiteral("REQ Width");
    case 10:
        return QStringLiteral("RSP Width");
    case 11:
        return QStringLiteral("DAT Width");
    case 12:
        return QStringLiteral("SNP Width");
    default:
        return QString();
    }
}

inline QString GuessParameterText(const CLog::Parameters& params, const int column)
{
    switch (column) {
    case 0:
        return GuiIssueName(params.GetIssue());
    case 1:
        return QString::number(static_cast<qulonglong>(params.GetNodeIdWidth()));
    case 2:
        return QString::number(static_cast<qulonglong>(params.GetReqAddrWidth()));
    case 3:
        return QString::number(static_cast<qulonglong>(params.GetReqRSVDCWidth()));
    case 4:
        return QString::number(static_cast<qulonglong>(params.GetDatRSVDCWidth()));
    case 5:
        return QString::number(static_cast<qulonglong>(params.GetDataWidth()));
    case 6:
        return GuiBoolText(params.IsDataCheckPresent());
    case 7:
        return GuiBoolText(params.IsPoisonPresent());
    case 8:
        return GuiBoolText(params.IsMPAMPresent());
    default:
        return QString();
    }
}

inline bool GuessParameterDiffers(const CLog::Parameters& guess,
                           const CLog::Parameters& declared,
                           const int column) noexcept
{
    switch (column) {
    case 0:
        return guess.GetIssue() != declared.GetIssue();
    case 1:
        return guess.GetNodeIdWidth() != declared.GetNodeIdWidth();
    case 2:
        return guess.GetReqAddrWidth() != declared.GetReqAddrWidth();
    case 3:
        return guess.GetReqRSVDCWidth() != declared.GetReqRSVDCWidth();
    case 4:
        return guess.GetDatRSVDCWidth() != declared.GetDatRSVDCWidth();
    case 5:
        return guess.GetDataWidth() != declared.GetDataWidth();
    case 6:
        return guess.IsDataCheckPresent() != declared.IsDataCheckPresent();
    case 7:
        return guess.IsPoisonPresent() != declared.IsPoisonPresent();
    case 8:
        return guess.IsMPAMPresent() != declared.IsMPAMPresent();
    default:
        return false;
    }
}

inline QString GuessWidthText(const CLogBTraceConfigurationGuess& guess, const std::size_t channelIndex)
{
    return QStringLiteral("%1 bits (%2 B stored)")
        .arg(static_cast<unsigned int>(guess.expectedBitWidths[channelIndex]))
        .arg(static_cast<unsigned int>(guess.expectedByteLengths[channelIndex]));
}

inline QString GuessSearchText(const CLogBTraceConfigurationGuess& guess)
{
    QStringList parts;
    const CLog::Parameters& params = guess.parameters;
    for (int column = 0; column < kGuessParameterColumnCount; ++column) {
        const QString name = GuessColumnName(column);
        const QString value = GuessParameterText(params, column);
        parts.append(name);
        parts.append(value);
        parts.append(QStringLiteral("%1=%2").arg(name, value));
    }

    for (std::size_t channelIndex = 0; channelIndex < guess.expectedBitWidths.size(); ++channelIndex) {
        const QString channel = GuessFlitKindName(channelIndex);
        const QString bitWidth =
            QString::number(static_cast<unsigned int>(guess.expectedBitWidths[channelIndex]));
        const QString byteLength =
            QString::number(static_cast<unsigned int>(guess.expectedByteLengths[channelIndex]));
        parts.append(channel);
        parts.append(QStringLiteral("%1 width").arg(channel));
        parts.append(QStringLiteral("%1 bits").arg(bitWidth));
        parts.append(QStringLiteral("%1 B").arg(byteLength));
        parts.append(QStringLiteral("%1=%2").arg(channel, bitWidth));
        parts.append(QStringLiteral("%1=%2B").arg(channel, byteLength));
        parts.append(GuessWidthText(guess, channelIndex));
    }

    return parts.join(QLatin1Char(' ')).toCaseFolded();
}

inline QString ParameterSummary(const CLog::Parameters& params)
{
    return QStringLiteral("Issue %1, NodeID %2, ReqAddr %3, ReqRSVDC %4, DatRSVDC %5, Data %6, DataCheck %7, Poison %8, MPAM %9")
        .arg(GuiIssueName(params.GetIssue()))
        .arg(static_cast<qulonglong>(params.GetNodeIdWidth()))
        .arg(static_cast<qulonglong>(params.GetReqAddrWidth()))
        .arg(static_cast<qulonglong>(params.GetReqRSVDCWidth()))
        .arg(static_cast<qulonglong>(params.GetDatRSVDCWidth()))
        .arg(static_cast<qulonglong>(params.GetDataWidth()))
        .arg(GuiBoolText(params.IsDataCheckPresent()))
        .arg(GuiBoolText(params.IsPoisonPresent()))
        .arg(GuiBoolText(params.IsMPAMPresent()));
}

class ConfigurationGuessTableModel final : public QAbstractTableModel {
public:
    explicit ConfigurationGuessTableModel(const std::vector<CLogBTraceConfigurationGuess>& guesses,
                                          CLog::Parameters declaredParameters,
                                          QObject* parent = nullptr)
        : QAbstractTableModel(parent)
        , guesses_(guesses)
        , declaredParameters_(declaredParameters)
    {
        rebuildVisibleRows();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(visibleRows_.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : kGuessColumnCount;
    }

    QVariant headerData(const int section,
                        const Qt::Orientation orientation,
                        const int role = Qt::DisplayRole) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return QVariant();
        }

        return GuessColumnName(section);
    }

    QVariant data(const QModelIndex& index, const int role = Qt::DisplayRole) const override
    {
        if (!index.isValid()
            || index.row() < 0
            || index.row() >= static_cast<int>(visibleRows_.size())) {
            return QVariant();
        }

        const CLogBTraceConfigurationGuess& guess =
            guesses_[visibleRows_[static_cast<std::size_t>(index.row())]];
        const CLog::Parameters& params = guess.parameters;
        const bool differsFromDeclared =
            GuessParameterDiffers(params, declaredParameters_, index.column());
        if (role == Qt::TextAlignmentRole) {
            return index.column() == 0
                ? int(Qt::AlignLeft | Qt::AlignVCenter)
                : int(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (role == Qt::BackgroundRole && differsFromDeclared) {
            return QBrush(QColor(QStringLiteral("#FFF1B8")));
        }
        if (role == Qt::ForegroundRole && differsFromDeclared) {
            return QColor(QStringLiteral("#6F3B00"));
        }
        if (role == Qt::FontRole && differsFromDeclared) {
            QFont font;
            font.setBold(true);
            return font;
        }
        if (role == Qt::ToolTipRole && differsFromDeclared) {
            return QStringLiteral("%1 from CLog.B: %2\nGuessed: %3")
                .arg(GuessColumnName(index.column()),
                     GuessParameterText(declaredParameters_, index.column()),
                     GuessParameterText(params, index.column()));
        }
        if (role != Qt::DisplayRole) {
            return QVariant();
        }

        if (index.column() >= 0 && index.column() < kGuessParameterColumnCount) {
            return GuessParameterText(params, index.column());
        }

        switch (index.column()) {
        case 9:
        case 10:
        case 11:
        case 12:
            return GuessWidthText(guess, static_cast<std::size_t>(index.column() - kGuessWidthColumnOffset));
        default:
            return QVariant();
        }
    }

    void setSearchText(const QString& text)
    {
        const QString normalized = text.simplified().toCaseFolded();
        if (searchText_ == normalized) {
            return;
        }

        searchText_ = normalized;
        resetFilter();
    }

    void setLockedParameter(const int column, const bool locked)
    {
        if (column < 0 || column >= kGuessParameterColumnCount) {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(column);
        if (lockedParameters_[index] == locked) {
            return;
        }

        lockedParameters_[index] = locked;
        resetFilter();
    }

    const CLogBTraceConfigurationGuess* guessAtRow(const int row) const
    {
        if (row < 0 || row >= static_cast<int>(visibleRows_.size())) {
            return nullptr;
        }

        return &guesses_[visibleRows_[static_cast<std::size_t>(row)]];
    }

    int totalGuessCount() const noexcept
    {
        return static_cast<int>(guesses_.size());
    }

private:
    bool acceptsLockedParameters(const CLogBTraceConfigurationGuess& guess) const
    {
        for (int column = 0; column < kGuessParameterColumnCount; ++column) {
            if (lockedParameters_[static_cast<std::size_t>(column)]
                && GuessParameterDiffers(guess.parameters, declaredParameters_, column)) {
                return false;
            }
        }
        return true;
    }

    bool acceptsSearchText(const CLogBTraceConfigurationGuess& guess) const
    {
        if (searchText_.isEmpty()) {
            return true;
        }

        const QString haystack = GuessSearchText(guess);
        const QStringList tokens = searchText_.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString& token : tokens) {
            if (!haystack.contains(token)) {
                return false;
            }
        }
        return true;
    }

    void rebuildVisibleRows()
    {
        visibleRows_.clear();
        visibleRows_.reserve(guesses_.size());
        for (std::size_t index = 0; index < guesses_.size(); ++index) {
            const CLogBTraceConfigurationGuess& guess = guesses_[index];
            if (acceptsLockedParameters(guess) && acceptsSearchText(guess)) {
                visibleRows_.push_back(index);
            }
        }
    }

    void resetFilter()
    {
        beginResetModel();
        rebuildVisibleRows();
        endResetModel();
    }

    const std::vector<CLogBTraceConfigurationGuess>& guesses_;
    CLog::Parameters declaredParameters_;
    std::vector<std::size_t> visibleRows_;
    std::array<bool, kGuessParameterColumnCount> lockedParameters_{};
    QString searchText_;
};

inline std::optional<CLog::Parameters> showConfigurationGuessDialog(
    QWidget* parent,
    const QString& sourceName,
    const CLogBTraceConfigurationGuessResult& result)
{
    if (result.guesses.empty()) {
        QMessageBox::information(parent,
                                 QStringLiteral("Guess CHI Configuration"),
                                 QStringLiteral("No CHI parameter combinations matched the observed flit lengths."));
        return std::nullopt;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Guess CHI Configuration"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QLabel* titleLabel = new QLabel(
        QStringLiteral("%1 possible CHI configuration%2")
            .arg(static_cast<qulonglong>(result.guesses.size()))
            .arg(result.guesses.size() == 1 ? QString() : QStringLiteral("s")),
        &dialog);
    titleLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:700; color:#18212B;"));
    layout->addWidget(titleLabel);

    if (!sourceName.isEmpty()) {
        QLabel* sourceLabel = new QLabel(QStringLiteral("File: %1").arg(sourceName), &dialog);
        sourceLabel->setObjectName(QStringLiteral("secondaryLabel"));
        sourceLabel->setWordWrap(true);
        layout->addWidget(sourceLabel);
    }

    QLabel* summaryLabel = new QLabel(
        QStringLiteral("Observed flit lengths: %1").arg(ObservedLengthsSummary(result)),
        &dialog);
    summaryLabel->setObjectName(QStringLiteral("secondaryLabel"));
    summaryLabel->setWordWrap(true);
    layout->addWidget(summaryLabel);

    QFrame* originalFrame = new QFrame(&dialog);
    originalFrame->setFrameShape(QFrame::StyledPanel);
    originalFrame->setStyleSheet(
        QStringLiteral("QFrame { background:#F8FAFC; border:1px solid #D6DEE6; border-radius:4px; }"));
    QVBoxLayout* originalLayout = new QVBoxLayout(originalFrame);
    originalLayout->setContentsMargins(10, 8, 10, 8);
    originalLayout->setSpacing(4);

    QLabel* originalCaption = new QLabel(QStringLiteral("CLog.B file flit configuration"), originalFrame);
    originalCaption->setStyleSheet(QStringLiteral("font-weight:700; color:#18212B;"));
    originalLayout->addWidget(originalCaption);

    QLabel* originalValues = new QLabel(ParameterSummary(result.declaredParameters), originalFrame);
    originalValues->setStyleSheet(QStringLiteral("color:#3F3F46;"));
    originalValues->setWordWrap(true);
    originalLayout->addWidget(originalValues);
    layout->addWidget(originalFrame);

    QLabel* differenceLabel =
        new QLabel(QStringLiteral("Candidate parameter cells that differ from the CLog.B file are highlighted."),
                   &dialog);
    differenceLabel->setObjectName(QStringLiteral("secondaryLabel"));
    differenceLabel->setWordWrap(true);
    layout->addWidget(differenceLabel);

    ConfigurationGuessTableModel* model =
        new ConfigurationGuessTableModel(result.guesses, result.declaredParameters, &dialog);

    QFrame* filterFrame = new QFrame(&dialog);
    filterFrame->setFrameShape(QFrame::StyledPanel);
    filterFrame->setStyleSheet(
        QStringLiteral("QFrame { background:#FFFFFF; border:1px solid #D6DEE6; border-radius:4px; }"));
    QVBoxLayout* filterLayout = new QVBoxLayout(filterFrame);
    filterLayout->setContentsMargins(10, 8, 10, 8);
    filterLayout->setSpacing(8);

    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(8);

    QLabel* searchLabel = new QLabel(QStringLiteral("Filter"), filterFrame);
    searchLabel->setStyleSheet(QStringLiteral("font-weight:700; color:#18212B;"));
    searchLayout->addWidget(searchLabel);

    QLineEdit* searchEdit = new QLineEdit(filterFrame);
    searchEdit->setPlaceholderText(QStringLiteral("Search parameters or flit widths"));
    searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(searchEdit, 1);

    QLabel* countLabel = new QLabel(filterFrame);
    countLabel->setObjectName(QStringLiteral("secondaryLabel"));
    searchLayout->addWidget(countLabel);
    filterLayout->addLayout(searchLayout);

    QGridLayout* lockLayout = new QGridLayout();
    lockLayout->setContentsMargins(0, 0, 0, 0);
    lockLayout->setHorizontalSpacing(10);
    lockLayout->setVerticalSpacing(4);
    QLabel* lockLabel = new QLabel(QStringLiteral("Lock CLog.B values"), filterFrame);
    lockLabel->setStyleSheet(QStringLiteral("font-weight:700; color:#18212B;"));
    lockLayout->addWidget(lockLabel, 0, 0);
    for (int column = 0; column < kGuessParameterColumnCount; ++column) {
        QCheckBox* lockBox = new QCheckBox(GuessColumnName(column), filterFrame);
        lockBox->setToolTip(QStringLiteral("Keep %1 = %2")
                                .arg(GuessColumnName(column),
                                     GuessParameterText(result.declaredParameters, column)));
        const int row = 1 + (column / 5);
        const int gridColumn = column % 5;
        lockLayout->addWidget(lockBox, row, gridColumn);
        QObject::connect(lockBox, &QCheckBox::toggled, &dialog, [model, column](const bool checked) {
            model->setLockedParameter(column, checked);
        });
    }
    filterLayout->addLayout(lockLayout);
    layout->addWidget(filterFrame);

    QTableView* table = new QTableView(&dialog);
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setWordWrap(false);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setHighlightSections(false);
    table->setMinimumWidth(860);
    table->setMinimumHeight(300);
    layout->addWidget(table, 1);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    QPushButton* selectButton = buttonBox->addButton(QStringLiteral("Use Selected"),
                                                     QDialogButtonBox::AcceptRole);
    selectButton->setObjectName(QStringLiteral("actionButton"));
    selectButton->setFixedHeight(22);
    if (QPushButton* cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setObjectName(QStringLiteral("actionButton"));
        cancelButton->setFixedHeight(22);
    }
    auto updateFilterSummary = [model, table, selectButton, countLabel]() {
        countLabel->setText(QStringLiteral("%1 / %2")
                                .arg(static_cast<qulonglong>(model->rowCount()))
                                .arg(static_cast<qulonglong>(model->totalGuessCount())));
        const bool hasSelection = table->selectionModel()
            && table->selectionModel()->hasSelection();
        selectButton->setEnabled(model->rowCount() > 0 && hasSelection);
    };
    QObject::connect(searchEdit, &QLineEdit::textChanged, model, &ConfigurationGuessTableModel::setSearchText);
    QObject::connect(model, &QAbstractItemModel::modelReset, &dialog, [model, table, updateFilterSummary]() {
        if (model->rowCount() > 0) {
            table->selectRow(0);
        }
        updateFilterSummary();
    });
    QObject::connect(table->selectionModel(),
                     &QItemSelectionModel::selectionChanged,
                     &dialog,
                     [updateFilterSummary](const QItemSelection&, const QItemSelection&) {
                         updateFilterSummary();
                     });
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(table, &QTableView::doubleClicked, &dialog, [&dialog](const QModelIndex& index) {
        if (index.isValid()) {
            dialog.accept();
        }
    });
    layout->addWidget(buttonBox);

    if (!result.guesses.empty()) {
        table->selectRow(0);
    }
    updateFilterSummary();

    const QScreen* screen = parent && parent->screen()
        ? parent->screen()
        : QGuiApplication::primaryScreen();
    const QRect availableGeometry = screen
        ? screen->availableGeometry()
        : QRect(0, 0, 1400, 900);
    dialog.resize(qMin(qMax(dialog.width(), 920), qMax(920, availableGeometry.width() - 120)),
                  qMin(qMax(dialog.height(), 520), qMax(520, availableGeometry.height() - 120)));

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const QModelIndex currentIndex = table->selectionModel()
        ? table->selectionModel()->currentIndex()
        : QModelIndex();
    const int selectedRow = currentIndex.isValid() ? currentIndex.row() : 0;
    const CLogBTraceConfigurationGuess* selectedGuess = model->guessAtRow(selectedRow);
    if (!selectedGuess) {
        return std::nullopt;
    }

    return selectedGuess->parameters;
}

inline std::optional<CLog::Parameters> showTraceLoadErrorDialog(QWidget* parent,
                                                         const QString& sourceName,
                                                         const QString& filePath,
                                                         const CLogBTraceLoadError& error)
{
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    std::optional<CLog::Parameters> selectedParameters;

    if (error.type == CLogBTraceLoadError::Type::UnsupportedParameters
        && !error.parameterDifferences.empty()) {
        QDialog dialog(parent);
        dialog.setWindowTitle(QStringLiteral("Open CLog.B"));

        QVBoxLayout* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(10);

        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(10);

        QLabel* iconLabel = new QLabel(&dialog);
        const QIcon errorIcon = (parent ? parent->style() : QApplication::style())
                                    ->standardIcon(QStyle::SP_MessageBoxCritical);
        iconLabel->setPixmap(errorIcon.pixmap(32, 32));
        iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

        QVBoxLayout* titleLayout = new QVBoxLayout();
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(3);

        QLabel* titleLabel = new QLabel(QStringLiteral("Load Failed: CHI Parameter Mismatch"), &dialog);
        titleLabel->setWordWrap(true);
        titleLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:700; color:#9B1C1C;"));
        titleLayout->addWidget(titleLabel);

        QLabel* summaryLabel = new QLabel(
            error.summary.isEmpty()
                ? QStringLiteral("The CLog.B trace cannot be decoded by this CloganGUI build.")
                : error.summary,
            &dialog);
        summaryLabel->setWordWrap(true);
        summaryLabel->setStyleSheet(QStringLiteral("color:#3F3F46;"));
        titleLayout->addWidget(summaryLabel);

        headerLayout->addLayout(titleLayout, 1);
        layout->addLayout(headerLayout);

        QFrame* alertFrame = new QFrame(&dialog);
        alertFrame->setFrameShape(QFrame::StyledPanel);
        alertFrame->setStyleSheet(
            QStringLiteral("QFrame { background:#FEF2F2; border:1px solid #F5C2C7; border-radius:4px; }"));
        QVBoxLayout* alertLayout = new QVBoxLayout(alertFrame);
        alertLayout->setContentsMargins(10, 8, 10, 8);
        alertLayout->setSpacing(6);

        if (!sourceName.isEmpty()) {
            QLabel* fileLabel = new QLabel(QStringLiteral("File: %1").arg(sourceName), alertFrame);
            fileLabel->setStyleSheet(QStringLiteral("color:#7F1D1D; font-weight:600;"));
            fileLabel->setWordWrap(true);
            alertLayout->addWidget(fileLabel);
        }

        if (!error.informativeText.isEmpty()) {
            QLabel* noteLabel = new QLabel(error.informativeText, alertFrame);
            noteLabel->setStyleSheet(QStringLiteral("color:#7F1D1D;"));
            noteLabel->setWordWrap(true);
            alertLayout->addWidget(noteLabel);
        }

        layout->addWidget(alertFrame);

        QLabel* tableCaption = new QLabel(QStringLiteral("Mismatched Parameters"), &dialog);
        tableCaption->setStyleSheet(QStringLiteral("font-weight:700; color:#18212B;"));
        layout->addWidget(tableCaption);

        QTableWidget* table = new QTableWidget(static_cast<int>(error.parameterDifferences.size()), 3, &dialog);
        table->setHorizontalHeaderLabels({QStringLiteral("Parameter"),
                                          QStringLiteral("Trace File"),
                                          QStringLiteral("Expected By App")});
        table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setAlternatingRowColors(false);
        table->setShowGrid(false);
        table->setWordWrap(false);
        table->verticalHeader()->hide();
        table->horizontalHeader()->setStretchLastSection(false);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->horizontalHeader()->setHighlightSections(false);
        table->setStyleSheet(
            QStringLiteral("QTableWidget { border:1px solid #D1D5DB; border-radius:4px; background:#FFFFFF; }"
                           "QHeaderView::section { background:#F3F4F6; color:#374151; padding:6px; border:0; border-bottom:1px solid #D1D5DB; font-weight:700; }"));

        for (int row = 0; row < static_cast<int>(error.parameterDifferences.size()); ++row) {
            const CLogBTraceParameterComparison& difference = error.parameterDifferences[static_cast<std::size_t>(row)];

            QTableWidgetItem* nameItem = new QTableWidgetItem(difference.name);
            QFont nameFont = nameItem->font();
            nameFont.setBold(true);
            nameItem->setFont(nameFont);
            nameItem->setForeground(QColor(QStringLiteral("#7F1D1D")));
            nameItem->setBackground(QColor(QStringLiteral("#FFF7F7")));
            table->setItem(row, 0, nameItem);

            QTableWidgetItem* traceItem = new QTableWidgetItem(difference.traceValue);
            traceItem->setBackground(QColor(QStringLiteral("#FEE2E2")));
            traceItem->setForeground(QColor(QStringLiteral("#991B1B")));
            table->setItem(row, 1, traceItem);

            QTableWidgetItem* appItem = new QTableWidgetItem(difference.applicationValue);
            appItem->setBackground(QColor(QStringLiteral("#F9FAFB")));
            appItem->setForeground(QColor(QStringLiteral("#111827")));
            table->setItem(row, 2, appItem);

            table->setRowHeight(row, 28);
        }

        table->resizeColumnsToContents();
        table->resizeRowsToContents();
        layout->addWidget(table, 1);

        QTextBrowser* detailsView = nullptr;
        if (!error.detailedText.isEmpty()) {
            QToolButton* detailsToggle = new QToolButton(&dialog);
            detailsToggle->setText(QStringLiteral("Show Full Trace Parameters"));
            detailsToggle->setCheckable(true);
            detailsToggle->setChecked(false);
            detailsToggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
            detailsToggle->setArrowType(Qt::RightArrow);
            detailsToggle->setObjectName(QStringLiteral("actionButton"));
            layout->addWidget(detailsToggle, 0, Qt::AlignLeft);

            detailsView = new QTextBrowser(&dialog);
            detailsView->setObjectName(QStringLiteral("detailBrowser"));
            detailsView->setVisible(false);
            detailsView->setMaximumHeight(110);
            detailsView->setFont(fixedFont);
            detailsView->setLineWrapMode(QTextEdit::NoWrap);
            detailsView->setPlainText(error.detailedText);
            layout->addWidget(detailsView);

            QObject::connect(detailsToggle, &QToolButton::toggled, &dialog, [&dialog, detailsToggle, detailsView](const bool checked) {
                detailsToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
                detailsToggle->setText(checked
                                           ? QStringLiteral("Hide Full Trace Parameters")
                                           : QStringLiteral("Show Full Trace Parameters"));
                detailsView->setVisible(checked);
                dialog.adjustSize();
            });
        }

        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
        if (QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok)) {
            okButton->setObjectName(QStringLiteral("actionButton"));
            okButton->setText(QStringLiteral("Close"));
            okButton->setFixedHeight(22);
        }
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        layout->addWidget(buttonBox);

        const QScreen* screen = parent && parent->screen()
            ? parent->screen()
            : QGuiApplication::primaryScreen();
        const QRect availableGeometry = screen
            ? screen->availableGeometry()
            : QRect(0, 0, 1400, 900);
        const int maxDialogWidth = qMax(760, availableGeometry.width() - 120);
        const int maxDialogHeight = qMax(420, availableGeometry.height() - 120);

        int desiredTableWidth = table->frameWidth() * 2 + 6;
        for (int column = 0; column < table->columnCount(); ++column) {
            desiredTableWidth += table->columnWidth(column);
        }
        desiredTableWidth += table->verticalScrollBar()->sizeHint().width();

        int desiredTableHeight = table->frameWidth() * 2 + table->horizontalHeader()->height() + 6;
        for (int row = 0; row < table->rowCount(); ++row) {
            desiredTableHeight += table->rowHeight(row);
        }

        table->setMinimumWidth(qMin(desiredTableWidth, maxDialogWidth - 48));
        table->setMinimumHeight(qMin(desiredTableHeight, maxDialogHeight - 240));

        layout->activate();
        dialog.adjustSize();
        dialog.resize(qMin(qMax(dialog.width(), 760), maxDialogWidth),
                      qMin(qMax(dialog.height(), 420), maxDialogHeight));

        dialog.exec();
        return std::nullopt;
    }

    QString informativeText = error.informativeText;
    if (!sourceName.isEmpty()) {
        informativeText = informativeText.isEmpty()
            ? QStringLiteral("File: %1").arg(sourceName)
            : QStringLiteral("File: %1\n\n%2").arg(sourceName, informativeText);
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Open CLog.B"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    QLabel* iconLabel = new QLabel(&dialog);
    const QIcon errorIcon = (parent ? parent->style() : QApplication::style())
                                ->standardIcon(QStyle::SP_MessageBoxCritical);
    iconLabel->setPixmap(errorIcon.pixmap(32, 32));
    iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(3);

    QLabel* titleLabel = new QLabel(QStringLiteral("Load Failed"), &dialog);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:700; color:#9B1C1C;"));
    titleLayout->addWidget(titleLabel);

    QLabel* summaryLabel = new QLabel(
        error.summary.isEmpty()
            ? QStringLiteral("The CLog.B trace could not be loaded.")
            : error.summary,
        &dialog);
    summaryLabel->setWordWrap(true);
    summaryLabel->setStyleSheet(QStringLiteral("color:#3F3F46;"));
    titleLayout->addWidget(summaryLabel);

    headerLayout->addLayout(titleLayout, 1);
    layout->addLayout(headerLayout);

    if (!informativeText.isEmpty()) {
        QFrame* alertFrame = new QFrame(&dialog);
        alertFrame->setFrameShape(QFrame::StyledPanel);
        alertFrame->setStyleSheet(
            QStringLiteral("QFrame { background:#FEF2F2; border:1px solid #F5C2C7; border-radius:4px; }"));

        QVBoxLayout* alertLayout = new QVBoxLayout(alertFrame);
        alertLayout->setContentsMargins(10, 8, 10, 8);
        alertLayout->setSpacing(6);

        QLabel* noteLabel = new QLabel(informativeText, alertFrame);
        noteLabel->setStyleSheet(QStringLiteral("color:#7F1D1D;"));
        noteLabel->setWordWrap(true);
        alertLayout->addWidget(noteLabel);

        layout->addWidget(alertFrame);
    }

    if (!error.flitReportText.isEmpty()) {
        QLabel* reportCaption = new QLabel(QStringLiteral("Current CHI Flit Report"), &dialog);
        reportCaption->setStyleSheet(QStringLiteral("font-weight:700; color:#18212B;"));
        layout->addWidget(reportCaption);

        QTextBrowser* reportView = new QTextBrowser(&dialog);
        reportView->setObjectName(QStringLiteral("detailBrowser"));
        reportView->setMinimumWidth(700);
        reportView->setMinimumHeight(160);
        reportView->setMaximumHeight(240);
        reportView->setFont(fixedFont);
        reportView->setLineWrapMode(QTextEdit::NoWrap);
        reportView->setPlainText(error.flitReportText);
        layout->addWidget(reportView, 1);
    }

    if (!error.detailedText.isEmpty()) {
        QLabel* detailsCaption = new QLabel(QStringLiteral("Debug Details"), &dialog);
        detailsCaption->setStyleSheet(QStringLiteral("font-weight:700; color:#18212B;"));
        layout->addWidget(detailsCaption);

        QToolButton* detailsToggle = new QToolButton(&dialog);
        detailsToggle->setText(QStringLiteral("Show Debug Details"));
        detailsToggle->setCheckable(true);
        detailsToggle->setChecked(false);
        detailsToggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
        detailsToggle->setArrowType(Qt::RightArrow);
        detailsToggle->setObjectName(QStringLiteral("actionButton"));
        layout->addWidget(detailsToggle, 0, Qt::AlignLeft);

        QTextBrowser* detailsView = new QTextBrowser(&dialog);
        detailsView->setObjectName(QStringLiteral("detailBrowser"));
        detailsView->setVisible(false);
        detailsView->setMinimumWidth(700);
        detailsView->setMinimumHeight(220);
        detailsView->setMaximumHeight(360);
        detailsView->setFont(fixedFont);
        detailsView->setLineWrapMode(QTextEdit::NoWrap);
        detailsView->setPlainText(error.detailedText);
        layout->addWidget(detailsView, 1);

        QObject::connect(detailsToggle, &QToolButton::toggled, &dialog, [&dialog, detailsToggle, detailsView](const bool checked) {
            detailsToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
            detailsToggle->setText(checked
                                       ? QStringLiteral("Hide Debug Details")
                                       : QStringLiteral("Show Debug Details"));
            detailsView->setVisible(checked);
            dialog.adjustSize();
        });
    }

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QPushButton* guessButton = nullptr;
    if (error.type == CLogBTraceLoadError::Type::FlitWidthMismatch
        && !filePath.isEmpty()) {
        guessButton = buttonBox->addButton(QStringLiteral("Guess Configuration"),
                                           QDialogButtonBox::ActionRole);
        guessButton->setObjectName(QStringLiteral("actionButton"));
        guessButton->setFixedHeight(22);
    }
    if (QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setObjectName(QStringLiteral("actionButton"));
        okButton->setText(QStringLiteral("Close"));
        okButton->setFixedHeight(22);
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    if (guessButton) {
        QObject::connect(guessButton, &QPushButton::clicked, &dialog, [&]() {
            CLogBTraceConfigurationGuessResult guessResult;
            CLogBTraceLoadError guessError;
            bool guessed = false;
            {
                WaitCursorGuard cursorGuard;
                guessed = CLogBTraceLoader::guessFlitConfigurations(filePath,
                                                                     guessResult,
                                                                     guessError);
            }

            if (!guessed) {
                const QString message = guessError.summary.isEmpty()
                    ? QStringLiteral("CloganGUI could not guess CHI parameter combinations for this trace.")
                    : (guessError.informativeText.isEmpty()
                           ? guessError.summary
                           : guessError.summary + QStringLiteral("\n\n") + guessError.informativeText);
                QMessageBox::warning(&dialog,
                                     QStringLiteral("Guess CHI Configuration"),
                                     message);
                return;
            }

            const std::optional<CLog::Parameters> selected =
                showConfigurationGuessDialog(&dialog, sourceName, guessResult);
            if (!selected.has_value()) {
                return;
            }

            selectedParameters = *selected;
            dialog.accept();
        });
    }
    layout->addWidget(buttonBox);

    const QScreen* screen = parent && parent->screen()
        ? parent->screen()
        : QGuiApplication::primaryScreen();
    const QRect availableGeometry = screen
        ? screen->availableGeometry()
        : QRect(0, 0, 1400, 900);
    const int maxDialogWidth = qMax(760, availableGeometry.width() - 120);
    const int maxDialogHeight = qMax(360, availableGeometry.height() - 120);

    layout->activate();
    dialog.adjustSize();
    dialog.resize(qMin(qMax(dialog.width(), 760), maxDialogWidth),
                  qMin(qMax(dialog.height(), 260), maxDialogHeight));
    dialog.exec();
    return selectedParameters;
}

class ChannelBadgeDelegate final : public QStyledItemDelegate {
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
        const bool transactionHighlighted =
            index.data(FlitTableModel::TransactionHighlightRole).toBool();

        const QString tag = index.data(FlitTableModel::ChannelTagRole).toString();
        QFont badgeFont(option.font);
        const qreal fontScale = index.data(FlitTableModel::BadgeFontScaleRole).toReal();
        badgeFont = ChannelBadgeScaledFont(badgeFont, fontScale);
        const QRect availableRect = ChannelBadgeContentRect(option.rect);
        const bool leftAligned = index.data(FlitTableModel::BadgeLeftAlignedRole).toBool();
        PaintChannelBadge(painter,
                          availableRect,
                          badgeFont,
                          text,
                          tag,
                          accent,
                          option.state.testFlag(QStyle::State_Selected),
                          transactionHighlighted,
                          leftAligned);
    }
};

class XactionIndexStateDelegate final : public QStyledItemDelegate {
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

        const auto state = static_cast<XactionIndexState>(
            index.data(FlitTableModel::XactionIndexStateRole).toInt());
        QColor fill(QStringLiteral("#AEB4BE"));
        switch (state) {
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

class NodeLabelDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static constexpr int kMinimumColumnWidth = 168;
    static constexpr int kValueAreaWidth = 52;
    static constexpr int kLabelGap = 6;

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
        return QSize(std::max({base.width(),
                               kValueAreaWidth + kLabelGap + labelWidth + 12,
                               kMinimumColumnWidth}),
                     base.height());
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
        const int valueAreaWidth =
            qMin(kValueAreaWidth, qMax(0, contentRect.width() - kLabelGap - labelWidth));
        const int labelLeft = contentRect.left() + valueAreaWidth + kLabelGap;

        QRect textRect(contentRect.left(),
                       contentRect.top(),
                       qMax(0, valueAreaWidth),
                       contentRect.height());
        QRect tagRect;
        const int remainingWidth = qMax(0, contentRect.right() - labelLeft + 1);
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
            const QColor labelColor = index.data(FlitTableModel::NodeLabelColorRole).value<QColor>();
            painter->setBrush(labelColor.isValid() ? labelColor : QColor(QStringLiteral("#8B949E")));
            painter->drawRoundedRect(tagRect, 6.0, 6.0);
            painter->setPen(label == QLatin1String("Before SAM")
                                ? QColor(QStringLiteral("#3F4752"))
                                : QColor(QStringLiteral("#FFFFFF")));
            painter->drawText(tagRect, Qt::AlignCenter, label);
        }

        painter->restore();
    }
};

}  // namespace CHIron::Gui::MainWindowDetail

