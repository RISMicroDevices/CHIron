`default_nettype none

import venus_pkg::*;

// ============================================================================
// venus_axi_master — one AXI4 line/beat read-write master.
//
// One outstanding transaction at a time, one instance per AXI port. 256-bit
// data, INCR bursts, AxSIZE=5 (32 B beats). Both whole-line (2-beat) and
// sub-line (1-beat) transfers are supported for the Size-aware NoSnp/Ptl flows
// (TRANSACTIONS.md §7.5):
//
//   rd_req/rd_gnt : start a read of (rd_len+1) beats at rd_addr, filling the
//                   line image starting at beat index rd_start (rd_start is
//                   Addr[5] for sub-line reads, 0 for line fills).
//   wr_req/wr_gnt : start a write of (wr_len+1) beats at wr_addr, draining the
//                   line image from beat index wr_start; wr_strb is the
//                   per-byte strobe for the whole line image (unused halves
//                   carry zero strobes).
//   rd_done/wr_done pulse for one cycle at completion (after RLAST / B).
//   rd_gnt/wr_gnt are combinational; a requester must hold its request until
//   granted. Read wins when both are requested in the same idle cycle.
//
// Addresses are used exactly as given (alignment is the tracker's job).
// AxID is passed through from rd_id/wr_id.
// ============================================================================

module venus_axi_master #(
    parameter int ADDR_W = 48,
    parameter int DATA_W = 256,
    parameter int ID_W   = 4
) (
    input  wire                     clock,
    input  wire                     reset,

    input  wire                     rd_req,
    output logic                    rd_gnt,
    input  wire  [ADDR_W-1:0]       rd_addr,
    input  wire  [ID_W-1:0]         rd_id,
    input  wire  [7:0]              rd_len,     // AxLEN: 0 = one beat, 1 = two beats
    input  wire                     rd_start,   // first beat index (Addr[5])
    output logic                    rd_done,
    output logic [LINE_BITS-1:0]    rd_data,

    input  wire                     wr_req,
    output logic                    wr_gnt,
    input  wire  [ADDR_W-1:0]       wr_addr,
    input  wire  [ID_W-1:0]         wr_id,
    input  wire  [7:0]              wr_len,     // AxLEN
    input  wire                     wr_start,   // first beat index
    input  wire  [LINE_BITS-1:0]    wr_data,
    input  wire  [LINE_BYTES-1:0]   wr_strb,
    output logic                    wr_done,

    output logic                    axi_m_awvalid,
    input  wire                     axi_m_awready,
    output logic [ADDR_W-1:0]       axi_m_awaddr,
    output logic [ID_W-1:0]         axi_m_awid,
    output logic [7:0]              axi_m_awlen,
    output logic [2:0]              axi_m_awsize,
    output logic [1:0]              axi_m_awburst,

    output logic                    axi_m_wvalid,
    input  wire                     axi_m_wready,
    output logic [DATA_W-1:0]       axi_m_wdata,
    output logic [DATA_W/8-1:0]     axi_m_wstrb,
    output logic                    axi_m_wlast,

    input  wire                     axi_m_bvalid,
    output logic                    axi_m_bready,
    input  wire  [ID_W-1:0]         axi_m_bid,
    input  wire  [1:0]              axi_m_bresp,

    output logic                    axi_m_arvalid,
    input  wire                     axi_m_arready,
    output logic [ADDR_W-1:0]       axi_m_araddr,
    output logic [ID_W-1:0]         axi_m_arid,
    output logic [7:0]              axi_m_arlen,
    output logic [2:0]              axi_m_arsize,
    output logic [1:0]              axi_m_arburst,

    input  wire                     axi_m_rvalid,
    output logic                    axi_m_rready,
    input  wire  [DATA_W-1:0]       axi_m_rdata,
    input  wire  [ID_W-1:0]         axi_m_rid,
    input  wire  [1:0]              axi_m_rresp,
    input  wire                     axi_m_rlast
);

    localparam int BEAT_W = DATA_W;
    localparam int STRB_W = DATA_W / 8;

    typedef enum logic [2:0] {
        S_IDLE  = 3'd0,
        S_AR    = 3'd1,
        S_R     = 3'd2,
        S_AW    = 3'd3,
        S_W     = 3'd4,
        S_B     = 3'd5
    } st_e;

    st_e                    st;
    logic [ADDR_W-1:0]      addr_q;
    logic [ID_W-1:0]        id_q;
    logic [LINE_BITS-1:0]   data_q;
    logic [LINE_BYTES-1:0]  strb_q;
    logic [7:0]             len_q;
    logic                   beat;        // current beat index (starts at rd_start/wr_start)
    logic                   last_beat_q; // final beat index (start + len)

    // Grant exactly when the transaction will be accepted in S_IDLE (the
    // !rd_done/!wr_done terms mirror the accept guard below: rd_done/wr_done
    // are one-cycle pulses in the IDLE cycle right after a completion, and a
    // request arriving in that window must NOT see a spurious grant).
    // Read wins when both are requested while idle: rd_gnt must NOT depend on
    // wr_req - requesters hold their request until granted, so mutual
    // exclusion terms (!wr_req/!rd_req) on both grants would livelock: each
    // grant would wait forever for the other request to drop.
    assign rd_gnt  = (st == S_IDLE) && rd_req && !rd_done;
    assign wr_gnt  = (st == S_IDLE) && wr_req && !rd_gnt && !wr_done;

    assign axi_m_awaddr  = addr_q;
    assign axi_m_awid    = id_q;
    assign axi_m_awlen   = len_q;
    assign axi_m_awsize  = 3'd5;
    assign axi_m_awburst = 2'b01;

    assign axi_m_araddr  = addr_q;
    assign axi_m_arid    = id_q;
    assign axi_m_arlen   = len_q;
    assign axi_m_arsize  = 3'd5;
    assign axi_m_arburst = 2'b01;

    assign axi_m_wdata = beat ? data_q[2*BEAT_W-1:BEAT_W] : data_q[BEAT_W-1:0];
    assign axi_m_wstrb = beat ? strb_q[2*STRB_W-1:STRB_W] : strb_q[STRB_W-1:0];
    assign axi_m_wlast = (beat == last_beat_q);

    assign axi_m_bready = (st == S_B);
    assign axi_m_rready = (st == S_R);

    assign axi_m_awvalid = (st == S_AW);
    assign axi_m_wvalid  = (st == S_W);
    assign axi_m_arvalid = (st == S_AR);

    assign rd_data = data_q;

    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            st         <= S_IDLE;
            addr_q     <= '0;
            id_q       <= '0;
            data_q     <= '0;
            strb_q     <= '0;
            len_q      <= '0;
            beat       <= 1'b0;
            last_beat_q <= 1'b0;
            rd_done    <= 1'b0;
            wr_done    <= 1'b0;
        end else begin
            rd_done <= 1'b0;
            wr_done <= 1'b0;

            unique case (st)
            S_IDLE: begin
                if (rd_gnt) begin
                    addr_q     <= rd_addr;
                    id_q       <= rd_id;
                    len_q      <= rd_len;
                    beat       <= rd_start;
                    last_beat_q <= rd_start + rd_len[0];
                    st         <= S_AR;
                end else if (wr_gnt) begin
                    addr_q     <= wr_addr;
                    id_q       <= wr_id;
                    len_q      <= wr_len;
                    data_q     <= wr_data;
                    strb_q     <= wr_strb;
                    beat       <= wr_start;
                    last_beat_q <= wr_start + wr_len[0];
                    st         <= S_AW;
                end
            end
            S_AR: begin
                if (axi_m_arvalid && axi_m_arready)
                    st <= S_R;
            end
            S_R: begin
                if (axi_m_rvalid && axi_m_rready) begin
                    if (!beat)
                        data_q[BEAT_W-1:0] <= axi_m_rdata;
                    else
                        data_q[2*BEAT_W-1:BEAT_W] <= axi_m_rdata;
                    if (axi_m_rlast || (beat == last_beat_q)) begin
                        rd_done <= 1'b1;
                        st      <= S_IDLE;
                        beat    <= 1'b0;
                    end else begin
                        beat <= beat + 1'b1;
                    end
                end
            end
            S_AW: begin
                if (axi_m_awvalid && axi_m_awready)
                    st <= S_W;
            end
            S_W: begin
                if (axi_m_wvalid && axi_m_wready) begin
                    if (axi_m_wlast) begin
                        st   <= S_B;
                        beat <= 1'b0;
                    end else begin
                        beat <= beat + 1'b1;
                    end
                end
            end
            S_B: begin
                if (axi_m_bvalid && axi_m_bready) begin
                    wr_done <= 1'b1;
                    st      <= S_IDLE;
                end
            end
            default: st <= S_IDLE;
            endcase
        end
    end

endmodule

`default_nettype wire
