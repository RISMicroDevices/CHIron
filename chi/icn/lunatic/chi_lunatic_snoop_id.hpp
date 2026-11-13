#pragma once

#include "chi_lunatic_types.hpp"

#include <functional>
#include <optional>
#include <set>

namespace CHI::Lunatic {

    class SnoopIdAllocator {
    public:
        using Filter = std::function<bool(TransactionId)>;

    private:
        TransactionId           next = 0;
        std::set<TransactionId> live;

    public:
        std::optional<TransactionId> Allocate(const Filter& filter = {});

        bool   Release(TransactionId txnId) noexcept;
        size_t Size() const noexcept { return live.size(); }
        void   Clear() noexcept;
    };

}
