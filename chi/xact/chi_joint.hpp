//#pragma once

//#ifndef __CHI__CHI_XACT_JOINT
//#define __CHI__CHI_XACT_JOINT

#include "chi_xactions/includes.hpp"
#ifndef CHI_XACT_JOINT__STANDALONE
#   include "chi_joint_header.hpp"              // IWYU pragma: keep
#   include "chi_xactions.hpp"                  // IWYU pragma: keep
#   include "../util/chi_util_decoding.hpp"     // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_JOINT_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_JOINT_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_JOINT_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_JOINT_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {

        //
        template<FlitConfigurationConcept config>
        class Joint;


        // Joint Denial Events
        enum class JointDenialSource {
            XACTION,
            JOINT
        };

        template<FlitConfigurationConcept config>
        class JointDenialEventBase {
        protected:
            Joint<config>&                      joint;
            std::shared_ptr<Xaction<config>>    xaction;
            XactDenialEnum                      denial;
            JointDenialSource                   denialSource;

        public:
            JointDenialEventBase(Joint<config>&                     joint,
                                 std::shared_ptr<Xaction<config>>   xaction,
                                 XactDenialEnum                     denial,
                                 JointDenialSource                  source) noexcept;

        public:
            Joint<config>&                          GetJoint() noexcept;
            const Joint<config>&                    GetJoint() const noexcept;

            std::shared_ptr<Xaction<config>>        GetXaction() noexcept;
            std::shared_ptr<const Xaction<config>>  GetXaction() const noexcept;

            XactDenialEnum                          GetDenial() const noexcept;
            JointDenialSource                       GetDenialSource() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointDeniedRequestEvent : public JointDenialEventBase<config>,
                                        public Gravity::Event<JointDeniedRequestEvent<config>> {
        protected:
            FiredRequestFlit<config>&   firedRequestFlit;

        public:
            JointDeniedRequestEvent(Joint<config>&                      joint,
                                    std::shared_ptr<Xaction<config>>    xaction,
                                    XactDenialEnum                      denial,
                                    JointDenialSource                   source,
                                    FiredRequestFlit<config>&           firedRequestFlit) noexcept;
        
        public:
            FiredRequestFlit<config>&       GetFiredFlit() noexcept;
            const FiredRequestFlit<config>& GetFiredFlit() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointDeniedResponseEvent : public JointDenialEventBase<config>,
                                         public Gravity::Event<JointDeniedResponseEvent<config>> {
        protected:
            FiredResponseFlit<config>&  firedResponseFlit;

        public:
            JointDeniedResponseEvent(Joint<config>&                     joint,
                                     std::shared_ptr<Xaction<config>>   xaction,
                                     XactDenialEnum                     denial,
                                     JointDenialSource                  source,
                                     FiredResponseFlit<config>&         firedResponseFlit) noexcept;

        public:
            FiredResponseFlit<config>&          GetFiredFlit() noexcept;
            const FiredResponseFlit<config>&    GetFiredFlit() const noexcept;                
        };


        // Joint Xaction Events
        template<FlitConfigurationConcept config>
        class JointXactionEventBase {
        protected:
            Joint<config>&                      joint;
            std::shared_ptr<Xaction<config>>    xaction;

        public:
            JointXactionEventBase(Joint<config>&                    joint, 
                                  std::shared_ptr<Xaction<config>>  xaction) noexcept;

        public:
            Joint<config>&                          GetJoint() noexcept;
            const Joint<config>&                    GetJoint() const noexcept;

            std::shared_ptr<Xaction<config>>        GetXaction() noexcept;
            std::shared_ptr<const Xaction<config>>  GetXaction() const noexcept;
        };

        template<FlitConfigurationConcept       config>
        class JointXactionAcceptedEvent : public JointXactionEventBase<config>, 
                                          public Gravity::Event<JointXactionAcceptedEvent<config>> {
        public:
            JointXactionAcceptedEvent(Joint<config>&                      joint, 
                                      std::shared_ptr<Xaction<config>>    xaction) noexcept;
                                    
            JointXactionAcceptedEvent(Joint<config>&                      joint,
                                      std::shared_ptr<Xaction<config>>    xaction,
                                      Flits::REQ<config>::srcid_t         snpTgtId) noexcept;

        private:
            Flits::REQ<config>::srcid_t   snoopTargetID;

        public:
            Flits::REQ<config>::srcid_t   GetSnoopTargetID() const noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointXactionRetryEvent : public JointXactionEventBase<config>,
                                       public Gravity::Event<JointXactionRetryEvent<config>> {
        public:
            JointXactionRetryEvent(Joint<config>&                     joint, 
                                   std::shared_ptr<Xaction<config>>   xaction) noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointXactionTxnIDAllocationEvent : public JointXactionEventBase<config>,
                                                 public Gravity::Event<JointXactionTxnIDAllocationEvent<config>> {
        public:
            JointXactionTxnIDAllocationEvent(Joint<config>&                      joint,
                                             std::shared_ptr<Xaction<config>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointXactionTxnIDFreeEvent : public JointXactionEventBase<config>,
                                           public Gravity::Event<JointXactionTxnIDFreeEvent<config>> {
        public:
            JointXactionTxnIDFreeEvent(Joint<config>&                      joint,
                                       std::shared_ptr<Xaction<config>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointXactionDBIDAllocationEvent : public JointXactionEventBase<config>,
                                                public Gravity::Event<JointXactionDBIDAllocationEvent<config>> {
        public:
            JointXactionDBIDAllocationEvent(Joint<config>&                      joint,
                                            std::shared_ptr<Xaction<config>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointXactionDBIDFreeEvent : public JointXactionEventBase<config>,
                                          public Gravity::Event<JointXactionDBIDFreeEvent<config>> {
        public:
            JointXactionDBIDFreeEvent(Joint<config>&                      joint,
                                      std::shared_ptr<Xaction<config>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept config>
        class JointXactionCompleteEvent : public JointXactionEventBase<config>,
                                          public Gravity::Event<JointXactionCompleteEvent<config>> {
        public:
            JointXactionCompleteEvent(Joint<config>&                      joint,
                                      std::shared_ptr<Xaction<config>>    xaction) noexcept;

            JointXactionCompleteEvent(Joint<config>&                      joint,
                                      std::shared_ptr<Xaction<config>>    xaction,
                                      Flits::REQ<config>::srcid_t         snpTgtId) noexcept;

        private:
            Flits::REQ<config>::srcid_t   snoopTargetID;

        public:
            Flits::REQ<config>::srcid_t   GetSnoopTargetID() const noexcept;
        };


        // Joint Base
        template<FlitConfigurationConcept config>
        class Joint {
        public:
            class EventHub {
            public:
                Gravity::EventBus<JointDeniedRequestEvent<config>>          OnDeniedRequest;
                Gravity::EventBus<JointDeniedResponseEvent<config>>         OnDeniedResponse;

                Gravity::EventBus<JointXactionAcceptedEvent<config>>        OnAccepted;
                Gravity::EventBus<JointXactionRetryEvent<config>>           OnRetry;
                Gravity::EventBus<JointXactionTxnIDAllocationEvent<config>> OnTxnIDAllocation;
                Gravity::EventBus<JointXactionTxnIDFreeEvent<config>>       OnTxnIDFree;
                Gravity::EventBus<JointXactionDBIDAllocationEvent<config>>  OnDBIDAllocation;
                Gravity::EventBus<JointXactionDBIDFreeEvent<config>>        OnDBIDFree;
                Gravity::EventBus<JointXactionCompleteEvent<config>>        OnComplete;

            public:
                EventHub() noexcept;
                void Clear() noexcept;
            };

            std::shared_ptr<EventHub> events;

        public:
            Joint() noexcept;

        public:
            virtual void            Clear() noexcept = 0;

            virtual XactScopeEnum   GetActiveScope() const noexcept = 0;

        protected:
            XactDenialEnum  RequestDeniedByJoint(XactDenialEnum                   denial,
                                                 FiredRequestFlit<config>&        firedRequestFlit,
                                                 std::shared_ptr<Xaction<config>> xaction = nullptr) noexcept;

            XactDenialEnum  RequestDeniedByXaction(XactDenialEnum                     denial,
                                                   FiredRequestFlit<config>&          firedRequestFlit,
                                                   std::shared_ptr<Xaction<config>>   xaction = nullptr) noexcept;

            XactDenialEnum  ResponseDeniedByJoint(XactDenialEnum                      denial,
                                                  FiredResponseFlit<config>&          firedResponseFlit,
                                                  std::shared_ptr<Xaction<config>>    xaction = nullptr) noexcept;

            XactDenialEnum  ResponseDeniedByXaction(XactDenialEnum                    denial,
                                                    FiredResponseFlit<config>&        firedResponseFlit,
                                                    std::shared_ptr<Xaction<config>>  xaction = nullptr) noexcept;

        public:
            virtual XactDenialEnum  NextTXREQ(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::REQ<config>&         reqFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextRXREQ(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::REQ<config>&         reqFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextTXSNP(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::SNP<config>&         snpFlit,
                const Flits::REQ<config>::srcid_t snpTgtId,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;
            
            virtual XactDenialEnum  NextRXSNP(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::SNP<config>&         snpFlit,
                const Flits::REQ<config>::srcid_t snpTgtId,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextTXRSP(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::RSP<config>&         rspFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextRXRSP(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::RSP<config>&         rspFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;
            
            virtual XactDenialEnum  NextTXDAT(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::DAT<config>&         datFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextRXDAT(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::DAT<config>&         rspFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept;
        };


        // RN-F Joint
        template<FlitConfigurationConcept config>
        class RNFJoint : public Joint<config> {
        public:
            using txreqid_t = union __txreqid_t {
                struct {
                    Flits::REQ<config>::txnid_t   txn;
                    Flits::REQ<config>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using txreqdbid_t = union __txreqdbid_t {
                struct {
                    Flits::RSP<config>::dbid_t    db;     // same for DAT
                    Flits::RSP<config>::srcid_t   src;    // same for DAT
                    Flits::RSP<config>::tgtid_t   tgt;    // same for DAT
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using txreqdbidovlp_t = union __txreqdbidovlp_t {
                struct {
                    Flits::RSP<config>::dbid_t    db;     // same for DAT
                    Flits::RSP<config>::srcid_t   src;    // same for DAT
                    Flits::RSP<config>::tgtid_t   tgt;    // same for DAT
                    Flits::RSP<config>::txnid_t   txn;    // same for DAT
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rxsnpid_t = union __rxsnpid_t {
                struct {
                    Flits::SNP<config>::txnid_t   txn;
                    Flits::SNP<config>::srcid_t   src;
                    Flits::REQ<config>::srcid_t   tgt;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };
            
            using pcrd_t = union __pcrd_t {
                struct {
                    Flits::RSP<config>::tgtid_t   tgt;
                    Flits::RSP<config>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rnsrc_t = union __rnsrc_t {
                struct {
                    Flits::REQ<config>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

        private:
            using GetXaction = std::function<std::shared_ptr<Xaction<config>>(
                const Global<config>&,
                const FiredRequestFlit<config>&,
                std::shared_ptr<Xaction<config>>)>;

        private:
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>        txTransactions;
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>        rxTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>        txDBIDTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>        txDBIDOverlappableTransactions;

            std::unordered_map<uint64_t, std::list<std::shared_ptr<Xaction<config>>>>
                                                                                  txRetriedTransactions;

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config>>>    grantedPCredits[1 << Flits::RSP<config>::PCRDTYPE_WIDTH];

            Opcodes::REQ::Decoder<Flits::REQ<config>, GetXaction>                 reqDecoder;
            Opcodes::SNP::Decoder<Flits::SNP<config>, GetXaction>                 snpDecoder;

        protected:
            static std::shared_ptr<Xaction<config>>   ConstructNone(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructAllocatingRead(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructNonAllocatingRead(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructImmediateWrite(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructWriteZero(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructCopyBackWrite(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructDataless(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructHomeSnoop(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructForwardSnoop(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructAtomic(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>>   ConstructIndependentStash(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;

        public:
            RNFJoint() noexcept;

        public:
            RNFJoint(const RNFJoint<config>& obj) noexcept;
            RNFJoint<config>& operator=(const RNFJoint<config>& obj) noexcept;

        public:
            void                    Fork() noexcept;

        public:
            virtual void            Clear() noexcept override;

            virtual XactScopeEnum   GetActiveScope() const noexcept override;

            virtual void            GetInflight(
                const Global<config>&                             glbl,
                std::vector<std::shared_ptr<Xaction<config>>>&    dstVector,
                bool                                              sortByTime = false
            ) const noexcept;

        public:
            virtual XactDenialEnum  NextTXREQ(
                const Global<config>&             glbl,
                uint64_t                           time,
                const Flits::REQ<config>&         reqFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextRXSNP(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::SNP<config>&         snpFlit,
                const Flits::REQ<config>::srcid_t snpTgtId,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextTXRSP(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::RSP<config>&         rspFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextRXRSP(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::RSP<config>&         rspFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextTXDAT(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::DAT<config>&         datFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextRXDAT(
                const Global<config>&             glbl,
                uint64_t                          time,
                const Flits::DAT<config>&         datFlit,
                std::shared_ptr<Xaction<config>>* xaction = nullptr
            ) noexcept override;
        };


        // SN-F Joint
        template<FlitConfigurationConcept config>
        class SNFJoint : public Joint<config> {
        public:
            using rxreqid_t = union __rxreqid_t {
                struct {
                    Flits::REQ<config>::txnid_t   txn;
                    Flits::REQ<config>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rxreqdbid_t = union __rxreqdbid_t {
                struct {
                    Flits::RSP<config>::dbid_t    db;
                    Flits::RSP<config>::srcid_t   src;
                    Flits::RSP<config>::tgtid_t   tgt;
                }           id; 
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rxreqdbidovlp_t = union __rxreqdbidovlp_t {
                struct {
                    Flits::RSP<config>::dbid_t    db;
                    Flits::RSP<config>::srcid_t   src;
                    Flits::RSP<config>::tgtid_t   tgt;
                    Flits::RSP<config>::txnid_t   txn;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using pcrd_t = union __pcrd_t {
                struct {
                    Flits::RSP<config>::tgtid_t   tgt;
                    Flits::RSP<config>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using hnsrc_t = union __hnsrc_t {
                struct {
                    Flits::REQ<config>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

        private:
            using GetXaction = std::function<std::shared_ptr<Xaction<config>>(
                const Global<config>&,
                const FiredRequestFlit<config>&,
                std::shared_ptr<Xaction<config>>)>;


        private:
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>        rxTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>        rxDBIDTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>        rxDBIDOverlappableTransactions;

            std::unordered_map<uint64_t, std::list<std::shared_ptr<Xaction<config>>>>
                                                                                  rxRetriedTransactions;

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config>>>    grantedPCredits[1 << Flits::RSP<config>::PCRDTYPE_WIDTH];

            Opcodes::REQ::Decoder<Flits::REQ<config>, GetXaction>                 reqDecoder;

        protected:
            static std::shared_ptr<Xaction<config>> ConstructNone(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>> ConstructHomeRead(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>> ConstructHomeWrite(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>> ConstructHomeWriteZero(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>> ConstructHomeDataless(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;
            static std::shared_ptr<Xaction<config>> ConstructHomeAtomic(const Global<config>&, const FiredRequestFlit<config>&, std::shared_ptr<Xaction<config>>) noexcept;

        public:
            SNFJoint() noexcept;

        public:
            virtual void            Clear() noexcept override;

            virtual XactScopeEnum   GetActiveScope() const noexcept override;

            virtual void            GetInflight(
                const Global<config>&                           glbl,
                std::vector<std::shared_ptr<Xaction<config>>>&  dstVector,
                bool                                            sortByTime = false
            ) const noexcept;

        public:
            virtual XactDenialEnum  NextRXREQ(
                const Global<config>&               glbl,
                uint64_t                            time,
                const Flits::REQ<config>&           reqFlit,
                std::shared_ptr<Xaction<config>>*   xaction = nullptr
            ) noexcept override;

            virtual XactDenialEnum  NextTXRSP(
                const Global<config>&               glbl,
                uint64_t                            time,
                const Flits::RSP<config>&           rspFlit,
                std::shared_ptr<Xaction<config>>*   xaction = nullptr
            ) noexcept override;

            virtual XactDenialEnum  NextTXDAT(
                const Global<config>&               glbl,
                uint64_t                            time,
                const Flits::DAT<config>&           datFlit,
                std::shared_ptr<Xaction<config>>*   xaction = nullptr
            ) noexcept override;

            virtual XactDenialEnum  NextRXDAT(
                const Global<config>&               glbl,
                uint64_t                            time,
                const Flits::DAT<config>&           datFlit,
                std::shared_ptr<Xaction<config>>*   xaction = nullptr
            ) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class JointDenialEventBase
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline JointDenialEventBase<config>::JointDenialEventBase(
        Joint<config>&                      joint,
        std::shared_ptr<Xaction<config>>    xaction,
        XactDenialEnum                      denial,
        JointDenialSource                   source
    ) noexcept
        : joint         (joint)
        , xaction       (xaction)
        , denial        (denial)
        , denialSource  (source)
    { }

    template<FlitConfigurationConcept config>
    inline Joint<config>& JointDenialEventBase<config>::GetJoint() noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept config>
    inline const Joint<config>& JointDenialEventBase<config>::GetJoint() const noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> JointDenialEventBase<config>::GetXaction() noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const Xaction<config>> JointDenialEventBase<config>::GetXaction() const noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum JointDenialEventBase<config>::GetDenial() const noexcept
    {
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline JointDenialSource JointDenialEventBase<config>::GetDenialSource() const noexcept
    {
        return denialSource;
    }
}

// Implementation of: class JointDeniedRequestEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline JointDeniedRequestEvent<config>::JointDeniedRequestEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction,
        XactDenialEnum                    denial,
        JointDenialSource                 source,
        FiredRequestFlit<config>&         firedRequestFlit
    ) noexcept
        : JointDenialEventBase<config>    (joint, xaction, denial, source)
        , firedRequestFlit                (firedRequestFlit)
    { }

    template<FlitConfigurationConcept config>
    inline FiredRequestFlit<config>& JointDeniedRequestEvent<config>::GetFiredFlit() noexcept
    {
        return firedRequestFlit;
    }

    template<FlitConfigurationConcept config>
    inline const FiredRequestFlit<config>& JointDeniedRequestEvent<config>::GetFiredFlit() const noexcept
    {
        return firedRequestFlit;
    }
}

// Implementation of: class JointDeniedResponseEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline JointDeniedResponseEvent<config>::JointDeniedResponseEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction,
        XactDenialEnum                    denial,
        JointDenialSource                 source,
        FiredResponseFlit<config>&        firedResponseFlit
    ) noexcept
        : JointDenialEventBase<config>    (joint, xaction, denial, source)
        , firedResponseFlit               (firedResponseFlit)
    { }

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>& JointDeniedResponseEvent<config>::GetFiredFlit() noexcept
    {
        return firedResponseFlit;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>& JointDeniedResponseEvent<config>::GetFiredFlit() const noexcept
    {
        return firedResponseFlit;
    }
}


// Implementation of: class JointXactionEventBase
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionEventBase<config>::JointXactionEventBase(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : joint     (joint)
        , xaction   (xaction)
    { }

    template<FlitConfigurationConcept config>
    inline Joint<config>& JointXactionEventBase<config>::GetJoint() noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept config>
    inline const Joint<config>& JointXactionEventBase<config>::GetJoint() const noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> JointXactionEventBase<config>::GetXaction() noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<const Xaction<config>> JointXactionEventBase<config>::GetXaction() const noexcept
    {
        return xaction;
    }
}

// Implementation of: class JointXactionAcceptedEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionAcceptedEvent<config>::JointXactionAcceptedEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
    { }

    template<FlitConfigurationConcept config>
    inline JointXactionAcceptedEvent<config>::JointXactionAcceptedEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction,
        Flits::REQ<config>::srcid_t       snpTgtId
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
        , snoopTargetID                   (snpTgtId)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::REQ<config>::srcid_t JointXactionAcceptedEvent<config>::GetSnoopTargetID() const noexcept
    {
        return snoopTargetID;
    }
}

// Implementation of: class JointXactionRetryEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionRetryEvent<config>::JointXactionRetryEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionCompleteEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept config>
    inline JointXactionCompleteEvent<config>::JointXactionCompleteEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
        , snoopTargetID                   (0)
    { }

    template<FlitConfigurationConcept config>
    inline JointXactionCompleteEvent<config>::JointXactionCompleteEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction,
        Flits::REQ<config>::srcid_t       snpTgtId
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
        , snoopTargetID                   (snpTgtId)
    { }

    template<FlitConfigurationConcept config>
    inline Flits::REQ<config>::srcid_t JointXactionCompleteEvent<config>::GetSnoopTargetID() const noexcept
    {
        return snoopTargetID;
    }
}

// Implementation of: class JointXactionTxnIDAllocationEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept config>
    inline JointXactionTxnIDAllocationEvent<config>::JointXactionTxnIDAllocationEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionTxnIDFreeEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept config>
    inline JointXactionTxnIDFreeEvent<config>::JointXactionTxnIDFreeEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionDBIDAllocationEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept config>
    inline JointXactionDBIDAllocationEvent<config>::JointXactionDBIDAllocationEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionDBIDFreeEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept config>
    inline JointXactionDBIDFreeEvent<config>::JointXactionDBIDFreeEvent(
        Joint<config>&                    joint,
        std::shared_ptr<Xaction<config>>  xaction
    ) noexcept
        : JointXactionEventBase<config>   (joint, xaction)
    { }
}


// Implementation of: class Joint
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline Joint<config>::Joint() noexcept
        : events    (std::make_shared<EventHub>())
    { }

    template<FlitConfigurationConcept config>
    inline Joint<config>::EventHub::EventHub() noexcept
        : OnDeniedRequest       (0)
        , OnDeniedResponse      (0)
        , OnAccepted            (0)
        , OnRetry               (0)
        , OnTxnIDAllocation     (0)
        , OnTxnIDFree           (0)
        , OnDBIDAllocation      (0)
        , OnDBIDFree            (0)
        , OnComplete            (0)
    { }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::EventHub::Clear() noexcept
    {
        OnDeniedRequest.UnregisterAll();
        OnDeniedResponse.UnregisterAll();
        OnAccepted.UnregisterAll();
        OnRetry.UnregisterAll();
        OnTxnIDAllocation.UnregisterAll();
        OnTxnIDFree.UnregisterAll();
        OnDBIDAllocation.UnregisterAll();
        OnDBIDFree.UnregisterAll();
        OnComplete.UnregisterAll();
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::RequestDeniedByJoint(
        XactDenialEnum                          denial,
        FiredRequestFlit<config>&               firedRequestFlit,
        std::shared_ptr<Xaction<config>>        xaction) noexcept
    {
        this->events->OnDeniedRequest(JointDeniedRequestEvent<config>(
            *this, xaction, denial, JointDenialSource::JOINT, firedRequestFlit));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::RequestDeniedByXaction(
        XactDenialEnum                          denial,
        FiredRequestFlit<config>&               firedRequestFlit,
        std::shared_ptr<Xaction<config>>        xaction) noexcept
    {
        this->events->OnDeniedRequest(JointDeniedRequestEvent<config>(
            *this, xaction, denial, JointDenialSource::XACTION, firedRequestFlit));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::ResponseDeniedByJoint(
        XactDenialEnum                          denial,
        FiredResponseFlit<config>&              firedResponseFlit,
        std::shared_ptr<Xaction<config>>        xaction) noexcept
    {
        this->events->OnDeniedResponse(JointDeniedResponseEvent<config>(
            *this, xaction, denial, JointDenialSource::JOINT, firedResponseFlit));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::ResponseDeniedByXaction(
        XactDenialEnum                          denial,
        FiredResponseFlit<config>&              firedResponseFlit,
        std::shared_ptr<Xaction<config>>        xaction) noexcept
    {
        this->events->OnDeniedResponse(JointDeniedResponseEvent<config>(
            *this, xaction, denial, JointDenialSource::XACTION, firedResponseFlit));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextTXREQ(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::REQ<config>&               reqFlit,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return RequestDeniedByJoint(XactDenial::DENIED_CHANNEL_TXREQ, 
            FiredRequestFlit<config>(GetActiveScope(), true, time, reqFlit));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextRXREQ(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::REQ<config>&               reqFlit,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return RequestDeniedByJoint(XactDenial::DENIED_CHANNEL_RXREQ,
            FiredRequestFlit<config>(GetActiveScope(), false, time, reqFlit));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextTXSNP(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::SNP<config>&               snpFlit,
        const Flits::REQ<config>::srcid_t       snpTgtId,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return RequestDeniedByJoint(XactDenial::DENIED_CHANNEL_TXSNP,
            FiredRequestFlit<config>(GetActiveScope(), true, time, snpFlit, snpTgtId));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextRXSNP(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::SNP<config>&               snpFlit,
        const Flits::REQ<config>::srcid_t       snpTgtId,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return RequestDeniedByJoint(XactDenial::DENIED_CHANNEL_RXSNP,
            FiredRequestFlit<config>(GetActiveScope(), false, time, snpFlit, snpTgtId));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextTXRSP(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::RSP<config>&               rspFlit,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return ResponseDeniedByJoint(XactDenial::DENIED_CHANNEL_TXRSP,
            FiredResponseFlit<config>(GetActiveScope(), true, time, rspFlit));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextRXRSP(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::RSP<config>&               rspFlit,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return ResponseDeniedByJoint(XactDenial::DENIED_CHANNEL_RXRSP,
            FiredResponseFlit<config>(GetActiveScope(), false, time, rspFlit));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextTXDAT(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::DAT<config>&               datFlit,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return ResponseDeniedByJoint(XactDenial::DENIED_CHANNEL_TXDAT,
            FiredResponseFlit<config>(GetActiveScope(), true, time, datFlit));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextRXDAT(
        const Global<config>&                   glbl,
        uint64_t                                time,
        const Flits::DAT<config>&               datFlit,
        std::shared_ptr<Xaction<config>>*       theXaction) noexcept
    {
        return ResponseDeniedByJoint(XactDenial::DENIED_CHANNEL_RXDAT,
            FiredResponseFlit<config>(GetActiveScope(), false, time, datFlit));
    }
}


// Implementation of: class RNFJoint
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructNone(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retired) noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructAllocatingRead(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionAllocatingRead<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructNonAllocatingRead(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionNonAllocatingRead<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructImmediateWrite(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionImmediateWrite<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructWriteZero(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionWriteZero<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructCopyBackWrite(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionCopyBackWrite<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructDataless(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionDataless<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructHomeSnoop(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionHomeSnoop<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructForwardSnoop(
        const Global<config>&             glbl,       
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionForwardSnoop<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructAtomic(
        const Global<config>&             glbl,       
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionAtomic<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructIndependentStash(
        const Global<config>&             glbl,       
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionIndependentStash<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline RNFJoint<config>::RNFJoint() noexcept
    {
        // TXREQ transactions
        #define SET_REQ_XACTION(opcode, type) \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&RNFJoint<config>::Construct##type)

        SET_REQ_XACTION(ReqLCrdReturn               , None              );  // 0x00
        SET_REQ_XACTION(ReadShared                  , AllocatingRead    );  // 0x01
        SET_REQ_XACTION(ReadClean                   , AllocatingRead    );  // 0x02
        SET_REQ_XACTION(ReadOnce                    , NonAllocatingRead );  // 0x03
        SET_REQ_XACTION(ReadNoSnp                   , NonAllocatingRead );  // 0x04
        SET_REQ_XACTION(PCrdReturn                  , None              );  // 0x05
                                                                            // 0x06
        SET_REQ_XACTION(ReadUnique                  , AllocatingRead    );  // 0x07
        SET_REQ_XACTION(CleanShared                 , Dataless          );  // 0x08
        SET_REQ_XACTION(CleanInvalid                , Dataless          );  // 0x09
        SET_REQ_XACTION(MakeInvalid                 , Dataless          );  // 0x0A
        SET_REQ_XACTION(CleanUnique                 , Dataless          );  // 0x0B
        SET_REQ_XACTION(MakeUnique                  , Dataless          );  // 0x0C
        SET_REQ_XACTION(Evict                       , Dataless          );  // 0x0D
                                                                            // 0x0E
                                                                            // 0x0F
                                                                            // 0x10
        SET_REQ_XACTION(ReadNoSnpSep                , None              );  // 0x11
                                                                            // 0x12
        SET_REQ_XACTION(CleanSharedPersistSep       , Dataless          );  // 0x13
        SET_REQ_XACTION(DVMOp                       , None              );  // 0x14
        SET_REQ_XACTION(WriteEvictFull              , CopyBackWrite     );  // 0x15
                                                                            // 0x16
        SET_REQ_XACTION(WriteCleanFull              , CopyBackWrite     );  // 0x17
        SET_REQ_XACTION(WriteUniquePtl              , ImmediateWrite    );  // 0x18
        SET_REQ_XACTION(WriteUniqueFull             , ImmediateWrite    );  // 0x19
        SET_REQ_XACTION(WriteBackPtl                , CopyBackWrite     );  // 0x1A
        SET_REQ_XACTION(WriteBackFull               , CopyBackWrite     );  // 0x1B
        SET_REQ_XACTION(WriteNoSnpPtl               , ImmediateWrite    );  // 0x1C
        SET_REQ_XACTION(WriteNoSnpFull              , ImmediateWrite    );  // 0x1D
                                                                            // 0x1E
                                                                            // 0x1F
        SET_REQ_XACTION(WriteUniqueFullStash        , ImmediateWrite    );  // 0x20
        SET_REQ_XACTION(WriteUniquePtlStash         , ImmediateWrite    );  // 0x21
        SET_REQ_XACTION(StashOnceShared             , IndependentStash  );  // 0x22
        SET_REQ_XACTION(StashOnceUnique             , IndependentStash  );  // 0x23
        SET_REQ_XACTION(ReadOnceCleanInvalid        , NonAllocatingRead );  // 0x24
        SET_REQ_XACTION(ReadOnceMakeInvalid         , NonAllocatingRead );  // 0x25
        SET_REQ_XACTION(ReadNotSharedDirty          , AllocatingRead    );  // 0x26
        SET_REQ_XACTION(CleanSharedPersist          , Dataless          );  // 0x27
        SET_REQ_XACTION(AtomicStore::ADD            , Atomic            );  // 0x28
        SET_REQ_XACTION(AtomicStore::CLR            , Atomic            );  // 0x29
        SET_REQ_XACTION(AtomicStore::EOR            , Atomic            );  // 0x2A
        SET_REQ_XACTION(AtomicStore::SET            , Atomic            );  // 0x2B
        SET_REQ_XACTION(AtomicStore::SMAX           , Atomic            );  // 0x2C
        SET_REQ_XACTION(AtomicStore::SMIN           , Atomic            );  // 0x2D
        SET_REQ_XACTION(AtomicStore::UMAX           , Atomic            );  // 0x2E
        SET_REQ_XACTION(AtomicStore::UMIN           , Atomic            );  // 0x2F
        SET_REQ_XACTION(AtomicLoad::ADD             , Atomic            );  // 0x30
        SET_REQ_XACTION(AtomicLoad::CLR             , Atomic            );  // 0x31
        SET_REQ_XACTION(AtomicLoad::EOR             , Atomic            );  // 0x32
        SET_REQ_XACTION(AtomicLoad::SET             , Atomic            );  // 0x33
        SET_REQ_XACTION(AtomicLoad::SMAX            , Atomic            );  // 0x34
        SET_REQ_XACTION(AtomicLoad::SMIN            , Atomic            );  // 0x35
        SET_REQ_XACTION(AtomicLoad::UMAX            , Atomic            );  // 0x36
        SET_REQ_XACTION(AtomicLoad::UMIN            , Atomic            );  // 0x37
        SET_REQ_XACTION(AtomicSwap                  , Atomic            );  // 0x38
        SET_REQ_XACTION(AtomicCompare               , Atomic            );  // 0x39
        SET_REQ_XACTION(PrefetchTgt                 , None              );  // 0x3A
                                                                            // 0x3B
                                                                            // 0x3C
                                                                            // 0x3D
                                                                            // 0x3E
                                                                            // 0x3F
                                                                            // 0x40
        SET_REQ_XACTION(MakeReadUnique              , AllocatingRead    );  // 0x41
        SET_REQ_XACTION(WriteEvictOrEvict           , CopyBackWrite     );  // 0x42
        SET_REQ_XACTION(WriteUniqueZero             , WriteZero         );  // 0x43
        SET_REQ_XACTION(WriteNoSnpZero              , WriteZero         );  // 0x44
                                                                            // 0x45
                                                                            // 0x46
        SET_REQ_XACTION(StashOnceSepShared          , IndependentStash  );  // 0x47
        SET_REQ_XACTION(StashOnceSepUnique          , IndependentStash  );  // 0x48
                                                                            // 0x49
                                                                            // 0x4A
                                                                            // 0x4B
        SET_REQ_XACTION(ReadPreferUnique            , AllocatingRead    );  // 0x4C
                                                                            // 0x4D
                                                                            // 0x4E
                                                                            // 0x4F
        SET_REQ_XACTION(WriteNoSnpFullCleanSh       , None              );  // 0x50
        SET_REQ_XACTION(WriteNoSnpFullCleanInv      , None              );  // 0x51
        SET_REQ_XACTION(WriteNoSnpFullCleanShPerSep , None              );  // 0x52
                                                                            // 0x53
        SET_REQ_XACTION(WriteUniqueFullCleanSh      , None              );  // 0x54
                                                                            // 0x55
        SET_REQ_XACTION(WriteUniqueFullCleanShPerSep, None              );  // 0x56
                                                                            // 0x57
        SET_REQ_XACTION(WriteBackFullCleanSh        , None              );  // 0x58
        SET_REQ_XACTION(WriteBackFullCleanInv       , None              );  // 0x59
        SET_REQ_XACTION(WriteBackFullCleanShPerSep  , None              );  // 0x5A
                                                                            // 0x5B
        SET_REQ_XACTION(WriteCleanFullCleanSh       , None              );  // 0x5C
                                                                            // 0x5D
        SET_REQ_XACTION(WriteCleanFullCleanShPerSep , None              );  // 0x5E
                                                                            // 0x5F
        SET_REQ_XACTION(WriteNoSnpPtlCleanSh        , None              );  // 0x60
        SET_REQ_XACTION(WriteNoSnpPtlCleanInv       , None              );  // 0x61
        SET_REQ_XACTION(WriteNoSnpPtlCleanShPerSep  , None              );  // 0x62
                                                                            // 0x63
        SET_REQ_XACTION(WriteUniquePtlCleanSh       , None              );  // 0x64
                                                                            // 0x65
        SET_REQ_XACTION(WriteUniquePtlCleanShPerSep , None              );  // 0x66
                                                                            // 0x67
                                                                            // 0x68
                                                                            // 0x69
                                                                            // 0x6A
                                                                            // 0x6B
                                                                            // 0x6C
                                                                            // 0x6D
                                                                            // 0x6E
                                                                            // 0x6F
                                                                            // 0x70
                                                                            // 0x71
                                                                            // 0x72
                                                                            // 0x73
                                                                            // 0x74
                                                                            // 0x75
                                                                            // 0x76
                                                                            // 0x77
                                                                            // 0x78
                                                                            // 0x79
                                                                            // 0x7A
                                                                            // 0x7B
                                                                            // 0x7C
                                                                            // 0x7D
                                                                            // 0x7E
                                                                            // 0x7F

        #undef SET_REQ_XACTION

        // RXSNP Transactions
        #define SET_SNP_XACTION(opcode, type) \
            snpDecoder[Opcodes::SNP::opcode].SetCompanion(&RNFJoint<config>::Construct##type)
        
        SET_SNP_XACTION(SnpLCrdReturn               , None              );  // 0x00
        SET_SNP_XACTION(SnpShared                   , HomeSnoop         );  // 0x01
        SET_SNP_XACTION(SnpClean                    , HomeSnoop         );  // 0x02
        SET_SNP_XACTION(SnpOnce                     , HomeSnoop         );  // 0x03
        SET_SNP_XACTION(SnpNotSharedDirty           , HomeSnoop         );  // 0x04
        SET_SNP_XACTION(SnpUniqueStash              , HomeSnoop         );  // 0x05
        SET_SNP_XACTION(SnpMakeInvalidStash         , HomeSnoop         );  // 0x06
        SET_SNP_XACTION(SnpUnique                   , HomeSnoop         );  // 0x07
        SET_SNP_XACTION(SnpCleanShared              , HomeSnoop         );  // 0x08
        SET_SNP_XACTION(SnpCleanInvalid             , HomeSnoop         );  // 0x09
        SET_SNP_XACTION(SnpMakeInvalid              , HomeSnoop         );  // 0x0A
        SET_SNP_XACTION(SnpStashUnique              , HomeSnoop         );  // 0x0B
        SET_SNP_XACTION(SnpStashShared              , HomeSnoop         );  // 0x0C
        SET_SNP_XACTION(SnpDVMOp                    , None              );  // 0x0D
                                                                            // 0x0E
                                                                            // 0x0F
        SET_SNP_XACTION(SnpQuery                    , HomeSnoop         );  // 0x10
        SET_SNP_XACTION(SnpSharedFwd                , ForwardSnoop      );  // 0x11
        SET_SNP_XACTION(SnpCleanFwd                 , ForwardSnoop      );  // 0x12
        SET_SNP_XACTION(SnpOnceFwd                  , ForwardSnoop      );  // 0x13
        SET_SNP_XACTION(SnpNotSharedDirtyFwd        , ForwardSnoop      );  // 0x14
        SET_SNP_XACTION(SnpPreferUnique             , HomeSnoop         );  // 0x15
        SET_SNP_XACTION(SnpPreferUniqueFwd          , ForwardSnoop      );  // 0x16
        SET_SNP_XACTION(SnpUniqueFwd                , ForwardSnoop      );  // 0x17
                                                                            // 0x18
                                                                            // 0x19
                                                                            // 0x1A
                                                                            // 0x1B
                                                                            // 0x1C
                                                                            // 0x1D
                                                                            // 0x1E
                                                                            // 0x1F

        #undef SET_SNP_XACTION
    }

    template<FlitConfigurationConcept config>
    inline RNFJoint<config>::RNFJoint(const RNFJoint<config>& obj) noexcept
        : Joint<config>                     (obj)
        , txTransactions                    (obj.txTransactions)
        , rxTransactions                    (obj.rxTransactions)
        , txDBIDTransactions                (obj.txDBIDTransactions)
        , txDBIDOverlappableTransactions    (obj.txDBIDOverlappableTransactions)
        , txRetriedTransactions             (obj.txRetriedTransactions)
        , grantedPCredits                   (/*obj.grantedPCredits*/)
        , reqDecoder                        (obj.reqDecoder)
        , snpDecoder                        (obj.snpDecoder)
    {
        std::copy(obj.grantedPCredits, obj.grantedPCredits + (1 << Flits::RSP<config>::PCRDTYPE_WIDTH),
            grantedPCredits);

        Fork();
    }

    template<FlitConfigurationConcept config>
    inline RNFJoint<config>& RNFJoint<config>::operator=(const RNFJoint<config>& obj) noexcept
    {
        *(Joint<config>*)(this) = obj;

        txTransactions                  = obj.txTransactions;
        rxTransactions                  = obj.rxTransactions;
        txDBIDTransactions              = obj.txDBIDTransactions;
        txDBIDOverlappableTransactions  = obj.txDBIDOverlappableTransactions;
        txRetriedTransactions           = obj.txRetriedTransactions;
    //  grantedPCredits                 = obj.grantedPCredits;
        reqDecoder                      = obj.reqDecoder;
        snpDecoder                      = obj.snpDecoder;

        std::copy(obj.grantedPCredits, obj.grantedPCredits + (1 << Flits::RSP<config>::PCRDTYPE_WIDTH),
            grantedPCredits);

        Fork();
    }

    template<FlitConfigurationConcept config>
    inline void RNFJoint<config>::Fork() noexcept
    {
        for (auto& p : txTransactions)
            p.second = p.second->Clone();

        for (auto& p : rxTransactions)
            p.second = p.second->Clone();

        for (auto& p : txDBIDTransactions)
            p.second = p.second->Clone();

        for (auto& p : txDBIDOverlappableTransactions)
            p.second = p.second->Clone();

        for (auto& p : txRetriedTransactions)
            for (auto& p1 : p.second)
                p1 = p1->Clone();
    }

    template<FlitConfigurationConcept config>
    inline void RNFJoint<config>::Clear() noexcept
    {
        txTransactions.clear();
        rxTransactions.clear();

        txDBIDTransactions.clear();

        txDBIDOverlappableTransactions.clear();

        txRetriedTransactions.clear();

        for (int i = 0; i < (1 << Flits::RSP<config>::PCRDTYPE_WIDTH); i++)
            grantedPCredits[i].clear();
    }

    template<FlitConfigurationConcept config>
    inline XactScopeEnum RNFJoint<config>::GetActiveScope() const noexcept
    {
        return XactScope::Requester;
    }

    template<FlitConfigurationConcept config>
    inline void RNFJoint<config>::GetInflight(
        const Global<config>&                             glbl,
        std::vector<std::shared_ptr<Xaction<config>>>&    dstVector,
        bool                                              sortByTime
    ) const noexcept
    {
        for (auto& rxTransaction : rxTransactions)
            dstVector.push_back(rxTransaction.second);

        for (auto& txTransaction : txTransactions)
            dstVector.push_back(txTransaction.second);

        for (auto& txDBIDTransaction : txDBIDTransactions)
        {
            if (!txDBIDTransaction.second->IsTxnIDComplete(glbl))
                continue;
            else
                dstVector.push_back(txDBIDTransaction.second);
        }

        for (auto& txDBIDOverlappableTransaction : txDBIDOverlappableTransactions)
        {
            if (!txDBIDOverlappableTransaction.second->IsTxnIDComplete(glbl))
                continue;
            else
                dstVector.push_back(txDBIDOverlappableTransaction.second);
        }

        if (sortByTime)
        {
            std::sort(dstVector.begin(), dstVector.end(),
                [] (std::shared_ptr<Xaction<config>> a, std::shared_ptr<Xaction<config>> b)
                {
                    return a->GetFirst().time < b->GetFirst().time;
                }
            );
        }
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum RNFJoint<config>::NextTXREQ(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::REQ<config>&         reqFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        txreqid_t key;
        key.value   = 0;
        key.id.src  = reqFlit.SrcID();
        key.id.txn  = reqFlit.TxnID();

        bool retry = false;

        //
        FiredRequestFlit<config> firedReqFlit(XactScope::Requester, true, time, reqFlit);

        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, GetXaction>& opcodeInfo = 
            reqDecoder.DecodeRNF(reqFlit.Opcode());

        if (!opcodeInfo.IsValid()) // unknown opcode
            return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_OPCODE_NOT_DECODED, firedReqFlit);

        //
        if (reqFlit.AllowRetry() == 0)
        {
            if (txTransactions.contains(key))
                return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_TXNID_IN_USE, firedReqFlit, txTransactions[key]);

            // iterate and compare retry transactions
            // TODO: simplified retry mapping by TxnID, temporary workaround for KunminghuV2
            //       specification retry mapping implementation here
            rnsrc_t rnKey;
            rnKey.value     = 0;
            rnKey.id.src    = reqFlit.SrcID();

            auto xactionList = txRetriedTransactions.find(rnKey);
            if (xactionList == txRetriedTransactions.end())
                return this->RequestDeniedByJoint(XactDenial::DENIED_NO_MATCHING_RETRY, firedReqFlit);

            auto xactionIter = xactionList->second.begin();

            for (; xactionIter != xactionList->second.end(); xactionIter++)
                if (reqFlit.TxnID() == xactionIter->get()->GetFirst().flit.req.TxnID())
                    break;

            if (xactionIter == xactionList->second.end())
                return this->RequestDeniedByJoint(XactDenial::DENIED_NO_MATCHING_RETRY, firedReqFlit);

            std::shared_ptr<Xaction<config>> firstXaction = *xactionIter;

            xactionList->second.erase(xactionIter);

            std::shared_ptr<Xaction<config>> retryXaction =
                opcodeInfo.GetCompanion()(glbl, firedReqFlit, firstXaction);

            if (!retryXaction) // unsupported opcode transaction
                return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, firedReqFlit);

            if (theXaction)
                *theXaction = retryXaction;
            
            if (retryXaction->GetFirstDenial() != XactDenial::ACCEPTED)
                return this->RequestDeniedByXaction(retryXaction->GetFirstDenial(), firedReqFlit, retryXaction);

            assert(firstXaction->GetRetryAck() && "transaction not getting RetryAck but lies in retried list");

            // check granted P-Credit
            pcrd_t pCrdKey;
            pCrdKey.value   = 0;
            pCrdKey.id.src  = firstXaction->GetRetryAck()->flit.rsp.SrcID();
            pCrdKey.id.tgt  = reqFlit.SrcID();

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config>>>& grantedPCredits
                = this->grantedPCredits[reqFlit.PCrdType()];

            auto iterPCreditList = grantedPCredits.find(pCrdKey);
            if (iterPCreditList == grantedPCredits.end())
                return this->RequestDeniedByJoint(XactDenial::DENIED_NO_MATCHING_PCREDIT, firedReqFlit);

            std::list<FiredResponseFlit<config>>& pCreditList = iterPCreditList->second;

            if (pCreditList.empty())
                return this->RequestDeniedByJoint(XactDenial::DENIED_NO_MATCHING_PCREDIT, firedReqFlit);

            XactDenialEnum denial =
                firstXaction->Resend(glbl, pCreditList.front(), retryXaction);
            
            if (denial != XactDenial::ACCEPTED)
                return this->RequestDeniedByXaction(denial, firedReqFlit, firstXaction);

            // original xaction complete
            txreqid_t firstKey;
            firstKey.value  = 0;
            firstKey.id.src = firstXaction->GetFirst().flit.req.SrcID();
            firstKey.id.txn = firstXaction->GetFirst().flit.req.TxnID();

            txTransactions.erase(firstKey);

            // event on retry xaction
            this->events->OnRetry(JointXactionRetryEvent<config>(*this, retryXaction));

            // event on TxnID allocation
            this->events->OnTxnIDAllocation(JointXactionTxnIDAllocationEvent<config>(*this, retryXaction));

            // consume P-Credit
            pCreditList.pop_front();

            // register retried xaction
            txTransactions[key] = retryXaction;
        }
        else
        {
            if (txTransactions.contains(key))
                return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_TXNID_IN_USE, firedReqFlit, txTransactions[key]);

            std::shared_ptr<Xaction<config>> xaction;

            xaction = opcodeInfo.GetCompanion()(glbl, firedReqFlit, nullptr);

            if (!xaction) // unsupported opcode transaction
                return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, firedReqFlit);

            if (theXaction)
                *theXaction = xaction;

            if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
                return this->RequestDeniedByXaction(xaction->GetFirstDenial(), firedReqFlit, xaction);

            // event on Request xaction accepted
            this->events->OnAccepted(JointXactionAcceptedEvent<config>(*this, xaction));

            // event on TxnID allocation
            this->events->OnTxnIDAllocation(JointXactionTxnIDAllocationEvent<config>(*this, xaction));

            txTransactions[key] = xaction;
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum RNFJoint<config>::NextRXSNP(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::SNP<config>&         snpFlit,
        const Flits::REQ<config>::srcid_t snpTgtId,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        rxsnpid_t key;
        key.value   = 0;
        key.id.tgt  = snpTgtId;
        key.id.src  = snpFlit.SrcID();
        key.id.txn  = snpFlit.TxnID();

        //
        FiredRequestFlit<config> firedSnpFlit(XactScope::Requester, false, time, snpFlit, snpTgtId);

        //
        if (rxTransactions.contains(key))
            return this->RequestDeniedByJoint(XactDenial::DENIED_SNP_TXNID_IN_USE, firedSnpFlit, rxTransactions[key]);

        const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, GetXaction>& opcodeInfo =
            snpDecoder.DecodeRNF(snpFlit.Opcode());
        
        if (!opcodeInfo.IsValid()) // unknown opcode
            return this->RequestDeniedByJoint(XactDenial::DENIED_SNP_OPCODE_NOT_DECODED, firedSnpFlit);
        
        std::shared_ptr<Xaction<config>> xaction =
            opcodeInfo.GetCompanion()(glbl, firedSnpFlit, nullptr);

        if (!xaction) // unsupported opcode transaction
            return this->RequestDeniedByJoint(XactDenial::DENIED_SNP_OPCODE_NOT_SUPPORTED, firedSnpFlit);
        
        if (theXaction)
            *theXaction = xaction;
        
        if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
            return this->RequestDeniedByXaction(xaction->GetFirstDenial(), firedSnpFlit, xaction);

        // event on Request xaction accepted
        this->events->OnAccepted(JointXactionAcceptedEvent<config>(*this, xaction, snpTgtId));

        rxTransactions[key] = xaction;

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum RNFJoint<config>::NextTXRSP(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::RSP<config>&         rspFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        FiredResponseFlit<config> firedRspFlit(XactScope::Requester, true, time, rspFlit);

        std::shared_ptr<Xaction<config>> xaction;

        // RSPs from requester might apply TxnID with DBID (CompAck).
        // Otherwise, snoop RSPs use TxnID from SNPs (SnpResp, SnpRespFwded).
        if (firedRspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
        {
            // DBID overlapping would never happen on this routine,
            // since Comp was impossible to be re-ordered on interconnect
            // on transactions with CompAck, on which the deallocation of DBIDs on HNs
            // were determined by RNs.

            txreqdbid_t key;
            key.value   = 0;
            key.id.tgt  = rspFlit.SrcID();
            key.id.src  = rspFlit.TgtID();
            key.id.db   = rspFlit.TxnID();

            auto xactionIter = txDBIDTransactions.find(key);
            if (xactionIter == txDBIDTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_RSP_TXNID_NOT_EXIST, firedRspFlit);

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextRSP(glbl, firedRspFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return this->ResponseDeniedByXaction(denial, firedRspFlit, xaction);
        }
        else if (firedRspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpResp
              || firedRspFlit.flit.rsp.Opcode() == Opcodes::RSP::SnpRespFwded)
        {
            rxsnpid_t key;
            key.value   = 0;
            key.id.tgt  = rspFlit.SrcID();
            key.id.src  = rspFlit.TgtID();
            key.id.txn  = rspFlit.TxnID();

            auto xactionIter = rxTransactions.find(key);
            if (xactionIter == rxTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_RSP_TXNID_NOT_EXIST, firedRspFlit);

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextRSP(glbl, firedRspFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return this->ResponseDeniedByXaction(denial, firedRspFlit, xaction);
        }
        else
            return this->ResponseDeniedByJoint(XactDenial::DENIED_RSP_OPCODE, firedRspFlit);

        if (xaction->GetFirst().IsREQ())
        {
            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));
                    
                    // DBID overlapping would never happen on CompAck
                    // see above for details

                    txreqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                    }
                    else
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBID.id.src  = xactionDBIDSource->flit.dat.HomeNID();
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                    }

                    txDBIDTransactions.erase(keyDBID);
                }
            }
            // consider DBID overlapping here
            else if (xaction->IsDBIDOverlappable(glbl))
            {
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    txreqdbidovlp_t keyDBIDOvlp;
                    keyDBIDOvlp.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();
                    }
                    else
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.dat.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.dat.TxnID();
                    }

                    txDBIDOverlappableTransactions.erase(keyDBIDOvlp);
                }
            }

            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));
            }
        }
        else // SNP
        {
            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction, rspFlit.SrcID()));

                rxsnpid_t key;
                key.value   = 0;
                key.id.tgt  = rspFlit.SrcID();
                key.id.src  = xaction->GetFirst().flit.snp.SrcID();
                key.id.txn  = xaction->GetFirst().flit.snp.TxnID();

                rxTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum RNFJoint<config>::NextRXRSP(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::RSP<config>&         rspFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        FiredResponseFlit<config> firedRspFlit(XactScope::Requester, false, time, rspFlit);

        // P-Credit Grant
        if (rspFlit.Opcode() == Opcodes::RSP::PCrdGrant)
        {
            pcrd_t pCrdKey;
            pCrdKey.value   = 0;
            pCrdKey.id.src  = rspFlit.SrcID();
            pCrdKey.id.tgt  = rspFlit.TgtID();

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config>>>& grantedPCredits
                = this->grantedPCredits[rspFlit.PCrdType()];

            auto iterPCreditList = grantedPCredits.try_emplace(pCrdKey).first;

            iterPCreditList->second.push_back(firedRspFlit);

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit);
                if (denial != XactDenial::ACCEPTED)
                    return this->ResponseDeniedByJoint(denial, firedRspFlit);
            }

            return XactDenial::ACCEPTED;
        }

        std::shared_ptr<Xaction<config>> xaction;

        // RSPs to requester do not apply TxnID with DBID,
        // but may pass new DBID to requester.

        txreqid_t key;
        key.value   = 0;
        key.id.src  = rspFlit.TgtID();
        key.id.txn  = rspFlit.TxnID();

        auto xactionIter = txTransactions.find(key);
        if (xactionIter == txTransactions.end())
            return this->ResponseDeniedByJoint(XactDenial::DENIED_RSP_TXNID_NOT_EXIST, firedRspFlit);

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextRSP(glbl, firedRspFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByXaction(denial, firedRspFlit, xaction);

        if (hasDBID)
        {
            txreqdbid_t keyDBID;
            keyDBID.value   = 0;
            keyDBID.id.tgt  = rspFlit.TgtID();
            keyDBID.id.src  = rspFlit.SrcID();
            keyDBID.id.db   = rspFlit.DBID();

            if (firstDBID)
            {
                // first DBID always not overlappable

                if (txDBIDTransactions.contains(keyDBID))
                {
                    xaction->SetLastDenial(XactDenial::DENIED_RSP_DBID_IN_USE);
                    return this->ResponseDeniedByJoint(XactDenial::DENIED_RSP_DBID_IN_USE, firedRspFlit, xaction);
                }

                txDBIDTransactions[keyDBID] = xaction;

                // event on DBID allocation
                this->events->OnDBIDAllocation(JointXactionDBIDAllocationEvent<config>(*this, xaction));
            }
            else
            {
                // nothing needed to be done here
                // the consistency of DBID should be checked inside Xaction
            }
        }

        if (xaction->GetFirst().IsREQ())
        {
            // on TxnID free
            if (xaction->IsTxnIDComplete(glbl))
            {
                // event on TxnID free
                this->events->OnTxnIDFree(JointXactionTxnIDFreeEvent<config>(*this, xaction));

                // remove related TxnID mapping
                txreqid_t key;
                key.value   = 0;
                key.id.src  = xaction->GetFirst().flit.req.SrcID();
                key.id.txn  = xaction->GetFirst().flit.req.TxnID();

                txTransactions.erase(key);
            }

            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));

                    // check overlappable DBID mapping first
                    txreqdbidovlp_t keyDBIDOvlp;
                    keyDBIDOvlp.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();
                    }
                    else
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.dat.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.dat.TxnID();
                    }

                    if (!txDBIDOverlappableTransactions.erase(keyDBIDOvlp))
                    {
                        // erase normal DBID mapping
                        // only when overlappable DBID mapping absent

                        txreqdbid_t keyDBID;
                        keyDBID.value   = 0;
                        if (xactionDBIDSource->IsRSP())
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        }
                        else
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.dat.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.dat.HomeNID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        }

                        // check TxnID for non-overlappable DBID on DBID complete
                        auto xactionDBIDIter = txDBIDTransactions.find(keyDBID);
                        if (xactionDBIDIter != txDBIDTransactions.end())
                        {
                            auto xactionDBID = xactionDBIDIter->second;
                            if (xactionDBID->GetFirst().flit.req.TxnID() == xaction->GetFirst().flit.req.TxnID())
                                txDBIDTransactions.erase(xactionDBIDIter);
                        }
                    }
                }
            }
            // on DBID overlap
            else if (xaction->IsDBIDOverlappable(glbl))
            {
                // move DBID mapping to overlappable if not moved yet
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // check overlappable DBID mapping
                    txreqdbidovlp_t keyDBIDOvlp;
                    keyDBIDOvlp.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();
                    }
                    else
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.dat.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.dat.TxnID();
                    }

                    // move on absent
                    if (txDBIDOverlappableTransactions.find(keyDBIDOvlp) == txDBIDOverlappableTransactions.end())
                    {
                        // erase normal DBID mapping
                        txreqdbid_t keyDBID;
                        keyDBID.value   = 0;
                        if (xactionDBIDSource->IsRSP())
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        }
                        else
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.dat.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.dat.HomeNID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        }

                        txDBIDTransactions.erase(keyDBID);

                        // set overlappable DBID mapping
                        txDBIDOverlappableTransactions[keyDBIDOvlp] = xaction;
                    }
                }
            }

            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));

                if (xaction->GotRetryAck())
                {
                    // only on RXRSP, transactions might be completed by retry

                    // record as retried
                    rnsrc_t rnKey;
                    rnKey.value     = 0;
                    rnKey.id.src    = rspFlit.TgtID();

                    txRetriedTransactions[rnKey].push_back(xaction);
                }
            }
        }
        else // SNP
        {
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction, rspFlit.TgtID()));

                rxsnpid_t key;
                key.value   = 0;
                key.id.tgt  = rspFlit.TgtID();
                key.id.src  = xaction->GetFirst().flit.snp.SrcID();
                key.id.txn  = xaction->GetFirst().flit.snp.TxnID();

                rxTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum RNFJoint<config>::NextTXDAT(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::DAT<config>&         datFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        FiredResponseFlit<config> firedDatFlit(XactScope::Requester, true, time, datFlit);

        std::shared_ptr<Xaction<config>> xaction;

        // DCT or Write
        if (firedDatFlit.IsToRequester(glbl))
        {
            rxsnpid_t key;
            key.value   = 0;
            key.id.tgt  = datFlit.SrcID();
            key.id.src  = datFlit.HomeNID();
            key.id.txn  = datFlit.DBID();

            auto xactionIter = rxTransactions.find(key);
            if (xactionIter == rxTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_DAT_DBID_NOT_EXIST, firedDatFlit);

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(glbl, firedDatFlit, hasDBID, firstDBID);
        
            if (denial != XactDenial::ACCEPTED)
                return this->ResponseDeniedByXaction(denial, firedDatFlit, xaction);
        }
        else if (firedDatFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespData
              || firedDatFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataPtl
              || firedDatFlit.flit.dat.Opcode() == Opcodes::DAT::SnpRespDataFwded)
        {
            rxsnpid_t key;
            key.value   = 0;
            key.id.tgt  = datFlit.SrcID();
            key.id.src  = datFlit.TgtID();
            key.id.txn  = datFlit.TxnID();

            auto xactionIter = rxTransactions.find(key);
            if (xactionIter == rxTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_DAT_TXNID_NOT_EXIST, firedDatFlit);

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(glbl, firedDatFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return this->ResponseDeniedByXaction(denial, firedDatFlit, xaction);
        }
        else
        {
            txreqdbid_t key;
            key.value   = 0;
            key.id.tgt  = datFlit.SrcID();
            key.id.src  = datFlit.TgtID();
            key.id.db   = datFlit.TxnID();

            auto xactionIter = txDBIDTransactions.find(key);
            if (xactionIter == txDBIDTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_DAT_TXNID_NOT_EXIST, firedDatFlit);

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(glbl, firedDatFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return this->ResponseDeniedByXaction(denial, firedDatFlit, xaction);
        }

        if (xaction->GetFirst().IsREQ())
        {
            // *NOTICE: TxnID not deallocating on TXDAT,
            //          since TXDAT depends on DBID response

            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));

                    // check overlappable DBID mapping first
                    txreqdbidovlp_t keyDBIDOvlp;
                    keyDBIDOvlp.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();
                    }
                    else
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.dat.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.dat.TxnID();
                    }

                    if (!txDBIDOverlappableTransactions.erase(keyDBIDOvlp))
                    {
                        // erase normal DBID mapping
                        // only when overlappable DBID mapping absent

                        txreqdbid_t keyDBID;
                        keyDBID.value   = 0;
                        if (xactionDBIDSource->IsRSP())
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        }
                        else
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.dat.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.dat.HomeNID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        }

                        // check TxnID for non-overlappable DBID on DBID complete
                        auto xactionDBIDIter = txDBIDTransactions.find(keyDBID);
                        if (xactionDBIDIter != txDBIDTransactions.end())
                        {
                            auto xactionDBID = xactionDBIDIter->second;
                            if (xactionDBID->GetFirst().flit.req.TxnID() == xaction->GetFirst().flit.req.TxnID())
                                txDBIDTransactions.erase(xactionDBIDIter);
                        }
                    }
                }
            }
            // on DBID overlap
            else if (xaction->IsDBIDOverlappable(glbl))
            {
                // move DBID mapping to overlappable if not moved yet
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // check overlappable DBID mapping
                    txreqdbidovlp_t keyDBIDOvlp;
                    keyDBIDOvlp.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();
                    }
                    else
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.dat.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.dat.TxnID();
                    }

                    // move on absent
                    if (txDBIDOverlappableTransactions.find(keyDBIDOvlp) == txDBIDOverlappableTransactions.end())
                    {
                        // erase normal DBID mapping
                        txreqdbid_t keyDBID;
                        keyDBID.value   = 0;
                        if (xactionDBIDSource->IsRSP())
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        }
                        else
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.dat.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.dat.HomeNID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        }

                        txDBIDTransactions.erase(keyDBID);

                        // set overlappable DBID mapping
                        txDBIDOverlappableTransactions[keyDBIDOvlp] = xaction;
                    }
                }
            }

            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));
            }
        }
        else // SNP
        {
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction, datFlit.SrcID()));
                    
                rxsnpid_t key;
                key.value   = 0;
                key.id.tgt  = datFlit.SrcID();
                key.id.src  = xaction->GetFirst().flit.snp.SrcID();
                key.id.txn  = xaction->GetFirst().flit.snp.TxnID();

                rxTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum RNFJoint<config>::NextRXDAT(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::DAT<config>&         datFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        FiredResponseFlit<config> firedDatFlit(XactScope::Requester, false, time, datFlit);

        std::shared_ptr<Xaction<config>> xaction;

        // DCT or Read
        txreqid_t key;
        key.value   = 0;
        key.id.src  = datFlit.TgtID();
        key.id.txn  = datFlit.TxnID();

        auto xactionIter = txTransactions.find(key);
        if (xactionIter == txTransactions.end())
            return this->ResponseDeniedByJoint(XactDenial::DENIED_DAT_TXNID_NOT_EXIST, firedDatFlit);

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDAT(glbl, firedDatFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByXaction(denial, firedDatFlit, xaction);

        if (hasDBID)
        {
            txreqdbid_t keyDBID;
            keyDBID.value   = 0;
            keyDBID.id.tgt  = datFlit.TgtID();
            keyDBID.id.src  = datFlit.HomeNID();
            keyDBID.id.db   = datFlit.DBID();

            if (firstDBID)
            {
                // first DBID always not overlappable

                if (txDBIDTransactions.contains(keyDBID))
                {
                    xaction->SetLastDenial(XactDenial::DENIED_DAT_DBID_IN_USE);
                    return this->ResponseDeniedByJoint(XactDenial::DENIED_DAT_DBID_IN_USE, firedDatFlit, xaction);
                }

                txDBIDTransactions[keyDBID] = xaction;

                // on DBID allocation
                this->events->OnDBIDAllocation(JointXactionDBIDAllocationEvent<config>(*this, xaction));
            }
            else
            {
                // nothing needed to be done here
                // the consistency of DBID should be checked inside Xaction
            }
        }

        if (xaction->GetFirst().IsREQ())
        {
            // on TxnID free
            if (xaction->IsTxnIDComplete(glbl))
            {
                // event on TxnID free
                this->events->OnTxnIDFree(JointXactionTxnIDFreeEvent<config>(*this, xaction));

                if (xaction->GetFirst().IsREQ())
                {
                    // remove related TxnID mapping
                    txreqid_t key;
                    key.value   = 0;
                    key.id.src  = xaction->GetFirst().flit.req.SrcID();
                    key.id.txn  = xaction->GetFirst().flit.req.TxnID();

                    txTransactions.erase(key);
                }
            }

            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));

                    // check overlappable DBID mapping first
                    txreqdbidovlp_t keyDBIDOvlp;
                    keyDBIDOvlp.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();
                    }
                    else
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.dat.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.dat.TxnID();
                    }

                    if (!txDBIDOverlappableTransactions.erase(keyDBIDOvlp))
                    {
                        // erase normal DBID mapping
                        // only when overlappable DBID mapping absent

                        txreqdbid_t keyDBID;
                        keyDBID.value   = 0;
                        if (xactionDBIDSource->IsRSP())
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        }
                        else
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.dat.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.dat.HomeNID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        }

                        // check TxnID for nno-overlappable DBID on DBID complete
                        auto xactionDBIDIter = txDBIDTransactions.find(keyDBID);
                        if (xactionDBIDIter != txDBIDTransactions.end())
                        {
                            auto xactionDBID = xactionDBIDIter->second;
                            if (xactionDBID->GetFirst().flit.req.TxnID() == xaction->GetFirst().flit.req.TxnID())
                                txDBIDTransactions.erase(xactionDBIDIter);
                        }
                    }
                }
            }
            // on DBID overlap
            else if (xaction->IsDBIDOverlappable(glbl))
            {
                // move DBID mapping to overlappable if not moved yet
                const FiredResponseFlit<config>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // check overlappable DBID mapping
                    txreqdbidovlp_t keyDBIDOvlp;
                    keyDBIDOvlp.value   = 0;
                    if (xactionDBIDSource->IsRSP())
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();
                    }
                    else
                    {
                        keyDBIDOvlp.id.db   = xactionDBIDSource->flit.dat.DBID();
                        keyDBIDOvlp.id.src  = xactionDBIDSource->flit.dat.SrcID();
                        keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.dat.TxnID();
                    }

                    // move on absent
                    if (txDBIDOverlappableTransactions.find(keyDBIDOvlp) == txDBIDOverlappableTransactions.end())
                    {
                        // erase normal DBID mapping
                        txreqdbid_t keyDBID;
                        keyDBID.value   = 0;
                        if (xactionDBIDSource->IsRSP())
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                        }
                        else
                        {
                            keyDBID.id.db   = xactionDBIDSource->flit.dat.DBID();
                            keyDBID.id.src  = xactionDBIDSource->flit.dat.HomeNID();
                            keyDBID.id.tgt  = xactionDBIDSource->flit.dat.TgtID();
                        }

                        txDBIDTransactions.erase(keyDBID);

                        // set overlappable DBID mapping
                        txDBIDOverlappableTransactions[keyDBIDOvlp] = xaction;
                    }
                }
            }

            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));
            }
        }
        else // SNP
        {
            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction, datFlit.TgtID()));
            
                rxsnpid_t key;
                key.value   = 0;
                key.id.tgt  = datFlit.TgtID();
                key.id.src  = xaction->GetFirst().flit.snp.SrcID();
                key.id.txn  = xaction->GetFirst().flit.snp.TxnID();

                rxTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }
}


// Implementation of: class SNFJoint
namespace /*CHI::*/Xact {
    /*
    ...
    */

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> SNFJoint<config>::ConstructNone(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retired) noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> SNFJoint<config>::ConstructHomeRead(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionHomeRead<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> SNFJoint<config>::ConstructHomeWrite(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionHomeWrite<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> SNFJoint<config>::ConstructHomeWriteZero(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionHomeWriteZero<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> SNFJoint<config>::ConstructHomeDataless(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionHomeDataless<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> SNFJoint<config>::ConstructHomeAtomic(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept
    {
        return std::make_shared<XactionHomeAtomic<config>>(glbl, reqFlit, retried);
    }

    template<FlitConfigurationConcept config>
    inline SNFJoint<config>::SNFJoint() noexcept
    {
        // TXREQ transactions
        #define SET_REQ_XACTION(opcode, type) \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&SNFJoint<config>::Construct##type)

            SET_REQ_XACTION(ReqLCrdReturn               , None              );  // 0x00
            SET_REQ_XACTION(ReadShared                  , None              );  // 0x01
            SET_REQ_XACTION(ReadClean                   , None              );  // 0x02
            SET_REQ_XACTION(ReadOnce                    , None              );  // 0x03
            SET_REQ_XACTION(ReadNoSnp                   , HomeRead          );  // 0x04
            SET_REQ_XACTION(PCrdReturn                  , None              );  // 0x05
                                                                                // 0x06
            SET_REQ_XACTION(ReadUnique                  , None              );  // 0x07
            SET_REQ_XACTION(CleanShared                 , HomeDataless      );  // 0x08
            SET_REQ_XACTION(CleanInvalid                , HomeDataless      );  // 0x09
            SET_REQ_XACTION(MakeInvalid                 , HomeDataless      );  // 0x0A
            SET_REQ_XACTION(CleanUnique                 , None              );  // 0x0B
            SET_REQ_XACTION(MakeUnique                  , None              );  // 0x0C
            SET_REQ_XACTION(Evict                       , None              );  // 0x0D
                                                                                // 0x0E
                                                                                // 0x0F
                                                                                // 0x10
            SET_REQ_XACTION(ReadNoSnpSep                , HomeRead          );  // 0x11
                                                                                // 0x12
            SET_REQ_XACTION(CleanSharedPersistSep       , HomeDataless      );  // 0x13
            SET_REQ_XACTION(DVMOp                       , None              );  // 0x14
            SET_REQ_XACTION(WriteEvictFull              , None              );  // 0x15
                                                                                // 0x16
            SET_REQ_XACTION(WriteCleanFull              , None              );  // 0x17
            SET_REQ_XACTION(WriteUniquePtl              , None              );  // 0x18
            SET_REQ_XACTION(WriteUniqueFull             , None              );  // 0x19
            SET_REQ_XACTION(WriteBackPtl                , None              );  // 0x1A
            SET_REQ_XACTION(WriteBackFull               , None              );  // 0x1B
            SET_REQ_XACTION(WriteNoSnpPtl               , HomeWrite         );  // 0x1C
            SET_REQ_XACTION(WriteNoSnpFull              , HomeWrite         );  // 0x1D
                                                                                // 0x1E
                                                                                // 0x1F
            SET_REQ_XACTION(WriteUniqueFullStash        , None              );  // 0x20
            SET_REQ_XACTION(WriteUniquePtlStash         , None              );  // 0x21
            SET_REQ_XACTION(StashOnceShared             , None              );  // 0x22
            SET_REQ_XACTION(StashOnceUnique             , None              );  // 0x23
            SET_REQ_XACTION(ReadOnceCleanInvalid        , None              );  // 0x24
            SET_REQ_XACTION(ReadOnceMakeInvalid         , None              );  // 0x25
            SET_REQ_XACTION(ReadNotSharedDirty          , None              );  // 0x26
            SET_REQ_XACTION(CleanSharedPersist          , HomeDataless      );  // 0x27
            SET_REQ_XACTION(AtomicStore::ADD            , HomeAtomic        );  // 0x28
            SET_REQ_XACTION(AtomicStore::CLR            , HomeAtomic        );  // 0x29
            SET_REQ_XACTION(AtomicStore::EOR            , HomeAtomic        );  // 0x2A
            SET_REQ_XACTION(AtomicStore::SET            , HomeAtomic        );  // 0x2B
            SET_REQ_XACTION(AtomicStore::SMAX           , HomeAtomic        );  // 0x2C
            SET_REQ_XACTION(AtomicStore::SMIN           , HomeAtomic        );  // 0x2D
            SET_REQ_XACTION(AtomicStore::UMAX           , HomeAtomic        );  // 0x2E
            SET_REQ_XACTION(AtomicStore::UMIN           , HomeAtomic        );  // 0x2F
            SET_REQ_XACTION(AtomicLoad::ADD             , HomeAtomic        );  // 0x30
            SET_REQ_XACTION(AtomicLoad::CLR             , HomeAtomic        );  // 0x31
            SET_REQ_XACTION(AtomicLoad::EOR             , HomeAtomic        );  // 0x32
            SET_REQ_XACTION(AtomicLoad::SET             , HomeAtomic        );  // 0x33
            SET_REQ_XACTION(AtomicLoad::SMAX            , HomeAtomic        );  // 0x34
            SET_REQ_XACTION(AtomicLoad::SMIN            , HomeAtomic        );  // 0x35
            SET_REQ_XACTION(AtomicLoad::UMAX            , HomeAtomic        );  // 0x36
            SET_REQ_XACTION(AtomicLoad::UMIN            , HomeAtomic        );  // 0x37
            SET_REQ_XACTION(AtomicSwap                  , HomeAtomic        );  // 0x38
            SET_REQ_XACTION(AtomicCompare               , HomeAtomic        );  // 0x39
            SET_REQ_XACTION(PrefetchTgt                 , None              );  // 0x3A
                                                                                // 0x3B
                                                                                // 0x3C
                                                                                // 0x3D
                                                                                // 0x3E
                                                                                // 0x3F
                                                                                // 0x40
            SET_REQ_XACTION(MakeReadUnique              , None              );  // 0x41
            SET_REQ_XACTION(WriteEvictOrEvict           , None              );  // 0x42
            SET_REQ_XACTION(WriteUniqueZero             , None              );  // 0x43
            SET_REQ_XACTION(WriteNoSnpZero              , HomeWriteZero     );  // 0x44
                                                                                // 0x45
                                                                                // 0x46
            SET_REQ_XACTION(StashOnceSepShared          , None              );  // 0x47
            SET_REQ_XACTION(StashOnceSepUnique          , None              );  // 0x48
                                                                                // 0x49
                                                                                // 0x4A
                                                                                // 0x4B
            SET_REQ_XACTION(ReadPreferUnique            , None              );  // 0x4C
                                                                                // 0x4D
                                                                                // 0x4E
                                                                                // 0x4F
            SET_REQ_XACTION(WriteNoSnpFullCleanSh       , None              );  // 0x50
            SET_REQ_XACTION(WriteNoSnpFullCleanInv      , None              );  // 0x51
            SET_REQ_XACTION(WriteNoSnpFullCleanShPerSep , None              );  // 0x52
                                                                                // 0x53
            SET_REQ_XACTION(WriteUniqueFullCleanSh      , None              );  // 0x54
                                                                                // 0x55
            SET_REQ_XACTION(WriteUniqueFullCleanShPerSep, None              );  // 0x56
                                                                                // 0x57
            SET_REQ_XACTION(WriteBackFullCleanSh        , None              );  // 0x58
            SET_REQ_XACTION(WriteBackFullCleanInv       , None              );  // 0x59
            SET_REQ_XACTION(WriteBackFullCleanShPerSep  , None              );  // 0x5A
                                                                                // 0x5B
            SET_REQ_XACTION(WriteCleanFullCleanSh       , None              );  // 0x5C
                                                                                // 0x5D
            SET_REQ_XACTION(WriteCleanFullCleanShPerSep , None              );  // 0x5E
                                                                                // 0x5F
            SET_REQ_XACTION(WriteNoSnpPtlCleanSh        , None              );  // 0x60
            SET_REQ_XACTION(WriteNoSnpPtlCleanInv       , None              );  // 0x61
            SET_REQ_XACTION(WriteNoSnpPtlCleanShPerSep  , None              );  // 0x62
                                                                                // 0x63
            SET_REQ_XACTION(WriteUniquePtlCleanSh       , None              );  // 0x64
                                                                                // 0x65
            SET_REQ_XACTION(WriteUniquePtlCleanShPerSep , None              );  // 0x66
                                                                                // 0x67
                                                                                // 0x68
                                                                                // 0x69
                                                                                // 0x6A
                                                                                // 0x6B
                                                                                // 0x6C
                                                                                // 0x6D
                                                                                // 0x6E
                                                                                // 0x6F
                                                                                // 0x70
                                                                                // 0x71
                                                                                // 0x72
                                                                                // 0x73
                                                                                // 0x74
                                                                                // 0x75
                                                                                // 0x76
                                                                                // 0x77
                                                                                // 0x78
                                                                                // 0x79
                                                                                // 0x7A
                                                                                // 0x7B
                                                                                // 0x7C
                                                                                // 0x7D
                                                                                // 0x7E
                                                                                // 0x7F
        
        #undef SET_REQ_XACTION
    }

    template<FlitConfigurationConcept config>
    inline void SNFJoint<config>::Clear() noexcept
    {
        rxTransactions.clear();
        
        rxDBIDTransactions.clear();

        rxDBIDOverlappableTransactions.clear();

        rxRetriedTransactions.clear();

        for (int i = 0; i < (1 << Flits::RSP<config>::PCRDTYPE_WIDTH); i++)
            grantedPCredits[i].clear();
    }

    template<FlitConfigurationConcept config>
    inline XactScopeEnum SNFJoint<config>::GetActiveScope() const noexcept
    {
        return XactScope::Subordinate;
    }

    template<FlitConfigurationConcept config>
    inline void SNFJoint<config>::GetInflight(
        const Global<config>&                             glbl,
        std::vector<std::shared_ptr<Xaction<config>>>&    dstVector,
        bool                                              sortByTime
    ) const noexcept
    {
        for (auto& rxTransaction : rxTransactions)
            dstVector.push_back(rxTransaction.second);

        for (auto& rxDBIDTransaction : rxDBIDTransactions)
        {
            if (!rxDBIDTransaction.second->IsTxnIDComplete(glbl))
                continue;
            else
                dstVector.push_back(rxDBIDTransaction.second);
        }

        for (auto& rxDBIDOverlappableTransaction : rxDBIDOverlappableTransactions)
        {
            if (!rxDBIDOverlappableTransaction.second->IsTxnIDComplete(glbl))
                continue;
            else
                dstVector.push_back(rxDBIDOverlappableTransaction.second);
        }

        if (sortByTime)
        {
            std::sort(dstVector.begin(), dstVector.end(),
                [] (std::shared_ptr<Xaction<config>> a, std::shared_ptr<Xaction<config>> b)
                {
                    return a->GetFirst().time < b->GetFirst().time;
                }
            );
        }
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum SNFJoint<config>::NextRXREQ(
        const Global<config>&             glbl,
        uint64_t                           time,
        const Flits::REQ<config>&         reqFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        rxreqid_t key;
        key.value   = 0;
        key.id.src  = reqFlit.SrcID();
        key.id.txn  = reqFlit.TxnID();

        bool retry = false;

        //
        FiredRequestFlit<config> firedReqFlit(XactScope::Subordinate, false, time, reqFlit);

        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, GetXaction>& opcodeInfo =
            reqDecoder.DecodeSNF(reqFlit.Opcode());

        if (!opcodeInfo.IsValid()) // unknown opcode
            return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_OPCODE_NOT_DECODED, firedReqFlit);

        //
        if (reqFlit.AllowRetry() == 0)
        {
            if (rxTransactions.contains(key))
                return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_TXNID_IN_USE, firedReqFlit, rxTransactions[key]);
        
            // iterate and compare retry transactions

            // TODO: retry query

            return this->RequestDeniedByJoint(XactDenial::DENIED_UNSUPPORTED_FEATURE, firedReqFlit);
        }
        else
        {
            if (rxTransactions.contains(key))
                return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_TXNID_IN_USE, firedReqFlit, rxTransactions[key]);

            std::shared_ptr<Xaction<config>> xaction;

            xaction = opcodeInfo.GetCompanion()(glbl, firedReqFlit, nullptr);

            if (!xaction) // unsupported opcode transactions
                return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, firedReqFlit);

            if (theXaction)
                *theXaction = xaction;

            if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
                return this->RequestDeniedByXaction(xaction->GetFirstDenial(), firedReqFlit, xaction);

            // event on Request xaction acccepted
            this->events->OnAccepted(JointXactionAcceptedEvent<config>(*this, xaction));

            // event on TxnID allocation
            this->events->OnTxnIDAllocation(JointXactionTxnIDAllocationEvent<config>(*this, xaction));

            rxTransactions[key] = xaction;
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum SNFJoint<config>::NextTXRSP(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::RSP<config>&         rspFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        FiredResponseFlit<config> firedRspFlit(XactScope::Subordinate, true, time, rspFlit);

        // P-Credit Grant
        if (rspFlit.Opcode() == Opcodes::RSP::PCrdGrant)
        {
            pcrd_t pCrdKey;
            pCrdKey.value   = 0;
            pCrdKey.id.src  = rspFlit.SrcID();
            pCrdKey.id.tgt  = rspFlit.TgtID();

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config>>>& grantedPCredits
                = this->grantedPCredits[rspFlit.PCrdType()];

            auto iterPCreditList = grantedPCredits.try_emplace(pCrdKey).first;

            iterPCreditList->second.push_back(firedRspFlit);

            //
            if (glbl.CHECK_FIELD_MAPPING.enable)
            {
                XactDenialEnum denial = glbl.CHECK_FIELD_MAPPING.Check(rspFlit);
                if (denial != XactDenial::ACCEPTED)
                    return this->ResponseDeniedByJoint(denial, firedRspFlit);
            }

            return XactDenial::ACCEPTED;
        }

        std::shared_ptr<Xaction<config>> xaction;

        // RSPs to requester do not apply TxnID with DBID,
        // but may pass new DBID to requester.

        rxreqid_t key;
        key.value   = 0;
        key.id.src  = rspFlit.TgtID();
        key.id.txn  = rspFlit.TxnID();

        auto xactionIter = rxTransactions.find(key);
        if (xactionIter == rxTransactions.end())
            return this->ResponseDeniedByJoint(XactDenial::DENIED_RSP_TXNID_NOT_EXIST, firedRspFlit);

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextRSP(glbl, firedRspFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByJoint(denial, firedRspFlit, xaction);

        if (hasDBID)
        {
            // on DWT, the DBID was allocated to RN

            rxreqdbid_t keyDBID;
            keyDBID.value   = 0;
            keyDBID.id.tgt  = rspFlit.TgtID();
            keyDBID.id.src  = rspFlit.SrcID();
            keyDBID.id.db   = rspFlit.DBID();

            if (firstDBID)
            {
                // first DBID always not overlappable

                if (rxDBIDTransactions.contains(keyDBID))
                {
                    xaction->SetLastDenial(XactDenial::DENIED_RSP_DBID_IN_USE);
                    return this->ResponseDeniedByJoint(XactDenial::DENIED_RSP_DBID_IN_USE, firedRspFlit, rxDBIDTransactions[keyDBID]);
                }

                rxDBIDTransactions[keyDBID] = xaction;

                // event on DBID allocation
                this->events->OnDBIDAllocation(JointXactionDBIDAllocationEvent<config>(*this, xaction));
            }
            else
            {
                // nothing needed to be done here
                // the consistency of DBID should be checked inside Xaction
            }
        }

        assert(xaction->GetFirst().IsREQ() && "no SNP for SN-F");

        // on TxnID free
        if (xaction->IsTxnIDComplete(glbl))
        {
            // event on TxnID free
            this->events->OnTxnIDFree(JointXactionTxnIDFreeEvent<config>(*this, xaction));

            // remove related TxnID mapping
            rxreqid_t key;
            key.value   = 0;
            key.id.src  = xaction->GetFirst().flit.req.SrcID();
            key.id.txn  = xaction->GetFirst().flit.req.TxnID();

            rxTransactions.erase(key);
        }

        // on DBID free
        if (xaction->IsDBIDComplete(glbl))
        {
            // remove related DBID mapping
            const FiredResponseFlit<config>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // event on DBID free
                this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));

                // check overlappable DBID mapping first
                rxreqdbidovlp_t keyDBIDOvlp;
                keyDBIDOvlp.value   = 0;
                keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();

                if (!rxDBIDOverlappableTransactions.erase(keyDBIDOvlp))
                {
                    // erase normal DBID mapping
                    // only when overlappable DBID mapping absent

                    rxreqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                    keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                    keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();

                    // check TxnID for non-overlappable DBID on DBID complete
                    auto xactionDBIDIter = rxDBIDTransactions.find(keyDBID);
                    if (xactionDBIDIter != rxDBIDTransactions.end())
                    {
                        auto xactionDBID = xactionDBIDIter->second;
                        if (xactionDBID->GetFirst().flit.req.TxnID() == xaction->GetFirst().flit.req.TxnID())
                            rxDBIDTransactions.erase(xactionDBIDIter);
                    }
                }
            }
        }
        // on DBID overlap
        else if (xaction->IsDBIDOverlappable(glbl))
        {
            // move DBID mapping to overlappable if not moved yet
            const FiredResponseFlit<config>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // check overlappable DBID mapping
                rxreqdbidovlp_t keyDBIDOvlp;
                keyDBIDOvlp.value   = 0;
                keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();

                // move on absent
                if (rxDBIDOverlappableTransactions.find(keyDBIDOvlp) == rxDBIDOverlappableTransactions.end())
                {
                    // erase normal DBID mapping
                    rxreqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                    keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                    keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();

                    rxDBIDTransactions.erase(keyDBID);

                    // set overlappable DBID mapping
                    rxDBIDOverlappableTransactions[keyDBIDOvlp] = xaction;
                }
            }
        }

        // on completion
        if (xaction->IsComplete(glbl))
        {
            // event on completion
            this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));

            if (xaction->GotRetryAck())
            {
                // only on TXRSP, transactions might be completed by retry

                // record as retried
                hnsrc_t hnKey;
                hnKey.value     = 0;
                hnKey.id.src    = rspFlit.TgtID();

                rxRetriedTransactions[hnKey].push_back(xaction);
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum SNFJoint<config>::NextTXDAT(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::DAT<config>&         datFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        FiredResponseFlit<config> firedDatFlit(XactScope::Subordinate, true, time, datFlit);

        std::shared_ptr<Xaction<config>> xaction;

        // DMT or Home Read
        rxreqid_t key;
        if (firedDatFlit.IsToRequester(glbl))
        {
            // DMT
            key.id.src  = datFlit.HomeNID();
            key.id.txn  = datFlit.DBID();
        }
        else
        {
            // Home Read
            key.id.src  = datFlit.TgtID();
            key.id.txn  = datFlit.TxnID();
        }

        auto xactionIter = rxTransactions.find(key);
        if (xactionIter == rxTransactions.end())
            return this->ResponseDeniedByJoint(XactDenial::DENIED_DAT_TXNID_NOT_EXIST, firedDatFlit);

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDAT(glbl, firedDatFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByXaction(denial, firedDatFlit, xaction);

        assert(!hasDBID && !firstDBID && "TXDAT never generates DBID on SN-F");

        assert(xaction->GetFirst().IsREQ() && "no SNP for SN-F");

        // on TxnID free
        if (xaction->IsTxnIDComplete(glbl))
        {
            // event on TxnID free
            this->events->OnTxnIDFree(JointXactionTxnIDFreeEvent<config>(*this, xaction));

            // remove related TxnID mapping
            rxreqid_t key;
            key.value   = 0;
            key.id.src  = xaction->GetFirst().flit.req.SrcID();
            key.id.txn  = xaction->GetFirst().flit.req.TxnID();

            rxTransactions.erase(key);
        }

        // on DBID free
        if (xaction->IsDBIDComplete(glbl))
        {
            // remove related DBID mapping
            const FiredResponseFlit<config>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // event on DBID free
                this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));

                // check overlappable DBID mapping first
                rxreqdbidovlp_t keyDBIDOvlp;
                keyDBIDOvlp.value   = 0;
                keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();

                if (!rxDBIDOverlappableTransactions.erase(keyDBIDOvlp))
                {
                    // erase normal DBID mapping
                    // only when overlappable DBID mapping absent

                    rxreqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                    keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                    keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();

                    // check TxnID for non-overlappable DBID on DBID complete
                    auto xactionDBIDIter = rxDBIDTransactions.find(keyDBID);
                    if (xactionDBIDIter != rxDBIDTransactions.end())
                    {
                        auto xactionDBID = xactionDBIDIter->second;
                        if (xactionDBID->GetFirst().flit.req.TxnID() == xaction->GetFirst().flit.req.TxnID())
                            rxDBIDTransactions.erase(xactionDBIDIter);
                    }
                }
            }
        }
        // on DBID overlap
        else if (xaction->IsDBIDOverlappable(glbl))
        {
            // move DBID mapping to overlappable if not moved yet
            const FiredResponseFlit<config>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // check overlappable DBID mapping
                rxreqdbidovlp_t keyDBIDOvlp;
                keyDBIDOvlp.value   = 0;
                keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();

                // move on absent
                if (rxDBIDOverlappableTransactions.find(keyDBIDOvlp) == rxDBIDOverlappableTransactions.end())
                {
                    // erase normal DBID mapping
                    rxreqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                    keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                    keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();

                    rxDBIDTransactions.erase(keyDBID);

                    // set overlappable DBID mapping
                    rxDBIDOverlappableTransactions[keyDBIDOvlp] = xaction;
                }
            }
        }

        if (xaction->IsComplete(glbl))
        {
            // event on completion
            this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config>
    inline XactDenialEnum SNFJoint<config>::NextRXDAT(
        const Global<config>&             glbl,
        uint64_t                          time,
        const Flits::DAT<config>&         datFlit,
        std::shared_ptr<Xaction<config>>* theXaction) noexcept
    {
        FiredResponseFlit<config> firedDatFlit(XactScope::Subordinate, false, time, datFlit);

        std::shared_ptr<Xaction<config>> xaction;

        // DWT or Write
        rxreqdbid_t key;
        key.value   = 0;
        key.id.tgt  = datFlit.SrcID();
        key.id.src  = datFlit.TgtID();
        key.id.db   = datFlit.TxnID();

        auto xactionIter = rxDBIDTransactions.find(key);
        if (xactionIter == rxDBIDTransactions.end())
            return this->ResponseDeniedByJoint(XactDenial::DENIED_DAT_TXNID_NOT_EXIST, firedDatFlit);

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDAT(glbl, firedDatFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByXaction(denial, firedDatFlit, xaction);

        assert(xaction->GetFirst().IsREQ() && "no SNP on SN-F");

        // *NOTICE: TxnID not deallocating on RXDAT,
        //          since RXDAT depends on DBID response
        
        // on DBID free
        if (xaction->IsDBIDComplete(glbl))
        {
            // remove related DBID mapping
            const FiredResponseFlit<config>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // event on DBID free
                this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));

                // check overlappable DBID mapping first
                rxreqdbidovlp_t keyDBIDOvlp;
                keyDBIDOvlp.value   = 0;
                keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();

                if (!rxDBIDOverlappableTransactions.erase(keyDBIDOvlp))
                {
                    // erase normal DBID mapping
                    // only when overlappable DBID mapping absent

                    rxreqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                    keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                    keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();

                    // check TxnID for non-overlappable DBID on DBID complete
                    auto xactionDBIDIter = rxDBIDTransactions.find(keyDBID);
                    if (xactionDBIDIter != rxDBIDTransactions.end())
                    {
                        auto xactionDBID = xactionDBIDIter->second;
                        if (xactionDBID->GetFirst().flit.req.TxnID() == xaction->GetFirst().flit.req.TxnID())
                            rxTransactions.erase(xactionDBIDIter);
                    }
                }
            }
        }
        // on DBID overlap
        else if (xaction->IsDBIDOverlappable(glbl))
        {
            // move DBID mapping to overlappable if not moved yet
            const FiredResponseFlit<config>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // check overlappable DBID maping
                rxreqdbidovlp_t keyDBIDOvlp;
                keyDBIDOvlp.value   = 0;
                keyDBIDOvlp.id.db   = xactionDBIDSource->flit.rsp.DBID();
                keyDBIDOvlp.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                keyDBIDOvlp.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();
                keyDBIDOvlp.id.txn  = xactionDBIDSource->flit.rsp.TxnID();

                // move on absent
                if (rxDBIDOverlappableTransactions.find(keyDBIDOvlp) == rxDBIDOverlappableTransactions.end())
                {
                    // erase normal DBID maping
                    rxreqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    keyDBID.id.db   = xactionDBIDSource->flit.rsp.DBID();
                    keyDBID.id.src  = xactionDBIDSource->flit.rsp.SrcID();
                    keyDBID.id.tgt  = xactionDBIDSource->flit.rsp.TgtID();

                    rxDBIDTransactions.erase(keyDBID);

                    // set overlappable DBID mapping
                    rxDBIDOverlappableTransactions[keyDBIDOvlp] = xaction;
                }
            }
        }

        if (xaction->IsComplete(glbl))
        {
            // event on completion
            this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));
        }

        return XactDenial::ACCEPTED;
    }
}


#endif // __CHI__CHI_XACT_JOINT_*
