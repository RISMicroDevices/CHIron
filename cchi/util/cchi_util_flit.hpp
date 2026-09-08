#pragma once

#ifndef __CCHI__CCHI_UTIL_FLIT
#define __CCHI__CCHI_UTIL_FLIT

#include <bit>
#include <cassert>                          // IWYU pragma: keep
#include <cstddef>                          // IWYU pragma: keep
#include <cstdint>                          // IWYU pragma: keep

#include "../spec/cchi_protocol_flits.hpp"

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
#   include "../../clog/clog_cchi.hpp"
#endif


namespace CCHI {

    namespace Flits {

        //
        /*
        Use Flit De-Serializers to decode the flits from FLIT bundles to the C++ Flit objects.

        *NOTICE: Fields are packed in the declared order of the flit struct, LSB-first:
                 bit 0 of a flit bundle is the LSB of byte 0 of the bundle.
                 A flit bundle is a little-endian array of 32-bit words holding at
                 least ceil(<flit bit width> / 32) words.

        *NOTICE: CCHI flit widths are fully determined by the flit configuration at
                 compile-time, hence no run-time bit length parameter is taken.
        */
        template<EVTFlitConfigurationConcept        config>
        inline bool DeserializeEVT(EVT<config>& flit, const uint32_t* flitBits) noexcept;

        template<REQFlitConfigurationConcept        config>
        inline bool DeserializeREQ(REQ<config>& flit, const uint32_t* flitBits) noexcept;

        template<SNPFlitConfigurationConcept        config>
        inline bool DeserializeSNP(SNP<config>& flit, const uint32_t* flitBits) noexcept;

        template<DnRSPFlitConfigurationConcept      config>
        inline bool DeserializeDnRSP(DnRSP<config>& flit, const uint32_t* flitBits) noexcept;

        template<UpRSPFlitConfigurationConcept      config>
        inline bool DeserializeUpRSP(UpRSP<config>& flit, const uint32_t* flitBits) noexcept;

        template<DnDATFlitConfigurationConcept      config>
        inline bool DeserializeDnDAT(DnDAT<config>& flit, const uint32_t* flitBits) noexcept;

        template<UpDATFlitConfigurationConcept      config>
        inline bool DeserializeUpDAT(UpDAT<config>& flit, const uint32_t* flitBits) noexcept;
        //

        /*
        Use Flit Serializers to encode the flits from C++ Flit objects to FLIT bundles.
        */
        template<EVTFlitConfigurationConcept        config>
        inline bool SerializeEVT(uint32_t* flitBits, const EVT<config>& flit) noexcept;

        template<REQFlitConfigurationConcept        config>
        inline bool SerializeREQ(uint32_t* flitBits, const REQ<config>& flit) noexcept;

        template<SNPFlitConfigurationConcept        config>
        inline bool SerializeSNP(uint32_t* flitBits, const SNP<config>& flit) noexcept;

        template<DnRSPFlitConfigurationConcept      config>
        inline bool SerializeDnRSP(uint32_t* flitBits, const DnRSP<config>& flit) noexcept;

        template<UpRSPFlitConfigurationConcept      config>
        inline bool SerializeUpRSP(uint32_t* flitBits, const UpRSP<config>& flit) noexcept;

        template<DnDATFlitConfigurationConcept      config>
        inline bool SerializeDnDAT(uint32_t* flitBits, const DnDAT<config>& flit) noexcept;

        template<UpDATFlitConfigurationConcept      config>
        inline bool SerializeUpDAT(uint32_t* flitBits, const UpDAT<config>& flit) noexcept;
        //

        //
        /*
        Use Flit Format Evaluators to evaluate flit format at run-time.
        *NOTICE: This utility only provides the computed format information of CCHI Flit.
                 The memory structure IS NOT and WILL NOT BE provided in near future,
                 since this was a niche demand.

        *NOTICE: The field-order/width formula of each channel is centralized in the
                 corresponding Measure::Eval() implementation. It must be kept in sync
                 with the flit struct field widths in cchi/spec/cchi_protocol_flits.hpp
                 and with the serializers/de-serializers in this file.
                 Pending spec changes (doc/cchi-spec-consistency-report.md):
                 - A2: a conditionally-sized AllowRetry field is to be added to EVT and REQ.
                 - A3: Way on EVT/DnRSP/DnDAT is currently gated on <UWPredict_Enable>
                       following the model; it may be re-gated on <UWPersist_Enable>.
        */

        // EVT Flit measure
        class EVTMeasure {
        public:
            /*
            Parameters for CCHI EVT Flit
            */
            class Parameters {
            public:
                size_t  opcodeWidth;            // run-time opcode width, derived from the component type
                size_t  txnIdWidth;
                size_t  upstreamNodeIdWidth;
                size_t  downstreamNodeIdWidth;
                size_t  wayIndexWidth;
                bool    uwPersistEnable;
                bool    uwPredictEnable;

            public:
                constexpr Parameters() noexcept
                    : opcodeWidth           (1)
                    , txnIdWidth            (7)
                    , upstreamNodeIdWidth   (5)
                    , downstreamNodeIdWidth (5)
                    , wayIndexWidth         (4)
                    , uwPersistEnable       (true)
                    , uwPredictEnable       (true)
                { }

            public:
                constexpr bool  Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  TxnID;
                size_t  SrcID;
                size_t  TgtID;
                size_t  Opcode;
                size_t  Addr;
                size_t  NS;
                size_t  MemAttr;
                size_t  WayValid;
                size_t  Way;
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CCHI EVT Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            constexpr bool          Eval(const Parameters& params) noexcept;

        public:
            constexpr size_t        BitLength() const noexcept
            { return width._; }

            constexpr size_t        ByteLength() const noexcept
            { return (width._ + 7) / 8; }

        public:
            static constexpr size_t EvalByteLength(const Parameters& params) noexcept;

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            static inline size_t    EvalByteLength(const CLog::CCHI::Parameters& params) noexcept;
#endif
        };

        // REQ Flit measure
        class REQMeasure {
        public:
            /*
            Parameters for CCHI REQ Flit
            */
            class Parameters {
            public:
                size_t  opcodeWidth;            // run-time opcode width, derived from the component type
                size_t  txnIdWidth;
                size_t  upstreamNodeIdWidth;
                size_t  downstreamNodeIdWidth;
                size_t  wayIndexWidth;
                size_t  tagAliasWidth;
                bool    uwPersistEnable;
                bool    uwPredictEnable;

            public:
                constexpr Parameters() noexcept
                    : opcodeWidth           (6)
                    , txnIdWidth            (7)
                    , upstreamNodeIdWidth   (5)
                    , downstreamNodeIdWidth (5)
                    , wayIndexWidth         (4)
                    , tagAliasWidth         (8)
                    , uwPersistEnable       (true)
                    , uwPredictEnable       (true)
                { }

            public:
                constexpr bool  Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  TxnID;
                size_t  SrcID;
                size_t  TgtID;
                size_t  Opcode;
                size_t  Size;
                size_t  Addr;
                size_t  TagAlias;
                size_t  NS;
                size_t  Order;
                size_t  MemAttr;
                size_t  Excl;
                size_t  ExpCompData;
                size_t  ExpCompStash;
                size_t  WayValid;
                size_t  Way;
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CCHI REQ Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            constexpr bool          Eval(const Parameters& params) noexcept;

        public:
            constexpr size_t        BitLength() const noexcept
            { return width._; }

            constexpr size_t        ByteLength() const noexcept
            { return (width._ + 7) / 8; }

        public:
            static constexpr size_t EvalByteLength(const Parameters& params) noexcept;

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            static inline size_t    EvalByteLength(const CLog::CCHI::Parameters& params) noexcept;
#endif
        };

        // SNP Flit measure
        class SNPMeasure {
        public:
            /*
            Parameters for CCHI SNP Flit
            */
            class Parameters {
            public:
                size_t  opcodeWidth;            // run-time opcode width, derived from the component type
                size_t  dbIdWidth;
                size_t  upstreamNodeIdWidth;
                size_t  downstreamNodeIdWidth;

            public:
                constexpr Parameters() noexcept
                    : opcodeWidth           (2)
                    , dbIdWidth             (8)
                    , upstreamNodeIdWidth   (5)
                    , downstreamNodeIdWidth (5)
                { }

            public:
                constexpr bool  Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  TxnID;
                size_t  SrcID;
                size_t  TgtID;
                size_t  Opcode;
                size_t  Addr;
                size_t  NS;
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CCHI SNP Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            constexpr bool          Eval(const Parameters& params) noexcept;

