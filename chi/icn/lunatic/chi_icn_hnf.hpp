//#pragma once

//#ifndef __CHI__LUNATIC__CHI_ICN_HNF
//#define __CHI__LUNATIC__CHI_ICN_HNF

#include <cstdint>
#ifndef LUNATIC_CHI_ICN_HNF__STANDALONE
#   include "chi_icn_hnf_header.hpp"            // IWYU pragma: keep
#   include "../../xact/chi_xactions.hpp"       // IWYU pragma: keep
#   include "../../xact/chi_joint.hpp"          // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__LUNATIC__CHI_ICN_HNF_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__LUNATIC__CHI_ICN_HNF_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__LUNATIC__CHI_ICN_HNF_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__LUNATIC__CHI_ICN_HNF_EB
#endif


/*
namespace CHI {
*/
    namespace Lunatic {

        class HNFCacheStateEnumBack {
        public:
            const char* name;
            const int   value;
            const char* description;

        public:
            inline constexpr HNFCacheStateEnumBack(const char* name, int value, const char* description = "") noexcept
            : name(name), value(value), description(description) { }

        public:
            inline constexpr operator int() const noexcept
            { return value; }

            inline constexpr operator const HNFCacheStateEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const HNFCacheStateEnumBack& other) const noexcept
            { return this->value == other.value; }

            inline constexpr bool operator!=(const HNFCacheStateEnumBack& other) const noexcept
            { return this->value != other.value; }
        };

        using HNFCacheStateEnum = const HNFCacheStateEnumBack*;

        namespace HNFCacheState {
            inline constexpr HNFCacheStateEnumBack II  = HNFCacheStateEnumBack("II"   , 0 , "HN Invalid, RN Invalid");
        //  inline constexpr HNFCacheStateEnumBack IS  = HNFCacheStateEnumBack("IS"   , 1);
        //  inline constexpr HNFCacheStateEnumBack IU  = HNFCacheStateEnumBack("IU"   , 2);
        //  inline constexpr HNFCacheStateEnumBack SI  = HNFCacheStateEnumBack("SI"   , 3);
        //  inline constexpr HNFCacheStateEnumBack SS  = HNFCacheStateEnumBack("SS"   , 4);
        //  inline constexpr HNFCacheStateEnumBack SSL = HNFCacheStateEnumBack("SSL"  , 5);
        //  inline constexpr HNFCacheStateEnumBack SU  = HNFCacheStateEnumBack("SU"   , 6);
        //  inline constexpr HNFCacheStateEnumBack SUL = HNFCacheStateEnumBack("SUL"  , 7);
            inline constexpr HNFCacheStateEnumBack UI  = HNFCacheStateEnumBack("UI"   , 8 , "HN Unique, RN Invalid");
            inline constexpr HNFCacheStateEnumBack UIL = HNFCacheStateEnumBack("UIL"  , 9 , "HN Unique with local clean copy, RN Invalid");
            inline constexpr HNFCacheStateEnumBack UID = HNFCacheStateEnumBack("UID"  , 10, "HN Unique with local dirty copy, RN Invalid");
            inline constexpr HNFCacheStateEnumBack US  = HNFCacheStateEnumBack("US"   , 11, "HN Unique, RN Shared");
            inline constexpr HNFCacheStateEnumBack USL = HNFCacheStateEnumBack("USL"  , 12, "HN Unique with local clean copy, RN Shared");
            inline constexpr HNFCacheStateEnumBack USD = HNFCacheStateEnumBack("USD"  , 13, "HN Unique with local dirty copy, RN Shared");
            inline constexpr HNFCacheStateEnumBack UU  = HNFCacheStateEnumBack("UU"   , 14, "HN Unique, RN Unique");
            inline constexpr HNFCacheStateEnumBack UUL = HNFCacheStateEnumBack("UUL"  , 15, "HN Unique with local clean copy, RN Unique");
            inline constexpr HNFCacheStateEnumBack UUD = HNFCacheStateEnumBack("UUD"  , 16, "HN Unique with local dirty copy, RN Unique");
        };

        template<FlitConfigurationConcept config>
        using nodeid_t = typename Flits::REQ<config>::tgtid_t;

        template<FlitConfigurationConcept config>
        using HNFNodeTable = std::vector<nodeid_t<config>>;

        class HNFDirectoryEntry {
        protected:
            uint64_t                        tag;
            HNFCacheStateEnum               state;

        protected:
            static constexpr uint64_t   GetTag(size_t setOffset, uint64_t PA) noexcept;

        public:
            HNFDirectoryEntry() noexcept;
            HNFDirectoryEntry(uint64_t tag, HNFCacheStateEnum state) noexcept;
            HNFDirectoryEntry(size_t setOffset, uint64_t PA, HNFCacheStateEnum state) noexcept;

        public:
            uint64_t            GetTag() const noexcept;
            void                SetTag(uint64_t tag) noexcept;
            void                SetTag(size_t setOffset, uint64_t PA) noexcept;

            HNFCacheStateEnum   GetState() const noexcept;
            void                SetState(HNFCacheStateEnum state) noexcept;

            bool                IsHit(uint64_t tag) const noexcept;
            bool                IsHit(size_t setOffset, uint64_t PA) const noexcept;
        };

        class HNFDirectory {
        protected:
            size_t                          waySize;
            size_t                          setSize;
            size_t                          setOffset;

            bool                            setNP2;

            HNFDirectoryEntry*              entries;
            size_t                          entryCount;

        public:
            HNFDirectory(size_t waySize, size_t setSize) noexcept;
            ~HNFDirectory() noexcept;

        public:
            void                        Clear() noexcept;
            size_t                      GetWaySize() const noexcept;
            size_t                      GetSetSize() const noexcept;
            size_t                      GetSize() const noexcept;

            uint64_t                    GetSet(uint64_t PA) const noexcept;
            int64_t                     GetWay(uint64_t PA) const noexcept;
            int64_t                     GetWay(uint64_t PA, size_t setIndex) const noexcept;
            int64_t                     GetWay(uint64_t PA, size_t* setIndex) const noexcept;

            HNFDirectoryEntry&          GetEntry(size_t setIndex, size_t wayIndex) noexcept;
            const HNFDirectoryEntry&    GetEntry(size_t setIndex, size_t wayIndex) const noexcept;

            HNFDirectoryEntry*          GetEntries(size_t setIndex) noexcept;
            const HNFDirectoryEntry*    GetEntries(size_t setIndex) const noexcept;

            HNFDirectoryEntry*          Get(uint64_t PA, size_t* setIndex = nullptr, size_t* wayIndex = nullptr) noexcept;
            const HNFDirectoryEntry*    Get(uint64_t PA, size_t* setIndex = nullptr, size_t* wayIndex = nullptr) const noexcept;

            HNFCacheStateEnum           Query(uint64_t PA, size_t* setIndex = nullptr, size_t* wayIndex = nullptr) const noexcept;
        };

        class HNFSnoopFilterEntry {
        protected:
            size_t                          size;
            HNFCacheStateEnum               state;
            std::vector<std::bitset<64>>    vector;

        public:
            HNFSnoopFilterEntry(size_t size) noexcept;

        public:
            size_t                          Size() const noexcept;

            HNFCacheStateEnum               GetState() const noexcept;
            void                            SetState(HNFCacheStateEnum state) noexcept;

            void                            ClearVector() noexcept;

            bool                            UpdateVectorI(size_t index) noexcept;
            template<FlitConfigurationConcept config>
            bool                            UpdateVectorI(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) noexcept;

            bool                            UpdateVectorS(size_t index) noexcept;
            template<FlitConfigurationConcept config>
            bool                            UpdateVectorS(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) noexcept;

            bool                            UpdateVectorU(size_t index) noexcept;
            template<FlitConfigurationConcept config>
            bool                            UpdateVectorU(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) noexcept;
            
            bool                            IsPresent(size_t index) const noexcept;
            template<FlitConfigurationConcept config>
            bool                            IsPresent(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) const noexcept;

            std::vector<size_t>             CollectPresent() const noexcept;
            template<FlitConfigurationConcept config>
            std::vector<nodeid_t<config>>   CollectPresent(const HNFNodeTable<config>& nodeTable) const noexcept;
        };

        class HNFSnoopFilter {
        protected:
            size_t                          setSize;
            size_t                          setOffset;
            size_t                          waySize;
            size_t                          vectorSize;

            bool                            setNP2;

            HNFSnoopFilterEntry*            entries;
            size_t                          entryCount;

        public:
            HNFSnoopFilter(size_t setSize, size_t waySize, size_t vectorSize) noexcept;
            ~HNFSnoopFilter() noexcept;

        public:
            size_t                          Size() const noexcept;
            void                            Clear() noexcept;
            
            // TODO
        };

        class HNFReplacer {
            // TODO
        };

        template<FlitConfigurationConcept config>
        class HNFTransaction {
        
        // TODO

        public:
            virtual bool        IsDone() const noexcept = 0;
            virtual uint64_t    GetPA() const noexcept;
            
        public:
            virtual std::shared_ptr<Flits::REQ<config>> PeekTXREQ() const noexcept = 0;
            virtual std::shared_ptr<Flits::REQ<config>> PopTXREQ() noexcept = 0;

            virtual std::shared_ptr<Flits::RSP<config>> PeekTXRSP() const noexcept = 0;
            virtual std::shared_ptr<Flits::RSP<config>> PopTXRSP() noexcept = 0;

            virtual std::shared_ptr<Flits::DAT<config>> PeekTXDAT() const noexcept = 0;
            virtual std::shared_ptr<Flits::DAT<config>> PopTXDAT() noexcept = 0;

            virtual std::shared_ptr<Flits::SNP<config>> PeekTXSNP() const noexcept = 0;
            virtual std::shared_ptr<Flits::SNP<config>> PopTXSNP() noexcept = 0;
        };

        template<FlitConfigurationConcept config>
        class HNFTransactionAllocatingRead : public HNFTransaction<config> {
        protected:
        };

        template<FlitConfigurationConcept config>
        class HNFTransactionSnoop : public HNFTransaction<config> {
        protected:
        };


        /*
        HN-G node (Global Configuration Home Node)
        */
        template<FlitConfigurationConcept config>
        class HNG {

        };

        /*
        HN-F node (Coherent Home Node)
        */
        template<FlitConfigurationConcept config>
        class HNF {
        public:
            HNF(Flits::REQ<config>::tgtid_t nodeId) noexcept;

            // TODO: Snoop Filter

        public:
            // TODO

            std::shared_ptr<Flits::REQ<config>> RXREQ() const noexcept;
            std::shared_ptr<Flits::SNP<config>> RXSNP() const noexcept;
            std::shared_ptr<Flits::RSP<config>> RXRSP() const noexcept;
            std::shared_ptr<Flits::DAT<config>> RXDAT() const noexcept;

            bool                                RXREQ(std::shared_ptr<Flits::REQ<config>>) noexcept;
            bool                                RXSNP(std::shared_ptr<Flits::SNP<config>>) noexcept;
            bool                                RXRSP(std::shared_ptr<Flits::RSP<config>>) noexcept;
            bool                                RXDAT(std::shared_ptr<Flits::DAT<config>>) noexcept;

            std::shared_ptr<Flits::REQ<config>> TXREQ() const noexcept;
            std::shared_ptr<Flits::SNP<config>> TXSNP(std::vector<typename Flits::SNP<config>::srcid_t>* snpTgtIds = nullptr) const noexcept;
            std::shared_ptr<Flits::RSP<config>> TXRSP() const noexcept;
            std::shared_ptr<Flits::DAT<config>> TXDAT() const noexcept;

        public:
            void Eval(uint64_t timestamp) noexcept;
        };
    }
