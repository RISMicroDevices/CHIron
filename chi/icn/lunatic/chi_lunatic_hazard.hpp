#pragma once

#include "chi_lunatic_types.hpp"

#include <map>
#include <optional>
#include <vector>

namespace CHI::Lunatic {

class HazardGraph {
    struct Entry {
        TransactionHandle handle = INVALID_TRANSACTION;
        Time age = 0;
    };

    std::map<LineKey, std::vector<Entry>> lines;

public:
    bool Add(TransactionHandle handle, const LineKey& line, Time age);
    bool IsHead(TransactionHandle handle, const LineKey& line) const noexcept;
    bool Has(const LineKey& line) const noexcept;
    std::optional<TransactionHandle> Next(const LineKey& line) const;
    std::vector<TransactionHandle> Order(const LineKey& line) const;
    bool Release(TransactionHandle handle, const LineKey& line) noexcept;
    bool Empty() const noexcept { return lines.empty(); }
    bool Validate() const noexcept;
    void Clear() noexcept { lines.clear(); }
};

}
