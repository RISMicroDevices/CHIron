#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {
    namespace {

        uint8_t StashCompResp(const Transaction& transaction) noexcept
        {
            return transaction.homeHit ? uint8_t(CHI::Eb::Resps::Comp_SC) : uint8_t(CHI::Eb::Resps::Comp_I);
        }

    }

    bool Core::Impl::PrepareTargetlessStash(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.directoryCommitted)
            return true;
        if (flow.targetlessSharedPeer)
        {
            transaction.homeHit = true;
            flow.directoryCommitted = true;
            return true;
        }
        if (transaction.homeHit)
        {
            flow.directoryCommitted = true;
            return true;
        }
        if (!flow.snoopDataValid && !transaction.Complete(COMPLETE_MEMORY))
        {
            StartMemoryRead(transaction);
            return false;
        }
        if (!PrepareVictim(transaction))
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return false;
        }

        const std::optional<size_t> currentWay = directory.FindWay(transaction.request.Line());
        const size_t way = currentWay ? *currentWay : flow.victimWay;
        HomeLine& home = InstallHome(transaction.request.Line(), way, transaction.data,
            flow.homeDirty || flow.dataDirty, time);
        CopyMetadata(home, transaction, options);
        home.validBytes = flow.validBytes;
        transaction.homeHit = true;
        flow.directoryCommitted = true;
        return true;
    }

    void Core::Impl::ResolveStash(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        flow.snoopLine = transaction.request.Line();
        flow.responseForm = transaction.handle % 3;
        flow.stashTarget = transaction.request.returnNid;
        flow.stashTargetValid = transaction.request.stashNidValid;
        if (flow.stashTargetValid)
        {
            if (flow.stashTarget != transaction.request.source)
                transaction.snoopTargets.push_back(flow.stashTarget);
        }
        else
        {
            for (NodeId target : SnoopTargets(transaction.request.Line()))
            {
                const CacheState state = snoopFilter.State(flow.snoopLine, target);
                flow.targetlessSharedPeer |= state == CacheState::SC || state == CacheState::SD;
                if (IsUnique(state))
                    transaction.snoopTargets.push_back(target);
            }
            if (transaction.snoopTargets.empty() && !transaction.homeHit && !flow.targetlessSharedPeer)
                StartMemoryRead(transaction);
        }
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        if (transaction.phase != Phase::WaitMemory)
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
        }
    }

    void Core::Impl::ActionStash(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (!flow.stashTargetValid && !PrepareTargetlessStash(transaction))
        {
            if (transaction.phase != Phase::WaitMemory)
            {
                transaction.phase = Phase::Action;
                transaction.readyTime = time;
            }
            return;
        }
        snoopFilter.Track(transaction.request.Line(), transaction.request.source, CacheState::I, time);
        if (!IsSeparatedStashRequest(transaction.request.opcode))
        {
            if (!flow.compSent)
                QueueComp(transaction, false, StashCompResp(transaction));
            return;
        }
        if (flow.compSent && flow.stashDoneSent)
        {
            transaction.phase = Phase::Return;
            transaction.readyTime = time;
            return;
        }

        switch (flow.responseForm)
        {
            case 0:
                if (!flow.compSent)
                    QueueComp(transaction, false, StashCompResp(transaction));
                if (!flow.stashDoneSent)
                    QueueStashDone(transaction);
                break;
            case 1:
                if (!flow.stashDoneSent)
                    QueueStashDone(transaction);
                if (!flow.compSent)
                    QueueComp(transaction, false, StashCompResp(transaction));
                break;
            default:
                if (!flow.compSent)
                    QueueStashDone(transaction, true);
                break;
        }
    }

    void Core::Impl::QueueStashDone(Transaction& transaction, bool withComp)
    {
        FlowState& flow = flows.at(transaction.handle);
        if ((withComp && flow.compSent) || (!withComp && flow.stashDoneSent))
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = withComp ? transaction.request.txnId : 0;
        response.opcode = withComp ? CHI::Eb::Opcodes::RSP::CompStashDone : CHI::Eb::Opcodes::RSP::StashDone;
        response.resp = withComp ? uint8_t(StashCompResp(transaction)) : uint8_t(0);
        response.respErr = transaction.responseRespErr;
        response.stashGroupId = transaction.request.stashGroupId;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
        {
            flow.stashDoneSent = true;
            if (withComp)
                flow.compSent = true;
            transaction.phase = Phase::Return;
            transaction.readyTime = time;
        }
    }

}
