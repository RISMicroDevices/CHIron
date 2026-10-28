#include "chi_lunatic_core.hpp"

#include <bit>

namespace CHI::Lunatic {

void Core::Impl::StartMemoryRead(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    if (transaction.homeHit || flow.snoopDataValid)
    {
        transaction.memoryRequired = false;
        transaction.Mark(COMPLETE_MEMORY);
        transaction.phase = Phase::Action;
        transaction.readyTime = pipeline.ScheduleData(time);
        return;
    }

    transaction.memoryRequired = true;
    flow.memoryWrite = false;
    if (options.backend == BackendKind::Internal)
    {
        flow.memoryReady = pipeline.SaturatingAdd(time, memory.NextLatency(options.memoryLatency));
        transaction.phase = Phase::WaitMemory;
        transaction.wait = WaitReason::Memory;
        transaction.readyTime = time;
        return;
    }

    Request request = transaction.request;
    request.source = nodeId;
    request.target = options.subordinateId;
    request.txnId = transaction.handle;
    request.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    request.allowRetry = false;
    request.expCompAck = false;
    if (!Queue(txreq, request, transaction.handle, transaction.actionAge, false, true))
    {
        transaction.readyTime = time;
        return;
    }
    if (!tracker.BindMemory(transaction.handle, options.subordinateId, transaction.handle))
    {
        Error(transaction.handle, "unable to bind downstream read ID");
        transaction.canceled = true;
        transaction.phase = Phase::Commit;
        return;
    }
    flow.backendPhase = BackendPhase::WaitRequestEmission;
    transaction.phase = Phase::WaitMemory;
    transaction.wait = WaitReason::Memory;
    transaction.readyTime = time;
}

void Core::Impl::CompleteMemoryRead(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    if (!transaction.memoryRequired)
        return;

    if (flow.memoryWrite)
        memory.Write(transaction.request.Line(), transaction.data, transaction.byteEnables);
    else
        transaction.data = memory.Read(transaction.request.Line());

    transaction.Mark(COMPLETE_MEMORY);
    transaction.memoryRequired = false;
    transaction.wait = WaitReason::None;
    flow.backendPhase = BackendPhase::Done;
    transaction.phase = Phase::Action;
    transaction.readyTime = pipeline.ScheduleData(time);
}

void Core::Impl::StartBackendWrite(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    if (transaction.canceled)
    {
        flow.backendPhase = BackendPhase::Done;
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
        return;
    }

    if (options.backend == BackendKind::Internal)
    {
        transaction.memoryRequired = true;
        flow.memoryWrite = true;
        flow.memoryReady = pipeline.SaturatingAdd(time, memory.NextLatency(options.memoryLatency));
        transaction.phase = Phase::WaitMemory;
        transaction.wait = WaitReason::Memory;
        transaction.readyTime = time;
        return;
    }

    Request request = transaction.request;
    request.source = nodeId;
    request.target = options.subordinateId;
    request.txnId = transaction.handle;
    request.allowRetry = false;
    request.expCompAck = false;
    if (!Queue(txreq, request, transaction.handle, transaction.actionAge, false, true))
    {
        transaction.readyTime = time;
        return;
    }
    if (!tracker.BindMemory(transaction.handle, options.subordinateId, transaction.handle))
    {
        Error(transaction.handle, "unable to bind downstream write ID");
        transaction.canceled = true;
        transaction.phase = Phase::Commit;
        return;
    }
    flow.backendBeat = transaction.receivedBeats ? std::countr_zero(flow.expectedBeatMask) : 0;
    flow.backendPhase = BackendPhase::WaitRequestEmission;
    transaction.phase = Phase::WaitMemory;
    transaction.wait = WaitReason::Memory;
    transaction.readyTime = time;
}

void Core::Impl::AdvanceBackendWrite(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    if (flow.backendPhase != BackendPhase::SendData)
        return;

    const size_t bytes = BytesPerBeat(options);
    const size_t beat = flow.backendBeat;
    if (beat >= BeatsPerLine(options) || !(flow.expectedBeatMask & (uint64_t(1) << beat)))
    {
        Error(transaction.handle, "invalid downstream write beat selection");
        transaction.canceled = true;
        transaction.phase = Phase::Commit;
        return;
    }

    const size_t offset = beat * bytes;
    Data data;
    data.source = nodeId;
    data.target = options.subordinateId;
    data.homeNid = nodeId;
    data.txnId = transaction.handle;
    data.dbid = flow.backendDBID;
    data.opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
    data.dataId = offset / 16;
    data.ccid = (offset / 16) % 2;
    data.qos = transaction.request.qos;
    data.traceTag = transaction.request.traceTag;
    for (size_t index = 0; index < bytes; ++index)
    {
        data.bytes[index] = transaction.data[offset + index];
        data.byteEnables[index] = transaction.byteEnables[offset + index];
        data.dataCheck[index] = transaction.dataCheck[offset + index];
    }
    for (size_t lane = 0; lane < bytes / 8; ++lane)
        data.poison[lane] = transaction.poison[offset / 8 + lane];

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
        transaction.readyTime = time;
    }
    else
    {
        flow.backendBeat = std::countr_zero(remaining);
        transaction.readyTime = time;
    }
}

