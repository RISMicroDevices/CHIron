//*****************************************************************************
// vcs_top.hpp — CohestraVCSTop: a POD shadow of the DUT boundary pins for the
// Cohestra VCS harness (the VCS/DPI counterpart of the Verilator-generated
// model class the cohestra_v3 flow binds to).
//
// One member per boundary pin of the DUT top (default avocado_top; see
// cohestra_packages/avocado/avocado_top.sv and integration/avocado_dut_if.svh),
// with the EXACT pin names, so the cohestra_v3 interface templates
// (V3CCHIInterface, V3AXISlaveInterface, V3AXIMasterInterface,
// V3AXIMonitorInterface, V3BootHost) instantiate unchanged against this type
// (precedent: cohestra_v3/test_chi_smoke.hpp CHIPackedRN0MockTop).
//
// Member types mirror how the Verilator model presents the same pins:
//   uint8_t        1..8-bit fields (valid/ready/opcode/IDs <= 8 bits/...)
//   uint32_t       9..32-bit fields (AXI IDs: Verilator would use IData for
//                  the 24-bit axi_m IDs and SData for the 11-bit axi_s0 IDs;
//                  uint32_t is used for both — the pin traits only require an
//                  integral type, and it keeps the DPI wire format uniform)
//   uint64_t       33..64-bit fields (the 48/45-bit addresses)
//   uint32_t[8]    256-bit data (Verilator VlWide<8> word order: [0] = LSW)
//
// eval()/final() glue: eval() runs the DPI exchange (see cohestra_vcs_dpi.cpp)
//   clock==0: pack drive struct -> CohestraVCS_DriveLow -> CohestraVCS_Sample
//             -> unpack into the DUT-output members (the "eval with clock=0"
//             phase of the Verilator flow, settle + sample atomically)
//   clock==1: CohestraVCS_DriveHigh (the posedge phase)
// final() is a no-op kept for Verilator-model API parity.
//
// The boundary has NO CHI pins (they are internal to avocado_top), so the
// V3CHI*Interface templates detect no ports and stay inert, exactly as they
// do for a Verilated avocado_top.
//*****************************************************************************
#pragma once

#include <cstdint>

#include "cohestra_vcs_dpi.hpp"

#include "interface.hpp"
#include "interface_axi.hpp"


// Members of one CCHI Type-1 port (harness-driven = DUT inputs first, then
// the DUT outputs). Widths are the contract widths (avocado_dut_if.svh).
#define COHESTRA_VCS_T1_MEMBERS(P) \
    /* rxevt (harness -> DUT) */ \
    uint8_t  cchi_t1p##P##_rxevt_valid            = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_TxnID       = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_SrcID       = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_TgtID       = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_Opcode      = 0; \
    uint64_t cchi_t1p##P##_rxevt_bits_Addr        = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_NS          = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_MemAttr     = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_WayValid    = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_Way         = 0; \
    uint8_t  cchi_t1p##P##_rxevt_bits_TraceTag    = 0; \
    uint8_t  cchi_t1p##P##_rxevt_ready            = 0; \
    /* rxreq (harness -> DUT) */ \
    uint8_t  cchi_t1p##P##_rxreq_valid            = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_TxnID       = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_SrcID       = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_TgtID       = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_Opcode      = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_Size        = 0; \
    uint64_t cchi_t1p##P##_rxreq_bits_Addr        = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_TagAlias    = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_NS          = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_Order       = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_MemAttr     = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_Excl        = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_ExpCompData = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_TraceTag    = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_WayValid    = 0; \
    uint8_t  cchi_t1p##P##_rxreq_bits_Way         = 0; \
    uint8_t  cchi_t1p##P##_rxreq_ready            = 0; \
    /* txsnp (DUT -> harness) */ \
    uint8_t  cchi_t1p##P##_txsnp_ready            = 0; \
    uint8_t  cchi_t1p##P##_txsnp_valid            = 0; \
    uint8_t  cchi_t1p##P##_txsnp_bits_TxnID       = 0; \
    uint8_t  cchi_t1p##P##_txsnp_bits_SrcID       = 0; \
    uint8_t  cchi_t1p##P##_txsnp_bits_TgtID       = 0; \
    uint8_t  cchi_t1p##P##_txsnp_bits_Opcode      = 0; \
    uint64_t cchi_t1p##P##_txsnp_bits_Addr        = 0; \
    uint8_t  cchi_t1p##P##_txsnp_bits_NS          = 0; \
    uint8_t  cchi_t1p##P##_txsnp_bits_TraceTag    = 0; \
    /* txrsp (DUT -> harness) */ \
    uint8_t  cchi_t1p##P##_txrsp_ready            = 0; \
    uint8_t  cchi_t1p##P##_txrsp_valid            = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_TxnID       = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_SrcID       = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_TgtID       = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_DBID        = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_Opcode      = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_RespErr     = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_Resp        = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_CBusy       = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_WayValid    = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_Way         = 0; \
    uint8_t  cchi_t1p##P##_txrsp_bits_TraceTag    = 0; \
    /* rxrsp (harness -> DUT) */ \
    uint8_t  cchi_t1p##P##_rxrsp_valid            = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_bits_TxnID       = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_bits_SrcID       = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_bits_TgtID       = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_bits_Opcode      = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_bits_RespErr     = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_bits_Resp        = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_bits_TraceTag    = 0; \
    uint8_t  cchi_t1p##P##_rxrsp_ready            = 0; \
    /* txdat (DUT -> harness) */ \
    uint8_t  cchi_t1p##P##_txdat_ready            = 0; \
    uint8_t  cchi_t1p##P##_txdat_valid            = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_TxnID       = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_SrcID       = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_TgtID       = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_DBID        = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_Opcode      = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_RespErr     = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_Resp        = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_DataSource  = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_CBusy       = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_DataID      = 0; \
    uint32_t cchi_t1p##P##_txdat_bits_Data[8]     = {}; \
    uint8_t  cchi_t1p##P##_txdat_bits_WayValid    = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_Way         = 0; \
    uint8_t  cchi_t1p##P##_txdat_bits_TraceTag    = 0; \
    /* rxdat (harness -> DUT) */ \
    uint8_t  cchi_t1p##P##_rxdat_valid            = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_TxnID       = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_SrcID       = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_TgtID       = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_Opcode      = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_RespErr     = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_Resp        = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_DataID      = 0; \
    uint32_t cchi_t1p##P##_rxdat_bits_Data[8]     = {}; \
    uint32_t cchi_t1p##P##_rxdat_bits_BE          = 0; \
    uint8_t  cchi_t1p##P##_rxdat_bits_TraceTag    = 0; \
    uint8_t  cchi_t1p##P##_rxdat_ready            = 0

