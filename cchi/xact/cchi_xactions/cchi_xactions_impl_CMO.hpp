#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__CMO
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__CMO

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionCMO : public Xaction<config> {
    public:
        XactionCMO(const Global<config>&             glbl,
                   const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionCMO<config>>                     CloneAsIs() const noexcept;

    public:
        bool                GotCompCMO() const noexcept;

        bool                IsResponseComplete(const Global<config>& glbl) const noexcept;

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


// Implementation of: class XactionCMO
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionCMO<config>::XactionCMO(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::CMO, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode != Opcodes::REQ::CleanShared
         && this->first.flit.req.Opcode != Opcodes::REQ::CleanInvalid
         && this->first.flit.req.Opcode != Opcodes::REQ::MakeInvalid
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by CMO transaction");
            return;
        }

        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionCMO<config>> XactionCMO<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionCMO<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionCMO<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCMO<config>::GotCompCMO() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::CompCMO });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCMO<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotCompCMO();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCMO<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCMO<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCMO<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCMO<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DNRSP, dnrspFlit);

        if (!dnrspFlit.IsDnRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNRSP, dnrspFlit);

        if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::CompCMO)
        {
            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.req.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.req.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_REQ, dnrspFlit, this->first);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_OPCODE, dnrspFlit,
            "This DnRSP Opcode is not expected for CMO transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCMO<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting UpRSP flits for CMO transactions");   
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCMO<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting DnDAT flits for CMO transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCMO<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, updatFlit,
            "Not expecting UpDAT flits for CMO transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__CMO
