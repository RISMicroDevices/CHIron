`default_nettype none

import venus_pkg::*;

// ============================================================================
// venus_mshr — one Venus transaction tracker slot.
//
// A slot tracks a single home transaction from admission to retirement. Slots
// are the oceanus-TSHR-inspired unit of concurrency (ARCHITECTURE.md §3): the
// top level guarantees at most one busy slot per cache line (same-address CAM),
// so inside a slot all directory/snoop-filter/AXI actions for its line appear
// atomic. Shared backends (directory, snoop filter, AXI masters) and the
// downstream emission muxes live in venus.sv; the slot drives request lines and
// consumes routed responses.
//
// Control style: a coarse phase enum (PH_*, per-kind walk mirroring
// TRANSACTIONS.md §T1..§T15) plus fine-grained collection bits (snoop-answer
// bitmap, write-beat bitmap, early-CompAck latch).
//
// Backend handshakes (single-outstanding, arbitrated in venus.sv). A command
// request is held until its grant; exactly one response follows:
//   dir_req && dir_gnt -> dir_rsp_valid        (directory LOOKUP/GRANT/REMOVE)
//   sf_req  && sf_gnt  -> sf_rsp_valid         (SF LOOKUP/UPDATE/REMOVE)
//   axi_rd_req && axi_rd_gnt -> axi_rd_done    (read fill, rd_data valid)
//   axi_wr_req && axi_wr_gnt -> axi_wr_done    (write drained)
// Emission handshakes (global per-cycle muxes in venus.sv):
//   snp_req  && snp_gnt  : SNP flit pushed; snp_gnt_id = allocated snoop TxnID.
//   dnrsp_req/dndat_req && *_gnt : downstream flit pushed. When dbid_req is
//       asserted the grant allocates a DBID, returned on dbid_gnt_id.
// Intake (routed by venus.sv via its steering tables):
//   snpans_* : UpRSP/SnpResp for one of this slot's snoop TxnIDs.
//   snpdat_* : UpDAT/SnpRespData beats (PD merge; frees the snoop ID at last).
//   wrdat_*  : UpDAT write data for this slot's DBID (wr_last frees it).
//   ack_valid: CompAck for this slot's DBID (latched into w_ack, frees DBID).
//   pop_*    : per-port DnRSP wire-fire broadcast for the Evict retire rule.
// ============================================================================

module venus_mshr #(
    parameter int NUM_T1        = 4,
    parameter int NUM_AXI       = 1,
    parameter int NODE_ID       = 16,
    parameter int SF_ENABLE     = 1,
    parameter int SF_ALLOW_OVER = 1,
    parameter int SF_BROADCAST  = 0,
    parameter int TAGALIAS_W    = 0,   // TagAlias width in use (0 = off .. 8)
    parameter int AXI_ID_W      = 4,
    localparam int PORT_W       = clog2_min1(NUM_T1),
    localparam int AXI_SEL_W    = clog2_min1(NUM_AXI),
    localparam int TAGAW        = (TAGALIAS_W > 0) ? TAGALIAS_W : 1
) (
    input  wire         clock,
    input  wire         reset,

    // ---- occupancy / same-address CAM / admission ---------------------------
    output logic        busy,          // slot holds a transaction
    output logic [LINE_W-1:0] cam_line,// line of the held transaction
    input  wire         alloc,         // 1-cycle admit pulse (top checked CAM)
    input  wire xact_kind_e  alloc_kind,
    input  wire [5:0]   alloc_opcode,  // raw REQ/EVT opcode (Ptl/Full, union bit)
    input  wire [PORT_W-1:0] alloc_port,
    input  wire [TXNID_W-1:0] alloc_txnid,
    input  wire [LINE_W-1:0] alloc_line,
    input  wire [2:0]   alloc_size,
    input  wire         alloc_addr5,   // Addr[5]: sub-line beat select
    input  wire         alloc_expcd,   // ExpCompData / ExpCompStash union bit
    input  wire [7:0]   alloc_alias,   // REQ TagAlias (0 for EVTs; TAGALIAS_W=0 tied)
    input  wire [AXI_SEL_W-1:0] alloc_axi,  // AXI port select (venus_axi_map)

    // ---- steering outputs for venus.sv tables --------------------------------
    output logic [PORT_W-1:0] o_port,  // requester port of the held transaction
    output logic [AXI_SEL_W-1:0] o_axi_sel,  // AXI port select of the held transaction
    output logic        o_kind_write,  // held transaction consumes write data
    output logic        o_kind_evt,    // held transaction is an EVT (shares lines)
    output logic        o_wr_hold,     // write-kind slot, AXI write not yet done
    output logic [LINE_W-1:0] o_vline, // victim line of an in-flight §T15 sub-flow
    output logic        o_vwb_hold,    // victim demotions started, WB not yet landed
    output logic        o_vic_act,     // victim sub-flow armed/active (blocks admissions)

    // ---- M5 same-line write-back hazard --------------------------------------
    input  wire         wb_blk,        // another slot holds this line pre-write
    input  wire         vic_blk,       // §T15: candidate victim line is another
                                       // slot's in-flight main line (arm retry)

    // ---- directory backend --------------------------------------------------
    output logic        dir_req,
    input  wire         dir_gnt,
    output logic [1:0]  dir_op,
    output logic [LINE_W-1:0] dir_line,
    output logic [PORT_W-1:0] dir_port,
    output logic        dir_rmline,
    output logic [1:0]  dir_state,
    output logic [NUM_T1-1:0] dir_sharers,
    output logic [PORT_W-1:0] dir_owner,
    output logic [NUM_T1*TAGAW-1:0] dir_aliases,  // GRANT image: per-port TagAlias
    output logic        dir_men,       // victim REMOVE: match enable + image
    output logic [1:0]  dir_mstate,
    output logic [NUM_T1-1:0] dir_msharers,
    output logic [PORT_W-1:0] dir_mowner,
    input  wire         dir_rsp_valid,
    input  wire         dir_rsp_hit,
    input  wire [1:0]   dir_rsp_state,
    input  wire [NUM_T1-1:0] dir_rsp_sharers,
    input  wire [PORT_W-1:0] dir_rsp_owner,
    input  wire [NUM_T1*TAGAW-1:0] dir_rsp_aliases,  // LOOKUP hit alias image
    input  wire         dir_rsp_full,
    input  wire         dir_rsp_vlive,
    input  wire [1:0]   dir_rsp_vstate,
    input  wire [NUM_T1-1:0] dir_rsp_vsharers,
    input  wire [PORT_W-1:0] dir_rsp_vowner,
    input  wire [LINE_W-1:0] dir_rsp_vline,

    // ---- snoop-filter backend ------------------------------------------------
    output logic        sf_req,
    input  wire         sf_gnt,
    output logic [1:0]  sf_op,
    output logic [LINE_W-1:0] sf_line,
    output logic [NUM_T1-1:0] sf_presence,
    output logic [PORT_W-1:0] sf_port,
    input  wire         sf_rsp_valid,
    input  wire [NUM_T1-1:0] sf_rsp_presence,
    input  wire         sf_rsp_maybe,

    // ---- AXI backend (routed by o_axi_sel) -----------------------------------
    output logic        axi_rd_req,
    input  wire         axi_rd_gnt,
    output logic [ADDR_W-1:0] axi_rd_addr,
    output logic [7:0]  axi_rd_len,
    output logic        axi_rd_start,
    input  wire         axi_rd_done,
    input  wire [LINE_BITS-1:0] axi_rd_data,
    output logic        axi_wr_req,
    input  wire         axi_wr_gnt,
    output logic [ADDR_W-1:0] axi_wr_addr,
    output logic [7:0]  axi_wr_len,
    output logic        axi_wr_start,
    output logic [LINE_BITS-1:0] axi_wr_data,
    output logic [LINE_BYTES-1:0] axi_wr_strb,
    input  wire         axi_wr_done,

    // ---- SNP emission ---------------------------------------------------------
    output logic        snp_req,
    input  wire         snp_gnt,
    input  wire [DBID_W-1:0] snp_gnt_id,
    output logic [PORT_W-1:0] snp_port,
    output snp_flit_t   snp_bits,

    // ---- DnRSP / DnDAT emission ------------------------------------------------
    output logic        dnrsp_req,
    input  wire         dnrsp_gnt,
    output dnrsp_flit_t dnrsp_bits,
    output logic        dndat_req,
    input  wire         dndat_gnt,
    output dndat_flit_t dndat_bits,
    output logic        dbid_req,        // the emitted flit allocates a DBID
    input  wire [DBID_W-1:0] dbid_gnt_id,// DBID allocated with the grant

    // ---- intake: snoop responses (routed by SNP TxnID, per answering port) -----
    input  wire [NUM_T1-1:0] snpans_valid,
    input  wire [DBID_W-1:0] snpans_id [NUM_T1],
    input  wire [2:0]   snpans_resp [NUM_T1],
    input  wire [NUM_T1-1:0] snpdat_valid,
    input  wire [DBID_W-1:0] snpdat_id [NUM_T1],
    input  wire [NUM_T1-1:0] snpdat_last,
    input  wire [2:0]   snpdat_resp [NUM_T1],
    input  wire [NUM_T1-1:0] snpdat_beat,
    input  wire [DATA_W-1:0] snpdat_data [NUM_T1],

    // ---- intake: write data (routed by DBID) -----------------------------------
    input  wire         wrdat_valid,
    input  wire         wrdat_beat,
    input  wire [BE_W-1:0] wrdat_be,
    input  wire [DATA_W-1:0] wrdat_data,

    // ---- intake: CompAck (routed by DBID) --------------------------------------
    input  wire         ack_valid,

    // ---- intake: per-port DnRSP wire-fire broadcast (Evict retire) -------------
    input  wire [NUM_T1-1:0] pop_valid,
    input  wire [TXNID_W-1:0] pop_id [NUM_T1],

    // ---- ID bookkeeping for venus.sv ------------------------------------------
    output logic        wr_dbid,         // slot's DBID (if any) is write-keyed
    output logic [DBID_W-1:0] wr_dbid_id,
    output logic        wr_last          // last expected write beat consumed
);

    localparam logic [1:0] DIR_OP_LOOKUP = 2'd0;
    localparam logic [1:0] DIR_OP_GRANT  = 2'd1;
    localparam logic [1:0] DIR_OP_REMOVE = 2'd2;

    localparam logic [1:0] SF_OP_LOOKUP  = 2'd0;
    localparam logic [1:0] SF_OP_UPDATE  = 2'd1;
    localparam logic [1:0] SF_OP_REMOVE  = 2'd2;

    // §T15 victim-arm retry throttle: cycles to wait before re-issuing the
    // directory LOOKUP after a vic_blk block. The directory arbiter is fixed
    // lowest-slot-index priority, so an unthrottled retry holds dir_req
    // ~continuously and starves a higher-index directory-bound blocker slot
    // (priority-inversion livelock); the gap guarantees the blocker gets
    // through, while the retry bandwidth stays negligible.
    localparam int  VIC_RETRY_THROTTLE = 32;

    // ------------------------------------------------------------------ phases
    // Coarse per-kind walk (TRANSACTIONS.md §T1..§T15).
    typedef enum logic [4:0] {
        PH_IDLE    = 5'd0,   // slot free
        PH_DIR_LU  = 5'd1,   // directory LOOKUP issued/awaited
        PH_SF_LU   = 5'd2,   // snoop-filter LOOKUP issued/awaited
        PH_SNOOP   = 5'd3,   // main-line snoops: issue + collect
        PH_VIC_SNP = 5'd4,   // §T15 victim: snoop victim holders
        PH_VIC_WB  = 5'd5,   // §T15 victim: AXI writeback of victim PD data
        PH_VIC_RM  = 5'd6,   // §T15 victim: DIR REMOVE of the victim line
        PH_VIC_RSF = 5'd7,   // §T15 victim: SF UPDATE(∅) of the victim line
        PH_AXI_RD  = 5'd8,   // AXI read fill into s_data
        PH_AXI_WR  = 5'd9,   // AXI write of s_data under s_strb
        PH_DBID    = 5'd10,  // write flow: emit the DBID grant (DBIDResp/CompDBID)
        PH_RESP    = 5'd11,  // downstream responses (beats / Comp / CompCMO ...)
        PH_COMMIT  = 5'd12,  // directory GRANT/REMOVE (commit-once)
        PH_SF_UPD  = 5'd13,  // SF UPDATE/REMOVE (commit-once)
        PH_WAIT_ACK= 5'd14,  // awaiting CompAck (may have pre-arrived: w_ack)
        PH_WAIT_WR = 5'd15,  // awaiting write-data beats
        PH_WAIT_POP= 5'd16,  // Evict: awaiting the Comp wire fire
        PH_DONE    = 5'd17   // retire this cycle
    } phase_e;

    phase_e                 ph;

    // ------------------------------------------------------------------ context
    xact_kind_e             c_kind;
    logic [5:0]             c_opcode;
    logic [PORT_W-1:0]      c_port;
    logic [TXNID_W-1:0]     c_txnid;
    logic [LINE_W-1:0]      c_line;
    logic [2:0]             c_size;
    logic                   c_addr5;
    logic                   c_expcd;
    logic [7:0]             c_alias;     // request's TagAlias (latched at alloc)
    logic [AXI_SEL_W-1:0]   c_axi;

    // directory snapshot (PH_DIR_LU response)
    logic                   s_hit;
    logic [1:0]             s_state;
    logic [NUM_T1-1:0]      s_sharers;
    logic [PORT_W-1:0]      s_owner;
    logic [NUM_T1*TAGAW-1:0] s_aliases;  // per-port TagAlias image at LOOKUP
    logic                   s_full;
    logic                   s_vlive;
    logic [1:0]             s_vstate;
    logic [NUM_T1-1:0]      s_vsharers;
    logic [PORT_W-1:0]      s_vowner;
    logic [LINE_W-1:0]      s_vline;

    // snoop-filter snapshot (PH_SF_LU response)
    logic [NUM_T1-1:0]      s_sf_pres;
    logic                   s_sf_maybe;

    // line buffer + byte strobes (TRANSACTIONS.md conventions)
    logic [LINE_BITS-1:0]   s_data;
    logic [LINE_BYTES-1:0]  s_strb;
    logic                   s_gotpd;     // any PD data merged (full-line image)

    // DBID
    logic                   s_dbid_v;
    logic [DBID_W-1:0]      s_dbid;

    // snoop bookkeeping
    logic [NUM_T1-1:0]      s_snp_pend;  // targets not yet issued
    logic [NUM_T1-1:0]      s_snp_out;   // issued, answer not yet complete
    logic [DBID_W-1:0]      s_snp_id [NUM_T1];

    // write-data collection (bit per DataID)
    logic [1:0]             s_wrbeats;

    // CompData emission progress (bit per DataID)
    logic [1:0]             s_rbeats;

    // early-CompAck latch (PROTOCOL §5.3)
    logic                   w_ack;

    // write-completion flag for the M5 write-hold (§7.3): set once the slot's
    // AXI write has landed; o_wr_hold stays high until then.
    logic                   s_wrdone;

    // §T15 victim-arm retry throttle: counts down after a vic_blk block;
    // while nonzero, PH_DIR_LU holds dir_req low (see the request mux).
    logic [5:0]             s_vic_wait;

    // backend "command accepted, response pending" flags (request hygiene)
    logic                   sent_dir;
    logic                   sent_sf;
    logic                   sent_rd;
    logic                   sent_wr;

    assign busy         = (ph != PH_IDLE);
    assign cam_line     = c_line;
    assign o_port       = c_port;
    // AXI channel select: the main line's hash captured at admission, except
    // during PH_VIC_WB where the transfer targets the *victim* line — its
    // writeback must land on the channel s_vline hashes to, not the main
    // line's (§T15; latent wrong-channel WB under NUM_AXI > 1 otherwise).
    logic [AXI_SEL_W-1:0] v_axi_sel;
    venus_axi_map #(.NUM_AXI(NUM_AXI), .LINE_W(LINE_W)) u_vmap (
        .line    (s_vline),
        .axi_sel (v_axi_sel)
    );
    assign o_axi_sel    = (ph == PH_VIC_WB) ? v_axi_sel : c_axi;
    assign o_kind_write = kind_is_write(c_kind);
    assign o_kind_evt   = (c_kind == XK_EVICT) || (c_kind == XK_WRITE_BACK);
    assign o_wr_hold    = busy && kind_is_write(c_kind) && !s_wrdone;
    // M5 extension (§7.3): while the §T15 victim sub-flow is demoting the
    // victim's holders (VIC_SNP) or writing the victim line back (VIC_WB),
    // memory for s_vline may be stale — a concurrent same-line fill must wait
    // for the writeback to land. On the victim-arm decision cycle the fresh
    // *candidate* (dir_rsp_vline) is exported instead of the register, so the
    // top level's victim hazard matrices (vic_blk in, o_vic_act out) see the
    // line being decided on this very cycle. Otherwise o_vline = s_vline,
    // meaningful under o_vwb_hold / o_vic_act.
    assign o_vline      = ((ph == PH_DIR_LU) && dir_rsp_valid) ? dir_rsp_vline
                                                               : s_vline;
    assign o_vwb_hold   = busy && ((ph == PH_VIC_SNP) || (ph == PH_VIC_WB));

    // §T15 victim-arm decision firing this cycle (mirrors the PH_DIR_LU branch
    // below exactly). o_vic_act covers the victim phases AND, combinationally,
    // the arm-decision cycle — closing the same-cycle window between arm and
    // phase entry so a concurrent admission of the victim line is blocked
    // from the arm on (the victim entry still reads as a hit until PH_VIC_RM).
    logic vic_arm;
    assign vic_arm   = (ph == PH_DIR_LU) && dir_rsp_valid
                       && kind_allocates(c_kind) && !dir_rsp_hit && dir_rsp_full
                       && dir_rsp_vlive && !vic_blk;
    assign o_vic_act = vic_arm
                       || (ph == PH_VIC_SNP) || (ph == PH_VIC_WB)
                       || (ph == PH_VIC_RM)  || (ph == PH_VIC_RSF);

    // ------------------------------------------------------------------ derived
    // Present-set from the directory snapshot.
    logic [NUM_T1-1:0] dir_present;
    always_comb begin
        unique case (s_state)
            DIR_S:   dir_present = s_sharers;
            DIR_U:   dir_present = NUM_T1'(1) << s_owner;
            default: dir_present = '0;
        endcase
    end

    // Snoop-mask composition (PROTOCOL §7.1/§7.2): exact directory presence,
    // optionally widened by the snoop filter; requester excluded for
    // reads/MakeUnique, included for CMOs/WriteUnique*/victim flows.
    function automatic logic [NUM_T1-1:0] mask_of(
        input logic [NUM_T1-1:0] present,
        input logic [NUM_T1-1:0] sf_pres,
        input logic              sf_maybe,
        input xact_kind_e        k,
        input logic [PORT_W-1:0] port
    );
        logic [NUM_T1-1:0] base;
        base = present;
        if (SF_ENABLE != 0) begin
            if (SF_ALLOW_OVER != 0)
                base = base | sf_pres;
            if (SF_BROADCAST != 0 || sf_maybe)
                base = {NUM_T1{1'b1}};
        end
        return kind_self_snoop(k) ? base : (base & ~(NUM_T1'(1) << port));
    endfunction

    // Snoop opcode per kind (TRANSACTIONS.md snoop-derivation table).
    function automatic logic [1:0] snp_opcode(input xact_kind_e k, input logic [5:0] op);
        unique case (k)
            XK_READ_ONCE:     return SNP_TO_CLEAN;
            XK_READ_SHARED:   return SNP_TO_SHARED;
            XK_READ_UNIQUE:   return SNP_TO_INVALID;
            XK_MAKE_UNIQUE:   return SNP_MAKE_INVALID;
            XK_WRITE_UNIQUE:  return (op == REQ_WRITE_UNIQUE_PTL) ? SNP_TO_INVALID
                                                                  : SNP_MAKE_INVALID;
            XK_CLEAN_SHARED:  return SNP_TO_CLEAN;
            XK_CLEAN_INVALID: return SNP_TO_INVALID;
            XK_MAKE_INVALID:  return SNP_MAKE_INVALID;
            default:          return SNP_TO_INVALID;
        endcase
    endfunction

    // True when this kind snoops for the current main-line snapshot. An alias
    // mismatch forces a (targeted) self-snoop even when the plain directory
    // state would not require any snoop at all.
    logic snp_needed;
    always_comb begin
        unique case (c_kind)
            XK_READ_ONCE:     snp_needed = s_hit && (s_state == DIR_U) && (s_owner != c_port);
            XK_READ_SHARED:   snp_needed = (s_hit && (s_state == DIR_U) && (s_owner != c_port))
                                           || s_alias_inv;
            XK_READ_UNIQUE:   snp_needed = (s_hit && !((s_state == DIR_U) && (s_owner == c_port)))
                                           || s_alias_inv;
            XK_MAKE_UNIQUE:   snp_needed = s_hit;   // s_alias_inv implies s_hit
            XK_WRITE_UNIQUE:  snp_needed = s_hit;
            XK_CLEAN_SHARED:  snp_needed = s_hit && (s_state == DIR_U);
            XK_CLEAN_INVALID: snp_needed = s_hit;
            XK_MAKE_INVALID:  snp_needed = s_hit;
            default:          snp_needed = 1'b0;
        endcase
    end

    // True when the main-line directory snapshot records the requester itself
    // as a current holder of the line. A recorded holder's copy is current by
    // construction, because every demotion of a cached copy goes through this
    // home. ExpCompData is a HINT from the requester: only a recorded holder
    // is allowed to be answered without data; a requester that is not recorded
    // (e.g. its copy was snooped away earlier) must always get CompData.
    logic req_hit;
    always_comb begin
        unique case (s_state)
            DIR_U:     req_hit = s_hit && (s_owner == c_port);
            DIR_S:     req_hit = s_hit && s_sharers[c_port];
            default:   req_hit = 1'b0;
        endcase
    end

    // TagAlias back-invalidation (TRANSACTIONS.md §T3/§T4/§T5 alias sub-flow):
    // an allocating read (ReadShared/ReadUnique/MakeUnique) from a recorded
    // holder whose request alias differs from the alias recorded for it means
    // the requester still holds a stale-alias copy, which must be invalidated
    // before the grant. The check folds into the main snoop phase: the
    // requester bit is OR-ed into the snoop mask at the arming sites below
    // and its snoop opcode is overridden to SNP_TO_INVALID (see snp_bits).
    // s_alias_inv is snapshot-domain (valid once PH_DIR_LU has latched);
    // rsp_alias_inv is the same compare in the response domain, valid on the
    // PH_DIR_LU response cycle itself, for the no-SF arming path there.
    logic s_alias_inv;
    logic rsp_req_hit;
    logic rsp_alias_inv;
    always_comb begin
        s_alias_inv = (TAGALIAS_W > 0) && kind_allocates(c_kind) && req_hit
                      && (s_aliases[c_port*TAGAW +: TAGAW] != c_alias[TAGAW-1:0]);
        unique case (dir_rsp_state)
            DIR_U:     rsp_req_hit = dir_rsp_hit && (dir_rsp_owner == c_port);
            DIR_S:     rsp_req_hit = dir_rsp_hit && dir_rsp_sharers[c_port];
            default:   rsp_req_hit = 1'b0;
        endcase
        rsp_alias_inv = (TAGALIAS_W > 0) && kind_allocates(c_kind) && rsp_req_hit
                        && (dir_rsp_aliases[c_port*TAGAW +: TAGAW] != c_alias[TAGAW-1:0]);
    end

    // True when the main line must be read from AXI.
    logic need_axi_rd;
    always_comb begin
        unique case (c_kind)
            XK_READ_NOSNP:    need_axi_rd = 1'b1;
            XK_READ_ONCE,
            XK_READ_SHARED:   need_axi_rd = !(snp_needed && s_gotpd);
            // An alias-invalidated requester no longer holds usable data: a
            // clean stale copy (no PD) means the line must be refilled from
            // AXI even though the directory hit; with PD the merged data
            // serves the response (after_snoop's writeback still runs first).
            XK_READ_UNIQUE:   need_axi_rd = (c_expcd || !req_hit || s_alias_inv)
                                            && !(snp_needed && s_gotpd);
            default:          need_axi_rd = 1'b0;
        endcase
    end

    // Next phase after the main snoop phase (TRANSACTIONS.md per kind).
    // Allocating kinds (RS/RU/MU) commit the directory BEFORE responding, so
    // a GRANT-full retry can never re-emit response flits (§T15 note).
    function automatic phase_e after_snoop();
        unique case (c_kind)
            XK_READ_ONCE:
                return (snp_needed && s_gotpd) ? PH_AXI_WR
                     : (need_axi_rd            ? PH_AXI_RD : PH_RESP);
            XK_READ_SHARED:
                return (snp_needed && s_gotpd) ? PH_AXI_WR : PH_AXI_RD;
            XK_READ_UNIQUE:
                return (snp_needed && s_gotpd) ? PH_AXI_WR
                     : (need_axi_rd            ? PH_AXI_RD : PH_COMMIT);
            XK_MAKE_UNIQUE:   return PH_COMMIT;
            XK_WRITE_UNIQUE:  return PH_DBID;
            XK_CLEAN_SHARED,
            XK_CLEAN_INVALID: return (snp_needed && s_gotpd) ? PH_AXI_WR : PH_RESP;
            default:          return PH_RESP;
        endcase
    endfunction

    // Next phase after the SF commit phase.
    function automatic phase_e after_sf();
        unique case (c_kind)
            XK_READ_SHARED,
            XK_READ_UNIQUE,
            XK_MAKE_UNIQUE:  return PH_RESP;
            XK_EVICT:        return PH_RESP;
            default:         return PH_DONE;   // WU/WB/CMOs: no CompAck
        endcase
    endfunction

    // Directory commit image (commit-once; TRANSACTIONS.md per kind).
    // cm_aliases: the recorded alias of the requester is updated to the
    // request's alias on allocating kinds; all other slots pass the LOOKUP
    // snapshot through unchanged (REMOVE kinds don't write aliases anyway).
    logic        cm_en;
    logic [1:0]  cm_op;
    logic        cm_rmline;
    logic [1:0]  cm_state;
    logic [NUM_T1-1:0] cm_sharers;
    logic [PORT_W-1:0] cm_owner;
    logic [NUM_T1*TAGAW-1:0] cm_aliases;
    always_comb begin
        cm_en      = 1'b0;
        cm_op      = DIR_OP_GRANT;
        cm_rmline  = 1'b1;
        cm_state   = DIR_S;
        cm_sharers = '0;
        cm_owner   = c_port;
        cm_aliases = s_aliases;
        unique case (c_kind)
            XK_READ_SHARED: begin
                cm_en = 1'b1;
                cm_aliases[c_port*TAGAW +: TAGAW] = c_alias[TAGAW-1:0];
                if (dir_present == '0) begin
                    // promotion (PROTOCOL §7.4): sole tenant -> U{requester}
                    cm_state = DIR_U;
                    cm_owner = c_port;
                end else begin
                    cm_state   = DIR_S;
                    cm_sharers = dir_present | (NUM_T1'(1) << c_port);
                end
            end
            XK_READ_UNIQUE,
            XK_MAKE_UNIQUE: begin
                cm_en    = 1'b1;
                cm_state = DIR_U;
                cm_owner = c_port;
                cm_aliases[c_port*TAGAW +: TAGAW] = c_alias[TAGAW-1:0];
            end
            XK_WRITE_UNIQUE,
            XK_CLEAN_INVALID,
            XK_MAKE_INVALID: begin
                cm_en     = 1'b1;
                cm_op     = DIR_OP_REMOVE;
                cm_rmline = 1'b1;
            end
            XK_WRITE_BACK,
            XK_EVICT: begin
                // PORT mode: clear only the evictor's own presence. Order-safe
                // against a same-line REQ slot that re-grants the line while
                // this EVT is in flight (§7.3 EVT-sharing).
                cm_en     = 1'b1;
                cm_op     = DIR_OP_REMOVE;
                cm_rmline = 1'b0;
            end
            default: ;
        endcase
    end

    // SF commit image (commit-once): presence after the transaction.
    logic        su_en;
    logic [1:0]  su_op;
    logic [NUM_T1-1:0] su_presence;
    always_comb begin
        su_en       = 1'b0;
        su_op       = SF_OP_UPDATE;
        su_presence = '0;
        unique case (c_kind)
            XK_READ_SHARED: begin
                su_en       = 1'b1;
                su_presence = (dir_present == '0)
                              ? (NUM_T1'(1) << c_port)
                              : (dir_present | (NUM_T1'(1) << c_port));
            end
            XK_READ_UNIQUE,
            XK_MAKE_UNIQUE: begin
                su_en       = 1'b1;
                su_presence = NUM_T1'(1) << c_port;
            end
            XK_WRITE_UNIQUE,
            XK_CLEAN_INVALID,
            XK_MAKE_INVALID: begin
                su_en       = 1'b1;
                su_presence = '0;
            end
            XK_WRITE_BACK,
            XK_EVICT: begin
                // PORT-mode removal, order-safe under EVT-sharing (§7.3).
                su_en = 1'b1;
                su_op = SF_OP_REMOVE;
            end
            default: ;  // ReadOnce/ReadNoSnp/WriteNoSnp/CleanShared/Stash: untouched
        endcase
    end

    // AXI address: line base for whole-line ops, 32 B base for sub-line ops.
    logic [ADDR_W-1:0] main_addr;
    assign main_addr = (c_size >= 3'd6) ? line_to_addr(c_line)
                                        : {c_line, c_addr5, 5'b00000};

    // Next snoop target (lowest set bit of s_snp_pend).
    logic [PORT_W-1:0] snp_target;
    always_comb begin
        snp_target = '0;
        for (int p = NUM_T1-1; p >= 0; p--)
            if (s_snp_pend[p])
                snp_target = PORT_W'(p);
    end

    // Snoop flit (opcode fixed per kind; target/TxnID vary per emission).
    // Two overrides force SnpToInvalid: the §T15 victim phase, and the
    // TagAlias back-invalidation of the requester's own stale-alias copy.
    always_comb begin
        snp_bits          = '0;
        snp_bits.txnid    = snp_gnt_id;
        snp_bits.srcid    = DN_NID_W'(NODE_ID);
        snp_bits.tgtid    = UP_NID_W'(snp_target);
        snp_bits.opcode   = ((ph == PH_VIC_SNP)
                             || (s_alias_inv && (snp_target == c_port)))
                            ? SNP_TO_INVALID
                            : snp_opcode(c_kind, c_opcode);
        snp_bits.addr     = line_to_snp_addr((ph == PH_VIC_SNP) ? s_vline : c_line);
        snp_bits.ns       = 1'b0;
        snp_bits.tracetag = 1'b0;
    end
    assign snp_port = snp_target;

    // ------------------------------------------------------------------ responses
    // resp_is_dat selects CompData; resp_need_dbid marks flits that allocate
    // and echo the DBID (PROTOCOL §5.2).
    logic        resp_is_dat;
    logic        resp_need_dbid;
    logic        resp_beat;
    logic [2:0]  resp_resp;

    always_comb begin
        resp_is_dat    = 1'b0;
        resp_need_dbid = 1'b0;
        resp_resp      = RESP_UC;
        unique case (c_kind)
            XK_READ_NOSNP,
            XK_READ_ONCE: begin
                resp_is_dat = 1'b1;
                resp_resp   = RESP_UC;
            end
            XK_READ_SHARED: begin
                resp_is_dat    = 1'b1;
                resp_need_dbid = 1'b1;
                resp_resp      = (dir_present == '0) ? RESP_UC : RESP_SC;
            end
            XK_READ_UNIQUE: begin
                resp_resp      = RESP_UC;
                // ExpCompData is a hint: CompData is owed whenever the
                // requester is not a recorded current holder (!req_hit),
                // not only when it asked for data (c_expcd). An alias
                // back-invalidation (s_alias_inv) snooped the requester's
                // stale-alias copy away, so it provably holds no data at
                // completion and data is mandatory (a snooped requester
                // must be returned data).
                resp_is_dat    = c_expcd || !req_hit || s_alias_inv;
                resp_need_dbid = 1'b1;      // CompData beat 0 / Comp both carry DBID
            end
            XK_MAKE_UNIQUE: resp_need_dbid = 1'b1;
            default: ;
        endcase
    end

    assign resp_beat = (c_size >= 3'd6) ? ((s_rbeats == 2'b01) ? 1'b1 : 1'b0) : c_addr5;

    always_comb begin
        dnrsp_bits          = '0;
        dnrsp_bits.txnid    = c_txnid;
        dnrsp_bits.srcid    = DN_NID_W'(NODE_ID);
        dnrsp_bits.tgtid    = UP_NID_W'(c_port);
        dnrsp_bits.dbid     = s_dbid_v ? s_dbid : (dbid_req ? dbid_gnt_id : '0);
        dnrsp_bits.resperr  = 2'b00;
        dnrsp_bits.cbusy    = 3'b000;
        dnrsp_bits.wayvalid = 1'b0;
        dnrsp_bits.way      = '0;
        dnrsp_bits.tracetag = 1'b0;
        unique case (c_kind)
            XK_READ_UNIQUE,
            XK_MAKE_UNIQUE: begin
                dnrsp_bits.opcode = DNRSP_COMP;
                dnrsp_bits.resp   = RESP_UC;
            end
            XK_WRITE_NOSNP,
            XK_WRITE_UNIQUE: begin
                dnrsp_bits.opcode = (ph == PH_DBID) ? DNRSP_DBIDRESP : DNRSP_COMP;
                dnrsp_bits.resp   = RESP_I;
            end
            XK_WRITE_BACK: begin
                dnrsp_bits.opcode = DNRSP_COMPDBID;
                dnrsp_bits.resp   = RESP_I;
            end
            XK_EVICT: begin
                dnrsp_bits.opcode = DNRSP_COMP;
                dnrsp_bits.resp   = RESP_I;
                dnrsp_bits.dbid   = '0;
            end
            XK_CLEAN_SHARED,
            XK_CLEAN_INVALID,
            XK_MAKE_INVALID: begin
                dnrsp_bits.opcode = DNRSP_COMPCMO;
                dnrsp_bits.resp   = RESP_I;
            end
            XK_STASH: begin
                dnrsp_bits.opcode = DNRSP_COMP_STASH;
                dnrsp_bits.resp   = RESP_I;
            end
            default: begin
                dnrsp_bits.opcode = DNRSP_COMP;
                dnrsp_bits.resp   = RESP_I;
            end
        endcase

        dndat_bits            = '0;
        dndat_bits.txnid      = c_txnid;
        dndat_bits.srcid      = DN_NID_W'(NODE_ID);
        dndat_bits.tgtid      = UP_NID_W'(c_port);
        dndat_bits.dbid       = s_dbid_v ? s_dbid : (dbid_req ? dbid_gnt_id : '0);
        dndat_bits.opcode     = DNDAT_COMPDATA;
        dndat_bits.resperr    = 2'b00;
        dndat_bits.resp       = resp_resp;
        dndat_bits.datasource = 5'd0;
        dndat_bits.cbusy      = 3'b000;
        dndat_bits.dataid     = resp_beat;
        dndat_bits.data       = resp_beat ? s_data[2*DATA_W-1:DATA_W]
                                          : s_data[DATA_W-1:0];
        dndat_bits.wayvalid   = 1'b0;
        dndat_bits.way        = '0;
        dndat_bits.tracetag   = 1'b0;
    end

    // ------------------------------------------------------------------ requests
    always_comb begin
        dir_req      = 1'b0;
        dir_op       = DIR_OP_LOOKUP;
        dir_line     = c_line;
        dir_port     = c_port;
        dir_rmline   = cm_rmline;
        dir_state    = cm_state;
        dir_sharers  = cm_sharers;
        dir_owner    = cm_owner;
        dir_aliases  = cm_aliases;
        dir_men      = 1'b0;
        dir_mstate   = DIR_I;
        dir_msharers = '0;
        dir_mowner   = '0;

        sf_req       = 1'b0;
        sf_op        = SF_OP_LOOKUP;
        sf_line      = c_line;
        sf_presence  = su_presence;
        sf_port      = c_port;

        axi_rd_req   = 1'b0;
        axi_rd_addr  = main_addr;
        axi_rd_len   = size_axlen(c_size);
        axi_rd_start = c_addr5;

        axi_wr_req   = 1'b0;
        axi_wr_addr  = main_addr;
        axi_wr_len   = kind_is_write(c_kind) ? size_axlen(c_size) : 8'd1;
        axi_wr_start = kind_is_write(c_kind) ? c_addr5 : 1'b0;
        axi_wr_data  = s_data;
        axi_wr_strb  = s_strb;

        snp_req      = 1'b0;

        dnrsp_req    = 1'b0;
        dndat_req    = 1'b0;
        dbid_req     = 1'b0;

        unique case (ph)
            PH_DIR_LU: begin
                // The retry throttle (s_vic_wait) only runs after a vic_blk
                // block; the first LOOKUP of a transaction is unthrottled
                // (s_vic_wait is cleared at alloc).
                dir_req = !sent_dir && (s_vic_wait == '0);
                dir_op  = DIR_OP_LOOKUP;
            end
            PH_SF_LU: begin
                sf_req = (SF_ENABLE != 0) && !sent_sf;
                sf_op  = SF_OP_LOOKUP;
            end
            PH_SNOOP: begin
                // Gate emission on snp_needed: on a miss, SF over-inclusion
                // (maybe_all/SF_BROADCAST) may arm a nonzero mask even though
                // no snoops are required — firing those would leave answers
                // outstanding past retirement (and is wasted traffic anyway).
                snp_req = (|s_snp_pend) && snp_needed;
            end
            PH_VIC_SNP: snp_req = |s_snp_pend;
            PH_VIC_RM: begin
                dir_req      = !sent_dir;
                dir_op       = DIR_OP_REMOVE;
                dir_line     = s_vline;
                dir_rmline   = 1'b1;
                // Atomic compare-and-remove (§T15): drop the victim entry only
                // while its meta still equals the LOOKUP-time image we snooped.
                dir_men      = 1'b1;
                dir_mstate   = s_vstate;
                dir_msharers = s_vsharers;
                dir_mowner   = s_vowner;
            end
            PH_VIC_RSF: begin
                sf_req      = (SF_ENABLE != 0) && !sent_sf;
                sf_op       = SF_OP_UPDATE;
                sf_line     = s_vline;
                sf_presence = '0;
            end
            PH_AXI_RD:  axi_rd_req = !sent_rd && !wb_blk;   // M5 (§7.3): wait
                                                            // for a colliding WB
                                                            // write to land first
            PH_VIC_WB: begin
                axi_wr_req   = !sent_wr;
                axi_wr_addr  = line_to_addr(s_vline);
                axi_wr_len   = 8'd1;
                axi_wr_start = 1'b0;
            end
            PH_AXI_WR:  axi_wr_req = !sent_wr;
            PH_DBID: begin
                dnrsp_req = 1'b1;
                dbid_req  = 1'b1;
            end
            PH_COMMIT: begin
                dir_req = cm_en && !sent_dir;
                dir_op  = cm_op;
            end
            PH_SF_UPD: begin
                sf_req = (SF_ENABLE != 0) && su_en && !sent_sf;
                sf_op  = su_op;
            end
            PH_RESP: begin
                if (resp_is_dat) begin
                    dndat_req = 1'b1;
                    dbid_req  = resp_need_dbid && !s_dbid_v;
                end else begin
                    dnrsp_req = 1'b1;
                    dbid_req  = resp_need_dbid && !s_dbid_v;
                end
            end
            default: ;
        endcase
    end

    // DBID visibility to the steering tables (venus.sv): write-keyed DBIDs only.
    assign wr_dbid    = s_dbid_v && kind_is_write(c_kind);
    assign wr_dbid_id = s_dbid;

    // Last expected write-data beat (frees the DBID, PROTOCOL §5.2).
    assign wr_last = wrdat_valid
                     && ((c_size >= 3'd6) ? (wrdat_beat && (s_wrbeats == 2'b01))
                                          : 1'b1);

    // Evict Comp wire fire for this slot's transaction.
    logic pop_fire;
    assign pop_fire = pop_valid[c_port] && (pop_id[c_port] == c_txnid);

    // ------------------------------------------------------------------ FSM
    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            ph          <= PH_IDLE;
            c_kind      <= XK_DROP;
            c_opcode    <= '0;
            c_port      <= '0;
            c_txnid     <= '0;
            c_line      <= '0;
            c_size      <= '0;
            c_addr5     <= 1'b0;
            c_expcd     <= 1'b0;
            c_alias     <= '0;
            c_axi       <= '0;
            s_hit       <= 1'b0;
            s_state     <= DIR_I;
            s_sharers   <= '0;
            s_owner     <= '0;
            s_aliases   <= '0;
            s_full      <= 1'b0;
            s_vlive     <= 1'b0;
            s_vstate    <= DIR_I;
            s_vsharers  <= '0;
            s_vowner    <= '0;
            s_vline     <= '0;
            s_sf_pres   <= '0;
            s_sf_maybe  <= 1'b0;
            s_data      <= '0;
            s_strb      <= '0;
            s_gotpd     <= 1'b0;
            s_dbid_v    <= 1'b0;
            s_dbid      <= '0;
            s_snp_pend  <= '0;
            s_snp_out   <= '0;
            s_wrbeats   <= '0;
            s_rbeats    <= '0;
            w_ack       <= 1'b0;
            s_wrdone    <= 1'b0;
            s_vic_wait  <= '0;
            sent_dir    <= 1'b0;
            sent_sf     <= 1'b0;
            sent_rd     <= 1'b0;
            sent_wr     <= 1'b0;
            for (int p = 0; p < NUM_T1; p++)
                s_snp_id[p] <= '0;
        end else begin

            // ---- orthogonal intake latches (any phase) ----
            if (ack_valid)
                w_ack <= 1'b1;

            // Snoop answers, per answering port. A SnpResp completes its target
            // immediately; SnpRespData merges PD data and completes at the last
            // beat. At most one PD source exists per snoop phase (single dirty
            // owner), so same-cycle merges from two ports cannot conflict.
            for (int p = 0; p < NUM_T1; p++) begin
                if (snpans_valid[p]) begin
`ifndef SYNTHESIS
                    if (!(s_snp_out[p] && (s_snp_id[p] == snpans_id[p]))) begin
                        $error("venus_mshr: SnpResp for unknown snoop id %0d port %0d (ph=%0d kind=%0d out=%b expid=%0d resp=%0d)",
                               snpans_id[p], p, ph, c_kind, s_snp_out,
                               s_snp_id[p], snpans_resp[p]);
                    end
