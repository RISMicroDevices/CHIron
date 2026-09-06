`default_nettype none

import venus_pkg::*;

// ============================================================================
// venus_directory — set-associative I/S/U coherence directory.
//
// One SRAM/REG word per set packs all WAYS entries {valid, tag(line), state[1:0],
// sharers[NUM_T1], owner, aliases[NUM_T1*TAGAW]} plus per-set replacement state
// (tree-PLRU for WAYS=4, round-robin otherwise). Storage backend is
// venus_sram_box (STORAGE_DIR selects REG or behavioral SRAM timing).
// aliases[p] records the TagAlias upstream p last obtained the line with; it is
// meaningful only while holder bit p is set (state S: sharers[p]; state U:
// owner == p). TAGALIAS_W=0 collapses the field to inert zero bits.
//
// Command contract (single outstanding command; see TRANSACTIONS.md):
//   OP 0 LOOKUP : read set; report hit + entry meta (including the alias image
//                 on rsp_aliases), fullness, and a victim proposal
//                 (invalid-way-first else PLRU) with the victim's full meta for
//                 the back-invalidation sub-flow (§T15). Pure read.
//   OP 1 GRANT  : RMW allocate/overwrite with the commit image supplied by the
//                 tracker (cmd_state/cmd_sharers/cmd_owner/cmd_aliases). Hit ->
//                 overwrite at the hit way. Miss -> invalid-way-first; when no
//                 invalid way exists the command is REJECTED with rsp_full=1
//                 and nothing is written (the tracker back-invalidates the PLRU
//                 victim and retries). PLRU is touched on success.
//   OP 2 REMOVE : RMW remove. cmd_rm_line=0 (PORT mode): clear cmd_port's sharer
//                 bit (or drop a U{port} entry); entry dropped when it becomes
//                 empty. cmd_rm_line=1 (LINE mode): drop the whole entry.
//                 With cmd_men=1 the drop is conditional on the entry meta still
//                 matching {cmd_mstate,cmd_msharers,cmd_mowner} — the atomic
//                 compare-and-remove used by the §T15 victim flow: a GRANT that
//                 landed after the tracker's LOOKUP makes its victim snoops
//                 stale, so the remove must no-op (rsp_hit=0) and let the
//                 tracker re-plan. Idempotent when the line is absent.
//                 Aliases are deliberately NOT part of the match image (holder
//                 identity is state/sharers/owner only); a cleared holder bit
//                 makes its alias slot don't-care, so REMOVE paths never touch
//                 the alias array.
//
// Handshake: cmd_valid&&cmd_ready accepts (cmd_ready also requires the storage
// backend to accept the read). rsp_valid is a one-cycle-level response held
// until rsp_ready. All RMW ops re-read the set inside the command, so commits
// from different slots to the same set can never clobber each other — only the
// allocation-vs-full outcome is subject to the retry race documented in §T15.
// ============================================================================

module venus_directory #(
    parameter int NUM_T1        = 4,
    parameter int SETS          = 64,
    parameter int WAYS          = 4,
    parameter int TAGALIAS_W    = 0,
    parameter int STORAGE       = 0,
    parameter int SRAM_LAT      = 1,
    parameter int SRAM_INTERVAL = 1,
    parameter int SRAM_PERIOD   = 1,
    parameter int SRAM_SLOT     = 0,
    localparam int PORT_W       = clog2_min1(NUM_T1),
    localparam int TAGAW        = (TAGALIAS_W > 0) ? TAGALIAS_W : 1
) (
    input  wire                         clock,
    input  wire                         reset,

    input  wire                         cmd_valid,
    output logic                        cmd_ready,
    input  wire  [1:0]                  cmd_op,
    input  wire  [LINE_W-1:0]           cmd_line,
    input  wire  [PORT_W-1:0]           cmd_port,       // GRANT owner / REMOVE port
    input  wire                         cmd_rm_line,    // REMOVE: 0=PORT mode, 1=LINE mode
    input  wire  [1:0]                  cmd_state,      // GRANT image: DIR_S / DIR_U
    input  wire  [NUM_T1-1:0]           cmd_sharers,    // GRANT image (state==S)
    input  wire  [PORT_W-1:0]           cmd_owner,      // GRANT image (state==U)
    input  wire  [NUM_T1*TAGAW-1:0]     cmd_aliases,    // GRANT image: per-port TagAlias
    input  wire                         cmd_men,        // REMOVE LINE mode: match enable
    input  wire  [1:0]                  cmd_mstate,     // REMOVE LINE mode: match image
    input  wire  [NUM_T1-1:0]           cmd_msharers,   //   (entry dropped only when its
    input  wire  [PORT_W-1:0]           cmd_mowner,     //    meta still matches; aliases
                                                        //    are NOT part of the match)

    output logic                        rsp_valid,
    input  wire                         rsp_ready,
    output logic                        rsp_hit,        // LOOKUP: hit; GRANT: accepted;
                                                        // REMOVE: entry was present and
                                                        // removed (0 on a match-fail no-op)
    output logic [1:0]                  rsp_state,      // LOOKUP hit meta
    output logic [NUM_T1-1:0]           rsp_sharers,
    output logic [PORT_W-1:0]           rsp_owner,
    output logic [NUM_T1*TAGAW-1:0]     rsp_aliases,    // LOOKUP hit per-port TagAlias image
    output logic [$clog2(WAYS)-1:0]     rsp_way,        // LOOKUP hit way / GRANT alloc way
    output logic                        rsp_full,       // LOOKUP: no invalid way;
                                                        // GRANT: rejected (full set)
    output logic                        rsp_vlive,      // victim proposal: entry live
    output logic [1:0]                  rsp_vstate,
    output logic [NUM_T1-1:0]           rsp_vsharers,
    output logic [PORT_W-1:0]           rsp_vowner,
    output logic [$clog2(WAYS)-1:0]     rsp_vway,
    output logic [LINE_W-1:0]           rsp_vline
);

    localparam int SET_W   = $clog2(SETS);
    localparam int WAY_IW  = $clog2(WAYS);
    localparam int ENT_W   = 1 + LINE_W + 2 + NUM_T1 + PORT_W + NUM_T1*TAGAW;
    localparam int PLRU_W  = (WAYS == 4) ? 3 : WAY_IW;
    localparam int BANK_W  = WAYS * ENT_W;
    localparam int WORD_W  = BANK_W + PLRU_W;

    localparam logic [1:0] OP_LOOKUP = 2'd0;
    localparam logic [1:0] OP_GRANT  = 2'd1;
    localparam logic [1:0] OP_REMOVE = 2'd2;

    typedef enum logic [1:0] {
        ST_IDLE = 2'd0,
        ST_WAIT = 2'd1,
        ST_HOLD = 2'd2,
        ST_WR   = 2'd3
    } st_e;

    st_e                    st;

    logic [1:0]             op_q;
    logic [LINE_W-1:0]      line_q;
    logic [PORT_W-1:0]      port_q;
    logic                   rmline_q;
    logic [1:0]             state_q;
    logic [NUM_T1-1:0]      sharers_q;
    logic [PORT_W-1:0]      owner_q;
    logic [NUM_T1*TAGAW-1:0] aliases_q;
    logic                   men_q;
    logic [1:0]             mstate_q;
    logic [NUM_T1-1:0]      msharers_q;
    logic [PORT_W-1:0]      mowner_q;
    logic [BANK_W-1:0]      bank_q;
    logic [PLRU_W-1:0]      plru_q;

    logic                   sram_req_valid;
    logic                   sram_req_ready;
    logic                   sram_req_we;
    logic [SET_W-1:0]       sram_req_addr;
    logic [WORD_W-1:0]      sram_req_wdata;
    logic                   sram_rsp_valid;
    logic [WORD_W-1:0]      sram_rsp_rdata;

    venus_sram_box #(
        .DEPTH    (SETS),
        .DW       (WORD_W),
        .STORAGE  (STORAGE),
        .LAT      (SRAM_LAT),
        .INTERVAL (SRAM_INTERVAL),
        .PERIOD   (SRAM_PERIOD),
        .SLOT     (SRAM_SLOT)
    ) u_box (
        .clock        (clock),
        .reset        (reset),
        .req_valid    (sram_req_valid),
        .req_ready    (sram_req_ready),
        .req_we       (sram_req_we),
        .req_addr     (sram_req_addr),
        .req_wdata    (sram_req_wdata),
        .rsp_valid    (sram_rsp_valid),
        .rsp_ready    (1'b1),
        .rsp_rdata    (sram_rsp_rdata)
    );

    // Set index: low bits of the line number.
    function automatic logic [SET_W-1:0] line_set(input logic [LINE_W-1:0] line);
        return line[SET_W-1:0];
    endfunction

    function automatic logic [ENT_W-1:0] pack_ent(
        input logic                 v,
        input logic [LINE_W-1:0]    tag,
        input logic [1:0]           stt,
        input logic [NUM_T1-1:0]    sh,
        input logic [PORT_W-1:0]    ow,
        input logic [NUM_T1*TAGAW-1:0] al
    );
        return {v, tag, stt, sh, ow, al};
    endfunction

    // ------------------------------------------------------------------
    // Combinational decode of the set read for the in-flight command:
    // hit scan, invalid-way scan, and the PLRU victim proposal.
    // ------------------------------------------------------------------
    logic                 hit_c;
    logic [WAY_IW-1:0]    hit_way_c;
    logic [1:0]           hit_st_c;
    logic [NUM_T1-1:0]    hit_sh_c;
    logic [PORT_W-1:0]    hit_ow_c;
    logic [NUM_T1*TAGAW-1:0] hit_al_c;
    logic                 free_c;
    logic [WAY_IW-1:0]    free_way_c;

    always_comb begin
        hit_c      = 1'b0;
        hit_way_c  = '0;
        hit_st_c   = DIR_I;
        hit_sh_c   = '0;
        hit_ow_c   = '0;
        hit_al_c   = '0;
        free_c     = 1'b0;
        free_way_c = '0;

        for (int w = 0; w < WAYS; w++) begin
            logic [ENT_W-1:0] e;
            logic             v;
            logic [LINE_W-1:0] tag;
            logic [1:0]        stt;
            logic [NUM_T1-1:0] sh;
            logic [PORT_W-1:0] ow;
            logic [NUM_T1*TAGAW-1:0] al;

            e = bank_q[w*ENT_W +: ENT_W];
            {v, tag, stt, sh, ow, al} = e;

            if (v && (tag == line_q)) begin
                hit_c     = 1'b1;
                hit_way_c = WAY_IW'(w);
                hit_st_c  = stt;
                hit_sh_c  = sh;
                hit_ow_c  = ow;
                hit_al_c  = al;
            end
            if (!v && !free_c) begin
                free_c     = 1'b1;
                free_way_c = WAY_IW'(w);
            end
        end
    end

    // PLRU victim way (only meaningful when free_c==0).
    logic [WAY_IW-1:0] plru_victim_c;
    always_comb begin
        if (WAYS == 4) begin
            // tree bits: b0 = MRU pair (0 left/ways01, 1 right/ways23);
            // b1/b2 = MRU way index inside the left/right pair.
            logic        pair;
            logic        inpair;
            pair   = ~plru_q[0];
            inpair = pair ? ~plru_q[2] : ~plru_q[1];
            plru_victim_c = {pair, inpair};
        end else begin
            plru_victim_c = plru_q;
        end
    end

    // PLRU update after touching wr_way (GRANT success).
    logic [PLRU_W-1:0] plru_next_c;
    always_comb begin
        plru_next_c = plru_q;
        if (WAYS == 4) begin
            plru_next_c[0] = wr_way[1];
            if (!wr_way[1])
                plru_next_c[1] = wr_way[0];
            else
                plru_next_c[2] = wr_way[0];
        end else begin
            plru_next_c = (wr_way == WAY_IW'(WAYS-1)) ? PLRU_W'(0)
                                                      : PLRU_W'(wr_way + 1'b1);
        end
    end

    // Victim proposal meta (PLRU way contents). The victim's aliases are NOT
    // surfaced: the §T15 flow invalidates the whole victim entry, so its
    // per-port aliases die with it.
    logic                 v_live_c;
    logic [LINE_W-1:0]    v_line_c;
    logic [1:0]           v_st_c;
    logic [NUM_T1-1:0]    v_sh_c;
    logic [PORT_W-1:0]    v_ow_c;
    logic [NUM_T1*TAGAW-1:0] v_al_c;
    always_comb begin
        logic [ENT_W-1:0] e;
        logic             v;
        e = bank_q[plru_victim_c*ENT_W +: ENT_W];
        {v, v_line_c, v_st_c, v_sh_c, v_ow_c, v_al_c} = e;
        v_live_c = v;
    end

    // ------------------------------------------------------------------
    // Command resolution: what (if anything) to write, and the response.
    // ------------------------------------------------------------------
    logic [WAY_IW-1:0] wr_way;
    logic [1:0]        wr_st;
    logic [NUM_T1-1:0] wr_sh;
    logic [PORT_W-1:0] wr_ow;
    logic [NUM_T1*TAGAW-1:0] wr_al;
    logic              wr_v;
    logic              need_wr;
    logic              is_full;
    logic              removed_c;

    always_comb begin
        wr_way    = hit_way_c;
        wr_st     = hit_st_c;
        wr_sh     = hit_sh_c;
        wr_ow     = hit_ow_c;
        wr_al     = hit_al_c;   // REMOVE: preserve (a cleared holder's alias
                                // slot becomes don't-care with its bit)
        wr_v      = hit_c;
        need_wr   = 1'b0;
        is_full   = 1'b0;
        removed_c = 1'b0;

        unique case (op_q)
        OP_LOOKUP: begin
            need_wr = 1'b0;
            is_full = !free_c;
        end
        OP_GRANT: begin
            wr_st = state_q;
            wr_sh = sharers_q;
            wr_ow = owner_q;
            wr_al = (TAGALIAS_W > 0) ? aliases_q : '0;
            wr_v  = 1'b1;
            if (hit_c) begin
                need_wr = 1'b1;
                wr_way  = hit_way_c;
            end else if (free_c) begin
                need_wr = 1'b1;
                wr_way  = free_way_c;
            end else begin
                // Full set: reject without writing; the tracker back-invalidates
                // the PLRU victim first and retries (TRANSACTIONS.md §T15).
                need_wr = 1'b0;
                is_full = 1'b1;
            end
        end
        OP_REMOVE: begin
            if (hit_c) begin
                if (rmline_q) begin
                    // LINE mode: drop the whole entry. With men_q (§T15 victim
                    // back-invalidation) the drop is conditional on the meta
                    // still matching the image the tracker snooped — a GRANT
                    // that landed after its LOOKUP makes those snoops stale,
                    // so the remove no-ops (removed_c=0) and the tracker
                    // re-plans from its full-set GRANT retry. The match image
                    // is state/sharers/owner only; aliases are excluded by
                    // design (they never mutate except via GRANT).
                    if (!men_q || (hit_st_c == mstate_q && hit_sh_c == msharers_q
                                   && hit_ow_c == mowner_q)) begin
                        need_wr   = 1'b1;
                        wr_v      = 1'b0;
                        wr_st     = DIR_I;
                        wr_sh     = '0;
                        wr_ow     = '0;
                        removed_c = 1'b1;
                    end
                end else if ((hit_st_c == DIR_U) && (hit_ow_c == port_q)) begin
                    need_wr   = 1'b1;
                    wr_v      = 1'b0;
                    wr_st     = DIR_I;
                    wr_sh     = '0;
                    wr_ow     = '0;
                    removed_c = 1'b1;
                end else if (hit_st_c == DIR_S) begin
                    wr_sh = hit_sh_c & ~(NUM_T1'(1) << port_q);
                    if (wr_sh != hit_sh_c) begin
                        need_wr   = 1'b1;
                        removed_c = 1'b1;
                        if (wr_sh == '0) begin
                            wr_v  = 1'b0;
                            wr_st = DIR_I;
                        end
                    end
                end
            end
        end
        default: ;
        endcase
    end

    logic [BANK_W-1:0] bank_upd;
    always_comb begin
        bank_upd = bank_q;
        bank_upd[wr_way*ENT_W +: ENT_W] = pack_ent(wr_v, line_q, wr_st, wr_sh, wr_ow, wr_al);
    end

    assign cmd_ready = (st == ST_IDLE) && sram_req_ready;

    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            st           <= ST_IDLE;
            op_q         <= '0;
            line_q       <= '0;
            port_q       <= '0;
            rmline_q     <= 1'b0;
            state_q      <= '0;
            sharers_q    <= '0;
            owner_q      <= '0;
            aliases_q    <= '0;
            men_q        <= 1'b0;
            mstate_q     <= '0;
            msharers_q   <= '0;
            mowner_q     <= '0;
            bank_q       <= '0;
            plru_q       <= '0;
            rsp_valid    <= 1'b0;
            rsp_hit      <= 1'b0;
            rsp_state    <= DIR_I;
            rsp_sharers  <= '0;
            rsp_owner    <= '0;
            rsp_aliases  <= '0;
            rsp_way      <= '0;
            rsp_full     <= 1'b0;
            rsp_vlive    <= 1'b0;
            rsp_vstate   <= DIR_I;
            rsp_vsharers <= '0;
            rsp_vowner   <= '0;
            rsp_vway     <= '0;
            rsp_vline    <= '0;
        end else begin
            if (rsp_valid && rsp_ready)
                rsp_valid <= 1'b0;

            unique case (st)
            ST_IDLE: begin
                if (cmd_valid && cmd_ready) begin
                    op_q      <= cmd_op;
                    line_q    <= cmd_line;
                    port_q    <= cmd_port;
                    rmline_q  <= cmd_rm_line;
                    state_q   <= cmd_state;
                    sharers_q <= cmd_sharers;
                    owner_q   <= cmd_owner;
                    aliases_q <= cmd_aliases;
                    men_q     <= cmd_men;
                    mstate_q  <= cmd_mstate;
                    msharers_q<= cmd_msharers;
                    mowner_q  <= cmd_mowner;
                    st        <= ST_WAIT;
                end
            end
            ST_WAIT: begin
                if (sram_rsp_valid) begin
                    bank_q <= sram_rsp_rdata[BANK_W-1:0];
                    plru_q <= sram_rsp_rdata[WORD_W-1:BANK_W];
                    st     <= ST_HOLD;
                end
            end
            ST_HOLD: begin
                if (need_wr) begin
                    st <= ST_WR;
                end else if (!rsp_valid || rsp_ready) begin
                    rsp_valid    <= 1'b1;
                    rsp_hit      <= (op_q == OP_REMOVE) ? removed_c : hit_c;
                    rsp_state    <= hit_c ? hit_st_c : DIR_I;
                    rsp_sharers  <= hit_c ? hit_sh_c : '0;
                    rsp_owner    <= hit_c ? hit_ow_c : '0;
                    rsp_aliases  <= hit_c ? hit_al_c : '0;
                    rsp_way      <= hit_way_c;
                    rsp_full     <= is_full;
                    rsp_vlive    <= v_live_c;
                    rsp_vstate   <= v_st_c;
                    rsp_vsharers <= v_sh_c;
                    rsp_vowner   <= v_ow_c;
                    rsp_vway     <= plru_victim_c;
                    rsp_vline    <= v_line_c;
                    st           <= ST_IDLE;
                end
            end
            ST_WR: begin
                if (sram_req_valid && sram_req_ready) begin
                    rsp_valid    <= 1'b1;
                    rsp_hit      <= (op_q == OP_GRANT) ? 1'b1 : removed_c;
                    rsp_state    <= (op_q == OP_GRANT) ? wr_st : (hit_c ? hit_st_c : DIR_I);
                    rsp_sharers  <= (op_q == OP_GRANT) ? wr_sh : (hit_c ? hit_sh_c : '0);
                    rsp_owner    <= (op_q == OP_GRANT) ? wr_ow : (hit_c ? hit_ow_c : '0);
                    rsp_aliases  <= (op_q == OP_GRANT) ? wr_al : (hit_c ? hit_al_c : '0);
                    rsp_way      <= wr_way;
                    rsp_full     <= 1'b0;
                    rsp_vlive    <= v_live_c;
                    rsp_vstate   <= v_st_c;
                    rsp_vsharers <= v_sh_c;
                    rsp_vowner   <= v_ow_c;
                    rsp_vway     <= plru_victim_c;
                    rsp_vline    <= v_line_c;
                    plru_q       <= (op_q == OP_GRANT) ? plru_next_c : plru_q;
                    st           <= ST_IDLE;
                end
            end
            default: st <= ST_IDLE;
            endcase
        end
    end

    always_comb begin
        sram_req_valid = 1'b0;
        sram_req_we    = 1'b0;
        sram_req_addr  = line_set(line_q);
        sram_req_wdata = {(op_q == OP_GRANT) ? plru_next_c : plru_q, bank_upd};

        if (st == ST_IDLE && cmd_valid) begin
            sram_req_valid = 1'b1;
            sram_req_we    = 1'b0;
            sram_req_addr  = line_set(cmd_line);
        end else if (st == ST_WR) begin
            sram_req_valid = 1'b1;
            sram_req_we    = 1'b1;
        end
    end

endmodule

`default_nettype wire
