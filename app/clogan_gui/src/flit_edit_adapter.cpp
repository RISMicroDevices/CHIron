#include "flit_edit_adapter.hpp"

#include "flit_table_model.hpp"

#include "chi_b/spec/chi_b_protocol_encoding.hpp"
#include "chi_b/util/chi_b_util_decoding.hpp"
#include "chi_b/util/chi_b_util_flit.hpp"
#include "chi_eb/spec/chi_eb_protocol_encoding.hpp"
#include "chi_eb/util/chi_eb_util_decoding.hpp"
#include "chi_eb/util/chi_eb_util_flit.hpp"
#include "chi_eb/xact/chi_eb_xact_field.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace CHIron::Gui {
namespace {

using FlexibleEbConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 512, true, true, true>;
using FlexibleEbReqFlit = CHI::Eb::Flits::REQ<FlexibleEbConfig>;
using FlexibleEbRspFlit = CHI::Eb::Flits::RSP<FlexibleEbConfig>;
using FlexibleEbDatFlit = CHI::Eb::Flits::DAT<FlexibleEbConfig>;
using FlexibleEbSnpFlit = CHI::Eb::Flits::SNP<FlexibleEbConfig>;

using FlexibleBConfig = CHI::B::FlitConfiguration<11, 52, 32, 32, 512, true, true>;
using FlexibleBReqFlit = CHI::B::Flits::REQ<FlexibleBConfig>;
using FlexibleBRspFlit = CHI::B::Flits::RSP<FlexibleBConfig>;
using FlexibleBDatFlit = CHI::B::Flits::DAT<FlexibleBConfig>;
using FlexibleBSnpFlit = CHI::B::Flits::SNP<FlexibleBConfig>;

struct FieldLocation {
    bool present = false;
    std::size_t lsb = 0;
    std::size_t width = 0;
    CHI::Eb::Xact::FieldConvention convention = CHI::Eb::Xact::FieldConvention::Y;
};

struct Measures {
    CHI::Eb::Flits::REQMeasure ebReq;
    CHI::Eb::Flits::RSPMeasure ebRsp;
    CHI::Eb::Flits::DATMeasure ebDat;
    CHI::Eb::Flits::SNPMeasure ebSnp;
    CHI::B::Flits::REQMeasure bReq;
    CHI::B::Flits::RSPMeasure bRsp;
    CHI::B::Flits::DATMeasure bDat;
    CHI::B::Flits::SNPMeasure bSnp;
};

bool parseInteger(QString text, qulonglong& value)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return false;
    }

    bool ok = false;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = text.sliced(2).toULongLong(&ok, 16);
        return ok;
    }

    bool hasHexLetter = false;
    for (const QChar ch : text) {
        if (ch.isDigit()) {
            continue;
        }
        const ushort code = ch.unicode();
        if ((code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F')) {
            hasHexLetter = true;
            continue;
        }
        return false;
    }

    value = text.toULongLong(&ok, hasHexLetter ? 16 : 10);
    return ok;
}

bool parseRowOpcode(const FlitRecord& row, std::uint32_t& opcode)
{
    qulonglong value = 0;
    if (!parseInteger(row.opcodeRaw, value)) {
        return false;
    }
    opcode = static_cast<std::uint32_t>(value);
    return true;
}

bool evaluateMeasures(const CLog::Parameters& params, Measures& measures)
{
    CHI::Eb::Flits::REQMeasure::Parameters ebReqParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
        .reqRsvdcWidth = params.GetReqRSVDCWidth(),
        .mpamPresent = params.IsMPAMPresent(),
    };
    CHI::Eb::Flits::RSPMeasure::Parameters ebRspParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
    };
    CHI::Eb::Flits::DATMeasure::Parameters ebDatParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .dataWidth = params.GetDataWidth(),
        .datRsvdcWidth = params.GetDatRSVDCWidth(),
        .dataCheckPresent = params.IsDataCheckPresent(),
        .poisonPresent = params.IsPoisonPresent(),
    };
    CHI::Eb::Flits::SNPMeasure::Parameters ebSnpParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
        .mpamPresent = params.IsMPAMPresent(),
    };

    CHI::B::Flits::REQMeasure::Parameters bReqParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
        .reqRsvdcWidth = params.GetReqRSVDCWidth(),
    };
    CHI::B::Flits::RSPMeasure::Parameters bRspParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
    };
    CHI::B::Flits::DATMeasure::Parameters bDatParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .dataWidth = params.GetDataWidth(),
        .datRsvdcWidth = params.GetDatRSVDCWidth(),
        .dataCheckPresent = params.IsDataCheckPresent(),
        .poisonPresent = params.IsPoisonPresent(),
    };
    CHI::B::Flits::SNPMeasure::Parameters bSnpParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
    };

    return measures.ebReq.Eval(ebReqParams)
        && measures.ebRsp.Eval(ebRspParams)
        && measures.ebDat.Eval(ebDatParams)
        && measures.ebSnp.Eval(ebSnpParams)
        && measures.bReq.Eval(bReqParams)
        && measures.bRsp.Eval(bRspParams)
        && measures.bDat.Eval(bDatParams)
        && measures.bSnp.Eval(bSnpParams);
}

CLog::Channel rawChannelFor(const FlitRecord& row)
{
    if (row.rawRecordAvailable) {
        return static_cast<CLog::Channel>(row.rawChannel);
    }

    switch (row.channel) {
    case FlitChannel::Req:
        if (row.channelTag == QLatin1String("Before SAM")) {
            return row.direction == FlitDirection::Tx ? CLog::Channel::TXREQ_BeforeSAM
                                                      : CLog::Channel::RXREQ_BeforeSAM;
        }
        return row.direction == FlitDirection::Tx ? CLog::Channel::TXREQ
                                                  : CLog::Channel::RXREQ;
    case FlitChannel::Rsp:
        return row.direction == FlitDirection::Tx ? CLog::Channel::TXRSP
                                                  : CLog::Channel::RXRSP;
    case FlitChannel::Dat:
        return row.direction == FlitDirection::Tx ? CLog::Channel::TXDAT
                                                  : CLog::Channel::RXDAT;
    case FlitChannel::Snp:
        return row.direction == FlitDirection::Tx ? CLog::Channel::TXSNP
                                                  : CLog::Channel::RXSNP;
    }

    return CLog::Channel::TXREQ;
}

std::size_t expectedByteLength(const CLog::Parameters& params,
                               const Measures& measures,
                               const FlitChannel channel)
{
    const auto aligned = [](const std::size_t bits) {
        return (bits + 7U) / 8U;
    };

    switch (params.GetIssue()) {
    case CLog::Issue::Eb:
        switch (channel) {
        case FlitChannel::Req:
            return aligned(measures.ebReq.width._);
        case FlitChannel::Rsp:
            return aligned(measures.ebRsp.width._);
        case FlitChannel::Dat:
            return aligned(measures.ebDat.width._);
        case FlitChannel::Snp:
            return aligned(measures.ebSnp.width._);
        }
        break;
    case CLog::Issue::B:
        switch (channel) {
        case FlitChannel::Req:
            return aligned(measures.bReq.width._);
        case FlitChannel::Rsp:
            return aligned(measures.bRsp.width._);
        case FlitChannel::Dat:
            return aligned(measures.bDat.width._);
        case FlitChannel::Snp:
            return aligned(measures.bSnp.width._);
        }
        break;
    }
    return 0;
}

