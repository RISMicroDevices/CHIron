#include "arguments.hpp"

#include <cstring>


// Implementation of: struct EarthArguments
namespace Cohestra {

    EarthArguments ParseEarthArguments(int argc, const char* argv[]) noexcept
    {
        EarthArguments arguments;

        arguments.executableName = argc > 0 ? argv[0] : "cohestra_earth";

        for (int i = 1; i < argc; ++i)
        {
            const char* arg = argv[i];

            if (!std::strcmp(arg, "--help") || !std::strcmp(arg, "-h"))
            {
                arguments.showHelp = true;
                return arguments;
            }

            if (!std::strcmp(arg, "--verbose") || !std::strcmp(arg, "-v"))
            {
                arguments.verbose = true;
                continue;
            }

            if (!std::strcmp(arg, "--config"))
            {
                if (i + 1 >= argc)
                {
                    arguments.valid        = false;
                    arguments.errorMessage = "missing value for --config";
                    return arguments;
                }

                arguments.configurationPath = argv[++i];
                continue;
            }

            if (!std::strcmp(arg, "--cycles"))
            {
                if (i + 1 >= argc)
                {
                    arguments.valid        = false;
                    arguments.errorMessage = "missing value for --cycles";
                    return arguments;
                }

                try {
                    arguments.maximumCycles = std::stoull(argv[++i]);
                } catch (...) {
                    arguments.valid        = false;
                    arguments.errorMessage = "invalid value for --cycles";
                    return arguments;
                }

                continue;
            }

            if (!std::strcmp(arg, "--ports"))
            {
                if (i + 1 >= argc)
                {
                    arguments.valid        = false;
                    arguments.errorMessage = "missing value for --ports";
                    return arguments;
                }

                try {
                    arguments.portCount = std::stoul(argv[++i]);
                } catch (...) {
                    arguments.valid        = false;
                    arguments.errorMessage = "invalid value for --ports";
                    return arguments;
                }

                if (!arguments.portCount || arguments.portCount > 8)
                {
                    arguments.valid        = false;
                    arguments.errorMessage = "--ports must be within [1, 8]";
                    return arguments;
                }

                continue;
            }

            arguments.valid        = false;
            arguments.errorMessage = std::string("unrecognized argument: ") + arg;
            return arguments;
        }

        return arguments;
    }
}
