#pragma once

#include <concepts>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <set>
#include <type_traits>
#include <unordered_map>

#include "../../chi_eb/spec/chi_eb_protocol_flits.hpp"
#include "../../chi_eb/util/chi_eb_util_flit.hpp"

#include "../cohestra_chi.hpp"
#include "interface.hpp"


namespace Cohestra {

    // TickPreHandshake (drive pins) -> Eval (clock=0) -> TickPostHandshake (sample/retire) -> Eval (clock=1)

    namespace details {

        // --------------------------------------------------------------------
        // Signal-type concepts and packed-bit accessors (reused from AXI patterns)
        // --------------------------------------------------------------------

        template<class T>
        concept V3CHIScalarSignal = std::is_integral_v<std::remove_cvref_t<T>>;

        template<class T>
        concept V3CHIDataSignal = std::is_integral_v<std::remove_cvref_t<T>>
            || requires (T t) { { t[0] } -> std::convertible_to<uint32_t&>; };

        template<class T>
        inline bool V3CHIPackedGetBit(const T& signal, size_t bit) noexcept
        {
            if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)
            {
                constexpr size_t bits = sizeof(std::remove_cvref_t<T>) * 8;
                if (bit >= bits)
                    return false;
                return (signal >> bit) & 1;
            }
            else
            {
                return (signal[bit / 32] >> (bit % 32)) & 1;
            }
        }

        template<class T>
        inline void V3CHIPackedSetBit(T& signal, size_t bit, bool value) noexcept
        {
            if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)
            {
                constexpr size_t bits = sizeof(std::remove_cvref_t<T>) * 8;
                if (bit >= bits)
                    return;

                using U = std::remove_cvref_t<T>;
                signal = (signal & ~(U(1) << bit)) | (U(value) << bit);
            }
            else
            {
                signal[bit / 32] = (signal[bit / 32] & ~(uint32_t(1) << (bit % 32)))
                                 | (uint32_t(value) << (bit % 32));
            }
        }

        // Copy a Verilator scalar or word-array signal into a uint32_t bit array.
        inline void V3CHICopySignalToFlitBits(const auto& signal, uint32_t* bits, size_t bitCount) noexcept
        {
            size_t wordCount = (bitCount + 31) / 32;
            for (size_t i = 0; i < wordCount; ++i)
                bits[i] = 0;

            for (size_t i = 0; i < bitCount; ++i)
                if (V3CHIPackedGetBit(signal, i))
                    bits[i / 32] |= uint32_t(1) << (i % 32);
        }

        // Copy a uint32_t bit array into a Verilator scalar or word-array signal.
        inline void V3CHICopyFlitBitsToSignal(auto& signal, const uint32_t* bits, size_t bitCount) noexcept
        {
            for (size_t i = 0; i < bitCount; ++i)
                V3CHIPackedSetBit(signal, i, (bits[i / 32] >> (i % 32)) & 1);
        }



        // --------------------------------------------------------------------
        // Channel tags
        // --------------------------------------------------------------------

        struct TXREQChannel { };
        struct TXRSPChannel { };
        struct TXDATChannel { };
        struct RXRSPChannel { };
        struct RXDATChannel { };
        struct RXSNPChannel { };
        struct RXREQChannel { };

        // --------------------------------------------------------------------
        // Flit field lists for separate-field pin binding.
        // The X(name) macro is applied to every public accessor name.
        // Conditional fields are handled with if constexpr inside the generated code.
        // --------------------------------------------------------------------

#define V3_CHI_REQ_FIELDS(X)                                                \
    X(QoS)                                                                  \
    X(TgtID)                                                                \
    X(SrcID)                                                                \
    X(TxnID)                                                                \
    X(ReturnNID)                                                            \
    X(StashNID)                                                             \
    X(StashNIDValid)                                                        \
    X(Endian)                                                               \
    X(ReturnTxnID)                                                          \
    X(StashLPIDValid)                                                       \
    X(StashLPID)                                                            \
    X(Opcode)                                                               \
    X(Size)                                                                 \
    X(Addr)                                                                 \
    X(NS)                                                                   \
    X(LikelyShared)                                                         \
    X(AllowRetry)                                                           \
    X(Order)                                                                \
    X(PCrdType)                                                             \
    X(MemAttr)                                                              \
    X(SnpAttr)                                                              \
    X(LPID)                                                                 \
    X(Excl)                                                                 \
    X(SnoopMe)                                                              \
    X(ExpCompAck)                                                           \
    X(TraceTag)                                                             \
    X(SLCRepHint)                                                           \
    X(Deep)                                                                 \
    X(DoDWT)                                                                \
    X(PGroupID)                                                             \
    X(StashGroupID)                                                         \
    X(TagGroupID)                                                           \
    X(TagOp)                                                                \
    X(MPAM)                                                                 \
    X(RSVDC)

#define V3_CHI_RSP_FIELDS(X)                                                \
    X(QoS)                                                                  \
    X(TgtID)                                                                \
    X(SrcID)                                                                \
    X(TxnID)                                                                \
    X(Opcode)                                                               \
    X(RespErr)                                                              \
    X(Resp)                                                                 \
    X(FwdState)                                                             \
    X(DataPull)                                                             \
    X(CBusy)                                                                \
    X(DBID)                                                                 \
    X(PGroupID)                                                             \
    X(StashGroupID)                                                         \
    X(TagGroupID)                                                           \
    X(PCrdType)                                                             \
    X(TagOp)                                                                \
    X(TraceTag)

#define V3_CHI_DAT_FIELDS(X)                                                \
    X(QoS)                                                                  \
    X(TgtID)                                                                \
    X(SrcID)                                                                \
    X(TxnID)                                                                \
    X(HomeNID)                                                              \
    X(Opcode)                                                               \
    X(RespErr)                                                              \
    X(Resp)                                                                 \
    X(FwdState)                                                             \
    X(DataPull)                                                             \
    X(DataSource)                                                           \
    X(CBusy)                                                                \
    X(DBID)                                                                 \
    X(CCID)                                                                 \
    X(DataID)                                                               \
    X(TagOp)                                                                \
    X(Tag)                                                                  \
    X(TU)                                                                   \
    X(TraceTag)                                                             \
    X(RSVDC)                                                                \
    X(BE)                                                                   \
    X(Data)                                                                 \
    X(DataCheck)                                                            \
    X(Poison)

#define V3_CHI_SNP_FIELDS(X)                                                \
    X(QoS)                                                                  \
    X(SrcID)                                                                \
    X(TxnID)                                                                \
    X(FwdNID)                                                               \
    X(FwdTxnID)                                                             \
    X(StashLPID)                                                            \
    X(StashLPIDValid)                                                       \
    X(VMIDExt)                                                              \
    X(Opcode)                                                               \
    X(Addr)                                                                 \
    X(NS)                                                                   \
    X(DoNotGoToSD)                                                          \
    X(RetToSrc)                                                             \
    X(TraceTag)                                                             \
    X(MPAM)


        // Two-arg variants used by the pin traits (P = port index).
#define V3_CHI_REQ_FIELDS_P(X, P)                                                    \
            X(P, QoS) X(P, TgtID) X(P, SrcID) X(P, TxnID) X(P, ReturnNID) X(P, StashNID)\
            X(P, StashNIDValid) X(P, Endian) X(P, ReturnTxnID) X(P, StashLPIDValid)\
            X(P, StashLPID) X(P, Opcode) X(P, Size) X(P, Addr) X(P, NS)\
            X(P, LikelyShared) X(P, AllowRetry) X(P, Order) X(P, PCrdType) X(P, MemAttr)\
            X(P, SnpAttr) X(P, LPID) X(P, Excl) X(P, SnoopMe) X(P, ExpCompAck)\
            X(P, TraceTag) X(P, SLCRepHint) X(P, Deep) X(P, DoDWT) X(P, PGroupID)\
            X(P, StashGroupID) X(P, TagGroupID) X(P, TagOp) X(P, MPAM) X(P, RSVDC)

#define V3_CHI_RSP_FIELDS_P(X, P)                                                    \
            X(P, QoS) X(P, TgtID) X(P, SrcID) X(P, TxnID) X(P, Opcode) X(P, RespErr)\
            X(P, Resp) X(P, FwdState) X(P, DataPull) X(P, CBusy) X(P, DBID)\
            X(P, PGroupID) X(P, StashGroupID) X(P, TagGroupID) X(P, PCrdType)\
            X(P, TagOp) X(P, TraceTag)

#define V3_CHI_DAT_FIELDS_P(X, P)                                                    \
            X(P, QoS) X(P, TgtID) X(P, SrcID) X(P, TxnID) X(P, HomeNID) X(P, Opcode)\
            X(P, RespErr) X(P, Resp) X(P, FwdState) X(P, DataPull) X(P, DataSource)\
            X(P, CBusy) X(P, DBID) X(P, CCID) X(P, DataID) X(P, TagOp) X(P, Tag)\
            X(P, TU) X(P, TraceTag) X(P, RSVDC) X(P, BE) X(P, Data) X(P, DataCheck)\
            X(P, Poison)

#define V3_CHI_SNP_FIELDS_P(X, P)                                                    \
            X(P, QoS) X(P, SrcID) X(P, TxnID) X(P, FwdNID) X(P, FwdTxnID)\
            X(P, StashLPID) X(P, StashLPIDValid) X(P, VMIDExt) X(P, Opcode) X(P, Addr)\
            X(P, NS) X(P, DoNotGoToSD) X(P, RetToSrc) X(P, TraceTag) X(P, MPAM)

        // --------------------------------------------------------------------
        // Separate-field pin traits: per-pin static accessor functions.
        // (Accessor functions, not pointer-to-member tables: Verilator 5
        //  emits top-level ports as reference members, where &VT::pin is
        //  ill-formed. Generated by macros so the source stays compact.)
        // --------------------------------------------------------------------

        // RN-side separate-field pin traits.
#define V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(PORT)                          \
        template<class VT>                                                  \
        struct V3CHISeparateFieldRNPinTraits<VT, PORT, TXREQChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_lcrdv; }    \
            V3_CHI_REQ_FIELDS_P(V3_CHI_REQ_FIELD_PTR, PORT)                         \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldRNPinTraits<VT, PORT, TXRSPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_lcrdv; }    \
            V3_CHI_RSP_FIELDS_P(V3_CHI_RSP_FIELD_PTR, PORT)                         \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldRNPinTraits<VT, PORT, TXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_FIELD_PTR, PORT)                         \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldRNPinTraits<VT, PORT, RXRSPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _rxrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxrsp_lcrdv; }    \
            V3_CHI_RSP_FIELDS_P(V3_CHI_RSP_FIELD_PTR, PORT)                         \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldRNPinTraits<VT, PORT, RXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_FIELD_PTR, PORT)                         \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldRNPinTraits<VT, PORT, RXSNPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_lcrdv; }    \
            V3_CHI_SNP_FIELDS_P(V3_CHI_SNP_FIELD_PTR, PORT)                         \
        };

#define V3_CHI_REQ_FIELD_PTR(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_bits_ ## name; }
#define V3_CHI_RSP_FIELD_PTR(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_bits_ ## name; }
#define V3_CHI_DAT_FIELD_PTR(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_bits_ ## name; }
#define V3_CHI_SNP_FIELD_PTR(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_bits_ ## name; }

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldRNPinTraits;

        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(0)
        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(1)
        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(2)
        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(3)
        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(4)
        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(5)
        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(6)
        V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS(7)

#undef V3_CHI_REQ_FIELD_PTR
#undef V3_CHI_RSP_FIELD_PTR
#undef V3_CHI_DAT_FIELD_PTR
#undef V3_CHI_SNP_FIELD_PTR

        // SN-side separate-field pin traits.
