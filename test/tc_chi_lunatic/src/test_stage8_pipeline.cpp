#include "driver.hpp"

#include <array>
#include <limits>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    constexpr std::array<Phase, 14> phases = {
        Phase::Receive,
        Phase::Decode,
        Phase::Admit,
        Phase::Hazard,
        Phase::Lookup,
        Phase::Resolve,
        Phase::Action,
        Phase::WaitMemory,
        Phase::WaitWriteData,
        Phase::WaitSnoop,
        Phase::Return,
        Phase::WaitCompAck,
        Phase::Commit,
        Phase::Retired
    };

    struct TraceEntry {
        EventKind     kind;
        Time          time;
        NodeId        source;
        NodeId        target;
        TransactionId txnId;
        uint16_t      opcode;
        uint8_t       dataId;

        bool operator==(const TraceEntry&) const = default;
    };

    class ConcurrentReadTrace {
        static constexpr TransactionId firstTxn = 31;
        static constexpr TransactionId secondTxn = 32;

        Driver<Config256>       driver;
        std::vector<TraceEntry> trace;
        size_t                  retired = 0;

        template<typename Event>
        void Record(const Event& event)
        {
            trace.push_back({ event.kind, event.time, event.source, event.target,
                event.txnId, event.opcode, 0 });
        }

        void PopAvailable(size_t repeatedPeeks, Time time)
        {
            while (const auto first = driver.node.PeekTXDAT())
            {
                const size_t eventCount = trace.size();
                const bool inflight = driver.node.HasInflight();
                for (size_t index = 0; index < repeatedPeeks; ++index)
                {
                    const auto next = driver.node.PeekTXDAT();
                    LUNATIC_CHECK(next);
                    LUNATIC_CHECK(next->Opcode() == first->Opcode());
                    LUNATIC_CHECK(next->TxnID() == first->TxnID());
                    LUNATIC_CHECK(next->DataID() == first->DataID());
                    LUNATIC_CHECK(trace.size() == eventCount);
                    LUNATIC_CHECK(driver.node.GetTime() == time);
                    LUNATIC_CHECK(driver.node.HasInflight() == inflight);
                    LUNATIC_CHECK(driver.node.ValidateInvariants());
                }

                const auto data = driver.node.PopTXDAT();
                LUNATIC_CHECK(data);
                LUNATIC_CHECK(data->Opcode() == CHI::Eb::Opcodes::DAT::CompData);
                LUNATIC_CHECK(!trace.empty());
                LUNATIC_CHECK(trace.back().kind == EventKind::EmittedTXDAT);
                trace.back().dataId = data->DataID();

                const size_t transaction = data->TxnID() == firstTxn ? 0 :
                    data->TxnID() == secondTxn ? 1 : 2;
                LUNATIC_CHECK(transaction < 2);
                LUNATIC_CHECK(data->TgtID() == (transaction == 0 ? 4 : 5));
                LUNATIC_CHECK(data->DataID() == 0 || data->DataID() == 2);
                for (size_t lane = 0; lane < Config256::dataWidth / 64; ++lane)
                    LUNATIC_CHECK(data->Data()[lane] == 0);
            }
        }

    public:
        explicit ConcurrentReadTrace(Options options)
            : driver(options)
        {
            auto events = driver.node.GetEvents();
            events->OnAcceptedRXREQ.Register(
                Gravity::MakeListener<AcceptedRXREQEvent>("stage8-accepted", 0,
                    [this](AcceptedRXREQEvent& event) { Record(event); }));
            events->OnEmittedTXDAT.Register(
                Gravity::MakeListener<EmittedTXDATEvent>("stage8-emitted", 0,
                    [this](EmittedTXDATEvent& event) { Record(event); }));
            events->OnRetired.Register(
                Gravity::MakeListener<RetiredEvent>("stage8-retired", 0,
                    [this](RetiredEvent& event) {
                        Record(event);
                        ++retired;
                    }));
        }

        std::vector<TraceEntry> Run(size_t repeatedPeeks = 0)
        {
            LUNATIC_CHECK(driver.node.GetTime() == 0);
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            LUNATIC_CHECK(!driver.node.HasInflight());
            trace.clear();
            retired = 0;

            LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(
                CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, firstTxn, 0x8000, false)));
            LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(
                CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, secondTxn, 0x8040, false)));
            driver.node.Tick(0);
            driver.node.Tick(0);
            LUNATIC_CHECK(driver.node.GetErrors().empty());

            std::array<size_t, 2> beats = {};
            for (Time cycle = 1; cycle <= 512 && retired != 2; ++cycle)
            {
                driver.node.Tick(cycle);
                const size_t before = trace.size();
                PopAvailable(repeatedPeeks, cycle);
                for (size_t index = before; index < trace.size(); ++index)
                {
                    if (trace[index].kind != EventKind::EmittedTXDAT)
                        continue;
                    const size_t transaction = trace[index].txnId == firstTxn ? 0 : 1;
                    ++beats[transaction];
                }
            }

            LUNATIC_CHECK(retired == 2);
            LUNATIC_CHECK((beats == std::array<size_t, 2>{ 2, 2 }));
            LUNATIC_CHECK(!driver.node.HasInflight());
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            LUNATIC_CHECK(driver.node.ValidateInvariants());
            return trace;
        }

        void Reset()
        {
            driver.node.Reset();
            LUNATIC_CHECK(driver.node.GetTime() == 0);
            LUNATIC_CHECK(!driver.node.HasInflight());
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            LUNATIC_CHECK(driver.node.ValidateInvariants());
        }
    };

    Options RandomTimingOptions()
    {
        Options options;
        options.tagLatency = 1;
        options.dataLatency = 1;
        options.memoryLatency = 2;
        options.receiveDelay = 1;
        options.decodeDelay = 1;
        options.admissionDelay = 1;
        options.hazardDelay = 1;
        options.lookupDelay = 1;
        options.resolveDelay = 1;
        options.actionDelay = 1;
        options.returnDelay = 1;
        options.commitDelay = 1;
        options.randomDelayLimit = 3;
        options.randomizeArbitration = true;
        options.seed = 0x5eed;
        return options;
    }

}

