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

    bool OrderingTracker::Validate() const noexcept
    {
        for (const auto& [source, handle] : entries)
        {
            (void)source;
            if (handle == INVALID_TRANSACTION)
                return false;
        }
        return true;
    }

}
