#include "bug_reporter.hpp"
#include "main_window.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include <QStyleFactory>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
QString executableDirectory()
{
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return QString();
    }

    buffer.resize(length);
    return QFileInfo(QString::fromStdWString(buffer)).absolutePath();
}

void ConfigureWindowsQtRuntime()
{
    const QString appDir = executableDirectory();
    if (appDir.isEmpty()) {
        return;
    }

    const QString platformsDir = QDir(appDir).filePath(QStringLiteral("platforms"));
    const bool hasWindowsPlatformPlugin =
        QFileInfo::exists(QDir(platformsDir).filePath(QStringLiteral("qwindows.dll")))
        || QFileInfo::exists(QDir(platformsDir).filePath(QStringLiteral("qwindowsd.dll")));

    if (hasWindowsPlatformPlugin) {
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", QDir::toNativeSeparators(platformsDir).toLocal8Bit());
    }

    const QByteArray requestedPlatform = qgetenv("QT_QPA_PLATFORM");
    if (requestedPlatform.isEmpty() || requestedPlatform == "offscreen") {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("windows"));
    }

    QCoreApplication::setLibraryPaths(QStringList{appDir});
}
#endif

void ApplyWindowsStyle(QApplication& app)
{
#ifdef Q_OS_WIN
    const QStringList styles = QStyleFactory::keys();

    if (styles.contains(QStringLiteral("windows11"), Qt::CaseInsensitive)) {
        app.setStyle(QStringLiteral("windows11"));
    } else if (styles.contains(QStringLiteral("windowsvista"), Qt::CaseInsensitive)) {
        app.setStyle(QStringLiteral("windowsvista"));
    } else if (styles.contains(QStringLiteral("Windows"), Qt::CaseInsensitive)) {
        app.setStyle(QStringLiteral("Windows"));
    }
#else
    Q_UNUSED(app)
#endif
}

