#pragma once

#include <cstdint>

/*
* CLog.B file handle operations.
*/
extern "C" void* CLogB_OpenFile(
    const char*         path,
    uint32_t*           status);

extern "C" void CLogB_CloseFile(
    void*               handle);

extern "C" void CLogB_ShareHandle(
    const char*         id,
    void*               handle);

extern "C" int CLogB_UnshareHandle(
    const char*         id);

extern "C" void* CLogB_GetSharedHandle(
    const char*         id
);


/*
* CLog.B parameters write operations.
*/
extern "C" void CLogB_WriteParameters(
    void*               handle,
    uint32_t            issue,
    uint32_t            nodeidWidth,
    uint32_t            addrWidth,
    uint32_t            reqRsvdcWidth,
    uint32_t            datRsvdcWidth,
    uint32_t            dataWidth,
    uint32_t            dataCheckPresent,
    uint32_t            poisonPresent,
    uint32_t            mpamPresent);

extern "C" void CLogB_SharedWriteParameters(
    const char*         id,
    uint32_t            issue,
    uint32_t            nodeidWidth,
    uint32_t            addrWidth,
    uint32_t            reqRsvdcWidth,
    uint32_t            datRsvdcWidth,
    uint32_t            dataWidth,
    uint32_t            dataCheckPresent,
    uint32_t            poisonPresent,
    uint32_t            mpamPresent);


/*
* CLog.B topologies write operations.
*/
extern "C" void CLogB_WriteTopo(
    void*               handle,
    uint32_t            nodeId,
    uint32_t            nodeType
);

extern "C" void CLogB_WriteTopoEnd(
    void*               handle);

extern "C" void CLogB_SharedWriteTopo(
    const char*         id,
    uint32_t            nodeId,
    uint32_t            nodeType
);
    
extern "C" void CLogB_SharedWriteTopoEnd(
    const char*         id);


/*
* CLog.B log record write operations.
*/
extern "C" void CLogB_WriteRecord(
    void*               handle,
    uint64_t            time,
    uint32_t            nodeId,
    uint32_t            channel,
    const uint32_t*     flit,
    uint32_t            flitLength
);

extern "C" void CLogB_SharedWriteRecord(
    const char*         id,
    uint64_t            time,
    uint32_t            nodeId,
    uint32_t            channel,
    const uint32_t*     flit,
    uint32_t            flitLength
);
