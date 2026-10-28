#pragma once

#include "chi_lunatic_arbiter.hpp"
#include "chi_lunatic_component.hpp"
#include "chi_lunatic_directory.hpp"
#include "chi_lunatic_hazard.hpp"
#include "chi_lunatic_memory.hpp"
#include "chi_lunatic_pipeline.hpp"
#include "chi_lunatic_policy.hpp"
#include "chi_lunatic_protocol.hpp"
#include "chi_lunatic_snoop_filter.hpp"
#include "chi_lunatic_tracker.hpp"

#include <deque>
#include <limits>
#include <map>

namespace CHI::Lunatic {

enum class BackendPhase : uint8_t {
    None,
    WaitRequestEmission,
    WaitDBID,
    SendData,
    WaitDataEmission,
    WaitComp,
    Done
};

struct FlowState {
    BackendPhase backendPhase = BackendPhase::None;
    Time memoryReady = 0;
    bool memoryWrite = false;
    DatabaseId backendDBID = 0;
    uint64_t expectedBeatMask = 0;
    uint64_t backendBeatMask = 0;
    size_t backendBeat = 0;
    size_t nextSnoop = 0;
    uint8_t writeResp = 0;
    std::map<NodeId, uint64_t> snoopBeatMasks;
    std::map<NodeId, uint8_t> snoopResponses;
    bool readReceiptSent = false;
    bool readReceiptEmitted = false;
    bool directoryCommitted = false;
    bool baselineFetch = false;
    bool baselineFetched = false;
    bool snoopDataPartial = false;
    ByteEnables snoopValidBytes{};
    ByteEnables validBytes{};
    bool spillPending = false;
    LineKey spillLine{};
    LineData spillData{};
    ByteEnables spillEnables{};
    bool victimSnoopFilterOnly = false;
    bool dbidSent = false;
    bool compSent = false;
    bool backendCompReceived = false;
    bool backendDataEmitted = false;
    bool snoopDataValid = false;
    bool dataDirty = false;
    bool passDirty = false;
    bool requesterWasValid = false;
    bool requesterWasDirty = false;
    CacheState requesterFinal = CacheState::I;
    LineKey snoopLine{};
    bool snoopForVictim = false;
    bool victimPending = false;
    bool victimLinked = false;
    bool victimDataValid = false;
    bool victimDirty = false;
    size_t victimWay = 0;
    LineKey victimLine{};
    LineData victimData{};
    bool allocateHome = false;
    bool removeHome = false;
    bool retainHome = false;
    bool homeDirty = false;
};

struct RetryRecord {
    Request request;
    bool ackPopped = false;
    bool grantQueued = false;
    bool grantPopped = false;
};

template<typename T>
struct Pending {
    T flit;
    TransactionHandle transaction = INVALID_TRANSACTION;
    Time age = 0;
    uint64_t serial = 0;
    size_t retry = std::numeric_limits<size_t>::max();
    bool last = false;
    bool downstream = false;
};

struct Core::Impl {
    NodeId nodeId;
    Options options;
    std::shared_ptr<EventHub> events;
    Time time = 0;
    Directory directory;
    SnoopFilter snoopFilter;
    std::unique_ptr<CachePolicy> policy;
    Tracker tracker;
    HazardGraph hazards;
    VirtualPipeline pipeline;
    SparseMemory memory;
    std::map<TransactionHandle, FlowState> flows;
    std::map<size_t, RetryRecord> retries;
    size_t nextRetry = 0;
    uint64_t nextAge = 0;
    uint64_t nextSerial = 0;

    std::deque<Request> rxreq;
    std::deque<Response> rxrsp;
    std::deque<Data> rxdat;
    std::deque<Pending<Request>> txreq;
    std::deque<Pending<Response>> txrsp;
    std::deque<Pending<Data>> txdat;
    std::deque<Pending<Snoop>> txsnp;
    std::vector<ErrorInfo> errors;

    Impl(NodeId nodeId, Options options, std::shared_ptr<EventHub> events);

