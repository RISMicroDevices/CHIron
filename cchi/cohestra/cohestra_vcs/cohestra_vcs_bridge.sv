//*****************************************************************************
// cohestra_vcs_bridge.sv — SystemVerilog bridge between the VCS-simulated DUT
// (default: avocado_top — override with +define+COHESTRA_VCS_TOP_MODULE=<name>)
// and the C++ Cohestra harness over DPI.
//
// There is NO free-running clock here: the C++ harness owns the clock. The
// initial block below parses the plusargs, checks the DPI struct layout, and
// calls the imported task CohestraVCS_Main() exactly once; the whole
// simulation runs inside that call. Time advances only inside the exported
// tasks the C++ side calls per half cycle:
//
//   CohestraVCS_DriveLow(drv)  — blocking-assign all driven pins from the
//                                to-DUT struct, clock=0, then #HP to let the
//                                DUT settle through the low phase
//                                (Verilator flow: "eval with clock=0")
//   CohestraVCS_Sample(out_s)  — zero-time read of the settled pins into the
//                                from-DUT struct (called right after DriveLow,
//                                and again right after DriveHigh so that the
//                                shadow pins end each cycle at the post-posedge
//                                state — matching the Verilator flow, where a
//                                clock=1 eval leaves the pins post-posedge)
//   CohestraVCS_DriveHigh()    — clock=1, #HP (the posedge propagates)
//                                (Verilator flow: "eval with clock=1")
//
// Timescale: 1ps/1ps, half period HP=500 → 1ns clock (1GHz). One harness time
// unit (one tick) = 500ps of simulator time; 2 ticks = 1 cycle, matching the
// cohestra_v3 Verilator flow's time bookkeeping exactly. waveform.start (in
// harness time units) maps to start*500 ps.
//
// Plusargs (all optional):
//   +cohestra_config=<path>     INI configuration file ("" = run on defaults)
//   +cohestra_cycles=<n>        harness-time budget (2 units per cycle;
//                               default 200000, 0 = unlimited)
//   +cohestra_vcd=<path>        dump VCD (when the INI does not enable one)
//   +cohestra_fsdb=<path>       dump FSDB (requires a VERDI_HOME build)
//   +cohestra_log_file=<path>   harness log file (default: auto-named)
//   +cohestra_no_log_file       console-only logging
//
// The pin names/widths at the avocado_top boundary come from
// cohestra_packages/avocado/avocado_top.sv and the contract header
// cohestra_packages/avocado/integration/avocado_dut_if.svh.
//*****************************************************************************

`timescale 1ps/1ps
`default_nettype none

