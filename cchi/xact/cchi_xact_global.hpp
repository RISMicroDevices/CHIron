#pragma once

#ifndef __CCHI__CCHI_XACT_GLOBAL
#define __CCHI__CCHI_XACT_GLOBAL

#include "../spec/cchi_protocol_flits.hpp"

#include "cchi_xact_base/cchi_xact_base_topology.hpp"


namespace CCHI::Xact {

    using GlobalTopology = Topology;

    template<FlitConfigurationConcept config>
    class Global {
    public:
        /*
        Topology of nodes, providing node IDs and types.
        */
        GlobalTopology
            TOPOLOGY;

    public:
        Global() noexcept;
    };
}

#endif // __CCHI__CCHI_XACT_GLOBAL
