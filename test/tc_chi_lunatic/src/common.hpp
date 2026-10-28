#pragma once

#include "chi/icn/lunatic/chi_lunatic.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace LunaticTest {

using Config128 = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 128, false, false, false>;
using Config256 = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 256, false, false, false>;
using Config512 = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 512, true, true, false>;

struct Case {
    std::string name;
    std::function<void()> run;
};

inline std::vector<Case>& Registry()
{
    static std::vector<Case> cases;
    return cases;
}

struct Register {
    Register(std::string name, std::function<void()> run)
    {
        Registry().push_back({ std::move(name), std::move(run) });
    }
};

inline void Check(bool condition, const char* expression, const char* file, int line)
{
    if (!condition)
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + expression);
}

template<typename Exception = std::invalid_argument, typename Callable>
void CheckThrows(Callable&& callable)
{
    bool caught = false;
    try {
        callable();
    } catch (const Exception&) {
        caught = true;
    }
    if (!caught)
        throw std::runtime_error("expected exception was not raised");
}

}

#define LUNATIC_TEST(name) \
    static void name(); \
    static ::LunaticTest::Register register_##name(#name, name); \
    static void name()

#define LUNATIC_CHECK(expression) \
    ::LunaticTest::Check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
