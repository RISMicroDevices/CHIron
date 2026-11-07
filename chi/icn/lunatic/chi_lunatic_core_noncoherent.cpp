#include "chi_lunatic_core.hpp"

#include <bit>

namespace CHI::Lunatic {

    void Core::Impl::ResolveNonCoherent(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::NonCoherentRead:
            {
                const TransferGeometry geometry = GetTransferGeometry(transaction.request, options);
                flow.expectedBeatMask = geometry.beatMask;
                flow.validBytes.fill(0);
                for (size_t index = geometry.validOffset; index < geometry.validOffset + geometry.validBytes; ++index)
                    flow.validBytes[index] = 1;
                if (transaction.request.order != 0 && !flow.separatedResponse && !flow.readReceiptSent)
                {
                    QueueReadReceipt(transaction);
                    if (!flow.readReceiptSent)
                    {
                        transaction.readyTime = time;
                        return;
                    }
                }
                StartMemoryRead(transaction);
                return;
            }
            case RequestClass::NonCoherentWrite:
                flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
                pipeline.Advance(transaction, Phase::Action, time);
                return;
            case RequestClass::ZeroWrite:
                transaction.data.fill(0);
                transaction.byteEnables.fill(1);
                GenerateMetadata(transaction.poison, transaction.dataCheck, transaction.data,
                                 transaction.byteEnables, options);
                StartBackendWrite(transaction);
                return;
            default:
                Error(transaction.handle, "invalid non-coherent request class");
                transaction.canceled = true;
                transaction.phase = Phase::Commit;
                transaction.readyTime = time;
                return;
        }
    }

    void Core::Impl::ActionNonCoherent(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::NonCoherentRead:
                transaction.responseResp = flow.separatedResponse
                    ? CHI::Eb::Resps::DataSepResp_UC : CHI::Eb::Resps::CompData_I;
                transaction.phase = Phase::Return;
                transaction.readyTime = time;
                transaction.sentBeats = 0;
                return;
            case RequestClass::NonCoherentWrite:
                if (flow.suppressArchitecturalEffects)
                {
                    if (!flow.dbidSent)
                    {
                        QueueDBID(transaction);
                        return;
                    }
                    if (!transaction.Complete(COMPLETE_WRITE_DATA))
                    {
                        transaction.phase = Phase::WaitWriteData;
                        transaction.readyTime = time;
                        return;
                    }
                    QueueComp(transaction, false, CHI::Eb::Resps::Comp_I, CHI::Eb::RespErrs::NDERR);
                    return;
                }
                if (!flow.dbidSent)
                {
                    QueueDBID(transaction);
                    return;
                }
                if (!transaction.Complete(COMPLETE_WRITE_DATA))
                {
                    transaction.phase = Phase::WaitWriteData;
                    transaction.readyTime = time;
                    return;
                }
                if (IsOrderedWriteObservation(transaction.request) && !transaction.Complete(COMPLETE_COMPACK))
                {
                    transaction.phase = Phase::WaitCompAck;
                    transaction.wait = WaitReason::CompAck;
                    transaction.readyTime = time;
                    return;
                }
                if (flow.backendPhase == BackendPhase::None)
                {
                    StartBackendWrite(transaction);
                    return;
                }
                if (flow.backendPhase == BackendPhase::SendData)
                {
                    AdvanceBackendWrite(transaction);
                    return;
                }
                if (flow.backendPhase == BackendPhase::Done)
                    QueueComp(transaction);
                return;
            case RequestClass::ZeroWrite:
                if (flow.backendPhase == BackendPhase::Done)
                    QueueComp(transaction, true);
                return;
            default:
                Error(transaction.handle, "invalid non-coherent action");
                transaction.canceled = true;
                transaction.phase = Phase::Commit;
                transaction.readyTime = time;
                return;
        }
    }

    bool Core::Impl::HandleWriteDAT(Transaction& transaction, const Data& flit)
    {
        namespace DAT = CHI::Eb::Opcodes::DAT;
        FlowState& flow = flows.at(transaction.handle);
        const auto requestClass = ClassifyRequest(transaction.request.opcode);
        if (requestClass == RequestClass::Atomic)
            return HandleAtomicDAT(transaction, flit);
        const bool actionWrite = transaction.phase == Phase::Action && flow.dbidEmitted &&
            (requestClass == RequestClass::NonCoherentWrite || requestClass == RequestClass::ImmediateWrite);
        if (transaction.phase != Phase::WaitWriteData && !actionWrite)
            return false;
        const bool copyback = IsCopybackRequest(transaction.request.opcode);
        if (copyback)
        {
            if (flit.opcode != DAT::CopyBackWrData || flit.txnId != transaction.dbid)
                return false;
        }
        else if ((requestClass != RequestClass::NonCoherentWrite && requestClass != RequestClass::ImmediateWrite) ||
                 (flit.opcode != DAT::NonCopyBackWrData && flit.opcode != DAT::NCBWrDataCompAck &&
                  flit.opcode != DAT::WriteDataCancel))
            return false;
        if (flit.txnId != transaction.dbid || (!copyback && flit.resp != 0) ||
            (flit.opcode == DAT::NCBWrDataCompAck && !transaction.request.expCompAck))
            return false;

        const TransferGeometry geometry = GetTransferGeometry(transaction.request, options);
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = flit.ByteOffset();
        if (offset + bytes > LINE_SIZE || offset % bytes != 0 || flit.ccid != geometry.ccid)
            return false;
        const size_t beat = offset / bytes;
        const uint64_t bit = uint64_t(1) << beat;
        namespace REQ = CHI::Eb::Opcodes::REQ;
        const bool fullWrite = transaction.request.opcode == REQ::WriteNoSnpFull ||
            transaction.request.opcode == REQ::WriteBackFull || transaction.request.opcode == REQ::WriteCleanFull ||
            transaction.request.opcode == REQ::WriteEvictFull || transaction.request.opcode == REQ::WriteUniqueFull ||
            transaction.request.opcode == REQ::WriteUniqueFullStash;
        for (size_t index = 0; index < bytes; ++index)
        {
            const bool enabled = flit.byteEnables[index] != 0;
            if ((!enabled && flit.bytes[index] != 0) ||
                (enabled && (offset + index < geometry.validOffset ||
                             offset + index >= geometry.validOffset + geometry.validBytes)) ||
                (fullWrite && !enabled && !(copyback && flit.resp == CHI::Eb::Resps::CopyBackWrData_I)) ||
                (copyback && flit.resp == CHI::Eb::Resps::CopyBackWrData_I && enabled))
                return false;
        }
        if (flit.opcode == DAT::WriteDataCancel)
        {
            if ((transaction.request.opcode != REQ::WriteNoSnpPtl && transaction.request.opcode != REQ::WriteUniquePtl &&
                 transaction.request.opcode != REQ::WriteUniquePtlStash) ||
                CHI::Eb::MemAttrs::ExtractDevice(transaction.request.memAttr) || transaction.receivedBeats != 0 ||
                !(flow.expectedBeatMask & bit) || (flow.cancelBeatMask & bit))
                return false;
            for (uint8_t enabled : flit.byteEnables)
                if (enabled)
                    return false;

            flow.cancelBeatMask |= bit;
            if ((flow.cancelBeatMask & flow.expectedBeatMask) != flow.expectedBeatMask)
                return true;

            transaction.canceled = true;
            transaction.receivedBeats = flow.expectedBeatMask;
            transaction.Mark(COMPLETE_WRITE_DATA);
            transaction.phase = Phase::Action;
            transaction.wait = WaitReason::None;
            transaction.readyTime = time;
            return true;
        }
        if (transaction.canceled || !(flow.expectedBeatMask & bit) || (transaction.receivedBeats & bit))
            return false;
        if (transaction.receivedBeats == 0)
            flow.writeResp = flit.resp;
        else if (flow.writeResp != flit.resp)
            return false;

        for (size_t index = 0; index < bytes; ++index)
        {
            if (flit.byteEnables[index])
                transaction.data[offset + index] = flit.bytes[index];
            transaction.byteEnables[offset + index] = flit.byteEnables[index];
            if (ClassifyRequest(transaction.request.opcode) == RequestClass::ImmediateWrite && flit.byteEnables[index])
                flow.validBytes[offset + index] = 1;
        }
        const ByteEnables enables = ExpandedEnables(flit.byteEnables, offset, bytes);
        MergeMetadata(transaction.poison, transaction.dataCheck, flit.poison, flit.dataCheck,
                      enables, offset, 0, bytes, options);
        flow.writeRespErr[beat] = flit.respErr;
        flow.writeRespErrMask |= bit;

        transaction.receivedBeats |= bit;
        if ((transaction.receivedBeats & flow.expectedBeatMask) != flow.expectedBeatMask)
            return true;

        transaction.Mark(COMPLETE_WRITE_DATA);
        if (flit.opcode == DAT::NCBWrDataCompAck)
            transaction.Mark(COMPLETE_COMPACK);
        transaction.phase = Phase::Action;
        transaction.wait = WaitReason::None;
        transaction.readyTime = time;
        return true;
    }

}
