#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__COMPONENT
#define __CCHI__CCHI_ICN_TAURUS__COMPONENT

#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <deque>
#include <span>

#include "../../xact/cchi_joint.hpp"

#include "cchi_taurus_component_afx.hpp"
#include "cchi_taurus_component_events.hpp"

#include "cchi_taurus_denial.hpp"
#include "cchi_taurus_state.hpp"
#include "cchi_taurus_sam.hpp"


namespace CCHI::Taurus {

    //
    // *NOTE: callback contract - bound callbacks and event listeners run
    //        synchronously, mid-iteration over the node's containers (Tick* loops,
    //        channel Peek/Pop paths, response dispatch). A callback must NOT call
    //        back into the same node (no Do*, Peek/Pop, Push, or Tick calls):
    //        doing so can invalidate the container iteration state the caller is
    //        still using (rehash/erase -> dead iterator -> UB). See ERRATA N3.
    template<class TEvent>
    class FutureNow {
    public:
        using func_t = std::function<void(const TEvent&)>;

    protected:
        DenialEnum              denial;
        std::optional<TEvent>   event;
        std::vector<func_t>     future;

        size_t                  firedFutureCount;

    public:
        FutureNow(DenialEnum denial = Denial::NOT_INITIALIZED) noexcept;
        FutureNow(DenialEnum denial, const TEvent& event) noexcept;

    public:
        // *NOTE: uniform bind contract - every Bind* returns true iff the callback
        //        was invoked (a now-future) or accepted for later invocation (a
        //        pending future); false means the callback was dropped (a rejected
        //        or inert future).
        bool        Bind(func_t func) noexcept;
        bool        BindNow(func_t func) noexcept;
        bool        BindFuture(func_t func) noexcept;

    public:
        bool        IsRejected() const noexcept;
        bool        IsAccepted() const noexcept;
        bool        IsDone() const noexcept;

        bool        IsFuture() const noexcept;
        bool        IsNow() const noexcept;

        DenialEnum  GetDenial() const noexcept;

        size_t      Fired() const noexcept;

    public:
        void        Fire(const TEvent& event) noexcept;
    };


    //
    template<FlitConfigurationConcept config>
    class UpstreamNode {
    public:
        // *NOTE: event listeners follow the same callback contract as FutureNow
        //        (no calls back into the same node - see ERRATA N3).
        class EventHub {
        public:
            Gravity::EventBus<UpstreamNodeEVTPreHazardDetectionEvent<config>>           OnEVTPreHazardDetection;
            Gravity::EventBus<UpstreamNodeEVTPostHazardDetectionEvent<config>>          OnEVTPostHazardDetection;
            Gravity::EventBus<UpstreamNodeEVTPreHazardPendingEvent<config>>             OnEVTPreHazardPending;
            Gravity::EventBus<UpstreamNodeEVTPostHazardPendingEvent<config>>            OnEVTPostHazardPending;
            Gravity::EventBus<UpstreamNodeEVTPreChannelPendingEvent<config>>            OnEVTPreChannelPending;
            Gravity::EventBus<UpstreamNodeEVTPostChannelPendingEvent<config>>           OnEVTPostChannelPending;
            Gravity::EventBus<UpstreamNodeEVTPreHazardToChannelPendingEvent<config>>    OnEVTPreHazardToChannelPending;
            Gravity::EventBus<UpstreamNodeEVTPostHazardToChannelPendingEvent<config>>   OnEVTPostHazardToChannelPending;
            Gravity::EventBus<UpstreamNodeEVTCacheStatePreDemotionEvent<config>>        OnEVTCacheStatePreDemotion;
            Gravity::EventBus<UpstreamNodeEVTCacheStatePostDemotionEvent<config>>       OnEVTCacheStatePostDemotion;
            Gravity::EventBus<UpstreamNodeEVTDataPreHazardDetectionEvent<config>>       OnEVTDataPreHazardDetection;
            Gravity::EventBus<UpstreamNodeEVTDataPostHazardDetectionEvent<config>>      OnEVTDataPostHazardDetection;
            Gravity::EventBus<UpstreamNodeEVTDataPreHazardPendingEvent<config>>         OnEVTDataPreHazardPending;
            Gravity::EventBus<UpstreamNodeEVTDataPostHazardPendingEvent<config>>        OnEVTDataPostHazardPending;
            Gravity::EventBus<UpstreamNodeEVTDataPreChannelPendingEvent<config>>        OnEVTDataPreChannelPending;
            Gravity::EventBus<UpstreamNodeEVTDataPostChannelPendingEvent<config>>       OnEVTDataPostChannelPending;
            Gravity::EventBus<UpstreamNodeEVTDataPreHazardToChannelPendingEvent<config>>  
                                                                                        OnEVTDataPreHazardToChannelPending;
            Gravity::EventBus<UpstreamNodeEVTDataPostHazardToChannelPendingEvent<config>> 
                                                                                        OnEVTDataPostHazardToChannelPending;
            Gravity::EventBus<UpstreamNodeEVTPreChannelChosenEvent<config>>             OnEVTPreChannelChosen;
            Gravity::EventBus<UpstreamNodeEVTPostChannelChosenEvent<config>>            OnEVTPostChannelChosen;
            Gravity::EventBus<UpstreamNodeEVTUpDATPreChannelChosenEvent<config>>        OnEVTUpDATPreChannelChosen;
            Gravity::EventBus<UpstreamNodeEVTUpDATPostChannelChosenEvent<config>>       OnEVTUpDATPostChannelChosen;

            Gravity::EventBus<UpstreamNodeSNPPreHazardDetectionEvent<config>>           OnSNPPreHazardDetection;
            Gravity::EventBus<UpstreamNodeSNPPostHazardDetectionEvent<config>>          OnSNPPostHazardDetection;
            Gravity::EventBus<UpstreamNodeSNPPreHazardPendingEvent<config>>             OnSNPPreHazardPending;
            Gravity::EventBus<UpstreamNodeSNPPostHazardPendingEvent<config>>            OnSNPPostHazardPending;
            Gravity::EventBus<UpstreamNodeSNPCacheStatePreDemotionEvent<config>>        OnSNPCacheStatePreDemotion;
            Gravity::EventBus<UpstreamNodeSNPCacheStatePostDemotionEvent<config>>       OnSNPCacheStatePostDemotion;
            Gravity::EventBus<UpstreamNodeSNPRespPreChannelPendingEvent<config>>        OnSNPRespPreChannelPending;
            Gravity::EventBus<UpstreamNodeSNPRespPostChannelPendingEvent<config>>       OnSNPRespPostChannelPending;
            Gravity::EventBus<UpstreamNodeSNPRespDataPreChannelPendingEvent<config>>    OnSNPRespDataPreChannelPending;
            Gravity::EventBus<UpstreamNodeSNPRespDataPostChannelPendingEvent<config>>   OnSNPRespDataPostChannelPending;
            Gravity::EventBus<UpstreamNodeSNPUpRSPPreChannelChosenEvent<config>>        OnSNPUpRSPPreChannelChosen;
            Gravity::EventBus<UpstreamNodeSNPUpRSPPostChannelChosenEvent<config>>       OnSNPUpRSPPostChannelChosen;
            Gravity::EventBus<UpstreamNodeSNPUpDATPreChannelChosenEvent<config>>        OnSNPUpDATPreChannelChosen;
            Gravity::EventBus<UpstreamNodeSNPUpDATPostChannelChosenEvent<config>>       OnSNPUpDATPostChannelChosen;

            Gravity::EventBus<UpstreamNodeREQPreHazardDetectionEvent<config>>           OnREQPreHazardDetection;
            Gravity::EventBus<UpstreamNodeREQPostHazardDetectionEvent<config>>          OnREQPostHazardDetection;
            Gravity::EventBus<UpstreamNodeREQPreHazardPendingEvent<config>>             OnREQPreHazardPending;
            Gravity::EventBus<UpstreamNodeREQPostHazardPendingEvent<config>>            OnREQPostHazardPending;
            Gravity::EventBus<UpstreamNodeREQPreChannelPendingEvent<config>>            OnREQPreChannelPending;
            Gravity::EventBus<UpstreamNodeREQPostChannelPendingEvent<config>>           OnREQPostChannelPending;
            Gravity::EventBus<UpstreamNodeREQPreHazardToChannelPendingEvent<config>>    OnREQPreHazardToChannelPending;
            Gravity::EventBus<UpstreamNodeREQPostHazardToChannelPendingEvent<config>>   OnREQPostHazardToChannelPending;
            Gravity::EventBus<UpstreamNodeREQCompAckPreChannelPendingEvent<config>>     OnREQCompAckPreChannelPending;
            Gravity::EventBus<UpstreamNodeREQCompAckPostChannelPendingEvent<config>>    OnREQCompAckPostChannelPending;
            Gravity::EventBus<UpstreamNodeREQPreChannelChosenEvent<config>>             OnREQPreChannelChosen;
            Gravity::EventBus<UpstreamNodeREQPostChannelChosenEvent<config>>            OnREQPostChannelChosen;
            Gravity::EventBus<UpstreamNodeREQUpRSPPreChannelChosenEvent<config>>        OnREQUpRSPPreChannelChosen;
            Gravity::EventBus<UpstreamNodeREQUpRSPPostChannelChosenEvent<config>>       OnREQUpRSPPostChannelChosen;

            Gravity::EventBus<UpstreamNodeXactAcceptedEVTEvent<config>>                 OnAcceptedEVT;
            Gravity::EventBus<UpstreamNodeXactAcceptedSNPEvent<config>>                 OnAcceptedSNP;
            Gravity::EventBus<UpstreamNodeXactAcceptedREQEvent<config>>                 OnAcceptedREQ;
            Gravity::EventBus<UpstreamNodeXactAcceptedDnRSPEvent<config>>               OnAcceptedDnRSP;
            Gravity::EventBus<UpstreamNodeXactAcceptedUpRSPEvent<config>>               OnAcceptedUpRSP;
            Gravity::EventBus<UpstreamNodeXactAcceptedDnDATEvent<config>>               OnAcceptedDnDAT;
            Gravity::EventBus<UpstreamNodeXactAcceptedUpDATEvent<config>>               OnAcceptedUpDAT;
            Gravity::EventBus<UpstreamNodeXactAcceptedPrefetchEvent<config>>            OnAcceptedPrefetch;
            Gravity::EventBus<UpstreamNodeXactAcceptedCMOEvent<config>>                 OnAcceptedCMO;
            Gravity::EventBus<UpstreamNodeXactAcceptedCompCMOEvent<config>>             OnAcceptedCompCMO;

            Gravity::EventBus<UpstreamNodeXactDeniedEVTEvent<config>>                   OnDeniedEVT;
            Gravity::EventBus<UpstreamNodeXactDeniedSNPEvent<config>>                   OnDeniedSNP;
            Gravity::EventBus<UpstreamNodeXactDeniedREQEvent<config>>                   OnDeniedREQ;
            Gravity::EventBus<UpstreamNodeXactDeniedDnRSPEvent<config>>                 OnDeniedDnRSP;
            Gravity::EventBus<UpstreamNodeXactDeniedUpRSPEvent<config>>                 OnDeniedUpRSP;
            Gravity::EventBus<UpstreamNodeXactDeniedDnDATEvent<config>>                 OnDeniedDnDAT;
            Gravity::EventBus<UpstreamNodeXactDeniedUpDATEvent<config>>                 OnDeniedUpDAT;
            Gravity::EventBus<UpstreamNodeXactDeniedPrefetchEvent<config>>              OnDeniedPrefetch;
            Gravity::EventBus<UpstreamNodeXactDeniedCMOEvent<config>>                   OnDeniedCMO;
            Gravity::EventBus<UpstreamNodeXactDeniedCompCMOEvent<config>>               OnDeniedCompCMO;

            Gravity::EventBus<UpstreamNodeCacheLineGrantedEvent<config>>                OnCacheLineGranted;
            Gravity::EventBus<UpstreamNodeCacheLinePreLoadEvent<config>>                OnCacheLinePreLoad;
            Gravity::EventBus<UpstreamNodeCacheLinePostLoadEvent<config>>               OnCacheLinePostLoad;
            Gravity::EventBus<UpstreamNodeCacheLinePreStoreEvent<config>>               OnCacheLinePreStore;
            Gravity::EventBus<UpstreamNodeCacheLinePostStoreEvent<config>>              OnCacheLinePostStore;
            
        public:
            EventHub() noexcept;
            void Clear() noexcept;
        };

        std::shared_ptr<EventHub> events;

    public:
        class GrantedEvent;
        class EvictedEvent;

        class CacheLine {
            friend class UpstreamNode<config>;

        protected:
            // owning node - lets the Load/Store accessors reach the node's
            // EventHub to fire the CacheLine Pre/Post Load/Store events.
            // *NOTE: lifetime contract - a shared_ptr handle from GetCacheLine()
            //        keeps the line object alive past reap and past node
            //        destruction, but must not be USED after the node is gone:
            //        every Load*/Store* accessor dereferences this raw pointer
            //        to fire events that reference the node (use-after-free).
            //        Event payloads hold the line by weak_ptr: their
            //        GetCacheLine() locks to null once the line is reaped or
            //        the node is destroyed - always check it. GetData()/GetPA()/
            //        GetState() and the in-flight/hazard predicates never touch
            //        owner and stay safe on an escaped shared_ptr handle.
            UpstreamNode<config>*                       owner;
            uint64_t                                    PA;
            uint64_t                                    data[8]                 = {};
            CacheStateEnum                              state                   = CacheState::Invalid;

            std::shared_ptr<Xact::Xaction<config>>      activeEVT               = nullptr;
            std::shared_ptr<FutureNow<EvictedEvent>>    activeEVTFuture         = nullptr;
            std::optional<Flits::EVT<config>>           pendingEVTHazardTXEVT   = std::nullopt;
            std::optional<Flits::EVT<config>>           pendingEVTChannelTXEVT  = std::nullopt;
            std::optional<Flits::UpDAT<config>>         pendingEVTHazardTXDAT0  = std::nullopt;
            std::optional<Flits::UpDAT<config>>         pendingEVTHazardTXDAT1  = std::nullopt;
            std::optional<Flits::UpDAT<config>>         pendingEVTChannelTXDAT0 = std::nullopt;
            std::optional<Flits::UpDAT<config>>         pendingEVTChannelTXDAT1 = std::nullopt;

            std::shared_ptr<Xact::Xaction<config>>      activeSNP               = nullptr;
            std::optional<Flits::SNP<config>>           pendingSNPHazardRXSNP   = std::nullopt;
            std::optional<Flits::UpRSP<config>>         pendingSNPChannelTXRSP  = std::nullopt;
            std::optional<Flits::UpDAT<config>>         pendingSNPChannelTXDAT0 = std::nullopt;
            std::optional<Flits::UpDAT<config>>         pendingSNPChannelTXDAT1 = std::nullopt;
            
            std::shared_ptr<Xact::Xaction<config>>      activeREQ               = nullptr;
            std::shared_ptr<FutureNow<GrantedEvent>>    activeREQFuture         = nullptr;
            std::optional<Flits::REQ<config>>           pendingREQHazardTXREQ   = std::nullopt;
            std::optional<Flits::REQ<config>>           pendingREQChannelTXREQ  = std::nullopt;
            std::optional<Flits::UpRSP<config>>         pendingREQChannelTXRSP  = std::nullopt;

            // *NOTE: emission ages - each is generated once at action start (in the
            //        Do* entry point or at snoop acceptance, hazard pend included) and
            //        covers every flit the action emits on the channel(s): ageTXEVT
            //        covers the TXEVT flit and its TXDAT copyback beats, ageTXREQ the
            //        TXREQ flit and its TXRSP CompAck, ageSNP the TXRSP SnpResp and
            //        the TXDAT SnpRespData beats. An age is only consulted while a
            //        slot it covers is pending; Is*InFlight includes the pended
            //        slots, so a covered age field is never overwritten early.
            uint64_t                                    ageTXEVT                = 0;
            uint64_t                                    ageTXREQ                = 0;
            uint64_t                                    ageSNP                  = 0;

        public:
            CacheLine(UpstreamNode<config>* owner, uint64_t PA) noexcept;

        public:
            // *NOTE: raw unchecked view - no Invalid/fill guards; prefer Load/LoadXX
            //        for checked access (they fail while the line is Invalid or filling)
            const std::span<const uint64_t, 8>      GetData() const noexcept;

        public:
            std::optional<std::span<const uint64_t, 8>>
                                                    Load() const noexcept;
            std::optional<uint64_t>                 Load64(size_t alignedOffset) const noexcept;
            std::optional<uint32_t>                 Load32(size_t alignedOffset) const noexcept;
            std::optional<uint16_t>                 Load16(size_t alignedOffset) const noexcept;
            std::optional<uint8_t>                  Load8(size_t alignedOffset) const noexcept;

            bool                                    Store(const std::span<const uint64_t, 8>& lineValue) noexcept;
            bool                                    Store64(size_t alignedOffset, uint64_t value) noexcept;
            bool                                    Store32(size_t alignedOffset, uint32_t value) noexcept;
            bool                                    Store16(size_t alignedOffset, uint16_t value) noexcept;
            bool                                    Store8(size_t alignedOffset, uint8_t value) noexcept;

        public:
            uint64_t                                GetPA() const noexcept;
            CacheStateEnum                          GetState() const noexcept;
        
