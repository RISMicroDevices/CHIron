#include "clogdpi.hpp"

#include <atomic>
#include <ios>
#include <iostream>
#include <fstream>
#include <thread>

#include "../common/concurrentqueue.hpp"

#include "clog_t.hpp"


//
struct LogTask {
    uint64_t    time;
    uint32_t    nodeId;
    uint32_t    channel;
    uint32_t    flit[16];
    uint32_t    flitLength;
};

class CLogHandle {
public:
    std::ofstream*                          ofs;
    moodycamel::ConcurrentQueue<LogTask>*   queue;
    std::atomic_bool                        stop;
    std::thread                             worker;
};


//
extern "C" void* CLogT_OpenFile(const char*     path,
                                uint32_t*       status)
{
    CLogHandle* handle = new CLogHandle;

    std::ofstream* ofs = 
        new std::ofstream(path, std::ios_base::out | std::ios_base::trunc);

    if (!*ofs)
    {
        *status = 1;
        std::cout << "[CLog.T] unable to open file: " << path << std::endl;
        return NULL;
    }
    else
    {
        std::cout << "[CLog.T] opened file: " << path << std::endl;
    }

    *status = 0;

    handle->ofs     = ofs;
    handle->queue   = new moodycamel::ConcurrentQueue<LogTask>;
    handle->stop    = false;

    std::atomic_thread_fence(std::memory_order_seq_cst);

    handle->worker = std::thread([=]() -> void {
        LogTask task;
        while (1)
        {
            if (handle->queue->try_dequeue(task)) 
            {
                CLog::CLogT::WriteCHISentenceLog(
                    *handle->ofs,
                    task.time,
                    task.nodeId,
                    CLog::Channel(task.channel),
                    task.flit,
                    task.flitLength
                );
            }
            else if (handle->stop)
                return;
        }
    });

    return handle;
}

extern "C" void CLogT_CloseFile(void*           handle)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    while (chandle->queue->size_approx());

    chandle->stop = true;

    chandle->worker.join();

    chandle->ofs->close();
    delete chandle->ofs;
    delete chandle->queue;
    delete chandle;
}


//
extern "C" void CLogT_WriteIssue(void*          handle,
                                 uint32_t       issue)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceIssue(
        *chandle->ofs,
        CLog::Issue(issue));
}

extern "C" void CLogT_WriteWidthNodeID(void*    handle, 
                                       uint32_t nodeIdWidth)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceNodeIDWidth(
        *chandle->ofs,
        nodeIdWidth);
}

extern "C" void CLogT_WriteWidthAddr(void*      handle, 
                                     uint32_t   addrWidth)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceAddrWidth(
        *chandle->ofs,
        addrWidth);
}

extern "C" void CLogT_WriteWidthReqRSVDC(void*      handle,
                                         uint32_t   reqRsvdcWidth)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceReqRSVDCWidth(
        *chandle->ofs,
        reqRsvdcWidth);
}

extern "C" void CLogT_WriteWidthDatRSVDC(void*      handle, 
                                         uint32_t   datRsvdcWidth)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceDatRSVDCWidth(
        *chandle->ofs,
        datRsvdcWidth);
}

extern "C" void CLogT_WriteWidthData(void*      handle,
                                     uint32_t   dataWidth)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceDataWidth(
        *chandle->ofs,
        dataWidth);
}

extern "C" void CLogT_WriteEnableDataCheck(void*    handle, 
                                           uint32_t dataCheckPresent)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceDataCheckPresent(
        *chandle->ofs,
        dataCheckPresent);
}

extern "C" void CLogT_WriteEnablePoison(void*       handle, 
                                        uint32_t    poisonPresent)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentencePoisonPresent(
        *chandle->ofs,
        poisonPresent);
}

extern "C" void CLogT_WriteEnableMPAM(void*     handle,
                                      uint32_t  mpamPresent)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceMPAMPresent(
        *chandle->ofs,
        mpamPresent);
}

extern "C" void CLogT_WriteTopo(void*       handle, 
                                uint32_t    nodeId, 
                                uint32_t    nodeType)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    CLog::CLogT::WriteCHISentenceTopo(
        *chandle->ofs,
        nodeId,
        CLog::NodeType(nodeType));
}

extern "C" void CLogT_WriteLog(void*            handle,
                               uint64_t         time,
                               uint32_t         nodeId,
                               uint32_t         channel,
                               const uint32_t*  flit,
                               uint32_t         flitLength)
{
    CLogHandle* chandle = (CLogHandle*) handle;

    flitLength = (flitLength + 31) / 32;

    while (!chandle->queue->try_enqueue(LogTask {
        .time       = time,
        .nodeId     = nodeId,
        .channel    = channel,
        .flit       = {
            flit[0],
            flit[1],
            flit[2],
            flit[3],
            flit[4],
            flit[5],
            flit[6],
            flit[7],
            flit[8],
            flit[9],
            flit[10],
            flit[11],
            flit[12],
            flit[13],
            flit[14],
            flit[15]
        },
        .flitLength = flitLength
    }));
}

