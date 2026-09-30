#include "arguments.hpp"

#include <string>


namespace {

    bool StartsWith(const std::string& value, const char* prefix) noexcept
    {
        return value.rfind(prefix, 0) == 0;
    }

    std::string StripWrappedQuotes(const std::string& value) noexcept
    {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            return value.substr(1, value.size() - 2);

        return value;
    }
}


namespace Cohestra {

    ArgumentsContext ParseArguments(int argc, const char* argv[]) noexcept
    {
        ArgumentsContext context;

        if (argc > 0 && argv && argv[0])
            context.executableName = argv[0];
        else
            context.executableName = "cohestra_v3";

        bool positionalOnly = false;

        for (int i = 1; i < argc; ++i)
        {
            const char* rawArgument = argv[i];

            if (!rawArgument)
            {
                context.valid = false;
                context.errorMessage = "Null argument encountered.";
                return context;
            }

            std::string argument(rawArgument);

            if (!positionalOnly && argument == "--")
            {
                positionalOnly = true;
                continue;
            }

            if (!positionalOnly && (argument == "-h" || argument == "--help"))
            {
                context.showHelp = true;
                continue;
            }

            if (!positionalOnly && argument == "-c")
            {
                if (i + 1 >= argc || !argv[i + 1])
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for -c.";
                    return context;
                }

                context.configurationPath = StripWrappedQuotes(std::string(argv[++i]));
                continue;
            }

            if (!positionalOnly && (argument == "--cycles" || argument == "-n"))
            {
                if (i + 1 >= argc || !argv[i + 1])
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --cycles.";
                    return context;
                }

                try
                {
                    context.maximumCycles = std::stoull(argv[++i]);
                }
                catch (...)
                {
                    context.valid = false;
                    context.errorMessage = "Invalid value for --cycles.";
                    return context;
                }
                continue;
            }

            if (!positionalOnly && argument == "--config")
            {
                if (i + 1 >= argc || !argv[i + 1])
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --config.";
                    return context;
                }

                context.configurationPath = StripWrappedQuotes(std::string(argv[++i]));
                continue;
            }

            if (!positionalOnly && StartsWith(argument, "--config="))
            {
                std::string value = argument.substr(9);

                if (value.empty())
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --config.";
                    return context;
                }

                context.configurationPath = StripWrappedQuotes(value);
                continue;
            }

            if (!positionalOnly && argument == "--log-file")
            {
                context.logToFile = true;
                continue;
            }

            if (!positionalOnly && argument == "--no-log-file")
            {
                context.logToFile = false;
                continue;
            }

            if (!positionalOnly && StartsWith(argument, "--log-file="))
            {
                std::string value = argument.substr(11);

                if (value.empty())
                {
                    context.valid = false;
                    context.errorMessage = "Missing value for --log-file.";
                    return context;
                }

                context.logToFile = true;
                context.logFilePath = StripWrappedQuotes(value);
                continue;
            }

            if (!positionalOnly && !argument.empty() && argument[0] == '-')
            {
                context.valid = false;
                context.errorMessage = "Unknown argument: " + argument;
                return context;
            }

            context.positionalArguments.push_back(StripWrappedQuotes(argument));
        }

        return context;
    }
}
