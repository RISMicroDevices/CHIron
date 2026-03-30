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

        template<FlitConfigurationConcept config>
        class FiredFlit {
        public:
            XactScopeEnum   scope;
            uint64_t        time;
            bool            isTX;

        public:
            Companion       companion;

        public:
            FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept;

        public:
            bool            IsTX() const noexcept;
            bool            IsRX() const noexcept;

        public:
            virtual ChannelTypeEnum GetChannel() const noexcept = 0;
        };

        template<FlitConfigurationConcept config>
        class FiredRequestFlit : public FiredFlit<config> {
        public:
            bool            isREQ;
            union {
                Flits::REQ<config>  req;
                Flits::SNP<config>  snp;
            }               flit;
            
            // REQ: Request source Node ID
            // SNP: Snoop target Node ID
            Flits::REQ<config>::srcid_t
                            nodeId;

        public:
            FiredRequestFlit(
                XactScopeEnum                       scope,
                bool                                isTX,
                uint64_t                            time,
                const Flits::REQ<config>&           reqFlit
            ) noexcept;
            
            FiredRequestFlit(
                XactScopeEnum                       scope,
                bool                                isTX,
                uint64_t                            time,
                const Flits::SNP<config>&           snpFlit,
                Flits::REQ<config>::srcid_t         snpTgtId
            ) noexcept;

        public:
            bool            IsREQ() const noexcept;
            bool            IsSNP() const noexcept;

            bool            IsTXREQ() const noexcept;
            bool            IsRXREQ() const noexcept;
            bool            IsTXSNP() const noexcept;
            bool            IsRXSNP() const noexcept;

        public:
            virtual ChannelTypeEnum GetChannel() const noexcept override;
            ChannelTypeEnum         GetChannelType() const noexcept;

        public:
            bool            IsToRequester(const Global<config>&) const noexcept;
            bool            IsToHome(const Global<config>&) const noexcept;
            bool            IsToSubordinate(const Global<config>&) const noexcept;

        public:
            bool            IsFromRequester(const Global<config>&) const noexcept;
            bool            IsFromHome(const Global<config>&) const noexcept;
            bool            IsFromSubordinate(const Global<config>&) const noexcept;

        public:
            bool            IsFrom(const Global<config>&, XactScopeEnum from) const noexcept;
            bool            IsTo(const Global<config>&, XactScopeEnum to) const noexcept;
            bool            IsFromTo(const Global<config>&, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Global<config>&) const noexcept;
            bool            IsFromRequesterToSubordinate(const Global<config>&) const noexcept;
            bool            IsFromHomeToRequester(const Global<config>&) const noexcept;
            bool            IsFromHomeToSubordinate(const Global<config>&) const noexcept;
            bool            IsFromSubordinateToRequester(const Global<config>&) const noexcept;
            bool            IsFromSubordinateToHome(const Global<config>&) const noexcept;

        public:
            bool            IsTXREQ(const Global<config>&, XactScopeEnum scope) const noexcept;
            bool            IsRXREQ(const Global<config>&, XactScopeEnum scope) const noexcept;
            bool            IsTXSNP(const Global<config>&, XactScopeEnum scope) const noexcept;
            bool            IsRXSNP(const Global<config>&, XactScopeEnum scope) const noexcept;
        };

        template<FlitConfigurationConcept config>
        class FiredResponseFlit : public FiredFlit<config> {
        public:
            bool            isRSP;
            union {
                Flits::RSP<config>  rsp;
                Flits::DAT<config>  dat;
            }               flit;

        public:
            FiredResponseFlit() noexcept;
            FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::RSP<config>& rspFlit) noexcept;
            FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DAT<config>& datFlit) noexcept;
        
        public:
            bool            IsRSP() const noexcept;
            bool            IsDAT() const noexcept;

            bool            IsTXRSP() const noexcept;
            bool            IsRXRSP() const noexcept;
            bool            IsTXDAT() const noexcept;
            bool            IsRXDAT() const noexcept;

        public:
            virtual ChannelTypeEnum GetChannel() const noexcept override;
            ChannelTypeEnum         GetChannelType() const noexcept;

        public:
            bool            IsToRequester(const Global<config>&) const noexcept;
            bool            IsToHome(const Global<config>&) const noexcept;
            bool            IsToSubordinate(const Global<config>&) const noexcept;

            bool            IsToRequesterDCT(const Global<config>&) const noexcept;
            bool            IsToRequesterDMT(const Global<config>&) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsToRequesterDWT(const Global<config>&) const noexcept;
            bool            IsToSubordinateDWT(const Global<config>&) const noexcept;
#endif

        public:
            bool            IsFromRequester(const Global<config>&) const noexcept;
            bool            IsFromHome(const Global<config>&) const noexcept;
            bool            IsFromSubordinate(const Global<config>&) const noexcept;

            bool            IsFromRequesterDCT(const Global<config>&) const noexcept;
            bool            IsFromSubordinateDMT(const Global<config>&) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsFromRequesterDWT(const Global<config>&) const noexcept;
            bool            IsFromSubordinateDWT(const Global<config>&) const noexcept;
