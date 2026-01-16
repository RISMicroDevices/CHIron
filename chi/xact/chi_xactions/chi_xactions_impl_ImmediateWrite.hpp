//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_IMMEDIATEWRITE)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_IMMEDIATEWRITE))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_IMMEDIATEWRITE
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_IMMEDIATEWRITE
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionImmediateWrite : public Xaction<config, conn> {
        public:
            XactionImmediateWrite(const Global<config, conn>&               glbl,
                                  const FiredRequestFlit<config, conn>&     first,
                                  std::shared_ptr<Xaction<config, conn>>    retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>          Clone() const noexcept override;
            std::shared_ptr<XactionImmediateWrite<config, conn>>    CloneAsIs() const noexcept;

        public:
            bool                            IsDataComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsResponseComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsAckComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsTagOpComplete(const Global<config, conn>& glbl) const noexcept;

            virtual bool                    IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept override;
            virtual bool                    IsDBIDComplete(const Global<config, conn>& glbl) const noexcept override;
            virtual bool                    IsComplete(const Global<config, conn>& glbl) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept override;

        protected:
            virtual const FiredResponseFlit<config, conn>*  
                                            GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept override;
            virtual std::optional<typename Flits::REQ<config, conn>::tgtid_t>
                                            GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept override;

            virtual bool                    IsDMTPossible() const noexcept override;
            virtual bool                    IsDCTPossible() const noexcept override;
            virtual bool                    IsDWTPossible() const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class XactionImmediateWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionImmediateWrite<config, conn>::XactionImmediateWrite(
        const Global<config, conn>&             glbl,
        const FiredRequestFlit<config, conn>&   firstFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::ImmediateWrite, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpPtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniquePtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniqueFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniquePtlStash
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniqueFullStash
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(glbl))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl.CHECK_FIELD_MAPPING->enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING->Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionImmediateWrite<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionImmediateWrite<config, conn>> XactionImmediateWrite<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionImmediateWrite<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsResponseComplete(const Global<config, conn>& glbl) const noexcept
    {
        bool gotDBID = false;
        bool gotComp = false;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(glbl) || !iter->IsRSP())
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            {
                gotDBID = true;
                gotComp = true;
                break;
            }
            else if (
                iter->flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
             || iter->flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd
#endif
            )
            {
                gotDBID = true;

                if (gotComp)
                    break;
            }
            else if (iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                gotComp = true;

                if (gotDBID)
                    break;
            }
        }

        return gotDBID && gotComp;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDataComplete(const Global<config, conn>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());
        
        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsFromRequester(glbl) || !iter->IsDAT())
                continue;

            if (
                iter->flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData
#ifdef CHI_ISSUE_EB_ENABLE
             || iter->flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck
#endif
            )
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
            else if (iter->flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
                return true;
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsAckComplete(const Global<config, conn>& glbl) const noexcept
    {
        if (!this->first.flit.req.ExpCompAck())
            return true;

        size_t index = this->subsequence.size() - 1;
        for (auto iter = this->subsequence.rbegin(); iter != this->subsequence.rend(); iter++, index--)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToHome(glbl))
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                return true;

#ifdef CHI_ISSUE_EB_ENABLE
            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck)
                return true;