/*
}
*/


// Implementation of: class HNFDirectoryEntry
namespace /*CHI::*/Lunatic {

    inline constexpr uint64_t HNFDirectoryEntry::GetTag(size_t setOffset, uint64_t PA) noexcept
    {
        return PA >> (setOffset + 6);
    }

    inline HNFDirectoryEntry::HNFDirectoryEntry() noexcept
        : tag   (0)
        , state (HNFCacheState::II)
    { }

    inline HNFDirectoryEntry::HNFDirectoryEntry(uint64_t tag, HNFCacheStateEnum state) noexcept
        : tag   (tag)
        , state (state)
    { }

    inline HNFDirectoryEntry::HNFDirectoryEntry(size_t setOffset, uint64_t PA, HNFCacheStateEnum state) noexcept
        : tag   (GetTag(setOffset, PA))
        , state (state)
    { }

    inline uint64_t HNFDirectoryEntry::GetTag() const noexcept
    {
        return tag;
    }

    inline void HNFDirectoryEntry::SetTag(uint64_t tag) noexcept
    {
        this->tag = tag;
    }

    inline void HNFDirectoryEntry::SetTag(size_t setOffset, uint64_t PA) noexcept
    {
        this->tag = GetTag(setOffset, PA);
    }

    inline HNFCacheStateEnum HNFDirectoryEntry::GetState() const noexcept
    {
        return state;
    }

