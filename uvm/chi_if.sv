// chi_if.sv
// CHI Issue E bus interface for UVM testbench.
//
// This interface models the physical channels between an RN (Requester Node)
// and an HN (Home Node), matching the signal names used in the CHI
// specification (§B-2).  Signal directions are defined from the perspective of
// the RN:
//
//   TXREQ / TXRSP / TXDAT  — driven by RN (DUT outputs in an RN verification)
//   RXRSP / RXDAT / RXSNP  — driven by HN (testbench drives, DUT receives)
//
// Parameters correspond to chi_pkg defaults; override them to match your DUT.

`ifndef CHI_IF_SV
`define CHI_IF_SV

`include "chi_pkg.sv"

interface chi_if #(
  parameter int unsigned NODEID_W   = chi_pkg::NODEID_W,
  parameter int unsigned ADDR_W     = chi_pkg::ADDR_W,
  parameter int unsigned DATA_W     = chi_pkg::DATA_W,
  parameter int unsigned RSVDC_W    = chi_pkg::RSVDC_W,
  parameter int unsigned TXNID_W    = chi_pkg::TXNID_W
) (
  input logic clk,
  input logic rst_n
);

  // -------------------------------------------------------------------------
  // TXREQ channel  (RN → HN)
  // -------------------------------------------------------------------------
  logic                       txreq_flitpend;
  logic                       txreq_flitv;
  logic [6:0]                 txreq_opcode;
  logic [TXNID_W-1:0]         txreq_txnid;
  logic [NODEID_W-1:0]        txreq_srcid;
  logic [NODEID_W-1:0]        txreq_tgtid;
  logic [ADDR_W-1:0]          txreq_addr;
  logic [2:0]                 txreq_size;
  logic [3:0]                 txreq_qos;
  logic [3:0]                 txreq_mem_attr;
  logic                       txreq_ns;
  logic                       txreq_allow_retry;
  logic [1:0]                 txreq_order;
  logic [3:0]                 txreq_pcrd_type;
  logic                       txreq_likely_shared;
  logic                       txreq_snp_attr;
  logic [4:0]                 txreq_lpid;
  logic                       txreq_excl;
  logic                       txreq_snoop_me;
  logic                       txreq_cah;
  logic [NODEID_W-1:0]        txreq_return_nid;
  logic                       txreq_stash_nid_valid;
  logic [NODEID_W-1:0]        txreq_stash_nid;
  logic [TXNID_W-1:0]         txreq_return_txnid;
  logic                       txreq_do_dwt;
  logic [RSVDC_W-1:0]         txreq_rsvdc;
  logic                       txreq_trace_tag;
  logic                       txreq_lcrdv;   // credit return (HN → RN)

  // -------------------------------------------------------------------------
  // TXRSP channel  (RN → HN)
  // -------------------------------------------------------------------------
  logic                       txrsp_flitpend;
  logic                       txrsp_flitv;
  logic [4:0]                 txrsp_opcode;
  logic [TXNID_W-1:0]         txrsp_txnid;
  logic [NODEID_W-1:0]        txrsp_srcid;
  logic [NODEID_W-1:0]        txrsp_tgtid;
  logic [1:0]                 txrsp_resp_err;
  logic [2:0]                 txrsp_resp;
  logic [TXNID_W-1:0]         txrsp_dbid;
  logic [3:0]                 txrsp_pcrd_type;
  logic [3:0]                 txrsp_qos;
  logic                       txrsp_trace_tag;
  logic                       txrsp_lcrdv;   // credit return (HN → RN)

  // -------------------------------------------------------------------------
  // TXDAT channel  (RN → HN)
  // -------------------------------------------------------------------------
  logic                       txdat_flitpend;
  logic                       txdat_flitv;
  logic [3:0]                 txdat_opcode;
  logic [TXNID_W-1:0]         txdat_txnid;
  logic [NODEID_W-1:0]        txdat_srcid;
  logic [NODEID_W-1:0]        txdat_tgtid;
  logic [NODEID_W-1:0]        txdat_home_nid;
  logic [1:0]                 txdat_resp_err;
  logic [2:0]                 txdat_resp;
  logic [TXNID_W-1:0]         txdat_dbid;
  logic [1:0]                 txdat_ccid;
  logic [1:0]                 txdat_data_id;
  logic [3:0]                 txdat_qos;
  logic [RSVDC_W-1:0]         txdat_rsvdc;
  logic [(DATA_W/8)-1:0]      txdat_be;
  logic [DATA_W-1:0]          txdat_data;
  logic                       txdat_trace_tag;
  logic                       txdat_lcrdv;   // credit return (HN → RN)

  // -------------------------------------------------------------------------
  // RXRSP channel  (HN → RN)
  // -------------------------------------------------------------------------
  logic                       rxrsp_flitpend;
  logic                       rxrsp_flitv;
  logic [4:0]                 rxrsp_opcode;
  logic [TXNID_W-1:0]         rxrsp_txnid;
  logic [NODEID_W-1:0]        rxrsp_srcid;
  logic [NODEID_W-1:0]        rxrsp_tgtid;
  logic [1:0]                 rxrsp_resp_err;
  logic [2:0]                 rxrsp_resp;
  logic [TXNID_W-1:0]         rxrsp_dbid;
  logic [3:0]                 rxrsp_pcrd_type;
  logic [3:0]                 rxrsp_qos;
  logic [2:0]                 rxrsp_fwd_state;
  logic                       rxrsp_data_pull;
  logic [2:0]                 rxrsp_cbusy;
  logic                       rxrsp_trace_tag;
  logic                       rxrsp_lcrdv;   // credit return (RN → HN)

  // -------------------------------------------------------------------------
  // RXDAT channel  (HN → RN)
  // -------------------------------------------------------------------------
  logic                       rxdat_flitpend;
  logic                       rxdat_flitv;
  logic [3:0]                 rxdat_opcode;
  logic [TXNID_W-1:0]         rxdat_txnid;
  logic [NODEID_W-1:0]        rxdat_srcid;
  logic [NODEID_W-1:0]        rxdat_tgtid;
  logic [NODEID_W-1:0]        rxdat_home_nid;
  logic [1:0]                 rxdat_resp_err;
  logic [2:0]                 rxdat_resp;
  logic [TXNID_W-1:0]         rxdat_dbid;
  logic [1:0]                 rxdat_ccid;
  logic [1:0]                 rxdat_data_id;
  logic [3:0]                 rxdat_qos;
  logic [RSVDC_W-1:0]         rxdat_rsvdc;
  logic [(DATA_W/8)-1:0]      rxdat_be;
  logic [DATA_W-1:0]          rxdat_data;
  logic [2:0]                 rxdat_fwd_state;
  logic                       rxdat_data_pull;
  logic [2:0]                 rxdat_cbusy;
  logic                       rxdat_trace_tag;
  logic                       rxdat_lcrdv;   // credit return (RN → HN)

  // -------------------------------------------------------------------------
  // RXSNP channel  (HN → RN)
  // -------------------------------------------------------------------------
  logic                       rxsnp_flitpend;
  logic                       rxsnp_flitv;
  logic [5:0]                 rxsnp_opcode;
  logic [TXNID_W-1:0]         rxsnp_txnid;
  logic [NODEID_W-1:0]        rxsnp_srcid;
  logic [NODEID_W-1:0]        rxsnp_fwd_nid;
  logic [TXNID_W-1:0]         rxsnp_fwd_txnid;
  logic [(ADDR_W-3)-1:0]      rxsnp_addr;
  logic                       rxsnp_ns;
  logic                       rxsnp_do_not_go_to_sd;
  logic                       rxsnp_ret_to_src;
  logic [3:0]                 rxsnp_qos;
  logic                       rxsnp_trace_tag;
  logic                       rxsnp_lcrdv;   // credit return (RN → HN)

  // -------------------------------------------------------------------------
  // Clocking blocks for the UVM driver and monitor
  // -------------------------------------------------------------------------

  // RN-side driver clocking block (drives TX channels, samples RX channels)
  clocking rn_driver_cb @(posedge clk);
    default input  #1step
            output #1;

    // Drive TX
    output txreq_flitpend, txreq_flitv,
           txreq_opcode, txreq_txnid, txreq_srcid, txreq_tgtid,
           txreq_addr, txreq_size, txreq_qos, txreq_mem_attr,
           txreq_ns, txreq_allow_retry, txreq_order, txreq_pcrd_type,
           txreq_likely_shared, txreq_snp_attr, txreq_lpid,
           txreq_excl, txreq_snoop_me, txreq_cah,
           txreq_return_nid, txreq_stash_nid_valid, txreq_stash_nid,
           txreq_return_txnid, txreq_do_dwt, txreq_rsvdc, txreq_trace_tag;

    output txrsp_flitpend, txrsp_flitv,
           txrsp_opcode, txrsp_txnid, txrsp_srcid, txrsp_tgtid,
           txrsp_resp_err, txrsp_resp, txrsp_dbid, txrsp_pcrd_type,
           txrsp_qos, txrsp_trace_tag;

    output txdat_flitpend, txdat_flitv,
           txdat_opcode, txdat_txnid, txdat_srcid, txdat_tgtid,
           txdat_home_nid, txdat_resp_err, txdat_resp, txdat_dbid,
           txdat_ccid, txdat_data_id, txdat_qos, txdat_rsvdc,
           txdat_be, txdat_data, txdat_trace_tag;

    // Sample RX credit returns driven by RN (return credits to HN)
    output rxrsp_lcrdv, rxdat_lcrdv, rxsnp_lcrdv;

    // Sample RX channels
    input  rxrsp_flitpend, rxrsp_flitv,
           rxrsp_opcode, rxrsp_txnid, rxrsp_srcid, rxrsp_tgtid,
           rxrsp_resp_err, rxrsp_resp, rxrsp_dbid, rxrsp_pcrd_type,
           rxrsp_qos, rxrsp_fwd_state, rxrsp_data_pull, rxrsp_cbusy,
           rxrsp_trace_tag;

    input  rxdat_flitpend, rxdat_flitv,
           rxdat_opcode, rxdat_txnid, rxdat_srcid, rxdat_tgtid,
           rxdat_home_nid, rxdat_resp_err, rxdat_resp, rxdat_dbid,
           rxdat_ccid, rxdat_data_id, rxdat_qos, rxdat_rsvdc,
           rxdat_be, rxdat_data, rxdat_fwd_state, rxdat_data_pull,
           rxdat_cbusy, rxdat_trace_tag;

    input  rxsnp_flitpend, rxsnp_flitv,
           rxsnp_opcode, rxsnp_txnid, rxsnp_srcid, rxsnp_fwd_nid,
           rxsnp_fwd_txnid, rxsnp_addr, rxsnp_ns, rxsnp_do_not_go_to_sd,
           rxsnp_ret_to_src, rxsnp_qos, rxsnp_trace_tag;

    // Sample TX credit returns driven by HN
    input  txreq_lcrdv, txrsp_lcrdv, txdat_lcrdv;
  endclocking : rn_driver_cb

  // Monitor clocking block (purely passive — samples all channels)
  clocking monitor_cb @(posedge clk);
    default input #1step;

    input txreq_flitpend, txreq_flitv,
          txreq_opcode, txreq_txnid, txreq_srcid, txreq_tgtid,
          txreq_addr, txreq_size, txreq_qos, txreq_mem_attr,
          txreq_ns, txreq_allow_retry, txreq_order, txreq_pcrd_type,
          txreq_likely_shared, txreq_snp_attr, txreq_lpid,
          txreq_excl, txreq_snoop_me, txreq_cah,
          txreq_return_nid, txreq_stash_nid_valid, txreq_stash_nid,
          txreq_return_txnid, txreq_do_dwt, txreq_rsvdc, txreq_trace_tag,
          txreq_lcrdv;

    input txrsp_flitpend, txrsp_flitv,
          txrsp_opcode, txrsp_txnid, txrsp_srcid, txrsp_tgtid,
          txrsp_resp_err, txrsp_resp, txrsp_dbid, txrsp_pcrd_type,
          txrsp_qos, txrsp_trace_tag, txrsp_lcrdv;

    input txdat_flitpend, txdat_flitv,
          txdat_opcode, txdat_txnid, txdat_srcid, txdat_tgtid,
          txdat_home_nid, txdat_resp_err, txdat_resp, txdat_dbid,
          txdat_ccid, txdat_data_id, txdat_qos, txdat_rsvdc,
          txdat_be, txdat_data, txdat_trace_tag, txdat_lcrdv;

    input rxrsp_flitpend, rxrsp_flitv,
          rxrsp_opcode, rxrsp_txnid, rxrsp_srcid, rxrsp_tgtid,
          rxrsp_resp_err, rxrsp_resp, rxrsp_dbid, rxrsp_pcrd_type,
          rxrsp_qos, rxrsp_fwd_state, rxrsp_data_pull, rxrsp_cbusy,
          rxrsp_trace_tag, rxrsp_lcrdv;

    input rxdat_flitpend, rxdat_flitv,
          rxdat_opcode, rxdat_txnid, rxdat_srcid, rxdat_tgtid,
          rxdat_home_nid, rxdat_resp_err, rxdat_resp, rxdat_dbid,
          rxdat_ccid, rxdat_data_id, rxdat_qos, rxdat_rsvdc,
          rxdat_be, rxdat_data, rxdat_fwd_state, rxdat_data_pull,
          rxdat_cbusy, rxdat_trace_tag, rxdat_lcrdv;

    input rxsnp_flitpend, rxsnp_flitv,
          rxsnp_opcode, rxsnp_txnid, rxsnp_srcid, rxsnp_fwd_nid,
          rxsnp_fwd_txnid, rxsnp_addr, rxsnp_ns, rxsnp_do_not_go_to_sd,
          rxsnp_ret_to_src, rxsnp_qos, rxsnp_trace_tag, rxsnp_lcrdv;
  endclocking : monitor_cb

endinterface : chi_if

`endif // CHI_IF_SV
