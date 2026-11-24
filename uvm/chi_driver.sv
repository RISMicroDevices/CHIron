// chi_driver.sv
// UVM driver for a CHI RN (Requester Node) under test.
//
// Drives the TXREQ, TXRSP, and TXDAT channels based on sequence items, and
// returns link credits on the RX channels.  The driver respects the CHI
// link-layer credit protocol: it only fires a flit on a channel when at least
// one credit is available (txreq_lcrdv / txrsp_lcrdv / txdat_lcrdv).
//
// For a model that controls an HN or SN instead of an RN, swap the TX/RX
// direction references and adjust the clocking block inputs accordingly.

`ifndef CHI_DRIVER_SV
`define CHI_DRIVER_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

class chi_driver extends uvm_driver #(chi_seq_item);
  `uvm_component_utils(chi_driver)

  // Virtual interface handle; set by the agent via uvm_config_db
  virtual chi_if vif;

  // Credit counts per TX channel
  int unsigned req_credits = 0;
  int unsigned rsp_credits = 0;
  int unsigned dat_credits = 0;

  // Initial RX credits to return to HN on start-up (typically 4 per channel)
  int unsigned init_rxrsp_credits = 4;
  int unsigned init_rxdat_credits = 4;
  int unsigned init_rxsnp_credits = 4;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    if (!uvm_config_db #(virtual chi_if)::get(this, "", "vif", vif))
      `uvm_fatal("CHI_DRIVER", "No virtual interface found in uvm_config_db")
  endfunction

  task run_phase(uvm_phase phase);
    // De-assert all TX valid signals at reset
    deassert_all();
    @(posedge vif.clk iff vif.rst_n === 1'b1);

    fork
      credit_monitor_req();
      credit_monitor_rsp();
      credit_monitor_dat();
      return_rx_credits();
      drive_items();
    join
  endtask

  // ---------------------------------------------------------------------------
  // Credit monitors — count incoming TX link credits from HN
  // ---------------------------------------------------------------------------
  task credit_monitor_req();
    forever begin
      @(vif.rn_driver_cb);
      if (vif.rn_driver_cb.txreq_lcrdv)
        req_credits++;
    end
  endtask

  task credit_monitor_rsp();
    forever begin
      @(vif.rn_driver_cb);
      if (vif.rn_driver_cb.txrsp_lcrdv)
        rsp_credits++;
    end
  endtask

  task credit_monitor_dat();
    forever begin
      @(vif.rn_driver_cb);
      if (vif.rn_driver_cb.txdat_lcrdv)
        dat_credits++;
    end
  endtask

  // ---------------------------------------------------------------------------
  // Return RX link credits to the HN at simulation start
  // ---------------------------------------------------------------------------
  task return_rx_credits();
    // Pulse rxrsp_lcrdv / rxdat_lcrdv / rxsnp_lcrdv for each initial credit.
    // CHI spec allows returning up to 15 credits; 4 per channel is typical.
    repeat (init_rxrsp_credits) begin
      @(vif.rn_driver_cb);
      vif.rn_driver_cb.rxrsp_lcrdv <= 1'b1;
      @(vif.rn_driver_cb);
      vif.rn_driver_cb.rxrsp_lcrdv <= 1'b0;
    end
    repeat (init_rxdat_credits) begin
      @(vif.rn_driver_cb);
      vif.rn_driver_cb.rxdat_lcrdv <= 1'b1;
      @(vif.rn_driver_cb);
      vif.rn_driver_cb.rxdat_lcrdv <= 1'b0;
    end
    repeat (init_rxsnp_credits) begin
      @(vif.rn_driver_cb);
      vif.rn_driver_cb.rxsnp_lcrdv <= 1'b1;
      @(vif.rn_driver_cb);
      vif.rn_driver_cb.rxsnp_lcrdv <= 1'b0;
    end
  endtask

  // ---------------------------------------------------------------------------
  // Main driving loop — pull items from the sequencer and drive them
  // ---------------------------------------------------------------------------
  task drive_items();
    chi_seq_item item;
    forever begin
      seq_item_port.get_next_item(item);
      case (item.channel)
        chi_seq_item::CH_TXREQ: drive_txreq(item);
        chi_seq_item::CH_TXRSP: drive_txrsp(item);
        chi_seq_item::CH_TXDAT: drive_txdat(item);
        default: `uvm_error("CHI_DRIVER", $sformatf("Unsupported channel %s in driver", item.channel.name()))
      endcase
      seq_item_port.item_done();
    end
  endtask

  // ---------------------------------------------------------------------------
  // Drive TXREQ (wait for credit, assert flitpend one cycle early, then flit)
  // ---------------------------------------------------------------------------
  task drive_txreq(chi_seq_item item);
    // Wait for a credit
    wait (req_credits > 0);
    req_credits--;

    // Assert flitpend one cycle before the flit (§B-46)
    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txreq_flitpend   <= 1'b1;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txreq_flitv      <= 1'b1;
    vif.rn_driver_cb.txreq_opcode     <= item.opcode[6:0];
    vif.rn_driver_cb.txreq_txnid      <= item.txnid;
    vif.rn_driver_cb.txreq_srcid      <= item.srcid;
    vif.rn_driver_cb.txreq_tgtid      <= item.tgtid;
    vif.rn_driver_cb.txreq_addr       <= item.addr;
    vif.rn_driver_cb.txreq_size       <= item.size;
    vif.rn_driver_cb.txreq_qos        <= item.qos;
    vif.rn_driver_cb.txreq_mem_attr   <= item.mem_attr;
    vif.rn_driver_cb.txreq_ns         <= item.ns;
    vif.rn_driver_cb.txreq_allow_retry<= item.allow_retry;
    vif.rn_driver_cb.txreq_order      <= item.order;
    vif.rn_driver_cb.txreq_pcrd_type  <= item.pcrd_type;
    vif.rn_driver_cb.txreq_snp_attr   <= item.snp_attr;
    vif.rn_driver_cb.txreq_excl       <= item.excl;
    vif.rn_driver_cb.txreq_trace_tag  <= item.trace_tag;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txreq_flitv      <= 1'b0;
    vif.rn_driver_cb.txreq_flitpend   <= 1'b0;
  endtask

  // ---------------------------------------------------------------------------
  // Drive TXRSP
  // ---------------------------------------------------------------------------
  task drive_txrsp(chi_seq_item item);
    wait (rsp_credits > 0);
    rsp_credits--;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txrsp_flitpend   <= 1'b1;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txrsp_flitv      <= 1'b1;
    vif.rn_driver_cb.txrsp_opcode     <= item.opcode[4:0];
    vif.rn_driver_cb.txrsp_txnid      <= item.txnid;
    vif.rn_driver_cb.txrsp_srcid      <= item.srcid;
    vif.rn_driver_cb.txrsp_tgtid      <= item.tgtid;
    vif.rn_driver_cb.txrsp_resp_err   <= item.resp_err;
    vif.rn_driver_cb.txrsp_resp       <= item.resp;
    vif.rn_driver_cb.txrsp_dbid       <= item.dbid;
    vif.rn_driver_cb.txrsp_pcrd_type  <= item.pcrd_type;
    vif.rn_driver_cb.txrsp_qos        <= item.qos;
    vif.rn_driver_cb.txrsp_trace_tag  <= item.trace_tag;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txrsp_flitv      <= 1'b0;
    vif.rn_driver_cb.txrsp_flitpend   <= 1'b0;
  endtask

  // ---------------------------------------------------------------------------
  // Drive TXDAT (WriteData — may require two beats for 256-bit data)
  // ---------------------------------------------------------------------------
  task drive_txdat(chi_seq_item item);
    wait (dat_credits > 0);
    dat_credits--;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txdat_flitpend   <= 1'b1;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txdat_flitv      <= 1'b1;
    vif.rn_driver_cb.txdat_opcode     <= item.opcode[3:0];
    vif.rn_driver_cb.txdat_txnid      <= item.txnid;
    vif.rn_driver_cb.txdat_srcid      <= item.srcid;
    vif.rn_driver_cb.txdat_tgtid      <= item.tgtid;
    vif.rn_driver_cb.txdat_home_nid   <= item.home_nid;
    vif.rn_driver_cb.txdat_resp_err   <= item.resp_err;
    vif.rn_driver_cb.txdat_resp       <= item.resp;
    vif.rn_driver_cb.txdat_dbid       <= item.dbid;
    vif.rn_driver_cb.txdat_ccid       <= item.ccid;
    vif.rn_driver_cb.txdat_data_id    <= item.data_id;
    vif.rn_driver_cb.txdat_qos        <= item.qos;
    vif.rn_driver_cb.txdat_be         <= item.be;
    vif.rn_driver_cb.txdat_data       <= item.data;
    vif.rn_driver_cb.txdat_trace_tag  <= item.trace_tag;

    @(vif.rn_driver_cb);
    vif.rn_driver_cb.txdat_flitv      <= 1'b0;
    vif.rn_driver_cb.txdat_flitpend   <= 1'b0;
  endtask

  // ---------------------------------------------------------------------------
  // De-assert everything
  // ---------------------------------------------------------------------------
  task deassert_all();
    vif.txreq_flitpend <= 0; vif.txreq_flitv <= 0;
    vif.txrsp_flitpend <= 0; vif.txrsp_flitv <= 0;
    vif.txdat_flitpend <= 0; vif.txdat_flitv <= 0;
    vif.rxrsp_lcrdv    <= 0;
    vif.rxdat_lcrdv    <= 0;
    vif.rxsnp_lcrdv    <= 0;
  endtask

endclass : chi_driver

`endif // CHI_DRIVER_SV
