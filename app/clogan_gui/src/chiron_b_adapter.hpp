#pragma once

#include "flit_record.hpp"

#include "chi_b/spec/chi_b_protocol.hpp"
#include "chi_b/util/chi_b_util_decoding.hpp"

#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace CHIron::Gui {

namespace Detail {

inline QString HexValue(const qulonglong value, const int digits = 0)
{
    QString hex = QString::number(value, 16).toUpper();
    if (digits > 0) {
        hex = hex.rightJustified(digits, QLatin1Char('0'));
    }
    return QStringLiteral("0x%1").arg(hex);
}

inline QString RawIntegral(const qulonglong value)
{
    return QStringLiteral("%1 (%2)").arg(HexValue(value), QString::number(value));
}

inline QString UppercaseHexDisplay(QString value)
{
    if (value.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        return QStringLiteral("0x%1").arg(value.mid(2).toUpper());
    }
    return value.toUpper();
}

inline QString Simplify(QString value)
{
    value = value.simplified();
    if (value == QLatin1String("0")
        || value.compare(QLatin1String("0x0"), Qt::CaseInsensitive) == 0) {
        return value;
    }
    return value;
}

inline QString MakeTxnLabel(const QString& txnId, const QString& dbid)
{
    if (txnId.isEmpty() && dbid.isEmpty()) {
        return QString();
    }
    if (dbid.isEmpty()) {
        return QStringLiteral("Txn %1").arg(txnId);
    }
    return QStringLiteral("Txn %1 / DBID %2").arg(txnId, dbid);
}

inline QString LabelValue(const QString& label, const QString& value)
{
    if (value.isEmpty()) {
        return QString();
    }
    return QStringLiteral("%1 %2").arg(label, value);
}

inline QString JoinSummary(std::initializer_list<QString> parts)
{
    QStringList filtered;
    for (const QString& part : parts) {
        if (!part.isEmpty()) {
            filtered.append(part);
        }
    }
    return filtered.join(QStringLiteral("  |  "));
}

inline void AddField(std::vector<FieldEntry>& fields,
                     const QString& scope,
                     const QString& name,
                     const QString& value,
                     const QString& raw)
{
    if (value.isEmpty() && raw.isEmpty()) {
        return;
    }

    fields.push_back(FieldEntry{
        .scope = InternFieldText(scope),
        .name = InternFieldText(name),
        .value = value,
        .raw = raw,
    });
}

template<typename Tenum>
QString EnumName(const Tenum* info)
{
    return info && info->IsValid()
        ? QString::fromLatin1(info->name)
        : QString();
}

template<typename Tdecoder, typename Topcode>
QString DecodeOpcode(const Topcode opcode)
{
    static const Tdecoder decoder;
    return QString::fromLatin1(decoder.Decode(opcode).GetName("Unknown"));
}

template<typename DatFlitT>
QString FormatDataValue(const DatFlitT& flit)
{
    const int wordCount = static_cast<int>(DatFlitT::DATA_WIDTH / 64);
    QString value;
    value.reserve(wordCount * 16);

    for (int index = wordCount - 1; index >= 0; --index) {
        value.append(QString::number(static_cast<qulonglong>(flit.Data()[index]), 16)
                         .toUpper()
                         .rightJustified(16, QLatin1Char('0')));
    }

    return value;
}

template<typename DatFlitT>
QString FormatDataRaw(const DatFlitT& flit)
{
    const int wordCount = static_cast<int>(DatFlitT::DATA_WIDTH / 64);
    QStringList fragments;
    const int previewCount = qMin(wordCount, 4);

    for (int index = 0; index < previewCount; ++index) {
        fragments.append(QStringLiteral("[%1]=0x%2")
                             .arg(index)
                             .arg(QString::number(static_cast<qulonglong>(flit.Data()[index]), 16)
                                      .toUpper()
                                      .rightJustified(16, QLatin1Char('0'))));
    }

    if (wordCount > previewCount) {
        fragments.append(QStringLiteral("..."));
    }

    return fragments.join(QStringLiteral(", "));
}

}  // namespace Detail

