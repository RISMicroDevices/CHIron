//*****************************************************************************
// l2openllc_vcs_top.sv — Cohestra VCS top module binding XSCache
// TestTop_L2OpenLLC (NUM_L2=4 Oceanus L2 -> OpenLLC -> OpenNCB -> AXI4) to
// the cohestra_vcs bridge boundary. Drop-in replacement for avocado_top on
// DUTs that carry no Avocado mesh: the bridge (cohestra_vcs_bridge.sv) and
// the C++ shadow top (vcs_top.hpp) are used UNCHANGED; this module alone
// absorbs every pin-contract delta. Select it with
// +define+COHESTRA_VCS_TOP_MODULE=l2openllc_vcs_top (build.sh knob of the
// same name) and pass the avocado integration dir as +incdir (for
// avocado_dut_if.svh).
//
// Boundary (identical to avocado_top's): clock, reset, 4x Type-1 CCHI ports
// cchi_t1p0..3_* (contract widths), 2x AXI4 memory ports axi_m0/axi_m1
// (harness is slave/memory), 1x AXI4 config port axi_s0 (harness is master).
//
// DUT wiring:
//   * cchi_t1p0..3  -> the four L2s' upstream Type-1 ports (Taurus upstream
//     nodes bind here; this DUT generation has no Type-4 upstream nodes
//     configured, so the harness only ever exercises these four).
//   * axi_m0        -> the OpenLLC/OpenNCB AXI4 memory master (the harness
//     memory model serves the CHI downstream traffic through it).
//   * axi_m1        -> NOTHING (the DUT has a single memory master). Outputs
//     left open, inputs ignored: the harness samples them as idle (the
//     bridge collapses undriven Z to 0 on the DPI crossing, the same
//     semantics the Verilator flow's zero-initialized pins have).
//   * axi_s0        -> NOTHING (no mesh config fabric here; without a [boot]
//     sequencer the harness master model never drives it). Inputs ignored,
//     outputs tied to 0.
//   * cchi_t4p0..7  -> the L2s' Type-4 upstream port pairs (rxreq in, txdat
//     out), unused in this integration: all rxreq inputs tied to 0 (valid=0
//     parks the channel), txdat outputs left open, and txdat_ready TIED TO 0
//     rather than left open — a floating ready would be Z/X in this 4-state
//     build, while the green Verilator reference effectively holds it at 0
//     (2-state, never driven). Tying reproduces the reference exactly and
//     keeps X out of the L2 DnDAT arbitration cones.
//   * log_dump / log_clean -> the only other DUT inputs besides clock/reset
//     (logging knobs), tied to 0 (same as scripts/avocado/avocado_dut.sv).
//
// CCHI shape deltas adapted per T1 port (same list as the Avocado shell
// scripts/avocado/avocado_dut.sv — TestTop_L2OpenLLC emits the same pin
// shapes as TestTop_L2Avocado). All adapted values provably fit the narrower
// side (the harness drives NIDs 0..3; the L2 allocates DBIDs/Ways/aliases
// from its small internal pools):
//
//   - Data:   contract pins are unpacked arrays of 8x 32-bit words; the DUT
//             emits flat [255:0] pins. Word w maps to bits [w*32 +: 32],
//             matching the harness's little-endian word (VlWide) order.
//   - TxnID:  DUT 8 bits, contract 7 (7 channels).
//   - DBID:   DUT 8 bits, contract 7 (txrsp/txdat).
//   - SrcID/TgtID: DUT 4 bits, contract 5 (all 7 channels).
//   - Way:    DUT 2 bits, contract 4 (rxevt/rxreq/txrsp/txdat).
//   - TagAlias: DUT 2 bits, contract 8 (rxreq).
//   - DataID: DUT 2 bits, contract 1 (rxdat/txdat; with 256-bit data a 64B
//             line is exactly 2 beats, so only DataID[0] is ever used).
//   - BE:     rxdat only, [31:0] on both sides — 1:1 via .* (no adaptation).
//
// AXI shape deltas on axi_m0 (DUT OpenNCB master vs contract memory port):
//   - ID:     DUT 4 bits (awid/bid/arid/rid), contract 24. DUT->contract
//             zero-extends; contract->DUT truncates (lossless: the memory
//             model echoes the IDs the DUT issued).
//   - Addr:   DUT 32 bits (awaddr/araddr), contract 48. DUT->contract
//             zero-extends (the configured memory window sits below 4 GiB).
//   Data/wstrb/len/size/burst/resp/valid/ready/last match 1:1.
//
// RX (harness->DUT) adapters truncate; TX (DUT->harness) adapters zero-extend
// (TxnID/DBID TX are the exceptions: DUT 8 -> contract 7 truncates, values
// provably < 128). VCS rejects wildcard `.*` connections with width
// mismatches, so every adapted pin is connected explicitly and `.*` covers
// only the rest (same idiom as avocado_dut.sv).
//*****************************************************************************