        public:
            bool                                    IsEVTInFlight(const Xact::Global<config>& glbl) const noexcept;
            bool                                    IsSNPInFlight(const Xact::Global<config>& glbl) const noexcept;
            bool                                    IsREQInFlight(const Xact::Global<config>& glbl) const noexcept;

            // true while an allocating read has promoted the state at the first
            // CompData beat but later beats are still outstanding - the data plane
            // is not consistent yet and the checked Load/Store accessors fail
            bool                                    IsREQFilling() const noexcept;
            
            bool                                    HasREQHazard(const Xact::Global<config>& glbl) const noexcept;
            bool                                    HasSNPHazard(const Xact::Global<config>& glbl) const noexcept;
            bool                                    HasEVTHazard(const Xact::Global<config>& glbl) const noexcept;
            bool                                    HasEVTDataHazard(const Xact::Global<config>& glbl, Flits::DnDAT<config>::dataid_t dataId) const noexcept;

            // an in-flight EVT that already received its first response
            // (Comp for Evict; Comp or CompDBIDResp for WriteBack)
            bool                                    GotEVTComp(const Xact::Global<config>& glbl) const noexcept;
        };

        class CacheLineEventBase {
        protected:
            // non-owning: breaking the line -> future -> fired-event -> line
            // cycle - the cacheable map is the sole long-term owner, so the
            // reap erase actually frees the line
            std::weak_ptr<CacheLine>                cacheLine;
            // value-cached: GetPA() stays valid on an expired payload
            uint64_t                                PA;

        public:
            CacheLineEventBase(std::shared_ptr<CacheLine> cacheLine) noexcept;

        public:
            uint64_t                                GetPA() const noexcept;
            // non-owning: locks to null once the line is reaped or the node is
            // destroyed - always check the result (see the lifetime contract
            // on CacheLine::owner)
            std::shared_ptr<CacheLine>              GetCacheLine() noexcept;
            std::shared_ptr<const CacheLine>        GetCacheLine() const noexcept;
        };

        class GrantedEvent : public CacheLineEventBase {
        public:
            GrantedEvent(std::shared_ptr<CacheLine> cacheLine) noexcept;
        };
        
        class EvictedEvent : public CacheLineEventBase {
        public:
            EvictedEvent(std::shared_ptr<CacheLine> cacheLine) noexcept;
        };

    public:
        class PrefetchEmittedEvent;

        class PrefetchEntry {
            friend class UpstreamNode<config>;

        protected:
            Flits::REQ<config>                          prefetchFlit;
            std::shared_ptr<FutureNow<PrefetchEmittedEvent>>
                                                        future;

            // action-start age of the DoPrefetch* call (see CacheLine::ageTXEVT)
            uint64_t                                    age = 0;

        public:
            PrefetchEntry(const Flits::REQ<config>& prefetchFlit, std::shared_ptr<FutureNow<PrefetchEmittedEvent>> future) noexcept;

        public:
            Flits::REQ<config>&                         GetPrefetchFlit() noexcept;
            const Flits::REQ<config>&                   GetPrefetchFlit() const noexcept;
        };

        class PrefetchEmittedEvent {
        protected:
            uint64_t                                PA;
            // owned copy: the queue entry is destroyed right after the future fires
            Flits::REQ<config>                  prefetchFlit;

        public:
            PrefetchEmittedEvent(uint64_t PA, const Flits::REQ<config>& prefetchFlit) noexcept;

        public:
            uint64_t                                GetPA() const noexcept;
            const Flits::REQ<config>&               GetPrefetchFlit() const noexcept;
        };

    public:
        class CMOCompleteEvent;

        class CMOEntry {
            friend class UpstreamNode<config>;

        protected:
            Flits::REQ<config>                          cmoFlit;
            std::shared_ptr<FutureNow<CMOCompleteEvent>> 
                                                        future;

            // action-start age of the DoCBO* call (see CacheLine::ageTXEVT)
            uint64_t                                    age = 0;

        public:
            CMOEntry(const Flits::REQ<config>& cmoFlit, std::shared_ptr<FutureNow<CMOCompleteEvent>> future) noexcept;

        public:
            Flits::REQ<config>&                         GetCMOFlit() noexcept;
            const Flits::REQ<config>&                   GetCMOFlit() const noexcept;
        };

        class CMOCompleteEvent {
        protected:
            std::shared_ptr<Xact::Xaction<config>>  xaction;

        public:
            CMOCompleteEvent(std::shared_ptr<Xact::Xaction<config>> xaction) noexcept;

        public:
            uint64_t                                GetPA() const noexcept;
            std::shared_ptr<Xact::Xaction<config>>  GetXaction() noexcept;
            std::shared_ptr<const Xact::Xaction<config>>
                                                    GetXaction() const noexcept;
        };

    public:
        class ReadEvent {
        protected:
            uint64_t                                PA;
            std::shared_ptr<uint64_t[]>             data;

        public:
            ReadEvent(uint64_t PA) noexcept;
            ReadEvent(uint64_t PA, std::shared_ptr<uint64_t[]> data) noexcept;

        public:
            uint64_t                                GetPA() const noexcept;

            std::shared_ptr<uint64_t[]>             GetData() noexcept;
            std::shared_ptr<const uint64_t[]>       GetData() const noexcept;
        };

        class CompleteEvent {
        protected:
            uint64_t                                PA;

        public:
            CompleteEvent(uint64_t PA) noexcept;

        public:
            uint64_t                                GetPA() const noexcept;
        };

    protected:
        size_t                                  xactionLimitEVT;
        size_t                                  xactionLimitSNP;
        size_t                                  xactionLimitREQ;
        size_t                                  xactionLimitTotal;

        size_t                                  prefetchQueueLimit;
        size_t                                  cmoQueueLimit;

    protected:
        Xact::Joint<config>                     joint;

        std::unordered_map<uint64_t, std::shared_ptr<CacheLine>>
                                                cacheable;

        std::unordered_map<uint64_t, std::shared_ptr<Xact::Xaction<config>>>
                                                noncacheable;

        std::deque<PrefetchEntry>               prefetchQueue;

        std::deque<CMOEntry>                    cmoQueue;

        // emitted CMO requests awaiting their CompCMO completion
        std::deque<CMOEntry>                    cmoTracker;

    protected:
        // TxnID allocation bitmap for upstream-initiated (REQ/EVT) transactions,
        // one bit per ID of the configured total xaction limit (clamped to the
        // TxnID field capacity, see the constructor)
        std::vector<bool>                       usedTxnID;

    public:
        uint64_t                                time;

        Xact::Global<config>                    glbl;

        Flits::up_nodeid_t<config>              nodeID;
        std::shared_ptr<SAM<config>>            sam;

        bool                                    enableSilentEviction;
        bool                                    enableStrictInitialState;

    public:
        UpstreamNode(
            Flits::up_nodeid_t<config>      nodeID = 0,
            std::shared_ptr<SAM<config>>    sam = nullptr,
            size_t                          xactionLimitEVT = 16,
            size_t                          xactionLimitSNP = 16,
            size_t                          xactionLimitREQ = 16,
            size_t                          xactionLimitTotal = (size_t{1} << config::txnIdWidth),
            size_t                          prefetchQueueLimit = 4,
            size_t                          cmoQueueLimit = 4,
            bool                            enableSilentEviction = false,
            bool                            enableStrictInitialState = true
        ) noexcept;

    protected:
        std::optional<size_t>                   AllocateTxnID() noexcept;
        void                                    FreeTxnID(size_t txnID) noexcept;

        bool                                    IsEVTInFlight(const CacheLine& cacheLine) const noexcept;
        bool                                    IsSNPInFlight(const CacheLine& cacheLine) const noexcept;
        bool                                    IsREQInFlight(const CacheLine& cacheLine) const noexcept;

        // true while an allocating read has promoted the state at the first CompData
        // beat but later beats are still outstanding - a hit must not be granted yet
        bool                                    IsREQFilling(const CacheLine& cacheLine) const noexcept;

        bool                                    HasREQHazard(const CacheLine& cacheLine) const noexcept;
        bool                                    HasSNPHazard(const CacheLine& cacheLine) const noexcept;
        bool                                    HasEVTHazard(const CacheLine& cacheLine) const noexcept;
        bool                                    HasEVTDataHazard(const CacheLine& cacheLine, Flits::DnDAT<config>::dataid_t dataId) const noexcept;

        size_t                                  CountInFlightEVT() const noexcept;
        size_t                                  CountInFlightSNP() const noexcept;
        size_t                                  CountInFlightREQ() const noexcept;

        // *NOTE: line-base contract -- the node tracks 64-byte cache lines. Any
        //        PA handed to a Do*/query API is normalized to its line base:
        //        the same CacheLine, hazard state and wire flit (Addr, always
        //        Size=B64) serve every address inside the line, and
        //        CacheLine::GetPA() reports the line base.
        static constexpr uint64_t               LineBase(uint64_t PA) noexcept;

        // map key: the cache-line base with its always-zero low bits shifted out
        static constexpr uint64_t               LineKey(uint64_t PA) noexcept;
    
    public:
        Xact::Joint<config>&                    GetJoint() noexcept;
        const Xact::Joint<config>&              GetJoint() const noexcept;
        
    public:
        bool                                    IsValid(uint64_t PA) const noexcept;
        CacheStateEnum                          GetState(uint64_t PA) const noexcept;
        // the returned handle must not be used after this node is destroyed -
        // see the lifetime contract on CacheLine::owner
        std::shared_ptr<CacheLine>              GetCacheLine(uint64_t PA) const noexcept;

    protected:
        void                                    SetCacheLine(std::shared_ptr<CacheLine> cacheLine) noexcept;

