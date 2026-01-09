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

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeSnoop : public Xaction<config, conn> {
        public:
            XactionHomeSnoop(const Global<config, conn>& glbl, const FiredRequestFlit<config, conn>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeSnoop<config, conn>> CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsDataComplete(const Global<config, conn>& glbl) const noexcept;

            virtual bool                    IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept override;
            virtual bool                    IsDBIDComplete(const Global<config, conn>& glbl) const noexcept override;
            virtual bool                    IsComplete(const Global<config, conn>& glbl) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept override;

        protected:
            virtual const FiredResponseFlit<config, conn>*  
                                            GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept override;
            virtual std::optional<typename Flits::REQ<config, conn>::tgtid_t>
                                            GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class XactionHomeSnoop
namespace /*CHI::*/Xact  {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeSnoop<config, conn>::XactionHomeSnoop(const Global<config, conn>& glbl, const FiredRequestFlit<config, conn>& first) noexcept
        : Xaction<config, conn>(XactionType::HomeSnoop, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsSNP())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
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
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeSnoop<config, conn>::Clone() const noexcept
    {
        return static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeSnoop<config, conn>> XactionHomeSnoop<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeSnoop<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsResponseComplete(const Global<config, conn>& glbl) const noexcept
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsDataComplete(const Global<config, conn>& glbl) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(Sizes::B64);

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
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Sizes::B64, iter->flit.dat.DataID());
                continue;
            }
        }

        return ((completeDataIDMask & ~collectedSnpDataID).none() || !needCollectedSnpDataID);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsDBIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsComplete(const Global<config, conn>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeSnoop<config, conn>::GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::REQ<config, conn>::tgtid_t> XactionHomeSnoop<config, conn>::GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        return std::nullopt;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeSnoop<config, conn>::NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
        {
            if (!rspFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

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
    inline XactDenialEnum XactionHomeSnoop<config, conn>::NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;
        
        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
        )
        {
            if (!datFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

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
