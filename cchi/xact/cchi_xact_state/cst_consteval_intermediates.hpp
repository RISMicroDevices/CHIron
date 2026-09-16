#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_CONSTEVAL_INTERMEDIATES
#define __CCHI__CCHI_XACT_STATE__CST_CONSTEVAL_INTERMEDIATES

#include "cst_xact_read.hpp"
#include "cst_xact_dataless.hpp"
#include "cst_xact_cmo.hpp"
#include "cst_xact_stash.hpp"
#include "cst_xact_write.hpp"
#include "cst_xact_snoop.hpp"


namespace CCHI {
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

                    template<CacheState S>
                    inline consteval CacheState _NextState() noexcept
                    {
                        // C++20 is stupid to handle constexpr union here,
                        // so we have only this ungly choice
                        if (!S      ) return CacheStates::UC;
                        if (S.UC    ) return CacheStates::UD;
                        if (S.UD    ) return CacheStates::SC;
                        if (S.SC    ) return CacheStates::I;
                        return CacheStates::None;
                    }

                    template<CacheResp R>
                    inline consteval CacheResp _NextResp() noexcept
                    {
                        if (!R      ) return CacheResps::I;
                        if (R.I     ) return CacheResps::SC;
                        if (R.SC    ) return CacheResps::UC;
                        if (R.UC    ) return CacheResps::I_PD;
                        if (R.I_PD  ) return CacheResps::SC_PD;
                        if (R.SC_PD ) return CacheResps::UC_PD;
                        return CacheResps::None;
                    }

                    inline constexpr size_t GetStateTableIndex(CacheState x) noexcept
                    {
                        return (
                            (((1ULL & !x.UC ) - 1) &  0ULL)
                          | (((1ULL & !x.UD ) - 1) &  1ULL)
                          | (((1ULL & !x.SC ) - 1) &  2ULL)
                          | (((1ULL & !x.I  ) - 1) &  3ULL)
                        );
                    }

                    inline constexpr size_t GetRespTableIndex(CacheResp x) noexcept
                    {
                        return (
                            (((1ULL & !x.I      ) - 1) &  0ULL)
                          | (((1ULL & !x.SC     ) - 1) &  1ULL)
                          | (((1ULL & !x.UC     ) - 1) &  2ULL)
                          | (((1ULL & !x.I_PD   ) - 1) &  3ULL)
                          | (((1ULL & !x.SC_PD  ) - 1) &  4ULL)
                          | (((1ULL & !x.UC_PD  ) - 1) &  5ULL)
                        );
                    }

                    /**/
                    // Table G generations
                    struct TableG0Element {

                        union {
                            std::array<CacheState, 4>   states;
                            uint32_t                    i32;
                        };

                        inline constexpr TableG0Element() noexcept : states({
                            CacheStates::None, CacheStates::None, CacheStates::None, CacheStates::None
                        }) {}
                    };

                    struct TableG2Element {

                        union {
                            std::array<CacheState, 4>   states;
                            uint32_t                    i32;
                        };

                        inline constexpr TableG2Element() noexcept : states({
                            CacheStates::None, CacheStates::None, CacheStates::None, CacheStates::None
                        }) {}
                    };

                    // !NOTICE: Never use TableG for initial state checks.
                    /*
                    * P  = Current states
                    * Pi = Intermediate (pre-reply) states
                    *
                    * Xr = Response state
                    *
                    * xs = Final state, fail on empty
                    * xi = Final intermediate state, fall-through on non-V, fail on empty
                    *
                    * Table G0 usage:
                    *   1. P * G0(Xr) = xs
                    *
                    * Table G2 usage:
                    *   1. P * G2(Pi) = xi
                    */
                    using TableG0 = std::array<TableG0Element, 6>;
                    using TableG2 = struct _TableG2 {
                        bool                            V;
                        std::array<TableG2Element, 4>   E;

                        inline constexpr _TableG2(bool v = false, std::array<TableG2Element, 4> e = {}) noexcept
                            : V(v), E(e) {}
                    };

