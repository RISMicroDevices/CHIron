#pragma once

#include <ostream>
#include <variant>
#ifndef CLOG2LOG__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#endif

#include <getopt.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <atomic>
#include <stdexcept>
#include <set>
#include <chrono>

#include "../../../clog/clog_t.hpp"
#include "../../../clog/clog_t_util.hpp"

#include "../../../common/concurrentqueue.hpp"


#ifndef CHI_NODEID_WIDTH
#   define  CHI_NODEID_WIDTH            7
#endif

#ifndef CHI_REQ_ADDR_WIDTH
#   define  CHI_REQ_ADDR_WIDTH          48
#endif

#ifndef CHI_REQ_RSVDC_WIDTH
#   define  CHI_REQ_RSVDC_WIDTH         0
#endif

#ifndef CHI_DAT_RSVDC_WIDTH
#   define  CHI_DAT_RSVDC_WIDTH         0
#endif

#ifndef CHI_DATA_WIDTH
#   define  CHI_DATA_WIDTH              256
#endif

#ifndef CHI_DATACHECK_PRESENT
#   define  CHI_DATACHECK_PRESENT       false
#endif

#ifndef CHI_POISON_PRESENT
#   define  CHI_POISON_PRESENT          false
#endif

#ifndef CHI_MPAM_PRESENT
#   define  CHI_MPAM_PRESENT            false
#endif


#ifdef CHI_ISSUE_B_ENABLE
    #include "../../../chi_b/spec/chi_b_protocol.hpp"           // IWYU pragma: keep
    #include "../../../chi_b/util/chi_b_util_flit.hpp"          // IWYU pragma: keep
    #include "../../../chi_b/util/chi_b_util_decoding.hpp"      // IWYU pragma: keep
    #define CHI_ISSUE_B_ENABLE
    using namespace CHI::B::Flits;
    using config = CHI::B::FlitConfiguration<
        CHI_NODEID_WIDTH,
        CHI_REQ_ADDR_WIDTH,
        CHI_REQ_RSVDC_WIDTH,
        CHI_DAT_RSVDC_WIDTH,
        CHI_DATA_WIDTH,
        CHI_DATACHECK_PRESENT,
        CHI_POISON_PRESENT
    >;
    using REQ_t = CHI::B::Flits::REQ<config>;
    using RSP_t = CHI::B::Flits::RSP<config>;
    using DAT_t = CHI::B::Flits::DAT<config>;
    using SNP_t = CHI::B::Flits::SNP<config>;
    using DecoderREQ_t = CHI::B::Opcodes::REQ::Decoder<REQ_t>;
    using DecoderRSP_t = CHI::B::Opcodes::RSP::Decoder<RSP_t>;
    using DecoderDAT_t = CHI::B::Opcodes::DAT::Decoder<DAT_t>;
    using DecoderSNP_t = CHI::B::Opcodes::SNP::Decoder<SNP_t>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    #include "../../../chi_eb/spec/chi_eb_protocol.hpp"         // IWYU pragma: keep
    #include "../../../chi_eb/util/chi_eb_util_flit.hpp"        // IWYU pragma: keep
    #include "../../../chi_eb/util/chi_eb_util_decoding.hpp"    // IWYU pragma: keep
    #define CHI_ISSUE_EB_ENABLE
    using namespace CHI::Eb::Flits;
    using config = CHI::Eb::FlitConfiguration<
        CHI_NODEID_WIDTH,
        CHI_REQ_ADDR_WIDTH,
        CHI_REQ_RSVDC_WIDTH,
        CHI_DAT_RSVDC_WIDTH,
        CHI_DATA_WIDTH,
        CHI_DATACHECK_PRESENT,
        CHI_POISON_PRESENT,
        CHI_MPAM_PRESENT
    >;
    using REQ_t = CHI::Eb::Flits::REQ<config>;
    using RSP_t = CHI::Eb::Flits::RSP<config>;
    using DAT_t = CHI::Eb::Flits::DAT<config>;
    using SNP_t = CHI::Eb::Flits::SNP<config>;
    using DecoderREQ_t = CHI::Eb::Opcodes::REQ::Decoder<REQ_t>;
    using DecoderRSP_t = CHI::Eb::Opcodes::RSP::Decoder<RSP_t>;
    using DecoderDAT_t = CHI::Eb::Opcodes::DAT::Decoder<DAT_t>;
    using DecoderSNP_t = CHI::Eb::Opcodes::SNP::Decoder<SNP_t>;
