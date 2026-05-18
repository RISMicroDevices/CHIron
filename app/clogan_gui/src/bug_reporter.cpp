#include "bug_reporter.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

#include <atomic>
#include <cctype>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#endif

namespace CHIron::Gui {

namespace {

struct BugReporterState {
    bool initialized = false;
    QString diagnosticsRootPath;
    QString sessionDirectoryPath;
    QString bugsDirectoryPath;
    QString crashesDirectoryPath;
    QString sessionLogPath;
    QString liveStatePath;
    QString pendingCrashMarkerPath;
    QString sessionStartedAtText;
    QString buildStampText;
    QString appName;
    QString appVersion;
    QString executablePath;
    QString environmentText;
    QtMessageHandler previousMessageHandler = nullptr;
    QFile logFile;
    QMutex logMutex;
    std::wstring crashesDirectoryWidePath;
    std::wstring sessionLogWidePath;
    std::wstring liveStateWidePath;
    std::wstring pendingCrashMarkerWidePath;
    std::string crashEnvironmentText;
    bool dbgHelpInitialized = false;
};

BugReporterState g_bugReporterState;
std::mutex g_crashWriteMutex;
std::atomic_bool g_crashWriteStarted = false;

QString timestampForFileName()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
}

QString diagnosticsRootPathForApp()
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (basePath.isEmpty()) {
        basePath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("diagnostics"));
    }
    return QDir(basePath).filePath(QStringLiteral("diagnostics"));
}

QString formatMessageType(const QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }

    return QStringLiteral("LOG");
}

QString buildEnvironmentText()
{
    const QString appName = QCoreApplication::applicationName().isEmpty()
        ? QStringLiteral("CloganGUI")
        : QCoreApplication::applicationName();
    const QString appVersion = QCoreApplication::applicationVersion().isEmpty()
        ? QStringLiteral("(unset)")
        : QCoreApplication::applicationVersion();
    const QString executablePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString diagnosticsRoot = QDir::toNativeSeparators(g_bugReporterState.diagnosticsRootPath);
    const QString crashesDirectory = QDir::toNativeSeparators(g_bugReporterState.crashesDirectoryPath);
    const QString sessionLogPath = QDir::toNativeSeparators(g_bugReporterState.sessionLogPath);
    const QString liveStatePath = QDir::toNativeSeparators(g_bugReporterState.liveStatePath);

    QString text;
    QTextStream stream(&text);
    stream << "Application: " << appName << '\n';
    stream << "Version: " << appVersion << '\n';
    stream << "Qt: " << QString::fromLatin1(qVersion()) << '\n';
    stream << "Build Stamp: " << QString::fromLatin1(__DATE__ " " __TIME__) << '\n';
    stream << "PID: " << QCoreApplication::applicationPid() << '\n';
    stream << "Executable: " << executablePath << '\n';
    stream << "Started: " << g_bugReporterState.sessionStartedAtText << '\n';
    stream << "Diagnostics Root: " << diagnosticsRoot << '\n';
    stream << "Crash Reports: " << crashesDirectory << '\n';
    stream << "Session Log: " << sessionLogPath << '\n';
    stream << "Live State: " << liveStatePath << '\n';
    stream << "Product: " << QSysInfo::prettyProductName() << '\n';
    stream << "Kernel Type: " << QSysInfo::kernelType() << '\n';
    stream << "Kernel Version: " << QSysInfo::kernelVersion() << '\n';
    stream << "CPU Arch: " << QSysInfo::currentCpuArchitecture() << '\n';
    stream << "Build ABI: " << QSysInfo::buildAbi() << '\n';
    stream << "Machine Host: " << QSysInfo::machineHostName() << '\n';
    return text;
}

bool ensureDirectoryPath(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    QDir directory;
    return directory.mkpath(path);
}

