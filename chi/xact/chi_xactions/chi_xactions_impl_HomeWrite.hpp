//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEWRITE)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEWRITE))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEWRITE
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEWRITE
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionHomeWrite : public Xaction<config> {
        public:
            XactionHomeWrite(const Global<config>&            glbl,
                             const FiredRequestFlit<config>&  first,
                             std::shared_ptr<Xaction<config>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeWrite<config>> CloneAsIs() const noexcept;

        public:
            bool                            IsDBIDResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsCompResponseComplete(const Global<config>& glbl) const noexcept;
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


// Implementation of: class XactionHomeWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionHomeWrite<config>::XactionHomeWrite(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried
    ) noexcept
        : Xaction<config>(XactionType::HomeWrite, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpPtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpFull
        )
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
    inline std::shared_ptr<Xaction<config>> XactionHomeWrite<config>::Clone() const noexcept
    {
        return static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionHomeWrite<config>> XactionHomeWrite<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeWrite<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsDBIDResponseComplete(const Global<config>& glbl) const noexcept
    {
        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (iter->opcode.rsp == Opcodes::RSP::DBIDResp
             || iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
                return true;
        }
        
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsCompResponseComplete(const Global<config>& glbl) const noexcept
    {
        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;
            
            if (iter->opcode.rsp == Opcodes::RSP::Comp
             || iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsDataComplete(const Global<config>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsDAT())
                continue;

            if (iter->flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
            {
                collectedDataID |= details::CollectDataID<config>(
                        this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
            else if (iter->flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
                return true;
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsDBIDResponseComplete(glbl) && IsCompResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsDBIDResponseComplete(glbl) && IsCompResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return IsDBIDResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWrite<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetLastRSP(
            { Opcodes::RSP::DBIDResp, Opcodes::RSP::Comp, Opcodes::RSP::CompDBIDResp });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionHomeWrite<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeWrite<config>::IsDWTPossible() const noexcept
    {
        return this->first.flit.req.DoDWT();
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWrite<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWrite<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWrite<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWrite<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWrite<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        // *NOTICE: The actual SrcID source is from ReturnNID of first REQ flit,
        //          and DWT route was confirmed by first DBIDResp to RN.
        //          The TgtID check of DBIDResp should be always guaranteed when DoDWT=1 regardless of
        //          any topological-actual DWT happened.
        return this->GetFirstRSPTo(glbl, XactScope::Requester,
            { Opcodes::RSP::DBIDResp });
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeWrite<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeWrite<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_RSP, rspFlit);

        if (!rspFlit.IsRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_RSP, rspFlit);

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;

            if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

            hasDBID = true;
            firstDBID = true;
    
            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }
    
            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Home Write transactions");

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_RSP_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp)
        {
#ifdef CHI_ISSUE_EB_ENABLE
            if (this->first.flit.req.DoDWT())
            {
                if (!rspFlit.IsFromSubordinateToRequester(glbl) && !rspFlit.IsFromSubordinateToHome(glbl))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN;

                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.ReturnNID())
                {
                    if (rspFlit.flit.rsp.TgtID() == this->first.flit.req.SrcID())
                        return XactDenial::DENIED_DWT_INOPERATIVENESS;

                    return XactDenial::DENIED_DWT_TGTID_MISMATCH;
                }
                    
                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.ReturnTxnID())
                    return XactDenial::DENIED_DWT_TXNID_MISMATCH;
            }
            else
#endif
            {
                if (!rspFlit.IsFromSubordinateToHome(glbl))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                    return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;
            }

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Home Write transactions");

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_RSP_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

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
    inline XactDenialEnum XactionHomeWrite<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DAT, datFlit);

        if (!datFlit.IsDAT())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DAT, datFlit);

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel
        )
        {
            const FiredResponseFlit<config>* optDBIDSource
                = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp,
                    Opcodes::RSP::CompDBIDResp });

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_RSP;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_DBID;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
            {
                if (this->HasDAT({ Opcodes::DAT::WriteDataCancel }))
                    return XactDenial::DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL;

                if (!this->NextREQDataID(datFlit))
                    return XactDenial::DENIED_DUPLICATED_DATAID;
            }
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
            {
                if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                    return XactDenial::DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA;
            }

            // check DWT consistency
            if (auto optDWTSrcID = this->GetDWTSrcID(glbl))
            {
                if (datFlit.flit.dat.SrcID() != *optDWTSrcID)
                    return XactDenial::DENIED_DWT_INCONSISTENT_SOURCE;
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
