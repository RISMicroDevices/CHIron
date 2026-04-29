//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEDATALESS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEDATALESS))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEDATALESS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEDATALESS
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionHomeDataless : public Xaction<config> {
        public:
            XactionHomeDataless(const Global<config>&             glbl,
                                const FiredRequestFlit<config>&   first,
                                std::shared_ptr<Xaction<config>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeDataless<config>>  CloneAsIs() const noexcept;

        public:
            bool                            IsCompResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsPersistResponseComplete(const Global<config>& glbl) const noexcept;
        
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


// Implementation of: class XactionHomeDataless
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionHomeDataless<config>::XactionHomeDataless(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried
    ) noexcept
        : Xaction<config>(XactionType::HomeDataless, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            /* Transactions without seperate Persist */
            this->first.flit.req.Opcode() != Opcodes::REQ::CleanInvalid
         && this->first.flit.req.Opcode() != Opcodes::REQ::MakeInvalid
         && this->first.flit.req.Opcode() != Opcodes::REQ::CleanShared
         && this->first.flit.req.Opcode() != Opcodes::REQ::CleanSharedPersist
            /* Transactions with seperate Persist */
         && this->first.flit.req.Opcode() != Opcodes::REQ::CleanSharedPersistSep
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Home Dataless transaction");
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(glbl))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
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
    inline std::shared_ptr<Xaction<config>> XactionHomeDataless<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionHomeDataless<config>> XactionHomeDataless<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeDataless<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsCompResponseComplete(const Global<config>& glbl) const noexcept
    {
        if (
            /* Transactions without seperate Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanShared
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersist
        )
        {
            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return true;
        }
        else if (
            /* Transactions with seperate Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersistSep
        )
        {
            if (this->HasRSP({ Opcodes::RSP::Comp }) || this->HasRSP({ Opcodes::RSP::CompPersist }))
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsPersistResponseComplete(const Global<config>& glbl) const noexcept
    {
        if (
            /* Transactions without seperate Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanShared
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersist
        )
        {
            return true;
        }
        else if (
            /* Transactions with seperate Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersistSep
        )
        {
            if (this->HasRSP({ Opcodes::RSP::Persist }) || this->HasRSP({ Opcodes::RSP::CompPersist }))
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(glbl) && IsPersistResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(glbl) && IsPersistResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeDataless<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetLastRSP(
            { Opcodes::RSP::Comp, Opcodes::RSP::Persist, Opcodes::RSP::CompPersist });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionHomeDataless<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeDataless<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeDataless<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeDataless<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeDataless<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeDataless<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeDataless<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeDataless<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeDataless<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_RSP, rspFlit);

        if (!rspFlit.IsRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_RSP, rspFlit);

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (
            /* Transactions without seperate Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::MakeInvalid
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanShared
         || this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersist
        )
        {
            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                if (!rspFlit.IsFromSubordinateToHome(glbl))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

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
            /* Transactions with seperate Persist */
            this->first.flit.req.Opcode() == Opcodes::REQ::CleanSharedPersistSep
        )
        {
            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompPersist)
            {
                if (!rspFlit.IsFromSubordinateToHome(glbl))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

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
                if (!rspFlit.IsFromSubordinateToHome(glbl))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

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
                if (!rspFlit.IsFromSubordinateToHome(glbl))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

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

        return this->ResponseFlitDenied(XactDenial::DENIED_RSP_OPCODE, rspFlit,
            "RSP opcode is not expected for Home Dataless transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeDataless<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_DAT, datFlit,
            "Not expecting any DAT flit for Home Dataless transactions");
    }
}


#endif
