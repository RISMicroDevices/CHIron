#include "chi_lunatic_core.hpp"

#include <algorithm>

namespace CHI::Lunatic {
    namespace {

        uint16_t ForwardingOpcode(uint16_t opcode) noexcept
        {
            namespace REQ = CHI::Eb::Opcodes::REQ;
            namespace SNP = CHI::Eb::Opcodes::SNP;
            switch (opcode)
            {
                case REQ::ReadShared: return SNP::SnpSharedFwd;
                case REQ::ReadClean: return SNP::SnpCleanFwd;
                case REQ::ReadOnce:
                case REQ::ReadOnceCleanInvalid: return SNP::SnpOnceFwd;
                case REQ::ReadNotSharedDirty: return SNP::SnpNotSharedDirtyFwd;
                case REQ::ReadPreferUnique: return SNP::SnpPreferUniqueFwd;
                case REQ::ReadUnique: return SNP::SnpUniqueFwd;
                default: return 0;
            }
        }

        std::optional<CacheState> DecodeForwardedState(uint8_t resp) noexcept
        {
            switch (resp)
            {
                case CHI::Eb::FwdStates::I: return CacheState::I;
                case CHI::Eb::FwdStates::SC: return CacheState::SC;
                case CHI::Eb::FwdStates::UC: return CacheState::UC;
                case CHI::Eb::FwdStates::UD_PD: return CacheState::UD;
                case CHI::Eb::FwdStates::SD_PD: return CacheState::SD;
                default: return std::nullopt;
            }
        }

    }

    bool Core::Impl::PrepareDCTId(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.dctSnoopIdValid)
            return true;

