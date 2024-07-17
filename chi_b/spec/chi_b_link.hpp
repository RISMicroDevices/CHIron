#pragma once

#ifndef __CHI_B__CHI_LINK
#define __CHI_B__CHI_LINK

#include "chi_b_link_channels.hpp"          // IWYU pragma: export
#include "chi_b_link_interfaces.hpp"        // IWYU pragma: export
#include "chi_b_link_links.hpp"             // IWYU pragma: export
#include "chi_b_link_ports.hpp"             // IWYU pragma: export

// CHI Link Layer headers


// CHI Link Layer utilities in CHI root namespace
namespace CHI::B {

    /*
    General Port instance.
    -------------------------------------
    Example:
        RN-F Port: Port<Links::RN::F<config, conn>>
        SN-I Port: Port<Links::SN::I<config, conn>>
    or with all default configurations:
        RN-F Port: Port<Links::RN::F<>>
        SN-I Port: Port<Links::SN::I<>>
    */
    template<Ports::LinkBoundConcept    link,
             class                      config      = FlitConfiguration<>,
             class                      conn        = Connection<>>
    using Port = CHI::B::Ports::Port<link, config, conn>;


    /*
    Port instances of RN.
    */
    template<class  config  = FlitConfiguration<>,
             class  conn    = Connection<>>
    using PortRN_F = CHI::B::Ports::RN::F<config, conn>;

    template<class  config  = FlitConfiguration<>,
             class  conn    = Connection<>>
    using PortRN_D = CHI::B::Ports::RN::D<config, conn>;

    template<class  config  = FlitConfiguration<>,
             class  conn    = Connection<>>
    using PortRN_I = CHI::B::Ports::RN::I<config, conn>;

    /*
    Port instances of SN.
    */
    template<class  config  = FlitConfiguration<>,
             class  conn    = Connection<>>
    using PortSN_F = CHI::B::Ports::SN::F<config, conn>;

    template<class  config  = FlitConfiguration<>,
             class  conn    = Connection<>>
    using PortSN_I = CHI::B::Ports::SN::I<config, conn>;
}


#endif // __CHI_B__CHI_LINK
