#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace Cohestra {

    struct ArgumentsContext {
        std::string                 executableName;
        bool                        valid;
        bool                        showHelp;
        std::optional<std::string>  configurationPath;
        std::vector<std::string>    positionalArguments;
        std::string                 errorMessage;
        uint64_t                    maximumCycles;
        bool                        logToFile;
        std::optional<std::string>  logFilePath;

        ArgumentsContext() noexcept
            : executableName()
            , valid(true)
            , showHelp(false)
            , configurationPath()
            , positionalArguments()
            , errorMessage()
            , maximumCycles(200000)
            , logToFile(true)
            , logFilePath()
        { }

    };

    ArgumentsContext ParseArguments(int argc, const char* argv[]) noexcept;
}
