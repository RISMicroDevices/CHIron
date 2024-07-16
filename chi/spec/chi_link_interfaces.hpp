//#pragma once

//#ifndef __CHI_B__CHI_LINK_INTERFACES
//#define __CHI_B__CHI_LINK_INTERFACES

#ifndef CHI_LINK_INTERFACES__STANDALONE
#   include "chi_link_interfaces_header.hpp"        // IWYU pragma: keep
#   include "chi_link_channels.hpp"
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
                class TX : public F, public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXDAT<config, conn>>  DAT;
                };

                /*
                RN-F RX interface.
                */
                class RX : public F, public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXDAT<config, conn>>  DAT;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXSNP<config, conn>>  SNP;
                };
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
                class TX : public D, public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXDAT<config, conn>>  DAT;
                };

                /*
                RN-D RX interface.
                */
                class RX : public D, public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXDAT<config, conn>>  DAT;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXSNP<config, conn>>  SNP;
                };
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
                class TX : public I, public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXREQ<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::TXDAT<config, conn>>  DAT;
                };

                /*
                RN-I RX interface.
                */
                class RX : public I, public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::RN::RXDAT<config, conn>>  DAT;
                };
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
                class TX : public F, public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXDAT<config, conn>>  DAT;
                };

                /*
                SN-F RX interface.
                */
                class RX : public F, public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXDAT<config, conn>>  DAT;
                };
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
                class TX : public I, public Interfaces::TX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXRSP<config, conn>>  RSP;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::TXDAT<config, conn>>  DAT;
                };

                /*
                SN-I RX interface.
                */
                class RX : public I, public Interfaces::RX {
                public:
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXREQ<config, conn>>  REQ;
                    as_pointer_if_t<conn::connectedChannel, Channels::SN::RXDAT<config, conn>>  DAT;
                };
            };
        }
    }
/*
}
*/

//#endif // __CHI_B__CHI_LINK_INTERFACES
