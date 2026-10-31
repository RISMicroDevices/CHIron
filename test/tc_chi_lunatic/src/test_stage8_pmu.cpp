#include "oracle_driver.hpp"
#include "axi_model.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void CompleteRead(Driver<config>& driver, TransactionId txnId)
    {
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == txnId);
        }
    }

    template<typename config>
    void ReturnCredit(Driver<config>& driver, NodeId source, uint8_t creditType)
    {
        typename Driver<config>::REQ request{};
        request.SrcID() = source;
        request.TgtID() = driver.node.GetNodeId();
        request.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
        request.PCrdType() = creditType;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
    }

    ByteEnables FullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void ReceiveUnique(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            dbid = data.DBID();
        }
        driver.CompAck(source, dbid);
    }

    template<typename config>
    class PMUAXIEndpoint {
    public:
        explicit PMUAXIEndpoint(Bridge<config>& bridge) : bridge(bridge) {}

        std::optional<AXIAddress> PeekAW() const { return bridge.PeekDownstreamAW(); }
        std::optional<AXIWriteData> PeekW() const { return bridge.PeekDownstreamW(); }
        std::optional<AXIAddress> PeekAR() const { return bridge.PeekDownstreamAR(); }
        std::optional<AXIAddress> PopAW() { return bridge.PopDownstreamAW(); }
        std::optional<AXIWriteData> PopW() { return bridge.PopDownstreamW(); }
        std::optional<AXIAddress> PopAR() { return bridge.PopDownstreamAR(); }
        bool PushB(const AXIWriteResponse& response) { return bridge.PushDownstreamB(response); }
        bool PushR(const AXIReadData& response) { return bridge.PushDownstreamR(response); }

    private:
        Bridge<config>& bridge;
    };

    class AXIFixture {
    public:
        using REQ = Bridge<Config512>::REQFlit;
        using RSP = Bridge<Config512>::RSPFlit;
        using DAT = Bridge<Config512>::DATFlit;

        Bridge<Config512>         bridge;
        AXIModel                  memory;
        PMUAXIEndpoint<Config512> endpoint;
        Time                      now = 0;

        AXIFixture()
            : bridge(2, [] {
                Options options;
                options.dataWidth = 512;
                options.backend = BackendKind::AXI;
                return options;
            }())
            , memory(64)
            , endpoint(bridge)
        {
        }

        void Tick()
        {
            memory.Tick(endpoint);
            bridge.Tick(++now);
        }

        template<typename Predicate>
        void Until(Predicate&& predicate, size_t limit = 1000)
        {
            for (size_t cycle = 0; cycle < limit; ++cycle)
            {
                if (predicate())
                    return;
                Tick();
            }
            throw std::runtime_error("Stage 8 AXI PMU condition timed out");
        }

        REQ Request(uint16_t opcode, TransactionId txnId, uint64_t address) const
        {
            REQ request{};
            request.SrcID() = 4;
            request.TgtID() = 2;
            request.TxnID() = txnId;
            request.Opcode() = opcode;
            request.Addr() = address;
            request.Size() = 6;
            request.AllowRetry() = 1;
            request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
            return request;
        }

        RSP NextRSP(uint16_t opcode)
        {
            Until([&] { return static_cast<bool>(bridge.PeekUpstreamRSP()); });
            const auto response = bridge.PopUpstreamRSP();
            LUNATIC_CHECK(response && response->Opcode() == opcode);
            return *response;
        }

        void Quiesce()
        {
            Until([&] { return bridge.Drain() && memory.Drain(); });
            LUNATIC_CHECK(memory.Errors().empty());
            LUNATIC_CHECK(bridge.GetNode().GetErrors().empty());
            LUNATIC_CHECK(bridge.GetNode().ValidateInvariants());
        }
    };

}

