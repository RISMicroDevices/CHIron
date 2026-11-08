#include "chi_lunatic_ordering.hpp"
#include "chi_lunatic_protocol.hpp"

namespace CHI::Lunatic {

    bool OrderingTracker::CanAdmit(const Request& request) const noexcept
    {
        if (request.order == 0)
            return true;
        if (IsOrderedWriteObservation(request))
            return orderedWriteEntries.find(request.source) == orderedWriteEntries.end();
        return entries.find({ request.source, request.Line() }) == entries.end();
    }

    bool OrderingTracker::Add(const Transaction& transaction) noexcept
    {
        if (transaction.request.order == 0)
            return true;
        if (IsOrderedWriteObservation(transaction.request))
            return orderedWriteEntries.emplace(transaction.request.source, transaction.handle).second;
        return entries.emplace(StreamKey{ transaction.request.source, transaction.request.Line() },
                               transaction.handle).second;
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
        for (auto iter = orderedWriteEntries.begin(); iter != orderedWriteEntries.end(); ++iter)
        {
            if (iter->second == handle)
            {
                orderedWriteEntries.erase(iter);
                return true;
            }
        }
        return false;
    }

    bool OrderingTracker::CanIssueDMT(const Transaction& transaction) const noexcept
    {
        const auto iter = downstreamDmtEntries.find({ transaction.request.source, transaction.request.Line() });
        return iter == downstreamDmtEntries.end() || iter->second == transaction.handle;
    }

    void OrderingTracker::HoldDMT(const Transaction& transaction) noexcept
    {
        downstreamDmtEntries.emplace(StreamKey{ transaction.request.source, transaction.request.Line() },
                                     transaction.handle);
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
        for (const auto& [key, handle] : entries)
        {
            (void)key;
            if (handle == INVALID_TRANSACTION)
                return false;
        }
        for (const auto& [key, handle] : downstreamDmtEntries)
        {
            (void)key;
            if (handle == INVALID_TRANSACTION)
                return false;
        }
        for (const auto& [source, handle] : orderedWriteEntries)
        {
            (void)source;
            if (handle == INVALID_TRANSACTION)
                return false;
        }
        return true;
    }

}
