#pragma once

#include "flit_record.hpp"

#include "cchi/util/cchi_util_decoding.hpp"
#include "cchi/util/cchi_util_flit.hpp"

#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace CHIron::Gui {

// Widest legal CCHI flit configuration: every parameter at its spec maximum
// and both upstream-way features enabled, so a runtime-measured flit of any
// legal CCHI configuration fits into these storage types.
// *NOTICE: dataIdWidth is the maximum over the legal data widths
//          (bit_width(512 / 64 - 1) = 3); CCHI::FlitConfiguration cannot
//          express it together with dataWidth = 512 (where DataID vanishes),
//          which is why this is a hand-written concept-conforming struct
//          rather than a FlitConfiguration alias.
struct FlexibleCCHIConfig {
    static constexpr ::CCHI::ComponentTypeEnum componentType = ::CCHI::ComponentType::TYPE_1;
    //
    static constexpr std::size_t opcodeEVTWidth    = ::CCHI::ComponentType::TYPE_1.opcodeWidthEVT;
    static constexpr std::size_t opcodeREQWidth    = ::CCHI::ComponentType::TYPE_1.opcodeWidthREQ;
    static constexpr std::size_t opcodeSNPWidth    = ::CCHI::ComponentType::TYPE_1.opcodeWidthSNP;
    static constexpr std::size_t opcodeDnRSPWidth  = ::CCHI::ComponentType::TYPE_1.opcodeWidthDnRSP;
    static constexpr std::size_t opcodeUpRSPWidth  = ::CCHI::ComponentType::TYPE_1.opcodeWidthUpRSP;
    static constexpr std::size_t opcodeDnDATWidth  = ::CCHI::ComponentType::TYPE_1.opcodeWidthDnDAT;
    static constexpr std::size_t opcodeUpDATWidth  = ::CCHI::ComponentType::TYPE_1.opcodeWidthUpDAT;
    static constexpr bool      hasEVT              = true;
    static constexpr bool      hasREQ              = true;
    static constexpr bool      hasSNP              = true;
    static constexpr bool      hasDnRSP            = true;
    static constexpr bool      hasUpRSP            = true;
    static constexpr bool      hasDnDAT            = true;
    static constexpr bool      hasUpDAT            = true;
    //
    static constexpr std::size_t txnIdWidth                = 10;
    static constexpr std::size_t dbIdWidth                 = 10;
    static constexpr std::size_t upstreamNodeIdWidth       = 7;
    static constexpr std::size_t downstreamNodeIdWidth     = 7;
    static constexpr std::size_t wayIndexWidth             = 5;
    static constexpr std::size_t dataIdWidth               = 3;
    static constexpr std::size_t dataWidth                 = 512;
    static constexpr std::size_t tagAliasWidth             = 8;
    static constexpr bool      upstreamWayPersistEnable    = true;
    static constexpr bool      upstreamWayPredictEnable    = true;
};

using FlexibleCCHIEvtFlit   = ::CCHI::Flits::EVT<FlexibleCCHIConfig>;
using FlexibleCCHIReqFlit   = ::CCHI::Flits::REQ<FlexibleCCHIConfig>;
using FlexibleCCHISnpFlit   = ::CCHI::Flits::SNP<FlexibleCCHIConfig>;
using FlexibleCCHIDnRspFlit = ::CCHI::Flits::DnRSP<FlexibleCCHIConfig>;
using FlexibleCCHIUpRspFlit = ::CCHI::Flits::UpRSP<FlexibleCCHIConfig>;
using FlexibleCCHIDnDatFlit = ::CCHI::Flits::DnDAT<FlexibleCCHIConfig>;
using FlexibleCCHIUpDatFlit = ::CCHI::Flits::UpDAT<FlexibleCCHIConfig>;

// UpRSP/UpDAT flit classes do not expose a WIDTH constant (unlike the other
// channels), so the flexible capacities are computed from the field widths.
inline constexpr std::size_t FlexibleCCHIUpRspFlitWidth =
      FlexibleCCHIUpRspFlit::TXNID_WIDTH   + FlexibleCCHIUpRspFlit::SRCID_WIDTH
    + FlexibleCCHIUpRspFlit::TGTID_WIDTH   + FlexibleCCHIUpRspFlit::OPCODE_WIDTH
    + FlexibleCCHIUpRspFlit::RESPERR_WIDTH + FlexibleCCHIUpRspFlit::RESP_WIDTH
    + FlexibleCCHIUpRspFlit::TRACETAG_WIDTH;

