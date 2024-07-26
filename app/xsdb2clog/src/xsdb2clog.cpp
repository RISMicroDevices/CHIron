#include <sqlite3.h>
#include <getopt.h>

#include <exception>
#include <iostream>
#include <ostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>

#include "xsdb2clog.hpp"

#include "../../../clog/clog_b.hpp"
#include "../../../clog/clog_t.hpp"


std::string     current_site;
uint64_t        current_stamp;
uint64_t        current_channel;
uint64_t        current_flit[7];

size_t          record_count = 0;

//
std::ostream*   output;

//
CLog::Channel GetChannel(uint64_t xsdbChannel)
{
    switch (xsdbChannel)
    {
        case XSDB::CHIChannel::TXREQ:   return CLog::Channel::TXREQ;
        case XSDB::CHIChannel::RXRSP:   return CLog::Channel::RXRSP;
        case XSDB::CHIChannel::RXDAT:   return CLog::Channel::RXDAT;
        case XSDB::CHIChannel::RXSNP:   return CLog::Channel::RXSNP;
        case XSDB::CHIChannel::TXRSP:   return CLog::Channel::TXRSP;
        case XSDB::CHIChannel::TXDAT:   return CLog::Channel::TXDAT;
        default:                        break;
    }

    std::cerr << "%ERROR: unrecognized CHI channel (" << xsdbChannel << ") from xsdb" << std::endl;

    throw std::exception();
}

//
static void CallbackSite(const std::string& str)
{
    current_site = str;
}

static void CallbackStamp(uint64_t val)
{
    current_stamp = val;
}

static void CallbackChannel(uint64_t val)
{
    current_channel = val;
}

static void CallbackFlit0(uint64_t val)
{
    current_flit[0] = val;
}

static void CallbackFlit1(uint64_t val)
{
    current_flit[1] = val;
}

static void CallbackFlit2(uint64_t val)
{
    current_flit[2] = val;
}

static void CallbackFlit3(uint64_t val)
{
    current_flit[3] = val;
}

static void CallbackFlit4(uint64_t val)
{
    current_flit[4] = val;
}

static void CallbackFlit5(uint64_t val)
{
    current_flit[5] = val;
}

static void CallbackFlit6(uint64_t val)
{
    current_flit[6] = val;
}

static void print_version() noexcept
{
    std::cerr << std::endl;
    std::cerr << "                CHIron Toolset" << std::endl;
    std::cerr << std::endl;
    std::cerr << "           xsdb2clog - " << __DATE__ << std::endl;
    std::cerr << std::endl;
    std::cerr << "  xsdb2clog: XiangShan ChiselDB to CLog Converter" << std::endl;
    std::cerr << std::endl;
}

