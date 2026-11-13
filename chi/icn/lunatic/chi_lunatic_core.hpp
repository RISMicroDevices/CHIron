#pragma once

#include "chi_lunatic_arbiter.hpp"
#include "chi_lunatic_atomic.hpp"
#include "chi_lunatic_bank.hpp"
#include "chi_lunatic_backend_axi.hpp"
#include "chi_lunatic_component.hpp"
#include "chi_lunatic_directory.hpp"
#include "chi_lunatic_hazard.hpp"
#include "chi_lunatic_memory.hpp"
#include "chi_lunatic_ordering.hpp"
#include "chi_lunatic_pipeline.hpp"
#include "chi_lunatic_policy.hpp"
#include "chi_lunatic_protocol.hpp"
#include "chi_lunatic_random.hpp"
#include "chi_lunatic_snoop_filter.hpp"
#include "chi_lunatic_tracker.hpp"

#include <deque>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <type_traits>

namespace CHI::Lunatic {

    enum class BackendPhase : uint8_t {
        None,
        WaitRequestEmission,
        WaitDBID,
        SendData,
        WaitDataEmission,
        WaitComp,
        WaitRetry,
        Done
    };

    struct AtomicFlowState {
        LineData        operands{};
        LinePoison      operandsPoison{};
        LineDataCheck   operandsDataCheck{};
        AtomicExecution execution{};
        uint64_t        resultBeatMask = 0;
        bool            returnSent = false;
        bool            returnEmitted = false;
        bool            backendResultReceived = false;
        bool            backendDirtyCommitted = false;
    };

    struct FlowState {
        Phase                      timingPhase = Phase::Retired;
        bool                       timingApplied = false;
        Phase                      eventPhase = Phase::Retired;
        bool                       eventApplied = false;
        std::map<SchedulePoint, Time> scheduleMarks;
        BackendPhase               backendPhase = BackendPhase::None;
        Request                    backendRequest{};
        Time                       memoryReady = 0;
        bool                       memoryWrite = false;
        DataBufferId               backendDBID = 0;
        uint64_t                   expectedBeatMask = 0;
        uint64_t                   backendExpectedBeatMask = 0;
        uint64_t                   backendBeatMask = 0;
        std::map<size_t, uint8_t>  backendBeatRespErr;
        size_t                     backendBeat = 0;
        size_t                     dataBeatScheduled = 0;
        size_t                     nextSnoop = 0;
        bool                       requestBufferOwned = false;
        bool                       snoopWaveOwned = false;
        bool                       responseOwned = false;
        bool                       backendOperationOwned = false;
        bool                       baselineOperationOwned = false;
        size_t                     assignedClass = 0;
        bool                       classOwned = false;
        uint8_t                    writeResp = 0;
        std::map<NodeId, uint64_t> snoopBeatMasks;
        std::map<NodeId, uint8_t>  snoopResponses;
        bool                       readReceiptSent = false;
        bool                       readReceiptEmitted = false;
        bool                       separatedResponse = false;
        bool                       separatedBackendRead = false;
        bool                       dmtActive = false;
        bool                       dctActive = false;
        bool                       dctForwarded = false;
        bool                       dctSnoopEmitted = false;
        bool                       dctProviderResponded = false;
        NodeId                     dctTarget = 0;
        uint8_t                    dctFwdState = 0;
        bool                       retToSrcUsed = false;
        bool                       dwtActive = false;
        bool                       directPersistActive = false;
        bool                       respSepSent = false;
        bool                       respSepEmitted = false;
        size_t                     readBeatsEmitted = 0;
        bool                       backendReadReceiptReceived = false;
        bool                       directoryCommitted = false;
        bool                       baselineFetch = false;
        bool                       baselineFetched = false;
        LineKey                    baselineLine{};
        bool                       snoopDataPartial = false;
        ByteEnables                snoopValidBytes{};
        ByteEnables                validBytes{};
        bool                       spillPending = false;
        bool                       spillGeneratedEviction = false;
        LineKey                    spillLine{};
        LineData                   spillData{};
        LinePoison                 spillPoison{};
        LineDataCheck              spillDataCheck{};
        ByteEnables                spillEnables{};
        bool                       victimSnoopFilterOnly = false;
        bool                       dbidSent = false;
        bool                       compSent = false;
        uint8_t                    responseForm = 0;
        bool                       compEmitted = false;
        bool                       dbidEmitted = false;
        bool                       stashDoneSent = false;
        bool                       stashDoneEmitted = false;
        bool                       evictOrEvictNoData = false;
        bool                       mergeImmediateHome = false;
        bool                       backendCompReceived = false;
        bool                       backendPersistReceived = false;
        bool                       backendSeparatedPersistResponse = false;
        bool                       backendPersistFirst = false;
        bool                       persistSent = false;
        bool                       persistEmitted = false;
        bool                       backendDataEmitted = false;
        bool                       snoopDataValid = false;
        bool                       dataDirty = false;
        bool                       passDirty = false;
        bool                       requesterWasValid = false;
        bool                       requesterWasDirty = false;
        CacheState                 requesterFinal = CacheState::I;
        LineKey                    snoopLine{};
        bool                       snoopForVictim = false;
        bool                       victimPending = false;
        bool                       victimLinked = false;
        bool                       victimDataValid = false;
        bool                       victimDataPartial = false;
        ByteEnables                victimValidBytes{};
        uint64_t                   victimGeneration = 0;
        bool                       victimResponseUpdated = false;
        bool                       victimDirty = false;
        size_t                     victimWay = 0;
        LineKey                    victimLine{};
        LineData                   victimData{};
        LinePoison                 victimPoison{};
        LineDataCheck              victimDataCheck{};
        bool                       allocateHome = false;
        bool                       removeHome = false;
        bool                       retainHome = false;
        bool                       homeDirty = false;
        AtomicFlowState            atomic;
    };

