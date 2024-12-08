// #pragma once

// #ifndef __CHI__CHI_LINK_LINKS
// #define __CHI__CHI_LINK_LINKS

#ifndef CHI_LINK_LINKS__STANDALONE
#   include "chi_link_links_header.hpp"
#   include "chi_link_interfaces.hpp"
#endif


#if !defined(__CHI__CHI_LINK_LINKS_B) \
 && !defined(__CHI__CHI_LINK_LINKS_EB)

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_LINK_LINKS_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_LINK_LINKS_EB
#endif


/*
namespace CHI {
*/
    namespace Links {

        //
        template<class T>
        concept InterfaceWithTXConcept = Interfaces::InterfaceWithTXConcept<T>;

        template<class T>
        concept InterfaceWithRXConcept = Interfaces::InterfaceWithRXConcept<T>;


        /*
        Outbound link.
        */
        template<InterfaceWithTXConcept                 inf,
                 class                                  config  = FlitConfiguration<>,
                 CHI::InterfaceLevelConnectionConcept   conn    = CHI::Connection<>>
        class Outbound {
        public:
            /*
            TXLINKACTIVEREQ: 1 bit
            See 13.5 Interface activation and deactivation on 13-320.
            */
            using txlinkactivereq_t = uint1_t;

            /*
            TXLINKACTIVEACK: 1 bit
            See 13.5 Interface activation and deactivation on 13-320.
            */
            using txlinkactiveack_t = uint1_t;

            /*
            TX: interface bundle
            */
            using tx_t = inf::TX;


        // Outbound Link signals
        public:
            as_pointer_if_t<conn::connectedIO,          txlinkactivereq_t>  TXLINKACTIVEREQ;
            as_pointer_if_t<conn::connectedIO,          txlinkactiveack_t>  TXLINKACTIVEACK;
            as_pointer_if_t<conn::connectedInterface,   tx_t>               TX;
        };


        /*
        Inbound link.
        */
        template<InterfaceWithRXConcept                 inf,
                 class                                  config  = FlitConfiguration<>,
                 CHI::InterfaceLevelConnectionConcept   conn    = CHI::Connection<>>
        class Inbound {
        public:
            /*
            RXLINKACTIVEREQ: 1 bit
            See 13.5 Interface activation and deactivation on 13-320.
            */
            using rxlinkactivereq_t = uint1_t;

            /*
            RXLINKACTIVEACK: 1 bit
            See 13.5 Interface activation and deactivation on 13-320.
            */
            using rxlinkactiveack_t = uint1_t;

            /*
            RX: interface bundle
            */
            using rx_t = inf::RX;

        
        // Inbound Link signals
        public:
            as_pointer_if_t<conn::connectedIO,          rxlinkactivereq_t>  RXLINKACTIVEREQ;
            as_pointer_if_t<conn::connectedIO,          rxlinkactiveack_t>  RXLINKACTIVEACK;
            as_pointer_if_t<conn::connectedInterface,   rx_t>               RX;
        };


        //
        template<class T>
        concept LinkBoundConcept = std::is_class_v<typename T::Outbound> && std::is_class_v<typename T::Inbound>;


        //
        namespace RN {

            /*
            Links of RN-F.
            */
            template<class                                  config  = FlitConfiguration<>,
                     CHI::InterfaceLevelConnectionConcept   conn    = CHI::Connection<>>
            class F {
            public:
                using Outbound  = Links::Outbound   <Interfaces::RN::F<config, conn>, config, conn>;
                using Inbound   = Links::Inbound    <Interfaces::RN::F<config, conn>, config, conn>;
            };

            /*
            Links of RN-D.
            */
            template<class                                  config  = FlitConfiguration<>,
                     CHI::InterfaceLevelConnectionConcept   conn    = CHI::Connection<>>
            class D {
            public:
                using Outbound  = Links::Outbound   <Interfaces::RN::D<config, conn>, config, conn>;
                using Inbound   = Links::Inbound    <Interfaces::RN::D<config, conn>, config, conn>;
            };

            /*
            Links of RN-I.
            */
            template<class                                  config  = FlitConfiguration<>,
                     CHI::InterfaceLevelConnectionConcept   conn    = CHI::Connection<>>
            class I {
            public:
                using Outbound  = Links::Outbound   <Interfaces::RN::I<config, conn>, config, conn>;
                using Inbound   = Links::Inbound    <Interfaces::RN::I<config, conn>, config, conn>;
            };
        };


        //
        namespace SN {

            /*
            Links of SN-F.
            */
            template<class                                  config  = FlitConfiguration<>,
                     CHI::InterfaceLevelConnectionConcept   conn    = CHI::Connection<>>
            class F {
            public:
                using Outbound  = Links::Outbound   <Interfaces::SN::F<config, conn>, config, conn>;
                using Inbound   = Links::Inbound    <Interfaces::SN::F<config, conn>, config, conn>;
            };

            /*
            Links of SN-I.
            */
            template<class                                  config  = FlitConfiguration<>,
                     CHI::InterfaceLevelConnectionConcept   conn    = CHI::Connection<>>
            class I {
            public:
                using Outbound  = Links::Outbound   <Interfaces::SN::I<config, conn>, config, conn>;
                using Inbound   = Links::Inbound    <Interfaces::SN::I<config, conn>, config, conn>;
            };
        };
    }
/*
}
*/


#endif // __CHI__CHI_LINK_LINKS_*

// #endif // __CHI__CHI_LINK_LINKS
