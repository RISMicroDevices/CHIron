#include "common.hpp"

int main()
{
    size_t failures = 0;
    for (const auto& test : LunaticTest::Registry())
    {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << "Finished " << LunaticTest::Registry().size() << " test case(s), "
              << failures << " failure(s).\n";
    return failures ? 1 : 0;
}
