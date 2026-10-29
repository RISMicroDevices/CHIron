#include "chi_lunatic_core.hpp"

#include <algorithm>
#include <bit>
#include <utility>

namespace CHI::Lunatic {

    Core::Impl::Impl(NodeId nodeId, Options options, std::shared_ptr<EventHub> events)
        : nodeId(nodeId)
        , options(std::move(options))
        , events(std::move(events))
        , directory(this->options)
        , snoopFilter(this->options)
        , policy(MakePolicy(this->options.policy))
        , tracker(this->options.transactionLimit)
        , pipeline(this->options)
    {
    }

    void                                Core::Impl::Reset()
    {
        rxreq.clear();
        rxrsp.clear();
        rxdat.clear();
        txreq.clear();
        txrsp.clear();
        txdat.clear();
        txsnp.clear();
        retries.clear();
        flows.clear();
        errors.clear();
        tracker.Clear();
        hazards.Clear();
        directory.Clear();
        snoopFilter.Clear();
        memory.Clear();
        pipeline.Reset();
        time = 0;
        nextAge = 0;
        nextSerial = 0;
        nextRetry = 0;
        events->Clear();

        Event event;
        event.kind = EventKind::Reset;
        event.home = nodeId;
        events->Publish(std::move(event));
    }

    void                                Core::Impl::Tick()
    {
        ProcessResponses();
        AdmitRequests();
        AdvanceTransactions();

        for (const auto& [index, retry] : retries)
            QueueRetryCredit(index);

        RetireFinished();
        events->Dispatch();
    }

    void                                Core::Impl::ProcessResponses()
    {
        while (!rxrsp.empty())
        {
            Response flit = std::move(rxrsp.front());
            rxrsp.pop_front();
            ProcessRSP(flit);
        }
        while (!rxdat.empty())
        {
            Data flit = std::move(rxdat.front());
            rxdat.pop_front();
            ProcessDAT(flit);
        }
    }

    void                                Core::Impl::ProcessRSP(const Response& flit)
    {
        namespace RSP = CHI::Eb::Opcodes::RSP;

        if (flit.opcode == RSP::CompAck)
        {
            Transaction* transaction = tracker.FindDBID(flit.source, flit.txnId);
            if (!transaction)
            {
                Error(INVALID_TRANSACTION, "unmatched CompAck");
                return;
            }
            if (!RequiresCompAck(transaction->request) || transaction->Complete(COMPLETE_COMPACK) ||
                transaction->phase != Phase::WaitCompAck || !transaction->Complete(COMPLETE_RESPONSE))
            {
                Error(transaction->handle, "unexpected or early CompAck");
                return;
            }
            transaction->Mark(COMPLETE_COMPACK);
            transaction->phase = Phase::Commit;
            transaction->readyTime = time;
            transaction->wait = WaitReason::None;
            return;
        }

        if (flit.opcode == RSP::SnpResp)
        {
            Transaction* transaction = tracker.FindSnoop(flit.source, flit.txnId);
            if (!transaction)
            {
                Error(INVALID_TRANSACTION, "unmatched SnpResp");
                return;
            }
            ProcessSnoopRSP(*transaction, flit);
            return;
        }

        if (Transaction* transaction = tracker.FindMemory(flit.source, flit.txnId))
        {
            if (!HandleBackendRSP(*transaction, flit))
                Error(transaction->handle, "unexpected downstream response");
            return;
        }

        Error(INVALID_TRANSACTION, "unmatched response");
    }

    void                                Core::Impl::ProcessDAT(const Data& flit)
    {
        namespace DAT = CHI::Eb::Opcodes::DAT;

        if (flit.opcode == DAT::CompData)
        {
            if (Transaction* transaction = tracker.FindMemory(flit.source, flit.txnId))
            {
                if (!HandleBackendDAT(*transaction, flit))
                    Error(transaction->handle, "unexpected downstream data");
                return;
            }
        }

        if (flit.opcode == DAT::NonCopyBackWrData || flit.opcode == DAT::NCBWrDataCompAck ||
            flit.opcode == DAT::CopyBackWrData || flit.opcode == DAT::WriteDataCancel)
        {
            Transaction* transaction = tracker.FindDBID(flit.source, flit.txnId);
            if (!transaction)
            {
                Error(INVALID_TRANSACTION, "unmatched write data");
                return;
            }
            if (!HandleWriteDAT(*transaction, flit))
                Error(transaction->handle, "invalid write data beat");
            return;
        }

        if (flit.opcode == DAT::SnpRespData || flit.opcode == DAT::SnpRespDataPtl)
        {
            Transaction* transaction = tracker.FindSnoop(flit.source, flit.txnId);
            if (!transaction)
            {
                Error(INVALID_TRANSACTION, "unmatched snoop data");
                return;
            }
            ProcessSnoopDAT(*transaction, flit);
            return;
        }

        Error(INVALID_TRANSACTION, "unsupported or unmatched data response");
    }

