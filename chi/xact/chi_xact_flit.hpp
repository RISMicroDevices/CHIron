//#pragma once

//#ifndef __CHI__CHI_XACT_FLIT
//#define __CHI__CHI_XACT_FLIT

#ifndef CHI_XACT_FLIT__STANDALONE
#   include "chi_xact_flit_header.hpp"          // IWYU pragma: keep
#   include "chi_xact_base_header.hpp"          // IWYU pragma: keep
#   include "chi_xact_base.hpp"                 // IWYU pragma: keep
#   include "chi_xact_global_header.hpp"        // IWYU pragma: keep
#   include "chi_xact_global.hpp"               // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_FLIT_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_FLIT_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_FLIT_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_FLIT_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        class FiredFlit {
        public:
            XactScopeEnum   scope;
            uint64_t        time;
            bool            isTX;

        public:
            FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept;

        public:
            bool            IsTX() const noexcept;
            bool            IsRX() const noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class FiredRequestFlit : public FiredFlit<config, conn> {
        public:
            bool            isREQ;
            union {
                Flits::REQ<config, conn>    req;
                Flits::SNP<config, conn>    snp;
            }               flit;
            
            // REQ: Request source Node ID
            // SNP: Snoop target Node ID
            Flits::REQ<config, conn>::srcid_t
                            nodeId;

        public:
            FiredRequestFlit(
                XactScopeEnum                       scope,
                bool                                isTX,
                uint64_t                            time,
                const Flits::REQ<config, conn>&     reqFlit
            ) noexcept;
            
            FiredRequestFlit(
                XactScopeEnum                       scope,
                bool                                isTX,
                uint64_t                            time,
                const Flits::SNP<config, conn>&     snpFlit,
                Flits::REQ<config, conn>::srcid_t   snpTgtId
            ) noexcept;

        public:
            bool            IsREQ() const noexcept;
            bool            IsSNP() const noexcept;

            bool            IsTXREQ() const noexcept;
            bool            IsRXREQ() const noexcept;
            bool            IsTXSNP() const noexcept;
            bool            IsRXSNP() const noexcept;

        public:
            bool            IsToRequester(const Topology& topo) const noexcept;
            bool            IsToHome(const Topology& topo) const noexcept;
            bool            IsToSubordinate(const Topology& topo) const noexcept;

        public:
            bool            IsFromRequester(const Topology& topo) const noexcept;
            bool            IsFromHome(const Topology& topo) const noexcept;
            bool            IsFromSubordinate(const Topology& topo) const noexcept;

        public:
            bool            IsFrom(const Topology& topo, XactScopeEnum from) const noexcept;
            bool            IsTo(const Topology& topo, XactScopeEnum to) const noexcept;
            bool            IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Topology& topo) const noexcept;
            bool            IsFromRequesterToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromHomeToRequester(const Topology& topo) const noexcept;
            bool            IsFromHomeToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToRequester(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToHome(const Topology& topo) const noexcept;

        public:
            bool            IsTXREQ(const Topology& topo, XactScopeEnum scope) const noexcept;
            bool            IsRXREQ(const Topology& topo, XactScopeEnum scope) const noexcept;
            bool            IsTXSNP(const Topology& topo, XactScopeEnum scope) const noexcept;
            bool            IsRXSNP(const Topology& topo, XactScopeEnum scope) const noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class FiredResponseFlit : public FiredFlit<config, conn> {
        public:
            bool            isRSP;
            union {
                Flits::RSP<config, conn>    rsp;
                Flits::DAT<config, conn>    dat;
            }               flit;

        public:
            FiredResponseFlit() noexcept;
            FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::RSP<config, conn>& rspFlit) noexcept;
            FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DAT<config, conn>& datFlit) noexcept;
        
        public:
            bool            IsRSP() const noexcept;
            bool            IsDAT() const noexcept;

            bool            IsTXRSP() const noexcept;
            bool            IsRXRSP() const noexcept;
            bool            IsTXDAT() const noexcept;
            bool            IsRXDAT() const noexcept;

        public:
            bool            IsToRequester(const Topology& topo) const noexcept;
            bool            IsToHome(const Topology& topo) const noexcept;
            bool            IsToSubordinate(const Topology& topo) const noexcept;

            bool            IsToRequesterDCT(const Topology& topo) const noexcept;
            bool            IsToRequesterDMT(const Topology& topo) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsToRequesterDWT(const Topology& topo) const noexcept;
            bool            IsToSubordinateDWT(const Topology& topo) const noexcept;
