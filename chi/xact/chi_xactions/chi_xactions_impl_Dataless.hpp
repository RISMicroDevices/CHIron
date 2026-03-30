//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"
#include <optional>


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

        template<FlitConfigurationConcept config>
        class XactionDataless : public Xaction<config> {
        public:
            XactionDataless(const Global<config>&             glbl,
                            const FiredRequestFlit<config>&   first,
                            std::shared_ptr<Xaction<config>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>  Clone() const noexcept override;
            std::shared_ptr<XactionDataless<config>>  CloneAsIs() const noexcept;

        public:
            bool                            IsCompResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsPersistResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsAckComplete(const Global<config>& glbl) const noexcept;

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


// Implementation of: class XactionDataless
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept config>
    inline XactionDataless<config>::XactionDataless(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried) noexcept
        : Xaction<config>(XactionType::Dataless, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL_NOT_REQ;
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
            this->firstDenial = XactDenial::DENIED_REQ_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(glbl))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl.CHECK_FIELD_MAPPING.enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionDataless<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionDataless<config>> XactionDataless<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionDataless<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsCompResponseComplete(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsPersistResponseComplete(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsAckComplete(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(glbl) && IsPersistResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(glbl) && IsPersistResponseComplete(glbl) && IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return IsAckComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDataless<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstRSP(
            { Opcodes::RSP::Comp, Opcodes::RSP::CompPersist });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionDataless<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDataless<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDataless<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDataless<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDataless<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDataless<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDataless<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDataless<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionDataless<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL_NOT_RSP;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
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
                    if (!rspFlit.IsFromHomeToRequester(glbl))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

                    //
                    if (glbl.CHECK_FIELD_MAPPING.enable)
                    {
                        XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
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
                    if (!rspFlit.IsFromHomeToRequester(glbl))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

                    hasDBID = true;
                    firstDBID = true;

                    //
                    if (glbl.CHECK_FIELD_MAPPING.enable)
                    {
                        XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                {
                    if (!rspFlit.IsFromRequesterToHome(glbl))
                        return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

                    if (this->HasRSP({ Opcodes::RSP::CompAck }))
                        return XactDenial::DENIED_COMPACK_AFTER_COMPACK;

                    auto lastComp = this->GetLastRSP({ Opcodes::RSP::Comp });
                    if (!lastComp)
                        return XactDenial::DENIED_COMPACK_BEFORE_COMP;

                    if (rspFlit.flit.rsp.TgtID() != lastComp->flit.rsp.SrcID())
                        return XactDenial::DENIED_RSP_TGTID_MISMATCHING_RSP;

                    if (rspFlit.flit.rsp.TxnID() != lastComp->flit.rsp.DBID())
                        return XactDenial::DENIED_RSP_TXNID_MISMATCHING_DBID;

                    //
                    if (glbl.CHECK_FIELD_MAPPING.enable)
                    {
                        XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
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
                    if (!rspFlit.IsFromHomeToRequester(glbl))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMPPERSIST_AFTER_COMP;

                    if (this->HasRSP({ Opcodes::RSP::Persist }))
                        return XactDenial::DENIED_COMPPERSIST_AFTER_PERSIST;
                    
                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_COMPPERSIST_AFTER_COMPPERSIST;

                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;
                    
                    if (rspFlit.flit.rsp.PGroupID() != this->first.flit.req.PGroupID())
                        return XactDenial::DENIED_PGROUPID_MISMATCH;
                    
                    //
                    if (glbl.CHECK_FIELD_MAPPING.enable)
                    {
                        XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                {
                    if (!rspFlit.IsFromHomeToRequester(glbl))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;

                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_COMP_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

                    //
                    if (glbl.CHECK_FIELD_MAPPING.enable)
                    {
                        XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Persist)
                {
                    if (!rspFlit.IsFromHomeToRequester(glbl) && !rspFlit.IsFromSubordinateToRequester(glbl))
                        return XactDenial::DENIED_RSP_NOT_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Persist }))
                        return XactDenial::DENIED_PERSIST_AFTER_PERSIST;
                    
                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_PERSIST_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;
                    
                    if (rspFlit.flit.rsp.PGroupID() != this->first.flit.req.PGroupID())
                        return XactDenial::DENIED_PGROUPID_MISMATCH;
                    
                    //
                    if (glbl.CHECK_FIELD_MAPPING.enable)
                    {
                        XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
            }
        }

        return XactDenial::DENIED_RSP_OPCODE;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionDataless<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL_DAT;
    }
}


#endif
