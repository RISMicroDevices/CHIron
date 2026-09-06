`default_nettype none

// ============================================================================
// venus — synthesizable CCHI Type-1 coherency home (top level).
//
// Default elaboration (NUM_T1=4, NUM_AXI=1, PARALLELISM=8, STORAGE=REG) is the
// Cohestra V3 DUT. The pin contract of this module is FROZEN (docs/PROTOCOL.md
// §2): the C++ V3 interfaces and tb/* bind to these exact names, widths,
// and the unpacked Data[0:7] shape. The pin list carries T1_MAX=8 Type-1 port
// sets; NUM_T1 (1..8) selects how many are live, ports >= NUM_T1 are tied off
// (RX ready = 0, TX valid = 0) and must never see traffic (asserted in sim).
// Every RX channel ready is occupancy-only (never a combinational function of
// same-cycle valid/payload) as required by the V3 tick model.
//
// Pin-contract extension: rxreq_bits_TagAlias[7:0] (VIPT alias, spec field
// order between Addr and NS) is present on every port set regardless of the
// TAGALIAS_W parameter; with TAGALIAS_W=0 the field is inert and the pins
// must be driven 0 (asserted in sim below).
//
// Structure (docs/ARCHITECTURE.md):
//   CCHI pins -> venus_port FIFO banks -> admission (kind decode, same-address
//   CAM, round-robin over ports) -> PARALLELISM venus_mshr tracker slots ->
//   shared single-outstanding backends (directory, snoop filter, AXI masters)
//   and per-cycle emission muxes (<=1 SNP, <=1 DnRSP, <=1 DnDAT). Home-allocated
//   IDs (snoop TxnIDs + DBIDs) come from one venus_id_alloc per port; returning
//   flits are steered to slots via the sn_slot/db_slot tables.
//
// Slot-based arbitration (directory/SF/AXI/emission selects) is fixed
// lowest-slot-index priority by default; AGE_MATRIX=1 switches all of them to
// transaction-age oldest-first via one shared venus_age_matrix (slot birth =
// admission), with no change to any downstream muxing/grant/steering logic.
// ============================================================================

module venus #(
    parameter int NUM_T1            = 4,   // Type-1 upstream ports live (1..T1_MAX=8)
    parameter int NUM_AXI           = 1,   // AXI downstream channels (1..2 on these pins)
    parameter int PARALLELISM       = 8,   // tracker slots
    parameter int NODE_ID           = 16,  // this home's CCHI node ID
    parameter int INFLIGHT_SNP      = 4,   // per-port TX SNP FIFO depth
    parameter int Q_EVT             = 2,   // per-port RX EVT FIFO depth
    parameter int Q_REQ             = 2,   // per-port RX REQ FIFO depth
    parameter int Q_RSP             = 8,   // per-port RSP FIFO depths
    parameter int Q_DAT             = 16,  // per-port DAT FIFO depths
    parameter int DIR_SETS          = 1024,
    parameter int DIR_WAYS          = 4,
    parameter int SF_ENABLE         = 1,   // snoop filter present
    parameter int SF_ALLOW_OVER     = 1,   // allow over-inclusive snoop masks
    parameter int SF_BROADCAST      = 0,   // always snoop all ports
    parameter int SF_SETS           = 256,
    parameter int SF_WAYS           = 4,
    parameter int TAGALIAS_W        = 0,   // TagAlias field width in use (0 = off .. 8)
    parameter int AGE_MATRIX        = 0,   // 1 = transaction-age oldest-first slot
                                           // arbitration (0 = fixed slot priority)
    parameter int STORAGE_DIR       = 0,   // STOR_REG / STOR_SRAM
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

    `define VENUS_T1_PORT(P) \
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

    `VENUS_T1_PORT(0),
    `VENUS_T1_PORT(1),
    `VENUS_T1_PORT(2),
    `VENUS_T1_PORT(3),
    `VENUS_T1_PORT(4),
    `VENUS_T1_PORT(5),
    `VENUS_T1_PORT(6),
    `VENUS_T1_PORT(7),

    `define VENUS_AXI_PORT(P) \
    output logic                    axi_m``P``_awvalid, \
    input  wire                     axi_m``P``_awready, \
    output logic [AXI_ADDR_W-1:0]   axi_m``P``_awaddr, \
    output logic [AXI_ID_W-1:0]     axi_m``P``_awid, \
    output logic [7:0]              axi_m``P``_awlen, \
    output logic [2:0]              axi_m``P``_awsize, \
    output logic [1:0]              axi_m``P``_awburst, \
    output logic                    axi_m``P``_wvalid, \
    input  wire                     axi_m``P``_wready, \
    output logic [AXI_DATA_W-1:0]   axi_m``P``_wdata, \
    output logic [AXI_DATA_W/8-1:0] axi_m``P``_wstrb, \
    output logic                    axi_m``P``_wlast, \
    input  wire                     axi_m``P``_bvalid, \
    output logic                    axi_m``P``_bready, \
    input  wire  [AXI_ID_W-1:0]     axi_m``P``_bid, \
    input  wire  [1:0]              axi_m``P``_bresp, \
    output logic                    axi_m``P``_arvalid, \
    input  wire                     axi_m``P``_arready, \
    output logic [AXI_ADDR_W-1:0]   axi_m``P``_araddr, \
    output logic [AXI_ID_W-1:0]     axi_m``P``_arid, \
    output logic [7:0]              axi_m``P``_arlen, \
    output logic [2:0]              axi_m``P``_arsize, \
    output logic [1:0]              axi_m``P``_arburst, \
    input  wire                     axi_m``P``_rvalid, \
    output logic                    axi_m``P``_rready, \
    input  wire  [AXI_DATA_W-1:0]   axi_m``P``_rdata, \
    input  wire  [AXI_ID_W-1:0]     axi_m``P``_rid, \
    input  wire  [1:0]              axi_m``P``_rresp, \
    input  wire                     axi_m``P``_rlast

    `VENUS_AXI_PORT(0),
    `VENUS_AXI_PORT(1),
    `VENUS_AXI_PORT(2),
    `VENUS_AXI_PORT(3)
);

    import venus_pkg::*;

    `undef VENUS_T1_PORT
    `undef VENUS_AXI_PORT

    localparam int PORT_W   = clog2_min1(NUM_T1);
    localparam int SLOT_W   = clog2_min1(PARALLELISM);
    localparam int AXI_N    = NUM_AXI;
    localparam int AXI_SEL_W = clog2_min1(AXI_N);
    localparam int ID_W     = TXNID_W;
    localparam int TAGAW    = (TAGALIAS_W > 0) ? TAGALIAS_W : 1;  // min-1 alias width

`ifndef SYNTHESIS
    // Elaboration-time parameter contract (docs/ARCHITECTURE.md §5).
    initial begin
        if (NUM_T1 < 1 || NUM_T1 > T1_MAX)
            $fatal(1, "venus: NUM_T1=%0d out of range 1..%0d", NUM_T1, T1_MAX);
        if (PARALLELISM < 1)
            $fatal(1, "venus: PARALLELISM=%0d must be >= 1", PARALLELISM);
        if (NUM_AXI < 1 || NUM_AXI > AXI_MAX)
            $fatal(1, "venus: NUM_AXI=%0d out of range 1..%0d", NUM_AXI, AXI_MAX);
        if (TAGALIAS_W < 0 || TAGALIAS_W > 8)
            $fatal(1, "venus: TAGALIAS_W=%0d out of range 0..8", TAGALIAS_W);
        if (AGE_MATRIX < 0 || AGE_MATRIX > 1)
            $fatal(1, "venus: AGE_MATRIX=%0d must be 0 or 1", AGE_MATRIX);
    end
