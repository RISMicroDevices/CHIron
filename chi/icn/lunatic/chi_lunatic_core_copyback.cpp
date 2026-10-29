#include "chi_lunatic_core.hpp"

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
             requestClass == RequestClass::CopybackWrite) && !PrepareSnoopFilterCapacity(transaction))
            return false;

        bool committed = false;
        switch (requestClass)
        {
            case RequestClass::ReadOnce:
            case RequestClass::CoherentRead:
            case RequestClass::MakeUnique:
                committed = CommitReadDirectory(transaction);
                break;
            case RequestClass::CopybackWrite:
                committed = CommitCopyback(transaction);
                break;
            case RequestClass::CMO:
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
            for (NodeId target : snoopFilter.Targets(line))
                context.requesterValid |= IsValid(snoopFilter.State(line, target));
        }
        const PolicyDecision decision = ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce
            ? policy->OnHomeAllocation(context) : policy->OnRNFill(context);
        flow.allocateHome = decision.allocateHome;
        flow.retainHome = decision.retainHome;
        flow.removeHome = decision.removeHome;

        size_t otherSharers = 0;
        for (NodeId target : snoopFilter.Targets(line))
        {
            if (target != transaction.request.source && IsValid(snoopFilter.State(line, target)))
                ++otherSharers;
        }

        bool partialDirtyLine = false;
        for (uint8_t valid : flow.validBytes)
            partialDirtyLine |= !valid;

        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::CoherentRead:
                if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadUnique)
                    flow.requesterFinal = partialDirtyLine && (flow.dataDirty || flow.passDirty || flow.homeDirty)
                        ? CacheState::UDP
                        : flow.dataDirty || flow.passDirty || flow.homeDirty ? CacheState::UD : CacheState::UC;
                else if (flow.passDirty)
                    flow.requesterFinal = otherSharers ? CacheState::SD : CacheState::UD;
                else
                    flow.requesterFinal = CacheState::SC;
                break;
            case RequestClass::MakeUnique:
                flow.requesterFinal = CacheState::UD;
                break;
            case RequestClass::ReadOnce:
                flow.requesterFinal = CacheState::I;
                break;
            default:
                return false;
        }

        if (decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
            return false;

        if (decision.allocateHome || decision.retainHome)
        {
            const std::optional<size_t> currentWay = directory.FindWay(line);
            const size_t way = currentWay ? *currentWay : flow.victimWay;
            bool anyRequesterDirty = IsDirty(flow.requesterFinal);
            for (NodeId target : snoopFilter.Targets(line))
            {
                if (target != transaction.request.source && IsDirty(snoopFilter.State(line, target)))
                    anyRequesterDirty = true;
            }
            const bool dirty = anyRequesterDirty ? false : flow.homeDirty || flow.dataDirty;
            HomeLine& home = InstallHome(line, way, transaction.data, dirty, time);
            home.validBytes = flow.validBytes;
        }
        else if (decision.removeHome && transaction.homeHit)
        {
            if (const auto way = directory.FindWay(line); way)
                directory.Invalidate(directory.SetIndex(line), *way);
        }

        if (options.policy == PolicyKind::Exclusive && !decision.allocateHome && !decision.retainHome &&
            (flow.homeDirty || flow.dataDirty) && !WriteLine(transaction, line, transaction.data, FullEnables()))
            return false;

        if (ClassifyRequest(transaction.request.opcode) != RequestClass::ReadOnce)
            snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
        return true;
    }

    bool Core::Impl::CommitCopyback(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        const bool dirty = IsDirtyCopyback(flow.writeResp);

        if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteBackPtl && !transaction.homeHit)
        {
            if (options.backend == BackendKind::CHI && !FetchBaseline(transaction))
                return false;
            if (options.backend == BackendKind::Internal)
            {
                const LineData base = memory.Read(line);
                for (size_t index = 0; index < LINE_SIZE; ++index)
                {
                    if (!transaction.byteEnables[index])
                        transaction.data[index] = base[index];
                }
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
        context.evictionHasData = true;
        const PolicyDecision decision = policy->OnCopyback(context);
        if (decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
            return false;

        if (decision.mergeCopyback && (decision.allocateHome || decision.retainHome || transaction.homeHit))
        {
            const std::optional<size_t> currentWay = directory.FindWay(line);
            HomeLine& home = InstallHome(line, currentWay ? *currentWay : flow.victimWay, transaction.data, dirty, time);
            if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteBackPtl)
                home.validBytes = transaction.byteEnables;
        }
        else if (!WriteLine(transaction, line, transaction.data, transaction.byteEnables))
            return false;

        if (decision.removeHome && transaction.homeHit)
            if (const auto way = directory.FindWay(line); way)
                directory.Invalidate(directory.SetIndex(line), *way);
        snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
        return true;
    }

    bool Core::Impl::CommitCMO(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        if (options.backend == BackendKind::CHI && flow.snoopDataPartial && !transaction.homeHit && !FetchBaseline(transaction))
            return false;
        HomeLine* home = directory.Find(line);

        if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanShared)
        {
            flow.requesterFinal = snoopFilter.State(line, transaction.request.source);
            if (home)
            {
                if (flow.snoopDataValid)
                    home->data = transaction.data;
                home->dirty = false;
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

    bool Core::Impl::PrepareSnoopFilterCapacity(Transaction& transaction)
    {
        const LineKey line = transaction.request.Line();
        if (snoopFilter.Find(line) || snoopFilter.HasInvalidWay(line))
            return true;

        FlowState& flow = flows.at(transaction.handle);
        if (flow.victimPending)
            return false;

        flow.victimSnoopFilterOnly = true;
        flow.victimWay = snoopFilter.ChooseVictim(line);
        const SnoopFilterEntry& victim = snoopFilter.At(snoopFilter.SetIndex(line), flow.victimWay);
        flow.victimLine = victim.key;
        flow.victimPending = true;
        if (const HomeLine* home = directory.Find(flow.victimLine))
        {
            flow.victimData = home->data;
            flow.victimDirty = home->dirty;
        }
        else
        {
            flow.victimData = memory.Read(flow.victimLine);
            flow.victimDirty = false;
        }
        flow.victimDataValid = true;

        hazards.Add(transaction.handle, flow.victimLine, transaction.actionAge);
        flow.victimLinked = true;
        if (!hazards.IsHead(transaction.handle, flow.victimLine))
            return false;

        flow.snoopLine = flow.victimLine;
        flow.snoopForVictim = true;
        transaction.snoopTargets = snoopFilter.Targets(flow.victimLine);
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        if (transaction.snoopTargets.empty())
        {
            return CompleteVictim(transaction);
        }
        return false;
    }

    bool Core::Impl::PrepareVictim(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        if (!flow.victimPending)
        {
            flow.victimWay = directory.ChooseVictim(line);
            const HomeLine& victim = directory.At(directory.SetIndex(line), flow.victimWay);
            if (!victim.valid)
                return true;
            flow.victimPending = true;
            flow.victimLine = victim.key;
            flow.victimData = victim.data;
            flow.victimDirty = victim.dirty;
            flow.victimDataValid = true;
        }

        if (!flow.victimLinked)
        {
            hazards.Add(transaction.handle, flow.victimLine, transaction.actionAge);
            flow.victimLinked = true;
        }
        if (!hazards.IsHead(transaction.handle, flow.victimLine))
            return false;

        PolicyContext context;
        context.line = flow.victimLine;
        context.homeValid = true;
        context.homeDirty = flow.victimDirty;
        context.requesterValid = !snoopFilter.Targets(flow.victimLine).empty();
        const PolicyDecision decision = policy->OnHomeEviction(context);
        if (decision.backInvalidate && context.requesterValid && flow.nextSnoop == 0)
        {
            flow.snoopForVictim = true;
            flow.snoopLine = flow.victimLine;
            transaction.snoopTargets = snoopFilter.Targets(flow.victimLine);
            transaction.pendingSnoops.clear();
            transaction.snoopsRequired = true;
            flow.nextSnoop = 0;
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
        if (options.backend == BackendKind::CHI && flow.victimDataPartial && !directory.Find(flow.victimLine) &&
            !FetchBaseline(transaction))
            return false;

        if (flow.victimSnoopFilterOnly)
        {
            if (flow.victimDirty)
            {
                if (HomeLine* home = directory.Find(flow.victimLine))
                {
                    home->data = flow.victimData;
                    home->dirty = true;
                }
                else if (!WriteLine(transaction, flow.victimLine, flow.victimData, FullEnables()))
                    return false;
            }
            const auto requesters = snoopFilter.Targets(flow.victimLine);
            for (NodeId requester : requesters)
                snoopFilter.Remove(flow.victimLine, requester);
            snoopFilter.Invalidate(snoopFilter.SetIndex(flow.victimLine), flow.victimWay);
        }
        else
        {
            if (flow.victimDirty && !WriteLine(transaction, flow.victimLine, flow.victimData, FullEnables()))
                return false;
            directory.Invalidate(directory.SetIndex(flow.victimLine), flow.victimWay);
        }

        if (flow.victimLinked)
            hazards.Release(transaction.handle, flow.victimLine);
        flow.victimLinked = false;
        flow.victimPending = false;
        flow.victimSnoopFilterOnly = false;
        flow.victimDataValid = false;
        flow.victimDataPartial = false;
        flow.victimValidBytes.fill(0);
        flow.victimDirty = false;
        flow.snoopForVictim = false;
        flow.snoopLine = transaction.request.Line();
        transaction.snoopTargets.clear();
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = false;
        flow.nextSnoop = 0;
        flow.snoopBeatMasks.clear();
        flow.snoopResponses.clear();
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
