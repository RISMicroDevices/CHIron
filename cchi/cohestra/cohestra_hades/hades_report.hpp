#pragma once

#if defined(COHESTRA_HADES)
#  if defined(__has_include) && !__has_include(<hades/transport.hpp>)
#    error "cohestra_hades: COHESTRA_HADES defined but HADES-Fabric headers not on the include path (set HADES_FABRIC_ROOT)"
#  endif
#endif

#if defined(COHESTRA_HADES) || __has_include(<hades/transport.hpp>)

#include <cstdint>
#include <map>

#include "hades_interface.hpp"
#include "../../xact/cchi_joint.hpp"

namespace Cohestra {
    class Instance;
}

namespace Cohestra::Hades {

    class HadesCompletionReporter {
    protected:
        using Config = CommonFlitConfigurationType1;
        static constexpr const char* LISTENER_NAME = "Cohestra.HadesCompletionReporter";

    protected:
        Instance&       instance;
        HadesFlitStore& store;
        bool            attached = false;

        uint64_t                    completedREQ = 0;
        uint64_t                    completedEVT = 0;
        std::map<uint64_t, uint64_t> reqOpcodes;
        std::map<uint64_t, uint64_t> evtOpcodes;

    public:
        HadesCompletionReporter(Instance& instance, HadesFlitStore& store) noexcept;
        ~HadesCompletionReporter() noexcept;

    public:
        void        Attach() noexcept;
        void        Detach() noexcept;
        void        Report() const noexcept;

        uint64_t    CompletedCCHICount() const noexcept;
        uint64_t    CompletedCCHIRequestCount() const noexcept { return completedREQ; }
        uint64_t    CompletedCCHIEventCount() const noexcept { return completedEVT; }
        uint64_t    CompletedAXICount() const noexcept;
        uint64_t    CompletedAXIWriteCount() const noexcept;
        uint64_t    CompletedAXIReadCount() const noexcept;

    protected:
        void        OnCCHIComplete(CCHI::Xact::JointXactionCompleteEvent<Config>& event) noexcept;
        void        OnAXIWriteComplete(AXIWriteCompleteEvent& event) noexcept;
        void        OnAXIReadComplete(AXIReadCompleteEvent& event) noexcept;
    };
}

#endif
