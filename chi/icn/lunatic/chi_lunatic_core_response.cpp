#include "chi_lunatic_core.hpp"

#include <algorithm>
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
        if (AcquireResponse(transaction, txrsp) && Queue(txrsp, response, transaction.handle, transaction.actionAge))
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
        if (AcquireResponse(transaction, txrsp) && Queue(txrsp, response, transaction.handle, transaction.actionAge))
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
        if (AcquireResponse(transaction, txrsp) && Queue(txrsp, response, transaction.handle, transaction.actionAge))
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
        response.respErr = transaction.responseRespErr;
        response.qos = transaction.request.qos;
        response.traceTag = transaction.request.traceTag;
        if (AcquireResponse(transaction, txrsp) && Queue(txrsp, response, transaction.handle, transaction.actionAge))
            flow.respSepSent = true;
    }

    size_t Core::Impl::ResponseCategory(const Response& response) const noexcept
    {
        switch (response.opcode)
        {
            case CHI::Eb::Opcodes::RSP::RetryAck:
                return 1;
            case CHI::Eb::Opcodes::RSP::PCrdGrant:
                return 2;
            case CHI::Eb::Opcodes::RSP::DBIDResp:
            case CHI::Eb::Opcodes::RSP::DBIDRespOrd:
            case CHI::Eb::Opcodes::RSP::CompDBIDResp:
                return 0;
            default:
                return 3;
        }
    }

    size_t Core::Impl::ResponseClass(const Pending<Response>& pending) const noexcept
    {
        if (pending.retry != std::numeric_limits<size_t>::max())
        {
            const auto retry = retries.find(pending.retry);
            if (retry != retries.end())
                return retry->second.classId;
        }
        if (pending.transaction != INVALID_TRANSACTION)
        {
            const auto flow = flows.find(pending.transaction);
            if (flow != flows.end())
                return flow->second.assignedClass;
        }
        return ClassForQos(pending.flit.qos);
    }

    size_t Core::Impl::SelectResponse(const std::deque<Pending<Response>>& queue,
                                      std::optional<bool> downstream, bool commit)
    {
        Time oldest = std::numeric_limits<Time>::max();
        for (const auto& pending : queue)
            if ((!downstream || pending.downstream == *downstream) && pending.readyTime <= time)
                oldest = std::min(oldest, pending.age);

        std::array<bool, 4> categories{};
        for (const auto& pending : queue)
            if ((!downstream || pending.downstream == *downstream) && pending.readyTime <= time &&
                pending.age == oldest && ResponseClass(pending) < categories.size())
                categories[ResponseCategory(pending.flit)] = true;
        const auto& classes = options.structure.classes;
        const std::array<uint64_t, 4> categoryWeights = {
            classes.fastResponseWeight, classes.retryResponseWeight,
            classes.staticGrantResponseWeight, classes.normalResponseWeight
        };
        const size_t category = SelectWeighted(categoryWeights, categories, responseCategoryRound, false);
        if (category == categories.size())
            return queue.size();

        std::array<bool, 4> eligibleClasses{};
        for (const auto& pending : queue)
            if ((!downstream || pending.downstream == *downstream) && pending.readyTime <= time &&
                pending.age == oldest && ResponseCategory(pending.flit) == category &&
                ResponseClass(pending) < eligibleClasses.size())
                eligibleClasses[ResponseClass(pending)] = true;
        const auto& classWeights = category == 1 || category == 2 ? classes.retryWeights : classes.requestWeights;
        WeightedRoundState& classRound = responseClassRounds[category];
        const size_t classId = SelectWeighted(classWeights, eligibleClasses, classRound, false);
        if (classId == eligibleClasses.size())
            return queue.size();

        size_t selected = queue.size();
        for (size_t index = 0; index < queue.size(); ++index)
        {
            const auto& pending = queue[index];
            if ((downstream && pending.downstream != *downstream) || pending.readyTime > time ||
                pending.age != oldest || ResponseCategory(pending.flit) != category ||
                ResponseClass(pending) != classId)
                continue;
            if (selected == queue.size() || pending.serial < queue[selected].serial)
                selected = index;
        }
        if (selected != queue.size() && commit)
        {
            SelectWeighted(categoryWeights, categories, responseCategoryRound, true);
            SelectWeighted(classWeights, eligibleClasses, classRound, true);
        }
        return selected;
    }

    void Core::Impl::QueueRetryCredits()
    {
        if (!options.structure.classes.enabled)
        {
            for (const auto& [index, retry] : retries)
            {
                (void)retry;
                QueueRetryCredit(index);
            }
            return;
        }

        Time oldest = std::numeric_limits<Time>::max();
        for (const auto& [index, retry] : retries)
        {
            (void)index;
            if (RetryCreditEligible(retry))
                oldest = std::min(oldest, retry.age);
        }
        size_t tier = retryRounds.size();
        for (const auto& [index, retry] : retries)
        {
            (void)index;
            if (RetryCreditEligible(retry) && retry.age == oldest)
                tier = std::min(tier, RetryTier(retry.bank, retry.classId));
        }
        if (tier == retryRounds.size())
            return;

        std::array<bool, 4> eligibleClasses{};
        for (const auto& [index, retry] : retries)
        {
            (void)index;
            if (RetryCreditEligible(retry) && retry.age == oldest &&
                RetryTier(retry.bank, retry.classId) == tier)
                eligibleClasses[retry.classId] = true;
        }
        WeightedRoundState& round = retryRounds[tier];
        const size_t classId = SelectWeighted(options.structure.classes.retryWeights, eligibleClasses, round, false);
        if (classId == eligibleClasses.size())
            return;
        size_t selected = std::numeric_limits<size_t>::max();
        for (const auto& [index, retry] : retries)
            if (RetryCreditEligible(retry) && retry.age == oldest && retry.classId == classId &&
                RetryTier(retry.bank, retry.classId) == tier)
            {
                selected = index;
                break;
            }
        if (selected != std::numeric_limits<size_t>::max() && QueueRetryCredit(selected))
            SelectWeighted(options.structure.classes.retryWeights, eligibleClasses, round, true);
    }

    bool Core::Impl::QueueRetryCredit(size_t index)
    {
        auto it = retries.find(index);
        if (it == retries.end() || !RetryCreditEligible(it->second))
            return false;

        RetryRecord& retry = it->second;
        Response response;
        response.source = nodeId;
        response.target = retry.request.source;
        response.opcode = CHI::Eb::Opcodes::RSP::PCrdGrant;
        response.pCrdType = retry.request.pCrdType;
        response.qos = retry.request.qos;
        response.traceTag = retry.request.traceTag;
        if (!Queue(txrsp, response, INVALID_TRANSACTION, nextAge++, false, false, index))
            return false;

        retry.grantQueued = true;
        retry.creditReserved = true;
        if (options.structure.classes.enabled)
            ++banks[retry.bank].retryReservations[retry.classId];
        return true;
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
            {
                if (!iter->second.creditReturned)
                    ReleaseRetrySource(iter->second.request.source, iter->second.bank, iter->second.classId);
                iter = retries.erase(iter);
            }
            else
                ++iter;
        }
        cancellationCredits.erase({ source, pCrdType });
    }

    bool Core::Impl::RetryReservationAvailable() const noexcept
    {
        if (options.structure.classes.enabled)
            return tracker.Size() + TotalRetryReservations() < OrdinaryTransactionLimit();
        size_t reserved = 0;
        for (const auto& [key, count] : availableCredits)
        {
            (void)key;
            reserved += count;
        }
        for (const auto& pending : txrsp)
            reserved += pending.flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant ? 1 : 0;
        return tracker.Size() + reserved < OrdinaryTransactionLimit();
    }

    bool Core::Impl::ConsumeRetryRequest(const Request& request, bool creditReturn)
    {
        if (!options.structure.classes.enabled)
        {
            const auto creditKey = std::make_pair(request.source, request.pCrdType);
            auto credit = availableCredits.find(creditKey);
            if (credit == availableCredits.end() || credit->second == 0)
                return false;

            size_t originalIndex = std::numeric_limits<size_t>::max();
            if (!creditReturn)
            {
                const auto original = SelectRetryIf([&](const auto& iter) {
                    return RetryRequestCompatible(iter->second.request, request, nodeId);
                });
                if (original != retries.end())
                    originalIndex = original->first;
            }
            else
            {
                if (RetryOriginals(request.source, request.pCrdType) == 0)
                    return false;
                const auto original = SelectRetryIf([&](const auto& iter) {
                    return iter->second.request.source == request.source &&
                        iter->second.request.pCrdType == request.pCrdType;
                });
                if (original != retries.end())
                    originalIndex = original->first;
            }
            if (originalIndex == std::numeric_limits<size_t>::max())
                return false;

            const size_t originalBank = retries.at(originalIndex).bank;
            const size_t originalClass = retries.at(originalIndex).classId;
            if (!creditReturn && !ClassAdmissionAvailable(request, originalClass, true))
                return false;

            const auto donor = SelectRetryIf([&](const auto& iter) {
                return iter->second.request.source == request.source &&
                    iter->second.request.pCrdType == request.pCrdType &&
                    iter->second.creditReserved && !iter->second.grantQueued;
            });
            if (donor != retries.end())
                donor->second.creditReserved = false;
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
            ReleaseRetrySource(request.source, originalBank, originalClass);
            PurgeReturnedRetries(request.source, request.pCrdType);
            return true;
        }

        const auto creditKey = std::make_pair(request.source, request.pCrdType);
        const auto credit = availableCredits.find(creditKey);
        if (credit == availableCredits.end() || credit->second == 0)
            return false;

        size_t originalIndex = std::numeric_limits<size_t>::max();
        size_t donorIndex = std::numeric_limits<size_t>::max();
        if (!creditReturn)
        {
            const auto original = SelectRetryIf([&](const auto& iter) {
                return RetryRequestCompatible(iter->second.request, request, nodeId);
            });
            if (original != retries.end())
                originalIndex = original->first;
            if (originalIndex == std::numeric_limits<size_t>::max())
                return false;
            const RetryRecord& originalRecord = retries.at(originalIndex);
            if (originalRecord.creditReserved && !originalRecord.grantQueued)
                donorIndex = originalIndex;
        }
        else if (RetryOriginals(request.source, request.pCrdType) == 0)
            return false;

        const auto donorMatch = SelectRetryIf([&](const auto& iter) {
            return donorIndex == std::numeric_limits<size_t>::max() &&
                iter->second.request.source == request.source &&
                iter->second.request.pCrdType == request.pCrdType &&
                iter->second.creditReserved && !iter->second.grantQueued;
        });
        if (donorMatch != retries.end())
            donorIndex = donorMatch->first;
        if (donorIndex == std::numeric_limits<size_t>::max())
            return false;
        if (creditReturn)
            originalIndex = donorIndex;

        RetryRecord& original = retries.at(originalIndex);
        RetryRecord& donor = retries.at(donorIndex);
        if (!creditReturn && !ClassAdmissionAvailable(request, original.classId, true))
            return false;

        size_t requiredCredits = 1;
        size_t queuedGrant = txrsp.size();
        if (originalIndex != donorIndex && original.creditReserved)
        {
            if (original.grantQueued)
            {
                for (size_t index = 0; index < txrsp.size(); ++index)
                    if (txrsp[index].retry == originalIndex &&
                        txrsp[index].flit.opcode == CHI::Eb::Opcodes::RSP::PCrdGrant)
                    {
                        queuedGrant = index;
                        break;
                    }
                if (queuedGrant == txrsp.size())
                    return false;
            }
            else
                ++requiredCredits;
        }
        if (credit->second < requiredCredits)
            return false;

        const auto consumeCredit = [&]() {
            auto pooled = availableCredits.find(creditKey);
            if (--pooled->second == 0)
                availableCredits.erase(pooled);
        };
        const auto releaseReservation = [&](RetryRecord& retry) {
            if (!retry.creditReserved)
                return;
            retry.creditReserved = false;
            if (options.structure.classes.enabled)
                --banks[retry.bank].retryReservations[retry.classId];
        };

        consumeCredit();
        releaseReservation(donor);
        if (originalIndex != donorIndex && original.creditReserved)
        {
            if (original.grantQueued)
            {
                txrsp.erase(txrsp.begin() + queuedGrant);
                original.grantQueued = false;
            }
            else
                consumeCredit();
            releaseReservation(original);
        }

        const size_t originalBank = original.bank;
        const size_t originalClass = original.classId;
        if (creditReturn)
        {
            original.creditReturned = true;
            ++cancellationCredits[creditKey];
            ++pmu.returnedCredits;
        }
        else
        {
            retries.erase(originalIndex);
            ++pmu.creditedResends;
        }
        ReleaseRetrySource(request.source, originalBank, originalClass);
        PurgeReturnedRetries(request.source, request.pCrdType);
        return true;
    }

    void Core::Impl::ReturnRead(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (transaction.canceled)
            return;
        if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::MakeReadUnique && flow.compSent)
            return;
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
        if (transaction.sentBeats != 0 && !ScheduleTransition(transaction, SchedulePoint::DataBeat))
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
        if (transaction.responseRespErr != CHI::Eb::RespErrs::NDERR)
            if (const auto error = flow.backendBeatRespErr.find(beat); error != flow.backendBeatRespErr.end())
                data.respErr = error->second;
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

        if (AcquireResponse(transaction, txdat) && Queue(txdat, data, transaction.handle, transaction.actionAge, remaining == 0))
            ++transaction.sentBeats;
    }

    void Core::Impl::FinishResponse(Transaction& transaction)
    {
        const auto requestClass = ClassifyRequest(transaction.request.opcode);
        const FlowState& flow = flows.at(transaction.handle);
        const bool canceledResponsePending = transaction.canceled &&
            ((flow.dbidSent && !flow.dbidEmitted) || (flow.compSent && !flow.compEmitted) ||
             (flow.readReceiptSent && !flow.readReceiptEmitted) || (flow.respSepSent && !flow.respSepEmitted) ||
             (flow.stashDoneSent && !flow.stashDoneEmitted) || (flow.persistSent && !flow.persistEmitted) ||
             (flow.atomic.returnSent && !flow.atomic.returnEmitted));
        if (canceledResponsePending)
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }
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
        if (!transaction.canceled && RequiresSeparatedPersist(transaction) &&
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
            ReleaseResponseIfComplete(transaction);
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
        ReleaseResponseIfComplete(transaction);
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
