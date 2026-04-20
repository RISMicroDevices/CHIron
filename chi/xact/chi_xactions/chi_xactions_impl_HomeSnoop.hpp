//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_HOMESNOOP)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMESNOOP))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_HOMESNOOP
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMESNOOP
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionHomeSnoop : public Xaction<config> {
        public:
            XactionHomeSnoop(const Global<config>& glbl, const FiredRequestFlit<config>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeSnoop<config>> CloneAsIs() const noexcept;

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


// Implementation of: class XactionHomeSnoop
namespace /*CHI::*/Xact  {

    template<FlitConfigurationConcept config>
    inline XactionHomeSnoop<config>::XactionHomeSnoop(const Global<config>& glbl, const FiredRequestFlit<config>& first) noexcept
        : Xaction<config>(XactionType::HomeSnoop, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsSNP())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_SNP, this->first);
            return;
        }

        if (
            this->first.flit.snp.Opcode() != Opcodes::SNP::SnpShared
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpClean
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpOnce
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpNotSharedDirty
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpUnique
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpPreferUnique
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpCleanShared
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpCleanInvalid
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpMakeInvalid
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpUniqueStash
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpMakeInvalidStash
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpStashUnique
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpStashShared
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpQuery
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_SNP_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToRequester(glbl))
        {
            this->firstDenial = XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN;
            return;
        }

        //
        if (glbl.CHECK_FIELD_MAPPING.enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING.Check(first.flit.snp);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionHomeSnoop<config>::Clone() const noexcept
    {
        return static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionHomeSnoop<config>> XactionHomeSnoop<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeSnoop<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDataComplete(const Global<config>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config>(Sizes::B64);

        bool needCollectedSnpDataID = false;

        std::bitset<4> collectedSnpDataID;
        
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                return true;

            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
            {
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config>(
                    Sizes::B64, iter->flit.dat.DataID());
                continue;
            }
        }

        return ((completeDataIDMask & ~collectedSnpDataID).none() || !needCollectedSnpDataID);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionHomeSnoop<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeSnoop<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL_NOT_RSP;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
        {
            if (!rspFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_SNP;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_SNP;

            if (this->HasRSP({ Opcodes::RSP::SnpResp }))
            {
                return XactDenial::DENIED_SNPRESP_AFTER_SNPRESP;
            }

            if (auto p = this->GetLastDAT({ 
                    Opcodes::DAT::SnpRespData, 
                    Opcodes::DAT::SnpRespDataPtl }))
            {
                if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespData)
                    return XactDenial::DENIED_SNPRESP_AFTER_SNPRESPDATA;
                else if (p->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                    return XactDenial::DENIED_SNPRESP_AFTER_SNPRESPDATAPTL;
                else
                    assert(false && "should not reach here");

                return XactDenial::DENIED_DUPLICATED_SNPRESP;
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
    inline XactDenialEnum XactionHomeSnoop<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_DAT;
        
        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL_NOT_DAT;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
        )
        {
            if (!datFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_SNP;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_SNP;

            if (!this->NextSNPDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            // check response consistency
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