// -----------------------------------------------------------------------------
// Boundary signal declarations: one CCHI Type-1 port (contract widths).
// -----------------------------------------------------------------------------
`define COHESTRA_VCS_CCHI_T1_SIGS(P) \
    logic        cchi_t1p``P``_rxevt_valid; \
    logic [6:0]  cchi_t1p``P``_rxevt_bits_TxnID; \
    logic [4:0]  cchi_t1p``P``_rxevt_bits_SrcID; \
    logic [4:0]  cchi_t1p``P``_rxevt_bits_TgtID; \
    logic        cchi_t1p``P``_rxevt_bits_Opcode; \
    logic [47:0] cchi_t1p``P``_rxevt_bits_Addr; \
    logic        cchi_t1p``P``_rxevt_bits_NS; \
    logic        cchi_t1p``P``_rxevt_bits_MemAttr; \
    logic        cchi_t1p``P``_rxevt_bits_WayValid; \
    logic [3:0]  cchi_t1p``P``_rxevt_bits_Way; \
    logic        cchi_t1p``P``_rxevt_bits_TraceTag; \
    logic        cchi_t1p``P``_rxevt_ready; \
    \
    logic        cchi_t1p``P``_rxreq_valid; \
    logic [6:0]  cchi_t1p``P``_rxreq_bits_TxnID; \
    logic [4:0]  cchi_t1p``P``_rxreq_bits_SrcID; \
    logic [4:0]  cchi_t1p``P``_rxreq_bits_TgtID; \
    logic [5:0]  cchi_t1p``P``_rxreq_bits_Opcode; \
    logic [2:0]  cchi_t1p``P``_rxreq_bits_Size; \
    logic [47:0] cchi_t1p``P``_rxreq_bits_Addr; \
    logic [7:0]  cchi_t1p``P``_rxreq_bits_TagAlias; \
    logic        cchi_t1p``P``_rxreq_bits_NS; \
    logic [1:0]  cchi_t1p``P``_rxreq_bits_Order; \
    logic [3:0]  cchi_t1p``P``_rxreq_bits_MemAttr; \
    logic        cchi_t1p``P``_rxreq_bits_Excl; \
    logic        cchi_t1p``P``_rxreq_bits_ExpCompData; \
    logic        cchi_t1p``P``_rxreq_bits_TraceTag; \
    logic        cchi_t1p``P``_rxreq_bits_WayValid; \
    logic [3:0]  cchi_t1p``P``_rxreq_bits_Way; \
    logic        cchi_t1p``P``_rxreq_ready; \
    \
    logic        cchi_t1p``P``_txsnp_ready; \
    logic        cchi_t1p``P``_txsnp_valid; \
    logic [6:0]  cchi_t1p``P``_txsnp_bits_TxnID; \
    logic [4:0]  cchi_t1p``P``_txsnp_bits_SrcID; \
    logic [4:0]  cchi_t1p``P``_txsnp_bits_TgtID; \
    logic [1:0]  cchi_t1p``P``_txsnp_bits_Opcode; \
    logic [44:0] cchi_t1p``P``_txsnp_bits_Addr; \
    logic        cchi_t1p``P``_txsnp_bits_NS; \
    logic        cchi_t1p``P``_txsnp_bits_TraceTag; \
    \
    logic        cchi_t1p``P``_txrsp_ready; \
    logic        cchi_t1p``P``_txrsp_valid; \
    logic [6:0]  cchi_t1p``P``_txrsp_bits_TxnID; \
    logic [4:0]  cchi_t1p``P``_txrsp_bits_SrcID; \
    logic [4:0]  cchi_t1p``P``_txrsp_bits_TgtID; \
    logic [6:0]  cchi_t1p``P``_txrsp_bits_DBID; \
    logic [2:0]  cchi_t1p``P``_txrsp_bits_Opcode; \
    logic [1:0]  cchi_t1p``P``_txrsp_bits_RespErr; \
    logic [2:0]  cchi_t1p``P``_txrsp_bits_Resp; \
    logic [2:0]  cchi_t1p``P``_txrsp_bits_CBusy; \
    logic        cchi_t1p``P``_txrsp_bits_WayValid; \
    logic [3:0]  cchi_t1p``P``_txrsp_bits_Way; \
    logic        cchi_t1p``P``_txrsp_bits_TraceTag; \
    \
    logic        cchi_t1p``P``_rxrsp_valid; \
    logic [6:0]  cchi_t1p``P``_rxrsp_bits_TxnID; \
    logic [4:0]  cchi_t1p``P``_rxrsp_bits_SrcID; \
    logic [4:0]  cchi_t1p``P``_rxrsp_bits_TgtID; \
    logic        cchi_t1p``P``_rxrsp_bits_Opcode; \
    logic [1:0]  cchi_t1p``P``_rxrsp_bits_RespErr; \
    logic [2:0]  cchi_t1p``P``_rxrsp_bits_Resp; \
    logic        cchi_t1p``P``_rxrsp_bits_TraceTag; \
    logic        cchi_t1p``P``_rxrsp_ready; \
    \
    logic        cchi_t1p``P``_txdat_ready; \
    logic        cchi_t1p``P``_txdat_valid; \
    logic [6:0]  cchi_t1p``P``_txdat_bits_TxnID; \
    logic [4:0]  cchi_t1p``P``_txdat_bits_SrcID; \
    logic [4:0]  cchi_t1p``P``_txdat_bits_TgtID; \
    logic [6:0]  cchi_t1p``P``_txdat_bits_DBID; \
    logic        cchi_t1p``P``_txdat_bits_Opcode; \
    logic [1:0]  cchi_t1p``P``_txdat_bits_RespErr; \
    logic [2:0]  cchi_t1p``P``_txdat_bits_Resp; \
    logic [4:0]  cchi_t1p``P``_txdat_bits_DataSource; \
    logic [2:0]  cchi_t1p``P``_txdat_bits_CBusy; \
    logic        cchi_t1p``P``_txdat_bits_DataID; \
    logic [31:0] cchi_t1p``P``_txdat_bits_Data [0:7]; \
    logic        cchi_t1p``P``_txdat_bits_WayValid; \
    logic [3:0]  cchi_t1p``P``_txdat_bits_Way; \
    logic        cchi_t1p``P``_txdat_bits_TraceTag; \
    \
    logic        cchi_t1p``P``_rxdat_valid; \
    logic [6:0]  cchi_t1p``P``_rxdat_bits_TxnID; \
    logic [4:0]  cchi_t1p``P``_rxdat_bits_SrcID; \
    logic [4:0]  cchi_t1p``P``_rxdat_bits_TgtID; \
    logic [1:0]  cchi_t1p``P``_rxdat_bits_Opcode; \
    logic [1:0]  cchi_t1p``P``_rxdat_bits_RespErr; \
    logic [2:0]  cchi_t1p``P``_rxdat_bits_Resp; \
    logic        cchi_t1p``P``_rxdat_bits_DataID; \
    logic [31:0] cchi_t1p``P``_rxdat_bits_Data [0:7]; \
    logic [31:0] cchi_t1p``P``_rxdat_bits_BE; \
    logic        cchi_t1p``P``_rxdat_bits_TraceTag; \
    logic        cchi_t1p``P``_rxdat_ready

// -----------------------------------------------------------------------------
// Boundary signal declarations: AXI memory port axi_m<P> (24-bit ID) and AXI
// config port axi_s<P> (11-bit ID).
// -----------------------------------------------------------------------------
`define COHESTRA_VCS_AXI_M_SIGS(P) \
    logic         axi_m``P``_awvalid; \
    logic [23:0]  axi_m``P``_awid; \
    logic [47:0]  axi_m``P``_awaddr; \
    logic [7:0]   axi_m``P``_awlen; \
    logic [2:0]   axi_m``P``_awsize; \
    logic [1:0]   axi_m``P``_awburst; \
    logic         axi_m``P``_awready; \
    logic         axi_m``P``_wvalid; \
    logic [255:0] axi_m``P``_wdata; \
    logic [31:0]  axi_m``P``_wstrb; \
    logic         axi_m``P``_wlast; \
    logic         axi_m``P``_wready; \
    logic         axi_m``P``_bvalid; \
    logic [23:0]  axi_m``P``_bid; \
    logic [1:0]   axi_m``P``_bresp; \
    logic         axi_m``P``_bready; \
    logic         axi_m``P``_arvalid; \
    logic [23:0]  axi_m``P``_arid; \
    logic [47:0]  axi_m``P``_araddr; \
    logic [7:0]   axi_m``P``_arlen; \
    logic [2:0]   axi_m``P``_arsize; \
    logic [1:0]   axi_m``P``_arburst; \
    logic         axi_m``P``_arready; \
    logic         axi_m``P``_rvalid; \
    logic [23:0]  axi_m``P``_rid; \
    logic [255:0] axi_m``P``_rdata; \
    logic [1:0]   axi_m``P``_rresp; \
    logic         axi_m``P``_rlast; \
    logic         axi_m``P``_rready