bool Core::Impl::HandleBackendRSP(Transaction& transaction, const Response& flit)
{
    namespace RSP = CHI::Eb::Opcodes::RSP;
    FlowState& flow = flows.at(transaction.handle);

    if (flit.opcode == RSP::ReadReceipt)
        return IsReadRequest(transaction.request.opcode) && transaction.request.order != 0;

    if (IsCMORequest(transaction.request.opcode))
    {
        if (flit.opcode != RSP::Comp)
            return false;
        flow.backendCompReceived = true;
        flow.backendPhase = BackendPhase::Done;
        tracker.UnbindMemory(options.subordinateId, transaction.handle);
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
        return true;
    }

    if (IsReadRequest(transaction.request.opcode))
        return false;

    if (flit.opcode == RSP::DBIDResp || flit.opcode == RSP::CompDBIDResp)
    {
        if (flow.backendPhase != BackendPhase::WaitDBID)
            return false;
        flow.backendDBID = flit.dbid;
        flow.backendCompReceived = flit.opcode == RSP::CompDBIDResp;
        if (ClassifyRequest(transaction.request.opcode) == RequestClass::ZeroWrite)
        {
            if (flow.backendCompReceived)
            {
                flow.backendPhase = BackendPhase::Done;
                tracker.UnbindMemory(options.subordinateId, transaction.handle);
                transaction.phase = Phase::Action;
            }
            else
            {
                flow.backendPhase = BackendPhase::WaitComp;
                transaction.phase = Phase::WaitMemory;
            }
        }
        else
        {
            flow.backendPhase = BackendPhase::SendData;
            transaction.phase = Phase::Action;
        }
        transaction.readyTime = time;
        return true;
    }

    if (flit.opcode == RSP::Comp)
    {
        if (ClassifyRequest(transaction.request.opcode) == RequestClass::ZeroWrite)
        {
            if (flow.backendPhase != BackendPhase::WaitComp)
                return false;
            flow.backendCompReceived = true;
            flow.backendPhase = BackendPhase::Done;
            tracker.UnbindMemory(options.subordinateId, transaction.handle);
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return true;
        }
        if (flow.backendPhase != BackendPhase::WaitComp && flow.backendPhase != BackendPhase::WaitDataEmission)
            return false;
        flow.backendCompReceived = true;
        if (flow.backendDataEmitted)
        {
            flow.backendPhase = BackendPhase::Done;
            tracker.UnbindMemory(options.subordinateId, transaction.handle);
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
        }
        return true;
    }

    return false;
}

bool Core::Impl::HandleBackendDAT(Transaction& transaction, const Data& flit)
{
    FlowState& flow = flows.at(transaction.handle);
    if (!transaction.memoryRequired || flow.memoryWrite || !IsReadRequest(transaction.request.opcode))
        return false;

    const size_t bytes = BytesPerBeat(options);
    const size_t offset = flit.ByteOffset();
    if (offset + bytes > LINE_SIZE || offset % bytes != 0)
        return false;
    const size_t beat = offset / bytes;
    const uint64_t bit = uint64_t(1) << beat;
    if (transaction.receivedBeats & bit)
        return false;

    for (size_t index = 0; index < bytes; ++index)
    {
        transaction.data[offset + index] = flit.bytes[index];
        transaction.byteEnables[offset + index] = 1;
        transaction.dataCheck[offset + index] = flit.dataCheck[index];
    }
    for (size_t lane = 0; lane < bytes / 8; ++lane)
        transaction.poison[offset / 8 + lane] = flit.poison[lane];
    transaction.responseRespErr = flit.respErr;
    transaction.receivedBeats |= bit;

    const uint64_t complete = (uint64_t(1) << BeatsPerLine(options)) - 1;
    if ((transaction.receivedBeats & complete) != complete)
        return true;

    transaction.Mark(COMPLETE_MEMORY);
    transaction.memoryRequired = false;
    transaction.wait = WaitReason::None;
    flow.backendPhase = BackendPhase::Done;
    tracker.UnbindMemory(options.subordinateId, transaction.handle);
    transaction.phase = Phase::Action;
    transaction.readyTime = pipeline.ScheduleData(time);
    return true;
}

}
