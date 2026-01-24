//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEATOMIC)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEATOMIC))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_HOMEATOMIC
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_HOMEATOMIC
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeAtomic : public Xaction<config, conn> {
        public:
            XactionHomeAtomic(const Global<config, conn>&             glbl,
                              const FiredRequestFlit<config, conn>&   first,
                              std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeAtomic<config, conn>>    CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsHNDataComplete(const Global<config, conn>& glbl) const noexcept;
            bool                            IsSNDataComplete(const Global<config, conn>& glbl) const noexcept;

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

            virtual const FiredResponseFlit<config, conn>*
                                            GetDWTSrcIDSource(const Global<config, conn>& glbl) const noexcept override;
            virtual const FiredResponseFlit<config, conn>*
                                            GetDWTTgtIDSource(const Global<config, conn>& glbl) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class XactionHomeAtomic
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeAtomic<config, conn>::XactionHomeAtomic(
        const Global<config, conn>&             glbl,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried
    ) noexcept
        : Xaction<config, conn>(XactionType::HomeAtomic, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            !Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode())
         && !Opcodes::REQ::AtomicLoad ::Is(this->first.flit.req.Opcode())
         && this->first.flit.req.Opcode() != Opcodes::REQ::AtomicSwap
         && this->first.flit.req.Opcode() != Opcodes::REQ::AtomicCompare
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(glbl))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
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
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeAtomic<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeAtomic<config, conn>> XactionHomeAtomic<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeAtomic<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsResponseComplete(const Global<config, conn>& glbl) const noexcept
    {
        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return true;

            if (this->HasRSP({ Opcodes::RSP::DBIDResp })
             && this->HasRSP({ Opcodes::RSP::Comp }))
                return true;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsHNDataComplete(const Global<config, conn>& glbl) const noexcept
    {
        if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
            return true;

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsSNDataComplete(const Global<config, conn>& glbl) const noexcept
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsSNDataComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsDBIDComplete(const Global<config, conn>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsHNDataComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsComplete(const Global<config, conn>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsHNDataComplete(glbl) && IsSNDataComplete(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept
    {
        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            return false;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            return IsHNDataComplete(glbl);
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeAtomic<config, conn>::GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        return this->GetFirstRSP(
            { Opcodes::RSP::Comp, Opcodes::RSP::DBIDResp, Opcodes::RSP::CompDBIDResp });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::REQ<config, conn>::tgtid_t> XactionHomeAtomic<config, conn>::GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept
    {
        const FiredResponseFlit<config, conn>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeAtomic<config, conn>::GetDMTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeAtomic<config, conn>::GetDMTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeAtomic<config, conn>::GetDCTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeAtomic<config, conn>::GetDCTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeAtomic<config, conn>::GetDWTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* XactionHomeAtomic<config, conn>::GetDWTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeAtomic<config, conn>::NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp)
        {
            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;
                
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;

                if (optDBIDSource->IsRSP())
                {
                    assert(rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp &&
                        "DBID source on RXRSP must be Comp in Home Atomic transactions");

                    if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                        return XactDenial::DENIED_DBID_MISMATCH;
                }
                else
                {
                    assert(rspFlit.flit.dat.Opcode() == Opcodes::DAT::CompData &&
                        "DBID source on RXDAT must be CompData in Home Atomic transactions");

                    if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.dat.DBID())
                        return XactDenial::DENIED_DBID_MISMATCH;
                }
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in AtomicStore Home Atomic transactions");
            
                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
                
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in AtomicStore Home Atomic transactions");

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;
    
                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeAtomic<config, conn>::NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
        {
            if (!datFlit.IsFromHomeToSubordinate(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_HN_TO_SN;

            const FiredResponseFlit<config, conn>* optDBIDSource;

            if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
            {
                optDBIDSource = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp,
                    Opcodes::RSP::CompDBIDResp
                });
            }
            else
            {
                optDBIDSource = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp
                });
            }

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                return XactDenial::DENIED_NCBWRDATA_AFTER_NCBWRDATA;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
        {
            if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;
            
            if (!datFlit.IsFromSubordinateToHome(glbl))
                return XactDenial::DENIED_DAT_NOT_FROM_SN_TO_HN;
            
            if (datFlit.flit.dat.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasDAT({ Opcodes::DAT::CompData }))
                return XactDenial::DENIED_COMPDATA_AFTER_COMPDATA;

            auto optDBID = this->GetDBID();
            if (optDBID)
            {
                if (datFlit.flit.dat.DBID() != *optDBID)
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;
    
            hasDBID = true;

            //
            if (glbl.CHECK_FIELD_MAPPING->enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


#endif
