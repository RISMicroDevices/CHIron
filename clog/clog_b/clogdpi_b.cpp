#include "clogdpi_b.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <thread>

#include "../../common/concurrentqueue.hpp"

#include "clog_b_tag.hpp"


#define CLOG_B_RECORD_BLOCK_LIMIT           1048576
#define CLOG_B_RECORD_BLOCK_COMPRESSION     1


/*
* CLog.B operation handle and structures.
*/
struct CLogBHandle {
    std::ofstream*                          ofs;
    std::string                             ofpath;
    moodycamel::ConcurrentQueue<CLog::CLogB::TagCHIRecords*>*   
                                            queue;
    std::atomic_bool                        stop;
    std::thread                             worker;

    std::vector<CLog::CLogB::TagCHITopologies::Node>
                                            topos;

    CLog::CLogB::TagCHIRecords*             records;
};


/*
* CLog.B file handle operations implementations.
*/

extern "C" void* CLogB_OpenFile(
    const char*         path,
    uint32_t*           status)
{
    CLogBHandle* handle = new CLogBHandle;

    std::ofstream* ofs =
        new std::ofstream(path, std::ios_base::out | std::ios_base::trunc);

    if (!*ofs)
    {
        *status = 1;
        std::cout << "[CLog.B] unable to open file: " << path << std::endl;
        return NULL;
    }
    else
    {
        std::cout << "[CLog.B] opened file: " << path << std::endl;
    }

    *status = 0;

    handle->ofs     = ofs;
    handle->ofpath  = path;
    handle->queue   = new moodycamel::ConcurrentQueue<CLog::CLogB::TagCHIRecords*>;
    handle->stop    = false;
    handle->records = nullptr;

    std::atomic_thread_fence(std::memory_order_seq_cst);

    handle->worker = std::thread([=]() -> void {
        CLog::CLogB::TagCHIRecords* records;
        while (1)
        {
            if (handle->queue->try_dequeue(records))
            {
                // write tag type
                handle->ofs->write(reinterpret_cast<char*>(&records->type), 1);

                // write tag
                records->Serialize(*handle->ofs);

                delete records;
            }
            else if (handle->stop)
                return;
        }
    });

    return handle;
}

extern "C" void CLogB_CloseFile(
    void*               handle)
{
    CLogBHandle* chandle = (CLogBHandle*) handle;

    while (chandle->queue->size_approx());

    chandle->stop = true;

    chandle->worker.join();

    chandle->ofs->close();

    std::cout << "[CLog.B] closed file: " << chandle->ofpath << std::endl;

    delete chandle->ofs;
    delete chandle->queue;
    if (chandle->records)
        delete chandle->records;
    delete chandle;
}


/*
* CLog.B parameters write operations implementations.
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
    uint32_t            mpamPresent)
{
    //
    CLog::Parameters params;
    params.SetIssue(CLog::Issue(issue));
    params.SetNodeIdWidth(nodeidWidth);
    params.SetReqAddrWidth(addrWidth);
    params.SetReqRSVDCWidth(reqRsvdcWidth);
    params.SetDatRSVDCWidth(datRsvdcWidth);
    params.SetDataWidth(dataWidth);
    params.SetDataCheckPresent(dataCheckPresent);
    params.SetPoisonPresent(poisonPresent);
    params.SetMPAMPresent(mpamPresent);

    //
    CLogBHandle* chandle = (CLogBHandle*) handle;

    CLog::CLogB::TagCHIParameters tag;
    tag.parameters = params;

    // write tag type
    chandle->ofs->write(reinterpret_cast<char*>(&tag.type), 1);

    // write tag
    tag.Serialize(*chandle->ofs);
}


/*
* CLog.B topologies write operations implementations.
*/
extern "C" void CLogB_WriteTopo(
    void*               handle,
    uint32_t            nodeId,
    uint32_t            nodeType)
{
    CLogBHandle* chandle = (CLogBHandle*) handle;

    chandle->topos.push_back(CLog::CLogB::TagCHITopologies::Node {
        .type   = CLog::NodeType(nodeType),
        .id     = uint16_t(nodeId)
    });
}

extern "C" void CLogB_WriteTopoEnd(
    void*               handle)
{
    CLogBHandle* chandle = (CLogBHandle*) handle;

    CLog::CLogB::TagCHITopologies tag;
    tag.nodes = chandle->topos;

    // write tag type
    chandle->ofs->write(reinterpret_cast<char*>(&tag.type), 1);

    // write tag
    tag.Serialize(*chandle->ofs);
}


/*
* CLog.B log record write operations.
*/
extern "C" void CLogB_WriteRecord(
    void*               handle,
    uint64_t            time,
    uint32_t            nodeId,
    uint32_t            channel,
    const uint32_t*     flit,
    uint32_t            flitLength)
{
    CLogBHandle* chandle = (CLogBHandle*) handle;

    uint64_t timeShift = chandle->records ? (time - (
        chandle->records->records.empty() ? chandle->records->head.timeBase
                                          : chandle->records->records.back().timeShift
    )) : 0;

    // allocate or split block
    if ((!chandle->records)                                             // allocate
    ||  (chandle->records->records.size() >= CLOG_B_RECORD_BLOCK_LIMIT) // split on block limit
    ||  (timeShift >= uint64_t(UINT32_MAX)))                            // split on time limit
    {
        if (chandle->records)
            while(!chandle->queue->try_enqueue(chandle->records));  // dispatch block to worker thread

        chandle->records = new CLog::CLogB::TagCHIRecords;
        chandle->records->head.timeBase = time;

        #if CLOG_B_RECORD_BLOCK_COMPRESSION
            chandle->records->head.compressed = 1;
        #else
            chandle->records->head.compressed = 0;
        #endif

        timeShift = 0;
    }

    // append record
    size_t flitLength32 = (flitLength + 3) >> 2;
    std::shared_ptr<uint32_t[]> flitData(new uint32_t[flitLength32]);

    std::memcpy(flitData.get(), flit, flitLength32 << 2);

    chandle->records->records.push_back({
        .timeShift  = uint32_t(timeShift),
        .nodeId     = uint16_t(nodeId),
        .channel    = CLog::Channel(channel),
        .flitLength = uint8_t(flitLength),
        .flit       = flitData
    });
}