LUNATIC_TEST(Stage8PMUReadSnapshotAndReset)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.GetPerformanceCounters() == PerformanceCounters{});
    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXDAT()); });
    const auto offered = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(offered.acceptedRXREQ == 1);
    LUNATIC_CHECK(offered.admittedTransactions == 1);
    LUNATIC_CHECK(offered.emittedTXDAT == 0);
    LUNATIC_CHECK(offered.retiredTransactions == 0);
    LUNATIC_CHECK(offered.GetStalls(WaitReason::Memory) > 0);

    for (size_t index = 0; index < 8; ++index)
    {
        LUNATIC_CHECK(driver.node.PeekTXDAT());
        LUNATIC_CHECK(driver.node.GetPerformanceCounters() == offered);
    }
    CompleteRead(driver, 1);
    driver.Quiesce();
    const auto completed = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(completed.acceptedRXREQ == 1);
    LUNATIC_CHECK(completed.admittedTransactions == 1);
    LUNATIC_CHECK(completed.retiredTransactions == 1);
    LUNATIC_CHECK(completed.emittedTXDAT == 2);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters() == completed);

    driver.node.Reset();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters() == PerformanceCounters{});
}

LUNATIC_TEST(Stage8PMURetryCreditsAndDecisionStalls)
{
    Driver<Config256> driver;
    size_t admissions = 0;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("pmu-retry", 0, [&](RequestAdmissionEvent& event) {
            ++admissions;
            if (admissions == 1)
            {
                event.Backpressure();
                return;
            }
            if (event.request.allowRetry)
                event.RequestRetry();
        }));

    auto older = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 3, 0x14000, false);
    auto younger = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 4, 0x18000, false);
    driver.node.PushRXREQ(older);
    driver.Tick();
    driver.Tick();
    driver.node.PushRXREQ(younger);
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    younger.TxnID() = 5;
    younger.AllowRetry() = 0;
    driver.node.PushRXREQ(younger);
    CompleteRead(driver, 5);
    ReturnCredit(driver, 4, older.PCrdType());
    driver.Quiesce();

    const auto pmu = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(pmu.acceptedRXREQ == 4);
    LUNATIC_CHECK(pmu.admittedTransactions == 1);
    LUNATIC_CHECK(pmu.retiredTransactions == 1);
    LUNATIC_CHECK(pmu.emittedTXRSP == 4);
    LUNATIC_CHECK(pmu.emittedRetryAcks == 2);
    LUNATIC_CHECK(pmu.emittedCreditGrants == 2);
    LUNATIC_CHECK(pmu.creditedResends == 1);
    LUNATIC_CHECK(pmu.returnedCredits == 1);
    LUNATIC_CHECK(pmu.GetStalls(WaitReason::Decision) == 1);
}

LUNATIC_TEST(Stage8PMUInputAndOutputQueuePressure)
{
    Options options;
    options.inputQueueLimit = 1;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("pmu-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry)
                event.RequestRetry();
        }));

    const auto first = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x20000, false);
    const auto second = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2, 0x24000, false);
    LUNATIC_CHECK(driver.node.PushRXREQ(first));
    LUNATIC_CHECK(!driver.node.PushRXREQ(second));
    driver.Tick();
    LUNATIC_CHECK(driver.node.PushRXREQ(second));
    driver.Tick();

    const auto held = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(held.GetStalls(WaitReason::InputQueue) >= 1);
    LUNATIC_CHECK(held.GetStalls(WaitReason::OutputQueue) >= 1);
    for (size_t index = 0; index < 4; ++index)
    {
        LUNATIC_CHECK(driver.node.PeekTXRSP());
        LUNATIC_CHECK(driver.node.GetPerformanceCounters() == held);
    }

    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    ReturnCredit(driver, 4, first.PCrdType());
    driver.Tick();
    ReturnCredit(driver, 5, second.PCrdType());
    driver.Quiesce();

    const auto pmu = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(pmu.acceptedRXREQ == 4);
    LUNATIC_CHECK(pmu.emittedRetryAcks == 2);
    LUNATIC_CHECK(pmu.emittedCreditGrants == 2);
    LUNATIC_CHECK(pmu.returnedCredits == 2);
    LUNATIC_CHECK(pmu.admittedTransactions == 0);
}