void appendLogLineLocked(const QString& line)
{
    if (!g_bugReporterState.logFile.isOpen()) {
        return;
    }

    const QByteArray utf8 = line.toUtf8();
    g_bugReporterState.logFile.write(utf8);
    g_bugReporterState.logFile.write("\n", 1);
    g_bugReporterState.logFile.flush();
}

void bugReporterMessageHandler(const QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& message)
{
    QString line;
    QTextStream stream(&line);
    stream << '[' << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "] ";
    stream << '[' << formatMessageType(type) << "] ";
    stream << "[tid=" << QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16) << "] ";
    if (context.category && *context.category) {
        stream << '[' << context.category << "] ";
    }
    if (context.file && *context.file) {
        stream << context.file;
        if (context.line > 0) {
            stream << ':' << context.line;
        }
        stream << ' ';
    }
    stream << message;

    {
        QMutexLocker locker(&g_bugReporterState.logMutex);
        appendLogLineLocked(line);
    }

    if (g_bugReporterState.previousMessageHandler) {
        g_bugReporterState.previousMessageHandler(type, context, message);
    } else {
        std::fprintf((type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) ? stderr : stdout,
                     "%s\n",
                     line.toLocal8Bit().constData());
        std::fflush((type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) ? stderr : stdout);
    }

    if (type == QtFatalMsg) {
        std::abort();
    }
}

bool writeTextFileAtomically(const QString& path, const QString& text)
{
    if (path.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(path);
    if (!ensureDirectoryPath(fileInfo.absolutePath())) {
        return false;
    }

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QByteArray utf8 = text.toUtf8();
    if (output.write(utf8) != utf8.size()) {
        output.cancelWriting();
        return false;
    }

    return output.commit();
}

bool copyFileIfExists(const QString& sourcePath, const QString& targetPath)
{
    if (sourcePath.isEmpty() || targetPath.isEmpty()) {
        return false;
    }

    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return false;
    }

    QFile::remove(targetPath);
    return QFile::copy(sourcePath, targetPath);
}

QString readTextFile(const QString& path)
{
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) {
        return QString();
    }

    return QString::fromUtf8(input.readAll());
}

QString buildManualReportText(const BugReportExportRequest& request)
{
    QString text;
    QTextStream stream(&text);
    stream << request.title << '\n';
    stream << QString(80, '=') << '\n';
    stream << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << '\n';
    stream << '\n';
    stream << "Environment" << '\n';
    stream << "-----------" << '\n';
    stream << g_bugReporterState.environmentText;
    stream << '\n';
    stream << "Summary" << '\n';
    stream << "-------" << '\n';
    stream << (request.summary.isEmpty()
                   ? QStringLiteral("Manual diagnostics export triggered from the CloganGUI UI.")
                   : request.summary)
           << '\n';
    stream << '\n';
    stream << "State Snapshot" << '\n';
    stream << "--------------" << '\n';
    stream << (request.stateText.isEmpty() ? QStringLiteral("(no state snapshot available)") : request.stateText);
    if (!request.stateText.endsWith(QChar('\n'))) {
        stream << '\n';
    }
    return text;
}

#ifdef Q_OS_WIN
std::wstring toWide(const QString& value)
{
    return std::wstring(reinterpret_cast<const wchar_t*>(value.utf16()), value.size());
}

constexpr DWORD kSyntheticExceptionCodeTerminate = 0xE0000001UL;
constexpr DWORD kSyntheticExceptionCodeAbort = 0xE0000002UL;
constexpr DWORD kSyntheticExceptionCodeSignal = 0xE0000003UL;
constexpr DWORD kSyntheticExceptionCodeBadAlloc = 0xE0000004UL;

std::wstring timestampForCrashArtifact()
{
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);

    wchar_t buffer[64];
    std::swprintf(buffer,
                  64,
                  L"%04u%02u%02u-%02u%02u%02u-%03u",
                  static_cast<unsigned>(localTime.wYear),
                  static_cast<unsigned>(localTime.wMonth),
                  static_cast<unsigned>(localTime.wDay),
                  static_cast<unsigned>(localTime.wHour),
                  static_cast<unsigned>(localTime.wMinute),
                  static_cast<unsigned>(localTime.wSecond),
                  static_cast<unsigned>(localTime.wMilliseconds));
    return buffer;
}