    struct WeightedRoundState {
        size_t   index = 0;
        uint64_t remaining = 0;
    };

    struct RetryRecord {
        Request request;
        size_t  bank = 0;
        size_t  classId = 0;
        Time    age = 0;
        bool    ackPopped = false;
        bool    grantQueued = false;
        bool    creditReserved = false;
        bool    creditReturned = false;
    };

    struct BackendRetryRecord {
        Request           request;
        TransactionHandle transaction = INVALID_TRANSACTION;
        uint64_t          serial = 0;
        size_t            bank = 0;
        size_t            classId = 0;
        uint8_t           retryAckPCrdType = 0;
        bool              creditReceived = false;
        uint64_t          selection = std::numeric_limits<uint64_t>::max();
    };

    template<typename T>
    struct Pending {
        T                 flit;
        TransactionHandle transaction = INVALID_TRANSACTION;
        Time              age = 0;
        uint64_t          serial = 0;
        size_t            retry = std::numeric_limits<size_t>::max();
        bool              last = false;
        bool              downstream = false;
        SchedulePoint     schedule = SchedulePoint::Automatic;
        Time              readyTime = 0;
    };

    SchedulePoint OutputPoint(const Request& flit, bool downstream, size_t retry);
    SchedulePoint OutputPoint(const Response& flit, bool downstream, size_t retry);
    SchedulePoint OutputPoint(const Data& flit, bool downstream, size_t retry);
    SchedulePoint OutputPoint(const Snoop& flit, bool downstream, size_t retry);

    struct BankState {
        Directory       directory;
        SnoopFilter     snoopFilter;
        HazardGraph     hazards;
        VirtualPipeline pipeline;
        size_t          activeOriginals = 0;
        size_t          requestBufferOccupants = 0;
        size_t          activeSnoopWaves = 0;
        size_t          outgoingSnoopQueue = 0;
        size_t          responseUnits = 0;
        size_t          backendOperations = 0;
        size_t          refillOperations = 0;
        std::array<size_t, 4> activeClasses{};
        std::array<size_t, 4> retryReservations{};

        explicit BankState(const Options& options, std::shared_ptr<DeterministicRandom> random = {})
            : directory(options, std::move(random)), snoopFilter(options, std::move(random)), pipeline(options)
        {
        }
    };

