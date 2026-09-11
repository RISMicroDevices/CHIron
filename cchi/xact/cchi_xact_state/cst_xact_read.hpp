#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_XACT_READ
#define __CCHI__CCHI_XACT_STATE__CST_XACT_READ

#include "cst_base.hpp"


namespace CCHI {
    namespace Xact {
        namespace CacheStateTransitions {

            namespace Transitions {

                //  Table. Cache state transitions at the Requester for Read request transactions
                //  (spec: transactions/structure.md "非 Cacheable 读" :19-22, "Cacheable 读" :69-85;
                //         coherency_protocol/request_types.md send/final state matrices)
                // ================================================================================
                //                      | Cache state   | Cache state  |
                // Request Type         | Initial       | Final        | CompData response (DnDAT)
                //                      |               |              | / Comp response (DnRSP)
                // ================================================================================
                // ReadNoSnp            | I             | I            | CompData_UC
                //                      |               |              | CompData_I
                constexpr CacheStateTransition ReadNoSnp_I_to_I = {
                    CacheStateTransition(I, I).TypeRead()
                        .CompData(I | UC)
                };
                //
                constexpr std::array ReadNoSnp = {
                    ReadNoSnp_I_to_I
                };
                // --------------------------------------------------------------------------------
                // ReadOnce             | I             | I            | CompData_UC
                //                      |               |              | CompData_I
                constexpr CacheStateTransition ReadOnce_I_to_I = {
                    CacheStateTransition(I, I).TypeRead()
                        .CompData(I | UC)
                };
                //
                constexpr std::array ReadOnce = {
                    ReadOnce_I_to_I
                };
                // --------------------------------------------------------------------------------
                // ReadShared           | I, SC         | SC           | CompData_SC
                //                      |               | UC           | CompData_UC
                //                      |               | UD           | CompData_UD_PD *
                //
                // D1: the spec's CompData_UD_PD is not encodable on the wire; it is authored here
                //     as wire UC_PD (matching the taurus model), landing on final UD.
                // D5: the spec's merged initial cell "I, SC" applies to all final-state rows.
                constexpr CacheStateTransition ReadShared_I_SC_to_SC = {
                    CacheStateTransition(I | SC, SC).TypeRead()
                        .CompData(SC)
                };
                constexpr CacheStateTransition ReadShared_I_SC_to_UC = {
                    CacheStateTransition(I | SC, UC).TypeRead()
                        .CompData(UC)
                };
                constexpr CacheStateTransition ReadShared_I_SC_to_UD = {
                    CacheStateTransition(I | SC, UD).TypeRead()
                        .CompData(UC_PD)    // D1
                };
                //
                constexpr std::array ReadShared = {
                    ReadShared_I_SC_to_SC,
                    ReadShared_I_SC_to_UC,
                    ReadShared_I_SC_to_UD
                };
                // --------------------------------------------------------------------------------
                // ReadUnique           | I             | UC           | CompData_UC
                //                      |               | UD           | CompData_UD_PD *
                //                      -------------------------------------------------------------------------------
                //                      | SC, UC        | UC           | CompData_UC,
                //                      |               |              | Comp_UC (ExpCompData = 0)
                //                      |               | UD           | CompData_UD_PD *
                //                      -------------------------------------------------------------------------------
                //                      | UD            | UD           | CompData_UC *,
                //                      |               |              | Comp_UC * (ExpCompData = 0),
                //                      |               |              | CompData_UD_PD *
                //
                // D1: CompData_UD_PD authored as wire UC_PD -> final UD (as above).
                // D5: the merged initial cells "SC, UC" apply to both final-state rows.
                // D6: ReadUnique from initial UD keeps UD on CompData_UC / Comp_UC / CompData_UC_PD:
                //     the returned data is stale and discarded by the Requester
                //     (structure.md :81-85 including the '*' note).
                constexpr CacheStateTransition ReadUnique_I_to_UC = {
                    CacheStateTransition(I, UC).TypeRead()
                        .CompData(UC)
                };
                constexpr CacheStateTransition ReadUnique_I_to_UD = {
                    CacheStateTransition(I, UD).TypeRead()
                        .CompData(UC_PD)    // D1
                };
                constexpr CacheStateTransition ReadUnique_SC_UC_to_UC = {
                    CacheStateTransition(SC | UC, UC).TypeRead()
                        .CompData(UC)
                        .Comp(UC)           // dataless Comp_UC path (ExpCompData = 0)
                };
                constexpr CacheStateTransition ReadUnique_SC_UC_to_UD = {
                    CacheStateTransition(SC | UC, UD).TypeRead()
                        .CompData(UC_PD)    // D1
                };
                constexpr CacheStateTransition ReadUnique_UD_to_UD = {
                    CacheStateTransition(UD, UD).TypeRead()     // D6
                        .CompData(UC | UC_PD)
                        .Comp(UC)           // D6: dataless Comp_UC path from UD keeps UD
                };
                //
                constexpr std::array ReadUnique = {
                    ReadUnique_I_to_UC,
                    ReadUnique_I_to_UD,
                    ReadUnique_SC_UC_to_UC,
                    ReadUnique_SC_UC_to_UD,
                    ReadUnique_UD_to_UD
                };
                //
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_XACT_READ