std::string narrowUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring symbolsListingPath()
{
    return toWide(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("CloganGUI.symbols.txt")));
}

bool writeUtf8File(const std::wstring& path, const std::string& content)
{
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);

    FILE* handle = _wfopen(path.c_str(), L"wb");
    if (!handle) {
        return false;
    }

    const bool ok = std::fwrite(content.data(), 1, content.size(), handle) == content.size();
    std::fclose(handle);
    return ok;
}

bool copyFileWideIfExists(const std::wstring& sourcePath, const std::wstring& targetPath)
{
    if (sourcePath.empty() || targetPath.empty()) {
        return false;
    }

    const DWORD sourceAttributes = GetFileAttributesW(sourcePath.c_str());
    if (sourceAttributes == INVALID_FILE_ATTRIBUTES || (sourceAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }

    DeleteFileW(targetPath.c_str());
    return CopyFileW(sourcePath.c_str(), targetPath.c_str(), FALSE) != FALSE;
}

std::string currentExceptionSummary()
{
    try {
        const std::exception_ptr exception = std::current_exception();
        if (!exception) {
            return "No active exception";
        }

        std::rethrow_exception(exception);
    } catch (const std::exception& e) {
        return std::string("std::exception: ") + e.what();
    } catch (...) {
        return "Non-standard exception";
    }
}

bool isCodeLikeSymbolType(const char symbolType) noexcept
{
    switch (symbolType) {
    case 't':
    case 'T':
    case 'w':
    case 'W':
    case 'i':
    case 'I':
        return true;
    default:
        return false;
    }
}

std::string lookupFallbackSymbolName(const DWORD64 address)
{
    const std::wstring symbolsPath = symbolsListingPath();
    FILE* handle = _wfopen(symbolsPath.c_str(), L"rb");
    if (!handle) {
        return {};
    }

    char line[8192];
    unsigned long long bestCodeAddress = 0;
    unsigned long long bestAnyAddress = 0;
    std::string bestCodeSymbol;
    std::string bestAnySymbol;

    while (std::fgets(line, static_cast<int>(sizeof(line)), handle)) {
        char* cursor = line;
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }

        if (!std::isxdigit(static_cast<unsigned char>(*cursor))) {
            continue;
        }

        char* endAddress = nullptr;
        const unsigned long long symbolAddress = std::strtoull(cursor, &endAddress, 16);
        if (endAddress == cursor) {
            continue;
        }
        if (symbolAddress > address) {
            break;
        }

        while (*endAddress == ' ' || *endAddress == '\t') {
            ++endAddress;
        }
        if (*endAddress == '\0' || *endAddress == '\n' || *endAddress == '\r') {
            continue;
        }

        const char symbolType = *endAddress++;
        while (*endAddress == ' ' || *endAddress == '\t') {
            ++endAddress;
        }
        if (*endAddress == '\0' || *endAddress == '\n' || *endAddress == '\r') {
            continue;
        }

        char* endSymbol = endAddress + std::strlen(endAddress);
        while (endSymbol > endAddress && (endSymbol[-1] == '\n' || endSymbol[-1] == '\r')) {
            --endSymbol;
        }

        const std::string symbolName(endAddress, static_cast<std::size_t>(endSymbol - endAddress));
        if (symbolName.empty()) {
            continue;
        }

        bestAnyAddress = symbolAddress;
        bestAnySymbol = symbolName;
        if (isCodeLikeSymbolType(symbolType)) {
            bestCodeAddress = symbolAddress;
            bestCodeSymbol = symbolName;
        }
    }

    std::fclose(handle);

    if (!bestCodeSymbol.empty()) {
        std::string result = bestCodeSymbol;
        if (address > bestCodeAddress) {
            char displacementBuffer[32];
            std::snprintf(displacementBuffer,
                          sizeof(displacementBuffer),
                          "+0x%llX",
                          static_cast<unsigned long long>(address - bestCodeAddress));
            result += displacementBuffer;
        }
        return result;
    }

    if (!bestAnySymbol.empty()) {
        std::string result = bestAnySymbol;
        if (address > bestAnyAddress) {
            char displacementBuffer[32];
            std::snprintf(displacementBuffer,
                          sizeof(displacementBuffer),
                          "+0x%llX",
                          static_cast<unsigned long long>(address - bestAnyAddress));
            result += displacementBuffer;
        }
        return result;
    }

    return {};
}

