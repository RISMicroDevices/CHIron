`ifndef VENUS_PKG_SV
`define VENUS_PKG_SV

// ============================================================================
// venus_pkg — constants, encodings and helper types for the Venus CCHI home.
//
// Contents:
//   * width/geometry constants for the Venus wire configuration (PROTOCOL.md §2)
//   * CCHI opcode encodings per channel (PROTOCOL.md §4) with one-line flow refs
//   * Resp encodings (PROTOCOL.md §3)
//   * directory state encoding and the transaction-kind decode used by the
//     tracker slots (xact_kind_e, req_kind/evt_kind)
//   * packed flit structs mirroring the CCHI pin contract
//   * small helper functions (beat counts, address mapping, resp predicates)
//
// Every opcode constant cites its TRANSACTIONS.md section (§T<n>).
// ============================================================================

package venus_pkg;

    // ------------------------------------------------------------------ geometry
    localparam int T1_MAX           = 8;    // architecture ceiling of Type-1 ports
    localparam int AXI_MAX          = 4;    // architecture ceiling of AXI ports

    localparam int TXNID_W          = 7;    // REQ/EVT TxnID width (PROTOCOL §2)
    localparam int DBID_W           = 7;    // DBID + SNP TxnID width (PROTOCOL §6, §8)
    localparam int UP_NID_W         = 5;    // upstream node ID width
    localparam int DN_NID_W         = 5;    // downstream (home) node ID width
    localparam int WAY_W            = 4;    // upstream way-hint width (ignored)
    localparam int DATA_W           = 256;  // data beat width
    localparam int BE_W             = DATA_W / 8;
    localparam int ADDR_W           = 48;
    localparam int SNP_ADDR_W       = 45;   // line address << 3 (PROTOCOL §2.3)
    localparam int LINE_BYTES       = 64;
    localparam int LINE_BITS        = LINE_BYTES * 8;
    localparam int BEATS_PER_LINE   = LINE_BITS / DATA_W;      // 2
    localparam int LINE_ADDR_LSB    = 6;    // log2(LINE_BYTES)
    localparam int LINE_W           = ADDR_W - LINE_ADDR_LSB;  // 42
    localparam int ID_SPACE         = 1 << TXNID_W;            // per-port ID pool

    localparam int STOR_REG         = 0;    // venus_sram_box: flop array
    localparam int STOR_SRAM        = 1;    // venus_sram_box: behavioral SRAM

    // ------------------------------------------------------------------ REQ opcodes (6b)
    // Upstream requests; flows in TRANSACTIONS.md §T1..§T12.
    localparam logic [5:0] REQ_STASH_SHARED       = 6'h00;  // hint, no-op + CompStash?   §T11
    localparam logic [5:0] REQ_STASH_UNIQUE       = 6'h01;  // hint, no-op + CompStash?   §T11
    localparam logic [5:0] REQ_READ_NOSNP         = 6'h02;  // AXI rd -> CompData xN       §T1
    localparam logic [5:0] REQ_READ_ONCE          = 6'h03;  // peek -> CompData xN         §T2
    localparam logic [5:0] REQ_READ_SHARED        = 6'h04;  // alloc S/promote U -> CD x2  §T3
    localparam logic [5:0] REQ_WRITE_NOSNP_PTL    = 6'h08;  // DBIDResp -> data -> AXI     §T6
    localparam logic [5:0] REQ_WRITE_NOSNP_FULL   = 6'h09;  // DBIDResp -> data -> AXI     §T6
    localparam logic [5:0] REQ_WRITE_UNIQUE_PTL   = 6'h0A;  // snoop+merge -> data -> AXI  §T7
    localparam logic [5:0] REQ_WRITE_UNIQUE_FULL  = 6'h0B;  // snoop -> data -> AXI        §T7
    localparam logic [5:0] REQ_CLEAN_SHARED       = 6'h0C;  // clean dirty owner -> CompCMO §T8
    localparam logic [5:0] REQ_CLEAN_INVALID      = 6'h0D;  // inval all, wb -> CompCMO    §T9
    localparam logic [5:0] REQ_MAKE_INVALID       = 6'h0E;  // inval all, drop -> CompCMO  §T10
    localparam logic [5:0] REQ_READ_UNIQUE        = 6'h10;  // alloc U -> CD x2 / Comp     §T4
    localparam logic [5:0] REQ_MAKE_UNIQUE        = 6'h12;  // inval peers -> Comp         §T5
    localparam logic [5:0] REQ_EVICT_BACK         = 6'h1E;  // reserved (rejected)         §T12
    localparam logic [5:0] REQ_EVICT_CLEAN        = 6'h1F;  // reserved (rejected)         §T12
    // 6'h20..6'h31: AtomicLoad/AtomicStore/AtomicSwap/AtomicCompare — rejected §T12.

    // Atomic predicate: 0x20..0x31 (0b100xxx = AtomicLoad, 0b101xxx = AtomicStore,
    // 0x30 swap, 0x31 compare). Per cchi_protocol_encoding.hpp, matching the
    // CompactCHI spec (channel_definitions.md).
    function automatic logic req_is_atomic(input logic [5:0] op);
        return (op[5:3] == 3'b100) || (op[5:3] == 3'b101) || (op == 6'h30) || (op == 6'h31);
    endfunction

    // ------------------------------------------------------------------ EVT opcodes (1b)
    localparam logic       EVT_EVICT            = 1'b0;   // clean eviction   §T13
    localparam logic       EVT_WRITE_BACK_FULL  = 1'b1;   // dirty eviction   §T14

    // ------------------------------------------------------------------ SNP opcodes (2b)
    // Semantics (upstream final state / data return) per PROTOCOL.md §4.3.
    localparam logic [1:0] SNP_MAKE_INVALID     = 2'b00;  // -> I, never returns data
    localparam logic [1:0] SNP_TO_INVALID       = 2'b01;  // -> I, UD returns I_PD data
    localparam logic [1:0] SNP_TO_SHARED        = 2'b10;  // -> S, UD returns SC_PD data
    localparam logic [1:0] SNP_TO_CLEAN         = 2'b11;  // -> UC, UD returns UC_PD data

    // ------------------------------------------------------------------ UpRSP opcodes (1b)
    localparam logic       UPRSP_COMPACK        = 1'b0;   // TxnID carries DBID
    localparam logic       UPRSP_SNPRESP        = 1'b1;   // TxnID carries SNP TxnID

    // ------------------------------------------------------------------ UpDAT opcodes (2b)
    localparam logic [1:0] UPDAT_NCB_WR         = 2'b00;  // NonCopyBackWrData (writes)   §T6/§T7
    localparam logic [1:0] UPDAT_CB_WR          = 2'b10;  // CopyBackWrData (WB, I_PD)    §T14
    localparam logic [1:0] UPDAT_SNPRESPDATA    = 2'b11;  // SnpRespData (PD from UD)     §16.1

    // ------------------------------------------------------------------ DnRSP opcodes (3b)
    localparam logic [2:0] DNRSP_COMP_STASH     = 3'b000; // stash completion (ExpCompStash=1) §T11
    localparam logic [2:0] DNRSP_COMP           = 3'b001; // completion w/o data
    localparam logic [2:0] DNRSP_DBIDRESP       = 3'b010; // data-buffer grant (writes)  §T6/§T7
    localparam logic [2:0] DNRSP_COMPDBID       = 3'b011; // Comp + DBIDResp combined    §T14
    localparam logic [2:0] DNRSP_COMPCMO        = 3'b100; // CMO completion              §T8..§T10

    // ------------------------------------------------------------------ DnDAT opcodes (1b)
    localparam logic       DNDAT_COMPDATA       = 1'b0;

    // ------------------------------------------------------------------ Resp encodings (3b, PROTOCOL §3)
    localparam logic [2:0] RESP_I               = 3'b000;
    localparam logic [2:0] RESP_SC              = 3'b001;
    localparam logic [2:0] RESP_UC              = 3'b010;
    localparam logic [2:0] RESP_I_PD            = 3'b100;
    localparam logic [2:0] RESP_SC_PD           = 3'b101;
    localparam logic [2:0] RESP_UC_PD           = 3'b110;

    // ------------------------------------------------------------------ directory state (PROTOCOL §3)
    // I = absent, S = shared-clean (sharers vector valid), U = unique at owner
    // (silently dirty possible: the home learns dirty via PD snoop data or WB).
    typedef enum logic [1:0] {
        DIR_I = 2'b00,
        DIR_S = 2'b01,
        DIR_U = 2'b10
    } dir_state_e;

    // ------------------------------------------------------------------ transaction kinds
    // Internal decode of REQ/EVT opcodes consumed by the tracker slots and the
    // directory. Ptl/Full sub-variants stay distinguishable via the saved opcode.
    typedef enum logic [3:0] {
        XK_STASH         = 4'd0,   // StashShared/StashUnique            §T11
        XK_READ_NOSNP    = 4'd1,   // ReadNoSnp                          §T1
        XK_READ_ONCE     = 4'd2,   // ReadOnce                           §T2
        XK_READ_SHARED   = 4'd3,   // ReadShared                         §T3
        XK_READ_UNIQUE   = 4'd4,   // ReadUnique                         §T4
        XK_MAKE_UNIQUE   = 4'd5,   // MakeUnique                         §T5
        XK_WRITE_NOSNP   = 4'd6,   // WriteNoSnpPtl/Full                 §T6
        XK_WRITE_UNIQUE  = 4'd7,   // WriteUniquePtl/Full                §T7
        XK_CLEAN_SHARED  = 4'd8,   // CleanShared                        §T8
        XK_CLEAN_INVALID = 4'd9,   // CleanInvalid                       §T9
        XK_MAKE_INVALID  = 4'd10,  // MakeInvalid                        §T10
        XK_EVICT         = 4'd11,  // EVT Evict                          §T13
        XK_WRITE_BACK    = 4'd12,  // EVT WriteBackFull                  §T14
        XK_DROP          = 4'd13   // rejected (reserved/atomic/unknown) §T12
    } xact_kind_e;

    // REQ opcode -> transaction kind. Unsupported/reserved/atomic map to XK_DROP.
    function automatic xact_kind_e req_kind(input logic [5:0] op);
        unique case (op)
            REQ_STASH_SHARED,
            REQ_STASH_UNIQUE:      return XK_STASH;
            REQ_READ_NOSNP:        return XK_READ_NOSNP;
            REQ_READ_ONCE:         return XK_READ_ONCE;
            REQ_READ_SHARED:       return XK_READ_SHARED;
            REQ_READ_UNIQUE:       return XK_READ_UNIQUE;
            REQ_MAKE_UNIQUE:       return XK_MAKE_UNIQUE;
            REQ_WRITE_NOSNP_PTL,
            REQ_WRITE_NOSNP_FULL:  return XK_WRITE_NOSNP;
            REQ_WRITE_UNIQUE_PTL,
            REQ_WRITE_UNIQUE_FULL: return XK_WRITE_UNIQUE;
            REQ_CLEAN_SHARED:      return XK_CLEAN_SHARED;
            REQ_CLEAN_INVALID:     return XK_CLEAN_INVALID;
            REQ_MAKE_INVALID:      return XK_MAKE_INVALID;
            default:               return XK_DROP;
        endcase
    endfunction

    // EVT opcode -> transaction kind (1-bit opcode space is fully covered).
    function automatic xact_kind_e evt_kind(input logic op);
        return op == EVT_WRITE_BACK_FULL ? XK_WRITE_BACK : XK_EVICT;
    endfunction

    // Kind predicates used by the tracker:
    //  - allocates: directory allocation on miss (victim flow applies)
    //  - self_snoop: the requester itself is included in the snoop mask (PROTOCOL §7.2)
    //  - is_write: consumes UpDAT write data keyed by DBID
    function automatic logic kind_allocates(input xact_kind_e k);
        return (k == XK_READ_SHARED) || (k == XK_READ_UNIQUE) || (k == XK_MAKE_UNIQUE);
    endfunction
    function automatic logic kind_self_snoop(input xact_kind_e k);
        return (k == XK_WRITE_UNIQUE) || (k == XK_CLEAN_SHARED)
            || (k == XK_CLEAN_INVALID) || (k == XK_MAKE_INVALID);
    endfunction
    function automatic logic kind_is_write(input xact_kind_e k);
        return (k == XK_WRITE_NOSNP) || (k == XK_WRITE_UNIQUE) || (k == XK_WRITE_BACK);
    endfunction

    // ------------------------------------------------------------------ helpers
    // Beat count for a 256-bit data path from REQ.Size (log2 bytes):
    // Size <= 5 (<=32B) -> 1 beat; Size 6 (64B) -> 2 beats. Size 7 is reserved
    // and degrades to 2 (a reserved encoding never reaches a slot).
    function automatic logic [1:0] size_beats(input logic [2:0] size);
        return (size >= 3'd6) ? 2'd2 : 2'd1;
    endfunction

    // AXI burst length (AxLEN) for a transaction: beats - 1.
    function automatic logic [7:0] size_axlen(input logic [2:0] size);
        return (size >= 3'd6) ? 8'd1 : 8'd0;
    endfunction

    function automatic logic resp_is_pd(input logic [2:0] resp);
        return resp[2];
    endfunction

    function automatic logic resp_not_i(input logic [2:0] resp);
        return (resp[1:0] != 2'b00);
    endfunction

    function automatic logic [LINE_W-1:0] addr_to_line(input logic [ADDR_W-1:0] addr);
        return addr[ADDR_W-1:LINE_ADDR_LSB];
    endfunction

    function automatic logic [ADDR_W-1:0] line_to_addr(input logic [LINE_W-1:0] line);
        return {line, {LINE_ADDR_LSB{1'b0}}};
    endfunction

    function automatic logic [SNP_ADDR_W-1:0] line_to_snp_addr(input logic [LINE_W-1:0] line);
        return {line, 3'b000};
    endfunction

    function automatic int unsigned clog2_min1(input int unsigned v);
        return (v <= 1) ? 1 : $clog2(v);
    endfunction

    // ------------------------------------------------------------------ flit structs
    // Packed structs mirroring the CCHI pin contract (PROTOCOL.md §2). Field order
    // here is the MSB->LSB packing order used inside FIFOs; the pin mapping macros in
    // venus.sv translate field-by-field, so packing order is not part of the contract.

    typedef struct packed {
        logic [TXNID_W-1:0]     txnid;
        logic [UP_NID_W-1:0]    srcid;
        logic [DN_NID_W-1:0]    tgtid;
        logic                   opcode;
        logic [ADDR_W-1:0]      addr;
        logic                   ns;
        logic                   memattr;
        logic                   wayvalid;
        logic [WAY_W-1:0]       way;
        logic                   tracetag;
    } evt_flit_t;

    typedef struct packed {
        logic [TXNID_W-1:0]     txnid;
        logic [UP_NID_W-1:0]    srcid;
        logic [DN_NID_W-1:0]    tgtid;
        logic [5:0]             opcode;
        logic [2:0]             size;
        logic [ADDR_W-1:0]      addr;
        logic [7:0]             tagalias;   // VIPT alias (§2.2); inert when TAGALIAS_W=0
        logic                   ns;
        logic [1:0]             order;
        logic [3:0]             memattr;
        logic                   excl;
        logic                   exp_comp_data;  // wire-shared with ExpCompStash (§2.2)
        logic                   tracetag;
        logic                   wayvalid;
        logic [WAY_W-1:0]       way;
    } req_flit_t;

    typedef struct packed {
        logic [TXNID_W-1:0]     txnid;
        logic [DN_NID_W-1:0]    srcid;
        logic [UP_NID_W-1:0]    tgtid;
        logic [1:0]             opcode;
        logic [SNP_ADDR_W-1:0]  addr;
        logic                   ns;
        logic                   tracetag;
    } snp_flit_t;

    typedef struct packed {
        logic [TXNID_W-1:0]     txnid;
        logic [UP_NID_W-1:0]    srcid;
        logic [DN_NID_W-1:0]    tgtid;
        logic                   opcode;
        logic [1:0]             resperr;
        logic [2:0]             resp;
        logic                   tracetag;
    } uprsp_flit_t;

    typedef struct packed {
        logic [TXNID_W-1:0]     txnid;
        logic [DN_NID_W-1:0]    srcid;
        logic [UP_NID_W-1:0]    tgtid;
        logic [DBID_W-1:0]      dbid;
        logic [2:0]             opcode;
        logic [1:0]             resperr;
        logic [2:0]             resp;
        logic [2:0]             cbusy;
        logic                   wayvalid;
        logic [WAY_W-1:0]       way;
        logic                   tracetag;
    } dnrsp_flit_t;

    typedef struct packed {
        logic [TXNID_W-1:0]     txnid;
        logic [UP_NID_W-1:0]    srcid;
        logic [DN_NID_W-1:0]    tgtid;
        logic [1:0]             opcode;
        logic [1:0]             resperr;
        logic [2:0]             resp;
        logic                   dataid;
        logic [DATA_W-1:0]      data;
        logic [BE_W-1:0]        be;
        logic                   tracetag;
    } updat_flit_t;

    typedef struct packed {
        logic [TXNID_W-1:0]     txnid;
        logic [DN_NID_W-1:0]    srcid;
        logic [UP_NID_W-1:0]    tgtid;
        logic [DBID_W-1:0]      dbid;
        logic                   opcode;
        logic [1:0]             resperr;
        logic [2:0]             resp;
        logic [4:0]             datasource;
        logic [2:0]             cbusy;
        logic                   dataid;
        logic [DATA_W-1:0]      data;
        logic                   wayvalid;
        logic [WAY_W-1:0]       way;
        logic                   tracetag;
    } dndat_flit_t;

    localparam int EVT_W    = $bits(evt_flit_t);
    localparam int REQ_W    = $bits(req_flit_t);
    localparam int SNP_W    = $bits(snp_flit_t);
    localparam int UPRSP_W  = $bits(uprsp_flit_t);
    localparam int DNRSP_W  = $bits(dnrsp_flit_t);
    localparam int UPDAT_W  = $bits(updat_flit_t);
    localparam int DNDAT_W  = $bits(dndat_flit_t);

endpackage

`endif