`endif

    // ------------------------------------------------------------------ pins
    // T1_MAX port sets on the boundary; ports >= NUM_T1 are tied off below.
    evt_flit_t   pin_rxevt [T1_MAX];
    req_flit_t   pin_rxreq [T1_MAX];
    uprsp_flit_t pin_rxrsp [T1_MAX];
    updat_flit_t pin_rxdat [T1_MAX];
    logic        pin_rxevt_v [T1_MAX], pin_rxevt_r [T1_MAX];
    logic        pin_rxreq_v [T1_MAX], pin_rxreq_r [T1_MAX];
    logic        pin_rxrsp_v [T1_MAX], pin_rxrsp_r [T1_MAX];
    logic        pin_rxdat_v [T1_MAX], pin_rxdat_r [T1_MAX];
    logic        pin_txsnp_v [T1_MAX], pin_txsnp_r [T1_MAX];
    logic        pin_txrsp_v [T1_MAX], pin_txrsp_r [T1_MAX];
    logic        pin_txdat_v [T1_MAX], pin_txdat_r [T1_MAX];
    snp_flit_t   pin_txsnp [T1_MAX];
    dnrsp_flit_t pin_txrsp [T1_MAX];
    dndat_flit_t pin_txdat [T1_MAX];

    `define VENUS_MAP_EVT(P) \
        pin_rxevt_v[P] = cchi_t1p``P``_rxevt_valid; \
        cchi_t1p``P``_rxevt_ready = pin_rxevt_r[P]; \
        pin_rxevt[P].txnid    = cchi_t1p``P``_rxevt_bits_TxnID; \
        pin_rxevt[P].srcid    = cchi_t1p``P``_rxevt_bits_SrcID; \
        pin_rxevt[P].tgtid    = cchi_t1p``P``_rxevt_bits_TgtID; \
        pin_rxevt[P].opcode   = cchi_t1p``P``_rxevt_bits_Opcode; \
        pin_rxevt[P].addr     = cchi_t1p``P``_rxevt_bits_Addr; \
        pin_rxevt[P].ns       = cchi_t1p``P``_rxevt_bits_NS; \
        pin_rxevt[P].memattr  = cchi_t1p``P``_rxevt_bits_MemAttr; \
        pin_rxevt[P].wayvalid = cchi_t1p``P``_rxevt_bits_WayValid; \
        pin_rxevt[P].way      = cchi_t1p``P``_rxevt_bits_Way; \
        pin_rxevt[P].tracetag = cchi_t1p``P``_rxevt_bits_TraceTag;

    `define VENUS_MAP_REQ(P) \
        pin_rxreq_v[P] = cchi_t1p``P``_rxreq_valid; \
        cchi_t1p``P``_rxreq_ready = pin_rxreq_r[P]; \
        pin_rxreq[P].txnid          = cchi_t1p``P``_rxreq_bits_TxnID; \
        pin_rxreq[P].srcid          = cchi_t1p``P``_rxreq_bits_SrcID; \
        pin_rxreq[P].tgtid          = cchi_t1p``P``_rxreq_bits_TgtID; \
        pin_rxreq[P].opcode         = cchi_t1p``P``_rxreq_bits_Opcode; \
        pin_rxreq[P].size           = cchi_t1p``P``_rxreq_bits_Size; \
        pin_rxreq[P].addr           = cchi_t1p``P``_rxreq_bits_Addr; \
        pin_rxreq[P].tagalias       = (TAGALIAS_W > 0) ? cchi_t1p``P``_rxreq_bits_TagAlias : 8'h00; \
        pin_rxreq[P].ns             = cchi_t1p``P``_rxreq_bits_NS; \
        pin_rxreq[P].order          = cchi_t1p``P``_rxreq_bits_Order; \
        pin_rxreq[P].memattr        = cchi_t1p``P``_rxreq_bits_MemAttr; \
        pin_rxreq[P].excl           = cchi_t1p``P``_rxreq_bits_Excl; \
        pin_rxreq[P].exp_comp_data  = cchi_t1p``P``_rxreq_bits_ExpCompData; \
        pin_rxreq[P].tracetag       = cchi_t1p``P``_rxreq_bits_TraceTag; \
        pin_rxreq[P].wayvalid       = cchi_t1p``P``_rxreq_bits_WayValid; \
        pin_rxreq[P].way            = cchi_t1p``P``_rxreq_bits_Way;

    `define VENUS_MAP_RSP(P) \
        pin_rxrsp_v[P] = cchi_t1p``P``_rxrsp_valid; \
        cchi_t1p``P``_rxrsp_ready = pin_rxrsp_r[P]; \
        pin_rxrsp[P].txnid    = cchi_t1p``P``_rxrsp_bits_TxnID; \
        pin_rxrsp[P].srcid    = cchi_t1p``P``_rxrsp_bits_SrcID; \
        pin_rxrsp[P].tgtid    = cchi_t1p``P``_rxrsp_bits_TgtID; \
        pin_rxrsp[P].opcode   = cchi_t1p``P``_rxrsp_bits_Opcode; \
        pin_rxrsp[P].resperr  = cchi_t1p``P``_rxrsp_bits_RespErr; \
        pin_rxrsp[P].resp     = cchi_t1p``P``_rxrsp_bits_Resp; \
        pin_rxrsp[P].tracetag = cchi_t1p``P``_rxrsp_bits_TraceTag;

    `define VENUS_MAP_DAT(P) \
        pin_rxdat_v[P] = cchi_t1p``P``_rxdat_valid; \
        cchi_t1p``P``_rxdat_ready = pin_rxdat_r[P]; \
        pin_rxdat[P].txnid    = cchi_t1p``P``_rxdat_bits_TxnID; \
        pin_rxdat[P].srcid    = cchi_t1p``P``_rxdat_bits_SrcID; \
        pin_rxdat[P].tgtid    = cchi_t1p``P``_rxdat_bits_TgtID; \
        pin_rxdat[P].opcode   = cchi_t1p``P``_rxdat_bits_Opcode; \
        pin_rxdat[P].resperr  = cchi_t1p``P``_rxdat_bits_RespErr; \
        pin_rxdat[P].resp     = cchi_t1p``P``_rxdat_bits_Resp; \
        pin_rxdat[P].dataid   = cchi_t1p``P``_rxdat_bits_DataID; \
        pin_rxdat[P].be       = cchi_t1p``P``_rxdat_bits_BE; \
        pin_rxdat[P].tracetag = cchi_t1p``P``_rxdat_bits_TraceTag; \
        pin_rxdat[P].data     = {cchi_t1p``P``_rxdat_bits_Data[7], cchi_t1p``P``_rxdat_bits_Data[6], \
                                 cchi_t1p``P``_rxdat_bits_Data[5], cchi_t1p``P``_rxdat_bits_Data[4], \
                                 cchi_t1p``P``_rxdat_bits_Data[3], cchi_t1p``P``_rxdat_bits_Data[2], \
                                 cchi_t1p``P``_rxdat_bits_Data[1], cchi_t1p``P``_rxdat_bits_Data[0]};

    `define VENUS_MAP_TX(P) \
        cchi_t1p``P``_txsnp_valid         = pin_txsnp_v[P]; \
        pin_txsnp_r[P]                    = cchi_t1p``P``_txsnp_ready; \
        cchi_t1p``P``_txsnp_bits_TxnID    = pin_txsnp[P].txnid; \
        cchi_t1p``P``_txsnp_bits_SrcID    = pin_txsnp[P].srcid; \
        cchi_t1p``P``_txsnp_bits_TgtID    = pin_txsnp[P].tgtid; \
        cchi_t1p``P``_txsnp_bits_Opcode   = pin_txsnp[P].opcode; \
        cchi_t1p``P``_txsnp_bits_Addr     = pin_txsnp[P].addr; \
        cchi_t1p``P``_txsnp_bits_NS       = pin_txsnp[P].ns; \
        cchi_t1p``P``_txsnp_bits_TraceTag = pin_txsnp[P].tracetag; \
        cchi_t1p``P``_txrsp_valid         = pin_txrsp_v[P]; \
        pin_txrsp_r[P]                    = cchi_t1p``P``_txrsp_ready; \
        cchi_t1p``P``_txrsp_bits_TxnID    = pin_txrsp[P].txnid; \
        cchi_t1p``P``_txrsp_bits_SrcID    = pin_txrsp[P].srcid; \
        cchi_t1p``P``_txrsp_bits_TgtID    = pin_txrsp[P].tgtid; \
        cchi_t1p``P``_txrsp_bits_DBID     = pin_txrsp[P].dbid; \
        cchi_t1p``P``_txrsp_bits_Opcode   = pin_txrsp[P].opcode; \
        cchi_t1p``P``_txrsp_bits_RespErr  = pin_txrsp[P].resperr; \
        cchi_t1p``P``_txrsp_bits_Resp     = pin_txrsp[P].resp; \
        cchi_t1p``P``_txrsp_bits_CBusy    = pin_txrsp[P].cbusy; \
        cchi_t1p``P``_txrsp_bits_WayValid = pin_txrsp[P].wayvalid; \
        cchi_t1p``P``_txrsp_bits_Way      = pin_txrsp[P].way; \
        cchi_t1p``P``_txrsp_bits_TraceTag = pin_txrsp[P].tracetag; \
        cchi_t1p``P``_txdat_valid         = pin_txdat_v[P]; \
        pin_txdat_r[P]                    = cchi_t1p``P``_txdat_ready; \
        cchi_t1p``P``_txdat_bits_TxnID    = pin_txdat[P].txnid; \
        cchi_t1p``P``_txdat_bits_SrcID    = pin_txdat[P].srcid; \
        cchi_t1p``P``_txdat_bits_TgtID    = pin_txdat[P].tgtid; \
        cchi_t1p``P``_txdat_bits_DBID     = pin_txdat[P].dbid; \
        cchi_t1p``P``_txdat_bits_Opcode   = pin_txdat[P].opcode; \
        cchi_t1p``P``_txdat_bits_RespErr  = pin_txdat[P].resperr; \
        cchi_t1p``P``_txdat_bits_Resp     = pin_txdat[P].resp; \
        cchi_t1p``P``_txdat_bits_DataSource = pin_txdat[P].datasource; \
        cchi_t1p``P``_txdat_bits_CBusy    = pin_txdat[P].cbusy; \
        cchi_t1p``P``_txdat_bits_DataID   = pin_txdat[P].dataid; \
        cchi_t1p``P``_txdat_bits_WayValid = pin_txdat[P].wayvalid; \
        cchi_t1p``P``_txdat_bits_Way      = pin_txdat[P].way; \
        cchi_t1p``P``_txdat_bits_TraceTag = pin_txdat[P].tracetag; \
        cchi_t1p``P``_txdat_bits_Data[0]  = pin_txdat[P].data[31:0]; \
        cchi_t1p``P``_txdat_bits_Data[1]  = pin_txdat[P].data[63:32]; \
        cchi_t1p``P``_txdat_bits_Data[2]  = pin_txdat[P].data[95:64]; \
        cchi_t1p``P``_txdat_bits_Data[3]  = pin_txdat[P].data[127:96]; \
        cchi_t1p``P``_txdat_bits_Data[4]  = pin_txdat[P].data[159:128]; \
        cchi_t1p``P``_txdat_bits_Data[5]  = pin_txdat[P].data[191:160]; \
        cchi_t1p``P``_txdat_bits_Data[6]  = pin_txdat[P].data[223:192]; \
        cchi_t1p``P``_txdat_bits_Data[7]  = pin_txdat[P].data[255:224];

    always_comb begin
        `VENUS_MAP_EVT(0) `VENUS_MAP_REQ(0) `VENUS_MAP_RSP(0) `VENUS_MAP_DAT(0) `VENUS_MAP_TX(0)
        `VENUS_MAP_EVT(1) `VENUS_MAP_REQ(1) `VENUS_MAP_RSP(1) `VENUS_MAP_DAT(1) `VENUS_MAP_TX(1)
        `VENUS_MAP_EVT(2) `VENUS_MAP_REQ(2) `VENUS_MAP_RSP(2) `VENUS_MAP_DAT(2) `VENUS_MAP_TX(2)
        `VENUS_MAP_EVT(3) `VENUS_MAP_REQ(3) `VENUS_MAP_RSP(3) `VENUS_MAP_DAT(3) `VENUS_MAP_TX(3)
        `VENUS_MAP_EVT(4) `VENUS_MAP_REQ(4) `VENUS_MAP_RSP(4) `VENUS_MAP_DAT(4) `VENUS_MAP_TX(4)
        `VENUS_MAP_EVT(5) `VENUS_MAP_REQ(5) `VENUS_MAP_RSP(5) `VENUS_MAP_DAT(5) `VENUS_MAP_TX(5)
        `VENUS_MAP_EVT(6) `VENUS_MAP_REQ(6) `VENUS_MAP_RSP(6) `VENUS_MAP_DAT(6) `VENUS_MAP_TX(6)
        `VENUS_MAP_EVT(7) `VENUS_MAP_REQ(7) `VENUS_MAP_RSP(7) `VENUS_MAP_DAT(7) `VENUS_MAP_TX(7)
    end

    `undef VENUS_MAP_EVT
    `undef VENUS_MAP_REQ
    `undef VENUS_MAP_RSP
    `undef VENUS_MAP_DAT
    `undef VENUS_MAP_TX

    // ------------------------------------------------------------------ AXI pins
    // AXI_MAX channel sets on the boundary; channels >= NUM_AXI are tied off
    // (the g_axim generate below drives their arrays to idle).
    logic                    ax_awvalid [AXI_MAX], ax_awready [AXI_MAX];
    logic [AXI_ADDR_W-1:0]   ax_awaddr  [AXI_MAX];
    logic [AXI_ID_W-1:0]     ax_awid    [AXI_MAX];
    logic [7:0]              ax_awlen   [AXI_MAX];
    logic [2:0]              ax_awsize  [AXI_MAX];
    logic [1:0]              ax_awburst [AXI_MAX];
    logic                    ax_wvalid  [AXI_MAX], ax_wready [AXI_MAX];
    logic [AXI_DATA_W-1:0]   ax_wdata   [AXI_MAX];
    logic [AXI_DATA_W/8-1:0] ax_wstrb   [AXI_MAX];
    logic                    ax_wlast   [AXI_MAX];
    logic                    ax_bvalid  [AXI_MAX], ax_bready [AXI_MAX];
    logic [AXI_ID_W-1:0]     ax_bid     [AXI_MAX];
    logic [1:0]              ax_bresp   [AXI_MAX];
    logic                    ax_arvalid [AXI_MAX], ax_arready [AXI_MAX];
    logic [AXI_ADDR_W-1:0]   ax_araddr  [AXI_MAX];
    logic [AXI_ID_W-1:0]     ax_arid    [AXI_MAX];
    logic [7:0]              ax_arlen   [AXI_MAX];
    logic [2:0]              ax_arsize  [AXI_MAX];
    logic [1:0]              ax_arburst [AXI_MAX];
    logic                    ax_rvalid  [AXI_MAX], ax_rready [AXI_MAX];
    logic [AXI_DATA_W-1:0]   ax_rdata   [AXI_MAX];
    logic [AXI_ID_W-1:0]     ax_rid     [AXI_MAX];
    logic [1:0]              ax_rresp   [AXI_MAX];
    logic                    ax_rlast   [AXI_MAX];

    `define VENUS_AXI_PINMAP(P) \
        axi_m``P``_awvalid = ax_awvalid[P]; \
        ax_awready[P]      = axi_m``P``_awready; \
        axi_m``P``_awaddr  = ax_awaddr[P]; \
        axi_m``P``_awid    = ax_awid[P]; \
        axi_m``P``_awlen   = ax_awlen[P]; \
        axi_m``P``_awsize  = ax_awsize[P]; \
        axi_m``P``_awburst = ax_awburst[P]; \
        axi_m``P``_wvalid  = ax_wvalid[P]; \
        ax_wready[P]       = axi_m``P``_wready; \
        axi_m``P``_wdata   = ax_wdata[P]; \
        axi_m``P``_wstrb   = ax_wstrb[P]; \
        axi_m``P``_wlast   = ax_wlast[P]; \
        ax_bvalid[P]       = axi_m``P``_bvalid; \
        axi_m``P``_bready  = ax_bready[P]; \
        ax_bid[P]          = axi_m``P``_bid; \
        ax_bresp[P]        = axi_m``P``_bresp; \
        axi_m``P``_arvalid = ax_arvalid[P]; \
        ax_arready[P]      = axi_m``P``_arready; \
        axi_m``P``_araddr  = ax_araddr[P]; \
        axi_m``P``_arid    = ax_arid[P]; \
        axi_m``P``_arlen   = ax_arlen[P]; \
        axi_m``P``_arsize  = ax_arsize[P]; \
        axi_m``P``_arburst = ax_arburst[P]; \
        ax_rvalid[P]       = axi_m``P``_rvalid; \
        axi_m``P``_rready  = ax_rready[P]; \
        ax_rdata[P]        = axi_m``P``_rdata; \
        ax_rid[P]          = axi_m``P``_rid; \
        ax_rresp[P]        = axi_m``P``_rresp; \
        ax_rlast[P]        = axi_m``P``_rlast;

    always_comb begin
        `VENUS_AXI_PINMAP(0)
        `VENUS_AXI_PINMAP(1)
        `VENUS_AXI_PINMAP(2)
        `VENUS_AXI_PINMAP(3)
    end

    `undef VENUS_AXI_PINMAP

    // ------------------------------------------------------------------ port adapters
    // Per-port FIFO banks (venus_port): Decoupled pins <-> internal flit
    // streams. RX-side ready is occupancy-only by construction (venus_fifo).
    evt_flit_t   evtf [NUM_T1];
    req_flit_t   reqf [NUM_T1];
    uprsp_flit_t uprspf [NUM_T1];
    updat_flit_t updatf [NUM_T1];
    logic        evtf_v [NUM_T1], evtf_r [NUM_T1];
    logic        reqf_v [NUM_T1], reqf_r [NUM_T1];
    logic        uprspf_v [NUM_T1], uprspf_r [NUM_T1];
    logic        updatf_v [NUM_T1], updatf_r [NUM_T1];

    logic        snpi_v [NUM_T1], snpi_r [NUM_T1];
    snp_flit_t   snpi [NUM_T1];
    logic        dnrspi_v [NUM_T1], dnrspi_r [NUM_T1];
    dnrsp_flit_t dnrspi [NUM_T1];
    logic        dndati_v [NUM_T1], dndati_r [NUM_T1];
    dndat_flit_t dndati [NUM_T1];

    logic        txrsp_fire [NUM_T1];
    logic [TXNID_W-1:0] txrsp_txnid [NUM_T1];
    logic [2:0]  txrsp_opc [NUM_T1];

    genvar gp;
    generate
        for (gp = 0; gp < T1_MAX; gp++) begin : g_port
            if (gp < NUM_T1) begin : g_live
                venus_port #(
                    .Q_EVT(Q_EVT), .Q_REQ(Q_REQ), .Q_RSP(Q_RSP), .Q_DAT(Q_DAT),
                    .Q_SNP(INFLIGHT_SNP)
                ) u_port (
                    .clock(clock), .reset(reset),
                    .rxevt_valid(pin_rxevt_v[gp]), .rxevt_ready(pin_rxevt_r[gp]), .rxevt_bits(pin_rxevt[gp]),
                    .rxreq_valid(pin_rxreq_v[gp]), .rxreq_ready(pin_rxreq_r[gp]), .rxreq_bits(pin_rxreq[gp]),
                    .txsnp_valid(pin_txsnp_v[gp]), .txsnp_ready(pin_txsnp_r[gp]), .txsnp_bits(pin_txsnp[gp]),
                    .txrsp_valid(pin_txrsp_v[gp]), .txrsp_ready(pin_txrsp_r[gp]), .txrsp_bits(pin_txrsp[gp]),
                    .rxrsp_valid(pin_rxrsp_v[gp]), .rxrsp_ready(pin_rxrsp_r[gp]), .rxrsp_bits(pin_rxrsp[gp]),
                    .txdat_valid(pin_txdat_v[gp]), .txdat_ready(pin_txdat_r[gp]), .txdat_bits(pin_txdat[gp]),
                    .rxdat_valid(pin_rxdat_v[gp]), .rxdat_ready(pin_rxdat_r[gp]), .rxdat_bits(pin_rxdat[gp]),
                    .evt_valid(evtf_v[gp]), .evt_ready(evtf_r[gp]), .evt_bits(evtf[gp]),
                    .req_valid(reqf_v[gp]), .req_ready(reqf_r[gp]), .req_bits(reqf[gp]),
                    .uprsp_valid(uprspf_v[gp]), .uprsp_ready(uprspf_r[gp]), .uprsp_bits(uprspf[gp]),
                    .updat_valid(updatf_v[gp]), .updat_ready(updatf_r[gp]), .updat_bits(updatf[gp]),
                    .snp_valid(snpi_v[gp]), .snp_ready(snpi_r[gp]), .snp_bits(snpi[gp]),
                    .dnrsp_valid(dnrspi_v[gp]), .dnrsp_ready(dnrspi_r[gp]), .dnrsp_bits(dnrspi[gp]),
                    .dndat_valid(dndati_v[gp]), .dndat_ready(dndati_r[gp]), .dndat_bits(dndati[gp]),
                    .txrsp_fire(txrsp_fire[gp]), .txrsp_txnid(txrsp_txnid[gp]), .txrsp_opcode(txrsp_opc[gp])
                );
            end else begin : g_tie
                assign pin_rxevt_r[gp] = 1'b0;
                assign pin_rxreq_r[gp] = 1'b0;
                assign pin_rxrsp_r[gp] = 1'b0;
                assign pin_rxdat_r[gp] = 1'b0;
                assign pin_txsnp_v[gp] = 1'b0;
                assign pin_txsnp[gp]   = '0;
                assign pin_txrsp_v[gp] = 1'b0;
                assign pin_txrsp[gp]   = '0;
                assign pin_txdat_v[gp] = 1'b0;
                assign pin_txdat[gp]   = '0;
            end
        end
    endgenerate

    // ------------------------------------------------------------------ ID allocators
    // One per port; the ID space is shared by snoop TxnIDs and DBIDs
    // (PROTOCOL.md §6). free0 = UpRSP-side (CompAck / SnpResp), free1 =
    // UpDAT-side (write-data last beat / SnpRespData last beat).
    logic                id_alloc_req [NUM_T1];
    logic                id_alloc_gnt [NUM_T1];
    logic                id_avail     [NUM_T1];
    logic [ID_W-1:0]     id_alloc_id  [NUM_T1];
    logic                id_free0_req [NUM_T1];
    logic [ID_W-1:0]     id_free0_id  [NUM_T1];
    logic                id_free1_req [NUM_T1];
    logic [ID_W-1:0]     id_free1_id  [NUM_T1];

    generate
        for (gp = 0; gp < NUM_T1; gp++) begin : g_id
            venus_id_alloc #(.N(ID_SPACE)) u_id (
                .clock(clock), .reset(reset),
                .alloc_req(id_alloc_req[gp]),
                .alloc_gnt(id_alloc_gnt[gp]),
                .avail    (id_avail[gp]),
                .alloc_id (id_alloc_id[gp]),
                .free0_req(id_free0_req[gp]),
                .free0_id (id_free0_id[gp]),
                .free1_req(id_free1_req[gp]),
                .free1_id (id_free1_id[gp])
            );
        end
    endgenerate

    // ==================================================================
    // Tracker slots (PARALLELISM x venus_mshr)
    // ==================================================================
    localparam int PAR = PARALLELISM;

    // Shared admission bus (at most one admission per cycle; per-slot strobe).
    logic [PAR-1:0]     alloc_stb;
    xact_kind_e         alloc_kind;
    logic [5:0]         alloc_opcode;
    logic [PORT_W-1:0]  alloc_port;
    logic [TXNID_W-1:0] alloc_txnid;
    logic [LINE_W-1:0]  alloc_line;
    logic [2:0]         alloc_size;
    logic               alloc_addr5;
    logic               alloc_expcd;
    logic [7:0]         alloc_alias;   // admitted REQ's TagAlias (0 for EVTs)
    logic [AXI_SEL_W-1:0] alloc_axi;

    logic               sl_busy [PAR];
    logic [LINE_W-1:0]  sl_line [PAR];
    logic [PORT_W-1:0]  sl_port [PAR];
    logic [AXI_SEL_W-1:0] sl_axi_sel [PAR];
    logic               sl_kind_write [PAR];
    logic               sl_kind_evt [PAR];
    logic               sl_wr_hold [PAR];
    logic               sl_wb_blk [PAR];
    logic [LINE_W-1:0]  sl_vline [PAR];
    logic               sl_vwb_hold [PAR];
    logic               sl_vic_blk [PAR];
    logic               sl_vic_act [PAR];

    logic               sl_dir_req [PAR];
    logic               sl_dir_gnt [PAR];
    logic [1:0]         sl_dir_op [PAR];
    logic [LINE_W-1:0]  sl_dir_line [PAR];
    logic [PORT_W-1:0]  sl_dir_port [PAR];
    logic               sl_dir_rmline [PAR];
    logic [1:0]         sl_dir_state [PAR];
    logic [NUM_T1-1:0]  sl_dir_sharers [PAR];
    logic [PORT_W-1:0]  sl_dir_owner [PAR];
    logic [NUM_T1*TAGAW-1:0] sl_dir_aliases [PAR];
    logic               sl_dir_men [PAR];
    logic [1:0]         sl_dir_mstate [PAR];
    logic [NUM_T1-1:0]  sl_dir_msharers [PAR];
    logic [PORT_W-1:0]  sl_dir_mowner [PAR];
    logic               sl_dir_rsp_valid [PAR];

    logic               sl_sf_req [PAR];
    logic               sl_sf_gnt [PAR];
    logic [1:0]         sl_sf_op [PAR];
    logic [LINE_W-1:0]  sl_sf_line [PAR];
    logic [NUM_T1-1:0]  sl_sf_presence [PAR];
    logic [PORT_W-1:0]  sl_sf_port [PAR];
    logic               sl_sf_rsp_valid [PAR];

    logic               sl_axi_rd_req [PAR];
    logic               sl_axi_rd_gnt [PAR];
    logic [ADDR_W-1:0]  sl_axi_rd_addr [PAR];
    logic [7:0]         sl_axi_rd_len [PAR];
    logic               sl_axi_rd_start [PAR];
    logic               sl_axi_rd_done [PAR];
    logic [LINE_BITS-1:0] sl_axi_rd_data [PAR];
    logic               sl_axi_wr_req [PAR];
    logic               sl_axi_wr_gnt [PAR];
    logic [ADDR_W-1:0]  sl_axi_wr_addr [PAR];
    logic [7:0]         sl_axi_wr_len [PAR];
    logic               sl_axi_wr_start [PAR];
    logic [LINE_BITS-1:0] sl_axi_wr_data [PAR];
    logic [LINE_BYTES-1:0] sl_axi_wr_strb [PAR];
    logic               sl_axi_wr_done [PAR];

    logic               sl_snp_req [PAR];
    logic               sl_snp_gnt [PAR];
    logic [DBID_W-1:0]  sl_snp_gnt_id [PAR];
    logic [PORT_W-1:0]  sl_snp_port [PAR];
    snp_flit_t          sl_snp_bits [PAR];

    logic               sl_dnrsp_req [PAR];
    logic               sl_dnrsp_gnt [PAR];
    dnrsp_flit_t        sl_dnrsp_bits [PAR];
    logic               sl_dndat_req [PAR];
    logic               sl_dndat_gnt [PAR];
    dndat_flit_t        sl_dndat_bits [PAR];
    logic               sl_dbid_req [PAR];
    logic [DBID_W-1:0]  sl_dbid_gnt_id [PAR];

    logic [NUM_T1-1:0]  sl_snpans_valid [PAR];
    logic [DBID_W-1:0]  sl_snpans_id [PAR][NUM_T1];
    logic [2:0]         sl_snpans_resp [PAR][NUM_T1];
    logic [NUM_T1-1:0]  sl_snpdat_valid [PAR];
    logic [DBID_W-1:0]  sl_snpdat_id [PAR][NUM_T1];
    logic [NUM_T1-1:0]  sl_snpdat_last [PAR];
    logic [2:0]         sl_snpdat_resp [PAR][NUM_T1];
    logic [NUM_T1-1:0]  sl_snpdat_beat [PAR];
    logic [DATA_W-1:0]  sl_snpdat_data [PAR][NUM_T1];

    logic               sl_wrdat_valid [PAR];
    logic               sl_wrdat_beat [PAR];
    logic [BE_W-1:0]    sl_wrdat_be [PAR];
    logic [DATA_W-1:0]  sl_wrdat_data [PAR];
    logic               sl_ack [PAR];

    logic [NUM_T1-1:0]  pop_valid;
    logic [TXNID_W-1:0] pop_id [NUM_T1];

    logic               sl_wr_dbid [PAR];
    logic [DBID_W-1:0]  sl_wr_dbid_id [PAR];
    logic               sl_wr_last [PAR];

    // Directory/snoop-filter response payloads (broadcast; per-slot strobes).
    logic               d_rsp_valid, d_rsp_hit, d_rsp_full, d_rsp_vlive;
    logic [1:0]         d_rsp_state, d_rsp_vstate;
    logic [NUM_T1-1:0]  d_rsp_sharers, d_rsp_vsharers;
    logic [PORT_W-1:0]  d_rsp_owner, d_rsp_vowner;
    logic [NUM_T1*TAGAW-1:0] d_rsp_aliases;
    logic [LINE_W-1:0]  d_rsp_vline;
    logic               f_rsp_valid;
    logic [NUM_T1-1:0]  f_rsp_presence;
    logic               f_rsp_maybe;

    genvar gi;
    generate
        for (gi = 0; gi < PAR; gi++) begin : g_slot
            venus_mshr #(
                .NUM_T1       (NUM_T1),
                .NUM_AXI      (AXI_N),
                .NODE_ID      (NODE_ID),
                .SF_ENABLE    (SF_ENABLE),
                .SF_ALLOW_OVER(SF_ALLOW_OVER),
                .SF_BROADCAST (SF_BROADCAST),
                .TAGALIAS_W   (TAGALIAS_W),
                .AXI_ID_W     (AXI_ID_W)
            ) u_mshr (
                .clock          (clock),
                .reset          (reset),
                .busy           (sl_busy[gi]),
                .cam_line       (sl_line[gi]),
                .alloc          (alloc_stb[gi]),
                .alloc_kind     (alloc_kind),
                .alloc_opcode   (alloc_opcode),
                .alloc_port     (alloc_port),
                .alloc_txnid    (alloc_txnid),
                .alloc_line     (alloc_line),
                .alloc_size     (alloc_size),
                .alloc_addr5    (alloc_addr5),
                .alloc_expcd    (alloc_expcd),
                .alloc_alias    (alloc_alias),
                .alloc_axi      (alloc_axi),
                .o_port         (sl_port[gi]),
                .o_axi_sel      (sl_axi_sel[gi]),
                .o_kind_write   (sl_kind_write[gi]),
                .o_kind_evt     (sl_kind_evt[gi]),
                .o_wr_hold      (sl_wr_hold[gi]),
                .o_vline        (sl_vline[gi]),
                .o_vwb_hold     (sl_vwb_hold[gi]),
                .o_vic_act      (sl_vic_act[gi]),
                .wb_blk         (sl_wb_blk[gi]),
                .vic_blk        (sl_vic_blk[gi]),
                .dir_req        (sl_dir_req[gi]),
                .dir_gnt        (sl_dir_gnt[gi]),
                .dir_op         (sl_dir_op[gi]),
                .dir_line       (sl_dir_line[gi]),
                .dir_port       (sl_dir_port[gi]),
                .dir_rmline     (sl_dir_rmline[gi]),
                .dir_state      (sl_dir_state[gi]),
                .dir_sharers    (sl_dir_sharers[gi]),
                .dir_owner      (sl_dir_owner[gi]),
                .dir_aliases    (sl_dir_aliases[gi]),
                .dir_men        (sl_dir_men[gi]),
                .dir_mstate     (sl_dir_mstate[gi]),
                .dir_msharers   (sl_dir_msharers[gi]),
                .dir_mowner     (sl_dir_mowner[gi]),
                .dir_rsp_valid  (sl_dir_rsp_valid[gi]),
                .dir_rsp_hit    (d_rsp_hit),
                .dir_rsp_state  (d_rsp_state),
                .dir_rsp_sharers(d_rsp_sharers),
                .dir_rsp_owner  (d_rsp_owner),
                .dir_rsp_aliases(d_rsp_aliases),
                .dir_rsp_full   (d_rsp_full),
                .dir_rsp_vlive  (d_rsp_vlive),
                .dir_rsp_vstate (d_rsp_vstate),
                .dir_rsp_vsharers(d_rsp_vsharers),
                .dir_rsp_vowner (d_rsp_vowner),
                .dir_rsp_vline  (d_rsp_vline),
                .sf_req         (sl_sf_req[gi]),
                .sf_gnt         (sl_sf_gnt[gi]),
                .sf_op          (sl_sf_op[gi]),
                .sf_line        (sl_sf_line[gi]),
                .sf_presence    (sl_sf_presence[gi]),
                .sf_port        (sl_sf_port[gi]),
                .sf_rsp_valid   (sl_sf_rsp_valid[gi]),
                .sf_rsp_presence(f_rsp_presence),
                .sf_rsp_maybe   (f_rsp_maybe),
                .axi_rd_req     (sl_axi_rd_req[gi]),
                .axi_rd_gnt     (sl_axi_rd_gnt[gi]),
                .axi_rd_addr    (sl_axi_rd_addr[gi]),
                .axi_rd_len     (sl_axi_rd_len[gi]),
                .axi_rd_start   (sl_axi_rd_start[gi]),
                .axi_rd_done    (sl_axi_rd_done[gi]),
                .axi_rd_data    (sl_axi_rd_data[gi]),
                .axi_wr_req     (sl_axi_wr_req[gi]),
                .axi_wr_gnt     (sl_axi_wr_gnt[gi]),
                .axi_wr_addr    (sl_axi_wr_addr[gi]),
                .axi_wr_len     (sl_axi_wr_len[gi]),
                .axi_wr_start   (sl_axi_wr_start[gi]),
                .axi_wr_data    (sl_axi_wr_data[gi]),
                .axi_wr_strb    (sl_axi_wr_strb[gi]),
                .axi_wr_done    (sl_axi_wr_done[gi]),
                .snp_req        (sl_snp_req[gi]),
                .snp_gnt        (sl_snp_gnt[gi]),
                .snp_gnt_id     (sl_snp_gnt_id[gi]),
                .snp_port       (sl_snp_port[gi]),
                .snp_bits       (sl_snp_bits[gi]),
                .dnrsp_req      (sl_dnrsp_req[gi]),
                .dnrsp_gnt      (sl_dnrsp_gnt[gi]),
                .dnrsp_bits     (sl_dnrsp_bits[gi]),
                .dndat_req      (sl_dndat_req[gi]),
                .dndat_gnt      (sl_dndat_gnt[gi]),
                .dndat_bits     (sl_dndat_bits[gi]),
                .dbid_req       (sl_dbid_req[gi]),
                .dbid_gnt_id    (sl_dbid_gnt_id[gi]),
                .snpans_valid   (sl_snpans_valid[gi]),
                .snpans_id      (sl_snpans_id[gi]),
                .snpans_resp    (sl_snpans_resp[gi]),
                .snpdat_valid   (sl_snpdat_valid[gi]),
                .snpdat_id      (sl_snpdat_id[gi]),
                .snpdat_last    (sl_snpdat_last[gi]),
                .snpdat_resp    (sl_snpdat_resp[gi]),
                .snpdat_beat    (sl_snpdat_beat[gi]),
                .snpdat_data    (sl_snpdat_data[gi]),
                .wrdat_valid    (sl_wrdat_valid[gi]),
                .wrdat_beat     (sl_wrdat_beat[gi]),
                .wrdat_be       (sl_wrdat_be[gi]),
                .wrdat_data     (sl_wrdat_data[gi]),
                .ack_valid      (sl_ack[gi]),
                .pop_valid      (pop_valid),
                .pop_id         (pop_id),
                .wr_dbid        (sl_wr_dbid[gi]),
                .wr_dbid_id     (sl_wr_dbid_id[gi]),
                .wr_last        (sl_wr_last[gi])
            );
        end
    endgenerate

    // ==================================================================
    // Admission: per-port candidate (EVT head before REQ head), kind decode,
    // unsupported-opcode drop, same-address CAM, round-robin across ports.
    // At most one admission per cycle into the lowest free slot.
    // ==================================================================
    logic               cand_v [NUM_T1];
    logic               cand_evt [NUM_T1];
    xact_kind_e         cand_kind [NUM_T1];
    logic [5:0]         cand_opc [NUM_T1];
    logic [TXNID_W-1:0] cand_txnid [NUM_T1];
    logic [LINE_W-1:0]  cand_line [NUM_T1];
    logic [2:0]         cand_size [NUM_T1];
    logic               cand_addr5 [NUM_T1];
    logic               cand_expcd [NUM_T1];
    logic [7:0]         cand_alias [NUM_T1];
    logic [NUM_T1-1:0]  drop_req;

    always_comb begin
        for (int p = 0; p < NUM_T1; p++) begin
            xact_kind_e rk;
            rk = req_kind(reqf[p].opcode);
            drop_req[p]  = reqf_v[p] && (rk == XK_DROP);

            cand_v[p]     = 1'b0;
            cand_evt[p]   = 1'b0;
            cand_kind[p]  = XK_DROP;
            cand_opc[p]   = '0;
            cand_txnid[p] = '0;
            cand_line[p]  = '0;
            cand_size[p]  = '0;
            cand_addr5[p] = 1'b0;
            cand_expcd[p] = 1'b0;
            cand_alias[p] = '0;

            if (evtf_v[p]) begin
                cand_v[p]     = 1'b1;
                cand_evt[p]   = 1'b1;
                cand_kind[p]  = evt_kind(evtf[p].opcode);
                cand_opc[p]   = {5'b00000, evtf[p].opcode};
                cand_txnid[p] = evtf[p].txnid;
                cand_line[p]  = addr_to_line(evtf[p].addr);
                cand_size[p]  = 3'd6;
                cand_addr5[p] = evtf[p].addr[5];
            end else if (reqf_v[p] && (rk != XK_DROP)) begin
                cand_v[p]     = 1'b1;
                cand_kind[p]  = rk;
                cand_opc[p]   = reqf[p].opcode;
                cand_txnid[p] = reqf[p].txnid;
                cand_line[p]  = addr_to_line(reqf[p].addr);
                cand_size[p]  = reqf[p].size;
                cand_addr5[p] = reqf[p].addr[5];
                cand_expcd[p] = reqf[p].exp_comp_data;
                cand_alias[p] = reqf[p].tagalias;
            end
        end
    end

    // Free-slot scan (lowest index) + same-address CAM (PROTOCOL §7.3).
    logic               fs_v;
    logic [SLOT_W-1:0]  fs_idx;
    logic [NUM_T1-1:0]  line_busy;

    always_comb begin
        fs_v   = 1'b0;
        fs_idx = '0;
        for (int i = PAR-1; i >= 0; i--) begin
            if (!sl_busy[i]) begin
                fs_v   = 1'b1;
                fs_idx = SLOT_W'(i);
            end
        end
        for (int p = 0; p < NUM_T1; p++) begin
            line_busy[p] = 1'b0;
            for (int i = 0; i < PAR; i++) begin
                if (sl_busy[i] && (sl_line[i] == cand_line[p]))
                    line_busy[p] = 1'b1;
                // §T15 reverse hazard: a REQ candidate also stalls while a
                // slot's victim sub-flow is armed on its line — the victim
                // entry still reads as a hit until PH_VIC_RM drops it, so an
                // admitted flow would otherwise plan on meta the victim flow
                // is about to remove. Deadlock-free: a victim sub-flow never
                // waits on admissions, so it always clears.
                if (sl_vic_act[i] && (sl_vline[i] == cand_line[p]))
                    line_busy[p] = 1'b1;
            end
        end
    end

    logic [PORT_W-1:0]  rr_ptr;
    logic [NUM_T1-1:0]  adm;
    logic               admit;
    logic [PORT_W-1:0]  admit_port;

    always_comb begin
        // Admission rule (PROTOCOL §7.3): a REQ candidate stalls while any
        // slot holds its line, or while any slot's §T15 victim sub-flow is
        // armed on its line (line_busy above); an EVT candidate may share a
        // line with an in-flight REQ — the EVT's first response is exactly
        // what unparks the upstream's snoop answer, so blocking it would
        // deadlock.
        for (int p = 0; p < NUM_T1; p++)
            adm[p] = cand_v[p] && fs_v && (cand_evt[p] || !line_busy[p]);
    end

    always_comb begin
        admit      = 1'b0;
        admit_port = rr_ptr;
        for (int k = 0; k < NUM_T1; k++) begin
            logic [PORT_W-1:0] idx;
            idx = PORT_W'((int'(rr_ptr) + k) % NUM_T1);
            if (!admit && adm[idx]) begin
                admit      = 1'b1;
                admit_port = idx;
            end
        end
    end

    // Line -> AXI channel map for the admitted transaction.
    logic [AXI_SEL_W-1:0] map_sel;
    venus_axi_map #(.NUM_AXI(AXI_N), .LINE_W(LINE_W)) u_map (
        .line    (alloc_line),
        .axi_sel (map_sel)
    );

    always_comb begin
        for (int i = 0; i < PAR; i++)
            alloc_stb[i] = admit && (fs_idx == SLOT_W'(i));

        alloc_kind   = cand_kind[admit_port];
        alloc_opcode = cand_opc[admit_port];
        alloc_port   = admit_port;
        alloc_txnid  = cand_txnid[admit_port];
        alloc_line   = cand_line[admit_port];
        alloc_size   = cand_size[admit_port];
        alloc_addr5  = cand_addr5[admit_port];
        alloc_expcd  = cand_expcd[admit_port];
        alloc_alias  = cand_alias[admit_port];
        alloc_axi    = map_sel;

        for (int p = 0; p < NUM_T1; p++) begin
            evtf_r[p] = admit && (admit_port == PORT_W'(p)) && cand_evt[p];
            reqf_r[p] = drop_req[p]
                        || (admit && (admit_port == PORT_W'(p)) && !cand_evt[p]);
        end
    end

    always_ff @(posedge clock or posedge reset) begin
        if (reset)
            rr_ptr <= '0;
        else if (admit)
            rr_ptr <= (admit_port == PORT_W'(NUM_T1-1)) ? '0 : PORT_W'(admit_port + 1'b1);
    end

`ifndef SYNTHESIS
    always_ff @(posedge clock) begin
        for (int p = 0; p < NUM_T1; p++) begin
            if (drop_req[p] && !reset)
                $warning("venus: dropped unsupported REQ opcode 0x%02h port %0d",
                         reqf[p].opcode, p);
        end
    end