#endif


static void print_version() noexcept
{
    std::cerr << std::endl;
    std::cerr << "                CHIron Toolset" << std::endl;
    std::cerr << std::endl;
    std::cerr << "           clog2log - " << __DATE__ << std::endl;
    std::cerr << std::endl;
    std::cerr << "  clog2log: CLog to text log Converter" << std::endl;
    std::cerr << std::endl;
}

static void print_help() noexcept
{
    std::cout << "Usage: clog2log [OPTION...]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -h, --help              print help info" << std::endl;
    std::cout << "  -v, --version           print version info" << std::endl;
    std::cout << "  -i, --input=FILE        specify CLog input file" << std::endl;
    std::cout << "  -T, --clogT             *default* specify input format as CLog.T" << std::endl;
    std::cout << "  -B, --clogB             specify input format as CLog.B" << std::endl;
    std::cout << "  -Z, --clogBz            specify input format as CLog.Bz" << std::endl;
    std::cout << "  -o, --output            specify output file" << std::endl;
    std::cout << "  -j, --json              output as json format" << std::endl;
    std::cout << std::endl;
}

template<typename Ta, typename Tb>
static void print_parameter_mismatch(const char* name, Ta value, Tb expected) noexcept
{
    std::cerr << "%ERROR: mismatched parameter: " << name << ", found: " << value << ", expected: " << expected << std::endl;
}

template<typename Tb>
static void print_parameter_mismatch(const char* name, Tb expected) noexcept
{
    std::cerr << "%ERROR: mismatched parameter: " << name << ", expected: " << expected << std::endl;
}

template<typename Ta, typename Tb>
static bool check_parameter(const char* name, Ta value, Tb expected)
{
    bool result = value == expected;
    if (!result)
        print_parameter_mismatch(name, value, expected);
    return result;
}

template<typename Tv>
inline bool is_visible(const std::string& value, const std::set<Tv>& whitelist, const std::set<Tv>& blacklist)
{
    if (!whitelist.empty())
        return whitelist.contains(value);

    return !blacklist.contains(value);
}

template<typename Tv>
inline void print_field_dec(std::ostream&                   os, 
                            bool                            asJson, 
                            const std::set<std::string>&    whitelist,
                            const std::set<std::string>&    blacklist,
                            const char*                     name,
                            Tv                              value)
{
    if (is_visible(name, whitelist, blacklist)) \
    { \
        std::ostringstream oss; \
        oss << ", "; \
        if (asJson) \
            oss << "\"" << name << "\":\""; \
        else \
            oss << name << "="; \
        oss << static_cast<uint64_t>(value); \
        if (asJson) \
            oss << "\""; \
        os << oss.str(); \
    }
}

template<>
inline void print_field_dec<std::monostate>(
    std::ostream&, bool, const std::set<std::string>&, const std::set<std::string>&, const char*, std::monostate)
{ }

template<typename Tv>
inline void print_field_hex(std::ostream&                   os, 
                            bool                            asJson, 
                            const std::set<std::string>&    whitelist,
                            const std::set<std::string>&    blacklist,
                            const char*                     name,
                            Tv                              value)
{
    if (is_visible(name, whitelist, blacklist)) \
    { \
        std::ostringstream oss; \
        oss << ", "; \
        if (asJson) \
            oss << "\"" << name << "\":\""; \
        else \
            oss << name << "="; \
        oss << "0x" << std::hex << static_cast<uint64_t>(value); \
        if (asJson) \
            oss << "\""; \
        os << oss.str(); \
    }
}

template<>
inline void print_field_hex<std::monostate>(
    std::ostream&, bool, const std::set<std::string>&, const std::set<std::string>&, const char*, std::monostate)
{ }

template<typename Tv>
inline void print_field_hex_data(std::ostream&                   os, 
                            bool                            asJson, 
                            const std::set<std::string>&    whitelist,
                            const std::set<std::string>&    blacklist,
                            const char*                     name,
                            Tv                              value,
                            size_t                          width)
{
    if (is_visible(name, whitelist, blacklist)) \
    { \
        std::ostringstream oss; \
        oss << ", "; \
        if (asJson) \
            oss << "\"" << name << "\":\""; \
        else \
            oss << name << "="; \
        oss << "0x" << std::hex; \
        for (int i = 0; i < width; i++) \
            oss << (i ? "_" : "") << std::setw(8) << std::setfill('0') << reinterpret_cast<uint32_t*>(value)[width - i - 1]; \
        if (asJson) \
            oss << "\""; \
        os << oss.str(); \
    }
}

