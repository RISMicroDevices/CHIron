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

        template<FlitConfigurationConcept config>
        class Local {
            
        };
    }

    namespace Xact {

        template<FlitConfigurationConcept config>
        class Xaction;

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


        // Xaction Denial Events
        template<FlitConfigurationConcept config>
        class XactionEventBase {
        protected:
            Xaction<config>& xaction;

        public:
            XactionEventBase(Xaction<config>& xaction) noexcept;

        public:
            Xaction<config>&        GetXaction() noexcept;
            const Xaction<config>&  GetXaction() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class XactionDeniedEventBase : public XactionEventBase<config> {
        protected:
            XactDenialEnum  denial;
            std::string     message;

        public:
            XactionDeniedEventBase(Xaction<config>& xaction, XactDenialEnum denial, const std::string& message = "") noexcept;

        public:
            XactDenialEnum          GetDenial() const noexcept;

            std::string&            GetMessage() noexcept;
            const std::string&      GetMessage() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class XactionDeniedRequestFlitEvent : public XactionDeniedEventBase<config>, 
                                              public Gravity::Event<XactionDeniedRequestFlitEvent<config>> {
        protected:
            const FiredRequestFlit<config>& flit;
        
        public:
            XactionDeniedRequestFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredRequestFlit<config>& flit, const std::string& message = "") noexcept;

        public:
            const FiredRequestFlit<config>& GetFlit() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class XactionDeniedResponseFlitEvent : public XactionDeniedEventBase<config>, 
                                               public Gravity::Event<XactionDeniedResponseFlitEvent<config>> {
        protected:
            const FiredResponseFlit<config>& flit;

        public:
            XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const std::string& message = "") noexcept;

        public:
            const FiredResponseFlit<config>& GetFlit() const noexcept;
        };

        
        // Xaction Base
        template<FlitConfigurationConcept config>
        class Xaction {
        public:
            class EventHub {
            public:
                Gravity::EventBus<XactionDeniedRequestFlitEvent<config>>    OnDeniedRequestFlit;
                Gravity::EventBus<XactionDeniedResponseFlitEvent<config>>   OnDeniedResponseFlit;

            public:
                EventHub() noexcept;
                void Clear() noexcept;
            };

            std::shared_ptr<EventHub> events;
            
        public:
            // *NOTICE: Subsequence Key contains several critical attributes and fields
            //          of fired response flits to speed up Xaction query operations,
            //          because full flit data might not fit into a single cache line and
            //          streaming on full flit data wastes cache lines and memory bandwidth.
            struct SubsequenceKey {
                XactDenialEnum  denial;
                bool            isRSP;
                union   {
                    Flits::RSP<config>::opcode_t rsp;
                    Flits::DAT<config>::opcode_t dat;
                }               opcode;
                bool            hasDBID;

                SubsequenceKey() noexcept;
                SubsequenceKey(XactDenialEnum, Flits::RSP<config>::opcode_t, bool hasDBID) noexcept;
                SubsequenceKey(XactDenialEnum, Flits::DAT<config>::opcode_t, bool hasDBID) noexcept;

                bool        IsRSP() const noexcept;
                bool        IsDAT() const noexcept;

                bool        IsAccepted() const noexcept;
                bool        IsDenied() const noexcept;

                bool        HasDBID() const noexcept;
            };

        private:
            const XactionType                               type;

        protected:
            FiredRequestFlit<config>                        first;
            XactDenialEnum                                  firstDenial;
            std::vector<FiredResponseFlit<config>>          subsequence;
            std::vector<SubsequenceKey>                     subsequenceKeys;

            bool                                            resent;
            std::shared_ptr<Xaction<config>>                resentXaction;
            FiredResponseFlit<config>                       sourcePCredit;
            XactDenialEnum                                  resentDenial;

            bool                                            retried;
            std::shared_ptr<Xaction<config>>                retriedXaction;

        public:
            Companion                                       companion;

        public:
            Xaction(XactionType                             type, 
                    const FiredRequestFlit<config>&         first) noexcept;

            Xaction(XactionType                             type, 
                    const FiredRequestFlit<config>&         first,
                    std::shared_ptr<Xaction<config>>        retried) noexcept;

            XactionType                                     GetType() const noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>        Clone() const noexcept = 0;
            template<class T> std::shared_ptr<T>            Clone() const noexcept;

        public:
            const FiredRequestFlit<config>&         GetFirst() const noexcept;
            XactDenialEnum                          GetFirstDenial() const noexcept;
            const std::vector<FiredResponseFlit<config>>&   
                                                    GetSubsequence() const noexcept;
            XactDenialEnum                          GetSubsequentDenial(size_t index) const noexcept;

            XactDenialEnum                          GetLastDenial() const noexcept;
            void                                    SetLastDenial(XactDenialEnum) noexcept;
            
            size_t                                  Revert(size_t count = 1) noexcept;

        public:
            bool                                    IsSecondTry() const noexcept;
            std::shared_ptr<Xaction<config>>        GetFirstTry() const noexcept;

        public:
            bool                                    HasRSP(std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            bool                                    HasDAT(std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;

            const FiredResponseFlit<config>*       GetFirstRSP() const noexcept;
            const FiredResponseFlit<config>*       GetFirstDAT() const noexcept;
            const FiredResponseFlit<config>*       GetLastRSP() const noexcept;
            const FiredResponseFlit<config>*       GetLastDAT() const noexcept;

            const FiredResponseFlit<config>*       GetFirst(ChannelTypeEnum channel) const noexcept;
            const FiredResponseFlit<config>*       GetLast(ChannelTypeEnum channel) const noexcept;

            const FiredResponseFlit<config>*       GetFirstRSP(std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetFirstDAT(std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetLastRSP(std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetLastDAT(std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;

            const FiredResponseFlit<config>*       GetFirst(std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept;
            const FiredResponseFlit<config>*       GetLast(std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept;

            const FiredResponseFlit<config>*       GetFirstRSPFrom(const Global<config>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetFirstDATFrom(const Global<config>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetLastRSPFrom(const Global<config>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetLastDATFrom(const Global<config>&, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config>*       GetFirstFrom(const Global<config>&, ChannelTypeEnum, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetLastFrom(const Global<config>&, ChannelTypeEnum, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config>*       GetFirstRSPFrom(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetFirstDATFrom(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetLastRSPFrom(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetLastDATFrom(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;

            const FiredResponseFlit<config>*       GetFirstFrom(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept;
            const FiredResponseFlit<config>*       GetLastFrom(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept;

            const FiredResponseFlit<config>*       GetFirstRSPTo(const Global<config>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetFirstDATTo(const Global<config>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetLastRSPTo(const Global<config>&, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetLastDATTo(const Global<config>&, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config>*       GetFirstTo(const Global<config>&, ChannelTypeEnum, XactScopeEnum) const noexcept;
            const FiredResponseFlit<config>*       GetLastTo(const Global<config>&, ChannelTypeEnum, XactScopeEnum) const noexcept;

            const FiredResponseFlit<config>*       GetFirstRSPTo(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetFirstDATTo(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetLastRSPTo(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*       GetLastDATTo(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;

            const FiredResponseFlit<config>*       GetFirstTo(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept;
            const FiredResponseFlit<config>*       GetLastTo(const Global<config>&, XactScopeEnum, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept;

        public:
            bool                                    GotDBID() const noexcept;
            std::optional<typename Flits::RSP<config>::dbid_t>
                                                    GetDBID() const noexcept;
            const FiredResponseFlit<config>*        GetDBIDSource() const noexcept;
            const FiredResponseFlit<config>*        GetLastDBIDSourceRSP(std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
            const FiredResponseFlit<config>*        GetLastDBIDSourceDAT(std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;

            bool                                    GotPrimaryTgtID(const Global<config>& glbl) const noexcept;
            std::optional<typename Flits::REQ<config>::tgtid_t>
                                                    GetPrimaryTgtID(const Global<config>& glbl) const noexcept;
            const FiredFlit<config>*                GetPrimaryTgtIDSource(const Global<config>& glbl) const noexcept;
            bool                                    IsPrimaryTgtIDSourceREQ(const Global<config>& glbl) const noexcept;

            bool                                    GotDMTSrcID(const Global<config>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config>::srcid_t>
                                                    GetDMTSrcID(const Global<config>& glbl) const noexcept;
            virtual const FiredResponseFlit<config>*
                                                    GetDMTSrcIDSource(const Global<config>& glbl) const noexcept;

            bool                                    GotDMTTgtID(const Global<config>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config>::tgtid_t>
                                                    GetDMTTgtID(const Global<config>& glbl) const noexcept;
            virtual const FiredResponseFlit<config>*
                                                    GetDMTTgtIDSource(const Global<config>& glbl) const noexcept;

            bool                                    GotDCTSrcID(const Global<config>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config>::srcid_t>
                                                    GetDCTSrcID(const Global<config>& glbl) const noexcept;
            virtual const FiredResponseFlit<config>*
                                                    GetDCTSrcIDSource(const Global<config>& glbl) const noexcept;

            bool                                    GotDCTTgtID(const Global<config>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config>::tgtid_t>
                                                    GetDCTTgtID(const Global<config>& glbl) const noexcept;
            virtual const FiredResponseFlit<config>*
                                                    GetDCTTgtIDSource(const Global<config>& glbl) const noexcept;

            bool                                    GotDWTSrcID(const Global<config>& glbl) const noexcept;
            std::optional<typename Flits::DAT<config>::srcid_t>
                                                    GetDWTSrcID(const Global<config>& glbl) const noexcept;
            virtual const FiredResponseFlit<config>*
                                                    GetDWTSrcIDSource(const Global<config>& glbl) const noexcept;

            bool                                    GotDWTTgtID(const Global<config>& glbl) const noexcept;
            std::optional<typename Flits::RSP<config>::srcid_t>
                                                    GetDWTTgtID(const Global<config>& glbl) const noexcept;
            virtual const FiredResponseFlit<config>*  
                                                    GetDWTTgtIDSource(const Global<config>& glbl) const noexcept;

            bool                                    GotRetryAck() const noexcept;
            const FiredResponseFlit<config>*        GetRetryAck() const noexcept;

            bool                                    IsResent() const noexcept;
            std::shared_ptr<Xaction<config>>        GetResentXaction() const noexcept;
            const FiredResponseFlit<config>&        GetPCreditSource() const noexcept;

            virtual XactDenialEnum                  Next(const Global<config>& glbl, const FiredResponseFlit<config>& flit, bool& hasDBID, bool& firstDBID) noexcept;
            virtual XactDenialEnum                  NextRSP(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept;
            virtual XactDenialEnum                  NextDAT(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept;

            virtual XactDenialEnum                  Resend(const Global<config>& glbl, FiredResponseFlit<config> pCrdFlit, std::shared_ptr<Xaction<config>> xaction) noexcept;
            virtual bool                            RevertResent() noexcept;

            virtual bool                            IsTxnIDComplete(const Global<config>& glbl) const noexcept = 0;
            virtual bool                            IsDBIDComplete(const Global<config>& glbl) const noexcept = 0;
            virtual bool                            IsComplete(const Global<config>& glbl) const noexcept = 0;

            // *NOTICE: Responses with both valid TxnID and DBID like Comp could be out-of-order on interconnect
            //          and came after DBID grant (e.g. DBIDResp, CompDBIDResp).
            virtual bool                            IsDBIDOverlappable(const Global<config>& glbl) const noexcept = 0;

        protected:
            virtual const FiredResponseFlit<config>*  
                                                    GetPrimaryTgtIDSourceNonREQ(const Global<config>& glbl) const noexcept = 0;
            virtual std::optional<typename Flits::REQ<config>::tgtid_t>
                                                    GetPrimaryTgtIDNonREQ(const Global<config>& glbl) const noexcept = 0;

        public:
            virtual bool                            IsDMTPossible() const noexcept;
            virtual bool                            IsDCTPossible() const noexcept;
            virtual bool                            IsDWTPossible() const noexcept;

        protected:
            virtual XactDenialEnum                  NextRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;
            virtual XactDenialEnum                  NextDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;

        protected:
            virtual XactDenialEnum                  NextRetryAckNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit) noexcept;
        
            virtual XactDenialEnum                  ResendNoRecord(const Global<config>& glbl, FiredResponseFlit<config> pCrdFlit, std::shared_ptr<Xaction<config>> xaction) noexcept;
        
            virtual bool                            NextDataID(Flits::REQ<config>::ssize_t, const FiredResponseFlit<config>& datFlit, std::initializer_list<typename Flits::DAT<config>::opcode_t>) noexcept;
            virtual bool                            NextREQDataID(const FiredResponseFlit<config>& datFlit, std::initializer_list<typename Flits::DAT<config>::opcode_t> = {}) noexcept;
            virtual bool                            NextSNPDataID(const FiredResponseFlit<config>& datFlit, std::initializer_list<typename Flits::DAT<config>::opcode_t> = {}) noexcept;

        protected:
            XactDenialEnum  RequestFlitDenied(XactDenialEnum                    denial, 
                                              const FiredRequestFlit<config>&   flit,
                                              const std::string&                message = "") noexcept;

            XactDenialEnum  ResponseFlitDenied(XactDenialEnum                     denial, 
                                               const FiredResponseFlit<config>&   flit,
                                               const std::string&                 message = "") noexcept;
        };
    }

    namespace Xact::details {

        template<FlitConfigurationConcept config>
        inline static std::bitset<4> CollectDataID(
            typename Flits::REQ<config>::ssize_t    reqSize,
            typename Flits::DAT<config>::dataid_t   dataID) noexcept;

        template<FlitConfigurationConcept config, typename T>
        requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
        inline static std::bitset<4> CollectDataID(
            typename Flits::REQ<config>::ssize_t                            reqSize,
            const T&                                                        flits,
            std::function<bool(size_t, const FiredResponseFlit<config>&)>   validFunc  = [](auto, auto) -> bool { return true; }) noexcept;

        template<FlitConfigurationConcept config, typename T>
        requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
        inline static std::bitset<4> CollectDataID(
            typename Flits::REQ<config>::ssize_t                            reqSize,
            const T&                                                        flits,
            std::initializer_list<typename Flits::DAT<config>::opcode_t>    datOpcodes) noexcept;

        template<FlitConfigurationConcept config>
        inline static std::bitset<4> GetDataIDCompleteMask(
            typename Flits::REQ<config>::ssize_t    reqSize) noexcept;
    }
/*
}
*/


// Implementation of: details
namespace /*CHI::*/Xact::details {

    template<FlitConfigurationConcept config>
    inline static std::bitset<4> CollectDataID(
        typename Flits::REQ<config>::ssize_t  reqSize,
        typename Flits::DAT<config>::dataid_t dataID) noexcept
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

    template<FlitConfigurationConcept config, typename T>
    requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
    inline static std::bitset<4> CollectDataID(
        typename Flits::REQ<config>::ssize_t  reqSize,
        const T& flits,
        std::function<bool(size_t, const FiredResponseFlit<config>&)> validFunc) noexcept
    {
        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (const FiredResponseFlit<config>& flit : flits)
        {
            if (flit.IsDAT() && validFunc(index, flit))
            {
                collectedDataID |= CollectDataID<config>(
                    reqSize, flit.flit.dat.DataID());
            }
            
            index++;
        }

        return collectedDataID;
    }

    template<FlitConfigurationConcept config, typename T>
    requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
    inline static std::bitset<4> CollectDataID(
        typename Flits::REQ<config>::ssize_t                            reqSize,
        const T&                                                        flits,
        std::initializer_list<typename Flits::DAT<config>::opcode_t>    datOpcodes) noexcept
    {
        return CollectDataID<config>(reqSize, flits, [&datOpcodes](auto, const auto& flit) -> bool {
            return flit.IsDAT() && datOpcodes.size() > 0 ? datOpcodes.contains(flit.flit.dat.Opcode()) : true;
        });
    }


    template<FlitConfigurationConcept config>
    inline static std::bitset<4> GetDataIDCompleteMask(
        typename Flits::REQ<config>::ssize_t  reqSize) noexcept
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

    template<FlitConfigurationConcept config>
    inline Xaction<config>::SubsequenceKey::SubsequenceKey() noexcept
    {
        this->denial        = XactDenial::NOT_INITIALIZED;
        this->isRSP         = false;
        this->hasDBID       = false;
    }

    template<FlitConfigurationConcept config>
    inline Xaction<config>::SubsequenceKey::SubsequenceKey(
        XactDenialEnum                  denial,
        Flits::RSP<config>::opcode_t    opcode,
        bool                            hasDBID) noexcept
    {
        this->denial        = denial;
        this->isRSP         = true;
        this->opcode.rsp    = opcode;
        this->hasDBID       = hasDBID;
    }

    template<FlitConfigurationConcept config>
    inline Xaction<config>::SubsequenceKey::SubsequenceKey(
        XactDenialEnum                  denial,
        Flits::DAT<config>::opcode_t    opcode,
        bool                            hasDBID) noexcept
    {
        this->denial        = denial;
        this->isRSP         = false;
        this->opcode.dat    = opcode;
        this->hasDBID       = hasDBID;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsRSP() const noexcept
    {
        return isRSP;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsDAT() const noexcept
    {
        return !IsRSP();
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsAccepted() const noexcept
    {
        return denial == XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsDenied() const noexcept
    {
        return !IsAccepted();
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::HasDBID() const noexcept
    {
        return hasDBID;
    }
}


// Implementation of: class Xaction
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline Xaction<config>::Xaction(XactionType                     type, 
                                    const FiredRequestFlit<config>& first) noexcept
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

    template<FlitConfigurationConcept config>
    inline Xaction<config>::Xaction(XactionType                         type, 
                                    const FiredRequestFlit<config>&     first,
                                    std::shared_ptr<Xaction<config>>    retried) noexcept
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

    template<FlitConfigurationConcept config>
    inline XactionType Xaction<config>::GetType() const noexcept
    {
        return type;
    }

    template<FlitConfigurationConcept config>
    inline Xaction<config>::EventHub::EventHub() noexcept
        : OnDeniedRequestFlit   (0)
        , OnDeniedResponseFlit  (0)
    { }

    template<FlitConfigurationConcept config>
    inline void Xaction<config>::EventHub::Clear() noexcept
    {
        OnDeniedRequestFlit.Clear();
        OnDeniedResponseFlit.Clear();
    }

    template<FlitConfigurationConcept config>
    template<class T>
    inline std::shared_ptr<T> Xaction<config>::Clone() const noexcept
    {
        return std::make_shared<T>(*static_cast<T*>(this));
    }

    template<FlitConfigurationConcept config>
    inline const FiredRequestFlit<config>& Xaction<config>::GetFirst() const noexcept
    {
        return first;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::GetFirstDenial() const noexcept
    {
        return firstDenial;
    }

    template<FlitConfigurationConcept config>
    inline const std::vector<FiredResponseFlit<config>>& Xaction<config>::GetSubsequence() const noexcept
    {
        return subsequence;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::GetSubsequentDenial(size_t index) const noexcept
    {
        return subsequenceKeys[index].denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::GetLastDenial() const noexcept
    {
        if (subsequenceKeys.empty())
            return firstDenial;

        return subsequenceKeys.back().denial;
    }

    template<FlitConfigurationConcept config>
    inline void Xaction<config>::SetLastDenial(XactDenialEnum denial) noexcept
    {
        if (subsequenceKeys.empty())
            this->firstDenial = denial;

        subsequenceKeys.back().denial = denial;
    }

    template<FlitConfigurationConcept config>
    inline size_t Xaction<config>::Revert(size_t count) noexcept
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

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::IsSecondTry() const noexcept
    {
        return retried;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Xaction<config>::GetFirstTry() const noexcept
    {
        return retriedXaction;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasRSP(
        std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasDAT(
        std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstRSP() const noexcept
    {
        return GetFirst(ChannelType::RSP);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDAT() const noexcept
    {
        return GetFirst(ChannelType::DAT);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastRSP() const noexcept
    {
        return GetLast(ChannelType::RSP);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDAT() const noexcept
    {
        return GetLast(ChannelType::DAT);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirst(ChannelTypeEnum channel) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLast(ChannelTypeEnum channel) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstRSP(
        std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirst(opcodes, {});
    }
    
    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDAT(
        std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirst({}, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastRSP(
        std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetLast(opcodes, {});
    }
    
    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDAT(
        std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetLast({}, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirst(
        std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes,
        std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLast(
        std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes,
        std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstRSPFrom(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstFrom(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDATFrom(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstFrom(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastRSPFrom(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastFrom(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDATFrom(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastFrom(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstFrom(const Global<config>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastFrom(const Global<config>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstRSPFrom(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirstFrom(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDATFrom(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirstFrom(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastRSPFrom(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetLastFrom(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDATFrom(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetLastFrom(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstFrom(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastFrom(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstRSPTo(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstTo(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDATTo(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetFirstTo(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastRSPTo(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastTo(glbl, ChannelType::RSP, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDATTo(const Global<config>& glbl, XactScopeEnum scope) const noexcept
    {
        return GetLastTo(glbl, ChannelType::DAT, scope);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstTo(const Global<config>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastTo(const Global<config>& glbl, ChannelTypeEnum channel, XactScopeEnum scope) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstRSPTo(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirstTo(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDATTo(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirstTo(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastRSPTo(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetLastTo(glbl, scope, opcodes, {});
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDATTo(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetLastTo(glbl, scope, {}, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstTo(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastTo(const Global<config>& glbl, XactScopeEnum scope, std::initializer_list<typename Flits::RSP<config>::opcode_t> rspOpcodes, std::initializer_list<typename Flits::DAT<config>::opcode_t> datOpcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotDBID() const noexcept
    {
        return GetDBIDSource() != nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::RSP<config>::dbid_t> Xaction<config>::GetDBID() const noexcept
    {
        const FiredResponseFlit<config>* dbidSource = GetDBIDSource();

        if (dbidSource != nullptr)
        {
            if (dbidSource->IsRSP())
                return { dbidSource->flit.rsp.DBID() };
            else
                return { static_cast<Flits::RSP<config>::dbid_t>(dbidSource->flit.dat.DBID()) };
        }

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDBIDSource() const noexcept
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

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDBIDSourceRSP(
        std::initializer_list<typename Flits::RSP<config>::opcode_t> opcodes) const noexcept
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
    
    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDBIDSourceDAT(
        std::initializer_list<typename Flits::DAT<config>::opcode_t> opcodes) const noexcept
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

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotPrimaryTgtID(const Global<config>& glbl) const noexcept
    {
        return GetPrimaryTgtID(glbl);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::REQ<config>::tgtid_t> Xaction<config>::GetPrimaryTgtID(const Global<config>& glbl) const noexcept
    {
        if (IsPrimaryTgtIDSourceREQ(glbl))
            return { this->first.flit.req.TgtID() };
        else
            return GetPrimaryTgtIDNonREQ(glbl);
    }

    template<FlitConfigurationConcept config>
    inline const FiredFlit<config>* Xaction<config>::GetPrimaryTgtIDSource(const Global<config>& glbl) const noexcept
    {
        if (IsPrimaryTgtIDSourceREQ(glbl))
            return &(this->first);
        else
            return GetPrimaryTgtIDSourceNonREQ(glbl);
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::IsPrimaryTgtIDSourceREQ(const Global<config>& glbl) const noexcept
    {
        if (!this->first.IsREQ())
            return false;

        if (!glbl.SAM_SCOPE.enable)
            return true;

        switch (glbl.SAM_SCOPE.Get(this->first.flit.req.SrcID())->value)
        {
            case SAMScope::AfterSAM:
                return true;

            case SAMScope::BeforeSAM:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotDMTSrcID(const Global<config>& glbl) const noexcept
    {
        return GetDMTSrcID(glbl);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::DAT<config>::srcid_t> Xaction<config>::GetDMTSrcID(const Global<config>& glbl) const noexcept
    {
        if (!IsDMTPossible())
            return std::nullopt;

        const FiredResponseFlit<config>* optSource = GetDMTSrcIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DMT SrcID should come from DAT channel only");

        return { optSource->flit.dat.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDMTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotDMTTgtID(const Global<config>& glbl) const noexcept
    {
        return GetDMTTgtID(glbl);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::DAT<config>::tgtid_t> Xaction<config>::GetDMTTgtID(const Global<config>& glbl) const noexcept
    {
        if (!IsDMTPossible())
            return std::nullopt;

        const FiredResponseFlit<config>* optSource = GetDMTTgtIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DMT TgtID should come from DAT channel only");

        return { optSource->flit.dat.TgtID() };
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDMTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotDCTSrcID(const Global<config>& glbl) const noexcept
    {
        return GetDCTSrcID(glbl);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::DAT<config>::srcid_t> Xaction<config>::GetDCTSrcID(const Global<config>& glbl) const noexcept
    {
        if (!IsDCTPossible())
            return std::nullopt;

        const FiredResponseFlit<config>* optSource = GetDCTSrcIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DCT SrcID should come from DAT channel only");

        return { optSource->flit.dat.SrcID() };
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDCTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotDCTTgtID(const Global<config>& glbl) const noexcept
    {
        return GetDCTTgtID(glbl);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::DAT<config>::tgtid_t> Xaction<config>::GetDCTTgtID(const Global<config>& glbl) const noexcept
    {
        if (!IsDCTPossible())
            return std::nullopt;

        const FiredResponseFlit<config>* optSource = GetDCTTgtIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsDAT() && "DCT TgtID should come from DAT channel only");

        return { optSource->flit.dat.TgtID() };
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDCTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotDWTSrcID(const Global<config>& glbl) const noexcept
    {
        return GetDWTSrcID(glbl);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::DAT<config>::srcid_t> Xaction<config>::GetDWTSrcID(const Global<config>& glbl) const noexcept
    {
        if (!IsDWTPossible())
            return std::nullopt;

        const FiredResponseFlit<config>* optSource = GetDWTSrcIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsRSP() && "DWT SrcID should come from RSP channel only");

        return { optSource->flit.rsp.TgtID() };
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDWTSrcIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotDWTTgtID(const Global<config>& glbl) const noexcept
    {
        return GetDWTTgtID(glbl);
    }

    template<FlitConfigurationConcept config>
    inline std::optional<typename Flits::RSP<config>::srcid_t> Xaction<config>::GetDWTTgtID(const Global<config>& glbl) const noexcept
    {
        if (!IsDWTPossible())
            return std::nullopt;

        const FiredResponseFlit<config>* optSource = GetDWTTgtIDSource(glbl);

        if (!optSource)
            return std::nullopt;

        assert(optSource->IsRSP() && "DWT TgtID should come from RSP channel only");

        return { optSource->flit.rsp.TgtID() };
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDWTTgtIDSource(const Global<config>& glbl) const noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::GotRetryAck() const noexcept
    {
        return GetRetryAck() != nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetRetryAck() const noexcept
    {
        return GetLastRSP({ Opcodes::RSP::RetryAck });
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::IsResent() const noexcept
    {
        return resent;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Xaction<config>::GetResentXaction() const noexcept
    {
        return resentXaction;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>& Xaction<config>::GetPCreditSource() const noexcept
    {
        return sourcePCredit;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::Next(const Global<config>& glbl, const FiredResponseFlit<config>& flit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return flit.IsRSP() ? NextRSP(glbl, flit, hasDBID, firstDBID) : NextDAT(glbl, flit, hasDBID, firstDBID);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::NextRSP(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!rspFlit.IsRSP()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL_NOT_RSP;

        XactDenialEnum denial = NextRSPNoRecord(glbl, rspFlit, hasDBID, firstDBID);

        subsequence.push_back(rspFlit);
        subsequenceKeys.emplace_back(denial, rspFlit.flit.rsp.Opcode(), hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::NextDAT(const Global<config>& glbl, const FiredResponseFlit<config>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!datFlit.IsDAT()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DAT, datFlit);

        XactDenialEnum denial = NextDATNoRecord(glbl, datFlit, hasDBID, firstDBID);

        subsequence.push_back(datFlit);
        subsequenceKeys.emplace_back(denial, datFlit.flit.dat.Opcode(), hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::Resend(const Global<config>& glbl, FiredResponseFlit<config> pCrdFlit, std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        XactDenialEnum denial = ResendNoRecord(glbl, pCrdFlit, xaction);

        resent = true;
        resentXaction = xaction;
        sourcePCredit = pCrdFlit;
        resentDenial = denial;

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::RevertResent() noexcept
    {
        if (resent)
        {
            resent = false;
            return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::IsDMTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::IsDCTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::IsDWTPossible() const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::NextRetryAckNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& rspFlit) noexcept
    {
        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL_NOT_RSP;

        if (rspFlit.flit.rsp.Opcode() != Opcodes::RSP::RetryAck)
            return this->ResponseFlitDenied(XactDenial::DENIED_RSP_OPCODE, rspFlit,
                "RetryAck expected");

        if (!rspFlit.IsFromHomeToRequester(glbl) && !rspFlit.IsFromSubordinateToHome(glbl))
            return XactDenial::DENIED_RSP_RETRYACK_ROUTE;

        if (!this->subsequence.empty())
            return XactDenial::DENIED_RETRY_ON_ACTIVE_PROGRESS;

        if (!this->first.flit.req.AllowRetry())
            return XactDenial::DENIED_RETRY_NO_ALLOWRETRY;

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

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::ResendNoRecord(const Global<config>& glbl, FiredResponseFlit<config> pCrdFlit, std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        if (xaction->GetType() != type)
            return XactDenial::DENIED_RETRY_DIFF_XACT_TYPE;

        if (!pCrdFlit.IsRSP())
            return XactDenial::DENIED_PCRD_CHANNEL_NOT_RSP;

        if (!xaction->GetFirst().IsREQ()) // TODO: use individual denial for "Retry on non-REQ xaction"
            return this->RequestFlitDenied(XactDenial::DENIED_CHANNEL_NOT_REQ, xaction->GetFirst());

        auto retryAck = GetRetryAck();
        if (!retryAck)
            return XactDenial::DENIED_PCRD_NO_RETRY;

        if (pCrdFlit.flit.rsp.PCrdType() != retryAck->flit.rsp.PCrdType())
            return XactDenial::DENIED_PCRD_TYPE_MISMATCH;

        // Check Fields Difference of retried request
        if (glbl.CHECK_FIELD_MAPPING.enable)
        {
            RequestFieldMapping fields 
                = glbl.CHECK_FIELD_MAPPING.REQ.checker.table.Get(this->first.flit.req.Opcode());

            if (fields)
            {
                Flits::REQ<config> origin = this->first.flit.req;
                Flits::REQ<config> retry = xaction->GetFirst().flit.req;
                    
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

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::NextDataID(
        Flits::REQ<config>::ssize_t                                   size, 
        const FiredResponseFlit<config>&                              datFlit,
        std::initializer_list<typename Flits::DAT<config>::opcode_t>  opcodes) noexcept
    {
        std::function<bool(size_t, const FiredResponseFlit<config>&)> validFunc = 
            [&](size_t i, const FiredResponseFlit<config>& flit) -> bool
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

        std::bitset<4> collectedDataID = details::CollectDataID<config>(
            size, this->subsequence, validFunc);

        std::bitset<4> nextDataID = details::CollectDataID<config>(
            size, datFlit.flit.dat.DataID());

        return (collectedDataID & nextDataID).none();
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::NextREQDataID(
        const FiredResponseFlit<config>&                              datFlit,
        std::initializer_list<typename Flits::DAT<config>::opcode_t>  opcodes) noexcept
    {
        return NextDataID(this->first.flit.req.Size(), datFlit, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::NextSNPDataID(
        const FiredResponseFlit<config>&                              datFlit,
        std::initializer_list<typename Flits::DAT<config>::opcode_t>  opcodes) noexcept
    {
        return NextDataID(Sizes::B64, datFlit, opcodes);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::RequestFlitDenied(
        XactDenialEnum                  denial,
        const FiredRequestFlit<config>& flit,
        const std::string&              message) noexcept
    {
        this->events->OnDeniedRequestFlit(XactionDeniedRequestFlitEvent<config>(
            *this, denial, flit, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::ResponseFlitDenied(
        XactDenialEnum                      denial,
        const FiredResponseFlit<config>&    flit,
        const std::string&                  message) noexcept
    {
        this->events->OnDeniedResponseFlit(XactionDeniedResponseFlitEvent<config>(
            *this, denial, flit, message));
        return denial;
    }
}


// Implementation of: class XactionEventBase
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionEventBase<config>::XactionEventBase(Xaction<config>& xaction) noexcept
        : xaction(xaction)
    { }

    template<FlitConfigurationConcept config>
    inline Xaction<config>& XactionEventBase<config>::GetXaction() noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline const Xaction<config>& XactionEventBase<config>::GetXaction() const noexcept
    {
        return xaction;
    }
}

// Implementation of: class XactionDeniedEventBase
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionDeniedEventBase<config>::XactionDeniedEventBase(Xaction<config>& xaction, XactDenialEnum denial, const std::string& message) noexcept
        : XactionEventBase<config>  (xaction)
        , denial                    (denial)
        , message                   (message)
    { }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum XactionDeniedEventBase<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline std::string& XactionDeniedEventBase<config>::GetMessage() noexcept
    {
        return message;
    }

    template<FlitConfigurationConcept config>
    inline const std::string& XactionDeniedEventBase<config>::GetMessage() const noexcept
    {
        return message;
    }
}

// Implementation of: class XactionDeniedRequestFlitEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionDeniedRequestFlitEvent<config>::XactionDeniedRequestFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredRequestFlit<config>& flit, const std::string& message) noexcept
        : XactionDeniedEventBase<config>    (xaction, denial, message)
        , flit                              (flit)
    { }

    template<FlitConfigurationConcept config>
    inline const FiredRequestFlit<config>& XactionDeniedRequestFlitEvent<config>::GetFlit() const noexcept
    {
        return flit;
    }
}

// Implementation of: class XactionDeniedResponseFlitEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline XactionDeniedResponseFlitEvent<config>::XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const std::string& message) noexcept
        : XactionDeniedEventBase<config>    (xaction, denial, message)
        , flit                              (flit)
    { }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>& XactionDeniedResponseFlitEvent<config>::GetFlit() const noexcept
    {
        return flit;
    }
}


#endif