#define V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(PORT)                          \
        template<class VT>                                                  \
        struct V3CHISeparateFieldSNPinTraits<VT, PORT, RXREQChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_lcrdv; }    \
            V3_CHI_REQ_FIELDS_P(V3_CHI_REQ_FIELD_PTR_SN, PORT)                      \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldSNPinTraits<VT, PORT, TXRSPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_lcrdv; }    \
            V3_CHI_RSP_FIELDS_P(V3_CHI_RSP_FIELD_PTR_SN, PORT)                      \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldSNPinTraits<VT, PORT, TXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_FIELD_PTR_SN, PORT)                      \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldSNPinTraits<VT, PORT, RXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_FIELD_PTR_SN, PORT)                      \
        };

#define V3_CHI_REQ_FIELD_PTR_SN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_bits_ ## name; }
#define V3_CHI_RSP_FIELD_PTR_SN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_bits_ ## name; }
#define V3_CHI_DAT_FIELD_PTR_SN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_bits_ ## name; }
#define V3_CHI_DAT_RX_FIELD_PTR_SN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_bits_ ## name; }

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldSNPinTraits;

        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(0)
        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(1)
        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(2)
        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(3)
        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(4)
        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(5)
        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(6)
        V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS(7)

#undef V3_CHI_REQ_FIELD_PTR_SN
#undef V3_CHI_RSP_FIELD_PTR_SN
#undef V3_CHI_DAT_FIELD_PTR_SN
#undef V3_CHI_DAT_RX_FIELD_PTR_SN

        // Monitor separate-field pin traits.
#define V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(PORT)                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorRNPinTraits<VT, PORT, TXREQChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_lcrdv; }    \
            V3_CHI_REQ_FIELDS_P(V3_CHI_REQ_FIELD_PTR_MON, PORT)                     \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorRNPinTraits<VT, PORT, TXRSPChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_lcrdv; }    \
            V3_CHI_RSP_FIELDS_P(V3_CHI_RSP_FIELD_PTR_MON, PORT)                     \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorRNPinTraits<VT, PORT, TXDATChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_FIELD_PTR_MON, PORT)                     \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorRNPinTraits<VT, PORT, RXRSPChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxrsp_lcrdv; }    \
            V3_CHI_RSP_FIELDS_P(V3_CHI_RSP_FIELD_PTR_MON, PORT)                     \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorRNPinTraits<VT, PORT, RXDATChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_FIELD_PTR_MON, PORT)                     \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorRNPinTraits<VT, PORT, RXSNPChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_lcrdv; }    \
            V3_CHI_SNP_FIELDS_P(V3_CHI_SNP_FIELD_PTR_MON, PORT)                     \
        };

#define V3_CHI_REQ_FIELD_PTR_MON(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_flit_ ## name; }
#define V3_CHI_RSP_FIELD_PTR_MON(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_flit_ ## name; }
#define V3_CHI_DAT_FIELD_PTR_MON(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_flit_ ## name; }
#define V3_CHI_SNP_FIELD_PTR_MON(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_flit_ ## name; }

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldMonitorRNPinTraits;

        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(0)
        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(1)
        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(2)
        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(3)
        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(4)
        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(5)
        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(6)
        V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS(7)

#undef V3_CHI_REQ_FIELD_PTR_MON
#undef V3_CHI_RSP_FIELD_PTR_MON
#undef V3_CHI_DAT_FIELD_PTR_MON
#undef V3_CHI_SNP_FIELD_PTR_MON

#define V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(PORT)                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorSNPinTraits<VT, PORT, RXREQChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_lcrdv; }    \
            V3_CHI_REQ_FIELDS_P(V3_CHI_REQ_FIELD_PTR_MONSN, PORT)                   \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorSNPinTraits<VT, PORT, TXRSPChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_lcrdv; }    \
            V3_CHI_RSP_FIELDS_P(V3_CHI_RSP_FIELD_PTR_MONSN, PORT)                   \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorSNPinTraits<VT, PORT, TXDATChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_FIELD_PTR_MONSN, PORT)                   \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHISeparateFieldMonitorSNPinTraits<VT, PORT, RXDATChannel> { \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_lcrdv; }    \
            V3_CHI_DAT_FIELDS_P(V3_CHI_DAT_RX_FIELD_PTR_MONSN, PORT)                \
        };

#define V3_CHI_REQ_FIELD_PTR_MONSN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_flit_ ## name; }
#define V3_CHI_RSP_FIELD_PTR_MONSN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_flit_ ## name; }
#define V3_CHI_DAT_FIELD_PTR_MONSN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_flit_ ## name; }
#define V3_CHI_DAT_RX_FIELD_PTR_MONSN(PORT, name)                                                \
            static inline auto& name(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_flit_ ## name; }

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldMonitorSNPinTraits;

        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(0)
        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(1)
        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(2)
        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(3)
        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(4)
        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(5)
        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(6)
        V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS(7)

#undef V3_CHI_REQ_FIELD_PTR_MONSN
#undef V3_CHI_RSP_FIELD_PTR_MONSN
#undef V3_CHI_DAT_FIELD_PTR_MONSN
#undef V3_CHI_DAT_RX_FIELD_PTR_MONSN

#undef V3_CHI_DEFINE_RN_SEPARATE_PIN_TRAITS
#undef V3_CHI_DEFINE_SN_SEPARATE_PIN_TRAITS
#undef V3_CHI_DEFINE_MONITOR_RN_SEPARATE_PIN_TRAITS
#undef V3_CHI_DEFINE_MONITOR_SN_SEPARATE_PIN_TRAITS


        // --------------------------------------------------------------------
        // Separate-field flit drive / sample helpers.
        // These are invoked by the channel policies using the traits structs above.
        // --------------------------------------------------------------------

#define V3_CHI_REQ_FIELD_DRIVE(name)                                        \
    if constexpr (requires { Traits::name(verilated) = flit.name(); })      \
        Traits::name(verilated) = flit.name();
#define V3_CHI_RSP_FIELD_DRIVE(name)                                        \
    if constexpr (requires { Traits::name(verilated) = flit.name(); })      \
        Traits::name(verilated) = flit.name();
#define V3_CHI_DAT_FIELD_DRIVE(name)                                        \
    if constexpr (std::is_array_v<std::remove_reference_t<decltype(flit.name())>>) \
    {                                                                       \
        if constexpr (requires { std::memcpy(&Traits::name(verilated), flit.name(), sizeof(flit.name())); }) \
            std::memcpy(&Traits::name(verilated), flit.name(), sizeof(flit.name())); \
    }                                                                       \
    else if constexpr (requires { Traits::name(verilated) = flit.name(); }) \
        Traits::name(verilated) = flit.name();
#define V3_CHI_SNP_FIELD_DRIVE(name)                                        \
    if constexpr (requires { Traits::name(verilated) = flit.name(); })      \
        Traits::name(verilated) = flit.name();

#define V3_CHI_REQ_FIELD_SAMPLE(name)                                       \
    if constexpr (requires { flit.name() = Traits::name(verilated); })      \
        flit.name() = Traits::name(verilated);
#define V3_CHI_RSP_FIELD_SAMPLE(name)                                       \
    if constexpr (requires { flit.name() = Traits::name(verilated); })      \
        flit.name() = Traits::name(verilated);
#define V3_CHI_DAT_FIELD_SAMPLE(name)                                       \
    if constexpr (std::is_array_v<std::remove_reference_t<decltype(flit.name())>>) \
    {                                                                       \
        if constexpr (requires { std::memcpy(flit.name(), &Traits::name(verilated), sizeof(flit.name())); }) \
            std::memcpy(flit.name(), &Traits::name(verilated), sizeof(flit.name())); \
    }                                                                       \
    else if constexpr (requires { flit.name() = Traits::name(verilated); }) \
        flit.name() = Traits::name(verilated);
#define V3_CHI_SNP_FIELD_SAMPLE(name)                                       \
    if constexpr (requires { flit.name() = Traits::name(verilated); })      \
        flit.name() = Traits::name(verilated);

        template<class VT, class Traits>
        inline void V3CHISeparateFieldDriveREQ(VT& verilated, const Cohestra::CHI::REQFlit& flit) noexcept
        {
            V3_CHI_REQ_FIELDS(V3_CHI_REQ_FIELD_DRIVE)
        }

        template<class VT, class Traits>
        inline void V3CHISeparateFieldDriveRSP(VT& verilated, const Cohestra::CHI::RSPFlit& flit) noexcept
        {
            V3_CHI_RSP_FIELDS(V3_CHI_RSP_FIELD_DRIVE)
        }

        template<class VT, class Traits>
        inline void V3CHISeparateFieldDriveDAT(VT& verilated, const Cohestra::CHI::DATFlit& flit) noexcept
        {
            V3_CHI_DAT_FIELDS(V3_CHI_DAT_FIELD_DRIVE)
        }

        template<class VT, class Traits>
        inline void V3CHISeparateFieldDriveSNP(VT& verilated, const Cohestra::CHI::SNPFlit& flit) noexcept
        {
            V3_CHI_SNP_FIELDS(V3_CHI_SNP_FIELD_DRIVE)
        }

        template<class VT, class Traits>
        inline void V3CHISeparateFieldSampleREQ(VT& verilated, Cohestra::CHI::REQFlit& flit) noexcept
        {
            V3_CHI_REQ_FIELDS(V3_CHI_REQ_FIELD_SAMPLE)
        }

        template<class VT, class Traits>
        inline void V3CHISeparateFieldSampleRSP(VT& verilated, Cohestra::CHI::RSPFlit& flit) noexcept
        {
            V3_CHI_RSP_FIELDS(V3_CHI_RSP_FIELD_SAMPLE)
        }

        template<class VT, class Traits>
        inline void V3CHISeparateFieldSampleDAT(VT& verilated, Cohestra::CHI::DATFlit& flit) noexcept
        {
            V3_CHI_DAT_FIELDS(V3_CHI_DAT_FIELD_SAMPLE)
        }

        template<class VT, class Traits>
        inline void V3CHISeparateFieldSampleSNP(VT& verilated, Cohestra::CHI::SNPFlit& flit) noexcept
        {
            V3_CHI_SNP_FIELDS(V3_CHI_SNP_FIELD_SAMPLE)
        }

#undef V3_CHI_REQ_FIELD_DRIVE
#undef V3_CHI_RSP_FIELD_DRIVE
#undef V3_CHI_DAT_FIELD_DRIVE
#undef V3_CHI_SNP_FIELD_DRIVE
#undef V3_CHI_REQ_FIELD_SAMPLE
#undef V3_CHI_RSP_FIELD_SAMPLE
#undef V3_CHI_DAT_FIELD_SAMPLE
#undef V3_CHI_SNP_FIELD_SAMPLE

        // --------------------------------------------------------------------
        // Packed-flit pin traits: only 4 pins per channel.
        // --------------------------------------------------------------------

#define V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(PORT)                            \
        template<class VT>                                                  \
        struct V3CHIPackedRNPinTraits<VT, PORT, TXREQChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_rn ## PORT ## _txreq_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedRNPinTraits<VT, PORT, TXRSPChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_rn ## PORT ## _txrsp_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedRNPinTraits<VT, PORT, TXDATChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_rn ## PORT ## _txdat_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedRNPinTraits<VT, PORT, RXRSPChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _rxrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxrsp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_rn ## PORT ## _rxrsp_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedRNPinTraits<VT, PORT, RXDATChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_rn ## PORT ## _rxdat_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedRNPinTraits<VT, PORT, RXSNPChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsnp_bits; } \
        };

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedRNPinTraits;

        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(0)
        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(1)
        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(2)
        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(3)
        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(4)
        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(5)
        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(6)
        V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS(7)

