//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__BASE)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__BASE))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__BASE
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__BASE
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class Local {
            
        };
    }

    namespace Xact {

        enum class XactionType {
            AllocatingRead  = 0,
            NonAllocatingRead,
            ImmediateWrite,
            WriteZero,
            CopyBackWrite,
            Atomic,
            IndependentStash,
            Dataless,
            HomeRead,
            HomeWrite,
            HomeWriteZero,
            HomeDataless,
            HomeAtomic,
            HomeSnoop,
            ForwardSnoop,
            DVMSnoop
        };


        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class Xaction {
        public:
            // *NOTICE: Subsequence Key contains several critical attributes and fields
            //          of fired response flits to speed up Xaction query operations,
            //          because full flit data might not fit into a single cache line and
            //          streaming on full flit data wastes cache lines and memory bandwidth.
            struct SubsequenceKey {
                XactDenialEnum  denial;
                bool            isRSP;
                union   {
                    Flits::RSP<config, conn>::opcode_t rsp;
                    Flits::DAT<config, conn>::opcode_t dat;
                }               opcode;
                bool            hasDBID;

                SubsequenceKey() noexcept;
                SubsequenceKey(XactDenialEnum, Flits::RSP<config, conn>::opcode_t, bool hasDBID) noexcept;
                SubsequenceKey(XactDenialEnum, Flits::DAT<config, conn>::opcode_t, bool hasDBID) noexcept;

                bool        IsRSP() const noexcept;
                bool        IsDAT() const noexcept;

                bool        IsAccepted() const noexcept;
                bool        IsDenied() const noexcept;

                bool        HasDBID() const noexcept;
            };

        private:
            const XactionType                               type;

        protected:
            FiredRequestFlit<config, conn>                  first;
            XactDenialEnum                                  firstDenial;
            std::vector<FiredResponseFlit<config, conn>>    subsequence;
            std::vector<SubsequenceKey>                     subsequenceKeys;

            bool                                            resent;
            std::shared_ptr<Xaction<config, conn>>          resentXaction;
            FiredResponseFlit<config, conn>                 sourcePCredit;
            XactDenialEnum                                  resentDenial;

            bool                                            retried;
            std::shared_ptr<Xaction<config, conn>>          retriedXaction;

        public:
            Companion                                       companion;

        public:
            Xaction(XactionType                             type, 
                    const FiredRequestFlit<config, conn>&   first) noexcept;

            Xaction(XactionType                             type, 
                    const FiredRequestFlit<config, conn>&   first,
                    std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

            XactionType                             GetType() const noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>          Clone() const noexcept = 0;
            template<class T> std::shared_ptr<T>                    Clone() const noexcept;

        public:
            const FiredRequestFlit<config, conn>&   GetFirst() const noexcept;
            XactDenialEnum                          GetFirstDenial() const noexcept;
            const std::vector<FiredResponseFlit<config, conn>>&   
                                                    GetSubsequence() const noexcept;
            XactDenialEnum                          GetSubsequentDenial(size_t index) const noexcept;

            XactDenialEnum                          GetLastDenial() const noexcept;
            void                                    SetLastDenial(XactDenialEnum) noexcept;
            
            size_t                                  Revert(size_t count = 1) noexcept;

        public:
            bool                                    IsSecondTry() const noexcept;
            std::shared_ptr<Xaction<config, conn>>  GetFirstTry() const noexcept;

        public:
            bool                                    HasRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            bool                                    HasDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstRSP() const noexcept;
            const FiredResponseFlit<config, conn>*  GetFirstDAT() const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastRSP() const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDAT() const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirst(ChannelTypeEnum channel) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLast(ChannelTypeEnum channel) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetFirstDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirst(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLast(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstRSPFrom(const Global<config, conn>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetFirstDATFrom(const Global<config, conn>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastRSPFrom(const Global<config, conn>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDATFrom(const Global<config, conn>&, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstFrom(const Global<config, conn>&, ChannelTypeEnum, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastFrom(const Global<config, conn>&, ChannelTypeEnum, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstRSPFrom(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetFirstDATFrom(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastRSPFrom(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDATFrom(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstFrom(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastFrom(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstRSPTo(const Global<config, conn>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetFirstDATTo(const Global<config, conn>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastRSPTo(const Global<config, conn>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDATTo(const Global<config, conn>&, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstTo(const Global<config, conn>&, ChannelTypeEnum, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastTo(const Global<config, conn>&, ChannelTypeEnum, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstRSPTo(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetFirstDATTo(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastRSPTo(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDATTo(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            const FiredResponseFlit<config, conn>*  GetFirstTo(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastTo(const Global<config, conn>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept;

        public:
            bool                                    GotDBID() const noexcept;
            std::optional<typename Flits::RSP<config, conn>::dbid_t>
                                                    GetDBID() const noexcept;
            const FiredResponseFlit<config, conn>*  GetDBIDSource() const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDBIDSourceRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDBIDSourceDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            bool                                    GotPrimaryTgtID(const Global<config, conn>& glbl) const noexcept;
            std::optional<typename Flits::REQ<config, conn>::tgtid_t>
                                                    GetPrimaryTgtID(const Global<config, conn>& glbl) const noexcept;
            const FiredFlit<config, conn>*          GetPrimaryTgtIDSource(const Global<config, conn>& glbl) const noexcept;
            bool                                    IsPrimaryTgtIDSourceREQ(const Global<config, conn>& glbl) const noexcept;

            bool                                    GotDMTSrcID(const Global<config, conn>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config, conn>::srcid_t>
                                                    GetDMTSrcID(const Global<config, conn>& glbl) const noexcept;
            virtual const FiredResponseFlit<config, conn>*
                                                    GetDMTSrcIDSource(const Global<config, conn>& glbl) const noexcept;

            bool                                    GotDMTTgtID(const Global<config, conn>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config, conn>::tgtid_t>
                                                    GetDMTTgtID(const Global<config, conn>& glbl) const noexcept;
            virtual const FiredResponseFlit<config, conn>*
                                                    GetDMTTgtIDSource(const Global<config, conn>& glbl) const noexcept;

            bool                                    GotDCTSrcID(const Global<config, conn>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config, conn>::srcid_t>
                                                    GetDCTSrcID(const Global<config, conn>& glbl) const noexcept;
            virtual const FiredResponseFlit<config, conn>*
                                                    GetDCTSrcIDSource(const Global<config, conn>& glbl) const noexcept;

            bool                                    GotDCTTgtID(const Global<config, conn>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config, conn>::tgtid_t>
                                                    GetDCTTgtID(const Global<config, conn>& glbl) const noexcept;
            virtual const FiredResponseFlit<config, conn>*
                                                    GetDCTTgtIDSource(const Global<config, conn>& glbl) const noexcept;

            bool                                    GotDWTSrcID(const Global<config, conn>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config, conn>::srcid_t>
                                                    GetDWTSrcID(const Global<config, conn>& glbl) const noexcept;
            virtual const FiredResponseFlit<config, conn>*
                                                    GetDWTSrcIDSource(const Global<config, conn>& glbl) const noexcept;

            bool                                    GotDWTTgtID(const Global<config, conn>& glbl) const noexcept;
            std::optional<typename Flits::RSP<config, conn>::srcid_t>
                                                    GetDWTTgtID(const Global<config, conn>& glbl) const noexcept;
            virtual const FiredResponseFlit<config, conn>*  
                                                    GetDWTTgtIDSource(const Global<config, conn>& glbl) const noexcept;

            bool                                    GotRetryAck() const noexcept;
            const FiredResponseFlit<config, conn>*  GetRetryAck() const noexcept;

            bool                                    IsResent() const noexcept;
            std::shared_ptr<Xaction<config, conn>>  GetResentXaction() const noexcept;
            const FiredResponseFlit<config, conn>&  GetPCreditSource() const noexcept;

            virtual XactDenialEnum                  Next(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& flit, bool& hasDBID, bool& firstDBID) noexcept;
            virtual XactDenialEnum                  NextRSP(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept;
            virtual XactDenialEnum                  NextDAT(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept;

            virtual XactDenialEnum                  Resend(const Global<config, conn>& glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept;
            virtual bool                            RevertResent() noexcept;

            virtual bool                            IsTxnIDComplete(const Global<config, conn>& glbl) const noexcept = 0;
            virtual bool                            IsDBIDComplete(const Global<config, conn>& glbl) const noexcept = 0;
            virtual bool                            IsComplete(const Global<config, conn>& glbl) const noexcept = 0;

            // *NOTICE: Responses with both valid TxnID and DBID like Comp could be out-of-order on interconnect
            //          and came after DBID grant (e.g. DBIDResp, CompDBIDResp).
            virtual bool                            IsDBIDOverlappable(const Global<config, conn>& glbl) const noexcept = 0;

        protected:
            virtual const FiredResponseFlit<config, conn>*  
                                                    GetPrimaryTgtIDSourceNonREQ(const Global<config, conn>& glbl) const noexcept = 0;
            virtual std::optional<typename Flits::REQ<config, conn>::tgtid_t>
                                                    GetPrimaryTgtIDNonREQ(const Global<config, conn>& glbl) const noexcept = 0;

        public:
            virtual bool                            IsDMTPossible() const noexcept;
            virtual bool                            IsDCTPossible() const noexcept;
            virtual bool                            IsDWTPossible() const noexcept;

        protected:
            virtual XactDenialEnum                  NextRSPNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;
            virtual XactDenialEnum                  NextDATNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;

        protected:
            virtual XactDenialEnum                  NextRetryAckNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit) noexcept;
        
            virtual XactDenialEnum                  ResendNoRecord(const Global<config, conn>& glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept;
        
            virtual bool                            NextDataID(Flits::REQ<config, conn>::ssize_t, const FiredResponseFlit<config, conn>& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) noexcept;
            virtual bool                            NextREQDataID(const FiredResponseFlit<config, conn>& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> = {}) noexcept;
            virtual bool                            NextSNPDataID(const FiredResponseFlit<config, conn>& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> = {}) noexcept;
        };
    }

    namespace Xact::details {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        inline static std::bitset<4> CollectDataID(
            typename Flits::REQ<config, conn>::ssize_t  reqSize,
            typename Flits::DAT<config, conn>::dataid_t dataID) noexcept;

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        inline static std::bitset<4> CollectDataID(
            typename Flits::REQ<config, conn>::ssize_t  reqSize,
            const std::vector<FiredResponseFlit<config, conn>>&) noexcept;

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        inline static std::bitset<4> GetDataIDCompleteMask(
            typename Flits::REQ<config, conn>::ssize_t  reqSize) noexcept;
    }
/*
}
*/


// Implementation of: details
namespace /*CHI::*/Xact::details {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline static std::bitset<4> CollectDataID(
        typename Flits::REQ<config, conn>::ssize_t  reqSize,
        typename Flits::DAT<config, conn>::dataid_t dataID) noexcept
    {
        std::bitset<4> collectedDataID;
        unsigned int requestSize = (1 << reqSize) << 3;

        if constexpr (config::dataWidth == 512)
        {
            collectedDataID.set(0);
        }
        else if constexpr (config::dataWidth == 256)
        {
            if (requestSize <= 256)
                collectedDataID.set(0);
            else // 512
                collectedDataID.set(dataID & 0x02);
        }
        else if constexpr (config::dataWidth == 128)
        {
            if (requestSize <= 128)
                collectedDataID.set(0);
            else if (requestSize <= 256)
                collectedDataID.set(dataID & 0x02);
            else // 512
                collectedDataID.set(dataID);
        }

        return collectedDataID;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline static std::bitset<4> CollectDataID(
        typename Flits::REQ<config, conn>::ssize_t  reqSize,
        const std::vector<FiredResponseFlit<config, conn>>& vec,
        std::function<bool(size_t, const FiredResponseFlit<config, conn>&)> validFunc = [](auto, auto) -> bool { return true; }) noexcept
    {
        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (const FiredResponseFlit<config, conn>& flit : vec)
        {
            if (flit.IsDAT() && validFunc(index, flit))
            {
                collectedDataID |= CollectDataID<config, conn>(
                    reqSize, flit.flit.dat.DataID());
            }
            
            index++;
        }

        return collectedDataID;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline static std::bitset<4> GetDataIDCompleteMask(
        typename Flits::REQ<config, conn>::ssize_t  reqSize) noexcept
    {
        std::bitset<4> collectedDataID;
        unsigned int requestSize = (1 << reqSize) << 3;

        if constexpr (config::dataWidth == 512)
        {
            collectedDataID.set(0);
        }
        
        if constexpr (config::dataWidth == 256)
        {
            if (requestSize <= 256)
            {
                collectedDataID.set(0);
            }
            else // 512
            {
                collectedDataID.set(0);
                collectedDataID.set(2);
            }
        }
        
        if constexpr (config::dataWidth == 128)
        {
            if (requestSize <= 128)
            {
                collectedDataID.set(0);
            }
            else if (requestSize <= 256)
            {
                collectedDataID.set(0);
                collectedDataID.set(2);
            }
            else // 512
            {
                collectedDataID.set(0);
                collectedDataID.set(1);
                collectedDataID.set(2);
                collectedDataID.set(3);
            }
        }

        return collectedDataID;
    }
}


// Implementation of: class Xaction::SubsequenceKey
namespace /*CHI::*/Xact {
    /*
    ...
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Xaction<config, conn>::SubsequenceKey::SubsequenceKey() noexcept
    {
        this->denial        = XactDenial::NOT_INITIALIZED;
        this->isRSP         = false;
        this->hasDBID       = false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Xaction<config, conn>::SubsequenceKey::SubsequenceKey(
        XactDenialEnum                      denial,
        Flits::RSP<config, conn>::opcode_t  opcode,
        bool                                hasDBID) noexcept
    {
        this->denial        = denial;
        this->isRSP         = true;
        this->opcode.rsp    = opcode;
        this->hasDBID       = hasDBID;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Xaction<config, conn>::SubsequenceKey::SubsequenceKey(
        XactDenialEnum                      denial,
        Flits::DAT<config, conn>::opcode_t  opcode,
        bool                                hasDBID) noexcept
    {
        this->denial        = denial;
        this->isRSP         = false;
        this->opcode.dat    = opcode;
        this->hasDBID       = hasDBID;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::SubsequenceKey::IsRSP() const noexcept
    {
        return isRSP;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::SubsequenceKey::IsDAT() const noexcept
    {
        return !IsRSP();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::SubsequenceKey::IsAccepted() const noexcept
    {
        return denial == XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::SubsequenceKey::IsDenied() const noexcept
    {
        return !IsAccepted();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::SubsequenceKey::HasDBID() const noexcept
    {
        return hasDBID;
    }
}


// Implementation of: class Xaction
namespace /*CHI::*/Xact {
    /*
    FiredRequestFlit<config, conn>                  first;
    XactDenialEnum                                  firstDenial;
    std::vector<FiredResponseFlit<config, conn>>    subsequence;
    std::vector<SubsequenceKey>                     subsequenceKeys;

    bool                                            resent;
    std::shared_ptr<Xaction<config, conn>>          resentXaction;
    FiredResponseFlit<config, conn>                 sourcePCredit;
    XactDenialEnum                                  resentDenial;

    bool                                            retried;
    std::shared_ptr<Xaction<config, conn>>          retriedXaction;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Xaction<config, conn>::Xaction(XactionType                               type, 
                                          const FiredRequestFlit<config, conn>&     first) noexcept
        : type              (type)
        , first             (first)
        , firstDenial       (XactDenial::NOT_INITIALIZED)
        , subsequence       ()
        , subsequenceKeys   ()
        , resent            (false)
        , resentXaction     (nullptr)
        , sourcePCredit     ()
        , resentDenial      (XactDenial::NOT_INITIALIZED)
        , retried           (false)
        , retriedXaction    (nullptr)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Xaction<config, conn>::Xaction(XactionType                               type, 
                                          const FiredRequestFlit<config, conn>&     first,
                                          std::shared_ptr<Xaction<config, conn>>    retried) noexcept
        : type              (type)
        , first             (first)
        , firstDenial       (XactDenial::NOT_INITIALIZED)
        , subsequence       ()
        , subsequenceKeys   ()
        , resent            (false)
        , resentXaction     (nullptr)
        , sourcePCredit     ()
        , resentDenial      (XactDenial::NOT_INITIALIZED)
        , retried           (retried)
        , retriedXaction    (retried)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionType Xaction<config, conn>::GetType() const noexcept
    {
        return type;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    template<class T>
    inline std::shared_ptr<T> Xaction<config, conn>::Clone() const noexcept
    {
        return std::make_shared<T>(*static_cast<T*>(this));
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredRequestFlit<config, conn>& Xaction<config, conn>::GetFirst() const noexcept
    {
        return first;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::GetFirstDenial() const noexcept
    {
        return firstDenial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const std::vector<FiredResponseFlit<config, conn>>& Xaction<config, conn>::GetSubsequence() const noexcept
    {
        return subsequence;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::GetSubsequentDenial(size_t index) const noexcept
    {
        return subsequenceKeys[index].denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::GetLastDenial() const noexcept
    {
        if (subsequenceKeys.empty())
            return firstDenial;

        return subsequenceKeys.back().denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Xaction<config, conn>::SetLastDenial(XactDenialEnum denial) noexcept
    {
        if (subsequenceKeys.empty())
            this->firstDenial = denial;

        subsequenceKeys.back().denial = denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline size_t Xaction<config, conn>::Revert(size_t count) noexcept
    {
        size_t rCount = 0;
        while (count-- > 0 && !subsequence.empty())
        {
            subsequence.pop_back();
            subsequenceKeys.pop_back();

            rCount++;
        }

        return rCount;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::IsSecondTry() const noexcept
    {
        return retried;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> Xaction<config, conn>::GetFirstTry() const noexcept
    {
        return retriedXaction;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::HasRSP(
        std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (opcodes.size() == 0)
                return true;

            for (auto opcode : opcodes)
                if (iter->opcode.rsp == opcode)
                    return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::HasDAT(
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsDAT())
                continue;

            if (opcodes.size() == 0)
                return true;

            for (auto opcode : opcodes)
                if (iter->opcode.dat == opcode)
                    return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstRSP() const noexcept
    {
        return GetFirst(ChannelType::RSP);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstDAT() const noexcept
    {
        return GetFirst(ChannelType::DAT);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastRSP() const noexcept
    {
        return GetLast(ChannelType::RSP);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDAT() const noexcept
    {
        return GetLast(ChannelType::DAT);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirst(ChannelTypeEnum channel) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (iter->GetChannelType() != channel)
                continue;

            return &(subsequence[index]);
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLast(ChannelTypeEnum channel) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (iter->GetChannelType() != channel)
                continue;

            return &(subsequence[index]);
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstRSP(
        std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetFirst(opcodes, {});
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstDAT(
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetFirst({}, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastRSP(
        std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetLast(opcodes, {});
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDAT(
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetLast({}, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirst(
        std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (iter->IsRSP())
            {
                for (auto opcode : rspOpcodes)
                    if (iter->opcode.rsp == opcode)
                        return &(subsequence[index]);
            }
            else
            {
                for (auto opcode : datOpcodes)
                    if (iter->opcode.dat == opcode)
                        return &(subsequence[index]);
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLast(
        std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (iter->IsRSP())
            {
                for (auto opcode : rspOpcodes)
                    if (iter->opcode.rsp == opcode)
                        return &(subsequence[index]);
            }
            else
            {
                for (auto opcode : datOpcodes)
                    if (iter->opcode.dat == opcode)
                        return &(subsequence[index]);
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstRSPFrom(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstFrom(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstDATFrom(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstFrom(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastRSPFrom(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastFrom(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDATFrom(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastFrom(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstFrom(const Global<config, conn>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (iter->GetChannelType() != channel)
                continue;

            if (!iter->IsFrom(glbl, scope))
                continue;

            return &(subsequence[index]);
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastFrom(const Global<config, conn>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (iter->GetChannelType() != channel)
                continue;

            if (!iter->IsFrom(glbl, scope))
                continue;

            return &(subsequence[index]);
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstRSPFrom(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetFirstFrom(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstDATFrom(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetFirstFrom(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastRSPFrom(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetLastFrom(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDATFrom(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetLastFrom(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstFrom(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsFrom(glbl, scope))
                continue;

            if (iter->IsRSP())
            {
                for (auto opcode : rspOpcodes)
                    if (iter->opcode.rsp == opcode)
                        return &(subsequence[index]);
            }
            else
            {
                for (auto opcode : datOpcodes)
                    if (iter->opcode.dat == opcode)
                        return &(subsequence[index]);
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastFrom(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsFrom(glbl, scope))
                continue;

            if (iter->IsRSP())
            {
                for (auto opcode : rspOpcodes)
                    if (iter->opcode.rsp == opcode)
                        return &(subsequence[index]);
            }
            else
            {
                for (auto opcode : datOpcodes)
                    if (iter->opcode.dat == opcode)
                        return &(subsequence[index]);
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstRSPTo(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstTo(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstDATTo(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstTo(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastRSPTo(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastTo(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDATTo(const Global<config, conn>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastTo(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstTo(const Global<config, conn>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (iter->GetChannelType() != channel)
                continue;

            if (!iter->IsTo(glbl, scope))
                continue;

            return &(subsequence[index]);
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastTo(const Global<config, conn>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (iter->GetChannelType() != channel)
                continue;

            if (!iter->IsTo(glbl, scope))
                continue;

            return &(subsequence[index]);
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstRSPTo(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetFirstTo(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstDATTo(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetFirstTo(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastRSPTo(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetLastTo(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDATTo(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        return GetLastTo(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstTo(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsTo(glbl, scope))
                continue;

            if (iter->IsRSP())
            {
                for (auto opcode : rspOpcodes)
                    if (iter->opcode.rsp == opcode)
                        return &(subsequence[index]);
            }
            else
            {
                for (auto opcode : datOpcodes)
                    if (iter->opcode.dat == opcode)
                        return &(subsequence[index]);
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastTo(const Global<config, conn>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> datOpcodes) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsTo(glbl, scope))
                continue;

            if (iter->IsRSP())
            {
                for (auto opcode : rspOpcodes)
                    if (iter->opcode.rsp == opcode)
                        return &(subsequence[index]);
            }
            else
            {
                for (auto opcode : datOpcodes)
                    if (iter->opcode.dat == opcode)
                        return &(subsequence[index]);
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotDBID() const noexcept
    {
        return GetDBIDSource() != nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::RSP<config, conn>::dbid_t> Xaction<config, conn>::GetDBID() const noexcept
    {
        const FiredResponseFlit<config, conn>* dbidSource = GetDBIDSource();

        if (dbidSource != nullptr)
        {
            if (dbidSource->IsRSP())
                return { dbidSource->flit.rsp.DBID() };
            else
                return { static_cast<Flits::RSP<config, conn>::dbid_t>(dbidSource->flit.dat.DBID()) };
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetDBIDSource() const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (iter->HasDBID())
                return &(subsequence[index]);
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDBIDSourceRSP(
        std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (opcodes.size() == 0)
            {
                if (iter->HasDBID())
                    return &(subsequence[index]);
            }
            else
            {
                for (auto opcode : opcodes)
                    if (iter->opcode.rsp == opcode)
                        if (iter->HasDBID())
                            return &(subsequence[index]);
            }
        }

        return nullptr;
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDBIDSourceDAT(
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        size_t index = subsequenceKeys.size() - 1;
        for (auto iter = subsequenceKeys.rbegin(); iter != subsequenceKeys.rend(); iter++, index--)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsDAT())
                continue;

            if (opcodes.size() == 0)
            {
                if (iter->HasDBID())
                    return subsequence[index];
            }
            else
            {
                for (auto opcode : opcodes)
                    if (iter->opcode.dat == opcode)
                        if (iter->HasDBID())
                            return subsequence[index];
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotPrimaryTgtID(const Global<config, conn>& glbl) const noexcept
    {
        return GetPrimaryTgtID(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::REQ<config, conn>::tgtid_t> Xaction<config, conn>::GetPrimaryTgtID(const Global<config, conn>& glbl) const noexcept
    {
        if (IsPrimaryTgtIDSourceREQ(glbl))
            return { this->first.flit.req.TgtID() };
        else
            return GetPrimaryTgtIDNonREQ(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredFlit<config, conn>* Xaction<config, conn>::GetPrimaryTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        if (IsPrimaryTgtIDSourceREQ(glbl))
            return &(this->first);
        else
            return GetPrimaryTgtIDSourceNonREQ(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::IsPrimaryTgtIDSourceREQ(const Global<config, conn>& glbl) const noexcept
    {
        if (!this->first.IsREQ())
            return false;

        if (!glbl.SAM_SCOPE->enable)
            return true;

        switch (glbl.SAM_SCOPE->Get(this->first.flit.req.SrcID())->value)
        {
            case SAMScope::AfterSAM:
                return true;

            case SAMScope::BeforeSAM:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotDMTSrcID(const Global<config, conn>& glbl) const noexcept
    {
        return GetDMTSrcID(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::DAT<config, conn>::srcid_t> Xaction<config, conn>::GetDMTSrcID(const Global<config, conn>& glbl) const noexcept
    {
        if (!IsDMTPossible())
            return std::nullopt;

        const FiredResponseFlit<config, conn>* optSource = GetDMTSrcIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DMT SrcID should come from DAT channel only");

        return { optSource->flit.dat.SrcID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetDMTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotDMTTgtID(const Global<config, conn>& glbl) const noexcept
    {
        return GetDMTTgtID(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::DAT<config, conn>::tgtid_t> Xaction<config, conn>::GetDMTTgtID(const Global<config, conn>& glbl) const noexcept
    {
        if (!IsDMTPossible())
            return std::nullopt;

        const FiredResponseFlit<config, conn>* optSource = GetDMTTgtIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DMT TgtID should come from DAT channel only");

        return { optSource->flit.dat.TgtID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetDMTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotDCTSrcID(const Global<config, conn>& glbl) const noexcept
    {
        return GetDCTSrcID(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::DAT<config, conn>::srcid_t> Xaction<config, conn>::GetDCTSrcID(const Global<config, conn>& glbl) const noexcept
    {
        if (!IsDCTPossible())
            return std::nullopt;

        const FiredResponseFlit<config, conn>* optSource = GetDCTSrcIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DCT SrcID should come from DAT channel only");

        return { optSource->flit.dat.SrcID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetDCTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotDCTTgtID(const Global<config, conn>& glbl) const noexcept
    {
        return GetDCTTgtID(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::DAT<config, conn>::tgtid_t> Xaction<config, conn>::GetDCTTgtID(const Global<config, conn>& glbl) const noexcept
    {
        if (!IsDCTPossible())
            return std::nullopt;

        const FiredResponseFlit<config, conn>* optSource = GetDCTTgtIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DCT TgtID should come from DAT channel only");

        return { optSource->flit.dat.TgtID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetDCTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotDWTSrcID(const Global<config, conn>& glbl) const noexcept
    {
        return GetDWTSrcID(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::DAT<config, conn>::srcid_t> Xaction<config, conn>::GetDWTSrcID(const Global<config, conn>& glbl) const noexcept
    {
        if (!IsDWTPossible())
            return std::nullopt;

        const FiredResponseFlit<config, conn>* optSource = GetDWTSrcIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsRSP() && "DWT SrcID should come from RSP channel only");

        return { optSource->flit.rsp.TgtID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetDWTSrcIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotDWTTgtID(const Global<config, conn>& glbl) const noexcept
    {
        return GetDWTTgtID(glbl);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::optional<typename Flits::RSP<config, conn>::srcid_t> Xaction<config, conn>::GetDWTTgtID(const Global<config, conn>& glbl) const noexcept
    {
        if (!IsDWTPossible())
            return std::nullopt;

        const FiredResponseFlit<config, conn>* optSource = GetDWTTgtIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsRSP() && "DWT TgtID should come from RSP channel only");

        return { optSource->flit.rsp.TgtID() };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetDWTTgtIDSource(const Global<config, conn>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::GotRetryAck() const noexcept
    {
        return GetRetryAck() != nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetRetryAck() const noexcept
    {
        return GetLastRSP({ Opcodes::RSP::RetryAck });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::IsResent() const noexcept
    {
        return resent;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> Xaction<config, conn>::GetResentXaction() const noexcept
    {
        return resentXaction;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>& Xaction<config, conn>::GetPCreditSource() const noexcept
    {
        return sourcePCredit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::Next(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& flit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return flit.IsRSP() ? NextRSP(glbl, flit, hasDBID, firstDBID) : NextDAT(glbl, flit, hasDBID, firstDBID);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::NextRSP(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!rspFlit.IsRSP()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL;

        XactDenialEnum denial = NextRSPNoRecord(glbl, rspFlit, hasDBID, firstDBID);

        subsequence.push_back(rspFlit);
        subsequenceKeys.emplace_back(denial, rspFlit.flit.rsp.Opcode(), hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::NextDAT(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!datFlit.IsDAT()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL;

        XactDenialEnum denial = NextDATNoRecord(glbl, datFlit, hasDBID, firstDBID);

        subsequence.push_back(datFlit);
        subsequenceKeys.emplace_back(denial, datFlit.flit.dat.Opcode(), hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::Resend(const Global<config, conn>& glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept
    {
        XactDenialEnum denial = ResendNoRecord(glbl, pCrdFlit, xaction);

        resent = true;
        resentXaction = xaction;
        sourcePCredit = pCrdFlit;
        resentDenial = denial;

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::RevertResent() noexcept
    {
        if (resent)
        {
            resent = false;
            return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::NextRetryAckNoRecord(const Global<config, conn>& glbl, const FiredResponseFlit<config, conn>& rspFlit) noexcept
    {
        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() != Opcodes::RSP::RetryAck)
            return XactDenial::DENIED_OPCODE;

        if (!rspFlit.IsFromHomeToRequester(glbl) && !rspFlit.IsFromSubordinateToHome(glbl))
            return XactDenial::DENIED_RSP_RETRYACK_ROUTE;

        if (!this->subsequence.empty())
            return XactDenial::DENIED_RETRY_ON_ACTIVE_PROGRESS;

        if (!this->first.flit.req.AllowRetry())
            return XactDenial::DENIED_RETRY_NO_ALLOWRETRY;

        if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
            return XactDenial::DENIED_TGTID_MISMATCH;

        if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
            return XactDenial::DENIED_TXNID_MISMATCH;

        //
        if (glbl.CHECK_FIELD_MAPPING->enable)
        {
            XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING->Check(rspFlit.flit.rsp);
            if (denial != XactDenial::ACCEPTED)
                return denial;
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::ResendNoRecord(const Global<config, conn>& glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept
    {
        if (xaction->GetType() != type)
            return XactDenial::DENIED_RETRY_DIFF_XACT_TYPE;

        if (!pCrdFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (!xaction->GetFirst().IsREQ())
            return XactDenial::DENIED_CHANNEL;

        auto retryAck = GetRetryAck();
        if (!retryAck)
            return XactDenial::DENIED_PCRD_NO_RETRY;

        if (pCrdFlit.flit.rsp.PCrdType() != retryAck->flit.rsp.PCrdType())
            return XactDenial::DENIED_PCRD_TYPE_MISMATCH;

        // Check Fields Difference of retried request
        if (glbl.CHECK_FIELD_MAPPING->enable)
        {
            RequestFieldMapping fields 
                = glbl.CHECK_FIELD_MAPPING->REQ.checker.table.Get(this->first.flit.req.Opcode());

            if (fields)
            {
                Flits::REQ<config, conn> origin = this->first.flit.req;
                Flits::REQ<config, conn> retry = xaction->GetFirst().flit.req;
                    
                // QoS
                /* Permitted to be different */
                
                // TgtID
                /* Permitted to be different */

                // SrcID
                if (FieldTrait::IsApplicable(fields->SrcID))
                    if (origin.SrcID() != retry.SrcID())
                        return XactDenial::DENIED_RETRY_DIFF_SRCID;

                // TxnID
                /* Permitted to be different */

                // ReturnNID
                if (FieldTrait::IsApplicable(fields->ReturnNID))
                    if (origin.ReturnNID() != retry.ReturnNID())
                        return XactDenial::DENIED_RETRY_DIFF_RETURNNID;

                // StashNID
                if (FieldTrait::IsApplicable(fields->StashNID))
                    if (origin.StashNID() != retry.StashNID())
                        return XactDenial::DENIED_RETRY_DIFF_STASHNID;
                
#ifdef CHI_ISSUE_EB_ENABLE
                // SLCRepHint
                /* Permitted to be different */
#endif

                // StashNIDValid
                if (FieldTrait::IsApplicable(fields->StashNIDValid))
                    if (origin.StashNIDValid() != retry.StashNIDValid())
                        return XactDenial::DENIED_RETRY_DIFF_STASHNIDVALID;

                // Endian
                if (FieldTrait::IsApplicable(fields->Endian))
                    if (origin.Endian() != retry.Endian())
                        return XactDenial::DENIED_RETRY_DIFF_ENDIAN;

#ifdef CHI_ISSUE_EB_ENABLE
                // Deep
                if (FieldTrait::IsApplicable(fields->Deep))
                    if (origin.Deep() != retry.Deep())
                        return XactDenial::DENIED_RETRY_DIFF_DEEP;
#endif

                // ReturnTxnID
                /* Permitted to be different */

                // StashLPIDValid
                if (FieldTrait::IsApplicable(fields->StashLPIDValid))
                    if (origin.StashLPIDValid() != retry.StashLPIDValid())
                        return XactDenial::DENIED_RETRY_DIFF_STASHLPIDVALID;

                // StashLPID
                if (FieldTrait::IsApplicable(fields->StashLPID))
                    if (origin.StashLPID() != retry.StashLPID())
                        return XactDenial::DENIED_RETRY_DIFF_STASHLPID;

                // Opcode
                if (FieldTrait::IsApplicable(fields->Opcode)) [[likely]]
                    if (origin.Opcode() != retry.Opcode())
                        return XactDenial::DENIED_RETRY_DIFF_OPCODE;

                // Size
                if (FieldTrait::IsApplicable(fields->Size))
                    if (origin.Size() != retry.Size())
                        return XactDenial::DENIED_RETRY_DIFF_SIZE;

                // Addr
                if (FieldTrait::IsApplicable(fields->Addr))
                    if (origin.Addr() != retry.Addr())
                        return XactDenial::DENIED_RETRY_DIFF_ADDR;

                // NS
                if (FieldTrait::IsApplicable(fields->NS))
                    if (origin.NS() != retry.NS())
                        return XactDenial::DENIED_RETRY_DIFF_NS;

                // LikelyShared
                if (FieldTrait::IsApplicable(fields->LikelyShared))
                    if (origin.LikelyShared() != retry.LikelyShared())
                        return XactDenial::DENIED_RETRY_DIFF_LIKELYSHARED;

                // AllowRetry
                /* Not checked */

                // Order
                if (FieldTrait::IsApplicable(fields->Order))
                    if (origin.Order() != retry.Order())
                        return XactDenial::DENIED_RETRY_DIFF_ORDER;

                // PCrdType
                /* Not checked */

                // MemAttr
                if (FieldTrait::IsApplicable(fields->Allocate))
                    if (MemAttrs::ExtractAllocate(origin.MemAttr()) != MemAttrs::ExtractAllocate(retry.MemAttr()))
                        return XactDenial::DENIED_RETRY_DIFF_MEMATTR;

                if (FieldTrait::IsApplicable(fields->Cacheable))
                    if (MemAttrs::ExtractCacheable(origin.MemAttr()) != MemAttrs::ExtractCacheable(retry.MemAttr()))
                        return XactDenial::DENIED_RETRY_DIFF_MEMATTR;

                if (FieldTrait::IsApplicable(fields->Device))
                    if (MemAttrs::ExtractDevice(origin.MemAttr()) != MemAttrs::ExtractDevice(retry.MemAttr()))
                        return XactDenial::DENIED_RETRY_DIFF_MEMATTR;
                
                if (FieldTrait::IsApplicable(fields->EWA))
                    if (MemAttrs::ExtractEWA(origin.MemAttr()) != MemAttrs::ExtractEWA(origin.MemAttr()))
                        return XactDenial::DENIED_RETRY_DIFF_MEMATTR;

                // SnpAttr
                if (FieldTrait::IsApplicable(fields->SnpAttr))
                    if (origin.SnpAttr() != retry.SnpAttr())
                        return XactDenial::DENIED_RETRY_DIFF_SNPATTR;

#ifdef CHI_ISSUE_EB_ENABLE
                // DoDWT
                if (FieldTrait::IsApplicable(fields->DoDWT))
                    if (origin.DoDWT() != retry.DoDWT())
                        return XactDenial::DENIED_RETRY_DIFF_DODWT;
#endif

                // LPID
                if (FieldTrait::IsApplicable(fields->LPID))
                    if (origin.LPID() != retry.LPID())
                        return XactDenial::DENIED_RETRY_DIFF_LPID;

#ifdef CHI_ISSUE_EB_ENABLE
                // PGroupID
                if (FieldTrait::IsApplicable(fields->PGroupID))
                    if (origin.PGroupID() != retry.PGroupID())
                        return XactDenial::DENIED_RETRY_DIFF_PGROUPID;
#endif
                
#ifdef CHI_ISSUE_EB_ENABLE
                // StashGroupID
                if (FieldTrait::IsApplicable(fields->StashGroupID))
                    if (origin.StashGroupID() != retry.StashGroupID())
                        return XactDenial::DENIED_RETRY_DIFF_STASHGROUPID;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
                // TagGroupID
                if (FieldTrait::IsApplicable(fields->TagGroupID))
                    if (origin.TagGroupID() != retry.TagGroupID())
                        return XactDenial::DENIED_RETRY_DIFF_TAGGROUPID;
#endif

                // Excl
                if (FieldTrait::IsApplicable(fields->Excl))
                    if (origin.Excl() != retry.Excl())
                        return XactDenial::DENIED_RETRY_DIFF_EXCL;

                // SnoopMe
                if (FieldTrait::IsApplicable(fields->SnoopMe))
                    if (origin.SnoopMe() != retry.SnoopMe())
                        return XactDenial::DENIED_RETRY_DIFF_SNOOPME;

                // ExpCompAck
                if (FieldTrait::IsApplicable(fields->ExpCompAck))
                    if (origin.ExpCompAck() != retry.ExpCompAck())
                        return XactDenial::DENIED_RETRY_DIFF_EXPCOMPACK;

#ifdef CHI_ISSUE_EB_ENABLE
                // TagOp
                if (FieldTrait::IsApplicable(fields->TagOp))
                    if (origin.TagOp() != retry.TagOp())
                        return XactDenial::DENIED_RETRY_DIFF_TAGOP;
#endif

                // TraceTag
                /* Permitted to be different */

                // MPAM
                if (FieldTrait::IsApplicable(fields->MPAM))
                    if (origin.MPAM() != retry.MPAM())
                        return XactDenial::DENIED_RETRY_DIFF_MPAM;

                // RSVDC
                /* Permitted to be different */
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::NextDataID(
        Flits::REQ<config, conn>::ssize_t                                   size, 
        const FiredResponseFlit<config, conn>&                              datFlit,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>  opcodes) noexcept
    {
        std::function<bool(size_t, const FiredResponseFlit<config, conn>&)> validFunc = 
            [&](size_t i, const FiredResponseFlit<config, conn>& flit) -> bool
        {
            bool opcodeMatch = false;
            if (opcodes.size() == 0)
                opcodeMatch = true;
            else
            {
                for (auto opcode : opcodes)
                    if (opcode == flit.flit.dat.Opcode())
                    {
                        opcodeMatch = true;
                        break;
                    }
            }

            return opcodeMatch && this->subsequenceKeys[i].denial == XactDenial::ACCEPTED;
        };

        std::bitset<4> collectedDataID = details::CollectDataID<config, conn>(
            size, this->subsequence, validFunc);

        std::bitset<4> nextDataID = details::CollectDataID<config, conn>(
            size, datFlit.flit.dat.DataID());

        return (collectedDataID & nextDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::NextREQDataID(
        const FiredResponseFlit<config, conn>&                              datFlit,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>  opcodes) noexcept
    {
        return NextDataID(this->first.flit.req.Size(), datFlit, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool Xaction<config, conn>::NextSNPDataID(
        const FiredResponseFlit<config, conn>&                              datFlit,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>  opcodes) noexcept
    {
        return NextDataID(Sizes::B64, datFlit, opcodes);
    }
}


#endif
