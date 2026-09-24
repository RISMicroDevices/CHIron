#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cohestra_config.hpp"
#include "cohestra_interface.hpp"

#include "../prof/cchi_prof_metrics.hpp"
#include "../prof/cchi_prof_joint.hpp"
#include "../prof/cchi_prof_report.hpp"

#include "../icn/taurus/cchi_taurus_component.hpp"


namespace Cohestra {

    class Instance;

    /*
    *   Cohestra performance profiler.
    *
    *   Wires the CCHI infrastructure profiler (cchi/prof/) to every Type-1
    *   upstream node of a cohestra instance, following the CacheLineDataMonitor
    *   pattern (named listeners, shared-hub dedup, [profiler] INI section):
    *
    *   - one CCHI::Prof::JointProfiler per node Joint
    *       (latency histograms, in-flight occupancy, TxnID/DBID pressure,
    *        transaction-layer denial counters - metric families A/C/D)
    *   - Taurus event-hub listeners per node
    *       (per-channel flit/byte counters and time series - family B;
    *        agent denials, cache-state demotions, shadow CacheStateMap denials,
    *        cache-line grant/load/store counters - families D/E)
    *
    *   Core-side operation latency (family F) is measured actively by the
    *   Core2Core stimulator (cohestra_stimulators/core2core.*), not passively
    *   here: Taurus does not emit events on Do* issue, so issue->grant latency
    *   cannot be observed without being the issuer.
    *
    *   All listeners are read-only and O(1); the whole profiler is inert when
    *   [profiler] enable = false (enabled by default).
    *
    *   Tick unit: all latencies/series are in node ticks. The harness tells
    *   the profiler the ticks-per-cycle ratio via SetTicksPerCycle (earth: 1,
    *   v3: 2 - half-cycle ticks); it is recorded in every report header.
    */
    class Profiler {
    public:
        static const ConfigurationEntryBack<bool>           PROFILER_ENABLE;
        static const ConfigurationEntryBack<uint64_t>       PROFILER_WINDOW;
        static const ConfigurationEntryBack<std::string>    PROFILER_REPORT_JSON;
        static const ConfigurationEntryBack<std::string>    PROFILER_REPORT_CSV_DIR;
        static const ConfigurationEntryBack<bool>           PROFILER_REPORT_TEXT;

    protected:
        using TaurusNode    = CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>;
        using JointProfiler = CCHI::Prof::JointProfiler<CommonFlitConfigurationType1>;

        struct NodeContext {
            size_t                          index;
            std::unique_ptr<JointProfiler>  joint;
            CCHI::Prof::MetricSet           agent;
        };

        static constexpr const char*    LISTENER_NAME = "Cohestra.Profiler";

    protected:
        Instance&                   instance;

        bool                        enabled         = PROFILER_ENABLE.defaultValue;
        uint64_t                    window          = PROFILER_WINDOW.defaultValue;
        std::string                 reportJSON      = PROFILER_REPORT_JSON.defaultValue;
        std::string                 reportCSVDir    = PROFILER_REPORT_CSV_DIR.defaultValue;
        bool                        reportText      = PROFILER_REPORT_TEXT.defaultValue;

        unsigned                    ticksPerCycle   = 1;

        bool                        attached        = false;

        std::vector<std::unique_ptr<NodeContext>>
                                    nodes;

        std::unordered_map<const TaurusNode*, NodeContext*>
                                    nodeLookup;

        // hubs the Taurus listeners are registered on; deduplicated because a
        // harness may share one hub between all upstream nodes
        std::vector<std::shared_ptr<TaurusNode::EventHub>>
                                    attachedHubs;

    public:
        explicit                    Profiler(Instance& instance) noexcept;
                                    ~Profiler() noexcept;

    public:
        bool                        LoadConfiguration(const StringLoaderBase& loader) noexcept;

        void                        SetTicksPerCycle(unsigned ticksPerCycle) noexcept;

