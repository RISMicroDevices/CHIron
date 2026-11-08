#include "chi_lunatic_core.hpp"

#include <bit>

namespace CHI::Lunatic {
    namespace {

        void CompleteAtomicBackendIfReady(Core::Impl& impl, Transaction& transaction)
        {
            FlowState& flow = impl.flows.at(transaction.handle);
            const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction.request.opcode);
            const bool complete = store ? flow.backendCompReceived && flow.backendDataEmitted
                : flow.atomic.backendResultReceived && flow.backendDataEmitted;
            if (!complete)
                return;
            flow.backendPhase = BackendPhase::Done;
            impl.tracker.UnbindMemory(impl.options.subordinateId, transaction.handle);
            transaction.phase = Phase::Action;
            transaction.readyTime = impl.time;
        }

        ByteEnables BackendAtomicFullEnables()
        {
            ByteEnables enables{};
            enables.fill(1);
            return enables;
        }

    }

    bool Core::Impl::PrepareAtomicBackend(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        if (!flow.atomic.backendDirtyCommitted)
        {
            if ((flow.dataDirty || flow.homeDirty) && flow.snoopDataPartial && !transaction.homeHit &&
                !FetchBaseline(transaction))
                return false;
            if ((flow.dataDirty || flow.homeDirty) && !WriteLine(transaction, line, transaction.data,
                                                                 BackendAtomicFullEnables()))
                return false;
            flow.atomic.backendDirtyCommitted = true;
        }
        if (const auto way = directory.FindWay(line); way)
            directory.Invalidate(directory.SetIndex(line), *way);
        flow.requesterFinal = CacheState::I;
        snoopFilter.Track(line, transaction.request.source, CacheState::I, time);
        return true;
    }

    void Core::Impl::StartAtomicBackend(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        Request request = transaction.request;
        request.source = nodeId;
        request.target = SelectSubordinate(options, request.address, request.nonSecure);
        request.txnId = transaction.handle;
        request.returnNid = nodeId;
        request.returnTxnId = transaction.handle;
        request.allowRetry = true;
        request.expCompAck = false;
        request.order = 0;
        request.snpAttr = 0;
        request.snoopMe = false;
        request.doDwt = false;
        request.stashNidValid = false;
        request.stashLpidValid = false;
        request.likelyShared = false;
        if (!Queue(txreq, request, transaction.handle, transaction.actionAge, false, true))
        {
            transaction.readyTime = time;
            return;
        }
        if (!tracker.BindMemory(transaction.handle, options.subordinateId, transaction.handle))
        {
            Error(transaction.handle, "unable to bind downstream atomic ID");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return;
        }
        flow.expectedBeatMask = AtomicOperandBeatMask(transaction.request, options);
        flow.backendBeat = std::countr_zero(flow.expectedBeatMask);
        flow.backendBeatMask = 0;
        flow.backendCompReceived = false;
        flow.backendDataEmitted = false;
        flow.atomic.backendResultReceived = false;
        flow.atomic.backendResultDBID = 0;
        flow.backendPhase = BackendPhase::WaitRequestEmission;
        transaction.phase = Phase::WaitMemory;
        transaction.wait = WaitReason::Memory;
        transaction.readyTime = time;
    }

    void Core::Impl::ActionAtomicBackend(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.backendPhase == BackendPhase::SendData)
        {
            if (flow.spillPending)
                AdvanceBackendWrite(transaction);
            else
                AdvanceAtomicBackendDAT(transaction);
            return;
        }
        if (!transaction.Complete(COMPLETE_WRITE_DATA))
        {
            transaction.phase = Phase::WaitWriteData;
            transaction.wait = WaitReason::WriteData;
            transaction.readyTime = time;
            return;
        }
        if (!PrepareAtomicBackend(transaction))
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }
        if (flow.backendPhase == BackendPhase::None)
        {
            StartAtomicBackend(transaction);
            return;
        }
        if (flow.backendPhase != BackendPhase::Done)
            return;

        flow.directoryCommitted = true;
        if (CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction.request.opcode))
        {
            transaction.responseRespErr = MergeResponseErrors(transaction.responseRespErr,
                MergeResponseErrors(flow.atomic.operandRespErr, flow.backendCompRespErr));
            if (!flow.compSent)
                QueueComp(transaction);
            else if (flow.compEmitted)
                FinishResponse(transaction);
        }
        else
        {
            transaction.phase = Phase::Return;
            transaction.readyTime = ScheduleData();
        }
    }

    void Core::Impl::AdvanceAtomicBackendDAT(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.backendPhase != BackendPhase::SendData)
            return;
        const size_t bytes = BytesPerBeat(options);
        const size_t beat = flow.backendBeat;
        if (beat >= BeatsPerLine(options) || !(flow.expectedBeatMask & (uint64_t(1) << beat)))
        {
            Error(transaction.handle, "invalid downstream atomic beat selection");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return;
        }

        AtomicGeometry geometry;
        if (!GetAtomicGeometry(transaction.request, geometry))
        {
            Error(transaction.handle, "invalid downstream atomic geometry");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return;
        }
        const size_t offset = beat * bytes;
        Data data;
        data.source = nodeId;
        data.target = flow.backendDBIDSource;
        data.homeNid = 0;
        data.txnId = flow.backendDBID;
        data.dbid = 0;
        data.opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.respErr = (flow.writeRespErrMask & (uint64_t(1) << beat)) ? flow.writeRespErr[beat] : 0;
        data.dataId = offset / 16;
        data.ccid = (transaction.request.address >> 4) & 3;
        data.qos = transaction.request.qos;
        data.traceTag = flow.backendWriteTraceTag;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            data.bytes[index] = flow.atomic.operands[lineOffset];
            data.byteEnables[index] = lineOffset >= geometry.windowOffset &&
                lineOffset < geometry.windowOffset + geometry.windowBytes;
            data.dataCheck[index] = flow.atomic.operandsDataCheck[lineOffset];
        }
        for (size_t lane = 0; lane < bytes / 8; ++lane)
            data.poison[lane] = flow.atomic.operandsPoison[offset / 8 + lane];

        const uint64_t remaining = flow.expectedBeatMask & ~(flow.backendBeatMask | (uint64_t(1) << beat));
        if (!Queue(txdat, data, transaction.handle, transaction.actionAge, remaining == 0, true))
        {
            transaction.readyTime = time;
            return;
        }
        flow.backendBeatMask |= uint64_t(1) << beat;
        if (remaining == 0)
        {
            flow.backendPhase = BackendPhase::WaitDataEmission;
            transaction.phase = Phase::WaitMemory;
        }
        else
            flow.backendBeat = std::countr_zero(remaining);
        transaction.readyTime = time;
    }

    bool Core::Impl::HandleAtomicBackendRSP(Transaction& transaction, const Response& flit)
    {
        namespace RSP = CHI::Eb::Opcodes::RSP;
        FlowState& flow = flows.at(transaction.handle);
        const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction.request.opcode);
        if (flit.opcode == RSP::DBIDResp || flit.opcode == RSP::CompDBIDResp)
        {
            if (flow.backendPhase != BackendPhase::WaitDBID || (!store && flit.opcode != RSP::DBIDResp) ||
                (flow.atomic.backendResultReceived && flow.atomic.backendResultDBID != flit.dbid))
                return false;
            flow.backendDBID = flit.dbid;
            flow.backendDBIDSource = flit.source;
            flow.backendWriteTraceTag = flit.traceTag;
            flow.backendCompReceived |= flit.opcode == RSP::CompDBIDResp;
            if (flit.opcode == RSP::CompDBIDResp)
                flow.backendCompRespErr = flit.respErr;
            flow.backendPhase = BackendPhase::SendData;
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return true;
        }
        if (store && flit.opcode == RSP::Comp)
        {
            if (flow.backendPhase != BackendPhase::WaitDBID && flow.backendPhase != BackendPhase::WaitComp &&
                flow.backendPhase != BackendPhase::WaitDataEmission)
                return false;
            flow.backendCompReceived = true;
            flow.backendCompRespErr = flit.respErr;
            CompleteAtomicBackendIfReady(*this, transaction);
            return true;
        }
        return false;
    }

    bool Core::Impl::HandleAtomicBackendDAT(Transaction& transaction, const Data& flit)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction.request.opcode) ||
            flit.opcode != CHI::Eb::Opcodes::DAT::CompData ||
            (flow.backendPhase != BackendPhase::WaitDBID && flow.backendPhase != BackendPhase::SendData &&
             flow.backendPhase != BackendPhase::WaitDataEmission && flow.backendPhase != BackendPhase::WaitComp) ||
            flit.txnId != transaction.handle ||
            (flow.backendPhase != BackendPhase::WaitDBID && flit.dbid != flow.backendDBID) ||
            flit.ccid != ((transaction.request.address >> 4) & 3) ||
            flit.resp != CHI::Eb::Resps::CompData_I || flow.atomic.backendResultReceived)
            return false;
        if (flow.backendPhase == BackendPhase::WaitDBID)
            flow.atomic.backendResultDBID = flit.dbid;

        AtomicGeometry geometry;
        if (!GetAtomicGeometry(transaction.request, geometry))
            return false;
        const size_t bytes = BytesPerBeat(options);
        const size_t address = transaction.request.address % LINE_SIZE;
        const size_t offset = address / bytes * bytes;
        if (flit.ByteOffset() != offset || flit.dataId != offset / 16)
            return false;
        ByteEnables enables{};
        for (size_t index = 0; index < geometry.operandBytes; ++index)
        {
            const size_t packetIndex = address - offset + index;
            transaction.data[address + index] = flit.bytes[packetIndex];
            enables[address + index] = 1;
        }
        MergeMetadata(transaction.poison, transaction.dataCheck, flit.poison, flit.dataCheck,
                      enables, address, address - offset, geometry.operandBytes, options);
        flow.atomic.backendResultRespErr = flit.respErr;
        flow.atomic.backendResultReceived = true;
        CompleteAtomicBackendIfReady(*this, transaction);
        return true;
    }

}
