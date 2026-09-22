#include "cohestra_cchi_flit_logger.hpp"

#include <algorithm>

#include "cohestra_instance.hpp"

#include "spdlog/spdlog.h"


// Implementation of: class CCHIFlitLogger
namespace Cohestra {

    const ConfigurationEntryBack<bool> CCHIFlitLogger::CONFIG_CCHI_VERBOSE
        ("CCHI_VERBOSE", "verbose", "cchi", false, DeserializeBool, SerializeBool);


    CCHIFlitLogger::CCHIFlitLogger(Instance& instance) noexcept
        : instance  (instance)
    { }

    CCHIFlitLogger::~CCHIFlitLogger() noexcept
    {
        Detach();
    }

    template<class T>
    bool CCHIFlitLogger::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
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

    bool CCHIFlitLogger::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        return LoadConfigurationEntry(loader, &CONFIG_CCHI_VERBOSE, enabled);
    }

    void CCHIFlitLogger::Attach() noexcept
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

            hub->OnAcceptedEVT  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &CCHIFlitLogger::OnAcceptedEVT, this));
            hub->OnAcceptedSNP  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &CCHIFlitLogger::OnAcceptedSNP, this));
            hub->OnAcceptedREQ  .Register(Gravity::MakeListener(LISTENER_NAME, 0, &CCHIFlitLogger::OnAcceptedREQ, this));
            hub->OnAcceptedDnRSP.Register(Gravity::MakeListener(LISTENER_NAME, 0, &CCHIFlitLogger::OnAcceptedDnRSP, this));
            hub->OnAcceptedUpRSP.Register(Gravity::MakeListener(LISTENER_NAME, 0, &CCHIFlitLogger::OnAcceptedUpRSP, this));
            hub->OnAcceptedDnDAT.Register(Gravity::MakeListener(LISTENER_NAME, 0, &CCHIFlitLogger::OnAcceptedDnDAT, this));
            hub->OnAcceptedUpDAT.Register(Gravity::MakeListener(LISTENER_NAME, 0, &CCHIFlitLogger::OnAcceptedUpDAT, this));

            attachedHubs.push_back(upstream->events);
        }

        attached = true;
    }

    void CCHIFlitLogger::Detach() noexcept
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

    bool CCHIFlitLogger::IsAttached() const noexcept
    {
        return attached;
    }

    bool CCHIFlitLogger::IsEnabled() const noexcept
    {
        return enabled;
    }

    uint64_t CCHIFlitLogger::GetOrAssignHash(const std::shared_ptr<CCHI::Xact::Xaction<Cfg>>& xaction) noexcept
    {
        if (!xaction)
            return 0;

        auto existing = xaction->companion.Get(CCHIFlitFormat::XactHashTag);

        if (existing)
            return *existing;

        uint64_t hash = nextHash++;
        xaction->companion.Set(CCHIFlitFormat::XactHashTag, hash);
        return hash;
    }


    // Channel formatters (shared implementation in CCHIFlitFormat)
    std::string CCHIFlitLogger::FormatEVT(const CCHI::Flits::EVT<Cfg>& flit) const noexcept
    {
        return CCHIFlitFormat::FormatEVT(flit);
    }

    std::string CCHIFlitLogger::FormatREQ(const CCHI::Flits::REQ<Cfg>& flit) const noexcept
    {
        return CCHIFlitFormat::FormatREQ(flit);
    }

    std::string CCHIFlitLogger::FormatSNP(const CCHI::Flits::SNP<Cfg>& flit) const noexcept
    {
        return CCHIFlitFormat::FormatSNP(flit);
    }

    std::string CCHIFlitLogger::FormatDnRSP(const CCHI::Flits::DnRSP<Cfg>& flit) const noexcept
    {
        return CCHIFlitFormat::FormatDnRSP(flit);
    }

    std::string CCHIFlitLogger::FormatUpRSP(const CCHI::Flits::UpRSP<Cfg>& flit) const noexcept
    {
        return CCHIFlitFormat::FormatUpRSP(flit);
    }

    std::string CCHIFlitLogger::FormatDnDAT(const CCHI::Flits::DnDAT<Cfg>& flit) const noexcept
    {
        return CCHIFlitFormat::FormatDnDAT(flit);
    }

    std::string CCHIFlitLogger::FormatUpDAT(const CCHI::Flits::UpDAT<Cfg>& flit) const noexcept
    {
        return CCHIFlitFormat::FormatUpDAT(flit);
    }


    // Taurus accept event handlers
    void CCHIFlitLogger::OnAcceptedEVT(CCHI::Taurus::UpstreamNodeXactAcceptedEVTEvent<Cfg>& event) noexcept
    {
        uint64_t hash = GetOrAssignHash(event.GetXaction());
        instance.GetLogger()->debug("CCHI [node={}] [TX-EVT] [#{}]{}",
            size_t(event.GetUpstream().nodeID), hash, FormatEVT(event.GetEVTFlit()));
    }

    void CCHIFlitLogger::OnAcceptedSNP(CCHI::Taurus::UpstreamNodeXactAcceptedSNPEvent<Cfg>& event) noexcept
    {
        uint64_t hash = GetOrAssignHash(event.GetXaction());
        instance.GetLogger()->debug("CCHI [node={}] [RX-SNP] [#{}]{}",
            size_t(event.GetUpstream().nodeID), hash, FormatSNP(event.GetSNPFlit()));
    }

    void CCHIFlitLogger::OnAcceptedREQ(CCHI::Taurus::UpstreamNodeXactAcceptedREQEvent<Cfg>& event) noexcept
    {
        uint64_t hash = GetOrAssignHash(event.GetXaction());
        instance.GetLogger()->debug("CCHI [node={}] [TX-REQ] [#{}]{}",
            size_t(event.GetUpstream().nodeID), hash, FormatREQ(event.GetREQFlit()));
    }

    void CCHIFlitLogger::OnAcceptedDnRSP(CCHI::Taurus::UpstreamNodeXactAcceptedDnRSPEvent<Cfg>& event) noexcept
    {
        uint64_t hash = GetOrAssignHash(event.GetXaction());
        instance.GetLogger()->debug("CCHI [node={}] [RX-RSP] [#{}]{}",
            size_t(event.GetUpstream().nodeID), hash, FormatDnRSP(event.GetDnRSPFlit()));
    }

    void CCHIFlitLogger::OnAcceptedUpRSP(CCHI::Taurus::UpstreamNodeXactAcceptedUpRSPEvent<Cfg>& event) noexcept
    {
        uint64_t hash = GetOrAssignHash(event.GetXaction());
        instance.GetLogger()->debug("CCHI [node={}] [TX-RSP] [#{}]{}",
            size_t(event.GetUpstream().nodeID), hash, FormatUpRSP(event.GetUpRSPFlit()));
    }

    void CCHIFlitLogger::OnAcceptedDnDAT(CCHI::Taurus::UpstreamNodeXactAcceptedDnDATEvent<Cfg>& event) noexcept
    {
        uint64_t hash = GetOrAssignHash(event.GetXaction());
        instance.GetLogger()->debug("CCHI [node={}] [RX-DAT] [#{}]{}",
            size_t(event.GetUpstream().nodeID), hash, FormatDnDAT(event.GetDnDATFlit()));
    }

    void CCHIFlitLogger::OnAcceptedUpDAT(CCHI::Taurus::UpstreamNodeXactAcceptedUpDATEvent<Cfg>& event) noexcept
    {
        uint64_t hash = GetOrAssignHash(event.GetXaction());
        instance.GetLogger()->debug("CCHI [node={}] [TX-DAT] [#{}]{}",
            size_t(event.GetUpstream().nodeID), hash, FormatUpDAT(event.GetUpDATFlit()));
    }
}
