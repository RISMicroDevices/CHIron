#pragma once

#ifndef __CCHI__CCHI_XACT_XACTIONS__BASE
#define __CCHI__CCHI_XACT_XACTIONS__BASE

#include <initializer_list>
#include <vector>
#include <memory>
#include <bitset>
#include <string>

#include "../../../common/eventbus.hpp"

#include "../../spec/cchi_protocol_flits.hpp"
#include "../../spec/cchi_protocol_encoding.hpp"

#include "../cchi_xact_base/cchi_xact_base_denial.hpp"

#include "../cchi_xact_global.hpp"
#include "../cchi_xact_flit.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class Xaction;

    template<ChannelTypeEnum>
    struct ChannelTypeTag {};

    enum class XactionType {
        NonCacheableRead = 0,
        CacheableTransientRead,
        CacheableAllocatingRead,
        Evict,
        WriteBack,
        CacheableDataless,
        CMO,
        Stash,
        NonCacheableWrite,
        CacheableWrite,
        Snoop,
        EvictRemote
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

        const FiredRequestFlit<config>*  complementRequest;

    public:
        XactionDeniedRequestFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredRequestFlit<config>& flit, const std::string& message = "") noexcept;
        XactionDeniedRequestFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredRequestFlit<config>& flit, const FiredRequestFlit<config>& complementRequest, const std::string& message = "") noexcept;

    public:
        const FiredRequestFlit<config>& GetFlit() const noexcept;

        bool                            HasComplementRequest() const noexcept;
        const FiredRequestFlit<config>* GetComplementRequest() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class XactionDeniedResponseFlitEvent : public XactionDeniedEventBase<config>, 
                                           public Gravity::Event<XactionDeniedResponseFlitEvent<config>> {
    protected:
        const FiredResponseFlit<config>& flit;

        const FiredRequestFlit<config>*  complementRequest;
        const FiredResponseFlit<config>* complementResponse;

    public:
        XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const std::string& message = "") noexcept;
        XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const FiredRequestFlit<config>& complementRequest, const std::string& message = "") noexcept;
        XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const FiredResponseFlit<config>& complementResponse, const std::string& message = "") noexcept;

    public:
        const FiredResponseFlit<config>& GetFlit() const noexcept;

        bool                             HasComplementRequest() const noexcept;
        const FiredRequestFlit<config>*  GetComplementRequest() const noexcept;
        bool                             HasComplementResponse() const noexcept;
        const FiredResponseFlit<config>* GetComplementResponse() const noexcept;
    };


    // Xaction Base
    template<FlitConfigurationConcept config>
    class Xaction {
    public:
        class EventHub {
        public:
            Gravity::EventBus<XactionDeniedRequestFlitEvent<config>>   OnDeniedRequestFlit;
            Gravity::EventBus<XactionDeniedResponseFlitEvent<config>>  OnDeniedResponseFlit;

        public:
            EventHub() noexcept;
            void Clear() noexcept;
        };

        std::shared_ptr<EventHub> events;

    public:
        struct SubsequenceKey {
        public:
            XactDenialEnum  denial;
            struct {
                bool        isDnRSP : 1;
                bool        isUpRSP : 1;
                bool        isDnDAT : 1;
                bool        isUpDAT : 1;
            };
            union {
                Flits::DnRSP<config>::opcode_t dnrsp;
                Flits::UpRSP<config>::opcode_t uprsp;
                Flits::DnDAT<config>::opcode_t dndat;
                Flits::UpDAT<config>::opcode_t updat;
            }               opcode;
            bool            hasDBID;

        public:
            SubsequenceKey() noexcept;

            template<ChannelTypeEnum, class T>
            SubsequenceKey(XactDenialEnum denial, T opcode, bool hasDBID) noexcept;

            template<ChannelTypeEnum C, class T>
            SubsequenceKey(ChannelTypeTag<C>, XactDenialEnum denial, T opcode, bool hasDBID) noexcept;

            bool IsDnRSP() const noexcept;
            bool IsUpRSP() const noexcept;
            bool IsDnDAT() const noexcept;
            bool IsUpDAT() const noexcept;
            ChannelTypeEnum GetChannelType() const noexcept;

            bool IsAccepted() const noexcept;
            bool IsDenied() const noexcept;

            bool HasDBID() const noexcept;
        };

    private:
        const XactionType                       type;

    protected:
        FiredRequestFlit<config>                first;
        XactDenialEnum                          firstDenial;
        std::vector<FiredResponseFlit<config>>  subsequence;
        std::vector<SubsequenceKey>             subsequenceKeys;

    public:
        Companion                               companion;
        
    public:
        Xaction(XactionType type, const FiredRequestFlit<config>& first) noexcept;

        XactionType                     GetType() const noexcept;

    public:
        virtual std::shared_ptr<Xaction<config>>    Clone() const noexcept = 0;
        template<class T> std::shared_ptr<T>        Clone() const noexcept;

    public:
        const FiredRequestFlit<config>&                 GetFirst() const noexcept;
        XactDenialEnum                                  GetFirstDenial() const noexcept;
        const std::vector<FiredResponseFlit<config>>&   GetSubsequence() const noexcept;
        XactDenialEnum                                  GetSubsequentDenial(size_t index) const noexcept;
        XactDenialEnum                                  GetLastDenial() const noexcept;
        void                                            SetLastDenial(XactDenialEnum) noexcept;

    public:
        bool                                            HasDnRSP() const noexcept;
        bool                                            HasUpRSP() const noexcept;
        bool                                            HasDnDAT() const noexcept;
        bool                                            HasUpDAT() const noexcept;

        bool                                            HasDnRSP(std::initializer_list<typename Flits::DnRSP<config>::opcode_t>) const noexcept;
        bool                                            HasUpRSP(std::initializer_list<typename Flits::UpRSP<config>::opcode_t>) const noexcept;
        bool                                            HasDnDAT(std::initializer_list<typename Flits::DnDAT<config>::opcode_t>) const noexcept;
        bool                                            HasUpDAT(std::initializer_list<typename Flits::UpDAT<config>::opcode_t>) const noexcept;

        const FiredResponseFlit<config>*                GetFirstDnRSP() const noexcept;
        const FiredResponseFlit<config>*                GetFirstUpRSP() const noexcept;
        const FiredResponseFlit<config>*                GetFirstDnDAT() const noexcept;
        const FiredResponseFlit<config>*                GetFirstUpDAT() const noexcept;
        const FiredResponseFlit<config>*                GetLastDnRSP() const noexcept;
        const FiredResponseFlit<config>*                GetLastUpRSP() const noexcept;
        const FiredResponseFlit<config>*                GetLastDnDAT() const noexcept;
        const FiredResponseFlit<config>*                GetLastUpDAT() const noexcept;

        const FiredResponseFlit<config>*                GetFirst(ChannelTypeEnum channel) const noexcept;
        const FiredResponseFlit<config>*                GetLast(ChannelTypeEnum channel) const noexcept;

        const FiredResponseFlit<config>*                GetFirstDnRSP(std::initializer_list<typename Flits::DnRSP<config>::opcode_t> opcodes) const noexcept;
        const FiredResponseFlit<config>*                GetFirstUpRSP(std::initializer_list<typename Flits::UpRSP<config>::opcode_t> opcodes) const noexcept;
        const FiredResponseFlit<config>*                GetFirstDnDAT(std::initializer_list<typename Flits::DnDAT<config>::opcode_t> opcodes) const noexcept;
        const FiredResponseFlit<config>*                GetFirstUpDAT(std::initializer_list<typename Flits::UpDAT<config>::opcode_t> opcodes) const noexcept;

        const FiredResponseFlit<config>*                GetLastDnRSP(std::initializer_list<typename Flits::DnRSP<config>::opcode_t> opcodes) const noexcept;
        const FiredResponseFlit<config>*                GetLastUpRSP(std::initializer_list<typename Flits::UpRSP<config>::opcode_t> opcodes) const noexcept;
        const FiredResponseFlit<config>*                GetLastDnDAT(std::initializer_list<typename Flits::DnDAT<config>::opcode_t> opcodes) const noexcept;
        const FiredResponseFlit<config>*                GetLastUpDAT(std::initializer_list<typename Flits::UpDAT<config>::opcode_t> opcodes) const noexcept;

        template<ChannelTypeEnum, class T>
        const FiredResponseFlit<config>*                GetFirst(std::initializer_list<T> opcodes) const noexcept;
        template<ChannelTypeEnum, class T>
        const FiredResponseFlit<config>*                GetLast(std::initializer_list<T> opcodes) const noexcept;

    public:
        bool                                            GotDBID() const noexcept;
        std::optional<typename Flits::DnRSP<config>::dbid_t>
                                                        GetDBID() const noexcept;
        const FiredResponseFlit<config>*                GetDBIDSource() const noexcept;
        const FiredResponseFlit<config>*                GetLastDBIDSourceRSP(std::initializer_list<typename Flits::DnRSP<config>::opcode_t> opcodes) const noexcept;
        const FiredResponseFlit<config>*                GetLastDBIDSourceDAT(std::initializer_list<typename Flits::DnDAT<config>::opcode_t> opcodes) const noexcept;

    public:
        virtual XactDenialEnum                          Next(const Global<config>& glbl, const FiredResponseFlit<config>& flit, bool& hasDBID, bool& firstDBID) noexcept;
        virtual XactDenialEnum                          NextDnRSP(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept;
        virtual XactDenialEnum                          NextUpRSP(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept;
        virtual XactDenialEnum                          NextDnDAT(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept;
        virtual XactDenialEnum                          NextUpDAT(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept;

        virtual bool                                    IsTxnIDComplete(const Global<config>& glbl) const noexcept = 0;
        virtual bool                                    IsDBIDComplete(const Global<config>& glbl) const noexcept = 0;
        virtual bool                                    IsComplete(const Global<config>& glbl) const noexcept = 0;

    protected:
        virtual XactDenialEnum                          NextDnRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;
        virtual XactDenialEnum                          NextUpRSPNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;
        virtual XactDenialEnum                          NextDnDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;
        virtual XactDenialEnum                          NextUpDATNoRecord(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept = 0;

    protected:
        virtual bool                                    NextDataID(typename Flits::REQ<config>::ssize_t                             reqSize, 
                                                                   const FiredResponseFlit<config>&                                 datFlit, 
                                                                   std::initializer_list<typename Flits::DnDAT<config>::opcode_t>   dndatOpcodes = {},
                                                                   std::initializer_list<typename Flits::UpDAT<config>::opcode_t>   updatOpcodes = {}) noexcept;

        virtual bool                                    NextEVTDataID(const FiredResponseFlit<config>&                                  datFlit, 
                                                                      std::initializer_list<typename Flits::UpDAT<config>::opcode_t>    updatOpcodes = {}) noexcept;
        virtual bool                                    NextSNPDataID(const FiredResponseFlit<config>&                                  datFlit,
                                                                      std::initializer_list<typename Flits::UpDAT<config>::opcode_t>    updatOpcodes = {}) noexcept;
        virtual bool                                    NextREQDataID(const FiredResponseFlit<config>&                                  datFlit, 
                                                                      std::initializer_list<typename Flits::DnDAT<config>::opcode_t>    dndatOpcodes = {},
                                                                      std::initializer_list<typename Flits::UpDAT<config>::opcode_t>    updatOpcodes = {}) noexcept;
        
    protected:
        XactDenialEnum  RequestFlitDenied(XactDenialEnum                    denial, 
                                          const FiredRequestFlit<config>&   flit,
                                          const std::string&                message = "") noexcept;

        XactDenialEnum  RequestFlitDenied(XactDenialEnum                    denial, 
                                          const FiredRequestFlit<config>&   flit,
                                          const FiredRequestFlit<config>&   complementRequest,
                                          const std::string&                message = "") noexcept;

        XactDenialEnum  ResponseFlitDenied(XactDenialEnum                     denial, 
                                           const FiredResponseFlit<config>&   flit,
                                           const std::string&                 message = "") noexcept;

        XactDenialEnum  ResponseFlitDenied(XactDenialEnum                     denial,
                                           const FiredResponseFlit<config>&   flit,
                                           const FiredResponseFlit<config>&   complementResponse,
                                           const std::string&                 message = "") noexcept;

        XactDenialEnum  ResponseFlitDenied(XactDenialEnum                     denial, 
                                           const FiredResponseFlit<config>&   flit,
                                           const FiredRequestFlit<config>&    complementRequest,
                                           const std::string&                 message = "") noexcept;
    };

    namespace details {

        template<FlitConfigurationConcept config>
        inline static std::bitset<8> CollectDataID(
            typename Flits::REQ<config>::ssize_t        reqSize,
            typename Flits::DnDAT<config>::dataid_t     dataID) noexcept;

        template<FlitConfigurationConcept config, typename T>
        requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
        inline static std::bitset<8> CollectDnDataID(
            typename Flits::REQ<config>::ssize_t                            reqSize,
            const T&                                                        flits,
            std::function<bool(size_t, const FiredResponseFlit<config>&)>   validFunc = [](auto, auto) -> bool { return true; }) noexcept;

        template<FlitConfigurationConcept config, typename T>
        requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
        inline static std::bitset<8> CollectDnDataID(
            typename Flits::REQ<config>::ssize_t                            reqSize,
            const T&                                                        flits,
            std::initializer_list<typename Flits::DnDAT<config>::opcode_t>  datOpcodes) noexcept;

        template<FlitConfigurationConcept config, typename T>
        requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
        inline static std::bitset<8> CollectUpDataID(
            typename Flits::REQ<config>::ssize_t                            reqSize,
            const T&                                                        flits,
            std::function<bool(size_t, const FiredResponseFlit<config>&)>   validFunc = [](auto, auto) -> bool { return true; }) noexcept;

        template<FlitConfigurationConcept config, typename T>
        requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
        inline static std::bitset<8> CollectUpDataID(
            typename Flits::REQ<config>::ssize_t                            reqSize,
            const T&                                                        flits,
            std::initializer_list<typename Flits::UpDAT<config>::opcode_t>  datOpcodes) noexcept;

        template<FlitConfigurationConcept config>
        inline static std::bitset<8> GetDataIDCompleteMask(
            typename Flits::REQ<config>::ssize_t    reqSize) noexcept;
    }
}


// Implementation of details
namespace CCHI::Xact::details {

    template<FlitConfigurationConcept config>
    inline static std::bitset<8> CollectDataID(
        typename Flits::REQ<config>::ssize_t    reqSize,
        typename Flits::DnDAT<config>::dataid_t dataID) noexcept
    {
        std::bitset<8> collectedDataID;

        if constexpr (config::dataWidth == 512)
            collectedDataID.set(0);
        else 
            collectedDataID.set(dataID);

        return collectedDataID;
    }

    template<FlitConfigurationConcept config, typename T>
    requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
    inline static std::bitset<8> CollectDnDataID(
        typename Flits::REQ<config>::ssize_t                            reqSize,
        const T&                                                        flits,
        std::function<bool(size_t, const FiredResponseFlit<config>&)>   validFunc) noexcept
    {
        std::bitset<8> collectedDataID;

        size_t index = 0;
        for (const FiredResponseFlit<config>& flit : flits)
        {
            if (flit.IsDnDAT() && validFunc(index, flit))
                collectedDataID |= CollectDataID<config>(reqSize, flit.flit.dndat.DataID);

            index++;
        }

        return collectedDataID;
    }

    template<FlitConfigurationConcept config, typename T>
    requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
    inline static std::bitset<8> CollectDnDataID(
        typename Flits::REQ<config>::ssize_t                            reqSize,
        const T&                                                        flits,
        std::initializer_list<typename Flits::DnDAT<config>::opcode_t>  datOpcodes) noexcept
    {
        return CollectDataID<config>(reqSize, flits, [&datOpcodes](auto, const FiredResponseFlit<config>& flit) -> bool {
            return datOpcodes.size() > 0 ? datOpcodes.contains(flit.flit.dndat.Opcode) : true;
        });
    }

    template<FlitConfigurationConcept config, typename T>
    requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
    inline static std::bitset<8> CollectUpDataID(
        typename Flits::REQ<config>::ssize_t                            reqSize,
        const T&                                                        flits,
        std::function<bool(size_t, const FiredResponseFlit<config>&)>   validFunc) noexcept
    {
        std::bitset<8> collectedDataID;

        size_t index = 0;
        for (const FiredResponseFlit<config>& flit : flits)
        {
            if (flit.IsUpDAT() && validFunc(index, flit))
                collectedDataID |= CollectDataID<config>(reqSize, flit.flit.updat.DataID);

            index++;
        }

        return collectedDataID;
    }

    template<FlitConfigurationConcept config, typename T>
    requires std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, FiredResponseFlit<config>>
    inline static std::bitset<8> CollectUpDataID(
        typename Flits::REQ<config>::ssize_t                            reqSize,
        const T&                                                        flits,
        std::initializer_list<typename Flits::UpDAT<config>::opcode_t>  datOpcodes) noexcept
    {
        return CollectUpDataID<config>(reqSize, flits, [&datOpcodes](auto, const FiredResponseFlit<config>& flit) -> bool {
            return datOpcodes.size() > 0 ? datOpcodes.contains(flit.flit.updat.Opcode) : true;
        });
    }

    template<FlitConfigurationConcept config>
    inline static std::bitset<8> GetDataIDCompleteMask(
        typename Flits::REQ<config>::ssize_t    reqSize) noexcept
    {
        std::bitset<8> completeMask;
        unsigned int requestSize = (1 << reqSize) << 3;

        if constexpr (config::dataWidth == 512)
        {
            completeMask.set(0);
        }

        if constexpr (config::dataWidth == 256)
        {
            if (requestSize <= 256)
            {
                completeMask.set(0);
            }
            else // 512
            {
                completeMask.set(0);
                completeMask.set(1);
            }
        }

        if constexpr (config::dataWidth == 128)
        {
            if (requestSize <= 128) 
            {
                completeMask.set(0);
            }
            else if (requestSize <= 256) 
            {
                completeMask.set(0);
                completeMask.set(1);
            }
            else // 512
            {
                completeMask.set(0);
                completeMask.set(1);
                completeMask.set(2);
                completeMask.set(3);
            }
        }

        if constexpr (config::dataWidth == 64)
        {
            if (requestSize <= 64) 
            {
                completeMask.set(0);
            }
            else if (requestSize <= 128) 
            {
                completeMask.set(0);
                completeMask.set(1);
            }
            else if (requestSize <= 256) 
            {
                completeMask.set(0);
                completeMask.set(1);
                completeMask.set(2);
                completeMask.set(3);
            }
            else // 512
            {
                completeMask.set(0);
                completeMask.set(1);
                completeMask.set(2);
                completeMask.set(3);
                completeMask.set(4);
                completeMask.set(5);
                completeMask.set(6);
                completeMask.set(7);
            }
        }

        return completeMask;
    }
}


// Implementation of: class Xaction::SubsequenceKey
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline Xaction<config>::SubsequenceKey::SubsequenceKey() noexcept
    {
        this->denial        = XactDenial::NOT_INITIALIZED;
        this->isDnRSP       = false;
        this->isUpRSP       = false;
        this->isDnDAT       = false;
        this->isUpDAT       = false;
        this->hasDBID       = false;
    }

    template<FlitConfigurationConcept config>
    template<ChannelTypeEnum C, class T>
    inline Xaction<config>::SubsequenceKey::SubsequenceKey(XactDenialEnum denial, T opcode, bool hasDBID) noexcept
        : SubsequenceKey(ChannelTypeTag<C> {}, denial, opcode, hasDBID)
    { }

    template<FlitConfigurationConcept config>
    template<ChannelTypeEnum C, class T>
    inline Xaction<config>::SubsequenceKey::SubsequenceKey(ChannelTypeTag<C>, XactDenialEnum denial, T opcode, bool hasDBID) noexcept
    {
        this->denial        = denial;
        this->isDnRSP       = (C == ChannelType::DnRSP);
        this->isUpRSP       = (C == ChannelType::UpRSP);
        this->isDnDAT       = (C == ChannelType::DnDAT);
        this->isUpDAT       = (C == ChannelType::UpDAT);
        if constexpr (C == ChannelType::DnRSP) this->opcode.dnrsp  = opcode;
        if constexpr (C == ChannelType::UpRSP) this->opcode.uprsp  = opcode;
        if constexpr (C == ChannelType::DnDAT) this->opcode.dndat  = opcode;
        if constexpr (C == ChannelType::UpDAT) this->opcode.updat  = opcode;
        this->hasDBID       = hasDBID;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsDnRSP() const noexcept
    {
        return isDnRSP;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsUpRSP() const noexcept
    {
        return isUpRSP;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsDnDAT() const noexcept
    {
        return isDnDAT;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::SubsequenceKey::IsUpDAT() const noexcept
    {
        return isUpDAT;
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum Xaction<config>::SubsequenceKey::GetChannelType() const noexcept
    {
        if (isDnRSP) return ChannelType::DnRSP;
        if (isUpRSP) return ChannelType::UpRSP;
        if (isDnDAT) return ChannelType::DnDAT;
        return ChannelType::UpDAT;
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
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline Xaction<config>::Xaction(XactionType                        type, 
                             const FiredRequestFlit<config>&    first) noexcept
        : type              (type)
        , first             (first)
        , firstDenial       (XactDenial::NOT_INITIALIZED)
        , subsequence       ()
        , subsequenceKeys   ()
    { }

    template<FlitConfigurationConcept config>
    inline XactionType Xaction<config>::GetType() const noexcept
    {
        return type;
    }

    template<FlitConfigurationConcept config>
    inline Xaction<config>::EventHub::EventHub() noexcept
        : OnDeniedRequestFlit     (0)
        , OnDeniedResponseFlit    (0)
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
        return std::make_shared<T>(static_cast<const T&>(*this));
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
        if (index >= subsequenceKeys.size())
            return XactDenial::NOT_INITIALIZED;

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
    inline bool Xaction<config>::HasDnRSP() const noexcept
    {
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (iter->IsDnRSP())
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasUpRSP() const noexcept
    {
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (iter->IsUpRSP())
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasDnDAT() const noexcept
    {
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (iter->IsDnDAT())
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasUpDAT() const noexcept
    {
        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (iter->IsUpDAT())
                return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasDnRSP(std::initializer_list<typename Flits::DnRSP<config>::opcode_t> opcodes) const noexcept
    {
        if (opcodes.size() == 0)
            return false;

        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (!iter->IsDnRSP())
                continue;

            for (auto opcode : opcodes)
                if (iter->opcode.dnrsp == opcode)
                    return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasUpRSP(std::initializer_list<typename Flits::UpRSP<config>::opcode_t> opcodes) const noexcept
    {
        if (opcodes.size() == 0)
            return false;

        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (!iter->IsUpRSP())
                continue;

            for (auto opcode : opcodes)
                if (iter->opcode.uprsp == opcode)
                    return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasDnDAT(std::initializer_list<typename Flits::DnDAT<config>::opcode_t> opcodes) const noexcept
    {
        if (opcodes.size() == 0)
            return false;

        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (!iter->IsDnDAT())
                continue;

            for (auto opcode : opcodes)
                if (iter->opcode.dndat == opcode)
                    return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::HasUpDAT(std::initializer_list<typename Flits::UpDAT<config>::opcode_t> opcodes) const noexcept
    {
        if (opcodes.size() == 0)
            return false;

        for (auto iter = subsequenceKeys.begin(); iter != subsequenceKeys.end(); iter++)
        {
            if (!iter->IsDenied())
                continue;

            if (!iter->IsUpDAT())
                continue;

            for (auto opcode : opcodes)
                if (iter->opcode.updat == opcode)
                    return true;
        }

        return false;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDnRSP() const noexcept
    {
        return GetFirst(ChannelType::DnRSP);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstUpRSP() const noexcept
    {
        return GetFirst(ChannelType::UpRSP);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDnDAT() const noexcept
    {
        return GetFirst(ChannelType::DnDAT);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstUpDAT() const noexcept
    {
        return GetFirst(ChannelType::UpDAT);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDnRSP() const noexcept
    {
        return GetLast(ChannelType::DnRSP);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastUpRSP() const noexcept
    {
        return GetLast(ChannelType::UpRSP);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDnDAT() const noexcept
    {
        return GetLast(ChannelType::DnDAT);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastUpDAT() const noexcept
    {
        return GetLast(ChannelType::UpDAT);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirst(ChannelTypeEnum channel) const noexcept
    {
        for (auto keyIt = subsequenceKeys.begin(), flitIt = subsequence.begin();
             keyIt != subsequenceKeys.end(); ++keyIt, ++flitIt)
        {
            if (keyIt->IsDenied())
                continue;

            if (keyIt->GetChannelType() == channel)
                return &*flitIt;
        }

        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLast(ChannelTypeEnum channel) const noexcept
    {
        for (auto keyIt = subsequenceKeys.rbegin(), flitIt = subsequence.rbegin();
             keyIt != subsequenceKeys.rend(); ++keyIt, ++flitIt)
        {
            if (keyIt->IsDenied())
                continue;

            if (keyIt->GetChannelType() == channel)
                return &*flitIt;
        }

        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDnRSP(std::initializer_list<typename Flits::DnRSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirst<ChannelType::DnRSP>(opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstUpRSP(std::initializer_list<typename Flits::UpRSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirst<ChannelType::UpRSP>(opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstDnDAT(std::initializer_list<typename Flits::DnDAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirst<ChannelType::DnDAT>(opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirstUpDAT(std::initializer_list<typename Flits::UpDAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetFirst<ChannelType::UpDAT>(opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDnRSP(std::initializer_list<typename Flits::DnRSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetLast<ChannelType::DnRSP>(opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastUpRSP(std::initializer_list<typename Flits::UpRSP<config>::opcode_t> opcodes) const noexcept
    {
        return GetLast<ChannelType::UpRSP>(opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastDnDAT(std::initializer_list<typename Flits::DnDAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetLast<ChannelType::DnDAT>(opcodes);
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLastUpDAT(std::initializer_list<typename Flits::UpDAT<config>::opcode_t> opcodes) const noexcept
    {
        return GetLast<ChannelType::UpDAT>(opcodes);
    }

    template<FlitConfigurationConcept config>
    template<ChannelTypeEnum channel, class T>
    inline const FiredResponseFlit<config>* Xaction<config>::GetFirst(std::initializer_list<T> opcodes) const noexcept
    {
        if (opcodes.size() == 0)
            return nullptr;

        for (auto keyIt = subsequenceKeys.begin(), flitIt = subsequence.begin();
             keyIt != subsequenceKeys.end(); ++keyIt, ++flitIt)
        {
            if (keyIt->IsDenied())
                continue;

            if (keyIt->GetChannelType() != channel)
                continue;

            for (auto opcode : opcodes)
            {
                if constexpr (channel == ChannelType::DnRSP)
                {
                    if (keyIt->opcode.dnrsp == opcode)
                        return &*flitIt;
                }
                else if constexpr (channel == ChannelType::UpRSP)
                {
                    if (keyIt->opcode.uprsp == opcode)
                        return &*flitIt;
                }
                else if constexpr (channel == ChannelType::DnDAT)
                {
                    if (keyIt->opcode.dndat == opcode)
                        return &*flitIt;
                }
                else // UpDAT
                {
                    if (keyIt->opcode.updat == opcode)
                        return &*flitIt;
                }
            }
        }

        return nullptr;
    }

    template<FlitConfigurationConcept config>
    template<ChannelTypeEnum channel, class T>
    inline const FiredResponseFlit<config>* Xaction<config>::GetLast(std::initializer_list<T> opcodes) const noexcept
    {
        if (opcodes.size() == 0)
            return nullptr;

        for (auto keyIt = subsequenceKeys.rbegin(), flitIt = subsequence.rbegin();
             keyIt != subsequenceKeys.rend(); ++keyIt, ++flitIt)
        {
            if (keyIt->IsDenied())
                continue;

            if (keyIt->GetChannelType() != channel)
                continue;

            for (auto opcode : opcodes)
            {
                if constexpr (channel == ChannelType::DnRSP)
                {
                    if (keyIt->opcode.dnrsp == opcode)
                        return &*flitIt;
                }
                else if constexpr (channel == ChannelType::UpRSP)
                {
                    if (keyIt->opcode.uprsp == opcode)
                        return &*flitIt;
                }
                else if constexpr (channel == ChannelType::DnDAT)
                {
                    if (keyIt->opcode.dndat == opcode)
                        return &*flitIt;
                }
                else // UpDAT
                {
                    if (keyIt->opcode.updat == opcode)
                        return &*flitIt;
                }
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
    inline std::optional<typename Flits::DnRSP<config>::dbid_t> Xaction<config>::GetDBID() const noexcept
    {
        const FiredResponseFlit<config>* dbidSource = GetDBIDSource();

        if (dbidSource == nullptr)
            return std::nullopt;

        if (dbidSource->IsDnRSP())
            return { dbidSource->flit.dnrsp.DBID };
        else if (dbidSource->IsDnDAT())
            return { static_cast<typename Flits::DnRSP<config>::dbid_t>(dbidSource->flit.dndat.DBID) };

        return std::nullopt;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* Xaction<config>::GetDBIDSource() const noexcept
    {
        for (auto keyIt = subsequenceKeys.begin(), flitIt = subsequence.begin();
             keyIt != subsequenceKeys.end(); ++keyIt, ++flitIt)
        {
            if (keyIt->IsDenied())
                continue;

            if (keyIt->HasDBID())
                return &*flitIt;
        }

        return nullptr;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::Next(const Global<config>& glbl, const FiredResponseFlit<config>& flit, bool& hasDBID, bool& firstDBID) noexcept
    {
        if (flit.IsDnRSP())
            return NextDnRSP(glbl, flit, hasDBID, firstDBID);
        else if (flit.IsUpRSP())
            return NextUpRSP(glbl, flit, hasDBID, firstDBID);
        else if (flit.IsDnDAT())
            return NextDnDAT(glbl, flit, hasDBID, firstDBID);
        else if (flit.IsUpDAT())
            return NextUpDAT(glbl, flit, hasDBID, firstDBID);

        return XactDenial::NOT_INITIALIZED;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::NextDnRSP(const Global<config>& glbl, const FiredResponseFlit<config>& dnrspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!dnrspFlit.IsDnRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNRSP, dnrspFlit);

        XactDenialEnum denial = this->NextDnRSPNoRecord(glbl, dnrspFlit, hasDBID, firstDBID);

        subsequence.push_back(dnrspFlit);
        subsequenceKeys.emplace_back(ChannelTypeTag<ChannelType::DnRSP>{}, denial, dnrspFlit.flit.dnrsp.Opcode, hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::NextUpRSP(const Global<config>& glbl, const FiredResponseFlit<config>& uprspFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!uprspFlit.IsUpRSP()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPRSP, uprspFlit);

        XactDenialEnum denial = this->NextUpRSPNoRecord(glbl, uprspFlit, hasDBID, firstDBID);

        subsequence.push_back(uprspFlit);
        subsequenceKeys.emplace_back(ChannelTypeTag<ChannelType::UpRSP>{}, denial, uprspFlit.flit.uprsp.Opcode, hasDBID);

        return denial;
    }
    
    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::NextDnDAT(const Global<config>& glbl, const FiredResponseFlit<config>& dndatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!dndatFlit.IsDnDAT()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_DNDAT, dndatFlit);

        XactDenialEnum denial = this->NextDnDATNoRecord(glbl, dndatFlit, hasDBID, firstDBID);

        subsequence.push_back(dndatFlit);
        subsequenceKeys.emplace_back(ChannelTypeTag<ChannelType::DnDAT>{}, denial, dndatFlit.flit.dndat.Opcode, hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::NextUpDAT(const Global<config>& glbl, const FiredResponseFlit<config>& updatFlit, bool& hasDBID, bool& firstDBID) noexcept
    {
        hasDBID = false;
        firstDBID = false;

        if (!updatFlit.IsUpDAT()) [[unlikely]]
            return this->ResponseFlitDenied(XactDenial::DENIED_CHANNEL_NOT_UPDAT, updatFlit);

        XactDenialEnum denial = this->NextUpDATNoRecord(glbl, updatFlit, hasDBID, firstDBID);

        subsequence.push_back(updatFlit);
        subsequenceKeys.emplace_back(ChannelTypeTag<ChannelType::UpDAT>{}, denial, updatFlit.flit.updat.Opcode, hasDBID);

        return denial;
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::NextDataID(Flits::REQ<config>::ssize_t                                     size, 
                                            const FiredResponseFlit<config>&                                datFlit, 
                                            std::initializer_list<typename Flits::DnDAT<config>::opcode_t>  dndatOpcodes,
                                            std::initializer_list<typename Flits::UpDAT<config>::opcode_t>  updatOpcodes) noexcept
    {
        if (datFlit.IsDnDAT())
        {
            std::function<bool(size_t, const FiredResponseFlit<config>&)> validFuncDn =
                [&](size_t i, const FiredResponseFlit<config>& flit) -> bool
            {
                bool opcodeMatch = false;
                if (datFlit.IsDnDAT() && flit.IsDnDAT())
                {
                    if (dndatOpcodes.size() > 0)
                        opcodeMatch = dndatOpcodes.contains(flit.flit.dndat.Opcode);
                    else
                        opcodeMatch = true;
                }

                return opcodeMatch && this->subsequenceKeys[i].IsAccepted();
            };

            std::bitset<8> collectedDataID = details::CollectDnDataID<config>(
                size, this->subsequence, validFuncDn);

            std::bitset<8> nextDataID = details::CollectDataID<config>(
                size, datFlit.flit.dndat.DataID);

            return (collectedDataID & nextDataID).none();
        }
        else if (datFlit.IsUpDAT())
        {
            std::function<bool(size_t, const FiredResponseFlit<config>&)> validFuncUp =
                [&](size_t i, const FiredResponseFlit<config>& flit) -> bool
            {
                bool opcodeMatch = false;
                if (datFlit.IsUpDAT() && flit.IsUpDAT())
                {
                    if (updatOpcodes.size() > 0)
                        opcodeMatch = updatOpcodes.contains(flit.flit.updat.Opcode);
                    else
                        opcodeMatch = true;
                }

                return opcodeMatch && this->subsequenceKeys[i].IsAccepted();
            };

            std::bitset<8> collectedDataID = details::CollectUpDataID<config>(
                size, this->subsequence, validFuncUp);

            std::bitset<8> nextDataID = details::CollectDataID<config>(
                size, datFlit.flit.updat.DataID);

            return (collectedDataID & nextDataID).none();
        }
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::NextEVTDataID(const FiredResponseFlit<config>&                                 datFlit, 
                                               std::initializer_list<typename Flits::UpDAT<config>::opcode_t>   updatOpcodes) noexcept
    {
        return NextDataID(Sizes::B64, datFlit, {}, updatOpcodes);
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::NextSNPDataID(const FiredResponseFlit<config>&                                 datFlit, 
                                               std::initializer_list<typename Flits::UpDAT<config>::opcode_t>   updatOpcodes) noexcept
    {
        return NextDataID(Sizes::B64, datFlit, {}, updatOpcodes);
    }

    template<FlitConfigurationConcept config>
    inline bool Xaction<config>::NextREQDataID(const FiredResponseFlit<config>&                                 datFlit, 
                                               std::initializer_list<typename Flits::DnDAT<config>::opcode_t>   dndatOpcodes,
                                               std::initializer_list<typename Flits::UpDAT<config>::opcode_t>   updatOpcodes) noexcept
    {
        return NextDataID(Sizes::B64, datFlit, dndatOpcodes, updatOpcodes);
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::RequestFlitDenied(
        XactDenialEnum                  denial,
        const FiredRequestFlit<config>& flit,
        const std::string&              message) noexcept
    {
        if (this->events)
            this->events->OnDeniedRequestFlit(XactionDeniedRequestFlitEvent<config>(
                *this, denial, flit, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::RequestFlitDenied(
        XactDenialEnum                  denial,
        const FiredRequestFlit<config>& flit,
        const FiredRequestFlit<config>& complementRequest,
        const std::string&              message) noexcept
    {
        if (this->events)
            this->events->OnDeniedRequestFlit(XactionDeniedRequestFlitEvent<config>(
                *this, denial, flit, complementRequest, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::ResponseFlitDenied(
        XactDenialEnum                   denial,
        const FiredResponseFlit<config>& flit,
        const std::string&               message) noexcept
    {
        if (this->events)
            this->events->OnDeniedResponseFlit(XactionDeniedResponseFlitEvent<config>(
                *this, denial, flit, message));
        return denial;
    }

    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::ResponseFlitDenied(
        XactDenialEnum                   denial,
        const FiredResponseFlit<config>& flit,
        const FiredResponseFlit<config>& complementResponse,
        const std::string&               message) noexcept
    {
        if (this->events)
            this->events->OnDeniedResponseFlit(XactionDeniedResponseFlitEvent<config>(
                *this, denial, flit, complementResponse, message));
        return denial;
    }
    
    template<FlitConfigurationConcept config>
    inline XactDenialEnum Xaction<config>::ResponseFlitDenied(
        XactDenialEnum                   denial,
        const FiredResponseFlit<config>& flit,
        const FiredRequestFlit<config>&  complementRequest,
        const std::string&               message) noexcept
    {
        if (this->events)
            this->events->OnDeniedResponseFlit(XactionDeniedResponseFlitEvent<config>(
                *this, denial, flit, complementRequest, message));
        return denial;
    }
}


// Implementation of: class XactionEventBase
namespace CCHI::Xact {

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
namespace CCHI::Xact {

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
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionDeniedRequestFlitEvent<config>::XactionDeniedRequestFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredRequestFlit<config>& flit, const std::string& message) noexcept
        : XactionDeniedEventBase<config>    (xaction, denial, message)
        , flit                              (flit)
        , complementRequest                 (nullptr)
    { }

    template<FlitConfigurationConcept config>
    inline XactionDeniedRequestFlitEvent<config>::XactionDeniedRequestFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredRequestFlit<config>& flit, const FiredRequestFlit<config>& complementRequest, const std::string& message) noexcept
        : XactionDeniedEventBase<config>    (xaction, denial, message)
        , flit                              (flit)
        , complementRequest                 (&complementRequest)
    { }

    template<FlitConfigurationConcept config>
    inline const FiredRequestFlit<config>& XactionDeniedRequestFlitEvent<config>::GetFlit() const noexcept
    {
        return flit;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDeniedRequestFlitEvent<config>::HasComplementRequest() const noexcept
    {
        return complementRequest != nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredRequestFlit<config>* XactionDeniedRequestFlitEvent<config>::GetComplementRequest() const noexcept
    {
        return complementRequest;
    }
}

// Implementation of: class XactionDeniedResponseFlitEvent
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline XactionDeniedResponseFlitEvent<config>::XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const std::string& message) noexcept
        : XactionDeniedEventBase<config>    (xaction, denial, message)
        , flit                              (flit)
        , complementResponse                (nullptr)
        , complementRequest                 (nullptr)
    { }

    template<FlitConfigurationConcept config>
    inline XactionDeniedResponseFlitEvent<config>::XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const FiredResponseFlit<config>& complementResponse, const std::string& message) noexcept
        : XactionDeniedEventBase<config>    (xaction, denial, message)
        , flit                              (flit)
        , complementResponse                (&complementResponse)
        , complementRequest                 (nullptr)
    { }

    template<FlitConfigurationConcept config>
    inline XactionDeniedResponseFlitEvent<config>::XactionDeniedResponseFlitEvent(Xaction<config>& xaction, XactDenialEnum denial, const FiredResponseFlit<config>& flit, const FiredRequestFlit<config>& complementRequest, const std::string& message) noexcept
        : XactionDeniedEventBase<config>    (xaction, denial, message)
        , flit                              (flit)
        , complementResponse                (nullptr)
        , complementRequest                 (&complementRequest)
    { }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>& XactionDeniedResponseFlitEvent<config>::GetFlit() const noexcept
    {
        return flit;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDeniedResponseFlitEvent<config>::HasComplementResponse() const noexcept
    {
        return complementResponse != nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredResponseFlit<config>* XactionDeniedResponseFlitEvent<config>::GetComplementResponse() const noexcept
    {
        return complementResponse;
    }

    template<FlitConfigurationConcept config>
    inline bool XactionDeniedResponseFlitEvent<config>::HasComplementRequest() const noexcept
    {
        return complementRequest != nullptr;
    }

    template<FlitConfigurationConcept config>
    inline const FiredRequestFlit<config>* XactionDeniedResponseFlitEvent<config>::GetComplementRequest() const noexcept
    {
        return complementRequest;
    }
}


#endif // __CCHI__CCHI_XACT_XACTIONS__BASE
