`ifndef VENUS_SRAM_IF_SV
`define VENUS_SRAM_IF_SV

// Replaceable 1R1W SRAM handshake used by directory and snoop-filter arrays.
// A user SRAM adapter must present the same req/rsp timing contract:
//   * req is accepted when req_valid && req_ready
//   * a read (req_we=0) produces exactly one rsp_valid beat LAT cycles later
//   * a write (req_we=1) has no response
// Venus never assumes combinational read data.

interface venus_sram_if #(
    parameter int AW = 6,
    parameter int DW = 128
);
    logic           req_valid;
    logic           req_ready;
    logic           req_we;
    logic [AW-1:0]  req_addr;
    logic [DW-1:0]  req_wdata;

    logic           rsp_valid;
    logic           rsp_ready;
    logic [DW-1:0]  rsp_rdata;

    modport master (
        output req_valid, req_we, req_addr, req_wdata, rsp_ready,
        input  req_ready, rsp_valid, rsp_rdata
    );

    modport slave (
        input  req_valid, req_we, req_addr, req_wdata, rsp_ready,
        output req_ready, rsp_valid, rsp_rdata
    );
endinterface

`endif