static void print_help() noexcept
{
    std::cout << "Usage: xsdb2clog [OPTION...]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -h, --help              print help info" << std::endl;
    std::cout << "  -v, --version           print version info" << std::endl;
    std::cout << "  -i, --xsdb=FILE         *necessary* specify XiangShan ChiselDB file" << std::endl;
    std::cout << "  -o, --output=FILE       specify CLog output file. by default, the clog output" << std::endl;
    std::cout << "                          is written to the standard console output" << std::endl;
    std::cout << "  -T, --clogT             *default* specify output format as CLog.T" << std::endl;
    std::cout << "  -B, --clogB             [WIP] specify output format as CLog.B" << std::endl;
    std::cout << "  -Z, --clogBz            [WIP] specify output format as CLog.Bz" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[])
{
    //
    std::string xsdbFile;
    std::string clogFile;

    std::string xsdbTableName   = "CHILog";

    // input parameters
    const struct option long_options[] = {
        { "help"            , 0, NULL, 'h' },
        { "version"         , 0, NULL, 'v' },
        { "xsdb"            , 1, NULL, 'i' },
        { "output"          , 1, NULL, 'o' },
        { "clogT"           , 0, NULL, 'T' },
        { "clogB"           , 0, NULL, 'B' },
        { "clogBz"          , 0, NULL, 'Z' },
        { 0                 , 0, NULL,  0  }
    };

    int long_index = 0;
    int o;
    while ((o = getopt_long(argc, argv, "hvi:o:TBZ", long_options, &long_index)) != -1)
    {
        switch (o)
        {
            case 0:
                switch (long_index)
                {
                    default:
                        print_help();
                        return 1;
                }

            case 'h':
                print_help();
                return 0;

            case 'v':
                print_version();
                return 0;

            case 'i':
                xsdbFile = optarg;
                break;

            case 'o':
                clogFile = optarg;
                break;

            case 'T':
                break;

            case 'B':
                std::cerr << "$ERROR: format CLog.B not supported currently, hold tight on future versions!" << std::endl;
                return 1;

            case 'Z':
                std::cerr << "$ERROR: format CLog.Bz not supported currently, hold tight on future versions!" << std::endl;
                return 1;

            default:
                print_help();
                return 1;
        }
    }

    //
    print_version();

    //
    if (xsdbFile.empty())
    {
        std::cerr << "%ERROR: please specify input XiangShan ChiselDB file with option '-D' or '--xsdb'" << std::endl;
        return 2;
    }

    /* Specify output direction */
    std::ofstream foutput;
    if (clogFile.empty())
    {
        output = &std::cout;
    }
    else
    {
        foutput.open(clogFile.c_str());

        if (!foutput)
        {
            std::cerr << "%ERROR: cannot open output file: " << clogFile << std::endl;
            return 3;
        }

        output = &foutput;
    }

    /* */
    sqlite3*        db;
    sqlite3_stmt*   dbstmt;

    const char*     zTail;
    char*           zErrMsg;
    int             rc;

    /* Open database */
    rc = sqlite3_open(xsdbFile.c_str(), &db);

    if (rc)
    {
        std::cerr << "%ERROR: cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    else
        std::cerr << "%INFO: database opened" << std::endl;

    /* Create SQL statement */
    std::ostringstream sql;

    sql << "SELECT ";
    sql << XSDB::COLUMN_SITE    << ", ";
    sql << XSDB::COLUMN_CHANNEL << ", ";
    sql << XSDB::COLUMN_STAMP   << ", ";
    sql << XSDB::COLUMN_FLIT0   << ", ";
    sql << XSDB::COLUMN_FLIT1   << ", ";
    sql << XSDB::COLUMN_FLIT2   << ", ";
    sql << XSDB::COLUMN_FLIT3   << ", ";
    sql << XSDB::COLUMN_FLIT4   << ", ";
    sql << XSDB::COLUMN_FLIT5   <<  " ";
    sql << "FROM " << xsdbTableName;

    /* Execute SQL statement */
    std::cerr << "%INFO: converting xsdb to clog ... " << std::endl;

    rc = sqlite3_prepare_v2(db, sql.str().c_str(), sql.str().length(), &dbstmt, &zTail);

    if (rc != SQLITE_OK)
    {
        std::cerr << "%ERROR: database error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);

        return -1;
    }

    try
    {
        while (sqlite3_step(dbstmt) == SQLITE_ROW)
        {
            int dbcol = 0;

            CallbackSite(std::string(
                (const char*) sqlite3_column_text(dbstmt, dbcol++)
            ));

            CallbackChannel(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            CallbackStamp(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            CallbackFlit0(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            CallbackFlit1(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            CallbackFlit2(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            CallbackFlit3(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            CallbackFlit4(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            CallbackFlit5(
                sqlite3_column_int64(dbstmt, dbcol++)
            );

            record_count++;

            CLog::CLogT::WriteCHISentenceLog(
                *output,
                current_stamp,
                4,
                GetChannel(current_channel),
                (uint32_t*) current_flit,
                14);

            if ((record_count & 0x3FF) == 0)
                std::cerr << ".";

            if ((record_count & 0x7FFF) == 0)
                std::cerr << std::endl;
        }
    }
    catch (std::exception& e)
    {
        sqlite3_finalize(dbstmt);
        sqlite3_close(db);

        return -1;
    }

    std::cerr << std::endl;
    std::cerr << "%INFO: operation done for " << record_count << " records" << std::endl;

    sqlite3_finalize(dbstmt);
    sqlite3_close(db);

    return 0;
}