    void                                Core::Impl::AdmitRequests()
    {
        if (rxreq.empty())
            return;

        if (tracker.Size() >= tracker.Limit())
        {
            Request& request = rxreq.front();
            if (!options.retryEnabled || !request.allowRetry || txrsp.size() >= options.outputQueueLimit)
                return;

            const size_t index = nextRetry++;
            RetryRecord record{ request, false, false, false };
            Response response;
            response.source = nodeId;
            response.target = request.source;
            response.txnId = request.txnId;
            response.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
            response.pCrdType = request.pCrdType;
            response.qos = request.qos;
            response.traceTag = request.traceTag;
            retries.emplace(index, record);
            rxreq.pop_front();
            Queue(txrsp, response, INVALID_TRANSACTION, nextAge++, false, false, index);
            return;
        }

        Request request = std::move(rxreq.front());
        rxreq.pop_front();
        AdmitRequest(request);
    }

    void                                Core::Impl::AdmitRequest(const Request& request)
    {
        const Time age = nextAge++;
        Transaction* transaction = tracker.Allocate(request, age);
        if (!transaction)
        {
            Deny(request, "duplicate live requester transaction ID");
            return;
        }

        flows.emplace(transaction->handle, FlowState{});
        transaction->readyTime = pipeline.SaturatingAdd(time, 1);
        transaction->phase = Phase::Receive;
    }

    void                                Core::Impl::AdvanceTransactions()
    {
        std::vector<TransactionHandle> handles = tracker.Handles();
        std::sort(handles.begin(), handles.end(), [&](TransactionHandle a, TransactionHandle b) {
            const Transaction* ta = tracker.Find(a);
            const Transaction* tb = tracker.Find(b);
            if (ta->actionAge != tb->actionAge)
                return ta->actionAge < tb->actionAge;
            return a < b;
        });

        for (TransactionHandle handle : handles)
        {
            Transaction* transaction = tracker.Find(handle);
            if (transaction && pipeline.Eligible(*transaction, time))
                Advance(*transaction);
        }
    }

