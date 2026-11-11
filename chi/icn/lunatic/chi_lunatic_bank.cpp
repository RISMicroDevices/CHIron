#include "chi_lunatic_bank.hpp"

#include <limits>
#include <stdexcept>

namespace CHI::Lunatic {

    BankMap::BankMap(size_t count)
    {
        if (!count || (count & (count - 1)) != 0)
            throw std::invalid_argument("Lunatic bank count must be a non-zero power of two");

        size_t bits = 0;
        while ((size_t(1) << bits) < count)
            ++bits;
        if (bits >= std::numeric_limits<size_t>::digits - 6)
            throw std::invalid_argument("Lunatic bank geometry exceeds the address width");

        bankCount = count;
        bankShift = bits;
    }

    BankMap::BankMap(const Options& options)
        : BankMap(options.structure.bankCount)
    {
    }

    size_t BankMap::Index(const LineKey& key) const noexcept
    {
        return (key.address >> 6) & (bankCount - 1);
    }

    size_t BankMap::SetIndex(const LineKey& key, size_t sets) const
    {
        if (!sets)
            throw std::invalid_argument("Lunatic bank set count must be non-zero");

        return (key.address >> (6 + bankShift)) % sets;
    }

}
