#pragma once

#include "cst_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_XACT_WRITE)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_XACT_WRITE))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_XACT_WRITE
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_XACT_WRITE
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

			namespace Transitions {

				//                              Table. Requester cache state transitions for Write request transactions
                // ====================================================================================================
                //                      | Cache state   | Before     |       |                     |
                // Request Type         | Initial       | WriteData  | Final | WriteData response  | Comp responses
                //                      | Expected      | response   |       |                     |
                // ----------------------------------------------------------------------------------------------------
                // WriteNoSnpPtl        | I             | -          | I     | NCBWrData or        | DBIDResp* + Comp
                //                      |               |            |       | NCBWrDataCompAck or | or CompDBIDResp
                //                      |               |            |       | WriteDataCancel     |
                constexpr CacheStateTransition WriteNoSnpPtl_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteNoSnpPtl = {
                    WriteNoSnpPtl_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteNoSnpFull       | I             | -          | I     | NCBWrData or        | DBIDResp* + Comp
                //                      |               |            |       | NCBWrDataCompAck    | or CompDBIDResp
                constexpr CacheStateTransition WriteNoSnpFull_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteNoSnpFull = {
                    WriteNoSnpFull_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteNoSnpZero       | I             | -          | I     | -                   | DBIDResp* + Comp
                //                      |               |            |       |                     | or CompDBIDResp
                constexpr CacheStateTransition WriteNoSnpZero_I_to_I = {
                    CacheStateTransition(I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteNoSnpZero = {
                    WriteNoSnpZero_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteUniquePtl       | I             | I          | I     | NCBWrData or        | DBIDResp* + Comp
                // WriteUniquePtlStash  |               |            |       | WriteDataCancel or  | or CompDBIDResp
                //                      |               |            |       | NCBWrDataCompAck    |
                constexpr CacheStateTransition WriteUniquePtl_I_to_I_to_I = {
                    CacheStateTransition(I, I, I).TypeWriteNonCopyBack()
                };
                constexpr CacheStateTransition WriteUniquePtlStash_I_to_I_to_I = {
                    CacheStateTransition(I, I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteUniquePtl = {
                    WriteUniquePtl_I_to_I_to_I
                };
                //
                constexpr std::array WriteUniquePtlStash = {
                    WriteUniquePtlStash_I_to_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteUniqueZero      | I             | I          | I     | -                   | DBIDResp* + Comp
                //                      |               |            |       |                     | or CompDBIDResp
                constexpr CacheStateTransition WriteUniqueZero_I_to_I_to_I = {
                    CacheStateTransition(I, I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteUniqueZero = {
                    WriteUniqueZero_I_to_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteUniqueFull      | I             | I          | I     | NCBWrData or        | DBIDResp* + Comp
                // WriteUniqueFullStash |               |            |       | NCBWrDataCompAck    | or CompDBIDResp
                constexpr CacheStateTransition WriteUniqueFull_I_to_I_to_I = {
                    CacheStateTransition(I, I, I).TypeWriteNonCopyBack()
                };
                constexpr CacheStateTransition WriteUniqueFullStash_I_to_I_to_I = {
                    CacheStateTransition(I, I, I).TypeWriteNonCopyBack()
                };
                //
                constexpr std::array WriteUniqueFull = {
                    WriteUniqueFull_I_to_I_to_I
                };
                //
                constexpr std::array WriteUniqueFullStash = {
                    WriteUniqueFullStash_I_to_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteBackFull        | UD            | UD         | I     | CBWrData_UD_PD      | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | UC         | I     | CBWrData_UC         | CompDBIDResp
                //                      -------------------------------------------------------------------------------
                //                      | UD, SD        | SD         | I     | CBWrData_SD_PD      | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | SC         | I     | CBWrData_SC         | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | I          | I     | CBWrData_I          | CompDBIDResp
                constexpr CacheStateTransition WriteBackFull_UD_to_UD_to_I = {
                    CacheStateTransition(UD, UD, I).TypeWriteCopyBack()
                        .CopyBackWrData(UD_PD)
                };
                constexpr CacheStateTransition WriteBackFull_UD_to_UC_to_I = {
                    CacheStateTransition(UD, UC, I).TypeWriteCopyBack()
                        .CopyBackWrData(UC)
                };
                constexpr CacheStateTransition WriteBackFull_UD_SD_to_SD_to_I = {
                    CacheStateTransition(UD | SD, SD, I).TypeWriteCopyBack()
                        .CopyBackWrData(SD_PD)
                };
                constexpr CacheStateTransition WriteBackFull_UD_SD_to_SC_to_I = {
                    CacheStateTransition(UD | SD, SC, I).TypeWriteCopyBack()
                        .CopyBackWrData(SC)
                };
                constexpr CacheStateTransition WriteBackFull_UD_SD_to_I_to_I = {
                    CacheStateTransition(UD | SD, I, I).TypeWriteCopyBack()
                        .CopyBackWrData(I)
                };
                //
                constexpr std::array WriteBackFull = {
                    WriteBackFull_UD_to_UD_to_I,
                    WriteBackFull_UD_to_UC_to_I,
                    WriteBackFull_UD_SD_to_SD_to_I,
                    WriteBackFull_UD_SD_to_SC_to_I,
                    WriteBackFull_UD_SD_to_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteBackPtl         | UDP           | UDP        | I     | CBWrData_UD_PD      | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | I          | I     | CBWrData_I          | CompDBIDResp
                constexpr CacheStateTransition WriteBackPtl_UDP_to_UDP_to_I = {
                    CacheStateTransition(UDP, UDP, I).TypeWriteCopyBack()
                        .CopyBackWrData(UD_PD)
                };
                constexpr CacheStateTransition WriteBackPtl_UDP_to_I_to_I = {
                    CacheStateTransition(UDP, I, I).TypeWriteCopyBack()
                        .CopyBackWrData(I)
                };
                //
                constexpr std::array WriteBackPtl = {
                    WriteBackPtl_UDP_to_UDP_to_I,
                    WriteBackPtl_UDP_to_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteCleanFull       | UD            | UD         | UC    | CBWrData_UD_PD      | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | UC         | UC    | CBWrData_UC         | CompDBIDResp
                //                      -------------------------------------------------------------------------------
                //                      | UD, SD        | SD         | SC    | CBWrData_SD_PD      | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | SC         | SC    | CBWrData_SC         | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | I          | I     | CBWrData_I          | CompDBIDResp
                constexpr CacheStateTransition WriteCleanFull_UD_to_UD_to_UC = {
                    CacheStateTransition(UD, UD, UC).TypeWriteCopyBack()
                        .CopyBackWrData(UD_PD)
                };
                constexpr CacheStateTransition WriteCleanFull_UD_to_UC_to_UC = {
                    CacheStateTransition(UD, UC, UC).TypeWriteCopyBack()
                        .CopyBackWrData(UC)
                };
                constexpr CacheStateTransition WriteCleanFull_UD_SD_to_SD_to_SC = {
                    CacheStateTransition(UD | SD, SD, SC).TypeWriteCopyBack()
                        .CopyBackWrData(SD_PD)
                };
                constexpr CacheStateTransition WriteCleanFull_UD_SD_to_SC_to_SC = {
                    CacheStateTransition(UD | SD, SC ,SC).TypeWriteCopyBack()
                        .CopyBackWrData(SC)
                };
                constexpr CacheStateTransition WriteCleanFull_UD_SD_to_I_to_I = {
                    CacheStateTransition(UD | SD, I, I).TypeWriteCopyBack()
                        .CopyBackWrData(I)
                };
                //
                constexpr std::array WriteCleanFull = {
                    WriteCleanFull_UD_to_UD_to_UC,
                    WriteCleanFull_UD_to_UC_to_UC,
                    WriteCleanFull_UD_SD_to_SD_to_SC,
                    WriteCleanFull_UD_SD_to_SC_to_SC,
                    WriteCleanFull_UD_SD_to_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteEvictFull       | UC            | UC         | I     | CBWrData_UC         | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | SC         | I     | CBWrData_SC         | CompDBIDResp
                //                      |               ---------------------------------------------------------------
                //                      |               | I          | I     | CBWrData_I          | CompDBIDResp
                constexpr CacheStateTransition WriteEvictFull_UC_to_UC_to_I = {
                    CacheStateTransition(UC, UC, I).TypeWriteCopyBack()
                        .CopyBackWrData(UC)
                };
                constexpr CacheStateTransition WriteEvictFull_UC_to_SC_to_I = {
                    CacheStateTransition(UC, SC, I).TypeWriteCopyBack()
                        .CopyBackWrData(SC)
                };
                constexpr CacheStateTransition WriteEvictFull_UC_to_I_to_I = {
                    CacheStateTransition(UC, I, I).TypeWriteCopyBack()
                        .CopyBackWrData(I)
                };
                //
                constexpr std::array WriteEvictFull = {
                    WriteEvictFull_UC_to_UC_to_I,
                    WriteEvictFull_UC_to_SC_to_I,
                    WriteEvictFull_UC_to_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // WriteEvictOrEvict    | UC            | UC         | I     | CBWrData_UC         | CompDBIDResp
                //                      |               |            |       ------------------------------------------
                //                      |               |            |       | None                | Comp
                //                      -------------------------------------------------------------------------------
                //                      | UC, SC        | SC         | I     | CBWrData_SC         | CompDBIDResp
                //                      |               |            |       ------------------------------------------
                //                      |               |            |       | None                | Comp
                //                      |               ---------------------------------------------------------------
                //                      |               | I          | I     | CBWrData_I          | CompDBIDResp
                //                      |               |            |       ------------------------------------------
                //                      |               |            |       | None                | Comp
                constexpr CacheStateTransition WriteEvictOrEvict_UC_to_UC_to_I = {
                    CacheStateTransition(UC, UC, I).TypeWriteCopyBack()
                        .CopyBackWrData(UC)
                };
                constexpr CacheStateTransition WriteEvictOrEvict_UC_SC_to_SC_to_I = {
                    CacheStateTransition(UC | SC, SC, I).TypeWriteCopyBack()
                        .CopyBackWrData(SC)
                };
                constexpr CacheStateTransition WriteEvictOrEvict_UC_SC_to_I_to_I = {
                    CacheStateTransition(UC | SC, I, I).TypeWriteCopyBack()
                        .CopyBackWrData(I)
                };
                //
                constexpr std::array WriteEvictOrEvict = {
                    WriteEvictOrEvict_UC_to_UC_to_I,
                    WriteEvictOrEvict_UC_SC_to_SC_to_I,
                    WriteEvictOrEvict_UC_SC_to_I_to_I
                };
                // ====================================================================================================
			}
		}
	}
/*
}
*/

#endif
