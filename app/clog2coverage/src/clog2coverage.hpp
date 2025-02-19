#pragma once

#ifndef CLOG2COVERAGE__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#endif

#include <getopt.h>

#include <iostream>
#include <fstream>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "../../../common/utility.hpp"
#include "../../../clog/clog_b/clog_b.hpp"


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


// #define DEBUG_NESTING_COLLECTION


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
    #include "../../../chi_eb/xact/chi_eb_joint.hpp"            // IWYU pragma: keep
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
    using XactDenialEnum_t = CHI::Eb::Xact::XactDenialEnum;
    using Global_t = CHI::Eb::Xact::Global<config>;
    using Topology_t = CHI::Eb::Xact::Topology;
    using RNFJoint_t = CHI::Eb::Xact::RNFJoint<config>;
    namespace NodeType = CHI::Eb::Xact::NodeType;
    namespace XactDenial = CHI::Eb::Xact::XactDenial;
#endif


static void print_version() noexcept
{
    std::cerr << std::endl;
    std::cerr << "                CHIron Toolset" << std::endl;
    std::cerr << std::endl;
    std::cerr << "           clog2coverage - " << __DATE__ << std::endl;
    std::cerr << std::endl;
    std::cerr << "  clog2coverage: CLog to Transaction Coverage" << std::endl;
    std::cerr << std::endl;
}

