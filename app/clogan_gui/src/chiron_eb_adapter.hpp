#pragma once

#include "config.hpp"
#include "flit_record.hpp"

#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace CHIron::Gui {

class EbFlitAdapter {
public:
    template<typename ReqFlitT>
    FlitRecord fromREQ(const qint64 timestamp,
                       const FlitDirection direction,
                       const ReqFlitT& flit) const
    {
        const FieldKeyValueMap map = CHI::Eb::Expresso::Flit::Map(flit);

        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Req;
        record.direction = direction;
        record.opcode = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::Opcode));
        record.opcodeRaw = formatIntegralKey(map, CHI::Eb::Expresso::Flit::Keys::REQ::Opcode);
        record.source = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::SrcID));
        record.target = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::TgtID));
        record.txnId = formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::TxnID);
        record.address = formatPattern(map, CHI::Eb::Expresso::Flit::Keys::REQ::Addr, "{1:#014x}");
        record.summary = joinSummary({
            labelValue(QStringLiteral("Addr"), record.address),
            labelValue(QStringLiteral("Size"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::Size)),
            labelValue(QStringLiteral("Retry"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::AllowRetry)),
            labelValue(QStringLiteral("Order"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::Order)),
            labelValue(QStringLiteral("NS"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::REQ::NS)),
        });
        record.transactionLabel = QStringLiteral("Txn %1").arg(record.txnId);
        record.fields = collectFields(CHI::Eb::Expresso::Flit::Keys::REQ::iteration(), map);
        return record;
    }

    template<typename RspFlitT>
    FlitRecord fromRSP(const qint64 timestamp,
                       const FlitDirection direction,
                       const RspFlitT& flit) const
    {
        const FieldKeyValueMap map = CHI::Eb::Expresso::Flit::Map(flit);

        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Rsp;
        record.direction = direction;
        record.opcode = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::Opcode));
        record.opcodeRaw = formatIntegralKey(map, CHI::Eb::Expresso::Flit::Keys::RSP::Opcode);
        record.source = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::SrcID));
        record.target = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::TgtID));
        record.txnId = formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::TxnID);
        record.dbid = formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::DBID);
        record.resp = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::Resp));
        record.fwdState = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::FwdState));
        record.respErr = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::RSP::RespErr));
        record.summary = joinSummary({
            labelValue(QStringLiteral("Resp"), record.resp),
            labelValue(QStringLiteral("Err"), record.respErr),
            labelValue(QStringLiteral("DBID"), record.dbid),
            labelValue(QStringLiteral("Fwd"), record.fwdState),
        });
        record.transactionLabel = makeTxnLabel(record.txnId, record.dbid);
        record.fields = collectFields(CHI::Eb::Expresso::Flit::Keys::RSP::iteration(), map);
        return record;
    }

    template<typename DatFlitT>
    FlitRecord fromDAT(const qint64 timestamp,
                       const FlitDirection direction,
                       const DatFlitT& flit) const
    {
        const FieldKeyValueMap map = CHI::Eb::Expresso::Flit::Map(flit);

        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Dat;
        record.direction = direction;
        record.opcode = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::Opcode));
        record.opcodeRaw = formatIntegralKey(map, CHI::Eb::Expresso::Flit::Keys::DAT::Opcode);
        record.source = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::SrcID));
        record.target = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::TgtID));
        record.txnId = formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::TxnID);
        record.dbid = formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::DBID);
        record.dataId = formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::DataID);
        record.resp = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::Resp));
        record.fwdState = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::FwdState));
        record.respErr = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::RespErr));
        record.summary = joinSummary({
            labelValue(QStringLiteral("Resp"), record.resp),
            labelValue(QStringLiteral("Beat"), record.dataId),
            labelValue(QStringLiteral("DBID"), record.dbid),
            labelValue(QStringLiteral("Src"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::DAT::DataSource)),
        });
        record.transactionLabel = makeTxnLabel(record.txnId, record.dbid);
        record.fields = collectFields(CHI::Eb::Expresso::Flit::Keys::DAT::iteration(), map);
        return record;
    }

    template<typename SnpFlitT>
    FlitRecord fromSNP(const qint64 timestamp,
                       const FlitDirection direction,
                       const SnpFlitT& flit) const
    {
        const FieldKeyValueMap map = CHI::Eb::Expresso::Flit::Map(flit);

        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = FlitChannel::Snp;
        record.direction = direction;
        record.opcode = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::SNP::Opcode));
        record.opcodeRaw = formatIntegralKey(map, CHI::Eb::Expresso::Flit::Keys::SNP::Opcode);
        record.source = InternDisplayString(formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::SNP::SrcID));
        record.txnId = formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::SNP::TxnID);
        record.address = formatPattern(map, CHI::Eb::Expresso::Flit::Keys::SNP::Addr, "{1:#014x}");
        record.summary = joinSummary({
            labelValue(QStringLiteral("Addr"), record.address),
            labelValue(QStringLiteral("FwdNID"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::SNP::FwdNID)),
            labelValue(QStringLiteral("FwdTxn"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::SNP::FwdTxnID)),
            labelValue(QStringLiteral("RetToSrc"), formatDisplay(map, CHI::Eb::Expresso::Flit::Keys::SNP::RetToSrc)),
        });
        record.transactionLabel = QStringLiteral("Snoop %1").arg(record.txnId);
        record.fields = collectFields(CHI::Eb::Expresso::Flit::Keys::SNP::iteration(), map);
        return record;
    }

