#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__SNOOP
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__SNOOP

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionSnoop : public Xaction<config> {
    public:
        XactionSnoop(const Global<config>&             glbl,
                     const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionSnoop<config>>                   CloneAsIs() const noexcept;

    public:
        bool                GotSnpResp() const noexcept;
        bool                GotAnySnpRespData() const noexcept;
        bool                GotAllSnpRespData() const noexcept;

        bool                GotAnyResp() const noexcept;

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


// Implementation of: class XactionSnoop
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionSnoop<config>::XactionSnoop(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::Snoop, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsSNP()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_SNP, this->first);
            return;
        }

        if (
            this->first.flit.snp.Opcode != Opcodes::SNP::SnpMakeInvalid
         && this->first.flit.snp.Opcode != Opcodes::SNP::SnpToInvalid
         && this->first.flit.snp.Opcode != Opcodes::SNP::SnpToShared
         && this->first.flit.snp.Opcode != Opcodes::SNP::SnpToClean
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_SNP_OPCODE, this->first,
                "This Opcode is not type of / supported by Snoop transaction");
            return;
        }

        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionSnoop<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionSnoop<config>> XactionSnoop<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionSnoop<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::GotSnpResp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::UpRSP::SnpResp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::GotAnySnpRespData() const noexcept
    {
        return this->HasDnDAT({ Opcodes::UpDAT::SnpRespData });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::GotAllSnpRespData() const noexcept
    {
        std::bitset<8> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(Sizes::B64);

        std::bitset<8> collectedDataID =
            details::CollectUpDataID(Sizes::B64, this->subsequence,
                [this](size_t i, const FiredResponseFlit<config>& flit) {
                    return this->subsequenceKeys[i].IsAccepted() && flit.flit.updat.Opcode == Opcodes::UpDAT::SnpRespData;
            });

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::GotAnyResp() const noexcept
    {
        return GotSnpResp() || GotAnySnpRespData();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotAllSnpRespData() || GotSnpResp();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionSnoop<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionSnoop<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNRSP, dnrspFlit,
            "Not expecting DnRSP flits for Snoop transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionSnoop<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_UPRSP, uprspFlit);
    
        if (!uprspFlit.IsUpRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPRSP, uprspFlit);

        if (uprspFlit.flit.uprsp.Opcode == Opcodes::UpRSP::SnpResp)
        {
            if (uprspFlit.flit.uprsp.TgtID != this->first.flit.snp.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TGTID_MISMATCHING_SNP, uprspFlit, this->first);

            if (uprspFlit.flit.uprsp.TxnID != this->first.flit.snp.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TXNID_MISMATCHING_SNP, uprspFlit, this->first);

            if (this->HasUpDAT({ Opcodes::UpDAT::SnpRespData }))
                return this->ResponseFlitDenied(XactDenial::DENIED_SNPRESP_AFTER_SNPRESPDATA, uprspFlit, this->GetLastUpDAT({ Opcodes::UpDAT::SnpRespData }));

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_OPCODE, uprspFlit,
            "This UpRSP Opcode is not type of / supported by Snoop transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionSnoop<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting DnDAT flits for Snoop transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionSnoop<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_UPDAT, updatFlit);

        if (!updatFlit.IsUpDAT()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPDAT, updatFlit);

        if (updatFlit.flit.updat.Opcode == Opcodes::UpDAT::SnpRespData)
        {
            if (updatFlit.flit.updat.TgtID != this->first.flit.snp.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_TGTID_MISMATCHING_SNP, updatFlit, this->first);

            if (updatFlit.flit.updat.TxnID != this->first.flit.snp.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_TXNID_MISMATCHING_SNP, updatFlit, this->first);

            if (this->HasUpRSP({ Opcodes::UpRSP::SnpResp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_SNPRESPDATA_AFTER_SNPRESP, updatFlit, this->GetLastUpRSP({ Opcodes::UpRSP::SnpResp }));

            if (!this->NextSNPDataID(updatFlit))
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_DUPLICATED_DATAID, updatFlit);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_OPCODE, updatFlit,
            "This UpDAT Opcode is not type of / supported by Snoop transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__SNOOP