inline constexpr std::size_t FlexibleCCHIUpDatFlitWidth =
      FlexibleCCHIUpDatFlit::TXNID_WIDTH   + FlexibleCCHIUpDatFlit::SRCID_WIDTH
    + FlexibleCCHIUpDatFlit::TGTID_WIDTH   + FlexibleCCHIUpDatFlit::OPCODE_WIDTH
    + FlexibleCCHIUpDatFlit::RESPERR_WIDTH + FlexibleCCHIUpDatFlit::RESP_WIDTH
    + FlexibleCCHIUpDatFlit::DATAID_WIDTH  + FlexibleCCHIUpDatFlit::DATA_WIDTH
    + FlexibleCCHIUpDatFlit::BE_WIDTH      + FlexibleCCHIUpDatFlit::TRACETAG_WIDTH;

// Fills FlitRecord rows from runtime-deserialized CCHI flits. Unlike the CHI
// adapters there is no Expresso mapping for CCHI, so the display columns and
// field rows are built by hand; the per-channel Measure carries the runtime
// field widths and gates conditionally-present fields (Opcode, WayValid, Way,
// TagAlias, DataID).
class CCHIFlitAdapter {
public:
    FlitRecord fromEVT(const qint64 timestamp,
                       const FlitDirection direction,
                       const FlexibleCCHIEvtFlit& flit,
                       const ::CCHI::Flits::EVTMeasure& measure) const
    {
        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Req;
        record.direction = direction;
        record.channelTag = QStringLiteral("EVT");
        fillOpcode(record, evtOpcodeName(flit), static_cast<qulonglong>(flit.Opcode), measure.width.Opcode > 0);
        record.source = internDecimal(static_cast<qulonglong>(flit.SrcID));
        record.target = internDecimal(static_cast<qulonglong>(flit.TgtID));
        record.txnId = decimalText(static_cast<qulonglong>(flit.TxnID));
        record.address = fixedHexText(static_cast<qulonglong>(flit.Addr), measure.width.Addr);
        record.summary = joinSummary({
            labelValue(QStringLiteral("Addr"), record.address),
            labelValue(QStringLiteral("NS"), decimalText(static_cast<qulonglong>(flit.NS))),
            labelValue(QStringLiteral("MemAttr"), decimalText(static_cast<qulonglong>(flit.MemAttr))),
        });
        record.transactionLabel = QStringLiteral("Txn %1").arg(record.txnId);

        const QString* scope = InternFieldText(QStringLiteral("EVT"));
        record.fields.reserve(16);
        appendField(record, scope, QStringLiteral("TxnID"), decimalText(static_cast<qulonglong>(flit.TxnID)), static_cast<qulonglong>(flit.TxnID));
        appendField(record, scope, QStringLiteral("SrcID"), decimalText(static_cast<qulonglong>(flit.SrcID)), static_cast<qulonglong>(flit.SrcID));
        appendField(record, scope, QStringLiteral("TgtID"), decimalText(static_cast<qulonglong>(flit.TgtID)), static_cast<qulonglong>(flit.TgtID));
        if (measure.width.Opcode > 0) {
            appendField(record, scope, QStringLiteral("Opcode"), evtOpcodeName(flit), static_cast<qulonglong>(flit.Opcode));
        }
        appendField(record, scope, QStringLiteral("Addr"), record.address, static_cast<qulonglong>(flit.Addr));
        appendField(record, scope, QStringLiteral("NS"), decimalText(static_cast<qulonglong>(flit.NS)), static_cast<qulonglong>(flit.NS));
        appendField(record, scope, QStringLiteral("MemAttr"), decimalText(static_cast<qulonglong>(flit.MemAttr)), static_cast<qulonglong>(flit.MemAttr));
        if (measure.width.WayValid > 0) {
            appendField(record, scope, QStringLiteral("WayValid"), decimalText(static_cast<qulonglong>(flit.WayValid)), static_cast<qulonglong>(flit.WayValid));
        }
        if (measure.width.Way > 0) {
            appendField(record, scope, QStringLiteral("Way"), decimalText(static_cast<qulonglong>(flit.Way)), static_cast<qulonglong>(flit.Way));
        }
        appendField(record, scope, QStringLiteral("TraceTag"), decimalText(static_cast<qulonglong>(flit.TraceTag)), static_cast<qulonglong>(flit.TraceTag));
        return record;
    }

