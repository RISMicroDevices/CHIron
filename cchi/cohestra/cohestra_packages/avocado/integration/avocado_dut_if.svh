//*****************************************************************************
// avocado_dut_if.svh — the Avocado DUT interface contract (single source).
//
// A user-supplied DUT plugging into avocado_top must expose, per port index
// P in 0..3:
//
//   * one Type-1 CCHI port  cchi_t1p<P>_*  (the Cohestra V3 CCHI contract —
//     separate-field pins, identical names/widths/directions to the
//     cohestra_v3 dummy top; these pass straight through avocado_top to the
//     harness and bind to the host-side Taurus upstream nodes)
//   * one packed CHI-E RN port  chi_rn<P>_*  (per channel: flit/flitpend/
//     flitv/lcrdv; the DUT drives txreq/txrsp/txdat and receives
//     rxsnp/rxrsp/rxdat — CHI-E flit configuration: REQ 162b, RSP 73b,
//     SNP 115b, DAT 422b)
//
// Link-active, SACTIVE, SYSCO and EVENT handling is done by avocado_top
// (links always up, EVENT looped back); the DUT never sees them.
//
// The macros below are shared by avocado_top.sv, the reference DUT template
// (avocado_dut_template.sv) and the contract checker (scripts/check_dut.sh),
// so all three stay in lockstep. Token-pasting idiom follows top_dummy.sv.
//*****************************************************************************

`ifndef AVOCADO_DUT_IF_SVH
`define AVOCADO_DUT_IF_SVH

