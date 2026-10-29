#include "chi_lunatic_core.hpp"

#include <bit>

namespace CHI::Lunatic {

    void Core::Impl::ResolveNonCoherent(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        switch (ClassifyRequest(transaction.request.opcode))
        {
            case RequestClass::NonCoherentRead:
                flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
                if (transaction.request.order != 0 && !flow.readReceiptSent)
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
            case RequestClass::NonCoherentWrite:
                flow.expectedBeatMask = TransferBeatMask(transaction.request, options);
                pipeline.Advance(transaction, Phase::Action, time);
                return;
            case RequestClass::ZeroWrite:
                transaction.data.fill(0);
                transaction.byteEnables.fill(1);
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
                transaction.responseResp = CHI::Eb::Resps::CompData_I;
                transaction.phase = Phase::Return;
                transaction.readyTime = time;
                transaction.sentBeats = 0;
                return;
            case RequestClass::NonCoherentWrite:
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
        if (transaction.phase != Phase::WaitWriteData)
            return false;
        if (IsCopybackRequest(transaction.request.opcode))
        {
            if (flit.opcode != DAT::CopyBackWrData || flit.txnId != transaction.dbid)
                return false;
            if (transaction.receivedBeats == 0)
                flow.writeResp = flit.resp;
            else if (flow.writeResp != flit.resp)
                return false;
        }
        else if (ClassifyRequest(transaction.request.opcode) != RequestClass::NonCoherentWrite ||
                 (flit.opcode != DAT::NonCopyBackWrData && flit.opcode != DAT::NCBWrDataCompAck &&
                  flit.opcode != DAT::WriteDataCancel))
            return false;
        if (flit.txnId != transaction.dbid)
            return false;

        const size_t bytes = BytesPerBeat(options);
        const size_t offset = flit.ByteOffset();
        if (offset + bytes > LINE_SIZE || offset % bytes != 0)
            return false;
        if (flit.opcode == DAT::WriteDataCancel)
        {
            transaction.canceled = true;
            transaction.receivedBeats = flow.expectedBeatMask;
            transaction.Mark(COMPLETE_WRITE_DATA);
            transaction.phase = Phase::Action;
            transaction.wait = WaitReason::None;
            transaction.readyTime = time;
            return true;
        }

        const size_t beat = offset / bytes;
        const uint64_t bit = uint64_t(1) << beat;
        if (!(flow.expectedBeatMask & bit) || (transaction.receivedBeats & bit))
            return false;

        for (size_t index = 0; index < bytes; ++index)
        {
            if (flit.byteEnables[index])
                transaction.data[offset + index] = flit.bytes[index];
            transaction.byteEnables[offset + index] = flit.byteEnables[index];
            transaction.dataCheck[offset + index] = flit.dataCheck[index];
        }
        for (size_t lane = 0; lane < bytes / 8; ++lane)
            transaction.poison[offset / 8 + lane] = flit.poison[lane];

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
