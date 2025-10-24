//#pragma once

//#ifndef __CHI__CHI_XACT_STATE
//#define __CHI__CHI_XACT_STATE

#include "chi_xact_state/includes.hpp"                      // IWYU pragma: keep
#include "chi_xact_state/base.hpp"                          // IWYU pragma: keep
#include "chi_xact_state/cst.hpp"                           // IWYU pragma: keep


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_STATE_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_STATE_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {

        namespace details {

            struct RNCohTrans {
                CacheState                      initial;
                CacheStateTransitions::Intermediates::details::TableG2
                    nested;
                const CacheStateTransitions::Intermediates::Tables*
                    tables;
                CacheStateTransition::Type      type;
            };
        }

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class RNCacheStateMap {
        private:
            static constexpr size_t ADDR_OFFSET_CACHE_BLOCK     = 6;
            static constexpr size_t ADDR_OFFSET_CACHE_MAP_SPAN  = 24;
            // *NOTICE: Used by access map of seer mode only
            // offset = 24 =>   1 GiB aligned ->   2 MiB bitset span
            // offset = 23 => 512 MiB aligned ->   1 MiB bitset span
            // offset = 22 => 256 MiB aligned -> 512 KiB bitset span
            // offset = 21 => 128 MiB aligned -> 256 KiB bitset span
            // offset = 20 =>  64 MiB aligned -> 128 KiB bitset span
            // offset = 19 =>  32 MiB aligned ->  64 KiB bitset span
            // offset = 18 =>  16 MiB aligned ->  32 KiB bitset span
            // offset = 17 =>   8 MiB aligned ->  16 KiB bitset span
            // offset = 16 =>   4 MiB aligned ->   8 KiB bitset span
            // offset = 15 =>   2 MiB aligned ->   4 KiB bitset span
            // offset = 14 =>   1 MiB aligned ->   2 KiB bitset span
            // ...
            static constexpr size_t ADDR_OFFSET_CACHE_MAP       = ADDR_OFFSET_CACHE_BLOCK + ADDR_OFFSET_CACHE_MAP_SPAN;

            static constexpr size_t SIZE_CACHE_BLOCK            = (1UL << ADDR_OFFSET_CACHE_BLOCK);
            static constexpr size_t SIZE_CACHE_MAP_SPAN         = (1UL << ADDR_OFFSET_CACHE_MAP_SPAN);

        private:
            Opcodes::REQ::Decoder<Flits::REQ<config>, const details::RNCohTrans*>
                        reqDecoder;

            Opcodes::SNP::Decoder<Flits::SNP<config>, const details::RNCohTrans*>
                        snpDecoder;

        private:
            std::unordered_map<typename Flits::REQ<config, conn>::addr_t::value_type, CacheState>
                        stateMap;

        private:
            bool        enableSilentEviction;
            bool        enableSilentSharing;
            bool        enableSilentStore;
            bool        enableSilentInvalidation;

        private:
            bool        seerEnabled;
            void*       seerConfusion;  // TODO

            std::unordered_map<typename Flits::REQ<config, conn>::addr_t::value_type, std::bitset<SIZE_CACHE_MAP_SPAN>>
                        seerAccessedMap;

        protected:
            bool        IsAccessed(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept;
            void        SetAccessed(Flits::REQ<config, conn>::addr_t::value_type addr) noexcept;

            std::pair<CacheState, bool> ExcavateWithSeer(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept;
            std::pair<CacheState, bool> EvaluateWithSeer(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept;

        public:
            CacheState  EvaluateSilently(CacheState state) const noexcept;

        public:
            RNCacheStateMap() noexcept;

        public:
            void            SetSilentEviction(bool enableSilentEviction) noexcept;
            void            SetSilentSharing(bool enableSilentSharing) noexcept;
            void            SetSilentStore(bool enableSilentStore) noexcept;
            void            SetSilentInvalidation(bool enableSilentInvalidation) noexcept;

            bool            HasSilentEviction() const noexcept;
            bool            HasSilentSharing() const noexcept;
            bool            HasSilentStore() const noexcept;
            bool            HasSilentInvalidation() const noexcept;

            void            Set(Flits::REQ<config, conn>::addr_t::value_type addr, CacheState state) noexcept;
            CacheState      Get(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept;

        public:
            CacheState      Excavate(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept;
            CacheState      Evaluate(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept;

        public:
            XactDenialEnum  NextTXREQ(Flits::REQ<config, conn>::addr_t::value_type addr, const Flits::REQ<config, conn>& flit) noexcept;
            XactDenialEnum  NextRXSNP(Flits::REQ<config, conn>::addr_t::value_type addr, const Flits::SNP<config, conn>& flit) noexcept;
            XactDenialEnum  NextTXRSP(Flits::REQ<config, conn>::addr_t::value_type addr, const Xaction<config, conn>& xaction, const Flits::RSP<config, conn>& flit) noexcept;
            XactDenialEnum  NextTXDAT(Flits::REQ<config, conn>::addr_t::value_type addr, const Xaction<config, conn>& xaction, const Flits::DAT<config, conn>& flit) noexcept;
            XactDenialEnum  NextRXRSP(Flits::REQ<config, conn>::addr_t::value_type addr, const Xaction<config, conn>& xaction, const Flits::RSP<config, conn>& flit) noexcept;
            XactDenialEnum  NextRXDAT(Flits::REQ<config, conn>::addr_t::value_type addr, const Xaction<config, conn>& xaction, const Flits::DAT<config, conn>& flit) noexcept;

        public:
            XactDenialEnum  Transfer(Flits::REQ<config, conn>::addr_t::value_type addr, CacheState state, const Xaction<config, conn>* nestingXaction = nullptr) noexcept;
        };
    }
/*
}
*/




// Implementation of: class RNCacheStateMap
namespace /*CHI::*/Xact {
    /*
    ...
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline RNCacheStateMap<config, conn>::RNCacheStateMap() noexcept
        : reqDecoder                ()
        , snpDecoder                ()
        , stateMap                  ()
        , enableSilentEviction      (true)
        , enableSilentSharing       (true)
        , enableSilentStore         (true)
        , enableSilentInvalidation  (true)
        , seerEnabled               (false)
        , seerConfusion             (nullptr)
        , seerAccessedMap           ()
    {
        // REQ transitions
        #define SET_REQ(type, opcode)  { \
            static const details::RNCohTrans _##opcode { \
                CacheStateTransitions::Initials::opcode, \
                CacheStateTransitions::Intermediates::Nested::opcode, \
                &CacheStateTransitions::Intermediates::opcode, \
                CacheStateTransition::Type::type \
            }; \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&_##opcode); \
        }

        #define SET_REQ_EX(type, opcode, name) { \
            static const details::RNCohTrans _##name { \
                CacheStateTransitions::Initials::name, \
                CacheStateTransitions::Intermediates::Nested::name, \
                &CacheStateTransitions::Intermediates::name, \
                CacheStateTransition::Type::type \
            }; \
            reqDecoder[Opcodes::REQ::opcode].SetCompanion(&_##name); \
        }

    //  SET_REQ(General         , ReqLCrdReturn                   );  // 0x00
        SET_REQ(Read            , ReadShared                      );  // 0x01
        SET_REQ(Read            , ReadClean                       );  // 0x02
        SET_REQ(Read            , ReadOnce                        );  // 0x03
        SET_REQ(Read            , ReadNoSnp                       );  // 0x04
    //  SET_REQ(General         , PCrdReturn                      );  // 0x05
                                                                      // 0x06
        SET_REQ(Read            , ReadUnique                      );  // 0x07
        SET_REQ(Dataless        , CleanShared                     );  // 0x08
        SET_REQ(Dataless        , CleanInvalid                    );  // 0x09
        SET_REQ(Dataless        , MakeInvalid                     );  // 0x0A
        SET_REQ(Dataless        , CleanUnique                     );  // 0x0B
        SET_REQ(Dataless        , MakeUnique                      );  // 0x0C
        SET_REQ(Dataless        , Evict                           );  // 0x0D
                                                                      // 0x0E
                                                                      // 0x0F
                                                                      // 0x10
    //  SET_REQ(                  ReadNoSnpSep                    );  // 0x11
                                                                      // 0x12
        SET_REQ(Dataless        , CleanSharedPersistSep           );  // 0x13
    //  SET_REQ(                  DVMOp                           );  // 0x14
        SET_REQ(WriteCopyBack   , WriteEvictFull                  );  // 0x15
                                                                      // 0x16
        SET_REQ(WriteCopyBack   , WriteCleanFull                  );  // 0x17
        SET_REQ(WriteNonCopyBack, WriteUniquePtl                  );  // 0x18
        SET_REQ(WriteNonCopyBack, WriteUniqueFull                 );  // 0x19
        SET_REQ(WriteCopyBack   , WriteBackPtl                    );  // 0x1A
        SET_REQ(WriteCopyBack   , WriteBackFull                   );  // 0x1B
        SET_REQ(WriteNonCopyBack, WriteNoSnpPtl                   );  // 0x1C
        SET_REQ(WriteNonCopyBack, WriteNoSnpFull                  );  // 0x1D
                                                                      // 0x1E
                                                                      // 0x1F
        SET_REQ(WriteNonCopyBack, WriteUniqueFullStash            );  // 0x20
        SET_REQ(WriteNonCopyBack, WriteUniquePtlStash             );  // 0x21
        SET_REQ(Dataless        , StashOnceShared                 );  // 0x22
        SET_REQ(Dataless        , StashOnceUnique                 );  // 0x23
        SET_REQ(Read            , ReadOnceCleanInvalid            );  // 0x24
        SET_REQ(Read            , ReadOnceMakeInvalid             );  // 0x25
        SET_REQ(Read            , ReadNotSharedDirty              );  // 0x26
        SET_REQ(Dataless        , CleanSharedPersist              );  // 0x27
        SET_REQ_EX(Atomic  , AtomicStore::ADD     , AtomicStore   );  // 0x28
        SET_REQ_EX(Atomic  , AtomicStore::CLR     , AtomicStore   );  // 0x29
        SET_REQ_EX(Atomic  , AtomicStore::EOR     , AtomicStore   );  // 0x2A
        SET_REQ_EX(Atomic  , AtomicStore::SET     , AtomicStore   );  // 0x2B
        SET_REQ_EX(Atomic  , AtomicStore::SMAX    , AtomicStore   );  // 0x2C
        SET_REQ_EX(Atomic  , AtomicStore::SMIN    , AtomicStore   );  // 0x2D
        SET_REQ_EX(Atomic  , AtomicStore::UMAX    , AtomicStore   );  // 0x2E
        SET_REQ_EX(Atomic  , AtomicStore::UMIN    , AtomicStore   );  // 0x2F
        SET_REQ_EX(Atomic  , AtomicLoad::ADD      , AtomicLoad    );  // 0x30
        SET_REQ_EX(Atomic  , AtomicLoad::CLR      , AtomicLoad    );  // 0x31
        SET_REQ_EX(Atomic  , AtomicLoad::EOR      , AtomicLoad    );  // 0x32
        SET_REQ_EX(Atomic  , AtomicLoad::SET      , AtomicLoad    );  // 0x33
        SET_REQ_EX(Atomic  , AtomicLoad::SMAX     , AtomicLoad    );  // 0x34
        SET_REQ_EX(Atomic  , AtomicLoad::SMIN     , AtomicLoad    );  // 0x35
        SET_REQ_EX(Atomic  , AtomicLoad::UMAX     , AtomicLoad    );  // 0x36
        SET_REQ_EX(Atomic  , AtomicLoad::UMIN     , AtomicLoad    );  // 0x37
        SET_REQ(Atomic          , AtomicSwap                      );  // 0x38
        SET_REQ(Atomic          , AtomicCompare                   );  // 0x39
    //  SET_REQ(                  PrefetchTgt                     );  // 0x3A
                                                                      // 0x3B
                                                                      // 0x3C
                                                                      // 0x3D
                                                                      // 0x3E
                                                                      // 0x3F
                                                                      // 0x40
        SET_REQ(MakeReadUnique  , MakeReadUnique                  );  // 0x41
        SET_REQ(WriteCopyBack   , WriteEvictOrEvict               );  // 0x42
        SET_REQ(WriteNonCopyBack, WriteUniqueZero                 );  // 0x43
        SET_REQ(WriteNonCopyBack, WriteNoSnpZero                  );  // 0x44
                                                                      // 0x45
                                                                      // 0x46
        SET_REQ(Dataless        , StashOnceSepShared              );  // 0x47
        SET_REQ(Dataless        , StashOnceSepUnique              );  // 0x48
                                                                      // 0x49
                                                                      // 0x4A
                                                                      // 0x4B
        SET_REQ(Read            , ReadPreferUnique                );  // 0x4C
                                                                      // 0x4D
                                                                      // 0x4E
                                                                      // 0x4F
    //  SET_REQ(                  WriteNoSnpFullCleanSh           );  // 0x50 // TODO: not listed in Issue E specification
    //  SET_REQ(                  WriteNoSnpFullCleanInv          );  // 0x51 // TODO: not listed in Issue E specification
    //  SET_REQ(                  WriteNoSnpFullCleanShPerSep     );  // 0x52 // TODO: not listed in Issue E specification
                                                                      // 0x53
    //  SET_REQ(                  WriteUniqueFullCleanSh          );  // 0x54 // TODO: not listed in Issue E specification
                                                                      // 0x55
    //  SET_REQ(                  WriteUniqueFullCleanShPerSep    );  // 0x56 // TODO: not listed in Issue E specification
                                                                      // 0x57
    //  SET_REQ(                  WriteBackFullCleanSh            );  // 0x58 // TODO: not listed in Issue E specification
    //  SET_REQ(                  WriteBackFullCleanInv           );  // 0x59 // TODO: not listed in Issue E specification
    //  SET_REQ(                  WriteBackFullCleanShPerSep      );  // 0x5A // TODO: not listed in Issue E specification
                                                                      // 0x5B
    //  SET_REQ(                  WriteCleanFullCleanSh           );  // 0x5C // TODO: not listed in Issue E specification
                                                                      // 0x5D
    //  SET_REQ(                  WriteCleanFullCleanShPerSep     );  // 0x5E // TODO: not listed in Issue E specification
                                                                      // 0x5F
    //  SET_REQ(                  WriteNoSnpPtlCleanSh            );  // 0x60 // TODO: not listed in Issue E specification
    //  SET_REQ(                  WriteNoSnpPtlCleanInv           );  // 0x61 // TODO: not listed in Issue E specification
    //  SET_REQ(                  WriteNoSnpPtlCleanShPerSep      );  // 0x62 // TODO: not listed in Issue E specification
                                                                      // 0x63
    //  SET_REQ(                  WriteUniquePtlCleanSh           );  // 0x64 // TODO: not listed in Issue E specification
                                                                      // 0x65
    //  SET_REQ(                  WriteUniquePtlCleanShPerSep     );  // 0x66 // TODO: not listed in Issue E specification
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

        
        // SNP transitions
        #define SET_SNP(type, opcode)  { \
            static const details::RNCohTrans _##opcode { \
                CacheStateTransitions::Initials::opcode, \
                CacheStateTransitions::Intermediates::details::TableG2(), \
                &CacheStateTransitions::Intermediates::opcode, \
                CacheStateTransition::Type::type \
            }; \
            snpDecoder[Opcodes::SNP::opcode].SetCompanion(&_##opcode); \
        }

        #define SET_SNP_EX(type, opcode, name) { \
            static const details::RNCohTrans _##name { \
                CacheStateTransitions::Initials::name, \
                CacheStateTransitions::Intermediates::details::TableG2(), \
                &CacheStateTransitions::Intermediates::name, \
                CacheStateTransition::Type::type \
            }; \
            snpDecoder[Opcodes::SNP::opcode].SetCompanion(&_##name); \
        }

    //  SET_SNP(General     , SnpLCrdReturn         );  // 0x00
        SET_SNP(Snoop       , SnpShared             );  // 0x01
        SET_SNP(Snoop       , SnpClean              );  // 0x02
        SET_SNP(Snoop       , SnpOnce               );  // 0x03
        SET_SNP(Snoop       , SnpNotSharedDirty     );  // 0x04
        SET_SNP(Snoop       , SnpUniqueStash        );  // 0x05
        SET_SNP(Snoop       , SnpMakeInvalidStash   );  // 0x06
        SET_SNP(Snoop       , SnpUnique             );  // 0x07
        SET_SNP(Snoop       , SnpCleanShared        );  // 0x08
        SET_SNP(Snoop       , SnpCleanInvalid       );  // 0x09
        SET_SNP(Snoop       , SnpMakeInvalid        );  // 0x0A
        SET_SNP(Snoop       , SnpStashUnique        );  // 0x0B
        SET_SNP(Snoop       , SnpStashShared        );  // 0x0C
    //                        SnpDVMOp                  // 0x0D
                                                        // 0x0E
                                                        // 0x0F
        SET_SNP(Snoop       , SnpQuery              );  // 0x10
        SET_SNP(SnoopForward, SnpSharedFwd          );  // 0x11
        SET_SNP(SnoopForward, SnpCleanFwd           );  // 0x12
        SET_SNP(SnoopForward, SnpOnceFwd            );  // 0x13
        SET_SNP(SnoopForward, SnpNotSharedDirtyFwd  );  // 0x14
        SET_SNP_EX(SnoopForward, SnpPreferUnique   , SnpPreferUnique_NoExcl   ); // 0x15
        SET_SNP_EX(SnoopForward, SnpPreferUniqueFwd, SnpPreferUniqueFwd_NoExcl); // 0x16
        SET_SNP(SnoopForward, SnpUniqueFwd          );  // 0x17
                                                        // 0x18
                                                        // 0x19
                                                        // 0x1A
                                                        // 0x1B
                                                        // 0x1C
                                                        // 0x1D
                                                        // 0x1E
                                                        // 0x1F

        #undef SET_SNP
        #undef SET_SNP_EX
        #undef SET_REQ
        #undef SET_REQ_EX
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool RNCacheStateMap<config, conn>::IsAccessed(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept
    {
        if (!seerEnabled)
            return false;

        auto iter = seerAccessedMap.find(addr >> ADDR_OFFSET_CACHE_MAP);

        if (iter == seerAccessedMap.end())
            return false;

        return iter->second.test(addr & ((1UL << ADDR_OFFSET_CACHE_MAP_SPAN) - 1));
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNCacheStateMap<config, conn>::SetAccessed(Flits::REQ<config, conn>::addr_t::value_type addr) noexcept
    {
        if (!seerEnabled)
            return;

        auto iter = seerAccessedMap.find(addr >> ADDR_OFFSET_CACHE_MAP);

        if (iter == seerAccessedMap.end())
        {
            std::bitset<SIZE_CACHE_MAP_SPAN> newBitset;
            newBitset.set(addr & ((1UL << ADDR_OFFSET_CACHE_MAP_SPAN) - 1));
            seerAccessedMap.emplace(addr >> ADDR_OFFSET_CACHE_MAP, std::move(newBitset));
        }
        else
        {
            iter->second.set(addr & ((1UL << ADDR_OFFSET_CACHE_MAP_SPAN) - 1));
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline CacheState RNCacheStateMap<config, conn>::EvaluateSilently(CacheState state) const noexcept
    {
        // silent transitions

        /* Cache eviction */
        if (enableSilentEviction)
        {
            if (state.UC || state.UCE || state.SC)
                state.I = true;
        }

        /* Local sharing */
        if (enableSilentSharing)
        {
            if (state.UC)
                state.SC = true;

            if (state.UD)
                state.SD = true;
        }

        /* Store */
        if (enableSilentStore)
        {
            if (state.UC || state.UCE || state.UDP)
                state.UD = true;

            if (state.UCE)
                state.UDP = true;
        }
        
        /* Cache Invalidate */
        if (enableSilentInvalidation)
        {
            if (state.UD || state.UDP)
                state.I = true;
        }
        //

        return state;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::pair<CacheState, bool> RNCacheStateMap<config, conn>::ExcavateWithSeer(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept
    {
        auto iter = stateMap.find(addr);

        if (!seerEnabled)
        {
            if (iter == stateMap.end())
                return { CacheStates::I, false };

            return { iter->second, false };
        }
        else
        {
            if (iter == stateMap.end())
            {
                // If seer mode is enabled, we put the transition on predicted path
                // when the address is not accessed yet in this seer section.
                if (IsAccessed(addr))
                    return { CacheStates::I, false };
                else
                    return { CacheStates::All, true };
            }

            return { iter->second, false };
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline std::pair<CacheState, bool> RNCacheStateMap<config, conn>::EvaluateWithSeer(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept
    {
        auto excavated = ExcavateWithSeer(addr);
        return { EvaluateSilently(excavated.first), excavated.second };
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNCacheStateMap<config, conn>::SetSilentEviction(bool enableSilentEviction) noexcept
    {
        this->enableSilentEviction = enableSilentEviction;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNCacheStateMap<config, conn>::SetSilentSharing(bool enableSilentSharing) noexcept
    {
        this->enableSilentSharing = enableSilentSharing;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNCacheStateMap<config, conn>::SetSilentStore(bool enableSilentStore) noexcept
    {
        this->enableSilentStore = enableSilentStore;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNCacheStateMap<config, conn>::SetSilentInvalidation(bool enableSilentInvalidation) noexcept
    {
        this->enableSilentInvalidation = enableSilentInvalidation;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool RNCacheStateMap<config, conn>::HasSilentEviction() const noexcept
    {
        return this->enableSilentEviction;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool RNCacheStateMap<config, conn>::HasSilentSharing() const noexcept
    {
        return this->enableSilentSharing;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool RNCacheStateMap<config, conn>::HasSilentStore() const noexcept
    {
        return this->enableSilentStore;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool RNCacheStateMap<config, conn>::HasSilentInvalidation() const noexcept
    {
        return this->enableSilentInvalidation;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void RNCacheStateMap<config, conn>::Set(Flits::REQ<config, conn>::addr_t::value_type addr, CacheState state) noexcept
    {
        stateMap[addr] = state;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline CacheState RNCacheStateMap<config, conn>::Get(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept
    {
        auto iter = stateMap.find(addr);
        return iter != stateMap.end() ? iter->second : CacheStates::None;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline CacheState RNCacheStateMap<config, conn>::Excavate(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept
    {
        return ExcavateWithSeer(addr).first;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline CacheState RNCacheStateMap<config, conn>::Evaluate(Flits::REQ<config, conn>::addr_t::value_type addr) const noexcept
    {
        return EvaluateWithSeer(addr).first;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNCacheStateMap<config, conn>::NextTXREQ(
        Flits::REQ<config, conn>::addr_t::value_type    addr,
        const Flits::REQ<config, conn>&                 flit) noexcept
    {
        // Decode REQ opcode
        const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::RNCohTrans*>& opcodeInfo = 
            reqDecoder.Decode(flit.Opcode());

        if (!opcodeInfo.IsValid()) // unknown opcode
            return XactDenial::DENIED_OPCODE;

        //
        const details::RNCohTrans* trans = opcodeInfo.GetCompanion();

        if (!trans)
                return XactDenial::DENIED_UNSUPPORTED_FEATURE;

        //
        const std::pair<CacheState, bool> initialState = EvaluateWithSeer(addr);

        // Initial state check
        CacheState actualInitialState = trans->initial & initialState.first;

        if (!actualInitialState)
            return XactDenial::DENIED_STATE_INITIAL;

        // Speculative path tracking
        if (initialState.second)
        {
            // TODO
        }

        // State update
        SetAccessed(addr);

        stateMap[addr] = actualInitialState;
        
        //
        return XactDenial::ACCEPTED;
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNCacheStateMap<config, conn>::NextTXRSP(
        Flits::REQ<config, conn>::addr_t::value_type    addr,
        const Xaction<config, conn>&                    xaction,
        const Flits::RSP<config, conn>&                 flit) noexcept
    {
        // Decode transition set from TXREQ/RXSNP opcode
        const details::RNCohTrans* trans;

        if (xaction.GetFirst().IsREQ())
        {
            /* 
            * *NOTICE: No state transition possible on TXRSP of RN REQ,
            *          ignore non-state-related TXRSPs.
            */
            return XactDenial::ACCEPTED;

            /*
            // Decode REQ opcode
            const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const CohTrans&>& opcodeInfo
                = reqDecoder.Decode(xaction.GetFirst().flit.req.Opcode());

            if (!opcodeInfo.IsValid()) // unknown opcode
                return XactDenial::DENIED_REQ_OPCODE;

            trans = &opcodeInfo.GetCompanion();

            if (!trans)
                return XactDenial::DENIED_UNSUPPORTED_FEATURE;
            */
        }
        else
        {
            // Decode SNP opcode
            const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, const details::RNCohTrans*>& opcodeInfo
                = snpDecoder.Decode(xaction.GetFirst().flit.snp.Opcode());

            if (!opcodeInfo.IsValid()) // unknown opcode
                return XactDenial::DENIED_SNP_OPCODE;

            bool retToSrc = xaction.GetFirst().flit.snp.RetToSrc();

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return XactDenial::DENIED_UNSUPPORTED_FEATURE;

            const CacheStateTransitions::Intermediates::details::TableG0* g0 = nullptr;
            const CacheStateTransitions::Intermediates::details::TableG1* g1 = nullptr;

            bool fwded = false;

            if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::SnpX)
            {
                const CacheStateTransitions::Intermediates::TablesSnpX* tables 
                    = static_cast<const CacheStateTransitions::Intermediates::TablesSnpX*>(trans->tables);

                if (flit.Opcode() == Opcodes::RSP::SnpResp)
                {
                    if (!retToSrc)
                        g0 = &tables->GSnpResp_RetToSrc_0();
                    else
                        g0 = &tables->GSnpResp_RetToSrc_1();
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::SnpXFwd)
            {
                const CacheStateTransitions::Intermediates::TablesSnpXFwd* tables 
                    = static_cast<const CacheStateTransitions::Intermediates::TablesSnpXFwd*>(trans->tables);

                if (flit.Opcode() == Opcodes::RSP::SnpResp)
                {
                    g0 = &tables->GSnpResp(retToSrc);
                }
                else if (flit.Opcode() == Opcodes::RSP::SnpRespFwded)
                {
                    g0 = &tables->G0SnpRespFwded(retToSrc);
                    g1 = &tables->G1SnpRespFwded(retToSrc);

                    fwded = true;
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;
            }
            else
                return XactDenial::DENIED_SNP_OPCODE;

            if (!g0 && !g1)
                return XactDenial::DENIED_SNP_OPCODE;

            //
            CacheResp resp = fwded ? CacheResp::FromSnpRespFwded(flit.Resp()) 
                                   : CacheResp::FromSnpResp(flit.Resp());

            CacheState nextState;

            //
            std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

            //
            if (g1)
            {
                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                    return XactDenial::DENIED_STATE_RESP_SNPRESPFWDED;

                CacheResp xf
                    = CacheStateTransitions::Intermediates::details::ProductG1(prevState.first, *g1, resp);

                if (!xf) // TODO: Existence result of G1 should be consistent with G0, replace with assertion here.
                    return XactDenial::DENIED_STATE_RESP_SNPRESPFWDED;

                CacheResp fwdState = CacheResp::FromSnpRespFwded(flit.Resp());

                if (!(fwdState & xf))
                    return XactDenial::DENIED_STATE_FWD_SNPRESPFWDED;

                nextState = xs;
            }
            else if (g0)
            {
                CacheState xs 
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);
                
                if (!xs)
                {
                    return fwded ? XactDenial::DENIED_STATE_RESP_SNPRESPFWDED 
                                 : XactDenial::DENIED_STATE_RESP_SNPRESP;
                }

                nextState = xs;
            }

            if (xaction.GetFirst().flit.snp.DoNotGoToSD())
            {
                // skip checks for SnpOnce & SnpOnceFwd
                //  - which always allows to go to SD regardless of DoNotGoToSD
                if (xaction.GetFirst().flit.snp.Opcode() != Opcodes::SNP::SnpOnce
                 && xaction.GetFirst().flit.snp.Opcode() != Opcodes::SNP::SnpOnceFwd)
                {
                    if (nextState.SD)
                        return XactDenial::DENIED_STATE_DONOTGOTOSD;
                }
            }
            
            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = nextState;

            //
            return XactDenial::ACCEPTED;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNCacheStateMap<config, conn>::NextTXDAT(
        Flits::REQ<config, conn>::addr_t::value_type    addr,
        const Xaction<config, conn>&                    xaction,
        const Flits::DAT<config, conn>&                 flit) noexcept
    {
        // Decode transition set from TXREQ/RXSNP opcode
        const details::RNCohTrans* trans;

        if (xaction.GetFirst().IsREQ())
        {
            // Decode REQ opcode
            const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::RNCohTrans*>& opcodeInfo
                = reqDecoder.Decode(xaction.GetFirst().flit.req.Opcode());

            if (!opcodeInfo.IsValid()) // unknown opcode
                return XactDenial::DENIED_REQ_OPCODE;

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return XactDenial::DENIED_UNSUPPORTED_FEATURE;

            //
            CacheState nextState;

            //
            std::pair<CacheState, bool> prevState; // lazy evaluation

            // check for multi-data-beat repeat
            const FiredResponseFlit<config, conn>* firstDAT = xaction.GetFirstDAT({ flit.Opcode() });
            
            if (firstDAT && firstDAT->flit.dat.DataID() != flit.DataID())
            {
                if (firstDAT->flit.dat.Resp() != flit.Resp())
                    return XactDenial::DENIED_STATE_MISMATCHED_REPEAT;

                return XactDenial::ACCEPTED;
            }

            // TXDAT Opcodes with state transitions or checks under REQ subsequence:
            //  CopyBackWriteData, NonCopyBackWriteData*, NCBWrDataCompAck*
            //
            // * checks for I state only
            if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Write)
            {
                // TXDAT Opcodes under Write Transactions:
                //  CopyBackWrData

                if (flit.Opcode() == Opcodes::DAT::CopyBackWrData)
                {
                    const CacheStateTransitions::Intermediates::details::TableG0* g0 
                        = &static_cast<const CacheStateTransitions::Intermediates::TablesWrite*>(trans->tables)->GCopyBackWrData();

                    prevState = EvaluateWithSeer(addr);

                    CacheResp resp = CacheResp::FromCopyBackWrData(flit.Resp());

                    CacheState xs
                        = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                    if (!xs)
                        return XactDenial::DENIED_STATE_COPYBACKWRDATA;

                    nextState = xs;
                }
                else
                    return XactDenial::DENIED_DAT_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::WriteNonCopyBack)
            {
                // TXDAT Opcodes under Non-CopyBack Write Transactions:
                //  NonCopyBackWrData, NCBWrDataCompAck

                if (flit.Opcode() == Opcodes::DAT::NonCopyBackWrData
                 || flit.Opcode() == Opcodes::DAT::NCBWrDataCompAck)
                {
                    prevState = EvaluateWithSeer(addr);

                    if (!(prevState.first & CacheStates::I))
                        return XactDenial::DENIED_STATE_NONCOPYBACKWRDATA;

                    nextState = CacheStates::I;
                }
                else if (flit.Opcode() == Opcodes::DAT::WriteDataCancel)
                    return XactDenial::ACCEPTED;
                else
                    return XactDenial::DENIED_DAT_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Atomic)
            {
                // TXDAT Opcodes under Atomic Transactions:
                //  NonCopyBackWrData

                if (flit.Opcode() == Opcodes::DAT::NonCopyBackWrData)
                {
                    return XactDenial::ACCEPTED;
                }
                else
                    return XactDenial::DENIED_DAT_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Read
                  || trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::MakeReadUnique
                  || trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Dataless)
            {
                return XactDenial::DENIED_DAT_OPCODE;
            }
            else
                return XactDenial::DENIED_REQ_OPCODE;

            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = nextState;

            //
            return XactDenial::ACCEPTED;
        }
        else
        {
            // Decode SNP opcode
            const Opcodes::OpcodeInfo<typename Flits::SNP<config>::opcode_t, const details::RNCohTrans*>& opcodeInfo
                = snpDecoder.Decode(xaction.GetFirst().flit.snp.Opcode());

            if (!opcodeInfo.IsValid()) // unknown opcode
                return XactDenial::DENIED_SNP_OPCODE;

            bool retToSrc = xaction.GetFirst().flit.snp.RetToSrc();

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return XactDenial::DENIED_UNSUPPORTED_FEATURE;

            // check for multi-data-beat repeat
            const FiredResponseFlit<config, conn>* firstDAT = xaction.GetFirstDAT({ flit.Opcode() });
            
            if (firstDAT && firstDAT->flit.dat.DataID() != flit.DataID())
            {
                if (firstDAT->flit.dat.Resp() != flit.Resp())
                    return XactDenial::DENIED_STATE_MISMATCHED_REPEAT;

                if (flit.Opcode() == Opcodes::DAT::SnpRespDataFwded)
                    if (firstDAT->flit.dat.FwdState() != flit.FwdState())
                        return XactDenial::DENIED_STATE_FWD_MISMATCHED_REPEAT;

                return XactDenial::ACCEPTED;
            }

            //
            const CacheStateTransitions::Intermediates::details::TableG0* g0 = nullptr;
            const CacheStateTransitions::Intermediates::details::TableG1* g1 = nullptr;

            bool fwded = false;

            if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::SnpX)
            {
                const CacheStateTransitions::Intermediates::TablesSnpX* tables
                    = static_cast<const CacheStateTransitions::Intermediates::TablesSnpX*>(trans->tables);

                if (flit.Opcode() == Opcodes::DAT::SnpRespData)
                {
                    g0 = &tables->GSnpRespData(retToSrc);
                }
                else if (flit.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                {
                    g0 = &tables->GSnpRespDataPtl(retToSrc);
                }
                else
                    return XactDenial::DENIED_DAT_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::SnpXFwd)
            {
                const CacheStateTransitions::Intermediates::TablesSnpXFwd* tables
                    = static_cast<const CacheStateTransitions::Intermediates::TablesSnpXFwd*>(trans->tables);

                if (flit.Opcode() == Opcodes::DAT::SnpRespData)
                {
                    g0 = &tables->GSnpRespData(retToSrc);
                }
                else if (flit.Opcode() == Opcodes::DAT::SnpRespDataPtl)
                {
                    g0 = &tables->GSnpRespDataPtl(retToSrc);
                }
                else if (flit.Opcode() == Opcodes::DAT::SnpRespDataFwded)
                {
                    g0 = &tables->G0SnpRespDataFwded(retToSrc);
                    g1 = &tables->G1SnpRespDataFwded(retToSrc);

                    fwded = true;
                }
                else
                    return XactDenial::DENIED_DAT_OPCODE;
            }
            else
                return XactDenial::DENIED_SNP_OPCODE;

            if (!g0 && !g1)
                return XactDenial::DENIED_SNP_OPCODE;

            //
            CacheResp resp = fwded ? CacheResp::FromSnpRespDataFwded(flit.Resp()) : (
                flit.Opcode() == Opcodes::DAT::SnpRespDataPtl ? CacheResp::FromSnpRespDataPtl(flit.Resp())
                                                              : CacheResp::FromSnpRespData(flit.Resp())
            );

            CacheState nextState;

            //
            std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

            //
            if (g1)
            {
                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                    return XactDenial::DENIED_STATE_RESP_SNPRESPDATAFWDED;

                CacheResp xf
                    = CacheStateTransitions::Intermediates::details::ProductG1(prevState.first, *g1, resp);

                if (!xf) // TODO: Existence result of G1 should be consistent with G0, replace with assertion here.
                    return XactDenial::DENIED_STATE_RESP_SNPRESPDATAFWDED;

                CacheResp fwdState = CacheResp::FromSnpRespDataFwded(flit.Resp());

                if (!(fwdState & xf))
                    return XactDenial::DENIED_STATE_FWD_SNPRESPDATAFWDED;

                nextState = xs;
            }
            else if (g0)
            {
                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                {
                    return fwded ? XactDenial::DENIED_STATE_RESP_SNPRESPDATAFWDED
                                 : XactDenial::DENIED_STATE_RESP_SNPRESPDATA;
                }

                nextState = xs;
            }

            if (xaction.GetFirst().flit.snp.DoNotGoToSD())
            {
                // skip checks for SnpOnce & SnpOnceFwd
                //  - which always allows to go to SD regardless of DoNotGoToSD
                if (xaction.GetFirst().flit.snp.Opcode() != Opcodes::SNP::SnpOnce
                 && xaction.GetFirst().flit.snp.Opcode() != Opcodes::SNP::SnpOnceFwd)
                {
                    if (nextState.SD)
                        return XactDenial::DENIED_STATE_DONOTGOTOSD;
                }
            }

            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = nextState;

            //
            return XactDenial::ACCEPTED;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNCacheStateMap<config, conn>::NextRXRSP(
        Flits::REQ<config, conn>::addr_t::value_type    addr,
        const Xaction<config, conn>&                    xaction,
        const Flits::RSP<config, conn>&                 flit) noexcept
    {
        // Decode transition set from TXREQ/RXSNP opcode
        const details::RNCohTrans* trans;

        if (xaction.GetFirst().IsREQ())
        {
            // Decode REQ opcode
            const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::RNCohTrans*>& opcodeInfo
                = reqDecoder.Decode(xaction.GetFirst().flit.req.Opcode());
            
            if (!opcodeInfo.IsValid()) // unknown opcode
                return XactDenial::DENIED_REQ_OPCODE;

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return XactDenial::DENIED_UNSUPPORTED_FEATURE;

            //
            CacheState nextState;

            //
            std::pair<CacheState, bool> prevState; // lazy evaluation

            // 
            if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Read)
            {
                // RXRSP Opcodes under Read Transactions:
                //  RespSepData*
                //
                // * no state-related transitions

                if (flit.Opcode() == Opcodes::RSP::RespSepData)
                {
                    return XactDenial::ACCEPTED;
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::MakeReadUnique)
            {
                // RXRSP Opcodes under MakeReadUnique Transactions:
                //  Comp, RespSepData*
                //
                // * no state-related transitions

                if (flit.Opcode() == Opcodes::RSP::Comp)
                {
                    const CacheStateTransitions::Intermediates::details::TableG0* g0
                        = &static_cast<const CacheStateTransitions::Intermediates::TablesMakeReadUnique*>(trans->tables)->GComp();

                    prevState = EvaluateWithSeer(addr);

                    CacheResp resp = CacheResp::FromComp(flit.Resp());

                    CacheState xs
                        = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                    if (!xs)
                        return XactDenial::DENIED_STATE_COMP;

                    nextState = xs;
                }
                else if (flit.Opcode() == Opcodes::RSP::RespSepData)
                {
                    return XactDenial::ACCEPTED;
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Dataless)
            {
                // RXRSP Opcodes under Dataless Transactions:
                //  Comp, CompPersist, CompStashDone*[1], StashDone*[2], Persist*[2]
                //
                // *[1]: checkers for I state only
                // *[2]: no state-related transitions

                if (flit.Opcode() == Opcodes::RSP::Comp
                 || flit.Opcode() == Opcodes::RSP::CompPersist)
                {
                    const CacheStateTransitions::Intermediates::details::TableG0* g0
                        = &static_cast<const CacheStateTransitions::Intermediates::TablesDataless*>(trans->tables)->GComp();

                    prevState = EvaluateWithSeer(addr);

                    CacheResp resp = CacheResp::FromComp(flit.Resp());

                    CacheState xs
                        = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                    if (!xs)
                        return XactDenial::DENIED_STATE_COMP;

                    nextState = xs;
                }
                else if (flit.Opcode() == Opcodes::RSP::CompStashDone)
                {
                    prevState = EvaluateWithSeer(addr);

                    if (!(prevState.first & CacheStates::I))
                        return XactDenial::DENIED_STATE_COMP;

                    nextState = CacheStates::I;
                }
                else if (flit.Opcode() == Opcodes::RSP::StashDone)
                {
                    return XactDenial::ACCEPTED;
                }
                else if (flit.Opcode() == Opcodes::RSP::Persist)
                {
                    return XactDenial::ACCEPTED;
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::WriteNonCopyBack)
            {
                // RXRSP Opcodes under Non-CopyBack Write Transactions:
                //  Comp*, DBIDResp*, CompDBIDResp*
                //
                // * checkers for I state immediately

                if (flit.Opcode() == Opcodes::RSP::Comp
                 || flit.Opcode() == Opcodes::RSP::DBIDResp
                 || flit.Opcode() == Opcodes::RSP::CompDBIDResp)
                {
                    prevState = EvaluateWithSeer(addr);

                    if (!(prevState.first & CacheStates::I))
                        return XactDenial::DENIED_STATE_COMP;

                    nextState = CacheStates::I;
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Write)
            {
                // RXRSP Opcodes under CopyBack Write Transactions:
                //  Comp, CompDBIDResp*
                //
                // * no state-related transitions
                //
                if (flit.Opcode() == Opcodes::RSP::Comp)
                {
                    // NOTICE: Prior to Issue G, Comp response was only possible for WriteEvictOrEvict
                    if (opcodeInfo.GetOpcode() != Opcodes::REQ::WriteEvictOrEvict)
                        return XactDenial::DENIED_RSP_OPCODE;

                    nextState = CacheStates::I;
                }
                else if (flit.Opcode() == Opcodes::RSP::CompDBIDResp)
                {
                    return XactDenial::ACCEPTED;
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Atomic)
            {
                // RXRSP Opcodes under Atomic Transactions:
                //  Comp, CompDBIDResp

                if (flit.Opcode() == Opcodes::RSP::Comp
                 || flit.Opcode() == Opcodes::RSP::CompDBIDResp)
                {
                    const CacheStateTransitions::Intermediates::details::TableG0* g0
                        = &static_cast<const CacheStateTransitions::Intermediates::TablesAtomic*>(trans->tables)->GComp();
                
                    prevState = EvaluateWithSeer(addr);

                    CacheResp resp = CacheResp::FromComp(flit.Resp());

                    CacheState xs
                        = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                    if (!xs)
                        return XactDenial::DENIED_STATE_COMP;

                    nextState = xs;
                }
                else
                    return XactDenial::DENIED_RSP_OPCODE;                
            }
            else
                return XactDenial::DENIED_REQ_OPCODE;

            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = nextState;

            //
            return XactDenial::ACCEPTED;
        }
        else
        {
            /*
            * *NOTICE: No RN RXRSP possible on SNP transactions.
            */
            return XactDenial::DENIED_RSP_OPCODE;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNCacheStateMap<config, conn>::NextRXDAT(
        Flits::REQ<config, conn>::addr_t::value_type    addr,
        const Xaction<config, conn>&                    xaction,
        const Flits::DAT<config, conn>&                 flit) noexcept
    {
        // Decode transition set from TXREQ/RXSNP opcode
        const details::RNCohTrans* trans;

        if (xaction.GetFirst().IsREQ())
        {
            // Decode REQ opcode
            const Opcodes::OpcodeInfo<typename Flits::REQ<config>::opcode_t, const details::RNCohTrans*>& opcodeInfo
                = reqDecoder.Decode(xaction.GetFirst().flit.req.Opcode());
            
            if (!opcodeInfo.IsValid()) // unknown opcode
                return XactDenial::DENIED_REQ_OPCODE;

            trans = opcodeInfo.GetCompanion();

            if (!trans)
                return XactDenial::DENIED_UNSUPPORTED_FEATURE;

            //
            CacheState nextState;

            //
            std::pair<CacheState, bool> prevState; // lazy evaluation

            // check for multi-data-beat repeat
            const FiredResponseFlit<config, conn>* firstDAT = xaction.GetFirstDAT({ flit.Opcode() });
            
            if (firstDAT && firstDAT->flit.dat.DataID() != flit.DataID())
            {
                if (firstDAT->flit.dat.Resp() != flit.Resp())
                    return XactDenial::DENIED_STATE_MISMATCHED_REPEAT;

                return XactDenial::ACCEPTED;
            }

            //
            if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Read)
            {
                // RXDAT Opcodes under Read Transactions:
                //  CompData, DataSepResp

                const CacheStateTransitions::Intermediates::details::TableG0* g0 = nullptr;
                CacheResp resp;

                if (flit.Opcode() == Opcodes::DAT::CompData)
                {
                    g0 = &static_cast<const CacheStateTransitions::Intermediates::TablesRead*>(trans->tables)->GCompData();
                    resp = CacheResp::FromCompData(flit.Resp());
                }
                else if (flit.Opcode() == Opcodes::DAT::DataSepResp)
                {
                    g0 = &static_cast<const CacheStateTransitions::Intermediates::TablesRead*>(trans->tables)->GDataSepResp();
                    resp = CacheResp::FromDataSepResp(flit.Resp());
                }
                else
                    return XactDenial::DENIED_DAT_OPCODE;

                prevState = EvaluateWithSeer(addr);

                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                {
                    return flit.Opcode() == Opcodes::DAT::CompData ? XactDenial::DENIED_STATE_COMPDATA
                                                                   : XactDenial::DENIED_STATE_DATASEPRESP;
                }

                nextState = xs;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::MakeReadUnique)
            {
                // RXDAT Opcodes under MakeReadUnique Transactions:
                //  CompData, DataSepResp

                const CacheStateTransitions::Intermediates::TablesMakeReadUnique* tables = nullptr;

                if (xaction.GetFirst().flit.req.Excl())
                    tables = &CacheStateTransitions::Intermediates::MakeReadUnique_Excl;
                else
                    tables = &CacheStateTransitions::Intermediates::MakeReadUnique;

                const CacheStateTransitions::Intermediates::details::TableG0* g0 = nullptr;
                CacheResp resp;

                if (flit.Opcode() == Opcodes::DAT::CompData)
                {
                    g0 = &tables->GCompData();
                    resp = CacheResp::FromCompData(flit.Resp());
                }
                else if (flit.Opcode() == Opcodes::DAT::DataSepResp)
                {
                    g0 = &tables->GDataSepResp();
                    resp = CacheResp::FromDataSepResp(flit.Resp());
                }
                else
                    return XactDenial::DENIED_DAT_OPCODE;

                prevState = EvaluateWithSeer(addr);

                CacheState xs
                    = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                if (!xs)
                {
                    return flit.Opcode() == Opcodes::DAT::CompData ? XactDenial::DENIED_STATE_COMPDATA
                                                                   : XactDenial::DENIED_STATE_DATASEPRESP;
                }
                
                nextState = xs;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Dataless)
            {
                return XactDenial::DENIED_DAT_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::WriteNonCopyBack)
            {
                return XactDenial::DENIED_DAT_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Write)
            {
                return XactDenial::DENIED_DAT_OPCODE;
            }
            else if (trans->tables->type == CacheStateTransitions::Intermediates::Tables::Type::Atomic)
            {
                // RXDAT Opcodes under Atomic Transactions:
                //  CompData

                if (flit.Opcode() == Opcodes::DAT::CompData)
                {
                    const CacheStateTransitions::Intermediates::details::TableG0* g0
                        = &static_cast<const CacheStateTransitions::Intermediates::TablesAtomic*>(trans->tables)->GCompData();
                    
                    prevState = EvaluateWithSeer(addr);

                    CacheResp resp = CacheResp::FromCompData(flit.Resp());

                    CacheState xs
                        = CacheStateTransitions::Intermediates::details::ProductG0(prevState.first, *g0, resp);

                    if (!xs)
                        return XactDenial::DENIED_STATE_COMPDATA;

                    nextState = xs;
                }
                else
                    return XactDenial::DENIED_DAT_OPCODE;
            }
            else
                return XactDenial::DENIED_REQ_OPCODE;
            
            // Speculative path tracking
            if (prevState.second)
            {
                // TODO
            }

            // State update
            SetAccessed(addr);

            stateMap[addr] = nextState;

            //
            return XactDenial::ACCEPTED;
        }
        else
        {
            /*
            * *NOTICE: No RN RXDAT possible on SNP transactions.
            */
            return XactDenial::DENIED_RSP_OPCODE;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNCacheStateMap<config, conn>::NextRXSNP(
        Flits::REQ<config, conn>::addr_t::value_type    addr,
        const Flits::SNP<config, conn>&                 flit) noexcept
    {
        return XactDenial::ACCEPTED;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RNCacheStateMap<config, conn>::Transfer(
        Flits::REQ<config, conn>::addr_t::value_type    addr,
        CacheState                                      state,
        const Xaction<config, conn>*                    nestingXaction) noexcept
    {
        if (!nestingXaction)
        {
            Set(addr, state);
            return XactDenial::ACCEPTED;
        }
        else
        {
            // Decode transition set from TXREQ/RXSNP opcode
            const details::RNCohTrans* trans;

            if (nestingXaction->GetFirst().IsREQ())
            {
                // Decode REQ opcode
                const Opcodes::OpcodeInfo<typename Flits::REQ<config, conn>::opcode_t, const details::RNCohTrans*>& opcodeInfo
                    = reqDecoder.Decode(nestingXaction->GetFirst().flit.req.Opcode());

                if (!opcodeInfo.IsValid()) // unknown opcode
                    return XactDenial::DENIED_REQ_OPCODE;

                trans = opcodeInfo.GetCompanion();

                if (!trans)
                    return XactDenial::DENIED_UNSUPPORTED_FEATURE;
                
                //
                std::pair<CacheState, bool> prevState = EvaluateWithSeer(addr);

                CacheState xi
                    = CacheStateTransitions::Intermediates::details::ProductG2(prevState.first, trans->nested, state);
                
                if (!xi)
                    return XactDenial::DENIED_STATE_NESTED_TRANSFER;

                //
                Set(addr, xi);

                return XactDenial::ACCEPTED;
            }
            else
                return XactDenial::DENIED_NESTING_SNP;
        }
    }
}


#endif // __CHI__CHI_XACT_STATE_*

//#endif // __CHI__CHI_XACT_STATE
