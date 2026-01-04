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

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionDVMSnoop : public Xaction<config, conn> {
        protected:
            FiredRequestFlit<config, conn>  second;
            XactDenialEnum                  secondDenial;

        public:
            XactionDVMSnoop(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionDVMSnoop<config, conn>>      CloneAsIs() const noexcept;

        public:
            bool                            IsRequestComplete(const Topology& topo) const noexcept;
            bool                            IsResponseComplete(const Topology& topo) const noexcept;;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;

        public:
            XactDenialEnum                  NextSNP(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& snpFlit) noexcept;
            XactDenialEnum                  NextSNPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& snpFlit) noexcept;
        };
    }
/*
}
*/


// Implementation of: class XactionDVMSnoop
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionDVMSnoop<config, conn>::XactionDVMSnoop(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& first) noexcept
        : Xaction<config, conn> (XactionType::DVMSnoop, first)
        , second                ()
        , secondDenial          (XactDenial::NOT_INITIALIZED)
    {
        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsSNP())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (this->first.flit.snp.Opcode() != Opcodes::SNP::SnpDVMOp) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToRequester(topo))
        {
            this->firstDenial = XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->CHECK_FIELD_MAPPING->Check(first.flit.snp);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionDVMSnoop<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionDVMSnoop<config, conn>> XactionDVMSnoop<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionDVMSnoop<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDVMSnoop<config, conn>::IsRequestComplete(const Topology& topo) const noexcept
    {
        return this->firstDenial == XactDenial::ACCEPTED && this->secondDenial == XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDVMSnoop<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        return this->HasRSP({ Opcodes::RSP::SnpResp });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDVMSnoop<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return IsComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDVMSnoop<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDVMSnoop<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return IsRequestComplete(topo) && IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDVMSnoop<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
        {
            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (!this->IsRequestComplete(topo))
                return XactDenial::DENIED_SNPRESP_BEFORE_ALL_SNPDVMOP;

            if (this->HasRSP({ Opcodes::RSP::SnpResp }))
                return XactDenial::DENIED_SNPRESP_AFTER_SNPRESP;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDVMSnoop<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDVMSnoop<config, conn>::NextSNP(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& snpFlit) noexcept
    {
        if (!snpFlit.IsSNP()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL;

        XactDenialEnum denial = NextSNPNoRecord(glbl, topo, snpFlit);

        this->second = snpFlit;
        this->secondDenial = denial;

        // TODO: When receiving unexpected third SnpDVMOp, the second was overlapped and replaced,
        //       a sequence might be needed to record all second SnpDVMOp flit.

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDVMSnoop<config, conn>::NextSNPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& snpFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (this->IsRequestComplete(topo))
            return XactDenial::DENIED_DUPLICATED_SNPDVMOP;

        if (snpFlit.flit.snp.Opcode() == Opcodes::SNP::SnpDVMOp)
        {
            if (!snpFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN;

            if (snpFlit.flit.snp.SrcID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_SRCID_MISMATCH;

            if (snpFlit.flit.snp.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if ((snpFlit.flit.snp.Addr() & 0x1) == (this->first.flit.snp.Addr() & 0x1))
                return XactDenial::DENIED_DUPLICATED_SNPDVMOP;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(snpFlit.flit.snp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


#endif
