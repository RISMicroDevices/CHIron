#pragma once

#ifndef __CCHI__CCHI_XACT_JOINT
#define __CCHI__CCHI_XACT_JOINT

#include <unordered_map>
#include <functional>
#include <memory>

#include "cchi_xactions.hpp"

#include "../util/cchi_util_decoding.hpp"


namespace CCHI::Xact {

    //
    template<FlitConfigurationConcept config>
    class Joint;

    class JointTypeEnumBack {
    public:
        const char* name;
        const int   ordinal;

    public:
        inline constexpr JointTypeEnumBack(const char* name, int ordinal) noexcept
            : name(name), ordinal(ordinal) 
        { }

    public:
        inline constexpr operator int() const noexcept 
        { return ordinal; }

        inline constexpr operator const JointTypeEnumBack*() const noexcept 
        { return this; }

        inline constexpr bool operator==(const JointTypeEnumBack& other) const noexcept
        { return this->ordinal == other.ordinal; }

        inline constexpr bool operator!=(const JointTypeEnumBack& other) const noexcept
        { return this->ordinal != other.ordinal; }
    };

    using JointTypeEnum = const JointTypeEnumBack*;

    namespace JointType {
        inline constexpr JointTypeEnumBack Type1    ("Type1", 0);
        inline constexpr JointTypeEnumBack Type2    ("Type2", 1);
        inline constexpr JointTypeEnumBack Type3    ("Type3", 2);
        inline constexpr JointTypeEnumBack Type4    ("Type4", 3);
        inline constexpr JointTypeEnumBack Type5    ("Type5", 4);
    }


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
        std::string                         message;

    public:
        JointDenialEventBase(Joint<config>&                     joint,
                             std::shared_ptr<Xaction<config>>   xaction,
                             XactDenialEnum                     denial,
                             JointDenialSource                  source,
                             const std::string&                 message = "") noexcept;

    public:
        Joint<config>&                          GetJoint() noexcept;
        const Joint<config>&                    GetJoint() const noexcept;

        std::shared_ptr<Xaction<config>>        GetXaction() noexcept;
        std::shared_ptr<const Xaction<config>>  GetXaction() const noexcept;

        XactDenialEnum                          GetDenial() const noexcept;
        JointDenialSource                       GetDenialSource() const noexcept;