    struct Core::Impl {
        NodeId                    nodeId;
        Options                   options;
        std::shared_ptr<EventHub> events;
        Time                      time = 0;
        PerformanceCounters       pmu;
        BankMap                   bankMap;
        std::vector<BankState>    banks;
        std::unique_ptr<CachePolicy>
                                  policy;
        Tracker                   tracker;
        OrderingTracker           ordering;
        DeterministicRandom       random;
        SparseMemory              memory;
        AXIBackend                axi;
        std::map<TransactionHandle, FlowState>
                                  flows;
        std::map<LineKey, size_t> providerNext;
        std::map<size_t, RetryRecord>
                                  retries;
        std::map<std::pair<NodeId, uint8_t>, size_t>
                                  availableCredits;
        std::map<std::pair<NodeId, uint8_t>, size_t>
                                  cancellationCredits;
        std::map<std::pair<NodeId, std::pair<size_t, size_t>>, size_t>
                                  retrySources;
        std::map<TransactionHandle, BackendRetryRecord>
                                  backendRetries;
        std::map<std::pair<NodeId, uint8_t>, size_t>
                                  backendCredits;
        std::map<RequestKey, Request>
                                  completedBackendReads;
        std::list<RequestKey>             completedBackendReadOrder;
        size_t                    nextBackendRetry = 0;
        size_t                    nextRetry = 0;
        uint64_t                  nextAge = 0;
        uint64_t                  nextSerial = 0;
        uint64_t                  nextBackendSelection = 0;
        std::array<WeightedRoundState, 3> retryRounds{};
        std::array<WeightedRoundState, 3> backendRetryRounds{};
        WeightedRoundState          requestClassRound{};
        WeightedRoundState          responseCategoryRound{};
        std::array<WeightedRoundState, 4> responseClassRounds{};

        std::deque<Request>           rxreq;
        std::deque<Response>          rxrsp;
        std::deque<Data>              rxdat;
        std::deque<Time>              rxreqTimes;
        std::deque<Time>              rxrspTimes;
        std::deque<Time>              rxdatTimes;
        std::deque<Pending<Request>>  txreq;
        std::deque<Pending<Response>> txrsp;
        std::deque<Pending<Data>>     txdat;
        std::deque<Pending<Snoop>>    txsnp;
        std::vector<ErrorInfo>        errors;

        Impl(NodeId nodeId, Options options, std::shared_ptr<EventHub> events);

        template<typename T>
        bool Push(std::deque<T>& queue, const T& flit, EventKind kind)
        {
            if (queue.size() >= options.inputQueueLimit)
            {
                pmu.RecordStall(WaitReason::InputQueue);
                return false;
            }
            queue.push_back(flit);
            const Time readyTime = banks.front().pipeline.SaturatingAdd(time,
                banks.front().pipeline.OperationDelay(SchedulePoint::InputRegister));
            if constexpr (std::is_same_v<T, Request>)
                rxreqTimes.push_back(readyTime);
            else if constexpr (std::is_same_v<T, Response>)
                rxrspTimes.push_back(readyTime);
            else if constexpr (std::is_same_v<T, Data>)
                rxdatTimes.push_back(readyTime);
            Publish(kind, flit);
            return true;
        }

        template<typename T>
        void Publish(EventKind kind, const T& flit, TransactionHandle handle = INVALID_TRANSACTION)
        {
            switch (kind)
            {
                case EventKind::AcceptedRXREQ: ++pmu.acceptedRXREQ; break;
                case EventKind::AcceptedRXRSP: ++pmu.acceptedRXRSP; break;
                case EventKind::AcceptedRXDAT: ++pmu.acceptedRXDAT; break;
                case EventKind::EmittedTXREQ: ++pmu.emittedTXREQ; break;
                case EventKind::EmittedTXRSP:
                    ++pmu.emittedTXRSP;
                    if (flit.opcode == CHI::Eb::Opcodes::RSP::RetryAck)
                        ++pmu.emittedRetryAcks;
                    else if (flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant)
                        ++pmu.emittedCreditGrants;
                    break;
                case EventKind::EmittedTXDAT: ++pmu.emittedTXDAT; break;
                case EventKind::EmittedTXSNP: ++pmu.emittedTXSNP; break;
                default: break;
            }

            Event event;
            event.kind = kind;
            event.time = time;
            event.home = nodeId;
            event.source = flit.source;
            event.target = flit.target;
            event.txnId = flit.txnId;
            event.opcode = flit.opcode;
            event.transaction = handle;
            if constexpr (requires { flit.Line(); })
                event.line = flit.Line();
            const bool emitted = kind == EventKind::EmittedTXREQ || kind == EventKind::EmittedTXRSP ||
                kind == EventKind::EmittedTXDAT || kind == EventKind::EmittedTXSNP;
            events->Publish(std::move(event), !emitted);
        }

