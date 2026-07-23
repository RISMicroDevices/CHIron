#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_TRANSIENT_READ
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_TRANSIENT_READ

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionCacheableTransientRead : public Xaction<config> {
    public:
        XactionCacheableTransientRead(const Global<config>&             glbl,
                                      const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionCacheableTransientRead<config>>  CloneAsIs() const noexcept;

    public:
        bool                GotCompData(Flits::DnDAT<config>::dataid_t dataID) const noexcept;
        bool                GotAnyCompData() const noexcept;
        bool                GotAllCompData() const noexcept;

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


// Implementation of: class XactionCacheableTransientRead
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionCacheableTransientRead<config>::XactionCacheableTransientRead(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::CacheableTransientRead, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode != Opcodes::REQ::ReadOnce
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Cacheable Transient Read transaction");
            return;
        }

        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionCacheableTransientRead<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionCacheableTransientRead<config>> XactionCacheableTransientRead<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionCacheableTransientRead<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableTransientRead<config>::GotCompData(Flits::DnDAT<config>::dataid_t dataID) const noexcept
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
    inline bool XactionCacheableTransientRead<config>::GotAnyCompData() const noexcept
    {
        return this->HasDnDAT({ Opcodes::DnDAT::CompData });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableTransientRead<config>::GotAllCompData() const noexcept
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
    inline bool XactionCacheableTransientRead<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotAllCompData();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableTransientRead<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableTransientRead<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCacheableTransientRead<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableTransientRead<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNRSP, dnrspFlit,
            "Not expecting DnRSP flits for Cacheable Transient Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableTransientRead<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting UpRSP flits for Cacheable Transient Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableTransientRead<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
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

            if (auto p = this->GetLastDnDAT({ Opcodes::DnDAT::CompData }))
            {
                if (dndatFlit.flit.dndat.Resp != p->flit.dndat.Resp)
                    return this->ResponseFlitDenied(XactDenial::DENIED_COMPDATA_RESP_MISMATCH, dndatFlit, *p);
            }

            if (!this->NextREQDataID(dndatFlit))
                return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_DUPLICATED_DATAID, dndatFlit);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_OPCODE, dndatFlit,
            "This DnDAT Opcode is not expected for Cacheable Transient Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCacheableTransientRead<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, updatFlit,
            "Not expecting UpDAT flits for Cacheable Transient Read transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__CACHEABLE_TRANSIENT_READ