    void                                Core::Impl::Advance(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        switch (transaction.phase)
        {
            case Phase::Receive:
                pipeline.Advance(transaction, Phase::Decode, time);
                return;
            case Phase::Decode:
                if (!IsSupportedRequest(transaction.request.opcode) || transaction.request.size > 6 ||
                    (ClassifyRequest(transaction.request.opcode) == RequestClass::ZeroWrite && transaction.request.size != 6))
                {
                    Deny(transaction.request, "unsupported request opcode or transfer geometry");
                    transaction.canceled = true;
                    transaction.dbid = transaction.handle;
                    QueueComp(transaction, false, CHI::Eb::Resps::Comp_I, 0b10);
                    return;
                }
                pipeline.Advance(transaction, Phase::Admit, time);
                return;
            case Phase::Admit:
                if (!tracker.BindDBID(transaction.handle, transaction.handle))
                {
                    Error(transaction.handle, "unable to bind database ID");
                    transaction.canceled = true;
                    transaction.phase = Phase::Commit;
                    transaction.readyTime = time;
                    return;
                }
                transaction.dbid = transaction.handle;
                hazards.Add(transaction.handle, transaction.request.Line(), transaction.actionAge);
                transaction.phase = Phase::Hazard;
                transaction.readyTime = time;
                return;
            case Phase::Hazard:
                if (!hazards.IsHead(transaction.handle, transaction.request.Line()) ||
                    (flow.victimLinked && !hazards.IsHead(transaction.handle, flow.victimLine)))
                {
                    transaction.wait = WaitReason::SameLine;
                    transaction.readyTime = time;
                    return;
                }
                transaction.wait = WaitReason::None;
                transaction.phase = Phase::Lookup;
                transaction.readyTime = pipeline.ScheduleTag(time);
                return;
            case Phase::Lookup:
            {
                const HomeLine* home = directory.Find(transaction.request.Line());
                transaction.homeHit = home && home->valid;
                flow.validBytes.fill(1);
                if (transaction.homeHit)
                {
                    transaction.data = home->data;
                    flow.homeDirty = home->dirty;
                    flow.validBytes = home->validBytes;
                }
                else if (IsReadRequest(transaction.request.opcode) || IsCMORequest(transaction.request.opcode))
                    transaction.data = memory.Read(transaction.request.Line());
                const CacheState requesterState = snoopFilter.State(transaction.request.Line(), transaction.request.source);
                flow.requesterWasValid = IsValid(requesterState);
                flow.requesterWasDirty = IsDirty(requesterState);
                pipeline.Advance(transaction, Phase::Resolve, time);
                return;
            }
            case Phase::Resolve:
                Resolve(transaction);
                return;
            case Phase::Action:
                Action(transaction);
                return;
            case Phase::WaitMemory:
                if (options.backend == BackendKind::Internal && transaction.memoryRequired && flow.memoryReady > time)
                    return;
                if (options.backend == BackendKind::Internal && transaction.memoryRequired)
                    CompleteMemoryRead(transaction);
                else if (flow.backendPhase == BackendPhase::Done)
                    pipeline.Advance(transaction, Phase::Action, time);
                return;
            case Phase::WaitWriteData:
            case Phase::WaitCompAck:
                return;
            case Phase::WaitSnoop:
                if (transaction.pendingSnoops.empty())
                    pipeline.Advance(transaction, Phase::Action, time);
                return;
            case Phase::Return:
                if (IsReadRequest(transaction.request.opcode))
                    ReturnRead(transaction);
                return;
            case Phase::Commit:
            case Phase::Retired:
                return;
        }
    }

    void                                Core::Impl::Resolve(Transaction& transaction)
    {
        if (IsCoherentRequest(transaction.request.opcode) || ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce)
            ResolveCoherent(transaction);
        else
            ResolveNonCoherent(transaction);
    }

    void                                Core::Impl::Action(Transaction& transaction)
    {
        if (IsCoherentRequest(transaction.request.opcode) || ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce)
            ActionCoherent(transaction);
        else
            ActionNonCoherent(transaction);
    }

    void                                Core::Impl::QueueDBID(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.dbidSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.dbid = transaction.dbid;
        response.opcode = CHI::Eb::Opcodes::RSP::DBIDResp;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
        {
            flow.dbidSent = true;
            transaction.phase = Phase::WaitWriteData;
            transaction.wait = WaitReason::WriteData;
            transaction.readyTime = time;
        }
    }

