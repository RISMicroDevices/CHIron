#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace CHI::Lunatic {

    using NodeId = uint16_t;
    using TransactionId = uint16_t;
    using DatabaseId = uint16_t;
    using TransactionHandle = uint32_t;
    using Time = uint64_t;

    inline constexpr size_t LINE_SIZE = 64;
    inline constexpr TransactionHandle INVALID_TRANSACTION = std::numeric_limits<TransactionHandle>::max();

    using LineData = std::array<uint8_t, LINE_SIZE>;
    using ByteEnables = std::array<uint8_t, LINE_SIZE>;

    struct LineKey {
        uint64_t                        address = 0;
        bool                            nonSecure = false;

        auto                            operator<=>(const LineKey&) const = default;

        static LineKey                  FromAddress(uint64_t address, bool nonSecure) noexcept
        {
            return { address & ~uint64_t(LINE_SIZE - 1), nonSecure };
        }
    };

    enum class Phase : uint8_t {
        Receive,
        Decode,
        Admit,
        Hazard,
        Lookup,
        Resolve,
        Action,
        WaitMemory,
        WaitWriteData,
        WaitSnoop,
        Return,
        WaitCompAck,
        Commit,
        Retired
    };

    enum class WaitReason : uint8_t {
        None,
        InputQueue,
        Tracker,
        SameLine,
        TagPort,
        DataPort,
        Memory,
        WriteData,
        Snoop,
        CompAck,
        OutputQueue,
        BackInvalidate
    };

    enum class Direction : uint8_t {
        RequesterToHome,
        HomeToRequester,
        HomeToSubordinate,
        SubordinateToHome
    };

    struct Request {
        NodeId                          source = 0;
        NodeId                          target = 0;
        TransactionId                   txnId = 0;
        uint64_t                        address = 0;
        uint16_t                        opcode = 0;
        uint16_t                        returnNid = 0;
        uint16_t                        returnTxnId = 0;
        uint8_t                         size = 6;
        uint8_t                         qos = 0;
        uint8_t                         order = 0;
        uint8_t                         memAttr = 0;
        uint8_t                         snpAttr = 0;
        uint8_t                         pCrdType = 0;
        uint8_t                         traceTag = 0;
        bool                            nonSecure = false;
        bool                            allowRetry = true;
        bool                            expCompAck = false;
        bool                            likelyShared = false;

        LineKey                         Line() const noexcept { return LineKey::FromAddress(address, nonSecure); }
    };

    struct Response {
        NodeId                          source = 0;
        NodeId                          target = 0;
        TransactionId                   txnId = 0;
        DatabaseId                      dbid = 0;
        uint16_t                        opcode = 0;
        uint8_t                         resp = 0;
        uint8_t                         respErr = 0;
        uint8_t                         fwdState = 0;
        uint8_t                         pCrdType = 0;
        uint8_t                         qos = 0;
        uint8_t                         cBusy = 0;
        uint8_t                         traceTag = 0;
    };

    struct Data {
        NodeId                          source = 0;
        NodeId                          target = 0;
        NodeId                          homeNid = 0;
        TransactionId                   txnId = 0;
        DatabaseId                      dbid = 0;
        uint16_t                        opcode = 0;
        uint8_t                         resp = 0;
        uint8_t                         respErr = 0;
        uint8_t                         dataSource = 0;
        uint8_t                         dataId = 0;
        uint8_t                         ccid = 0;
        uint8_t                         qos = 0;
        uint8_t                         cBusy = 0;
        uint8_t                         traceTag = 0;
        LineData                        bytes{};
        ByteEnables                     byteEnables{};
        std::array<uint8_t, 8>          poison{};
        std::array<uint8_t, LINE_SIZE>  dataCheck{};

        size_t                          ByteOffset() const noexcept { return size_t(dataId) * 16; }
    };

    struct Snoop {
        NodeId                          source = 0;
        NodeId                          target = 0;
        TransactionId                   txnId = 0;
        uint64_t                        address = 0;
        uint16_t                        opcode = 0;
        uint8_t                         qos = 0;
        uint8_t                         traceTag = 0;
        bool                            nonSecure = false;
        bool                            doNotGoToSD = false;
        bool                            retToSrc = false;

        LineKey                         Line() const noexcept { return LineKey::FromAddress(address, nonSecure); }
    };

    struct HomeLineInfo {
        LineData                        data{};
        bool                            valid = false;
        bool                            dirty = false;
    };

    struct ErrorInfo {
        Time                            time = 0;
        TransactionHandle               transaction = INVALID_TRANSACTION;
        std::string                     reason;
    };

}
