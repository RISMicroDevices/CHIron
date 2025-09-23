#pragma once

#include "cst_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_XACT_DATALESS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_XACT_DATALESS))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_XACT_DATALESS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_XACT_DATALESS
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

			namespace Transitions {
				
				//                    Table. Cache state transitions at the Requester for Dataless request transactions
                // ====================================================================================================
                //                          | Cache state   | Others     |          |              
                // Request Type             | Initial       | Permitted  | Final    | Comp response
                //                          | Expected      |            |          |              
                // ----------------------------------------------------------------------------------------------------
                // CleanUnique              | I             | UC, UCE    | UCE      | Comp_UC
                //                          ---------------------------------------------------------------------------
                //                          | SC            | UC         | UC       | Comp_UC
                //                          ---------------------------------------------------------------------------
                //                          | SD            | UD         | UD       | Comp_UC
                constexpr CacheStateTransition CleanUnique_I_or_UC_UCE_to_UCE = {
                    CacheStateTransition(I, UC | UCE, UCE).TypeDataless()
                        .Comp(UC)
                };
                constexpr CacheStateTransition CleanUnique_SC_or_UC_to_UC = {
                    CacheStateTransition(SC, UC, UC).TypeDataless()
                        .Comp(UC)
                };
                constexpr CacheStateTransition CleanUnique_SD_or_UD_to_UD = {
                    CacheStateTransition(SD, UD, UD).TypeDataless()
                        .Comp(UC)
                };
                //
                constexpr std::array CleanUnique = {
                    CleanUnique_I_or_UC_UCE_to_UCE,
                    CleanUnique_SC_or_UC_to_UC,
                    CleanUnique_SD_or_UD_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // MakeUnique               | I, SC, SD     | UC, UCE    | UD       | Comp_UC
                constexpr CacheStateTransition MakeUnique_I_SC_SD_or_UC_UCE_to_UD = {
                    CacheStateTransition(I | SC | SD, UC | UCE, UD).TypeDataless()
                        .Comp(UC)
                };
                //
                constexpr std::array MakeUnique = {
                    MakeUnique_I_SC_SD_or_UC_UCE_to_UD
                };
                // ----------------------------------------------------------------------------------------------------
                // Evict                    | I             | -          | I        | Comp_I
                constexpr CacheStateTransition Evict_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless()
                        .Comp(I)
                };
                //
                constexpr std::array Evict = {
                    Evict_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // StashOnceUnique          | I             | -          | I        | Comp
                constexpr CacheStateTransition StashOnceUnique_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless()
                        .Comp(CacheResps::All)
                };
                //
                constexpr std::array StashOnceUnique = {
                    StashOnceUnique_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // StashOnceSepUnique       | I             | -          | I        | Comp + StashDone
                //                          |               |            |          | or CompStashDone
                constexpr CacheStateTransition StashOnceSepUnique_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless()
                        .Comp(CacheResps::All)
                };
                //
                constexpr std::array StashOnceSepUnique = {
                    StashOnceSepUnique_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // StashOnceShared          | I             | -          | I        | Comp
                constexpr CacheStateTransition StashOnceShared_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless()
                        .Comp(CacheResps::All)
                };
                //
                constexpr std::array StashOnceShared = {
                    StashOnceShared_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // StashOnceSepShared       | I             | -          | I        | Comp + StashDone
                //                          |               |            |          | or CompStashDone
                constexpr CacheStateTransition StashOnceSepShared_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless()
                        .Comp(CacheResps::All)
                };
                //
                constexpr std::array StashOnceSepShared = {
                    StashOnceSepShared_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // CleanShared              | I, SC, UC     | -          | No       | Comp_UC
                // CleanSharedPersist       |               |            | Change   | Comp_SC
                //                          |               |            |          | Comp_I
                constexpr CacheStateTransition CleanShared_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                constexpr CacheStateTransition CleanShared_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                constexpr CacheStateTransition CleanShared_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                constexpr CacheStateTransition CleanSharedPersist_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                constexpr CacheStateTransition CleanSharedPersist_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                constexpr CacheStateTransition CleanSharedPersist_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                //
                constexpr std::array CleanShared = {
                    CleanShared_I_to_I,
                    CleanShared_SC_to_SC,
                    CleanShared_UC_to_UC
                };
                //
                constexpr std::array CleanSharedPersist = {
                    CleanSharedPersist_I_to_I,
                    CleanSharedPersist_SC_to_SC,
                    CleanSharedPersist_UC_to_UC
                };
                // ----------------------------------------------------------------------------------------------------
                // CleanSharedPersistSep    | I, SC, UC     | -          | No       | Comp_UC + Persist
                //                          |               |            | Change   | or CompPersist_UC
                //                          |               |            |          -----------------------------------
                //                          |               |            |          | Comp_SC + Persist
                //                          |               |            |          | or CompPersist_SC
                //                          |               |            |          -----------------------------------
                //                          |               |            |          | Comp_I + Persist
                //                          |               |            |          | or CompPersist_I
                constexpr CacheStateTransition CleanSharedPersistSep_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                constexpr CacheStateTransition CleanSharedPersistSep_SC_to_SC = {
                    CacheStateTransition(SC, SC).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                constexpr CacheStateTransition CleanSharedPersistSep_UC_to_UC = {
                    CacheStateTransition(UC, UC).TypeDataless().NoChange()
                        .Comp(I | SC | UC)
                };
                //
                constexpr std::array CleanSharedPersistSep = {
                    CleanSharedPersistSep_I_to_I,
                    CleanSharedPersistSep_SC_to_SC,
                    CleanSharedPersistSep_UC_to_UC
                };
                // ----------------------------------------------------------------------------------------------------
                // CleanInvalid             | I             | -          | I        | Comp_I
                constexpr CacheStateTransition CleanInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless()
                        .Comp(I)
                };
                //
                constexpr std::array CleanInvalid = {
                    CleanInvalid_I_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // MakeInvalid              | I             | -          | I        | Comp_I
                constexpr CacheStateTransition MakeInvalid_I_to_I = {
                    CacheStateTransition(I, I).TypeDataless()
                        .Comp(I)
                };
                //
                constexpr std::array MakeInvalid = {
                    MakeInvalid_I_to_I
                };
                // ====================================================================================================
			}
		}
	}
/*
}
*/

#endif