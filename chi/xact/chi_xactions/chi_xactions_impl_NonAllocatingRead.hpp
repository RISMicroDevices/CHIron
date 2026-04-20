//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_NONALLOCATINGREAD)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_NONALLOCATINGREAD))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_NONALLOCATINGREAD
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_NONALLOCATINGREAD
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionNonAllocatingRead : public Xaction<config> {
        public:
            XactionNonAllocatingRead(const Global<config>&             glbl,
                                     const FiredRequestFlit<config>&   first,
                                     std::shared_ptr<Xaction<config>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>          Clone() const noexcept override;
            std::shared_ptr<XactionNonAllocatingRead<config>> CloneAsIs() const noexcept;
        
        public:
            bool                            IsAckComplete(const Global<config>& glbl) const noexcept;
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


// Implementation of: class XactionNonAllocatingRead
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept config>
    inline XactionNonAllocatingRead<config>::XactionNonAllocatingRead(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   firstFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
        : Xaction<config>(XactionType::NonAllocatingRead, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::ReadNoSnp
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadOnce
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadOnceCleanInvalid
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadOnceMakeInvalid 
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

        if (glbl.CHECK_FIELD_MAPPING.enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING.Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionNonAllocatingRead<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionNonAllocatingRead<config>> XactionNonAllocatingRead<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionNonAllocatingRead<config>>(*this);
    }

    template<FlitConfigurationConcept       config>
    inline bool XactionNonAllocatingRead<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        bool gotResp = false;
        bool gotReceipt = this->first.flit.req.Order() == 0;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(glbl))
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
            {
                gotResp = true;
            }
            else if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::ReadReceipt)
            {
                gotReceipt = true;
            }
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                gotResp = true;

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

        return gotResp && gotReceipt && (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsAckComplete(const Global<config>& glbl) const noexcept
    {
        if (!this->first.flit.req.ExpCompAck())
            return true;
            
        size_t index = this->subsequence.size() - 1;
        for (auto iter = this->subsequence.rbegin(); iter != this->subsequence.rend(); iter++, index--)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsRSP() || !iter->IsToHome(glbl))
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionNonAllocatingRead<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetFirst(
            { Opcodes::RSP::ReadReceipt, Opcodes::RSP::RespSepData },
            { Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsDMTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsDCTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionNonAllocatingRead<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionNonAllocatingRead<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstDATFrom(glbl, XactScope::Subordinate,
            { Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp });
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionNonAllocatingRead<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionNonAllocatingRead<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstDATFrom(glbl, XactScope::Requester,
            { Opcodes::DAT::CompData });
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionNonAllocatingRead<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionNonAllocatingRead<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionNonAllocatingRead<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionNonAllocatingRead<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        if (optSource->IsRSP())
            return { optSource->flit.rsp.SrcID() };
        else
            return { optSource->flit.dat.HomeNID() };
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonAllocatingRead<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL_NOT_RSP;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::ReadReceipt)
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (this->HasRSP({ Opcodes::RSP::ReadReceipt }))
                return XactDenial::DENIED_DUPLICATED_READRECEIPT;

            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
#ifdef CHI_ISSUE_EB_ENABLE
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (this->HasRSP({ Opcodes::RSP::RespSepData }))
                return XactDenial::DENIED_RESPSEP_AFTER_RESPSEP;

            if (this->HasDAT({ Opcodes::DAT::CompData }))
                return XactDenial::DENIED_RESPSEPDATA_AFTER_COMPDATA;

            auto optDBID = this->GetDBID();
            if (optDBID)
            {
                if (rspFlit.flit.rsp.DBID() != *optDBID)
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
#endif

        return XactDenial::DENIED_RSP_OPCODE;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionNonAllocatingRead<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
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
            if (!datFlit.IsToRequester(glbl))
                return XactDenial::DENIED_DAT_NOT_TO_RN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_REQ;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_REQ;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                if (this->HasDAT({ Opcodes::DAT::DataSepResp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_DATASEP;

                if (this->HasRSP({ Opcodes::RSP::RespSepData }))
                    return XactDenial::DENIED_COMPDATA_AFTER_RESPSEP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_COMP;

                if (auto p = this->GetLastDAT({ Opcodes::DAT::CompData }))
                {
                    if (datFlit.flit.dat.Resp() != p->flit.dat.Resp())
                        return XactDenial::DENIED_COMPDATA_RESP_MISMATCH;
                }
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                if (this->HasDAT({ Opcodes::DAT::CompData }))
                    return XactDenial::DENIED_DATASEPRESP_AFTER_COMPDATA;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_DATASEPRESP_AFTER_COMP;

                if (auto p = this->GetLastDAT({ Opcodes::DAT::DataSepResp }))
                {
                    if (datFlit.flit.dat.Resp() != p->flit.dat.Resp())
                        return XactDenial::DENIED_DATASEPRESP_RESP_MISMATCH;
                }
            }
#endif

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            auto optDBID = this->GetDBID();
            if (optDBID)
            {
                if (datFlit.flit.dat.DBID() != *optDBID)
                    return XactDenial::DENIED_DAT_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            // check DMT/DCT consistency
            if (auto optDMTSrcID = this->GetDMTSrcID(glbl))
            {
                if (datFlit.flit.dat.SrcID() != *optDMTSrcID)
                    return XactDenial::DENIED_DMT_INCONSISTENT_SOURCE;
            }
            else if (auto optDCTSrcID = this->GetDCTSrcID(glbl))
            {
                if (datFlit.flit.dat.SrcID() != *optDCTSrcID)
                    return XactDenial::DENIED_DCT_INCONSISTENT_SOURCE;
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