`define COHESTRA_VCS_AXI_S_SIGS(P) \
    logic         axi_s``P``_awvalid; \
    logic [10:0]  axi_s``P``_awid; \
    logic [47:0]  axi_s``P``_awaddr; \
    logic [7:0]   axi_s``P``_awlen; \
    logic [2:0]   axi_s``P``_awsize; \
    logic [1:0]   axi_s``P``_awburst; \
    logic         axi_s``P``_awready; \
    logic         axi_s``P``_wvalid; \
    logic [255:0] axi_s``P``_wdata; \
    logic [31:0]  axi_s``P``_wstrb; \
    logic         axi_s``P``_wlast; \
    logic         axi_s``P``_wready; \
    logic         axi_s``P``_bvalid; \
    logic [10:0]  axi_s``P``_bid; \
    logic [1:0]   axi_s``P``_bresp; \
    logic         axi_s``P``_bready; \
    logic         axi_s``P``_arvalid; \
    logic [10:0]  axi_s``P``_arid; \
    logic [47:0]  axi_s``P``_araddr; \
    logic [7:0]   axi_s``P``_arlen; \
    logic [2:0]   axi_s``P``_arsize; \
    logic [1:0]   axi_s``P``_arburst; \
    logic         axi_s``P``_arready; \
    logic         axi_s``P``_rvalid; \
    logic [10:0]  axi_s``P``_rid; \
    logic [255:0] axi_s``P``_rdata; \
    logic [1:0]   axi_s``P``_rresp; \
    logic         axi_s``P``_rlast; \
    logic         axi_s``P``_rready

// -----------------------------------------------------------------------------
// DUT connections (pin = same-named bridge signal).
// -----------------------------------------------------------------------------
`define COHESTRA_VCS_CCHI_T1_CONN(P) \
    .cchi_t1p``P``_rxevt_valid            (cchi_t1p``P``_rxevt_valid), \
    .cchi_t1p``P``_rxevt_bits_TxnID       (cchi_t1p``P``_rxevt_bits_TxnID), \
    .cchi_t1p``P``_rxevt_bits_SrcID       (cchi_t1p``P``_rxevt_bits_SrcID), \
    .cchi_t1p``P``_rxevt_bits_TgtID       (cchi_t1p``P``_rxevt_bits_TgtID), \
    .cchi_t1p``P``_rxevt_bits_Opcode      (cchi_t1p``P``_rxevt_bits_Opcode), \
    .cchi_t1p``P``_rxevt_bits_Addr        (cchi_t1p``P``_rxevt_bits_Addr), \
    .cchi_t1p``P``_rxevt_bits_NS          (cchi_t1p``P``_rxevt_bits_NS), \
    .cchi_t1p``P``_rxevt_bits_MemAttr     (cchi_t1p``P``_rxevt_bits_MemAttr), \
    .cchi_t1p``P``_rxevt_bits_WayValid    (cchi_t1p``P``_rxevt_bits_WayValid), \
    .cchi_t1p``P``_rxevt_bits_Way         (cchi_t1p``P``_rxevt_bits_Way), \
    .cchi_t1p``P``_rxevt_bits_TraceTag    (cchi_t1p``P``_rxevt_bits_TraceTag), \
    .cchi_t1p``P``_rxevt_ready            (cchi_t1p``P``_rxevt_ready), \
    .cchi_t1p``P``_rxreq_valid            (cchi_t1p``P``_rxreq_valid), \
    .cchi_t1p``P``_rxreq_bits_TxnID       (cchi_t1p``P``_rxreq_bits_TxnID), \
    .cchi_t1p``P``_rxreq_bits_SrcID       (cchi_t1p``P``_rxreq_bits_SrcID), \
    .cchi_t1p``P``_rxreq_bits_TgtID       (cchi_t1p``P``_rxreq_bits_TgtID), \
    .cchi_t1p``P``_rxreq_bits_Opcode      (cchi_t1p``P``_rxreq_bits_Opcode), \
    .cchi_t1p``P``_rxreq_bits_Size        (cchi_t1p``P``_rxreq_bits_Size), \
    .cchi_t1p``P``_rxreq_bits_Addr        (cchi_t1p``P``_rxreq_bits_Addr), \
    .cchi_t1p``P``_rxreq_bits_TagAlias    (cchi_t1p``P``_rxreq_bits_TagAlias), \
    .cchi_t1p``P``_rxreq_bits_NS          (cchi_t1p``P``_rxreq_bits_NS), \
    .cchi_t1p``P``_rxreq_bits_Order       (cchi_t1p``P``_rxreq_bits_Order), \
    .cchi_t1p``P``_rxreq_bits_MemAttr     (cchi_t1p``P``_rxreq_bits_MemAttr), \
    .cchi_t1p``P``_rxreq_bits_Excl        (cchi_t1p``P``_rxreq_bits_Excl), \
    .cchi_t1p``P``_rxreq_bits_ExpCompData (cchi_t1p``P``_rxreq_bits_ExpCompData), \
    .cchi_t1p``P``_rxreq_bits_TraceTag    (cchi_t1p``P``_rxreq_bits_TraceTag), \
    .cchi_t1p``P``_rxreq_bits_WayValid    (cchi_t1p``P``_rxreq_bits_WayValid), \
    .cchi_t1p``P``_rxreq_bits_Way         (cchi_t1p``P``_rxreq_bits_Way), \
    .cchi_t1p``P``_rxreq_ready            (cchi_t1p``P``_rxreq_ready), \
    .cchi_t1p``P``_txsnp_ready            (cchi_t1p``P``_txsnp_ready), \
    .cchi_t1p``P``_txsnp_valid            (cchi_t1p``P``_txsnp_valid), \
    .cchi_t1p``P``_txsnp_bits_TxnID       (cchi_t1p``P``_txsnp_bits_TxnID), \
    .cchi_t1p``P``_txsnp_bits_SrcID       (cchi_t1p``P``_txsnp_bits_SrcID), \
    .cchi_t1p``P``_txsnp_bits_TgtID       (cchi_t1p``P``_txsnp_bits_TgtID), \
    .cchi_t1p``P``_txsnp_bits_Opcode      (cchi_t1p``P``_txsnp_bits_Opcode), \
    .cchi_t1p``P``_txsnp_bits_Addr        (cchi_t1p``P``_txsnp_bits_Addr), \
    .cchi_t1p``P``_txsnp_bits_NS          (cchi_t1p``P``_txsnp_bits_NS), \
    .cchi_t1p``P``_txsnp_bits_TraceTag    (cchi_t1p``P``_txsnp_bits_TraceTag), \
    .cchi_t1p``P``_txrsp_ready            (cchi_t1p``P``_txrsp_ready), \
    .cchi_t1p``P``_txrsp_valid            (cchi_t1p``P``_txrsp_valid), \
    .cchi_t1p``P``_txrsp_bits_TxnID       (cchi_t1p``P``_txrsp_bits_TxnID), \
    .cchi_t1p``P``_txrsp_bits_SrcID       (cchi_t1p``P``_txrsp_bits_SrcID), \
    .cchi_t1p``P``_txrsp_bits_TgtID       (cchi_t1p``P``_txrsp_bits_TgtID), \
    .cchi_t1p``P``_txrsp_bits_DBID        (cchi_t1p``P``_txrsp_bits_DBID), \
    .cchi_t1p``P``_txrsp_bits_Opcode      (cchi_t1p``P``_txrsp_bits_Opcode), \
    .cchi_t1p``P``_txrsp_bits_RespErr     (cchi_t1p``P``_txrsp_bits_RespErr), \
    .cchi_t1p``P``_txrsp_bits_Resp        (cchi_t1p``P``_txrsp_bits_Resp), \
    .cchi_t1p``P``_txrsp_bits_CBusy       (cchi_t1p``P``_txrsp_bits_CBusy), \
    .cchi_t1p``P``_txrsp_bits_WayValid    (cchi_t1p``P``_txrsp_bits_WayValid), \
    .cchi_t1p``P``_txrsp_bits_Way         (cchi_t1p``P``_txrsp_bits_Way), \
    .cchi_t1p``P``_txrsp_bits_TraceTag    (cchi_t1p``P``_txrsp_bits_TraceTag), \
    .cchi_t1p``P``_rxrsp_valid            (cchi_t1p``P``_rxrsp_valid), \
    .cchi_t1p``P``_rxrsp_bits_TxnID       (cchi_t1p``P``_rxrsp_bits_TxnID), \
    .cchi_t1p``P``_rxrsp_bits_SrcID       (cchi_t1p``P``_rxrsp_bits_SrcID), \
    .cchi_t1p``P``_rxrsp_bits_TgtID       (cchi_t1p``P``_rxrsp_bits_TgtID), \
    .cchi_t1p``P``_rxrsp_bits_Opcode      (cchi_t1p``P``_rxrsp_bits_Opcode), \
    .cchi_t1p``P``_rxrsp_bits_RespErr     (cchi_t1p``P``_rxrsp_bits_RespErr), \
    .cchi_t1p``P``_rxrsp_bits_Resp        (cchi_t1p``P``_rxrsp_bits_Resp), \
    .cchi_t1p``P``_rxrsp_bits_TraceTag    (cchi_t1p``P``_rxrsp_bits_TraceTag), \
    .cchi_t1p``P``_rxrsp_ready            (cchi_t1p``P``_rxrsp_ready), \
    .cchi_t1p``P``_txdat_ready            (cchi_t1p``P``_txdat_ready), \
    .cchi_t1p``P``_txdat_valid            (cchi_t1p``P``_txdat_valid), \
    .cchi_t1p``P``_txdat_bits_TxnID       (cchi_t1p``P``_txdat_bits_TxnID), \
    .cchi_t1p``P``_txdat_bits_SrcID       (cchi_t1p``P``_txdat_bits_SrcID), \
    .cchi_t1p``P``_txdat_bits_TgtID       (cchi_t1p``P``_txdat_bits_TgtID), \
    .cchi_t1p``P``_txdat_bits_DBID        (cchi_t1p``P``_txdat_bits_DBID), \
    .cchi_t1p``P``_txdat_bits_Opcode      (cchi_t1p``P``_txdat_bits_Opcode), \
    .cchi_t1p``P``_txdat_bits_RespErr     (cchi_t1p``P``_txdat_bits_RespErr), \
    .cchi_t1p``P``_txdat_bits_Resp        (cchi_t1p``P``_txdat_bits_Resp), \
    .cchi_t1p``P``_txdat_bits_DataSource  (cchi_t1p``P``_txdat_bits_DataSource), \
    .cchi_t1p``P``_txdat_bits_CBusy       (cchi_t1p``P``_txdat_bits_CBusy), \
    .cchi_t1p``P``_txdat_bits_DataID      (cchi_t1p``P``_txdat_bits_DataID), \
    .cchi_t1p``P``_txdat_bits_Data        (cchi_t1p``P``_txdat_bits_Data), \
    .cchi_t1p``P``_txdat_bits_WayValid    (cchi_t1p``P``_txdat_bits_WayValid), \
    .cchi_t1p``P``_txdat_bits_Way         (cchi_t1p``P``_txdat_bits_Way), \
    .cchi_t1p``P``_txdat_bits_TraceTag    (cchi_t1p``P``_txdat_bits_TraceTag), \
    .cchi_t1p``P``_rxdat_valid            (cchi_t1p``P``_rxdat_valid), \
    .cchi_t1p``P``_rxdat_bits_TxnID       (cchi_t1p``P``_rxdat_bits_TxnID), \
    .cchi_t1p``P``_rxdat_bits_SrcID       (cchi_t1p``P``_rxdat_bits_SrcID), \
    .cchi_t1p``P``_rxdat_bits_TgtID       (cchi_t1p``P``_rxdat_bits_TgtID), \
    .cchi_t1p``P``_rxdat_bits_Opcode      (cchi_t1p``P``_rxdat_bits_Opcode), \
    .cchi_t1p``P``_rxdat_bits_RespErr     (cchi_t1p``P``_rxdat_bits_RespErr), \
    .cchi_t1p``P``_rxdat_bits_Resp        (cchi_t1p``P``_rxdat_bits_Resp), \
    .cchi_t1p``P``_rxdat_bits_DataID      (cchi_t1p``P``_rxdat_bits_DataID), \
    .cchi_t1p``P``_rxdat_bits_Data        (cchi_t1p``P``_rxdat_bits_Data), \
    .cchi_t1p``P``_rxdat_bits_BE          (cchi_t1p``P``_rxdat_bits_BE), \
    .cchi_t1p``P``_rxdat_bits_TraceTag    (cchi_t1p``P``_rxdat_bits_TraceTag), \
    .cchi_t1p``P``_rxdat_ready            (cchi_t1p``P``_rxdat_ready)

