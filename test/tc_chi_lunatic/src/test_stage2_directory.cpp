#include "common.hpp"
#include "chi/icn/lunatic/chi_lunatic_directory.hpp"
#include "chi/icn/lunatic/chi_lunatic_policy.hpp"
#include "chi/icn/lunatic/chi_lunatic_snoop_filter.hpp"

#include <limits>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

LineKey Line(uint64_t index, bool nonSecure = false)
{
    return LineKey::FromAddress(index * LINE_SIZE, nonSecure);
}

void CheckDecision(const PolicyDecision& actual, const PolicyDecision& expected)
{
    LUNATIC_CHECK(actual.allocateHome == expected.allocateHome);
    LUNATIC_CHECK(actual.retainHome == expected.retainHome);
    LUNATIC_CHECK(actual.removeHome == expected.removeHome);
    LUNATIC_CHECK(actual.mergeCopyback == expected.mergeCopyback);
    LUNATIC_CHECK(actual.backInvalidate == expected.backInvalidate);
}

}

LUNATIC_TEST(Stage2DirectoryHitMissAndNonPowerOfTwoMapping)
{
    Directory directory(3, 2);
    const LineKey line0 = Line(0);
    const LineKey line1 = Line(1);
    const LineKey line2 = Line(2);
    const LineKey line3 = Line(3);
    const LineKey secure0 = Line(0, false);
    const LineKey nonSecure0 = Line(0, true);

    LUNATIC_CHECK(directory.Sets() == 3);
    LUNATIC_CHECK(directory.Ways() == 2);
    LUNATIC_CHECK(directory.SetIndex(line0) == 0);
    LUNATIC_CHECK(directory.SetIndex(line1) == 1);
    LUNATIC_CHECK(directory.SetIndex(line2) == 2);
    LUNATIC_CHECK(directory.SetIndex(line3) == 0);
    LUNATIC_CHECK(directory.SetIndex(Line(8)) == 2);
    LUNATIC_CHECK(directory.SetIndex(LineKey{ LINE_SIZE + 1, false }) == 1);
    LUNATIC_CHECK(directory.SetIndex(secure0) == directory.SetIndex(nonSecure0));
    LUNATIC_CHECK(!directory.Find(line0));
    LUNATIC_CHECK(!directory.FindWay(line0));

    directory.Install(secure0, 0, 10).dirty = true;
    LUNATIC_CHECK(directory.Find(secure0));
    LUNATIC_CHECK(directory.FindWay(secure0) == 0);
    LUNATIC_CHECK(!directory.Find(nonSecure0));
    LUNATIC_CHECK(directory.ValidCount() == 1);

    directory.Install(nonSecure0, 1, 11);
    directory.Install(line3, 0, 12);
    LUNATIC_CHECK(directory.Find(nonSecure0));
    LUNATIC_CHECK(directory.FindWay(nonSecure0) == 1);
    LUNATIC_CHECK(!directory.Find(secure0));
    LUNATIC_CHECK(directory.Find(line3));
    LUNATIC_CHECK(directory.ValidCount() == 2);

    const Directory& constant = directory;
    LUNATIC_CHECK(constant.Find(line3));
    LUNATIC_CHECK(constant.FindWay(nonSecure0) == 1);
    LUNATIC_CHECK(!constant.Find(line1));
}

