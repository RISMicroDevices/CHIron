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
        protected:
            bool                                            dataPullResponse;
            uint8_t                                         dataPullEncoding;
            Flits::RSP<config>::dbid_t                      dataPullDBID;
            std::shared_ptr<XactionAllocatingRead<config>>  dataPullRead;

        public:
            XactionHomeSnoop(const Global<config>& glbl, const FiredRequestFlit<config>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeSnoop<config>> CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsDataComplete(const Global<config>& glbl) const noexcept;
            bool                            IsDataPull() const noexcept;
            Flits::RSP<config>::dbid_t      GetDataPullDBID() const noexcept;

            virtual bool                    IsTxnIDComplete(const Global<config>& glbl) const noexcept override;
            virtual bool                    IsDBIDComplete(const Global<config>& glbl) const noexcept override;
            virtual bool                    IsComplete(const Global<config>& glbl) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Global<config>& glbl) const noexcept override;

        protected:
            bool                            IsStash() const noexcept;
            XactDenialEnum                  NextDataPullNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& flit) noexcept;

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
        , dataPullResponse  (false)
        , dataPullEncoding  (0)
        , dataPullDBID      (0)
        , dataPullRead      (nullptr)
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
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_SNP_OPCODE, this->first,
                "This Opcode is not type of / supported by Home Snoop transaction");
            return;
        }

        if (!this->first.IsFromHomeToRequester(glbl))
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN, this->first);
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
        auto cloned = std::make_shared<XactionHomeSnoop<config>>(*this);
        if (dataPullRead)
            cloned->dataPullRead = dataPullRead->CloneAsIs();
        return cloned;
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
    inline bool XactionHomeSnoop<config>::IsDataPull() const noexcept
    {
        return dataPullRead != nullptr;
    }

    template<FlitConfigurationConcept config>
    inline Flits::RSP<config>::dbid_t XactionHomeSnoop<config>::GetDataPullDBID() const noexcept
    {
        return dataPullDBID;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl) &&
            (!dataPullRead || dataPullRead->IsResponseComplete(glbl));
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl) &&
            (!dataPullRead || dataPullRead->IsAckComplete(glbl));
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsResponseComplete(glbl) && IsDataComplete(glbl) &&
            (!dataPullRead || dataPullRead->IsComplete(glbl));
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsStash() const noexcept
    {
        return this->first.flit.snp.Opcode() == Opcodes::SNP::SnpUniqueStash
            || this->first.flit.snp.Opcode() == Opcodes::SNP::SnpMakeInvalidStash
            || this->first.flit.snp.Opcode() == Opcodes::SNP::SnpStashUnique
            || this->first.flit.snp.Opcode() == Opcodes::SNP::SnpStashShared;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeSnoop<config>::NextDataPullNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& flit) noexcept
    {
        const uint8_t encoding = flit.IsRSP() ? uint8_t(flit.flit.rsp.DataPull()) : uint8_t(flit.flit.dat.DataPull());
        const uint16_t dbid = flit.IsRSP() ? uint16_t(flit.flit.rsp.DBID()) : uint16_t(flit.flit.dat.DBID());

        if (!IsStash())
        {
            if (flit.IsRSP() && encoding != 0)
                return XactDenial::DENIED_RSP_FIELD_DATAPULL;
            return XactDenial::ACCEPTED;
        }

        if (encoding > 1)
            return flit.IsRSP() ? XactDenial::DENIED_RSP_FIELD_DATAPULL : XactDenial::DENIED_DAT_FIELD_DATAPULL;

        if (dataPullResponse)
        {
            if (encoding != dataPullEncoding)
                return flit.IsRSP() ? XactDenial::DENIED_RSP_FIELD_DATAPULL : XactDenial::DENIED_DAT_FIELD_DATAPULL;

            if (dataPullRead && dbid != dataPullDBID)
                return flit.IsRSP() ? XactDenial::DENIED_RSP_DBID_MISMATCH : XactDenial::DENIED_DAT_DBID_MISMATCH;

            return XactDenial::ACCEPTED;
        }

        if (encoding == 0)
        {
            dataPullResponse = true;
            dataPullEncoding = encoding;
            dataPullDBID = dbid;
            return XactDenial::ACCEPTED;
        }

        bool valid = false;
        if (this->first.flit.snp.Opcode() == Opcodes::SNP::SnpUniqueStash)
        {
            if (flit.IsRSP())
                valid = flit.flit.rsp.Resp() == Resps::SnpResp_I;
            else if (flit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData)
                valid = flit.flit.dat.Resp() == Resps::SnpRespData_I || flit.flit.dat.Resp() == Resps::SnpRespData_I_PD;
            else if (flit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                valid = flit.flit.dat.Resp() == Resps::SnpRespDataPtl_I_PD;
        }
        else if (this->first.flit.snp.Opcode() == Opcodes::SNP::SnpMakeInvalidStash)
        {
            valid = flit.IsRSP() && flit.flit.rsp.Resp() == Resps::SnpResp_I;
        }
        else if (this->first.flit.snp.Opcode() == Opcodes::SNP::SnpStashUnique)
        {
            valid = flit.IsRSP() &&
                (flit.flit.rsp.Resp() == Resps::SnpResp_I || flit.flit.rsp.Resp() == Resps::SnpResp_UC ||
                 flit.flit.rsp.Resp() == Resps::SnpResp_SC || flit.flit.rsp.Resp() == Resps::SnpResp_SD);
        }
        else if (this->first.flit.snp.Opcode() == Opcodes::SNP::SnpStashShared)
        {
            valid = flit.IsRSP() &&
                (flit.flit.rsp.Resp() == Resps::SnpResp_I || flit.flit.rsp.Resp() == Resps::SnpResp_UC);
        }

        if (!valid)
            return flit.IsRSP() ? XactDenial::DENIED_RSP_FIELD_DATAPULL : XactDenial::DENIED_DAT_FIELD_DATAPULL;

        Flits::REQ<config> request{};
        request.QoS() = this->first.flit.snp.QoS();
        request.TgtID() = this->first.flit.snp.SrcID();
        request.SrcID() = flit.IsRSP() ? flit.flit.rsp.SrcID() : flit.flit.dat.SrcID();
        request.TxnID() = dbid;
        request.Opcode() = this->first.flit.snp.Opcode() == Opcodes::SNP::SnpStashShared
            ? Opcodes::REQ::ReadNotSharedDirty : Opcodes::REQ::ReadUnique;
        request.Size() = Sizes::B64;
        request.Addr() = uint64_t(this->first.flit.snp.Addr()) << 3;
        request.NS() = this->first.flit.snp.NS();
        request.AllowRetry() = 1;
        request.SnpAttr() = 1;
        request.MemAttr() = MemAttrs::WriteBack_Allocate;
        request.ExpCompAck() = 1;
        request.TraceTag() = this->first.flit.snp.TraceTag();
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (Flits::REQ<config>::hasMPAM && Flits::SNP<config>::hasMPAM)
            request.MPAM() = this->first.flit.snp.MPAM();
#endif

        FiredRequestFlit<config> firedRequest(XactScope::Requester, true, flit.time, request);
        auto read = std::make_shared<XactionAllocatingRead<config>>(glbl, firedRequest);
        if (read->GetFirstDenial() != XactDenial::ACCEPTED)
            return read->GetFirstDenial();

        dataPullResponse = true;
        dataPullEncoding = encoding;
        dataPullDBID = dbid;
        dataPullRead = read;
        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeSnoop<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        if (dataPullRead)
            return this->GetFirst(
                { Opcodes::RSP::RespSepData },
                { Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp });
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionHomeSnoop<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* source = GetPrimaryTgtIDSourceNonREQ(glbl);
        if (!source)
            return std::nullopt;
        if (source->IsRSP())
            return { source->flit.rsp.SrcID() };
        return { source->flit.dat.HomeNID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeSnoop<config>::IsDMTPossible() const noexcept
    {
        return IsDataPull();
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
        if (dataPullRead)
            return dataPullRead->GetDMTSrcIDSource(glbl);
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
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_RSP, rspFlit);

        if (!rspFlit.IsRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_RSP, rspFlit);

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
        {
            if (!rspFlit.IsFromRequesterToHome(glbl))
                return this->ResponseFlitDenied(XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN, rspFlit);

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

            return NextDataPullNoRecord(glbl, rspFlit);
        }
        else if (dataPullRead && rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
        {
            return dataPullRead->NextRSP(glbl, rspFlit, hasDBID, firstDBID);
        }
#ifdef CHI_ISSUE_EB_ENABLE
        else if (dataPullRead && rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
        {
            return dataPullRead->NextRSP(glbl, rspFlit, hasDBID, firstDBID);
        }
#endif

        return this->ResponseFlitDenied(XactDenial::DENIED_RSP_OPCODE, rspFlit,
            "This RSP Opcode is not expected for Home Snoop transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeSnoop<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DAT, datFlit);
        
        if (!datFlit.IsDAT())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DAT, datFlit);

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
        )
        {
            if (!datFlit.IsFromRequesterToHome(glbl))
                return this->ResponseFlitDenied(XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN, datFlit);

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_SNP;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_SNP;

            if (!this->NextSNPDataID(datFlit, {
                Opcodes::DAT::SnpRespData,
                Opcodes::DAT::SnpRespDataPtl
            }))
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

            return NextDataPullNoRecord(glbl, datFlit);
        }
        else if (dataPullRead &&
            (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData
#ifdef CHI_ISSUE_EB_ENABLE
          || datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp
#endif
            ))
        {
            return dataPullRead->NextDAT(glbl, datFlit, hasDBID, firstDBID);
        }

        return this->ResponseFlitDenied(XactDenial::DENIED_DAT_OPCODE, datFlit,
            "This DAT Opcode is not expected for Home Snoop transactions");
    }
}


#endif
