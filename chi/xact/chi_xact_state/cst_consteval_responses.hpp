#pragma once

#include "cst_consteval_intermediates.hpp"
#include "cst_xact_read.hpp"
#include "cst_xact_dataless.hpp"
#include "cst_xact_write.hpp"
#include "cst_xact_atomic.hpp"
#include "cst_xact_snoop.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_CONSTEVAL_RESPONSES)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_CONSTEVAL_RESPONSES))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_CONSTEVAL_RESPONSES
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_CONSTEVAL_RESPONSES
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

            //
            namespace Responses {

                namespace details {

                    struct TableR0 {

                        union {
                            std::array<CacheResp, 7>    resps;
                            struct {
                                uint64_t                i64_0;
                                uint64_t                i64_1;
                            };
                        };

                        inline constexpr TableR0() noexcept : resps({
                            CacheResps::None, CacheResps::None, CacheResps::None, CacheResps::None,
                            CacheResps::None, CacheResps::None, CacheResps::None
                        }) {}
                    };

                    template<CacheState S>
                    inline consteval CacheState _NextState() noexcept
                    {
                        // C++20 is stupid to handle constexpr union here,
                        // so we have only this ungly choice
                        if (!S      ) return CacheStates::UC;
                        if (S.UC    ) return CacheStates::UCE;
                        if (S.UCE   ) return CacheStates::UD;
                        if (S.UD    ) return CacheStates::UDP;
                        if (S.UDP   ) return CacheStates::SC;
                        if (S.SC    ) return CacheStates::SD;
                        if (S.SD    ) return CacheStates::I;
                        return CacheStates::None;
                    }

                    inline constexpr size_t GetStateTableIndex(CacheState x) noexcept
                    {
                        return (
                            (((1ULL & ~x.UC     ) - 1) &  0ULL)
                          | (((1ULL & ~x.UCE    ) - 1) &  1ULL)
                          | (((1ULL & ~x.UD     ) - 1) &  2ULL)
                          | (((1ULL & ~x.UDP    ) - 1) &  3ULL)
                          | (((1ULL & ~x.SC     ) - 1) &  4ULL)
                          | (((1ULL & ~x.SD     ) - 1) &  5ULL)
                          | (((1ULL & ~x.I      ) - 1) &  6ULL)
                        );
                    }

                    inline constexpr std::pair<uint64_t, uint64_t> GetStateFactorForR0(CacheState x)
                    {
                        return
                            std::make_pair(
                                (
                                    ((((1ULL & ~x.UC        ) - 1) & 0xFFFFULL) << (GetStateTableIndex(CacheStates::UC    ) * 16))
                                  | ((((1ULL & ~x.UCE       ) - 1) & 0xFFFFULL) << (GetStateTableIndex(CacheStates::UCE   ) * 16))
                                  | ((((1ULL & ~x.UD        ) - 1) & 0xFFFFULL) << (GetStateTableIndex(CacheStates::UD    ) * 16))
                                  | ((((1ULL & ~x.UDP       ) - 1) & 0xFFFFULL) << (GetStateTableIndex(CacheStates::UDP   ) * 16))
                                ),
                                (
                                    ((((1ULL & ~x.SC        ) - 1) & 0xFFFFULL) << ((GetStateTableIndex(CacheStates::SC    ) - 4) * 16))
                                  | ((((1ULL & ~x.SD        ) - 1) & 0xFFFFULL) << ((GetStateTableIndex(CacheStates::SD    ) - 4) * 16))
                                  | ((((1ULL & ~x.I         ) - 1) & 0xFFFFULL) << ((GetStateTableIndex(CacheStates::I     ) - 4) * 16))
                                )
                            );
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheState S>
                    inline consteval TableR0 GetTableR0CompDataDCT(TableR0 A = TableR0()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheResp resp = CacheResps::None;
                            for (CacheStateTransition T : Ts)
                                if (T.GetPermittedInitials() & S)
                                    resp = resp | T.respCompData;

                            A.resps[GetStateTableIndex(S)] = resp;

                            return GetTableR0CompDataDCT<N, Ts, _NextState<S>()>(A);
                        }
                        else
                            return A;
                    }

                    inline constexpr CacheResp GetRespFromR0(TableR0 e)
                    {
                        return std::reduce(e.resps.begin(), e.resps.end(), CacheResps::None, std::bit_or<>());
                    }

                    inline constexpr CacheResp GetRespFromR0(uint64_t ei0, uint64_t ei1)
                    {
                        TableR0 e;
                        e.i64_0 = ei0;
                        e.i64_1 = ei1;
                        return GetRespFromR0(e);
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableR0 GetTableR0CompDataDCT() noexcept
                    {
                        return GetTableR0CompDataDCT<N, Ts, _NextState<CacheStates::None>()>();
                    }

                    inline constexpr CacheResp ProductR0(const CacheState P, const TableR0& R0) noexcept
                    {
                        auto px = GetStateFactorForR0(P);
                        auto rx_0 = R0.i64_0;
                        auto rx_1 = R0.i64_1;

                        return GetRespFromR0(
                            px.first  & rx_0,
                            px.second & rx_1
                        );
                    }
                }

                #define _Table_DCT(opcode)     details::GetTableR0CompDataDCT<Transitions::opcode.size(), Transitions::opcode>()

                // Response states for Snoop Forward transactions
                constexpr details::TableR0   SnpOnceFwd                  = _Table_DCT(SnpOnceFwd);
                constexpr details::TableR0   SnpCleanFwd                 = _Table_DCT(SnpCleanFwd);
                constexpr details::TableR0   SnpNotSharedDirtyFwd        = _Table_DCT(SnpNotSharedDirtyFwd);
                constexpr details::TableR0   SnpSharedFwd                = _Table_DCT(SnpSharedFwd);
                constexpr details::TableR0   SnpUniqueFwd                = _Table_DCT(SnpUniqueFwd);
                constexpr details::TableR0   SnpPreferUniqueFwd_NoExcl   = _Table_DCT(SnpPreferUniqueFwd_NoExcl);
                constexpr details::TableR0   SnpPreferUniqueFwd_InExcl   = _Table_DCT(SnpPreferUniqueFwd_InExcl);

                #undef _Table_DCT
            }
		}
	}
/*
}
*/

#endif
