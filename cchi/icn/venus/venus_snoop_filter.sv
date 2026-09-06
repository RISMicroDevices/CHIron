`default_nettype none

import venus_pkg::*;

// ============================================================================
// venus_snoop_filter — presence filter for snoop-mask over-inclusion control.
//
// Tracks, per line, a conservative superset of the upstream nodes that may hold
// the line: exact on UPDATE (rebuilt from the directory commit image), lossy on
// replacement overflow (per-set maybe_all flag instead of an entry). May be
// conservative (extra 1s cause spurious snoops, answered SnpResp_I) but never
// clears a live holder. Enabled per-transaction by SF_ENABLE; mask composition
// and the SF_ALLOW_OVER/SF_BROADCAST policies live in venus_mshr (PROTOCOL §7.1).
//
// Command contract (single outstanding; storage via venus_sram_box):
//   OP 0 LOOKUP    : read; report hit presence + maybe_all for the set.
//   OP 1 UPDATE    : write cmd_presence for cmd_line (exact rebuild; entry
//                    dropped when presence==0; clears maybe_all[set]; on way
//                    overflow sets maybe_all[set] instead of an entry).
//   OP 2 REMOVE    : clear cmd_port's presence bit; drop entry if it empties.
//   OP 3 SET_MAYBE : set maybe_all[set] (spare; unused by the tracker today).
// ============================================================================

module venus_snoop_filter #(
    parameter int NUM_T1        = 4,
    parameter int SETS          = 64,
    parameter int WAYS          = 4,
    parameter int STORAGE       = 0,
    parameter int SRAM_LAT      = 1,
    parameter int SRAM_INTERVAL = 1,
    parameter int SRAM_PERIOD   = 1,
    parameter int SRAM_SLOT     = 0,
    localparam int PORT_W       = clog2_min1(NUM_T1)
) (
    input  wire                         clock,
    input  wire                         reset,

    input  wire                         cmd_valid,
    output logic                        cmd_ready,
    input  wire  [1:0]                  cmd_op,
    input  wire  [LINE_W-1:0]           cmd_line,
    input  wire  [NUM_T1-1:0]           cmd_presence,
    input  wire  [PORT_W-1:0]           cmd_port,

    output logic                        rsp_valid,
    input  wire                         rsp_ready,
    output logic                        rsp_hit,
    output logic [NUM_T1-1:0]           rsp_presence,
    output logic                        rsp_maybe
);

    localparam int SET_W   = $clog2(SETS);
    localparam int WAY_IW  = $clog2(WAYS);
    localparam int ENT_W   = 1 + LINE_W + NUM_T1;
    localparam int BANK_W  = WAYS * ENT_W;

    localparam logic [1:0] OP_LOOKUP    = 2'd0;
    localparam logic [1:0] OP_UPDATE    = 2'd1;
    localparam logic [1:0] OP_REMOVE    = 2'd2;
    localparam logic [1:0] OP_SET_MAYBE = 2'd3;

    typedef enum logic [1:0] {
        ST_IDLE = 2'd0,
        ST_WAIT = 2'd1,
        ST_HOLD = 2'd2,
        ST_WR   = 2'd3
    } st_e;

    st_e                    st;
    logic [1:0]             op_q;
    logic [LINE_W-1:0]      line_q;
    logic [NUM_T1-1:0]      pres_q;
    logic [PORT_W-1:0]      port_q;
    logic [BANK_W-1:0]      bank_q;

    logic [SETS-1:0]        maybe_all;

    logic                   sram_req_valid;
    logic                   sram_req_ready;
    logic                   sram_req_we;
    logic [SET_W-1:0]       sram_req_addr;
    logic [BANK_W-1:0]      sram_req_wdata;
    logic                   sram_rsp_valid;
    logic [BANK_W-1:0]      sram_rsp_rdata;

    venus_sram_box #(
        .DEPTH    (SETS),
        .DW       (BANK_W),
        .STORAGE  (STORAGE),
        .LAT      (SRAM_LAT),
        .INTERVAL (SRAM_INTERVAL),
        .PERIOD   (SRAM_PERIOD),
        .SLOT     (SRAM_SLOT)
    ) u_box (
        .clock        (clock),
        .reset      (reset),
        .req_valid  (sram_req_valid),
        .req_ready  (sram_req_ready),
        .req_we     (sram_req_we),
        .req_addr   (sram_req_addr),
        .req_wdata  (sram_req_wdata),
        .rsp_valid  (sram_rsp_valid),
        .rsp_ready  (1'b1),
        .rsp_rdata  (sram_rsp_rdata)
    );

    function automatic logic [SET_W-1:0] line_set(input logic [LINE_W-1:0] line);
        return line[SET_W-1:0];
    endfunction

    logic                 hit_c;
    logic [WAY_IW-1:0]    hit_way_c;
    logic [NUM_T1-1:0]    hit_pr_c;
    logic                 free_c;
    logic [WAY_IW-1:0]    free_way_c;

    always_comb begin
        hit_c      = 1'b0;
        hit_way_c  = '0;
        hit_pr_c   = '0;
        free_c     = 1'b0;
        free_way_c = '0;

        for (int w = 0; w < WAYS; w++) begin
            logic [ENT_W-1:0]  e;
            logic              v;
            logic [LINE_W-1:0] tag;
            logic [NUM_T1-1:0] pr;
            e = bank_q[w*ENT_W +: ENT_W];
            {v, tag, pr} = e;
            if (v && (tag == line_q)) begin
                hit_c     = 1'b1;
                hit_way_c = WAY_IW'(w);
                hit_pr_c  = pr;
            end
            if (!v && !free_c) begin
                free_c     = 1'b1;
                free_way_c = WAY_IW'(w);
            end
        end
    end

    logic [WAY_IW-1:0] wr_way;
    logic [NUM_T1-1:0] wr_pr;
    logic              wr_v;
    logic              need_wr;
    logic              set_maybe;

    always_comb begin
        wr_way    = hit_way_c;
        wr_pr     = hit_pr_c;
        wr_v      = hit_c;
        need_wr   = 1'b0;
        set_maybe = 1'b0;

        unique case (op_q)
        OP_LOOKUP: need_wr = 1'b0;
        OP_UPDATE: begin
            need_wr = 1'b1;
            wr_pr   = pres_q;
            wr_v    = (pres_q != '0);
            if (hit_c)
                wr_way = hit_way_c;
            else if (free_c)
                wr_way = free_way_c;
            else begin
                wr_way    = '0;
                set_maybe = 1'b1;
                wr_v      = 1'b0;
            end
        end
        OP_REMOVE: begin
            if (hit_c) begin
                wr_pr   = hit_pr_c & ~(NUM_T1'(1) << port_q);
                need_wr = 1'b1;
                wr_v    = (wr_pr != '0);
            end
        end
        OP_SET_MAYBE: begin
            set_maybe = 1'b1;
            need_wr   = 1'b0;
        end
        default: ;
        endcase
    end

    logic [BANK_W-1:0] bank_upd;
    always_comb begin
        bank_upd = bank_q;
        bank_upd[wr_way*ENT_W +: ENT_W] = {wr_v, line_q, wr_pr};
    end

    assign cmd_ready = (st == ST_IDLE) && sram_req_ready;

    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            st           <= ST_IDLE;
            op_q         <= '0;
            line_q       <= '0;
            pres_q       <= '0;
            port_q       <= '0;
            bank_q       <= '0;
            maybe_all    <= '0;
            rsp_valid    <= 1'b0;
            rsp_hit      <= 1'b0;
            rsp_presence <= '0;
            rsp_maybe    <= 1'b0;
        end else begin
            if (rsp_valid && rsp_ready)
                rsp_valid <= 1'b0;

            unique case (st)
            ST_IDLE: begin
                if (cmd_valid && cmd_ready) begin
                    op_q   <= cmd_op;
                    line_q <= cmd_line;
                    pres_q <= cmd_presence;
                    port_q <= cmd_port;
                    st     <= ST_WAIT;
                end
            end
            ST_WAIT: begin
                if (sram_rsp_valid) begin
                    bank_q <= sram_rsp_rdata;
                    st     <= ST_HOLD;
                end
            end
            ST_HOLD: begin
                if (set_maybe)
                    maybe_all[line_set(line_q)] <= 1'b1;

                if (need_wr && !set_maybe) begin
                    st <= ST_WR;
                end else if (!rsp_valid || rsp_ready) begin
                    rsp_valid    <= 1'b1;
                    rsp_hit      <= hit_c;
                    rsp_presence <= hit_c ? hit_pr_c : '0;
                    rsp_maybe    <= maybe_all[line_set(line_q)] || set_maybe;
                    if (op_q == OP_UPDATE && !set_maybe)
                        maybe_all[line_set(line_q)] <= 1'b0;
                    st <= ST_IDLE;
                end
            end
            ST_WR: begin
                if (sram_req_valid && sram_req_ready) begin
                    rsp_valid    <= 1'b1;
                    rsp_hit      <= wr_v;
                    rsp_presence <= wr_pr;
                    rsp_maybe    <= maybe_all[line_set(line_q)];
                    if (op_q == OP_UPDATE)
                        maybe_all[line_set(line_q)] <= 1'b0;
                    st <= ST_IDLE;
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
        sram_req_wdata = bank_upd;

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
