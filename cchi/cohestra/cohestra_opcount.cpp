#include "cohestra_opcount.hpp"

#include <algorithm>

#include "../util/cchi_util_decoding.hpp"

#include "cohestra_instance.hpp"


// File-local opcode table printer
namespace {

    template<class TDecoder>
    void ReportChannelOpcodes(const std::shared_ptr<spdlog::logger>&  logger,
                              const char*                             channel,
                              const std::map<uint64_t, uint64_t>&     counts,
                              const TDecoder&                         decoder) noexcept
    {
        logger->info("  - {}:", channel);

        // every opcode permitted for a Type-1 node, zero counts included
        auto iter = decoder.IterateType1();

        for (;;)
        {
            const auto& info = iter.Next();

            if (!info.IsValid())
                break;

            uint64_t count = 0;

            if (auto it = counts.find(info.GetOpcode()); it != counts.end())
                count = it->second;

            logger->info("    - {}: {}", info.GetName(), count);
        }

        // defensive: opcodes observed but not permitted for a Type-1 node
        // (should never happen; printed so nothing is silently dropped)
        for (const auto& [opcode, count] : counts)
        {
            auto coded = static_cast<typename TDecoder::opcode_t>(opcode);

            if (!decoder.DecodeType1(coded).IsValid())
                logger->info("    - {}: {} (not a Type-1 opcode)",
                    decoder.Decode(coded).GetName("<Unknown>"), count);
        }
    }
}


// Implementation of: class OpCount
namespace Cohestra {

    const ConfigurationEntryBack<bool> OpCount::OPCOUNT_ENABLE
        ("OPCOUNT_ENABLE", "enable", "opcount", true, DeserializeBool, SerializeBool);


    OpCount::OpCount(Instance& instance) noexcept
        : instance  (instance)
    { }

    OpCount::~OpCount() noexcept
    {
        Detach();
    }

