#include "trace_statistics.hpp"

#include "config.hpp"

#include "chi_b/util/chi_b_util_decoding.hpp"
#include "chi_eb/util/chi_eb_util_decoding.hpp"

#include <algorithm>
#include <utility>

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

QString directionText(const FlitDirection direction)
{
    return direction == FlitDirection::Tx
        ? QStringLiteral("TX")
        : QStringLiteral("RX");
}

QString channelText(const FlitChannel channel)
{
    return ToString(channel);
}

FlitDirection directionFromRawChannel(const CLog::Channel channel)
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXRSP:
    case CLog::Channel::TXDAT:
    case CLog::Channel::TXSNP:
    case CLog::Channel::TXREQ_BeforeSAM:
        return FlitDirection::Tx;
    case CLog::Channel::RXREQ:
    case CLog::Channel::RXRSP:
    case CLog::Channel::RXDAT:
    case CLog::Channel::RXSNP:
    case CLog::Channel::RXREQ_BeforeSAM:
        return FlitDirection::Rx;
    }

    return FlitDirection::Tx;
}

FlitChannel logicalChannelFromRawChannel(const CLog::Channel channel)
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::RXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
        return FlitChannel::Req;
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP:
        return FlitChannel::Rsp;
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT:
        return FlitChannel::Dat;
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP:
        return FlitChannel::Snp;
    }

    return FlitChannel::Req;
}

CLog::Channel normalizedRawChannel(const CLog::Channel channel)
{
    switch (channel) {
    case CLog::Channel::TXREQ_BeforeSAM:
        return CLog::Channel::TXREQ;
    case CLog::Channel::RXREQ_BeforeSAM:
        return CLog::Channel::RXREQ;
    default:
        return channel;
    }
}

std::size_t logicalChannelIndex(const FlitChannel channel)
{
    switch (channel) {
    case FlitChannel::Req:
        return 0;
    case FlitChannel::Rsp:
        return 1;
    case FlitChannel::Dat:
        return 2;
    case FlitChannel::Snp:
        return 3;
    }

    return 0;
}

QString rowOpcodeCountKey(const FlitDirection direction,
                          const FlitChannel channel,
                          const QString& opcode)
{
    return QStringLiteral("%1|%2|%3")
        .arg(directionText(direction),
             channelText(channel),
             opcode.isEmpty() ? QStringLiteral("Unknown") : opcode);
}

TraceStatisticsOpcodeCount decodeRowOpcodeCount(const QString& key, const std::uint64_t count)
{
    const QStringList parts = key.split(QChar('|'));
    TraceStatisticsOpcodeCount result;
    result.direction = parts.value(0);
    result.channel = parts.value(1);
    result.opcode = parts.value(2);
    result.count = count;
    return result;
}

