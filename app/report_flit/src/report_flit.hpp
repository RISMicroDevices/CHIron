#include <iostream>

#ifndef REPORT_FLIT__STANDALONE
#   define CHI_ISSUE_B_ENABLE
#endif

#ifndef _NODEID_WIDTH
#   define  _NODEID_WIDTH           7
#endif

#ifndef _REQ_ADDR_WIDTH
#   define  _REQ_ADDR_WIDTH         48
#endif

#ifndef _REQ_RSVDC_WIDTH
#   define  _REQ_RSVDC_WIDTH        0
#endif

#ifndef _DAT_RSVDC_WIDTH
#   define  _DAT_RSVDC_WIDTH        0
#endif

#ifndef _DATA_WIDTH
#   define  _DATA_WIDTH             256
#endif

#ifndef _DATACHECK_PRESENT
#   define  _DATACHECK_PRESENT      false
#endif

#ifndef _POISON_PRESENT
#   define  _POISON_PRESENT         false
#endif

#ifndef _MPAM_PRESENT
#   define  _MPAM_PRESENT           false
#endif

#ifdef CHI_ISSUE_B_ENABLE
    #include "../../../chi_b/spec/chi_b_protocol_flits.hpp"          // IWYU pragma: keep
    #define CHI_ISSUE_B_ENABLE
    using namespace CHI::B::Flits;
    using config = CHI::B::FlitConfiguration<
        _NODEID_WIDTH,
        _REQ_ADDR_WIDTH,
        _REQ_RSVDC_WIDTH,
        _DAT_RSVDC_WIDTH,
        _DATA_WIDTH,
        _DATACHECK_PRESENT,
        _POISON_PRESENT
    >;
#endif

#ifdef CHI_ISSUE_C_ENABLE
    #include "../../../chi_c/spec/chi_c_protocol_flits.hpp"          // IWYU pragma: keep
    #define CHI_ISSUE_C_ENABLE
    using namespace CHI::C::Flits;
    using config = CHI::C::FlitConfiguration<
        _NODEID_WIDTH,
        _REQ_ADDR_WIDTH,
        _REQ_RSVDC_WIDTH,
        _DAT_RSVDC_WIDTH,
        _DATA_WIDTH,
        _DATACHECK_PRESENT,
        _POISON_PRESENT
    >;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    #include "../../../chi_eb/spec/chi_eb_protocol_flits.hpp"        // IWYU pragma: keep
    #define CHI_ISSUE_EB_ENABLE
    using namespace CHI::Eb::Flits;
    using config = CHI::Eb::FlitConfiguration<
        _NODEID_WIDTH,
        _REQ_ADDR_WIDTH,
        _REQ_RSVDC_WIDTH,
        _DAT_RSVDC_WIDTH,
        _DATA_WIDTH,
        _DATACHECK_PRESENT,
        _POISON_PRESENT,
        _MPAM_PRESENT
    >;
#endif

#define NUM_FORMATL std::left << std::setw(3) << std::setfill(' ')
#define NUM_FORMATR std::right << std::setw(3) << std::setfill(' ')

#define INFO_FORMAT(root, name) NUM_FORMATL << root<config>::name##_WIDTH << std::right << "    Range: [" << NUM_FORMATR << root<config>::name##_MSB << ":" << NUM_FORMATR << root<config>::name##_LSB << "]"

static void print_version() noexcept
{
    std::cerr << std::endl;
    std::cerr << "                CHIron Toolset" << std::endl;
    std::cerr << std::endl;
    std::cerr << "           report_flit - " << __DATE__ << std::endl;
    std::cerr << std::endl;
    std::cerr << "  report_flit: Static CHI Flit information reporter" << std::endl;
    std::cerr << std::endl;
}