FieldLocation makeLocation(const std::size_t lsb,
                           const std::size_t width,
                           const CHI::Eb::Xact::FieldConvention convention = CHI::Eb::Xact::FieldConvention::Y)
{
    return FieldLocation{
        .present = width > 0,
        .lsb = lsb,
        .width = width,
        .convention = convention,
    };
}

struct MeasuredFields {
    std::size_t QoS = 0;
    std::size_t TgtID = 0;
    std::size_t SrcID = 0;
    std::size_t TxnID = 0;
    std::size_t ReturnNID = 0;
    std::size_t StashNIDValid = 0;
    std::size_t ReturnTxnID = 0;
    std::size_t Opcode = 0;
    std::size_t Size = 0;
    std::size_t Addr = 0;
    std::size_t NS = 0;
    std::size_t LikelyShared = 0;
    std::size_t AllowRetry = 0;
    std::size_t Order = 0;
    std::size_t PCrdType = 0;
    std::size_t MemAttr = 0;
    std::size_t SnpAttr = 0;
    std::size_t TagGroupID = 0;
    std::size_t LPID = 0;
    std::size_t Excl = 0;
    std::size_t ExpCompAck = 0;
    std::size_t TagOp = 0;
    std::size_t TraceTag = 0;
    std::size_t MPAM = 0;
    std::size_t RSVDC = 0;
    std::size_t RespErr = 0;
    std::size_t Resp = 0;
    std::size_t FwdState = 0;
    std::size_t CBusy = 0;
    std::size_t DBID = 0;
    std::size_t CCID = 0;
    std::size_t DataID = 0;
    std::size_t HomeNID = 0;
    std::size_t DataSource = 0;
    std::size_t Tag = 0;
    std::size_t TU = 0;
    std::size_t BE = 0;
    std::size_t Data = 0;
    std::size_t DataCheck = 0;
    std::size_t Poison = 0;
    std::size_t FwdNID = 0;
    std::size_t FwdTxnID = 0;
    std::size_t DoNotGoToSD = 0;
    std::size_t RetToSrc = 0;
};

template<typename Tfields>
MeasuredFields measuredFields(const Tfields& fields)
{
    MeasuredFields out;
    if constexpr (requires { fields.QoS; }) out.QoS = fields.QoS;
    if constexpr (requires { fields.TgtID; }) out.TgtID = fields.TgtID;
    if constexpr (requires { fields.SrcID; }) out.SrcID = fields.SrcID;
    if constexpr (requires { fields.TxnID; }) out.TxnID = fields.TxnID;
    if constexpr (requires { fields.ReturnNID; }) out.ReturnNID = fields.ReturnNID;
    if constexpr (requires { fields.StashNIDValid; }) out.StashNIDValid = fields.StashNIDValid;
    if constexpr (requires { fields.ReturnTxnID; }) out.ReturnTxnID = fields.ReturnTxnID;
    if constexpr (requires { fields.Opcode; }) out.Opcode = fields.Opcode;
    if constexpr (requires { fields.Size; }) out.Size = fields.Size;
    if constexpr (requires { fields.Addr; }) out.Addr = fields.Addr;
    if constexpr (requires { fields.NS; }) out.NS = fields.NS;
    if constexpr (requires { fields.LikelyShared; }) out.LikelyShared = fields.LikelyShared;
    if constexpr (requires { fields.AllowRetry; }) out.AllowRetry = fields.AllowRetry;
    if constexpr (requires { fields.Order; }) out.Order = fields.Order;
    if constexpr (requires { fields.PCrdType; }) out.PCrdType = fields.PCrdType;
    if constexpr (requires { fields.MemAttr; }) out.MemAttr = fields.MemAttr;
    if constexpr (requires { fields.SnpAttr; }) out.SnpAttr = fields.SnpAttr;
    if constexpr (requires { fields.TagGroupID; }) out.TagGroupID = fields.TagGroupID;
    if constexpr (requires { fields.LPID; }) out.LPID = fields.LPID;
    if constexpr (requires { fields.Excl; }) out.Excl = fields.Excl;
    if constexpr (requires { fields.ExpCompAck; }) out.ExpCompAck = fields.ExpCompAck;
    if constexpr (requires { fields.TagOp; }) out.TagOp = fields.TagOp;
    if constexpr (requires { fields.TraceTag; }) out.TraceTag = fields.TraceTag;
    if constexpr (requires { fields.MPAM; }) out.MPAM = fields.MPAM;
    if constexpr (requires { fields.RSVDC; }) out.RSVDC = fields.RSVDC;
    if constexpr (requires { fields.RespErr; }) out.RespErr = fields.RespErr;
    if constexpr (requires { fields.Resp; }) out.Resp = fields.Resp;
    if constexpr (requires { fields.FwdState; }) out.FwdState = fields.FwdState;
    if constexpr (requires { fields.CBusy; }) out.CBusy = fields.CBusy;
    if constexpr (requires { fields.DBID; }) out.DBID = fields.DBID;
    if constexpr (requires { fields.CCID; }) out.CCID = fields.CCID;
    if constexpr (requires { fields.DataID; }) out.DataID = fields.DataID;
    if constexpr (requires { fields.HomeNID; }) out.HomeNID = fields.HomeNID;
    if constexpr (requires { fields.DataSource; }) out.DataSource = fields.DataSource;
    if constexpr (requires { fields.Tag; }) out.Tag = fields.Tag;
    if constexpr (requires { fields.TU; }) out.TU = fields.TU;
    if constexpr (requires { fields.BE; }) out.BE = fields.BE;
    if constexpr (requires { fields.Data; }) out.Data = fields.Data;
    if constexpr (requires { fields.DataCheck; }) out.DataCheck = fields.DataCheck;
    if constexpr (requires { fields.Poison; }) out.Poison = fields.Poison;
    if constexpr (requires { fields.FwdNID; }) out.FwdNID = fields.FwdNID;
    if constexpr (requires { fields.FwdTxnID; }) out.FwdTxnID = fields.FwdTxnID;
    if constexpr (requires { fields.DoNotGoToSD; }) out.DoNotGoToSD = fields.DoNotGoToSD;
    if constexpr (requires { fields.RetToSrc; }) out.RetToSrc = fields.RetToSrc;
    return out;
}

