//#pragma once

//#ifndef __CHI_B__CHI_LINK_CHANNELS
//#define __CHI_B__CHI_LINK_CHANNELS

#ifndef CHI_LINK_CHANNELS__STANDALONE
#   include "chi_link_channels_header.hpp"         // IWYU pragma: keep
#   include "chi_protocol_flits.hpp"
#endif



/*
namespace CHI::B {
*/

    namespace Channels {

        //
        template<class                              config      = FlitConfiguration<>,
                 CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
        class REQ {
        public:
            /*
            REQFLITPEND: 1 bit
            Request Flit Pending. Early indication that a request flit might be transmitted in the following 
            cycle. See Flit level clock gating on page 13-319.
            */
            using flitpend_t = uint1_t;

            /*
            REQFLITV: 1 bit
            Request Flit Valid. The transmitter sets this signal HIGH to indicate when REQFLIT[(R-1):0] is 
            valid.
            */
            using flitv_t = uint1_t;

            /*
            REQFLIT: bundle
            Request Flit. See Request flit on page 12-289 for a description of the request flit format.
            */
            using flit_t = Flits::REQ<config, conn>;

            /*
            REQLCRDV: 1 bit
            Request L-Credit Valid. The receiver sets this signal HIGH to return a request channel L-Credit to 
            a transmitter. See L-Credit flow control on page 13-317.
            */
            using lcrdv_t = uint1_t;

        
        // REQ channel signals
        public:
            as_pointer_if_t<conn::connectedIO,      flitpend_t>     FLITPEND;
            as_pointer_if_t<conn::connectedIO,      flitv_t>        FLITV;
            as_pointer_if_t<conn::connectedFlit,    flit_t>         FLIT;
            as_pointer_if_t<conn::connectedIO,      lcrdv_t>        LCRDV;
        };


        //
        template<class                              config      = FlitConfiguration<>,
                 CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
        class RSP {
        public:
            /*
            RSPFLITPEND: 1 bit
            Response Flit Pending. Early indication that a response flit might be transmitted in the following 
            cycle. See Flit level clock gating on page 13-319.
            */
            using flitpend_t = uint1_t;

            /*
            RSPFLITV: 1 bit
            Response Flit Valid. The transmitter sets this signal HIGH to indicate when RSPFLIT[(R-1):0] is
            valid.
            */
            using flitv_t = uint1_t;

            /*
            RSPFLIT: bundle
            Response Flit. See Response flit on page 12-290 for a description of the response flit format.
            */
            using flit_t = Flits::RSP<config, conn>;

            /*
            RSPLCRDV: 1 bit
            Response L-Credit Valid. The receiver sets this signal HIGH to return a response channel L-Credit to
            a transmitter. See L-Credit flow control on page 13-317.
            */
            using lcrdv_t = uint1_t;

        // RSP channel signals
        public:
            as_pointer_if_t<conn::connectedIO,      flitpend_t>     FLITPEND;
            as_pointer_if_t<conn::connectedIO,      flitv_t>        FLITV;
            as_pointer_if_t<conn::connectedFlit,    flit_t>         FLIT;
            as_pointer_if_t<conn::connectedIO,      lcrdv_t>        LCRDV;
        };


        // 
        template<class                              config      = FlitConfiguration<>,
                 CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
        class SNP {
        public:
            /*
            SNPFLITPEND: 1 bit
            Snoop Flit Pending. Early indication that a snoop flit might be transmitted in the following cycle.
            See Flit level clock gating on page 13-319.
            */
            using flitpend_t = uint1_t;

            /*
            SNPFLITV: 1 bit
            Snoop Flit Valid. The transmitter sets this signal HIGH to indicate when SNPFLIT[(R-1):0] is valid.
            */
            using flitv_t = uint1_t;

            /*
            SNPFLIT: bundle
            Snoop Flit. See Snoop flit on page 12-291 for a description of the snoop flit format.
            */
            using flit_t = Flits::SNP<config, conn>;

            /*
            SNPLCRDV: 1 bit
            Snoop L-Credit Valid. The receiver sets this signal HIGH to return a snoop channel L-Credit to a 
            transmitter. See L-Credit flow control on page 13-317.
            */
            using lcrdv_t = uint1_t;

        // SNP channel signals
        public:
            as_pointer_if_t<conn::connectedIO,      flitpend_t>     FLITPEND;
            as_pointer_if_t<conn::connectedIO,      flitv_t>        FLITV;
            as_pointer_if_t<conn::connectedFlit,    flit_t>         FLIT;
            as_pointer_if_t<conn::connectedIO,      lcrdv_t>        LCRDV;
        };


        //
        template<class                              config      = FlitConfiguration<>,
                 CHI::FlitLevelConnectionConcept    conn        = CHI::Connection<>>
        class DAT {
        public:
            /*
            DATFLITPEND: 1 bit
            Data Flit Pending. Early indication that a data flit might be transmitted in the following cycle. 
            See Flit level clock gating on page 13-319.
            */
            using flitpend_t = uint1_t;

            /*
            DATFLITV: 1 bit
            Data Flit Valid. The transmitter sets this signal HIGH to indicate when DATFLIT[(R-1):0] is valid.
            */
            using flitv_t = uint1_t;

            /*
            DATFLIT: bundle
            Data Flit. See Data flit on page 12-292 for a description of the data flit format.
            */
            using flit_t = Flits::DAT<config, conn>;

            /*
            DATLCRDV: 1 bit
            Data L-Credit Valid. The receiver sets this signal HIGH to return a data channel L-Credit to a 
            transmitter. See L-Credit flow control on page 13-317.
            */
            using lcrdv_t = uint1_t;

        // DAT channel signals
        public:
            as_pointer_if_t<conn::connectedIO,      flitpend_t>     FLITPEND;
            as_pointer_if_t<conn::connectedIO,      flitv_t>        FLITV;
            as_pointer_if_t<conn::connectedFlit,    flit_t>         FLIT;
            as_pointer_if_t<conn::connectedIO,      lcrdv_t>        LCRDV;
        };


        namespace RN {

            /*
            TXREQ. Outbound Request Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using TXREQ = Channels::REQ<config, conn>;

            /*
            TXRSP. Outbound Response Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using TXRSP = Channels::RSP<config, conn>;

            /*
            TXDAT. Outbound Data Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using TXDAT = Channels::DAT<config, conn>;

            /*
            RXRSP. Inbound Response Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using RXRSP = Channels::RSP<config, conn>;

            /*
            RXDAT. Inbound Data Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using RXDAT = Channels::DAT<config, conn>;

            /*
            RXSNP. Inbound Snoop Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using RXSNP = Channels::SNP<config, conn>;
        };


        namespace SN {

            /*
            RXREQ. Inbound Request Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using RXREQ = Channels::REQ<config, conn>;

            /*
            TXRSP. Outbound Response Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using TXRSP = Channels::RSP<config, conn>;

            /*
            TXDAT. Outbound Data Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using TXDAT = Channels::DAT<config, conn>;

            /*
            RXDAT. Inbound Data Channel.
            */
            template<class  config      = FlitConfiguration<>,
                     class  conn        = CHI::Connection<>>
            using RXDAT = Channels::DAT<config, conn>;
        };
    }

/*
}
*/
