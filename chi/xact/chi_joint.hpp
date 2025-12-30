//#pragma once

//#ifndef __CHI__CHI_XACT_JOINT
//#define __CHI__CHI_XACT_JOINT

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
        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class Joint;


        // Joint Events
        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionEventBase {
        protected:
            Joint<config, conn>&                    joint;
            std::shared_ptr<Xaction<config, conn>>  xaction;

        public:
            JointXactionEventBase(Joint<config, conn>&                      joint, 
                                  std::shared_ptr<Xaction<config, conn>>    xaction) noexcept;

        public:
            Joint<config, conn>&                            GetJoint() noexcept;
            const Joint<config, conn>&                      GetJoint() const noexcept;

            std::shared_ptr<Xaction<config, conn>>          GetXaction() noexcept;
            std::shared_ptr<const Xaction<config, conn>>    GetXaction() const noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionAcceptedEvent : public JointXactionEventBase<config, conn>, 
                                          public Gravity::Event<JointXactionAcceptedEvent<config, conn>> {
        public:
            JointXactionAcceptedEvent(Joint<config, conn>&                      joint, 
                                      std::shared_ptr<Xaction<config, conn>>    xaction) noexcept;
                                    
            JointXactionAcceptedEvent(Joint<config, conn>&                      joint,
                                      std::shared_ptr<Xaction<config, conn>>    xaction,
                                      Flits::REQ<config, conn>::srcid_t         snpTgtId) noexcept;

        private:
            Flits::REQ<config, conn>::srcid_t   snoopTargetID;

        public:
            Flits::REQ<config, conn>::srcid_t   GetSnoopTargetID() const noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionRetryEvent : public JointXactionEventBase<config, conn>,
                                       public Gravity::Event<JointXactionRetryEvent<config, conn>> {
        public:
            JointXactionRetryEvent(Joint<config, conn>&                     joint, 
                                   std::shared_ptr<Xaction<config, conn>>   xaction) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionTxnIDAllocationEvent : public JointXactionEventBase<config, conn>,
                                                 public Gravity::Event<JointXactionTxnIDAllocationEvent<config, conn>> {
        public:
            JointXactionTxnIDAllocationEvent(Joint<config, conn>&                      joint,
                                             std::shared_ptr<Xaction<config, conn>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionTxnIDFreeEvent : public JointXactionEventBase<config, conn>,
                                           public Gravity::Event<JointXactionTxnIDFreeEvent<config, conn>> {
        public:
            JointXactionTxnIDFreeEvent(Joint<config, conn>&                      joint,
                                       std::shared_ptr<Xaction<config, conn>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionDBIDAllocationEvent : public JointXactionEventBase<config, conn>,
                                                public Gravity::Event<JointXactionDBIDAllocationEvent<config, conn>> {
        public:
            JointXactionDBIDAllocationEvent(Joint<config, conn>&                      joint,
                                            std::shared_ptr<Xaction<config, conn>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionDBIDFreeEvent : public JointXactionEventBase<config, conn>,
                                          public Gravity::Event<JointXactionDBIDFreeEvent<config, conn>> {
        public:
            JointXactionDBIDFreeEvent(Joint<config, conn>&                      joint,
                                      std::shared_ptr<Xaction<config, conn>>    xaction) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class JointXactionCompleteEvent : public JointXactionEventBase<config, conn>,
                                          public Gravity::Event<JointXactionCompleteEvent<config, conn>> {
        public:
            JointXactionCompleteEvent(Joint<config, conn>&                      joint,
                                      std::shared_ptr<Xaction<config, conn>>    xaction) noexcept;

            JointXactionCompleteEvent(Joint<config, conn>&                      joint,
                                      std::shared_ptr<Xaction<config, conn>>    xaction,
                                      Flits::REQ<config, conn>::srcid_t         snpTgtId) noexcept;

        private:
            Flits::REQ<config, conn>::srcid_t   snoopTargetID;

        public:
            Flits::REQ<config, conn>::srcid_t   GetSnoopTargetID() const noexcept;
        };


        // Joint Base
        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        class Joint {
        public:
            Gravity::EventBus<JointXactionAcceptedEvent<config, conn>>  OnAccepted;
            Gravity::EventBus<JointXactionRetryEvent<config, conn>>     OnRetry;
            Gravity::EventBus<JointXactionTxnIDAllocationEvent<config, conn>>
                                                                        OnTxnIDAllocation;
            Gravity::EventBus<JointXactionTxnIDFreeEvent<config, conn>> OnTxnIDFree;
            Gravity::EventBus<JointXactionDBIDAllocationEvent<config, conn>>
                                                                        OnDBIDAllocation;
            Gravity::EventBus<JointXactionDBIDFreeEvent<config, conn>>  OnDBIDFree;
            Gravity::EventBus<JointXactionCompleteEvent<config, conn>>  OnComplete;

        public:
            Joint() noexcept;

        public:
            virtual void            Clear() noexcept = 0;

            void                    ClearListeners() noexcept;

        public:
            virtual XactDenialEnum  NextTXREQ(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::REQ<config, conn>&         reqFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextRXREQ(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::REQ<config, conn>&         reqFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextTXSNP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::SNP<config, conn>&         snpFlit,
                const Flits::REQ<config, conn>::srcid_t snpTgtId,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;
            
            virtual XactDenialEnum  NextRXSNP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::SNP<config, conn>&         snpFlit,
                const Flits::REQ<config, conn>::srcid_t snpTgtId,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextTXRSP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::RSP<config, conn>&         rspFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextRXRSP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::RSP<config, conn>&         rspFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;
            
            virtual XactDenialEnum  NextTXDAT(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::DAT<config, conn>&         datFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;

            virtual XactDenialEnum  NextRXDAT(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::DAT<config, conn>&         rspFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept;
        };


        // RN-F Joint
        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class RNFJoint : public Joint<config, conn> {
        public:
            using txreqid_t = union __txreqid_t {
                struct {
                    Flits::REQ<config, conn>::txnid_t   txn;
                    Flits::REQ<config, conn>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using txreqdbid_t = union __txreqdbid_t {
                struct {
                    Flits::RSP<config, conn>::dbid_t    db;     // same for DAT
                    Flits::RSP<config, conn>::srcid_t   src;    // same for DAT
                    Flits::RSP<config, conn>::tgtid_t   tgt;    // same for DAT
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using txreqdbidovlp_t = union __txreqdbidovlp_t {
                struct {
                    Flits::RSP<config, conn>::dbid_t    db;     // same for DAT
                    Flits::RSP<config, conn>::srcid_t   src;    // same for DAT
                    Flits::RSP<config, conn>::tgtid_t   tgt;    // same for DAT
                    Flits::RSP<config, conn>::txnid_t   txn;    // same for DAT
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rxsnpid_t = union __rxsnpid_t {
                struct {
                    Flits::SNP<config, conn>::txnid_t   txn;
                    Flits::SNP<config, conn>::srcid_t   src;
                    Flits::REQ<config, conn>::srcid_t   tgt;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };
            
            using pcrd_t = union __pcrd_t {
                struct {
                    Flits::RSP<config, conn>::tgtid_t   tgt;
                    Flits::RSP<config, conn>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rnsrc_t = union __rnsrc_t {
                struct {
                    Flits::REQ<config, conn>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

        private:
            using GetXaction = std::function<std::shared_ptr<Xaction<config, conn>>(
                Global<config, conn>*,
                const Topology&,
                const FiredRequestFlit<config, conn>&,
                std::shared_ptr<Xaction<config, conn>>)>;

        private:
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        txTransactions;
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        rxTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        txDBIDTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        txDBIDOverlappableTransactions;

            std::unordered_map<uint64_t, std::list<std::shared_ptr<Xaction<config, conn>>>>
                                                                                        txRetriedTransactions;

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config, conn>>>    grantedPCredits[1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH];

            Opcodes::REQ::Decoder<Flits::REQ<config>, GetXaction>                       reqDecoder;
            Opcodes::SNP::Decoder<Flits::SNP<config>, GetXaction>                       snpDecoder;

        protected:
            static std::shared_ptr<Xaction<config, conn>>   ConstructNone(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructAllocatingRead(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructNonAllocatingRead(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructImmediateWrite(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructWriteZero(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructCopyBackWrite(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructDataless(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructHomeSnoop(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructForwardSnoop(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructAtomic(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructIndependentStash(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;

        public:
            RNFJoint() noexcept;

        public:
            RNFJoint(const RNFJoint<config, conn>& obj) noexcept;
            RNFJoint<config, conn>& operator=(const RNFJoint<config, conn>& obj) noexcept;

        public:
            void                    Fork() noexcept;

        public:
            virtual void            Clear() noexcept override;

            virtual void            GetInflight(
                const Topology&                                         topo,
                std::vector<std::shared_ptr<Xaction<config, conn>>>&    dstVector,
                bool                                                    sortByTime = false
            ) const noexcept;

        public:
            virtual XactDenialEnum  NextTXREQ(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::REQ<config, conn>&         reqFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextRXSNP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::SNP<config, conn>&         snpFlit,
                const Flits::REQ<config, conn>::srcid_t snpTgtId,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextTXRSP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::RSP<config, conn>&         rspFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextRXRSP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::RSP<config, conn>&         rspFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextTXDAT(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::DAT<config, conn>&         datFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;
            
            virtual XactDenialEnum  NextRXDAT(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::DAT<config, conn>&         datFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;
        };


        // SN-F Joint
        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class SNFJoint : public Joint<config, conn> {
        public:
            using rxreqid_t = union __rxreqid_t {
                struct {
                    Flits::REQ<config, conn>::txnid_t   txn;
                    Flits::REQ<config, conn>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rxreqdbid_t = union __rxreqdbid_t {
                struct {
                    Flits::RSP<config, conn>::dbid_t    db;
                    Flits::RSP<config, conn>::srcid_t   src;
                    Flits::RSP<config, conn>::tgtid_t   tgt;
                }           id; 
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using rxreqdbidovlp_t = union __rxreqdbidovlp_t {
                struct {
                    Flits::RSP<config, conn>::dbid_t    db;
                    Flits::RSP<config, conn>::srcid_t   src;
                    Flits::RSP<config, conn>::tgtid_t   tgt;
                    Flits::RSP<config, conn>::txnid_t   txn;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using pcrd_t = union __pcrd_t {
                struct {
                    Flits::RSP<config, conn>::tgtid_t   tgt;
                    Flits::RSP<config, conn>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

            using hnsrc_t = union __hnsrc_t {
                struct {
                    Flits::REQ<config, conn>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

        private:
            using GetXaction = std::function<std::shared_ptr<Xaction<config, conn>>(
                Global<config, conn>*,
                const Topology&,
                const FiredRequestFlit<config, conn>&,
                std::shared_ptr<Xaction<config, conn>>)>;

        private:
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        rxTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        rxDBIDTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        rxDBIDOverlappableTransactions;

            std::unordered_map<uint64_t, std::list<std::shared_ptr<Xaction<config, conn>>>>
                                                                                        rxRetriedTransactions;

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config, conn>>>    grantedPCredits[1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH];

            Opcodes::REQ::Decoder<Flits::REQ<config>, GetXaction>                       reqDecoder;

        protected:
            static std::shared_ptr<Xaction<config, conn>>   ConstructNone(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructHomeRead(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructHomeWrite(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructHomeWriteZero(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructHomeDataless(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructHomeAtomic(Global<config, conn>*, const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;

        public:
            SNFJoint() noexcept;

        public:
            virtual void            Clear() noexcept override;

            virtual void            GetInflight(
                const Topology&                                         topo,
                std::vector<std::shared_ptr<Xaction<config, conn>>>&    dstVector,
                bool                                                    sortByTime = false
            ) const noexcept;

        public:
            virtual XactDenialEnum  NextRXREQ(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::REQ<config, conn>&         reqFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;

            virtual XactDenialEnum  NextTXRSP(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::RSP<config, conn>&         rspFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;

            virtual XactDenialEnum  NextTXDAT(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::DAT<config, conn>&         datFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;

            virtual XactDenialEnum  NextRXDAT(
                Global<config, conn>*                   glbl,
                uint64_t                                time,
                const Topology&                         topo,
                const Flits::DAT<config, conn>&         datFlit,
                std::shared_ptr<Xaction<config, conn>>* xaction = nullptr
            ) noexcept override;
        };
    }
/*
}
*/


// Implementation of: class JointXactionEventBase
namespace /*CHI::*/Xact {
    /*
    Joint<config, conn>&                    joint;
    std::shared_ptr<Xaction<config, conn>>  xaction;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionEventBase<config, conn>::JointXactionEventBase(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : joint     (joint)
        , xaction   (xaction)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Joint<config, conn>& JointXactionEventBase<config, conn>::GetJoint() noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline const Joint<config, conn>& JointXactionEventBase<config, conn>::GetJoint() const noexcept
    {
        return joint;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> JointXactionEventBase<config, conn>::GetXaction() noexcept
    {
        return xaction;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<const Xaction<config, conn>> JointXactionEventBase<config, conn>::GetXaction() const noexcept
    {
        return xaction;
    }
}

// Implementation of: class JointXactionAcceptedEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionAcceptedEvent<config, conn>::JointXactionAcceptedEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionAcceptedEvent<config, conn>::JointXactionAcceptedEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction,
        Flits::REQ<config, conn>::srcid_t       snpTgtId
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
        , snoopTargetID                         (snpTgtId)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Flits::REQ<config, conn>::srcid_t JointXactionAcceptedEvent<config, conn>::GetSnoopTargetID() const noexcept
    {
        return snoopTargetID;
    }
}

// Implementation of: class JointXactionRetryEvent
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionRetryEvent<config, conn>::JointXactionRetryEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionCompleteEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionCompleteEvent<config, conn>::JointXactionCompleteEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
        , snoopTargetID                         (0)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionCompleteEvent<config, conn>::JointXactionCompleteEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction,
        Flits::REQ<config, conn>::srcid_t       snpTgtId
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
        , snoopTargetID                         (snpTgtId)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Flits::REQ<config, conn>::srcid_t JointXactionCompleteEvent<config, conn>::GetSnoopTargetID() const noexcept
    {
        return snoopTargetID;
    }
}

// Implementation of: class JointXactionTxnIDAllocationEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionTxnIDAllocationEvent<config, conn>::JointXactionTxnIDAllocationEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionTxnIDFreeEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionTxnIDFreeEvent<config, conn>::JointXactionTxnIDFreeEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionDBIDAllocationEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionDBIDAllocationEvent<config, conn>::JointXactionDBIDAllocationEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
    { }
}

// Implementation of: class JointXactionDBIDFreeEvent
namespace /*CHI::*/Xact {
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline JointXactionDBIDFreeEvent<config, conn>::JointXactionDBIDFreeEvent(
        Joint<config, conn>&                    joint,
        std::shared_ptr<Xaction<config, conn>>  xaction
    ) noexcept
        : JointXactionEventBase<config, conn>   (joint, xaction)
    { }
}


// Implementation of: class Joint
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline Joint<config, conn>::Joint() noexcept
        : OnAccepted            (0)
        , OnRetry               (0)
        , OnTxnIDAllocation     (0)
        , OnTxnIDFree           (0)
        , OnDBIDAllocation      (0)
        , OnDBIDFree            (0)
        , OnComplete            (0)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Joint<config, conn>::ClearListeners() noexcept
    {
        OnAccepted.UnregisterAll();
        OnRetry.UnregisterAll();
        OnTxnIDAllocation.UnregisterAll();
        OnTxnIDFree.UnregisterAll();
        OnDBIDAllocation.UnregisterAll();
        OnDBIDFree.UnregisterAll();
        OnComplete.UnregisterAll();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextTXREQ(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::REQ<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextRXREQ(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::REQ<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextTXSNP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::SNP<config, conn>&         snpFlit,
        const Flits::REQ<config, conn>::srcid_t snpTgtId,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextRXSNP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::SNP<config, conn>&         snpFlit,
        const Flits::REQ<config, conn>::srcid_t snpTgtId,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextTXRSP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::RSP<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextRXRSP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::RSP<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextTXDAT(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::DAT<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum Joint<config, conn>::NextRXDAT(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::DAT<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        return XactDenial::DENIED_CHANNEL;
    }
}


// Implementation of: class RNFJoint
namespace /*CHI::*/Xact {
    /*
    ...
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructNone(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retired) noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructAllocatingRead(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionAllocatingRead<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructNonAllocatingRead(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionNonAllocatingRead<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructImmediateWrite(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionImmediateWrite<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructWriteZero(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionWriteZero<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructCopyBackWrite(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionCopyBackWrite<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructDataless(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionDataless<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructHomeSnoop(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionHomeSnoop<config, conn>>(glbl, topo, reqFlit);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructForwardSnoop(
        Global<config, conn>*                   glbl,       
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionForwardSnoop<config, conn>>(glbl, topo, reqFlit);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructAtomic(
        Global<config, conn>*                   glbl,       
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionAtomic<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructIndependentStash(
        Global<config, conn>*                   glbl,       
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionIndependentStash<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline RNFJoint<config, conn>::RNFJoint() noexcept
    {
        // TXREQ transactions
        #define SET_REQ_XACTION(opcode, type) \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&RNFJoint<config, conn>::Construct##type)

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
            snpDecoder[Opcodes::SNP::opcode].SetCompanion(&RNFJoint<config, conn>::Construct##type)
        
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline RNFJoint<config, conn>::RNFJoint(const RNFJoint<config, conn>& obj) noexcept
        : Joint<config, conn>               (obj)
        , txTransactions                    (obj.txTransactions)
        , rxTransactions                    (obj.rxTransactions)
        , txDBIDTransactions                (obj.txDBIDTransactions)
        , txDBIDOverlappableTransactions    (obj.txDBIDOverlappableTransactions)
        , txRetriedTransactions             (obj.txRetriedTransactions)
        , grantedPCredits                   (/*obj.grantedPCredits*/)
        , reqDecoder                        (obj.reqDecoder)
        , snpDecoder                        (obj.snpDecoder)
    {
        std::copy(obj.grantedPCredits, obj.grantedPCredits + (1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH),
            grantedPCredits);

        Fork();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline RNFJoint<config, conn>& RNFJoint<config, conn>::operator=(const RNFJoint<config, conn>& obj) noexcept
    {
        *(Joint<config, conn>*)(this) = obj;

        txTransactions                  = obj.txTransactions;
        rxTransactions                  = obj.rxTransactions;
        txDBIDTransactions              = obj.txDBIDTransactions;
        txDBIDOverlappableTransactions  = obj.txDBIDOverlappableTransactions;
        txRetriedTransactions           = obj.txRetriedTransactions;
    //  grantedPCredits                 = obj.grantedPCredits;
        reqDecoder                      = obj.reqDecoder;
        snpDecoder                      = obj.snpDecoder;

        std::copy(obj.grantedPCredits, obj.grantedPCredits + (1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH),
            grantedPCredits);

        Fork();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNFJoint<config, conn>::Fork() noexcept
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNFJoint<config, conn>::Clear() noexcept
    {
        txTransactions.clear();
        rxTransactions.clear();

        txDBIDTransactions.clear();

        txDBIDOverlappableTransactions.clear();

        txRetriedTransactions.clear();

        for (int i = 0; i < (1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH); i++)
            grantedPCredits[i].clear();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNFJoint<config, conn>::GetInflight(
        const Topology&                                         topo,
        std::vector<std::shared_ptr<Xaction<config, conn>>>&    dstVector,
        bool                                                    sortByTime
    ) const noexcept
    {
        for (auto& rxTransaction : rxTransactions)
            dstVector.push_back(rxTransaction.second);

        for (auto& txTransaction : txTransactions)
            dstVector.push_back(txTransaction.second);

        for (auto& txDBIDTransaction : txDBIDTransactions)
        {
            if (!txDBIDTransaction.second->IsTxnIDComplete(topo))
                continue;
            else
                dstVector.push_back(txDBIDTransaction.second);
        }

        for (auto& txDBIDOverlappableTransaction : txDBIDOverlappableTransactions)
        {
            if (!txDBIDOverlappableTransaction.second->IsTxnIDComplete(topo))
                continue;
            else
                dstVector.push_back(txDBIDOverlappableTransaction.second);
        }

        if (sortByTime)
        {
            std::sort(dstVector.begin(), dstVector.end(),
                [] (std::shared_ptr<Xaction<config, conn>> a, std::shared_ptr<Xaction<config, conn>> b)
                {
                    return a->GetFirst().time < b->GetFirst().time;
                }
            );
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNFJoint<config, conn>::NextTXREQ(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::REQ<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        txreqid_t key;
        key.value   = 0;
        key.id.src  = reqFlit.SrcID();
        key.id.txn  = reqFlit.TxnID();

        bool retry = false;

        //
        FiredRequestFlit<config, conn> firedReqFlit(XactScope::Requester, true, time, reqFlit);

        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, GetXaction>& opcodeInfo = 
            reqDecoder.DecodeRNF(reqFlit.Opcode());

        if (!opcodeInfo.IsValid()) // unknown opcode
            return XactDenial::DENIED_OPCODE;

        //
        if (reqFlit.AllowRetry() == 0)
        {
            if (txTransactions.contains(key))
                return XactDenial::DENIED_TXNID_IN_USE;

            // iterate and compare retry transactions
            // TODO: simplified retry mapping by TxnID, temporary workaround for KunminghuV2
            //       specification retry mapping implementation here
            rnsrc_t rnKey;
            rnKey.value     = 0;
            rnKey.id.src    = reqFlit.SrcID();

            auto xactionList = txRetriedTransactions.find(rnKey);
            if (xactionList == txRetriedTransactions.end())
                return XactDenial::DENIED_NO_MATCHING_RETRY;

            auto xactionIter = xactionList->second.begin();

            for (; xactionIter != xactionList->second.end(); xactionIter++)
                if (reqFlit.TxnID() == xactionIter->get()->GetFirst().flit.req.TxnID())
                    break;

            if (xactionIter == xactionList->second.end())
                return XactDenial::DENIED_NO_MATCHING_RETRY;

            std::shared_ptr<Xaction<config, conn>> firstXaction = *xactionIter;

            xactionList->second.erase(xactionIter);

            std::shared_ptr<Xaction<config, conn>> retryXaction =
                opcodeInfo.GetCompanion()(glbl, topo, firedReqFlit, firstXaction);

            if (!retryXaction) // unsupported opcode transaction
                return XactDenial::DENIED_OPCODE;

            if (theXaction)
                *theXaction = retryXaction;
            
            if (retryXaction->GetFirstDenial() != XactDenial::ACCEPTED)
                return retryXaction->GetFirstDenial();

            assert(firstXaction->GetRetryAck() && "transaction not getting RetryAck but lies in retried list");

            // check granted P-Credit
            pcrd_t pCrdKey;
            pCrdKey.value   = 0;
            pCrdKey.id.src  = firstXaction->GetRetryAck()->flit.rsp.SrcID();
            pCrdKey.id.tgt  = reqFlit.SrcID();

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config, conn>>>& grantedPCredits
                = this->grantedPCredits[reqFlit.PCrdType()];

            auto iterPCreditList = grantedPCredits.find(pCrdKey);
            if (iterPCreditList == grantedPCredits.end())
                return XactDenial::DENIED_NO_MATCHING_PCREDIT;

            std::list<FiredResponseFlit<config, conn>>& pCreditList = iterPCreditList->second;

            if (pCreditList.empty())
                return XactDenial::DENIED_NO_MATCHING_PCREDIT;

            XactDenialEnum denial =
                firstXaction->Resend(glbl, pCreditList.front(), retryXaction);
            
            if (denial != XactDenial::ACCEPTED)
                return denial;

            // original xaction complete
            txreqid_t firstKey;
            firstKey.value  = 0;
            firstKey.id.src = firstXaction->GetFirst().flit.req.SrcID();
            firstKey.id.txn = firstXaction->GetFirst().flit.req.TxnID();

            txTransactions.erase(firstKey);

            // event on retry xaction
            this->OnRetry(JointXactionRetryEvent<config, conn>(*this, retryXaction));

            // event on TxnID allocation
            this->OnTxnIDAllocation(JointXactionTxnIDAllocationEvent<config, conn>(*this, retryXaction));

            // consume P-Credit
            pCreditList.pop_front();

            // register retried xaction
            txTransactions[key] = retryXaction;
        }
        else
        {
            if (txTransactions.contains(key))
                return XactDenial::DENIED_TXNID_IN_USE;

            std::shared_ptr<Xaction<config, conn>> xaction;

            xaction = opcodeInfo.GetCompanion()(glbl, topo, firedReqFlit, nullptr);

            if (!xaction) // unsupported opcode transaction
                return XactDenial::DENIED_OPCODE;

            if (theXaction)
                *theXaction = xaction;

            if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
                return xaction->GetFirstDenial();

            // event on Request xaction accepted
            this->OnAccepted(JointXactionAcceptedEvent<config, conn>(*this, xaction));

            // event on TxnID allocation
            this->OnTxnIDAllocation(JointXactionTxnIDAllocationEvent<config, conn>(*this, xaction));

            txTransactions[key] = xaction;
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNFJoint<config, conn>::NextRXSNP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::SNP<config, conn>&         snpFlit,
        const Flits::REQ<config, conn>::srcid_t snpTgtId,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        rxsnpid_t key;
        key.value   = 0;
        key.id.tgt  = snpTgtId;
        key.id.src  = snpFlit.SrcID();
        key.id.txn  = snpFlit.TxnID();

        //
        if (rxTransactions.contains(key))
            return XactDenial::DENIED_TXNID_IN_USE;

        //
        FiredRequestFlit<config, conn> firedSnpFlit(XactScope::Requester, false, time, snpFlit, snpTgtId);

        const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, GetXaction>& opcodeInfo =
            snpDecoder.DecodeRNF(snpFlit.Opcode());
        
        if (!opcodeInfo.IsValid()) // unknown opcode
            return XactDenial::DENIED_OPCODE;
        
        std::shared_ptr<Xaction<config, conn>> xaction =
            opcodeInfo.GetCompanion()(glbl, topo, firedSnpFlit, nullptr);

        if (!xaction) // unsupported opcode transaction
            return XactDenial::DENIED_XACTION_NOT_SUPPORTED;
        
        if (theXaction)
            *theXaction = xaction;
        
        if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
            return xaction->GetFirstDenial();

        // event on Request xaction accepted
        this->OnAccepted(JointXactionAcceptedEvent<config, conn>(*this, xaction, snpTgtId));

        rxTransactions[key] = xaction;

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNFJoint<config, conn>::NextTXRSP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::RSP<config, conn>&         rspFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        FiredResponseFlit<config, conn> firedRspFlit(XactScope::Requester, true, time, rspFlit);

        std::shared_ptr<Xaction<config, conn>> xaction;

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
                return XactDenial::DENIED_TXNID_NOT_EXIST;

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextRSP(glbl, topo, firedRspFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return denial;
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
                return XactDenial::DENIED_TXNID_NOT_EXIST;

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextRSP(glbl, topo, firedRspFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return denial;
        }
        else
            return XactDenial::DENIED_OPCODE;

        if (xaction->GetFirst().IsREQ())
        {
            // on DBID free
            if (xaction->IsDBIDComplete(topo))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->OnDBIDFree(JointXactionDBIDFreeEvent<config, conn>(*this, xaction));
                    
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
            else if (xaction->IsDBIDOverlappable(topo))
            {
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
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
            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction));
            }
        }
        else // SNP
        {
            // on completion
            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction, rspFlit.SrcID()));

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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNFJoint<config, conn>::NextRXRSP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::RSP<config, conn>&         rspFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        FiredResponseFlit<config, conn> firedRspFlit(XactScope::Requester, false, time, rspFlit);

        // P-Credit Grant
        if (rspFlit.Opcode() == Opcodes::RSP::PCrdGrant)
        {
            pcrd_t pCrdKey;
            pCrdKey.value   = 0;
            pCrdKey.id.src  = rspFlit.SrcID();
            pCrdKey.id.tgt  = rspFlit.TgtID();

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config, conn>>>& grantedPCredits
                = this->grantedPCredits[rspFlit.PCrdType()];

            auto iterPCreditList = grantedPCredits.try_emplace(pCrdKey).first;

            iterPCreditList->second.push_back(firedRspFlit);

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(rspFlit);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        std::shared_ptr<Xaction<config, conn>> xaction;

        // RSPs to requester do not apply TxnID with DBID,
        // but may pass new DBID to requester.

        txreqid_t key;
        key.value   = 0;
        key.id.src  = rspFlit.TgtID();
        key.id.txn  = rspFlit.TxnID();

        auto xactionIter = txTransactions.find(key);
        if (xactionIter == txTransactions.end())
            return XactDenial::DENIED_TXNID_NOT_EXIST;

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextRSP(glbl, topo, firedRspFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return denial;

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
                    xaction->SetLastDenial(XactDenial::DENIED_DBID_IN_USE);
                    return XactDenial::DENIED_DBID_IN_USE;
                }

                txDBIDTransactions[keyDBID] = xaction;

                // event on DBID allocation
                this->OnDBIDAllocation(JointXactionDBIDAllocationEvent<config, conn>(*this, xaction));
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
            if (xaction->IsTxnIDComplete(topo))
            {
                // event on TxnID free
                this->OnTxnIDFree(JointXactionTxnIDFreeEvent<config, conn>(*this, xaction));

                // remove related TxnID mapping
                txreqid_t key;
                key.value   = 0;
                key.id.src  = xaction->GetFirst().flit.req.SrcID();
                key.id.txn  = xaction->GetFirst().flit.req.TxnID();

                txTransactions.erase(key);
            }

            // on DBID free
            if (xaction->IsDBIDComplete(topo))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->OnDBIDFree(JointXactionDBIDFreeEvent<config, conn>(*this, xaction));

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
            else if (xaction->IsDBIDOverlappable(topo))
            {
                // move DBID mapping to overlappable if not moved yet
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
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
            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction));

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
            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction, rspFlit.TgtID()));

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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNFJoint<config, conn>::NextTXDAT(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::DAT<config, conn>&         datFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        FiredResponseFlit<config, conn> firedDatFlit(XactScope::Requester, true, time, datFlit);

        std::shared_ptr<Xaction<config, conn>> xaction;

        // DCT or Write
        if (firedDatFlit.IsToRequester(topo))
        {
            rxsnpid_t key;
            key.value   = 0;
            key.id.tgt  = datFlit.SrcID();
            key.id.src  = datFlit.HomeNID();
            key.id.txn  = datFlit.DBID();

            auto xactionIter = rxTransactions.find(key);
            if (xactionIter == rxTransactions.end())
                return XactDenial::DENIED_DBID_NOT_EXIST;

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(glbl, topo, firedDatFlit, hasDBID, firstDBID);
        
            if (denial != XactDenial::ACCEPTED)
                return denial;
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
                return XactDenial::DENIED_TXNID_NOT_EXIST;

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(glbl, topo, firedDatFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return denial;
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
                return XactDenial::DENIED_TXNID_NOT_EXIST;

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(glbl, topo, firedDatFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return denial;
        }

        if (xaction->GetFirst().IsREQ())
        {
            // *NOTICE: TxnID not deallocating on TXDAT,
            //          since TXDAT depends on DBID response

            // on DBID free
            if (xaction->IsDBIDComplete(topo))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->OnDBIDFree(JointXactionDBIDFreeEvent<config, conn>(*this, xaction));

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
            else if (xaction->IsDBIDOverlappable(topo))
            {
                // move DBID mapping to overlappable if not moved yet
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
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

            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction));
            }
        }
        else // SNP
        {
            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction, datFlit.SrcID()));
                    
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNFJoint<config, conn>::NextRXDAT(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::DAT<config, conn>&         datFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        FiredResponseFlit<config, conn> firedDatFlit(XactScope::Requester, false, time, datFlit);

        std::shared_ptr<Xaction<config, conn>> xaction;

        // DCT or Read
        txreqid_t key;
        key.value   = 0;
        key.id.src  = datFlit.TgtID();
        key.id.txn  = datFlit.TxnID();

        auto xactionIter = txTransactions.find(key);
        if (xactionIter == txTransactions.end())
            return XactDenial::DENIED_TXNID_NOT_EXIST;

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDAT(glbl, topo, firedDatFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return denial;

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
                    xaction->SetLastDenial(XactDenial::DENIED_DBID_IN_USE);
                    return XactDenial::DENIED_DBID_IN_USE;
                }

                txDBIDTransactions[keyDBID] = xaction;

                // on DBID allocation
                this->OnDBIDAllocation(JointXactionDBIDAllocationEvent<config, conn>(*this, xaction));
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
            if (xaction->IsTxnIDComplete(topo))
            {
                // event on TxnID free
                this->OnTxnIDFree(JointXactionTxnIDFreeEvent<config, conn>(*this, xaction));

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
            if (xaction->IsDBIDComplete(topo))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
                    xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->OnDBIDFree(JointXactionDBIDFreeEvent<config, conn>(*this, xaction));

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
            else if (xaction->IsDBIDOverlappable(topo))
            {
                // move DBID mapping to overlappable if not moved yet
                const FiredResponseFlit<config, conn>* xactionDBIDSource =
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
            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction));
            }
        }
        else // SNP
        {
            // on completion
            if (xaction->IsComplete(topo))
            {
                // event on completion
                this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction, datFlit.TgtID()));
            
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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> SNFJoint<config, conn>::ConstructNone(
        Global<config, conn>*                   glbl,
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retired) noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> SNFJoint<config, conn>::ConstructHomeRead(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionHomeRead<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> SNFJoint<config, conn>::ConstructHomeWrite(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionHomeWrite<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> SNFJoint<config, conn>::ConstructHomeWriteZero(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionHomeWriteZero<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> SNFJoint<config, conn>::ConstructHomeDataless(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionHomeDataless<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> SNFJoint<config, conn>::ConstructHomeAtomic(
        Global<config, conn>*                   glbl,
        const Topology&                         topo,
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionHomeAtomic<config, conn>>(glbl, topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline SNFJoint<config, conn>::SNFJoint() noexcept
    {
        // TXREQ transactions
        #define SET_REQ_XACTION(opcode, type) \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&SNFJoint<config, conn>::Construct##type)

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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void SNFJoint<config, conn>::Clear() noexcept
    {
        rxTransactions.clear();
        
        rxDBIDTransactions.clear();

        rxDBIDOverlappableTransactions.clear();

        rxRetriedTransactions.clear();

        for (int i = 0; i < (1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH); i++)
            grantedPCredits[i].clear();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void SNFJoint<config, conn>::GetInflight(
        const Topology&                                         topo,
        std::vector<std::shared_ptr<Xaction<config, conn>>>&    dstVector,
        bool                                                    sortByTime
    ) const noexcept
    {
        for (auto& rxTransaction : rxTransactions)
            dstVector.push_back(rxTransaction.second);

        for (auto& rxDBIDTransaction : rxDBIDTransactions)
        {
            if (!rxDBIDTransaction.second->IsTxnIDComplete(topo))
                continue;
            else
                dstVector.push_back(rxDBIDTransaction.second);
        }

        for (auto& rxDBIDOverlappableTransaction : rxDBIDOverlappableTransactions)
        {
            if (!rxDBIDOverlappableTransaction.second->IsTxnIDComplete(topo))
                continue;
            else
                dstVector.push_back(rxDBIDOverlappableTransaction.second);
        }

        if (sortByTime)
        {
            std::sort(dstVector.begin(), dstVector.end(),
                [] (std::shared_ptr<Xaction<config, conn>> a, std::shared_ptr<Xaction<config, conn>> b)
                {
                    return a->GetFirst().time < b->GetFirst().time;
                }
            );
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum SNFJoint<config, conn>::NextRXREQ(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::REQ<config, conn>&         reqFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        rxreqid_t key;
        key.value   = 0;
        key.id.src  = reqFlit.SrcID();
        key.id.txn  = reqFlit.TxnID();

        bool retry = false;

        //
        FiredRequestFlit<config, conn> firedReqFlit(XactScope::Subordinate, false, time, reqFlit);

        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, GetXaction>& opcodeInfo =
            reqDecoder.DecodeSNF(reqFlit.Opcode());

        if (!opcodeInfo.IsValid()) // unknown opcode
            return XactDenial::DENIED_OPCODE;

        //
        if (reqFlit.AllowRetry() == 0)
        {
            if (rxTransactions.contains(key))
                return XactDenial::DENIED_TXNID_IN_USE;
        
            // iterate and compare retry transactions

            // TODO: retry query

            return XactDenial::DENIED_UNSUPPORTED_FEATURE;
        }
        else
        {
            if (rxTransactions.contains(key))
                return XactDenial::DENIED_TXNID_IN_USE;

            std::shared_ptr<Xaction<config, conn>> xaction;

            xaction = opcodeInfo.GetCompanion()(glbl, topo, firedReqFlit, nullptr);

            if (!xaction) // unsupported opcode transactions
                return XactDenial::DENIED_OPCODE;

            if (theXaction)
                *theXaction = xaction;

            if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
                return xaction->GetFirstDenial();

            // event on Request xaction acccepted
            this->OnAccepted(JointXactionAcceptedEvent<config, conn>(*this, xaction));

            // event on TxnID allocation
            this->OnTxnIDAllocation(JointXactionTxnIDAllocationEvent<config, conn>(*this, xaction));

            rxTransactions[key] = xaction;
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum SNFJoint<config, conn>::NextTXRSP(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::RSP<config, conn>&         rspFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        FiredResponseFlit<config, conn> firedRspFlit(XactScope::Subordinate, true, time, rspFlit);

        // P-Credit Grant
        if (rspFlit.Opcode() == Opcodes::RSP::PCrdGrant)
        {
            pcrd_t pCrdKey;
            pCrdKey.value   = 0;
            pCrdKey.id.src  = rspFlit.SrcID();
            pCrdKey.id.tgt  = rspFlit.TgtID();

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config, conn>>>& grantedPCredits
                = this->grantedPCredits[rspFlit.PCrdType()];

            auto iterPCreditList = grantedPCredits.try_emplace(pCrdKey).first;

            iterPCreditList->second.push_back(firedRspFlit);

            //
            if (glbl)
            {
                XactDenialEnum denial = glbl->CHECK_FIELD_MAPPING->Check(rspFlit);
                if (denial != XactDenial::ACCEPTED)
                    return denial;
            }

            return XactDenial::ACCEPTED;
        }

        std::shared_ptr<Xaction<config, conn>> xaction;

        // RSPs to requester do not apply TxnID with DBID,
        // but may pass new DBID to requester.

        rxreqid_t key;
        key.value   = 0;
        key.id.src  = rspFlit.TgtID();
        key.id.txn  = rspFlit.TxnID();

        auto xactionIter = rxTransactions.find(key);
        if (xactionIter == rxTransactions.end())
            return XactDenial::DENIED_TXNID_NOT_EXIST;

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextRSP(glbl, topo, firedRspFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return denial;

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
                    xaction->SetLastDenial(XactDenial::DENIED_DBID_IN_USE);
                    return XactDenial::DENIED_DBID_IN_USE;
                }

                rxDBIDTransactions[keyDBID] = xaction;

                // event on DBID allocation
                this->OnDBIDAllocation(JointXactionDBIDAllocationEvent<config, conn>(*this, xaction));
            }
            else
            {
                // nothing needed to be done here
                // the consistency of DBID should be checked inside Xaction
            }
        }

        assert(xaction->GetFirst().IsREQ() && "no SNP for SN-F");

        // on TxnID free
        if (xaction->IsTxnIDComplete(topo))
        {
            // event on TxnID free
            this->OnTxnIDFree(JointXactionTxnIDFreeEvent<config, conn>(*this, xaction));

            // remove related TxnID mapping
            rxreqid_t key;
            key.value   = 0;
            key.id.src  = xaction->GetFirst().flit.req.SrcID();
            key.id.txn  = xaction->GetFirst().flit.req.TxnID();

            rxTransactions.erase(key);
        }

        // on DBID free
        if (xaction->IsDBIDComplete(topo))
        {
            // remove related DBID mapping
            const FiredResponseFlit<config, conn>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // event on DBID free
                this->OnDBIDFree(JointXactionDBIDFreeEvent<config, conn>(*this, xaction));

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
        else if (xaction->IsDBIDOverlappable(topo))
        {
            // move DBID mapping to overlappable if not moved yet
            const FiredResponseFlit<config, conn>* xactionDBIDSource =
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
        if (xaction->IsComplete(topo))
        {
            // event on completion
            this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction));

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

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum SNFJoint<config, conn>::NextTXDAT(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::DAT<config, conn>&         datFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        FiredResponseFlit<config, conn> firedDatFlit(XactScope::Subordinate, true, time, datFlit);

        std::shared_ptr<Xaction<config, conn>> xaction;

        // DMT or Home Read
        rxreqid_t key;
        if (firedDatFlit.IsToRequester(topo))
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
            return XactDenial::DENIED_TXNID_NOT_EXIST;

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDAT(glbl, topo, firedDatFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return denial;

        assert(!hasDBID && !firstDBID && "TXDAT never generates DBID on SN-F");

        assert(xaction->GetFirst().IsREQ() && "no SNP for SN-F");

        // on TxnID free
        if (xaction->IsTxnIDComplete(topo))
        {
            // event on TxnID free
            this->OnTxnIDFree(JointXactionTxnIDFreeEvent<config, conn>(*this, xaction));

            // remove related TxnID mapping
            rxreqid_t key;
            key.value   = 0;
            key.id.src  = xaction->GetFirst().flit.req.SrcID();
            key.id.txn  = xaction->GetFirst().flit.req.TxnID();

            rxTransactions.erase(key);
        }

        // on DBID free
        if (xaction->IsDBIDComplete(topo))
        {
            // remove related DBID mapping
            const FiredResponseFlit<config, conn>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // event on DBID free
                this->OnDBIDFree(JointXactionDBIDFreeEvent<config, conn>(*this, xaction));

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
        else if (xaction->IsDBIDOverlappable(topo))
        {
            // move DBID mapping to overlappable if not moved yet
            const FiredResponseFlit<config, conn>* xactionDBIDSource =
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

        if (xaction->IsComplete(topo))
        {
            // event on completion
            this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction));
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum SNFJoint<config, conn>::NextRXDAT(
        Global<config, conn>*                   glbl,
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::DAT<config, conn>&         datFlit,
        std::shared_ptr<Xaction<config, conn>>* theXaction) noexcept
    {
        FiredResponseFlit<config, conn> firedDatFlit(XactScope::Subordinate, false, time, datFlit);

        std::shared_ptr<Xaction<config, conn>> xaction;

        // DWT or Write
        rxreqdbid_t key;
        key.value   = 0;
        key.id.tgt  = datFlit.SrcID();
        key.id.src  = datFlit.TgtID();
        key.id.db   = datFlit.TxnID();

        auto xactionIter = rxDBIDTransactions.find(key);
        if (xactionIter == rxDBIDTransactions.end())
            return XactDenial::DENIED_TXNID_NOT_EXIST;

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDAT(glbl, topo, firedDatFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return denial;

        assert(xaction->GetFirst().IsREQ() && "no SNP on SN-F");

        // *NOTICE: TxnID not deallocating on RXDAT,
        //          since RXDAT depends on DBID response
        
        // on DBID free
        if (xaction->IsDBIDComplete(topo))
        {
            // remove related DBID mapping
            const FiredResponseFlit<config, conn>* xactionDBIDSource =
                xaction->GetDBIDSource();

            if (xactionDBIDSource)
            {
                assert(xactionDBIDSource->IsRSP() && "TXDAT never generates DBID on SN-F");

                // event on DBID free
                this->OnDBIDFree(JointXactionDBIDFreeEvent<config, conn>(*this, xaction));

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
        else if (xaction->IsDBIDOverlappable(topo))
        {
            // move DBID mapping to overlappable if not moved yet
            const FiredResponseFlit<config, conn>* xactionDBIDSource =
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

        if (xaction->IsComplete(topo))
        {
            // event on completion
            this->OnComplete(JointXactionCompleteEvent<config, conn>(*this, xaction));
        }

        return XactDenial::ACCEPTED;
    }
}


#endif // __CHI__CHI_XACT_JOINT_*