QString decodeFastOpcode(const CLog::Parameters& parameters,
                         const CLog::Channel channel,
                         const std::uint32_t opcode)
{
    switch (parameters.GetIssue()) {
    case CLog::Issue::Eb:
        switch (channel) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::RXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ_BeforeSAM:
            return QString::fromLatin1(
                CHI::Eb::Opcodes::REQ::Decoder<FlexibleEbReqFlit>::INSTANCE
                    .Decode(static_cast<FlexibleEbReqFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP:
            return QString::fromLatin1(
                CHI::Eb::Opcodes::RSP::Decoder<FlexibleEbRspFlit>::INSTANCE
                    .Decode(static_cast<FlexibleEbRspFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT:
            return QString::fromLatin1(
                CHI::Eb::Opcodes::DAT::Decoder<FlexibleEbDatFlit>::INSTANCE
                    .Decode(static_cast<FlexibleEbDatFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP:
            return QString::fromLatin1(
                CHI::Eb::Opcodes::SNP::Decoder<FlexibleEbSnpFlit>::INSTANCE
                    .Decode(static_cast<FlexibleEbSnpFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        }
        break;
    case CLog::Issue::B:
        switch (channel) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::RXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ_BeforeSAM:
            return QString::fromLatin1(
                CHI::B::Opcodes::REQ::Decoder<FlexibleBReqFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBReqFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP:
            return QString::fromLatin1(
                CHI::B::Opcodes::RSP::Decoder<FlexibleBRspFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBRspFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT:
            return QString::fromLatin1(
                CHI::B::Opcodes::DAT::Decoder<FlexibleBDatFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBDatFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP:
            return QString::fromLatin1(
                CHI::B::Opcodes::SNP::Decoder<FlexibleBSnpFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBSnpFlit::opcode_t>(opcode))
                    .GetName("Unknown"));
        }
        break;
    }

    return QStringLiteral("Unknown");
}

void sortOpcodeCounts(std::vector<TraceStatisticsOpcodeCount>& counts)
{
    std::sort(counts.begin(),
              counts.end(),
              [](const TraceStatisticsOpcodeCount& lhs, const TraceStatisticsOpcodeCount& rhs) {
                  if (lhs.count != rhs.count) {
                      return lhs.count > rhs.count;
                  }
                  if (lhs.direction != rhs.direction) {
                      return lhs.direction < rhs.direction;
                  }
                  if (lhs.channel != rhs.channel) {
                      return lhs.channel < rhs.channel;
                  }
                  return lhs.opcode < rhs.opcode;
              });
}

QString issueText(const CLog::Issue issue)
{
    switch (issue) {
    case CLog::Issue::B:
        return QStringLiteral("B");
    case CLog::Issue::Eb:
        return QStringLiteral("E.b");
    }

    return QStringLiteral("Unknown");
}

}  // namespace

void TraceStatisticsAccumulator::reset()
{
    parameters_ = {};
    hasParameters_ = false;
    hasTimestampRange_ = false;
    totalFlits_ = 0;
    txFlits_ = 0;
    rxFlits_ = 0;
    channelCounts_.fill(0);
    addressedFlits_ = 0;
    dbidFlits_ = 0;
    firstTimestamp_ = 0;
    lastTimestamp_ = 0;
    uniqueSources_.clear();
    uniqueTargets_.clear();
    rowOpcodeCounts_.clear();
    fastOpcodeCounts_.clear();
}

void TraceStatisticsAccumulator::setParameters(const CLog::Parameters& parameters)
{
    parameters_ = parameters;
    hasParameters_ = true;
}

void TraceStatisticsAccumulator::setTimestampRange(const qint64 firstTimestamp, const qint64 lastTimestamp)
{
    firstTimestamp_ = firstTimestamp;
    lastTimestamp_ = lastTimestamp;
    hasTimestampRange_ = true;
}

void TraceStatisticsAccumulator::addRow(const FlitRecord& row)
{
    ++totalFlits_;

    if (!hasTimestampRange_) {
        firstTimestamp_ = row.timestamp;
        lastTimestamp_ = row.timestamp;
        hasTimestampRange_ = true;
    } else {
        firstTimestamp_ = std::min(firstTimestamp_, row.timestamp);
        lastTimestamp_ = std::max(lastTimestamp_, row.timestamp);
    }

    if (row.direction == FlitDirection::Tx) {
        ++txFlits_;
    } else {
        ++rxFlits_;
    }

    ++channelCounts_[logicalChannelIndex(row.channel)];
    if (!row.source.isEmpty()) {
        uniqueSources_.insert(row.source);
    }
    if (!row.target.isEmpty()) {
        uniqueTargets_.insert(row.target);
    }
    if (!row.address.isEmpty()) {
        ++addressedFlits_;
    }
    if (!row.dbid.isEmpty()) {
        ++dbidFlits_;
    }

    const QString opcode = row.opcode.isEmpty()
        ? (row.opcodeRaw.isEmpty() ? QStringLiteral("Unknown") : row.opcodeRaw)
        : row.opcode;
    const QString key = rowOpcodeCountKey(row.direction, row.channel, opcode);
    rowOpcodeCounts_[key] = rowOpcodeCounts_.value(key) + 1;
}

void TraceStatisticsAccumulator::addFastRecord(const CLog::Channel channel,
                                               const CLogBTraceFastRecordInfo& record)
{
    ++totalFlits_;

    const FlitDirection direction = directionFromRawChannel(channel);
    const FlitChannel logicalChannel = logicalChannelFromRawChannel(channel);
    if (direction == FlitDirection::Tx) {
        ++txFlits_;
    } else {
        ++rxFlits_;
    }

    ++channelCounts_[logicalChannelIndex(logicalChannel)];
    if (record.sourceId != std::numeric_limits<std::uint32_t>::max()) {
        uniqueSources_.insert(QString::number(record.sourceId));
    }
    if (record.targetId != std::numeric_limits<std::uint32_t>::max()) {
        uniqueTargets_.insert(QString::number(record.targetId));
    }
    if (record.address != std::numeric_limits<std::uint64_t>::max()) {
        ++addressedFlits_;
    }
    if (record.dbid != std::numeric_limits<std::uint32_t>::max()) {
        ++dbidFlits_;
    }

    const FastOpcodeKey key{
        .channel = static_cast<std::uint8_t>(normalizedRawChannel(channel)),
        .opcode = record.opcode,
    };
    auto found = fastOpcodeCounts_.find(key);
    if (found == fastOpcodeCounts_.end()) {
        fastOpcodeCounts_.emplace(key, 1U);
    } else {
        ++found->second;
    }
}

TraceStatisticsResult TraceStatisticsAccumulator::finalize() const
{
    TraceStatisticsResult result;
    result.totalFlits = totalFlits_;
    result.txFlits = txFlits_;
    result.rxFlits = rxFlits_;
    result.channelCounts = channelCounts_;
    result.addressedFlits = addressedFlits_;
    result.dbidFlits = dbidFlits_;
    result.uniqueSourceCount = static_cast<std::uint64_t>(uniqueSources_.size());
    result.uniqueTargetCount = static_cast<std::uint64_t>(uniqueTargets_.size());
    result.firstTimestamp = firstTimestamp_;
    result.lastTimestamp = lastTimestamp_;

    if (hasParameters_) {
        result.issue = issueText(parameters_.GetIssue());
        result.nodeIdWidth = parameters_.GetNodeIdWidth();
        result.reqAddrWidth = parameters_.GetReqAddrWidth();
        result.dataWidth = parameters_.GetDataWidth();
        result.dataCheckPresent = parameters_.IsDataCheckPresent();
        result.poisonPresent = parameters_.IsPoisonPresent();
        result.mpamPresent = parameters_.IsMPAMPresent();
    }

    result.opcodeCounts.reserve(rowOpcodeCounts_.size() + fastOpcodeCounts_.size());
    for (auto it = rowOpcodeCounts_.cbegin(); it != rowOpcodeCounts_.cend(); ++it) {
        result.opcodeCounts.push_back(decodeRowOpcodeCount(it.key(), it.value()));
    }

    for (const auto& [key, count] : fastOpcodeCounts_) {
        const CLog::Channel rawChannel = static_cast<CLog::Channel>(key.channel);
        result.opcodeCounts.push_back(TraceStatisticsOpcodeCount{
            .direction = directionText(directionFromRawChannel(rawChannel)),
            .channel = channelText(logicalChannelFromRawChannel(rawChannel)),
            .opcode = decodeFastOpcode(parameters_, rawChannel, key.opcode),
            .count = count,
        });
    }

    result.uniqueOpcodeCount = static_cast<std::uint64_t>(result.opcodeCounts.size());
    sortOpcodeCounts(result.opcodeCounts);
    return result;
}

TraceStatisticsResult CalculateTraceStatistics(const std::vector<FlitRecord>& rows)
{
    TraceStatisticsAccumulator accumulator;
    for (const FlitRecord& row : rows) {
        accumulator.addRow(row);
    }
    return accumulator.finalize();
}

}  // namespace CHIron::Gui
