`default_nettype none

// V3 simulation top (external memory variant): synthesizable Venus with its
// AXI master ports exposed on the boundary ('axi_m*'), to be served by the
// C++ AXIMemory through V3AXISlaveInterface. All 8 CCHI Type-1 port sets stay
// on the boundary so V3CCHIInterface can bind them (ports >= NUM_T1 are tied
// off inside Venus and driven idle by the harness). The AXI buses are
// additionally exposed as all-output monitor taps ('mon_axi*') so
// V3AXIMonitorInterface can observe the traffic passively.

module venus_v3_top_extmem #(
    parameter int NUM_T1            = 4,
    parameter int NUM_AXI           = 1,
    parameter int PARALLELISM       = 8,
    parameter int NODE_ID           = 16,
    parameter int INFLIGHT_SNP      = 4,
    parameter int Q_EVT             = 2,
    parameter int Q_REQ             = 2,
    parameter int Q_RSP             = 8,
    parameter int Q_DAT             = 16,
    parameter int DIR_SETS          = 1024,
    parameter int DIR_WAYS          = 4,
    parameter int SF_ENABLE         = 1,
    parameter int SF_ALLOW_OVER     = 1,
    parameter int SF_BROADCAST      = 0,
    parameter int SF_SETS           = 256,
    parameter int SF_WAYS           = 4,
    parameter int TAGALIAS_W        = 0,
    parameter int AGE_MATRIX        = 0,
    parameter int STORAGE_DIR       = 0,
    parameter int STORAGE_SF        = 0,
    parameter int SRAM_LAT          = 1,
    parameter int SRAM_INTERVAL     = 1,
    parameter int SRAM_PERIOD       = 1,
    parameter int SRAM_SLOT         = 0,
    parameter int AXI_ADDR_W        = 48,
    parameter int AXI_DATA_W        = 256,
    parameter int AXI_ID_W          = 4
) (
    input  wire         clock,
    input  wire         reset,

    `define VENUS_V3_T1_PORT(P) \
    input  wire         cchi_t1p``P``_rxevt_valid, \
    input  wire [6:0]   cchi_t1p``P``_rxevt_bits_TxnID, \
    input  wire [4:0]   cchi_t1p``P``_rxevt_bits_SrcID, \
    input  wire [4:0]   cchi_t1p``P``_rxevt_bits_TgtID, \
    input  wire         cchi_t1p``P``_rxevt_bits_Opcode, \
    input  wire [47:0]  cchi_t1p``P``_rxevt_bits_Addr, \
    input  wire         cchi_t1p``P``_rxevt_bits_NS, \
    input  wire         cchi_t1p``P``_rxevt_bits_MemAttr, \
    input  wire         cchi_t1p``P``_rxevt_bits_WayValid, \
    input  wire [3:0]   cchi_t1p``P``_rxevt_bits_Way, \
    input  wire         cchi_t1p``P``_rxevt_bits_TraceTag, \
    output logic        cchi_t1p``P``_rxevt_ready, \
    input  wire         cchi_t1p``P``_rxreq_valid, \
    input  wire [6:0]   cchi_t1p``P``_rxreq_bits_TxnID, \
    input  wire [4:0]   cchi_t1p``P``_rxreq_bits_SrcID, \
    input  wire [4:0]   cchi_t1p``P``_rxreq_bits_TgtID, \
    input  wire [5:0]   cchi_t1p``P``_rxreq_bits_Opcode, \
    input  wire [2:0]   cchi_t1p``P``_rxreq_bits_Size, \
    input  wire [47:0]  cchi_t1p``P``_rxreq_bits_Addr, \
    input  wire [7:0]   cchi_t1p``P``_rxreq_bits_TagAlias, \
    input  wire         cchi_t1p``P``_rxreq_bits_NS, \
    input  wire [1:0]   cchi_t1p``P``_rxreq_bits_Order, \
    input  wire [3:0]   cchi_t1p``P``_rxreq_bits_MemAttr, \
    input  wire         cchi_t1p``P``_rxreq_bits_Excl, \
    input  wire         cchi_t1p``P``_rxreq_bits_ExpCompData, \
    input  wire         cchi_t1p``P``_rxreq_bits_TraceTag, \
    input  wire         cchi_t1p``P``_rxreq_bits_WayValid, \
    input  wire [3:0]   cchi_t1p``P``_rxreq_bits_Way, \
    output logic        cchi_t1p``P``_rxreq_ready, \
    input  wire         cchi_t1p``P``_txsnp_ready, \
    output logic        cchi_t1p``P``_txsnp_valid, \
    output logic [6:0]  cchi_t1p``P``_txsnp_bits_TxnID, \
    output logic [4:0]  cchi_t1p``P``_txsnp_bits_SrcID, \
    output logic [4:0]  cchi_t1p``P``_txsnp_bits_TgtID, \
    output logic [1:0]  cchi_t1p``P``_txsnp_bits_Opcode, \
    output logic [44:0] cchi_t1p``P``_txsnp_bits_Addr, \
    output logic        cchi_t1p``P``_txsnp_bits_NS, \
    output logic        cchi_t1p``P``_txsnp_bits_TraceTag, \
    input  wire         cchi_t1p``P``_txrsp_ready, \
    output logic        cchi_t1p``P``_txrsp_valid, \
    output logic [6:0]  cchi_t1p``P``_txrsp_bits_TxnID, \
    output logic [4:0]  cchi_t1p``P``_txrsp_bits_SrcID, \
    output logic [4:0]  cchi_t1p``P``_txrsp_bits_TgtID, \
    output logic [6:0]  cchi_t1p``P``_txrsp_bits_DBID, \
    output logic [2:0]  cchi_t1p``P``_txrsp_bits_Opcode, \
    output logic [1:0]  cchi_t1p``P``_txrsp_bits_RespErr, \
    output logic [2:0]  cchi_t1p``P``_txrsp_bits_Resp, \
    output logic [2:0]  cchi_t1p``P``_txrsp_bits_CBusy, \
    output logic        cchi_t1p``P``_txrsp_bits_WayValid, \
    output logic [3:0]  cchi_t1p``P``_txrsp_bits_Way, \
    output logic        cchi_t1p``P``_txrsp_bits_TraceTag, \
    input  wire         cchi_t1p``P``_rxrsp_valid, \
    input  wire [6:0]   cchi_t1p``P``_rxrsp_bits_TxnID, \
    input  wire [4:0]   cchi_t1p``P``_rxrsp_bits_SrcID, \
    input  wire [4:0]   cchi_t1p``P``_rxrsp_bits_TgtID, \
    input  wire         cchi_t1p``P``_rxrsp_bits_Opcode, \
    input  wire [1:0]   cchi_t1p``P``_rxrsp_bits_RespErr, \
    input  wire [2:0]   cchi_t1p``P``_rxrsp_bits_Resp, \
    input  wire         cchi_t1p``P``_rxrsp_bits_TraceTag, \
    output logic        cchi_t1p``P``_rxrsp_ready, \
    input  wire         cchi_t1p``P``_txdat_ready, \
    output logic        cchi_t1p``P``_txdat_valid, \
    output logic [6:0]  cchi_t1p``P``_txdat_bits_TxnID, \
    output logic [4:0]  cchi_t1p``P``_txdat_bits_SrcID, \
    output logic [4:0]  cchi_t1p``P``_txdat_bits_TgtID, \
    output logic [6:0]  cchi_t1p``P``_txdat_bits_DBID, \
    output logic        cchi_t1p``P``_txdat_bits_Opcode, \
    output logic [1:0]  cchi_t1p``P``_txdat_bits_RespErr, \
    output logic [2:0]  cchi_t1p``P``_txdat_bits_Resp, \
    output logic [4:0]  cchi_t1p``P``_txdat_bits_DataSource, \
    output logic [2:0]  cchi_t1p``P``_txdat_bits_CBusy, \
    output logic        cchi_t1p``P``_txdat_bits_DataID, \
    output logic [31:0] cchi_t1p``P``_txdat_bits_Data [0:7], \
    output logic        cchi_t1p``P``_txdat_bits_WayValid, \
    output logic [3:0]  cchi_t1p``P``_txdat_bits_Way, \
    output logic        cchi_t1p``P``_txdat_bits_TraceTag, \
    input  wire         cchi_t1p``P``_rxdat_valid, \
    input  wire [6:0]   cchi_t1p``P``_rxdat_bits_TxnID, \
    input  wire [4:0]   cchi_t1p``P``_rxdat_bits_SrcID, \
    input  wire [4:0]   cchi_t1p``P``_rxdat_bits_TgtID, \
    input  wire [1:0]   cchi_t1p``P``_rxdat_bits_Opcode, \
    input  wire [1:0]   cchi_t1p``P``_rxdat_bits_RespErr, \
    input  wire [2:0]   cchi_t1p``P``_rxdat_bits_Resp, \
    input  wire         cchi_t1p``P``_rxdat_bits_DataID, \
    input  wire [31:0]  cchi_t1p``P``_rxdat_bits_Data [0:7], \
    input  wire [31:0]  cchi_t1p``P``_rxdat_bits_BE, \
    input  wire         cchi_t1p``P``_rxdat_bits_TraceTag, \
    output logic        cchi_t1p``P``_rxdat_ready

    // AXI master ports, served externally by the C++ memory model
    `define VENUS_V3_AXI_PORT(P) \
    output logic                        axi_m``P``_awvalid, \
    input  wire                         axi_m``P``_awready, \
    output logic [AXI_ADDR_W-1:0]       axi_m``P``_awaddr, \
    output logic [AXI_ID_W-1:0]         axi_m``P``_awid, \
    output logic [7:0]                  axi_m``P``_awlen, \
    output logic [2:0]                  axi_m``P``_awsize, \
    output logic [1:0]                  axi_m``P``_awburst, \
    output logic                        axi_m``P``_wvalid, \
    input  wire                         axi_m``P``_wready, \
    output logic [AXI_DATA_W-1:0]       axi_m``P``_wdata, \
    output logic [AXI_DATA_W/8-1:0]     axi_m``P``_wstrb, \
    output logic                        axi_m``P``_wlast, \
    input  wire                         axi_m``P``_bvalid, \
    output logic                        axi_m``P``_bready, \
    input  wire [AXI_ID_W-1:0]          axi_m``P``_bid, \
    input  wire [1:0]                   axi_m``P``_bresp, \
    output logic                        axi_m``P``_arvalid, \
    input  wire                         axi_m``P``_arready, \
    output logic [AXI_ADDR_W-1:0]       axi_m``P``_araddr, \
    output logic [AXI_ID_W-1:0]         axi_m``P``_arid, \
    output logic [7:0]                  axi_m``P``_arlen, \
    output logic [2:0]                  axi_m``P``_arsize, \
    output logic [1:0]                  axi_m``P``_arburst, \
    input  wire                         axi_m``P``_rvalid, \
    output logic                        axi_m``P``_rready, \
    input  wire [AXI_DATA_W-1:0]        axi_m``P``_rdata, \
    input  wire [AXI_ID_W-1:0]          axi_m``P``_rid, \
    input  wire [1:0]                   axi_m``P``_rresp, \
    input  wire                         axi_m``P``_rlast

    // Passive all-output monitor taps of the AXI buses
    `define VENUS_V3_AXI_MON(P) \
    output logic                        mon_axi``P``_awvalid, \
    output logic                        mon_axi``P``_awready, \
    output logic [AXI_ADDR_W-1:0]       mon_axi``P``_awaddr, \
    output logic [AXI_ID_W-1:0]         mon_axi``P``_awid, \
    output logic [7:0]                  mon_axi``P``_awlen, \
    output logic [2:0]                  mon_axi``P``_awsize, \
    output logic [1:0]                  mon_axi``P``_awburst, \
    output logic                        mon_axi``P``_wvalid, \
    output logic                        mon_axi``P``_wready, \
    output logic [AXI_DATA_W-1:0]       mon_axi``P``_wdata, \
    output logic [AXI_DATA_W/8-1:0]     mon_axi``P``_wstrb, \
    output logic                        mon_axi``P``_wlast, \
    output logic                        mon_axi``P``_bvalid, \
    output logic                        mon_axi``P``_bready, \
    output logic [AXI_ID_W-1:0]         mon_axi``P``_bid, \
    output logic [1:0]                  mon_axi``P``_bresp, \
    output logic                        mon_axi``P``_arvalid, \
    output logic                        mon_axi``P``_arready, \
    output logic [AXI_ADDR_W-1:0]       mon_axi``P``_araddr, \
    output logic [AXI_ID_W-1:0]         mon_axi``P``_arid, \
    output logic [7:0]                  mon_axi``P``_arlen, \
    output logic [2:0]                  mon_axi``P``_arsize, \
    output logic [1:0]                  mon_axi``P``_arburst, \
    output logic                        mon_axi``P``_rvalid, \
    output logic                        mon_axi``P``_rready, \
    output logic [AXI_DATA_W-1:0]       mon_axi``P``_rdata, \
    output logic [AXI_ID_W-1:0]         mon_axi``P``_rid, \
    output logic [1:0]                  mon_axi``P``_rresp, \
    output logic                        mon_axi``P``_rlast

    `VENUS_V3_T1_PORT(0),
    `VENUS_V3_T1_PORT(1),
    `VENUS_V3_T1_PORT(2),
    `VENUS_V3_T1_PORT(3),
    `VENUS_V3_T1_PORT(4),
    `VENUS_V3_T1_PORT(5),
    `VENUS_V3_T1_PORT(6),
    `VENUS_V3_T1_PORT(7),

    `VENUS_V3_AXI_PORT(0),
    `VENUS_V3_AXI_PORT(1),
    `VENUS_V3_AXI_PORT(2),
    `VENUS_V3_AXI_PORT(3),

    `VENUS_V3_AXI_MON(0),
    `VENUS_V3_AXI_MON(1),
    `VENUS_V3_AXI_MON(2),
    `VENUS_V3_AXI_MON(3)
);

    `undef VENUS_V3_T1_PORT
    `undef VENUS_V3_AXI_PORT
    `undef VENUS_V3_AXI_MON

    `define VENUS_V3_T1_CONN(P) \
        .cchi_t1p``P``_rxevt_valid(cchi_t1p``P``_rxevt_valid), \
        .cchi_t1p``P``_rxevt_bits_TxnID(cchi_t1p``P``_rxevt_bits_TxnID), \
        .cchi_t1p``P``_rxevt_bits_SrcID(cchi_t1p``P``_rxevt_bits_SrcID), \
        .cchi_t1p``P``_rxevt_bits_TgtID(cchi_t1p``P``_rxevt_bits_TgtID), \
        .cchi_t1p``P``_rxevt_bits_Opcode(cchi_t1p``P``_rxevt_bits_Opcode), \
        .cchi_t1p``P``_rxevt_bits_Addr(cchi_t1p``P``_rxevt_bits_Addr), \
        .cchi_t1p``P``_rxevt_bits_NS(cchi_t1p``P``_rxevt_bits_NS), \
        .cchi_t1p``P``_rxevt_bits_MemAttr(cchi_t1p``P``_rxevt_bits_MemAttr), \
        .cchi_t1p``P``_rxevt_bits_WayValid(cchi_t1p``P``_rxevt_bits_WayValid), \
        .cchi_t1p``P``_rxevt_bits_Way(cchi_t1p``P``_rxevt_bits_Way), \
        .cchi_t1p``P``_rxevt_bits_TraceTag(cchi_t1p``P``_rxevt_bits_TraceTag), \
        .cchi_t1p``P``_rxevt_ready(cchi_t1p``P``_rxevt_ready), \
        .cchi_t1p``P``_rxreq_valid(cchi_t1p``P``_rxreq_valid), \
        .cchi_t1p``P``_rxreq_bits_TxnID(cchi_t1p``P``_rxreq_bits_TxnID), \
        .cchi_t1p``P``_rxreq_bits_SrcID(cchi_t1p``P``_rxreq_bits_SrcID), \
        .cchi_t1p``P``_rxreq_bits_TgtID(cchi_t1p``P``_rxreq_bits_TgtID), \
        .cchi_t1p``P``_rxreq_bits_Opcode(cchi_t1p``P``_rxreq_bits_Opcode), \
        .cchi_t1p``P``_rxreq_bits_Size(cchi_t1p``P``_rxreq_bits_Size), \
        .cchi_t1p``P``_rxreq_bits_Addr(cchi_t1p``P``_rxreq_bits_Addr), \
        .cchi_t1p``P``_rxreq_bits_TagAlias(cchi_t1p``P``_rxreq_bits_TagAlias), \
        .cchi_t1p``P``_rxreq_bits_NS(cchi_t1p``P``_rxreq_bits_NS), \
        .cchi_t1p``P``_rxreq_bits_Order(cchi_t1p``P``_rxreq_bits_Order), \
        .cchi_t1p``P``_rxreq_bits_MemAttr(cchi_t1p``P``_rxreq_bits_MemAttr), \
        .cchi_t1p``P``_rxreq_bits_Excl(cchi_t1p``P``_rxreq_bits_Excl), \
        .cchi_t1p``P``_rxreq_bits_ExpCompData(cchi_t1p``P``_rxreq_bits_ExpCompData), \
        .cchi_t1p``P``_rxreq_bits_TraceTag(cchi_t1p``P``_rxreq_bits_TraceTag), \
        .cchi_t1p``P``_rxreq_bits_WayValid(cchi_t1p``P``_rxreq_bits_WayValid), \
        .cchi_t1p``P``_rxreq_bits_Way(cchi_t1p``P``_rxreq_bits_Way), \
        .cchi_t1p``P``_rxreq_ready(cchi_t1p``P``_rxreq_ready), \
        .cchi_t1p``P``_txsnp_ready(cchi_t1p``P``_txsnp_ready), \
        .cchi_t1p``P``_txsnp_valid(cchi_t1p``P``_txsnp_valid), \
        .cchi_t1p``P``_txsnp_bits_TxnID(cchi_t1p``P``_txsnp_bits_TxnID), \
        .cchi_t1p``P``_txsnp_bits_SrcID(cchi_t1p``P``_txsnp_bits_SrcID), \
        .cchi_t1p``P``_txsnp_bits_TgtID(cchi_t1p``P``_txsnp_bits_TgtID), \
        .cchi_t1p``P``_txsnp_bits_Opcode(cchi_t1p``P``_txsnp_bits_Opcode), \
        .cchi_t1p``P``_txsnp_bits_Addr(cchi_t1p``P``_txsnp_bits_Addr), \
        .cchi_t1p``P``_txsnp_bits_NS(cchi_t1p``P``_txsnp_bits_NS), \
        .cchi_t1p``P``_txsnp_bits_TraceTag(cchi_t1p``P``_txsnp_bits_TraceTag), \
        .cchi_t1p``P``_txrsp_ready(cchi_t1p``P``_txrsp_ready), \
        .cchi_t1p``P``_txrsp_valid(cchi_t1p``P``_txrsp_valid), \
        .cchi_t1p``P``_txrsp_bits_TxnID(cchi_t1p``P``_txrsp_bits_TxnID), \
        .cchi_t1p``P``_txrsp_bits_SrcID(cchi_t1p``P``_txrsp_bits_SrcID), \
        .cchi_t1p``P``_txrsp_bits_TgtID(cchi_t1p``P``_txrsp_bits_TgtID), \
        .cchi_t1p``P``_txrsp_bits_DBID(cchi_t1p``P``_txrsp_bits_DBID), \
        .cchi_t1p``P``_txrsp_bits_Opcode(cchi_t1p``P``_txrsp_bits_Opcode), \
        .cchi_t1p``P``_txrsp_bits_RespErr(cchi_t1p``P``_txrsp_bits_RespErr), \
        .cchi_t1p``P``_txrsp_bits_Resp(cchi_t1p``P``_txrsp_bits_Resp), \
        .cchi_t1p``P``_txrsp_bits_CBusy(cchi_t1p``P``_txrsp_bits_CBusy), \
        .cchi_t1p``P``_txrsp_bits_WayValid(cchi_t1p``P``_txrsp_bits_WayValid), \
        .cchi_t1p``P``_txrsp_bits_Way(cchi_t1p``P``_txrsp_bits_Way), \
        .cchi_t1p``P``_txrsp_bits_TraceTag(cchi_t1p``P``_txrsp_bits_TraceTag), \
        .cchi_t1p``P``_rxrsp_valid(cchi_t1p``P``_rxrsp_valid), \
        .cchi_t1p``P``_rxrsp_bits_TxnID(cchi_t1p``P``_rxrsp_bits_TxnID), \
        .cchi_t1p``P``_rxrsp_bits_SrcID(cchi_t1p``P``_rxrsp_bits_SrcID), \
        .cchi_t1p``P``_rxrsp_bits_TgtID(cchi_t1p``P``_rxrsp_bits_TgtID), \
        .cchi_t1p``P``_rxrsp_bits_Opcode(cchi_t1p``P``_rxrsp_bits_Opcode), \
        .cchi_t1p``P``_rxrsp_bits_RespErr(cchi_t1p``P``_rxrsp_bits_RespErr), \
        .cchi_t1p``P``_rxrsp_bits_Resp(cchi_t1p``P``_rxrsp_bits_Resp), \
        .cchi_t1p``P``_rxrsp_bits_TraceTag(cchi_t1p``P``_rxrsp_bits_TraceTag), \
        .cchi_t1p``P``_rxrsp_ready(cchi_t1p``P``_rxrsp_ready), \
        .cchi_t1p``P``_txdat_ready(cchi_t1p``P``_txdat_ready), \
        .cchi_t1p``P``_txdat_valid(cchi_t1p``P``_txdat_valid), \
        .cchi_t1p``P``_txdat_bits_TxnID(cchi_t1p``P``_txdat_bits_TxnID), \
        .cchi_t1p``P``_txdat_bits_SrcID(cchi_t1p``P``_txdat_bits_SrcID), \
        .cchi_t1p``P``_txdat_bits_TgtID(cchi_t1p``P``_txdat_bits_TgtID), \
        .cchi_t1p``P``_txdat_bits_DBID(cchi_t1p``P``_txdat_bits_DBID), \
        .cchi_t1p``P``_txdat_bits_Opcode(cchi_t1p``P``_txdat_bits_Opcode), \
        .cchi_t1p``P``_txdat_bits_RespErr(cchi_t1p``P``_txdat_bits_RespErr), \
        .cchi_t1p``P``_txdat_bits_Resp(cchi_t1p``P``_txdat_bits_Resp), \
        .cchi_t1p``P``_txdat_bits_DataSource(cchi_t1p``P``_txdat_bits_DataSource), \
        .cchi_t1p``P``_txdat_bits_CBusy(cchi_t1p``P``_txdat_bits_CBusy), \
        .cchi_t1p``P``_txdat_bits_DataID(cchi_t1p``P``_txdat_bits_DataID), \
        .cchi_t1p``P``_txdat_bits_Data(cchi_t1p``P``_txdat_bits_Data), \
        .cchi_t1p``P``_txdat_bits_WayValid(cchi_t1p``P``_txdat_bits_WayValid), \
        .cchi_t1p``P``_txdat_bits_Way(cchi_t1p``P``_txdat_bits_Way), \
        .cchi_t1p``P``_txdat_bits_TraceTag(cchi_t1p``P``_txdat_bits_TraceTag), \
        .cchi_t1p``P``_rxdat_valid(cchi_t1p``P``_rxdat_valid), \
        .cchi_t1p``P``_rxdat_bits_TxnID(cchi_t1p``P``_rxdat_bits_TxnID), \
        .cchi_t1p``P``_rxdat_bits_SrcID(cchi_t1p``P``_rxdat_bits_SrcID), \
        .cchi_t1p``P``_rxdat_bits_TgtID(cchi_t1p``P``_rxdat_bits_TgtID), \
        .cchi_t1p``P``_rxdat_bits_Opcode(cchi_t1p``P``_rxdat_bits_Opcode), \
        .cchi_t1p``P``_rxdat_bits_RespErr(cchi_t1p``P``_rxdat_bits_RespErr), \
        .cchi_t1p``P``_rxdat_bits_Resp(cchi_t1p``P``_rxdat_bits_Resp), \
        .cchi_t1p``P``_rxdat_bits_DataID(cchi_t1p``P``_rxdat_bits_DataID), \
        .cchi_t1p``P``_rxdat_bits_Data(cchi_t1p``P``_rxdat_bits_Data), \
        .cchi_t1p``P``_rxdat_bits_BE(cchi_t1p``P``_rxdat_bits_BE), \
        .cchi_t1p``P``_rxdat_bits_TraceTag(cchi_t1p``P``_rxdat_bits_TraceTag), \
        .cchi_t1p``P``_rxdat_ready(cchi_t1p``P``_rxdat_ready)

    `define VENUS_V3_AXI_CONN(P) \
        .axi_m``P``_awvalid(axi_m``P``_awvalid), .axi_m``P``_awready(axi_m``P``_awready), .axi_m``P``_awaddr(axi_m``P``_awaddr), \
        .axi_m``P``_awid(axi_m``P``_awid), .axi_m``P``_awlen(axi_m``P``_awlen), .axi_m``P``_awsize(axi_m``P``_awsize), .axi_m``P``_awburst(axi_m``P``_awburst), \
        .axi_m``P``_wvalid(axi_m``P``_wvalid), .axi_m``P``_wready(axi_m``P``_wready), .axi_m``P``_wdata(axi_m``P``_wdata), \
        .axi_m``P``_wstrb(axi_m``P``_wstrb), .axi_m``P``_wlast(axi_m``P``_wlast), \
        .axi_m``P``_bvalid(axi_m``P``_bvalid), .axi_m``P``_bready(axi_m``P``_bready), .axi_m``P``_bid(axi_m``P``_bid), .axi_m``P``_bresp(axi_m``P``_bresp), \
        .axi_m``P``_arvalid(axi_m``P``_arvalid), .axi_m``P``_arready(axi_m``P``_arready), .axi_m``P``_araddr(axi_m``P``_araddr), \
        .axi_m``P``_arid(axi_m``P``_arid), .axi_m``P``_arlen(axi_m``P``_arlen), .axi_m``P``_arsize(axi_m``P``_arsize), .axi_m``P``_arburst(axi_m``P``_arburst), \
        .axi_m``P``_rvalid(axi_m``P``_rvalid), .axi_m``P``_rready(axi_m``P``_rready), .axi_m``P``_rdata(axi_m``P``_rdata), \
        .axi_m``P``_rid(axi_m``P``_rid), .axi_m``P``_rresp(axi_m``P``_rresp), .axi_m``P``_rlast(axi_m``P``_rlast)

    venus #(
        .NUM_T1(NUM_T1),
        .NUM_AXI(NUM_AXI),
        .PARALLELISM(PARALLELISM),
        .NODE_ID(NODE_ID),
        .INFLIGHT_SNP(INFLIGHT_SNP),
        .Q_EVT(Q_EVT),
        .Q_REQ(Q_REQ),
        .Q_RSP(Q_RSP),
        .Q_DAT(Q_DAT),
        .DIR_SETS(DIR_SETS),
        .DIR_WAYS(DIR_WAYS),
        .SF_ENABLE(SF_ENABLE),
        .SF_ALLOW_OVER(SF_ALLOW_OVER),
        .SF_BROADCAST(SF_BROADCAST),
        .SF_SETS(SF_SETS),
        .SF_WAYS(SF_WAYS),
        .TAGALIAS_W(TAGALIAS_W),
        .AGE_MATRIX(AGE_MATRIX),
        .STORAGE_DIR(STORAGE_DIR),
        .STORAGE_SF(STORAGE_SF),
        .SRAM_LAT(SRAM_LAT),
        .SRAM_INTERVAL(SRAM_INTERVAL),
        .SRAM_PERIOD(SRAM_PERIOD),
        .SRAM_SLOT(SRAM_SLOT),
        .AXI_ADDR_W(AXI_ADDR_W),
        .AXI_DATA_W(AXI_DATA_W),
        .AXI_ID_W(AXI_ID_W)
    ) u_venus (
        .clock(clock),
        .reset(reset),
        `VENUS_V3_T1_CONN(0),
        `VENUS_V3_T1_CONN(1),
        `VENUS_V3_T1_CONN(2),
        `VENUS_V3_T1_CONN(3),
        `VENUS_V3_T1_CONN(4),
        `VENUS_V3_T1_CONN(5),
        `VENUS_V3_T1_CONN(6),
        `VENUS_V3_T1_CONN(7),
        `VENUS_V3_AXI_CONN(0),
        `VENUS_V3_AXI_CONN(1),
        `VENUS_V3_AXI_CONN(2),
        `VENUS_V3_AXI_CONN(3)
    );

    `undef VENUS_V3_T1_CONN
    `undef VENUS_V3_AXI_CONN

    // Monitor tap assigns (passive observation of the AXI buses)
    `define VENUS_V3_AXI_MON_ASSIGN(P) \
        assign mon_axi``P``_awvalid = axi_m``P``_awvalid; \
        assign mon_axi``P``_awready = axi_m``P``_awready; \
        assign mon_axi``P``_awaddr  = axi_m``P``_awaddr; \
        assign mon_axi``P``_awid    = axi_m``P``_awid; \
        assign mon_axi``P``_awlen   = axi_m``P``_awlen; \
        assign mon_axi``P``_awsize  = axi_m``P``_awsize; \
        assign mon_axi``P``_awburst = axi_m``P``_awburst; \
        assign mon_axi``P``_wvalid  = axi_m``P``_wvalid; \
        assign mon_axi``P``_wready  = axi_m``P``_wready; \
        assign mon_axi``P``_wdata   = axi_m``P``_wdata; \
        assign mon_axi``P``_wstrb   = axi_m``P``_wstrb; \
        assign mon_axi``P``_wlast   = axi_m``P``_wlast; \
        assign mon_axi``P``_bvalid  = axi_m``P``_bvalid; \
        assign mon_axi``P``_bready  = axi_m``P``_bready; \
        assign mon_axi``P``_bid     = axi_m``P``_bid; \
        assign mon_axi``P``_bresp   = axi_m``P``_bresp; \
        assign mon_axi``P``_arvalid = axi_m``P``_arvalid; \
        assign mon_axi``P``_arready = axi_m``P``_arready; \
        assign mon_axi``P``_araddr  = axi_m``P``_araddr; \
        assign mon_axi``P``_arid    = axi_m``P``_arid; \
        assign mon_axi``P``_arlen   = axi_m``P``_arlen; \
        assign mon_axi``P``_arsize  = axi_m``P``_arsize; \
        assign mon_axi``P``_arburst = axi_m``P``_arburst; \
        assign mon_axi``P``_rvalid  = axi_m``P``_rvalid; \
        assign mon_axi``P``_rready  = axi_m``P``_rready; \
        assign mon_axi``P``_rdata   = axi_m``P``_rdata; \
        assign mon_axi``P``_rid     = axi_m``P``_rid; \
        assign mon_axi``P``_rresp   = axi_m``P``_rresp; \
        assign mon_axi``P``_rlast   = axi_m``P``_rlast;

    `VENUS_V3_AXI_MON_ASSIGN(0)
    `VENUS_V3_AXI_MON_ASSIGN(1)
    `VENUS_V3_AXI_MON_ASSIGN(2)
    `VENUS_V3_AXI_MON_ASSIGN(3)

    `undef VENUS_V3_AXI_MON_ASSIGN

endmodule

`default_nettype wire
