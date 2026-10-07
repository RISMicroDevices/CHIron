//*****************************************************************************
// lint_stub_top.sv — lint stand-in for the DUT top: an avocado_top-shaped
// empty shell whose port list is generated from the SAME contract macros
// (cohestra_packages/avocado/integration/avocado_dut_if.svh) that define the
// real boundary, so linting cohestra_vcs_bridge.sv against it checks the
// bridge's signal declarations, connections and DPI pack/unpack against the
// true pin contract.
//
// Usage (host-side syntax check, no VCS needed):
//   $ verilator --lint-only --timing -Wno-fatal \
//       +define+COHESTRA_VCS_TOP_MODULE=lint_stub_top \
//       +incdir+<chiron>/cchi/cohestra/cohestra_vcs \
//       +incdir+<chiron>/cchi/cohestra/cohestra_packages/avocado/integration \
//       lint_stub_top.sv <...>/cohestra_vcs_bridge.sv \
//       --top-module cohestra_vcs_bridge
//*****************************************************************************

`timescale 1ps/1ps
`default_nettype none

`include "avocado_dut_if.svh"

module lint_stub_top (
    input  logic clock,
    input  logic reset,

    `AVOCADO_CCHI_T1_PORT_DECL(0),
    `AVOCADO_CCHI_T1_PORT_DECL(1),
    `AVOCADO_CCHI_T1_PORT_DECL(2),
    `AVOCADO_CCHI_T1_PORT_DECL(3),

    `AVOCADO_AXI_M_PORT_DECL(0),
    `AVOCADO_AXI_M_PORT_DECL(1),

    `AVOCADO_AXI_S_PORT_DECL(0)
);

    // Empty shell: the outputs float (X under 4-state); lint only checks the
    // structural contract.

endmodule

`default_nettype wire
