`default_nettype none

import venus_pkg::*;

// ============================================================================
// venus_axi_map — line -> AXI port hash (multi-channel memory striping).
//
// XOR-folds the line address into the select width, then takes low bits
// (power-of-two NUM_AXI) or modulo. The same line always hashes to the same
// port, so all traffic for a line stays ordered on one AXI channel.
// Replace this module (same ports) to install a custom map.
// ============================================================================

module venus_axi_map #(
    parameter int NUM_AXI = 1,
    parameter int LINE_W  = venus_pkg::LINE_W
) (
    input  wire  [LINE_W-1:0]                 line,
    output logic [$clog2(NUM_AXI > 1 ? NUM_AXI : 2)-1:0] axi_sel
);

    localparam int SEL_W = $clog2(NUM_AXI > 1 ? NUM_AXI : 2);

    logic [SEL_W-1:0] folded;
    integer i;

    always_comb begin
        folded = '0;
        for (i = 0; i < LINE_W; i++)
            folded[i % SEL_W] = folded[i % SEL_W] ^ line[i];

        if (NUM_AXI <= 1)
            axi_sel = '0;
        else if ((NUM_AXI & (NUM_AXI - 1)) == 0)
            axi_sel = folded;
        else
            axi_sel = SEL_W'(folded % NUM_AXI);
    end

endmodule

`default_nettype wire
