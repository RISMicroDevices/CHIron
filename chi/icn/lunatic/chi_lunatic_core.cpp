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
        , random(this->options.seed)
        , axi(this->options)
    {
    }

    void Core::Impl::Reset()
    {
        rxreq.clear();
        rxrsp.clear();
        rxdat.clear();
        txreq.clear();
        txrsp.clear();
        txdat.clear();
        txsnp.clear();
        retries.clear();
        backendRetries.clear();
        backendCredits.clear();
        completedBackendReads.clear();
        completedBackendReadOrder.clear();
        flows.clear();
        errors.clear();
        tracker.Clear();
        hazards.Clear();
        ordering.Clear();
        directory.Clear();
        snoopFilter.Clear();
        memory.Clear();
        axi.Reset();
        pipeline.Reset();
        random.Reset(options.seed);
        pmu.Reset();
        time = 0;
        nextAge = 0;
        nextSerial = 0;
        nextBackendRetry = 0;
        nextRetry = 0;

        Event event;
        event.kind = EventKind::Reset;
        event.home = nodeId;
        events->Publish(std::move(event));
    }

    void Core::Impl::Tick()
    {
        if (options.backend == BackendKind::AXI)
        {
            axi.Tick();
            while (auto error = axi.PopError())
                Error(INVALID_TRANSACTION, std::move(*error));
            while (const auto completion = axi.PeekCompletion())
            {
                HandleAXICompletion(*completion);
                axi.PopCompletion();
            }
        }
        ProcessResponses();
        AdmitRequests();
        AdvanceTransactions();
        AdvanceBackendRetries();

        for (const auto& [index, retry] : retries)
            QueueRetryCredit(index);

        RetireFinished();
    }

    void Core::Impl::ProcessResponses()
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

    void Core::Impl::ProcessRSP(const Response& flit)
    {
        namespace RSP = CHI::Eb::Opcodes::RSP;

        if (flit.opcode == RSP::PCrdGrant)
        {
            ++backendCredits[{ flit.source, flit.pCrdType }];
            return;
        }

        if (flit.opcode == RSP::RetryAck)
        {
            Transaction* transaction = tracker.FindMemory(flit.source, flit.txnId);
            if (!transaction || !HandleBackendRetry(*transaction, flit))
                Error(transaction ? transaction->handle : INVALID_TRANSACTION, "unexpected downstream RetryAck");
            return;
        }

        if (flit.opcode == RSP::CompAck)
        {
            Transaction* transaction = tracker.FindDBID(flit.source, flit.txnId);
            if (!transaction)
            {
                Error(INVALID_TRANSACTION, "unmatched CompAck");
                return;
            }
            const auto requestClass = ClassifyRequest(transaction->request.opcode);
            const FlowState& flow = flows.at(transaction->handle);
            bool responseComplete = transaction->phase == Phase::WaitCompAck && transaction->Complete(COMPLETE_RESPONSE);
            const bool earlyWriteAck = (requestClass == RequestClass::NonCoherentWrite ||
                requestClass == RequestClass::ImmediateWrite) && flow.dbidEmitted &&
                !transaction->Complete(COMPLETE_RESPONSE);
            const bool earlySeparatedAck = flow.separatedResponse &&
                flow.respSepEmitted && (transaction->request.order == 0 || flow.readBeatsEmitted != 0) &&
                !transaction->Complete(COMPLETE_RESPONSE);
            const bool dmtResponseReady = flow.dmtActive &&
                (flow.separatedResponse ? flow.respSepEmitted
                                        : transaction->request.order == 0 || flow.readReceiptEmitted) &&
                !transaction->Complete(COMPLETE_RESPONSE);
            if (!RequiresTransactionCompAck(*transaction) || transaction->Complete(COMPLETE_COMPACK) ||
                (!responseComplete && !earlyWriteAck && !earlySeparatedAck && !dmtResponseReady))
            {
                Error(transaction->handle, "unexpected or early CompAck");
                return;
            }
            if (dmtResponseReady)
            {
                FlowState& mutableFlow = flows.at(transaction->handle);
                mutableFlow.backendPhase = BackendPhase::Done;
                transaction->Mark(COMPLETE_MEMORY);
                transaction->Mark(COMPLETE_RESPONSE);
                transaction->memoryRequired = false;
                responseComplete = true;
            }
            transaction->Mark(COMPLETE_COMPACK);
            if (IsOrderedWriteObservation(transaction->request))
                ordering.Release(transaction->handle);
            if (responseComplete)
            {
                transaction->phase = Phase::Commit;
                transaction->readyTime = time;
                transaction->wait = WaitReason::None;
            }
            return;
        }

        if (flit.opcode == RSP::SnpRespFwded)
        {
            Transaction* transaction = tracker.FindSnoop(flit.source, flit.txnId);
            if (!transaction)
            {
                Error(INVALID_TRANSACTION, "unmatched SnpRespFwded");
                return;
            }
            FlowState& flow = flows.at(transaction->handle);
            if (!flow.dctActive ||
                std::find(transaction->pendingSnoops.begin(), transaction->pendingSnoops.end(), flit.source) ==
                    transaction->pendingSnoops.end())
            {
                Error(transaction->handle, "unexpected forwarding snoop response");
                return;
            }
            flow.dctForwarded = true;
            CompleteSnoopTarget(*transaction, flit.source, flit.resp, false);
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

        if (flit.opcode == RSP::ReadReceipt && flit.target == nodeId)
        {
            const RequestKey key{ flit.source, flit.txnId };
            auto completed = completedBackendReads.find(key);
            if (completed != completedBackendReads.end())
            {
                completedBackendReads.erase(completed);
                completedBackendReadOrder.remove(key);
                return;
            }
        }

        if (Transaction* transaction = tracker.FindMemory(flit.source, flit.txnId))
        {
            if (!HandleBackendRSP(*transaction, flit))
                Error(transaction->handle, "unexpected downstream response");
            return;
        }

        Error(INVALID_TRANSACTION, "unmatched response");
    }

    void Core::Impl::ProcessDAT(const Data& flit)
    {
        namespace DAT = CHI::Eb::Opcodes::DAT;

        if (flit.opcode == DAT::CompData || flit.opcode == DAT::DataSepResp)
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

        if (flit.opcode == DAT::SnpRespData || flit.opcode == DAT::SnpRespDataPtl ||
            flit.opcode == DAT::SnpRespDataFwded)
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

    void Core::Impl::AdmitRequests()
    {
        if (rxreq.empty())
            return;

        const auto decision = events->Decide(RequestAdmissionEvent(time, nodeId, rxreq.front()));
        if (decision.IsBackpressured())
        {
            pmu.RecordStall(WaitReason::Decision);
            return;
        }

        bool requestRetry = decision.IsRetryRequested();
        if (requestRetry && (!options.retryEnabled || !rxreq.front().allowRetry ||
            rxreq.front().opcode == CHI::Eb::Opcodes::REQ::PCrdReturn))
        {
            Error(INVALID_TRANSACTION, "requester retry decision is not legal for this request");
            requestRetry = false;
        }

        if (rxreq.front().opcode == CHI::Eb::Opcodes::REQ::PCrdReturn)
        {
            Request request = std::move(rxreq.front());
            rxreq.pop_front();
            if (!ConsumeRetryRequest(request, true))
                Deny(request, "PCrdReturn has no matching credit");
            return;
        }

        if (!rxreq.front().allowRetry)
        {
            if (!ordering.CanAdmit(rxreq.front()))
            {
                pmu.RecordStall(WaitReason::InputQueue);
                return;
            }
            Request request = std::move(rxreq.front());
            rxreq.pop_front();
            ConsumeRetryRequest(request, false);
            AdmitRequest(request);
            return;
        }

        const bool reservationAvailable = RetryReservationAvailable();
        if (!reservationAvailable)
            pmu.RecordStall(WaitReason::Tracker);
        if (requestRetry || !reservationAvailable)
        {
            Request& request = rxreq.front();
            if (!options.retryEnabled || !request.allowRetry)
                return;
            if (txrsp.size() >= options.outputQueueLimit)
            {
                pmu.RecordStall(WaitReason::OutputQueue);
                return;
            }

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

        if (!ordering.CanAdmit(rxreq.front()))
        {
            pmu.RecordStall(WaitReason::InputQueue);
            return;
        }

        Request request = std::move(rxreq.front());
        rxreq.pop_front();
        AdmitRequest(request);
    }

    void Core::Impl::AdmitRequest(const Request& request)
    {
        const Time age = nextAge++;
        Transaction* transaction = tracker.Allocate(request, age);
        if (!transaction)
        {
            Deny(request, "duplicate live requester transaction ID");
            return;
        }
        ++pmu.admittedTransactions;

        if (!ordering.Add(*transaction))
        {
            Error(transaction->handle, "duplicate requester ordering gate");
            transaction->canceled = true;
            transaction->phase = Phase::Commit;
            return;
        }

        const auto requestClass = ClassifyRequest(request.opcode);
        const bool separatedEligible = requestClass == RequestClass::NonCoherentRead ||
            requestClass == RequestClass::ReadOnce;
        const bool separatedLegal = separatedEligible &&
            (request.order == 0 || RequiresCompAck(request));
        FlowState flow;
        flow.separatedResponse = separatedLegal && options.separatedReadResponse;
        flow.separatedBackendRead = separatedLegal && options.separatedBackendRead;
        flow.dmtActive = options.dmtEnabled && options.backend == BackendKind::CHI && separatedEligible &&
            CHI::Eb::MemAttrs::ExtractAllocate(request.memAttr) == 0;
        flow.dctActive = options.dctEnabled &&
            (requestClass == RequestClass::ReadOnce || requestClass == RequestClass::CoherentRead) &&
            request.opcode != CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid &&
            CHI::Eb::MemAttrs::ExtractAllocate(request.memAttr) == 0;
        flow.dwtActive = options.dwtEnabled && options.backend == BackendKind::CHI &&
            requestClass == RequestClass::ImmediateWrite && !IsStashRequest(request.opcode) &&
            !request.expCompAck && !IsOrderedWriteObservation(request);
        flow.directPersistActive = options.directPersistEnabled && options.backend == BackendKind::CHI &&
            request.opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep;
        flows.emplace(transaction->handle, std::move(flow));
        transaction->readyTime = pipeline.SaturatingAdd(time, 1);
        transaction->phase = Phase::Receive;
    }

    void Core::Impl::AdvanceTransactions()
    {
        std::vector<TransactionHandle> handles = tracker.Handles();
        std::sort(handles.begin(), handles.end(), [&](TransactionHandle a, TransactionHandle b) {
            const Transaction* ta = tracker.Find(a);
            const Transaction* tb = tracker.Find(b);
            if (ta->actionAge != tb->actionAge)
                return ta->actionAge < tb->actionAge;
            return a < b;
        });

        if (options.randomizeArbitration && handles.size() > 1)
        {
            struct WorkGroup {
                LineKey                       line;
                std::vector<TransactionHandle> handles;
            };
            std::vector<WorkGroup> groups;
            std::map<LineKey, size_t> groupByLine;
            for (TransactionHandle handle : handles)
            {
                const Transaction* transaction = tracker.Find(handle);
                if (!transaction || !pipeline.Eligible(*transaction, time))
                    continue;
                const LineKey line = transaction->request.Line();
                auto [it, inserted] = groupByLine.emplace(line, groups.size());
                if (inserted)
                    groups.push_back({ line, {} });
                groups[it->second].handles.push_back(handle);
            }
            for (size_t size = groups.size(); size > 1; --size)
                std::swap(groups[size - 1], groups[random.Bounded(size - 1)]);
            handles.clear();
            for (const WorkGroup& group : groups)
                handles.insert(handles.end(), group.handles.begin(), group.handles.end());
        }

        for (TransactionHandle handle : handles)
        {
            Transaction* transaction = tracker.Find(handle);
            if (!transaction || !pipeline.Eligible(*transaction, time))
                continue;

            FlowState& flow = flows.at(handle);
            if (flow.timingPhase != transaction->phase)
            {
                flow.timingPhase = transaction->phase;
                flow.timingApplied = false;
            }
            if (!flow.timingApplied)
            {
                Time delay = pipeline.PhaseDelay(transaction->phase);
                if (options.randomDelayLimit && pipeline.DelayablePhase(transaction->phase))
                    delay = pipeline.SaturatingAdd(delay, random.Bounded(options.randomDelayLimit));
                transaction->readyTime = pipeline.SaturatingAdd(transaction->readyTime, delay);
                flow.timingApplied = true;
            }

            if (flow.eventPhase != transaction->phase)
            {
                flow.eventPhase = transaction->phase;
                flow.eventApplied = false;
            }
            if (!flow.eventApplied)
            {
                const auto decision = events->Decide(TransactionScheduleEvent(time, nodeId, *transaction));
                if (decision.IsBackpressured())
                {
                    pmu.RecordStall(WaitReason::Decision);
                    continue;
                }
                transaction->readyTime = pipeline.SaturatingAdd(
                    transaction->readyTime, decision.GetAdditionalDelay());
                flow.eventApplied = true;
            }
            if (pipeline.Eligible(*transaction, time))
                Advance(*transaction);
        }
    }

    Time Core::Impl::ScheduleTag()
    {
        if (pipeline.TagBusy(time))
            pmu.RecordStall(WaitReason::TagPort);
        return pipeline.ScheduleTag(time);
    }

    Time Core::Impl::ScheduleData()
    {
        if (pipeline.DataBusy(time))
            pmu.RecordStall(WaitReason::DataPort);
        return pipeline.ScheduleData(time);
    }

    void Core::Impl::Advance(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        switch (transaction.phase)
        {
            case Phase::Receive:
                pipeline.Advance(transaction, Phase::Decode, time);
                return;
            case Phase::Decode:
            {
                const auto requestClass = ClassifyRequest(transaction.request.opcode);
                const bool invalidStash = (requestClass == RequestClass::Stash &&
                    (transaction.request.size != 6 || transaction.request.order != 0 || transaction.request.expCompAck)) ||
                    (IsStashRequest(transaction.request.opcode) && !transaction.request.stashNidValid) ||
                    (transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash &&
                     transaction.request.size != 6);
                const bool invalidCleanUnique = requestClass == RequestClass::CleanUnique &&
                    (transaction.request.size != 6 || !transaction.request.expCompAck);
                const bool invalidPersistence = requestClass == RequestClass::Persistence &&
                    (transaction.request.size != 6 || transaction.request.expCompAck ||
                     (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersist &&
                      transaction.request.pGroupId != 0));
                AtomicGeometry atomicGeometry;
                const bool invalidAtomic = requestClass == RequestClass::Atomic &&
                    (!GetAtomicGeometry(transaction.request, atomicGeometry) || transaction.request.expCompAck);
                const bool invalidOrder = transaction.request.order == 1 ||
                    (transaction.request.order != 0 && !IsOrderingSupported(transaction.request.opcode));
                if (options.backend == BackendKind::AXI && requestClass == RequestClass::Persistence)
                {
                    Deny(transaction.request, "AXI write response is not a persistence guarantee");
                    transaction.canceled = true;
                    transaction.dbid = transaction.handle;
                    transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
                    if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
                        QueueCompPersist(transaction);
                    else
                        QueueComp(transaction, false, CHI::Eb::Resps::Comp_UC, CHI::Eb::RespErrs::NDERR);
                    return;
                }
                if (!IsSupportedRequest(transaction.request.opcode) || transaction.request.size > 6 ||
                    (requestClass == RequestClass::ZeroWrite && transaction.request.size != 6) || invalidStash ||
                    invalidCleanUnique || invalidPersistence || invalidAtomic || invalidOrder)
                {
                    Deny(transaction.request, "unsupported request opcode or transfer geometry");
                    transaction.canceled = true;
                    transaction.dbid = transaction.handle;
                    QueueComp(transaction, false, CHI::Eb::Resps::Comp_I, 0b10);
                    return;
                }
                pipeline.Advance(transaction, Phase::Admit, time);
                return;
            }
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
                    pmu.RecordStall(WaitReason::SameLine);
                    return;
                }
                transaction.wait = WaitReason::None;
                transaction.phase = Phase::Lookup;
                transaction.readyTime = ScheduleTag();
                return;
            case Phase::Lookup:
            {
                const HomeLine* home = directory.Find(transaction.request.Line());
                transaction.homeHit = home && home->valid;
                flow.validBytes.fill(1);
                if (transaction.homeHit)
                {
                    transaction.data = home->data;
                    CopyMetadata(transaction, *home, options);
                    flow.homeDirty = home->dirty;
                    flow.validBytes = home->validBytes;
                }
                else if (IsReadRequest(transaction.request.opcode) || IsCMORequest(transaction.request.opcode))
                {
                    const LineImage image = memory.ReadImage(transaction.request.Line());
                    transaction.data = image.data;
                    CopyMetadata(transaction, image, options);
                }
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
                {
                    pmu.RecordStall(WaitReason::Memory);
                    return;
                }
                if (options.backend == BackendKind::Internal && transaction.memoryRequired)
                    CompleteMemoryRead(transaction);
                else if (flow.backendPhase == BackendPhase::Done)
                    pipeline.Advance(transaction, Phase::Action, time);
                else
                    pmu.RecordStall(WaitReason::Memory);
                return;
            case Phase::WaitWriteData:
                pmu.RecordStall(WaitReason::WriteData);
                return;
            case Phase::WaitCompAck:
                pmu.RecordStall(WaitReason::CompAck);
                return;
            case Phase::WaitSnoop:
                if (transaction.pendingSnoops.empty())
                    pipeline.Advance(transaction, Phase::Action, time);
                else
                    pmu.RecordStall(WaitReason::Snoop);
                return;
            case Phase::Return:
                if (IsAtomicRequest(transaction.request.opcode))
                    ReturnAtomic(transaction);
                else if (IsReadRequest(transaction.request.opcode))
                    ReturnRead(transaction);
                return;
            case Phase::Commit:
            case Phase::Retired:
                return;
        }
    }

    void Core::Impl::Resolve(Transaction& transaction)
    {
        if (IsCoherentRequest(transaction.request.opcode) || ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce)
            ResolveCoherent(transaction);
        else
            ResolveNonCoherent(transaction);
    }

    void Core::Impl::Action(Transaction& transaction)
    {
        if (IsCoherentRequest(transaction.request.opcode) || ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce)
            ActionCoherent(transaction);
        else
            ActionNonCoherent(transaction);
    }

    void Core::Impl::RetireFinished()
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

    void Core::Impl::Retire(TransactionHandle handle)
    {
        Transaction* transaction = tracker.Find(handle);
        if (!transaction)
            return;

        FlowState& flow = flows.at(handle);
        if (flow.dmtActive && !flow.backendReadReceiptReceived)
        {
            const RequestKey key{ options.subordinateId, flow.backendRequest.txnId };
            completedBackendReads[key] = flow.backendRequest;
            completedBackendReadOrder.remove(key);
            completedBackendReadOrder.push_back(key);
            if (completedBackendReads.size() > 64)
            {
                completedBackendReads.erase(completedBackendReadOrder.front());
                completedBackendReadOrder.pop_front();
            }
        }
        ordering.Release(handle);
        hazards.Release(handle, transaction->request.Line());
        if (flow.victimLinked)
            hazards.Release(handle, flow.victimLine);
        ++pmu.retiredTransactions;

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

    void Core::Impl::OnEmitted(const Pending<Request>& pending)
    {
        Transaction* transaction = tracker.Find(pending.transaction);
        if (!transaction)
            return;
        FlowState& flow = flows.at(transaction->handle);
        if (flow.backendPhase == BackendPhase::WaitRequestEmission)
        {
            flow.backendRequest = pending.flit;
            flow.backendPhase = flow.dwtActive || (!flow.spillPending &&
                (IsReadRequest(transaction->request.opcode) || IsCMORequest(transaction->request.opcode)))
                ? BackendPhase::WaitComp : BackendPhase::WaitDBID;
            if (!flow.dmtActive)
                transaction->phase = Phase::WaitMemory;
            transaction->readyTime = time;
        }
    }

    void Core::Impl::OnEmitted(const Pending<Response>& pending)
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
            return;
        }

        Transaction* transaction = tracker.Find(pending.transaction);
        if (!transaction)
            return;
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::ReadReceipt)
        {
            flows.at(transaction->handle).readReceiptEmitted = true;
            ordering.Release(transaction->handle);
            if (flows.at(transaction->handle).dmtActive)
                FinishResponse(*transaction);
            return;
        }
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::RespSepData)
        {
            flows.at(transaction->handle).respSepEmitted = true;
            ordering.Release(transaction->handle);
            FinishResponse(*transaction);
            return;
        }
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::StashDone)
        {
            flows.at(transaction->handle).stashDoneEmitted = true;
            FinishResponse(*transaction);
            return;
        }
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompStashDone)
        {
            flows.at(transaction->handle).compEmitted = true;
            flows.at(transaction->handle).stashDoneEmitted = true;
            FinishResponse(*transaction);
            return;
        }
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::DBIDResp ||
            pending.flit.opcode == CHI::Eb::Opcodes::RSP::DBIDRespOrd)
        {
            flows.at(transaction->handle).dbidEmitted = true;
            if (!IsOrderedWriteObservation(transaction->request))
                ordering.Release(transaction->handle);
            const auto requestClass = ClassifyRequest(transaction->request.opcode);
            if (requestClass == RequestClass::ImmediateWrite || requestClass == RequestClass::WriteUniqueZero ||
                requestClass == RequestClass::Atomic)
                FinishResponse(*transaction);
        }
        if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::Comp ||
            pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompPersist ||
            pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompDBIDResp)
        {
            flows.at(transaction->handle).compEmitted = true;
            if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompDBIDResp)
            {
                flows.at(transaction->handle).dbidEmitted = true;
                if (!IsOrderedWriteObservation(transaction->request))
                    ordering.Release(transaction->handle);
            }
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
            pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompPersist ||
            pending.flit.opcode == CHI::Eb::Opcodes::RSP::CompDBIDResp)
            FinishResponse(*transaction);
    }

    void Core::Impl::OnEmitted(const Pending<Data>& pending)
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
                if (IsAtomicRequest(transaction->request.opcode) && !flow.spillPending)
                {
                    const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction->request.opcode);
                    const bool complete = store ? flow.backendCompReceived : flow.atomic.backendResultReceived;
                    if (complete)
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
                    return;
                }
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
        if (IsReadRequest(transaction->request.opcode) &&
            (pending.flit.opcode == CHI::Eb::Opcodes::DAT::CompData ||
             pending.flit.opcode == CHI::Eb::Opcodes::DAT::DataSepResp))
            ++flow.readBeatsEmitted;
        if (pending.last)
        {
            if (IsAtomicRequest(transaction->request.opcode) &&
                !CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction->request.opcode))
                flow.atomic.returnEmitted = true;
            FinishResponse(*transaction);
        }
    }

    void Core::Impl::OnEmitted(const Pending<Snoop>&)
    {
    }

    bool Core::Impl::Reentry()
    {
        if (!events->Dispatching())
            return false;

        const std::string reason = "Lunatic same-node event reentry is prohibited";
        errors.push_back({ time, INVALID_TRANSACTION, reason });
        Event event;
        event.kind = EventKind::Error;
        event.time = time;
        event.home = nodeId;
        event.message = reason;
        events->Defer(std::move(event));
        return true;
    }

    void Core::Impl::Error(TransactionHandle handle, std::string reason)
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

    void Core::Impl::Deny(const Request& request, std::string reason)
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

    bool Core::Impl::HasInflight() const noexcept
    {
        return !rxreq.empty() || !rxrsp.empty() || !rxdat.empty() || !txreq.empty() || !txrsp.empty() ||
               !txdat.empty() || !txsnp.empty() || !retries.empty() || !backendRetries.empty() ||
               !backendCredits.empty() || !tracker.Empty() || !hazards.Empty() || !ordering.Empty() ||
               (options.backend == BackendKind::AXI && axi.HasInflight());
    }

    bool Core::Impl::ValidateInvariants() const noexcept
    {
        if (!tracker.Validate() || !hazards.Validate() || !ordering.Validate())
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
