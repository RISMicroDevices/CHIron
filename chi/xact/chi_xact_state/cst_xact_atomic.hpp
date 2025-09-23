#pragma once

#include "cst_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_XACT_ATOMIC)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_XACT_ATOMIC))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_XACT_ATOMIC
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_XACT_ATOMIC
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

			namespace Transitions {

				//                               Table. Request cache state transitions for Atomic request transcations
                // ====================================================================================================
                //                      | Cache state   | Others    |       |                    |
                // Request Type         | Initial       | Permitted | Final | WriteData response | Comp Response
                //                      | Expected      |           |       |                    |
                // ----------------------------------------------------------------------------------------------------
                // AtomicStore          | I, SC         | UC, UD    | I     | NCBWrData          | DBIDResp* + Comp_I
                //                      | UCE, SD       | UDP       |       |                    | or CompDBIDResp
                constexpr CacheStateTransition AtomicStore_I_SC_UCE_SD_or_UC_UD_UDP_to_I = {
                    CacheStateTransition(I | SC | UCE | SD, UC | UD | UDP, I).TypeAtomic()
                        .Comp(I)
                };
                //
                constexpr std::array AtomicStore = {
                    AtomicStore_I_SC_UCE_SD_or_UC_UD_UDP_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // AtomicLoad           | I, SC         | UC, UD    | I     | NCBWrData          | DBIDResp*
                //                      | UCE, SD       | UDP       |       |                    | + CompData_I
                constexpr CacheStateTransition AtomicLoad_I_SC_UCE_SD_or_UC_UD_UDP_to_I = {
                    CacheStateTransition(I | SC | UCE | SD, UC | UD | UDP, I).TypeAtomic()
                        .CompData(I)
                };
                //
                constexpr std::array AtomicLoad = {
                    AtomicLoad_I_SC_UCE_SD_or_UC_UD_UDP_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // AtomicSwap           | I, SC         | UC, UD    | I     | NCBWrData          | DBIDResp*
                //                      | UCE, SD       | UDP       |       |                    | + CompData_I
                constexpr CacheStateTransition AtomicSwap_I_SC_UCE_SD_or_UC_UD_UDP_to_I = {
                    CacheStateTransition(I | SC | UCE | SD, UC | UD | UDP, I).TypeAtomic()
                        .CompData(I)
                };
                //
                constexpr std::array AtomicSwap = {
                    AtomicSwap_I_SC_UCE_SD_or_UC_UD_UDP_to_I
                };
                // ----------------------------------------------------------------------------------------------------
                // AtomicCompare        | I, SC         | UC, UD    | I     | NCBWrData          | DBIDResp*
                //                      | UCE, SD       | UDP       |       |                    | + CompData_I
                constexpr CacheStateTransition AtomicCompare_I_SC_UCE_SD_or_UC_UD_UDP_to_I = {
                    CacheStateTransition(I | SC | UCE | SD, UC | UD | UDP, I).TypeAtomic()
                        .CompData(I)
                };
                //
                constexpr std::array AtomicCompare = {
                    AtomicCompare_I_SC_UCE_SD_or_UC_UD_UDP_to_I
                };
                // ====================================================================================================
			}
		}
	}
/*
}
*/

#endif
