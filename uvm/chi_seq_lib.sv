// chi_seq_lib.sv
// Sequence library for common CHI RN transaction patterns.
//
// Each sequence generates a self-contained stimulus scenario using the
// chi_driver to drive the DUT.  For full transaction support the sequences
// also wait for the expected HN responses (driven by an HN reactive model
// or the CHIron reference model running in co-simulation).
//
// Sequences:
//   chi_base_seq        — abstract base (common utilities)
//   chi_read_shared_seq — ReadShared allocating read
//   chi_read_unique_seq — ReadUnique for write-intent read
//   chi_write_unique_seq— WriteUniqueFull write
//   chi_write_back_seq  — WriteBackFull dirty eviction
//   chi_atomic_seq      — AtomicSwap operation
//   chi_snp_resp_seq    — Respond to an incoming SnpShared (passive response)
//   chi_rand_seq        — Randomised mix of all the above

`ifndef CHI_SEQ_LIB_SV
`define CHI_SEQ_LIB_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

// ===========================================================================
// chi_base_seq — base class with common helpers
// ===========================================================================
class chi_base_seq extends uvm_sequence #(chi_seq_item);
  `uvm_object_utils(chi_base_seq)

  // Node IDs for this sequence
  rand logic [NODEID_W-1:0] src_id  = 1;
  rand logic [NODEID_W-1:0] tgt_id  = 8;   // typical HN-F NodeID
  rand logic [TXNID_W-1:0]  txn_id  = 0;

  function new(string name = "chi_base_seq");
    super.new(name);
  endfunction

  // Build a minimal REQ item with common fields pre-filled
  function chi_seq_item make_req(
    input logic [6:0]        opcode,
    input logic [ADDR_W-1:0] addr,
    input logic [2:0]        size = 3'd6  // 64 B cache line
  );
    chi_seq_item req;
    req = chi_seq_item::type_id::create("req");
    req.channel     = chi_seq_item::CH_TXREQ;
    req.opcode      = opcode;
    req.txnid       = txn_id;
    req.srcid       = src_id;
    req.tgtid       = tgt_id;
    req.addr        = addr;
    req.size        = size;
    req.qos         = 4'd0;
    req.mem_attr    = 4'b0010;  // Cacheable, Shareable
    req.allow_retry = 1'b1;
    req.order       = 2'b00;
    return req;
  endfunction

  // Build a CompAck RSP item (sent by RN to HN after receiving Comp/CompData)
  function chi_seq_item make_comp_ack(
    input logic [NODEID_W-1:0] tgt,
    input logic [TXNID_W-1:0]  txnid
  );
    chi_seq_item rsp;
    rsp = chi_seq_item::type_id::create("comp_ack");
    rsp.channel   = chi_seq_item::CH_TXRSP;
    rsp.opcode    = RSP_CompAck;
    rsp.txnid     = txnid;
    rsp.srcid     = src_id;
    rsp.tgtid     = tgt;
    rsp.qos       = 4'd0;
    return rsp;
  endfunction

  // Build a WriteData (CopyBackWrData / NonCopyBackWrData) DAT item
  function chi_seq_item make_write_data(
    input logic [3:0]        opcode,
    input logic [NODEID_W-1:0] tgt,
    input logic [TXNID_W-1:0]  txnid,
    input logic [DATA_W-1:0]   data,
    input logic [BE_W-1:0]     be = '1
  );
    chi_seq_item dat;
    dat = chi_seq_item::type_id::create("write_data");
    dat.channel   = chi_seq_item::CH_TXDAT;
    dat.opcode    = {3'b0, opcode};
    dat.txnid     = txnid;
    dat.srcid     = src_id;
    dat.tgtid     = tgt;
    dat.data      = data;
    dat.be        = be;
    dat.data_id   = 2'd0;
    dat.ccid      = 2'd0;
    dat.qos       = 4'd0;
    return dat;
  endfunction

endclass : chi_base_seq

// ===========================================================================
// chi_read_shared_seq — ReadShared with CompAck
// ===========================================================================
class chi_read_shared_seq extends chi_base_seq;
  `uvm_object_utils(chi_read_shared_seq)

  rand logic [ADDR_W-1:0] addr;

  constraint addr_align_c { addr[5:0] == 6'h0; }  // cache-line aligned

  function new(string name = "chi_read_shared_seq");
    super.new(name);
  endfunction

  task body();
    chi_seq_item req = make_req(REQ_ReadShared, addr);

    `uvm_info("CHI_SEQ", $sformatf("ReadShared addr=0x%012h txnid=0x%03h",
              addr, txn_id), UVM_MEDIUM)

    start_item(req);
    finish_item(req);

    // In a real environment the HN model drives a CompData response on RXDAT
    // and the RN responds with CompAck.  Here we send the CompAck immediately
    // to close the transaction; adapt this when connected to an HN model.
    begin
      chi_seq_item ack = make_comp_ack(tgt_id, txn_id);
      start_item(ack);
      finish_item(ack);
    end
  endtask

endclass : chi_read_shared_seq

// ===========================================================================
// chi_read_unique_seq — ReadUnique (write-intent / ownership upgrade)
// ===========================================================================
class chi_read_unique_seq extends chi_base_seq;
  `uvm_object_utils(chi_read_unique_seq)

  rand logic [ADDR_W-1:0] addr;
  constraint addr_align_c { addr[5:0] == 6'h0; }

  function new(string name = "chi_read_unique_seq");
    super.new(name);
  endfunction

  task body();
    chi_seq_item req = make_req(REQ_ReadUnique, addr);

    `uvm_info("CHI_SEQ", $sformatf("ReadUnique addr=0x%012h txnid=0x%03h",
              addr, txn_id), UVM_MEDIUM)

    start_item(req);
    finish_item(req);

    // CompAck follows Comp or CompData from HN
    begin
      chi_seq_item ack = make_comp_ack(tgt_id, txn_id);
      start_item(ack);
      finish_item(ack);
    end
  endtask

endclass : chi_read_unique_seq

// ===========================================================================
// chi_write_unique_seq — WriteUniqueFull with NonCopyBackWrData
// ===========================================================================
class chi_write_unique_seq extends chi_base_seq;
  `uvm_object_utils(chi_write_unique_seq)

  rand logic [ADDR_W-1:0] addr;
  rand logic [DATA_W-1:0] write_data;
  constraint addr_align_c { addr[5:0] == 6'h0; }

  function new(string name = "chi_write_unique_seq");
    super.new(name);
  endfunction

  task body();
    chi_seq_item req = make_req(REQ_WriteUniqueFull, addr);

    `uvm_info("CHI_SEQ", $sformatf("WriteUniqueFull addr=0x%012h txnid=0x%03h",
              addr, txn_id), UVM_MEDIUM)

    // 1. Send WriteUniqueFull request
    start_item(req);
    finish_item(req);

    // 2. After receiving DBIDResp from HN, send write data
    //    (The actual DBIDResp is handled by the HN model / reactive agent)
    begin
      chi_seq_item wdat = make_write_data(
        DAT_NonCopyBackWrData[3:0], tgt_id, txn_id, write_data);
      start_item(wdat);
      finish_item(wdat);
    end
  endtask

endclass : chi_write_unique_seq

// ===========================================================================
// chi_write_back_seq — WriteBackFull dirty eviction
// ===========================================================================
class chi_write_back_seq extends chi_base_seq;
  `uvm_object_utils(chi_write_back_seq)

  rand logic [ADDR_W-1:0] addr;
  rand logic [DATA_W-1:0] dirty_data;
  constraint addr_align_c { addr[5:0] == 6'h0; }

  function new(string name = "chi_write_back_seq");
    super.new(name);
  endfunction

  task body();
    chi_seq_item req = make_req(REQ_WriteBackFull, addr);

    `uvm_info("CHI_SEQ", $sformatf("WriteBackFull addr=0x%012h txnid=0x%03h",
              addr, txn_id), UVM_MEDIUM)

    // 1. WriteBackFull request
    start_item(req);
    finish_item(req);

    // 2. Send CopyBackWrData in response to DBIDResp
    begin
      chi_seq_item wdat = make_write_data(
        DAT_CopyBackWrData[3:0], tgt_id, txn_id, dirty_data);
      wdat.resp = 3'b011;  // UD_PD — sending unique dirty data
      start_item(wdat);
      finish_item(wdat);
    end
  endtask

endclass : chi_write_back_seq

// ===========================================================================
// chi_atomic_seq — AtomicSwap (load-linked / store-conditional style)
// ===========================================================================
class chi_atomic_seq extends chi_base_seq;
  `uvm_object_utils(chi_atomic_seq)

  rand logic [ADDR_W-1:0] addr;
  rand logic [63:0]        swap_data;  // new value
  constraint addr_align_c { addr[2:0] == 3'h0; }

  function new(string name = "chi_atomic_seq");
    super.new(name);
  endfunction

  task body();
    chi_seq_item req = make_req(REQ_AtomicSwap, addr, 3'd3);  // 8 B
    req.mem_attr = 4'b0110;  // No-allocate, device

    `uvm_info("CHI_SEQ", $sformatf("AtomicSwap addr=0x%012h txnid=0x%03h",
              addr, txn_id), UVM_MEDIUM)

    start_item(req);
    finish_item(req);
    // Return data comes in DAT channel (CompData or DataSepResp)
    // CompAck sent after receiving data
    begin
      chi_seq_item ack = make_comp_ack(tgt_id, txn_id);
      start_item(ack);
      finish_item(ack);
    end
  endtask

endclass : chi_atomic_seq

// ===========================================================================
// chi_snp_resp_seq
// Sends a SnpResp (no data, state change to I) in reply to a snoop.
// Typically driven by the HN reactive model triggering this sequence via the
// sequencer.  Bind snoop opcode and address before starting.
// ===========================================================================
class chi_snp_resp_seq extends chi_base_seq;
  `uvm_object_utils(chi_snp_resp_seq)

  logic [TXNID_W-1:0]  snp_txnid = '0;
  logic [NODEID_W-1:0] snp_srcid = '0;
  logic [2:0]          snp_resp  = 3'b000;  // default: I (Invalidate)

  function new(string name = "chi_snp_resp_seq");
    super.new(name);
  endfunction

  task body();
    chi_seq_item rsp;
    rsp = chi_seq_item::type_id::create("snp_resp");
    rsp.channel   = chi_seq_item::CH_TXRSP;
    rsp.opcode    = RSP_SnpResp;
    rsp.txnid     = snp_txnid;
    rsp.srcid     = src_id;
    rsp.tgtid     = snp_srcid;
    rsp.resp      = snp_resp;
    rsp.qos       = 4'd0;

    `uvm_info("CHI_SEQ", $sformatf("SnpResp txnid=0x%03h resp=0x%01h",
              snp_txnid, snp_resp), UVM_MEDIUM)

    start_item(rsp);
    finish_item(rsp);
  endtask

endclass : chi_snp_resp_seq

// ===========================================================================
// chi_rand_seq — randomised mix of transactions
// ===========================================================================
class chi_rand_seq extends chi_base_seq;
  `uvm_object_utils(chi_rand_seq)

  int unsigned num_transactions = 20;
  rand logic [ADDR_W-1:0] base_addr;
  constraint base_addr_c { base_addr[11:6] inside {[0:63]}; base_addr[5:0] == 6'h0; }

  function new(string name = "chi_rand_seq");
    super.new(name);
  endfunction

  task body();
    for (int i = 0; i < num_transactions; i++) begin
      int pick;
      logic [ADDR_W-1:0] addr;

      pick = $urandom_range(0, 3);
      addr = (base_addr + (i * 64)) & ~64'h3F;

      case (pick)
        0: begin
          chi_read_shared_seq seq = chi_read_shared_seq::type_id::create("rs");
          seq.src_id = src_id;
          seq.tgt_id = tgt_id;
          seq.txn_id = txn_id + i;
          seq.addr   = addr;
          seq.start(m_sequencer);
        end
        1: begin
          chi_write_unique_seq seq = chi_write_unique_seq::type_id::create("wu");
          seq.src_id     = src_id;
          seq.tgt_id     = tgt_id;
          seq.txn_id     = txn_id + i;
          seq.addr       = addr;
          seq.write_data = $urandom();
          seq.start(m_sequencer);
        end
        2: begin
          chi_write_back_seq seq = chi_write_back_seq::type_id::create("wb");
          seq.src_id    = src_id;
          seq.tgt_id    = tgt_id;
          seq.txn_id    = txn_id + i;
          seq.addr      = addr;
          seq.dirty_data= $urandom();
          seq.start(m_sequencer);
        end
        3: begin
          chi_atomic_seq seq = chi_atomic_seq::type_id::create("at");
          seq.src_id    = src_id;
          seq.tgt_id    = tgt_id;
          seq.txn_id    = txn_id + i;
          seq.addr      = addr;
          seq.swap_data = $urandom();
          seq.start(m_sequencer);
        end
      endcase
    end
  endtask

endclass : chi_rand_seq

`endif // CHI_SEQ_LIB_SV
