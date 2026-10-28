#pragma once

#include "chi_lunatic_types.hpp"

#include <optional>
#include <vector>

namespace CHI::Lunatic {

class Arbiter {
    struct Entry {
        TransactionHandle handle;
        Time age;
        uint8_t qos;
    };

    std::vector<Entry> entries;

public:
    bool Submit(TransactionHandle handle, Time age, uint8_t qos = 0);
    bool Remove(TransactionHandle handle) noexcept;
    std::optional<TransactionHandle> Peek() const noexcept;
    std::optional<TransactionHandle> PeekQoS() const noexcept;
    size_t Size() const noexcept { return entries.size(); }
    void Clear() noexcept { entries.clear(); }
};

}