void* instructionPointerFromContext(const CONTEXT& context) noexcept
{
#if defined(_M_X64) || defined(__x86_64__)
    return reinterpret_cast<void*>(context.Rip);
#elif defined(_M_IX86) || defined(__i386__)
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(context.Eip));
#elif defined(_M_ARM64) || defined(__aarch64__)
    return reinterpret_cast<void*>(context.Pc);
#elif defined(_M_ARM)
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(context.Pc));
#else
    return nullptr;
#endif
}

bool ensureDbgHelpInitialized() noexcept
{
    if (g_bugReporterState.dbgHelpInitialized) {
        return true;
    }

    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SymGetOptions() | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    g_bugReporterState.dbgHelpInitialized = SymInitialize(process, nullptr, TRUE) != FALSE;
    return g_bugReporterState.dbgHelpInitialized;
}

void appendStackBacktrace(std::string& report, EXCEPTION_POINTERS* exceptionPointers)
{
    report += "\nStack Backtrace\n---------------\n";

    if (!exceptionPointers || !exceptionPointers->ContextRecord) {
        report += "(no exception context available)\n";
        return;
    }

    if (!ensureDbgHelpInitialized()) {
        report += "(DbgHelp symbol handler initialization failed)\n";
        return;
    }

    const HANDLE process = GetCurrentProcess();
    const HANDLE thread = GetCurrentThread();
    CONTEXT context = *exceptionPointers->ContextRecord;
    STACKFRAME64 frame{};
    DWORD machineType = 0;

#if defined(_M_X64) || defined(__x86_64__)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rsp;
    frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86) || defined(__i386__)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#elif defined(_M_ARM64) || defined(__aarch64__)
    machineType = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context.Pc;
    frame.AddrFrame.Offset = context.Fp;
    frame.AddrStack.Offset = context.Sp;
#else
    report += "(stack backtrace is not implemented for this CPU architecture)\n";
    return;
#endif

    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    constexpr unsigned int kMaxFrames = 64;
    for (unsigned int frameIndex = 0; frameIndex < kMaxFrames; ++frameIndex) {
        const BOOL walked = StackWalk64(machineType,
                                        process,
                                        thread,
                                        &frame,
                                        &context,
                                        nullptr,
                                        SymFunctionTableAccess64,
                                        SymGetModuleBase64,
                                        nullptr);
        if (!walked || frame.AddrPC.Offset == 0) {
            if (frameIndex == 0) {
                report += "(unable to unwind stack frames)\n";
            }
            break;
        }

        const DWORD64 address = frame.AddrPC.Offset;
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 symbolDisplacement = 0;
        const BOOL haveSymbol = SymFromAddr(process, address, &symbolDisplacement, symbol);

        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(lineInfo);
        DWORD lineDisplacement = 0;
        const BOOL haveLine = SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo);

        IMAGEHLP_MODULE64 moduleInfo{};
        moduleInfo.SizeOfStruct = sizeof(moduleInfo);
        const BOOL haveModule = SymGetModuleInfo64(process, address, &moduleInfo);

        char addressBuffer[32];
        std::snprintf(addressBuffer, sizeof(addressBuffer), "0x%p", reinterpret_cast<void*>(address));
        report += '#';
        if (frameIndex < 10) {
            report += '0';
        }
        report += std::to_string(frameIndex);
        report += ' ';
        report += addressBuffer;

        if (haveModule && moduleInfo.ModuleName[0] != '\0') {
            report += ' ';
            report += moduleInfo.ModuleName;
            report += '!';
        } else {
            report += ' ';
        }

        if (haveSymbol) {
            report += symbol->Name;
            if (symbolDisplacement != 0) {
                char displacementBuffer[32];
                std::snprintf(displacementBuffer,
                              sizeof(displacementBuffer),
                              "+0x%llX",
                              static_cast<unsigned long long>(symbolDisplacement));
                report += displacementBuffer;
            }
        } else {
            const std::string fallbackSymbol = lookupFallbackSymbolName(address);
            if (!fallbackSymbol.empty()) {
                report += fallbackSymbol;
                report += " [symbols.txt]";
            } else {
                report += "(symbol unavailable)";
            }
        }

        if (haveLine && lineInfo.FileName) {
            report += " (";
            report += lineInfo.FileName;
            report += ':';
            report += std::to_string(lineInfo.LineNumber);
            report += ')';
        }

        report += '\n';
    }
}

