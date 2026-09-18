#pragma once

#ifndef __CCHI__CCHI_XACT_STATE
#define __CCHI__CCHI_XACT_STATE

#include "cchi_xact_state/includes.hpp"                     // IWYU pragma: keep
#include "cchi_xact_state/base.hpp"                         // IWYU pragma: keep
#include "cchi_xact_state/cst.hpp"                          // IWYU pragma: keep


namespace CCHI {
    namespace Xact {

        namespace details {

            struct CohTrans {
                CacheState                                      initial;
                CacheStateTransitions::Intermediates::details::TableG2
                                                                nested;
                const CacheStateTransitions::Intermediates::Tables*
                                                                tables;
                CacheStateTransition::Type                      type;
            };
        }

        template<FlitConfigurationConcept config>
        class CacheStateMap;

        template<FlitConfigurationConcept config>
        class CacheStateMapEventBase {
        protected:
            CacheStateMap<config>&      host;
            const Xaction<config>*      xaction;

        public:
            CacheStateMapEventBase(CacheStateMap<config>& host, const Xaction<config>* xaction) noexcept;

        public:
            CacheStateMap<config>&          GetCacheStateMap() noexcept;
            const CacheStateMap<config>&    GetCacheStateMap() const noexcept;

            const Xaction<config>*          GetXaction() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedEventBase : public CacheStateMapEventBase<config> {
        protected:
            XactDenialEnum  denial;
            std::string     message;

        public:
            CacheStateMapDeniedEventBase(CacheStateMap<config>&     host,
                                         const Xaction<config>*     xaction,
                                         XactDenialEnum             denial,
                                         const std::string&         message = "") noexcept;

        public:
            XactDenialEnum          GetDenial() const noexcept;

            std::string&            GetMessage() noexcept;
            const std::string&      GetMessage() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedRequestEventBase : public CacheStateMapDeniedEventBase<config> {
        protected:
            const FiredRequestFlit<config>& flit;

        public:
            CacheStateMapDeniedRequestEventBase(CacheStateMap<config>&          host,
                                                const Xaction<config>*          xaction,
                                                XactDenialEnum                  denial,
                                                const FiredRequestFlit<config>& flit,
                                                const std::string&              message = "") noexcept;

        public:
            const FiredRequestFlit<config>& GetFlit() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedResponseEventBase : public CacheStateMapDeniedEventBase<config> {
        protected:
            const FiredResponseFlit<config>& flit;

        public:
            CacheStateMapDeniedResponseEventBase(CacheStateMap<config>&             host,
                                                 const Xaction<config>*             xaction,
                                                 XactDenialEnum                     denial,
                                                 const FiredResponseFlit<config>&    flit,
                                                 const std::string&                  message = "") noexcept;

        public:
            const FiredResponseFlit<config>& GetFlit() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedTXEVTEvent : public CacheStateMapDeniedRequestEventBase<config>,
                                              public Gravity::Event<CacheStateMapDeniedTXEVTEvent<config>> {
        public:
            CacheStateMapDeniedTXEVTEvent(CacheStateMap<config>&            host,
                                          const Xaction<config>*            xaction,
                                          XactDenialEnum                    denial,
                                          const FiredRequestFlit<config>&   flit,
                                          const std::string&                message = "") noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedTXREQEvent : public CacheStateMapDeniedRequestEventBase<config>,
                                              public Gravity::Event<CacheStateMapDeniedTXREQEvent<config>> {
        public:
            CacheStateMapDeniedTXREQEvent(CacheStateMap<config>&            host,
                                          const Xaction<config>*            xaction,
                                          XactDenialEnum                    denial,
                                          const FiredRequestFlit<config>&   flit,
                                          const std::string&                message = "") noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedRXSNPEvent : public CacheStateMapDeniedRequestEventBase<config>,
                                              public Gravity::Event<CacheStateMapDeniedRXSNPEvent<config>> {
        public:
            CacheStateMapDeniedRXSNPEvent(CacheStateMap<config>&            host,
                                          const Xaction<config>*            xaction,
                                          XactDenialEnum                    denial,
                                          const FiredRequestFlit<config>&   flit,
                                          const std::string&                message = "") noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedRXDnRSPEvent : public CacheStateMapDeniedResponseEventBase<config>,
                                                public Gravity::Event<CacheStateMapDeniedRXDnRSPEvent<config>> {
        public:
            CacheStateMapDeniedRXDnRSPEvent(CacheStateMap<config>&              host,
                                            const Xaction<config>*              xaction,
                                            XactDenialEnum                      denial,
                                            const FiredResponseFlit<config>&    flit,
                                            const std::string&                  message = "") noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedTXUpRSPEvent : public CacheStateMapDeniedResponseEventBase<config>,
                                                public Gravity::Event<CacheStateMapDeniedTXUpRSPEvent<config>> {
        public:
            CacheStateMapDeniedTXUpRSPEvent(CacheStateMap<config>&              host,
                                            const Xaction<config>*              xaction,
                                            XactDenialEnum                      denial,
                                            const FiredResponseFlit<config>&    flit,
                                            const std::string&                  message = "") noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedRXDnDATEvent : public CacheStateMapDeniedResponseEventBase<config>,
                                                public Gravity::Event<CacheStateMapDeniedRXDnDATEvent<config>> {
        public:
            CacheStateMapDeniedRXDnDATEvent(CacheStateMap<config>&              host,
                                            const Xaction<config>*              xaction,
                                            XactDenialEnum                      denial,
                                            const FiredResponseFlit<config>&    flit,
                                            const std::string&                  message = "") noexcept;
        };

        template<FlitConfigurationConcept config>
        class CacheStateMapDeniedTXUpDATEvent : public CacheStateMapDeniedResponseEventBase<config>,
                                                public Gravity::Event<CacheStateMapDeniedTXUpDATEvent<config>> {
        public:
            CacheStateMapDeniedTXUpDATEvent(CacheStateMap<config>&              host,
                                            const Xaction<config>*              xaction,
                                            XactDenialEnum                      denial,
                                            const FiredResponseFlit<config>&    flit,
                                            const std::string&                  message = "") noexcept;
        };


        /*
        CacheStateMap: requester-side (Type-1 upstream node) cache-coherency state
        checker and predictor for CCHI - a replication of the CHI-side
        RNCacheStateMap (chi/xact/chi_xact_state.hpp) adapted to the CCHI protocol.

        This is a second-stage checker: Joint/Xaction decide whether a flit sequence
        is a legal transaction; CacheStateMap decides whether that legal sequence
        implies a legal requester cache-state transition, and tracks a per-address
        state set.
        */
        template<FlitConfigurationConcept config>
        class CacheStateMap {
        public:
            class EventHub {
            public:
                Gravity::EventBus<CacheStateMapDeniedTXEVTEvent<config>>    OnDeniedTXEVT;
                Gravity::EventBus<CacheStateMapDeniedTXREQEvent<config>>    OnDeniedTXREQ;
                Gravity::EventBus<CacheStateMapDeniedRXSNPEvent<config>>    OnDeniedRXSNP;
                Gravity::EventBus<CacheStateMapDeniedRXDnRSPEvent<config>>  OnDeniedRXDnRSP;
                Gravity::EventBus<CacheStateMapDeniedTXUpRSPEvent<config>>  OnDeniedTXUpRSP;
                Gravity::EventBus<CacheStateMapDeniedRXDnDATEvent<config>>  OnDeniedRXDnDAT;
                Gravity::EventBus<CacheStateMapDeniedTXUpDATEvent<config>>  OnDeniedTXUpDAT;

            public:
                EventHub() noexcept;
                void Clear() noexcept;
            };

            std::shared_ptr<EventHub> events;

        private:
            static constexpr size_t ADDR_OFFSET_CACHE_BLOCK     = 6;
            static constexpr size_t ADDR_OFFSET_CACHE_MAP_SPAN  = 24;
            // *NOTICE: Used by access map of seer mode only
            // offset = 24 =>   1 GiB aligned ->   2 MiB bitset span
            // offset = 23 => 512 MiB aligned ->   1 MiB bitset span
            // offset = 22 => 256 MiB aligned -> 512 KiB bitset span
            // offset = 21 => 128 MiB aligned -> 256 KiB bitset span
            // offset = 20 =>  64 MiB aligned -> 128 KiB bitset span
            // offset = 19 =>  32 MiB aligned ->  64 KiB bitset span
            // offset = 18 =>  16 MiB aligned ->  32 KiB bitset span
            // offset = 17 =>   8 MiB aligned ->  16 KiB bitset span
            // offset = 16 =>   4 MiB aligned ->   8 KiB bitset span
            // offset = 15 =>   2 MiB aligned ->   4 KiB bitset span
            // offset = 14 =>   1 MiB aligned ->   2 KiB bitset span
            // ...
            static constexpr size_t ADDR_OFFSET_CACHE_MAP       = ADDR_OFFSET_CACHE_BLOCK + ADDR_OFFSET_CACHE_MAP_SPAN;

            static constexpr size_t SIZE_CACHE_BLOCK            = (1UL << ADDR_OFFSET_CACHE_BLOCK);
            static constexpr size_t SIZE_CACHE_MAP_SPAN         = (1UL << ADDR_OFFSET_CACHE_MAP_SPAN);

        private:
            Opcodes::EVT::Decoder<Flits::EVT<config>, const details::CohTrans*>
                        evtDecoder;

            Opcodes::REQ::Decoder<Flits::REQ<config>, const details::CohTrans*>
                        reqDecoder;

            Opcodes::SNP::Decoder<Flits::SNP<config>, const details::CohTrans*>
                        snpDecoder;

        private:
            std::unordered_map<typename Flits::REQ<config>::addr_t::value_type, CacheState>
                        stateMap;

        private:
            bool        enableSilentEviction;
            bool        enableSilentSharing;
            bool        enableSilentStore;
            bool        enableSilentInvalidation;

        private:
            bool        seerEnabled;
            void*       seerConfusion;  // TODO

            std::unordered_map<typename Flits::REQ<config>::addr_t::value_type, std::bitset<SIZE_CACHE_MAP_SPAN>>
                        seerAccessedMap;

        protected:
            bool        IsAccessed(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept;
            void        SetAccessed(typename Flits::REQ<config>::addr_t::value_type addr) noexcept;

            std::pair<CacheState, bool> ExcavateWithSeer(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept;
            std::pair<CacheState, bool> EvaluateWithSeer(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept;

            XactDenialEnum  DeniedTXEVT(XactDenialEnum              denial,
                                        const Xaction<config>*      xaction,
                                        uint64_t                    time,
                                        const Flits::EVT<config>&   flit,
                                        const std::string&          message = "") noexcept;
            XactDenialEnum  DeniedTXREQ(XactDenialEnum              denial,
                                        const Xaction<config>*      xaction,
                                        uint64_t                    time,
                                        const Flits::REQ<config>&   flit,
                                        const std::string&          message = "") noexcept;
            XactDenialEnum  DeniedRXSNP(XactDenialEnum              denial,
                                        const Xaction<config>*      xaction,
                                        uint64_t                    time,
                                        const Flits::SNP<config>&   flit,
                                        const std::string&          message = "") noexcept;
            XactDenialEnum  DeniedRXDnRSP(XactDenialEnum                denial,
                                          const Xaction<config>*        xaction,
                                          uint64_t                      time,
                                          const Flits::DnRSP<config>&   flit,
                                          const std::string&            message = "") noexcept;
            XactDenialEnum  DeniedTXUpRSP(XactDenialEnum                denial,
                                          const Xaction<config>*        xaction,
                                          uint64_t                      time,
                                          const Flits::UpRSP<config>&   flit,
                                          const std::string&            message = "") noexcept;
            XactDenialEnum  DeniedRXDnDAT(XactDenialEnum                denial,
                                          const Xaction<config>*        xaction,
                                          uint64_t                      time,
                                          const Flits::DnDAT<config>&   flit,
                                          const std::string&            message = "") noexcept;
            XactDenialEnum  DeniedTXUpDAT(XactDenialEnum                denial,
                                          const Xaction<config>*        xaction,
                                          uint64_t                      time,
                                          const Flits::UpDAT<config>&   flit,
                                          const std::string&            message = "") noexcept;

        public:
            // D7: silent transitions - the CCHI spec is silent about them; the four
            //     toggles below replicate the CHI-side documented assumptions
            //     (all default-on) and widen the tracked set before every check:
            //       Eviction:     UC | SC -> +I
            //       Sharing:      UC -> +SC
            //       Store:        UC -> +UD
            //       Invalidation: UD -> +I
            CacheState  EvaluateSilently(CacheState state) const noexcept;

        public:
            CacheStateMap() noexcept;

        public:
            void            SetSilentEviction(bool enableSilentEviction) noexcept;
            void            SetSilentSharing(bool enableSilentSharing) noexcept;
            void            SetSilentStore(bool enableSilentStore) noexcept;
            void            SetSilentInvalidation(bool enableSilentInvalidation) noexcept;

            bool            HasSilentEviction() const noexcept;
            bool            HasSilentSharing() const noexcept;
            bool            HasSilentStore() const noexcept;
            bool            HasSilentInvalidation() const noexcept;

            void            Set(typename Flits::REQ<config>::addr_t::value_type addr, CacheState state) noexcept;
            CacheState      Get(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept;

        public:
            CacheState      Excavate(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept;
            CacheState      Evaluate(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept;

        public:
            XactDenialEnum  NextTXEVT(typename Flits::REQ<config>::addr_t::value_type addr, uint64_t time, const Flits::EVT<config>& flit) noexcept;
            XactDenialEnum  NextTXREQ(typename Flits::REQ<config>::addr_t::value_type addr, uint64_t time, const Flits::REQ<config>& flit) noexcept;
            XactDenialEnum  NextRXSNP(typename Flits::REQ<config>::addr_t::value_type addr, uint64_t time, const Flits::SNP<config>& flit) noexcept;
            XactDenialEnum  NextRXDnRSP(typename Flits::REQ<config>::addr_t::value_type addr, uint64_t time, const Xaction<config>& xaction, const Flits::DnRSP<config>& flit) noexcept;
            XactDenialEnum  NextTXUpRSP(typename Flits::REQ<config>::addr_t::value_type addr, uint64_t time, const Xaction<config>& xaction, const Flits::UpRSP<config>& flit) noexcept;
            XactDenialEnum  NextRXDnDAT(typename Flits::REQ<config>::addr_t::value_type addr, uint64_t time, const Xaction<config>& xaction, const Flits::DnDAT<config>& flit) noexcept;
            XactDenialEnum  NextTXUpDAT(typename Flits::REQ<config>::addr_t::value_type addr, uint64_t time, const Xaction<config>& xaction, const Flits::UpDAT<config>& flit) noexcept;

        public:
            // *NOTICE: On SNP nesting (a nested eviction/writeback while a snoop is
            //          in flight) the transfer target is checked against the snoop's
            //          G2 (start -> pre-reply) table; DENIED_STATE_NESTED_TRANSFER on
            //          empty. On REQ/EVT nesting the transfer is a passthrough Set
            //          (no intermediate-state model for REQ/EVT-nested transfers in
            //          CCHI; the transfer target is authoritative).
            //          'time' is only used for denial events.
            XactDenialEnum  Transfer(typename Flits::REQ<config>::addr_t::value_type addr, CacheState state, const Xaction<config>* nestingXaction = nullptr, uint64_t time = 0) noexcept;
        };
    }
}




// Implementation of: class CacheStateMap
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline CacheStateMapEventBase<config>::CacheStateMapEventBase(
        CacheStateMap<config>&      host,
        const Xaction<config>*      xaction) noexcept
        : host      (host)
        , xaction   (xaction)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMap<config>& CacheStateMapEventBase<config>::GetCacheStateMap() noexcept
    {
        return host;
    }

    template<FlitConfigurationConcept config>
    inline const CacheStateMap<config>& CacheStateMapEventBase<config>::GetCacheStateMap() const noexcept
    {
        return host;
    }

    template<FlitConfigurationConcept config>
    inline const Xaction<config>* CacheStateMapEventBase<config>::GetXaction() const noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedEventBase<config>::CacheStateMapDeniedEventBase(
        CacheStateMap<config>&      host,
        const Xaction<config>*      xaction,
        XactDenialEnum              denial,
        const std::string&          message) noexcept
        : CacheStateMapEventBase<config>    (host, xaction)
        , denial                            (denial)
        , message                           (message)
    { }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMapDeniedEventBase<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline std::string& CacheStateMapDeniedEventBase<config>::GetMessage() noexcept
    {
        return message;
    }

    template<FlitConfigurationConcept config>
    inline const std::string& CacheStateMapDeniedEventBase<config>::GetMessage() const noexcept
    {
        return message;
    }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedRequestEventBase<config>::CacheStateMapDeniedRequestEventBase(
        CacheStateMap<config>&          host,
        const Xaction<config>*          xaction,
        XactDenialEnum                  denial,
        const FiredRequestFlit<config>& flit,
        const std::string&              message) noexcept
        : CacheStateMapDeniedEventBase<config>  (host, xaction, denial, message)
        , flit                                  (flit)
    { }

    template<FlitConfigurationConcept config>
    inline const FiredRequestFlit<config>& CacheStateMapDeniedRequestEventBase<config>::GetFlit() const noexcept
    {
        return flit;
    }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedResponseEventBase<config>::CacheStateMapDeniedResponseEventBase(
        CacheStateMap<config>&              host,
        const Xaction<config>*              xaction,
        XactDenialEnum                      denial,
        const FiredResponseFlit<config>&    flit,
        const std::string&                  message) noexcept
        : CacheStateMapDeniedEventBase<config>  (host, xaction, denial, message)
        , flit                                  (flit)
    { }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>& CacheStateMapDeniedResponseEventBase<config>::GetFlit() const noexcept
    {
        return flit;
    }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedTXEVTEvent<config>::CacheStateMapDeniedTXEVTEvent(
        CacheStateMap<config>&          host,
        const Xaction<config>*          xaction,
        XactDenialEnum                  denial,
        const FiredRequestFlit<config>& flit,
        const std::string&              message) noexcept
        : CacheStateMapDeniedRequestEventBase<config>(host, xaction, denial, flit, message)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedTXREQEvent<config>::CacheStateMapDeniedTXREQEvent(
        CacheStateMap<config>&          host,
        const Xaction<config>*          xaction,
        XactDenialEnum                  denial,
        const FiredRequestFlit<config>& flit,
        const std::string&              message) noexcept
        : CacheStateMapDeniedRequestEventBase<config>(host, xaction, denial, flit, message)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedRXSNPEvent<config>::CacheStateMapDeniedRXSNPEvent(
        CacheStateMap<config>&          host,
        const Xaction<config>*          xaction,
        XactDenialEnum                  denial,
        const FiredRequestFlit<config>& flit,
        const std::string&              message) noexcept
        : CacheStateMapDeniedRequestEventBase<config>(host, xaction, denial, flit, message)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedRXDnRSPEvent<config>::CacheStateMapDeniedRXDnRSPEvent(
        CacheStateMap<config>&              host,
        const Xaction<config>*              xaction,
        XactDenialEnum                      denial,
        const FiredResponseFlit<config>&    flit,
        const std::string&                  message) noexcept
        : CacheStateMapDeniedResponseEventBase<config>(host, xaction, denial, flit, message)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedTXUpRSPEvent<config>::CacheStateMapDeniedTXUpRSPEvent(
        CacheStateMap<config>&              host,
        const Xaction<config>*              xaction,
        XactDenialEnum                      denial,
        const FiredResponseFlit<config>&    flit,
        const std::string&                  message) noexcept
        : CacheStateMapDeniedResponseEventBase<config>(host, xaction, denial, flit, message)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedRXDnDATEvent<config>::CacheStateMapDeniedRXDnDATEvent(
        CacheStateMap<config>&              host,
        const Xaction<config>*              xaction,
        XactDenialEnum                      denial,
        const FiredResponseFlit<config>&    flit,
        const std::string&                  message) noexcept
        : CacheStateMapDeniedResponseEventBase<config>(host, xaction, denial, flit, message)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMapDeniedTXUpDATEvent<config>::CacheStateMapDeniedTXUpDATEvent(
        CacheStateMap<config>&              host,
        const Xaction<config>*              xaction,
        XactDenialEnum                      denial,
        const FiredResponseFlit<config>&    flit,
        const std::string&                  message) noexcept
        : CacheStateMapDeniedResponseEventBase<config>(host, xaction, denial, flit, message)
    { }

    template<FlitConfigurationConcept config>
    inline CacheStateMap<config>::EventHub::EventHub() noexcept
        : OnDeniedTXEVT     (0)
        , OnDeniedTXREQ     (0)
        , OnDeniedRXSNP     (0)
        , OnDeniedRXDnRSP   (0)
        , OnDeniedTXUpRSP   (0)
        , OnDeniedRXDnDAT   (0)
        , OnDeniedTXUpDAT   (0)
    { }

    template<FlitConfigurationConcept config>
    inline void CacheStateMap<config>::EventHub::Clear() noexcept
    {
        OnDeniedTXEVT.UnregisterAll();
        OnDeniedTXREQ.UnregisterAll();
        OnDeniedRXSNP.UnregisterAll();
        OnDeniedRXDnRSP.UnregisterAll();
        OnDeniedTXUpRSP.UnregisterAll();
        OnDeniedRXDnDAT.UnregisterAll();
        OnDeniedTXUpDAT.UnregisterAll();
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::DeniedTXEVT(
        XactDenialEnum              denial,
        const Xaction<config>*      xaction,
        uint64_t                    time,
        const Flits::EVT<config>&   flit,
        const std::string&          message) noexcept
    {
        FiredRequestFlit<config> firedFlit(XactScope::Upstream, true, time, flit);

        if (this->events)
            this->events->OnDeniedTXEVT(CacheStateMapDeniedTXEVTEvent<config>(
                *this, xaction, denial, firedFlit, message));

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::DeniedTXREQ(
        XactDenialEnum              denial,
        const Xaction<config>*      xaction,
        uint64_t                    time,
        const Flits::REQ<config>&   flit,
        const std::string&          message) noexcept
    {
        FiredRequestFlit<config> firedFlit(XactScope::Upstream, true, time, flit);

        if (this->events)
            this->events->OnDeniedTXREQ(CacheStateMapDeniedTXREQEvent<config>(
                *this, xaction, denial, firedFlit, message));

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::DeniedRXSNP(
        XactDenialEnum              denial,
        const Xaction<config>*      xaction,
        uint64_t                    time,
        const Flits::SNP<config>&   flit,
        const std::string&          message) noexcept
    {
        FiredRequestFlit<config> firedFlit(XactScope::Upstream, false, time, flit);

        if (this->events)
            this->events->OnDeniedRXSNP(CacheStateMapDeniedRXSNPEvent<config>(
                *this, xaction, denial, firedFlit, message));

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::DeniedRXDnRSP(
        XactDenialEnum              denial,
        const Xaction<config>*      xaction,
        uint64_t                    time,
        const Flits::DnRSP<config>& flit,
        const std::string&          message) noexcept
    {
        FiredResponseFlit<config> firedFlit(XactScope::Upstream, false, time, flit);

        if (this->events)
            this->events->OnDeniedRXDnRSP(CacheStateMapDeniedRXDnRSPEvent<config>(
                *this, xaction, denial, firedFlit, message));

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::DeniedTXUpRSP(
        XactDenialEnum              denial,
        const Xaction<config>*      xaction,
        uint64_t                    time,
        const Flits::UpRSP<config>& flit,
        const std::string&          message) noexcept
    {
        FiredResponseFlit<config> firedFlit(XactScope::Upstream, true, time, flit);

        if (this->events)
            this->events->OnDeniedTXUpRSP(CacheStateMapDeniedTXUpRSPEvent<config>(
                *this, xaction, denial, firedFlit, message));

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::DeniedRXDnDAT(
        XactDenialEnum              denial,
        const Xaction<config>*      xaction,
        uint64_t                    time,
        const Flits::DnDAT<config>& flit,
        const std::string&          message) noexcept
    {
        FiredResponseFlit<config> firedFlit(XactScope::Upstream, false, time, flit);

        if (this->events)
            this->events->OnDeniedRXDnDAT(CacheStateMapDeniedRXDnDATEvent<config>(
                *this, xaction, denial, firedFlit, message));

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::DeniedTXUpDAT(
        XactDenialEnum              denial,
        const Xaction<config>*      xaction,
        uint64_t                    time,
        const Flits::UpDAT<config>& flit,
        const std::string&          message) noexcept
    {
        FiredResponseFlit<config> firedFlit(XactScope::Upstream, true, time, flit);

        if (this->events)
            this->events->OnDeniedTXUpDAT(CacheStateMapDeniedTXUpDATEvent<config>(
                *this, xaction, denial, firedFlit, message));

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline CacheStateMap<config>::CacheStateMap() noexcept
        : events                    (std::make_shared<EventHub>())
        , evtDecoder                ()
        , reqDecoder                ()
        , snpDecoder                ()
        , stateMap                  ()
        , enableSilentEviction      (true)
        , enableSilentSharing       (true)
        , enableSilentStore         (true)
        , enableSilentInvalidation  (true)
        , seerEnabled               (false)
        , seerConfusion             (nullptr)
        , seerAccessedMap           ()
    {
        // EVT transitions
        #define SET_EVT(type, opcode)  { \
            static const details::CohTrans _##opcode { \
                CacheStateTransitions::Initials::opcode, \
                CacheStateTransitions::Intermediates::Nested::opcode, \
                &CacheStateTransitions::Intermediates::opcode, \
                CacheStateTransition::Type::type \
            }; \
            evtDecoder[Opcodes::EVT::opcode].SetCompanion(&_##opcode); \
        }

        SET_EVT(Evict           , Evict                 );  // 0x00
        SET_EVT(WriteBack       , WriteBackFull         );  // 0x01

        #undef SET_EVT

        // REQ transitions
        #define SET_REQ(type, opcode)  { \
            static const details::CohTrans _##opcode { \
                CacheStateTransitions::Initials::opcode, \
                CacheStateTransitions::Intermediates::Nested::opcode, \
                &CacheStateTransitions::Intermediates::opcode, \
                CacheStateTransition::Type::type \
            }; \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&_##opcode); \
        }

        SET_REQ(Stash           , StashShared           );  // 0x00
        SET_REQ(Stash           , StashUnique           );  // 0x01
        SET_REQ(Read            , ReadNoSnp             );  // 0x02
        SET_REQ(Read            , ReadOnce              );  // 0x03
        SET_REQ(Read            , ReadShared            );  // 0x04
                                                            // 0x05
                                                            // 0x06
                                                            // 0x07
        SET_REQ(WriteNonCopyBack, WriteNoSnpPtl         );  // 0x08
        SET_REQ(WriteNonCopyBack, WriteNoSnpFull        );  // 0x09
        SET_REQ(WriteNonCopyBack, WriteUniquePtl        );  // 0x0A
        SET_REQ(WriteNonCopyBack, WriteUniqueFull       );  // 0x0B
        SET_REQ(CMO             , CleanShared           );  // 0x0C
        SET_REQ(CMO             , CleanInvalid          );  // 0x0D
        SET_REQ(CMO             , MakeInvalid           );  // 0x0E
                                                            // 0x0F
        SET_REQ(Read            , ReadUnique            );  // 0x10
                                                            // 0x11
        SET_REQ(Dataless        , MakeUnique            );  // 0x12
                                                            // ...
    //  Atomic* (0x20 .. 0x31): atomics are TBD in the CCHI specification; not modeled.
    //  EvictBack (0x1E) / EvictClean (0x1F): remote evictions carry no local
    //  requester cache-line state; not modeled.

        #undef SET_REQ

        // SNP transitions
        #define SET_SNP(type, opcode)  { \
            static const details::CohTrans _##opcode { \
                CacheStateTransitions::Initials::opcode, \
                CacheStateTransitions::Intermediates::Nested::opcode, \
                &CacheStateTransitions::Intermediates::opcode, \
                CacheStateTransition::Type::type \
            }; \
            snpDecoder[Opcodes::SNP::opcode].SetCompanion(&_##opcode); \
        }

        SET_SNP(Snoop           , SnpMakeInvalid        );  // 0x00
        SET_SNP(Snoop           , SnpToInvalid          );  // 0x01
        SET_SNP(Snoop           , SnpToShared           );  // 0x02
        SET_SNP(Snoop           , SnpToClean            );  // 0x03

        #undef SET_SNP
    }

    template<FlitConfigurationConcept config>
    inline bool CacheStateMap<config>::IsAccessed(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept
    {
        if (!seerEnabled)
            return false;

        auto iter = seerAccessedMap.find(addr >> ADDR_OFFSET_CACHE_MAP);

        if (iter == seerAccessedMap.end())
            return false;

        return iter->second.test(addr & ((1UL << ADDR_OFFSET_CACHE_MAP_SPAN) - 1));
    }

    template<FlitConfigurationConcept config>
    inline void CacheStateMap<config>::SetAccessed(typename Flits::REQ<config>::addr_t::value_type addr) noexcept
    {
        if (!seerEnabled)
            return;

        auto iter = seerAccessedMap.find(addr >> ADDR_OFFSET_CACHE_MAP);

        if (iter == seerAccessedMap.end())
        {
            std::bitset<SIZE_CACHE_MAP_SPAN> newBitset;
            newBitset.set(addr & ((1UL << ADDR_OFFSET_CACHE_MAP_SPAN) - 1));
            seerAccessedMap.emplace(addr >> ADDR_OFFSET_CACHE_MAP, std::move(newBitset));
        }
        else
        {
            iter->second.set(addr & ((1UL << ADDR_OFFSET_CACHE_MAP_SPAN) - 1));
        }
    }

    template<FlitConfigurationConcept config>
    inline CacheState CacheStateMap<config>::EvaluateSilently(CacheState state) const noexcept
    {
        // silent transitions (D7: spec-silent assumptions, all default-on)

        /* Cache eviction */
        if (enableSilentEviction)
        {
            if (state.UC || state.SC)
                state.I = true;
        }

        /* Local sharing */
        if (enableSilentSharing)
        {
            if (state.UC)
                state.SC = true;
        }

        /* Store */
        if (enableSilentStore)
        {
            if (state.UC)
                state.UD = true;
        }

        /* Cache invalidate */
        if (enableSilentInvalidation)
        {
            if (state.UD)
                state.I = true;
        }
        //

        return state;
    }

    template<FlitConfigurationConcept config>
    inline std::pair<CacheState, bool> CacheStateMap<config>::ExcavateWithSeer(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept
    {
        auto iter = stateMap.find(addr);

        if (!seerEnabled)
        {
            if (iter == stateMap.end())
                return { CacheStates::I, false };

            return { iter->second, false };
        }
        else
        {
            if (iter == stateMap.end())
            {
                // If seer mode is enabled, we put the transition on predicted path
                // when the address is not accessed yet in this seer section.
                if (IsAccessed(addr))
                    return { CacheStates::I, false };
                else
                    return { CacheStates::All, true };
            }

            return { iter->second, false };
        }
    }

    template<FlitConfigurationConcept config>
    inline std::pair<CacheState, bool> CacheStateMap<config>::EvaluateWithSeer(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept
    {
        auto excavated = ExcavateWithSeer(addr);
        return { EvaluateSilently(excavated.first), excavated.second };
    }

    template<FlitConfigurationConcept config>
    inline void CacheStateMap<config>::SetSilentEviction(bool enableSilentEviction) noexcept
    {
        this->enableSilentEviction = enableSilentEviction;
    }

    template<FlitConfigurationConcept config>
    inline void CacheStateMap<config>::SetSilentSharing(bool enableSilentSharing) noexcept
    {
        this->enableSilentSharing = enableSilentSharing;
    }

    template<FlitConfigurationConcept config>
    inline void CacheStateMap<config>::SetSilentStore(bool enableSilentStore) noexcept
    {
        this->enableSilentStore = enableSilentStore;
    }

    template<FlitConfigurationConcept config>
    inline void CacheStateMap<config>::SetSilentInvalidation(bool enableSilentInvalidation) noexcept
    {
        this->enableSilentInvalidation = enableSilentInvalidation;
    }

    template<FlitConfigurationConcept config>
    inline bool CacheStateMap<config>::HasSilentEviction() const noexcept
    {
        return this->enableSilentEviction;
    }

    template<FlitConfigurationConcept config>
    inline bool CacheStateMap<config>::HasSilentSharing() const noexcept
    {
        return this->enableSilentSharing;
    }

    template<FlitConfigurationConcept config>
    inline bool CacheStateMap<config>::HasSilentStore() const noexcept
    {
        return this->enableSilentStore;
    }

    template<FlitConfigurationConcept config>
    inline bool CacheStateMap<config>::HasSilentInvalidation() const noexcept
    {
        return this->enableSilentInvalidation;
    }

    template<FlitConfigurationConcept config>
    inline void CacheStateMap<config>::Set(typename Flits::REQ<config>::addr_t::value_type addr, CacheState state) noexcept
    {
        stateMap[addr] = state;
    }

    template<FlitConfigurationConcept config>
    inline CacheState CacheStateMap<config>::Get(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept
    {
        auto iter = stateMap.find(addr);
        return iter != stateMap.end() ? iter->second : CacheStates::None;
    }

    template<FlitConfigurationConcept config>
    inline CacheState CacheStateMap<config>::Excavate(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept
    {
        return ExcavateWithSeer(addr).first;
    }

    template<FlitConfigurationConcept config>
    inline CacheState CacheStateMap<config>::Evaluate(typename Flits::REQ<config>::addr_t::value_type addr) const noexcept
    {
        return EvaluateWithSeer(addr).first;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::NextTXEVT(
        typename Flits::REQ<config>::addr_t::value_type   addr,
        uint64_t                                          time,
        const Flits::EVT<config>&                         flit) noexcept
    {
        // Decode EVT opcode
        const Opcodes::OpcodeInfo<typename Flits::EVT<config>::opcode_t, const details::CohTrans*>& opcodeInfo =
            evtDecoder.Decode(flit.Opcode);

        if (!opcodeInfo.IsValid()) // unknown opcode
            return this->DeniedTXEVT(XactDenial::DENIED_EVT_OPCODE_NOT_DECODED, nullptr, time, flit,
                "This Opcode cannot be decoded by TXEVT");

        //
        const details::CohTrans* trans = opcodeInfo.GetCompanion();

        if (!trans)
            return this->DeniedTXEVT(XactDenial::DENIED_EVT_OPCODE_NOT_SUPPORTED, nullptr, time, flit,
                "This Opcode is not supported by coherency transition for TXEVT");

        //
        const std::pair<CacheState, bool> initialState = EvaluateWithSeer(addr);

        // Initial state check (D11: Evict <- I only, WriteBackFull <- UD only)
        CacheState actualInitialState = trans->initial & initialState.first;

        if (!actualInitialState)
            return XactDenial::DENIED_STATE_INITIAL;

        // Speculative path tracking
        if (initialState.second)
        {
            // TODO
        }

        // State update
        SetAccessed(addr);

        stateMap[addr] = actualInitialState;

        //
        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::NextTXREQ(
        typename Flits::REQ<config>::addr_t::value_type   addr,
        uint64_t                                          time,
        const Flits::REQ<config>&                         flit) noexcept
    {
        // Decode REQ opcode
        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::CohTrans*>& opcodeInfo =
            reqDecoder.Decode(flit.Opcode);

        if (!opcodeInfo.IsValid()) // unknown opcode
            return this->DeniedTXREQ(XactDenial::DENIED_REQ_OPCODE_NOT_DECODED, nullptr, time, flit,
                "This Opcode cannot be decoded by TXREQ");

        //
        const details::CohTrans* trans = opcodeInfo.GetCompanion();

        if (!trans)
            return this->DeniedTXREQ(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, nullptr, time, flit,
                "This Opcode is not supported by coherency transition for TXREQ");

        //
        const std::pair<CacheState, bool> initialState = EvaluateWithSeer(addr);

        // Initial state check
        CacheState actualInitialState = trans->initial & initialState.first;

        if (!actualInitialState)
            return XactDenial::DENIED_STATE_INITIAL;

        // Speculative path tracking
        if (initialState.second)
        {
            // TODO
        }

        // State update
        SetAccessed(addr);

        stateMap[addr] = actualInitialState;

        //
        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::NextRXDnRSP(
        typename Flits::REQ<config>::addr_t::value_type   addr,
        uint64_t                                          time,
        const Xaction<config>&                            xaction,
        const Flits::DnRSP<config>&                       flit) noexcept
    {
        // Decode transition set from TXEVT/TXREQ opcode
        const details::CohTrans* trans;

        if (xaction.GetFirst().IsEVT())
        {
            // Decode EVT opcode
            const Opcodes::OpcodeInfo<typename Flits::EVT<config>::opcode_t, const details::CohTrans*>& opcodeInfo
                = evtDecoder.Decode(xaction.GetFirst().flit.evt.Opcode);

            if (!opcodeInfo.IsValid()) // unknown opcode
                return this->DeniedRXDnRSP(XactDenial::DENIED_EVT_OPCODE_NOT_DECODED, &xaction, time, flit,
                    "This Opcode cannot be decoded by RXDnRSP");

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return this->DeniedRXDnRSP(XactDenial::DENIED_EVT_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                    "This Opcode is not supported by coherency transition for RXDnRSP");
        }
        else if (xaction.GetFirst().IsREQ())
        {
            // Decode REQ opcode
            const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::CohTrans*>& opcodeInfo
                = reqDecoder.Decode(xaction.GetFirst().flit.req.Opcode);

            if (!opcodeInfo.IsValid()) // unknown opcode
                return this->DeniedRXDnRSP(XactDenial::DENIED_REQ_OPCODE_NOT_DECODED, &xaction, time, flit,
                    "This Opcode cannot be decoded by RXDnRSP");

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return this->DeniedRXDnRSP(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                    "This Opcode is not supported by coherency transition for RXDnRSP");
        }
        else
        {
            /*
            * *NOTICE: No RXDnRSP possible on SNP transactions.
            */
            return this->DeniedRXDnRSP(XactDenial::DENIED_CHANNEL_DNRSP, &xaction, time, flit,
                "No RXDnRSP possible on SNP transactions");
        }

        //
        CacheState nextState;

        //
        std::pair<CacheState, bool> prevState; // lazy evaluation

        //
        if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Read)
        {
            // RXDnRSP Opcodes under Read Transactions:
            //  Comp (ReadUnique dataless path only)

            if (flit.Opcode == Opcodes::DnRSP::Comp)
            {
                const CacheStateTransitions::Intermediates::details::TableG0* g0
                    = &static_cast<const CacheStateTransitions::Intermediates::TablesRead*>(trans->tables)->GComp();

                prevState = EvaluateWithSeer(addr);

                CacheResp resp = CacheResp::FromComp(flit.Resp);

                if (!resp)
                    return XactDenial::DENIED_STATE_COMP;

                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                    return XactDenial::DENIED_STATE_COMP;

                nextState = xs;
            }
            else
                return this->DeniedRXDnRSP(XactDenial::DENIED_DNRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for Read transactions on RXDnRSP");
        }
        else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Dataless)
        {
            // RXDnRSP Opcodes under Dataless Transactions:
            //  Comp

            if (flit.Opcode == Opcodes::DnRSP::Comp)
            {
                const CacheStateTransitions::Intermediates::details::TableG0* g0
                    = &static_cast<const CacheStateTransitions::Intermediates::TablesDataless*>(trans->tables)->GComp();

                prevState = EvaluateWithSeer(addr);

                CacheResp resp = CacheResp::FromComp(flit.Resp);

                if (!resp)
                    return XactDenial::DENIED_STATE_COMP;

                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                    return XactDenial::DENIED_STATE_COMP;

                nextState = xs;
            }
            else
                return this->DeniedRXDnRSP(XactDenial::DENIED_DNRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for Dataless transactions on RXDnRSP");
        }
        else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::CMO)
        {
            // RXDnRSP Opcodes under CMO Transactions:
            //  CompCMO

            if (flit.Opcode == Opcodes::DnRSP::CompCMO)
            {
                // D8: the rows encode initial == final, so the G0 product degenerates
                //     to "current (silently widened) states intersected with the
                //     compliant set"; an empty product means the state did not comply
                //     at CompCMO.
                const CacheStateTransitions::Intermediates::details::TableG0* g0
                    = &static_cast<const CacheStateTransitions::Intermediates::TablesCMO*>(trans->tables)->GCompCMO();

                prevState = EvaluateWithSeer(addr);

                CacheResp resp = CacheResp::FromCompCMO(flit.Resp);

                if (!resp)
                    return XactDenial::DENIED_STATE_COMPCMO;

                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                    return XactDenial::DENIED_STATE_COMPCMO;

                nextState = xs;
            }
            else
                return this->DeniedRXDnRSP(XactDenial::DENIED_DNRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for CMO transactions on RXDnRSP");
        }
        else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Stash)
        {
            // RXDnRSP Opcodes under Stash Transactions:
            //  CompStash*
            //
            // * D9: CompStash carries prefetch feedback, not state, and its arrival
            //       does not mark stash completion; accepted without state change.

            if (flit.Opcode == Opcodes::DnRSP::CompStash)
            {
                return XactDenial::ACCEPTED;
            }
            else
                return this->DeniedRXDnRSP(XactDenial::DENIED_DNRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for Stash transactions on RXDnRSP");
        }
        else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::WriteNonCopyBack)
        {
            // RXDnRSP Opcodes under Non-CopyBack Write Transactions:
            //  Comp*, DBIDResp*, CompDBIDResp*
            //
            // * D10/D12: no state-related transitions.

            if (flit.Opcode == Opcodes::DnRSP::Comp
             || flit.Opcode == Opcodes::DnRSP::DBIDResp
             || flit.Opcode == Opcodes::DnRSP::CompDBIDResp)
            {
                return XactDenial::ACCEPTED;
            }
            else
                return this->DeniedRXDnRSP(XactDenial::DENIED_DNRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for WriteNonCopyBack transactions on RXDnRSP");
        }
        else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Evict)
        {
            // RXDnRSP Opcodes under Evict (EVT) Transactions:
            //  Comp (D12: no state carried, Resp = I assumed)

            if (flit.Opcode == Opcodes::DnRSP::Comp)
            {
                const CacheStateTransitions::Intermediates::details::TableG0* g0
                    = &static_cast<const CacheStateTransitions::Intermediates::TablesEvict*>(trans->tables)->GComp();

                prevState = EvaluateWithSeer(addr);

                CacheResp resp = CacheResp::FromComp(flit.Resp);

                if (!resp)
                    return XactDenial::DENIED_STATE_COMP;

                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                    return XactDenial::DENIED_STATE_COMP;

                nextState = xs;
            }
            else
                return this->DeniedRXDnRSP(XactDenial::DENIED_DNRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for Evict transactions on RXDnRSP");
        }
        else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::WriteBack)
        {
            // RXDnRSP Opcodes under WriteBack (EVT) Transactions:
            //  Comp*, DBIDResp*, CompDBIDResp*
            //
            // * D12: no state-related transitions; the UD -> I transition happens on
            //        CopyBackWrData (TXUpDAT).

            if (flit.Opcode == Opcodes::DnRSP::Comp
             || flit.Opcode == Opcodes::DnRSP::DBIDResp
             || flit.Opcode == Opcodes::DnRSP::CompDBIDResp)
            {
                return XactDenial::ACCEPTED;
            }
            else
                return this->DeniedRXDnRSP(XactDenial::DENIED_DNRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for WriteBack transactions on RXDnRSP");
        }
        else
            return this->DeniedRXDnRSP(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                "This Opcode's coherency transition table type not recognized for RXDnRSP");

        // Speculative path tracking
        if (prevState.second)
        {
            // TODO
        }

        // State update
        SetAccessed(addr);

        stateMap[addr] = nextState;

        //
        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::NextTXUpRSP(
        typename Flits::REQ<config>::addr_t::value_type   addr,
        uint64_t                                          time,
        const Xaction<config>&                            xaction,
        const Flits::UpRSP<config>&                       flit) noexcept
    {
        if (xaction.GetFirst().IsSNP())
        {
            // Decode SNP opcode
            const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, const details::CohTrans*>& opcodeInfo
                = snpDecoder.Decode(xaction.GetFirst().flit.snp.Opcode);

            if (!opcodeInfo.IsValid()) // unknown opcode
                return this->DeniedTXUpRSP(XactDenial::DENIED_SNP_OPCODE_NOT_DECODED, &xaction, time, flit,
                    "This Opcode cannot be decoded by TXUpRSP");

            const details::CohTrans* trans = opcodeInfo.GetCompanion();

            if (!trans)
                return this->DeniedTXUpRSP(XactDenial::DENIED_SNP_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                    "This Opcode is not supported by coherency transition for TXUpRSP");

            // TXUpRSP Opcodes under Snoop Transactions:
            //  SnpResp

            if (flit.Opcode != Opcodes::UpRSP::SnpResp)
                return this->DeniedTXUpRSP(XactDenial::DENIED_UPRSP_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for Snoop transactions on TXUpRSP");

            //
            const CacheStateTransitions::Intermediates::details::TableG0* g0
                = &static_cast<const CacheStateTransitions::Intermediates::TablesSnp*>(trans->tables)->GSnpResp();

            // *NOTICE: keyed by the current (possibly nested-mutated) state, which is
            //          the snoop's pre-reply state (structure.md "回复前状态" column).
            std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

            CacheResp resp = CacheResp::FromSnpResp(flit.Resp);

            if (!resp)
                return XactDenial::DENIED_STATE_RESP_SNPRESP;

            CacheState xs
                = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

            if (!xs)
                return XactDenial::DENIED_STATE_RESP_SNPRESP;

            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = xs;

            //
            return XactDenial::ACCEPTED;
        }
        else
        {
            // TXUpRSP Opcodes under EVT/REQ Transactions:
            //  CompAck*
            //
            // * D12: CompAck has no state effect.

            if (flit.Opcode == Opcodes::UpRSP::CompAck)
                return XactDenial::ACCEPTED;

            return this->DeniedTXUpRSP(XactDenial::DENIED_UPRSP_OPCODE, &xaction, time, flit,
                "This Opcode is unexpected for EVT/REQ transactions on TXUpRSP");
        }
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::NextRXDnDAT(
        typename Flits::REQ<config>::addr_t::value_type   addr,
        uint64_t                                          time,
        const Xaction<config>&                            xaction,
        const Flits::DnDAT<config>&                       flit) noexcept
    {
        if (!xaction.GetFirst().IsREQ())
        {
            /*
            * *NOTICE: No RXDnDAT possible on EVT/SNP transactions.
            */
            return this->DeniedRXDnDAT(XactDenial::DENIED_CHANNEL_DNDAT, &xaction, time, flit,
                "No RXDnDAT possible on EVT/SNP transactions");
        }

        // Decode REQ opcode
        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::CohTrans*>& opcodeInfo
            = reqDecoder.Decode(xaction.GetFirst().flit.req.Opcode);

        if (!opcodeInfo.IsValid()) // unknown opcode
            return this->DeniedRXDnDAT(XactDenial::DENIED_REQ_OPCODE_NOT_DECODED, &xaction, time, flit,
                "This Opcode cannot be decoded by RXDnDAT");

        const details::CohTrans* trans = opcodeInfo.GetCompanion();

        if (!trans)
            return this->DeniedRXDnDAT(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                "This Opcode is not supported by coherency transition for RXDnDAT");

        // RXDnDAT Opcodes under REQ Transactions:
        //  CompData (Read transactions only)

        if (flit.Opcode != Opcodes::DnDAT::CompData)
            return this->DeniedRXDnDAT(XactDenial::DENIED_DNDAT_OPCODE, &xaction, time, flit,
                "This Opcode is unexpected on RXDnDAT");

        // check for multi-data-beat repeat (transition is applied once, on the first beat)
        const FiredResponseFlit<config>* firstDAT = xaction.GetFirstDnDAT({ flit.Opcode });

        if (firstDAT && firstDAT->flit.dndat.DataID != flit.DataID)
        {
            if (firstDAT->flit.dndat.Resp != flit.Resp)
                return XactDenial::DENIED_STATE_MISMATCH_REPEAT;

            return XactDenial::ACCEPTED;
        }

        if (trans->tables->type != CacheStateTransitions::Intermediates::Tables::Type::Read)
            return this->DeniedRXDnDAT(XactDenial::DENIED_DNDAT_OPCODE, &xaction, time, flit,
                "CompData is unexpected for non-Read transactions on RXDnDAT");

        //
        const CacheStateTransitions::Intermediates::details::TableG0* g0
            = &static_cast<const CacheStateTransitions::Intermediates::TablesRead*>(trans->tables)->GCompData();

        //
        std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

        CacheResp resp = CacheResp::FromCompData(flit.Resp);

        if (!resp)
            return XactDenial::DENIED_STATE_COMPDATA;

        CacheState xs
            = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

        if (!xs)
            return XactDenial::DENIED_STATE_COMPDATA;

        // Speculative path tracking
        if (prevState.second)
        {
            // TODO
        }

        // State update
        SetAccessed(addr);

        stateMap[addr] = xs;

        //
        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::NextTXUpDAT(
        typename Flits::REQ<config>::addr_t::value_type   addr,
        uint64_t                                          time,
        const Xaction<config>&                            xaction,
        const Flits::UpDAT<config>&                       flit) noexcept
    {
        // Decode transition set from TXEVT/TXREQ/RXSNP opcode
        const details::CohTrans* trans;

        if (xaction.GetFirst().IsEVT())
        {
            // Decode EVT opcode
            const Opcodes::OpcodeInfo<typename Flits::EVT<config>::opcode_t, const details::CohTrans*>& opcodeInfo
                = evtDecoder.Decode(xaction.GetFirst().flit.evt.Opcode);

            if (!opcodeInfo.IsValid()) // unknown opcode
                return this->DeniedTXUpDAT(XactDenial::DENIED_EVT_OPCODE_NOT_DECODED, &xaction, time, flit,
                    "This Opcode cannot be decoded by TXUpDAT");

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return this->DeniedTXUpDAT(XactDenial::DENIED_EVT_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                    "This Opcode is not supported by coherency transition for TXUpDAT");

            //
            CacheState nextState;

            //
            std::pair<CacheState, bool> prevState; // lazy evaluation

            if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::WriteBack)
            {
                // TXUpDAT Opcodes under WriteBack Transactions:
                //  CopyBackWrData

                if (flit.Opcode != Opcodes::UpDAT::CopyBackWrData)
                    return this->DeniedTXUpDAT(XactDenial::DENIED_UPDAT_OPCODE, &xaction, time, flit,
                        "This Opcode is unexpected for WriteBack transactions on TXUpDAT");

                // check for multi-data-beat repeat (transition is applied once, on the first beat)
                const FiredResponseFlit<config>* firstDAT = xaction.GetFirstUpDAT({ flit.Opcode });

                if (firstDAT && firstDAT->flit.updat.DataID != flit.DataID)
                {
                    if (firstDAT->flit.updat.Resp != flit.Resp)
                        return XactDenial::DENIED_STATE_MISMATCH_REPEAT;

                    return XactDenial::ACCEPTED;
                }

                const CacheStateTransitions::Intermediates::details::TableG0* g0
                    = &static_cast<const CacheStateTransitions::Intermediates::TablesWriteBack*>(trans->tables)->GCopyBackWrData();

                prevState = EvaluateWithSeer(addr);

                CacheResp resp = CacheResp::FromCopyBackWrData(flit.Resp);

                if (!resp)
                    return XactDenial::DENIED_STATE_COPYBACKWRDATA;

                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                    return XactDenial::DENIED_STATE_COPYBACKWRDATA;

                nextState = xs;
            }
            else
                return this->DeniedTXUpDAT(XactDenial::DENIED_CHANNEL_UPDAT, &xaction, time, flit,
                    "No TXUpDAT possible on Evict transactions");

            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = nextState;

            //
            return XactDenial::ACCEPTED;
        }
        else if (xaction.GetFirst().IsREQ())
        {
            // Decode REQ opcode
            const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::CohTrans*>& opcodeInfo
                = reqDecoder.Decode(xaction.GetFirst().flit.req.Opcode);

            if (!opcodeInfo.IsValid()) // unknown opcode
                return this->DeniedTXUpDAT(XactDenial::DENIED_REQ_OPCODE_NOT_DECODED, &xaction, time, flit,
                    "This Opcode cannot be decoded by TXUpDAT");

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return this->DeniedTXUpDAT(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                    "This Opcode is not supported by coherency transition for TXUpDAT");

            if (trans->tables->type != CacheStateTransitions::Intermediates::Tables::Type::WriteNonCopyBack)
                return this->DeniedTXUpDAT(XactDenial::DENIED_UPDAT_OPCODE, &xaction, time, flit,
                    "No TXUpDAT is expected for this transaction type");

            // TXUpDAT Opcodes under Non-CopyBack Write Transactions:
            //  NonCopyBackWrData*
            //
            // * D10: checks for I state only, no state transition.

            if (flit.Opcode != Opcodes::UpDAT::NonCopyBackWrData)
                return this->DeniedTXUpDAT(XactDenial::DENIED_UPDAT_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for WriteNonCopyBack transactions on TXUpDAT");

            // check for multi-data-beat repeat (check is applied once, on the first beat)
            const FiredResponseFlit<config>* firstDAT = xaction.GetFirstUpDAT({ flit.Opcode });

            if (firstDAT && firstDAT->flit.updat.DataID != flit.DataID)
            {
                if (firstDAT->flit.updat.Resp != flit.Resp)
                    return XactDenial::DENIED_STATE_MISMATCH_REPEAT;

                return XactDenial::ACCEPTED;
            }

            //
            std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

            if (!(prevState.first & CacheStates::I))
                return XactDenial::DENIED_STATE_NONCOPYBACKWRDATA;

            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = CacheStates::I;

            //
            return XactDenial::ACCEPTED;
        }
        else
        {
            // Decode SNP opcode
            const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, const details::CohTrans*>& opcodeInfo
                = snpDecoder.Decode(xaction.GetFirst().flit.snp.Opcode);

            if (!opcodeInfo.IsValid()) // unknown opcode
                return this->DeniedTXUpDAT(XactDenial::DENIED_SNP_OPCODE_NOT_DECODED, &xaction, time, flit,
                    "This Opcode cannot be decoded by TXUpDAT");

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return this->DeniedTXUpDAT(XactDenial::DENIED_SNP_OPCODE_NOT_SUPPORTED, &xaction, time, flit,
                    "This Opcode is not supported by coherency transition for TXUpDAT");

            // TXUpDAT Opcodes under Snoop Transactions:
            //  SnpRespData

            if (flit.Opcode != Opcodes::UpDAT::SnpRespData)
                return this->DeniedTXUpDAT(XactDenial::DENIED_UPDAT_OPCODE, &xaction, time, flit,
                    "This Opcode is unexpected for Snoop transactions on TXUpDAT");

            // check for multi-data-beat repeat (transition is applied once, on the first beat)
            const FiredResponseFlit<config>* firstDAT = xaction.GetFirstUpDAT({ flit.Opcode });

            if (firstDAT && firstDAT->flit.updat.DataID != flit.DataID)
            {
                if (firstDAT->flit.updat.Resp != flit.Resp)
                    return XactDenial::DENIED_STATE_MISMATCH_REPEAT;

                return XactDenial::ACCEPTED;
            }

            //
            const CacheStateTransitions::Intermediates::details::TableG0* g0
                = &static_cast<const CacheStateTransitions::Intermediates::TablesSnp*>(trans->tables)->GSnpRespData();

            // *NOTICE: keyed by the current (possibly nested-mutated) state, which is
            //          the snoop's pre-reply state (structure.md "回复前状态" column).
            std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

            CacheResp resp = CacheResp::FromSnpRespData(flit.Resp);

            if (!resp)
                return XactDenial::DENIED_STATE_RESP_SNPRESPDATA;

            CacheState xs
                = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

            if (!xs)
                return XactDenial::DENIED_STATE_RESP_SNPRESPDATA;

            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = xs;

            //
            return XactDenial::ACCEPTED;
        }
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::NextRXSNP(
        [[maybe_unused]] typename Flits::REQ<config>::addr_t::value_type  addr,
        [[maybe_unused]] uint64_t                                         time,
        [[maybe_unused]] const Flits::SNP<config>&                        flit) noexcept
    {
        // *NOTICE: No state transition or check on snoop arrival; the tracked state is
        //          NOT narrowed here. Snoop responses are checked against the current
        //          (possibly nested-mutated) state, and nested evictions/writebacks
        //          during the snoop go through Transfer() with G2 checks.
        //          The SNP Addr field is 45-bit with bits [2:0] omitted; callers pass
        //          the byte address as (snp.Addr << 3).
        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum CacheStateMap<config>::Transfer(
        typename Flits::REQ<config>::addr_t::value_type   addr,
        CacheState                                        state,
        const Xaction<config>*                            nestingXaction,
        uint64_t                                          time) noexcept
    {
        if (!nestingXaction)
        {
            Set(addr, state);
            return XactDenial::ACCEPTED;
        }
        else
        {
            if (nestingXaction->GetFirst().IsSNP())
            {
                // Decode SNP opcode
                const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, const details::CohTrans*>& opcodeInfo
                    = snpDecoder.Decode(nestingXaction->GetFirst().flit.snp.Opcode);

                if (!opcodeInfo.IsValid()) // unknown opcode
                    return this->DeniedRXSNP(XactDenial::DENIED_SNP_OPCODE_NOT_DECODED, nestingXaction, time, nestingXaction->GetFirst().flit.snp,
                        "This Opcode cannot be decoded by Transfer");

                const details::CohTrans* trans = opcodeInfo.GetCompanion();

                if (!trans)
                    return this->DeniedRXSNP(XactDenial::DENIED_SNP_OPCODE_NOT_SUPPORTED, nestingXaction, time, nestingXaction->GetFirst().flit.snp,
                        "This Opcode is not supported by coherency transition for Transfer");

                //
                std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

                CacheState xi
                    = CacheStateTransitions::Intermediates::details::ProductG2(prevState.first, trans->nested, state);

                if (!xi)
                    return XactDenial::DENIED_STATE_NESTED_TRANSFER;

                //
                Set(addr, xi);

                return XactDenial::ACCEPTED;
            }
            else
            {
                // *NOTICE: Passthrough on EVT/REQ nesting - no intermediate-state model
                //          for EVT/REQ-nested transfers in CCHI; the transfer target is
                //          authoritative.
                Set(addr, state);
                return XactDenial::ACCEPTED;
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE
