#pragma once

#include "chi_lunatic_transaction.hpp"

#include <map>
#include <utility>

namespace CHI::Lunatic {

    class OrderingTracker {
        using StreamKey = std::pair<NodeId, LineKey>;

        std::map<StreamKey, TransactionHandle> entries;
        std::map<StreamKey, TransactionHandle> downstreamDmtEntries;
        std::map<NodeId, TransactionHandle>    orderedWriteEntries;

    public:
        bool CanAdmit(const Request& request) const noexcept;
        bool Add(const Transaction& transaction) noexcept;
        bool Release(TransactionHandle handle) noexcept;
        bool CanIssueDMT(const Transaction& transaction) const noexcept;
        void HoldDMT(const Transaction& transaction) noexcept;
        void ReleaseDMT(TransactionHandle handle) noexcept;
        bool Empty() const noexcept
        {
            return entries.empty() && downstreamDmtEntries.empty() && orderedWriteEntries.empty();
        }
        bool Validate() const noexcept;
        void Clear() noexcept
        {
            entries.clear();
            downstreamDmtEntries.clear();
            orderedWriteEntries.clear();
        }
    };

}