`include "avocado_dut_if.svh"

module l2openllc_vcs_top (
    input  logic        clock,
    input  logic        reset,

    // 4x Type-1 CCHI ports (to the harness Taurus upstream nodes)
    `AVOCADO_CCHI_T1_PORT_DECL(0),
    `AVOCADO_CCHI_T1_PORT_DECL(1),
    `AVOCADO_CCHI_T1_PORT_DECL(2),
    `AVOCADO_CCHI_T1_PORT_DECL(3),

    // 2x AXI4 memory ports (to the harness memory models; only axi_m0 is
    // backed by the DUT — see the header)
    `AVOCADO_AXI_M_PORT_DECL(0),
    `AVOCADO_AXI_M_PORT_DECL(1),

    // 1x AXI4 config port (from the harness master / boot sequencer; unused)
    `AVOCADO_AXI_S_PORT_DECL(0)
);

    // ------------------------------------------------------------------
    // Shape adapters per T1 port (see header). Wire suffix _q4/_q2/_q1-style
    // locals carry the DUT-side (narrower) values; _flat the flat 256-bit
    // data. Identical adaptation set to scripts/avocado/avocado_dut.sv.
    // ------------------------------------------------------------------
`define L2OPENLLC_T1_ADAPT(P) \
    logic [255:0] txdat_data_flat_``P; \
    logic [255:0] rxdat_data_flat_``P; \
    logic [1:0]   txdat_dataid_``P; \
    logic [1:0]   rxdat_dataid_``P; \
    logic [7:0]   rxevt_txnid_``P, rxreq_txnid_``P, rxrsp_txnid_``P, rxdat_txnid_``P; \
    logic [7:0]   txsnp_txnid_``P, txrsp_txnid_``P, txdat_txnid_``P; \
    logic [7:0]   txrsp_dbid_``P, txdat_dbid_``P; \
    logic [3:0]   rxevt_srcid_``P, rxevt_tgtid_``P, rxreq_srcid_``P, rxreq_tgtid_``P; \
    logic [3:0]   rxrsp_srcid_``P, rxrsp_tgtid_``P, rxdat_srcid_``P, rxdat_tgtid_``P; \
    logic [3:0]   txsnp_srcid_``P, txsnp_tgtid_``P, txrsp_srcid_``P, txrsp_tgtid_``P; \
    logic [3:0]   txdat_srcid_``P, txdat_tgtid_``P; \
    logic [1:0]   rxevt_way_``P, rxreq_way_``P, txrsp_way_``P, txdat_way_``P; \
    logic [1:0]   rxreq_tagalias_``P; \
    for (genvar w = 0; w < 8; w++) begin : g_data_adapt_``P \
        assign cchi_t1p``P``_txdat_bits_Data[w] = txdat_data_flat_``P``[w*32 +: 32]; \
        assign rxdat_data_flat_``P``[w*32 +: 32] = cchi_t1p``P``_rxdat_bits_Data[w]; \
    end \
    /* TX (DUT->contract): zero-extend (TxnID/DBID truncate 8->7) */ \
    assign cchi_t1p``P``_txsnp_bits_TxnID  = txsnp_txnid_``P``[6:0]; \
    assign cchi_t1p``P``_txrsp_bits_TxnID  = txrsp_txnid_``P``[6:0]; \
    assign cchi_t1p``P``_txdat_bits_TxnID  = txdat_txnid_``P``[6:0]; \
    assign cchi_t1p``P``_txrsp_bits_DBID   = txrsp_dbid_``P``[6:0]; \
    assign cchi_t1p``P``_txdat_bits_DBID   = txdat_dbid_``P``[6:0]; \
    assign cchi_t1p``P``_txsnp_bits_SrcID  = {1'b0, txsnp_srcid_``P}; \
    assign cchi_t1p``P``_txsnp_bits_TgtID  = {1'b0, txsnp_tgtid_``P}; \
    assign cchi_t1p``P``_txrsp_bits_SrcID  = {1'b0, txrsp_srcid_``P}; \
    assign cchi_t1p``P``_txrsp_bits_TgtID  = {1'b0, txrsp_tgtid_``P}; \
    assign cchi_t1p``P``_txdat_bits_SrcID  = {1'b0, txdat_srcid_``P}; \
    assign cchi_t1p``P``_txdat_bits_TgtID  = {1'b0, txdat_tgtid_``P}; \
    assign cchi_t1p``P``_txrsp_bits_Way    = {2'b00, txrsp_way_``P}; \
    assign cchi_t1p``P``_txdat_bits_Way    = {2'b00, txdat_way_``P}; \
    assign cchi_t1p``P``_txdat_bits_DataID = txdat_dataid_``P``[0]; \
    /* RX (contract->DUT): truncate (values provably fit) */ \
    assign rxevt_txnid_``P  = cchi_t1p``P``_rxevt_bits_TxnID; \
    assign rxreq_txnid_``P  = cchi_t1p``P``_rxreq_bits_TxnID; \
    assign rxrsp_txnid_``P  = cchi_t1p``P``_rxrsp_bits_TxnID; \
    assign rxdat_txnid_``P  = cchi_t1p``P``_rxdat_bits_TxnID; \
    assign rxevt_srcid_``P  = cchi_t1p``P``_rxevt_bits_SrcID[3:0]; \
    assign rxevt_tgtid_``P  = cchi_t1p``P``_rxevt_bits_TgtID[3:0]; \
    assign rxreq_srcid_``P  = cchi_t1p``P``_rxreq_bits_SrcID[3:0]; \
    assign rxreq_tgtid_``P  = cchi_t1p``P``_rxreq_bits_TgtID[3:0]; \
    assign rxrsp_srcid_``P  = cchi_t1p``P``_rxrsp_bits_SrcID[3:0]; \
    assign rxrsp_tgtid_``P  = cchi_t1p``P``_rxrsp_bits_TgtID[3:0]; \
    assign rxdat_srcid_``P  = cchi_t1p``P``_rxdat_bits_SrcID[3:0]; \
    assign rxdat_tgtid_``P  = cchi_t1p``P``_rxdat_bits_TgtID[3:0]; \
    assign rxevt_way_``P    = cchi_t1p``P``_rxevt_bits_Way[1:0]; \
    assign rxreq_way_``P    = cchi_t1p``P``_rxreq_bits_Way[1:0]; \
    assign rxreq_tagalias_``P = cchi_t1p``P``_rxreq_bits_TagAlias[1:0]; \
    assign rxdat_dataid_``P = {1'b0, cchi_t1p``P``_rxdat_bits_DataID};

    `L2OPENLLC_T1_ADAPT(0)
    `L2OPENLLC_T1_ADAPT(1)
    `L2OPENLLC_T1_ADAPT(2)
    `L2OPENLLC_T1_ADAPT(3)

`undef L2OPENLLC_T1_ADAPT

    // ------------------------------------------------------------------
    // axi_m0 adapter: DUT (OpenNCB master, 4-bit ID / 32-bit addr) <->
    // contract memory port (24-bit ID / 48-bit addr). TX zero-extends; RX
    // truncates (lossless: the memory model echoes DUT-issued IDs, and the
    // configured window sits below 4 GiB).
    // ------------------------------------------------------------------
    logic [3:0]  axi_m0_dut_awid,  axi_m0_dut_bid,  axi_m0_dut_arid,  axi_m0_dut_rid;
    logic [31:0] axi_m0_dut_awaddr, axi_m0_dut_araddr;

    /* TX (DUT->contract): zero-extend */
    assign axi_m0_awid   = {20'b0, axi_m0_dut_awid};
    assign axi_m0_awaddr = {16'b0, axi_m0_dut_awaddr};
    assign axi_m0_arid   = {20'b0, axi_m0_dut_arid};
    assign axi_m0_araddr = {16'b0, axi_m0_dut_araddr};
    /* RX (contract->DUT): truncate (values provably fit) */
    assign axi_m0_dut_bid = axi_m0_bid[3:0];
    assign axi_m0_dut_rid = axi_m0_rid[3:0];

    // ------------------------------------------------------------------
    // DUT: 4x Oceanus L2 -> OpenLLC -> OpenNCB -> AXI4 (XSCache
    // TestTop_L2OpenLLC). All unadapted t1p pins connect 1:1 by name via .*;
    // the snoop alias outputs (outside the contract) are left open; the
    // unused Type-4 ports are parked (see header).
    // ------------------------------------------------------------------
`define L2OPENLLC_T1_PORT_ADAPT(P) \
    .cchi_t1p``P``_txdat_bits_Data   (txdat_data_flat_``P), \
    .cchi_t1p``P``_rxdat_bits_Data   (rxdat_data_flat_``P), \
    .cchi_t1p``P``_rxevt_bits_TxnID  (rxevt_txnid_``P), \
    .cchi_t1p``P``_rxreq_bits_TxnID  (rxreq_txnid_``P), \
    .cchi_t1p``P``_rxrsp_bits_TxnID  (rxrsp_txnid_``P), \
    .cchi_t1p``P``_rxdat_bits_TxnID  (rxdat_txnid_``P), \
    .cchi_t1p``P``_txsnp_bits_TxnID  (txsnp_txnid_``P), \
    .cchi_t1p``P``_txrsp_bits_TxnID  (txrsp_txnid_``P), \
    .cchi_t1p``P``_txdat_bits_TxnID  (txdat_txnid_``P), \
    .cchi_t1p``P``_txrsp_bits_DBID   (txrsp_dbid_``P), \
    .cchi_t1p``P``_txdat_bits_DBID   (txdat_dbid_``P), \
    .cchi_t1p``P``_rxevt_bits_SrcID  (rxevt_srcid_``P), \
    .cchi_t1p``P``_rxevt_bits_TgtID  (rxevt_tgtid_``P), \
    .cchi_t1p``P``_rxreq_bits_SrcID  (rxreq_srcid_``P), \
    .cchi_t1p``P``_rxreq_bits_TgtID  (rxreq_tgtid_``P), \
    .cchi_t1p``P``_rxrsp_bits_SrcID  (rxrsp_srcid_``P), \
    .cchi_t1p``P``_rxrsp_bits_TgtID  (rxrsp_tgtid_``P), \
    .cchi_t1p``P``_rxdat_bits_SrcID  (rxdat_srcid_``P), \
    .cchi_t1p``P``_rxdat_bits_TgtID  (rxdat_tgtid_``P), \
    .cchi_t1p``P``_txsnp_bits_SrcID  (txsnp_srcid_``P), \
    .cchi_t1p``P``_txsnp_bits_TgtID  (txsnp_tgtid_``P), \
    .cchi_t1p``P``_txrsp_bits_SrcID  (txrsp_srcid_``P), \
    .cchi_t1p``P``_txrsp_bits_TgtID  (txrsp_tgtid_``P), \
    .cchi_t1p``P``_txdat_bits_SrcID  (txdat_srcid_``P), \
    .cchi_t1p``P``_txdat_bits_TgtID  (txdat_tgtid_``P), \
    .cchi_t1p``P``_rxevt_bits_Way    (rxevt_way_``P), \
    .cchi_t1p``P``_rxreq_bits_Way    (rxreq_way_``P), \
    .cchi_t1p``P``_txrsp_bits_Way    (txrsp_way_``P), \
    .cchi_t1p``P``_txdat_bits_Way    (txdat_way_``P), \
    .cchi_t1p``P``_rxreq_bits_TagAlias (rxreq_tagalias_``P), \
    .cchi_t1p``P``_txdat_bits_DataID (txdat_dataid_``P), \
    .cchi_t1p``P``_rxdat_bits_DataID (rxdat_dataid_``P), \
    .cchi_t1p``P``_txsnp_bits_alias  (),

    // Unused Type-4 port pair P: rxreq inputs tied 0 (valid=0 parks the
    // channel), txdat outputs open, txdat_ready tied 0 (NOT left open — a
    // floating input would be Z/X in 4-state while the Verilator reference
    // holds it at 0; see the header).
`define L2OPENLLC_T4P_PARK(P) \
    .cchi_t4p``P``_rxreq_valid         (1'b0), \
    .cchi_t4p``P``_rxreq_ready         (), \
    .cchi_t4p``P``_rxreq_bits_Addr     ('0), \
    .cchi_t4p``P``_rxreq_bits_TraceTag ('0), \
    .cchi_t4p``P``_rxreq_bits_Way      ('0), \
    .cchi_t4p``P``_rxreq_bits_WayValid ('0), \
    .cchi_t4p``P``_rxreq_bits_ExpCompData ('0), \
    .cchi_t4p``P``_rxreq_bits_Excl     ('0), \
    .cchi_t4p``P``_rxreq_bits_MemAttr  ('0), \
    .cchi_t4p``P``_rxreq_bits_Order    ('0), \
    .cchi_t4p``P``_rxreq_bits_NS       ('0), \
    .cchi_t4p``P``_rxreq_bits_Size     ('0), \
    .cchi_t4p``P``_rxreq_bits_TagAlias ('0), \
    .cchi_t4p``P``_rxreq_bits_Opcode   ('0), \
    .cchi_t4p``P``_rxreq_bits_TgtID    ('0), \
    .cchi_t4p``P``_rxreq_bits_SrcID    ('0), \
    .cchi_t4p``P``_rxreq_bits_TxnID    ('0), \
    .cchi_t4p``P``_txdat_valid         (), \
    .cchi_t4p``P``_txdat_ready         (1'b0), \
    .cchi_t4p``P``_txdat_bits_Data     (), \
    .cchi_t4p``P``_txdat_bits_TraceTag (), \
    .cchi_t4p``P``_txdat_bits_Way      (), \
    .cchi_t4p``P``_txdat_bits_WayValid (), \
    .cchi_t4p``P``_txdat_bits_CBusy    (), \
    .cchi_t4p``P``_txdat_bits_DataSource (), \
    .cchi_t4p``P``_txdat_bits_DataID   (), \
    .cchi_t4p``P``_txdat_bits_Resp     (), \
    .cchi_t4p``P``_txdat_bits_RespErr  (), \
    .cchi_t4p``P``_txdat_bits_Opcode   (), \
    .cchi_t4p``P``_txdat_bits_DBID     (), \
    .cchi_t4p``P``_txdat_bits_TgtID    (), \
    .cchi_t4p``P``_txdat_bits_SrcID    (), \
    .cchi_t4p``P``_txdat_bits_TxnID    (),

    TestTop_L2OpenLLC u_dut (
        `L2OPENLLC_T1_PORT_ADAPT(0)
        `L2OPENLLC_T1_PORT_ADAPT(1)
        `L2OPENLLC_T1_PORT_ADAPT(2)
        `L2OPENLLC_T1_PORT_ADAPT(3)

        `L2OPENLLC_T4P_PARK(0)
        `L2OPENLLC_T4P_PARK(1)
        `L2OPENLLC_T4P_PARK(2)
        `L2OPENLLC_T4P_PARK(3)
        `L2OPENLLC_T4P_PARK(4)
        `L2OPENLLC_T4P_PARK(5)
        `L2OPENLLC_T4P_PARK(6)
        `L2OPENLLC_T4P_PARK(7)

        // axi_m0: explicit (width-adapted; `.*` would mismatch)
        .axi_m0_awvalid (axi_m0_awvalid),
        .axi_m0_awready (axi_m0_awready),
        .axi_m0_awid    (axi_m0_dut_awid),
        .axi_m0_awaddr  (axi_m0_dut_awaddr),
        .axi_m0_awlen   (axi_m0_awlen),
        .axi_m0_awsize  (axi_m0_awsize),
        .axi_m0_awburst (axi_m0_awburst),
        .axi_m0_wvalid  (axi_m0_wvalid),
        .axi_m0_wready  (axi_m0_wready),
        .axi_m0_wdata   (axi_m0_wdata),
        .axi_m0_wstrb   (axi_m0_wstrb),
        .axi_m0_wlast   (axi_m0_wlast),
        .axi_m0_bvalid  (axi_m0_bvalid),
        .axi_m0_bready  (axi_m0_bready),
        .axi_m0_bid     (axi_m0_dut_bid),
        .axi_m0_bresp   (axi_m0_bresp),
        .axi_m0_arvalid (axi_m0_arvalid),
        .axi_m0_arready (axi_m0_arready),
        .axi_m0_arid    (axi_m0_dut_arid),
        .axi_m0_araddr  (axi_m0_dut_araddr),
        .axi_m0_arlen   (axi_m0_arlen),
        .axi_m0_arsize  (axi_m0_arsize),
        .axi_m0_arburst (axi_m0_arburst),
        .axi_m0_rvalid  (axi_m0_rvalid),
        .axi_m0_rready  (axi_m0_rready),
        .axi_m0_rid     (axi_m0_dut_rid),
        .axi_m0_rdata   (axi_m0_rdata),
        .axi_m0_rresp   (axi_m0_rresp),
        .axi_m0_rlast   (axi_m0_rlast),

        // logging knobs (the only DUT inputs besides clock/reset/cchi/axi —
        // same tie-off as scripts/avocado/avocado_dut.sv)
        .log_dump       (1'b0),
        .log_clean      (1'b0),
        .*
    );

`undef L2OPENLLC_T1_PORT_ADAPT
`undef L2OPENLLC_T4P_PARK

    // ------------------------------------------------------------------
    // Contract ports with no backing hardware on this DUT (see header):
    //   axi_m1 (second memory port): outputs undriven (bridge samples the
    //     undriven nets as 0 across the DPI crossing — idle), inputs ignored.
    //   axi_s0 (config port): inputs ignored, outputs tied to 0 (there is no
    //     mesh config fabric and no boot sequencer drives it here).
    // ------------------------------------------------------------------
    assign axi_s0_awready = 1'b0;
    assign axi_s0_wready  = 1'b0;
    assign axi_s0_bvalid  = 1'b0;
    assign axi_s0_bid     = '0;
    assign axi_s0_bresp   = '0;
    assign axi_s0_arready = 1'b0;
    assign axi_s0_rvalid  = 1'b0;
    assign axi_s0_rid     = '0;
    assign axi_s0_rdata   = '0;
    assign axi_s0_rresp   = '0;
    assign axi_s0_rlast   = 1'b0;

endmodule