    FlitRecord fromREQ(const qint64 timestamp,
                       const FlitDirection direction,
                       const FlexibleCCHIReqFlit& flit,
                       const ::CCHI::Flits::REQMeasure& measure) const
    {
        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Req;
        record.direction = direction;
        fillOpcode(record, reqOpcodeName(flit), static_cast<qulonglong>(flit.Opcode), measure.width.Opcode > 0);
        record.source = internDecimal(static_cast<qulonglong>(flit.SrcID));
        record.target = internDecimal(static_cast<qulonglong>(flit.TgtID));
        record.txnId = decimalText(static_cast<qulonglong>(flit.TxnID));
        record.address = fixedHexText(static_cast<qulonglong>(flit.Addr), measure.width.Addr);
        record.summary = joinSummary({
            labelValue(QStringLiteral("Addr"), record.address),
            labelValue(QStringLiteral("Size"), sizeName(flit)),
            labelValue(QStringLiteral("Order"), decimalText(static_cast<qulonglong>(flit.Order))),
            labelValue(QStringLiteral("NS"), decimalText(static_cast<qulonglong>(flit.NS))),
        });
        record.transactionLabel = QStringLiteral("Txn %1").arg(record.txnId);

        const QString* scope = InternFieldText(QStringLiteral("REQ"));
        record.fields.reserve(16);
        appendField(record, scope, QStringLiteral("TxnID"), decimalText(static_cast<qulonglong>(flit.TxnID)), static_cast<qulonglong>(flit.TxnID));
        appendField(record, scope, QStringLiteral("SrcID"), decimalText(static_cast<qulonglong>(flit.SrcID)), static_cast<qulonglong>(flit.SrcID));
        appendField(record, scope, QStringLiteral("TgtID"), decimalText(static_cast<qulonglong>(flit.TgtID)), static_cast<qulonglong>(flit.TgtID));
        if (measure.width.Opcode > 0) {
            appendField(record, scope, QStringLiteral("Opcode"), reqOpcodeName(flit), static_cast<qulonglong>(flit.Opcode));
        }
        appendField(record, scope, QStringLiteral("Size"), sizeName(flit), static_cast<qulonglong>(flit.Size));
        appendField(record, scope, QStringLiteral("Addr"), record.address, static_cast<qulonglong>(flit.Addr));
        if (measure.width.TagAlias > 0) {
            appendField(record, scope, QStringLiteral("TagAlias"), fixedHexText(static_cast<qulonglong>(flit.TagAlias), measure.width.TagAlias), static_cast<qulonglong>(flit.TagAlias));
        }
        appendField(record, scope, QStringLiteral("NS"), decimalText(static_cast<qulonglong>(flit.NS)), static_cast<qulonglong>(flit.NS));
        appendField(record, scope, QStringLiteral("Order"), decimalText(static_cast<qulonglong>(flit.Order)), static_cast<qulonglong>(flit.Order));
        appendField(record, scope, QStringLiteral("MemAttr"), decimalText(static_cast<qulonglong>(flit.MemAttr)), static_cast<qulonglong>(flit.MemAttr));
        appendField(record, scope, QStringLiteral("Excl"), decimalText(static_cast<qulonglong>(flit.Excl)), static_cast<qulonglong>(flit.Excl));
        appendField(record, scope, QStringLiteral("ExpCompData"), decimalText(static_cast<qulonglong>(flit.ExpCompData)), static_cast<qulonglong>(flit.ExpCompData));
        if (measure.width.WayValid > 0) {
            appendField(record, scope, QStringLiteral("WayValid"), decimalText(static_cast<qulonglong>(flit.WayValid)), static_cast<qulonglong>(flit.WayValid));
        }
        if (measure.width.Way > 0) {
            appendField(record, scope, QStringLiteral("Way"), decimalText(static_cast<qulonglong>(flit.Way)), static_cast<qulonglong>(flit.Way));
        }
        appendField(record, scope, QStringLiteral("TraceTag"), decimalText(static_cast<qulonglong>(flit.TraceTag)), static_cast<qulonglong>(flit.TraceTag));
        return record;
    }

