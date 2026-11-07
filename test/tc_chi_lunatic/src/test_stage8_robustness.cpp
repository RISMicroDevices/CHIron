#include "driver.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <optional>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    struct TraceEntry {
        EventKind     kind = EventKind::Error;
        Time          time = 0;
        NodeId        source = 0;
        NodeId        target = 0;
        TransactionId txnId = 0;
        uint16_t      opcode = 0;
        uint8_t       dataId = 0;

        bool operator==(const TraceEntry&) const = default;
    };

    struct RunResult {
        std::vector<TraceEntry>    trace;
        PerformanceCounters        pmu;
        std::map<LineKey, LineData> memory;
        size_t                     completed = 0;
        size_t                     canceled = 0;
        size_t                     completedReads = 0;
        size_t                     completedWrites = 0;
        size_t                     retryAcks = 0;
        size_t                     creditGrants = 0;
        size_t                     forcedRetries = 0;
        size_t                     resourceRetries = 0;
        size_t                     admissionBackpressures = 0;
        size_t                     scheduleBackpressures = 0;
        size_t                     scheduleDelays = 0;
        size_t                     heldResponses = 0;
        size_t                     heldData = 0;
        size_t                     cycles = 0;

        bool operator==(const RunResult&) const = default;
    };

    Options RobustnessOptions()
    {
        Options options;
        options.inputQueueLimit = 2;
        options.outputQueueLimit = 2;
        options.transactionLimit = 2;
        options.tagLatency = 1;
        options.dataLatency = 1;
        options.memoryLatency = 3;
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
        options.seed = 0x86c0de;
        return options;
    }

    LineData Pattern(uint64_t address)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t((address / LINE_SIZE) * 29 + index * 17 + 11);
        return data;
    }

    class MixedTraffic {
        using Base = Driver<Config256>;
        using REQ = Base::REQ;
        using RSP = Base::RSP;
        using DAT = Base::DAT;

        struct Operation {
            REQ           request{};
            NodeId        source = 0;
            TransactionId initialTxn = 0;
            TransactionId finalTxn = 0;
            TransactionId activeTxn = 0;
            uint8_t       creditType = 0;
            LineKey       line{};
            bool          read = false;
            bool          forceRetry = false;
            bool          intentionalCancel = false;
            bool          initialSent = false;
            bool          retryAck = false;
            bool          grant = false;
            bool          dbid = false;
            bool          completed = false;
            bool          canceled = false;
            size_t        readBeats = 0;
            LineData      readData{};
        };

        struct ResponseKey {
            NodeId        source = 0;
            NodeId        target = 0;
            TransactionId txnId = 0;
            DataBufferId  dbid = 0;
            uint16_t      opcode = 0;
            uint8_t       resp = 0;
            uint8_t       respErr = 0;
            uint8_t       creditType = 0;

            explicit ResponseKey(const RSP& response)
                : source(response.SrcID())
                , target(response.TgtID())
                , txnId(response.TxnID())
                , dbid(response.DBID())
                , opcode(response.Opcode())
                , resp(response.Resp())
                , respErr(response.RespErr())
                , creditType(response.PCrdType())
            {
            }

            bool operator==(const ResponseKey&) const = default;
        };

        struct DataKey {
            NodeId                       source = 0;
            NodeId                       target = 0;
            TransactionId                txnId = 0;
            DataBufferId                 dbid = 0;
            uint16_t                     opcode = 0;
            uint8_t                      dataId = 0;
            uint8_t                      respErr = 0;
            uint64_t                     enables = 0;
            std::array<uint64_t, Config256::dataWidth / 64> lanes{};

            explicit DataKey(const DAT& data)
                : source(data.SrcID())
                , target(data.TgtID())
                , txnId(data.TxnID())
                , dbid(data.DBID())
                , opcode(data.Opcode())
                , dataId(data.DataID())
                , respErr(data.RespErr())
                , enables(data.BE())
            {
                for (size_t lane = 0; lane < lanes.size(); ++lane)
                    lanes[lane] = data.Data()[lane];
            }

            bool operator==(const DataKey&) const = default;
        };

        Base                          driver;
        std::vector<Operation>        operations;
        std::deque<REQ>               controls;
        std::deque<size_t>            awaitingCredits;
        std::map<LineKey, LineData>   initialMemory;
        std::vector<TraceEntry>       trace;
        size_t                        nextInitial = 0;
        size_t                        completed = 0;
        size_t                        canceled = 0;
        size_t                        completedReads = 0;
        size_t                        completedWrites = 0;
        size_t                        retryAcks = 0;
        size_t                        creditGrants = 0;
        size_t                        forcedRetries = 0;
        size_t                        resourceRetries = 0;
        size_t                        creditedResends = 0;
        size_t                        returnedCredits = 0;
        size_t                        admissionDecisions = 0;
        size_t                        scheduleDecisions = 0;
        size_t                        admissionBackpressures = 0;
        size_t                        scheduleBackpressures = 0;
        size_t                        scheduleDelays = 0;
        size_t                        heldResponses = 0;
        size_t                        heldData = 0;
        size_t                        retiredEvents = 0;
        size_t                        deniedEvents = 0;
        size_t                        resetEvents = 0;

        template<typename Event>
        void Record(const Event& event)
        {
            trace.push_back({ event.kind, event.time, event.source, event.target,
                event.txnId, event.opcode, 0 });
        }

        size_t FindInitial(NodeId source, TransactionId txnId) const
        {
            for (size_t index = 0; index < operations.size(); ++index)
            {
                if (operations[index].source == source && operations[index].initialTxn == txnId)
                    return index;
            }
            return operations.size();
        }

        size_t FindActive(NodeId source, TransactionId txnId) const
        {
            for (size_t index = 0; index < operations.size(); ++index)
            {
                const auto& operation = operations[index];
                if (operation.source == source && operation.activeTxn == txnId &&
                    !operation.completed && !operation.canceled)
                    return index;
            }
            return operations.size();
        }

        std::optional<RSP> StableResponse()
        {
            const auto pmu = driver.node.GetPerformanceCounters();
            const auto time = driver.node.GetTime();
            const auto eventCount = trace.size();
            const bool inflight = driver.node.HasInflight();
            const auto first = driver.node.PeekTXRSP();
            for (size_t index = 0; index < 4; ++index)
            {
                const auto next = driver.node.PeekTXRSP();
                LUNATIC_CHECK(next.has_value() == first.has_value());
                if (first)
                    LUNATIC_CHECK(ResponseKey(*next) == ResponseKey(*first));
                LUNATIC_CHECK(driver.node.GetPerformanceCounters() == pmu);
                LUNATIC_CHECK(driver.node.GetTime() == time);
                LUNATIC_CHECK(trace.size() == eventCount);
                LUNATIC_CHECK(driver.node.HasInflight() == inflight);
                LUNATIC_CHECK(driver.node.ValidateInvariants());
            }
            return first;
        }

        std::optional<DAT> StableData()
        {
            const auto pmu = driver.node.GetPerformanceCounters();
            const auto time = driver.node.GetTime();
            const auto eventCount = trace.size();
            const bool inflight = driver.node.HasInflight();
            const auto first = driver.node.PeekTXDAT();
            for (size_t index = 0; index < 4; ++index)
            {
                const auto next = driver.node.PeekTXDAT();
                LUNATIC_CHECK(next.has_value() == first.has_value());
                if (first)
                    LUNATIC_CHECK(DataKey(*next) == DataKey(*first));
                LUNATIC_CHECK(driver.node.GetPerformanceCounters() == pmu);
                LUNATIC_CHECK(driver.node.GetTime() == time);
                LUNATIC_CHECK(trace.size() == eventCount);
                LUNATIC_CHECK(driver.node.HasInflight() == inflight);
                LUNATIC_CHECK(driver.node.ValidateInvariants());
            }
            return first;
        }

        void Complete(size_t index)
        {
            auto& operation = operations[index];
            LUNATIC_CHECK(!operation.completed && !operation.canceled);
            operation.completed = true;
            ++completed;
            if (operation.read)
                ++completedReads;
            else
                ++completedWrites;
        }

        void HandleResponse(const RSP& response)
        {
            namespace Opcodes = CHI::Eb::Opcodes::RSP;
            if (response.Opcode() == Opcodes::RetryAck)
            {
                const size_t index = FindInitial(response.TgtID(), response.TxnID());
                LUNATIC_CHECK(index < operations.size());
                auto& operation = operations[index];
                LUNATIC_CHECK(operation.initialSent && !operation.retryAck);
                LUNATIC_CHECK(!operation.completed && !operation.canceled);
                operation.creditType = response.PCrdType();
                operation.retryAck = true;
                awaitingCredits.push_back(index);
                ++retryAcks;
                if (operation.forceRetry)
                    ++forcedRetries;
                else
                    ++resourceRetries;
                return;
            }

            if (response.Opcode() == Opcodes::PCrdGrant)
            {
                const auto match = std::find_if(awaitingCredits.begin(), awaitingCredits.end(),
                    [&](size_t index) {
                        const auto& operation = operations[index];
                        return operation.source == response.TgtID() &&
                            operation.creditType == response.PCrdType();
                    });
                LUNATIC_CHECK(match != awaitingCredits.end());
                const size_t index = *match;
                awaitingCredits.erase(match);
                auto& operation = operations[index];
                LUNATIC_CHECK(operation.retryAck && !operation.grant);
                operation.grant = true;
                ++creditGrants;

                if (operation.intentionalCancel)
                {
                    REQ creditReturn{};
                    creditReturn.SrcID() = operation.source;
                    creditReturn.TgtID() = driver.node.GetNodeId();
                    creditReturn.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
                    creditReturn.PCrdType() = operation.creditType;
                    controls.push_back(creditReturn);
                    operation.canceled = true;
                    ++canceled;
                    ++returnedCredits;
                    return;
                }

                auto resend = operation.request;
                operation.activeTxn = operation.finalTxn;
                resend.TxnID() = operation.finalTxn;
                resend.AllowRetry() = 0;
                resend.PCrdType() = operation.creditType;
                LUNATIC_CHECK(resend.TxnID() != operation.initialTxn);
                controls.push_back(resend);
                ++creditedResends;
                return;
            }

            const size_t index = FindActive(response.TgtID(), response.TxnID());
            LUNATIC_CHECK(index < operations.size());
            auto& operation = operations[index];
            LUNATIC_CHECK(!operation.read);
            if (response.Opcode() == Opcodes::DBIDResp)
            {
                LUNATIC_CHECK(!operation.dbid);
                operation.dbid = true;
                return;
            }
            if (response.Opcode() == Opcodes::CompDBIDResp)
            {
                LUNATIC_CHECK(!operation.dbid);
                Complete(index);
                return;
            }
            LUNATIC_CHECK(response.Opcode() == Opcodes::Comp);
            LUNATIC_CHECK(operation.dbid);
            Complete(index);
        }

        void HandleData(const DAT& data)
        {
            LUNATIC_CHECK(data.Opcode() == CHI::Eb::Opcodes::DAT::CompData);
            const size_t index = FindActive(data.TgtID(), data.TxnID());
            LUNATIC_CHECK(index < operations.size());
            auto& operation = operations[index];
            LUNATIC_CHECK(operation.read);
            constexpr size_t bytes = Config256::dataWidth / 8;
            LUNATIC_CHECK(operation.readBeats < BeatsPerLine(driver.node.GetOptions()));
            LUNATIC_CHECK(data.DataID() == operation.readBeats * bytes / 16);
            LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::OK);
            for (size_t lane = 0; lane < Config256::dataWidth / 64; ++lane)
            {
                for (size_t byte = 0; byte < 8; ++byte)
                {
                    operation.readData[operation.readBeats * bytes + lane * 8 + byte] =
                        uint8_t(data.Data()[lane] >> (byte * 8));
                }
            }
            ++operation.readBeats;
            if (operation.readBeats == BeatsPerLine(driver.node.GetOptions()))
            {
                LUNATIC_CHECK(operation.readData == initialMemory.at(operation.line));
                Complete(index);
            }
        }

        void OfferRequests()
        {
            while (!controls.empty())
            {
                if (!driver.node.PushRXREQ(controls.front()))
                    return;
                controls.pop_front();
            }
            while (nextInitial < operations.size())
            {
                auto& operation = operations[nextInitial];
                if (!driver.node.PushRXREQ(operation.request))
                    return;
                operation.initialSent = true;
                ++nextInitial;
            }
        }

        bool Accounted() const
        {
            return completed + canceled == operations.size();
        }

        RunResult Finish()
        {
            LUNATIC_CHECK(driver.node.Drain());
            LUNATIC_CHECK(!driver.node.HasInflight());
            LUNATIC_CHECK(!driver.node.PeekTXREQ());
            LUNATIC_CHECK(!driver.node.PeekTXRSP());
            LUNATIC_CHECK(!driver.node.PeekTXDAT());
            LUNATIC_CHECK(!driver.node.PeekTXSNP());
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            LUNATIC_CHECK(driver.node.ValidateInvariants());
            LUNATIC_CHECK(deniedEvents == 0);

            size_t expectedCompleted = 0;
            size_t expectedCanceled = 0;
            size_t expectedForced = 0;
            auto finalMemory = initialMemory;
            for (const auto& operation : operations)
            {
                LUNATIC_CHECK(operation.completed != operation.canceled);
                LUNATIC_CHECK(operation.initialSent);
                expectedCompleted += operation.completed ? 1 : 0;
                expectedCanceled += operation.canceled ? 1 : 0;
                expectedForced += operation.forceRetry ? 1 : 0;
                if (operation.completed && !operation.read)
                    finalMemory[operation.line] = LineData{};
            }
            LUNATIC_CHECK(completed == expectedCompleted);
            LUNATIC_CHECK(canceled == expectedCanceled);
            LUNATIC_CHECK(completedReads == 5);
            LUNATIC_CHECK(completedWrites == 5);
            LUNATIC_CHECK(completed == completedReads + completedWrites);
            LUNATIC_CHECK(canceled == 2);
            LUNATIC_CHECK(retiredEvents == completed);
            LUNATIC_CHECK(forcedRetries == expectedForced);
            LUNATIC_CHECK(resourceRetries > 0);
            LUNATIC_CHECK(retryAcks == forcedRetries + resourceRetries);
            LUNATIC_CHECK(creditGrants == retryAcks);
            LUNATIC_CHECK(creditedResends + returnedCredits == retryAcks);
            LUNATIC_CHECK(returnedCredits == canceled);
            LUNATIC_CHECK(awaitingCredits.empty());
            LUNATIC_CHECK(controls.empty());
            LUNATIC_CHECK(admissionBackpressures > 0);
            LUNATIC_CHECK(scheduleBackpressures > 0);
            LUNATIC_CHECK(scheduleDelays > 0);
            LUNATIC_CHECK(heldResponses > 0);
            LUNATIC_CHECK(heldData > 0);

            for (const auto& [line, data] : finalMemory)
                LUNATIC_CHECK(driver.node.GetMemory().Read(line) == data);

            const auto pmu = driver.node.GetPerformanceCounters();
            LUNATIC_CHECK(pmu.acceptedRXREQ == operations.size() + retryAcks);
            LUNATIC_CHECK(pmu.acceptedRXRSP == 0);
            LUNATIC_CHECK(pmu.acceptedRXDAT == 0);
            LUNATIC_CHECK(pmu.emittedTXREQ == 0);
            LUNATIC_CHECK(pmu.emittedTXSNP == 0);
            LUNATIC_CHECK(pmu.emittedTXDAT == completedReads * BeatsPerLine(driver.node.GetOptions()));
            LUNATIC_CHECK(pmu.emittedTXRSP == retryAcks * 2 + completedWrites);
            LUNATIC_CHECK(pmu.admittedTransactions == completed);
            LUNATIC_CHECK(pmu.retiredTransactions == completed);
            LUNATIC_CHECK(pmu.emittedRetryAcks == retryAcks);
            LUNATIC_CHECK(pmu.emittedCreditGrants == creditGrants);
            LUNATIC_CHECK(pmu.creditedResends == creditedResends);
            LUNATIC_CHECK(pmu.returnedCredits == returnedCredits);
            LUNATIC_CHECK(pmu.GetStalls(WaitReason::InputQueue) > 0);
            LUNATIC_CHECK(pmu.GetStalls(WaitReason::OutputQueue) > 0);
            LUNATIC_CHECK(pmu.GetStalls(WaitReason::Tracker) > 0);
            LUNATIC_CHECK(pmu.GetStalls(WaitReason::Decision) > 0);

            return { trace, pmu, finalMemory, completed, canceled, completedReads, completedWrites,
                retryAcks, creditGrants, forcedRetries, resourceRetries, admissionBackpressures,
                scheduleBackpressures, scheduleDelays, heldResponses, heldData, size_t(driver.now) };
        }

    public:
        explicit MixedTraffic(Options options)
            : driver(options)
        {
            auto events = driver.node.GetEvents();
            events->OnAcceptedRXREQ.Register(
                Gravity::MakeListener<AcceptedRXREQEvent>("robustness-accepted", 0,
                    [this](AcceptedRXREQEvent& event) { Record(event); }));
            events->OnEmittedTXRSP.Register(
                Gravity::MakeListener<EmittedTXRSPEvent>("robustness-rsp", 0,
                    [this](EmittedTXRSPEvent& event) { Record(event); }));
            events->OnEmittedTXDAT.Register(
                Gravity::MakeListener<EmittedTXDATEvent>("robustness-dat", 0,
                    [this](EmittedTXDATEvent& event) { Record(event); }));
            events->OnRetired.Register(
                Gravity::MakeListener<RetiredEvent>("robustness-retired", 0,
                    [this](RetiredEvent& event) {
                        Record(event);
                        ++retiredEvents;
                    }));
            events->OnDenied.Register(
                Gravity::MakeListener<DeniedEvent>("robustness-denied", 0,
                    [this](DeniedEvent&) { ++deniedEvents; }));
            events->OnReset.Register(
                Gravity::MakeListener<ResetEvent>("robustness-reset", 0,
                    [this](ResetEvent&) { ++resetEvents; }));
            events->OnRequestAdmission.Register(
                Gravity::MakeListener<RequestAdmissionEvent>("robustness-admission", 0,
                    [this](RequestAdmissionEvent& event) {
                        const size_t decision = admissionDecisions++;
                        if (event.request.opcode != CHI::Eb::Opcodes::REQ::PCrdReturn && decision % 8 == 3)
                        {
                            ++admissionBackpressures;
                            event.Backpressure();
                            return;
                        }
                        if (!event.request.allowRetry)
                            return;
                        const size_t index = FindInitial(event.request.source, event.request.txnId);
                        if (index < operations.size() && operations[index].forceRetry &&
                            !operations[index].retryAck)
                            event.RequestRetry();
                    }));
            events->OnTransactionSchedule.Register(
                Gravity::MakeListener<TransactionScheduleEvent>("robustness-schedule", 0,
                    [this](TransactionScheduleEvent& event) {
                        const size_t decision = scheduleDecisions++;
                        if (event.phase == Phase::Decode && decision % 5 == 2)
                        {
                            ++scheduleBackpressures;
                            event.Backpressure();
                        }
                        else if ((event.phase == Phase::Lookup || event.phase == Phase::Return) &&
                                 decision % 3 == 1)
                        {
                            ++scheduleDelays;
                            event.AddDelay(1 + decision % 2);
                        }
                    }));
        }

        void Start()
        {
            LUNATIC_CHECK(driver.node.GetTime() == 0);
            LUNATIC_CHECK(!driver.node.HasInflight());
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            driver.now = 0;
            operations.clear();
            controls.clear();
            awaitingCredits.clear();
            initialMemory.clear();
            trace.clear();
            nextInitial = 0;
            completed = canceled = completedReads = completedWrites = 0;
            retryAcks = creditGrants = forcedRetries = resourceRetries = 0;
            creditedResends = returnedCredits = 0;
            admissionDecisions = scheduleDecisions = 0;
            admissionBackpressures = scheduleBackpressures = scheduleDelays = 0;
            heldResponses = heldData = 0;
            retiredEvents = deniedEvents = 0;

            struct Description {
                NodeId   source;
                bool     read;
                uint64_t address;
                bool     forceRetry;
                bool     cancel;
            };
            const std::array descriptions = {
                Description{ 4, true,  0x10000, false, false },
                Description{ 5, false, 0x30000, true,  false },
                Description{ 7, true,  0x10040, true,  true  },
                Description{ 4, false, 0x30040, false, false },
                Description{ 5, true,  0x10000, true,  false },
                Description{ 7, false, 0x30000, false, false },
                Description{ 4, true,  0x20000, false, false },
                Description{ 5, false, 0x40000, true,  true  },
                Description{ 7, true,  0x10040, false, false },
                Description{ 4, false, 0x30040, true,  false },
                Description{ 5, true,  0x20040, false, false },
                Description{ 7, false, 0x40040, false, false }
            };

            for (size_t index = 0; index < descriptions.size(); ++index)
            {
                const auto& description = descriptions[index];
                const TransactionId initialTxn = TransactionId(0x10 + index);
                Operation operation;
                operation.request = driver.Request(
                    description.read ? CHI::Eb::Opcodes::REQ::ReadNoSnp : CHI::Eb::Opcodes::REQ::WriteNoSnpZero,
                    description.source, initialTxn, description.address, false);
                LUNATIC_CHECK(operation.request.PCrdType() == 0);
                operation.source = description.source;
                operation.initialTxn = initialTxn;
                operation.finalTxn = TransactionId(0x80 + index);
                operation.activeTxn = initialTxn;
                operation.line = LineKey::FromAddress(description.address, false);
                operation.read = description.read;
                operation.forceRetry = description.forceRetry;
                operation.intentionalCancel = description.cancel;
                operations.push_back(operation);
                initialMemory.emplace(operation.line, Pattern(description.address));
            }

            for (const auto& [line, data] : initialMemory)
                driver.node.GetMemory().WriteFull(line, data);
        }

        void Step()
        {
            OfferRequests();
            driver.Tick();
            LUNATIC_CHECK(driver.node.ValidateInvariants());
            LUNATIC_CHECK(!driver.node.PeekTXREQ());
            LUNATIC_CHECK(!driver.node.PeekTXSNP());

            const auto response = StableResponse();
            const bool acceptResponse = driver.now > 6 && driver.now % 4 == 0;
            if (response && acceptResponse)
            {
                const auto popped = driver.node.PopTXRSP();
                LUNATIC_CHECK(popped);
                LUNATIC_CHECK(ResponseKey(*popped) == ResponseKey(*response));
                HandleResponse(*popped);
            }
            else if (response)
                ++heldResponses;

            const auto data = StableData();
            const bool acceptData = driver.now > 9 && driver.now % 6 == 2;
            if (data && acceptData)
            {
                const auto popped = driver.node.PopTXDAT();
                LUNATIC_CHECK(popped);
                LUNATIC_CHECK(DataKey(*popped) == DataKey(*data));
                LUNATIC_CHECK(!trace.empty() && trace.back().kind == EventKind::EmittedTXDAT);
                trace.back().dataId = popped->DataID();
                HandleData(*popped);
            }
            else if (data)
                ++heldData;
        }

        RunResult Run()
        {
            Start();
            bool drained = false;
            for (size_t cycle = 0; cycle < 1500; ++cycle)
            {
                Step();
                if (Accounted() && nextInitial == operations.size() && controls.empty() && driver.node.Drain())
                {
                    drained = true;
                    break;
                }
            }
            LUNATIC_CHECK(drained);
            LUNATIC_CHECK(driver.now < 1500);
            return Finish();
        }

        void RunPartial()
        {
            Start();
            bool partial = false;
            for (size_t cycle = 0; cycle < 500; ++cycle)
            {
                Step();
                const auto pmu = driver.node.GetPerformanceCounters();
                if (completed > 0 && canceled > 0 && pmu.emittedRetryAcks > 0 && driver.node.HasInflight())
                {
                    partial = true;
                    break;
                }
            }
            LUNATIC_CHECK(partial);
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            LUNATIC_CHECK(driver.node.ValidateInvariants());
        }

        void Reset()
        {
            driver.node.Reset();
            driver.now = 0;
            LUNATIC_CHECK(resetEvents == 1);
            LUNATIC_CHECK(driver.node.GetTime() == 0);
            LUNATIC_CHECK(driver.node.GetPerformanceCounters() == PerformanceCounters{});
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            LUNATIC_CHECK(!driver.node.HasInflight());
            LUNATIC_CHECK(driver.node.ValidateInvariants());
            LUNATIC_CHECK(driver.node.Drain());
            LUNATIC_CHECK(driver.node.GetPerformanceCounters() == PerformanceCounters{});
        }
    };

}

