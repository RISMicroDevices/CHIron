`default_nettype none

// ============================================================================
// venus_id_alloc — lowest-free-ID bitmap allocator.
//
// One instance per upstream port; the instance's ID space is shared between
// home-allocated snoop TxnIDs and DBIDs (PROTOCOL.md §6). alloc_req is granted
// combinationally (alloc_gnt) when a free ID exists; avail exposes the same
// information without requesting, for top-level grant-condition composition.
// The ID is marked used on alloc_gnt and freed via the two free ports (the
// protocol guarantees at most one UpRSP-side and one UpDAT-side free per cycle
// per port — see PROTOCOL.md §6).
// ============================================================================

module venus_id_alloc #(
    parameter int N = 128
) (
    input  wire                 clock,
    input  wire                 reset,

    input  wire                 alloc_req,
    output logic                alloc_gnt,
    output logic                avail,
    output logic [$clog2(N)-1:0] alloc_id,

    input  wire                 free0_req,
    input  wire  [$clog2(N)-1:0] free0_id,
    input  wire                 free1_req,
    input  wire  [$clog2(N)-1:0] free1_id
);

    logic [N-1:0] used;

    logic                 found;
    logic [$clog2(N)-1:0] found_id;

    always_comb begin
        found    = 1'b0;
        found_id = '0;
        for (int i = 0; i < N; i++) begin
            if (!found && !used[i]) begin
                found    = 1'b1;
                found_id = $clog2(N)'(i);
            end
        end
    end

    assign alloc_gnt = alloc_req && found;
    assign avail     = found;
    assign alloc_id  = found_id;

    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            used <= '0;
        end else begin
            if (alloc_gnt)
                used[found_id] <= 1'b1;
            if (free0_req)
                used[free0_id] <= 1'b0;
            if (free1_req)
                used[free1_id] <= 1'b0;
        end
    end

endmodule

`default_nettype wire