    inline void HNFDirectoryEntry::SetState(HNFCacheStateEnum state) noexcept
    {
        this->state = state;
    }

    inline bool HNFDirectoryEntry::IsHit(uint64_t tag) const noexcept
    {
        return this->tag == tag && this->state != HNFCacheState::II;
    }

    inline bool HNFDirectoryEntry::IsHit(size_t setOffset, uint64_t PA) const noexcept
    {
        return IsHit(GetTag(setOffset, PA));
    }
}

// Implementation of: class HNFDirectory
namespace /*CHI::*/Lunatic {

    inline HNFDirectory::HNFDirectory(size_t waySize, size_t setSize) noexcept
        : waySize   (waySize)
        , setSize   (setSize)
    {
        setOffset = 0;
        for (size_t s = setSize; s > 1; s >>= 1)
            setOffset++;

        setNP2 = std::popcount(setSize) != 1;

        entries = new HNFDirectoryEntry[waySize * setSize];
        entryCount = waySize * setSize;
    }

    inline HNFDirectory::~HNFDirectory() noexcept
    {
        delete[] entries;
        entries = nullptr;
    }

    inline void HNFDirectory::Clear() noexcept
    {
        for (size_t i = 0; i < entryCount; i++)
            entries[i] = HNFDirectoryEntry();
    }

