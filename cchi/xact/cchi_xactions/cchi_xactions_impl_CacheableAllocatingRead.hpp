#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_ALLOCATING_READ
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_ALLOCATING_READ

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionCacheableAllocatingRead : public Xaction<config> {
    public:
        XactionCacheableAllocatingRead(const Global<config>&            glbl,
                                       const FiredRequestFlit<config>&  first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionCacheableAllocatingRead<config>> CloneAsIs() const noexcept;

    public:
        bool                GotCompData(Flits::DnDAT<config>::dataid_t dataID) const noexcept;
        bool                GotAnyCompData() const noexcept;
        bool                GotAllCompData() const noexcept;
        bool                GotComp() const noexcept;
        bool                GotCompAck() const noexcept;

    public:
        bool                IsAckComplete(const Global<config>& glbl) const noexcept;
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


// Implementation of: class XactionCacheableAllocatingRead
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionCacheableAllocatingRead<config>::XactionCacheableAllocatingRead(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::CacheableAllocatingRead, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::ReadShared
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadUnique
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Cacheable Allocating Read transaction");
            return;
        }

        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionCacheableAllocatingRead<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionCacheableAllocatingRead<config>> XactionCacheableAllocatingRead<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionCacheableAllocatingRead<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::GotCompData(Flits::DnDAT<config>::dataid_t dataID) const noexcept
    {
        for (auto keyIt = this->subsequenceKeys.begin(), flitIt = this->subsequence.begin(); 
             keyIt != this->subsequenceKeys.end(); ++keyIt, ++flitIt)
        {
            if (keyIt->IsAccepted() 
             && flitIt->flit.dndat.Opcode == Opcodes::DnDAT::CompData
             && flitIt->flit.dndat.DataID == dataID)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::GotAnyCompData() const noexcept
    {
        return this->HasDnDAT({ Opcodes::DnDAT::CompData });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::GotAllCompData() const noexcept
    {
        std::bitset<8> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(this->first.flit.req.Size);

        std::bitset<8> collectedDataID =
            details::CollectDnDataID(this->first.flit.req.Size, this->subsequence, 
                [this](size_t i, const FiredResponseFlit<config>& flit) {
                    return this->subsequenceKeys[i].IsAccepted() && flit.flit.dndat.Opcode == Opcodes::DnDAT::CompData;
            });

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::GotComp() const noexcept
    {
        return this->HasDnRSP({ Opcodes::DnRSP::Comp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::GotCompAck() const noexcept
    {
        return this->HasUpRSP({ Opcodes::UpRSP::CompAck });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotComp() || GotAllCompData();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::IsAckComplete(const Global<config>& glbl) const noexcept
    {
        return GotCompAck();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableAllocatingRead<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableAllocatingRead<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DNRSP, dnrspFlit);

        if (!dnrspFlit.IsDnRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNRSP, dnrspFlit);

        if (dnrspFlit.flit.dnrsp.Opcode == Opcodes::DnRSP::Comp)
        {
            if (this->first.flit.req.Opcode != Opcodes::REQ::ReadUnique)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_OPCODE, dnrspFlit,
                    "Comp is only expected for ReadUnique");

            if (dnrspFlit.flit.dnrsp.TgtID != this->first.flit.req.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TGTID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (dnrspFlit.flit.dnrsp.TxnID != this->first.flit.req.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_TXNID_MISMATCHING_REQ, dnrspFlit, this->first);

            if (this->HasDnRSP({ Opcodes::DnRSP::Comp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMP, dnrspFlit, this->GetLastDnRSP({ Opcodes::DnRSP::Comp }));

            if (this->HasDnDAT({ Opcodes::DnDAT::CompData }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_AFTER_COMPDATA, dnrspFlit, this->GetLastDnDAT({ Opcodes::DnDAT::CompData }));

            if (this->first.flit.req.ExpCompData)
                return this->ResponseFlitDenied(XactDenial::DENIED_COMP_ON_EXPCOMPDATA, dnrspFlit, this->first);

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();
            if (!optDBIDSource)
            {
                if (optDBIDSource->IsDnRSP() && dnrspFlit.flit.dnrsp.DBID != optDBIDSource->flit.dnrsp.DBID
                 || optDBIDSource->IsDnDAT() && dnrspFlit.flit.dnrsp.DBID != optDBIDSource->flit.dndat.DBID)
                    return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_DBID_MISMATCH, dnrspFlit, *optDBIDSource);
            }
            else
                firstDBID = true;

            hasDBID = true;

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNRSP_OPCODE, dnrspFlit,
            "This DnRSP Opcode is not expected for Cacheable Allocating Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableAllocatingRead<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_UPRSP, uprspFlit);

        if (!uprspFlit.IsUpRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPRSP, uprspFlit);

        if (uprspFlit.flit.uprsp.Opcode == Opcodes::UpRSP::CompAck)
        {
            if (this->first.flit.req.Opcode == Opcodes::REQ::ReadUnique)
            {
                if (!this->HasDnDAT({ Opcodes::DnDAT::CompData }) && !this->HasDnRSP({ Opcodes::DnRSP::Comp }))
                    return this->ResponseFlitDenied(XactDenial::DENIED_COMPACK_BEFORE_COMPDATA_OR_COMP, uprspFlit);
            }
            else
            {
                if (!this->HasDnDAT({ Opcodes::DnDAT::CompData }))
                    return this->ResponseFlitDenied(XactDenial::DENIED_COMPACK_BEFORE_COMPDATA, uprspFlit);
            }

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (!optDBIDSource)
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPACK_BEFORE_DBID, uprspFlit,
                    "No DBID established after CompData/Comp, this might be an internal error");

            if (optDBIDSource->IsDnDAT())
            {
                if (uprspFlit.flit.uprsp.TgtID != optDBIDSource->flit.dndat.SrcID)
                    return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TGTID_MISMATCHING_DNDAT, uprspFlit, *optDBIDSource);

                if (uprspFlit.flit.uprsp.TxnID != optDBIDSource->flit.dndat.DBID)
                    return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TXNID_MISMATCHING_DNDAT, uprspFlit, *optDBIDSource);
            }
            else // DnRSP
            {
                if (uprspFlit.flit.uprsp.TgtID != optDBIDSource->flit.dnrsp.SrcID)
                    return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TGTID_MISMATCHING_DNRSP, uprspFlit, *optDBIDSource);

                if (uprspFlit.flit.uprsp.TxnID != optDBIDSource->flit.dnrsp.DBID)
                    return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_TXNID_MISMATCHING_DNRSP, uprspFlit, *optDBIDSource);
            }

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_UPRSP_OPCODE, uprspFlit,
            "This UpRSP Opcode is not expected for Cacheable Allocating Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableAllocatingRead<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DNDAT, dndatFlit);

        if (!dndatFlit.IsDnDAT())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNDAT, dndatFlit);

        if (dndatFlit.flit.dndat.Opcode == Opcodes::DnDAT::CompData)
        {
            if (dndatFlit.flit.dndat.TgtID != this->first.flit.req.SrcID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_TGTID_MISMATCHING_REQ, dndatFlit, this->first);

            if (dndatFlit.flit.dndat.TxnID != this->first.flit.req.TxnID)
                return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_TXNID_MISMATCHING_REQ, dndatFlit, this->first);

            if (this->HasDnRSP({ Opcodes::DnRSP::Comp }))
                return this->ResponseFlitDenied(XactDenial::DENIED_COMPDATA_AFTER_COMP, dndatFlit, this->GetLastDnRSP({ Opcodes::DnRSP::Comp }));

            if (auto p = this->GetLastDnDAT({ Opcodes::DnDAT::CompData }))
            {
                if (dndatFlit.flit.dndat.Resp != p->flit.dndat.Resp)
                    return this->ResponseFlitDenied(XactDenial::DENIED_COMPDATA_RESP_MISMATCH, dndatFlit, *p);
            }

            if (!this->NextREQDataID(dndatFlit))
                return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_DUPLICATED_DATAID, dndatFlit);

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();
            if (!optDBIDSource)
            {
                if (optDBIDSource->IsDnRSP() && dndatFlit.flit.dndat.DBID != optDBIDSource->flit.dnrsp.DBID
                 || optDBIDSource->IsDnDAT() && dndatFlit.flit.dndat.DBID != optDBIDSource->flit.dndat.DBID)
                    return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_DBID_MISMATCH, dndatFlit, *optDBIDSource);
            }
            else
                firstDBID = true;

            hasDBID = true;

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_OPCODE, dndatFlit,
            "This DnDAT Opcode is not expected for Cacheable Allocating Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableAllocatingRead<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, datFlit,
            "Not expecting any UpDAT flit for Cacheable Allocating Read transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_ALLOCATING_READ
