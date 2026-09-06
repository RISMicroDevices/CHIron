`default_nettype none

// Non-synthesizable sparse AXI4 slave. Zero-fills untouched lines.

module venus_axi_mem #(
    parameter int ADDR_W = 48,
    parameter int DATA_W = 256,
    parameter int ID_W   = 4,
    parameter int LAT    = 2
) (
    input  wire                     clock,
    input  wire                     reset,

    input  wire                     axi_m_awvalid,
    output logic                    axi_m_awready,
    input  wire  [ADDR_W-1:0]       axi_m_awaddr,
    input  wire  [ID_W-1:0]         axi_m_awid,
    input  wire  [7:0]              axi_m_awlen,
    input  wire  [2:0]              axi_m_awsize,
    input  wire  [1:0]              axi_m_awburst,

    input  wire                     axi_m_wvalid,
    output logic                    axi_m_wready,
    input  wire  [DATA_W-1:0]       axi_m_wdata,
    input  wire  [DATA_W/8-1:0]     axi_m_wstrb,
    input  wire                     axi_m_wlast,

    output logic                    axi_m_bvalid,
    input  wire                     axi_m_bready,
    output logic [ID_W-1:0]         axi_m_bid,
    output logic [1:0]              axi_m_bresp,

    input  wire                     axi_m_arvalid,
    output logic                    axi_m_arready,
    input  wire  [ADDR_W-1:0]       axi_m_araddr,
    input  wire  [ID_W-1:0]         axi_m_arid,
    input  wire  [7:0]              axi_m_arlen,
    input  wire  [2:0]              axi_m_arsize,
    input  wire  [1:0]              axi_m_arburst,

    output logic                    axi_m_rvalid,
    input  wire                     axi_m_rready,
    output logic [DATA_W-1:0]       axi_m_rdata,
    output logic [ID_W-1:0]         axi_m_rid,
    output logic [1:0]              axi_m_rresp,
    output logic                    axi_m_rlast
);

    import venus_pkg::*;

    typedef logic [LINE_BITS-1:0] line_t;
    line_t mem [logic [LINE_W-1:0]];

    typedef enum logic [2:0] {
        S_IDLE = 3'd0,
        S_W    = 3'd1,
        S_B    = 3'd2,
        S_R    = 3'd3
    } st_e;

    st_e                    st;
    logic [ADDR_W-1:0]      addr_q;
    logic [ID_W-1:0]        id_q;
    logic [7:0]             len_q;
    logic                   beat;
    logic [LAT:0]           wait_c;
    logic [LINE_BITS-1:0]   line_q;
    logic [LINE_W-1:0]      key;

    assign key = addr_q[ADDR_W-1:LINE_ADDR_LSB];

    assign axi_m_awready = (st == S_IDLE);
    assign axi_m_arready = (st == S_IDLE) && !axi_m_awvalid;
    assign axi_m_wready  = (st == S_W);
    assign axi_m_bvalid  = (st == S_B);
    assign axi_m_bresp   = 2'b00;
    assign axi_m_bid     = id_q;
    assign axi_m_rvalid  = (st == S_R);
    assign axi_m_rresp   = 2'b00;
    assign axi_m_rid     = id_q;
    // Honour the burst length: the last beat is beat index arlen, not always 1.
    assign axi_m_rlast   = (beat == len_q[0]);
    assign axi_m_rdata   = beat ? line_q[2*DATA_W-1:DATA_W] : line_q[DATA_W-1:0];

    integer b;

    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            st     <= S_IDLE;
            addr_q <= '0;
            id_q   <= '0;
            len_q  <= '0;
            beat   <= 1'b0;
            wait_c <= '0;
            line_q <= '0;
        end else begin
            unique case (st)
            S_IDLE: begin
                beat   <= 1'b0;
                wait_c <= '0;
                if (axi_m_awvalid && axi_m_awready) begin
                    addr_q <= axi_m_awaddr;
                    id_q   <= axi_m_awid;
                    len_q  <= axi_m_awlen;
                    if (mem.exists(axi_m_awaddr[ADDR_W-1:LINE_ADDR_LSB]))
                        line_q <= mem[axi_m_awaddr[ADDR_W-1:LINE_ADDR_LSB]];
                    else
                        line_q <= '0;
                    st <= S_W;
                end else if (axi_m_arvalid && axi_m_arready) begin
                    addr_q <= axi_m_araddr;
                    id_q   <= axi_m_arid;
                    len_q  <= axi_m_arlen;
                    if (mem.exists(axi_m_araddr[ADDR_W-1:LINE_ADDR_LSB]))
                        line_q <= mem[axi_m_araddr[ADDR_W-1:LINE_ADDR_LSB]];
                    else
                        line_q <= '0;
                    st <= S_R;
                end
            end
            S_W: begin
                if (axi_m_wvalid && axi_m_wready) begin
                    if (!beat) begin
                        for (b = 0; b < DATA_W/8; b++)
                            if (axi_m_wstrb[b])
                                line_q[b*8 +: 8] <= axi_m_wdata[b*8 +: 8];
                    end else begin
                        for (b = 0; b < DATA_W/8; b++)
                            if (axi_m_wstrb[b])
                                line_q[DATA_W + b*8 +: 8] <= axi_m_wdata[b*8 +: 8];
                    end
                    if (axi_m_wlast) begin
                        st   <= S_B;
                        beat <= 1'b0;
                    end else begin
                        beat <= 1'b1;
                    end
                end
            end
            S_B: begin
                mem[key] = line_q;
                if (axi_m_bvalid && axi_m_bready)
                    st <= S_IDLE;
            end
            S_R: begin
                if (axi_m_rvalid && axi_m_rready) begin
                    if (axi_m_rlast)
                        st <= S_IDLE;
                    else
                        beat <= 1'b1;
                end
            end
            default: st <= S_IDLE;
            endcase
        end
    end

endmodule

`default_nettype wire