    public:
        std::shared_ptr<FutureNow<GrantedEvent>>        DoLoad(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<GrantedEvent>>        DoStore(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<GrantedEvent>>        DoStoreLine(uint64_t PA) noexcept;

        std::shared_ptr<FutureNow<EvictedEvent>>        DoEvict(uint64_t PA) noexcept;

        bool                                            DoEvictSilently(uint64_t PA) noexcept;

        std::shared_ptr<FutureNow<PrefetchEmittedEvent>>
                                                        DoPrefetchLoad(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<PrefetchEmittedEvent>>
                                                        DoPrefetchStore(uint64_t PA) noexcept;

        std::shared_ptr<FutureNow<ReadEvent>>           DoLoadThrough(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CompleteEvent>>       DoStoreThrough(uint64_t PA, const uint64_t data[8]) noexcept;

        std::shared_ptr<FutureNow<ReadEvent>>           DoNonCacheableRead(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CompleteEvent>>       DoNonCacheableWrite(uint64_t PA, const uint64_t data[8]) noexcept;

        std::shared_ptr<FutureNow<CMOCompleteEvent>>    DoCBOClean(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CMOCompleteEvent>>    DoCBOFlush(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CMOCompleteEvent>>    DoCBOInval(uint64_t PA) noexcept;

    public:
        void                                    Tick(uint64_t time) noexcept;

    protected:  
        void                                    TickEVT() noexcept;
        void                                    TickSNP() noexcept;
        void                                    TickREQ() noexcept;

    protected:
        // monotonic action-age source: one stamp per action, generated at action
        // start (the Do* entry points and snoop acceptance) - see CacheLine::ageTXEVT
        uint64_t                                emissionAgeCounter = 0;

        // per-channel emission order: one (action age, LineKey) entry per pending
        // channel slot, kept sorted by AgedPush - the emission order per channel
        // equals the action-start (Do* call) order
        std::deque<std::pair<uint64_t, uint64_t>>
                                                agedTXEVT;
        std::deque<std::pair<uint64_t, uint64_t>>
                                                agedTXREQ;  // per-line REQs only
        std::deque<std::pair<uint64_t, uint64_t>>
                                                agedTXRSP;  // SNP-RSP and CompAck slots
        std::deque<std::pair<uint64_t, uint64_t>>
                                                agedTXDAT;  // SNP-DAT0/1 and EVT-DAT0/1 slots

    protected:
        uint64_t                                NextEmissionAge() noexcept;

        // insert at the first position with a greater age (stable for equal
        // ages); the common case - the new entry is the youngest - is O(1) at
        // the back. Sorted insertion (not plain push_back) because hazard-to-
        // channel transfers, Tick-time pends and hash-order Tick releases can
        // push ages older than entries already in the deque.
        void                                    AgedPush(std::deque<std::pair<uint64_t, uint64_t>>& queue,
                                                         uint64_t age, uint64_t key) noexcept;

        // erase the first entry matching (age, key) - O(m), m = pending
        // candidates on that channel (tiny)
        void                                    AgedErase(std::deque<std::pair<uint64_t, uint64_t>>& queue,
                                                          uint64_t age, uint64_t key) noexcept;

        // pend helpers: every channel-slot assignment goes through these, so a
        // pended slot and its aged* deque entry can never diverge
        void                                    PendEVTChannelTXEVT(CacheLine& cacheLine, const Flits::EVT<config>& flit, uint64_t age) noexcept;
        void                                    PendREQChannelTXREQ(CacheLine& cacheLine, const Flits::REQ<config>& flit, uint64_t age) noexcept;
        void                                    PendREQChannelTXRSP(CacheLine& cacheLine, const Flits::UpRSP<config>& flit) noexcept;
        void                                    PendSNPChannelTXRSP(CacheLine& cacheLine, const Flits::UpRSP<config>& flit) noexcept;
        void                                    PendSNPChannelTXDAT0(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept;
        void                                    PendSNPChannelTXDAT1(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept;
        void                                    PendEVTChannelTXDAT0(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept;
        void                                    PendEVTChannelTXDAT1(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept;

    public:
        // *NOTE: emission-order contract - per channel, pending flits are offered
        //        in action-start order: the emission order extracted from the
        //        action ages is identical to the Do* call order (snoops ordered
        //        by acceptance), across lines and across action classes
        //        (DoLoad/DoStore*/DoEvict, DoPrefetch*, DoCBO* - no action class
        //        has fixed precedence; TXREQ merges per-line REQs and the
        //        prefetch/CMO queue fronts into one age order). Per-line slot
        //        priorities are unchanged (SNP-RSP before CompAck; SNP-DAT0,
        //        SNP-DAT1, EVT-DAT0, EVT-DAT1) - a line selected through an
        //        older slot's deque entry may emit its newer slot's flit.
        //        Hazard-parked flits enter the order with their action-start
        //        age when they are released to the channel. A PreChannelChosen
        //        cancellation keeps the flit pended, skips that line for the
        //        rest of the call and picks the next candidate strictly in age
        //        order - a cancelled candidate is never re-offered within the
        //        same call, and is offered first again on the next call.
        std::optional<Flits::EVT<config>>       PeekTXEVT() noexcept;
        std::optional<Flits::EVT<config>>       PopTXEVT() noexcept;

        std::optional<Flits::REQ<config>>       PeekTXREQ() noexcept;
        std::optional<Flits::REQ<config>>       PopTXREQ() noexcept;

        bool                                    PushRXSNP(const Flits::SNP<config>& snpFlit) noexcept;

        std::optional<Flits::UpRSP<config>>     PeekTXRSP() noexcept;
        std::optional<Flits::UpRSP<config>>     PopTXRSP() noexcept;

        std::optional<Flits::UpDAT<config>>     PeekTXDAT() noexcept;
        std::optional<Flits::UpDAT<config>>     PopTXDAT() noexcept;

        bool                                    PushRXRSP(const Flits::DnRSP<config>& dnrspFlit) noexcept;

        bool                                    PushRXDAT(const Flits::DnDAT<config>& dndatFlit) noexcept;
    };
}


// Implementation of: class UpstreamNode
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::UpstreamNode(
        Flits::up_nodeid_t<config>      nodeID,
        std::shared_ptr<SAM<config>>    sam,
        size_t                          xactionLimitEVT,
        size_t                          xactionLimitSNP,
        size_t                          xactionLimitREQ,
        size_t                          xactionLimitTotal,
        size_t                          prefetchQueueLimit,
        size_t                          cmoQueueLimit,
        bool                            enableSilentEviction,
        bool                            enableStrictInitialState
    ) noexcept
        : events                    (std::make_shared<EventHub>())
        , xactionLimitEVT           (xactionLimitEVT)
        , xactionLimitSNP           (xactionLimitSNP)
        , xactionLimitREQ           (xactionLimitREQ)
        // a configured total limit beyond the TxnID field capacity is clamped:
        // wider IDs would truncate in flit.TxnID and alias on the wire
        , xactionLimitTotal         (xactionLimitTotal < (size_t{1} << config::txnIdWidth)
                                        ? xactionLimitTotal : (size_t{1} << config::txnIdWidth))
        , prefetchQueueLimit        (prefetchQueueLimit)
        , cmoQueueLimit             (cmoQueueLimit)
        , joint                     ()
        , cacheable                 ()
        , noncacheable              ()
        , usedTxnID                 (xactionLimitTotal < (size_t{1} << config::txnIdWidth)
                                        ? xactionLimitTotal : (size_t{1} << config::txnIdWidth), false)
        , time                      (0)
        , glbl                      ()
        , nodeID                    (nodeID)
        , sam                       (sam ? std::move(sam) : std::make_shared<NoSAM<config>>())
        , enableSilentEviction      (enableSilentEviction)
        , enableStrictInitialState  (enableStrictInitialState)
    { }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::IsEVTInFlight(const CacheLine& cacheLine) const noexcept
    {
        return cacheLine.IsEVTInFlight(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::IsSNPInFlight(const CacheLine& cacheLine) const noexcept
    {
        return cacheLine.IsSNPInFlight(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::IsREQInFlight(const CacheLine& cacheLine) const noexcept
    {
        return cacheLine.IsREQInFlight(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::IsREQFilling(const CacheLine& cacheLine) const noexcept
    {
        return cacheLine.IsREQFilling();
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::HasREQHazard(const CacheLine& cacheLine) const noexcept
    {
        return cacheLine.HasREQHazard(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::HasSNPHazard(const CacheLine& cacheLine) const noexcept
    {
        return cacheLine.HasSNPHazard(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::HasEVTHazard(const CacheLine& cacheLine) const noexcept
    {
        return cacheLine.HasEVTHazard(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::HasEVTDataHazard(const CacheLine& cacheLine, Flits::DnDAT<config>::dataid_t dataId) const noexcept
    {
        return cacheLine.HasEVTDataHazard(glbl, dataId);
    }

    template<FlitConfigurationConcept config>
    inline size_t UpstreamNode<config>::CountInFlightEVT() const noexcept
    {
        size_t count = 0;

        for (const auto& [PA, cacheLine] : cacheable)
            if (cacheLine->IsEVTInFlight(glbl))
                count++;

        return count;
    }

    template<FlitConfigurationConcept config>
    inline size_t UpstreamNode<config>::CountInFlightSNP() const noexcept
    {
        size_t count = 0;

        for (const auto& [PA, cacheLine] : cacheable)
            if (cacheLine->IsSNPInFlight(glbl))
                count++;

        return count;
    }

    template<FlitConfigurationConcept config>
    inline size_t UpstreamNode<config>::CountInFlightREQ() const noexcept
    {
        size_t count = 0;

        for (const auto& [PA, cacheLine] : cacheable)
            if (cacheLine->IsREQInFlight(glbl))
                count++;

        return count;
    }

    template<FlitConfigurationConcept config>
    inline Xact::Joint<config>& UpstreamNode<config>::GetJoint() noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept config>
    inline const Xact::Joint<config>& UpstreamNode<config>::GetJoint() const noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept config>
    inline constexpr uint64_t UpstreamNode<config>::LineBase(uint64_t PA) noexcept
    {
        return PA & ~((uint64_t{1} << Sizes::B64) - 1);
    }

    template<FlitConfigurationConcept config>
    inline constexpr uint64_t UpstreamNode<config>::LineKey(uint64_t PA) noexcept
    {
        return LineBase(PA) >> Sizes::B64;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::IsValid(uint64_t PA) const noexcept
    {
        return cacheable.contains(LineKey(PA));
    }

    template<FlitConfigurationConcept config>
    inline CacheStateEnum UpstreamNode<config>::GetState(uint64_t PA) const noexcept
    {
        auto it = cacheable.find(LineKey(PA));
        if (it == cacheable.end())
            return CacheState::Invalid;

        return it->second->state;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<typename UpstreamNode<config>::CacheLine> UpstreamNode<config>::GetCacheLine(uint64_t PA) const noexcept
    {
        auto it = cacheable.find(LineKey(PA));
        if (it == cacheable.end())
            return nullptr;

        return it->second;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::SetCacheLine(std::shared_ptr<CacheLine> cacheLine) noexcept
    {
        // key by the line's own normalized PA (never a caller-supplied
        // argument): the map key and the line can never disagree
        cacheable[LineKey(cacheLine->PA)] = cacheLine;
    }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNode<config>::NextEmissionAge() noexcept
    {
        return ++emissionAgeCounter;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::AgedPush(std::deque<std::pair<uint64_t, uint64_t>>& queue, uint64_t age, uint64_t key) noexcept
    {
        size_t pos = queue.size();

        while (pos > 0 && queue[pos - 1].first > age)
            --pos;

        queue.insert(queue.begin() + pos, { age, key });
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::AgedErase(std::deque<std::pair<uint64_t, uint64_t>>& queue, uint64_t age, uint64_t key) noexcept
    {
        for (auto it = queue.begin(); it != queue.end(); ++it)
            if (it->first == age && it->second == key)
            {
                queue.erase(it);
                return;
            }
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendEVTChannelTXEVT(CacheLine& cacheLine, const Flits::EVT<config>& flit, uint64_t age) noexcept
    {
        cacheLine.pendingEVTChannelTXEVT = flit;
        cacheLine.ageTXEVT = age;
        AgedPush(agedTXEVT, age, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendREQChannelTXREQ(CacheLine& cacheLine, const Flits::REQ<config>& flit, uint64_t age) noexcept
    {
        cacheLine.pendingREQChannelTXREQ = flit;
        cacheLine.ageTXREQ = age;
        AgedPush(agedTXREQ, age, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendREQChannelTXRSP(CacheLine& cacheLine, const Flits::UpRSP<config>& flit) noexcept
    {
        cacheLine.pendingREQChannelTXRSP = flit;
        AgedPush(agedTXRSP, cacheLine.ageTXREQ, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendSNPChannelTXRSP(CacheLine& cacheLine, const Flits::UpRSP<config>& flit) noexcept
    {
        cacheLine.pendingSNPChannelTXRSP = flit;
        AgedPush(agedTXRSP, cacheLine.ageSNP, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendSNPChannelTXDAT0(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept
    {
        cacheLine.pendingSNPChannelTXDAT0 = flit;
        AgedPush(agedTXDAT, cacheLine.ageSNP, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendSNPChannelTXDAT1(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept
    {
        cacheLine.pendingSNPChannelTXDAT1 = flit;
        AgedPush(agedTXDAT, cacheLine.ageSNP, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendEVTChannelTXDAT0(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept
    {
        cacheLine.pendingEVTChannelTXDAT0 = flit;
        AgedPush(agedTXDAT, cacheLine.ageTXEVT, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::PendEVTChannelTXDAT1(CacheLine& cacheLine, const Flits::UpDAT<config>& flit) noexcept
    {
        cacheLine.pendingEVTChannelTXDAT1 = flit;
        AgedPush(agedTXDAT, cacheLine.ageTXEVT, LineKey(cacheLine.PA));
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::GrantedEvent>> UpstreamNode<config>::DoLoad(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        bool hazard = false;

        if (cacheLine)
        {
            if ((cacheLine->state == CacheState::Shared
              || cacheLine->state == CacheState::UniqueClean
              || cacheLine->state == CacheState::UniqueDirty)
             && !IsREQFilling(*cacheLine))
            {
                // TODO: event: LoadHitEvent

                // *NOTE: no OnCacheLineGranted here - the hub event carries the granting
                //        xaction and a hit has none (it never creates a REQ); the
                //        LoadHitEvent TODO above covers hit-path observability

                // Immediate hit
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::DONE, GrantedEvent(cacheLine));
            }

            if (IsREQInFlight(*cacheLine))
            {
                // REQ with same PA in-flight, cannot accept new request
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_PA_REQ_BUSY);
            }

            hazard = HasREQHazard(*cacheLine);
        }
        else
        {
            cacheLine = std::make_shared<CacheLine>(this, PA);
            SetCacheLine(cacheLine);
        }

        //
        if (CountInFlightREQ() >= xactionLimitREQ)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_REQ_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event: REQAllocationEvent (By DoLoad)

        //
        Flits::REQ<config> reqFlit;
        reqFlit.TxnID = *txnID;
        reqFlit.SrcID = nodeID;
        reqFlit.TgtID = sam->Map(PA);
        reqFlit.Opcode = Opcodes::REQ::ReadShared;
        reqFlit.Size = Sizes::B64;
        reqFlit.Addr = PA;
        reqFlit.NS = 0;
        reqFlit.Order = 0; // TODO: Order
        reqFlit.MemAttr = 0; // TODO: MemAttr
        reqFlit.Excl = 0;
        reqFlit.ExpCompData = 1;
        reqFlit.WayValid = 0;
        reqFlit.Way = 0;
        reqFlit.TraceTag = 0;

        if (events)
            events->OnREQPreHazardDetection(*this, *cacheLine, reqFlit, hazard);

        if (events)
            events->OnREQPostHazardDetection(*this, *cacheLine, reqFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreHazardPending(*this, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                cacheLine->pendingREQHazardTXREQ = { reqFlit };
                cacheLine->ageTXREQ = actionAge;
            }

            if (events)
                events->OnREQPostHazardPending(*this, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
            }
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreChannelPending(*this, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                PendREQChannelTXREQ(*cacheLine, reqFlit, actionAge);
            }

            if (events)
                events->OnREQPostChannelPending(*this, *cacheLine, reqFlit, denial);            

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
            }
        }

        // TODO: event LoadMissEvent

        std::shared_ptr<FutureNow<GrantedEvent>> future 
            = std::make_shared<FutureNow<GrantedEvent>>(Denial::ACCEPTED);

        cacheLine->activeREQFuture = future;
        return future;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::GrantedEvent>> UpstreamNode<config>::DoStore(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        bool hazard = false;

        if (cacheLine)
        {
            if ((cacheLine->state == CacheState::UniqueClean
              || cacheLine->state == CacheState::UniqueDirty)
             && !IsREQFilling(*cacheLine))
            {
                // TODO: event: StoreHitEvent

                // Immediate hit
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::DONE, GrantedEvent(cacheLine));
            }

            if (IsREQInFlight(*cacheLine))
            {
                // REQ with same PA in-flight, cannot accept new request
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_PA_REQ_BUSY);
            }

            hazard = HasREQHazard(*cacheLine);
        }
        else
        {
            cacheLine = std::make_shared<CacheLine>(this, PA);
            SetCacheLine(cacheLine);
        }

        //
        if (CountInFlightREQ() >= xactionLimitREQ)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_REQ_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event REQAllocationEvent (By DoStore)

        //
        Flits::REQ<config> reqFlit;
        reqFlit.TxnID = *txnID;
        reqFlit.SrcID = nodeID;
        reqFlit.TgtID = sam->Map(PA);
        reqFlit.Opcode = Opcodes::REQ::ReadUnique;
        reqFlit.Size = Sizes::B64;
        reqFlit.Addr = PA;
        reqFlit.NS = 0;
        reqFlit.Order = 0; // TODO: Order
        reqFlit.MemAttr = 0; // TODO: MemAttr
        reqFlit.Excl = 0;
        reqFlit.ExpCompData = 1;
        reqFlit.WayValid = 0;
        reqFlit.Way = 0;
        reqFlit.TraceTag = 0;
        // *NOTE: 'ExpCompData' must not be refreshed on releasing hazard for ReadUnique

        if (events)
            events->OnREQPreHazardDetection(*this, *cacheLine, reqFlit, hazard);

        if (events)
            events->OnREQPostHazardDetection(*this, *cacheLine, reqFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreHazardPending(*this, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                cacheLine->pendingREQHazardTXREQ = { reqFlit };
                cacheLine->ageTXREQ = actionAge;
            }

            if (events)
                events->OnREQPostHazardPending(*this, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
            }
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            reqFlit.ExpCompData = cacheLine->state == CacheState::Invalid ? 1 : 0;

            if (events)
                denial = events->OnREQPreChannelPending(*this, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                PendREQChannelTXREQ(*cacheLine, reqFlit, actionAge);
            }

            if (events)
                events->OnREQPostChannelPending(*this, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
            }
        }

        // TODO: event: StoreMissEvent

        std::shared_ptr<FutureNow<GrantedEvent>> future 
            = std::make_shared<FutureNow<GrantedEvent>>(Denial::ACCEPTED);

        cacheLine->activeREQFuture = future;
        return future;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::GrantedEvent>> UpstreamNode<config>::DoStoreLine(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        bool hazard = false;

        if (cacheLine)
        {
            if ((cacheLine->state == CacheState::UniqueClean
              || cacheLine->state == CacheState::UniqueDirty)
             && !IsREQFilling(*cacheLine))
            {
                // TODO: event: StoreHitEvent

                // Immediate hit
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::DONE, GrantedEvent(cacheLine));
            }

            if (IsREQInFlight(*cacheLine))
            {
                // REQ with same PA in-flight, cannot accept new request
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_PA_REQ_BUSY);
            }

            hazard = HasREQHazard(*cacheLine);
        }
        else
        {
            cacheLine = std::make_shared<CacheLine>(this, PA);
            SetCacheLine(cacheLine);
        }

        //
        if (CountInFlightREQ() >= xactionLimitREQ)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_REQ_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event REQAllocationEvent (By DoStoreAll)

        //
        Flits::REQ<config> reqFlit;
        reqFlit.TxnID = *txnID;
        reqFlit.SrcID = nodeID;
        reqFlit.TgtID = sam->Map(PA);
        reqFlit.Opcode = Opcodes::REQ::MakeUnique;
        reqFlit.Size = Sizes::B64;
        reqFlit.Addr = PA;
        reqFlit.NS = 0;
        reqFlit.Order = 0; // TODO: Order
        reqFlit.MemAttr = 0; // TODO: MemAttr
        reqFlit.Excl = 0;
        reqFlit.ExpCompData = 0;
        reqFlit.WayValid = 0;
        reqFlit.Way = 0;
        reqFlit.TraceTag = 0;

        if (events)
            events->OnREQPreHazardDetection(*this, *cacheLine, reqFlit, hazard);

        if (events)
            events->OnREQPostHazardDetection(*this, *cacheLine, reqFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreHazardPending(*this, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                cacheLine->pendingREQHazardTXREQ = { reqFlit };
                cacheLine->ageTXREQ = actionAge;
            }

            if (events)
                events->OnREQPostHazardPending(*this, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
            }
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreChannelPending(*this, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                PendREQChannelTXREQ(*cacheLine, reqFlit, actionAge);
            }

            if (events)
                events->OnREQPostChannelPending(*this, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
            }
        }

        // TODO: event: StoreMissEvent

        std::shared_ptr<FutureNow<GrantedEvent>> future 
            = std::make_shared<FutureNow<GrantedEvent>>(Denial::ACCEPTED);

        cacheLine->activeREQFuture = future;
        return future;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::EvictedEvent>> UpstreamNode<config>::DoEvict(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        if (!cacheLine)
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_EVICT_MISS);

        if (cacheLine->state == CacheState::Invalid)
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_EVICT_MISS);

        if (IsREQInFlight(*cacheLine))
        {
            // REQ with same PA in-flight, cannot accept new eviction;
            // otherwise the eviction could deadlock with the snoops that the
            // in-flight REQ's competitors are waiting for
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_PA_REQ_BUSY);
        }

        if (IsEVTInFlight(*cacheLine))
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_PA_EVT_BUSY);

        //
        if (CountInFlightEVT() >= xactionLimitEVT)
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_EVT_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event: EVTAllocationEvent

        //
        Flits::EVT<config> evtFlit;
        evtFlit.TxnID = *txnID;
        evtFlit.SrcID = nodeID;
        evtFlit.TgtID = sam->Map(PA);
        evtFlit.Opcode = cacheLine->state == CacheState::UniqueDirty ? Opcodes::EVT::WriteBackFull : Opcodes::EVT::Evict;
        evtFlit.Addr = PA;
        evtFlit.NS = 0;
        evtFlit.MemAttr = 0; // TODO: MemAttr
        evtFlit.WayValid = 0;
        evtFlit.Way = 0;
        evtFlit.TraceTag = 0;

        bool hazard = HasEVTHazard(*cacheLine);

        if (events)
            events->OnEVTPreHazardDetection(*this, *cacheLine, evtFlit, hazard);

        if (events)
            events->OnEVTPostHazardDetection(*this, *cacheLine, evtFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnEVTPreHazardPending(*this, *cacheLine, evtFlit).GetDenial();

            if (!denial->IsRejected())
            {
                cacheLine->pendingEVTHazardTXEVT = { evtFlit };
                cacheLine->ageTXEVT = actionAge;
            }

            if (events)
                events->OnEVTPostHazardPending(*this, *cacheLine, evtFlit, denial);

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<EvictedEvent>>(denial);
            }
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnEVTPreChannelPending(*this, *cacheLine, evtFlit).GetDenial();

            if (!denial->IsRejected())
            {
                PendEVTChannelTXEVT(*cacheLine, evtFlit, actionAge);

                CacheStateEnum nextState = CacheState::Invalid;

                if (events)
                    events->OnEVTCacheStatePreDemotion(*this, *cacheLine, *cacheLine->pendingEVTChannelTXEVT, cacheLine->state, nextState);

                if (events)
                    events->OnEVTCacheStatePostDemotion(*this, *cacheLine, *cacheLine->pendingEVTChannelTXEVT, cacheLine->state, nextState);

                // *NOTE: This also applicable for Write-Back, since EVT was always the highest priority transaction
                cacheLine->state = nextState;
            }

            if (events)
                events->OnEVTPostChannelPending(*this, *cacheLine, evtFlit, denial);

            if (denial->IsRejected())
            {
                FreeTxnID(*txnID);
                return std::make_shared<FutureNow<EvictedEvent>>(denial);
            }
        }

        // TODO: event: EvictHitEvent

        std::shared_ptr<FutureNow<EvictedEvent>> future 
            = std::make_shared<FutureNow<EvictedEvent>>(Denial::ACCEPTED);

        cacheLine->activeEVTFuture = future;
        return future;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::DoEvictSilently(uint64_t PA) noexcept
    {
        // TODO: Future support for silent eviction

        return false;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::PrefetchEmittedEvent>> UpstreamNode<config>::DoPrefetchLoad(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        //
        if (prefetchQueue.size() >= prefetchQueueLimit)
            return std::make_shared<FutureNow<PrefetchEmittedEvent>>(Denial::REJECTED_TAURUS_PREFETCH_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<PrefetchEmittedEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event: REQAllocationEvent (By DoPrefetchLoad)

        Flits::REQ<config> flit;
        flit.TxnID = *txnID;
        flit.SrcID = nodeID;
        flit.TgtID = sam->Map(PA);
        flit.Opcode = Opcodes::REQ::StashShared;
        flit.Size = Sizes::B64;
        flit.Addr = PA;
        flit.NS = 0;
        flit.Order = 0;
        flit.MemAttr = 0;
        flit.Excl = 0;
        flit.ExpCompStash = 0;
        flit.WayValid = 0;
        flit.Way = 0;
        flit.TraceTag = 0;

        // TODO: event: PrefetchPreQueueEvent

        std::shared_ptr<FutureNow<PrefetchEmittedEvent>> future 
            = std::make_shared<FutureNow<PrefetchEmittedEvent>>(Denial::ACCEPTED);

        prefetchQueue.emplace_back(flit, future);
        prefetchQueue.back().age = actionAge;
        
        // TODO: event: PrefetchPostQueueEvent

        return future;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::PrefetchEmittedEvent>> UpstreamNode<config>::DoPrefetchStore(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        //
        if (prefetchQueue.size() >= prefetchQueueLimit)
            return std::make_shared<FutureNow<PrefetchEmittedEvent>>(Denial::REJECTED_TAURUS_PREFETCH_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<PrefetchEmittedEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event: REQAllocationEvent (By DoPrefetchStore)

        Flits::REQ<config> flit;
        flit.TxnID = *txnID;
        flit.SrcID = nodeID;
        flit.TgtID = sam->Map(PA);
        flit.Opcode = Opcodes::REQ::StashUnique;
        flit.Size = Sizes::B64;
        flit.Addr = PA;
        flit.NS = 0;
        flit.Order = 0;
        flit.MemAttr = 0;
        flit.Excl = 0;
        flit.ExpCompStash = 0;
        flit.WayValid = 0;
        flit.Way = 0;
        flit.TraceTag = 0;

        // TODO: event: PrefetchPreQueueEvent

        std::shared_ptr<FutureNow<PrefetchEmittedEvent>> future 
            = std::make_shared<FutureNow<PrefetchEmittedEvent>>(Denial::ACCEPTED);

        prefetchQueue.emplace_back(flit, future);
        prefetchQueue.back().age = actionAge;
        
        // TODO: event: PrefetchPostQueueEvent

        return future;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::ReadEvent>> UpstreamNode<config>::DoLoadThrough(uint64_t PA) noexcept
    {
        // TODO: not supported yet

        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::CompleteEvent>> UpstreamNode<config>::DoStoreThrough(uint64_t PA, const uint64_t data[8]) noexcept
    {
        // TODO: not supported yet

        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::ReadEvent>> UpstreamNode<config>::DoNonCacheableRead(uint64_t PA) noexcept
    {
        // TODO: not supported yet

        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::CompleteEvent>> UpstreamNode<config>::DoNonCacheableWrite(uint64_t PA, const uint64_t data[8]) noexcept
    {
        // TODO: not supported yet

        return nullptr;
    }
    
    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::CMOCompleteEvent>> UpstreamNode<config>::DoCBOClean(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        //
        if (cmoQueue.size() + cmoTracker.size() >= cmoQueueLimit)
            return std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::REJECTED_TAURUS_CMO_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event: REQAllocationEvent (By DoCBOClean)

        Flits::REQ<config> flit;
        flit.TxnID = *txnID;
        flit.SrcID = nodeID;
        flit.TgtID = sam->Map(PA);
        flit.Opcode = Opcodes::REQ::CleanShared;
        flit.Size = Sizes::B64;
        flit.Addr = PA;
        flit.NS = 0;
        flit.Order = 0;
        flit.MemAttr = 0;
        flit.Excl = 0;
        flit.ExpCompData = 0;
        flit.WayValid = 0;
        flit.Way = 0;
        flit.TraceTag = 0;

        // TODO: event: CMOPreQueueEvent

        std::shared_ptr<FutureNow<CMOCompleteEvent>> future 
            = std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::ACCEPTED);

        cmoQueue.emplace_back(flit, future);
        cmoQueue.back().age = actionAge;

        // TODO: event: CMOPostQueueEvent

        return future;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::CMOCompleteEvent>> UpstreamNode<config>::DoCBOFlush(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        //
        if (cmoQueue.size() + cmoTracker.size() >= cmoQueueLimit)
            return std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::REJECTED_TAURUS_CMO_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event: REQAllocationEvent (By DoCBOFlush)

        Flits::REQ<config> flit;
        flit.TxnID = *txnID;
        flit.SrcID = nodeID;
        flit.TgtID = sam->Map(PA);
        flit.Opcode = Opcodes::REQ::CleanInvalid;
        flit.Size = Sizes::B64;
        flit.Addr = PA;
        flit.NS = 0;
        flit.Order = 0;
        flit.MemAttr = 0;
        flit.Excl = 0;
        flit.ExpCompData = 0;
        flit.WayValid = 0;
        flit.Way = 0;
        flit.TraceTag = 0;

        // TODO: event: CMOPreQueueEvent

        std::shared_ptr<FutureNow<CMOCompleteEvent>> future 
            = std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::ACCEPTED);

        cmoQueue.emplace_back(flit, future);
        cmoQueue.back().age = actionAge;

        // TODO: event: CMOPostQueueEvent

        return future;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::CMOCompleteEvent>> UpstreamNode<config>::DoCBOInval(uint64_t PA) noexcept
    {
        PA = LineBase(PA);

        //
        if (cmoQueue.size() + cmoTracker.size() >= cmoQueueLimit)
            return std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::REJECTED_TAURUS_CMO_LIMIT_EXCEEDED);

        //
        auto txnID = AllocateTxnID();
        const uint64_t actionAge = NextEmissionAge(); // action-start age, orders all its flits

        if (!txnID)
            return std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED);

        // TODO: event: REQAllocationEvent (By DoCBOInval)

        Flits::REQ<config> flit;
        flit.TxnID = *txnID;
        flit.SrcID = nodeID;
        flit.TgtID = sam->Map(PA);
        flit.Opcode = Opcodes::REQ::MakeInvalid;
        flit.Size = Sizes::B64;
        flit.Addr = PA;
        flit.NS = 0;
        flit.Order = 0;
        flit.MemAttr = 0;
        flit.Excl = 0;
        flit.ExpCompData = 0;
        flit.WayValid = 0;
        flit.Way = 0;
        flit.TraceTag = 0;

        // TODO: event: CMOPreQueueEvent

        std::shared_ptr<FutureNow<CMOCompleteEvent>> future 
            = std::make_shared<FutureNow<CMOCompleteEvent>>(Denial::ACCEPTED);

        cmoQueue.emplace_back(flit, future);
        cmoQueue.back().age = actionAge;

        // TODO: event: CMOPostQueueEvent

        return future;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::Tick(uint64_t time) noexcept
    {
        this->time = time;

        TickEVT();
        TickSNP();
        TickREQ();

        for (auto it = cacheable.begin(); it != cacheable.end();)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.state == CacheState::Invalid
             && !cacheLine.IsEVTInFlight(glbl)
             && !cacheLine.IsSNPInFlight(glbl)
             && !cacheLine.IsREQInFlight(glbl))
            {
                it = cacheable.erase(it);
            }
            else
                ++it;
        }
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::TickEVT() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingEVTHazardTXEVT)
            {
                bool hazard = HasEVTHazard(cacheLine);

                if (events)
                    events->OnEVTPreHazardDetection(*this, cacheLine, *cacheLine.pendingEVTHazardTXEVT, hazard);

                if (events)
                    events->OnEVTPostHazardDetection(*this, cacheLine, *cacheLine.pendingEVTHazardTXEVT, hazard);

                if (hazard)
                    continue;

                if (events)
                    events->OnEVTPreHazardToChannelPending(*this, cacheLine, *cacheLine.pendingEVTHazardTXEVT);

                PendEVTChannelTXEVT(cacheLine, *cacheLine.pendingEVTHazardTXEVT, cacheLine.ageTXEVT);
                cacheLine.pendingEVTHazardTXEVT.reset();

                if (events)
                    events->OnEVTPostHazardToChannelPending(*this, cacheLine, *cacheLine.pendingEVTChannelTXEVT);

                CacheStateEnum nextState = CacheState::Invalid;

                if (events)
                    events->OnEVTCacheStatePreDemotion(*this, cacheLine, *cacheLine.pendingEVTChannelTXEVT, cacheLine.state, nextState);

                if (events)
                    events->OnEVTCacheStatePostDemotion(*this, cacheLine, *cacheLine.pendingEVTChannelTXEVT, cacheLine.state, nextState);

                // *NOTE: This also applicable for Write-Back, since EVT was always the highest priority transaction
                cacheLine.state = nextState;
            }
            else if (cacheLine.activeEVT)
            {
                if (cacheLine.activeEVT->GetType() == Xact::XactionType::Evict)
                {
                    Xact::XactionEvict<config>& xactionEvict 
                        = static_cast<Xact::XactionEvict<config>&>(*cacheLine.activeEVT);
                    
                    if (xactionEvict.IsComplete(glbl) && cacheLine.activeEVTFuture && !cacheLine.activeEVTFuture->Fired())
                    {
                        // TODO: event: EvictCompleteEvent

                        cacheLine.activeEVTFuture->Fire(EvictedEvent(it->second));
                    }
                }
                else if (cacheLine.activeEVT->GetType() == Xact::XactionType::WriteBack)
                {
                    Xact::XactionWriteBack<config>& xactionWriteBack 
                        = static_cast<Xact::XactionWriteBack<config>&>(*cacheLine.activeEVT);

                    if (xactionWriteBack.GotDBIDResp())
                    {
                        bool justGotDBIDResp0 = !xactionWriteBack.GotCopyBackWrData(0);
                        bool justGotDBIDResp1 = !xactionWriteBack.GotCopyBackWrData(1);

                        if (cacheLine.pendingEVTHazardTXDAT0)
                        {
                            justGotDBIDResp0 = false;

                            bool hazard = HasEVTDataHazard(cacheLine, 0);

                            if (events)
                                events->OnEVTDataPreHazardDetection(*this, cacheLine, *cacheLine.pendingEVTHazardTXDAT0, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, cacheLine, *cacheLine.pendingEVTHazardTXDAT0, hazard);

                            if (!hazard)
                            {
                                if (events)
                                    events->OnEVTDataPreHazardToChannelPending(*this, cacheLine, *cacheLine.pendingEVTHazardTXDAT0);

                                PendEVTChannelTXDAT0(cacheLine, *cacheLine.pendingEVTHazardTXDAT0);
                                cacheLine.pendingEVTHazardTXDAT0.reset();

                                if (events)
                                    events->OnEVTDataPostHazardToChannelPending(*this, cacheLine, *cacheLine.pendingEVTChannelTXDAT0);
                            }
                        }
                        else if (cacheLine.pendingEVTChannelTXDAT0)
                        {
                            justGotDBIDResp0 = false;
                        }

                        if (cacheLine.pendingEVTHazardTXDAT1)
                        {
                            justGotDBIDResp1 = false;

                            bool hazard = HasEVTDataHazard(cacheLine, 1);

                            if (events)
                                events->OnEVTDataPreHazardDetection(*this, cacheLine, *cacheLine.pendingEVTHazardTXDAT1, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, cacheLine, *cacheLine.pendingEVTHazardTXDAT1, hazard);

                            if (!hazard)
                            {
                                if (events)
                                    events->OnEVTDataPreHazardToChannelPending(*this, cacheLine, *cacheLine.pendingEVTHazardTXDAT1);

                                PendEVTChannelTXDAT1(cacheLine, *cacheLine.pendingEVTHazardTXDAT1);
                                cacheLine.pendingEVTHazardTXDAT1.reset();

                                if (events)
                                    events->OnEVTDataPostHazardToChannelPending(*this, cacheLine, *cacheLine.pendingEVTChannelTXDAT1);
                            }
                        }
                        else if (cacheLine.pendingEVTChannelTXDAT1)
                        {
                            justGotDBIDResp1 = false;
                        }
                        
                        if (justGotDBIDResp0)
                        {
                            Flits::UpDAT<config> datFlit;
                            datFlit.TxnID = *xactionWriteBack.GetDBID();
                            datFlit.SrcID = nodeID;
                            datFlit.TgtID = xactionWriteBack.GetDBIDSource()->flit.dnrsp.SrcID;
                            datFlit.Opcode = Opcodes::UpDAT::CopyBackWrData;
                            datFlit.RespErr = 0;
                            datFlit.Resp = Resps::I_PD;
                            datFlit.BE = 0xFFFFFFFF;
                            datFlit.TraceTag = 0;

                            datFlit.DataID = 0;
                            datFlit.Data[0] = cacheLine.data[0];
                            datFlit.Data[1] = cacheLine.data[1];
                            datFlit.Data[2] = cacheLine.data[2];
                            datFlit.Data[3] = cacheLine.data[3];

                            bool hazard = HasEVTDataHazard(cacheLine, 0);

                            if (events)
                                events->OnEVTDataPreHazardDetection(*this, cacheLine, datFlit, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, cacheLine, datFlit, hazard);

                            if (hazard)
                            {
                                if (events)
                                    events->OnEVTDataPreHazardPending(*this, cacheLine, datFlit);

                                cacheLine.pendingEVTHazardTXDAT0 = datFlit;

                                if (events)
                                    events->OnEVTDataPostHazardPending(*this, cacheLine, datFlit);
                            }
                            else
                            {
                                if (events)
                                    events->OnEVTDataPreChannelPending(*this, cacheLine, datFlit);

                                PendEVTChannelTXDAT0(cacheLine, datFlit);

                                if (events)
                                    events->OnEVTDataPostChannelPending(*this, cacheLine, datFlit);
                            }
                        }

                        if (justGotDBIDResp1)
                        {
                            Flits::UpDAT<config> datFlit;
                            datFlit.TxnID = *xactionWriteBack.GetDBID();
                            datFlit.SrcID = nodeID;
                            datFlit.TgtID = xactionWriteBack.GetDBIDSource()->flit.dnrsp.SrcID;
                            datFlit.Opcode = Opcodes::UpDAT::CopyBackWrData;
                            datFlit.RespErr = 0;
                            datFlit.Resp = Resps::I_PD;
                            datFlit.BE = 0xFFFFFFFF;
                            datFlit.TraceTag = 0;

                            datFlit.DataID = 1;
                            datFlit.Data[0] = cacheLine.data[4];
                            datFlit.Data[1] = cacheLine.data[5];
                            datFlit.Data[2] = cacheLine.data[6];
                            datFlit.Data[3] = cacheLine.data[7];

                            bool hazard = HasEVTDataHazard(cacheLine, 1);

                            if (events)
                                events->OnEVTDataPreHazardDetection(*this, cacheLine, datFlit, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, cacheLine, datFlit, hazard);

                            if (hazard)
                            {
                                if (events)
                                    events->OnEVTDataPreHazardPending(*this, cacheLine, datFlit);

                                cacheLine.pendingEVTHazardTXDAT1 = datFlit;

                                if (events)
                                    events->OnEVTDataPostHazardPending(*this, cacheLine, datFlit);
                            }
                            else
                            {
                                if (events)
                                    events->OnEVTDataPreChannelPending(*this, cacheLine, datFlit);

                                PendEVTChannelTXDAT1(cacheLine, datFlit);

                                if (events)
                                    events->OnEVTDataPostChannelPending(*this, cacheLine, datFlit);
                            }
                        }
                    }

                    if (xactionWriteBack.IsComplete(glbl) && cacheLine.activeEVTFuture && !cacheLine.activeEVTFuture->Fired())
                    {
                        // TODO: event: EvictCompleteEvent

                        cacheLine.activeEVTFuture->Fire(EvictedEvent(it->second));
                    }
                }
            }

            // release the TxnID of a fully completed EVT transaction
            if (cacheLine.activeEVT && cacheLine.activeEVT->IsComplete(glbl))
            {
                FreeTxnID(cacheLine.activeEVT->GetFirst().flit.evt.TxnID);
                cacheLine.activeEVT.reset();

                // TODO: event: EVTDeallocationEvent
            }
        }
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::TickSNP() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingSNPHazardRXSNP)
            {
                bool hazard = HasSNPHazard(cacheLine);

                if (events)
                    events->OnSNPPreHazardDetection(*this, cacheLine, *cacheLine.pendingSNPHazardRXSNP, hazard);
                
                if (events)
                    events->OnSNPPostHazardDetection(*this, cacheLine, *cacheLine.pendingSNPHazardRXSNP, hazard);

                if (hazard)
                    continue;

                auto& flit = *cacheLine.pendingSNPHazardRXSNP;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextSNP(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedSNP(*this, cacheLine, xaction, flit);
                }
                else 
                {
                    if (events)
                        events->OnDeniedSNP(*this, cacheLine, denial, xaction, flit);
                }

                // TODO: event SNPPreHazardToActiveEvent

                cacheLine.activeSNP = xaction;
                cacheLine.pendingSNPHazardRXSNP.reset();

                // TODO: event SNPPostHazardToActiveEvent
            }
            else if (cacheLine.activeSNP)
            {
                if (cacheLine.activeSNP->GetType() == Xact::XactionType::Snoop)
                {
                    Xact::XactionSnoop<config>& xactionSnoop
                        = static_cast<Xact::XactionSnoop<config>&>(*cacheLine.activeSNP);

                    if (xactionSnoop.GotAnyResp()
                     || cacheLine.pendingSNPChannelTXRSP
                     || cacheLine.pendingSNPChannelTXDAT0
                     || cacheLine.pendingSNPChannelTXDAT1)
                    {
                        continue;
                    }

                    // *NOTE: An EVT is always handled first, since EVT has the highest
                    //        priority. When a SNP and an EVT left their queues in the
                    //        same Tick, this SNP is already active but unprocessed
                    //        while the EVT is pending emission or in flight before
                    //        its first Comp/CompDBIDResp. The SNP's state change and
                    //        response must wait for that first response:
                    //
                    //        time = 5 (between Tick(5) and Tick(6)):
                    //            PushRXSNP(SnpToShared)      // activates (activeSNP set),
                    //                                        // but not processed yet
                    //            DoEvict(pa)                 // channel-pends Evict,
                    //                                        // demotes state to Invalid
                    //            PopTXEVT()                  // Evict on the wire (activeEVT)
                    //        Tick(6):
                    //            TickSNP, here:              // gated - no state change,
                    //                                        // no SnpResp while awaiting Comp
                    //            PushRXRSP(Comp)             // the EVT's first response
                    //        Tick(7):
                    //            TickSNP, here:              // gate lifts - SnpResp I pended
                    //                                        // from the post-eviction state
                    //
                    //        This cannot deadlock: the gate only bites when the SNP and
                    //        the EVT left in the same Tick, and the EVT never waits for
                    //        the SNP (HasEVTHazard does not consult SNP state).
                    if (cacheLine.pendingEVTChannelTXEVT
                     || (cacheLine.activeEVT && !cacheLine.GotEVTComp(glbl)))
                        continue;

                    Flits::SNP<config> flit = xactionSnoop.GetFirst().flit.snp;

                    // state transition and snoop response combinations
                    bool retToSrc;
                    CacheStateEnum state;
                    RespEnum resp;

                    switch (flit.Opcode)
                    {
                        case Opcodes::SNP::SnpMakeInvalid:

                            switch (*cacheLine.state)
                            {
                                case CacheState::Invalid:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::Shared:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::UniqueClean:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::UniqueDirty:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;
                            }

                            break;

                        case Opcodes::SNP::SnpToInvalid:

                            switch (*cacheLine.state)
                            {
                                case CacheState::Invalid:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::Shared:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::UniqueClean:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::UniqueDirty:
                                    retToSrc = true;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I_PD;
                                    break;
                            }

                            break;

                        case Opcodes::SNP::SnpToShared:
                        
                            switch (*cacheLine.state)
                            {
                                case CacheState::Invalid:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::Shared:
                                    retToSrc = false;
                                    state = CacheState::Shared;
                                    resp = Resps::Enum::SC;
                                    break;

                                case CacheState::UniqueClean:
                                    retToSrc = false;
                                    state = CacheState::Shared;
                                    resp = Resps::Enum::SC;
                                    break;

                                case CacheState::UniqueDirty:
                                    retToSrc = true;
                                    state = CacheState::Shared;
                                    resp = Resps::Enum::SC_PD;
                                    break;
                            }

                            break;

                        case Opcodes::SNP::SnpToClean:

                            switch (*cacheLine.state)
                            {
                                case CacheState::Invalid:
                                    retToSrc = false;
                                    state = CacheState::Invalid;
                                    resp = Resps::Enum::I;
                                    break;

                                case CacheState::Shared:
                                    retToSrc = false;
                                    state = CacheState::Shared;
                                    resp = Resps::Enum::SC;
                                    break;

                                case CacheState::UniqueClean:
                                    retToSrc = false;
                                    state = CacheState::UniqueClean;
                                    resp = Resps::Enum::UC;
                                    break;

                                case CacheState::UniqueDirty:
                                    retToSrc = true;
                                    state = CacheState::UniqueClean;
                                    resp = Resps::Enum::UC_PD;
                                    break;
                            }
                        
                            break;

                        default:
                            // should not reach here
                            continue;
                    }

                    if (events)
                        events->OnSNPCacheStatePreDemotion(*this, cacheLine, flit, cacheLine.state, state);

                    if (events)
                        events->OnSNPCacheStatePostDemotion(*this, cacheLine, flit, cacheLine.state, state);

                    // TODO: The cache state demotion event of SNPs might should be associated with the final Resp and RetToSrc

                    if (retToSrc)
                    {
                        Flits::UpDAT<config> datFlit;
                        datFlit.TxnID = flit.TxnID;
                        datFlit.SrcID = nodeID;
                        datFlit.TgtID = flit.SrcID;
                        datFlit.Opcode = Opcodes::UpDAT::SnpRespData;
                        datFlit.RespErr = 0;
                        datFlit.Resp = resp->value;
                        datFlit.BE = 0xFFFFFFFF;
                        datFlit.TraceTag = 0;

                        datFlit.DataID = 0;
                        datFlit.Data[0] = cacheLine.data[0];
                        datFlit.Data[1] = cacheLine.data[1];
                        datFlit.Data[2] = cacheLine.data[2];
                        datFlit.Data[3] = cacheLine.data[3];

                        if (events)
                            events->OnSNPRespDataPreChannelPending(*this, cacheLine, datFlit);

                        PendSNPChannelTXDAT0(cacheLine, datFlit);

                        if (events)
                            events->OnSNPRespDataPostChannelPending(*this, cacheLine, datFlit);

                        datFlit.DataID = 1;
                        datFlit.Data[0] = cacheLine.data[4];
                        datFlit.Data[1] = cacheLine.data[5];
                        datFlit.Data[2] = cacheLine.data[6];
                        datFlit.Data[3] = cacheLine.data[7];

                        if (events)
                            events->OnSNPRespDataPreChannelPending(*this, cacheLine, datFlit);

                        PendSNPChannelTXDAT1(cacheLine, datFlit);

                        if (events)
                            events->OnSNPRespDataPostChannelPending(*this, cacheLine, datFlit);
                    }
                    else
                    {
                        Flits::UpRSP<config> rspFlit;
                        rspFlit.TxnID = flit.TxnID;
                        rspFlit.SrcID = nodeID;
                        rspFlit.TgtID = flit.SrcID;
                        rspFlit.Opcode = Opcodes::UpRSP::SnpResp;
                        rspFlit.RespErr = 0;
                        rspFlit.Resp = resp->value;
                        rspFlit.TraceTag = 0;

                        if (events)
                            events->OnSNPRespPreChannelPending(*this, cacheLine, rspFlit);

                        PendSNPChannelTXRSP(cacheLine, rspFlit);

                        if (events)
                            events->OnSNPRespPostChannelPending(*this, cacheLine, rspFlit);
                    }

                    cacheLine.state = state;
                }
            }
        }
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::TickREQ() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingREQHazardTXREQ)
            {
                bool hazard = HasREQHazard(cacheLine);

                if (events)
                    events->OnREQPreHazardDetection(*this, cacheLine, *cacheLine.pendingREQHazardTXREQ, hazard);

                if (events)
                    events->OnREQPostHazardDetection(*this, cacheLine, *cacheLine.pendingREQHazardTXREQ, hazard);

                if (hazard)
                    continue;

                if (events)
                    events->OnREQPreHazardToChannelPending(*this, cacheLine, *cacheLine.pendingREQHazardTXREQ);

                PendREQChannelTXREQ(cacheLine, *cacheLine.pendingREQHazardTXREQ, cacheLine.ageTXREQ);
                cacheLine.pendingREQHazardTXREQ.reset();

                if (events)
                    events->OnREQPostHazardToChannelPending(*this, cacheLine, *cacheLine.pendingREQChannelTXREQ);
            }
            else if (cacheLine.activeREQ)
            {
                if (cacheLine.activeREQ->GetType() == Xact::XactionType::CacheableDataless)
                {
                    Xact::XactionCacheableDataless<config>& xactionCacheableDataless 
                        = static_cast<Xact::XactionCacheableDataless<config>&>(*cacheLine.activeREQ);

                    if (xactionCacheableDataless.GotComp() 
                     && cacheLine.activeREQFuture 
                     && !cacheLine.activeREQFuture->Fired())
                    {
                        if (events)
                            events->OnCacheLineGranted(*this, cacheLine, cacheLine.activeREQ);

                        cacheLine.activeREQFuture->Fire(GrantedEvent(it->second));
                    }
                    
                    if (xactionCacheableDataless.GotComp() 
                     && !xactionCacheableDataless.GotCompAck() 
                     && !cacheLine.pendingREQChannelTXRSP)
                    {
                        Flits::UpRSP<config> rspFlit;
                        rspFlit.TxnID = *xactionCacheableDataless.GetDBID();
                        rspFlit.SrcID = nodeID;
                        rspFlit.TgtID = xactionCacheableDataless.GetDBIDSource()->flit.dnrsp.SrcID;
                        rspFlit.Opcode = Opcodes::UpRSP::CompAck;
                        rspFlit.RespErr = 0;
                        rspFlit.Resp = 0;
                        rspFlit.TraceTag = 0;

                        if (events)
                            events->OnREQCompAckPreChannelPending(*this, cacheLine, rspFlit);

                        PendREQChannelTXRSP(cacheLine, rspFlit);

                        if (events)
                            events->OnREQCompAckPostChannelPending(*this, cacheLine, rspFlit);
                    }
                }
                else if (cacheLine.activeREQ->GetType() == Xact::XactionType::CacheableAllocatingRead)
                {
                    Xact::XactionCacheableAllocatingRead<config>& xactionCacheableAllocatingRead
                        = static_cast<Xact::XactionCacheableAllocatingRead<config>&>(*cacheLine.activeREQ);

                    if ((xactionCacheableAllocatingRead.GotComp() || xactionCacheableAllocatingRead.GotAllCompData())
                     && cacheLine.activeREQFuture 
                     && !cacheLine.activeREQFuture->Fired())
                    {
                        if (events)
                            events->OnCacheLineGranted(*this, cacheLine, cacheLine.activeREQ);

                        cacheLine.activeREQFuture->Fire(GrantedEvent(it->second));
                    }

                    if ((xactionCacheableAllocatingRead.GotComp() || xactionCacheableAllocatingRead.GotAnyCompData())
                     && !xactionCacheableAllocatingRead.GotCompAck()
                     && !cacheLine.pendingREQChannelTXRSP)
                    {
                        const Xact::FiredResponseFlit<config>* dbidSource = xactionCacheableAllocatingRead.GetDBIDSource();

                        Flits::UpRSP<config> rspFlit;
                        rspFlit.TxnID = *xactionCacheableAllocatingRead.GetDBID();
                        rspFlit.SrcID = nodeID;
                        rspFlit.TgtID = dbidSource->IsDnDAT() ? dbidSource->flit.dndat.SrcID : dbidSource->flit.dnrsp.SrcID;
                        rspFlit.Opcode = Opcodes::UpRSP::CompAck;
                        rspFlit.RespErr = 0;
                        rspFlit.Resp = 0;
                        rspFlit.TraceTag = 0;

                        if (events)
                            events->OnREQCompAckPreChannelPending(*this, cacheLine, rspFlit);

                        PendREQChannelTXRSP(cacheLine, rspFlit);

                        if (events)
                            events->OnREQCompAckPostChannelPending(*this, cacheLine, rspFlit);
                    }
                }
            }

            // release the TxnID of a fully completed REQ transaction
            if (cacheLine.activeREQ && cacheLine.activeREQ->IsComplete(glbl))
            {
                FreeTxnID(cacheLine.activeREQ->GetFirst().flit.req.TxnID);
                cacheLine.activeREQ.reset();

                // TODO: event: REQDeallocationEvent
            }
        }
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::EVT<config>> UpstreamNode<config>::PeekTXEVT() noexcept
    {
        // single-slot channel: exactly one deque entry per line, so a cancelled
        // candidate can never be re-offered through a second entry of the same
        // line - a plain continue suffices (no skip set needed)
        for (size_t i = 0; i < agedTXEVT.size(); ++i)
        {
            auto [age, key] = agedTXEVT[i];

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || !lineIt->second->pendingEVTChannelTXEVT
             || lineIt->second->ageTXEVT != age)
            {
                agedTXEVT.erase(agedTXEVT.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            if (events)
                if (events->OnEVTPreChannelChosen(*this, cacheLine, *cacheLine.pendingEVTChannelTXEVT).IsCancelled())
                    continue;

            if (events)
                events->OnEVTPostChannelChosen(*this, cacheLine, *cacheLine.pendingEVTChannelTXEVT);

            return { *cacheLine.pendingEVTChannelTXEVT };
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::EVT<config>> UpstreamNode<config>::PopTXEVT() noexcept
    {
        // single-slot channel: exactly one deque entry per line, so a cancelled
        // candidate can never be re-offered through a second entry of the same
        // line - a plain continue suffices (no skip set needed)
        for (size_t i = 0; i < agedTXEVT.size(); ++i)
        {
            auto [age, key] = agedTXEVT[i];

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || !lineIt->second->pendingEVTChannelTXEVT
             || lineIt->second->ageTXEVT != age)
            {
                agedTXEVT.erase(agedTXEVT.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            auto& flit = *cacheLine.pendingEVTChannelTXEVT;

            if (events)
                if (events->OnEVTPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    continue;

            if (events)
                events->OnEVTPostChannelChosen(*this, cacheLine, flit);

            std::shared_ptr<Xact::Xaction<config>> xaction;
            XactDenialEnum denial = joint.NextEVT(glbl, time, flit, &xaction);

            if (denial == XactDenial::ACCEPTED)
            {
                if (events)
                    events->OnAcceptedEVT(*this, cacheLine, xaction, flit);
            }
            else
            {
                if (events)
                    events->OnDeniedEVT(*this, cacheLine, denial, xaction, flit);

                FreeTxnID(flit.TxnID);
            }

            // a completed predecessor still holds its bitmap TxnID: completion
            // needs no Tick (the joint completes on the last copyback-beat pop)
            // but the TickEVT trailing block is the only other free site, and it
            // can never see the predecessor once overwritten here
            if (cacheLine.activeEVT && cacheLine.activeEVT->IsComplete(glbl))
                FreeTxnID(cacheLine.activeEVT->GetFirst().flit.evt.TxnID);

            cacheLine.activeEVT = xaction;
            cacheLine.pendingEVTChannelTXEVT.reset();
            AgedErase(agedTXEVT, cacheLine.ageTXEVT, key);

            return { flit };
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::REQ<config>> UpstreamNode<config>::PeekTXREQ() noexcept
    {
        // TXREQ merges the three action classes into one age order: per-line REQs
        // from agedTXREQ, prefetches and CMOs from their queue fronts (FIFO push
        // order = call order, so the front is the class's oldest) - an O(1)
        // three-way min, re-evaluated at every iteration: a cancelled per-line
        // REQ yields to an older prefetch/CMO next (rule 3)
        for (size_t i = 0; i < agedTXREQ.size(); ++i)
        {
            const bool queuePrefetchWins = !prefetchQueue.empty()
                                        && (cmoQueue.empty() || prefetchQueue.front().age < cmoQueue.front().age);
            const bool queueCMOWins      = !cmoQueue.empty() && !queuePrefetchWins;
            const uint64_t queueAge      = queuePrefetchWins ? prefetchQueue.front().age
                                         : (queueCMOWins ? cmoQueue.front().age : 0);

            auto [age, key] = agedTXREQ[i];

            // the queue candidate is older than this line candidate: the per-line
            // walk is done, the queue branch dispatches after the loop
            if ((queuePrefetchWins || queueCMOWins) && queueAge < age)
                break;

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || !lineIt->second->pendingREQChannelTXREQ
             || lineIt->second->ageTXREQ != age)
            {
                agedTXREQ.erase(agedTXREQ.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            // single-slot channel: exactly one deque entry per line, so a
            // cancelled candidate can never be re-offered through a second entry
            // of the same line - a plain continue suffices (no skip set needed)
            if (events)
                if (events->OnREQPreChannelChosen(*this, cacheLine, *cacheLine.pendingREQChannelTXREQ).IsCancelled())
                    continue;

            if (events)
                events->OnREQPostChannelChosen(*this, cacheLine, *cacheLine.pendingREQChannelTXREQ);

            return { *cacheLine.pendingREQChannelTXREQ };
        }

        // a remaining queue candidate dispatches here: it is older than every
        // per-line candidate, or no per-line candidate survived the walk
        const bool queuePrefetchWins = !prefetchQueue.empty()
                                    && (cmoQueue.empty() || prefetchQueue.front().age < cmoQueue.front().age);
        const bool queueCMOWins      = !cmoQueue.empty() && !queuePrefetchWins;

        if (queuePrefetchWins)
        {
            // TODO: event: PrefetchPreChannelChosenEvent

            if (true) // result of event
            {
                // TODO: event: PrefetchPostChannelChosenEvent

                return { prefetchQueue.front().GetPrefetchFlit() };
            }
        }

        if (queueCMOWins)
        {
            // TODO: event: CMOPreChannelChosenEvent

            if (true) // result of event
            {
                // TODO: event: CMOPostChannelChosenEvent

                return { cmoQueue.front().GetCMOFlit() };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::REQ<config>> UpstreamNode<config>::PopTXREQ() noexcept
    {
        // TXREQ merges the three action classes into one age order: per-line REQs
        // from agedTXREQ, prefetches and CMOs from their queue fronts (FIFO push
        // order = call order, so the front is the class's oldest) - an O(1)
        // three-way min, re-evaluated at every iteration: a cancelled per-line
        // REQ yields to an older prefetch/CMO next (rule 3)
        for (size_t i = 0; i < agedTXREQ.size(); ++i)
        {
            const bool queuePrefetchWins = !prefetchQueue.empty()
                                        && (cmoQueue.empty() || prefetchQueue.front().age < cmoQueue.front().age);
            const bool queueCMOWins      = !cmoQueue.empty() && !queuePrefetchWins;
            const uint64_t queueAge      = queuePrefetchWins ? prefetchQueue.front().age
                                         : (queueCMOWins ? cmoQueue.front().age : 0);

            auto [age, key] = agedTXREQ[i];

            // the queue candidate is older than this line candidate: the per-line
            // walk is done, the queue branch dispatches after the loop
            if ((queuePrefetchWins || queueCMOWins) && queueAge < age)
                break;

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || !lineIt->second->pendingREQChannelTXREQ
             || lineIt->second->ageTXREQ != age)
            {
                agedTXREQ.erase(agedTXREQ.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            auto& flit = *cacheLine.pendingREQChannelTXREQ;

            // single-slot channel: exactly one deque entry per line, so a
            // cancelled candidate can never be re-offered through a second entry
            // of the same line - a plain continue suffices (no skip set needed)
            if (events)
                if (events->OnREQPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    continue;

            if (events)
                events->OnREQPostChannelChosen(*this, cacheLine, flit);

            std::shared_ptr<Xact::Xaction<config>> xaction;
            XactDenialEnum denial = joint.NextREQ(glbl, time, flit, &xaction);

            if (denial == XactDenial::ACCEPTED)
            {
                if (events)
                    events->OnAcceptedREQ(*this, cacheLine, xaction, flit);
            }
            else
            {
                if (events)
                    events->OnDeniedREQ(*this, cacheLine, denial, xaction, flit);

                FreeTxnID(flit.TxnID);
            }

            // a completed predecessor still holds its bitmap TxnID: completion
            // needs no Tick (the joint completes on the CompAck pop) but the
            // TickREQ trailing block is the only other free site, and it can
            // never see the predecessor once overwritten here
            if (cacheLine.activeREQ && cacheLine.activeREQ->IsComplete(glbl))
                FreeTxnID(cacheLine.activeREQ->GetFirst().flit.req.TxnID);

            cacheLine.activeREQ = xaction;
            cacheLine.pendingREQChannelTXREQ.reset();
            AgedErase(agedTXREQ, cacheLine.ageTXREQ, key);

            return { flit };
        }

        // a remaining queue candidate dispatches here: it is older than every
        // per-line candidate, or no per-line candidate survived the walk
        const bool queuePrefetchWins = !prefetchQueue.empty()
                                    && (cmoQueue.empty() || prefetchQueue.front().age < cmoQueue.front().age);
        const bool queueCMOWins      = !cmoQueue.empty() && !queuePrefetchWins;

        if (queuePrefetchWins)
        {
            // TODO: event: PrefetchPreChannelChosenEvent

            if (true) // result of event
            {
                // TODO: event: PrefetchPostChannelChosenEvent

                PrefetchEntry& prefetch = prefetchQueue.front();
                auto& flit = prefetch.GetPrefetchFlit();

                prefetch.future->Fire(PrefetchEmittedEvent(flit.Addr, flit));

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextREQ(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedPrefetch(*this, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedPrefetch(*this, denial, xaction, flit);
                }

                // ExpCompStash=0: no CompStash is expected and the joint retires the
                // Stash xaction immediately, so the TxnID ends at emission
                FreeTxnID(flit.TxnID);

                auto out = flit;
                prefetchQueue.pop_front();

                return { out };
            }
        }

        if (queueCMOWins)
        {
            // TODO: event: CMOPreChannelChosenEvent

            if (true) // result of event
            {
                // TODO: event: CMOPostChannelChosenEvent

                CMOEntry& cmo = cmoQueue.front();

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextREQ(glbl, time, cmo.GetCMOFlit(), &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedCMO(*this, xaction, cmo.GetCMOFlit());

                    // completion (future fire + TxnID free) happens on CompCMO in PushRXRSP
                    cmoTracker.push_back(std::move(cmo));
                    cmoQueue.pop_front();

                    return { cmoTracker.back().GetCMOFlit() };
                }

                if (events)
                    events->OnDeniedCMO(*this, denial, xaction, cmo.GetCMOFlit());

                // denied at Pop: the flit is still emitted (Peek/Pop consistency),
                // but the joint is not tracking it - fire now and free the TxnID
                auto out = cmo.GetCMOFlit();
                cmo.future->Fire(CMOCompleteEvent(xaction));
                FreeTxnID(out.TxnID);
                cmoQueue.pop_front();

                return { out };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::PushRXSNP(const Flits::SNP<config>& snpFlit) noexcept
    {
        Flits::SNP<config> flit = snpFlit;

        // reconstruct the full PA in 64 bits: the SNP Addr field carries
        // PA >> 3 in a 45-bit truncated type, whose own operator<< would
        // re-mask PA[47:45] away
        const uint64_t PA = uint64_t(flit.Addr) << 3;

        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        if (cacheLine)
        {
            if (IsSNPInFlight(*cacheLine))
            {
                // SNP with same PA in-flight, cannot accept new request
                return false;
            }
        }

        // SNP in-flight limit reached, backpressure the snoop
        if (CountInFlightSNP() >= xactionLimitSNP)
            return false;

        if (!cacheLine)
        {
            cacheLine = std::make_shared<CacheLine>(this, PA);
            SetCacheLine(cacheLine);
        }

        // acceptance point: the snoop's action-start age covers its SnpResp and
        // SnpRespData beats (a backpressured snoop above never reaches this stamp)
        cacheLine->ageSNP = NextEmissionAge();

        bool hazard = HasSNPHazard(*cacheLine);

        if (events)
            events->OnSNPPreHazardDetection(*this, *cacheLine, flit, hazard);

        if (events)
            events->OnSNPPostHazardDetection(*this, *cacheLine, flit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnSNPPreHazardPending(*this, *cacheLine, flit).GetDenial();

            if (!denial->IsRejected())
                cacheLine->pendingSNPHazardRXSNP = flit;

            if (events)
                events->OnSNPPostHazardPending(*this, *cacheLine, flit, denial);

            if (denial->IsRejected())
                return false;
        }
        else
        {
            std::shared_ptr<Xact::Xaction<config>> xaction;
            XactDenialEnum denial = joint.NextSNP(glbl, time, flit, &xaction);

            if (denial == XactDenial::ACCEPTED)
            {
                if (events)
                    events->OnAcceptedSNP(*this, *cacheLine, xaction, flit);
            }
            else
            {
                if (events)
                    events->OnDeniedSNP(*this, *cacheLine, denial, xaction, flit);
            }

            cacheLine->activeSNP = xaction;
        }

        return true;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpRSP<config>> UpstreamNode<config>::PeekTXRSP() noexcept
    {
        // line keys already offered (and cancelled) this call; empty in the
        // common case - a line occupies one deque entry per pending slot kind,
        // and after a cancellation its remaining entries must not re-offer it
        std::vector<uint64_t> skipped;

        for (size_t i = 0; i < agedTXRSP.size(); ++i)
        {
            auto [age, key] = agedTXRSP[i];

            bool isSkipped = false;
            for (uint64_t skippedKey : skipped)
                if (skippedKey == key)
                {
                    isSkipped = true;
                    break;
                }

            if (isSkipped)
                continue;

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || (!(lineIt->second->pendingSNPChannelTXRSP && lineIt->second->ageSNP == age)
              && !(lineIt->second->pendingREQChannelTXRSP && lineIt->second->ageTXREQ == age)))
            {
                agedTXRSP.erase(agedTXRSP.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            if (cacheLine.pendingSNPChannelTXRSP)
            {
                if (events)
                    if (events->OnSNPUpRSPPreChannelChosen(*this, cacheLine, *cacheLine.pendingSNPChannelTXRSP).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnSNPUpRSPPostChannelChosen(*this, cacheLine, *cacheLine.pendingSNPChannelTXRSP);

                return { *cacheLine.pendingSNPChannelTXRSP };
            }

            if (cacheLine.pendingREQChannelTXRSP)
            {
                if (events)
                    if (events->OnREQUpRSPPreChannelChosen(*this, cacheLine, *cacheLine.pendingREQChannelTXRSP).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnREQUpRSPPostChannelChosen(*this, cacheLine, *cacheLine.pendingREQChannelTXRSP);

                return { *cacheLine.pendingREQChannelTXRSP };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpRSP<config>> UpstreamNode<config>::PopTXRSP() noexcept
    {
        // line keys already offered (and cancelled) this call; empty in the
        // common case - a line occupies one deque entry per pending slot kind,
        // and after a cancellation its remaining entries must not re-offer it
        std::vector<uint64_t> skipped;

        for (size_t i = 0; i < agedTXRSP.size(); ++i)
        {
            auto [age, key] = agedTXRSP[i];

            bool isSkipped = false;
            for (uint64_t skippedKey : skipped)
                if (skippedKey == key)
                {
                    isSkipped = true;
                    break;
                }

            if (isSkipped)
                continue;

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || (!(lineIt->second->pendingSNPChannelTXRSP && lineIt->second->ageSNP == age)
              && !(lineIt->second->pendingREQChannelTXRSP && lineIt->second->ageTXREQ == age)))
            {
                agedTXRSP.erase(agedTXRSP.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            if (cacheLine.pendingSNPChannelTXRSP)
            {
                auto& flit = *cacheLine.pendingSNPChannelTXRSP;

                if (events)
                    if (events->OnSNPUpRSPPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnSNPUpRSPPostChannelChosen(*this, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpRSP(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpRSP(*this, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpRSP(*this, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingSNPChannelTXRSP.reset();
                // erase the POPPED slot's entry (its own age), not necessarily the
                // entry that won the iteration
                AgedErase(agedTXRSP, cacheLine.ageSNP, key);

                return { flit };
            }

            if (cacheLine.pendingREQChannelTXRSP)
            {
                auto& flit = *cacheLine.pendingREQChannelTXRSP;

                if (events)
                    if (events->OnREQUpRSPPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnREQUpRSPPostChannelChosen(*this, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpRSP(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpRSP(*this, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpRSP(*this, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingREQChannelTXRSP.reset();
                // erase the POPPED slot's entry (its own age), not necessarily the
                // entry that won the iteration
                AgedErase(agedTXRSP, cacheLine.ageTXREQ, key);

                return { flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpDAT<config>> UpstreamNode<config>::PeekTXDAT() noexcept
    {
        // line keys already offered (and cancelled) this call; empty in the
        // common case - a line occupies one deque entry per pending beat, and
        // after a cancellation its remaining entries must not re-offer it
        std::vector<uint64_t> skipped;

        for (size_t i = 0; i < agedTXDAT.size(); ++i)
        {
            auto [age, key] = agedTXDAT[i];

            bool isSkipped = false;
            for (uint64_t skippedKey : skipped)
                if (skippedKey == key)
                {
                    isSkipped = true;
                    break;
                }

            if (isSkipped)
                continue;

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || (!(lineIt->second->pendingSNPChannelTXDAT0 && lineIt->second->ageSNP == age)
              && !(lineIt->second->pendingSNPChannelTXDAT1 && lineIt->second->ageSNP == age)
              && !(lineIt->second->pendingEVTChannelTXDAT0 && lineIt->second->ageTXEVT == age)
              && !(lineIt->second->pendingEVTChannelTXDAT1 && lineIt->second->ageTXEVT == age)))
            {
                agedTXDAT.erase(agedTXDAT.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            if (cacheLine.pendingSNPChannelTXDAT0)
            {
                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, cacheLine, *cacheLine.pendingSNPChannelTXDAT0).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, cacheLine, *cacheLine.pendingSNPChannelTXDAT0);

                return { *cacheLine.pendingSNPChannelTXDAT0 };
            }

            if (cacheLine.pendingSNPChannelTXDAT1)
            {
                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, cacheLine, *cacheLine.pendingSNPChannelTXDAT1).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, cacheLine, *cacheLine.pendingSNPChannelTXDAT1);

                return { *cacheLine.pendingSNPChannelTXDAT1 };
            }

            if (cacheLine.pendingEVTChannelTXDAT0)
            {
                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, cacheLine, *cacheLine.pendingEVTChannelTXDAT0).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, cacheLine, *cacheLine.pendingEVTChannelTXDAT0);

                return { *cacheLine.pendingEVTChannelTXDAT0 };
            }

            if (cacheLine.pendingEVTChannelTXDAT1)
            {
                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, cacheLine, *cacheLine.pendingEVTChannelTXDAT1).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, cacheLine, *cacheLine.pendingEVTChannelTXDAT1);

                return { *cacheLine.pendingEVTChannelTXDAT1 };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpDAT<config>> UpstreamNode<config>::PopTXDAT() noexcept
    {
        // line keys already offered (and cancelled) this call; empty in the
        // common case - a line occupies one deque entry per pending beat, and
        // after a cancellation its remaining entries must not re-offer it
        std::vector<uint64_t> skipped;

        for (size_t i = 0; i < agedTXDAT.size(); ++i)
        {
            auto [age, key] = agedTXDAT[i];

            bool isSkipped = false;
            for (uint64_t skippedKey : skipped)
                if (skippedKey == key)
                {
                    isSkipped = true;
                    break;
                }

            if (isSkipped)
                continue;

            auto lineIt = cacheable.find(key);
            // self-heal: the entry is stale if the line is gone or no pending
            // slot carries this age
            if (lineIt == cacheable.end()
             || (!(lineIt->second->pendingSNPChannelTXDAT0 && lineIt->second->ageSNP == age)
              && !(lineIt->second->pendingSNPChannelTXDAT1 && lineIt->second->ageSNP == age)
              && !(lineIt->second->pendingEVTChannelTXDAT0 && lineIt->second->ageTXEVT == age)
              && !(lineIt->second->pendingEVTChannelTXDAT1 && lineIt->second->ageTXEVT == age)))
            {
                agedTXDAT.erase(agedTXDAT.begin() + i);
                --i;
                continue;
            }

            CacheLine& cacheLine = *lineIt->second;

            if (cacheLine.pendingSNPChannelTXDAT0)
            {
                auto& flit = *cacheLine.pendingSNPChannelTXDAT0;

                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingSNPChannelTXDAT0.reset();
                // erase the POPPED slot's entry (its own age), not necessarily the
                // entry that won the iteration; the two SNP-DAT beats of one snoop
                // share ageSNP, so this erases one of the two identical pairs and
                // the remaining one keeps beat 1 a candidate
                AgedErase(agedTXDAT, cacheLine.ageSNP, key);

                return { flit };
            }

            if (cacheLine.pendingSNPChannelTXDAT1)
            {
                auto& flit = *cacheLine.pendingSNPChannelTXDAT1;

                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingSNPChannelTXDAT1.reset();
                AgedErase(agedTXDAT, cacheLine.ageSNP, key);

                return { flit };
            }

            if (cacheLine.pendingEVTChannelTXDAT0)
            {
                auto& flit = *cacheLine.pendingEVTChannelTXDAT0;

                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingEVTChannelTXDAT0.reset();
                AgedErase(agedTXDAT, cacheLine.ageTXEVT, key);

                return { flit };
            }

            if (cacheLine.pendingEVTChannelTXDAT1)
            {
                auto& flit = *cacheLine.pendingEVTChannelTXDAT1;

                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, cacheLine, flit).IsCancelled())
                    {
                        skipped.push_back(key);
                        continue;
                    }

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingEVTChannelTXDAT1.reset();
                AgedErase(agedTXDAT, cacheLine.ageTXEVT, key);

                return { flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::PushRXRSP(const Flits::DnRSP<config>& dnrspFlit) noexcept
    {
        // TODO: event: DnRSPPreChannelActiveEvent

        // TODO: event: DnRSPPostChannelActiveEvent

        if (dnrspFlit.Opcode == Opcodes::DnRSP::Comp)
        {
            // Possible transaction:
            // 1. Cacheable Dataless
            //  - MakeUnique -> [Comp] -> CompAck
            // 2. Cacheable Allocating Read
            //  - ReadUnique -> [Comp] -> CompAck
            // 3. Evict
            //  - Evict -> [Comp]
            // 4. Write-Back
            //  - WriteBackFull -> DBIDResp -> CopyBackWrData -> [Comp]

            for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
            {
                CacheLine& cacheLine = *it->second;

                if (cacheLine.activeREQ 
                 && !cacheLine.activeREQ->IsComplete(glbl)
                 && cacheLine.activeREQ->GetTxnID() == dnrspFlit.TxnID)
                {
                    // TODO: event: DnRSPPreChannelREQConsumeEvent

                    // TODO: event: DnRSPPostChannelREQConsumeEvent

                    std::shared_ptr<Xact::Xaction<config>> xaction;
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit, &xaction);

                    if (denial == XactDenial::ACCEPTED)
                    {
                        if (events)
                            events->OnAcceptedDnRSP(*this, cacheLine, xaction, dnrspFlit);

                        CacheStateEnum state;

                        if (xaction->GetType() == Xact::XactionType::CacheableDataless)
                        {
                            std::shared_ptr<Xact::XactionCacheableDataless<config>> xactionCacheableDataless
                                = std::static_pointer_cast<Xact::XactionCacheableDataless<config>>(xaction);

                            switch (xactionCacheableDataless->GetFirst().flit.req.Opcode)
                            {
                                case Opcodes::REQ::MakeUnique:
                                {
                                    state = CacheState::UniqueClean;
                                    break;
                                }

                                default:
                                    // may be should not reach here, or unsupported REQ
                                    return true;
                            }
                        }
                        else if (xaction->GetType() == Xact::XactionType::CacheableAllocatingRead)
                        {
                            std::shared_ptr<Xact::XactionCacheableAllocatingRead<config>> xactionCacheableAllocatingRead
                                = std::static_pointer_cast<Xact::XactionCacheableAllocatingRead<config>>(xaction);

                            switch (xactionCacheableAllocatingRead->GetFirst().flit.req.Opcode)
                            {
                                case Opcodes::REQ::ReadUnique:
                                {
                                    state = CacheState::UniqueClean;
                                    break;
                                }

                                default:
                                    // may be should not reach here, or unsupported REQ
                                    return true;
                            }
                        }
                        else
                            return true;

                        // TODO: REQDnRSPCacheStatePrePromotionEvent should be supported after state checker was implemented

                        // TODO: event: REQDnRSPCacheStatePostPromotionEvent

                        cacheLine.state = state;
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnRSP(*this, cacheLine, denial, xaction, dnrspFlit);
                    }

                    return true;
                }
                else if (cacheLine.activeEVT
                 && !cacheLine.activeEVT->IsComplete(glbl)
                 && cacheLine.activeEVT->GetTxnID() == dnrspFlit.TxnID)
                {
                    // TODO: event: DnRSPPreChannelEVTConsumeEvent

                    // TODO: event: DnRSPPostChannelEVTConsumeEvent

                    std::shared_ptr<Xact::Xaction<config>> xaction;
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit, &xaction);

                    if (denial == XactDenial::ACCEPTED)
                    {
                        if (events)
                            events->OnAcceptedDnRSP(*this, cacheLine, xaction, dnrspFlit);
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnRSP(*this, cacheLine, denial, xaction, dnrspFlit);
                    }

                    return true;
                }
            }

            // TODO: event: DnRSPDroppedEvent (MISSING_TXNID)

            return true;
        }
        else if (dnrspFlit.Opcode == Opcodes::DnRSP::DBIDResp)
        {
            // Possible transaction:
            // 1. Write-Back
            //  - WriteBackFull -> [DBIDResp] -> CopyBackWrData -> Comp

            for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
            {
                CacheLine& cacheLine = *it->second;

                if (cacheLine.activeEVT
                 && !cacheLine.activeEVT->IsComplete(glbl)
                 && cacheLine.activeEVT->GetTxnID() == dnrspFlit.TxnID)
                {
                    // TODO: event: DnRSPPreChannelEVTConsumeEvent

                    // TODO: event: DnRSPPostChannelEVTConsumeEvent

                    std::shared_ptr<Xact::Xaction<config>> xaction;
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit, &xaction);

                    if (denial == XactDenial::ACCEPTED)
                    {
                        if (events)
                            events->OnAcceptedDnRSP(*this, cacheLine, xaction, dnrspFlit);
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnRSP(*this, cacheLine, denial, xaction, dnrspFlit);
                    }

                    return true;
                }
            }

            // TODO: event: DnRSPDroppedEvent (MISSING_TXNID)

            return true;
        }
        else if (dnrspFlit.Opcode == Opcodes::DnRSP::CompDBIDResp)
        {
            // Possible transaction:
            // 1. Write-Back
            //  - WriteBackFull -> [CompDBIDResp] -> CopyBackWrData -> Comp

            for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
            {
                CacheLine& cacheLine = *it->second;

                if (cacheLine.activeEVT
                 && !cacheLine.activeEVT->IsComplete(glbl)
                 && cacheLine.activeEVT->GetTxnID() == dnrspFlit.TxnID)
                {
                    // TODO: event: DnRSPPreChannelEVTConsumeEvent

                    // TODO: event: DnRSPPostChannelEVTConsumeEvent

                    std::shared_ptr<Xact::Xaction<config>> xaction;
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit, &xaction);

                    if (denial == XactDenial::ACCEPTED)
                    {
                        if (events)
                            events->OnAcceptedDnRSP(*this, cacheLine, xaction, dnrspFlit);
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnRSP(*this, cacheLine, denial, xaction, dnrspFlit);
                    }

                    return true;
                }
            }

            // TODO: event: DnRSPDroppedEvent (MISSING_TXNID)

            return true;
        }
        else if (dnrspFlit.Opcode == Opcodes::DnRSP::CompCMO)
        {
            // Possible transaction:
            // 1. CMO
            //  - CleanShared/CleanInvalid/MakeInvalid -> [CompCMO]

            for (auto it = cmoTracker.begin(); it != cmoTracker.end(); ++it)
            {
                if (it->GetCMOFlit().TxnID == dnrspFlit.TxnID)
                {
                    std::shared_ptr<Xact::Xaction<config>> xaction;
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit, &xaction);

                    if (denial == XactDenial::ACCEPTED)
                    {
                        if (events)
                            events->OnAcceptedCompCMO(*this, xaction, dnrspFlit);

                        it->future->Fire(CMOCompleteEvent(xaction));

                        FreeTxnID(dnrspFlit.TxnID);
                        cmoTracker.erase(it);
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedCompCMO(*this, denial, xaction, dnrspFlit);

                        // entry stays tracked - the transaction is still outstanding
                    }

                    return true;
                }
            }

            // no tracked CMO matches (e.g. the late CompCMO of a Pop-time-denied CMO)
            // TODO: event: DnRSPDroppedEvent (MISSING_TXNID)

            return true;
        }

        // TODO: event: DnRSPDroppedEvent (UNRECOGNIZED_OPCODE)

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::PushRXDAT(const Flits::DnDAT<config>& dndatFlit) noexcept
    {
        // TODO: event: DnDATPreChannelActiveEvent

        // TODO: event: DnDATPostChannelActiveEvent

        if (dndatFlit.Opcode == Opcodes::DnDAT::CompData)
        {
            // Possible transaction:
            // 1. Cacheable Allocating Read
            //  - ReadShared -> [CompData] -> CompAck
            //  - ReadUnique -> [CompData] -> CompAck

            for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
            {
                CacheLine& cacheLine = *it->second;

                if (cacheLine.activeREQ
                 && !cacheLine.activeREQ->IsComplete(glbl)
                 && cacheLine.activeREQ->GetTxnID() == dndatFlit.TxnID)
                {
                    // TODO: event: DnDATPreChannelREQConsumeEvent

                    // TODO: event: DnDATPostChannelREQConsumeEvent

                    std::shared_ptr<Xact::Xaction<config>> xaction;
                    XactDenialEnum denial = joint.NextDnDAT(glbl, time, dndatFlit, &xaction);

                    if (denial == XactDenial::ACCEPTED)
                    {
                        if (events)
                            events->OnAcceptedDnDAT(*this, cacheLine, xaction, dndatFlit);

                        CacheStateEnum state;

                        if (xaction->GetType() == Xact::XactionType::CacheableAllocatingRead)
                        {
                            std::shared_ptr<Xact::XactionCacheableAllocatingRead<config>> xactionCacheableAllocatingRead
                                = std::static_pointer_cast<Xact::XactionCacheableAllocatingRead<config>>(xaction);

                            // store the beat payload into the line (every beat;
                            // the state promotion below stays first-beat-only)
                            size_t dataID = static_cast<size_t>(dndatFlit.DataID);

                            if (dataID < 2)
                            {
                                cacheLine.data[dataID * 4 + 0] = dndatFlit.Data[0];
                                cacheLine.data[dataID * 4 + 1] = dndatFlit.Data[1];
                                cacheLine.data[dataID * 4 + 2] = dndatFlit.Data[2];
                                cacheLine.data[dataID * 4 + 3] = dndatFlit.Data[3];
                            }

                            if (xaction->GetFirstDnDAT({ Opcodes::DnDAT::CompData }) != xaction->GetLastDnDAT({ Opcodes::DnDAT::CompData }))
                            {
                                // no longer the first DnDAT flit, state already updated
                                return true;
                            }

                            switch (xactionCacheableAllocatingRead->GetFirst().flit.req.Opcode)
                            {
                                case Opcodes::REQ::ReadShared:
                                {
                                    if (dndatFlit.Resp == Resps::SC)
                                        state = CacheState::Shared;
                                    else if (dndatFlit.Resp == Resps::UC)
                                        state = CacheState::UniqueClean;
                                    else if (dndatFlit.Resp == Resps::UC_PD)
                                        state = CacheState::UniqueDirty;
                                    else
                                    {
                                        // TODO: should not reach here maybe, filtered by state checker
                                        return true;
                                    }

                                    break;
                                }

                                case Opcodes::REQ::ReadUnique:
                                {
                                    state = CacheState::UniqueClean;
                                    break;
                                }

                                default:
                                    // may be should not reach here, or unsupported REQ
                                    return true;
                            }

                            // TODO: REQDnDATCacheStatePrePromotionEvent should be supported after state checker was implemented

                            // TODO: event: REQDnDATCacheStatePostPromotionEvent

                            cacheLine.state = state;
                        }
                        else
                            return true;
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnDAT(*this, cacheLine, denial, xaction, dndatFlit);
                    }

                    return true;
                }
            }

            // TODO: unexpected DnDAT

            return true;
        }

        // TODO: unrecognized DnDAT

        return true;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<size_t> UpstreamNode<config>::AllocateTxnID() noexcept
    {
        // allocate and mark the first free TxnID; the ID stays occupied until the
        // transaction completes (TickREQ/TickEVT) or its flit is denied by the joint
        for (size_t i = 0; i < usedTxnID.size(); ++i)
        {
            if (!usedTxnID[i])
            {
                usedTxnID[i] = true;
                return { i };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::FreeTxnID(size_t txnID) noexcept
    {
        usedTxnID.at(txnID) = false;
    }
}


// Implementation of: template<class TEvent> class FutureNow
namespace CCHI::Taurus {

    template<class TEvent>
    inline FutureNow<TEvent>::FutureNow(DenialEnum denial) noexcept
        : denial            (denial)
        , event             ()
        , future            ()
        , firedFutureCount  (0)
    { }

    template<class TEvent>
    inline FutureNow<TEvent>::FutureNow(DenialEnum denial, const TEvent& event) noexcept
        : denial            (denial)
        , event             (event)
        , future            ()
        , firedFutureCount  (0)
    { }

    template<class TEvent>
    inline bool FutureNow<TEvent>::Bind(func_t func) noexcept
    {
        if (IsNow())
        {
            func(*event);
            return true;
        }

        if (IsFuture())
        {
            future.push_back(func);
            return true;
        }

        return false;
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::BindNow(func_t func) noexcept
    {
        if (IsNow())
        {
            func(*event);
            return true;
        }

        return false;
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::BindFuture(func_t func) noexcept
    {
        if (IsFuture())
        {
            future.push_back(func);
            return true;
        }

        return false;
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsRejected() const noexcept
    {
        return denial->IsRejected();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsAccepted() const noexcept
    {
        return denial->IsAccepted();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsDone() const noexcept
    {
        return denial->IsDone();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsFuture() const noexcept
    {
        return !event.has_value() && denial->IsAccepted();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsNow() const noexcept
    {
        return event.has_value() && !denial->IsRejected();
    }

    template<class TEvent>
    inline size_t FutureNow<TEvent>::Fired() const noexcept
    {
        return firedFutureCount;
    }

    template<class TEvent>
    inline DenialEnum FutureNow<TEvent>::GetDenial() const noexcept
    {
        return denial;
    }

    template<class TEvent>
    inline void FutureNow<TEvent>::Fire(const TEvent& event) noexcept
    {
        this->event.emplace(event);

        for (auto& func : future)
            func(event);

        future.clear();

        firedFutureCount++;
    }
}


// Implementation of: class UpstreamNode::EventHub
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::EventHub::EventHub() noexcept
    { }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::EventHub::Clear() noexcept
    {
        OnEVTPreHazardDetection.UnregisterAll();
        OnEVTPostHazardDetection.UnregisterAll();
        OnEVTPreHazardPending.UnregisterAll();
        OnEVTPostHazardPending.UnregisterAll();
        OnEVTPreChannelPending.UnregisterAll();
        OnEVTPostChannelPending.UnregisterAll();
        OnEVTPreHazardToChannelPending.UnregisterAll();
        OnEVTPostHazardToChannelPending.UnregisterAll();
        OnEVTCacheStatePreDemotion.UnregisterAll();
        OnEVTCacheStatePostDemotion.UnregisterAll();
        OnEVTDataPreHazardDetection.UnregisterAll();
        OnEVTDataPostHazardDetection.UnregisterAll();
        OnEVTDataPreHazardPending.UnregisterAll();
        OnEVTDataPostHazardPending.UnregisterAll();
        OnEVTDataPreChannelPending.UnregisterAll();
        OnEVTDataPostChannelPending.UnregisterAll();
        OnEVTDataPreHazardToChannelPending.UnregisterAll();
        OnEVTDataPostHazardToChannelPending.UnregisterAll();
        OnEVTPreChannelChosen.UnregisterAll();
        OnEVTPostChannelChosen.UnregisterAll();
        OnEVTUpDATPreChannelChosen.UnregisterAll();
        OnEVTUpDATPostChannelChosen.UnregisterAll();

        OnSNPPreHazardDetection.UnregisterAll();
        OnSNPPostHazardDetection.UnregisterAll();
        OnSNPPreHazardPending.UnregisterAll();
        OnSNPPostHazardPending.UnregisterAll();
        OnSNPCacheStatePreDemotion.UnregisterAll();
        OnSNPCacheStatePostDemotion.UnregisterAll();
        OnSNPRespPreChannelPending.UnregisterAll();
        OnSNPRespPostChannelPending.UnregisterAll();
        OnSNPRespDataPreChannelPending.UnregisterAll();
        OnSNPRespDataPostChannelPending.UnregisterAll();
        OnSNPUpRSPPreChannelChosen.UnregisterAll();
        OnSNPUpRSPPostChannelChosen.UnregisterAll();
        OnSNPUpDATPreChannelChosen.UnregisterAll();
        OnSNPUpDATPostChannelChosen.UnregisterAll();

        OnREQPreHazardDetection.UnregisterAll();
        OnREQPostHazardDetection.UnregisterAll();
        OnREQPreHazardPending.UnregisterAll();
        OnREQPostHazardPending.UnregisterAll();
        OnREQPreChannelPending.UnregisterAll();
        OnREQPostChannelPending.UnregisterAll();
        OnREQPreHazardToChannelPending.UnregisterAll();
        OnREQPostHazardToChannelPending.UnregisterAll();
        OnREQCompAckPreChannelPending.UnregisterAll();
        OnREQCompAckPostChannelPending.UnregisterAll();
        OnREQPreChannelChosen.UnregisterAll();
        OnREQPostChannelChosen.UnregisterAll();
        OnREQUpRSPPreChannelChosen.UnregisterAll();
        OnREQUpRSPPostChannelChosen.UnregisterAll();

        OnAcceptedEVT.UnregisterAll();
        OnAcceptedSNP.UnregisterAll();
        OnAcceptedREQ.UnregisterAll();
        OnAcceptedDnRSP.UnregisterAll();
        OnAcceptedUpRSP.UnregisterAll();
        OnAcceptedDnDAT.UnregisterAll();
        OnAcceptedUpDAT.UnregisterAll();
        OnAcceptedPrefetch.UnregisterAll();
        OnAcceptedCMO.UnregisterAll();
        OnAcceptedCompCMO.UnregisterAll();

        OnDeniedEVT.UnregisterAll();
        OnDeniedSNP.UnregisterAll();
        OnDeniedREQ.UnregisterAll();
        OnDeniedDnRSP.UnregisterAll();
        OnDeniedUpRSP.UnregisterAll();
        OnDeniedDnDAT.UnregisterAll();
        OnDeniedUpDAT.UnregisterAll();
        OnDeniedPrefetch.UnregisterAll();
        OnDeniedCMO.UnregisterAll();
        OnDeniedCompCMO.UnregisterAll();

        OnCacheLineGranted.UnregisterAll();
        OnCacheLinePreLoad.UnregisterAll();
        OnCacheLinePostLoad.UnregisterAll();
        OnCacheLinePreStore.UnregisterAll();
        OnCacheLinePostStore.UnregisterAll();
    }
}


// Implementation of: class UpstreamNode::CacheLine
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::CacheLineEventBase::CacheLineEventBase(
        std::shared_ptr<CacheLine> cacheLine) noexcept
        : cacheLine (cacheLine)
        , PA        (cacheLine->GetPA())
    { }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNode<config>::CacheLineEventBase::GetPA() const noexcept
    {
        return PA;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<typename UpstreamNode<config>::CacheLine>
    UpstreamNode<config>::CacheLineEventBase::GetCacheLine() noexcept
    {
        return cacheLine.lock();
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const typename UpstreamNode<config>::CacheLine>
    UpstreamNode<config>::CacheLineEventBase::GetCacheLine() const noexcept
    {
        return cacheLine.lock();
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::GrantedEvent::GrantedEvent(
        std::shared_ptr<CacheLine> cacheLine) noexcept
        : CacheLineEventBase(std::move(cacheLine))
    { }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::EvictedEvent::EvictedEvent(
        std::shared_ptr<CacheLine> cacheLine) noexcept
        : CacheLineEventBase(std::move(cacheLine))
    { }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::PrefetchEntry::PrefetchEntry(
        const Flits::REQ<config>& prefetchFlit,
        std::shared_ptr<FutureNow<PrefetchEmittedEvent>> future) noexcept
        : prefetchFlit  (prefetchFlit)
        , future        (std::move(future))
    { }

    template<FlitConfigurationConcept config>
    inline Flits::REQ<config>& UpstreamNode<config>::PrefetchEntry::GetPrefetchFlit() noexcept
    {
        return prefetchFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::REQ<config>& UpstreamNode<config>::PrefetchEntry::GetPrefetchFlit() const noexcept
    {
        return prefetchFlit;
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::PrefetchEmittedEvent::PrefetchEmittedEvent(
        uint64_t PA,
        const Flits::REQ<config>& prefetchFlit) noexcept
        : PA            (PA)
        , prefetchFlit  (prefetchFlit)
    { }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNode<config>::PrefetchEmittedEvent::GetPA() const noexcept
    {
        return PA;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::REQ<config>& UpstreamNode<config>::PrefetchEmittedEvent::GetPrefetchFlit() const noexcept
    {
        return prefetchFlit;
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::CMOEntry::CMOEntry(
        const Flits::REQ<config>& cmoFlit,
        std::shared_ptr<FutureNow<CMOCompleteEvent>> future) noexcept
        : cmoFlit   (cmoFlit)
        , future    (std::move(future))
    { }

    template<FlitConfigurationConcept config>
    inline Flits::REQ<config>& UpstreamNode<config>::CMOEntry::GetCMOFlit() noexcept
    {
        return cmoFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::REQ<config>& UpstreamNode<config>::CMOEntry::GetCMOFlit() const noexcept
    {
        return cmoFlit;
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::CMOCompleteEvent::CMOCompleteEvent(
        std::shared_ptr<Xact::Xaction<config>> xaction) noexcept
        : xaction   (xaction)
    { }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNode<config>::CMOCompleteEvent::GetPA() const noexcept
    {
        if (!xaction)
            return 0;

        return xaction->GetFirst().flit.req.Addr;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xact::Xaction<config>> UpstreamNode<config>::CMOCompleteEvent::GetXaction() noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const Xact::Xaction<config>> UpstreamNode<config>::CMOCompleteEvent::GetXaction() const noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::ReadEvent::ReadEvent(uint64_t PA) noexcept
        : PA    (PA)
        , data  (nullptr)
    { }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::ReadEvent::ReadEvent(
        uint64_t PA,
        std::shared_ptr<uint64_t[]> data) noexcept
        : PA    (PA)
        , data  (std::move(data))
    { }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNode<config>::ReadEvent::GetPA() const noexcept
    {
        return PA;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<uint64_t[]> UpstreamNode<config>::ReadEvent::GetData() noexcept
    {
        return data;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const uint64_t[]> UpstreamNode<config>::ReadEvent::GetData() const noexcept
    {
        return data;
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::CompleteEvent::CompleteEvent(uint64_t PA) noexcept
        : PA(PA)
    { }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNode<config>::CompleteEvent::GetPA() const noexcept
    {
        return PA;
    }

    template<FlitConfigurationConcept config>
    inline const std::span<const uint64_t, 8> UpstreamNode<config>::CacheLine::GetData() const noexcept
    {
        return std::span<const uint64_t, 8>(data);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<std::span<const uint64_t, 8>> UpstreamNode<config>::CacheLine::Load() const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (IsREQFilling())
            return std::nullopt;

        using LoadType = UpstreamNodeCacheLineLoadEventBase<config>::LoadType;

        if (owner->events)
            owner->events->OnCacheLinePreLoad(*owner, *this, LoadType::LOAD_LINE, 0);

        auto span = std::span<const uint64_t, 8>(data);

        if (owner->events)
            owner->events->OnCacheLinePostLoad(*owner, *this, LoadType::LOAD_LINE, 0);

        return { span };
    }

    template<FlitConfigurationConcept config>
    inline std::optional<uint64_t> UpstreamNode<config>::CacheLine::Load64(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 8)
            return std::nullopt;

        if (IsREQFilling())
            return std::nullopt;

        using LoadType = UpstreamNodeCacheLineLoadEventBase<config>::LoadType;

        if (owner->events)
            owner->events->OnCacheLinePreLoad(*owner, *this, LoadType::LOAD_64, alignedOffset);

        uint64_t value = data[alignedOffset];

        if (owner->events)
            owner->events->OnCacheLinePostLoad(*owner, *this, LoadType::LOAD_64, alignedOffset);

        return { value };
    }

    template<FlitConfigurationConcept config>
    inline std::optional<uint32_t> UpstreamNode<config>::CacheLine::Load32(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 16)
            return std::nullopt;

        if (IsREQFilling())
            return std::nullopt;

        using LoadType = UpstreamNodeCacheLineLoadEventBase<config>::LoadType;

        if (owner->events)
            owner->events->OnCacheLinePreLoad(*owner, *this, LoadType::LOAD_32, alignedOffset);

        uint32_t value = uint32_t(data[alignedOffset >> 1] >> ((alignedOffset & 1) * 32));

        if (owner->events)
            owner->events->OnCacheLinePostLoad(*owner, *this, LoadType::LOAD_32, alignedOffset);

        return { value };

    }

    template<FlitConfigurationConcept config>
    inline std::optional<uint16_t> UpstreamNode<config>::CacheLine::Load16(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 32)
            return std::nullopt;

        if (IsREQFilling())
            return std::nullopt;

        using LoadType = UpstreamNodeCacheLineLoadEventBase<config>::LoadType;

        if (owner->events)
            owner->events->OnCacheLinePreLoad(*owner, *this, LoadType::LOAD_16, alignedOffset);

        uint16_t value = uint16_t(data[alignedOffset >> 2] >> ((alignedOffset & 3) * 16));

        if (owner->events)
            owner->events->OnCacheLinePostLoad(*owner, *this, LoadType::LOAD_16, alignedOffset);

        return { value };
    }

    template<FlitConfigurationConcept config>
    inline std::optional<uint8_t> UpstreamNode<config>::CacheLine::Load8(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 64)
            return std::nullopt;

        if (IsREQFilling())
            return std::nullopt;

        using LoadType = UpstreamNodeCacheLineLoadEventBase<config>::LoadType;

        if (owner->events)
            owner->events->OnCacheLinePreLoad(*owner, *this, LoadType::LOAD_8, alignedOffset);

        uint8_t value = uint8_t(data[alignedOffset >> 3] >> ((alignedOffset & 7) * 8));

        if (owner->events)
            owner->events->OnCacheLinePostLoad(*owner, *this, LoadType::LOAD_8, alignedOffset);

        return { value };
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store(const std::span<const uint64_t, 8>& newData) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (IsREQFilling())
            return false;
        
        using StoreType = UpstreamNodeCacheLineStoreEventBase<config>::StoreType;

        if (owner->events)
            owner->events->OnCacheLinePreStore(*owner, *this, StoreType::STORE_LINE, 0);

        data[0] = newData[0];
        data[1] = newData[1];
        data[2] = newData[2];
        data[3] = newData[3];
        data[4] = newData[4];
        data[5] = newData[5];
        data[6] = newData[6];
        data[7] = newData[7];

        this->state = CacheState::UniqueDirty;

        if (owner->events)
            owner->events->OnCacheLinePostStore(*owner, *this, StoreType::STORE_LINE, 0);

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store64(size_t alignedOffset, uint64_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 8)
            return false;

        if (IsREQFilling())
            return false;

        using StoreType = UpstreamNodeCacheLineStoreEventBase<config>::StoreType;

        if (owner->events)
            owner->events->OnCacheLinePreStore(*owner, *this, StoreType::STORE_64, alignedOffset);

        data[alignedOffset] = value;

        this->state = CacheState::UniqueDirty;

        if (owner->events)
            owner->events->OnCacheLinePostStore(*owner, *this, StoreType::STORE_64, alignedOffset);

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store32(size_t alignedOffset, uint32_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 16)
            return false;

        if (IsREQFilling())
            return false;

        using StoreType = UpstreamNodeCacheLineStoreEventBase<config>::StoreType;

        if (owner->events)
            owner->events->OnCacheLinePreStore(*owner, *this, StoreType::STORE_32, alignedOffset);

        uint64_t& lane = data[alignedOffset >> 1];
        const uint64_t mask = uint64_t(0xFFFFFFFF) << ((alignedOffset & 1) * 32);

        lane = (lane & ~mask) | ((uint64_t(value) << ((alignedOffset & 1) * 32)) & mask);

        this->state = CacheState::UniqueDirty;

        if (owner->events)
            owner->events->OnCacheLinePostStore(*owner, *this, StoreType::STORE_32, alignedOffset);

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store16(size_t alignedOffset, uint16_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 32)
            return false;

        if (IsREQFilling())
            return false;

        using StoreType = UpstreamNodeCacheLineStoreEventBase<config>::StoreType;

        if (owner->events)
            owner->events->OnCacheLinePreStore(*owner, *this, StoreType::STORE_16, alignedOffset);

        uint64_t& lane = data[alignedOffset >> 2];
        const uint64_t mask = uint64_t(0xFFFF) << ((alignedOffset & 3) * 16);

        lane = (lane & ~mask) | ((uint64_t(value) << ((alignedOffset & 3) * 16)) & mask);

        this->state = CacheState::UniqueDirty;

        if (owner->events)
            owner->events->OnCacheLinePostStore(*owner, *this, StoreType::STORE_16, alignedOffset);

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store8(size_t alignedOffset, uint8_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 64)
            return false;

        if (IsREQFilling())
            return false;

        using StoreType = UpstreamNodeCacheLineStoreEventBase<config>::StoreType;

        if (owner->events)
            owner->events->OnCacheLinePreStore(*owner, *this, StoreType::STORE_8, alignedOffset);

        uint64_t& lane = data[alignedOffset >> 3];
        const uint64_t mask = uint64_t(0xFF) << ((alignedOffset & 7) * 8);

        lane = (lane & ~mask) | ((uint64_t(value) << ((alignedOffset & 7) * 8)) & mask);

        this->state = CacheState::UniqueDirty;

        if (owner->events)
            owner->events->OnCacheLinePostStore(*owner, *this, StoreType::STORE_8, alignedOffset);

        return true;
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::CacheLine::CacheLine(UpstreamNode<config>* owner, uint64_t PA) noexcept
        : owner (owner)
        , PA    (LineBase(PA))
    { }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNode<config>::CacheLine::GetPA() const noexcept
    {
        return PA;
    }

    template<FlitConfigurationConcept config>
    inline CacheStateEnum UpstreamNode<config>::CacheLine::GetState() const noexcept
    {
        return state;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::IsEVTInFlight(const Xact::Global<config>& glbl) const noexcept
    {
        if (this->pendingEVTHazardTXEVT)
            return true;

        if (this->pendingEVTChannelTXEVT)
            return true;

        if (this->pendingEVTHazardTXDAT0)
            return true;

        if (this->pendingEVTHazardTXDAT1)
            return true;

        if (this->pendingEVTChannelTXDAT0)
            return true;

        if (this->pendingEVTChannelTXDAT1)
            return true;

        if (!this->activeEVT)
            return false;

        return !this->activeEVT->IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::IsSNPInFlight(const Xact::Global<config>& glbl) const noexcept
    {
        if (this->pendingSNPHazardRXSNP)
            return true;

        if (this->pendingSNPChannelTXRSP)
            return true;

        if (this->pendingSNPChannelTXDAT0)
            return true;

        if (this->pendingSNPChannelTXDAT1)
            return true;

        if (!this->activeSNP)
            return false;

        return !this->activeSNP->IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::IsREQInFlight(const Xact::Global<config>& glbl) const noexcept
    {
        if (this->pendingREQHazardTXREQ)
            return true;

        if (this->pendingREQChannelTXREQ)
            return true;

        if (this->pendingREQChannelTXRSP)
            return true;

        if (!this->activeREQ)
            return false;

        return !this->activeREQ->IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::IsREQFilling() const noexcept
    {
        if (!this->activeREQ)
            return false;

        if (this->activeREQ->GetType() != Xact::XactionType::CacheableAllocatingRead)
            return false;

        const Xact::XactionCacheableAllocatingRead<config>& xaction
            = static_cast<const Xact::XactionCacheableAllocatingRead<config>&>(*this->activeREQ);

        return xaction.GotAnyCompData() && !xaction.GotAllCompData();
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::HasREQHazard(const Xact::Global<config>& glbl) const noexcept
    {
        // *NOTE: a channel-pended (or hazard-pended) EVT has not reached the joint yet
        //        (activeEVT is still null), but it already owns the line: its state was
        //        demoted to Invalid at DoEvict time. A REQ must park behind it, exactly
        //        as it parks behind an in-flight pre-Comp EVT below.
        if (pendingEVTHazardTXEVT)
            return true;

        if (pendingEVTChannelTXEVT)
            return true;

        if (IsEVTInFlight(glbl) && activeEVT)
        {
            if (activeEVT->GetType() == Xact::XactionType::Evict)
            {
                const Xact::XactionEvict<config>& xactionEvict 
                    = static_cast<const Xact::XactionEvict<config>&>(*activeEVT);

                if (!xactionEvict.GotComp())
                    return true;
            }
            else if (activeEVT->GetType() == Xact::XactionType::WriteBack)
            {
                const Xact::XactionWriteBack<config>& xactionWriteBack 
                    = static_cast<const Xact::XactionWriteBack<config>&>(*activeEVT);

                if (!xactionWriteBack.GotComp())
                    return true;
            }
            else
            {
                // TODO: maybe should not reach here
            }
        }
        
        // *NOTE: block only while an active SNP has not updated the cache state yet
        //        (not yet processed by TickSNP: no response pended or sent). A REQ
        //        must compute its flit fields from the post-snoop state. Once the
        //        state is updated, the remaining REQ-vs-SnpResp wire ordering is
        //        the downstream's responsibility.
        if (activeSNP
         && !pendingSNPChannelTXRSP
         && !pendingSNPChannelTXDAT0
         && !pendingSNPChannelTXDAT1)
        {
            if (activeSNP->GetType() == Xact::XactionType::Snoop)
            {
                const Xact::XactionSnoop<config>& xactionSnoop 
                    = static_cast<const Xact::XactionSnoop<config>&>(*activeSNP);

                if (!xactionSnoop.GotAnyResp())
                    return true;
            }
            else
            {
                // TODO: maybe should not reach here
            }
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::HasSNPHazard(const Xact::Global<config>& glbl) const noexcept
    {
        if (pendingEVTHazardTXEVT)
            return true;

        if (pendingEVTChannelTXEVT)
            return true;

        if (activeEVT && !GotEVTComp(glbl))
            return true;

        if (activeREQ)
        {
            if (activeREQ->GetType() == Xact::XactionType::CacheableAllocatingRead)
            {
                const Xact::XactionCacheableAllocatingRead<config>& xaction
                    = static_cast<const Xact::XactionCacheableAllocatingRead<config>&>(*activeREQ);

                if (xaction.GotAnyCompData() && !xaction.GotAllCompData())
                    return true;
            }
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::GotEVTComp(const Xact::Global<config>& glbl) const noexcept
    {
        if (!activeEVT)
            return false;

        if (activeEVT->GetType() == Xact::XactionType::Evict)
        {
            const Xact::XactionEvict<config>& xactionEvict
                = static_cast<const Xact::XactionEvict<config>&>(*activeEVT);

            return xactionEvict.GotComp();
        }
        else if (activeEVT->GetType() == Xact::XactionType::WriteBack)
        {
            const Xact::XactionWriteBack<config>& xactionWriteBack
                = static_cast<const Xact::XactionWriteBack<config>&>(*activeEVT);

            // GotComp() covers both Comp and CompDBIDResp
            return xactionWriteBack.GotComp();
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::HasEVTHazard(const Xact::Global<config>& glbl) const noexcept
    {
        if (pendingREQChannelTXREQ)
            return true;

        if (activeREQ)
        {
            if (activeREQ->GetType() == Xact::XactionType::CacheableAllocatingRead)
            {
                const Xact::XactionCacheableAllocatingRead<config>& xaction
                    = static_cast<const Xact::XactionCacheableAllocatingRead<config>&>(*activeREQ);

                if (xaction.GotAnyCompData() && !xaction.GotAllCompData())
                    return true;
            }
        }

        // *NOTE: a parked SNP (pendingSNPHazardRXSNP) never hazards an EVT: a SNP that
        //        has not left the hazard pending queue has no priority claim, and the
        //        EVT is always handled first. This also makes the SNP<->EVT blocking
        //        one-directional (SNP waits for EVT), so no circular wait can form.

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::HasEVTDataHazard(const Xact::Global<config>&, Flits::DnDAT<config>::dataid_t) const noexcept
    {
        // *NOTE: no data hazard can exist between CopyBackWrData and an in-flight
        //        allocating read. A conforming downstream never returns CompData before
        //        receiving the corresponding CopyBackWrData, and both beats are built at
        //        the first post-DBIDResp Tick - strictly before any fill can land in
        //        cacheLine.data (the only other writers are Store*, rejected on the
        //        Invalid state the line was demoted to at DoEvict time, and snoops to an
        //        Invalid line answer SnpResp I without touching data; a split
        //        DBIDResp-before-Comp flow is likewise safe because HasREQHazard then
        //        still holds the read). The build therefore always snapshots the
        //        evicted data, and this predicate is always false. The
        //        pendingEVTHazardTXDAT0/1 machinery and the hazard-detection events are
        //        retained for the listener SetHazard() override, which can only inject
        //        bounded backpressure.
        return false;
    }
}


#endif // __CCHI__CCHI_ICN_TAURUS__COMPONENT