template<typename MappingT>
CHI::Eb::Xact::FieldConvention conventionOrY(const MappingT* mapping,
                                             const QString& fieldName)
{
    using FC = CHI::Eb::Xact::FieldConvention;
    if (!mapping) {
        return FC::Y;
    }

    if constexpr (requires { mapping->QoS; }) {
        if (fieldName == QLatin1String("QoS")) return mapping->QoS;
    }
    if constexpr (requires { mapping->TgtID; }) {
        if (fieldName == QLatin1String("TgtID")) return mapping->TgtID;
    }
    if constexpr (requires { mapping->SrcID; }) {
        if (fieldName == QLatin1String("SrcID")) return mapping->SrcID;
    }
    if constexpr (requires { mapping->TxnID; }) {
        if (fieldName == QLatin1String("TxnID")) return mapping->TxnID;
    }
    if constexpr (requires { mapping->Opcode; }) {
        if (fieldName == QLatin1String("Opcode")) return mapping->Opcode;
    }
    if constexpr (requires { mapping->AllowRetry; }) {
        if (fieldName == QLatin1String("AllowRetry")) return mapping->AllowRetry;
    }
    if constexpr (requires { mapping->PCrdType; }) {
        if (fieldName == QLatin1String("PCrdType")) return mapping->PCrdType;
    }
    if constexpr (requires { mapping->RSVDC; }) {
        if (fieldName == QLatin1String("RSVDC")) return mapping->RSVDC;
    }
    if constexpr (requires { mapping->TagOp; }) {
        if (fieldName == QLatin1String("TagOp")) return mapping->TagOp;
    }
    if constexpr (requires { mapping->TraceTag; }) {
        if (fieldName == QLatin1String("TraceTag")) return mapping->TraceTag;
    }
    if constexpr (requires { mapping->MPAM; }) {
        if (fieldName == QLatin1String("MPAM")) return mapping->MPAM;
    }
    if constexpr (requires { mapping->Addr; }) {
        if (fieldName == QLatin1String("Addr")) return mapping->Addr;
    }
    if constexpr (requires { mapping->NS; }) {
        if (fieldName == QLatin1String("NS")) return mapping->NS;
    }
    if constexpr (requires { mapping->Size; }) {
        if (fieldName == QLatin1String("Size")) return mapping->Size;
    }
    if constexpr (requires { mapping->Allocate; }) {
        if (fieldName == QLatin1String("Allocate")) return mapping->Allocate;
    }
    if constexpr (requires { mapping->Cacheable; }) {
        if (fieldName == QLatin1String("Cacheable")) return mapping->Cacheable;
    }
    if constexpr (requires { mapping->Device; }) {
        if (fieldName == QLatin1String("Device")) return mapping->Device;
    }
    if constexpr (requires { mapping->EWA; }) {
        if (fieldName == QLatin1String("EWA")) return mapping->EWA;
    }
    if constexpr (requires { mapping->SnpAttr; }) {
        if (fieldName == QLatin1String("SnpAttr")) return mapping->SnpAttr;
    }
    if constexpr (requires { mapping->Order; }) {
        if (fieldName == QLatin1String("Order")) return mapping->Order;
    }
    if constexpr (requires { mapping->LikelyShared; }) {
        if (fieldName == QLatin1String("LikelyShared")) return mapping->LikelyShared;
    }
    if constexpr (requires { mapping->Excl; }) {
        if (fieldName == QLatin1String("Excl")) return mapping->Excl;
    }
    if constexpr (requires { mapping->ExpCompAck; }) {
        if (fieldName == QLatin1String("ExpCompAck")) return mapping->ExpCompAck;
    }
    if constexpr (requires { mapping->LPID; }) {
        if (fieldName == QLatin1String("LPID")) return mapping->LPID;
    }
    if constexpr (requires { mapping->TagGroupID; }) {
        if (fieldName == QLatin1String("TagGroupID")) return mapping->TagGroupID;
    }
    if constexpr (requires { mapping->ReturnNID; }) {
        if (fieldName == QLatin1String("ReturnNID")) return mapping->ReturnNID;
    }
    if constexpr (requires { mapping->StashNIDValid; }) {
        if (fieldName == QLatin1String("StashNIDValid")) return mapping->StashNIDValid;
    }
    if constexpr (requires { mapping->ReturnTxnID; }) {
        if (fieldName == QLatin1String("ReturnTxnID")) return mapping->ReturnTxnID;
    }
    if constexpr (requires { mapping->RespErr; }) {
        if (fieldName == QLatin1String("RespErr")) return mapping->RespErr;
    }
    if constexpr (requires { mapping->Resp; }) {
        if (fieldName == QLatin1String("Resp")) return mapping->Resp;
    }
    if constexpr (requires { mapping->CBusy; }) {
        if (fieldName == QLatin1String("CBusy")) return mapping->CBusy;
    }
    if constexpr (requires { mapping->DBID; }) {
        if (fieldName == QLatin1String("DBID")) return mapping->DBID;
    }
    if constexpr (requires { mapping->CCID; }) {
        if (fieldName == QLatin1String("CCID")) return mapping->CCID;
    }
    if constexpr (requires { mapping->DataID; }) {
        if (fieldName == QLatin1String("DataID")) return mapping->DataID;
    }
    if constexpr (requires { mapping->BE; }) {
        if (fieldName == QLatin1String("BE")) return mapping->BE;
    }
    if constexpr (requires { mapping->Data; }) {
        if (fieldName == QLatin1String("Data")) return mapping->Data;
    }
    if constexpr (requires { mapping->HomeNID; }) {
        if (fieldName == QLatin1String("HomeNID")) return mapping->HomeNID;
    }
    if constexpr (requires { mapping->FwdState; }) {
        if (fieldName == QLatin1String("FwdState")) return mapping->FwdState;
    }
    if constexpr (requires { mapping->DataSource; }) {
        if (fieldName == QLatin1String("DataSource")) return mapping->DataSource;
    }
    if constexpr (requires { mapping->DataCheck; }) {
        if (fieldName == QLatin1String("DataCheck")) return mapping->DataCheck;
    }
    if constexpr (requires { mapping->Poison; }) {
        if (fieldName == QLatin1String("Poison")) return mapping->Poison;
    }
    if constexpr (requires { mapping->Tag; }) {
        if (fieldName == QLatin1String("Tag")) return mapping->Tag;
    }
    if constexpr (requires { mapping->TU; }) {
        if (fieldName == QLatin1String("TU")) return mapping->TU;
    }
    if constexpr (requires { mapping->FwdNID; }) {
        if (fieldName == QLatin1String("FwdNID")) return mapping->FwdNID;
    }
    if constexpr (requires { mapping->FwdTxnID; }) {
        if (fieldName == QLatin1String("FwdTxnID")) return mapping->FwdTxnID;
    }
    if constexpr (requires { mapping->DoNotGoToSD; }) {
        if (fieldName == QLatin1String("DoNotGoToSD")) return mapping->DoNotGoToSD;
    }
    if constexpr (requires { mapping->RetToSrc; }) {
        if (fieldName == QLatin1String("RetToSrc")) return mapping->RetToSrc;
    }

    return FC::Y;
}

