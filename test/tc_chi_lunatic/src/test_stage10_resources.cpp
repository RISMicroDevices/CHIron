#include "driver.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using MetadataConfig256 = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 256, true, true, false>;

    Options Type3Internal(size_t bankCount)
    {
        Options options = MakeType3Options();
        options.backend = BackendKind::Internal;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        options.policy = PolicyKind::NINE;
        options.inputQueueLimit = 96;
        options.outputQueueLimit = 96;
        options.structure.bankCount = bankCount;
        options.structure.requestBufferLimit = 0;
        options.structure.refillResourceLimit = 0;
        options.structure.responseResourceLimit = 0;
        options.structure.memoryResourceLimit = 0;
        options.structure.snoopResourceLimit = 0;
        options.structure.outgoingSnoopQueueLimit = 0;
        return options;
    }

    Options Type7Internal()
    {
        Options options = MakeType7Options();
        options.backend = BackendKind::Internal;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        options.policy = PolicyKind::NINE;
        options.inputQueueLimit = 96;
        options.outputQueueLimit = 96;
        options.structure.requestBufferLimit = 0;
        options.structure.snoopResourceLimit = 0;
        options.structure.outgoingSnoopQueueLimit = 0;
        return options;
    }

    LineKey BankLine(size_t bank, size_t alias = 0)
    {
        return LineKey::FromAddress(0x80000 + bank * LINE_SIZE + alias * 4 * LINE_SIZE, false);
    }

    LineData Pattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    ByteEnables FullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void PopReadData(Driver<Config256>& driver, TransactionId txnId, NodeId source, bool compAck)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == txnId);
            LUNATIC_CHECK(data.TgtID() == source);
            LUNATIC_CHECK(data.DataID() == beat * 2);
            dbid = data.DBID();
        }
        if (compAck)
            driver.CompAck(source, dbid);
    }

    struct ReadCompletion {
        TransactionId txnId = 0;
        NodeId        source = 0;
    };

    ReadCompletion PopAnyReadData(Driver<Config256>& driver, bool compAck,
                                  std::map<TransactionId, uint8_t>& beats)
    {
        for (size_t count = 0; count < 32; ++count)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            const TransactionId txnId = data.TxnID();
            LUNATIC_CHECK(data.DataID() == 0 || data.DataID() == 2);
            const uint8_t bit = data.DataID() == 0 ? 1 : 2;
            uint8_t& received = beats[txnId];
            LUNATIC_CHECK(!(received & bit));
            received |= bit;
            if (received != 3)
                continue;
            if (compAck)
                driver.CompAck(data.TgtID(), data.DBID());
            return { txnId, data.TgtID() };
        }
        throw std::runtime_error("read completion did not arrive");
    }

    void SnoopResponse(Driver<Config256>& driver, NodeId source, TransactionId txnId, uint8_t resp,
                       bool forwarded = false, uint8_t fwdState = 0)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = source;
        response.TgtID() = driver.node.GetNodeId();
        response.TxnID() = txnId;
        response.Opcode() = forwarded ? CHI::Eb::Opcodes::RSP::SnpRespFwded : CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = resp;
        response.FwdState() = fwdState;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void ForwardedData(Driver<Config256>& driver, NodeId source, TransactionId txnId, const LineData& data)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            Driver<Config256>::DAT flit{};
            flit.SrcID() = source;
            flit.TgtID() = driver.node.GetNodeId();
            flit.HomeNID() = 0;
            flit.TxnID() = txnId;
            flit.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespDataFwded;
            flit.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
            flit.FwdState() = CHI::Eb::FwdStates::SC;
            flit.DataID() = beat * 2;
            flit.CCID() = 0;
            flit.BE() = ~uint32_t(0);
            for (size_t index = 0; index < 32; ++index)
                flit.Data()[index / 8] |= uint64_t(data[beat * 32 + index]) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(flit));
        }
        driver.Tick();
    }

    void StartWriteback(Driver<Config256>& driver, NodeId source, TransactionId txnId,
                        const LineKey& line, const LineData& data)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(
            driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, source, txnId, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(source, txnId, response.DBID(), data, FullEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    }

    LineData SeedData(const LineKey& line)
    {
        return Pattern((line.address / LINE_SIZE) & 0xff);
    }

    void SeedUnique(Driver<Config256>& driver, NodeId source, TransactionId txnId, const LineKey& line)
    {
        const LineData expected = SeedData(line);
        driver.node.GetMemory().WriteFull(line, expected);
        StartWriteback(driver, source, txnId, line, expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(source, txnId + 1, line.address,
            CHI::Eb::Opcodes::REQ::ReadUnique, true) == expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(line, source) == CacheState::UD);
    }

    void SeedShared(Driver<Config256>& driver, NodeId source, TransactionId txnId, const LineKey& line)
    {
        const LineData expected = SeedData(line);
        driver.node.GetMemory().WriteFull(line, expected);
        LUNATIC_CHECK(driver.ReadData(source, txnId, line.address,
            CHI::Eb::Opcodes::REQ::ReadShared, true) == expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(line, source) == CacheState::SC);
    }

    void StartRead(Driver<Config256>& driver, uint16_t opcode, NodeId source, TransactionId txnId,
                   const LineKey& line, bool allowRetry = false, uint8_t qos = 0)
    {
        auto request = driver.Request(opcode, source, txnId, line.address, false);
        request.AllowRetry() = allowRetry;
        request.QoS() = qos;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.node.Tick(driver.now);
    }

    Request InternalRead(NodeId source, TransactionId txnId, const LineKey& line)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = CHI::Eb::Opcodes::REQ::ReadUnique;
        request.address = line.address;
        request.size = 6;
        request.allowRetry = false;
        request.expCompAck = true;
        request.snpAttr = 1;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        return request;
    }

    bool ImplPushRequest(Core::Impl& impl, const Request& request)
    {
        return impl.Push(impl.rxreq, request, EventKind::AcceptedRXREQ);
    }

    void ImplTick(Core::Impl& impl, Time time)
    {
        impl.time = time;
        impl.Tick();
    }

    template<typename config>
    void SendBackendReadData(Driver<config>& driver, TransactionId txnId, const LineKey& line,
                             const LineData& data)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename Driver<config>::DAT flit{};
            flit.SrcID() = 6;
            flit.TgtID() = driver.node.GetNodeId();
            flit.HomeNID() = driver.node.GetNodeId();
            flit.TxnID() = txnId;
            flit.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            flit.Resp() = CHI::Eb::Resps::CompData_I;
            flit.DataID() = beat * bytes / 16;
            flit.CCID() = (line.address >> 4) & 3;
            for (size_t index = 0; index < bytes; ++index)
                flit.Data()[index / 8] |= uint64_t(data[beat * bytes + index]) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(flit));
        }
        driver.Tick();
    }

}

