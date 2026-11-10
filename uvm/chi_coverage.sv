// chi_coverage.sv
// Functional coverage collector for CHI RN transactions.
//
// Receives chi_seq_item from the monitor's analysis port and updates
// covergroups for opcodes, cache state responses, QoS values, and
// cross-coverage between transaction types and response types.

`ifndef CHI_COVERAGE_SV
`define CHI_COVERAGE_SV

`include "uvm_macros.svh"
import uvm_pkg::*;
import chi_pkg::*;

class chi_coverage extends uvm_subscriber #(chi_seq_item);
  `uvm_component_utils(chi_coverage)

  chi_seq_item current_item;

  // -------------------------------------------------------------------------
  // REQ opcode coverage
  // -------------------------------------------------------------------------
  covergroup cg_req_opcodes;
    cp_opcode: coverpoint current_item.opcode iff
                          (current_item.channel == chi_seq_item::CH_TXREQ) {
      bins read_shared            = {REQ_ReadShared};
      bins read_clean             = {REQ_ReadClean};
      bins read_once              = {REQ_ReadOnce};
      bins read_no_snp            = {REQ_ReadNoSnp};
      bins read_unique            = {REQ_ReadUnique};
      bins read_not_shared_dirty  = {REQ_ReadNotSharedDirty};
      bins read_once_clean_inv    = {REQ_ReadOnceCleanInvalid};
      bins read_once_make_inv     = {REQ_ReadOnceMakeInvalid};
      bins write_unique_full      = {REQ_WriteUniqueFull};
      bins write_unique_ptl       = {REQ_WriteUniquePtl};
      bins write_no_snp_full      = {REQ_WriteNoSnpFull};
      bins write_no_snp_ptl       = {REQ_WriteNoSnpPtl};
      bins write_back_full        = {REQ_WriteBackFull};
      bins write_back_ptl         = {REQ_WriteBackPtl};
      bins write_clean_full       = {REQ_WriteCleanFull};
      bins write_evict_full       = {REQ_WriteEvictFull};
      bins clean_unique           = {REQ_CleanUnique};
      bins make_unique            = {REQ_MakeUnique};
      bins evict                  = {REQ_Evict};
      bins clean_shared           = {REQ_CleanShared};
      bins clean_invalid          = {REQ_CleanInvalid};
      bins make_invalid           = {REQ_MakeInvalid};
      bins atomic_swap            = {REQ_AtomicSwap};
      bins atomic_compare         = {REQ_AtomicCompare};
      bins stash_unique           = {REQ_StashOnceUnique};
      bins stash_shared           = {REQ_StashOnceShared};
      bins other                  = default;
    }
    cp_size: coverpoint current_item.size iff
                        (current_item.channel == chi_seq_item::CH_TXREQ) {
      bins size_1B  = {3'd0};
      bins size_2B  = {3'd1};
      bins size_4B  = {3'd2};
      bins size_8B  = {3'd3};
      bins size_16B = {3'd4};
      bins size_32B = {3'd5};
      bins size_64B = {3'd6};
    }
    cp_ns: coverpoint current_item.ns iff
                      (current_item.channel == chi_seq_item::CH_TXREQ);
    cp_excl: coverpoint current_item.excl iff
                        (current_item.channel == chi_seq_item::CH_TXREQ);
    cross cp_opcode, cp_size;
    cross cp_opcode, cp_ns;
  endgroup : cg_req_opcodes

  // -------------------------------------------------------------------------
  // RSP opcode and response coverage
  // -------------------------------------------------------------------------
  covergroup cg_rsp;
    cp_opcode: coverpoint current_item.opcode iff
                          (current_item.channel inside {chi_seq_item::CH_RXRSP,
                                                        chi_seq_item::CH_TXRSP}) {
      bins comp           = {RSP_Comp};
      bins comp_dbid_resp = {RSP_CompDBIDResp};
      bins dbid_resp      = {RSP_DBIDResp};
      bins retry_ack      = {RSP_RetryAck};
      bins read_receipt   = {RSP_ReadReceipt};
      bins comp_ack       = {RSP_CompAck};
      bins snp_resp       = {RSP_SnpResp};
      bins snp_resp_fwded = {RSP_SnpRespFwded};
      bins pcrd_grant     = {RSP_PCrdGrant};
      bins other          = default;
    }
    cp_resp: coverpoint current_item.resp iff
                        (current_item.channel inside {chi_seq_item::CH_RXRSP,
                                                      chi_seq_item::CH_TXRSP}) {
      bins I   = {3'b000};
      bins SC  = {3'b001};
      bins UC  = {3'b010};
      bins UD  = {3'b011};
      bins SD  = {3'b101};
      bins UCE = {3'b110};
      bins other = default;
    }
    cp_resp_err: coverpoint current_item.resp_err iff
                            (current_item.channel inside {chi_seq_item::CH_RXRSP,
                                                          chi_seq_item::CH_TXRSP}) {
      bins ok      = {2'b00};
      bins ex_fail = {2'b01};
      bins data_err= {2'b10};
      bins nderr   = {2'b11};
    }
    cross cp_opcode, cp_resp;
    cross cp_opcode, cp_resp_err;
  endgroup : cg_rsp

  // -------------------------------------------------------------------------
  // DAT channel coverage
  // -------------------------------------------------------------------------
  covergroup cg_dat;
    cp_opcode: coverpoint current_item.opcode iff
                          (current_item.channel inside {chi_seq_item::CH_TXDAT,
                                                        chi_seq_item::CH_RXDAT}) {
      bins comp_data         = {DAT_CompData};
      bins copy_back_wr_data = {DAT_CopyBackWrData};
      bins non_copy_wr_data  = {DAT_NonCopyBackWrData};
      bins snp_resp_data     = {DAT_SnpRespData};
      bins snp_resp_data_ptl = {DAT_SnpRespDataPtl};
      bins snp_resp_data_fwd = {DAT_SnpRespDataFwded};
      bins data_sep_resp     = {DAT_DataSepResp};
      bins write_data_cancel = {DAT_WriteDataCancel};
      bins other             = default;
    }
    cp_resp: coverpoint current_item.resp iff
                        (current_item.channel inside {chi_seq_item::CH_TXDAT,
                                                      chi_seq_item::CH_RXDAT});
    cp_data_id: coverpoint current_item.data_id iff
                           (current_item.channel inside {chi_seq_item::CH_TXDAT,
                                                         chi_seq_item::CH_RXDAT});
  endgroup : cg_dat

  // -------------------------------------------------------------------------
  // SNP opcode coverage
  // -------------------------------------------------------------------------
  covergroup cg_snp;
    cp_opcode: coverpoint current_item.opcode iff
                          (current_item.channel == chi_seq_item::CH_RXSNP) {
      bins snp_once            = {SNP_SnpOnce};
      bins snp_clean           = {SNP_SnpClean};
      bins snp_shared          = {SNP_SnpShared};
      bins snp_not_shared_dirty= {SNP_SnpNotSharedDirty};
      bins snp_unique          = {SNP_SnpUnique};
      bins snp_clean_shared    = {SNP_SnpCleanShared};
      bins snp_clean_invalid   = {SNP_SnpCleanInvalid};
      bins snp_make_invalid    = {SNP_SnpMakeInvalid};
      bins snp_prefer_unique   = {SNP_SnpPreferUnique};
      bins snp_dvm_op          = {SNP_SnpDVMOp};
      bins fwd_snoop           = {SNP_SnpOnceFwd, SNP_SnpSharedFwd,
                                  SNP_SnpNotSharedDirtyFwd, SNP_SnpUniqueFwd,
                                  SNP_SnpPreferUniqueFwd};
      bins other               = default;
    }
    cp_ret_to_src: coverpoint current_item.ret_to_src iff
                              (current_item.channel == chi_seq_item::CH_RXSNP);
    cp_do_not_go_to_sd: coverpoint current_item.do_not_go_to_sd iff
                                   (current_item.channel == chi_seq_item::CH_RXSNP);
    cross cp_opcode, cp_ret_to_src;
  endgroup : cg_snp

  // -------------------------------------------------------------------------
  // QoS coverage (all channels)
  // -------------------------------------------------------------------------
  covergroup cg_qos;
    cp_qos: coverpoint current_item.qos {
      bins qos_0  = {4'd0};
      bins qos_1  = {4'd1};
      bins qos_2  = {4'd2};
      bins qos_3  = {4'd3};
      bins qos_4  = {4'd4};
      bins qos_5  = {4'd5};
      bins qos_6  = {4'd6};
      bins qos_7  = {4'd7};
      bins qos_8  = {4'd8};
      bins qos_9  = {4'd9};
      bins qos_10 = {4'd10};
      bins qos_11 = {4'd11};
      bins qos_12 = {4'd12};
      bins qos_13 = {4'd13};
      bins qos_14 = {4'd14};
      bins qos_15 = {4'd15};
    }
    cp_channel: coverpoint current_item.channel;
    cross cp_qos, cp_channel;
  endgroup : cg_qos

  function new(string name, uvm_component parent);
    super.new(name, parent);
    cg_req_opcodes = new();
    cg_rsp         = new();
    cg_dat         = new();
    cg_snp         = new();
    cg_qos         = new();
  endfunction

  function void write(chi_seq_item item);
    current_item = item;
    cg_req_opcodes.sample();
    cg_rsp.sample();
    cg_dat.sample();
    cg_snp.sample();
    cg_qos.sample();
  endfunction

endclass : chi_coverage

`endif // CHI_COVERAGE_SV