void writeCrashArtifacts(const char* reason,
                         EXCEPTION_POINTERS* exceptionPointers,
                         const std::string& detailText)
{
    if (!reason || g_bugReporterState.crashesDirectoryWidePath.empty()) {
        return;
    }

    bool expected = false;
    if (!g_crashWriteStarted.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    std::lock_guard<std::mutex> guard(g_crashWriteMutex);

    const DWORD processId = GetCurrentProcessId();
    const std::wstring crashDirectory =
        g_bugReporterState.crashesDirectoryWidePath
        + L"\\crash-" + timestampForCrashArtifact()
        + L"-pid" + std::to_wstring(static_cast<unsigned long long>(processId));
    std::error_code directoryError;
    std::filesystem::create_directories(crashDirectory, directoryError);

    const std::wstring reportPath = crashDirectory + L"\\crash.txt";
    const std::wstring dumpPath = crashDirectory + L"\\crash.dmp";
    const std::wstring copiedLogPath = crashDirectory + L"\\session.log";
    const std::wstring copiedStatePath = crashDirectory + L"\\current_state.txt";

    std::string report;
    report += std::string("Crash Report\n");
    report += std::string(80, '=') + "\n";
    report += "Reason: ";
    report += reason;
    report += "\n";
    report += "PID: ";
    report += std::to_string(static_cast<unsigned long long>(processId));
    report += "\n";
    report += "Crash Directory: ";
    report += narrowUtf8(crashDirectory);
    report += "\n";
    if (exceptionPointers && exceptionPointers->ExceptionRecord) {
        char buffer[128];
        std::snprintf(buffer,
                      sizeof(buffer),
                      "0x%08lX",
                      static_cast<unsigned long>(exceptionPointers->ExceptionRecord->ExceptionCode));
        report += "Exception Code: ";
        report += buffer;
        report += "\n";
        std::snprintf(buffer,
                      sizeof(buffer),
                      "0x%p",
                      exceptionPointers->ExceptionRecord->ExceptionAddress);
        report += "Exception Address: ";
        report += buffer;
        report += "\n";
    }
    if (!detailText.empty()) {
        report += "Details: ";
        report += detailText;
        report += "\n";
    }

    report += "\nCrash Reporter\n--------------\n";
    report += "Emergency crash report checkpoint reached before stack unwinding and dump capture.\n";
    report += "If this file lacks a Stack Backtrace section, crash handling failed while collecting extended diagnostics.\n";
    report += "\nArtifacts\n---------\n";
    report += "Crash Directory: ";
    report += narrowUtf8(crashDirectory);
    report += "\n";
    report += "Crash Dump: ";
    report += narrowUtf8(dumpPath);
    report += "\n";
    report += "Session Log: ";
    report += narrowUtf8(copiedLogPath);
    report += "\n";
    report += "Live State: ";
    report += narrowUtf8(copiedStatePath);
    report += "\n";

    writeUtf8File(reportPath, report);
    writeUtf8File(g_bugReporterState.pendingCrashMarkerWidePath, narrowUtf8(crashDirectory));
    copyFileWideIfExists(g_bugReporterState.sessionLogWidePath, copiedLogPath);
    copyFileWideIfExists(g_bugReporterState.liveStateWidePath, copiedStatePath);

    appendStackBacktrace(report, exceptionPointers);
    report += "\nEnvironment\n-----------\n";
    report += g_bugReporterState.crashEnvironmentText;
    if (!g_bugReporterState.crashEnvironmentText.empty()
        && g_bugReporterState.crashEnvironmentText.back() != '\n') {
        report += '\n';
    }

    writeUtf8File(reportPath, report);

    if (exceptionPointers) {
        HANDLE dumpFile = CreateFileW(dumpPath.c_str(),
                                      GENERIC_WRITE,
                                      0,
                                      nullptr,
                                      CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL,
                                      nullptr);
        if (dumpFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
            exceptionInfo.ThreadId = GetCurrentThreadId();
            exceptionInfo.ExceptionPointers = exceptionPointers;
            exceptionInfo.ClientPointers = FALSE;

            MiniDumpWriteDump(GetCurrentProcess(),
                              processId,
                              dumpFile,
                              static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs
                                  | MiniDumpWithHandleData
                                  | MiniDumpWithThreadInfo
                                  | MiniDumpWithIndirectlyReferencedMemory),
                              &exceptionInfo,
                              nullptr,
                              nullptr);
            CloseHandle(dumpFile);
        }
    }

    copyFileWideIfExists(g_bugReporterState.sessionLogWidePath, copiedLogPath);
    copyFileWideIfExists(g_bugReporterState.liveStateWidePath, copiedStatePath);
    writeUtf8File(g_bugReporterState.pendingCrashMarkerWidePath, narrowUtf8(crashDirectory));
}

