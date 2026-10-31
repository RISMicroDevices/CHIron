#include "chi_lunatic_core.hpp"

#include <algorithm>

namespace CHI::Lunatic {

    uint16_t Core::Impl::SnoopOpcode(const Transaction& transaction, const FlowState& flow, NodeId target) const
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        namespace SNP = CHI::Eb::Opcodes::SNP;
        if (flow.snoopForVictim)
            return SNP::SnpCleanInvalid;
        if (flow.dctActive && flow.nextSnoop == 0)
        {
            switch (transaction.request.opcode)
            {
                case REQ::ReadShared: return SNP::SnpSharedFwd;
                case REQ::ReadClean: return SNP::SnpCleanFwd;
                case REQ::ReadOnce: return SNP::SnpOnceFwd;
                case REQ::ReadNotSharedDirty: return SNP::SnpNotSharedDirtyFwd;
                case REQ::ReadPreferUnique: return SNP::SnpPreferUniqueFwd;
                default: return SNP::SnpUniqueFwd;
            }
        }
        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::CoherentRead:
                if (transaction.request.opcode == REQ::ReadUnique)
                    return SNP::SnpUnique;
                if (transaction.request.opcode == REQ::ReadClean)
                    return SNP::SnpClean;
                if (transaction.request.opcode == REQ::ReadNotSharedDirty)
                    return SNP::SnpNotSharedDirty;
                if (transaction.request.opcode == REQ::ReadPreferUnique)
                    return SNP::SnpPreferUnique;
                return SNP::SnpShared;
            case RequestClass::ReadOnce:
                if (transaction.request.opcode == REQ::ReadOnceCleanInvalid ||
                    transaction.request.opcode == REQ::ReadOnceMakeInvalid)
                    return SNP::SnpUnique;
                return SNP::SnpOnce;
            case RequestClass::MakeUnique: return SNP::SnpUnique;
            case RequestClass::MakeReadUnique: return SNP::SnpCleanInvalid;
            case RequestClass::ImmediateWrite:
                if (target == transaction.request.returnNid &&
                    transaction.request.opcode == REQ::WriteUniquePtlStash)
                    return SNP::SnpUniqueStash;
                if (target == transaction.request.returnNid &&
                    transaction.request.opcode == REQ::WriteUniqueFullStash)
                    return SNP::SnpMakeInvalidStash;
                return SNP::SnpUnique;
            case RequestClass::WriteUniqueZero: return SNP::SnpUnique;
            case RequestClass::CleanUnique: return SNP::SnpUnique;
            case RequestClass::Atomic: return SNP::SnpUnique;
            case RequestClass::Persistence: return SNP::SnpCleanShared;
            case RequestClass::CMO:
                if (transaction.request.opcode == REQ::CleanShared)
                    return SNP::SnpCleanShared;
                if (transaction.request.opcode == REQ::CleanInvalid)
                    return SNP::SnpCleanInvalid;
                return SNP::SnpMakeInvalid;
            case RequestClass::Stash:
                if (target != transaction.request.returnNid)
                    return SNP::SnpUnique;
                return transaction.request.opcode == REQ::StashOnceUnique ||
                    transaction.request.opcode == REQ::StashOnceSepUnique ? SNP::SnpStashUnique : SNP::SnpStashShared;
            default: return SNP::SnpUnique;
        }
    }

    void Core::Impl::PlanReadSnoops(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!transaction.request.snpAttr &&
            (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
             transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid))
        {
            if (transaction.homeHit)
                pipeline.Advance(transaction, Phase::Action, time);
            else
                StartMemoryRead(transaction);
            return;
        }
        std::vector<NodeId> candidates;
        for (NodeId target : snoopFilter.Targets(transaction.request.Line()))
        {
            if (target == transaction.request.source)
                continue;
            const CacheState state = snoopFilter.State(transaction.request.Line(), target);
            const auto requestClass = ClassifyRequest(transaction.request.opcode);
            const bool needsUnique = transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadUnique ||
                                     transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
                                     transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid ||
                                     requestClass == RequestClass::MakeUnique || requestClass == RequestClass::MakeReadUnique;
            const bool needsDowngrade = IsUnique(state) &&
                (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadShared ||
                 transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadClean ||
                 transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadNotSharedDirty ||
                 transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadPreferUnique);
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

    void Core::Impl::PlanCMOSnoops(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        for (NodeId target : snoopFilter.Targets(transaction.request.Line()))
        {
            if (target == transaction.request.source)
                continue;
            const CacheState state = snoopFilter.State(transaction.request.Line(), target);
            const bool cleanShared = transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanShared ||
                ClassifyRequest(transaction.request.opcode) == RequestClass::Persistence;
            if (!cleanShared || IsDirty(state) || !options.preciseSnoopFilter)
                transaction.snoopTargets.push_back(target);
        }
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
    }

    bool Core::Impl::EmitSnoops(Transaction& transaction)
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
        if (flow.dctActive && index == 0)
        {
            snoop.txnId = transaction.handle;
            snoop.fwdNid = transaction.request.source;
            snoop.fwdTxnId = transaction.request.txnId;
        }
        snoop.address = flow.snoopLine.address;
        snoop.nonSecure = flow.snoopLine.nonSecure;
        snoop.opcode = SnoopOpcode(transaction, flow, target);
        snoop.qos = transaction.request.qos;
        snoop.traceTag = transaction.request.traceTag;
        snoop.stashLpid = IsStashRequest(transaction.request.opcode) ? transaction.request.stashLpid : 0;
        snoop.stashLpidValid = IsStashRequest(transaction.request.opcode) && transaction.request.stashLpidValid;
        snoop.doNotGoToSD = options.doNotGoToSD ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUnique ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpCleanShared ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpCleanInvalid ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalid ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUniqueStash ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        snoop.retToSrc = !IsCMORequest(transaction.request.opcode) && !IsAtomicRequest(transaction.request.opcode) &&
            (snoop.opcode == CHI::Eb::Opcodes::SNP::SnpShared ||
             snoop.opcode == CHI::Eb::Opcodes::SNP::SnpOnce || snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUnique);

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

    void Core::Impl::ProcessSnoopRSP(Transaction& transaction, const Response& flit)
    {
        if (!ValidateSnoopResponse(transaction, flit.source, flit.resp, false, false, flit.fwdState))
            Error(transaction.handle, "illegal snoop response");
        CompleteSnoopTarget(transaction, flit.source, flit.resp, false);
    }

    void Core::Impl::ProcessSnoopDAT(Transaction& transaction, const Data& flit)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataFwded)
        {
            if (!flow.dctActive ||
                std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), flit.source) ==
                    transaction.pendingSnoops.end())
            {
                Error(transaction.handle, "unexpected forwarding snoop data");
                return;
            }
            flow.dctForwarded = true;
        }
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
        if (flit.opcode != CHI::Eb::Opcodes::DAT::SnpRespDataFwded &&
            !ValidateSnoopResponse(transaction, flit.source, flit.resp, true,
                flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataPtl, flit.dataSource))
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
        else if (ClassifyRequest(transaction.request.opcode) == RequestClass::ImmediateWrite)
        {
            for (size_t index = 0; index < bytes; ++index)
                flow.validBytes[offset + index] = 1;
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
            LinePoison& poison = flow.snoopForVictim ? flow.victimPoison : transaction.poison;
            LineDataCheck& dataCheck = flow.snoopForVictim ? flow.victimDataCheck : transaction.dataCheck;
            const ByteEnables enables = ExpandedEnables(flit.byteEnables, offset, bytes);
            MergeMetadata(poison, dataCheck, flit.poison, flit.dataCheck, enables, offset, 0, bytes, options);
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

    bool Core::Impl::ValidateSnoopResponse(Transaction& transaction, NodeId target, uint8_t resp,
                                           bool withData, bool partial, uint8_t dataPull) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        if (std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target) == transaction.pendingSnoops.end())
            return false;
        if (dataPull != 0)
            return false;
        const CacheState initial = snoopFilter.State(flow.snoopLine, target);
        const uint16_t opcode = SnoopOpcode(transaction, flow, target);
        const CacheState final = DecodeSnoopState(transaction, target, resp, partial);
        const bool passDirty = (resp & CHI::Eb::Resps::PassDirty) != 0;
        const bool stashRead = opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
            opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        if (stashRead)
        {
            if (withData || partial || passDirty)
                return false;
            if (resp == CHI::Eb::Resps::SnpResp_I)
                return true;
            switch (initial)
            {
                case CacheState::UC:
                case CacheState::UCE: return resp == CHI::Eb::Resps::SnpResp_UC;
                case CacheState::UD:
                case CacheState::UDP: return resp == CHI::Eb::Resps::SnpResp_UD;
                case CacheState::SC: return resp == CHI::Eb::Resps::SnpResp_SC;
                case CacheState::SD: return resp == CHI::Eb::Resps::SnpResp_SD;
                default: return false;
            }
        }
        if (passDirty && !IsDirty(initial))
            return false;
        if (opcode == CHI::Eb::Opcodes::SNP::SnpCleanShared)
        {
            if (IsDirty(final) || (initial == CacheState::UCE && final != CacheState::I))
                return false;
            return !IsDirty(initial) || (withData && passDirty);
        }
        if (options.doNotGoToSD && final == CacheState::SD && opcode != CHI::Eb::Opcodes::SNP::SnpOnce)
            return false;
        if (opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalid ||
            opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash)
            return !withData && resp == CHI::Eb::Resps::SnpResp_I;
        if (opcode == CHI::Eb::Opcodes::SNP::SnpUniqueStash)
        {
            if (final != CacheState::I)
                return false;
            switch (initial)
            {
                case CacheState::I:
                case CacheState::UCE: return !withData;
                case CacheState::UC:
                case CacheState::SC: return !partial && !passDirty;
                case CacheState::UD:
                case CacheState::SD: return withData && !partial && passDirty;
                case CacheState::UDP: return withData && partial && passDirty;
                default: return false;
            }
        }
        const bool invalidateWithData = opcode == CHI::Eb::Opcodes::SNP::SnpUnique ||
            opcode == CHI::Eb::Opcodes::SNP::SnpCleanInvalid;
        if (invalidateWithData && final != CacheState::I)
            return false;
        if (IsDirty(initial) && invalidateWithData)
            return withData && passDirty;
        if (IsDirty(initial) && opcode != CHI::Eb::Opcodes::SNP::SnpMakeInvalid &&
            opcode != CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash && !withData)
            return false;
        return true;
    }

    CacheState Core::Impl::DecodeSnoopState(Transaction& transaction, NodeId target, uint8_t resp, bool partial) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        const CacheState initial = snoopFilter.State(flow.snoopLine, target);
        const uint16_t opcode = SnoopOpcode(transaction, flow, target);
        if (opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique || opcode == CHI::Eb::Opcodes::SNP::SnpStashShared)
            return initial;
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

    void Core::Impl::CompleteSnoopTarget(Transaction& transaction, NodeId target, uint8_t resp, bool, bool partial)
    {
        FlowState& flow = flows.at(transaction.handle);
        const CacheState final = DecodeSnoopState(transaction, target, resp, partial);
        snoopFilter.Track(flow.snoopLine, target, final, time);
        auto it = std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target);
        if (it != transaction.pendingSnoops.end())
            transaction.pendingSnoops.erase(it);
        if (transaction.pendingSnoops.empty())
        {
            if (flow.dctForwarded)
                FinishResponse(transaction);
            else
            {
                transaction.phase = Phase::Action;
                transaction.wait = WaitReason::None;
                transaction.readyTime = time;
            }
        }
    }

}
