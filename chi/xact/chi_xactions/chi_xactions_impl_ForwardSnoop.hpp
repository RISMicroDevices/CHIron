//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_FORWARDSNOOP)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_FORWARDSNOOP))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_FORWARDSNOOP
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_FORWARDSNOOP
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionForwardSnoop : public Xaction<config, conn> {
        public:
            XactionForwardSnoop(const Global<config, conn>& glbl, const FiredRequestFlit<config, conn>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionForwardSnoop<config, conn>>  CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsDataComplete(const Global<config, conn>& glbl) const noexcept;

            virtual bool                    IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept override;
            virtual bool                    IsDBIDComplete(const Global<config, conn>& glbl) const noexcept override;
            virtual bool                    IsComplete(const Global<config, conn>& glbl) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept override;

            bool                            IsDCT() const noexcept;

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


// Implementation of: class XactionForwardSnoop
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionForwardSnoop<config, conn>::XactionForwardSnoop(const Global<config, conn>& glbl, const FiredRequestFlit<config, conn>& first) noexcept
        : Xaction<config, conn>(XactionType::ForwardSnoop, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsSNP())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.snp.Opcode() != Opcodes::SNP::SnpSharedFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpCleanFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpOnceFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpNotSharedDirtyFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpPreferUniqueFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpUniqueFwd
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToRequester(glbl))
        {
            this->firstDenial = XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN;
            return;
        }

        //
        if (glbl.CHECK_FIELD_MAPPING->enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING->Check(first.flit.snp);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionForwardSnoop<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionForwardSnoop<config, conn>> XactionForwardSnoop<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionForwardSnoop<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsResponseComplete(const Global<config, conn>& glbl) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
             || iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDataComplete(const Global<config, conn>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(Sizes::B64);
        
        bool needCollectedSnpDataID = false;
        bool needCollectedFwdDataID = false;

        std::bitset<4> collectedSnpDataID;
        std::bitset<4> collectedFwdDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                return true;
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
                  || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
            {
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Sizes::B64, iter->flit.dat.DataID());
                continue;
            }
            else if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
            {
                needCollectedFwdDataID = true;
                continue;
            }
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
            {
                needCollectedFwdDataID = true;
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Sizes::B64, iter->flit.dat.DataID());
                continue;
            }
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                collectedFwdDataID |= details::CollectDataID<config, conn>(
                    Sizes::B64, iter->flit.dat.DataID());
                continue;
            }
        }

        return ((completeDataIDMask & ~collectedSnpDataID).none() || !needCollectedSnpDataID)
            && ((completeDataIDMask & ~collectedFwdDataID).none() || !needCollectedFwdDataID);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDBIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsComplete(const Global<config, conn>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDCT() const noexcept
    {
        return this->HasRSP({ Opcodes::RSP::SnpRespFwded }) || this->HasDAT({ Opcodes::DAT::SnpRespDataFwded });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionForwardSnoop<config, conn>::GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::REQ<config, conn>::tgtid_t> XactionForwardSnoop<config, conn>::GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        return std::nullopt;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDCTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionForwardSnoop<config, conn>::NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
        {
            if (!rspFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (auto p = this->GetLastRSP({
                    Opcodes::RSP::SnpResp,
                    Opcodes::RSP::SnpRespFwded}))
            {
                if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                {
                    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESP_AFTER_SNPRESP;
                    else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPFWDED_AFTER_SNPRESP;
                    else
                        assert(false && "should not reach here");
                }
                else if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                {
                    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESP_AFTER_SNPRESPFWDED;
                    else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPFWDED_AFTER_SNPRESPFWDED;
                    else
                        assert(false && "should not reach here");
                }
                else
                    assert(false && "should not reach here");

                return XactDenial::DENIED_DUPLICATED_SNPRESP;
            }

            if (auto p = this->GetLastDAT({
                    Opcodes::DAT::SnpRespData, 
                    Opcodes::DAT::SnpRespDataPtl,
                    Opcodes::DAT::SnpRespDataFwded}))
            {
                if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespData)
                {
                    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESP_AFTER_SNPRESPDATA;
                    else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPFWDED_AFTER_SNPRESPDATA;
                    else
                        assert(false && "should not reach here");
                }
                else if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                {
                    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESP_AFTER_SNPRESPDATAPTL;
                    else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPFWDED_AFTER_SNPRESPDATAPTL;
                    else
                        assert(false && "should not reach here");
                }
                else if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
                {
                    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESP_AFTER_SNPRESPDATAFWDED;
                    else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPFWDED_AFTER_SNPRESPDATAFWDED;
                    else
                        assert(false && "should not reach here");
                }
                else
                    assert(false && "should not reach here");

                return XactDenial::DENIED_DUPLICATED_SNPRESP;
            }

            // check forward state and resp
            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
            {
                if (!RespAndFwdStates::IsSnpRespFwdedValid(rspFlit.flit.rsp.Resp(), rspFlit.flit.rsp.FwdState().decay()))
                    return XactDenial::DENIED_SNPRESPFWDED_INVALID_FWDSTATE_RESP;

                // check forward state consistency between forwarded responses
                if (auto p = this->GetLastDAT({ Opcodes::DAT::CompData }))
                {
                    if (rspFlit.flit.rsp.FwdState() != p->flit.dat.Resp())
                        return XactDenial::DENIED_SNPRESPFWDED_COMPDATA_FWDSTATE_MISMATCH;
                }
            }

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
    inline XactDenialEnum XactionForwardSnoop<config, conn>::NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
        {
            if (!datFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData)
            {
                if (auto p = this->GetLastRSP({ 
                    Opcodes::RSP::SnpResp,
                    Opcodes::RSP::SnpRespFwded }))
                {
                    if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESPDATA_AFTER_SNPRESP;
                    else if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPDATA_AFTER_SNPRESPFWDED;
                    else
                        assert(false && "should not reach here");

                    return XactDenial::DENIED_DUPLICATED_SNPRESPDATA;
                }

                if (auto p = this->GetLastDAT({
                    Opcodes::DAT::SnpRespDataPtl,
                    Opcodes::DAT::SnpRespDataFwded }))
                {
                    if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                        return XactDenial::DENIED_SNPRESPDATA_AFTER_SNPRESPDATAPTL;
                    else if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
                        return XactDenial::DENIED_SNPRESPDATA_AFTER_SNPRESPDATAFWDED;
                    else
                        assert(false && "should not reach here");

                    return XactDenial::DENIED_DUPLICATED_SNPRESPDATA;
                }
            }
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
            {
                if (auto p = this->GetLastRSP({ 
                    Opcodes::RSP::SnpResp,
                    Opcodes::RSP::SnpRespFwded }))
                {
                    if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESPDATAPTL_AFTER_SNPRESP;
                    else if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPDATAPTL_AFTER_SNPRESPFWDED;
                    else
                        assert(false && "should not reach here");

                    return XactDenial::DENIED_DUPLICATED_SNPRESPDATA;
                }

                if (auto p = this->GetLastDAT({
                    Opcodes::DAT::SnpRespData,
                    Opcodes::DAT::SnpRespDataFwded}))
                {
                    if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespData)
                        return XactDenial::DENIED_SNPRESPDATAPTL_AFTER_SNPRESPDATA;
                    else if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
                        return XactDenial::DENIED_SNPRESPDATAPTL_AFTER_SNPRESPDATAFWDED;
                    else
                        assert(false && "should not reach here");

                    return XactDenial::DENIED_DUPLICATED_SNPRESPDATA;
                }
            }
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
            {
                if (auto p = this->GetLastRSP({ 
                    Opcodes::RSP::SnpResp,
                    Opcodes::RSP::SnpRespFwded }))
                {
                    if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                        return XactDenial::DENIED_SNPRESPDATAFWDED_AFTER_SNPRESP;
                    else if (p->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
                        return XactDenial::DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPFWDED;
                    else
                        assert(false && "should not reach here");

                    return XactDenial::DENIED_DUPLICATED_SNPRESPDATA;
                }

                if (auto p = this->GetLastDAT({
                    Opcodes::DAT::SnpRespData,
                    Opcodes::DAT::SnpRespDataPtl }))
                {
                    if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespData)
                        return XactDenial::DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPDATA;
                    else if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                        return XactDenial::DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPDATAPTL;
                    else
                        assert(false && "should not reach here");

                    return XactDenial::DENIED_DUPLICATED_SNPRESPDATA;
                }

                // check forward state and resp
                if (!RespAndFwdStates::IsSnpRespDataFwdedValid(datFlit.flit.dat.Resp(), datFlit.flit.dat.FwdState().decay()))
                    return XactDenial::DENIED_SNPRESPDATAFWDED_INVALID_FWDSTATE_RESP;
            }
            else
                assert(false && "should not reach here");

            if (!this->NextSNPDataID(datFlit, {
                Opcodes::DAT::SnpRespData,
                Opcodes::DAT::SnpRespDataPtl,
                Opcodes::DAT::SnpRespDataFwded
            }))
                return XactDenial::DENIED_DUPLICATED_DATAID;
            
            // check forward state consistency between forwarded responses
            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData)
            {
                if (auto p = this->GetLastDAT({ Opcodes::DAT::SnpRespData }))
                {
                    if (datFlit.flit.dat.Resp() != p->flit.dat.Resp())
                        return XactDenial::DENIED_SNPRESPDATA_RESP_MISMATCH;
                }
            }
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
            {
                if (auto p = this->GetLastDAT({ Opcodes::DAT::SnpRespDataPtl }))
                {
                    if (datFlit.flit.dat.Resp() != p->flit.dat.Resp())
                        return XactDenial::DENIED_SNPRESPDATAPTL_RESP_MISMATCH;
                }
            }
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
            {
                if (auto p = this->GetLastDAT({ Opcodes::DAT::SnpRespDataFwded }))
                {
                    if (datFlit.flit.dat.Resp() != p->flit.dat.Resp())
                        return XactDenial::DENIED_SNPRESPDATAFWDED_RESP_MISMATCH;

                    if (datFlit.flit.dat.FwdState() != p->flit.dat.FwdState())
                        return XactDenial::DENIED_SNPRESPDATAFWDED_FWDSTATE_MISMATCH;
                }

                if (auto p = this->GetLastDAT({ Opcodes::DAT::CompData }))
                {
                    if (datFlit.flit.dat.FwdState() != p->flit.dat.Resp())
                        return XactDenial::DENIED_SNPRESPDATAFWDED_COMPDATA_FWDSTATE_MISMATCH;
                }
            }

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
        {
            if (!datFlit.IsToRequesterDCT(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_RN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.FwdNID())
                return XactDenial::DENIED_FWDNID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.FwdTxnID())
                return XactDenial::DENIED_FWDTXNID_MISMATCH;

            if (datFlit.flit.dat.DBID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_DBID_MISMATCH;

            if (datFlit.flit.dat.HomeNID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_HOMENID_MISMATCH;

            if (!this->NextSNPDataID(datFlit, {
                Opcodes::DAT::CompData
            }))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            // check forward state consistency between forwarded responses
            if (auto p = this->GetLastRSP({ Opcodes::RSP::SnpRespFwded }))
            {
                if (datFlit.flit.dat.Resp() != p->flit.rsp.FwdState())
                    return XactDenial::DENIED_SNPRESPFWDED_COMPDATA_FWDSTATE_MISMATCH;
            }
            else if (auto p = this->GetLastDAT({ Opcodes::DAT::SnpRespDataFwded }))
            {
                if (datFlit.flit.dat.Resp() != p->flit.dat.FwdState())
                    return XactDenial::DENIED_SNPRESPDATAFWDED_COMPDATA_FWDSTATE_MISMATCH;
            }
            
            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


#endif