    FlitRecord fromSNP(const qint64 timestamp,
                       const FlitDirection direction,
                       const FlexibleCCHISnpFlit& flit,
                       const ::CCHI::Flits::SNPMeasure& measure) const
    {
        // SNP addresses are stored without the low 3 bits (cacheline granularity).
        const qulonglong address = static_cast<qulonglong>(flit.Addr) << 3;

        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Snp;
        record.direction = direction;
        fillOpcode(record, snpOpcodeName(flit), static_cast<qulonglong>(flit.Opcode), measure.width.Opcode > 0);
        record.source = internDecimal(static_cast<qulonglong>(flit.SrcID));
        record.target = internDecimal(static_cast<qulonglong>(flit.TgtID));
        record.txnId = decimalText(static_cast<qulonglong>(flit.TxnID));
        record.address = fixedHexText(address, measure.width.Addr + 3);
        record.summary = joinSummary({
            labelValue(QStringLiteral("Addr"), record.address),
            labelValue(QStringLiteral("NS"), decimalText(static_cast<qulonglong>(flit.NS))),
        });
        record.transactionLabel = QStringLiteral("Snoop %1").arg(record.txnId);

        const QString* scope = InternFieldText(QStringLiteral("SNP"));
        record.fields.reserve(16);
        appendField(record, scope, QStringLiteral("TxnID"), decimalText(static_cast<qulonglong>(flit.TxnID)), static_cast<qulonglong>(flit.TxnID));
        appendField(record, scope, QStringLiteral("SrcID"), decimalText(static_cast<qulonglong>(flit.SrcID)), static_cast<qulonglong>(flit.SrcID));
        appendField(record, scope, QStringLiteral("TgtID"), decimalText(static_cast<qulonglong>(flit.TgtID)), static_cast<qulonglong>(flit.TgtID));
        if (measure.width.Opcode > 0) {
            appendField(record, scope, QStringLiteral("Opcode"), snpOpcodeName(flit), static_cast<qulonglong>(flit.Opcode));
        }
        appendField(record, scope, QStringLiteral("Addr"), record.address, static_cast<qulonglong>(flit.Addr));
        appendField(record, scope, QStringLiteral("NS"), decimalText(static_cast<qulonglong>(flit.NS)), static_cast<qulonglong>(flit.NS));
        appendField(record, scope, QStringLiteral("TraceTag"), decimalText(static_cast<qulonglong>(flit.TraceTag)), static_cast<qulonglong>(flit.TraceTag));
        return record;
    }

    FlitRecord fromDnRSP(const qint64 timestamp,
                         const FlitDirection direction,
                         const FlexibleCCHIDnRspFlit& flit,
                         const ::CCHI::Flits::DnRSPMeasure& measure) const
    {
        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Rsp;
        record.direction = direction;
        record.channelTag = QStringLiteral("DNRSP");
        fillOpcode(record, dnRspOpcodeName(flit), static_cast<qulonglong>(flit.Opcode), measure.width.Opcode > 0);
        record.source = internDecimal(static_cast<qulonglong>(flit.SrcID));
        record.target = internDecimal(static_cast<qulonglong>(flit.TgtID));
        record.txnId = decimalText(static_cast<qulonglong>(flit.TxnID));
        record.dbid = decimalText(static_cast<qulonglong>(flit.DBID));
        record.resp = InternDisplayString(respName(flit.Resp));
        record.respErr = InternDisplayString(decimalText(static_cast<qulonglong>(flit.RespErr)));
        record.summary = joinSummary({
            labelValue(QStringLiteral("Resp"), record.resp),
            labelValue(QStringLiteral("Err"), record.respErr),
            labelValue(QStringLiteral("DBID"), record.dbid),
        });
        record.transactionLabel = makeTxnLabel(record.txnId, record.dbid);

        const QString* scope = InternFieldText(QStringLiteral("DNRSP"));
        record.fields.reserve(16);
        appendField(record, scope, QStringLiteral("TxnID"), decimalText(static_cast<qulonglong>(flit.TxnID)), static_cast<qulonglong>(flit.TxnID));
        appendField(record, scope, QStringLiteral("SrcID"), decimalText(static_cast<qulonglong>(flit.SrcID)), static_cast<qulonglong>(flit.SrcID));
        appendField(record, scope, QStringLiteral("TgtID"), decimalText(static_cast<qulonglong>(flit.TgtID)), static_cast<qulonglong>(flit.TgtID));
        appendField(record, scope, QStringLiteral("DBID"), decimalText(static_cast<qulonglong>(flit.DBID)), static_cast<qulonglong>(flit.DBID));
        if (measure.width.Opcode > 0) {
            appendField(record, scope, QStringLiteral("Opcode"), dnRspOpcodeName(flit), static_cast<qulonglong>(flit.Opcode));
        }
        appendField(record, scope, QStringLiteral("RespErr"), decimalText(static_cast<qulonglong>(flit.RespErr)), static_cast<qulonglong>(flit.RespErr));
        appendField(record, scope, QStringLiteral("Resp"), respName(flit.Resp), static_cast<qulonglong>(flit.Resp));
        appendField(record, scope, QStringLiteral("CBusy"), decimalText(static_cast<qulonglong>(flit.CBusy)), static_cast<qulonglong>(flit.CBusy));
        if (measure.width.WayValid > 0) {
            appendField(record, scope, QStringLiteral("WayValid"), decimalText(static_cast<qulonglong>(flit.WayValid)), static_cast<qulonglong>(flit.WayValid));
        }
        if (measure.width.Way > 0) {
            appendField(record, scope, QStringLiteral("Way"), decimalText(static_cast<qulonglong>(flit.Way)), static_cast<qulonglong>(flit.Way));
        }
        appendField(record, scope, QStringLiteral("TraceTag"), decimalText(static_cast<qulonglong>(flit.TraceTag)), static_cast<qulonglong>(flit.TraceTag));
        return record;
    }