`define COHESTRA_VCS_AXI_CONN(PREFIX) \
    .PREFIX``awvalid (PREFIX``awvalid), \
    .PREFIX``awid    (PREFIX``awid), \
    .PREFIX``awaddr  (PREFIX``awaddr), \
    .PREFIX``awlen   (PREFIX``awlen), \
    .PREFIX``awsize  (PREFIX``awsize), \
    .PREFIX``awburst (PREFIX``awburst), \
    .PREFIX``awready (PREFIX``awready), \
    .PREFIX``wvalid  (PREFIX``wvalid), \
    .PREFIX``wdata   (PREFIX``wdata), \
    .PREFIX``wstrb   (PREFIX``wstrb), \
    .PREFIX``wlast   (PREFIX``wlast), \
    .PREFIX``wready  (PREFIX``wready), \
    .PREFIX``bvalid  (PREFIX``bvalid), \
    .PREFIX``bid     (PREFIX``bid), \
    .PREFIX``bresp   (PREFIX``bresp), \
    .PREFIX``bready  (PREFIX``bready), \
    .PREFIX``arvalid (PREFIX``arvalid), \
    .PREFIX``arid    (PREFIX``arid), \
    .PREFIX``araddr  (PREFIX``araddr), \
    .PREFIX``arlen   (PREFIX``arlen), \
    .PREFIX``arsize  (PREFIX``arsize), \
    .PREFIX``arburst (PREFIX``arburst), \
    .PREFIX``arready (PREFIX``arready), \
    .PREFIX``rvalid  (PREFIX``rvalid), \
    .PREFIX``rid     (PREFIX``rid), \
    .PREFIX``rdata   (PREFIX``rdata), \
    .PREFIX``rresp   (PREFIX``rresp), \
    .PREFIX``rlast   (PREFIX``rlast), \
    .PREFIX``rready  (PREFIX``rready)

// -----------------------------------------------------------------------------
// Drive assigns (to-DUT struct -> pins), one port group per macro.
// D = the to-DUT struct expression; P = the port index.
// -----------------------------------------------------------------------------
`define COHESTRA_VCS_DRIVE_T1(P, D) \
    cchi_t1p``P``_rxevt_valid            = D.cchi_t1p``P``.rxevt_valid[0]; \
    cchi_t1p``P``_rxevt_bits_TxnID       = D.cchi_t1p``P``.rxevt_bits_TxnID[6:0]; \
    cchi_t1p``P``_rxevt_bits_SrcID       = D.cchi_t1p``P``.rxevt_bits_SrcID[4:0]; \
    cchi_t1p``P``_rxevt_bits_TgtID       = D.cchi_t1p``P``.rxevt_bits_TgtID[4:0]; \
    cchi_t1p``P``_rxevt_bits_Opcode      = D.cchi_t1p``P``.rxevt_bits_Opcode[0]; \
    cchi_t1p``P``_rxevt_bits_Addr        = D.cchi_t1p``P``.rxevt_bits_Addr[47:0]; \
    cchi_t1p``P``_rxevt_bits_NS          = D.cchi_t1p``P``.rxevt_bits_NS[0]; \
    cchi_t1p``P``_rxevt_bits_MemAttr     = D.cchi_t1p``P``.rxevt_bits_MemAttr[0]; \
    cchi_t1p``P``_rxevt_bits_WayValid    = D.cchi_t1p``P``.rxevt_bits_WayValid[0]; \
    cchi_t1p``P``_rxevt_bits_Way         = D.cchi_t1p``P``.rxevt_bits_Way[3:0]; \
    cchi_t1p``P``_rxevt_bits_TraceTag    = D.cchi_t1p``P``.rxevt_bits_TraceTag[0]; \
    \
    cchi_t1p``P``_rxreq_valid            = D.cchi_t1p``P``.rxreq_valid[0]; \
    cchi_t1p``P``_rxreq_bits_TxnID       = D.cchi_t1p``P``.rxreq_bits_TxnID[6:0]; \
    cchi_t1p``P``_rxreq_bits_SrcID       = D.cchi_t1p``P``.rxreq_bits_SrcID[4:0]; \
    cchi_t1p``P``_rxreq_bits_TgtID       = D.cchi_t1p``P``.rxreq_bits_TgtID[4:0]; \
    cchi_t1p``P``_rxreq_bits_Opcode      = D.cchi_t1p``P``.rxreq_bits_Opcode[5:0]; \
    cchi_t1p``P``_rxreq_bits_Size        = D.cchi_t1p``P``.rxreq_bits_Size[2:0]; \
    cchi_t1p``P``_rxreq_bits_Addr        = D.cchi_t1p``P``.rxreq_bits_Addr[47:0]; \
    cchi_t1p``P``_rxreq_bits_TagAlias    = D.cchi_t1p``P``.rxreq_bits_TagAlias[7:0]; \
    cchi_t1p``P``_rxreq_bits_NS          = D.cchi_t1p``P``.rxreq_bits_NS[0]; \
    cchi_t1p``P``_rxreq_bits_Order       = D.cchi_t1p``P``.rxreq_bits_Order[1:0]; \
    cchi_t1p``P``_rxreq_bits_MemAttr     = D.cchi_t1p``P``.rxreq_bits_MemAttr[3:0]; \
    cchi_t1p``P``_rxreq_bits_Excl        = D.cchi_t1p``P``.rxreq_bits_Excl[0]; \
    cchi_t1p``P``_rxreq_bits_ExpCompData = D.cchi_t1p``P``.rxreq_bits_ExpCompData[0]; \
    cchi_t1p``P``_rxreq_bits_TraceTag    = D.cchi_t1p``P``.rxreq_bits_TraceTag[0]; \
    cchi_t1p``P``_rxreq_bits_WayValid    = D.cchi_t1p``P``.rxreq_bits_WayValid[0]; \
    cchi_t1p``P``_rxreq_bits_Way         = D.cchi_t1p``P``.rxreq_bits_Way[3:0]; \
    \
    cchi_t1p``P``_rxrsp_valid            = D.cchi_t1p``P``.rxrsp_valid[0]; \
    cchi_t1p``P``_rxrsp_bits_TxnID       = D.cchi_t1p``P``.rxrsp_bits_TxnID[6:0]; \
    cchi_t1p``P``_rxrsp_bits_SrcID       = D.cchi_t1p``P``.rxrsp_bits_SrcID[4:0]; \
    cchi_t1p``P``_rxrsp_bits_TgtID       = D.cchi_t1p``P``.rxrsp_bits_TgtID[4:0]; \
    cchi_t1p``P``_rxrsp_bits_Opcode      = D.cchi_t1p``P``.rxrsp_bits_Opcode[0]; \
    cchi_t1p``P``_rxrsp_bits_RespErr     = D.cchi_t1p``P``.rxrsp_bits_RespErr[1:0]; \
    cchi_t1p``P``_rxrsp_bits_Resp        = D.cchi_t1p``P``.rxrsp_bits_Resp[2:0]; \
    cchi_t1p``P``_rxrsp_bits_TraceTag    = D.cchi_t1p``P``.rxrsp_bits_TraceTag[0]; \
    \
    cchi_t1p``P``_rxdat_valid            = D.cchi_t1p``P``.rxdat_valid[0]; \
    cchi_t1p``P``_rxdat_bits_TxnID       = D.cchi_t1p``P``.rxdat_bits_TxnID[6:0]; \
    cchi_t1p``P``_rxdat_bits_SrcID       = D.cchi_t1p``P``.rxdat_bits_SrcID[4:0]; \
    cchi_t1p``P``_rxdat_bits_TgtID       = D.cchi_t1p``P``.rxdat_bits_TgtID[4:0]; \
    cchi_t1p``P``_rxdat_bits_Opcode      = D.cchi_t1p``P``.rxdat_bits_Opcode[1:0]; \
    cchi_t1p``P``_rxdat_bits_RespErr     = D.cchi_t1p``P``.rxdat_bits_RespErr[1:0]; \
    cchi_t1p``P``_rxdat_bits_Resp        = D.cchi_t1p``P``.rxdat_bits_Resp[2:0]; \
    cchi_t1p``P``_rxdat_bits_DataID      = D.cchi_t1p``P``.rxdat_bits_DataID[0]; \
    for (int i = 0; i < 8; i++) \
        cchi_t1p``P``_rxdat_bits_Data[i] = D.cchi_t1p``P``.rxdat_bits_Data[i*32 +: 32]; \
    cchi_t1p``P``_rxdat_bits_BE          = D.cchi_t1p``P``.rxdat_bits_BE; \
    cchi_t1p``P``_rxdat_bits_TraceTag    = D.cchi_t1p``P``.rxdat_bits_TraceTag[0]; \
    \
    cchi_t1p``P``_txsnp_ready            = D.cchi_t1p``P``.txsnp_ready[0]; \
    cchi_t1p``P``_txrsp_ready            = D.cchi_t1p``P``.txrsp_ready[0]; \
    cchi_t1p``P``_txdat_ready            = D.cchi_t1p``P``.txdat_ready[0]

