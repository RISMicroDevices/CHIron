#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__WRITEBACK
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__WRITEBACK

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionWriteBack : public Xaction<config> {
    public:
        XactionWriteBack(const Global<config>&             glbl,
                         const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionWriteBack<config>>               CloneAsIs() const noexcept;

    public:
        bool                GotDBIDResp() const noexcept;
        bool                GotComp() const noexcept;
        bool                GotAnyCompData() const noexcept;
        bool                GotAllCompData() const noexcept;

    public:
        bool                IsResponseComplete(const Global<config>& glbl) const noexcept;
        bool                IsDataComplete(const Global<config>& glbl) const noexcept;

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


// Implementation of: class XactionWriteBack
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionWriteBack<config>::XactionWriteBack(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::WriteBack, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsEVT()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_EVT, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode != Opcodes::EVT::WriteBackFull
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_EVT_OPCODE, this->first,
                "This Opcode is not type of / supported by Write-Back transaction");
            return;
        }

        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionWriteBack<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionWriteBack<config>> XactionWriteBack<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionWriteBack<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::GotDBIDResp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::DBIDResp, Opcodes::DnRSP::CompDBIDResp });        
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::GotComp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::Comp, Opcodes::DnRSP::CompDBIDResp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::GotAnyCompData() const noexcept
    {
        return this->HasDnDAT({ Opcodes::DnDAT::CompData });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::GotAllCompData() const noexcept
    {
        std::bitset<8> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(this->first.flit.req.Size);

        std::bitset<8> collectedDataID =
            details::CollectDnDataID(this->first.flit.req.Size, this->subsequence,
                [this](size_t i, const FiredResponseFlit<config>& flit) noexcept -> bool {
                    return this->subsequenceKeys[i].IsAccepted() && flit.flit.dndat.Opcode == Opcodes::DnDAT::CompData;
            });

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotDBIDResp() && this->GotComp();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::IsDataComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotAllCompData();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionWriteBack<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionWriteBack<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
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

            if (this->HasDnRSP({ Opcodes::DnRSP::Comp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::Comp }));

            if (this->HasDnRSP({ Opcodes::DnRSP::CompDBIDResp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::CompDBIDResp }));

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }
        else if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::DBIDResp)
        {
            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.evt.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_EVT, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.evt.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_EVT, dnrspFlit, this->first);

            if (this->HasDnRSP({ Opcodes::DnRSP::DBIDResp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::DBIDResp }));

            if (this->HasDnRSP({ Opcodes::DnRSP::CompDBIDResp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::CompDBIDResp }));

            hasDBID = true;
            firstDBID = true;

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }
        else if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::CompDBIDResp)
        {
            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.evt.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_EVT, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.evt.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_EVT, dnrspFlit, this->first);

            if (this->HasDnRSP({ Opcodes::DnRSP::CompDBIDResp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::CompDBIDResp }));

            if (this->HasDnRSP({ Opcodes::DnRSP::Comp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::Comp }));

            if (this->HasDnRSP({ Opcodes::DnRSP::DBIDResp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::DBIDResp }));

            hasDBID = true;
            firstDBID = true;

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_OPCODE, dnrspFlit,
            "This DnRSP Opcode is not expected for Write-Back transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionWriteBack<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting UpRSP flits for Write-Back transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionWriteBack<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting DnDAT flits for Write-Back transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionWriteBack<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_UPDAT, updatFlit);

        if (!updatFlit.IsUpDAT()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPDAT, updatFlit);

        if (updatFlit.flit.updat.Opcode == Opcodes::UpDAT::CopyBackWrData)
        {
            const FiredResponseFlit<config>* optDBIDSource
                = this->GetLastDBIDSourceRSP({
                    Opcodes::DnRSP::DBIDResp,
                    Opcodes::DnRSP::CompDBIDResp });

            if (!optDBIDSource)
                return this->ResponseFlitDenied(XactDenial::DENIED_DATA_BEFORE_DBID, updatFlit);

            if (updatFlit.flit.updat.TgtID() != optDBIDSource->flit.dnrsp.SrcID())
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_TGTID_MISMATCHING_DNRSP, updatFlit, *optDBIDSource);

            if (updatFlit.flit.updat.TxnID() != optDBIDSource->flit.dnrsp.DBID())
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_TXNID_MISMATCHING_DBID, updatFlit, *optDBIDSource);

            if (!this->NextEVTDataID(updatFlit))
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_DUPLICATED_DATAID, updatFlit);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_OPCODE, updatFlit,
            "This UpDAT Opcode is not expected for Write-Back transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__WRITEBACK
