// chi_test.sv
// Example UVM tests for CHI RN verification.
//
// Tests:
//   chi_base_test     — base test (env setup, no stimulus)
//   chi_read_test     — runs multiple ReadShared sequences
//   chi_write_test    — runs multiple WriteUniqueFull sequences
//   chi_rand_test     — randomised mix of all transaction types

`ifndef CHI_TEST_SV
`define CHI_TEST_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

// ===========================================================================
// chi_base_test
// ===========================================================================
class chi_base_test extends uvm_test;
  `uvm_component_utils(chi_base_test)

  chi_env env;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    // Create environment
    env = chi_env::type_id::create("env", this);

    // Configure the virtual interface (set in chi_top.sv before run_test)
    // uvm_config_db is typically populated from the top-level module.

    // Configure CLog logging
    uvm_config_db #(bit)::set   (this, "env.agent.monitor", "log_enable", 1);
    uvm_config_db #(string)::set(this, "env.agent.monitor", "log_path",   "chi_sim.clog");
    uvm_config_db #(int)::set   (this, "env.agent.monitor", "nodeid",     1);
  endfunction

  task run_phase(uvm_phase phase);
    phase.raise_objection(this);
    // Base test does nothing — sub-classes override
    #100ns;
    phase.drop_objection(this);
  endtask

  function void report_phase(uvm_phase phase);
    uvm_report_server rs = uvm_report_server::get_server();
    if (rs.get_severity_count(UVM_ERROR) > 0 ||
        rs.get_severity_count(UVM_FATAL) > 0)
      `uvm_info("TEST", "*** TEST FAILED ***", UVM_NONE)
    else
      `uvm_info("TEST", "*** TEST PASSED ***", UVM_NONE)
  endfunction

endclass : chi_base_test

// ===========================================================================
// chi_read_test — ReadShared transactions
// ===========================================================================
class chi_read_test extends chi_base_test;
  `uvm_component_utils(chi_read_test)

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  task run_phase(uvm_phase phase);
    chi_read_shared_seq seq;
    phase.raise_objection(this);

    seq = chi_read_shared_seq::type_id::create("seq");
    seq.src_id = 7'd1;
    seq.tgt_id = 7'd8;

    repeat (10) begin
      if (!seq.randomize() with { addr[5:0] == 6'h0; })
        `uvm_fatal("TEST", "Randomisation failure")
      seq.txn_id++;
      seq.start(env.agent.sequencer);
    end

    #200ns;
    phase.drop_objection(this);
  endtask

endclass : chi_read_test

// ===========================================================================
// chi_write_test — WriteUniqueFull transactions
// ===========================================================================
class chi_write_test extends chi_base_test;
  `uvm_component_utils(chi_write_test)

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  task run_phase(uvm_phase phase);
    chi_write_unique_seq seq;
    phase.raise_objection(this);

    seq = chi_write_unique_seq::type_id::create("seq");
    seq.src_id = 7'd1;
    seq.tgt_id = 7'd8;

    repeat (10) begin
      if (!seq.randomize() with { addr[5:0] == 6'h0; })
        `uvm_fatal("TEST", "Randomisation failure")
      seq.txn_id++;
      seq.start(env.agent.sequencer);
    end

    #200ns;
    phase.drop_objection(this);
  endtask

endclass : chi_write_test

// ===========================================================================
// chi_rand_test — randomised mix
// ===========================================================================
class chi_rand_test extends chi_base_test;
  `uvm_component_utils(chi_rand_test)

  int unsigned num_transactions = 100;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    // Read num_transactions from +UVM_TESTARGS or command line
    void'($value$plusargs("NUM_TRANSACTIONS=%0d", num_transactions));
  endfunction

  task run_phase(uvm_phase phase);
    chi_rand_seq seq;
    phase.raise_objection(this);

    seq = chi_rand_seq::type_id::create("seq");
    seq.src_id           = 7'd1;
    seq.tgt_id           = 7'd8;
    seq.num_transactions = num_transactions;

    if (!seq.randomize())
      `uvm_fatal("TEST", "Randomisation failure")

    seq.start(env.agent.sequencer);

    #500ns;
    phase.drop_objection(this);
  endtask

endclass : chi_rand_test

`endif // CHI_TEST_SV