    template<class T>
    bool OpCount::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry);

        if (!rawValue)
        {
            dst = entry->defaultValue;
            return true;
        }

        std::optional<T> deserializedValue = entry->deserializer(*rawValue);

        if (!deserializedValue)
        {
            instance.GetLogger()->error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }

    bool OpCount::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        return LoadConfigurationEntry(loader, &OPCOUNT_ENABLE, enabled);
    }

    void OpCount::Attach() noexcept
    {
        if (attached || !enabled)
            return;

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            // a harness may share one event hub between all upstream nodes -
            // register only once per hub
            if (!upstream->events)
                continue;

            if (std::find(attachedHubs.begin(), attachedHubs.end(), upstream->events) != attachedHubs.end())
                continue;

            auto hub = upstream->events;

            hub->OnAcceptedEVT  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &OpCount::OnAcceptedEVT, this));
            hub->OnAcceptedSNP  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &OpCount::OnAcceptedSNP, this));
            hub->OnAcceptedREQ  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &OpCount::OnAcceptedREQ, this));
            hub->OnAcceptedDnRSP.Register(Gravity::MakeListener(LISTENER_NAME, 0, &OpCount::OnAcceptedDnRSP, this));
            hub->OnAcceptedUpRSP.Register(Gravity::MakeListener(LISTENER_NAME, 0, &OpCount::OnAcceptedUpRSP, this));
            hub->OnAcceptedDnDAT.Register(Gravity::MakeListener(LISTENER_NAME, 0, &OpCount::OnAcceptedDnDAT, this));
            hub->OnAcceptedUpDAT.Register(Gravity::MakeListener(LISTENER_NAME, 0, &OpCount::OnAcceptedUpDAT, this));

            attachedHubs.push_back(upstream->events);
        }

        attached = true;
    }

    void OpCount::Detach() noexcept
    {
        for (auto& hub : attachedHubs)
        {
            hub->OnAcceptedEVT  .Unregister(LISTENER_NAME);
            hub->OnAcceptedSNP  .Unregister(LISTENER_NAME);
            hub->OnAcceptedREQ  .Unregister(LISTENER_NAME);
            hub->OnAcceptedDnRSP.Unregister(LISTENER_NAME);
            hub->OnAcceptedUpRSP.Unregister(LISTENER_NAME);
            hub->OnAcceptedDnDAT.Unregister(LISTENER_NAME);
            hub->OnAcceptedUpDAT.Unregister(LISTENER_NAME);
        }

        attachedHubs.clear();
        attached = false;
    }

    bool OpCount::IsAttached() const noexcept
    {
        return attached;
    }

    bool OpCount::IsEnabled() const noexcept
    {
        return enabled;
    }

    void OpCount::Report() const noexcept
    {
        if (!enabled)
            return;

        auto logger = instance.GetLogger();

        logger->info("Upstream opcode count report:");

        using Cfg = CommonFlitConfigurationType1;

        static const NodeCounts EMPTY;

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream)
                continue;

            auto        it     = nodeCounts.find(upstream);
            const auto& counts = it != nodeCounts.end() ? it->second : EMPTY;

            logger->info(" - Upstream node {} (node ID {}):", index, size_t(upstream->nodeID));

            ReportChannelOpcodes(logger, "TXREQ", counts.txREQ,   CCHI::REQOpcodeDecoder  <CCHI::Flits::REQ  <Cfg>>::INSTANCE);
            ReportChannelOpcodes(logger, "TXRSP", counts.txUpRSP, CCHI::UpRSPOpcodeDecoder<CCHI::Flits::UpRSP<Cfg>>::INSTANCE);
            ReportChannelOpcodes(logger, "TXDAT", counts.txUpDAT, CCHI::UpDATOpcodeDecoder<CCHI::Flits::UpDAT<Cfg>>::INSTANCE);
            ReportChannelOpcodes(logger, "TXEVT", counts.txEVT,   CCHI::EVTOpcodeDecoder  <CCHI::Flits::EVT  <Cfg>>::INSTANCE);
            ReportChannelOpcodes(logger, "RXSNP", counts.rxSNP,   CCHI::SNPOpcodeDecoder  <CCHI::Flits::SNP  <Cfg>>::INSTANCE);
            ReportChannelOpcodes(logger, "RXRSP", counts.rxDnRSP, CCHI::DnRSPOpcodeDecoder<CCHI::Flits::DnRSP<Cfg>>::INSTANCE);
            ReportChannelOpcodes(logger, "RXDAT", counts.rxDnDAT, CCHI::DnDATOpcodeDecoder<CCHI::Flits::DnDAT<Cfg>>::INSTANCE);
        }
    }

    void OpCount::OnAcceptedEVT(CCHI::Taurus::UpstreamNodeXactAcceptedEVTEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        nodeCounts[&event.GetUpstream()].txEVT[event.GetEVTFlit().Opcode]++;
    }

    void OpCount::OnAcceptedSNP(CCHI::Taurus::UpstreamNodeXactAcceptedSNPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        nodeCounts[&event.GetUpstream()].rxSNP[event.GetSNPFlit().Opcode]++;
    }

    void OpCount::OnAcceptedREQ(CCHI::Taurus::UpstreamNodeXactAcceptedREQEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        nodeCounts[&event.GetUpstream()].txREQ[event.GetREQFlit().Opcode]++;
    }

    void OpCount::OnAcceptedDnRSP(CCHI::Taurus::UpstreamNodeXactAcceptedDnRSPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        nodeCounts[&event.GetUpstream()].rxDnRSP[event.GetDnRSPFlit().Opcode]++;
    }

    void OpCount::OnAcceptedUpRSP(CCHI::Taurus::UpstreamNodeXactAcceptedUpRSPEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        nodeCounts[&event.GetUpstream()].txUpRSP[event.GetUpRSPFlit().Opcode]++;
    }

    void OpCount::OnAcceptedDnDAT(CCHI::Taurus::UpstreamNodeXactAcceptedDnDATEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        nodeCounts[&event.GetUpstream()].rxDnDAT[event.GetDnDATFlit().Opcode]++;
    }

    void OpCount::OnAcceptedUpDAT(CCHI::Taurus::UpstreamNodeXactAcceptedUpDATEvent<CommonFlitConfigurationType1>& event) noexcept
    {
        nodeCounts[&event.GetUpstream()].txUpDAT[event.GetUpDATFlit().Opcode]++;
    }
}
