#include "driver.hpp"

#include <array>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineKey StrategyLine(size_t index)
    {
        return LineKey::FromAddress(0xc0000 + index * LINE_SIZE, false);
    }

    LineData StrategyData(uint8_t value)
    {
        LineData data{};
        data.fill(value);
        return data;
    }

    Options ArrayOptions(ReplacementKind kind)
    {
        Options options;
        options.slcSets = 1;
        options.slcWays = 4;
        options.snoopFilterSets = 1;
        options.snoopFilterWays = 4;
        options.strategy.homeReplacement = kind;
        options.strategy.snoopFilterReplacement = kind;
        return options;
    }

    void FillDirectory(Directory& directory)
    {
        for (size_t way = 0; way < directory.Ways(); ++way)
        {
            const LineKey line = StrategyLine(way);
            directory.Install(line, directory.ChooseVictim(line), way);
        }
    }

    void FillFilter(SnoopFilter& filter)
    {
        for (size_t way = 0; way < filter.Ways(); ++way)
        {
            const LineKey line = StrategyLine(way);
            filter.Track(line, 4, CacheState::SC, way);
        }
    }

    Request SchedulerRequest(NodeId source, TransactionId txnId, const LineKey& line, uint8_t qos = 0)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.address = line.address;
        request.size = 6;
        request.qos = qos;
        request.memAttr = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    Transaction& AddScheduled(Core::Impl& impl, NodeId source, TransactionId txnId,
                              const LineKey& line, uint8_t qos = 0)
    {
        impl.AdmitRequest(SchedulerRequest(source, txnId, line, qos));
        Transaction& transaction = *impl.tracker.FindRequest(source, txnId);
        transaction.phase = Phase::Action;
        transaction.readyTime = 0;
        return transaction;
    }

    std::vector<TransactionHandle> ScheduledOrder(Core::Impl& impl)
    {
        std::vector<TransactionHandle> order;
        impl.events->OnTransactionSchedule.Register(
            Gravity::MakeListener<TransactionScheduleEvent>("strategy-order", 0,
                [&](TransactionScheduleEvent& event) { order.push_back(event.transaction); }));
        impl.AdvanceTransactions();
        return order;
    }

    ByteEnables StrategyEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void CheckCopybackAllocation(Driver<Config256>& driver, const LineKey& line)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(
            driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 40, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 40, response.DBID(), StrategyData(0x6c), StrategyEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
    }

    void CheckWriteCleanKeepsPolicy(Driver<Config256>& driver, const LineKey& line)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(
            driver.Request(CHI::Eb::Opcodes::REQ::WriteCleanFull, 4, 41, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 41, response.DBID(), StrategyData(0x7d), StrategyEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UC);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.node.GetMemory().Read(line) == StrategyData(0x7d));
        LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::UC);
    }

}

LUNATIC_TEST(Stage10StrategiesDirectoryPLRUAndInvalidFirst)
{
    Directory directory(ArrayOptions(ReplacementKind::PLRU));
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(4)) == 0);
    FillDirectory(directory);
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(4)) == 0);
    directory.Access(StrategyLine(0));
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(4)) == 2);
    directory.Install(StrategyLine(4), 2, 4);
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(5)) == 1);
    directory.Invalidate(directory.SetIndex(StrategyLine(1)), 1);
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(5)) == 1);
    directory.Clear();
    LUNATIC_CHECK(directory.ValidCount() == 0);
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(5)) == 0);
}

LUNATIC_TEST(Stage10StrategiesSnoopFilterPLRUUpdate)
{
    SnoopFilter filter(ArrayOptions(ReplacementKind::PLRU));
    FillFilter(filter);
    LUNATIC_CHECK(filter.ChooseVictim(StrategyLine(4)) == 0);
    filter.Track(StrategyLine(0), 4, CacheState::SC, 10);
    LUNATIC_CHECK(filter.ChooseVictim(StrategyLine(4)) == 2);
}

LUNATIC_TEST(Stage10StrategiesLegacyAgeReplacementUnchanged)
{
    Options options = ArrayOptions(ReplacementKind::Age);
    options.slcWays = 2;
    Directory directory(options);
    directory.Install(StrategyLine(0), 0, 30);
    directory.Install(StrategyLine(1), 1, 20);
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(2)) == 1);
    directory.Access(StrategyLine(0));
    LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(2)) == 1);
}

