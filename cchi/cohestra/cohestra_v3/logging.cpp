#include "logging.hpp"

#include <atomic>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include "spdlog/spdlog.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/details/fmt_helper.h"
#include "spdlog/details/os.h"


namespace
{
    // Current simulation tick, printed by the [%k] log-line tag; fed by
    // Cohestra::SetCurrentLogTime from the simulation loop.
    std::atomic<uint64_t> currentLogTime{0};

    // Tick tag flag formatter (%k): prints the current simulation tick at the
    // front of every log line (same time base as the FST waveform times and
    // the timestamps embedded in log messages). Used as "[%k]" in patterns.
    class CycleTagFormatter final : public spdlog::custom_flag_formatter
    {
    public:
        void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
        {
            spdlog::details::fmt_helper::append_int(currentLogTime.load(std::memory_order_relaxed), dest);
        }

        std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
        {
            return std::make_unique<CycleTagFormatter>();
        }
    };

#ifndef _WIN32
    // Level tag flag formatter: prints the level name in a darker color with
    // brackets in plain color, and restores the lighter message color afterwards.
    class DeepLevelTagFormatter final : public spdlog::custom_flag_formatter
    {
    public:
        void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
        {
            const char* tagColor = "\033[37m";
            const char* messageColor = "\033[0m";

            switch (msg.level)
            {
            case spdlog::level::warn:
                tagColor = "\033[33m";      // darker yellow
                messageColor = "\033[93m";  // lighter yellow
                break;

            case spdlog::level::err:
                tagColor = "\033[31m";      // darker red
                messageColor = "\033[91m";  // lighter red
                break;

            case spdlog::level::critical:
                tagColor = "\033[1m\033[41m";
                break;

            case spdlog::level::info:
                tagColor = "\033[32m";
                break;

            case spdlog::level::debug:
                tagColor = "\033[36m";
                break;

            default:
                break;
            }

            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t("\033[0m"), dest);
            dest.push_back('[');
            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t(tagColor), dest);
            spdlog::details::fmt_helper::append_string_view(spdlog::level::to_string_view(msg.level), dest);
            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t("\033[0m"), dest);
            dest.push_back(']');
            spdlog::details::fmt_helper::append_string_view(spdlog::string_view_t(messageColor), dest);
        }

        std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
        {
            return std::make_unique<DeepLevelTagFormatter>();
        }
    };
#endif

    // Builds a pattern formatter with the [%k] tick tag flag always
    // registered (and, on POSIX, optionally the [%*] deep level tag).
    std::unique_ptr<spdlog::pattern_formatter> MakeTaggedFormatter(const std::string& pattern, bool deepLevelTag)
    {
        auto formatter = std::make_unique<spdlog::pattern_formatter>();

        formatter->add_flag<CycleTagFormatter>('k');

#ifndef _WIN32
        if (deepLevelTag)
            formatter->add_flag<DeepLevelTagFormatter>('*');
#else
        (void) deepLevelTag;
#endif

        formatter->set_pattern(pattern);
        return formatter;
    }

    template<class T>
    void ConfigureColorSink(const std::shared_ptr<T>& sink, FILE* file) noexcept
    {
#ifdef _WIN32
        (void) file;

        sink->set_color(spdlog::level::warn, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        sink->set_color(spdlog::level::err, FOREGROUND_RED | FOREGROUND_INTENSITY);

        // color the whole message, not only the level tag
        sink->set_formatter(MakeTaggedFormatter("%^[%k] [%Y-%m-%d %H:%M:%S.%e] [%l] %v%$", false));
#else
        sink->set_color(spdlog::level::warn, "\033[93m"); // light yellow
        sink->set_color(spdlog::level::err, "\033[91m");  // light red

        if (spdlog::details::os::in_terminal(file) && spdlog::details::os::is_color_terminal())
        {
            // color the whole message, with a deeper color for the level tag
            // and the origin color for the tick tag and the timestamp
            sink->set_formatter(MakeTaggedFormatter(
                "%^\033[0m[%k] [%Y-%m-%d %H:%M:%S.%e] %* %v%$", true));
        }
        else
        {
            // plain layout for piped/redirected output (no colors); matches
            // the default "%+" rendering, plus the tick tag
            sink->set_formatter(MakeTaggedFormatter(
                "[%k] [%Y-%m-%d %H:%M:%S.%e] [%l] %v", false));
        }
#endif
    }

    std::string MakeDefaultLogFilePath(const std::string& executableName) noexcept
    {
        // derive the prefix from the current executable name, e.g.
        // "./build/cohestra_v3" -> "cohestra_v3", "cohestra_v3.exe" -> "cohestra_v3"
        std::string prefix = executableName;

        size_t separator = prefix.find_last_of("/\\");
        if (separator != std::string::npos)
            prefix = prefix.substr(separator + 1);

        if (prefix.size() > 4 &&
            (prefix.compare(prefix.size() - 4, 4, ".exe") == 0 ||
             prefix.compare(prefix.size() - 4, 4, ".EXE") == 0))
            prefix.resize(prefix.size() - 4);

        if (prefix.empty())
            prefix = "cohestra_v3";

        std::tm tm = spdlog::details::os::localtime(std::time(nullptr));

        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%s_%04d%02d%02d_%02d%02d%02d.log",
            prefix.c_str(),
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

        return buffer;
    }
}


namespace Cohestra {

    void SetCurrentLogTime(uint64_t time) noexcept
    {
        currentLogTime.store(time, std::memory_order_relaxed);
    }

    void ConfigureLogColors() noexcept
    {
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
            if (!logger)
                return;

            for (const auto& sink : logger->sinks())
            {
                if (auto colorSink = std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(sink))
                {
                    ConfigureColorSink(colorSink, stdout);
                }
                else if (auto colorSink = std::dynamic_pointer_cast<spdlog::sinks::stderr_color_sink_mt>(sink))
                {
                    ConfigureColorSink(colorSink, stderr);
                }
            }
        });
    }

    void ConfigureFileLogging(const ArgumentsContext& arguments) noexcept
    {
        if (!arguments.logToFile)
            return;

        std::string path = arguments.logFilePath ? *arguments.logFilePath
                                                 : MakeDefaultLogFilePath(arguments.executableName);

        try
        {
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path);
            fileSink->set_formatter(MakeTaggedFormatter("[%k] [%Y-%m-%d %H:%M:%S.%e] [%l] %v", false));
            spdlog::default_logger()->sinks().push_back(fileSink);

            spdlog::info("File logging enabled: {}.", path);
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            spdlog::warn("Failed to open log file {}: {}. File logging disabled.", path, ex.what());
        }
    }
}