LUNATIC_TEST(Stage8PipelinePhaseDelayMapping)
{
    VirtualPipeline defaults(Options{});
    for (Phase phase : phases)
    {
        LUNATIC_CHECK(defaults.PhaseDelay(phase) == 0);
    }

    Options options;
    options.receiveDelay = 1;
    options.decodeDelay = 2;
    options.admissionDelay = 3;
    options.hazardDelay = 4;
    options.lookupDelay = 5;
    options.resolveDelay = 6;
    options.actionDelay = 7;
    options.returnDelay = 8;
    options.commitDelay = 9;
    VirtualPipeline pipeline(options);

    constexpr std::array<Time, 14> delays = { 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 8, 0, 9, 0 };
    constexpr std::array<bool, 14> delayable = {
        true, true, true, true, true, true, true,
        false, false, false, true, false, true, false
    };
    for (size_t index = 0; index < phases.size(); ++index)
    {
        LUNATIC_CHECK(pipeline.PhaseDelay(phases[index]) == delays[index]);
        LUNATIC_CHECK(pipeline.DelayablePhase(phases[index]) == delayable[index]);
    }
    LUNATIC_CHECK(pipeline.PhaseDelay(static_cast<Phase>(255)) == 0);
    LUNATIC_CHECK(!pipeline.DelayablePhase(static_cast<Phase>(255)));

    LUNATIC_CHECK(options.randomDelayLimit == 0);
    LUNATIC_CHECK(!options.randomizeArbitration);
    LUNATIC_CHECK(options.seed == 0);
}

LUNATIC_TEST(Stage8PipelineSaturatingAdd)
{
    VirtualPipeline pipeline(Options{});
    constexpr Time maximum = std::numeric_limits<Time>::max();
    LUNATIC_CHECK(pipeline.SaturatingAdd(0, 0) == 0);
    LUNATIC_CHECK(pipeline.SaturatingAdd(7, 11) == 18);
    LUNATIC_CHECK(pipeline.SaturatingAdd(maximum - 3, 2) == maximum - 1);
    LUNATIC_CHECK(pipeline.SaturatingAdd(maximum - 3, 3) == maximum);
    LUNATIC_CHECK(pipeline.SaturatingAdd(maximum - 3, 4) == maximum);
    LUNATIC_CHECK(pipeline.SaturatingAdd(maximum, maximum) == maximum);
}

LUNATIC_TEST(Stage8PipelineTickTimeRules)
{
    Driver<Config256> driver;
    driver.node.Tick(8);
    LUNATIC_CHECK(driver.node.GetTime() == 8);
    LUNATIC_CHECK(driver.node.GetErrors().empty());

    driver.node.Tick(8);
    LUNATIC_CHECK(driver.node.GetTime() == 8);
    LUNATIC_CHECK(driver.node.GetErrors().empty());

    driver.node.Tick(7);
    LUNATIC_CHECK(driver.node.GetTime() == 8);
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.GetErrors().back().time == 8);
    driver.node.Tick(8);
    LUNATIC_CHECK(driver.node.GetTime() == 8);
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.ValidateInvariants());

    driver.node.Reset();
    LUNATIC_CHECK(driver.node.GetTime() == 0);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.ReadData(4, 1, 0x9000, CHI::Eb::Opcodes::REQ::ReadNoSnp) == LineData{});
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.HasInflight());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage8PipelineSeededTraceResetAndPurePeek)
{
    const Options options = RandomTimingOptions();
    ConcurrentReadTrace original(options);
    const auto baseline = original.Run();
    original.Reset();
    const auto replayed = original.Run();
    LUNATIC_CHECK(replayed == baseline);

    ConcurrentReadTrace sameSeed(options);
    LUNATIC_CHECK(sameSeed.Run() == baseline);

    ConcurrentReadTrace peeked(options);
    LUNATIC_CHECK(peeked.Run(7) == baseline);
}

LUNATIC_TEST(Stage8PipelineRandomizedArbitrationVariesAcrossSeeds)
{
    Options options;
    options.randomizeArbitration = true;
    options.randomDelayLimit = 0;

    std::vector<std::vector<TraceEntry>> traces;
    constexpr std::array<uint64_t, 8> seeds = { 0, 1, 2, 3, 4, 5, 6, 7 };
    for (uint64_t seed : seeds)
    {
        options.seed = seed;
        ConcurrentReadTrace run(options);
        traces.push_back(run.Run());
    }

    bool observedDifference = false;
    for (size_t index = 1; index < traces.size(); ++index)
        observedDifference = observedDifference || traces[index] != traces.front();
    LUNATIC_CHECK(observedDifference);
}

LUNATIC_TEST(Stage8PipelineDefaultTraceIgnoresSeed)
{
    Options firstOptions;
    firstOptions.seed = 0;
    ConcurrentReadTrace first(firstOptions);
    const auto baseline = first.Run();

    Options secondOptions;
    secondOptions.seed = std::numeric_limits<uint64_t>::max();
    ConcurrentReadTrace second(secondOptions);
    LUNATIC_CHECK(second.Run() == baseline);
}