FieldLocation fieldLocation(const FlitRecord& row,
                            const CLogBTraceMetadata* metadata,
                            const Measures& measures,
                            const QString& fieldName)
{
    if (!metadata) {
        return {};
    }

    std::uint32_t opcode = 0;
    parseRowOpcode(row, opcode);
    const bool eb = metadata->parameters.GetIssue() == CLog::Issue::Eb;
    using FC = CHI::Eb::Xact::FieldConvention;
    const FC y = FC::Y;

    const auto reqConvention = [&](const QString& name) {
        if (!eb) return y;
        static const CHI::Eb::Xact::RequestFieldMappingTable table;
        return conventionOrY(table.Get(static_cast<CHI::Eb::Flits::REQ<>::opcode_t>(opcode)), name);
    };
    const auto rspConvention = [&](const QString& name) {
        if (!eb) return y;
        static const CHI::Eb::Xact::ResponseFieldMappingTable table;
        return conventionOrY(table.Get(static_cast<CHI::Eb::Flits::RSP<>::opcode_t>(opcode)), name);
    };
    const auto datConvention = [&](const QString& name) {
        if (!eb) return y;
        static const CHI::Eb::Xact::DataFieldMappingTable table;
        return conventionOrY(table.Get(static_cast<CHI::Eb::Flits::DAT<>::opcode_t>(opcode)), name);
    };
    const auto snpConvention = [&](const QString& name) {
        if (!eb) return y;
        static const CHI::Eb::Xact::SnoopFieldMappingTable table;
        return conventionOrY(table.Get(static_cast<CHI::Eb::Flits::SNP<>::opcode_t>(opcode)), name);
    };

    switch (row.channel) {
    case FlitChannel::Req: {
        const MeasuredFields width = eb ? measuredFields(measures.ebReq.width)
                                        : measuredFields(measures.bReq.width);
        const MeasuredFields lsb = eb ? measuredFields(measures.ebReq.lsb)
                                      : measuredFields(measures.bReq.lsb);
        const auto conv = reqConvention(fieldName);
        if (fieldName == QLatin1String("QoS")) return makeLocation(lsb.QoS, width.QoS, conv);
        if (fieldName == QLatin1String("TgtID")) return makeLocation(lsb.TgtID, width.TgtID, conv);
        if (fieldName == QLatin1String("SrcID")) return makeLocation(lsb.SrcID, width.SrcID, conv);
        if (fieldName == QLatin1String("TxnID")) return makeLocation(lsb.TxnID, width.TxnID, conv);
        if (fieldName == QLatin1String("ReturnNID")) return makeLocation(lsb.ReturnNID, width.ReturnNID, conv);
        if (fieldName == QLatin1String("StashNIDValid")) return makeLocation(lsb.StashNIDValid, width.StashNIDValid, conv);
        if (fieldName == QLatin1String("ReturnTxnID")) return makeLocation(lsb.ReturnTxnID, width.ReturnTxnID, conv);
        if (fieldName == QLatin1String("Opcode")) return makeLocation(lsb.Opcode, width.Opcode, conv);
        if (fieldName == QLatin1String("Size")) return makeLocation(lsb.Size, width.Size, conv);
        if (fieldName == QLatin1String("Addr")) return makeLocation(lsb.Addr, width.Addr, conv);
        if (fieldName == QLatin1String("NS")) return makeLocation(lsb.NS, width.NS, conv);
        if (fieldName == QLatin1String("LikelyShared")) return makeLocation(lsb.LikelyShared, width.LikelyShared, conv);
        if (fieldName == QLatin1String("AllowRetry")) return makeLocation(lsb.AllowRetry, width.AllowRetry, conv);
        if (fieldName == QLatin1String("Order")) return makeLocation(lsb.Order, width.Order, conv);
        if (fieldName == QLatin1String("PCrdType")) return makeLocation(lsb.PCrdType, width.PCrdType, conv);
        if (fieldName == QLatin1String("MemAttr")) return makeLocation(lsb.MemAttr, width.MemAttr, conv);
        if (fieldName == QLatin1String("Allocate")) return makeLocation(lsb.MemAttr + 3, 1, conv);
        if (fieldName == QLatin1String("Cacheable")) return makeLocation(lsb.MemAttr + 2, 1, conv);
        if (fieldName == QLatin1String("Device")) return makeLocation(lsb.MemAttr + 1, 1, conv);
        if (fieldName == QLatin1String("EWA")) return makeLocation(lsb.MemAttr, 1, conv);
        if (fieldName == QLatin1String("SnpAttr")) return makeLocation(lsb.SnpAttr, width.SnpAttr, conv);
        if (eb && fieldName == QLatin1String("TagGroupID")) return makeLocation(lsb.TagGroupID, width.TagGroupID, conv);
        if (!eb && fieldName == QLatin1String("LPID")) return makeLocation(lsb.LPID, width.LPID, conv);
        if (fieldName == QLatin1String("Excl")) return makeLocation(lsb.Excl, width.Excl, conv);
        if (fieldName == QLatin1String("ExpCompAck")) return makeLocation(lsb.ExpCompAck, width.ExpCompAck, conv);
        if (eb && fieldName == QLatin1String("TagOp")) return makeLocation(lsb.TagOp, width.TagOp, conv);
        if (fieldName == QLatin1String("TraceTag")) return makeLocation(lsb.TraceTag, width.TraceTag, conv);
        if (eb && fieldName == QLatin1String("MPAM")) return makeLocation(lsb.MPAM, width.MPAM, conv);
        if (fieldName == QLatin1String("RSVDC")) return makeLocation(lsb.RSVDC, width.RSVDC, conv);
        break;
    }
    case FlitChannel::Rsp: {
        const MeasuredFields width = eb ? measuredFields(measures.ebRsp.width)
                                        : measuredFields(measures.bRsp.width);
        const MeasuredFields lsb = eb ? measuredFields(measures.ebRsp.lsb)
                                      : measuredFields(measures.bRsp.lsb);
        const auto conv = rspConvention(fieldName);
        if (fieldName == QLatin1String("QoS")) return makeLocation(lsb.QoS, width.QoS, conv);
        if (fieldName == QLatin1String("TgtID")) return makeLocation(lsb.TgtID, width.TgtID, conv);
        if (fieldName == QLatin1String("SrcID")) return makeLocation(lsb.SrcID, width.SrcID, conv);
        if (fieldName == QLatin1String("TxnID")) return makeLocation(lsb.TxnID, width.TxnID, conv);
        if (fieldName == QLatin1String("Opcode")) return makeLocation(lsb.Opcode, width.Opcode, conv);
        if (fieldName == QLatin1String("RespErr")) return makeLocation(lsb.RespErr, width.RespErr, conv);
        if (fieldName == QLatin1String("Resp")) return makeLocation(lsb.Resp, width.Resp, conv);
        if (fieldName == QLatin1String("FwdState")) return makeLocation(lsb.FwdState, width.FwdState, conv);
        if (eb && fieldName == QLatin1String("CBusy")) return makeLocation(lsb.CBusy, width.CBusy, conv);
        if (fieldName == QLatin1String("DBID")) return makeLocation(lsb.DBID, width.DBID, conv);
        if (fieldName == QLatin1String("PCrdType")) return makeLocation(lsb.PCrdType, width.PCrdType, conv);
        if (eb && fieldName == QLatin1String("TagOp")) return makeLocation(lsb.TagOp, width.TagOp, conv);
        if (fieldName == QLatin1String("TraceTag")) return makeLocation(lsb.TraceTag, width.TraceTag, conv);
        break;
    }
    case FlitChannel::Dat: {
        const MeasuredFields width = eb ? measuredFields(measures.ebDat.width)
                                        : measuredFields(measures.bDat.width);
        const MeasuredFields lsb = eb ? measuredFields(measures.ebDat.lsb)
                                      : measuredFields(measures.bDat.lsb);
        const auto conv = datConvention(fieldName);
        if (fieldName == QLatin1String("QoS")) return makeLocation(lsb.QoS, width.QoS, conv);
        if (fieldName == QLatin1String("TgtID")) return makeLocation(lsb.TgtID, width.TgtID, conv);
        if (fieldName == QLatin1String("SrcID")) return makeLocation(lsb.SrcID, width.SrcID, conv);
        if (fieldName == QLatin1String("TxnID")) return makeLocation(lsb.TxnID, width.TxnID, conv);
        if (fieldName == QLatin1String("HomeNID")) return makeLocation(lsb.HomeNID, width.HomeNID, conv);
        if (fieldName == QLatin1String("Opcode")) return makeLocation(lsb.Opcode, width.Opcode, conv);
        if (fieldName == QLatin1String("RespErr")) return makeLocation(lsb.RespErr, width.RespErr, conv);
        if (fieldName == QLatin1String("Resp")) return makeLocation(lsb.Resp, width.Resp, conv);
        if (fieldName == QLatin1String("FwdState")) return makeLocation(lsb.FwdState, width.FwdState, conv);
        if (fieldName == QLatin1String("DataSource")) return makeLocation(lsb.DataSource, width.DataSource, conv);
        if (eb && fieldName == QLatin1String("CBusy")) return makeLocation(lsb.CBusy, width.CBusy, conv);
        if (fieldName == QLatin1String("DBID")) return makeLocation(lsb.DBID, width.DBID, conv);
        if (fieldName == QLatin1String("CCID")) return makeLocation(lsb.CCID, width.CCID, conv);
        if (fieldName == QLatin1String("DataID")) return makeLocation(lsb.DataID, width.DataID, conv);
        if (eb && fieldName == QLatin1String("TagOp")) return makeLocation(lsb.TagOp, width.TagOp, conv);
        if (eb && fieldName == QLatin1String("Tag")) return makeLocation(lsb.Tag, width.Tag, conv);
        if (eb && fieldName == QLatin1String("TU")) return makeLocation(lsb.TU, width.TU, conv);
        if (fieldName == QLatin1String("TraceTag")) return makeLocation(lsb.TraceTag, width.TraceTag, conv);
        if (fieldName == QLatin1String("RSVDC")) return makeLocation(lsb.RSVDC, width.RSVDC, conv);
        if (fieldName == QLatin1String("BE")) return makeLocation(lsb.BE, width.BE, conv);
        if (fieldName == QLatin1String("Data")) return makeLocation(lsb.Data, width.Data, conv);
        if (fieldName == QLatin1String("DataCheck")) return makeLocation(lsb.DataCheck, width.DataCheck, conv);
        if (fieldName == QLatin1String("Poison")) return makeLocation(lsb.Poison, width.Poison, conv);
        break;
    }
    case FlitChannel::Snp: {
        const MeasuredFields width = eb ? measuredFields(measures.ebSnp.width)
                                        : measuredFields(measures.bSnp.width);
        const MeasuredFields lsb = eb ? measuredFields(measures.ebSnp.lsb)
                                      : measuredFields(measures.bSnp.lsb);
        const auto conv = snpConvention(fieldName);
        if (fieldName == QLatin1String("QoS")) return makeLocation(lsb.QoS, width.QoS, conv);
        if (fieldName == QLatin1String("SrcID")) return makeLocation(lsb.SrcID, width.SrcID, conv);
        if (fieldName == QLatin1String("TxnID")) return makeLocation(lsb.TxnID, width.TxnID, conv);
        if (fieldName == QLatin1String("FwdNID")) return makeLocation(lsb.FwdNID, width.FwdNID, conv);
        if (fieldName == QLatin1String("FwdTxnID")) return makeLocation(lsb.FwdTxnID, width.FwdTxnID, conv);
        if (fieldName == QLatin1String("Opcode")) return makeLocation(lsb.Opcode, width.Opcode, conv);
        if (fieldName == QLatin1String("Addr")) return makeLocation(lsb.Addr, width.Addr, conv);
        if (fieldName == QLatin1String("NS")) return makeLocation(lsb.NS, width.NS, conv);
        if (fieldName == QLatin1String("DoNotGoToSD")) return makeLocation(lsb.DoNotGoToSD, width.DoNotGoToSD, conv);
        if (fieldName == QLatin1String("RetToSrc")) return makeLocation(lsb.RetToSrc, width.RetToSrc, conv);
        if (fieldName == QLatin1String("TraceTag")) return makeLocation(lsb.TraceTag, width.TraceTag, conv);
        if (eb && fieldName == QLatin1String("MPAM")) return makeLocation(lsb.MPAM, width.MPAM, conv);
        break;
    }
    }

    return {};
}