LUNATIC_TEST(Stage10ResourcesPerBankOriginalsReleaseOne)
{
    Options options = Type3Internal(4);
    options.transactionLimit = 8;
    Driver<Config256> driver(options);
    const LineKey first = BankLine(0, 0);
    const LineKey second = BankLine(0, 1);
    const LineKey other = BankLine(1, 0);
    std::vector<TransactionId> lookups;
    driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("resource-original-lookup", 0,
            [&](TransactionScheduleEvent& event) {
                if (event.phase == Phase::Lookup)
                    lookups.push_back(event.transaction);
            }));

    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 10, first);
    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 11, second);
    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 6, 12, other);
    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 7, 13, BankLine(0, 2));
    driver.Until([&] { return lookups.size() == 3; });
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 3);
    std::map<TransactionId, uint8_t> completions;

    PopAnyReadData(driver, false, completions);
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().retiredTransactions == 1);

    PopAnyReadData(driver, false, completions);
    driver.Tick();
    driver.Until([&] { return lookups.size() == 4; });
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 4);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().retiredTransactions == 2);
    LUNATIC_CHECK(driver.node.ValidateInvariants());

    PopAnyReadData(driver, false, completions);
    PopAnyReadData(driver, false, completions);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10ResourcesGlobalOriginalLimit)
{
    Options options = Type3Internal(4);
    Driver<Config256> driver(options);
    for (size_t index = 0; index < 65; ++index)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp,
            4 + index % 4, 100 + index, BankLine(index % 4, index / 4).address, false);
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
    }
    for (size_t index = 0; index < 65; ++index)
        driver.node.Tick(0);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 64);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
    std::map<TransactionId, uint8_t> completions;

    PopAnyReadData(driver, false, completions);
    driver.Tick();
    driver.node.Tick(driver.now);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 65);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
    driver.node.Reset();
    LUNATIC_CHECK(!driver.node.HasInflight());
}

