`default_nettype none

// REG flop array or a timed behavioral 1R1W SRAM.
// STORAGE=0 (REG): req_ready=1, read data the next cycle.
// STORAGE=1 (SRAM): honours LAT / INTERVAL / PERIOD+SLOT availability.

module venus_sram_box #(
    parameter int DEPTH     = 64,
    parameter int DW        = 128,
    parameter int STORAGE   = 0,
    parameter int LAT       = 1,
    parameter int INTERVAL  = 1,
    parameter int PERIOD    = 1,
    parameter int SLOT      = 0
) (
    input  wire                     clock,
    input  wire                     reset,

    input  wire                     req_valid,
    output logic                    req_ready,
    input  wire                     req_we,
    input  wire  [$clog2(DEPTH)-1:0] req_addr,
    input  wire  [DW-1:0]           req_wdata,

    output logic                    rsp_valid,
    input  wire                     rsp_ready,
    output logic [DW-1:0]           rsp_rdata
);

    localparam int AW     = $clog2(DEPTH);
    localparam int LAT_W  = (LAT < 1) ? 1 : $clog2(LAT + 1);
    localparam int INT_W  = (INTERVAL < 1) ? 1 : $clog2(INTERVAL + 1);
    localparam int PER_W  = (PERIOD < 1) ? 1 : $clog2(PERIOD + 1);

    logic [DW-1:0] mem [DEPTH];

    if (STORAGE == 0) begin : g_reg
        logic           pend_valid;
        logic [DW-1:0]  pend_data;
        integer         mi;

        assign req_ready = 1'b1;

        always_ff @(posedge clock or posedge reset) begin
            if (reset) begin
                pend_valid <= 1'b0;
                pend_data  <= '0;
                for (mi = 0; mi < DEPTH; mi++)
                    mem[mi] <= '0;
            end else begin
                if (req_valid && req_ready) begin
                    if (req_we) begin
                        mem[req_addr] <= req_wdata;
                        pend_valid    <= 1'b0;
                    end else begin
                        pend_valid <= 1'b1;
                        pend_data  <= mem[req_addr];
                    end
                end else if (rsp_valid && rsp_ready) begin
                    pend_valid <= 1'b0;
                end
            end
        end

        assign rsp_valid = pend_valid;
        assign rsp_rdata = pend_data;
    end else begin : g_sram
        logic [PER_W-1:0]   cyc;
        logic [INT_W-1:0]   cool;
        logic [LAT:1]       pipe_v;
        logic [DW-1:0]      pipe_d [1:LAT];

        logic avail;
        logic cooling;

        assign avail   = (PERIOD <= 1) || (cyc == PER_W'(SLOT));
        assign cooling = (INTERVAL > 1) && (cool != '0);
        assign req_ready = avail && !cooling;

        integer li;
        integer mi;

        always_ff @(posedge clock or posedge reset) begin
            if (reset) begin
                cyc    <= '0;
                cool   <= '0;
                pipe_v <= '0;
                for (li = 1; li <= LAT; li++)
                    pipe_d[li] <= '0;
                for (mi = 0; mi < DEPTH; mi++)
                    mem[mi] <= '0;
            end else begin
                if (PERIOD > 1)
                    cyc <= (cyc == PER_W'(PERIOD-1)) ? '0 : PER_W'(cyc + 1'b1);

                if (req_valid && req_ready)
                    cool <= (INTERVAL > 1) ? INT_W'(INTERVAL-1) : '0;
                else if (cool != '0)
                    cool <= INT_W'(cool - 1'b1);

                if (LAT == 1) begin
                    if (req_valid && req_ready && !req_we) begin
                        pipe_v[1] <= 1'b1;
                        pipe_d[1] <= mem[req_addr];
                    end else begin
                        pipe_v[1] <= 1'b0;
                    end
                end else begin
                    pipe_v[1] <= (req_valid && req_ready && !req_we);
                    pipe_d[1] <= mem[req_addr];
                    for (li = 2; li <= LAT; li++) begin
                        pipe_v[li] <= pipe_v[li-1];
                        pipe_d[li] <= pipe_d[li-1];
                    end
                end

                if (req_valid && req_ready && req_we)
                    mem[req_addr] <= req_wdata;
            end
        end

        assign rsp_valid = pipe_v[LAT];
        assign rsp_rdata = pipe_d[LAT];
    end

endmodule

`default_nettype wire
