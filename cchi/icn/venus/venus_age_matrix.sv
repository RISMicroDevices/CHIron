`default_nettype none

// ============================================================================
// venus_age_matrix — transaction-age registry for oldest-first arbitration.
//
// One shared age matrix over the PARALLELISM tracker slots (slot birth =
// admission), serving every slot-based arbiter in venus.sv (directory, snoop
// filter, per-channel AXI read/write, SNP/DnRSP/DnDAT emission) when the
// AGE_MATRIX parameter is set. Mirrors oceanus's NCBTransactionAgeMatrix:
// upper-triangle age registers, a birth strobe makes the newborn slot the
// youngest, and each select port combinationally grants the oldest requester
// as a one-hot vector.
//
// Semantics:
//   * age[i][j] (i < j) reads "slot i is older than slot j"; the diagonal
//     reads 1 and the lower triangle reads the complement of the mirrored
//     upper bit, so older() is a total order over the slots.
//   * Birth strobe k: column k is set (everyone older than k), row k is
//     cleared (k older than nobody) — k becomes youngest.
//   * Select: gnt(i) = req(i) AND (for all j: older(i,j) OR !req(j)) —
//     exactly one winner among the requesters, the oldest.
// Slots assert no requests while idle (phase-FSM driven), so stale age state
// for unallocated slots never participates; a slot's first request happens
// at earliest the cycle after its birth edge, when the matrix has absorbed it.
// ============================================================================

module venus_age_matrix #(
    parameter int N    = 8,   // tracker slots (PARALLELISM)
    parameter int NSEL = 1    // arbiter select ports sharing the matrix
) (
    input  wire              clock,
    input  wire              reset,
    input  wire              alloc_v,        // slot birth strobe (<=1/cycle)
    input  wire  [N-1:0]     alloc_oh,       // one-hot newborn slot
    input  wire  [NSEL*N-1:0] req,           // per-arbiter request vectors
    output logic [NSEL*N-1:0] gnt            // per-arbiter one-hot oldest
);

    // Upper-triangle age registers only (N*(N-1)/2 flops; 28 at N=8); entries
    // with i >= j are never read (older() folds them into the upper triangle).
    logic age [N][N];

    function automatic logic older(input int i, input int j);
        if (i == j)     return 1'b1;
        else if (i < j) return age[i][j];
        else            return ~age[j][i];
    endfunction

    // Birth: the newborn slot becomes the youngest of the total order.
    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            for (int i = 0; i < N; i++)
                for (int j = i + 1; j < N; j++)
                    age[i][j] <= 1'b0;
        end else if (alloc_v) begin
            for (int k = 0; k < N; k++) begin
                if (alloc_oh[k]) begin
                    for (int i = 0; i < k; i++)
                        age[i][k] <= 1'b1;   // everyone older than k
                    for (int j = k + 1; j < N; j++)
                        age[k][j] <= 1'b0;   // k older than nobody
                end
            end
        end
    end

    // Combinational oldest-requester one-hot select per arbiter port.
    always_comb begin
        for (int s = 0; s < NSEL; s++) begin
            for (int i = 0; i < N; i++) begin
                logic win;
                win = req[s*N + i];
                for (int j = 0; j < N; j++) begin
                    if ((j != i) && req[s*N + j] && !older(i, j))
                        win = 1'b0;
                end
                gnt[s*N + i] = win;
            end
        end
    end

`ifndef SYNTHESIS
    always_ff @(posedge clock) begin
        if (!reset) begin
            for (int s = 0; s < NSEL; s++) begin
                if (!$onehot0(gnt[s*N +: N]))
                    $error("venus_age_matrix: grant port %0d not one-hot-or-zero (%b)",
                           s, gnt[s*N +: N]);
                if ((gnt[s*N +: N] & ~req[s*N +: N]) != '0)
                    $error("venus_age_matrix: grant port %0d not a subset of req (gnt %b req %b)",
                           s, gnt[s*N +: N], req[s*N +: N]);
            end
            if (alloc_v && !$onehot(alloc_oh))
                $error("venus_age_matrix: alloc_oh not one-hot (%b)", alloc_oh);
        end
    end
`endif

endmodule

`default_nettype wire
