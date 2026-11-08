#include "cohestra_lunatic.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Cohestra {

    LunaticModel::LunaticModel(CHI::CHIToRNInterface& rnInterface, Configuration configuration)
        : rnInterface(&rnInterface), configuration(std::move(configuration))
    {
        if (this->configuration.homeNodeId >= (1u << CHI::CommonFlitConfiguration::nodeIdWidth))
            throw std::invalid_argument("Lunatic home node ID exceeds the CHI field width");
        if (this->configuration.rnPorts.empty())
            throw std::invalid_argument("Lunatic requires at least one RN port");
        if (!this->configuration.serviceQuantum)
            throw std::invalid_argument("Lunatic service quantum must be non-zero");
        if (this->configuration.options.mteEnabled || this->configuration.options.dvmEnabled)
            throw std::invalid_argument("Lunatic MTE and DVM are prohibited");
        if (this->configuration.options.backend == ::CHI::Lunatic::BackendKind::CHI)
            throw std::invalid_argument("Lunatic Cohestra CHI subordinate routing is not connected");

        routes = this->configuration.rnPorts;
        std::sort(routes.begin(), routes.end(), [](const RNPort& a, const RNPort& b) { return a.port < b.port; });

        const auto present = rnInterface.GetPortIndices();
        for (const auto& route : routes)
        {
            if (!present.contains(route.port))
                throw std::invalid_argument("Lunatic RN port is absent from the CHI interface");
            if (route.nodeId == this->configuration.homeNodeId)
                throw std::invalid_argument("Lunatic RN and home node IDs must differ");
            if (route.nodeId >= (1u << CHI::CommonFlitConfiguration::nodeIdWidth))
                throw std::invalid_argument("Lunatic RN node ID exceeds the CHI field width");
            if (!portByNode.emplace(route.nodeId, route.port).second)
                throw std::invalid_argument("Lunatic RN node IDs must be unique");
            for (auto previous = routes.begin(); &route != &*previous; ++previous)
                if (previous->port == route.port)
                    throw std::invalid_argument("Lunatic RN port assignments must be unique");
        }

        if (this->configuration.options.backend == ::CHI::Lunatic::BackendKind::Internal)
            home = std::make_unique<Interconnect>(this->configuration.homeNodeId,
                this->configuration.options, this->configuration.events);
        else
            home = std::make_unique<Bridge>(this->configuration.homeNodeId,
                this->configuration.options, this->configuration.events);
        Home().Reset();
    }

    LunaticModel::HomeNode& LunaticModel::Home() noexcept
    {
        return std::visit([](auto& node) -> HomeNode& { return node->GetNode(); }, home);
    }

    const LunaticModel::HomeNode& LunaticModel::Home() const noexcept
    {
        return std::visit([](const auto& node) -> const HomeNode& { return node->GetNode(); }, home);
    }

    LunaticModel::HomeNode& LunaticModel::GetNode() noexcept { return Home(); }
    const LunaticModel::HomeNode& LunaticModel::GetNode() const noexcept { return Home(); }

    const std::vector<std::string>& LunaticModel::GetIntegrationErrors() const noexcept
    {
        return integrationErrors;
    }

    const CHI::CHITransportStatusInterface* LunaticModel::TransportStatus() const noexcept
    {
        return dynamic_cast<const CHI::CHITransportStatusInterface*>(rnInterface);
    }

    void LunaticModel::Reset()
    {
        if (configuration.resetPolicy == ResetPolicy::IdleOnly && !IsDrained())
        {
            integrationErrors.emplace_back("Lunatic idle-only reset requires a drained model");
            return;
        }
        auto* transportReset = dynamic_cast<CHI::CHITransportResetInterface*>(rnInterface);
        if (!transportReset)
        {
            ReportError("Lunatic transport does not support coordinated reset");
            return;
        }
        transportReset->Reset();
        Home().Reset();
        snoopAllowed.clear();
        integrationErrors.clear();
        nextPort = 0;
        lastTickTime = 0;
        lastTickValid = false;
        failed = false;
    }

    std::optional<size_t> LunaticModel::FindPort(NodeId nodeId) const noexcept
    {
        const auto iter = portByNode.find(nodeId);
        if (iter == portByNode.end())
            return std::nullopt;
        return iter->second;
    }

    bool LunaticModel::HasSnoopActivity(size_t port) const noexcept
    {
        for (const auto& route : routes)
            if (route.port == port)
                return Home().HasSnoopActivity(route.nodeId);
        return true;
    }

    void LunaticModel::SetSnoopAllowed(size_t port, bool allowed) noexcept
    {
        snoopAllowed[port] = allowed;
    }

    void LunaticModel::ReportError(std::string reason) noexcept
    {
        if (!failed)
            integrationErrors.push_back(std::move(reason));
        failed = true;
    }

    bool LunaticModel::ValidateSource(const RNPort& route, NodeId source) noexcept
    {
        if (source == route.nodeId)
            return true;
        ReportError("Lunatic RN source ID does not match its port assignment");
        return false;
    }

    void LunaticModel::ServeRequests(const RNPort& route) noexcept
    {
        for (size_t count = 0; count < configuration.serviceQuantum && !failed; ++count)
        {
            const auto flit = rnInterface->PeekTXREQ(route.port);
            if (!flit)
                return;
            if (!ValidateSource(route, flit->SrcID()))
                return;
            if (flit->TgtID() != configuration.homeNodeId)
            {
                ReportError("Lunatic RN request targets an unknown home");
                return;
            }
            if (Home().PushRXREQ(*flit))
                rnInterface->PopTXREQ(route.port);
            else
                return;
        }
    }

    void LunaticModel::ServeResponses(const RNPort& route) noexcept
    {
        for (size_t count = 0; count < configuration.serviceQuantum && !failed; ++count)
        {
            const auto flit = rnInterface->PeekTXRSP(route.port);
            if (!flit)
                return;
            if (!ValidateSource(route, flit->SrcID()))
                return;

            bool accepted = false;
            if (flit->TgtID() == configuration.homeNodeId)
                accepted = Home().PushRXRSP(*flit);
            else if (FindPort(flit->TgtID()))
            {
                ReportError("Lunatic RN response cannot target a peer");
                return;
            }
            else
            {
                ReportError("Lunatic RN response targets an unknown node");
                return;
            }

            if (accepted)
                rnInterface->PopTXRSP(route.port);
            else
                return;
        }
    }

    void LunaticModel::ServeData(const RNPort& route) noexcept
    {
        for (size_t count = 0; count < configuration.serviceQuantum && !failed; ++count)
        {
            const auto flit = rnInterface->PeekTXDAT(route.port);
            if (!flit)
                return;
            if (!ValidateSource(route, flit->SrcID()))
                return;

            bool accepted = false;
            if (flit->TgtID() == configuration.homeNodeId)
                accepted = Home().PushRXDAT(*flit);
            else if (const auto target = FindPort(flit->TgtID()))
            {
                if (flit->Opcode() != ::CHI::Eb::Opcodes::DAT::CompData)
                {
                    ReportError("Lunatic RN data opcode cannot target a peer");
                    return;
                }
                accepted = rnInterface->PushRXDAT(*target, *flit);
            }
            else
            {
                ReportError("Lunatic RN data targets an unknown node");
                return;
            }

            if (accepted)
                rnInterface->PopTXDAT(route.port);
            else
                return;
        }
    }

    void LunaticModel::EmitResponses() noexcept
    {
        for (size_t count = 0; count < configuration.serviceQuantum && !failed; ++count)
        {
            const auto flit = Home().PeekTXRSP(false);
            if (!flit)
                return;
            const auto target = FindPort(flit->TgtID());
            if (!target)
            {
                ReportError("Lunatic home response targets an unknown RN");
                return;
            }
            if (rnInterface->PushRXRSP(*target, *flit))
                Home().PopTXRSP(false);
            else
                return;
        }
    }

    void LunaticModel::EmitData() noexcept
    {
        for (size_t count = 0; count < configuration.serviceQuantum && !failed; ++count)
        {
            const auto flit = Home().PeekTXDAT(false);
            if (!flit)
                return;
            const auto target = FindPort(flit->TgtID());
            if (!target)
            {
                ReportError("Lunatic home data targets an unknown RN");
                return;
            }
            if (rnInterface->PushRXDAT(*target, *flit))
                Home().PopTXDAT(false);
            else
                return;
        }
    }

    void LunaticModel::EmitSnoops() noexcept
    {
        for (size_t count = 0; count < configuration.serviceQuantum && !failed; ++count)
        {
            const auto emission = Home().PeekTXSNP();
            if (!emission)
                return;
            const auto target = FindPort(emission->target);
            if (!target)
            {
                ReportError("Lunatic snoop targets an unknown RN");
                return;
            }
            const auto allowed = snoopAllowed.find(*target);
            if (allowed != snoopAllowed.end() && !allowed->second)
                return;
            if (rnInterface->PushRXSNP(*target, emission->flit))
                Home().PopTXSNP();
            else
                return;
        }
    }

    void LunaticModel::Tick(uint64_t time)
    {
        if (failed || (lastTickValid && lastTickTime == time))
            return;
        if (time & 1)
        {
            ReportError("Lunatic Cohestra timestamps must be even");
            return;
        }
        if (lastTickValid && time < lastTickTime)
        {
            ReportError("Lunatic Cohestra time must not decrease");
            return;
        }
        lastTickTime = time;
        lastTickValid = true;
        Evaluate();
    }

    void LunaticModel::Evaluate()
    {
        if (failed)
            return;
        for (size_t offset = 0; offset < routes.size() && !failed; ++offset)
        {
            const RNPort& route = routes[(nextPort + offset) % routes.size()];
            ServeRequests(route);
            ServeResponses(route);
            ServeData(route);
        }
        nextPort = routes.empty() ? 0 : (nextPort + 1) % routes.size();
        if (failed)
            return;

        Home().Tick(lastTickValid ? lastTickTime / 2 : 0);
        EmitResponses();
        EmitData();
        EmitSnoops();
    }

    bool LunaticModel::Drain()
    {
        return Home().Drain() && IsDrained();
    }

    bool LunaticModel::IsDrained() const noexcept
    {
        const auto* status = TransportStatus();
        return !Home().HasInflight() && status && status->IsDrained();
    }

    bool LunaticModel::Validate() const noexcept
    {
        const auto* status = TransportStatus();
        return !failed && integrationErrors.empty() && Home().GetErrors().empty() &&
            Home().ValidateInvariants() && status && status->ValidateCreditAccounting();
    }

    bool LunaticModel::HasErrors() const noexcept
    {
        return failed || !integrationErrors.empty() || !Home().GetErrors().empty();
    }

}