LUNATIC_TEST(Stage2DirectoryVictimOrderingAndFixedStorage)
{
    Directory directory(3, 3);
    const LineKey line0 = Line(0);
    const LineKey line3 = Line(3);
    const LineKey line6 = Line(6);
    const LineKey line9 = Line(9);
    const size_t set = directory.SetIndex(line0);

    LUNATIC_CHECK((directory.VictimCandidates(line0) == std::vector<size_t>{ 0, 1, 2 }));
    LUNATIC_CHECK(directory.ChooseVictim(line0) == 0);
    HomeLine* original = &directory.At(set, 0);

    directory.Install(line0, 2, 50);
    LUNATIC_CHECK(directory.ChooseVictim(line3) == 0);
    directory.Install(line3, 0, 20);
    LUNATIC_CHECK(directory.ChooseVictim(line6) == 1);
    directory.Install(line6, 1, 20);
    LUNATIC_CHECK(directory.ChooseVictim(line9) == 0);

    directory.At(set, 0).age = 30;
    LUNATIC_CHECK(directory.ChooseVictim(line9) == 1);
    directory.Invalidate(set, 2);
    LUNATIC_CHECK(directory.ChooseVictim(line9) == 2);
    LUNATIC_CHECK(original == &directory.At(set, 0));

    HomeLine& installed = directory.Install(line9, 2, 5);
    installed.data[0] = 0xaa;
    installed.validBytes[0] = 1;
    installed.dirty = true;
    directory.Invalidate(set, 2);
    LUNATIC_CHECK(!installed.valid);
    LUNATIC_CHECK(!installed.dirty);
    LUNATIC_CHECK(installed.age == 0);
    LUNATIC_CHECK(installed.data[0] == 0);
    LUNATIC_CHECK(installed.validBytes[0] == 0);

    directory.Install(line0, 0, 60);
    directory.At(set, 0).data[1] = 0xbb;
    directory.At(set, 0).validBytes[1] = 1;
    directory.At(set, 0).dirty = true;
    HomeLine& reinstalled = directory.Install(line3, 0, 61);
    LUNATIC_CHECK(reinstalled.valid);
    LUNATIC_CHECK(!reinstalled.dirty);
    LUNATIC_CHECK(reinstalled.data[1] == 0);
    LUNATIC_CHECK(reinstalled.validBytes[1] == 0);
    LUNATIC_CHECK(reinstalled.age == 61);
    LUNATIC_CHECK(original == &reinstalled);

    CheckThrows<std::out_of_range>([&] { directory.Install(line0, 3, 0); });
    directory.Clear();
    LUNATIC_CHECK(directory.ValidCount() == 0);
    LUNATIC_CHECK(!directory.Find(line3));
    LUNATIC_CHECK(original == &directory.At(set, 0));
}

LUNATIC_TEST(Stage2DirectoryOptionsAndInvalidGeometry)
{
    Options options;
    options.slcSets = 5;
    options.slcWays = 2;
    Directory directory(options);
    LUNATIC_CHECK(directory.Sets() == 5);
    LUNATIC_CHECK(directory.Ways() == 2);

    CheckThrows([] { Directory invalid(0, 1); });
    CheckThrows([] { Directory invalid(1, 0); });
    CheckThrows([] {
        Directory invalid(std::numeric_limits<size_t>::max(), 2);
    });
}

LUNATIC_TEST(Stage2PreciseSnoopFilterStatesAndVectors)
{
    SnoopFilter filter(3, 2, true);
    const LineKey key = Line(4);

    LUNATIC_CHECK(filter.IsPrecise());
    LUNATIC_CHECK(filter.SetIndex(key) == 1);
    LUNATIC_CHECK(!filter.Find(key));
    LUNATIC_CHECK(!filter.FindWay(key));
    LUNATIC_CHECK(filter.State(key, 1) == CacheState::I);
    LUNATIC_CHECK(filter.Sharers(key).empty());
    LUNATIC_CHECK(filter.Targets(key).empty());

    filter.Track(key, 9, CacheState::I, 1);
    LUNATIC_CHECK(!filter.Find(key));
    LUNATIC_CHECK(filter.ValidCount() == 0);

    filter.Track(key, 9, CacheState::SC, 10);
    filter.Track(key, 2, CacheState::SD, 11);
    filter.Track(key, 7, CacheState::UC, 12);
    filter.Track(key, 4, CacheState::UD, 13);
    filter.Track(key, 5, CacheState::I, 14);

    LUNATIC_CHECK(filter.Find(key));
    LUNATIC_CHECK(filter.FindWay(key) == 0);
    LUNATIC_CHECK(filter.ValidCount() == 1);
    LUNATIC_CHECK(filter.Find(key)->requesters.size() == 4);
    LUNATIC_CHECK(filter.Find(key)->age == 13);
    LUNATIC_CHECK(filter.State(key, 9) == CacheState::SC);
    LUNATIC_CHECK(filter.State(key, 2) == CacheState::SD);
    LUNATIC_CHECK(filter.State(key, 7) == CacheState::UC);
    LUNATIC_CHECK(filter.State(key, 4) == CacheState::UD);
    LUNATIC_CHECK(filter.State(key, 5) == CacheState::I);
    LUNATIC_CHECK(filter.HasRequester(key, 4));
    LUNATIC_CHECK(!filter.HasRequester(key, 5));
    LUNATIC_CHECK((filter.Sharers(key) == std::vector<NodeId>{ 2, 9 }));
    LUNATIC_CHECK((filter.Targets(key) == std::vector<NodeId>{ 2, 4, 7, 9 }));

    filter.Track(key, 7, CacheState::I, 15);
    LUNATIC_CHECK(filter.State(key, 7) == CacheState::I);
    LUNATIC_CHECK(!filter.HasRequester(key, 7));
    LUNATIC_CHECK((filter.Targets(key) == std::vector<NodeId>{ 2, 4, 9 }));

    filter.Remove(key, 2);
    filter.Remove(key, 4);
    LUNATIC_CHECK(filter.ValidCount() == 1);
    LUNATIC_CHECK((filter.Targets(key) == std::vector<NodeId>{ 9 }));
    filter.Remove(key, 9);
    LUNATIC_CHECK(!filter.Find(key));
    LUNATIC_CHECK(filter.ValidCount() == 0);
    filter.Remove(key, 9);
}

