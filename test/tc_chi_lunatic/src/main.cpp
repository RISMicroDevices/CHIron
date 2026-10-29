#include "common.hpp"

int main(int argc, char** argv)
{
    const std::string               filter = argc > 1 ? argv[1] : "";
    size_t                          failures = 0;
    size_t                          executed = 0;
    for (const auto& test : LunaticTest::Registry())
    {
        if (!filter.empty() && test.name.find(filter) == std::string::npos)
            continue;
        ++executed;
        try
        {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << "Finished " << executed << " test case(s), "
              << failures << " failure(s).\n";
    return failures ? 1 : 0;
}