        const auto allocated = snoopIdAllocator.Allocate([&](TransactionId candidate) {
            return tracker.FindDBID(transaction.request.source, candidate) == nullptr;
        });
        if (!allocated)
        {
            pmu.RecordStall(WaitReason::Tracker);
            return false;
        }
        if (!tracker.RebindDBID(transaction.handle, *allocated))
        {
            snoopIdAllocator.Release(*allocated);
            Error(transaction.handle, "unable to rebind DCT acknowledgment ID");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            transaction.readyTime = time;
            return false;
        }
        if (flow.allocatedDBIDValid)
        {
            snoopIdAllocator.Release(flow.allocatedDBID);
            flow.allocatedDBIDValid = false;
        }
        flow.dctSnoopId = *allocated;
        flow.dctSnoopIdValid = true;
        return true;
    }

    uint16_t Core::Impl::SnoopOpcode(const Transaction& transaction, const FlowState& flow, NodeId target) const
    {
        namespace REQ = CHI::Eb::Opcodes::REQ;
        namespace SNP = CHI::Eb::Opcodes::SNP;
        if (flow.snoopForVictim)
            return SNP::SnpCleanInvalid;
        if (flow.dctActive && target == flow.dctTarget)
            return ForwardingOpcode(transaction.request.opcode);
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
                Bank(transaction).pipeline.Advance(transaction, Phase::Action, time);
            else
                StartMemoryRead(transaction);
            return;
        }
        std::vector<NodeId> candidates;
        for (NodeId target : Bank(flow.snoopLine).snoopFilter.Targets(transaction.request.Line()))
        {
            if (target == transaction.request.source)
                continue;
            const CacheState state = Bank(flow.snoopLine).snoopFilter.State(transaction.request.Line(), target);
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
            for (NodeId target : Bank(flow.snoopLine).snoopFilter.Targets(transaction.request.Line()))
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
                Bank(transaction).pipeline.Advance(transaction, Phase::Action, time);
            else
                StartMemoryRead(transaction);
            return;
        }

        flow.dctTarget = candidates.front();
        flow.retToSrcUsed = false;
        if (flow.dctActive)
        {
            const std::vector<NodeId> owners = Bank(flow.snoopLine).snoopFilter.Targets(transaction.request.Line());
            const bool soleProvider = options.preciseSnoopFilter && owners.size() == 1 &&
                owners.front() == flow.dctTarget;
            const bool uniqueProviderRequired = transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadUnique ||
                transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadPreferUnique;
            const bool inclusiveMiss = options.policy == PolicyKind::Inclusive && !transaction.homeHit &&
                ClassifyRequest(transaction.request.opcode) != RequestClass::ReadOnce;
            flow.dctActive = ForwardingOpcode(transaction.request.opcode) != 0 &&
                transaction.request.size == 6 && !flow.homeDirty && !inclusiveMiss &&
                (!uniqueProviderRequired || soleProvider);
            if (flow.dctActive)
            {
                if (options.strategy.providerSelection == ProviderSelectionKind::RoundRobin)
                {
                    size_t& next = providerNext[transaction.request.Line()];
                    flow.dctTarget = candidates[next % candidates.size()];
                    ++next;
                }
                SnoopForwardingEvent choice(time, nodeId, transaction,
                    flow.dctTarget, Bank(flow.snoopLine).snoopFilter.State(transaction.request.Line(), flow.dctTarget),
                    owners.size(), flow.homeDirty);
                if (!options.strategy.forwardDct)
                    choice.UseNonForwarding();
                const auto decision = events->Decide(std::move(choice));
                flow.dctActive = decision.IsForwarding();
            }
        }

        if (flow.dctActive && !PrepareDCTId(transaction))
        {
            transaction.readyTime = time;
            return;
        }
        transaction.snoopTargets = candidates;
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = true;
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        transaction.readyTime = ScheduleResult(transaction, SchedulePoint::SnoopDecision, time);
        if (options.strategy.snoopIssueAtResolve)
            EmitSnoops(transaction);
    }

    void Core::Impl::PlanCMOSnoops(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        for (NodeId target : Bank(flow.snoopLine).snoopFilter.Targets(transaction.request.Line()))
        {
            if (target == transaction.request.source)
                continue;
            const CacheState state = Bank(flow.snoopLine).snoopFilter.State(transaction.request.Line(), target);
            const bool cleanShared = transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanShared ||
                ClassifyRequest(transaction.request.opcode) == RequestClass::Persistence;
            if (!cleanShared || IsDirty(state) || !options.preciseSnoopFilter)
                transaction.snoopTargets.push_back(target);
        }
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        const bool writeRequired = options.strategy.cmoBackendWhenRequired &&
            transaction.request.opcode != CHI::Eb::Opcodes::REQ::MakeInvalid &&
            (flow.dataDirty || flow.homeDirty);
        transaction.readyTime = ScheduleResult(transaction, SchedulePoint::SnoopDecision,
            time + (writeRequired ? options.cmoWriteLatency : 0));
        if (options.strategy.cmoBackendWhenRequired && transaction.snoopTargets.empty() &&
            (transaction.request.opcode == CHI::Eb::Opcodes::REQ::MakeInvalid ||
             (!flow.dataDirty && !flow.homeDirty)))
            ActionCoherent(transaction);
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
        const bool forwarding = flow.dctActive && target == flow.dctTarget;
        TransactionId allocated = 0;
        if (forwarding)
        {
            if (!flow.dctSnoopIdValid)
            {
                Error(transaction.handle, "DCT snoop ID is not allocated");
                transaction.canceled = true;
                transaction.phase = Phase::Commit;
                transaction.readyTime = time;
                return false;
            }
            allocated = flow.dctSnoopId;
        }
        else
        {
            const auto id = snoopIdAllocator.Allocate();
            if (!id)
            {
                pmu.RecordStall(WaitReason::Tracker);
                transaction.readyTime = time;
                return false;
            }
            allocated = *id;
            flow.snoopIds[target] = allocated;
        }
        const auto releaseID = [&] {
            if (!forwarding)
            {
                snoopIdAllocator.Release(allocated);
                flow.snoopIds.erase(target);
            }
        };

        Snoop snoop;
        snoop.source = nodeId;
        snoop.target = target;
        snoop.txnId = allocated;
        if (forwarding)
        {
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
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUniqueFwd ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpCleanShared ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpCleanInvalid ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalid ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUniqueStash ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        snoop.retToSrc = !flow.retToSrcUsed && !IsCMORequest(transaction.request.opcode) &&
            !IsAtomicRequest(transaction.request.opcode) &&
            (snoop.opcode == CHI::Eb::Opcodes::SNP::SnpShared ||
             snoop.opcode == CHI::Eb::Opcodes::SNP::SnpOnce || snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUnique);

        if (tracker.FindSnoop(target, snoop.txnId))
        {
            releaseID();
            Error(transaction.handle, "snoop response ID is still live");
            return false;
        }
        BankState& bank = Bank(flow.snoopLine);
        if (!flow.snoopWaveOwned && options.structure.snoopResourceLimit &&
            bank.activeSnoopWaves >= options.structure.snoopResourceLimit)
        {
            releaseID();
            pmu.RecordStall(WaitReason::Snoop);
            return false;
        }
        if (options.structure.outgoingSnoopQueueLimit &&
            bank.outgoingSnoopQueue >= options.structure.outgoingSnoopQueueLimit)
        {
            releaseID();
            pmu.RecordStall(WaitReason::OutputQueue);
            return false;
        }
        if (!Queue(txsnp, snoop, transaction.handle, transaction.actionAge, index + 1 == transaction.snoopTargets.size()))
        {
            releaseID();
            return false;
        }
        ++bank.outgoingSnoopQueue;
        const bool newWave = !flow.snoopWaveOwned;
        if (newWave)
        {
            ++bank.activeSnoopWaves;
            flow.snoopWaveOwned = true;
        }
        if (snoop.retToSrc)
            flow.retToSrcUsed = true;
        if (!tracker.BindSnoop(transaction.handle, target, snoop.txnId))
        {
            releaseID();
            if (newWave)
            {
                --bank.activeSnoopWaves;
                flow.snoopWaveOwned = false;
            }
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
        if (flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataFwded &&
            !AcceptForwardedResponse(transaction, flit.source, flit.resp, flit.fwdState, true))
        {
            Error(transaction.handle, "unexpected or illegal forwarding snoop data");
            return;
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

        const CacheState initial = Bank(flow.snoopLine).snoopFilter.State(flow.snoopLine, flit.source);
        const bool dirty = IsDirty(initial) ||
            (flit.resp & CHI::Eb::Resps::PassDirty) != 0;
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

    bool Core::Impl::AcceptForwardedResponse(Transaction& transaction, NodeId target, uint8_t resp,
                                             uint8_t fwdState, bool withData)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.dctActive || target != flow.dctTarget ||
            std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target) ==
                transaction.pendingSnoops.end())
            return false;

        const std::optional<CacheState> requester = DecodeForwardedState(fwdState);
        if (!requester)
            return false;
        const bool validPair = withData
            ? CHI::Eb::RespAndFwdStates::IsSnpRespDataFwdedValid(resp, fwdState)
            : CHI::Eb::RespAndFwdStates::IsSnpRespFwdedValid(resp, fwdState);
        if (!validPair)
            return false;

        const CacheState initial = Bank(flow.snoopLine).snoopFilter.State(flow.snoopLine, target);
        if (initial == CacheState::I || initial == CacheState::UCE || initial == CacheState::UDP)
            return false;
        const CacheState provider = DecodeSnoopState(transaction, target, resp, withData, false);
        const uint16_t opcode = SnoopOpcode(transaction, flow, target);
        const bool passDirty = withData && (resp & CHI::Eb::Resps::PassDirty) != 0;
        if (passDirty && !IsDirty(initial))
            return false;

        bool legal = false;
        switch (opcode)
        {
            case CHI::Eb::Opcodes::SNP::SnpOnceFwd:
                if (*requester != CacheState::I)
                    break;
                switch (initial)
                {
                    case CacheState::UC:
                        legal = !withData && (provider == CacheState::UC || provider == CacheState::SC ||
                            provider == CacheState::I);
                        break;
                    case CacheState::UD:
                        legal = (!withData && (provider == CacheState::UD || provider == CacheState::SD)) ||
                            (withData && passDirty && (provider == CacheState::SC || provider == CacheState::I));
                        break;
                    case CacheState::SC:
                        legal = !withData && (provider == CacheState::SC || provider == CacheState::I);
                        break;
                    case CacheState::SD:
                        legal = (!withData && provider == CacheState::SD) ||
                            (withData && passDirty && (provider == CacheState::SC || provider == CacheState::I));
                        break;
                    default:
                        break;
                }
                break;
            case CHI::Eb::Opcodes::SNP::SnpCleanFwd:
            case CHI::Eb::Opcodes::SNP::SnpNotSharedDirtyFwd:
                if (*requester != CacheState::SC)
                    break;
                if (initial == CacheState::UC || initial == CacheState::SC)
                    legal = !withData && (provider == CacheState::SC || provider == CacheState::I);
                else if (initial == CacheState::UD || initial == CacheState::SD)
                    legal = (!withData && provider == CacheState::SD) ||
                        (withData && passDirty && (provider == CacheState::SC || provider == CacheState::I));
                break;
            case CHI::Eb::Opcodes::SNP::SnpSharedFwd:
                if (initial == CacheState::UC || initial == CacheState::SC)
                {
                    legal = *requester == CacheState::SC && !withData && !passDirty &&
                        (provider == CacheState::SC || provider == CacheState::I);
                }
                else if (initial == CacheState::UD || initial == CacheState::SD)
                {
                    if (provider == CacheState::SD)
                        legal = *requester == CacheState::SC && !withData && !passDirty;
                    else if (provider == CacheState::SC || provider == CacheState::I)
                    {
                        if (*requester == CacheState::SD)
                            legal = !withData && !passDirty;
                        else if (*requester == CacheState::SC)
                            legal = withData && passDirty;
                    }
                }
                break;
            case CHI::Eb::Opcodes::SNP::SnpUniqueFwd:
                legal = provider == CacheState::I && !withData && !passDirty &&
                    (((initial == CacheState::UC || initial == CacheState::SC) && *requester == CacheState::UC) ||
                     ((initial == CacheState::UD || initial == CacheState::SD) && *requester == CacheState::UD));
                break;
            case CHI::Eb::Opcodes::SNP::SnpPreferUniqueFwd:
                if (provider == CacheState::I)
                {
                    legal = !withData && !passDirty &&
                        (((initial == CacheState::UC || initial == CacheState::SC) && *requester == CacheState::UC) ||
                         ((initial == CacheState::UD || initial == CacheState::SD) && *requester == CacheState::UD));
                }
                else if (initial == CacheState::UC)
                {
                    legal = *requester == CacheState::SC && provider == CacheState::SC && !withData && !passDirty;
                }
                else if (initial == CacheState::UD || initial == CacheState::SD)
                {
                    if (provider == CacheState::SD)
                        legal = *requester == CacheState::SC && !withData && !passDirty;
                    else if (provider == CacheState::SC)
                        legal = *requester == CacheState::SC && withData && passDirty;
                }
                else if (initial == CacheState::SC)
                {
                    legal = *requester == CacheState::SC && provider == CacheState::SC && !withData && !passDirty;
                }
                break;
            default:
                return false;
        }
        if (!legal)
            return false;
        if (options.doNotGoToSD && provider == CacheState::SD &&
            opcode != CHI::Eb::Opcodes::SNP::SnpOnceFwd)
            return false;
        if (IsDirty(initial) && !withData && !IsDirty(provider) && !IsDirty(*requester))
            return false;
        if (flow.dctForwarded && flow.dctFwdState != fwdState)
            return false;

        if (!flow.dctForwarded)
        {
            flow.dctForwarded = true;
            flow.dctFwdState = fwdState;
            flow.requesterFinal = *requester;
        }
        return true;
    }

    bool Core::Impl::ValidateSnoopResponse(Transaction& transaction, NodeId target, uint8_t resp,
                                           bool withData, bool partial, uint8_t dataPull) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        if (std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target) == transaction.pendingSnoops.end())
            return false;
        if (dataPull != 0)
            return false;
        const CacheState initial = Bank(flow.snoopLine).snoopFilter.State(flow.snoopLine, target);
        const uint16_t opcode = SnoopOpcode(transaction, flow, target);
        const CacheState final = DecodeSnoopState(transaction, target, resp, withData, partial);
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
        const bool dirtyTransitionPossible = IsDirty(initial) || initial == CacheState::UC;
        if (passDirty && !dirtyTransitionPossible)
            return false;
        if (opcode == CHI::Eb::Opcodes::SNP::SnpCleanShared)
        {
            if (IsDirty(final) || (initial == CacheState::UCE && final != CacheState::I))
                return false;
            return !IsDirty(initial) || (withData && passDirty);
        }
        if (options.doNotGoToSD && final == CacheState::SD && opcode != CHI::Eb::Opcodes::SNP::SnpOnce &&
            opcode != CHI::Eb::Opcodes::SNP::SnpOnceFwd)
            return false;
        if ((opcode == CHI::Eb::Opcodes::SNP::SnpShared || opcode == CHI::Eb::Opcodes::SNP::SnpSharedFwd ||
             opcode == CHI::Eb::Opcodes::SNP::SnpClean || opcode == CHI::Eb::Opcodes::SNP::SnpCleanFwd ||
             opcode == CHI::Eb::Opcodes::SNP::SnpNotSharedDirty ||
             opcode == CHI::Eb::Opcodes::SNP::SnpNotSharedDirtyFwd) && IsUnique(final))
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
                case CacheState::UC: return !partial;
                case CacheState::SC: return !partial && !passDirty;
                case CacheState::UD:
                case CacheState::SD: return withData && !partial && passDirty;
                case CacheState::UDP: return withData && partial && passDirty;
                default: return false;
            }
        }
        const bool invalidateWithData = opcode == CHI::Eb::Opcodes::SNP::SnpUnique ||
            opcode == CHI::Eb::Opcodes::SNP::SnpUniqueFwd || opcode == CHI::Eb::Opcodes::SNP::SnpCleanInvalid;
        if (invalidateWithData && final != CacheState::I)
            return false;
        if (IsDirty(initial) && invalidateWithData)
            return withData && passDirty;
        if (IsDirty(initial) && opcode != CHI::Eb::Opcodes::SNP::SnpMakeInvalid &&
            opcode != CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash && !withData && !IsDirty(final))
            return false;
        return true;
    }

    CacheState Core::Impl::DecodeSnoopState(Transaction& transaction, NodeId target, uint8_t resp,
                                            bool withData, bool partial) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        const CacheState initial = Bank(flow.snoopLine).snoopFilter.State(flow.snoopLine, target);
        const uint16_t opcode = SnoopOpcode(transaction, flow, target);
        const bool stashRead = opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
            opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        if (stashRead)
            return initial;
        if (!withData && (resp & 0b011) == 0b010 && !(resp & CHI::Eb::Resps::PassDirty))
        {
            if (initial == CacheState::UCE)
                return CacheState::UCE;
            if (initial == CacheState::UDP)
                return CacheState::UDP;
        }
        switch (resp & 0b011)
        {
            case 0b000: return CacheState::I;
            case 0b001: return CacheState::SC;
            case 0b011: return CacheState::SD;
            default:
                if (partial)
                    return CacheState::UDP;
                if (!withData && IsDirty(initial) &&
                    opcode != CHI::Eb::Opcodes::SNP::SnpCleanShared &&
                    opcode != CHI::Eb::Opcodes::SNP::SnpCleanInvalid)
                    return CacheState::UD;
                if (!withData || (resp & CHI::Eb::Resps::PassDirty))
                    return CacheState::UC;
                return CacheState::UD;
        }
    }

    void Core::Impl::CompleteSnoopTarget(Transaction& transaction, NodeId target, uint8_t resp,
                                         bool withData, bool partial)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.dctActive && target == flow.dctTarget)
            flow.dctProviderResponded = true;
        else if (const auto allocated = flow.snoopIds.find(target); allocated != flow.snoopIds.end())
        {
            snoopIdAllocator.Release(allocated->second);
            flow.snoopIds.erase(allocated);
        }
        const CacheState final = DecodeSnoopState(transaction, target, resp, withData, partial);
        const bool ambiguousUnique = final == CacheState::UD && !partial && (resp & 0b011) == 0b010 &&
            !(resp & CHI::Eb::Resps::PassDirty) && Bank(flow.snoopLine).snoopFilter.PossiblyClean(flow.snoopLine, target);
        if (flow.victimSnoopFilterOnly && flow.victimPending && flow.snoopLine == flow.victimLine)
        {
            const SnoopFilterEntry& current = Bank(flow.snoopLine).snoopFilter.At(Bank(flow.snoopLine).snoopFilter.SetIndex(flow.victimLine), flow.victimWay);
            if (current.valid && current.key == flow.victimLine && current.generation == flow.victimGeneration)
            {
                Bank(flow.snoopLine).snoopFilter.Track(flow.snoopLine, target, final, time, ambiguousUnique);
                flow.victimGeneration = current.generation;
                flow.victimResponseUpdated = true;
            }
        }
        else
            Bank(flow.snoopLine).snoopFilter.Track(flow.snoopLine, target, final, time, ambiguousUnique);
        auto it = std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target);
        if (it != transaction.pendingSnoops.end())
        {
            transaction.pendingSnoops.erase(it);
            ++pmu.completedSnoopTargets;
        }
        if (transaction.pendingSnoops.empty())
        {
            if (flow.nextSnoop >= transaction.snoopTargets.size())
                ReleaseSnoopWave(transaction);
            if (flow.dctForwarded && !flow.snoopForVictim && !flow.victimPending)
                FinishResponse(transaction);
            else if (options.strategy.snoopDataThroughDataPort)
            {
                transaction.wait = WaitReason::None;
                ActionCoherent(transaction);
                if (transaction.phase == Phase::Return)
                    transaction.readyTime = ScheduleData(transaction, options.snoopDataLatency);
            }
            else
            {
                transaction.phase = Phase::Action;
                transaction.wait = WaitReason::None;
                transaction.readyTime = time;
            }
        }
    }

}
