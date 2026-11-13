#include "chi_lunatic_snoop_id.hpp"

namespace CHI::Lunatic {

    std::optional<TransactionId> SnoopIdAllocator::Allocate(const Filter& filter)
    {
        constexpr size_t capacity = size_t(1) << 12;
        for (size_t attempt = 0; attempt < capacity; ++attempt)
        {
            const TransactionId candidate = TransactionId((next + attempt) % capacity);
            if (live.contains(candidate) || (filter && !filter(candidate)))
                continue;
            live.insert(candidate);
            next = TransactionId((candidate + 1) % capacity);
            return candidate;
        }
        return std::nullopt;
    }

    bool SnoopIdAllocator::Release(TransactionId txnId) noexcept
    {
        return live.erase(txnId) != 0;
    }

    void SnoopIdAllocator::Clear() noexcept
    {
        live.clear();
        next = 0;
    }

}
