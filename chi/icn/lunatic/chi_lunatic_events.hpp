#pragma once

#include "chi_lunatic_types.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace CHI::Lunatic {

enum class EventKind : uint8_t {
    AcceptedRXREQ,
    AcceptedRXRSP,
    AcceptedRXDAT,
    EmittedTXREQ,
    EmittedTXRSP,
    EmittedTXDAT,
    EmittedTXSNP,
    Denied,
    Retired,
    Reset,
    Error
};

struct Event {
    EventKind kind = EventKind::Error;
    Time time = 0;
    NodeId home = 0;
    NodeId source = 0;
    NodeId target = 0;
    TransactionId txnId = 0;
    TransactionHandle transaction = INVALID_TRANSACTION;
    uint16_t opcode = 0;
    LineKey line{};
    std::string message;
};

class EventHub {
public:
    using Listener = std::function<void(const Event&)>;

private:
    struct Registration {
        uint64_t id;
        std::string name;
        int priority;
        uint64_t order;
        Listener listener;
    };

    std::vector<Registration> listeners;
    std::deque<Event> pending;
    uint64_t nextListenerId = 1;
    uint64_t nextOrder = 0;
    bool dispatching = false;
    bool reentryRejected = false;

public:
    uint64_t Register(std::string name, int priority, Listener listener);
    bool Unregister(uint64_t id) noexcept;
    void Publish(Event event);
    bool Dispatch();
    void Clear() noexcept;

    size_t Pending() const noexcept { return pending.size(); }
    bool ReentryRejected() const noexcept { return reentryRejected; }
};

}