// ---------------------------------------------------------------------------
// Type-1 CCHI port P: pin declarations (directions from the DUT's point of
// view — identical at the avocado_top boundary, the pass-through is 1:1).
// ---------------------------------------------------------------------------
`define AVOCADO_CCHI_T1_PORT_DECL(P) \
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

// ---------------------------------------------------------------------------
// Type-1 CCHI port P: pass-through connections (avocado_top -> avocado_dut).
// ---------------------------------------------------------------------------
`define AVOCADO_CCHI_T1_PORT_CONN(P) \
    .cchi_t1p``P``_rxevt_valid         (cchi_t1p``P``_rxevt_valid), \
    .cchi_t1p``P``_rxevt_bits_TxnID    (cchi_t1p``P``_rxevt_bits_TxnID), \
    .cchi_t1p``P``_rxevt_bits_SrcID    (cchi_t1p``P``_rxevt_bits_SrcID), \
    .cchi_t1p``P``_rxevt_bits_TgtID    (cchi_t1p``P``_rxevt_bits_TgtID), \
    .cchi_t1p``P``_rxevt_bits_Opcode   (cchi_t1p``P``_rxevt_bits_Opcode), \
    .cchi_t1p``P``_rxevt_bits_Addr     (cchi_t1p``P``_rxevt_bits_Addr), \
    .cchi_t1p``P``_rxevt_bits_NS       (cchi_t1p``P``_rxevt_bits_NS), \
    .cchi_t1p``P``_rxevt_bits_MemAttr  (cchi_t1p``P``_rxevt_bits_MemAttr), \
    .cchi_t1p``P``_rxevt_bits_WayValid (cchi_t1p``P``_rxevt_bits_WayValid), \
    .cchi_t1p``P``_rxevt_bits_Way      (cchi_t1p``P``_rxevt_bits_Way), \
    .cchi_t1p``P``_rxevt_bits_TraceTag (cchi_t1p``P``_rxevt_bits_TraceTag), \
    .cchi_t1p``P``_rxevt_ready         (cchi_t1p``P``_rxevt_ready), \
    \
    .cchi_t1p``P``_rxreq_valid         (cchi_t1p``P``_rxreq_valid), \
    .cchi_t1p``P``_rxreq_bits_TxnID    (cchi_t1p``P``_rxreq_bits_TxnID), \
    .cchi_t1p``P``_rxreq_bits_SrcID    (cchi_t1p``P``_rxreq_bits_SrcID), \
    .cchi_t1p``P``_rxreq_bits_TgtID    (cchi_t1p``P``_rxreq_bits_TgtID), \
    .cchi_t1p``P``_rxreq_bits_Opcode   (cchi_t1p``P``_rxreq_bits_Opcode), \
    .cchi_t1p``P``_rxreq_bits_Size     (cchi_t1p``P``_rxreq_bits_Size), \
    .cchi_t1p``P``_rxreq_bits_Addr     (cchi_t1p``P``_rxreq_bits_Addr), \
    .cchi_t1p``P``_rxreq_bits_TagAlias (cchi_t1p``P``_rxreq_bits_TagAlias), \
    .cchi_t1p``P``_rxreq_bits_NS       (cchi_t1p``P``_rxreq_bits_NS), \
    .cchi_t1p``P``_rxreq_bits_Order    (cchi_t1p``P``_rxreq_bits_Order), \
    .cchi_t1p``P``_rxreq_bits_MemAttr  (cchi_t1p``P``_rxreq_bits_MemAttr), \
    .cchi_t1p``P``_rxreq_bits_Excl     (cchi_t1p``P``_rxreq_bits_Excl), \
    .cchi_t1p``P``_rxreq_bits_ExpCompData (cchi_t1p``P``_rxreq_bits_ExpCompData), \
    .cchi_t1p``P``_rxreq_bits_TraceTag (cchi_t1p``P``_rxreq_bits_TraceTag), \
    .cchi_t1p``P``_rxreq_bits_WayValid (cchi_t1p``P``_rxreq_bits_WayValid), \
    .cchi_t1p``P``_rxreq_bits_Way      (cchi_t1p``P``_rxreq_bits_Way), \
    .cchi_t1p``P``_rxreq_ready         (cchi_t1p``P``_rxreq_ready), \
    \
    .cchi_t1p``P``_txsnp_ready         (cchi_t1p``P``_txsnp_ready), \
    .cchi_t1p``P``_txsnp_valid         (cchi_t1p``P``_txsnp_valid), \
    .cchi_t1p``P``_txsnp_bits_TxnID    (cchi_t1p``P``_txsnp_bits_TxnID), \
    .cchi_t1p``P``_txsnp_bits_SrcID    (cchi_t1p``P``_txsnp_bits_SrcID), \
    .cchi_t1p``P``_txsnp_bits_TgtID    (cchi_t1p``P``_txsnp_bits_TgtID), \
    .cchi_t1p``P``_txsnp_bits_Opcode   (cchi_t1p``P``_txsnp_bits_Opcode), \
    .cchi_t1p``P``_txsnp_bits_Addr     (cchi_t1p``P``_txsnp_bits_Addr), \
    .cchi_t1p``P``_txsnp_bits_NS       (cchi_t1p``P``_txsnp_bits_NS), \
    .cchi_t1p``P``_txsnp_bits_TraceTag (cchi_t1p``P``_txsnp_bits_TraceTag), \
    \
    .cchi_t1p``P``_txrsp_ready         (cchi_t1p``P``_txrsp_ready), \
    .cchi_t1p``P``_txrsp_valid         (cchi_t1p``P``_txrsp_valid), \
    .cchi_t1p``P``_txrsp_bits_TxnID    (cchi_t1p``P``_txrsp_bits_TxnID), \
    .cchi_t1p``P``_txrsp_bits_SrcID    (cchi_t1p``P``_txrsp_bits_SrcID), \
    .cchi_t1p``P``_txrsp_bits_TgtID    (cchi_t1p``P``_txrsp_bits_TgtID), \
    .cchi_t1p``P``_txrsp_bits_DBID     (cchi_t1p``P``_txrsp_bits_DBID), \
    .cchi_t1p``P``_txrsp_bits_Opcode   (cchi_t1p``P``_txrsp_bits_Opcode), \
    .cchi_t1p``P``_txrsp_bits_RespErr  (cchi_t1p``P``_txrsp_bits_RespErr), \
    .cchi_t1p``P``_txrsp_bits_Resp     (cchi_t1p``P``_txrsp_bits_Resp), \
    .cchi_t1p``P``_txrsp_bits_CBusy    (cchi_t1p``P``_txrsp_bits_CBusy), \
    .cchi_t1p``P``_txrsp_bits_WayValid (cchi_t1p``P``_txrsp_bits_WayValid), \
    .cchi_t1p``P``_txrsp_bits_Way      (cchi_t1p``P``_txrsp_bits_Way), \
    .cchi_t1p``P``_txrsp_bits_TraceTag (cchi_t1p``P``_txrsp_bits_TraceTag), \
    \
    .cchi_t1p``P``_rxrsp_valid         (cchi_t1p``P``_rxrsp_valid), \
    .cchi_t1p``P``_rxrsp_bits_TxnID    (cchi_t1p``P``_rxrsp_bits_TxnID), \
    .cchi_t1p``P``_rxrsp_bits_SrcID    (cchi_t1p``P``_rxrsp_bits_SrcID), \
    .cchi_t1p``P``_rxrsp_bits_TgtID    (cchi_t1p``P``_rxrsp_bits_TgtID), \
    .cchi_t1p``P``_rxrsp_bits_Opcode   (cchi_t1p``P``_rxrsp_bits_Opcode), \
    .cchi_t1p``P``_rxrsp_bits_RespErr  (cchi_t1p``P``_rxrsp_bits_RespErr), \
    .cchi_t1p``P``_rxrsp_bits_Resp     (cchi_t1p``P``_rxrsp_bits_Resp), \
    .cchi_t1p``P``_rxrsp_bits_TraceTag (cchi_t1p``P``_rxrsp_bits_TraceTag), \
    .cchi_t1p``P``_rxrsp_ready         (cchi_t1p``P``_rxrsp_ready), \
    \
    .cchi_t1p``P``_txdat_ready         (cchi_t1p``P``_txdat_ready), \
    .cchi_t1p``P``_txdat_valid         (cchi_t1p``P``_txdat_valid), \
    .cchi_t1p``P``_txdat_bits_TxnID    (cchi_t1p``P``_txdat_bits_TxnID), \
    .cchi_t1p``P``_txdat_bits_SrcID    (cchi_t1p``P``_txdat_bits_SrcID), \
    .cchi_t1p``P``_txdat_bits_TgtID    (cchi_t1p``P``_txdat_bits_TgtID), \
    .cchi_t1p``P``_txdat_bits_DBID     (cchi_t1p``P``_txdat_bits_DBID), \
    .cchi_t1p``P``_txdat_bits_Opcode   (cchi_t1p``P``_txdat_bits_Opcode), \
    .cchi_t1p``P``_txdat_bits_RespErr  (cchi_t1p``P``_txdat_bits_RespErr), \
    .cchi_t1p``P``_txdat_bits_Resp     (cchi_t1p``P``_txdat_bits_Resp), \
    .cchi_t1p``P``_txdat_bits_DataSource (cchi_t1p``P``_txdat_bits_DataSource), \
    .cchi_t1p``P``_txdat_bits_CBusy    (cchi_t1p``P``_txdat_bits_CBusy), \
    .cchi_t1p``P``_txdat_bits_DataID   (cchi_t1p``P``_txdat_bits_DataID), \
    .cchi_t1p``P``_txdat_bits_Data     (cchi_t1p``P``_txdat_bits_Data), \
    .cchi_t1p``P``_txdat_bits_WayValid (cchi_t1p``P``_txdat_bits_WayValid), \
    .cchi_t1p``P``_txdat_bits_Way      (cchi_t1p``P``_txdat_bits_Way), \
    .cchi_t1p``P``_txdat_bits_TraceTag (cchi_t1p``P``_txdat_bits_TraceTag), \
    \
    .cchi_t1p``P``_rxdat_valid         (cchi_t1p``P``_rxdat_valid), \
    .cchi_t1p``P``_rxdat_bits_TxnID    (cchi_t1p``P``_rxdat_bits_TxnID), \
    .cchi_t1p``P``_rxdat_bits_SrcID    (cchi_t1p``P``_rxdat_bits_SrcID), \
    .cchi_t1p``P``_rxdat_bits_TgtID    (cchi_t1p``P``_rxdat_bits_TgtID), \
    .cchi_t1p``P``_rxdat_bits_Opcode   (cchi_t1p``P``_rxdat_bits_Opcode), \
    .cchi_t1p``P``_rxdat_bits_RespErr  (cchi_t1p``P``_rxdat_bits_RespErr), \
    .cchi_t1p``P``_rxdat_bits_Resp     (cchi_t1p``P``_rxdat_bits_Resp), \
    .cchi_t1p``P``_rxdat_bits_DataID   (cchi_t1p``P``_rxdat_bits_DataID), \
    .cchi_t1p``P``_rxdat_bits_Data     (cchi_t1p``P``_rxdat_bits_Data), \
    .cchi_t1p``P``_rxdat_bits_BE       (cchi_t1p``P``_rxdat_bits_BE), \
    .cchi_t1p``P``_rxdat_bits_TraceTag (cchi_t1p``P``_rxdat_bits_TraceTag), \
    .cchi_t1p``P``_rxdat_ready         (cchi_t1p``P``_rxdat_ready)

