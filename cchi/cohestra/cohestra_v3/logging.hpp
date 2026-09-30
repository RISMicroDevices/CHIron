#pragma once

#include <cstdint>

#include "arguments.hpp"


namespace Cohestra {

    void ConfigureLogColors() noexcept;
    void ConfigureFileLogging(const ArgumentsContext& arguments) noexcept;

    // Sets the simulation tick printed by the [%k] log-line tag.
    void SetCurrentLogTime(uint64_t time) noexcept;
}