    FlitRecord fromUpRSP(const qint64 timestamp,
                         const FlitDirection direction,
                         const FlexibleCCHIUpRspFlit& flit,
                         const ::CCHI::Flits::UpRSPMeasure& measure) const
    {
        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Rsp;
        record.direction = direction;
        record.channelTag = QStringLiteral("UPRSP");
        fillOpcode(record, upRspOpcodeName(flit), static_cast<qulonglong>(flit.Opcode), measure.width.Opcode > 0);
        record.source = internDecimal(static_cast<qulonglong>(flit.SrcID));
        record.target = internDecimal(static_cast<qulonglong>(flit.TgtID));
        record.txnId = decimalText(static_cast<qulonglong>(flit.TxnID));
        record.resp = InternDisplayString(respName(flit.Resp));
        record.respErr = InternDisplayString(decimalText(static_cast<qulonglong>(flit.RespErr)));
        record.summary = joinSummary({
            labelValue(QStringLiteral("Resp"), record.resp),
            labelValue(QStringLiteral("Err"), record.respErr),
        });
        record.transactionLabel = QStringLiteral("Txn %1").arg(record.txnId);

        const QString* scope = InternFieldText(QStringLiteral("UPRSP"));
        record.fields.reserve(16);
        appendField(record, scope, QStringLiteral("TxnID"), decimalText(static_cast<qulonglong>(flit.TxnID)), static_cast<qulonglong>(flit.TxnID));
        appendField(record, scope, QStringLiteral("SrcID"), decimalText(static_cast<qulonglong>(flit.SrcID)), static_cast<qulonglong>(flit.SrcID));
        appendField(record, scope, QStringLiteral("TgtID"), decimalText(static_cast<qulonglong>(flit.TgtID)), static_cast<qulonglong>(flit.TgtID));
        if (measure.width.Opcode > 0) {
            appendField(record, scope, QStringLiteral("Opcode"), upRspOpcodeName(flit), static_cast<qulonglong>(flit.Opcode));
        }
        appendField(record, scope, QStringLiteral("RespErr"), decimalText(static_cast<qulonglong>(flit.RespErr)), static_cast<qulonglong>(flit.RespErr));
        appendField(record, scope, QStringLiteral("Resp"), respName(flit.Resp), static_cast<qulonglong>(flit.Resp));
        appendField(record, scope, QStringLiteral("TraceTag"), decimalText(static_cast<qulonglong>(flit.TraceTag)), static_cast<qulonglong>(flit.TraceTag));
        return record;
    }