private:
    CHI::Eb::Expresso::Flit::DecodingFormatter<ViewerConfig> formatter_;

private:
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

    static QString labelValue(const QString& label, const QString& value)
    {
        if (value.isEmpty()) {
            return QString();
        }
        return QStringLiteral("%1 %2").arg(label, value);
    }

    static QString uppercaseHexDisplay(QString value)
    {
        if (value.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
            return QStringLiteral("0x%1").arg(value.mid(2).toUpper());
        }
        return value.toUpper();
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

    QString formatDisplay(const FieldKeyValueMap& map, const FieldKey key) const
    {
        if (map.find(key) == map.end()) {
            return QString();
        }
        return simplify(QString::fromStdString(key->Format(formatter_, map, "{1}")));
    }

    QString formatPattern(const FieldKeyValueMap& map, const FieldKey key, const char* pattern) const
    {
        if (map.find(key) == map.end()) {
            return QString();
        }
        return simplify(uppercaseHexDisplay(QString::fromStdString(key->Format(formatter_, map, pattern))));
    }

    static QString formatIntegralKey(const FieldKeyValueMap& map, const FieldKey key)
    {
        const auto valueIt = map.find(key);
        if (valueIt == map.end() || !valueIt->second.IsIntegral()) {
            return QString();
        }

        return QStringLiteral("0x%1")
            .arg(QString::number(static_cast<qulonglong>(valueIt->second.AsIntegral()), 16).toUpper());
    }

    static QString simplify(QString value)
    {
        value = value.simplified();
        if (value == QLatin1String("0")
            || value.compare(QLatin1String("0x0"), Qt::CaseInsensitive) == 0) {
            return value;
        }
        return value;
    }

    static QString scopeName(const CHI::Eb::Expresso::Flit::KeyCategory category)
    {
        switch (category) {
        case CHI::Eb::Expresso::Flit::KeyCategory::REQ:
            return QStringLiteral("REQ");
        case CHI::Eb::Expresso::Flit::KeyCategory::RSP:
            return QStringLiteral("RSP");
        case CHI::Eb::Expresso::Flit::KeyCategory::DAT:
            return QStringLiteral("DAT");
        case CHI::Eb::Expresso::Flit::KeyCategory::SNP:
            return QStringLiteral("SNP");
        }
        return QStringLiteral("?");
    }

    static QString formatRawValue(const CHI::Eb::Expresso::Flit::Value& value)
    {
        if (value.IsIntegral()) {
            const auto raw = static_cast<qulonglong>(value.AsIntegral());
            return QStringLiteral("0x%1 (%2)")
                .arg(QString::number(raw, 16).toUpper())
                .arg(QString::number(raw));
        }

        if (value.IsVector()) {
            const auto& words = value.AsVector();
            QStringList fragments;
            const std::size_t limit = words.size() < 4 ? words.size() : 4U;

            for (std::size_t index = 0; index < limit; ++index) {
                fragments.append(QStringLiteral("[%1]=0x%2")
                                     .arg(index)
                                     .arg(QString::number(static_cast<qulonglong>(words[index]), 16).toUpper()));
            }
            if (words.size() > limit) {
                fragments.append(QStringLiteral("..."));
            }
            return fragments.join(QStringLiteral(", "));
        }

        return QString();
    }

    std::vector<FieldEntry> collectFields(const CHI::Eb::Expresso::Flit::KeyIteration& keys,
                                          const FieldKeyValueMap& map) const
    {
        std::vector<FieldEntry> fields;
        fields.reserve(24);

        for (auto it = keys.begin(); it != keys.end(); ++it) {
            const FieldKey key = *it;
            const auto valueIt = map.find(key);
            if (valueIt == map.end()) {
                continue;
            }

            const QString decoded = formatDisplay(map, key);
            const QString raw = formatRawValue(valueIt->second);
            if (decoded.isEmpty() && raw.isEmpty()) {
                continue;
            }

            fields.push_back(FieldEntry{
                .scope = InternFieldText(scopeName(key->category)),
                .name = InternFieldText(QString::fromLatin1(key->canonicalName)),
                .value = decoded,
                .raw = raw,
            });
        }

        return fields;
    }
};

}  // namespace CHIron::Gui
