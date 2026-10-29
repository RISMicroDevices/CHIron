#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {

    void Core::Impl::ResolveImmediateWrite(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        flow.snoopLine = transaction.request.Line();
        flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
        flow.responseForm = transaction.handle % 3;
        if (ClassifyRequest(transaction.request.opcode) == RequestClass::WriteUniqueZero)
        {
            transaction.data.fill(0);
            transaction.byteEnables.fill(1);
            flow.validBytes.fill(1);
            transaction.Mark(COMPLETE_WRITE_DATA);
        }
        else if (!transaction.homeHit)
            flow.validBytes.fill(0);

        bool stashTargetPlanned = false;
        for (NodeId target : snoopFilter.Targets(transaction.request.Line()))
        {
            if (target != transaction.request.source)
            {
                transaction.snoopTargets.push_back(target);
                stashTargetPlanned |= IsStashRequest(transaction.request.opcode) && target == transaction.request.returnNid;
            }
        }
        if (ClassifyRequest(transaction.request.opcode) == RequestClass::ImmediateWrite &&
            IsStashRequest(transaction.request.opcode) && transaction.request.stashNidValid &&
            transaction.request.returnNid != transaction.request.source && !stashTargetPlanned)
            transaction.snoopTargets.push_back(transaction.request.returnNid);
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
    }

    void Core::Impl::QueueImmediateResponse(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        switch (flow.responseForm)
        {
            case 0:
                if (!flow.dbidSent)
                    QueueDBID(transaction);
                if (!flow.compSent)
                    QueueComp(transaction);
                break;
            case 1:
                if (!flow.compSent)
                    QueueComp(transaction);
                if (!flow.dbidSent)
                    QueueDBID(transaction);
                break;
            default:
                if (!flow.compSent)
                {
                    QueueComp(transaction, true);
                    flow.dbidSent = flow.compSent;
                }
                break;
        }
    }

    void Core::Impl::ActionImmediateWrite(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.dbidSent || !flow.compSent)
        {
            QueueImmediateResponse(transaction);
            return;
        }
        if (!transaction.Complete(COMPLETE_WRITE_DATA))
        {
            transaction.phase = Phase::WaitWriteData;
            transaction.wait = WaitReason::WriteData;
            transaction.readyTime = time;
            return;
        }
        if (!PrepareHomeCommit(transaction))
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }
        if (flow.compEmitted)
            FinishResponse(transaction);
    }

    void Core::Impl::ActionEvictOrEvict(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        PolicyContext context;
        context.line = transaction.request.Line();
        context.requester = transaction.request.source;
        context.homeValid = transaction.homeHit;
        context.homeDirty = flow.homeDirty;
        context.requesterValid = flow.requesterWasValid;
        context.requesterDirty = flow.requesterWasDirty;
        context.requesterRetains = false;
        context.evictionHasData = true;
        const PolicyDecision decision = policy->OnEvictOrEvict(context);

        if (!decision.requestData)
        {
            flow.evictOrEvictNoData = true;
            EvictRequester(transaction);
            flow.directoryCommitted = true;
            if (!flow.compSent)
                QueueComp(transaction, false, CHI::Eb::Resps::Comp_I);
            else if (flow.compEmitted)
                FinishResponse(transaction);
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
            transaction.wait = WaitReason::WriteData;
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
    }

    bool Core::Impl::CommitImmediateWrite(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.directoryCommitted)
            return true;
        if (transaction.canceled)
        {
            flow.backendPhase = BackendPhase::Done;
            flow.directoryCommitted = true;
            snoopFilter.Track(transaction.request.Line(), transaction.request.source, CacheState::I, time);
            return true;
        }

        const LineKey line = transaction.request.Line();
        PolicyContext context;
        context.line = line;
        context.requester = transaction.request.source;
        context.homeValid = transaction.homeHit;
        context.homeDirty = flow.homeDirty;
        context.requesterValid = false;
        context.requesterDirty = flow.dataDirty;
        context.evictionHasData = true;
        context.allocationHint = CHI::Eb::MemAttrs::ExtractAllocate(transaction.request.memAttr) != 0;
        const PolicyDecision decision = policy->OnImmediateWrite(context);
        flow.allocateHome = decision.allocateHome;
        flow.retainHome = decision.retainHome;
        flow.removeHome = decision.removeHome;
        flow.mergeImmediateHome = decision.mergeCopyback &&
            (decision.allocateHome || decision.retainHome || transaction.homeHit);

        if (flow.mergeImmediateHome && decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
            return false;

        if (flow.mergeImmediateHome)
        {
            const std::optional<size_t> currentWay = directory.FindWay(line);
            const size_t way = currentWay ? *currentWay : flow.victimWay;
            const bool dirty = flow.homeDirty || flow.dataDirty || !flow.validBytes.empty();
            HomeLine& home = InstallHome(line, way, transaction.data, dirty, time);
            home.validBytes = flow.validBytes;
            flow.backendPhase = BackendPhase::Done;
        }
        else
        {
            if (!WriteLine(transaction, line, transaction.data, transaction.byteEnables))
                return false;
            flow.backendPhase = BackendPhase::Done;
        }

        if (decision.removeHome && transaction.homeHit)
            if (const auto way = directory.FindWay(line); way)
                directory.Invalidate(directory.SetIndex(line), *way);

        flow.requesterFinal = CacheState::I;
        snoopFilter.Track(line, transaction.request.source, CacheState::I, time);
        flow.directoryCommitted = true;
        return true;
    }

}
