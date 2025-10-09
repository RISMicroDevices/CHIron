//#pragma once

//#ifndef __CHI__CHI_XACT_XACTIONS
//#define __CHI__CHI_XACT_XACTIONS

#ifndef CHI_XACT_XACTIONS__STANDALONE
#   include "chi_xactions_header.hpp"               // IWYU pragma: keep
#   include "chi_xact_base_header.hpp"              // IWYU pragma: keep
#   include "chi_xact_base.hpp"                     // IWYU pragma: keep
#   include "chi_xact_checkers_field_header.hpp"    // IWYU pragma: keep
#   include "chi_xact_checkers_field.hpp"           // IWYU pragma: keep
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

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class Global {
        public:
            RequestFieldMappingChecker<config, conn>    reqFieldMappingChecker;
            SnoopFieldMappingChecker<config, conn>      snpFieldMappingChecker;
            ResponseFieldMappingChecker<config, conn>   rspFieldMappingChecker;
            DataFieldMappingChecker<config, conn>       datFieldMappingChecker;
        };

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
            CopyBackWrite,
            Atomic,
            Dataless,
            HomeRead,
            HomeWrite,
            HomeWriteZero,
            HomeDataless,
            HomeAtomic,
            HomeSnoop,
            ForwardSnoop
        };


        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class Xaction {
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

            const FiredResponseFlit<config, conn>*  GetFirstRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetFirstDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            bool                                    GotDBID() const noexcept;
            std::optional<typename Flits::RSP<config, conn>::dbid_t>
                                                    GetDBID() const noexcept;
            const FiredResponseFlit<config, conn>*  GetDBIDSource() const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDBIDSourceRSP(std::initializer_list<typename Flits::RSP<config, conn>::opcode_t>) const noexcept;
            const FiredResponseFlit<config, conn>*  GetLastDBIDSourceDAT(std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) const noexcept;

            bool                                    GotRetryAck() const noexcept;
            const FiredResponseFlit<config, conn>*  GetRetryAck() const noexcept;

            bool                                    IsResent() const noexcept;
            std::shared_ptr<Xaction<config, conn>>  GetResentXaction() const noexcept;
            const FiredResponseFlit<config, conn>&  GetPCreditSource() const noexcept;

            virtual XactDenialEnum                  Next(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& flit, bool& hasDBID, bool& firstDBID) noexcept;
            virtual XactDenialEnum                  NextRSP(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept;
            virtual XactDenialEnum                  NextDAT(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept;

            virtual XactDenialEnum                  Resend(Global<config, conn>* glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept;
            virtual bool                            RevertResent() noexcept;

            virtual bool                            IsTxnIDComplete(const Topology& topo) const noexcept = 0;
            virtual bool                            IsDBIDComplete(const Topology& topo) const noexcept = 0;
            virtual bool                            IsComplete(const Topology& topo) const noexcept = 0;

            // *NOTICE: Responses with both valid TxnID and DBID like Comp could be out-of-order on interconnect
            //          and came after DBID grant (e.g. DBIDResp, CompDBIDResp).
            virtual bool                            IsDBIDOverlappable(const Topology& topo) const noexcept = 0;

        protected:
            virtual XactDenialEnum                  NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;
            virtual XactDenialEnum                  NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;

        protected:
            virtual XactDenialEnum                  NextRetryAckNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit) noexcept;
        
            virtual XactDenialEnum                  ResendNoRecord(Global<config, conn>* glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept;
        
            virtual bool                            NextDataID(Flits::REQ<config, conn>::ssize_t, const FiredResponseFlit<config, conn>& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t>) noexcept;
            virtual bool                            NextREQDataID(const FiredResponseFlit<config, conn>& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> = {}) noexcept;
            virtual bool                            NextSNPDataID(const FiredResponseFlit<config, conn>& datFlit, std::initializer_list<typename Flits::DAT<config, conn>::opcode_t> = {}) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionAllocatingRead : public Xaction<config, conn> {
        public:
            XactionAllocatingRead(Global<config, conn>*                     glbl, 
                                  const Topology&                           topo, 
                                  const FiredRequestFlit<config, conn>&     first,
                                  std::shared_ptr<Xaction<config, conn>>    retried = nullptr) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>          Clone() const noexcept override;
            std::shared_ptr<XactionAllocatingRead<config, conn>>    CloneAsIs() const noexcept;

        public:
            bool                            IsAckComplete(const Topology& topo) const noexcept;
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            
            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        protected:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionNonAllocatingRead : public Xaction<config, conn> {
        public:
            XactionNonAllocatingRead(Global<config, conn>*                  glbl,
                                     const Topology&                        topo,
                                     const FiredRequestFlit<config, conn>&  first,
                                     std::shared_ptr<Xaction<config, conn>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>          Clone() const noexcept override;
            std::shared_ptr<XactionNonAllocatingRead<config, conn>> CloneAsIs() const noexcept;
        
        public:
            bool                            IsAckComplete(const Topology& topo) const noexcept;
            bool                            IsResponseComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionImmediateWrite : public Xaction<config, conn> {
        public:
            XactionImmediateWrite(Global<config, conn>*                     glbl,
                                  const Topology&                           topo,
                                  const FiredRequestFlit<config, conn>&     first,
                                  std::shared_ptr<Xaction<config, conn>>    retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>          Clone() const noexcept override;
            std::shared_ptr<XactionImmediateWrite<config, conn>>    CloneAsIs() const noexcept;

        public:
            bool                            IsDataComplete(const Topology& topo) const noexcept;
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsAckComplete(const Topology& topo) const noexcept;
            bool                            IsTagOpComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        // TODO: XactionWriteZero

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionCopyBackWrite : public Xaction<config, conn> {
        public:
            XactionCopyBackWrite(Global<config, conn>*                  glbl,
                                 const Topology&                        topo,
                                 const FiredRequestFlit<config, conn>&  first,
                                 std::shared_ptr<Xaction<config, conn>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionCopyBackWrite<config, conn>> CloneAsIs() const noexcept;

        public:
            bool                            IsDataComplete(const Topology& topo) const noexcept;
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsAckComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        // TODO: XactionCombinedImmediateWriteAndCMO

        // TODO: XactionCombinedImmediateWriteAndPersistCMO

        // TODO: XactionCombinedCopyBackWriteAndCMO

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionAtomic : public Xaction<config, conn> {
        public:
            XactionAtomic(Global<config, conn>*                     glbl,
                          const Topology&                           topo,
                          const FiredRequestFlit<config, conn>&     first,
                          std::shared_ptr<Xaction<config, conn>>    retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>  Clone() const noexcept override;
            std::shared_ptr<XactionAtomic<config, conn>>    CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsRNDataComplete(const Topology& topo) const noexcept;
            bool                            IsHNDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        // TODO: XactionStash

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionDataless : public Xaction<config, conn> {
        public:
            XactionDataless(Global<config, conn>*                   glbl,
                            const Topology&                         topo,
                            const FiredRequestFlit<config, conn>&   first,
                            std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>  Clone() const noexcept override;
            std::shared_ptr<XactionDataless<config, conn>>  CloneAsIs() const noexcept;

        public:
            bool                            IsCompResponseComplete(const Topology& topo) const noexcept;
            bool                            IsPersistResponseComplete(const Topology& topo) const noexcept;
            bool                            IsAckComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        // TODO: XactionPrefetch

        // TODO: XactionDVMOp

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeRead : public Xaction<config, conn> {
        public:
            XactionHomeRead(Global<config, conn>*                   glbl,
                            const Topology&                         topo, 
                            const FiredRequestFlit<config, conn>&   first,
                            std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeRead<config, conn>>  CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };
        
        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeWrite : public Xaction<config, conn> {
        public:
            XactionHomeWrite(Global<config, conn>*                  glbl,
                             const Topology&                        topo, 
                             const FiredRequestFlit<config, conn>&  first,
                             std::shared_ptr<Xaction<config, conn>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeWrite<config, conn>> CloneAsIs() const noexcept;

        public:
            bool                            IsDBIDResponseComplete(const Topology& topo) const noexcept;
            bool                            IsCompResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeWriteZero : public Xaction<config, conn> {
        public:
            XactionHomeWriteZero(Global<config, conn>*                  glbl,
                                 const Topology&                        topo,
                                 const FiredRequestFlit<config, conn>&  first,
                                 std::shared_ptr<Xaction<config, conn>> retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeWriteZero<config, conn>> CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

            public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        // TODO: XactionHomeCombinedWriteAndCMO

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeDataless : public Xaction<config, conn> {
        public:
            XactionHomeDataless(Global<config, conn>*                   glbl,
                                const Topology&                         topo,
                                const FiredRequestFlit<config, conn>&   first,
                                std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeDataless<config, conn>>  CloneAsIs() const noexcept;

        public:
            bool                            IsCompResponseComplete(const Topology& topo) const noexcept;
            bool                            IsPersistResponseComplete(const Topology& topo) const noexcept;
        
            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeAtomic : public Xaction<config, conn> {
        public:
            XactionHomeAtomic(Global<config, conn>*                   glbl,
                              const Topology&                         topo,
                              const FiredRequestFlit<config, conn>&   first,
                              std::shared_ptr<Xaction<config, conn>>  retried) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionHomeAtomic<config, conn>>    CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsHNDataComplete(const Topology& topo) const noexcept;
            bool                            IsSNDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionHomeSnoop : public Xaction<config, conn> {
        public:
            XactionHomeSnoop(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>  Clone() const noexcept override;
            std::shared_ptr<XactionHomeSnoop<config, conn>> CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class XactionForwardSnoop : public Xaction<config, conn> {
        public:
            XactionForwardSnoop(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& first) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config, conn>>      Clone() const noexcept override;
            std::shared_ptr<XactionForwardSnoop<config, conn>>  CloneAsIs() const noexcept;

        public:
            bool                            IsResponseComplete(const Topology& topo) const noexcept;
            bool                            IsDataComplete(const Topology& topo) const noexcept;

            virtual bool                    IsTxnIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsDBIDComplete(const Topology& topo) const noexcept override;
            virtual bool                    IsComplete(const Topology& topo) const noexcept override;

            virtual bool                    IsDBIDOverlappable(const Topology& topo) const noexcept override;

            bool                            IsDCT() const noexcept;

        public:
            virtual XactDenialEnum          NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept override;
            virtual XactDenialEnum          NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept override;
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
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstRSP(
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
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetFirstDAT(
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
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastRSP(
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
                return &(subsequence[index]);

            for (auto opcode : opcodes)
                if (iter->opcode.rsp == opcode)
                    return &(subsequence[index]);
        }

        return nullptr;
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const FiredResponseFlit<config, conn>* Xaction<config, conn>::GetLastDAT(
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
    inline XactDenialEnum Xaction<config, conn>::Next(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& flit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return flit.IsRSP() ? NextRSP(glbl, topo, flit, hasDBID, firstDBID) : NextDAT(glbl, topo, flit, hasDBID, firstDBID);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::NextRSP(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!rspFlit.IsRSP()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL;

        XactDenialEnum denial = NextRSPNoRecord(glbl, topo, rspFlit, hasDBID, firstDBID);

        subsequence.push_back(rspFlit);
        subsequenceKeys.emplace_back(denial, rspFlit.flit.rsp.Opcode(), hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::NextDAT(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!datFlit.IsDAT()) [[unlikely]]
            return XactDenial::DENIED_CHANNEL;

        XactDenialEnum denial = NextDATNoRecord(glbl, topo, datFlit, hasDBID, firstDBID);

        subsequence.push_back(datFlit);
        subsequenceKeys.emplace_back(denial, datFlit.flit.dat.Opcode(), hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::Resend(Global<config, conn>* glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept
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
    inline XactDenialEnum Xaction<config, conn>::NextRetryAckNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit) noexcept
    {
        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() != Opcodes::RSP::RetryAck)
            return XactDenial::DENIED_OPCODE;

        if (!rspFlit.IsFromHomeToRequester(topo) && !rspFlit.IsFromSubordinateToHome(topo))
            return XactDenial::DENIED_COMMUNICATION;

        if (!this->subsequence.empty())
            return XactDenial::DENIED_RETRY_ON_ACTIVE_PROGRESS;

        if (!this->first.flit.req.AllowRetry())
            return XactDenial::DENIED_RETRY_NO_ALLOWRETRY;

        if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
            return XactDenial::DENIED_TGTID_MISMATCH;

        if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
            return XactDenial::DENIED_TXNID_MISMATCH;

        //
        if (glbl)
        {
            XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
            if (denial != XactDenial::ACCEPTED)
                return denial;
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Xaction<config, conn>::ResendNoRecord(Global<config, conn>* glbl, FiredResponseFlit<config, conn> pCrdFlit, std::shared_ptr<Xaction<config, conn>> xaction) noexcept
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
        if (glbl)
        {
            RequestFieldMapping fields 
                = glbl->reqFieldMappingChecker.table.Get(this->first.flit.req.Opcode());

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
                    if (MemAttr::ExtractAllocate(origin.MemAttr()) != MemAttr::ExtractAllocate(retry.MemAttr()))
                        return XactDenial::DENIED_RETRY_DIFF_MEMATTR;

                if (FieldTrait::IsApplicable(fields->Cacheable))
                    if (MemAttr::ExtractCacheable(origin.MemAttr()) != MemAttr::ExtractCacheable(retry.MemAttr()))
                        return XactDenial::DENIED_RETRY_DIFF_MEMATTR;

                if (FieldTrait::IsApplicable(fields->Device))
                    if (MemAttr::ExtractDevice(origin.MemAttr()) != MemAttr::ExtractDevice(retry.MemAttr()))
                        return XactDenial::DENIED_RETRY_DIFF_MEMATTR;
                
                if (FieldTrait::IsApplicable(fields->EWA))
                    if (MemAttr::ExtractEWA(origin.MemAttr()) != MemAttr::ExtractEWA(origin.MemAttr()))
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
        return NextDataID(Size<64>::value, datFlit, opcodes);
    }
}


// Implementation of: class XactionAllocatingRead
namespace /*CHI::*/Xact {
    /*
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionAllocatingRead<config, conn>::XactionAllocatingRead(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   firstFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::AllocatingRead, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
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
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionAllocatingRead<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionAllocatingRead<config, conn>> XactionAllocatingRead<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionAllocatingRead<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        bool gotResp = false;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(topo))
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
            {
                gotResp = true;
            }
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
                
                gotResp = true;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            // MakeReadUnique only
            else if (this->first.flit.req.Opcode() == Opcodes::REQ::MakeReadUnique)
            {
                if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
            }
#endif
        }

        return gotResp && (completeDataIDMask & ~collectedDataID).none();
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

            if (!iter->IsRSP() || !iter->IsToHome(topo))
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAllocatingRead<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionAllocatingRead<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
#ifdef CHI_ISSUE_EB_ENABLE
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RespSepData
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
        )
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
            {
                if (this->HasRSP({ Opcodes::RSP::RespSepData }))
                    return XactDenial::DENIED_RESPSEP_AFTER_RESPSEP;

                if (this->HasDAT({ Opcodes::DAT::CompData }))
                    return XactDenial::DENIED_RESPSEP_AFTER_COMPDATA;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_RESPSEP_AFTER_COMP;
            }
            else // Comp
            {
                if (this->first.flit.req.Opcode() != Opcodes::REQ::MakeReadUnique)
                    return XactDenial::DENIED_OPCODE;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;

                if (this->HasRSP({ Opcodes::RSP::RespSepData }))
                    return XactDenial::DENIED_COMP_AFTER_RESPSEP;

                if (this->HasDAT({ Opcodes::DAT::DataSepResp }))
                    return XactDenial::DENIED_COMP_AFTER_DATASEP;

                if (this->HasDAT({ Opcodes::DAT::CompData }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDATA;
            }

            auto optDBID = this->GetDBID();
            if (optDBID)
            {
                if (rspFlit.flit.rsp.DBID() != *optDBID)
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
#endif
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
        {
            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (
                !this->HasDAT({ Opcodes::DAT::CompData })
#ifdef CHI_ISSUE_EB_ENABLE
             && !this->HasRSP({ Opcodes::RSP::RespSepData })
#endif
            )
#ifdef CHI_ISSUE_EB_ENABLE
                return XactDenial::DENIED_COMPACK_BEFORE_COMPDATA_OR_RESPSEPDATA;
#else
                return XactDenial::DENIED_COMPACK_BEFORE_COMPDATA;
#endif

            auto optDBID = this->GetDBID();

            if (!optDBID)
                return XactDenial::DENIED_COMPACK_BEFORE_DBID;

            if (rspFlit.flit.rsp.TxnID() != *optDBID)
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionAllocatingRead<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData
#ifdef CHI_ISSUE_EB_ENABLE
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp
#endif
        )
        {
            if (!datFlit.IsToRequester(topo))
                return XactDenial::DENIED_DAT_NOT_TO_RN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                if (this->HasDAT({ Opcodes::DAT::DataSepResp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_DATASEP;

                if (this->HasRSP({ Opcodes::RSP::RespSepData }))
                    return XactDenial::DENIED_COMPDATA_AFTER_RESPSEP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_COMP;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                if (this->HasDAT({ Opcodes::DAT::CompData }))
                    return XactDenial::DENIED_DATASEP_AFTER_COMPDATA;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_DATASEP_AFTER_COMP;
            }
#endif

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

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
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }
            
            //
            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionNonAllocatingRead
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionNonAllocatingRead<config, conn>::XactionNonAllocatingRead(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   firstFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::NonAllocatingRead, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::ReadNoSnp
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadOnce
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadOnceCleanInvalid
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadOnceMakeInvalid 
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionNonAllocatingRead<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionNonAllocatingRead<config, conn>> XactionNonAllocatingRead<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionNonAllocatingRead<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionNonAllocatingRead<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        bool gotResp = false;
        bool gotReceipt = this->first.flit.req.Order() == 0;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(topo))
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
            {
                gotResp = true;
            }
            else if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::ReadReceipt)
            {
                gotReceipt = true;
            }
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                gotResp = true;

                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
#endif
        }

        return gotResp && gotReceipt && (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionNonAllocatingRead<config, conn>::IsAckComplete(const Topology& topo) const noexcept
    {
        if (!this->first.flit.req.ExpCompAck())
            return true;
            
        size_t index = this->subsequence.size() - 1;
        for (auto iter = this->subsequence.rbegin(); iter != this->subsequence.rend(); iter++, index--)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsRSP() || !iter->IsToHome(topo))
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionNonAllocatingRead<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionNonAllocatingRead<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionNonAllocatingRead<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionNonAllocatingRead<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionNonAllocatingRead<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::ReadReceipt)
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({ Opcodes::RSP::ReadReceipt }))
                return XactDenial::DENIED_DUPLICATED_READRECEIPT;

            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
#ifdef CHI_ISSUE_EB_ENABLE
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RespSepData)
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({ Opcodes::RSP::RespSepData }))
                return XactDenial::DENIED_RESPSEP_AFTER_RESPSEP;

            if (this->HasDAT({ Opcodes::DAT::CompData }))
                return XactDenial::DENIED_RESPSEP_AFTER_COMPDATA;

            auto optDBID = this->GetDBID();
            if (optDBID)
            {
                if (rspFlit.flit.rsp.DBID() != *optDBID)
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
#endif

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionNonAllocatingRead<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData
#ifdef CHI_ISSUE_EB_ENABLE
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp
#endif
        )
        {
            if (!datFlit.IsToRequester(topo))
                return XactDenial::DENIED_DAT_NOT_TO_RN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                if (this->HasDAT({ Opcodes::DAT::DataSepResp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_DATASEP;

                if (this->HasRSP({ Opcodes::RSP::RespSepData }))
                    return XactDenial::DENIED_COMPDATA_AFTER_RESPSEP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_COMP;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                if (this->HasDAT({ Opcodes::DAT::CompData }))
                    return XactDenial::DENIED_DATASEP_AFTER_COMPDATA;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_DATASEP_AFTER_COMP;
            }
#endif

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

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
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionImmediateWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionImmediateWrite<config, conn>::XactionImmediateWrite(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   firstFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::ImmediateWrite, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpPtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniquePtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniqueFull
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniquePtlStash
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteUniqueFullStash
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromRequesterToHome(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionImmediateWrite<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionImmediateWrite<config, conn>> XactionImmediateWrite<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionImmediateWrite<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        bool gotDBID = false;
        bool gotComp = false;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(topo) || !iter->IsRSP())
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            {
                gotDBID = true;
                gotComp = true;
                break;
            }
            else if (
                iter->flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
             || iter->flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd
#endif
            )
            {
                gotDBID = true;

                if (gotComp)
                    break;
            }
            else if (iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                gotComp = true;

                if (gotDBID)
                    break;
            }
        }

        return gotDBID && gotComp;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());
        
        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsFromRequester(topo) || !iter->IsDAT())
                continue;

            if (
                iter->flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData
#ifdef CHI_ISSUE_EB_ENABLE
             || iter->flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck
#endif
            )
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
            else if (iter->flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
                return true;
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsAckComplete(const Topology& topo) const noexcept
    {
        if (!this->first.flit.req.ExpCompAck())
            return true;

        size_t index = this->subsequence.size() - 1;
        for (auto iter = this->subsequence.rbegin(); iter != this->subsequence.rend(); iter++, index--)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToHome(topo))
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                return true;

#ifdef CHI_ISSUE_EB_ENABLE
            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck)
                return true;
#endif
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsTagOpComplete(const Topology& topo) const noexcept
    {
        if (this->first.flit.req.TagOp() != TagOp::MatchFetch)
            return true;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index--)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsToRequester(topo) || !iter->IsRSP())
                continue;

            if (iter->flit.rsp.Opcode() == Opcodes::RSP::TagMatch)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsDataComplete(topo) && IsAckComplete(topo) && IsTagOpComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionImmediateWrite<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionImmediateWrite<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;

            if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

            hasDBID = true;
            firstDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Immediate Write transactions");

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
    
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (
                    true
#ifdef CHI_ISSUE_EB_ENABLE
                 && !this->first.flit.req.DoDWT() // Never check DBID of Comp on DWT
#endif
                )
                {
                    if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                        return XactDenial::DENIED_DBID_MISMATCH;
                }
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd
#endif
        )
        {
            if (!rspFlit.IsToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_TO_RN;

            if (rspFlit.IsFromSubordinate(topo))
            {
                if (this->first.flit.req.ExpCompAck())
                    return XactDenial::DENIED_DWT_ON_EXPCOMPACK;

#ifdef CHI_ISSUE_EB_ENABLE
                if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd)
                    return XactDenial::DENIED_DWT_WITH_DBIDRESPORD;
#endif
            }

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Immediate Write transactions");

                if (this->HasRSP({ Opcodes::RSP::DBIDResp, Opcodes::RSP::DBIDRespOrd }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;
                
                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::TagMatch)
        {
            // TODO: MTE features currently not supported

            return XactDenial::DENIED_UNSUPPORTED_FEATURE_OPCODE;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
        {
            if (!this->first.flit.req.ExpCompAck())
                return XactDenial::DENIED_COMPACK_ON_NON_EXPCOMPACK;

            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Immediate Write transactions");

                if (rspFlit.flit.rsp.TgtID() != optDBIDSource->flit.rsp.SrcID())
                    return XactDenial::DENIED_TGTID_MISMATCH;

                if (rspFlit.flit.rsp.TxnID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_TXNID_MISMATCH;
            }
            else
                return XactDenial::DENIED_COMPACK_BEFORE_DBID;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionImmediateWrite<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData
#ifdef CHI_ISSUE_EB_ENABLE
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck
#endif
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel
        )
        {
            const FiredResponseFlit<config, conn>* optDBIDSource 
                = this->GetLastDBIDSourceRSP({ 
                    Opcodes::RSP::DBIDResp,
                    Opcodes::RSP::DBIDRespOrd, 
                    Opcodes::RSP::CompDBIDResp });

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
            {
                if (this->HasDAT({ Opcodes::DAT::NCBWrDataCompAck }))
                    return XactDenial::DENIED_NCBWRDATA_AFTER_NCBWRDATACOMPACK;

                if (this->HasDAT({ Opcodes::DAT::WriteDataCancel }))
                    return XactDenial::DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL;

                if (!this->NextREQDataID(datFlit))
                    return XactDenial::DENIED_DUPLICATED_DATAID;

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck)
            {
                if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                    return XactDenial::DENIED_NCBWRDATACOMPACK_AFTER_NCBWRDATA;

                if (this->HasDAT({ Opcodes::DAT::WriteDataCancel }))
                    return XactDenial::DENIED_NCBWRDATACOMPACK_AFTER_WRITEDATACANCEL;

                if (!this->NextREQDataID(datFlit))
                    return XactDenial::DENIED_DUPLICATED_DATAID;

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
#endif
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
            {
                if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                    return XactDenial::DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA;

                if (this->HasDAT({ Opcodes::DAT::NCBWrDataCompAck }))
                    return XactDenial::DENIED_WRITEDATACANCEL_AFTER_NCBWRDATACOMPACK;

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionCopyBackWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionCopyBackWrite<config, conn>::XactionCopyBackWrite(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   firstFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::CopyBackWrite, firstFlit, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
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
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(firstFlit.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionCopyBackWrite<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionCopyBackWrite<config, conn>> XactionCopyBackWrite<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionCopyBackWrite<config, conn>>(*this);
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

            if (!iter->IsToRequester(topo))
                continue;

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
                return true;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvictOnly
            if (this->first.flit.req.Opcode() == Opcodes::REQ::WriteEvictOrEvict)
                if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
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
                if (iter->IsRSP() && iter->IsFromHome(topo) && iter->flit.rsp.Opcode() == Opcodes::RSP::Comp)
                    return true;
#endif

            if (!iter->IsFromRequester(topo))
                continue;

            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
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

                if (iter->IsRSP() && iter->IsFromHome(topo) && iter->flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
                    return true;

                if (!iter->IsToHome(topo))
                    continue;

                if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::CompAck)
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
    inline bool XactionCopyBackWrite<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsDataComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionCopyBackWrite<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionCopyBackWrite<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
#endif
        )
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

#ifdef CHI_ISSUE_EB_ENABLE
                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;
#endif
            }
#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                    return XactDenial::DENIED_OPCODE;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
                
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;
            }
#endif

            hasDBID = true;
            firstDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

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
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

            if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                return XactDenial::DENIED_COMPACK_AFTER_DBIDRESP;

            auto optDBID = this->GetDBID();

            if (!optDBID)
                return XactDenial::DENIED_COMPACK_BEFORE_COMP;

            if (rspFlit.flit.rsp.TxnID() != *optDBID)
                return XactDenial::DENIED_TXNID_MISMATCH;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
#endif

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionCopyBackWrite<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CopyBackWrData)
        {
            if (!datFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

#ifdef CHI_ISSUE_EB_ENABLE
            // WriteEvictOrEvict only
            if (this->first.flit.req.Opcode() != Opcodes::REQ::WriteEvictOrEvict)
                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_DATA_AFTER_COMP;
#endif

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            assert(optDBIDSource->IsRSP() &&
                "DBID never comes from DAT channel in CopyBack Write transactions");

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            //
            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionAtomic
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionAtomic<config, conn>::XactionAtomic(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::Atomic, first, retried)
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

        if (!this->first.IsFromRequesterToHome(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionAtomic<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionAtomic<config, conn>> XactionAtomic<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionAtomic<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAtomic<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAtomic<config, conn>::IsRNDataComplete(const Topology& topo) const noexcept
    {
        if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
            return true;

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAtomic<config, conn>::IsHNDataComplete(const Topology& topo) const noexcept
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
    inline bool XactionAtomic<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsHNDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAtomic<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsRNDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAtomic<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsHNDataComplete(topo) && IsRNDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionAtomic<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            return false;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            return IsRNDataComplete(topo);
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionAtomic<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;
        
        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
#ifdef CHI_ISSUE_EB_ENABLE
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDRespOrd
#endif
        )
        {
            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

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
                        return XactDenial::DENIED_DBID_MISMATCH;
                }
                else
                {
                    assert(optDBIDSource->flit.dat.Opcode() == Opcodes::DAT::CompData &&
                        "DBID source on RXDAT must be CompData in Atomic transactions");

                    if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.dat.DBID())
                        return XactDenial::DENIED_DBID_MISMATCH;
                }
            }
            else
                firstDBID = true;

            hasDBID = true;
            
            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in AtomicStore Atomic transactions");

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
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

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
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionAtomic<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
        {
            if (!datFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

            const FiredResponseFlit<config, conn>* optDBIDSource;

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
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                return XactDenial::DENIED_NCBWRDATA_AFTER_NCBWRDATA;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
        {
            if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;

            if (!datFlit.IsFromHomeToRequester(topo))
                return XactDenial::DENIED_DAT_NOT_FROM_HN_TO_RN;

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
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionDataless
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionDataless<config, conn>::XactionDataless(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
        : Xaction<config, conn>(XactionType::Dataless, first, retried)
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
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionDataless<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionDataless<config, conn>> XactionDataless<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionDataless<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsCompResponseComplete(const Topology& topo) const noexcept
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsPersistResponseComplete(const Topology& topo) const noexcept
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
    inline bool XactionDataless<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(topo) && IsPersistResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(topo) && IsPersistResponseComplete(topo) && IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionDataless<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return IsAckComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDataless<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
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
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
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
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    hasDBID = true;
                    firstDBID = true;

                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
                {
                    if (!rspFlit.IsFromRequesterToHome(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

                    if (this->HasRSP({ Opcodes::RSP::CompAck }))
                        return XactDenial::DENIED_COMPACK_AFTER_COMPACK;

                    auto lastComp = this->GetLastRSP({ Opcodes::RSP::Comp });
                    if (!lastComp)
                        return XactDenial::DENIED_COMPACK_BEFORE_COMP;

                    if (rspFlit.flit.rsp.TgtID() != lastComp->flit.rsp.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;

                    if (rspFlit.flit.rsp.TxnID() != lastComp->flit.rsp.DBID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
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
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
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
                    
                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Comp }))
                        return XactDenial::DENIED_COMP_AFTER_COMP;

                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_COMP_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                        return XactDenial::DENIED_TXNID_MISMATCH;

                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
                else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Persist)
                {
                    if (!rspFlit.IsFromHomeToRequester(topo) && !rspFlit.IsFromSubordinateToRequester(topo))
                        return XactDenial::DENIED_RSP_NOT_TO_RN;
                    
                    if (this->HasRSP({ Opcodes::RSP::Persist }))
                        return XactDenial::DENIED_PERSIST_AFTER_PERSIST;
                    
                    if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                        return XactDenial::DENIED_PERSIST_AFTER_COMPPERSIST;
                    
                    if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                        return XactDenial::DENIED_TGTID_MISMATCH;
                    
                    if (rspFlit.flit.rsp.PGroupID() != this->first.flit.req.PGroupID())
                        return XactDenial::DENIED_PGROUPID_MISMATCH;
                    
                    //
                    if (glbl)
                    {
                        XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                        if (denial != XactDenial::ACCEPTED)
                            return denial;
                    }

                    return XactDenial::ACCEPTED;
                }
            }
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionDataless<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


// Implementation of: class XactionHomeRead
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeRead<config, conn>::XactionHomeRead(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried
    ) noexcept
        : Xaction<config, conn>(XactionType::HomeRead, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::ReadNoSnp
#ifdef CHI_ISSUE_EB_ENABLE
         && this->first.flit.req.Opcode() != Opcodes::REQ::ReadNoSnpSep
#endif
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeRead<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeRead<config, conn>> XactionHomeRead<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeRead<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeRead<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        if (this->first.flit.req.Order() != 0)
            return this->HasRSP({ Opcodes::RSP::ReadReceipt });

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeRead<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());

        std::bitset<4> collectedDataID;

        size_t index = 0;
        for (auto iter = this->subsequence.begin(); iter != this->subsequence.end(); iter++, index++)
        {
            if (this->subsequenceKeys[index].IsDenied())
                continue;

            if (!iter->IsFromSubordinate(topo))
                continue;

            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                collectedDataID |= details::CollectDataID<config, conn>(
                    this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
#endif
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeRead<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return IsComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeRead<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return IsComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeRead<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeRead<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeRead<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::ReadReceipt)
        {
            if (!rspFlit.IsFromSubordinateToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->first.flit.req.Order() == 0)
                return XactDenial::DENIED_READRECEIPT_ON_NO_ORDER;

            if (this->HasRSP({ Opcodes::RSP::ReadReceipt }))
                return XactDenial::DENIED_READRECEIPT_AFTER_READRECEIPT;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeRead<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData
#ifdef CHI_ISSUE_EB_ENABLE
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp
#endif
        )
        {
            if (!datFlit.IsFromSubordinate(topo))
                return XactDenial::DENIED_DAT_NOT_FROM_SN;

            if (datFlit.flit.dat.TgtID() == this->first.flit.req.SrcID())
            {
                if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_TXNID_MISMATCH;
            }
            else if (datFlit.flit.dat.TgtID() == this->first.flit.req.ReturnNID())
            {
                if (datFlit.flit.dat.TxnID() != this->first.flit.req.ReturnTxnID())
                    return XactDenial::DENIED_TXNID_MISMATCH;
            }
            else
                return XactDenial::DENIED_DAT_NOT_TO_HN_OR_RN;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
            {
                if (this->HasDAT({ Opcodes::DAT::DataSepResp }))
                    return XactDenial::DENIED_COMPDATA_AFTER_DATASEP;
            }
#ifdef CHI_ISSUE_EB_ENABLE
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::DataSepResp)
            {
                if (this->HasDAT({ Opcodes::DAT::CompData }))
                    return XactDenial::DENIED_DATASEP_AFTER_COMPDATA;
            }
#endif

            if (!this->NextREQDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionHomeWrite
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeWrite<config, conn>::XactionHomeWrite(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried
    ) noexcept
        : Xaction<config, conn>(XactionType::HomeWrite, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpPtl
         && this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpFull
        )
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeWrite<config, conn>::Clone() const noexcept
    {
        return static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeWrite<config, conn>> XactionHomeWrite<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeWrite<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWrite<config, conn>::IsDBIDResponseComplete(const Topology& topo) const noexcept
    {
        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (iter->opcode.rsp == Opcodes::RSP::DBIDResp
             || iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
                return true;
        }
        
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWrite<config, conn>::IsCompResponseComplete(const Topology& topo) const noexcept
    {
        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;
            
            if (iter->opcode.rsp == Opcodes::RSP::Comp
             || iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWrite<config, conn>::IsDataComplete(const Topology& topo) const noexcept
    {
        std::bitset<4> completeDataIDMask =
            details::GetDataIDCompleteMask<config, conn>(this->first.flit.req.Size());

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
                collectedDataID |= details::CollectDataID<config, conn>(
                        this->first.flit.req.Size(), iter->flit.dat.DataID());
            }
            else if (iter->flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
                return true;
        }

        return (completeDataIDMask & ~collectedDataID).none();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWrite<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsDBIDResponseComplete(topo) && IsCompResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWrite<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWrite<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsDBIDResponseComplete(topo) && IsCompResponseComplete(topo) && IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWrite<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return IsDBIDResponseComplete(topo) && IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeWrite<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!rspFlit.IsFromSubordinateToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (this->HasRSP({ Opcodes::RSP::Comp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;

            if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

            hasDBID = true;
            firstDBID = true;
    
            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }
    
            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!rspFlit.IsFromSubordinateToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Home Write transactions");

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
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp)
        {
#ifdef CHI_ISSUE_EB_ENABLE
            if (this->first.flit.req.DoDWT())
            {
                if (!rspFlit.IsFromSubordinateToRequester(topo) && !rspFlit.IsFromSubordinateToHome(topo))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN;

                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.ReturnNID())
                    return XactDenial::DENIED_TGTID_MISMATCH;
                    
                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.ReturnTxnID())
                    return XactDenial::DENIED_TXNID_MISMATCH;
            }
            else
#endif
            {
                if (!rspFlit.IsFromSubordinateToHome(topo))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                    return XactDenial::DENIED_TGTID_MISMATCH;

                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_TXNID_MISMATCH;
            }

            const FiredResponseFlit<config, conn>* optDBIDSource = this->GetDBIDSource();

            if (optDBIDSource)
            {
                assert(optDBIDSource->IsRSP() &&
                    "DBID never comes from DAT channel in Home Write transactions");

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;

                if (rspFlit.flit.rsp.DBID() != optDBIDSource->flit.rsp.DBID())
                    return XactDenial::DENIED_DBID_MISMATCH;
            }
            else
                firstDBID = true;

            hasDBID = true;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeWrite<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (
            datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData
         || datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel
        )
        {
            const FiredResponseFlit<config, conn>* optDBIDSource
                = this->GetLastDBIDSourceRSP({
                    Opcodes::RSP::DBIDResp,
                    Opcodes::RSP::CompDBIDResp });

            if (!optDBIDSource)
                return XactDenial::DENIED_DATA_BEFORE_DBIDRESP;

            if (datFlit.flit.dat.TgtID() != optDBIDSource->flit.rsp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != optDBIDSource->flit.rsp.DBID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
            {
                if (this->HasDAT({ Opcodes::DAT::WriteDataCancel }))
                    return XactDenial::DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL;

                if (!this->NextREQDataID(datFlit))
                    return XactDenial::DENIED_DUPLICATED_DATAID;

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
            else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::WriteDataCancel)
            {
                if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
                    return XactDenial::DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA;

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionHomeWriteZero
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeWriteZero<config, conn>::XactionHomeWriteZero(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried
    ) noexcept
        : Xaction<config, conn>(XactionType::HomeWriteZero, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
            return;
        }

        if (
            this->first.flit.req.Opcode() != Opcodes::REQ::WriteNoSnpZero
        ) [[unlikely]]
        {
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeWriteZero<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeWriteZero<config, conn>> XactionHomeWriteZero<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeWriteZero<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
    {
        bool gotDBID = false;
        bool gotComp = false;

        for (auto iter = this->subsequenceKeys.begin(); iter != this->subsequenceKeys.end(); iter++)
        {
            if (iter->IsDenied())
                continue;

            if (!iter->IsRSP())
                continue;

            if (iter->opcode.rsp == Opcodes::RSP::CompDBIDResp)
            {
                return true;
            }
            else if (iter->opcode.rsp == Opcodes::RSP::DBIDResp)
            {
                gotDBID = true;

                if (gotComp)
                    return true;
            }
            else if (iter->opcode.rsp == Opcodes::RSP::Comp)
            {
                gotComp = true;

                if (gotDBID)
                    return true;
            }
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeWriteZero<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeWriteZero<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (
            rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp
         || rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp
        )
        {
            if (!rspFlit.IsFromSubordinateToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

            if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_DBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMPDBIDRESP_AFTER_COMP;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::DBIDResp }))
                    return XactDenial::DENIED_DBIDRESP_AFTER_DBIDRESP;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                if (this->HasRSP({ Opcodes::RSP::CompDBIDResp }))
                    return XactDenial::DENIED_COMP_AFTER_COMPDBIDRESP;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeWriteZero<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hashDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


// Implementation of: class XactionHomeDataless
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeDataless<config, conn>::XactionHomeDataless(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   first,
        std::shared_ptr<Xaction<config, conn>>  retried
    ) noexcept
        : Xaction<config, conn>(XactionType::HomeDataless, first, retried)
    {
        // *NOTICE: AllowRetry should be checked by external scoreboards.

        this->firstDenial = XactDenial::ACCEPTED;

        if (!this->first.IsREQ())
        {
            this->firstDenial = XactDenial::DENIED_CHANNEL;
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
            this->firstDenial = XactDenial::DENIED_OPCODE;
            return;
        }

        if (!this->first.IsFromHomeToSubordinate(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(first.flit.req);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeDataless<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeDataless<config, conn>> XactionHomeDataless<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeDataless<config, conn>>(*this);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeDataless<config, conn>::IsCompResponseComplete(const Topology& topo) const noexcept
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeDataless<config, conn>::IsPersistResponseComplete(const Topology& topo) const noexcept
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeDataless<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(topo) && IsPersistResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeDataless<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeDataless<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsCompResponseComplete(topo) && IsPersistResponseComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeDataless<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeDataless<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
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
                if (!rspFlit.IsFromSubordinateToHome(topo))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;

                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                    return XactDenial::DENIED_TGTID_MISMATCH;

                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_TXNID_MISMATCH;

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
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
                if (!rspFlit.IsFromSubordinateToHome(topo))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

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

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
            {
                if (!rspFlit.IsFromSubordinateToHome(topo))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

                if (this->HasRSP({ Opcodes::RSP::Comp }))
                    return XactDenial::DENIED_COMP_AFTER_COMP;

                if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                    return XactDenial::DENIED_COMP_AFTER_COMPPERSIST;

                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                    return XactDenial::DENIED_TGTID_MISMATCH;
                
                if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
                    return XactDenial::DENIED_TXNID_MISMATCH;

                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                    if (denial != XactDenial::ACCEPTED)
                        return denial;
                }

                return XactDenial::ACCEPTED;
            }
            else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Persist)
            {
                if (!rspFlit.IsFromSubordinateToHome(topo))
                    return XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN;

                if (this->HasRSP({ Opcodes::RSP::Persist }))
                    return XactDenial::DENIED_PERSIST_AFTER_PERSIST;
                
                if (this->HasRSP({ Opcodes::RSP::CompPersist }))
                    return XactDenial::DENIED_PERSIST_AFTER_COMPPERSIST;
                
                if (rspFlit.flit.rsp.TgtID() != this->first.flit.req.SrcID())
                    return XactDenial::DENIED_TGTID_MISMATCH;
                
                if (rspFlit.flit.rsp.PGroupID() != this->first.flit.req.PGroupID())
                    return XactDenial::DENIED_PGROUPID_MISMATCH;
                
                //
                if (glbl)
                {
                    XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
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
    inline XactDenialEnum XactionHomeDataless<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


// Implementation of: class XactionHomeAtomic
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeAtomic<config, conn>::XactionHomeAtomic(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
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

        if (!this->first.IsFromHomeToSubordinate(topo))
        {
            this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->reqFieldMappingChecker.Check(first.flit.req);
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
    inline bool XactionHomeAtomic<config, conn>::IsResponseComplete(const Topology& topo) const noexcept
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
    inline bool XactionHomeAtomic<config, conn>::IsHNDataComplete(const Topology& topo) const noexcept
    {
        if (this->HasDAT({ Opcodes::DAT::NonCopyBackWrData }))
            return true;

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsSNDataComplete(const Topology& topo) const noexcept
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
    inline bool XactionHomeAtomic<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsSNDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsHNDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return this->GotRetryAck() || IsResponseComplete(topo) && IsHNDataComplete(topo) && IsSNDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeAtomic<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        /* AtomicStore */
        if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
        {
            return false;
        }
        /* AtomicLoad/Swap/Compare */
        else
        {
            return IsHNDataComplete(topo);
        }

        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeAtomic<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
            return this->NextRetryAckNoRecord(glbl, topo, rspFlit);
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::DBIDResp)
        {
            if (!rspFlit.IsFromSubordinateToHome(topo))
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
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromSubordinateToHome(topo))
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
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
        {
            if (!Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;

            if (!rspFlit.IsFromSubordinateToHome(topo))
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
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeAtomic<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!datFlit.IsDAT())
            return XactDenial::DENIED_CHANNEL;

        if (datFlit.flit.dat.Opcode() == Opcodes::DAT::NonCopyBackWrData)
        {
            if (!datFlit.IsFromHomeToSubordinate(topo))
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
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }
        else if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
        {
            if (Opcodes::REQ::AtomicStore::Is(this->first.flit.req.Opcode()))
                return XactDenial::DENIED_OPCODE;
            
            if (!datFlit.IsFromSubordinateToHome(topo))
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
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionHomeSnoop
namespace /*CHI::*/Xact  {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionHomeSnoop<config, conn>::XactionHomeSnoop(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& first) noexcept
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

        if (!this->first.IsFromHomeToRequester(topo))
        {
            this->firstDenial = XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN;
            return;
        }

        //
        if (glbl)
        {
            this->firstDenial = glbl->snpFieldMappingChecker.Check(first.flit.snp);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionHomeSnoop<config, conn>::Clone() const noexcept
    {
        return static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionHomeSnoop<config, conn>> XactionHomeSnoop<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionHomeSnoop<config, conn>>(*this);
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

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
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

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                return true;

            if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
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
    inline bool XactionHomeSnoop<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return IsComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return IsComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return IsResponseComplete(topo) && IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionHomeSnoop<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeSnoop<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (this->IsComplete(topo))
            return XactDenial::DENIED_COMPLETED;

        if (!rspFlit.IsRSP())
            return XactDenial::DENIED_CHANNEL;

        if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
        {
            if (!rspFlit.IsFromRequesterToHome(topo))
                return XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN;

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

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            // TODO: check response state combination here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionHomeSnoop<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
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
                return XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN;

            if (datFlit.flit.dat.TgtID() != this->first.flit.snp.SrcID())
                return XactDenial::DENIED_TGTID_MISMATCH;

            if (datFlit.flit.dat.TxnID() != this->first.flit.snp.TxnID())
                return XactDenial::DENIED_TXNID_MISMATCH;

            if (!this->NextSNPDataID(datFlit))
                return XactDenial::DENIED_DUPLICATED_DATAID;

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            // TODO: check reponse state combination here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}


// Implementation of: class XactionForwardSnoop
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactionForwardSnoop<config, conn>::XactionForwardSnoop(Global<config, conn>* glbl, const Topology& topo, const FiredRequestFlit<config, conn>& first) noexcept
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

        //
        if (glbl)
        {
            this->firstDenial = glbl->snpFieldMappingChecker.Check(first.flit.snp);
            if (this->firstDenial != XactDenial::ACCEPTED)
                return;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> XactionForwardSnoop<config, conn>::Clone() const noexcept
    {
        return std::static_pointer_cast<Xaction<config, conn>>(CloneAsIs());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<XactionForwardSnoop<config, conn>> XactionForwardSnoop<config, conn>::CloneAsIs() const noexcept
    {
        return std::make_shared<XactionForwardSnoop<config, conn>>(*this);
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

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
             || iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded
             || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
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

            if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpResp)
                return true;
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespData
                  || iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl)
            {
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Size<64>::value, iter->flit.dat.DataID());
                continue;
            }
            else if (iter->IsRSP() && iter->flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
            {
                needCollectedFwdDataID = true;
                continue;
            }
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
            {
                needCollectedFwdDataID = true;
                needCollectedSnpDataID = true;
                collectedSnpDataID |= details::CollectDataID<config, conn>(
                    Size<64>::value, iter->flit.dat.DataID());
                continue;
            }
            else if (iter->IsDAT() && iter->flit.dat.Opcode() == Opcodes::DAT::CompData)
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
    inline bool XactionForwardSnoop<config, conn>::IsTxnIDComplete(const Topology& topo) const noexcept
    {
        return IsComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDBIDComplete(const Topology& topo) const noexcept
    {
        return IsComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsComplete(const Topology& topo) const noexcept
    {
        return IsResponseComplete(topo) && IsDataComplete(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDBIDOverlappable(const Topology& topo) const noexcept
    {
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool XactionForwardSnoop<config, conn>::IsDCT() const noexcept
    {
        return this->HasRSP({ Opcodes::RSP::SnpRespFwded }) || this->HasDAT({ Opcodes::DAT::SnpRespDataFwded });
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionForwardSnoop<config, conn>::NextRSPNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& rspFlit, bool& hasDBID, bool& firstDBID) noexcept
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

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->rspFieldMappingChecker.Check(rspFlit.flit.rsp);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }
            
            // TODO: check response state combination here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum XactionForwardSnoop<config, conn>::NextDATNoRecord(Global<config, conn>* glbl, const Topology& topo, const FiredResponseFlit<config, conn>& datFlit, bool& hasDBID, bool& firstDBID) noexcept
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
            
            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            // TODO: check response state combination here

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
            
            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->datFieldMappingChecker.Check(datFlit.flit.dat);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            // TODO: check response state combination here

            return XactDenial::ACCEPTED;
        }

        return XactDenial::DENIED_OPCODE;
    }
}



#endif // __CHI__CHI_XACT_XACTIONS_*

//#endif // __CHI__CHI_XACT_XACTIONS
