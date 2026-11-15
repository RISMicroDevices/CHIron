#include "chi_lunatic_core.hpp"

#include <algorithm>
#include <bit>

namespace CHI::Lunatic {

    Transaction* Core::Impl::FindBackendTransaction(NodeId source, NodeId target,
                                                     TransactionId txnId) noexcept
    {
        if (target != nodeId || source == nodeId)
            return nullptr;

        Transaction* result = nullptr;
        for (auto& [handle, flow] : flows)
        {
            if (flow.backendPhase == BackendPhase::None ||
                flow.backendPhase == BackendPhase::WaitRequestEmission || flow.backendRequest.txnId != txnId ||
                (flow.backendResponderKnown && flow.backendResponder != source))
                continue;
            Transaction* candidate = tracker.Find(handle);
            if (!candidate || !tracker.FindMemory(options.subordinateId, handle))
                continue;
            if (result)
                return nullptr;
            result = candidate;
        }
        return result;
    }

    void Core::Impl::RecordBackendResponder(Transaction& transaction, NodeId source) noexcept
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.backendResponderKnown)
            return;
        flow.backendResponder = source;
        flow.backendResponderKnown = true;
    }

    bool RequiresBackendReadReceipt(const FlowState& flow) noexcept
    {
        return !flow.backendRetryAckReceived &&
            (flow.backendRequest.opcode == CHI::Eb::Opcodes::REQ::ReadNoSnp ||
             flow.backendRequest.opcode == CHI::Eb::Opcodes::REQ::ReadNoSnpSep) &&
            flow.backendRequest.order != 0 && !flow.backendReadReceiptReceived;
    }

    bool Core::Impl::HandleBackendRetry(Transaction& transaction, const Response& flit)
    {
        FlowState& flow = flows.at(transaction.handle);
        const bool awaitingWriteAcceptance = flow.backendPhase == BackendPhase::WaitDBID &&
            !flow.backendCompReceived && !flow.atomic.backendResultReceived;
        const bool canceledBackendWrite = transaction.canceled && flow.backendOperationOwned;
        const bool pendingCmo = IsCMORequest(transaction.request.opcode) && flow.backendOperationOwned &&
            !flow.backendCompReceived && !flow.backendPersistReceived;
        if ((!transaction.memoryRequired && !awaitingWriteAcceptance && !canceledBackendWrite && !pendingCmo) ||
            !flow.backendRequest.allowRetry || backendRetries.contains(transaction.handle))
            return false;

        BackendRetryRecord record{ flow.backendRequest, transaction.handle, nextBackendRetry++ };
        record.bank = BankIndex(flow.backendRequest.Line());
        record.classId = options.structure.classes.enabled ? ClassForQos(flow.backendRequest.qos)
                                                           : flow.backendRequest.pCrdType;
        record.retryAckPCrdType = flit.pCrdType;
        record.retryAckSource = flit.source;
        backendRetries.emplace(transaction.handle, record);
        tracker.UnbindMemory(options.subordinateId, record.request.txnId);
        flow.backendPhase = BackendPhase::WaitRetry;
        flow.backendWriteTraceTag = 0;
        flow.backendBeatMask = 0;
        flow.backendCompReceived = false;
        flow.backendPersistReceived = false;
        flow.backendCompRespErr = 0;
        flow.backendPersistRespErr = 0;
        flow.separatedPersistResponse = false;
        flow.backendSeparatedCompReceived = false;
        flow.backendSeparatedPersistReceived = false;
        flow.persistSent = false;
        flow.persistEmitted = false;
        flow.backendSeparatedPersistResponse = false;
        flow.backendPersistFirst = false;
        flow.backendDataEmitted = false;
        flow.backendReadRespErrMask = 0;
        flow.backendReadResp = 0;
        flow.backendReadRespValid = false;
        flow.backendReadReceiptReceived = false;
        flow.backendRetryAckReceived = true;
        flow.atomic.backendResultReceived = false;
        flow.atomic.backendResultDBID = 0;
        flow.atomic.backendResultRespErr = 0;
        flow.atomic.backendDirtyCommitted = false;
        transaction.receivedBeats = 0;
        transaction.responseRespErr = CHI::Eb::RespErrs::OK;
        flow.backendBeatRespErr.clear();
        transaction.wait = WaitReason::Memory;
        transaction.phase = Phase::WaitMemory;
        transaction.readyTime = time;
        return true;
    }

    Request MakeBackendCreditReturnRequest(const BackendRetryRecord& retry, NodeId home) noexcept
    {
        return MakePCrdReturnRequest(home, retry.creditSource, retry.retryAckPCrdType,
                                     retry.request.qos, retry.request.traceTag);
    }

    void Core::Impl::AdvanceBackendRetries()
    {
        if (!options.structure.classes.enabled)
        {
            for (auto credit = backendCredits.begin(); credit != backendCredits.end();)
            {
                while (credit->second)
                {
                    auto selected = backendRetries.end();
                    for (auto iter = backendRetries.begin(); iter != backendRetries.end(); ++iter)
                    {
                        BackendRetryRecord& retry = iter->second;
                        if (retry.creditReceived || retry.retryAckSource != credit->first.first ||
                            retry.retryAckPCrdType != credit->first.second)
                            continue;
                        if (selected == backendRetries.end() ||
                            (options.strategy.downstreamRetrySelection == RetrySelectionKind::Oldest
                                ? retry.serial < selected->second.serial : retry.serial > selected->second.serial))
                            selected = iter;
                    }
                    if (selected == backendRetries.end())
                        break;
                    selected->second.creditSource = credit->first.first;
                    selected->second.creditReceived = true;
                    --credit->second;
                }
                if (!credit->second)
                    credit = backendCredits.erase(credit);
                else
                    ++credit;
            }
        }
        else
        {
            for (auto credit = backendCredits.begin(); credit != backendCredits.end();)
            {
                while (credit->second)
                {
                    const bool oldest = options.strategy.downstreamRetrySelection == RetrySelectionKind::Oldest;
                    uint64_t selectedSerial = oldest ? std::numeric_limits<uint64_t>::max() : 0;
                    for (const auto& [handle, retry] : backendRetries)
                    {
                        (void)handle;
                        if (!retry.creditReceived && retry.retryAckSource == credit->first.first &&
                            retry.retryAckPCrdType == credit->first.second)
                        {
                            if ((oldest && retry.serial < selectedSerial) || (!oldest && retry.serial > selectedSerial))
                                selectedSerial = retry.serial;
                        }
                    }
                    size_t tier = backendRetryRounds.size();
                    for (const auto& [handle, retry] : backendRetries)
                    {
                        (void)handle;
                        if (!retry.creditReceived && retry.retryAckSource == credit->first.first &&
                            retry.retryAckPCrdType == credit->first.second && retry.serial == selectedSerial)
                            tier = std::min(tier, RetryTier(retry.bank, retry.classId));
                    }
                    if (tier == backendRetryRounds.size())
                        break;

                    std::array<bool, 4> eligibleClasses{};
                    for (const auto& [handle, retry] : backendRetries)
                    {
                        (void)handle;
                        if (!retry.creditReceived && retry.retryAckSource == credit->first.first &&
                            retry.retryAckPCrdType == credit->first.second && retry.serial == selectedSerial &&
                            RetryTier(retry.bank, retry.classId) == tier)
                            eligibleClasses[retry.classId] = true;
                    }
                    WeightedRoundState& round = backendRetryRounds[tier];
                    const size_t classId = SelectWeighted(options.structure.classes.retryWeights,
                        eligibleClasses, round, false);
                    auto selected = backendRetries.end();
                    for (auto iter = backendRetries.begin(); iter != backendRetries.end(); ++iter)
                    {
                        BackendRetryRecord& retry = iter->second;
                        if (!retry.creditReceived && retry.retryAckSource == credit->first.first &&
                            retry.retryAckPCrdType == credit->first.second && retry.serial == selectedSerial &&
                            retry.classId == classId && RetryTier(retry.bank, retry.classId) == tier)
                        {
                            selected = iter;
                            break;
                        }
                    }
                    if (selected == backendRetries.end())
                        break;
                    selected->second.creditSource = credit->first.first;
                    selected->second.creditReceived = true;
                    selected->second.selection = nextBackendSelection++;
                    --credit->second;
                    SelectWeighted(options.structure.classes.retryWeights, eligibleClasses, round, true);
                }
                if (!credit->second)
                    credit = backendCredits.erase(credit);
                else
                    ++credit;
            }
        }

        for (auto iter = backendRetries.begin(); iter != backendRetries.end();)
        {
            if (options.structure.classes.enabled)
            {
                auto selected = backendRetries.end();
                for (auto candidate = backendRetries.begin(); candidate != backendRetries.end(); ++candidate)
                {
                    if (!candidate->second.creditReceived)
                        continue;
                    if (selected == backendRetries.end() ||
                        candidate->second.selection < selected->second.selection ||
                        (candidate->second.selection == selected->second.selection &&
                         candidate->first < selected->first))
                        selected = candidate;
                }
                if (selected == backendRetries.end())
                    break;
                iter = selected;
            }
            BackendRetryRecord& retry = iter->second;
            if (!retry.creditReceived)
            {
                ++iter;
                continue;
            }

            Transaction* transaction = tracker.Find(retry.transaction);
            if (!transaction)
            {
                iter = backendRetries.erase(iter);
                continue;
            }

            const bool canceled = transaction->canceled;
            Request request;
            if (canceled)
            {
                request = MakeBackendCreditReturnRequest(retry, nodeId);
            }
            else
            {
                request = retry.request;
                request.allowRetry = false;
                request.pCrdType = retry.retryAckPCrdType;
            }

            if (!Queue(txreq, request, retry.transaction, transaction->actionAge, false, true,
                std::numeric_limits<size_t>::max(), SchedulePoint::BackendResend))
            {
                transaction->readyTime = time;
                if (options.structure.classes.enabled)
                    break;
                ++iter;
                continue;
            }

            FlowState& flow = flows.at(transaction->handle);
            if (canceled)
                CompleteBackendOperation(*transaction);
            else
            {
                if (!tracker.BindMemory(transaction->handle, options.subordinateId, request.txnId))
                {
                    Error(transaction->handle, "unable to rebind downstream read ID");
                    transaction->canceled = true;
                    transaction->phase = Phase::Commit;
                }
                flow.backendPhase = BackendPhase::WaitRequestEmission;
                transaction->phase = Phase::WaitMemory;
                transaction->readyTime = time;
            }
            iter = backendRetries.erase(iter);
        }
    }

    void Core::Impl::RetryBackendSubmission(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        ReleaseBackendOperation(transaction);
        ReleaseRefill(transaction);
        transaction.memoryRequired = false;
        transaction.wait = WaitReason::None;
        transaction.readyTime = time;
        flow.backendCompReceived = false;
        flow.backendPersistReceived = false;
        flow.backendSeparatedPersistResponse = false;
        flow.backendPersistFirst = false;
        flow.backendDataEmitted = false;
        flow.backendPhase = BackendPhase::None;
        flow.directoryCommitted = false;
    }

    void Core::Impl::StartMemoryRead(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.dataPullWriteStash)
            flow.dmtActive = options.dmtEnabled && options.backend == BackendKind::CHI &&
                CHI::Eb::MemAttrs::ExtractAllocate(transaction.request.memAttr) == 0 &&
                !flow.dataPullHomeDataAvailable;
        const bool orderedSeparatedDMT = flow.dmtActive && flow.separatedResponse && transaction.request.order != 0;
        if (orderedSeparatedDMT && !ordering.CanIssueDMT(transaction))
        {
            transaction.wait = WaitReason::Memory;
            transaction.readyTime = time;
            return;
        }
        if (transaction.memoryRequired)
        {
            transaction.phase = Phase::WaitMemory;
            transaction.readyTime = time;
            return;
        }
        if ((transaction.homeHit && !flow.victimDataPartial) || (flow.snoopDataValid && !flow.snoopDataPartial && !flow.victimDataPartial))
        {
            transaction.memoryRequired = false;
            transaction.Mark(COMPLETE_MEMORY);
            transaction.phase = Phase::Action;
            transaction.readyTime = ScheduleData(transaction,
                transaction.homeHit ? options.homeHitDataLatency : 0);
            return;
        }
        if ((flow.snoopDataPartial || flow.victimDataPartial) && !transaction.homeHit)
        {
            flow.baselineFetch = true;
            flow.baselineFetched = false;
            flow.baselineLine = flow.victimPending && flow.victimDataPartial ? flow.victimLine : transaction.request.Line();
        }

        if (!AcquireBackendOperation(transaction) || (flow.baselineFetch && !AcquireRefill(transaction)))
        {
            ReleaseBackendOperation(transaction);
            ReleaseRefill(transaction);
            transaction.wait = WaitReason::Memory;
            transaction.readyTime = time;
            return;
        }
        transaction.memoryRequired = true;
        flow.memoryWrite = false;
        flow.backendBeatMask = 0;
        flow.backendReadResp = 0;
        flow.backendReadRespValid = false;
        const bool localAtomicRead = IsAtomicRequest(transaction.request.opcode) &&
            (options.backend != BackendKind::CHI || options.strategy.atomicExecution == AtomicExecutionKind::Local);
        flow.backendExpectedBeatMask = localAtomicRead ? (uint64_t(1) << BeatsPerLine(options)) - 1 : 0;
        if (options.backend == BackendKind::Internal)
        {
            flow.memoryReady = Bank(transaction).pipeline.SaturatingAdd(time, memory.NextLatency(options.memoryLatency));
            transaction.phase = Phase::WaitMemory;
            transaction.wait = WaitReason::Memory;
            transaction.readyTime = time;
            return;
        }
        if (options.backend == BackendKind::AXI)
        {
            AXIOperation operation;
            operation.transaction = transaction.handle;
            operation.line = flow.baselineFetch ? flow.baselineLine : transaction.request.Line();
            operation.beatMask = flow.baselineFetch
                ? (uint64_t(1) << BeatsPerLine(options)) - 1
                : flow.backendExpectedBeatMask ? flow.backendExpectedBeatMask
                : flow.expectedBeatMask ? flow.expectedBeatMask : (uint64_t(1) << BeatsPerLine(options)) - 1;
            Request geometryRequest = transaction.request;
            if (flow.baselineFetch)
            {
                geometryRequest.address = operation.line.address;
                geometryRequest.nonSecure = operation.line.nonSecure;
                geometryRequest.size = 6;
            }
            operation.geometry = GetTransferGeometry(geometryRequest, options);
            operation.memAttr = transaction.request.memAttr;
            operation.qos = transaction.request.qos;
            operation.atomic = IsAtomicRequest(transaction.request.opcode);
            if (!axi.Submit(operation))
            {
                RetryBackendSubmission(transaction);
                return;
            }
            flow.backendCompReceived = false;
            flow.backendDataEmitted = false;
            flow.backendPhase = BackendPhase::WaitComp;
            transaction.phase = Phase::WaitMemory;
            transaction.wait = WaitReason::Memory;
            transaction.readyTime = time;
            return;
        }

        Request request = transaction.request;
        request.source = nodeId;
        request.target = SelectSubordinate(options, request.address, request.nonSecure);
        request.txnId = transaction.handle;
        request.returnNid = nodeId;
        request.returnTxnId = transaction.handle;
        request.order = transaction.request.order != 0;
        if (flow.dmtActive)
        {
            request.returnNid = transaction.request.source;
            request.returnTxnId = transaction.request.txnId;
            request.order = 1;
        }
        request.opcode = flow.separatedBackendRead
            ? CHI::Eb::Opcodes::REQ::ReadNoSnpSep : CHI::Eb::Opcodes::REQ::ReadNoSnp;
        if (localAtomicRead)
            request.size = 6;
        if (flow.baselineFetch)
        {
            request.address = flow.baselineLine.address;
            request.nonSecure = flow.baselineLine.nonSecure;
            request.size = 6;
        }
        request.allowRetry = true;
        request.expCompAck = false;
        request.snpAttr = 0;
        request.doDwt = false;
        request.stashNidValid = false;
        request.stashLpidValid = false;
        if (!Queue(txreq, request, transaction.handle, transaction.actionAge, false, true))
        {
            RetryBackendSubmission(transaction);
            return;
        }
        if (orderedSeparatedDMT)
            ordering.HoldDMT(transaction);
        const RequestKey completedKey{ options.subordinateId, request.txnId };
        completedBackendReads.erase(completedKey);
        completedBackendReadOrder.remove(completedKey);
        if (!tracker.BindMemory(transaction.handle, options.subordinateId, transaction.handle))
        {
            Error(transaction.handle, "unable to bind downstream read ID");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return;
        }
        flow.backendPhase = BackendPhase::WaitRequestEmission;
        transaction.phase = flow.dmtActive ? Phase::Action : Phase::WaitMemory;
        transaction.wait = WaitReason::Memory;
        transaction.readyTime = time;
    }

    void Core::Impl::CompleteMemoryRead(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!transaction.memoryRequired)
            return;

        if (flow.memoryWrite)
            memory.WriteImage(transaction.request.Line(), MakeLineImage(transaction, options),
                              transaction.byteEnables, options);
        else if (flow.baselineFetch)
        {
            const LineImage baseline = memory.ReadImage(transaction.request.Line());
            const bool victimBaseline = flow.victimPending && flow.victimDataPartial;
            LineData& destination = victimBaseline ? flow.victimData : transaction.data;
            ByteEnables enables{};
            for (size_t index = 0; index < LINE_SIZE; ++index)
            {
                const bool preserve = victimBaseline ? flow.victimValidBytes[index]
                    : flow.snoopDataPartial ? flow.snoopValidBytes[index]
                    : IsCopybackRequest(transaction.request.opcode) && transaction.byteEnables[index];
                if (!preserve)
                {
                    destination[index] = baseline.data[index];
                    enables[index] = 1;
                }
            }
            LinePoison& poison = victimBaseline ? flow.victimPoison : transaction.poison;
            LineDataCheck& dataCheck = victimBaseline ? flow.victimDataCheck : transaction.dataCheck;
            MergeMetadata(poison, dataCheck, baseline.poison, baseline.dataCheck,
                          enables, 0, 0, LINE_SIZE, options);
        }
        else
        {
            const LineImage image = memory.ReadImage(transaction.request.Line());
            transaction.data = image.data;
            CopyMetadata(transaction, image, options);
        }

        transaction.Mark(COMPLETE_MEMORY);
        transaction.memoryRequired = false;
        transaction.wait = WaitReason::None;
        CompleteBackendOperation(transaction);
        transaction.phase = Phase::Action;
        transaction.readyTime = ScheduleData(transaction);
    }

    void Core::Impl::StartBackendWrite(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (transaction.canceled)
        {
            CompleteBackendOperation(transaction);
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }

        if (!AcquireBackendOperation(transaction))
        {
            transaction.wait = WaitReason::Memory;
            transaction.readyTime = time;
            return;
        }
        if (options.backend == BackendKind::Internal)
        {
            transaction.memoryRequired = true;
            flow.memoryWrite = true;
            flow.memoryReady = Bank(transaction).pipeline.SaturatingAdd(time, memory.NextLatency(options.memoryLatency));
            transaction.phase = Phase::WaitMemory;
            transaction.wait = WaitReason::Memory;
            transaction.readyTime = time;
            return;
        }
        if (options.backend == BackendKind::AXI)
        {
            const auto requestClass = ClassifyRequest(transaction.request.opcode);
            const bool fullSpill = flow.spillPending &&
                !(flow.spillLine == transaction.request.Line() &&
                  (requestClass == RequestClass::ImmediateWrite || requestClass == RequestClass::WriteUniqueZero));
            AXIOperation operation;
            operation.transaction = transaction.handle;
            operation.line = flow.spillPending ? flow.spillLine : transaction.request.Line();
            operation.data = flow.spillPending ? flow.spillData : transaction.data;
            operation.enables = flow.spillPending ? flow.spillEnables : transaction.byteEnables;
            operation.beatMask = fullSpill ? (uint64_t(1) << BeatsPerLine(options)) - 1 : flow.expectedBeatMask;
            if (!operation.beatMask)
                operation.beatMask = (uint64_t(1) << BeatsPerLine(options)) - 1;
            Request geometryRequest = transaction.request;
            if (flow.spillPending)
            {
                geometryRequest.address = operation.line.address;
                geometryRequest.nonSecure = operation.line.nonSecure;
                if (fullSpill)
                    geometryRequest.size = 6;
            }
            geometryRequest.memAttr = flow.spillGeneratedEviction ? uint8_t(CHI::Eb::MemAttrs::WriteBack_Allocate)
                                                                  : transaction.request.memAttr;
            operation.geometry = GetTransferGeometry(geometryRequest, options);
            operation.memAttr = geometryRequest.memAttr;
            operation.qos = transaction.request.qos;
            operation.write = true;
            if (!axi.Submit(operation))
            {
                RetryBackendSubmission(transaction);
                return;
            }
            flow.backendCompReceived = false;
            flow.backendDataEmitted = false;
            flow.backendPhase = BackendPhase::WaitComp;
            transaction.phase = Phase::WaitMemory;
            transaction.wait = WaitReason::Memory;
            transaction.readyTime = time;
            return;
        }

        Request request = transaction.request;
        request.source = nodeId;
        request.target = SelectSubordinate(options, request.address, request.nonSecure);
        request.txnId = transaction.handle;
        request.returnNid = nodeId;
        request.returnTxnId = 0;
        request.order = 0;
        request.allowRetry = true;
        request.expCompAck = false;
        request.snpAttr = 0;
        request.doDwt = false;
        request.stashNidValid = false;
        request.stashLpidValid = false;
        if (flow.atomic.localExecution)
        {
            request.opcode = CHI::Eb::Opcodes::REQ::WriteNoSnpFull;
            request.size = 6;
            flow.expectedBeatMask = (uint64_t(1) << BeatsPerLine(options)) - 1;
        }
        if (flow.dwtActive)
        {
            request.doDwt = true;
            request.snpAttr = 1;
            request.returnNid = transaction.request.source;
            request.returnTxnId = transaction.request.txnId;
            flow.directoryCommitted = true;
            Bank(transaction).snoopFilter.Track(transaction.request.Line(), transaction.request.source, CacheState::I, time);
        }
        const auto requestClass = ClassifyRequest(transaction.request.opcode);
        if (requestClass == RequestClass::ImmediateWrite || requestClass == RequestClass::WriteEvictOrEvict)
        {
            const bool full = requestClass == RequestClass::WriteEvictOrEvict ||
                transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFull ||
                transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash;
            request.opcode = full ? CHI::Eb::Opcodes::REQ::WriteNoSnpFull : CHI::Eb::Opcodes::REQ::WriteNoSnpPtl;
        }
        else if (requestClass == RequestClass::WriteUniqueZero)
            request.opcode = CHI::Eb::Opcodes::REQ::WriteNoSnpZero;
        if (flow.spillPending)
        {
            const bool sameLineImmediate = flow.spillLine == transaction.request.Line() &&
                (requestClass == RequestClass::ImmediateWrite || requestClass == RequestClass::WriteUniqueZero);
            request.address = flow.spillLine.address;
            request.nonSecure = flow.spillLine.nonSecure;
            request.memAttr = flow.spillGeneratedEviction ? uint8_t(CHI::Eb::MemAttrs::WriteBack_Allocate)
                                                          : transaction.request.memAttr;
            request.snpAttr = 0;
            if (!sameLineImmediate)
            {
                request.opcode = CHI::Eb::Opcodes::REQ::WriteNoSnpFull;
                request.size = 6;
                request.snoopMe = false;
                flow.expectedBeatMask = (uint64_t(1) << BeatsPerLine(options)) - 1;
            }
            flow.backendBeat = transaction.receivedBeats ? std::countr_zero(flow.expectedBeatMask) : 0;
            flow.backendBeatMask = 0;
            flow.backendCompReceived = false;
            flow.backendDataEmitted = false;
        }
        else
            flow.backendBeat = transaction.receivedBeats ? std::countr_zero(flow.expectedBeatMask) : 0;
        if (!Queue(txreq, request, transaction.handle, transaction.actionAge, false, true))
        {
            RetryBackendSubmission(transaction);
            return;
        }
        if (!tracker.BindMemory(transaction.handle, options.subordinateId, transaction.handle))
        {
            Error(transaction.handle, "unable to bind downstream write ID");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return;
        }
        flow.backendPhase = BackendPhase::WaitRequestEmission;
        transaction.phase = Phase::WaitMemory;
        transaction.wait = WaitReason::Memory;
        transaction.readyTime = time;
    }

    void Core::Impl::AdvanceBackendWrite(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.backendPhase != BackendPhase::SendData)
            return;
        if (flow.backendBeatMask && !ScheduleTransition(transaction, SchedulePoint::DataBeat))
            return;

        const size_t bytes = BytesPerBeat(options);
        const size_t beat = flow.backendBeat;
        if (beat >= BeatsPerLine(options) || !(flow.expectedBeatMask & (uint64_t(1) << beat)))
        {
            Error(transaction.handle, "invalid downstream write beat selection");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return;
        }

        const size_t offset = beat * bytes;
        Data data;
        data.source = nodeId;
        data.target = flow.backendDBIDSource;
        data.homeNid = 0;
        data.txnId = flow.backendDBID;
        data.dbid = 0;
        data.opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        const bool originalWriteData = !flow.spillPending ||
            (!flow.spillGeneratedEviction && flow.spillLine == transaction.request.Line());
        data.respErr = originalWriteData && (flow.writeRespErrMask & (uint64_t(1) << beat))
            ? flow.writeRespErr[beat] : 0;
        const bool snoopWriteData = flow.spillPending && !flow.spillGeneratedEviction &&
            flow.spillLine == transaction.request.Line() && flow.snoopDataValid && !flow.snoopForVictim;
        if (snoopWriteData && (flow.snoopDataRespErrMask & (uint64_t(1) << beat)))
            data.respErr = MergeResponseErrors(data.respErr, flow.snoopDataRespErr[beat]);
        const bool victimWriteData = flow.spillPending && flow.spillGeneratedEviction &&
            flow.spillLine == flow.victimLine && flow.victimDataValid;
        if (victimWriteData && (flow.victimDataRespErrMask & (uint64_t(1) << beat)))
            data.respErr = MergeResponseErrors(data.respErr, flow.victimDataRespErr[beat]);
        data.dataId = offset / 16;
        data.ccid = flow.spillPending ? (flow.spillLine.address >> 4) & 3 : (transaction.request.address >> 4) & 3;
        data.qos = transaction.request.qos;
        data.traceTag = flow.backendWriteTraceTag;
        const LineData& payload = flow.spillPending ? flow.spillData : transaction.data;
        const ByteEnables& enables = flow.spillPending ? flow.spillEnables : transaction.byteEnables;
        const LinePoison& poison = flow.spillPending ? flow.spillPoison : transaction.poison;
        const LineDataCheck& dataCheck = flow.spillPending ? flow.spillDataCheck : transaction.dataCheck;
        for (size_t index = 0; index < bytes; ++index)
        {
            data.bytes[index] = payload[offset + index];
            data.byteEnables[index] = enables[offset + index];
            data.dataCheck[index] = dataCheck[offset + index];
        }
        for (size_t lane = 0; lane < bytes / 8; ++lane)
            data.poison[lane] = poison[offset / 8 + lane];

        const uint64_t remaining = flow.expectedBeatMask & ~(flow.backendBeatMask | (uint64_t(1) << beat));
        if (!Queue(txdat, data, transaction.handle, transaction.actionAge, remaining == 0, true))
        {
            transaction.readyTime = time;
            return;
        }

        flow.backendBeatMask |= uint64_t(1) << beat;
        if (remaining == 0)
        {
            flow.backendPhase = BackendPhase::WaitDataEmission;
            transaction.phase = Phase::WaitMemory;
            transaction.readyTime = time;
        }
        else
        {
            flow.backendBeat = std::countr_zero(remaining);
            transaction.readyTime = time;
        }
    }

    bool Core::Impl::HandleBackendRSP(Transaction& transaction, const Response& flit)
    {
        namespace RSP = CHI::Eb::Opcodes::RSP;
        FlowState& flow = flows.at(transaction.handle);

        if (flit.opcode == RSP::ReadReceipt)
        {
            if (!IsReadRequest(transaction.request.opcode) ||
                flow.backendRequest.order == 0 || flow.backendReadReceiptReceived)
                return false;
            flow.backendReadReceiptReceived = true;
            if (flow.dmtActive && !transaction.Complete(COMPLETE_MEMORY))
            {
                transaction.Mark(COMPLETE_MEMORY);
                transaction.memoryRequired = false;
                CompleteBackendOperation(transaction);
                transaction.phase = Phase::Return;
                transaction.readyTime = time;
            }
            if (transaction.Complete(COMPLETE_MEMORY))
                tracker.UnbindMemory(options.subordinateId, transaction.handle);
            return true;
        }

        if (IsAtomicRequest(transaction.request.opcode) && !flow.atomic.localExecution && !flow.spillPending)
            return HandleAtomicBackendRSP(transaction, flit);

        if (IsCMORequest(transaction.request.opcode) && !flow.spillPending)
        {
            if (flow.directPersistActive || !options.subordinatePersistSepEnabled ||
                transaction.request.opcode != CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
            {
                if (flit.opcode != RSP::Comp)
                    return false;
                flow.backendCompReceived = true;
                flow.backendCompRespErr = flit.respErr;
                transaction.responseRespErr = flit.respErr;
                CompleteBackendOperation(transaction);
                tracker.UnbindMemory(options.subordinateId, transaction.handle);
                transaction.phase = Phase::Action;
                transaction.readyTime = time;
                return true;
            }

            if (flit.opcode == RSP::CompPersist)
            {
                if (flow.separatedPersistResponse || flow.backendPersistReceived ||
                    flit.pGroupId != transaction.request.pGroupId)
                    return false;
                flow.backendCompReceived = true;
                flow.backendPersistReceived = true;
                flow.backendCompRespErr = flit.respErr;
                flow.backendPersistRespErr = flit.respErr;
                transaction.responseRespErr = flit.respErr;
            }
            else if (flit.opcode == RSP::Comp)
            {
                if (flow.backendSeparatedCompReceived)
                    return false;
                flow.backendCompReceived = true;
                flow.backendCompRespErr = flit.respErr;
                flow.backendSeparatedCompReceived = true;
                flow.separatedPersistResponse = true;
                flow.backendSeparatedPersistResponse = true;
            }
            else if (flit.opcode == RSP::Persist)
            {
                if (flow.backendSeparatedPersistReceived || flit.pGroupId != transaction.request.pGroupId)
                    return false;
                flow.backendPersistFirst = !flow.backendCompReceived;
                flow.backendPersistReceived = true;
                flow.backendPersistRespErr = flit.respErr;
                flow.backendSeparatedPersistReceived = true;
                flow.separatedPersistResponse = true;
                flow.backendSeparatedPersistResponse = true;
            }
            else
                return false;

            if (flow.backendCompReceived && flow.backendPersistReceived)
            {
                CompleteBackendOperation(transaction);
                tracker.UnbindMemory(options.subordinateId, transaction.handle);
                transaction.phase = Phase::Action;
            }
            else
            {
                flow.backendPhase = BackendPhase::WaitComp;
                transaction.phase = Phase::WaitMemory;
            }
            transaction.readyTime = time;
            return true;
        }

        if (IsReadRequest(transaction.request.opcode) && !flow.spillPending)
            return false;

        if (flit.opcode == RSP::DBIDResp || flit.opcode == RSP::CompDBIDResp)
        {
            if (flow.backendPhase != BackendPhase::WaitDBID)
                return false;
            flow.backendDBID = flit.dbid;
            flow.backendDBIDSource = flit.source;
            flow.backendWriteTraceTag = flit.traceTag;
            flow.backendCompReceived |= flit.opcode == RSP::CompDBIDResp;
            if (ClassifyRequest(transaction.request.opcode) == RequestClass::ZeroWrite ||
                ClassifyRequest(transaction.request.opcode) == RequestClass::WriteUniqueZero)
            {
                if (flow.backendCompReceived)
                {
                    CompleteBackendOperation(transaction);
                    tracker.UnbindMemory(options.subordinateId, transaction.handle);
                    transaction.phase = Phase::Action;
                }
                else
                {
                    flow.backendPhase = BackendPhase::WaitComp;
                    transaction.phase = Phase::WaitMemory;
                }
            }
            else
            {
                flow.backendPhase = BackendPhase::SendData;
                transaction.phase = Phase::Action;
            }
            transaction.readyTime = time;
            return true;
        }

        if (flit.opcode == RSP::Comp)
        {
            if (flow.dwtActive)
            {
                flow.backendCompReceived = true;
                transaction.responseRespErr = flit.respErr;
                CompleteBackendOperation(transaction);
                tracker.UnbindMemory(options.subordinateId, transaction.handle);
                transaction.phase = Phase::Action;
                transaction.readyTime = time;
                return true;
            }
            if (ClassifyRequest(transaction.request.opcode) == RequestClass::ZeroWrite ||
                ClassifyRequest(transaction.request.opcode) == RequestClass::WriteUniqueZero)
            {
                if (flow.backendPhase != BackendPhase::WaitComp)
                    return false;
                flow.backendCompReceived = true;
                transaction.responseRespErr = flit.respErr;
                CompleteBackendOperation(transaction);
                tracker.UnbindMemory(options.subordinateId, transaction.handle);
                transaction.phase = Phase::Action;
                transaction.readyTime = time;
                return true;
            }
            if (flow.backendCompReceived ||
                (flow.backendPhase != BackendPhase::WaitDBID && flow.backendPhase != BackendPhase::SendData &&
                 flow.backendPhase != BackendPhase::WaitComp && flow.backendPhase != BackendPhase::WaitDataEmission))
                return false;
            flow.backendCompReceived = true;
            transaction.responseRespErr = MergeResponseErrors(transaction.responseRespErr, flit.respErr);
            if (flow.backendDataEmitted)
            {
                CompleteBackendOperation(transaction);
                tracker.UnbindMemory(options.subordinateId, transaction.handle);
                transaction.phase = Phase::Action;
            }
            transaction.readyTime = time;
            return true;
        }

        return false;
    }

    bool Core::Impl::HandleBackendDAT(Transaction& transaction, const Data& flit)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (options.backend == BackendKind::CHI && IsAtomicRequest(transaction.request.opcode) &&
            !flow.atomic.localExecution && !flow.spillPending && !flow.baselineFetch)
            return HandleAtomicBackendDAT(transaction, flit);
        const bool localAtomicRead = (options.backend != BackendKind::CHI || flow.atomic.localExecution) &&
            IsAtomicRequest(transaction.request.opcode);
        if (!transaction.memoryRequired || flow.memoryWrite ||
            (!IsReadRequest(transaction.request.opcode) && !flow.baselineFetch && !localAtomicRead))
            return false;
        if (flow.separatedBackendRead)
        {
            if (flit.opcode != CHI::Eb::Opcodes::DAT::DataSepResp)
                return false;
        }
        else if (flit.opcode != CHI::Eb::Opcodes::DAT::CompData)
            return false;

        const size_t bytes = BytesPerBeat(options);
        const size_t offset = flit.ByteOffset();
        if (offset + bytes > LINE_SIZE || offset % bytes != 0)
            return false;
        const size_t beat = offset / bytes;
        const uint64_t bit = uint64_t(1) << beat;
        const uint64_t complete = flow.baselineFetch
            ? (uint64_t(1) << BeatsPerLine(options)) - 1
            : flow.backendExpectedBeatMask ? flow.backendExpectedBeatMask
            : flow.expectedBeatMask ? flow.expectedBeatMask : (uint64_t(1) << BeatsPerLine(options)) - 1;
        const uint64_t backendAddress = flow.baselineFetch ? flow.baselineLine.address
            : options.backend == BackendKind::CHI ? flow.backendRequest.address : transaction.request.address;
        uint64_t& received = flow.baselineFetch || localAtomicRead ? flow.backendBeatMask : transaction.receivedBeats;
        if (!(complete & bit) || (received & bit) || flit.ccid != ((backendAddress >> 4) & 3) ||
            (flow.backendReadRespValid && flow.backendReadResp != flit.resp))
            return false;
        flow.backendReadResp = flit.resp;
        flow.backendReadRespValid = true;

        const bool victimBaseline = flow.baselineFetch && flow.victimPending && flow.victimDataPartial;
        const auto requestClass = ClassifyRequest(transaction.request.opcode);
        const bool immediateBaseline = requestClass == RequestClass::ImmediateWrite || requestClass == RequestClass::WriteUniqueZero;
        LineData& destination = victimBaseline ? flow.victimData : transaction.data;
        ByteEnables enables{};
        for (size_t index = 0; index < bytes; ++index)
        {
            const bool preserve = flow.baselineFetch &&
                (victimBaseline ? flow.victimValidBytes[offset + index]
                                : immediateBaseline ? flow.validBytes[offset + index]
                                : flow.snoopDataPartial ? flow.snoopValidBytes[offset + index]
                                : IsCopybackRequest(transaction.request.opcode) && transaction.byteEnables[offset + index]);
            if (!preserve)
            {
                destination[offset + index] = flit.bytes[index];
                enables[offset + index] = 1;
            }
            if (!flow.baselineFetch || !IsCopybackRequest(transaction.request.opcode))
                transaction.byteEnables[offset + index] = 1;
            if (flow.baselineFetch)
            {
                if (victimBaseline)
                    flow.victimValidBytes[offset + index] = 1;
                else
                    flow.validBytes[offset + index] = 1;
            }
        }
        LinePoison& poison = victimBaseline ? flow.victimPoison : transaction.poison;
        LineDataCheck& dataCheck = victimBaseline ? flow.victimDataCheck : transaction.dataCheck;
        MergeMetadata(poison, dataCheck, flit.poison, flit.dataCheck,
                      enables, offset, 0, bytes, options);
        flow.backendReadRespErr[beat] = MergeResponseErrors(flit.respErr,
            HasUnsupportedMetadataError(flit, enables, offset, 0, bytes, options)
                ? uint8_t(CHI::Eb::RespErrs::DERR) : uint8_t(CHI::Eb::RespErrs::OK));
        flow.backendBeatRespErr[beat] = flow.backendReadRespErr[beat];
        flow.backendReadRespErrMask |= bit;
        if (transaction.responseRespErr == CHI::Eb::RespErrs::OK ||
            flow.backendReadRespErr[beat] == CHI::Eb::RespErrs::NDERR)
            transaction.responseRespErr = flow.backendReadRespErr[beat];
        received |= bit;

        if ((received & complete) != complete)
            return true;

        transaction.Mark(COMPLETE_MEMORY);
        transaction.memoryRequired = false;
        transaction.wait = WaitReason::None;
        CompleteBackendOperation(transaction);
        if (flow.baselineFetch)
        {
            flow.baselineFetched = true;
            flow.baselineFetch = false;
        }
        if (flow.backendRequest.order == 0 || flow.backendReadReceiptReceived)
            tracker.UnbindMemory(options.subordinateId, transaction.handle);
        transaction.phase = Phase::Action;
        transaction.readyTime = ScheduleData(transaction);
        return true;
    }

    bool Core::Impl::WriteLine(Transaction& transaction, const LineKey& line, const LineData& data, const ByteEnables& enables)
    {
        LineImage image = MakeLineImage(transaction, options);
        image.data = data;
        return WriteLineImage(transaction, line, image, enables);
    }

    bool Core::Impl::WriteLineImage(Transaction& transaction, const LineKey& line, const LineImage& image,
                                    const ByteEnables& enables, bool generatedEviction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (options.backend == BackendKind::Internal)
        {
            memory.WriteImage(line, image, enables, options);
            ReleaseRefill(transaction, line);
            return true;
        }

        if (flow.spillPending)
        {
            if (flow.backendPhase == BackendPhase::None)
            {
                StartBackendWrite(transaction);
                return false;
            }
            if (flow.backendPhase != BackendPhase::Done)
                return false;
            LineImage spill;
            spill.data = flow.spillData;
            spill.poison = flow.spillPoison;
            spill.dataCheck = flow.spillDataCheck;
            if (!transaction.responseRespErr)
                memory.WriteImage(flow.spillLine, spill, flow.spillEnables, options);
            ReleaseRefill(transaction, flow.spillLine);
            flow.spillPending = false;
            flow.spillGeneratedEviction = false;
            flow.backendPhase = BackendPhase::None;
            return true;
        }

        flow.spillPending = true;
        flow.spillGeneratedEviction = generatedEviction;
        flow.spillLine = line;
        flow.spillData = image.data;
        flow.spillPoison = image.poison;
        flow.spillDataCheck = image.dataCheck;
        flow.spillEnables = enables;
        StartBackendWrite(transaction);
        return false;
    }

    bool Core::Impl::FetchBaseline(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (options.backend == BackendKind::Internal)
            return true;

        const LineKey requested = flow.victimPending && flow.victimDataPartial ? flow.victimLine : transaction.request.Line();
        if (flow.baselineFetched && flow.baselineLine == requested)
            return true;
        if (flow.baselineFetch && flow.backendPhase == BackendPhase::Done && flow.baselineLine == requested)
        {
            flow.baselineFetched = true;
            return true;
        }
        flow.baselineFetch = true;
        flow.baselineLine = requested;
        flow.baselineFetched = false;
        StartMemoryRead(transaction);
        return false;
    }

}
