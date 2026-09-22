#pragma once

#include "cohestra_config.hpp"
#include "cohestra_interface.hpp"
#include "cohestra_cchi_flit_format.hpp"

#include "../icn/taurus/cchi_taurus_component.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>


namespace Cohestra {

    class Instance;

    /*
    *   Configurable, transaction-aware verbose logger for CCHI flits.
    *
    *   Attaches one named listener per channel-accept event to the Taurus
    *   event hub(s) of every Type-1 upstream node (shared-hub dedup, the
    *   same pattern as OpCount). For every accepted flit it emits one
    *   spdlog::debug line showing:
    *     - the upstream node ID,
    *     - the channel direction (TX/RX) and channel name,
    *     - a monotonic hash tag shared by all flits of the same transaction,
    *     - decoded fields with their raw hex values in parentheses.
    *
    *   The transaction hash tag is stored directly on the Xaction's public
    *   Companion container, so no external map is required.
    */
    class CCHIFlitLogger {
    public:
        static const ConfigurationEntryBack<bool>       CONFIG_CCHI_VERBOSE;

    protected:
        using Cfg = CommonFlitConfigurationType1;
        using TaurusNode = CCHI::Taurus::UpstreamNode<Cfg>;

        static constexpr const char*                    LISTENER_NAME = "Cohestra.CCHIFlitLogger";

    protected:
        Instance&                                       instance;

        bool                                            enabled     = CONFIG_CCHI_VERBOSE.defaultValue;
        bool                                            attached    = false;

        uint64_t                                        nextHash    = 0;

        // hubs the listeners are registered on; deduplicated because a
        // harness may share one hub between all upstream nodes
        std::vector<std::shared_ptr<TaurusNode::EventHub>>
                                                        attachedHubs;

    public:
        explicit                                        CCHIFlitLogger(Instance& instance) noexcept;
                                                        ~CCHIFlitLogger() noexcept;

    public:
        bool                                            LoadConfiguration(const StringLoaderBase& loader) noexcept;

    public:
        void                                            Attach() noexcept;
        void                                            Detach() noexcept;
        bool                                            IsAttached() const noexcept;

        bool                                            IsEnabled() const noexcept;

    protected:
        template<class T>
        bool                                            LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

    protected:
        uint64_t                                        GetOrAssignHash(const std::shared_ptr<CCHI::Xact::Xaction<Cfg>>& xaction) noexcept;

    protected:
        // Channel formatters: return the decoded-field portion of the log line.
        std::string                                     FormatEVT(const CCHI::Flits::EVT<Cfg>& flit) const noexcept;
        std::string                                     FormatREQ(const CCHI::Flits::REQ<Cfg>& flit) const noexcept;
        std::string                                     FormatSNP(const CCHI::Flits::SNP<Cfg>& flit) const noexcept;
        std::string                                     FormatDnRSP(const CCHI::Flits::DnRSP<Cfg>& flit) const noexcept;
        std::string                                     FormatUpRSP(const CCHI::Flits::UpRSP<Cfg>& flit) const noexcept;
        std::string                                     FormatDnDAT(const CCHI::Flits::DnDAT<Cfg>& flit) const noexcept;
        std::string                                     FormatUpDAT(const CCHI::Flits::UpDAT<Cfg>& flit) const noexcept;

    protected:
        // Taurus event handlers (channel accepts)
        void                                            OnAcceptedEVT(CCHI::Taurus::UpstreamNodeXactAcceptedEVTEvent<Cfg>& event) noexcept;
        void                                            OnAcceptedSNP(CCHI::Taurus::UpstreamNodeXactAcceptedSNPEvent<Cfg>& event) noexcept;
        void                                            OnAcceptedREQ(CCHI::Taurus::UpstreamNodeXactAcceptedREQEvent<Cfg>& event) noexcept;
        void                                            OnAcceptedDnRSP(CCHI::Taurus::UpstreamNodeXactAcceptedDnRSPEvent<Cfg>& event) noexcept;
        void                                            OnAcceptedUpRSP(CCHI::Taurus::UpstreamNodeXactAcceptedUpRSPEvent<Cfg>& event) noexcept;
        void                                            OnAcceptedDnDAT(CCHI::Taurus::UpstreamNodeXactAcceptedDnDATEvent<Cfg>& event) noexcept;
        void                                            OnAcceptedUpDAT(CCHI::Taurus::UpstreamNodeXactAcceptedUpDATEvent<Cfg>& event) noexcept;
    };
}