        public:
            constexpr size_t        BitLength() const noexcept
            { return width._; }

            constexpr size_t        ByteLength() const noexcept
            { return (width._ + 7) / 8; }

        public:
            static constexpr size_t EvalByteLength(const Parameters& params) noexcept;

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            static inline size_t    EvalByteLength(const CLog::CCHI::Parameters& params) noexcept;
#endif
        };

        // DnRSP Flit measure
        class DnRSPMeasure {
        public:
            /*
            Parameters for CCHI DnRSP Flit
            */
            class Parameters {
            public:
                size_t  opcodeWidth;            // run-time opcode width, derived from the component type
                size_t  txnIdWidth;
                size_t  dbIdWidth;
                size_t  upstreamNodeIdWidth;
                size_t  downstreamNodeIdWidth;
                size_t  wayIndexWidth;
                bool    uwPersistEnable;
                bool    uwPredictEnable;

            public:
                constexpr Parameters() noexcept
                    : opcodeWidth           (3)
                    , txnIdWidth            (7)
                    , dbIdWidth             (8)
                    , upstreamNodeIdWidth   (5)
                    , downstreamNodeIdWidth (5)
                    , wayIndexWidth         (4)
                    , uwPersistEnable       (true)
                    , uwPredictEnable       (true)
                { }

            public:
                constexpr bool  Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  TxnID;
                size_t  SrcID;
                size_t  TgtID;
                size_t  DBID;
                size_t  Opcode;
                size_t  RespErr;
                size_t  Resp;
                size_t  CBusy;
                size_t  WayValid;
                size_t  Way;
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CCHI DnRSP Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            constexpr bool          Eval(const Parameters& params) noexcept;

        public:
            constexpr size_t        BitLength() const noexcept
            { return width._; }

            constexpr size_t        ByteLength() const noexcept
            { return (width._ + 7) / 8; }

        public:
            static constexpr size_t EvalByteLength(const Parameters& params) noexcept;

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            static inline size_t    EvalByteLength(const CLog::CCHI::Parameters& params) noexcept;
#endif
        };

        // UpRSP Flit measure
        class UpRSPMeasure {
        public:
            /*
            Parameters for CCHI UpRSP Flit
            */
            class Parameters {
            public:
                size_t  opcodeWidth;            // run-time opcode width, derived from the component type
                size_t  dbIdWidth;
                size_t  upstreamNodeIdWidth;
                size_t  downstreamNodeIdWidth;

            public:
                constexpr Parameters() noexcept
                    : opcodeWidth           (1)
                    , dbIdWidth             (8)
                    , upstreamNodeIdWidth   (5)
                    , downstreamNodeIdWidth (5)
                { }

            public:
                constexpr bool  Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  TxnID;
                size_t  SrcID;
                size_t  TgtID;
                size_t  Opcode;
                size_t  RespErr;
                size_t  Resp;
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CCHI UpRSP Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            constexpr bool          Eval(const Parameters& params) noexcept;

        public:
            constexpr size_t        BitLength() const noexcept
            { return width._; }

            constexpr size_t        ByteLength() const noexcept
            { return (width._ + 7) / 8; }

        public:
            static constexpr size_t EvalByteLength(const Parameters& params) noexcept;

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            static inline size_t    EvalByteLength(const CLog::CCHI::Parameters& params) noexcept;
#endif
        };

        // DnDAT Flit measure
        class DnDATMeasure {
        public:
            /*
            Parameters for CCHI DnDAT Flit
            */
            class Parameters {
            public:
                size_t  opcodeWidth;            // run-time opcode width, derived from the component type
                size_t  txnIdWidth;
                size_t  dbIdWidth;
                size_t  upstreamNodeIdWidth;
                size_t  downstreamNodeIdWidth;
                size_t  wayIndexWidth;
                size_t  dataWidth;
                bool    uwPersistEnable;
                bool    uwPredictEnable;

            public:
                constexpr Parameters() noexcept
                    : opcodeWidth           (1)
                    , txnIdWidth            (7)
                    , dbIdWidth             (8)
                    , upstreamNodeIdWidth   (5)
                    , downstreamNodeIdWidth (5)
                    , wayIndexWidth         (4)
                    , dataWidth             (256)
                    , uwPersistEnable       (true)
                    , uwPredictEnable       (true)
                { }

            public:
                constexpr bool  Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  TxnID;
                size_t  SrcID;
                size_t  TgtID;
                size_t  DBID;
                size_t  Opcode;
                size_t  RespErr;
                size_t  Resp;
                size_t  DataSource;
                size_t  CBusy;
                size_t  WayValid;
                size_t  Way;
                size_t  DataID;
                size_t  Data;
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CCHI DnDAT Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            constexpr bool          Eval(const Parameters& params) noexcept;

        public:
            constexpr size_t        BitLength() const noexcept
            { return width._; }

            constexpr size_t        ByteLength() const noexcept
            { return (width._ + 7) / 8; }

        public:
            static constexpr size_t EvalByteLength(const Parameters& params) noexcept;

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            static inline size_t    EvalByteLength(const CLog::CCHI::Parameters& params) noexcept;
#endif
        };

        // UpDAT Flit measure
        class UpDATMeasure {
        public:
            /*
            Parameters for CCHI UpDAT Flit
            */
            class Parameters {
            public:
                size_t  opcodeWidth;            // run-time opcode width, derived from the component type
                size_t  dbIdWidth;
                size_t  upstreamNodeIdWidth;
                size_t  downstreamNodeIdWidth;
                size_t  dataWidth;

            public:
                constexpr Parameters() noexcept
                    : opcodeWidth           (2)
                    , dbIdWidth             (8)
                    , upstreamNodeIdWidth   (5)
                    , downstreamNodeIdWidth (5)
                    , dataWidth             (256)
                { }

            public:
                constexpr bool  Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  TxnID;
                size_t  SrcID;
                size_t  TgtID;
                size_t  Opcode;
                size_t  RespErr;
                size_t  Resp;
                size_t  DataID;
                size_t  Data;
                size_t  BE;
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CCHI UpDAT Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            constexpr bool          Eval(const Parameters& params) noexcept;

        public:
            constexpr size_t        BitLength() const noexcept
            { return width._; }

            constexpr size_t        ByteLength() const noexcept
            { return (width._ + 7) / 8; }

        public:
            static constexpr size_t EvalByteLength(const Parameters& params) noexcept;

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            static inline size_t    EvalByteLength(const CLog::CCHI::Parameters& params) noexcept;
#endif
        };
    }
}


namespace CCHI {

    namespace Flits {

        namespace details {

            /*
            Flit walker.
            */
            class FlitWalker {
            public:
                size_t index;
                size_t offset;

            public:
                const uint32_t* flitBits;

            public:
                inline FlitWalker(const uint32_t* flitBits) noexcept
                    : index     (0)
                    , offset    (0)
                    , flitBits  (flitBits)
                { }

                inline uint32_t Walk32(size_t width = 32) noexcept
                {
                    uint32_t field
                        = (flitBits[index] >> offset) & (0xFFFFFFFFU >> (32 - width));

                    int32_t next_width = int32_t(offset + width) - 32;
                    if (next_width > 0)
                    {
                        field |= (flitBits[++index] & (0xFFFFFFFFU >> (32 - next_width))) << (width - next_width);
                        offset = next_width;
                    }
                    else if ((offset += width) == 32)
                    {
                        offset = 0;
                        index++;
                    }

                    return field;
                }

                inline uint64_t Walk64(size_t width) noexcept
                {
                    if (width <= 32)
                        return Walk32(width);

                    return  uint64_t(Walk32(32))
                        |  (uint64_t(Walk32(width - 32)) << 32);
                }

                inline void Finish(size_t width) const noexcept
                {
                    assert(index * 32 + offset == width);
                }
            };

            /*
            Flit appender.
            */
            class FlitAppender {
            public:
                size_t index;
                size_t offset;

            public:
                uint32_t*   flitBits;

            public:
                inline FlitAppender(uint32_t* flitBits) noexcept
                    : index     (0)
                    , offset    (0)
                    , flitBits  (flitBits)
                {
                    flitBits[0] = 0;
                }

                inline void Append32(uint32_t value, size_t width) noexcept
                {
                    flitBits[index] |= (value & (0xFFFFFFFFU >> (32 - width))) << offset;

                    size_t next = offset + width;

                    if (next >= 32)
                    {
                        offset = next - 32;
                        flitBits[++index] = offset != 0 ?
                            (value >> (width - offset)) & (0xFFFFFFFFU >> (32 - offset)) : 0;
                    }
                    else
                    {
                        offset = next;
                    }
                }