#endif

        public:
            bool            IsFrom(const Global<config>&, XactScopeEnum from) const noexcept;
            bool            IsTo(const Global<config>&, XactScopeEnum to) const noexcept;
            bool            IsFromTo(const Global<config>&, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Global<config>&) const noexcept;
            bool            IsFromRequesterToSubordinate(const Global<config>&) const noexcept;
            bool            IsFromHomeToRequester(const Global<config>&) const noexcept;
            bool            IsFromHomeToSubordinate(const Global<config>&) const noexcept;
            bool            IsFromSubordinateToRequester(const Global<config>&) const noexcept;
            bool            IsFromSubordinateToHome(const Global<config>&) const noexcept;

        public:
            bool            IsTXRSP(const Global<config>&, XactScopeEnum scope) const noexcept;
            bool            IsRXRSP(const Global<config>&, XactScopeEnum scope) const noexcept;
            bool            IsTXDAT(const Global<config>&, XactScopeEnum scope) const noexcept;
            bool            IsRXDAT(const Global<config>&, XactScopeEnum scope) const noexcept;
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

    template<FlitConfigurationConcept config>
    inline FiredFlit<config>::FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept
        : scope     (scope)
        , time      (time)
        , isTX      (isTX)
    { }

    template<FlitConfigurationConcept config>
    inline bool FiredFlit<config>::IsTX() const noexcept
    {
        return isTX;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredFlit<config>::IsRX() const noexcept
    {
        return !isTX;
    }
}


// Implementation of: class FiredRequestFlit
namespace /*CHI::*/Xact {
    /*
    bool        isREQ;
    union {
        Flits::REQ<config>    req;
        Flits::SNP<config>    snp;
    }           flit;
    */

    template<FlitConfigurationConcept config>
    inline FiredRequestFlit<config>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::REQ<config>& reqFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isREQ             (true)
        , nodeId            (reqFlit.SrcID())
    { 
        flit.req = reqFlit;
    }

