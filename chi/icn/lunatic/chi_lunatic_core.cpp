#include "chi_lunatic_core.hpp"

#include <algorithm>
#include <bit>
#include <utility>

namespace CHI::Lunatic {

    Core::Impl::Impl(NodeId nodeId, Options options, std::shared_ptr<EventHub> events)
        : nodeId(nodeId)
        , options(std::move(options))
        , events(std::move(events))
        , bankMap(this->options)
        , policy(MakePolicy(this->options.policy))
        , tracker(this->options.transactionLimit)
        , random(this->options.seed)
        , axi(this->options)
    {
        banks.reserve(bankMap.Count());
        for (size_t index = 0; index < bankMap.Count(); ++index)
            banks.emplace_back(this->options);
    }

    size_t Core::Impl::BankIndex(const LineKey& line) const noexcept
    {
        return bankMap.Index(line);
    }

    BankState& Core::Impl::Bank(const LineKey& line)
    {
        return banks[BankIndex(line)];
    }

    const BankState& Core::Impl::Bank(const LineKey& line) const
    {
        return banks[BankIndex(line)];
    }

    BankState& Core::Impl::Bank(const Transaction& transaction)
    {
        return Bank(transaction.request.Line());
    }

    const BankState& Core::Impl::Bank(const Transaction& transaction) const
    {
        return Bank(transaction.request.Line());
    }

    size_t Core::Impl::OrdinaryTransactionLimit() const noexcept
    {
        return tracker.Limit() - options.structure.reservedTransactionEntries;
    }

    bool Core::Impl::OriginalAdmissionAvailable(const LineKey& line, bool resend) const
    {
        const size_t capacity = resend && options.structure.classes.enabled ?
            tracker.Limit() : OrdinaryTransactionLimit();
        if (tracker.Size() >= capacity)
            return false;
        const BankState& bank = Bank(line);
        const size_t reservations = !resend && options.structure.classes.enabled ?
            BankRetryReservations(BankIndex(line)) : 0;
        if (bankMap.Count() > 1 &&
            bank.activeOriginals + reservations >= tracker.Limit() / bankMap.Count())
            return false;
        return !options.structure.requestBufferLimit ||
            bank.requestBufferOccupants < options.structure.requestBufferLimit;
    }

    void Core::Impl::ReleaseOriginal(Transaction& transaction)
    {
        --Bank(transaction).activeOriginals;
    }