// AXI pull channels (harness -> DUT): awready/wready/B/arready/R. P is both
// the pin-prefix index and the struct member name (axi_m0/axi_m1/axi_s0).
`define COHESTRA_VCS_DRIVE_AXI_PULL(P, D) \
    P``_awready = D.P``.awready[0]; \
    P``_wready  = D.P``.wready[0]; \
    P``_bvalid  = D.P``.bvalid[0]; \
    P``_bid     = D.P``.bid[$bits(P``_bid)-1:0]; \
    P``_bresp   = D.P``.bresp[1:0]; \
    P``_arready = D.P``.arready[0]; \
    P``_rvalid  = D.P``.rvalid[0]; \
    P``_rid     = D.P``.rid[$bits(P``_rid)-1:0]; \
    P``_rdata   = D.P``.rdata; \
    P``_rresp   = D.P``.rresp[1:0]; \
    P``_rlast   = D.P``.rlast[0]

// AXI push channels (harness -> DUT): AW/W/bready/AR/rready (axi_s0 only;
// the axi_m push channels are DUT outputs and are sampled, not driven).
`define COHESTRA_VCS_DRIVE_AXI_PUSH(P, D) \
    P``_awvalid = D.P``.awvalid[0]; \
    P``_awid    = D.P``.awid[$bits(P``_awid)-1:0]; \
    P``_awaddr  = D.P``.awaddr[47:0]; \
    P``_awlen   = D.P``.awlen[7:0]; \
    P``_awsize  = D.P``.awsize[2:0]; \
    P``_awburst = D.P``.awburst[1:0]; \
    P``_wvalid  = D.P``.wvalid[0]; \
    P``_wdata   = D.P``.wdata; \
    P``_wstrb   = D.P``.wstrb; \
    P``_wlast   = D.P``.wlast[0]; \
    P``_bready  = D.P``.bready[0]; \
    P``_arvalid = D.P``.arvalid[0]; \
    P``_arid    = D.P``.arid[$bits(P``_arid)-1:0]; \
    P``_araddr  = D.P``.araddr[47:0]; \
    P``_arlen   = D.P``.arlen[7:0]; \
    P``_arsize  = D.P``.arsize[2:0]; \
    P``_arburst = D.P``.arburst[1:0]; \
    P``_rready  = D.P``.rready[0]

