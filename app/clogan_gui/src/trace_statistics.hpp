#pragma once

#include "clog_b_trace_loader.hpp"
#include "flit_record.hpp"

#include <QHash>
#include <QSet>
#include <QString>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace CHIron::Gui {

struct TraceStatisticsOpcodeCount {
    QString direction;
    QString channel;
    QString opcode;
    std::uint64_t count = 0;
};

struct TraceStatisticsResult {
    QString issue;
    std::uint64_t totalFlits = 0;
    std::uint64_t txFlits = 0;
    std::uint64_t rxFlits = 0;
    std::array<std::uint64_t, 4> channelCounts{};
    std::uint64_t addressedFlits = 0;
    std::uint64_t dbidFlits = 0;
    std::uint64_t uniqueSourceCount = 0;
    std::uint64_t uniqueTargetCount = 0;
    std::uint64_t uniqueOpcodeCount = 0;
    qint64 firstTimestamp = 0;
    qint64 lastTimestamp = 0;
    std::size_t nodeIdWidth = 0;
    std::size_t reqAddrWidth = 0;
    std::size_t dataWidth = 0;
    bool dataCheckPresent = false;
    bool poisonPresent = false;
    bool mpamPresent = false;
    std::vector<TraceStatisticsOpcodeCount> opcodeCounts;
};

class TraceStatisticsAccumulator final {
public:
    void reset();
    void setParameters(const CLog::Parameters& parameters);
    void setTimestampRange(qint64 firstTimestamp, qint64 lastTimestamp);
    void addRow(const FlitRecord& row);
    void addFastRecord(CLog::Channel channel, const CLogBTraceFastRecordInfo& record);
    TraceStatisticsResult finalize() const;

private:
    struct FastOpcodeKey {
        std::uint8_t channel = 0;
        std::uint32_t opcode = 0;

        bool operator==(const FastOpcodeKey& other) const noexcept
        {
            return channel == other.channel
                && opcode == other.opcode;
        }
    };

    struct FastOpcodeKeyHash {
        std::size_t operator()(const FastOpcodeKey& key) const noexcept
        {
            return (static_cast<std::size_t>(key.channel) << 24)
                ^ static_cast<std::size_t>(key.opcode);
        }
    };

private:
    CLog::Parameters parameters_{};
    bool hasParameters_ = false;
    bool hasTimestampRange_ = false;
    std::uint64_t totalFlits_ = 0;
    std::uint64_t txFlits_ = 0;
    std::uint64_t rxFlits_ = 0;
    std::array<std::uint64_t, 4> channelCounts_{};
    std::uint64_t addressedFlits_ = 0;
    std::uint64_t dbidFlits_ = 0;
    qint64 firstTimestamp_ = 0;
    qint64 lastTimestamp_ = 0;
    QSet<QString> uniqueSources_;
    QSet<QString> uniqueTargets_;
    QHash<QString, std::uint64_t> rowOpcodeCounts_;
    std::unordered_map<FastOpcodeKey, std::uint64_t, FastOpcodeKeyHash> fastOpcodeCounts_;
};

TraceStatisticsResult CalculateTraceStatistics(const std::vector<FlitRecord>& rows);

}  // namespace CHIron::Gui
