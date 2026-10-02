#pragma once

#include <memory>
#include <vector>

#include "../cohestra_boot.hpp"
#include "../cohestra_instance.hpp"
#include "../cohestra_axi.hpp"

#include "interface.hpp"
#include "interface_axi.hpp"
#include "interface_chi.hpp"


namespace Cohestra {

    /*
    *   V3BootHost: typed host context handed to boot sequencer factories by
    *   the cohestra_v3 harness.
    *
    *   A boot sequencer is free to sequence through any driver the harness
    *   owns: the CCHI master side (instance + CCHI interface), the AXI slave
    *   memory / AXI master models, the CHI interfaces, or the raw interfaces
    *   themselves. Fields not needed by a boot sequencer are simply ignored.
    */
    template<class VT>
    struct V3BootHost {

        // CCHI master side: Taurus upstream nodes and instance state
        Instance*                                       instance            = nullptr;

        // Raw bus interfaces
        std::shared_ptr<V3CCHIInterface<VT>>            cchiInterface       = nullptr;
        std::shared_ptr<V3AXISlaveInterface<VT>>        axiSlaveInterface   = nullptr;
        std::shared_ptr<V3AXIMasterInterface<VT>>       axiMasterInterface  = nullptr;
        std::shared_ptr<V3AXIMonitorInterface<VT>>      axiMonitorInterface = nullptr;
        std::shared_ptr<V3CHIFromRNInterface<VT>>       chiFromRNInterface  = nullptr;
        std::shared_ptr<V3CHIToRNInterface<VT>>         chiToRNInterface    = nullptr;
        std::shared_ptr<V3CHIFromSNInterface<VT>>       chiFromSNInterface  = nullptr;
        std::shared_ptr<V3CHIToSNInterface<VT>>         chiToSNInterface    = nullptr;
        std::shared_ptr<V3CHIMonitorInterface<VT>>      chiMonitorInterface = nullptr;

        // Bus models (owned by the harness; never nullptr, point to the
        // harness-side vectors in main())
        std::vector<std::shared_ptr<AXIMemory>>*        axiMemories         = nullptr;
        std::vector<std::shared_ptr<AXIMaster>>*        axiMasters          = nullptr;

    public:
        // Boot Host contract (BootHostContract in ../cohestra_boot.hpp).
        // The public fields above remain available for booters that need
        // drivers outside the contract (raw interfaces, AXI memories, ...).
        Instance*   GetInstance() const noexcept
        { return instance; }

        size_t      GetCCHIUpstreamCount() const noexcept
        { return instance ? instance->GetType1UpstreamCount() : 0; }

        CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*
                    GetCCHIUpstream(size_t index) const noexcept
        { return instance ? instance->GetType1Upstream(index) : nullptr; }

        size_t      GetAXIMasterCount() const noexcept
        { return axiMasters ? axiMasters->size() : 0; }

        AXIMaster*  GetAXIMaster(size_t index) const noexcept
        { return (axiMasters && index < axiMasters->size()) ? (*axiMasters)[index].get() : nullptr; }

    public:
        static_assert(BootHostContract<V3BootHost>);
    };
}