    void                                Core::Impl::QueueComp(Transaction& transaction, bool withDBID, uint8_t resp, uint8_t respErr)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.compSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.dbid = transaction.dbid;
        response.opcode = withDBID ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::Comp;
        response.resp = resp;
        response.respErr = respErr;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
        {
            flow.compSent = true;
            transaction.phase = Phase::Return;
            transaction.readyTime = time;
        }
    }

    void                                Core::Impl::QueueReadReceipt(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.readReceiptSent || transaction.request.order == 0)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.dbid = transaction.dbid;
        response.opcode = CHI::Eb::Opcodes::RSP::ReadReceipt;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
            flow.readReceiptSent = true;
    }

    void                                Core::Impl::QueueRetryCredit(size_t index)
    {
        auto it = retries.find(index);
        if (it == retries.end() || !it->second.ackPopped || it->second.grantQueued || tracker.Size() >= tracker.Limit())
            return;

        Response response;
        response.source = nodeId;
        response.target = it->second.request.source;
        response.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
        response.pCrdType = it->second.request.pCrdType;
        if (Queue(txrsp, response, INVALID_TRANSACTION, nextAge++, false, false, index))
            it->second.grantQueued = true;
    }

    void                                Core::Impl::ReturnRead(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (transaction.request.order != 0 && !flow.readReceiptEmitted)
        {
            if (!flow.readReceiptSent)
                QueueReadReceipt(transaction);
            return;
        }

        uint64_t mask = flow.expectedBeatMask;
        if (!mask)
            mask = (uint64_t(1) << BeatsPerLine(options)) - 1;
        if (transaction.sentBeats >= size_t(std::popcount(mask)))
            return;

        uint64_t selected = mask;
        for (size_t index = 0; index < transaction.sentBeats; ++index)
            selected &= selected - 1;
        const size_t beat = std::countr_zero(selected);
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = beat * bytes;
        const uint64_t remaining = selected & ~(uint64_t(1) << beat);

        Data data;
        data.source = nodeId;
        data.target = transaction.request.source;
        data.homeNid = nodeId;
        data.txnId = transaction.request.txnId;
        data.dbid = transaction.dbid;
        data.opcode = CHI::Eb::Opcodes::DAT::CompData;
        data.resp = transaction.responseResp;
        data.respErr = transaction.responseRespErr;
        data.dataId = offset / 16;
        data.ccid = (transaction.request.address >> 4) & 3;
        data.qos = transaction.request.qos;
        data.traceTag = transaction.request.traceTag;
        for (size_t index = 0; index < bytes; ++index)
        {
            data.bytes[index] = transaction.data[offset + index];
            data.byteEnables[index] = flow.validBytes[offset + index];
            data.dataCheck[index] = transaction.dataCheck[offset + index];
        }
        for (size_t lane = 0; lane < bytes / 8; ++lane)
            data.poison[lane] = transaction.poison[offset / 8 + lane];

        if (Queue(txdat, data, transaction.handle, transaction.actionAge, remaining == 0))
            ++transaction.sentBeats;
    }

    void                                Core::Impl::FinishResponse(Transaction& transaction)
    {
        transaction.Mark(COMPLETE_RESPONSE);
        transaction.wait = RequiresCompAck(transaction.request) && !transaction.Complete(COMPLETE_COMPACK)
            ? WaitReason::CompAck : WaitReason::None;
        transaction.phase = transaction.wait == WaitReason::CompAck ? Phase::WaitCompAck : Phase::Commit;
        transaction.readyTime = time;
    }

    void                                Core::Impl::RetireFinished()
    {
        std::vector<TransactionHandle> handles;
        for (TransactionHandle handle : tracker.Handles())
        {
            const Transaction* transaction = tracker.Find(handle);
            if (transaction->phase == Phase::Commit && transaction->readyTime <= time)
                handles.push_back(handle);
        }
        for (TransactionHandle handle : handles)
            Retire(handle);
    }

    void                                Core::Impl::Retire(TransactionHandle handle)
    {
        Transaction* transaction = tracker.Find(handle);
        if (!transaction)
            return;

        FlowState& flow = flows.at(handle);
        hazards.Release(handle, transaction->request.Line());
        if (flow.victimLinked)
            hazards.Release(handle, flow.victimLine);

        Event event;
        event.kind = EventKind::Retired;
        event.time = time;
        event.home = nodeId;
        event.source = transaction->request.source;
        event.target = nodeId;
        event.txnId = transaction->request.txnId;
        event.opcode = transaction->request.opcode;
        event.transaction = handle;
        event.line = transaction->request.Line();
        events->Publish(std::move(event));

        tracker.Retire(handle);
        flows.erase(handle);
    }

    void                                Core::Impl::OnEmitted(const Pending<Request>& pending)
    {
        Transaction* transaction = tracker.Find(pending.transaction);
        if (!transaction)
            return;
        FlowState& flow = flows.at(transaction->handle);
        if (flow.backendPhase == BackendPhase::WaitRequestEmission)
        {
            flow.backendPhase = !flow.spillPending && (IsReadRequest(transaction->request.opcode) || IsCMORequest(transaction->request.opcode))
                ? BackendPhase::WaitComp : BackendPhase::WaitDBID;
            transaction->phase = Phase::WaitMemory;
            transaction->readyTime = time;
        }
    }

    void                                Core::Impl::OnEmitted(const Pending<Response>& pending)
    {
        if (pending.retry != std::numeric_limits<size_t>::max())
        {
            auto it = retries.find(pending.retry);
            if (it == retries.end())
                return;
            if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::RetryAck)
                it->second.ackPopped = true;
            if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant)
                it->second.grantPopped = true;
            if (it->second.ackPopped && it->second.grantPopped)
                retries.erase(it);
            return;
        }

        Transaction* transaction = tracker.Find(pending.transaction);
        if (!transaction)
            return;
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::ReadReceipt)
        {
            flows.at(transaction->handle).readReceiptEmitted = true;
            return;
        }
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompDBIDResp && IsCopybackRequest(transaction->request.opcode))
        {
            transaction->Mark(COMPLETE_RESPONSE);
            transaction->phase = Phase::WaitWriteData;
            transaction->wait = WaitReason::WriteData;
            transaction->readyTime = time;
            return;
        }
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::Comp ||
            pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompDBIDResp)
            FinishResponse(*transaction);
    }

    void                                Core::Impl::OnEmitted(const Pending<Data>& pending)
    {
        Transaction* transaction = tracker.Find(pending.transaction);
        if (!transaction)
            return;
        FlowState& flow = flows.at(transaction->handle);
        if (pending.downstream)
        {
            if (pending.last)
            {
                flow.backendDataEmitted = true;
                if (flow.backendCompReceived)
                {
                    flow.backendPhase = BackendPhase::Done;
                    tracker.UnbindMemory(options.subordinateId, transaction->handle);
                    transaction->phase = Phase::Action;
                    transaction->readyTime = time;
                }
                else
                {
                    flow.backendPhase = BackendPhase::WaitComp;
                    transaction->phase = Phase::WaitMemory;
                }
            }
            return;
        }
        if (pending.last)
            FinishResponse(*transaction);
    }

    void                                Core::Impl::OnEmitted(const Pending<Snoop>&)
    {
    }

    void                                Core::Impl::Error(TransactionHandle handle, std::string reason)
    {
        errors.push_back({ time, handle, reason });
        Event event;
        event.kind = EventKind::Error;
        event.time = time;
        event.home = nodeId;
        event.transaction = handle;
        event.message = std::move(reason);
        events->Publish(std::move(event));
    }

    void                                Core::Impl::Deny(const Request& request, std::string reason)
    {
        Error(INVALID_TRANSACTION, reason);
        Event event;
        event.kind = EventKind::Denied;
        event.time = time;
        event.home = nodeId;
        event.source = request.source;
        event.target = request.target;
        event.txnId = request.txnId;
        event.opcode = request.opcode;
        event.line = request.Line();
        event.message = std::move(reason);
        events->Publish(std::move(event));
    }

    bool                                Core::Impl::HasInflight() const noexcept
    {
        return !rxreq.empty() || !rxrsp.empty() || !rxdat.empty() || !txreq.empty() || !txrsp.empty() ||
               !txdat.empty() || !txsnp.empty() || !retries.empty() || !tracker.Empty() || !hazards.Empty();
    }

    bool                                Core::Impl::ValidateInvariants() const noexcept
    {
        if (!tracker.Validate() || !hazards.Validate())
            return false;
        if (!tracker.Empty())
            return true;

        for (size_t set = 0; set < snoopFilter.Sets(); ++set)
        {
            for (size_t way = 0; way < snoopFilter.Ways(); ++way)
            {
                const SnoopFilterEntry& entry = snoopFilter.At(set, way);
                if (!entry.valid)
                    continue;
                size_t validRequesters = 0;
                size_t dirtyRequesters = 0;
                for (const auto& [requester, state] : entry.requesters)
                {
                    validRequesters += IsValid(state) ? 1 : 0;
                    dirtyRequesters += IsDirty(state) ? 1 : 0;
                }

                const HomeLine* home = directory.Find(entry.key);
                if (options.policy == PolicyKind::Inclusive && validRequesters && (!home || !home->valid))
                    return false;
                if (options.policy == PolicyKind::Exclusive && validRequesters && home && home->valid)
                    return false;
                if (home && home->dirty && dirtyRequesters)
                    return false;
                if (dirtyRequesters > 1)
                    return false;
            }
        }
        return true;
    }

}