#undef V3_CHI_DEFINE_RN_PACKED_PIN_TRAITS

#define V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(PORT)                            \
        template<class VT>                                                  \
        struct V3CHIPackedSNPinTraits<VT, PORT, RXREQChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_sn ## PORT ## _rxreq_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedSNPinTraits<VT, PORT, TXRSPChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_sn ## PORT ## _txrsp_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedSNPinTraits<VT, PORT, TXDATChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_sn ## PORT ## _txdat_bits; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedSNPinTraits<VT, PORT, RXDATChannel> {             \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.chi_sn ## PORT ## _rxdat_bits; } \
        };

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedSNPinTraits;

        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(0)
        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(1)
        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(2)
        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(3)
        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(4)
        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(5)
        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(6)
        V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS(7)

#undef V3_CHI_DEFINE_SN_PACKED_PIN_TRAITS

#define V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(PORT)                    \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorRNPinTraits<VT, PORT, TXREQChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txreq_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorRNPinTraits<VT, PORT, TXRSPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txrsp_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorRNPinTraits<VT, PORT, TXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _txdat_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorRNPinTraits<VT, PORT, RXRSPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxrsp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxrsp_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorRNPinTraits<VT, PORT, RXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxdat_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorRNPinTraits<VT, PORT, RXSNPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_rn ## PORT ## _rxsnp_flit; } \
        };

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedMonitorRNPinTraits;

        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(0)
        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(1)
        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(2)
        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(3)
        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(4)
        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(5)
        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(6)
        V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS(7)

#undef V3_CHI_DEFINE_MONITOR_RN_PACKED_PIN_TRAITS

#define V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(PORT)                    \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorSNPinTraits<VT, PORT, RXREQChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxreq_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorSNPinTraits<VT, PORT, TXRSPChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txrsp_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorSNPinTraits<VT, PORT, TXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _txdat_flit; } \
        };                                                                  \
        template<class VT>                                                  \
        struct V3CHIPackedMonitorSNPinTraits<VT, PORT, RXDATChannel> {      \
            static inline uint8_t& FlitPend(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_flitpend; } \
            static inline uint8_t& FlitV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_flitv; }    \
            static inline uint8_t& LCrdV(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_lcrdv; }    \
            static inline auto& Bits(VT& v) noexcept { return v.mon_chi_sn ## PORT ## _rxdat_flit; } \
        };

        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedMonitorSNPinTraits;

        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(0)
        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(1)
        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(2)
        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(3)
        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(4)
        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(5)
        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(6)
        V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS(7)

#undef V3_CHI_DEFINE_MONITOR_SN_PACKED_PIN_TRAITS

        // --------------------------------------------------------------------
        // Packed-flit pack / unpack helpers.
        // These use the bit-range constants from the CHI::Eb flit classes.
        // --------------------------------------------------------------------

        inline void V3CHIPackREQ(const Cohestra::CHI::REQFlit& flit, uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::SerializeREQ(flit, bits, Cohestra::CHI::REQFlit::WIDTH);
        }

        inline void V3CHIPackRSP(const Cohestra::CHI::RSPFlit& flit, uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::SerializeRSP(flit, bits, Cohestra::CHI::RSPFlit::WIDTH);
        }

        inline void V3CHIPackDAT(const Cohestra::CHI::DATFlit& flit, uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::SerializeDAT(flit, bits, Cohestra::CHI::DATFlit::WIDTH);
        }

        inline void V3CHIPackSNP(const Cohestra::CHI::SNPFlit& flit, uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::SerializeSNP(flit, bits, Cohestra::CHI::SNPFlit::WIDTH);
        }

        inline void V3CHIUnpackREQ(Cohestra::CHI::REQFlit& flit, const uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::DeserializeREQ(flit, const_cast<uint32_t*>(bits), Cohestra::CHI::REQFlit::WIDTH);
        }

        inline void V3CHIUnpackRSP(Cohestra::CHI::RSPFlit& flit, const uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::DeserializeRSP(flit, const_cast<uint32_t*>(bits), Cohestra::CHI::RSPFlit::WIDTH);
        }

        inline void V3CHIUnpackDAT(Cohestra::CHI::DATFlit& flit, const uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::DeserializeDAT(flit, const_cast<uint32_t*>(bits), Cohestra::CHI::DATFlit::WIDTH);
        }

        inline void V3CHIUnpackSNP(Cohestra::CHI::SNPFlit& flit, const uint32_t* bits) noexcept
        {
            ::CHI::Eb::Flits::DeserializeSNP(flit, const_cast<uint32_t*>(bits), Cohestra::CHI::SNPFlit::WIDTH);
        }


        // --------------------------------------------------------------------
        // Channel policies.
        // A policy knows how to read/write one channel's pins for a specific
        // port, pin style, and side (RN / SN / monitor).
        // --------------------------------------------------------------------

        // RN-side separate-field policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldRNChannelPolicy {
            using Traits = V3CHISeparateFieldRNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT& verilated, const void* flitVoid, bool valid) noexcept
            {
                Traits::FlitV(verilated) = valid ? 1 : 0;
                if (!valid)
                    return;

                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)
                    V3CHISeparateFieldDriveREQ<VT, Traits>(verilated, *static_cast<const Cohestra::CHI::REQFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>)
                    V3CHISeparateFieldDriveRSP<VT, Traits>(verilated, *static_cast<const Cohestra::CHI::RSPFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHISeparateFieldDriveDAT<VT, Traits>(verilated, *static_cast<const Cohestra::CHI::DATFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)
                    V3CHISeparateFieldDriveSNP<VT, Traits>(verilated, *static_cast<const Cohestra::CHI::SNPFlit*>(flitVoid));
            }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)
                    V3CHISeparateFieldSampleREQ<VT, Traits>(verilated, *static_cast<Cohestra::CHI::REQFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>)
                    V3CHISeparateFieldSampleRSP<VT, Traits>(verilated, *static_cast<Cohestra::CHI::RSPFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHISeparateFieldSampleDAT<VT, Traits>(verilated, *static_cast<Cohestra::CHI::DATFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)
                    V3CHISeparateFieldSampleSNP<VT, Traits>(verilated, *static_cast<Cohestra::CHI::SNPFlit*>(flitVoid));
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept
            {
                return Traits::LCrdV(verilated) != 0;
            }

            static void DriveLCrdV(VT& verilated, bool value) noexcept
            {
                Traits::LCrdV(verilated) = value ? 1 : 0;
            }

            static void ClearValid(VT& verilated) noexcept
            {
                Traits::FlitV(verilated) = 0;
                Traits::FlitPend(verilated) = 0;
            }

            static void ClearLCrdV(VT& verilated) noexcept
            {
                Traits::LCrdV(verilated) = 0;
            }

            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>) return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)  return Cohestra::CHI::SNPFlit::WIDTH;
                return 0;
            }
        };

        // SN-side separate-field policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldSNChannelPolicy {
            using Traits = V3CHISeparateFieldSNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT& verilated, const void* flitVoid, bool valid) noexcept
            {
                Traits::FlitV(verilated) = valid ? 1 : 0;
                if (!valid)
                    return;

                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)
                    V3CHISeparateFieldDriveREQ<VT, Traits>(verilated, *static_cast<const Cohestra::CHI::REQFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)
                    V3CHISeparateFieldDriveRSP<VT, Traits>(verilated, *static_cast<const Cohestra::CHI::RSPFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHISeparateFieldDriveDAT<VT, Traits>(verilated, *static_cast<const Cohestra::CHI::DATFlit*>(flitVoid));
            }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)
                    V3CHISeparateFieldSampleREQ<VT, Traits>(verilated, *static_cast<Cohestra::CHI::REQFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)
                    V3CHISeparateFieldSampleRSP<VT, Traits>(verilated, *static_cast<Cohestra::CHI::RSPFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHISeparateFieldSampleDAT<VT, Traits>(verilated, *static_cast<Cohestra::CHI::DATFlit*>(flitVoid));
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept { return Traits::LCrdV(verilated) != 0; }
            static void DriveLCrdV(VT& verilated, bool value) noexcept { Traits::LCrdV(verilated) = value ? 1 : 0; }
            static void ClearValid(VT& verilated) noexcept { Traits::FlitV(verilated) = 0; Traits::FlitPend(verilated) = 0; }
            static void ClearLCrdV(VT& verilated) noexcept { Traits::LCrdV(verilated) = 0; }

            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)  return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                return 0;
            }
        };

        // Monitor RN separate-field policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldMonitorRNChannelPolicy {
            using Traits = V3CHISeparateFieldMonitorRNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT&, const void*, bool) noexcept { }
            static void DriveLCrdV(VT&, bool) noexcept { }
            static void ClearValid(VT&) noexcept { }
            static void ClearLCrdV(VT&) noexcept { }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)
                    V3CHISeparateFieldSampleREQ<VT, Traits>(verilated, *static_cast<Cohestra::CHI::REQFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>)
                    V3CHISeparateFieldSampleRSP<VT, Traits>(verilated, *static_cast<Cohestra::CHI::RSPFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHISeparateFieldSampleDAT<VT, Traits>(verilated, *static_cast<Cohestra::CHI::DATFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)
                    V3CHISeparateFieldSampleSNP<VT, Traits>(verilated, *static_cast<Cohestra::CHI::SNPFlit*>(flitVoid));
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept { return Traits::LCrdV(verilated) != 0; }
            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>) return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)  return Cohestra::CHI::SNPFlit::WIDTH;
                return 0;
            }
        };

        // Monitor SN separate-field policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHISeparateFieldMonitorSNChannelPolicy {
            using Traits = V3CHISeparateFieldMonitorSNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT&, const void*, bool) noexcept { }
            static void DriveLCrdV(VT&, bool) noexcept { }
            static void ClearValid(VT&) noexcept { }
            static void ClearLCrdV(VT&) noexcept { }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)
                    V3CHISeparateFieldSampleREQ<VT, Traits>(verilated, *static_cast<Cohestra::CHI::REQFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)
                    V3CHISeparateFieldSampleRSP<VT, Traits>(verilated, *static_cast<Cohestra::CHI::RSPFlit*>(flitVoid));
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHISeparateFieldSampleDAT<VT, Traits>(verilated, *static_cast<Cohestra::CHI::DATFlit*>(flitVoid));
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept { return Traits::LCrdV(verilated) != 0; }
            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)  return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                return 0;
            }
        };

        // RN-side packed-flit policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedRNChannelPolicy {
            using Traits = V3CHIPackedRNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT& verilated, const void* flitVoid, bool valid) noexcept
            {
                Traits::FlitV(verilated) = valid ? 1 : 0;
                if (!valid)
                    return;

                constexpr size_t width = FlitWidth();
                uint32_t bits[(width + 31) / 32] = {};

                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)
                    V3CHIPackREQ(*static_cast<const Cohestra::CHI::REQFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>)
                    V3CHIPackRSP(*static_cast<const Cohestra::CHI::RSPFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHIPackDAT(*static_cast<const Cohestra::CHI::DATFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)
                    V3CHIPackSNP(*static_cast<const Cohestra::CHI::SNPFlit*>(flitVoid), bits);

                V3CHICopyFlitBitsToSignal(Traits::Bits(verilated), bits, width);
            }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                constexpr size_t width = FlitWidth();
                uint32_t bits[(width + 31) / 32] = {};
                V3CHICopySignalToFlitBits(Traits::Bits(verilated), bits, width);

                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)
                    V3CHIUnpackREQ(*static_cast<Cohestra::CHI::REQFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>)
                    V3CHIUnpackRSP(*static_cast<Cohestra::CHI::RSPFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHIUnpackDAT(*static_cast<Cohestra::CHI::DATFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)
                    V3CHIUnpackSNP(*static_cast<Cohestra::CHI::SNPFlit*>(flitVoid), bits);
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept { return Traits::LCrdV(verilated) != 0; }
            static void DriveLCrdV(VT& verilated, bool value) noexcept { Traits::LCrdV(verilated) = value ? 1 : 0; }
            static void ClearValid(VT& verilated) noexcept { Traits::FlitV(verilated) = 0; Traits::FlitPend(verilated) = 0; }
            static void ClearLCrdV(VT& verilated) noexcept { Traits::LCrdV(verilated) = 0; }

            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>) return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)  return Cohestra::CHI::SNPFlit::WIDTH;
                return 0;
            }
        };

        // SN-side packed-flit policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedSNChannelPolicy {
            using Traits = V3CHIPackedSNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT& verilated, const void* flitVoid, bool valid) noexcept
            {
                Traits::FlitV(verilated) = valid ? 1 : 0;
                if (!valid)
                    return;

                constexpr size_t width = FlitWidth();
                uint32_t bits[(width + 31) / 32] = {};

                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)
                    V3CHIPackREQ(*static_cast<const Cohestra::CHI::REQFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)
                    V3CHIPackRSP(*static_cast<const Cohestra::CHI::RSPFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHIPackDAT(*static_cast<const Cohestra::CHI::DATFlit*>(flitVoid), bits);

                V3CHICopyFlitBitsToSignal(Traits::Bits(verilated), bits, width);
            }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                constexpr size_t width = FlitWidth();
                uint32_t bits[(width + 31) / 32] = {};
                V3CHICopySignalToFlitBits(Traits::Bits(verilated), bits, width);

                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)
                    V3CHIUnpackREQ(*static_cast<Cohestra::CHI::REQFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)
                    V3CHIUnpackRSP(*static_cast<Cohestra::CHI::RSPFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHIUnpackDAT(*static_cast<Cohestra::CHI::DATFlit*>(flitVoid), bits);
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept { return Traits::LCrdV(verilated) != 0; }
            static void DriveLCrdV(VT& verilated, bool value) noexcept { Traits::LCrdV(verilated) = value ? 1 : 0; }
            static void ClearValid(VT& verilated) noexcept { Traits::FlitV(verilated) = 0; Traits::FlitPend(verilated) = 0; }
            static void ClearLCrdV(VT& verilated) noexcept { Traits::LCrdV(verilated) = 0; }

            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)  return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                return 0;
            }
        };

        // Monitor RN packed-flit policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedMonitorRNChannelPolicy {
            using Traits = V3CHIPackedMonitorRNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT&, const void*, bool) noexcept { }
            static void DriveLCrdV(VT&, bool) noexcept { }
            static void ClearValid(VT&) noexcept { }
            static void ClearLCrdV(VT&) noexcept { }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                constexpr size_t width = FlitWidth();
                uint32_t bits[(width + 31) / 32] = {};
                V3CHICopySignalToFlitBits(Traits::Bits(verilated), bits, width);

                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)
                    V3CHIUnpackREQ(*static_cast<Cohestra::CHI::REQFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>)
                    V3CHIUnpackRSP(*static_cast<Cohestra::CHI::RSPFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHIUnpackDAT(*static_cast<Cohestra::CHI::DATFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)
                    V3CHIUnpackSNP(*static_cast<Cohestra::CHI::SNPFlit*>(flitVoid), bits);
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept { return Traits::LCrdV(verilated) != 0; }
            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, TXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel> || std::is_same_v<ChannelTag, RXRSPChannel>) return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, RXSNPChannel>)  return Cohestra::CHI::SNPFlit::WIDTH;
                return 0;
            }
        };

        // Monitor SN packed-flit policy.
        template<class VT, size_t PORT, class ChannelTag>
        struct V3CHIPackedMonitorSNChannelPolicy {
            using Traits = V3CHIPackedMonitorSNPinTraits<VT, PORT, ChannelTag>;

            static void DriveFlit(VT&, const void*, bool) noexcept { }
            static void DriveLCrdV(VT&, bool) noexcept { }
            static void ClearValid(VT&) noexcept { }
            static void ClearLCrdV(VT&) noexcept { }

            static bool SampleFlit(VT& verilated, void* flitVoid) noexcept
            {
                if (!(Traits::FlitV(verilated)))
                    return false;

                constexpr size_t width = FlitWidth();
                uint32_t bits[(width + 31) / 32] = {};
                V3CHICopySignalToFlitBits(Traits::Bits(verilated), bits, width);

                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)
                    V3CHIUnpackREQ(*static_cast<Cohestra::CHI::REQFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)
                    V3CHIUnpackRSP(*static_cast<Cohestra::CHI::RSPFlit*>(flitVoid), bits);
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>)
                    V3CHIUnpackDAT(*static_cast<Cohestra::CHI::DATFlit*>(flitVoid), bits);
                return true;
            }

            static bool SampleLCrdV(VT& verilated) noexcept { return Traits::LCrdV(verilated) != 0; }
            static constexpr size_t FlitWidth() noexcept
            {
                if constexpr (std::is_same_v<ChannelTag, RXREQChannel>)       return Cohestra::CHI::REQFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXRSPChannel>)  return Cohestra::CHI::RSPFlit::WIDTH;
                else if constexpr (std::is_same_v<ChannelTag, TXDATChannel> || std::is_same_v<ChannelTag, RXDATChannel>) return Cohestra::CHI::DATFlit::WIDTH;
                return 0;
            }
        };