`endif
                    if (s_snp_out[p] && (s_snp_id[p] == snpans_id[p]))
                        s_snp_out[p] <= 1'b0;
                end

                if (snpdat_valid[p]) begin
                    if (resp_is_pd(snpdat_resp[p])) begin
                        if (!snpdat_beat[p]) begin
                            s_data[DATA_W-1:0]        <= snpdat_data[p];
                            s_strb[BE_W-1:0]          <= {BE_W{1'b1}};
                        end else begin
                            s_data[2*DATA_W-1:DATA_W] <= snpdat_data[p];
                            s_strb[2*BE_W-1:BE_W]     <= {BE_W{1'b1}};
                        end
                        s_gotpd <= 1'b1;
                    end
                    if (snpdat_last[p]) begin
`ifndef SYNTHESIS
                        if (!(s_snp_out[p] && (s_snp_id[p] == snpdat_id[p])))
                            $error("venus_mshr: SnpRespData for unknown snoop id %0d port %0d",
                                   snpdat_id[p], p);
`endif
                        if (s_snp_out[p] && (s_snp_id[p] == snpdat_id[p]))
                            s_snp_out[p] <= 1'b0;
                    end
                end
            end

            if (wrdat_valid) begin
`ifndef SYNTHESIS
                if (!s_dbid_v)
                    $error("venus_mshr: write data before DBID grant");
