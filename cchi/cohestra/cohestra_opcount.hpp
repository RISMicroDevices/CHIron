#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "cohestra_config.hpp"
#include "cohestra_interface.hpp"

#include "../icn/taurus/cchi_taurus_component.hpp"


namespace Cohestra {

    class Instance;

    /*
    *   Cohestra upstream opcode counter.
    *
    *   Attaches one named listener per channel-accept event to the Taurus
    *   event hub(s) of every Type-1 upstream node (shared-hub dedup, the
    *   Profiler pattern) and counts flit opcodes per node per channel.
    *   Accept events fire once per flit accepted at the node boundary, so
    *   counts are exact wire counts: a PCrdRetried request counts once per
    *   flit it appears on the wire, and ReqLCrdReturn/PCrdReturn are counted
    *   as their own opcodes.
    *
    *   Report() prints every opcode permitted for a Type-1 node on each
    *   channel - zero counts included, so coverage gaps (e.g. disabled CMOs)
    *   stay visible. Names and the permitted set come from the CCHI opcode
    *   decoders (cchi/util/cchi_util_decoding.hpp, IterateType1).
    *
    *   The whole counter is inert when [opcount] enable = false
    *   (enabled by default).
    */
    class OpCount {
    public:
        static const ConfigurationEntryBack<bool>       OPCOUNT_ENABLE;

    protected:
        using TaurusNode = CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>;

        struct NodeCounts {
            std::map<uint64_t, uint64_t>    txEVT;
            std::map<uint64_t, uint64_t>    txREQ;
            std::map<uint64_t, uint64_t>    txUpRSP;
            std::map<uint64_t, uint64_t>    txUpDAT;
            std::map<uint64_t, uint64_t>    rxSNP;
            std::map<uint64_t, uint64_t>    rxDnRSP;
            std::map<uint64_t, uint64_t>    rxDnDAT;
        };

        static constexpr const char*        LISTENER_NAME = "Cohestra.OpCount";

    protected:
        Instance&                           instance;

        bool                                enabled     = OPCOUNT_ENABLE.defaultValue;
        bool                                attached    = false;

        std::unordered_map<const TaurusNode*, NodeCounts>
                                            nodeCounts;

        // hubs the listeners are registered on; deduplicated because a
        // harness may share one hub between all upstream nodes
        std::vector<std::shared_ptr<TaurusNode::EventHub>>
                                            attachedHubs;

    public:
        explicit                            OpCount(Instance& instance) noexcept;
                                            ~OpCount() noexcept;

    public:
        bool                                LoadConfiguration(const StringLoaderBase& loader) noexcept;

    public:
        void                                Attach() noexcept;
        void                                Detach() noexcept;
        bool                                IsAttached() const noexcept;

        bool                                IsEnabled() const noexcept;

    public:
        // Emits the per-node, per-channel opcode count table (logger).
        void                                Report() const noexcept;

    protected:
        template<class T>
        bool                                LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

    protected:
        // Taurus event handlers (channel accepts)
        void                                OnAcceptedEVT(CCHI::Taurus::UpstreamNodeXactAcceptedEVTEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                                OnAcceptedSNP(CCHI::Taurus::UpstreamNodeXactAcceptedSNPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                                OnAcceptedREQ(CCHI::Taurus::UpstreamNodeXactAcceptedREQEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                                OnAcceptedPrefetch(CCHI::Taurus::UpstreamNodeXactAcceptedPrefetchEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                                OnAcceptedDnRSP(CCHI::Taurus::UpstreamNodeXactAcceptedDnRSPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                                OnAcceptedUpRSP(CCHI::Taurus::UpstreamNodeXactAcceptedUpRSPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                                OnAcceptedDnDAT(CCHI::Taurus::UpstreamNodeXactAcceptedDnDATEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                                OnAcceptedUpDAT(CCHI::Taurus::UpstreamNodeXactAcceptedUpDATEvent<CommonFlitConfigurationType1>& event) noexcept;
    };
}
