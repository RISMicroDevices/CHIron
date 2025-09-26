#pragma once

#include "cst_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_XACT_READ)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_XACT_READ))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_XACT_READ
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_XACT_READ
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

			namespace Transitions {

				//                        Table. Cache state transitions at the Requester for Read request transactions
                // ====================================================================================================
                //                      | Cache state   | Others     |       |                |
                // Request Type         | Initial       | Permitted  | Final | Comp response  | Seperate responses
                //                      | Expected      |            |       |                |
                // ====================================================================================================
                // ReadNoSnp            | I             | -          | I     | CompData_UC,   | RespSepData +
                //                      |               |            |       | CompData_I     | DataSepResp_UC
                constexpr CacheStateTransition ReadNoSnp_I = {
                    CacheStateTransition(I, I).TypeRead()
                        .CompData(I | UC)
                        .DataSepResp(UC)
                };
                //
                constexpr std::array ReadNoSnp = {
                    ReadNoSnp_I
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadOnce             | I             | -          | I     | CompData_UC,   | RespSepData + 
                //                      |               |            |       | CompData_I     | DataSepResp_UC
                constexpr CacheStateTransition ReadOnce_I_to_I = {
                    CacheStateTransition(I, I).TypeRead()
                        .CompData(I | UC)
                        .DataSepResp(UC)
                };
                //
                constexpr std::array ReadOnce = {
                    ReadOnce_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadOnceCleanInvalid | I             | -          | I     | CompData_UC,   | RespSepData + 
                //                      |               |            |       | CompData_I     | DataSepResp_UC
                constexpr CacheStateTransition ReadOnceCleanInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeRead()
                        .CompData(I | UC)
                        .DataSepResp(UC)
                };
                //
                constexpr std::array ReadOnceCleanInvalid = {
                    ReadOnceCleanInvalid_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadOnceMakeInvalid  | I             | -          | I     | CompData_UC,   | RespSepData + 
                //                      |               |            |       | CompData_I     | DataSepResp_UC
                constexpr CacheStateTransition ReadOnceMakeInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeRead()
                        .CompData(I | UC)
                        .DataSepResp(UC)
                };
                //
                constexpr std::array ReadOnceMakeInvalid = {
                    ReadOnceMakeInvalid_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadClean            | I             | -          | SC     | CompData_SC    | RespSepData + 
                //  TagOp = Transfer    |               |            |        |                | DataSepResp_SC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UC     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | UC, UCE       | -          | UC     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            |        |----------------------------------------
                //                      |               |            |        | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | UD, UDP       | -          | UD     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            |        |----------------------------------------
                //                      |               |            |        | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | SC            | -          | SC     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            |-------------------------------------------------
                //                      |               |            | UC     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | SD            | -          | SD     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            |-------------------------------------------------
                //                      |               |            | UD     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                constexpr CacheStateTransition ReadClean_Transfer_I_to_SC = {
                    CacheStateTransition(I, SC).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadClean_Transfer_I_to_UC = {
                    CacheStateTransition(I, UC).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition ReadClean_Transfer_UC_UCE_to_UC = {
                    CacheStateTransition(UC | UCE, UC).TypeRead()
                        .CompData(SC | UC)
                        .DataSepResp(SC | UC)
                };
                constexpr CacheStateTransition ReadClean_Transfer_UD_UDP_to_UD = {
                    CacheStateTransition(UD | UDP, UD).TypeRead()
                        .CompData(SC | UC)
                        .DataSepResp(SC | UC)
                };
                constexpr CacheStateTransition ReadClean_Transfer_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadClean_Transfer_SC_to_UC = {
                    CacheStateTransition(SC, UC).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition ReadClean_Transfer_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadClean_Transfer_SD_to_UD = {
                    CacheStateTransition(SD, UD).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                //
                constexpr std::array ReadClean_Transfer = {
                    ReadClean_Transfer_I_to_SC,
                    ReadClean_Transfer_I_to_UC,
                    ReadClean_Transfer_UC_UCE_to_UC,
                    ReadClean_Transfer_UD_UDP_to_UD,
                    ReadClean_Transfer_SC_to_SC,
                    ReadClean_Transfer_SC_to_UC,
                    ReadClean_Transfer_SD_to_SD,
                    ReadClean_Transfer_SD_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadClean            | I             | -          | SC     | CompData_SC    | RespSepData +
                //  TagOp != Transfer   |               |            |        |                | DataSepResp_SC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UC     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | -          | UC     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                constexpr CacheStateTransition ReadClean_I_to_SC = {
                    CacheStateTransition(I, SC).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadClean_I_to_UC = {
                    CacheStateTransition(I, UC).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition ReadClean_UCE_to_UC = {
                    CacheStateTransition(UCE, UC).TypeRead()
                        .CompData(SC | UC)
                        .DataSepResp(SC | UC)
                };
                //
                constexpr std::array ReadClean = {
                    ReadClean_I_to_SC,
                    ReadClean_I_to_UC,
                    ReadClean_UCE_to_UC
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadNotSharedDirty   | I, UCE        | -          | SC     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UC     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UD     | CompData_UD_PD | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                constexpr CacheStateTransition ReadNotSharedDirty_I_UCE_to_SC = {
                    CacheStateTransition(I | UCE, SC).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadNotSharedDirty_I_UCE_to_UC = {
                    CacheStateTransition(I | UCE, UC).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition ReadNotSharedDirty_I_UCE_to_UD = {
                    CacheStateTransition(I | UCE, UD).TypeRead()
                        .CompData(UD_PD)
                        .DataSepResp(UD_PD)
                };
                //
                constexpr std::array ReadNotSharedDirty = {
                    ReadNotSharedDirty_I_UCE_to_SC,
                    ReadNotSharedDirty_I_UCE_to_UC,
                    ReadNotSharedDirty_I_UCE_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadShared           | I, UCE        | -          | SC     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UC     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            --------------------------------------------------
                //                      |               |            | SD     | CompData_SD_PD | -
                //                      |               |            --------------------------------------------------
                //                      |               |            | UD     | CompData_UD_PD | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                constexpr CacheStateTransition ReadShared_I_UCE_to_SC = {
                    CacheStateTransition(I | UCE, SC).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadShared_I_UCE_to_UC = {
                    CacheStateTransition(I | UCE, UC).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition ReadShared_I_UCE_to_SD = {
                    CacheStateTransition(I | UCE, SD).TypeRead()
                        .CompData(SD_PD)
                };
                constexpr CacheStateTransition ReadShared_I_UCE_to_UD = {
                    CacheStateTransition(I | UCE, UD).TypeRead()
                        .CompData(UD_PD)
                        .DataSepResp(UD_PD)
                };
                //
                constexpr std::array ReadShared = {
                    ReadShared_I_UCE_to_SC,
                    ReadShared_I_UCE_to_UC,
                    ReadShared_I_UCE_to_SD,
                    ReadShared_I_UCE_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadUnique           | I, SC         | UC, UCE    | UC     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UD     | CompData_UD_PD | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                //                      -------------------------------------------------------------------------------
                //                      | SD            | UD, UDP    | UD     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        | CompData_UD_PD | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                constexpr CacheStateTransition ReadUnique_I_SC_or_UC_UCE_to_UC = {
                    CacheStateTransition(I | SC, UC | UCE, UC).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition ReadUnique_I_SC_or_UC_UCE_to_UD = {
                    CacheStateTransition(I | SC, UC | UCE, UD).TypeRead()
                        .CompData(UD_PD)
                        .DataSepResp(UD_PD)
                };
                constexpr CacheStateTransition ReadUnique_SD_or_UD_UDP_to_UD = {
                    CacheStateTransition(SD, UD | UDP, UD).TypeRead()
                        .CompData(UC | UD_PD)
                        .DataSepResp(UC | UD_PD)
                };
                //
                constexpr std::array ReadUnique = {
                    ReadUnique_I_SC_or_UC_UCE_to_UC,
                    ReadUnique_I_SC_or_UC_UCE_to_UD,
                    ReadUnique_SD_or_UD_UDP_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // ReadPreferUnique     | I, SC         | -          | SC     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UC     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UD     | CompData_UD_PD | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                //                      -------------------------------------------------------------------------------
                //                      | SD            | -          | SD     | CompData_SC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UD     | CompData_UC    | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        | CompData_UD_PD | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                constexpr CacheStateTransition ReadPreferUnique_I_SC_to_SC = {
                    CacheStateTransition(I | SC, SC).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadPreferUnique_I_SC_to_UC = {
                    CacheStateTransition(I | SC, UC).TypeRead()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition ReadPreferUnique_I_SC_to_UD = {
                    CacheStateTransition(I | SC, UD).TypeRead()
                        .CompData(UD_PD)
                        .DataSepResp(UD_PD)
                };
                constexpr CacheStateTransition ReadPreferUnique_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeRead()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition ReadPreferUnique_SD_to_UD = {
                    CacheStateTransition(SD, UD).TypeRead()
                        .CompData(UC | UD_PD)
                        .DataSepResp(UC | UD_PD)
                };
                //
                constexpr std::array ReadPreferUnique = {
                    ReadPreferUnique_I_SC_to_SC,
                    ReadPreferUnique_I_SC_to_UC,
                    ReadPreferUnique_I_SC_to_UD,
                    ReadPreferUnique_SD_to_SD,
                    ReadPreferUnique_SD_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // MakeReadUnique       |               | At time of |        |                |
                //  (non-Excl and Excl) |               | response   |        |                |
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD         | UD     | Comp_UC        |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        | CompData_UC    |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        |                | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | SC, SD        | SC         | UC     | Comp_UC        |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        | CompData_UC    |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        |                | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UD     | Comp_UD_PD     |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        | CompData_UD_PD |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        |                | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I          | UC     | CompData_UC    |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        |                | RespSepData +
                //                      |               |            |        |                | DataSepResp_UC
                //                      |               |            --------------------------------------------------
                //                      |               |            | UD     | CompData_UD_PD |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        |                | RespSepData +
                //                      |               |            |        |                | DataSepResp_UD_PD
                constexpr CacheStateTransition MakeReadUnique_SD_to_SD_to_UD = {
                    CacheStateTransition(SD, SD, UD).TypeMakeReadUnique()
                        .Comp(UC)
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition MakeReadUnique_SC_SD_to_SC_to_UC = {
                    CacheStateTransition(SC | SD, SC, UC).TypeMakeReadUnique()
                        .Comp(UC)
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition MakeReadUnique_SC_SD_to_SC_to_UD = {
                    CacheStateTransition(SC | SD, SC, UD).TypeMakeReadUnique()
                        .Comp(UD_PD)
                        .CompData(UD_PD)
                        .DataSepResp(UD_PD)
                };
                constexpr CacheStateTransition MakeReadUnique_SC_SD_to_I_to_UC = {
                    CacheStateTransition(SC | SD, I, UC).TypeMakeReadUnique()
                        .CompData(UC)
                        .DataSepResp(UC)
                };
                constexpr CacheStateTransition MakeReadUnique_SC_SD_to_I_to_UD = {
                    CacheStateTransition(SC | SD, I, UD).TypeMakeReadUnique()
                        .CompData(UD_PD)
                        .DataSepResp(UD_PD)
                };
                //
                constexpr std::array MakeReadUnique = {
                    MakeReadUnique_SD_to_SD_to_UD,
                    MakeReadUnique_SC_SD_to_SC_to_UC,
                    MakeReadUnique_SC_SD_to_SC_to_UD,
                    MakeReadUnique_SC_SD_to_I_to_UC,
                    MakeReadUnique_SC_SD_to_I_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // MakeReadUnique       |               | At time of |        |                |
                //  (Excl additional)   |               | response   |        |                |
                //                      -------------------------------------------------------------------------------
                //                      | SC, SD        | SC         | SC     | Comp_SC        |
                //                      |               ---------------------------------------------------------------
                //                      |               | I          | SC     | CompData_SC    |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        |                | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD         | SD     | Comp_SC        |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        | CompData_SC    |
                //                      |               |            |        -----------------------------------------
                //                      |               |            |        |                | RespSepData +
                //                      |               |            |        |                | DataSepResp_SC
                constexpr CacheStateTransition MakeReadUnique_Excl_SC_SD_to_SC_to_SC = {
                    CacheStateTransition(SC | SD, SC, SC).TypeMakeReadUnique()
                        .Comp(SC)
                };
                constexpr CacheStateTransition MakeReadUnique_Excl_SC_SD_to_I_to_SC = {
                    CacheStateTransition(SC | SD, I, SC).TypeMakeReadUnique()
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                constexpr CacheStateTransition MakeReadUnique_Excl_SD_to_SD_to_SD = {
                    CacheStateTransition(SD, SD, SD).TypeMakeReadUnique()
                        .Comp(SC)
                        .CompData(SC)
                        .DataSepResp(SC)
                };
                //
                constexpr std::array MakeReadUnique_Excl = {
                    MakeReadUnique_SD_to_SD_to_UD,
                    MakeReadUnique_SC_SD_to_SC_to_UC,
                    MakeReadUnique_SC_SD_to_SC_to_UD,
                    MakeReadUnique_SC_SD_to_I_to_UC,
                    MakeReadUnique_SC_SD_to_I_to_UD,
                    MakeReadUnique_Excl_SC_SD_to_SC_to_SC,
                    MakeReadUnique_Excl_SC_SD_to_I_to_SC,
                    MakeReadUnique_Excl_SD_to_SD_to_SD
                };
                // ====================================================================================================
			}
		}
	}
/*
}
*/


#endif
