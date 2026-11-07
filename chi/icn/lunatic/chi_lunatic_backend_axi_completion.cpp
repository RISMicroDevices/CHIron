#include "chi_lunatic_core.hpp"

#include <bit>

namespace CHI::Lunatic {

    void Core::Impl::HandleAXICompletion(const AXICompletion& completion)
    {
        Transaction* transaction = tracker.Find(completion.transaction);
        if (!transaction)
        {
            Error(completion.transaction, "AXI completion has no live transaction");
            return;
        }

        FlowState& flow = flows.at(transaction->handle);
        const bool failed = completion.response != AXIResp::OKAY || !completion.protocolError.empty();
        if (!completion.protocolError.empty())
            Error(transaction->handle, completion.protocolError);
        if (failed)
            transaction->responseRespErr = CHI::Eb::RespErrs::NDERR;

        if (completion.write)
        {
            flow.backendCompReceived = true;
            flow.backendDataEmitted = true;
            flow.backendPhase = BackendPhase::Done;
            transaction->phase = Phase::Action;
            transaction->wait = WaitReason::None;
            transaction->readyTime = time;
            return;
        }

        const size_t bytes = BytesPerBeat(options);
        uint64_t remaining = completion.beatMask;
        while (remaining)
        {
            const size_t beat = size_t(std::countr_zero(remaining));
            const size_t offset = beat * bytes;
            Data data;
            data.source = options.subordinateId;
            data.target = nodeId;
            data.homeNid = nodeId;
            data.txnId = transaction->handle;
            data.opcode = CHI::Eb::Opcodes::DAT::CompData;
            data.resp = CHI::Eb::Resps::CompData_I;
            data.respErr = failed ? uint8_t(CHI::Eb::RespErrs::NDERR) : uint8_t(CHI::Eb::RespErrs::OK);
            data.dataId = offset / 16;
            data.ccid = flow.baselineFetch ? 0 : (transaction->request.address >> 4) & 3;
            for (size_t index = 0; index < bytes; ++index)
                data.bytes[index] = completion.data[offset + index];
            if (!HandleBackendDAT(*transaction, data))
            {
                Error(transaction->handle, "invalid AXI read completion geometry");
                flow.backendPhase = BackendPhase::Done;
                transaction->phase = Phase::Action;
                transaction->readyTime = time;
                return;
            }
            remaining &= remaining - 1;
        }
    }

}