    public:
        void                        Attach() noexcept;
        void                        Detach() noexcept;
        bool                        IsAttached() const noexcept;

        bool                        IsEnabled() const noexcept;

    public:
        // Emits the text report (logger), the JSON document (report.json) and
        // the CSV dumps (report.csv.dir), according to configuration.
        void                        Report(uint64_t elapsedTicks) const noexcept;

    protected:
        template<class T>
        bool                        LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

        NodeContext*                ContextOf(const TaurusNode& node) const noexcept;
        NodeContext&                RequireNode(size_t index);

    protected:
        // Taurus event handlers (channel accepts: family B counters/series)
        void                        OnAcceptedEVT(CCHI::Taurus::UpstreamNodeXactAcceptedEVTEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnAcceptedSNP(CCHI::Taurus::UpstreamNodeXactAcceptedSNPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnAcceptedREQ(CCHI::Taurus::UpstreamNodeXactAcceptedREQEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnAcceptedDnRSP(CCHI::Taurus::UpstreamNodeXactAcceptedDnRSPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnAcceptedUpRSP(CCHI::Taurus::UpstreamNodeXactAcceptedUpRSPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnAcceptedDnDAT(CCHI::Taurus::UpstreamNodeXactAcceptedDnDATEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnAcceptedUpDAT(CCHI::Taurus::UpstreamNodeXactAcceptedUpDATEvent<CommonFlitConfigurationType1>& event) noexcept;

        // Taurus event handlers (agent denials: family D)
        void                        OnDeniedEVT(CCHI::Taurus::UpstreamNodeXactDeniedEVTEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedSNP(CCHI::Taurus::UpstreamNodeXactDeniedSNPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedREQ(CCHI::Taurus::UpstreamNodeXactDeniedREQEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedDnRSP(CCHI::Taurus::UpstreamNodeXactDeniedDnRSPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedUpRSP(CCHI::Taurus::UpstreamNodeXactDeniedUpRSPEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedDnDAT(CCHI::Taurus::UpstreamNodeXactDeniedDnDATEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedUpDAT(CCHI::Taurus::UpstreamNodeXactDeniedUpDATEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedPrefetch(CCHI::Taurus::UpstreamNodeXactDeniedPrefetchEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedCMO(CCHI::Taurus::UpstreamNodeXactDeniedCMOEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnDeniedCompCMO(CCHI::Taurus::UpstreamNodeXactDeniedCompCMOEvent<CommonFlitConfigurationType1>& event) noexcept;

        // Taurus event handlers (shadow CacheStateMap denials: family D)
        void                        OnCacheStateMapDeniedRequest(CCHI::Taurus::UpstreamNodeCacheStateMapDeniedRequestEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnCacheStateMapDeniedResponse(CCHI::Taurus::UpstreamNodeCacheStateMapDeniedResponseEvent<CommonFlitConfigurationType1>& event) noexcept;

        // Taurus event handlers (cache-state demotions: family E)
        void                        OnEVTDemotion(CCHI::Taurus::UpstreamNodeEVTCacheStatePostDemotionEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnSNPDemotion(CCHI::Taurus::UpstreamNodeSNPCacheStatePostDemotionEvent<CommonFlitConfigurationType1>& event) noexcept;

        // Taurus event handlers (cache-line data plane: family E/F counters)
        void                        OnCacheLineGranted(CCHI::Taurus::UpstreamNodeCacheLineGrantedEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnCacheLinePostLoad(CCHI::Taurus::UpstreamNodeCacheLinePostLoadEvent<CommonFlitConfigurationType1>& event) noexcept;
        void                        OnCacheLinePostStore(CCHI::Taurus::UpstreamNodeCacheLinePostStoreEvent<CommonFlitConfigurationType1>& event) noexcept;

    protected:
        void                        CountChannel(NodeContext& ctx, const char* channel, uint64_t time, bool data, bool tx) noexcept;
    };
}
