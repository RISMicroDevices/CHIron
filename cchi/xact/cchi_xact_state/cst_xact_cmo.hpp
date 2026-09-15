#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_XACT_CMO
#define __CCHI__CCHI_XACT_STATE__CST_XACT_CMO

#include "cst_base.hpp"


namespace CCHI {
    namespace Xact {
        namespace CacheStateTransitions {

            namespace Transitions {

                //  Table. Cache state transitions at the Requester for CMO request transactions
                //  (spec: transactions/structure.md "缓存行维护（CMO）" :185-194;
                //         coherency_protocol/request_types.md :140-158)
                // ================================================================================
                //                      | Cache state   | Cache state  |
                // Request Type         | Initial       | Final        | CompCMO response (DnRSP)
                // ================================================================================
                // CleanShared          | I             | I            | CompCMO *
                //                      | SC            | SC           | CompCMO *
                //                      | UC            | UC           | CompCMO *
                //                      | UD            | UC *         | CompCMO *
                //
                // D8: CompCMO does not itself cause a Requester state transition: on CompCMO the
                //     Requester's state must already comply with the final-state requirement
                //     (structure.md :194 note - a non-complying line is first updated by a nested
                //     Snoop from the Completer). The rows therefore keep initial == final over
                //     the complying states only (CleanShared: I/SC/UC; UD is non-compliant), and
                //     the response Resp is stateless, so all encodings are accepted (CacheResps::All).
                //     The CMO's send-time state set is separate (request_types.md send matrix:
                //     all four states) and is carried by 'initialSend' - the line keeps its
                //     current state for the whole in-flight window and must answer snoops from
                //     it; the compliant-set keying here applies only at CompCMO.
                constexpr CacheStateTransition CleanShared_I_to_I = {
                    CacheStateTransition(I, I).TypeCMO()
                        .SendInitial(I | SC | UC | UD)
                        .CompCMO(CacheResps::All)
                };
                constexpr CacheStateTransition CleanShared_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeCMO()
                        .SendInitial(I | SC | UC | UD)
                        .CompCMO(CacheResps::All)
                };
                constexpr CacheStateTransition CleanShared_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeCMO()
                        .SendInitial(I | SC | UC | UD)
                        .CompCMO(CacheResps::All)
                };
                //
                constexpr std::array CleanShared = {
                    CleanShared_I_to_I,
                    CleanShared_SC_to_SC,
                    CleanShared_UC_to_UC
                };
                // --------------------------------------------------------------------------------
                // CleanInvalid         | I, SC, UC, UD (send-time)   | I *  | CompCMO *
                //
                // D8: at CompCMO time only I complies (final must be Invalid); initial == final
                //     over the complying states keys the G0 compliance check; the send-time
                //     set (all four states) is carried by 'initialSend'.
                constexpr CacheStateTransition CleanInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeCMO()
                        .SendInitial(I | SC | UC | UD)
                        .CompCMO(CacheResps::All)
                };
                //
                constexpr std::array CleanInvalid = {
                    CleanInvalid_I_to_I
                };
                // --------------------------------------------------------------------------------
                // MakeInvalid          | I, SC, UC, UD (send-time)   | I *  | CompCMO *
                //
                // D8: same as CleanInvalid.
                constexpr CacheStateTransition MakeInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeCMO()
                        .SendInitial(I | SC | UC | UD)
                        .CompCMO(CacheResps::All)
                };
                //
                constexpr std::array MakeInvalid = {
                    MakeInvalid_I_to_I
                };
                //
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_XACT_CMO
