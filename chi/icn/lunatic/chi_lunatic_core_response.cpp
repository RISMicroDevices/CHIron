#include "chi_lunatic_core.hpp"

#include <bit>

namespace CHI::Lunatic {

    void Core::Impl::QueueDBID(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.dbidSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.dbid = transaction.dbid;
        response.opcode = CHI::Eb::Opcodes::RSP::DBIDResp;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
        {
            flow.dbidSent = true;
            const auto requestClass = ClassifyRequest(transaction.request.opcode);
            if ((requestClass == RequestClass::NonCoherentWrite || requestClass == RequestClass::ImmediateWrite) &&
                !transaction.Complete(COMPLETE_WRITE_DATA))
            {
                transaction.phase = Phase::WaitWriteData;
                transaction.wait = WaitReason::WriteData;
            }
            else
            {
                transaction.phase = Phase::Action;
                transaction.wait = WaitReason::None;
            }
            transaction.readyTime = time;
        }
    }

    void Core::Impl::QueueComp(Transaction& transaction, bool withDBID, uint8_t resp, uint8_t respErr)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.compSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.dbid = transaction.dbid;
        response.opcode = withDBID ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::Comp;
        response.resp = resp;
        response.respErr = respErr;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
        {
            flow.compSent = true;
            if (ClassifyRequest(transaction.request.opcode) == RequestClass::ImmediateWrite &&
                !transaction.canceled && !transaction.Complete(COMPLETE_WRITE_DATA))
            {
                transaction.phase = Phase::WaitWriteData;
                transaction.wait = WaitReason::WriteData;
            }
            else
                transaction.phase = Phase::Return;
            transaction.readyTime = time;
        }
    }

    void Core::Impl::QueueReadReceipt(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.readReceiptSent || transaction.request.order == 0)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.dbid = transaction.dbid;
        response.opcode = CHI::Eb::Opcodes::RSP::ReadReceipt;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
            flow.readReceiptSent = true;
    }

    void Core::Impl::QueueRetryCredit(size_t index)
    {
        auto it = retries.find(index);
        if (it == retries.end() || !it->second.ackPopped || it->second.grantQueued || tracker.Size() >= tracker.Limit())
            return;

        Response response;
        response.source = nodeId;
        response.target = it->second.request.source;
        response.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
        response.pCrdType = it->second.request.pCrdType;
        if (Queue(txrsp, response, INVALID_TRANSACTION, nextAge++, false, false, index))
            it->second.grantQueued = true;
    }

    void Core::Impl::ReturnRead(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (transaction.request.order != 0 && !flow.readReceiptEmitted)
        {
            if (!flow.readReceiptSent)
                QueueReadReceipt(transaction);
            return;
        }

        uint64_t mask = flow.expectedBeatMask;
        if (!mask)
            mask = (uint64_t(1) << BeatsPerLine(options)) - 1;
        if (transaction.sentBeats >= size_t(std::popcount(mask)))
            return;

        uint64_t selected = mask;
        for (size_t index = 0; index < transaction.sentBeats; ++index)
            selected &= selected - 1;
        const size_t beat = std::countr_zero(selected);
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = beat * bytes;
        const uint64_t remaining = selected & ~(uint64_t(1) << beat);

        Data data;
        data.source = nodeId;
        data.target = transaction.request.source;
        data.homeNid = nodeId;
        data.txnId = transaction.request.txnId;
        data.dbid = transaction.dbid;
        data.opcode = CHI::Eb::Opcodes::DAT::CompData;
        data.resp = transaction.responseResp;
        data.respErr = transaction.responseRespErr;
        data.dataId = offset / 16;
        data.ccid = (transaction.request.address >> 4) & 3;
        data.qos = transaction.request.qos;
        data.traceTag = transaction.request.traceTag;
        for (size_t index = 0; index < bytes; ++index)
        {
            data.bytes[index] = transaction.data[offset + index];
            data.byteEnables[index] = flow.validBytes[offset + index];
            data.dataCheck[index] = transaction.dataCheck[offset + index];
        }
        for (size_t lane = 0; lane < bytes / 8; ++lane)
            data.poison[lane] = transaction.poison[offset / 8 + lane];

        if (Queue(txdat, data, transaction.handle, transaction.actionAge, remaining == 0))
            ++transaction.sentBeats;
    }

    void Core::Impl::FinishResponse(Transaction& transaction)
    {
        const auto requestClass = ClassifyRequest(transaction.request.opcode);
        const FlowState& flow = flows.at(transaction.handle);
        if (!transaction.canceled &&
            (requestClass == RequestClass::ImmediateWrite || requestClass == RequestClass::WriteUniqueZero) &&
            (!transaction.Complete(COMPLETE_WRITE_DATA) || !flow.directoryCommitted ||
             !flow.dbidEmitted || !flow.compEmitted))
        {
            const bool responseQueueIncomplete = !flow.dbidSent || !flow.compSent;
            transaction.phase = responseQueueIncomplete || transaction.Complete(COMPLETE_WRITE_DATA)
                ? Phase::Action : Phase::WaitWriteData;
            transaction.readyTime = time;
            return;
        }
        if (!transaction.canceled && requestClass == RequestClass::Atomic)
        {
            const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction.request.opcode);
            const bool responseComplete = flow.dbidEmitted &&
                (store ? flow.compEmitted : flow.atomic.returnEmitted);
            if (!transaction.Complete(COMPLETE_WRITE_DATA) || !flow.directoryCommitted || !responseComplete)
            {
                const bool responseQueueIncomplete = !flow.dbidSent || (store && !flow.compSent);
                transaction.phase = responseQueueIncomplete || transaction.Complete(COMPLETE_WRITE_DATA)
                    ? Phase::Action : Phase::WaitWriteData;
                transaction.readyTime = time;
                return;
            }
        }
        if (!transaction.canceled && requestClass == RequestClass::Stash &&
            IsSeparatedStashRequest(transaction.request.opcode) && (!flow.compEmitted || !flow.stashDoneEmitted))
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }
        transaction.Mark(COMPLETE_RESPONSE);
        transaction.wait = RequiresTransactionCompAck(transaction) && !transaction.Complete(COMPLETE_COMPACK)
            ? WaitReason::CompAck : WaitReason::None;
        transaction.phase = transaction.wait == WaitReason::CompAck ? Phase::WaitCompAck : Phase::Commit;
        transaction.readyTime = time;
    }

    bool Core::Impl::RequiresTransactionCompAck(const Transaction& transaction) const
    {
        if (transaction.canceled)
            return false;
        if (ClassifyRequest(transaction.request.opcode) == RequestClass::WriteEvictOrEvict &&
            flows.at(transaction.handle).evictOrEvictNoData)
            return true;
        return RequiresCompAck(transaction.request);
    }

}
