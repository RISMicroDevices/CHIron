`default_nettype none

module venus_fifo #(
    parameter int DW    = 8,
    parameter int DEPTH = 2
) (
    input  wire             clock,
    input  wire             reset,

    input  wire             wr_valid,
    output logic            wr_ready,
    input  wire  [DW-1:0]   wr_data,

    output logic            rd_valid,
    input  wire             rd_ready,
    output logic [DW-1:0]   rd_data,

    output logic [$clog2(DEPTH+1)-1:0] count
);

    localparam int PTR_W = (DEPTH <= 1) ? 1 : $clog2(DEPTH);

    logic [DW-1:0]      mem [DEPTH];
    logic [PTR_W-1:0]   wptr;
    logic [PTR_W-1:0]   rptr;
    logic [$clog2(DEPTH+1)-1:0] cnt;

    assign count    = cnt;
    assign wr_ready = (cnt != DEPTH[$clog2(DEPTH+1)-1:0]);
    assign rd_valid = (cnt != '0);
    assign rd_data  = mem[rptr];

    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            wptr <= '0;
            rptr <= '0;
            cnt  <= '0;
        end else begin
            if (wr_valid && wr_ready && rd_valid && rd_ready) begin
                mem[wptr] <= wr_data;
                wptr      <= (wptr == PTR_W'(DEPTH-1)) ? '0 : PTR_W'(wptr + 1'b1);
                rptr      <= (rptr == PTR_W'(DEPTH-1)) ? '0 : PTR_W'(rptr + 1'b1);
            end else if (wr_valid && wr_ready) begin
                mem[wptr] <= wr_data;
                wptr      <= (wptr == PTR_W'(DEPTH-1)) ? '0 : PTR_W'(wptr + 1'b1);
                cnt       <= cnt + 1'b1;
            end else if (rd_valid && rd_ready) begin
                rptr      <= (rptr == PTR_W'(DEPTH-1)) ? '0 : PTR_W'(rptr + 1'b1);
                cnt       <= cnt - 1'b1;
            end
        end
    end

endmodule

`default_nettype wire
