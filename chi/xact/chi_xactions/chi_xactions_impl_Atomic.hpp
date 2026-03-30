//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_ATOMIC)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_ATOMIC))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_ATOMIC
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_ATOMIC
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionAtomic : public Xaction<config> {
        public:
            XactionAtomic(const Global<config>&               glbl,
                          const FiredRequestFlit<config>&     first,
                          std::shared_ptr<Xaction<config>>    retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>  Clone() const noexcept override;
            std::shared_ptr<XactionAtomic<config>>    CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsRNDataComplete(const Global<config>& glbl) const noexcept;
            bool                            IsHNDataComplete(const Global<config>& glbl) const noexcept;

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


// Implementation of: class XactionAtomic
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionAtomic<config>::XactionAtomic(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried) noexcept
        : Xaction<config>(XactionType::Atomic, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL_NOT_REQ;
            return;
        }

        if (
            !Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode())
         && !Opcodes::REQ::AtomicLoad ::Is(this->first.flit.req.Opcode())
         && this->first.flit.req.Opcode() != Opcodes::REQ::AtomicSwap
         && this->first.flit.req.Opcode() != Opcodes::REQ::AtomicCompare
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
    inline std::shared_ptr<Xaction<config>> XactionAtomic<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionAtomic<config>> XactionAtomic<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionAtomic<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return true;

            if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd })
             && this->HasRSP({ Opcodes::RSP::Comp }))
                return true;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd }))
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsRNDataComplete(const Global<config>& glbl) const noexcept
    {
        if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
            return true;

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsHNDataComplete(const Global<config>& glbl) const noexcept
    {
        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            return true;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            if (this->HasDAT({ Opcodes::DAT::CompData }))
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsHNDataComplete(glbl);
    }

    template<FlitConfigurationConcept       config>
    inline bool XactionAtomic<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsRNDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsHNDataComplete(glbl) && IsRNDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
    {
        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            return false;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            return IsRNDataComplete(glbl);
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionAtomic<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstRSP(
            { Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd, Opcodes::RSP::Comp, Opcodes::RSP::CompDBIDResp });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionAtomic<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = this->GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionAtomic<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionAtomic<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }
    
    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionAtomic<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionAtomic<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionAtomic<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionAtomic<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionAtomic<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionAtomic<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_RSP;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL_NOT_RSP;
        
        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd
#endif
        )
        {
            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;
                
                if (optDBIDSource->IsRSP())
                {
                    assert(optDBIDSource->flit.rsp.Opcode() == Opcodes::RSP::Comp &&
                        "DBID source on RXRSP must be Comp in Atomic transactions");

                    if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                        return XactDenial::DENIED_RSP_DBID_MISMATCH;
                }
                else
                {
                    assert(optDBIDSource->flit.dat.Opcode() == Opcodes::DAT::CompData &&
                        "DBID source on RXDAT must be CompData in Atomic transactions");

                    if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.dat.DBID())
                        return XactDenial::DENIED_RSP_DBID_MISMATCH;
                }
            }
            else
                firstDBID = true;

            hasDBID = true;
            
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
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_RSP_OPCODE;

            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in AtomicStore Atomic transactions");

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_RSP_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_RSP_OPCODE;

            if (!rspFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in AtomicStore Atomic transactions");

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;
    
                if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_RSP_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

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
    inline XactDenialEnum XactionAtomic<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED_DAT;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL_NOT_DAT;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
        {
            if (!datFlit.IsFromRequesterToHome(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

            const FiredResponseFlit<config>* optDBIDSource;

            if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
            {
                optDBIDSource = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp,
                    Opcodes::RSP::DBIDRespOrd,
                    Opcodes::RSP::CompDBIDResp
                });
            }
            else
            {
                optDBIDSource = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp,
                    Opcodes::RSP::DBIDRespOrd
                });
            }

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_RSP;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_DBID;

            if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                return XactDenial::DENIED_NCBWRDATA_AFTER_NCBWRDATA;

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
        {
            if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_DAT_OPCODE;

            if (!datFlit.IsFromHomeToRequester(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_HN_TO_RN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_REQ;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_REQ;

            if (this->HasDAT({ Opcodes::DAT::CompData }))
                return XactDenial::DENIED_COMPDATA_AFTER_COMPDATA;

            auto optDBID = this->GetDBID();
            if (optDBID)
            {
                if (datFlit.flit.dat.DBID() != *optDBID)
                    return XactDenial::DENIED_DAT_DBID_MISMATCH;
            }
            else
                firstDBID = true;
    
            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_DAT_OPCODE;
    }
}


#endif