#undef V3_CHI_REQ_FIELDS
#undef V3_CHI_RSP_FIELDS
#undef V3_CHI_DAT_FIELDS
#undef V3_CHI_SNP_FIELDS


        // --------------------------------------------------------------------
        // Credit channel helper.
        // One instance per channel per port. Owns a TX buffer for flits the
        // harness wants to drive, and an RX observation queue for flits arriving
        // from the DUT. Credit counters implement CHI L-Credit flow control.
        // --------------------------------------------------------------------

        template<class TFlit>
        class CHICreditChannel {
        public:
            ResizeableFIFO<TFlit>           txBuffer;
            std::deque<TFlit>               rxObservations;
            std::deque<CHI::FiredFlit<TFlit>> firedObservations;

            int                             txCredits       = 0;
            int                             rxCredits       = 0;
            int                             rxPendingReturns= 0;

            bool                            txDriven        = false;

        public:
            bool Push(const TFlit& flit) noexcept
            {
                return txBuffer.Push(flit);
            }

            void SetBufferSize(size_t size) noexcept
            {
                txBuffer.TryResize(size);
            }

            void SetRxCreditCount(size_t count) noexcept
            {
                rxPendingReturns = static_cast<int>(count);
            }

            // Drive phase: place the head flit on the pins if we have credits.
            template<class VT, template<class, size_t, class> class ChannelPolicy, size_t PORT, class ChannelTag>
            void Drive(VT& verilated) noexcept
            {
                ChannelPolicy<VT, PORT, ChannelTag>::ClearValid(verilated);

                if (!txDriven && !txBuffer.IsEmpty() && txCredits > 0)
                {
                    const TFlit* head = txBuffer.Peek();
                    ChannelPolicy<VT, PORT, ChannelTag>::DriveFlit(verilated, const_cast<void*>(static_cast<const void*>(head)), true);
                    txDriven = true;
                }
            }

            // Retire phase: consume driven flits and observe returned credits.
            template<class VT, template<class, size_t, class> class ChannelPolicy, size_t PORT, class ChannelTag>
            void Retire(VT& verilated) noexcept
            {
                if (txDriven)
                {
                    if (ChannelPolicy<VT, PORT, ChannelTag>::SampleLCrdV(verilated))
                    {
                        txBuffer.Pop();
                        --txCredits;
                        txDriven = false;
                    }
                }

                // DUT may return a credit even when we are not driving.
                if (ChannelPolicy<VT, PORT, ChannelTag>::SampleLCrdV(verilated))
                    ++txCredits;
            }

            // Observe phase: capture inbound flits and decrement available credits.
            template<class VT, template<class, size_t, class> class ChannelPolicy, size_t PORT, class ChannelTag>
            void Observe(VT& verilated, uint64_t time) noexcept
            {
                (void)time;
                if (rxCredits > 0)
                {
                    TFlit flit {};
                    if (ChannelPolicy<VT, PORT, ChannelTag>::SampleFlit(verilated, static_cast<void*>(&flit)))
                    {
                        rxObservations.push_back(flit);
                        --rxCredits;
                    }
                }
            }

            // Drive phase for receiver-side channels: return credits to the DUT.
            template<class VT, template<class, size_t, class> class ChannelPolicy, size_t PORT, class ChannelTag>
            void ReturnCredits(VT& verilated) noexcept
            {
                ChannelPolicy<VT, PORT, ChannelTag>::ClearLCrdV(verilated);
                if (rxPendingReturns > 0)
                {
                    ChannelPolicy<VT, PORT, ChannelTag>::DriveLCrdV(verilated, true);
                    --rxPendingReturns;
                    ++rxCredits;
                }
            }

            bool HasObservation() const noexcept
            {
                return !rxObservations.empty();
            }

            const TFlit* PeekObservation() const noexcept
            {
                return rxObservations.empty() ? nullptr : &rxObservations.front();
            }

            std::optional<TFlit> PopObservation() noexcept
            {
                if (rxObservations.empty())
                    return std::nullopt;

                TFlit flit = rxObservations.front();
                rxObservations.pop_front();
                ++rxPendingReturns;
                return { flit };
            }

            // Monitor-only observation: capture completed handshakes
            // (FLITV && LCRDV) with a timestamp.
            template<class VT, template<class, size_t, class> class ChannelPolicy, size_t PORT, class ChannelTag>
            void MonitorObserve(VT& verilated, uint64_t time) noexcept
            {
                if (!ChannelPolicy<VT, PORT, ChannelTag>::SampleLCrdV(verilated))
                    return;

                TFlit flit {};
                if (!ChannelPolicy<VT, PORT, ChannelTag>::SampleFlit(verilated, static_cast<void*>(&flit)))
                    return;

                firedObservations.push_back(CHI::FiredFlit<TFlit>{ time, flit });
            }

            bool HasFiredObservation() const noexcept
            {
                return !firedObservations.empty();
            }

            const CHI::FiredFlit<TFlit>* PeekFiredObservation() const noexcept
            {
                return firedObservations.empty() ? nullptr : &firedObservations.front();
            }

            std::optional<CHI::FiredFlit<TFlit>> PopFiredObservation() noexcept
            {
                if (firedObservations.empty())
                    return std::nullopt;

                CHI::FiredFlit<TFlit> fired = firedObservations.front();
                firedObservations.pop_front();
                return { fired };
            }
        };


        // ----------------------------------------------------------------
        // CHI port presence concepts.
        // A port is considered present when the first channel's control
        // pins (flitpend / flitv / lcrdv) exist on the verilated module.
        // The same concept is used for both separate-field and packed-flit
        // pin styles; the alias used by the harness selects the policy that
        // matches the module's pin naming.
        // ----------------------------------------------------------------