template<>
inline void print_field_hex_data<std::monostate>(
    std::ostream&, bool, const std::set<std::string>&, const std::set<std::string>&, const char*, std::monostate, size_t)
{ }

inline int clog2log(int argc, char* argv[])
{
    //
    std::string clogFile;
    std::string outputFile = "output.log";

    std::set<std::string> whitelist;
    std::set<std::string> blacklist;

    bool asJson = false;

    // input parameters
    const struct option long_options[] = {
        { "help"            , 0, NULL, 'h' },
        { "version"         , 0, NULL, 'v' },
        { "input"           , 1, NULL, 'i' },
        { "clogT"           , 0, NULL, 'T' },
        { "clogB"           , 0, NULL, 'B' },
        { "clogBz"          , 0, NULL, 'Z' },
        { "output"          , 1, NULL, 'o' },
        { "json"            , 0, NULL, 'j' }
    };

    int long_index = 0;
    int o;
    while ((o = getopt_long(argc, argv, "hvi:TBZo:j", long_options, &long_index)) != -1)
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

            case 'o':
                outputFile = optarg;
                break;

            case 'j':
                asJson = true;
                break;

            default:
                print_help();
                return 1;
        }
    }

    //
    print_version();

    //
    if (clogFile.empty())
    {
        std::cerr << "%ERROR: please specify input CLog file with option '-i' or '--input'" << std::endl;
        return 2;
    }

    if (outputFile.empty())
    {
        std::cerr << "%ERROR: please specify output file with option '-o' or '--output'" << std::endl;
        return 2;
    }

    /* Concurrent Parsing */
    struct ParseAndWriteTask {
        uint64_t        time;
        uint64_t        nodeId;
        CLog::Channel   channel;
        union {
            REQ_t   req;
            RSP_t   rsp;
            DAT_t   dat;
            SNP_t   snp;
        }               flit;
    };

    moodycamel::ConcurrentQueue<ParseAndWriteTask>* queue
        = new moodycamel::ConcurrentQueue<ParseAndWriteTask>(32768);

    /* Prepare parsing context */
    size_t flitLengthREQ    = (REQ_t::WIDTH + 7) / 8;
    size_t flitLengthRSP    = (RSP_t::WIDTH + 7) / 8;
    size_t flitLengthDAT    = (DAT_t::WIDTH + 7) / 8;
    size_t flitLengthSNP    = (SNP_t::WIDTH + 7) / 8;

    size_t flitLengthMax    = std::max({flitLengthREQ, flitLengthRSP, flitLengthDAT, flitLengthSNP});

    DecoderREQ_t*   decoderReq = new DecoderREQ_t;
    DecoderRSP_t*   decoderRsp = new DecoderRSP_t;
    DecoderDAT_t*   decoderDat = new DecoderDAT_t;
    DecoderSNP_t*   decoderSnp = new DecoderSNP_t;

    /* Parse CLog file */
    bool                            finished = false;
    bool                            errored  = false;

    bool                            paramEnd = false;

    CLog::CLogT::Parser<>           parser;

    CLog::Parameters                params;
    CLog::CLogT::ParametersSerDes<> paramsSerDes;

    paramsSerDes.SetParametersReference(&params);
    paramsSerDes.RegisterAsDeserializer(parser);

    std::atomic_thread_fence(std::memory_order_seq_cst);

    parser.RegisterExecutor(CLog::CLogT::Sentence::CHI_LOG::Token, 
        [&](auto& parser, auto& token, auto& is) -> bool {
            
            //
            if (!paramEnd)
            {
                paramsSerDes.Unregister(parser);

                // check parameters
                #ifdef CHI_ISSUE_B_ENABLE
                    if ((finished |= (params.GetIssue() != CLog::Issue::B)))
                        print_parameter_mismatch("CHI.Issue", "B");
                #endif
                #ifdef CHI_ISSUE_EB_ENABLE
                    if ((finished |= (params.GetIssue() != CLog::Issue::Eb)))
                        print_parameter_mismatch("CHI.Issue", "E.b");
                #endif

                finished |= !check_parameter("CHI.NodeIdWidth", params.GetNodeIdWidth(), CHI_NODEID_WIDTH);
                finished |= !check_parameter("CHI.ReqAddrWidth", params.GetReqAddrWidth(), CHI_REQ_ADDR_WIDTH);
                finished |= !check_parameter("CHI.ReqRSVDCWidth", params.GetReqRSVDCWidth(), CHI_REQ_RSVDC_WIDTH);
                finished |= !check_parameter("CHI.DatRSVDCWidth", params.GetDatRSVDCWidth(), CHI_DAT_RSVDC_WIDTH);
                finished |= !check_parameter("CHI.DataWidth", params.GetDataWidth(), CHI_DATA_WIDTH);
                finished |= !check_parameter("CHI.HasDataCheck", params.IsDataCheckPresent(), CHI_DATACHECK_PRESENT);
                finished |= !check_parameter("CHI.HasPoison", params.IsPoisonPresent(), CHI_POISON_PRESENT);

                #ifdef CHI_ISSUE_EB_ENABLE
                    finished |= !check_parameter("CHI.HasMPAM", params.IsMPAMPresent(), CHI_MPAM_PRESENT);
                #endif
            }

            //
            ParseAndWriteTask   task;
            uint32_t            flit[flitLengthMax];

            size_t              flitLengthRead = 0;
            bool                flitOverflow;

            if (!CLog::CLogT::Sentence::CHI_LOG::Term::Read(
                is,
                task.time,
                task.nodeId,
                task.channel,
                flit,
                flitLengthMax,
                &flitLengthRead,
                &flitOverflow))
            {
                std::cerr << "%ERROR: chi.log term reading error" << std::endl;
                return false;
            }

            //
            switch (task.channel)
            {
                //
                case CLog::Channel::RXREQ:
                case CLog::Channel::TXREQ:

                    if (flitLengthRead > ((flitLengthREQ + 3) >> 2))
                        std::cerr << "%WARNING: overflowed REQ flit length."
                                  << " read: " << flitLengthRead << ", expected: " << flitLengthREQ << "." << std::endl;

                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeREQ<config>(task.flit.req, flit, REQ_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeREQ<config>(task.flit.req, flit, REQ_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize REQ flit" << std::endl;
                        return false;
                    }
                    
                    break;

                //
                case CLog::Channel::RXRSP:
                case CLog::Channel::TXRSP:

                    if (flitLengthRead > ((flitLengthRSP + 3) >> 2))
                        std::cerr << "%WARNING: overflowed RSP flit length."
                                  << " read: " << flitLengthRead << ", expected: " << flitLengthRSP << "." << std::endl;

                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeRSP<config>(task.flit.rsp, flit, RSP_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeRSP<config>(task.flit.rsp, flit, RSP_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize RSP flit" << std::endl;
                        return false;
                    }

                    break;

                //
                case CLog::Channel::RXDAT:
                case CLog::Channel::TXDAT:

                    if (flitLengthRead > ((flitLengthDAT + 3) >> 2))
                        std::cerr << "%WARNING: overflowed DAT flit length."
                                  << " read: " << flitLengthRead << ", expected: " << flitLengthDAT << "." << std::endl;

                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeDAT<config>(task.flit.dat, flit, DAT_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeDAT<config>(task.flit.dat, flit, DAT_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize DAT flit" << std::endl;
                        return false;
                    }

                    break;

                //
                case CLog::Channel::RXSNP:
                case CLog::Channel::TXSNP:

                    if (flitLengthRead > ((flitLengthSNP + 3) >> 2))
                        std::cerr << "%WARNING: overflowed SNP flit length."
                                  << " read: " << flitLengthRead << ", expected: " << flitLengthSNP << "." << std::endl;

                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeSNP<config>(task.flit.snp, flit, SNP_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeSNP<config>(task.flit.snp, flit, SNP_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize SNP flit" << std::endl;
                        return false;
                    }

                    break;

                //
                default:
                    std::cerr << "%ERROR: unknown CHI channel: " << uint32_t(task.channel) << std::endl;
                    finished = true;
                    return false;
            }

            while (!queue->try_enqueue(task));

            return true;
    });

    // open output file
    std::ofstream ofs(outputFile);

    if (!ofs)
    {
        std::cerr << "%ERROR: cannot open output file: " << outputFile << std::endl;
        return 3;
    }

    #define PRINT_FIELD_DEC(channel, field) \
        print_field_dec(ofs, asJson, whitelist, blacklist, #field, task.flit.channel.field())

    #define PRINT_FIELD_HEX(channel, field) \
        print_field_hex(ofs, asJson, whitelist, blacklist, #field, task.flit.channel.field())

    #define PRINT_FIELD_HEX_DATA(channel, field, width) \
        print_field_hex_data(ofs, asJson, whitelist, blacklist, #field, task.flit.channel.field(), width)

    std::thread outputWorker([&]() -> void {

        ParseAndWriteTask task;

        if (asJson)
        {
            ofs << "{\n";
            ofs << "\"logs\":[\n";
        }

        bool first = true;

        while (1)
        {
            if (queue->try_dequeue(task))
            {
                if (asJson)
                {
                    if (first)
                        first = false;
                    else
                        ofs << ",\n";

                    ofs << "{";
                }
                else if (first)
                    first = false;
                else
                    ofs << "\n";

                if (asJson)
                {
                    ofs << "\"time\":\"" << task.time << "\"" << ", ";
                    ofs << "\"node\":\"" << task.nodeId << "\"" << ", ";
                    ofs << "\"at\":\"";
                }
                else
                    ofs << "[" << task.time << "] (" << task.nodeId << ") <";

                switch (task.channel)
                {
                    //
                    case CLog::Channel::RXREQ:
                    case CLog::Channel::TXREQ:

                        if (task.channel == CLog::Channel::RXREQ)
                            ofs << "RXREQ";
                        else
                            ofs << "TXREQ";

                        if (asJson)
                            ofs << "\", ";
                        else
                            ofs << "> ";

                        {
                            auto info = decoderReq->Decode(task.flit.req.Opcode());

                            if (!info.IsValid())
                            {
                                std::cerr << "%ERROR: unknown opcode on REQ: " 
                                    << uint32_t(task.flit.req.Opcode()) << std::endl;
                                errored = true;
                                return;
                            }

                            if (asJson)
                                ofs << "\"Opcode\":\"";

                            ofs << info.GetName();

                            if (asJson)
                                ofs << "\"";
                        }

                        PRINT_FIELD_DEC(req, QoS);
                        PRINT_FIELD_DEC(req, TgtID);
                        PRINT_FIELD_DEC(req, SrcID);
                        PRINT_FIELD_DEC(req, TxnID);
                        PRINT_FIELD_DEC(req, ReturnNID);
                        PRINT_FIELD_DEC(req, StashNID);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(req, SLCRepHint);
#endif
                        PRINT_FIELD_DEC(req, StashNIDValid);
                        PRINT_FIELD_DEC(req, Endian);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(req, Deep);
#endif
                        PRINT_FIELD_DEC(req, ReturnTxnID);
                        PRINT_FIELD_DEC(req, StashLPIDValid);
                        PRINT_FIELD_DEC(req, StashLPID);
                        PRINT_FIELD_DEC(req, Size);
                        PRINT_FIELD_HEX(req, Addr);
                        PRINT_FIELD_DEC(req, NS);
                        PRINT_FIELD_DEC(req, LikelyShared);
                        PRINT_FIELD_DEC(req, AllowRetry);
                        PRINT_FIELD_DEC(req, Order);
                        PRINT_FIELD_DEC(req, PCrdType);
                        PRINT_FIELD_DEC(req, MemAttr);
                        PRINT_FIELD_DEC(req, SnpAttr);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(req, DoDWT);
#endif
                        PRINT_FIELD_DEC(req, LPID);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(req, PGroupID);
                        PRINT_FIELD_DEC(req, StashGroupID);
                        PRINT_FIELD_DEC(req, TagGroupID);
#endif
                        PRINT_FIELD_DEC(req, Excl);
                        PRINT_FIELD_DEC(req, SnoopMe);
                        PRINT_FIELD_DEC(req, ExpCompAck);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(req, TagOp);
#endif
                        PRINT_FIELD_DEC(req, TraceTag);
#ifdef CHI_ISSUE_EB_ENABLE
                        if constexpr (REQ_t::hasMPAM)
                            PRINT_FIELD_DEC(req, MPAM);
#endif
                        if constexpr (REQ_t::hasRSVDC)
                            PRINT_FIELD_DEC(req, RSVDC);

                        break;

                    //
                    case CLog::Channel::RXRSP:
                    case CLog::Channel::TXRSP:

                        if (task.channel == CLog::Channel::RXRSP)
                            ofs << "RXRSP";
                        else
                            ofs << "TXRSP";

                        if (asJson)
                            ofs << "\", ";
                        else
                            ofs << "> ";

                        {
                            auto info = decoderRsp->Decode(task.flit.rsp.Opcode());

                            if (!info.IsValid())
                            {
                                std::cerr << "%ERROR: unknown opcode on RSP: " 
                                    << uint32_t(task.flit.req.Opcode()) << std::endl;
                                errored = true;
                                return;
                            }

                            if (asJson)
                                ofs << "\"Opcode\":\"";

                            ofs << info.GetName();

                            if (asJson)
                                ofs << "\"";
                        }

                        PRINT_FIELD_DEC(rsp, QoS);
                        PRINT_FIELD_DEC(rsp, TgtID);
                        PRINT_FIELD_DEC(rsp, SrcID);
                        PRINT_FIELD_DEC(rsp, TxnID);
                        PRINT_FIELD_DEC(rsp, RespErr);
                        PRINT_FIELD_DEC(rsp, Resp);
                        PRINT_FIELD_DEC(rsp, FwdState);
                        PRINT_FIELD_DEC(rsp, DataPull);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(rsp, CBusy);
#endif
                        PRINT_FIELD_DEC(rsp, DBID);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(rsp, PGroupID);
                        PRINT_FIELD_DEC(rsp, StashGroupID);
                        PRINT_FIELD_DEC(rsp, TagGroupID);
#endif
                        PRINT_FIELD_DEC(rsp, PCrdType);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(rsp, TagOp);
#endif
                        PRINT_FIELD_DEC(rsp, TraceTag);

                        break;

                    //
                    case CLog::Channel::RXDAT:
                    case CLog::Channel::TXDAT:

                        if (task.channel == CLog::Channel::RXDAT)
                            ofs << "RXDAT";
                        else
                            ofs << "TXDAT";

                        if (asJson)
                            ofs << "\", ";
                        else
                            ofs << "> ";

                        {
                            auto info = decoderDat->Decode(task.flit.dat.Opcode());

                            if (!info.IsValid())
                            {
                                std::cerr << "%ERROR: unknown opcode on DAT: " 
                                    << uint32_t(task.flit.req.Opcode()) << std::endl;
                                errored = true;
                                return;
                            }

                            if (asJson)
                                ofs << "\"Opcode\":\"";

                            ofs << info.GetName();

                            if (asJson)
                                ofs << "\"";
                        }

                        PRINT_FIELD_DEC(dat, QoS);
                        PRINT_FIELD_DEC(dat, TgtID);
                        PRINT_FIELD_DEC(dat, SrcID);
                        PRINT_FIELD_DEC(dat, TxnID);
                        PRINT_FIELD_DEC(dat, HomeNID);
                        PRINT_FIELD_DEC(dat, RespErr);
                        PRINT_FIELD_DEC(dat, Resp);
                        PRINT_FIELD_DEC(dat, FwdState);
                        PRINT_FIELD_DEC(dat, DataPull);
                        PRINT_FIELD_DEC(dat, DataSource);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(dat, CBusy);
#endif
                        PRINT_FIELD_DEC(dat, DBID);
                        PRINT_FIELD_DEC(dat, CCID);
                        PRINT_FIELD_DEC(dat, DataID);
#ifdef CHI_ISSUE_EB_ENABLE
                        PRINT_FIELD_DEC(dat, TagOp);
                        PRINT_FIELD_DEC(dat, Tag);
                        PRINT_FIELD_DEC(dat, TU);
#endif
                        PRINT_FIELD_DEC(dat, TraceTag);
                        if (DAT_t::hasRSVDC)
                            PRINT_FIELD_DEC(dat, RSVDC);
                        PRINT_FIELD_HEX(dat, BE);
                        PRINT_FIELD_HEX_DATA(dat, Data, DAT_t::DATA_WIDTH / 32);
                        if (DAT_t::hasDataCheck)
                            PRINT_FIELD_HEX(dat, DataCheck);
                        if (DAT_t::hasPoison)
                            PRINT_FIELD_HEX(dat, Poison);

                        break;

                    //
                    case CLog::Channel::RXSNP:
                    case CLog::Channel::TXSNP:

                        if (task.channel == CLog::Channel::RXSNP)
                            ofs << "RXSNP";
                        else
                            ofs << "TXSNP";

                        if (asJson)
                            ofs << "\", ";
                        else
                            ofs << "> ";

                        {
                            auto info = decoderSnp->Decode(task.flit.snp.Opcode());

                            if (!info.IsValid())
                            {
                                std::cerr << "%ERROR: unknown opcode on SNP: " 
                                    << uint32_t(task.flit.req.Opcode()) << std::endl;
                                errored = true;
                                return;
                            }

                            if (asJson)
                                ofs << "\"Opcode\":\"";

                            ofs << info.GetName();

                            if (asJson)
                                ofs << "\"";
                        }

                        PRINT_FIELD_DEC(snp, QoS);
                        PRINT_FIELD_DEC(snp, SrcID);
                        PRINT_FIELD_DEC(snp, TxnID);
                        PRINT_FIELD_DEC(snp, FwdNID);
                        PRINT_FIELD_DEC(snp, FwdTxnID);
                        PRINT_FIELD_DEC(snp, StashLPIDValid);
                        PRINT_FIELD_DEC(snp, StashLPID);
                        PRINT_FIELD_DEC(snp, VMIDExt);
                        PRINT_FIELD_HEX(snp, Addr);
                        PRINT_FIELD_DEC(snp, NS);
                        PRINT_FIELD_DEC(snp, DoNotGoToSD);
#ifdef CHI_ISSUE_B_ENABLE
                        PRINT_FIELD_DEC(snp, DoNotDataPull);
#endif
                        PRINT_FIELD_DEC(snp, RetToSrc);
                        PRINT_FIELD_DEC(snp, TraceTag);
#ifdef CHI_ISSUE_EB_ENABLE
                        if constexpr (SNP_t::hasMPAM)
                            PRINT_FIELD_DEC(snp, MPAM);
#endif

                        break;

                    //
                    default:
                        std::cerr << "%ERROR: unknown CHI channel: " << uint32_t(task.channel) << std::endl;
                        throw new std::logic_error("ShouldNotReachHere");
                }

                if (asJson)
                {
                    ofs << "}";
                }
            }
            else if (finished)
            {
                if (asJson)
                {
                    ofs << "\n]\n";
                    ofs << "}\n";
                }

                return;
            }
        }
    });

    // open input file
    std::ifstream ifs(clogFile);

    if (!ifs)
    {
        std::cerr << "%ERROR: cannot open input file: " << clogFile << std::endl;
        return 3;
    }

    // parsing
    auto beforeTime = std::chrono::system_clock::now();

    int dotCount = 0;

    while (1)
    {
        if (errored)
        {
            std::cerr << "%ERROR: CLog.T CHI decoding error on file: " << clogFile 
                << ", after " << parser.GetExecutionCounter() << " tokens."  << std::endl;
            break;
        }

        if (!parser.Parse(ifs, true))
        {
            std::cerr << "%ERROR: CLog.T parsing error on file: " << clogFile 
                << ", after " << parser.GetExecutionCounter() << " tokens."  << std::endl;
            errored = true;
            break;
        }

        if (!ifs)
            break;

        if (!(parser.GetExecutionCounter() & 0xFFFF))
        {
            dotCount++;
            std::cout << "." << std::flush;
        }

        if (dotCount == 31)
        {
            dotCount = 0;
            std::cout << std::endl;
        }
    }

    finished = true;

    outputWorker.join();

    auto afterTime = std::chrono::system_clock::now();

    std::cout << std::endl;
    std::cout << "%INFO: operation done for " << parser.GetExecutionCounter() << " records" 
        << " in " << std::chrono::duration<double, std::milli>(afterTime - beforeTime).count() << " ms" << std::endl;

    ofs.close();
    ifs.close();

    delete queue;

    delete decoderReq;
    delete decoderRsp;
    delete decoderDat;
    delete decoderSnp;

    if (errored)
        return -1;

    return 0;
}