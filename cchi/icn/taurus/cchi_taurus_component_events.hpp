#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__COMPONENT_EVENTS
#define __CCHI__CCHI_ICN_TAURUS__COMPONENT_EVENTS

#include <memory>

#include "cchi_taurus_component_afx.hpp"

#include "../../xact/cchi_joint.hpp"            // IWYU pragma: keep


namespace CCHI::Taurus {

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


#endif // __CCHI__CCHI_ICN_TAURUS__COMPONENT_EVENTS
