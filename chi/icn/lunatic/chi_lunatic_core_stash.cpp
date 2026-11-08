#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {
    namespace {

        uint8_t StashCompResp(const Transaction& transaction) noexcept
        {
            return transaction.homeHit ? uint8_t(CHI::Eb::Resps::Comp_SC) : uint8_t(CHI::Eb::Resps::Comp_I);
        }

    }

    void Core::Impl::ResolveStash(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        flow.snoopLine = transaction.request.Line();
        flow.responseForm = transaction.handle % 3;
        flow.stashTarget = transaction.request.returnNid;
        flow.stashTargetValid = transaction.request.stashNidValid;
        if (flow.stashTargetValid && flow.stashTarget != transaction.request.source)
            transaction.snoopTargets.push_back(flow.stashTarget);
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
    }

    void Core::Impl::ActionStash(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
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