LUNATIC_TEST(Stage2LossySnoopFilterRetainsInvalidTargets)
{
    SnoopFilter filter(2, 2, false);
    const LineKey key = Line(1);
    const LineKey other = Line(2);

    LUNATIC_CHECK(!filter.IsPrecise());
    filter.Track(key, 8, CacheState::UC, 0);
    filter.Track(key, 8, CacheState::I, 1);
    filter.Track(key, 3, CacheState::SC, 2);
    filter.Track(key, 5, CacheState::UC, 3);

    LUNATIC_CHECK(filter.ValidCount() == 1);
    LUNATIC_CHECK(filter.HasRequester(key, 8));
    LUNATIC_CHECK(filter.State(key, 8) == CacheState::I);
    LUNATIC_CHECK((filter.Targets(key) == std::vector<NodeId>{ 3, 5, 8 }));
    LUNATIC_CHECK((filter.Sharers(key) == std::vector<NodeId>{ 3 }));

    filter.Remove(key, 3);
    LUNATIC_CHECK(filter.Sharers(key).empty());
    LUNATIC_CHECK((filter.Targets(key) == std::vector<NodeId>{ 5, 8 }));

    filter.Track(other, 5, CacheState::SD, 4);
    filter.RemoveRequester(5);
    LUNATIC_CHECK(!filter.HasRequester(key, 5));
    LUNATIC_CHECK(!filter.Find(other));
    LUNATIC_CHECK((filter.Targets(key) == std::vector<NodeId>{ 8 }));

    filter.RemoveRequester(8);
    LUNATIC_CHECK(!filter.Find(key));
    LUNATIC_CHECK(filter.ValidCount() == 0);
}

LUNATIC_TEST(Stage2SnoopFilterReplacementAndFixedStorage)
{
    for (bool precise : { true, false }) {
        SnoopFilter filter(3, 3, precise);
        const LineKey line0 = Line(0);
        const LineKey line3 = Line(3);
        const LineKey line6 = Line(6);
        const LineKey line9 = Line(9);
        const LineKey line12 = Line(12);
        const size_t set = filter.SetIndex(line0);

        LUNATIC_CHECK((filter.VictimCandidates(line0) == std::vector<size_t>{ 0, 1, 2 }));
        LUNATIC_CHECK(filter.ChooseVictim(line0) == 0);
        SnoopFilterEntry* original = &filter.At(set, 0);

        filter.Track(line0, 1, CacheState::SC, 10);
        LUNATIC_CHECK(filter.ChooseVictim(line3) == 1);
        filter.Track(line3, 1, CacheState::SC, 10);
        LUNATIC_CHECK(filter.ChooseVictim(line6) == 2);
        filter.Track(line6, 1, CacheState::SC, 20);
        LUNATIC_CHECK(filter.ChooseVictim(line9) == 0);

        filter.At(set, 0).age = 30;
        LUNATIC_CHECK(filter.ChooseVictim(line9) == 1);
        filter.Remove(line3, 1);
        LUNATIC_CHECK(filter.ChooseVictim(line9) == 1);

        filter.Track(line9, 2, CacheState::UC, 5);
        LUNATIC_CHECK(!filter.Find(line3));
        LUNATIC_CHECK(filter.FindWay(line9) == 1);
        LUNATIC_CHECK(filter.ValidCount() == 3);
        LUNATIC_CHECK(original == &filter.At(set, 0));

        filter.Track(line0, 3, CacheState::UD, 1);
        LUNATIC_CHECK(filter.Find(line0)->age == 1);
        LUNATIC_CHECK(filter.ChooseVictim(line12) == 0);
        CheckThrows<std::overflow_error>([&] { filter.Track(line12, 4, CacheState::SC, 2); });
        filter.Invalidate(filter.SetIndex(line0), *filter.FindWay(line0));
        LUNATIC_CHECK(!filter.Find(line0));
        filter.Track(line12, 4, CacheState::SC, 2);
        LUNATIC_CHECK(filter.FindWay(line12) == 0);
        LUNATIC_CHECK(filter.Find(line6));
        LUNATIC_CHECK(original == &filter.At(set, 0));

        filter.Clear();
        LUNATIC_CHECK(filter.ValidCount() == 0);
        LUNATIC_CHECK(filter.Targets(line6).empty());
        LUNATIC_CHECK(filter.At(set, 0).requesters.empty());
        LUNATIC_CHECK(original == &filter.At(set, 0));
    }
}

