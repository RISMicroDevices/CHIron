//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_DVMSNOOP)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_DVMSNOOP))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_DVMSNOOP
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_DVMSNOOP
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionDVMSnoop : public Xaction<config> {
        protected:
            FiredRequestFlit<config>    second;
            XactDenialEnum              secondDenial;

        public:
            XactionDVMSnoop(const Global<config>& glbl, const FiredRequestFlit<config>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>      Clone() const noexcept override;
            std::shared_ptr<XactionDVMSnoop<config>>      CloneAsIs() const noexcept;

        public:
            bool                            IsRequestComplete(const Global<config>& glbl) const noexcept;
            bool                            IsResponseComplete(const Global<config>& glbl) const noexcept;;

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

        public:
            XactDenialEnum                  NextSNP(const Global<config>& glbl, const FiredRequestFlit<config>& snpFlit) noexcept;
            XactDenialEnum                  NextSNPNoRecord(const Global<config>& glbl, const FiredRequestFlit<config>& snpFlit) noexcept;
        };
    }
/*
}
*/


// Implementation of: class XactionDVMSnoop
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionDVMSnoop<config>::XactionDVMSnoop(const Global<config>& glbl, const FiredRequestFlit<config>& first) noexcept
        : Xaction<config> (XactionType::DVMSnoop, first)
        , second                ()
        , secondDenial          (XactDenial::NOT_INITIALIZED)
    {
        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsSNP())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_SNP, this->first);
            return;
        }

        if (this->first.flit.snp.Opcode() != Opcodes::SNP::SnpDVMOp) [[unlikely]]
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
    inline std::shared_ptr<Xaction<config>> XactionDVMSnoop<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionDVMSnoop<config>> XactionDVMSnoop<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionDVMSnoop<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsRequestComplete(const Global<config>& glbl) const noexcept
    {
        return this->firstDenial == XactDenial::ACCEPTED && this->secondDenial == XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        return this->HasRSP({ Opcodes::RSP::SnpResp });
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return IsRequestComplete(glbl) && IsResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDVMSnoop<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionDVMSnoop<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDVMSnoop<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDVMSnoop<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDVMSnoop<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDVMSnoop<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDVMSnoop<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDVMSnoop<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDVMSnoop<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionDVMSnoop<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
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
                return XactDenial::DENIED_SNP_TXNID_MISMATCHING_SNP;

            if (!this->IsRequestComplete(glbl))
                return XactDenial::DENIED_SNPRESP_BEFORE_ALL_SNPDVMOP;

            if (this->HasRSP({ Opcodes::RSP::SnpResp }))
                return XactDenial::DENIED_SNPRESP_AFTER_SNPRESP;

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
    inline XactDenialEnum XactionDVMSnoop<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL_DAT;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionDVMSnoop<config>::NextSNP(const Global<config>& glbl, const FiredRequestFlit<config>& snpFlit) noexcept
    {
        if (!snpFlit.IsSNP()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL_NOT_SNP;

        XactDenialEnum denial = NextSNPNoRecord(glbl, snpFlit);

        this->second = snpFlit;
        this->secondDenial = denial;

        // TODO: When receiving unexpected third SnpDVMOp, the second was overlapped and replaced,
        //       a sequence might be needed to record all second SnpDVMOp flit.

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionDVMSnoop<config>::NextSNPNoRecord(const Global<config>& glbl, const FiredRequestFlit<config>& snpFlit) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_SNP;

        if (this->IsRequestComplete(glbl))
            return XactDenial::DENIED_DUPLICATED_SNPDVMOP;

        if (snpFlit.flit.snp.Opcode() == Opcodes::SNP::SnpDVMOp)
        {
            if (!snpFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN;

            if (snpFlit.flit.snp.SrcID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_SRCID_MISMATCH;

            if (snpFlit.flit.snp.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_SNP_TXNID_MISMATCHING_SNP;

            if ((snpFlit.flit.snp.Addr() & 0x1) == (this->first.flit.snp.Addr() & 0x1))
                return XactDenial::DENIED_DUPLICATED_SNPDVMOP;

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(snpFlit.flit.snp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_SNP_OPCODE;
    }
}


#endif
