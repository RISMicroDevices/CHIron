//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_DATALESS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_DATALESS))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_DATALESS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_DATALESS
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionDataless : public Xaction<config, conn> {
        public:
            XactionDataless(Global<config, conn>*                   glbl,
                            const Topology&                         topo,
                            const FiredRequestFlit<config, conn>&   first,
                            std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>  Clone() const noexcept override;
            std::shared_ptr<XactionDataless<config, conn>>  CloneAsIs() const noexcept;

        public:
            bool                            IsCompResponseComplete(const Topology& topo) const noexcept;
            bool                            IsPersistResponseComplete(const Topology& topo) const noexcept;
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


// Implementation of: class XactionDataless
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionDataless<config, conn>::XactionDataless(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::Dataless, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            /* Transactions without CompAck or Persist */
            this->first.flit.req.Opcode() != Opcodes::REQ::CleanInvalid
         && this->first.flit.req.Opcode() != Opcodes::REQ::MakeInvalid
         && this->first.flit.req.Opcode() != Opcodes::REQ::CleanShared
         && this->first.flit.req.Opcode() != Opcodes::REQ::CleanSharedPersist
         && this->first.flit.req.Opcode() != Opcodes::REQ::Evict
            /* Transactions with CompAck */
         && this->first.flit.req.Opcode() != Opcodes::REQ::CleanUnique
         && this->first.flit.req.Opcode() != Opcodes::REQ::MakeUnique
            /* Transactions with Persist */
         && this->first.flit.req.Opcode() != Opcodes::REQ::CleanSharedPersistSep
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
            this->firstDenial = glbl->CHECK_FIELD_MAPPING->Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionDataless<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionDataless<config, conn>> XactionDataless<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionDataless<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsCompResponseComplete(const Topology& topo) const noexcept
    {
        if (
            /* Transactions without CompAck or Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanShared
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersist
         || this->first.flit.req.Opcode() == Opcodes::REQ::Evict
            /* Transactions with CompAck */
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanUnique
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeUnique
        )
        {
            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return true;
        }
        else if (
            /* Transactions with Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersistSep
        )
        {
            if (this->HasRSP({ Opcodes::RSP::Comp }) || this->HasRSP({ Opcodes::RSP::CompPersist }))
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsPersistResponseComplete(const Topology& topo) const noexcept
    {
        if (
            /* Transactions without CompAck or Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanShared
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersist
         || this->first.flit.req.Opcode() == Opcodes::REQ::Evict
            /* Transactions with CompAck */
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanUnique
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeUnique
        )
        {
            return true;
        }
        else if (
            /* Transactions with Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersistSep
        )
        {
            if (this->HasRSP({ Opcodes::RSP::Persist }) || this->HasRSP({ Opcodes::RSP::CompPersist }))
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsAckComplete(const Topology& topo) const noexcept
    {
        if (
            /* Transactions without CompAck or Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanShared
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersist
         || this->first.flit.req.Opcode() == Opcodes::REQ::Evict
        )
        {
            return true;
        }
        else if (
            /* Transactions with CompAck */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanUnique
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeUnique
        )
        {
            return this->HasRSP({ Opcodes::RSP::CompAck });
        }
        else if (
            /* Transactions with Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersistSep
        )
        {
            return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(topo) && IsPersistResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(topo) && IsPersistResponseComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDataless<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else
        {
            if (
                /* Transactions without CompAck or Persist */
                this->first.flit.req.Opcode() == Opcodes::REQ::CleanInvalid
             || this->first.flit.req.Opcode() == Opcodes::REQ::MakeInvalid
             || this->first.flit.req.Opcode() == Opcodes::REQ::CleanShared
             || this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersist
             || this->first.flit.req.Opcode() == Opcodes::REQ::Evict
            )
            {
                if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
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
            }
            else if (
                /* Transactions with CompAck */
                this->first.flit.req.Opcode() == Opcodes::REQ::CleanUnique
             || this->first.flit.req.Opcode() == Opcodes::REQ::MakeUnique
            )
            {
                if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

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
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                {
                    if (!rspFlit.IsFromRequesterToHome(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

                    if (this->HasRSP({ Opcodes::RSP::CompAck }))
                        return XactDenial::DENIED_COMPACK_AFTER_COMPACK;

                    auto lastComp = this->GetLastRSP({ Opcodes::RSP::Comp });
                    if (!lastComp)
                        return XactDenial::DENIED_COMPACK_BEFORE_COMP;

                    if (rspFlit.flit.rsp.TgtID() != lastComp->flit.rsp.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;

                    if (rspFlit.flit.rsp.TxnID() != lastComp->flit.rsp.DBID())
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
            }
            else if (
                /* Transactions with Persist */
                this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersistSep
            )
            {
                if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompPersist)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMPPERSIST_AFTER_COMP;

                    if (this->HasRSP({ Opcodes::RSP::Persist }))
                        return XactDenial::DENIED_COMPPERSIST_AFTER_PERSIST;
                    
                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_COMPPERSIST_AFTER_COMPPERSIST;

                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.PGroupID() != this->first.flit.req.PGroupID())
                        return XactDenial::DENIED_PGROUPID_MISMATCH;
                    
                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;

                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_COMP_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
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
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Persist)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo) && !rspFlit.IsFromSubordinateToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Persist }))
                        return XactDenial::DENIED_PERSIST_AFTER_PERSIST;
                    
                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_PERSIST_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.PGroupID() != this->first.flit.req.PGroupID())
                        return XactDenial::DENIED_PGROUPID_MISMATCH;
                    
                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
            }
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDataless<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


#endif