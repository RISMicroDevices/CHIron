// chi_seq_item.sv
// UVM sequence item representing a single CHI flit on any channel.
//
// The item carries the channel kind (REQ/RSP/DAT/SNP) and the direction
// (TX from RN, or RX from HN), plus all field values.  The packed `flit`
// field holds the raw 512-bit representation used by CLog DPI functions.

`ifndef CHI_SEQ_ITEM_SV
`define CHI_SEQ_ITEM_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

class chi_seq_item extends uvm_sequence_item;
  `uvm_object_utils_begin(chi_seq_item)
    `uvm_field_enum(chi_channel_e, channel, UVM_ALL_ON)
    `uvm_field_int(srcid,          UVM_ALL_ON)
    `uvm_field_int(tgtid,          UVM_ALL_ON)
    `uvm_field_int(txnid,          UVM_ALL_ON)
    `uvm_field_int(opcode,         UVM_ALL_ON)
    `uvm_field_int(addr,           UVM_ALL_ON)
    `uvm_field_int(size,           UVM_ALL_ON)
    `uvm_field_int(qos,            UVM_ALL_ON)
    `uvm_field_int(mem_attr,       UVM_ALL_ON)
    `uvm_field_int(ns,             UVM_ALL_ON)
    `uvm_field_int(allow_retry,    UVM_ALL_ON)
    `uvm_field_int(order,          UVM_ALL_ON)
    `uvm_field_int(pcrd_type,      UVM_ALL_ON)
    `uvm_field_int(snp_attr,       UVM_ALL_ON)
    `uvm_field_int(excl,           UVM_ALL_ON)
    `uvm_field_int(resp_err,       UVM_ALL_ON)
    `uvm_field_int(resp,           UVM_ALL_ON)
    `uvm_field_int(dbid,           UVM_ALL_ON)
    `uvm_field_int(data,           UVM_ALL_ON)
    `uvm_field_int(be,             UVM_ALL_ON)
    `uvm_field_int(data_id,        UVM_ALL_ON)
    `uvm_field_int(ccid,           UVM_ALL_ON)
    `uvm_field_int(home_nid,       UVM_ALL_ON)
    `uvm_field_int(fwd_nid,        UVM_ALL_ON)
    `uvm_field_int(fwd_txnid,      UVM_ALL_ON)
    `uvm_field_int(fwd_state,      UVM_ALL_ON)
    `uvm_field_int(ret_to_src,     UVM_ALL_ON)
    `uvm_field_int(do_not_go_to_sd,UVM_ALL_ON)
    `uvm_field_int(trace_tag,      UVM_ALL_ON)
    `uvm_field_int(flit,           UVM_ALL_ON | UVM_NOPRINT)
    `uvm_field_int(flit_length,    UVM_ALL_ON)
    `uvm_field_int(cycle,          UVM_ALL_ON)
    `uvm_field_int(nodeid,         UVM_ALL_ON)
    `uvm_field_int(clog_channel,   UVM_ALL_ON)
  `uvm_object_utils_end

  // -------------------------------------------------------------------------
  // Channel / direction identifiers
  // -------------------------------------------------------------------------
  typedef enum {
    CH_TXREQ, CH_TXRSP, CH_TXDAT,
    CH_RXRSP, CH_RXDAT, CH_RXSNP
  } chi_channel_e;

  rand chi_channel_e channel;

  // -------------------------------------------------------------------------
  // Common fields (used by multiple channels)
  // -------------------------------------------------------------------------
  rand logic [NODEID_W-1:0]   srcid;
  rand logic [NODEID_W-1:0]   tgtid;
  rand logic [TXNID_W-1:0]    txnid;
  rand logic [6:0]             opcode;   // widest: REQ uses 7 bits
  rand logic [ADDR_W-1:0]      addr;
  rand logic [2:0]             size;
  rand logic [3:0]             qos;
  rand logic [3:0]             mem_attr;
  rand logic                   ns;
  rand logic                   allow_retry;
  rand logic [1:0]             order;
  rand logic [3:0]             pcrd_type;
  rand logic                   snp_attr;
  rand logic                   excl;

  // RSP / DAT
  rand logic [1:0]             resp_err;
  rand logic [2:0]             resp;
  rand logic [TXNID_W-1:0]     dbid;

  // DAT
  rand logic [DATA_W-1:0]      data;
  rand logic [BE_W-1:0]        be;
  rand logic [1:0]             data_id;
  rand logic [1:0]             ccid;
  rand logic [NODEID_W-1:0]    home_nid;

  // SNP
  rand logic [NODEID_W-1:0]    fwd_nid;
  rand logic [TXNID_W-1:0]     fwd_txnid;
  rand logic [2:0]             fwd_state;
  rand logic                   ret_to_src;
  rand logic                   do_not_go_to_sd;

  // Misc
  rand logic                   trace_tag;

  // -------------------------------------------------------------------------
  // Raw flit for CLog DPI logging (packed into 512 bits)
  // Populated by the monitor pack() method or by the driver before sending.
  // -------------------------------------------------------------------------
  logic [511:0]                flit;
  int unsigned                 flit_length; // actual bit width of this channel's flit
  longint unsigned             cycle;       // clock cycle at capture
  int unsigned                 nodeid;      // node this flit was captured at
  int unsigned                 clog_channel;// CLOG_CH_* constant

  // -------------------------------------------------------------------------
  // Constructor
  // -------------------------------------------------------------------------
  function new(string name = "chi_seq_item");
    super.new(name);
    flit        = '0;
    flit_length = 0;
    cycle       = 0;
    nodeid      = 0;
    clog_channel= 0;
  endfunction

  // -------------------------------------------------------------------------
  // Pack a REQ flit into the raw 512-bit field.
  // Only the lower flit_length bits are meaningful.
  // -------------------------------------------------------------------------
  function void pack_req();
    // Pack REQ flit fields in CHI Issue E order (LSB first).
    // Field order: QoS, TgtID, SrcID, TxnID, ReturnNID, StashNIDValid,
    //   ReturnTxnID, StashNID, Endian, Deep, Opcode, Size, Addr, NS,
    //   LikelyShared, AllowRetry, Order, PCrdType, MemAttr, SnpAttr,
    //   DoDWT, LPID, Excl, SnoopMe, CAH, RSVDC, TraceTag
    flit = '0;
    begin
      int pos = 0;
      flit[pos +: 4]        = qos;           pos += 4;
      flit[pos +: NODEID_W] = tgtid;         pos += NODEID_W;
      flit[pos +: NODEID_W] = srcid;         pos += NODEID_W;
      flit[pos +: TXNID_W]  = txnid;         pos += TXNID_W;
      pos += NODEID_W;   // ReturnNID (not carried in item)
      pos += 1;          // StashNIDValid
      pos += TXNID_W;    // ReturnTxnID
      pos += NODEID_W;   // StashNID
      pos += 1;          // Endian
      pos += 1;          // Deep
      flit[pos +: 7]     = opcode[6:0];     pos += 7;
      flit[pos +: 3]     = size;            pos += 3;
      flit[pos +: ADDR_W]= addr;            pos += ADDR_W;
      flit[pos]          = ns;              pos += 1;
      pos += 1;          // LikelyShared
      flit[pos]          = allow_retry;     pos += 1;
      flit[pos +: 2]     = order;           pos += 2;
      flit[pos +: 4]     = pcrd_type;       pos += 4;
      flit[pos +: 4]     = mem_attr;        pos += 4;
      flit[pos]          = snp_attr;        pos += 1;
      pos += 1;          // DoDWT
      pos += 5;          // LPID
      flit[pos]          = excl;            pos += 1;
      pos += 1;          // SnoopMe
      pos += 1;          // CAH
      pos += RSVDC_W;    // RSVDC
      flit[pos]          = trace_tag;
    end
    flit_length  = 88 + 4*NODEID_W + 2*TXNID_W + ADDR_W + RSVDC_W;
    clog_channel = CLOG_CH_TXREQ;
  endfunction

  // -------------------------------------------------------------------------
  // Convenience: return a one-line summary string
  // -------------------------------------------------------------------------
  function string convert2string();
    return $sformatf("CHI[%s] op=0x%02h txnid=0x%03h src=0x%02h tgt=0x%02h addr=0x%012h",
                     channel.name(), opcode, txnid, srcid, tgtid, addr);
  endfunction

endclass : chi_seq_item

`endif // CHI_SEQ_ITEM_SV
