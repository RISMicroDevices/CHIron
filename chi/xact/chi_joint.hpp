//#pragma once

//#ifndef __CHI__CHI_XACT_JOINT
//#define __CHI__CHI_XACT_JOINT

#include <unordered_map>
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

        public:
            virtual XactDenialEnum  NextTXREQ(uint64_t time, const Topology& topo, const Flits::REQ<config, conn>& reqFlit) noexcept = 0;
            virtual XactDenialEnum  NextRXSNP(uint64_t time, const Topology& topo, const Flits::SNP<config, conn>& snpFlit,
                                                                                   const Flits::REQ<config, conn>::srcid_t snpTgtId) noexcept = 0;
            virtual XactDenialEnum  NextTXRSP(uint64_t time, const Topology& topo, const Flits::RSP<config, conn>& rspFlit) noexcept = 0;
            virtual XactDenialEnum  NextRXRSP(uint64_t time, const Topology& topo, const Flits::RSP<config, conn>& rspFlit) noexcept = 0;
            virtual XactDenialEnum  NextTXDAT(uint64_t time, const Topology& topo, const Flits::DAT<config, conn>& datFlit) noexcept = 0;
            virtual XactDenialEnum  NextRXDAT(uint64_t time, const Topology& topo, const Flits::DAT<config, conn>& rspFlit) noexcept = 0;
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
                    Flits::RSP<config, conn>::srcid_t   src;
                }           id;
                uint64_t    value;
                inline operator uint64_t() const noexcept { return value; }
            };

        private:
            using GetXaction = std::function<std::shared_ptr<Xaction<config, conn>>(
                const Topology&,
                const FiredRequestFlit<config, conn>&,
                std::shared_ptr<Xaction<config, conn>>)>;

        private:
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        txTransactions;
            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        rxTransactions;

            std::unordered_map<uint64_t, std::shared_ptr<Xaction<config, conn>>>        txDBIDTransactions;

            std::unordered_map<uint64_t, std::list<std::shared_ptr<Xaction<config, conn>>>>
                                                                                        txRetriedTransactions;

            std::unordered_map<uint64_t, std::list<FiredResponseFlit<config, conn>>>    grantedPCredits[1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH];

            Opcodes::REQ::Decoder<Flits::REQ<config>, GetXaction>                       reqDecoder;
            Opcodes::SNP::Decoder<Flits::SNP<config>, GetXaction>                       snpDecoder;

        protected:
            static std::shared_ptr<Xaction<config, conn>>   ConstructNone(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructAllocatingRead(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructNonAllocatingRead(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructImmediateWrite(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructCopyBackWrite(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructDataless(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructHomeSnoop(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;
            static std::shared_ptr<Xaction<config, conn>>   ConstructForwardSnoop(const Topology&, const FiredRequestFlit<config, conn>&, std::shared_ptr<Xaction<config, conn>>) noexcept;

        public:
            RNFJoint() noexcept;

        public:
            virtual void            Clear() noexcept override;

        public:
            virtual XactDenialEnum  NextTXREQ(uint64_t time, const Topology& topo, const Flits::REQ<config, conn>& reqFlit) noexcept override;
            virtual XactDenialEnum  NextRXSNP(uint64_t time, const Topology& topo, const Flits::SNP<config, conn>& snpFlit,
                                                                                   const Flits::REQ<config, conn>::srcid_t snpTgtId) noexcept override;
            virtual XactDenialEnum  NextTXRSP(uint64_t time, const Topology& topo, const Flits::RSP<config, conn>& rspFlit) noexcept override;
            virtual XactDenialEnum  NextRXRSP(uint64_t time, const Topology& topo, const Flits::RSP<config, conn>& rspFlit) noexcept override;
            virtual XactDenialEnum  NextTXDAT(uint64_t time, const Topology& topo, const Flits::DAT<config, conn>& datFlit) noexcept override;
            virtual XactDenialEnum  NextRXDAT(uint64_t time, const Topology& topo, const Flits::DAT<config, conn>& datFlit) noexcept override;
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
}


// Implementation of: class RNFJoint
namespace /*CHI::*/Xact {
    /*
    ...
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructNone(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retired) noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructAllocatingRead(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionAllocatingRead<config, conn>>(topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructNonAllocatingRead(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionNonAllocatingRead<config, conn>>(topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructImmediateWrite(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionImmediateWrite<config, conn>>(topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructCopyBackWrite(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionCopyBackWrite<config, conn>>(topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructDataless(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionDataless<config, conn>>(topo, reqFlit, retried);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructHomeSnoop(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionHomeSnoop<config, conn>>(topo, reqFlit);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::shared_ptr<Xaction<config, conn>> RNFJoint<config, conn>::ConstructForwardSnoop(
        const Topology&                         topo, 
        const FiredRequestFlit<config, conn>&   reqFlit,
        std::shared_ptr<Xaction<config, conn>>  retried) noexcept
    {
        return std::make_shared<XactionForwardSnoop<config, conn>>(topo, reqFlit);
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
        SET_REQ_XACTION(StashOnceShared             , None              );  // 0x22
        SET_REQ_XACTION(StashOnceUnique             , None              );  // 0x23
        SET_REQ_XACTION(ReadOnceCleanInvalid        , NonAllocatingRead );  // 0x24
        SET_REQ_XACTION(ReadOnceMakeInvalid         , NonAllocatingRead );  // 0x25
        SET_REQ_XACTION(ReadNotSharedDirty          , AllocatingRead    );  // 0x26
        SET_REQ_XACTION(CleanSharedPersist          , Dataless          );  // 0x27
        SET_REQ_XACTION(AtomicStore::ADD            , None              );  // 0x28
        SET_REQ_XACTION(AtomicStore::CLR            , None              );  // 0x29
        SET_REQ_XACTION(AtomicStore::EOR            , None              );  // 0x2A
        SET_REQ_XACTION(AtomicStore::SET            , None              );  // 0x2B
        SET_REQ_XACTION(AtomicStore::SMAX           , None              );  // 0x2C
        SET_REQ_XACTION(AtomicStore::SMIN           , None              );  // 0x2D
        SET_REQ_XACTION(AtomicStore::UMAX           , None              );  // 0x2E
        SET_REQ_XACTION(AtomicStore::UMIN           , None              );  // 0x2F
        SET_REQ_XACTION(AtomicLoad::ADD             , None              );  // 0x30
        SET_REQ_XACTION(AtomicLoad::CLR             , None              );  // 0x31
        SET_REQ_XACTION(AtomicLoad::EOR             , None              );  // 0x32
        SET_REQ_XACTION(AtomicLoad::SET             , None              );  // 0x33
        SET_REQ_XACTION(AtomicLoad::SMAX            , None              );  // 0x34
        SET_REQ_XACTION(AtomicLoad::SMIN            , None              );  // 0x35
        SET_REQ_XACTION(AtomicLoad::UMAX            , None              );  // 0x36
        SET_REQ_XACTION(AtomicLoad::UMIN            , None              );  // 0x37
        SET_REQ_XACTION(AtomicSwap                  , None              );  // 0x38
        SET_REQ_XACTION(AtomicCompare               , None              );  // 0x39
        SET_REQ_XACTION(PrefetchTgt                 , None              );  // 0x3A
                                                                            // 0x3B
                                                                            // 0x3C
                                                                            // 0x3D
                                                                            // 0x3E
                                                                            // 0x3F
                                                                            // 0x40
        SET_REQ_XACTION(MakeReadUnique              , AllocatingRead    );  // 0x41
        SET_REQ_XACTION(WriteEvictOrEvict           , CopyBackWrite     );  // 0x42
        SET_REQ_XACTION(WriteUniqueZero             , None              );  // 0x43
        SET_REQ_XACTION(WriteNoSnpZero              , None              );  // 0x44
                                                                            // 0x45
                                                                            // 0x46
        SET_REQ_XACTION(StashOnceSepShared          , None              );  // 0x47
        SET_REQ_XACTION(StashOnceSepUnique          , None              );  // 0x48
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
    inline void RNFJoint<config, conn>::Clear() noexcept
    {
        txTransactions.clear();
        rxTransactions.clear();

        txDBIDTransactions.clear();

        txRetriedTransactions.clear();

        for (int i = 0; i < (1 << Flits::RSP<config, conn>::PCRDTYPE_WIDTH); i++)
            grantedPCredits[i].clear();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNFJoint<config, conn>::NextTXREQ(
        uint64_t                        time,
        const Topology&                 topo,
        const Flits::REQ<config, conn>& reqFlit) noexcept
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
                opcodeInfo.GetCompanion()(topo, firedReqFlit, firstXaction);

            if (!retryXaction) // unsupported opcode transaction
                return XactDenial::DENIED_OPCODE;
            
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
                firstXaction->Resend(pCreditList.front(), retryXaction);
            
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

            xaction = opcodeInfo.GetCompanion()(topo, firedReqFlit, nullptr);

            if (!xaction) // unsupported opcode transaction
                return XactDenial::DENIED_OPCODE;

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
        uint64_t                                time,
        const Topology&                         topo,
        const Flits::SNP<config, conn>&         snpFlit,
        const Flits::REQ<config, conn>::srcid_t snpTgtId) noexcept
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
        FiredRequestFlit<config, conn> firedSnpFlit(XactScope::Requester, false, time, snpFlit);

        const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, GetXaction>& opcodeInfo =
            snpDecoder.DecodeRNF(snpFlit.Opcode());
        
        if (!opcodeInfo.IsValid()) // unknown opcode
            return XactDenial::DENIED_OPCODE;
        
        std::shared_ptr<Xaction<config, conn>> xaction =
            opcodeInfo.GetCompanion()(topo, firedSnpFlit, nullptr);

        if (!xaction) // unsupported opcode transaction
            return XactDenial::DENIED_OPCODE;
        
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
        uint64_t                        time,
        const Topology&                 topo,
        const Flits::RSP<config, conn>& rspFlit) noexcept
    {
        FiredResponseFlit<config, conn> firedRspFlit(XactScope::Requester, true, time, rspFlit);

        std::shared_ptr<Xaction<config, conn>> xaction;

        // RSPs from requester might apply TxnID with DBID (CompAck).
        // Otherwise, snoop RSPs use TxnID from SNPs (SnpResp, SnpRespFwded).
        if (firedRspFlit.flit.rsp.Opcode() == Opcodes::RSP::CompAck)
        {
            txreqdbid_t key;
            key.value   = 0;
            key.id.tgt  = rspFlit.SrcID();
            key.id.src  = rspFlit.TgtID();
            key.id.db   = rspFlit.TxnID();

            auto xactionIter = txDBIDTransactions.find(key);
            if (xactionIter == txDBIDTransactions.end())
                return XactDenial::DENIED_TXNID_NOT_EXIST;

            xaction = xactionIter->second;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextRSP(topo, firedRspFlit, hasDBID, firstDBID);

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

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextRSP(topo, firedRspFlit, hasDBID, firstDBID);

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
        uint64_t                        time,
        const Topology&                 topo,
        const Flits::RSP<config, conn>& rspFlit) noexcept
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

            // TODO: pass through Fields Checker here

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

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextRSP(topo, firedRspFlit, hasDBID, firstDBID);

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
        uint64_t                        time,
        const Topology&                 topo,
        const Flits::DAT<config, conn>& datFlit) noexcept
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

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(topo, firedDatFlit, hasDBID, firstDBID);
        
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

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(topo, firedDatFlit, hasDBID, firstDBID);

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

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextDAT(topo, firedDatFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return denial;
        }

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
        uint64_t                        time,
        const Topology&                 topo,
        const Flits::DAT<config, conn>& datFlit) noexcept
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

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDAT(topo, firedDatFlit, hasDBID, firstDBID);

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


#endif // __CHI__CHI_XACT_JOINT_*
