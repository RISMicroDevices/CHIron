// chi_top.sv
// Top-level testbench module for CHI RN verification.
//
// This module:
//   1. Instantiates chi_if and connects it to the DUT
//   2. Provides the virtual interface to UVM via uvm_config_db
//   3. Calls run_test() to start UVM
//
// Replace `chi_rn_dut` with your actual DUT module name and port map.

`ifndef CHI_TOP_SV
`define CHI_TOP_SV

`include "uvm_macros.svh"
`include "chi_pkg.sv"
`include "chi_if.sv"
`include "chi_seq_item.sv"
`include "chi_driver.sv"
`include "chi_monitor.sv"
`include "chi_agent.sv"
`include "chi_scoreboard.sv"
`include "chi_coverage.sv"
`include "chi_seq_lib.sv"
`include "chi_env.sv"
`include "chi_test.sv"

import uvm_pkg::*;
import chi_pkg::*;

module chi_top;

  // -------------------------------------------------------------------------
  // Clock and reset generation
  // -------------------------------------------------------------------------
  logic clk;
  logic rst_n;

  // 1 GHz clock
  initial clk = 1'b0;
  always #0.5ns clk = ~clk;

  // Active-low reset: assert for 10 cycles
  initial begin
    rst_n = 1'b0;
    repeat (10) @(posedge clk);
    rst_n = 1'b1;
  end

  // -------------------------------------------------------------------------
  // CHI interface instance
  // -------------------------------------------------------------------------
  chi_if #(
    .NODEID_W (chi_pkg::NODEID_W),
    .ADDR_W   (chi_pkg::ADDR_W),
    .DATA_W   (chi_pkg::DATA_W),
    .RSVDC_W  (chi_pkg::RSVDC_W)
  ) chi_bus (
    .clk  (clk),
    .rst_n(rst_n)
  );

  // -------------------------------------------------------------------------
  // DUT instantiation
  // Replace this section with your actual DUT port connections.
  // -------------------------------------------------------------------------
  /*
  chi_rn_dut dut (
    .clk              (clk),
    .rst_n            (rst_n),

    // TXREQ
    .txreq_flitpend   (chi_bus.txreq_flitpend),
    .txreq_flitv      (chi_bus.txreq_flitv),
    .txreq_opcode     (chi_bus.txreq_opcode),
    .txreq_txnid      (chi_bus.txreq_txnid),
    .txreq_srcid      (chi_bus.txreq_srcid),
    .txreq_tgtid      (chi_bus.txreq_tgtid),
    .txreq_addr       (chi_bus.txreq_addr),
    .txreq_size       (chi_bus.txreq_size),
    .txreq_qos        (chi_bus.txreq_qos),
    .txreq_mem_attr   (chi_bus.txreq_mem_attr),
    .txreq_ns         (chi_bus.txreq_ns),
    .txreq_allow_retry(chi_bus.txreq_allow_retry),
    .txreq_order      (chi_bus.txreq_order),
    .txreq_pcrd_type  (chi_bus.txreq_pcrd_type),
    .txreq_lcrdv      (chi_bus.txreq_lcrdv),

    // TXRSP
    .txrsp_flitpend   (chi_bus.txrsp_flitpend),
    .txrsp_flitv      (chi_bus.txrsp_flitv),
    .txrsp_opcode     (chi_bus.txrsp_opcode),
    .txrsp_txnid      (chi_bus.txrsp_txnid),
    .txrsp_srcid      (chi_bus.txrsp_srcid),
    .txrsp_tgtid      (chi_bus.txrsp_tgtid),
    .txrsp_resp_err   (chi_bus.txrsp_resp_err),
    .txrsp_resp       (chi_bus.txrsp_resp),
    .txrsp_dbid       (chi_bus.txrsp_dbid),
    .txrsp_lcrdv      (chi_bus.txrsp_lcrdv),

    // TXDAT
    .txdat_flitpend   (chi_bus.txdat_flitpend),
    .txdat_flitv      (chi_bus.txdat_flitv),
    .txdat_opcode     (chi_bus.txdat_opcode),
    .txdat_txnid      (chi_bus.txdat_txnid),
    .txdat_srcid      (chi_bus.txdat_srcid),
    .txdat_tgtid      (chi_bus.txdat_tgtid),
    .txdat_be         (chi_bus.txdat_be),
    .txdat_data       (chi_bus.txdat_data),
    .txdat_lcrdv      (chi_bus.txdat_lcrdv),

    // RXRSP
    .rxrsp_flitpend   (chi_bus.rxrsp_flitpend),
    .rxrsp_flitv      (chi_bus.rxrsp_flitv),
    .rxrsp_opcode     (chi_bus.rxrsp_opcode),
    .rxrsp_txnid      (chi_bus.rxrsp_txnid),
    .rxrsp_srcid      (chi_bus.rxrsp_srcid),
    .rxrsp_tgtid      (chi_bus.rxrsp_tgtid),
    .rxrsp_resp       (chi_bus.rxrsp_resp),
    .rxrsp_dbid       (chi_bus.rxrsp_dbid),
    .rxrsp_lcrdv      (chi_bus.rxrsp_lcrdv),

    // RXDAT
    .rxdat_flitpend   (chi_bus.rxdat_flitpend),
    .rxdat_flitv      (chi_bus.rxdat_flitv),
    .rxdat_opcode     (chi_bus.rxdat_opcode),
    .rxdat_txnid      (chi_bus.rxdat_txnid),
    .rxdat_home_nid   (chi_bus.rxdat_home_nid),
    .rxdat_data       (chi_bus.rxdat_data),
    .rxdat_lcrdv      (chi_bus.rxdat_lcrdv),

    // RXSNP
    .rxsnp_flitpend   (chi_bus.rxsnp_flitpend),
    .rxsnp_flitv      (chi_bus.rxsnp_flitv),
    .rxsnp_opcode     (chi_bus.rxsnp_opcode),
    .rxsnp_txnid      (chi_bus.rxsnp_txnid),
    .rxsnp_srcid      (chi_bus.rxsnp_srcid),
    .rxsnp_addr       (chi_bus.rxsnp_addr),
    .rxsnp_lcrdv      (chi_bus.rxsnp_lcrdv)
  );
  */

  // -------------------------------------------------------------------------
  // UVM setup
  // -------------------------------------------------------------------------
  initial begin
    // Pass the virtual interface to the UVM environment
    uvm_config_db #(virtual chi_if)::set(null, "uvm_test_top.*", "vif", chi_bus);

    // Start the test (test name provided via +UVM_TESTNAME)
    run_test();
  end

  // -------------------------------------------------------------------------
  // Timeout watchdog — adjust as needed
  // -------------------------------------------------------------------------
  initial begin
    #10ms;
    `uvm_fatal("TIMEOUT", "Simulation timeout — possible hang")
  end

endmodule : chi_top

`endif // CHI_TOP_SV