inline int report_flit_main()
{
    print_version();

    //
    std::cout << "========================================================" << std::endl;
#ifdef CHI_ISSUE_B_ENABLE
    std::cout << "report_flit (Issue B)" << std::endl;
#endif
#ifdef CHI_ISSUE_C_ENABLE
    std::cout << "report_flit (Issue C)" << std::endl;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "report_flit (Issue E.b)" << std::endl;
#endif
    //
    std::cout << "=Parameters=============================================" << std::endl;
    std::cout << "config::nodeIdWidth       = " << config::nodeIdWidth << std::endl;
    std::cout << "config::reqAddrWidth      = " << config::reqAddrWidth << std::endl;
    std::cout << "config::snpAddrWidth      = " << config::snpAddrWidth << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "config::tagWidth          = " << config::tagWidth << std::endl;
    std::cout << "config::tagUpdateWidth    = " << config::tagUpdateWidth << std::endl;
#endif
    std::cout << "config::reqRsvdcWidth     = " << config::reqRsvdcWidth << std::endl;
    std::cout << "config::datRsvdcWidth     = " << config::datRsvdcWidth << std::endl;
    std::cout << "config::dataWidth         = " << config::dataWidth << std::endl;
    std::cout << "config::byteEnableWidth   = " << config::byteEnableWidth << std::endl;
    std::cout << "config::dataCheckWidth    = " << config::dataCheckWidth << std::endl;
    std::cout << "config::poisonWidth       = " << config::poisonWidth << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "config::mpamWidth         = " << config::mpamWidth << std::endl;
#endif
    std::cout << std::endl;
    std::cout << "=CHI Overall============================================" << std::endl;
    std::cout << "CHI REQ Width: " << REQ<config>::WIDTH << std::endl;
    std::cout << "CHI RSP Width: " << RSP<config>::WIDTH << std::endl;
    std::cout << "CHI SNP Width: " << SNP<config>::WIDTH << std::endl;
    std::cout << "CHI DAT Width: " << DAT<config>::WIDTH << std::endl;
    std::cout << std::endl;
    std::cout << "=REQ====================================================" << std::endl;
    std::cout << "CHI REQ Width: " << REQ<config>::WIDTH << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "CHI REQ::QoS              Width: " << INFO_FORMAT(REQ, QOS)               << std::endl;
    std::cout << "CHI REQ::TgtId            Width: " << INFO_FORMAT(REQ, TGTID)             << std::endl;
    std::cout << "CHI REQ::SrcId            Width: " << INFO_FORMAT(REQ, SRCID)             << std::endl;
    std::cout << "CHI REQ::TxnId            Width: " << INFO_FORMAT(REQ, TXNID)             << std::endl;
    std::cout << "CHI REQ::ReturnNID        Width: " << INFO_FORMAT(REQ, RETURNNID)         << std::endl;
    std::cout << "      -> StashNID         Width: " << INFO_FORMAT(REQ, STASHNID)          << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "      -> SLCRepHint       Width: " << INFO_FORMAT(REQ, SLCREPHINT)        << std::endl;
#endif
    std::cout << "CHI REQ::StashNIDValid    Width: " << INFO_FORMAT(REQ, STASHNIDVALID)     << std::endl;
    std::cout << "      -> Endian           Width: " << INFO_FORMAT(REQ, ENDIAN)            << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "      -> Deep             Width: " << INFO_FORMAT(REQ, DEEP)              << std::endl;
#endif
    std::cout << "CHI REQ::ReturnTxnID      Width: " << INFO_FORMAT(REQ, RETURNTXNID)       << std::endl;
    std::cout << "      -> StashLPIDValid   Width: " << INFO_FORMAT(REQ, STASHLPIDVALID)    << std::endl;
    std::cout << "      -> StashLPID        Width: " << INFO_FORMAT(REQ, STASHLPID)         << std::endl;
    std::cout << "CHI REQ::Opcode           Width: " << INFO_FORMAT(REQ, OPCODE)            << std::endl;
    std::cout << "CHI REQ::Size             Width: " << INFO_FORMAT(REQ, SSIZE)             << std::endl;
    std::cout << "CHI REQ::Addr             Width: " << INFO_FORMAT(REQ, ADDR)              << std::endl;
    std::cout << "CHI REQ::NS               Width: " << INFO_FORMAT(REQ, NS)                << std::endl;
    std::cout << "CHI REQ::LikelyShared     Width: " << INFO_FORMAT(REQ, LIKELYSHARED)      << std::endl;
    std::cout << "CHI REQ::AllowRetry       Width: " << INFO_FORMAT(REQ, ALLOWRETRY)        << std::endl;
    std::cout << "CHI REQ::Order            Width: " << INFO_FORMAT(REQ, ORDER)             << std::endl;
    std::cout << "CHI REQ::PCrdType         Width: " << INFO_FORMAT(REQ, PCRDTYPE)          << std::endl;
    std::cout << "CHI REQ::MemAttr          Width: " << INFO_FORMAT(REQ, MEMATTR)           << std::endl;
    std::cout << "CHI REQ::SnpAttr          Width: " << INFO_FORMAT(REQ, SNPATTR)           << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "      -> DoDWT            Width: " << INFO_FORMAT(REQ, DODWT)             << std::endl;
#endif
    std::cout << "CHI REQ::LPID             Width: " << INFO_FORMAT(REQ, LPID)              << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "      -> PGroupID         Width: " << INFO_FORMAT(REQ, PGROUPID)          << std::endl;
    std::cout << "      -> StashGroupID     Width: " << INFO_FORMAT(REQ, STASHGROUPID)      << std::endl;
    std::cout << "      -> TagGroupID       Width: " << INFO_FORMAT(REQ, TAGGROUPID)        << std::endl;
#endif
    std::cout << "CHI REQ::Excl             Width: " << INFO_FORMAT(REQ, EXCL)              << std::endl;
    std::cout << "      -> SnoopMe          Width: " << INFO_FORMAT(REQ, SNOOPME)           << std::endl;
    std::cout << "CHI REQ::ExpCompAck       Width: " << INFO_FORMAT(REQ, EXPCOMPACK)        << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "CHI REQ::TagOp            Width: " << INFO_FORMAT(REQ, TAGOP)             << std::endl;
#endif
    std::cout << "CHI REQ::TraceTag         Width: " << INFO_FORMAT(REQ, TRACETAG)          << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    if constexpr (REQ<config>::hasMPAM)
        std::cout << "CHI REQ::MPAM             Width: " << INFO_FORMAT(REQ, MPAM)              << std::endl;
    else
        std::cout << "CHI REQ::MPAM             (N/A)" << std::endl;
#endif
    if constexpr (REQ<config>::hasRSVDC)
        std::cout << "CHI REQ::RSVDC            Width: " << INFO_FORMAT(REQ, RSVDC)             << std::endl;
    else
        std::cout << "CHI REQ::RSVDC            (N/A)" << std::endl;
    std::cout << std::endl;
    std::cout << "=RSP====================================================" << std::endl;
    std::cout << "CHI RSP Width: " << RSP<config>::WIDTH << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "CHI RSP::QoS              Width: " << INFO_FORMAT(RSP, QOS)               << std::endl;
    std::cout << "CHI RSP::TgtId            Width: " << INFO_FORMAT(RSP, TGTID)             << std::endl;
    std::cout << "CHI RSP::SrcId            Width: " << INFO_FORMAT(RSP, SRCID)             << std::endl;
    std::cout << "CHI RSP::TxnId            Width: " << INFO_FORMAT(RSP, TXNID)             << std::endl;
    std::cout << "CHI RSP::Opcode           Width: " << INFO_FORMAT(RSP, OPCODE)            << std::endl;
    std::cout << "CHI RSP::RespErr          Width: " << INFO_FORMAT(RSP, RESPERR)           << std::endl;
    std::cout << "CHI RSP::Resp             Width: " << INFO_FORMAT(RSP, RESP)              << std::endl;
    std::cout << "CHI RSP::FwdState         Width: " << INFO_FORMAT(RSP, FWDSTATE)          << std::endl;
    std::cout << "      -> DataPull         Width: " << INFO_FORMAT(RSP, DATAPULL)          << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "CHI RSP::CBusy            Width: " << INFO_FORMAT(RSP, CBUSY)             << std::endl;
#endif
    std::cout << "CHI RSP::DBID             Width: " << INFO_FORMAT(RSP, DBID)              << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "      -> PGroupID         Width: " << INFO_FORMAT(RSP, PGROUPID)          << std::endl;
    std::cout << "      -> StashGroupID     Width: " << INFO_FORMAT(RSP, STASHGROUPID)      << std::endl;
    std::cout << "      -> TagGroupID       Width: " << INFO_FORMAT(RSP, TAGGROUPID)        << std::endl;
#endif
    std::cout << "CHI RSP::PCrdType         Width: " << INFO_FORMAT(RSP, PCRDTYPE)          << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "CHI RSP::TagOp            Width: " << INFO_FORMAT(RSP, TAGOP)             << std::endl;
#endif
    std::cout << "CHI RSP::TraceTag         Width: " << INFO_FORMAT(RSP, TRACETAG)          << std::endl;
    std::cout << std::endl;
    std::cout << "=SNP====================================================" << std::endl;
    std::cout << "CHI SNP Width: " << SNP<config>::WIDTH << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "CHI SNP::QoS              Width: " << INFO_FORMAT(SNP, QOS)               << std::endl;
    std::cout << "CHI SNP::SrcId            Width: " << INFO_FORMAT(SNP, SRCID)             << std::endl;
    std::cout << "CHI SNP::TxnId            Width: " << INFO_FORMAT(SNP, TXNID)             << std::endl;
    std::cout << "CHI SNP::FwdNID           Width: " << INFO_FORMAT(SNP, FWDNID)            << std::endl; 
    std::cout << "CHI SNP::FwdTxnID         Width: " << INFO_FORMAT(SNP, FWDTXNID)          << std::endl;
    std::cout << "      -> StashLPIDValid   Width: " << INFO_FORMAT(SNP, STASHLPIDVALID)    << std::endl;
    std::cout << "      -> StashLPID        Width: " << INFO_FORMAT(SNP, STASHLPID)         << std::endl;
    std::cout << "      -> VMIDExt          Width: " << INFO_FORMAT(SNP, VMIDEXT)           << std::endl;
    std::cout << "CHI SNP::Opcode           Width: " << INFO_FORMAT(SNP, OPCODE)            << std::endl;
    std::cout << "CHI SNP::Addr             Width: " << INFO_FORMAT(SNP, ADDR)              << std::endl;
    std::cout << "CHI SNP::NS               Width: " << INFO_FORMAT(SNP, NS)                << std::endl;
    std::cout << "CHI SNP::DoNotGoToSD      Width: " << INFO_FORMAT(SNP, DONOTGOTOSD)       << std::endl;
#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
    std::cout << "      -> DoNotDataPull    Width: " << INFO_FORMAT(SNP, DONOTDATAPULL)     << std::endl;
#endif
    std::cout << "CHI SNP::RetToSrc         Width: " << INFO_FORMAT(SNP, RETTOSRC)          << std::endl;
    std::cout << "CHI SNP::TraceTag         Width: " << INFO_FORMAT(SNP, TRACETAG)          << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    if constexpr (SNP<config>::hasMPAM)
        std::cout << "CHI SNP::MPAM             Width: " << INFO_FORMAT(SNP, MPAM)              << std::endl;
    else
        std::cout << "CHI SNP::MPAM             (N/A)" << std::endl;
#endif
    std::cout << std::endl;
    std::cout << "=DAT====================================================" << std::endl;
    std::cout << "CHI DAT Width: " << DAT<config>::WIDTH << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "CHI DAT::QoS              Width: " << INFO_FORMAT(DAT, QOS)               << std::endl;
    std::cout << "CHI DAT::TgtID            Width: " << INFO_FORMAT(DAT, TGTID)             << std::endl;
    std::cout << "CHI DAT::SrcID            Width: " << INFO_FORMAT(DAT, SRCID)             << std::endl;
    std::cout << "CHI DAT::TxnID            Width: " << INFO_FORMAT(DAT, TXNID)             << std::endl;
    std::cout << "CHI DAT::HomeNID          Width: " << INFO_FORMAT(DAT, HOMENID)           << std::endl;
    std::cout << "CHI DAT::Opcode           Width: " << INFO_FORMAT(DAT, OPCODE)            << std::endl;
    std::cout << "CHI DAT::RespErr          Width: " << INFO_FORMAT(DAT, RESPERR)           << std::endl;
    std::cout << "CHI DAT::Resp             Width: " << INFO_FORMAT(DAT, RESP)              << std::endl;
    std::cout << "CHI DAT::FwdState         Width: " << INFO_FORMAT(DAT, FWDSTATE)          << std::endl;
    std::cout << "      -> DataPull         Width: " << INFO_FORMAT(DAT, DATAPULL)          << std::endl;
    std::cout << "      -> DataSource       Width: " << INFO_FORMAT(DAT, DATASOURCE)        << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "CHI DAT::CBusy            Width: " << INFO_FORMAT(DAT, CBUSY)             << std::endl;
#endif
    std::cout << "CHI DAT::DBID             Width: " << INFO_FORMAT(DAT, DBID)              << std::endl;
    std::cout << "CHI DAT::CCID             Width: " << INFO_FORMAT(DAT, CCID)              << std::endl;
    std::cout << "CHI DAT::DataID           Width: " << INFO_FORMAT(DAT, DATAID)            << std::endl;
#ifdef CHI_ISSUE_EB_ENABLE
    std::cout << "CHI DAT::TagOp            Width: " << INFO_FORMAT(DAT, TAGOP)             << std::endl;
    std::cout << "CHI DAT::Tag              Width: " << INFO_FORMAT(DAT, TAG)               << std::endl;
    std::cout << "CHI DAT::TU               Width: " << INFO_FORMAT(DAT, TU)                << std::endl;
#endif
    std::cout << "CHI DAT::TraceTag         Width: " << INFO_FORMAT(DAT, TRACETAG)          << std::endl;
    if constexpr (DAT<config>::hasRSVDC)
        std::cout << "CHI DAT::RSVDC            Width: " << INFO_FORMAT(DAT, RSVDC)             << std::endl;
    else
        std::cout << "CHI DAT::RSVDC            (N/A)" << std::endl;
    std::cout << "CHI DAT::BE               Width: " << INFO_FORMAT(DAT, BE)                << std::endl;
    std::cout << "CHI DAT::Data             Width: " << INFO_FORMAT(DAT, DATA)              << std::endl;
    if constexpr (DAT<config>::hasDataCheck)
        std::cout << "CHI DAT::DataCheck        Width: " << INFO_FORMAT(DAT, DATACHECK)         << std::endl;
    else
        std::cout << "CHI DAT::DataCheck        (N/A)" << std::endl;
    if constexpr (DAT<config>::hasPoison)
        std::cout << "CHI DAT::Poison           Width: " << INFO_FORMAT(DAT, POISON)            << std::endl;
    else
        std::cout << "CHI DAT::Poison           (N/A)" << std::endl;
    std::cout << std::endl;
    std::cout << "========================================================" << std::endl;

    return 0;
}