    inline size_t HNFDirectory::GetWaySize() const noexcept
    {
        return waySize;
    }

    inline size_t HNFDirectory::GetSetSize() const noexcept
    {
        return setSize;
    }

    inline size_t HNFDirectory::GetSize() const noexcept
    {
        return entryCount;
    }

    inline uint64_t HNFDirectory::GetSet(uint64_t PA) const noexcept
    {
        if (setNP2)
            return (PA >> 6) % setSize;
        else
            return (PA >> 6) & (setSize - 1);
    }

    inline int64_t HNFDirectory::GetWay(uint64_t PA) const noexcept
    {
        return GetWay(PA, GetSet(PA));
    }

    inline int64_t HNFDirectory::GetWay(uint64_t PA, size_t setIndex) const noexcept
    {
        for (size_t wayIndex = 0; wayIndex < waySize; wayIndex++)
        {
            if (GetEntry(setIndex, wayIndex).IsHit(setOffset, PA))
                return wayIndex;
        }

       return -1;
    }

    inline int64_t HNFDirectory::GetWay(uint64_t PA, size_t* setIndex) const noexcept
    {
        size_t s = GetSet(PA);

        if (setIndex)
            *setIndex = s;

        return GetWay(PA, s);
    }

    inline HNFDirectoryEntry& HNFDirectory::GetEntry(size_t setIndex, size_t wayIndex) noexcept
    {
        return entries[setIndex * waySize + wayIndex];
    }

    inline const HNFDirectoryEntry& HNFDirectory::GetEntry(size_t setIndex, size_t wayIndex) const noexcept
    {
        return entries[setIndex * waySize + wayIndex];
    }

    inline HNFDirectoryEntry* HNFDirectory::GetEntries(size_t setIndex) noexcept
    {
        return &entries[setIndex * waySize];
    }

    inline const HNFDirectoryEntry* HNFDirectory::GetEntries(size_t setIndex) const noexcept
    {
        return &entries[setIndex * waySize];
    }

    inline HNFDirectoryEntry* HNFDirectory::Get(uint64_t PA, size_t* setIndex, size_t* wayIndex) noexcept
    {
        size_t s = GetSet(PA);
        size_t w = GetWay(PA, s);

        if (setIndex) *setIndex = s;
        if (wayIndex) *wayIndex = w;

        if (w < 0)
            return nullptr;

        return &GetEntry(s, w);
    }

