#pragma once

#ifndef __XSDB2CLOG
#define __XSDB2CLOG

#include <cstdint>


namespace XSDB
{
    /*
    * CHI Channel encoding in ChiselDB::CHILog
    */
    //
    class CHIChannel {
    public:
        static constexpr const uint64_t     TXREQ       = 0;
        static constexpr const uint64_t     RXRSP       = 1;
        static constexpr const uint64_t     RXDAT       = 2;
        static constexpr const uint64_t     RXSNP       = 3;
        static constexpr const uint64_t     TXRSP       = 4;
        static constexpr const uint64_t     TXDAT       = 5;
    };
    //

    /*
    * SQLite column names in ChiselDB::CHILog
    */
    //
    static constexpr const char*    COLUMN_SITE         = "SITE";
    static constexpr const char*    COLUMN_CHANNEL      = "CHANNEL";
    static constexpr const char*    COLUMN_STAMP        = "STAMP";
    static constexpr const char*    COLUMN_FLIT0        = "FLITALL_0";
    static constexpr const char*    COLUMN_FLIT1        = "FLITALL_1";
    static constexpr const char*    COLUMN_FLIT2        = "FLITALL_2";
    static constexpr const char*    COLUMN_FLIT3        = "FLITALL_3";
    static constexpr const char*    COLUMN_FLIT4        = "FLITALL_4";
    static constexpr const char*    COLUMN_FLIT5        = "FLITALL_5";
    static constexpr const char*    COLUMN_FLIT6        = "FLITALL_6";
    //
}


#endif // __XSDB2CLOG
