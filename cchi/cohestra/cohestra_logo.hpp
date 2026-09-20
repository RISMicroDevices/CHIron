#pragma once

#include <string>
#include <array>

#include "spdlog/spdlog.h"


namespace Cohestra {

    class Logo {
    public:
        static constexpr size_t  Height  = 5;

    public:
        static const std::array<std::string, Height>& Get() noexcept;

        static const void PrintToLog(const std::string& loggerName = "") noexcept;
    };
}


// Implementation of: class Logo
namespace Cohestra {

    inline const std::array<std::string, Logo::Height>& Logo::Get() noexcept
    {
        static const std::array<std::string, Logo::Height> logo = {
            "   ______      __              __            ",
            "  / ____/___  / /_  ___  _____/ /__________ _",
            " / /   / __ \\/ __ \\/ _ \\/ ___/ __/ ___/ __ `/",
            "/ /___/ /_/ / / / /  __(__  ) /_/ /  / /_/ / ",
            "\\____/\\____/_/ /_/\\___/____/\\__/_/   \\__,_/  "
        };

        return logo;
    }

    inline const void Logo::PrintToLog(const std::string& loggerName) noexcept
    {
        auto logger = spdlog::get(loggerName);

        if (!logger)
            logger = spdlog::default_logger();

        for (const auto& line : Get())
            logger->info(line);
    }
}
