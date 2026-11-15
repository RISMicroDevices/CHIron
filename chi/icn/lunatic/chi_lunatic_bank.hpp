#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_types.hpp"

namespace CHI::Lunatic {

    class BankMap {
        size_t bankCount = 1;
        size_t bankShift = 0;

    public:
        explicit BankMap(size_t bankCount);
        explicit BankMap(const Options& options);

        size_t Count() const noexcept { return bankCount; }
        size_t Index(const LineKey& key) const noexcept;
        size_t SetIndex(const LineKey& key, size_t sets) const;
    };

}
