#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__EVICT
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__EVICT

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionEvict : public Xaction<config> {
    public:
        XactionEvict(const Global<config>&             glbl,
                     const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionEvict<config>>                   CloneAsIs() const noexcept;

    public:
        bool                GotComp() const noexcept;

    public:
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


// Implementation of: class XactionEvict
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionEvict<config>::XactionEvict(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::Evict, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsEVT()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_EVT, this->first);
            return;
        }

        if (
            this->first.flit.evt.Opcode != Opcodes::EVT::Evict
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_EVT_OPCODE, this->first,
                "This Opcode is not type of / supported by Evict transaction");
            return;
        }

        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionEvict<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionEvict<config>> XactionEvict<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionEvict<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvict<config>::GotComp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::Comp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvict<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotComp();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvict<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvict<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvict<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvict<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DNRSP, dnrspFlit);

        if (!dnrspFlit.IsDnRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNRSP, dnrspFlit);

        if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::Comp)
        {
            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.evt.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_EVT, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.evt.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_EVT, dnrspFlit, this->first);

            if (this->HasDnRSP({ Opcodes::DnRSP::Comp })) [[unlikely]]
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::Comp }));

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_OPCODE, dnrspFlit,
            "This DnRSP Opcode is not expected for Evict transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvict<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting UpRSP flits for Evict transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvict<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting DnDAT flits for Evict transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvict<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, updatFlit,
            "Not expecting UpDAT flits for Evict transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__EVICT