    template<FlitConfigurationConcept config>
    inline FiredRequestFlit<config>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::SNP<config>& snpFlit, Flits::REQ<config>::srcid_t snpTgtId) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isREQ             (false)
        , nodeId            (snpTgtId)
    {
        flit.snp = snpFlit;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsREQ() const noexcept
    {
        return isREQ;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsSNP() const noexcept
    {
        return !isREQ;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTXREQ() const noexcept
    {
        return this->IsTX() && IsREQ();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsRXREQ() const noexcept
    {
        return this->IsRX() && IsREQ();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTXSNP() const noexcept
    {
        return this->IsTX() && IsSNP();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsRXSNP() const noexcept
    {
        return this->IsRX() && IsSNP();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredRequestFlit<config>::GetChannel() const noexcept
    {
        return GetChannelType();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredRequestFlit<config>::GetChannelType() const noexcept
    {
        return IsREQ() ? ChannelType::REQ : ChannelType::SNP;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsToRequester(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsToHome(const Global<config>& glbl) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:

                if (this->IsTXREQ())
                {
                    if (glbl.SAM_SCOPE.enable)
                    {
                        switch (glbl.SAM_SCOPE.Get(flit.req.SrcID()).value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY.IsHome(flit.req.TgtID());

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
                    if (glbl.SAM_SCOPE.enable)
                    {
                        switch (glbl.SAM_SCOPE.Get(flit.req.SrcID())->value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY.IsHome(flit.req.TgtID());

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

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsToSubordinate(const Global<config>& glbl) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester: // For PrefetchTgt only in standard AMBA CHI

                if (this->IsTXREQ())
                {
                    if (glbl.SAM_SCOPE.enable)
                    {
                        case SAMScope::AfterSAM:
                            return glbl.TOPOLOGY.IsSubordinate(flit.req.TgtID());

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
                    if (glbl.SAM_SCOPE.enable)
                    {
                        switch (glbl.SAM_SCOPE.Get(flit.req.SrcID())->value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY.IsSubordinate(flit.req.TgtID());

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
                    if (glbl.SAM_SCOPE.enable)
                    {
                        switch (glbl.SAM_SCOPE.Get(flit.req.SrcID())->value)
                        {
                            case SAMScope::AfterSAM:
                                return glbl.TOPOLOGY.IsSubordinate(flit.req.TgtID());

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

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromRequester(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromHome(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromSubordinate(const Global<config>& glbl) const noexcept
    {
        // Requests never come from Subordnates.
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFrom(const Global<config>& glbl, XactScopeEnum from) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTo(const Global<config>& glbl, XactScopeEnum to) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromTo(const Global<config>& glbl, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        return IsFrom(glbl, from) && IsTo(glbl, to);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromRequesterToHome(const Global<config>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromRequesterToSubordinate(const Global<config>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromHomeToRequester(const Global<config>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromHomeToSubordinate(const Global<config>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromSubordinateToRequester(const Global<config>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsFromSubordinateToHome(const Global<config>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTXREQ(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsREQ() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsRXREQ(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsREQ() && IsTo(glbl, scope);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTXSNP(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsSNP() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsRXSNP(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsSNP() && IsTo(glbl, scope);
    }
}

// Implementation of: class FiredResponseFlit
namespace /*CHI::*/Xact {
    /*
    bool        isRSP;
    union {
        Flits::RSP<config>    rsp;
        Flits::DAT<config>    dat;
    }           flit;
    */

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit() noexcept
        : FiredFlit<config> (XactScope::Requester, false, 0)
        , isRSP             (false)
    { }

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::RSP<config>& rspFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isRSP             (true)
    {
        flit.rsp = rspFlit;
    }

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DAT<config>& datFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isRSP             (false)
    {
        flit.dat = datFlit;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRSP() const noexcept
    {
        return isRSP;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsDAT() const noexcept
    {
        return !isRSP;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXRSP() const noexcept
    {
        return this->IsTX() && IsRSP();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXRSP() const noexcept
    {
        return this->IsRX() && IsRSP();
    }
    
    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXDAT() const noexcept
    {
        return this->IsTX() && IsDAT();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXDAT() const noexcept
    {
        return this->IsRX() && IsDAT();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredResponseFlit<config>::GetChannel() const noexcept
    {
        return GetChannelType();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredResponseFlit<config>::GetChannelType() const noexcept
    {
        return IsRSP() ? ChannelType::RSP : ChannelType::DAT;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsToRequester(const Global<config>& glbl) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRX()
                    || IsToRequesterDCT(glbl);

            case XactScope::Home:
                return this->IsTXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.TgtID());

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

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsToHome(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && glbl.TOPOLOGY.IsHome(flit.rsp.TgtID()) 
                    || this->IsTXDAT() && glbl.TOPOLOGY.IsHome(flit.dat.TgtID());

            case XactScope::Home:
                return this->IsRX();

            case XactScope::Subordinate:
                return this->IsTXRSP() && glbl.TOPOLOGY.IsHome(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY.IsHome(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsToSubordinate(const Global<config>& glbl) const noexcept
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
                return this->IsTXRSP() && glbl.TOPOLOGY.IsSubordinate(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY.IsSubordinate(flit.dat.TgtID());

            case XactScope::Subordinate:
                return this->IsRX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsToRequesterDCT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsToRequesterDMT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && glbl.TOPOLOGY.IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsToRequesterDWT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && glbl.TOPOLOGY.IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsToSubordinateDWT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && glbl.TOPOLOGY.IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromRequester(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTX()
                    || IsFromRequesterDCT(glbl);

            case XactScope::Home:
                return this->IsRXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.SrcID());

            case XactScope::Subordinate:
                return IsFromRequesterDWT(glbl);

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromHome(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && glbl.TOPOLOGY.IsHome(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY.IsHome(flit.dat.SrcID());

            case XactScope::Home:
                return this->IsTX();

            case XactScope::Subordinate:
                return this->IsRXDAT() && glbl.TOPOLOGY.IsHome(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromSubordinate(const Global<config>& glbl) const noexcept
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
                return this->IsRXRSP() && glbl.TOPOLOGY.IsSubordinate(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY.IsSubordinate(flit.dat.SrcID());

            case XactScope::Subordinate:
                return this->IsTX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromRequesterDCT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromSubordinateDMT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && glbl.TOPOLOGY.IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->isTXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;   
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromRequesterDWT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && glbl.TOPOLOGY.IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && glbl.TOPOLOGY.IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif
    
#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromSubordinateDWT(const Global<config>& glbl) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && glbl.TOPOLOGY.IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && glbl.TOPOLOGY.IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFrom(const Global<config>& glbl, XactScopeEnum from) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTo(const Global<config>& glbl, XactScopeEnum to) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromTo(const Global<config>& glbl, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        return IsFrom(glbl, from) && IsTo(glbl, to);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromRequesterToHome(const Global<config>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromRequesterToSubordinate(const Global<config>& glbl) const noexcept
    {
        return IsFromRequester(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromHomeToRequester(const Global<config>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromHomeToSubordinate(const Global<config>& glbl) const noexcept
    {
        return IsFromHome(glbl) && IsToSubordinate(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromSubordinateToRequester(const Global<config>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToRequester(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsFromSubordinateToHome(const Global<config>& glbl) const noexcept
    {
        return IsFromSubordinate(glbl) && IsToHome(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXRSP(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsRSP() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXRSP(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsRSP() && IsTo(glbl, scope);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXDAT(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsDAT() && IsFrom(glbl, scope);
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXDAT(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return IsDAT() && IsTo(glbl, scope);
    }
}


#endif

//#endif // __CHI__CHI_XACT_FLIT