bool writeBits(std::vector<std::uint32_t>& words,
               const std::size_t lsb,
               const std::size_t width,
               const qulonglong value)
{
    if (width == 0 || width > 64) {
        return false;
    }
    if (width < 64 && (value >> width) != 0) {
        return false;
    }

    for (std::size_t bit = 0; bit < width; ++bit) {
        const std::size_t targetBit = lsb + bit;
        const std::size_t wordIndex = targetBit / 32U;
        if (wordIndex >= words.size()) {
            return false;
        }
        const std::uint32_t mask = std::uint32_t{1} << (targetBit % 32U);
        if ((value >> bit) & 1U) {
            words[wordIndex] |= mask;
        } else {
            words[wordIndex] &= ~mask;
        }
    }
    return true;
}

template<typename DecoderT>
bool parseOpcodeByName(const QString& text, std::uint32_t& opcode)
{
    const DecoderT& decoder = DecoderT::INSTANCE;
    auto iterator = decoder.Iterate();
    while (true) {
        const auto& info = iterator.Next();
        if (!info.IsValid()) {
            break;
        }
        const QString name = QString::fromLatin1(info.GetName("Unknown"));
        if (name.compare(text, Qt::CaseInsensitive) == 0) {
            opcode = static_cast<std::uint32_t>(info.GetOpcode());
            return true;
        }
    }
    return false;
}

