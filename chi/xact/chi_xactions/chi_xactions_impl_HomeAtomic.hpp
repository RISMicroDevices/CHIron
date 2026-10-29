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

        template<FlitConfigurationConcept config>
        class XactionHomeAtomic : public Xaction<config> {
        public:
            XactionHomeAtomic(const Global<config>&             glbl,
                              const FiredRequestFlit<config>&   first,
                              std::shared_ptr<Xaction<config>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeAtomic<config>>    CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Global<config>& glbl) const noexcept;
            bool                            IsHNDataComplete(const Global<config>& glbl) const noexcept;
            bool                            IsSNDataComplete(const Global<config>& glbl) const noexcept;

            virtual bool                    IsTxnIDComplete(const Global<config>& glbl) const noexcept override;
            virtual bool                    IsDBIDComplete(const Global<config>& glbl) const noexcept override;
            virtual bool                    IsComplete(const Global<config>& glbl) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Global<config>& glbl) const noexcept override;

        protected:
            XactDenialEnum                  CheckREQFields() const noexcept;
            XactDenialEnum                  CheckDATFields(const FiredResponseFlit<config>& datFlit) const noexcept;

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


// Implementation of: class XactionHomeAtomic
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionHomeAtomic<config>::XactionHomeAtomic(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   first,
        std::shared_ptr<Xaction<config>>  retried
    ) noexcept
        : Xaction<config>(XactionType::HomeAtomic, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, this->first);
            return;
        }

        if (
            !Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode())
         && !Opcodes::REQ::AtomicLoad ::Is(this->first.flit.req.Opcode())
         && this->first.flit.req.Opcode() != Opcodes::REQ::AtomicSwap
         && this->first.flit.req.Opcode() != Opcodes::REQ::AtomicCompare
        ) [[unlikely]]
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_OPCODE, this->first,
                "This Opcode is not type of / supported by Home Atomic transaction");
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(glbl))
        {
            this->firstDenial = this->RequestFlitDenied(XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN, this->first);
            return;
        }

        this->firstDenial = CheckREQFields();
        if (this->firstDenial != XactDenial::ACCEPTED)
            return;

        //
        if (glbl.CHECK_FIELD_MAPPING.enable)
        {
            this->firstDenial = glbl.CHECK_FIELD_MAPPING.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeAtomic<config>::CheckREQFields() const noexcept
    {
        const auto& req = this->first.flit.req;
        const bool isCompare = req.Opcode() == Opcodes::REQ::AtomicCompare;

        if (isCompare)
        {
            if (req.Size() < Sizes::B2 || req.Size() > Sizes::B32)
                return XactDenial::DENIED_REQ_FIELD_SIZE;
        }
        else if (req.Size() > Sizes::B8)
            return XactDenial::DENIED_REQ_FIELD_SIZE;

        const uint64_t alignment = uint64_t{1} << (isCompare ? req.Size() - 1 : req.Size());
        if ((static_cast<uint64_t>(req.Addr()) & (alignment - 1)) != 0)
            return XactDenial::DENIED_REQ_FIELD_ADDR;

        if (req.Order() != 0)
            return XactDenial::DENIED_REQ_FIELD_ORDER;

        if (req.SnpAttr() != 0)
            return XactDenial::DENIED_REQ_FIELD_SNPATTR;

        if (req.SnoopMe() != 0)
            return XactDenial::DENIED_REQ_FIELD_SNOOPME;

#ifdef CHI_ISSUE_EB_ENABLE
        if (req.DoDWT() != 0)
            return XactDenial::DENIED_REQ_FIELD_DODWT;
#endif

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeAtomic<config>::CheckDATFields(const FiredResponseFlit<config>& datFlit) const noexcept
    {
        const auto& req = this->first.flit.req;
        const auto& dat = datFlit.flit.dat;
        const bool isCompare = req.Opcode() == Opcodes::REQ::AtomicCompare;
        const bool isWriteData = dat.Opcode() == Opcodes::DAT::NonCopyBackWrData;

        uint64_t dataAddress = req.Addr();
        uint64_t dataSize = uint64_t{1} << req.Size();

        if (isCompare)
        {
            if (isWriteData)
            {
                if ((dataAddress & (dataSize - 1)) != 0)
                    dataAddress -= dataSize / 2;
            }
            else
                dataSize /= 2;
        }

        if (dat.CCID() != ((static_cast<uint64_t>(req.Addr()) >> 4) & 0x3))
            return XactDenial::DENIED_DAT_FIELD_CCID;

        constexpr uint64_t packetSize = config::dataWidth / 8;
        const uint64_t packetOffset = static_cast<uint64_t>(dat.DataID()) * 16;
        const uint64_t windowOffset = dataAddress & 0x3f;

        if (dataSize <= 16)
        {
            if (dat.DataID() != (windowOffset / packetSize) * (packetSize / 16))
                return XactDenial::DENIED_DAT_FIELD_DATAID;
        }
        else
        {
            constexpr uint64_t dataIdsPerPacket = packetSize / 16;
            const uint64_t firstDataID = (windowOffset / packetSize) * dataIdsPerPacket;
            const uint64_t lastDataID = ((windowOffset + dataSize - 1) / packetSize) * dataIdsPerPacket;
            if (dat.DataID() != firstDataID && dat.DataID() != lastDataID)
                return XactDenial::DENIED_DAT_FIELD_DATAID;
        }

        if (windowOffset >= packetOffset + packetSize
         || windowOffset + dataSize <= packetOffset)
            return XactDenial::DENIED_DAT_FIELD_DATAID;

        if (dat.Opcode() == Opcodes::DAT::CompData)
            return XactDenial::ACCEPTED;

        uint64_t expectedBE = 0;

        for (uint64_t byte = 0; byte < packetSize; byte++)
        {
            const uint64_t byteOffset = packetOffset + byte;
            if (byteOffset >= windowOffset && byteOffset < windowOffset + dataSize)
                expectedBE |= uint64_t{1} << byte;
        }

        if (dat.BE() != static_cast<typename Flits::DAT<config>::be_t>(expectedBE))
            return XactDenial::DENIED_DAT_FIELD_BE;

        for (uint64_t byte = 0; byte < packetSize; byte++)
        {
            const uint64_t mask = uint64_t{1} << byte;
            const uint64_t value = dat.Data()[byte / 8] >> ((byte % 8) * 8);
            if (!(expectedBE & mask) && (value & 0xff))
                return XactDenial::DENIED_DAT_FIELD_DATA;
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> XactionHomeAtomic<config>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<XactionHomeAtomic<config>> XactionHomeAtomic<config>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeAtomic<config>>(*this);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsResponseComplete(const Global<config>& glbl) const noexcept
    {
        bool gotDBIDResponse = this->HasRSP({ Opcodes::RSP::DBIDResp });
#ifdef CHI_ISSUE_EB_ENABLE
        gotDBIDResponse = gotDBIDResponse || this->HasRSP({ Opcodes::RSP::DBIDRespOrd });
#endif

        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return true;

            if (gotDBIDResponse && this->HasRSP({ Opcodes::RSP::Comp }))
                return true;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            if (gotDBIDResponse)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsHNDataComplete(const Global<config>& glbl) const noexcept
    {
        const bool useFullDataID = config::dataWidth == 128
            && this->first.flit.req.Opcode() == Opcodes::REQ::AtomicCompare
            && this->first.flit.req.Size() == Sizes::B32;

        std::bitset<4> completeDataIDMask;
        if (useFullDataID)
        {
            completeDataIDMask = details::GetDataIDCompleteMask<config>(Sizes::B64);
            const unsigned int windowDataID = (static_cast<uint64_t>(this->first.flit.req.Addr()) >> 4) & 0x2;
            for (unsigned int dataID = 0; dataID < 4; dataID++)
                if ((dataID & 0x2) != windowDataID)
                    completeDataIDMask.reset(dataID);
        }
        else
            completeDataIDMask = details::GetDataIDCompleteMask<config>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsDAT())
                continue;

            if (iter->flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
            {
                if (useFullDataID)
                    collectedDataID |= details::CollectDataID<config>(Sizes::B64, iter->flit.dat.DataID());
                else
                    collectedDataID |= details::CollectDataID<config>(
                        this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsSNDataComplete(const Global<config>& glbl) const noexcept
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
    inline bool XactionHomeAtomic<config>::IsTxnIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsSNDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsDBIDComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsHNDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsComplete(const Global<config>& glbl) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(glbl) && IsHNDataComplete(glbl) && IsSNDataComplete(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsDBIDOverlappable(const Global<config>& glbl) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeAtomic<config>::GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept
    {
        return this->GetFirstRSP({
            Opcodes::RSP::Comp,
            Opcodes::RSP::DBIDResp,
#ifdef CHI_ISSUE_EB_ENABLE
            Opcodes::RSP::DBIDRespOrd,
#endif
            Opcodes::RSP::CompDBIDResp
        });
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> XactionHomeAtomic<config>::GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept
    {
        const FiredResponseFlit<config>* optSource = GetPrimaryTgtIDSourceNonREQ(glbl);

        if (!optSource)
            return std::nullopt;

        return { optSource->flit.rsp.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionHomeAtomic<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeAtomic<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeAtomic<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeAtomic<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeAtomic<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeAtomic<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionHomeAtomic<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeAtomic<config>::NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_RSP, rspFlit);

        if (!rspFlit.IsRSP())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_RSP, rspFlit);

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd
#endif
        )
        {
            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return this->ResponseFlitDenied(XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN, rspFlit);

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            bool gotDBIDResponse = this->HasRSP({ Opcodes::RSP::DBIDResp });
#ifdef CHI_ISSUE_EB_ENABLE
            gotDBIDResponse = gotDBIDResponse || this->HasRSP({ Opcodes::RSP::DBIDRespOrd });
#endif
            if (gotDBIDResponse)
                return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;

            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;

            const FiredResponseFlit<config>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(!optDBIDSource->IsRSP() && optDBIDSource->flit.dat.Opcode() == Opcodes::DAT::CompData &&
                    "DBID source on RXDAT must be CompData in Home Atomic transactions");

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.dat.DBID())
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
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return this->ResponseFlitDenied(XactDenial::DENIED_RSP_OPCODE, rspFlit,
                    "Comp is only expected for AtomicStore");

            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return this->ResponseFlitDenied(XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN, rspFlit);

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMP_AFTER_COMP;

            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

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
                return this->ResponseFlitDenied(XactDenial::DENIED_RSP_OPCODE, rspFlit,
                    "CompDBIDResp is only expected for AtomicStore");

            if (!rspFlit.IsFromSubordinateToHome(glbl))
                return this->ResponseFlitDenied(XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN, rspFlit);

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_RSP_TGTID_MISMATCHING_REQ;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;

            bool gotDBIDResponse = this->HasRSP({ Opcodes::RSP::DBIDResp });
#ifdef CHI_ISSUE_EB_ENABLE
            gotDBIDResponse = gotDBIDResponse || this->HasRSP({ Opcodes::RSP::DBIDRespOrd });
#endif
            if (gotDBIDResponse)
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

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

        return this->ResponseFlitDenied(XactDenial::DENIED_RSP_OPCODE, rspFlit,
            "This RSP Opcode is not expected for Home Atomic transactions");
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionHomeAtomic<config>::NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(glbl))
            return this->ResponseFlitDenied(XactDenial::DENIED_COMPLETED_DAT, datFlit);

        if (!datFlit.IsDAT())
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DAT, datFlit);

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
        {
            if (!datFlit.IsFromHomeToSubordinate(glbl))
                return this->ResponseFlitDenied(XactDenial::DENIED_DAT_NOT_FROM_HN_TO_SN, datFlit);

            const FiredResponseFlit<config>* optDBIDSource;

            if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
            {
                optDBIDSource = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp,
#ifdef CHI_ISSUE_EB_ENABLE
                    Opcodes::RSP::DBIDRespOrd,
#endif
                    Opcodes::RSP::CompDBIDResp
                });
            }
            else
            {
                optDBIDSource = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
                    , Opcodes::RSP::DBIDRespOrd
#endif
                });
            }

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_RSP;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_DBID;

            XactDenialEnum denial = CheckDATFields(datFlit);
            if (denial != XactDenial::ACCEPTED)
                return denial;

            bool nextDataID;
            if (config::dataWidth == 128
             && this->first.flit.req.Opcode() == Opcodes::REQ::AtomicCompare
             && this->first.flit.req.Size() == Sizes::B32)
                nextDataID = this->NextDataID(Sizes::B64, datFlit, { Opcodes::DAT::NonCopyBackWrData });
            else
                nextDataID = this->NextREQDataID(datFlit, { Opcodes::DAT::NonCopyBackWrData });

            if (!nextDataID)
                return XactDenial::DENIED_DUPLICATED_DATAID;

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
                return this->ResponseFlitDenied(XactDenial::DENIED_DAT_OPCODE, datFlit,
                    "CompData is not expected for AtomicStore");
            
            if (!datFlit.IsFromSubordinateToHome(glbl))
                return this->ResponseFlitDenied(XactDenial::DENIED_DAT_NOT_FROM_SN_TO_HN, datFlit);
            
            if (datFlit.flit.dat.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_DAT_TGTID_MISMATCHING_REQ;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_DAT_TXNID_MISMATCHING_REQ;

            if (this->HasDAT({ Opcodes::DAT::CompData }))
                return XactDenial::DENIED_COMPDATA_AFTER_COMPDATA;

            XactDenialEnum denial = CheckDATFields(datFlit);
            if (denial != XactDenial::ACCEPTED)
                return denial;

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

        return this->ResponseFlitDenied(XactDenial::DENIED_DAT_OPCODE, datFlit,
            "This Opcode is not expected for Home Atomic transactions");
    }
}


#endif