// -----------------------------------------------------------------------------
// Sample assigns (pins -> from-DUT struct), one port group per macro.
// S = the from-DUT struct expression; P = the port index. Sampled pins are
// zero-extended into the 2-state wire words (X/Z collapse to 0, the same
// 2-state semantics the Verilator model has).
// -----------------------------------------------------------------------------
`define COHESTRA_VCS_SAMPLE_T1(P, S) \
    S.cchi_t1p``P``.rxevt_ready = 32'(cchi_t1p``P``_rxevt_ready); \
    S.cchi_t1p``P``.rxreq_ready = 32'(cchi_t1p``P``_rxreq_ready); \
    S.cchi_t1p``P``.rxrsp_ready = 32'(cchi_t1p``P``_rxrsp_ready); \
    S.cchi_t1p``P``.rxdat_ready = 32'(cchi_t1p``P``_rxdat_ready); \
    \
    S.cchi_t1p``P``.txsnp_valid        = 32'(cchi_t1p``P``_txsnp_valid); \
    S.cchi_t1p``P``.txsnp_bits_TxnID   = 32'(cchi_t1p``P``_txsnp_bits_TxnID); \
    S.cchi_t1p``P``.txsnp_bits_SrcID   = 32'(cchi_t1p``P``_txsnp_bits_SrcID); \
    S.cchi_t1p``P``.txsnp_bits_TgtID   = 32'(cchi_t1p``P``_txsnp_bits_TgtID); \
    S.cchi_t1p``P``.txsnp_bits_Opcode  = 32'(cchi_t1p``P``_txsnp_bits_Opcode); \
    S.cchi_t1p``P``.txsnp_bits_Addr    = 64'(cchi_t1p``P``_txsnp_bits_Addr); \
    S.cchi_t1p``P``.txsnp_bits_NS      = 32'(cchi_t1p``P``_txsnp_bits_NS); \
    S.cchi_t1p``P``.txsnp_bits_TraceTag = 32'(cchi_t1p``P``_txsnp_bits_TraceTag); \
    \
    S.cchi_t1p``P``.txrsp_valid        = 32'(cchi_t1p``P``_txrsp_valid); \
    S.cchi_t1p``P``.txrsp_bits_TxnID   = 32'(cchi_t1p``P``_txrsp_bits_TxnID); \
    S.cchi_t1p``P``.txrsp_bits_SrcID   = 32'(cchi_t1p``P``_txrsp_bits_SrcID); \
    S.cchi_t1p``P``.txrsp_bits_TgtID   = 32'(cchi_t1p``P``_txrsp_bits_TgtID); \
    S.cchi_t1p``P``.txrsp_bits_DBID    = 32'(cchi_t1p``P``_txrsp_bits_DBID); \
    S.cchi_t1p``P``.txrsp_bits_Opcode  = 32'(cchi_t1p``P``_txrsp_bits_Opcode); \
    S.cchi_t1p``P``.txrsp_bits_RespErr = 32'(cchi_t1p``P``_txrsp_bits_RespErr); \
    S.cchi_t1p``P``.txrsp_bits_Resp    = 32'(cchi_t1p``P``_txrsp_bits_Resp); \
    S.cchi_t1p``P``.txrsp_bits_CBusy   = 32'(cchi_t1p``P``_txrsp_bits_CBusy); \
    S.cchi_t1p``P``.txrsp_bits_WayValid = 32'(cchi_t1p``P``_txrsp_bits_WayValid); \
    S.cchi_t1p``P``.txrsp_bits_Way     = 32'(cchi_t1p``P``_txrsp_bits_Way); \
    S.cchi_t1p``P``.txrsp_bits_TraceTag = 32'(cchi_t1p``P``_txrsp_bits_TraceTag); \
    \
    S.cchi_t1p``P``.txdat_valid        = 32'(cchi_t1p``P``_txdat_valid); \
    S.cchi_t1p``P``.txdat_bits_TxnID   = 32'(cchi_t1p``P``_txdat_bits_TxnID); \
    S.cchi_t1p``P``.txdat_bits_SrcID   = 32'(cchi_t1p``P``_txdat_bits_SrcID); \
    S.cchi_t1p``P``.txdat_bits_TgtID   = 32'(cchi_t1p``P``_txdat_bits_TgtID); \
    S.cchi_t1p``P``.txdat_bits_DBID    = 32'(cchi_t1p``P``_txdat_bits_DBID); \
    S.cchi_t1p``P``.txdat_bits_Opcode  = 32'(cchi_t1p``P``_txdat_bits_Opcode); \
    S.cchi_t1p``P``.txdat_bits_RespErr = 32'(cchi_t1p``P``_txdat_bits_RespErr); \
    S.cchi_t1p``P``.txdat_bits_Resp    = 32'(cchi_t1p``P``_txdat_bits_Resp); \
    S.cchi_t1p``P``.txdat_bits_DataSource = 32'(cchi_t1p``P``_txdat_bits_DataSource); \
    S.cchi_t1p``P``.txdat_bits_CBusy   = 32'(cchi_t1p``P``_txdat_bits_CBusy); \
    S.cchi_t1p``P``.txdat_bits_DataID  = 32'(cchi_t1p``P``_txdat_bits_DataID); \
    for (int i = 0; i < 8; i++) \
        S.cchi_t1p``P``.txdat_bits_Data[i*32 +: 32] = cchi_t1p``P``_txdat_bits_Data[i]; \
    S.cchi_t1p``P``.txdat_bits_WayValid = 32'(cchi_t1p``P``_txdat_bits_WayValid); \
    S.cchi_t1p``P``.txdat_bits_Way     = 32'(cchi_t1p``P``_txdat_bits_Way); \
    S.cchi_t1p``P``.txdat_bits_TraceTag = 32'(cchi_t1p``P``_txdat_bits_TraceTag)

`define COHESTRA_VCS_SAMPLE_AXI_PUSH(P, S) \
    S.P``.awvalid = 32'(P``_awvalid); \
    S.P``.awid    = 32'(P``_awid); \
    S.P``.awaddr  = 64'(P``_awaddr); \
    S.P``.awlen   = 32'(P``_awlen); \
    S.P``.awsize  = 32'(P``_awsize); \
    S.P``.awburst = 32'(P``_awburst); \
    S.P``.wvalid  = 32'(P``_wvalid); \
    S.P``.wdata   = P``_wdata; \
    S.P``.wstrb   = 32'(P``_wstrb); \
    S.P``.wlast   = 32'(P``_wlast); \
    S.P``.bready  = 32'(P``_bready); \
    S.P``.arvalid = 32'(P``_arvalid); \
    S.P``.arid    = 32'(P``_arid); \
    S.P``.araddr  = 64'(P``_araddr); \
    S.P``.arlen   = 32'(P``_arlen); \
    S.P``.arsize  = 32'(P``_arsize); \
    S.P``.arburst = 32'(P``_arburst); \
    S.P``.rready  = 32'(P``_rready)