                    // G0 element builder: for resp R and current state S, OR the finals of all
                    // rows whose initial (requests) / pre-reply (snoops) set contains S and whose
                    // response field F contains R.
                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp CacheStateTransition::* F, CacheResp R, CacheState S>
                    inline consteval TableG0Element GetTableG0Element(TableG0Element E = TableG0Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.initial & S) && ((T.*F) & R))
                                    state = state | T.final;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG0Element<N, Ts, F, R, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp CacheStateTransition::* F, CacheResp R>
                    inline consteval TableG0Element GetTableG0Element() noexcept
                    {
                        return GetTableG0Element<N, Ts, F, R, _NextState<CacheStates::None>()>();
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheResp CacheStateTransition::* F,
                        CacheResp R,
                        TableG0 A = TableG0()>
                    inline consteval TableG0 GetTableG0() noexcept
                    {
                        if constexpr (R)
                        {
                            constexpr auto nextA = CopyOnWrite<GetRespTableIndex(R)>(A,
                                GetTableG0Element<N, Ts, F, R>());

                            return GetTableG0<N, Ts, F, _NextResp<R>(), nextA>();
                        }
                        else
                            return A;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheResp CacheStateTransition::* F>
                    inline consteval TableG0 GetTableG0() noexcept
                    {
                        return GetTableG0<N, Ts, F, _NextResp<CacheResps::None>()>();
                    }

                    // G2 element builder (snoops only): for target pre-reply state Si and
                    // snoop-arrival state S, Si is reachable if any row starts at S and lists
                    // Si in its pre-reply set.
                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheState Si, CacheState S>
                    inline consteval TableG2Element GetTableG2SnoopElement(TableG2Element E = TableG2Element()) noexcept
                    {
                        if constexpr (S)
                        {
                            CacheState state = CacheStates::None;
                            for (CacheStateTransition T : Ts)
                                if ((T.startState & S) && (T.initial & Si))
                                    state = state | Si;

                            E.states[GetStateTableIndex(S)] = state;

                            return GetTableG2SnoopElement<N, Ts, Si, _NextState<S>()>(E);
                        }
                        else
                            return E;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts, CacheState Si>
                    inline consteval TableG2Element GetTableG2SnoopElement() noexcept
                    {
                        return GetTableG2SnoopElement<N, Ts, Si, _NextState<CacheStates::None>()>();
                    }

                    template<
                        size_t N,
                        std::array<CacheStateTransition, N> Ts,
                        CacheState Si,
                        TableG2 A = TableG2()>
                    inline consteval TableG2 GetTableG2Snoop() noexcept
                    {
                        if constexpr (Si)
                        {
                            constexpr auto nextA = CopyOnWrite<GetStateTableIndex(Si)>(A.E,
                                GetTableG2SnoopElement<N, Ts, Si>());

                            return GetTableG2Snoop<N, Ts, _NextState<Si>(), TableG2(true, nextA)>();
                        }
                        else
                            return A;
                    }

                    template<size_t N, std::array<CacheStateTransition, N> Ts>
                    inline consteval TableG2 GetTableG2Snoop() noexcept
                    {
                        return GetTableG2Snoop<N, Ts, _NextState<CacheStates::None>()>();
                    }
                    /**/

                    //
                    inline constexpr uint32_t GetStateFactorForG0(CacheState x) noexcept
                    {
                        return (
                            ((((1UL & !x.UC ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::UC ) * 8))
                          | ((((1UL & !x.UD ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::UD ) * 8))
                          | ((((1UL & !x.SC ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::SC ) * 8))
                          | ((((1UL & !x.I  ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::I  ) * 8))
                        );
                    }

                    inline constexpr uint32_t GetStateFactorForG2(CacheState x) noexcept
                    {
                        return (
                            ((((1UL & !x.UC ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::UC ) * 8))
                          | ((((1UL & !x.UD ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::UD ) * 8))
                          | ((((1UL & !x.SC ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::SC ) * 8))
                          | ((((1UL & !x.I  ) - 1) & 0xFFUL) << (GetStateTableIndex(CacheStates::I  ) * 8))
                        );
                    }

                    inline constexpr CacheState GetStateFromG0Element(TableG0Element e) noexcept
                    {
                        return std::reduce(e.states.begin(), e.states.end(), CacheStates::None, std::bit_or<>());
                    }

                    inline constexpr CacheState GetStateFromG0Element(uint32_t ei) noexcept
                    {
                        TableG0Element e;
                        e.i32 = ei;
                        return GetStateFromG0Element(e);
                    }

                    inline constexpr CacheState GetStateFromG2Element(TableG2Element e) noexcept
                    {
                        return std::reduce(e.states.begin(), e.states.end(), CacheStates::None, std::bit_or<>());
                    }

                    inline constexpr CacheState GetStateFromG2Element(uint32_t ei) noexcept
                    {
                        TableG2Element e;
                        e.i32 = ei;
                        return GetStateFromG2Element(e);
                    }

                    inline constexpr CacheState ProductG0(const CacheState P, const TableG0& G0, const CacheResp Xr) noexcept
                    {
                        assert(std::popcount(Xr.i8) == 1);

                        auto px = GetStateFactorForG0(P);
                        auto rx = G0[GetRespTableIndex(Xr)].i32;

                        return GetStateFromG0Element(px & rx);
                    }

                    inline constexpr CacheState ProductG2(const CacheState P, const TableG2& G2, const CacheState Pi) noexcept
                    {
                        if (G2.V)
                        {
                            assert(std::popcount(Pi.i8) == 1);

                            auto px = details::GetStateFactorForG2(P);
                            auto rx = G2.E[details::GetStateTableIndex(Pi)].i32;

                            return GetStateFromG2Element(px & rx);
                        }
                        else
                            return Pi;
                    }
                }

                struct Tables {
                    enum class Type {
                        Read = 0,
                        Evict,
                        WriteBack,
                        Dataless,
                        CMO,
                        Stash,
                        WriteNonCopyBack,
                        Snoop
                    } type;

                    inline constexpr Tables(Type t) noexcept : type(t) {}
                };

                struct TablesRead : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // CompData (DnDAT)
                    details::TableG0 g1; // Comp (DnRSP, ReadUnique dataless path)

                    inline constexpr TablesRead() noexcept
                        : Tables(Tables::Type::Read) {}

                    inline constexpr TablesRead(details::TableG0 g0, details::TableG0 g1) noexcept
                        : Tables(Tables::Type::Read), g0(g0), g1(g1) {}

                    inline constexpr const details::TableG0& GCompData() const noexcept
                    { return g0; }

                    inline constexpr const details::TableG0& GComp() const noexcept
                    { return g1; }
                };

                struct TablesDataless : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // Comp (DnRSP)

                    inline constexpr TablesDataless() noexcept
                        : Tables(Tables::Type::Dataless) {}

                    inline constexpr TablesDataless(details::TableG0 g) noexcept
                        : Tables(Tables::Type::Dataless), g0(g) {}

                    inline constexpr const details::TableG0& GComp() const noexcept
                    { return g0; }
                };

                struct TablesCMO : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // CompCMO (DnRSP)

                    inline constexpr TablesCMO() noexcept
                        : Tables(Tables::Type::CMO) {}

                    inline constexpr TablesCMO(details::TableG0 g) noexcept
                        : Tables(Tables::Type::CMO), g0(g) {}

                    inline constexpr const details::TableG0& GCompCMO() const noexcept
                    { return g0; }
                };

                struct TablesStash : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // CompStash (DnRSP) - introspection only, see D9

                    inline constexpr TablesStash() noexcept
                        : Tables(Tables::Type::Stash) {}

                    inline constexpr TablesStash(details::TableG0 g) noexcept
                        : Tables(Tables::Type::Stash), g0(g) {}

                    inline constexpr const details::TableG0& GCompStash() const noexcept
                    { return g0; }
                };

                struct TablesWrite : public Tables {
                    // D10: Non-CopyBack write responses carry no state; checked structurally.
                    inline constexpr TablesWrite() noexcept
                        : Tables(Tables::Type::WriteNonCopyBack) {}
                };

                struct TablesEvict : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // Comp (DnRSP)

                    inline constexpr TablesEvict() noexcept
                        : Tables(Tables::Type::Evict) {}

                    inline constexpr TablesEvict(details::TableG0 g) noexcept
                        : Tables(Tables::Type::Evict), g0(g) {}

                    inline constexpr const details::TableG0& GComp() const noexcept
                    { return g0; }
                };

                struct TablesWriteBack : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // CopyBackWrData (UpDAT)

                    inline constexpr TablesWriteBack() noexcept
                        : Tables(Tables::Type::WriteBack) {}

                    inline constexpr TablesWriteBack(details::TableG0 g) noexcept
                        : Tables(Tables::Type::WriteBack), g0(g) {}

                    inline constexpr const details::TableG0& GCopyBackWrData() const noexcept
                    { return g0; }
                };

                struct TablesSnp : public Tables {
                    /* union not used for constexpr, consteval compatibility */
                    details::TableG0 g0; // SnpResp (UpRSP)
                    details::TableG0 g1; // SnpRespData (UpDAT)

                    inline constexpr TablesSnp() noexcept
                        : Tables(Tables::Type::Snoop) {}

                    inline constexpr TablesSnp(details::TableG0 g0, details::TableG0 g1) noexcept
                        : Tables(Tables::Type::Snoop), g0(g0), g1(g1) {}

                    inline constexpr const details::TableG0& GSnpResp() const noexcept
                    { return g0; }

                    inline constexpr const details::TableG0& GSnpRespData() const noexcept
                    { return g1; }
                };

                #define _Tables_Read(opcode) TablesRead( \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respCompData>(), \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respComp>() \
                )

                #define _Tables_Dataless(opcode) TablesDataless( \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respComp>() \
                )

                #define _Tables_CMO(opcode) TablesCMO( \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respCompCMO>() \
                )

                #define _Tables_Stash(opcode) TablesStash( \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respCompStash>() \
                )

                #define _Tables_WriteNonCopyBack(opcode) TablesWrite()

                #define _Tables_Evict(opcode) TablesEvict( \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respComp>() \
                )

                #define _Tables_WriteBack(opcode) TablesWriteBack( \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respCopyBackWrData>() \
                )

                #define _Tables_Snoop(opcode) TablesSnp( \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respSnpResp>(), \
                    details::GetTableG0<Transitions::opcode.size(), Transitions::opcode, &CacheStateTransition::respSnpRespData>() \
                )

                // Intermediate tables for Read request transactions
                constexpr TablesRead        ReadNoSnp           = _Tables_Read(ReadNoSnp);
                constexpr TablesRead        ReadOnce            = _Tables_Read(ReadOnce);
                constexpr TablesRead        ReadShared          = _Tables_Read(ReadShared);
                constexpr TablesRead        ReadUnique          = _Tables_Read(ReadUnique);

                // Intermediate tables for Dataless request transactions
                constexpr TablesDataless    MakeUnique          = _Tables_Dataless(MakeUnique);

                // Intermediate tables for CMO request transactions
                constexpr TablesCMO         CleanShared         = _Tables_CMO(CleanShared);
                constexpr TablesCMO         CleanInvalid        = _Tables_CMO(CleanInvalid);
                constexpr TablesCMO         MakeInvalid         = _Tables_CMO(MakeInvalid);

                // Intermediate tables for Stash request transactions
                constexpr TablesStash       StashShared         = _Tables_Stash(StashShared);
                constexpr TablesStash       StashUnique         = _Tables_Stash(StashUnique);

                // Intermediate tables for Write request transactions
                constexpr TablesWrite       WriteNoSnpPtl       = _Tables_WriteNonCopyBack(WriteNoSnpPtl);
                constexpr TablesWrite       WriteNoSnpFull      = _Tables_WriteNonCopyBack(WriteNoSnpFull);
                constexpr TablesWrite       WriteUniquePtl      = _Tables_WriteNonCopyBack(WriteUniquePtl);
                constexpr TablesWrite       WriteUniqueFull     = _Tables_WriteNonCopyBack(WriteUniqueFull);

                // Intermediate tables for EVT channel transactions
                constexpr TablesEvict       Evict               = _Tables_Evict(Evict);
                constexpr TablesWriteBack   WriteBackFull       = _Tables_WriteBack(WriteBackFull);

                // Intermediate tables for Snoop transactions
                constexpr TablesSnp         SnpMakeInvalid      = _Tables_Snoop(SnpMakeInvalid);
                constexpr TablesSnp         SnpToInvalid        = _Tables_Snoop(SnpToInvalid);
                constexpr TablesSnp         SnpToShared         = _Tables_Snoop(SnpToShared);
                constexpr TablesSnp         SnpToClean          = _Tables_Snoop(SnpToClean);

                #undef _Tables_Read
                #undef _Tables_Dataless
                #undef _Tables_CMO
                #undef _Tables_Stash
                #undef _Tables_WriteNonCopyBack
                #undef _Tables_Evict
                #undef _Tables_WriteBack
                #undef _Tables_Snoop


                namespace Nested {

                    #define _TableG2_General(opcode)        { false, {} }
                    #define _TableG2_Snoop(opcode)          details::GetTableG2Snoop<Transitions::opcode.size(), Transitions::opcode>()

                    // Nested transition tables for Read request transactions (passthrough)
                    constexpr details::TableG2 ReadNoSnp        = _TableG2_General(ReadNoSnp);
                    constexpr details::TableG2 ReadOnce         = _TableG2_General(ReadOnce);
                    constexpr details::TableG2 ReadShared       = _TableG2_General(ReadShared);
                    constexpr details::TableG2 ReadUnique       = _TableG2_General(ReadUnique);

                    // Nested transition tables for Dataless request transactions (passthrough)
                    constexpr details::TableG2 MakeUnique       = _TableG2_General(MakeUnique);

                    // Nested transition tables for CMO request transactions (passthrough)
                    constexpr details::TableG2 CleanShared      = _TableG2_General(CleanShared);
                    constexpr details::TableG2 CleanInvalid     = _TableG2_General(CleanInvalid);
                    constexpr details::TableG2 MakeInvalid      = _TableG2_General(MakeInvalid);

                    // Nested transition tables for Stash request transactions (passthrough)
                    constexpr details::TableG2 StashShared      = _TableG2_General(StashShared);
                    constexpr details::TableG2 StashUnique      = _TableG2_General(StashUnique);

                    // Nested transition tables for Write request transactions (passthrough)
                    constexpr details::TableG2 WriteNoSnpPtl    = _TableG2_General(WriteNoSnpPtl);
                    constexpr details::TableG2 WriteNoSnpFull   = _TableG2_General(WriteNoSnpFull);
                    constexpr details::TableG2 WriteUniquePtl   = _TableG2_General(WriteUniquePtl);
                    constexpr details::TableG2 WriteUniqueFull  = _TableG2_General(WriteUniqueFull);

                    // Nested transition tables for EVT channel transactions (passthrough)
                    constexpr details::TableG2 Evict            = _TableG2_General(Evict);
                    constexpr details::TableG2 WriteBackFull    = _TableG2_General(WriteBackFull);

                    // Nested transition tables for Snoop transactions (start -> pre-reply, drive
                    // the nested Transfer() checks against the snoops' 回复前状态 columns)
                    constexpr details::TableG2 SnpMakeInvalid   = _TableG2_Snoop(SnpMakeInvalid);
                    constexpr details::TableG2 SnpToInvalid     = _TableG2_Snoop(SnpToInvalid);
                    constexpr details::TableG2 SnpToShared      = _TableG2_Snoop(SnpToShared);
                    constexpr details::TableG2 SnpToClean       = _TableG2_Snoop(SnpToClean);

                    #undef _TableG2_General
                    #undef _TableG2_Snoop
                }
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_CONSTEVAL_INTERMEDIATES