void writeSyntheticCrashArtifacts(const char* reason,
                                  const DWORD exceptionCode,
                                  const std::string& detailText)
{
    CONTEXT context{};
    RtlCaptureContext(&context);

    EXCEPTION_RECORD exceptionRecord{};
    exceptionRecord.ExceptionCode = exceptionCode;
    exceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    exceptionRecord.ExceptionAddress = instructionPointerFromContext(context);

    EXCEPTION_POINTERS exceptionPointers{};
    exceptionPointers.ExceptionRecord = &exceptionRecord;
    exceptionPointers.ContextRecord = &context;

    writeCrashArtifacts(reason, &exceptionPointers, detailText);
}

const char* signalName(const int signalNumber) noexcept
{
    switch (signalNumber) {
    case SIGABRT:
        return "SIGABRT";
    case SIGFPE:
        return "SIGFPE";
    case SIGILL:
        return "SIGILL";
    case SIGINT:
        return "SIGINT";
    case SIGSEGV:
        return "SIGSEGV";
    case SIGTERM:
        return "SIGTERM";
    default:
        return "UNKNOWN";
    }
}

DWORD exceptionCodeForSignal(const int signalNumber) noexcept
{
    switch (signalNumber) {
    case SIGFPE:
        return EXCEPTION_FLT_INVALID_OPERATION;
    case SIGILL:
        return EXCEPTION_ILLEGAL_INSTRUCTION;
    case SIGSEGV:
        return EXCEPTION_ACCESS_VIOLATION;
    case SIGABRT:
        return kSyntheticExceptionCodeAbort;
    default:
        return kSyntheticExceptionCodeSignal;
    }
}

LONG WINAPI topLevelExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
    writeCrashArtifacts("Unhandled SEH exception", exceptionPointers, {});
    return EXCEPTION_EXECUTE_HANDLER;
}

void signalHandler(const int signalNumber)
{
    std::string detailText = "Signal: ";
    detailText += signalName(signalNumber);
    writeSyntheticCrashArtifacts("CRT signal", exceptionCodeForSignal(signalNumber), detailText);
    std::_Exit(128 + signalNumber);
}