`define COHESTRA_VCS_SAMPLE_AXI_PULL(P, S) \
    S.P``.awready = 32'(P``_awready); \
    S.P``.wready  = 32'(P``_wready); \
    S.P``.bvalid  = 32'(P``_bvalid); \
    S.P``.bid     = 32'(P``_bid); \
    S.P``.bresp   = 32'(P``_bresp); \
    S.P``.arready = 32'(P``_arready); \
    S.P``.rvalid  = 32'(P``_rvalid); \
    S.P``.rid     = 32'(P``_rid); \
    S.P``.rdata   = P``_rdata; \
    S.P``.rresp   = 32'(P``_rresp); \
    S.P``.rlast   = 32'(P``_rlast)


module cohestra_vcs_bridge;

    // -------------------------------------------------------------------------
    // DPI typedefs + import/export declarations (module scope).
    // -------------------------------------------------------------------------
    `include "cohestra_vcs_dpi.svh"

    // Half period in timescale units (1ps): 500 = 500ps, a 1ns (1GHz) clock.
    localparam int unsigned HP = `COHESTRA_VCS_HALF_PERIOD;

    // -------------------------------------------------------------------------
    // Boundary signals
    // -------------------------------------------------------------------------
    logic clock;
    logic reset;

    `COHESTRA_VCS_CCHI_T1_SIGS(0);
    `COHESTRA_VCS_CCHI_T1_SIGS(1);
    `COHESTRA_VCS_CCHI_T1_SIGS(2);
    `COHESTRA_VCS_CCHI_T1_SIGS(3);

    `COHESTRA_VCS_AXI_M_SIGS(0);
    `COHESTRA_VCS_AXI_M_SIGS(1);

    `COHESTRA_VCS_AXI_S_SIGS(0);

    // -------------------------------------------------------------------------
    // DUT
    // -------------------------------------------------------------------------
    `COHESTRA_VCS_TOP_MODULE u_dut (
        .clock  (clock),
        .reset  (reset),

        `COHESTRA_VCS_CCHI_T1_CONN(0),
        `COHESTRA_VCS_CCHI_T1_CONN(1),
        `COHESTRA_VCS_CCHI_T1_CONN(2),
        `COHESTRA_VCS_CCHI_T1_CONN(3),

        `COHESTRA_VCS_AXI_CONN(axi_m0_),
        `COHESTRA_VCS_AXI_CONN(axi_m1_),

        `COHESTRA_VCS_AXI_CONN(axi_s0_)
    );

    // -------------------------------------------------------------------------
    // Exported drive/sample tasks & functions (C++ -> SV; bodies referenced by
    // the export declarations in cohestra_vcs_dpi.svh).
    //
    // NOTE: the #HP delays are guarded for `VERILATOR` so that
    // `verilator --lint-only --timing` parses this file (Verilator does not
    // support timing controls inside DPI-exported tasks); VCS sees the real
    // delays. The lint flow never simulates, so the guard is lint-only.
    // -------------------------------------------------------------------------
    task CohestraVCS_DriveLow(input cohestra_vcs_to_dut_t drv);
        `COHESTRA_VCS_DRIVE_T1(0, drv);
        `COHESTRA_VCS_DRIVE_T1(1, drv);
        `COHESTRA_VCS_DRIVE_T1(2, drv);
        `COHESTRA_VCS_DRIVE_T1(3, drv);

        `COHESTRA_VCS_DRIVE_AXI_PULL(axi_m0, drv);
        `COHESTRA_VCS_DRIVE_AXI_PULL(axi_m1, drv);

        `COHESTRA_VCS_DRIVE_AXI_PUSH(axi_s0, drv);

        reset = drv.reset[0];
        clock = 1'b0;
`ifndef VERILATOR
        #(HP);
