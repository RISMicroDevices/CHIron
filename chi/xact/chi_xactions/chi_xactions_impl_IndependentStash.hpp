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

        template<FlitConfigurationConcept config>
        class XactionIndependentStash : public Xaction<config> {
        public:
            XactionIndependentStash(const Global<config>&             glbl,
                                    const FiredRequestFlit<config>&   first,
                                    std::shared_ptr<Xaction<config>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>          Clone() const noexcept override;
            std::shared_ptr<XactionIndependentStash<config>>  CloneAsIs() const noexcept;

        public:
            bool                            IsCompResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsStashDoneResponseComplete(const Global<config>& glbl) const noexcept;

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


// Implementation of: class XactionIndependentStash
namespace /*CHI::*/Xact {
 
    template<FlitConfigurationConcept config>
    inline XactionIndependentStash<config>::XactionIndependentStash(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried) noexcept
        : Xaction<config>(XactionType::IndependentStash, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceUnique
         && this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceShared
         && this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceSepUnique
         && this->first.flit.req.Opcode() != Opcodes::REQ::StashOnceSepShared
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
    inline std::shared_ptr<Xaction<config>> XactionIndependentStash<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }
        
    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionIndependentStash<config>> XactionIndependentStash<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionIndependentStash<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsCompResponseComplete(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsStashDoneResponseComplete(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return IsComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(glbl) && IsStashDoneResponseComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionIndependentStash<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetLastRSP({ Opcodes::RSP::Comp, Opcodes::RSP::StashDone, Opcodes::RSP::CompStashDone });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionIndependentStash<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsDMTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsDCTPossible() const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionIndependentStash<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionIndependentStash<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstDATFrom(glbl, XactScope::Subordinate,
            { Opcodes::DAT::CompData, Opcodes::DAT::DataSepResp });
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionIndependentStash<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionIndependentStash<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstDATFrom(glbl, XactScope::Requester,
            { Opcodes::DAT::CompData });
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionIndependentStash<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }
    
    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionIndependentStash<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionIndependentStash<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionIndependentStash<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL_NOT_RSP;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMP_AFTER_COMP;

            if (this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepUnique
             || this->first.flit.req.Opcode() == Opcodes::REQ::StashOnceSepShared)
            {
                if (this->HasRSP({ Opcodes::RSP::CompStashDone }))
                    return XactDenial::DENIED_COMP_AFTER_COMPSTASHDONE;
            }

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
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
                    return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

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
                    return XactDenial::DENIED_RSP_OPCODE;

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

        return XactDenial::DENIED_RSP_OPCODE;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionIndependentStash<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL_DAT;
    }
}


#endif