// Members of one AXI memory port axi_m<P> (the mesh SBSX master drives
// AW/W/AR + B/R readys; the harness memory model drives the rest).
#define COHESTRA_VCS_AXI_M_MEMBERS(P) \
    uint8_t  axi_m##P##_awvalid = 0; \
    uint8_t  axi_m##P##_awready = 0; \
    uint32_t axi_m##P##_awid    = 0; \
    uint64_t axi_m##P##_awaddr  = 0; \
    uint8_t  axi_m##P##_awlen   = 0; \
    uint8_t  axi_m##P##_awsize  = 0; \
    uint8_t  axi_m##P##_awburst = 0; \
    uint8_t  axi_m##P##_wvalid  = 0; \
    uint8_t  axi_m##P##_wready  = 0; \
    uint32_t axi_m##P##_wdata[8] = {}; \
    uint32_t axi_m##P##_wstrb   = 0; \
    uint8_t  axi_m##P##_wlast   = 0; \
    uint8_t  axi_m##P##_bvalid  = 0; \
    uint8_t  axi_m##P##_bready  = 0; \
    uint32_t axi_m##P##_bid     = 0; \
    uint8_t  axi_m##P##_bresp   = 0; \
    uint8_t  axi_m##P##_arvalid = 0; \
    uint8_t  axi_m##P##_arready = 0; \
    uint32_t axi_m##P##_arid    = 0; \
    uint64_t axi_m##P##_araddr  = 0; \
    uint8_t  axi_m##P##_arlen   = 0; \
    uint8_t  axi_m##P##_arsize  = 0; \
    uint8_t  axi_m##P##_arburst = 0; \
    uint8_t  axi_m##P##_rvalid  = 0; \
    uint8_t  axi_m##P##_rready  = 0; \
    uint32_t axi_m##P##_rid     = 0; \
    uint32_t axi_m##P##_rdata[8] = {}; \
    uint8_t  axi_m##P##_rresp   = 0; \
    uint8_t  axi_m##P##_rlast   = 0

