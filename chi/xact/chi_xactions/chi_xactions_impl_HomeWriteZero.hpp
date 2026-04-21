//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEWRITEZERO)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEWRITEZERO))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEWRITEZERO
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEWRITEZERO
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionHomeWriteZero : public Xaction<config> {
        public:
            XactionHomeWriteZero(const Global<config>&            glbl,
                                 const FiredRequestFlit<config>&  first,
                                 std::shared_ptr<Xaction<config>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeWriteZero<config>> CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config>& glbl) const noexcept;

            virtual bool                    IsTxnIDComplete(const Global<config>& glbl) const noexcept override;
            virtual bool                    IsDBIDComplete(const Global<config>& glbl) const noexcept override;
            virtual bool                    IsComplete(const Global<config>& glbl) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Global<config>& glbl) const noexcept override;

        protected:
            virtual const FiredResponseFlit<config>*  
                                            GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept override;
            virtual std::optional<typename Flits::REQ<config>::tgtid_t>
                                            GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept override;

        public:
            virtual bool                    IsDMTPossible() const noexcept override;
            virtual bool                    IsDCTPossible() const noexcept override;
            virtual bool                    IsDWTPossible() const noexcept override;

            virtual const FiredResponseFlit<config>*
                                            GetDMTSrcIDSource(const Global<config>& glbl) const noexcept override;
            virtual const FiredResponseFlit<config>*
                                            GetDMTTgtIDSource(const Global<config>& glbl) const noexcept override;

            virtual const FiredResponseFlit<config>*
                                            GetDCTSrcIDSource(const Global<config>& glbl) const noexcept override;
            virtual const FiredResponseFlit<config>*
                                            GetDCTTgtIDSource(const Global<config>& glbl) const noexcept override;

            virtual const FiredResponseFlit<config>*
                                            GetDWTSrcIDSource(const Global<config>& glbl) const noexcept override;
            virtual const FiredResponseFlit<config>*
                                            GetDWTTgtIDSource(const Global<config>& glbl) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class XactionHomeWriteZero
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionHomeWriteZero<config>::XactionHomeWriteZero(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried
    ) noexcept
        : Xaction<config>(XactionType::HomeWriteZero, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpZero
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_REQ_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(glbl))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
            return;
        }

        //
        if (glbl.CHECK_FIELD_MAPPING.enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionHomeWriteZero<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionHomeWriteZero<config>> XactionHomeWriteZero<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeWriteZero<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWriteZero<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetLastRSP(
            { Opcodes::RSP::DBIDResp, Opcodes::RSP::Comp, Opcodes::RSP::CompDBIDResp });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionHomeWriteZero<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWriteZero<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWriteZero<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWriteZero<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWriteZero<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWriteZero<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWriteZero<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        bool gotDBID = false;
        bool gotComp = false;

        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
            {
                return true;
            }
            else if (iter->opcode.rsp == Opcodes::RSP::DBIDResp)
            {
                gotDBID = true;

                if (gotComp)
                    return true;
            }
            else if (iter->opcode.rsp == Opcodes::RSP::Comp)
            {
                gotComp = true;

                if (gotDBID)
                    return true;
            }
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWriteZero<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeWriteZero<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_RSP, rspFlit);

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
        )
        {
            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
            }

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_RSP_OPCODE;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeWriteZero<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hashDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL_DAT;
    }
}


#endif
