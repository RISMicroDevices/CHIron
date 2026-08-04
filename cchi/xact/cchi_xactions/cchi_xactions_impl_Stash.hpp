#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__STASH
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__STASH

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionStash : public Xaction<config> {
    public:
        XactionStash(const Global<config>&             glbl,
                     const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionStash<config>>                   CloneAsIs() const noexcept;

    public:
        bool                ExpCompStash() const noexcept;
        bool                GotCompStash() const noexcept;

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


// Implementation of: class XactionStash
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionStash<config>::XactionStash(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::Stash, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode != Opcodes::REQ::StashShared
         && this->first.flit.req.Opcode != Opcodes::REQ::StashUnique
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Stash transaction");
            return;
        }
        
        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionStash<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionStash<config>> XactionStash<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionStash<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionStash<config>::ExpCompStash() const noexcept
    {
        return this->first.flit.req.ExpCompStash;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionStash<config>::GotCompStash() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::CompStash });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionStash<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return !ExpCompStash() || GotCompStash();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionStash<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionStash<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionStash<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionStash<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DNRSP, dnrspFlit);

        if (!dnrspFlit.IsDnRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNRSP, dnrspFlit);

        if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::CompStash)
        {
            if (!this->ExpCompStash())
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPSTASH_NOT_EXPECTED, dnrspFlit);

            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.req.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.req.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_REQ, dnrspFlit, this->first);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_OPCODE, dnrspFlit,
            "This DnRSP Opcode is not expected for Stash transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionStash<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting any UpRSP flit for Stash transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionStash<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting any DnDAT flit for Stash transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionStash<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, updatFlit,
            "Not expecting any UpDAT flit for Stash transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__STASH
