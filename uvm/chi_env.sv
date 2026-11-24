// chi_env.sv
// UVM environment for a single-RN CHI verification setup.
//
// Topology:
//   +-----------------------------------------------+
//   |  chi_env                                      |
//   |                                               |
//   |  chi_agent (active, RN side)                  |
//   |    ├── sequencer                              |
//   |    ├── driver  ──────────────────► DUT (RN)  |
//   |    └── monitor ──────────────────► analysis  |
//   |                                     │         |
//   |  chi_scoreboard ◄───────────────────┤         |
//   |  chi_coverage   ◄───────────────────┘         |
//   +-----------------------------------------------+

`ifndef CHI_ENV_SV
`define CHI_ENV_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

class chi_env extends uvm_env;
  `uvm_component_utils(chi_env)

  chi_agent        agent;
  chi_scoreboard   scoreboard;
  chi_coverage     coverage;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    agent      = chi_agent::type_id::create("agent",      this);
    scoreboard = chi_scoreboard::type_id::create("scoreboard", this);
    coverage   = chi_coverage::type_id::create("coverage",    this);
  endfunction

  function void connect_phase(uvm_phase phase);
    // Connect monitor analysis port to scoreboard and coverage
    agent.ap.connect(scoreboard.analysis_export);
    agent.ap.connect(coverage.analysis_export);
  endfunction

endclass : chi_env

`endif // CHI_ENV_SV