        template<size_t N>
        static size_t SelectWeighted(const std::array<uint64_t, N>& weights,
                                     const std::array<bool, N>& eligible,
                                     WeightedRoundState& state, bool commit)
        {
            WeightedRoundState next = state;
            size_t selected = N;
            const size_t current = next.index < N ? next.index : 0;
            if (next.remaining && eligible[current])
            {
                selected = current;
            }
            else
            {
                const size_t offset = next.remaining ? 1 : 0;
                for (size_t step = 0; step < N; ++step)
                {
                    const size_t candidate = (current + offset + step) % N;
                    if (!eligible[candidate])
                        continue;
                    selected = candidate;
                    break;
                }
                if (selected == N)
                    return N;
                next.index = selected;
                next.remaining = weights[selected] ? weights[selected] : 1;
            }
            --next.remaining;
            if (!next.remaining)
                next.index = (selected + 1) % N;
            if (commit)
                state = next;
            return selected;
        }

        template<typename Predicate>
        auto SelectRetryIf(Predicate predicate)
        {
            if (options.strategy.upstreamRetrySelection == RetrySelectionKind::Oldest)
                for (auto iter = retries.begin(); iter != retries.end(); ++iter)
                    if (predicate(iter))
                        return iter;
            for (auto iter = retries.rbegin(); iter != retries.rend(); ++iter)
                if (predicate(iter))
                    return std::prev(iter.base());
            return retries.end();
        }

        template<typename T>
        size_t Select(const std::deque<Pending<T>>& queue, bool commit = false)
        {
            if constexpr (std::is_same_v<T, Response>)
            {
                if (options.structure.classes.enabled &&
                    options.strategy.outputArbitration == OutputArbitrationKind::OldestClassWeighted)
                    return SelectResponse(queue, std::nullopt, commit);
            }
            size_t selected = queue.size();
            for (size_t index = 0; index < queue.size(); ++index)
            {
                if (queue[index].readyTime > time)
                    continue;
                if (selected == queue.size() || queue[index].age < queue[selected].age ||
                    (queue[index].age == queue[selected].age && queue[index].serial < queue[selected].serial))
                    selected = index;
            }
            return selected;
        }

        template<typename T>
        size_t Select(const std::deque<Pending<T>>& queue, bool downstream, bool commit)
        {
            if constexpr (std::is_same_v<T, Response>)
            {
                if (options.structure.classes.enabled &&
                    options.strategy.outputArbitration == OutputArbitrationKind::OldestClassWeighted)
                    return SelectResponse(queue, downstream, commit);
            }
            size_t selected = queue.size();
            for (size_t index = 0; index < queue.size(); ++index)
            {
                if (queue[index].downstream != downstream || queue[index].readyTime > time)
                    continue;
                if (selected == queue.size() || queue[index].age < queue[selected].age ||
                    (queue[index].age == queue[selected].age && queue[index].serial < queue[selected].serial))
                    selected = index;
            }
            return selected;
        }

        template<typename T>
        const T* Peek(const std::deque<Pending<T>>& queue)
        {
            if (queue.empty())
                return nullptr;
            const size_t selected = Select(queue, false);
            return selected == queue.size() ? nullptr : &queue[selected].flit;
        }

        template<typename T>
        const T* Peek(const std::deque<Pending<T>>& queue, bool downstream)
        {
            const size_t selected = Select(queue, downstream, false);
            return selected == queue.size() ? nullptr : &queue[selected].flit;
        }