LUNATIC_TEST(Stage8PMUSnoopAndBackInvalidateStalls)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.snoopFilterSets = 1;
    options.snoopFilterWays = 1;
    CheckedDriver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x30000, false);
    const LineKey b = LineKey::FromAddress(0x30040, false);
    LineData dataA{};
    LineData dataB{};
    for (size_t index = 0; index < LINE_SIZE; ++index)
    {
        dataA[index] = uint8_t(index + 1);
        dataB[index] = uint8_t(index + 71);
    }
    driver.driver.node.GetMemory().WriteFull(a, dataA);
    driver.driver.node.GetMemory().WriteFull(b, dataB);

    LUNATIC_CHECK(driver.ReadData(4, 1, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
    driver.Quiesce();
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 2, b.address, true));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    driver.driver.Tick();
    driver.driver.Tick();
    const auto waiting = driver.driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(waiting.GetStalls(WaitReason::BackInvalidate) > 0);
    LUNATIC_CHECK(waiting.GetStalls(WaitReason::Snoop) > 0);

    driver.SnoopData(4, snoop.flit.TxnID(), dataA, CHI::Eb::Resps::SnpRespData_I);
    ReceiveUnique(driver, 5, 2);
    driver.Quiesce();
    const auto pmu = driver.driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(pmu.emittedTXSNP == 1);
    LUNATIC_CHECK(pmu.completedSnoopTargets == 1);
    LUNATIC_CHECK(pmu.acceptedRXDAT == 2);
    LUNATIC_CHECK(pmu.emittedTXDAT == 4);
    LUNATIC_CHECK(pmu.retiredTransactions == 2);
}

LUNATIC_TEST(Stage8PMUWriteDataStall)
{
    Driver<Config256> driver;
    const LineData data{};
    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 1, 0x40000, false));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    driver.Tick();
    driver.Tick();
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().GetStalls(WaitReason::WriteData) > 0);
    driver.WriteData(4, 1, response.DBID(), data, FullEnables(), CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();

    const auto pmu = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(pmu.acceptedRXDAT == 2);
    LUNATIC_CHECK(pmu.emittedTXRSP == 2);
    LUNATIC_CHECK(pmu.retiredTransactions == 1);
}

LUNATIC_TEST(Stage8PMUCompAckStall)
{
    Driver<Config256> driver;
    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, 0x50000, true));
    DataBufferId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
        dbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
    driver.Tick();
    driver.Tick();
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().GetStalls(WaitReason::CompAck) > 0);
    driver.CompAck(4, dbid);
    driver.Quiesce();

    const auto pmu = driver.node.GetPerformanceCounters();
    LUNATIC_CHECK(pmu.acceptedRXRSP == 1);
    LUNATIC_CHECK(pmu.emittedTXDAT == 2);
    LUNATIC_CHECK(pmu.retiredTransactions == 1);
}

LUNATIC_TEST(Stage8PMUAXIReadWriteActivity)
{
    AXIFixture fixture;
    fixture.bridge.PushUpstreamREQ(fixture.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 1, 0x60000));
    fixture.Until([&] { return static_cast<bool>(fixture.bridge.PeekUpstreamDAT()); });
    const auto read = fixture.bridge.PopUpstreamDAT();
    LUNATIC_CHECK(read);
    fixture.Quiesce();

    auto pmu = fixture.bridge.GetNode().GetPerformanceCounters();
    LUNATIC_CHECK(pmu.emittedTXAR == 1);
    LUNATIC_CHECK(pmu.acceptedRXR == 1);
    LUNATIC_CHECK(pmu.emittedTXAW == 0);
    LUNATIC_CHECK(pmu.emittedTXW == 0);
    LUNATIC_CHECK(pmu.acceptedRXB == 0);
    LUNATIC_CHECK(pmu.emittedTXDAT == 1);

    fixture.bridge.PushUpstreamREQ(fixture.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 2, 0x60040));
    const auto response = fixture.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    AXIFixture::DAT data{};
    data.SrcID() = 4;
    data.TgtID() = 2;
    data.TxnID() = response.DBID();
    data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
    data.DataID() = 0;
    for (size_t index = 0; index < LINE_SIZE; ++index)
    {
        data.Data()[index / 8] |= uint64_t(index + 3) << ((index % 8) * 8);
        data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
    }
    LUNATIC_CHECK(fixture.bridge.PushUpstreamDAT(data));
    fixture.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    fixture.Quiesce();

    pmu = fixture.bridge.GetNode().GetPerformanceCounters();
    LUNATIC_CHECK(pmu.emittedTXAR == 1);
    LUNATIC_CHECK(pmu.acceptedRXR == 1);
    LUNATIC_CHECK(pmu.emittedTXAW == 1);
    LUNATIC_CHECK(pmu.emittedTXW == 1);
    LUNATIC_CHECK(pmu.acceptedRXB == 1);
    LUNATIC_CHECK(pmu.acceptedRXDAT == 1);
    LUNATIC_CHECK(pmu.emittedTXRSP == 2);
    LUNATIC_CHECK(pmu.admittedTransactions == 2);
    LUNATIC_CHECK(pmu.retiredTransactions == 2);
    LUNATIC_CHECK(fixture.memory.CompletedReads().size() == 1);
    LUNATIC_CHECK(fixture.memory.CompletedWrites().size() == 1);

    fixture.bridge.Reset();
    LUNATIC_CHECK(fixture.bridge.GetNode().GetPerformanceCounters() == PerformanceCounters{});
}

