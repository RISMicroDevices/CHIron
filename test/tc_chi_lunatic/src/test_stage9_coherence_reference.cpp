#include "oracle_driver.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;

    LineData ReferencePattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 23 + index / 7);
        return data;
    }

    ByteEnables FullReferenceBytes()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    bool ReferenceUnique(CacheState state)
    {
        return state == CacheState::UC || state == CacheState::UCE ||
            state == CacheState::UD || state == CacheState::UDP;
    }

    bool ReferenceDirty(CacheState state)
    {
        return state == CacheState::UD || state == CacheState::UDP || state == CacheState::SD;
    }

    class CoherenceReference {
        TestDriver&             driver;
        std::vector<LineKey>    lines;
        std::array<NodeId, 4>   requesters{ 4, 5, 6, 7 };

        template<typename Event>
        void Observe(Gravity::EventBus<Event>& bus, const std::string& name)
        {
            bus.Register(Gravity::MakeListener<Event>(name, 0, [&](Event&) { CheckTemporal(); }));
        }

    public:
        size_t eventObservations = 0;

        CoherenceReference(TestDriver& driver, std::vector<LineKey> lines)
            : driver(driver), lines(std::move(lines))
        {
            const auto events = driver.driver.node.GetEvents();
            Observe(events->OnAcceptedRXREQ, "coherence-rxreq");
            Observe(events->OnAcceptedRXRSP, "coherence-rxrsp");
            Observe(events->OnAcceptedRXDAT, "coherence-rxdat");
            Observe(events->OnEmittedTXRSP, "coherence-txrsp");
            Observe(events->OnEmittedTXDAT, "coherence-txdat");
            Observe(events->OnEmittedTXSNP, "coherence-txsnp");
            Observe(events->OnRetired, "coherence-retired");
            driver.driver.node.GetEvents()->OnTransactionSchedule.Register(
                Gravity::MakeListener<TransactionScheduleEvent>("coherence-schedule", 0,
                    [&](TransactionScheduleEvent&) { CheckTemporal(); }));
        }

        void CheckTemporal()
        {
            ++eventObservations;
            for (const LineKey& line : lines)
            {
                const auto home = driver.driver.node.InspectHome(line);
                size_t unique = 0;
                size_t shared = 0;
                size_t dirty = 0;
                for (const NodeId requester : requesters)
                {
                    const CacheState state = driver.driver.node.InspectRequester(line, requester);
                    unique += ReferenceUnique(state) ? 1 : 0;
                    shared += state == CacheState::SC || state == CacheState::SD ? 1 : 0;
                    dirty += ReferenceDirty(state) ? 1 : 0;
                }
                LUNATIC_CHECK(unique <= 1);
                LUNATIC_CHECK(unique == 0 || shared == 0);
                LUNATIC_CHECK(dirty <= 1);
                LUNATIC_CHECK(!home.dirty || home.valid);
                LUNATIC_CHECK(!home.dirty || dirty == 0);
            }
        }

        void CheckStable(const LineKey& line,
            std::initializer_list<std::pair<NodeId, CacheState>> states,
            bool homeValid, bool homeDirty, const LineData& coherentData,
            const LineData& memoryData, NodeId dirtyRequester = 0)
        {
            std::array<CacheState, 4> expected{};
            expected.fill(CacheState::I);
            for (const auto& [requester, state] : states)
            {
                LUNATIC_CHECK(requester >= 4 && requester <= 7);
                expected[requester - 4] = state;
            }
            size_t dirty = 0;
            for (size_t index = 0; index < requesters.size(); ++index)
            {
                const CacheState actual = driver.driver.node.InspectRequester(line, requesters[index]);
                if (actual != expected[index])
                    throw std::runtime_error("reference state mismatch: line=" +
                        std::to_string(line.address) + " requester=" + std::to_string(requesters[index]) +
                        " actual=" + std::to_string(static_cast<unsigned>(actual)) +
                        " expected=" + std::to_string(static_cast<unsigned>(expected[index])));
                dirty += ReferenceDirty(actual) ? 1 : 0;
            }
            const auto home = driver.driver.node.InspectHome(line);
            LUNATIC_CHECK(home.valid == homeValid);
            LUNATIC_CHECK(home.dirty == homeDirty);
            LUNATIC_CHECK(!homeValid || home.data == coherentData);
            LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == memoryData);
            LUNATIC_CHECK(!homeDirty || dirtyRequester == 0);
            LUNATIC_CHECK(dirty == (dirtyRequester ? 1 : 0));
            if (dirtyRequester)
                LUNATIC_CHECK(ReferenceDirty(driver.driver.node.InspectRequester(line, dirtyRequester)));
            CheckTemporal();
        }
    };

    LineData ReceiveReferenceData(TestDriver& driver, NodeId source, TransactionId txnId,
        uint8_t expectedResp, bool acknowledge = true, DataBufferId* returnedDBID = nullptr)
    {
        LineData result{};
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source && data.TxnID() == txnId);
            LUNATIC_CHECK(data.Resp() == expectedResp);
            LUNATIC_CHECK(data.DataID() == beat * 2);
            dbid = data.DBID();
            for (size_t index = 0; index < 32; ++index)
                result[beat * 32 + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        if (acknowledge)
            driver.CompAck(source, dbid);
        if (returnedDBID)
            *returnedDBID = dbid;
        return result;
    }

    void SeedHomeDirty(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 9, 90, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(9, 90, response.DBID(), data, FullReferenceBytes(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    void SeedUnique(TestDriver& driver, const LineKey& line, NodeId source,
        TransactionId txnId, const LineData& data)
    {
        SeedHomeDirty(driver, line, data);
        LUNATIC_CHECK(driver.ReadData(source, txnId, line.address,
            CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage9ReferenceUniqueDirtyHandoff)
{
    Options options;
    options.policy = PolicyKind::NINE;
    TestDriver driver(options);
    const LineKey line = LineKey::FromAddress(0xb0000, false);
    const LineData initial = ReferencePattern(17);
    const LineData dirty = ReferencePattern(131);
    driver.driver.node.GetMemory().WriteFull(line, initial);
    CoherenceReference reference(driver, { line });

    SeedHomeDirty(driver, line, dirty);
    reference.CheckStable(line, {}, true, true, dirty, initial);
    LUNATIC_CHECK(driver.ReadData(4, 1, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
    driver.Quiesce();
    reference.CheckStable(line, { { 4, CacheState::UD } }, true, false, dirty, initial, 4);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 2, line.address, true));
    const auto unique = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(unique.target == 4);
    driver.SnoopData(4, unique.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_I_PD);
    LUNATIC_CHECK(ReceiveReferenceData(driver, 5, 2, CHI::Eb::Resps::CompData_UD_PD) == dirty);
    driver.Quiesce();
    reference.CheckStable(line, { { 5, CacheState::UD } }, true, false, dirty, initial, 5);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 6, 3, line.address));
    const auto clean = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
    LUNATIC_CHECK(clean.target == 5);
    driver.SnoopData(5, clean.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_UC_PD);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 3);
    driver.Quiesce();
    reference.CheckStable(line, { { 5, CacheState::UC } }, true, false, dirty, dirty);
    LUNATIC_CHECK(reference.eventObservations != 0);
}

LUNATIC_TEST(Stage9ReferenceThreeTargetAggregation)
{
    Options options;
    options.policy = PolicyKind::NINE;
    TestDriver driver(options);
    const LineKey line = LineKey::FromAddress(0xb1000, false);
    const LineData initial = ReferencePattern(29);
    const LineData dirty = ReferencePattern(149);
    driver.driver.node.GetMemory().WriteFull(line, initial);
    CoherenceReference reference(driver, { line });
    SeedUnique(driver, line, 4, 10, dirty);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 11, line.address, true));
    const auto first = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    LUNATIC_CHECK(first.target == 4);
    driver.SnoopData(4, first.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_SC_PD);
    LUNATIC_CHECK(ReceiveReferenceData(driver, 5, 11, CHI::Eb::Resps::CompData_SD_PD) == dirty);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 6, 12, line.address, true));
    const auto second = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    LUNATIC_CHECK(second.target == 5);
    driver.SnoopData(5, second.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_SD);
    LUNATIC_CHECK(ReceiveReferenceData(driver, 6, 12, CHI::Eb::Resps::CompData_SC) == dirty);
    driver.Quiesce();
    reference.CheckStable(line, { { 4, CacheState::SC }, { 5, CacheState::SD },
        { 6, CacheState::SC } }, true, false, dirty, initial, 5);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 7, 13, line.address, true));
    const std::array snoops{
        driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique),
        driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique),
        driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique)
    };
    const auto byTarget = [&](NodeId target) -> const TestDriver::SNP& {
        const auto it = std::find_if(snoops.begin(), snoops.end(),
            [&](const auto& snoop) { return snoop.target == target; });
        LUNATIC_CHECK(it != snoops.end());
        return *it;
    };
    std::vector<NodeId> targets;
    for (const auto& snoop : snoops)
        targets.push_back(snoop.target);
    std::sort(targets.begin(), targets.end());
    LUNATIC_CHECK(targets == std::vector<NodeId>({ 4, 5, 6 }));
    driver.SnoopData(5, byTarget(5).flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_I_PD);
    driver.SnoopResponse(4, byTarget(4).flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    for (size_t cycle = 0; cycle < 6; ++cycle)
    {
        driver.driver.Tick();
        LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == initial);
    }
    reference.CheckStable(line, { { 4, CacheState::I }, { 5, CacheState::I },
        { 6, CacheState::SC } }, true, false, dirty, initial);
    driver.SnoopResponse(6, byTarget(6).flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    LUNATIC_CHECK(ReceiveReferenceData(driver, 7, 13, CHI::Eb::Resps::CompData_UD_PD) == dirty);
    driver.Quiesce();
    reference.CheckStable(line, { { 7, CacheState::UD } }, true, false, dirty, initial, 7);
    LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
    LUNATIC_CHECK(reference.eventObservations != 0);
}

