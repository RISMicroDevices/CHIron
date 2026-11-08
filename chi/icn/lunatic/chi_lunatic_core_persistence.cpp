#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {

    void Core::Impl::ActionPersistence(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
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
            request.target = SelectSubordinate(options, request.address, request.nonSecure);
            request.txnId = transaction.handle;
            request.returnNid = 0;
            request.returnTxnId = 0;
            request.allowRetry = true;
            request.expCompAck = false;
            request.snpAttr = 0;
            request.doDwt = false;
            request.stashNidValid = false;
            request.stashLpidValid = false;
            if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
                request.returnNid = flow.directPersistActive ? transaction.request.source : nodeId;
            else
                request.pGroupId = 0;
            if (!Queue(txreq, request, transaction.handle, transaction.actionAge, false, true))
            {
                transaction.readyTime = time;
                return;
            }
            if (!tracker.BindMemory(transaction.handle, options.subordinateId, transaction.handle))
            {
                Error(transaction.handle, "unable to bind downstream persistence ID");
                transaction.canceled = true;
                transaction.phase = Phase::Commit;
                return;
            }
            flow.backendPhase = BackendPhase::WaitRequestEmission;
            transaction.phase = Phase::WaitMemory;
            transaction.readyTime = time;
            return;
        }
        if (options.backend == BackendKind::CHI && flow.backendPhase != BackendPhase::Done)
            return;

        if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep && !flow.directPersistActive)
        {
            if (flow.separatedPersistResponse)
            {
                if (!flow.compSent)
                    QueueComp(transaction, false, CHI::Eb::Resps::Comp_UC, flow.backendCompRespErr);
                else if (!flow.persistSent)
                    QueuePersist(transaction);
            }
            else
                QueueCompPersist(transaction);
        }
        else
            QueueComp(transaction, false, CHI::Eb::Resps::Comp_UC);
    }

    void Core::Impl::QueueCompPersist(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.compSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.opcode = CHI::Eb::Opcodes::RSP::CompPersist;
        response.resp = CHI::Eb::Resps::Comp_UC;
        response.respErr = transaction.responseRespErr;
        response.pGroupId = transaction.request.pGroupId;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
        {
            flow.compSent = true;
            transaction.phase = Phase::Return;
            transaction.readyTime = time;
        }
    }

    void Core::Impl::QueuePersist(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.persistSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.opcode = CHI::Eb::Opcodes::RSP::Persist;
        response.respErr = flow.backendPersistRespErr;
        response.pGroupId = transaction.request.pGroupId;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
        {
            flow.persistSent = true;
            transaction.phase = Phase::Return;
            transaction.readyTime = time;
        }
    }

}
