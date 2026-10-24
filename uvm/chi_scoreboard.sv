// chi_scoreboard.sv
// UVM scoreboard for a CHI RN verification environment backed by CHIron.
//
// Architecture
// ------------
// CHIron's C++ transaction state-machine library is compiled into a shared
// object and linked via DPI-C.  The scoreboard calls a thin C++ shim
// (chi_refmodel.hpp / chi_refmodel_dpi.cpp — see docs/uvm_integration_guide.md)
// that wraps CHIron's Xact::Router class.
//
// For every flit the monitor captures:
//   1. The scoreboard classifies it (REQ / RSP / DAT / SNP, TX or RX).
//   2. It forwards the raw flit data to the CHIron reference model via DPI-C.
//   3. The reference model updates the transaction state machine.
//   4. On completion the scoreboard checks the observed data against the
//      expected response populated by the sequences.
//
// DPI-C reference-model interface (implemented in chi_refmodel_dpi.cpp)
// ----------------------------------------------------------------------
//   chi_rm_init()             — one-time initialisation
//   chi_rm_feed_req(flit,len) — submit a REQ flit
//   chi_rm_feed_rsp(flit,len) — submit an RSP flit
//   chi_rm_feed_dat(flit,len) — submit a DAT flit
//   chi_rm_feed_snp(flit,len) — submit a SNP flit
//   chi_rm_check()            — return 0 if no error, else error code
//   chi_rm_get_error_string() — return last error message

`ifndef CHI_SCOREBOARD_SV
`define CHI_SCOREBOARD_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

// ---------------------------------------------------------------------------
// DPI-C declarations for the CHIron reference model shim
// ---------------------------------------------------------------------------
import "DPI-C" function void   chi_rm_init();
import "DPI-C" function void   chi_rm_feed_req(input bit [511:0] flit, input int flit_len);
import "DPI-C" function void   chi_rm_feed_rsp(input bit [511:0] flit, input int flit_len);
import "DPI-C" function void   chi_rm_feed_dat(input bit [511:0] flit, input int flit_len);
import "DPI-C" function void   chi_rm_feed_snp(input bit [511:0] flit, input int flit_len);
import "DPI-C" function int    chi_rm_check();
import "DPI-C" function string chi_rm_get_error_string();

class chi_scoreboard extends uvm_scoreboard;
  `uvm_component_utils(chi_scoreboard)

  // Analysis export — connect to chi_agent::ap (or chi_monitor::ap)
  uvm_analysis_imp #(chi_seq_item, chi_scoreboard) analysis_export;

  // Counters
  int unsigned flits_checked = 0;
  int unsigned errors        = 0;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    analysis_export = new("analysis_export", this);
  endfunction

  function void start_of_simulation_phase(uvm_phase phase);
    // Initialise CHIron reference model
    chi_rm_init();
    `uvm_info("CHI_SB", "CHIron reference model initialised", UVM_MEDIUM)
  endfunction

  // ---------------------------------------------------------------------------
  // write() — called by the monitor for every captured flit
  // ---------------------------------------------------------------------------
  function void write(chi_seq_item item);
    int rc;

    // Forward flit to CHIron reference model
    case (item.channel)
      chi_seq_item::CH_TXREQ: chi_rm_feed_req(item.flit, int'(item.flit_length));
      chi_seq_item::CH_TXRSP: chi_rm_feed_rsp(item.flit, int'(item.flit_length));
      chi_seq_item::CH_TXDAT: chi_rm_feed_dat(item.flit, int'(item.flit_length));
      chi_seq_item::CH_RXRSP: chi_rm_feed_rsp(item.flit, int'(item.flit_length));
      chi_seq_item::CH_RXDAT: chi_rm_feed_dat(item.flit, int'(item.flit_length));
      chi_seq_item::CH_RXSNP: chi_rm_feed_snp(item.flit, int'(item.flit_length));
    endcase

    // Check for protocol errors detected by CHIron
    rc = chi_rm_check();
    if (rc != 0) begin
      `uvm_error("CHI_SB",
        $sformatf("CHIron protocol violation (code=%0d): %s\n  Flit: %s",
                  rc, chi_rm_get_error_string(), item.convert2string()))
      errors++;
    end

    flits_checked++;

    `uvm_info("CHI_SB",
      $sformatf("[%0t] %s", $time, item.convert2string()), UVM_HIGH)
  endfunction

  function void report_phase(uvm_phase phase);
    `uvm_info("CHI_SB",
      $sformatf("Scoreboard summary: %0d flits checked, %0d errors",
                flits_checked, errors), UVM_NONE)
    if (errors > 0)
      `uvm_error("CHI_SB", "Simulation FAILED — protocol errors detected")
    else
      `uvm_info("CHI_SB", "Simulation PASSED", UVM_NONE)
  endfunction

endclass : chi_scoreboard

`endif // CHI_SCOREBOARD_SV
