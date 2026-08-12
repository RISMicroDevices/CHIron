#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_DATALESS
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_DATALESS

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionCacheableDataless : public Xaction<config> {
    public:
        XactionCacheableDataless(const Global<config>&             glbl,
                                 const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionCacheableDataless<config>>       CloneAsIs() const noexcept;

    public:
        bool                GotComp() const noexcept;
        bool                GotCompAck() const noexcept;

        bool                IsResponseComplete(const Global<config>& glbl) const noexcept;
        bool                IsAckComplete(const Global<config>& glbl) const noexcept;

        virtual bool        IsTxnIDComplete(const Global<config>& glbl) const noexcept override;
        virtual bool        IsDBIDComplete(const Global<config>& glbl) const noexcept override;
        virtual bool        IsComplete(const Global<config>& glbl) const noexcept override;

    protected:
        virtual XactDenialEnum  NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        virtual XactDenialEnum  NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        virtual XactDenialEnum  NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        virtual XactDenialEnum  NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept override;
    };
}


// Implementation of: class XactionCacheableDataless
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionCacheableDataless<config>::XactionCacheableDataless(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::CacheableDataless, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode != Opcodes::REQ::MakeUnique
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Cacheable Dataless transaction");
            return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionCacheableDataless<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionCacheableDataless<config>> XactionCacheableDataless<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionCacheableDataless<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableDataless<config>::GotComp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::Comp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableDataless<config>::GotCompAck() const noexcept
    {
        return this->HasUpRSP({ Opcodes::UpRSP::CompAck });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableDataless<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotComp();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableDataless<config>::IsAckComplete(const Global<config>& glbl) const noexcept
    {
        return GotCompAck();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableDataless<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableDataless<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableDataless<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableDataless<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DNRSP, dnrspFlit);

        if (!dnrspFlit.IsDnRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNRSP, dnrspFlit);

        if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::Comp)
        {
            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.req.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.req.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (this->HasDnRSP({ Opcodes::DnRSP::Comp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMP, dnrspFlit, *this->GetLastDnRSP({ Opcodes::DnRSP::Comp }));

            hasDBID = true;
            firstDBID = true;
         
            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_OPCODE, dnrspFlit,
            "This DnRSP Opcode is not expected for Cacheable Dataless transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableDataless<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_UPRSP, uprspFlit);

        if (!uprspFlit.IsUpRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPRSP, uprspFlit);

        if (uprspFlit.flit.uprsp.Opcode == Opcodes::UpRSP::CompAck)
        {
            if (!this->HasDnRSP({ Opcodes::DnRSP::Comp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPACK_BEFORE_COMP, uprspFlit);

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (!optDBIDSource)
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPACK_BEFORE_DBID, uprspFlit,
                    "No DBID established after Comp, this might be an internal error");

            if (uprspFlit.flit.uprsp.TgtID != optDBIDSource->flit.dnrsp.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TGTID_MISMATCHING_DNRSP, uprspFlit, *optDBIDSource);

            if (uprspFlit.flit.uprsp.TxnID != optDBIDSource->flit.dnrsp.DBID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TXNID_MISMATCHING_DNRSP, uprspFlit, *optDBIDSource);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_OPCODE, uprspFlit,
            "This UpRSP Opcode is not expected for Cacheable Dataless transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableDataless<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting DnDAT flits for Cacheable Dataless transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableDataless<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, updatFlit,
            "Not expecting UpDAT flits for Cacheable Dataless transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_DATALESS
