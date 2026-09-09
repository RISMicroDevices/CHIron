#pragma once

#ifndef __CCHI__CCHI_PROF__JOINT
#define __CCHI__CCHI_PROF__JOINT

#include <cstdint>
#include <string>
#include <algorithm>

#include "../xact/cchi_joint.hpp"

#include "cchi_prof_metrics.hpp"


/*
*   CCHI Performance Profiling Infrastructure: Joint-level profiler.
*
*   JointProfiler attaches to the EventHub of any CCHI::Xact::Joint and builds
*   the wire-level performance metric families that can be derived from the
*   transaction layer alone (no cohestra dependency):
*
*   A. Latency:    per-xaction-class histograms of total and per-phase latency
*                  (req -> first/last CompData, -> Comp, -> DBIDResp, -> CompAck,
*                  data spans, snoop response/data, CMO/Stash/EVT completions).
*                  All latencies are in simulation ticks (FiredFlit::time).
*   C. Occupancy:  in-flight xaction level (MSHR-occupancy analog), TxnID and
*                  DBID in-use levels, allocation/free counters.
*   D. Backpressure: denial counters split by XactDenial code, request/response
*                  side, and denial source (JOINT vs XACTION).
*
*   The profiler only reads event payloads; it never calls back into the Joint
*   (see the listener contract in cchi/icn/taurus/cchi_taurus_component.hpp).
*   Handlers are O(1) and perform no allocation after Attach() warm-up.
*
*   Metric naming (all inside the owned MetricSet):
*       xact.total                                 histogram: total latency of all xactions
*       xact.<XactionTypeName>.total               histogram: total latency per xaction class
*       xact.<XactionTypeName>.first_rsp           histogram: request -> first response flit
*       xact.read.first_data / .last_data / .comp_ack
*       xact.read.datasource.<v>                   counter: CompData DataSource value distribution
*       xact.dataless.comp / .comp_ack
*       xact.cmo.compcmo
*       xact.stash.comp
*       xact.write.dbid / .comp / .data_span
*       xact.writeback.dbid / .data_span
*       xact.evict.comp
*       xact.snoop.resp / .first_data
*       joint.accepted / joint.completed           counters
*       joint.inflight                             gauge (max = peak occupancy)
*       joint.inflight.ts                          gauge time series
*       txnid.allocated / txnid.freed              counters
*       txnid.inuse[.ts]                           gauge + series
*       dbid.allocated / dbid.freed                counters
*       dbid.inuse[.ts]                            gauge + series
*       joint.denied.request.total / .response.total
*       joint.denied.request.at_joint / .at_xaction  (and .response.*)
*       joint.denied.request.<XACT_DENIED_*>       counters per denial code (and .response.*)
*/
namespace CCHI::Prof {

    const char* XactionTypeName(Xact::XactionType type) noexcept;


    template<FlitConfigurationConcept config>
    class JointProfiler {
    public:
        static constexpr const char*    LISTENER_NAME = "CCHI.Prof.JointProfiler";

        struct Config {
            uint64_t    seriesWindowTicks   = 1024;     // window for *.ts series
        };

    protected:
        Xact::Joint<config>*    joint;
        Config                  profConfig;
        MetricSet               metrics;

        uint64_t                inFlight;
        uint64_t                txnIDInUse;
        uint64_t                dbidInUse;

    public:
        JointProfiler(Config profConfig = {}) noexcept;
        virtual ~JointProfiler() noexcept;

    public:
        bool                    IsAttached() const noexcept;
        bool                    Attach(Xact::Joint<config>& joint) noexcept;
        void                    Detach() noexcept;

        MetricSet&              Metrics() noexcept;
        const MetricSet&        Metrics() const noexcept;

    public:
        // Event handlers (registered on the Joint's EventHub).
        void                    HandleAccepted(Xact::JointXactionAcceptedEvent<config>& event) noexcept;
        void                    HandleComplete(Xact::JointXactionCompleteEvent<config>& event) noexcept;
        void                    HandleTxnIDAllocation(Xact::JointXactionTxnIDAllocationEvent<config>& event) noexcept;
        void                    HandleTxnIDFree(Xact::JointXactionTxnIDFreeEvent<config>& event) noexcept;
        void                    HandleDBIDAllocation(Xact::JointXactionDBIDAllocationEvent<config>& event) noexcept;
        void                    HandleDBIDFree(Xact::JointXactionDBIDFreeEvent<config>& event) noexcept;
        void                    HandleDeniedRequest(Xact::JointDeniedRequestEvent<config>& event) noexcept;
        void                    HandleDeniedResponse(Xact::JointDeniedResponseEvent<config>& event) noexcept;

    protected:
        void                    CollectLatency(const Xact::Xaction<config>& xaction) noexcept;

