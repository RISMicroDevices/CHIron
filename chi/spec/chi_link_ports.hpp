#pragma once

//#ifndef __CHI__CHI_LINK_PORTS
//#define __CHI__CHI_LINK_PORTS

#ifndef CHI_LINK_PORTS__STANDALONE
#   include "chi_link_links.hpp"
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_LINK_PORTS_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_LINK_PORTS_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_LINK_PORTS_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_LINK_PORTS_EB
#endif


/*
namespace CHI::B {
*/
    namespace Ports {

        //
        template<class T>
        concept LinkBoundConcept = Links::LinkBoundConcept<T>;


        /*
        Port.
        */
        template<LinkBoundConcept   link,
                 class              config      = FlitConfiguration<>,
                 class              conn        = CHI::Connection<>>
        class Port : public link::Outbound, public link::Inbound {
        public:
            /*
            TXSACTIVE: 1 bit
            See 13.7 Protocol layer activity indication on 13-332.
            */
            using txsactive_t = uint1_t;

            /*
            RXSACTIVE: 1 bit
            See 13.7 Protocol layer activity indication on 13-332.
            */
            using rxsactive_t = uint1_t;


        // Port signals (extended), others from link::Outbound and link::Inbound
        public:
            as_pointer_if_t<conn::connectedIO,          txsactive_t>    TXSACTIVE;
            as_pointer_if_t<conn::connectedIO,          rxsactive_t>    RXSACTIVE;
        };


        //
        namespace RN {

            /*
            Port of RN-F.
            */
            template<class                              config      = FlitConfiguration<>,
                     CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
            using F = Port<Links::RN::F<config, conn>>;

            /*
            Port of RN-D.
            */
            template<class                              config      = FlitConfiguration<>,
                     CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
            using D = Port<Links::RN::D<config, conn>>;

            /*
            Port of RN-I.
            */
            template<class                              config      = FlitConfiguration<>,
                     CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
            using I = Port<Links::RN::I<config, conn>>;
        };


        //
        namespace SN {

            /*
            Ports of SN-F.
            */
            template<class                              config      = FlitConfiguration<>,
                     CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
            using F = Port<Links::SN::F<config, conn>>;

            /*
            Ports of SN-I.
            */
            template<class                              config      = FlitConfiguration<>,
                     CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
            using I = Port<Links::SN::I<config, conn>>;
        };
    }
/*
}
*/


#endif // __CHI__CHI_LINK_PORTS_*

//#endif // __CHI__CHI_LINK_PORTS
