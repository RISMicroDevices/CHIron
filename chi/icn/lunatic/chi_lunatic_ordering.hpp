#pragma once

#include "chi_lunatic_transaction.hpp"

#include <map>

namespace CHI::Lunatic {

    class OrderingTracker {
        std::map<NodeId, TransactionHandle> entries;

    public:
        bool CanAdmit(const Request& request) const noexcept;
        bool Add(const Transaction& transaction) noexcept;
        bool Release(TransactionHandle handle) noexcept;
        bool Empty() const noexcept { return entries.empty(); }
        bool Validate() const noexcept;
        void Clear() noexcept { entries.clear(); }
    };

}
