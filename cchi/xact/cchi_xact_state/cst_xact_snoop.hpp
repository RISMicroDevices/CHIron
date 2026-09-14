#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_XACT_SNOOP
#define __CCHI__CCHI_XACT_STATE__CST_XACT_SNOOP

#include "cst_base.hpp"


namespace CCHI {
    namespace Xact {
        namespace CacheStateTransitions {

            namespace Transitions {

                //  Table. Snoop cache state transitions and valid completion responses
                //  (spec: transactions/structure.md "Snoop" :306-348;
                //         coherency_protocol/request_types.md :252-272)
                //
                //  Row fields map to the spec table columns as:
                //      .Start(start)                       = 起始状态 (state at snoop arrival)
                //      CacheStateTransition(initial, ...)  = 回复前状态 (pre-reply state, may have
                //                                            been changed by nested evictions or
                //                                            writebacks - structure.md :348 note)
                //      CacheStateTransition(..., final)    = 最终状态 (state after the response)
                //      .SnpResp(...) / .SnpRespData(...)   = 回复 (UpRSP/UpDAT)
                //
                // D3: outgoing SnpRespData *_PD means the dirty data went home (PassDirty) and the
                //     final state is the corresponding clean variant:
                //         I_PD -> I, SC_PD -> SC, UC_PD -> UC.
                // ================================================================================
                //                      | Start         | Pre-reply   | Final | Snoop response
                // ================================================================================
                // SnpMakeInvalid       | UD            | UD,UC,SC,I  | I     | SnpResp_I
                //                      | UC            | UC,SC,I     | I     | SnpResp_I
                //                      | SC            | SC,I        | I     | SnpResp_I
                //                      | I             | I           | I     | SnpResp_I
                //
                // SnpMakeInvalid returns no data and discards Dirty copies
                // (request_types.md :254-257).
                constexpr CacheStateTransition SnpMakeInvalid_UD_to_I = {
                    CacheStateTransition(UD | UC | SC | I, I).TypeSnoop()
                        .Start(UD)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_UC_to_I = {
                    CacheStateTransition(UC | SC | I, I).TypeSnoop()
                        .Start(UC)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_SC_to_I = {
                    CacheStateTransition(SC | I, I).TypeSnoop()
                        .Start(SC)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .Start(I)
                        .SnpResp(I)
                };
                //
                constexpr std::array SnpMakeInvalid = {
                    SnpMakeInvalid_UD_to_I,
                    SnpMakeInvalid_UC_to_I,
                    SnpMakeInvalid_SC_to_I,
                    SnpMakeInvalid_I_to_I
                };
                // --------------------------------------------------------------------------------
                // SnpToInvalid         | UD            | UD          | I     | SnpRespData_I_PD
                //                      |               | UC,SC       | I     | SnpRespData_I
                //                      |               | UC,SC,I     | I     | SnpResp_I
                //                      | UC            | UC,SC       | I     | SnpRespData_I
                //                      |               | UC,SC,I     | I     | SnpResp_I
                //                      | SC            | SC          | I     | SnpRespData_I
                //                      |               | SC,I        | I     | SnpResp_I
                //                      | I             | I           | I     | SnpResp_I
                //
                // D3: SnpRespData_I_PD (dirty data returned) -> final I.
                constexpr CacheStateTransition SnpToInvalid_UD_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .Start(UD)
                        .SnpRespData(I_PD)              // D3
                };
                constexpr CacheStateTransition SnpToInvalid_UD_UCSC_to_I = {
                    CacheStateTransition(UC | SC, I).TypeSnoop()
                        .Start(UD)
                        .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpToInvalid_UD_UCSCI_to_I = {
                    CacheStateTransition(UC | SC | I, I).TypeSnoop()
                        .Start(UD)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToInvalid_UC_UCSC_to_I = {
                    CacheStateTransition(UC | SC, I).TypeSnoop()
                        .Start(UC)
                        .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpToInvalid_UC_UCSCI_to_I = {
                    CacheStateTransition(UC | SC | I, I).TypeSnoop()
                        .Start(UC)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToInvalid_SC_SC_to_I = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .Start(SC)
                        .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpToInvalid_SC_SCI_to_I = {
                    CacheStateTransition(SC | I, I).TypeSnoop()
                        .Start(SC)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToInvalid_I_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .Start(I)
                        .SnpResp(I)
                };
                //
                constexpr std::array SnpToInvalid = {
                    SnpToInvalid_UD_UD_to_I,
                    SnpToInvalid_UD_UCSC_to_I,
                    SnpToInvalid_UD_UCSCI_to_I,
                    SnpToInvalid_UC_UCSC_to_I,
                    SnpToInvalid_UC_UCSCI_to_I,
                    SnpToInvalid_SC_SC_to_I,
                    SnpToInvalid_SC_SCI_to_I,
                    SnpToInvalid_I_I_to_I
                };
                // --------------------------------------------------------------------------------
                // SnpToShared          | UD            | UD          | SC    | SnpRespData_SC_PD
                //                      |               |             | I     | SnpRespData_I_PD
                //                      | UD, UC        | UC,SC       | SC    | SnpRespData_SC,
                //                      |               |             |       | SnpResp_SC
                //                      |               |             | I     | SnpRespData_I,
                //                      |               |             |       | SnpResp_I
                //                      |               | I           | I     | SnpResp_I
                //                      | SC            | SC          | SC    | SnpRespData_SC,
                //                      |               |             |       | SnpResp_SC
                //                      |               |             | I     | SnpRespData_I,
                //                      |               |             |       | SnpResp_I
                //                      |               | I           | I     | SnpResp_I
                //                      | I             | I           | I     | SnpResp_I
                //
                // D3: SnpRespData_SC_PD -> final SC; SnpRespData_I_PD -> final I.
                // Final state must not be Unique (request_types.md :264-267).
                constexpr CacheStateTransition SnpToShared_UD_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .Start(UD)
                        .SnpRespData(SC_PD)             // D3
                };
                constexpr CacheStateTransition SnpToShared_UD_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .Start(UD)
                        .SnpRespData(I_PD)              // D3
                };
                constexpr CacheStateTransition SnpToShared_UDUC_UCSC_to_SC = {
                    CacheStateTransition(UC | SC, SC).TypeSnoop()
                        .Start(UD | UC)
                        .SnpRespData(SC)
                        .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpToShared_UDUC_UCSC_to_I = {
                    CacheStateTransition(UC | SC, I).TypeSnoop()
                        .Start(UD | UC)
                        .SnpRespData(I)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToShared_UDUC_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .Start(UD | UC)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToShared_SC_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .Start(SC)
                        .SnpRespData(SC)
                        .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpToShared_SC_SC_to_I = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .Start(SC)
                        .SnpRespData(I)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToShared_SC_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .Start(SC)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToShared_I_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .Start(I)
                        .SnpResp(I)
                };
                //
                constexpr std::array SnpToShared = {
                    SnpToShared_UD_UD_to_SC,
                    SnpToShared_UD_UD_to_I,
                    SnpToShared_UDUC_UCSC_to_SC,
                    SnpToShared_UDUC_UCSC_to_I,
                    SnpToShared_UDUC_I_to_I,
                    SnpToShared_SC_SC_to_SC,
                    SnpToShared_SC_SC_to_I,
                    SnpToShared_SC_I_to_I,
                    SnpToShared_I_I_to_I
                };
                // --------------------------------------------------------------------------------
                // SnpToClean           | UD            | UD          | UD, UC*| SnpRespData_UD_PD *
                //                      |               |             | SC     | SnpRespData_SC_PD
                //                      |               |             | I      | SnpRespData_I_PD
                //                      | UD, UC        | UC          | UC     | SnpRespData_UC,
                //                      |               |             |        | SnpResp_UC
                //                      |               |             | SC     | SnpRespData_SC,
                //                      |               |             |        | SnpResp_SC
                //                      |               |             | I      | SnpRespData_I,
                //                      |               |             |        | SnpResp_I
                //                      | UD, UC, SC    | SC          | SC     | SnpRespData_SC,
                //                      |               |             |        | SnpResp_SC
                //                      |               |             | I      | SnpRespData_I,
                //                      |               |             |        | SnpResp_I
                //                      | UD, UC, SC, I | I           | I      | SnpResp_I
                //
                // D3: the spec row "UD | UD | UD, UC | SnpRespData_UD_PD" (structure.md :333) is
                //     self-contradictory with request_types.md :269-272 (SnpToClean final must not
                //     be Dirty). It is authored here as wire UC_PD -> final UC, resolving in favor
                //     of request_types.md (dirty went home, final is the clean variant).
                //     SnpRespData_SC_PD -> final SC; SnpRespData_I_PD -> final I.
                constexpr CacheStateTransition SnpToClean_UD_UD_to_UC = {
                    CacheStateTransition(UD, UC).TypeSnoop()
                        .Start(UD)
                        .SnpRespData(UC_PD)             // D3
                };
                constexpr CacheStateTransition SnpToClean_UD_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .Start(UD)
                        .SnpRespData(SC_PD)             // D3
                };
                constexpr CacheStateTransition SnpToClean_UD_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .Start(UD)
                        .SnpRespData(I_PD)              // D3
                };
                constexpr CacheStateTransition SnpToClean_UDUC_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .Start(UD | UC)
                        .SnpRespData(UC)
                        .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpToClean_UDUC_UC_to_SC = {
                    CacheStateTransition(UC, SC).TypeSnoop()
                        .Start(UD | UC)
                        .SnpRespData(SC)
                        .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpToClean_UDUC_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .Start(UD | UC)
                        .SnpRespData(I)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToClean_UDUCSC_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .Start(UD | UC | SC)
                        .SnpRespData(SC)
                        .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpToClean_UDUCSC_SC_to_I = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .Start(UD | UC | SC)
                        .SnpRespData(I)
                        .SnpResp(I)
                };
                constexpr CacheStateTransition SnpToClean_Any_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .Start(UD | UC | SC | I)
                        .SnpResp(I)
                };
                //
                constexpr std::array SnpToClean = {
                    SnpToClean_UD_UD_to_UC,
                    SnpToClean_UD_UD_to_SC,
                    SnpToClean_UD_UD_to_I,
                    SnpToClean_UDUC_UC_to_UC,
                    SnpToClean_UDUC_UC_to_SC,
                    SnpToClean_UDUC_UC_to_I,
                    SnpToClean_UDUCSC_SC_to_SC,
                    SnpToClean_UDUCSC_SC_to_I,
                    SnpToClean_Any_I_to_I
                };
                //
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_XACT_SNOOP
