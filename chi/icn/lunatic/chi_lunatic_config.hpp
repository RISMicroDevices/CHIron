#pragma once

#include "chi_lunatic_types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

namespace CHI::Lunatic {

    class SystemAddressMap;

    enum class RequesterRole : uint8_t {
        RN_F,
        RN_D,
        RN_I
    };

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
        size_t      inputQueueLimit = 16;
        size_t      outputQueueLimit = 16;
        size_t      transactionLimit = 16;
        size_t      snoopLimit = 32;
        size_t      slcSets = 1024;
        size_t      slcWays = 16;
        size_t      snoopFilterSets = 4096;
        size_t      snoopFilterWays = 16;
        size_t      dataWidth = 256;
        uint64_t    tagLatency = 2;
        uint64_t    dataLatency = 2;
        uint64_t    memoryLatency = 8;
        uint64_t    receiveDelay = 0;
        uint64_t    decodeDelay = 0;
        uint64_t    admissionDelay = 0;
        uint64_t    hazardDelay = 0;
        uint64_t    lookupDelay = 0;
        uint64_t    resolveDelay = 0;
        uint64_t    actionDelay = 0;
        uint64_t    returnDelay = 0;
        uint64_t    commitDelay = 0;
        uint64_t    randomDelayLimit = 0;
        uint64_t    seed = 0;
        NodeId      subordinateId = 0;
        PolicyKind  policy = PolicyKind::Exclusive;
        BackendKind backend = BackendKind::Internal;

        std::map<NodeId, RequesterRole> requesterRoles;
        std::shared_ptr<const SystemAddressMap> addressMap;

        size_t      axiOutstandingLimit = 1;
        uint8_t     axiIdWidth = 4;
        bool        randomizeArbitration = false;
        bool        preciseSnoopFilter = true;
        bool        doNotGoToSD = false;
        bool        retryEnabled = true;
        bool        separatedReadResponse = false;
        bool        separatedBackendRead = false;
        bool        dmtEnabled = false;
        bool        dctEnabled = false;
        bool        dwtEnabled = false;
        bool        directPersistEnabled = false;
        bool        subordinatePersistSepEnabled = true;
        bool        poisonEnabled = false;
        bool        dataCheckEnabled = false;
        bool        mteEnabled = false;
        bool        dvmEnabled = false;
    };

    RequesterRole GetRequesterRole(const Options& options, NodeId requester) noexcept;
    bool IsSnoopableRequester(const Options& options, NodeId requester) noexcept;
    void   ValidateOptions(const Options& options);
    size_t BytesPerBeat(const Options& options) noexcept;
    size_t BeatsPerLine(const Options& options) noexcept;

}
