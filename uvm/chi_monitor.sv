// chi_monitor.sv
// UVM monitor for the CHI bus.
//
// Observes all six active channels (TXREQ, TXRSP, TXDAT, RXRSP, RXDAT, RXSNP)
// and publishes chi_seq_item transactions on the analysis port.
//
// CHIron CLog integration
// -----------------------
// When LOG_ENABLE is 1 the monitor calls the CLog.B DPI-C functions
// (from clog/clog_b/clogdpi_b.svh) to log every flit captured.  The log
// file can be post-processed by the CHIron app tools (clog2log, clog2coverage,
// clog2mkcfg, report_flit).
//
// Set the following uvm_config_db keys before build_phase:
//   "log_enable"   (bit)   — 0/1 (default 1)
//   "log_path"     (string)— path for the CLog binary file
//   "nodeid"       (int)   — node ID reported in log records

`ifndef CHI_MONITOR_SV
`define CHI_MONITOR_SV

`include "uvm_macros.svh"
`include "../clog/clog_b/clogdpi_b.svh"

import uvm_pkg::*;
import chi_pkg::*;

class chi_monitor extends uvm_monitor;
  `uvm_component_utils(chi_monitor)

  // Analysis port — broadcasts captured items to scoreboard, coverage, etc.
  uvm_analysis_port #(chi_seq_item) ap;

  // Virtual interface
  virtual chi_if vif;

  // CLog configuration
  bit          log_enable = 1;
  string       log_path   = "chi_sim.clog";
  int unsigned nodeid_cfg = 0;

  // CLog file handle (chandle is a DPI opaque pointer)
  chandle      clog_handle;

  // Tracks simulation cycle count
  longint unsigned cycle_count = 0;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    ap = new("ap", this);

    if (!uvm_config_db #(virtual chi_if)::get(this, "", "vif", vif))
      `uvm_fatal("CHI_MONITOR", "No virtual interface found in uvm_config_db")

    void'(uvm_config_db #(bit)::get(this, "", "log_enable", log_enable));
    void'(uvm_config_db #(string)::get(this, "", "log_path",   log_path));
    void'(uvm_config_db #(int)::get(this, "", "nodeid",     nodeid_cfg));
  endfunction

  function void start_of_simulation_phase(uvm_phase phase);
    if (log_enable) begin
      int status;
      clog_handle = CLogB_OpenFile(log_path, status);
      if (status != 0 || clog_handle == null)
        `uvm_fatal("CHI_MONITOR", $sformatf("Failed to open CLog file: %s (status=%0d)", log_path, status))

      // Write protocol parameters matching our FlitConfiguration
      CLogB_WriteParameters(
        clog_handle,
        int'(chi_pkg::CLOG_ISSUE_E), // issue
        NODEID_W,               // nodeIdWidth
        ADDR_W,                 // addrWidth
        RSVDC_W,                // reqRsvdcWidth
        RSVDC_W,                // datRsvdcWidth
        DATA_W,                 // dataWidth
        0,                      // dataCheckPresent
        0,                      // poisonPresent
        0                       // mpamPresent
      );

      // Write topology: one RN-F node (extend as needed for your system)
      CLogB_WriteTopo(clog_handle, nodeid_cfg, `CLOG_NODE_TYPE_RN_F);
      CLogB_WriteTopoEnd(clog_handle);
    end
  endfunction

  function void final_phase(uvm_phase phase);
    if (log_enable && clog_handle != null)
      CLogB_CloseFile(clog_handle);
  endfunction

  task run_phase(uvm_phase phase);
    @(posedge vif.clk iff vif.rst_n === 1'b1);
    fork
      count_cycles();
      monitor_txreq();
      monitor_txrsp();
      monitor_txdat();
      monitor_rxrsp();
      monitor_rxdat();
      monitor_rxsnp();
    join
  endtask

  // ---------------------------------------------------------------------------
  // Cycle counter
  // ---------------------------------------------------------------------------
  task count_cycles();
    forever begin
      @(vif.monitor_cb);
      cycle_count++;
    end
  endtask

  // ---------------------------------------------------------------------------
  // TXREQ monitor
  // ---------------------------------------------------------------------------
  task monitor_txreq();
    chi_seq_item item;
    forever begin
      @(vif.monitor_cb);
      if (vif.monitor_cb.txreq_flitv) begin
        item = chi_seq_item::type_id::create("txreq_item");
        item.channel    = chi_seq_item::CH_TXREQ;
        item.opcode     = vif.monitor_cb.txreq_opcode;
        item.txnid      = vif.monitor_cb.txreq_txnid;
        item.srcid      = vif.monitor_cb.txreq_srcid;
        item.tgtid      = vif.monitor_cb.txreq_tgtid;
        item.addr       = vif.monitor_cb.txreq_addr;
        item.size       = vif.monitor_cb.txreq_size;
        item.qos        = vif.monitor_cb.txreq_qos;
        item.mem_attr   = vif.monitor_cb.txreq_mem_attr;
        item.ns         = vif.monitor_cb.txreq_ns;
        item.allow_retry= vif.monitor_cb.txreq_allow_retry;
        item.order      = vif.monitor_cb.txreq_order;
        item.pcrd_type  = vif.monitor_cb.txreq_pcrd_type;
        item.snp_attr   = vif.monitor_cb.txreq_snp_attr;
        item.excl       = vif.monitor_cb.txreq_excl;
        item.trace_tag  = vif.monitor_cb.txreq_trace_tag;
        item.cycle      = cycle_count;
        item.nodeid     = nodeid_cfg;
        item.clog_channel = CLOG_CH_TXREQ;

        pack_req_flit(item);
        log_flit(item);
        ap.write(item);
      end
    end
  endtask

  // ---------------------------------------------------------------------------
  // TXRSP monitor
  // ---------------------------------------------------------------------------
  task monitor_txrsp();
    chi_seq_item item;
    forever begin
      @(vif.monitor_cb);
      if (vif.monitor_cb.txrsp_flitv) begin
        item = chi_seq_item::type_id::create("txrsp_item");
        item.channel    = chi_seq_item::CH_TXRSP;
        item.opcode     = {2'b0, vif.monitor_cb.txrsp_opcode};
        item.txnid      = vif.monitor_cb.txrsp_txnid;
        item.srcid      = vif.monitor_cb.txrsp_srcid;
        item.tgtid      = vif.monitor_cb.txrsp_tgtid;
        item.resp_err   = vif.monitor_cb.txrsp_resp_err;
        item.resp       = vif.monitor_cb.txrsp_resp;
        item.dbid       = vif.monitor_cb.txrsp_dbid;
        item.pcrd_type  = vif.monitor_cb.txrsp_pcrd_type;
        item.qos        = vif.monitor_cb.txrsp_qos;
        item.trace_tag  = vif.monitor_cb.txrsp_trace_tag;
        item.cycle      = cycle_count;
        item.nodeid     = nodeid_cfg;
        item.clog_channel = CLOG_CH_TXRSP;

        pack_rsp_flit(item);
        log_flit(item);
        ap.write(item);
      end
    end
  endtask

  // ---------------------------------------------------------------------------
  // TXDAT monitor
  // ---------------------------------------------------------------------------
  task monitor_txdat();
    chi_seq_item item;
    forever begin
      @(vif.monitor_cb);
      if (vif.monitor_cb.txdat_flitv) begin
        item = chi_seq_item::type_id::create("txdat_item");
        item.channel    = chi_seq_item::CH_TXDAT;
        item.opcode     = {3'b0, vif.monitor_cb.txdat_opcode};
        item.txnid      = vif.monitor_cb.txdat_txnid;
        item.srcid      = vif.monitor_cb.txdat_srcid;
        item.tgtid      = vif.monitor_cb.txdat_tgtid;
        item.home_nid   = vif.monitor_cb.txdat_home_nid;
        item.resp_err   = vif.monitor_cb.txdat_resp_err;
        item.resp       = vif.monitor_cb.txdat_resp;
        item.dbid       = vif.monitor_cb.txdat_dbid;
        item.ccid       = vif.monitor_cb.txdat_ccid;
        item.data_id    = vif.monitor_cb.txdat_data_id;
        item.qos        = vif.monitor_cb.txdat_qos;
        item.be         = vif.monitor_cb.txdat_be;
        item.data       = vif.monitor_cb.txdat_data;
        item.trace_tag  = vif.monitor_cb.txdat_trace_tag;
        item.cycle      = cycle_count;
        item.nodeid     = nodeid_cfg;
        item.clog_channel = CLOG_CH_TXDAT;

        pack_dat_flit(item);
        log_flit(item);
        ap.write(item);
      end
    end
  endtask

  // ---------------------------------------------------------------------------
  // RXRSP monitor
  // ---------------------------------------------------------------------------
  task monitor_rxrsp();
    chi_seq_item item;
    forever begin
      @(vif.monitor_cb);
      if (vif.monitor_cb.rxrsp_flitv) begin
        item = chi_seq_item::type_id::create("rxrsp_item");
        item.channel    = chi_seq_item::CH_RXRSP;
        item.opcode     = {2'b0, vif.monitor_cb.rxrsp_opcode};
        item.txnid      = vif.monitor_cb.rxrsp_txnid;
        item.srcid      = vif.monitor_cb.rxrsp_srcid;
        item.tgtid      = vif.monitor_cb.rxrsp_tgtid;
        item.resp_err   = vif.monitor_cb.rxrsp_resp_err;
        item.resp       = vif.monitor_cb.rxrsp_resp;
        item.dbid       = vif.monitor_cb.rxrsp_dbid;
        item.pcrd_type  = vif.monitor_cb.rxrsp_pcrd_type;
        item.qos        = vif.monitor_cb.rxrsp_qos;
        item.fwd_state  = vif.monitor_cb.rxrsp_fwd_state;
        item.trace_tag  = vif.monitor_cb.rxrsp_trace_tag;
        item.cycle      = cycle_count;
        item.nodeid     = nodeid_cfg;
        item.clog_channel = CLOG_CH_RXRSP;

        pack_rsp_flit(item);
        log_flit(item);
        ap.write(item);
      end
    end
  endtask

  // ---------------------------------------------------------------------------
  // RXDAT monitor
  // ---------------------------------------------------------------------------
  task monitor_rxdat();
    chi_seq_item item;
    forever begin
      @(vif.monitor_cb);
      if (vif.monitor_cb.rxdat_flitv) begin
        item = chi_seq_item::type_id::create("rxdat_item");
        item.channel    = chi_seq_item::CH_RXDAT;
        item.opcode     = {3'b0, vif.monitor_cb.rxdat_opcode};
        item.txnid      = vif.monitor_cb.rxdat_txnid;
        item.srcid      = vif.monitor_cb.rxdat_srcid;
        item.tgtid      = vif.monitor_cb.rxdat_tgtid;
        item.home_nid   = vif.monitor_cb.rxdat_home_nid;
        item.resp_err   = vif.monitor_cb.rxdat_resp_err;
        item.resp       = vif.monitor_cb.rxdat_resp;
        item.dbid       = vif.monitor_cb.rxdat_dbid;
        item.ccid       = vif.monitor_cb.rxdat_ccid;
        item.data_id    = vif.monitor_cb.rxdat_data_id;
        item.qos        = vif.monitor_cb.rxdat_qos;
        item.be         = vif.monitor_cb.rxdat_be;
        item.data       = vif.monitor_cb.rxdat_data;
        item.fwd_state  = vif.monitor_cb.rxdat_fwd_state;
        item.trace_tag  = vif.monitor_cb.rxdat_trace_tag;
        item.cycle      = cycle_count;
        item.nodeid     = nodeid_cfg;
        item.clog_channel = CLOG_CH_RXDAT;

        pack_dat_flit(item);
        log_flit(item);
        ap.write(item);
      end
    end
  endtask

  // ---------------------------------------------------------------------------
  // RXSNP monitor
  // ---------------------------------------------------------------------------
  task monitor_rxsnp();
    chi_seq_item item;
    forever begin
      @(vif.monitor_cb);
      if (vif.monitor_cb.rxsnp_flitv) begin
        item = chi_seq_item::type_id::create("rxsnp_item");
        item.channel         = chi_seq_item::CH_RXSNP;
        item.opcode          = {1'b0, vif.monitor_cb.rxsnp_opcode};
        item.txnid           = vif.monitor_cb.rxsnp_txnid;
        item.srcid           = vif.monitor_cb.rxsnp_srcid;
        item.fwd_nid         = vif.monitor_cb.rxsnp_fwd_nid;
        item.fwd_txnid       = vif.monitor_cb.rxsnp_fwd_txnid;
        item.addr            = {{(ADDR_W - SNP_ADDR_W){1'b0}}, vif.monitor_cb.rxsnp_addr, 3'b0};
        item.ns              = vif.monitor_cb.rxsnp_ns;
        item.do_not_go_to_sd = vif.monitor_cb.rxsnp_do_not_go_to_sd;
        item.ret_to_src      = vif.monitor_cb.rxsnp_ret_to_src;
        item.qos             = vif.monitor_cb.rxsnp_qos;
        item.trace_tag       = vif.monitor_cb.rxsnp_trace_tag;
        item.cycle           = cycle_count;
        item.nodeid          = nodeid_cfg;
        item.clog_channel    = CLOG_CH_RXSNP;

        pack_snp_flit(item);
        log_flit(item);
        ap.write(item);
      end
    end
  endtask

  // ---------------------------------------------------------------------------
  // Flit packing helpers
  // Pack the decoded fields back into the 512-bit raw flit for CLog logging.
  // Bit positions follow CHI Issue E flit format with default parameters
  // (7-bit NodeID, 48-bit addr, 256-bit data, 4-bit RSVDC).
  //
  // NOTE: Exact field positions must match your FlitConfiguration; consult
  // chi/spec/chi_protocol_flits.hpp for the authoritative layout.
  // ---------------------------------------------------------------------------
  function void pack_req_flit(chi_seq_item item);
    logic [511:0] f = '0;
    // A simplified packing — adjust offsets for your exact FlitConfiguration.
    // Field order (LSB first): QoS[3:0], TgtID, SrcID, TxnID, ReturnNID,
    //   StashNIDValid, ReturnTxnID, StashNID, Endian, Deep, Opcode[6:0],
    //   Size[2:0], Addr[47:0], NS, LikelyShared, AllowRetry, Order[1:0],
    //   PCrdType[3:0], MemAttr[3:0], SnpAttr, DoDWT, LPID[4:0],
    //   Excl, SnoopMe, CAH, RSVDC, TraceTag
    int pos = 0;
    f[pos +: 4]            = item.qos;          pos += 4;
    f[pos +: NODEID_W]     = item.tgtid;        pos += NODEID_W;
    f[pos +: NODEID_W]     = item.srcid;        pos += NODEID_W;
    f[pos +: TXNID_W]      = item.txnid;        pos += TXNID_W;
    f[pos +: NODEID_W]     = '0;               pos += NODEID_W;  // ReturnNID
    f[pos]                 = '0;               pos += 1;          // StashNIDValid
    f[pos +: TXNID_W]      = '0;               pos += TXNID_W;   // ReturnTxnID
    f[pos +: NODEID_W]     = '0;               pos += NODEID_W;  // StashNID
    f[pos]                 = '0;               pos += 1;          // Endian
    f[pos]                 = '0;               pos += 1;          // Deep
    f[pos +: 7]            = item.opcode[6:0];  pos += 7;
    f[pos +: 3]            = item.size;         pos += 3;
    f[pos +: ADDR_W]       = item.addr;         pos += ADDR_W;
    f[pos]                 = item.ns;           pos += 1;
    f[pos]                 = '0;               pos += 1;          // LikelyShared
    f[pos]                 = item.allow_retry;  pos += 1;
    f[pos +: 2]            = item.order;        pos += 2;
    f[pos +: 4]            = item.pcrd_type;    pos += 4;
    f[pos +: 4]            = item.mem_attr;     pos += 4;
    f[pos]                 = item.snp_attr;     pos += 1;
    f[pos]                 = '0;               pos += 1;  // DoDWT
    f[pos +: 5]            = '0;               pos += 5;  // LPID
    f[pos]                 = item.excl;         pos += 1;
    f[pos]                 = '0;               pos += 1;  // SnoopMe
    f[pos]                 = '0;               pos += 1;  // CAH
    f[pos +: RSVDC_W]      = '0;               pos += RSVDC_W;
    f[pos]                 = item.trace_tag;
    item.flit        = f;
    item.flit_length = 88 + 4*NODEID_W + 2*TXNID_W + ADDR_W + RSVDC_W;
  endfunction

  function void pack_rsp_flit(chi_seq_item item);
    logic [511:0] f = '0;
    int pos = 0;
    f[pos +: 4]        = item.qos;          pos += 4;
    f[pos +: NODEID_W] = item.tgtid;        pos += NODEID_W;
    f[pos +: NODEID_W] = item.srcid;        pos += NODEID_W;
    f[pos +: TXNID_W]  = item.txnid;        pos += TXNID_W;
    f[pos +: 5]        = item.opcode[4:0];  pos += 5;
    f[pos +: 2]        = item.resp_err;     pos += 2;
    f[pos +: 3]        = item.resp;         pos += 3;
    f[pos +: 3]        = item.fwd_state;    pos += 3;
    f[pos +: TXNID_W]  = item.dbid;         pos += TXNID_W;
    f[pos +: 4]        = item.pcrd_type;    pos += 4;
    f[pos]             = item.trace_tag;
    item.flit        = f;
    item.flit_length = 32 + 2*NODEID_W + TXNID_W + TXNID_W;
  endfunction

  function void pack_dat_flit(chi_seq_item item);
    logic [511:0] f = '0;
    int pos = 0;
    f[pos +: 4]        = item.qos;          pos += 4;
    f[pos +: NODEID_W] = item.tgtid;        pos += NODEID_W;
    f[pos +: NODEID_W] = item.srcid;        pos += NODEID_W;
    f[pos +: TXNID_W]  = item.txnid;        pos += TXNID_W;
    f[pos +: NODEID_W] = item.home_nid;     pos += NODEID_W;
    f[pos +: 4]        = item.opcode[3:0];  pos += 4;
    f[pos +: 2]        = item.resp_err;     pos += 2;
    f[pos +: 3]        = item.resp;         pos += 3;
    f[pos +: TXNID_W]  = item.dbid;         pos += TXNID_W;
    f[pos +: 2]        = item.ccid;         pos += 2;
    f[pos +: 2]        = item.data_id;      pos += 2;
    f[pos +: RSVDC_W]  = '0;               pos += RSVDC_W;
    f[pos +: BE_W]     = item.be;           pos += BE_W;
    f[pos +: DATA_W]   = item.data;         pos += DATA_W;
    f[pos]             = item.trace_tag;
    item.flit        = f;
    item.flit_length = 36 + 3*NODEID_W + 2*TXNID_W + RSVDC_W + BE_W + DATA_W;
  endfunction

  function void pack_snp_flit(chi_seq_item item);
    logic [511:0] f = '0;
    int pos = 0;
    f[pos +: 4]            = item.qos;               pos += 4;
    f[pos +: NODEID_W]     = item.srcid;             pos += NODEID_W;
    f[pos +: TXNID_W]      = item.txnid;             pos += TXNID_W;
    f[pos +: NODEID_W]     = item.fwd_nid;           pos += NODEID_W;
    f[pos +: TXNID_W]      = item.fwd_txnid;         pos += TXNID_W;
    f[pos +: 6]            = item.opcode[5:0];       pos += 6;
    f[pos +: SNP_ADDR_W]   = item.addr[ADDR_W-1:3];  pos += SNP_ADDR_W;
    f[pos]                 = item.ns;                pos += 1;
    f[pos]                 = item.do_not_go_to_sd;   pos += 1;
    f[pos]                 = item.ret_to_src;        pos += 1;
    f[pos]                 = item.trace_tag;
    item.flit        = f;
    item.flit_length = 16 + 2*NODEID_W + 2*TXNID_W + SNP_ADDR_W;
  endfunction

  // ---------------------------------------------------------------------------
  // CLog record writer
  // ---------------------------------------------------------------------------
  function void log_flit(chi_seq_item item);
    if (log_enable && clog_handle != null)
      CLogB_WriteRecord(
        clog_handle,
        longint'(item.cycle),
        int'(item.nodeid),
        int'(item.clog_channel),
        item.flit,
        int'(item.flit_length)
      );
  endfunction

endclass : chi_monitor

`endif // CHI_MONITOR_SV
