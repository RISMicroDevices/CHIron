#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {
    namespace {

        bool HasErrorTransactionGeometry(const Request& request) noexcept
        {
            if (request.size > 6 || request.order == 1)
                return false;
            switch (request.opcode)
            {
                case CHI::Eb::Opcodes::REQ::WriteNoSnpFull:
                case CHI::Eb::Opcodes::REQ::WriteUniqueFull:
                case CHI::Eb::Opcodes::REQ::WriteUniqueFullStash:
                case CHI::Eb::Opcodes::REQ::WriteBackFull:
                case CHI::Eb::Opcodes::REQ::WriteCleanFull:
                case CHI::Eb::Opcodes::REQ::WriteEvictFull:
                    return request.size == 6;
                default:
                    break;
            }
            switch (ClassifyRequest(request.opcode))
            {
                case RequestClass::ReadOnce:
                case RequestClass::CoherentRead:
                case RequestClass::MakeUnique:
                case RequestClass::MakeReadUnique:
                case RequestClass::ZeroWrite:
                case RequestClass::WriteUniqueZero:
                case RequestClass::WriteEvictOrEvict:
                case RequestClass::Evict:
                case RequestClass::CMO:
                case RequestClass::CleanUnique:
                case RequestClass::Persistence:
                case RequestClass::Stash:
                    return request.size == 6;
                default:
                    return true;
            }
        }

    }

    void Core::Impl::CompleteErrorTransaction(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const auto requestClass = ClassifyRequest(transaction.request.opcode);
        AtomicGeometry atomicGeometry;
        const auto bindErrorDBID = [&]() noexcept {
            if (tracker.BindDBID(transaction.handle, transaction.handle))
                return true;
            Error(transaction.handle, "unable to bind database ID");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            transaction.readyTime = time;
            return false;
        };

        Deny(transaction.request, "unsupported request opcode or transfer geometry");
        if (IsReadRequest(transaction.request.opcode) && HasErrorTransactionGeometry(transaction.request))
        {
            const TransferGeometry geometry = GetTransferGeometry(transaction.request, options);
            flow.suppressArchitecturalEffects = true;
            flow.expectedBeatMask = geometry.beatMask;
            flow.validBytes.fill(0);
            for (size_t index = geometry.validOffset; index < geometry.validOffset + geometry.validBytes; ++index)
                flow.validBytes[index] = 1;
            flow.dmtActive = false;
            flow.dctActive = false;
            flow.directoryCommitted = true;
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            transaction.responseResp = flow.separatedResponse
                ? CHI::Eb::Resps::DataSepResp_UC : CHI::Eb::Resps::CompData_I;
            transaction.sentBeats = 0;
            if (!bindErrorDBID())
                return;
            transaction.phase = Phase::Return;
            transaction.readyTime = ScheduleData();
            return;
        }
        if (requestClass == RequestClass::ImmediateWrite &&
            (!IsStashRequest(transaction.request.opcode) || transaction.request.stashNidValid) &&
            HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
            flow.dwtActive = false;
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            QueueDBID(transaction);
            return;
        }
        if (requestClass == RequestClass::Persistence && HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.directoryCommitted = true;
            flow.directPersistActive = false;
            flow.separatedPersistResponse = false;
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            QueueCompPersist(transaction);
            return;
        }
        if (requestClass == RequestClass::Stash && transaction.request.stashNidValid &&
            HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.directoryCommitted = true;
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            if (IsSeparatedStashRequest(transaction.request.opcode))
                QueueStashDone(transaction, true);
            else
                QueueComp(transaction, false,
                    transaction.homeHit ? CHI::Eb::Resps::Comp_SC : CHI::Eb::Resps::Comp_I,
                    CHI::Eb::RespErrs::NDERR);
            return;
        }
        if ((requestClass == RequestClass::ZeroWrite || requestClass == RequestClass::WriteUniqueZero) &&
            HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.directoryCommitted = true;
            transaction.Mark(COMPLETE_WRITE_DATA);
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            QueueComp(transaction, true, CHI::Eb::Resps::Comp_I, CHI::Eb::RespErrs::NDERR);
            flow.dbidSent = flow.compSent;
            return;
        }
        if ((requestClass == RequestClass::CMO || requestClass == RequestClass::CleanUnique ||
             requestClass == RequestClass::MakeUnique || requestClass == RequestClass::Evict ||
             requestClass == RequestClass::WriteEvictOrEvict) &&
            HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.directoryCommitted = true;
            flow.evictOrEvictNoData = requestClass == RequestClass::WriteEvictOrEvict;
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            const uint8_t resp = requestClass == RequestClass::CleanUnique ||
                requestClass == RequestClass::MakeUnique ? CHI::Eb::Resps::Comp_UC : CHI::Eb::Resps::Comp_I;
            QueueComp(transaction, false, resp, CHI::Eb::RespErrs::NDERR);
            return;
        }
        if (requestClass == RequestClass::CopybackWrite && HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            QueueComp(transaction, true, CHI::Eb::Resps::Comp_I, CHI::Eb::RespErrs::NDERR);
            flow.dbidSent = flow.compSent;
            return;
        }
        if (requestClass == RequestClass::Atomic && GetAtomicGeometry(transaction.request, atomicGeometry) &&
            HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.expectedBeatMask = AtomicOperandBeatMask(transaction.request, options);
            flow.atomic.resultBeatMask = AtomicResultBeatMask(transaction.request, options);
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            QueueDBID(transaction);
            return;
        }
        if (requestClass == RequestClass::NonCoherentWrite && HasErrorTransactionGeometry(transaction.request))
        {
            flow.suppressArchitecturalEffects = true;
            flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
            transaction.dbid = transaction.handle;
            transaction.responseRespErr = CHI::Eb::RespErrs::NDERR;
            if (!bindErrorDBID())
                return;
            QueueDBID(transaction);
            return;
        }
        transaction.canceled = true;
        transaction.dbid = transaction.handle;
        QueueComp(transaction, false, CHI::Eb::Resps::Comp_I, 0b10);
    }

}
