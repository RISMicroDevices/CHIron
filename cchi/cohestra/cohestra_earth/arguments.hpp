#pragma once

#include <cstdint>
#include <optional>
#include <string>


namespace Cohestra {

    // Command line arguments of the cohestra_earth executable.
    struct EarthArguments {
        bool                        valid               = true;
        bool                        showHelp            = false;
        std::string                 errorMessage;
        std::string                 executableName;

        std::optional<std::string>  configurationPath;

        size_t                      portCount           = 2;
        uint64_t                    maximumCycles       = 10000000;

        bool                        verbose             = false;
    };

    EarthArguments  ParseEarthArguments(int argc, const char* argv[]) noexcept;
}
