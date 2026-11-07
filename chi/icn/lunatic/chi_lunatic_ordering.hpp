#pragma once

#include "chi_lunatic_transaction.hpp"

#include <map>

namespace CHI::Lunatic {

    class OrderingTracker {
        std::map<NodeId, TransactionHandle> entries;
        std::map<NodeId, TransactionHandle> downstreamDmtEntries;

    public:
        bool CanAdmit(const Request& request) const noexcept;
        bool Add(const Transaction& transaction) noexcept;
        bool Release(TransactionHandle handle) noexcept;
        bool CanIssueDMT(const Transaction& transaction) const noexcept;
        void HoldDMT(const Transaction& transaction) noexcept;
        void ReleaseDMT(TransactionHandle handle) noexcept;
        bool Empty() const noexcept { return entries.empty() && downstreamDmtEntries.empty(); }
        bool Validate() const noexcept;
        void Clear() noexcept { entries.clear(); downstreamDmtEntries.clear(); }
    };

}
