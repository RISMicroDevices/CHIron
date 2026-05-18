#pragma once

#include <QColor>
#include <QHash>
#include <QString>

#include <deque>
#include <cstdint>
#include <optional>
#include <mutex>
#include <vector>

namespace CHIron::Gui {

enum class FlitChannel {
    Req,
    Rsp,
    Dat,
    Snp
};

enum class FlitDirection {
    Tx,
    Rx
};

enum class XactionIndexState {
    NotStarted,
    Pending,
    Indexed,
    Denied
};

struct FieldEntry {
    const QString* scope = nullptr;
    const QString* name = nullptr;
    QString value;
    QString raw;
};

struct FlitRecord {
    qint64 timestamp = 0;
    FlitChannel channel = FlitChannel::Req;
    FlitDirection direction = FlitDirection::Tx;
    QString opcode;
    QString opcodeRaw;
    QString source;
    QString target;
    QString txnId;
    QString address;
    QString dbid;
    QString dataId;
    QString resp;
    QString fwdState;
    QString respErr;
    QString summary;
    QString annotation;
    QString transactionLabel;
    QString transactionGroupKey;
    QString channelTag;
    QString xactionDebugLog;
    std::optional<quint16> channelNodeId;
    bool dimTarget = false;
    bool xactionIndexChecked = false;
    bool xactionIndexed = false;
    bool xactionIndexProcessedByJoint = false;
    XactionIndexState xactionIndexState = XactionIndexState::NotStarted;
    std::vector<FieldEntry> fields;
    bool rawRecordAvailable = false;
    std::uint16_t rawNodeId = 0;
    std::uint8_t rawChannel = 0;
    std::uint8_t rawFlitLength = 0;
    std::vector<std::uint32_t> rawFlitWords;
};

inline QString ToString(const FlitChannel channel)
{
    switch (channel) {
    case FlitChannel::Req:
        return QStringLiteral("REQ");
    case FlitChannel::Rsp:
        return QStringLiteral("RSP");
    case FlitChannel::Dat:
        return QStringLiteral("DAT");
    case FlitChannel::Snp:
        return QStringLiteral("SNP");
    }

    return QStringLiteral("?");
}

inline QString ToString(const FlitDirection direction)
{
    return direction == FlitDirection::Tx ? QStringLiteral("TX")
                                          : QStringLiteral("RX");
}

inline QColor ChannelAccent(const FlitChannel channel)
{
    switch (channel) {
    case FlitChannel::Req:
        return QColor(QStringLiteral("#F3A847"));
    case FlitChannel::Rsp:
        return QColor(QStringLiteral("#5AB9D6"));
    case FlitChannel::Dat:
        return QColor(QStringLiteral("#7BC96F"));
    case FlitChannel::Snp:
        return QColor(QStringLiteral("#E7776F"));
    }

    return QColor(QStringLiteral("#D9D9D9"));
}

inline const QString& EmptyFieldText()
{
    static const QString empty;
    return empty;
}

class FieldTextPool final {
public:
    static FieldTextPool& instance()
    {
        static FieldTextPool pool;
        return pool;
    }

    const QString* intern(const QString& text)
    {
        if (text.isEmpty()) {
            return nullptr;
        }

        const std::lock_guard<std::mutex> lock(mutex_);
        const auto found = pool_.constFind(text);
        if (found != pool_.cend()) {
            return found.value();
        }

        storage_.push_back(text);
        const QString* stored = &storage_.back();
        pool_.insert(*stored, stored);
        return stored;
    }

private:
    QHash<QString, const QString*> pool_;
    std::deque<QString> storage_;
    std::mutex mutex_;
};

inline const QString* InternFieldText(const QString& text)
{
    return FieldTextPool::instance().intern(text);
}

class SharedDisplayStringPool final {
public:
    static SharedDisplayStringPool& instance()
    {
        static SharedDisplayStringPool pool;
        return pool;
    }

    QString intern(const QString& text)
    {
        if (text.isEmpty() || text.size() > 48) {
            return text;
        }

        const std::lock_guard<std::mutex> lock(mutex_);
        const auto found = pool_.constFind(text);
        if (found != pool_.cend()) {
            return found.value();
        }

        pool_.insert(text, text);
        return text;
    }

private:
    QHash<QString, QString> pool_;
    std::mutex mutex_;
};

inline QString InternDisplayString(const QString& text)
{
    return SharedDisplayStringPool::instance().intern(text);
}

inline const QString& FieldScopeText(const FieldEntry& field)
{
    return field.scope ? *field.scope : EmptyFieldText();
}

inline const QString& FieldNameText(const FieldEntry& field)
{
    return field.name ? *field.name : EmptyFieldText();
}

}  // namespace CHIron::Gui