void terminateHandler()
{
    writeSyntheticCrashArtifacts("std::terminate",
                                 kSyntheticExceptionCodeTerminate,
                                 currentExceptionSummary());
    std::_Exit(EXIT_FAILURE);
}
#endif

}  // namespace

void BugReporter::initialize()
{
    if (g_bugReporterState.initialized) {
        return;
    }

    g_bugReporterState.diagnosticsRootPath = diagnosticsRootPathForApp();
    g_bugReporterState.sessionDirectoryPath =
        QDir(g_bugReporterState.diagnosticsRootPath).filePath(QStringLiteral("session"));
    g_bugReporterState.bugsDirectoryPath =
        QDir(g_bugReporterState.diagnosticsRootPath).filePath(QStringLiteral("bugs"));
    g_bugReporterState.crashesDirectoryPath =
        QDir(g_bugReporterState.diagnosticsRootPath).filePath(QStringLiteral("crashes"));
    g_bugReporterState.pendingCrashMarkerPath =
        QDir(g_bugReporterState.sessionDirectoryPath).filePath(QStringLiteral("pending-crash.txt"));
    g_bugReporterState.sessionLogPath =
        QDir(g_bugReporterState.sessionDirectoryPath)
            .filePath(QStringLiteral("session-%1.log").arg(timestampForFileName()));
    g_bugReporterState.liveStatePath =
        QDir(g_bugReporterState.sessionDirectoryPath).filePath(QStringLiteral("current_state.txt"));
    g_bugReporterState.sessionStartedAtText =
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    g_bugReporterState.buildStampText = QString::fromLatin1(__DATE__ " " __TIME__);
    g_bugReporterState.appName = QCoreApplication::applicationName().isEmpty()
        ? QStringLiteral("CloganGUI")
        : QCoreApplication::applicationName();
    g_bugReporterState.appVersion = QCoreApplication::applicationVersion();
    g_bugReporterState.executablePath = QCoreApplication::applicationFilePath();

    ensureDirectoryPath(g_bugReporterState.sessionDirectoryPath);
    ensureDirectoryPath(g_bugReporterState.bugsDirectoryPath);
    ensureDirectoryPath(g_bugReporterState.crashesDirectoryPath);

    g_bugReporterState.logFile.setFileName(g_bugReporterState.sessionLogPath);
    const bool logFileOpened =
        g_bugReporterState.logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    Q_UNUSED(logFileOpened)

    g_bugReporterState.environmentText = buildEnvironmentText();
#ifdef Q_OS_WIN
    g_bugReporterState.crashesDirectoryWidePath = toWide(g_bugReporterState.crashesDirectoryPath);
    g_bugReporterState.sessionLogWidePath = toWide(g_bugReporterState.sessionLogPath);
    g_bugReporterState.liveStateWidePath = toWide(g_bugReporterState.liveStatePath);
    g_bugReporterState.pendingCrashMarkerWidePath = toWide(g_bugReporterState.pendingCrashMarkerPath);
    ensureDbgHelpInitialized();
#endif
    g_bugReporterState.crashEnvironmentText = g_bugReporterState.environmentText.toUtf8().toStdString();

    g_bugReporterState.previousMessageHandler = qInstallMessageHandler(bugReporterMessageHandler);
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(topLevelExceptionFilter);
    std::set_terminate(terminateHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGTERM, signalHandler);
#endif

    g_bugReporterState.initialized = true;
    updateLiveState(QStringLiteral("Application started.\n"));
}

