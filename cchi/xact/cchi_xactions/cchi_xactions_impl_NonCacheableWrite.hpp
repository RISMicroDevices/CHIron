#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__NON_CACHEABLE_WRITE
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__NON_CACHEABLE_WRITE

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionNonCacheableWrite : public Xaction<config> {
    public:
        XactionNonCacheableWrite(const Global<config>&             glbl,
                                 const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionNonCacheableWrite<config>>       CloneAsIs() const noexcept;

    public:
        bool                GotDBIDResp() const noexcept;
        bool                GotComp() const noexcept;
        bool                GotAnyNonCopyBackWrData() const noexcept;
        bool                GotAllNonCopyBackWrData() const noexcept;

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


// Implementation of: class XactionNonCacheableWrite
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionNonCacheableWrite<config>::XactionNonCacheableWrite(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::NonCacheableWrite, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpPtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpFull
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Non-Cacheable Write transaction");
            return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionNonCacheableWrite<config>::Clone() const noexcept
    {
        return std::make_shared<XactionNonCacheableWrite<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionNonCacheableWrite<config>> XactionNonCacheableWrite<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionNonCacheableWrite<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::GotDBIDResp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::DBIDResp, Opcodes::DnRSP::CompDBIDResp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::GotComp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::Comp, Opcodes::DnRSP::CompDBIDResp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::GotAnyNonCopyBackWrData() const noexcept
    {
        return this->HasDnDAT({ Opcodes::UpDAT::NonCopyBackWrData});
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::GotAllNonCopyBackWrData() const noexcept
    {
        std::bitset<8> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(this->first.flit.req.Size);

        std::bitset<8> collectedDataID =
            details::CollectUpDataID(this->first.flit.req.Size, this->subsequence,
                [this](size_t i, const FiredResponseFlit<config>& flit) {
                    return this->subsequenceKeys[i].IsAccepted() && flit.flit.updat.Opcode == Opcodes::UpDAT::NonCopyBackWrData;
            });

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotDBIDResp() && GotComp();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::IsDataComplete(const Global<config>& glbl) const noexcept
    {
        return GotAllNonCopyBackWrData();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableWrite<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableWrite<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
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
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::Comp }));

            if (this->HasDnRSP({ Opcodes::DnRSP::CompDBIDResp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::CompDBIDResp }));

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }
        else if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::DBIDResp)
        {
            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.req.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.req.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_REQ, dnrspFlit, this->first);

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
            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.req.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.req.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_REQ, dnrspFlit, this->first);

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
            "This DnRSP Opcode is not type of / supported by Non-Cacheable Write transaction");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableWrite<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting UpRSP flits for Non-Cacheable Write transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableWrite<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting DnDAT flits for Non-Cacheable Write transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableWrite<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_UPDAT, updatFlit);

        if (!updatFlit.IsUpDAT()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPDAT, updatFlit);

        if (updatFlit.flit.updat.Opcode == Opcodes::UpDAT::NonCopyBackWrData)
        {
            const FiredResponseFlit<config>* optDBIDSource 
                = this->GetLastDBIDSourceRSP({
                    Opcodes::DnRSP::DBIDResp,
                    Opcodes::DnRSP::CompDBIDResp });

            if (!optDBIDSource)
                return this->ResponseFlitDenied(XactDenial::DENIED_DATA_BEFORE_DBID, updatFlit);

            if (updatFlit.flit.updat.TgtID != optDBIDSource->flit.dnrsp.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_TGTID_MISMATCHING_DNRSP, updatFlit, *optDBIDSource);

            if (updatFlit.flit.updat.TxnID != optDBIDSource->flit.dnrsp.DBID)
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_TXNID_MISMATCHING_DBID, updatFlit, *optDBIDSource);

            if (!this->NextREQDataID(updatFlit))
                return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_DUPLICATED_DATAID, updatFlit);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_UPDAT_OPCODE, updatFlit,
            "This UpDAT Opcode is not type of / supported by Non-Cacheable Write transaction");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__NON_CACHEABLE_WRITE