#endif

        public:
            bool            IsFromRequester(const Topology& topo) const noexcept;
            bool            IsFromHome(const Topology& topo) const noexcept;
            bool            IsFromSubordinate(const Topology& topo) const noexcept;

            bool            IsFromRequesterDCT(const Topology& topo) const noexcept;
            bool            IsFromSubordinateDMT(const Topology& topo) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsFromRequesterDWT(const Topology& topo) const noexcept;
            bool            IsFromSubordinateDWT(const Topology& topo) const noexcept;
#endif

        public:
            bool            IsFrom(const Topology& topo, XactScopeEnum from) const noexcept;
            bool            IsTo(const Topology& topo, XactScopeEnum to) const noexcept;
            bool            IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Topology& topo) const noexcept;
            bool            IsFromRequesterToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromHomeToRequester(const Topology& topo) const noexcept;
            bool            IsFromHomeToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToRequester(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToHome(const Topology& topo) const noexcept;

        public:
            bool            IsTXRSP(const Topology& topo, XactScopeEnum scope) const noexcept;
            bool            IsRXRSP(const Topology& topo, XactScopeEnum scope) const noexcept;
            bool            IsTXDAT(const Topology& topo, XactScopeEnum scope) const noexcept;
            bool            IsRXDAT(const Topology& topo, XactScopeEnum scope) const noexcept;
        };
    }
/*
}
*/


// Implementation of: class FiredFlit
namespace /*CHI::*/Xact {
    /*
    XactScopeEnum   scope;
    uint64_t        time;
    bool            isTX;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredFlit<config, conn>::FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept
        : scope     (scope)
        , time      (time)
        , isTX      (isTX)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredFlit<config, conn>::IsTX() const noexcept
    {
        return isTX;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredFlit<config, conn>::IsRX() const noexcept
    {
        return !isTX;
    }
}


// Implementation of: class FiredRequestFlit
namespace /*CHI::*/Xact {
    /*
    bool        isREQ;
    union {
        Flits::REQ<config, conn>    req;
        Flits::SNP<config, conn>    snp;
    }           flit;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredRequestFlit<config, conn>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::REQ<config, conn>& reqFlit) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isREQ                     (true)
        , nodeId                    (reqFlit.SrcID())
    { 
        flit.req = reqFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredRequestFlit<config, conn>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::SNP<config, conn>& snpFlit, Flits::REQ<config, conn>::srcid_t snpTgtId) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isREQ                     (false)
        , nodeId                    (snpTgtId)
    {
        flit.snp = snpFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsREQ() const noexcept
    {
        return isREQ;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsSNP() const noexcept
    {
        return !isREQ;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXREQ() const noexcept
    {
        return this->IsTX() && IsREQ();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXREQ() const noexcept
    {
        return this->IsRX() && IsREQ();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXSNP() const noexcept
    {
        return this->IsTX() && IsSNP();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXSNP() const noexcept
    {
        return this->IsRX() && IsSNP();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToRequester(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRX();

            case XactScope::Home:
                return this->IsTXREQ() && topo.IsRequester(flit.req.TgtID()) 
                    || this->IsTXSNP();

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToHome(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                // Don't check TgtID of TXREQ, RN E-SAM could remap this on network
                return this->IsTXREQ() /*&& topo.IsHome(flit.req.TgtId())*/;

