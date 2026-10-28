#pragma once

#include "chi_lunatic_types.hpp"

#include <cstddef>
#include <cstdint>

namespace CHI::Lunatic {

enum class PolicyKind : uint8_t {
    NINE,
    Exclusive,
    Inclusive
};

enum class BackendKind : uint8_t {
    Internal,
    CHI,
    AXI
};

struct Options {
    size_t inputQueueLimit = 16;
    size_t outputQueueLimit = 16;
    size_t transactionLimit = 16;
    size_t snoopLimit = 32;
    size_t slcSets = 1024;
    size_t slcWays = 16;
    size_t snoopFilterSets = 4096;
    size_t snoopFilterWays = 16;
    size_t dataWidth = 256;
    uint64_t tagLatency = 2;
    uint64_t dataLatency = 2;
    uint64_t memoryLatency = 8;
    uint64_t seed = 0;
    NodeId subordinateId = 0;
    PolicyKind policy = PolicyKind::Exclusive;
    BackendKind backend = BackendKind::Internal;
    bool preciseSnoopFilter = true;
    bool doNotGoToSD = false;
    bool retryEnabled = true;
    bool poisonEnabled = true;
    bool dataCheckEnabled = true;
    bool mteEnabled = false;
    bool dvmEnabled = false;
};

void ValidateOptions(const Options& options);
size_t BytesPerBeat(const Options& options) noexcept;
size_t BeatsPerLine(const Options& options) noexcept;

}