    void Core::Impl::ReleaseRequestBuffer(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.requestBufferOwned)
            return;
        flow.requestBufferOwned = false;
        --Bank(transaction).requestBufferOccupants;
    }

    void Core::Impl::ReleaseSnoopWave(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.snoopWaveOwned)
            return;
        flow.snoopWaveOwned = false;
        --Bank(flow.snoopLine).activeSnoopWaves;
    }

    bool Core::Impl::AcquireBackendOperation(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.backendOperationOwned)
            return true;
        BankState& bank = Bank(transaction);
        if (options.structure.memoryResourceLimit &&
            bank.backendOperations >= options.structure.memoryResourceLimit)
        {
            pmu.RecordStall(WaitReason::Memory);
            return false;
        }
        ++bank.backendOperations;
        flow.backendOperationOwned = true;
        return true;
    }

    bool Core::Impl::AcquireRefill(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.baselineOperationOwned)
            return true;
        BankState& bank = Bank(flow.baselineLine);
        if (options.structure.refillResourceLimit &&
            bank.refillOperations >= options.structure.refillResourceLimit)
        {
            pmu.RecordStall(WaitReason::Memory);
            return false;
        }
        ++bank.refillOperations;
        flow.baselineOperationOwned = true;
        return true;
    }

    void Core::Impl::ReleaseResponse(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.responseOwned)
            return;
        flow.responseOwned = false;
        --Bank(transaction).responseUnits;
    }

    void Core::Impl::ReleaseResponseIfComplete(Transaction& transaction)
    {
        if (transaction.Complete(COMPLETE_RESPONSE) &&
            (!RequiresTransactionCompAck(transaction) || transaction.Complete(COMPLETE_COMPACK)))
            ReleaseResponse(transaction);
    }

    void Core::Impl::ReleaseBackendOperation(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.backendOperationOwned)
            return;
        flow.backendOperationOwned = false;
        --Bank(transaction).backendOperations;
    }

    void Core::Impl::CompleteBackendOperation(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        flow.backendPhase = BackendPhase::Done;
        ReleaseBackendOperation(transaction);
    }

    void Core::Impl::ReleaseRefill(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.baselineOperationOwned)
            return;
        flow.baselineOperationOwned = false;
        --Bank(flow.baselineLine).refillOperations;
    }

    void Core::Impl::ReleaseRefill(Transaction& transaction, const LineKey& line)
    {
        if (flows.at(transaction.handle).baselineLine == line)
            ReleaseRefill(transaction);
    }

    size_t Core::Impl::ClassForQos(uint8_t qos) const noexcept
    {
        const auto& classes = options.structure.classes;
        for (size_t index = 0; index < classes.qosMinimum.size(); ++index)
            if (qos >= classes.qosMinimum[index] && qos <= classes.qosMaximum[index])
                return index;
        return classes.qosMinimum.size();
    }

    size_t Core::Impl::RetryTier(size_t bank, size_t classId) const noexcept
    {
        const auto& classes = options.structure.classes;
        const size_t occupancy = banks[bank].activeClasses[classId] + banks[bank].retryReservations[classId];
        if (occupancy < classes.dedicated[classId])
            return 0;
        if (occupancy < classes.contendedMinimum[classId])
            return 1;
        return 2;
    }

    bool Core::Impl::RetryCreditEligible(const RetryRecord& retry) const
    {
        if (!retry.ackPopped ||
            (options.structure.classes.enabled ? retry.creditReserved || retry.creditReturned : retry.grantQueued) ||
            OutstandingRetryCredits(retry.request.source, retry.request.pCrdType) >=
                RetryOriginals(retry.request.source, retry.request.pCrdType))
            return false;
        return !options.structure.classes.enabled ||
            banks[retry.bank].activeClasses[retry.classId] + banks[retry.bank].retryReservations[retry.classId] <
                options.structure.classes.maximum[retry.classId];
    }

    bool Core::Impl::ClassAdmissionAvailable(const Request& request, size_t classId, bool resend) const
    {
        if (!OriginalAdmissionAvailable(request.Line(), resend))
            return false;
        if (!options.structure.classes.enabled)
            return true;
        if (classId >= options.structure.classes.maximum.size())
            return false;

        const auto& classes = options.structure.classes;
        const BankState& bank = Bank(request.Line());
        const size_t additional = resend ? 0 : 1;
        if (bank.activeClasses[classId] + bank.retryReservations[classId] + additional >
            classes.maximum[classId])
            return false;
        if (resend)
            return true;

        size_t dedicatedOthers = 0;
        for (size_t index = 0; index < classes.dedicated.size(); ++index)
            if (index != classId)
                dedicatedOthers += classes.dedicated[index];
        return tracker.Size() + TotalRetryReservations() + additional + dedicatedOthers <=
            OrdinaryTransactionLimit();
    }

    bool Core::Impl::RetryAdmissionAvailable(const Request& request, size_t classId) const
    {
        (void)request;
        return !options.structure.classes.enabled || classId < options.structure.classes.maximum.size();
    }

    size_t Core::Impl::RetrySourceCount() const noexcept
    {
        size_t sources = 0;
        NodeId previous = 0;
        bool havePrevious = false;
        for (const auto& [key, count] : retrySources)
        {
            (void)count;
            if (havePrevious && key.first == previous)
                continue;
            previous = key.first;
            havePrevious = true;
            ++sources;
        }
        return sources;
    }

    bool Core::Impl::RetrySourceAvailable(NodeId source) const noexcept
    {
        if (!options.structure.retrySourceLimit)
            return true;
        for (const auto& [key, count] : retrySources)
        {
            (void)count;
            if (key.first == source)
                return true;
        }
        return RetrySourceCount() < options.structure.retrySourceLimit;
    }

    void Core::Impl::AcquireRetrySource(const Request& request)
    {
        const size_t bank = BankIndex(request.Line());
        const size_t classId = options.structure.classes.enabled ? ClassForQos(request.qos) : request.pCrdType;
        ++retrySources[{ request.source, { bank, classId } }];
    }

    void Core::Impl::ReleaseRetrySource(NodeId source, size_t bank, size_t classId)
    {
        const auto key = std::make_pair(source, std::make_pair(bank, classId));
        auto it = retrySources.find(key);
        if (it == retrySources.end())
            return;
        if (it->second > 1)
            --it->second;
        else
            retrySources.erase(it);
    }

    void Core::Impl::ReleaseClass(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.classOwned)
            return;
        --Bank(transaction).activeClasses[flow.assignedClass];
        flow.classOwned = false;
    }

    size_t Core::Impl::BankRetryReservations(size_t bank) const noexcept
    {
        size_t total = 0;
        for (size_t count : banks[bank].retryReservations)
            total += count;
        return total;
    }

    size_t Core::Impl::TotalRetryReservations() const noexcept
    {
        size_t total = 0;
        for (const BankState& bank : banks)
            for (size_t count : bank.retryReservations)
                total += count;
        return total;
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
        availableCredits.clear();
        cancellationCredits.clear();
        retrySources.clear();
        backendRetries.clear();
        backendCredits.clear();
        completedBackendReads.clear();
        completedBackendReadOrder.clear();
        flows.clear();
        errors.clear();
        tracker.Clear();
        ordering.Clear();
        for (BankState& bank : banks)
        {
            bank.hazards.Clear();
            bank.directory.Clear();
            bank.snoopFilter.Clear();
            bank.pipeline.Reset();
            bank.activeOriginals = 0;
            bank.requestBufferOccupants = 0;
            bank.activeSnoopWaves = 0;
            bank.outgoingSnoopQueue = 0;
            bank.responseUnits = 0;
            bank.backendOperations = 0;
            bank.refillOperations = 0;
            bank.activeClasses.fill(0);
            bank.retryReservations.fill(0);
        }
        memory.Clear();
        axi.Reset();
        random.Reset(options.seed);
        pmu.Reset();
        time = 0;
        nextAge = 0;
        nextSerial = 0;
        nextBackendRetry = 0;
        nextRetry = 0;
        nextBackendSelection = 0;
        retryRounds = {};
        backendRetryRounds = {};
        responseCategoryRound = {};
        responseClassRounds = {};

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

        QueueRetryCredits();

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
            bool responseComplete = transaction->Complete(COMPLETE_RESPONSE);
            const bool earlyReadAck = !transaction->canceled && !flow.dmtActive && !flow.separatedResponse &&
                IsReadRequest(transaction->request.opcode) && flow.readBeatsEmitted != 0 &&
                !transaction->Complete(COMPLETE_RESPONSE);
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
            const bool earlyDCTAck = flow.dctActive && flow.dctSnoopEmitted &&
                (!flow.dctProviderResponded || flow.dctForwarded) && !transaction->Complete(COMPLETE_RESPONSE);
            const bool compAckAllowed = !transaction->canceled && (AllowsCompAck(transaction->request) ||
                (requestClass == RequestClass::WriteEvictOrEvict && flow.evictOrEvictNoData));
            if (!compAckAllowed || transaction->Complete(COMPLETE_COMPACK) ||
                (!responseComplete && !earlyReadAck && !earlyWriteAck && !earlySeparatedAck &&
                 !dmtResponseReady && !earlyDCTAck))
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
            ReleaseResponseIfComplete(*transaction);
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
            if (!AcceptForwardedResponse(*transaction, flit.source, flit.resp, flit.fwdState, false))
            {
                Error(transaction->handle, "unexpected or illegal forwarding snoop response");
                return;
            }
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
            size_t classId = options.structure.classes.enabled ? ClassForQos(rxreq.front().qos) : 0;
            bool resend = false;
            const auto credit = availableCredits.find({ rxreq.front().source, rxreq.front().pCrdType });
            if (credit != availableCredits.end() && credit->second != 0)
            {
                for (auto iter = retries.rbegin(); iter != retries.rend(); ++iter)
                {
                    if (!RetryRequestCompatible(iter->second.request, rxreq.front(), nodeId))
                        continue;
                    classId = iter->second.classId;
                    resend = true;
                    break;
                }
            }
            if (!ClassAdmissionAvailable(rxreq.front(), classId, resend) || !ordering.CanAdmit(rxreq.front()))
            {
                pmu.RecordStall(WaitReason::InputQueue);
                return;
            }
            Request request = std::move(rxreq.front());
            rxreq.pop_front();
            if (resend)
                ConsumeRetryRequest(request, false);
            AdmitRequest(request, classId);
            return;
        }

        const size_t classId = options.structure.classes.enabled ? ClassForQos(rxreq.front().qos) :
            rxreq.front().pCrdType;
        const bool admissionAvailable = RetryReservationAvailable() &&
            ClassAdmissionAvailable(rxreq.front(), classId, false);
        if (!admissionAvailable)
            pmu.RecordStall(WaitReason::Tracker);
        if (requestRetry || !admissionAvailable)
        {
            Request& request = rxreq.front();
            if (!options.retryEnabled || !request.allowRetry)
                return;
            if (txrsp.size() >= options.outputQueueLimit)
            {
                pmu.RecordStall(WaitReason::OutputQueue);
                return;
            }

            if (!RetryAdmissionAvailable(request, classId) || !RetrySourceAvailable(request.source))
            {
                pmu.RecordStall(WaitReason::Tracker);
                return;
            }
            AcquireRetrySource(request);
            const size_t index = nextRetry++;
            const Time age = nextAge++;
            RetryRecord record{ request, BankIndex(request.Line()), classId, age, false, false, false };
            record.request.pCrdType = classId;
            Response response;
            response.source = nodeId;
            response.target = request.source;
            response.txnId = request.txnId;
            response.opcode = CHI::Eb::Opcodes::RSP::RetryAck;
            response.pCrdType = classId;
            response.qos = request.qos;
            response.traceTag = request.traceTag;
            retries.emplace(index, record);
            rxreq.pop_front();
            Queue(txrsp, response, INVALID_TRANSACTION, age, false, false, index);
            return;
        }

        if (!ClassAdmissionAvailable(rxreq.front(), classId, false))
        {
            pmu.RecordStall(WaitReason::Tracker);
            return;
        }
        if (!ordering.CanAdmit(rxreq.front()))
        {
            pmu.RecordStall(WaitReason::InputQueue);
            return;
        }

        Request request = std::move(rxreq.front());
        rxreq.pop_front();
        AdmitRequest(request, classId);
    }

    void Core::Impl::AdmitRequest(const Request& request, size_t classId)
    {
        if (classId == std::numeric_limits<size_t>::max())
            classId = ClassForQos(request.qos);
        if (options.structure.classes.enabled && classId >= options.structure.classes.maximum.size())
        {
            Deny(request, "request QoS has no assigned class");
            return;
        }
        const Time age = nextAge++;
        Transaction* transaction = tracker.Allocate(request, age);
        if (!transaction)
        {
            Deny(request, "duplicate live requester transaction ID");
            return;
        }
        ++pmu.admittedTransactions;
        BankState& bank = Bank(*transaction);
        ++bank.activeOriginals;
        ++bank.requestBufferOccupants;

        if (!ordering.Add(*transaction))
        {
            const TransactionHandle handle = transaction->handle;
            --bank.activeOriginals;
            --bank.requestBufferOccupants;
            tracker.Retire(handle);
            Error(handle, "duplicate requester ordering gate");
            return;
        }

        if (options.structure.classes.enabled)
            ++bank.activeClasses[classId];

        const auto requestClass = ClassifyRequest(request.opcode);
        const bool separatedEligible = requestClass == RequestClass::NonCoherentRead ||
            requestClass == RequestClass::ReadOnce;
        const bool separatedLegal = separatedEligible &&
            (request.order == 0 || RequiresCompAck(request));
        FlowState flow;
        flow.requestBufferOwned = true;
        flow.assignedClass = options.structure.classes.enabled ? classId : 0;
        flow.classOwned = options.structure.classes.enabled;
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
        transaction->readyTime = Bank(*transaction).pipeline.SaturatingAdd(time, 1);
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
                if (!transaction || !Bank(*transaction).pipeline.Eligible(*transaction, time))
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
            if (!transaction || !Bank(*transaction).pipeline.Eligible(*transaction, time))
                continue;

            FlowState& flow = flows.at(handle);
            if (flow.timingPhase != transaction->phase)
            {
                flow.timingPhase = transaction->phase;
                flow.timingApplied = false;
            }
            if (!flow.timingApplied)
            {
                Time delay = Bank(*transaction).pipeline.PhaseDelay(transaction->phase);
                if (options.randomDelayLimit && Bank(*transaction).pipeline.DelayablePhase(transaction->phase))
                    delay = Bank(*transaction).pipeline.SaturatingAdd(delay, random.Bounded(options.randomDelayLimit));
                transaction->readyTime = Bank(*transaction).pipeline.SaturatingAdd(transaction->readyTime, delay);
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
                transaction->readyTime = Bank(*transaction).pipeline.SaturatingAdd(
                    transaction->readyTime, decision.GetAdditionalDelay());
                flow.eventApplied = true;
            }
            if (Bank(*transaction).pipeline.Eligible(*transaction, time))
                Advance(*transaction);
        }
    }

    Time Core::Impl::ScheduleTag(Transaction& transaction)
    {
        BankState& bank = Bank(transaction);
        if (bank.pipeline.TagBusy(time))
            pmu.RecordStall(WaitReason::TagPort);
        return bank.pipeline.ScheduleTag(time);
    }

    Time Core::Impl::ScheduleData(Transaction& transaction)
    {
        BankState& bank = Bank(transaction);
        if (bank.pipeline.DataBusy(time))
            pmu.RecordStall(WaitReason::DataPort);
        return bank.pipeline.ScheduleData(time);
    }

    void Core::Impl::Advance(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        switch (transaction.phase)
        {
            case Phase::Receive:
                Bank(transaction).pipeline.Advance(transaction, Phase::Decode, time);
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
                Bank(transaction).pipeline.Advance(transaction, Phase::Admit, time);
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
                Bank(transaction).hazards.Add(transaction.handle, transaction.request.Line(), transaction.actionAge);
                transaction.phase = Phase::Hazard;
                transaction.readyTime = time;
                return;
            case Phase::Hazard:
                if (!Bank(transaction).hazards.IsHead(transaction.handle, transaction.request.Line()) ||
                    (flow.victimLinked && !Bank(flow.victimLine).hazards.IsHead(transaction.handle, flow.victimLine)))
                {
                    transaction.wait = WaitReason::SameLine;
                    transaction.readyTime = time;
                    pmu.RecordStall(WaitReason::SameLine);
                    return;
                }
                transaction.wait = WaitReason::None;
                transaction.phase = Phase::Lookup;
                transaction.readyTime = ScheduleTag(transaction);
                ReleaseRequestBuffer(transaction);
                return;
            case Phase::Lookup:
            {
                const HomeLine* home = Bank(transaction).directory.Find(transaction.request.Line());
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
                const CacheState requesterState = Bank(transaction).snoopFilter.State(transaction.request.Line(), transaction.request.source);
                flow.requesterWasValid = IsValid(requesterState);
                flow.requesterWasDirty = IsDirty(requesterState);
                Bank(transaction).pipeline.Advance(transaction, Phase::Resolve, time);
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
                    Bank(transaction).pipeline.Advance(transaction, Phase::Action, time);
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
                    Bank(transaction).pipeline.Advance(transaction, Phase::Action, time);
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
        ReleaseRequestBuffer(*transaction);
        ReleaseSnoopWave(*transaction);
        ReleaseOriginal(*transaction);
        ReleaseClass(*transaction);
        ReleaseResponse(*transaction);
        ReleaseBackendOperation(*transaction);
        ReleaseRefill(*transaction);
        Bank(transaction->request.Line()).hazards.Release(handle, transaction->request.Line());
        if (flow.victimLinked)
            Bank(flow.victimLine).hazards.Release(handle, flow.victimLine);
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
        events->Publish(std::move(event), false);

        tracker.Retire(handle);
        flows.erase(handle);
        events->DispatchDeferred();
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
            if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant)
            {
                if (auto it = retries.find(pending.retry); it != retries.end())
                    it->second.grantQueued = false;
                ++availableCredits[{ pending.flit.target, pending.flit.pCrdType }];
                return;
            }
            if (auto it = retries.find(pending.retry);
                it != retries.end() && pending.flit.opcode == CHI::Eb::Opcodes::RSP::RetryAck)
                it->second.ackPopped = true;
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
                        CompleteBackendOperation(*transaction);
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
                    CompleteBackendOperation(*transaction);
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

    void Core::Impl::OnEmitted(const Pending<Snoop>& pending)
    {
        --Bank(pending.flit.Line()).outgoingSnoopQueue;
        Transaction* transaction = tracker.Find(pending.transaction);
        if (!transaction)
            return;
        FlowState& flow = flows.at(transaction->handle);
        if (flow.dctActive && pending.flit.target == flow.dctTarget &&
            pending.flit.txnId == transaction->handle &&
            pending.flit.fwdNid == transaction->request.source &&
            pending.flit.fwdTxnId == transaction->request.txnId)
            flow.dctSnoopEmitted = true;
    }

    bool Core::Impl::Reentry()
    {
        if (!events->Dispatching(nodeId))
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
        if (!rxreq.empty() || !rxrsp.empty() || !rxdat.empty() || !txreq.empty() || !txrsp.empty() ||
            !txdat.empty() || !txsnp.empty() || !retries.empty() || !backendRetries.empty() ||
            !backendCredits.empty() || !availableCredits.empty() || !cancellationCredits.empty() ||
            !retrySources.empty() || !tracker.Empty() || !ordering.Empty() ||
            (options.backend == BackendKind::AXI && axi.HasInflight()))
            return true;
        for (const BankState& bank : banks)
            if (!bank.hazards.Empty() || bank.activeOriginals || bank.requestBufferOccupants ||
                bank.activeSnoopWaves || bank.outgoingSnoopQueue || bank.responseUnits ||
                bank.backendOperations || bank.refillOperations ||
                std::any_of(bank.activeClasses.begin(), bank.activeClasses.end(), [](size_t count) { return count != 0; }) ||
                std::any_of(bank.retryReservations.begin(), bank.retryReservations.end(), [](size_t count) { return count != 0; }))
                return true;
        return false;
    }

    bool Core::Impl::ValidateInvariants() const noexcept
    {
        const size_t occupancy = tracker.Size();
        const size_t capacity = options.structure.classes.enabled ?
            tracker.Limit() : OrdinaryTransactionLimit();
        if (!tracker.Validate() || !ordering.Validate() || occupancy > capacity)
            return false;

        std::vector<size_t> expectedOriginals(banks.size(), 0);
        std::vector<size_t> expectedBuffers(banks.size(), 0);
        std::vector<size_t> expectedWaves(banks.size(), 0);
        std::vector<size_t> expectedOutgoing(banks.size(), 0);
        std::vector<size_t> expectedResponses(banks.size(), 0);
        std::vector<size_t> expectedBackend(banks.size(), 0);
        std::vector<size_t> expectedRefills(banks.size(), 0);
        std::vector<std::array<size_t, 4>> expectedClasses(banks.size());
        std::vector<std::array<size_t, 4>> expectedRetryReservations(banks.size());
        for (TransactionHandle handle : tracker.Handles())
        {
            const Transaction* transaction = tracker.Find(handle);
            if (!transaction)
                return false;
            ++expectedOriginals[BankIndex(transaction->request.Line())];
            const FlowState& flow = flows.at(handle);
            if (flow.requestBufferOwned)
                ++expectedBuffers[BankIndex(transaction->request.Line())];
            if (flow.snoopWaveOwned)
            {
                const size_t owner = BankIndex(flow.snoopLine);
                if (owner >= banks.size())
                    return false;
                ++expectedWaves[owner];
            }
            if (flow.responseOwned)
                ++expectedResponses[BankIndex(transaction->request.Line())];
            if (flow.backendOperationOwned)
                ++expectedBackend[BankIndex(transaction->request.Line())];
            if (flow.baselineOperationOwned)
                ++expectedRefills[BankIndex(flow.baselineLine)];
            if (options.structure.classes.enabled)
            {
                if (!flow.classOwned || flow.assignedClass >= expectedClasses[0].size() ||
                    (flow.assignedClass != ClassForQos(transaction->request.qos) &&
                     flow.assignedClass != transaction->request.pCrdType))
                    return false;
                ++expectedClasses[BankIndex(transaction->request.Line())][flow.assignedClass];
            }
            else if (flow.classOwned)
                return false;
        }
        for (const auto& pending : txsnp)
            ++expectedOutgoing[BankIndex(pending.flit.Line())];

        std::map<std::pair<NodeId, uint8_t>, size_t> sourceReservations;
        std::map<std::pair<NodeId, uint8_t>, size_t> requiredReservations;
        std::map<std::pair<NodeId, uint8_t>, size_t> reservedCredits;
        if (options.structure.retrySourceLimit && RetrySourceCount() > options.structure.retrySourceLimit)
            return false;
        for (const auto& [key, count] : retrySources)
        {
            if (!count || key.second.first >= banks.size() ||
                (options.structure.classes.enabled && key.second.second >= expectedRetryReservations[0].size()))
                return false;
            sourceReservations[{ key.first, key.second.second }] += count;
        }
        for (const auto& [index, retry] : retries)
        {
            if (retry.bank >= banks.size() ||
                (options.structure.classes.enabled && retry.classId >= expectedRetryReservations[0].size()) ||
                (options.structure.classes.enabled && retry.grantQueued && !retry.creditReserved))
                return false;
            ++requiredReservations[{ retry.request.source, retry.request.pCrdType }];
            if (!options.structure.classes.enabled || !retry.creditReserved)
                continue;
            ++expectedRetryReservations[retry.bank][retry.classId];
            ++reservedCredits[{ retry.request.source, retry.request.pCrdType }];
            bool queued = false;
            for (const auto& pending : txrsp)
                queued = queued || (pending.retry == index &&
                    pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant);
            if (retry.grantQueued != queued)
                return false;
        }
        for (const auto& [key, count] : cancellationCredits)
        {
            auto original = requiredReservations.find(key);
            if (original == requiredReservations.end() || count > original->second)
                return false;
            original->second -= count;
        }
        for (const auto& [key, count] : sourceReservations)
        {
            const auto required = requiredReservations.find(key);
            if (required == requiredReservations.end() || count != required->second)
                return false;
        }
        for (const auto& [key, count] : reservedCredits)
            if (OutstandingRetryCredits(key.first, key.second) != count)
                return false;
        for (const auto& [key, count] : requiredReservations)
        {
            (void)count;
            if (OutstandingRetryCredits(key.first, key.second) > RetryOriginals(key.first, key.second))
                return false;
        }
        for (const auto& [key, count] : availableCredits)
        {
            (void)count;
            if (OutstandingRetryCredits(key.first, key.second) > RetryOriginals(key.first, key.second))
                return false;
        }
        for (const auto& pending : txrsp)
            if (pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant &&
                OutstandingRetryCredits(pending.flit.target, pending.flit.pCrdType) >
                    RetryOriginals(pending.flit.target, pending.flit.pCrdType))
                return false;

        for (size_t owner = 0; owner < banks.size(); ++owner)
        {
            const BankState& bank = banks[owner];
            if (bank.activeOriginals != expectedOriginals[owner] ||
                bank.requestBufferOccupants != expectedBuffers[owner] ||
                bank.activeSnoopWaves != expectedWaves[owner] ||
                bank.outgoingSnoopQueue != expectedOutgoing[owner] ||
                bank.responseUnits != expectedResponses[owner] ||
                bank.backendOperations != expectedBackend[owner] ||
                bank.refillOperations != expectedRefills[owner] ||
                bank.activeClasses != expectedClasses[owner] ||
                bank.retryReservations != expectedRetryReservations[owner])
                return false;
            if (bankMap.Count() > 1 && bank.activeOriginals > tracker.Limit() / bankMap.Count())
                return false;
            if (options.structure.classes.enabled)
                for (size_t classId = 0; classId < bank.activeClasses.size(); ++classId)
                    if (bank.activeClasses[classId] + bank.retryReservations[classId] >
                        options.structure.classes.maximum[classId])
                        return false;
            if (options.structure.requestBufferLimit &&
                bank.requestBufferOccupants > options.structure.requestBufferLimit)
                return false;
            if (options.structure.snoopResourceLimit &&
                bank.activeSnoopWaves > options.structure.snoopResourceLimit)
                return false;
            if (options.structure.outgoingSnoopQueueLimit &&
                bank.outgoingSnoopQueue > options.structure.outgoingSnoopQueueLimit)
                return false;
            if (options.structure.responseResourceLimit &&
                bank.responseUnits > options.structure.responseResourceLimit)
                return false;
            if (options.structure.memoryResourceLimit &&
                bank.backendOperations > options.structure.memoryResourceLimit)
                return false;
            if (options.structure.refillResourceLimit &&
                bank.refillOperations > options.structure.refillResourceLimit)
                return false;
        }

        for (const BankState& bank : banks)
        {
            if (!bank.hazards.Validate())
                return false;
            const size_t owner = static_cast<size_t>(&bank - banks.data());
            for (size_t set = 0; set < bank.directory.Sets(); ++set)
            {
                for (size_t way = 0; way < bank.directory.Ways(); ++way)
                {
                    const HomeLine& home = bank.directory.At(set, way);
                    if (home.valid && BankIndex(home.key) != owner)
                        return false;
                }
            }
            for (size_t set = 0; set < bank.snoopFilter.Sets(); ++set)
            {
                for (size_t way = 0; way < bank.snoopFilter.Ways(); ++way)
                {
                    const SnoopFilterEntry& entry = bank.snoopFilter.At(set, way);
                    if (entry.valid && BankIndex(entry.key) != owner)
                        return false;
                }
            }
        }
        if (!tracker.Empty())
            return true;

        for (const BankState& bank : banks)
        {
            for (size_t set = 0; set < bank.snoopFilter.Sets(); ++set)
            {
                for (size_t way = 0; way < bank.snoopFilter.Ways(); ++way)
                {
                    const SnoopFilterEntry& entry = bank.snoopFilter.At(set, way);
                    if (!entry.valid)
                        continue;
                    size_t validRequesters = 0;
                    size_t dirtyRequesters = 0;
                    for (const auto& [requester, state] : entry.requesters)
                    {
                        validRequesters += IsValid(state) ? 1 : 0;
                        dirtyRequesters += IsDirty(state) ? 1 : 0;
                    }

                    const HomeLine* home = Bank(entry.key).directory.Find(entry.key);
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
        }
        return true;
    }

}
