#pragma once

#include "cst_xact_read.hpp"
#include "cst_xact_dataless.hpp"
#include "cst_xact_write.hpp"
#include "cst_xact_atomic.hpp"
#include "cst_xact_snoop.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_CONSTEVAL_INTERMEDIATES)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_CONSTEVAL_INTERMEDIATES))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_CONSTEVAL_INTERMEDIATES
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_CONSTEVAL_INTERMEDIATES
#endif

/*
namespace CHI {
*/
	namespace Xact {
		namespace CacheStateTransitions {

			//
            namespace Intermediates {

                namespace details {

                    template<size_t I, class T, size_t M>
                    inline consteval std::array<T, M> CopyOnWrite(std::array<T, M> a, T val) noexcept
                    {
                        std::array<T, M> result = a;
                        result[I] = val;
                        return result;
                    }

                    template<size_t I, class T, size_t M, std::array<T, M> a, T val>
                    inline consteval std::array<T, M> CopyOnWrite() noexcept
                    {
                        std::array<T, M> result = a;
                        result[I] = val;
                        return result;
                    }

                    template<size_t I, size_t S, class T, size_t M, size_t N>
                    inline consteval std::array<T, M> CopyOnWriteAll(std::array<T, M> dst, std::array<T, N> src) noexcept
                    {
                        std::array<T, M> result = dst;
                        for (size_t i = 0; i < S; i++)
                            result[I + i] = src[i];
                        return result;
                    }

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

                    template<CacheResp R>
                    inline consteval CacheResp _NextResp() noexcept
                    {
                        if (!R      ) return CacheResps::UC;
                        if (R.UC    ) return CacheResps::UCE;
                        if (R.UCE   ) return CacheResps::UD;
                        if (R.UD    ) return CacheResps::UDP;
                        if (R.UDP   ) return CacheResps::SC;
                        if (R.SC    ) return CacheResps::SD;
                        if (R.SD    ) return CacheResps::I;
                        if (R.I     ) return CacheResps::UC_PD;
                        if (R.UC_PD ) return CacheResps::UCE_PD;
                        if (R.UCE_PD) return CacheResps::UD_PD;
                        if (R.UD_PD ) return CacheResps::UDP_PD;
                        if (R.UDP_PD) return CacheResps::SC_PD;
                        if (R.SC_PD ) return CacheResps::SD_PD;
                        if (R.SD_PD ) return CacheResps::I_PD;
                        return CacheResps::None;
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

                    inline constexpr size_t GetRespTableIndex(CacheResp x) noexcept
                    {
                        return (
                            (((1ULL & ~x.UC     ) - 1) &  0ULL)
                          | (((1ULL & ~x.UCE    ) - 1) &  1ULL)
                          | (((1ULL & ~x.UD     ) - 1) &  2ULL)
                          | (((1ULL & ~x.UDP    ) - 1) &  3ULL)
                          | (((1ULL & ~x.SC     ) - 1) &  4ULL)
                          | (((1ULL & ~x.SD     ) - 1) &  5ULL)
                          | (((1ULL & ~x.I      ) - 1) &  6ULL)
                          | (((1ULL & ~x.UC_PD  ) - 1) &  7ULL)
                          | (((1ULL & ~x.UCE_PD ) - 1) &  8ULL)
                          | (((1ULL & ~x.UD_PD  ) - 1) &  9ULL)
                          | (((1ULL & ~x.UDP_PD ) - 1) & 10ULL)
                          | (((1ULL & ~x.SC_PD  ) - 1) & 11ULL)
                          | (((1ULL & ~x.SD_PD  ) - 1) & 12ULL)
                          | (((1ULL & ~x.I_PD   ) - 1) & 13ULL)
                        );
                    }

                    /**/
                    // Table G generations
                    struct TableG0Element {

                        union {
                            std::array<CacheState, 7>   states;
                            uint64_t                    i64;
                        };

                        inline constexpr TableG0Element() noexcept : states({
                            CacheStates::None, CacheStates::None, CacheStates::None, CacheStates::None,
                            CacheStates::None, CacheStates::None, CacheStates::None
                        }) {}
                    };

                    struct TableG1Element {

                        union {
                            std::array<CacheResp, 7>    resps;
                            struct {
                                uint64_t                i64_0;
                                uint64_t                i64_1;
                            };
                        };

                        inline constexpr TableG1Element() noexcept : resps({
                            CacheResps::None, CacheResps::None, CacheResps::None, CacheResps::None,
                            CacheResps::None, CacheResps::None, CacheResps::None
                        }) {}
                    };

                    struct TableG2Element {

                        union {
                            std::array<CacheState, 7>   states;
                            uint64_t                    i64;
                        };
                        
                        inline constexpr TableG2Element() noexcept : states({
                            CacheStates::None, CacheStates::None, CacheStates::None, CacheStates::None,
                            CacheStates::None, CacheStates::None, CacheStates::None
                        }) {}
                    };

                    // !NOTICE: Never use TableG for initial state checks.
                    /*
                    * P  = Initial states
                    * Pi = Intermediate states
                    *
                    * Xr = Response state
                    * Xf = Forward state
                    *
                    * x  = Final checker, fail on empty (might indicate any type of states, but for proving and detection)
                    * xs = Final state, fail on empty
                    * xf = Final forwarded state, fail on empty
                    * xi = Final intermediate state, fall-through on non-V, fail on empty
                    *
                    * Table G0 usage:
                    *   1. P * G0(Xr) = xs
                    *
                    * Table G1 usage:
                    *   1. P * G1(Xr) = xf
                    *   2. P * G1(Xr) * Xf = xf
                    *
                    * Table G2 usage:
                    *   1. P * G2(Pi) = xi
                    */
                    using TableG0 = std::array<TableG0Element, 14>;
                    using TableG1 = std::array<TableG1Element, 14>;
                    using TableG2 = struct _TableG2 {
                        bool                            V   = false;
                        std::array<TableG2Element, 7>   E   = {};
                    };

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0CompElement(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.GetPermittedInitials() & S) && (T.respComp & R))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0CompElement<N, Ts, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0CompElement() noexcept
                    {
                        return GetTableG0CompElement<N, Ts, R, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0CompDataElement(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.GetPermittedInitials() & S) && (T.respCompData & R))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0CompDataElement<N, Ts, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0CompDataElement() noexcept
                    {
                        return GetTableG0CompDataElement<N, Ts, R, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0DataSepRespElement(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.GetPermittedInitials() & S) && (T.respDataSepResp & R))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0DataSepRespElement<N, Ts, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0DataSepRespElement() noexcept
                    {
                        return GetTableG0DataSepRespElement<N, Ts, R, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0CopyBackWrDataElement(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            // NOTICE: 'initialPermittedState' here stands for state 'before WriteData response'.
                            //         Since initial state checks were not done by TableG0, and cacheable writes
                            //         have narrower initial state space than that of 'before WriteData response',
                            //         the input state of TableG0 simply takes 'before WriteData response' states
                            //         for transition checks.
                            
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialPermittedState & S) && (T.respCopyBackWrData & R))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0CopyBackWrDataElement<N, Ts, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0CopyBackWrDataElement() noexcept
                    {
                        return GetTableG0CopyBackWrDataElement<N, Ts, R, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0MakeReadUniqueCompElement(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            // NOTICE: 'initialPermittedState' here stands for state 'at time of response'.
                            //         Since initial state checks were not done by TableG0, and cacheable writes
                            //         have narrower initial state space than that of 'at time of response',
                            //         the input state of TableG0 simply takes 'at time of response' states
                            //         for transition checks.

                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialPermittedState & S) && (T.respComp & R))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0MakeReadUniqueCompElement<N, Ts, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0MakeReadUniqueCompElement() noexcept
                    {
                        return GetTableG0MakeReadUniqueCompElement<N, Ts, R, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0MakeReadUniqueCompDataElement(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            // NOTICE: @see: GetTableG0MakeReadUniqueCompElement

                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialPermittedState & S) && (T.respCompData & R))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0MakeReadUniqueCompDataElement<N, Ts, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0MakeReadUniqueCompDataElement() noexcept
                    {
                        return GetTableG0MakeReadUniqueCompDataElement<N, Ts, R, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0MakeReadUniqueDataSepRespElement(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            // NOTICE: @see: GetTableG0MakeReadUniqueCompElement

                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialPermittedState & S) && (T.respDataSepResp & R))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0MakeReadUniqueDataSepRespElement<N, Ts, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0MakeReadUniqueDataSepRespElement() noexcept
                    {
                        return GetTableG0MakeReadUniqueDataSepRespElement<N, Ts, R, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0SnpRespElement(RetToSrc retToSrc, TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialExpectedState & S) && (T.respSnpResp & R) && (T.retToSrc & retToSrc))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0SnpRespElement<N, Ts, R, _NextState<S>()>(retToSrc, E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0SnpRespElement(RetToSrc retToSrc) noexcept
                    {
                        return GetTableG0SnpRespElement<N, Ts, R, _NextState<CacheStates::None>()>(retToSrc);
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0SnpRespDataElement(RetToSrc retToSrc, TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialExpectedState & S) && (T.respSnpRespData & R) && (T.retToSrc & retToSrc))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0SnpRespDataElement<N, Ts, R, _NextState<S>()>(retToSrc, E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0SnpRespDataElement(RetToSrc retToSrc) noexcept
                    {
                        return GetTableG0SnpRespDataElement<N, Ts, R, _NextState<CacheStates::None>()>(retToSrc);
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0SnpRespDataPtlElement(RetToSrc retToSrc, TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialExpectedState & S) && (T.respSnpRespData & R) && (T.retToSrc & retToSrc))
                                    state = state | T.finalState;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0SnpRespDataElement<N, Ts, R, _NextState<S>()>(retToSrc, E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG0Element GetTableG0SnpRespDataPtlElement(RetToSrc retToSrc) noexcept
                    {
                        return GetTableG0SnpRespDataPtlElement<N, Ts, R, _NextState<CacheStates::None>()>(retToSrc);
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG1Element GetTableG1SnpRespFwdedElement(RetToSrc retToSrc, TableG1Element E = TableG1Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheResp resp = CacheResps::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialExpectedState & S) && (T.respSnpResp & R) && (T.retToSrc & retToSrc))
                                    resp = resp | T.respCompData;

                            E.resps[GetStateTableIndex(S)] = resp;

                            return GetTableG1SnpRespFwdedElement<N, Ts, R, _NextState<S>()>(retToSrc, E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG1Element GetTableG1SnpRespFwdedElement(RetToSrc retToSrc) noexcept
                    {
                        return GetTableG1SnpRespFwdedElement<N, Ts, R, _NextState<CacheStates::None>()>(retToSrc);
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R, CacheState S>
                    inline consteval TableG1Element GetTableG1SnpRespDataFwdedElement(RetToSrc retToSrc, TableG1Element E = TableG1Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheResp resp = CacheResps::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialExpectedState & S) && (T.respSnpRespData & R) && (T.retToSrc & retToSrc))
                                    resp = resp | T.respCompData;

                            E.resps[GetStateTableIndex(S)] = resp;

                            return GetTableG1SnpRespDataFwdedElement<N, Ts, R, _NextState<S>()>(retToSrc, E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp R>
                    inline consteval TableG1Element GetTableG1SnpRespDataFwdedElement(RetToSrc retToSrc) noexcept
                    {
                        return GetTableG1SnpRespDataFwdedElement<N, Ts, R, _NextState<CacheStates::None>()>(retToSrc);
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheState Si, CacheState S>
                    inline consteval TableG2Element GetTableG2MakeReadUniqueElement(TableG2Element E = TableG2Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialExpectedState & S) && (T.initialPermittedState & Si))
                                    state = state | Si;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG2MakeReadUniqueElement<N, Ts, Si, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheState Si>
                    inline consteval TableG2Element GetTableG2MakeReadUniqueElement() noexcept
                    {
                        return GetTableG2MakeReadUniqueElement<N, Ts, Si, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheState Si, CacheState S>
                    inline consteval TableG2Element GetTableG2WriteElement(TableG2Element E = TableG2Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initialExpectedState & S) && (T.initialPermittedState & Si))
                                    state = state | Si;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG2WriteElement<N, Ts, Si, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheState Si>
                    inline consteval TableG2Element GetTableG2WriteElement() noexcept
                    {
                        return GetTableG2WriteElement<N, Ts, Si, _NextState<CacheStates::None>()>();
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0Comp() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0CompElement<N, Ts, R>());

                            return GetTableG0Comp<N, Ts, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0CompData() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0CompDataElement<N, Ts, R>());

                            return GetTableG0CompData<N, Ts, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0DataSepResp() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0DataSepRespElement<N, Ts, R>());

                            return GetTableG0DataSepResp<N, Ts, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0CopyBackWrData() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A,
                                GetTableG0CopyBackWrDataElement<N, Ts, R>());

                            return GetTableG0CopyBackWrData<N, Ts, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0MakeReadUniqueComp() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0MakeReadUniqueCompElement<N, Ts, R>());

                            return GetTableG0MakeReadUniqueComp<N, Ts, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0MakeReadUniqueCompData() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0MakeReadUniqueCompDataElement<N, Ts, R>());

                            return GetTableG0MakeReadUniqueCompData<N, Ts, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0MakeReadUniqueDataSepResp() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0MakeReadUniqueDataSepRespElement<N, Ts, R>());

                            return GetTableG0MakeReadUniqueDataSepResp<N, Ts, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        RetToSrc retToSrc,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0SnpResp() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0SnpRespElement<N, Ts, R>(retToSrc));

                            return GetTableG0SnpResp<N, Ts, retToSrc, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        RetToSrc retToSrc,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0SnpRespData() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG0SnpRespDataElement<N, Ts, R>(retToSrc));

                            return GetTableG0SnpRespData<N, Ts, retToSrc, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        RetToSrc retToSrc,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0SnpRespDataPtl() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A,
                                GetTableG0SnpRespDataPtlElement<N, Ts, R>(retToSrc));

                            return GetTableG0SnpRespDataPtl<N, Ts, retToSrc, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        RetToSrc retToSrc,
                        CacheResp R,
                        TableG1 A = TableG1()>
                    inline consteval TableG1 GetTableG1SnpRespFwded() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG1SnpRespFwdedElement<N, Ts, R>(retToSrc));

                            return GetTableG1SnpRespFwded<N, Ts, retToSrc, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        RetToSrc retToSrc,
                        CacheResp R,
                        TableG1 A = TableG1()>
                    inline consteval TableG1 GetTableG1SnpRespDataFwded() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A, 
                                GetTableG1SnpRespDataFwdedElement<N, Ts, R>(retToSrc));

                            return GetTableG1SnpRespDataFwded<N, Ts, retToSrc, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheState Si,
                        TableG2 A = TableG2()>
                    inline consteval TableG2 GetTableG2MakeReadUnique() noexcept
                    {
                        if constexpr (Si)
                        {
                            constexpr auto nextA = CopyOnWrite<GetStateTableIndex(Si)>(A.E,
                                GetTableG2MakeReadUniqueElement<N, Ts, Si>());

                            return GetTableG2MakeReadUnique<N, Ts, _NextState<Si>(), TableG2(true, nextA)>();
                        }
                        else
                            return A;
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheState Si,
                        TableG2 A = TableG2()>
                    inline consteval TableG2 GetTableG2Write() noexcept
                    {
                        if constexpr (Si)
                        {
                            constexpr auto nextA = CopyOnWrite<GetStateTableIndex(Si)>(A.E,
                                GetTableG2WriteElement<N, Ts, Si>());

                            return GetTableG2Write<N, Ts, _NextState<Si>(), TableG2(true, nextA)>();
                        }
                        else
                            return A;
                    }

                    //
                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG0 GetTableG0Comp() noexcept
                    {
                        return GetTableG0Comp<N, Ts, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG0 GetTableG0CompData() noexcept
                    {
                        return GetTableG0CompData<N, Ts, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG0 GetTableG0DataSepResp() noexcept
                    {
                        return GetTableG0DataSepResp<N, Ts, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG0 GetTableG0CopyBackWrData() noexcept
                    {
                        return GetTableG0CopyBackWrData<N, Ts, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG0 GetTableG0MakeReadUniqueComp() noexcept
                    {
                        return GetTableG0MakeReadUniqueComp<N, Ts, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG0 GetTableG0MakeReadUniqueCompData() noexcept
                    {
                        return GetTableG0MakeReadUniqueCompData<N, Ts, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG0 GetTableG0MakeReadUniqueDataSepResp() noexcept
                    {
                        return GetTableG0MakeReadUniqueDataSepResp<N, Ts, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, RetToSrc retToSrc>
                    inline consteval TableG0 GetTableG0SnpResp() noexcept
                    {
                        return GetTableG0SnpResp<N, Ts, retToSrc, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, RetToSrc retToSrc>
                    inline consteval TableG0 GetTableG0SnpRespData() noexcept
                    {
                        return GetTableG0SnpRespData<N, Ts, retToSrc, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, RetToSrc retToSrc>
                    inline consteval TableG0 GetTableG0SnpRespDataPtl() noexcept
                    {
                        return GetTableG0SnpRespDataPtl<N, Ts, retToSrc, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, RetToSrc retToSrc>
                    inline consteval TableG1 GetTableG1SnpRespFwded() noexcept
                    {
                        return GetTableG1SnpRespFwded<N, Ts, retToSrc, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, RetToSrc retToSrc>
                    inline consteval TableG1 GetTableG1SnpRespDataFwded() noexcept
                    {
                        return GetTableG1SnpRespDataFwded<N, Ts, retToSrc, _NextResp<CacheResps::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG2 GetTableG2MakeReadUnique() noexcept
                    {
                        return GetTableG2MakeReadUnique<N, Ts, _NextState<CacheStates::None>()>();
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG2 GetTableG2Write() noexcept
                    {
                        return GetTableG2Write<N, Ts, _NextState<CacheStates::None>()>();
                    }
                    /**/

                    //
                    inline constexpr uint64_t GetStateFactorForG0(CacheState x)
                    {
                        return (
                            ((((1ULL & ~x.UC        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UC    ) * 8))
                          | ((((1ULL & ~x.UCE       ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UCE   ) * 8))
                          | ((((1ULL & ~x.UD        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UD    ) * 8))
                          | ((((1ULL & ~x.UDP       ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UDP   ) * 8))
                          | ((((1ULL & ~x.SC        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::SC    ) * 8))
                          | ((((1ULL & ~x.SD        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::SD    ) * 8))
                          | ((((1ULL & ~x.I         ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::I     ) * 8))
                        );
                    }

                    inline constexpr std::pair<uint64_t, uint64_t> GetStateFactorForG1(CacheState x)
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

                    inline constexpr uint64_t GetStateFactorForG2(CacheState x)
                    {
                        return (
                            ((((1ULL & ~x.UC        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UC    ) * 8))
                          | ((((1ULL & ~x.UCE       ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UCE   ) * 8))
                          | ((((1ULL & ~x.UD        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UD    ) * 8))
                          | ((((1ULL & ~x.UDP       ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::UDP   ) * 8))
                          | ((((1ULL & ~x.SC        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::SC    ) * 8))
                          | ((((1ULL & ~x.SD        ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::SD    ) * 8))
                          | ((((1ULL & ~x.I         ) - 1) & 0xFFULL) << (GetStateTableIndex(CacheStates::I     ) * 8))
                        );
                    }

                    inline constexpr CacheState GetStateFromG0Element(TableG0Element e)
                    {
                        return std::reduce(e.states.begin(), e.states.end(), CacheStates::None, std::bit_or<>());
                    }

                    inline constexpr CacheState GetStateFromG0Element(uint64_t ei)
                    {
                        TableG0Element e;
                        e.i64 = ei;
                        return GetStateFromG0Element(e);
                    }

                    inline constexpr CacheResp GetRespFromG1Element(TableG1Element e)
                    {
                        return std::reduce(e.resps.begin(), e.resps.end(), CacheResps::None, std::bit_or<>());
                    }

                    inline constexpr CacheResp GetRespFromG1Element(uint64_t ei0, uint64_t ei1)
                    {
                        TableG1Element e;
                        e.i64_0 = ei0;
                        e.i64_1 = ei1;
                        return GetRespFromG1Element(e);
                    }

                    inline constexpr CacheState GetStateFromG2Element(TableG0Element e)
                    {
                        return std::reduce(e.states.begin(), e.states.end(), CacheStates::None, std::bit_or<>());
                    }

                    inline constexpr CacheState GetStateFromG2Element(uint64_t ei)
                    {
                        TableG0Element e;
                        e.i64 = ei;
                        return GetStateFromG2Element(e);
                    }

                    inline constexpr CacheState ProductG0(const CacheState P, const TableG0& G0, const CacheResp Xr) noexcept
                    {
                        if (Xr.UC && Xr.UD)
                        {
                            assert(std::popcount(Xr.i16) == 2);

                            auto px = GetStateFactorForG0(P);
                            auto rx0 = G0[GetRespTableIndex(CacheResps::UC)].i64;
                            auto rx1 = G0[GetRespTableIndex(CacheResps::UD)].i64;
                            
                            return GetStateFromG0Element(px & (rx0 | rx1));
                        }
                        else
                        {
                            assert(std::popcount(Xr.i16) == 1);

                            auto px = GetStateFactorForG0(P);
                            auto rx = G0[GetRespTableIndex(Xr)].i64;

                            return GetStateFromG0Element(px & rx);
                        }
                    }

                    inline constexpr CacheResp ProductG1(const CacheState P, const TableG1& G1, const CacheResp Xr) noexcept
                    {
                        if (Xr.UC && Xr.UD)
                        {
                            assert(std::popcount(Xr.i16) == 2);

                            auto px = GetStateFactorForG1(P);
                            auto rx0_0 = G1[GetRespTableIndex(CacheResps::UC)].i64_0;
                            auto rx0_1 = G1[GetRespTableIndex(CacheResps::UC)].i64_1;
                            auto rx1_0 = G1[GetRespTableIndex(CacheResps::UD)].i64_0;
                            auto rx1_1 = G1[GetRespTableIndex(CacheResps::UD)].i64_1;

                            return GetRespFromG1Element(
                                px.first  & (rx0_0 | rx1_0),
                                px.second & (rx0_1 | rx1_1)
                            );
                        }
                        else
                        {
                            assert(std::popcount(Xr.i16) == 1);

                            auto px = GetStateFactorForG1(P);
                            auto rx_0 = G1[GetRespTableIndex(CacheResps::UC)].i64_0;
                            auto rx_1 = G1[GetRespTableIndex(CacheResps::UD)].i64_1;

                            return GetRespFromG1Element(
                                px.first  & rx_0,
                                px.second & rx_1
                            );
                        }
                    }

                    inline constexpr CacheState ProductG2(const CacheState P, const TableG2& G2, const CacheState Pi) noexcept
                    {
                        if (G2.V)
                        {
                            assert(std::popcount(Pi.i8) == 1);

                            auto px = details::GetStateFactorForG2(P);
                            auto rx = G2.E[details::GetStateTableIndex(Pi)].i64;

                            return GetStateFromG2Element(px & rx);
                        }
                        else
                            return Pi;
                    }
                }

                struct Tables {
                    enum class Type {
                        Read,
                        MakeReadUnique,
                        Dataless,
                        Write,
                        Atomic,
                        SnpX,
                        SnpXFwd,
                        WriteNonCopyBack
                    } type;

                    inline constexpr Tables(Type t) noexcept : type(t) {}
                };

                struct TablesRead : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // CompData
                    details::TableG0 g1; // DataSepResp

                    inline constexpr TablesRead() noexcept 
                        : Tables(Tables::Type::Read) {}

                    inline constexpr TablesRead(details::TableG0 g0, details::TableG0 g1) noexcept
                        : Tables(Tables::Type::Read), g0(g0), g1(g1) {}

                    inline constexpr const details::TableG0& GCompData() const noexcept
                    { return g0; }

                    inline constexpr const details::TableG0& GDataSepResp() const noexcept
                    { return g1; }
                };

                struct TablesMakeReadUnique : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // Comp
                    details::TableG0 g1; // CompData
                    details::TableG0 g2; // DataSepResp

                    inline constexpr TablesMakeReadUnique() noexcept 
                        : Tables(Tables::Type::MakeReadUnique) {}
                    
                    inline constexpr TablesMakeReadUnique(details::TableG0 g0, details::TableG0 g1, details::TableG0 g2) noexcept
                        : Tables(Tables::Type::MakeReadUnique), g0(g0), g1(g1), g2(g2) {}

                    inline constexpr const details::TableG0& GComp() const noexcept
                    { return g0; }

                    inline constexpr const details::TableG0& GCompData() const noexcept
                    { return g1; }

                    inline constexpr const details::TableG0& GDataSepResp() const noexcept
                    { return g2; }
                };

                struct TablesDataless : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // Comp

                    inline constexpr TablesDataless() noexcept 
                        : Tables(Tables::Type::Dataless) {}

                    inline constexpr TablesDataless(details::TableG0 g) noexcept
                        : Tables(Tables::Type::Dataless), g0(g) {}

                    inline constexpr const details::TableG0& GComp() const noexcept
                    { return g0; }
                };

                struct TablesWrite : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // CopyBackWrData

                    inline constexpr TablesWrite() noexcept 
                        : Tables(Tables::Type::Write) {}

                    inline constexpr TablesWrite(details::TableG0 g) noexcept
                        : Tables(Tables::Type::Write), g0(g) {}

                    inline constexpr const details::TableG0& GCopyBackWrData() const noexcept
                    { return g0; }
                };
                
                struct TablesWriteNonCopyBack : public Tables {
                    inline constexpr TablesWriteNonCopyBack() noexcept
                        : Tables(Tables::Type::WriteNonCopyBack) {}
                };

                struct TablesAtomic : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // Comp
                    details::TableG0 g1; // CompData

                    inline constexpr TablesAtomic() noexcept 
                        : Tables(Tables::Type::Atomic) {}

                    inline constexpr TablesAtomic(details::TableG0 g0, details::TableG0 g1) noexcept
                        : Tables(Tables::Type::Atomic), g0(g0), g1(g1) {}

                    inline constexpr const details::TableG0& GComp() const noexcept
                    { return g0; }

                    inline constexpr const details::TableG0& GCompData() const noexcept
                    { return g1; }
                };

                struct TablesSnpX : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // SnpResp (RetToSrc = 0)
                    details::TableG0 g1; // SnpResp (RetToSrc = 1)
                    details::TableG0 g2; // SnpRespData (RetToSrc = 0)
                    details::TableG0 g3; // SnpRespData (RetToSrc = 1)
                    details::TableG0 g4; // SnpRespDataPtl (RetToSrc = 0)
                    details::TableG0 g5; // SnpRespDataPtl (RetToSrc = 1)

                    inline constexpr TablesSnpX() noexcept 
                        : Tables(Tables::Type::SnpX) {}

                    inline constexpr TablesSnpX(details::TableG0 g0, details::TableG0 g1, details::TableG0 g2, details::TableG0 g3, details::TableG0 g4, details::TableG0 g5) noexcept
                        : Tables(Tables::Type::SnpX), g0(g0), g1(g1), g2(g2), g3(g3), g4(g4), g5(g5) {}

                    inline constexpr const details::TableG0& GSnpResp_RetToSrc_0() const noexcept
                    { return g0; }

                    inline constexpr const details::TableG0& GSnpResp_RetToSrc_1() const noexcept
                    { return g1; }

                    inline constexpr const details::TableG0& GSnpResp(bool retToSrc) const noexcept
                    {
                        return retToSrc ? GSnpResp_RetToSrc_1() : GSnpResp_RetToSrc_0();
                    }

                    inline constexpr const details::TableG0& GSnpRespData_RetToSrc_0() const noexcept
                    { return g2; }

                    inline constexpr const details::TableG0& GSnpRespData_RetToSrc_1() const noexcept
                    { return g3; }

                    inline constexpr const details::TableG0& GSnpRespData(bool retToSrc) const noexcept
					{
                        return retToSrc ? GSnpRespData_RetToSrc_1() : GSnpRespData_RetToSrc_0();
                    }

                    inline constexpr const details::TableG0& GSnpRespDataPtl_RetToSrc_0() const noexcept
                    { return g4; }

                    inline constexpr const details::TableG0& GSnpRespDataPtl_RetToSrc_1() const noexcept
                    { return g5; }

                    inline constexpr const details::TableG0& GSnpRespDataPtl(bool retToSrc) const noexcept
                    {
                        return retToSrc ? GSnpRespDataPtl_RetToSrc_1() : GSnpRespDataPtl_RetToSrc_0();
                    }
                };

                struct TablesSnpXFwd : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // SnpResp (RetToSrc = 0)
                    details::TableG0 g1; // SnpResp (RetToSrc = 1)
                    details::TableG0 g2; // SnpRespData (RetToSrc = 0)
                    details::TableG0 g3; // SnpRespData (RetToSrc = 1)
                    details::TableG1 g4; // SnpRespFwded (RetToSrc = 0)
                    details::TableG1 g5; // SnpRespFwded (RetToSrc = 1)
                    details::TableG1 g6; // SnpRespDataFwded (RetToSrc = 0)
                    details::TableG1 g7; // SnpRespDataFwded (RetToSrc = 1)
                    details::TableG0 g8; // SnpRespDataPtl (RetToSrc = 0)
                    details::TableG0 g9; // SnpRespDataPtl (RetToSrc = 1)

                    inline constexpr TablesSnpXFwd() noexcept 
                        : Tables(Tables::Type::SnpXFwd) {}
                    
                    inline constexpr TablesSnpXFwd(details::TableG0 g0, details::TableG0 g1, details::TableG0 g2, details::TableG0 g3,
                                                  details::TableG1 g4, details::TableG1 g5, details::TableG1 g6, details::TableG1 g7,
                                                  details::TableG0 g8, details::TableG0 g9) noexcept
                        : Tables(Tables::Type::SnpXFwd), g0(g0), g1(g1), g2(g2), g3(g3), g4(g4), g5(g5), g6(g6), g7(g7), g8(g8), g9(g9) {}
                
                    inline constexpr const details::TableG0& GSnpResp_RetToSrc_0() const noexcept
                    { return g0; }

                    inline constexpr const details::TableG0& GSnpResp_RetToSrc_1() const noexcept
                    { return g1; }

                    inline constexpr const details::TableG0& GSnpResp(bool retToSrc) const noexcept
					{
						return retToSrc ? GSnpResp_RetToSrc_1() : GSnpResp_RetToSrc_0();
					}

                    inline constexpr const details::TableG0& GSnpRespData_RetToSrc_0() const noexcept
                    { return g2; }

                    inline constexpr const details::TableG0& GSnpRespData_RetToSrc_1() const noexcept
                    { return g3; }

                    inline constexpr const details::TableG0& GSnpRespData(bool retToSrc) const noexcept
                    {
						return retToSrc ? GSnpRespData_RetToSrc_1() : GSnpRespData_RetToSrc_0();
                    }

                    inline constexpr const details::TableG1& GSnpRespFwded_RetToSrc_0() const noexcept
                    { return g4; }

                    inline constexpr const details::TableG1& GSnpRespFwded_RetToSrc_1() const noexcept
                    { return g5; }

                    inline constexpr const details::TableG1& GSnpRespFwded(bool retToSrc) const noexcept
                    {
						return retToSrc ? GSnpRespFwded_RetToSrc_1() : GSnpRespFwded_RetToSrc_0();
                    }

                    inline constexpr const details::TableG1& GSnpRespDataFwded_RetToSrc_0() const noexcept
                    { return g6; }

                    inline constexpr const details::TableG1& GSnpRespDataFwded_RetToSrc_1() const noexcept
                    { return g7; }

                    inline constexpr const details::TableG1& GSnpRespDataFwded(bool retToSrc) const noexcept
                    {
                        return retToSrc ? GSnpRespDataFwded_RetToSrc_1() : GSnpRespDataFwded_RetToSrc_0();
                    }

                    inline constexpr const details::TableG0& GSnpRespDataPtl_RetToSrc_0() const noexcept
                    { return g8; }

                    inline constexpr const details::TableG0& GSnpRespDataPtl_RetToSrc_1() const noexcept
                    { return g9; }

                    inline constexpr const details::TableG0& GSnpRespDataPtl(bool retToSrc) const noexcept
                    {
                        return retToSrc ? GSnpRespDataPtl_RetToSrc_1() : GSnpRespDataPtl_RetToSrc_0();
                    }
                };

                #define _Tables_Dataless(opcode) TablesDataless( \
                    details::GetTableG0Comp<Transitions::opcode.size(), Transitions::opcode>() \
                )

                #define _Tables_Read(opcode) TablesRead( \
                    details::GetTableG0CompData<Transitions::opcode.size(), Transitions::opcode>(), \
                    details::GetTableG0DataSepResp<Transitions::opcode.size(), Transitions::opcode>() \
                )
                
                #define _Tables_MakeReadUnique(opcode) TablesMakeReadUnique( \
                    details::GetTableG0MakeReadUniqueComp<Transitions::opcode.size(), Transitions::opcode>(), \
                    details::GetTableG0MakeReadUniqueCompData<Transitions::opcode.size(), Transitions::opcode>(), \
                    details::GetTableG0MakeReadUniqueDataSepResp<Transitions::opcode.size(), Transitions::opcode>() \
                )

                #define _Tables_WriteCopyBack(opcode) TablesWrite( \
                    details::GetTableG0CopyBackWrData<Transitions::opcode.size(), Transitions::opcode>() \
                )

                #define _Tables_WriteNonCopyBack(opcode) TablesWriteNonCopyBack()

                #define _Tables_Atomic(opcode) TablesAtomic( \
                    details::GetTableG0Comp<Transitions::opcode.size(), Transitions::opcode>(), \
                    details::GetTableG0CompData<Transitions::opcode.size(), Transitions::opcode>() \
                )

                #define _Tables_SnpX(opcode) TablesSnpX( \
                    details::GetTableG0SnpResp<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG0SnpResp<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>(), \
                    details::GetTableG0SnpRespData<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG0SnpRespData<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>(), \
                    details::GetTableG0SnpRespDataPtl<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG0SnpRespDataPtl<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>() \
                )

                #define _Tables_SnpXFwd(opcode) TablesSnpXFwd( \
                    details::GetTableG0SnpResp<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG0SnpResp<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>(), \
                    details::GetTableG0SnpRespData<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG0SnpRespData<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>(), \
                    details::GetTableG1SnpRespFwded<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG1SnpRespFwded<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>(), \
                    details::GetTableG1SnpRespDataFwded<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG1SnpRespDataFwded<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>(), \
                    details::GetTableG0SnpRespDataPtl<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A0>(), \
                    details::GetTableG0SnpRespDataPtl<Transitions::opcode.size(), Transitions::opcode, RetToSrcs::A1>() \
                )

                // Intermediate tables for Read request transactions
                constexpr TablesRead                ReadNoSnp                       = _Tables_Read(ReadNoSnp);
                constexpr TablesRead                ReadOnce                        = _Tables_Read(ReadOnce);
                constexpr TablesRead                ReadOnceCleanInvalid            = _Tables_Read(ReadOnceCleanInvalid);
                constexpr TablesRead                ReadOnceMakeInvalid             = _Tables_Read(ReadOnceMakeInvalid);
                constexpr TablesRead                ReadClean_Transfer              = _Tables_Read(ReadClean_Transfer);
                constexpr TablesRead                ReadClean                       = _Tables_Read(ReadClean);
                constexpr TablesRead                ReadNotSharedDirty              = _Tables_Read(ReadNotSharedDirty);
                constexpr TablesRead                ReadShared                      = _Tables_Read(ReadShared);
                constexpr TablesRead                ReadUnique                      = _Tables_Read(ReadUnique);
                constexpr TablesRead                ReadPreferUnique                = _Tables_Read(ReadPreferUnique);
                constexpr TablesMakeReadUnique      MakeReadUnique                  = _Tables_MakeReadUnique(MakeReadUnique);
                constexpr TablesMakeReadUnique      MakeReadUnique_Excl             = _Tables_MakeReadUnique(MakeReadUnique_Excl);

                // Intermediate tables for Dataless request transactions
                constexpr TablesDataless            CleanUnique                     = _Tables_Dataless(CleanUnique);
                constexpr TablesDataless            MakeUnique                      = _Tables_Dataless(MakeUnique);
                constexpr TablesDataless            Evict                           = _Tables_Dataless(Evict);
                constexpr TablesDataless            StashOnceUnique                 = _Tables_Dataless(StashOnceUnique);
                constexpr TablesDataless            StashOnceSepUnique              = _Tables_Dataless(StashOnceSepUnique);
                constexpr TablesDataless            StashOnceShared                 = _Tables_Dataless(StashOnceShared);
                constexpr TablesDataless            StashOnceSepShared              = _Tables_Dataless(StashOnceSepShared);
                constexpr TablesDataless            CleanShared                     = _Tables_Dataless(CleanShared);
                constexpr TablesDataless            CleanSharedPersist              = _Tables_Dataless(CleanSharedPersist);
                constexpr TablesDataless            CleanSharedPersistSep           = _Tables_Dataless(CleanSharedPersistSep);
                constexpr TablesDataless            CleanInvalid                    = _Tables_Dataless(CleanInvalid);
                constexpr TablesDataless            MakeInvalid                     = _Tables_Dataless(MakeInvalid);

                // Intermediate tables for Write request transactions
                constexpr TablesWriteNonCopyBack    WriteNoSnpPtl                   = _Tables_WriteNonCopyBack(WriteNoSnpPtl);
                constexpr TablesWriteNonCopyBack    WriteNoSnpFull                  = _Tables_WriteNonCopyBack(WriteNoSnpFull);
                constexpr TablesWriteNonCopyBack    WriteNoSnpZero                  = _Tables_WriteNonCopyBack(WriteNoSnpZero);
                constexpr TablesWriteNonCopyBack    WriteUniquePtl                  = _Tables_WriteNonCopyBack(WriteUniquePtl);
                constexpr TablesWriteNonCopyBack    WriteUniquePtlStash             = _Tables_WriteNonCopyBack(WriteUniquePtlStash);
                constexpr TablesWriteNonCopyBack    WriteUniqueZero                 = _Tables_WriteNonCopyBack(WriteUniqueZero);
                constexpr TablesWriteNonCopyBack    WriteUniqueFull                 = _Tables_WriteNonCopyBack(WriteUniqueFull);
                constexpr TablesWriteNonCopyBack    WriteUniqueFullStash            = _Tables_WriteNonCopyBack(WriteUniqueFullStash);
                constexpr TablesWrite               WriteBackFull                   = _Tables_WriteCopyBack(WriteBackFull);
                constexpr TablesWrite               WriteBackPtl                    = _Tables_WriteCopyBack(WriteBackPtl);
                constexpr TablesWrite               WriteCleanFull                  = _Tables_WriteCopyBack(WriteCleanFull);
                constexpr TablesWrite               WriteEvictFull                  = _Tables_WriteCopyBack(WriteEvictFull);
                constexpr TablesWrite               WriteEvictOrEvict               = _Tables_WriteCopyBack(WriteEvictOrEvict);

                // Intermediate tables for Atomic request transactions
                constexpr TablesAtomic              AtomicStore                     = _Tables_Atomic(AtomicStore);
                constexpr TablesAtomic              AtomicLoad                      = _Tables_Atomic(AtomicLoad);
                constexpr TablesAtomic              AtomicSwap                      = _Tables_Atomic(AtomicSwap);
                constexpr TablesAtomic              AtomicCompare                   = _Tables_Atomic(AtomicCompare);

                // Intermediate tables for Snoop request transactions
                constexpr TablesSnpX                SnpOnce                         = _Tables_SnpX(SnpOnce);
                constexpr TablesSnpX                SnpClean                        = _Tables_SnpX(SnpClean);
                constexpr TablesSnpX                SnpShared                       = _Tables_SnpX(SnpShared);
                constexpr TablesSnpX                SnpNotSharedDirty               = _Tables_SnpX(SnpNotSharedDirty);
                constexpr TablesSnpX                SnpPreferUnique_NoExcl          = _Tables_SnpX(SnpPreferUnique_NoExcl);
                constexpr TablesSnpX                SnpUnique                       = _Tables_SnpX(SnpUnique);
                constexpr TablesSnpX                SnpPreferUnique_InExcl          = _Tables_SnpX(SnpPreferUnique_InExcl);
                constexpr TablesSnpX                SnpCleanShared                  = _Tables_SnpX(SnpCleanShared);
                constexpr TablesSnpX                SnpCleanInvalid                 = _Tables_SnpX(SnpCleanInvalid);
                constexpr TablesSnpX                SnpMakeInvalid                  = _Tables_SnpX(SnpMakeInvalid);
                constexpr TablesSnpX                SnpQuery                        = _Tables_SnpX(SnpQuery);
                constexpr TablesSnpX                SnpUniqueStash                  = _Tables_SnpX(SnpUniqueStash);
                constexpr TablesSnpX                SnpMakeInvalidStash             = _Tables_SnpX(SnpMakeInvalidStash);
                constexpr TablesSnpX                SnpStashUnique                  = _Tables_SnpX(SnpStashUnique);
                constexpr TablesSnpX                SnpStashShared                  = _Tables_SnpX(SnpStashShared);

                // Intermediate tables for Snoop Forward request transactions
                constexpr TablesSnpXFwd             SnpOnceFwd                      = _Tables_SnpXFwd(SnpOnceFwd);
                constexpr TablesSnpXFwd             SnpCleanFwd                     = _Tables_SnpXFwd(SnpCleanFwd);
                constexpr TablesSnpXFwd             SnpNotSharedDirtyFwd            = _Tables_SnpXFwd(SnpNotSharedDirtyFwd);
                constexpr TablesSnpXFwd             SnpSharedFwd                    = _Tables_SnpXFwd(SnpSharedFwd);
                constexpr TablesSnpXFwd             SnpUniqueFwd                    = _Tables_SnpXFwd(SnpUniqueFwd);
                constexpr TablesSnpXFwd             SnpPreferUniqueFwd_NoExcl       = _Tables_SnpXFwd(SnpPreferUniqueFwd_NoExcl);
                constexpr TablesSnpXFwd             SnpPreferUniqueFwd_InExcl       = _Tables_SnpXFwd(SnpPreferUniqueFwd_InExcl);

                #undef _Tables_Dataless
                #undef _Tables_Read
                #undef _Tables_MakeReadUnique
                #undef _Tables_WriteCopyBack
                #undef _Tables_WriteNonCopyBack
                #undef _Tables_Atomic
                #undef _Tables_SnpX
                #undef _Tables_SnpXFwd


                namespace Nested {

                    #define _TableG2_General(opcode)        { false, {} }
                    #define _TableG2_MakeReadUnique(opcode) details::GetTableG2MakeReadUnique<Transitions::opcode.size(), Transitions::opcode>()
                    #define _TableG2_Write(opcode)          details::GetTableG2Write<Transitions::opcode.size(), Transitions::opcode>()
                    
                    // Nested transitions tables for Read request transactions
                    constexpr details::TableG2 ReadNoSnp                        = _TableG2_General(ReadNoSnp);
                    constexpr details::TableG2 ReadOnce                         = _TableG2_General(ReadOnce);
                    constexpr details::TableG2 ReadOnceCleanInvalid             = _TableG2_General(ReadOnceCleanInvalid);
                    constexpr details::TableG2 ReadOnceMakeInvalid              = _TableG2_General(ReadOnceMakeInvalid);
                    constexpr details::TableG2 ReadClean_Transfer               = _TableG2_General(ReadClean_Transfer);
                    constexpr details::TableG2 ReadClean                        = _TableG2_General(ReadClean);
                    constexpr details::TableG2 ReadNotSharedDirty               = _TableG2_General(ReadNotSharedDirty);
                    constexpr details::TableG2 ReadShared                       = _TableG2_General(ReadShared);
                    constexpr details::TableG2 ReadUnique                       = _TableG2_General(ReadUnique);
                    constexpr details::TableG2 ReadPreferUnique                 = _TableG2_General(ReadPreferUnique);
                    constexpr details::TableG2 MakeReadUnique                   = _TableG2_MakeReadUnique(MakeReadUnique);
                    constexpr details::TableG2 MakeReadUnique_Excl              = _TableG2_MakeReadUnique(MakeReadUnique_Excl);

                    // Nested transitions tables for Dataless request transactions
                    constexpr details::TableG2 CleanUnique                      = _TableG2_General(CleanUnique);
                    constexpr details::TableG2 MakeUnique                       = _TableG2_General(MakeUnique);
                    constexpr details::TableG2 Evict                            = _TableG2_General(Evict);
                    constexpr details::TableG2 StashOnceUnique                  = _TableG2_General(StashOnceUnique);
                    constexpr details::TableG2 StashOnceSepUnique               = _TableG2_General(StashOnceSepUnique);
                    constexpr details::TableG2 StashOnceShared                  = _TableG2_General(StashOnceShared);
                    constexpr details::TableG2 StashOnceSepShared               = _TableG2_General(StashOnceSepShared);
                    constexpr details::TableG2 CleanShared                      = _TableG2_General(CleanShared);
                    constexpr details::TableG2 CleanSharedPersist               = _TableG2_General(CleanSharedPersist);
                    constexpr details::TableG2 CleanSharedPersistSep            = _TableG2_General(CleanSharedPersistSep);
                    constexpr details::TableG2 CleanInvalid                     = _TableG2_General(CleanInvalid);
                    constexpr details::TableG2 MakeInvalid                      = _TableG2_General(MakeInvalid);

                    // Nested transitions tables for Write request transactions
                    constexpr details::TableG2 WriteNoSnpPtl                    = _TableG2_Write(WriteNoSnpPtl);
                    constexpr details::TableG2 WriteNoSnpFull                   = _TableG2_Write(WriteNoSnpFull);
                    constexpr details::TableG2 WriteNoSnpZero                   = _TableG2_Write(WriteNoSnpZero);
                    constexpr details::TableG2 WriteUniquePtl                   = _TableG2_Write(WriteUniquePtl);
                    constexpr details::TableG2 WriteUniquePtlStash              = _TableG2_Write(WriteUniquePtlStash);
                    constexpr details::TableG2 WriteUniqueZero                  = _TableG2_Write(WriteUniqueZero);
                    constexpr details::TableG2 WriteUniqueFull                  = _TableG2_Write(WriteUniqueFull);
                    constexpr details::TableG2 WriteUniqueFullStash             = _TableG2_Write(WriteUniqueFullStash);
                    constexpr details::TableG2 WriteBackFull                    = _TableG2_Write(WriteBackFull);
                    constexpr details::TableG2 WriteBackPtl                     = _TableG2_Write(WriteBackPtl);
                    constexpr details::TableG2 WriteCleanFull                   = _TableG2_Write(WriteCleanFull);
                    constexpr details::TableG2 WriteEvictFull                   = _TableG2_Write(WriteEvictFull);
                    constexpr details::TableG2 WriteEvictOrEvict                = _TableG2_Write(WriteEvictOrEvict);

                    // Nested transitions tables for Atomic request transactions
                    constexpr details::TableG2 AtomicStore                      = _TableG2_General(AtomicStore);
                    constexpr details::TableG2 AtomicLoad                       = _TableG2_General(AtomicLoad);
                    constexpr details::TableG2 AtomicSwap                       = _TableG2_General(AtomicSwap);
                    constexpr details::TableG2 AtomicCompare                    = _TableG2_General(AtomicCompare);

                    #undef _TableG2_General
                    #undef _TableG2_MakeReadUnique
                    #undef _TableG2_Write
                };
            }
		}
	}
/*
}
*/

#endif
