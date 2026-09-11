#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_XACT_WRITE
#define __CCHI__CCHI_XACT_STATE__CST_XACT_WRITE

#include "cst_base.hpp"


namespace CCHI {
    namespace Xact {
        namespace CacheStateTransitions {

            namespace Transitions {

                //  Table. Cache state transitions at the Requester for Write request transactions
                //  (spec: transactions/structure.md "非 Cacheable 写" :246-251,
                //         "Cacheable 写" :272-275;
                //         coherency_protocol/request_types.md :196-240)
                // ================================================================================
                //                      | Cache state   | Cache state  |                  |
                // Request Type         | Initial       | Final        | Data (UpDAT)     | DnRSP responses
                // ================================================================================
                // WriteNoSnpPtl        | I             | I            | NonCopyBackWrData| DBIDResp + Comp,
                //                      |               |              |                  | CompDBIDResp
                //
                // D10: Non-CopyBack writes carry no cache-line state transition; the DnRSP
                //      responses (DBIDResp/Comp/CompDBIDResp) and NonCopyBackWrData have no
                //      state effect (see also D12). The requester must be (and stays) I - this
                //      is enforced structurally by CacheStateMap as an I-only hardcoded check.
                constexpr CacheStateTransition WriteNoSnpPtl_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteNoSnpPtl = {
                    WriteNoSnpPtl_I_to_I
                };
                // --------------------------------------------------------------------------------
                // WriteNoSnpFull       | I             | I            | NonCopyBackWrData| DBIDResp + Comp,
                //                      |               |              |                  | CompDBIDResp
                //
                // D10: as WriteNoSnpPtl.
                constexpr CacheStateTransition WriteNoSnpFull_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteNoSnpFull = {
                    WriteNoSnpFull_I_to_I
                };
                // --------------------------------------------------------------------------------
                // WriteUniquePtl       | I             | I            | NonCopyBackWrData| DBIDResp + Comp,
                //                      |               |              |                  | CompDBIDResp
                //
                // D10: as WriteNoSnpPtl.
                constexpr CacheStateTransition WriteUniquePtl_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteUniquePtl = {
                    WriteUniquePtl_I_to_I
                };
                // --------------------------------------------------------------------------------
                // WriteUniqueFull      | I             | I            | NonCopyBackWrData| DBIDResp + Comp,
                //                      |               |              |                  | CompDBIDResp
                //
                // D10: as WriteNoSnpPtl.
                constexpr CacheStateTransition WriteUniqueFull_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteUniqueFull = {
                    WriteUniqueFull_I_to_I
                };
                // --------------------------------------------------------------------------------

                //  Table. Cache state transitions at the Requester for EVT channel transactions
                //  (spec: transactions/structure.md "缓存行踢出" :101-103,
                //         "缓存行写回" :137-140;
                //         coherency_protocol/request_types.md :64-101)
                // ================================================================================
                //                      | Cache state   | Cache state  |                  |
                // Request Type         | Initial       | Final        | Data (UpDAT)     | DnRSP responses
                // ================================================================================
                // Evict                | I             | I            | -                | Comp
                //
                // D11: Evict is only allowed from I (a Clean copy is silently invalidated before
                //      the explicit Evict; see the silent-eviction toggle in CacheStateMap).
                // D12: Comp carries no state; its Resp field is assumed to be driven as I.
                constexpr CacheStateTransition Evict_I_to_I = {
                    CacheStateTransition(I, I).TypeEvict()
                        .Comp(I)
                };
                //
                constexpr std::array Evict = {
                    Evict_I_to_I
                };
                // --------------------------------------------------------------------------------
                // WriteBackFull        | UD            | I            | CopyBackWrData_UD_PD *
                //                      |               |              |                  | DBIDResp + Comp,
                //                      |               |              |                  | CompDBIDResp
                //
                // D2: the spec's CopyBackWrData_UD_PD is not encodable on the wire; the writeback
                //     data is authored as wire I_PD (matching the taurus model) landing on final
                //     I. Wire I is also accepted.
                // D11: WriteBackFull is only allowed from UD.
                // D12: DBIDResp/Comp/CompDBIDResp have no state effect; the UD -> I transition
                //      happens on CopyBackWrData.
                constexpr CacheStateTransition WriteBackFull_UD_to_I = {
                    CacheStateTransition(UD, I).TypeWriteBack()
                        .CopyBackWrData(I_PD | I)      // D2
                };
                //
                // D13: after acceptance (D11: UD only), the tracked UD -> I transition can be
                //      consumed early by a racing snoop response (SnpResp I) narrowing the
                //      tracked state to I before the writeback data beats drain. The data was
                //      pulled while UD, so late CopyBackWrData beats from I are legal
                //      completions of this transaction.
                // TODO: This should be combined with nesting tracking
                constexpr CacheStateTransition WriteBackFull_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteBack()
                        .CopyBackWrData(I_PD | I)
                };
                //
                constexpr std::array WriteBackFull = {
                    WriteBackFull_UD_to_I,
                    WriteBackFull_I_to_I
                };
                //
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_XACT_WRITE
