//#pragma once

//#ifndef __CHI_B__CHI_LINK_INTERFACES
//#define __CHI_B__CHI_LINK_INTERFACES

#ifndef CHI_LINK_INTERFACES__STANDALONE
#   include "chi_link_interfaces_header.hpp"        // IWYU pragma: keep
#   include "chi_link_channels.hpp"
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_LINK_INTERFACES_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_LINK_INTERFACES_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_LINK_INTERFACES_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_LINK_INTERFACES_EB
#endif


/*
namespace CHI {
*/   
    namespace Interfaces {

        //
        class TX {};
        class RX {};

        //
        template<class T>
        concept InterfaceTXConcept = std::derived_from<T, Interfaces::TX>;

        //
        template<class T>
        concept InterfaceRXConcept = std::derived_from<T, Interfaces::RX>;


        //
        template<class T>
        concept InterfaceWithTXConcept = InterfaceTXConcept<typename T::TX>;

        //
        template<class T>
        concept InterfaceWithRXConcept = InterfaceRXConcept<typename T::RX>;
        

        //
        namespace RN {

            /*
            RN-F interface.
            */
            template<class                              config  = FlitConfiguration<>,
                     CHI::ChannelLevelConnectionConcept conn    = CHI::Connection<>>
            class F {
            public:

                /*
                RN-F TX interface.
                */
                class TX : public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXDAT<config, conn>>  DAT;
                } TX;

                /*
                RN-F RX interface.
                */
                class RX : public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXDAT<config, conn>>  DAT;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXSNP<config, conn>>  SNP;
                } RX;
            };


            /*
            RN-D interface.
            */
            template<class                              config  = FlitConfiguration<>,
                     CHI::ChannelLevelConnectionConcept conn    = CHI::Connection<>>
            class D {
            public:

                /*
                RN-D TX interface.
                */
                class TX : public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXDAT<config, conn>>  DAT;
                } TX;

                /*
                RN-D RX interface.
                */
                class RX : public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXDAT<config, conn>>  DAT;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXSNP<config, conn>>  SNP;
                } RX;
            };

            
            /*
            RN-I interface.
            */
            template<class                              config  = FlitConfiguration<>,
                     CHI::ChannelLevelConnectionConcept conn    = CHI::Connection<>>
            class I {
            public:

                /*
                RN-I TX interface.
                */
                class TX : public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXDAT<config, conn>>  DAT;
                } TX;

                /*
                RN-I RX interface.
                */
                class RX : public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXDAT<config, conn>>  DAT;
                } RX;
            };
        }


        //
        namespace SN {

            /*
            SN-F interface.
            */
            template<class                              config  = FlitConfiguration<>,
                     CHI::ChannelLevelConnectionConcept conn    = CHI::Connection<>>
            class F {
            public:

                /*
                SN-F TX interface.
                */
                class TX : public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXDAT<config, conn>>  DAT;
                } TX;

                /*
                SN-F RX interface.
                */
                class RX : public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXDAT<config, conn>>  DAT;
                } RX;
            };


            /*
            SN-I interface.
            */
            template<class                              config  = FlitConfiguration<>,
                     CHI::ChannelLevelConnectionConcept conn    = CHI::Connection<>>
            class I {
            public:

                /*
                SN-I TX interface.
                */
                class TX : public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXDAT<config, conn>>  DAT;
                } TX;

                /*
                SN-I RX interface.
                */
                class RX : public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXDAT<config, conn>>  DAT;
                } RX;
            };
        }
    }
/*
}
*/


#endif // __CHI__CHI_LINK_INTERFACES_*

//#endif // __CHI_B__CHI_LINK_INTERFACES