`endif

    // M5 write-hold blocking (PROTOCOL §7.3): a slot's AXI read stalls while
    // another slot holds the same line with its write not yet landed — either a
    // write-kind slot on its own line, or a §T15 victim sub-flow whose victim
    // writeback (s_vline) has not yet landed (victim demotions make memory
    // stale until the WB completes).
    always_comb begin
        for (int i = 0; i < PAR; i++) begin
            sl_wb_blk[i] = 1'b0;
            for (int j = 0; j < PAR; j++) begin
                if ((i != j) && sl_busy[j] && (sl_line[j] == sl_line[i])
                    && sl_wr_hold[j])
                    sl_wb_blk[i] = 1'b1;
                if ((i != j) && sl_vwb_hold[j] && (sl_vline[j] == sl_line[i]))
                    sl_wb_blk[i] = 1'b1;
            end
        end
    end

    // §T15 victim-arm hazard matrix (Direction 1): slot i may not arm a victim
    // whose line is another slot's in-flight main line — that flow is mid-
    // transaction on the same directory entry, and snooping it off the
    // lookup-time image would desync the directory (the blocker's own commit
    // is still pending). The arm retries the LOOKUP instead (see venus_mshr).
    // Deadlock-free: the blocker's main line is directory-resident (a missed
    // line can't be proposed as victim), so it hit or already allocated,
    // never needs a victim itself, and always completes independently; the
    // retry then proceeds with a fresh proposal. (Victim-vs-victim needs no
    // check: the compare-and-remove makes that case benign.)
    always_comb begin
        for (int i = 0; i < PAR; i++) begin
            sl_vic_blk[i] = 1'b0;
            for (int j = 0; j < PAR; j++) begin
                if ((i != j) && sl_busy[j] && (sl_line[j] == sl_vline[i]))
                    sl_vic_blk[i] = 1'b1;
            end
        end
    end

    // ==================================================================
    // Backend arbitration (each backend is single-outstanding so at most one
    // command is in flight and its response returns to the recorded owner).
    // Slot selection is fixed lowest-index priority (AGE_MATRIX=0, default)
    // or transaction-age oldest-first (AGE_MATRIX=1) per the generate
    // branches at each site; the *_sel/*_sel_v contract below is identical
    // either way, so all downstream muxing/grant/steering logic is untouched.
    // ==================================================================

    // Age matrix (AGE_MATRIX=1): one shared birth-ordered registry over the
    // PAR tracker slots (slot birth = admission alloc_stb, one-hot, <=1 per
    // cycle) granting each backend/emission class its OLDEST requester.
    // Slice packing: [dir, sf, rd[0..AXI_N-1], wr[0..AXI_N-1], snp, dnrsp,
    // dndat]. A slot's first request comes at earliest the cycle after its
    // birth edge, so the registered matrix always covers it; idle slots
    // request nothing, so their stale age bits never participate.
    localparam int AGE_NSEL  = 5 + 2*AXI_N;
    localparam int AGE_DIR   = 0;
    localparam int AGE_SF    = 1;
    localparam int AGE_RD0   = 2;          // rd[m] slice = AGE_RD0 + m
    localparam int AGE_WR0   = 2 + AXI_N;  // wr[m] slice = AGE_WR0 + m
    localparam int AGE_SNP   = 2 + 2*AXI_N;
    localparam int AGE_DNRSP = 3 + 2*AXI_N;
    localparam int AGE_DNDAT = 4 + 2*AXI_N;

    logic [AGE_NSEL*PAR-1:0] age_req;
    logic [AGE_NSEL*PAR-1:0] age_gnt;

    generate
    if (AGE_MATRIX != 0) begin : g_age_matrix
        // Request packing; the AXI classes are masked by the slot's channel
        // select exactly like the fixed-priority loops below.
        always_comb begin
            age_req = '0;
            for (int i = 0; i < PAR; i++) begin
                age_req[AGE_DIR*PAR + i]   = sl_dir_req[i];
                age_req[AGE_SF*PAR + i]    = sl_sf_req[i];
                for (int m = 0; m < AXI_N; m++) begin
                    age_req[(AGE_RD0+m)*PAR + i] = sl_axi_rd_req[i]
                                                   && (sl_axi_sel[i] == AXI_SEL_W'(m));
                    age_req[(AGE_WR0+m)*PAR + i] = sl_axi_wr_req[i]
                                                   && (sl_axi_sel[i] == AXI_SEL_W'(m));
                end
                age_req[AGE_SNP*PAR + i]   = sl_snp_req[i];
                age_req[AGE_DNRSP*PAR + i] = sl_dnrsp_req[i];
                age_req[AGE_DNDAT*PAR + i] = sl_dndat_req[i];
            end
        end

        venus_age_matrix #(
            .N   (PAR),
            .NSEL(AGE_NSEL)
        ) u_age (
            .clock   (clock),
            .reset   (reset),
            .alloc_v (|alloc_stb),
            .alloc_oh(alloc_stb),
            .req     (age_req),
            .gnt     (age_gnt)
        );
    end else begin : g_age_none
        assign age_req = '0;
        assign age_gnt = '0;
    end
    endgenerate

    logic               dir_sel_v;
    logic [SLOT_W-1:0]  dir_sel;
    logic [SLOT_W-1:0]  dir_own;
    logic               dir_cmd_ready;

    // Directory command select (AGE_MATRIX=0: fixed priority, verbatim;
    // AGE_MATRIX=1: age-matrix one-hot grant folded back to an index — the
    // grant is one-hot, so the lowest-set-bit loop shape is just an encoder).
    generate
    if (AGE_MATRIX == 0) begin : g_age_off_dir
        always_comb begin
            dir_sel_v = 1'b0;
            dir_sel   = '0;
            for (int i = PAR-1; i >= 0; i--) begin
                if (sl_dir_req[i]) begin
                    dir_sel_v = 1'b1;
                    dir_sel   = SLOT_W'(i);
                end
            end
        end
    end else begin : g_age_on_dir
        always_comb begin
            dir_sel_v = |age_gnt[AGE_DIR*PAR +: PAR];
            dir_sel   = '0;
            for (int i = PAR-1; i >= 0; i--) begin
                if (age_gnt[AGE_DIR*PAR + i])
                    dir_sel = SLOT_W'(i);
            end
        end
    end
    endgenerate

    venus_directory #(
        .NUM_T1       (NUM_T1),
        .SETS         (DIR_SETS),
        .WAYS         (DIR_WAYS),
        .TAGALIAS_W   (TAGALIAS_W),
        .STORAGE      (STORAGE_DIR),
        .SRAM_LAT     (SRAM_LAT),
        .SRAM_INTERVAL(SRAM_INTERVAL),
        .SRAM_PERIOD  (SRAM_PERIOD),
        .SRAM_SLOT    (SRAM_SLOT)
    ) u_dir (
        .clock        (clock),
        .reset        (reset),
        .cmd_valid    (dir_sel_v),
        .cmd_ready    (dir_cmd_ready),
        .cmd_op       (sl_dir_op[dir_sel]),
        .cmd_line     (sl_dir_line[dir_sel]),
        .cmd_port     (sl_dir_port[dir_sel]),
        .cmd_rm_line  (sl_dir_rmline[dir_sel]),
        .cmd_state    (sl_dir_state[dir_sel]),
        .cmd_sharers  (sl_dir_sharers[dir_sel]),
        .cmd_owner    (sl_dir_owner[dir_sel]),
        .cmd_aliases  (sl_dir_aliases[dir_sel]),
        .cmd_men      (sl_dir_men[dir_sel]),
        .cmd_mstate   (sl_dir_mstate[dir_sel]),
        .cmd_msharers (sl_dir_msharers[dir_sel]),
        .cmd_mowner   (sl_dir_mowner[dir_sel]),
        .rsp_valid    (d_rsp_valid),
        .rsp_ready    (1'b1),
        .rsp_hit      (d_rsp_hit),
        .rsp_state    (d_rsp_state),
        .rsp_sharers  (d_rsp_sharers),
        .rsp_owner    (d_rsp_owner),
        .rsp_aliases  (d_rsp_aliases),
        .rsp_way      (),
        .rsp_full     (d_rsp_full),
        .rsp_vlive    (d_rsp_vlive),
        .rsp_vstate   (d_rsp_vstate),
        .rsp_vsharers (d_rsp_vsharers),
        .rsp_vowner   (d_rsp_vowner),
        .rsp_vway     (),
        .rsp_vline    (d_rsp_vline)
    );

    always_comb begin
        for (int i = 0; i < PAR; i++) begin
            sl_dir_gnt[i]       = dir_sel_v && (dir_sel == SLOT_W'(i)) && dir_cmd_ready;
            sl_dir_rsp_valid[i] = d_rsp_valid && (dir_own == SLOT_W'(i));
        end
    end

    always_ff @(posedge clock or posedge reset) begin
        if (reset)
            dir_own <= '0;
        else if (dir_sel_v && dir_cmd_ready)
            dir_own <= dir_sel;
    end

    logic               sf_sel_v;
    logic [SLOT_W-1:0]  sf_sel;
    logic [SLOT_W-1:0]  sf_own;
    logic               sf_cmd_ready;

    generate
    if (AGE_MATRIX == 0) begin : g_age_off_sf
        always_comb begin
            sf_sel_v = 1'b0;
            sf_sel   = '0;
            for (int i = PAR-1; i >= 0; i--) begin
                if (sl_sf_req[i]) begin
                    sf_sel_v = 1'b1;
                    sf_sel   = SLOT_W'(i);
                end
            end
        end
    end else begin : g_age_on_sf
        always_comb begin
            sf_sel_v = |age_gnt[AGE_SF*PAR +: PAR];
            sf_sel   = '0;
            for (int i = PAR-1; i >= 0; i--) begin
                if (age_gnt[AGE_SF*PAR + i])
                    sf_sel = SLOT_W'(i);
            end
        end
    end
    endgenerate

    venus_snoop_filter #(
        .NUM_T1       (NUM_T1),
        .SETS         (SF_SETS),
        .WAYS         (SF_WAYS),
        .STORAGE      (STORAGE_SF),
        .SRAM_LAT     (SRAM_LAT),
        .SRAM_INTERVAL(SRAM_INTERVAL),
        .SRAM_PERIOD  (SRAM_PERIOD),
        .SRAM_SLOT    (SRAM_SLOT)
    ) u_sf (
        .clock        (clock),
        .reset        (reset),
        .cmd_valid    (sf_sel_v),
        .cmd_ready    (sf_cmd_ready),
        .cmd_op       (sl_sf_op[sf_sel]),
        .cmd_line     (sl_sf_line[sf_sel]),
        .cmd_presence (sl_sf_presence[sf_sel]),
        .cmd_port     (sl_sf_port[sf_sel]),
        .rsp_valid    (f_rsp_valid),
        .rsp_ready    (1'b1),
        .rsp_hit      (),
        .rsp_presence (f_rsp_presence),
        .rsp_maybe    (f_rsp_maybe)
    );

    always_comb begin
        for (int i = 0; i < PAR; i++) begin
            sl_sf_gnt[i]       = sf_sel_v && (sf_sel == SLOT_W'(i)) && sf_cmd_ready;
            sl_sf_rsp_valid[i] = f_rsp_valid && (sf_own == SLOT_W'(i));
        end
    end

    always_ff @(posedge clock or posedge reset) begin
        if (reset)
            sf_own <= '0;
        else if (sf_sel_v && sf_cmd_ready)
            sf_own <= sf_sel;
    end

    // ==================================================================
    // AXI masters (one per channel) and their arbitration. Read and write
    // channels are arbitrated independently; the master itself prefers the
    // read when both land while idle.
    // ==================================================================
    logic               rd_sel_v [AXI_N];
    logic [SLOT_W-1:0]  rd_sel [AXI_N];
    logic [SLOT_W-1:0]  rd_own [AXI_N];
    logic               wr_sel_v [AXI_N];
    logic [SLOT_W-1:0]  wr_sel [AXI_N];
    logic [SLOT_W-1:0]  wr_own [AXI_N];
    logic               m_rd_gnt [AXI_N];
    logic               m_rd_done [AXI_N];
    logic [LINE_BITS-1:0] m_rd_data [AXI_N];
    logic               m_wr_gnt [AXI_N];
    logic               m_wr_done [AXI_N];

    generate
    if (AGE_MATRIX == 0) begin : g_age_off_axi
        always_comb begin
            for (int m = 0; m < AXI_N; m++) begin
                rd_sel_v[m] = 1'b0;
                rd_sel[m]   = '0;
                wr_sel_v[m] = 1'b0;
                wr_sel[m]   = '0;
                for (int i = PAR-1; i >= 0; i--) begin
                    if (sl_axi_rd_req[i] && (sl_axi_sel[i] == AXI_SEL_W'(m))) begin
                        rd_sel_v[m] = 1'b1;
                        rd_sel[m]   = SLOT_W'(i);
                    end
                    if (sl_axi_wr_req[i] && (sl_axi_sel[i] == AXI_SEL_W'(m))) begin
                        wr_sel_v[m] = 1'b1;
                        wr_sel[m]   = SLOT_W'(i);
                    end
                end
            end
        end
    end else begin : g_age_on_axi
        always_comb begin
            for (int m = 0; m < AXI_N; m++) begin
                rd_sel_v[m] = |age_gnt[(AGE_RD0+m)*PAR +: PAR];
                rd_sel[m]   = '0;
                wr_sel_v[m] = |age_gnt[(AGE_WR0+m)*PAR +: PAR];
                wr_sel[m]   = '0;
                for (int i = PAR-1; i >= 0; i--) begin
                    if (age_gnt[(AGE_RD0+m)*PAR + i])
                        rd_sel[m] = SLOT_W'(i);
                    if (age_gnt[(AGE_WR0+m)*PAR + i])
                        wr_sel[m] = SLOT_W'(i);
                end
            end
        end
    end
    endgenerate

    always_comb begin
        for (int i = 0; i < PAR; i++) begin
            sl_axi_rd_gnt[i]  = 1'b0;
            sl_axi_rd_done[i] = 1'b0;
            sl_axi_rd_data[i] = '0;
            sl_axi_wr_gnt[i]  = 1'b0;
            sl_axi_wr_done[i] = 1'b0;
            for (int m = 0; m < AXI_N; m++) begin
                if (sl_axi_sel[i] == AXI_SEL_W'(m)) begin
                    sl_axi_rd_gnt[i]  = rd_sel_v[m] && (rd_sel[m] == SLOT_W'(i))
                                        && m_rd_gnt[m];
                    sl_axi_rd_done[i] = m_rd_done[m] && (rd_own[m] == SLOT_W'(i));
                    sl_axi_rd_data[i] = m_rd_data[m];
                    sl_axi_wr_gnt[i]  = wr_sel_v[m] && (wr_sel[m] == SLOT_W'(i))
                                        && m_wr_gnt[m];
                    sl_axi_wr_done[i] = m_wr_done[m] && (wr_own[m] == SLOT_W'(i));
                end
            end
        end
    end

    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            for (int m = 0; m < AXI_N; m++) begin
                rd_own[m] <= '0;
                wr_own[m] <= '0;
            end
        end else begin
            for (int m = 0; m < AXI_N; m++) begin
                if (rd_sel_v[m] && m_rd_gnt[m])
                    rd_own[m] <= rd_sel[m];
                if (wr_sel_v[m] && m_wr_gnt[m])
                    wr_own[m] <= wr_sel[m];
            end
        end
    end

    // ---- AXI masters (channels >= NUM_AXI tied off) ----
    genvar gm;
    generate
        for (gm = 0; gm < AXI_MAX; gm++) begin : g_axim
            if (gm < AXI_N) begin : g_live
                venus_axi_master #(
                    .ADDR_W(AXI_ADDR_W),
                    .DATA_W(AXI_DATA_W),
                    .ID_W  (AXI_ID_W)
                ) u_axi (
                    .clock(clock), .reset(reset),
                    .rd_req  (rd_sel_v[gm]),
                    .rd_gnt  (m_rd_gnt[gm]),
                    .rd_addr (sl_axi_rd_addr[rd_sel[gm]]),
                    .rd_id   ({AXI_ID_W{1'b0}}),
                    .rd_len  (sl_axi_rd_len[rd_sel[gm]]),
                    .rd_start(sl_axi_rd_start[rd_sel[gm]]),
                    .rd_done (m_rd_done[gm]),
                    .rd_data (m_rd_data[gm]),
                    .wr_req  (wr_sel_v[gm]),
                    .wr_gnt  (m_wr_gnt[gm]),
                    .wr_addr (sl_axi_wr_addr[wr_sel[gm]]),
                    .wr_id   ({AXI_ID_W{1'b0}}),
                    .wr_len  (sl_axi_wr_len[wr_sel[gm]]),
                    .wr_start(sl_axi_wr_start[wr_sel[gm]]),
                    .wr_data (sl_axi_wr_data[wr_sel[gm]]),
                    .wr_strb (sl_axi_wr_strb[wr_sel[gm]]),
                    .wr_done (m_wr_done[gm]),
                    .axi_m_awvalid(ax_awvalid[gm]), .axi_m_awready(ax_awready[gm]),
                    .axi_m_awaddr(ax_awaddr[gm]), .axi_m_awid(ax_awid[gm]),
                    .axi_m_awlen(ax_awlen[gm]), .axi_m_awsize(ax_awsize[gm]), .axi_m_awburst(ax_awburst[gm]),
                    .axi_m_wvalid(ax_wvalid[gm]), .axi_m_wready(ax_wready[gm]),
                    .axi_m_wdata(ax_wdata[gm]), .axi_m_wstrb(ax_wstrb[gm]), .axi_m_wlast(ax_wlast[gm]),
                    .axi_m_bvalid(ax_bvalid[gm]), .axi_m_bready(ax_bready[gm]),
                    .axi_m_bid(ax_bid[gm]), .axi_m_bresp(ax_bresp[gm]),
                    .axi_m_arvalid(ax_arvalid[gm]), .axi_m_arready(ax_arready[gm]),
                    .axi_m_araddr(ax_araddr[gm]), .axi_m_arid(ax_arid[gm]),
                    .axi_m_arlen(ax_arlen[gm]), .axi_m_arsize(ax_arsize[gm]), .axi_m_arburst(ax_arburst[gm]),
                    .axi_m_rvalid(ax_rvalid[gm]), .axi_m_rready(ax_rready[gm]),
                    .axi_m_rdata(ax_rdata[gm]), .axi_m_rid(ax_rid[gm]),
                    .axi_m_rresp(ax_rresp[gm]), .axi_m_rlast(ax_rlast[gm])
                );
            end else begin : g_tie
                assign ax_awvalid[gm] = 1'b0;
                assign ax_awaddr[gm]  = '0;
                assign ax_awid[gm]    = '0;
                assign ax_awlen[gm]   = '0;
                assign ax_awsize[gm]  = '0;
                assign ax_awburst[gm] = '0;
                assign ax_wvalid[gm]  = 1'b0;
                assign ax_wdata[gm]   = '0;
                assign ax_wstrb[gm]   = '0;
                assign ax_wlast[gm]   = 1'b0;
                assign ax_bready[gm]  = 1'b0;
                assign ax_arvalid[gm] = 1'b0;
                assign ax_araddr[gm]  = '0;
                assign ax_arid[gm]    = '0;
                assign ax_arlen[gm]   = '0;
                assign ax_arsize[gm]  = '0;
                assign ax_arburst[gm] = '0;
                assign ax_rready[gm]  = 1'b0;
            end
        end
    endgenerate

    // ==================================================================
    // Emission muxes: <=1 SNP, <=1 DnRSP, <=1 DnDAT per cycle (slot select is
    // fixed priority with AGE_MATRIX=0, age-oldest with AGE_MATRIX=1).
    // ID-allocation priority per port: SNP > DnRSP > DnDAT.
    // ==================================================================
    logic               snp_sel_v;
    logic [SLOT_W-1:0]  snp_sel;
    logic [PORT_W-1:0]  snp_tp;
    logic               snp_go;
    logic [NUM_T1-1:0]  snp_serves;

    logic               rs_sel_v;
    logic [SLOT_W-1:0]  rs_sel;
    logic [PORT_W-1:0]  rs_rp;
    logic               rs_needs;
    logic               rs_go;
    logic [NUM_T1-1:0]  rs_serves;

    logic               dt_sel_v;
    logic [SLOT_W-1:0]  dt_sel;
    logic [PORT_W-1:0]  dt_dp;
    logic               dt_needs;
    logic               dt_go;

    logic [SLOT_W-1:0]  sn_slot [NUM_T1][ID_SPACE];
    logic [SLOT_W-1:0]  db_slot [NUM_T1][ID_SPACE];

    // Stage A: selection (slot-register derived only). AGE_MATRIX=0: fixed
    // lowest-slot-index priority, verbatim; AGE_MATRIX=1: age-matrix oldest
    // grant encoded back into the same signals.
    generate
    if (AGE_MATRIX == 0) begin : g_age_off_emit
        always_comb begin
            snp_sel_v = 1'b0;
            snp_sel   = '0;
            rs_sel_v  = 1'b0;
            rs_sel    = '0;
            dt_sel_v  = 1'b0;
            dt_sel    = '0;
            for (int i = PAR-1; i >= 0; i--) begin
                if (sl_snp_req[i]) begin
                    snp_sel_v = 1'b1;
                    snp_sel   = SLOT_W'(i);
                end
                if (sl_dnrsp_req[i]) begin
                    rs_sel_v = 1'b1;
                    rs_sel   = SLOT_W'(i);
                end
                if (sl_dndat_req[i]) begin
                    dt_sel_v = 1'b1;
                    dt_sel   = SLOT_W'(i);
                end
            end
        end
    end else begin : g_age_on_emit
        always_comb begin
            snp_sel_v = |age_gnt[AGE_SNP*PAR +: PAR];
            rs_sel_v  = |age_gnt[AGE_DNRSP*PAR +: PAR];
            dt_sel_v  = |age_gnt[AGE_DNDAT*PAR +: PAR];
            snp_sel   = '0;
            rs_sel    = '0;
            dt_sel    = '0;
            for (int i = PAR-1; i >= 0; i--) begin
                if (age_gnt[AGE_SNP*PAR + i])
                    snp_sel = SLOT_W'(i);
                if (age_gnt[AGE_DNRSP*PAR + i])
                    rs_sel = SLOT_W'(i);
                if (age_gnt[AGE_DNDAT*PAR + i])
                    dt_sel = SLOT_W'(i);
            end
        end
    end
    endgenerate

    always_comb begin
        snp_tp   = sl_snp_port[snp_sel];
        rs_rp    = sl_port[rs_sel];
        dt_dp    = sl_port[dt_sel];
        rs_needs = sl_dbid_req[rs_sel];
        dt_needs = sl_dbid_req[dt_sel];
    end

    // Stage B: SNP grant (top allocator priority) and its port claim.
    always_comb begin
        snp_go = snp_sel_v && id_avail[snp_tp] && snpi_r[snp_tp];
        for (int p = 0; p < NUM_T1; p++)
            snp_serves[p] = snp_go && (snp_tp == PORT_W'(p));
    end

    // Stage C: DnRSP grant (loses the allocator only to the SNP class).
    always_comb begin
        rs_go = rs_sel_v && dnrspi_r[rs_rp]
                && (!rs_needs || (id_avail[rs_rp] && !snp_serves[rs_rp]));
        for (int p = 0; p < NUM_T1; p++)
            rs_serves[p] = rs_go && rs_needs && (rs_rp == PORT_W'(p));
    end

    // Stage D: DnDAT grant (loses the allocator to SNP and DnRSP classes),
    // plus the per-slot grant strobes and allocated-ID plumbing.
    always_comb begin
        dt_go = dt_sel_v && dndati_r[dt_dp]
                && (!dt_needs || (id_avail[dt_dp] && !snp_serves[dt_dp]
                                  && !rs_serves[dt_dp]));

        for (int p = 0; p < NUM_T1; p++)
            id_alloc_req[p] = snp_serves[p] || rs_serves[p]
                              || (dt_go && dt_needs && (dt_dp == PORT_W'(p)));

        for (int i = 0; i < PAR; i++) begin
            sl_snp_gnt[i]    = snp_go && (snp_sel == SLOT_W'(i));
            sl_snp_gnt_id[i] = id_alloc_id[sl_snp_port[i]];
            sl_dnrsp_gnt[i]  = rs_go && (rs_sel == SLOT_W'(i));
            sl_dndat_gnt[i]  = dt_go && (dt_sel == SLOT_W'(i));
            sl_dbid_gnt_id[i] = (rs_sel_v && (rs_sel == SLOT_W'(i))) ? id_alloc_id[rs_rp]
                              : ((dt_sel_v && (dt_sel == SLOT_W'(i))) ? id_alloc_id[dt_dp]
                                                                      : '0);
        end
    end

    // Stage E: FIFO pushes (read the flits, which depend on stage D's IDs).
    always_comb begin
        for (int p = 0; p < NUM_T1; p++) begin
            snpi_v[p]   = snp_go && (snp_tp == PORT_W'(p));
            snpi[p]     = sl_snp_bits[snp_sel];
            dnrspi_v[p] = rs_go && (rs_rp == PORT_W'(p));
            dnrspi[p]   = sl_dnrsp_bits[rs_sel];
            dndati_v[p] = dt_go && (dt_dp == PORT_W'(p));
            dndati[p]   = sl_dndat_bits[dt_sel];
        end
    end

    // Steering tables: (port, ID) -> owning slot.
    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            for (int p = 0; p < NUM_T1; p++) begin
                for (int i = 0; i < ID_SPACE; i++) begin
                    sn_slot[p][i] <= '0;
                    db_slot[p][i] <= '0;
                end
            end
        end else begin
            if (snp_go)
                sn_slot[snp_tp][id_alloc_id[snp_tp]] <= snp_sel;
            if (rs_go && rs_needs)
                db_slot[rs_rp][id_alloc_id[rs_rp]] <= rs_sel;
            if (dt_go && dt_needs)
                db_slot[dt_dp][id_alloc_id[dt_dp]] <= dt_sel;
        end
    end

    // ==================================================================
    // Intake routing: UpRSP/UpDAT heads are decoded per port and steered to
    // slots via the tables. Every UpRSP flit completes exactly one
    // home-allocated ID (CompAck -> DBID, SnpResp -> snoop TxnID); every
    // terminal UpDAT beat completes one (write-data last beat / SnpRespData
    // last beat).
    // ==================================================================
    always_comb begin
        for (int i = 0; i < PAR; i++) begin
            sl_snpans_valid[i] = '0;
            sl_snpdat_valid[i] = '0;
            sl_snpdat_last[i]  = '0;
            sl_snpdat_beat[i]  = '0;
            sl_wrdat_valid[i]  = 1'b0;
            sl_wrdat_beat[i]   = 1'b0;
            sl_wrdat_be[i]     = '0;
            sl_wrdat_data[i]   = '0;
            sl_ack[i]          = 1'b0;
            for (int p = 0; p < NUM_T1; p++) begin
                sl_snpans_id[i][p]    = '0;
                sl_snpans_resp[i][p]  = '0;
                sl_snpdat_id[i][p]    = '0;
                sl_snpdat_resp[i][p]  = '0;
                sl_snpdat_data[i][p]  = '0;
            end
        end

        for (int p = 0; p < NUM_T1; p++) begin
            logic [SLOT_W-1:0] s;

            uprspf_r[p]    = uprspf_v[p];
            updatf_r[p]    = updatf_v[p];
            id_free0_req[p] = uprspf_v[p];
            id_free0_id[p]  = uprspf[p].txnid;
            id_free1_req[p] = updatf_v[p]
                              && ((updatf[p].opcode == UPDAT_SNPRESPDATA)
                                  ? (updatf[p].dataid == 1'b1)
                                  : sl_wr_last[db_slot[p][updatf[p].txnid]]);
            id_free1_id[p]  = updatf[p].txnid;
            pop_valid[p]    = txrsp_fire[p];
            pop_id[p]       = txrsp_txnid[p];

            s = '0;
            if (uprspf_v[p]) begin
                if (uprspf[p].opcode == UPRSP_SNPRESP) begin
                    s = sn_slot[p][uprspf[p].txnid];
                    sl_snpans_valid[s][p] = 1'b1;
                    sl_snpans_id[s][p]    = uprspf[p].txnid;
                    sl_snpans_resp[s][p]  = uprspf[p].resp;
                end else begin
                    s = db_slot[p][uprspf[p].txnid];
                    sl_ack[s] = 1'b1;
                end
            end

            if (updatf_v[p]) begin
                if (updatf[p].opcode == UPDAT_SNPRESPDATA) begin
                    s = sn_slot[p][updatf[p].txnid];
                    sl_snpdat_valid[s][p] = 1'b1;
                    sl_snpdat_id[s][p]    = updatf[p].txnid;
                    sl_snpdat_last[s][p]  = (updatf[p].dataid == 1'b1);
                    sl_snpdat_resp[s][p]  = updatf[p].resp;
                    sl_snpdat_beat[s][p]  = updatf[p].dataid;
                    sl_snpdat_data[s][p]  = updatf[p].data;
                end else begin
                    s = db_slot[p][updatf[p].txnid];
                    sl_wrdat_valid[s] = 1'b1;
                    sl_wrdat_beat[s]  = updatf[p].dataid;
                    sl_wrdat_be[s]    = updatf[p].be;
                    sl_wrdat_data[s]  = updatf[p].data;
                end
            end
        end
    end

    // ==================================================================
    // Simulation assertions (protocol invariants as executable docs).
    // ==================================================================
`ifndef SYNTHESIS
    always_ff @(posedge clock) begin
        if (!reset) begin
            // At most one REQ-kind slot per line (PROTOCOL §7.3). EVT slots
            // may share a line with an in-flight REQ slot.
            for (int i = 0; i < PAR; i++) begin
                for (int j = i + 1; j < PAR; j++) begin
                    if (sl_busy[i] && sl_busy[j] && (sl_line[i] == sl_line[j])
                        && !sl_kind_evt[i] && !sl_kind_evt[j])
                        $error("venus: REQ slots %0d and %0d share line 0x%0h",
                               i, j, sl_line[i]);
                end
            end
            // Steering sanity: every routed flit finds a busy slot.
            for (int p = 0; p < NUM_T1; p++) begin
                if (uprspf_v[p] && (uprspf[p].opcode == UPRSP_COMPACK)
                    && !sl_busy[db_slot[p][uprspf[p].txnid]])
                    $error("venus: CompAck to idle slot (port %0d id %0d)",
                           p, uprspf[p].txnid);
                if (updatf_v[p] && (updatf[p].opcode == 2'b01))
                    $error("venus: reserved UpDAT opcode on port %0d", p);
            end
            // Ports >= NUM_T1 are tied off; traffic there is a harness/
            // configuration error (e.g. stimulator driving a port beyond the
            // live count) and would otherwise hang silently.
            for (int p = NUM_T1; p < T1_MAX; p++) begin
                if (pin_rxreq_v[p] || pin_rxevt_v[p] || pin_rxrsp_v[p] || pin_rxdat_v[p])
                    $error("venus: traffic on disabled Type-1 port %0d (>= NUM_T1=%0d)",
                           p, NUM_T1);
            end
        end
    end

    // TagAlias pin contract: the pins are present on every port set, but with
    // TAGALIAS_W=0 the field is inert and must be tied 0 (checked on valid
    // REQs only; idle-cycle payload is don't-care).
    if (TAGALIAS_W == 0) begin : g_ta_off
        `define VENUS_TA_OFF_CHK(P) \
            always_ff @(posedge clock) begin \
                if (!reset && cchi_t1p``P``_rxreq_valid \
                    && (cchi_t1p``P``_rxreq_bits_TagAlias !== 8'h00)) \
                    $error("venus: TagAlias nonzero on port %0d with TAGALIAS_W=0", P); \
            end
        `VENUS_TA_OFF_CHK(0)
        `VENUS_TA_OFF_CHK(1)
        `VENUS_TA_OFF_CHK(2)
        `VENUS_TA_OFF_CHK(3)
        `VENUS_TA_OFF_CHK(4)
        `VENUS_TA_OFF_CHK(5)
        `VENUS_TA_OFF_CHK(6)
        `VENUS_TA_OFF_CHK(7)
        `undef VENUS_TA_OFF_CHK
    end
`endif

endmodule

`default_nettype wire
