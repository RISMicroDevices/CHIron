//*****************************************************************************
// biscotto_dut_template.sv — reference shell for a user-supplied Biscotto DUT.
//
// A user DUT plugs into biscotto_top as a module exposing the contract below
// (the default module name is biscotto_dut; if your RTL uses another name,
// pass +define+BISCOTTO_DUT_MODULE=<name> — in the Cohestra V3 CMake build
// through -DCOHESTRA_V3_VERILATOR_ARGS="+define+BISCOTTO_DUT_MODULE=<name>",
// and scripts/check_dut.sh takes it as the BISCOTTO_DUT_MODULE env var):
//
//   clock / reset        global clock; high-active reset
//   cchi_t1p0..2_*       3x Type-1 CCHI ports (Cohestra V3 CCHI contract;
//                        the harness drives rxevt/rxreq/rxrsp/rxdat and
//                        accepts txsnp/txrsp/txdat)
//   chi_rn0..2_*         3x packed CHI-E RN ports (the DUT is the RN-F:
//                        drives txreq/txrsp/txdat, receives rxsnp/rxrsp/
//                        rxdat; flit widths REQ 162 / RSP 73 / DAT 422 /
//                        SNP 115; L-credit flow control via lcrdv)
//   mon_chi_rn0..2_*     3x passive CHI-E RN monitor taps (pure DUT outputs:
//                        per channel flitpend/flitv/lcrdv + the packed flit
//                        at the pin width). biscotto_top connects them 1:1
//                        by name; this shell mirrors its own chi_rn pins.
//
// Port names/widths/directions are fixed by integration/biscotto_dut_if.svh —
// instantiate your real RTL inside this shell (or use the same macros in
// your own module header) and validate with scripts/check_dut.sh.
//
// This template compiles standalone and parks everything (never issues,
// never accepts); replace the tie-offs with your logic.
//*****************************************************************************

`include "biscotto_dut_if.svh"

// Passive CHI-E RN monitor taps of port P (pin names/widths fixed by
// BISCOTTO_TOP_CHI_MON_PORT_DECL in biscotto_top.sv, which connects them
// to the DUT 1:1 by name). Local to this template, mirroring that macro.
`define BISCOTTO_DUT_TEMPLATE_MON_DECL(P) \
    output logic         mon_chi_rn``P``_txreq_flitpend, \
    output logic         mon_chi_rn``P``_txreq_flitv, \
    output logic         mon_chi_rn``P``_txreq_lcrdv, \
    output logic [161:0] mon_chi_rn``P``_txreq_flit, \
    output logic         mon_chi_rn``P``_txrsp_flitpend, \
    output logic         mon_chi_rn``P``_txrsp_flitv, \
    output logic         mon_chi_rn``P``_txrsp_lcrdv, \
    output logic [72:0]  mon_chi_rn``P``_txrsp_flit, \
    output logic         mon_chi_rn``P``_txdat_flitpend, \
    output logic         mon_chi_rn``P``_txdat_flitv, \
    output logic         mon_chi_rn``P``_txdat_lcrdv, \
    output logic [421:0] mon_chi_rn``P``_txdat_flit, \
    output logic         mon_chi_rn``P``_rxsnp_flitpend, \
    output logic         mon_chi_rn``P``_rxsnp_flitv, \
    output logic         mon_chi_rn``P``_rxsnp_lcrdv, \
    output logic [114:0] mon_chi_rn``P``_rxsnp_flit, \
    output logic         mon_chi_rn``P``_rxrsp_flitpend, \
    output logic         mon_chi_rn``P``_rxrsp_flitv, \
    output logic         mon_chi_rn``P``_rxrsp_lcrdv, \
    output logic [72:0]  mon_chi_rn``P``_rxrsp_flit, \
    output logic         mon_chi_rn``P``_rxdat_flitpend, \
    output logic         mon_chi_rn``P``_rxdat_flitv, \
    output logic         mon_chi_rn``P``_rxdat_lcrdv, \
    output logic [421:0] mon_chi_rn``P``_rxdat_flit

