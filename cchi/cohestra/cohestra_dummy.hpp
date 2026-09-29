#pragma once

#include <cstdint>


namespace Cohestra {

#define COHESTRA_DUMMY_V3_TYPE1_PORT(PORT) \
    uint8_t cchi_t1p ## PORT ## _rxevt_ready = 0; \
    uint8_t cchi_t1p ## PORT ## _rxevt_valid = 0; \
    uint16_t cchi_t1p ## PORT ## _rxevt_bits_TxnID = 0; \
    uint8_t cchi_t1p ## PORT ## _rxevt_bits_SrcID = 0, cchi_t1p ## PORT ## _rxevt_bits_TgtID = 0, cchi_t1p ## PORT ## _rxevt_bits_Opcode = 0; \
    uint64_t cchi_t1p ## PORT ## _rxevt_bits_Addr = 0; \
    uint8_t cchi_t1p ## PORT ## _rxevt_bits_NS = 0, cchi_t1p ## PORT ## _rxevt_bits_MemAttr = 0, cchi_t1p ## PORT ## _rxevt_bits_TraceTag = 0; \
    uint8_t cchi_t1p ## PORT ## _rxreq_ready = 0, cchi_t1p ## PORT ## _rxreq_valid = 0; \
    uint16_t cchi_t1p ## PORT ## _rxreq_bits_TxnID = 0; \
    uint8_t cchi_t1p ## PORT ## _rxreq_bits_SrcID = 0, cchi_t1p ## PORT ## _rxreq_bits_TgtID = 0, cchi_t1p ## PORT ## _rxreq_bits_Opcode = 0, cchi_t1p ## PORT ## _rxreq_bits_Size = 0; \
    uint64_t cchi_t1p ## PORT ## _rxreq_bits_Addr = 0; \
    uint8_t cchi_t1p ## PORT ## _rxreq_bits_TagAlias = 0; \
    uint8_t cchi_t1p ## PORT ## _rxreq_bits_NS = 0, cchi_t1p ## PORT ## _rxreq_bits_Order = 0, cchi_t1p ## PORT ## _rxreq_bits_MemAttr = 0, cchi_t1p ## PORT ## _rxreq_bits_Excl = 0, cchi_t1p ## PORT ## _rxreq_bits_ExpCompData = 0, cchi_t1p ## PORT ## _rxreq_bits_TraceTag = 0; \
    uint8_t cchi_t1p ## PORT ## _txsnp_ready = 0, cchi_t1p ## PORT ## _txsnp_valid = 0; \
    uint16_t cchi_t1p ## PORT ## _txsnp_bits_TxnID = 0; \
    uint8_t cchi_t1p ## PORT ## _txsnp_bits_SrcID = 0, cchi_t1p ## PORT ## _txsnp_bits_TgtID = 0, cchi_t1p ## PORT ## _txsnp_bits_Opcode = 0; \
    uint64_t cchi_t1p ## PORT ## _txsnp_bits_Addr = 0; \
    uint8_t cchi_t1p ## PORT ## _txsnp_bits_NS = 0, cchi_t1p ## PORT ## _txsnp_bits_TraceTag = 0; \
    uint8_t cchi_t1p ## PORT ## _txrsp_ready = 0, cchi_t1p ## PORT ## _txrsp_valid = 0; \
    uint16_t cchi_t1p ## PORT ## _txrsp_bits_TxnID = 0; \
    uint8_t cchi_t1p ## PORT ## _txrsp_bits_SrcID = 0, cchi_t1p ## PORT ## _txrsp_bits_TgtID = 0, cchi_t1p ## PORT ## _txrsp_bits_DBID = 0, cchi_t1p ## PORT ## _txrsp_bits_Opcode = 0, cchi_t1p ## PORT ## _txrsp_bits_RespErr = 0, cchi_t1p ## PORT ## _txrsp_bits_Resp = 0, cchi_t1p ## PORT ## _txrsp_bits_CBusy = 0, cchi_t1p ## PORT ## _txrsp_bits_TraceTag = 0; \
    uint8_t cchi_t1p ## PORT ## _rxrsp_ready = 0, cchi_t1p ## PORT ## _rxrsp_valid = 0; \
    uint16_t cchi_t1p ## PORT ## _rxrsp_bits_TxnID = 0; \
    uint8_t cchi_t1p ## PORT ## _rxrsp_bits_SrcID = 0, cchi_t1p ## PORT ## _rxrsp_bits_TgtID = 0, cchi_t1p ## PORT ## _rxrsp_bits_Opcode = 0, cchi_t1p ## PORT ## _rxrsp_bits_RespErr = 0, cchi_t1p ## PORT ## _rxrsp_bits_Resp = 0, cchi_t1p ## PORT ## _rxrsp_bits_TraceTag = 0; \
    uint8_t cchi_t1p ## PORT ## _txdat_ready = 0, cchi_t1p ## PORT ## _txdat_valid = 0; \
    uint16_t cchi_t1p ## PORT ## _txdat_bits_TxnID = 0; \
    uint8_t cchi_t1p ## PORT ## _txdat_bits_SrcID = 0, cchi_t1p ## PORT ## _txdat_bits_TgtID = 0, cchi_t1p ## PORT ## _txdat_bits_DBID = 0, cchi_t1p ## PORT ## _txdat_bits_Opcode = 0, cchi_t1p ## PORT ## _txdat_bits_RespErr = 0, cchi_t1p ## PORT ## _txdat_bits_Resp = 0, cchi_t1p ## PORT ## _txdat_bits_DataSource = 0, cchi_t1p ## PORT ## _txdat_bits_CBusy = 0, cchi_t1p ## PORT ## _txdat_bits_DataID = 0; \
    uint32_t cchi_t1p ## PORT ## _txdat_bits_Data[8] = {}; \
    uint8_t cchi_t1p ## PORT ## _txdat_bits_TraceTag = 0; \
    uint8_t cchi_t1p ## PORT ## _rxdat_ready = 0, cchi_t1p ## PORT ## _rxdat_valid = 0; \
    uint16_t cchi_t1p ## PORT ## _rxdat_bits_TxnID = 0; \
    uint8_t cchi_t1p ## PORT ## _rxdat_bits_SrcID = 0, cchi_t1p ## PORT ## _rxdat_bits_TgtID = 0, cchi_t1p ## PORT ## _rxdat_bits_Opcode = 0, cchi_t1p ## PORT ## _rxdat_bits_RespErr = 0, cchi_t1p ## PORT ## _rxdat_bits_Resp = 0, cchi_t1p ## PORT ## _rxdat_bits_DataID = 0; \
    uint32_t cchi_t1p ## PORT ## _rxdat_bits_Data[8] = {}; \
    uint32_t cchi_t1p ## PORT ## _rxdat_bits_BE = 0; \
    uint8_t cchi_t1p ## PORT ## _rxdat_bits_TraceTag = 0;

    struct DummyV3CCHIModule1P {
        COHESTRA_DUMMY_V3_TYPE1_PORT(0)
    };

    struct DummyV3CCHIModule2P {
        COHESTRA_DUMMY_V3_TYPE1_PORT(0)
        COHESTRA_DUMMY_V3_TYPE1_PORT(1)
    };

    struct DummyV3CCHIModule3P {
        COHESTRA_DUMMY_V3_TYPE1_PORT(0)
        COHESTRA_DUMMY_V3_TYPE1_PORT(1)
        COHESTRA_DUMMY_V3_TYPE1_PORT(2)
    };

    struct DummyV3CCHIModule4P {
        COHESTRA_DUMMY_V3_TYPE1_PORT(0)
        COHESTRA_DUMMY_V3_TYPE1_PORT(1)
        COHESTRA_DUMMY_V3_TYPE1_PORT(2)
        COHESTRA_DUMMY_V3_TYPE1_PORT(3)
    };

#undef COHESTRA_DUMMY_V3_TYPE1_PORT
}
