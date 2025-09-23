#pragma once

#include "cst_xact_read.hpp"
#include "cst_xact_dataless.hpp"
#include "cst_xact_write.hpp"
#include "cst_xact_atomic.hpp"
#include "cst_xact_snoop.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_CONSTEVAL_INITIALS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_CONSTEVAL_INITIALS))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_CONSTEVAL_INITIALS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_CONSTEVAL_INITIALS
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

			//
            namespace Initials {

                namespace details {

                    template<size_t N>
                    inline consteval CacheState MapReduceInitialsGeneral(const std::array<CacheStateTransition, N>& a) noexcept
                    {
                        std::array<CacheState, N> b;
                        std::transform(a.begin(), a.end(), b.begin(), [](CacheStateTransition x) { return x.initialExpectedState | x.initialPermittedState; });
                        return std::reduce(b.begin(), b.end(), CacheStates::None, std::bit_or<>());
                    }

                    template<size_t N>
                    inline consteval CacheState MapReduceInitialsMakeReadUnique(const std::array<CacheStateTransition, N>& a) noexcept
                    {
                        std::array<CacheState, N> b;
                        std::transform(a.begin(), a.end(), b.begin(), [](CacheStateTransition x) { return x.initialExpectedState; });
                        return std::reduce(b.begin(), b.end(), CacheStates::None, std::bit_or<>());
                    }

                    template<size_t N>
                    inline consteval CacheState MapReduceInitialsWrite(const std::array<CacheStateTransition, N>& a) noexcept
                    {
                        std::array<CacheState, N> b;
                        std::transform(a.begin(), a.end(), b.begin(), [](CacheStateTransition x) { return x.initialExpectedState; });
                        return std::reduce(b.begin(), b.end(), CacheStates::None, std::bit_or<>());
                    }
                }

                #define _MR_General(opcode)         details::MapReduceInitialsGeneral(Transitions::opcode)
                #define _MR_MakeReadUnique(opcode)  details::MapReduceInitialsMakeReadUnique(Transitions::opcode)
                #define _MR_Write(opcode)           details::MapReduceInitialsWrite(Transitions::opcode)

                // Initial states for Read request transactions
                constexpr CacheState ReadNoSnp                      = _MR_General(ReadNoSnp);
                constexpr CacheState ReadOnce                       = _MR_General(ReadOnce);
                constexpr CacheState ReadOnceCleanInvalid           = _MR_General(ReadOnceCleanInvalid);
                constexpr CacheState ReadOnceMakeInvalid            = _MR_General(ReadOnceMakeInvalid);
                constexpr CacheState ReadClean_Transfer             = _MR_General(ReadClean_Transfer);
                constexpr CacheState ReadClean                      = _MR_General(ReadClean);
                constexpr CacheState ReadNotSharedDirty             = _MR_General(ReadNotSharedDirty);
                constexpr CacheState ReadShared                     = _MR_General(ReadShared);
                constexpr CacheState ReadUnique                     = _MR_General(ReadUnique);
                constexpr CacheState ReadPreferUnique               = _MR_General(ReadPreferUnique);
                constexpr CacheState MakeReadUnique                 = _MR_MakeReadUnique(MakeReadUnique);
                constexpr CacheState MakeReadUnique_Excl            = _MR_MakeReadUnique(MakeReadUnique_Excl);

                // Initial states for Dataless request transactions
                constexpr CacheState CleanUnique                    = _MR_General(CleanUnique);
                constexpr CacheState MakeUnique                     = _MR_General(MakeUnique);
                constexpr CacheState Evict                          = _MR_General(Evict);
                constexpr CacheState StashOnceUnique                = _MR_General(StashOnceUnique);
                constexpr CacheState StashOnceSepUnique             = _MR_General(StashOnceSepUnique);
                constexpr CacheState StashOnceShared                = _MR_General(StashOnceShared);
                constexpr CacheState StashOnceSepShared             = _MR_General(StashOnceSepShared);
                constexpr CacheState CleanShared                    = _MR_General(CleanShared);
                constexpr CacheState CleanSharedPersist             = _MR_General(CleanSharedPersist);
                constexpr CacheState CleanSharedPersistSep          = _MR_General(CleanSharedPersistSep);
                constexpr CacheState CleanInvalid                   = _MR_General(CleanInvalid);
                constexpr CacheState MakeInvalid                    = _MR_General(MakeInvalid);

                // Initial states for Write request transactions
                constexpr CacheState WriteNoSnpPtl                  = _MR_Write(WriteNoSnpPtl);
                constexpr CacheState WriteNoSnpFull                 = _MR_Write(WriteNoSnpFull);
                constexpr CacheState WriteNoSnpZero                 = _MR_Write(WriteNoSnpZero);
                constexpr CacheState WriteUniquePtl                 = _MR_Write(WriteUniquePtl);
                constexpr CacheState WriteUniquePtlStash            = _MR_Write(WriteUniquePtlStash);
                constexpr CacheState WriteUniqueZero                = _MR_Write(WriteUniqueZero);
                constexpr CacheState WriteUniqueFull                = _MR_Write(WriteUniqueFull);
                constexpr CacheState WriteUniqueFullStash           = _MR_Write(WriteUniqueFullStash);
                constexpr CacheState WriteBackFull                  = _MR_Write(WriteBackFull);
                constexpr CacheState WriteBackPtl                   = _MR_Write(WriteBackPtl);
                constexpr CacheState WriteCleanFull                 = _MR_Write(WriteCleanFull);
                constexpr CacheState WriteEvictFull                 = _MR_Write(WriteEvictFull);
                constexpr CacheState WriteEvictOrEvict              = _MR_Write(WriteEvictOrEvict);

                // Initial states for Atomic request transactions
                constexpr CacheState AtomicStore                    = _MR_General(AtomicStore);
                constexpr CacheState AtomicLoad                     = _MR_General(AtomicLoad);
                constexpr CacheState AtomicSwap                     = _MR_General(AtomicSwap);
                constexpr CacheState AtomicCompare                  = _MR_General(AtomicCompare);

                #undef _MR_General
                #undef _MR_MakeReadUnique
                #undef _MR_Write
            }
		}
	}
/*
}
*/

#endif
