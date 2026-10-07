//*****************************************************************************
// cohestra_vcs_dpi.cpp — C++ side of the Cohestra VCS DPI layer: the layout
// guard, the harness entry (CohestraVCS_Main, called once from the bridge's
// initial block), and the drive/sample exchange behind CohestraVCSTop::eval().
//
// This file is compiled by VCS on the simv link line (see build.sh), NOT into
// the CMake harness archive (the archive's consumers link through it).
//
// DPI scope: the exported tasks/functions are declared inside module
// cohestra_vcs_bridge, so the C++ side must call them with the bridge's scope
// active. CohestraVCS_Main is a *context* import, so when the bridge calls it
// the C++ side captures the caller's scope (svGetScope) and every export call
// re-enters that scope explicitly (svSetScope).
//*****************************************************************************

#include "cohestra_vcs_dpi.hpp"

#include <cstdio>
#include <cstring>
#include <exception>

#include "vcs_top.hpp"


namespace {

    // The bridge instance scope, captured in CohestraVCS_Main (a context
    // import called from the bridge's initial block).
    svScope g_bridgeScope = nullptr;


    // -------------------------------------------------------------------------
    // Pack: shadow top (harness-driven members) -> to-DUT wire struct.
    // Field order in code is irrelevant (named members); the STRUCT LAYOUT is
    // the contract (see cohestra_vcs_dpi.hpp).
    // -------------------------------------------------------------------------
#define COHESTRA_VCS_PACK_T1(P) \
    w.cchi_t1p##P.rxevt_valid            = t.cchi_t1p##P##_rxevt_valid; \
    w.cchi_t1p##P.rxevt_bits_TxnID       = t.cchi_t1p##P##_rxevt_bits_TxnID; \
    w.cchi_t1p##P.rxevt_bits_SrcID       = t.cchi_t1p##P##_rxevt_bits_SrcID; \
    w.cchi_t1p##P.rxevt_bits_TgtID       = t.cchi_t1p##P##_rxevt_bits_TgtID; \
    w.cchi_t1p##P.rxevt_bits_Opcode      = t.cchi_t1p##P##_rxevt_bits_Opcode; \
    w.cchi_t1p##P.rxevt_bits_AddrLo      = uint32_t(t.cchi_t1p##P##_rxevt_bits_Addr); \
    w.cchi_t1p##P.rxevt_bits_AddrHi      = uint32_t(t.cchi_t1p##P##_rxevt_bits_Addr >> 32); \
    w.cchi_t1p##P.rxevt_bits_NS          = t.cchi_t1p##P##_rxevt_bits_NS; \
    w.cchi_t1p##P.rxevt_bits_MemAttr     = t.cchi_t1p##P##_rxevt_bits_MemAttr; \
    w.cchi_t1p##P.rxevt_bits_WayValid    = t.cchi_t1p##P##_rxevt_bits_WayValid; \
    w.cchi_t1p##P.rxevt_bits_Way         = t.cchi_t1p##P##_rxevt_bits_Way; \
    w.cchi_t1p##P.rxevt_bits_TraceTag    = t.cchi_t1p##P##_rxevt_bits_TraceTag; \
    \
    w.cchi_t1p##P.rxreq_valid            = t.cchi_t1p##P##_rxreq_valid; \
    w.cchi_t1p##P.rxreq_bits_TxnID       = t.cchi_t1p##P##_rxreq_bits_TxnID; \
    w.cchi_t1p##P.rxreq_bits_SrcID       = t.cchi_t1p##P##_rxreq_bits_SrcID; \
    w.cchi_t1p##P.rxreq_bits_TgtID       = t.cchi_t1p##P##_rxreq_bits_TgtID; \
    w.cchi_t1p##P.rxreq_bits_Opcode      = t.cchi_t1p##P##_rxreq_bits_Opcode; \
    w.cchi_t1p##P.rxreq_bits_Size        = t.cchi_t1p##P##_rxreq_bits_Size; \
    w.cchi_t1p##P.rxreq_bits_AddrLo      = uint32_t(t.cchi_t1p##P##_rxreq_bits_Addr); \
    w.cchi_t1p##P.rxreq_bits_AddrHi      = uint32_t(t.cchi_t1p##P##_rxreq_bits_Addr >> 32); \
    w.cchi_t1p##P.rxreq_bits_TagAlias    = t.cchi_t1p##P##_rxreq_bits_TagAlias; \
    w.cchi_t1p##P.rxreq_bits_NS          = t.cchi_t1p##P##_rxreq_bits_NS; \
    w.cchi_t1p##P.rxreq_bits_Order       = t.cchi_t1p##P##_rxreq_bits_Order; \
    w.cchi_t1p##P.rxreq_bits_MemAttr     = t.cchi_t1p##P##_rxreq_bits_MemAttr; \
    w.cchi_t1p##P.rxreq_bits_Excl        = t.cchi_t1p##P##_rxreq_bits_Excl; \
    w.cchi_t1p##P.rxreq_bits_ExpCompData = t.cchi_t1p##P##_rxreq_bits_ExpCompData; \
    w.cchi_t1p##P.rxreq_bits_TraceTag    = t.cchi_t1p##P##_rxreq_bits_TraceTag; \
    w.cchi_t1p##P.rxreq_bits_WayValid    = t.cchi_t1p##P##_rxreq_bits_WayValid; \
    w.cchi_t1p##P.rxreq_bits_Way         = t.cchi_t1p##P##_rxreq_bits_Way; \
    \
    w.cchi_t1p##P.rxrsp_valid            = t.cchi_t1p##P##_rxrsp_valid; \
    w.cchi_t1p##P.rxrsp_bits_TxnID       = t.cchi_t1p##P##_rxrsp_bits_TxnID; \
    w.cchi_t1p##P.rxrsp_bits_SrcID       = t.cchi_t1p##P##_rxrsp_bits_SrcID; \
    w.cchi_t1p##P.rxrsp_bits_TgtID       = t.cchi_t1p##P##_rxrsp_bits_TgtID; \
    w.cchi_t1p##P.rxrsp_bits_Opcode      = t.cchi_t1p##P##_rxrsp_bits_Opcode; \
    w.cchi_t1p##P.rxrsp_bits_RespErr     = t.cchi_t1p##P##_rxrsp_bits_RespErr; \
    w.cchi_t1p##P.rxrsp_bits_Resp        = t.cchi_t1p##P##_rxrsp_bits_Resp; \
    w.cchi_t1p##P.rxrsp_bits_TraceTag    = t.cchi_t1p##P##_rxrsp_bits_TraceTag; \
    \
    w.cchi_t1p##P.rxdat_valid            = t.cchi_t1p##P##_rxdat_valid; \
    w.cchi_t1p##P.rxdat_bits_TxnID       = t.cchi_t1p##P##_rxdat_bits_TxnID; \
    w.cchi_t1p##P.rxdat_bits_SrcID       = t.cchi_t1p##P##_rxdat_bits_SrcID; \
    w.cchi_t1p##P.rxdat_bits_TgtID       = t.cchi_t1p##P##_rxdat_bits_TgtID; \
    w.cchi_t1p##P.rxdat_bits_Opcode      = t.cchi_t1p##P##_rxdat_bits_Opcode; \
    w.cchi_t1p##P.rxdat_bits_RespErr     = t.cchi_t1p##P##_rxdat_bits_RespErr; \
    w.cchi_t1p##P.rxdat_bits_Resp        = t.cchi_t1p##P##_rxdat_bits_Resp; \
    w.cchi_t1p##P.rxdat_bits_DataID      = t.cchi_t1p##P##_rxdat_bits_DataID; \
    std::memcpy(w.cchi_t1p##P.rxdat_bits_Data, t.cchi_t1p##P##_rxdat_bits_Data, 8 * sizeof(uint32_t)); \
    w.cchi_t1p##P.rxdat_bits_BE          = t.cchi_t1p##P##_rxdat_bits_BE; \
    w.cchi_t1p##P.rxdat_bits_TraceTag    = t.cchi_t1p##P##_rxdat_bits_TraceTag; \
    \
    w.cchi_t1p##P.txsnp_ready            = t.cchi_t1p##P##_txsnp_ready; \
    w.cchi_t1p##P.txrsp_ready            = t.cchi_t1p##P##_txrsp_ready; \
    w.cchi_t1p##P.txdat_ready            = t.cchi_t1p##P##_txdat_ready

#define COHESTRA_VCS_PACK_AXI_PULL(W, T, PREFIX) \
    W.awready = T.PREFIX##awready; \
    W.wready  = T.PREFIX##wready; \
    W.bvalid  = T.PREFIX##bvalid; \
    W.bid     = T.PREFIX##bid; \
    W.bresp   = T.PREFIX##bresp; \
    W.arready = T.PREFIX##arready; \
    W.rvalid  = T.PREFIX##rvalid; \
    W.rid     = T.PREFIX##rid; \
    std::memcpy(W.rdata, T.PREFIX##rdata, 8 * sizeof(uint32_t)); \
    W.rresp   = T.PREFIX##rresp; \
    W.rlast   = T.PREFIX##rlast

#define COHESTRA_VCS_PACK_AXI_PUSH(W, T, PREFIX) \
    W.awvalid  = T.PREFIX##awvalid; \
    W.awid     = T.PREFIX##awid; \
    W.awaddrLo = uint32_t(T.PREFIX##awaddr); \
    W.awaddrHi = uint32_t(T.PREFIX##awaddr >> 32); \
    W.awlen    = T.PREFIX##awlen; \
    W.awsize   = T.PREFIX##awsize; \
    W.awburst  = T.PREFIX##awburst; \
    W.wvalid   = T.PREFIX##wvalid; \
    std::memcpy(W.wdata, T.PREFIX##wdata, 8 * sizeof(uint32_t)); \
    W.wstrb    = T.PREFIX##wstrb; \
    W.wlast    = T.PREFIX##wlast; \
    W.bready   = T.PREFIX##bready; \
    W.arvalid  = T.PREFIX##arvalid; \
    W.arid     = T.PREFIX##arid; \
    W.araddrLo = uint32_t(T.PREFIX##araddr); \
    W.araddrHi = uint32_t(T.PREFIX##araddr >> 32); \
    W.arlen    = T.PREFIX##arlen; \
    W.arsize   = T.PREFIX##arsize; \
    W.arburst  = T.PREFIX##arburst; \
    W.rready   = T.PREFIX##rready

    void PackToDUT(const CohestraVCSTop& t, Cohestra::VCS::wire::ToDUT& w) noexcept
    {
        w.reset = t.reset;

        COHESTRA_VCS_PACK_T1(0);
        COHESTRA_VCS_PACK_T1(1);
        COHESTRA_VCS_PACK_T1(2);
        COHESTRA_VCS_PACK_T1(3);

        // axi_m0/axi_m1: the harness memory model drives the pull channels.
        COHESTRA_VCS_PACK_AXI_PULL(w.axi_m0, t, axi_m0_);
        COHESTRA_VCS_PACK_AXI_PULL(w.axi_m1, t, axi_m1_);

        // axi_s0: the harness master model drives the push channels.
        COHESTRA_VCS_PACK_AXI_PUSH(w.axi_s0, t, axi_s0_);
    }


    // -------------------------------------------------------------------------
    // Unpack: from-DUT wire struct -> shadow top (DUT-output members).
    // -------------------------------------------------------------------------
#define COHESTRA_VCS_UNPACK_T1(P) \
    t.cchi_t1p##P##_rxevt_ready            = uint8_t(w.cchi_t1p##P.rxevt_ready); \
    t.cchi_t1p##P##_rxreq_ready            = uint8_t(w.cchi_t1p##P.rxreq_ready); \
    t.cchi_t1p##P##_rxrsp_ready            = uint8_t(w.cchi_t1p##P.rxrsp_ready); \
    t.cchi_t1p##P##_rxdat_ready            = uint8_t(w.cchi_t1p##P.rxdat_ready); \
    \
    t.cchi_t1p##P##_txsnp_valid            = uint8_t(w.cchi_t1p##P.txsnp_valid); \
    t.cchi_t1p##P##_txsnp_bits_TxnID       = uint8_t(w.cchi_t1p##P.txsnp_bits_TxnID); \
    t.cchi_t1p##P##_txsnp_bits_SrcID       = uint8_t(w.cchi_t1p##P.txsnp_bits_SrcID); \
    t.cchi_t1p##P##_txsnp_bits_TgtID       = uint8_t(w.cchi_t1p##P.txsnp_bits_TgtID); \
    t.cchi_t1p##P##_txsnp_bits_Opcode      = uint8_t(w.cchi_t1p##P.txsnp_bits_Opcode); \
    t.cchi_t1p##P##_txsnp_bits_Addr        = uint64_t(w.cchi_t1p##P.txsnp_bits_AddrLo) \
                                           | (uint64_t(w.cchi_t1p##P.txsnp_bits_AddrHi) << 32); \
    t.cchi_t1p##P##_txsnp_bits_NS          = uint8_t(w.cchi_t1p##P.txsnp_bits_NS); \
    t.cchi_t1p##P##_txsnp_bits_TraceTag    = uint8_t(w.cchi_t1p##P.txsnp_bits_TraceTag); \
    \
    t.cchi_t1p##P##_txrsp_valid            = uint8_t(w.cchi_t1p##P.txrsp_valid); \
    t.cchi_t1p##P##_txrsp_bits_TxnID       = uint8_t(w.cchi_t1p##P.txrsp_bits_TxnID); \
    t.cchi_t1p##P##_txrsp_bits_SrcID       = uint8_t(w.cchi_t1p##P.txrsp_bits_SrcID); \
    t.cchi_t1p##P##_txrsp_bits_TgtID       = uint8_t(w.cchi_t1p##P.txrsp_bits_TgtID); \
    t.cchi_t1p##P##_txrsp_bits_DBID        = uint8_t(w.cchi_t1p##P.txrsp_bits_DBID); \
    t.cchi_t1p##P##_txrsp_bits_Opcode      = uint8_t(w.cchi_t1p##P.txrsp_bits_Opcode); \
    t.cchi_t1p##P##_txrsp_bits_RespErr     = uint8_t(w.cchi_t1p##P.txrsp_bits_RespErr); \
    t.cchi_t1p##P##_txrsp_bits_Resp        = uint8_t(w.cchi_t1p##P.txrsp_bits_Resp); \
    t.cchi_t1p##P##_txrsp_bits_CBusy       = uint8_t(w.cchi_t1p##P.txrsp_bits_CBusy); \
    t.cchi_t1p##P##_txrsp_bits_WayValid    = uint8_t(w.cchi_t1p##P.txrsp_bits_WayValid); \
    t.cchi_t1p##P##_txrsp_bits_Way         = uint8_t(w.cchi_t1p##P.txrsp_bits_Way); \
    t.cchi_t1p##P##_txrsp_bits_TraceTag    = uint8_t(w.cchi_t1p##P.txrsp_bits_TraceTag); \
    \
    t.cchi_t1p##P##_txdat_valid            = uint8_t(w.cchi_t1p##P.txdat_valid); \
    t.cchi_t1p##P##_txdat_bits_TxnID       = uint8_t(w.cchi_t1p##P.txdat_bits_TxnID); \
    t.cchi_t1p##P##_txdat_bits_SrcID       = uint8_t(w.cchi_t1p##P.txdat_bits_SrcID); \
    t.cchi_t1p##P##_txdat_bits_TgtID       = uint8_t(w.cchi_t1p##P.txdat_bits_TgtID); \
    t.cchi_t1p##P##_txdat_bits_DBID        = uint8_t(w.cchi_t1p##P.txdat_bits_DBID); \
    t.cchi_t1p##P##_txdat_bits_Opcode      = uint8_t(w.cchi_t1p##P.txdat_bits_Opcode); \
    t.cchi_t1p##P##_txdat_bits_RespErr     = uint8_t(w.cchi_t1p##P.txdat_bits_RespErr); \
    t.cchi_t1p##P##_txdat_bits_Resp        = uint8_t(w.cchi_t1p##P.txdat_bits_Resp); \
    t.cchi_t1p##P##_txdat_bits_DataSource  = uint8_t(w.cchi_t1p##P.txdat_bits_DataSource); \
    t.cchi_t1p##P##_txdat_bits_CBusy       = uint8_t(w.cchi_t1p##P.txdat_bits_CBusy); \
    t.cchi_t1p##P##_txdat_bits_DataID      = uint8_t(w.cchi_t1p##P.txdat_bits_DataID); \
    std::memcpy(t.cchi_t1p##P##_txdat_bits_Data, w.cchi_t1p##P.txdat_bits_Data, 8 * sizeof(uint32_t)); \
    t.cchi_t1p##P##_txdat_bits_WayValid    = uint8_t(w.cchi_t1p##P.txdat_bits_WayValid); \
    t.cchi_t1p##P##_txdat_bits_Way         = uint8_t(w.cchi_t1p##P.txdat_bits_Way); \
    t.cchi_t1p##P##_txdat_bits_TraceTag    = uint8_t(w.cchi_t1p##P.txdat_bits_TraceTag)

#define COHESTRA_VCS_UNPACK_AXI_PULL(T, W, PREFIX) \
    T.PREFIX##awready = uint8_t(W.awready); \
    T.PREFIX##wready  = uint8_t(W.wready); \
    T.PREFIX##bvalid  = uint8_t(W.bvalid); \
    T.PREFIX##bid     = W.bid; \
    T.PREFIX##bresp   = uint8_t(W.bresp); \
    T.PREFIX##arready = uint8_t(W.arready); \
    T.PREFIX##rvalid  = uint8_t(W.rvalid); \
    T.PREFIX##rid     = W.rid; \
    std::memcpy(T.PREFIX##rdata, W.rdata, 8 * sizeof(uint32_t)); \
    T.PREFIX##rresp   = uint8_t(W.rresp); \
    T.PREFIX##rlast   = uint8_t(W.rlast)

#define COHESTRA_VCS_UNPACK_AXI_PUSH(T, W, PREFIX) \
    T.PREFIX##awvalid = uint8_t(W.awvalid); \
    T.PREFIX##awid    = W.awid; \
    T.PREFIX##awaddr  = uint64_t(W.awaddrLo) | (uint64_t(W.awaddrHi) << 32); \
    T.PREFIX##awlen   = uint8_t(W.awlen); \
    T.PREFIX##awsize  = uint8_t(W.awsize); \
    T.PREFIX##awburst = uint8_t(W.awburst); \
    T.PREFIX##wvalid  = uint8_t(W.wvalid); \
    std::memcpy(T.PREFIX##wdata, W.wdata, 8 * sizeof(uint32_t)); \
    T.PREFIX##wstrb   = W.wstrb; \
    T.PREFIX##wlast   = uint8_t(W.wlast); \
    T.PREFIX##bready  = uint8_t(W.bready); \
    T.PREFIX##arvalid = uint8_t(W.arvalid); \
    T.PREFIX##arid    = W.arid; \
    T.PREFIX##araddr  = uint64_t(W.araddrLo) | (uint64_t(W.araddrHi) << 32); \
    T.PREFIX##arlen   = uint8_t(W.arlen); \
    T.PREFIX##arsize  = uint8_t(W.arsize); \
    T.PREFIX##arburst = uint8_t(W.arburst); \
    T.PREFIX##rready  = uint8_t(W.rready)

    void UnpackFromDUT(const Cohestra::VCS::wire::FromDUT& w, CohestraVCSTop& t) noexcept
    {
        COHESTRA_VCS_UNPACK_T1(0);
        COHESTRA_VCS_UNPACK_T1(1);
        COHESTRA_VCS_UNPACK_T1(2);
        COHESTRA_VCS_UNPACK_T1(3);

        // axi_m0/axi_m1: the DUT (mesh memory masters) drives the push channels.
        COHESTRA_VCS_UNPACK_AXI_PUSH(t, w.axi_m0, axi_m0_);
        COHESTRA_VCS_UNPACK_AXI_PUSH(t, w.axi_m1, axi_m1_);

        // axi_s0: the DUT (mesh config slave) drives the pull channels.
        COHESTRA_VCS_UNPACK_AXI_PULL(t, w.axi_s0, axi_s0_);
    }

#undef COHESTRA_VCS_PACK_T1
#undef COHESTRA_VCS_PACK_AXI_PULL
#undef COHESTRA_VCS_PACK_AXI_PUSH
#undef COHESTRA_VCS_UNPACK_T1
#undef COHESTRA_VCS_UNPACK_AXI_PULL
#undef COHESTRA_VCS_UNPACK_AXI_PUSH
}


//*****************************************************************************
// Layout guard (bridge calls this before starting the harness).
//*****************************************************************************
extern "C" int CohestraVCS_CheckLayout(int toDutBits, int fromDutBits)
{
    const int expectedTo   = int(8 * sizeof(Cohestra::VCS::wire::ToDUT));
    const int expectedFrom = int(8 * sizeof(Cohestra::VCS::wire::FromDUT));

    if (toDutBits != expectedTo || fromDutBits != expectedFrom)
    {
        std::fprintf(stderr,
            "[cohestra_vcs] FATAL: DPI struct layout mismatch between SystemVerilog and C++:\n"
            "  to-DUT:   SV $bits = %d, C++ 8*sizeof = %d\n"
            "  from-DUT: SV $bits = %d, C++ 8*sizeof = %d\n"
            "  The packed structs in cohestra_vcs_dpi.svh and their mirrors in\n"
            "  cohestra_vcs_dpi.hpp must match bit for bit (see the layout\n"
            "  contract in either file's header).\n",
            toDutBits, expectedTo, fromDutBits, expectedFrom);
        return 1;
    }

    return 0;
}


//*****************************************************************************
// The harness entry (called once from the bridge's initial block).
//*****************************************************************************
extern "C" void CohestraVCS_Main(
    const char*         configPath,
    uint64_t            maximumCycles,
    const char*         vcdPath,
    const char*         fsdbPath,
    const char*         logFilePath,
    int                 noLogFile,
    int*                status)
{
    // Capture the caller's scope (the cohestra_vcs_bridge instance) so the
    // exported tasks/functions can be re-entered from any C++ context.
    g_bridgeScope = svGetScope();

    Cohestra::VCS::HarnessArguments args;
    args.configPath     = configPath   ? configPath   : "";
    args.maximumCycles  = maximumCycles;
    args.vcdPath        = vcdPath      ? vcdPath      : "";
    args.fsdbPath       = fsdbPath     ? fsdbPath     : "";
    args.logFilePath    = logFilePath  ? logFilePath  : "";
    args.noLogFile      = noLogFile != 0;

    int rc;
    try
    {
        rc = Cohestra::VCS::RunHarness(args);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[cohestra_vcs] FATAL: uncaught exception in harness: %s\n", e.what());
        rc = 1;
    }
    catch (...)
    {
        std::fprintf(stderr, "[cohestra_vcs] FATAL: uncaught unknown exception in harness.\n");
        rc = 1;
    }

    if (status)
        *status = rc;

    g_bridgeScope = nullptr;
}


//*****************************************************************************
// The exchange behind CohestraVCSTop::eval() + the waveform control.
//*****************************************************************************
namespace Cohestra::VCS {

    bool IsBridgeBound() noexcept
    {
        return g_bridgeScope != nullptr;
    }

    void EvalTop(CohestraVCSTop& top) noexcept
    {
        if (!g_bridgeScope)
        {
            // Not bound to a live simulation (host-side use of the shadow
            // top): there is nothing to exchange with.
            return;
        }

        if (top.clock == 0)
        {
            // "eval with clock=0": drive the pins, settle through the low
            // phase, then sample the settled pins back into the shadow.
            wire::ToDUT drv;
            PackToDUT(top, drv);

            svSetScope(g_bridgeScope);
            CohestraVCS_DriveLow(reinterpret_cast<const svBitVecVal*>(&drv));

            wire::FromDUT out;
            svSetScope(g_bridgeScope);
            CohestraVCS_Sample(reinterpret_cast<svBitVecVal*>(&out));

            UnpackFromDUT(out, top);
        }
        else
        {
            // "eval with clock=1": the posedge. Driven pins hold.
            svSetScope(g_bridgeScope);
            CohestraVCS_DriveHigh();

            // Re-sample after the posedge: a Verilator clock=1 eval leaves the
            // pins at their post-posedge state, and the harness's cycle-start
            // logic (Instance::TickCommon) depends on that — a beat retired at
            // the posedge must no longer be visible, otherwise the lingering
            // pre-fire sample is pushed to the xaction layer a second time
            // (seen as XACT_DENIED_DNDAT_DUPLICATED_DATAID /
            // XACT_DENIED_DNRSP_TXNID_NOT_EXIST storms).
            wire::FromDUT out;
            svSetScope(g_bridgeScope);
            CohestraVCS_Sample(reinterpret_cast<svBitVecVal*>(&out));

            UnpackFromDUT(out, top);
        }
    }

    bool OpenWaveform(uint32_t waveType, const char* path, uint64_t startTick) noexcept
    {
        if (!g_bridgeScope)
            return false;

        svSetScope(g_bridgeScope);
        CohestraVCS_OpenWaveform(waveType, path, startTick);
        return true;
    }

    void CloseWaveform() noexcept
    {
        if (!g_bridgeScope)
            return;

        svSetScope(g_bridgeScope);
        CohestraVCS_CloseWaveform();
    }
}