`endif
                // Write wins over any previously merged PD (§T7): apply BE.
                for (int b = 0; b < BE_W; b++) begin
                    if (wrdat_be[b]) begin
                        if (!wrdat_beat)
                            s_data[b*8 +: 8] <= wrdat_data[b*8 +: 8];
                        else
                            s_data[DATA_W + b*8 +: 8] <= wrdat_data[b*8 +: 8];
                    end
                end
                if (!wrdat_beat) begin
                    s_strb[BE_W-1:0]      <= s_strb[BE_W-1:0] | wrdat_be;
                    s_wrbeats[0]          <= 1'b1;
                end else begin
                    s_strb[2*BE_W-1:BE_W] <= s_strb[2*BE_W-1:BE_W] | wrdat_be;
                    s_wrbeats[1]          <= 1'b1;
                end
            end

            // §T15 victim-arm retry throttle countdown (loaded by the vic_blk
            // branch in PH_DIR_LU below; while nonzero the request mux holds
            // dir_req low, so the fixed-priority directory arbiter always
            // sees gaps that let a directory-bound blocker slot through).
            if (s_vic_wait != '0)
                s_vic_wait <= s_vic_wait - 6'd1;

            // backend response bookkeeping
            if (dir_rsp_valid)
                sent_dir <= 1'b0;
            if (sf_rsp_valid)
                sent_sf <= 1'b0;
            if (axi_rd_gnt)
                sent_rd <= 1'b1;
            if (axi_rd_done)
                sent_rd <= 1'b0;
            if (axi_wr_gnt)
                sent_wr <= 1'b1;
            if (axi_wr_done)
                sent_wr <= 1'b0;
            if (dir_gnt)
                sent_dir <= 1'b1;
            if (sf_gnt)
                sent_sf <= 1'b1;

            // ---- main phase walk ----
            unique case (ph)

            PH_IDLE: begin
                if (alloc) begin
                    c_kind     <= alloc_kind;
                    c_opcode   <= alloc_opcode;
                    c_port     <= alloc_port;
                    c_txnid    <= alloc_txnid;
                    c_line     <= alloc_line;
                    c_size     <= alloc_size;
                    c_addr5    <= alloc_addr5;
                    c_expcd    <= alloc_expcd;
                    c_alias    <= alloc_alias;
                    c_axi      <= alloc_axi;
                    s_hit      <= 1'b0;
                    s_state    <= DIR_I;
                    s_sharers  <= '0;
                    s_owner    <= '0;
                    s_aliases  <= '0;
                    s_full     <= 1'b0;
                    s_vlive    <= 1'b0;
                    s_vstate   <= DIR_I;
                    s_vsharers <= '0;
                    s_vowner   <= '0;
                    s_vline    <= '0;
                    s_sf_pres  <= '0;
                    s_sf_maybe <= 1'b0;
                    s_data     <= '0;
                    s_strb     <= '0;
                    s_gotpd    <= 1'b0;
                    s_dbid_v   <= 1'b0;
                    s_snp_pend <= '0;
                    s_snp_out  <= '0;
                    s_wrbeats  <= '0;
                    s_rbeats   <= '0;
                    w_ack      <= 1'b0;
                    s_wrdone   <= 1'b0;
                    s_vic_wait <= '0;
                    sent_dir   <= 1'b0;
                    sent_sf    <= 1'b0;
                    sent_rd    <= 1'b0;
                    sent_wr    <= 1'b0;

                    // Entry dispatch per kind (TRANSACTIONS.md §T1..§T14).
                    unique case (alloc_kind)
                        XK_READ_NOSNP:    ph <= PH_AXI_RD;
                        XK_READ_ONCE,
                        XK_READ_SHARED,
                        XK_READ_UNIQUE,
                        XK_MAKE_UNIQUE,
                        XK_WRITE_UNIQUE,
                        XK_CLEAN_SHARED,
                        XK_CLEAN_INVALID,
                        XK_MAKE_INVALID:  ph <= PH_DIR_LU;
                        XK_WRITE_NOSNP:   ph <= PH_DBID;
                        XK_WRITE_BACK:    ph <= PH_DBID;
                        XK_EVICT:         ph <= PH_COMMIT;
                        XK_STASH:         ph <= alloc_expcd ? PH_RESP : PH_DONE;
                        default:          ph <= PH_DONE;  // XK_DROP never admitted
                    endcase
                end
            end

            // ---- directory LOOKUP (all dir-touching kinds) ----
            PH_DIR_LU: begin
                if (dir_rsp_valid) begin
                    s_hit      <= dir_rsp_hit;
                    s_state    <= dir_rsp_state;
                    s_sharers  <= dir_rsp_sharers;
                    s_owner    <= dir_rsp_owner;
                    s_aliases  <= dir_rsp_aliases;
                    s_full     <= dir_rsp_full;
                    s_vlive    <= dir_rsp_vlive;
                    s_vstate   <= dir_rsp_vstate;
                    s_vsharers <= dir_rsp_vsharers;
                    s_vowner   <= dir_rsp_vowner;
                    s_vline    <= dir_rsp_vline;

                    if (kind_allocates(c_kind) && !dir_rsp_hit && dir_rsp_full
                        && dir_rsp_vlive) begin
                        if (vic_blk) begin
                            // §T15 arm hazard: the proposed victim line is
                            // another slot's in-flight main line. Do NOT arm —
                            // re-issue the LOOKUP after a throttled gap and
                            // re-plan from a fresh proposal once the blocker
                            // completes. A pure stall is not enough: the image
                            // would still be the stale one (snoops off it would
                            // kill the blocker's freshly-committed copy while
                            // the compare-and-remove then no-ops → desync), so
                            // each retry re-looks-up. The throttle
                            // (VIC_RETRY_THROTTLE, counted down by s_vic_wait;
                            // dir_req held low meanwhile) exists because the
                            // directory arbiter is fixed lowest-slot-index
                            // priority: an unthrottled retry holds dir_req
                            // ~continuously and starves a higher-index
                            // directory-bound blocker forever (priority-
                            // inversion livelock). Deadlock-free: the blocker's
                            // main line is directory-resident (a missed line
                            // can't be proposed as victim), so it hit or
                            // already allocated, never needs a victim itself,
                            // and always completes independently once it gets
                            // the directory.
                            s_vic_wait <= 6'(VIC_RETRY_THROTTLE);
                            ph <= PH_DIR_LU;
                        end else begin
                            // §T15: allocating read, miss, full set -> victim sub-flow.
                            // Arm the victim mask from the response (dir-exact).
                            unique case (dir_rsp_vstate)
                                DIR_S:   s_snp_pend <= dir_rsp_vsharers;
                                DIR_U:   s_snp_pend <= NUM_T1'(1) << dir_rsp_vowner;
                                default: s_snp_pend <= '0;
                            endcase
                            ph <= PH_VIC_SNP;
                        end
                    end else if (SF_ENABLE != 0) begin
                        ph <= PH_SF_LU;
                    end else begin
                        // No SF: arm the main mask from the response and go
                        // snoop. The alias-mismatch self-snoop of the requester
                        // (response-domain compare — the snapshot registers
                        // latch only at this edge) is OR-ed in here.
                        logic [NUM_T1-1:0] pres;
                        unique case (dir_rsp_state)
                            DIR_S:   pres = dir_rsp_sharers;
                            DIR_U:   pres = NUM_T1'(1) << dir_rsp_owner;
                            default: pres = '0;
                        endcase
                        s_snp_pend <= (kind_self_snoop(c_kind)
                                       ? pres
                                       : (pres & ~(NUM_T1'(1) << c_port)))
                                      | (NUM_T1'(rsp_alias_inv) << c_port);
                        ph <= PH_SNOOP;
                    end
                end
            end

            // ---- snoop-filter LOOKUP (refines the mask) ----
            PH_SF_LU: begin
                if (sf_rsp_valid) begin
                    s_sf_pres  <= sf_rsp_presence;
                    s_sf_maybe <= sf_rsp_maybe;
                    // Arm the main mask from the dir snapshot (stable) plus the
                    // just-returned SF response — but only when this snapshot
                    // actually requires snoops (see the snp_req gate). An alias
                    // mismatch adds the requester itself as a target.
                    s_snp_pend <= snp_needed
                                  ? (mask_of(dir_present, sf_rsp_presence, sf_rsp_maybe,
                                             c_kind, c_port)
                                     | (NUM_T1'(s_alias_inv) << c_port))
                                  : '0;
                    ph         <= PH_SNOOP;
                end
            end

            // ---- main-line snoops ----
            PH_SNOOP: begin
                if (!snp_needed || (s_snp_pend == '0 && s_snp_out == '0))
                    ph <= after_snoop();
            end

            // ---- §T15 victim: snoop the victim line's holders ----
            PH_VIC_SNP: begin
                if (s_snp_pend == '0 && s_snp_out == '0)
                    ph <= s_gotpd ? PH_VIC_WB : PH_VIC_RM;
            end

            PH_VIC_WB: begin
                if (axi_wr_done)
                    ph <= PH_VIC_RM;
            end

            PH_VIC_RM: begin
                if (dir_rsp_valid) begin
                    if (SF_ENABLE != 0) begin
                        // SF clear only when the entry was actually dropped.
                        // On a compare-and-remove no-op (dir_rsp_hit=0) the
                        // line may have been re-granted to a new owner, and
                        // clearing its SF presence could erase that record;
                        // the set is still full, so the main flow's GRANT
                        // retries and re-plans with a fresh victim (§T15).
                        ph <= dir_rsp_hit ? PH_VIC_RSF : PH_SF_LU;
                    end else begin
                        // Resume the main flow without SF: arm from the snapshot.
                        s_snp_pend <= kind_self_snoop(c_kind)
                                      ? dir_present
                                      : (dir_present & ~(NUM_T1'(1) << c_port));
                        ph <= PH_SNOOP;
                    end
                end
            end

            PH_VIC_RSF: begin
                if (sf_rsp_valid)
                    ph <= PH_SF_LU;   // main-line SF lookup, then snoop phase
            end

            // ---- AXI read fill ----
            PH_AXI_RD: begin
                if (axi_rd_done) begin
                    s_data <= axi_rd_data;
                    if (c_size >= 3'd6)
                        s_strb <= {LINE_BYTES{1'b1}};
                    else if (!c_addr5)
                        s_strb[BE_W-1:0] <= {BE_W{1'b1}};
                    else
                        s_strb[2*BE_W-1:BE_W] <= {BE_W{1'b1}};
                    // Allocating kinds commit before responding (§T15 note).
                    unique case (c_kind)
                        XK_READ_SHARED,
                        XK_READ_UNIQUE:  ph <= PH_COMMIT;
                        default:         ph <= PH_RESP;
                    endcase
                end
            end

            // ---- AXI write (merged image / write drain / PD writeback) ----
            PH_AXI_WR: begin
                if (axi_wr_done) begin
                    if (kind_is_write(c_kind))
                        s_wrdone <= 1'b1;   // releases same-line readers (§7.3)
                    unique case (c_kind)
                        XK_READ_SHARED,
                        XK_READ_UNIQUE:   ph <= PH_COMMIT;   // commit before RESP
                        XK_READ_ONCE,
                        XK_CLEAN_SHARED,
                        XK_CLEAN_INVALID: ph <= PH_RESP;
                        XK_WRITE_BACK:    ph <= PH_COMMIT;
                        default:          ph <= PH_RESP;   // WN/WU: Comp after write
                    endcase
                end
            end

            // ---- write-flow DBID grant emission ----
            PH_DBID: begin
                if (dnrsp_gnt) begin
                    s_dbid   <= dbid_gnt_id;
                    s_dbid_v <= 1'b1;
                    ph       <= PH_WAIT_WR;
                end
            end

            // ---- write-data collection ----
            PH_WAIT_WR: begin
                if (wr_last)
                    ph <= PH_AXI_WR;
            end

            // ---- downstream responses ----
            PH_RESP: begin
                if (resp_is_dat) begin
                    if (dndat_gnt) begin
                        if (resp_need_dbid && !s_dbid_v) begin
                            s_dbid   <= dbid_gnt_id;
                            s_dbid_v <= 1'b1;
                        end
                        s_rbeats <= s_rbeats | (2'b01 << resp_beat);
                        if ((c_size >= 3'd6) ? (s_rbeats == 2'b01) : 1'b1) begin
                            // last beat emitted; reads/dataless wait CompAck
                            unique case (c_kind)
                                XK_READ_NOSNP,
                                XK_READ_ONCE:  ph <= PH_DONE;
                                default:       ph <= PH_WAIT_ACK;
                            endcase
                        end
                    end
                end else if (dnrsp_gnt) begin
                    if (resp_need_dbid && !s_dbid_v) begin
                        s_dbid   <= dbid_gnt_id;
                        s_dbid_v <= 1'b1;
                    end
                    unique case (c_kind)
                        XK_READ_UNIQUE,
                        XK_MAKE_UNIQUE:  ph <= PH_WAIT_ACK;
                        XK_WRITE_NOSNP,
                        XK_WRITE_UNIQUE: ph <= PH_COMMIT;   // Comp after AXI B
                        XK_CLEAN_SHARED,
                        XK_CLEAN_INVALID,
                        XK_MAKE_INVALID: ph <= PH_COMMIT;
                        XK_EVICT:        ph <= PH_WAIT_POP;
                        default:         ph <= PH_DONE;     // Stash
                    endcase
                end
            end

            // ---- directory commit (commit-once) ----
            PH_COMMIT: begin
                if (!cm_en)
                    ph <= PH_SF_UPD;
                else if (dir_rsp_valid) begin
                    if (kind_allocates(c_kind) && dir_rsp_full) begin
                        // Lost the post-victim allocation race (§T15): re-plan
                        // from a fresh LOOKUP. Response progress is untouched
                        // because allocating kinds commit before responding.
                        s_gotpd <= 1'b0;
                        ph      <= PH_DIR_LU;
                    end else begin
                        ph <= PH_SF_UPD;
                    end
                end
            end

            // ---- snoop-filter commit ----
            PH_SF_UPD: begin
                if ((SF_ENABLE == 0) || !su_en)
                    ph <= after_sf();
                else if (sf_rsp_valid)
                    ph <= after_sf();
            end

            // ---- CompAck wait (reads/dataless) ----
            PH_WAIT_ACK: begin
                if (w_ack)
                    ph <= PH_DONE;
            end

            // ---- Evict Comp wire pop ----
            PH_WAIT_POP: begin
                if (pop_fire)
                    ph <= PH_DONE;
            end

            PH_DONE: begin
                ph <= PH_IDLE;
            end

            default: ph <= PH_IDLE;
            endcase

            // Snoop emission progress (any snoop phase): record the granted ID.
            if (snp_gnt && ((ph == PH_SNOOP) || (ph == PH_VIC_SNP))) begin
                s_snp_pend[snp_target] <= 1'b0;
                s_snp_out[snp_target]  <= 1'b1;
                s_snp_id[snp_target]   <= snp_gnt_id;
            end
        end
    end

endmodule

`default_nettype wire
