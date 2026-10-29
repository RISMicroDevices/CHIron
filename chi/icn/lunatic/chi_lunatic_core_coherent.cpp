#include "chi_lunatic_core.hpp"

#include <algorithm>

namespace CHI::Lunatic {
    namespace {

        uint64_t                        CopybackMask(const Request& request, const Options& options)
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

        uint16_t                        SnoopOpcode(const Transaction& transaction, const FlowState& flow)
        {
            namespace REQ = CHI::Eb::Opcodes::REQ;
            namespace SNP = CHI::Eb::Opcodes::SNP;
            if (flow.snoopForVictim)
                return SNP::SnpCleanInvalid;
            switch (ClassifyRequest(transaction.request.opcode))
            {
                case RequestClass::CoherentRead:
                    return transaction.request.opcode == REQ::ReadUnique ? SNP::SnpUnique : SNP::SnpShared;
                case RequestClass::ReadOnce: return SNP::SnpOnce;
                case RequestClass::MakeUnique: return SNP::SnpUnique;
                case RequestClass::CMO:
                    if (transaction.request.opcode == REQ::CleanShared)
                        return SNP::SnpCleanShared;
                    if (transaction.request.opcode == REQ::CleanInvalid)
                        return SNP::SnpCleanInvalid;
                    return SNP::SnpMakeInvalid;
                default: return SNP::SnpUnique;
            }
        }

    }

    void                                Core::Impl::ResolveCoherent(Transaction& transaction)
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