class BFlitAdapter {
public:
    template<typename ReqFlitT>
    FlitRecord fromREQ(const qint64 timestamp,
                       const FlitDirection direction,
                       const ReqFlitT& flit) const
    {
        FlitRecord record;
        record.fields.reserve(22);
        record.timestamp = timestamp;
        record.channel = FlitChannel::Req;
        record.direction = direction;
        record.opcode = InternDisplayString(
            Detail::DecodeOpcode<CHI::B::Opcodes::REQ::Decoder<ReqFlitT>>(flit.Opcode()));
        record.opcodeRaw = Detail::HexValue(static_cast<qulonglong>(flit.Opcode()));
        record.source = InternDisplayString(QString::number(static_cast<qulonglong>(flit.SrcID())));
        record.target = InternDisplayString(QString::number(static_cast<qulonglong>(flit.TgtID())));
        record.txnId = QString::number(static_cast<qulonglong>(flit.TxnID()));
        record.address = Detail::Simplify(
            Detail::UppercaseHexDisplay(Detail::HexValue(static_cast<qulonglong>(flit.Addr()), 12)));
        const QString size = Detail::EnumName(CHI::B::Sizes::ToEnum(flit.Size()));
        const QString ns = Detail::EnumName(CHI::B::NSs::ToEnum(flit.NS()));
        const QString order = Detail::EnumName(CHI::B::Orders::ToEnum(flit.Order()));
        record.summary = Detail::JoinSummary({
            Detail::LabelValue(QStringLiteral("Addr"), record.address),
            Detail::LabelValue(QStringLiteral("Size"), size),
            Detail::LabelValue(QStringLiteral("Retry"), QString::number(static_cast<qulonglong>(flit.AllowRetry()))),
            Detail::LabelValue(QStringLiteral("Order"), order),
            Detail::LabelValue(QStringLiteral("NS"), ns),
        });
        record.transactionLabel = QStringLiteral("Txn %1").arg(record.txnId);
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("Opcode"), record.opcode, record.opcodeRaw);
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("SrcID"), record.source, Detail::RawIntegral(static_cast<qulonglong>(flit.SrcID())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("TgtID"), record.target, Detail::RawIntegral(static_cast<qulonglong>(flit.TgtID())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("TxnID"), record.txnId, Detail::RawIntegral(static_cast<qulonglong>(flit.TxnID())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("ReturnNID"), QString::number(static_cast<qulonglong>(flit.ReturnNID())), Detail::RawIntegral(static_cast<qulonglong>(flit.ReturnNID())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("StashNIDValid"), QString::number(static_cast<qulonglong>(flit.StashNIDValid())), Detail::RawIntegral(static_cast<qulonglong>(flit.StashNIDValid())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("ReturnTxnID"), QString::number(static_cast<qulonglong>(flit.ReturnTxnID())), Detail::RawIntegral(static_cast<qulonglong>(flit.ReturnTxnID())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("Size"), size, Detail::RawIntegral(static_cast<qulonglong>(flit.Size())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("Addr"), record.address, Detail::RawIntegral(static_cast<qulonglong>(flit.Addr())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("NS"), ns, Detail::RawIntegral(static_cast<qulonglong>(flit.NS())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("LikelyShared"), QString::number(static_cast<qulonglong>(flit.LikelyShared())), Detail::RawIntegral(static_cast<qulonglong>(flit.LikelyShared())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("AllowRetry"), QString::number(static_cast<qulonglong>(flit.AllowRetry())), Detail::RawIntegral(static_cast<qulonglong>(flit.AllowRetry())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("Order"), order, Detail::RawIntegral(static_cast<qulonglong>(flit.Order())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("PCrdType"), QString::number(static_cast<qulonglong>(flit.PCrdType())), Detail::RawIntegral(static_cast<qulonglong>(flit.PCrdType())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("MemAttr"), QString::number(static_cast<qulonglong>(flit.MemAttr())), Detail::RawIntegral(static_cast<qulonglong>(flit.MemAttr())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("SnpAttr"), QString::number(static_cast<qulonglong>(flit.SnpAttr())), Detail::RawIntegral(static_cast<qulonglong>(flit.SnpAttr())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("LPID"), QString::number(static_cast<qulonglong>(flit.LPID())), Detail::RawIntegral(static_cast<qulonglong>(flit.LPID())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("Excl"), QString::number(static_cast<qulonglong>(flit.Excl())), Detail::RawIntegral(static_cast<qulonglong>(flit.Excl())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("ExpCompAck"), QString::number(static_cast<qulonglong>(flit.ExpCompAck())), Detail::RawIntegral(static_cast<qulonglong>(flit.ExpCompAck())));
        Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("TraceTag"), QString::number(static_cast<qulonglong>(flit.TraceTag())), Detail::RawIntegral(static_cast<qulonglong>(flit.TraceTag())));
        if constexpr (ReqFlitT::hasRSVDC) {
            Detail::AddField(record.fields, QStringLiteral("REQ"), QStringLiteral("RSVDC"), QString::number(static_cast<qulonglong>(flit.RSVDC())), Detail::RawIntegral(static_cast<qulonglong>(flit.RSVDC())));
        }
        return record;
    }

    template<typename RspFlitT>
    FlitRecord fromRSP(const qint64 timestamp,
                       const FlitDirection direction,
                       const RspFlitT& flit) const
    {
        FlitRecord record;
        record.fields.reserve(10);
        record.timestamp = timestamp;
        record.channel = FlitChannel::Rsp;
        record.direction = direction;
        record.opcode = InternDisplayString(
            Detail::DecodeOpcode<CHI::B::Opcodes::RSP::Decoder<RspFlitT>>(flit.Opcode()));
        record.opcodeRaw = Detail::HexValue(static_cast<qulonglong>(flit.Opcode()));
        record.source = InternDisplayString(QString::number(static_cast<qulonglong>(flit.SrcID())));
        record.target = InternDisplayString(QString::number(static_cast<qulonglong>(flit.TgtID())));
        record.txnId = QString::number(static_cast<qulonglong>(flit.TxnID()));
        record.dbid = QString::number(static_cast<qulonglong>(flit.DBID()));
        record.resp = InternDisplayString(Detail::EnumName(CHI::B::Resps::ToEnum(flit.Resp())));
        record.fwdState = InternDisplayString(
            Detail::EnumName(
                CHI::B::FwdStates::ToEnum(CHI::B::FwdState(static_cast<unsigned int>(flit.FwdState())))));
        record.respErr = InternDisplayString(Detail::EnumName(CHI::B::RespErrs::ToEnum(flit.RespErr())));
        record.summary = Detail::JoinSummary({
            Detail::LabelValue(QStringLiteral("Resp"), record.resp),
            Detail::LabelValue(QStringLiteral("Err"), record.respErr),
            Detail::LabelValue(QStringLiteral("DBID"), record.dbid),
            Detail::LabelValue(QStringLiteral("Fwd"), record.fwdState),
        });
        record.transactionLabel = Detail::MakeTxnLabel(record.txnId, record.dbid);
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("Opcode"), record.opcode, record.opcodeRaw);
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("SrcID"), record.source, Detail::RawIntegral(static_cast<qulonglong>(flit.SrcID())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("TgtID"), record.target, Detail::RawIntegral(static_cast<qulonglong>(flit.TgtID())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("TxnID"), record.txnId, Detail::RawIntegral(static_cast<qulonglong>(flit.TxnID())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("RespErr"), record.respErr, Detail::RawIntegral(static_cast<qulonglong>(flit.RespErr())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("Resp"), record.resp, Detail::RawIntegral(static_cast<qulonglong>(flit.Resp())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("FwdState"), record.fwdState, Detail::RawIntegral(static_cast<qulonglong>(flit.FwdState())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("DBID"), record.dbid, Detail::RawIntegral(static_cast<qulonglong>(flit.DBID())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("PCrdType"), QString::number(static_cast<qulonglong>(flit.PCrdType())), Detail::RawIntegral(static_cast<qulonglong>(flit.PCrdType())));
        Detail::AddField(record.fields, QStringLiteral("RSP"), QStringLiteral("TraceTag"), QString::number(static_cast<qulonglong>(flit.TraceTag())), Detail::RawIntegral(static_cast<qulonglong>(flit.TraceTag())));
        return record;
    }

    template<typename DatFlitT>
    FlitRecord fromDAT(const qint64 timestamp,
                       const FlitDirection direction,
                       const DatFlitT& flit) const
    {
        FlitRecord record;
        record.fields.reserve(18);
        record.timestamp = timestamp;
        record.channel = FlitChannel::Dat;
        record.direction = direction;
        record.opcode = InternDisplayString(
            Detail::DecodeOpcode<CHI::B::Opcodes::DAT::Decoder<DatFlitT>>(flit.Opcode()));
        record.opcodeRaw = Detail::HexValue(static_cast<qulonglong>(flit.Opcode()));
        record.source = InternDisplayString(QString::number(static_cast<qulonglong>(flit.SrcID())));
        record.target = InternDisplayString(QString::number(static_cast<qulonglong>(flit.TgtID())));
        record.txnId = QString::number(static_cast<qulonglong>(flit.TxnID()));
        record.dbid = QString::number(static_cast<qulonglong>(flit.DBID()));
        record.dataId = QString::number(static_cast<qulonglong>(flit.DataID()));
        record.resp = InternDisplayString(Detail::EnumName(CHI::B::Resps::ToEnum(flit.Resp())));
        record.fwdState = InternDisplayString(
            Detail::EnumName(
                CHI::B::FwdStates::ToEnum(CHI::B::FwdState(static_cast<unsigned int>(flit.FwdState())))));
        record.respErr = InternDisplayString(Detail::EnumName(CHI::B::RespErrs::ToEnum(flit.RespErr())));
        record.summary = Detail::JoinSummary({
            Detail::LabelValue(QStringLiteral("Resp"), record.resp),
            Detail::LabelValue(QStringLiteral("Beat"), record.dataId),
            Detail::LabelValue(QStringLiteral("DBID"), record.dbid),
            Detail::LabelValue(QStringLiteral("Src"), QString::number(static_cast<qulonglong>(flit.DataSource()))),
        });
        record.transactionLabel = Detail::MakeTxnLabel(record.txnId, record.dbid);
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("Opcode"), record.opcode, record.opcodeRaw);
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("SrcID"), record.source, Detail::RawIntegral(static_cast<qulonglong>(flit.SrcID())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("TgtID"), record.target, Detail::RawIntegral(static_cast<qulonglong>(flit.TgtID())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("TxnID"), record.txnId, Detail::RawIntegral(static_cast<qulonglong>(flit.TxnID())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("HomeNID"), QString::number(static_cast<qulonglong>(flit.HomeNID())), Detail::RawIntegral(static_cast<qulonglong>(flit.HomeNID())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("RespErr"), record.respErr, Detail::RawIntegral(static_cast<qulonglong>(flit.RespErr())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("Resp"), record.resp, Detail::RawIntegral(static_cast<qulonglong>(flit.Resp())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("FwdState"), record.fwdState, Detail::RawIntegral(static_cast<qulonglong>(flit.FwdState())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("DataSource"), QString::number(static_cast<qulonglong>(flit.DataSource())), Detail::RawIntegral(static_cast<qulonglong>(flit.DataSource())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("DBID"), record.dbid, Detail::RawIntegral(static_cast<qulonglong>(flit.DBID())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("CCID"), QString::number(static_cast<qulonglong>(flit.CCID())), Detail::RawIntegral(static_cast<qulonglong>(flit.CCID())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("DataID"), record.dataId, Detail::RawIntegral(static_cast<qulonglong>(flit.DataID())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("TraceTag"), QString::number(static_cast<qulonglong>(flit.TraceTag())), Detail::RawIntegral(static_cast<qulonglong>(flit.TraceTag())));
        if constexpr (DatFlitT::hasRSVDC) {
            Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("RSVDC"), QString::number(static_cast<qulonglong>(flit.RSVDC())), Detail::RawIntegral(static_cast<qulonglong>(flit.RSVDC())));
        }
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("BE"), Detail::UppercaseHexDisplay(Detail::HexValue(static_cast<qulonglong>(flit.BE()))), Detail::RawIntegral(static_cast<qulonglong>(flit.BE())));
        Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("Data"), Detail::FormatDataValue(flit), Detail::FormatDataRaw(flit));
        if constexpr (DatFlitT::hasDataCheck) {
            Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("DataCheck"), Detail::UppercaseHexDisplay(Detail::HexValue(static_cast<qulonglong>(flit.DataCheck()))), Detail::RawIntegral(static_cast<qulonglong>(flit.DataCheck())));
        }
        if constexpr (DatFlitT::hasPoison) {
            Detail::AddField(record.fields, QStringLiteral("DAT"), QStringLiteral("Poison"), QString::number(static_cast<qulonglong>(flit.Poison())), Detail::RawIntegral(static_cast<qulonglong>(flit.Poison())));
        }
        return record;
    }

    template<typename SnpFlitT>
    FlitRecord fromSNP(const qint64 timestamp,
                       const FlitDirection direction,
                       const SnpFlitT& flit) const
    {
        FlitRecord record;
        record.fields.reserve(10);
        record.timestamp = timestamp;
        record.channel = FlitChannel::Snp;
        record.direction = direction;
        record.opcode = InternDisplayString(
            Detail::DecodeOpcode<CHI::B::Opcodes::SNP::Decoder<SnpFlitT>>(flit.Opcode()));
        record.opcodeRaw = Detail::HexValue(static_cast<qulonglong>(flit.Opcode()));
        record.source = InternDisplayString(QString::number(static_cast<qulonglong>(flit.SrcID())));
        record.txnId = QString::number(static_cast<qulonglong>(flit.TxnID()));
        record.address = Detail::Simplify(
            Detail::UppercaseHexDisplay(Detail::HexValue(static_cast<qulonglong>(flit.Addr() << 3), 12)));
        record.summary = Detail::JoinSummary({
            Detail::LabelValue(QStringLiteral("Addr"), record.address),
            Detail::LabelValue(QStringLiteral("FwdNID"), QString::number(static_cast<qulonglong>(flit.FwdNID()))),
            Detail::LabelValue(QStringLiteral("FwdTxn"), QString::number(static_cast<qulonglong>(flit.FwdTxnID()))),
            Detail::LabelValue(QStringLiteral("RetToSrc"), QString::number(static_cast<qulonglong>(flit.RetToSrc()))),
        });
        record.transactionLabel = QStringLiteral("Snoop %1").arg(record.txnId);
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("Opcode"), record.opcode, record.opcodeRaw);
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("SrcID"), record.source, Detail::RawIntegral(static_cast<qulonglong>(flit.SrcID())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("TxnID"), record.txnId, Detail::RawIntegral(static_cast<qulonglong>(flit.TxnID())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("FwdNID"), QString::number(static_cast<qulonglong>(flit.FwdNID())), Detail::RawIntegral(static_cast<qulonglong>(flit.FwdNID())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("FwdTxnID"), QString::number(static_cast<qulonglong>(flit.FwdTxnID())), Detail::RawIntegral(static_cast<qulonglong>(flit.FwdTxnID())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("Addr"), record.address, Detail::RawIntegral(static_cast<qulonglong>(flit.Addr())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("NS"), Detail::EnumName(CHI::B::NSs::ToEnum(flit.NS())), Detail::RawIntegral(static_cast<qulonglong>(flit.NS())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("DoNotGoToSD"), QString::number(static_cast<qulonglong>(flit.DoNotGoToSD())), Detail::RawIntegral(static_cast<qulonglong>(flit.DoNotGoToSD())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("RetToSrc"), QString::number(static_cast<qulonglong>(flit.RetToSrc())), Detail::RawIntegral(static_cast<qulonglong>(flit.RetToSrc())));
        Detail::AddField(record.fields, QStringLiteral("SNP"), QStringLiteral("TraceTag"), QString::number(static_cast<qulonglong>(flit.TraceTag())), Detail::RawIntegral(static_cast<qulonglong>(flit.TraceTag())));
        return record;
    }
};

}  // namespace CHIron::Gui