LUNATIC_TEST(Stage10StrategiesRandomReplacementResetAndReplay)
{
    Options options = ArrayOptions(ReplacementKind::PseudoRandom);
    options.seed = 0x1234;
    Directory first(options);
    Directory second(options);
    FillDirectory(first);
    FillDirectory(second);
    std::vector<size_t> expected;
    for (size_t index = 0; index < 12; ++index)
        expected.push_back(first.ChooseVictim(StrategyLine(8)));
    for (size_t choice : expected)
        LUNATIC_CHECK(second.ChooseVictim(StrategyLine(8)) == choice);

    first.Clear();
    second.Clear();
    FillDirectory(first);
    FillDirectory(second);
    for (size_t choice : expected)
        LUNATIC_CHECK(first.ChooseVictim(StrategyLine(8)) == choice);
    for (size_t choice : expected)
        LUNATIC_CHECK(second.ChooseVictim(StrategyLine(8)) == choice);
}

LUNATIC_TEST(Stage10StrategiesSnoopFilterRandomReplay)
{
    Options options = ArrayOptions(ReplacementKind::PseudoRandom);
    options.seed = 0x5678;
    SnoopFilter first(options);
    SnoopFilter second(options);
    FillFilter(first);
    FillFilter(second);
    std::vector<size_t> expected;
    for (size_t index = 0; index < 12; ++index)
        expected.push_back(first.ChooseVictim(StrategyLine(8)));
    for (size_t choice : expected)
        LUNATIC_CHECK(second.ChooseVictim(StrategyLine(8)) == choice);

    first.Clear();
    second.Clear();
    FillFilter(first);
    FillFilter(second);
    for (size_t choice : expected)
        LUNATIC_CHECK(first.ChooseVictim(StrategyLine(8)) == choice);
    for (size_t choice : expected)
        LUNATIC_CHECK(second.ChooseVictim(StrategyLine(8)) == choice);
}

LUNATIC_TEST(Stage10StrategiesCoreRandomResetReplay)
{
    Options options = MakeType7Options();
    options.slcSets = 1;
    options.slcWays = 4;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 4;
    options.seed = 0x91;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    FillDirectory(impl.banks[0].directory);
    FillFilter(impl.banks[0].snoopFilter);
    std::vector<std::pair<size_t, size_t>> expected;
    for (size_t index = 0; index < 10; ++index)
        expected.emplace_back(impl.banks[0].directory.ChooseVictim(StrategyLine(12)),
            impl.banks[0].snoopFilter.ChooseVictim(StrategyLine(12)));
    impl.Reset();
    FillDirectory(impl.banks[0].directory);
    FillFilter(impl.banks[0].snoopFilter);
    for (const auto& [home, filter] : expected)
    {
        LUNATIC_CHECK(impl.banks[0].directory.ChooseVictim(StrategyLine(12)) == home);
        LUNATIC_CHECK(impl.banks[0].snoopFilter.ChooseVictim(StrategyLine(12)) == filter);
    }
}

LUNATIC_TEST(Stage10StrategiesSelectiveSharedAndCopybackAllocation)
{
    Options options = MakeType3Options();
    options.backend = BackendKind::Internal;
    options.poisonEnabled = false;
    options.dataCheckEnabled = false;
    options.policy = PolicyKind::Exclusive;
    options.structure.bankCount = 1;
    Driver<Config256> driver(options);
    const LineKey shared = StrategyLine(0);
    const LineKey noPeer = StrategyLine(1);
    const LineKey unique = StrategyLine(2);
    const LineKey copyback = StrategyLine(3);
    driver.node.GetMemory().WriteFull(shared, StrategyData(0x31));
    driver.node.GetMemory().WriteFull(noPeer, StrategyData(0x53));
    driver.node.GetMemory().WriteFull(unique, StrategyData(0x42));
    LUNATIC_CHECK(driver.ReadData(5, 1, shared.address,
        CHI::Eb::Opcodes::REQ::ReadUnique, true) == StrategyData(0x31));
    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, 4, 2, shared.address, true)));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpNotSharedDirty);
    LUNATIC_CHECK(snoop.target == 5);
    driver.WriteData(5, 0, snoop.flit.TxnID(), StrategyData(0x31), StrategyEnables(),
        CHI::Eb::Opcodes::DAT::SnpRespData, CHI::Eb::Resps::SnpRespData_SC_PD);
    LUNATIC_CHECK(driver.ReadData(4, 2, shared.address,
        CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, true) == StrategyData(0x31));
    LUNATIC_CHECK(driver.ReadData(4, 3, noPeer.address,
        CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, true) == StrategyData(0x53));
    LUNATIC_CHECK(driver.ReadData(5, 4, unique.address,
        CHI::Eb::Opcodes::REQ::ReadUnique, true) == StrategyData(0x42));
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.InspectHome(shared).valid);
    LUNATIC_CHECK(!driver.node.InspectHome(noPeer).valid);
    LUNATIC_CHECK(!driver.node.InspectHome(unique).valid);
    CheckCopybackAllocation(driver, copyback);
    CheckWriteCleanKeepsPolicy(driver, StrategyLine(4));

    Options legacy;
    Driver<Config256> baseline(legacy);
    baseline.node.GetMemory().WriteFull(noPeer, StrategyData(0x53));
    LUNATIC_CHECK(baseline.ReadData(4, 1, noPeer.address,
        CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, true) == StrategyData(0x53));
    baseline.Quiesce();
    LUNATIC_CHECK(!baseline.node.InspectHome(noPeer).valid);
}

