#pragma once

#include "flit_record.hpp"

#include <QStringList>

#include <algorithm>
#include <utility>

namespace CHIron::Gui {

inline QChar TransactionGroupKeySeparator()
{
    return QChar(0x1F);
}

inline QString TransactionKeyPart(QString value)
{
    value = value.trimmed();
    value.replace(TransactionGroupKeySeparator(), QChar('_'));
    return value;
}

inline void AppendTransactionKey(QStringList& keys, QString key)
{
    key = key.trimmed();
    if (key.isEmpty() || keys.contains(key)) {
        return;
    }
    keys.append(std::move(key));
}

inline void AppendTransactionGroupKey(FlitRecord& record, const QString& key)
{
    if (key.trimmed().isEmpty()) {
        return;
    }

    QStringList keys = record.transactionGroupKey.split(TransactionGroupKeySeparator(),
                                                        Qt::SkipEmptyParts);
    AppendTransactionKey(keys, key);
    record.transactionGroupKey = keys.join(TransactionGroupKeySeparator());
}

inline QString SortedEndpointPairKey(const QString& prefix,
                                     const QString& lhs,
                                     const QString& rhs,
                                     const QString& id)
{
    QString left = TransactionKeyPart(lhs);
    QString right = TransactionKeyPart(rhs);
    const QString normalizedId = TransactionKeyPart(id);
    if (left.isEmpty() || right.isEmpty() || normalizedId.isEmpty()) {
        return QString();
    }
    if (right < left) {
        std::swap(left, right);
    }
    return prefix + QLatin1Char('|') + left + QLatin1Char('|') + right + QLatin1Char('|') + normalizedId;
}

inline const FieldEntry* FindTransactionField(const FlitRecord& record, const QString& fieldName)
{
    for (const FieldEntry& field : record.fields) {
        if (FieldNameText(field) == fieldName) {
            return &field;
        }
    }
    return nullptr;
}

inline QString TransactionFieldValue(const FlitRecord& record, const QString& fieldName)
{
    const FieldEntry* field = FindTransactionField(record, fieldName);
    return field ? field->value : QString();
}

inline QStringList BuildFallbackTransactionGroupKeys(const FlitRecord& record)
{
    QStringList keys;

    const QString txnId = TransactionKeyPart(record.txnId);
    const QString source = TransactionKeyPart(record.source);
    const QString target = TransactionKeyPart(record.target);
    const QString dbid = TransactionKeyPart(record.dbid);

    if (!record.transactionLabel.trimmed().isEmpty()) {
        AppendTransactionKey(keys, QStringLiteral("label|") + TransactionKeyPart(record.transactionLabel));
    }

    if (!txnId.isEmpty()) {
        if (!source.isEmpty()) {
            AppendTransactionKey(keys,
                                 QStringLiteral("src-txn|%1|%2|%3")
                                     .arg(ToString(record.channel), source, txnId));
        }
        if (!source.isEmpty() && !target.isEmpty()) {
            AppendTransactionKey(keys,
                                 SortedEndpointPairKey(QStringLiteral("txn-pair"),
                                                       source,
                                                       target,
                                                       txnId));
        }

        if ((record.channel == FlitChannel::Req || record.channel == FlitChannel::Snp)
            && !source.isEmpty()) {
            AppendTransactionKey(keys,
                                 QStringLiteral("origin|%1|%2|%3")
                                     .arg(ToString(record.channel), source, txnId));
        } else if ((record.channel == FlitChannel::Rsp || record.channel == FlitChannel::Dat)
                   && !target.isEmpty()) {
            AppendTransactionKey(keys, QStringLiteral("origin|REQ|%1|%2").arg(target, txnId));
            AppendTransactionKey(keys, QStringLiteral("origin|SNP|%1|%2").arg(target, txnId));
        }
    }

    if (!dbid.isEmpty() && !source.isEmpty() && !target.isEmpty()) {
        AppendTransactionKey(keys,
                             SortedEndpointPairKey(QStringLiteral("dbid-pair"),
                                                   source,
                                                   target,
                                                   dbid));
    }

    if (dbid.isEmpty()
        && !txnId.isEmpty()
        && !source.isEmpty()
        && !target.isEmpty()
        && (record.channel == FlitChannel::Rsp || record.channel == FlitChannel::Dat)) {
        AppendTransactionKey(keys,
                             SortedEndpointPairKey(QStringLiteral("dbid-pair"),
                                                   source,
                                                   target,
                                                   txnId));
    }

    const QString fwdNid = TransactionKeyPart(TransactionFieldValue(record, QStringLiteral("FwdNID")));
    const QString fwdTxnId = TransactionKeyPart(TransactionFieldValue(record, QStringLiteral("FwdTxnID")));
    if (!fwdNid.isEmpty() && !fwdTxnId.isEmpty()) {
        AppendTransactionKey(keys, QStringLiteral("fwd-txn|%1|%2").arg(fwdNid, fwdTxnId));
    }

    return keys;
}

inline QStringList TransactionGroupKeys(const FlitRecord& record)
{
    QStringList keys = record.transactionGroupKey.split(TransactionGroupKeySeparator(),
                                                        Qt::SkipEmptyParts);
    if (record.xactionIndexChecked) {
        return keys;
    }

    for (const QString& key : BuildFallbackTransactionGroupKeys(record)) {
        AppendTransactionKey(keys, key);
    }
    return keys;
}

inline void AppendFallbackTransactionGroupKeys(FlitRecord& record)
{
    if (record.xactionIndexChecked) {
        return;
    }

    for (const QString& key : BuildFallbackTransactionGroupKeys(record)) {
        AppendTransactionGroupKey(record, key);
    }
}

}  // namespace CHIron::Gui