QString ViewerStyleSheet()
{
    return QStringLiteral(
        "QLabel#sectionLabel {"
        "  font-size: 8pt;"
        "  font-weight: 700;"
        "  color: #52606C;"
        "  padding-right: 4px;"
        "}"
        "QLabel#secondaryLabel {"
        "  color: #6A7782;"
        "  font-size: 8pt;"
        "}"
        "QLabel#groupCaption {"
        "  color: #4F5D68;"
        "  font-size: 8pt;"
        "  font-weight: 600;"
        "  padding-right: 4px;"
        "}"
        "QLabel#metricBadge {"
        "  padding: 2px 6px;"
        "  min-height: 18px;"
        "  color: #17232C;"
        "  background: #FFFFFF;"
        "  border: 1px solid #CCD5DD;"
        "  border-radius: 3px;"
        "  font-weight: 600;"
        "}"
        "QFrame#traceToolbar {"
        "  background: #EEF2F5;"
        "  border-top: 1px solid #FFFFFF;"
        "  border-bottom: 1px solid #C8D0D8;"
        "}"
        "QFrame#traceToolbarGroup {"
        "  background: #F9FBFC;"
        "  border: 1px solid #CDD5DC;"
        "  border-radius: 4px;"
        "}"
        "QFrame#traceToolbar QLineEdit {"
        "  min-height: 20px;"
        "  padding: 0 6px;"
        "  color: #18212B;"
        "  background: #FFFFFF;"
        "  border: 1px solid #C6CFD8;"
        "  border-radius: 0px;"
        "  selection-background-color: #AFC7DB;"
        "}"
        "QFrame#traceToolbar QLineEdit:focus {"
        "  border-color: #7D9CB7;"
        "  background: #FCFDFF;"
        "}"
        "QToolButton#filterToggle {"
        "  min-width: 34px;"
        "  min-height: 20px;"
        "  padding: 0 7px;"
        "  color: #2A3944;"
        "  background: #F3F6F8;"
        "  border: 1px solid #C8D0D8;"
        "  border-radius: 0px;"
        "  font-weight: 700;"
        "}"
        "QToolButton#filterToggle:hover {"
        "  background: #E9EEF2;"
        "}"
        "QToolButton#filterToggle:checked {"
        "  color: #17232C;"
        "  background: #D7E4EE;"
        "  border-color: #8AA4BA;"
        "}"
        "QToolButton#traceToolbarFoldButton {"
        "  min-width: 20px;"
        "  max-width: 22px;"
        "  min-height: 20px;"
        "  max-height: 22px;"
        "  padding: 0px;"
        "  color: #2A3944;"
        "  background: #F3F6F8;"
        "  border: 1px solid #C8D0D8;"
        "  border-radius: 0px;"
        "}"
        "QToolButton#traceToolbarFoldButton:hover {"
        "  background: #E9EEF2;"
        "}"
        "QToolButton#traceToolbarFoldButton:checked {"
        "  color: #17232C;"
        "  background: #D7E4EE;"
        "  border-color: #8AA4BA;"
        "}"
        "QPushButton#actionButton {"
        "  min-height: 20px;"
        "  padding: 0 9px;"
        "  color: #22303A;"
        "  background: #FFFFFF;"
        "  border: 1px solid #C4CDD6;"
        "  border-radius: 0px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#actionButton:hover {"
        "  background: #F5F8FA;"
        "}"
        "QToolTip {"
        "  color: #18212B;"
        "  background: #F8FBFD;"
        "  border: 1px solid #AEB9C2;"
        "  padding: 4px 6px;"
        "}"
        "QTableView {"
        "  selection-background-color: #365F87;"
        "  selection-color: #FFFFFF;"
        "}"
        "QTableView::item:selected {"
        "  color: #FFFFFF;"
        "}"
        "ads--CDockWidget {"
        "  border-style: solid;"
        "  border-color: #EEF2F5;"
        "  border-width: 1px 0 0 0;"
        "}"
        "ads--CDockSplitter::handle, QSplitter::handle {"
        "  background: transparent;"
        "}"
        "ads--CDockSplitter::handle:horizontal, QSplitter::handle:horizontal {"
        "  width: 5px;"
        "  border-left: 1px solid #E7ECF1;"
        "}"
        "ads--CDockSplitter::handle:vertical, QSplitter::handle:vertical {"
        "  height: 5px;"
        "  border-top: 1px solid #E7ECF1;"
        "}"
        "ads--CDockSplitter::handle:hover, QSplitter::handle:hover {"
        "  background: #F6F9FB;"
        "}"
        "ads--CDockAreaTitleBar {"
        "  background: #EEF2F5;"
        "  border-top: 1px solid #FFFFFF;"
        "  border-bottom: 1px solid #C4CED7;"
        "  padding-bottom: 0px;"
        "}"
        "ads--CDockAreaWidget[focused=\"true\"] ads--CDockAreaTitleBar {"
        "  border-bottom: 1px solid #86A6BF;"
        "}"
        "ads--CDockWidgetTab {"
        "  color: #40515F;"
        "  background: #E5EBF0;"
        "  border-style: solid;"
        "  border-color: #BCC7D0;"
        "  border-width: 1px 1px 0 0;"
        "  padding: 0 8px;"
        "}"
        "ads--CDockWidgetTab QLabel {"
        "  color: #40515F;"
        "  font-weight: 600;"
        "}"
        "ads--CDockWidgetTab:hover {"
        "  background: #EEF3F6;"
        "  border-color: #AAB8C3;"
        "}"
        "ads--CDockWidgetTab[activeTab=\"true\"] {"
        "  background: #FFFFFF;"
        "  border-color: #9DAFBD;"
        "  border-width: 1px 1px 0 0;"
        "}"
        "ads--CDockWidgetTab[activeTab=\"true\"] QLabel {"
        "  color: #17232C;"
        "  font-weight: 700;"
        "}"
        "ads--CDockWidgetTab[focused=\"true\"] {"
        "  background: #E5F0F8;"
        "  border-color: #86A6BF;"
        "}"
        "ads--CDockWidgetTab[focused=\"true\"] QLabel {"
        "  color: #102A43;"
        "}"
        "#tabCloseButton {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  padding: 0px -2px;"
        "}"
        "#tabCloseButton:hover {"
        "  background: #DCE6ED;"
        "  border-color: #AAB8C3;"
        "}"
        "QStatusBar {"
            "  color: #6B7280;"
        "}"
    );
}

}  // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    ConfigureWindowsQtRuntime();
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CloganGUI"));
    app.setOrganizationName(QStringLiteral("CloganGUI"));
    app.setWindowIcon(QIcon(QStringLiteral(":/cyanus.png")));

    CHIron::Gui::BugReporter::initialize();

    ApplyWindowsStyle(app);
    app.setStyleSheet(ViewerStyleSheet());

    CHIron::Gui::MainWindow window;
    window.setWindowIcon(app.windowIcon());
    window.show();

    const QString pendingCrashDirectory =
        CHIron::Gui::BugReporter::takePendingCrashReportDirectory();
    if (!pendingCrashDirectory.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, pendingCrashDirectory]() {
            QMessageBox notice(&window);
            notice.setIcon(QMessageBox::Warning);
            notice.setWindowTitle(QStringLiteral("Previous Crash Detected"));
            notice.setText(QStringLiteral("The previous CloganGUI run ended unexpectedly."));
            notice.setInformativeText(
                QStringLiteral("Crash artifacts were saved to:\n%1")
                    .arg(QDir::toNativeSeparators(pendingCrashDirectory)));
            QPushButton* openFolderButton = notice.addButton(QStringLiteral("Open Folder"),
                                                            QMessageBox::ActionRole);
            notice.addButton(QMessageBox::Close);
            notice.exec();
            if (notice.clickedButton() == openFolderButton) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(pendingCrashDirectory));
            }
        });
    }

    const int exitCode = app.exec();
    CHIron::Gui::BugReporter::shutdown();
    return exitCode;
}