LUNATIC_TEST(Stage8RobustnessAXIBackpressureResetAndRestart)
{
    AXIFixture fixture;
    bool hold = true;
    fixture.bridge.GetNode().GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("axi-reset-backpressure", 0, [&](RequestAdmissionEvent& event) {
            if (hold)
                event.Backpressure();
        }));
    fixture.memory.Stall(AXIChannel::AR, 4);
    fixture.memory.Stall(AXIChannel::R, 4);
    fixture.bridge.PushUpstreamREQ(fixture.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 1, 0x70000));
    fixture.Tick();
    LUNATIC_CHECK(!fixture.bridge.Drain());
    LUNATIC_CHECK(fixture.bridge.GetNode().GetPerformanceCounters().GetStalls(WaitReason::Decision) > 0);

    hold = false;
    fixture.Until([&] { return fixture.bridge.GetNode().GetPerformanceCounters().acceptedRXR == 1; });
    LUNATIC_CHECK(fixture.bridge.HasInflight());
    fixture.bridge.Reset();
    fixture.memory.Reset();
    fixture.now = 0;
    LUNATIC_CHECK(!fixture.bridge.HasInflight());
    LUNATIC_CHECK(!fixture.bridge.PeekDownstreamAW());
    LUNATIC_CHECK(!fixture.bridge.PeekDownstreamW());
    LUNATIC_CHECK(!fixture.bridge.PeekDownstreamAR());
    LUNATIC_CHECK(!fixture.bridge.PeekUpstreamDAT());
    LUNATIC_CHECK(fixture.bridge.GetNode().GetPerformanceCounters() == PerformanceCounters{});

    hold = true;
    fixture.memory.Stall(AXIChannel::AR, 2);
    fixture.memory.Stall(AXIChannel::R, 2);
    fixture.bridge.PushUpstreamREQ(fixture.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 1, 0x70000));
    fixture.Tick();
    LUNATIC_CHECK(!fixture.bridge.Drain());
    hold = false;
    fixture.Until([&] { return static_cast<bool>(fixture.bridge.PeekUpstreamDAT()); });
    LUNATIC_CHECK(fixture.bridge.PopUpstreamDAT());
    fixture.Quiesce();
    const auto pmu = fixture.bridge.GetNode().GetPerformanceCounters();
    LUNATIC_CHECK(pmu.acceptedRXREQ == 1);
    LUNATIC_CHECK(pmu.admittedTransactions == 1);
    LUNATIC_CHECK(pmu.retiredTransactions == 1);
    LUNATIC_CHECK(pmu.emittedTXAR == 1);
    LUNATIC_CHECK(pmu.acceptedRXR == 1);
    LUNATIC_CHECK(pmu.GetStalls(WaitReason::Decision) > 0);
}