#define COHESTRA_V3_CHI_RN_PORT_REQUIREMENTS(PORT)                          \
        { t.chi_rn ## PORT ## _txreq_flitpend } -> std::convertible_to<uint8_t&>; \
        { t.chi_rn ## PORT ## _txreq_flitv    } -> std::convertible_to<uint8_t&>; \
        { t.chi_rn ## PORT ## _txreq_lcrdv    } -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_CHI_SN_PORT_REQUIREMENTS(PORT)                          \
        { t.chi_sn ## PORT ## _rxreq_flitpend } -> std::convertible_to<uint8_t&>; \
        { t.chi_sn ## PORT ## _rxreq_flitv    } -> std::convertible_to<uint8_t&>; \
        { t.chi_sn ## PORT ## _rxreq_lcrdv    } -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_CHI_MONITOR_RN_PORT_REQUIREMENTS(PORT)                  \
        { t.mon_chi_rn ## PORT ## _txreq_flitpend } -> std::convertible_to<uint8_t&>; \
        { t.mon_chi_rn ## PORT ## _txreq_flitv    } -> std::convertible_to<uint8_t&>; \
        { t.mon_chi_rn ## PORT ## _txreq_lcrdv    } -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_CHI_MONITOR_SN_PORT_REQUIREMENTS(PORT)                  \
        { t.mon_chi_sn ## PORT ## _rxreq_flitpend } -> std::convertible_to<uint8_t&>; \
        { t.mon_chi_sn ## PORT ## _rxreq_flitv    } -> std::convertible_to<uint8_t&>; \
        { t.mon_chi_sn ## PORT ## _rxreq_lcrdv    } -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(PORT)                      \
        template<class T>                                                   \
        concept V3CHIModuleRNPort ## PORT = requires (T t) {                \
            COHESTRA_V3_CHI_RN_PORT_REQUIREMENTS(PORT)                      \
        };

#define COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(PORT)                      \
        template<class T>                                                   \
        concept V3CHIModuleSNPort ## PORT = requires (T t) {                \
            COHESTRA_V3_CHI_SN_PORT_REQUIREMENTS(PORT)                      \
        };

#define COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(PORT)              \
        template<class T>                                                   \
        concept V3CHIModuleMonitorRNPort ## PORT = requires (T t) {         \
            COHESTRA_V3_CHI_MONITOR_RN_PORT_REQUIREMENTS(PORT)              \
        };

#define COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(PORT)              \
        template<class T>                                                   \
        concept V3CHIModuleMonitorSNPort ## PORT = requires (T t) {         \
            COHESTRA_V3_CHI_MONITOR_SN_PORT_REQUIREMENTS(PORT)              \
        };

        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS(7)

        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS(7)

        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS(7)

        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS(7)

#undef COHESTRA_V3_DECLARE_CHI_MONITOR_SN_PORT_CONCEPTS
#undef COHESTRA_V3_DECLARE_CHI_MONITOR_RN_PORT_CONCEPTS
#undef COHESTRA_V3_DECLARE_CHI_SN_PORT_CONCEPTS
#undef COHESTRA_V3_DECLARE_CHI_RN_PORT_CONCEPTS
#undef COHESTRA_V3_CHI_MONITOR_SN_PORT_REQUIREMENTS
#undef COHESTRA_V3_CHI_MONITOR_RN_PORT_REQUIREMENTS
#undef COHESTRA_V3_CHI_SN_PORT_REQUIREMENTS
#undef COHESTRA_V3_CHI_RN_PORT_REQUIREMENTS

    } // namespace details


    // ----------------------------------------------------------------
    // V3CHIFromRNInterfaceT: RN-facing CHI interface of a verilated V3
    // module. The harness acts as the RN, driving TXREQ/TXRSP/TXDAT and
    // receiving RXRSP/RXDAT/RXSNP. Credit-based flow control is handled
    // automatically per channel.
    // ----------------------------------------------------------------

    template<class VT, template<class, size_t, class> class ChannelPolicy>
    class V3CHIFromRNInterfaceT : public CHI::CHIFromRNInterface {
    public:
        static constexpr bool HasPort0() noexcept { return details::V3CHIModuleRNPort0<VT>; }
        static constexpr bool HasPort1() noexcept { return details::V3CHIModuleRNPort1<VT>; }
        static constexpr bool HasPort2() noexcept { return details::V3CHIModuleRNPort2<VT>; }
        static constexpr bool HasPort3() noexcept { return details::V3CHIModuleRNPort3<VT>; }
        static constexpr bool HasPort4() noexcept { return details::V3CHIModuleRNPort4<VT>; }
        static constexpr bool HasPort5() noexcept { return details::V3CHIModuleRNPort5<VT>; }
        static constexpr bool HasPort6() noexcept { return details::V3CHIModuleRNPort6<VT>; }
        static constexpr bool HasPort7() noexcept { return details::V3CHIModuleRNPort7<VT>; }

        static constexpr bool HasPort(size_t index) noexcept
        {
            switch (index)
            {
                case 0: return HasPort0();
                case 1: return HasPort1();
                case 2: return HasPort2();
                case 3: return HasPort3();
                case 4: return HasPort4();
                case 5: return HasPort5();
                case 6: return HasPort6();
                case 7: return HasPort7();
                default: return false;
            }
        }

    protected:
        struct Port {
            details::CHICreditChannel<CHI::REQFlit> txreq;
            details::CHICreditChannel<CHI::RSPFlit> txrsp;
            details::CHICreditChannel<CHI::DATFlit> txdat;
            details::CHICreditChannel<CHI::RSPFlit> rxrsp;
            details::CHICreditChannel<CHI::DATFlit> rxdat;
            details::CHICreditChannel<CHI::SNPFlit> rxsnp;
        };

        VT*                 verilated           = nullptr;

        uint64_t            lastPreTickTime     = 0;
        bool                lastPreTickValid    = false;
        uint64_t            lastPostTickTime    = 0;
        bool                lastPostTickValid   = false;

        std::set<size_t>    portIndices;
        std::unordered_map<size_t, Port>
                            ports;

    public:
        V3CHIFromRNInterfaceT(VT* verilated) noexcept
            : verilated(verilated)
        {
#define _INIT_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                portIndices.insert(port);                                   \
                auto& p = ports[port];                                      \
                p.txreq.SetBufferSize(4);                                   \
                p.txrsp.SetBufferSize(4);                                   \
                p.txdat.SetBufferSize(4);                                   \
                p.rxrsp.SetBufferSize(4);                                   \
                p.rxrsp.SetRxCreditCount(4);                                \
                p.rxdat.SetBufferSize(4);                                   \
                p.rxdat.SetRxCreditCount(4);                                \
                p.rxsnp.SetBufferSize(4);                                   \
                p.rxsnp.SetRxCreditCount(4);                                \
            }
            _INIT_PORT(0)
            _INIT_PORT(1)
            _INIT_PORT(2)
            _INIT_PORT(3)
            _INIT_PORT(4)
            _INIT_PORT(5)
            _INIT_PORT(6)
            _INIT_PORT(7)
#undef _INIT_PORT
        }

        virtual ~V3CHIFromRNInterfaceT() noexcept = default;

    public:
        virtual void TickPreHandshake(uint64_t time) noexcept override
        {
            if (lastPreTickValid && lastPreTickTime == time)
                return;
            lastPreTickTime = time;
            lastPreTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.txreq.template Drive<VT, ChannelPolicy, port, details::TXREQChannel>(*verilated); \
                p.txrsp.template Drive<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated); \
                p.txdat.template Drive<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated); \
                p.rxrsp.template ReturnCredits<VT, ChannelPolicy, port, details::RXRSPChannel>(*verilated); \
                p.rxdat.template ReturnCredits<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated); \
                p.rxsnp.template ReturnCredits<VT, ChannelPolicy, port, details::RXSNPChannel>(*verilated); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

        virtual void TickPostHandshake(uint64_t time) noexcept override
        {
            if (lastPostTickValid && lastPostTickTime == time)
                return;
            lastPostTickTime = time;
            lastPostTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.txreq.template Retire<VT, ChannelPolicy, port, details::TXREQChannel>(*verilated); \
                p.txrsp.template Retire<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated); \
                p.txdat.template Retire<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated); \
                p.rxrsp.template Observe<VT, ChannelPolicy, port, details::RXRSPChannel>(*verilated, time); \
                p.rxdat.template Observe<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated, time); \
                p.rxsnp.template Observe<VT, ChannelPolicy, port, details::RXSNPChannel>(*verilated, time); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

    public:
        virtual size_t GetPortCount() const noexcept override
        {
            return portIndices.size();
        }

        virtual std::set<size_t> GetPortIndices() const noexcept override
        {
            return portIndices;
        }

        virtual size_t GetPortMaxIndex() const noexcept override
        {
            if (portIndices.empty())
                return 0;
            return *portIndices.rbegin();
        }

    public:
        virtual bool PushTXREQ(size_t index, const CHI::REQFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return false;
            return it->second.txreq.Push(flit);
        }

        virtual bool PushTXRSP(size_t index, const CHI::RSPFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return false;
            return it->second.txrsp.Push(flit);
        }

        virtual bool PushTXDAT(size_t index, const CHI::DATFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return false;
            return it->second.txdat.Push(flit);
        }

        virtual bool HasRXRSP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return false;
            return it->second.rxrsp.HasObservation();
        }

        virtual std::optional<CHI::RSPFlit> PeekRXRSP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.rxrsp.PeekObservation();
            if (!obs)
                return std::nullopt;
            return *obs;
        }

        virtual std::optional<CHI::RSPFlit> PopRXRSP(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.rxrsp.PopObservation();
        }

        virtual bool HasRXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return false;
            return it->second.rxdat.HasObservation();
        }

        virtual std::optional<CHI::DATFlit> PeekRXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.rxdat.PeekObservation();
            if (!obs)
                return std::nullopt;
            return *obs;
        }

        virtual std::optional<CHI::DATFlit> PopRXDAT(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.rxdat.PopObservation();
        }

        virtual bool HasRXSNP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return false;
            return it->second.rxsnp.HasObservation();
        }

        virtual std::optional<CHI::SNPFlit> PeekRXSNP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.rxsnp.PeekObservation();
            if (!obs)
                return std::nullopt;
            return *obs;
        }

        virtual std::optional<CHI::SNPFlit> PopRXSNP(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.rxsnp.PopObservation();
        }

    public:
        virtual bool SetAllTXREQBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.txreq.SetBufferSize(bufferSize);
            return true;
        }

        virtual bool SetAllTXRSPBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.txrsp.SetBufferSize(bufferSize);
            return true;
        }

        virtual bool SetAllTXDATBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.txdat.SetBufferSize(bufferSize);
            return true;
        }
    };


    // ----------------------------------------------------------------
    // V3CHIToRNInterfaceT: downstream-facing CHI interface towards an RN
    // DUT. The harness drives RXRSP/RXDAT/RXSNP and observes TXREQ/TXRSP
    // /TXDAT.
    // ----------------------------------------------------------------

    template<class VT, template<class, size_t, class> class ChannelPolicy>
    class V3CHIToRNInterfaceT : public CHI::CHIToRNInterface {
    public:
        static constexpr bool HasPort0() noexcept { return details::V3CHIModuleRNPort0<VT>; }
        static constexpr bool HasPort1() noexcept { return details::V3CHIModuleRNPort1<VT>; }
        static constexpr bool HasPort2() noexcept { return details::V3CHIModuleRNPort2<VT>; }
        static constexpr bool HasPort3() noexcept { return details::V3CHIModuleRNPort3<VT>; }
        static constexpr bool HasPort4() noexcept { return details::V3CHIModuleRNPort4<VT>; }
        static constexpr bool HasPort5() noexcept { return details::V3CHIModuleRNPort5<VT>; }
        static constexpr bool HasPort6() noexcept { return details::V3CHIModuleRNPort6<VT>; }
        static constexpr bool HasPort7() noexcept { return details::V3CHIModuleRNPort7<VT>; }

        static constexpr bool HasPort(size_t index) noexcept
        {
            switch (index)
            {
                case 0: return HasPort0();
                case 1: return HasPort1();
                case 2: return HasPort2();
                case 3: return HasPort3();
                case 4: return HasPort4();
                case 5: return HasPort5();
                case 6: return HasPort6();
                case 7: return HasPort7();
                default: return false;
            }
        }

    protected:
        struct Port {
            details::CHICreditChannel<CHI::RSPFlit> rxrsp;
            details::CHICreditChannel<CHI::DATFlit> rxdat;
            details::CHICreditChannel<CHI::SNPFlit> rxsnp;
            details::CHICreditChannel<CHI::REQFlit> txreq;
            details::CHICreditChannel<CHI::RSPFlit> txrsp;
            details::CHICreditChannel<CHI::DATFlit> txdat;
        };

        VT*                 verilated           = nullptr;

        uint64_t            lastPreTickTime     = 0;
        bool                lastPreTickValid    = false;
        uint64_t            lastPostTickTime    = 0;
        bool                lastPostTickValid   = false;

        std::set<size_t>    portIndices;
        std::unordered_map<size_t, Port>
                            ports;

    public:
        V3CHIToRNInterfaceT(VT* verilated) noexcept
            : verilated(verilated)
        {
#define _INIT_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                portIndices.insert(port);                                   \
                auto& p = ports[port];                                      \
                p.rxrsp.SetBufferSize(4);                                   \
                p.rxrsp.SetRxCreditCount(4);                                \
                p.rxdat.SetBufferSize(4);                                   \
                p.rxdat.SetRxCreditCount(4);                                \
                p.rxsnp.SetBufferSize(4);                                   \
                p.rxsnp.SetRxCreditCount(4);                                \
                p.txreq.SetBufferSize(4);                                   \
                p.txreq.SetRxCreditCount(4);                                \
                p.txrsp.SetBufferSize(4);                                   \
                p.txrsp.SetRxCreditCount(4);                                \
                p.txdat.SetBufferSize(4);                                   \
                p.txdat.SetRxCreditCount(4);                                \
            }
            _INIT_PORT(0)
            _INIT_PORT(1)
            _INIT_PORT(2)
            _INIT_PORT(3)
            _INIT_PORT(4)
            _INIT_PORT(5)
            _INIT_PORT(6)
            _INIT_PORT(7)
#undef _INIT_PORT
        }

        virtual ~V3CHIToRNInterfaceT() noexcept = default;

    public:
        virtual void TickPreHandshake(uint64_t time) noexcept override
        {
            if (lastPreTickValid && lastPreTickTime == time)
                return;
            lastPreTickTime = time;
            lastPreTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.rxrsp.template Drive<VT, ChannelPolicy, port, details::RXRSPChannel>(*verilated); \
                p.rxdat.template Drive<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated); \
                p.rxsnp.template Drive<VT, ChannelPolicy, port, details::RXSNPChannel>(*verilated); \
                p.txreq.template ReturnCredits<VT, ChannelPolicy, port, details::TXREQChannel>(*verilated); \
                p.txrsp.template ReturnCredits<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated); \
                p.txdat.template ReturnCredits<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

        virtual void TickPostHandshake(uint64_t time) noexcept override
        {
            if (lastPostTickValid && lastPostTickTime == time)
                return;
            lastPostTickTime = time;
            lastPostTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.rxrsp.template Retire<VT, ChannelPolicy, port, details::RXRSPChannel>(*verilated); \
                p.rxdat.template Retire<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated); \
                p.rxsnp.template Retire<VT, ChannelPolicy, port, details::RXSNPChannel>(*verilated); \
                p.txreq.template Observe<VT, ChannelPolicy, port, details::TXREQChannel>(*verilated, time); \
                p.txrsp.template Observe<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated, time); \
                p.txdat.template Observe<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated, time); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

    public:
        virtual size_t GetPortCount() const noexcept override { return portIndices.size(); }
        virtual std::set<size_t> GetPortIndices() const noexcept override { return portIndices; }
        virtual size_t GetPortMaxIndex() const noexcept override
        {
            if (portIndices.empty())
                return 0;
            return *portIndices.rbegin();
        }

    public:
        virtual bool PushRXRSP(size_t index, const CHI::RSPFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.rxrsp.Push(flit);
        }

        virtual bool PushRXDAT(size_t index, const CHI::DATFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.rxdat.Push(flit);
        }

        virtual bool PushRXSNP(size_t index, const CHI::SNPFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.rxsnp.Push(flit);
        }

        virtual bool HasTXREQ(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.txreq.HasObservation();
        }

        virtual std::optional<CHI::REQFlit> PeekTXREQ(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.txreq.PeekObservation();
            return obs ? std::optional<CHI::REQFlit>(*obs) : std::nullopt;
        }

        virtual std::optional<CHI::REQFlit> PopTXREQ(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.txreq.PopObservation();
        }

        virtual bool HasTXRSP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.txrsp.HasObservation();
        }

        virtual std::optional<CHI::RSPFlit> PeekTXRSP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.txrsp.PeekObservation();
            return obs ? std::optional<CHI::RSPFlit>(*obs) : std::nullopt;
        }

        virtual std::optional<CHI::RSPFlit> PopTXRSP(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.txrsp.PopObservation();
        }

        virtual bool HasTXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.txdat.HasObservation();
        }

        virtual std::optional<CHI::DATFlit> PeekTXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.txdat.PeekObservation();
            return obs ? std::optional<CHI::DATFlit>(*obs) : std::nullopt;
        }

        virtual std::optional<CHI::DATFlit> PopTXDAT(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.txdat.PopObservation();
        }

    public:
        virtual bool SetAllRXRSPBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.rxrsp.SetBufferSize(bufferSize);
            return true;
        }

        virtual bool SetAllRXDATBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.rxdat.SetBufferSize(bufferSize);
            return true;
        }

        virtual bool SetAllRXSNPBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.rxsnp.SetBufferSize(bufferSize);
            return true;
        }
    };


    // ----------------------------------------------------------------
    // V3CHIFromSNInterfaceT: SN-facing CHI interface of a verilated V3
    // module. The harness acts as the SN, driving TXRSP/TXDAT and
    // receiving RXREQ/RXDAT.
    // ----------------------------------------------------------------

    template<class VT, template<class, size_t, class> class ChannelPolicy>
    class V3CHIFromSNInterfaceT : public CHI::CHIFromSNInterface {
    public:
        static constexpr bool HasPort0() noexcept { return details::V3CHIModuleSNPort0<VT>; }
        static constexpr bool HasPort1() noexcept { return details::V3CHIModuleSNPort1<VT>; }
        static constexpr bool HasPort2() noexcept { return details::V3CHIModuleSNPort2<VT>; }
        static constexpr bool HasPort3() noexcept { return details::V3CHIModuleSNPort3<VT>; }
        static constexpr bool HasPort4() noexcept { return details::V3CHIModuleSNPort4<VT>; }
        static constexpr bool HasPort5() noexcept { return details::V3CHIModuleSNPort5<VT>; }
        static constexpr bool HasPort6() noexcept { return details::V3CHIModuleSNPort6<VT>; }
        static constexpr bool HasPort7() noexcept { return details::V3CHIModuleSNPort7<VT>; }

        static constexpr bool HasPort(size_t index) noexcept
        {
            switch (index)
            {
                case 0: return HasPort0();
                case 1: return HasPort1();
                case 2: return HasPort2();
                case 3: return HasPort3();
                case 4: return HasPort4();
                case 5: return HasPort5();
                case 6: return HasPort6();
                case 7: return HasPort7();
                default: return false;
            }
        }

    protected:
        struct Port {
            details::CHICreditChannel<CHI::RSPFlit> txrsp;
            details::CHICreditChannel<CHI::DATFlit> txdat;
            details::CHICreditChannel<CHI::REQFlit> rxreq;
            details::CHICreditChannel<CHI::DATFlit> rxdat;
        };

        VT*                 verilated           = nullptr;

        uint64_t            lastPreTickTime     = 0;
        bool                lastPreTickValid    = false;
        uint64_t            lastPostTickTime    = 0;
        bool                lastPostTickValid   = false;

        std::set<size_t>    portIndices;
        std::unordered_map<size_t, Port>
                            ports;

    public:
        V3CHIFromSNInterfaceT(VT* verilated) noexcept
            : verilated(verilated)
        {
#define _INIT_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                portIndices.insert(port);                                   \
                auto& p = ports[port];                                      \
                p.txrsp.SetBufferSize(4);                                   \
                p.txdat.SetBufferSize(4);                                   \
                p.rxreq.SetBufferSize(4);                                   \
                p.rxreq.SetRxCreditCount(4);                                \
                p.rxdat.SetBufferSize(4);                                   \
                p.rxdat.SetRxCreditCount(4);                                \
            }
            _INIT_PORT(0)
            _INIT_PORT(1)
            _INIT_PORT(2)
            _INIT_PORT(3)
            _INIT_PORT(4)
            _INIT_PORT(5)
            _INIT_PORT(6)
            _INIT_PORT(7)
#undef _INIT_PORT
        }

        virtual ~V3CHIFromSNInterfaceT() noexcept = default;

    public:
        virtual void TickPreHandshake(uint64_t time) noexcept override
        {
            if (lastPreTickValid && lastPreTickTime == time)
                return;
            lastPreTickTime = time;
            lastPreTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.txrsp.template Drive<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated); \
                p.txdat.template Drive<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated); \
                p.rxreq.template ReturnCredits<VT, ChannelPolicy, port, details::RXREQChannel>(*verilated); \
                p.rxdat.template ReturnCredits<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

        virtual void TickPostHandshake(uint64_t time) noexcept override
        {
            if (lastPostTickValid && lastPostTickTime == time)
                return;
            lastPostTickTime = time;
            lastPostTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.txrsp.template Retire<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated); \
                p.txdat.template Retire<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated); \
                p.rxreq.template Observe<VT, ChannelPolicy, port, details::RXREQChannel>(*verilated, time); \
                p.rxdat.template Observe<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated, time); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

    public:
        virtual size_t GetPortCount() const noexcept override { return portIndices.size(); }
        virtual std::set<size_t> GetPortIndices() const noexcept override { return portIndices; }
        virtual size_t GetPortMaxIndex() const noexcept override
        {
            if (portIndices.empty())
                return 0;
            return *portIndices.rbegin();
        }

    public:
        virtual bool PushTXRSP(size_t index, const CHI::RSPFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.txrsp.Push(flit);
        }

        virtual bool PushTXDAT(size_t index, const CHI::DATFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.txdat.Push(flit);
        }

        virtual bool HasRXREQ(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.rxreq.HasObservation();
        }

        virtual std::optional<CHI::REQFlit> PeekRXREQ(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.rxreq.PeekObservation();
            return obs ? std::optional<CHI::REQFlit>(*obs) : std::nullopt;
        }

        virtual std::optional<CHI::REQFlit> PopRXREQ(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.rxreq.PopObservation();
        }

        virtual bool HasRXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.rxdat.HasObservation();
        }

        virtual std::optional<CHI::DATFlit> PeekRXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.rxdat.PeekObservation();
            return obs ? std::optional<CHI::DATFlit>(*obs) : std::nullopt;
        }

        virtual std::optional<CHI::DATFlit> PopRXDAT(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.rxdat.PopObservation();
        }

    public:
        virtual bool SetAllTXRSPBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.txrsp.SetBufferSize(bufferSize);
            return true;
        }

        virtual bool SetAllTXDATBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.txdat.SetBufferSize(bufferSize);
            return true;
        }
    };


    // ----------------------------------------------------------------
    // V3CHIToSNInterfaceT: upstream-facing CHI interface towards an SN
    // DUT. The harness drives RXREQ/RXDAT and observes TXRSP/TXDAT.
    // ----------------------------------------------------------------

    template<class VT, template<class, size_t, class> class ChannelPolicy>
    class V3CHIToSNInterfaceT : public CHI::CHIToSNInterface {
    public:
        static constexpr bool HasPort0() noexcept { return details::V3CHIModuleSNPort0<VT>; }
        static constexpr bool HasPort1() noexcept { return details::V3CHIModuleSNPort1<VT>; }
        static constexpr bool HasPort2() noexcept { return details::V3CHIModuleSNPort2<VT>; }
        static constexpr bool HasPort3() noexcept { return details::V3CHIModuleSNPort3<VT>; }
        static constexpr bool HasPort4() noexcept { return details::V3CHIModuleSNPort4<VT>; }
        static constexpr bool HasPort5() noexcept { return details::V3CHIModuleSNPort5<VT>; }
        static constexpr bool HasPort6() noexcept { return details::V3CHIModuleSNPort6<VT>; }
        static constexpr bool HasPort7() noexcept { return details::V3CHIModuleSNPort7<VT>; }

        static constexpr bool HasPort(size_t index) noexcept
        {
            switch (index)
            {
                case 0: return HasPort0();
                case 1: return HasPort1();
                case 2: return HasPort2();
                case 3: return HasPort3();
                case 4: return HasPort4();
                case 5: return HasPort5();
                case 6: return HasPort6();
                case 7: return HasPort7();
                default: return false;
            }
        }

    protected:
        struct Port {
            details::CHICreditChannel<CHI::REQFlit> rxreq;
            details::CHICreditChannel<CHI::DATFlit> rxdat;
            details::CHICreditChannel<CHI::RSPFlit> txrsp;
            details::CHICreditChannel<CHI::DATFlit> txdat;
        };

        VT*                 verilated           = nullptr;

        uint64_t            lastPreTickTime     = 0;
        bool                lastPreTickValid    = false;
        uint64_t            lastPostTickTime    = 0;
        bool                lastPostTickValid   = false;

        std::set<size_t>    portIndices;
        std::unordered_map<size_t, Port>
                            ports;

    public:
        V3CHIToSNInterfaceT(VT* verilated) noexcept
            : verilated(verilated)
        {
#define _INIT_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                portIndices.insert(port);                                   \
                auto& p = ports[port];                                      \
                p.rxreq.SetBufferSize(4);                                   \
                p.rxreq.SetRxCreditCount(4);                                \
                p.rxdat.SetBufferSize(4);                                   \
                p.rxdat.SetRxCreditCount(4);                                \
                p.txrsp.SetBufferSize(4);                                   \
                p.txrsp.SetRxCreditCount(4);                                \
                p.txdat.SetBufferSize(4);                                   \
                p.txdat.SetRxCreditCount(4);                                \
            }
            _INIT_PORT(0)
            _INIT_PORT(1)
            _INIT_PORT(2)
            _INIT_PORT(3)
            _INIT_PORT(4)
            _INIT_PORT(5)
            _INIT_PORT(6)
            _INIT_PORT(7)
#undef _INIT_PORT
        }

        virtual ~V3CHIToSNInterfaceT() noexcept = default;

    public:
        virtual void TickPreHandshake(uint64_t time) noexcept override
        {
            if (lastPreTickValid && lastPreTickTime == time)
                return;
            lastPreTickTime = time;
            lastPreTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.rxreq.template Drive<VT, ChannelPolicy, port, details::RXREQChannel>(*verilated); \
                p.rxdat.template Drive<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated); \
                p.txrsp.template ReturnCredits<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated); \
                p.txdat.template ReturnCredits<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

        virtual void TickPostHandshake(uint64_t time) noexcept override
        {
            if (lastPostTickValid && lastPostTickTime == time)
                return;
            lastPostTickTime = time;
            lastPostTickValid = true;

#define _TICK_PORT(port)                                                    \
            if constexpr (HasPort##port())                                  \
            {                                                               \
                auto& p = ports[port];                                      \
                p.rxreq.template Retire<VT, ChannelPolicy, port, details::RXREQChannel>(*verilated); \
                p.rxdat.template Retire<VT, ChannelPolicy, port, details::RXDATChannel>(*verilated); \
                p.txrsp.template Observe<VT, ChannelPolicy, port, details::TXRSPChannel>(*verilated, time); \
                p.txdat.template Observe<VT, ChannelPolicy, port, details::TXDATChannel>(*verilated, time); \
            }
            _TICK_PORT(0)
            _TICK_PORT(1)
            _TICK_PORT(2)
            _TICK_PORT(3)
            _TICK_PORT(4)
            _TICK_PORT(5)
            _TICK_PORT(6)
            _TICK_PORT(7)
#undef _TICK_PORT
        }

    public:
        virtual size_t GetPortCount() const noexcept override { return portIndices.size(); }
        virtual std::set<size_t> GetPortIndices() const noexcept override { return portIndices; }
        virtual size_t GetPortMaxIndex() const noexcept override
        {
            if (portIndices.empty())
                return 0;
            return *portIndices.rbegin();
        }

    public:
        virtual bool PushRXREQ(size_t index, const CHI::REQFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.rxreq.Push(flit);
        }

        virtual bool PushRXDAT(size_t index, const CHI::DATFlit& flit) noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.rxdat.Push(flit);
        }

        virtual bool HasTXRSP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.txrsp.HasObservation();
        }

        virtual std::optional<CHI::RSPFlit> PeekTXRSP(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.txrsp.PeekObservation();
            return obs ? std::optional<CHI::RSPFlit>(*obs) : std::nullopt;
        }

        virtual std::optional<CHI::RSPFlit> PopTXRSP(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.txrsp.PopObservation();
        }

        virtual bool HasTXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            return it != ports.end() && it->second.txdat.HasObservation();
        }

        virtual std::optional<CHI::DATFlit> PeekTXDAT(size_t index) const noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            auto* obs = it->second.txdat.PeekObservation();
            return obs ? std::optional<CHI::DATFlit>(*obs) : std::nullopt;
        }

        virtual std::optional<CHI::DATFlit> PopTXDAT(size_t index) noexcept override
        {
            auto it = ports.find(index);
            if (it == ports.end())
                return std::nullopt;
            return it->second.txdat.PopObservation();
        }

    public:
        virtual bool SetAllRXREQBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.rxreq.SetBufferSize(bufferSize);
            return true;
        }

        virtual bool SetAllRXDATBufferSize(size_t bufferSize) noexcept override
        {
            for (auto& [index, port] : ports)
                port.rxdat.SetBufferSize(bufferSize);
            return true;
        }
    };


    // ----------------------------------------------------------------
    // V3CHIMonitorInterfaceT: passive monitor of all CHI channels on a
    // verilated V3 module. Both RN-side and SN-side monitor taps are
    // observed. No pins are driven.
    // ----------------------------------------------------------------

    template<class VT, template<class, size_t, class> class RNChannelPolicy,
                       template<class, size_t, class> class SNChannelPolicy>
    class V3CHIMonitorInterfaceT : public CHI::CHIMonitorInterface {
    public:
        static constexpr bool HasRNPort0() noexcept { return details::V3CHIModuleMonitorRNPort0<VT>; }
        static constexpr bool HasRNPort1() noexcept { return details::V3CHIModuleMonitorRNPort1<VT>; }
        static constexpr bool HasRNPort2() noexcept { return details::V3CHIModuleMonitorRNPort2<VT>; }
        static constexpr bool HasRNPort3() noexcept { return details::V3CHIModuleMonitorRNPort3<VT>; }
        static constexpr bool HasRNPort4() noexcept { return details::V3CHIModuleMonitorRNPort4<VT>; }
        static constexpr bool HasRNPort5() noexcept { return details::V3CHIModuleMonitorRNPort5<VT>; }
        static constexpr bool HasRNPort6() noexcept { return details::V3CHIModuleMonitorRNPort6<VT>; }
        static constexpr bool HasRNPort7() noexcept { return details::V3CHIModuleMonitorRNPort7<VT>; }

        static constexpr bool HasRNPort(size_t index) noexcept
        {
            switch (index)
            {
                case 0: return HasRNPort0();
                case 1: return HasRNPort1();
                case 2: return HasRNPort2();
                case 3: return HasRNPort3();
                case 4: return HasRNPort4();
                case 5: return HasRNPort5();
                case 6: return HasRNPort6();
                case 7: return HasRNPort7();
                default: return false;
            }
        }

        static constexpr bool HasSNPort0() noexcept { return details::V3CHIModuleMonitorSNPort0<VT>; }
        static constexpr bool HasSNPort1() noexcept { return details::V3CHIModuleMonitorSNPort1<VT>; }
        static constexpr bool HasSNPort2() noexcept { return details::V3CHIModuleMonitorSNPort2<VT>; }
        static constexpr bool HasSNPort3() noexcept { return details::V3CHIModuleMonitorSNPort3<VT>; }
        static constexpr bool HasSNPort4() noexcept { return details::V3CHIModuleMonitorSNPort4<VT>; }
        static constexpr bool HasSNPort5() noexcept { return details::V3CHIModuleMonitorSNPort5<VT>; }
        static constexpr bool HasSNPort6() noexcept { return details::V3CHIModuleMonitorSNPort6<VT>; }
        static constexpr bool HasSNPort7() noexcept { return details::V3CHIModuleMonitorSNPort7<VT>; }

        static constexpr bool HasSNPort(size_t index) noexcept
        {
            switch (index)
            {
                case 0: return HasSNPort0();
                case 1: return HasSNPort1();
                case 2: return HasSNPort2();
                case 3: return HasSNPort3();
                case 4: return HasSNPort4();
                case 5: return HasSNPort5();
                case 6: return HasSNPort6();
                case 7: return HasSNPort7();
                default: return false;
            }
        }

    protected:
        struct RNPort {
            details::CHICreditChannel<CHI::REQFlit> txreq;
            details::CHICreditChannel<CHI::RSPFlit> txrsp;
            details::CHICreditChannel<CHI::DATFlit> txdat;
            details::CHICreditChannel<CHI::RSPFlit> rxrsp;
            details::CHICreditChannel<CHI::DATFlit> rxdat;
            details::CHICreditChannel<CHI::SNPFlit> rxsnp;
        };

        struct SNPort {
            details::CHICreditChannel<CHI::REQFlit> rxreq;
            details::CHICreditChannel<CHI::RSPFlit> txrsp;
            details::CHICreditChannel<CHI::DATFlit> txdat;
            details::CHICreditChannel<CHI::DATFlit> rxdat;
        };

        VT*                 verilated           = nullptr;

        uint64_t            lastTickTime        = 0;
        bool                lastTickValid       = false;

        std::set<size_t>    portIndices;
        std::unordered_map<size_t, RNPort>
                            rnPorts;
        std::unordered_map<size_t, SNPort>
                            snPorts;

    public:
        V3CHIMonitorInterfaceT(VT* verilated) noexcept
            : verilated(verilated)
        {
#define _INIT_RN_PORT(port)                                                 \
            if constexpr (HasRNPort##port())                                \
            {                                                               \
                portIndices.insert(port);                                   \
                rnPorts.emplace(port, RNPort{});                            \
            }
            _INIT_RN_PORT(0)
            _INIT_RN_PORT(1)
            _INIT_RN_PORT(2)
            _INIT_RN_PORT(3)
            _INIT_RN_PORT(4)
            _INIT_RN_PORT(5)
            _INIT_RN_PORT(6)
            _INIT_RN_PORT(7)
#undef _INIT_RN_PORT

#define _INIT_SN_PORT(port)                                                 \
            if constexpr (HasSNPort##port())                                \
            {                                                               \
                portIndices.insert(port);                                   \
                snPorts.emplace(port, SNPort{});                            \
            }
            _INIT_SN_PORT(0)
            _INIT_SN_PORT(1)
            _INIT_SN_PORT(2)
            _INIT_SN_PORT(3)
            _INIT_SN_PORT(4)
            _INIT_SN_PORT(5)
            _INIT_SN_PORT(6)
            _INIT_SN_PORT(7)
#undef _INIT_SN_PORT
        }

        virtual ~V3CHIMonitorInterfaceT() noexcept = default;

    public:
        virtual void TickPostHandshake(uint64_t time) noexcept override
        {
            if (lastTickValid && lastTickTime == time)
                return;
            lastTickTime = time;
            lastTickValid = true;

#define _TICK_RN_PORT(port)                                                 \
            if constexpr (HasRNPort##port())                                \
            {                                                               \
                auto it = rnPorts.find(port);                               \
                if (it != rnPorts.end()) {                                  \
                    auto& p = it->second;                                   \
                    p.txreq.template MonitorObserve<VT, RNChannelPolicy, port, details::TXREQChannel>(*verilated, time); \
                    p.txrsp.template MonitorObserve<VT, RNChannelPolicy, port, details::TXRSPChannel>(*verilated, time); \
                    p.txdat.template MonitorObserve<VT, RNChannelPolicy, port, details::TXDATChannel>(*verilated, time); \
                    p.rxrsp.template MonitorObserve<VT, RNChannelPolicy, port, details::RXRSPChannel>(*verilated, time); \
                    p.rxdat.template MonitorObserve<VT, RNChannelPolicy, port, details::RXDATChannel>(*verilated, time); \
                    p.rxsnp.template MonitorObserve<VT, RNChannelPolicy, port, details::RXSNPChannel>(*verilated, time); \
                }                                                           \
            }
            _TICK_RN_PORT(0)
            _TICK_RN_PORT(1)
            _TICK_RN_PORT(2)
            _TICK_RN_PORT(3)
            _TICK_RN_PORT(4)
            _TICK_RN_PORT(5)
            _TICK_RN_PORT(6)
            _TICK_RN_PORT(7)
#undef _TICK_RN_PORT

#define _TICK_SN_PORT(port)                                                 \
            if constexpr (HasSNPort##port())                                \
            {                                                               \
                auto it = snPorts.find(port);                               \
                if (it != snPorts.end()) {                                  \
                    auto& p = it->second;                                   \
                    p.rxreq.template MonitorObserve<VT, SNChannelPolicy, port, details::RXREQChannel>(*verilated, time); \
                    p.txrsp.template MonitorObserve<VT, SNChannelPolicy, port, details::TXRSPChannel>(*verilated, time); \
                    p.txdat.template MonitorObserve<VT, SNChannelPolicy, port, details::TXDATChannel>(*verilated, time); \
                    p.rxdat.template MonitorObserve<VT, SNChannelPolicy, port, details::RXDATChannel>(*verilated, time); \
                }                                                           \
            }
            _TICK_SN_PORT(0)
            _TICK_SN_PORT(1)
            _TICK_SN_PORT(2)
            _TICK_SN_PORT(3)
            _TICK_SN_PORT(4)
            _TICK_SN_PORT(5)
            _TICK_SN_PORT(6)
            _TICK_SN_PORT(7)
#undef _TICK_SN_PORT
        }

    public:
        virtual size_t GetPortCount() const noexcept override { return portIndices.size(); }
        virtual std::set<size_t> GetPortIndices() const noexcept override { return portIndices; }
        virtual size_t GetPortMaxIndex() const noexcept override
        {
            if (portIndices.empty())
                return 0;
            return *portIndices.rbegin();
        }

    public:
        virtual bool HasTXREQ(size_t index) const noexcept override
        {
            auto it = rnPorts.find(index);
            return it != rnPorts.end() && it->second.txreq.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredREQ> PopTXREQ(size_t index) noexcept override
        {
            auto it = rnPorts.find(index);
            if (it == rnPorts.end())
                return std::nullopt;
            return it->second.txreq.PopFiredObservation();
        }

        virtual bool HasTXRSP(size_t index) const noexcept override
        {
            auto it = rnPorts.find(index);
            return it != rnPorts.end() && it->second.txrsp.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredRSP> PopTXRSP(size_t index) noexcept override
        {
            auto it = rnPorts.find(index);
            if (it == rnPorts.end())
                return std::nullopt;
            return it->second.txrsp.PopFiredObservation();
        }

        virtual bool HasTXDAT(size_t index) const noexcept override
        {
            auto it = rnPorts.find(index);
            return it != rnPorts.end() && it->second.txdat.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredDAT> PopTXDAT(size_t index) noexcept override
        {
            auto it = rnPorts.find(index);
            if (it == rnPorts.end())
                return std::nullopt;
            return it->second.txdat.PopFiredObservation();
        }

        virtual bool HasRXRSP(size_t index) const noexcept override
        {
            auto it = rnPorts.find(index);
            return it != rnPorts.end() && it->second.rxrsp.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredRSP> PopRXRSP(size_t index) noexcept override
        {
            auto it = rnPorts.find(index);
            if (it == rnPorts.end())
                return std::nullopt;
            return it->second.rxrsp.PopFiredObservation();
        }

        virtual bool HasRXDAT(size_t index) const noexcept override
        {
            auto it = rnPorts.find(index);
            return it != rnPorts.end() && it->second.rxdat.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredDAT> PopRXDAT(size_t index) noexcept override
        {
            auto it = rnPorts.find(index);
            if (it == rnPorts.end())
                return std::nullopt;
            return it->second.rxdat.PopFiredObservation();
        }

        virtual bool HasRXSNP(size_t index) const noexcept override
        {
            auto it = rnPorts.find(index);
            return it != rnPorts.end() && it->second.rxsnp.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredSNP> PopRXSNP(size_t index) noexcept override
        {
            auto it = rnPorts.find(index);
            if (it == rnPorts.end())
                return std::nullopt;
            return it->second.rxsnp.PopFiredObservation();
        }

        virtual bool HasSN_RXREQ(size_t index) const noexcept override
        {
            auto it = snPorts.find(index);
            return it != snPorts.end() && it->second.rxreq.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredREQ> PopSN_RXREQ(size_t index) noexcept override
        {
            auto it = snPorts.find(index);
            if (it == snPorts.end())
                return std::nullopt;
            return it->second.rxreq.PopFiredObservation();
        }

        virtual bool HasSN_TXRSP(size_t index) const noexcept override
        {
            auto it = snPorts.find(index);
            return it != snPorts.end() && it->second.txrsp.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredRSP> PopSN_TXRSP(size_t index) noexcept override
        {
            auto it = snPorts.find(index);
            if (it == snPorts.end())
                return std::nullopt;
            return it->second.txrsp.PopFiredObservation();
        }

        virtual bool HasSN_TXDAT(size_t index) const noexcept override
        {
            auto it = snPorts.find(index);
            return it != snPorts.end() && it->second.txdat.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredDAT> PopSN_TXDAT(size_t index) noexcept override
        {
            auto it = snPorts.find(index);
            if (it == snPorts.end())
                return std::nullopt;
            return it->second.txdat.PopFiredObservation();
        }

        virtual bool HasSN_RXDAT(size_t index) const noexcept override
        {
            auto it = snPorts.find(index);
            return it != snPorts.end() && it->second.rxdat.HasFiredObservation();
        }

        virtual std::optional<CHI::FiredDAT> PopSN_RXDAT(size_t index) noexcept override
        {
            auto it = snPorts.find(index);
            if (it == snPorts.end())
                return std::nullopt;
            return it->second.rxdat.PopFiredObservation();
        }
    };


    // ----------------------------------------------------------------
    // Type aliases for the two supported pin styles.
    // ----------------------------------------------------------------

    template<class VT>
    using V3CHIFromRNInterface = V3CHIFromRNInterfaceT<VT, details::V3CHISeparateFieldRNChannelPolicy>;

    template<class VT>
    using V3CHIFromRNInterfacePacked = V3CHIFromRNInterfaceT<VT, details::V3CHIPackedRNChannelPolicy>;

    template<class VT>
    using V3CHIToRNInterface = V3CHIToRNInterfaceT<VT, details::V3CHISeparateFieldRNChannelPolicy>;

    template<class VT>
    using V3CHIToRNInterfacePacked = V3CHIToRNInterfaceT<VT, details::V3CHIPackedRNChannelPolicy>;

    template<class VT>
    using V3CHIFromSNInterface = V3CHIFromSNInterfaceT<VT, details::V3CHISeparateFieldSNChannelPolicy>;

    template<class VT>
    using V3CHIFromSNInterfacePacked = V3CHIFromSNInterfaceT<VT, details::V3CHIPackedSNChannelPolicy>;

    template<class VT>
    using V3CHIToSNInterface = V3CHIToSNInterfaceT<VT, details::V3CHISeparateFieldSNChannelPolicy>;

    template<class VT>
    using V3CHIToSNInterfacePacked = V3CHIToSNInterfaceT<VT, details::V3CHIPackedSNChannelPolicy>;

    template<class VT>
    using V3CHIMonitorInterface = V3CHIMonitorInterfaceT<VT,
        details::V3CHISeparateFieldMonitorRNChannelPolicy,
        details::V3CHISeparateFieldMonitorSNChannelPolicy>;

    template<class VT>
    using V3CHIMonitorInterfacePacked = V3CHIMonitorInterfaceT<VT,
        details::V3CHIPackedMonitorRNChannelPolicy,
        details::V3CHIPackedMonitorSNChannelPolicy>;


    // ----------------------------------------------------------------
    // Monitor flit pin style detection. The port-presence concepts above
    // see only the control pins (flitpend/flitv/lcrdv), which are common to
    // both pin styles, so the style must be detected from the flit pins:
    // packed-style taps carry one whole-flit vector per channel
    // (mon_chi_<rn|sn><P>_<ch>_flit), separate-field taps carry one pin per
    // flit field (mon_chi_<rn|sn><P>_<ch>_flit_<Field>).
    // ----------------------------------------------------------------

    namespace details {

#define COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(PORT)                          \
        requires (T t) { t.mon_chi_rn ## PORT ## _txreq_flit; } ||          \
        requires (T t) { t.mon_chi_sn ## PORT ## _rxreq_flit; } ||

        template<class T>
        concept V3CHIModuleMonitorPackedStyle =
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(0)
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(1)
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(2)
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(3)
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(4)
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(5)
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(6)
            COHESTRA_V3_CHI_MONITOR_PACKED_PROBE(7)
            false;

#undef COHESTRA_V3_CHI_MONITOR_PACKED_PROBE
    }

    // V3CHIMonitorInterfaceAuto: selects the packed or separate-field
    // monitor channel policy from the module's pin style, so both styles
    // (and modules without monitor taps, where either policy is inert)
    // compile under one harness source.
    template<class VT>
    using V3CHIMonitorInterfaceAuto = std::conditional_t<
        details::V3CHIModuleMonitorPackedStyle<VT>,
        V3CHIMonitorInterfacePacked<VT>,
        V3CHIMonitorInterface<VT>>;

} // namespace Cohestra