            case XactScope::Home:
                return this->IsRXREQ();

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToSubordinate(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return false;

            case XactScope::Home:
                return this->IsTXREQ() && topo.IsSubordinate(flit.req.TgtID());

            case XactScope::Subordinate:
                return this->IsRXREQ();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequester(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsTXREQ();

            case XactScope::Home:
                return this->IsRXREQ();

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHome(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRXSNP();

            case XactScope::Home:
                return this->IsTXSNP();

            case XactScope::Subordinate:
                return this->IsRXREQ();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinate(const Topology& topo) const noexcept
    {
        // Requests never come from Subordnates.
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFrom(const Topology& topo, XactScopeEnum from) const noexcept
    {
        switch (from->value)
        {
            case XactScope::Requester:
                if (!IsFromRequester(topo))
                    return false;

            case XactScope::Home:
                if (!IsFromHome(topo))
                    return false;

            case XactScope::Subordinate:
                if (!IsFromSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTo(const Topology& topo, XactScopeEnum to) const noexcept
    {
        switch (to->value)
        {
            case XactScope::Requester:
                if (!IsToRequester(topo))
                    return false;
            
            case XactScope::Home:
                if (!IsToHome(topo))
                    return false;
            
            case XactScope::Subordinate:
                if (!IsToSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        return IsFrom(topo, from) && IsTo(topo, to);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequesterToHome(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToHome(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequesterToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHomeToRequester(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHomeToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinateToRequester(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinateToHome(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToHome(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXREQ(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsREQ() && IsFrom(topo, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXREQ(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsREQ() && IsTo(topo, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXSNP(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsSNP() && IsFrom(topo, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXSNP(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsSNP() && IsTo(topo, scope);
    }
}

// Implementation of: class FiredResponseFlit
namespace /*CHI::*/Xact {
    /*
    bool        isRSP;
    union {
        Flits::RSP<config, conn>    rsp;
        Flits::DAT<config, conn>    dat;
    }           flit;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredResponseFlit<config, conn>::FiredResponseFlit() noexcept
        : FiredFlit<config, conn>   (XactScope::Requester, false, 0)
        , isRSP                     (false)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredResponseFlit<config, conn>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::RSP<config, conn>& rspFlit) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isRSP                     (true)
    {
        flit.rsp = rspFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredResponseFlit<config, conn>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DAT<config, conn>& datFlit) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isRSP                     (false)
    {
        flit.dat = datFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRSP() const noexcept
    {
        return isRSP;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsDAT() const noexcept
    {
        return !isRSP;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXRSP() const noexcept
    {
        return this->IsTX() && IsRSP();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXRSP() const noexcept
    {
        return this->IsRX() && IsRSP();
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXDAT() const noexcept
    {
        return this->IsTX() && IsDAT();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXDAT() const noexcept
    {
        return this->IsRX() && IsDAT();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequester(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRX()
                    || IsToRequesterDCT(topo);

            case XactScope::Home:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID());

            case XactScope::Subordinate:
                return IsToRequesterDMT(topo)
#ifdef CHI_ISSUE_B_ENABLE
                    ;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                    || IsToRequesterDWT(topo);
#endif

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToHome(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && topo.IsHome(flit.rsp.TgtID()) 
                    || this->IsTXDAT() && topo.IsHome(flit.dat.TgtID());

            case XactScope::Home:
                return this->IsRX();

            case XactScope::Subordinate:
                return this->IsTXRSP() && topo.IsHome(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsHome(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToSubordinate(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
#ifdef CHI_ISSUE_B_ENABLE
                return false;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                return IsToSubordinateDWT(topo);
#endif

            case XactScope::Home:
                return this->IsTXRSP() && topo.IsSubordinate(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsSubordinate(flit.dat.TgtID());

            case XactScope::Subordinate:
                return this->IsRX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDCT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && topo.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDMT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && topo.IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && topo.IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToSubordinateDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && topo.IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequester(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTX()
                    || IsFromRequesterDCT(topo);

            case XactScope::Home:
                return this->IsRXRSP() && topo.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            case XactScope::Subordinate:
                return IsFromRequesterDWT(topo);

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHome(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && topo.IsHome(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsHome(flit.dat.SrcID());

            case XactScope::Home:
                return this->IsTX();

            case XactScope::Subordinate:
                return this->IsRXDAT() && topo.IsHome(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinate(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
#ifdef CHI_ISSUE_B_ENABLE
                return false;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                return IsFromSubordinateDWT(topo);
#endif

            case XactScope::Home:
                return this->IsRXRSP() && topo.IsSubordinate(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsSubordinate(flit.dat.SrcID());

            case XactScope::Subordinate:
                return this->IsTX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterDCT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && topo.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateDMT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && topo.IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->isTXDAT() && topo.IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;   
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && topo.IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif
    
#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && topo.IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFrom(const Topology& topo, XactScopeEnum from) const noexcept
    {
        switch (from->value)
        {
            case XactScope::Requester:
                if (!IsFromRequester(topo))
                    return false;

            case XactScope::Home:
                if (!IsFromHome(topo))
                    return false;

            case XactScope::Subordinate:
                if (!IsFromSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTo(const Topology& topo, XactScopeEnum to) const noexcept
    {
        switch (to->value)
        {
            case XactScope::Requester:
                if (!IsToRequester(topo))
                    return false;
            
            case XactScope::Home:
                if (!IsToHome(topo))
                    return false;
            
            case XactScope::Subordinate:
                if (!IsToSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        return IsFrom(topo, from) && IsTo(topo, to);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterToHome(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToHome(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHomeToRequester(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHomeToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateToRequester(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateToHome(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToHome(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXRSP(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsRSP() && IsFrom(topo, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXRSP(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsRSP() && IsTo(topo, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXDAT(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsDAT() && IsFrom(topo, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXDAT(const Topology& topo, XactScopeEnum scope) const noexcept
    {
        return IsDAT() && IsTo(topo, scope);
    }
}


#endif

//#endif // __CHI__CHI_XACT_FLIT