void BugReporter::shutdown()
{
    if (!g_bugReporterState.initialized) {
        return;
    }

    qInstallMessageHandler(g_bugReporterState.previousMessageHandler);
    g_bugReporterState.previousMessageHandler = nullptr;

    {
        QMutexLocker locker(&g_bugReporterState.logMutex);
        if (g_bugReporterState.logFile.isOpen()) {
            g_bugReporterState.logFile.flush();
            g_bugReporterState.logFile.close();
        }
    }

#ifdef Q_OS_WIN
    if (g_bugReporterState.dbgHelpInitialized) {
        SymCleanup(GetCurrentProcess());
        g_bugReporterState.dbgHelpInitialized = false;
    }
#endif

    g_bugReporterState.initialized = false;
}

QString BugReporter::diagnosticsRootPath()
{
    return g_bugReporterState.diagnosticsRootPath;
}

QString BugReporter::sessionLogPath()
{
    return g_bugReporterState.sessionLogPath;
}

QString BugReporter::liveStatePath()
{
    return g_bugReporterState.liveStatePath;
}

void BugReporter::updateLiveState(const QString& stateText)
{
    if (!g_bugReporterState.initialized || g_bugReporterState.liveStatePath.isEmpty()) {
        return;
    }

    writeTextFileAtomically(g_bugReporterState.liveStatePath, stateText);
}

BugReportExportResult BugReporter::exportBugReport(const BugReportExportRequest& request)
{
    BugReportExportResult result;
    if (!g_bugReporterState.initialized) {
        result.errorMessage = QStringLiteral("BugReporter is not initialized.");
        return result;
    }

    const QString reportDirectory = QDir(g_bugReporterState.bugsDirectoryPath)
        .filePath(QStringLiteral("bug-report-%1").arg(timestampForFileName()));
    if (!ensureDirectoryPath(reportDirectory)) {
        result.errorMessage = QStringLiteral("Failed to create the bug-report directory.");
        return result;
    }

    const QString reportPath = QDir(reportDirectory).filePath(QStringLiteral("report.txt"));
    const QString reportText = buildManualReportText(request);
    if (!writeTextFileAtomically(reportPath, reportText)) {
        result.errorMessage = QStringLiteral("Failed to write the bug-report summary.");
        return result;
    }

    copyFileIfExists(g_bugReporterState.sessionLogPath,
                     QDir(reportDirectory).filePath(QStringLiteral("session.log")));

    if (!request.stateText.isEmpty()) {
        writeTextFileAtomically(QDir(reportDirectory).filePath(QStringLiteral("current_state.txt")),
                                request.stateText);
    } else {
        copyFileIfExists(g_bugReporterState.liveStatePath,
                         QDir(reportDirectory).filePath(QStringLiteral("current_state.txt")));
    }

    copyFileIfExists(request.layoutFilePath,
                     QDir(reportDirectory).filePath(QStringLiteral("clogan_gui_layout.ini")));

    result.ok = true;
    result.directoryPath = reportDirectory;
    result.reportPath = reportPath;
    return result;
}

QString BugReporter::takePendingCrashReportDirectory()
{
    const QString markerPath = g_bugReporterState.pendingCrashMarkerPath;
    if (markerPath.isEmpty()) {
        return QString();
    }

    const QString crashDirectory = readTextFile(markerPath).trimmed();
    QFile::remove(markerPath);
    return crashDirectory;
}

[[noreturn]] void BugReporter::failFastOutOfMemory(const QString& context)
{
    const QByteArray detailUtf8 = context.toUtf8();

#ifdef Q_OS_WIN
    writeSyntheticCrashArtifacts("std::bad_alloc",
                                 kSyntheticExceptionCodeBadAlloc,
                                 std::string("Out of memory: ")
                                     + (detailUtf8.isEmpty() ? std::string("unknown context")
                                                             : std::string(detailUtf8.constData(),
                                                                           static_cast<std::size_t>(detailUtf8.size()))));
    std::_Exit(EXIT_FAILURE);
#else
    std::fprintf(stderr,
                 "CloganGUI terminating after std::bad_alloc: %s\n",
                 detailUtf8.isEmpty() ? "unknown context" : detailUtf8.constData());
    std::fflush(stderr);
    std::abort();
#endif
}

}  // namespace CHIron::Gui
