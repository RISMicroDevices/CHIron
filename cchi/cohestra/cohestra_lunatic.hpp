#pragma once

#include "cohestra_chi.hpp"

#include "../../chi/icn/lunatic/chi_lunatic_bridge.hpp"
#include "../../chi/icn/lunatic/chi_lunatic_interconnect.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Cohestra {

    class LunaticModel {
    public:
        using Config      = ::CHI::Lunatic::Options;
        using NodeId      = ::CHI::Lunatic::NodeId;
        using EventHub    = ::CHI::Lunatic::EventHub;
        using HomeNode    = ::CHI::Lunatic::HomeNode<CHI::CommonFlitConfiguration>;
        using Interconnect = ::CHI::Lunatic::Interconnect<CHI::CommonFlitConfiguration>;
        using Bridge      = ::CHI::Lunatic::Bridge<CHI::CommonFlitConfiguration>;

        enum class ResetPolicy : uint8_t {
            CoordinatedHard,
            IdleOnly
        };

        struct RNPort {
            size_t  port = 0;
            NodeId  nodeId = 0;
        };

        struct Configuration {
            NodeId                      homeNodeId = 0;
            Config                      options{};
            std::vector<RNPort>         rnPorts;
            size_t                      serviceQuantum = 1;
            ResetPolicy                 resetPolicy = ResetPolicy::CoordinatedHard;
            std::shared_ptr<EventHub>   events;
        };

    private:
        using HomeVariant = std::variant<std::unique_ptr<Interconnect>, std::unique_ptr<Bridge>>;

        CHI::CHIToRNInterface*              rnInterface = nullptr;
        Configuration                       configuration;
        HomeVariant                         home;
        std::vector<RNPort>                 routes;
        std::map<NodeId, size_t>            portByNode;
        std::map<size_t, bool>              snoopAllowed;
        std::vector<std::string>            integrationErrors;
        size_t                              nextPort = 0;
        uint64_t                            lastTickTime = 0;
        bool                                lastTickValid = false;
        bool                                failed = false;

    public:
        LunaticModel(CHI::CHIToRNInterface& rnInterface, Configuration configuration);

        void        Reset();
        void        Tick(uint64_t time);
        void        Evaluate();
        bool        Drain();
        bool        IsDrained() const noexcept;
        bool        Validate() const noexcept;
        bool        HasErrors() const noexcept;

        HomeNode&                       GetNode() noexcept;
        const HomeNode&                 GetNode() const noexcept;
        const std::vector<std::string>& GetIntegrationErrors() const noexcept;
        bool                            HasSnoopActivity(size_t port) const noexcept;
        void                            SetSnoopAllowed(size_t port, bool allowed) noexcept;

    private:
        HomeNode&                           Home() noexcept;
        const HomeNode&                     Home() const noexcept;
        const CHI::CHITransportStatusInterface* TransportStatus() const noexcept;
        std::optional<size_t>               FindPort(NodeId nodeId) const noexcept;
        void                                ReportError(std::string reason) noexcept;
        bool                                ValidateSource(const RNPort& route, NodeId source) noexcept;

        void                                ServeRequests(const RNPort& route) noexcept;
        void                                ServeResponses(const RNPort& route) noexcept;
        void                                ServeData(const RNPort& route) noexcept;
        void                                EmitResponses() noexcept;
        void                                EmitData() noexcept;
        void                                EmitSnoops() noexcept;
    };

}
