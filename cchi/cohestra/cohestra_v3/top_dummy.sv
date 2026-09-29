`default_nettype none

`define COHESTRA_T1_PORT_DECL(P) \
    input  logic        cchi_t1p``P``_rxevt_valid, \
    input  logic [6:0]  cchi_t1p``P``_rxevt_bits_TxnID, \
    input  logic [4:0]  cchi_t1p``P``_rxevt_bits_SrcID, \
    input  logic [4:0]  cchi_t1p``P``_rxevt_bits_TgtID, \
    input  logic        cchi_t1p``P``_rxevt_bits_Opcode, \
    input  logic [47:0] cchi_t1p``P``_rxevt_bits_Addr, \
    input  logic        cchi_t1p``P``_rxevt_bits_NS, \
    input  logic        cchi_t1p``P``_rxevt_bits_MemAttr, \
    input  logic        cchi_t1p``P``_rxevt_bits_WayValid, \
    input  logic [3:0]  cchi_t1p``P``_rxevt_bits_Way, \
    input  logic        cchi_t1p``P``_rxevt_bits_TraceTag, \
    output logic        cchi_t1p``P``_rxevt_ready, \
    \
    input  logic        cchi_t1p``P``_rxreq_valid, \
    input  logic [6:0]  cchi_t1p``P``_rxreq_bits_TxnID, \
    input  logic [4:0]  cchi_t1p``P``_rxreq_bits_SrcID, \
    input  logic [4:0]  cchi_t1p``P``_rxreq_bits_TgtID, \
    input  logic [5:0]  cchi_t1p``P``_rxreq_bits_Opcode, \
    input  logic [2:0]  cchi_t1p``P``_rxreq_bits_Size, \
    input  logic [47:0] cchi_t1p``P``_rxreq_bits_Addr, \
    input  logic [7:0]  cchi_t1p``P``_rxreq_bits_TagAlias, \
    input  logic        cchi_t1p``P``_rxreq_bits_NS, \
    input  logic [1:0]  cchi_t1p``P``_rxreq_bits_Order, \
    input  logic [3:0]  cchi_t1p``P``_rxreq_bits_MemAttr, \
    input  logic        cchi_t1p``P``_rxreq_bits_Excl, \
    input  logic        cchi_t1p``P``_rxreq_bits_ExpCompData, \
    input  logic        cchi_t1p``P``_rxreq_bits_TraceTag, \
    input  logic        cchi_t1p``P``_rxreq_bits_WayValid, \
    input  logic [3:0]  cchi_t1p``P``_rxreq_bits_Way, \
    output logic        cchi_t1p``P``_rxreq_ready, \
    \
    input  logic        cchi_t1p``P``_txsnp_ready, \
    output logic        cchi_t1p``P``_txsnp_valid, \
    output logic [6:0]  cchi_t1p``P``_txsnp_bits_TxnID, \
    output logic [4:0]  cchi_t1p``P``_txsnp_bits_SrcID, \
    output logic [4:0]  cchi_t1p``P``_txsnp_bits_TgtID, \
    output logic [1:0]  cchi_t1p``P``_txsnp_bits_Opcode, \
    output logic [44:0] cchi_t1p``P``_txsnp_bits_Addr, \
    output logic        cchi_t1p``P``_txsnp_bits_NS, \
    output logic        cchi_t1p``P``_txsnp_bits_TraceTag, \
    \
    input  logic        cchi_t1p``P``_txrsp_ready, \
    output logic        cchi_t1p``P``_txrsp_valid, \
    output logic [6:0]  cchi_t1p``P``_txrsp_bits_TxnID, \
    output logic [4:0]  cchi_t1p``P``_txrsp_bits_SrcID, \
    output logic [4:0]  cchi_t1p``P``_txrsp_bits_TgtID, \
    output logic [6:0]  cchi_t1p``P``_txrsp_bits_DBID, \
    output logic [2:0]  cchi_t1p``P``_txrsp_bits_Opcode, \
    output logic [1:0]  cchi_t1p``P``_txrsp_bits_RespErr, \
    output logic [2:0]  cchi_t1p``P``_txrsp_bits_Resp, \
    output logic [2:0]  cchi_t1p``P``_txrsp_bits_CBusy, \
    output logic        cchi_t1p``P``_txrsp_bits_WayValid, \
    output logic [3:0]  cchi_t1p``P``_txrsp_bits_Way, \
    output logic        cchi_t1p``P``_txrsp_bits_TraceTag, \
    \
    input  logic        cchi_t1p``P``_rxrsp_valid, \
    input  logic [6:0]  cchi_t1p``P``_rxrsp_bits_TxnID, \
    input  logic [4:0]  cchi_t1p``P``_rxrsp_bits_SrcID, \
    input  logic [4:0]  cchi_t1p``P``_rxrsp_bits_TgtID, \
    input  logic        cchi_t1p``P``_rxrsp_bits_Opcode, \
    input  logic [1:0]  cchi_t1p``P``_rxrsp_bits_RespErr, \
    input  logic [2:0]  cchi_t1p``P``_rxrsp_bits_Resp, \
    input  logic        cchi_t1p``P``_rxrsp_bits_TraceTag, \
    output logic        cchi_t1p``P``_rxrsp_ready, \
    \
    input  logic        cchi_t1p``P``_txdat_ready, \
    output logic        cchi_t1p``P``_txdat_valid, \
    output logic [6:0]  cchi_t1p``P``_txdat_bits_TxnID, \
    output logic [4:0]  cchi_t1p``P``_txdat_bits_SrcID, \
    output logic [4:0]  cchi_t1p``P``_txdat_bits_TgtID, \
    output logic [6:0]  cchi_t1p``P``_txdat_bits_DBID, \
    output logic        cchi_t1p``P``_txdat_bits_Opcode, \
    output logic [1:0]  cchi_t1p``P``_txdat_bits_RespErr, \
    output logic [2:0]  cchi_t1p``P``_txdat_bits_Resp, \
    output logic [4:0]  cchi_t1p``P``_txdat_bits_DataSource, \
    output logic [2:0]  cchi_t1p``P``_txdat_bits_CBusy, \
    output logic        cchi_t1p``P``_txdat_bits_DataID, \
    output logic [31:0] cchi_t1p``P``_txdat_bits_Data [0:7], \
    output logic        cchi_t1p``P``_txdat_bits_WayValid, \
    output logic [3:0]  cchi_t1p``P``_txdat_bits_Way, \
    output logic        cchi_t1p``P``_txdat_bits_TraceTag, \
    \
    input  logic        cchi_t1p``P``_rxdat_valid, \
    input  logic [6:0]  cchi_t1p``P``_rxdat_bits_TxnID, \
    input  logic [4:0]  cchi_t1p``P``_rxdat_bits_SrcID, \
    input  logic [4:0]  cchi_t1p``P``_rxdat_bits_TgtID, \
    input  logic [1:0]  cchi_t1p``P``_rxdat_bits_Opcode, \
    input  logic [1:0]  cchi_t1p``P``_rxdat_bits_RespErr, \
    input  logic [2:0]  cchi_t1p``P``_rxdat_bits_Resp, \
    input  logic        cchi_t1p``P``_rxdat_bits_DataID, \
    input  logic [31:0] cchi_t1p``P``_rxdat_bits_Data [0:7], \
    input  logic [31:0] cchi_t1p``P``_rxdat_bits_BE, \
    input  logic        cchi_t1p``P``_rxdat_bits_TraceTag, \
    output logic        cchi_t1p``P``_rxdat_ready

