#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_XACT_STASH
#define __CCHI__CCHI_XACT_STATE__CST_XACT_STASH

#include "cst_base.hpp"


namespace CCHI {
    namespace Xact {
        namespace CacheStateTransitions {

            namespace Transitions {

                //  Table. Cache state transitions at the Requester for Stash request transactions
                //  (spec: transactions/structure.md "跨层预取" :214-223;
                //         coherency_protocol/request_types.md :168-184)
                // ================================================================================
                //                      | Cache state   | Cache state  |
                // Request Type         | Initial       | Final        | CompStash response (DnRSP)
                // ================================================================================
                // StashShared          | I             | I            | CompStash (ExpCompStash=1)
                //                      | SC            | SC           |
                //                      | UC            | UC           |
                //                      | UD            | UD           |
                //
                // D9: Stash never changes the Requester cache-line state; rows are no-change rows.
                //     CompStash carries prefetch feedback, not state, and its arrival does not
                //     mark stash completion (structure.md :209-210), so all Resp encodings are
                //     accepted (CacheResps::All).
                constexpr CacheStateTransition StashShared_I_to_I = {
                    CacheStateTransition(I, I).TypeStash()
                        .CompStash(CacheResps::All)
                };
                constexpr CacheStateTransition StashShared_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeStash()
                        .CompStash(CacheResps::All)
                };
                constexpr CacheStateTransition StashShared_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeStash()
                        .CompStash(CacheResps::All)
                };
                constexpr CacheStateTransition StashShared_UD_to_UD = {
                    CacheStateTransition(UD, UD).TypeStash()
                        .CompStash(CacheResps::All)
                };
                //
                constexpr std::array StashShared = {
                    StashShared_I_to_I,
                    StashShared_SC_to_SC,
                    StashShared_UC_to_UC,
                    StashShared_UD_to_UD
                };
                // --------------------------------------------------------------------------------
                // StashUnique          | I             | I            | CompStash (ExpCompStash=1)
                //                      | SC            | SC           |
                //                      | UC            | UC           |
                //                      | UD            | UD           |
                //
                // D9: same as StashShared.
                constexpr CacheStateTransition StashUnique_I_to_I = {
                    CacheStateTransition(I, I).TypeStash()
                        .CompStash(CacheResps::All)
                };
                constexpr CacheStateTransition StashUnique_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeStash()
                        .CompStash(CacheResps::All)
                };
                constexpr CacheStateTransition StashUnique_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeStash()
                        .CompStash(CacheResps::All)
                };
                constexpr CacheStateTransition StashUnique_UD_to_UD = {
                    CacheStateTransition(UD, UD).TypeStash()
                        .CompStash(CacheResps::All)
                };
                //
                constexpr std::array StashUnique = {
                    StashUnique_I_to_I,
                    StashUnique_SC_to_SC,
                    StashUnique_UC_to_UC,
                    StashUnique_UD_to_UD
                };
                //
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_XACT_STASH
