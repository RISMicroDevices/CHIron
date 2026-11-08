#include "chi_lunatic_core.hpp"

#include <algorithm>

namespace CHI::Lunatic {
    namespace {

        bool IsDirtyCopyback(uint8_t resp)
        {
            return resp == CHI::Eb::Resps::CopyBackWrData_UD_PD || resp == CHI::Eb::Resps::CopyBackWrData_SD_PD;
        }

        ByteEnables FullEnables()
        {
            ByteEnables enables{};
            enables.fill(1);
            return enables;
        }

    }

    bool Core::Impl::PrepareHomeCommit(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.directoryCommitted)
            return true;

        const auto requestClass = ClassifyRequest(transaction.request.opcode);
        if ((requestClass == RequestClass::CoherentRead || requestClass == RequestClass::MakeUnique ||
             requestClass == RequestClass::MakeReadUnique || requestClass == RequestClass::CopybackWrite ||
             requestClass == RequestClass::WriteEvictOrEvict || requestClass == RequestClass::CleanUnique) &&
            !PrepareSnoopFilterCapacity(transaction))
            return false;

        bool committed = false;
        switch (requestClass)
        {
            case RequestClass::ReadOnce:
            case RequestClass::CoherentRead:
            case RequestClass::MakeUnique:
            case RequestClass::MakeReadUnique:
                committed = CommitReadDirectory(transaction);
                break;
            case RequestClass::CopybackWrite:
            case RequestClass::WriteEvictOrEvict:
                committed = CommitCopyback(transaction);
                break;
            case RequestClass::ImmediateWrite:
            case RequestClass::WriteUniqueZero:
                committed = CommitImmediateWrite(transaction);
                break;
            case RequestClass::Atomic:
                committed = CommitAtomic(transaction);
                break;
            case RequestClass::CleanUnique:
                committed = CommitCleanUnique(transaction);
                break;
            case RequestClass::CMO:
            case RequestClass::Persistence:
                committed = CommitCMO(transaction);
                break;
            default:
                committed = true;
                break;
        }
        if (committed)
            flow.directoryCommitted = true;
        return committed;
    }

    bool Core::Impl::CommitReadDirectory(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (transaction.responseRespErr == CHI::Eb::RespErrs::NDERR)
            return true;
        const LineKey line = transaction.request.Line();
        PolicyContext context;
        context.line = line;
        context.requester = transaction.request.source;
        context.homeValid = transaction.homeHit;
        context.homeDirty = flow.homeDirty;
        context.requesterValid = flow.requesterWasValid;
        context.requesterDirty = flow.requesterWasDirty;
        if (ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce)
        {
            for (NodeId target : SnoopTargets(line))
                context.requesterValid |= IsValid(snoopFilter.State(line, target));
        }
        PolicyDecision decision = ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce
            ? policy->OnHomeAllocation(context) : policy->OnRNFill(context);
        if (flow.dctForwarded && !flow.snoopDataValid)
        {
            decision.allocateHome = false;
            decision.retainHome = transaction.homeHit && options.policy != PolicyKind::Exclusive;
            decision.removeHome = transaction.homeHit && options.policy == PolicyKind::Exclusive;
        }
        flow.allocateHome = decision.allocateHome;
        flow.retainHome = decision.retainHome;
        flow.removeHome = decision.removeHome;

        size_t otherSharers = 0;
        for (NodeId target : SnoopTargets(line))
        {
            if (target != transaction.request.source && IsValid(snoopFilter.State(line, target)))
                ++otherSharers;
        }

        bool partialDirtyLine = false;
        for (uint8_t valid : flow.validBytes)
            partialDirtyLine |= !valid;

        if (!flow.dctForwarded)
        {
            switch (ClassifyRequest(transaction.request.opcode))
            {
                case RequestClass::CoherentRead:
                    if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadUnique)
                        flow.requesterFinal = partialDirtyLine && (flow.dataDirty || flow.passDirty || flow.homeDirty || flow.requesterWasDirty)
                            ? CacheState::UDP
                            : flow.dataDirty || flow.passDirty || flow.homeDirty || flow.requesterWasDirty ? CacheState::UD : CacheState::UC;
                    else if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadNotSharedDirty)
                        flow.requesterFinal = CacheState::SC;
                    else if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadPreferUnique)
                        flow.requesterFinal = snoopFilter.State(line, transaction.request.source) == CacheState::SD
                            ? CacheState::SD : CacheState::SC;
                    else if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadClean)
                    {
                        const CacheState initial = snoopFilter.State(line, transaction.request.source);
                        switch (initial)
                        {
                            case CacheState::UC:
                            case CacheState::UCE:
                                flow.requesterFinal = CacheState::UC;
                                break;
                            case CacheState::UD:
                            case CacheState::UDP:
                                flow.requesterFinal = CacheState::UD;
                                break;
                            case CacheState::SD:
                                flow.requesterFinal = CacheState::SD;
                                break;
                            default:
                                flow.requesterFinal = CacheState::SC;
                                break;
                        }
                    }
                    else if (flow.passDirty)
                        flow.requesterFinal = otherSharers ? CacheState::SD : CacheState::UD;
                    else
                        flow.requesterFinal = CacheState::SC;
                    break;
                case RequestClass::MakeReadUnique:
                {
                    const CacheState responseState = snoopFilter.State(line, transaction.request.source);
                    if (responseState == CacheState::SD)
                        flow.requesterFinal = CacheState::UD;
                    else if (responseState == CacheState::SC)
                        flow.requesterFinal = CacheState::UC;
                    else if (IsValid(responseState))
                        flow.requesterFinal = IsDirty(responseState) ? CacheState::UD : CacheState::UC;
                    else
                        flow.requesterFinal = partialDirtyLine && (flow.dataDirty || flow.passDirty || flow.homeDirty)
                            ? CacheState::UDP
                            : flow.dataDirty || flow.passDirty || flow.homeDirty ? CacheState::UD : CacheState::UC;
                    break;
                }
                case RequestClass::MakeUnique:
                    flow.requesterFinal = CacheState::UD;
                    break;
                case RequestClass::ReadOnce:
                    flow.requesterFinal = CacheState::I;
                    break;
                default:
                    return false;
            }
        }

        if (decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
            return false;

        if (decision.allocateHome || decision.retainHome)
        {
            const std::optional<size_t> currentWay = directory.FindWay(line);
            const size_t way = currentWay ? *currentWay : flow.victimWay;
            bool anyRequesterDirty = IsDirty(flow.requesterFinal);
            for (NodeId target : SnoopTargets(line))
            {
                if (target != transaction.request.source && IsDirty(snoopFilter.State(line, target)))
                    anyRequesterDirty = true;
            }
            const bool dirty = anyRequesterDirty ? false : flow.homeDirty || flow.dataDirty;
            HomeLine& home = InstallHome(line, way, transaction.data, dirty, time);
            CopyMetadata(home, transaction, options);
            home.validBytes = flow.validBytes;
        }
        else if (decision.removeHome && transaction.homeHit)
        {
            if (const auto way = directory.FindWay(line); way)
                directory.Invalidate(directory.SetIndex(line), *way);
        }

        const bool invalidatingRead = transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
            transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid;
        if (invalidatingRead && (flow.homeDirty || flow.dataDirty))
        {
            if (!WriteLine(transaction, line, transaction.data, FullEnables()))
                return false;
            if (HomeLine* home = directory.Find(line))
            {
                home->dirty = false;
                directory.Touch(line);
            }
        }
        else if (options.policy == PolicyKind::Exclusive && !decision.allocateHome && !decision.retainHome &&
            (flow.homeDirty || flow.dataDirty) && !WriteLine(transaction, line, transaction.data, FullEnables()))
            return false;

        if (flow.dctForwarded || transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
            transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid)
            snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
        else if (ClassifyRequest(transaction.request.opcode) != RequestClass::ReadOnce)
            snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
        return true;
    }

    bool Core::Impl::CommitCopyback(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        const bool dirty = IsDirtyCopyback(flow.writeResp);
        const bool hasData = std::any_of(transaction.byteEnables.begin(), transaction.byteEnables.end(),
            [](uint8_t enabled) { return enabled != 0; });

        if (hasData && transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteBackPtl && !transaction.homeHit)
        {
            if (options.backend != BackendKind::Internal && !FetchBaseline(transaction))
                return false;
            if (options.backend == BackendKind::Internal)
            {
                const LineImage baseline = memory.ReadImage(line);
                ByteEnables enables{};
                for (size_t index = 0; index < LINE_SIZE; ++index)
                {
                    if (transaction.byteEnables[index])
                        continue;
                    transaction.data[index] = baseline.data[index];
                    enables[index] = 1;
                }
                MergeMetadata(transaction.poison, transaction.dataCheck, baseline.poison, baseline.dataCheck,
                              enables, 0, 0, LINE_SIZE, options);
            }
        }

        if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteCleanFull)
        {
            const uint8_t state = flow.writeResp & 0b011;
            flow.requesterFinal = state == 0b000 ? CacheState::I
                    : (state == 0b001 || state == 0b011) ? CacheState::SC : CacheState::UC;
        }
        else
            flow.requesterFinal = CacheState::I;

        PolicyContext context;
        context.line = line;
        context.requester = transaction.request.source;
        context.homeValid = transaction.homeHit;
        context.homeDirty = flow.homeDirty;
        context.requesterValid = flow.requesterWasValid;
        context.requesterDirty = dirty;
        context.requesterRetains = IsValid(flow.requesterFinal);
        context.evictionHasData = hasData;
        const PolicyDecision decision = policy->OnCopyback(context);
        bool otherRequesterValid = false;
        for (NodeId target : SnoopTargets(line))
        {
            if (target != transaction.request.source && IsValid(snoopFilter.State(line, target)))
                otherRequesterValid = true;
        }
        const bool writeThrough = options.policy == PolicyKind::Exclusive &&
            !IsValid(flow.requesterFinal) && otherRequesterValid;
        if (hasData && !writeThrough && decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
            return false;

        if (hasData)
        {
            if (!writeThrough && decision.mergeCopyback &&
                (decision.allocateHome || decision.retainHome || transaction.homeHit))
            {
                const std::optional<size_t> currentWay = directory.FindWay(line);
                HomeLine& home = InstallHome(line, currentWay ? *currentWay : flow.victimWay, transaction.data, dirty, time);
                CopyMetadata(home, transaction, options);
                if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteBackPtl)
                    home.validBytes = transaction.byteEnables;
            }
            else if (!WriteLine(transaction, line, transaction.data, transaction.byteEnables))
                return false;
        }

        if ((decision.removeHome || writeThrough) && transaction.homeHit)
            if (const auto way = directory.FindWay(line); way)
                directory.Invalidate(directory.SetIndex(line), *way);
        snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
        return true;
    }

    bool Core::Impl::CommitCMO(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        if (flow.snoopDataPartial && !transaction.homeHit)
        {
            if (options.backend != BackendKind::Internal)
            {
                if (!FetchBaseline(transaction))
                    return false;
            }
            else
            {
                const LineImage baseline = memory.ReadImage(line);
                ByteEnables enables{};
                for (size_t index = 0; index < LINE_SIZE; ++index)
                {
                    if (flow.snoopValidBytes[index])
                        continue;
                    transaction.data[index] = baseline.data[index];
                    enables[index] = 1;
                }
                MergeMetadata(transaction.poison, transaction.dataCheck, baseline.poison, baseline.dataCheck,
                              enables, 0, 0, LINE_SIZE, options);
            }
        }
        HomeLine* home = directory.Find(line);

        if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanShared ||
            ClassifyRequest(transaction.request.opcode) == RequestClass::Persistence)
        {
            flow.requesterFinal = snoopFilter.State(line, transaction.request.source);
            if (home)
            {
                if (flow.snoopDataValid)
                {
                    home->data = transaction.data;
                    CopyMetadata(*home, transaction, options);
                }
                home->dirty = false;
                directory.Touch(line);
            }
            if ((flow.dataDirty || flow.homeDirty) && !WriteLine(transaction, line, transaction.data, FullEnables()))
                return false;
            return true;
        }

        flow.requesterFinal = CacheState::I;
        if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanInvalid && (flow.dataDirty || flow.homeDirty) &&
            !WriteLine(transaction, line, transaction.data, FullEnables()))
            return false;
        if (home)
            if (const auto way = directory.FindWay(line); way)
                directory.Invalidate(directory.SetIndex(line), *way);
        snoopFilter.Track(line, transaction.request.source, CacheState::I, time);
        return true;
    }

    size_t Core::Impl::ChooseVictim(TransactionHandle handle, VictimDomain domain, const LineKey& line)
    {
        const bool directoryDomain = domain == VictimDomain::Directory;
        if (events->OnVictimSelection.empty())
            return directoryDomain ? directory.ChooseVictim(line) : snoopFilter.ChooseVictim(line);

        std::vector<size_t> candidates = directoryDomain
            ? directory.VictimCandidates(line) : snoopFilter.VictimCandidates(line);
        const auto decision = events->Decide(
            VictimSelectionEvent(time, nodeId, handle, domain, line, candidates));
        const auto selected = decision.GetSelectedVictim();
        if (selected)
        {
            for (size_t way : candidates)
                if (way == *selected)
                    return way;
            Error(handle, "victim decision selected a way outside the legal candidate set");
        }
        return directoryDomain ? directory.ChooseVictim(line) : snoopFilter.ChooseVictim(line);
    }

    bool Core::Impl::PrepareSnoopFilterCapacity(Transaction& transaction)
    {
        const LineKey line = transaction.request.Line();
        if (snoopFilter.Find(line) || snoopFilter.HasInvalidWay(line))
            return true;

        FlowState& flow = flows.at(transaction.handle);
        if (flow.victimPending)
            return false;

        flow.victimSnoopFilterOnly = true;
        flow.victimWay = ChooseVictim(transaction.handle, VictimDomain::SnoopFilter, line);
        const SnoopFilterEntry& victim = snoopFilter.At(snoopFilter.SetIndex(line), flow.victimWay);
        flow.victimLine = victim.key;
        flow.victimGeneration = victim.generation;
        flow.victimResponseUpdated = false;
        flow.victimPending = true;
        if (const HomeLine* home = directory.Find(flow.victimLine))
        {
            flow.victimData = home->data;
            flow.victimPoison = options.poisonEnabled ? home->poison : LinePoison{};
            flow.victimDataCheck = options.dataCheckEnabled ? home->dataCheck : LineDataCheck{};
            flow.victimDirty = home->dirty;
        }
        else
        {
            const LineImage image = memory.ReadImage(flow.victimLine);
            flow.victimData = image.data;
            flow.victimPoison = options.poisonEnabled ? image.poison : LinePoison{};
            flow.victimDataCheck = options.dataCheckEnabled ? image.dataCheck : LineDataCheck{};
            flow.victimDirty = false;
        }
        flow.victimDataValid = true;

        hazards.Append(transaction.handle, flow.victimLine, transaction.actionAge);
        flow.victimLinked = true;
        flow.snoopLine = flow.victimLine;
        flow.snoopForVictim = true;
        transaction.snoopTargets = SnoopTargets(flow.victimLine);
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        if (!hazards.IsHead(transaction.handle, flow.victimLine))
        {
            pmu.RecordStall(WaitReason::SameLine);
            return false;
        }

        if (transaction.snoopTargets.empty())
        {
            return CompleteVictim(transaction);
        }
        pmu.RecordStall(WaitReason::BackInvalidate);
        return false;
    }

    bool Core::Impl::PrepareVictim(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        if (!flow.victimPending)
        {
            flow.victimWay = ChooseVictim(transaction.handle, VictimDomain::Directory, line);
            const HomeLine& victim = directory.At(directory.SetIndex(line), flow.victimWay);
            if (!victim.valid)
                return true;
            flow.victimPending = true;
            flow.victimLine = victim.key;
        flow.victimGeneration = victim.generation;
        flow.victimResponseUpdated = false;
            flow.victimData = victim.data;
            flow.victimPoison = options.poisonEnabled ? victim.poison : LinePoison{};
            flow.victimDataCheck = options.dataCheckEnabled ? victim.dataCheck : LineDataCheck{};
            flow.victimDirty = victim.dirty;
            flow.victimDataValid = true;
        }

        if (!flow.victimLinked)
        {
            hazards.Append(transaction.handle, flow.victimLine, transaction.actionAge);
            flow.victimLinked = true;
        }

        PolicyContext context;
        context.line = flow.victimLine;
        context.homeValid = true;
        context.homeDirty = flow.victimDirty;
        context.requesterValid = !SnoopTargets(flow.victimLine).empty();
        const PolicyDecision decision = policy->OnHomeEviction(context);
        if (decision.backInvalidate && context.requesterValid && flow.nextSnoop == 0)
        {
            flow.snoopForVictim = true;
            flow.snoopLine = flow.victimLine;
            transaction.snoopTargets = SnoopTargets(flow.victimLine);
            transaction.pendingSnoops.clear();
            transaction.snoopsRequired = true;
            flow.nextSnoop = 0;
            pmu.RecordStall(WaitReason::BackInvalidate);
            return false;
        }

        if (!hazards.IsHead(transaction.handle, flow.victimLine))
        {
            pmu.RecordStall(WaitReason::SameLine);
            return false;
        }
        if (flow.nextSnoop == 0)
        {
            return CompleteVictim(transaction);
        }
        return false;
    }

    bool Core::Impl::CompleteVictim(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.victimPending)
            return true;

        auto discardVictim = [&] {
            if (flow.victimLinked)
                hazards.Release(transaction.handle, flow.victimLine);
            flow.victimLinked = false;
            flow.victimPending = false;
            flow.victimSnoopFilterOnly = false;
            flow.victimDataValid = false;
            flow.victimDataPartial = false;
            flow.victimValidBytes.fill(0);
            flow.victimGeneration = 0;
            flow.victimResponseUpdated = false;
            flow.victimDirty = false;
            flow.snoopForVictim = false;
            flow.snoopLine = transaction.request.Line();
            transaction.snoopTargets.clear();
            transaction.pendingSnoops.clear();
            transaction.snoopsRequired = false;
            flow.nextSnoop = 0;
            flow.snoopBeatMasks.clear();
            flow.snoopResponses.clear();
        };

        const bool snoopFilterOnly = flow.victimSnoopFilterOnly;
        if (snoopFilterOnly)
        {
            const SnoopFilterEntry& current = snoopFilter.At(snoopFilter.SetIndex(flow.victimLine), flow.victimWay);
            const bool identityMatches = current.valid ? current.key == flow.victimLine : flow.victimResponseUpdated;
            if (!identityMatches || current.generation != flow.victimGeneration)
            {
                discardVictim();
                return PrepareSnoopFilterCapacity(transaction);
            }
        }
        else
        {
            const HomeLine& current = directory.At(directory.SetIndex(flow.victimLine), flow.victimWay);
            if (!current.valid || current.key != flow.victimLine || current.generation != flow.victimGeneration)
            {
                discardVictim();
                return PrepareVictim(transaction);
            }
        }

        if (options.backend != BackendKind::Internal && flow.victimDataPartial && !directory.Find(flow.victimLine) &&
            !FetchBaseline(transaction))
            return false;

        LineImage victimImage;
        victimImage.data = flow.victimData;
        victimImage.poison = flow.victimPoison;
        victimImage.dataCheck = flow.victimDataCheck;
        if (flow.victimSnoopFilterOnly)
        {
            if (flow.victimDirty)
            {
                if (HomeLine* home = directory.Find(flow.victimLine))
                {
                    home->data = flow.victimData;
                    home->poison = flow.victimPoison;
                    home->dataCheck = flow.victimDataCheck;
                    home->dirty = true;
                    directory.Touch(flow.victimLine);
                }
                else if (!WriteLineImage(transaction, flow.victimLine, victimImage, FullEnables(), true))
                    return false;
            }
            const auto requesters = snoopFilter.Targets(flow.victimLine);
            for (NodeId requester : requesters)
                snoopFilter.Remove(flow.victimLine, requester);
            snoopFilter.Invalidate(snoopFilter.SetIndex(flow.victimLine), flow.victimWay);
        }
        else
        {
            if (flow.victimDirty && !WriteLineImage(transaction, flow.victimLine, victimImage, FullEnables(), true))
                return false;
            directory.Invalidate(directory.SetIndex(flow.victimLine), flow.victimWay);
        }

        discardVictim();
        return true;
    }

    HomeLine& Core::Impl::InstallHome(const LineKey& line, size_t way, const LineData& data, bool dirty, Time age)
    {
        HomeLine& home = directory.Install(line, way, age);
        home.data = data;
        home.validBytes = FullEnables();
        home.dirty = dirty;
        return home;
    }

    void Core::Impl::EvictRequester(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        snoopFilter.Remove(transaction.request.Line(), transaction.request.source);
        flow.requesterFinal = CacheState::I;
    }

}
