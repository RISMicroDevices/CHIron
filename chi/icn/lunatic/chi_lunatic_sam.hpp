#pragma once

#include "chi_lunatic_config.hpp"

namespace CHI::Lunatic {

    class SystemAddressMap {
    public:
        virtual ~SystemAddressMap() noexcept = default;
        virtual NodeId Map(uint64_t address, bool nonSecure) const noexcept = 0;
    };

    class FixedSystemAddressMap final : public SystemAddressMap {
        NodeId target;

    public:
        explicit FixedSystemAddressMap(NodeId target) : target(target) {}

        NodeId Map(uint64_t, bool) const noexcept override { return target; }
    };

    inline NodeId SelectSubordinate(const Options& options, uint64_t address, bool nonSecure) noexcept
    {
        return options.addressMap ? options.addressMap->Map(address, nonSecure) : options.subordinateId;
    }

}
