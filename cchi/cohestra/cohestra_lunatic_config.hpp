#pragma once

#include "cohestra_lunatic.hpp"

namespace Cohestra {

    enum class LunaticDCTChoice : uint8_t {
        Default,
        Normal,
        Forward
    };

    struct LunaticIntegrationConfiguration {
        bool                        enabled = false;
        bool                        requireActivity = true;
        uint32_t                    drainCycleLimit = 10000;
        bool                        retryForceOnce = false;
        LunaticDCTChoice            dctChoice = LunaticDCTChoice::Default;
        LunaticModel::Configuration model;
    };

    bool LoadLunaticIntegrationConfiguration(const StringLoaderBase& loader,
        LunaticIntegrationConfiguration& configuration) noexcept;

}
