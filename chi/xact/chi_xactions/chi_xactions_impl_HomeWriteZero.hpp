//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEWRITEZERO)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEWRITEZERO))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEWRITEZERO
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEWRITEZERO
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeWriteZero : public Xaction<config, conn> {
        public:
            XactionHomeWriteZero(Global<config, conn>*                  glbl,
                                 const Topology&                        topo,
                                 const FiredRequestFlit<config, conn>&  first,
                                 std::shared_ptr<Xaction<config, conn>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeWriteZero<config, conn>> CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

            public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class XactionHomeWriteZero
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeWriteZero<config, conn>::XactionHomeWriteZero(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried
    ) noexcept
        : Xaction<config, conn>(XactionType::HomeWriteZero, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpZero
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->CHECK_FIELD_MAPPING->Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeWriteZero<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeWriteZero<config, conn>> XactionHomeWriteZero<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeWriteZero<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        bool gotDBID = false;
        bool gotComp = false;

        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
            {
                return true;
            }
            else if (iter->opcode.rsp == Opcodes::RSP::DBIDResp)
            {
                gotDBID = true;

                if (gotComp)
                    return true;
            }
            else if (iter->opcode.rsp == Opcodes::RSP::Comp)
            {
                gotComp = true;

                if (gotDBID)
                    return true;
            }
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeWriteZero<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
        )
        {
            if (!rspFlit.IsFromSubordinateToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeWriteZero<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hashDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


#endif