LUNATIC_TEST(Stage8RobustnessMixedTrafficSameSeedReplay)
{
    MixedTraffic original(RobustnessOptions());
    const auto baseline = original.Run();
    MixedTraffic replay(RobustnessOptions());
    const auto repeated = replay.Run();
    LUNATIC_CHECK(repeated == baseline);
}

LUNATIC_TEST(Stage8RobustnessResetRestartsSeededMixedTraffic)
{
    MixedTraffic fresh(RobustnessOptions());
    const auto baseline = fresh.Run();

    MixedTraffic restarted(RobustnessOptions());
    restarted.RunPartial();
    restarted.Reset();
    const auto replayed = restarted.Run();
    LUNATIC_CHECK(replayed == baseline);
}

LUNATIC_TEST(Stage8RobustnessPersistentAdmissionBackpressureBlocksDrain)
{
    Driver<Config256> driver;
    bool hold = true;
    size_t blocked = 0;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("persistent-admission", 0,
            [&](RequestAdmissionEvent& event) {
                if (hold)
                {
                    ++blocked;
                    event.Backpressure();
                }
            }));

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x50000, false)));
    for (size_t cycle = 0; cycle < 6; ++cycle)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.Drain());
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
        LUNATIC_CHECK(driver.node.HasInflight());
        LUNATIC_CHECK(driver.node.ValidateInvariants());
    }
    LUNATIC_CHECK(blocked >= 6);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().GetStalls(WaitReason::Decision) >= 6);

    hold = false;
    for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TgtID() == 4);
        LUNATIC_CHECK(data.TxnID() == 7);
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(!driver.node.HasInflight());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage8RobustnessLateAdmissionCannotBypassSameLineHazard)
{
    Driver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x58000, false);
    bool holdOlderAdmit = true;
    bool holdOlderAction = true;
    bool olderAdmitSeen = false;
    bool olderActionSeen = false;
    bool youngerActionSeen = false;

    driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("late-admission-hazard", 0,
            [&](TransactionScheduleEvent& event) {
                if (event.transaction == 0 && event.phase == Phase::Admit && holdOlderAdmit)
                {
                    olderAdmitSeen = true;
                    event.Backpressure();
                }
                if (event.transaction == 0 && event.phase == Phase::Action && holdOlderAction)
                {
                    olderActionSeen = true;
                    event.Backpressure();
                }
                if (event.transaction == 1 && event.phase == Phase::Action)
                    youngerActionSeen = true;
            }));

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 4, 40, line.address)));
    driver.Until([&] { return olderAdmitSeen; });
    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 5, 41, line.address)));

    for (size_t cycle = 0; cycle < 12; ++cycle)
    {
        driver.Tick();
        LUNATIC_CHECK(!youngerActionSeen);
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
        LUNATIC_CHECK(!driver.node.PeekTXSNP());
    }

    holdOlderAdmit = false;
    driver.Until([&] { return olderActionSeen; });
    LUNATIC_CHECK(!youngerActionSeen);
    holdOlderAction = false;

    const auto first = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(first.TgtID() == 4);
    LUNATIC_CHECK(first.Resp() == CHI::Eb::Resps::Comp_UC);
    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        driver.Tick();
        LUNATIC_CHECK(!youngerActionSeen);
        LUNATIC_CHECK(!driver.node.PeekTXSNP());
    }
    driver.CompAck(4, first.DBID());

    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 4);
    const LineData discarded{};
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, snoop.flit.TxnID(), snoop.flit.TxnID(), discarded, enables,
        CHI::Eb::Opcodes::DAT::SnpRespData, CHI::Eb::Resps::SnpRespData_I_PD);

    const auto second = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(second.TgtID() == 5);
    LUNATIC_CHECK(second.Resp() == CHI::Eb::Resps::Comp_UC);
    driver.CompAck(5, second.DBID());
    driver.Quiesce();

    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 5) == CacheState::UD);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}