template<typename DecoderT>
bool opcodeIsValid(const qulonglong opcode)
{
    const DecoderT& decoder = DecoderT::INSTANCE;
    return decoder.Decode(static_cast<typename DecoderT::opcode_t>(opcode)).IsValid();
}

bool parseOpcodeValue(const FlitRecord& row,
                      const QString& text,
                      qulonglong& value)
{
    if (parseInteger(text, value)) {
        switch (row.channel) {
        case FlitChannel::Req:
            if (opcodeIsValid<CHI::Eb::Opcodes::REQ::Decoder<FlexibleEbReqFlit>>(value)
                || opcodeIsValid<CHI::B::Opcodes::REQ::Decoder<FlexibleBReqFlit>>(value)) return true;
            break;
        case FlitChannel::Rsp:
            if (opcodeIsValid<CHI::Eb::Opcodes::RSP::Decoder<FlexibleEbRspFlit>>(value)
                || opcodeIsValid<CHI::B::Opcodes::RSP::Decoder<FlexibleBRspFlit>>(value)) return true;
            break;
        case FlitChannel::Dat:
            if (opcodeIsValid<CHI::Eb::Opcodes::DAT::Decoder<FlexibleEbDatFlit>>(value)
                || opcodeIsValid<CHI::B::Opcodes::DAT::Decoder<FlexibleBDatFlit>>(value)) return true;
            break;
        case FlitChannel::Snp:
            if (opcodeIsValid<CHI::Eb::Opcodes::SNP::Decoder<FlexibleEbSnpFlit>>(value)
                || opcodeIsValid<CHI::B::Opcodes::SNP::Decoder<FlexibleBSnpFlit>>(value)) return true;
            break;
        }
        return false;
    }

    std::uint32_t opcode = 0;
    switch (row.channel) {
    case FlitChannel::Req:
        if (!parseOpcodeByName<CHI::Eb::Opcodes::REQ::Decoder<FlexibleEbReqFlit>>(text, opcode)
            && !parseOpcodeByName<CHI::B::Opcodes::REQ::Decoder<FlexibleBReqFlit>>(text, opcode)) return false;
        break;
    case FlitChannel::Rsp:
        if (!parseOpcodeByName<CHI::Eb::Opcodes::RSP::Decoder<FlexibleEbRspFlit>>(text, opcode)
            && !parseOpcodeByName<CHI::B::Opcodes::RSP::Decoder<FlexibleBRspFlit>>(text, opcode)) return false;
        break;
    case FlitChannel::Dat:
        if (!parseOpcodeByName<CHI::Eb::Opcodes::DAT::Decoder<FlexibleEbDatFlit>>(text, opcode)
            && !parseOpcodeByName<CHI::B::Opcodes::DAT::Decoder<FlexibleBDatFlit>>(text, opcode)) return false;
        break;
    case FlitChannel::Snp:
        if (!parseOpcodeByName<CHI::Eb::Opcodes::SNP::Decoder<FlexibleEbSnpFlit>>(text, opcode)
            && !parseOpcodeByName<CHI::B::Opcodes::SNP::Decoder<FlexibleBSnpFlit>>(text, opcode)) return false;
        break;
    }

    value = opcode;
    return true;
}

template<typename EnumFn>
bool parseEnumValue(const QString& text,
                    const std::size_t width,
                    EnumFn&& enumFn,
                    qulonglong& value)
{
    if (parseInteger(text, value)) {
        return true;
    }

    const std::size_t limit = width >= 12 ? 0 : (std::size_t{1} << width);
    for (std::size_t candidate = 0; candidate < limit; ++candidate) {
        const auto* info = enumFn(static_cast<unsigned int>(candidate));
        if (!info || !info->IsValid()) {
            continue;
        }
        const QString name = QString::fromLatin1(info->name);
        if (name.compare(text, Qt::CaseInsensitive) == 0) {
            value = static_cast<qulonglong>(candidate);
            return true;
        }
    }
    return false;
}

bool parseValueForField(const FlitRecord& row,
                        const QString& fieldName,
                        const FieldLocation& location,
                        QString text,
                        qulonglong& value)
{
    text = text.trimmed();
    if (fieldName == QLatin1String("Opcode")) {
        return parseOpcodeValue(row, text, value);
    }
    if (fieldName == QLatin1String("Resp")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::Resps::ToEnum(CHI::Eb::Resp(candidate));
        }, value);
    }
    if (fieldName == QLatin1String("RespErr")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::RespErrs::ToEnum(CHI::Eb::RespErr(candidate));
        }, value);
    }
    if (fieldName == QLatin1String("FwdState")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::FwdStates::ToEnum(CHI::Eb::FwdState(candidate));
        }, value);
    }
    if (fieldName == QLatin1String("NS")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::NSs::ToEnum(CHI::Eb::NS(candidate));
        }, value);
    }
    if (fieldName == QLatin1String("Size")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::Sizes::ToEnum(CHI::Eb::Size(candidate));
        }, value);
    }
    if (fieldName == QLatin1String("Order")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::Orders::ToEnum(CHI::Eb::Order(candidate));
        }, value);
    }
    if (fieldName == QLatin1String("SnpAttr")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::SnpAttrs::ToEnum(CHI::Eb::SnpAttr(candidate));
        }, value);
    }
    if (fieldName == QLatin1String("MemAttr")) {
        return parseEnumValue(text, location.width, [](const unsigned int candidate) {
            return CHI::Eb::MemAttrs::ToEnum(CHI::Eb::MemAttr(candidate));
        }, value);
    }

    return parseInteger(text, value);
}

void clearDerivedState(FlitRecord& row)
{
    row.transactionGroupKey.clear();
    row.xactionDebugLog.clear();
    row.xactionIndexChecked = false;
    row.xactionIndexed = false;
    row.xactionIndexProcessedByJoint = false;
    row.xactionIndexState = XactionIndexState::NotStarted;
}

bool fixedConventionAllows(const CHI::Eb::Xact::FieldConvention convention,
                           const qulonglong value)
{
    using FC = CHI::Eb::Xact::FieldConvention;
    switch (convention) {
    case FC::A0:
    case FC::I0:
        return value == 0;
    case FC::A1:
        return value == 1;
    case FC::B8:
        return value == CHI::Eb::Sizes::B8;
    case FC::B64:
        return value == CHI::Eb::Sizes::B64;
    case FC::D:
        return value == 0;
    case FC::X:
    case FC::Y:
    case FC::S:
        return true;
    }
    return true;
}