LUNATIC_TEST(Stage2SnoopFilterOptionsAndInvalidGeometry)
{
    Options options;
    options.snoopFilterSets = 5;
    options.snoopFilterWays = 2;
    options.preciseSnoopFilter = false;
    SnoopFilter filter(options);
    LUNATIC_CHECK(filter.SetIndex(Line(7)) == 2);
    LUNATIC_CHECK(!filter.IsPrecise());

    CheckThrows([] { SnoopFilter invalid(0, 1); });
    CheckThrows([] { SnoopFilter invalid(1, 0); });
    CheckThrows([] {
        SnoopFilter invalid(std::numeric_limits<size_t>::max(), 2);
    });
}

LUNATIC_TEST(Stage2HomeDataAndRequesterPresenceAreIndependent)
{
    Directory directory(1, 1);
    SnoopFilter filter(1, 1, true);
    const LineKey key = Line(0);

    directory.Install(key, 0, 1).data[0] = 0x5a;
    filter.Track(key, 3, CacheState::SC, 1);
    LUNATIC_CHECK(directory.Find(key));
    LUNATIC_CHECK(filter.HasRequester(key, 3));

    filter.Remove(key, 3);
    LUNATIC_CHECK(directory.Find(key));
    LUNATIC_CHECK(directory.Find(key)->data[0] == 0x5a);
    LUNATIC_CHECK(filter.Targets(key).empty());

    filter.Track(key, 4, CacheState::UD, 2);
    directory.Invalidate(0, 0);
    LUNATIC_CHECK(!directory.Find(key));
    LUNATIC_CHECK(filter.HasRequester(key, 4));
    LUNATIC_CHECK((filter.Targets(key) == std::vector<NodeId>{ 4 }));
}

LUNATIC_TEST(Stage2NinePolicyDecisions)
{
    const auto policy = MakePolicy(PolicyKind::NINE);
    LUNATIC_CHECK(policy->Kind() == PolicyKind::NINE);

    PolicyContext context;
    context.requesterValid = true;
    CheckDecision(policy->OnRNFill(context), { true, false, false, false, false });
    context.homeValid = true;
    context.homeDirty = true;
    CheckDecision(policy->OnRNFill(context), { false, true, false, false, false });

    context.evictionHasData = false;
    CheckDecision(policy->OnRNEviction(context), { false, true, false, false, false });
    context.evictionHasData = true;
    context.requesterDirty = true;
    CheckDecision(policy->OnRNEviction(context), { false, true, false, true, false });
    context.homeValid = false;
    CheckDecision(policy->OnRNEviction(context), { true, false, false, true, false });
    context.evictionHasData = false;
    CheckDecision(policy->OnRNEviction(context), { false, false, false, false, false });

    CheckDecision(policy->OnHomeAllocation(context), { true, false, false, false, false });
    context.homeValid = true;
    CheckDecision(policy->OnHomeAllocation(context), { false, true, false, false, false });
    CheckDecision(policy->OnHomeEviction(context), { false, false, true, false, false });
    context.homeValid = false;
    CheckDecision(policy->OnHomeEviction(context), { false, false, false, false, false });

    context.requesterDirty = true;
    CheckDecision(policy->OnCopyback(context), { true, false, false, true, false });
    context.homeValid = true;
    context.requesterDirty = false;
    context.evictionHasData = true;
    CheckDecision(policy->OnCopyback(context), { false, true, false, true, false });
    context.evictionHasData = false;
    CheckDecision(policy->OnCopyback(context), { false, true, false, false, false });
}

