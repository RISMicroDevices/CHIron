#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_XACT_DATALESS
#define __CCHI__CCHI_XACT_STATE__CST_XACT_DATALESS

#include "cst_base.hpp"


namespace CCHI {
    namespace Xact {
        namespace CacheStateTransitions {

            namespace Transitions {

                //  Table. Cache state transitions at the Requester for Dataless request transactions
                //  (spec: transactions/structure.md "无数据权限转移" :161-163;
                //         coherency_protocol/request_types.md :111-125)
                // ================================================================================
                //                      | Cache state   | Cache state  |
                // Request Type         | Initial       | Final        | Comp response (DnRSP)
                // ================================================================================
                // MakeUnique           | I             | UD           | Comp_UC
                //
                // D4: initial states are I, SC, UC, UD per request_types.md :117 (send-time
                //     matrix), overriding structure.md's I-only row. The Comp_UC response
                //     always lands on UD (taurus model: a line acquired from Invalid holds
                //     architecturally undefined content until the full-line overwrite lands).
                constexpr CacheStateTransition MakeUnique_to_UD = {
                    CacheStateTransition(I | SC | UC | UD, UD).TypeDataless()
                        .Comp(UC)
                };
                //
                constexpr std::array MakeUnique = {
                    MakeUnique_to_UD
                };
                // --------------------------------------------------------------------------------
                // *NOTICE: CleanUnique is currently a reserved alias of MakeUnique
                //          (request_types.md :105-106) with no dedicated REQ opcode encoding;
                //          it is intentionally not modeled here.
                //
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_XACT_DATALESS
