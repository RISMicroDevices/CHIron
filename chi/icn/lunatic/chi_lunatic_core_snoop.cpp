#include "chi_lunatic_core.hpp"
#include "chi_lunatic_snoop_response.hpp"

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
            if (needsUnique || needsDowngrade || snoopFilter.PossiblyDirty(transaction.request.Line(), target))
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

        flow.dctTarget = candidates.front();
        flow.retToSrcUsed = false;
        if (flow.dctActive)
        {
            const std::vector<NodeId> owners = snoopFilter.Targets(transaction.request.Line());
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
                const auto decision = events->Decide(SnoopForwardingEvent(time, nodeId, transaction,
                    flow.dctTarget, snoopFilter.State(transaction.request.Line(), flow.dctTarget),
                    owners.size(), flow.homeDirty));
                flow.dctActive = decision.IsForwarding();
            }
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
            const bool cleanShared = transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanShared ||
                ClassifyRequest(transaction.request.opcode) == RequestClass::Persistence;
            if (!cleanShared || snoopFilter.PossiblyDirty(transaction.request.Line(), target) ||
                !options.preciseSnoopFilter)
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
        if (flow.dctActive && target == flow.dctTarget)
        {
            snoop.txnId = transaction.handle;
            snoop.fwdNid = transaction.request.source;
            snoop.fwdTxnId = transaction.request.txnId;
        }
        snoop.address = flow.snoopLine.address + (flow.snoopForVictim ? 0 : (transaction.request.address & 0x30));
        snoop.nonSecure = flow.snoopLine.nonSecure;
        snoop.opcode = SnoopOpcode(transaction, flow, target);
        snoop.qos = transaction.request.qos;
        snoop.traceTag = transaction.request.traceTag;
        const bool stashSnoop = snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUniqueStash ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
            snoop.opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        snoop.stashLpid = stashSnoop ? transaction.request.stashLpid : 0;
        snoop.stashLpidValid = stashSnoop && transaction.request.stashLpidValid;
        snoop.mpam = stashSnoop ? transaction.request.mpam : uint16_t(snoop.nonSecure) << 10;
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
            Error(transaction.handle, "snoop response ID is still live");
            return false;
        }
        if (!Queue(txsnp, snoop, transaction.handle, transaction.actionAge, index + 1 == transaction.snoopTargets.size()))
            return false;
        flow.snoopRetToSrc[target] = snoop.retToSrc;
        flow.snoopDoNotGoToSD[target] = snoop.doNotGoToSD;
        if (snoop.retToSrc)
            flow.retToSrcUsed = true;
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
        {
            Error(transaction.handle, "illegal snoop response");
            return;
        }
        if ((flit.respErr != CHI::Eb::RespErrs::OK && flit.respErr != CHI::Eb::RespErrs::NDERR) ||
            (flit.respErr == CHI::Eb::RespErrs::NDERR && flit.resp != CHI::Eb::Resps::SnpResp_I))
        {
            Error(transaction.handle, "illegal snoop error response");
            return;
        }
        if (flit.respErr == CHI::Eb::RespErrs::NDERR)
            transaction.responseRespErr = MergeResponseErrors(transaction.responseRespErr, flit.respErr);
        CompleteSnoopTarget(transaction, flit.source, flit.resp, false);
    }

    void Core::Impl::ProcessSnoopDAT(Transaction& transaction, const Data& flit)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flit.respErr != CHI::Eb::RespErrs::OK && flit.respErr != CHI::Eb::RespErrs::DERR)
        {
            Error(transaction.handle, "illegal snoop data error response");
            return;
        }
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = flit.ByteOffset();
        const uint64_t snoopAddress = flow.snoopLine.address +
            (flow.snoopForVictim ? 0 : (transaction.request.address & 0x30));
        if (offset + bytes > LINE_SIZE || offset % bytes != 0 || flit.ccid != ((snoopAddress >> 4) & 3))
        {
            Error(transaction.handle, "invalid snoop data packet position");
            return;
        }
        const bool partial = flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataPtl;
        for (size_t index = 0; index < bytes; ++index)
            if ((!partial && !flit.byteEnables[index]) || (!flit.byteEnables[index] && flit.bytes[index] != 0))
            {
                Error(transaction.handle, "invalid snoop data byte enables");
                return;
            }
        if (flit.opcode == CHI::Eb::Opcodes::DAT::SnpRespDataFwded &&
            !AcceptForwardedResponse(transaction, flit.source, flit.resp, flit.fwdState, true))
        {
            Error(transaction.handle, "unexpected or illegal forwarding snoop data");
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
        if (flit.opcode != CHI::Eb::Opcodes::DAT::SnpRespDataFwded &&
            !ValidateSnoopResponse(transaction, flit.source, flit.resp, true, partial, flit.dataSource))
        {
            Error(transaction.handle, "illegal snoop data response");
            return;
        }
        flow.snoopResponses[flit.source] = flit.resp;

        const CacheState initial = snoopFilter.State(flow.snoopLine, flit.source);
        const bool dirty = IsDirty(initial) ||
            (flit.resp & CHI::Eb::Resps::PassDirty) != 0;
        const bool dirtyError = dirty && flit.respErr == CHI::Eb::RespErrs::DERR;
        auto& errorValues = flow.snoopForVictim ? flow.victimDataRespErr : flow.snoopDataRespErr;
        uint64_t& errorMask = flow.snoopForVictim ? flow.victimDataRespErrMask : flow.snoopDataRespErrMask;
        if (!(errorMask & bit))
            errorValues[beat] = CHI::Eb::RespErrs::OK;
        if (dirtyError)
        {
            errorValues[beat] = MergeResponseErrors(errorValues[beat], flit.respErr);
            if (!IsReadRequest(transaction.request.opcode))
                transaction.responseRespErr = MergeResponseErrors(transaction.responseRespErr, flit.respErr);
        }
        errorMask |= bit;
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
        const CacheState initial = snoopFilter.State(flow.snoopLine, target);
        std::vector<CacheState> possibleInitial = PossibleSnoopStates(initial);
        if (initial == CacheState::UD && snoopFilter.PossiblyClean(flow.snoopLine, target))
        {
            const std::vector<CacheState> cleanInitial = PossibleSnoopStates(CacheState::UC);
            possibleInitial.insert(possibleInitial.end(), cleanInitial.begin(), cleanInitial.end());
        }
        const ForwardedSnoopResponse response{
            .opcode = SnoopOpcode(transaction, flow, target),
            .resp = resp,
            .fwdState = fwdState,
            .withData = withData,
            .retToSrc = flow.snoopRetToSrc.contains(target) && flow.snoopRetToSrc.at(target),
            .doNotGoToSD = flow.snoopDoNotGoToSD.contains(target) && flow.snoopDoNotGoToSD.at(target)
        };
        if (!IsForwardedSnoopResponseLegal(possibleInitial, response))
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

        const CacheState initial = snoopFilter.State(flow.snoopLine, target);
        std::vector<CacheState> possibleInitial = PossibleSnoopStates(initial);
        if (initial == CacheState::UD && snoopFilter.PossiblyClean(flow.snoopLine, target))
        {
            const std::vector<CacheState> cleanInitial = PossibleSnoopStates(CacheState::UC);
            possibleInitial.insert(possibleInitial.end(), cleanInitial.begin(), cleanInitial.end());
        }

        const OrdinarySnoopResponse response{
            .opcode = SnoopOpcode(transaction, flow, target),
            .resp = resp,
            .dataPull = dataPull,
            .withData = withData,
            .partial = partial,
            .retToSrc = flow.snoopRetToSrc.contains(target) && flow.snoopRetToSrc.at(target),
            .doNotGoToSD = flow.snoopDoNotGoToSD.contains(target) && flow.snoopDoNotGoToSD.at(target)
        };
        return IsOrdinarySnoopResponseLegal(possibleInitial, response);
    }

    CacheState Core::Impl::DecodeSnoopState(Transaction& transaction, NodeId target, uint8_t resp,
                                            bool withData, bool partial) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        const CacheState initial = snoopFilter.State(flow.snoopLine, target);
        const uint16_t opcode = SnoopOpcode(transaction, flow, target);
        const bool stashRead = opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
            opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        if (stashRead && resp == CHI::Eb::Resps::SnpResp_I)
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
                if (resp & CHI::Eb::Resps::PassDirty)
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
        const CacheState final = DecodeSnoopState(transaction, target, resp, withData, partial);
        const bool ambiguousUnique = final == CacheState::UD && !partial && (resp & 0b011) == 0b010 &&
            !(resp & CHI::Eb::Resps::PassDirty) && snoopFilter.PossiblyClean(flow.snoopLine, target);
        if (flow.victimSnoopFilterOnly && flow.victimPending && flow.snoopLine == flow.victimLine)
        {
            const SnoopFilterEntry& current = snoopFilter.At(snoopFilter.SetIndex(flow.victimLine), flow.victimWay);
            if (current.valid && current.key == flow.victimLine && current.generation == flow.victimGeneration)
            {
                snoopFilter.Track(flow.snoopLine, target, final, time, ambiguousUnique);
                flow.victimGeneration = current.generation;
                flow.victimResponseUpdated = true;
            }
        }
        else
            snoopFilter.Track(flow.snoopLine, target, final, time, ambiguousUnique);
        auto it = std::find(transaction.pendingSnoops.begin(), transaction.pendingSnoops.end(), target);
        if (it != transaction.pendingSnoops.end())
        {
            transaction.pendingSnoops.erase(it);
            ++pmu.completedSnoopTargets;
        }
        if (transaction.pendingSnoops.empty())
        {
            if (flow.dctForwarded && !flow.snoopForVictim && !flow.victimPending)
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