LUNATIC_TEST(Stage9ReferenceHomeDirtyWithoutSnoopData)
{
    Options options;
    options.policy = PolicyKind::NINE;
    TestDriver driver(options);
    const LineKey line = LineKey::FromAddress(0xb2000, false);
    const LineData initial = ReferencePattern(43);
    const LineData dirty = ReferencePattern(163);
    driver.driver.node.GetMemory().WriteFull(line, initial);
    CoherenceReference reference(driver, { line });
    SeedHomeDirty(driver, line, dirty);

    LUNATIC_CHECK(driver.ReadData(5, 20, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == dirty);
    driver.Quiesce();
    reference.CheckStable(line, { { 5, CacheState::SC } }, true, true, dirty, initial);
    LUNATIC_CHECK(driver.ReadData(6, 21, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == dirty);
    driver.Quiesce();
    reference.CheckStable(line, { { 5, CacheState::SC }, { 6, CacheState::SC } },
        true, true, dirty, initial);
    LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 7, 22, line.address, true));
    const auto first = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    const auto second = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    std::vector<NodeId> targets{ first.target, second.target };
    std::sort(targets.begin(), targets.end());
    LUNATIC_CHECK(targets == std::vector<NodeId>({ 5, 6 }));
    driver.SnoopResponse(second.target, second.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    driver.SnoopResponse(first.target, first.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    LUNATIC_CHECK(ReceiveReferenceData(driver, 7, 22, CHI::Eb::Resps::CompData_UD_PD) == dirty);
    driver.Quiesce();
    reference.CheckStable(line, { { 7, CacheState::UD } }, true, false, dirty, initial, 7);
    LUNATIC_CHECK(reference.eventObservations != 0);
}

LUNATIC_TEST(Stage9ReferenceSameLineHazardIsSelective)
{
    Options options;
    options.policy = PolicyKind::NINE;
    TestDriver driver(options);
    const LineKey a = LineKey::FromAddress(0xb3000, false);
    const LineKey b = LineKey::FromAddress(0xb4000, false);
    const LineData initialA = ReferencePattern(59);
    const LineData dirtyA = ReferencePattern(181);
    const LineData dataB = ReferencePattern(197);
    driver.driver.node.GetMemory().WriteFull(a, initialA);
    driver.driver.node.GetMemory().WriteFull(b, dataB);
    CoherenceReference reference(driver, { a, b });
    SeedHomeDirty(driver, a, dirtyA);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 30, a.address, true));
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 31, a.address, true));
    DataBufferId firstDBID = 0;
    LUNATIC_CHECK(ReceiveReferenceData(driver, 4, 30, CHI::Eb::Resps::CompData_UD_PD,
        false, &firstDBID) == dirtyA);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 32, b.address, true));
    LUNATIC_CHECK(ReceiveReferenceData(driver, 6, 32, CHI::Eb::Resps::CompData_UC) == dataB);
    for (size_t cycle = 0; cycle < 6; ++cycle)
    {
        driver.driver.Tick();
        LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
        LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
    }
    reference.CheckStable(a, { { 4, CacheState::UD }, { 6, CacheState::I } },
        true, false, dirtyA, initialA, 4);
    reference.CheckStable(b, { { 6, CacheState::UC } }, true, false, dataB, dataB);

    driver.CompAck(4, firstDBID);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 4);
    driver.SnoopData(4, snoop.flit.TxnID(), dirtyA, CHI::Eb::Resps::SnpRespData_I_PD);
    LUNATIC_CHECK(ReceiveReferenceData(driver, 5, 31, CHI::Eb::Resps::CompData_UD_PD) == dirtyA);
    driver.Quiesce();
    reference.CheckStable(a, { { 5, CacheState::UD } }, true, false, dirtyA, initialA, 5);
    reference.CheckStable(b, { { 6, CacheState::UC } }, true, false, dataB, dataB);
    LUNATIC_CHECK(reference.eventObservations != 0);
}

