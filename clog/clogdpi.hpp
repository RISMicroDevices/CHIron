#pragma once

#include <cstdint>

/*
* CLog.T file handle operations.
*/
extern "C" void* CLogT_OpenFile(
    const char*         path, 
    uint32_t*           status);

extern "C" void CLogT_CloseFile(
    void*               handle);

/*
* CLog.T parameters write operations.
*/
extern "C" void CLogT_WriteIssue(
    void*               handle,
    uint32_t            issue);

extern "C" void CLogT_WriteWidthNodeID(
    void*               handle,
    uint32_t            nodeIdWidth);

extern "C" void CLogT_WriteWidthAddr(
    void*               handle,
    uint32_t            addrWidth);

extern "C" void CLogT_WriteWidthReqRSVDC(
    void*               handle,
    uint32_t            reqRsvdcWidth);

extern "C" void CLogT_WriteWidthDatRSVDC(
    void*               handle,
    uint32_t            datRsvdcWidth);

extern "C" void CLogT_WriteWidthData(
    void*               handle,
    uint32_t            dataWidth);

extern "C" void CLogT_WriteEnableDataCheck(
    void*               handle,
    uint32_t            dataCheckPresent);

extern "C" void CLogT_WriteEnablePoison(
    void*               handle,
    uint32_t            poisonPresent);

extern "C" void CLogT_WriteEnableMPAM(
    void*               handle,
    uint32_t            mpamPresent);

extern "C" void CLogT_WriteTopo(
    void*               handle,
    uint32_t            nodeId,
    uint32_t            nodeType);

/*
* CLog.T log write operations.
*/
extern "C" void CLogT_WriteLog(
    void*               handle,
    uint64_t            time,
    uint32_t            nodeId,
    uint32_t            channel,
    const uint32_t*     flit,
    uint32_t            flitLength);
