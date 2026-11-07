#include "chi_lunatic_core.hpp"

#include <algorithm>

namespace CHI::Lunatic {
    namespace {

        uint64_t CopybackMask(const Request& request, const Options& options)
        {
            return TransferBeatMask(request, options);
        }

    }

    void Core::Impl::ResolveCoherent(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        flow.snoopLine = transaction.request.Line();
        if (IsReadRequest(transaction.request.opcode))
        {
            flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
            if (transaction.request.order != 0 && !flow.separatedResponse && !flow.readReceiptSent)
            {
                QueueReadReceipt(transaction);
                if (!flow.readReceiptSent)
                {
                    transaction.readyTime = time;
                    return;
                }
            }
        }
        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::ReadOnce:
            case RequestClass::CoherentRead:
            case RequestClass::MakeUnique:
            case RequestClass::MakeReadUnique:
                PlanReadSnoops(transaction);
                return;
            case RequestClass::ImmediateWrite:
            case RequestClass::WriteUniqueZero:
                ResolveImmediateWrite(transaction);
                return;
            case RequestClass::WriteEvictOrEvict:
            case RequestClass::CopybackWrite:
                flow.expectedBeatMask = CopybackMask(transaction.request, options);
                pipeline.Advance(transaction, Phase::Action, time);
                return;
            case RequestClass::Evict:
                pipeline.Advance(transaction, Phase::Action, time);
                return;
            case RequestClass::CMO:
            case RequestClass::CleanUnique:
            case RequestClass::Persistence:
                PlanCMOSnoops(transaction);
                return;
            case RequestClass::Stash:
                ResolveStash(transaction);
                return;
            case RequestClass::Atomic:
                ResolveAtomic(transaction);
                return;
            default:
                Error(transaction.handle, "invalid coherent request class");
                transaction.canceled = true;
                transaction.phase = Phase::Commit;
                transaction.readyTime = time;
                return;
        }
    }

    void Core::Impl::ActionCoherent(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.backendPhase == BackendPhase::SendData)
        {
            if (IsAtomicRequest(transaction.request.opcode) && !flow.spillPending)
                AdvanceAtomicBackendDAT(transaction);
            else
                AdvanceBackendWrite(transaction);
            return;
        }

        if (flow.victimPending)
        {
            if (flow.victimLinked && !hazards.IsHead(transaction.handle, flow.victimLine))
            {
                transaction.readyTime = time;
                return;
            }
            if (flow.nextSnoop < transaction.snoopTargets.size())
            {
                EmitSnoops(transaction);
                return;
            }
            if (!transaction.pendingSnoops.empty())
            {
                transaction.phase = Phase::WaitSnoop;
                transaction.readyTime = time;
                return;
            }
            if (!CompleteVictim(transaction))
            {
                transaction.phase = Phase::Action;
                transaction.readyTime = time;
                return;
            }
        }

        if (transaction.snoopsRequired)
        {
            if (flow.nextSnoop < transaction.snoopTargets.size())
            {
                EmitSnoops(transaction);
                return;
            }
            if (!transaction.pendingSnoops.empty())
            {
                transaction.phase = Phase::WaitSnoop;
                transaction.readyTime = time;
                return;
            }
            transaction.snoopsRequired = false;
            transaction.snoopTargets.clear();
            flow.nextSnoop = 0;
            flow.snoopBeatMasks.clear();
            flow.snoopResponses.clear();
        }

        if (flow.dctForwarded)
        {
            if (!PrepareHomeCommit(transaction))
            {
                transaction.phase = Phase::Action;
                transaction.readyTime = time;
                return;
            }
            FinishResponse(transaction);
            return;
        }

        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::ReadOnce:
            case RequestClass::CoherentRead:
                if (!transaction.Complete(COMPLETE_MEMORY) && !transaction.homeHit && (!flow.snoopDataValid || flow.snoopDataPartial))
                {
                    StartMemoryRead(transaction);
                    return;
                }
                if (!PrepareHomeCommit(transaction))
                {
                    transaction.phase = Phase::Action;
                    transaction.readyTime = time;
                    return;
                }
                if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadUnique)
                    transaction.responseResp = flow.requesterFinal == CacheState::UD || flow.requesterFinal == CacheState::UDP
                        ? CHI::Eb::Resps::CompData_UD_PD : CHI::Eb::Resps::CompData_UC;
                else if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadNotSharedDirty ||
                         transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadPreferUnique ||
                         transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadClean)
                    transaction.responseResp = CHI::Eb::Resps::CompData_SC;
                else if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadShared)
                {
                    transaction.responseResp = flow.requesterFinal == CacheState::UD || flow.requesterFinal == CacheState::UDP
                        ? CHI::Eb::Resps::CompData_UD_PD
                        : flow.requesterFinal == CacheState::SD
                            ? CHI::Eb::Resps::CompData_SD_PD : CHI::Eb::Resps::CompData_SC;
                }
                else if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
                         transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid)
                    transaction.responseResp = CHI::Eb::Resps::CompData_I;
                else
                    transaction.responseResp = CHI::Eb::Resps::CompData_UC;
                if (flow.separatedResponse)
                    transaction.responseResp = CHI::Eb::Resps::DataSepResp_UC;
                transaction.sentBeats = 0;
                transaction.phase = Phase::Return;
                transaction.readyTime = time;
                return;
            case RequestClass::MakeReadUnique:
            {
                const CacheState responseState = snoopFilter.State(transaction.request.Line(), transaction.request.source);
                if (!transaction.Complete(COMPLETE_MEMORY) && !transaction.homeHit && !IsValid(responseState) &&
                    (!flow.snoopDataValid || flow.snoopDataPartial))
                {
                    StartMemoryRead(transaction);
                    return;
                }
                if (!PrepareHomeCommit(transaction))
                {
                    transaction.phase = Phase::Action;
                    transaction.readyTime = time;
                    return;
                }
                if (responseState == CacheState::SC || responseState == CacheState::SD)
                {
                    QueueComp(transaction, false,
                        responseState == CacheState::SD ? CHI::Eb::Resps::Comp_UD_PD : CHI::Eb::Resps::Comp_UC);
                    return;
                }
                transaction.responseResp = flow.requesterFinal == CacheState::UD || flow.requesterFinal == CacheState::UDP
                    ? CHI::Eb::Resps::CompData_UD_PD : CHI::Eb::Resps::CompData_UC;
                transaction.sentBeats = 0;
                transaction.phase = Phase::Return;
                transaction.readyTime = time;
                return;
            }
            case RequestClass::ImmediateWrite:
            case RequestClass::WriteUniqueZero:
                ActionImmediateWrite(transaction);
                return;
            case RequestClass::WriteEvictOrEvict:
                ActionEvictOrEvict(transaction);
                return;
            case RequestClass::Stash:
                ActionStash(transaction);
                return;
            case RequestClass::Atomic:
                ActionAtomic(transaction);
                return;
            case RequestClass::MakeUnique:
                if (!PrepareHomeCommit(transaction))
                {
                    transaction.phase = Phase::Action;
                    transaction.readyTime = time;
                    return;
                }
                QueueComp(transaction, false, CHI::Eb::Resps::Comp_UC);
                return;
            case RequestClass::CopybackWrite:
                if (flow.suppressArchitecturalEffects)
                {
                    if (!flow.dbidSent)
                    {
                        QueueComp(transaction, true, CHI::Eb::Resps::Comp_I, CHI::Eb::RespErrs::NDERR);
                        flow.dbidSent = flow.compSent;
                        return;
                    }
                    if (!transaction.Complete(COMPLETE_WRITE_DATA))
                    {
                        transaction.phase = Phase::WaitWriteData;
                        transaction.readyTime = time;
                        return;
                    }
                    flow.directoryCommitted = true;
                    transaction.Mark(COMPLETE_COMMIT);
                    transaction.phase = Phase::Commit;
                    transaction.readyTime = time;
                    return;
                }
                if (!flow.dbidSent)
                {
                    QueueComp(transaction, true);
                    flow.dbidSent = flow.compSent;
                    return;
                }
                if (!transaction.Complete(COMPLETE_WRITE_DATA))
                {
                    transaction.phase = Phase::WaitWriteData;
                    transaction.readyTime = time;
                    return;
                }
                if (!PrepareHomeCommit(transaction))
                {
                    transaction.phase = Phase::Action;
                    transaction.readyTime = time;
                    return;
                }
                transaction.Mark(COMPLETE_COMMIT);
                transaction.phase = Phase::Commit;
                transaction.readyTime = time;
                return;
            case RequestClass::Evict:
                EvictRequester(transaction);
                QueueComp(transaction, false, CHI::Eb::Resps::Comp_I);
                return;
            case RequestClass::CleanUnique:
                if (snoopFilter.State(transaction.request.Line(), transaction.request.source) == CacheState::UDP)
                {
                    Error(transaction.handle, "CleanUnique requester UDP is unsupported");
                    transaction.canceled = true;
                    QueueComp(transaction, false, CHI::Eb::Resps::Comp_UC, 0b10);
                    return;
                }
                if (!PrepareHomeCommit(transaction))
                {
                    transaction.phase = Phase::Action;
                    transaction.readyTime = time;
                    return;
                }
                QueueComp(transaction, false, CHI::Eb::Resps::Comp_UC);
                return;
            case RequestClass::Persistence:
                ActionPersistence(transaction);
                return;
            case RequestClass::CMO:
                if (!PrepareHomeCommit(transaction))
                {
                    transaction.phase = Phase::Action;
                    transaction.readyTime = time;
                    return;
                }
                if (options.backend == BackendKind::CHI && flow.backendPhase == BackendPhase::None)
                {
                    Request request = transaction.request;
                    request.source = nodeId;
                    request.target = options.subordinateId;
                    request.txnId = transaction.handle;
                    request.returnNid = 0;
                    request.returnTxnId = 0;
                    request.allowRetry = true;
                    request.expCompAck = false;
                    request.snpAttr = 0;
                    request.doDwt = false;
                    request.stashNidValid = false;
                    request.stashLpidValid = false;
                    if (!Queue(txreq, request, transaction.handle, transaction.actionAge, false, true))
                    {
                        transaction.readyTime = time;
                        return;
                    }
                    tracker.BindMemory(transaction.handle, options.subordinateId, transaction.handle);
                    flow.backendPhase = BackendPhase::WaitRequestEmission;
                    transaction.phase = Phase::WaitMemory;
                    transaction.readyTime = time;
                    return;
                }
                if (options.backend == BackendKind::CHI && flow.backendPhase != BackendPhase::Done)
                    return;
                QueueComp(transaction, false, flow.requesterFinal == CacheState::I ? CHI::Eb::Resps::Comp_I
                    : flow.requesterFinal == CacheState::SC ? CHI::Eb::Resps::Comp_SC : CHI::Eb::Resps::Comp_UC);
                return;
            default:
                Error(transaction.handle, "invalid coherent action");
                transaction.canceled = true;
                transaction.phase = Phase::Commit;
                transaction.readyTime = time;
                return;
        }
    }

}