    FlitRecord fromDnDAT(const qint64 timestamp,
                         const FlitDirection direction,
                         const FlexibleCCHIDnDatFlit& flit,
                         const ::CCHI::Flits::DnDATMeasure& measure) const
    {
        const bool hasDataId = measure.width.DataID > 0;

        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Dat;
        record.direction = direction;
        record.channelTag = QStringLiteral("DNDAT");
        fillOpcode(record, dnDatOpcodeName(flit), static_cast<qulonglong>(flit.Opcode), measure.width.Opcode > 0);
        record.source = internDecimal(static_cast<qulonglong>(flit.SrcID));
        record.target = internDecimal(static_cast<qulonglong>(flit.TgtID));
        record.txnId = decimalText(static_cast<qulonglong>(flit.TxnID));
        record.dbid = decimalText(static_cast<qulonglong>(flit.DBID));
        if (hasDataId) {
            record.dataId = decimalText(static_cast<qulonglong>(flit.DataID));
        }
        record.resp = InternDisplayString(respName(flit.Resp));
        record.respErr = InternDisplayString(decimalText(static_cast<qulonglong>(flit.RespErr)));
        record.summary = joinSummary({
            labelValue(QStringLiteral("Resp"), record.resp),
            labelValue(QStringLiteral("Beat"), record.dataId),
            labelValue(QStringLiteral("DBID"), record.dbid),
            labelValue(QStringLiteral("Src"), decimalText(static_cast<qulonglong>(flit.DataSource))),
        });
        record.transactionLabel = makeTxnLabel(record.txnId, record.dbid);

        const QString* scope = InternFieldText(QStringLiteral("DNDAT"));
        record.fields.reserve(16);
        appendField(record, scope, QStringLiteral("TxnID"), decimalText(static_cast<qulonglong>(flit.TxnID)), static_cast<qulonglong>(flit.TxnID));
        appendField(record, scope, QStringLiteral("SrcID"), decimalText(static_cast<qulonglong>(flit.SrcID)), static_cast<qulonglong>(flit.SrcID));
        appendField(record, scope, QStringLiteral("TgtID"), decimalText(static_cast<qulonglong>(flit.TgtID)), static_cast<qulonglong>(flit.TgtID));
        appendField(record, scope, QStringLiteral("DBID"), decimalText(static_cast<qulonglong>(flit.DBID)), static_cast<qulonglong>(flit.DBID));
        if (measure.width.Opcode > 0) {
            appendField(record, scope, QStringLiteral("Opcode"), dnDatOpcodeName(flit), static_cast<qulonglong>(flit.Opcode));
        }
        appendField(record, scope, QStringLiteral("RespErr"), decimalText(static_cast<qulonglong>(flit.RespErr)), static_cast<qulonglong>(flit.RespErr));
        appendField(record, scope, QStringLiteral("Resp"), respName(flit.Resp), static_cast<qulonglong>(flit.Resp));
        appendField(record, scope, QStringLiteral("DataSource"), decimalText(static_cast<qulonglong>(flit.DataSource)), static_cast<qulonglong>(flit.DataSource));
        appendField(record, scope, QStringLiteral("CBusy"), decimalText(static_cast<qulonglong>(flit.CBusy)), static_cast<qulonglong>(flit.CBusy));
        if (measure.width.WayValid > 0) {
            appendField(record, scope, QStringLiteral("WayValid"), decimalText(static_cast<qulonglong>(flit.WayValid)), static_cast<qulonglong>(flit.WayValid));
        }
        if (measure.width.Way > 0) {
            appendField(record, scope, QStringLiteral("Way"), decimalText(static_cast<qulonglong>(flit.Way)), static_cast<qulonglong>(flit.Way));
        }
        if (hasDataId) {
            appendField(record, scope, QStringLiteral("DataID"), decimalText(static_cast<qulonglong>(flit.DataID)), static_cast<qulonglong>(flit.DataID));
        }
        appendDataField(record, scope, flit.Data, measure.width.Data);
        appendField(record, scope, QStringLiteral("TraceTag"), decimalText(static_cast<qulonglong>(flit.TraceTag)), static_cast<qulonglong>(flit.TraceTag));
        return record;
    }