// ---------------------------------------------------------------------------
// Packed CHI-E RN port P (DUT <-> mesh RN-F port): pin declarations,
// directions from the DUT's point of view (the DUT is the RN-F).
// ---------------------------------------------------------------------------
`define AVOCADO_CHI_RN_PORT_DECL(P) \
    output logic         chi_rn``P``_txreq_flitpend, \
    output logic         chi_rn``P``_txreq_flitv, \
    output logic [161:0] chi_rn``P``_txreq_flit, \
    input  logic         chi_rn``P``_txreq_lcrdv, \
    \
    output logic         chi_rn``P``_txrsp_flitpend, \
    output logic         chi_rn``P``_txrsp_flitv, \
    output logic [72:0]  chi_rn``P``_txrsp_flit, \
    input  logic         chi_rn``P``_txrsp_lcrdv, \
    \
    output logic         chi_rn``P``_txdat_flitpend, \
    output logic         chi_rn``P``_txdat_flitv, \
    output logic [421:0] chi_rn``P``_txdat_flit, \
    input  logic         chi_rn``P``_txdat_lcrdv, \
    \
    input  logic         chi_rn``P``_rxrsp_flitpend, \
    input  logic         chi_rn``P``_rxrsp_flitv, \
    input  logic [72:0]  chi_rn``P``_rxrsp_flit, \
    output logic         chi_rn``P``_rxrsp_lcrdv, \
    \
    input  logic         chi_rn``P``_rxdat_flitpend, \
    input  logic         chi_rn``P``_rxdat_flitv, \
    input  logic [421:0] chi_rn``P``_rxdat_flit, \
    output logic         chi_rn``P``_rxdat_lcrdv, \
    \
    input  logic         chi_rn``P``_rxsnp_flitpend, \
    input  logic         chi_rn``P``_rxsnp_flitv, \
    input  logic [114:0] chi_rn``P``_rxsnp_flit, \
    output logic         chi_rn``P``_rxsnp_lcrdv

// Packed CHI-E RN port P: connections for the avocado_dut instantiation.
`define AVOCADO_CHI_RN_PORT_CONN(P) \
    .chi_rn``P``_txreq_flitpend (chi_rn``P``_txreq_flitpend), \
    .chi_rn``P``_txreq_flitv    (chi_rn``P``_txreq_flitv), \
    .chi_rn``P``_txreq_flit     (chi_rn``P``_txreq_flit), \
    .chi_rn``P``_txreq_lcrdv    (chi_rn``P``_txreq_lcrdv), \
    \
    .chi_rn``P``_txrsp_flitpend (chi_rn``P``_txrsp_flitpend), \
    .chi_rn``P``_txrsp_flitv    (chi_rn``P``_txrsp_flitv), \
    .chi_rn``P``_txrsp_flit     (chi_rn``P``_txrsp_flit), \
    .chi_rn``P``_txrsp_lcrdv    (chi_rn``P``_txrsp_lcrdv), \
    \
    .chi_rn``P``_txdat_flitpend (chi_rn``P``_txdat_flitpend), \
    .chi_rn``P``_txdat_flitv    (chi_rn``P``_txdat_flitv), \
    .chi_rn``P``_txdat_flit     (chi_rn``P``_txdat_flit), \
    .chi_rn``P``_txdat_lcrdv    (chi_rn``P``_txdat_lcrdv), \
    \
    .chi_rn``P``_rxrsp_flitpend (chi_rn``P``_rxrsp_flitpend), \
    .chi_rn``P``_rxrsp_flitv    (chi_rn``P``_rxrsp_flitv), \
    .chi_rn``P``_rxrsp_flit     (chi_rn``P``_rxrsp_flit), \
    .chi_rn``P``_rxrsp_lcrdv    (chi_rn``P``_rxrsp_lcrdv), \
    \
    .chi_rn``P``_rxdat_flitpend (chi_rn``P``_rxdat_flitpend), \
    .chi_rn``P``_rxdat_flitv    (chi_rn``P``_rxdat_flitv), \
    .chi_rn``P``_rxdat_flit     (chi_rn``P``_rxdat_flit), \
    .chi_rn``P``_rxdat_lcrdv    (chi_rn``P``_rxdat_lcrdv), \
    \
    .chi_rn``P``_rxsnp_flitpend (chi_rn``P``_rxsnp_flitpend), \
    .chi_rn``P``_rxsnp_flitv    (chi_rn``P``_rxsnp_flitv), \
    .chi_rn``P``_rxsnp_flit     (chi_rn``P``_rxsnp_flit), \
    .chi_rn``P``_rxsnp_lcrdv    (chi_rn``P``_rxsnp_lcrdv)

