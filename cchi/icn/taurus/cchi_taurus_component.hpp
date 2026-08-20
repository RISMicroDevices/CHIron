#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__COMPONENT
#define __CCHI__CCHI_ICN_TAURUS__COMPONENT

#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <list>

#include "../../xact/cchi_joint.hpp"

#include "cchi_taurus_component_afx.hpp"
#include "cchi_taurus_component_events.hpp"

#include "cchi_taurus_denial.hpp"
#include "cchi_taurus_state.hpp"
#include "cchi_taurus_sam.hpp"


namespace CCHI::Taurus {

    //
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

            Gravity::EventBus<UpstreamNodeXactDeniedEVTEvent<config>>                   OnDeniedEVT;
            Gravity::EventBus<UpstreamNodeXactDeniedSNPEvent<config>>                   OnDeniedSNP;
            Gravity::EventBus<UpstreamNodeXactDeniedREQEvent<config>>                   OnDeniedREQ;
            Gravity::EventBus<UpstreamNodeXactDeniedDnRSPEvent<config>>                 OnDeniedDnRSP;
            Gravity::EventBus<UpstreamNodeXactDeniedUpRSPEvent<config>>                 OnDeniedUpRSP;
            Gravity::EventBus<UpstreamNodeXactDeniedDnDATEvent<config>>                 OnDeniedDnDAT;
            Gravity::EventBus<UpstreamNodeXactDeniedUpDATEvent<config>>                 OnDeniedUpDAT;
            
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

        public:
            std::optional<uint64_t>                 Load64(size_t alignedOffset) const noexcept;
            std::optional<uint32_t>                 Load32(size_t alignedOffset) const noexcept;
            std::optional<uint16_t>                 Load16(size_t alignedOffset) const noexcept;
            std::optional<uint8_t>                  Load8(size_t alignedOffset) const noexcept;

            bool                                    Store64(size_t alignedOffset, uint64_t value) noexcept;
            bool                                    Store32(size_t alignedOffset, uint32_t value) noexcept;
            bool                                    Store16(size_t alignedOffset, uint16_t value) noexcept;
            bool                                    Store8(size_t alignedOffset, uint8_t value) noexcept;
        
        public:
            bool                                    IsEVTInFlight(const Xact::Global<config>& glbl) const noexcept;
            bool                                    IsSNPInFlight(const Xact::Global<config>& glbl) const noexcept;
            bool                                    IsREQInFlight(const Xact::Global<config>& glbl) const noexcept;
            
