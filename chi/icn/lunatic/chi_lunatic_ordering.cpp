#include "chi_lunatic_ordering.hpp"

namespace CHI::Lunatic {

    bool OrderingTracker::CanAdmit(const Request& request) const noexcept
    {
        return request.order == 0 || entries.find(request.source) == entries.end();
    }

    bool OrderingTracker::Add(const Transaction& transaction) noexcept
    {
        if (transaction.request.order == 0)
            return true;
        return entries.emplace(transaction.request.source, transaction.handle).second;
    }

    bool OrderingTracker::Release(TransactionHandle handle) noexcept
    {
        for (auto iter = entries.begin(); iter != entries.end(); ++iter)
        {
            if (iter->second == handle)
            {
                entries.erase(iter);
                return true;
            }
        }
        return false;
    }

    bool OrderingTracker::CanIssueDMT(const Transaction& transaction) const noexcept
    {
        const auto iter = downstreamDmtEntries.find(transaction.request.source);
        return iter == downstreamDmtEntries.end() || iter->second == transaction.handle;
    }

    void OrderingTracker::HoldDMT(const Transaction& transaction) noexcept
    {
        downstreamDmtEntries.emplace(transaction.request.source, transaction.handle);
    }

    void OrderingTracker::ReleaseDMT(TransactionHandle handle) noexcept
    {
        for (auto iter = downstreamDmtEntries.begin(); iter != downstreamDmtEntries.end(); ++iter)
        {
            if (iter->second == handle)
            {
                downstreamDmtEntries.erase(iter);
                return;
            }
        }
    }

    bool OrderingTracker::Validate() const noexcept
    {
        for (const auto& [source, handle] : entries)
        {
            (void)source;
            if (handle == INVALID_TRANSACTION)
                return false;
        }
        for (const auto& [source, handle] : downstreamDmtEntries)
        {
            (void)source;
            if (handle == INVALID_TRANSACTION)
                return false;
        }
        return true;
    }

}