    inline const HNFDirectoryEntry* HNFDirectory::Get(uint64_t PA, size_t* setIndex, size_t* wayIndex) const noexcept
    {
        size_t s = GetSet(PA);
        size_t w = GetWay(PA, s);

        if (setIndex) *setIndex = s;
        if (wayIndex) *wayIndex = w;

        if (w < 0)
            return nullptr;

        return &GetEntry(s, w);
    }

    inline HNFCacheStateEnum HNFDirectory::Query(uint64_t PA, size_t* setIndex, size_t* wayIndex) const noexcept
    {
        const HNFDirectoryEntry* entry = Get(PA, setIndex, wayIndex);
        if (!entry)
            return HNFCacheState::II;

        return entry->GetState();
    }
}


// Implementation of: class HNFSnoopFilterEntry
namespace /*CHI::*/Lunatic {

    inline HNFSnoopFilterEntry::HNFSnoopFilterEntry(size_t size) noexcept
        : size  (size)
        , state (HNFCacheState::II)
    {
        size_t numBits = (size + 63) >> 6;
        vector.resize(numBits);
    }

    inline size_t HNFSnoopFilterEntry::Size() const noexcept
    {
        return size;
    }

    inline HNFCacheStateEnum HNFSnoopFilterEntry::GetState() const noexcept
    {
        return state;
    }

    inline void HNFSnoopFilterEntry::SetState(HNFCacheStateEnum state) noexcept
    {
        this->state = state;
    }

    inline void HNFSnoopFilterEntry::ClearVector() noexcept
    {
        for (auto& bits : vector)
            bits.reset();
    }

    inline bool HNFSnoopFilterEntry::UpdateVectorI(size_t index) noexcept
    {
        if (index >= size)
            return false;

        vector[index >> 6].reset(index & 0x3F);
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool HNFSnoopFilterEntry::UpdateVectorI(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) noexcept
    {
        auto it = std::find(nodeTable.begin(), nodeTable.end(), nodeId);
        if (it == nodeTable.end())
            return false;

        return UpdateI(std::distance(nodeTable.begin(), it));
    }

    inline bool HNFSnoopFilterEntry::UpdateVectorS(size_t index) noexcept
    {
        if (index >= size)
            return false;

        vector[index >> 6].set(index & 0x3F);
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool HNFSnoopFilterEntry::UpdateVectorS(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) noexcept
    {
        auto it = std::find(nodeTable.begin(), nodeTable.end(), nodeId);
        if (it == nodeTable.end())
            return false;

        return UpdateS(std::distance(nodeTable.begin(), it));
    }

    inline bool HNFSnoopFilterEntry::UpdateVectorU(size_t index) noexcept
    {
        if (index >= size)
            return false;

        for (auto& bits : vector)
            bits.reset();
        
        vector[index >> 6].set(index & 0x3F);
        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool HNFSnoopFilterEntry::UpdateVectorU(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) noexcept
    {
        auto it = std::find(nodeTable.begin(), nodeTable.end(), nodeId);
        if (it == nodeTable.end())
            return false;

        return UpdateU(std::distance(nodeTable.begin(), it));
    }

    inline bool HNFSnoopFilterEntry::IsPresent(size_t index) const noexcept
    {
        if (index >= size)
            return false;

        return vector[index >> 6].test(index & 0x3F);
    }

    template<FlitConfigurationConcept config>
    inline bool HNFSnoopFilterEntry::IsPresent(const HNFNodeTable<config>& nodeTable, nodeid_t<config> nodeId) const noexcept
    {
        auto it = std::find(nodeTable.begin(), nodeTable.end(), nodeId);
        if (it == nodeTable.end())
            return false;

        return IsPresent(std::distance(nodeTable.begin(), it));
    }

    inline std::vector<size_t> HNFSnoopFilterEntry::CollectPresent() const noexcept
    {
        std::vector<size_t> result;
        for (size_t i = 0; i < size; i++)
        {
            if (IsPresent(i))
                result.push_back(i);
        }
        return result;
    }

    template<FlitConfigurationConcept config>
    inline std::vector<nodeid_t<config>> HNFSnoopFilterEntry::CollectPresent(const HNFNodeTable<config>& nodeTable) const noexcept
    {
        std::vector<nodeid_t<config>> result;
        for (size_t i = 0; i < size; i++)
        {
            if (IsPresent(i))
                result.push_back(nodeTable[i]);
        }
        return result;
    }
}


#endif
