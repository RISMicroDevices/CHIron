#pragma once

#include "chi_lunatic_arbiter.hpp"
#include "chi_lunatic_atomic.hpp"
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
#include "chi_lunatic_sam.hpp"
#include "chi_lunatic_snoop_filter.hpp"
#include "chi_lunatic_snoop_id.hpp"
#include "chi_lunatic_tracker.hpp"

#include <deque>
#include <limits>
#include <list>
#include <map>

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
        DataBufferId    backendResultDBID = 0;
        bool            returnSent = false;
        bool            returnEmitted = false;
        bool            backendResultReceived = false;
        uint8_t         operandRespErr = 0;
        uint8_t         backendResultRespErr = 0;
        bool            backendDirtyCommitted = false;
    };

    struct FlowState {
        Phase                      timingPhase = Phase::Retired;
        bool                       timingApplied = false;
        Phase                      eventPhase = Phase::Retired;
        bool                       eventApplied = false;
        BackendPhase               backendPhase = BackendPhase::None;
        Request                    backendRequest{};
        Time                       memoryReady = 0;
        bool                       memoryWrite = false;
        DataBufferId               backendDBID = 0;
        NodeId                     backendResponder = 0;
        NodeId                     backendDBIDSource = 0;
        bool                       backendResponderKnown = false;
        uint8_t                    backendWriteTraceTag = 0;
        uint64_t                   expectedBeatMask = 0;
        uint64_t                   cancelBeatMask = 0;
        uint64_t                   backendBeatMask = 0;
        size_t                     backendBeat = 0;
        size_t                     nextSnoop = 0;
        uint8_t                    writeResp = 0;
        std::map<NodeId, uint64_t> snoopBeatMasks;
        std::map<NodeId, uint8_t>  snoopResponses;
        std::map<NodeId, bool>     snoopRetToSrc;
        std::map<NodeId, bool>     snoopDoNotGoToSD;
        std::map<NodeId, TransactionId>
                                   snoopIds;
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
        TransactionId              dctSnoopId = 0;
        bool                       dctSnoopIdValid = false;
        uint8_t                    dctFwdState = 0;
        bool                       retToSrcUsed = false;
        bool                       dwtActive = false;
        bool                       directPersistActive = false;
        bool                       respSepSent = false;
        bool                       respSepEmitted = false;
        size_t                     readBeatsEmitted = 0;
        std::array<uint8_t, 4>     backendReadRespErr{};
        uint64_t                   backendReadRespErrMask = 0;
        std::array<uint8_t, 4>     snoopDataRespErr{};
        uint64_t                   snoopDataRespErrMask = 0;
        std::array<uint8_t, 4>     victimDataRespErr{};
        uint64_t                   victimDataRespErrMask = 0;
        uint8_t                    backendReadResp = 0;
        bool                       backendReadRespValid = false;
        bool                       backendReadReceiptReceived = false;
        bool                       backendRetryAckReceived = false;
        bool                       suppressArchitecturalEffects = false;
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
        std::array<uint8_t, 4>     writeRespErr{};
        uint64_t                   writeRespErrMask = 0;
        bool                       victimSnoopFilterOnly = false;
        TransactionId              allocatedDBID = 0;
        bool                       allocatedDBIDValid = false;
        bool                       dbidSent = false;
        bool                       compSent = false;
        bool                       persistSent = false;
        uint8_t                    responseForm = 0;
        bool                       compEmitted = false;
        bool                       tagMatchSent = false;
        bool                       persistEmitted = false;
        bool                       dbidEmitted = false;
        bool                       stashDoneSent = false;
        bool                       stashDoneEmitted = false;
        bool                       evictOrEvictNoData = false;
        bool                       mergeImmediateHome = false;
        bool                       backendCompReceived = false;
        bool                       backendPersistReceived = false;
        uint8_t                    backendCompRespErr = 0;
        uint8_t                    backendPersistRespErr = 0;
        bool                       separatedPersistResponse = false;
        bool                       backendSeparatedCompReceived = false;
        bool                       backendSeparatedPersistReceived = false;
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

    bool RequiresBackendReadReceipt(const FlowState& flow) noexcept;

    struct RetryRecord {
        Request request;
        bool    ackPopped = false;
        bool    grantQueued = false;
    };

    struct BackendRetryRecord {
        Request           request;
        TransactionHandle transaction = INVALID_TRANSACTION;
        uint64_t          serial = 0;
        uint8_t           retryAckPCrdType = 0;
        NodeId            retryAckSource = 0;
        NodeId            creditSource = 0;
        bool              creditReceived = false;
    };

    Request MakeBackendCreditReturnRequest(const BackendRetryRecord& retry, NodeId home) noexcept;

    template<typename T>
    struct Pending {
        T                 flit;
        TransactionHandle transaction = INVALID_TRANSACTION;
        Time              age = 0;
        uint64_t          serial = 0;
        size_t            retry = std::numeric_limits<size_t>::max();
        bool              last = false;
        bool              downstream = false;
    };

    struct Core::Impl {
        NodeId                    nodeId;
        Options                   options;
        std::shared_ptr<EventHub> events;
        Time                      time = 0;
        PerformanceCounters       pmu;
        Directory                 directory;
        SnoopFilter               snoopFilter;
        std::unique_ptr<CachePolicy>
                                  policy;
        Tracker                   tracker;
        SnoopIdAllocator          snoopIdAllocator;
        HazardGraph               hazards;
        OrderingTracker           ordering;
        VirtualPipeline           pipeline;
        DeterministicRandom       random;
        SparseMemory              memory;
        AXIBackend                axi;
        std::map<TransactionHandle, FlowState>
                                  flows;
        std::map<size_t, RetryRecord>
                                  retries;
        std::map<std::pair<NodeId, uint8_t>, size_t>
                                  availableCredits;
        std::map<std::pair<NodeId, uint8_t>, size_t>
                                  cancellationCredits;
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

        std::deque<Request>           rxreq;
        std::deque<Response>          rxrsp;
        std::deque<Data>              rxdat;
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

        template<typename T>
        static size_t Select(const std::deque<Pending<T>>& queue)
        {
            size_t selected = 0;
            for (size_t index = 1; index < queue.size(); ++index)
            {
                if (queue[index].age < queue[selected].age ||
                    (queue[index].age == queue[selected].age && queue[index].serial < queue[selected].serial))
                    selected = index;
            }
            return selected;
        }

        template<typename T>
        static size_t Select(const std::deque<Pending<T>>& queue, bool downstream)
        {
            size_t selected = queue.size();
            for (size_t index = 0; index < queue.size(); ++index)
            {
                if (queue[index].downstream != downstream)
                    continue;
                if (selected == queue.size() || queue[index].age < queue[selected].age ||
                    (queue[index].age == queue[selected].age && queue[index].serial < queue[selected].serial))
                    selected = index;
            }
            return selected;
        }

        template<typename T>
        const T* Peek(const std::deque<Pending<T>>& queue) const
        {
            return queue.empty() ? nullptr : &queue[Select(queue)].flit;
        }

        template<typename T>
        const T* Peek(const std::deque<Pending<T>>& queue, bool downstream) const
        {
            const size_t selected = Select(queue, downstream);
            return selected == queue.size() ? nullptr : &queue[selected].flit;
        }

        template<typename T>
        std::optional<T> Pop(std::deque<Pending<T>>& queue, EventKind kind)
        {
            if (queue.empty())
                return std::nullopt;
            const size_t selected = Select(queue);
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
            const size_t selected = Select(queue, downstream);
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
                   size_t retry = std::numeric_limits<size_t>::max())
        {
            if (queue.size() >= options.outputQueueLimit)
            {
                pmu.RecordStall(WaitReason::OutputQueue);
                return false;
            }
            queue.push_back({ flit, handle, age, nextSerial++, retry, last, downstream });
            return true;
        }

        void Reset();
        void Tick();
        void ProcessResponses();
        void ProcessRSP(const Response& flit);
        void ProcessDAT(const Data& flit);
        void AdmitRequests();
        void AdmitRequest(const Request& request);
        void AdvanceTransactions();
        void Advance(Transaction& transaction);
        void CompleteErrorTransaction(Transaction& transaction);
        Time ScheduleTag();
        Time ScheduleData();
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
        void QueueTagMatch(Transaction& transaction);
        void QueueReadReceipt(Transaction& transaction);
        void QueueRespSepData(Transaction& transaction);
        void   QueueRetryCredit(size_t index);
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
        Transaction* FindBackendTransaction(NodeId source, NodeId target, TransactionId txnId) noexcept;
        void RecordBackendResponder(Transaction& transaction, NodeId source) noexcept;
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

        bool PrepareDCTId(Transaction& transaction);
        std::vector<NodeId> SnoopTargets(const LineKey& line) const;
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
