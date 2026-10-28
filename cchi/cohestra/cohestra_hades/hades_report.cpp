#if defined(COHESTRA_HADES)
#  if defined(__has_include) && !__has_include(<hades/transport.hpp>)
#    error "cohestra_hades: COHESTRA_HADES defined but HADES-Fabric headers not on the include path (set HADES_FABRIC_ROOT)"
#  endif
#endif

#if defined(COHESTRA_HADES) || __has_include(<hades/transport.hpp>)

#include "hades_report.hpp"

#include "../cohestra_instance.hpp"
#include "../../icn/taurus/cchi_taurus_component.hpp"
#include "../../util/cchi_util_decoding.hpp"


namespace Cohestra::Hades {

    HadesCompletionReporter::HadesCompletionReporter(Instance& instance, HadesFlitStore& store) noexcept
        : instance(instance), store(store)
    { }

    HadesCompletionReporter::~HadesCompletionReporter() noexcept
    {
        Detach();
    }

    void HadesCompletionReporter::Attach() noexcept
    {
        if (attached)
            return;

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            upstream->GetJoint().events->OnComplete.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &HadesCompletionReporter::OnCCHIComplete, this));
        }

        auto monitorEvents = store.AXICompletionMonitor().events;
        monitorEvents->OnWriteComplete.Register(
            Gravity::MakeListener(LISTENER_NAME, 0, &HadesCompletionReporter::OnAXIWriteComplete, this));
        monitorEvents->OnReadComplete.Register(
            Gravity::MakeListener(LISTENER_NAME, 0, &HadesCompletionReporter::OnAXIReadComplete, this));

        attached = true;
    }

    void HadesCompletionReporter::Detach() noexcept
    {
        if (!attached)
            return;

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            upstream->GetJoint().events->OnComplete.Unregister(LISTENER_NAME);
        }

        auto monitorEvents = store.AXICompletionMonitor().events;
        monitorEvents->OnWriteComplete.Unregister(LISTENER_NAME);
        monitorEvents->OnReadComplete.Unregister(LISTENER_NAME);

        attached = false;
    }

    uint64_t HadesCompletionReporter::CompletedCCHICount() const noexcept
    {
        return completedREQ + completedEVT;
    }

    uint64_t HadesCompletionReporter::CompletedAXIWriteCount() const noexcept
    {
        return store.AXICompletionMonitor().GetCompletedWriteCount();
    }

    uint64_t HadesCompletionReporter::CompletedAXIReadCount() const noexcept
    {
        return store.AXICompletionMonitor().GetCompletedReadCount();
    }

    uint64_t HadesCompletionReporter::CompletedAXICount() const noexcept
    {
        return CompletedAXIWriteCount() + CompletedAXIReadCount();
    }

    void HadesCompletionReporter::OnCCHIComplete(
        CCHI::Xact::JointXactionCompleteEvent<Config>& event) noexcept
    {
        auto xaction = event.GetXaction();

        if (!xaction)
            return;

        const auto& first = xaction->GetFirst();
        auto logger = instance.GetLogger();

        if (first.IsTXREQ())
        {
            const auto& request = first.flit.req;
            const uint64_t opcode = static_cast<uint64_t>(request.Opcode);
            const auto name = CCHI::REQOpcodeDecoder<CCHI::Flits::REQ<Config>>::INSTANCE
                .DecodeType1(request.Opcode).GetName();

            ++completedREQ;
            ++reqOpcodes[opcode];

            logger->info("HADES CCHI completion: upstream=0 class=REQ opcode={} txnid={:#x} addr={:#x} "
                "response_flits={} request_time={}.",
                name, static_cast<uint64_t>(request.TxnID), static_cast<uint64_t>(request.Addr),
                xaction->GetSubsequence().size(), first.time);
        }
        else if (first.IsTXEVT())
        {
            const auto& request = first.flit.evt;
            const uint64_t opcode = static_cast<uint64_t>(request.Opcode);
            const auto name = CCHI::EVTOpcodeDecoder<CCHI::Flits::EVT<Config>>::INSTANCE
                .DecodeType1(request.Opcode).GetName();

            ++completedEVT;
            ++evtOpcodes[opcode];

            logger->info("HADES CCHI completion: upstream=0 class=EVT opcode={} txnid={:#x} addr={:#x} "
                "response_flits={} request_time={}.",
                name, static_cast<uint64_t>(request.TxnID), static_cast<uint64_t>(request.Addr),
                xaction->GetSubsequence().size(), first.time);
        }
    }

    void HadesCompletionReporter::OnAXIWriteComplete(AXIWriteCompleteEvent& event) noexcept
    {
        const auto& transaction = event.GetTransaction();
        const auto& request = transaction.request.Bundle;

        instance.GetLogger()->info("HADES AXI completion: port=0 direction=write id={:#x} addr={:#x} "
            "len={} size={} burst={} data_beats={} resp={} request_time={} complete_time={}.",
            static_cast<uint64_t>(request.Id), static_cast<uint64_t>(request.Addr),
            static_cast<uint64_t>(request.Len), static_cast<uint64_t>(request.Size),
            AXI::ToString(request.Burst), transaction.data.size(),
            AXI::ToString(transaction.response.Bundle.Resp), transaction.request.Time, event.GetTime());
    }

    void HadesCompletionReporter::OnAXIReadComplete(AXIReadCompleteEvent& event) noexcept
    {
        const auto& transaction = event.GetTransaction();
        const auto& request = transaction.request.Bundle;
        auto logger = instance.GetLogger();

        if (transaction.data.empty())
        {
            logger->warn("HADES AXI completion: port=0 direction=read id={:#x} addr={:#x} "
                "completed without a final R beat.",
                static_cast<uint64_t>(request.Id), static_cast<uint64_t>(request.Addr));
            return;
        }

        logger->info("HADES AXI completion: port=0 direction=read id={:#x} addr={:#x} "
            "len={} size={} burst={} data_beats={} resp={} request_time={} complete_time={}.",
            static_cast<uint64_t>(request.Id), static_cast<uint64_t>(request.Addr),
            static_cast<uint64_t>(request.Len), static_cast<uint64_t>(request.Size),
            AXI::ToString(request.Burst), transaction.data.size(),
            AXI::ToString(transaction.data.back().Bundle.Resp), transaction.request.Time, event.GetTime());
    }

    void HadesCompletionReporter::Report() const noexcept
    {
        auto logger = instance.GetLogger();

        logger->info("HADES completed transaction summary:");
        logger->info(" - CCHI upstream: {} completed (REQ {}, EVT {}; RXSNP excluded).",
            CompletedCCHICount(), completedREQ, completedEVT);

        for (const auto& [opcode, count] : reqOpcodes)
        {
            using Decoder = CCHI::REQOpcodeDecoder<CCHI::Flits::REQ<Config>>;
            const auto name = Decoder::INSTANCE
                .DecodeType1(static_cast<Decoder::opcode_t>(opcode)).GetName();
            logger->info("   - REQ {}: {}", name, count);
        }

        for (const auto& [opcode, count] : evtOpcodes)
        {
            using Decoder = CCHI::EVTOpcodeDecoder<CCHI::Flits::EVT<Config>>;
            const auto name = Decoder::INSTANCE
                .DecodeType1(static_cast<Decoder::opcode_t>(opcode)).GetName();
            logger->info("   - EVT {}: {}", name, count);
        }

        logger->info(" - AXI slave port 0: {} completed ({} writes on B, {} reads on final R).",
            CompletedAXICount(), CompletedAXIWriteCount(), CompletedAXIReadCount());
    }
}

#endif
