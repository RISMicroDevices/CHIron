//*****************************************************************************
// cornetto_dut_template.sv — reference shell for a user-supplied Cornetto DUT.
//
// A user DUT plugs into cornetto_top as a module exposing the contract below
// (the default module name is cornetto_dut; if your RTL uses another name,
// pass +define+CORNETTO_DUT_MODULE=<name> — the Cohestra V3 CMake build
// exposes this as -DCOHESTRA_V3_CORNETTO_DUT_MODULE, and scripts/check_dut.sh
// takes it as the CORNETTO_DUT_MODULE env var):
//
//   clock / reset        global clock; high-active reset
//   cchi_t1p0..1_*       2x Type-1 CCHI ports (Cohestra V3 CCHI contract;
//                        the harness drives rxevt/rxreq/rxrsp/rxdat and
//                        accepts txsnp/txrsp/txdat)
//   chi_rn0..1_*         2x packed CHI-E RN ports (the DUT is the RN-F:
//                        drives txreq/txrsp/txdat, receives rxsnp/rxrsp/
//                        rxdat; flit widths REQ 162 / RSP 73 / DAT 422 /
//                        SNP 115; L-credit flow control via lcrdv)
//
// Port names/widths/directions are fixed by integration/cornetto_dut_if.svh —
// instantiate your real RTL inside this shell (or use the same macros in
// your own module header) and validate with scripts/check_dut.sh.
//
// This template compiles standalone and parks everything (never issues,
// never accepts); replace the tie-offs with your logic.
//*****************************************************************************

`include "cornetto_dut_if.svh"

module cornetto_dut (
    input  logic        clock,
    input  logic        reset,

    `CORNETTO_CCHI_T1_PORT_DECL(0),
    `CORNETTO_CCHI_T1_PORT_DECL(1),

    `CORNETTO_CHI_RN_PORT_DECL(0),
    `CORNETTO_CHI_RN_PORT_DECL(1)
);

    // ------------------------------------------------------------------
    // Parked shell: no CCHI traffic accepted or issued, no CHI flits
    // issued, no link credits returned. Replace with your RTL.
    // ------------------------------------------------------------------
    genvar p;

`define CORNETTO_DUT_PARK(P) \
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

    `CORNETTO_DUT_PARK(0)
    `CORNETTO_DUT_PARK(1)

`undef CORNETTO_DUT_PARK

endmodule