bool fixedConventionValue(const CHI::Eb::Xact::FieldConvention convention,
                          qulonglong& value)
{
    using FC = CHI::Eb::Xact::FieldConvention;
    switch (convention) {
    case FC::A0:
    case FC::I0:
    case FC::D:
        value = 0;
        return true;
    case FC::A1:
        value = 1;
        return true;
    case FC::B8:
        value = CHI::Eb::Sizes::B8;
        return true;
    case FC::B64:
        value = CHI::Eb::Sizes::B64;
        return true;
    case FC::X:
    case FC::Y:
    case FC::S:
        return false;
    }
    return false;
}

bool isFixedConvention(const CHI::Eb::Xact::FieldConvention convention)
{
    using FC = CHI::Eb::Xact::FieldConvention;
    return convention == FC::A0
        || convention == FC::A1
        || convention == FC::B8
        || convention == FC::B64
        || convention == FC::D
        || convention == FC::I0;
}

const QStringList& templateFieldNamesStorage()
{
    static const QStringList kFieldNames{
        QStringLiteral("QoS"),
        QStringLiteral("TgtID"),
        QStringLiteral("SrcID"),
        QStringLiteral("TxnID"),
        QStringLiteral("ReturnNID"),
        QStringLiteral("StashNIDValid"),
        QStringLiteral("ReturnTxnID"),
        QStringLiteral("Opcode"),
        QStringLiteral("Size"),
        QStringLiteral("Addr"),
        QStringLiteral("NS"),
        QStringLiteral("LikelyShared"),
        QStringLiteral("AllowRetry"),
        QStringLiteral("Order"),
        QStringLiteral("PCrdType"),
        QStringLiteral("MemAttr"),
        QStringLiteral("SnpAttr"),
        QStringLiteral("TagGroupID"),
        QStringLiteral("LPID"),
        QStringLiteral("Excl"),
        QStringLiteral("ExpCompAck"),
        QStringLiteral("TagOp"),
        QStringLiteral("TraceTag"),
        QStringLiteral("MPAM"),
        QStringLiteral("RSVDC"),
        QStringLiteral("RespErr"),
        QStringLiteral("Resp"),
        QStringLiteral("FwdState"),
        QStringLiteral("CBusy"),
        QStringLiteral("DBID"),
        QStringLiteral("CCID"),
        QStringLiteral("DataID"),
        QStringLiteral("HomeNID"),
        QStringLiteral("DataSource"),
        QStringLiteral("Tag"),
        QStringLiteral("TU"),
        QStringLiteral("BE"),
        QStringLiteral("Data"),
        QStringLiteral("DataCheck"),
        QStringLiteral("Poison"),
        QStringLiteral("FwdNID"),
        QStringLiteral("FwdTxnID"),
        QStringLiteral("DoNotGoToSD"),
        QStringLiteral("RetToSrc"),
    };
    return kFieldNames;
}

void prefillFixedFields(FlitRecord& row,
                        const CLogBTraceMetadata* metadata,
                        const Measures& measures)
{
    for (const QString& fieldName : templateFieldNamesStorage()) {
        const FieldLocation location = fieldLocation(row, metadata, measures, fieldName);
        qulonglong value = 0;
        if (!location.present || !fixedConventionValue(location.convention, value)) {
            continue;
        }
        writeBits(row.rawFlitWords, location.lsb, location.width, value);
    }
}

}  // namespace

QString FlitEditAdapter::fieldNameForColumn(const int column,
                                            const QString& optionalFieldName,
                                            const FlitRecord& row)
{
    if (column >= FlitTableModel::ColumnCount) {
        return optionalFieldName;
    }

    switch (column) {
    case FlitTableModel::TimeColumn:
        return QStringLiteral("Time");
    case FlitTableModel::ChannelColumn:
        return QStringLiteral("Channel");
    case FlitTableModel::DirectionColumn:
        return QStringLiteral("Direction");
    case FlitTableModel::OpcodeColumn:
        return QStringLiteral("Opcode");
    case FlitTableModel::SourceColumn:
        return QStringLiteral("SrcID");
    case FlitTableModel::TargetColumn:
        return QStringLiteral("TgtID");
    case FlitTableModel::TxnColumn:
        return QStringLiteral("TxnID");
    case FlitTableModel::AddressColumn:
        return row.address.isEmpty() ? QStringLiteral("DBID") : QStringLiteral("Addr");
    case FlitTableModel::DataIdColumn:
        return QStringLiteral("DataID");
    case FlitTableModel::RespColumn:
        return QStringLiteral("Resp");
    case FlitTableModel::FwdStateColumn:
        return QStringLiteral("FwdState");
    case FlitTableModel::RespErrColumn:
        return QStringLiteral("RespErr");
    default:
        break;
    }
    return QString();
}

QStringList FlitEditAdapter::templateFieldNames()
{
    return templateFieldNamesStorage();
}

FlitEditCellState FlitEditAdapter::cellState(const FlitRecord& row,
                                             const CLogBTraceMetadata* metadata,
                                             const QString& fieldName)
{
    if (fieldName.isEmpty() || fieldName == QLatin1String("Xact")) {
        return {.reason = QStringLiteral("This table column is not editable.")};
    }
    if (!metadata) {
        return {.reason = QStringLiteral("No trace metadata is available for CHI validation.")};
    }
    if (fieldName == QLatin1String("Time")) {
        return {
            .applicable = true,
            .editable = true,
            .fixed = false,
            .reason = QString(),
        };
    }
    if (fieldName == QLatin1String("Channel")
        || fieldName == QLatin1String("Direction")) {
        return {
            .applicable = true,
            .editable = false,
            .fixed = true,
            .reason = QStringLiteral("Change channel or direction by inserting a new template row."),
        };
    }
    if (!row.rawRecordAvailable) {
        return {.reason = QStringLiteral("This row has no raw CLog.B payload.")};
    }

    Measures measures;
    if (!evaluateMeasures(metadata->parameters, measures)) {
        return {.reason = QStringLiteral("The trace CHI parameters cannot be evaluated.")};
    }

    const FieldLocation location = fieldLocation(row, metadata, measures, fieldName);
    if (!location.present) {
        return {.reason = QStringLiteral("%1 is not present on this channel.").arg(fieldName)};
    }

    const bool applicable = CHI::Eb::Xact::FieldTrait::IsApplicable(location.convention);
    if (!applicable) {
        return {
            .applicable = false,
            .editable = false,
            .fixed = isFixedConvention(location.convention),
            .reason = QStringLiteral("%1 does not apply to this opcode/channel.").arg(fieldName),
        };
    }

    const bool editable = !isFixedConvention(location.convention);
    return {
        .applicable = true,
        .editable = editable,
        .fixed = !editable,
        .reason = editable
            ? QString()
            : QStringLiteral("%1 is fixed by the CHI opcode field mapping.").arg(fieldName),
    };
}

