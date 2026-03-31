// chi_pkg.sv
// SystemVerilog package for CHIron-based UVM verification.
//
// Defines CHI Issue E constants, opcodes, flit field widths, and flit
// structs for a default configuration:
//   NodeID  = 7-bit
//   Address = 48-bit
//   Data    = 256-bit
//   RSVDC   = 4-bit (REQ and DAT)
//
// Adjust the DATA_W / ADDR_W / NODEID_W parameters to match your DUT.

`ifndef CHI_PKG_SV
`define CHI_PKG_SV

package chi_pkg;

  // ---------------------------------------------------------------------------
  // Protocol configuration (match your FlitConfiguration<> template instantiation)
  // ---------------------------------------------------------------------------
  parameter int unsigned NODEID_W   = 7;   // 7..11
  parameter int unsigned ADDR_W     = 48;  // 44..52
  parameter int unsigned DATA_W     = 256; // 128/256/512
  parameter int unsigned RSVDC_W    = 4;   // 0/4/8/12/16/24/32
  parameter int unsigned TXNID_W    = 12;
  parameter int unsigned BE_W       = DATA_W / 8;
  parameter int unsigned SNP_ADDR_W = ADDR_W - 3;

  // CLog channel constants — match clogdpi_b.svh / clogdpi_t.svh
  parameter int unsigned CLOG_ISSUE_E     = 3;

  parameter int unsigned CLOG_CH_TXREQ    = 0;
  parameter int unsigned CLOG_CH_TXRSP    = 1;
  parameter int unsigned CLOG_CH_TXDAT    = 2;
  parameter int unsigned CLOG_CH_TXSNP    = 3;
  parameter int unsigned CLOG_CH_RXREQ    = 4;
  parameter int unsigned CLOG_CH_RXRSP    = 5;
  parameter int unsigned CLOG_CH_RXDAT    = 6;
  parameter int unsigned CLOG_CH_RXSNP    = 7;

  // ---------------------------------------------------------------------------
  // REQ channel opcodes  (CHI Issue E, §B-356)
  // ---------------------------------------------------------------------------
  typedef logic [6:0] req_opcode_t;

  parameter req_opcode_t REQ_ReqLCrdReturn          = 7'h00;
  parameter req_opcode_t REQ_ReadShared             = 7'h01;
  parameter req_opcode_t REQ_ReadClean              = 7'h02;
  parameter req_opcode_t REQ_ReadOnce               = 7'h03;
  parameter req_opcode_t REQ_ReadNoSnp              = 7'h04;
  parameter req_opcode_t REQ_PCrdReturn             = 7'h05;
  parameter req_opcode_t REQ_ReadUnique             = 7'h07;
  parameter req_opcode_t REQ_CleanShared            = 7'h08;
  parameter req_opcode_t REQ_CleanInvalid           = 7'h09;
  parameter req_opcode_t REQ_MakeInvalid            = 7'h0A;
  parameter req_opcode_t REQ_CleanUnique            = 7'h0B;
  parameter req_opcode_t REQ_MakeUnique             = 7'h0C;
  parameter req_opcode_t REQ_Evict                  = 7'h0D;
  parameter req_opcode_t REQ_DVMOp                  = 7'h14;
  parameter req_opcode_t REQ_WriteEvictFull         = 7'h15;
  parameter req_opcode_t REQ_WriteCleanFull         = 7'h17;
  parameter req_opcode_t REQ_WriteUniquePtl         = 7'h18;
  parameter req_opcode_t REQ_WriteUniqueFull        = 7'h19;
  parameter req_opcode_t REQ_WriteBackPtl           = 7'h1A;
  parameter req_opcode_t REQ_WriteBackFull          = 7'h1B;
  parameter req_opcode_t REQ_WriteNoSnpPtl          = 7'h1C;
  parameter req_opcode_t REQ_WriteNoSnpFull         = 7'h1D;
  parameter req_opcode_t REQ_WriteUniqueFullStash   = 7'h20;
  parameter req_opcode_t REQ_WriteUniquePtlStash    = 7'h21;
  parameter req_opcode_t REQ_StashOnceShared        = 7'h22;
  parameter req_opcode_t REQ_StashOnceUnique        = 7'h23;
  parameter req_opcode_t REQ_ReadOnceCleanInvalid   = 7'h24;
  parameter req_opcode_t REQ_ReadOnceMakeInvalid    = 7'h25;
  parameter req_opcode_t REQ_ReadNotSharedDirty     = 7'h26;
  parameter req_opcode_t REQ_CleanSharedPersist     = 7'h27;
  parameter req_opcode_t REQ_AtomicSwap             = 7'h38;
  parameter req_opcode_t REQ_AtomicCompare          = 7'h39;
  parameter req_opcode_t REQ_PrefetchTgt            = 7'h3A;

  // ---------------------------------------------------------------------------
  // RSP channel opcodes
  // ---------------------------------------------------------------------------
  typedef logic [4:0] rsp_opcode_t;

  parameter rsp_opcode_t RSP_RespLCrdReturn  = 5'h00;
  parameter rsp_opcode_t RSP_SnpResp         = 5'h01;
  parameter rsp_opcode_t RSP_CompAck         = 5'h02;
  parameter rsp_opcode_t RSP_RetryAck        = 5'h03;
  parameter rsp_opcode_t RSP_Comp            = 5'h04;
  parameter rsp_opcode_t RSP_CompDBIDResp    = 5'h05;
  parameter rsp_opcode_t RSP_DBIDResp        = 5'h06;
  parameter rsp_opcode_t RSP_PCrdGrant       = 5'h07;
  parameter rsp_opcode_t RSP_ReadReceipt     = 5'h08;
  parameter rsp_opcode_t RSP_SnpRespFwded   = 5'h09;
  parameter rsp_opcode_t RSP_TagMatch        = 5'h0A;
  parameter rsp_opcode_t RSP_RespSepData     = 5'h0B;
  parameter rsp_opcode_t RSP_Persist         = 5'h0C;
  parameter rsp_opcode_t RSP_CompPersist     = 5'h0D;
  parameter rsp_opcode_t RSP_DBIDRespOrd     = 5'h0E;
  parameter rsp_opcode_t RSP_StashDone       = 5'h10;
  parameter rsp_opcode_t RSP_CompStashDone   = 5'h11;
  parameter rsp_opcode_t RSP_CompCMO         = 5'h14;

  // ---------------------------------------------------------------------------
  // DAT channel opcodes
  // ---------------------------------------------------------------------------
  typedef logic [3:0] dat_opcode_t;

  parameter dat_opcode_t DAT_DataLCrdReturn  = 4'h0;
  parameter dat_opcode_t DAT_SnpRespData     = 4'h1;
  parameter dat_opcode_t DAT_CopyBackWrData  = 4'h2;
  parameter dat_opcode_t DAT_NonCopyBackWrData = 4'h3;
  parameter dat_opcode_t DAT_CompData        = 4'h4;
  parameter dat_opcode_t DAT_SnpRespDataPtl  = 4'h5;
  parameter dat_opcode_t DAT_SnpRespDataFwded = 4'h6;
  parameter dat_opcode_t DAT_WriteDataCancel = 4'h7;
  parameter dat_opcode_t DAT_DataSepResp     = 4'hB;
  parameter dat_opcode_t DAT_NCBWrDataCompAck = 4'hC;

  // ---------------------------------------------------------------------------
  // SNP channel opcodes
  // ---------------------------------------------------------------------------
  typedef logic [5:0] snp_opcode_t;

  parameter snp_opcode_t SNP_SnpLCrdReturn         = 6'h00;
  parameter snp_opcode_t SNP_SnpShared             = 6'h01;
  parameter snp_opcode_t SNP_SnpClean              = 6'h02;
  parameter snp_opcode_t SNP_SnpOnce               = 6'h03;
  parameter snp_opcode_t SNP_SnpNotSharedDirty     = 6'h04;
  parameter snp_opcode_t SNP_SnpUniqueStash        = 6'h05;
  parameter snp_opcode_t SNP_SnpMakeInvalidStash   = 6'h06;
  parameter snp_opcode_t SNP_SnpUnique             = 6'h07;
  parameter snp_opcode_t SNP_SnpCleanShared        = 6'h08;
  parameter snp_opcode_t SNP_SnpCleanInvalid       = 6'h09;
  parameter snp_opcode_t SNP_SnpMakeInvalid        = 6'h0A;
  parameter snp_opcode_t SNP_SnpStashUnique        = 6'h0B;
  parameter snp_opcode_t SNP_SnpStashShared        = 6'h0C;
  parameter snp_opcode_t SNP_SnpDVMOp              = 6'h0D;
  parameter snp_opcode_t SNP_SnpPreferUnique       = 6'h1F;
  parameter snp_opcode_t SNP_SnpPreferUniqueFwd    = 6'h20;
  parameter snp_opcode_t SNP_SnpOnceFwd            = 6'h21;
  parameter snp_opcode_t SNP_SnpSharedFwd          = 6'h22;
  parameter snp_opcode_t SNP_SnpNotSharedDirtyFwd  = 6'h24;
  parameter snp_opcode_t SNP_SnpUniqueFwd          = 6'h26;

  // ---------------------------------------------------------------------------
  // Flit structs
  // All widths use the package parameters above.
  // ---------------------------------------------------------------------------

  typedef struct packed {
    logic [3:0]          qos;
    logic [NODEID_W-1:0] tgtid;
    logic [NODEID_W-1:0] srcid;
    logic [TXNID_W-1:0]  txnid;
    logic [NODEID_W-1:0] return_nid;
    logic                stash_nid_valid;
    logic [TXNID_W-1:0]  return_txnid;
    logic [NODEID_W-1:0] stash_nid;
    logic                endian;
    logic                deep;
    req_opcode_t         opcode;
    logic [2:0]          size;
    logic [ADDR_W-1:0]   addr;
    logic                ns;
    logic                likely_shared;
    logic                allow_retry;
    logic [1:0]          order;
    logic [3:0]          pcrd_type;
    logic [3:0]          mem_attr;
    logic                snp_attr;
    logic                do_dwt;
    logic [4:0]          lpid;
    logic                excl;
    logic                snoop_me;
    logic                cah;
    logic [RSVDC_W-1:0]  rsvdc;
    logic                trace_tag;
    logic [10:0]         mpam;
  } req_flit_t;

  typedef struct packed {
    logic [3:0]          qos;
    logic [NODEID_W-1:0] tgtid;
    logic [NODEID_W-1:0] srcid;
    logic [TXNID_W-1:0]  txnid;
    rsp_opcode_t         opcode;
    logic [1:0]          resp_err;
    logic [2:0]          resp;
    logic [2:0]          fwd_state;
    logic                data_pull;
    logic [2:0]          cbusy;
    logic [TXNID_W-1:0]  dbid;
    logic                corrected_err;
    logic [7:0]          pgroup_id;
    logic [3:0]          pcrd_type;
    logic                trace_tag;
    logic [10:0]         mpam;
  } rsp_flit_t;

  typedef struct packed {
    logic [3:0]          qos;
    logic [NODEID_W-1:0] tgtid;
    logic [NODEID_W-1:0] srcid;
    logic [TXNID_W-1:0]  txnid;
    logic [NODEID_W-1:0] home_nid;
    dat_opcode_t         opcode;
    logic [1:0]          resp_err;
    logic [2:0]          resp;
    logic [2:0]          fwd_state;
    logic                data_pull;
    logic [2:0]          cbusy;
    logic [TXNID_W-1:0]  dbid;
    logic [1:0]          ccid;
    logic [1:0]          data_id;
    logic [1:0]          tag_op;
    logic [1:0]          tu;
    logic                trace_tag;
    logic [RSVDC_W-1:0]  rsvdc;
    logic [BE_W-1:0]     be;
    logic [DATA_W-1:0]   data;
  } dat_flit_t;

  typedef struct packed {
    logic [3:0]          qos;
    logic [NODEID_W-1:0] srcid;
    logic [TXNID_W-1:0]  txnid;
    logic [NODEID_W-1:0] fwd_nid;
    logic [TXNID_W-1:0]  fwd_txnid;
    snp_opcode_t         opcode;
    logic [SNP_ADDR_W-1:0] addr;
    logic                ns;
    logic                do_not_go_to_sd;
    logic                vm_id_ext;
    logic                ret_to_src;
    logic                trace_tag;
    logic [10:0]         mpam;
  } snp_flit_t;

  // ---------------------------------------------------------------------------
  // Cache state encoding (CHI §B-237)
  // ---------------------------------------------------------------------------
  typedef enum logic [2:0] {
    CACHE_I   = 3'b000,  // Invalid
    CACHE_SC  = 3'b001,  // Shared Clean
    CACHE_UC  = 3'b010,  // Unique Clean
    CACHE_UD  = 3'b011,  // Unique Dirty
    CACHE_SD  = 3'b101,  // Shared Dirty
    CACHE_UCE = 3'b110,  // Unique Clean Empty
    CACHE_UDP = 3'b111   // Unique Dirty Partial
  } cache_state_e;

  // ---------------------------------------------------------------------------
  // Transaction type classification helpers
  // ---------------------------------------------------------------------------
  function automatic logic is_read_req(input req_opcode_t op);
    return op inside {REQ_ReadShared, REQ_ReadClean, REQ_ReadOnce,
                      REQ_ReadNoSnp, REQ_ReadUnique, REQ_ReadNotSharedDirty,
                      REQ_ReadOnceCleanInvalid, REQ_ReadOnceMakeInvalid};
  endfunction

  function automatic logic is_write_req(input req_opcode_t op);
    return op inside {REQ_WriteUniqueFull, REQ_WriteUniquePtl,
                      REQ_WriteNoSnpFull, REQ_WriteNoSnpPtl,
                      REQ_WriteBackFull, REQ_WriteBackPtl,
                      REQ_WriteCleanFull, REQ_WriteEvictFull,
                      REQ_WriteUniqueFullStash, REQ_WriteUniquePtlStash};
  endfunction

  function automatic logic is_dataless_req(input req_opcode_t op);
    return op inside {REQ_CleanShared, REQ_CleanInvalid, REQ_MakeInvalid,
                      REQ_CleanUnique, REQ_MakeUnique, REQ_Evict,
                      REQ_CleanSharedPersist};
  endfunction

  function automatic logic is_atomic_req(input req_opcode_t op);
    return op inside {REQ_AtomicSwap, REQ_AtomicCompare} ||
           ((op & 7'h38) == 7'h28) || // AtomicStore range
           ((op & 7'h38) == 7'h30);   // AtomicLoad range
  endfunction

endpackage : chi_pkg

`endif // CHI_PKG_SV