LUNATIC_TEST(Stage10ResourcesRequestBufferEightNineAndLookupRelease)
{
    Options options = Type3Internal(1);
    options.structure.requestBufferLimit = 8;
    Driver<Config256> driver(options);
    for (size_t index = 0; index < 9; ++index)
        StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 4 + index, 200 + index, BankLine(0, index));
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 8);

    for (size_t cycle = 0; cycle < 4; ++cycle)
        driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 8);
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 9);
    LUNATIC_CHECK(driver.node.ValidateInvariants());

    for (size_t index = 0; index < 9; ++index)
        PopReadData(driver, 200 + index, 4 + index, false);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10ResourcesRequestBufferBackpressureAndCancellation)
{
    Options options = Type3Internal(1);
    options.structure.requestBufferLimit = 1;
    Driver<Config256> driver(options);
    bool hold = true;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("resource-admission-hold", 0,
            [&](RequestAdmissionEvent& event) {
                if (hold && event.request.txnId == 300)
                    event.Backpressure();
            }));

    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 300, BankLine(0, 0));
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 0);
    hold = false;
    driver.node.Tick(driver.now);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 1);
    driver.Until([&] { return driver.now == 5; });

    auto invalid = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 301, BankLine(0, 1).address, false);
    invalid.Size() = 7;
    invalid.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(invalid));
    driver.node.Tick(driver.now);
    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 6, 302, BankLine(0, 2));
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 2);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 301);
    driver.Tick();
    driver.Until([&] { return driver.node.GetPerformanceCounters().admittedTransactions == 3; });
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.ValidateInvariants());

    PopReadData(driver, 300, 4, false);
    PopReadData(driver, 302, 6, false);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10ResourcesType7ReservedRawEntry)
{
    Options options = Type7Internal();
    Driver<Config256> driver(options);
    for (size_t index = 0; index < 15; ++index)
        StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 4 + index % 4, 400 + index, BankLine(0, index),
            false, 15);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 15);

    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 415, BankLine(0, 15), true, 15);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 15);
    const auto retry = driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(retry.TxnID() == 415);
    driver.Tick();
    LUNATIC_CHECK(!driver.node.PeekTXRSP());

    PopReadData(driver, 400, 4, false);
    driver.Tick();
    const auto grant = driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    auto resend = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 415, BankLine(0, 15).address, false);
    resend.AllowRetry() = 0;
    resend.PCrdType() = grant.PCrdType();
    resend.QoS() = 15;
    LUNATIC_CHECK(driver.node.PushRXREQ(resend));
    driver.node.Tick(driver.now);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 16);

    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 416, BankLine(0, 16), false, 15);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 16);
    LUNATIC_CHECK(driver.node.GetOptions().transactionLimit == 16);
    LUNATIC_CHECK(driver.node.GetOptions().structure.reservedTransactionEntries == 1);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
    driver.node.Reset();
    LUNATIC_CHECK(!driver.node.HasInflight());
}

LUNATIC_TEST(Stage10ResourcesZeroLimitsAndLegacyTiming)
{
    Options options;
    options.structure.bankCount = 4;
    options.transactionLimit = 64;
    options.inputQueueLimit = 96;
    options.outputQueueLimit = 96;
    options.retryEnabled = false;
    Driver<Config256> driver(options);
    for (size_t index = 0; index < 32; ++index)
        StartRead(driver, CHI::Eb::Opcodes::REQ::ReadNoSnp, 4 + index % 4, 500 + index,
            BankLine(index % 2, index / 2));
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 32);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
    driver.node.Reset();

    Driver<Config256> legacy;
    std::vector<Time> retired;
    legacy.node.GetEvents()->OnRetired.Register(
        Gravity::MakeListener<RetiredEvent>("resource-legacy-retired", 0,
            [&](RetiredEvent& event) { retired.push_back(event.time); }));
    LUNATIC_CHECK(legacy.node.PushRXREQ(
        legacy.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 550, 0x90000, false)));
    while (legacy.now < 18)
    {
        legacy.Tick();
        LUNATIC_CHECK(!legacy.node.PeekTXDAT());
    }
    legacy.Tick();
    LUNATIC_CHECK(legacy.node.PopTXDAT());
    legacy.Tick();
    LUNATIC_CHECK(legacy.node.PopTXDAT());
    legacy.Tick();
    LUNATIC_CHECK(retired == std::vector<Time>{ 21 });
    LUNATIC_CHECK(legacy.node.Drain());
}

