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

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionCopyBackWrite : public Xaction<config, conn> {
        public:
            XactionCopyBackWrite(Global<config, conn>*                  glbl,
                                 const Topology&                        topo,
                                 const FiredRequestFlit<config, conn>&  first,
                                 std::shared_ptr<Xaction<config, conn>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionCopyBackWrite<config, conn>> CloneAsIs() const noexcept;

        public:
            bool                            IsDataComplete(const Topology& topo) const noexcept;
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsAckComplete(const Topology& topo) const noexcept;

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


// Implementation of: class XactionCopyBackWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionCopyBackWrite<config, conn>::XactionCopyBackWrite(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   firstFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::CopyBackWrite, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
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
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->CHECK_FIELD_MAPPING->Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionCopyBackWrite<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionCopyBackWrite<config, conn>> XactionCopyBackWrite<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionCopyBackWrite<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(topo))
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
                if (iter->IsRSP() && iter->IsFromHome(topo) && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif

            if (!iter->IsFromRequester(topo))
                continue;

            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsAckComplete(const Topology& topo) const noexcept
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

                if (iter->IsRSP() && iter->IsFromHome(topo) && iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
                    return true;

                if (!iter->IsToHome(topo))
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionCopyBackWrite<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
#endif
        )
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

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
                    return XactDenial::DENIED_OPCODE;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
                
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;
            }
#endif

            hasDBID = true;
            firstDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
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
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return XactDenial::DENIED_COMPACK_AFTER_DBIDRESP;

            auto optDBID = this->GetDBID();

            if (!optDBID)
                return XactDenial::DENIED_COMPACK_BEFORE_COMP;

            if (rspFlit.flit.rsp.TxnID() != *optDBID)
                return XactDenial::DENIED_TXNID_MISMATCH;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
#endif

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionCopyBackWrite<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
        {
            if (!datFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_DATA_AFTER_COMP;
#endif

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            assert(optDBIDSource->IsRSP() &&
                "DBID never comes from DAT channel in CopyBack Write transactions");

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (auto p = this->GetLastDAT({ Opcodes::DAT::CopyBackWrData }))
            {
                if (datFlit.flit.dat.Resp() != p->flit.dat.Resp())
                    return XactDenial::DENIED_COPYBACKWRDATA_RESP_MISMATCH;
            }

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            //
            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


#endif