                inline void Append64(uint64_t value, size_t width) noexcept
                {
                    if (width <= 32)
                        Append32(uint32_t(value), width);
                    else
                    {
                        Append32(uint32_t(value)        , 32);
                        Append32(uint32_t(value >> 32)  , width - 32);
                    }
                }

                inline void Finish(size_t width) const noexcept
                {
                    assert(index * 32 + offset == width);
                }
            };

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
            /*
            Maps a CLog CCHI node type to its CCHI component type descriptor,
            which carries the per-channel opcode widths.
            */
            inline constexpr ComponentTypeEnum ComponentTypeOf(CLog::CCHI::NodeType nodeType) noexcept
            {
                switch (nodeType)
                {
                    case CLog::CCHI::NodeType::Type1:   return ComponentType::TYPE_1;
                    case CLog::CCHI::NodeType::Type2:   return ComponentType::TYPE_2;
                    case CLog::CCHI::NodeType::Type3:   return ComponentType::TYPE_3;
                    case CLog::CCHI::NodeType::Type4:   return ComponentType::TYPE_4;
                    case CLog::CCHI::NodeType::Type5:   return ComponentType::TYPE_5;
                    case CLog::CCHI::NodeType::Home:    return ComponentType::HOME;
                    default:                            return ComponentType::Unknown;
                }
            }
#endif
        }

        // Flit de-serializer implementations.
        /*
        EVT flit de-serializer.
        */
        template<EVTFlitConfigurationConcept    config>
        inline bool DeserializeEVT(EVT<config>& flit, const uint32_t* flitBits) noexcept
        {
            using EVT_t = EVT<config>;

            details::FlitWalker walker(flitBits);

            flit.TxnID      = walker.Walk32(EVT_t::TXNID_WIDTH);
            flit.SrcID      = walker.Walk32(EVT_t::SRCID_WIDTH);
            flit.TgtID      = walker.Walk32(EVT_t::TGTID_WIDTH);

            if constexpr (EVT_t::OPCODE_WIDTH > 0)
                flit.Opcode = walker.Walk32(EVT_t::OPCODE_WIDTH);

            flit.Addr       = walker.Walk64(EVT_t::ADDR_WIDTH);
            flit.NS         = walker.Walk32(EVT_t::NS_WIDTH);
            flit.MemAttr    = walker.Walk32(EVT_t::MEMATTR_WIDTH);

            if constexpr (EVT_t::WAYVALID_WIDTH > 0)
                flit.WayValid = walker.Walk32(EVT_t::WAYVALID_WIDTH);

            if constexpr (EVT_t::WAY_WIDTH > 0)
                flit.Way    = walker.Walk32(EVT_t::WAY_WIDTH);

            flit.TraceTag   = walker.Walk32(EVT_t::TRACETAG_WIDTH);

            //
            walker.Finish(EVT_t::WIDTH);

            return true;
        }

        /*
        REQ flit de-serializer.
        *NOTICE: ExpCompData/ExpCompStash share one overlapped bit. Both union
                alternatives are written with the same walked value, so either
                accessor observes the bit.
        */
        template<REQFlitConfigurationConcept    config>
        inline bool DeserializeREQ(REQ<config>& flit, const uint32_t* flitBits) noexcept
        {
            using REQ_t = REQ<config>;

            details::FlitWalker walker(flitBits);

            flit.TxnID          = walker.Walk32(REQ_t::TXNID_WIDTH);
            flit.SrcID          = walker.Walk32(REQ_t::SRCID_WIDTH);
            flit.TgtID          = walker.Walk32(REQ_t::TGTID_WIDTH);

            if constexpr (REQ_t::OPCODE_WIDTH > 0)
                flit.Opcode     = walker.Walk32(REQ_t::OPCODE_WIDTH);

            flit.Size           = walker.Walk32(REQ_t::SSIZE_WIDTH);
            flit.Addr           = walker.Walk64(REQ_t::ADDR_WIDTH);

            if constexpr (REQ_t::TAGALIAS_WIDTH > 0)
                flit.TagAlias   = walker.Walk32(REQ_t::TAGALIAS_WIDTH);

            flit.NS             = walker.Walk32(REQ_t::NS_WIDTH);
            flit.Order          = walker.Walk32(REQ_t::ORDER_WIDTH);
            flit.MemAttr        = walker.Walk32(REQ_t::MEMATTR_WIDTH);
            flit.Excl           = walker.Walk32(REQ_t::EXCL_WIDTH);

            flit.ExpCompData    = walker.Walk32(REQ_t::EXPCOMPDATA_WIDTH);
            flit.ExpCompStash   = flit.ExpCompData;

            if constexpr (REQ_t::WAYVALID_WIDTH > 0)
                flit.WayValid   = walker.Walk32(REQ_t::WAYVALID_WIDTH);

            if constexpr (REQ_t::WAY_WIDTH > 0)
                flit.Way        = walker.Walk32(REQ_t::WAY_WIDTH);

            flit.TraceTag       = walker.Walk32(REQ_t::TRACETAG_WIDTH);

            //
            walker.Finish(REQ_t::WIDTH);

            return true;
        }

        /*
        SNP flit de-serializer.
        */
        template<SNPFlitConfigurationConcept    config>
        inline bool DeserializeSNP(SNP<config>& flit, const uint32_t* flitBits) noexcept
        {
            using SNP_t = SNP<config>;

            details::FlitWalker walker(flitBits);

            flit.TxnID      = walker.Walk32(SNP_t::TXNID_WIDTH);
            flit.SrcID      = walker.Walk32(SNP_t::SRCID_WIDTH);
            flit.TgtID      = walker.Walk32(SNP_t::TGTID_WIDTH);

            if constexpr (SNP_t::OPCODE_WIDTH > 0)
                flit.Opcode = walker.Walk32(SNP_t::OPCODE_WIDTH);

            flit.Addr       = walker.Walk64(SNP_t::ADDR_WIDTH);
            flit.NS         = walker.Walk32(SNP_t::NS_WIDTH);
            flit.TraceTag   = walker.Walk32(SNP_t::TRACETAG_WIDTH);

            //
            walker.Finish(SNP_t::WIDTH);

            return true;
        }

        /*
        DnRSP flit de-serializer.
        */
        template<DnRSPFlitConfigurationConcept  config>
        inline bool DeserializeDnRSP(DnRSP<config>& flit, const uint32_t* flitBits) noexcept
        {
            using DnRSP_t = DnRSP<config>;

            details::FlitWalker walker(flitBits);

            flit.TxnID      = walker.Walk32(DnRSP_t::TXNID_WIDTH);
            flit.SrcID      = walker.Walk32(DnRSP_t::SRCID_WIDTH);
            flit.TgtID      = walker.Walk32(DnRSP_t::TGTID_WIDTH);
            flit.DBID       = walker.Walk32(DnRSP_t::DBID_WIDTH);

            if constexpr (DnRSP_t::OPCODE_WIDTH > 0)
                flit.Opcode = walker.Walk32(DnRSP_t::OPCODE_WIDTH);

            flit.RespErr    = walker.Walk32(DnRSP_t::RESPERR_WIDTH);
            flit.Resp       = walker.Walk32(DnRSP_t::RESP_WIDTH);
            flit.CBusy      = walker.Walk32(DnRSP_t::CBUSY_WIDTH);

            if constexpr (DnRSP_t::WAYVALID_WIDTH > 0)
                flit.WayValid = walker.Walk32(DnRSP_t::WAYVALID_WIDTH);

            if constexpr (DnRSP_t::WAY_WIDTH > 0)
                flit.Way    = walker.Walk32(DnRSP_t::WAY_WIDTH);

            flit.TraceTag   = walker.Walk32(DnRSP_t::TRACETAG_WIDTH);

            //
            walker.Finish(DnRSP_t::WIDTH);

            return true;
        }