module biscotto_dut (
    input  logic        clock,
    input  logic        reset,

    `BISCOTTO_CCHI_T1_PORT_DECL(0),
    `BISCOTTO_CCHI_T1_PORT_DECL(1),
    `BISCOTTO_CCHI_T1_PORT_DECL(2),

    `BISCOTTO_CHI_RN_PORT_DECL(0),
    `BISCOTTO_CHI_RN_PORT_DECL(1),
    `BISCOTTO_CHI_RN_PORT_DECL(2),

    `BISCOTTO_DUT_TEMPLATE_MON_DECL(0),
    `BISCOTTO_DUT_TEMPLATE_MON_DECL(1),
    `BISCOTTO_DUT_TEMPLATE_MON_DECL(2)
);

    // ------------------------------------------------------------------
    // Parked shell: no CCHI traffic accepted or issued, no CHI flits
    // issued, no link credits returned. Replace with your RTL.
    // ------------------------------------------------------------------
    genvar p;

`define BISCOTTO_DUT_PARK(P) \
    always_comb begin \
        cchi_t1p``P``_rxevt_ready         = 1'b0; \
        cchi_t1p``P``_rxreq_ready         = 1'b0; \
        cchi_t1p``P``_rxrsp_ready         = 1'b0; \
        cchi_t1p``P``_rxdat_ready         = 1'b0; \
        cchi_t1p``P``_txsnp_valid         = 1'b0; \
        cchi_t1p``P``_txsnp_bits_TxnID    = '0; \
        cchi_t1p``P``_txsnp_bits_SrcID    = '0; \
        cchi_t1p``P``_txsnp_bits_TgtID    = '0; \
        cchi_t1p``P``_txsnp_bits_Opcode   = '0; \
        cchi_t1p``P``_txsnp_bits_Addr     = '0; \
        cchi_t1p``P``_txsnp_bits_NS       = '0; \
        cchi_t1p``P``_txsnp_bits_TraceTag = '0; \
        cchi_t1p``P``_txrsp_valid         = 1'b0; \
        cchi_t1p``P``_txrsp_bits_TxnID    = '0; \
        cchi_t1p``P``_txrsp_bits_SrcID    = '0; \
        cchi_t1p``P``_txrsp_bits_TgtID    = '0; \
        cchi_t1p``P``_txrsp_bits_DBID     = '0; \
        cchi_t1p``P``_txrsp_bits_Opcode   = '0; \
        cchi_t1p``P``_txrsp_bits_RespErr  = '0; \
        cchi_t1p``P``_txrsp_bits_Resp     = '0; \
        cchi_t1p``P``_txrsp_bits_CBusy    = '0; \
        cchi_t1p``P``_txrsp_bits_WayValid = '0; \
        cchi_t1p``P``_txrsp_bits_Way      = '0; \
        cchi_t1p``P``_txrsp_bits_TraceTag = '0; \
        cchi_t1p``P``_txdat_valid         = 1'b0; \
        cchi_t1p``P``_txdat_bits_TxnID    = '0; \
        cchi_t1p``P``_txdat_bits_SrcID    = '0; \
        cchi_t1p``P``_txdat_bits_TgtID    = '0; \
        cchi_t1p``P``_txdat_bits_DBID     = '0; \
        cchi_t1p``P``_txdat_bits_Opcode   = '0; \
        cchi_t1p``P``_txdat_bits_RespErr  = '0; \
        cchi_t1p``P``_txdat_bits_Resp     = '0; \
        cchi_t1p``P``_txdat_bits_DataSource = '0; \
        cchi_t1p``P``_txdat_bits_CBusy    = '0; \
        cchi_t1p``P``_txdat_bits_DataID   = '0; \
        cchi_t1p``P``_txdat_bits_WayValid = '0; \
        cchi_t1p``P``_txdat_bits_Way      = '0; \
        cchi_t1p``P``_txdat_bits_TraceTag = '0; \
        for (int i = 0; i < 8; i++) \
            cchi_t1p``P``_txdat_bits_Data[i] = '0; \
        chi_rn``P``_txreq_flitpend = 1'b0; \
        chi_rn``P``_txreq_flitv    = 1'b0; \
        chi_rn``P``_txreq_flit     = '0; \
        chi_rn``P``_txrsp_flitpend = 1'b0; \
        chi_rn``P``_txrsp_flitv    = 1'b0; \
        chi_rn``P``_txrsp_flit     = '0; \
        chi_rn``P``_txdat_flitpend = 1'b0; \
        chi_rn``P``_txdat_flitv    = 1'b0; \
        chi_rn``P``_txdat_flit     = '0; \
        chi_rn``P``_rxrsp_lcrdv    = 1'b0; \
        chi_rn``P``_rxdat_lcrdv    = 1'b0; \
        chi_rn``P``_rxsnp_lcrdv    = 1'b0; \
    end

    `BISCOTTO_DUT_PARK(0)
    `BISCOTTO_DUT_PARK(1)
    `BISCOTTO_DUT_PARK(2)

`undef BISCOTTO_DUT_PARK

    // ------------------------------------------------------------------
    // Passive monitor taps: mirror this shell's own chi_rn pins 1:1 so the
    // harness observes exactly what the DUT drives and receives.
    // ------------------------------------------------------------------
`define BISCOTTO_DUT_TEMPLATE_MON_TAP(P) \
    assign mon_chi_rn``P``_txreq_flitpend = chi_rn``P``_txreq_flitpend; \
    assign mon_chi_rn``P``_txreq_flitv    = chi_rn``P``_txreq_flitv; \
    assign mon_chi_rn``P``_txreq_lcrdv    = chi_rn``P``_txreq_lcrdv; \
    assign mon_chi_rn``P``_txreq_flit     = chi_rn``P``_txreq_flit; \
    assign mon_chi_rn``P``_txrsp_flitpend = chi_rn``P``_txrsp_flitpend; \
    assign mon_chi_rn``P``_txrsp_flitv    = chi_rn``P``_txrsp_flitv; \
    assign mon_chi_rn``P``_txrsp_lcrdv    = chi_rn``P``_txrsp_lcrdv; \
    assign mon_chi_rn``P``_txrsp_flit     = chi_rn``P``_txrsp_flit; \
    assign mon_chi_rn``P``_txdat_flitpend = chi_rn``P``_txdat_flitpend; \
    assign mon_chi_rn``P``_txdat_flitv    = chi_rn``P``_txdat_flitv; \
    assign mon_chi_rn``P``_txdat_lcrdv    = chi_rn``P``_txdat_lcrdv; \
    assign mon_chi_rn``P``_txdat_flit     = chi_rn``P``_txdat_flit; \
    assign mon_chi_rn``P``_rxsnp_flitpend = chi_rn``P``_rxsnp_flitpend; \
    assign mon_chi_rn``P``_rxsnp_flitv    = chi_rn``P``_rxsnp_flitv; \
    assign mon_chi_rn``P``_rxsnp_lcrdv    = chi_rn``P``_rxsnp_lcrdv; \
    assign mon_chi_rn``P``_rxsnp_flit     = chi_rn``P``_rxsnp_flit; \
    assign mon_chi_rn``P``_rxrsp_flitpend = chi_rn``P``_rxrsp_flitpend; \
    assign mon_chi_rn``P``_rxrsp_flitv    = chi_rn``P``_rxrsp_flitv; \
    assign mon_chi_rn``P``_rxrsp_lcrdv    = chi_rn``P``_rxrsp_lcrdv; \
    assign mon_chi_rn``P``_rxrsp_flit     = chi_rn``P``_rxrsp_flit; \
    assign mon_chi_rn``P``_rxdat_flitpend = chi_rn``P``_rxdat_flitpend; \
    assign mon_chi_rn``P``_rxdat_flitv    = chi_rn``P``_rxdat_flitv; \
    assign mon_chi_rn``P``_rxdat_lcrdv    = chi_rn``P``_rxdat_lcrdv; \
    assign mon_chi_rn``P``_rxdat_flit     = chi_rn``P``_rxdat_flit;

    `BISCOTTO_DUT_TEMPLATE_MON_TAP(0)
    `BISCOTTO_DUT_TEMPLATE_MON_TAP(1)
    `BISCOTTO_DUT_TEMPLATE_MON_TAP(2)

`undef BISCOTTO_DUT_TEMPLATE_MON_TAP
`undef BISCOTTO_DUT_TEMPLATE_MON_DECL

endmodule