// Members of one AXI config port axi_s<P> (the harness master model drives
// AW/W/AR + B/R readys; the mesh config slave drives the rest).
#define COHESTRA_VCS_AXI_S_MEMBERS(P) \
    uint8_t  axi_s##P##_awvalid = 0; \
    uint8_t  axi_s##P##_awready = 0; \
    uint32_t axi_s##P##_awid    = 0; \
    uint64_t axi_s##P##_awaddr  = 0; \
    uint8_t  axi_s##P##_awlen   = 0; \
    uint8_t  axi_s##P##_awsize  = 0; \
    uint8_t  axi_s##P##_awburst = 0; \
    uint8_t  axi_s##P##_wvalid  = 0; \
    uint8_t  axi_s##P##_wready  = 0; \
    uint32_t axi_s##P##_wdata[8] = {}; \
    uint32_t axi_s##P##_wstrb   = 0; \
    uint8_t  axi_s##P##_wlast   = 0; \
    uint8_t  axi_s##P##_bvalid  = 0; \
    uint8_t  axi_s##P##_bready  = 0; \
    uint32_t axi_s##P##_bid     = 0; \
    uint8_t  axi_s##P##_bresp   = 0; \
    uint8_t  axi_s##P##_arvalid = 0; \
    uint8_t  axi_s##P##_arready = 0; \
    uint32_t axi_s##P##_arid    = 0; \
    uint64_t axi_s##P##_araddr  = 0; \
    uint8_t  axi_s##P##_arlen   = 0; \
    uint8_t  axi_s##P##_arsize  = 0; \
    uint8_t  axi_s##P##_arburst = 0; \
    uint8_t  axi_s##P##_rvalid  = 0; \
    uint8_t  axi_s##P##_rready  = 0; \
    uint32_t axi_s##P##_rid     = 0; \
    uint32_t axi_s##P##_rdata[8] = {}; \
    uint8_t  axi_s##P##_rresp   = 0; \
    uint8_t  axi_s##P##_rlast   = 0


struct CohestraVCSTop {

    // Clock/reset: driven by the harness like any other pin. `clock` selects
    // the eval() phase (0 = drive-low + settle + sample, 1 = posedge).
    uint8_t clock = 0;
    uint8_t reset = 0;

    // 4x CCHI Type-1 ports (to the harness Taurus upstream nodes)
    COHESTRA_VCS_T1_MEMBERS(0);
    COHESTRA_VCS_T1_MEMBERS(1);
    COHESTRA_VCS_T1_MEMBERS(2);
    COHESTRA_VCS_T1_MEMBERS(3);

    // 2x AXI4 memory ports (from the mesh SBSX masters, to the harness
    // memory models)
    COHESTRA_VCS_AXI_M_MEMBERS(0);
    COHESTRA_VCS_AXI_M_MEMBERS(1);

    // 1x AXI4 config port (from the harness master / boot sequencer, to the
    // mesh configuration ingress)
    COHESTRA_VCS_AXI_S_MEMBERS(0);

    // Verilator-model API parity: the harness loop calls eval() after every
    // clock write; final() runs at teardown. See the header comment.
    void eval()  noexcept { Cohestra::VCS::EvalTop(*this); }
    void final() noexcept { }
};

#undef COHESTRA_VCS_T1_MEMBERS
#undef COHESTRA_VCS_AXI_M_MEMBERS
#undef COHESTRA_VCS_AXI_S_MEMBERS


//*****************************************************************************
// Compile-time proof that this shadow satisfies the cohestra_v3 pin traits of
// the avocado_top boundary (and nothing more).
//*****************************************************************************
static_assert(Cohestra::details::V3CCHIModuleT1P0<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P1<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P2<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P3<CohestraVCSTop>);
static_assert(!Cohestra::details::V3CCHIModuleT1P4<CohestraVCSTop>);

// The contract carries the WayValid/Way user bits on rxevt/rxreq/txrsp/txdat,
// so both UW feature concepts must detect (matching the Verilated avocado_top).
static_assert(Cohestra::details::V3CCHIModuleT1P0UWPredict<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P1UWPredict<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P2UWPredict<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P3UWPredict<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P0UWPersist<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P1UWPersist<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P2UWPersist<CohestraVCSTop>);
static_assert(Cohestra::details::V3CCHIModuleT1P3UWPersist<CohestraVCSTop>);

// axi_m0/axi_m1 are AXI memory (harness-slave) ports, 256-bit data.
static_assert(Cohestra::details::V3AXIMemoryModulePort0<CohestraVCSTop>);
static_assert(Cohestra::details::V3AXIMemoryModulePort1<CohestraVCSTop>);
static_assert(!Cohestra::details::V3AXIMemoryModulePort2<CohestraVCSTop>);
static_assert(Cohestra::details::V3AXIMemoryPortTraits<CohestraVCSTop, 0>::DataBits == 256);
static_assert(Cohestra::details::V3AXIMemoryPortTraits<CohestraVCSTop, 1>::DataBits == 256);

// axi_s0 is the AXI master (harness-master) port, 256-bit data.
static_assert(Cohestra::details::V3AXIMasterModulePort0<CohestraVCSTop>);
static_assert(!Cohestra::details::V3AXIMasterModulePort1<CohestraVCSTop>);
static_assert(Cohestra::details::V3AXIMasterPortTraits<CohestraVCSTop, 0>::DataBits == 256);

// No AXI monitor taps and no CHI pins at the avocado_top boundary.
static_assert(!Cohestra::details::V3AXIMonitorModulePort0<CohestraVCSTop>);