        /*
        UpRSP flit de-serializer.
        *NOTICE: UpRSP<config> does not provide a WIDTH constant (unlike the other
                channels), so the total bit width is computed from the field widths.
        */
        template<UpRSPFlitConfigurationConcept  config>
        inline bool DeserializeUpRSP(UpRSP<config>& flit, const uint32_t* flitBits) noexcept
        {
            using UpRSP_t = UpRSP<config>;

            static constexpr size_t WIDTH =
                  UpRSP_t::TXNID_WIDTH    + UpRSP_t::SRCID_WIDTH    + UpRSP_t::TGTID_WIDTH
                + UpRSP_t::OPCODE_WIDTH   + UpRSP_t::RESPERR_WIDTH  + UpRSP_t::RESP_WIDTH
                + UpRSP_t::TRACETAG_WIDTH;

            details::FlitWalker walker(flitBits);

            flit.TxnID      = walker.Walk32(UpRSP_t::TXNID_WIDTH);
            flit.SrcID      = walker.Walk32(UpRSP_t::SRCID_WIDTH);
            flit.TgtID      = walker.Walk32(UpRSP_t::TGTID_WIDTH);

            if constexpr (UpRSP_t::OPCODE_WIDTH > 0)
                flit.Opcode = walker.Walk32(UpRSP_t::OPCODE_WIDTH);

            flit.RespErr    = walker.Walk32(UpRSP_t::RESPERR_WIDTH);
            flit.Resp       = walker.Walk32(UpRSP_t::RESP_WIDTH);
            flit.TraceTag   = walker.Walk32(UpRSP_t::TRACETAG_WIDTH);

            //
            walker.Finish(WIDTH);

            return true;
        }

        /*
        DnDAT flit de-serializer.
        */
        template<DnDATFlitConfigurationConcept  config>
        inline bool DeserializeDnDAT(DnDAT<config>& flit, const uint32_t* flitBits) noexcept
        {
            using DnDAT_t = DnDAT<config>;

            details::FlitWalker walker(flitBits);

            flit.TxnID      = walker.Walk32(DnDAT_t::TXNID_WIDTH);
            flit.SrcID      = walker.Walk32(DnDAT_t::SRCID_WIDTH);
            flit.TgtID      = walker.Walk32(DnDAT_t::TGTID_WIDTH);
            flit.DBID       = walker.Walk32(DnDAT_t::DBID_WIDTH);

            if constexpr (DnDAT_t::OPCODE_WIDTH > 0)
                flit.Opcode = walker.Walk32(DnDAT_t::OPCODE_WIDTH);

            flit.RespErr    = walker.Walk32(DnDAT_t::RESPERR_WIDTH);
            flit.Resp       = walker.Walk32(DnDAT_t::RESP_WIDTH);
            flit.DataSource = walker.Walk32(DnDAT_t::DATASOURCE_WIDTH);
            flit.CBusy      = walker.Walk32(DnDAT_t::CBUSY_WIDTH);

            if constexpr (DnDAT_t::WAYVALID_WIDTH > 0)
                flit.WayValid = walker.Walk32(DnDAT_t::WAYVALID_WIDTH);

            if constexpr (DnDAT_t::WAY_WIDTH > 0)
                flit.Way    = walker.Walk32(DnDAT_t::WAY_WIDTH);

            if constexpr (DnDAT_t::DATAID_WIDTH > 0)
                flit.DataID = walker.Walk32(DnDAT_t::DATAID_WIDTH);

            for (size_t i = 0; i < DnDAT_t::DATA_WIDTH / 64; ++i)
                flit.Data[i] = walker.Walk64(64);

            flit.TraceTag   = walker.Walk32(DnDAT_t::TRACETAG_WIDTH);

            //
            walker.Finish(DnDAT_t::WIDTH);

            return true;
        }

        /*
        UpDAT flit de-serializer.
        *NOTICE: UpDAT<config> does not provide a WIDTH constant (unlike the other
                channels), so the total bit width is computed from the field widths.
        */
        template<UpDATFlitConfigurationConcept  config>
        inline bool DeserializeUpDAT(UpDAT<config>& flit, const uint32_t* flitBits) noexcept
        {
            using UpDAT_t = UpDAT<config>;

            static constexpr size_t WIDTH =
                  UpDAT_t::TXNID_WIDTH    + UpDAT_t::SRCID_WIDTH    + UpDAT_t::TGTID_WIDTH
                + UpDAT_t::OPCODE_WIDTH   + UpDAT_t::RESPERR_WIDTH  + UpDAT_t::RESP_WIDTH
                + UpDAT_t::DATAID_WIDTH   + UpDAT_t::DATA_WIDTH     + UpDAT_t::BE_WIDTH
                + UpDAT_t::TRACETAG_WIDTH;

            details::FlitWalker walker(flitBits);

            flit.TxnID      = walker.Walk32(UpDAT_t::TXNID_WIDTH);
            flit.SrcID      = walker.Walk32(UpDAT_t::SRCID_WIDTH);
            flit.TgtID      = walker.Walk32(UpDAT_t::TGTID_WIDTH);

            if constexpr (UpDAT_t::OPCODE_WIDTH > 0)
                flit.Opcode = walker.Walk32(UpDAT_t::OPCODE_WIDTH);

            flit.RespErr    = walker.Walk32(UpDAT_t::RESPERR_WIDTH);
            flit.Resp       = walker.Walk32(UpDAT_t::RESP_WIDTH);

            if constexpr (UpDAT_t::DATAID_WIDTH > 0)
                flit.DataID = walker.Walk32(UpDAT_t::DATAID_WIDTH);

            for (size_t i = 0; i < UpDAT_t::DATA_WIDTH / 64; ++i)
                flit.Data[i] = walker.Walk64(64);

            flit.BE         = walker.Walk32(UpDAT_t::BE_WIDTH);
            flit.TraceTag   = walker.Walk32(UpDAT_t::TRACETAG_WIDTH);

            //
            walker.Finish(WIDTH);

            return true;
        }


        // Flit serializer implementations.
        /*
        EVT flit serializer.
        */
        template<EVTFlitConfigurationConcept    config>
        inline bool SerializeEVT(uint32_t* flitBits, const EVT<config>& flit) noexcept
        {
            using EVT_t = EVT<config>;

            details::FlitAppender appender(flitBits);

            appender.Append32(flit.TxnID        , EVT_t::TXNID_WIDTH);
            appender.Append32(flit.SrcID        , EVT_t::SRCID_WIDTH);
            appender.Append32(flit.TgtID        , EVT_t::TGTID_WIDTH);

            if constexpr (EVT_t::OPCODE_WIDTH > 0)
                appender.Append32(flit.Opcode   , EVT_t::OPCODE_WIDTH);

            appender.Append64(flit.Addr         , EVT_t::ADDR_WIDTH);
            appender.Append32(flit.NS           , EVT_t::NS_WIDTH);
            appender.Append32(flit.MemAttr      , EVT_t::MEMATTR_WIDTH);

            if constexpr (EVT_t::WAYVALID_WIDTH > 0)
                appender.Append32(flit.WayValid , EVT_t::WAYVALID_WIDTH);

            if constexpr (EVT_t::WAY_WIDTH > 0)
                appender.Append32(flit.Way      , EVT_t::WAY_WIDTH);

            appender.Append32(flit.TraceTag     , EVT_t::TRACETAG_WIDTH);

            //
            appender.Finish(EVT_t::WIDTH);

            return true;
        }

        /*
        REQ flit serializer.
        *NOTICE: ExpCompData/ExpCompStash share one overlapped bit. The ExpCompData
                alternative is read; both alternatives occupy the same storage.
        */
        template<REQFlitConfigurationConcept    config>
        inline bool SerializeREQ(uint32_t* flitBits, const REQ<config>& flit) noexcept
        {
            using REQ_t = REQ<config>;

            details::FlitAppender appender(flitBits);

            appender.Append32(flit.TxnID        , REQ_t::TXNID_WIDTH);
            appender.Append32(flit.SrcID        , REQ_t::SRCID_WIDTH);
            appender.Append32(flit.TgtID        , REQ_t::TGTID_WIDTH);

            if constexpr (REQ_t::OPCODE_WIDTH > 0)
                appender.Append32(flit.Opcode   , REQ_t::OPCODE_WIDTH);

            appender.Append32(flit.Size         , REQ_t::SSIZE_WIDTH);
            appender.Append64(flit.Addr         , REQ_t::ADDR_WIDTH);

            if constexpr (REQ_t::TAGALIAS_WIDTH > 0)
                appender.Append32(flit.TagAlias , REQ_t::TAGALIAS_WIDTH);

            appender.Append32(flit.NS           , REQ_t::NS_WIDTH);
            appender.Append32(flit.Order        , REQ_t::ORDER_WIDTH);
            appender.Append32(flit.MemAttr      , REQ_t::MEMATTR_WIDTH);
            appender.Append32(flit.Excl         , REQ_t::EXCL_WIDTH);
            appender.Append32(flit.ExpCompData  , REQ_t::EXPCOMPDATA_WIDTH);

            if constexpr (REQ_t::WAYVALID_WIDTH > 0)
                appender.Append32(flit.WayValid , REQ_t::WAYVALID_WIDTH);

            if constexpr (REQ_t::WAY_WIDTH > 0)
                appender.Append32(flit.Way      , REQ_t::WAY_WIDTH);

            appender.Append32(flit.TraceTag     , REQ_t::TRACETAG_WIDTH);

            //
            appender.Finish(REQ_t::WIDTH);

            return true;
        }

