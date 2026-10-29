#pragma once

#include "chi_lunatic_component.hpp"

namespace CHI::Lunatic {

    template<CHI::Eb::FlitConfigurationConcept config>
    class Interconnect {
    public:
        using Node = HomeNode<config>;
        using REQFlit = typename Node::REQFlit;
        using RSPFlit = typename Node::RSPFlit;
        using DATFlit = typename Node::DATFlit;
        using SNPEmission = typename Node::SNPEmission;

    private:
        Node                        node;

    public:
        explicit Interconnect(NodeId nodeId, Options options = {}, std::shared_ptr<EventHub> events = nullptr)
            : node(nodeId, std::move(options), std::move(events))
        {
        }

        Node&                       GetNode() noexcept { return node; }
        const Node&                 GetNode() const noexcept { return node; }

        void                        Reset() { node.Reset(); }
        void                        Tick(Time time) { node.Tick(time); }
        bool                        Drain() { return node.Drain(); }
        bool                        HasInflight() const noexcept { return node.HasInflight(); }

        bool                        PushRXREQ(const REQFlit& flit) { return node.PushRXREQ(flit); }
        bool                        PushRXRSP(const RSPFlit& flit) { return node.PushRXRSP(flit); }
        bool                        PushRXDAT(const DATFlit& flit) { return node.PushRXDAT(flit); }

        std::optional<REQFlit>      PeekTXREQ() const { return node.PeekTXREQ(); }
        std::optional<RSPFlit>      PeekTXRSP() const { return node.PeekTXRSP(); }
        std::optional<DATFlit>      PeekTXDAT() const { return node.PeekTXDAT(); }
        std::optional<SNPEmission>  PeekTXSNP() const { return node.PeekTXSNP(); }

        std::optional<REQFlit>      PopTXREQ() { return node.PopTXREQ(); }
        std::optional<RSPFlit>      PopTXRSP() { return node.PopTXRSP(); }
        std::optional<DATFlit>      PopTXDAT() { return node.PopTXDAT(); }
        std::optional<SNPEmission>  PopTXSNP() { return node.PopTXSNP(); }
    };

}