            bool                                    HasREQHazard(const Xact::Global<config>& glbl) const noexcept;
            bool                                    HasSNPHazard(const Xact::Global<config>& glbl) const noexcept;
            bool                                    HasEVTHazard(const Xact::Global<config>& glbl) const noexcept;
            bool                                    HasEVTDataHazard(const Xact::Global<config>& glbl, Flits::DnDAT<config>::dataid_t dataId) const noexcept;
        };

        class CacheLineEventBase {
        protected:
            uint64_t                                PA;
            std::shared_ptr<CacheLine>              cacheLine;

        public:
            CacheLineEventBase(uint64_t PA, std::shared_ptr<CacheLine> cacheLine) noexcept;

        public:
            uint64_t                                GetPA() const noexcept;
            std::shared_ptr<CacheLine>              GetCacheLine() noexcept;
            std::shared_ptr<const CacheLine>        GetCacheLine() const noexcept;
        };

        class GrantedEvent : public CacheLineEventBase {
        public:
            GrantedEvent(uint64_t PA, std::shared_ptr<CacheLine> cacheLine) noexcept;
        };
        
        class EvictedEvent : public CacheLineEventBase {
        public:
            EvictedEvent(uint64_t PA, std::shared_ptr<CacheLine> cacheLine) noexcept;
        };

        class EmittedEvent : public CacheLineEventBase {
        public:
            EmittedEvent(uint64_t PA, std::shared_ptr<CacheLine> cacheLine) noexcept;
        };

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

    protected:
        Xact::Joint<config>                     joint;

        std::unordered_map<uint64_t, std::shared_ptr<CacheLine>>
                                                cacheable;

        std::unordered_map<uint64_t, std::shared_ptr<Xact::Xaction<config>>>
                                                noncacheable;

    protected:
        std::list<std::shared_ptr<CacheLine>>   queueTXREQ;
        std::list<std::shared_ptr<CacheLine>>   queueTXRSP;
        std::list<std::shared_ptr<CacheLine>>   queueTXDAT;

        // TxnID allocation bitmap for upstream-initiated (REQ/EVT) transactions,
        // one bit per ID of the configured total xaction limit
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
            size_t                          xactionLimitEVT = 1,
            size_t                          xactionLimitSNP = 1,
            size_t                          xactionLimitREQ = 1,
            size_t                          xactionLimitTotal = (size_t{1} << config::txnIdWidth),
            bool                            enableSilentEviction = false,
            bool                            enableStrictInitialState = true
        ) noexcept;

    protected:
        std::optional<size_t>                   AllocateTxnID() noexcept;
        void                                    FreeTxnID(size_t txnID) noexcept;

        bool                                    IsEVTInFlight(const CacheLine& cacheLine) const noexcept;
        bool                                    IsSNPInFlight(const CacheLine& cacheLine) const noexcept;
        bool                                    IsREQInFlight(const CacheLine& cacheLine) const noexcept;

        bool                                    HasREQHazard(const CacheLine& cacheLine) const noexcept;
        bool                                    HasSNPHazard(const CacheLine& cacheLine) const noexcept;
        bool                                    HasEVTHazard(const CacheLine& cacheLine) const noexcept;
        bool                                    HasEVTDataHazard(const CacheLine& cacheLine, Flits::DnDAT<config>::dataid_t dataId) const noexcept;
        
    public:
        bool                                    IsValid(uint64_t PA) const noexcept;
        CacheStateEnum                          GetState(uint64_t PA) const noexcept;
        std::shared_ptr<CacheLine>              GetCacheLine(uint64_t PA) const noexcept;

    protected:
        void                                    SetCacheLine(uint64_t PA, std::shared_ptr<CacheLine> cacheLine) noexcept;

    public:
        std::shared_ptr<FutureNow<GrantedEvent>>    DoLoad(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<GrantedEvent>>    DoStore(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<GrantedEvent>>    DoStoreLine(uint64_t PA) noexcept;

        std::shared_ptr<FutureNow<EvictedEvent>>    DoEvict(uint64_t PA) noexcept;

        bool                                        DoEvictSilently(uint64_t PA) noexcept;

        std::shared_ptr<FutureNow<EmittedEvent>>    DoPrefetchLoad(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<EmittedEvent>>    DoPrefetchStore(uint64_t PA) noexcept;

        std::shared_ptr<FutureNow<ReadEvent>>       DoCacheableRead(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CompleteEvent>>   DoCacheableWrite(uint64_t PA, const uint64_t data[8]) noexcept;

        std::shared_ptr<FutureNow<ReadEvent>>       DoNonCacheableRead(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CompleteEvent>>   DoNonCacheableWrite(uint64_t PA, const uint64_t data[8]) noexcept;

        std::shared_ptr<FutureNow<CompleteEvent>>   DoCBOClean(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CompleteEvent>>   DoCBOFlush(uint64_t PA) noexcept;
        std::shared_ptr<FutureNow<CompleteEvent>>   DoCBOInval(uint64_t PA) noexcept;

    public:
        void                                    Tick(uint64_t time) noexcept;

    protected:  
        void                                    TickEVT() noexcept;
        void                                    TickSNP() noexcept;
        void                                    TickREQ() noexcept;

    public:
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
        bool                            enableSilentEviction,
        bool                            enableStrictInitialState
    ) noexcept
        : events                    (std::make_shared<EventHub>())
        , xactionLimitEVT           (xactionLimitEVT)
        , xactionLimitSNP           (xactionLimitSNP)
        , xactionLimitREQ           (xactionLimitREQ)
        , xactionLimitTotal         (xactionLimitTotal)
        , joint                     ()
        , cacheable                 ()
        , noncacheable              ()
        , queueTXREQ                ()
        , queueTXRSP                ()
        , queueTXDAT                ()
        , usedTxnID                 (xactionLimitTotal, false)
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
    inline bool UpstreamNode<config>::IsValid(uint64_t PA) const noexcept
    {
        return cacheable.contains(PA >> 3);
    }

    template<FlitConfigurationConcept config>
    inline CacheStateEnum UpstreamNode<config>::GetState(uint64_t PA) const noexcept
    {
        auto it = cacheable.find(PA >> 3);
        if (it == cacheable.end())
            return CacheState::Invalid;

        return it->second->state;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<typename UpstreamNode<config>::CacheLine> UpstreamNode<config>::GetCacheLine(uint64_t PA) const noexcept
    {
        auto it = cacheable.find(PA >> 3);
        if (it == cacheable.end())
            return nullptr;

        return it->second;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::SetCacheLine(uint64_t PA, std::shared_ptr<CacheLine> cacheLine) noexcept
    {
        cacheable[PA >> 3] = cacheLine;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<FutureNow<typename UpstreamNode<config>::GrantedEvent>> UpstreamNode<config>::DoLoad(uint64_t PA) noexcept
    {
        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        bool hazard = false;

        if (cacheLine)
        {
            if (cacheLine->state == CacheState::Shared
             || cacheLine->state == CacheState::UniqueClean
             || cacheLine->state == CacheState::UniqueDirty)
            {
                // TODO: event: LoadHitEvent

                // Immediate hit
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::DONE, GrantedEvent(PA, cacheLine));
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
            cacheLine = std::make_shared<CacheLine>();
            SetCacheLine(PA, cacheLine);
        }

        //
        auto txnID = AllocateTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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
            events->OnREQPreHazardDetection(*this, PA, *cacheLine, reqFlit, hazard);

        if (events)
            events->OnREQPostHazardDetection(*this, PA, *cacheLine, reqFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreHazardPending(*this, PA, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
                cacheLine->pendingREQHazardTXREQ = { reqFlit };

            if (events)
                events->OnREQPostHazardPending(*this, PA, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreChannelPending(*this, PA, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                cacheLine->pendingREQChannelTXREQ = { reqFlit };
                queueTXREQ.push_back(cacheLine);
            }

            if (events)
                events->OnREQPostChannelPending(*this, PA, *cacheLine, reqFlit, denial);            

            if (denial->IsRejected())
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
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
        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        bool hazard = false;

        if (cacheLine)
        {
            if (cacheLine->state == CacheState::UniqueClean
             || cacheLine->state == CacheState::UniqueDirty)
            {
                // TODO: event: StoreHitEvent

                // Immediate hit
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::DONE, GrantedEvent(PA, cacheLine));
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
            cacheLine = std::make_shared<CacheLine>();
            SetCacheLine(PA, cacheLine);
        }

        //
        auto txnID = AllocateTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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
            events->OnREQPreHazardDetection(*this, PA, *cacheLine, reqFlit, hazard);

        if (events)
            events->OnREQPostHazardDetection(*this, PA, *cacheLine, reqFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreHazardPending(*this, PA, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
                cacheLine->pendingREQHazardTXREQ = { reqFlit };

            if (events)
                events->OnREQPostHazardPending(*this, PA, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreChannelPending(*this, PA, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                reqFlit.ExpCompData = cacheLine->state == CacheState::Invalid ? 1 : 0;
                cacheLine->pendingREQChannelTXREQ = { reqFlit };
                queueTXREQ.push_back(cacheLine);
            }

            if (events)
                events->OnREQPostChannelPending(*this, PA, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
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
        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(PA);

        bool hazard = false;

        if (cacheLine)
        {
            if (cacheLine->state == CacheState::UniqueClean
             || cacheLine->state == CacheState::UniqueDirty)
            {
                // TODO: event: StoreHitEvent

                // Immediate hit
                return std::make_shared<FutureNow<GrantedEvent>>(Denial::DONE, GrantedEvent(PA, cacheLine));
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
            cacheLine = std::make_shared<CacheLine>();
            SetCacheLine(PA, cacheLine);
        }

        //
        auto txnID = AllocateTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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
            events->OnREQPreHazardDetection(*this, PA, *cacheLine, reqFlit, hazard);

        if (events)
            events->OnREQPostHazardDetection(*this, PA, *cacheLine, reqFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreHazardPending(*this, PA, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
                cacheLine->pendingREQHazardTXREQ = { reqFlit };

            if (events)
                events->OnREQPostHazardPending(*this, PA, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnREQPreChannelPending(*this, PA, *cacheLine, reqFlit).GetDenial();

            if (!denial->IsRejected())
            {
                cacheLine->pendingREQChannelTXREQ = { reqFlit };
                queueTXREQ.push_back(cacheLine);
            }

            if (events)
                events->OnREQPostChannelPending(*this, PA, *cacheLine, reqFlit, denial);

            if (denial->IsRejected())
                return std::make_shared<FutureNow<GrantedEvent>>(denial);
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
        auto txnID = AllocateTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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
            events->OnEVTPreHazardDetection(*this, PA, *cacheLine, evtFlit, hazard);

        if (events)
            events->OnEVTPostHazardDetection(*this, PA, *cacheLine, evtFlit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnEVTPreHazardPending(*this, PA, *cacheLine, evtFlit).GetDenial();

            if (!denial->IsRejected())
                cacheLine->pendingEVTHazardTXEVT = { evtFlit };

            if (events)
                events->OnEVTPostHazardPending(*this, PA, *cacheLine, evtFlit, denial);

            if (denial->IsRejected())
                return std::make_shared<FutureNow<EvictedEvent>>(denial);
        }
        else
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnEVTPreChannelPending(*this, PA, *cacheLine, evtFlit).GetDenial();

            if (!denial->IsRejected())
                cacheLine->pendingEVTChannelTXEVT = { evtFlit };

            if (events)
                events->OnEVTPostChannelPending(*this, PA, *cacheLine, evtFlit, denial);

            if (denial->IsRejected())
                return std::make_shared<FutureNow<EvictedEvent>>(denial);
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
                    events->OnEVTPreHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXEVT, hazard);

                if (events)
                    events->OnEVTPostHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXEVT, hazard);

                if (hazard)
                    continue;

                if (events)
                    events->OnEVTPreHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXEVT);

                cacheLine.pendingEVTChannelTXEVT = cacheLine.pendingEVTHazardTXEVT;
                cacheLine.pendingEVTHazardTXEVT.reset();

                if (events)
                    events->OnEVTPostHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXEVT);

                CacheStateEnum nextState = CacheState::Invalid;

                if (events)
                    events->OnEVTCacheStatePreDemotion(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXEVT, cacheLine.state, nextState);

                if (events)
                    events->OnEVTCacheStatePostDemotion(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXEVT, cacheLine.state, nextState);

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

                        cacheLine.activeEVTFuture->Fire(EvictedEvent(xactionEvict.GetFirst().flit.evt.Addr, it->second));
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
                                events->OnEVTDataPreHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXDAT0, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXDAT0, hazard);

                            if (!hazard)
                            {
                                if (events)
                                    events->OnEVTDataPreHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXDAT0);

                                cacheLine.pendingEVTChannelTXDAT0 = cacheLine.pendingEVTHazardTXDAT0;
                                cacheLine.pendingEVTHazardTXDAT0.reset();

                                if (events)
                                    events->OnEVTDataPostHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXDAT0);
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
                                events->OnEVTDataPreHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXDAT1, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXDAT1, hazard);

                            if (!hazard)
                            {
                                if (events)
                                    events->OnEVTDataPreHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTHazardTXDAT1);

                                cacheLine.pendingEVTChannelTXDAT1 = cacheLine.pendingEVTHazardTXDAT1;
                                cacheLine.pendingEVTHazardTXDAT1.reset();

                                if (events)
                                    events->OnEVTDataPostHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXDAT1);

                                // EVT TXDAT cannot be actually rejected by events and would try to pend the flit again on next Tick
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
                                events->OnEVTDataPreHazardDetection(*this, it->first << 3, cacheLine, datFlit, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, it->first << 3, cacheLine, datFlit, hazard);

                            if (hazard)
                            {
                                bool cancelled = false;

                                if (events)
                                    cancelled = events->OnEVTDataPreHazardPending(*this, it->first << 3, cacheLine, datFlit).IsCancelled();

                                if (!cancelled)
                                {
                                    cacheLine.pendingEVTHazardTXDAT0 = datFlit;

                                    if (events)
                                        events->OnEVTDataPostHazardPending(*this, it->first << 3, cacheLine, datFlit);
                                }

                                // EVT UpDAT cannot be actually cancelled by events and would try to pend the flit again on next Tick
                            }
                            else
                            {
                                bool cancelled = false;

                                if (events)
                                    cancelled = events->OnEVTDataPreChannelPending(*this, it->first << 3, cacheLine, datFlit).IsCancelled();

                                if (!cancelled)
                                {
                                    cacheLine.pendingEVTChannelTXDAT0 = datFlit;

                                    if (events)
                                        events->OnEVTDataPostChannelPending(*this, it->first << 3, cacheLine, datFlit);
                                }

                                // EVT UpDAT cannot be actually cancelled by events and would try to pend the flit again on next Tick
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
                                events->OnEVTDataPreHazardDetection(*this, it->first << 3, cacheLine, datFlit, hazard);

                            if (events)
                                events->OnEVTDataPostHazardDetection(*this, it->first << 3, cacheLine, datFlit, hazard);

                            if (hazard)
                            {
                                bool cancelled = false;

                                if (events)
                                    cancelled = events->OnEVTDataPreHazardPending(*this, it->first << 3, cacheLine, datFlit).IsCancelled();

                                if (!cancelled)
                                {
                                    cacheLine.pendingEVTHazardTXDAT1 = datFlit;

                                    if (events)
                                        events->OnEVTDataPostHazardPending(*this, it->first << 3, cacheLine, datFlit);
                                }

                                // EVT UpDAT cannot be actually cancelled by events and would try to pend the flit again on next Tick
                            }
                            else
                            {
                                bool cancelled = false;

                                if (events)
                                    cancelled = events->OnEVTDataPreChannelPending(*this, it->first << 3, cacheLine, datFlit).IsCancelled();

                                if (!cancelled)
                                {
                                    cacheLine.pendingEVTChannelTXDAT1 = datFlit;

                                    if (events)
                                        events->OnEVTDataPostChannelPending(*this, it->first << 3, cacheLine, datFlit);
                                }

                                // EVT UpDAT cannot be actually cancelled by events and would try to pend the flit again on next Tick
                            }
                        }
                    }

                    if (xactionWriteBack.IsComplete(glbl) && cacheLine.activeEVTFuture && !cacheLine.activeEVTFuture->Fired())
                    {
                        // TODO: event: EvictCompleteEvent

                        cacheLine.activeEVTFuture->Fire(EvictedEvent(xactionWriteBack.GetFirst().flit.evt.Addr, it->second));
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
                    events->OnSNPPreHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPHazardRXSNP, hazard);
                
                if (events)
                    events->OnSNPPostHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPHazardRXSNP, hazard);

                if (HasSNPHazard(cacheLine))
                    continue;

                auto& flit = *cacheLine.pendingSNPHazardRXSNP;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextSNP(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedSNP(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else 
                {
                    if (events)
                        events->OnDeniedSNP(*this, it->first << 3, cacheLine, denial, xaction, flit);
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
                            break;
                    }

                    if (events)
                        events->OnSNPCacheStatePreDemotion(*this, it->first << 3, cacheLine, flit, cacheLine.state, state);

                    if (events)
                        events->OnSNPCacheStatePostDemotion(*this, it->first << 3, cacheLine, flit, cacheLine.state, state);

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
                            events->OnSNPRespDataPreChannelPending(*this, it->first << 3, cacheLine, datFlit);

                        cacheLine.pendingSNPChannelTXDAT0 = datFlit;

                        if (events)
                            events->OnSNPRespDataPostChannelPending(*this, it->first << 3, cacheLine, datFlit);

                        datFlit.DataID = 1;
                        datFlit.Data[0] = cacheLine.data[4];
                        datFlit.Data[1] = cacheLine.data[5];
                        datFlit.Data[2] = cacheLine.data[6];
                        datFlit.Data[3] = cacheLine.data[7];

                        if (events)
                            events->OnSNPRespDataPreChannelPending(*this, it->first << 3, cacheLine, datFlit);

                        cacheLine.pendingSNPChannelTXDAT1 = datFlit;

                        if (events)
                            events->OnSNPRespDataPostChannelPending(*this, it->first << 3, cacheLine, datFlit);
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
                            events->OnSNPRespPreChannelPending(*this, it->first << 3, cacheLine, rspFlit);

                        cacheLine.pendingSNPChannelTXRSP = rspFlit;

                        if (events)
                            events->OnSNPRespPostChannelPending(*this, it->first << 3, cacheLine, rspFlit);
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
                    events->OnREQPreHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingREQHazardTXREQ, hazard);

                if (events)
                    events->OnREQPostHazardDetection(*this, it->first << 3, cacheLine, *cacheLine.pendingREQHazardTXREQ, hazard);

                if (hazard)
                    continue;

                if (events)
                    events->OnREQPreHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingREQHazardTXREQ);

                cacheLine.pendingREQChannelTXREQ = cacheLine.pendingREQHazardTXREQ;
                cacheLine.pendingREQHazardTXREQ.reset();

                if (events)
                    events->OnREQPostHazardToChannelPending(*this, it->first << 3, cacheLine, *cacheLine.pendingREQChannelTXREQ);
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
                        // TODO: event: ReadCompleteEvent

                        cacheLine.activeREQFuture->Fire(GrantedEvent(xactionCacheableDataless.GetFirst().flit.req.Addr, it->second));
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

                        bool cancelled = false;

                        if (events)
                            cancelled = events->OnREQCompAckPreChannelPending(*this, it->first << 3, cacheLine, rspFlit).IsCancelled();

                        if (!cancelled)
                        {
                            cacheLine.pendingREQChannelTXRSP = rspFlit;

                            if (events)
                                events->OnREQCompAckPostChannelPending(*this, it->first << 3, cacheLine, rspFlit);
                        }

                        // CompAck cannot be actually cancalled by events and would try to pend the flit again on next Tick
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
                        // TODO: event: ReadCompleteEvent

                        cacheLine.activeREQFuture->Fire(GrantedEvent(xactionCacheableAllocatingRead.GetFirst().flit.req.Addr, it->second));
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

                        bool cancelled = false;

                        if (events)
                            cancelled = events->OnREQCompAckPreChannelPending(*this, it->first << 3, cacheLine, rspFlit).IsCancelled();

                        if (!cancelled)
                        {
                            cacheLine.pendingREQChannelTXRSP = rspFlit;

                            if (events)
                                events->OnREQCompAckPostChannelPending(*this, it->first << 3, cacheLine, rspFlit);
                        }

                        // CompAck cannot be actually cancalled by events and would try to pend the flit again on next Tick
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
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingEVTChannelTXEVT)
            {
                if (events)
                    if (events->OnEVTPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXEVT).IsCancelled())
                        continue;

                if (events)
                    events->OnEVTPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXEVT);

                return { *cacheLine.pendingEVTChannelTXEVT };
            }
        }        

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::EVT<config>> UpstreamNode<config>::PopTXEVT() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingEVTChannelTXEVT)
            {
                auto& flit = *cacheLine.pendingEVTChannelTXEVT;

                if (events)
                    if (events->OnEVTPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnEVTPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextEVT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedEVT(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedEVT(*this, it->first << 3, cacheLine, denial, xaction, flit);

                    FreeTxnID(flit.TxnID);
                }

                cacheLine.activeEVT = xaction;
                cacheLine.pendingEVTChannelTXEVT.reset();

                return { flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::REQ<config>> UpstreamNode<config>::PeekTXREQ() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingREQChannelTXREQ)
            {
                if (events)
                    if (events->OnREQPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingREQChannelTXREQ).IsCancelled())
                        continue;

                if (events)
                    events->OnREQPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingREQChannelTXREQ);

                return { *cacheLine.pendingREQChannelTXREQ };
            }
        }        

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::REQ<config>> UpstreamNode<config>::PopTXREQ() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingREQChannelTXREQ)
            {
                auto& flit = *cacheLine.pendingREQChannelTXREQ;

                if (events)
                    if (events->OnREQPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnREQPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextREQ(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedREQ(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedREQ(*this, it->first << 3, cacheLine, denial, xaction, flit);

                    FreeTxnID(flit.TxnID);
                }

                cacheLine.activeREQ = xaction;
                cacheLine.pendingREQChannelTXREQ.reset();

                return { flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::PushRXSNP(const Flits::SNP<config>& snpFlit) noexcept
    {
        Flits::SNP<config> flit = snpFlit;

        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(flit.Addr << 3);

        if (cacheLine)
        {
            if (IsSNPInFlight(*cacheLine))
            {
                // SNP with same PA in-flight, cannot accept new request
                return false;
            }
        }
        else
        {
            cacheLine = std::make_shared<CacheLine>();
            SetCacheLine(flit.Addr << 3, cacheLine);
        }

        bool hazard = HasSNPHazard(*cacheLine);

        if (events)
            events->OnSNPPreHazardDetection(*this, flit.Addr << 3, *cacheLine, flit, hazard);

        if (events)
            events->OnSNPPostHazardDetection(*this, flit.Addr << 3, *cacheLine, flit, hazard);

        if (hazard)
        {
            DenialEnum denial = Denial::ACCEPTED;

            if (events)
                denial = events->OnSNPPreHazardPending(*this, flit.Addr << 3, *cacheLine, flit).GetDenial();

            if (!denial->IsRejected())
                cacheLine->pendingSNPHazardRXSNP = flit;

            if (events)
                events->OnSNPPostHazardPending(*this, flit.Addr << 3, *cacheLine, flit, denial);

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
                    events->OnAcceptedSNP(*this, flit.Addr << 3, *cacheLine, xaction, flit);
            }
            else
            {
                if (events)
                    events->OnDeniedSNP(*this, flit.Addr << 3, *cacheLine, denial, xaction, flit);
            }

            cacheLine->activeSNP = xaction;
        }

        return true;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpRSP<config>> UpstreamNode<config>::PeekTXRSP() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingSNPChannelTXRSP)
            {
                if (events)
                    if (events->OnSNPUpRSPPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPChannelTXRSP).IsCancelled())
                        continue;

                if (events)
                    events->OnSNPUpRSPPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPChannelTXRSP);

                return { *cacheLine.pendingSNPChannelTXRSP };
            }

            if (cacheLine.pendingREQChannelTXRSP)
            {
                if (events)
                    if (events->OnREQUpRSPPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingREQChannelTXRSP).IsCancelled())
                        continue;

                if (events)
                    events->OnREQUpRSPPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingREQChannelTXRSP);

                return { *cacheLine.pendingREQChannelTXRSP };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpRSP<config>> UpstreamNode<config>::PopTXRSP() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingSNPChannelTXRSP)
            {
                auto& flit = *cacheLine.pendingSNPChannelTXRSP;

                if (events)
                    if (events->OnSNPUpRSPPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnSNPUpRSPPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpRSP(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpRSP(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpRSP(*this, it->first << 3, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingSNPChannelTXRSP.reset();

                return { flit };
            }

            if (cacheLine.pendingREQChannelTXRSP)
            {
                auto& flit = *cacheLine.pendingREQChannelTXRSP;

                if (events)
                    if (events->OnREQUpRSPPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnREQUpRSPPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpRSP(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpRSP(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpRSP(*this, it->first << 3, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingREQChannelTXRSP.reset();

                return { flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpDAT<config>> UpstreamNode<config>::PeekTXDAT() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingSNPChannelTXDAT0)
            {
                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPChannelTXDAT0).IsCancelled())
                        continue;

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPChannelTXDAT0);

                return { *cacheLine.pendingSNPChannelTXDAT0 };
            }

            if (cacheLine.pendingSNPChannelTXDAT1)
            {
                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPChannelTXDAT1).IsCancelled())
                        continue;

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingSNPChannelTXDAT1);

                return { *cacheLine.pendingSNPChannelTXDAT1 };
            }

            if (cacheLine.pendingEVTChannelTXDAT0)
            {
                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXDAT0).IsCancelled())
                        continue;

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXDAT0);

                return { *cacheLine.pendingEVTChannelTXDAT0 };
            }

            if (cacheLine.pendingEVTChannelTXDAT1)
            {
                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXDAT1).IsCancelled())
                        continue;

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, it->first << 3, cacheLine, *cacheLine.pendingEVTChannelTXDAT1);

                return { *cacheLine.pendingEVTChannelTXDAT1 };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpDAT<config>> UpstreamNode<config>::PopTXDAT() noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingSNPChannelTXDAT0)
            {
                auto& flit = *cacheLine.pendingSNPChannelTXDAT0;

                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, it->first << 3, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingSNPChannelTXDAT0.reset();

                return { flit };
            }

            if (cacheLine.pendingSNPChannelTXDAT1)
            {
                auto& flit = *cacheLine.pendingSNPChannelTXDAT1;

                if (events)
                    if (events->OnSNPUpDATPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnSNPUpDATPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, it->first << 3, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingSNPChannelTXDAT1.reset();

                return { flit };
            }

            if (cacheLine.pendingEVTChannelTXDAT0)
            {
                auto& flit = *cacheLine.pendingEVTChannelTXDAT0;

                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, it->first << 3, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingEVTChannelTXDAT0.reset();

                return { flit };
            }

            if (cacheLine.pendingEVTChannelTXDAT1)
            {
                auto& flit = *cacheLine.pendingEVTChannelTXDAT1;

                if (events)
                    if (events->OnEVTUpDATPreChannelChosen(*this, it->first << 3, cacheLine, flit).IsCancelled())
                        continue;

                if (events)
                    events->OnEVTUpDATPostChannelChosen(*this, it->first << 3, cacheLine, flit);

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, flit, &xaction);

                if (denial == XactDenial::ACCEPTED)
                {
                    if (events)
                        events->OnAcceptedUpDAT(*this, it->first << 3, cacheLine, xaction, flit);
                }
                else
                {
                    if (events)
                        events->OnDeniedUpDAT(*this, it->first << 3, cacheLine, denial, xaction, flit);
                }

                cacheLine.pendingEVTChannelTXDAT1.reset();
                
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
                            events->OnAcceptedDnRSP(*this, it->first << 3, cacheLine, xaction, dnrspFlit);

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
                                    break;
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
                                    break;
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
                            events->OnDeniedDnRSP(*this, it->first << 3, cacheLine, denial, xaction, dnrspFlit);
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
                            events->OnAcceptedDnRSP(*this, it->first << 3, cacheLine, xaction, dnrspFlit);
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnRSP(*this, it->first << 3, cacheLine, denial, xaction, dnrspFlit);
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
                            events->OnAcceptedDnRSP(*this, it->first << 3, cacheLine, xaction, dnrspFlit);
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnRSP(*this, it->first << 3, cacheLine, denial, xaction, dnrspFlit);
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
                            events->OnAcceptedDnRSP(*this, it->first << 3, cacheLine, xaction, dnrspFlit);
                    }
                    else
                    {
                        if (events)
                            events->OnDeniedDnRSP(*this, it->first << 3, cacheLine, denial, xaction, dnrspFlit);
                    }

                    return true;
                }
            }

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
                            events->OnAcceptedDnDAT(*this, it->first << 3, cacheLine, xaction, dndatFlit);

                        CacheStateEnum state;

                        if (xaction->GetType() == Xact::XactionType::CacheableAllocatingRead)
                        {
                            std::shared_ptr<Xact::XactionCacheableAllocatingRead<config>> xactionCacheableAllocatingRead
                                = std::static_pointer_cast<Xact::XactionCacheableAllocatingRead<config>>(xaction);

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
                                    break;
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
                            events->OnDeniedDnDAT(*this, it->first << 3, cacheLine, denial, xaction, dndatFlit);
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
            return false;
        }

        return true;
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
        return !event.has_value() && !denial->IsRejected();
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
        this->event = event;

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
    { }
}