LUNATIC_TEST(Stage10StrategiesRefillFirstPreservesPriorityAndAge)
{
    Options options;
    options.strategy.requestArbitration = RequestArbitrationKind::RefillFirst;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& older = AddScheduled(impl, 4, 1, StrategyLine(0));
    auto& refill = AddScheduled(impl, 5, 2, StrategyLine(1));
    auto& younger = AddScheduled(impl, 6, 3, StrategyLine(2));
    auto& waiting = AddScheduled(impl, 7, 4, StrategyLine(3));
    refill.Mark(COMPLETE_MEMORY);
    impl.flows.at(refill.handle).backendPhase = BackendPhase::Done;
    waiting.memoryRequired = true;
    const auto order = ScheduledOrder(impl);
    LUNATIC_CHECK(order.size() == 4);
    LUNATIC_CHECK(order[0] == refill.handle);
    LUNATIC_CHECK(order[1] == older.handle);
    LUNATIC_CHECK(order[2] == younger.handle);
    LUNATIC_CHECK(order[3] == waiting.handle);
}

LUNATIC_TEST(Stage10StrategiesClassWeightedOldestAgeGroup)
{
    Options options = MakeType7Options();
    options.backend = BackendKind::Internal;
    options.structure.classes.requestWeights = { 2, 1, 1, 1 };
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    impl.nextAge = 8;
    auto& first = AddScheduled(impl, 4, 1, StrategyLine(0), 15);
    impl.nextAge = 8;
    auto& second = AddScheduled(impl, 5, 2, StrategyLine(1), 15);
    impl.nextAge = 8;
    auto& third = AddScheduled(impl, 6, 3, StrategyLine(2), 12);
    const auto order = ScheduledOrder(impl);
    LUNATIC_CHECK((order == std::vector<TransactionHandle>{ first.handle, second.handle, third.handle }));
}

LUNATIC_TEST(Stage10StrategiesLegacyAgeArbitrationUnchanged)
{
    Options options;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    auto& first = AddScheduled(impl, 4, 1, StrategyLine(0));
    auto& second = AddScheduled(impl, 5, 2, StrategyLine(1));
    const auto order = ScheduledOrder(impl);
    LUNATIC_CHECK((order == std::vector<TransactionHandle>{ first.handle, second.handle }));
}

LUNATIC_TEST(Stage10StrategiesPLRUNonPowerOfTwoWays)
{
    for (size_t ways : { size_t(2), size_t(3) })
    {
        Options options = ArrayOptions(ReplacementKind::PLRU);
        options.slcWays = ways;
        Directory directory(options);
        FillDirectory(directory);
        LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(4)) == 0);
        directory.Access(StrategyLine(0));
        LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(4)) == 1);
        directory.Install(StrategyLine(4), 1, 4);
        LUNATIC_CHECK(directory.ChooseVictim(StrategyLine(5)) == 0);
    }
}

LUNATIC_TEST(Stage10StrategiesSelectiveWriteEvictOrEvict)
{
    Options options = MakeType3Options();
    options.backend = BackendKind::Internal;
    options.structure.bankCount = 1;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    const LineKey line = StrategyLine(5);
    Request request = SchedulerRequest(4, 50, line);
    request.opcode = CHI::Eb::Opcodes::REQ::WriteEvictOrEvict;
    request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
    impl.AdmitRequest(request);
    Transaction& transaction = *impl.tracker.FindRequest(4, 50);
    transaction.data = StrategyData(0x4e);
    transaction.byteEnables = StrategyEnables();
    impl.flows.at(transaction.handle).writeResp = CHI::Eb::Resps::CopyBackWrData_UD_PD;
    impl.flows.at(transaction.handle).requesterWasValid = true;
    impl.flows.at(transaction.handle).requesterWasDirty = true;
    LUNATIC_CHECK(impl.CommitCopyback(transaction));
    const HomeLine* home = impl.Bank(line).directory.Find(line);
    LUNATIC_CHECK(home && home->dirty);
    LUNATIC_CHECK(home->data == StrategyData(0x4e));
    LUNATIC_CHECK(impl.ValidateInvariants());
    impl.Retire(transaction.handle);
    LUNATIC_CHECK(impl.ValidateInvariants());
}
