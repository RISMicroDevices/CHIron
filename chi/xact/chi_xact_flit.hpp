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
            bool            IsToRequester(const Global<config, conn>&) const noexcept;
            bool            IsToHome(const Global<config, conn>&) const noexcept;
            bool            IsToSubordinate(const Global<config, conn>&) const noexcept;

        public:
            bool            IsFromRequester(const Global<config, conn>&) const noexcept;
            bool            IsFromHome(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinate(const Global<config, conn>&) const noexcept;

        public:
            bool            IsFrom(const Global<config, conn>&, XactScopeEnum from) const noexcept;
            bool            IsTo(const Global<config, conn>&, XactScopeEnum to) const noexcept;
            bool            IsFromTo(const Global<config, conn>&, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Global<config, conn>&) const noexcept;
            bool            IsFromRequesterToSubordinate(const Global<config, conn>&) const noexcept;
            bool            IsFromHomeToRequester(const Global<config, conn>&) const noexcept;
            bool            IsFromHomeToSubordinate(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinateToRequester(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinateToHome(const Global<config, conn>&) const noexcept;

        public:
            bool            IsTXREQ(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
            bool            IsRXREQ(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
            bool            IsTXSNP(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
            bool            IsRXSNP(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
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
            bool            IsToRequester(const Global<config, conn>&) const noexcept;
            bool            IsToHome(const Global<config, conn>&) const noexcept;
            bool            IsToSubordinate(const Global<config, conn>&) const noexcept;

            bool            IsToRequesterDCT(const Global<config, conn>&) const noexcept;
            bool            IsToRequesterDMT(const Global<config, conn>&) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsToRequesterDWT(const Global<config, conn>&) const noexcept;
            bool            IsToSubordinateDWT(const Global<config, conn>&) const noexcept;
#endif

        public:
            bool            IsFromRequester(const Global<config, conn>&) const noexcept;
            bool            IsFromHome(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinate(const Global<config, conn>&) const noexcept;

            bool            IsFromRequesterDCT(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinateDMT(const Global<config, conn>&) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsFromRequesterDWT(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinateDWT(const Global<config, conn>&) const noexcept;
#endif

        public:
            bool            IsFrom(const Global<config, conn>&, XactScopeEnum from) const noexcept;
            bool            IsTo(const Global<config, conn>&, XactScopeEnum to) const noexcept;
            bool            IsFromTo(const Global<config, conn>&, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Global<config, conn>&) const noexcept;
            bool            IsFromRequesterToSubordinate(const Global<config, conn>&) const noexcept;
            bool            IsFromHomeToRequester(const Global<config, conn>&) const noexcept;
            bool            IsFromHomeToSubordinate(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinateToRequester(const Global<config, conn>&) const noexcept;
            bool            IsFromSubordinateToHome(const Global<config, conn>&) const noexcept;

        public:
            bool            IsTXRSP(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
            bool            IsRXRSP(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
            bool            IsTXDAT(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
            bool            IsRXDAT(const Global<config, conn>&, XactScopeEnum scope) const noexcept;
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
    inline bool FiredRequestFlit<config, conn>::IsToRequester(const Global<config, conn>& glbl) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRXSNP();

            case XactScope::Home:
                return this->IsTXSNP();

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToHome(const Global<config, conn>& glbl) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:

                if (this->IsTXREQ())
                {
                    if (glbl.SAM_SCOPE->enable)
                    {
                        switch (glbl.SAM_SCOPE->Get(flit.req.SrcID())->value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY->IsHome(flit.req.TgtID());

                            case SAMScope::BeforeSAM:
                                return true; // Do not check TgtID of TXREQ before SAM

                            [[unlikely]] default:
                                return false;
                        }
                    }
                    else
                        return true; // Do not check TgtID of TXREQ on SAM check disabled
                }
                else
                    return false;

            case XactScope::Home:
                
                if (this->IsRXREQ())
                {
                    if (glbl.SAM_SCOPE->enable)
                    {
                        switch (glbl.SAM_SCOPE->Get(flit.req.SrcID())->value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY->IsHome(flit.req.TgtID());

                            case SAMScope::BeforeSAM:
                                return true; // Do not check TgtID of RXREQ before SAM

                            [[unlikely]] default:
                                return false;
                        }
                    }
                    else
                        return true; // Do not check TgtID of RXREQ on SAM check disabled
                }
                else
                    return false;

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester: // For PrefetchTgt only in standard AMBA CHI

                if (this->IsTXREQ())
                {
                    if (glbl.SAM_SCOPE->enable)
                    {
                        case SAMScope::AfterSAM:
                            return glbl.TOPOLOGY->IsSubordinate(flit.req.TgtID());

                        case SAMScope::BeforeSAM:
                            return flit.req.Opcode() == Opcodes::REQ::PrefetchTgt; // Do not check TgtID of TXREQ before SAM

                        [[unlikely]] default:
                            return false;
                    }
                    else
                        return flit.req.Opcode() == Opcodes::REQ::PrefetchTgt; // Do not check TgtID of TXREQ on SAM check disabled
                }
                else
                    return false;

            case XactScope::Home: // Except PrefetchTgt

                if (this->IsTXREQ())
                {
                    if (glbl.SAM_SCOPE->enable)
                    {
                        switch (glbl.SAM_SCOPE->Get(flit.req.SrcID())->value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY->IsSubordinate(flit.req.TgtID());

                            case SAMScope::BeforeSAM:
                                return flit.req.Opcode() != Opcodes::REQ::PrefetchTgt; // Do not check TgtID of TXREQ before SAM

                            [[unlikely]] default:
                                return false;
                        }
                    }
                    else
                        return flit.req.Opcode() != Opcodes::REQ::PrefetchTgt; // Do not check TgtID of TXREQ on SAM check disabled
                }
                else
                    return false;

            case XactScope::Subordinate:

                if (this->IsRXREQ())
                {
                    if (glbl.SAM_SCOPE->enable)
                    {
                        switch (glbl.SAM_SCOPE->Get(flit.req.SrcID())->value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY->IsSubordinate(flit.req.TgtID());

                            case SAMScope::BeforeSAM:
                                return true; // Do not check TgtID of RXREQ before SAM

                            [[unlikely]] default:
                                return false;
                        }
                    }
                    else
                        return true; // Do not check TgtID of RXREQ on SAM check disabled
                }
                else
                    return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequester(const Global<config, conn>& glbl) const noexcept
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
    inline bool FiredRequestFlit<config, conn>::IsFromHome(const Global<config, conn>& glbl) const noexcept
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
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        // Requests never come from Subordnates.
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFrom(const Global<config, conn>& glbl, XactScopeEnum from) const noexcept
    {
        switch (from->value)
        {
            case XactScope::Requester:
                if (!IsFromRequester(glbl))
                    return false;

            case XactScope::Home:
                if (!IsFromHome(glbl))
                    return false;

            case XactScope::Subordinate:
                if (!IsFromSubordinate(glbl))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTo(const Global<config, conn>& glbl, XactScopeEnum to) const noexcept
    {
        switch (to->value)
        {
            case XactScope::Requester:
                if (!IsToRequester(glbl))
                    return false;
            
            case XactScope::Home:
                if (!IsToHome(glbl))
                    return false;
            
            case XactScope::Subordinate:
                if (!IsToSubordinate(glbl))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromTo(const Global<config, conn>& glbl, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        return IsFrom(glbl, from) && IsTo(glbl, to);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequesterToHome(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequesterToSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHomeToRequester(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHomeToSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinateToRequester(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinateToHome(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXREQ(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsREQ() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXREQ(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsREQ() && IsTo(glbl, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXSNP(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsSNP() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXSNP(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsSNP() && IsTo(glbl, scope);
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
    inline bool FiredResponseFlit<config, conn>::IsToRequester(const Global<config, conn>& glbl) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRX()
                    || IsToRequesterDCT(glbl);

            case XactScope::Home:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.TgtID());

            case XactScope::Subordinate:
                return IsToRequesterDMT(glbl)
#ifdef CHI_ISSUE_B_ENABLE
                    ;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                    || IsToRequesterDWT(glbl);
#endif

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToHome(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsHome(flit.rsp.TgtID()) 
                    || this->IsTXDAT() && glbl.TOPOLOGY->IsHome(flit.dat.TgtID());

            case XactScope::Home:
                return this->IsRX();

            case XactScope::Subordinate:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsHome(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY->IsHome(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
#ifdef CHI_ISSUE_B_ENABLE
                return false;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                return IsToSubordinateDWT(glbl);
#endif

            case XactScope::Home:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsSubordinate(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY->IsSubordinate(flit.dat.TgtID());

            case XactScope::Subordinate:
                return this->IsRX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDCT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.SrcID());

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
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDMT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && glbl.TOPOLOGY->IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDWT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && glbl.TOPOLOGY->IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToSubordinateDWT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && glbl.TOPOLOGY->IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequester(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTX()
                    || IsFromRequesterDCT(glbl);

            case XactScope::Home:
                return this->IsRXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.SrcID());

            case XactScope::Subordinate:
                return IsFromRequesterDWT(glbl);

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHome(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && glbl.TOPOLOGY->IsHome(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY->IsHome(flit.dat.SrcID());

            case XactScope::Home:
                return this->IsTX();

            case XactScope::Subordinate:
                return this->IsRXDAT() && glbl.TOPOLOGY->IsHome(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
#ifdef CHI_ISSUE_B_ENABLE
                return false;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                return IsFromSubordinateDWT(glbl);
#endif

            case XactScope::Home:
                return this->IsRXRSP() && glbl.TOPOLOGY->IsSubordinate(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY->IsSubordinate(flit.dat.SrcID());

            case XactScope::Subordinate:
                return this->IsTX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterDCT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.SrcID());

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
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateDMT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && glbl.TOPOLOGY->IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->isTXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;   
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterDWT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && glbl.TOPOLOGY->IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && glbl.TOPOLOGY->IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif
    
#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateDWT(const Global<config, conn>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && glbl.TOPOLOGY->IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && glbl.TOPOLOGY->IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFrom(const Global<config, conn>& glbl, XactScopeEnum from) const noexcept
    {
        switch (from->value)
        {
            case XactScope::Requester:
                if (!IsFromRequester(glbl))
                    return false;

            case XactScope::Home:
                if (!IsFromHome(glbl))
                    return false;

            case XactScope::Subordinate:
                if (!IsFromSubordinate(glbl))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTo(const Global<config, conn>& glbl, XactScopeEnum to) const noexcept
    {
        switch (to->value)
        {
            case XactScope::Requester:
                if (!IsToRequester(glbl))
                    return false;
            
            case XactScope::Home:
                if (!IsToHome(glbl))
                    return false;
            
            case XactScope::Subordinate:
                if (!IsToSubordinate(glbl))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromTo(const Global<config, conn>& glbl, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        return IsFrom(glbl, from) && IsTo(glbl, to);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterToHome(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterToSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHomeToRequester(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHomeToSubordinate(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateToRequester(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateToHome(const Global<config, conn>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXRSP(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsRSP() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXRSP(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsRSP() && IsTo(glbl, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXDAT(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsDAT() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXDAT(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsDAT() && IsTo(glbl, scope);
    }
}


#endif

//#endif // __CHI__CHI_XACT_FLIT
