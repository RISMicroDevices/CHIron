#include "common.hpp"
#include "chi/icn/lunatic/chi_lunatic_policy.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void CheckDecision(const PolicyDecision& actual, const PolicyDecision& expected)
    {
        LUNATIC_CHECK(actual.allocateHome == expected.allocateHome);
        LUNATIC_CHECK(actual.retainHome == expected.retainHome);
        LUNATIC_CHECK(actual.removeHome == expected.removeHome);
        LUNATIC_CHECK(actual.mergeCopyback == expected.mergeCopyback);
        LUNATIC_CHECK(actual.backInvalidate == expected.backInvalidate);
        LUNATIC_CHECK(actual.requestData == expected.requestData);
    }

    void CheckImmediateWrite(PolicyKind kind)
    {
        const auto policy = MakePolicy(kind);
        PolicyContext context;
        context.requesterValid = true;

        CheckDecision(policy->OnImmediateWrite(context), { false, false, false, true, false, false });
        context.allocationHint = true;
        CheckDecision(policy->OnImmediateWrite(context), { true, false, false, true, false, false });

        context.homeValid = true;
        CheckDecision(policy->OnImmediateWrite(context), { false, true, false, true, false, false });
        context.allocationHint = false;
        CheckDecision(policy->OnImmediateWrite(context), { false, true, false, true, false, false });
    }

    void CheckEvictOrEvict(PolicyKind kind, const PolicyDecision& requesterInvalid)
    {
        const auto policy = MakePolicy(kind);
        PolicyContext context;
        context.requesterDirty = true;
        context.evictionHasData = true;
        CheckDecision(policy->OnEvictOrEvict(context), requesterInvalid);

        context = {};
        context.requesterValid = true;
        context.homeValid = true;
        context.evictionHasData = true;
        CheckDecision(policy->OnEvictOrEvict(context), { false, true, false, false, false, false });

        context = {};
        context.requesterValid = true;
        CheckDecision(policy->OnEvictOrEvict(context), { true, false, false, false, false, true });

        context.requesterDirty = true;
        CheckDecision(policy->OnEvictOrEvict(context), { true, false, false, true, false, true });
        context.homeValid = true;
        CheckDecision(policy->OnEvictOrEvict(context), { false, true, false, true, false, true });
    }

}

LUNATIC_TEST(Stage5NineImmediateWriteDecisions)
{
    CheckImmediateWrite(PolicyKind::NINE);
}

LUNATIC_TEST(Stage5ExclusiveImmediateWriteDecisions)
{
    CheckImmediateWrite(PolicyKind::Exclusive);
}

LUNATIC_TEST(Stage5InclusiveImmediateWriteDecisions)
{
    CheckImmediateWrite(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5NineEvictOrEvictDecisions)
{
    CheckEvictOrEvict(PolicyKind::NINE, {});
}

LUNATIC_TEST(Stage5ExclusiveEvictOrEvictDecisions)
{
    CheckEvictOrEvict(PolicyKind::Exclusive, {});
}

LUNATIC_TEST(Stage5InclusiveEvictOrEvictDecisions)
{
    CheckEvictOrEvict(PolicyKind::Inclusive, {});
}
