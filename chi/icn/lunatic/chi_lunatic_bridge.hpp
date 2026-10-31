#pragma once

#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {

    template<CHI::Eb::FlitConfigurationConcept config>
    class Bridge {
    public:
        using Node = HomeNode<config>;
        using REQFlit = typename Node::REQFlit;
        using RSPFlit = typename Node::RSPFlit;
        using DATFlit = typename Node::DATFlit;
        using SNPEmission = typename Node::SNPEmission;

    private:
        Node node;

    public:
        explicit Bridge(NodeId nodeId, Options options = {}, std::shared_ptr<EventHub> events = nullptr)
            : node(nodeId, std::move(options), std::move(events))
        {
        }

        Node&       GetNode() noexcept { return node; }
        const Node& GetNode() const noexcept { return node; }

        void Reset() { node.Reset(); }
        void Tick(Time time) { node.Tick(time); }
        bool Drain() { return node.Drain(); }
        bool HasInflight() const noexcept { return node.HasInflight(); }

        bool PushUpstreamREQ(const REQFlit& flit) { return node.PushRXREQ(flit); }
        bool PushUpstreamRSP(const RSPFlit& flit) { return node.PushRXRSP(flit); }
        bool PushUpstreamDAT(const DATFlit& flit) { return node.PushRXDAT(flit); }
        bool PushDownstreamRSP(const RSPFlit& flit) { return node.PushRXRSP(flit); }
        bool PushDownstreamDAT(const DATFlit& flit) { return node.PushRXDAT(flit); }
        bool PushDownstreamB(const AXIWriteResponse& response) { return node.PushRXB(response); }
        bool PushDownstreamR(const AXIReadData& response) { return node.PushRXR(response); }

        std::optional<AXIAddress>   PeekDownstreamAW() const { return node.PeekTXAW(); }
        std::optional<AXIWriteData> PeekDownstreamW() const { return node.PeekTXW(); }
        std::optional<AXIAddress>   PeekDownstreamAR() const { return node.PeekTXAR(); }
        std::optional<AXIAddress>   PopDownstreamAW() { return node.PopTXAW(); }
        std::optional<AXIWriteData> PopDownstreamW() { return node.PopTXW(); }
        std::optional<AXIAddress>   PopDownstreamAR() { return node.PopTXAR(); }

        std::optional<RSPFlit>      PeekUpstreamRSP() const { return node.PeekTXRSP(false); }
        std::optional<DATFlit>      PeekUpstreamDAT() const { return node.PeekTXDAT(false); }
        std::optional<SNPEmission> PeekUpstreamSNP() const { return node.PeekTXSNP(); }
        std::optional<REQFlit>      PeekDownstreamREQ() const { return node.PeekTXREQ(); }
        std::optional<RSPFlit>      PeekDownstreamRSP() const { return node.PeekTXRSP(true); }
        std::optional<DATFlit>      PeekDownstreamDAT() const { return node.PeekTXDAT(true); }

        std::optional<RSPFlit>      PopUpstreamRSP() { return node.PopTXRSP(false); }
        std::optional<DATFlit>      PopUpstreamDAT() { return node.PopTXDAT(false); }
        std::optional<SNPEmission> PopUpstreamSNP() { return node.PopTXSNP(); }
        std::optional<REQFlit>      PopDownstreamREQ() { return node.PopTXREQ(); }
        std::optional<RSPFlit>      PopDownstreamRSP() { return node.PopTXRSP(true); }
        std::optional<DATFlit>      PopDownstreamDAT() { return node.PopTXDAT(true); }
    };

}
