#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__COMPONENT_EVENTS
#define __CCHI__CCHI_ICN_TAURUS__COMPONENT_EVENTS

#include <memory>

#include "cchi_taurus_component_afx.hpp"
#include "cchi_taurus_denial.hpp"
#include "cchi_taurus_state.hpp"

#include "../../xact/cchi_joint.hpp"            // IWYU pragma: keep


namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    class EVTFlitEventBase {
    protected:
        Flits::EVT<config>&         evtFlit;

    public:
        EVTFlitEventBase(Flits::EVT<config>& evtFlit) noexcept;

    public:
        Flits::EVT<config>&         GetEVTFlit() noexcept;
        const Flits::EVT<config>&   GetEVTFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class SNPFlitEventBase {
    protected:
        Flits::SNP<config>&         snpFlit;
    
    public:
        SNPFlitEventBase(Flits::SNP<config>& snpFlit) noexcept;

    public:
        Flits::SNP<config>&         GetSNPFlit() noexcept;
        const Flits::SNP<config>&   GetSNPFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class REQFlitEventBase {
    protected:
        Flits::REQ<config>&         reqFlit;

    public:
        REQFlitEventBase(Flits::REQ<config>& reqFlit) noexcept;

    public:
        Flits::REQ<config>&         GetREQFlit() noexcept;
        const Flits::REQ<config>&   GetREQFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpRSPFlitEventBase {
    protected:
        Flits::UpRSP<config>&       uprspFlit;

    public:
        UpRSPFlitEventBase(Flits::UpRSP<config>& uprspFlit) noexcept;

    public:
        Flits::UpRSP<config>&       GetUpRSPFlit() noexcept;
        const Flits::UpRSP<config>& GetUpRSPFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class DnRSPFlitEventBase {
    protected:
        Flits::DnRSP<config>&       dnrspFlit;

    public:
        DnRSPFlitEventBase(Flits::DnRSP<config>& dnrspFlit) noexcept;

    public:
        Flits::DnRSP<config>&       GetDnRSPFlit() noexcept;
        const Flits::DnRSP<config>& GetDnRSPFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpDATFlitEventBase {
    protected:
        Flits::UpDAT<config>&       updatFlit;

    public:
        UpDATFlitEventBase(Flits::UpDAT<config>& updatFlit) noexcept;

    public:
        Flits::UpDAT<config>&       GetUpDATFlit() noexcept;
        const Flits::UpDAT<config>& GetUpDATFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class DnDATFlitEventBase {
    protected:
        Flits::DnDAT<config>&       dndatFlit;

    public:
        DnDATFlitEventBase(Flits::DnDAT<config>& dndatFlit) noexcept;

    public:
        Flits::DnDAT<config>&       GetDnDATFlit() noexcept;
        const Flits::DnDAT<config>& GetDnDATFlit() const noexcept;
    };


    class CacheStatePreDemotionEventBase {
    protected:
        CacheStateEnum              prevState;
        CacheStateEnum&             nextState;

    public:
        CacheStatePreDemotionEventBase(CacheStateEnum prevState, CacheStateEnum& nextState) noexcept;

    public:
        CacheStateEnum              GetPrevState() const noexcept;
        CacheStateEnum              GetNextState() const noexcept;

        // The 'nextState' was only allowed when it was the same or a lower state than the 'nextState':
        //  - nextState = Invalid, only SetNextState(Invalid) is allowed
        //  - nextState = Shared, only SetNextState(Invalid) or SetNextState(Shared) is allowed
        //  - nextState = UniqueClean, every SetNextState() is allowed
        //  - nextState = UniqueDirty, every SetNextState() is allowed
        bool                        SetNextState(CacheStateEnum nextState) noexcept;
    };

    class CacheStatePostDemotionEventBase {
    protected:
        CacheStateEnum              prevState;
        CacheStateEnum              nextState;

    public:
        CacheStatePostDemotionEventBase(CacheStateEnum prevState, CacheStateEnum nextState) noexcept;

    public:
        CacheStateEnum              GetPrevState() const noexcept;
        CacheStateEnum              GetNextState() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeEventBase {
    protected:
        UpstreamNode<config>&       upstream;

    public:
        UpstreamNodeEventBase(UpstreamNode<config>& upstream) noexcept;

    public:
        UpstreamNode<config>&       GetUpstream() noexcept;
        const UpstreamNode<config>& GetUpstream() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeCacheLineEventBase : public UpstreamNodeEventBase<config> {
    protected:
        uint64_t                    PA;
        const typename UpstreamNode<config>::CacheLine&
                                    cacheLine;

    public:
        UpstreamNodeCacheLineEventBase(UpstreamNode<config>&                            upstream, 
                                       uint64_t                                         PA, 
                                       const typename UpstreamNode<config>::CacheLine&  cacheLine) noexcept;
    
    public:
        uint64_t                    GetPA() const noexcept;
        const typename UpstreamNode<config>::CacheLine&
                                    GetCacheLine() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedEventBase : public UpstreamNodeCacheLineEventBase<config> {
    protected:
        XactDenialEnum              denial;
        std::shared_ptr<Xact::Xaction<config>> 
                                    xaction;
    
    public:
        UpstreamNodeXactDeniedEventBase(UpstreamNode<config>&                           upstream, 
                                        uint64_t                                        PA, 
                                        const typename UpstreamNode<config>::CacheLine& cacheLine,
                                        XactDenialEnum                                  denial,
                                        std::shared_ptr<Xact::Xaction<config>>          xaction) noexcept;

    public:
        XactDenialEnum              GetDenial() const noexcept;
        std::shared_ptr<Xact::Xaction<config>>
                                    GetXaction() noexcept;
        std::shared_ptr<const Xact::Xaction<config>>
                                    GetXaction() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedEventBase : public UpstreamNodeCacheLineEventBase<config> {
    protected:
        std::shared_ptr<Xact::Xaction<config>> 
                                    xaction;
    
    public:
        UpstreamNodeXactAcceptedEventBase(UpstreamNode<config>&                             upstream,
                                          uint64_t                                          PA, 
                                          const typename UpstreamNode<config>::CacheLine&   cacheLine,
                                          std::shared_ptr<Xact::Xaction<config>>            xaction) noexcept;

    public:
        std::shared_ptr<Xact::Xaction<config>>
                                    GetXaction() noexcept;
        std::shared_ptr<const Xact::Xaction<config>>
                                    GetXaction() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedSNPEvent : public UpstreamNodeXactDeniedEventBase<config>
                                         , public Gravity::Event<UpstreamNodeXactDeniedSNPEvent<config>> {
    protected:
        const Flits::SNP<config>&   snpFlit;

    public:
        UpstreamNodeXactDeniedSNPEvent(UpstreamNode<config>&                            upstream,
                                       uint64_t                                         PA, 
                                       const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                       XactDenialEnum                                   denial,
                                       std::shared_ptr<Xact::Xaction<config>>           xaction,
                                       const Flits::SNP<config>&                        snpFlit) noexcept;

    public:
        const Flits::SNP<config>&   GetSNPFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedSNPEvent : public UpstreamNodeXactAcceptedEventBase<config>
                                           , public Gravity::Event<UpstreamNodeXactAcceptedSNPEvent<config>> {
    protected:
        const Flits::SNP<config>&   snpFlit;

    public:
        UpstreamNodeXactAcceptedSNPEvent(UpstreamNode<config>&                            upstream,
                                         uint64_t                                         PA, 
                                         const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                         std::shared_ptr<Xact::Xaction<config>>           xaction,
                                         const Flits::SNP<config>&                        snpFlit) noexcept;

    public:
        const Flits::SNP<config>&   GetSNPFlit() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedEVTEvent : public UpstreamNodeXactDeniedEventBase<config>
                                         , public Gravity::Event<UpstreamNodeXactDeniedEVTEvent<config>> {
    protected:
        const Flits::EVT<config>&   evtFlit;

    public:
        UpstreamNodeXactDeniedEVTEvent(UpstreamNode<config>&                            upstream,
                                       uint64_t                                         PA, 
                                       const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                       XactDenialEnum                                   denial,
                                       std::shared_ptr<Xact::Xaction<config>>           xaction,
                                       const Flits::EVT<config>&                        evtFlit) noexcept;

    public:
        const Flits::EVT<config>&   GetEVTFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedEVTEvent : public UpstreamNodeXactAcceptedEventBase<config>
                                           , public Gravity::Event<UpstreamNodeXactAcceptedEVTEvent<config>> {
    protected:
        const Flits::EVT<config>&   evtFlit;

    public:
        UpstreamNodeXactAcceptedEVTEvent(UpstreamNode<config>&                            upstream,
                                         uint64_t                                         PA, 
                                         const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                         std::shared_ptr<Xact::Xaction<config>>           xaction,
                                         const Flits::EVT<config>&                        evtFlit) noexcept;

    public:
        const Flits::EVT<config>&   GetEVTFlit() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedREQEvent : public UpstreamNodeXactDeniedEventBase<config>
                                         , public Gravity::Event<UpstreamNodeXactDeniedREQEvent<config>> {
    protected:
        const Flits::REQ<config>&   reqFlit;

    public:
        UpstreamNodeXactDeniedREQEvent(UpstreamNode<config>&                            upstream,
                                       uint64_t                                         PA,
                                       const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                       XactDenialEnum                                   denial,
                                       std::shared_ptr<Xact::Xaction<config>>           xaction,
                                       const Flits::REQ<config>&                        reqFlit) noexcept;

    public:
        const Flits::REQ<config>&   GetREQFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedREQEvent : public UpstreamNodeXactAcceptedEventBase<config>
                                           , public Gravity::Event<UpstreamNodeXactAcceptedREQEvent<config>> {
    protected:
        const Flits::REQ<config>&   reqFlit;

    public:
        UpstreamNodeXactAcceptedREQEvent(UpstreamNode<config>&                            upstream,
                                         uint64_t                                         PA,
                                         const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                         std::shared_ptr<Xact::Xaction<config>>           xaction,
                                         const Flits::REQ<config>&                        reqFlit) noexcept;

    public:
        const Flits::REQ<config>&   GetREQFlit() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedDnRSPEvent : public UpstreamNodeXactDeniedEventBase<config>
                                           , public Gravity::Event<UpstreamNodeXactDeniedDnRSPEvent<config>> {
    protected:
        const Flits::DnRSP<config>& dnrspFlit;

    public:
        UpstreamNodeXactDeniedDnRSPEvent(UpstreamNode<config>&                            upstream,
                                         uint64_t                                         PA,
                                         const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                         XactDenialEnum                                   denial,
                                         std::shared_ptr<Xact::Xaction<config>>           xaction,
                                         const Flits::DnRSP<config>&                      dnrspFlit) noexcept;

    public:
        const Flits::DnRSP<config>& GetDnRSPFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedDnRSPEvent : public UpstreamNodeXactAcceptedEventBase<config>
                                             , public Gravity::Event<UpstreamNodeXactAcceptedDnRSPEvent<config>> {
    protected:
        const Flits::DnRSP<config>& dnrspFlit;

    public:
        UpstreamNodeXactAcceptedDnRSPEvent(UpstreamNode<config>&                            upstream,
                                           uint64_t                                         PA,
                                           const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                           std::shared_ptr<Xact::Xaction<config>>           xaction,
                                           const Flits::DnRSP<config>&                      dnrspFlit) noexcept;

    public:
        const Flits::DnRSP<config>& GetDnRSPFlit() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedUpRSPEvent : public UpstreamNodeXactDeniedEventBase<config>
                                           , public Gravity::Event<UpstreamNodeXactDeniedUpRSPEvent<config>> {
    protected:
        const Flits::UpRSP<config>& uprspFlit;

    public:
        UpstreamNodeXactDeniedUpRSPEvent(UpstreamNode<config>&                            upstream,
                                         uint64_t                                         PA,
                                         const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                         XactDenialEnum                                   denial,
                                         std::shared_ptr<Xact::Xaction<config>>           xaction,
                                         const Flits::UpRSP<config>&                      uprspFlit) noexcept;

    public:
        const Flits::UpRSP<config>& GetUpRSPFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedUpRSPEvent : public UpstreamNodeXactAcceptedEventBase<config>
                                             , public Gravity::Event<UpstreamNodeXactAcceptedUpRSPEvent<config>> {
    protected:
        const Flits::UpRSP<config>& uprspFlit;

    public:
        UpstreamNodeXactAcceptedUpRSPEvent(UpstreamNode<config>&                            upstream,
                                           uint64_t                                         PA,
                                           const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                           std::shared_ptr<Xact::Xaction<config>>           xaction,
                                           const Flits::UpRSP<config>&                      uprspFlit) noexcept;

    public:
        const Flits::UpRSP<config>& GetUpRSPFlit() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedDnDATEvent : public UpstreamNodeXactDeniedEventBase<config>
                                           , public Gravity::Event<UpstreamNodeXactDeniedDnDATEvent<config>> {
    protected:
        const Flits::DnDAT<config>& dndatFlit;

    public:
        UpstreamNodeXactDeniedDnDATEvent(UpstreamNode<config>&                            upstream,
                                         uint64_t                                         PA,
                                         const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                         XactDenialEnum                                   denial,
                                         std::shared_ptr<Xact::Xaction<config>>           xaction,
                                         const Flits::DnDAT<config>&                      dndatFlit) noexcept;

    public:
        const Flits::DnDAT<config>& GetDnDATFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedDnDATEvent : public UpstreamNodeXactAcceptedEventBase<config>
                                             , public Gravity::Event<UpstreamNodeXactAcceptedDnDATEvent<config>> {
    protected:
        const Flits::DnDAT<config>& dndatFlit;

    public:
        UpstreamNodeXactAcceptedDnDATEvent(UpstreamNode<config>&                            upstream,
                                           uint64_t                                         PA,
                                           const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                           std::shared_ptr<Xact::Xaction<config>>           xaction,
                                           const Flits::DnDAT<config>&                      dndatFlit) noexcept;

    public:
        const Flits::DnDAT<config>& GetDnDATFlit() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeXactDeniedUpDATEvent : public UpstreamNodeXactDeniedEventBase<config>
                                           , public Gravity::Event<UpstreamNodeXactDeniedUpDATEvent<config>> {
    protected:
        const Flits::UpDAT<config>& updatFlit;

    public:
        UpstreamNodeXactDeniedUpDATEvent(UpstreamNode<config>&                            upstream,
                                         uint64_t                                         PA,
                                         const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                         XactDenialEnum                                   denial,
                                         std::shared_ptr<Xact::Xaction<config>>           xaction,
                                         const Flits::UpDAT<config>&                      updatFlit) noexcept;

    public:
        const Flits::UpDAT<config>& GetUpDATFlit() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeXactAcceptedUpDATEvent : public UpstreamNodeXactAcceptedEventBase<config>
                                             , public Gravity::Event<UpstreamNodeXactAcceptedUpDATEvent<config>> {
    protected:
        const Flits::UpDAT<config>& updatFlit;

    public:
        UpstreamNodeXactAcceptedUpDATEvent(UpstreamNode<config>&                            upstream,
                                           uint64_t                                         PA,
                                           const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                           std::shared_ptr<Xact::Xaction<config>>           xaction,
                                           const Flits::UpDAT<config>&                      updatFlit) noexcept;

    public:
        const Flits::UpDAT<config>& GetUpDATFlit() const noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPreHazardDetectionEvent : public EVTFlitEventBase<config>
                                                 , public UpstreamNodeCacheLineEventBase<config>
                                                 , public Gravity::Event<UpstreamNodeEVTPreHazardDetectionEvent<config>> {
    protected:
        bool&   hazard;

    public:
        UpstreamNodeEVTPreHazardDetectionEvent(UpstreamNode<config>&                            upstream,
                                               uint64_t                                         PA,
                                               const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                               Flits::EVT<config>&                              evtFlit,
                                               bool&                                            hazard) noexcept;

    public:
        bool    HasHazard() const noexcept;
        void    SetHazard() noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPostHazardDetectionEvent : public EVTFlitEventBase<config>
                                                  , public UpstreamNodeCacheLineEventBase<config>
                                                  , public Gravity::Event<UpstreamNodeEVTPostHazardDetectionEvent<config>> {
    protected:
        bool    hazard;

    public:
        UpstreamNodeEVTPostHazardDetectionEvent(UpstreamNode<config>&                            upstream,
                                                uint64_t                                         PA,
                                                const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                Flits::EVT<config>&                              evtFlit,
                                                bool                                             hazard) noexcept;

    public:
        bool    HasHazard() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPreHazardPendingEvent : public EVTFlitEventBase<config>
                                               , public UpstreamNodeCacheLineEventBase<config>
                                               , public Gravity::Event<UpstreamNodeEVTPreHazardPendingEvent<config>> {
    protected:
        DenialEnum  denial  = Denial::ACCEPTED;

    public:
        UpstreamNodeEVTPreHazardPendingEvent(UpstreamNode<config>&                            upstream,
                                             uint64_t                                         PA,
                                             const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                             Flits::EVT<config>&                              evtFlit) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;

        void        SetDenial(DenialEnum denial) noexcept;
        void        Deny(DenialEnum denial = Denial::REJECTED_TAURUS_EVENT) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPostHazardPendingEvent : public EVTFlitEventBase<config>
                                                , public UpstreamNodeCacheLineEventBase<config>
                                                , public Gravity::Event<UpstreamNodeEVTPostHazardPendingEvent<config>> {
    protected:
        DenialEnum  denial;

    public:
        UpstreamNodeEVTPostHazardPendingEvent(UpstreamNode<config>&                            upstream,
                                              uint64_t                                         PA,
                                              const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                              Flits::EVT<config>&                              evtFlit,
                                              DenialEnum                                       denial) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPreChannelPendingEvent : public EVTFlitEventBase<config>
                                                , public UpstreamNodeCacheLineEventBase<config>
                                                , public Gravity::Event<UpstreamNodeEVTPreChannelPendingEvent<config>> {
    protected:
        DenialEnum  denial  = Denial::ACCEPTED;

    public:
        UpstreamNodeEVTPreChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                              uint64_t                                         PA,
                                              const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                              Flits::EVT<config>&                              evtFlit) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;

        void        SetDenial(DenialEnum denial) noexcept;
        void        Deny(DenialEnum denial = Denial::REJECTED_TAURUS_EVENT) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPostChannelPendingEvent : public EVTFlitEventBase<config>
                                                 , public UpstreamNodeCacheLineEventBase<config>
                                                 , public Gravity::Event<UpstreamNodeEVTPostChannelPendingEvent<config>> {
    protected:
        DenialEnum  denial;

    public:
        UpstreamNodeEVTPostChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                               uint64_t                                         PA,
                                               const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                               Flits::EVT<config>&                              evtFlit,
                                               DenialEnum                                       denial) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPreHazardToChannelPendingEvent : public EVTFlitEventBase<config>
                                                        , public UpstreamNodeCacheLineEventBase<config>
                                                        , public Gravity::Event<UpstreamNodeEVTPreHazardToChannelPendingEvent<config>> {
    public:
        UpstreamNodeEVTPreHazardToChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                                      uint64_t                                         PA,
                                                      const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                      Flits::EVT<config>&                              evtFlit) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPostHazardToChannelPendingEvent : public EVTFlitEventBase<config>
                                                         , public UpstreamNodeCacheLineEventBase<config>
                                                         , public Gravity::Event<UpstreamNodeEVTPostHazardToChannelPendingEvent<config>> {
    public:
        UpstreamNodeEVTPostHazardToChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                                       uint64_t                                         PA,
                                                       const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                       Flits::EVT<config>&                              evtFlit) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTCacheStatePreDemotionEvent : public EVTFlitEventBase<config>
                                                    , public CacheStatePreDemotionEventBase
                                                    , public UpstreamNodeCacheLineEventBase<config>
                                                    , public Gravity::Event<UpstreamNodeEVTCacheStatePreDemotionEvent<config>> {
    public:
        UpstreamNodeEVTCacheStatePreDemotionEvent(UpstreamNode<config>&                            upstream,
                                                  uint64_t                                         PA,
                                                  const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                  Flits::EVT<config>&                              evtFlit,
                                                  CacheStateEnum                                   prevState,
                                                  CacheStateEnum&                                  nextState) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTCacheStatePostDemotionEvent : public EVTFlitEventBase<config>
                                                     , public CacheStatePostDemotionEventBase
                                                     , public UpstreamNodeCacheLineEventBase<config>
                                                     , public Gravity::Event<UpstreamNodeEVTCacheStatePostDemotionEvent<config>> {
    public:
        UpstreamNodeEVTCacheStatePostDemotionEvent(UpstreamNode<config>&                            upstream,
                                                   uint64_t                                         PA,
                                                   const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                   Flits::EVT<config>&                              evtFlit,
                                                   CacheStateEnum                                   prevState,
                                                   CacheStateEnum                                   nextState) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPreHazardDetectionEvent : public UpDATFlitEventBase<config>
                                                     , public UpstreamNodeCacheLineEventBase<config>
                                                     , public Gravity::Event<UpstreamNodeEVTDataPreHazardDetectionEvent<config>> {
    protected:
        bool&   hazard;

    public:
        UpstreamNodeEVTDataPreHazardDetectionEvent(UpstreamNode<config>&                            upstream,
                                                   uint64_t                                         PA,
                                                   const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                   Flits::UpDAT<config>&                            updatFlit,
                                                   bool&                                            hazard) noexcept;

    public:
        bool    HasHazard() const noexcept;
        void    SetHazard() noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPostHazardDetectionEvent : public UpDATFlitEventBase<config>
                                                      , public UpstreamNodeCacheLineEventBase<config>
                                                      , public Gravity::Event<UpstreamNodeEVTDataPostHazardDetectionEvent<config>> {
    protected:
        bool    hazard;

    public:
        UpstreamNodeEVTDataPostHazardDetectionEvent(UpstreamNode<config>&                            upstream,
                                                    uint64_t                                         PA,
                                                    const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                    Flits::UpDAT<config>&                            updatFlit,
                                                    bool                                             hazard) noexcept;

    public:
        bool    HasHazard() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPreHazardPendingEvent : public UpDATFlitEventBase<config>
                                                   , public UpstreamNodeCacheLineEventBase<config>
                                                   , public Gravity::Event<UpstreamNodeEVTDataPreHazardPendingEvent<config>> {
    protected:
        DenialEnum  denial  = Denial::ACCEPTED;

    public:
        UpstreamNodeEVTDataPreHazardPendingEvent(UpstreamNode<config>&                            upstream,
                                                 uint64_t                                         PA,
                                                 const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                 Flits::UpDAT<config>&                            updatFlit) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;

        void        SetDenial(DenialEnum denial) noexcept;
        void        Deny(DenialEnum denial = Denial::REJECTED_TAURUS_EVENT) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPostHazardPendingEvent : public UpDATFlitEventBase<config>
                                                    , public UpstreamNodeCacheLineEventBase<config>
                                                    , public Gravity::Event<UpstreamNodeEVTDataPostHazardPendingEvent<config>> {
    protected:
        DenialEnum  denial;

    public:
        UpstreamNodeEVTDataPostHazardPendingEvent(UpstreamNode<config>&                            upstream,
                                                  uint64_t                                         PA,
                                                  const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                  Flits::UpDAT<config>&                            updatFlit,
                                                  DenialEnum                                       denial) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPreChannelPendingEvent : public UpDATFlitEventBase<config>
                                                    , public UpstreamNodeCacheLineEventBase<config>
                                                    , public Gravity::Event<UpstreamNodeEVTDataPreChannelPendingEvent<config>> {
    protected:
        DenialEnum  denial  = Denial::ACCEPTED;

    public:
        UpstreamNodeEVTDataPreChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                                  uint64_t                                         PA,
                                                  const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                  Flits::UpDAT<config>&                            updatFlit) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;

        void        SetDenial(DenialEnum denial) noexcept;
        void        Deny(DenialEnum denial = Denial::REJECTED_TAURUS_EVENT) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPostChannelPendingEvent : public UpDATFlitEventBase<config>
                                                     , public UpstreamNodeCacheLineEventBase<config>
                                                     , public Gravity::Event<UpstreamNodeEVTDataPostChannelPendingEvent<config>> {
    protected:
        DenialEnum  denial;

    public:
        UpstreamNodeEVTDataPostChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                                   uint64_t                                         PA,
                                                   const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                   Flits::UpDAT<config>&                            updatFlit,
                                                   DenialEnum                                       denial) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPreHazardToChannelPendingEvent : public UpDATFlitEventBase<config>
                                                            , public UpstreamNodeCacheLineEventBase<config>
                                                            , public Gravity::Event<UpstreamNodeEVTDataPreHazardToChannelPendingEvent<config>> {
    public:
        UpstreamNodeEVTDataPreHazardToChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                                          uint64_t                                         PA,
                                                          const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                          Flits::UpDAT<config>&                            updatFlit) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTDataPostHazardToChannelPendingEvent : public UpDATFlitEventBase<config>
                                                             , public UpstreamNodeCacheLineEventBase<config>
                                                             , public Gravity::Event<UpstreamNodeEVTDataPostHazardToChannelPendingEvent<config>> {
    public:
        UpstreamNodeEVTDataPostHazardToChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                                           uint64_t                                         PA,
                                                           const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                           Flits::UpDAT<config>&                            updatFlit) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPreChannelChosenEvent : public Gravity::CancellableEvent
                                               , public EVTFlitEventBase<config>
                                               , public UpstreamNodeCacheLineEventBase<config>
                                               , public Gravity::Event<UpstreamNodeEVTPreChannelChosenEvent<config>> {
    public:
        UpstreamNodeEVTPreChannelChosenEvent(UpstreamNode<config>&                            upstream,
                                             uint64_t                                         PA,
                                             const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                             Flits::EVT<config>&                              evtFlit) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTPostChannelChosenEvent : public EVTFlitEventBase<config>
                                                , public UpstreamNodeCacheLineEventBase<config>
                                                , public Gravity::Event<UpstreamNodeEVTPostChannelChosenEvent<config>> {
    public:
        UpstreamNodeEVTPostChannelChosenEvent(UpstreamNode<config>&                            upstream,
                                              uint64_t                                         PA,
                                              const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                              Flits::EVT<config>&                              evtFlit) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTUpDATPreChannelChosenEvent : public Gravity::CancellableEvent
                                                    , public UpDATFlitEventBase<config>
                                                    , public UpstreamNodeCacheLineEventBase<config>
                                                    , public Gravity::Event<UpstreamNodeEVTUpDATPreChannelChosenEvent<config>> {
    public:
        UpstreamNodeEVTUpDATPreChannelChosenEvent(UpstreamNode<config>&                            upstream,
                                                  uint64_t                                         PA,
                                                  const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                  Flits::UpDAT<config>&                            updatFlit) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeEVTUpDATPostChannelChosenEvent : public UpDATFlitEventBase<config>
                                                     , public UpstreamNodeCacheLineEventBase<config>
                                                     , public Gravity::Event<UpstreamNodeEVTUpDATPostChannelChosenEvent<config>> {
    public:
        UpstreamNodeEVTUpDATPostChannelChosenEvent(UpstreamNode<config>&                            upstream,
                                                   uint64_t                                         PA,
                                                   const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                   Flits::UpDAT<config>&                            updatFlit) noexcept;
    };


    template<FlitConfigurationConcept config>
    class UpstreamNodeREQPreHazardDetectionEvent : public REQFlitEventBase<config>
                                                 , public UpstreamNodeCacheLineEventBase<config>
                                                 , public Gravity::Event<UpstreamNodeREQPreHazardDetectionEvent<config>> {
    protected:
        bool&   hazard;

    public:
        UpstreamNodeREQPreHazardDetectionEvent(UpstreamNode<config>&                            upstream,
                                               uint64_t                                         PA,
                                               const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                               Flits::REQ<config>&                              reqFlit,
                                               bool&                                            hazard) noexcept;

    public:
        bool    HasHazard() const noexcept;
        void    SetHazard() noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeREQPostHazardDetectionEvent : public REQFlitEventBase<config>
                                                  , public UpstreamNodeCacheLineEventBase<config>
                                                  , public Gravity::Event<UpstreamNodeREQPostHazardDetectionEvent<config>> {
    protected:
        bool    hazard;

    public:
        UpstreamNodeREQPostHazardDetectionEvent(UpstreamNode<config>&                            upstream,
                                                uint64_t                                         PA,
                                                const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                                Flits::REQ<config>&                              reqFlit,
                                                bool                                             hazard) noexcept;

    public:
        bool    HasHazard() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeREQPreHazardPendingEvent : public REQFlitEventBase<config>
                                               , public UpstreamNodeCacheLineEventBase<config>
                                               , public Gravity::Event<UpstreamNodeREQPreHazardPendingEvent<config>> {
    protected:
        DenialEnum  denial  = Denial::ACCEPTED;

    public:
        UpstreamNodeREQPreHazardPendingEvent(UpstreamNode<config>&                            upstream,
                                             uint64_t                                         PA,
                                             const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                             Flits::REQ<config>&                              reqFlit) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;

        void        SetDenial(DenialEnum denial) noexcept;
        void        Deny(DenialEnum denial = Denial::REJECTED_TAURUS_EVENT) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeREQPostHazardPendingEvent : public REQFlitEventBase<config>
                                                , public UpstreamNodeCacheLineEventBase<config>
                                                , public Gravity::Event<UpstreamNodeREQPostHazardPendingEvent<config>> {
    protected:
        DenialEnum  denial;

    public:
        UpstreamNodeREQPostHazardPendingEvent(UpstreamNode<config>&                            upstream,
                                              uint64_t                                         PA,
                                              const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                              Flits::REQ<config>&                              reqFlit,
                                              DenialEnum                                       denial) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeREQPreChannelPendingEvent : public REQFlitEventBase<config>
                                                 , public UpstreamNodeCacheLineEventBase<config>
                                                 , public Gravity::Event<UpstreamNodeREQPreChannelPendingEvent<config>> {
    protected:
        DenialEnum  denial  = Denial::ACCEPTED;

    public:
        UpstreamNodeREQPreChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                              uint64_t                                         PA,
                                              const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                              Flits::REQ<config>&                              reqFlit) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;

        void        SetDenial(DenialEnum denial) noexcept;
        void        Deny(DenialEnum denial = Denial::REJECTED_TAURUS_EVENT) noexcept;
    };

    template<FlitConfigurationConcept config>
    class UpstreamNodeREQPostChannelPendingEvent : public REQFlitEventBase<config>
                                                  , public UpstreamNodeCacheLineEventBase<config>
                                                  , public Gravity::Event<UpstreamNodeREQPostChannelPendingEvent<config>> {
    protected:
        DenialEnum  denial;

    public:
        UpstreamNodeREQPostChannelPendingEvent(UpstreamNode<config>&                            upstream,
                                               uint64_t                                         PA,
                                               const typename UpstreamNode<config>::CacheLine&  cacheLine,
                                               Flits::REQ<config>&                              reqFlit,
                                               DenialEnum                                       denial) noexcept;

    public:
        DenialEnum  GetDenial() const noexcept;
        bool        IsDenied() const noexcept;
    };
}


// Implementation of: class EVTFlitEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline EVTFlitEventBase<config>::EVTFlitEventBase(Flits::EVT<config>& evtFlit) noexcept
        : evtFlit (evtFlit)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::EVT<config>& EVTFlitEventBase<config>::GetEVTFlit() noexcept
    {
        return evtFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::EVT<config>& EVTFlitEventBase<config>::GetEVTFlit() const noexcept
    {
        return evtFlit;
    }
}


// Implementation of: class SNPFlitEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline SNPFlitEventBase<config>::SNPFlitEventBase(Flits::SNP<config>& snpFlit) noexcept
        : snpFlit (snpFlit)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::SNP<config>& SNPFlitEventBase<config>::GetSNPFlit() noexcept
    {
        return snpFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::SNP<config>& SNPFlitEventBase<config>::GetSNPFlit() const noexcept
    {
        return snpFlit;
    }
}


// Implementation of: class REQFlitEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline REQFlitEventBase<config>::REQFlitEventBase(Flits::REQ<config>& reqFlit) noexcept
        : reqFlit (reqFlit)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::REQ<config>& REQFlitEventBase<config>::GetREQFlit() noexcept
    {
        return reqFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::REQ<config>& REQFlitEventBase<config>::GetREQFlit() const noexcept
    {
        return reqFlit;
    }
}


// Implementation of: class UpRSPFlitEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpRSPFlitEventBase<config>::UpRSPFlitEventBase(Flits::UpRSP<config>& uprspFlit) noexcept
        : uprspFlit (uprspFlit)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::UpRSP<config>& UpRSPFlitEventBase<config>::GetUpRSPFlit() noexcept
    {
        return uprspFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::UpRSP<config>& UpRSPFlitEventBase<config>::GetUpRSPFlit() const noexcept
    {
        return uprspFlit;
    }
}


// Implementation of: class DnRSPFlitEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline DnRSPFlitEventBase<config>::DnRSPFlitEventBase(Flits::DnRSP<config>& dnrspFlit) noexcept
        : dnrspFlit (dnrspFlit)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::DnRSP<config>& DnRSPFlitEventBase<config>::GetDnRSPFlit() noexcept
    {
        return dnrspFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::DnRSP<config>& DnRSPFlitEventBase<config>::GetDnRSPFlit() const noexcept
    {
        return dnrspFlit;
    }
}


// Implementation of: class UpDATFlitEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpDATFlitEventBase<config>::UpDATFlitEventBase(Flits::UpDAT<config>& updatFlit) noexcept
        : updatFlit (updatFlit)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::UpDAT<config>& UpDATFlitEventBase<config>::GetUpDATFlit() noexcept
    {
        return updatFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::UpDAT<config>& UpDATFlitEventBase<config>::GetUpDATFlit() const noexcept
    {
        return updatFlit;
    }
}


// Implementation of: class DnDATFlitEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline DnDATFlitEventBase<config>::DnDATFlitEventBase(Flits::DnDAT<config>& dndatFlit) noexcept
        : dndatFlit (dndatFlit)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::DnDAT<config>& DnDATFlitEventBase<config>::GetDnDATFlit() noexcept
    {
        return dndatFlit;
    }

    template<FlitConfigurationConcept config>
    inline const Flits::DnDAT<config>& DnDATFlitEventBase<config>::GetDnDATFlit() const noexcept
    {
        return dndatFlit;
    }
}


// Implementation of: class CacheStatePreDemotionEventBase
namespace CCHI::Taurus {

    inline CacheStatePreDemotionEventBase::CacheStatePreDemotionEventBase(
        CacheStateEnum  prevState,
        CacheStateEnum& nextState) noexcept
        : prevState (prevState)
        , nextState (nextState)
    { }

    inline CacheStateEnum CacheStatePreDemotionEventBase::GetPrevState() const noexcept
    {
        return prevState;
    }

    inline CacheStateEnum CacheStatePreDemotionEventBase::GetNextState() const noexcept
    {
        return nextState;
    }

    inline bool CacheStatePreDemotionEventBase::SetNextState(CacheStateEnum nextState) noexcept
    {
        if (this->nextState == CacheState::UniqueClean
         || this->nextState == CacheState::UniqueDirty
         || nextState->ordinal <= this->nextState->ordinal)
        {
            this->nextState = nextState;
            return true;
        }

        return false;
    }
}


// Implementation of: class CacheStatePostDemotionEventBase
namespace CCHI::Taurus {

    inline CacheStatePostDemotionEventBase::CacheStatePostDemotionEventBase(
        CacheStateEnum prevState,
        CacheStateEnum nextState) noexcept
        : prevState (prevState)
        , nextState (nextState)
    { }

    inline CacheStateEnum CacheStatePostDemotionEventBase::GetPrevState() const noexcept
    {
        return prevState;
    }

    inline CacheStateEnum CacheStatePostDemotionEventBase::GetNextState() const noexcept
    {
        return nextState;
    }
}


// Implementation of: class UpstreamNodeEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEventBase<config>::UpstreamNodeEventBase(UpstreamNode<config>& upstream) noexcept
        : upstream  (upstream)
    { }

    template<FlitConfigurationConcept config>
    inline UpstreamNode<config>& UpstreamNodeEventBase<config>::GetUpstream() noexcept
    {
        return upstream;
    }

    template<FlitConfigurationConcept config>
    inline const UpstreamNode<config>& UpstreamNodeEventBase<config>::GetUpstream() const noexcept
    {
        return upstream;
    }
}


// Implementation of: class UpstreamNodeCacheLineEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeCacheLineEventBase<config>::UpstreamNodeCacheLineEventBase(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine) noexcept
        : UpstreamNodeEventBase<config>(upstream)
        , PA        (PA)
        , cacheLine (cacheLine)
    { }

    template<FlitConfigurationConcept config>
    inline uint64_t UpstreamNodeCacheLineEventBase<config>::GetPA() const noexcept
    {
        return PA;
    }

    template<FlitConfigurationConcept config>
    inline const typename UpstreamNode<config>::CacheLine&
    UpstreamNodeCacheLineEventBase<config>::GetCacheLine() const noexcept
    {
        return cacheLine;
    }
}


// Implementation of: class UpstreamNodeXactDeniedEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedEventBase<config>::UpstreamNodeXactDeniedEventBase(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction) noexcept
        : UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , denial  (denial)
        , xaction (std::move(xaction))
    { }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum UpstreamNodeXactDeniedEventBase<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xact::Xaction<config>>
    UpstreamNodeXactDeniedEventBase<config>::GetXaction() noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const Xact::Xaction<config>>
    UpstreamNodeXactDeniedEventBase<config>::GetXaction() const noexcept
    {
        return xaction;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedEventBase
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedEventBase<config>::UpstreamNodeXactAcceptedEventBase(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction) noexcept
        : UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , xaction (std::move(xaction))
    { }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xact::Xaction<config>>
    UpstreamNodeXactAcceptedEventBase<config>::GetXaction() noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const Xact::Xaction<config>>
    UpstreamNodeXactAcceptedEventBase<config>::GetXaction() const noexcept
    {
        return xaction;
    }
}


// Implementation of: class UpstreamNodeXactDeniedSNPEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedSNPEvent<config>::UpstreamNodeXactDeniedSNPEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::SNP<config>&                       snpFlit) noexcept
        : UpstreamNodeXactDeniedEventBase<config>(upstream, PA, cacheLine, denial, std::move(xaction))
        , snpFlit   (snpFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::SNP<config>& UpstreamNodeXactDeniedSNPEvent<config>::GetSNPFlit() const noexcept
    {
        return snpFlit;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedSNPEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedSNPEvent<config>::UpstreamNodeXactAcceptedSNPEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::SNP<config>&                       snpFlit) noexcept
        : UpstreamNodeXactAcceptedEventBase<config>(upstream, PA, cacheLine, std::move(xaction))
        , snpFlit   (snpFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::SNP<config>& UpstreamNodeXactAcceptedSNPEvent<config>::GetSNPFlit() const noexcept
    {
        return snpFlit;
    }
}


// Implementation of: class UpstreamNodeXactDeniedEVTEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedEVTEvent<config>::UpstreamNodeXactDeniedEVTEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::EVT<config>&                       evtFlit) noexcept
        : UpstreamNodeXactDeniedEventBase<config>(upstream, PA, cacheLine, denial, std::move(xaction))
        , evtFlit   (evtFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::EVT<config>& UpstreamNodeXactDeniedEVTEvent<config>::GetEVTFlit() const noexcept
    {
        return evtFlit;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedEVTEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedEVTEvent<config>::UpstreamNodeXactAcceptedEVTEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::EVT<config>&                       evtFlit) noexcept
        : UpstreamNodeXactAcceptedEventBase<config>(upstream, PA, cacheLine, std::move(xaction))
        , evtFlit   (evtFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::EVT<config>& UpstreamNodeXactAcceptedEVTEvent<config>::GetEVTFlit() const noexcept
    {
        return evtFlit;
    }
}


// Implementation of: class UpstreamNodeXactDeniedREQEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedREQEvent<config>::UpstreamNodeXactDeniedREQEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::REQ<config>&                       reqFlit) noexcept
        : UpstreamNodeXactDeniedEventBase<config>(upstream, PA, cacheLine, denial, std::move(xaction))
        , reqFlit   (reqFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::REQ<config>& UpstreamNodeXactDeniedREQEvent<config>::GetREQFlit() const noexcept
    {
        return reqFlit;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedREQEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedREQEvent<config>::UpstreamNodeXactAcceptedREQEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::REQ<config>&                       reqFlit) noexcept
        : UpstreamNodeXactAcceptedEventBase<config>(upstream, PA, cacheLine, std::move(xaction))
        , reqFlit   (reqFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::REQ<config>& UpstreamNodeXactAcceptedREQEvent<config>::GetREQFlit() const noexcept
    {
        return reqFlit;
    }
}


// Implementation of: class UpstreamNodeXactDeniedDnRSPEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedDnRSPEvent<config>::UpstreamNodeXactDeniedDnRSPEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::DnRSP<config>&                     dnrspFlit) noexcept
        : UpstreamNodeXactDeniedEventBase<config>(upstream, PA, cacheLine, denial, std::move(xaction))
        , dnrspFlit (dnrspFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::DnRSP<config>& UpstreamNodeXactDeniedDnRSPEvent<config>::GetDnRSPFlit() const noexcept
    {
        return dnrspFlit;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedDnRSPEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedDnRSPEvent<config>::UpstreamNodeXactAcceptedDnRSPEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::DnRSP<config>&                     dnrspFlit) noexcept
        : UpstreamNodeXactAcceptedEventBase<config>(upstream, PA, cacheLine, std::move(xaction))
        , dnrspFlit (dnrspFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::DnRSP<config>& UpstreamNodeXactAcceptedDnRSPEvent<config>::GetDnRSPFlit() const noexcept
    {
        return dnrspFlit;
    }
}


// Implementation of: class UpstreamNodeXactDeniedUpRSPEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedUpRSPEvent<config>::UpstreamNodeXactDeniedUpRSPEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::UpRSP<config>&                     uprspFlit) noexcept
        : UpstreamNodeXactDeniedEventBase<config>(upstream, PA, cacheLine, denial, std::move(xaction))
        , uprspFlit (uprspFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::UpRSP<config>& UpstreamNodeXactDeniedUpRSPEvent<config>::GetUpRSPFlit() const noexcept
    {
        return uprspFlit;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedUpRSPEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedUpRSPEvent<config>::UpstreamNodeXactAcceptedUpRSPEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::UpRSP<config>&                     uprspFlit) noexcept
        : UpstreamNodeXactAcceptedEventBase<config>(upstream, PA, cacheLine, std::move(xaction))
        , uprspFlit (uprspFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::UpRSP<config>& UpstreamNodeXactAcceptedUpRSPEvent<config>::GetUpRSPFlit() const noexcept
    {
        return uprspFlit;
    }
}


// Implementation of: class UpstreamNodeXactDeniedDnDATEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedDnDATEvent<config>::UpstreamNodeXactDeniedDnDATEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::DnDAT<config>&                     dndatFlit) noexcept
        : UpstreamNodeXactDeniedEventBase<config>(upstream, PA, cacheLine, denial, std::move(xaction))
        , dndatFlit (dndatFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::DnDAT<config>& UpstreamNodeXactDeniedDnDATEvent<config>::GetDnDATFlit() const noexcept
    {
        return dndatFlit;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedDnDATEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedDnDATEvent<config>::UpstreamNodeXactAcceptedDnDATEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::DnDAT<config>&                     dndatFlit) noexcept
        : UpstreamNodeXactAcceptedEventBase<config>(upstream, PA, cacheLine, std::move(xaction))
        , dndatFlit (dndatFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::DnDAT<config>& UpstreamNodeXactAcceptedDnDATEvent<config>::GetDnDATFlit() const noexcept
    {
        return dndatFlit;
    }
}


// Implementation of: class UpstreamNodeXactDeniedUpDATEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactDeniedUpDATEvent<config>::UpstreamNodeXactDeniedUpDATEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        XactDenialEnum                                  denial,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::UpDAT<config>&                     updatFlit) noexcept
        : UpstreamNodeXactDeniedEventBase<config>(upstream, PA, cacheLine, denial, std::move(xaction))
        , updatFlit (updatFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::UpDAT<config>& UpstreamNodeXactDeniedUpDATEvent<config>::GetUpDATFlit() const noexcept
    {
        return updatFlit;
    }
}


// Implementation of: class UpstreamNodeXactAcceptedUpDATEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeXactAcceptedUpDATEvent<config>::UpstreamNodeXactAcceptedUpDATEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        std::shared_ptr<Xact::Xaction<config>>          xaction,
        const Flits::UpDAT<config>&                     updatFlit) noexcept
        : UpstreamNodeXactAcceptedEventBase<config>(upstream, PA, cacheLine, std::move(xaction))
        , updatFlit (updatFlit)
    { }

    template<FlitConfigurationConcept config>
    inline const Flits::UpDAT<config>& UpstreamNodeXactAcceptedUpDATEvent<config>::GetUpDATFlit() const noexcept
    {
        return updatFlit;
    }
}


// Implementation of: class UpstreamNodeEVTPreHazardDetectionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPreHazardDetectionEvent<config>::UpstreamNodeEVTPreHazardDetectionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit,
        bool&                                           hazard) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , hazard                                (hazard)
    { }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTPreHazardDetectionEvent<config>::HasHazard() const noexcept
    {
        return hazard;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTPreHazardDetectionEvent<config>::SetHazard() noexcept
    {
        hazard = true;
    }
}


// Implementation of: class UpstreamNodeEVTPostHazardDetectionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPostHazardDetectionEvent<config>::UpstreamNodeEVTPostHazardDetectionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit,
        bool                                            hazard) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , hazard                                (hazard)
    { }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTPostHazardDetectionEvent<config>::HasHazard() const noexcept
    {
        return hazard;
    }
}


// Implementation of: class UpstreamNodeEVTPreHazardPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPreHazardPendingEvent<config>::UpstreamNodeEVTPreHazardPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTPreHazardPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTPreHazardPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTPreHazardPendingEvent<config>::SetDenial(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTPreHazardPendingEvent<config>::Deny(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }
}


// Implementation of: class UpstreamNodeEVTPostHazardPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPostHazardPendingEvent<config>::UpstreamNodeEVTPostHazardPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit,
        DenialEnum                                      denial) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , denial                                (denial)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTPostHazardPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTPostHazardPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }
}


// Implementation of: class UpstreamNodeEVTPreChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPreChannelPendingEvent<config>::UpstreamNodeEVTPreChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTPreChannelPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTPreChannelPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTPreChannelPendingEvent<config>::SetDenial(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTPreChannelPendingEvent<config>::Deny(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }
}


// Implementation of: class UpstreamNodeEVTPostChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPostChannelPendingEvent<config>::UpstreamNodeEVTPostChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit,
        DenialEnum                                      denial) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , denial                                (denial)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTPostChannelPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTPostChannelPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }
}


// Implementation of: class UpstreamNodeEVTPreHazardToChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPreHazardToChannelPendingEvent<config>::UpstreamNodeEVTPreHazardToChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTPostHazardToChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPostHazardToChannelPendingEvent<config>::UpstreamNodeEVTPostHazardToChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTCacheStatePreDemotionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTCacheStatePreDemotionEvent<config>::UpstreamNodeEVTCacheStatePreDemotionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit,
        CacheStateEnum                                  prevState,
        CacheStateEnum&                                 nextState) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , CacheStatePreDemotionEventBase        (prevState, nextState)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTCacheStatePostDemotionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTCacheStatePostDemotionEvent<config>::UpstreamNodeEVTCacheStatePostDemotionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit,
        CacheStateEnum                                  prevState,
        CacheStateEnum                                  nextState) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , CacheStatePostDemotionEventBase       (prevState, nextState)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTDataPreHazardDetectionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPreHazardDetectionEvent<config>::UpstreamNodeEVTDataPreHazardDetectionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit,
        bool&                                           hazard) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , hazard                                (hazard)
    { }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTDataPreHazardDetectionEvent<config>::HasHazard() const noexcept
    {
        return hazard;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTDataPreHazardDetectionEvent<config>::SetHazard() noexcept
    {
        hazard = true;
    }
}


// Implementation of: class UpstreamNodeEVTDataPostHazardDetectionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPostHazardDetectionEvent<config>::UpstreamNodeEVTDataPostHazardDetectionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit,
        bool                                            hazard) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , hazard                                (hazard)
    { }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTDataPostHazardDetectionEvent<config>::HasHazard() const noexcept
    {
        return hazard;
    }
}


// Implementation of: class UpstreamNodeEVTDataPreHazardPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPreHazardPendingEvent<config>::UpstreamNodeEVTDataPreHazardPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTDataPreHazardPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTDataPreHazardPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTDataPreHazardPendingEvent<config>::SetDenial(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTDataPreHazardPendingEvent<config>::Deny(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }
}


// Implementation of: class UpstreamNodeEVTDataPostHazardPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPostHazardPendingEvent<config>::UpstreamNodeEVTDataPostHazardPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit,
        DenialEnum                                      denial) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , denial                                (denial)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTDataPostHazardPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTDataPostHazardPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }
}


// Implementation of: class UpstreamNodeEVTDataPreChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPreChannelPendingEvent<config>::UpstreamNodeEVTDataPreChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTDataPreChannelPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTDataPreChannelPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTDataPreChannelPendingEvent<config>::SetDenial(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeEVTDataPreChannelPendingEvent<config>::Deny(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }
}


// Implementation of: class UpstreamNodeEVTDataPostChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPostChannelPendingEvent<config>::UpstreamNodeEVTDataPostChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit,
        DenialEnum                                      denial) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , denial                                (denial)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeEVTDataPostChannelPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeEVTDataPostChannelPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }
}


// Implementation of: class UpstreamNodeEVTDataPreHazardToChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPreHazardToChannelPendingEvent<config>::UpstreamNodeEVTDataPreHazardToChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTDataPostHazardToChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTDataPostHazardToChannelPendingEvent<config>::UpstreamNodeEVTDataPostHazardToChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTPreChannelChosenEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPreChannelChosenEvent<config>::UpstreamNodeEVTPreChannelChosenEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTPostChannelChosenEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTPostChannelChosenEvent<config>::UpstreamNodeEVTPostChannelChosenEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::EVT<config>&                             evtFlit) noexcept
        : EVTFlitEventBase<config>              (evtFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTUpDATPreChannelChosenEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTUpDATPreChannelChosenEvent<config>::UpstreamNodeEVTUpDATPreChannelChosenEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeEVTUpDATPostChannelChosenEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeEVTUpDATPostChannelChosenEvent<config>::UpstreamNodeEVTUpDATPostChannelChosenEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::UpDAT<config>&                           updatFlit) noexcept
        : UpDATFlitEventBase<config>            (updatFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }
}


// Implementation of: class UpstreamNodeREQPreHazardDetectionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeREQPreHazardDetectionEvent<config>::UpstreamNodeREQPreHazardDetectionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::REQ<config>&                             reqFlit,
        bool&                                           hazard) noexcept
        : REQFlitEventBase<config>              (reqFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , hazard                                (hazard)
    { }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeREQPreHazardDetectionEvent<config>::HasHazard() const noexcept
    {
        return hazard;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeREQPreHazardDetectionEvent<config>::SetHazard() noexcept
    {
        hazard = true;
    }
}


// Implementation of: class UpstreamNodeREQPostHazardDetectionEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeREQPostHazardDetectionEvent<config>::UpstreamNodeREQPostHazardDetectionEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::REQ<config>&                             reqFlit,
        bool                                            hazard) noexcept
        : REQFlitEventBase<config>              (reqFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , hazard                                (hazard)
    { }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeREQPostHazardDetectionEvent<config>::HasHazard() const noexcept
    {
        return hazard;
    }
}


// Implementation of: class UpstreamNodeREQPreHazardPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeREQPreHazardPendingEvent<config>::UpstreamNodeREQPreHazardPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::REQ<config>&                             reqFlit) noexcept
        : REQFlitEventBase<config>              (reqFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeREQPreHazardPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeREQPreHazardPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeREQPreHazardPendingEvent<config>::SetDenial(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeREQPreHazardPendingEvent<config>::Deny(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }
}


// Implementation of: class UpstreamNodeREQPostHazardPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeREQPostHazardPendingEvent<config>::UpstreamNodeREQPostHazardPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::REQ<config>&                             reqFlit,
        DenialEnum                                      denial) noexcept
        : REQFlitEventBase<config>              (reqFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , denial                                (denial)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeREQPostHazardPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeREQPostHazardPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }
}


// Implementation of: class UpstreamNodeREQPreChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeREQPreChannelPendingEvent<config>::UpstreamNodeREQPreChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::REQ<config>&                             reqFlit) noexcept
        : REQFlitEventBase<config>              (reqFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeREQPreChannelPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeREQPreChannelPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeREQPreChannelPendingEvent<config>::SetDenial(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNodeREQPreChannelPendingEvent<config>::Deny(DenialEnum denial) noexcept
    {
        this->denial = denial;
    }
}


// Implementation of: class UpstreamNodeREQPostChannelPendingEvent
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline UpstreamNodeREQPostChannelPendingEvent<config>::UpstreamNodeREQPostChannelPendingEvent(
        UpstreamNode<config>&                           upstream,
        uint64_t                                        PA,
        const typename UpstreamNode<config>::CacheLine& cacheLine,
        Flits::REQ<config>&                             reqFlit,
        DenialEnum                                      denial) noexcept
        : REQFlitEventBase<config>              (reqFlit)
        , UpstreamNodeCacheLineEventBase<config>(upstream, PA, cacheLine)
        , denial                                (denial)
    { }

    template<FlitConfigurationConcept config>
    inline DenialEnum UpstreamNodeREQPostChannelPendingEvent<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNodeREQPostChannelPendingEvent<config>::IsDenied() const noexcept
    {
        return denial->IsRejected();
    }
}


#endif // __CCHI__CCHI_ICN_TAURUS__COMPONENT_EVENTS