        std::string&                            GetMessage() noexcept;
        const std::string&                      GetMessage() const noexcept;
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
                                FiredRequestFlit<config>&           firedRequestFlit,
                                const std::string&                  message = "") noexcept;
    
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
                                 FiredResponseFlit<config>&         firedResponseFlit,
                                 const std::string&                 message = "") noexcept;

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
    };


    // Joint
    template<FlitConfigurationConcept config>
    class Joint {
    public:
        class EventHub {
        public:
            Gravity::EventBus<JointDeniedRequestEvent<config>>          OnDeniedRequest;
            Gravity::EventBus<JointDeniedResponseEvent<config>>         OnDeniedResponse;

            Gravity::EventBus<JointXactionAcceptedEvent<config>>        OnAccepted;
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
        using reqid_t = union __reqid_t {
            struct {
                Flits::txnid_t<config>      txn;
                Flits::up_nodeid_t<config>  src;
            }           id;
            uint64_t    value;
            inline operator uint64_t() const noexcept { return value; }
        };

        using evtid_t = reqid_t;

        using reqdbid_t = union __reqdbid_t {
            struct {
                Flits::dbid_t<config>       db;
                Flits::dn_nodeid_t<config>  src;
                Flits::up_nodeid_t<config>  tgt;
            }           id;
            uint64_t    value;
            inline operator uint64_t() const noexcept { return value; }
        };

        using evtdbid_t = reqdbid_t;

        using snpid_t = union __snpid_t {
            struct {
                Flits::dbid_t<config>       txn;
                Flits::dn_nodeid_t<config>  src;
                Flits::up_nodeid_t<config>  tgt;
            }           id;
            uint64_t    value;
            inline operator uint64_t() const noexcept { return value; }
        };

    private:
        using GetXaction = std::function<std::shared_ptr<Xaction<config>>(
            const Global<config>&,
            const FiredRequestFlit<config>&)>;

    private:
        std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>  upTransactions;
        std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>  dnTransactions;

        std::unordered_map<uint64_t, std::shared_ptr<Xaction<config>>>  upDBIDTransactions;

        Opcodes::EVT::Decoder<Flits::EVT<config>, GetXaction> evtDecoder;
        Opcodes::SNP::Decoder<Flits::SNP<config>, GetXaction> snpDecoder;
        Opcodes::REQ::Decoder<Flits::REQ<config>, GetXaction> reqDecoder;

    protected:
        static std::shared_ptr<Xaction<config>> ConstructNone(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructNonCacheableRead(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructCacheableTransientRead(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructCacheableAllocatingRead(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructEvict(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructWriteBack(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructCacheableDataless(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructCMO(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructStash(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructNonCacheableWrite(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructCacheableWrite(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructSnoop(const Global<config>&, const FiredRequestFlit<config>&) noexcept;
        static std::shared_ptr<Xaction<config>> ConstructEvictRemote(const Global<config>&, const FiredRequestFlit<config>&) noexcept;

    protected:
        JointTypeEnum   type;

    public:
        Joint(JointTypeEnum type = JointType::Type1) noexcept;

        JointTypeEnum   GetType() const noexcept;
        void            Clear() noexcept;

    protected:
        XactDenialEnum  RequestDeniedByJoint(XactDenialEnum                         denial,
                                             FiredRequestFlit<config>&              firedRequestFlit,
                                             std::shared_ptr<Xaction<config>>       xaction = nullptr,
                                             const std::string&                     message = "") noexcept;

        XactDenialEnum  RequestDeniedByXaction(XactDenialEnum                       denial,
                                               FiredRequestFlit<config>&            firedRequestFlit,
                                               std::shared_ptr<Xaction<config>>     xaction = nullptr,
                                               const std::string&                   message = "") noexcept;

        XactDenialEnum  ResponseDeniedByJoint(XactDenialEnum                        denial,
                                              FiredResponseFlit<config>&            firedResponseFlit,
                                              std::shared_ptr<Xaction<config>>      xaction = nullptr,
                                              const std::string&                    message = "") noexcept;

        XactDenialEnum  ResponseDeniedByXaction(XactDenialEnum                      denial,
                                                FiredResponseFlit<config>&          firedResponseFlit,
                                                std::shared_ptr<Xaction<config>>    xaction = nullptr,
                                                const std::string&                  message = "") noexcept;

        void            XactionAccepted(std::shared_ptr<Xaction<config>> xaction) noexcept;
        void            XactionTxnIDAllocated(std::shared_ptr<Xaction<config>> xaction) noexcept;
        void            XactionTxnIDFreed(std::shared_ptr<Xaction<config>> xaction) noexcept;
        void            XactionDBIDAllocated(std::shared_ptr<Xaction<config>> xaction) noexcept;
        void            XactionDBIDFreed(std::shared_ptr<Xaction<config>> xaction) noexcept;
        void            XactionCompleted(std::shared_ptr<Xaction<config>> xaction) noexcept;

    public:
        XactDenialEnum  NextREQ(
            const Global<config>&               glbl,
            uint64_t                            time,
            const Flits::REQ<config>&           reqFlit,
            std::shared_ptr<Xaction<config>>*   xaction = nullptr
        ) noexcept;

        XactDenialEnum  NextSNP(
            const Global<config>&               glbl,
            uint64_t                            time,
            const Flits::SNP<config>&           snpFlit,
            std::shared_ptr<Xaction<config>>*   xaction = nullptr
        ) noexcept;

        XactDenialEnum  NextEVT(
            const Global<config>&               glbl,
            uint64_t                            time,
            const Flits::EVT<config>&           evtFlit,
            std::shared_ptr<Xaction<config>>*   xaction = nullptr
        ) noexcept;

        XactDenialEnum  NextDnRSP(
            const Global<config>&               glbl,
            uint64_t                            time,
            const Flits::DnRSP<config>&         dnrspFlit,
            std::shared_ptr<Xaction<config>>*   xaction = nullptr
        ) noexcept;

        XactDenialEnum  NextUpRSP(
            const Global<config>&               glbl,
            uint64_t                            time,
            const Flits::UpRSP<config>&         uprspFlit,
            std::shared_ptr<Xaction<config>>*   xaction = nullptr
        ) noexcept;

        XactDenialEnum  NextDnDAT(
            const Global<config>&               glbl,
            uint64_t                            time,
            const Flits::DnDAT<config>&         dndatFlit,
            std::shared_ptr<Xaction<config>>*   xaction = nullptr
        ) noexcept;

        XactDenialEnum  NextUpDAT(
            const Global<config>&               glbl,
            uint64_t                            time,
            const Flits::UpDAT<config>&         updatFlit,
            std::shared_ptr<Xaction<config>>*   xaction = nullptr
        ) noexcept;
    };
}


// Implementation of: class JointDenialEventBase
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    JointDenialEventBase<config>::JointDenialEventBase(Joint<config>&                     joint,
                                                       std::shared_ptr<Xaction<config>>   xaction,
                                                       XactDenialEnum                     denial,
                                                       JointDenialSource                  source,
                                                       const std::string&                 message) noexcept
        : joint         (joint)
        , xaction       (xaction)
        , denial        (denial)
        , denialSource  (source)
        , message       (message)
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

    template<FlitConfigurationConcept config>
    inline std::string& JointDenialEventBase<config>::GetMessage() noexcept
    {
        return message;
    }

    template<FlitConfigurationConcept config>
    inline const std::string& JointDenialEventBase<config>::GetMessage() const noexcept
    {
        return message;
    }
}

// Implementation of: class JointDeniedRequestEvent
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointDeniedRequestEvent<config>::JointDeniedRequestEvent(Joint<config>&                     joint,
                                                                    std::shared_ptr<Xaction<config>>   xaction,
                                                                    XactDenialEnum                     denial,
                                                                    JointDenialSource                  source,
                                                                    FiredRequestFlit<config>&          firedRequestFlit,
                                                                    const std::string&                 message) noexcept
        : JointDenialEventBase<config>  (joint, xaction, denial, source, message)
        , firedRequestFlit              (firedRequestFlit)
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
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointDeniedResponseEvent<config>::JointDeniedResponseEvent(Joint<config>&                    joint,
                                                                     std::shared_ptr<Xaction<config>>   xaction,
                                                                     XactDenialEnum                     denial,
                                                                     JointDenialSource                  source,
                                                                     FiredResponseFlit<config>&         firedResponseFlit,
                                                                     const std::string&                 message) noexcept
        : JointDenialEventBase<config>  (joint, xaction, denial, source, message)
        , firedResponseFlit             (firedResponseFlit)
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
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionEventBase<config>::JointXactionEventBase(Joint<config>&                    joint, 
                                                                std::shared_ptr<Xaction<config>>  xaction) noexcept
        : joint         (joint)
        , xaction       (xaction)
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
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionAcceptedEvent<config>::JointXactionAcceptedEvent(Joint<config>&                    joint, 
                                                                        std::shared_ptr<Xaction<config>>  xaction) noexcept
        : JointXactionEventBase<config>  (joint, xaction)
    { }
}

// Implementation of: class JointXactionTxnIDAllocationEvent
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionTxnIDAllocationEvent<config>::JointXactionTxnIDAllocationEvent(Joint<config>&                    joint, 
                                                                                      std::shared_ptr<Xaction<config>>  xaction) noexcept
        : JointXactionEventBase<config>  (joint, xaction)
    { }
}

// Implementation of: class JointXactionTxnIDFreeEvent
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionTxnIDFreeEvent<config>::JointXactionTxnIDFreeEvent(Joint<config>&                    joint, 
                                                                          std::shared_ptr<Xaction<config>>  xaction) noexcept
        : JointXactionEventBase<config>  (joint, xaction)
    { }
}

// Implementation of: class JointXactionDBIDAllocationEvent
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionDBIDAllocationEvent<config>::JointXactionDBIDAllocationEvent(Joint<config>&                    joint, 
                                                                                    std::shared_ptr<Xaction<config>>  xaction) noexcept
        : JointXactionEventBase<config>  (joint, xaction)
    { }
}

// Implementation of: class JointXactionDBIDFreeEvent
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionDBIDFreeEvent<config>::JointXactionDBIDFreeEvent(Joint<config>&                    joint, 
                                                                        std::shared_ptr<Xaction<config>>  xaction) noexcept
        : JointXactionEventBase<config>  (joint, xaction)
    { }
}

// Implementation of: class JointXactionCompleteEvent
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline JointXactionCompleteEvent<config>::JointXactionCompleteEvent(Joint<config>&                    joint, 
                                                                        std::shared_ptr<Xaction<config>>  xaction) noexcept
        : JointXactionEventBase<config>  (joint, xaction)
    { }
}


// Implementation of: class Joint::EventHub
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline Joint<config>::EventHub::EventHub() noexcept
        : OnDeniedRequest   (0)
        , OnDeniedResponse  (0)
        , OnAccepted        (0)
        , OnTxnIDAllocation (0)
        , OnTxnIDFree       (0)
        , OnDBIDAllocation  (0)
        , OnDBIDFree        (0)
        , OnComplete        (0)
    { }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::EventHub::Clear() noexcept
    {
        OnDeniedRequest.UnregisterAll();
        OnDeniedResponse.UnregisterAll();
        OnAccepted.UnregisterAll();
        OnTxnIDAllocation.UnregisterAll();
        OnTxnIDFree.UnregisterAll();
        OnDBIDAllocation.UnregisterAll();
        OnDBIDFree.UnregisterAll();
        OnComplete.UnregisterAll();
    }
}

// Implementation of: class Joint
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline Joint<config>::Joint(JointTypeEnum type) noexcept
        : events               (std::make_shared<EventHub>())
        , upTransactions       ()
        , dnTransactions       ()
        , upDBIDTransactions   ()
        , evtDecoder           ()
        , snpDecoder           ()
        , reqDecoder           ()
        , type                 (type)
    {
        // REQ transactions
        #define SET_REQ_XACTION(opcode, type) \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&Joint<config>::Construct##type)

        SET_REQ_XACTION(StashShared     , Stash                     );  // 0x00
        SET_REQ_XACTION(StashUnique     , Stash                     );  // 0x01
        SET_REQ_XACTION(ReadNoSnp       , NonCacheableRead          );  // 0x02
        SET_REQ_XACTION(ReadOnce        , CacheableTransientRead    );  // 0x03
        SET_REQ_XACTION(ReadShared      , CacheableAllocatingRead   );  // 0x04
                                                                        // 0x05
                                                                        // 0x06
                                                                        // 0x07    
        SET_REQ_XACTION(WriteNoSnpPtl   , NonCacheableWrite         );  // 0x08
        SET_REQ_XACTION(WriteNoSnpFull  , NonCacheableWrite         );  // 0x09
        SET_REQ_XACTION(WriteUniquePtl  , CacheableWrite            );  // 0x0A
        SET_REQ_XACTION(WriteUniqueFull , CacheableWrite            );  // 0x0B
        SET_REQ_XACTION(CleanShared     , CMO                       );  // 0x0C
        SET_REQ_XACTION(CleanInvalid    , CMO                       );  // 0x0D
        SET_REQ_XACTION(MakeInvalid     , CMO                       );  // 0x0E
                                                                        // 0x0F
        SET_REQ_XACTION(ReadUnique      , CacheableAllocatingRead   );  // 0x10
                                                                        // 0x11
        SET_REQ_XACTION(MakeUnique      , CacheableDataless         );  // 0x12
                                                                        // 0x13
                                                                        // 0x14
                                                                        // 0x15
                                                                        // 0x16
                                                                        // 0x17
                                                                        // 0x18
                                                                        // 0x19
                                                                        // 0x1A
                                                                        // 0x1B
                                                                        // 0x1C
                                                                        // 0x1D
        SET_REQ_XACTION(EvictBack       , EvictRemote               );  // 0x1E
                                                                        // 0x1F
            
        #undef SET_REQ_XACTION

        // EVT transactions
        #define SET_EVT_XACTION(opcode, type) \
            evtDecoder[Opcodes::EVT::opcode].SetCompanion(&Joint<config>::Construct##type)

        SET_EVT_XACTION(Evict           , Evict                     );  // 0x00
        SET_EVT_XACTION(WriteBackFull   , WriteBack                 );  // 0x01

        #undef SET_EVT_XACTION

        // SNP transactions
        #define SET_SNP_XACTION(opcode, type) \
            snpDecoder[Opcodes::SNP::opcode].SetCompanion(&Joint<config>::Construct##type)

        SET_SNP_XACTION(SnpMakeInvalid  , Snoop                     );  // 0x00
        SET_SNP_XACTION(SnpToInvalid    , Snoop                     );  // 0x01
        SET_SNP_XACTION(SnpToShared     , Snoop                     );  // 0x02
        SET_SNP_XACTION(SnpToClean      , Snoop                     );  // 0x03

        #undef SET_SNP_XACTION
    }

    template<FlitConfigurationConcept config>
    inline JointTypeEnum Joint<config>::GetType() const noexcept
    {
        return type;
    }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::Clear() noexcept
    {
        upTransactions.clear();
        dnTransactions.clear();
        upDBIDTransactions.clear();
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructNone(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructNonCacheableRead(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionNonCacheableRead<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructCacheableTransientRead(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionCacheableTransientRead<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructCacheableAllocatingRead(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionCacheableAllocatingRead<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructEvict(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionEvict<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructWriteBack(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionWriteBack<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructCacheableDataless(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionCacheableDataless<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructCMO(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionCMO<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructStash(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionStash<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructNonCacheableWrite(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionNonCacheableWrite<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructCacheableWrite(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionCacheableWrite<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructSnoop(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionSnoop<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline std::shared_ptr<Xaction<config>> Joint<config>::ConstructEvictRemote(const Global<config>& glbl, const FiredRequestFlit<config>& reqFlit) noexcept
    {
        return std::make_shared<XactionEvictRemote<config>>(glbl, reqFlit);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::RequestDeniedByJoint(XactDenialEnum                         denial,
                                                              FiredRequestFlit<config>&              firedRequestFlit,
                                                              std::shared_ptr<Xaction<config>>       xaction,
                                                              const std::string&                     message) noexcept
    {
        if (this->events)
            this->events->OnDeniedRequest(JointDeniedRequestEvent<config>(
                *this, xaction, denial, JointDenialSource::JOINT, firedRequestFlit, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::RequestDeniedByXaction(XactDenialEnum                       denial,
                                                                FiredRequestFlit<config>&            firedRequestFlit,
                                                                std::shared_ptr<Xaction<config>>     xaction,
                                                                const std::string&                   message) noexcept
    {
        if (this->events)
            this->events->OnDeniedRequest(JointDeniedRequestEvent<config>(
                *this, xaction, denial, JointDenialSource::XACTION, firedRequestFlit, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::ResponseDeniedByJoint(XactDenialEnum                        denial,
                                                               FiredResponseFlit<config>&            firedResponseFlit,
                                                               std::shared_ptr<Xaction<config>>      xaction,
                                                               const std::string&                    message) noexcept
    {
        if (this->events)
            this->events->OnDeniedResponse(JointDeniedResponseEvent<config>(
                *this, xaction, denial, JointDenialSource::JOINT, firedResponseFlit, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::ResponseDeniedByXaction(XactDenialEnum                      denial,
                                                                 FiredResponseFlit<config>&          firedResponseFlit,
                                                                 std::shared_ptr<Xaction<config>>    xaction,
                                                                 const std::string&                  message) noexcept
    {
        if (this->events)
            this->events->OnDeniedResponse(JointDeniedResponseEvent<config>(
                *this, xaction, denial, JointDenialSource::XACTION, firedResponseFlit, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::XactionAccepted(std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        if (this->events)
            this->events->OnAccepted(JointXactionAcceptedEvent<config>(*this, xaction));
    }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::XactionTxnIDAllocated(std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        if (this->events)
            this->events->OnTxnIDAllocation(JointXactionTxnIDAllocationEvent<config>(*this, xaction));
    }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::XactionTxnIDFreed(std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        if (this->events)
            this->events->OnTxnIDFree(JointXactionTxnIDFreeEvent<config>(*this, xaction));
    }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::XactionDBIDAllocated(std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        if (this->events)
            this->events->OnDBIDAllocation(JointXactionDBIDAllocationEvent<config>(*this, xaction));
    }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::XactionDBIDFreed(std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        if (this->events)
            this->events->OnDBIDFree(JointXactionDBIDFreeEvent<config>(*this, xaction));
    }

    template<FlitConfigurationConcept config>
    inline void Joint<config>::XactionCompleted(std::shared_ptr<Xaction<config>> xaction) noexcept
    {
        if (this->events)
            this->events->OnComplete(JointXactionCompleteEvent<config>(*this, xaction));
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextREQ(const Global<config>&               glbl,
                                                 uint64_t                            time,
                                                 const Flits::REQ<config>&           reqFlit,
                                                 std::shared_ptr<Xaction<config>>*   theXaction) noexcept
    {
        reqid_t key;
        key.value   = 0;
        key.id.src  = reqFlit.SrcID;
        key.id.txn  = reqFlit.TxnID;

        //
        FiredRequestFlit<config> firedReqFlit(XactScope::Upstream, true, time, reqFlit);

        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, GetXaction>& opcodeInfo 
            = reqDecoder.Decode(reqFlit.Opcode);

        if (!opcodeInfo.IsValid())
            return RequestDeniedByJoint(XactDenial::DENIED_REQ_OPCODE_NOT_DECODED, firedReqFlit, 
                nullptr, "This opcode could not be decoded by Opcodes::REQ::Decoder");

        //
        if (upTransactions.contains(key))
            return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_TXNID_IN_USE, firedReqFlit, upTransactions[key]);

        std::shared_ptr<Xaction<config>> xaction
            = opcodeInfo.GetCompanion()(glbl, firedReqFlit);

        if (!xaction) // unsupported opcode transaction
            return this->RequestDeniedByJoint(XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED, firedReqFlit,
                nullptr, "No Xaction type mapped for this opcode");

        if (theXaction)
            *theXaction = xaction;

        if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
            return this->RequestDeniedByXaction(xaction->GetFirstDenial(), firedReqFlit, xaction);

        // event on REQ xaction accepted
        this->XactionAccepted(xaction);

        // event on TxnID allocated
        this->XactionTxnIDAllocated(xaction);

        // TODO: immediate deallocation

        upTransactions[key] = xaction;

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextSNP(const Global<config>&               glbl,
                                                 uint64_t                            time,
                                                 const Flits::SNP<config>&           snpFlit,
                                                 std::shared_ptr<Xaction<config>>*   theXaction) noexcept
    {
        snpid_t key;
        key.value   = 0;
        key.id.txn  = snpFlit.TxnID;
        key.id.src  = snpFlit.SrcID;
        key.id.tgt  = snpFlit.TgtID;

        //
        FiredRequestFlit<config> firedSnpFlit(XactScope::Upstream, false, time, snpFlit);

        const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, GetXaction>& opcodeInfo 
            = snpDecoder.Decode(snpFlit.Opcode);

        if (!opcodeInfo.IsValid()) // unknown opcode
            return RequestDeniedByJoint(XactDenial::DENIED_SNP_OPCODE_NOT_DECODED, firedSnpFlit, 
                nullptr, "This opcode could not be decoded by Opcodes::SNP::Decoder");

        //
        if (dnTransactions.contains(key))
            return this->RequestDeniedByJoint(XactDenial::DENIED_SNP_TXNID_IN_USE, firedSnpFlit, dnTransactions[key]);

        std::shared_ptr<Xaction<config>> xaction
            = opcodeInfo.GetCompanion()(glbl, firedSnpFlit);

        if (!xaction) // unsupported opcode transaction
            return this->RequestDeniedByJoint(XactDenial::DENIED_SNP_OPCODE_NOT_SUPPORTED, firedSnpFlit,
                nullptr, "No Xaction type mapped for this opcode");

        if (theXaction)
            *theXaction = xaction;

        if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
            return this->RequestDeniedByJoint(xaction->GetFirstDenial(), firedSnpFlit, xaction);

        // event on SNP xaction accepted
        this->XactionAccepted(xaction);

        dnTransactions[key] = xaction;

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextEVT(const Global<config>&               glbl,
                                                 uint64_t                            time,
                                                 const Flits::EVT<config>&           evtFlit,
                                                 std::shared_ptr<Xaction<config>>*   theXaction) noexcept
    {
        evtid_t key;
        key.value   = 0;
        key.id.src  = evtFlit.SrcID;
        key.id.txn  = evtFlit.TxnID;

        //
        FiredRequestFlit<config> firedEvtFlit(XactScope::Upstream, true, time, evtFlit);

        const Opcodes::OpcodeInfo<typename Flits::EVT<config>::opcode_t, GetXaction>& opcodeInfo 
            = evtDecoder.Decode(evtFlit.Opcode);

        if (!opcodeInfo.IsValid()) // unknown opcode
            return RequestDeniedByJoint(XactDenial::DENIED_EVT_OPCODE_NOT_DECODED, firedEvtFlit, 
                nullptr, "This opcode could not be decoded by Opcodes::EVT::Decoder");

        //
        if (upTransactions.contains(key))
            return this->RequestDeniedByJoint(XactDenial::DENIED_EVT_TXNID_IN_USE, firedEvtFlit, upTransactions[key]);

        std::shared_ptr<Xaction<config>> xaction
            = opcodeInfo.GetCompanion()(glbl, firedEvtFlit);

        if (!xaction) // unsupported opcode transaction
            return this->RequestDeniedByJoint(XactDenial::DENIED_EVT_OPCODE_NOT_SUPPORTED, firedEvtFlit,
                nullptr, "No Xaction type mapped for this opcode");

        if (theXaction)
            *theXaction = xaction;

        if (xaction->GetFirstDenial() != XactDenial::ACCEPTED)
            return this->RequestDeniedByXaction(xaction->GetFirstDenial(), firedEvtFlit, xaction);

        // event on EVT xaction accepted
        this->XactionAccepted(xaction);

        // event on TxnID allocated
        this->XactionTxnIDAllocated(xaction);

        upTransactions[key] = xaction;

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextDnRSP(const Global<config>&                glbl,
                                                    uint64_t                            time,
                                                    const Flits::DnRSP<config>&         dnrspFlit,
                                                    std::shared_ptr<Xaction<config>>*   theXaction) noexcept
    {
        FiredResponseFlit firedDnrspFlit(XactScope::Upstream, false, time, dnrspFlit);

        std::shared_ptr<Xaction<config>> xaction;

        reqid_t key;
        key.value   = 0;
        key.id.src  = dnrspFlit.TgtID;
        key.id.txn  = dnrspFlit.TxnID;

        auto xactionIter = upTransactions.find(key);
        if (xactionIter == upTransactions.end())
            return this->ResponseDeniedByJoint(XactDenial::DENIED_DNRSP_TXNID_NOT_EXIST, firedDnrspFlit);

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDnRSP(glbl, firedDnrspFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByXaction(denial, firedDnrspFlit, xaction);

        // *NOTICE: For further flits carrying DBID, 
        //          the consistency of DBID should be checked inside Xaction
        if (hasDBID && firstDBID)
        {
            reqdbid_t keyDBID;
            keyDBID.value   = 0;
            keyDBID.id.tgt  = dnrspFlit.TgtID;
            keyDBID.id.src  = dnrspFlit.SrcID;
            keyDBID.id.db   = dnrspFlit.DBID;

            if (upDBIDTransactions.contains(keyDBID))
            {
                xaction->SetLastDenial(XactDenial::DENIED_DNRSP_DBID_IN_USE);
                return this->ResponseDeniedByJoint(XactDenial::DENIED_DNRSP_DBID_IN_USE, firedDnrspFlit);
            }

            upDBIDTransactions[keyDBID] = xaction;

            // event on DBID allocated
            this->XactionDBIDAllocated(xaction);
        }

        if (xaction->GetFirst().IsREQ() || xaction->GetFirst().IsEVT())
        {
            // on TxnID free
            if (xaction->IsTxnIDComplete(glbl))
            {
                // event on TxnID free
                this->XactionTxnIDFreed(xaction);

                // remove related TxnID mapping
                reqid_t key;
                key.value   = 0;
                if (xaction->GetFirst().IsREQ())
                {
                    key.id.src  = xaction->GetFirst().flit.req.SrcID;
                    key.id.txn  = xaction->GetFirst().flit.req.TxnID;
                }
                else // EVT
                {
                    key.id.src  = xaction->GetFirst().flit.evt.SrcID;
                    key.id.txn  = xaction->GetFirst().flit.evt.TxnID;
                }

                upTransactions.erase(key);
            }

            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                const FiredResponseFlit<config>* xactionDBIDSource
                    = xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->XactionDBIDFreed(xaction);

                    reqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    if (xactionDBIDSource->IsDnRSP())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dnrsp.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dnrsp.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dnrsp.TgtID;
                    }
                    else if (xactionDBIDSource->IsDnDAT())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dndat.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dndat.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dndat.TgtID;
                    }
                    else
                        return XactDenial::ACCEPTED; // TODO: consider this as an internal error

                    // check TxnID for DBID complete on downstream RSPs to prevent duplicated removing
                    auto xactionDBIDIter = upDBIDTransactions.find(keyDBID);
                    if (xactionDBIDIter != upDBIDTransactions.end())
                    {
                        auto xactionDBID = xactionDBIDIter->second;
                        
                        Flits::txnid_t<config> thisTxnID, thatTxnID;
                        thisTxnID = xaction->GetFirst().IsREQ() ? xaction->GetFirst().flit.req.TxnID : xaction->GetFirst().flit.evt.TxnID;
                        thatTxnID = xactionDBID->GetFirst().IsREQ() ? xactionDBID->GetFirst().flit.req.TxnID : xactionDBID->GetFirst().flit.evt.TxnID;
                        
                        if (thisTxnID == thatTxnID)
                            upDBIDTransactions.erase(xactionDBIDIter);
                    }

                    upDBIDTransactions.erase(keyDBID);
                }
            }

            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);
            }
        }
        else if (xaction->GetFirst().IsSNP())
        {
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);

                snpid_t key;
                key.value  = 0;
                key.id.tgt = xaction->GetFirst().flit.snp.TgtID;
                key.id.src = xaction->GetFirst().flit.snp.SrcID;
                key.id.txn = xaction->GetFirst().flit.snp.TxnID;

                dnTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextUpRSP(const Global<config>&                glbl,
                                                    uint64_t                            time,
                                                    const Flits::UpRSP<config>&         uprspFlit,
                                                    std::shared_ptr<Xaction<config>>*   theXaction) noexcept
    {
        FiredResponseFlit<config> firedUprspFlit(XactScope::Upstream, true, time, uprspFlit);

        std::shared_ptr<Xaction<config>> xaction;

        // RSPs from requester might apply TxnID with DBID (CompAck).
        // Otherwise, snoop RSPs use TxnID from SNPs (SnpResp).
        if (uprspFlit.Opcode == Opcodes::UpRSP::CompAck)
        {
            reqdbid_t key;
            key.value   = 0;
            key.id.tgt  = uprspFlit.SrcID;
            key.id.src  = uprspFlit.TgtID;
            key.id.db   = uprspFlit.TxnID;

            auto xactionIter = upDBIDTransactions.find(key);
            if (xactionIter == upDBIDTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_UPRSP_TXNID_NOT_EXIST, firedUprspFlit);
        
            xaction = xactionIter->second;
        }
        else if (uprspFlit.Opcode == Opcodes::UpRSP::SnpResp)
        {
            snpid_t key;
            key.value   = 0;
            key.id.tgt  = uprspFlit.SrcID;
            key.id.src  = uprspFlit.TgtID;
            key.id.txn  = uprspFlit.TxnID;

            auto xactionIter = dnTransactions.find(key);
            if (xactionIter == dnTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_UPRSP_TXNID_NOT_EXIST, firedUprspFlit);

            xaction = xactionIter->second;
        }
        else
            return this->ResponseDeniedByJoint(XactDenial::DENIED_UPRSP_OPCODE, firedUprspFlit);

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextUpRSP(glbl, firedUprspFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByXaction(denial, firedUprspFlit, xaction);

        if (xaction->GetFirst().IsREQ() || xaction->GetFirst().IsEVT())
        {
            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config>* xactionDBIDSource
                    = xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->XactionDBIDFreed(xaction);

                    reqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    if (xactionDBIDSource->IsDnRSP())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dnrsp.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dnrsp.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dnrsp.TgtID;
                    }
                    else if (xactionDBIDSource->IsDnDAT())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dndat.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dndat.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dndat.TgtID;
                    }
                    else
                        return XactDenial::ACCEPTED; // TODO: consider this as an internal error

                    upDBIDTransactions.erase(keyDBID);
                }
            }

            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);
            }
        }
        else if (xaction->GetFirst().IsSNP())
        {
            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);

                snpid_t key;
                key.value   = 0;
                key.id.tgt  = xaction->GetFirst().flit.snp.TgtID;
                key.id.src  = xaction->GetFirst().flit.snp.SrcID;
                key.id.txn  = xaction->GetFirst().flit.snp.TxnID;

                dnTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextDnDAT(const Global<config>&                glbl,
                                                    uint64_t                            time,
                                                    const Flits::DnDAT<config>&         dndatFlit,
                                                    std::shared_ptr<Xaction<config>>*   theXaction) noexcept
    {
        FiredResponseFlit<config> firedDndatFlit(XactScope::Upstream, false, time, dndatFlit);

        std::shared_ptr<Xaction<config>> xaction;

        reqid_t key;
        key.value   = 0;
        key.id.src  = dndatFlit.TgtID;
        key.id.txn  = dndatFlit.TxnID;

        auto xactionIter = upTransactions.find(key);
        if (xactionIter == upTransactions.end())
            return this->ResponseDeniedByJoint(XactDenial::DENIED_DNDAT_TXNID_NOT_EXIST, firedDndatFlit);

        xaction = xactionIter->second;

        if (theXaction)
            *theXaction = xaction;

        bool hasDBID, firstDBID;

        XactDenialEnum denial = xaction->NextDnDAT(glbl, firedDndatFlit, hasDBID, firstDBID);

        if (denial != XactDenial::ACCEPTED)
            return this->ResponseDeniedByXaction(denial, firedDndatFlit, xaction);

        // *NOTICE: For further flits carrying DBID, 
        //          the consistency of DBID should be checked inside Xaction
        if (hasDBID && firstDBID)
        {
            reqdbid_t keyDBID;
            keyDBID.value   = 0;
            keyDBID.id.tgt  = dndatFlit.TgtID;
            keyDBID.id.src  = dndatFlit.SrcID;
            keyDBID.id.db   = dndatFlit.DBID;

            if (upDBIDTransactions.contains(keyDBID))
            {
                xaction->SetLastDenial(XactDenial::DENIED_DNDAT_DBID_IN_USE);
                return this->ResponseDeniedByJoint(XactDenial::DENIED_DNDAT_DBID_IN_USE, firedDndatFlit);
            }

            upDBIDTransactions[keyDBID] = xaction;

            // event on DBID allocated
            this->XactionDBIDAllocated(xaction);
        }

        if (xaction->GetFirst().IsREQ() || xaction->GetFirst().IsEVT())
        {
            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config>* xactionDBIDSource
                    = xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->XactionDBIDFreed(xaction);

                    reqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    if (xactionDBIDSource->IsDnRSP())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dnrsp.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dnrsp.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dnrsp.TgtID;
                    }
                    else if (xactionDBIDSource->IsDnDAT())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dndat.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dndat.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dndat.TgtID;
                    }
                    else
                        return XactDenial::ACCEPTED; // TODO: consider this as an internal error

                    // check TxnID for DBID complete on downstream DATs to prevent duplicated removing
                    auto xactionDBIDIter = upDBIDTransactions.find(keyDBID);
                    if (xactionDBIDIter != upDBIDTransactions.end())
                    {
                        auto xactionDBID = xactionDBIDIter->second;
                        
                        Flits::txnid_t<config> thisTxnID, thatTxnID;
                        thisTxnID = xaction->GetFirst().IsREQ() ? xaction->GetFirst().flit.req.TxnID : xaction->GetFirst().flit.evt.TxnID;
                        thatTxnID = xactionDBID->GetFirst().IsREQ() ? xactionDBID->GetFirst().flit.req.TxnID : xactionDBID->GetFirst().flit.evt.TxnID;
                        
                        if (thisTxnID == thatTxnID)
                            upDBIDTransactions.erase(xactionDBIDIter);
                    }
                }
            }

            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);
            }
        }
        else if (xaction->GetFirst().IsSNP())
        {
            // on completion
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);

                snpid_t key;
                key.value   = 0;
                key.id.tgt  = xaction->GetFirst().flit.snp.TgtID;
                key.id.src  = xaction->GetFirst().flit.snp.SrcID;
                key.id.txn  = xaction->GetFirst().flit.snp.TxnID;

                dnTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Joint<config>::NextUpDAT(const Global<config>&                glbl,
                                                    uint64_t                            time,
                                                    const Flits::UpDAT<config>&         updatFlit,
                                                    std::shared_ptr<Xaction<config>>*   theXaction) noexcept
    {
        FiredResponseFlit<config> firedUpdatFlit(XactScope::Upstream, true, time, updatFlit);

        std::shared_ptr<Xaction<config>> xaction;

        if (updatFlit.Opcode == Opcodes::UpDAT::SnpRespData)
        {
            snpid_t key;
            key.value   = 0;
            key.id.tgt  = updatFlit.SrcID;
            key.id.src  = updatFlit.TgtID;
            key.id.txn  = updatFlit.TxnID;

            auto xactionIter = dnTransactions.find(key);
            if (xactionIter == dnTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_UPDAT_TXNID_NOT_EXIST, firedUpdatFlit);

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextUpDAT(glbl, firedUpdatFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return this->ResponseDeniedByXaction(denial, firedUpdatFlit, xaction);
        }
        else
        {
            reqdbid_t key;
            key.value   = 0;
            key.id.tgt  = updatFlit.SrcID;
            key.id.src  = updatFlit.TgtID;
            key.id.db   = updatFlit.TxnID;

            auto xactionIter = upDBIDTransactions.find(key);
            if (xactionIter == upDBIDTransactions.end())
                return this->ResponseDeniedByJoint(XactDenial::DENIED_UPDAT_TXNID_NOT_EXIST, firedUpdatFlit);

            xaction = xactionIter->second;

            if (theXaction)
                *theXaction = xaction;

            bool hasDBID, firstDBID;

            XactDenialEnum denial = xaction->NextUpDAT(glbl, firedUpdatFlit, hasDBID, firstDBID);

            if (denial != XactDenial::ACCEPTED)
                return this->ResponseDeniedByXaction(denial, firedUpdatFlit, xaction);
        }

        if (xaction->GetFirst().IsREQ() || xaction->GetFirst().IsEVT())
        {
            // on DBID free
            if (xaction->IsDBIDComplete(glbl))
            {
                // remove related DBID mapping
                const FiredResponseFlit<config>* xactionDBIDSource
                    = xaction->GetDBIDSource();

                if (xactionDBIDSource)
                {
                    // event on DBID free
                    this->XactionDBIDFreed(xaction);

                    reqdbid_t keyDBID;
                    keyDBID.value   = 0;
                    if (xactionDBIDSource->IsDnRSP())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dnrsp.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dnrsp.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dnrsp.TgtID;
                    }
                    else if (xactionDBIDSource->IsDnDAT())
                    {
                        keyDBID.id.db   = xactionDBIDSource->flit.dndat.DBID;
                        keyDBID.id.src  = xactionDBIDSource->flit.dndat.SrcID;
                        keyDBID.id.tgt  = xactionDBIDSource->flit.dndat.TgtID;
                    }
                    else
                        return XactDenial::ACCEPTED; // TODO: consider this as an internal error

                    // check TxnID for DBID complete on upstream DATs to prevent duplicated removing
                    auto xactionDBIDIter = upDBIDTransactions.find(keyDBID);
                    if (xactionDBIDIter != upDBIDTransactions.end())
                    {
                        auto xactionDBID = xactionDBIDIter->second;
                        
                        Flits::txnid_t<config> thisTxnID, thatTxnID;
                        thisTxnID = xaction->GetFirst().IsREQ() ? xaction->GetFirst().flit.req.TxnID : xaction->GetFirst().flit.evt.TxnID;
                        thatTxnID = xactionDBID->GetFirst().IsREQ() ? xactionDBID->GetFirst().flit.req.TxnID : xactionDBID->GetFirst().flit.evt.TxnID;
                        
                        if (thisTxnID == thatTxnID)
                            upDBIDTransactions.erase(xactionDBIDIter);
                    }
                }
            }

            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);
            }
        }
        else // SNP
        {
            if (xaction->IsComplete(glbl))
            {
                // event on completion
                this->XactionCompleted(xaction);

                snpid_t key;
                key.value   = 0;
                key.id.tgt  = xaction->GetFirst().flit.snp.TgtID;
                key.id.src  = xaction->GetFirst().flit.snp.SrcID;
                key.id.txn  = xaction->GetFirst().flit.snp.TxnID;

                dnTransactions.erase(key);
            }
        }

        return XactDenial::ACCEPTED;
    }
}


#endif // __CCHI__CCHI_XACT_JOINT