`endif
    endtask

    function void CohestraVCS_Sample(output cohestra_vcs_from_dut_t out_s);
        `COHESTRA_VCS_SAMPLE_T1(0, out_s);
        `COHESTRA_VCS_SAMPLE_T1(1, out_s);
        `COHESTRA_VCS_SAMPLE_T1(2, out_s);
        `COHESTRA_VCS_SAMPLE_T1(3, out_s);

        `COHESTRA_VCS_SAMPLE_AXI_PUSH(axi_m0, out_s);
        `COHESTRA_VCS_SAMPLE_AXI_PUSH(axi_m1, out_s);

        `COHESTRA_VCS_SAMPLE_AXI_PULL(axi_s0, out_s);
    endfunction

    task CohestraVCS_DriveHigh();
        clock = 1'b1;
`ifndef VERILATOR
        #(HP);
`endif
    endtask

    // -------------------------------------------------------------------------
    // Waveform control ($dumpvars / $fsdbDumpvars). 'start_tick' is in harness
    // time units (1 unit = 500ps); a nonzero start gates dumping with
    // $dumpoff/$dumpon. FSDB requires the Verdi PLI linked at build time
    // (+define+COHESTRA_VCS_FSDB by build.sh when VERDI_HOME is set).
    // -------------------------------------------------------------------------
    int wave_open_type = `COHESTRA_VCS_WAVE_NONE;

    task CohestraVCS_OpenWaveform(input int wave_type, input string path, input longint unsigned start_tick);
        case (wave_type)
            `COHESTRA_VCS_WAVE_VCD: begin
                $dumpfile(path);
                $dumpvars(0, cohestra_vcs_bridge);
                wave_open_type = `COHESTRA_VCS_WAVE_VCD;
                if (start_tick > 0) begin
                    $dumpoff;
                    fork
                        begin
                            #(start_tick * HP);
                            $dumpon;
                        end
                    join_none
                end
                $display("[cohestra_vcs] VCD dumping to %0s (start tick %0d)", path, start_tick);
            end
            `COHESTRA_VCS_WAVE_FSDB: begin
`ifdef COHESTRA_VCS_FSDB
                $fsdbDumpfile(path);
                $fsdbDumpvars(0, cohestra_vcs_bridge);
                wave_open_type = `COHESTRA_VCS_WAVE_FSDB;
                if (start_tick > 0) begin
                    $fsdbDumpoff;
                    fork
                        begin
                            #(start_tick * HP);
                            $fsdbDumpon;
                        end
                    join_none
                end
                $display("[cohestra_vcs] FSDB dumping to %0s (start tick %0d)", path, start_tick);
`else
                $display("[cohestra_vcs] ERROR: FSDB waveform requested but this bridge was built without Verdi PLI support (rebuild via build.sh with VERDI_HOME set).");
                $fatal(1, "cohestra_vcs: FSDB unsupported by this build");
`endif
            end
            default:
                $display("[cohestra_vcs] waveform type %0d: no dump.", wave_type);
        endcase
    endtask

    task CohestraVCS_CloseWaveform();
`ifndef VERILATOR
        if (wave_open_type == `COHESTRA_VCS_WAVE_VCD)
            $dumpoff;   // VCS has no $dumpclose; dumpoff stops, $finish closes
        // FSDB files are flushed/closed by the simulator at $finish.
`endif
        wave_open_type = `COHESTRA_VCS_WAVE_NONE;
    endtask

    // -------------------------------------------------------------------------
    // The single harness entry: parse plusargs, check the struct layout, run
    // the harness, report the exit status.
    // -------------------------------------------------------------------------
    initial begin : cohestra_vcs_entry
        string           config_path;
        string           vcd_path;
        string           fsdb_path;
        string           log_path;
        longint unsigned cycles;
        int              no_log;
        int              status;
        int              layout_rc;

        cohestra_vcs_to_dut_t init_drv;

        // Drive clean zeros until the first DriveLow (no posedge can occur
        // before it: clock starts low and DriveHigh only follows a DriveLow).
        init_drv = '0;
        `COHESTRA_VCS_DRIVE_T1(0, init_drv);
        `COHESTRA_VCS_DRIVE_T1(1, init_drv);
        `COHESTRA_VCS_DRIVE_T1(2, init_drv);
        `COHESTRA_VCS_DRIVE_T1(3, init_drv);
        `COHESTRA_VCS_DRIVE_AXI_PULL(axi_m0, init_drv);
        `COHESTRA_VCS_DRIVE_AXI_PULL(axi_m1, init_drv);
        `COHESTRA_VCS_DRIVE_AXI_PUSH(axi_s0, init_drv);
        reset = 1'b0;
        clock = 1'b0;

        if (!$value$plusargs("cohestra_config=%s", config_path)) config_path = "";
        if (!$value$plusargs("cohestra_cycles=%d", cycles))      cycles = 200000;
        if (!$value$plusargs("cohestra_vcd=%s", vcd_path))       vcd_path = "";
        if (!$value$plusargs("cohestra_fsdb=%s", fsdb_path))     fsdb_path = "";
        if (!$value$plusargs("cohestra_log_file=%s", log_path))  log_path = "";
        no_log = $test$plusargs("cohestra_no_log_file");

        $display("[cohestra_vcs] bridge up: half period %0d ps (1 ns clock), config=\"%0s\", cycles=%0d",
            HP, config_path, cycles);

        layout_rc = CohestraVCS_CheckLayout($bits(cohestra_vcs_to_dut_t), $bits(cohestra_vcs_from_dut_t));
        if (layout_rc != 0)
            $fatal(1, "cohestra_vcs: DPI struct layout mismatch (see the C++ side message above)");

        CohestraVCS_Main(config_path, cycles, vcd_path, fsdb_path, log_path, no_log, status);

        $display("COHESTRA_VCS_EXIT_STATUS=%0d", status);

        if (status == 0)
            $finish;
        else
            $fatal(1, "cohestra_vcs harness failed (status %0d)", status);
    end

endmodule

`undef COHESTRA_VCS_CCHI_T1_SIGS
`undef COHESTRA_VCS_AXI_M_SIGS
`undef COHESTRA_VCS_AXI_S_SIGS
`undef COHESTRA_VCS_CCHI_T1_CONN
`undef COHESTRA_VCS_AXI_CONN
`undef COHESTRA_VCS_DRIVE_T1
`undef COHESTRA_VCS_DRIVE_AXI_PULL
`undef COHESTRA_VCS_DRIVE_AXI_PUSH
`undef COHESTRA_VCS_SAMPLE_T1
`undef COHESTRA_VCS_SAMPLE_AXI_PUSH
`undef COHESTRA_VCS_SAMPLE_AXI_PULL

`default_nettype wire
