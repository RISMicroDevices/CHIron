#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {
    namespace {

        ByteEnables AtomicFullEnables()
        {
            ByteEnables enables{};
            enables.fill(1);
            return enables;
        }

    }

    void Core::Impl::ResolveAtomic(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        flow.snoopLine = transaction.request.Line();
        flow.expectedBeatMask = AtomicOperandBeatMask(transaction.request, options);
        flow.atomic.resultBeatMask = AtomicResultBeatMask(transaction.request, options);
        flow.responseForm = transaction.handle % 3;
        for (NodeId target : SnoopTargets(transaction.request.Line()))
        {
            if (target != transaction.request.source || transaction.request.snoopMe)
                transaction.snoopTargets.push_back(target);
        }
        transaction.pendingSnoops.clear();
        transaction.snoopsRequired = !transaction.snoopTargets.empty();
        flow.nextSnoop = 0;
        transaction.phase = Phase::Action;
        transaction.readyTime = time;
    }

    void Core::Impl::ActionAtomic(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(transaction.request.opcode);
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
                transaction.wait = WaitReason::WriteData;
                transaction.readyTime = time;
                return;
            }
            flow.directoryCommitted = true;
            if (store)
                QueueComp(transaction, false, CHI::Eb::Resps::Comp_I, CHI::Eb::RespErrs::NDERR);
            else
            {
                transaction.responseResp = CHI::Eb::Resps::CompData_I;
                transaction.sentBeats = 0;
                transaction.phase = Phase::Return;
                transaction.readyTime = ScheduleData();
            }
            return;
        }
        if (store && !flow.dbidSent)
        {
            QueueDBID(transaction);
            return;
        }
        if (!store && !flow.dbidSent)
        {
            QueueDBID(transaction);
            return;
        }
        if (options.backend == BackendKind::CHI)
        {
            ActionAtomicBackend(transaction);
            return;
        }
        if (!transaction.Complete(COMPLETE_MEMORY))
        {
            StartMemoryRead(transaction);
            return;
        }
        if (!transaction.Complete(COMPLETE_WRITE_DATA))
        {
            transaction.phase = Phase::WaitWriteData;
            transaction.wait = WaitReason::WriteData;
            transaction.readyTime = time;
            return;
        }
        if (store)
            transaction.responseRespErr = MergeResponseErrors(transaction.responseRespErr,
                                                                    flow.atomic.operandRespErr);
        if (options.backend == BackendKind::AXI && transaction.responseRespErr)
        {
            flow.directoryCommitted = true;
            if (store)
            {
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
            return;
        }
        if (!PrepareHomeCommit(transaction))
        {
            transaction.phase = Phase::Action;
            transaction.readyTime = time;
            return;
        }
        if (!store)
        {
            transaction.phase = Phase::Return;
            transaction.readyTime = ScheduleData();
            return;
        }
        if (!flow.compSent)
        {
            QueueComp(transaction);
            return;
        }
        if (flow.compEmitted)
            FinishResponse(transaction);
    }

    void Core::Impl::ReturnAtomic(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        if (flow.atomic.returnSent)
            return;
        AtomicGeometry geometry;
        if (!GetAtomicGeometry(transaction.request, geometry))
        {
            Error(transaction.handle, "invalid atomic return geometry");
            transaction.canceled = true;
            transaction.phase = Phase::Commit;
            return;
        }

        const size_t bytes = BytesPerBeat(options);
        const size_t address = transaction.request.address % LINE_SIZE;
        const size_t offset = address / bytes * bytes;
        Data data;
        data.source = nodeId;
        data.target = transaction.request.source;
        data.homeNid = nodeId;
        data.txnId = transaction.request.txnId;
        data.dbid = transaction.dbid;
        data.opcode = CHI::Eb::Opcodes::DAT::CompData;
        data.resp = CHI::Eb::Resps::CompData_I;
        data.respErr = MergeResponseErrors(transaction.responseRespErr,
            MergeResponseErrors(flow.atomic.operandRespErr, flow.atomic.backendResultRespErr));
        data.dataId = offset / 16;
        data.ccid = (transaction.request.address >> 4) & 3;
        data.qos = transaction.request.qos;
        data.traceTag = transaction.request.traceTag;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            data.bytes[index] = transaction.data[lineOffset];
            data.byteEnables[index] = lineOffset >= address && lineOffset < address + geometry.operandBytes;
            data.dataCheck[index] = transaction.dataCheck[lineOffset];
        }
        for (size_t lane = 0; lane < bytes / 8; ++lane)
            data.poison[lane] = transaction.poison[offset / 8 + lane];
        if (Queue(txdat, data, transaction.handle, transaction.actionAge, true))
            flow.atomic.returnSent = true;
    }

    bool Core::Impl::HandleAtomicDAT(Transaction& transaction, const Data& flit)
    {
        FlowState& flow = flows.at(transaction.handle);
        const bool dataPhase = transaction.phase == Phase::WaitWriteData ||
            ((transaction.phase == Phase::Action || transaction.phase == Phase::WaitMemory) && flow.dbidEmitted);
        if (!dataPhase || !flow.dbidEmitted || flit.opcode != CHI::Eb::Opcodes::DAT::NonCopyBackWrData ||
            flit.txnId != transaction.dbid || flit.ccid != ((transaction.request.address >> 4) & 3))
            return false;

        AtomicGeometry geometry;
        if (!GetAtomicGeometry(transaction.request, geometry))
            return false;
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = flit.ByteOffset();
        if (offset + bytes > LINE_SIZE || offset % bytes != 0)
            return false;
        const size_t beat = offset / bytes;
        const uint64_t bit = uint64_t(1) << beat;
        if (!(flow.expectedBeatMask & bit) || (transaction.receivedBeats & bit))
            return false;

        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            const bool enabled = lineOffset >= geometry.windowOffset &&
                lineOffset < geometry.windowOffset + geometry.windowBytes;
            if (bool(flit.byteEnables[index]) != enabled || (!enabled && flit.bytes[index] != 0))
                return false;
        }
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            if (flit.byteEnables[index])
                flow.atomic.operands[lineOffset] = flit.bytes[index];
            transaction.byteEnables[lineOffset] = flit.byteEnables[index];
        }
        const ByteEnables enables = ExpandedEnables(flit.byteEnables, offset, bytes);
        MergeMetadata(flow.atomic.operandsPoison, flow.atomic.operandsDataCheck,
                      flit.poison, flit.dataCheck, enables, offset, 0, bytes, options);
        flow.writeRespErr[beat] = MergeResponseErrors(flit.respErr,
            HasUnsupportedMetadataError(flit, enables, offset, 0, bytes, options)
                ? uint8_t(CHI::Eb::RespErrs::DERR) : uint8_t(CHI::Eb::RespErrs::OK));
        flow.writeRespErrMask |= bit;
        flow.atomic.operandRespErr = MergeResponseErrors(flow.atomic.operandRespErr, flow.writeRespErr[beat]);

        transaction.receivedBeats |= bit;
        if ((transaction.receivedBeats & flow.expectedBeatMask) != flow.expectedBeatMask)
            return true;
        transaction.Mark(COMPLETE_WRITE_DATA);
        transaction.phase = Phase::Action;
        transaction.wait = WaitReason::None;
        transaction.readyTime = time;
        return true;
    }

    bool Core::Impl::CommitAtomic(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        flow.atomic.execution = ExecuteAtomic(transaction.request, transaction.data, flow.atomic.operands);
        LineImage commitImage = MakeLineImage(transaction, options);
        commitImage.data = flow.atomic.execution.write ? flow.atomic.execution.data : transaction.data;
        if (flow.atomic.execution.write)
        {
            ByteEnables changed{};
            for (size_t index = 0; index < LINE_SIZE; ++index)
                changed[index] = commitImage.data[index] != transaction.data[index];
            GenerateDataCheck(commitImage.dataCheck, commitImage.data, changed, options);
            if (options.poisonEnabled)
                for (size_t lane = 0; lane < commitImage.poison.size(); ++lane)
                    commitImage.poison[lane] |= flow.atomic.operandsPoison[lane];
        }

        if (HomeLine* home = directory.Find(line))
        {
            if (flow.atomic.execution.write || flow.dataDirty)
            {
                home->data = commitImage.data;
                CopyMetadata(*home, commitImage, options);
                home->dirty = true;
                directory.Touch(line);
            }
        }
        else if (flow.atomic.execution.write || flow.dataDirty)
        {
            PolicyContext context;
            context.line = line;
            context.requester = transaction.request.source;
            context.homeValid = false;
            context.requesterValid = false;
            context.requesterDirty = true;
            context.evictionHasData = true;
            context.allocationHint = CHI::Eb::MemAttrs::ExtractAllocate(transaction.request.memAttr) != 0;
            const PolicyDecision decision = policy->OnImmediateWrite(context);
            if (decision.allocateHome && !PrepareVictim(transaction))
                return false;
            if (decision.allocateHome || decision.retainHome)
            {
                HomeLine& home = InstallHome(line, flow.victimWay, commitImage.data, true, time);
                CopyMetadata(home, commitImage, options);
            }
            else if (!WriteLineImage(transaction, line, commitImage, AtomicFullEnables()))
                return false;
        }
        flow.requesterFinal = CacheState::I;
        snoopFilter.Track(line, transaction.request.source, CacheState::I, time);
        return true;
    }

}
