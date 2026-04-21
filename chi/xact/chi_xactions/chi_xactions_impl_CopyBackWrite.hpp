//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_COPYBACKWRITE)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_COPYBACKWRITE))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_COPYBACKWRITE
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_COPYBACKWRITE
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionCopyBackWrite : public Xaction<config> {
        public:
            XactionCopyBackWrite(const Global<config>&            glbl,
                                 const FiredRequestFlit<config>&  first,
                                 std::shared_ptr<Xaction<config>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>      Clone() const noexcept override;
            std::shared_ptr<XactionCopyBackWrite<config>> CloneAsIs() const noexcept;

        public:
            bool                            IsDataComplete(const Global<config>& glbl) const noexcept;
            bool                            IsResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsAckComplete(const Global<config>& glbl) const noexcept;

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


// Implementation of: class XactionCopyBackWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionCopyBackWrite<config>::XactionCopyBackWrite(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   firstFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
        : Xaction<config>(XactionType::CopyBackWrite, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteBackPtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteBackFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteCleanFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictFull
#ifdef CHI_ISSUE_EB_ENABLE
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict
#endif
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_REQ_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(glbl))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl.CHECK_FIELD_MAPPING.enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING.Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionCopyBackWrite<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionCopyBackWrite<config>> XactionCopyBackWrite<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionCopyBackWrite<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(glbl))
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
                return true;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvictOnly
            if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
                if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsDataComplete(const Global<config>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
                if (iter->IsRSP() && iter->IsFromHome(glbl) && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif

            if (!iter->IsFromRequester(glbl))
                continue;

            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
            {
                collectedDataID |= details::CollectDataID<config>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsAckComplete(const Global<config>& glbl) const noexcept
    {
#ifdef CHI_ISSUE_EB_ENABLE
        // WriteEvictOrEvict only
        if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
        {
            size_t index = this->subsequence.size() - 1;
            for (auto iter = this->subsequence.rbegin(); iter != this->subsequence.rend(); iter++, index--)
            {
                if (this->subsequenceKeys[index].IsDenied())
                    continue;

                if (iter->IsRSP() && iter->IsFromHome(glbl) && iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
                    return true;

                if (!iter->IsToHome(glbl))
                    continue;

                if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                    return true;
            }
        }
        else
            return true;

        return false;
#else
        return true;
#endif
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsDataComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsDataComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionCopyBackWrite<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstRSP(
            { Opcodes::RSP::CompDBIDResp, Opcodes::RSP::Comp });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionCopyBackWrite<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionCopyBackWrite<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionCopyBackWrite<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionCopyBackWrite<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionCopyBackWrite<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionCopyBackWrite<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionCopyBackWrite<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionCopyBackWrite<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCopyBackWrite<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_RSP, rspFlit);

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
#endif
        )
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

#ifdef CHI_ISSUE_EB_ENABLE
                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;
#endif
            }
#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                    return XactDenial::DENIED_RSP_OPCODE;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
                
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;
            }
#endif

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
#ifdef CHI_ISSUE_EB_ENABLE
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck
        )
        {
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                return XactDenial::DENIED_RSP_OPCODE;

            if (!rspFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return XactDenial::DENIED_COMPACK_AFTER_DBIDRESP;

            auto optDBID = this->GetDBID();

            if (!optDBID)
                return XactDenial::DENIED_COMPACK_BEFORE_COMP;

            if (rspFlit.flit.rsp.TxnID() != *optDBID)
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_DBID;

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
#endif

        return XactDenial::DENIED_RSP_OPCODE;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionCopyBackWrite<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_DAT;

        if (!datFlit.IsDAT())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DAT, datFlit);

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
        {
            if (!datFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_DATA_AFTER_COMP;
#endif

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            assert(optDBIDSource->IsRSP() &&
                "DBID never comes from DAT channel in CopyBack Write transactions");

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_RSP;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_DBID;

            if (auto p = this->GetLastDAT({ Opcodes::DAT::CopyBackWrData }))
            {
                if (datFlit.flit.dat.Resp() != p->flit.dat.Resp())
                    return XactDenial::DENIED_COPYBACKWRDATA_RESP_MISMATCH;
            }

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            //
            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_DAT_OPCODE;
    }
}


#endif
