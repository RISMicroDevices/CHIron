#pragma once

#include "cohestra_axi.hpp"

#include "cohestra_lunatic.hpp"

namespace Cohestra {

    class LunaticAXIAdapter {
    public:
        using HomeNode = LunaticModel::HomeNode;

    private:
        HomeNode*  node;
        AXIMemory* memory;

    public:
        LunaticAXIAdapter(HomeNode& node, AXIMemory& memory) noexcept;

        void Tick() noexcept;
        bool Validate() const noexcept;
    };

}
