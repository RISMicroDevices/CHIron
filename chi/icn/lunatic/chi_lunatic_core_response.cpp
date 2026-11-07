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
        response.opcode = transaction.request.order != 0
            ? CHI::Eb::Opcodes::RSP::DBIDRespOrd : CHI::Eb::Opcodes::RSP::DBIDResp;
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
        response.respErr = respErr ? respErr : transaction.responseRespErr;
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

    void Core::Impl::QueueTagMatch(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.tagMatchSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.opcode = CHI::Eb::Opcodes::RSP::TagMatch;
        response.resp = CHI::Eb::Resps::Fail;
        response.respErr = CHI::Eb::RespErrs::OK;
        response.tagOp = CHI::Eb::TagOp::Invalid;
        response.tagGroupId = transaction.request.tagGroupId;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
            flow.tagMatchSent = true;
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

    void Core::Impl::QueueRespSepData(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.respSepSent)
            return;

        Response response;
        response.source = nodeId;
        response.target = transaction.request.source;
        response.txnId = transaction.request.txnId;
        response.dbid = transaction.dbid;
        response.opcode = CHI::Eb::Opcodes::RSP::RespSepData;
        response.resp = 0;
        response.respErr = transaction.responseRespErr == CHI::Eb::RespErrs::DERR
            ? uint8_t(CHI::Eb::RespErrs::OK) : transaction.responseRespErr;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (Queue(txrsp, response, transaction.handle, transaction.actionAge))
            flow.respSepSent = true;
    }

    void Core::Impl::QueueRetryCredit(size_t index)
    {
        auto it = retries.find(index);
        if (it == retries.end() || !it->second.ackPopped || it->second.grantQueued)
            return;

        const NodeId source = it->second.request.source;
        const uint8_t pCrdType = it->second.request.pCrdType;
        if (OutstandingRetryCredits(source, pCrdType) >= RetryOriginals(source, pCrdType))
            return;

        Response response;
        response.source = nodeId;
        response.target = source;
        response.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
        response.pCrdType = pCrdType;
        response.traceTag = it->second.request.traceTag;
        if (Queue(txrsp, response, INVALID_TRANSACTION, nextAge++, false, false, index))
            it->second.grantQueued = true;
    }

    size_t Core::Impl::OutstandingRetryCredits(NodeId source, uint8_t pCrdType) const noexcept
    {
        size_t count = 0;
        if (const auto credit = availableCredits.find({ source, pCrdType }); credit != availableCredits.end())
            count += credit->second;
        for (const auto& pending : txrsp)
        {
            count += pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant &&
                pending.flit.target == source && pending.flit.pCrdType == pCrdType ? 1 : 0;
        }
        return count;
    }

    size_t Core::Impl::RetryOriginals(NodeId source, uint8_t pCrdType) const noexcept
    {
        size_t originals = 0;
        for (const auto& [index, retry] : retries)
        {
            (void)index;
            originals += retry.request.source == source && retry.request.pCrdType == pCrdType ? 1 : 0;
        }
        const auto canceled = cancellationCredits.find({ source, pCrdType });
        const size_t cancellations = canceled == cancellationCredits.end() ? 0 : canceled->second;
        return originals > cancellations ? originals - cancellations : 0;
    }

    void Core::Impl::PurgeReturnedRetries(NodeId source, uint8_t pCrdType)
    {
        if (RetryOriginals(source, pCrdType) != 0)
            return;
        for (auto iter = retries.begin(); iter != retries.end();)
        {
            if (iter->second.request.source == source && iter->second.request.pCrdType == pCrdType)
                iter = retries.erase(iter);
            else
                ++iter;
        }
        cancellationCredits.erase({ source, pCrdType });
    }

    bool Core::Impl::RetryReservationAvailable() const noexcept
    {
        size_t reserved = 0;
        for (const auto& [key, count] : availableCredits)
        {
            (void)key;
            reserved += count;
        }
        for (const auto& pending : txrsp)
            reserved += pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant ? 1 : 0;
        return tracker.Size() + reserved < tracker.Limit();
    }

    bool Core::Impl::ConsumeRetryRequest(const Request& request, bool creditReturn)
    {
        const auto creditKey = std::make_pair(request.source, request.pCrdType);
        auto credit = availableCredits.find(creditKey);
        if (credit == availableCredits.end() || credit->second == 0)
            return false;

        size_t originalIndex = std::numeric_limits<size_t>::max();
        if (!creditReturn)
        {
            for (auto iter = retries.rbegin(); iter != retries.rend(); ++iter)
            {
                if (!RetryRequestCompatible(iter->second.request, request, nodeId))
                    continue;
                originalIndex = iter->first;
                break;
            }
            if (originalIndex == std::numeric_limits<size_t>::max())
                return false;
        }
        else if (RetryOriginals(request.source, request.pCrdType) == 0)
            return false;

        if (--credit->second == 0)
            availableCredits.erase(credit);
        if (creditReturn)
        {
            ++cancellationCredits[creditKey];
            ++pmu.returnedCredits;
        }
        else
        {
            retries.erase(originalIndex);
            ++pmu.creditedResends;
        }
        PurgeReturnedRetries(request.source, request.pCrdType);
        return true;
    }

    void Core::Impl::ReturnRead(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (transaction.request.order != 0 && !flow.separatedResponse && !flow.readReceiptEmitted)
        {
            if (!flow.readReceiptSent)
                QueueReadReceipt(transaction);
            return;
        }
        if (flow.separatedResponse && !flow.respSepSent)
        {
            QueueRespSepData(transaction);
            if (!flow.respSepSent)
                return;
        }
        if (flow.dmtActive)
        {
            if (flow.separatedResponse ? flow.respSepEmitted
                                       : transaction.request.order == 0 || flow.readReceiptEmitted)
                FinishResponse(transaction);
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
        data.opcode = flow.separatedResponse
            ? CHI::Eb::Opcodes::DAT::DataSepResp : CHI::Eb::Opcodes::DAT::CompData;
        data.resp = transaction.responseResp;
        data.respErr = transaction.responseRespErr;
        const bool snoopErrorValid = flow.snoopDataRespErrMask & (uint64_t(1) << beat);
        const bool backendErrorValid = flow.backendReadRespErrMask & (uint64_t(1) << beat);
        if (snoopErrorValid || backendErrorValid)
        {
            data.respErr = transaction.responseRespErr == CHI::Eb::RespErrs::NDERR
                ? uint8_t(CHI::Eb::RespErrs::NDERR) : uint8_t(CHI::Eb::RespErrs::OK);
            if (snoopErrorValid)
                data.respErr = MergeResponseErrors(data.respErr, flow.snoopDataRespErr[beat]);
            if (backendErrorValid)
                data.respErr = MergeResponseErrors(data.respErr, flow.backendReadRespErr[beat]);
        }
        data.dataId = offset / 16;
        data.ccid = (transaction.request.address >> 4) & 3;
        data.qos = transaction.request.qos;
        data.traceTag = transaction.request.traceTag;
        const bool device = CHI::Eb::MemAttrs::ExtractDevice(transaction.request.memAttr) != 0;
        for (size_t index = 0; index < bytes; ++index)
        {
            data.bytes[index] = device && !flow.validBytes[offset + index] ? 0 : transaction.data[offset + index];
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
        if (!transaction.canceled && flow.dwtActive &&
            (!flow.directoryCommitted || !flow.backendCompReceived || !flow.compEmitted))
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }
        if (!transaction.canceled && !flow.dwtActive &&
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
        if (!transaction.canceled && requestClass == RequestClass::Persistence &&
            transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep &&
            flow.separatedPersistResponse && !flow.directPersistActive &&
            (!flow.compEmitted || !flow.persistEmitted))
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }
        if (!transaction.canceled && flow.dctForwarded)
        {
            if (!flow.directoryCommitted)
            {
                transaction.phase = Phase::Action;
                transaction.wait = WaitReason::None;
                transaction.readyTime = time;
                return;
            }
            transaction.Mark(COMPLETE_RESPONSE);
            transaction.wait = RequiresTransactionCompAck(transaction) && !transaction.Complete(COMPLETE_COMPACK)
                ? WaitReason::CompAck : WaitReason::None;
            transaction.phase = transaction.wait == WaitReason::CompAck ? Phase::WaitCompAck : Phase::Commit;
            transaction.readyTime = time;
            return;
        }
        if (!transaction.canceled && flow.dmtActive && !flow.backendReadReceiptReceived)
        {
            transaction.phase = Phase::Return;
            transaction.wait = WaitReason::None;
            transaction.readyTime = time;
            return;
        }
        if (!transaction.canceled && flow.separatedResponse)
        {
            const uint64_t mask = flow.expectedBeatMask
                ? flow.expectedBeatMask : (uint64_t(1) << BeatsPerLine(options)) - 1;
            if (!flow.respSepEmitted || (!flow.dmtActive && flow.readBeatsEmitted != size_t(std::popcount(mask))))
            {
                transaction.phase = Phase::Return;
                transaction.wait = WaitReason::None;
                transaction.readyTime = time;
                return;
            }
        }
        transaction.Mark(COMPLETE_RESPONSE);
        transaction.wait = RequiresTransactionCompAck(transaction) && !transaction.Complete(COMPLETE_COMPACK)
            ? WaitReason::CompAck : WaitReason::None;
        transaction.phase = transaction.wait == WaitReason::CompAck ? Phase::WaitCompAck : Phase::Commit;
        transaction.readyTime = time;
    }

    bool Core::Impl::RequiresTransactionCompAck(const Transaction& transaction) const
    {
        if (transaction.canceled && !IsOrderedWriteObservation(transaction.request))
            return false;
        if (ClassifyRequest(transaction.request.opcode) == RequestClass::WriteEvictOrEvict &&
            flows.at(transaction.handle).evictOrEvictNoData)
            return true;
        return RequiresCompAck(transaction.request);
    }

}
