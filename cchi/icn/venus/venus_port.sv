`default_nettype none

import venus_pkg::*;

// One Type-1 Decoupled adapter: CCHI valid/ready pins <-> internal FIFOs.

module venus_port #(
    parameter int Q_EVT = 2,
    parameter int Q_REQ = 2,
    parameter int Q_RSP = 8,
    parameter int Q_DAT = 16,
    parameter int Q_SNP = 4
) (
    input  wire         clock,
    input  wire         reset,

    input  wire         rxevt_valid,
    output logic        rxevt_ready,
    input  evt_flit_t   rxevt_bits,

    input  wire         rxreq_valid,
    output logic        rxreq_ready,
    input  req_flit_t   rxreq_bits,

    output logic        txsnp_valid,
    input  wire         txsnp_ready,
    output snp_flit_t   txsnp_bits,

    output logic        txrsp_valid,
    input  wire         txrsp_ready,
    output dnrsp_flit_t txrsp_bits,

    input  wire         rxrsp_valid,
    output logic        rxrsp_ready,
    input  uprsp_flit_t rxrsp_bits,

    output logic        txdat_valid,
    input  wire         txdat_ready,
    output dndat_flit_t txdat_bits,

    input  wire         rxdat_valid,
    output logic        rxdat_ready,
    input  updat_flit_t rxdat_bits,

    output logic        evt_valid,
    input  wire         evt_ready,
    output evt_flit_t   evt_bits,

    output logic        req_valid,
    input  wire         req_ready,
    output req_flit_t   req_bits,

    output logic        uprsp_valid,
    input  wire         uprsp_ready,
    output uprsp_flit_t uprsp_bits,

    output logic        updat_valid,
    input  wire         updat_ready,
    output updat_flit_t updat_bits,

    input  wire         snp_valid,
    output logic        snp_ready,
    input  snp_flit_t   snp_bits,

    input  wire         dnrsp_valid,
    output logic        dnrsp_ready,
    input  dnrsp_flit_t dnrsp_bits,

    input  wire         dndat_valid,
    output logic        dndat_ready,
    input  dndat_flit_t dndat_bits,

    output logic        txrsp_fire,
    output logic [TXNID_W-1:0] txrsp_txnid,
    output logic [2:0]  txrsp_opcode
);

    venus_fifo #(.DW(EVT_W),   .DEPTH(Q_EVT)) u_evt (
        .clock(clock), .reset(reset),
        .wr_valid(rxevt_valid), .wr_ready(rxevt_ready), .wr_data(rxevt_bits),
        .rd_valid(evt_valid),   .rd_ready(evt_ready),   .rd_data(evt_bits),
        .count()
    );

    venus_fifo #(.DW(REQ_W),   .DEPTH(Q_REQ)) u_req (
        .clock(clock), .reset(reset),
        .wr_valid(rxreq_valid), .wr_ready(rxreq_ready), .wr_data(rxreq_bits),
        .rd_valid(req_valid),   .rd_ready(req_ready),   .rd_data(req_bits),
        .count()
    );

    venus_fifo #(.DW(UPRSP_W), .DEPTH(Q_RSP)) u_uprsp (
        .clock(clock), .reset(reset),
        .wr_valid(rxrsp_valid), .wr_ready(rxrsp_ready), .wr_data(rxrsp_bits),
        .rd_valid(uprsp_valid), .rd_ready(uprsp_ready), .rd_data(uprsp_bits),
        .count()
    );

    venus_fifo #(.DW(UPDAT_W), .DEPTH(Q_DAT)) u_updat (
        .clock(clock), .reset(reset),
        .wr_valid(rxdat_valid), .wr_ready(rxdat_ready), .wr_data(rxdat_bits),
        .rd_valid(updat_valid), .rd_ready(updat_ready), .rd_data(updat_bits),
        .count()
    );

    venus_fifo #(.DW(SNP_W),   .DEPTH(Q_SNP)) u_snp (
        .clock(clock), .reset(reset),
        .wr_valid(snp_valid),   .wr_ready(snp_ready),   .wr_data(snp_bits),
        .rd_valid(txsnp_valid), .rd_ready(txsnp_ready), .rd_data(txsnp_bits),
        .count()
    );

    venus_fifo #(.DW(DNRSP_W), .DEPTH(Q_RSP)) u_dnrsp (
        .clock(clock), .reset(reset),
        .wr_valid(dnrsp_valid), .wr_ready(dnrsp_ready), .wr_data(dnrsp_bits),
        .rd_valid(txrsp_valid), .rd_ready(txrsp_ready), .rd_data(txrsp_bits),
        .count()
    );

    venus_fifo #(.DW(DNDAT_W), .DEPTH(Q_DAT)) u_dndat (
        .clock(clock), .reset(reset),
        .wr_valid(dndat_valid), .wr_ready(dndat_ready), .wr_data(dndat_bits),
        .rd_valid(txdat_valid), .rd_ready(txdat_ready), .rd_data(txdat_bits),
        .count()
    );

    assign txrsp_fire   = txrsp_valid && txrsp_ready;
    assign txrsp_txnid  = txrsp_bits.txnid;
    assign txrsp_opcode = txrsp_bits.opcode;

endmodule

`default_nettype wire
