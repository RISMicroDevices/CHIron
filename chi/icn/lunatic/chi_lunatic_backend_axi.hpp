#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_protocol.hpp"
#include "chi_lunatic_types.hpp"

#include <deque>
#include <map>
#include <optional>
#include <string>

namespace CHI::Lunatic {

    enum class AXIBurst : uint8_t {
        FIXED = 0b00,
        INCR  = 0b01,
        WRAP  = 0b10
    };

    enum class AXIResp : uint8_t {
        OKAY   = 0b00,
        EXOKAY = 0b01,
        SLVERR = 0b10,
        DECERR = 0b11
    };

    struct AXIAddress {
        uint32_t  id = 0;
        uint64_t  address = 0;
        uint8_t   len = 0;
        uint8_t   size = 0;
        AXIBurst  burst = AXIBurst::INCR;
        uint8_t   cache = 0;
        uint8_t   qos = 0;
        uint8_t   prot = 0;

        bool operator==(const AXIAddress&) const = default;
    };

    struct AXIWriteData {
        LineData    data{};
        ByteEnables strobes{};
        bool        last = false;

        bool operator==(const AXIWriteData&) const = default;
    };

    struct AXIWriteResponse {
        uint32_t id = 0;
        AXIResp  resp = AXIResp::OKAY;
    };

    struct AXIReadData {
        uint32_t id = 0;
        LineData data{};
        AXIResp  resp = AXIResp::OKAY;
        bool     last = false;
    };

    struct AXIOperation {
        TransactionHandle transaction = INVALID_TRANSACTION;
        LineKey           line{};
        LineData          data{};
        ByteEnables       enables{};
        uint64_t          beatMask = 0;
        TransferGeometry  geometry{};
        uint8_t           memAttr = 0;
        uint8_t           qos = 0;
        bool              write = false;
        bool              atomic = false;
    };

    struct AXICompletion {
        TransactionHandle transaction = INVALID_TRANSACTION;
        LineKey           line{};
        LineData          data{};
        uint64_t          beatMask = 0;
        AXIResp           response = AXIResp::OKAY;
        bool              write = false;
        std::string       protocolError;
    };

    class AXIBackend {
    public:
        explicit AXIBackend(Options options);

        void Reset();
        void Tick();
        bool Submit(const AXIOperation& operation);
        bool HasInflight() const noexcept;
        size_t Outstanding(bool write) const noexcept;

        std::optional<AXIAddress>   PeekAW() const;
        std::optional<AXIWriteData> PeekW() const;
        std::optional<AXIAddress>   PeekAR() const;
        std::optional<AXIAddress>   PopAW();
        std::optional<AXIWriteData> PopW();
        std::optional<AXIAddress>   PopAR();

        bool PushB(const AXIWriteResponse& response);
        bool PushR(const AXIReadData& response);

        std::optional<AXICompletion> PeekCompletion() const;
        std::optional<AXICompletion> PopCompletion();
        std::optional<std::string>   PopError();

    private:
        struct Record {
            AXIOperation operation;
            AXIAddress   address{};
            uint64_t     serial = 0;
            size_t       nextWriteBeat = 0;
            size_t       receivedBeats = 0;
            LineData     readData{};
            AXIResp      response = AXIResp::OKAY;
            bool         addressEmitted = false;
            bool         completionQueued = false;
            std::string  protocolError;
        };

        Options                        options;
        std::map<uint32_t, Record>     writes;
        std::map<uint32_t, Record>     reads;
        std::deque<AXIAddress>         aw;
        std::deque<AXIAddress>         ar;
        std::deque<AXIWriteResponse>   b;
        std::deque<AXIReadData>        r;
        std::deque<AXICompletion>      completions;
        std::deque<std::string>        errors;
        uint64_t                       nextSerial = 0;

        static size_t                  FirstBeat(const Record& record) noexcept;
        static size_t                  BeatCount(const Record& record) noexcept;
        const Record*                  SelectedWrite() const noexcept;
        Record*                        SelectedWrite() noexcept;
        AXIWriteData                   WriteBeat(const Record& record) const;
        AXIAddress                     MakeAddress(const AXIOperation& operation, uint32_t id) const;
        uint32_t                       AllocateId(const std::map<uint32_t, Record>& records) const noexcept;
        bool                           Complete(Record& record, bool write);
        void                           ProcessResponses();
        void                           ProcessB(const AXIWriteResponse& response);
        void                           ProcessR(const AXIReadData& response);
    };

    uint8_t AXIReadCache(uint8_t memAttr) noexcept;
    uint8_t AXIWriteCache(uint8_t memAttr) noexcept;

}