        template<typename T>
        std::optional<T> Pop(std::deque<Pending<T>>& queue, EventKind kind)
        {
            if (queue.empty())
                return std::nullopt;
            const size_t selected = Select(queue, true);
            if (selected == queue.size())
                return std::nullopt;
            Pending<T> pending = std::move(queue[selected]);
            queue.erase(queue.begin() + selected);
            std::optional<T> result(std::move(pending.flit));
            Publish(kind, *result, pending.transaction);
            OnEmitted(pending);
            events->DispatchDeferred();
            return result;
        }

        template<typename T>
        std::optional<T> Pop(std::deque<Pending<T>>& queue, EventKind kind, bool downstream)
        {
            const size_t selected = Select(queue, downstream, true);
            if (selected == queue.size())
                return std::nullopt;
            Pending<T> pending = std::move(queue[selected]);
            queue.erase(queue.begin() + selected);
            std::optional<T> result(std::move(pending.flit));
            Publish(kind, *result, pending.transaction);
            OnEmitted(pending);
            events->DispatchDeferred();
            return result;
        }

        template<typename T>
        bool Queue(std::deque<Pending<T>>& queue, const T& flit, TransactionHandle handle,
                   Time age, bool last = false, bool downstream = false,
                   size_t retry = std::numeric_limits<size_t>::max(),
                   SchedulePoint point = SchedulePoint::Automatic)
        {
            if (queue.size() >= options.outputQueueLimit)
            {
                pmu.RecordStall(WaitReason::OutputQueue);
                return false;
            }
            const SchedulePoint selected = point == SchedulePoint::Automatic
                ? OutputPoint(flit, downstream, retry) : point;
            queue.push_back({ flit, handle, age, nextSerial++, retry, last, downstream,
                selected, ScheduleOutput(handle, selected, time, retry) });
            return true;
        }

        template<typename T>
        bool AcquireResponse(Transaction& transaction, const std::deque<Pending<T>>& queue)
        {
            if (queue.size() >= options.outputQueueLimit)
            {
                pmu.RecordStall(WaitReason::OutputQueue);
                return false;
            }
            FlowState& flow = flows.at(transaction.handle);
            if (flow.responseOwned)
                return true;
            BankState& bank = Bank(transaction);
            if (options.structure.responseResourceLimit &&
                bank.responseUnits >= options.structure.responseResourceLimit)
            {
                pmu.RecordStall(WaitReason::OutputQueue);
                return false;
            }
            ++bank.responseUnits;
            flow.responseOwned = true;
            return true;
        }

