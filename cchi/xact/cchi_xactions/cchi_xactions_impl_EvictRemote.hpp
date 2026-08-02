#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS_IMPL__EVICT_REMOTE
#define __CCHI__CCHI_XACT_XACTIONS_IMPL__EVICT_REMOTE

#include "../../spec/cchi_protocol_encoding.hpp"

#include "cchi_xactions_base.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class XactionEvictRemote : public Xaction<config> {
    public:
        XactionEvictRemote(const Global<config>&             glbl,
                           const FiredRequestFlit<config>&   first) noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>                Clone() const noexcept override;
        std::shared_ptr<XactionEvictRemote<config>>             CloneAsIs() const noexcept;

    public:
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


// Implementation of: class XactionEvictRemote
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionEvictRemote<config>::XactionEvictRemote(
        const Global<config>&               glbl,
        const FiredRequestFlit<config>&     first) noexcept
        : Xaction<config>(XactionType::EvictRemote, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;
    
        if (!this->first.IsRXREQ()) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode != Opcodes::REQ::EvictBack
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "Not expecting Evict flits for Evict Remote transactions");
            return;
        }

        // TODO: Field Mapping Check
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionEvictRemote<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionEvictRemote<config>> XactionEvictRemote<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionEvictRemote<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvictRemote<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvictRemote<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionEvictRemote<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvictRemote<config>::NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNRSP, dnrspFlit,
            "Not expecting DnRSP flits for Evict Remote transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvictRemote<config>::NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPRSP, uprspFlit,
            "Not expecting UpRSP flits for Evict Remote transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvictRemote<config>::NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DNDAT, dndatFlit,
            "Not expecting DnDAT flits for Evict Remote transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionEvictRemote<config>::NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_UPDAT, updatFlit,
            "Not expecting UpDAT flits for Evict Remote transactions");
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS_IMPL__EVICT_REMOTE