LUNATIC_TEST(Stage10ResourcesWavePerParentAndOtherBank)
{
    Options options = Type3Internal(2);
    options.structure.snoopResourceLimit = 1;
    options.structure.outgoingSnoopQueueLimit = 1;
    Driver<Config256> driver(options);
    const LineKey first = BankLine(0, 0);
    const LineKey second = BankLine(0, 1);
    const LineKey other = BankLine(1, 0);
    SeedShared(driver, 4, 600, first);
    SeedShared(driver, 5, 601, first);
    SeedShared(driver, 4, 602, second);
    SeedShared(driver, 4, 603, other);

    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadUnique, 6, 610, first);
    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadUnique, 7, 611, second);
    StartRead(driver, CHI::Eb::Opcodes::REQ::ReadUnique, 8, 612, other);
    const auto firstA = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    const auto otherWave = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    const auto firstB = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(firstA.target == 4);
    LUNATIC_CHECK(firstB.target == 5);
    LUNATIC_CHECK(otherWave.target == 4);

    SnoopResponse(driver, 4, firstA.flit.TxnID(), 0xff);
    LUNATIC_CHECK(!driver.node.PeekTXSNP());
    SnoopResponse(driver, 4, otherWave.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    SnoopResponse(driver, 4, firstA.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    LUNATIC_CHECK(!driver.node.PeekTXSNP());
    SnoopResponse(driver, 5, firstB.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);

    const auto secondWave = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(secondWave.target == 4);
    SnoopResponse(driver, 4, secondWave.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    std::map<TransactionId, uint8_t> completions;
    PopAnyReadData(driver, true, completions);
    PopAnyReadData(driver, true, completions);
    PopAnyReadData(driver, true, completions);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10ResourcesDctCompAckKeepsProviderWave)
{
    Options options = Type7Internal();
    options.dctEnabled = true;
    options.structure.snoopResourceLimit = 1;
    Driver<Config256> driver(options);
    const LineKey first = BankLine(0, 0);
    const LineKey second = BankLine(0, 1);
    SeedUnique(driver, 6, 700, first);
    SeedUnique(driver, 6, 701, second);

    auto firstRequest = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 710, first.address, true);
    firstRequest.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    LUNATIC_CHECK(driver.node.PushRXREQ(firstRequest));
    const auto firstSnoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    driver.CompAck(4, firstSnoop.flit.TxnID());
    LUNATIC_CHECK(driver.node.GetErrors().empty());

    auto secondRequest = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 8, 711, second.address, true);
    secondRequest.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    LUNATIC_CHECK(driver.node.PushRXREQ(secondRequest));
    driver.Tick();
    LUNATIC_CHECK(!driver.node.PeekTXSNP());
    ForwardedData(driver, 6, firstSnoop.flit.TxnID(), SeedData(first));

    const auto secondSnoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    ForwardedData(driver, 6, secondSnoop.flit.TxnID(), SeedData(second));
    driver.CompAck(8, secondSnoop.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10ResourcesVictimDiscardDoesNotLeak)
{
    Options options = Type3Internal(1);
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    options.structure.snoopResourceLimit = 1;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    const LineKey incoming = BankLine(0, 1);
    const LineKey victim = BankLine(0, 0);
    impl.InstallHome(victim, 0, Pattern(80), false, 0);
    impl.Bank(victim).snoopFilter.Track(victim, 4, CacheState::SC, 0);

    Request request = InternalRead(5, 800, incoming);
    request.opcode = CHI::Eb::Opcodes::REQ::WriteBackFull;
    request.expCompAck = false;
    impl.AdmitRequest(request, std::numeric_limits<size_t>::max());
    Transaction& transaction = *impl.tracker.FindRequest(5, 800);
    transaction.data = Pattern(81);
    transaction.byteEnables = FullEnables();
    transaction.Mark(COMPLETE_WRITE_DATA);
    transaction.phase = Phase::Action;
    transaction.readyTime = 0;

    LUNATIC_CHECK(!impl.PrepareVictim(transaction));
    FlowState& flow = impl.flows.at(transaction.handle);
    LUNATIC_CHECK(flow.victimPending && flow.victimLine == victim);
    LUNATIC_CHECK(impl.EmitSnoops(transaction));
    const auto snoop = impl.Pop(impl.txsnp, EventKind::EmittedTXSNP);
    LUNATIC_CHECK(snoop && snoop->target == 4);
    LUNATIC_CHECK(impl.tracker.FindSnoop(4, snoop->txnId) == &transaction);
    LUNATIC_CHECK(impl.banks[0].activeSnoopWaves == 1);

    impl.Bank(victim).directory.Invalidate(impl.Bank(victim).directory.SetIndex(victim), 0);
    Response response;
    response.source = 4;
    response.target = 2;
    response.txnId = snoop->txnId;
    response.opcode = CHI::Eb::Opcodes::RSP::SnpResp;
    response.resp = CHI::Eb::Resps::SnpResp_I;
    impl.ProcessSnoopRSP(transaction, response);
    LUNATIC_CHECK(transaction.pendingSnoops.empty());
    LUNATIC_CHECK(impl.banks[0].activeSnoopWaves == 0);

    impl.ActionCoherent(transaction);
    LUNATIC_CHECK(!flow.victimPending);
    LUNATIC_CHECK(!flow.snoopWaveOwned);
    LUNATIC_CHECK(!impl.Bank(victim).hazards.Has(victim));
    impl.ActionCoherent(transaction);
    LUNATIC_CHECK(impl.Bank(incoming).directory.Find(incoming));
    LUNATIC_CHECK(impl.banks[0].activeSnoopWaves == 0);
    LUNATIC_CHECK(impl.errors.empty());
    LUNATIC_CHECK(impl.ValidateInvariants());

    while (impl.Pop(impl.txrsp, EventKind::EmittedTXRSP))
    {
    }
    impl.Retire(transaction.handle);
    LUNATIC_CHECK(impl.tracker.Empty());
    LUNATIC_CHECK(!impl.HasInflight());
    LUNATIC_CHECK(impl.ValidateInvariants());
}

LUNATIC_TEST(Stage10ResourcesRetireWithPendingSnoops)
{
    Options options = Type3Internal(1);
    options.structure.snoopResourceLimit = 1;
    options.structure.outgoingSnoopQueueLimit = 1;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    const LineKey line = BankLine(0, 0);
    impl.Bank(line).snoopFilter.Track(line, 6, CacheState::UD, 0);
    LUNATIC_CHECK(ImplPushRequest(impl, InternalRead(4, 900, line)));
    ImplTick(impl, 0);
    for (Time cycle = 1; cycle <= 8; ++cycle)
        ImplTick(impl, cycle);

    const auto handle = impl.tracker.Handles().front();
    const auto pending = impl.Peek(impl.txsnp);
    LUNATIC_CHECK(pending);
    LUNATIC_CHECK(impl.tracker.FindSnoop(pending->target, pending->txnId));
    LUNATIC_CHECK(impl.banks[0].activeSnoopWaves == 1);
    LUNATIC_CHECK(impl.banks[0].outgoingSnoopQueue == 1);
    impl.Retire(handle);
    LUNATIC_CHECK(impl.tracker.Empty());
    LUNATIC_CHECK(!impl.tracker.FindSnoop(pending->target, pending->txnId));
    LUNATIC_CHECK(impl.banks[0].activeOriginals == 0);
    LUNATIC_CHECK(impl.banks[0].activeSnoopWaves == 0);
    LUNATIC_CHECK(impl.banks[0].outgoingSnoopQueue == 1);
    LUNATIC_CHECK(impl.HasInflight());
    LUNATIC_CHECK(impl.ValidateInvariants());

    const auto popped = impl.Pop(impl.txsnp, EventKind::EmittedTXSNP);
    LUNATIC_CHECK(popped);
    LUNATIC_CHECK(impl.banks[0].outgoingSnoopQueue == 0);
    LUNATIC_CHECK(!impl.HasInflight());
    LUNATIC_CHECK(impl.ValidateInvariants());
}

LUNATIC_TEST(Stage10ResourcesResetRestoresAndReplays)
{
    Options options = Type3Internal(1);
    options.structure.requestBufferLimit = 1;
    options.structure.snoopResourceLimit = 1;
    options.structure.outgoingSnoopQueueLimit = 1;
    auto events = std::make_shared<EventHub>();
    Core::Impl impl(2, options, events);
    const LineKey line = BankLine(0, 0);
    impl.Bank(line).snoopFilter.Track(line, 6, CacheState::UD, 0);
    LUNATIC_CHECK(ImplPushRequest(impl, InternalRead(4, 910, line)));
    ImplTick(impl, 0);
    for (Time cycle = 1; cycle <= 8; ++cycle)
        ImplTick(impl, cycle);
    const auto baseline = *impl.Peek(impl.txsnp);

    Request invalid = InternalRead(5, 911, BankLine(0, 1));
    invalid.size = 7;
    LUNATIC_CHECK(ImplPushRequest(impl, invalid));
    ImplTick(impl, 8);
    LUNATIC_CHECK(impl.banks[0].requestBufferOccupants == 1);
    LUNATIC_CHECK(impl.banks[0].activeOriginals == 2);
    LUNATIC_CHECK(impl.banks[0].activeSnoopWaves == 1);
    LUNATIC_CHECK(impl.banks[0].outgoingSnoopQueue == 1);

    impl.Reset();
    LUNATIC_CHECK(!impl.HasInflight());
    LUNATIC_CHECK(impl.banks[0].activeOriginals == 0);
    LUNATIC_CHECK(impl.banks[0].requestBufferOccupants == 0);
    LUNATIC_CHECK(impl.banks[0].activeSnoopWaves == 0);
    LUNATIC_CHECK(impl.banks[0].outgoingSnoopQueue == 0);
    LUNATIC_CHECK(impl.ValidateInvariants());

    impl.Bank(line).snoopFilter.Track(line, 6, CacheState::UD, 0);
    LUNATIC_CHECK(ImplPushRequest(impl, InternalRead(4, 910, line)));
    ImplTick(impl, 0);
    for (Time cycle = 1; cycle <= 8; ++cycle)
        ImplTick(impl, cycle);
    const auto replayed = *impl.Peek(impl.txsnp);
    LUNATIC_CHECK(replayed.txnId == baseline.txnId);
    LUNATIC_CHECK(replayed.address == baseline.address);
    LUNATIC_CHECK(replayed.opcode == baseline.opcode);
    LUNATIC_CHECK(impl.time == 8);
    LUNATIC_CHECK(impl.ValidateInvariants());
    LUNATIC_CHECK(impl.Pop(impl.txsnp, EventKind::EmittedTXSNP));
}

LUNATIC_TEST(Stage10ResourcesType3MetadataSmoke)
{
    Options options = MakeType3Options();
    options.subordinateId = 6;
    Driver<MetadataConfig256> driver(options);
    const std::array<LineKey, 4> lines = { BankLine(0), BankLine(1), BankLine(2), BankLine(3) };
    for (size_t index = 0; index < lines.size(); ++index)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4 + index, 950 + index,
            lines[index].address, false);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.node.Tick(0);
    }

    std::array<TransactionId, 4> downstream{};
    for (size_t index = 0; index < lines.size(); ++index)
        downstream[index] = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp).TxnID();
    for (size_t index = 0; index < lines.size(); ++index)
        SendBackendReadData(driver, downstream[index], lines[index], Pattern(100 + index));

    for (size_t transaction = 0; transaction < lines.size(); ++transaction)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == 950 + transaction);
            const LineData expected = Pattern(100 + transaction);
            for (size_t index = 0; index < 32; ++index)
            {
                const uint8_t actual = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
                LUNATIC_CHECK(actual == expected[beat * 32 + index]);
            }
        }
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}