        /*
        SNP flit serializer.
        */
        template<SNPFlitConfigurationConcept    config>
        inline bool SerializeSNP(uint32_t* flitBits, const SNP<config>& flit) noexcept
        {
            using SNP_t = SNP<config>;

            details::FlitAppender appender(flitBits);

            appender.Append32(flit.TxnID        , SNP_t::TXNID_WIDTH);
            appender.Append32(flit.SrcID        , SNP_t::SRCID_WIDTH);
            appender.Append32(flit.TgtID        , SNP_t::TGTID_WIDTH);

            if constexpr (SNP_t::OPCODE_WIDTH > 0)
                appender.Append32(flit.Opcode   , SNP_t::OPCODE_WIDTH);

            appender.Append64(flit.Addr         , SNP_t::ADDR_WIDTH);
            appender.Append32(flit.NS           , SNP_t::NS_WIDTH);
            appender.Append32(flit.TraceTag     , SNP_t::TRACETAG_WIDTH);

            //
            appender.Finish(SNP_t::WIDTH);

            return true;
        }

        /*
        DnRSP flit serializer.
        */
        template<DnRSPFlitConfigurationConcept  config>
        inline bool SerializeDnRSP(uint32_t* flitBits, const DnRSP<config>& flit) noexcept
        {
            using DnRSP_t = DnRSP<config>;

            details::FlitAppender appender(flitBits);

            appender.Append32(flit.TxnID        , DnRSP_t::TXNID_WIDTH);
            appender.Append32(flit.SrcID        , DnRSP_t::SRCID_WIDTH);
            appender.Append32(flit.TgtID        , DnRSP_t::TGTID_WIDTH);
            appender.Append32(flit.DBID         , DnRSP_t::DBID_WIDTH);

            if constexpr (DnRSP_t::OPCODE_WIDTH > 0)
                appender.Append32(flit.Opcode   , DnRSP_t::OPCODE_WIDTH);

            appender.Append32(flit.RespErr      , DnRSP_t::RESPERR_WIDTH);
            appender.Append32(flit.Resp         , DnRSP_t::RESP_WIDTH);
            appender.Append32(flit.CBusy        , DnRSP_t::CBUSY_WIDTH);

            if constexpr (DnRSP_t::WAYVALID_WIDTH > 0)
                appender.Append32(flit.WayValid , DnRSP_t::WAYVALID_WIDTH);

            if constexpr (DnRSP_t::WAY_WIDTH > 0)
                appender.Append32(flit.Way      , DnRSP_t::WAY_WIDTH);

            appender.Append32(flit.TraceTag     , DnRSP_t::TRACETAG_WIDTH);

            //
            appender.Finish(DnRSP_t::WIDTH);

            return true;
        }

        /*
        UpRSP flit serializer.
        */
        template<UpRSPFlitConfigurationConcept  config>
        inline bool SerializeUpRSP(uint32_t* flitBits, const UpRSP<config>& flit) noexcept
        {
            using UpRSP_t = UpRSP<config>;

            static constexpr size_t WIDTH =
                  UpRSP_t::TXNID_WIDTH    + UpRSP_t::SRCID_WIDTH    + UpRSP_t::TGTID_WIDTH
                + UpRSP_t::OPCODE_WIDTH   + UpRSP_t::RESPERR_WIDTH  + UpRSP_t::RESP_WIDTH
                + UpRSP_t::TRACETAG_WIDTH;

            details::FlitAppender appender(flitBits);

            appender.Append32(flit.TxnID        , UpRSP_t::TXNID_WIDTH);
            appender.Append32(flit.SrcID        , UpRSP_t::SRCID_WIDTH);
            appender.Append32(flit.TgtID        , UpRSP_t::TGTID_WIDTH);

            if constexpr (UpRSP_t::OPCODE_WIDTH > 0)
                appender.Append32(flit.Opcode   , UpRSP_t::OPCODE_WIDTH);

            appender.Append32(flit.RespErr      , UpRSP_t::RESPERR_WIDTH);
            appender.Append32(flit.Resp         , UpRSP_t::RESP_WIDTH);
            appender.Append32(flit.TraceTag     , UpRSP_t::TRACETAG_WIDTH);

            //
            appender.Finish(WIDTH);

            return true;
        }

        /*
        DnDAT flit serializer.
        */
        template<DnDATFlitConfigurationConcept  config>
        inline bool SerializeDnDAT(uint32_t* flitBits, const DnDAT<config>& flit) noexcept
        {
            using DnDAT_t = DnDAT<config>;

            details::FlitAppender appender(flitBits);

            appender.Append32(flit.TxnID        , DnDAT_t::TXNID_WIDTH);
            appender.Append32(flit.SrcID        , DnDAT_t::SRCID_WIDTH);
            appender.Append32(flit.TgtID        , DnDAT_t::TGTID_WIDTH);
            appender.Append32(flit.DBID         , DnDAT_t::DBID_WIDTH);

            if constexpr (DnDAT_t::OPCODE_WIDTH > 0)
                appender.Append32(flit.Opcode   , DnDAT_t::OPCODE_WIDTH);

            appender.Append32(flit.RespErr      , DnDAT_t::RESPERR_WIDTH);
            appender.Append32(flit.Resp         , DnDAT_t::RESP_WIDTH);
            appender.Append32(flit.DataSource   , DnDAT_t::DATASOURCE_WIDTH);
            appender.Append32(flit.CBusy        , DnDAT_t::CBUSY_WIDTH);

            if constexpr (DnDAT_t::WAYVALID_WIDTH > 0)
                appender.Append32(flit.WayValid , DnDAT_t::WAYVALID_WIDTH);

            if constexpr (DnDAT_t::WAY_WIDTH > 0)
                appender.Append32(flit.Way      , DnDAT_t::WAY_WIDTH);

            if constexpr (DnDAT_t::DATAID_WIDTH > 0)
                appender.Append32(flit.DataID   , DnDAT_t::DATAID_WIDTH);

            for (size_t i = 0; i < DnDAT_t::DATA_WIDTH / 64; ++i)
                appender.Append64(flit.Data[i]  , 64);

            appender.Append32(flit.TraceTag     , DnDAT_t::TRACETAG_WIDTH);

            //
            appender.Finish(DnDAT_t::WIDTH);

            return true;
        }

        /*
        UpDAT flit serializer.
        */
        template<UpDATFlitConfigurationConcept  config>
        inline bool SerializeUpDAT(uint32_t* flitBits, const UpDAT<config>& flit) noexcept
        {
            using UpDAT_t = UpDAT<config>;

            static constexpr size_t WIDTH =
                  UpDAT_t::TXNID_WIDTH    + UpDAT_t::SRCID_WIDTH    + UpDAT_t::TGTID_WIDTH
                + UpDAT_t::OPCODE_WIDTH   + UpDAT_t::RESPERR_WIDTH  + UpDAT_t::RESP_WIDTH
                + UpDAT_t::DATAID_WIDTH   + UpDAT_t::DATA_WIDTH     + UpDAT_t::BE_WIDTH
                + UpDAT_t::TRACETAG_WIDTH;

            details::FlitAppender appender(flitBits);

            appender.Append32(flit.TxnID        , UpDAT_t::TXNID_WIDTH);
            appender.Append32(flit.SrcID        , UpDAT_t::SRCID_WIDTH);
            appender.Append32(flit.TgtID        , UpDAT_t::TGTID_WIDTH);

            if constexpr (UpDAT_t::OPCODE_WIDTH > 0)
                appender.Append32(flit.Opcode   , UpDAT_t::OPCODE_WIDTH);

            appender.Append32(flit.RespErr      , UpDAT_t::RESPERR_WIDTH);
            appender.Append32(flit.Resp         , UpDAT_t::RESP_WIDTH);

            if constexpr (UpDAT_t::DATAID_WIDTH > 0)
                appender.Append32(flit.DataID   , UpDAT_t::DATAID_WIDTH);

            for (size_t i = 0; i < UpDAT_t::DATA_WIDTH / 64; ++i)
                appender.Append64(flit.Data[i]  , 64);

            appender.Append32(flit.BE           , UpDAT_t::BE_WIDTH);
            appender.Append32(flit.TraceTag     , UpDAT_t::TRACETAG_WIDTH);

            //
            appender.Finish(WIDTH);

            return true;
        }