#endif
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsTagOpComplete(const Global<config, conn>& glbl) const noexcept
    {
        if (this->first.flit.req.TagOp() != TagOp::MatchFetch)
            return true;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index--)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(glbl) || !iter->IsRSP())
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::TagMatch)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDBIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsDataComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsComplete(const Global<config, conn>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsDataComplete(glbl) && IsAckComplete(glbl) && IsTagOpComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept
    {
        return IsDataComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionImmediateWrite<config, conn>::GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (iter->opcode.rsp == Opcodes::RSP::DBIDResp)
            {
                // DBIDResp could be sent from DWT target, which was not the primary SAM target
                if (this->subsequence[index].IsFromHome(glbl))
                    return &(this->subsequence[index]);
            }
            else if (iter->opcode.rsp == Opcodes::RSP::DBIDRespOrd
                  || iter->opcode.rsp == Opcodes::RSP::Comp
                  || iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
            {
                return &(this->subsequence[index]);
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::REQ<config, conn>::tgtid_t> XactionImmediateWrite<config, conn>::GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        const FiredResponseFlit<config, conn>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDWTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionImmediateWrite<config, conn>::NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;

            if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

            hasDBID = true;
            firstDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Immediate Write transactions");

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
    
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (
                    true
#ifdef CHI_ISSUE_EB_ENABLE
                 && !this->first.flit.req.DoDWT() // Never check DBID of Comp on DWT
#endif
                )
                {
                    if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                        return XactDenial::DENIED_DBID_MISMATCH;
                }
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd
#endif
        )
        {
            if (!rspFlit.IsToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_TO_RN;

            if (rspFlit.IsFromSubordinate(glbl))
            {
                if (this->first.flit.req.ExpCompAck())
                    return XactDenial::DENIED_DWT_ON_EXPCOMPACK;

#ifdef CHI_ISSUE_EB_ENABLE
                if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd)
                    return XactDenial::DENIED_DWT_WITH_DBIDRESPORD;
#endif
            }

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Immediate Write transactions");

                if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;
                
                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::TagMatch)
        {
            // TODO: MTE features currently not supported

            return XactDenial::DENIED_UNSUPPORTED_FEATURE_OPCODE;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
        {
            if (!this->first.flit.req.ExpCompAck())
                return XactDenial::DENIED_COMPACK_ON_NON_EXPCOMPACK;

            if (!rspFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Immediate Write transactions");

                if (rspFlit.flit.rsp.TgtID() != optDBIDSource->flit.rsp.SrcID())
                    return XactDenial::DENIED_TGTID_MISMATCH;

                if (rspFlit.flit.rsp.TxnID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_TXNID_MISMATCH;
            }
            else
                return XactDenial::DENIED_COMPACK_BEFORE_DBID;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionImmediateWrite<config, conn>::NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData
#ifdef CHI_ISSUE_EB_ENABLE
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck
#endif
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel
        )
        {
            const FiredResponseFlit<config, conn>* optDBIDSource 
                = this->GetLastDBIDSourceRSP({ 
                    Opcodes::RSP::DBIDResp,
                    Opcodes::RSP::DBIDRespOrd, 
                    Opcodes::RSP::CompDBIDResp });

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
            {
                if (this->HasDAT({ Opcodes::DAT::NCBWrDataCompAck }))
                    return XactDenial::DENIED_NCBWRDATA_AFTER_NCBWRDATACOMPACK;

                if (this->HasDAT({ Opcodes::DAT::WriteDataCancel }))
                    return XactDenial::DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL;

                if (!this->NextREQDataID(datFlit))
                    return XactDenial::DENIED_DUPLICATED_DATAID;

                //
                if (glbl.CHECK_FIELD_MAPPING->enable)
                {
                    XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck)
            {
                if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                    return XactDenial::DENIED_NCBWRDATACOMPACK_AFTER_NCBWRDATA;

                if (this->HasDAT({ Opcodes::DAT::WriteDataCancel }))
                    return XactDenial::DENIED_NCBWRDATACOMPACK_AFTER_WRITEDATACANCEL;

                if (!this->NextREQDataID(datFlit))
                    return XactDenial::DENIED_DUPLICATED_DATAID;

                //
                if (glbl.CHECK_FIELD_MAPPING->enable)
                {
                    XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
#endif
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
            {
                if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                    return XactDenial::DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA;

                if (this->HasDAT({ Opcodes::DAT::NCBWrDataCompAck }))
                    return XactDenial::DENIED_WRITEDATACANCEL_AFTER_NCBWRDATACOMPACK;

                //
                if (glbl.CHECK_FIELD_MAPPING->enable)
                {
                    XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
        }

        return XactDenial::DENIED_OPCODE;
    }
}


#endif
