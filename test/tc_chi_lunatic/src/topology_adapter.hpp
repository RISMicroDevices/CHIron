#pragma once

#include "common.hpp"

namespace LunaticTest {

    template<typename config>
    class RequestTargetAdapter {
    public:
        using Node = CHI::Lunatic::HomeNode<config>;
        using REQ = typename Node::REQFlit;
        using TargetMap = std::function<CHI::Lunatic::NodeId(uint64_t, bool)>;

    private:
        Node&     node;
        TargetMap targetMap;

    public:
        RequestTargetAdapter(Node& node, TargetMap targetMap)
            : node(node), targetMap(std::move(targetMap))
        {
        }

        bool PushRXREQ(REQ flit)
        {
            flit.TgtID() = targetMap(flit.Addr(), flit.NS());
            return node.PushRXREQ(flit);
        }
    };

}
