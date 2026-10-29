#include "chi_lunatic_policy.hpp"

#include <stdexcept>

namespace CHI::Lunatic {
    namespace {

        PolicyDecision                  KeepHome(const PolicyContext& context)
        {
            PolicyDecision decision;
            decision.allocateHome = !context.homeValid;
            decision.retainHome = context.homeValid;
            return decision;
        }

        PolicyDecision                  MergeEviction(const PolicyContext& context)
        {
            PolicyDecision decision = KeepHome(context);
            decision.mergeCopyback = context.evictionHasData;
            return decision;
        }

        PolicyDecision                  MergeCopyback(const PolicyContext& context)
        {
            PolicyDecision decision = KeepHome(context);
            decision.mergeCopyback = context.requesterDirty || context.evictionHasData;
            return decision;
        }

        class NinePolicy final : public CachePolicy {
        public:
            PolicyKind                  Kind() const noexcept override { return PolicyKind::NINE; }

            PolicyDecision              OnRNFill(const PolicyContext& context) const override
            {
                return KeepHome(context);
            }

            PolicyDecision              OnRNEviction(const PolicyContext& context) const override
            {
                PolicyDecision decision;
                decision.allocateHome = !context.homeValid && context.evictionHasData;
                decision.retainHome = context.homeValid;
                decision.mergeCopyback = context.evictionHasData;
                return decision;
            }

            PolicyDecision              OnHomeAllocation(const PolicyContext& context) const override
            {
                return KeepHome(context);
            }

            PolicyDecision              OnHomeEviction(const PolicyContext& context) const override
            {
                PolicyDecision decision;
                decision.removeHome = context.homeValid;
                return decision;
            }

            PolicyDecision              OnCopyback(const PolicyContext& context) const override
            {
                return MergeCopyback(context);
            }
        };

        class ExclusivePolicy final : public CachePolicy {
        public:
            PolicyKind                  Kind() const noexcept override { return PolicyKind::Exclusive; }

            PolicyDecision              OnRNFill(const PolicyContext& context) const override
            {
                PolicyDecision decision;
                decision.removeHome = context.homeValid;
                return decision;
            }

            PolicyDecision              OnRNEviction(const PolicyContext& context) const override
            {
                return MergeEviction(context);
            }

            PolicyDecision              OnHomeAllocation(const PolicyContext& context) const override
            {
                if (!context.requesterValid)
                    return KeepHome(context);

                PolicyDecision decision;
                decision.removeHome = context.homeValid;
                return decision;
            }

            PolicyDecision              OnHomeEviction(const PolicyContext& context) const override
            {
                PolicyDecision decision;
                decision.removeHome = context.homeValid;
                return decision;
            }

            PolicyDecision              OnCopyback(const PolicyContext& context) const override
            {
                if (context.requesterRetains)
                {
                    PolicyDecision decision;
                    decision.removeHome = context.homeValid;
                    return decision;
                }
                return MergeCopyback(context);
            }
        };

        class InclusivePolicy final : public CachePolicy {
        public:
            PolicyKind                  Kind() const noexcept override { return PolicyKind::Inclusive; }

            PolicyDecision              OnRNFill(const PolicyContext& context) const override
            {
                return KeepHome(context);
            }

            PolicyDecision              OnRNEviction(const PolicyContext& context) const override
            {
                PolicyDecision decision;
                decision.allocateHome = !context.homeValid && context.evictionHasData;
                decision.retainHome = context.homeValid;
                decision.mergeCopyback = context.evictionHasData;
                return decision;
            }

            PolicyDecision              OnHomeAllocation(const PolicyContext& context) const override
            {
                return KeepHome(context);
            }

            PolicyDecision              OnHomeEviction(const PolicyContext& context) const override
            {
                PolicyDecision decision;
                decision.removeHome = context.homeValid;
                decision.backInvalidate = context.requesterValid;
                return decision;
            }

            PolicyDecision              OnCopyback(const PolicyContext& context) const override
            {
                return MergeCopyback(context);
            }
        };

    }

    std::unique_ptr<CachePolicy>        MakePolicy(PolicyKind kind)
    {
        switch (kind) {
            case PolicyKind::NINE:
                return std::make_unique<NinePolicy>();
            case PolicyKind::Exclusive:
                return std::make_unique<ExclusivePolicy>();
            case PolicyKind::Inclusive:
                return std::make_unique<InclusivePolicy>();
        }
        throw std::invalid_argument("Lunatic home policy is invalid");
    }

}
