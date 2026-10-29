#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_types.hpp"

#include <memory>

namespace CHI::Lunatic {

    struct PolicyContext {
        LineKey line{};
        NodeId  requester = 0;
        bool    homeValid = false;
        bool    homeDirty = false;
        bool    requesterValid = false;
        bool    requesterDirty = false;
        bool    requesterRetains = false;
        bool    evictionHasData = false;
    };

    struct PolicyDecision {
        bool allocateHome = false;
        bool retainHome = false;
        bool removeHome = false;
        bool mergeCopyback = false;
        bool backInvalidate = false;
    };

    class CachePolicy {
    public:
        virtual ~CachePolicy() = default;

        virtual PolicyKind     Kind() const noexcept = 0;
        virtual PolicyDecision OnRNFill(const PolicyContext& context) const = 0;
        virtual PolicyDecision OnRNEviction(const PolicyContext& context) const = 0;
        virtual PolicyDecision OnHomeAllocation(const PolicyContext& context) const = 0;
        virtual PolicyDecision OnHomeEviction(const PolicyContext& context) const = 0;
        virtual PolicyDecision OnCopyback(const PolicyContext& context) const = 0;
    };

    std::unique_ptr<CachePolicy> MakePolicy(PolicyKind kind);

}