FlitEditResult FlitEditAdapter::applyEdit(FlitRecord& row,
                                          const CLogBTraceMetadata* metadata,
                                          const QString& fieldName,
                                          const QString& text)
{
    if (!metadata) {
        return {
            .summary = QStringLiteral("Edit rejected."),
            .detail = QStringLiteral("No trace metadata is available for validation."),
        };
    }

    if (fieldName == QLatin1String("Time")) {
        qulonglong value = 0;
        if (!parseInteger(text, value)
            || value > static_cast<qulonglong>(std::numeric_limits<qint64>::max())) {
            return {
                .summary = QStringLiteral("Invalid timestamp."),
                .detail = QStringLiteral("Enter a non-negative decimal or hexadecimal timestamp."),
            };
        }
        row.timestamp = static_cast<qint64>(value);
        clearDerivedState(row);
        return {.ok = true, .summary = QString(), .detail = QString()};
    }

    if (fieldName == QLatin1String("Channel")
        || fieldName == QLatin1String("Direction")) {
        return {
            .summary = QStringLiteral("Edit rejected."),
            .detail = QStringLiteral("Change channel or direction by inserting a new template row."),
        };
    }

    FlitEditCellState state = cellState(row, metadata, fieldName);
    if (!state.editable) {
        return {
            .summary = QStringLiteral("Edit rejected."),
            .detail = state.reason,
        };
    }

    Measures measures;
    if (!evaluateMeasures(metadata->parameters, measures)) {
        return {
            .summary = QStringLiteral("Edit rejected."),
            .detail = QStringLiteral("The trace CHI parameters cannot be evaluated."),
        };
    }

    const FieldLocation location = fieldLocation(row, metadata, measures, fieldName);
    qulonglong value = 0;
    if (!parseValueForField(row, fieldName, location, text, value)) {
        return {
            .summary = QStringLiteral("Invalid %1 value.").arg(fieldName),
            .detail = QStringLiteral("Use a decoded string, decimal integer, or hexadecimal integer."),
        };
    }

    if (fieldName == QLatin1String("Addr")
        && row.channel == FlitChannel::Snp) {
        if ((value & 0x7ULL) != 0) {
            return {
                .summary = QStringLiteral("Invalid snoop address."),
                .detail = QStringLiteral("SNP addresses must be 8-byte aligned."),
            };
        }
        value >>= 3U;
    }

    if (!fixedConventionAllows(location.convention, value)) {
        return {
            .summary = QStringLiteral("Invalid fixed field value."),
            .detail = QStringLiteral("%1 is fixed by the CHI opcode mapping.").arg(fieldName),
        };
    }

    if (location.width < 64 && (value >> location.width) != 0) {
        return {
            .summary = QStringLiteral("Value out of range."),
            .detail = QStringLiteral("%1 is %2 bits wide.").arg(fieldName).arg(location.width),
        };
    }

    FlitRecord edited = row;
    if (!writeBits(edited.rawFlitWords, location.lsb, location.width, value)) {
        return {
            .summary = QStringLiteral("Edit rejected."),
            .detail = QStringLiteral("%1 is too wide or unavailable in the raw flit payload.").arg(fieldName),
        };
    }
    if (fieldName == QLatin1String("Opcode")) {
        edited.opcodeRaw = QString::number(value);
    }

    if (fieldName == QLatin1String("Opcode")) {
        const FieldLocation newLocation = fieldLocation(edited, metadata, measures, fieldName);
        if (!newLocation.present || !CHI::Eb::Xact::FieldTrait::IsApplicable(newLocation.convention)) {
            return {
                .summary = QStringLiteral("Invalid opcode."),
                .detail = QStringLiteral("The opcode is not valid for this channel."),
            };
        }
        prefillFixedFields(edited, metadata, measures);
    }

    CLogBTraceLoadError decodeError;
    FlitRecord decoded;
    if (!CLogBTraceLoader::decodeRawRecord(*metadata,
                                           edited.timestamp,
                                           rawChannelFor(edited),
                                           edited.rawNodeId,
                                           edited.rawFlitLength,
                                           edited.rawFlitWords,
                                           decoded,
                                           decodeError)) {
        return {
            .summary = decodeError.summary.isEmpty()
                ? QStringLiteral("Edited flit could not be decoded.")
                : decodeError.summary,
            .detail = decodeError.informativeText,
        };
    }

    decoded.timestamp = edited.timestamp;
    decoded.rawRecordAvailable = true;
    decoded.rawNodeId = edited.rawNodeId;
    decoded.rawChannel = edited.rawChannel;
    decoded.rawFlitLength = edited.rawFlitLength;
    decoded.rawFlitWords = std::move(edited.rawFlitWords);
    clearDerivedState(decoded);
    row = std::move(decoded);
    return {.ok = true, .summary = QString(), .detail = QString()};
}

bool FlitEditAdapter::makeTemplate(const CLogBTraceMetadata* metadata,
                                   const FlitChannel channel,
                                   const FlitDirection direction,
                                   const qint64 timestamp,
                                   FlitRecord& row,
                                   QString* errorText)
{
    if (!metadata) {
        if (errorText) {
            *errorText = QStringLiteral("No trace metadata is available for template creation.");
        }
        return false;
    }

    Measures measures;
    if (!evaluateMeasures(metadata->parameters, measures)) {
        if (errorText) {
            *errorText = QStringLiteral("The trace CHI parameters cannot be evaluated.");
        }
        return false;
    }

    const std::size_t byteLength = expectedByteLength(metadata->parameters, measures, channel);
    if (byteLength == 0 || byteLength > std::numeric_limits<std::uint8_t>::max()) {
        if (errorText) {
            *errorText = QStringLiteral("The selected flit template has an unsupported width.");
        }
        return false;
    }

    row = {};
    row.timestamp = timestamp;
    row.channel = channel;
    row.direction = direction;
    row.rawRecordAvailable = true;
    row.rawNodeId = 0;
    row.rawChannel = static_cast<std::uint8_t>(rawChannelFor(row));
    row.rawFlitLength = static_cast<std::uint8_t>(byteLength);
    row.rawFlitWords.assign((byteLength + 3U) / 4U, 0U);
    prefillFixedFields(row, metadata, measures);

    CLogBTraceLoadError decodeError;
    FlitRecord decoded;
    if (!CLogBTraceLoader::decodeRawRecord(*metadata,
                                           row.timestamp,
                                           rawChannelFor(row),
                                           row.rawNodeId,
                                           row.rawFlitLength,
                                           row.rawFlitWords,
                                           decoded,
                                           decodeError)) {
        if (errorText) {
            *errorText = decodeError.summary.isEmpty()
                ? QStringLiteral("The template row could not be decoded.")
                : decodeError.summary;
        }
        return false;
    }

    decoded.rawRecordAvailable = true;
    decoded.rawNodeId = row.rawNodeId;
    decoded.rawChannel = row.rawChannel;
    decoded.rawFlitLength = row.rawFlitLength;
    decoded.rawFlitWords = row.rawFlitWords;
    clearDerivedState(decoded);
    row = std::move(decoded);
    return true;
}

}  // namespace CHIron::Gui
