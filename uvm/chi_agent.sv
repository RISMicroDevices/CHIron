// chi_agent.sv
// UVM agent encapsulating the CHI driver, monitor, and sequencer.
//
// The agent can operate in active (UVM_ACTIVE) or passive (UVM_PASSIVE) mode:
//   - Active:  driver + sequencer + monitor
//   - Passive: monitor only
//
// The analysis port from the internal monitor is re-exported so that
// higher-level components (scoreboard, coverage) can connect to it.

`ifndef CHI_AGENT_SV
`define CHI_AGENT_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

class chi_agent extends uvm_agent;
  `uvm_component_utils(chi_agent)

  // Sub-components
  chi_driver                    driver;
  chi_monitor                   monitor;
  uvm_sequencer #(chi_seq_item) sequencer;

  // Re-exported analysis port from the monitor
  uvm_analysis_port #(chi_seq_item) ap;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    // The monitor is always created (passive or active)
    monitor = chi_monitor::type_id::create("monitor", this);

    if (get_is_active() == UVM_ACTIVE) begin
      driver    = chi_driver::type_id::create("driver",    this);
      sequencer = uvm_sequencer #(chi_seq_item)::type_id::create("sequencer", this);
    end
  endfunction

  function void connect_phase(uvm_phase phase);
    // Export the monitor analysis port at agent level
    ap = monitor.ap;

    if (get_is_active() == UVM_ACTIVE)
      driver.seq_item_port.connect(sequencer.seq_item_export);
  endfunction

endclass : chi_agent

`endif // CHI_AGENT_SV
