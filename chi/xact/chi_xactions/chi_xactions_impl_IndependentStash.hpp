//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"

#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_INDEPENDENTSTASH)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_INDEPENDENTSTASH))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_INDEPENDENTSTASH
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_INDEPENDENTSTASH
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionIndependentStash : public Xaction<config, conn> {
        public:
            XactionIndependentStash(const Global<config, conn>&             glbl,
                                    const FiredRequestFlit<config, conn>&   first,
                                    std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>          Clone() const noexcept override;
            std::shared_ptr<XactionIndependentStash<config, conn>>  CloneAsIs() const noexcept;

        public:
            bool                            IsCompResponseComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsStashDoneResponseComplete(const Global<config, conn>& glbl) const noexcept;

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
            virtual bool                    IsDMTPossible() const noexcept override;
            virtual bool                    IsDCTPossible() const noexcept override;
            virtual bool                    IsDWTPossible() const noexcept override;

            virtual const FiredResponseFlit<config, conn>*
                                            GetDMTSrcIDSource(const Global<config, conn>& glbl) const noexcept override;
            virtual const FiredResponseFlit<config, conn>*
                                            GetDMTTgtIDSource(const Global<config, conn>& glbl) const noexcept override;

            virtual const FiredResponseFlit<config, conn>*
                                            GetDCTSrcIDSource(const Global<config, conn>& glbl) const noexcept override;
            virtual const FiredResponseFlit<config, conn>*
                                            GetDCTTgtIDSource(const Global<config, conn>& glbl) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class XactionIndependentStash
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionIndependentStash<config, conn>::XactionIndependentStash(
        const Global<config, conn>&             glbl,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::IndependentStash, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceUnique
         && this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceShared
         && this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceSepUnique
         && this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceSepShared
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(glbl))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl.CHECK_FIELD_MAPPING->enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING->Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionIndependentStash<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }
        
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionIndependentStash<config, conn>> XactionIndependentStash<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionIndependentStash<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsCompResponseComplete(const Global<config, conn>& glbl) const noexcept
    {
        if (this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceUnique
         || this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceShared)
        {
            return this->HasRSP({ Opcodes::RSP::Comp });
        }
        else if (this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepUnique
              || this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepShared)
        {
            return this->HasRSP({ Opcodes::RSP::Comp, Opcodes::RSP::CompStashDone });
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsStashDoneResponseComplete(const Global<config, conn>& glbl) const noexcept
    {
        if (this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceUnique
         || this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceShared)
        {
            return true;
        }
        else if (this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepUnique
              || this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepShared)
        {
            return this->HasRSP({ Opcodes::RSP::StashDone, Opcodes::RSP::CompStashDone }); 
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsDBIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsComplete(const Global<config, conn>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(glbl) && IsStashDoneResponseComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionIndependentStash<config, conn>::GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        return this->GetLastRSP({ Opcodes::RSP::Comp, Opcodes::RSP::StashDone, Opcodes::RSP::CompStashDone });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::REQ<config, conn>::tgtid_t> XactionIndependentStash<config, conn>::GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        const FiredResponseFlit<config, conn>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsDMTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsDCTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionIndependentStash<config, conn>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionIndependentStash<config, conn>::GetDMTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return this->GetFirstDATFrom(glbl, XactScope::Subordinate,
            { Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionIndependentStash<config, conn>::GetDMTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionIndependentStash<config, conn>::GetDCTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return this->GetFirstDATFrom(glbl, XactScope::Requester,
            { Opcodes::DAT::CompData });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionIndependentStash<config, conn>::GetDCTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionIndependentStash<config, conn>::NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMP_AFTER_COMP;

            if (this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepUnique
             || this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepShared)
            {
                if (this->HasRSP({ Opcodes::RSP::CompStashDone }))
                    return XactDenial::DENIED_COMP_AFTER_COMPSTASHDONE;
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
        else if (this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepUnique
              || this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepShared)
        {
            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::StashDone
             || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompStashDone)
            {
                if (!rspFlit.IsFromHomeToRequester(glbl))
                    return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                    return XactDenial::DENIED_TGTID_MISMATCH;

                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_TXNID_MISMATCH;

                if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::StashDone)
                {
                    if (this->HasRSP({ Opcodes::RSP::StashDone }))
                        return XactDenial::DENIED_STASHDONE_AFTER_STASHDONE;

                    if (this->HasRSP({ Opcodes::RSP::CompStashDone }))
                        return XactDenial::DENIED_STASHDONE_AFTER_COMPSTASHDONE;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompStashDone)
                {
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMPSTASHDONE_AFTER_COMP;

                    if (this->HasRSP({ Opcodes::RSP::StashDone }))
                        return XactDenial::DENIED_COMPSTASHDONE_AFTER_STASHDONE;

                    if (this->HasRSP({ Opcodes::RSP::CompStashDone }))
                        return XactDenial::DENIED_COMPSTASHDONE_AFTER_COMPSTASHDONE;
                }
                else
                    return XactDenial::DENIED_OPCODE;

                //
                if (glbl.CHECK_FIELD_MAPPING->enable)
                {
                    XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionIndependentStash<config, conn>::NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


#endif