// ---------------------------------------------------------------------------
// Internal wires of avocado_top: the CHI nets shared by the DUT and the mesh
// RN-F port, plus the per-port loopback wires (TXLINKACTIVE ack, EVENT).
// ---------------------------------------------------------------------------
`define AVOCADO_CHI_RN_WIRES_DECL(P) \
    logic         chi_rn``P``_txreq_flitpend; \
    logic         chi_rn``P``_txreq_flitv; \
    logic [161:0] chi_rn``P``_txreq_flit; \
    logic         chi_rn``P``_txreq_lcrdv; \
    logic         chi_rn``P``_txrsp_flitpend; \
    logic         chi_rn``P``_txrsp_flitv; \
    logic [72:0]  chi_rn``P``_txrsp_flit; \
    logic         chi_rn``P``_txrsp_lcrdv; \
    logic         chi_rn``P``_txdat_flitpend; \
    logic         chi_rn``P``_txdat_flitv; \
    logic [421:0] chi_rn``P``_txdat_flit; \
    logic         chi_rn``P``_txdat_lcrdv; \
    logic         chi_rn``P``_rxrsp_flitpend; \
    logic         chi_rn``P``_rxrsp_flitv; \
    logic [72:0]  chi_rn``P``_rxrsp_flit; \
    logic         chi_rn``P``_rxrsp_lcrdv; \
    logic         chi_rn``P``_rxdat_flitpend; \
    logic         chi_rn``P``_rxdat_flitv; \
    logic [421:0] chi_rn``P``_rxdat_flit; \
    logic         chi_rn``P``_rxdat_lcrdv; \
    logic         chi_rn``P``_rxsnp_flitpend; \
    logic         chi_rn``P``_rxsnp_flitv; \
    logic [114:0] chi_rn``P``_rxsnp_flit; \
    logic         chi_rn``P``_rxsnp_lcrdv; \
    logic         chi_rn``P``_txla_req; \
    logic         chi_rn``P``_eventi; \
    logic         chi_rn``P``_evento

// ---------------------------------------------------------------------------
// Mesh RN-F port NID <-> DUT chi_rn port P connection (for the Avocado
// instance inside avocado_top). The flit channels land directly on the
// shared chi_rn<P>_* nets; link-active is held always-up, SYSCO is held
// requested, and the EVENT handshake is looped back (IACK=IREQ, OREQ=OACK).
// ---------------------------------------------------------------------------
`define AVOCADO_RNF_MESH_CONN(P, NID) \
    .RNF``NID``_RXREQFLITV       (chi_rn``P``_txreq_flitv), \
    .RNF``NID``_RXREQFLITPEND    (chi_rn``P``_txreq_flitpend), \
    .RNF``NID``_RXREQFLIT        (chi_rn``P``_txreq_flit), \
    .RNF``NID``_RXREQLCRDV       (chi_rn``P``_txreq_lcrdv), \
    .RNF``NID``_RXRSPFLITV       (chi_rn``P``_txrsp_flitv), \
    .RNF``NID``_RXRSPFLITPEND    (chi_rn``P``_txrsp_flitpend), \
    .RNF``NID``_RXRSPFLIT        (chi_rn``P``_txrsp_flit), \
    .RNF``NID``_RXRSPLCRDV       (chi_rn``P``_txrsp_lcrdv), \
    .RNF``NID``_TXRSPFLIT        (chi_rn``P``_rxrsp_flit), \
    .RNF``NID``_TXRSPFLITV       (chi_rn``P``_rxrsp_flitv), \
    .RNF``NID``_TXRSPFLITPEND    (chi_rn``P``_rxrsp_flitpend), \
    .RNF``NID``_TXRSPLCRDV       (chi_rn``P``_rxrsp_lcrdv), \
    .RNF``NID``_TXSNPFLIT        (chi_rn``P``_rxsnp_flit), \
    .RNF``NID``_TXSNPFLITV       (chi_rn``P``_rxsnp_flitv), \
    .RNF``NID``_TXSNPFLITPEND    (chi_rn``P``_rxsnp_flitpend), \
    .RNF``NID``_TXSNPLCRDV       (chi_rn``P``_rxsnp_lcrdv), \
    .RNF``NID``_RXDATFLITV       (chi_rn``P``_txdat_flitv), \
    .RNF``NID``_RXDATFLITPEND    (chi_rn``P``_txdat_flitpend), \
    .RNF``NID``_RXDATFLIT        (chi_rn``P``_txdat_flit), \
    .RNF``NID``_RXDATLCRDV       (chi_rn``P``_txdat_lcrdv), \
    .RNF``NID``_TXDATFLIT        (chi_rn``P``_rxdat_flit), \
    .RNF``NID``_TXDATFLITV       (chi_rn``P``_rxdat_flitv), \
    .RNF``NID``_TXDATFLITPEND    (chi_rn``P``_rxdat_flitpend), \
    .RNF``NID``_TXDATLCRDV       (chi_rn``P``_rxdat_lcrdv), \
    .RNF``NID``_RXLINKACTIVEREQ  (1'b1), \
    .RNF``NID``_RXLINKACTIVEACK  (), \
    .RNF``NID``_TXLINKACTIVEREQ  (chi_rn``P``_txla_req), \
    .RNF``NID``_TXLINKACTIVEACK  (chi_rn``P``_txla_req), \
    .RNF``NID``_RXSACTIVE        (1'b1), \
    .RNF``NID``_TXSACTIVE        (), \
    .RNF``NID``_SYSCOREQ         (1'b1), \
    .RNF``NID``_SYSCOACK         (), \
    .RNF``NID``_EVENTIREQ        (chi_rn``P``_eventi), \
    .RNF``NID``_EVENTIACK        (chi_rn``P``_eventi), \
    .RNF``NID``_EVENTOREQ        (chi_rn``P``_evento), \
    .RNF``NID``_EVENTOACK        (chi_rn``P``_evento)

// ---------------------------------------------------------------------------
// Cohestra V3 AXI port declarations at the avocado_top boundary.
//   M (harness is slave/memory; mesh SBSX is the master): 24-bit ID.
//   S (harness is master; mesh RN-I config slave):         11-bit ID.
// ---------------------------------------------------------------------------
`define AVOCADO_AXI_M_PORT_DECL(P) \
    output logic         axi_m``P``_awvalid, \
    output logic [23:0]  axi_m``P``_awid, \
    output logic [47:0]  axi_m``P``_awaddr, \
    output logic [7:0]   axi_m``P``_awlen, \
    output logic [2:0]   axi_m``P``_awsize, \
    output logic [1:0]   axi_m``P``_awburst, \
    input  logic         axi_m``P``_awready, \
    output logic         axi_m``P``_wvalid, \
    output logic [255:0] axi_m``P``_wdata, \
    output logic [31:0]  axi_m``P``_wstrb, \
    output logic         axi_m``P``_wlast, \
    input  logic         axi_m``P``_wready, \
    input  logic         axi_m``P``_bvalid, \
    input  logic [23:0]  axi_m``P``_bid, \
    input  logic [1:0]   axi_m``P``_bresp, \
    output logic         axi_m``P``_bready, \
    output logic         axi_m``P``_arvalid, \
    output logic [23:0]  axi_m``P``_arid, \
    output logic [47:0]  axi_m``P``_araddr, \
    output logic [7:0]   axi_m``P``_arlen, \
    output logic [2:0]   axi_m``P``_arsize, \
    output logic [1:0]   axi_m``P``_arburst, \
    input  logic         axi_m``P``_arready, \
    input  logic         axi_m``P``_rvalid, \
    input  logic [23:0]  axi_m``P``_rid, \
    input  logic [255:0] axi_m``P``_rdata, \
    input  logic [1:0]   axi_m``P``_rresp, \
    input  logic         axi_m``P``_rlast, \
    output logic         axi_m``P``_rready

`define AVOCADO_AXI_S_PORT_DECL(P) \
    input  logic         axi_s``P``_awvalid, \
    input  logic [10:0]  axi_s``P``_awid, \
    input  logic [47:0]  axi_s``P``_awaddr, \
    input  logic [7:0]   axi_s``P``_awlen, \
    input  logic [2:0]   axi_s``P``_awsize, \
    input  logic [1:0]   axi_s``P``_awburst, \
    output logic         axi_s``P``_awready, \
    input  logic         axi_s``P``_wvalid, \
    input  logic [255:0] axi_s``P``_wdata, \
    input  logic [31:0]  axi_s``P``_wstrb, \
    input  logic         axi_s``P``_wlast, \
    output logic         axi_s``P``_wready, \
    output logic         axi_s``P``_bvalid, \
    output logic [10:0]  axi_s``P``_bid, \
    output logic [1:0]   axi_s``P``_bresp, \
    input  logic         axi_s``P``_bready, \
    input  logic         axi_s``P``_arvalid, \
    input  logic [10:0]  axi_s``P``_arid, \
    input  logic [47:0]  axi_s``P``_araddr, \
    input  logic [7:0]   axi_s``P``_arlen, \
    input  logic [2:0]   axi_s``P``_arsize, \
    input  logic [1:0]   axi_s``P``_arburst, \
    output logic         axi_s``P``_arready, \
    output logic         axi_s``P``_rvalid, \
    output logic [10:0]  axi_s``P``_rid, \
    output logic [255:0] axi_s``P``_rdata, \
    output logic [1:0]   axi_s``P``_rresp, \
    output logic         axi_s``P``_rlast, \
    input  logic         axi_s``P``_rready

// ---------------------------------------------------------------------------
// Mesh SBSX memory master MEMAXIS -> Cohestra axi_m<P> (harness memory).
// ACE5-Lite-only pins are left open (outputs) or tied 0 (inputs); RRESP is
// 3 bits at the mesh boundary and takes {1'b0, axi_m<P>_rresp}.
// ---------------------------------------------------------------------------
`define AVOCADO_AXI_MEM_CONN(MEMN, P) \
    .MEM``MEMN``_AXI_AWAKEUP    (), \
    .MEM``MEMN``_AXI_AWID       (axi_m``P``_awid), \
    .MEM``MEMN``_AXI_AWADDR     (axi_m``P``_awaddr), \
    .MEM``MEMN``_AXI_AWLEN      (axi_m``P``_awlen), \
    .MEM``MEMN``_AXI_AWSIZE     (axi_m``P``_awsize), \
    .MEM``MEMN``_AXI_AWBURST    (axi_m``P``_awburst), \
    .MEM``MEMN``_AXI_AWLOCK     (), \
    .MEM``MEMN``_AXI_AWCACHE    (), \
    .MEM``MEMN``_AXI_AWPROT     (), \
    .MEM``MEMN``_AXI_AWQOS      (), \
    .MEM``MEMN``_AXI_AWNSAID    (), \
    .MEM``MEMN``_AXI_AWSNOOP    (), \
    .MEM``MEMN``_AXI_AWCMO      (), \
    .MEM``MEMN``_AXI_AWTAGOP    (), \
    .MEM``MEMN``_AXI_AWDOMAIN   (), \
    .MEM``MEMN``_AXI_AWUSER     (), \
    .MEM``MEMN``_AXI_AWTRACE    (), \
    .MEM``MEMN``_AXI_AWMPAM     (), \
    .MEM``MEMN``_AXI_AWIDUNQ    (), \
    .MEM``MEMN``_AXI_AWVALID    (axi_m``P``_awvalid), \
    .MEM``MEMN``_AXI_AWREADY    (axi_m``P``_awready), \
    .MEM``MEMN``_AXI_WDATA      (axi_m``P``_wdata), \
    .MEM``MEMN``_AXI_WSTRB      (axi_m``P``_wstrb), \
    .MEM``MEMN``_AXI_WLAST      (axi_m``P``_wlast), \
    .MEM``MEMN``_AXI_WUSER      (), \
    .MEM``MEMN``_AXI_WTRACE     (), \
    .MEM``MEMN``_AXI_WPOISON    (), \
    .MEM``MEMN``_AXI_WDATACHK   (), \
    .MEM``MEMN``_AXI_WTAG       (), \
    .MEM``MEMN``_AXI_WTAGUPDATE (), \
    .MEM``MEMN``_AXI_WVALID     (axi_m``P``_wvalid), \
    .MEM``MEMN``_AXI_WREADY     (axi_m``P``_wready), \
    .MEM``MEMN``_AXI_BID        (axi_m``P``_bid), \
    .MEM``MEMN``_AXI_BRESP      (axi_m``P``_bresp), \
    .MEM``MEMN``_AXI_BCOMP      (1'b0), \
    .MEM``MEMN``_AXI_BTAGMATCH  ('0), \
    .MEM``MEMN``_AXI_BUSER      ('0), \
    .MEM``MEMN``_AXI_BTRACE     (1'b0), \
    .MEM``MEMN``_AXI_BIDUNQ     (1'b0), \
    .MEM``MEMN``_AXI_BPERSIST   (1'b0), \
    .MEM``MEMN``_AXI_BVALID     (axi_m``P``_bvalid), \
    .MEM``MEMN``_AXI_BREADY     (axi_m``P``_bready), \
    .MEM``MEMN``_AXI_ARID       (axi_m``P``_arid), \
    .MEM``MEMN``_AXI_ARADDR     (axi_m``P``_araddr), \
    .MEM``MEMN``_AXI_ARLEN      (axi_m``P``_arlen), \
    .MEM``MEMN``_AXI_ARSIZE     (axi_m``P``_arsize), \
    .MEM``MEMN``_AXI_ARBURST    (axi_m``P``_arburst), \
    .MEM``MEMN``_AXI_ARLOCK     (), \
    .MEM``MEMN``_AXI_ARCACHE    (), \
    .MEM``MEMN``_AXI_ARPROT     (), \
    .MEM``MEMN``_AXI_ARQOS      (), \
    .MEM``MEMN``_AXI_ARNSAID    (), \
    .MEM``MEMN``_AXI_ARSNOOP    (), \
    .MEM``MEMN``_AXI_ARTAGOP    (), \
    .MEM``MEMN``_AXI_ARDOMAIN   (), \
    .MEM``MEMN``_AXI_ARUSER     (), \
    .MEM``MEMN``_AXI_ARTRACE    (), \
    .MEM``MEMN``_AXI_ARMPAM     (), \
    .MEM``MEMN``_AXI_ARIDUNQ    (), \
    .MEM``MEMN``_AXI_ARVALID    (axi_m``P``_arvalid), \
    .MEM``MEMN``_AXI_ARREADY    (axi_m``P``_arready), \
    .MEM``MEMN``_AXI_RID        (axi_m``P``_rid), \
    .MEM``MEMN``_AXI_RDATA      (axi_m``P``_rdata), \
    .MEM``MEMN``_AXI_RUSER      (1'b0), \
    .MEM``MEMN``_AXI_RRESP      ({1'b0, axi_m``P``_rresp}), \
    .MEM``MEMN``_AXI_RLAST      (axi_m``P``_rlast), \
    .MEM``MEMN``_AXI_RTRACE     (1'b0), \
    .MEM``MEMN``_AXI_RPOISON    ('0), \
    .MEM``MEMN``_AXI_RDATACHK   ('0), \
    .MEM``MEMN``_AXI_RTAG       ('0), \
    .MEM``MEMN``_AXI_RIDUNQ     (1'b0), \
    .MEM``MEMN``_AXI_RVALID     (axi_m``P``_rvalid), \
    .MEM``MEMN``_AXI_RREADY     (axi_m``P``_rready)

`endif // AVOCADO_DUT_IF_SVH
