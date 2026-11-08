#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {
    namespace {

        bool IsStashSnoop(uint16_t opcode) noexcept
        {
            return opcode == CHI::Eb::Opcodes::SNP::SnpUniqueStash ||
                opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash ||
                opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
                opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        }

        bool IsSharedIndependentStash(uint16_t opcode) noexcept
        {
            return opcode == CHI::Eb::Opcodes::REQ::StashOnceShared ||
                opcode == CHI::Eb::Opcodes::REQ::StashOnceSepShared;
        }

    }

    bool Core::Impl::CreateDataPullTransaction(Transaction& transaction, NodeId stashee,
                                               DataBufferId dbid, uint8_t qos, uint8_t traceTag)
    {
        const auto parentClass = ClassifyRequest(transaction.request.opcode);
        const bool writeStash = parentClass == RequestClass::ImmediateWrite;
        Request request;
        request.source = stashee;
        request.target = nodeId;
        request.txnId = dbid;
        request.address = transaction.request.address;
        request.opcode = !writeStash && IsSharedIndependentStash(transaction.request.opcode)
            ? CHI::Eb::Opcodes::REQ::ReadNotSharedDirty : CHI::Eb::Opcodes::REQ::ReadUnique;
        request.size = 6;
        request.qos = qos;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.snpAttr = 1;
        request.traceTag = traceTag;
        request.nonSecure = transaction.request.nonSecure;
        request.allowRetry = false;
        request.expCompAck = true;

        const Time age = nextAge++;
        Transaction* implied = tracker.Allocate(request, age);
        if (!implied)
        {
            Error(transaction.handle, "duplicate DataPull requester transaction ID");
            return false;
        }
        ++pmu.admittedTransactions;

        FlowState flow;
        flow.separatedResponse = options.separatedReadResponse;
        flow.separatedBackendRead = options.separatedBackendRead;
        flow.dmtActive = options.dmtEnabled && options.backend == BackendKind::CHI && !writeStash;
        flow.dataPullParent = transaction.handle;
        flow.dataPullWriteStash = writeStash;
        flow.dataPullHomeDataAvailable = transaction.homeHit;
        flows.emplace(implied->handle, std::move(flow));
        if (!ordering.Add(*implied))
        {
            Error(transaction.handle, "duplicate DataPull ordering gate");
            tracker.Retire(implied->handle);
            flows.erase(implied->handle);
            return false;
        }
        if (!hazards.Add(implied->handle, implied->request.Line(), implied->actionAge))
        {
            Error(transaction.handle, "duplicate DataPull hazard gate");
            ordering.Release(implied->handle);
            tracker.Retire(implied->handle);
            flows.erase(implied->handle);
            return false;
        }
        flows.at(transaction.handle).dataPullChildren.push_back(implied->handle);
        implied->readyTime = pipeline.SaturatingAdd(time, 1);
        implied->phase = Phase::Receive;
        return true;
    }

    bool Core::Impl::HandleDataPullResponse(Transaction& transaction, const Response& flit)
    {
        return HandleDataPullResponse(transaction, flit.source, flit.dbid, flit.fwdState,
                                      flit.qos, flit.traceTag);
    }

    bool Core::Impl::HandleDataPullResponse(Transaction& transaction, const Data& flit)
    {
        return HandleDataPullResponse(transaction, flit.source, flit.dbid, flit.dataSource,
                                      flit.qos, flit.traceTag);
    }

    bool Core::Impl::HandleDataPullResponse(Transaction& transaction, NodeId stashee,
                                            DataBufferId dbid, uint8_t dataPull,
                                            uint8_t qos, uint8_t traceTag)
    {
        const bool stashSnoop = IsStashSnoop(SnoopOpcode(transaction, flows.at(transaction.handle), stashee));
        if (dataPull == 0 || !stashSnoop)
            return true;
        if (dataPull != 1)
        {
            Error(transaction.handle, "reserved DataPull encoding");
            return false;
        }

        FlowState& flow = flows.at(transaction.handle);
        if (const auto existing = flow.dataPullIds.find(stashee); existing != flow.dataPullIds.end())
        {
            if (existing->second != dbid)
            {
                Error(transaction.handle, "inconsistent DataPull response DBID");
                return false;
            }
            return true;
        }
        flow.dataPullIds.emplace(stashee, dbid);
        if (!CreateDataPullTransaction(transaction, stashee, dbid, qos, traceTag))
        {
            flow.dataPullIds.erase(stashee);
            return false;
        }
        return true;
    }

}
