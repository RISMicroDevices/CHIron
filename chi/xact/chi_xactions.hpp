//#pragma once

//#ifndef __CHI__CHI_XACT_XACTIONS
//#define __CHI__CHI_XACT_XACTIONS

#include <initializer_list>
#ifndef CHI_XACT_XACTIONS__STANDALONE
#   include "chi_xactions_header.hpp"               // IWYU pragma: keep
#   include "chi_xact_base_header.hpp"              // IWYU pragma: keep
#   include "chi_xact_base.hpp"                     // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {

        enum class XactionType {
            AllocatingRead  = 0,
            CopyBackWrite,
            Dataless,
            HomeSnoop,
            ForwardSnoop
        };


        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class Xaction {
        public:
            using FiredRequestFlit  = FiredRequestFlit<config, conn>;
            using FiredResponseFlit = FiredResponseFlit<config, conn>;

        private:
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

                SubsequenceKey(XactDenialEnum, Flits::RSP<config, conn>::opcode_t) noexcept;
                SubsequenceKey(XactDenialEnum, Flits::DAT<config, conn>::opcode_t) noexcept;

                bool        IsRSP() const noexcept;
                bool        IsDAT() const noexcept;

                bool        IsAccepted() const noexcept;
                bool        IsDenied() const noexcept;
            };

        private:
            const XactionType                       type;

        protected:
            FiredRequestFlit                        first;
            XactDenialEnum                          firstDenial;
            std::vector<FiredResponseFlit>          subsequence;
            std::vector<SubsequenceKey>             subsequenceKeys;

            bool                                    resent;
            std::shared_ptr<Xaction<config, conn>>  resentXaction;
            FiredResponseFlit                       sourcePCredit;
            XactDenialEnum                          resentDenial;

        public:
            Xaction(XactionType type, const FiredRequestFlit& first) noexcept;

            XactionType                             GetType() const noexcept;

        public:
            const FiredRequestFlit&                 GetFirst() const noexcept;
            const XactDenialEnum                    GetFirstDenial() const noexcept;
            const std::vector<FiredResponseFlit>&   GetSubsequence() const noexcept;
            XactDenialEnum                          GetSubsequentDenial(size_t index) const noexcept;
            
            size_t                                  Revert(size_t count = 1) noexcept;

        public:
            bool                                    HasRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            bool                                    HasDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            const FiredResponseFlit*                GetFirstRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit*                GetFirstDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit*                GetLastRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit*                GetLastDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            bool                                    GotRetryAck() const noexcept;
            const FiredResponseFlit*                GetRetryAck() const noexcept;

            bool                                    IsResent() const noexcept;
            std::shared_ptr<Xaction<config, conn>>  GetResentXaction() const noexcept;
            const FiredResponseFlit&                GetPCreditSource() const noexcept;

            virtual XactDenialEnum                  Next(const Topology& topo, const FiredResponseFlit& flit) noexcept;
            virtual XactDenialEnum                  NextRSP(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept;
            virtual XactDenialEnum                  NextDAT(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept;

            virtual XactDenialEnum                  Resend(FiredResponseFlit pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept;
            virtual bool                            RevertResent() noexcept;

            virtual bool                            IsComplete(const Topology& topo) const noexcept = 0;

        protected:
            virtual XactDenialEnum                  NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept = 0;
            virtual XactDenialEnum                  NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept = 0;

        protected:
            virtual XactDenialEnum                  NextRetryAckNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept;
        
            virtual XactDenialEnum                  ResendNoRecord(FiredResponseFlit pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept;
        
            virtual bool                            NextDataID(Flits::REQ<config, conn>::ssize_t, const FiredResponseFlit& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) noexcept;
            virtual bool                            NextREQDataID(const FiredResponseFlit& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) noexcept;
            virtual bool                            NextSNPDataID(const FiredResponseFlit& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionAllocatingRead : public Xaction<config, conn> {
        public:
            using FiredRequestFlit  = Xaction<config, conn>::FiredRequestFlit;
            using FiredResponseFlit = Xaction<config, conn>::FiredResponseFlit;

        public:
            XactionAllocatingRead(const Topology& topo, const FiredRequestFlit& first) noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;
            bool                            IsAckComplete(const Topology& topo) const noexcept;

            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

        protected:
            virtual XactDenialEnum          NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept override;
        };

        // TODO: XactionNonAllocatingRead

        // TODO: XactionImmediateWrite

        // TODO: XactionWriteZero

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionCopyBackWrite : public Xaction<config, conn> {
        public:
            using FiredRequestFlit  = Xaction<config, conn>::FiredRequestFlit;
            using FiredResponseFlit = Xaction<config, conn>::FiredResponseFlit;

        public:
            XactionCopyBackWrite(const Topology& topo, const FiredRequestFlit& first) noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;
            bool                            IsAckComplete(const Topology& topo) const noexcept;

            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept override;
        };

        // TODO: XactionCombinedImmediateWriteAndCMO

        // TODO: XactionCombinedImmediateWriteAndPersistCMO

        // TODO: XactionCombinedCopyBackWriteAndCMO

        // TODO: XactionAtomic

        // TODO: XactionStash

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionDataless : public Xaction<config, conn> {
        public:
            using FiredRequestFlit  = Xaction<config, conn>::FiredRequestFlit;
            using FiredResponseFlit = Xaction<config, conn>::FiredResponseFlit;

        public:
            XactionDataless(const Topology& topo, const FiredRequestFlit& first) noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsAckComplete(const Topology& topo) const noexcept;

            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept override;
        };

        // TODO: XactionPrefetch

        // TODO: XactionDVMOp

        // TODO: XactionHomeRead
        
        // TODO: XactionHomeWrite

        // TODO: XactionHomeWriteZero

        // TODO: XactionHomeCombinedWriteAndCMO

        // TODO: XactionHomeDataless

        // TODO: XactionHomeAtomic

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeSnoop : public Xaction<config, conn> {
        public:
            using FiredRequestFlit  = Xaction<config, conn>::FiredRequestFlit;
            using FiredResponseFlit = Xaction<config, conn>::FiredResponseFlit;

        public:
            XactionHomeSnoop(const Topology& topo, const FiredRequestFlit& first) noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept override;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionForwardSnoop : public Xaction<config, conn> {
        public:
            using FiredRequestFlit  = Xaction<config, conn>::FiredRequestFlit;
            using FiredResponseFlit = Xaction<config, conn>::FiredResponseFlit;

        public:
            XactionForwardSnoop(const Topology& topo, const FiredRequestFlit& first) noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            bool                            IsDCT() const noexcept;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept override;
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
        unsigned int requestSize = 1 << reqSize;

        if constexpr (config::dataWidth == 512)
            collectedDataID.set(0);

        if constexpr (config::dataWidth == 256)
        {
            if (requestSize <= 256)
                collectedDataID.set(0);
            else // 512
                collectedDataID.set(dataID & 0x02);
        }

        if constexpr (config::dataWidth == 128)
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
                collectedDataID |= CollectDataID<config, conn>(
                    reqSize, flit.flit.dat.DataID());

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
        unsigned int requestSize = 1 << reqSize;

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



// Implementation of: class Xaction
namespace /*CHI::*/Xact {
    /*
    FiredRequestFlit                first;
    XactDenialEnum                  firstDenial;
    std::vector<FiredResponseFlit>  subsequence;
    std::vector<XactDenialEnum>     subsequentDenial;
    Retry                           retry;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Xaction<config, conn>::Xaction(XactionType type, const FiredRequestFlit& first) noexcept
        : type              (type)
        , first             (first)
        , firstDenial       (XactDenial::ACCEPTED)
        , subsequence       ()
        , subsequenceKeys   ()
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionType Xaction<config, conn>::GetType() const noexcept
    {
        return type;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const Xaction<config, conn>::FiredRequestFlit& Xaction<config, conn>::GetFirst() const noexcept
    {
        return first;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const XactDenialEnum Xaction<config, conn>::GetFirstDenial() const noexcept
    {
        return firstDenial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const std::vector<typename Xaction<config, conn>::FiredResponseFlit>& Xaction<config, conn>::GetSubsequence() const noexcept
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
    inline const Xaction<config, conn>::FiredResponseFlit* Xaction<config, conn>::GetFirstRSP(
        std::initializer_list<typename Flits::RSP<config, conn>::opcode_t> opcodes) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (opcodes.size() == 0)
                return subsequence[index];

            for (auto opcode : opcodes)
                if (iter->opcode.rsp == opcode)
                    return subsequence[index];
        }

        return nullptr;
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const Xaction<config, conn>::FiredResponseFlit* Xaction<config, conn>::GetFirstDAT(
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> opcodes) const noexcept
    {
        size_t index = 0;
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++, index++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsDAT())
                continue;

            if (opcodes.size() == 0)
                return subsequence[index];

            for (auto opcode : opcodes)
                if (iter->opcode.dat == opcode)
                    return subsequence[index];
        }

        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const Xaction<config, conn>::FiredResponseFlit* Xaction<config, conn>::GetLastRSP(
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
                return subsequence[index];

            for (auto opcode : opcodes)
                if (iter->opcode.rsp == opcode)
                    return subsequence[index];
        }

        return nullptr;
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const Xaction<config, conn>::FiredResponseFlit* Xaction<config, conn>::GetLastDAT(
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
                return subsequence[index];

            for (auto opcode : opcodes)
                if (iter->opcode.dat == opcode)
                    return subsequence[index];
        }

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
    inline const Xaction<config, conn>::FiredResponseFlit* Xaction<config, conn>::GetRetryAck() const noexcept
    {
        return GetLastRSP(Opcodes::RSP::RetryAck);
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
    inline const Xaction<config, conn>::FiredResponseFlit& Xaction<config, conn>::GetPCreditSource() const noexcept
    {
        return sourcePCredit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::Next(const Topology& topo, const FiredResponseFlit& flit) noexcept
    {
        return flit.IsRSP() ? NextRSP(topo, flit) : NextDAT(topo, flit);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::NextRSP(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        if (!rspFlit.IsRSP()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL;

        XactDenialEnum denial = NextRSPNoRecord(topo, rspFlit);

        subsequence.push_back(rspFlit);
        subsequenceKeys.emplace_back(denial, rspFlit.flit.rsp);

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::NextDAT(const Topology& topo, const FiredResponseFlit& datFlit) noexcept
    {
        if (!datFlit.IsDAT()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL;

        XactDenialEnum denial = NextDATNoRecord(topo, datFlit);

        subsequence.push_back(datFlit);
        subsequenceKeys.emplace_back(denial, datFlit.flit.dat);

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::Resend(FiredResponseFlit pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept
    {
        XactDenialEnum denial = ResendNoRecord(pCrdFlit, xaction);

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
    inline XactDenialEnum Xaction<config, conn>::NextRetryAckNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() != Opcodes::RSP::RetryAck)
            return XactDenial::DENIED_OPCODE;

        if (!rspFlit.IsFromHomeToRequester(topo) && !rspFlit.IsFromSubordinateToHome(topo))
            return XactDenial::DENIED_COMMUNICATION;

        if (!this->first.flit.req.AllowRetry())
            return XactDenial::DENIED_RETRY_NO_ALLOWRETRY;

        if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
            return XactDenial::DENIED_TGTID_MISMATCH;

        if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
            return XactDenial::DENIED_TXNID_MISMATCH;

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::ResendNoRecord(FiredResponseFlit pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept
    {
        // *TODO: Do not check XactionType right now for KunminghuV2 workaround.
        /*
        if (xaction->GetType() != type)
            return XactDenial::DENIED_RETRY_DIFF_XACT_TYPE;
        */

        if (!pCrdFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (!xaction->GetFirst().flit.IsREQ())
            return XactDenial::DENIED_CHANNEL;

        auto retryAck = GetRetryAck();
        if (!retryAck)
            return XactDenial::DENIED_PCRD_NO_RETRY;

        if (pCrdFlit.flit.rsp.PCrdType() != retryAck->flit.rsp.PCrdType())
            return XactDenial::DENIED_PCRD_TYPE_MISMATCH;

        // TODO: check Fields Difference of retried request

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    bool Xaction<config, conn>::NextDataID(
        Flits::REQ<config, conn>::ssize_t                                   size, 
        const FiredResponseFlit&                                            datFlit,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>  opcodes) noexcept
    {
        std::bitset<4> collectedDataID = details::CollectDataID(
            size, this->subsequence, [&](size_t i, const FiredResponseFlit& flit) {

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

                return opcodeMatch && !this->subsequenceKeys[i].denial;
            });

        std::bitset<4> nextDataID = details::CollectDataID(
            size, datFlit.flit.dat.DataID());

        return (collectedDataID & nextDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    bool Xaction<config, conn>::NextREQDataID(
        const FiredResponseFlit&                                            datFlit,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>  opcodes) noexcept
    {
        return NextDataID(this->first.flit.req.Size(), datFlit, opcodes);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    bool Xaction<config, conn>::NextSNPDataID(
        const FiredResponseFlit&                                            datFlit,
        std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>  opcodes) noexcept
    {
        return NextDataID(Size<64>::value, datFlit, opcodes);
    }
}


// Implementation of: class XactionAllocatingRead
namespace /*CHI::*/Xact {
    /*
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionAllocatingRead<config, conn>::XactionAllocatingRead(const Topology& topo, const FiredRequestFlit& firstFlit) noexcept
        : Xaction<config, conn>(XactionType::AllocatingRead, firstFlit)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.flit.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::ReadClean
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadNotSharedDirty
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadShared
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadUnique
#ifdef CHI_ISSUE_EB_ENABLE
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadPreferUnique
         && this->first.flit.req.Opcode() != Opcodes::REQ::MakeReadUnique
#endif
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(topo))
        {
            this->firstDenial = XactDenial::DENIED_COMMUNICATION;
            return;
        }

        // TODO: go through Flit Field Checkers here
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->flit.IsToRequester(topo))
                continue;

            if (iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
                return true;

#ifdef CHI_ISSUE_EB_ENABLE
            if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
                return true;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // MakeReadUnique only
            if (this->first.flit.req.Opcode() == Opcodes::REQ::MakeReadUnique)
                if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->flit.IsToRequester(topo))
                continue;

            if (iter->flit.IsDAT() && 
#ifdef CHI_ISSUE_EB_ENABLE
                (iter->flit.dat.Opcode() == Opcodes::DAT::DataSepResp || 
#endif
                 iter->flit.dat.Opcode() == Opcodes::DAT::CompData))
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID(), [&](size_t i) { return !this->subsequenceKeys[i].denial; });
            }

#ifdef CHI_ISSUE_EB_ENABLE
            // MakeReadUnique only
            if (this->first.flit.req.Opcode() == Opcodes::REQ::MakeReadUnique)
                if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsAckComplete(const Topology& topo) const noexcept
    {
        size_t index = this->subsequence.size() - 1;
        for (auto iter = this->subsequence.rbegin(); iter != this->subsequence.rend(); iter++, index--)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->flit.IsRSP() || !iter->flit.IsToHome(topo))
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionAllocatingRead<config, conn>::NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(topo, rspFlit);
#ifdef CHI_ISSUE_EB_ENABLE
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RespSepData
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
        )
        {
            if (!rspFlit.IsToRequester(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
            {
                if (this->HasDAT(Opcodes::DAT::CompData))
                    return XactDenial::DENIED_RESPSEP_AFTER_COMPDATA;

                if (this->HasRSP(Opcodes::RSP::Comp))
                    return XactDenial::DENIED_RESPSEP_AFTER_COMP;
            }
            else // Comp
            {
                if (this->HasRSP(Opcodes::RSP::RespSepData))
                    return XactDenial::DENIED_COMP_AFTER_RESPSEP;

                if (this->HasDAT(Opcodes::DAT::DataSepResp))
                    return XactDenial::DENIED_COMP_AFTER_DATASEP;

                if (this->HasDAT(Opcodes::DAT::CompData))
                    return XactDenial::DENIED_COMP_AFTER_COMPDATA;
            }

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }
#endif

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionAllocatingRead<config, conn>::NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        // TODO: pass through Flit Field Checkers here

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData
#ifdef CHI_ISSUE_EB_ENABLE
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp
#endif
        )
        {
            if (!datFlit.IsToRequester(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (datFlit.flit.dat.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                if (this->HasDAT(Opcodes::DAT::DataSepResp))
                    return XactDenial::DENIED_COMPDATA_AFTER_DATASEP;

                if (this->HasRSP(Opcodes::RSP::RespSepData))
                    return XactDenial::DENIED_COMPDATA_AFTER_RESPSEP;

                if (this->HasRSP(Opcodes::RSP::Comp))
                    return XactDenial::DENIED_COMPDATA_AFTER_COMP;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                if (this->HasDAT(Opcodes::DAT::CompData))
                    return XactDenial::DENIED_DATASEP_AFTER_COMPDATA;

                if (this->HasRSP(Opcodes::RSP::Comp))
                    return XactDenial::DENIED_DATASEP_AFTER_COMP;
            }
#endif

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;
            
            //
            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionCopyBackWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionCopyBackWrite<config, conn>::XactionCopyBackWrite(const Topology& topo, const FiredRequestFlit& firstFlit) noexcept
        : Xaction<config, conn>(XactionType::CopyBackWrite, firstFlit)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.flit.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteBackPtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteBackFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteCleanFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictFull
#ifdef CHI_ISSUE_EB_ENABLE
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict
#endif
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(topo))
        {
            this->firstDenial = XactDenial::DENIED_COMMUNICATION;
            return;
        }

        // TODO: go through Flit Field Checkers here
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->flit.IsToRequester(topo))
                continue;

            if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
                return true;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvictOnly
            if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
                if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
                if (iter->flit.IsRSP() && iter->flit.IsFromHome(topo) && iter->flit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif

            if (!iter->flit.IsFromRequester(topo))
                continue;

            if (iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsAckComplete(const Topology& topo) const noexcept
    {
#ifdef CHI_ISSUE_EB_ENABLE
        // WriteEvictOrEvict only
        if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
        {
            size_t index = this->subsequence.size() - 1;
            for (auto iter = this->subsequence.rbegin(); iter != this->subsequence.rend(); iter++, index--)
            {
                if (this->subsequenceKeys[index].IsDenied())
                    continue;

                if (iter->flit.IsRSP() && iter->flit.IsFromHome(topo) && iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
                    return true;

                if (!iter->flit.IsToHome(topo))
                    continue;

                if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                    return true;
            }
        }
        else
            return true;

        return false;
#else
        return true;
#endif
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionCopyBackWrite<config, conn>::NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(topo, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
#endif
        )
        {
#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                    return XactDenial::DENIED_OPCODE;
#endif

            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }
#ifdef CHI_ISSUE_EB_ENABLE
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck
        )
        {
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (!this->HasRSP(Opcodes::RSP::Comp))
                return XactDenial::DENIED_COMPACK_BEFORE_COMP;

            if (this->HasRSP(Opcodes::RSP::CompDBIDResp))
                return XactDenial::DENIED_COMPACK_AFTER_DBIDRESP;

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }
#endif

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionCopyBackWrite<config, conn>::NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
        {
            if (!datFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                if (this->HasRSP(Opcodes::RSP::Comp))
                    return XactDenial::DENIED_DATA_AFTER_COMP;
#endif

            if (!this->HasRSP(Opcodes::RSP::CompDBIDResp))
                return XactDenial::DENIED_DATA_BEFORE_DBID;

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            //
            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionDataless
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionDataless<config, conn>::XactionDataless(const Topology& topo, const FiredRequestFlit& first) noexcept
        : Xaction<config, conn>(XactionType::Dataless, first)
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
            this->firstDenial = XactDenial::DENIED_COMMUNICATION;
            return;
        }

        // TODO: go through Flit Field Checkers here
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
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
            if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                return true;

            if (this->HasRSP({ Opcodes::RSP::Comp }) && this->HasRSP({ Opcodes::RSP::Persist }))
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
    inline bool XactionDataless<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return IsResponseComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDataless<config, conn>::NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(topo, rspFlit);
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
                        return XactDenial::DENIED_COMMUNICATION;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    // TODO: pass through Flit Field Checkers here

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
                        return XactDenial::DENIED_COMMUNICATION;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    // TODO: pass through Flit Field Checkers here

                    return XactDenial::ACCEPTED;   
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                {
                    if (!rspFlit.IsFromRequesterToHome(topo))
                        return XactDenial::DENIED_COMMUNICATION;

                    if (this->HasRSP({ Opcodes::RSP::CompAck }))
                        return XactDenial::DENIED_COMPACK_AFTER_COMPACK;

                    auto lastComp = this->GetLastRSP({ Opcodes::RSP::Comp });
                    if (!lastComp)
                        return XactDenial::DENIED_COMPACK_BEFORE_COMP;

                    if (rspFlit.flit.rsp.TgtID() != lastComp->flit.rsp.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;

                    if (rspFlit.flit.rsp.TxnID() != lastComp->flit.rsp.DBID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    // TODO: pass through Flit Field Checkers here

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
                        return XactDenial::DENIED_COMMUNICATION;
                    
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
                    
                    // TODO: pass through Flit Field Checkers here

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_COMMUNICATION;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;

                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_COMP_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    // TODO: pass through Flit Field Checkers here

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Persist)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo) && !rspFlit.IsFromSubordinateToRequester(topo))
                        return XactDenial::DENIED_COMMUNICATION;
                    
                    if (this->HasRSP({ Opcodes::RSP::Persist }))
                        return XactDenial::DENIED_PERSIST_AFTER_PERSIST;
                    
                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_PERSIST_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.PGroupID() != this->first.flit.req.PGroupID())
                        return XactDenial::DENIED_PGROUPID_MISMATCH;
                    
                    // TODO: pass through Flit Field Checkers here

                    return XactDenial::ACCEPTED;
                }
            }
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDataless<config, conn>::NextDATNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


// Implementation of: class XactionHomeSnoop
namespace /*CHI::*/Xact  {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeSnoop<config, conn>::XactionHomeSnoop(const Topology& topo, const FiredRequestFlit& first) noexcept
        : Xaction<config, conn>(XactionType::HomeSnoop, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsSNP())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.snp.Opcode() != Opcodes::SNP::SnpShared
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpClean
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpOnce
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpNotSharedDirty
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpUnique
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpPreferUnique
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpCleanShared
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpCleanInvalid
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpMakeInvalid
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpUniqueStash
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpMakeInvalidStash
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpStashUnique
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpStashShared
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpQuery
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(topo))
        {
            this->firstDenial = XactDenial::DENIED_COMMUNICATION;
            return;
        }

        // TODO: go through Flit Field Checkers here
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp
             || iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(Size<64>::value);

        bool needCollectedSnpDataID = false;

        std::bitset<4> collectedSnpDataID;
        
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                return true;

            if (iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
            {
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Size<64>::value, iter->flit.dat.DataID());
                continue;
            }
        }

        return ((completeDataIDMask & ~collectedSnpDataID).none() || !needCollectedSnpDataID);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return IsResponseComplete(topo) && IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeSnoop<config, conn>::NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
        {
            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({ Opcodes::RSP::SnpResp }))
                return XactDenial::DENIED_DUPLICATED_SNPRESP;

            if (this->HasDAT({ 
                    Opcodes::DAT::SnpRespData, 
                    Opcodes::DAT::SnpRespDataPtl }))
                return XactDenial::DENIED_DUPLICATED_SNPRESP;

            // TODO: check response state combination here

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeSnoop<config, conn>::NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;
        
        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
        )
        {
            if (!datFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (!this->NextSNPDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            // TODO: check reponse state combination here

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionForwardSnoop
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionForwardSnoop<config, conn>::XactionForwardSnoop(const Topology& topo, const FiredRequestFlit& first) noexcept
        : Xaction<config, conn>(XactionType::ForwardSnoop, first)
    {
        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsSNP())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.snp.Opcode() != Opcodes::SNP::SnpSharedFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpCleanFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpOnceFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpNotSharedDirtyFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpPreferUniqueFwd
         && this->first.flit.snp.Opcode() != Opcodes::SNP::SnpUniqueFwd
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToRequester(topo))
        {
            this->firstDenial = XactDenial::DENIED_COMMUNICATION;
            return;
        }

        // TODO: go through Flit Field Checkers here
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp
             || iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
             || iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded
             || iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(Size<64>::value);
        
        bool needCollectedSnpDataID = false;
        bool needCollectedFwdDataID = false;

        std::bitset<4> collectedSnpDataID;
        std::bitset<4> collectedFwdDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                return true;
            else if (iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
            {
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Size<64>::value, iter->flit.dat.DataID());
                continue;
            }
            else if (iter->flit.IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
            {
                needCollectedFwdDataID = true;
                continue;
            }
            else if (iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
            {
                needCollectedFwdDataID = true;
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Size<64>::value, iter->flit.dat.DataID());
                continue;
            }
            else if (iter->flit.IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                collectedFwdDataID |= details::CollectDataID<config, conn>(
                    Size<64>::value, iter->flit.dat.DataID());
                continue;
            }
        }

        return ((completeDataIDMask & ~collectedSnpDataID).none() || !needCollectedSnpDataID)
            && ((completeDataIDMask & ~collectedFwdDataID).none() || !needCollectedFwdDataID);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return IsResponseComplete(topo) && IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDCT() const noexcept
    {
        return this->HasRSP({ Opcodes::RSP::SnpRespFwded }) || this->HasDAT({ Opcodes::DAT::SnpRespDataFwded });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionForwardSnoop<config, conn>::NextRSPNoRecord(const Topology& topo, const FiredResponseFlit& rspFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
        {
            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({
                    Opcodes::RSP::SnpResp,
                    Opcodes::RSP::SnpRespFwded}))
                return XactDenial::DENIED_DUPLICATED_SNPRESP;

            if (this->HasDAT({
                    Opcodes::DAT::SnpRespData, 
                    Opcodes::DAT::SnpRespDataPtl,
                    Opcodes::DAT::SnpRespDataFwded}))
                return XactDenial::DENIED_DUPLICATED_SNPRESP;
            
            // TODO: check response state combination here

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionForwardSnoop<config, conn>::NextDATNoRecord(const Topology& topo, const FiredResponseFlit& datFlit) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
        {
            if (!datFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (!this->NextSNPDataID(datFlit, {
                Opcodes::DAT::SnpRespData,
                Opcodes::DAT::SnpRespDataPtl,
                Opcodes::DAT::SnpRespDataFwded
            }))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            // TODO: check response state combination here

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }
        else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
        {
            if (!datFlit.IsToRequesterDCT(topo))
                return XactDenial::DENIED_COMMUNICATION;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.FwdNID())
                return XactDenial::DENIED_FWDNID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.FwdTxnID())
                return XactDenial::DENIED_FWDTXNID_MISMATCH;

            if (datFlit.flit.dat.DBID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_DBID_MISMATCH;

            if (datFlit.flit.dat.HomeNID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_HOMENID_MISMATCH;

            if (!this->NextSNPDataID(datFlit, {
                Opcodes::DAT::CompData
            }))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            // TODO: check response state combination here

            // TODO: pass through Flit Field Checkers here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}





#endif // __CHI__CHI_XACT_XACTIONS_*

//#endif // __CHI__CHI_XACT_XACTIONS
