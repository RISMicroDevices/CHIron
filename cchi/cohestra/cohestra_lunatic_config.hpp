#pragma once

#include "cohestra_lunatic.hpp"

namespace Cohestra {

    struct LunaticIntegrationConfiguration {
        bool                        enabled = false;
        bool                        requireActivity = true;
        uint32_t                    drainCycleLimit = 10000;
        LunaticModel::Configuration model;
    };

    bool LoadLunaticIntegrationConfiguration(const StringLoaderBase& loader,
        LunaticIntegrationConfiguration& configuration) noexcept;

}