    FlitRecord fromUpDAT(const qint64 timestamp,
                         const FlitDirection direction,
                         const FlexibleCCHIUpDatFlit& flit,
                         const ::CCHI::Flits::UpDATMeasure& measure) const
    {
        const bool hasDataId = measure.width.DataID > 0;

        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Dat;
        record.direction = direction;
        record.channelTag = QStringLiteral("UPDAT");
        fillOpcode(record, upDatOpcodeName(flit), static_cast<qulonglong>(flit.Opcode), measure.width.Opcode > 0);
        record.source = internDecimal(static_cast<qulonglong>(flit.SrcID));
        record.target = internDecimal(static_cast<qulonglong>(flit.TgtID));
        record.txnId = decimalText(static_cast<qulonglong>(flit.TxnID));
        if (hasDataId) {
            record.dataId = decimalText(static_cast<qulonglong>(flit.DataID));
        }
        record.resp = InternDisplayString(respName(flit.Resp));
        record.respErr = InternDisplayString(decimalText(static_cast<qulonglong>(flit.RespErr)));
        record.summary = joinSummary({
            labelValue(QStringLiteral("Resp"), record.resp),
            labelValue(QStringLiteral("Beat"), record.dataId),
            labelValue(QStringLiteral("BE"), fixedHexText(static_cast<qulonglong>(flit.BE), measure.width.BE)),
        });
        record.transactionLabel = QStringLiteral("Txn %1").arg(record.txnId);

        const QString* scope = InternFieldText(QStringLiteral("UPDAT"));
        record.fields.reserve(16);
        appendField(record, scope, QStringLiteral("TxnID"), decimalText(static_cast<qulonglong>(flit.TxnID)), static_cast<qulonglong>(flit.TxnID));
        appendField(record, scope, QStringLiteral("SrcID"), decimalText(static_cast<qulonglong>(flit.SrcID)), static_cast<qulonglong>(flit.SrcID));
        appendField(record, scope, QStringLiteral("TgtID"), decimalText(static_cast<qulonglong>(flit.TgtID)), static_cast<qulonglong>(flit.TgtID));
        if (measure.width.Opcode > 0) {
            appendField(record, scope, QStringLiteral("Opcode"), upDatOpcodeName(flit), static_cast<qulonglong>(flit.Opcode));
        }
        appendField(record, scope, QStringLiteral("RespErr"), decimalText(static_cast<qulonglong>(flit.RespErr)), static_cast<qulonglong>(flit.RespErr));
        appendField(record, scope, QStringLiteral("Resp"), respName(flit.Resp), static_cast<qulonglong>(flit.Resp));
        if (hasDataId) {
            appendField(record, scope, QStringLiteral("DataID"), decimalText(static_cast<qulonglong>(flit.DataID)), static_cast<qulonglong>(flit.DataID));
        }
        appendDataField(record, scope, flit.Data, measure.width.Data);
        appendField(record, scope, QStringLiteral("BE"), fixedHexText(static_cast<qulonglong>(flit.BE), measure.width.BE), static_cast<qulonglong>(flit.BE));
        appendField(record, scope, QStringLiteral("TraceTag"), decimalText(static_cast<qulonglong>(flit.TraceTag)), static_cast<qulonglong>(flit.TraceTag));
        return record;
    }

private:
    static QString decimalText(const qulonglong value)
    {
        return QString::number(value);
    }