LUNATIC_TEST(Stage9ReferenceVictimMergesAndFlushesReplacement)
{
    Options options;
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    TestDriver driver(options);
    const LineKey a = LineKey::FromAddress(0xb5000, false);
    const LineKey b = LineKey::FromAddress(0xb5040, false);
    const LineData initialA = ReferencePattern(71);
    const LineData updateA = ReferencePattern(211);
    const LineData initialB = ReferencePattern(83);
    const LineData dataB = ReferencePattern(227);
    driver.driver.node.GetMemory().WriteFull(a, initialA);
    driver.driver.node.GetMemory().WriteFull(b, initialB);
    CoherenceReference reference(driver, { a, b });
    ByteEnables enables{};
    LineData merged = initialA;
    for (size_t index = 0; index < enables.size(); ++index)
    {
        enables[index] = index % 32 < 16;
        if (enables[index])
            merged[index] = updateA[index];
    }

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 9, 40, a.address));
    const auto write = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(9, 40, write.DBID(), updateA, enables,
        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
    reference.CheckStable(a, {}, true, true, merged, initialA);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 41, a.address, true));
    LUNATIC_CHECK(ReceiveReferenceData(driver, 4, 41, CHI::Eb::Resps::CompData_UD_PD) == merged);
    driver.Quiesce();
    reference.CheckStable(a, { { 4, CacheState::UDP } }, true, false, merged, initialA, 4);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 42, b.address));
    const auto replacement = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(6, 42, replacement.DBID(), dataB, FullReferenceBytes(),
        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(victim.target == 4);
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(a) == initialA);
    driver.SnoopData(4, victim.flit.TxnID(), merged, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true);
    driver.Quiesce();
    reference.CheckStable(a, {}, false, false, merged, merged);
    reference.CheckStable(b, {}, true, true, dataB, initialB);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 43, a.address, true));
    LUNATIC_CHECK(ReceiveReferenceData(driver, 5, 43, CHI::Eb::Resps::CompData_UC) == merged);
    driver.Quiesce();
    reference.CheckStable(a, { { 5, CacheState::UC } }, true, false, merged, merged);
    reference.CheckStable(b, {}, false, false, dataB, dataB);
    LUNATIC_CHECK(reference.eventObservations != 0);
}