        void Reset();
        void Tick();
        void ProcessResponses();
        void ProcessRSP(const Response& flit);
        void ProcessDAT(const Data& flit);
        void AdmitRequests();
        void AdmitRequest(const Request& request, size_t classId = std::numeric_limits<size_t>::max());
        bool IsRefillTransaction(const Transaction& transaction) const;
        void OrderTransactions(std::vector<TransactionHandle>& handles);
        void AdvanceTransactions();
        void Advance(Transaction& transaction);
        size_t BankIndex(const LineKey& line) const noexcept;
        BankState& Bank(const LineKey& line);
        const BankState& Bank(const LineKey& line) const;
        BankState& Bank(const Transaction& transaction);
        const BankState& Bank(const Transaction& transaction) const;
        size_t OrdinaryTransactionLimit() const noexcept;
        bool OriginalAdmissionAvailable(const LineKey& line, bool resend) const;
        void ReleaseOriginal(Transaction& transaction);
        void ReleaseRequestBuffer(Transaction& transaction);
        void ReleaseSnoopWave(Transaction& transaction);
        bool AcquireBackendOperation(Transaction& transaction);
        bool AcquireRefill(Transaction& transaction);
        void ReleaseResponse(Transaction& transaction);
        void ReleaseResponseIfComplete(Transaction& transaction);
        void ReleaseBackendOperation(Transaction& transaction);
        void CompleteBackendOperation(Transaction& transaction);
        void ReleaseRefill(Transaction& transaction);
        void ReleaseRefill(Transaction& transaction, const LineKey& line);
        size_t ClassForQos(uint8_t qos) const noexcept;
        size_t RetryTier(size_t bank, size_t classId) const noexcept;
        bool RetryCreditEligible(const RetryRecord& retry) const;
        bool ClassAdmissionAvailable(const Request& request, size_t classId, bool resend) const;
        bool RetryAdmissionAvailable(const Request& request, size_t classId) const;
        size_t RetrySourceCount() const noexcept;
        bool RetrySourceAvailable(NodeId source) const noexcept;
        void AcquireRetrySource(const Request& request);
        void ReleaseRetrySource(NodeId source, size_t bank, size_t classId);
        void ReleaseClass(Transaction& transaction);
        size_t BankRetryReservations(size_t bank) const noexcept;
        size_t TotalRetryReservations() const noexcept;
        Time ScheduleTag(Transaction& transaction, Time earliest);
        Time ScheduleData(Transaction& transaction, Time latency = 0);
        Time ScheduleResult(Transaction& transaction, SchedulePoint point, Time earliest);
        bool ScheduleTransition(Transaction& transaction, SchedulePoint point);
        Time ScheduleOutput(TransactionHandle handle, SchedulePoint point, Time earliest,
                            size_t retry = std::numeric_limits<size_t>::max());
        void Resolve(Transaction& transaction);
        void ResolveNonCoherent(Transaction& transaction);
        void ResolveCoherent(Transaction& transaction);
        void Action(Transaction& transaction);
        void ActionNonCoherent(Transaction& transaction);
        void ActionCoherent(Transaction& transaction);
        void ResolveImmediateWrite(Transaction& transaction);
        void ActionImmediateWrite(Transaction& transaction);
        void QueueImmediateResponse(Transaction& transaction);
        void ActionEvictOrEvict(Transaction& transaction);
        void ResolveStash(Transaction& transaction);
        void ActionStash(Transaction& transaction);
        void ActionPersistence(Transaction& transaction);
        void ResolveAtomic(Transaction& transaction);
        void ActionAtomic(Transaction& transaction);
        void ReturnAtomic(Transaction& transaction);
        void ReturnRead(Transaction& transaction);
        void QueueDBID(Transaction& transaction);
        void QueueComp(Transaction& transaction, bool withDBID = false, uint8_t resp = 0,
                       uint8_t respErr = 0);
        void QueueStashDone(Transaction& transaction, bool withComp = false);
        void QueueCompPersist(Transaction& transaction);
        void QueuePersist(Transaction& transaction);
        bool RequiresSeparatedPersist(const Transaction& transaction) const;
        void QueueReadReceipt(Transaction& transaction);
        void QueueRespSepData(Transaction& transaction);
        size_t ResponseCategory(const Response& response) const noexcept;
        size_t ResponseClass(const Pending<Response>& pending) const noexcept;
        size_t SelectResponse(const std::deque<Pending<Response>>& queue,
                              std::optional<bool> downstream, bool commit);
        void   QueueRetryCredits();
        bool   QueueRetryCredit(size_t index);
        size_t OutstandingRetryCredits(NodeId source, uint8_t pCrdType) const noexcept;
        size_t RetryOriginals(NodeId source, uint8_t pCrdType) const noexcept;
        void   PurgeReturnedRetries(NodeId source, uint8_t pCrdType);
        bool   RetryReservationAvailable() const noexcept;
        bool   ConsumeRetryRequest(const Request& request, bool creditReturn);
        void FinishResponse(Transaction& transaction);
        bool RequiresTransactionCompAck(const Transaction& transaction) const;
        void RetireFinished();
        void Retire(TransactionHandle handle);