    static QString hexText(const qulonglong value)
    {
        return QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper());
    }

    static QString fixedHexText(const qulonglong value, const std::size_t bitWidth)
    {
        const int digits = qMax(1, static_cast<int>((bitWidth + 3) / 4));
        return QStringLiteral("0x%1")
            .arg(QString::number(value, 16).toUpper().rightJustified(digits, QLatin1Char('0')));
    }

    static QString rawIntegralText(const qulonglong value)
    {
        return QStringLiteral("%1 (%2)").arg(hexText(value), QString::number(value));
    }

    static QString internDecimal(const qulonglong value)
    {
        return InternDisplayString(decimalText(value));
    }

    static QString labelValue(const QString& label, const QString& value)
    {
        if (value.isEmpty()) {
            return QString();
        }
        return QStringLiteral("%1 %2").arg(label, value);
    }

    static QString joinSummary(std::initializer_list<QString> parts)
    {
        QStringList filtered;
        for (const QString& part : parts) {
            if (!part.isEmpty()) {
                filtered.append(part);
            }
        }
        return filtered.join(QStringLiteral("  |  "));
    }

    static QString makeTxnLabel(const QString& txnId, const QString& dbid)
    {
        if (txnId.isEmpty() && dbid.isEmpty()) {
            return QString();
        }
        if (dbid.isEmpty()) {
            return QStringLiteral("Txn %1").arg(txnId);
        }
        return QStringLiteral("Txn %1 / DBID %2").arg(txnId, dbid);
    }

    static void fillOpcode(FlitRecord& record,
                           const QString& name,
                           const qulonglong rawValue,
                           const bool present)
    {
        if (!present) {
            return;
        }
        record.opcode = InternDisplayString(name);
        record.opcodeRaw = hexText(rawValue);
    }

    static void appendField(FlitRecord& record,
                            const QString* scope,
                            const QString& name,
                            const QString& value,
                            const qulonglong rawValue)
    {
        record.fields.push_back(FieldEntry{
            .scope = scope,
            .name = InternFieldText(name),
            .value = value,
            .raw = rawIntegralText(rawValue),
        });
    }

    static void appendDataField(FlitRecord& record,
                                const QString* scope,
                                const std::uint64_t* data,
                                const std::size_t dataWidth)
    {
        const std::size_t wordCount = dataWidth / 64;

        QString value;
        value.reserve(static_cast<int>(wordCount * 16));
        for (std::size_t index = wordCount; index-- > 0;) {
            value.append(QString::number(static_cast<qulonglong>(data[index]), 16)
                             .toUpper()
                             .rightJustified(16, QLatin1Char('0')));
        }

        QStringList fragments;
        const std::size_t previewCount = qMin<std::size_t>(wordCount, 4);
        for (std::size_t index = 0; index < previewCount; ++index) {
            fragments.append(QStringLiteral("[%1]=0x%2")
                                 .arg(index)
                                 .arg(QString::number(static_cast<qulonglong>(data[index]), 16)
                                          .toUpper()
                                          .rightJustified(16, QLatin1Char('0'))));
        }
        if (wordCount > previewCount) {
            fragments.append(QStringLiteral("..."));
        }

        record.fields.push_back(FieldEntry{
            .scope = scope,
            .name = InternFieldText(QStringLiteral("Data")),
            .value = value,
            .raw = fragments.join(QStringLiteral(", ")),
        });
    }

    static QString sizeName(const FlexibleCCHIReqFlit& flit)
    {
        return QString::fromLatin1(::CCHI::Sizes::ToEnum(flit.Size)->name);
    }

    static QString respName(const ::CCHI::Resps::type& resp)
    {
        return QString::fromLatin1(::CCHI::Resps::ToEnum(resp)->name);
    }

    static QString evtOpcodeName(const FlexibleCCHIEvtFlit& flit)
    {
        return QString::fromLatin1(
            ::CCHI::Opcodes::EVT::Decoder<FlexibleCCHIEvtFlit>::INSTANCE
                .Decode(flit.Opcode)
                .GetName("Unknown"));
    }

    static QString reqOpcodeName(const FlexibleCCHIReqFlit& flit)
    {
        return QString::fromLatin1(
            ::CCHI::Opcodes::REQ::Decoder<FlexibleCCHIReqFlit>::INSTANCE
                .Decode(flit.Opcode)
                .GetName("Unknown"));
    }

    static QString snpOpcodeName(const FlexibleCCHISnpFlit& flit)
    {
        return QString::fromLatin1(
            ::CCHI::Opcodes::SNP::Decoder<FlexibleCCHISnpFlit>::INSTANCE
                .Decode(flit.Opcode)
                .GetName("Unknown"));
    }

    static QString dnRspOpcodeName(const FlexibleCCHIDnRspFlit& flit)
    {
        return QString::fromLatin1(
            ::CCHI::Opcodes::DnRSP::Decoder<FlexibleCCHIDnRspFlit>::INSTANCE
                .Decode(flit.Opcode)
                .GetName("Unknown"));
    }

    static QString upRspOpcodeName(const FlexibleCCHIUpRspFlit& flit)
    {
        return QString::fromLatin1(
            ::CCHI::Opcodes::UpRSP::Decoder<FlexibleCCHIUpRspFlit>::INSTANCE
                .Decode(flit.Opcode)
                .GetName("Unknown"));
    }

    static QString dnDatOpcodeName(const FlexibleCCHIDnDatFlit& flit)
    {
        return QString::fromLatin1(
            ::CCHI::Opcodes::DnDAT::Decoder<FlexibleCCHIDnDatFlit>::INSTANCE
                .Decode(flit.Opcode)
                .GetName("Unknown"));
    }

    static QString upDatOpcodeName(const FlexibleCCHIUpDatFlit& flit)
    {
        return QString::fromLatin1(
            ::CCHI::Opcodes::UpDAT::Decoder<FlexibleCCHIUpDatFlit>::INSTANCE
                .Decode(flit.Opcode)
                .GetName("Unknown"));
    }

};


}  // namespace CHIron::Gui
