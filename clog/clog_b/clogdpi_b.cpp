#include "clogdpi_b.hpp"

#include <atomic>
#include <cstdint>
#include <ios>
#include <iostream>
#include <fstream>
#include <thread>
#include <cstring>
#include <unordered_map>

#include "../../common/concurrentqueue.hpp"

#include "clog_b.hpp"


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
    uint64_t                                lastRecordTime;
};

std::unordered_map<std::string, CLogBHandle*> SHARED_HANDLES;


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

    std::atomic_thread_fence(std::memory_order_seq_cst);

    handle->worker = std::thread([=]() -> void {
        CLog::CLogB::TagCHIRecords* records;
        while (1)
        {
            if (handle->queue->try_dequeue(records))
            {
                CLog::CLogB::Writer().Next(*handle->ofs, records);
                delete records;
            }
            else if (handle->stop)
                return;
        }
    });

    handle->records = nullptr;

    return handle;
}

extern "C" void CLogB_CloseFile(
    void*               handle)
{
    CLogBHandle* chandle = (CLogBHandle*) handle;

    if (chandle->records)
    {
        while (!chandle->queue->try_enqueue(chandle->records));
        chandle->records = nullptr;
    }

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

extern "C" void CLogB_ShareHandle(
    const char*         id,
    void*               handle)
{
    SHARED_HANDLES[std::string(id)] = (CLogBHandle*) handle;
}

extern "C" int CLogB_UnshareHandle(
    const char*         id)
{
    return SHARED_HANDLES.erase(std::string(id));
}

extern "C" void* CLogB_GetSharedHandle(
    const char*         id)
{
    auto iter = SHARED_HANDLES.find(std::string(id));

    if (iter == SHARED_HANDLES.end())
        return nullptr;

    return iter->second;
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

    CLog::CLogB::Writer().Next(*chandle->ofs, &tag);
}

extern "C" void CLogB_SharedWriteParameters(
    const char*         id,
    uint32_t            issue,
    uint32_t            nodeIdWidth,
    uint32_t            addrWidth,
    uint32_t            reqRsvdcWidth,
    uint32_t            datRsvdcWidth,
    uint32_t            dataWidth,
    uint32_t            dataCheckPresent,
    uint32_t            poisonPresent,
    uint32_t            mpamPresent)
{
    auto iter = SHARED_HANDLES.find(std::string(id));

    if (iter == SHARED_HANDLES.end())
        return;

    CLogB_WriteParameters(
        iter->second,
        issue,
        nodeIdWidth,
        addrWidth,
        reqRsvdcWidth,
        datRsvdcWidth,
        dataWidth,
        dataCheckPresent,
        poisonPresent,
        mpamPresent
    );
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

    CLog::CLogB::Writer().Next(*chandle->ofs, &tag);
}

extern "C" void CLogB_SharedWriteTopo(
    const char*         id,
    uint32_t            nodeId,
    uint32_t            nodeType)
{
    auto iter = SHARED_HANDLES.find(std::string(id));

    if (iter == SHARED_HANDLES.end())
        return;

    CLogB_WriteTopo(
        iter->second,
        nodeId,
        nodeType
    );
}
    
extern "C" void CLogB_SharedWriteTopoEnd(
    const char*         id)
{
    auto iter = SHARED_HANDLES.find(std::string(id));

    if (iter == SHARED_HANDLES.end())
        return;

    CLogB_WriteTopoEnd(
        iter->second
    );
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
    //
    size_t flitLength8 = (flitLength + 7) >> 3;

    //
    CLogBHandle* chandle = (CLogBHandle*) handle;

    uint64_t timeShift = chandle->records ? (time - (
        chandle->records->records.empty() ? chandle->records->head.timeBase
                                          : chandle->lastRecordTime
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
    size_t flitLength32 = (flitLength8 + 3) >> 2;
    std::shared_ptr<uint32_t[]> flitData(new uint32_t[flitLength32]);

    std::memcpy(flitData.get(), flit, flitLength8);

    chandle->records->records.push_back({
        .timeShift  = uint32_t(timeShift),
        .nodeId     = uint16_t(nodeId),
        .channel    = CLog::Channel(channel),
        .flitLength = uint8_t(flitLength8),
        .flit       = flitData
    });

    chandle->lastRecordTime = time;
}

extern "C" void CLogB_SharedWriteRecord(
    const char*         id,
    uint64_t            time,
    uint32_t            nodeId,
    uint32_t            channel,
    const uint32_t*     flit,
    uint32_t            flitLength)
{
    auto iter = SHARED_HANDLES.find(std::string(id));

    if (iter == SHARED_HANDLES.end())
        return;

    CLogB_WriteRecord(
        iter->second,
        time,
        nodeId,
        channel,
        flit,
        flitLength
    );
}