        static uint64_t         LastFlitTime(const Xact::Xaction<config>& xaction) noexcept;
    };
}



// Implementation of: XactionTypeName
namespace CCHI::Prof {

    inline const char* XactionTypeName(Xact::XactionType type) noexcept
    {
        switch (type)
        {
            case Xact::XactionType::NonCacheableRead:       return "NonCacheableRead";
            case Xact::XactionType::CacheableTransientRead: return "CacheableTransientRead";
            case Xact::XactionType::CacheableAllocatingRead:return "CacheableAllocatingRead";
            case Xact::XactionType::Evict:                  return "Evict";
            case Xact::XactionType::WriteBack:              return "WriteBack";
            case Xact::XactionType::CacheableDataless:      return "CacheableDataless";
            case Xact::XactionType::CMO:                    return "CMO";
            case Xact::XactionType::Stash:                  return "Stash";
            case Xact::XactionType::NonCacheableWrite:      return "NonCacheableWrite";
            case Xact::XactionType::CacheableWrite:         return "CacheableWrite";
            case Xact::XactionType::Snoop:                  return "Snoop";
            case Xact::XactionType::EvictRemote:            return "EvictRemote";
            default:                                        return "Unknown";
        }
    }
}



// Implementation of: class JointProfiler
namespace CCHI::Prof {

    template<FlitConfigurationConcept config>
    inline JointProfiler<config>::JointProfiler(Config profConfig) noexcept
        : joint     (nullptr)
        , profConfig(profConfig)
        , metrics   ()
        , inFlight  (0)
        , txnIDInUse(0)
        , dbidInUse (0)
    { }

    template<FlitConfigurationConcept config>
    inline JointProfiler<config>::~JointProfiler() noexcept
    {
        Detach();
    }