static void print_help() noexcept
{
    std::cout << "Usage: clog2coverage [OPTION...]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -h, --help              print help info" << std::endl;
    std::cout << "  -v, --version           print version info" << std::endl;
    std::cout << "  -i, --input=FILE        specify CLog input file" << std::endl;
    std::cout << "  -T, --clogT             specify input format as CLog.T" << std::endl;
    std::cout << "  -B, --clogB             specify input format as CLog.B" << std::endl;
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


struct vector_uint16_hash {

    size_t operator()(const std::vector<uint16_t>& key) const noexcept
    {
        uint64_t hash = 0;

        for (int i = 0; i < key.size(); i++)
        {
            size_t t = key[i];

            if ((i + 1) < key.size())
                t |= uint64_t(key[i + 1]) << 16;

            if ((i + 2) < key.size())
                t |= uint64_t(key[i + 2]) << 32;

            if ((i + 3) < key.size())
                t |= uint64_t(key[i + 3]) << 48;

            hash ^= t;
        }

        return hash;
    }
};

struct vector_uint16_equal_to {

    bool operator()(const std::vector<uint16_t>& lhs, const std::vector<uint16_t>& rhs) const noexcept
    {
        if (lhs.size() != rhs.size())
            return false;

        for (int i = 0; i < lhs.size(); i++)
            if (lhs[i] != rhs[i])
                return false;

        return true;
    }
};


struct flow_entry {
    bool        isRSP;
    bool        isRX;
    uint16_t    opcode;
};

struct vector_flow_entry_hash {

    size_t operator()(const std::vector<flow_entry>& key) const noexcept
    {
        uint64_t hash = 0;

        for (int i = 0; i < key.size(); i++)
        {
            size_t t = key[i].opcode;

            if ((i + 1) < key.size())
                t |= uint64_t(key[i + 1].opcode) << 16;

            if ((i + 2) < key.size())
                t |= uint64_t(key[i + 2].opcode) << 32;

            if ((i + 3) < key.size())
                t |= uint64_t(key[i + 3].opcode) << 48;

            hash ^= t;
        }

        return hash;
    }
};

struct vector_flow_entry_equal_to {

    bool operator()(const std::vector<flow_entry>& lhs, const std::vector<flow_entry>& rhs) const noexcept
    {
        if (lhs.size() != rhs.size())
            return false;

        for (int i = 0; i < lhs.size(); i++)
        {
            if (lhs[i].opcode != rhs[i].opcode)
                return false;

            if (lhs[i].isRSP != rhs[i].isRSP)
                return false;

            if (lhs[i].isRX != rhs[i].isRX)
                return false;
        }

        return true;
    }
};


inline int clog2coverage(int argc, char* argv[])
{
    size_t      counter = 0;

    //
    std::string clogFile;
    std::string outputFile = "output.log";

    std::vector<std::string> inputFiles;

    bool asJson = false;
    
    bool clogT = false;
    bool clogB = false;
    
    bool verboseFlit = false;
    bool verboseXact = false;
    bool verboseNest = false;

    // input parameters
    const struct option long_options[] = {
        { "help"            , 0, NULL, 'h' },
        { "version"         , 0, NULL, 'v' },
        { "input"           , 1, NULL, 'i' },
        { "clogT"           , 0, NULL, 'T' },
        { "clogB"           , 0, NULL, 'B' },
        { "output"          , 1, NULL, 'o' },
        { "json"            , 0, NULL, 'j' }
    };

    int long_index = 0;
    int o;
        while ((o = getopt_long(argc, argv, "hvi:TBo:j", long_options, &long_index)) != -1)
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
                inputFiles.push_back(clogFile);
                break;

            case 'T':
                if (clogB)
                {
                    std::cerr << "%ERROR: multiple source format specified, '-T' confliction" << std::endl;
                    return 1;
                }
                clogT = true;
                break;

            case 'B':
                if (clogT)
                {
                    std::cerr << "%ERROR: multiple source format specified, '-B' confliction" << std::endl;
                    return 1;
                }
                clogB = true;
                break;

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
    if (clogT)
    {
        std::cerr << "%ERROR: sorry, CLog.T is not supported by clog2coverage for now" << std::endl;
        return 2;
    }

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

    if (!(clogT || clogB))
    {
        std::cerr << "%ERROR: please specify input format" << std::endl;
        return 2;
    }

    std::cout << "%WARNING: collects RN-F ports only for now" << std::endl;

    /* Prepare parsing context */
    size_t flitLengthREQ    = (REQ_t::WIDTH + 7) / 8;
    size_t flitLengthRSP    = (RSP_t::WIDTH + 7) / 8;
    size_t flitLengthDAT    = (DAT_t::WIDTH + 7) / 8;
    size_t flitLengthSNP    = (SNP_t::WIDTH + 7) / 8;

    //
    std::array<uint64_t, 1 << REQ_t::OPCODE_WIDTH>  countTXREQ;
    std::array<uint64_t, 1 << REQ_t::OPCODE_WIDTH>  countTXREQRetry;

    std::array<uint64_t, 1 << RSP_t::OPCODE_WIDTH>  countTXRSP;

    std::array<uint64_t, 1 << DAT_t::OPCODE_WIDTH>  countTXDAT;

    std::array<uint64_t, 1 << SNP_t::OPCODE_WIDTH>  countRXSNP;

    std::array<uint64_t, 1 << RSP_t::OPCODE_WIDTH>  countRXRSP;

    std::array<uint64_t, 1 << DAT_t::OPCODE_WIDTH>  countRXDAT;

    //   REQ        SNP
    std::array<std::array<uint64_t, 1 << SNP_t::OPCODE_WIDTH>, 1 << REQ_t::OPCODE_WIDTH>
                                                    countTXREQNestedRXSNP;

    std::array<std::array<uint64_t, 1 << SNP_t::OPCODE_WIDTH>, 1 << REQ_t::OPCODE_WIDTH>
                                                    countTXREQRetryIntervalNestedRXSNP;

    std::array<std::array<uint64_t, 1 << SNP_t::OPCODE_WIDTH>, 1 << REQ_t::OPCODE_WIDTH>
                                                    countTXREQRetryNestedRXSNP;

    countTXREQ.fill(0);
    countTXREQRetry.fill(0);
    countTXRSP.fill(0);
    countTXDAT.fill(0);
    countRXSNP.fill(0);
    countRXRSP.fill(0);
    countRXDAT.fill(0);

    for (int i = 0; i < (1 << REQ_t::OPCODE_WIDTH); i++)
        countTXREQNestedRXSNP[i].fill(0);

    for (int i = 0; i < (1 << REQ_t::OPCODE_WIDTH); i++)
        countTXREQRetryIntervalNestedRXSNP[i].fill(0);

    for (int i = 0; i < (1 << REQ_t::OPCODE_WIDTH); i++)
        countTXREQRetryNestedRXSNP[i].fill(0);

    
    //
    std::array<
        std::unordered_map<std::vector<flow_entry>, uint64_t, vector_flow_entry_hash, vector_flow_entry_equal_to>, 
        1 << REQ_t::OPCODE_WIDTH> countFlowREQ;

    //
    Topology_t topo;
    
    //
    uint64_t xactionAcceptedCount = 0;
    uint64_t xactionDeniedCount = 0;

    DecoderREQ_t decoderREQ;
    DecoderSNP_t decoderSNP;
    DecoderRSP_t decoderRSP;
    DecoderDAT_t decoderDAT;

    Global_t glbl;

    RNFJoint_t joint;

    if (verboseXact)
    {
        joint.OnAccepted.Register(Gravity::MakeListener(
            "onAcceptedVerbose",
            0,
            std::function([&](CHI::Eb::Xact::JointXactionAcceptedEvent<config>& event) -> void {
                std::cout << ">> Xaction ACCEPTED: " << std::endl;
                std::cout << ">>";
                if (event.GetXaction()->GetFirst().IsREQ())
                {
                    std::cout << "  [" << event.GetXaction()->GetFirst().time << "] TXREQ: ";
                    std::cout << decoderREQ.Decode(event.GetXaction()->GetFirst().flit.req.Opcode()).GetName();
                    std::cout << std::endl;
                }
                else
                {
                    std::cout << "  [" << event.GetXaction()->GetFirst().time << "] RXSNP: ";
                    std::cout << decoderSNP.Decode(event.GetXaction()->GetFirst().flit.snp.Opcode()).GetName();
                    std::cout << std::endl;
                }
            })
        ));

        joint.OnRetry.Register(Gravity::MakeListener(
            "onRetryVerbose",
            0,
            std::function([&](CHI::Eb::Xact::JointXactionRetryEvent<config>& event) -> void {
                std::cout << ">> Xaction RETRY:" << std::endl;
                std::cout << ">>";
                if (event.GetXaction()->GetFirst().IsREQ())
                {
                    std::cout << "  [" << event.GetXaction()->GetFirst().time << "] TXREQ: ";
                    std::cout << decoderREQ.Decode(event.GetXaction()->GetFirst().flit.req.Opcode()).GetName();
                    std::cout << std::endl;
                }
            })
        ));

        joint.OnComplete.Register(Gravity::MakeListener(
            "onCompleteVerbose",
            0,
            std::function([&](CHI::Eb::Xact::JointXactionCompleteEvent<config>& event) -> void {
                std::cout << ">> Xaction " << uint64_t(event.GetXaction()->GetType()) << " COMPLETE:" << std::endl;
                // first
                std::cout << ">>";
                if (event.GetXaction()->GetFirst().IsREQ())
                {
                    std::cout << "  [" << event.GetXaction()->GetFirst().time << "] TXREQ: ";
                    std::cout << decoderREQ.Decode(event.GetXaction()->GetFirst().flit.req.Opcode()).GetName() << " ";
                    std::cout << "(";
                    std::cout << "TxnID = " << uint64_t(event.GetXaction()->GetFirst().flit.req.TxnID());
                    std::cout << ", SrcID = " << uint64_t(event.GetXaction()->GetFirst().flit.req.SrcID());
                    std::cout << ", AllowRetry = " << uint64_t(event.GetXaction()->GetFirst().flit.req.AllowRetry());
                    std::cout << ")";
                    std::cout << std::endl;
                }
                else
                {
                    std::cout << "  [" << event.GetXaction()->GetFirst().time << "] RXSNP: ";
                    std::cout << decoderSNP.Decode(event.GetXaction()->GetFirst().flit.snp.Opcode()).GetName();
                    std::cout << std::endl;
                }
                // subsequent
                size_t index = 0;
                for (auto& subsequent : event.GetXaction()->GetSubsequence())
                {
                    std::cout << ">>";
                    if (subsequent.IsDAT())
                    {
                        std::cout << "  [" << subsequent.time << "] ";
                        if (subsequent.IsTX())
                            std::cout << "TXDAT: ";
                        else
                            std::cout << "RXDAT: ";
                        std::cout << decoderDAT.Decode(subsequent.flit.dat.Opcode()).GetName();
                        std::cout << " (" << event.GetXaction()->GetSubsequentDenial(index)->name << ": ";
                        std::cout << "TxnID = " << uint64_t(subsequent.flit.dat.TxnID());
                        std::cout << ", DBID = " << uint64_t(subsequent.flit.dat.DBID());
                        std::cout << ")";
                        std::cout << std::endl;
                    }
                    else
                    {
                        std::cout << "  [" << subsequent.time << "] ";
                        if (subsequent.IsTX())
                            std::cout << "TXRSP: ";
                        else
                            std::cout << "RXRSP: ";
                        std::cout << decoderRSP.Decode(subsequent.flit.rsp.Opcode()).GetName();
                        std::cout << " (" << event.GetXaction()->GetSubsequentDenial(index)->name << ": ";
                        std::cout << "TxnID = " << uint64_t(subsequent.flit.rsp.TxnID());
                        std::cout << ", DBID = " << uint64_t(subsequent.flit.rsp.DBID());
                        std::cout << ")";
                        std::cout << std::endl;
                    }

                    index++;
                }
            })
        ));
    }

    // collect nesting coverage
    //                 Node ID                      Address   TXREQ Opcode
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t    >> nestMap;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t    >> nestMapRetryInterval;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t    >> nestMapRetry;

    joint.OnAccepted.Register(Gravity::MakeListener(
        "onAcceptedNestCollection",
        0,
        std::function([&](CHI::Eb::Xact::JointXactionAcceptedEvent<config>& event) -> void {
            if (event.GetXaction()->GetFirst().IsREQ())
            {
                uint64_t srcId  = event.GetXaction()->GetFirst().flit.req.SrcID();
                uint64_t addr   = event.GetXaction()->GetFirst().flit.req.Addr();
                uint64_t opcode = event.GetXaction()->GetFirst().flit.req.Opcode();

                std::unordered_map<uint64_t, uint64_t>& nestAddrMap = nestMap[srcId];

                if (nestAddrMap.find(addr) != nestAddrMap.end())
                {
                    std::cerr << "%WARNING: [" <<event.GetXaction()->GetFirst().time << "]" 
                        << " multiple same address TXREQ ("
                        << decoderREQ.Decode(nestAddrMap[addr]).GetName()
                        << " <- " 
                        << decoderREQ.Decode(opcode).GetName() << ")"
                        << " on node " << srcId << " at " 
                        << StringAppender().Hex().ShowBase().Append(addr).ToString() << std::endl;
                    return;
                }

#ifdef DEBUG_NESTING_COLLECTION
                std::cout << "%DEBUG: nestMap"
                    << "[" << srcId << "]" 
                    << "[" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                    << " = "
                    << decoderREQ.Decode(opcode).GetName()
                    << std::endl;
#endif

                nestAddrMap[addr] = opcode;
            }
            else // SNP
            {
                uint64_t tgtId  = event.GetSnoopTargetID();
                uint64_t addr   = event.GetXaction()->GetFirst().flit.snp.Addr();
                uint64_t opcode = event.GetXaction()->GetFirst().flit.snp.Opcode();

                addr = addr << 3;

                // nest on normal
                std::unordered_map<uint64_t, uint64_t>* nestAddrMap = &nestMap[tgtId];

                auto iterNestAddrMap = nestAddrMap->find(addr);
                if (iterNestAddrMap != nestAddrMap->end())
                {
                    if (verboseNest)
                    {
                        std::cout << "%INFO: [" << event.GetXaction()->GetFirst().time << "]" 
                            << " nested ("
                            << decoderSNP.Decode(opcode).GetName()
                            << " <- "
                            << decoderREQ.Decode(iterNestAddrMap->second).GetName();
                        std::cout << ") on node " << tgtId << " at " 
                            << StringAppender().Hex().ShowBase().Append(addr).ToString() << std::endl;
                    }

                    countTXREQNestedRXSNP[iterNestAddrMap->second][opcode]++;
                }

                // nest on retry interval
                nestAddrMap = &nestMapRetryInterval[tgtId];

                iterNestAddrMap = nestAddrMap->find(addr);
                if (iterNestAddrMap != nestAddrMap->end())
                {
                    if (verboseNest)
                    {
                        std::cout << "%INFO: [" << event.GetXaction()->GetFirst().time << "]" 
                            << " nested on retry interval ("
                            << decoderSNP.Decode(opcode).GetName()
                            << " <- "
                            << decoderREQ.Decode(iterNestAddrMap->second).GetName();
                        std::cout << ") on node " << tgtId << " at " 
                            << StringAppender().Hex().ShowBase().Append(addr).ToString() << std::endl;
                    }

                    countTXREQRetryIntervalNestedRXSNP[iterNestAddrMap->second][opcode]++;
                }

                // nest on retired
                nestAddrMap = &nestMapRetry[tgtId];

                iterNestAddrMap = nestAddrMap->find(addr);
                if (iterNestAddrMap != nestAddrMap->end())
                {
                    if (verboseNest)
                    {
                        std::cout << "%INFO: [" << event.GetXaction()->GetFirst().time << "]" 
                            << " nested on retried ("
                            << decoderSNP.Decode(opcode).GetName()
                            << " <- "
                            << decoderREQ.Decode(iterNestAddrMap->second).GetName();
                        std::cout << ") on node " << tgtId << " at " 
                            << StringAppender().Hex().ShowBase().Append(addr).ToString() << std::endl;
                    }

                    countTXREQRetryNestedRXSNP[iterNestAddrMap->second][opcode]++;
                }
            }
        })
    ));

    joint.OnTxnIDFree.Register(Gravity::MakeListener(
        "onTxnIDFreeNestCollection",
        0,
        std::function([&](CHI::Eb::Xact::JointXactionTxnIDFreeEvent<config>& event) -> void {
            if (event.GetXaction()->GetFirst().IsREQ())
            {
                uint64_t srcId  = event.GetXaction()->GetFirst().flit.req.SrcID();
                uint64_t addr   = event.GetXaction()->GetFirst().flit.req.Addr();

                if (event.GetXaction()->IsSecondTry())
                {
#ifdef DEBUG_NESTING_COLLECTION
                    std::cout << "%DEBUG: nestMapRetry"
                        << "[" << srcId << "]" 
                        << " erase [" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                        << std::endl;
#endif
                    
                    nestMapRetry[srcId].erase(addr);
                }
                else
                {
#ifdef DEBUG_NESTING_COLLECTION
                    std::cout << "%DEBUG: nestMap"
                        << "[" << srcId << "]" 
                        << " erase [" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                        << std::endl;
#endif
                    
                    nestMap[srcId].erase(addr);
                }
            }
        })
    ));

    joint.OnComplete.Register(Gravity::MakeListener(
        "onCompleteNestCollection",
        0,
        std::function([&](CHI::Eb::Xact::JointXactionCompleteEvent<config>& event) -> void {
            if (event.GetXaction()->GetFirst().IsREQ())
            {
                uint64_t srcId  = event.GetXaction()->GetFirst().flit.req.SrcID();
                uint64_t addr   = event.GetXaction()->GetFirst().flit.req.Addr();

                if (event.GetXaction()->IsSecondTry())
                {
#ifdef DEBUG_NESTING_COLLECTION
                    std::cout << "%DEBUG: nestMapRetry"
                        << "[" << srcId << "]" 
                        << " erase [" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                        << std::endl;
#endif
                    
                    nestMapRetry[srcId].erase(addr);
                }
                else
                {
#ifdef DEBUG_NESTING_COLLECTION
                    std::cout << "%DEBUG: nestMap"
                        << "[" << srcId << "]" 
                        << " erase [" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                        << std::endl;
#endif

                    nestMap[srcId].erase(addr);
                }

                if (event.GetXaction()->GotRetryAck())
                {
                    uint64_t srcId  = event.GetXaction()->GetFirst().flit.req.SrcID();
                    uint64_t addr   = event.GetXaction()->GetFirst().flit.req.Addr();
                    uint64_t opcode = event.GetXaction()->GetFirst().flit.req.Opcode();

                    std::unordered_map<uint64_t, uint64_t>& nestAddrMap = nestMapRetryInterval[srcId];

                    if (nestAddrMap.find(addr) != nestAddrMap.end())
                    {
                        std::cerr << "%WARNING: [" << event.GetXaction()->GetFirst().time << "]" 
                            << " multiple same address TXREQ on retry interval ("
                            << decoderREQ.Decode(nestAddrMap[addr]).GetName()
                            << " <- " 
                            << decoderREQ.Decode(opcode).GetName() << ")"
                            << " on node " << srcId << " at " 
                            << StringAppender().Hex().ShowBase().Append(addr).ToString() << std::endl;
                        return;
                    }

#ifdef DEBUG_NESTING_COLLECTION
                    std::cout << "%DEBUG: nestMapRetryInterval"
                        << "[" << srcId << "]" 
                        << "[" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                        << " = "
                        << decoderREQ.Decode(opcode).GetName()
                        << std::endl;
#endif

                    nestAddrMap[addr] = opcode;
                }
            }
        })
    ));

    joint.OnRetry.Register(Gravity::MakeListener(
        "onRetryNestCollection",
        0,
        std::function([&](CHI::Eb::Xact::JointXactionRetryEvent<config>& event) -> void {
            if (event.GetXaction()->GetFirst().IsREQ())
            {
                uint64_t srcId  = event.GetXaction()->GetFirst().flit.req.SrcID();
                uint64_t addr   = event.GetXaction()->GetFirst().flit.req.Addr();

#ifdef DEBUG_NESTING_COLLECTION
                std::cout << "%DEBUG: nestMapRetryInterval"
                    << "[" << srcId << "]" 
                    << " erase [" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                    << std::endl;
#endif

                nestMapRetryInterval[srcId].erase(addr);

                uint64_t opcode = event.GetXaction()->GetFirst().flit.req.Opcode();

                std::unordered_map<uint64_t, uint64_t>& nestAddrMap = nestMapRetry[srcId];

                if (nestAddrMap.find(addr) != nestAddrMap.end())
                {
                    std::cerr << "%WARNING: [" <<event.GetXaction()->GetFirst().time << "]" 
                        << " multiple same address TXREQ on retried ("
                        << decoderREQ.Decode(nestAddrMap[addr]).GetName()
                        << " <- " 
                        << decoderREQ.Decode(opcode).GetName() << ")"
                        << " on node " << srcId << " at " 
                        << StringAppender().Hex().ShowBase().Append(addr).ToString() << std::endl;
                    return;
                }

#ifdef DEBUG_NESTING_COLLECTION
                std::cout << "%DEBUG: nestMapRetry"
                    << "[" << srcId << "]" 
                    << "[" << StringAppender().Hex().ShowBase().Append(addr).ToString() << "]"
                    << " = "
                    << decoderREQ.Decode(opcode).GetName()
                    << std::endl;
#endif

                nestAddrMap[addr] = opcode;
            }
        })
    ));


    joint.OnComplete.Register(Gravity::MakeListener(
        "onCompleteFlowCollection",
        0,
        std::function([&](CHI::Eb::Xact::JointXactionCompleteEvent<config>& event) -> void {
            if (event.GetXaction()->GetFirst().IsREQ())
            {
                uint16_t opcode = event.GetXaction()->GetFirst().flit.req.Opcode();

                std::vector<flow_entry> flow;

                for (auto& firedFlit : event.GetXaction()->GetSubsequence())
                {
                    flow_entry entry;

                    entry.isRSP = firedFlit.IsRSP();
                    entry.isRX  = firedFlit.IsRX();

                    if (firedFlit.IsRSP())
                        entry.opcode = firedFlit.flit.rsp.Opcode();
                    else
                        entry.opcode = firedFlit.flit.dat.Opcode();

                    flow.push_back(entry);
                }

                auto& flowCountMap = countFlowREQ[opcode];
                auto flowCountIter = flowCountMap.find(flow);

                if (flowCountIter == flowCountMap.end())
                    flowCountMap[flow] = 1;
                else
                    flowCountIter->second++;
            }
        })
    ));


    //
    CLog::CLogB::ReaderWithCallback                 reader;
    std::unordered_map<uint16_t, CLog::NodeType>    nodes;

    reader.callbackOnTagCHITopologies = [&](std::shared_ptr<CLog::CLogB::TagCHITopologies>  tag,
                                            std::string&                                    errorMessage) -> bool {
        for (auto& node : tag->nodes)
        {
            nodes[node.id] = node.type;

            switch (node.type)
            {
                case CLog::NodeType::RN_F: topo.SetNode(node.id, NodeType::RN_F); break;
                case CLog::NodeType::HN_F: topo.SetNode(node.id, NodeType::HN_F); break;
                default: break;
            }

            std::cout << "%INFO: CHI Topologies: read node " << CLog::NodeTypeToString(node.type) << " " << uint64_t(node.id) << std::endl;
        }
        return true;
    };

    bool firstDisplay = true;
    uint64_t time;
    reader.callbackOnTagCHIRecords = [&](std::shared_ptr<CLog::CLogB::TagCHIRecords>    tag,
                                         std::string&                                   errorMessage) -> bool {
        union {
                REQ_t   req;
                RSP_t   rsp;
                DAT_t   dat;
                SNP_t   snp;
            } flit;

        XactDenialEnum_t denial;

        time = tag->head.timeBase;

        for (auto& record : (*tag).records)
        {
            time += record.timeShift;

            if (nodes[record.nodeId] != CLog::NodeType::RN_F)
                continue;

            switch (record.channel)
            {
                //
                case CLog::Channel::TXREQ:

                    if (size_t(record.flitLength) != flitLengthREQ)
                        std::cerr << "%WARNING: mismatched REQ flit length."
                                  << " read: " << size_t(record.flitLength) << ", expected: " << flitLengthREQ << "." << std::endl;
                    
                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeREQ<config>(flit.req, record.flit.get(), REQ_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeREQ<config>(flit.req, record.flit.get(), REQ_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize REQ flit" << std::endl;
                        return false;
                    }

                    if (flit.req.AllowRetry())
                        countTXREQ[flit.req.Opcode()]++;
                    else
                        countTXREQRetry[flit.req.Opcode()]++;

                    denial = joint.NextTXREQ(&glbl, time, topo, flit.req);

                    if (verboseFlit)
                    {
                        std::cout << "[" << time << "] <TXREQ> " << denial->name << " ";
                        std::cout << decoderREQ.Decode(flit.req.Opcode()).GetName() << " ";
                        std::cout << "(";
                        std::cout << "TxnID = " << flit.req.TxnID();
                        std::cout << ", TgtID = " << flit.req.TgtID();
                        std::cout << ", SrcID = " << flit.req.SrcID();
                        std::cout << ", AllowRetry = " << uint64_t(flit.req.AllowRetry());
                        std::cout << ", PCrdType = " << uint64_t(flit.req.PCrdType());
                        std::cout << ")";
                        std::cout << std::endl;
                    }

                    break;

                //
                case CLog::Channel::RXRSP:
                case CLog::Channel::TXRSP:

                    if (size_t(record.flitLength) != flitLengthRSP)
                        std::cerr << "%WARNING: mismatched RSP flit length."
                                  << " read: " << size_t(record.flitLength) << ", expected: " << flitLengthRSP << "." << std::endl;

                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeRSP<config>(flit.rsp, record.flit.get(), RSP_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeRSP<config>(flit.rsp, record.flit.get(), RSP_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize RSP flit" << std::endl;
                        return false;
                    }

                    if (record.channel == CLog::Channel::RXRSP)
                        countRXRSP[flit.rsp.Opcode()]++;
                    else
                        countTXRSP[flit.rsp.Opcode()]++;

                    if (record.channel == CLog::Channel::RXRSP)
                        denial = joint.NextRXRSP(&glbl, time, topo, flit.rsp);
                    else
                        denial = joint.NextTXRSP(&glbl, time, topo, flit.rsp);

                    if (verboseFlit)
                    {
                        std::cout << "[" << time << "] ";
                        if (record.channel == CLog::Channel::RXRSP)
                            std::cout << "<RXRSP>";
                        else
                            std::cout << "<TXRSP>";
                        std::cout << " " << denial->name << " ";
                        std::cout << decoderRSP.Decode(flit.rsp.Opcode()).GetName() << " ";
                        std::cout << "(";
                        std::cout << "TxnID = " << flit.rsp.TxnID();
                        std::cout << ", TgtID = " << flit.rsp.TgtID();
                        std::cout << ", SrcID = " << flit.rsp.SrcID();
                        std::cout << ", DBID = " << flit.rsp.DBID();
                        std::cout << ", PCrdType = " << uint64_t(flit.rsp.PCrdType());
                        std::cout << ")";
                        std::cout << std::endl;
                    }

                    break;

                //
                case CLog::Channel::RXDAT:
                case CLog::Channel::TXDAT:

                    if (size_t(record.flitLength) != flitLengthDAT)
                        std::cerr << "%WARNING: mismatched DAT flit length."
                                  << " read: " << size_t(record.flitLength) << ", expected: " << flitLengthDAT << "." << std::endl;

                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeDAT<config>(flit.dat, record.flit.get(), DAT_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeDAT<config>(flit.dat, record.flit.get(), DAT_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize DAT flit" << std::endl;
                        return false;
                    }

                    if (record.channel == CLog::Channel::RXDAT)
                        countRXDAT[flit.dat.Opcode()]++;
                    else
                        countTXDAT[flit.dat.Opcode()]++;

                    if (record.channel == CLog::Channel::RXDAT)
                        denial = joint.NextRXDAT(&glbl, time, topo, flit.dat);
                    else
                        denial = joint.NextTXDAT(&glbl, time, topo, flit.dat);

                    if (verboseFlit)
                    {
                        std::cout << "[" << time << "] ";
                        if (record.channel == CLog::Channel::RXDAT)
                            std::cout << "<RXDAT>";
                        else
                            std::cout << "<TXDAT>";
                        std::cout << " " << denial->name << " ";
                        std::cout << decoderDAT.Decode(flit.dat.Opcode()).GetName() << " ";
                        std::cout << "(";
                        std::cout << "TxnID = " << flit.dat.TxnID();
                        std::cout << ", TgtID = " << flit.dat.TgtID();
                        std::cout << ", SrcID = " << flit.dat.SrcID();
                        std::cout << ", DBID = " << flit.dat.DBID();
                        std::cout << ")";
                        std::cout << std::endl;
                    }

                    break;

                //
                case CLog::Channel::RXSNP:

                    if (size_t(record.flitLength) != flitLengthSNP)
                        std::cerr << "%WARNING: mismatched SNP flit length."
                                  << " read: " << size_t(record.flitLength) << ", expected: " << flitLengthSNP << "." << std::endl;

                    if (!
#ifdef CHI_ISSUE_B_ENABLE
                        CHI::B::Flits::DeserializeSNP<config>(flit.snp, record.flit.get(), SNP_t::WIDTH)
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                        CHI::Eb::Flits::DeserializeSNP<config>(flit.snp, record.flit.get(), SNP_t::WIDTH)
#endif
                    )
                    {
                        std::cerr << "%ERROR: unable to deserialize SNP flit" << std::endl;
                        return false;
                    }

                    countRXSNP[flit.snp.Opcode()]++;

                    denial = joint.NextRXSNP(&glbl, time, topo, flit.snp, record.nodeId);

                    if (verboseFlit)
                    {
                        std::cout << "[" << time << "] <RXSNP> " << denial->name << " ";
                        std::cout << decoderSNP.Decode(flit.snp.Opcode()).GetName() << " ";
                        std::cout << "(";
                        std::cout << "TxnID = " << flit.snp.TxnID();
                        std::cout << ", SrcID = " << flit.snp.SrcID();
                        std::cout << ")";
                        std::cout << std::endl;
                    }

                    break;

                //
                default:
                    std::cerr << "%ERROR: unknown CHI channel: " << uint32_t(record.channel) << std::endl;
                    return false;
            }

            if (denial == XactDenial::ACCEPTED)
                xactionAcceptedCount++;
            else
                xactionDeniedCount++;

            if (xactionDeniedCount)
            {
                std::cerr << "%ERROR: denied Xaction flit at time [" << time << "]" << std::endl;
                return false;
            }
        }

        return true;
    };


    for (auto& inputFile : inputFiles)
    {
        // clear context
        joint.Clear();

        nestMap.clear();
        nestMapRetryInterval.clear();
        nestMapRetry.clear();

        // open input file
        std::ifstream ifs(inputFile);

        if (!ifs)
        {
            std::cerr << "%ERROR: cannot open input file: " << inputFile << std::endl;
            return 3;
        }

        std::string errorMessage;

        while (1)
        {
            std::shared_ptr<CLog::CLogB::Tag> tag;
            if (!(tag = reader.Next(ifs, errorMessage)))
            {
                std::cerr << "%ERROR: CLog.B reading error on file: " << inputFile
                    << ", after " << counter << " tags: " << errorMessage << std::endl;
                break;
            }

            if (tag->type == CLog::CLogB::Encodings::_EOF)
                break;

            std::cerr << "%INFO: [" << time << "] CLog.B  : " << counter << " tag(s) read\n";
            std::cerr << "%INFO: [" << time << "] Xaction : " << xactionAcceptedCount << " flit(s) accepted\n";
            std::cerr << "%INFO: [" << time << "]           " << xactionDeniedCount << " flit(s) denied" << std::endl;
            std::cerr << "%INFO: =================================================\n";

            counter++;

            if (!ifs)
                break;
        }
    }

    std::cerr << "%INFO: [" << time << "] CLog.B  : " << counter << " tag(s) read\n";
    std::cerr << "%INFO: [" << time << "] Xaction : " << xactionAcceptedCount << " flit(s) accepted\n";
    std::cerr << "%INFO: [" << time << "]           " << xactionDeniedCount << " flit(s) denied" << std::endl;
    std::cerr << "%INFO: =================================================\n";

    // TXREQ
    std::cout << std::endl;
    std::cout << "TXREQ opcode coverage: " << std::endl;

    auto iterTXREQ = decoderREQ.IterateRNF();
    auto opcodeTXREQ = iterTXREQ.Next();
    do 
    {
        std::cout << opcodeTXREQ.GetName() 
            << " " << countTXREQ        [opcodeTXREQ.GetOpcode()] 
            << " " << countTXREQRetry   [opcodeTXREQ.GetOpcode()] 
            << std::endl;
    }
    while ((opcodeTXREQ = iterTXREQ.Next()).IsValid());

    std::cout << std::endl;

    // TXRSP
    std::cout << std::endl;
    std::cout << "TXRSP opcode coverage: " << std::endl;

    auto iterTXRSP = decoderRSP.IterateRNF();
    auto opcodeTXRSP = iterTXRSP.Next();
    do
    {
        std::cout << opcodeTXRSP.GetName()
            << " " << countTXRSP        [opcodeTXRSP.GetOpcode()]
            << std::endl;
    }
    while ((opcodeTXRSP = iterTXRSP.Next()).IsValid());

    std::cout << std::endl;

    // TXDAT
    std::cout << std::endl;
    std::cout << "TXDAT opcode coverage: " << std::endl;

    auto iterTXDAT = decoderDAT.IterateRNF();
    auto opcodeTXDAT = iterTXDAT.Next();
    do
    {
        std::cout << opcodeTXDAT.GetName()
            << " " << countTXDAT        [opcodeTXDAT.GetOpcode()]
            << std::endl;
    }
    while ((opcodeTXDAT = iterTXDAT.Next()).IsValid());

    std::cout << std::endl;

    // RXSNP
    std::cout << std::endl;
    std::cout << "RXSNP opcode coverage: " << std::endl;

    auto iterRXSNP = decoderSNP.IterateRNF();
    auto opcodeRXSNP = iterRXSNP.Next();
    do
    {
        std::cout << opcodeRXSNP.GetName()
            << " " << countRXSNP        [opcodeRXSNP.GetOpcode()]
            << std::endl;
    }
    while ((opcodeRXSNP = iterRXSNP.Next()).IsValid());

    std::cout << std::endl;

    // RXRSP
    std::cout << std::endl;
    std::cout << "RXRSP opcode coverage: " << std::endl;

    auto iterRXRSP = decoderRSP.IterateRNF();
    auto opcodeRXRSP = iterRXRSP.Next();
    do
    {
        std::cout << opcodeRXRSP.GetName()
            << " " << countRXRSP        [opcodeRXRSP.GetOpcode()]
            << std::endl;
    }
    while ((opcodeRXRSP = iterRXRSP.Next()).IsValid());

    std::cout << std::endl;

    // RXDAT
    std::cout << std::endl;
    std::cout << "RXDAT opcode coverage: " << std::endl;

    auto iterRXDAT = decoderDAT.IterateRNF();
    auto opcodeRXDAT = iterRXDAT.Next();
    do
    {
        std::cout << opcodeRXDAT.GetName()
            << " " << countRXDAT        [opcodeRXDAT.GetOpcode()]
            << std::endl;
    }
    while ((opcodeRXDAT = iterRXDAT.Next()).IsValid());

    std::cout << std::endl;

    // Xaction and Xaction Nesting Coverage
    std::cout << "= Nesting Coverage =============================================" << std::endl;
    std::cout << std::endl;

    iterTXREQ = decoderREQ.IterateRNF();
    opcodeTXREQ = iterTXREQ.Next();
    do
    {
        if (!countTXREQ[opcodeTXREQ.GetOpcode()] && !countTXREQRetry[opcodeTXREQ.GetOpcode()])
            continue;

        std::cout << opcodeTXREQ.GetName() << "" << std::endl;

        iterRXSNP = decoderSNP.IterateRNF();
        auto opcodeRXSNP = iterRXSNP.Next();
        do
        {
            std::cout << "  => " << opcodeRXSNP.GetName()
                << " " << countTXREQNestedRXSNP             [opcodeTXREQ.GetOpcode()][opcodeRXSNP.GetOpcode()]
                << " " << countTXREQRetryIntervalNestedRXSNP[opcodeTXREQ.GetOpcode()][opcodeRXSNP.GetOpcode()]
                << " " << countTXREQRetryNestedRXSNP        [opcodeTXREQ.GetOpcode()][opcodeRXSNP.GetOpcode()]
                << " " << countTXREQNestedRXSNP             [opcodeTXREQ.GetOpcode()][opcodeRXSNP.GetOpcode()]
                        + countTXREQRetryIntervalNestedRXSNP[opcodeTXREQ.GetOpcode()][opcodeRXSNP.GetOpcode()]
                        + countTXREQRetryNestedRXSNP        [opcodeTXREQ.GetOpcode()][opcodeRXSNP.GetOpcode()]
                << std::endl;
        }
        while ((opcodeRXSNP = iterRXSNP.Next()).IsValid());

        std::cout << std::endl;
    }
    while ((opcodeTXREQ = iterTXREQ.Next()).IsValid());
    std::cout << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << std::endl;

    
    // Xaction and Xaction Flow Coverage
    std::cout << "= Flow Coverage ================================================" << std::endl;
    std::cout << std::endl;

    iterTXREQ = decoderREQ.IterateRNF();
    opcodeTXREQ = iterTXREQ.Next();
    do
    {
        int i = 0;

        if (!countTXREQ[opcodeTXREQ.GetOpcode()] && !countTXREQRetry[opcodeTXREQ.GetOpcode()])
            continue;

        std::cout << "Flow variations of: " << opcodeTXREQ.GetName() << "" << std::endl;
        std::cout << std::endl;

        for (auto& flow : countFlowREQ[opcodeTXREQ.GetOpcode()])
        {
            std::cout << "variation #" << i << " " << flow.second << std::endl;
            for (auto entry : flow.first)
            {
                std::cout << "<";

                if (entry.isRX)
                    std::cout << "RX";
                else
                    std::cout << "TX";

                if (entry.isRSP)
                {
                    std::cout << "RSP> "
                        << decoderRSP.Decode(entry.opcode).GetName()
                        << std::endl;
                }
                else
                {
                    std::cout << "DAT> "
                        << decoderDAT.Decode(entry.opcode).GetName()
                        << std::endl;
                }
            }
            std::cout << std::endl;
        }

        std::cout << "----------------------------------------------------------------" << std::endl;
    }
    while ((opcodeTXREQ = iterTXREQ.Next()).IsValid());

    std::cout << "================================================================" << std::endl;

    // TODO


    std::cout << std::endl;
    std::cout << "Xaction: Accepted " << xactionAcceptedCount << " transaction flit(s)." << std::endl;
    std::cout << "Xaction: Denied " << xactionDeniedCount << " transaction flit(s)." << std::endl;

    return 0;
}