    void                                Core::Impl::ActionCoherent(Transaction& transaction)
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
                else if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadShared)
                {
                    transaction.responseResp = flow.requesterFinal == CacheState::UD || flow.requesterFinal == CacheState::UDP
                        ? CHI::Eb::Resps::CompData_UD_PD
                        : flow.requesterFinal == CacheState::SD
                            ? CHI::Eb::Resps::CompData_SD_PD : CHI::Eb::Resps::CompData_SC;
                }
                else
                    transaction.responseResp = CHI::Eb::Resps::CompData_UC;
                transaction.sentBeats = 0;
                transaction.phase = Phase::Return;
                transaction.readyTime = time;
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

    void                                Core::Impl::PlanReadSnoops(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        std::vector<NodeId> candidates;
        for (NodeId target : snoopFilter.Targets(transaction.request.Line()))
        {
            if (target == transaction.request.source)
                continue;
            const CacheState state = snoopFilter.State(transaction.request.Line(), target);
            const bool needsUnique = transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadUnique ||
                                     ClassifyRequest(transaction.request.opcode) == RequestClass::MakeUnique;
            const bool needsDowngrade = transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadShared && IsUnique(state);
            if (needsUnique || needsDowngrade || IsDirty(state))
                candidates.push_back(target);
        }

        if (candidates.empty() && !transaction.homeHit &&
            ClassifyRequest(transaction.request.opcode) != RequestClass::MakeUnique)
        {
            for (NodeId target : snoopFilter.Targets(transaction.request.Line()))
            {
                if (target != transaction.request.source)
                {
                    candidates.push_back(target);
                    break;
                }
            }
        }

        if (candidates.empty())
        {
            if (ClassifyRequest(transaction.request.opcode) == RequestClass::MakeUnique)
                pipeline.Advance(transaction, Phase::Action, time);
            else
                StartMemoryRead(transaction);
            return;
        }

        transaction.snoopTargets = candidates;
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = true;
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
    }

    void                                Core::Impl::PlanCMOSnoops(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        for (NodeId target : snoopFilter.Targets(transaction.request.Line()))
        {
            if (target == transaction.request.source)
                continue;
            const CacheState state = snoopFilter.State(transaction.request.Line(), target);
            if (transaction.request.opcode != CHI::Eb::Opcodes::REQ::CleanShared || IsDirty(state))
                transaction.snoopTargets.push_back(target);
        }
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
    }

    bool                                Core::Impl::EmitSnoops(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (transaction.pendingSnoops.size() >= options.snoopLimit)
        {
            transaction.phase = Phase::WaitSnoop;
            transaction.wait = WaitReason::Snoop;
            transaction.readyTime = time;
            return false;
        }
        if (flow.nextSnoop >= transaction.snoopTargets.size())
            return true;

        const size_t index = flow.nextSnoop;
        const NodeId target = transaction.snoopTargets[index];
        Snoop snoop;
        snoop.source = nodeId;
        snoop.target = target;
        snoop.txnId = transaction.handle * options.snoopLimit + (index % options.snoopLimit);
        snoop.address = flow.snoopLine.address;
        snoop.nonSecure = flow.snoopLine.nonSecure;
        snoop.opcode = SnoopOpcode(transaction, flow);
        snoop.qos = transaction.request.qos;
        snoop.traceTag = transaction.request.traceTag;
        snoop.doNotGoToSD = options.doNotGoToSD ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUnique ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpCleanShared ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpCleanInvalid ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalid;
        snoop.retToSrc = snoop.opcode == CHI::Eb::Opcodes::SNP::SnpShared ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpOnce || snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUnique;

        if (tracker.FindSnoop(target, snoop.txnId))
        {
            Error(transaction.handle, "snoop response ID is still live");
            return false;
        }
        if (!Queue(txsnp, snoop, transaction.handle, transaction.actionAge, index + 1 == transaction.snoopTargets.size()))
            return false;
        if (!tracker.BindSnoop(transaction.handle, target, snoop.txnId))
        {
            Error(transaction.handle, "unable to bind snoop response ID");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return false;
        }
        transaction.pendingSnoops.push_back(target);
        ++flow.nextSnoop;
        transaction.phase = transaction.pendingSnoops.size() >= options.snoopLimit || flow.nextSnoop == transaction.snoopTargets.size()
            ? Phase::WaitSnoop : Phase::Action;
        transaction.wait = transaction.phase == Phase::WaitSnoop ? WaitReason::Snoop : WaitReason::None;
        transaction.readyTime = time;
        return true;
    }

    void                                Core::Impl::ProcessSnoopRSP(Transaction& transaction, const Response& flit)
    {
        if (!ValidateSnoopResponse(transaction, flit.source, flit.resp, false))
            Error(transaction.handle, "illegal snoop response");
        CompleteSnoopTarget(transaction, flit.source, flit.resp, false);
    }

    void                                Core::Impl::ProcessSnoopDAT(Transaction& transaction, const Data& flit)
    {
        FlowState& flow = flows.at(transaction.handle);
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = flit.ByteOffset();
        if (offset + bytes > LINE_SIZE || offset % bytes != 0)
        {
            Error(transaction.handle, "invalid snoop data offset");
            return;
        }
        const size_t beat = offset / bytes;
        const uint64_t bit = uint64_t(1) << beat;
        uint64_t& received = flow.snoopBeatMasks[flit.source];
        if (received & bit)
        {
            Error(transaction.handle, "duplicate snoop data beat");
            return;
        }
        if (flow.snoopResponses.contains(flit.source) && flow.snoopResponses[flit.source] != flit.resp)
        {
            Error(transaction.handle, "inconsistent snoop response state");
            return;
        }
        flow.snoopResponses[flit.source] = flit.resp;
        if (!ValidateSnoopResponse(transaction, flit.source, flit.resp, true, flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataPtl))
            Error(transaction.handle, "illegal snoop data response");

        const CacheState initial = snoopFilter.State(flow.snoopLine, flit.source);
        const bool dirty = IsDirty(initial);
        if (flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataPtl)
        {
            if (flow.snoopForVictim)
            {
                flow.victimDataPartial = true;
                for (size_t index = 0; index < bytes; ++index)
                    flow.victimValidBytes[offset + index] = flit.byteEnables[index];
            }
            else
            {
                flow.snoopDataPartial = true;
                for (size_t index = 0; index < bytes; ++index)
                {
                    flow.snoopValidBytes[offset + index] = flit.byteEnables[index];
                    flow.validBytes[offset + index] = flit.byteEnables[index];
                }
            }
        }
        const bool replace = flow.snoopForVictim
            ? (!flow.victimDataValid || dirty || !flow.victimDirty)
            : (!flow.snoopDataValid || dirty || !flow.dataDirty);
        if (replace)
        {
            LineData& destination = flow.snoopForVictim ? flow.victimData : transaction.data;
            for (size_t index = 0; index < bytes; ++index)
            {
                if (flit.byteEnables[index])
                    destination[offset + index] = flit.bytes[index];
            }
        }
        received |= bit;
        if (flow.snoopForVictim)
        {
            flow.victimDataValid = true;
            flow.victimDirty |= dirty;
        }
        else
        {
            flow.snoopDataValid = true;
            flow.dataDirty |= dirty;
            flow.passDirty |= (flit.resp & CHI::Eb::Resps::PassDirty) != 0;
        }

        const uint64_t complete = (uint64_t(1) << BeatsPerLine(options)) - 1;
        if ((received & complete) == complete)
            CompleteSnoopTarget(transaction, flit.source, flit.resp, true, flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataPtl);
    }

    bool                                Core::Impl::ValidateSnoopResponse(Transaction& transaction, NodeId target, uint8_t resp, bool withData, bool partial) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        if (std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target) == transaction.pendingSnoops.end())
            return false;
        const CacheState initial = snoopFilter.State(flow.snoopLine, target);
        const uint16_t opcode = SnoopOpcode(transaction, flow);
        const CacheState final = DecodeSnoopState(transaction, target, resp, partial);
        const bool passDirty = (resp & CHI::Eb::Resps::PassDirty) != 0;
        if (passDirty && !IsDirty(initial))
            return false;
        if (options.doNotGoToSD && final == CacheState::SD && opcode != CHI::Eb::Opcodes::SNP::SnpOnce)
            return false;
        if (opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalid)
            return !withData && final == CacheState::I;
        if ((opcode == CHI::Eb::Opcodes::SNP::SnpUnique || opcode == CHI::Eb::Opcodes::SNP::SnpCleanInvalid) &&
            final != CacheState::I)
            return false;
        if (IsDirty(initial) && (opcode == CHI::Eb::Opcodes::SNP::SnpUnique || opcode == CHI::Eb::Opcodes::SNP::SnpCleanInvalid))
            return withData && passDirty;
        if (IsDirty(initial) && opcode != CHI::Eb::Opcodes::SNP::SnpMakeInvalid && !withData)
            return false;
        return true;
    }

    CacheState                          Core::Impl::DecodeSnoopState(Transaction& transaction, NodeId target, uint8_t resp, bool partial) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        const CacheState initial = snoopFilter.State(flow.snoopLine, target);
        const uint16_t opcode = SnoopOpcode(transaction, flow);
        switch (resp & 0b011)
        {
            case 0b000: return CacheState::I;
            case 0b001: return CacheState::SC;
            case 0b011: return CacheState::SD;
            default:
                if (partial)
                    return CacheState::UDP;
                if (IsDirty(initial) && !(resp & CHI::Eb::Resps::PassDirty) &&
                    opcode != CHI::Eb::Opcodes::SNP::SnpCleanShared && opcode != CHI::Eb::Opcodes::SNP::SnpCleanInvalid)
                    return CacheState::UD;
                return CacheState::UC;
        }
    }

    void                                Core::Impl::CompleteSnoopTarget(Transaction& transaction, NodeId target, uint8_t resp, bool, bool partial)
    {
        FlowState& flow = flows.at(transaction.handle);
        const CacheState final = DecodeSnoopState(transaction, target, resp, partial);
        snoopFilter.Track(flow.snoopLine, target, final, time);
        auto it = std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target);
        if (it != transaction.pendingSnoops.end())
            transaction.pendingSnoops.erase(it);
        if (transaction.pendingSnoops.empty())
        {
            transaction.phase = Phase::Action;
            transaction.wait = WaitReason::None;
            transaction.readyTime = time;
        }
    }

}