        void StartMemoryRead(Transaction& transaction);
        void RetryBackendSubmission(Transaction& transaction);
        void AdvanceBackendRetries();
        bool HandleBackendRetry(Transaction& transaction, const Response& flit);
        void StartBackendWrite(Transaction& transaction);
        void AdvanceBackendWrite(Transaction& transaction);
        void ActionAtomicBackend(Transaction& transaction);
        bool PrepareAtomicBackend(Transaction& transaction);
        void StartAtomicBackend(Transaction& transaction);
        void AdvanceAtomicBackendDAT(Transaction& transaction);
        bool HandleAtomicBackendRSP(Transaction& transaction, const Response& flit);
        bool HandleAtomicBackendDAT(Transaction& transaction, const Data& flit);
        bool HandleBackendRSP(Transaction& transaction, const Response& flit);
        bool HandleBackendDAT(Transaction& transaction, const Data& flit);
        void HandleAXICompletion(const AXICompletion& completion);
        bool HandleWriteDAT(Transaction& transaction, const Data& flit);
        bool HandleAtomicDAT(Transaction& transaction, const Data& flit);
        void CompleteMemoryRead(Transaction& transaction);
        bool WriteLine(Transaction& transaction, const LineKey& line, const LineData& data,
                       const ByteEnables& enables);
        bool WriteLineImage(Transaction& transaction, const LineKey& line, const LineImage& image,
                            const ByteEnables& enables, bool generatedEviction = false);
        bool FetchBaseline(Transaction& transaction);

        uint16_t   SnoopOpcode(const Transaction& transaction, const FlowState& flow, NodeId target) const;
        void       PlanReadSnoops(Transaction& transaction);
        void       PlanCMOSnoops(Transaction& transaction);
        bool       EmitSnoops(Transaction& transaction);
        void       ProcessSnoopRSP(Transaction& transaction, const Response& flit);
        void       ProcessSnoopDAT(Transaction& transaction, const Data& flit);
        void       CompleteSnoopTarget(Transaction& transaction, NodeId target, uint8_t resp,
                                       bool withData, bool partial = false);
        CacheState DecodeSnoopState(Transaction& transaction, NodeId target, uint8_t resp,
                                    bool withData, bool partial = false) const;
        bool       ValidateSnoopResponse(Transaction& transaction, NodeId target, uint8_t resp,
                                         bool withData, bool partial = false, uint8_t dataPull = 0) const;
        bool       AcceptForwardedResponse(Transaction& transaction, NodeId target, uint8_t resp,
                                           uint8_t fwdState, bool withData);

        bool      PrepareHomeCommit(Transaction& transaction);
        bool      PrepareSnoopFilterCapacity(Transaction& transaction);
        size_t    ChooseVictim(TransactionHandle handle, VictimDomain domain, const LineKey& line);
        bool      PrepareVictim(Transaction& transaction);
        bool      CompleteVictim(Transaction& transaction);
        bool      CommitReadDirectory(Transaction& transaction);
        bool      CommitCopyback(Transaction& transaction);
        bool      CommitImmediateWrite(Transaction& transaction);
        bool      CommitAtomic(Transaction& transaction);
        bool      CommitCleanUnique(Transaction& transaction);
        bool      CommitCMO(Transaction& transaction);
        HomeLine& InstallHome(const LineKey& line, size_t way, const LineData& data, bool dirty,
                              Time age);
        void      EvictRequester(Transaction& transaction);

        void OnEmitted(const Pending<Request>& pending);
        void OnEmitted(const Pending<Response>& pending);
        void OnEmitted(const Pending<Data>& pending);
        void OnEmitted(const Pending<Snoop>& pending);

        bool Reentry();
        void Error(TransactionHandle handle, std::string reason);
        void Deny(const Request& request, std::string reason);
        bool HasInflight() const noexcept;
        bool ValidateInvariants() const noexcept;
    };

    inline std::optional<Response> Core::PeekTXRSP(bool downstream) const
    {
        if (impl->Reentry())
            return std::nullopt;
        const auto* value = impl->Peek(impl->txrsp, downstream);
        return value ? std::optional<Response>(*value) : std::nullopt;
    }

    inline std::optional<Data> Core::PeekTXDAT(bool downstream) const
    {
        if (impl->Reentry())
            return std::nullopt;
        const auto* value = impl->Peek(impl->txdat, downstream);
        return value ? std::optional<Data>(*value) : std::nullopt;
    }

    inline std::optional<Response> Core::PopTXRSP(bool downstream)
    {
        return impl->Reentry() ? std::nullopt : impl->Pop(impl->txrsp, EventKind::EmittedTXRSP, downstream);
    }

    inline std::optional<Data> Core::PopTXDAT(bool downstream)
    {
        return impl->Reentry() ? std::nullopt : impl->Pop(impl->txdat, EventKind::EmittedTXDAT, downstream);
    }

}
