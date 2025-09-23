#pragma once

#include "cst_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_XACT_SNOOP)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_XACT_SNOOP))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_XACT_SNOOP
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_XACT_SNOOP
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

			namespace Transitions {

				//               Table. SnpOnce Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpOnce              | I             | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | UC        | I, SC     | X         | SnpResp_UC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_UC
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpResp_SC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | UCE       | I         | X         | SnpResp_UC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | UD        | SD        | X         | SnpRespData_UD
                //                      |               ---------------------------------------------------------------
                //                      |               | SD        | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | SnpRespDataPtl_I_PD
                //                      |               --------------------------------------------------------------- 
                //                      |               | UDP       | -         | X         | SnpRespDataPtl_UD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | SnpResp_SC
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpResp_I
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpOnce_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpOnce_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(UC)
                            .SnpRespData(UC)
                };
                constexpr CacheStateTransition SnpOnce_UC_to_SC = {
                    CacheStateTransition(UC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(SC)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpOnce_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpOnce_UCE_to_UCE ={
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpOnce_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpOnce_UD_to_UD = {
                    CacheStateTransition(UD, UD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(UD)
                };
                constexpr CacheStateTransition SnpOnce_UD_to_SD = {
                    CacheStateTransition(UD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpOnce_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpOnce_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpOnce_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpOnce_UDP_to_UDP = {
                    CacheStateTransition(UDP, UDP).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(UD)
                };
                constexpr CacheStateTransition SnpOnce_SC_to_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpOnce_SC_to_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpOnce_SC_to_I_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpOnce_SC_to_I_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpOnce_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpOnce_SD_to_SC = {
                    CacheStateTransition(SD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpOnce_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpOnce = {
                    SnpOnce_I_to_I,
                    SnpOnce_UC_to_UC,
                    SnpOnce_UC_to_SC,
                    SnpOnce_UC_to_I,
                    SnpOnce_UCE_to_UCE,
                    SnpOnce_UCE_to_I,
                    SnpOnce_UD_to_UD,
                    SnpOnce_UD_to_SD,
                    SnpOnce_UD_to_SC,
                    SnpOnce_UD_to_I,
                    SnpOnce_UDP_to_I,
                    SnpOnce_UDP_to_UDP,
                    SnpOnce_SC_to_SC_RetToSrc_0,
                    SnpOnce_SC_to_SC_RetToSrc_1,
                    SnpOnce_SC_to_I_RetToSrc_0,
                    SnpOnce_SC_to_I_RetToSrc_1,
                    SnpOnce_SD_to_SD,
                    SnpOnce_SD_to_SC,
                    SnpOnce_SD_to_I
                };
                //

                //              Table. SnpClean Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpClean             | I             | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | SC        | I         | X         | SnpResp_SC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | SnpResp_SC
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpResp_I
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpClean_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpClean_UC_to_SC = {
                    CacheStateTransition(UC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(SC)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpClean_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpClean_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpClean_UD_to_SD = {
                    CacheStateTransition(UD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpClean_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpClean_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpClean_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpClean_SC_to_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpClean_SC_to_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpClean_SC_to_I_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpClean_SC_to_I_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpClean_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpClean_SD_to_SC = {
                    CacheStateTransition(SD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpClean_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpClean = {
                    SnpClean_I_to_I,
                    SnpClean_UC_to_SC,
                    SnpClean_UC_to_I,
                    SnpClean_UCE_to_I,
                    SnpClean_UD_to_SD,
                    SnpClean_UD_to_SC,
                    SnpClean_UD_to_I,
                    SnpClean_UDP_to_I,
                    SnpClean_SC_to_SC_RetToSrc_0,
                    SnpClean_SC_to_SC_RetToSrc_1,
                    SnpClean_SC_to_I_RetToSrc_0,
                    SnpClean_SC_to_I_RetToSrc_1,
                    SnpClean_SD_to_SD,
                    SnpClean_SD_to_SC,
                    SnpClean_SD_to_I
                };
                //

                //             Table. SnpShared Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpShared            | I             | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | SC        | I         | X         | SnpResp_SC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | SnpResp_SC
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpResp_I
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpShared_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpShared_UC_to_SC = {
                    CacheStateTransition(UC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(SC)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpShared_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpShared_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpShared_UD_to_SD = {
                    CacheStateTransition(UD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpShared_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpShared_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpShared_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpShared_SC_to_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpShared_SC_to_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpShared_SC_to_I_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpShared_SC_to_I_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpShared_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpShared_SD_to_SC = {
                    CacheStateTransition(SD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpShared_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpShared = {
                    SnpShared_I_to_I,
                    SnpShared_UC_to_SC,
                    SnpShared_UC_to_I,
                    SnpShared_UCE_to_I,
                    SnpShared_UD_to_SD,
                    SnpShared_UD_to_SC,
                    SnpShared_UD_to_I,
                    SnpShared_UDP_to_I,
                    SnpShared_SC_to_SC_RetToSrc_0,
                    SnpShared_SC_to_SC_RetToSrc_1,
                    SnpShared_SC_to_I_RetToSrc_0,
                    SnpShared_SC_to_I_RetToSrc_1,
                    SnpShared_SD_to_SD,
                    SnpShared_SD_to_SC,
                    SnpShared_SD_to_I
                };
                //

                //     Table. SnpNotSharedDirty Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpNotSharedDirty    | I             | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | SC        | I         | X         | SnpResp_SC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | SnpResp_SC
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpResp_I
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpNotSharedDirty_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_UC_to_SC = {
                    CacheStateTransition(UC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(SC)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_UD_to_SD = {
                    CacheStateTransition(UD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_SC_to_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_SC_to_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_SC_to_I_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_SC_to_I_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_SD_to_SC = {
                    CacheStateTransition(SD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpNotSharedDirty_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpNotSharedDirty = {
                    SnpNotSharedDirty_I_to_I,
                    SnpNotSharedDirty_UC_to_SC,
                    SnpNotSharedDirty_UC_to_I,
                    SnpNotSharedDirty_UCE_to_I,
                    SnpNotSharedDirty_UD_to_SD,
                    SnpNotSharedDirty_UD_to_SC,
                    SnpNotSharedDirty_UD_to_I,
                    SnpNotSharedDirty_UDP_to_I,
                    SnpNotSharedDirty_SC_to_SC_RetToSrc_0,
                    SnpNotSharedDirty_SC_to_SC_RetToSrc_1,
                    SnpNotSharedDirty_SC_to_I_RetToSrc_0,
                    SnpNotSharedDirty_SC_to_I_RetToSrc_1,
                    SnpNotSharedDirty_SD_to_SD,
                    SnpNotSharedDirty_SD_to_SC,
                    SnpNotSharedDirty_SD_to_I
                };
                //

                //       Table. SnpPreferUnique Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpPreferUnique      | I             | I         | -         | X         | SnpResp_I
                // (NOT IN              -------------------------------------------------------------------------------
                //  exclusive sequence) | UC            | SC        | I         | X         | SnpResp_SC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | SnpResp_SC
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpResp_I
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD(a)     | -         | X         | SnpRespData_SD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | X         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | X         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_UC_to_SC = {
                    CacheStateTransition(UC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(SC)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_UD_to_SD = {
                    CacheStateTransition(UD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_SC_to_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_SC_to_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(SC)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_SC_to_I_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_SC_to_I_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SD)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_SD_to_SC = {
                    CacheStateTransition(SD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpPreferUnique_NoExcl_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpPreferUnique_NoExcl = {
                    SnpPreferUnique_NoExcl_I_to_I,
                    SnpPreferUnique_NoExcl_UC_to_SC,
                    SnpPreferUnique_NoExcl_UC_to_I,
                    SnpPreferUnique_NoExcl_UCE_to_I,
                    SnpPreferUnique_NoExcl_UD_to_SD,
                    SnpPreferUnique_NoExcl_UD_to_SC,
                    SnpPreferUnique_NoExcl_UD_to_I,
                    SnpPreferUnique_NoExcl_UDP_to_I,
                    SnpPreferUnique_NoExcl_SC_to_SC_RetToSrc_0,
                    SnpPreferUnique_NoExcl_SC_to_SC_RetToSrc_1,
                    SnpPreferUnique_NoExcl_SC_to_I_RetToSrc_0,
                    SnpPreferUnique_NoExcl_SC_to_I_RetToSrc_1,
                    SnpPreferUnique_NoExcl_SD_to_SD,
                    SnpPreferUnique_NoExcl_SD_to_SC,
                    SnpPreferUnique_NoExcl_SD_to_I
                };
                //

                //             Table. SnpUnique Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpUnique            | I             | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | I         | -         | X         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | I         | -         | X         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | I         | -         | 0         | SnpResp_I
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | I         | -         | X         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpUnique_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUnique_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpUnique_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUnique_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpUnique_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpUnique_SC_to_I_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUnique_SC_to_I_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpUnique_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpUnique = {
                    SnpUnique_I_to_I,
                    SnpUnique_UC_to_I,
                    SnpUnique_UCE_to_I,
                    SnpUnique_UD_to_I,
                    SnpUnique_UDP_to_I,
                    SnpUnique_SC_to_I_RetToSrc_0,
                    SnpUnique_SC_to_I_RetToSrc_1,
                    SnpUnique_SD_to_I
                };
                //

                //       Table. SnpPreferUnique Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpPreferUnique      | I             | I         | -         | X         | SnpResp_I
                // (RIGHT IN            -------------------------------------------------------------------------------
                //  exclusive sequence) | UC            | I         | -         | X         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | I         | -         | X         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | I         | -         | 0         | SnpResp_I
                //                      |               |           |           ---------------------------------------
                //                      |               |           |           | 1         | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | I         | -         | X         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpPreferUnique_InExcl_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_InExcl_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_InExcl_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_InExcl_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpPreferUnique_InExcl_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpPreferUnique_InExcl_SC_to_I_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_InExcl_SC_to_I_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpPreferUnique_InExcl_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpPreferUnique_InExcl = {
                    SnpPreferUnique_InExcl_I_to_I,
                    SnpPreferUnique_InExcl_UC_to_I,
                    SnpPreferUnique_InExcl_UCE_to_I,
                    SnpPreferUnique_InExcl_UD_to_I,
                    SnpPreferUnique_InExcl_UDP_to_I,
                    SnpPreferUnique_InExcl_SC_to_I_RetToSrc_0,
                    SnpPreferUnique_InExcl_SC_to_I_RetToSrc_1,
                    SnpPreferUnique_InExcl_SD_to_I
                };
                //

                //        Table. SnpCleanShared Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpCleanShared       | I             | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | UC        | I, SC     | 0         | SnpResp_UC
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | 0         | SnpResp_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | UC        | I, SC     | 0         | SnpRespData_UC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | SC        | I         | 0         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | 0         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | SnpResp_SC
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SC        | I         | 0         | SnpRespData_SC_PD
                //                      |               ---------------------------------------------------------------
                //                      |               | I         | -         | 0         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpCleanShared_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanShared_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpCleanShared_UC_to_SC = {
                    CacheStateTransition(UC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpCleanShared_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanShared_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanShared_UD_to_UC = {
                    CacheStateTransition(UD, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(UC_PD)
                };
                constexpr CacheStateTransition SnpCleanShared_UD_to_SC = {
                    CacheStateTransition(UD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpCleanShared_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpCleanShared_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpCleanShared_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpCleanShared_SC_to_I = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanShared_SD_to_SC = {
                    CacheStateTransition(SD, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(SC_PD)
                };
                constexpr CacheStateTransition SnpCleanShared_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpCleanShared = {
                    SnpCleanShared_I_to_I,
                    SnpCleanShared_UC_to_UC,
                    SnpCleanShared_UC_to_SC,
                    SnpCleanShared_UC_to_I,
                    SnpCleanShared_UCE_to_I,
                    SnpCleanShared_UD_to_UC,
                    SnpCleanShared_UD_to_SC,
                    SnpCleanShared_UD_to_I,
                    SnpCleanShared_UDP_to_I,
                    SnpCleanShared_SC_to_SC,
                    SnpCleanShared_SC_to_I,
                    SnpCleanShared_SD_to_SC,
                    SnpCleanShared_SD_to_I
                };
                //

                //       Table. SnpCleanInvalid Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpCleanInvalid      | I             | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | I         | -         | 0         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | 0         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | I         | -         | 0         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpCleanInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanInvalid_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanInvalid_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanInvalid_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpCleanInvalid_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpCleanInvalid_SC_to_I = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanInvalid_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpCleanInvalid = {
                    SnpCleanInvalid_I_to_I,
                    SnpCleanInvalid_UC_to_I,
                    SnpCleanInvalid_UCE_to_I,
                    SnpCleanInvalid_UD_to_I,
                    SnpCleanInvalid_UDP_to_I,
                    SnpCleanInvalid_SC_to_I,
                    SnpCleanInvalid_SD_to_I
                };
                //

                //        Table. SnpMakeInvalid Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpMakeInvalid       | I             | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SC            | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | I         | -         | 0         | SnpResp_I
                constexpr CacheStateTransition SnpMakeInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_SC_to_I = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpMakeInvalid_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                //
                constexpr std::array SnpMakeInvalid = {
                    SnpMakeInvalid_I_to_I,
                    SnpMakeInvalid_UC_to_I,
                    SnpMakeInvalid_UCE_to_I,
                    SnpMakeInvalid_UD_to_I,
                    SnpMakeInvalid_UDP_to_I,
                    SnpMakeInvalid_SC_to_I,
                    SnpMakeInvalid_SD_to_I
                };
                //

                //              Table. SnpQuery Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpQuery             | I             | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UC            | UC        | -         | 0         | SnpResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | UCE       | -         | 0         | SnpResp_UC
                //                      -------------------------------------------------------------------------------
                //                      | UD            | UD        | -         | 0         | SnpResp_UD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | UDP       | -         | 0         | SnpResp_UD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | -         | 0         | SnpResp_SC
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | SnpResp_SD
                constexpr CacheStateTransition SnpQuery_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpQuery_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpQuery_UCE_to_UC = {
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpQuery_UD_to_UD = {
                    CacheStateTransition(UD, UD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UD)
                };
                constexpr CacheStateTransition SnpQuery_UDP_to_UD = {
                    CacheStateTransition(UDP, UDP).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UD)
                };
                constexpr CacheStateTransition SnpQuery_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpQuery_SD_to_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SD)
                };
                //
                constexpr std::array SnpQuery = {
                    SnpQuery_I_to_I,
                    SnpQuery_UC_to_UC,
                    SnpQuery_UCE_to_UC,
                    SnpQuery_UD_to_UD,
                    SnpQuery_UDP_to_UD,
                    SnpQuery_SC_to_SC,
                    SnpQuery_SD_to_SD
                };
                //

                //        Table. SnpUniqueStash Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpUniqueStash       | I             | I         | -         | 0         | SnpResp_I
                //                      ------------------------------------------------------------------------------- 
                //                      | UC            | I         | -         | 0         | SnpRespData_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | 0         | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | I         | -         | 0         | SnpRespData_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | 0         | SnpRespDataPtl_I_PD
                //                      -------------------------------------------------------------------------------
                //                      | SC            | I         | -         | 0         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpRespData_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | I         | -         | 0         | SnpRespData_I_PD
                constexpr CacheStateTransition SnpUniqueStash_I_to_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUniqueStash_UC_to_I = {
                    CacheStateTransition(UC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpRespData(I)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUniqueStash_UCE_to_I = {
                    CacheStateTransition(UCE, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUniqueStash_UD_to_I = {
                    CacheStateTransition(UD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpUniqueStash_UDP_to_I = {
                    CacheStateTransition(UDP, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpUniqueStash_SC_to_I = {
                    CacheStateTransition(SC, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(I)
                            .SnpRespData(I)
                };
                constexpr CacheStateTransition SnpUniqueStash_SD_to_I = {
                    CacheStateTransition(SD, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpUniqueStash = {
                    SnpUniqueStash_I_to_I,
                    SnpUniqueStash_UC_to_I,
                    SnpUniqueStash_UCE_to_I,
                    SnpUniqueStash_UD_to_I,
                    SnpUniqueStash_UDP_to_I,
                    SnpUniqueStash_SC_to_I,
                    SnpUniqueStash_SD_to_I
                };
                //

                //   Table. SnpMakeInvalidStash Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpMakeInvalidStash  | Any           | I         | -         | 0         | SnpResp_I
                constexpr CacheStateTransition SnpMakeInvalidStash_Any_to_I = {
                    CacheStateTransition(CacheStates::All, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(I)
                };
                //
                constexpr std::array SnpMakeInvalidStash = {
                    SnpMakeInvalidStash_Any_to_I
                };
                //

                //        Table. SnpStashUnique Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpStashUnique       | I             | I         | -         | 0         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I_Read
                //                      -------------------------------------------------------------------------------
                //                      | UC            | UC        | -         | 0         | SnpResp_UC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | UCE       | -         | 0         | SnpResp_UC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_UC_Read
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | UD        | -         | 0         | SnpResp_UD
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | UDP       | -         | 0         | SnpResp_UD
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | -         | 0         | SnpResp_SC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_SC_Read
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | SnpResp_SD
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_SD_Read
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                constexpr CacheStateTransition SnpStashUnique_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashUnique_I_to_I_as_I_Read = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashUnique_UC_to_UC_as_UC = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpStashUnique_UC_to_UC_as_I = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashUnique_UCE_to_UCE_as_UC = {
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpStashUnique_UCE_to_UCE_as_UC_Read = {
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpStashUnique_UCE_to_UCE_as_I = {
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashUnique_UD_to_UD_as_UD = {
                    CacheStateTransition(UD, UD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UD)
                };
                constexpr CacheStateTransition SnpStashUnique_UD_to_UD_as_I = {
                    CacheStateTransition(UD, UD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashUnique_UDP_to_UDP_as_UD = {
                    CacheStateTransition(UDP, UDP).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UD)
                };
                constexpr CacheStateTransition SnpStashUnique_UDP_to_UDP_as_I = {
                    CacheStateTransition(UDP, UDP).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashUnique_SC_to_SC_as_SC = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpStashUnique_SC_to_SC_as_SC_Read = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpStashUnique_SC_to_SC_as_I = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashUnique_SD_to_SD_as_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SD)
                };
                constexpr CacheStateTransition SnpStashUnique_SD_to_SD_as_SD_Read = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(SD)
                };
                constexpr CacheStateTransition SnpStashUnique_SD_to_SD_as_I = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                //
                constexpr std::array SnpStashUnique = {
                    SnpStashUnique_I_to_I_as_I,
                    SnpStashUnique_I_to_I_as_I_Read,
                    SnpStashUnique_UC_to_UC_as_UC,
                    SnpStashUnique_UC_to_UC_as_I,
                    SnpStashUnique_UCE_to_UCE_as_UC,
                    SnpStashUnique_UCE_to_UCE_as_UC_Read,
                    SnpStashUnique_UCE_to_UCE_as_I,
                    SnpStashUnique_UD_to_UD_as_UD,
                    SnpStashUnique_UD_to_UD_as_I,
                    SnpStashUnique_UDP_to_UDP_as_UD,
                    SnpStashUnique_UDP_to_UDP_as_I,
                    SnpStashUnique_SC_to_SC_as_SC,
                    SnpStashUnique_SC_to_SC_as_SC_Read,
                    SnpStashUnique_SC_to_SC_as_I,
                    SnpStashUnique_SD_to_SD_as_SD,
                    SnpStashUnique_SD_to_SD_as_SD_Read,
                    SnpStashUnique_SD_to_SD_as_I
                };
                //

                //        Table. SnpStashShared Cache state transitions, RetToSrc value, and valid completion responses
                // ====================================================================================================
                //                      | Initial       | Final                 |           |
                //                      | cache state   | cache state           |           | 
                // Request Type         |               |-----------------------| RetToSrc  | Snoop response
                //                      |               | Expected  | Others    |           |
                //                      |               |           | permitted |           |
                // ----------------------------------------------------------------------------------------------------
                // SnpStashShared       | I             | I         | -         | 0         | SnpResp_I
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I_Read
                //                      -------------------------------------------------------------------------------
                //                      | UC            | UC        | -         | 0         | SnpResp_UC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UCE           | UCE       | -         | 0         | SnpResp_UC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_UC_Read
                //                      |               |           |           |           --------------------------- 
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UD            | UD        | -         | 0         | SnpResp_UD
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | UDP           | UDP       | -         | 0         | SnpResp_UD
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SC            | SC        | -         | 0         | SnpResp_SC
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                //                      -------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | SnpResp_SD
                //                      |               |           |           |           ---------------------------
                //                      |               |           |           |           | SnpResp_I
                constexpr CacheStateTransition SnpStashShared_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashShared_I_to_I_as_I_Read = {
                    CacheStateTransition(I, I).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashShared_UC_to_UC_as_UC = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpStashShared_UC_to_UC_as_I = {
                    CacheStateTransition(UC, UC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashShared_UCE_to_UCE_as_UC = {
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpStashShared_UCE_to_UCE_as_UC_Read = {
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                        .DataPull()
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpStashShared_UCE_to_UCE_as_I = {
                    CacheStateTransition(UCE, UCE).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashShared_UD_to_UD_as_UD = {
                    CacheStateTransition(UD, UD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UD)
                };
                constexpr CacheStateTransition SnpStashShared_UD_to_UD_as_I = {
                    CacheStateTransition(UD, UD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashShared_UDP_to_UDP_as_UD = {
                    CacheStateTransition(UDP, UDP).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UD)
                };
                constexpr CacheStateTransition SnpStashShared_UDP_to_UDP_as_I = {
                    CacheStateTransition(UDP, UDP).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashShared_SC_to_SC_as_SC = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpStashShared_SC_to_SC_as_I = {
                    CacheStateTransition(SC, SC).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpStashShared_SD_to_SD_as_SD = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SD)
                };
                constexpr CacheStateTransition SnpStashShared_SD_to_SD_as_I = {
                    CacheStateTransition(SD, SD).TypeSnoop()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                //
                constexpr std::array SnpStashShared = {
                    SnpStashShared_I_to_I_as_I,
                    SnpStashShared_I_to_I_as_I_Read,
                    SnpStashShared_UC_to_UC_as_UC,
                    SnpStashShared_UC_to_UC_as_I,
                    SnpStashShared_UCE_to_UCE_as_UC,
                    SnpStashShared_UCE_to_UCE_as_UC_Read,
                    SnpStashShared_UCE_to_UCE_as_I,
                    SnpStashShared_UD_to_UD_as_UD,
                    SnpStashShared_UD_to_UD_as_I,
                    SnpStashShared_UDP_to_UDP_as_UD,
                    SnpStashShared_UDP_to_UDP_as_I,
                    SnpStashShared_SC_to_SC_as_SC,
                    SnpStashShared_SC_to_SC_as_I,
                    SnpStashShared_SD_to_SD_as_SD,
                    SnpStashShared_SD_to_SD_as_I
                };
                //

                //                                                                                    Table. SnpOnceFwd Snoopee state transitions
                // ==============================================================================================================================
                //                      | Snoopee       | Snoopee               |           |                                                   
                //                      | Initial       | Final                 |           | Snoopee response to                               
                //                      | cache state   | cache state           |           |                                                   
                // Request Type         |               |-----------------------| RetToSrc  |----------------------------------------------------
                //                      |               | Expected  | Others    |           | Requester         | Home                          
                //                      |               |           | permitted |           |                   |                                
                // ------------------------------------------------------------------------------------------------------------------------------
                // SnpOnceFwd           | I             | I         | -         | 0         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UC            | UC        | -         | 0         | CompData_I        | SnpResp_UC_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | 0         | CompData_I        | SnpResp_SC_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_I        | SnpResp_I_Fwded_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UCE           | UCE       | -         | 0         | -                 | SnpResp_UC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UD            | UD        | -         | 0         | CompData_I        | SnpResp_UD_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SD        | -         | 0         | CompData_I        | SnpResp_SD_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | 0         | CompData_I        | SnpRespData_SC_PD_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_I        | SnpRespData_I_PD_Fwded_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UDP           | UDP       | -         | 0         | -                 | SnpRespDataPtl_UD
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | -                 | SnpRespDataPtl_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | -                 | SnpResp_SC
                //                      |               |           |           |           -----------------------------------------------------
                //                      |               |           |           |           | CompData_I        | SnpResp_SC_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | -                 | SnpResp_I
                //                      |               |           |           |           -----------------------------------------------------
                //                      |               |           |           |           | CompData_I        | SnpResp_I_Fwded_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | CompData_I        | SnpResp_SD_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | 0         | CompData_I        | SnpRespData_SC_PD_Fwded_I
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_I        | SnpRespData_I_PD_Fwded_I
                constexpr CacheStateTransition SnpOnceFwd_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UC_to_UC_as_UC_Fwded_I = {
                    CacheStateTransition(UC, UC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(UC, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UC_to_SC_as_SC_Fwded_I = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UC_to_I_as_I_Fwded_I = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UCE_to_UCE_as_UC = {
                    CacheStateTransition(UCE, UCE).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(UC)
                };
                constexpr CacheStateTransition SnpOnceFwd_UCE_to_I_as_I = {
                    CacheStateTransition(UCE, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UD_to_UD_as_UD_Fwded_I = {
                    CacheStateTransition(UD, UD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(UD, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UD_to_SD_as_SD_Fwded_I = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UD_to_SC_as_SC_PD_Fwded_I = {
                    CacheStateTransition(UD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataFwded(SC_PD, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UD_to_I_as_I_PD_Fwded_I = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataFwded(I_PD, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_UDP_to_UDP_as_UD = {
                    CacheStateTransition(UDP, UDP).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataPtl(UD)
                };
                constexpr CacheStateTransition SnpOnceFwd_UDP_to_I_as_I_PD = {
                    CacheStateTransition(UDP, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpOnceFwd_SC_to_SC_as_SC = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(SC)
                };
                constexpr CacheStateTransition SnpOnceFwd_SC_to_SC_as_SC_Fwded_I = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_SC_to_I_as_I = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpOnceFwd_SC_to_I_as_I_Fwded_I = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_SD_to_SD_as_SD_Fwded_I = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_SD_to_SC_as_SC_PD_Fwded_I = {
                    CacheStateTransition(SD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataFwded(SC_PD, I)
                };
                constexpr CacheStateTransition SnpOnceFwd_SD_to_I_as_I_PD_Fwded_I = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataFwded(I_PD, I)
                };
                //
                constexpr std::array SnpOnceFwd = {
                    SnpOnceFwd_I_to_I_as_I,
                    SnpOnceFwd_UC_to_UC_as_UC_Fwded_I,
                    SnpOnceFwd_UC_to_SC_as_SC_Fwded_I,
                    SnpOnceFwd_UC_to_I_as_I_Fwded_I,
                    SnpOnceFwd_UCE_to_UCE_as_UC,
                    SnpOnceFwd_UCE_to_I_as_I,
                    SnpOnceFwd_UD_to_UD_as_UD_Fwded_I,
                    SnpOnceFwd_UD_to_SD_as_SD_Fwded_I,
                    SnpOnceFwd_UD_to_SC_as_SC_PD_Fwded_I,
                    SnpOnceFwd_UD_to_I_as_I_PD_Fwded_I,
                    SnpOnceFwd_UDP_to_UDP_as_UD,
                    SnpOnceFwd_UDP_to_I_as_I_PD,
                    SnpOnceFwd_SC_to_SC_as_SC,
                    SnpOnceFwd_SC_to_SC_as_SC_Fwded_I,
                    SnpOnceFwd_SC_to_I_as_I,
                    SnpOnceFwd_SC_to_I_as_I_Fwded_I,
                    SnpOnceFwd_SD_to_SD_as_SD_Fwded_I,
                    SnpOnceFwd_SD_to_SC_as_SC_PD_Fwded_I,
                    SnpOnceFwd_SD_to_I_as_I_PD_Fwded_I
                };
                //

                //                                                                                   Table. SnpCleanFwd Snoopee state transitions
                // ==============================================================================================================================
                //                      | Snoopee       | Snoopee               |           |                                                   
                //                      | Initial       | Final                 |           | Snoopee response to                               
                //                      | cache state   | cache state           |           |                                                   
                // Request Type         |               |-----------------------| RetToSrc  |----------------------------------------------------
                //                      |               | Expected  | Others    |           | Requester         | Home                          
                //                      |               |           | permitted |           |                   |                                
                // ------------------------------------------------------------------------------------------------------------------------------
                // SnpCleanFwd          | I             | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UC            | SC        | I         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SC       | SnpResp_I_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_I_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | X         | CompData_SC       | SnpRespData_I_PD_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | -                 | SnpRespDataPtl_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SC       | SnpResp_I_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_I_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | X         | CompData_SC       | SnpRespData_I_PD_Fwded_SC
                constexpr CacheStateTransition SnpCleanFwd_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UCE_to_I_as_I = {
                    CacheStateTransition(UCE, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpCleanFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(UD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UD_to_I_as_I_PD_Fwded_SC = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(I_PD, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_UDP_to_I_as_I_PD = {
                    CacheStateTransition(UDP, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpCleanFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_SD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(SD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                constexpr CacheStateTransition SnpCleanFwd_SD_to_I_as_I_PD_Fwded_SC = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(I_PD, SC)
                };
                //
                constexpr std::array SnpCleanFwd = {
                    SnpCleanFwd_I_to_I_as_I,
                    SnpCleanFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpCleanFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpCleanFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_0,
                    SnpCleanFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_1,
                    SnpCleanFwd_UCE_to_I_as_I,
                    SnpCleanFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpCleanFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpCleanFwd_UD_to_SC_as_SC_PD_Fwded_SC,
                    SnpCleanFwd_UD_to_I_as_I_PD_Fwded_SC,
                    SnpCleanFwd_UDP_to_I_as_I_PD,
                    SnpCleanFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpCleanFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpCleanFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_0,
                    SnpCleanFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_1,
                    SnpCleanFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpCleanFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpCleanFwd_SD_to_SC_as_SC_PD_Fwded_SC,
                    SnpCleanFwd_SD_to_I_as_I_PD_Fwded_SC
                };
                //

                //                                                                          Table. SnpNotSharedDirtyFwd Snoopee state transitions
                // ==============================================================================================================================
                //                      | Snoopee       | Snoopee               |           |                                                   
                //                      | Initial       | Final                 |           | Snoopee response to                               
                //                      | cache state   | cache state           |           |                                                   
                // Request Type         |               |-----------------------| RetToSrc  |----------------------------------------------------
                //                      |               | Expected  | Others    |           | Requester         | Home                          
                //                      |               |           | permitted |           |                   |                                
                // ------------------------------------------------------------------------------------------------------------------------------
                // SnpNotSharedDirtyFwd | I             | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UC            | SC        | I         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SC       | SnpResp_I_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_I_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | X         | CompData_SC       | SnpRespData_I_PD_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | -                 | SnpRespDataPtl_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SC       | SnpResp_I_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_I_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | X         | CompData_SC       | SnpRespData_I_PD_Fwded_SC
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UCE_to_I_as_I = {
                    CacheStateTransition(UCE, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(UD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UD_to_I_as_I_PD_Fwded_SC = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(I_PD, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_UDP_to_I_as_I_PD = {
                    CacheStateTransition(UDP, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(SD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                constexpr CacheStateTransition SnpNotSharedDirtyFwd_SD_to_I_as_I_PD_Fwded_SC = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(I_PD, SC)
                };
                //
                constexpr std::array SnpNotSharedDirtyFwd = {
                    SnpNotSharedDirtyFwd_I_to_I_as_I,
                    SnpNotSharedDirtyFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpNotSharedDirtyFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpNotSharedDirtyFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_0,
                    SnpNotSharedDirtyFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_1,
                    SnpNotSharedDirtyFwd_UCE_to_I_as_I,
                    SnpNotSharedDirtyFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpNotSharedDirtyFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpNotSharedDirtyFwd_UD_to_SC_as_SC_PD_Fwded_SC,
                    SnpNotSharedDirtyFwd_UD_to_I_as_I_PD_Fwded_SC,
                    SnpNotSharedDirtyFwd_UDP_to_I_as_I_PD,
                    SnpNotSharedDirtyFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpNotSharedDirtyFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpNotSharedDirtyFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_0,
                    SnpNotSharedDirtyFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_1,
                    SnpNotSharedDirtyFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpNotSharedDirtyFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpNotSharedDirtyFwd_SD_to_SC_as_SC_PD_Fwded_SC,
                    SnpNotSharedDirtyFwd_SD_to_I_as_I_PD_Fwded_SC
                };
                //

                //                                                                                  Table. SnpSharedFwd Snoopee state transitions
                // ==============================================================================================================================
                //                      | Snoopee       | Snoopee               |           |                                                   
                //                      | Initial       | Final                 |           | Snoopee response to                               
                //                      | cache state   | cache state           |           |                                                   
                // Request Type         |               |-----------------------| RetToSrc  |----------------------------------------------------
                //                      |               | Expected  | Others    |           | Requester         | Home                          
                //                      |               |           | permitted |           |                   |                                
                // ------------------------------------------------------------------------------------------------------------------------------
                // SnpSharedFwd         | I             | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UC            | SC        | I         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SC       | SnpResp_I_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_I_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | 0         | CompData_SD_PD    | SnpResp_SC_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SD_PD    | SnpRespData_SC_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SD_PD    | SnpResp_I_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SD_PD    | SnpRespData_I_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | X         | CompData_SC       | SnpRespData_I_PD_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | -                 | SnpRespDataPtl_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SC            | SC        | I         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SC       | SnpResp_I_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_I_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | I         | 0         | CompData_SD_PD    | SnpResp_SC_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SD_PD    | SnpRespData_SC_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | I         | -         | 0         | CompData_SD_PD    | SnpResp_I_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SD_PD    | SnpRespData_I_Fwded_SD_PD
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | X         | CompData_SC       | SnpRespData_I_PD_Fwded_SC
                constexpr CacheStateTransition SnpSharedFwd_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpSharedFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UCE_to_I_as_I = {
                    CacheStateTransition(UCE, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_0 = {
                    CacheStateTransition(UD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_1 = {
                    CacheStateTransition(UD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(UD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_I_as_I_Fwded_SD_PD_RetToSrc_0 = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_I_as_I_Fwded_SD_PD_RetToSrc_1 = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_UD_to_I_as_I_PD_Fwded_SC = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(I_PD, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_UDP_to_I_as_I_PD = {
                    CacheStateTransition(UDP, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_0 = {
                    CacheStateTransition(SD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_1 = {
                    CacheStateTransition(SD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(SD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_I_as_I_Fwded_SD_PD_RetToSrc_0 = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_I_as_I_Fwded_SD_PD_RetToSrc_1 = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(I, SD_PD)
                };
                constexpr CacheStateTransition SnpSharedFwd_SD_to_I_as_I_PD_Fwded_SC = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(I_PD, SC)
                };
                //
                constexpr std::array SnpSharedFwd = {
                    SnpSharedFwd_I_to_I_as_I,
                    SnpSharedFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpSharedFwd_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpSharedFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_0,
                    SnpSharedFwd_UC_to_I_as_I_Fwded_SC_RetToSrc_1,
                    SnpSharedFwd_UCE_to_I_as_I,
                    SnpSharedFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpSharedFwd_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpSharedFwd_UD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_0,
                    SnpSharedFwd_UD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_1,
                    SnpSharedFwd_UD_to_SC_as_SC_PD_Fwded_SC,
                    SnpSharedFwd_UD_to_I_as_I_Fwded_SD_PD_RetToSrc_0,
                    SnpSharedFwd_UD_to_I_as_I_Fwded_SD_PD_RetToSrc_1,
                    SnpSharedFwd_UD_to_I_as_I_PD_Fwded_SC,
                    SnpSharedFwd_UDP_to_I_as_I_PD,
                    SnpSharedFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpSharedFwd_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpSharedFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_0,
                    SnpSharedFwd_SC_to_I_as_I_Fwded_SC_RetToSrc_1,
                    SnpSharedFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpSharedFwd_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpSharedFwd_SD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_0,
                    SnpSharedFwd_SD_to_SC_as_SC_Fwded_SD_PD_RetToSrc_1,
                    SnpSharedFwd_SD_to_SC_as_SC_PD_Fwded_SC,
                    SnpSharedFwd_SD_to_I_as_I_Fwded_SD_PD_RetToSrc_0,
                    SnpSharedFwd_SD_to_I_as_I_Fwded_SD_PD_RetToSrc_1,
                    SnpSharedFwd_SD_to_I_as_I_PD_Fwded_SC
                };
                //

                //                                                                                  Table. SnpUniqueFwd Snoopee state transitions
                // ==============================================================================================================================
                //                      | Snoopee       | Snoopee               |           |                                                   
                //                      | Initial       | Final                 |           | Snoopee response to                               
                //                      | cache state   | cache state           |           |                                                   
                // Request Type         |               |-----------------------| RetToSrc  |----------------------------------------------------
                //                      |               | Expected  | Others    |           | Requester         | Home                          
                //                      |               |           | permitted |           |                   |                                
                // ------------------------------------------------------------------------------------------------------------------------------
                // SnpUniqueFwd         | I             | I         | -         | 0         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UC            | I         | -         | 0         | CompData_UC       | SnpResp_I_Fwded_UC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | 0         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UD            | I         | -         | 0         | CompData_UD_PD    | SnpResp_I_Fwded_UD_PD
                //                      |               |           |           |           -----------------------------------------------------
                //                      |               |           |           |           | -                 | SnpRespData_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | 0         | -                 | SnpRespDataPtl_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SC            | I         | -         | 0         | CompData_UC       | SnpResp_I_Fwded_UC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SD            | I         | -         | 0         | CompData_UD_PD    | SnpResp_I_Fwded_UD_PD
                //                      |               |           |           |           -----------------------------------------------------
                //                      |               |           |           |           | -                 | SnpRespData_I_PD
                constexpr CacheStateTransition SnpUniqueFwd_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUniqueFwd_UC_to_I_as_I_Fwded_UC = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, UC)
                };
                constexpr CacheStateTransition SnpUniqueFwd_UCE_to_I_as_I = {
                    CacheStateTransition(UCE, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpUniqueFwd_UD_to_I_as_I_Fwded_UD_PD = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, UD_PD)
                };
                constexpr CacheStateTransition SnpUniqueFwd_UD_to_I_as_I_PD = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpUniqueFwd_UDP_to_I_as_I_PD = {
                    CacheStateTransition(UDP, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpUniqueFwd_SC_to_I_as_I_Fwded_UC = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, UC)
                };
                constexpr CacheStateTransition SnpUniqueFwd_SD_to_I_as_I_Fwded_UD_PD = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(I, UD_PD)
                };
                constexpr CacheStateTransition SnpUniqueFwd_SD_to_I_as_I_PD = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpUniqueFwd = {
                    SnpUniqueFwd_I_to_I_as_I,
                    SnpUniqueFwd_UC_to_I_as_I_Fwded_UC,
                    SnpUniqueFwd_UCE_to_I_as_I,
                    SnpUniqueFwd_UD_to_I_as_I_Fwded_UD_PD,
                    SnpUniqueFwd_UD_to_I_as_I_PD,
                    SnpUniqueFwd_UDP_to_I_as_I_PD,
                    SnpUniqueFwd_SC_to_I_as_I_Fwded_UC,
                    SnpUniqueFwd_SD_to_I_as_I_Fwded_UD_PD,
                    SnpUniqueFwd_SD_to_I_as_I_PD
                };
                //

                //                                Table. State transitions for SnpPreferUniqueFwd when Snoopee is executing an exclusive sequence
                // ==============================================================================================================================
                //                      | Snoopee       | Snoopee               |           |                                                   
                //                      | Initial       | Final                 |           | Snoopee response to                               
                //                      | cache state   | cache state           |           |                                                   
                // Request Type         |               |-----------------------| RetToSrc  |----------------------------------------------------
                //                      |               | Expected  | Others    |           | Requester         | Home                          
                //                      |               |           | permitted |           |                   |                                
                // ------------------------------------------------------------------------------------------------------------------------------
                // SnpPreferUniqueFwd   | I             | I         | -         | X         | -                 | SnpResp_I
                // (In Excl)            ---------------------------------------------------------------------------------------------------------
                //                      | UC            | SC        | -         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | -         | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | -                 | SnpRespDataPtl_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SC            | SC        | -         | 0         | CompData_SC       | SnpResp_SC_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SC_Fwded_SC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SD            | SD        | -         | 0         | CompData_SC       | SnpResp_SD_Fwded_SC
                //                      |               |           |           -----------------------------------------------------------------
                //                      |               |           |           | 1         | CompData_SC       | SnpRespData_SD_Fwded_SC
                //                      |               -----------------------------------------------------------------------------------------
                //                      |               | SC        | -         | X         | CompData_SC       | SnpRespData_SC_PD_Fwded_SC
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_UCE_to_I_as_I = {
                    CacheStateTransition(UCE, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(UD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_UD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(UD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_UDP_to_I_as_I_PD = {
                    CacheStateTransition(UDP, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SC, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SC, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A0)
                            .SnpRespFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1 = {
                    CacheStateTransition(SD, SD).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::A1)
                            .SnpRespDataFwded(SD, SC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_InExcl_SD_to_SC_as_SC_PD_Fwded_SC = {
                    CacheStateTransition(SD, SC).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataFwded(SC_PD, SC)
                };
                //
                constexpr std::array SnpPreferUniqueFwd_InExcl = {
                    SnpPreferUniqueFwd_InExcl_I_to_I_as_I,
                    SnpPreferUniqueFwd_InExcl_UC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpPreferUniqueFwd_InExcl_UC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpPreferUniqueFwd_InExcl_UCE_to_I_as_I,
                    SnpPreferUniqueFwd_InExcl_UD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpPreferUniqueFwd_InExcl_UD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpPreferUniqueFwd_InExcl_UD_to_SC_as_SC_PD_Fwded_SC,
                    SnpPreferUniqueFwd_InExcl_UDP_to_I_as_I_PD,
                    SnpPreferUniqueFwd_InExcl_SC_to_SC_as_SC_Fwded_SC_RetToSrc_0,
                    SnpPreferUniqueFwd_InExcl_SC_to_SC_as_SC_Fwded_SC_RetToSrc_1,
                    SnpPreferUniqueFwd_InExcl_SD_to_SD_as_SD_Fwded_SC_RetToSrc_0,
                    SnpPreferUniqueFwd_InExcl_SD_to_SD_as_SD_Fwded_SC_RetToSrc_1,
                    SnpPreferUniqueFwd_InExcl_SD_to_SC_as_SC_PD_Fwded_SC
                };
                //

                //                            Table. State transitions for SnpPreferUniqueFwd when Snoopee is not executing an exclusive sequence
                // ==============================================================================================================================
                //                      | Snoopee       | Snoopee               |           |                                                   
                //                      | Initial       | Final                 |           | Snoopee response to                               
                //                      | cache state   | cache state           |           |                                                   
                // Request Type         |               |-----------------------| RetToSrc  |----------------------------------------------------
                //                      |               | Expected  | Others    |           | Requester         | Home                          
                //                      |               |           | permitted |           |                   |                                
                // ------------------------------------------------------------------------------------------------------------------------------
                // SnpPreferUniqueFwd   | I             | I         | -         | X         | -                 | SnpResp_I
                // (Not In Excl)        ---------------------------------------------------------------------------------------------------------
                //                      | UC            | I         | -         | X         | CompData_UC       | SnpResp_I_Fwded_UC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UCE           | I         | -         | X         | -                 | SnpResp_I
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UD            | I         | -         | X         | CompData_UD_PD    | SnpResp_I_Fwded_UD_PD
                //                      |               |           |           |           -----------------------------------------------------
                //                      |               |           |           |           | -                 | SnpRespData_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | UDP           | I         | -         | X         | -                 | SnpRespDataPtl_I_PD
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SC            | I         | -         | X         | CompData_UC       | SnpResp_I_Fwded_UC
                //                      ---------------------------------------------------------------------------------------------------------
                //                      | SD            | I         | -         | X         | CompData_UD_PD    | SnpResp_I_Fwded_UD_PD
                //                      |               |           |           |           -----------------------------------------------------
                //                      |               |           |           |           | -                 | SnpRespData_I_PD
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_I_to_I_as_I = {
                    CacheStateTransition(I, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_UC_to_I_as_I_Fwded_UC = {
                    CacheStateTransition(UC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespFwded(I, UC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_UCE_to_I_as_I = {
                    CacheStateTransition(UCE, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpResp(I)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_UD_to_I_as_I_Fwded_UD_PD = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespFwded(I, UD_PD)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_UD_to_I_as_I_PD = {
                    CacheStateTransition(UD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_UDP_to_I_as_I_PD = {
                    CacheStateTransition(UDP, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespDataPtl(I_PD)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_SC_to_I_as_I_Fwded_UC = {
                    CacheStateTransition(SC, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespFwded(I, UC)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_SD_to_I_as_I_Fwded_UD_PD = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespFwded(I, UD_PD)
                };
                constexpr CacheStateTransition SnpPreferUniqueFwd_NoExcl_SD_to_I_as_I_PD = {
                    CacheStateTransition(SD, I).TypeSnoopForward()
                        .RetToSrc(RetToSrcs::X)
                            .SnpRespData(I_PD)
                };
                //
                constexpr std::array SnpPreferUniqueFwd_NoExcl = {
                    SnpPreferUniqueFwd_NoExcl_I_to_I_as_I,
                    SnpPreferUniqueFwd_NoExcl_UC_to_I_as_I_Fwded_UC,
                    SnpPreferUniqueFwd_NoExcl_UCE_to_I_as_I,
                    SnpPreferUniqueFwd_NoExcl_UD_to_I_as_I_Fwded_UD_PD,
                    SnpPreferUniqueFwd_NoExcl_UD_to_I_as_I_PD,
                    SnpPreferUniqueFwd_NoExcl_UDP_to_I_as_I_PD,
                    SnpPreferUniqueFwd_NoExcl_SC_to_I_as_I_Fwded_UC,
                    SnpPreferUniqueFwd_NoExcl_SD_to_I_as_I_Fwded_UD_PD,
                    SnpPreferUniqueFwd_NoExcl_SD_to_I_as_I_PD
                };
			}
		}
	}
/*
}
*/


#endif