`define COHESTRA_T1_PORT_DRIVE(P) \
    cchi_t1p``P``_rxevt_ready = 1'b1; \
    cchi_t1p``P``_rxreq_ready = 1'b1; \
    cchi_t1p``P``_rxrsp_ready = 1'b1; \
    cchi_t1p``P``_rxdat_ready = 1'b1; \
    \
    cchi_t1p``P``_txsnp_valid = cchi_t1p``P``_rxreq_valid; \
    cchi_t1p``P``_txsnp_bits_TxnID = cchi_t1p``P``_rxreq_bits_TxnID; \
    cchi_t1p``P``_txsnp_bits_SrcID = cchi_t1p``P``_rxreq_bits_SrcID; \
    cchi_t1p``P``_txsnp_bits_TgtID = cchi_t1p``P``_rxreq_bits_TgtID; \
    cchi_t1p``P``_txsnp_bits_Opcode = cchi_t1p``P``_rxreq_bits_Opcode[1:0]; \
    cchi_t1p``P``_txsnp_bits_Addr = cchi_t1p``P``_rxreq_bits_Addr[44:0]; \
    cchi_t1p``P``_txsnp_bits_NS = cchi_t1p``P``_rxreq_bits_NS; \
    cchi_t1p``P``_txsnp_bits_TraceTag = cchi_t1p``P``_rxreq_bits_TraceTag ^ cchi_t1p``P``_txsnp_ready; \
    \
    cchi_t1p``P``_txrsp_valid = cchi_t1p``P``_rxevt_valid; \
    cchi_t1p``P``_txrsp_bits_TxnID = cchi_t1p``P``_rxevt_bits_TxnID; \
    cchi_t1p``P``_txrsp_bits_SrcID = cchi_t1p``P``_rxevt_bits_SrcID; \
    cchi_t1p``P``_txrsp_bits_TgtID = cchi_t1p``P``_rxevt_bits_TgtID; \
    cchi_t1p``P``_txrsp_bits_DBID = cchi_t1p``P``_rxevt_bits_TxnID; \
    cchi_t1p``P``_txrsp_bits_Opcode = {2'b00, cchi_t1p``P``_rxevt_bits_Opcode}; \
    cchi_t1p``P``_txrsp_bits_RespErr = cchi_t1p``P``_rxrsp_bits_RespErr; \
    cchi_t1p``P``_txrsp_bits_Resp = cchi_t1p``P``_rxrsp_bits_Resp; \
    cchi_t1p``P``_txrsp_bits_CBusy = {2'b00, cchi_t1p``P``_rxrsp_valid}; \
    cchi_t1p``P``_txrsp_bits_WayValid = cchi_t1p``P``_rxevt_bits_WayValid; \
    cchi_t1p``P``_txrsp_bits_Way = cchi_t1p``P``_rxevt_bits_Way; \
    cchi_t1p``P``_txrsp_bits_TraceTag = cchi_t1p``P``_rxevt_bits_TraceTag ^ cchi_t1p``P``_rxrsp_bits_TraceTag ^ cchi_t1p``P``_txrsp_ready; \
    \
    cchi_t1p``P``_txdat_valid = cchi_t1p``P``_rxreq_valid; \
    cchi_t1p``P``_txdat_bits_TxnID = cchi_t1p``P``_rxreq_bits_TxnID; \
    cchi_t1p``P``_txdat_bits_SrcID = cchi_t1p``P``_rxreq_bits_SrcID; \
    cchi_t1p``P``_txdat_bits_TgtID = cchi_t1p``P``_rxreq_bits_TgtID; \
    cchi_t1p``P``_txdat_bits_DBID = cchi_t1p``P``_rxreq_bits_TxnID; \
    cchi_t1p``P``_txdat_bits_Opcode = cchi_t1p``P``_rxreq_bits_Opcode[0]; \
    cchi_t1p``P``_txdat_bits_RespErr = cchi_t1p``P``_rxdat_bits_RespErr; \
    cchi_t1p``P``_txdat_bits_Resp = cchi_t1p``P``_rxdat_bits_Resp; \
    cchi_t1p``P``_txdat_bits_DataSource = {4'b0000, cchi_t1p``P``_rxreq_bits_ExpCompData}; \
    cchi_t1p``P``_txdat_bits_CBusy = {2'b00, cchi_t1p``P``_rxdat_valid}; \
    cchi_t1p``P``_txdat_bits_DataID = cchi_t1p``P``_rxdat_bits_DataID; \
    for (int i = 0; i < 8; ++i) begin \
        cchi_t1p``P``_txdat_bits_Data[i] = cchi_t1p``P``_rxdat_bits_Data[i]; \
    end \
    cchi_t1p``P``_txdat_bits_WayValid = cchi_t1p``P``_rxreq_bits_WayValid; \
    cchi_t1p``P``_txdat_bits_Way = cchi_t1p``P``_rxreq_bits_Way; \
    cchi_t1p``P``_txdat_bits_TraceTag = cchi_t1p``P``_rxreq_bits_TraceTag ^ cchi_t1p``P``_rxdat_bits_TraceTag ^ cchi_t1p``P``_txdat_ready;

module top_dummy (
    input  logic clock,
    input  logic reset,
    `COHESTRA_T1_PORT_DECL(0),
    `COHESTRA_T1_PORT_DECL(1),
    `COHESTRA_T1_PORT_DECL(2),
    `COHESTRA_T1_PORT_DECL(3)
);

    always_comb begin : drive_port0
        `COHESTRA_T1_PORT_DRIVE(0)
    end

    always_comb begin : drive_port1
        `COHESTRA_T1_PORT_DRIVE(1)
    end

    always_comb begin : drive_port2
        `COHESTRA_T1_PORT_DRIVE(2)
    end

    always_comb begin : drive_port3
        `COHESTRA_T1_PORT_DRIVE(3)
    end

endmodule

`undef COHESTRA_T1_PORT_DRIVE
`undef COHESTRA_T1_PORT_DECL

`default_nettype wire
