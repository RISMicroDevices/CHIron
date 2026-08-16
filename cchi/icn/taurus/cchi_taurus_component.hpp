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

        std::vector<std::shared_ptr<Xact::Xaction<config>>>
                                                queueTxnID;

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
        std::optional<size_t>                   NextTxnID() const noexcept;
        void                                    FreeTxnID(size_t txnID) noexcept;
        void                                    OccupyTxnID(size_t txnID, std::shared_ptr<Xact::Xaction<config>> xaction) noexcept;

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

    public:
        void                                    Tick(uint64_t time) noexcept;

    protected:  
        void                                    TickEVT() noexcept;
        void                                    TickSNP() noexcept;
        void                                    TickREQ() noexcept;

    public:
        std::optional<Flits::EVT<config>>       PeekTXEVT() const noexcept;
        std::optional<Flits::EVT<config>>       PopTXEVT() noexcept;

        std::optional<Flits::REQ<config>>       PeekTXREQ() const noexcept;
        std::optional<Flits::REQ<config>>       PopTXREQ() noexcept;

        bool                                    PushRXSNP(const Flits::SNP<config>& snpFlit) noexcept;

        std::optional<Flits::UpRSP<config>>     PeekTXRSP() const noexcept;
        std::optional<Flits::UpRSP<config>>     PopTXRSP() noexcept;

        std::optional<Flits::UpDAT<config>>     PeekTXDAT() const noexcept;
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
        , queueTxnID                (xactionLimitTotal)
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
        auto txnID = NextTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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

        if (hazard)
            cacheLine->pendingREQHazardTXREQ = { reqFlit };
        else
        {
            cacheLine->pendingREQChannelTXREQ = { reqFlit };
            queueTXREQ.push_back(cacheLine);
        }

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
        auto txnID = NextTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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
        // *NOTE: 'ExpCompData' should be refreshed on releasing hazard for ReadUnique

        if (hazard)
            cacheLine->pendingREQHazardTXREQ = { reqFlit };
        else
        {
            reqFlit.ExpCompData = cacheLine->state == CacheState::Invalid ? 1 : 0;
            cacheLine->pendingREQChannelTXREQ = { reqFlit };
            queueTXREQ.push_back(cacheLine);
        }

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
        auto txnID = NextTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<GrantedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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

        if (hazard)
            cacheLine->pendingREQHazardTXREQ = { reqFlit };
        else
        {
            cacheLine->pendingREQChannelTXREQ = { reqFlit };
            queueTXREQ.push_back(cacheLine);
        }

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
            // in-flight REQ's competitors are waiting for, and, since upstream
            // TxnIDs are currently always 0, the REQ and the EVT could not be
            // told apart on the joint and the DnRSP channel either
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_PA_REQ_BUSY);
        }

        if (IsEVTInFlight(*cacheLine))
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_PA_EVT_BUSY);

        //
        auto txnID = NextTxnID();

        if (!txnID)
            return std::make_shared<FutureNow<EvictedEvent>>(Denial::REJECTED_TAURUS_TXNID_BUSY);

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

        if (HasEVTHazard(*cacheLine))
            cacheLine->pendingEVTHazardTXEVT = { evtFlit };
        else
        {
            cacheLine->pendingEVTChannelTXEVT = { evtFlit };
        }

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
                if (HasEVTHazard(cacheLine))
                    continue;

                // *NOTE: This also applicable for Write-Back, since EVT was always the highest priority transaction
                cacheLine.state = CacheState::Invalid;

                cacheLine.pendingEVTChannelTXEVT = cacheLine.pendingEVTHazardTXEVT;
                cacheLine.pendingEVTHazardTXEVT.reset();
            }
            else if (cacheLine.activeEVT)
            {
                if (cacheLine.activeEVT->GetType() == Xact::XactionType::Evict)
                {
                    Xact::XactionEvict<config>& xactionEvict 
                        = static_cast<Xact::XactionEvict<config>&>(*cacheLine.activeEVT);
                    
                    if (xactionEvict.IsComplete(glbl) && cacheLine.activeEVTFuture && !cacheLine.activeEVTFuture->Fired())
                    {
                        cacheLine.activeEVTFuture->Fire(EvictedEvent(xactionEvict.GetFirst().flit.evt.Addr, it->second));
                    }
                }
                else if (cacheLine.activeEVT->GetType() == Xact::XactionType::WriteBack)
                {
                    Xact::XactionWriteBack<config>& xactionWriteBack 
                        = static_cast<Xact::XactionWriteBack<config>&>(*cacheLine.activeEVT);

                    if (xactionWriteBack.GotDBIDResp())
                    {
                        bool justGotDBIDResp = !xactionWriteBack.GotAnyCopyBackWrData();

                        if (cacheLine.pendingEVTHazardTXDAT0)
                        {
                            justGotDBIDResp = false;

                            if (!HasEVTDataHazard(cacheLine, 0))
                            {
                                cacheLine.pendingEVTChannelTXDAT0 = cacheLine.pendingEVTHazardTXDAT0;
                                cacheLine.pendingEVTHazardTXDAT0.reset();
                            }
                        }
                        else if (cacheLine.pendingEVTChannelTXDAT0)
                        {
                            justGotDBIDResp = false;
                        }

                        if (cacheLine.pendingEVTHazardTXDAT1)
                        {
                            justGotDBIDResp = false;

                            if (!HasEVTDataHazard(cacheLine, 1))
                            {
                                cacheLine.pendingEVTChannelTXDAT1 = cacheLine.pendingEVTHazardTXDAT1;
                                cacheLine.pendingEVTHazardTXDAT1.reset();
                            }
                        }
                        else if (cacheLine.pendingEVTChannelTXDAT1)
                        {
                            justGotDBIDResp = false;
                        }
                        
                        if (justGotDBIDResp)
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

                            if (HasEVTDataHazard(cacheLine, 0))
                                cacheLine.pendingEVTHazardTXDAT0 = datFlit;
                            else
                                cacheLine.pendingEVTChannelTXDAT0 = datFlit;

                            datFlit.DataID = 1;
                            datFlit.Data[0] = cacheLine.data[4];
                            datFlit.Data[1] = cacheLine.data[5];
                            datFlit.Data[2] = cacheLine.data[6];
                            datFlit.Data[3] = cacheLine.data[7];

                            if (HasEVTDataHazard(cacheLine, 1))
                                cacheLine.pendingEVTHazardTXDAT1 = datFlit;
                            else
                                cacheLine.pendingEVTChannelTXDAT1 = datFlit;
                        }
                    }

                    if (xactionWriteBack.IsComplete(glbl) && cacheLine.activeEVTFuture && !cacheLine.activeEVTFuture->Fired())
                    {
                        cacheLine.activeEVTFuture->Fire(EvictedEvent(xactionWriteBack.GetFirst().flit.evt.Addr, it->second));
                    }
                }
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
                if (HasSNPHazard(cacheLine))
                    continue;

                auto flit = cacheLine.pendingSNPHazardRXSNP;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextSNP(glbl, time, *flit, &xaction);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.activeSNP = xaction;
                cacheLine.pendingSNPHazardRXSNP.reset();
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

                    const Flits::SNP<config>& flit = xactionSnoop.GetFirst().flit.snp;

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
                                    state = CacheState::Shared;
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
                    }

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

                        cacheLine.pendingSNPChannelTXDAT0 = datFlit;

                        datFlit.DataID = 1;
                        datFlit.Data[0] = cacheLine.data[4];
                        datFlit.Data[1] = cacheLine.data[5];
                        datFlit.Data[2] = cacheLine.data[6];
                        datFlit.Data[3] = cacheLine.data[7];

                        cacheLine.pendingSNPChannelTXDAT1 = datFlit;
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

                        cacheLine.pendingSNPChannelTXRSP = rspFlit;
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
                if (HasREQHazard(cacheLine))
                    continue;

                cacheLine.pendingREQChannelTXREQ = cacheLine.pendingREQHazardTXREQ;
                cacheLine.pendingREQHazardTXREQ.reset();
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

                        cacheLine.pendingREQChannelTXRSP = rspFlit;
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

                        cacheLine.pendingREQChannelTXRSP = rspFlit;
                    }
                }
            }
        }
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::EVT<config>> UpstreamNode<config>::PeekTXEVT() const noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingEVTChannelTXEVT)
                return { *cacheLine.pendingEVTChannelTXEVT };
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
                auto flit = cacheLine.pendingEVTChannelTXEVT;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextEVT(glbl, time, *flit, &xaction);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.activeEVT = xaction;
                cacheLine.pendingEVTChannelTXEVT.reset();
                return { *flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::REQ<config>> UpstreamNode<config>::PeekTXREQ() const noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingREQChannelTXREQ)
                return { *cacheLine.pendingREQChannelTXREQ };
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
                auto flit = cacheLine.pendingREQChannelTXREQ;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextREQ(glbl, time, *flit, &xaction);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.activeREQ = xaction;
                cacheLine.pendingREQChannelTXREQ.reset();
                return { *flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::PushRXSNP(const Flits::SNP<config>& snpFlit) noexcept
    {
        std::shared_ptr<CacheLine> cacheLine = GetCacheLine(snpFlit.Addr << 3);

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
            SetCacheLine(snpFlit.Addr << 3, cacheLine);
        }

        if (HasSNPHazard(*cacheLine))
        {
            cacheLine->pendingSNPHazardRXSNP = snpFlit;
        }
        else
        {
            std::shared_ptr<Xact::Xaction<config>> xaction;
            XactDenialEnum denial = joint.NextSNP(glbl, time, snpFlit, &xaction);

            if (denial != XactDenial::ACCEPTED)
            {
                // TODO: denial event
            }

            cacheLine->activeSNP = xaction;
        }

        return true;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpRSP<config>> UpstreamNode<config>::PeekTXRSP() const noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingSNPChannelTXRSP)
                return { *cacheLine.pendingSNPChannelTXRSP };

            if (cacheLine.pendingREQChannelTXRSP)
                return { *cacheLine.pendingREQChannelTXRSP };
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
                auto flit = cacheLine.pendingSNPChannelTXRSP;
                XactDenialEnum denial = joint.NextUpRSP(glbl, time, *flit);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.pendingSNPChannelTXRSP.reset();
                return { *flit };
            }

            if (cacheLine.pendingREQChannelTXRSP)
            {
                auto flit = cacheLine.pendingREQChannelTXRSP;
                XactDenialEnum denial = joint.NextUpRSP(glbl, time, *flit);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.pendingREQChannelTXRSP.reset();
                return { *flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<Flits::UpDAT<config>> UpstreamNode<config>::PeekTXDAT() const noexcept
    {
        for (auto it = cacheable.begin(); it != cacheable.end(); ++it)
        {
            CacheLine& cacheLine = *it->second;

            if (cacheLine.pendingSNPChannelTXDAT0)
                return { *cacheLine.pendingSNPChannelTXDAT0 };

            if (cacheLine.pendingSNPChannelTXDAT1)
                return { *cacheLine.pendingSNPChannelTXDAT1 };

            if (cacheLine.pendingEVTChannelTXDAT0)
                return { *cacheLine.pendingEVTChannelTXDAT0 };

            if (cacheLine.pendingEVTChannelTXDAT1)
                return { *cacheLine.pendingEVTChannelTXDAT1 };
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
                auto flit = cacheLine.pendingSNPChannelTXDAT0;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, *flit, &xaction);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.pendingSNPChannelTXDAT0.reset();
                return { *flit };
            }

            if (cacheLine.pendingSNPChannelTXDAT1)
            {
                auto flit = cacheLine.pendingSNPChannelTXDAT1;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, *flit, &xaction);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.pendingSNPChannelTXDAT1.reset();
                return { *flit };
            }

            if (cacheLine.pendingEVTChannelTXDAT0)
            {
                auto flit = cacheLine.pendingEVTChannelTXDAT0;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, *flit, &xaction);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.pendingEVTChannelTXDAT0.reset();
                return { *flit };
            }

            if (cacheLine.pendingEVTChannelTXDAT1)
            {
                auto flit = cacheLine.pendingEVTChannelTXDAT1;

                std::shared_ptr<Xact::Xaction<config>> xaction;
                XactDenialEnum denial = joint.NextUpDAT(glbl, time, *flit, &xaction);

                if (denial != XactDenial::ACCEPTED)
                {
                    // TODO: denial event
                }

                cacheLine.pendingEVTChannelTXDAT1.reset();
                return { *flit };
            }
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::PushRXRSP(const Flits::DnRSP<config>& dnrspFlit) noexcept
    {
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
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit);

                    if (denial != XactDenial::ACCEPTED)
                    {
                        // TODO: denial event
                    }

                    return true;
                }
                else if (cacheLine.activeEVT
                 && !cacheLine.activeEVT->IsComplete(glbl)
                 && cacheLine.activeEVT->GetTxnID() == dnrspFlit.TxnID)
                {
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit);

                    if (denial != XactDenial::ACCEPTED)
                    {
                        // TODO: denial event
                    }

                    return true;
                }
            }

            // TODO: unexpected DnRSP

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
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit);

                    if (denial != XactDenial::ACCEPTED)
                    {
                        // TODO: denial event
                    }

                    return true;
                }
            }

            // TODO: unexpected DnRSP

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
                    XactDenialEnum denial = joint.NextDnRSP(glbl, time, dnrspFlit);

                    if (denial != XactDenial::ACCEPTED)
                    {
                        // TODO: denial event
                    }

                    return true;
                }
            }

            // TODO: unexpected DnRSP

            return true;
        }

        // TODO: unrecognized DnRSP

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool UpstreamNode<config>::PushRXDAT(const Flits::DnDAT<config>& dndatFlit) noexcept
    {
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
                    XactDenialEnum denial = joint.NextDnDAT(glbl, time, dndatFlit);

                    if (denial != XactDenial::ACCEPTED)
                    {
                        // TODO: denial event
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

    // TODO
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
        return !event.has_value();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsNow() const noexcept
    {
        return event.has_value();
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
    inline std::optional<size_t> UpstreamNode<config>::NextTxnID() const noexcept
    {
        for (size_t i = 0; i < queueTxnID.size(); ++i)
        {
            if (!queueTxnID[i])
                return { i };
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::FreeTxnID(size_t txnID) noexcept
    {
        queueTxnID.at(txnID) = nullptr;
    }

    template<FlitConfigurationConcept config>
    inline void UpstreamNode<config>::OccupyTxnID(size_t txnID, std::shared_ptr<Xact::Xaction<config>> xaction) noexcept
    {
        queueTxnID.at(txnID) = xaction;
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

                if (!xaction.GotAllCompData())
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

                if (!xaction.GotComp() && !xaction.GotAnyCompData())
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