        //
    }
}


// Implementation of: class *Measure::Parameters
namespace CCHI {

    namespace Flits {

        inline constexpr bool EVTMeasure::Parameters::Check() const noexcept
        {
            if (!CCHI::CheckTxnIDWidth(txnIdWidth))
                return false;

            if (!CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckWayIndexWidth(wayIndexWidth))
                return false;

            return true;
        }

        inline constexpr bool REQMeasure::Parameters::Check() const noexcept
        {
            if (!CCHI::CheckTxnIDWidth(txnIdWidth))
                return false;

            if (!CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckWayIndexWidth(wayIndexWidth))
                return false;

            if (!CCHI::CheckTagAliasWidth(tagAliasWidth))
                return false;

            return true;
        }

        inline constexpr bool SNPMeasure::Parameters::Check() const noexcept
        {
            if (!CCHI::CheckDBIDWidth(dbIdWidth))
                return false;

            if (!CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
                return false;

            return true;
        }

        inline constexpr bool DnRSPMeasure::Parameters::Check() const noexcept
        {
            if (!CCHI::CheckTxnIDWidth(txnIdWidth))
                return false;

            if (!CCHI::CheckDBIDWidth(dbIdWidth))
                return false;

            if (!CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckWayIndexWidth(wayIndexWidth))
                return false;

            return true;
        }

        inline constexpr bool UpRSPMeasure::Parameters::Check() const noexcept
        {
            if (!CCHI::CheckDBIDWidth(dbIdWidth))
                return false;

            if (!CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
                return false;

            return true;
        }

        inline constexpr bool DnDATMeasure::Parameters::Check() const noexcept
        {
            if (!CCHI::CheckTxnIDWidth(txnIdWidth))
                return false;

            if (!CCHI::CheckDBIDWidth(dbIdWidth))
                return false;

            if (!CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckWayIndexWidth(wayIndexWidth))
                return false;

            if (!CCHI::CheckDataWidth(dataWidth))
                return false;

            return true;
        }

        inline constexpr bool UpDATMeasure::Parameters::Check() const noexcept
        {
            if (!CCHI::CheckDBIDWidth(dbIdWidth))
                return false;

            if (!CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
                return false;

            if (!CCHI::CheckDataWidth(dataWidth))
                return false;

            return true;
        }
    }
}


// Implementation of: class EVTMeasure
namespace CCHI {

    namespace Flits {

        inline constexpr bool EVTMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // TxnID
            width   .TxnID = params.txnIdWidth;
            lsb     .TxnID = 0;
            msb     .TxnID = width.TxnID - 1;

            // SrcID
            width   .SrcID = params.upstreamNodeIdWidth;
            lsb     .SrcID = msb.TxnID + 1;
            msb     .SrcID = msb.TxnID + width.SrcID;

            // TgtID
            width   .TgtID = params.downstreamNodeIdWidth;
            lsb     .TgtID = msb.SrcID + 1;
            msb     .TgtID = msb.SrcID + width.TgtID;

            // Opcode
            width   .Opcode = params.opcodeWidth;
            lsb     .Opcode = msb.TgtID + 1;
            msb     .Opcode = msb.TgtID + width.Opcode;

            // Addr
            width   .Addr = 48;
            lsb     .Addr = msb.Opcode + 1;
            msb     .Addr = msb.Opcode + width.Addr;

            // NS
            width   .NS = 1;
            lsb     .NS = msb.Addr + 1;
            msb     .NS = msb.Addr + width.NS;

            // MemAttr
            width   .MemAttr = 1;
            lsb     .MemAttr = msb.NS + 1;
            msb     .MemAttr = msb.NS + width.MemAttr;

            // WayValid
            width   .WayValid = params.uwPersistEnable ? 1 : 0;
            lsb     .WayValid = msb.MemAttr + 1;
            msb     .WayValid = msb.MemAttr + width.WayValid;

            // Way
            // *NOTICE: gated on <UWPredict_Enable> to follow the current model
            //          (EVT<config>::WAY_WIDTH); pending spec issue A3
            //          (doc/cchi-spec-consistency-report.md) may re-gate this
            //          on <UWPersist_Enable>.
            width   .Way = params.uwPredictEnable ? params.wayIndexWidth : 0;
            lsb     .Way = msb.WayValid + 1;
            msb     .Way = msb.WayValid + width.Way;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.Way + 1;
            msb     .TraceTag = msb.Way + width.TraceTag;

            //
            width._ = width.TxnID   + width.SrcID   + width.TgtID   + width.Opcode
                    + width.Addr    + width.NS      + width.MemAttr + width.WayValid
                    + width.Way     + width.TraceTag;
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
}


// Implementation of: class REQMeasure
namespace CCHI {

    namespace Flits {

        inline constexpr bool REQMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // TxnID
            width   .TxnID = params.txnIdWidth;
            lsb     .TxnID = 0;
            msb     .TxnID = width.TxnID - 1;

            // SrcID
            width   .SrcID = params.upstreamNodeIdWidth;
            lsb     .SrcID = msb.TxnID + 1;
            msb     .SrcID = msb.TxnID + width.SrcID;

            // TgtID
            width   .TgtID = params.downstreamNodeIdWidth;
            lsb     .TgtID = msb.SrcID + 1;
            msb     .TgtID = msb.SrcID + width.TgtID;

            // Opcode
            width   .Opcode = params.opcodeWidth;
            lsb     .Opcode = msb.TgtID + 1;
            msb     .Opcode = msb.TgtID + width.Opcode;

            // Size
            width   .Size = 3;
            lsb     .Size = msb.Opcode + 1;
            msb     .Size = msb.Opcode + width.Size;

            // Addr
            width   .Addr = 48;
            lsb     .Addr = msb.Size + 1;
            msb     .Addr = msb.Size + width.Addr;

            // TagAlias
            width   .TagAlias = params.tagAliasWidth;
            lsb     .TagAlias = msb.Addr + 1;
            msb     .TagAlias = msb.Addr + width.TagAlias;

            // NS
            width   .NS = 1;
            lsb     .NS = msb.TagAlias + 1;
            msb     .NS = msb.TagAlias + width.NS;

            // Order
            width   .Order = 2;
            lsb     .Order = msb.NS + 1;
            msb     .Order = msb.NS + width.Order;

            // MemAttr
            width   .MemAttr = 4;
            lsb     .MemAttr = msb.Order + 1;
            msb     .MemAttr = msb.Order + width.MemAttr;

            // Excl
            width   .Excl = 1;
            lsb     .Excl = msb.MemAttr + 1;
            msb     .Excl = msb.MemAttr + width.Excl;

            // ExpCompData
            width   .ExpCompData = 1;
            lsb     .ExpCompData = msb.Excl + 1;
            msb     .ExpCompData = msb.Excl + width.ExpCompData;

            // ExpCompStash (overlaps ExpCompData)
            width   .ExpCompStash = 1;
            lsb     .ExpCompStash = lsb.ExpCompData;
            msb     .ExpCompStash = msb.ExpCompData;

            // WayValid
            width   .WayValid = (params.uwPersistEnable || params.uwPredictEnable) ? 1 : 0;
            lsb     .WayValid = msb.ExpCompData + 1;
            msb     .WayValid = msb.ExpCompData + width.WayValid;

            // Way
            width   .Way = (params.uwPersistEnable || params.uwPredictEnable) ? params.wayIndexWidth : 0;
            lsb     .Way = msb.WayValid + 1;
            msb     .Way = msb.WayValid + width.Way;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.Way + 1;
            msb     .TraceTag = msb.Way + width.TraceTag;

            //
            width._ = width.TxnID       + width.SrcID   + width.TgtID       + width.Opcode
                    + width.Size        + width.Addr    + width.TagAlias    + width.NS
                    + width.Order       + width.MemAttr + width.Excl        + width.ExpCompData
                 /* + width.ExpCompStash (overlapped) */
                    + width.WayValid    + width.Way     + width.TraceTag;
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
}


// Implementation of: class SNPMeasure
namespace CCHI {

    namespace Flits {

        inline constexpr bool SNPMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // TxnID
            width   .TxnID = params.dbIdWidth;
            lsb     .TxnID = 0;
            msb     .TxnID = width.TxnID - 1;

            // SrcID
            width   .SrcID = params.downstreamNodeIdWidth;
            lsb     .SrcID = msb.TxnID + 1;
            msb     .SrcID = msb.TxnID + width.SrcID;

            // TgtID
            width   .TgtID = params.upstreamNodeIdWidth;
            lsb     .TgtID = msb.SrcID + 1;
            msb     .TgtID = msb.SrcID + width.TgtID;

            // Opcode
            width   .Opcode = params.opcodeWidth;
            lsb     .Opcode = msb.TgtID + 1;
            msb     .Opcode = msb.TgtID + width.Opcode;

            // Addr
            width   .Addr = 45;
            lsb     .Addr = msb.Opcode + 1;
            msb     .Addr = msb.Opcode + width.Addr;

            // NS
            width   .NS = 1;
            lsb     .NS = msb.Addr + 1;
            msb     .NS = msb.Addr + width.NS;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.NS + 1;
            msb     .TraceTag = msb.NS + width.TraceTag;

            //
            width._ = width.TxnID   + width.SrcID   + width.TgtID   + width.Opcode
                    + width.Addr    + width.NS      + width.TraceTag;
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
}


// Implementation of: class DnRSPMeasure
namespace CCHI {

    namespace Flits {

        inline constexpr bool DnRSPMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // TxnID
            width   .TxnID = params.txnIdWidth;
            lsb     .TxnID = 0;
            msb     .TxnID = width.TxnID - 1;

            // SrcID
            width   .SrcID = params.downstreamNodeIdWidth;
            lsb     .SrcID = msb.TxnID + 1;
            msb     .SrcID = msb.TxnID + width.SrcID;

            // TgtID
            width   .TgtID = params.upstreamNodeIdWidth;
            lsb     .TgtID = msb.SrcID + 1;
            msb     .TgtID = msb.SrcID + width.TgtID;

            // DBID
            width   .DBID = params.dbIdWidth;
            lsb     .DBID = msb.TgtID + 1;
            msb     .DBID = msb.TgtID + width.DBID;

            // Opcode
            width   .Opcode = params.opcodeWidth;
            lsb     .Opcode = msb.DBID + 1;
            msb     .Opcode = msb.DBID + width.Opcode;

            // RespErr
            width   .RespErr = 2;
            lsb     .RespErr = msb.Opcode + 1;
            msb     .RespErr = msb.Opcode + width.RespErr;

            // Resp
            width   .Resp = 3;
            lsb     .Resp = msb.RespErr + 1;
            msb     .Resp = msb.RespErr + width.Resp;

            // CBusy
            width   .CBusy = 3;
            lsb     .CBusy = msb.Resp + 1;
            msb     .CBusy = msb.Resp + width.CBusy;

            // WayValid
            width   .WayValid = params.uwPersistEnable ? 1 : 0;
            lsb     .WayValid = msb.CBusy + 1;
            msb     .WayValid = msb.CBusy + width.WayValid;

            // Way
            // *NOTICE: gated on <UWPredict_Enable> to follow the current model
            //          (DnRSP<config>::WAY_WIDTH); pending spec issue A3
            //          (doc/cchi-spec-consistency-report.md) may re-gate this
            //          on <UWPersist_Enable>.
            width   .Way = params.uwPredictEnable ? params.wayIndexWidth : 0;
            lsb     .Way = msb.WayValid + 1;
            msb     .Way = msb.WayValid + width.Way;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.Way + 1;
            msb     .TraceTag = msb.Way + width.TraceTag;

            //
            width._ = width.TxnID       + width.SrcID   + width.TgtID   + width.DBID
                    + width.Opcode      + width.RespErr + width.Resp    + width.CBusy
                    + width.WayValid    + width.Way     + width.TraceTag;
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
}


// Implementation of: class UpRSPMeasure
namespace CCHI {

    namespace Flits {

        inline constexpr bool UpRSPMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // TxnID
            width   .TxnID = params.dbIdWidth;
            lsb     .TxnID = 0;
            msb     .TxnID = width.TxnID - 1;

            // SrcID
            width   .SrcID = params.upstreamNodeIdWidth;
            lsb     .SrcID = msb.TxnID + 1;
            msb     .SrcID = msb.TxnID + width.SrcID;

            // TgtID
            width   .TgtID = params.downstreamNodeIdWidth;
            lsb     .TgtID = msb.SrcID + 1;
            msb     .TgtID = msb.SrcID + width.TgtID;

            // Opcode
            width   .Opcode = params.opcodeWidth;
            lsb     .Opcode = msb.TgtID + 1;
            msb     .Opcode = msb.TgtID + width.Opcode;

            // RespErr
            width   .RespErr = 2;
            lsb     .RespErr = msb.Opcode + 1;
            msb     .RespErr = msb.Opcode + width.RespErr;

            // Resp
            width   .Resp = 3;
            lsb     .Resp = msb.RespErr + 1;
            msb     .Resp = msb.RespErr + width.Resp;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.Resp + 1;
            msb     .TraceTag = msb.Resp + width.TraceTag;

            //
            width._ = width.TxnID   + width.SrcID   + width.TgtID   + width.Opcode
                    + width.RespErr + width.Resp    + width.TraceTag;
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
}


// Implementation of: class DnDATMeasure
namespace CCHI {

    namespace Flits {

        inline constexpr bool DnDATMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // TxnID
            width   .TxnID = params.txnIdWidth;
            lsb     .TxnID = 0;
            msb     .TxnID = width.TxnID - 1;

            // SrcID
            width   .SrcID = params.downstreamNodeIdWidth;
            lsb     .SrcID = msb.TxnID + 1;
            msb     .SrcID = msb.TxnID + width.SrcID;

            // TgtID
            width   .TgtID = params.upstreamNodeIdWidth;
            lsb     .TgtID = msb.SrcID + 1;
            msb     .TgtID = msb.SrcID + width.TgtID;

            // DBID
            width   .DBID = params.dbIdWidth;
            lsb     .DBID = msb.TgtID + 1;
            msb     .DBID = msb.TgtID + width.DBID;

            // Opcode
            width   .Opcode = params.opcodeWidth;
            lsb     .Opcode = msb.DBID + 1;
            msb     .Opcode = msb.DBID + width.Opcode;

            // RespErr
            width   .RespErr = 2;
            lsb     .RespErr = msb.Opcode + 1;
            msb     .RespErr = msb.Opcode + width.RespErr;

            // Resp
            width   .Resp = 3;
            lsb     .Resp = msb.RespErr + 1;
            msb     .Resp = msb.RespErr + width.Resp;

            // DataSource
            width   .DataSource = 5;
            lsb     .DataSource = msb.Resp + 1;
            msb     .DataSource = msb.Resp + width.DataSource;

            // CBusy
            width   .CBusy = 3;
            lsb     .CBusy = msb.DataSource + 1;
            msb     .CBusy = msb.DataSource + width.CBusy;

            // WayValid
            width   .WayValid = params.uwPersistEnable ? 1 : 0;
            lsb     .WayValid = msb.CBusy + 1;
            msb     .WayValid = msb.CBusy + width.WayValid;

            // Way
            // *NOTICE: gated on <UWPredict_Enable> to follow the current model
            //          (DnDAT<config>::WAY_WIDTH); pending spec issue A3
            //          (doc/cchi-spec-consistency-report.md) may re-gate this
            //          on <UWPersist_Enable>.
            width   .Way = params.uwPredictEnable ? params.wayIndexWidth : 0;
            lsb     .Way = msb.WayValid + 1;
            msb     .Way = msb.WayValid + width.Way;

            // DataID
            width   .DataID = std::bit_width(size_t(512 / params.dataWidth - 1));
            lsb     .DataID = msb.Way + 1;
            msb     .DataID = msb.Way + width.DataID;

            // Data
            width   .Data = params.dataWidth;
            lsb     .Data = msb.DataID + 1;
            msb     .Data = msb.DataID + width.Data;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.Data + 1;
            msb     .TraceTag = msb.Data + width.TraceTag;

            //
            width._ = width.TxnID       + width.SrcID       + width.TgtID   + width.DBID
                    + width.Opcode      + width.RespErr     + width.Resp    + width.DataSource
                    + width.CBusy       + width.WayValid    + width.Way     + width.DataID
                    + width.Data        + width.TraceTag;
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
}


// Implementation of: class UpDATMeasure
namespace CCHI {

    namespace Flits {

        inline constexpr bool UpDATMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // TxnID
            width   .TxnID = params.dbIdWidth;
            lsb     .TxnID = 0;
            msb     .TxnID = width.TxnID - 1;

            // SrcID
            width   .SrcID = params.upstreamNodeIdWidth;
            lsb     .SrcID = msb.TxnID + 1;
            msb     .SrcID = msb.TxnID + width.SrcID;

            // TgtID
            width   .TgtID = params.downstreamNodeIdWidth;
            lsb     .TgtID = msb.SrcID + 1;
            msb     .TgtID = msb.SrcID + width.TgtID;

            // Opcode
            width   .Opcode = params.opcodeWidth;
            lsb     .Opcode = msb.TgtID + 1;
            msb     .Opcode = msb.TgtID + width.Opcode;

            // RespErr
            width   .RespErr = 2;
            lsb     .RespErr = msb.Opcode + 1;
            msb     .RespErr = msb.Opcode + width.RespErr;

            // Resp
            width   .Resp = 3;
            lsb     .Resp = msb.RespErr + 1;
            msb     .Resp = msb.RespErr + width.Resp;

            // DataID
            width   .DataID = std::bit_width(size_t(512 / params.dataWidth - 1));
            lsb     .DataID = msb.Resp + 1;
            msb     .DataID = msb.Resp + width.DataID;

            // Data
            width   .Data = params.dataWidth;
            lsb     .Data = msb.DataID + 1;
            msb     .Data = msb.DataID + width.Data;

            // BE
            width   .BE = 32;
            lsb     .BE = msb.Data + 1;
            msb     .BE = msb.Data + width.BE;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.BE + 1;
            msb     .TraceTag = msb.BE + width.TraceTag;

            //
            width._ = width.TxnID       + width.SrcID   + width.TgtID   + width.Opcode
                    + width.RespErr     + width.Resp    + width.DataID  + width.Data
                    + width.BE          + width.TraceTag;
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
}


// Implementation of: class *Measure (byte-length evaluators)
namespace CCHI {

    namespace Flits {

        inline constexpr size_t EVTMeasure::EvalByteLength(const Parameters& params) noexcept
        {
            EVTMeasure measure{};

            if (!measure.Eval(params))
                return 0;

            return measure.ByteLength();
        }

        inline constexpr size_t REQMeasure::EvalByteLength(const Parameters& params) noexcept
        {
            REQMeasure measure{};

            if (!measure.Eval(params))
                return 0;

            return measure.ByteLength();
        }

        inline constexpr size_t SNPMeasure::EvalByteLength(const Parameters& params) noexcept
        {
            SNPMeasure measure{};

            if (!measure.Eval(params))
                return 0;

            return measure.ByteLength();
        }

        inline constexpr size_t DnRSPMeasure::EvalByteLength(const Parameters& params) noexcept
        {
            DnRSPMeasure measure{};

            if (!measure.Eval(params))
                return 0;

            return measure.ByteLength();
        }

        inline constexpr size_t UpRSPMeasure::EvalByteLength(const Parameters& params) noexcept
        {
            UpRSPMeasure measure{};

            if (!measure.Eval(params))
                return 0;

            return measure.ByteLength();
        }

        inline constexpr size_t DnDATMeasure::EvalByteLength(const Parameters& params) noexcept
        {
            DnDATMeasure measure{};

            if (!measure.Eval(params))
                return 0;

            return measure.ByteLength();
        }

        inline constexpr size_t UpDATMeasure::EvalByteLength(const Parameters& params) noexcept
        {
            UpDATMeasure measure{};

            if (!measure.Eval(params))
                return 0;

            return measure.ByteLength();
        }

#ifndef CCHI_UTIL_FLIT__NO_CLOG_INTEROP
        //
        /*
        Run-time byte-length evaluators on CLog CCHI parameters.
        The opcode width of each channel is taken from the component type
        descriptor (cchi/basic/cchi_components.hpp), keyed by the node type.
        */
        inline size_t EVTMeasure::EvalByteLength(const CLog::CCHI::Parameters& params) noexcept
        {
            Parameters p;

            p.opcodeWidth            = details::ComponentTypeOf(params.GetComponentType())->opcodeWidthEVT;
            p.txnIdWidth             = params.GetTxnIdWidth();
            p.upstreamNodeIdWidth    = params.GetUpstreamNodeIdWidth();
            p.downstreamNodeIdWidth  = params.GetDownstreamNodeIdWidth();
            p.wayIndexWidth          = params.GetWayIndexWidth();
            p.uwPersistEnable        = params.IsUWPersistEnabled();
            p.uwPredictEnable        = params.IsUWPredictEnabled();

            return EvalByteLength(p);
        }

        inline size_t REQMeasure::EvalByteLength(const CLog::CCHI::Parameters& params) noexcept
        {
            Parameters p;

            p.opcodeWidth            = details::ComponentTypeOf(params.GetComponentType())->opcodeWidthREQ;
            p.txnIdWidth             = params.GetTxnIdWidth();
            p.upstreamNodeIdWidth    = params.GetUpstreamNodeIdWidth();
            p.downstreamNodeIdWidth  = params.GetDownstreamNodeIdWidth();
            p.wayIndexWidth          = params.GetWayIndexWidth();
            p.tagAliasWidth          = params.GetTagAliasWidth();
            p.uwPersistEnable        = params.IsUWPersistEnabled();
            p.uwPredictEnable        = params.IsUWPredictEnabled();

            return EvalByteLength(p);
        }

        inline size_t SNPMeasure::EvalByteLength(const CLog::CCHI::Parameters& params) noexcept
        {
            Parameters p;

            p.opcodeWidth            = details::ComponentTypeOf(params.GetComponentType())->opcodeWidthSNP;
            p.dbIdWidth              = params.GetDBIdWidth();
            p.upstreamNodeIdWidth    = params.GetUpstreamNodeIdWidth();
            p.downstreamNodeIdWidth  = params.GetDownstreamNodeIdWidth();

            return EvalByteLength(p);
        }

        inline size_t DnRSPMeasure::EvalByteLength(const CLog::CCHI::Parameters& params) noexcept
        {
            Parameters p;

            p.opcodeWidth            = details::ComponentTypeOf(params.GetComponentType())->opcodeWidthDnRSP;
            p.txnIdWidth             = params.GetTxnIdWidth();
            p.dbIdWidth              = params.GetDBIdWidth();
            p.upstreamNodeIdWidth    = params.GetUpstreamNodeIdWidth();
            p.downstreamNodeIdWidth  = params.GetDownstreamNodeIdWidth();
            p.wayIndexWidth          = params.GetWayIndexWidth();
            p.uwPersistEnable        = params.IsUWPersistEnabled();
            p.uwPredictEnable        = params.IsUWPredictEnabled();

            return EvalByteLength(p);
        }

        inline size_t UpRSPMeasure::EvalByteLength(const CLog::CCHI::Parameters& params) noexcept
        {
            Parameters p;

            p.opcodeWidth            = details::ComponentTypeOf(params.GetComponentType())->opcodeWidthUpRSP;
            p.dbIdWidth              = params.GetDBIdWidth();
            p.upstreamNodeIdWidth    = params.GetUpstreamNodeIdWidth();
            p.downstreamNodeIdWidth  = params.GetDownstreamNodeIdWidth();

            return EvalByteLength(p);
        }

        inline size_t DnDATMeasure::EvalByteLength(const CLog::CCHI::Parameters& params) noexcept
        {
            Parameters p;

            p.opcodeWidth            = details::ComponentTypeOf(params.GetComponentType())->opcodeWidthDnDAT;
            p.txnIdWidth             = params.GetTxnIdWidth();
            p.dbIdWidth              = params.GetDBIdWidth();
            p.upstreamNodeIdWidth    = params.GetUpstreamNodeIdWidth();
            p.downstreamNodeIdWidth  = params.GetDownstreamNodeIdWidth();
            p.wayIndexWidth          = params.GetWayIndexWidth();
            p.dataWidth              = params.GetDataWidth();
            p.uwPersistEnable        = params.IsUWPersistEnabled();
            p.uwPredictEnable        = params.IsUWPredictEnabled();

            return EvalByteLength(p);
        }

        inline size_t UpDATMeasure::EvalByteLength(const CLog::CCHI::Parameters& params) noexcept
        {
            Parameters p;

            p.opcodeWidth            = details::ComponentTypeOf(params.GetComponentType())->opcodeWidthUpDAT;
            p.dbIdWidth              = params.GetDBIdWidth();
            p.upstreamNodeIdWidth    = params.GetUpstreamNodeIdWidth();
            p.downstreamNodeIdWidth  = params.GetDownstreamNodeIdWidth();
            p.dataWidth              = params.GetDataWidth();

            return EvalByteLength(p);
        }
#endif // CCHI_UTIL_FLIT__NO_CLOG_INTEROP
    }
}


#endif // __CCHI__CCHI_UTIL_FLIT