// Implementation of: class UpstreamNode::CacheLine
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::CacheLineEventBase::CacheLineEventBase(
        uint64_t PA,
        std::shared_ptr<CacheLine> cacheLine) noexcept
        : PA        (PA)
        , cacheLine (std::move(cacheLine))
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
        return cacheLine;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const typename UpstreamNode<config>::CacheLine>
    UpstreamNode<config>::CacheLineEventBase::GetCacheLine() const noexcept
    {
        return cacheLine;
    }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::GrantedEvent::GrantedEvent(
        uint64_t PA,
        std::shared_ptr<CacheLine> cacheLine) noexcept
        : CacheLineEventBase(PA, std::move(cacheLine))
    { }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::EvictedEvent::EvictedEvent(
        uint64_t PA,
        std::shared_ptr<CacheLine> cacheLine) noexcept
        : CacheLineEventBase(PA, std::move(cacheLine))
    { }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>::EmittedEvent::EmittedEvent(
        uint64_t PA,
        std::shared_ptr<CacheLine> cacheLine) noexcept
        : CacheLineEventBase(PA, std::move(cacheLine))
    { }

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
    inline std::optional<uint64_t> UpstreamNode<config>::CacheLine::Load64(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 8)
            return std::nullopt;

        return { data[alignedOffset] };
    }

    template<FlitConfigurationConcept config>
    inline std::optional<uint32_t> UpstreamNode<config>::CacheLine::Load32(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 16)
            return std::nullopt;

        return { uint32_t(data[alignedOffset >> 1] >> ((alignedOffset & 1) * 32)) };
    }

    template<FlitConfigurationConcept config>
    inline std::optional<uint16_t> UpstreamNode<config>::CacheLine::Load16(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 32)
            return std::nullopt;

        return { uint16_t(data[alignedOffset >> 2] >> ((alignedOffset & 3) * 16)) };
    }

    template<FlitConfigurationConcept config>
    inline std::optional<uint8_t> UpstreamNode<config>::CacheLine::Load8(size_t alignedOffset) const noexcept
    {
        if (this->state == CacheState::Invalid)
            return std::nullopt;

        if (alignedOffset >= 64)
            return std::nullopt;

        return { uint8_t(data[alignedOffset >> 3] >> ((alignedOffset & 7) * 8)) };
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store64(size_t alignedOffset, uint64_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 8)
            return false;

        data[alignedOffset] = value;

        this->state = CacheState::UniqueDirty;

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store32(size_t alignedOffset, uint32_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 16)
            return false;

        uint64_t& lane = data[alignedOffset >> 1];
        const uint64_t mask = uint64_t(0xFFFFFFFF) << ((alignedOffset & 1) * 32);

        lane = (lane & ~mask) | ((uint64_t(value) << ((alignedOffset & 1) * 32)) & mask);

        this->state = CacheState::UniqueDirty;

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store16(size_t alignedOffset, uint16_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 32)
            return false;

        uint64_t& lane = data[alignedOffset >> 2];
        const uint64_t mask = uint64_t(0xFFFF) << ((alignedOffset & 3) * 16);

        lane = (lane & ~mask) | ((uint64_t(value) << ((alignedOffset & 3) * 16)) & mask);

        this->state = CacheState::UniqueDirty;

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::Store8(size_t alignedOffset, uint8_t value) noexcept
    {
        if (this->state != CacheState::UniqueClean && this->state != CacheState::UniqueDirty)
            return false;

        if (alignedOffset >= 64)
            return false;

        uint64_t& lane = data[alignedOffset >> 3];
        const uint64_t mask = uint64_t(0xFF) << ((alignedOffset & 7) * 8);

        lane = (lane & ~mask) | ((uint64_t(value) << ((alignedOffset & 7) * 8)) & mask);

        this->state = CacheState::UniqueDirty;

        return true;
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
    inline bool UpstreamNode<config>::CacheLine::HasREQHazard(const Xact::Global<config>& glbl) const noexcept
    {
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
        else if (IsSNPInFlight(glbl) && activeSNP)
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

        if (activeEVT)
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

        if (pendingSNPHazardRXSNP)
            return true;

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::CacheLine::HasEVTDataHazard(const Xact::Global<config>& glbl, Flits::DnDAT<config>::dataid_t dataId) const noexcept
    {
        if (activeREQ)
        {
            const Xact::XactionCacheableAllocatingRead<config>& xaction
                = static_cast<const Xact::XactionCacheableAllocatingRead<config>&>(*activeREQ);

            if (!xaction.GotComp() && !xaction.GotCompData(dataId))
                return true;
        }

        return false;
    }
}


#endif // __CCHI__CCHI_ICN_TAURUS__COMPONENT
