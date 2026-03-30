//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEREAD)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEREAD))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEREAD
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEREAD
#endif


/*
namespace CHI {
*/
    namespace Xact {
        
        template<FlitConfigurationConcept config>
        class XactionHomeRead : public Xaction<config> {
        public:
            XactionHomeRead(const Global<config>&             glbl,
                            const FiredRequestFlit<config>&   first,
                            std::shared_ptr<Xaction<config>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeRead<config>>  CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsDataComplete(const Global<config>& glbl) const noexcept;

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


// Implementation of: class XactionHomeRead
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionHomeRead<config>::XactionHomeRead(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried
    ) noexcept
        : Xaction<config>(XactionType::HomeRead, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL_NOT_REQ;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::ReadNoSnp
#ifdef CHI_ISSUE_EB_ENABLE
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadNoSnpSep
#endif
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
    inline std::shared_ptr<Xaction<config>> XactionHomeRead<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionHomeRead<config>> XactionHomeRead<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeRead<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        if (this->first.flit.req.Order() != 0)
            return this->HasRSP({ Opcodes::RSP::ReadReceipt });

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsDataComplete(const Global<config>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsFromSubordinate(glbl))
                continue;

            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                collectedDataID |= details::CollectDataID<config>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                collectedDataID |= details::CollectDataID<config>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
#endif
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeRead<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetFirst(
            { Opcodes::RSP::ReadReceipt }, 
            { Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionHomeRead<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource;

        if (!optSource)
            return std::nullopt;

        if (optSource->IsRSP())
            return { optSource->flit.rsp.SrcID() };
        else
            return { optSource->flit.dat.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsDMTPossible() const noexcept
    {
        return this->first.flit.req.ReturnNID() != this->first.flit.req.SrcID();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeRead<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeRead<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeRead<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstDATTo(glbl, XactScope::Requester,
            { Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp });
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeRead<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeRead<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeRead<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeRead<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeRead<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL_NOT_RSP;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::ReadReceipt)
        {
            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (this->first.flit.req.Order() == 0)
                return XactDenial::DENIED_READRECEIPT_ON_NO_ORDER;

            if (this->HasRSP({ Opcodes::RSP::ReadReceipt }))
                return XactDenial::DENIED_READRECEIPT_AFTER_READRECEIPT;

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
    inline XactDenialEnum XactionHomeRead<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_DAT;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL_NOT_DAT;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData
#ifdef CHI_ISSUE_EB_ENABLE
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp
#endif
        )
        {
            if (!datFlit.IsFromSubordinate(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_SN;

            if (datFlit.flit.dat.TgtID() == this->first.flit.req.SrcID())
            {
                if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_DAT_TXNID_MISMATCHING_REQ;
            }
            else if (datFlit.flit.dat.TgtID() == this->first.flit.req.ReturnNID())
            {
                if (datFlit.flit.dat.TxnID() != this->first.flit.req.ReturnTxnID())
                    return XactDenial::DENIED_DAT_TXNID_MISMATCHING_DMT;
            }
            else
                return XactDenial::DENIED_DAT_NOT_TO_HN_OR_RN;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                if (this->HasDAT({ Opcodes::DAT::DataSepResp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_DATASEP;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                if (this->HasDAT({ Opcodes::DAT::CompData }))
                    return XactDenial::DENIED_DATASEPRESP_AFTER_COMPDATA;
            }
#endif

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;
            
            // check DMT consistency
            if (auto optDMTTgtID = this->GetDMTTgtID(glbl))
            {
                if (datFlit.flit.dat.TgtID() != *optDMTTgtID)
                    return XactDenial::DENIED_DMT_INCONSISTENT_TARGET;
            }

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_DAT_OPCODE;
    }
}


#endif