LUNATIC_TEST(Stage2ExclusivePolicyDecisions)
{
    const auto policy = MakePolicy(PolicyKind::Exclusive);
    LUNATIC_CHECK(policy->Kind() == PolicyKind::Exclusive);

    PolicyContext context;
    context.requesterValid = true;
    CheckDecision(policy->OnRNFill(context), { false, false, false, false, false });
    context.homeValid = true;
    CheckDecision(policy->OnRNFill(context), { false, false, true, false, false });

    context.homeValid = false;
    CheckDecision(policy->OnRNEviction(context), { true, false, false, false, false });
    context.evictionHasData = true;
    CheckDecision(policy->OnRNEviction(context), { true, false, false, true, false });
    context.homeValid = true;
    CheckDecision(policy->OnRNEviction(context), { false, true, false, true, false });

    context.requesterValid = false;
    context.evictionHasData = false;
    CheckDecision(policy->OnHomeAllocation(context), { false, true, false, false, false });
    context.homeValid = false;
    CheckDecision(policy->OnHomeAllocation(context), { true, false, false, false, false });
    context.requesterValid = true;
    CheckDecision(policy->OnHomeAllocation(context), { false, false, false, false, false });
    context.homeValid = true;
    CheckDecision(policy->OnHomeAllocation(context), { false, false, true, false, false });

    CheckDecision(policy->OnHomeEviction(context), { false, false, true, false, false });
    context.homeValid = false;
    CheckDecision(policy->OnHomeEviction(context), { false, false, false, false, false });

    context.requesterDirty = true;
    CheckDecision(policy->OnCopyback(context), { true, false, false, true, false });
    context.homeValid = true;
    context.requesterDirty = false;
    context.evictionHasData = true;
    CheckDecision(policy->OnCopyback(context), { false, true, false, true, false });
}

LUNATIC_TEST(Stage2InclusivePolicyDecisions)
{
    const auto policy = MakePolicy(PolicyKind::Inclusive);
    LUNATIC_CHECK(policy->Kind() == PolicyKind::Inclusive);

    PolicyContext context;
    context.requesterValid = true;
    CheckDecision(policy->OnRNFill(context), { true, false, false, false, false });
    context.homeValid = true;
    CheckDecision(policy->OnRNFill(context), { false, true, false, false, false });

    CheckDecision(policy->OnRNEviction(context), { false, true, false, false, false });
    context.evictionHasData = true;
    CheckDecision(policy->OnRNEviction(context), { false, true, false, true, false });
    context.homeValid = false;
    CheckDecision(policy->OnRNEviction(context), { true, false, false, true, false });
    context.evictionHasData = false;
    CheckDecision(policy->OnRNEviction(context), { false, false, false, false, false });

    CheckDecision(policy->OnHomeAllocation(context), { true, false, false, false, false });
    context.homeValid = true;
    CheckDecision(policy->OnHomeAllocation(context), { false, true, false, false, false });

    context.requesterValid = true;
    CheckDecision(policy->OnHomeEviction(context), { false, false, true, false, true });
    context.requesterValid = false;
    CheckDecision(policy->OnHomeEviction(context), { false, false, true, false, false });
    context.homeValid = false;
    CheckDecision(policy->OnHomeEviction(context), { false, false, false, false, false });

    context.requesterDirty = true;
    CheckDecision(policy->OnCopyback(context), { true, false, false, true, false });
    context.homeValid = true;
    context.requesterDirty = false;
    context.evictionHasData = true;
    CheckDecision(policy->OnCopyback(context), { false, true, false, true, false });
    context.evictionHasData = false;
    CheckDecision(policy->OnCopyback(context), { false, true, false, false, false });
}

LUNATIC_TEST(Stage2PolicyFactoryRejectsInvalidKind)
{
    CheckThrows([] { MakePolicy(static_cast<PolicyKind>(0xff)); });
}
