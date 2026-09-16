#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_CONSTEVAL_INITIALS
#define __CCHI__CCHI_XACT_STATE__CST_CONSTEVAL_INITIALS

#include "cst_xact_read.hpp"
#include "cst_xact_dataless.hpp"
#include "cst_xact_cmo.hpp"
#include "cst_xact_stash.hpp"
#include "cst_xact_write.hpp"
#include "cst_xact_snoop.hpp"


namespace CCHI {
    namespace Xact {
        namespace CacheStateTransitions {

            //
            namespace Initials {

                namespace details {

                    template<size_t N>
                    inline consteval CacheState MapReduceInitials(const std::array<CacheStateTransition, N>& a) noexcept
                    {
                        CacheState r = CacheStates::None;
                        for (const CacheStateTransition& x : a)
                            r = r | (x.initialSend ? x.initialSend : x.initial);
                        return r;
                    }

                    template<size_t N>
                    inline consteval CacheState MapReduceStartStates(const std::array<CacheStateTransition, N>& a) noexcept
                    {
                        CacheState r = CacheStates::None;
                        for (const CacheStateTransition& x : a)
                            r = r | x.startState;
                        return r;
                    }
                }

                #define _MR_Initials(opcode)    details::MapReduceInitials(Transitions::opcode)
                #define _MR_Starts(opcode)      details::MapReduceStartStates(Transitions::opcode)

                // Initial states for Read request transactions
                constexpr CacheState ReadNoSnp          = _MR_Initials(ReadNoSnp);
                constexpr CacheState ReadOnce           = _MR_Initials(ReadOnce);
                constexpr CacheState ReadShared         = _MR_Initials(ReadShared);
                constexpr CacheState ReadUnique         = _MR_Initials(ReadUnique);

                // Initial states for Dataless request transactions
                constexpr CacheState MakeUnique         = _MR_Initials(MakeUnique);

                // Initial states for CMO request transactions
                // *NOTICE: these are the send-time states (request_types.md CMO send matrix:
                //          all four states), carried by the rows' 'initialSend'; the rows'
                //          'initial'/'final' keep encoding the compliant at-CompCMO states
                //          for the G0 compliance check (D8, cst_xact_cmo.hpp).
                constexpr CacheState CleanShared        = _MR_Initials(CleanShared);
                constexpr CacheState CleanInvalid       = _MR_Initials(CleanInvalid);
                constexpr CacheState MakeInvalid        = _MR_Initials(MakeInvalid);

                // Initial states for Stash request transactions
                constexpr CacheState StashShared        = _MR_Initials(StashShared);
                constexpr CacheState StashUnique        = _MR_Initials(StashUnique);

                // Initial states for Write request transactions
                constexpr CacheState WriteNoSnpPtl      = _MR_Initials(WriteNoSnpPtl);
                constexpr CacheState WriteNoSnpFull     = _MR_Initials(WriteNoSnpFull);
                constexpr CacheState WriteUniquePtl     = _MR_Initials(WriteUniquePtl);
                constexpr CacheState WriteUniqueFull    = _MR_Initials(WriteUniqueFull);

                // Initial states for EVT channel transactions
                constexpr CacheState Evict              = _MR_Initials(Evict);
                constexpr CacheState WriteBackFull      = _MR_Initials(WriteBackFull);

                // Initial (snoop-arrival, "起始状态") states for Snoop transactions
                constexpr CacheState SnpMakeInvalid     = _MR_Starts(SnpMakeInvalid);
                constexpr CacheState SnpToInvalid       = _MR_Starts(SnpToInvalid);
                constexpr CacheState SnpToShared        = _MR_Starts(SnpToShared);
                constexpr CacheState SnpToClean         = _MR_Starts(SnpToClean);

                #undef _MR_Initials
                #undef _MR_Starts
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_CONSTEVAL_INITIALS
