#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__NON_CACHEABLE_READ
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__NON_CACHEABLE_READ

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionNonCacheableRead : public Xaction<config> {
    public:
        XactionNonCacheableRead(const Global<config>&             glbl,
                                const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>            Clone() const noexcept override;
        std::shared_ptr<XactionNonCacheableRead<config>>    CloneAsIs() const noexcept;
    
    public:
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


// Implementation of: class XactionNonCacheableRead
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionNonCacheableRead<config>::XactionNonCacheableRead(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::NonCacheableRead, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::ReadNoSnp
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Non-Cacheable Read transaction");
            return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionNonCacheableRead<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionNonCacheableRead<config>> XactionNonCacheableRead<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionNonCacheableRead<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableRead<config>::GotAnyCompData() const noexcept
    {
        return this->HasDnDAT({ Opcodes::DnDAT::CompData });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableRead<config>::GotAllCompData() const noexcept
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
    inline bool XactionNonCacheableRead<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return GotAllCompData();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableRead<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableRead<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonCacheableRead<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableRead<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNRSP, dnrspFlit,
            "Not expecting DnRSP flits for Non-Cacheable Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableRead<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting UpRSP flits for Non-Cacheable Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableRead<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
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

            if (!this->NextREQDataID(dndatFlit))
                return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_DUPLICATED_DATAID, dndatFlit);

            // TODO: Field Mapping Check

            return XactDenial::ACCEPTED;
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DNDAT_OPCODE, dndatFlit,
            "This DnDAT Opcode is not expected for Non-Cacheable Read transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonCacheableRead<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, updatFlit,
            "Not expecting UpDAT flits for Non-Cacheable Read transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__NON_CACHEABLE_READ