    template<typename T>
    bool Push(std::deque<T>& queue, const T& flit, EventKind kind)
    {
        if (queue.size() >= options.inputQueueLimit)
            return false;
        queue.push_back(flit);
        Publish(kind, flit);
        return true;
    }

    template<typename T>
    void Publish(EventKind kind, const T& flit, TransactionHandle handle = INVALID_TRANSACTION)
    {
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
        events->Publish(std::move(event));
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
    const T* Peek(const std::deque<Pending<T>>& queue) const
    {
        return queue.empty() ? nullptr : &queue[Select(queue)].flit;
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
        return result;
    }

    template<typename T>
    bool Queue(std::deque<Pending<T>>& queue, const T& flit, TransactionHandle handle,
               Time age, bool last = false, bool downstream = false, size_t retry = std::numeric_limits<size_t>::max())
    {
        if (queue.size() >= options.outputQueueLimit)
            return false;
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
    void Resolve(Transaction& transaction);
    void ResolveNonCoherent(Transaction& transaction);
    void ResolveCoherent(Transaction& transaction);
    void Action(Transaction& transaction);
    void ActionNonCoherent(Transaction& transaction);
    void ActionCoherent(Transaction& transaction);
    void ReturnRead(Transaction& transaction);
    void QueueDBID(Transaction& transaction);
    void QueueComp(Transaction& transaction, bool withDBID = false, uint8_t resp = 0, uint8_t respErr = 0);
    void QueueReadReceipt(Transaction& transaction);
    void QueueRetryCredit(size_t index);
    void FinishResponse(Transaction& transaction);
    void RetireFinished();
    void Retire(TransactionHandle handle);

    void StartMemoryRead(Transaction& transaction);
    void StartBackendWrite(Transaction& transaction);
    void AdvanceBackendWrite(Transaction& transaction);
    bool HandleBackendRSP(Transaction& transaction, const Response& flit);
    bool HandleBackendDAT(Transaction& transaction, const Data& flit);
    bool HandleWriteDAT(Transaction& transaction, const Data& flit);
    void CompleteMemoryRead(Transaction& transaction);
    bool WriteLine(Transaction& transaction, const LineKey& line, const LineData& data, const ByteEnables& enables);
    bool FetchBaseline(Transaction& transaction);

    void PlanReadSnoops(Transaction& transaction);
    void PlanCMOSnoops(Transaction& transaction);
    bool EmitSnoops(Transaction& transaction);
    void ProcessSnoopRSP(Transaction& transaction, const Response& flit);
    void ProcessSnoopDAT(Transaction& transaction, const Data& flit);
    void CompleteSnoopTarget(Transaction& transaction, NodeId target, uint8_t resp, bool withData, bool partial = false);
    CacheState DecodeSnoopState(Transaction& transaction, NodeId target, uint8_t resp, bool partial = false) const;
    bool ValidateSnoopResponse(Transaction& transaction, NodeId target, uint8_t resp, bool withData, bool partial = false) const;

    bool PrepareHomeCommit(Transaction& transaction);
    bool PrepareSnoopFilterCapacity(Transaction& transaction);
    bool PrepareVictim(Transaction& transaction);
    bool CompleteVictim(Transaction& transaction);
    bool CommitReadDirectory(Transaction& transaction);
    bool CommitCopyback(Transaction& transaction);
    bool CommitCMO(Transaction& transaction);
    HomeLine& InstallHome(const LineKey& line, size_t way, const LineData& data, bool dirty, Time age);
    void EvictRequester(Transaction& transaction);

    void OnEmitted(const Pending<Request>& pending);
    void OnEmitted(const Pending<Response>& pending);
    void OnEmitted(const Pending<Data>& pending);
    void OnEmitted(const Pending<Snoop>& pending);

    void Error(TransactionHandle handle, std::string reason);
    void Deny(const Request& request, std::string reason);
    bool HasInflight() const noexcept;
    bool ValidateInvariants() const noexcept;
};

}
