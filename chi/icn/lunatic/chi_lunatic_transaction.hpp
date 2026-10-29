#pragma once

#include "chi_lunatic_types.hpp"

#include <cstdint>
#include <vector>

namespace CHI::Lunatic {

    enum Completion : uint32_t {
        COMPLETE_WRITE_DATA = 1u << 0,
        COMPLETE_MEMORY = 1u << 1,
        COMPLETE_SNOOPS = 1u << 2,
        COMPLETE_RESPONSE = 1u << 3,
        COMPLETE_COMPACK = 1u << 4,
        COMPLETE_COMMIT = 1u << 5
    };

    struct Transaction {
        const TransactionHandle        handle;
        const Request                  request;
        const Time                     actionAge;
        Phase                          phase = Phase::Receive;
        WaitReason                     wait = WaitReason::None;
        Time                           readyTime = 0;
        DataBufferId                   dbid = 0;
        LineData                       data{};
        ByteEnables                    byteEnables{};
        std::array<uint8_t, 8>         poison{};
        std::array<uint8_t, LINE_SIZE> dataCheck{};
        uint64_t                       receivedBeats = 0;
        uint64_t                       sentBeats = 0;
        uint32_t                       completion = 0;
        uint8_t                        responseResp = 0;
        uint8_t                        responseRespErr = 0;
        bool                           responseDirty = false;
        bool                           homeHit = false;
        bool                           allocateHome = false;
        bool                           memoryRequired = false;
        bool                           snoopsRequired = false;
        bool                           canceled = false;
        std::vector<NodeId>            snoopTargets;
        std::vector<NodeId>            pendingSnoops;

        Transaction(TransactionHandle handle, const Request& request, Time actionAge);

        bool Complete(Completion flag) const noexcept { return completion & flag; }
        void Mark(Completion flag) noexcept { completion |= flag; }
    };

}
