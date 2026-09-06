`default_nettype none

import venus_pkg::*;

// ============================================================================
// venus_tb — directed + light-random standalone testbench for Venus.
// Venus itself is synthesizable; this TB and venus_axi_mem are not.
//
// Scenario map (see docs/TRANSACTIONS.md for the expected flows):
//   S1  ReadShared cold miss, sole tenant -> CompData UC (promotion, §7.4)
//   S2  ReadUnique, snoop unique owner (SnpToInvalid)
//   S3  MakeUnique, snoop unique owner (SnpMakeInvalid)
//   S4  Evict
//   S5  WriteBackFull + CopyBackWrData
//   S6  re-ReadShared of the written-back line -> UC + data check
//   S7  ReadNoSnp whole line
//   S8  ReadNoSnp sub-line (Size<B64, single beat)
//   S9  ReadOnce miss
//   S10 ReadOnce dirty-U hit (SnpToClean + SnpRespData_UC_PD + writeback)
//   S11 WriteUniqueFull + data verify
//   S12 WriteUniquePtl with PD merge under BE + data verify
//   S13 WriteNoSnpFull + WriteNoSnpPtl sub-line
//   S14 CleanShared on dirty unique owner
//   S15 CleanInvalid on dirty unique owner (other requester)
//   S16 CleanInvalid by the owner itself (requester self-snoop, §7.2)
//   S17 MakeInvalid by a sharer itself (self-snoop, dirty dropped)
//   S18 StashShared (no response) / StashUnique (CompStash)
//   S19 directory-full victim back-invalidation (§T15)
//   S20 M5: ReadNoSnp behind an in-flight WriteBackFull (§5.8, §7.3)
//   S21 shared ReadShared (SC) + ReadUnique upgrade (ExpCompData=0, Comp)
//   S22 EVT Evict sharing a line with an in-flight ReadUnique (§7.3)
//   S23 WriteBackFull sharing a line with an in-flight ReadUnique (M5 hold)
//   S24 ReadShared alias mismatch on an S-held line (self SnpToInvalid; TAGALIAS_W>0)
//   S25 ReadUnique alias mismatch on a self-owned dirty line (PD -> CompData; ditto)
//   S26 same-alias S->U upgrade: guaranteed no self-snoop (ditto)
//   S27 alias-changed S->U: one targeted SnpToInvalid + CompData (a snooped
//       requester is owed data even at ExpCompData=0) (ditto)
// ============================================================================

module venus_tb #(
    parameter int NUM_T1      = 4,   // live upstream ports (pin ceiling 8)
    parameter int NUM_AXI     = 1,   // live AXI channels (pin ceiling 4)
    parameter int PARALLELISM = 8,   // tracker slots
    parameter int TAGALIAS_W  = 0,   // TagAlias width (0: S24-S27 skipped)
    parameter int AGE_MATRIX  = 0    // 1 = age-oldest slot arbitration
);

    localparam int AXI_ADDR_W  = 48;
    localparam int AXI_DATA_W  = 256;
    localparam int AXI_ID_W    = 4;
    localparam int NODE_ID     = 16;

    logic clock;
    logic reset;

    initial begin
        clock = 1'b0;
        forever #5 clock = ~clock;
    end

    `define TB_T1_DECL(P) \
        logic        t1p``P``_rxevt_valid; \
        logic [6:0]  t1p``P``_rxevt_bits_TxnID; \
        logic [4:0]  t1p``P``_rxevt_bits_SrcID; \
        logic [4:0]  t1p``P``_rxevt_bits_TgtID; \
        logic        t1p``P``_rxevt_bits_Opcode; \
        logic [47:0] t1p``P``_rxevt_bits_Addr; \
        logic        t1p``P``_rxevt_bits_NS; \
        logic        t1p``P``_rxevt_bits_MemAttr; \
        logic        t1p``P``_rxevt_bits_WayValid; \
        logic [3:0]  t1p``P``_rxevt_bits_Way; \
        logic        t1p``P``_rxevt_bits_TraceTag; \
        logic        t1p``P``_rxevt_ready; \
        logic        t1p``P``_rxreq_valid; \
        logic [6:0]  t1p``P``_rxreq_bits_TxnID; \
        logic [4:0]  t1p``P``_rxreq_bits_SrcID; \
        logic [4:0]  t1p``P``_rxreq_bits_TgtID; \
        logic [5:0]  t1p``P``_rxreq_bits_Opcode; \
        logic [2:0]  t1p``P``_rxreq_bits_Size; \
        logic [47:0] t1p``P``_rxreq_bits_Addr; \
        logic [7:0]  t1p``P``_rxreq_bits_TagAlias; \
        logic        t1p``P``_rxreq_bits_NS; \
        logic [1:0]  t1p``P``_rxreq_bits_Order; \
        logic [3:0]  t1p``P``_rxreq_bits_MemAttr; \
        logic        t1p``P``_rxreq_bits_Excl; \
        logic        t1p``P``_rxreq_bits_ExpCompData; \
        logic        t1p``P``_rxreq_bits_TraceTag; \
        logic        t1p``P``_rxreq_bits_WayValid; \
        logic [3:0]  t1p``P``_rxreq_bits_Way; \
        logic        t1p``P``_rxreq_ready; \
        logic        t1p``P``_txsnp_ready; \
        logic        t1p``P``_txsnp_valid; \
        logic [6:0]  t1p``P``_txsnp_bits_TxnID; \
        logic [4:0]  t1p``P``_txsnp_bits_SrcID; \
        logic [4:0]  t1p``P``_txsnp_bits_TgtID; \
        logic [1:0]  t1p``P``_txsnp_bits_Opcode; \
        logic [44:0] t1p``P``_txsnp_bits_Addr; \
        logic        t1p``P``_txsnp_bits_NS; \
        logic        t1p``P``_txsnp_bits_TraceTag; \
        logic        t1p``P``_txrsp_ready; \
        logic        t1p``P``_txrsp_valid; \
        logic [6:0]  t1p``P``_txrsp_bits_TxnID; \
        logic [4:0]  t1p``P``_txrsp_bits_SrcID; \
        logic [4:0]  t1p``P``_txrsp_bits_TgtID; \
        logic [6:0]  t1p``P``_txrsp_bits_DBID; \
        logic [2:0]  t1p``P``_txrsp_bits_Opcode; \
        logic [1:0]  t1p``P``_txrsp_bits_RespErr; \
        logic [2:0]  t1p``P``_txrsp_bits_Resp; \
        logic [2:0]  t1p``P``_txrsp_bits_CBusy; \
        logic        t1p``P``_txrsp_bits_WayValid; \
        logic [3:0]  t1p``P``_txrsp_bits_Way; \
        logic        t1p``P``_txrsp_bits_TraceTag; \
        logic        t1p``P``_rxrsp_valid; \
        logic [6:0]  t1p``P``_rxrsp_bits_TxnID; \
        logic [4:0]  t1p``P``_rxrsp_bits_SrcID; \
        logic [4:0]  t1p``P``_rxrsp_bits_TgtID; \
        logic        t1p``P``_rxrsp_bits_Opcode; \
        logic [1:0]  t1p``P``_rxrsp_bits_RespErr; \
        logic [2:0]  t1p``P``_rxrsp_bits_Resp; \
        logic        t1p``P``_rxrsp_bits_TraceTag; \
        logic        t1p``P``_rxrsp_ready; \
        logic        t1p``P``_txdat_ready; \
        logic        t1p``P``_txdat_valid; \
        logic [6:0]  t1p``P``_txdat_bits_TxnID; \
        logic [4:0]  t1p``P``_txdat_bits_SrcID; \
        logic [4:0]  t1p``P``_txdat_bits_TgtID; \
        logic [6:0]  t1p``P``_txdat_bits_DBID; \
        logic        t1p``P``_txdat_bits_Opcode; \
        logic [1:0]  t1p``P``_txdat_bits_RespErr; \
        logic [2:0]  t1p``P``_txdat_bits_Resp; \
        logic [4:0]  t1p``P``_txdat_bits_DataSource; \
        logic [2:0]  t1p``P``_txdat_bits_CBusy; \
        logic        t1p``P``_txdat_bits_DataID; \
        logic [31:0] t1p``P``_txdat_bits_Data [0:7]; \
        logic        t1p``P``_txdat_bits_WayValid; \
        logic [3:0]  t1p``P``_txdat_bits_Way; \
        logic        t1p``P``_txdat_bits_TraceTag; \
        logic        t1p``P``_rxdat_valid; \
        logic [6:0]  t1p``P``_rxdat_bits_TxnID; \
        logic [4:0]  t1p``P``_rxdat_bits_SrcID; \
        logic [4:0]  t1p``P``_rxdat_bits_TgtID; \
        logic [1:0]  t1p``P``_rxdat_bits_Opcode; \
        logic [1:0]  t1p``P``_rxdat_bits_RespErr; \
        logic [2:0]  t1p``P``_rxdat_bits_Resp; \
        logic        t1p``P``_rxdat_bits_DataID; \
        logic [31:0] t1p``P``_rxdat_bits_Data [0:7]; \
        logic [31:0] t1p``P``_rxdat_bits_BE; \
        logic        t1p``P``_rxdat_bits_TraceTag; \
        logic        t1p``P``_rxdat_ready

    `TB_T1_DECL(0);
    `TB_T1_DECL(1);
    `TB_T1_DECL(2);
    `TB_T1_DECL(3);
    `TB_T1_DECL(4);
    `TB_T1_DECL(5);
    `TB_T1_DECL(6);
    `TB_T1_DECL(7);

    logic                    axi_m0_awvalid, axi_m0_awready;
    logic [AXI_ADDR_W-1:0]   axi_m0_awaddr;
    logic [AXI_ID_W-1:0]     axi_m0_awid;
    logic [7:0]              axi_m0_awlen;
    logic [2:0]              axi_m0_awsize;
    logic [1:0]              axi_m0_awburst;
    logic                    axi_m0_wvalid, axi_m0_wready;
    logic [AXI_DATA_W-1:0]   axi_m0_wdata;
    logic [AXI_DATA_W/8-1:0] axi_m0_wstrb;
    logic                    axi_m0_wlast;
    logic                    axi_m0_bvalid, axi_m0_bready;
    logic [AXI_ID_W-1:0]     axi_m0_bid;
    logic [1:0]              axi_m0_bresp;
    logic                    axi_m0_arvalid, axi_m0_arready;
    logic [AXI_ADDR_W-1:0]   axi_m0_araddr;
    logic [AXI_ID_W-1:0]     axi_m0_arid;
    logic [7:0]              axi_m0_arlen;
    logic [2:0]              axi_m0_arsize;
    logic [1:0]              axi_m0_arburst;
    logic                    axi_m0_rvalid, axi_m0_rready;
    logic [AXI_DATA_W-1:0]   axi_m0_rdata;
    logic [AXI_ID_W-1:0]     axi_m0_rid;
    logic [1:0]              axi_m0_rresp;
    logic                    axi_m0_rlast;

    logic                    axi_m1_awvalid, axi_m1_awready;
    logic [AXI_ADDR_W-1:0]   axi_m1_awaddr;
    logic [AXI_ID_W-1:0]     axi_m1_awid;
    logic [7:0]              axi_m1_awlen;
    logic [2:0]              axi_m1_awsize;
    logic [1:0]              axi_m1_awburst;
    logic                    axi_m1_wvalid, axi_m1_wready;
    logic [AXI_DATA_W-1:0]   axi_m1_wdata;
    logic [AXI_DATA_W/8-1:0] axi_m1_wstrb;
    logic                    axi_m1_wlast;
    logic                    axi_m1_bvalid, axi_m1_bready;
    logic [AXI_ID_W-1:0]     axi_m1_bid;
    logic [1:0]              axi_m1_bresp;
    logic                    axi_m1_arvalid, axi_m1_arready;
    logic [AXI_ADDR_W-1:0]   axi_m1_araddr;
    logic [AXI_ID_W-1:0]     axi_m1_arid;
    logic [7:0]              axi_m1_arlen;
    logic [2:0]              axi_m1_arsize;
    logic [1:0]              axi_m1_arburst;
    logic                    axi_m1_rvalid, axi_m1_rready;
    logic [AXI_DATA_W-1:0]   axi_m1_rdata;
    logic [AXI_ID_W-1:0]     axi_m1_rid;
    logic [1:0]              axi_m1_rresp;
    logic                    axi_m1_rlast;

    // AXI channels 2/3: pin ceiling only, always tied off in this TB (the
    // directed scenarios use the single memory on channel 0).
    logic                    axi_m2_awvalid, axi_m2_awready;
    logic [AXI_ADDR_W-1:0]   axi_m2_awaddr;
    logic [AXI_ID_W-1:0]     axi_m2_awid;
    logic [7:0]              axi_m2_awlen;
    logic [2:0]              axi_m2_awsize;
    logic [1:0]              axi_m2_awburst;
    logic                    axi_m2_wvalid, axi_m2_wready;
    logic [AXI_DATA_W-1:0]   axi_m2_wdata;
    logic [AXI_DATA_W/8-1:0] axi_m2_wstrb;
    logic                    axi_m2_wlast;
    logic                    axi_m2_bvalid, axi_m2_bready;
    logic [AXI_ID_W-1:0]     axi_m2_bid;
    logic [1:0]              axi_m2_bresp;
    logic                    axi_m2_arvalid, axi_m2_arready;
    logic [AXI_ADDR_W-1:0]   axi_m2_araddr;
    logic [AXI_ID_W-1:0]     axi_m2_arid;
    logic [7:0]              axi_m2_arlen;
    logic [2:0]              axi_m2_arsize;
    logic [1:0]              axi_m2_arburst;
    logic                    axi_m2_rvalid, axi_m2_rready;
    logic [AXI_DATA_W-1:0]   axi_m2_rdata;
    logic [AXI_ID_W-1:0]     axi_m2_rid;
    logic [1:0]              axi_m2_rresp;
    logic                    axi_m2_rlast;

    logic                    axi_m3_awvalid, axi_m3_awready;
    logic [AXI_ADDR_W-1:0]   axi_m3_awaddr;
    logic [AXI_ID_W-1:0]     axi_m3_awid;
    logic [7:0]              axi_m3_awlen;
    logic [2:0]              axi_m3_awsize;
    logic [1:0]              axi_m3_awburst;
    logic                    axi_m3_wvalid, axi_m3_wready;
    logic [AXI_DATA_W-1:0]   axi_m3_wdata;
    logic [AXI_DATA_W/8-1:0] axi_m3_wstrb;
    logic                    axi_m3_wlast;
    logic                    axi_m3_bvalid, axi_m3_bready;
    logic [AXI_ID_W-1:0]     axi_m3_bid;
    logic [1:0]              axi_m3_bresp;
    logic                    axi_m3_arvalid, axi_m3_arready;
    logic [AXI_ADDR_W-1:0]   axi_m3_araddr;
    logic [AXI_ID_W-1:0]     axi_m3_arid;
    logic [7:0]              axi_m3_arlen;
    logic [2:0]              axi_m3_arsize;
    logic [1:0]              axi_m3_arburst;
    logic                    axi_m3_rvalid, axi_m3_rready;
    logic [AXI_DATA_W-1:0]   axi_m3_rdata;
    logic [AXI_ID_W-1:0]     axi_m3_rid;
    logic [1:0]              axi_m3_rresp;
    logic                    axi_m3_rlast;

    venus #(
        .NUM_T1(NUM_T1),
        .NUM_AXI(NUM_AXI),
        .PARALLELISM(PARALLELISM),
        .NODE_ID(NODE_ID),
        .SF_ENABLE(1),
        .SF_ALLOW_OVER(1),
        .SF_BROADCAST(0),
        .TAGALIAS_W(TAGALIAS_W),
        .AGE_MATRIX(AGE_MATRIX),
        .STORAGE_DIR(0),
        .STORAGE_SF(0)
    ) dut (
        .clock(clock), .reset(reset),
        `define TB_T1_CONN(P) \
            .cchi_t1p``P``_rxevt_valid(t1p``P``_rxevt_valid), \
            .cchi_t1p``P``_rxevt_bits_TxnID(t1p``P``_rxevt_bits_TxnID), \
            .cchi_t1p``P``_rxevt_bits_SrcID(t1p``P``_rxevt_bits_SrcID), \
            .cchi_t1p``P``_rxevt_bits_TgtID(t1p``P``_rxevt_bits_TgtID), \
            .cchi_t1p``P``_rxevt_bits_Opcode(t1p``P``_rxevt_bits_Opcode), \
            .cchi_t1p``P``_rxevt_bits_Addr(t1p``P``_rxevt_bits_Addr), \
            .cchi_t1p``P``_rxevt_bits_NS(t1p``P``_rxevt_bits_NS), \
            .cchi_t1p``P``_rxevt_bits_MemAttr(t1p``P``_rxevt_bits_MemAttr), \
            .cchi_t1p``P``_rxevt_bits_WayValid(t1p``P``_rxevt_bits_WayValid), \
            .cchi_t1p``P``_rxevt_bits_Way(t1p``P``_rxevt_bits_Way), \
            .cchi_t1p``P``_rxevt_bits_TraceTag(t1p``P``_rxevt_bits_TraceTag), \
            .cchi_t1p``P``_rxevt_ready(t1p``P``_rxevt_ready), \
            .cchi_t1p``P``_rxreq_valid(t1p``P``_rxreq_valid), \
            .cchi_t1p``P``_rxreq_bits_TxnID(t1p``P``_rxreq_bits_TxnID), \
            .cchi_t1p``P``_rxreq_bits_SrcID(t1p``P``_rxreq_bits_SrcID), \
            .cchi_t1p``P``_rxreq_bits_TgtID(t1p``P``_rxreq_bits_TgtID), \
            .cchi_t1p``P``_rxreq_bits_Opcode(t1p``P``_rxreq_bits_Opcode), \
            .cchi_t1p``P``_rxreq_bits_Size(t1p``P``_rxreq_bits_Size), \
            .cchi_t1p``P``_rxreq_bits_Addr(t1p``P``_rxreq_bits_Addr), \
            .cchi_t1p``P``_rxreq_bits_TagAlias(t1p``P``_rxreq_bits_TagAlias), \
            .cchi_t1p``P``_rxreq_bits_NS(t1p``P``_rxreq_bits_NS), \
            .cchi_t1p``P``_rxreq_bits_Order(t1p``P``_rxreq_bits_Order), \
            .cchi_t1p``P``_rxreq_bits_MemAttr(t1p``P``_rxreq_bits_MemAttr), \
            .cchi_t1p``P``_rxreq_bits_Excl(t1p``P``_rxreq_bits_Excl), \
            .cchi_t1p``P``_rxreq_bits_ExpCompData(t1p``P``_rxreq_bits_ExpCompData), \
            .cchi_t1p``P``_rxreq_bits_TraceTag(t1p``P``_rxreq_bits_TraceTag), \
            .cchi_t1p``P``_rxreq_bits_WayValid(t1p``P``_rxreq_bits_WayValid), \
            .cchi_t1p``P``_rxreq_bits_Way(t1p``P``_rxreq_bits_Way), \
            .cchi_t1p``P``_rxreq_ready(t1p``P``_rxreq_ready), \
            .cchi_t1p``P``_txsnp_ready(t1p``P``_txsnp_ready), \
            .cchi_t1p``P``_txsnp_valid(t1p``P``_txsnp_valid), \
            .cchi_t1p``P``_txsnp_bits_TxnID(t1p``P``_txsnp_bits_TxnID), \
            .cchi_t1p``P``_txsnp_bits_SrcID(t1p``P``_txsnp_bits_SrcID), \
            .cchi_t1p``P``_txsnp_bits_TgtID(t1p``P``_txsnp_bits_TgtID), \
            .cchi_t1p``P``_txsnp_bits_Opcode(t1p``P``_txsnp_bits_Opcode), \
            .cchi_t1p``P``_txsnp_bits_Addr(t1p``P``_txsnp_bits_Addr), \
            .cchi_t1p``P``_txsnp_bits_NS(t1p``P``_txsnp_bits_NS), \
            .cchi_t1p``P``_txsnp_bits_TraceTag(t1p``P``_txsnp_bits_TraceTag), \
            .cchi_t1p``P``_txrsp_ready(t1p``P``_txrsp_ready), \
            .cchi_t1p``P``_txrsp_valid(t1p``P``_txrsp_valid), \
            .cchi_t1p``P``_txrsp_bits_TxnID(t1p``P``_txrsp_bits_TxnID), \
            .cchi_t1p``P``_txrsp_bits_SrcID(t1p``P``_txrsp_bits_SrcID), \
            .cchi_t1p``P``_txrsp_bits_TgtID(t1p``P``_txrsp_bits_TgtID), \
            .cchi_t1p``P``_txrsp_bits_DBID(t1p``P``_txrsp_bits_DBID), \
            .cchi_t1p``P``_txrsp_bits_Opcode(t1p``P``_txrsp_bits_Opcode), \
            .cchi_t1p``P``_txrsp_bits_RespErr(t1p``P``_txrsp_bits_RespErr), \
            .cchi_t1p``P``_txrsp_bits_Resp(t1p``P``_txrsp_bits_Resp), \
            .cchi_t1p``P``_txrsp_bits_CBusy(t1p``P``_txrsp_bits_CBusy), \
            .cchi_t1p``P``_txrsp_bits_WayValid(t1p``P``_txrsp_bits_WayValid), \
            .cchi_t1p``P``_txrsp_bits_Way(t1p``P``_txrsp_bits_Way), \
            .cchi_t1p``P``_txrsp_bits_TraceTag(t1p``P``_txrsp_bits_TraceTag), \
            .cchi_t1p``P``_rxrsp_valid(t1p``P``_rxrsp_valid), \
            .cchi_t1p``P``_rxrsp_bits_TxnID(t1p``P``_rxrsp_bits_TxnID), \
            .cchi_t1p``P``_rxrsp_bits_SrcID(t1p``P``_rxrsp_bits_SrcID), \
            .cchi_t1p``P``_rxrsp_bits_TgtID(t1p``P``_rxrsp_bits_TgtID), \
            .cchi_t1p``P``_rxrsp_bits_Opcode(t1p``P``_rxrsp_bits_Opcode), \
            .cchi_t1p``P``_rxrsp_bits_RespErr(t1p``P``_rxrsp_bits_RespErr), \
            .cchi_t1p``P``_rxrsp_bits_Resp(t1p``P``_rxrsp_bits_Resp), \
            .cchi_t1p``P``_rxrsp_bits_TraceTag(t1p``P``_rxrsp_bits_TraceTag), \
            .cchi_t1p``P``_rxrsp_ready(t1p``P``_rxrsp_ready), \
            .cchi_t1p``P``_txdat_ready(t1p``P``_txdat_ready), \
            .cchi_t1p``P``_txdat_valid(t1p``P``_txdat_valid), \
            .cchi_t1p``P``_txdat_bits_TxnID(t1p``P``_txdat_bits_TxnID), \
            .cchi_t1p``P``_txdat_bits_SrcID(t1p``P``_txdat_bits_SrcID), \
            .cchi_t1p``P``_txdat_bits_TgtID(t1p``P``_txdat_bits_TgtID), \
            .cchi_t1p``P``_txdat_bits_DBID(t1p``P``_txdat_bits_DBID), \
            .cchi_t1p``P``_txdat_bits_Opcode(t1p``P``_txdat_bits_Opcode), \
            .cchi_t1p``P``_txdat_bits_RespErr(t1p``P``_txdat_bits_RespErr), \
            .cchi_t1p``P``_txdat_bits_Resp(t1p``P``_txdat_bits_Resp), \
            .cchi_t1p``P``_txdat_bits_DataSource(t1p``P``_txdat_bits_DataSource), \
            .cchi_t1p``P``_txdat_bits_CBusy(t1p``P``_txdat_bits_CBusy), \
            .cchi_t1p``P``_txdat_bits_DataID(t1p``P``_txdat_bits_DataID), \
            .cchi_t1p``P``_txdat_bits_Data(t1p``P``_txdat_bits_Data), \
            .cchi_t1p``P``_txdat_bits_WayValid(t1p``P``_txdat_bits_WayValid), \
            .cchi_t1p``P``_txdat_bits_Way(t1p``P``_txdat_bits_Way), \
            .cchi_t1p``P``_txdat_bits_TraceTag(t1p``P``_txdat_bits_TraceTag), \
            .cchi_t1p``P``_rxdat_valid(t1p``P``_rxdat_valid), \
            .cchi_t1p``P``_rxdat_bits_TxnID(t1p``P``_rxdat_bits_TxnID), \
            .cchi_t1p``P``_rxdat_bits_SrcID(t1p``P``_rxdat_bits_SrcID), \
            .cchi_t1p``P``_rxdat_bits_TgtID(t1p``P``_rxdat_bits_TgtID), \
            .cchi_t1p``P``_rxdat_bits_Opcode(t1p``P``_rxdat_bits_Opcode), \
            .cchi_t1p``P``_rxdat_bits_RespErr(t1p``P``_rxdat_bits_RespErr), \
            .cchi_t1p``P``_rxdat_bits_Resp(t1p``P``_rxdat_bits_Resp), \
            .cchi_t1p``P``_rxdat_bits_DataID(t1p``P``_rxdat_bits_DataID), \
            .cchi_t1p``P``_rxdat_bits_Data(t1p``P``_rxdat_bits_Data), \
            .cchi_t1p``P``_rxdat_bits_BE(t1p``P``_rxdat_bits_BE), \
            .cchi_t1p``P``_rxdat_bits_TraceTag(t1p``P``_rxdat_bits_TraceTag), \
            .cchi_t1p``P``_rxdat_ready(t1p``P``_rxdat_ready)
        `TB_T1_CONN(0),
        `TB_T1_CONN(1),
        `TB_T1_CONN(2),
        `TB_T1_CONN(3),
        `TB_T1_CONN(4),
        `TB_T1_CONN(5),
        `TB_T1_CONN(6),
        `TB_T1_CONN(7),
        .axi_m0_awvalid(axi_m0_awvalid), .axi_m0_awready(axi_m0_awready), .axi_m0_awaddr(axi_m0_awaddr),
        .axi_m0_awid(axi_m0_awid), .axi_m0_awlen(axi_m0_awlen), .axi_m0_awsize(axi_m0_awsize), .axi_m0_awburst(axi_m0_awburst),
        .axi_m0_wvalid(axi_m0_wvalid), .axi_m0_wready(axi_m0_wready), .axi_m0_wdata(axi_m0_wdata),
        .axi_m0_wstrb(axi_m0_wstrb), .axi_m0_wlast(axi_m0_wlast),
        .axi_m0_bvalid(axi_m0_bvalid), .axi_m0_bready(axi_m0_bready), .axi_m0_bid(axi_m0_bid), .axi_m0_bresp(axi_m0_bresp),
        .axi_m0_arvalid(axi_m0_arvalid), .axi_m0_arready(axi_m0_arready), .axi_m0_araddr(axi_m0_araddr),
        .axi_m0_arid(axi_m0_arid), .axi_m0_arlen(axi_m0_arlen), .axi_m0_arsize(axi_m0_arsize), .axi_m0_arburst(axi_m0_arburst),
        .axi_m0_rvalid(axi_m0_rvalid), .axi_m0_rready(axi_m0_rready), .axi_m0_rdata(axi_m0_rdata),
        .axi_m0_rid(axi_m0_rid), .axi_m0_rresp(axi_m0_rresp), .axi_m0_rlast(axi_m0_rlast),
        .axi_m1_awvalid(axi_m1_awvalid), .axi_m1_awready(axi_m1_awready), .axi_m1_awaddr(axi_m1_awaddr),
        .axi_m1_awid(axi_m1_awid), .axi_m1_awlen(axi_m1_awlen), .axi_m1_awsize(axi_m1_awsize), .axi_m1_awburst(axi_m1_awburst),
        .axi_m1_wvalid(axi_m1_wvalid), .axi_m1_wready(axi_m1_wready), .axi_m1_wdata(axi_m1_wdata),
        .axi_m1_wstrb(axi_m1_wstrb), .axi_m1_wlast(axi_m1_wlast),
        .axi_m1_bvalid(axi_m1_bvalid), .axi_m1_bready(axi_m1_bready), .axi_m1_bid(axi_m1_bid), .axi_m1_bresp(axi_m1_bresp),
        .axi_m1_arvalid(axi_m1_arvalid), .axi_m1_arready(axi_m1_arready), .axi_m1_araddr(axi_m1_araddr),
        .axi_m1_arid(axi_m1_arid), .axi_m1_arlen(axi_m1_arlen), .axi_m1_arsize(axi_m1_arsize), .axi_m1_arburst(axi_m1_arburst),
        .axi_m1_rvalid(axi_m1_rvalid), .axi_m1_rready(axi_m1_rready), .axi_m1_rdata(axi_m1_rdata),
        .axi_m1_rid(axi_m1_rid), .axi_m1_rresp(axi_m1_rresp), .axi_m1_rlast(axi_m1_rlast),
        .axi_m2_awvalid(axi_m2_awvalid), .axi_m2_awready(axi_m2_awready), .axi_m2_awaddr(axi_m2_awaddr),
        .axi_m2_awid(axi_m2_awid), .axi_m2_awlen(axi_m2_awlen), .axi_m2_awsize(axi_m2_awsize), .axi_m2_awburst(axi_m2_awburst),
        .axi_m2_wvalid(axi_m2_wvalid), .axi_m2_wready(axi_m2_wready), .axi_m2_wdata(axi_m2_wdata),
        .axi_m2_wstrb(axi_m2_wstrb), .axi_m2_wlast(axi_m2_wlast),
        .axi_m2_bvalid(axi_m2_bvalid), .axi_m2_bready(axi_m2_bready), .axi_m2_bid(axi_m2_bid), .axi_m2_bresp(axi_m2_bresp),
        .axi_m2_arvalid(axi_m2_arvalid), .axi_m2_arready(axi_m2_arready), .axi_m2_araddr(axi_m2_araddr),
        .axi_m2_arid(axi_m2_arid), .axi_m2_arlen(axi_m2_arlen), .axi_m2_arsize(axi_m2_arsize), .axi_m2_arburst(axi_m2_arburst),
        .axi_m2_rvalid(axi_m2_rvalid), .axi_m2_rready(axi_m2_rready), .axi_m2_rdata(axi_m2_rdata),
        .axi_m2_rid(axi_m2_rid), .axi_m2_rresp(axi_m2_rresp), .axi_m2_rlast(axi_m2_rlast),
        .axi_m3_awvalid(axi_m3_awvalid), .axi_m3_awready(axi_m3_awready), .axi_m3_awaddr(axi_m3_awaddr),
        .axi_m3_awid(axi_m3_awid), .axi_m3_awlen(axi_m3_awlen), .axi_m3_awsize(axi_m3_awsize), .axi_m3_awburst(axi_m3_awburst),
        .axi_m3_wvalid(axi_m3_wvalid), .axi_m3_wready(axi_m3_wready), .axi_m3_wdata(axi_m3_wdata),
        .axi_m3_wstrb(axi_m3_wstrb), .axi_m3_wlast(axi_m3_wlast),
        .axi_m3_bvalid(axi_m3_bvalid), .axi_m3_bready(axi_m3_bready), .axi_m3_bid(axi_m3_bid), .axi_m3_bresp(axi_m3_bresp),
        .axi_m3_arvalid(axi_m3_arvalid), .axi_m3_arready(axi_m3_arready), .axi_m3_araddr(axi_m3_araddr),
        .axi_m3_arid(axi_m3_arid), .axi_m3_arlen(axi_m3_arlen), .axi_m3_arsize(axi_m3_arsize), .axi_m3_arburst(axi_m3_arburst),
        .axi_m3_rvalid(axi_m3_rvalid), .axi_m3_rready(axi_m3_rready), .axi_m3_rdata(axi_m3_rdata),
        .axi_m3_rid(axi_m3_rid), .axi_m3_rresp(axi_m3_rresp), .axi_m3_rlast(axi_m3_rlast)
    );

    venus_axi_mem #(.ADDR_W(AXI_ADDR_W), .DATA_W(AXI_DATA_W), .ID_W(AXI_ID_W), .LAT(2)) u_mem (
        .clock(clock), .reset(reset),
        .axi_m_awvalid(axi_m0_awvalid), .axi_m_awready(axi_m0_awready), .axi_m_awaddr(axi_m0_awaddr),
        .axi_m_awid(axi_m0_awid), .axi_m_awlen(axi_m0_awlen), .axi_m_awsize(axi_m0_awsize), .axi_m_awburst(axi_m0_awburst),
        .axi_m_wvalid(axi_m0_wvalid), .axi_m_wready(axi_m0_wready), .axi_m_wdata(axi_m0_wdata),
        .axi_m_wstrb(axi_m0_wstrb), .axi_m_wlast(axi_m0_wlast),
        .axi_m_bvalid(axi_m0_bvalid), .axi_m_bready(axi_m0_bready), .axi_m_bid(axi_m0_bid), .axi_m_bresp(axi_m0_bresp),
        .axi_m_arvalid(axi_m0_arvalid), .axi_m_arready(axi_m0_arready), .axi_m_araddr(axi_m0_araddr),
        .axi_m_arid(axi_m0_arid), .axi_m_arlen(axi_m0_arlen), .axi_m_arsize(axi_m0_arsize), .axi_m_arburst(axi_m0_arburst),
        .axi_m_rvalid(axi_m0_rvalid), .axi_m_rready(axi_m0_rready), .axi_m_rdata(axi_m0_rdata),
        .axi_m_rid(axi_m0_rid), .axi_m_rresp(axi_m0_rresp), .axi_m_rlast(axi_m0_rlast)
    );

    assign axi_m1_awready = 1'b0;
    assign axi_m1_wready  = 1'b0;
    assign axi_m1_bvalid  = 1'b0;
    assign axi_m1_bid     = '0;
    assign axi_m1_bresp   = '0;
    assign axi_m1_arready = 1'b0;
    assign axi_m1_rvalid  = 1'b0;
    assign axi_m1_rdata   = '0;
    assign axi_m1_rid     = '0;
    assign axi_m1_rresp   = '0;
    assign axi_m1_rlast   = 1'b0;

    assign axi_m2_awready = 1'b0;
    assign axi_m2_wready  = 1'b0;
    assign axi_m2_bvalid  = 1'b0;
    assign axi_m2_bid     = '0;
    assign axi_m2_bresp   = '0;
    assign axi_m2_arready = 1'b0;
    assign axi_m2_rvalid  = 1'b0;
    assign axi_m2_rdata   = '0;
    assign axi_m2_rid     = '0;
    assign axi_m2_rresp   = '0;
    assign axi_m2_rlast   = 1'b0;

    assign axi_m3_awready = 1'b0;
    assign axi_m3_wready  = 1'b0;
    assign axi_m3_bvalid  = 1'b0;
    assign axi_m3_bid     = '0;
    assign axi_m3_bresp   = '0;
    assign axi_m3_arready = 1'b0;
    assign axi_m3_rvalid  = 1'b0;
    assign axi_m3_rdata   = '0;
    assign axi_m3_rid     = '0;
    assign axi_m3_rresp   = '0;
    assign axi_m3_rlast   = 1'b0;

    // ==================================================================
    // Ceiling-only upstream ports 4..7 (live when NUM_T1 > 4)
    // ==================================================================
    // The directed scenarios drive ports 0..3 only. Ports 4..7 never issue
    // requests and never hold a line, but at NUM_T1 > 4 an over-inclusive
    // snoop mask (maybe_all / SF_ALLOW_OVER) still snoops them — so they
    // auto-answer every snoop with SnpResp_I, in order, via a small TxnID
    // queue. Everything else on these ports is parked.
    `define TB_T1_AUTO_ANSWER(P) \
        assign t1p``P``_rxevt_valid = 1'b0; \
        assign t1p``P``_rxreq_valid = 1'b0; \
        assign t1p``P``_rxdat_valid = 1'b0; \
        assign t1p``P``_txsnp_ready = 1'b1; \
        assign t1p``P``_txrsp_ready = 1'b1; \
        assign t1p``P``_txdat_ready = 1'b1; \
        logic [6:0] t1p``P``_snpq [0:15]; \
        logic [3:0] t1p``P``_snpq_w, t1p``P``_snpq_r; \
        logic [4:0] t1p``P``_snpq_n; \
        always_ff @(posedge clock) begin \
            if (reset) begin \
                t1p``P``_snpq_w <= '0; \
                t1p``P``_snpq_r <= '0; \
                t1p``P``_snpq_n <= '0; \
                t1p``P``_rxrsp_valid <= 1'b0; \
                t1p``P``_rxrsp_bits_TxnID <= '0; \
                t1p``P``_rxrsp_bits_SrcID <= 5'(P); \
                t1p``P``_rxrsp_bits_TgtID <= 5'(NODE_ID); \
                t1p``P``_rxrsp_bits_Opcode <= UPRSP_SNPRESP; \
                t1p``P``_rxrsp_bits_RespErr <= 2'b00; \
                t1p``P``_rxrsp_bits_Resp <= RESP_I; \
                t1p``P``_rxrsp_bits_TraceTag <= 1'b0; \
            end else begin \
                if (t1p``P``_txsnp_valid) begin \
                    t1p``P``_snpq[t1p``P``_snpq_w] <= t1p``P``_txsnp_bits_TxnID; \
                    t1p``P``_snpq_w <= t1p``P``_snpq_w + 1; \
                end \
                if (t1p``P``_rxrsp_valid && t1p``P``_rxrsp_ready) begin \
                    t1p``P``_rxrsp_valid <= 1'b0; \
                    t1p``P``_snpq_r <= t1p``P``_snpq_r + 1; \
                end \
                if (!t1p``P``_rxrsp_valid && (t1p``P``_snpq_n != 0)) begin \
                    t1p``P``_rxrsp_valid <= 1'b1; \
                    t1p``P``_rxrsp_bits_TxnID <= t1p``P``_snpq[t1p``P``_snpq_r]; \
                end \
                t1p``P``_snpq_n <= t1p``P``_snpq_n + 5'(t1p``P``_txsnp_valid) \
                                   - 5'(t1p``P``_rxrsp_valid && t1p``P``_rxrsp_ready); \
            end \
        end

    `TB_T1_AUTO_ANSWER(4)
    `TB_T1_AUTO_ANSWER(5)
    `TB_T1_AUTO_ANSWER(6)
    `TB_T1_AUTO_ANSWER(7)

    `undef TB_T1_AUTO_ANSWER

    // ==================================================================
    // TB infrastructure
    // ==================================================================
    int errors;
    int timeout;

    logic [31:0] cd_data [0:1][0:7];   // captured CompData beats (by DataID)
    logic [2:0]  g_resp;               // last captured DnRSP Resp

    task automatic chk(input logic cond, input string msg);
        if (!cond) begin
            $error("%s", msg);
            errors++;
        end
    endtask

    task automatic idle_port(input int p);
        case (p)
        0: begin
            t1p0_rxevt_valid = 1'b0;
            t1p0_rxreq_valid = 1'b0;
            t1p0_rxrsp_valid = 1'b0;
            t1p0_rxdat_valid = 1'b0;
            t1p0_txsnp_ready = 1'b1;
            t1p0_txrsp_ready = 1'b1;
            t1p0_txdat_ready = 1'b1;
        end
        1: begin
            t1p1_rxevt_valid = 1'b0;
            t1p1_rxreq_valid = 1'b0;
            t1p1_rxrsp_valid = 1'b0;
            t1p1_rxdat_valid = 1'b0;
            t1p1_txsnp_ready = 1'b1;
            t1p1_txrsp_ready = 1'b1;
            t1p1_txdat_ready = 1'b1;
        end
        2: begin
            t1p2_rxevt_valid = 1'b0;
            t1p2_rxreq_valid = 1'b0;
            t1p2_rxrsp_valid = 1'b0;
            t1p2_rxdat_valid = 1'b0;
            t1p2_txsnp_ready = 1'b1;
            t1p2_txrsp_ready = 1'b1;
            t1p2_txdat_ready = 1'b1;
        end
        default: begin
            t1p3_rxevt_valid = 1'b0;
            t1p3_rxreq_valid = 1'b0;
            t1p3_rxrsp_valid = 1'b0;
            t1p3_rxdat_valid = 1'b0;
            t1p3_txsnp_ready = 1'b1;
            t1p3_txrsp_ready = 1'b1;
            t1p3_txdat_ready = 1'b1;
        end
        endcase
    endtask

    // ---- REQ/EVT drivers -------------------------------------------------
    task automatic drive_req_sz(input int p, input logic [5:0] opc, input logic [47:0] addr,
                                input logic [2:0] size, input logic [6:0] txn, input logic expc,
                                input logic [7:0] ta = 8'h00);
        case (p)
        0: begin
            t1p0_rxreq_valid = 1'b1; t1p0_rxreq_bits_TxnID = txn; t1p0_rxreq_bits_SrcID = 5'(p);
            t1p0_rxreq_bits_TgtID = 5'(NODE_ID); t1p0_rxreq_bits_Opcode = opc; t1p0_rxreq_bits_Size = size;
            t1p0_rxreq_bits_Addr = addr; t1p0_rxreq_bits_TagAlias = ta; t1p0_rxreq_bits_NS = 1'b0; t1p0_rxreq_bits_Order = 2'b00;
            t1p0_rxreq_bits_MemAttr = 4'b0000; t1p0_rxreq_bits_Excl = 1'b0; t1p0_rxreq_bits_ExpCompData = expc;
            t1p0_rxreq_bits_TraceTag = 1'b0; t1p0_rxreq_bits_WayValid = 1'b0; t1p0_rxreq_bits_Way = '0;
        end
        1: begin
            t1p1_rxreq_valid = 1'b1; t1p1_rxreq_bits_TxnID = txn; t1p1_rxreq_bits_SrcID = 5'(p);
            t1p1_rxreq_bits_TgtID = 5'(NODE_ID); t1p1_rxreq_bits_Opcode = opc; t1p1_rxreq_bits_Size = size;
            t1p1_rxreq_bits_Addr = addr; t1p1_rxreq_bits_TagAlias = ta; t1p1_rxreq_bits_NS = 1'b0; t1p1_rxreq_bits_Order = 2'b00;
            t1p1_rxreq_bits_MemAttr = 4'b0000; t1p1_rxreq_bits_Excl = 1'b0; t1p1_rxreq_bits_ExpCompData = expc;
            t1p1_rxreq_bits_TraceTag = 1'b0; t1p1_rxreq_bits_WayValid = 1'b0; t1p1_rxreq_bits_Way = '0;
        end
        2: begin
            t1p2_rxreq_valid = 1'b1; t1p2_rxreq_bits_TxnID = txn; t1p2_rxreq_bits_SrcID = 5'(p);
            t1p2_rxreq_bits_TgtID = 5'(NODE_ID); t1p2_rxreq_bits_Opcode = opc; t1p2_rxreq_bits_Size = size;
            t1p2_rxreq_bits_Addr = addr; t1p2_rxreq_bits_TagAlias = ta; t1p2_rxreq_bits_NS = 1'b0; t1p2_rxreq_bits_Order = 2'b00;
            t1p2_rxreq_bits_MemAttr = 4'b0000; t1p2_rxreq_bits_Excl = 1'b0; t1p2_rxreq_bits_ExpCompData = expc;
            t1p2_rxreq_bits_TraceTag = 1'b0; t1p2_rxreq_bits_WayValid = 1'b0; t1p2_rxreq_bits_Way = '0;
        end
        default: begin
            t1p3_rxreq_valid = 1'b1; t1p3_rxreq_bits_TxnID = txn; t1p3_rxreq_bits_SrcID = 5'(p);
            t1p3_rxreq_bits_TgtID = 5'(NODE_ID); t1p3_rxreq_bits_Opcode = opc; t1p3_rxreq_bits_Size = size;
            t1p3_rxreq_bits_Addr = addr; t1p3_rxreq_bits_TagAlias = ta; t1p3_rxreq_bits_NS = 1'b0; t1p3_rxreq_bits_Order = 2'b00;
            t1p3_rxreq_bits_MemAttr = 4'b0000; t1p3_rxreq_bits_Excl = 1'b0; t1p3_rxreq_bits_ExpCompData = expc;
            t1p3_rxreq_bits_TraceTag = 1'b0; t1p3_rxreq_bits_WayValid = 1'b0; t1p3_rxreq_bits_Way = '0;
        end
        endcase
    endtask

    task automatic clear_req(input int p);
        case (p)
        0: t1p0_rxreq_valid = 1'b0;
        1: t1p1_rxreq_valid = 1'b0;
        2: t1p2_rxreq_valid = 1'b0;
        default: t1p3_rxreq_valid = 1'b0;
        endcase
    endtask

    function automatic logic req_ready(input int p);
        case (p)
        0: return t1p0_rxreq_ready;
        1: return t1p1_rxreq_ready;
        2: return t1p2_rxreq_ready;
        default: return t1p3_rxreq_ready;
        endcase
    endfunction

    task automatic send_req_sz(input int p, input logic [5:0] opc, input logic [47:0] addr,
                               input logic [2:0] size, input logic [6:0] txn, input logic expc,
                               input logic [7:0] ta = 8'h00);
        timeout = 0;
        drive_req_sz(p, opc, addr, size, txn, expc, ta);
        @(posedge clock);
        while (!req_ready(p)) begin
            timeout++;
            if (timeout > 2000) begin
                $error("timeout waiting for req ready port %0d", p);
                errors++;
                break;
            end
            @(posedge clock);
        end
        clear_req(p);
    endtask

    task automatic send_req(input int p, input logic [5:0] opc, input logic [47:0] addr,
                            input logic [6:0] txn, input logic expc, input logic [7:0] ta = 8'h00);
        send_req_sz(p, opc, addr, 3'd6, txn, expc, ta);
    endtask

    task automatic send_evt(input int p, input logic opc, input logic [47:0] addr, input logic [6:0] txn);
        case (p)
        0: begin
            t1p0_rxevt_valid = 1'b1; t1p0_rxevt_bits_TxnID = txn; t1p0_rxevt_bits_SrcID = 5'(p);
            t1p0_rxevt_bits_TgtID = 5'(NODE_ID); t1p0_rxevt_bits_Opcode = opc; t1p0_rxevt_bits_Addr = addr;
            t1p0_rxevt_bits_NS = 1'b0; t1p0_rxevt_bits_MemAttr = 1'b0;
            t1p0_rxevt_bits_WayValid = 1'b0; t1p0_rxevt_bits_Way = '0; t1p0_rxevt_bits_TraceTag = 1'b0;
        end
        1: begin
            t1p1_rxevt_valid = 1'b1; t1p1_rxevt_bits_TxnID = txn; t1p1_rxevt_bits_SrcID = 5'(p);
            t1p1_rxevt_bits_TgtID = 5'(NODE_ID); t1p1_rxevt_bits_Opcode = opc; t1p1_rxevt_bits_Addr = addr;
            t1p1_rxevt_bits_NS = 1'b0; t1p1_rxevt_bits_MemAttr = 1'b0;
            t1p1_rxevt_bits_WayValid = 1'b0; t1p1_rxevt_bits_Way = '0; t1p1_rxevt_bits_TraceTag = 1'b0;
        end
        2: begin
            t1p2_rxevt_valid = 1'b1; t1p2_rxevt_bits_TxnID = txn; t1p2_rxevt_bits_SrcID = 5'(p);
            t1p2_rxevt_bits_TgtID = 5'(NODE_ID); t1p2_rxevt_bits_Opcode = opc; t1p2_rxevt_bits_Addr = addr;
            t1p2_rxevt_bits_NS = 1'b0; t1p2_rxevt_bits_MemAttr = 1'b0;
            t1p2_rxevt_bits_WayValid = 1'b0; t1p2_rxevt_bits_Way = '0; t1p2_rxevt_bits_TraceTag = 1'b0;
        end
        default: begin
            t1p3_rxevt_valid = 1'b1; t1p3_rxevt_bits_TxnID = txn; t1p3_rxevt_bits_SrcID = 5'(p);
            t1p3_rxevt_bits_TgtID = 5'(NODE_ID); t1p3_rxevt_bits_Opcode = opc; t1p3_rxevt_bits_Addr = addr;
            t1p3_rxevt_bits_NS = 1'b0; t1p3_rxevt_bits_MemAttr = 1'b0;
            t1p3_rxevt_bits_WayValid = 1'b0; t1p3_rxevt_bits_Way = '0; t1p3_rxevt_bits_TraceTag = 1'b0;
        end
        endcase
        @(posedge clock);
        timeout = 0;
        while (!((p == 0) ? t1p0_rxevt_ready : (p == 1) ? t1p1_rxevt_ready
                 : (p == 2) ? t1p2_rxevt_ready : t1p3_rxevt_ready)) begin
            timeout++;
            if (timeout > 2000) begin
                $error("timeout EVT ready port %0d", p);
                errors++;
                break;
            end
            @(posedge clock);
        end
        case (p)
        0: t1p0_rxevt_valid = 1'b0;
        1: t1p1_rxevt_valid = 1'b0;
        2: t1p2_rxevt_valid = 1'b0;
        default: t1p3_rxevt_valid = 1'b0;
        endcase
    endtask

    // ---- downstream monitors ---------------------------------------------
    function automatic logic rsp_valid(input int p);
        case (p)
        0: return t1p0_txrsp_valid;
        1: return t1p1_txrsp_valid;
        2: return t1p2_txrsp_valid;
        default: return t1p3_txrsp_valid;
        endcase
    endfunction

    function automatic logic [2:0] rsp_opc(input int p);
        case (p)
        0: return t1p0_txrsp_bits_Opcode;
        1: return t1p1_txrsp_bits_Opcode;
        2: return t1p2_txrsp_bits_Opcode;
        default: return t1p3_txrsp_bits_Opcode;
        endcase
    endfunction

    function automatic logic [2:0] rsp_resp(input int p);
        case (p)
        0: return t1p0_txrsp_bits_Resp;
        1: return t1p1_txrsp_bits_Resp;
        2: return t1p2_txrsp_bits_Resp;
        default: return t1p3_txrsp_bits_Resp;
        endcase
    endfunction

    function automatic logic [6:0] rsp_dbid(input int p);
        case (p)
        0: return t1p0_txrsp_bits_DBID;
        1: return t1p1_txrsp_bits_DBID;
        2: return t1p2_txrsp_bits_DBID;
        default: return t1p3_txrsp_bits_DBID;
        endcase
    endfunction

    function automatic logic dat_valid(input int p);
        case (p)
        0: return t1p0_txdat_valid;
        1: return t1p1_txdat_valid;
        2: return t1p2_txdat_valid;
        default: return t1p3_txdat_valid;
        endcase
    endfunction

    function automatic logic [2:0] dat_resp(input int p);
        case (p)
        0: return t1p0_txdat_bits_Resp;
        1: return t1p1_txdat_bits_Resp;
        2: return t1p2_txdat_bits_Resp;
        default: return t1p3_txdat_bits_Resp;
        endcase
    endfunction

    function automatic logic [6:0] dat_dbid(input int p);
        case (p)
        0: return t1p0_txdat_bits_DBID;
        1: return t1p1_txdat_bits_DBID;
        2: return t1p2_txdat_bits_DBID;
        default: return t1p3_txdat_bits_DBID;
        endcase
    endfunction

    function automatic logic dat_dataid(input int p);
        case (p)
        0: return t1p0_txdat_bits_DataID;
        1: return t1p1_txdat_bits_DataID;
        2: return t1p2_txdat_bits_DataID;
        default: return t1p3_txdat_bits_DataID;
        endcase
    endfunction

    function automatic logic [31:0] dat_word(input int p, input int i);
        case (p)
        0: return t1p0_txdat_bits_Data[i];
        1: return t1p1_txdat_bits_Data[i];
        2: return t1p2_txdat_bits_Data[i];
        default: return t1p3_txdat_bits_Data[i];
        endcase
    endfunction

    function automatic logic snp_valid(input int p);
        case (p)
        0: return t1p0_txsnp_valid;
        1: return t1p1_txsnp_valid;
        2: return t1p2_txsnp_valid;
        default: return t1p3_txsnp_valid;
        endcase
    endfunction

    function automatic logic [6:0] snp_txn(input int p);
        case (p)
        0: return t1p0_txsnp_bits_TxnID;
        1: return t1p1_txsnp_bits_TxnID;
        2: return t1p2_txsnp_bits_TxnID;
        default: return t1p3_txsnp_bits_TxnID;
        endcase
    endfunction

    function automatic logic [1:0] snp_opc(input int p);
        case (p)
        0: return t1p0_txsnp_bits_Opcode;
        1: return t1p1_txsnp_bits_Opcode;
        2: return t1p2_txsnp_bits_Opcode;
        default: return t1p3_txsnp_bits_Opcode;
        endcase
    endfunction

    // ---- UpRSP / UpDAT drivers --------------------------------------------
    task automatic drive_uprsp(input int p, input logic opc, input logic [6:0] txn, input logic [2:0] resp);
        case (p)
        0: begin
            t1p0_rxrsp_valid = 1'b1; t1p0_rxrsp_bits_TxnID = txn; t1p0_rxrsp_bits_SrcID = 5'(p);
            t1p0_rxrsp_bits_TgtID = 5'(NODE_ID); t1p0_rxrsp_bits_Opcode = opc;
            t1p0_rxrsp_bits_RespErr = 2'b00; t1p0_rxrsp_bits_Resp = resp; t1p0_rxrsp_bits_TraceTag = 1'b0;
        end
        1: begin
            t1p1_rxrsp_valid = 1'b1; t1p1_rxrsp_bits_TxnID = txn; t1p1_rxrsp_bits_SrcID = 5'(p);
            t1p1_rxrsp_bits_TgtID = 5'(NODE_ID); t1p1_rxrsp_bits_Opcode = opc;
            t1p1_rxrsp_bits_RespErr = 2'b00; t1p1_rxrsp_bits_Resp = resp; t1p1_rxrsp_bits_TraceTag = 1'b0;
        end
        2: begin
            t1p2_rxrsp_valid = 1'b1; t1p2_rxrsp_bits_TxnID = txn; t1p2_rxrsp_bits_SrcID = 5'(p);
            t1p2_rxrsp_bits_TgtID = 5'(NODE_ID); t1p2_rxrsp_bits_Opcode = opc;
            t1p2_rxrsp_bits_RespErr = 2'b00; t1p2_rxrsp_bits_Resp = resp; t1p2_rxrsp_bits_TraceTag = 1'b0;
        end
        default: begin
            t1p3_rxrsp_valid = 1'b1; t1p3_rxrsp_bits_TxnID = txn; t1p3_rxrsp_bits_SrcID = 5'(p);
            t1p3_rxrsp_bits_TgtID = 5'(NODE_ID); t1p3_rxrsp_bits_Opcode = opc;
            t1p3_rxrsp_bits_RespErr = 2'b00; t1p3_rxrsp_bits_Resp = resp; t1p3_rxrsp_bits_TraceTag = 1'b0;
        end
        endcase
    endtask

    task automatic clear_uprsp(input int p);
        case (p)
        0: t1p0_rxrsp_valid = 1'b0;
        1: t1p1_rxrsp_valid = 1'b0;
        2: t1p2_rxrsp_valid = 1'b0;
        default: t1p3_rxrsp_valid = 1'b0;
        endcase
    endtask

    function automatic logic uprsp_ready(input int p);
        case (p)
        0: return t1p0_rxrsp_ready;
        1: return t1p1_rxrsp_ready;
        2: return t1p2_rxrsp_ready;
        default: return t1p3_rxrsp_ready;
        endcase
    endfunction

    task automatic send_uprsp(input int p, input logic opc, input logic [6:0] txn, input logic [2:0] resp);
        timeout = 0;
        drive_uprsp(p, opc, txn, resp);
        @(posedge clock);
        while (!uprsp_ready(p)) begin
            timeout++;
            if (timeout > 2000) begin
                $error("timeout UpRSP ready port %0d", p);
                errors++;
                break;
            end
            @(posedge clock);
        end
        clear_uprsp(p);
    endtask

    task automatic send_compack(input int p, input logic [6:0] dbid);
        send_uprsp(p, UPRSP_COMPACK, dbid, 3'b000);
    endtask

    task automatic drive_updat(input int p, input logic [1:0] opc, input logic [6:0] txn,
                               input logic [2:0] resp, input logic dataid, input logic [31:0] be,
                               input logic [31:0] base);
        logic [31:0] d [0:7];
        for (int i = 0; i < 8; i++)
            d[i] = base + 32'(i);
        case (p)
        0: begin
            t1p0_rxdat_valid = 1'b1; t1p0_rxdat_bits_TxnID = txn; t1p0_rxdat_bits_SrcID = 5'(p);
            t1p0_rxdat_bits_TgtID = 5'(NODE_ID); t1p0_rxdat_bits_Opcode = opc;
            t1p0_rxdat_bits_RespErr = 2'b00; t1p0_rxdat_bits_Resp = resp; t1p0_rxdat_bits_DataID = dataid;
            t1p0_rxdat_bits_BE = be; t1p0_rxdat_bits_TraceTag = 1'b0;
            for (int i = 0; i < 8; i++) t1p0_rxdat_bits_Data[i] = d[i];
        end
        1: begin
            t1p1_rxdat_valid = 1'b1; t1p1_rxdat_bits_TxnID = txn; t1p1_rxdat_bits_SrcID = 5'(p);
            t1p1_rxdat_bits_TgtID = 5'(NODE_ID); t1p1_rxdat_bits_Opcode = opc;
            t1p1_rxdat_bits_RespErr = 2'b00; t1p1_rxdat_bits_Resp = resp; t1p1_rxdat_bits_DataID = dataid;
            t1p1_rxdat_bits_BE = be; t1p1_rxdat_bits_TraceTag = 1'b0;
            for (int i = 0; i < 8; i++) t1p1_rxdat_bits_Data[i] = d[i];
        end
        2: begin
            t1p2_rxdat_valid = 1'b1; t1p2_rxdat_bits_TxnID = txn; t1p2_rxdat_bits_SrcID = 5'(p);
            t1p2_rxdat_bits_TgtID = 5'(NODE_ID); t1p2_rxdat_bits_Opcode = opc;
            t1p2_rxdat_bits_RespErr = 2'b00; t1p2_rxdat_bits_Resp = resp; t1p2_rxdat_bits_DataID = dataid;
            t1p2_rxdat_bits_BE = be; t1p2_rxdat_bits_TraceTag = 1'b0;
            for (int i = 0; i < 8; i++) t1p2_rxdat_bits_Data[i] = d[i];
        end
        default: begin
            t1p3_rxdat_valid = 1'b1; t1p3_rxdat_bits_TxnID = txn; t1p3_rxdat_bits_SrcID = 5'(p);
            t1p3_rxdat_bits_TgtID = 5'(NODE_ID); t1p3_rxdat_bits_Opcode = opc;
            t1p3_rxdat_bits_RespErr = 2'b00; t1p3_rxdat_bits_Resp = resp; t1p3_rxdat_bits_DataID = dataid;
            t1p3_rxdat_bits_BE = be; t1p3_rxdat_bits_TraceTag = 1'b0;
            for (int i = 0; i < 8; i++) t1p3_rxdat_bits_Data[i] = d[i];
        end
        endcase
    endtask

    task automatic clear_updat(input int p);
        case (p)
        0: t1p0_rxdat_valid = 1'b0;
        1: t1p1_rxdat_valid = 1'b0;
        2: t1p2_rxdat_valid = 1'b0;
        default: t1p3_rxdat_valid = 1'b0;
        endcase
    endtask

    function automatic logic updat_ready(input int p);
        case (p)
        0: return t1p0_rxdat_ready;
        1: return t1p1_rxdat_ready;
        2: return t1p2_rxdat_ready;
        default: return t1p3_rxdat_ready;
        endcase
    endfunction

    task automatic send_updat_beat(input int p, input logic [1:0] opc, input logic [6:0] txn,
                                   input logic [2:0] resp, input logic dataid, input logic [31:0] be,
                                   input logic [31:0] base);
        timeout = 0;
        drive_updat(p, opc, txn, resp, dataid, be, base);
        @(posedge clock);
        while (!updat_ready(p)) begin
            timeout++;
            if (timeout > 2000) begin
                $error("timeout UpDAT ready port %0d", p);
                errors++;
                break;
            end
            @(posedge clock);
        end
        clear_updat(p);
    endtask

    // Answer every snoop on every UNMASKED port with SnpResp_I for N cycles.
    // Masked ports are left for scenario-specific handling.
    task automatic answer_snoops_i_x(input int cycles, input logic [3:0] skip);
        int i;
        for (i = 0; i < cycles; i++) begin
            @(posedge clock);
            if (!skip[0]) begin
                if (snp_valid(0)) drive_uprsp(0, UPRSP_SNPRESP, snp_txn(0), RESP_I);
                else              clear_uprsp(0);
            end
            if (!skip[1]) begin
                if (snp_valid(1)) drive_uprsp(1, UPRSP_SNPRESP, snp_txn(1), RESP_I);
                else              clear_uprsp(1);
            end
            if (!skip[2]) begin
                if (snp_valid(2)) drive_uprsp(2, UPRSP_SNPRESP, snp_txn(2), RESP_I);
                else              clear_uprsp(2);
            end
            if (!skip[3]) begin
                if (snp_valid(3)) drive_uprsp(3, UPRSP_SNPRESP, snp_txn(3), RESP_I);
                else              clear_uprsp(3);
            end
        end
        clear_uprsp(0);
        clear_uprsp(1);
        clear_uprsp(2);
        clear_uprsp(3);
    endtask

    // ---- completion waiters -------------------------------------------------
    task automatic wait_compdata(input int p, input logic [2:0] expect_resp, input int nbeats,
                                 output logic [6:0] dbid);
        int beats;
        beats = 0;
        timeout = 0;
        dbid = '0;
        while (beats < nbeats) begin
            @(posedge clock);
            timeout++;
            if (timeout > 4000) begin
                $error("timeout waiting CompData port %0d", p);
                errors++;
                return;
            end
            if (dat_valid(p)) begin
                if (dat_resp(p) !== expect_resp) begin
                    $error("CompData resp %0d != %0d on port %0d", dat_resp(p), expect_resp, p);
                    errors++;
                end
                for (int i = 0; i < 8; i++)
                    cd_data[dat_dataid(p)][i] = dat_word(p, i);
                dbid = dat_dbid(p);
                beats++;
            end
        end
    endtask

    task automatic wait_dnrsp(input int p, input logic [2:0] expect_opc, output logic [6:0] dbid);
        timeout = 0;
        dbid = '0;
        forever begin
            @(posedge clock);
            timeout++;
            if (timeout > 4000) begin
                $error("timeout waiting DnRSP opc %0d port %0d", expect_opc, p);
                errors++;
                return;
            end
            if (rsp_valid(p) && rsp_opc(p) == expect_opc) begin
                dbid   = rsp_dbid(p);
                g_resp = rsp_resp(p);
                return;
            end
        end
    endtask

    // Assert no downstream response/data for a number of cycles (Stash ECD=0).
    task automatic expect_silent_port(input int p, input int cycles);
        for (int i = 0; i < cycles; i++) begin
            @(posedge clock);
            if (rsp_valid(p) || dat_valid(p)) begin
                $error("unexpected downstream flit on port %0d (expected silence)", p);
                errors++;
                return;
            end
        end
    endtask

    // Assert no snoop is emitted to port p for a number of cycles (TagAlias
    // scenarios: a same-alias requester must never be self-snooped).
    task automatic expect_silent_snoop(input int p, input int cycles);
        for (int i = 0; i < cycles; i++) begin
            @(posedge clock);
            if (snp_valid(p)) begin
                $error("unexpected snoop on port %0d (expected silence)", p);
                errors++;
                return;
            end
        end
    endtask

    // ---- snoop answerers -----------------------------------------------------
    // Answer every snoop on every port with SnpResp_I for N cycles.
    task automatic answer_snoops_i(input int cycles);
        int i;
        for (i = 0; i < cycles; i++) begin
            @(posedge clock);
            if (snp_valid(0)) drive_uprsp(0, UPRSP_SNPRESP, snp_txn(0), RESP_I);
            else              clear_uprsp(0);
            if (snp_valid(1)) drive_uprsp(1, UPRSP_SNPRESP, snp_txn(1), RESP_I);
            else              clear_uprsp(1);
            if (snp_valid(2)) drive_uprsp(2, UPRSP_SNPRESP, snp_txn(2), RESP_I);
            else              clear_uprsp(2);
            if (snp_valid(3)) drive_uprsp(3, UPRSP_SNPRESP, snp_txn(3), RESP_I);
            else              clear_uprsp(3);
        end
        clear_uprsp(0);
        clear_uprsp(1);
        clear_uprsp(2);
        clear_uprsp(3);
    endtask

    // Wait for a snoop on port p (checking its opcode), answer SnpResp with resp.
    task automatic answer_snoop_resp(input int p, input logic [1:0] expect_opc, input logic [2:0] resp);
        logic [6:0] txn;
        timeout = 0;
        forever begin
            @(posedge clock);
            timeout++;
            if (timeout > 4000) begin
                $error("timeout waiting snoop port %0d", p);
                errors++;
                return;
            end
            if (snp_valid(p)) begin
                if (snp_opc(p) !== expect_opc) begin
                    $error("snoop opc %0d != %0d on port %0d", snp_opc(p), expect_opc, p);
                    errors++;
                end
                txn = snp_txn(p);
                break;
            end
        end
        send_uprsp(p, UPRSP_SNPRESP, txn, resp);
    endtask

    // Wait for a snoop on port p (checking its opcode), answer with two
    // SnpRespData beats carrying resp_pd and the given data pattern.
    task automatic answer_snoop_data(input int p, input logic [1:0] expect_opc,
                                     input logic [2:0] resp_pd, input logic [31:0] base);
        logic [6:0] txn;
        timeout = 0;
        forever begin
            @(posedge clock);
            timeout++;
            if (timeout > 4000) begin
                $error("timeout waiting snoop port %0d", p);
                errors++;
                return;
            end
            if (snp_valid(p)) begin
                if (snp_opc(p) !== expect_opc) begin
                    $error("snoop opc %0d != %0d on port %0d", snp_opc(p), expect_opc, p);
                    errors++;
                end
                txn = snp_txn(p);
                break;
            end
        end
        send_updat_beat(p, UPDAT_SNPRESPDATA, txn, resp_pd, 1'b0, 32'hFFFF_FFFF, base);
        send_updat_beat(p, UPDAT_SNPRESPDATA, txn, resp_pd, 1'b1, 32'hFFFF_FFFF, base + 32'h1000);
    endtask

    // ---- data checkers --------------------------------------------------------
    task automatic check_cd_pat(input logic [31:0] base0, input logic [31:0] base1, input int nbeats);
        for (int i = 0; i < 8; i++) begin
            if (cd_data[0][i] !== base0 + 32'(i)) begin
                $error("CompData beat0 word%0d 0x%08h != 0x%08h", i, cd_data[0][i], base0 + 32'(i));
                errors++;
            end
        end
        if (nbeats == 2) begin
            for (int i = 0; i < 8; i++) begin
                if (cd_data[1][i] !== base1 + 32'(i)) begin
                    $error("CompData beat1 word%0d 0x%08h != 0x%08h", i, cd_data[1][i], base1 + 32'(i));
                    errors++;
                end
            end
        end
    endtask

    task automatic check_cd_zero(input int nbeats);
        for (int i = 0; i < 8; i++) begin
            if (cd_data[0][i] !== 32'h0) begin
                $error("CompData beat0 word%0d 0x%08h != 0", i, cd_data[0][i]);
                errors++;
            end
        end
        if (nbeats == 2) begin
            for (int i = 0; i < 8; i++) begin
                if (cd_data[1][i] !== 32'h0) begin
                    $error("CompData beat1 word%0d 0x%08h != 0", i, cd_data[1][i]);
                    errors++;
                end
            end
        end
    endtask

    // ==================================================================
    // Stimulus
    // ==================================================================
    initial begin
        logic [6:0] dbid;
        logic [6:0] dbid2;
        logic [6:0] snptxn;
        bit         wb_done;
        int i;
        errors = 0;
        reset = 1'b1;
        for (i = 0; i < 4; i++)
            idle_port(i);
        repeat (8) @(posedge clock);
        reset = 1'b0;
        repeat (4) @(posedge clock);

        // ---- S1: ReadShared cold miss -> promotion to UC (§7.4) ----
        $display("TB S1: ReadShared port0 line 0x1000 (cold, AXI fill, CompData UC)");
        send_req(0, REQ_READ_SHARED, 48'h1000, 7'd1, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        check_cd_zero(2);
        send_compack(0, dbid);
        repeat (20) @(posedge clock);

        // ---- S2: ReadUnique port1 same line (snoop owner SnpToInvalid) ----
        $display("TB S2: ReadUnique port1 line 0x1000");
        send_req(1, REQ_READ_UNIQUE, 48'h1000, 7'd2, 1'b1);
        fork
            answer_snoop_resp(0, SNP_TO_INVALID, RESP_I);
            wait_compdata(1, RESP_UC, 2, dbid);
        join
        check_cd_zero(2);
        send_compack(1, dbid);
        repeat (20) @(posedge clock);

        // ---- S3: MakeUnique port2 same line (snoop owner SnpMakeInvalid) ----
        $display("TB S3: MakeUnique port2 line 0x1000");
        send_req(2, REQ_MAKE_UNIQUE, 48'h1000, 7'd3, 1'b0);
        fork
            answer_snoop_resp(1, SNP_MAKE_INVALID, RESP_I);
            wait_dnrsp(2, DNRSP_COMP, dbid);
        join
        send_compack(2, dbid);
        repeat (20) @(posedge clock);

        // ---- S4: Evict port2 line 0x1000 ----
        $display("TB S4: Evict port2 line 0x1000");
        send_evt(2, EVT_EVICT, 48'h1000, 7'd4);
        wait_dnrsp(2, DNRSP_COMP, dbid);
        repeat (20) @(posedge clock);

        // ---- S5: WriteBackFull port0 line 0x2000 ----
        $display("TB S5: WriteBackFull port0 line 0x2000");
        send_evt(0, EVT_WRITE_BACK_FULL, 48'h2000, 7'd5);
        wait_dnrsp(0, DNRSP_COMPDBID, dbid);
        send_updat_beat(0, UPDAT_CB_WR, dbid, RESP_I_PD, 1'b0, 32'hFFFF_FFFF, 32'hA5A5_0000);
        send_updat_beat(0, UPDAT_CB_WR, dbid, RESP_I_PD, 1'b1, 32'hFFFF_FFFF, 32'h5A5A_1000);
        repeat (40) @(posedge clock);

        // ---- S6: re-ReadShared 0x2000 -> UC + WB data ----
        $display("TB S6: ReadShared port0 line 0x2000 (promotion, WB data)");
        send_req(0, REQ_READ_SHARED, 48'h2000, 7'd6, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        check_cd_pat(32'hA5A5_0000, 32'h5A5A_1000, 2);
        send_compack(0, dbid);
        repeat (20) @(posedge clock);

        // ---- S7: ReadNoSnp whole line ----
        $display("TB S7: ReadNoSnp port1 line 0x3000");
        send_req(1, REQ_READ_NOSNP, 48'h3000, 7'd7, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        check_cd_zero(2);
        repeat (20) @(posedge clock);

        // ---- S8: ReadNoSnp sub-line (Size=4 -> single beat) ----
        $display("TB S8: ReadNoSnp port1 addr 0x3010 size 16B");
        send_req_sz(1, REQ_READ_NOSNP, 48'h3010, 3'd4, 7'd8, 1'b1);
        wait_compdata(1, RESP_UC, 1, dbid);
        repeat (20) @(posedge clock);

        // ---- S9: ReadOnce miss ----
        $display("TB S9: ReadOnce port2 line 0x4000 (miss)");
        send_req(2, REQ_READ_ONCE, 48'h4000, 7'd9, 1'b1);
        wait_compdata(2, RESP_UC, 2, dbid);
        check_cd_zero(2);
        repeat (20) @(posedge clock);

        // ---- S10: ReadOnce dirty-U hit (PD + writeback) ----
        $display("TB S10: RS port0 0x4000 (promote), then ReadOnce port1 (PD hit)");
        send_req(0, REQ_READ_SHARED, 48'h4000, 7'd10, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_ONCE, 48'h4000, 7'd11, 1'b1);
        fork
            answer_snoop_data(0, SNP_TO_CLEAN, RESP_UC_PD, 32'hC0DE_0000);
            wait_compdata(1, RESP_UC, 2, dbid);
        join
        check_cd_pat(32'hC0DE_0000, 32'hC0DE_1000, 2);
        repeat (20) @(posedge clock);

        // ---- S11: WriteUniqueFull port0 line 0x5000 + verify ----
        $display("TB S11: WriteUniqueFull port0 line 0x5000");
        send_req(0, REQ_WRITE_UNIQUE_FULL, 48'h5000, 7'd12, 1'b0);
        wait_dnrsp(0, DNRSP_DBIDRESP, dbid);
        send_updat_beat(0, UPDAT_NCB_WR, dbid, RESP_I, 1'b0, 32'hFFFF_FFFF, 32'hF00D_0000);
        send_updat_beat(0, UPDAT_NCB_WR, dbid, RESP_I, 1'b1, 32'hFFFF_FFFF, 32'hF00D_1000);
        wait_dnrsp(0, DNRSP_COMP, dbid2);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_NOSNP, 48'h5000, 7'd13, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        check_cd_pat(32'hF00D_0000, 32'hF00D_1000, 2);
        repeat (10) @(posedge clock);

        // ---- S12: WriteUniquePtl with PD merge under BE ----
        $display("TB S12: RS port2 0x5000, then WriteUniquePtl port1 (PD merge)");
        send_req(2, REQ_READ_SHARED, 48'h5000, 7'd14, 1'b1);
        wait_compdata(2, RESP_UC, 2, dbid);
        send_compack(2, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_WRITE_UNIQUE_PTL, 48'h5000, 7'd15, 1'b0);
        fork
            answer_snoop_data(2, SNP_TO_INVALID, RESP_I_PD, 32'hD1FF_0000);
            begin
                wait_dnrsp(1, DNRSP_DBIDRESP, dbid);
                // write words 0..3 of beat 0 only (BE = bytes 0..15)
                send_updat_beat(1, UPDAT_NCB_WR, dbid, RESP_I, 1'b0, 32'h0000_FFFF, 32'hBEEF_0000);
                send_updat_beat(1, UPDAT_NCB_WR, dbid, RESP_I, 1'b1, 32'h0000_0000, 32'hBEEF_1000);
                wait_dnrsp(1, DNRSP_COMP, dbid2);
            end
        join
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_NOSNP, 48'h5000, 7'd16, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        // merged: beat0 words 0-3 = write (BEEF_0000+i), words 4-7 = PD (D1FF_0000+i);
        // beat1 fully PD (D1FF_1000+i).
        for (i = 0; i < 4; i++)
            chk(cd_data[0][i] === 32'hBEEF_0000 + 32'(i), "S12 merged write words wrong");
        for (i = 4; i < 8; i++)
            chk(cd_data[0][i] === 32'hD1FF_0000 + 32'(i), "S12 merged PD words wrong");
        for (i = 0; i < 8; i++)
            chk(cd_data[1][i] === 32'hD1FF_1000 + 32'(i), "S12 beat1 PD words wrong");
        repeat (10) @(posedge clock);

        // ---- S13: WriteNoSnpFull + WriteNoSnpPtl sub-line ----
        $display("TB S13: WriteNoSnpFull/Ptl port2 lines 0x6000/0x6040");
        send_req(2, REQ_WRITE_NOSNP_FULL, 48'h6000, 7'd17, 1'b0);
        wait_dnrsp(2, DNRSP_DBIDRESP, dbid);
        send_updat_beat(2, UPDAT_NCB_WR, dbid, RESP_I, 1'b0, 32'hFFFF_FFFF, 32'hCAFE_0000);
        send_updat_beat(2, UPDAT_NCB_WR, dbid, RESP_I, 1'b1, 32'hFFFF_FFFF, 32'hCAFE_1000);
        wait_dnrsp(2, DNRSP_COMP, dbid2);
        send_req_sz(2, REQ_WRITE_NOSNP_PTL, 48'h6040, 3'd5, 7'd18, 1'b0);
        wait_dnrsp(2, DNRSP_DBIDRESP, dbid);
        send_updat_beat(2, UPDAT_NCB_WR, dbid, RESP_I, 1'b0, 32'h0000_FFFF, 32'hFACE_0000);
        wait_dnrsp(2, DNRSP_COMP, dbid2);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_NOSNP, 48'h6000, 7'd19, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        check_cd_pat(32'hCAFE_0000, 32'hCAFE_1000, 2);
        send_req(1, REQ_READ_NOSNP, 48'h6040, 7'd20, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        for (i = 0; i < 4; i++)
            chk(cd_data[0][i] === 32'hFACE_0000 + 32'(i), "S13 Ptl words wrong");
        for (i = 4; i < 8; i++)
            chk(cd_data[0][i] === 32'h0, "S13 Ptl upper words should be zero");
        repeat (10) @(posedge clock);

        // ---- S14: CleanShared on dirty unique owner ----
        $display("TB S14: RS port0 0x7000, CleanShared port1 (PD writeback)");
        send_req(0, REQ_READ_SHARED, 48'h7000, 7'd21, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_CLEAN_SHARED, 48'h7000, 7'd22, 1'b0);
        fork
            answer_snoop_data(0, SNP_TO_CLEAN, RESP_UC_PD, 32'hC1EA_0000);
            wait_dnrsp(1, DNRSP_COMPCMO, dbid);
        join
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_NOSNP, 48'h7000, 7'd23, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        check_cd_pat(32'hC1EA_0000, 32'hC1EA_1000, 2);
        repeat (10) @(posedge clock);

        // ---- S15: CleanInvalid on dirty unique owner (other requester) ----
        $display("TB S15: CleanInvalid port2 line 0x7000 (PD writeback)");
        send_req(2, REQ_CLEAN_INVALID, 48'h7000, 7'd24, 1'b0);
        fork
            answer_snoop_data(0, SNP_TO_INVALID, RESP_I_PD, 32'hC1EA_0000);
            wait_dnrsp(2, DNRSP_COMPCMO, dbid);
        join
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_NOSNP, 48'h7000, 7'd25, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        check_cd_pat(32'hC1EA_0000, 32'hC1EA_1000, 2);
        repeat (10) @(posedge clock);

        // ---- S16: CleanInvalid by the owner itself (requester self-snoop) ----
        $display("TB S16: RS port0 0x8000, CleanInvalid port0 (self-snoop)");
        send_req(0, REQ_READ_SHARED, 48'h8000, 7'd26, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(0, REQ_CLEAN_INVALID, 48'h8000, 7'd27, 1'b0);
        fork
            answer_snoop_data(0, SNP_TO_INVALID, RESP_I_PD, 32'hD00D_0000);
            wait_dnrsp(0, DNRSP_COMPCMO, dbid);
        join
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_NOSNP, 48'h8000, 7'd28, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        check_cd_pat(32'hD00D_0000, 32'hD00D_1000, 2);
        repeat (10) @(posedge clock);

        // ---- S17: MakeInvalid by a sharer itself (dirty dropped) ----
        $display("TB S17: RS port1 0x8000, MakeInvalid port1 (self-snoop)");
        send_req(1, REQ_READ_SHARED, 48'h8000, 7'd29, 1'b1);
        wait_compdata(1, RESP_UC, 2, dbid);
        send_compack(1, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_MAKE_INVALID, 48'h8000, 7'd30, 1'b0);
        fork
            answer_snoop_resp(1, SNP_MAKE_INVALID, RESP_I);
            wait_dnrsp(1, DNRSP_COMPCMO, dbid);
        join
        repeat (10) @(posedge clock);

        // ---- S18: StashShared (silent) / StashUnique (CompStash) ----
        $display("TB S18: StashShared ECD=0 (silent), StashUnique ECD=1 (CompStash)");
        send_req_sz(0, REQ_STASH_SHARED, 48'h9000, 3'd6, 7'd31, 1'b0);
        expect_silent_port(0, 40);
        send_req_sz(0, REQ_STASH_UNIQUE, 48'h9000, 3'd6, 7'd32, 1'b1);
        wait_dnrsp(0, DNRSP_COMP_STASH, dbid);
        repeat (20) @(posedge clock);

        // ---- S19: directory-full victim back-invalidation ----
        $display("TB S19: fill set 0 (4 ways), then force a 5th allocation");
        send_req(0, REQ_READ_SHARED, 48'h10000, 7'd33, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        send_req(0, REQ_READ_SHARED, 48'h20000, 7'd34, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        send_req(0, REQ_READ_SHARED, 48'h30000, 7'd35, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        send_req(0, REQ_READ_SHARED, 48'h40000, 7'd36, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        // 5th line into the full set: back-invalidates the PLRU victim (0x10000).
        send_req(1, REQ_READ_SHARED, 48'h50000, 7'd37, 1'b1);
        fork
            answer_snoop_resp(0, SNP_TO_INVALID, RESP_I);
            wait_compdata(1, RESP_UC, 2, dbid);
        join
        send_compack(1, dbid);
        repeat (10) @(posedge clock);
        // Re-read the back-invalidated line: another victim flow (way1, 0x20000).
        send_req(2, REQ_READ_SHARED, 48'h10000, 7'd38, 1'b1);
        fork
            answer_snoop_resp(0, SNP_TO_INVALID, RESP_I);
            wait_compdata(2, RESP_UC, 2, dbid);
        join
        send_compack(2, dbid);
        repeat (20) @(posedge clock);

        // ---- S20: M5 — ReadNoSnp behind an in-flight WriteBackFull ----
        $display("TB S20: WriteBackFull port0 0xA000 with ReadNoSnp port1 behind it");
        send_evt(0, EVT_WRITE_BACK_FULL, 48'hA000, 7'd39);
        wait_dnrsp(0, DNRSP_COMPDBID, dbid);
        fork
            begin
                send_req(1, REQ_READ_NOSNP, 48'hA000, 7'd40, 1'b1);
                wait_compdata(1, RESP_UC, 2, dbid2);
                check_cd_pat(32'hABBA_0000, 32'hABBA_1000, 2);
            end
            begin
                // The read must not complete before the WB data has landed.
                for (i = 0; i < 20; i++) begin
                    @(posedge clock);
                    chk(!dat_valid(1), "M5 violated: CompData before CopyBackWrData");
                end
                send_updat_beat(0, UPDAT_CB_WR, dbid, RESP_I_PD, 1'b0, 32'hFFFF_FFFF, 32'hABBA_0000);
                send_updat_beat(0, UPDAT_CB_WR, dbid, RESP_I_PD, 1'b1, 32'hFFFF_FFFF, 32'hABBA_1000);
            end
        join
        repeat (40) @(posedge clock);

        // ---- S21: shared ReadShared (SC), then ReadUnique upgrade (ECD=0, Comp) ----
        $display("TB S21: RS port0+port1 0xB000 (SC), RU port1 ECD=0 (Comp only)");
        send_req(0, REQ_READ_SHARED, 48'hB000, 7'd41, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);          // sole tenant -> promoted UC
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_SHARED, 48'hB000, 7'd42, 1'b1);
        fork
            answer_snoop_resp(0, SNP_TO_SHARED, RESP_SC);
            wait_compdata(1, RESP_SC, 2, dbid);      // other holder -> SC, dir S{0,1}
        join
        send_compack(1, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_UNIQUE, 48'hB000, 7'd43, 1'b0);
        fork
            answer_snoop_resp(0, SNP_TO_INVALID, RESP_I);
            wait_dnrsp(1, DNRSP_COMP, dbid);         // upgrade: Comp, no CompData
        join
        send_compack(1, dbid);
        repeat (20) @(posedge clock);

        // ---- S22: EVT Evict shares a line with an in-flight RU (§7.3) ----
        $display("TB S22: RU port1 0xC000 in flight, Evict port0 shares the line");
        send_req(0, REQ_READ_SHARED, 48'hC000, 7'd44, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_UNIQUE, 48'hC000, 7'd45, 1'b1);
        // As soon as the RU's snoop appears at port0 the RU slot is mid-flow;
        // the Evict for the same line must still admit and complete (§7.3).
        // (0xC000 is in a maybe_all SF set, so every port gets snooped.)
        fork
            answer_snoops_i(120);
            begin
                timeout = 0;
                while (!snp_valid(0)) begin
                    @(posedge clock);
                    timeout++;
                    if (timeout > 2000) begin
                        $error("timeout waiting snoop port 0 (S22)");
                        errors++;
                        break;
                    end
                end
                send_evt(0, EVT_EVICT, 48'hC000, 7'd46);
                wait_dnrsp(0, DNRSP_COMP, dbid2);
            end
            wait_compdata(1, RESP_UC, 2, dbid);
        join
        send_compack(1, dbid);
        repeat (20) @(posedge clock);

        // ---- S23: WriteBackFull shares a line with an in-flight RU (M5) ----
        // Taurus parks the RU's snoop behind its own pending WB-EVT and
        // answers it only after the WB's CompDBIDResp; this reproduces that
        // interleaving exactly (the parked answer is withheld until then).
        $display("TB S23: RU port1 0xC400 in flight, WB port0 shares the line (M5)");
        send_req(0, REQ_READ_SHARED, 48'hC400, 7'd47, 1'b1);
        wait_compdata(0, RESP_UC, 2, dbid);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_UNIQUE, 48'hC400, 7'd48, 1'b1);
        wb_done = 1'b0;
        fork
            answer_snoops_i_x(150, 4'b0001);     // ports 1..3 answered; port0 parked
            begin
                timeout = 0;
                while (!snp_valid(0)) begin
                    @(posedge clock);
                    timeout++;
                    if (timeout > 2000) begin
                        $error("timeout waiting snoop port 0 (S23)");
                        errors++;
                        break;
                    end
                end
                snptxn = snp_txn(0);
                send_evt(0, EVT_WRITE_BACK_FULL, 48'hC400, 7'd49);
                wait_dnrsp(0, DNRSP_COMPDBID, dbid2);
                // unpark the snoop answer now that the WB is granted
                send_uprsp(0, UPRSP_SNPRESP, snptxn, RESP_I);
                // The WB's dirty data flows only to the WB slot; the RU's
                // read must wait for it and then see it (M5).
                send_updat_beat(0, UPDAT_CB_WR, dbid2, RESP_I_PD, 1'b0, 32'hFFFF_FFFF, 32'hAB00_0000);
                send_updat_beat(0, UPDAT_CB_WR, dbid2, RESP_I_PD, 1'b1, 32'hFFFF_FFFF, 32'hAB00_1000);
                wb_done = 1'b1;
            end
            begin
                // M5: the RU must not see CompData before the WB data landed.
                while (!wb_done) begin
                    @(posedge clock);
                    chk(!dat_valid(1), "M5 violated: RU CompData before WB data (S23)");
                end
            end
            wait_compdata(1, RESP_UC, 2, dbid);
        join
        check_cd_pat(32'hAB00_0000, 32'hAB00_1000, 2);
        send_compack(1, dbid);
        repeat (20) @(posedge clock);

        // ==============================================================
        // TagAlias scenarios (VIPT alias back-invalidation). Run only
        // when the feature is built in; at TAGALIAS_W=0 the field is
        // inert and these are skipped.
        // ==============================================================
        if (TAGALIAS_W > 0) begin

        // ---- S24: ReadShared alias mismatch on an S-held line ----
        // The requester's stale-alias copy must be invalidated by a targeted
        // SnpToInvalid before the grant; no other port may be snooped.
        $display("TB S24: RS p0/p1 0xE000 alias 0x11, Evict p1, RS p0 alias 0x22 (self SnpToInvalid)");
        send_req(0, REQ_READ_SHARED, 48'hE000, 7'd50, 1'b1, 8'h11);
        wait_compdata(0, RESP_UC, 2, dbid);          // promotion: U{0} @ 0x11
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_SHARED, 48'hE000, 7'd51, 1'b1, 8'h11);
        fork
            answer_snoop_resp(0, SNP_TO_SHARED, RESP_SC);
            wait_compdata(1, RESP_SC, 2, dbid);      // dir S{0,1}
        join
        send_compack(1, dbid);
        repeat (10) @(posedge clock);
        send_evt(1, EVT_EVICT, 48'hE000, 7'd52);     // dir S{0}, alias[0]=0x11 survives
        wait_dnrsp(1, DNRSP_COMP, dbid);
        repeat (10) @(posedge clock);
        send_req(0, REQ_READ_SHARED, 48'hE000, 7'd53, 1'b1, 8'h22);  // alias changed
        fork
            answer_snoop_resp(0, SNP_TO_INVALID, RESP_I);   // targeted self-invalidate
            begin
                expect_silent_snoop(1, 60);           // no snoop to any other port
                expect_silent_snoop(2, 60);
                expect_silent_snoop(3, 60);
            end
            wait_compdata(0, RESP_SC, 2, dbid);      // refill, dir S{0} @ 0x22
        join
        check_cd_zero(2);
        send_compack(0, dbid);
        repeat (20) @(posedge clock);

        // ---- S25: ReadUnique alias mismatch on a self-owned dirty line ----
        // The stale-alias dirty copy is invalidated and its data flows home
        // (I_PD), writes back to memory, AND serves the new CompData.
        $display("TB S25: RU p0 0xE400 alias 0x33 (dirty), RU p0 alias 0x44 (SnpToInvalid + I_PD)");
        send_req(0, REQ_READ_UNIQUE, 48'hE400, 7'd54, 1'b1, 8'h33);  // fresh line, U{0} @ 0x33
        wait_compdata(0, RESP_UC, 2, dbid);
        check_cd_zero(2);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        // (p0 has since written the line: it is dirty under alias 0x33)
        send_req(0, REQ_READ_UNIQUE, 48'hE400, 7'd55, 1'b1, 8'h44);  // alias changed
        fork
            answer_snoop_data(0, SNP_TO_INVALID, RESP_I_PD, 32'hD1A1_0000);  // old-alias dirty data home
            begin
                expect_silent_snoop(1, 80);
                expect_silent_snoop(2, 80);
                expect_silent_snoop(3, 80);
            end
            wait_compdata(0, RESP_UC, 2, dbid);      // CompData returns the dirty data
        join
        check_cd_pat(32'hD1A1_0000, 32'hD1A1_1000, 2);
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        // The line landed back with alias 0x44: a same-alias ReadShared must
        // NOT be snooped, and must see the written-back dirty data.
        send_req(0, REQ_READ_SHARED, 48'hE400, 7'd56, 1'b1, 8'h44);
        fork
            begin
                expect_silent_snoop(0, 60);
                expect_silent_snoop(1, 60);
                expect_silent_snoop(2, 60);
                expect_silent_snoop(3, 60);
            end
            wait_compdata(0, RESP_SC, 2, dbid);
        join
        check_cd_pat(32'hD1A1_0000, 32'hD1A1_1000, 2);
        send_compack(0, dbid);
        repeat (20) @(posedge clock);

        // ---- S26: same-alias S->U upgrade — guaranteed no self-snoop ----
        $display("TB S26: RS p0/p1 0xE800 alias 0x55, Evict p1, RU p0 alias 0x55 (no snoop at all)");
        send_req(0, REQ_READ_SHARED, 48'hE800, 7'd57, 1'b1, 8'h55);
        wait_compdata(0, RESP_UC, 2, dbid);          // U{0} @ 0x55
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_SHARED, 48'hE800, 7'd58, 1'b1, 8'h55);
        fork
            answer_snoop_resp(0, SNP_TO_SHARED, RESP_SC);
            wait_compdata(1, RESP_SC, 2, dbid);      // dir S{0,1}
        join
        send_compack(1, dbid);
        repeat (10) @(posedge clock);
        send_evt(1, EVT_EVICT, 48'hE800, 7'd59);     // dir S{0}, alias[0]=0x55
        wait_dnrsp(1, DNRSP_COMP, dbid);
        repeat (10) @(posedge clock);
        send_req(0, REQ_READ_UNIQUE, 48'hE800, 7'd60, 1'b0, 8'h55);  // same alias
        fork
            begin
                expect_silent_snoop(0, 60);           // requester NOT snooped...
                expect_silent_snoop(1, 60);           // ... and neither is anyone else
                expect_silent_snoop(2, 60);
                expect_silent_snoop(3, 60);
            end
            wait_dnrsp(0, DNRSP_COMP, dbid);         // normal U grant: Comp (ECD=0)
        join
        send_compack(0, dbid);
        repeat (20) @(posedge clock);

        // ---- S27: alias-changed S->U — exactly one targeted SnpToInvalid ----
        // The back-invalidated requester held only a clean S copy, so the
        // grant must return data (a snooped requester is always owed data):
        // CompData UC carrying the line's memory image, not a dataless Comp.
        $display("TB S27: RS p0/p1 0xEC00 alias 0x66, Evict p1, RU p0 alias 0x77 (one self SnpToInvalid + CompData)");
        send_req(0, REQ_READ_SHARED, 48'hEC00, 7'd61, 1'b1, 8'h66);
        wait_compdata(0, RESP_UC, 2, dbid);          // U{0} @ 0x66
        send_compack(0, dbid);
        repeat (10) @(posedge clock);
        send_req(1, REQ_READ_SHARED, 48'hEC00, 7'd62, 1'b1, 8'h66);
        fork
            answer_snoop_resp(0, SNP_TO_SHARED, RESP_SC);
            wait_compdata(1, RESP_SC, 2, dbid);      // dir S{0,1}
        join
        send_compack(1, dbid);
        repeat (10) @(posedge clock);
        send_evt(1, EVT_EVICT, 48'hEC00, 7'd63);     // dir S{0}, alias[0]=0x66
        wait_dnrsp(1, DNRSP_COMP, dbid);
        repeat (10) @(posedge clock);
        send_req(0, REQ_READ_UNIQUE, 48'hEC00, 7'd64, 1'b0, 8'h77);  // alias changed
        fork
            begin
                answer_snoop_resp(0, SNP_TO_INVALID, RESP_I);   // the one targeted self-snoop
                expect_silent_snoop(0, 40);                     // ... and no second one
            end
            begin
                expect_silent_snoop(1, 80);
                expect_silent_snoop(2, 80);
                expect_silent_snoop(3, 80);
            end
            wait_compdata(0, RESP_UC, 2, dbid);      // U grant w/ data after the self-invalidate
        join
        check_cd_zero(2);                            // clean copy dropped: memory image (zeros)
        send_compack(0, dbid);
        repeat (20) @(posedge clock);

        end

        if (errors == 0)
            $display("VENUS_TB PASSED");
        else
            $display("VENUS_TB FAILED with %0d error(s)", errors);
        $finish;
    end

    initial begin
        #4_000_000;
        $error("global timeout");
        $finish;
    end

endmodule

`default_nettype wire