    template<FlitConfigurationConcept config>
    inline bool JointProfiler<config>::IsAttached() const noexcept
    {
        return joint != nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool JointProfiler<config>::Attach(Xact::Joint<config>& joint) noexcept
    {
        if (this->joint)
            return false;

        if (!joint.events)
            return false;

        this->joint = &joint;

        auto hub = joint.events;

        hub->OnAccepted         .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleAccepted, this));
        hub->OnComplete         .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleComplete, this));
        hub->OnTxnIDAllocation  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleTxnIDAllocation, this));
        hub->OnTxnIDFree        .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleTxnIDFree, this));
        hub->OnDBIDAllocation   .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleDBIDAllocation, this));
        hub->OnDBIDFree         .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleDBIDFree, this));
        hub->OnDeniedRequest    .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleDeniedRequest, this));
        hub->OnDeniedResponse   .Register(Gravity::MakeListener(LISTENER_NAME, 0, &JointProfiler::HandleDeniedResponse, this));

        return true;
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::Detach() noexcept
    {
        if (!joint)
            return;

        auto hub = joint->events;

        if (hub)
        {
            hub->OnAccepted         .Unregister(LISTENER_NAME);
            hub->OnComplete         .Unregister(LISTENER_NAME);
            hub->OnTxnIDAllocation  .Unregister(LISTENER_NAME);
            hub->OnTxnIDFree        .Unregister(LISTENER_NAME);
            hub->OnDBIDAllocation   .Unregister(LISTENER_NAME);
            hub->OnDBIDFree         .Unregister(LISTENER_NAME);
            hub->OnDeniedRequest    .Unregister(LISTENER_NAME);
            hub->OnDeniedResponse   .Unregister(LISTENER_NAME);
        }

        joint = nullptr;
    }

    template<FlitConfigurationConcept config>
    inline MetricSet& JointProfiler<config>::Metrics() noexcept
    {
        return metrics;
    }

    template<FlitConfigurationConcept config>
    inline const MetricSet& JointProfiler<config>::Metrics() const noexcept
    {
        return metrics;
    }

    template<FlitConfigurationConcept config>
    inline uint64_t JointProfiler<config>::LastFlitTime(const Xact::Xaction<config>& xaction) noexcept
    {
        uint64_t t = xaction.GetFirst().time;

        for (const auto& flit : xaction.GetSubsequence())
            t = std::max(t, flit.time);

        return t;
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleAccepted(Xact::JointXactionAcceptedEvent<config>& event) noexcept
    {
        auto xaction = event.GetXaction();
        if (!xaction)
            return;

        uint64_t t = xaction->GetFirst().time;

        metrics.C("joint.accepted").Inc();

        inFlight++;
        metrics.G("joint.inflight").Set(inFlight);
        metrics.S("joint.inflight.ts", profConfig.seriesWindowTicks, true).Set(t, inFlight);
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleComplete(Xact::JointXactionCompleteEvent<config>& event) noexcept
    {
        auto xaction = event.GetXaction();
        if (!xaction)
            return;

        uint64_t t = LastFlitTime(*xaction);

        metrics.C("joint.completed").Inc();

        inFlight = inFlight ? inFlight - 1 : 0;
        metrics.G("joint.inflight").Set(inFlight);
        metrics.S("joint.inflight.ts", profConfig.seriesWindowTicks, true).Set(t, inFlight);

        CollectLatency(*xaction);
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleTxnIDAllocation(Xact::JointXactionTxnIDAllocationEvent<config>& event) noexcept
    {
        auto xaction = event.GetXaction();
        if (!xaction)
            return;

        uint64_t t = xaction->GetFirst().time;

        metrics.C("txnid.allocated").Inc();

        txnIDInUse++;
        metrics.G("txnid.inuse").Set(txnIDInUse);
        metrics.S("txnid.inuse.ts", profConfig.seriesWindowTicks, true).Set(t, txnIDInUse);
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleTxnIDFree(Xact::JointXactionTxnIDFreeEvent<config>& event) noexcept
    {
        auto xaction = event.GetXaction();
        if (!xaction)
            return;

        uint64_t t = LastFlitTime(*xaction);

        metrics.C("txnid.freed").Inc();

        txnIDInUse = txnIDInUse ? txnIDInUse - 1 : 0;
        metrics.G("txnid.inuse").Set(txnIDInUse);
        metrics.S("txnid.inuse.ts", profConfig.seriesWindowTicks, true).Set(t, txnIDInUse);
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleDBIDAllocation(Xact::JointXactionDBIDAllocationEvent<config>& event) noexcept
    {
        auto xaction = event.GetXaction();
        if (!xaction)
            return;

        uint64_t t = LastFlitTime(*xaction);

        metrics.C("dbid.allocated").Inc();

        dbidInUse++;
        metrics.G("dbid.inuse").Set(dbidInUse);
        metrics.S("dbid.inuse.ts", profConfig.seriesWindowTicks, true).Set(t, dbidInUse);
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleDBIDFree(Xact::JointXactionDBIDFreeEvent<config>& event) noexcept
    {
        auto xaction = event.GetXaction();
        if (!xaction)
            return;

        uint64_t t = LastFlitTime(*xaction);

        metrics.C("dbid.freed").Inc();

        dbidInUse = dbidInUse ? dbidInUse - 1 : 0;
        metrics.G("dbid.inuse").Set(dbidInUse);
        metrics.S("dbid.inuse.ts", profConfig.seriesWindowTicks, true).Set(t, dbidInUse);
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleDeniedRequest(Xact::JointDeniedRequestEvent<config>& event) noexcept
    {
        metrics.C("joint.denied.request.total").Inc();

        if (event.GetDenialSource() == Xact::JointDenialSource::JOINT)
            metrics.C("joint.denied.request.at_joint").Inc();
        else
            metrics.C("joint.denied.request.at_xaction").Inc();

        auto denial = event.GetDenial();
        if (denial)
            metrics.C(std::string("joint.denied.request.") + denial->name).Inc();
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::HandleDeniedResponse(Xact::JointDeniedResponseEvent<config>& event) noexcept
    {
        metrics.C("joint.denied.response.total").Inc();

        if (event.GetDenialSource() == Xact::JointDenialSource::JOINT)
            metrics.C("joint.denied.response.at_joint").Inc();
        else
            metrics.C("joint.denied.response.at_xaction").Inc();

        auto denial = event.GetDenial();
        if (denial)
            metrics.C(std::string("joint.denied.response.") + denial->name).Inc();
    }

    template<FlitConfigurationConcept config>
    inline void JointProfiler<config>::CollectLatency(const Xact::Xaction<config>& xaction) noexcept
    {
        // NOTE: CCHI::Opcodes::* are plain (non-template) namespaces whose constants
        //       are typed on the default FlitConfiguration, matching the idiom used
        //       by the xaction implementations in cchi/xact/cchi_xactions/.
        uint64_t t0   = xaction.GetFirst().time;
        uint64_t tEnd = LastFlitTime(xaction);

        const std::string typeName = XactionTypeName(xaction.GetType());

        // Totals.
        metrics.H("xact.total").Record(tEnd - t0);
        metrics.H("xact." + typeName + ".total").Record(tEnd - t0);

        // Generic first-response phase (any response channel).
        uint64_t tFirstRsp = tEnd;
        for (const auto& flit : xaction.GetSubsequence())
            tFirstRsp = std::min(tFirstRsp, flit.time);
        if (!xaction.GetSubsequence().empty())
            metrics.H("xact." + typeName + ".first_rsp").Record(tFirstRsp - t0);

        // Class-specific phases.
        switch (xaction.GetType())
        {
            case Xact::XactionType::NonCacheableRead:
            case Xact::XactionType::CacheableTransientRead:
            case Xact::XactionType::CacheableAllocatingRead:
            {
                auto* firstData = xaction.GetFirstDnDAT({ Opcodes::DnDAT::CompData });
                auto* lastData  = xaction.GetLastDnDAT ({ Opcodes::DnDAT::CompData });
                auto* compAck   = xaction.GetFirstUpRSP({ Opcodes::UpRSP::CompAck });

                if (firstData)
                {
                    metrics.H("xact.read.first_data").Record(firstData->time - t0);

                    // DataSource value distribution (raw values; encoding of
                    // forwarded-vs-memory sources is configuration/domain-specific).
                    if constexpr (requires { firstData->flit.dndat.DataSource; })
                    {
                        metrics.C("xact.read.datasource."
                            + std::to_string(uint64_t(firstData->flit.dndat.DataSource))).Inc();
                    }
                }

                if (lastData)
                    metrics.H("xact.read.last_data").Record(lastData->time - t0);

                if (compAck)
                    metrics.H("xact.read.comp_ack").Record(compAck->time - t0);
                break;
            }

            case Xact::XactionType::CacheableDataless:
            {
                auto* comp    = xaction.GetFirstDnRSP({ Opcodes::DnRSP::Comp });
                auto* compAck = xaction.GetFirstUpRSP({ Opcodes::UpRSP::CompAck });

                if (comp)
                    metrics.H("xact.dataless.comp").Record(comp->time - t0);

                if (compAck)
                    metrics.H("xact.dataless.comp_ack").Record(compAck->time - t0);
                break;
            }

            case Xact::XactionType::CMO:
            {
                auto* compCMO = xaction.GetFirstDnRSP({ Opcodes::DnRSP::CompCMO });

                if (compCMO)
                    metrics.H("xact.cmo.compcmo").Record(compCMO->time - t0);
                break;
            }

            case Xact::XactionType::Stash:
            {
                auto* comp = xaction.GetFirstDnRSP({ Opcodes::DnRSP::Comp, Opcodes::DnRSP::CompStash });

                if (comp)
                    metrics.H("xact.stash.comp").Record(comp->time - t0);
                break;
            }

            case Xact::XactionType::NonCacheableWrite:
            case Xact::XactionType::CacheableWrite:
            {
                auto* dbid      = xaction.GetFirstDnRSP({ Opcodes::DnRSP::DBIDResp, Opcodes::DnRSP::CompDBIDResp });
                auto* comp      = xaction.GetFirstDnRSP({ Opcodes::DnRSP::Comp, Opcodes::DnRSP::CompDBIDResp });
                auto* firstData = xaction.GetFirstUpDAT({ Opcodes::UpDAT::NonCopyBackWrData });
                auto* lastData  = xaction.GetLastUpDAT ({ Opcodes::UpDAT::NonCopyBackWrData });

                if (dbid)
                    metrics.H("xact.write.dbid").Record(dbid->time - t0);

                if (comp)
                    metrics.H("xact.write.comp").Record(comp->time - t0);

                if (firstData && lastData)
                    metrics.H("xact.write.data_span").Record(lastData->time - firstData->time);
                break;
            }

            case Xact::XactionType::WriteBack:
            {
                auto* dbid      = xaction.GetFirstDnRSP({ Opcodes::DnRSP::DBIDResp, Opcodes::DnRSP::CompDBIDResp });
                auto* firstData = xaction.GetFirstUpDAT({ Opcodes::UpDAT::CopyBackWrData });
                auto* lastData  = xaction.GetLastUpDAT ({ Opcodes::UpDAT::CopyBackWrData });

                if (dbid)
                    metrics.H("xact.writeback.dbid").Record(dbid->time - t0);

                if (firstData && lastData)
                    metrics.H("xact.writeback.data_span").Record(lastData->time - firstData->time);
                break;
            }

            case Xact::XactionType::Evict:
            case Xact::XactionType::EvictRemote:
            {
                auto* comp = xaction.GetFirstDnRSP({ Opcodes::DnRSP::Comp });

                if (comp)
                    metrics.H("xact.evict.comp").Record(comp->time - t0);
                break;
            }

            case Xact::XactionType::Snoop:
            {
                auto* resp      = xaction.GetFirstUpRSP({ Opcodes::UpRSP::SnpResp });
                auto* firstData = xaction.GetFirstUpDAT({ Opcodes::UpDAT::SnpRespData });

                if (resp)
                    metrics.H("xact.snoop.resp").Record(resp->time - t0);

                if (firstData)
                    metrics.H("xact.snoop.first_data").Record(firstData->time - t0);
                break;
            }

            default:
                break;
        }
    }
}

#endif // __CCHI__CCHI_PROF__JOINT
