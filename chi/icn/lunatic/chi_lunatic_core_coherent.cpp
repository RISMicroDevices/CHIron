#include "chi_lunatic_core.hpp"

#include <algorithm>

namespace CHI::Lunatic {
    namespace {

        uint64_t CopybackMask(const Request& request, const Options& options)
        {
            const size_t bytes = BytesPerBeat(options);
            const size_t transfer = std::min<size_t>(LINE_SIZE, size_t(1) << request.size);
            const size_t first = (request.address % LINE_SIZE) / bytes;
            const size_t count = std::max<size_t>(1, (transfer + bytes - 1) / bytes);
            uint64_t mask = 0;
            for (size_t beat = 0; beat < count && first + beat < BeatsPerLine(options); ++beat)
                mask |= uint64_t(1) << (first + beat);
            return mask;
        }

    }

    void Core::Impl::ResolveCoherent(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        flow.snoopLine = transaction.request.Line();
        if (IsReadRequest(transaction.request.opcode))
            flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::ReadOnce:
            case RequestClass::CoherentRead:
            case RequestClass::MakeUnique:
            case RequestClass::MakeReadUnique:
                PlanReadSnoops(transaction);
                return;
            case RequestClass::CopybackWrite:
                flow.expectedBeatMask = CopybackMask(transaction.request, options);
                pipeline.Advance(transaction, Phase::Action, time);
                return;
            case RequestClass::Evict:
                pipeline.Advance(transaction, Phase::Action, time);
                return;
            case RequestClass::CMO:
                PlanCMOSnoops(transaction);
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
            AdvanceBackendWrite(transaction);
            return;
        }

        if (flow.victimPending)
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
            if (flow.victimLinked && !hazards.IsHead(transaction.handle, flow.victimLine))
            {
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
                    request.allowRetry = false;
                    request.expCompAck = false;
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
