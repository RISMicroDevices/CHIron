#include "cohestra_axi.hpp"

#include <new>


// Implementation of: Cohestra::AXI text encoders
namespace Cohestra::AXI {

    const char* ToString(Burst burst) noexcept
    {
        switch (burst)
        {
            case Burst::FIXED:  return "FIXED";
            case Burst::INCR:   return "INCR";
            case Burst::WRAP:   return "WRAP";
            default:            return "UNKNOWN";
        }
    }

    const char* ToString(Resp resp) noexcept
    {
        switch (resp)
        {
            case Resp::OKAY:    return "OKAY";
            case Resp::EXOKAY:  return "EXOKAY";
            case Resp::SLVERR:  return "SLVERR";
            case Resp::DECERR:  return "DECERR";
            default:            return "UNKNOWN";
        }
    }
}


// Implementation of: AXIMemory lane accessors & text helpers
namespace {

    size_t SanitizeDataBits(size_t dataBits) noexcept
    {
        switch (dataBits)
        {
            case 64:
            case 128:
            case 256:
            case 512:
            case 1024:
                return dataBits;

            default:
                spdlog::default_logger()->error(
                    "AXIMemory: unsupported data width of {} bit(s), must be one of 64, 128, 256, 512, 1024; falling back to 64.",
                    dataBits);
                return 64;
        }
    }

    void DataSetByte(std::vector<uint64_t>& data, size_t byte, uint8_t value) noexcept
    {
        size_t lane = byte / 8;
        if (lane >= data.size())
            return;

        size_t shift = (byte % 8) * 8;
        data[lane] = (data[lane] & ~(uint64_t(0xFF) << shift))
                   | (uint64_t(value) << shift);
    }

    std::string DumpAXIData(const std::vector<uint64_t>& data, size_t bytes)
    {
        static const char HEX[] = "0123456789abcdef";

        std::string str;
        str.reserve(bytes * 2 + 4);
        str.append("[ ");

        for (size_t i = bytes; i-- > 0;)
        {
            uint8_t byte = Cohestra::AXI::DataByteAt(data, i);
            str.push_back(HEX[byte >> 4]);
            str.push_back(HEX[byte & 0xF]);
            str.push_back(' ');
        }

        str.append("]");
        return str;
    }
}


// Implementation of: class Cohestra::AXIEventBase
namespace Cohestra {

    AXIEventBase::AXIEventBase(uint64_t time) noexcept
        : time  (time)
    { }

    uint64_t AXIEventBase::GetTime() const noexcept
    {
        return time;
    }
}


// Implementation of: class Cohestra::AXIWriteAcceptEvent
namespace Cohestra {

    AXIWriteAcceptEvent::AXIWriteAcceptEvent(uint64_t time, const AXI::FiredChannelAW& channel) noexcept
        : AXIEventBase  (time)
        , channel       (channel)
    { }

    const AXI::FiredChannelAW& AXIWriteAcceptEvent::GetChannel() const noexcept
    {
        return channel;
    }
}


// Implementation of: class Cohestra::AXIWriteDataEvent
namespace Cohestra {

    AXIWriteDataEvent::AXIWriteDataEvent(uint64_t time, const AXI::FiredChannelW& channel) noexcept
        : AXIEventBase  (time)
        , channel       (channel)
    { }

    const AXI::FiredChannelW& AXIWriteDataEvent::GetChannel() const noexcept
    {
        return channel;
    }
}


// Implementation of: class Cohestra::AXIWriteResponseEvent
namespace Cohestra {

    AXIWriteResponseEvent::AXIWriteResponseEvent(uint64_t time, const AXI::FiredChannelB& channel) noexcept
        : AXIEventBase  (time)
        , channel       (channel)
    { }

    const AXI::FiredChannelB& AXIWriteResponseEvent::GetChannel() const noexcept
    {
        return channel;
    }
}


// Implementation of: class Cohestra::AXIReadAcceptEvent
namespace Cohestra {

    AXIReadAcceptEvent::AXIReadAcceptEvent(uint64_t time, const AXI::FiredChannelAR& channel) noexcept
        : AXIEventBase  (time)
        , channel       (channel)
    { }

    const AXI::FiredChannelAR& AXIReadAcceptEvent::GetChannel() const noexcept
    {
        return channel;
    }
}


// Implementation of: class Cohestra::AXIReadDataEvent
namespace Cohestra {

    AXIReadDataEvent::AXIReadDataEvent(uint64_t time, const AXI::FiredChannelR& channel) noexcept
        : AXIEventBase  (time)
        , channel       (channel)
    { }

    const AXI::FiredChannelR& AXIReadDataEvent::GetChannel() const noexcept
    {
        return channel;
    }
}


// Implementation of: class Cohestra::AXIReadResponseEvent
namespace Cohestra {

    AXIReadResponseEvent::AXIReadResponseEvent(uint64_t time, const AXI::FiredChannelR& channel) noexcept
        : AXIEventBase  (time)
        , channel       (channel)
    { }

    const AXI::FiredChannelR& AXIReadResponseEvent::GetChannel() const noexcept
    {
        return channel;
    }
}


// Implementation of: class Cohestra::AXIWriteTransaction
namespace Cohestra {

    AXIWriteTransaction::AXIWriteTransaction(const AXI::FiredChannelAW& request) noexcept
        : request   (request)
    { }
}


// Implementation of: class Cohestra::AXIReadTransaction
namespace Cohestra {

    AXIReadTransaction::AXIReadTransaction(const AXI::FiredChannelAR& request) noexcept
        : request   (request)
    { }
}


// Implementation of: class Cohestra::AXIWriteCompleteEvent
namespace Cohestra {

    AXIWriteCompleteEvent::AXIWriteCompleteEvent(uint64_t time, const AXIWriteTransaction& transaction) noexcept
        : AXIEventBase  (time)
        , transaction   (transaction)
    { }

    const AXIWriteTransaction& AXIWriteCompleteEvent::GetTransaction() const noexcept
    {
        return transaction;
    }
}


// Implementation of: class Cohestra::AXIReadCompleteEvent
namespace Cohestra {

    AXIReadCompleteEvent::AXIReadCompleteEvent(uint64_t time, const AXIReadTransaction& transaction) noexcept
        : AXIEventBase  (time)
        , transaction   (transaction)
    { }

    const AXIReadTransaction& AXIReadCompleteEvent::GetTransaction() const noexcept
    {
        return transaction;
    }
}


// Implementation of: class Cohestra::AXIMonitor
namespace Cohestra {

    AXIMonitor::AXIMonitor(std::shared_ptr<AXIMonitorInterface> busInterface, size_t portIndex) noexcept
        : busInterface  (busInterface)
        , busPortIndex  (portIndex)
    { }

    void AXIMonitor::TickPostHandshake(uint64_t time) noexcept
    {
        this->time = time;

        if (!busInterface)
            return;

        busInterface->TickPostHandshake(time);

        // drain the beats observed on all five channels into the Push functions
        while (auto channel = busInterface->PopAW(busPortIndex))
            PushAW(*channel);

        while (auto channel = busInterface->PopW(busPortIndex))
            PushW(*channel);

        while (auto channel = busInterface->PopB(busPortIndex))
            PushB(*channel);

        while (auto channel = busInterface->PopAR(busPortIndex))
            PushAR(*channel);

        while (auto channel = busInterface->PopR(busPortIndex))
            PushR(*channel);
    }

    void AXIMonitor::PushAW(const AXI::FiredChannelAW& channel) noexcept
    {
        events->OnWriteAccept(channel.Time, channel);

        activeWrites.emplace_back(channel);
    }

    void AXIMonitor::PushW(const AXI::FiredChannelW& channel) noexcept
    {
        events->OnWriteData(channel.Time, channel);

        // AXI4 write data follows the order of the write addresses, so the
        // beat binds to the oldest write with pending data
        for (auto& write : activeWrites)
        {
            if (write.dataDone)
                continue;

            write.data.push_back(channel);

            if (channel.Bundle.Last)
                write.dataDone = true;

            break;
        }
    }

    void AXIMonitor::PushB(const AXI::FiredChannelB& channel) noexcept
    {
        events->OnWriteResponse(channel.Time, channel);

        // complete the oldest write with a matching ID
        for (auto iter = activeWrites.begin(); iter != activeWrites.end(); iter++)
        {
            if (iter->request.Bundle.Id != channel.Bundle.Id)
                continue;

            iter->response = channel;

            events->OnWriteComplete(channel.Time, *iter);

            activeWrites.erase(iter);
            countCompletedWrites++;
            break;
        }
    }

    void AXIMonitor::PushAR(const AXI::FiredChannelAR& channel) noexcept
    {
        events->OnReadAccept(channel.Time, channel);

        activeReads[channel.Bundle.Id].emplace_back(channel);
    }

    void AXIMonitor::PushR(const AXI::FiredChannelR& channel) noexcept
    {
        events->OnReadData(channel.Time, channel);

        if (channel.Bundle.Last)
            events->OnReadResponse(channel.Time, channel);

        // the beat binds to the oldest active read of its ID
        auto iter = activeReads.find(channel.Bundle.Id);
        if (iter == activeReads.end() || iter->second.empty())
            return;

        auto& read = iter->second.front();
        read.data.push_back(channel);

        if (channel.Bundle.Last)
        {
            events->OnReadComplete(channel.Time, read);

            iter->second.pop_front();

            if (iter->second.empty())
                activeReads.erase(iter);

            countCompletedReads++;
        }
    }

    uint64_t AXIMonitor::GetCompletedWriteCount() const noexcept
    {
        return countCompletedWrites;
    }

    uint64_t AXIMonitor::GetCompletedReadCount() const noexcept
    {
        return countCompletedReads;
    }
}


// Implementation of: class Cohestra::AXIMemory (configuration entries)
namespace Cohestra {

    const ConfigurationEntryBack<uint64_t> AXIMemory::CONFIG_AXI_MEMORY_START
        ("AXI_MEMORY_START", "start", "memory", 0x80000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> AXIMemory::CONFIG_AXI_MEMORY_END
        ("AXI_MEMORY_END", "end", "memory", 0xA0000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint32_t> AXIMemory::CONFIG_AXI_MEMORY_READ_DEPTH
        ("AXI_MEMORY_READ_DEPTH", "read.depth", "memory", 0, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> AXIMemory::CONFIG_AXI_MEMORY_WRITE_DEPTH
        ("AXI_MEMORY_WRITE_DEPTH", "write.depth", "memory", 0, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> AXIMemory::CONFIG_AXI_MEMORY_READ_LATENCY
        ("AXI_MEMORY_READ_LATENCY", "read.latency", "memory", 0, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> AXIMemory::CONFIG_AXI_MEMORY_WRITE_LATENCY
        ("AXI_MEMORY_WRITE_LATENCY", "write.latency", "memory", 0, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> AXIMemory::CONFIG_AXI_MEMORY_CYCLE_UNIT
        ("AXI_MEMORY_CYCLE_UNIT", "cycle.unit", "memory", 1, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<bool> AXIMemory::CONFIG_AXI_MEMORY_OOOR
        ("AXI_MEMORY_OOOR", "ooor", "memory", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> AXIMemory::CONFIG_AXI_MEMORY_VERBOSE
        ("AXI_MEMORY_VERBOSE", "verbose", "memory", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> AXIMemory::CONFIG_AXI_MEMORY_VERBOSE_DATA
        ("AXI_MEMORY_VERBOSE_DATA", "verbose.data", "memory", false, DeserializeBool, SerializeBool);
}


// Implementation of: class Cohestra::AXIMemory::WriteTransaction
namespace Cohestra {

    AXIMemory::WriteTransaction::WriteTransaction(const AXI::FiredChannelAW& request) noexcept
        : request   (request)
    { }
}


// Implementation of: class Cohestra::AXIMemory::ReadTransaction
namespace Cohestra {

    AXIMemory::ReadTransaction::ReadTransaction(const AXI::FiredChannelAR& request) noexcept
        : request   (request)
    { }
}


// Implementation of: class Cohestra::AXIMemory
namespace Cohestra {

    AXIMemory::AXIMemory(size_t dataBits, unsigned int seed) noexcept
        : dataBits      (SanitizeDataBits(dataBits))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , pmem          (nullptr)
        , pmemExternal  (false)
        , seed          (seed)
        , rand          (seed)
        , memoryStart   (CONFIG_AXI_MEMORY_START.defaultValue)
        , memoryEnd     (CONFIG_AXI_MEMORY_END.defaultValue)
        , readDepth     (CONFIG_AXI_MEMORY_READ_DEPTH.defaultValue)
        , writeDepth    (CONFIG_AXI_MEMORY_WRITE_DEPTH.defaultValue)
        , readLatency   (CONFIG_AXI_MEMORY_READ_LATENCY.defaultValue)
        , writeLatency  (CONFIG_AXI_MEMORY_WRITE_LATENCY.defaultValue)
        , cycleUnit     (CONFIG_AXI_MEMORY_CYCLE_UNIT.defaultValue)
        , ooor          (CONFIG_AXI_MEMORY_OOOR.defaultValue)
        , verbose       (CONFIG_AXI_MEMORY_VERBOSE.defaultValue)
        , verboseData   (CONFIG_AXI_MEMORY_VERBOSE_DATA.defaultValue)
    { }

    AXIMemory::AXIMemory(size_t dataBits, unsigned int seed, uint8_t* pmem) noexcept
        : dataBits      (SanitizeDataBits(dataBits))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , pmem          (pmem)
        , pmemExternal  (true)
        , seed          (seed)
        , rand          (seed)
        , memoryStart   (CONFIG_AXI_MEMORY_START.defaultValue)
        , memoryEnd     (CONFIG_AXI_MEMORY_END.defaultValue)
        , readDepth     (CONFIG_AXI_MEMORY_READ_DEPTH.defaultValue)
        , writeDepth    (CONFIG_AXI_MEMORY_WRITE_DEPTH.defaultValue)
        , readLatency   (CONFIG_AXI_MEMORY_READ_LATENCY.defaultValue)
        , writeLatency  (CONFIG_AXI_MEMORY_WRITE_LATENCY.defaultValue)
        , cycleUnit     (CONFIG_AXI_MEMORY_CYCLE_UNIT.defaultValue)
        , ooor          (CONFIG_AXI_MEMORY_OOOR.defaultValue)
        , verbose       (CONFIG_AXI_MEMORY_VERBOSE.defaultValue)
        , verboseData   (CONFIG_AXI_MEMORY_VERBOSE_DATA.defaultValue)
    { }

    AXIMemory::AXIMemory(std::shared_ptr<AXISlaveInterface> busInterface, size_t portIndex,
                         unsigned int seed) noexcept
        : busInterface  (busInterface)
        , busPortIndex  (portIndex)
        , dataBits      (SanitizeDataBits(busInterface ? busInterface->GetPortDataBits(portIndex) : 0))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , pmem          (nullptr)
        , pmemExternal  (false)
        , seed          (seed)
        , rand          (seed)
        , memoryStart   (CONFIG_AXI_MEMORY_START.defaultValue)
        , memoryEnd     (CONFIG_AXI_MEMORY_END.defaultValue)
        , readDepth     (CONFIG_AXI_MEMORY_READ_DEPTH.defaultValue)
        , writeDepth    (CONFIG_AXI_MEMORY_WRITE_DEPTH.defaultValue)
        , readLatency   (CONFIG_AXI_MEMORY_READ_LATENCY.defaultValue)
        , writeLatency  (CONFIG_AXI_MEMORY_WRITE_LATENCY.defaultValue)
        , cycleUnit     (CONFIG_AXI_MEMORY_CYCLE_UNIT.defaultValue)
        , ooor          (CONFIG_AXI_MEMORY_OOOR.defaultValue)
        , verbose       (CONFIG_AXI_MEMORY_VERBOSE.defaultValue)
        , verboseData   (CONFIG_AXI_MEMORY_VERBOSE_DATA.defaultValue)
    { }

    AXIMemory::AXIMemory(size_t dataBits, unsigned int seed,
                         uint64_t memoryStart, uint64_t memoryEnd,
                         uint32_t readDepth, uint32_t writeDepth,
                         uint32_t readLatency, uint32_t writeLatency, uint32_t cycleUnit,
                         bool ooor, bool verbose, bool verboseData) noexcept
        : dataBits      (SanitizeDataBits(dataBits))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , pmem          (nullptr)
        , pmemExternal  (false)
        , seed          (seed)
        , rand          (seed)
        , memoryStart   (memoryStart)
        , memoryEnd     (memoryEnd)
        , readDepth     (readDepth)
        , writeDepth    (writeDepth)
        , readLatency   (readLatency)
        , writeLatency  (writeLatency)
        , cycleUnit     (cycleUnit)
        , ooor          (ooor)
        , verbose       (verbose)
        , verboseData   (verboseData)
    {
        if (memoryEnd <= memoryStart)
            logger->error("AXIMemory: memory window end ({:#x}) must be greater than start ({:#x}).",
                memoryEnd, memoryStart);
    }

    AXIMemory::AXIMemory(size_t dataBits, unsigned int seed, uint8_t* pmem,
                         uint64_t memoryStart, uint64_t memoryEnd,
                         uint32_t readDepth, uint32_t writeDepth,
                         uint32_t readLatency, uint32_t writeLatency, uint32_t cycleUnit,
                         bool ooor, bool verbose, bool verboseData) noexcept
        : dataBits      (SanitizeDataBits(dataBits))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , pmem          (pmem)
        , pmemExternal  (true)
        , seed          (seed)
        , rand          (seed)
        , memoryStart   (memoryStart)
        , memoryEnd     (memoryEnd)
        , readDepth     (readDepth)
        , writeDepth    (writeDepth)
        , readLatency   (readLatency)
        , writeLatency  (writeLatency)
        , cycleUnit     (cycleUnit)
        , ooor          (ooor)
        , verbose       (verbose)
        , verboseData   (verboseData)
    {
        if (memoryEnd <= memoryStart)
            logger->error("AXIMemory: memory window end ({:#x}) must be greater than start ({:#x}).",
                memoryEnd, memoryStart);
    }

    AXIMemory::AXIMemory(std::shared_ptr<AXISlaveInterface> busInterface, size_t portIndex,
                         unsigned int seed,
                         uint64_t memoryStart, uint64_t memoryEnd,
                         uint32_t readDepth, uint32_t writeDepth,
                         uint32_t readLatency, uint32_t writeLatency, uint32_t cycleUnit,
                         bool ooor, bool verbose, bool verboseData) noexcept
        : busInterface  (busInterface)
        , busPortIndex  (portIndex)
        , dataBits      (SanitizeDataBits(busInterface ? busInterface->GetPortDataBits(portIndex) : 0))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , pmem          (nullptr)
        , pmemExternal  (false)
        , seed          (seed)
        , rand          (seed)
        , memoryStart   (memoryStart)
        , memoryEnd     (memoryEnd)
        , readDepth     (readDepth)
        , writeDepth    (writeDepth)
        , readLatency   (readLatency)
        , writeLatency  (writeLatency)
        , cycleUnit     (cycleUnit)
        , ooor          (ooor)
        , verbose       (verbose)
        , verboseData   (verboseData)
    {
        if (memoryEnd <= memoryStart)
            logger->error("AXIMemory: memory window end ({:#x}) must be greater than start ({:#x}).",
                memoryEnd, memoryStart);
    }

    AXIMemory::~AXIMemory() noexcept
    {
        if (!pmemExternal)
            delete[] pmem;
    }

    bool AXIMemory::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        bool good = LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_START,         memoryStart)   &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_END,           memoryEnd)     &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_READ_DEPTH,    readDepth)     &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_WRITE_DEPTH,   writeDepth)    &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_READ_LATENCY,  readLatency)   &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_WRITE_LATENCY, writeLatency)  &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_CYCLE_UNIT,    cycleUnit)     &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_OOOR,          ooor)          &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_VERBOSE,       verbose)       &&
                    LoadConfigurationEntry(loader, &CONFIG_AXI_MEMORY_VERBOSE_DATA,  verboseData);

        if (good && memoryEnd <= memoryStart)
        {
            logger->error("AXIMemory: memory window end ({:#x}) must be greater than start ({:#x}).",
                memoryEnd, memoryStart);
            return false;
        }

        return good;
    }

    void AXIMemory::TickPreHandshake(uint64_t time) noexcept
    {
        this->time = time;

        if (busInterface)
            busInterface->TickPreHandshake(time);

        // pick the next read to serve when no read is currently sending
        if (!preparedRead)
        {
            std::shared_ptr<ReadTransaction> picked = nullptr;

            if (ooor)
            {
                // AXI requires same-ID reads to be returned in issue order,
                // so only the oldest active read of each ID is a legal
                // candidate for an out-of-order pick.
                std::vector<std::shared_ptr<ReadTransaction>> candidates;

                for (auto iter = activeReads.begin(); iter != activeReads.end(); iter++)
                {
                    bool blockedByOlder = false;

                    for (auto prior = activeReads.begin(); prior != iter; prior++)
                        if ((*prior)->request.Bundle.Id == (*iter)->request.Bundle.Id)
                        {
                            blockedByOlder = true;
                            break;
                        }

                    if (!blockedByOlder)
                        candidates.push_back(*iter);
                }

                if (!candidates.empty())
                {
                    auto& randPicked = candidates[rand() % candidates.size()];

                    if (uint64_t(readLatency) * cycleUnit <= time - randPicked->request.Time)
                        picked = randPicked;
                }
            }
            else
            {
                for (auto& seqPicked : activeReads)
                {
                    if (uint64_t(readLatency) * cycleUnit > time - seqPicked->request.Time)
                        continue;

                    picked = seqPicked;
                    break;
                }
            }

            preparedRead = picked;
        }

        if (!busInterface)
            return;

        // complete the channel interactions of the owned interface port

        // downstream accepts (AW before W, so a same-cycle AW+W pair works)
        if (auto aw = busInterface->PeekAW(busPortIndex))
            if (PushAW(*aw))
                busInterface->PopAW(busPortIndex);

        if (auto w = busInterface->PeekW(busPortIndex))
            if (PushW(*w))
                busInterface->PopW(busPortIndex);

        if (auto ar = busInterface->PeekAR(busPortIndex))
            if (PushAR(*ar))
                busInterface->PopAR(busPortIndex);

        // downstream responses
        if (auto b = PeekB())
            if (busInterface->PushB(busPortIndex, *b))
                PopB();

        if (auto r = PeekR())
            if (busInterface->PushR(busPortIndex, *r))
                PopR();
    }

    void AXIMemory::TickPostHandshake(uint64_t time) noexcept
    {
        // retire driven B / R beats on the settled mid-cycle handshake
        if (busInterface)
            busInterface->TickPostHandshake(time);
    }

    bool AXIMemory::PushAW(const AXI::BundleChannelAW& channel) noexcept
    {
        if (writeDepth && writeDepth <= activeWrites.size() + finishedWrites.size())
        {
            ReportBackpressure("AW denied (write depth reached)");
            return false;
        }

        if ((size_t(1) << channel.Size) > dataBytes)
        {
            ReportViolation("AW size overflow, denied");
            return false;
        }

        switch (channel.Burst)
        {
            case AXI::Burst::FIXED:
            case AXI::Burst::INCR:
            case AXI::Burst::WRAP:
                break;

            default:
                ReportViolation("AW with unknown burst type, denied");
                return false;
        }

        if (verbose)
            logger->debug("AXIMemory: [fire AW] id: {:#x}, addr: {:#x}, size: {:#x}, burst: {}, len: {:#x} at time {}.",
                uint64_t(channel.Id), channel.Addr, uint64_t(channel.Size),
                AXI::ToString(channel.Burst), uint64_t(channel.Len), time);

        activeWrites.push_back(std::make_shared<WriteTransaction>(
            AXI::FiredChannelAW { time, channel }));

        return true;
    }

    bool AXIMemory::PushW(const AXI::BundleChannelW& channel) noexcept
    {
        if (activeWrites.empty())
        {
            ReportViolation("unexpected W before AW, denied");
            return false;
        }

        auto trans = activeWrites.front();

        if (trans->dataDone)
        {
            ReportViolation("W beat after WLAST, denied");
            return false;
        }

        if (verbose)
        {
            if (verboseData)
                logger->debug("AXIMemory: [fire W] (id: {:#x}, addr: {:#x}), last: {}, data: {} at time {}.",
                    uint64_t(trans->request.Bundle.Id), trans->request.Bundle.Addr,
                    uint64_t(channel.Last), DumpAXIData(channel.Data, dataBytes), time);
            else
                logger->debug("AXIMemory: [fire W] (id: {:#x}, addr: {:#x}), last: {} at time {}.",
                    uint64_t(trans->request.Bundle.Id), trans->request.Bundle.Addr,
                    uint64_t(channel.Last), time);
        }

        trans->data.push_back(AXI::FiredChannelW { time, channel });

        if (channel.Last)
        {
            trans->dataDone = true;

            EnsureAllocated();

            //
            uint64_t    address  = trans->request.Bundle.Addr;
            AXI::Burst  burst    = trans->request.Bundle.Burst;

            size_t      numBytes = size_t(1) << trans->request.Bundle.Size;
            size_t      burstLen = size_t(trans->request.Bundle.Len) + 1;

            if (burst == AXI::Burst::INCR || burst == AXI::Burst::WRAP)
                address = address & ~(uint64_t(numBytes) - 1);

            // confirm memory write
            for (auto& beat : trans->data)
            {
                uint64_t paddr = (address & ~(uint64_t(dataBytes) - 1)) - memoryStart;

                bool outOfBound = false;
                for (size_t i = 0; i < dataBytes; i++)
                {
                    if (!AXI::StrbBitAt(beat.Bundle.Strb, i))
                        continue;

                    uint64_t bytePAddr = paddr + i;

                    if (bytePAddr >= Size())
                    {
                        outOfBound = true;
                        continue;
                    }

                    pmem[bytePAddr] = AXI::DataByteAt(beat.Bundle.Data, i);
                }

                if (outOfBound)
                {
                    logger->warn("AXIMemory: memory write out of bound backing (addr={:#x}, size={:#x}, burst={}, len={:#x}) at time {} with last beat at time {}.",
                        trans->request.Bundle.Addr, uint64_t(trans->request.Bundle.Size),
                        AXI::ToString(trans->request.Bundle.Burst), uint64_t(trans->request.Bundle.Len),
                        trans->request.Time, beat.Time);

                    countDeniedProtocol++;
                }

                address = AXI::AdvanceBurstAddress(address, burst, numBytes, burstLen);
            }

            //
            trans->response.Bundle.Id   = trans->request.Bundle.Id;
            trans->response.Bundle.Resp = AXI::Resp::OKAY;

            finishedWrites.push_back(trans);
            activeWrites.pop_front();
        }

        return true;
    }

    bool AXIMemory::PushAR(const AXI::BundleChannelAR& channel) noexcept
    {
        if (readDepth && readDepth <= activeReads.size())
        {
            ReportBackpressure("AR denied (read depth reached)");
            return false;
        }

        if ((size_t(1) << channel.Size) > dataBytes)
        {
            ReportViolation("AR size overflow, denied");
            return false;
        }

        switch (channel.Burst)
        {
            case AXI::Burst::FIXED:
            case AXI::Burst::INCR:
            case AXI::Burst::WRAP:
                break;

            default:
                ReportViolation("AR with unknown burst type, denied");
                return false;
        }

        // AXI permits multiple outstanding reads with the same ID. Same-ID
        // ordering is preserved by the serving policy in TickPreHandshake:
        // the sequential pick scans activeReads in acceptance order, and the
        // ooor pick is restricted to the oldest active read of each ID.
        if (verbose)
            logger->debug("AXIMemory: [fire AR] id: {:#x}, addr: {:#x}, size: {:#x}, burst: {}, len: {:#x} at time {}.",
                uint64_t(channel.Id), channel.Addr, uint64_t(channel.Size),
                AXI::ToString(channel.Burst), uint64_t(channel.Len), time);

        auto trans = std::make_shared<ReadTransaction>(
            AXI::FiredChannelAR { time, channel });

        // confirm read
        uint64_t    address  = channel.Addr;
        AXI::Burst  burst    = channel.Burst;

        size_t      numBytes = size_t(1) << channel.Size;
        size_t      burstLen = size_t(channel.Len) + 1;

        if (burst == AXI::Burst::INCR || burst == AXI::Burst::WRAP)
            address = address & ~(uint64_t(numBytes) - 1);

        EnsureAllocated();

        for (size_t i = 0; i < burstLen; i++)
        {
            auto& beat = trans->data.emplace_back();

            beat.Bundle.Id      = channel.Id;
            beat.Bundle.Resp    = AXI::Resp::OKAY;
            beat.Bundle.Last    = (i + 1) == burstLen;
            beat.Bundle.Data.assign(dataLanes, 0);

            uint64_t paddr = (address & ~(uint64_t(dataBytes) - 1)) - memoryStart;

            bool outOfBound = false;
            for (size_t j = 0; j < numBytes; j++)
            {
                uint64_t bytePAddr = paddr + j;

                if (bytePAddr >= Size())
                {
                    outOfBound = true;
                    continue;
                }

                DataSetByte(beat.Bundle.Data, j, pmem[bytePAddr]);
            }

            if (outOfBound)
            {
                logger->warn("AXIMemory: memory read out of bound backing (addr={:#x}, size={:#x}, burst={}, len={:#x}) at time {}.",
                    channel.Addr, uint64_t(channel.Size), AXI::ToString(channel.Burst),
                    uint64_t(channel.Len), time);

                countDeniedProtocol++;
            }

            address = AXI::AdvanceBurstAddress(address, burst, numBytes, burstLen);
        }

        activeReads.push_back(trans);

        return true;
    }

    bool AXIMemory::HasB() const noexcept
    {
        if (finishedWrites.empty())
            return false;

        const auto& finishedWrite = finishedWrites.front();

        return uint64_t(writeLatency) * cycleUnit <= time - finishedWrite->data.back().Time;
    }

    std::optional<AXI::BundleChannelB> AXIMemory::PeekB() const noexcept
    {
        if (!HasB())
            return std::nullopt;

        return { finishedWrites.front()->response.Bundle };
    }

    std::optional<AXI::BundleChannelB> AXIMemory::PopB() noexcept
    {
        if (!HasB())
            return std::nullopt;

        auto finishedWrite = finishedWrites.front();
        finishedWrites.pop_front();

        finishedWrite->response.Time = time;

        if (verbose)
            logger->debug("AXIMemory: [fire B] id: {:#x}, resp: {} at time {}.",
                uint64_t(finishedWrite->response.Bundle.Id),
                AXI::ToString(finishedWrite->response.Bundle.Resp), time);

        return { finishedWrite->response.Bundle };
    }

    bool AXIMemory::HasR() const noexcept
    {
        return preparedRead != nullptr;
    }

    std::optional<AXI::BundleChannelR> AXIMemory::PeekR() const noexcept
    {
        if (!preparedRead)
            return std::nullopt;

        return { preparedRead->data[preparedRead->dataPending].Bundle };
    }

    std::optional<AXI::BundleChannelR> AXIMemory::PopR() noexcept
    {
        if (!preparedRead)
            return std::nullopt;

        auto trans = preparedRead;

        auto& beat = trans->data[trans->dataPending];
        beat.Time = time;

        AXI::BundleChannelR channel = beat.Bundle;

        if (verbose)
        {
            if (verboseData)
                logger->debug("AXIMemory: [fire R] id: {:#x}, (addr: {:#x}), resp: {}, last: {}, data: {} at time {}.",
                    uint64_t(channel.Id), trans->request.Bundle.Addr,
                    AXI::ToString(channel.Resp), uint64_t(channel.Last),
                    DumpAXIData(channel.Data, dataBytes), time);
            else
                logger->debug("AXIMemory: [fire R] id: {:#x}, (addr: {:#x}), resp: {}, last: {} at time {}.",
                    uint64_t(channel.Id), trans->request.Bundle.Addr,
                    AXI::ToString(channel.Resp), uint64_t(channel.Last), time);
        }

        trans->dataPending++;

        if (channel.Last || trans->dataPending == trans->data.size())
        {
            for (auto iter = activeReads.begin(); iter != activeReads.end(); iter++)
            {
                if (iter->get() == trans.get())
                {
                    activeReads.erase(iter);
                    break;
                }
            }

            preparedRead = nullptr;
        }

        return { channel };
    }

    size_t AXIMemory::GetDataBits() const noexcept
    {
        return dataBits;
    }

    size_t AXIMemory::GetDataBytes() const noexcept
    {
        return dataBytes;
    }

    uint8_t* AXIMemory::Memory() noexcept
    {
        return pmem;
    }

    const uint8_t* AXIMemory::Memory() const noexcept
    {
        return pmem;
    }

    uint8_t& AXIMemory::Access(uint64_t addr) noexcept
    {
        EnsureAllocated();

        if (!pmem || !Accessible(addr))
        {
            logger->warn("AXIMemory: direct access to {:#x} outside the configured memory window [{:#x}, {:#x}) at time {}.",
                addr, memoryStart, memoryEnd, time);

            countDeniedProtocol++;

            static uint8_t dummy = 0;
            return dummy;
        }

        return pmem[addr - memoryStart];
    }

    uint8_t AXIMemory::Access(uint64_t addr) const noexcept
    {
        // reads of a not yet allocated backing store observe zeroes
        if (!pmem || !Accessible(addr))
            return 0;

        return pmem[addr - memoryStart];
    }

    bool AXIMemory::Accessible(uint64_t addr) const noexcept
    {
        return addr >= memoryStart && addr < memoryEnd;
    }

    size_t AXIMemory::Size() const noexcept
    {
        return memoryEnd - memoryStart;
    }

    uint64_t AXIMemory::GetBackpressureDenialCount() const noexcept
    {
        return countDeniedBackpressure;
    }

    uint64_t AXIMemory::GetProtocolDenialCount() const noexcept
    {
        return countDeniedProtocol;
    }

    std::shared_ptr<spdlog::logger> AXIMemory::GetLogger() const noexcept
    {
        return logger;
    }

    void AXIMemory::SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept
    {
        this->logger = logger;
    }

    bool AXIMemory::IsWActive(uint32_t wid) const noexcept
    {
        for (auto activeWrite : activeWrites)
            if (activeWrite->request.Bundle.Id == wid)
                return true;

        return false;
    }

    void AXIMemory::EnsureAllocated() noexcept
    {
        if (pmem || pmemExternal)
            return;

        pmem = new (std::nothrow) uint8_t[Size()]();

        if (!pmem)
            logger->error("AXIMemory: failed to allocate {} byte(s) of backing store.", Size());
    }

    void AXIMemory::ReportBackpressure(const char* what) noexcept
    {
        countDeniedBackpressure++;

        if (verbose)
            logger->debug("AXIMemory: {} at time {}.", what, time);
    }

    void AXIMemory::ReportViolation(const char* what) noexcept
    {
        countDeniedProtocol++;

        logger->warn("AXIMemory: {} at time {}.", what, time);
    }
}


// Implementation of: AXIMaster lane accessors & burst arrangement helpers
namespace {

    void StrbSetBit(std::vector<uint64_t>& strb, size_t bit) noexcept
    {
        size_t lane = bit / 64;
        if (lane >= strb.size())
            return;

        strb[lane] |= (uint64_t(1) << (bit % 64));
    }

    uint8_t SizeOfBurstBeat(size_t numBytes) noexcept
    {
        uint8_t size = 0;
        while ((size_t(1) << size) < numBytes)
            size++;

        return size;
    }

    bool ValidateBurstParams(uint64_t addr, Cohestra::AXI::Burst burst, size_t numBytes,
                             size_t burstLen, size_t dataBytes) noexcept
    {
        using Burst = Cohestra::AXI::Burst;

        if (!numBytes || (numBytes & (numBytes - 1)) || numBytes > dataBytes)
            return false;

        if (!burstLen)
            return false;

        switch (burst)
        {
            case Burst::FIXED:
                if (burstLen > 16)
                    return false;

                // a FIXED beat must not cross the data-bus boundary
                if ((addr & (uint64_t(dataBytes) - 1)) + numBytes > dataBytes)
                    return false;
                break;

            case Burst::INCR:
                if (burstLen > 256)
                    return false;
                break;

            case Burst::WRAP:
                if (burstLen != 2 && burstLen != 4 && burstLen != 8 && burstLen != 16)
                    return false;
                break;

            default:
                return false;
        }

        return true;
    }

    // Burst arrangement of the width helpers: an access that fits in a single
    // data beat is FIXED ('FIXED is preferred'); a wider access is an aligned
    // INCR, or a WRAP starting at the beat holding 'addr' (critical-word-first).
    Cohestra::AXI::Burst ArrangeHelperBurst(size_t dataBytes, uint64_t addr, size_t accessBytes,
                                            Cohestra::AXIMaster::AccessMode mode,
                                            uint64_t& outAddr, size_t& outNumBytes,
                                            size_t& outBurstLen) noexcept
    {
        using AccessMode = Cohestra::AXIMaster::AccessMode;

        if (accessBytes <= dataBytes)
        {
            outAddr     = addr;
            outNumBytes = accessBytes;
            outBurstLen = 1;
            return Cohestra::AXI::Burst::FIXED;
        }

        outNumBytes = dataBytes;
        outBurstLen = accessBytes / dataBytes;

        if (mode == AccessMode::WRAP_CWF)
        {
            outAddr = addr & ~(uint64_t(dataBytes) - 1);
            return Cohestra::AXI::Burst::WRAP;
        }

        outAddr = addr & ~(uint64_t(accessBytes) - 1);
        return Cohestra::AXI::Burst::INCR;
    }

    // Builds the W beats of a write burst. 'data' covers the burst's address
    // span from its base (FIXED: 'numBytes' bytes replayed on every beat;
    // INCR / WRAP: the aligned burst window); the payload byte of each window
    // offset lands on the addressed byte lane of the beat covering it, with
    // the matching strobe.
    std::vector<Cohestra::AXI::BundleChannelW> ArrangeWriteBeats(
        size_t dataBytes, uint64_t addr, Cohestra::AXI::Burst burst, size_t numBytes,
        size_t burstLen, const std::vector<uint64_t>& data) noexcept
    {
        using Burst = Cohestra::AXI::Burst;

        size_t dataLanes = dataBytes / 8;
        size_t strbLanes = (dataBytes + 63) / 64;

        uint64_t burstBase = addr;
        uint64_t address   = addr;

        if (burst == Burst::INCR || burst == Burst::WRAP)
            address = address & ~(uint64_t(numBytes) - 1);

        if (burst == Burst::INCR)
            burstBase = address;
        else if (burst == Burst::WRAP)
            burstBase = address & ~(numBytes * burstLen - 1);

        std::vector<Cohestra::AXI::BundleChannelW> beats;

        for (size_t i = 0; i < burstLen; i++)
        {
            auto& beat = beats.emplace_back();

            beat.Data.assign(dataLanes, 0);
            beat.Strb.assign(strbLanes, 0);
            beat.Last = (i + 1) == burstLen;

            size_t inBeat = size_t(address & (uint64_t(dataBytes) - 1));
            size_t slice  = size_t(address - burstBase);

            for (size_t j = 0; j < numBytes; j++)
            {
                DataSetByte(beat.Data, inBeat + j, Cohestra::AXI::DataByteAt(data, slice + j));
                StrbSetBit(beat.Strb, inBeat + j);
            }

            address = Cohestra::AXI::AdvanceBurstAddress(address, burst, numBytes, burstLen);
        }

        return beats;
    }
}


// Implementation of: class Cohestra::AXIMaster (configuration entries)
namespace Cohestra {

    const ConfigurationEntryBack<uint32_t> AXIMaster::CONFIG_AXI_MASTER_READ_DEPTH
        ("AXI_MASTER_READ_DEPTH", "read.depth", "master", 0, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> AXIMaster::CONFIG_AXI_MASTER_WRITE_DEPTH
        ("AXI_MASTER_WRITE_DEPTH", "write.depth", "master", 0, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<bool> AXIMaster::CONFIG_AXI_MASTER_VERBOSE
        ("AXI_MASTER_VERBOSE", "verbose", "master", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> AXIMaster::CONFIG_AXI_MASTER_VERBOSE_DATA
        ("AXI_MASTER_VERBOSE_DATA", "verbose.data", "master", false, DeserializeBool, SerializeBool);
}


// Implementation of: class Cohestra::AXIMaster::WriteTransaction
namespace Cohestra {

    AXIMaster::WriteTransaction::WriteTransaction(const AXI::FiredChannelAW& request, WriteFuture future) noexcept
        : request   (request)
        , future    (future)
    { }
}


// Implementation of: class Cohestra::AXIMaster::ReadTransaction
namespace Cohestra {

    AXIMaster::ReadTransaction::ReadTransaction(const AXI::FiredChannelAR& request, ReadFuture future) noexcept
        : request   (request)
        , future    (future)
    { }
}


// Implementation of: class Cohestra::AXIMaster
namespace Cohestra {

    AXIMaster::AXIMaster(size_t dataBits, unsigned int seed) noexcept
        : dataBits      (SanitizeDataBits(dataBits))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , seed          (seed)
        , rand          (seed)
        , readDepth     (CONFIG_AXI_MASTER_READ_DEPTH.defaultValue)
        , writeDepth    (CONFIG_AXI_MASTER_WRITE_DEPTH.defaultValue)
        , verbose       (CONFIG_AXI_MASTER_VERBOSE.defaultValue)
        , verboseData   (CONFIG_AXI_MASTER_VERBOSE_DATA.defaultValue)
    { }

    AXIMaster::AXIMaster(std::shared_ptr<AXIMasterInterface> busInterface, size_t portIndex,
                         unsigned int seed) noexcept
        : busInterface  (busInterface)
        , busPortIndex  (portIndex)
        , dataBits      (SanitizeDataBits(busInterface ? busInterface->GetPortDataBits(portIndex) : 0))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , seed          (seed)
        , rand          (seed)
        , readDepth     (CONFIG_AXI_MASTER_READ_DEPTH.defaultValue)
        , writeDepth    (CONFIG_AXI_MASTER_WRITE_DEPTH.defaultValue)
        , verbose       (CONFIG_AXI_MASTER_VERBOSE.defaultValue)
        , verboseData   (CONFIG_AXI_MASTER_VERBOSE_DATA.defaultValue)
    { }

    AXIMaster::AXIMaster(size_t dataBits, unsigned int seed,
                         uint32_t readDepth, uint32_t writeDepth,
                         bool verbose, bool verboseData) noexcept
        : dataBits      (SanitizeDataBits(dataBits))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , seed          (seed)
        , rand          (seed)
        , readDepth     (readDepth)
        , writeDepth    (writeDepth)
        , verbose       (verbose)
        , verboseData   (verboseData)
    { }

    AXIMaster::AXIMaster(std::shared_ptr<AXIMasterInterface> busInterface, size_t portIndex,
                         unsigned int seed,
                         uint32_t readDepth, uint32_t writeDepth,
                         bool verbose, bool verboseData) noexcept
        : busInterface  (busInterface)
        , busPortIndex  (portIndex)
        , dataBits      (SanitizeDataBits(busInterface ? busInterface->GetPortDataBits(portIndex) : 0))
        , dataBytes     (this->dataBits / 8)
        , dataLanes     (this->dataBits / 64)
        , seed          (seed)
        , rand          (seed)
        , readDepth     (readDepth)
        , writeDepth    (writeDepth)
        , verbose       (verbose)
        , verboseData   (verboseData)
    { }

    bool AXIMaster::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        return LoadConfigurationEntry(loader, &CONFIG_AXI_MASTER_READ_DEPTH,    readDepth)   &&
               LoadConfigurationEntry(loader, &CONFIG_AXI_MASTER_WRITE_DEPTH,   writeDepth)  &&
               LoadConfigurationEntry(loader, &CONFIG_AXI_MASTER_VERBOSE,       verbose)     &&
               LoadConfigurationEntry(loader, &CONFIG_AXI_MASTER_VERBOSE_DATA,  verboseData);
    }

    void AXIMaster::TickPreHandshake(uint64_t time) noexcept
    {
        this->time = time;

        if (!busInterface)
            return;

        busInterface->TickPreHandshake(time);

        // complete the channel interactions of the owned interface port

        // upstream requests (AW before W, so a same-cycle AW+W pair works)
        if (auto aw = PeekAW())
            if (busInterface->PushAW(busPortIndex, *aw))
                PopAW();

        if (auto w = PeekW())
            if (busInterface->PushW(busPortIndex, *w))
                PopW();

        if (auto ar = PeekAR())
            if (busInterface->PushAR(busPortIndex, *ar))
                PopAR();

        // downstream responses
        if (auto b = busInterface->PopB(busPortIndex))
            PushB(*b);

        if (auto r = busInterface->PopR(busPortIndex))
            PushR(*r);
    }

    void AXIMaster::TickPostHandshake(uint64_t time) noexcept
    {
        // retire driven AW / W / AR beats on the settled mid-cycle handshake
        if (busInterface)
            busInterface->TickPostHandshake(time);
    }

    AXIMaster::WriteFuture AXIMaster::DoWrite(uint64_t addr, AXI::Burst burst, size_t numBytes,
                                              size_t burstLen, const std::vector<uint64_t>& data) noexcept
    {
        if (!ValidateBurstParams(addr, burst, numBytes, burstLen, dataBytes))
        {
            logger->warn("AXIMaster: write with invalid burst arguments (addr={:#x}, burst={}, numBytes={:#x}, burstLen={:#x}), denied at time {}.",
                addr, AXI::ToString(burst), numBytes, burstLen, time);

            countDenied++;
            return std::make_shared<AXI::FutureNow<AXIWriteTransaction>>(AXI::Denial::REJECTED_ARGUMENT);
        }

        // the payload covers the burst's address span from its base
        size_t span = burst == AXI::Burst::FIXED ? numBytes : numBytes * burstLen;

        if (data.size() != (span + 7) / 8)
        {
            logger->warn("AXIMaster: write payload of {} lane(s) does not cover the burst span of {:#x} byte(s), denied at time {}.",
                data.size(), span, time);

            countDenied++;
            return std::make_shared<AXI::FutureNow<AXIWriteTransaction>>(AXI::Denial::REJECTED_ARGUMENT);
        }

        if (writeDepth && writeDepth <= activeWrites.size())
        {
            if (verbose)
                logger->debug("AXIMaster: write denied (write depth reached) at time {}.", time);

            countDenied++;
            return std::make_shared<AXI::FutureNow<AXIWriteTransaction>>(AXI::Denial::REJECTED_WRITE_DEPTH);
        }

        //
        WriteFuture future = std::make_shared<AXI::FutureNow<AXIWriteTransaction>>(AXI::Denial::ACCEPTED);

        AXI::FiredChannelAW request;
        request.Bundle.Id     = AllocateWriteId();
        request.Bundle.Addr   = addr;
        request.Bundle.Len    = uint8_t(burstLen - 1);
        request.Bundle.Size   = SizeOfBurstBeat(numBytes);
        request.Bundle.Burst  = burst;

        auto trans = std::make_shared<WriteTransaction>(request, future);
        trans->dataPending = ArrangeWriteBeats(dataBytes, addr, burst, numBytes, burstLen, data);

        activeWrites.push_back(trans);

        return future;
    }

    AXIMaster::ReadFuture AXIMaster::DoRead(uint64_t addr, AXI::Burst burst, size_t numBytes,
                                            size_t burstLen) noexcept
    {
        if (!ValidateBurstParams(addr, burst, numBytes, burstLen, dataBytes))
        {
            logger->warn("AXIMaster: read with invalid burst arguments (addr={:#x}, burst={}, numBytes={:#x}, burstLen={:#x}), denied at time {}.",
                addr, AXI::ToString(burst), numBytes, burstLen, time);

            countDenied++;
            return std::make_shared<AXI::FutureNow<AXIReadTransaction>>(AXI::Denial::REJECTED_ARGUMENT);
        }

        if (readDepth && readDepth <= activeReads.size())
        {
            if (verbose)
                logger->debug("AXIMaster: read denied (read depth reached) at time {}.", time);

            countDenied++;
            return std::make_shared<AXI::FutureNow<AXIReadTransaction>>(AXI::Denial::REJECTED_READ_DEPTH);
        }

        //
        ReadFuture future = std::make_shared<AXI::FutureNow<AXIReadTransaction>>(AXI::Denial::ACCEPTED);

        AXI::FiredChannelAR request;
        request.Bundle.Id     = AllocateReadId();
        request.Bundle.Addr   = addr;
        request.Bundle.Len    = uint8_t(burstLen - 1);
        request.Bundle.Size   = SizeOfBurstBeat(numBytes);
        request.Bundle.Burst  = burst;

        activeReads.push_back(std::make_shared<ReadTransaction>(request, future));

        return future;
    }

    AXIMaster::WriteFuture AXIMaster::Write8(uint64_t addr, uint8_t value, AccessMode mode) noexcept
    {
        uint64_t lanes[1] = { value };

        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 1, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(lanes, lanes + 1));
    }

    AXIMaster::WriteFuture AXIMaster::Write16(uint64_t addr, uint16_t value, AccessMode mode) noexcept
    {
        uint64_t lanes[1] = { value };

        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 2, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(lanes, lanes + 1));
    }

    AXIMaster::WriteFuture AXIMaster::Write32(uint64_t addr, uint32_t value, AccessMode mode) noexcept
    {
        uint64_t lanes[1] = { value };

        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 4, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(lanes, lanes + 1));
    }

    AXIMaster::WriteFuture AXIMaster::Write64(uint64_t addr, uint64_t value, AccessMode mode) noexcept
    {
        uint64_t lanes[1] = { value };

        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 8, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(lanes, lanes + 1));
    }

    AXIMaster::WriteFuture AXIMaster::Write128(uint64_t addr, const uint64_t value[2], AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 16, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(value, value + 2));
    }

    AXIMaster::WriteFuture AXIMaster::Write256(uint64_t addr, const uint64_t value[4], AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 32, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(value, value + 4));
    }

    AXIMaster::WriteFuture AXIMaster::Write512(uint64_t addr, const uint64_t value[8], AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 64, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(value, value + 8));
    }

    AXIMaster::WriteFuture AXIMaster::Write1024(uint64_t addr, const uint64_t value[16], AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 128, mode, addrArranged, numBytes, burstLen);

        return DoWrite(addrArranged, burst, numBytes, burstLen,
                       std::vector<uint64_t>(value, value + 16));
    }

    AXIMaster::ReadFuture AXIMaster::Read8(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 1, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    AXIMaster::ReadFuture AXIMaster::Read16(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 2, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    AXIMaster::ReadFuture AXIMaster::Read32(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 4, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    AXIMaster::ReadFuture AXIMaster::Read64(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 8, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    AXIMaster::ReadFuture AXIMaster::Read128(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 16, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    AXIMaster::ReadFuture AXIMaster::Read256(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 32, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    AXIMaster::ReadFuture AXIMaster::Read512(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 64, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    AXIMaster::ReadFuture AXIMaster::Read1024(uint64_t addr, AccessMode mode) noexcept
    {
        uint64_t addrArranged;
        size_t   numBytes, burstLen;
        AXI::Burst burst = ArrangeHelperBurst(dataBytes, addr, 128, mode, addrArranged, numBytes, burstLen);

        return DoRead(addrArranged, burst, numBytes, burstLen);
    }

    bool AXIMaster::HasAW() const noexcept
    {
        if (activeWrites.empty())
            return false;

        return !activeWrites.front()->requestSent;
    }

    std::optional<AXI::BundleChannelAW> AXIMaster::PeekAW() const noexcept
    {
        if (!HasAW())
            return std::nullopt;

        return { activeWrites.front()->request.Bundle };
    }

    std::optional<AXI::BundleChannelAW> AXIMaster::PopAW() noexcept
    {
        if (!HasAW())
            return std::nullopt;

        auto trans = activeWrites.front();
        trans->requestSent  = true;
        trans->request.Time = time;

        if (verbose)
            logger->debug("AXIMaster: [fire AW] id: {:#x}, addr: {:#x}, size: {:#x}, burst: {}, len: {:#x} at time {}.",
                uint64_t(trans->request.Bundle.Id), trans->request.Bundle.Addr,
                uint64_t(trans->request.Bundle.Size), AXI::ToString(trans->request.Bundle.Burst),
                uint64_t(trans->request.Bundle.Len), time);

        return { trans->request.Bundle };
    }

    bool AXIMaster::HasW() const noexcept
    {
        // write data follows write address order (AXI4): the oldest write with
        // a driven AW and beats left to drive streams next
        for (const auto& write : activeWrites)
        {
            if (!write->requestSent)
                break;

            if (!write->dataPending.empty())
                return true;
        }

        return false;
    }

    std::optional<AXI::BundleChannelW> AXIMaster::PeekW() const noexcept
    {
        for (const auto& write : activeWrites)
        {
            if (!write->requestSent)
                break;

            if (!write->dataPending.empty())
                return { write->dataPending.front() };
        }

        return std::nullopt;
    }

    std::optional<AXI::BundleChannelW> AXIMaster::PopW() noexcept
    {
        for (const auto& write : activeWrites)
        {
            if (!write->requestSent)
                break;

            if (write->dataPending.empty())
                continue;

            AXI::BundleChannelW channel = write->dataPending.front();
            write->dataPending.erase(write->dataPending.begin());
            write->data.push_back(AXI::FiredChannelW { time, channel });

            if (verbose)
            {
                if (verboseData)
                    logger->debug("AXIMaster: [fire W] (id: {:#x}, addr: {:#x}), last: {}, data: {} at time {}.",
                        uint64_t(write->request.Bundle.Id), write->request.Bundle.Addr,
                        uint64_t(channel.Last), DumpAXIData(channel.Data, dataBytes), time);
                else
                    logger->debug("AXIMaster: [fire W] (id: {:#x}, addr: {:#x}), last: {} at time {}.",
                        uint64_t(write->request.Bundle.Id), write->request.Bundle.Addr,
                        uint64_t(channel.Last), time);
            }

            return { channel };
        }

        return std::nullopt;
    }

    bool AXIMaster::HasAR() const noexcept
    {
        if (activeReads.empty())
            return false;

        return !activeReads.front()->requestSent;
    }

    std::optional<AXI::BundleChannelAR> AXIMaster::PeekAR() const noexcept
    {
        if (!HasAR())
            return std::nullopt;

        return { activeReads.front()->request.Bundle };
    }

    std::optional<AXI::BundleChannelAR> AXIMaster::PopAR() noexcept
    {
        if (!HasAR())
            return std::nullopt;

        auto trans = activeReads.front();
        trans->requestSent  = true;
        trans->request.Time = time;

        if (verbose)
            logger->debug("AXIMaster: [fire AR] id: {:#x}, addr: {:#x}, size: {:#x}, burst: {}, len: {:#x} at time {}.",
                uint64_t(trans->request.Bundle.Id), trans->request.Bundle.Addr,
                uint64_t(trans->request.Bundle.Size), AXI::ToString(trans->request.Bundle.Burst),
                uint64_t(trans->request.Bundle.Len), time);

        return { trans->request.Bundle };
    }

    bool AXIMaster::PushB(const AXI::BundleChannelB& channel) noexcept
    {
        // complete the oldest write with a matching ID
        for (auto iter = activeWrites.begin(); iter != activeWrites.end(); iter++)
        {
            auto& write = *iter;

            if (write->request.Bundle.Id != channel.Id)
                continue;

            if (!write->requestSent || !write->dataPending.empty())
            {
                logger->warn("AXIMaster: B of an unfinished write (id: {:#x}), denied at time {}.",
                    uint64_t(channel.Id), time);
                return false;
            }

            write->response = AXI::FiredChannelB { time, channel };

            if (verbose)
                logger->debug("AXIMaster: [fire B] id: {:#x}, resp: {} at time {}.",
                    uint64_t(channel.Id), AXI::ToString(channel.Resp), time);

            AXIWriteTransaction completion(write->request);
            completion.data      = write->data;
            completion.response  = write->response;
            completion.dataDone  = true;

            write->future->Fire(completion);

            activeWrites.erase(iter);
            countCompletedWrites++;
            return true;
        }

        logger->warn("AXIMaster: unexpected B (id: {:#x}) with no matching active write, denied at time {}.",
            uint64_t(channel.Id), time);
        return false;
    }

    bool AXIMaster::PushR(const AXI::BundleChannelR& channel) noexcept
    {
        // the beat binds to the oldest active read of its ID
        for (auto iter = activeReads.begin(); iter != activeReads.end(); iter++)
        {
            auto& read = *iter;

            if (read->request.Bundle.Id != channel.Id)
                continue;

            read->data.push_back(AXI::FiredChannelR { time, channel });

            if (verbose)
            {
                if (verboseData)
                    logger->debug("AXIMaster: [fire R] id: {:#x}, (addr: {:#x}), resp: {}, last: {}, data: {} at time {}.",
                        uint64_t(channel.Id), read->request.Bundle.Addr,
                        AXI::ToString(channel.Resp), uint64_t(channel.Last),
                        DumpAXIData(channel.Data, dataBytes), time);
                else
                    logger->debug("AXIMaster: [fire R] id: {:#x}, (addr: {:#x}), resp: {}, last: {} at time {}.",
                        uint64_t(channel.Id), read->request.Bundle.Addr,
                        AXI::ToString(channel.Resp), uint64_t(channel.Last), time);
            }

            if (!channel.Last)
                return true;

            AXIReadTransaction completion(read->request);
            completion.data = read->data;

            read->future->Fire(completion);

            activeReads.erase(iter);
            countCompletedReads++;
            return true;
        }

        logger->warn("AXIMaster: unexpected R (id: {:#x}) with no matching active read, denied at time {}.",
            uint64_t(channel.Id), time);
        return false;
    }

    size_t AXIMaster::GetDataBits() const noexcept
    {
        return dataBits;
    }

    size_t AXIMaster::GetDataBytes() const noexcept
    {
        return dataBytes;
    }

    uint64_t AXIMaster::GetDeniedCount() const noexcept
    {
        return countDenied;
    }

    uint64_t AXIMaster::GetCompletedWriteCount() const noexcept
    {
        return countCompletedWrites;
    }

    uint64_t AXIMaster::GetCompletedReadCount() const noexcept
    {
        return countCompletedReads;
    }

    std::shared_ptr<spdlog::logger> AXIMaster::GetLogger() const noexcept
    {
        return logger;
    }

    void AXIMaster::SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept
    {
        this->logger = logger;
    }

    bool AXIMaster::IsWActive(uint32_t wid) const noexcept
    {
        for (auto activeWrite : activeWrites)
            if (activeWrite->request.Bundle.Id == wid)
                return true;

        return false;
    }

    bool AXIMaster::IsRActive(uint32_t rid) const noexcept
    {
        for (auto activeRead : activeReads)
            if (activeRead->request.Bundle.Id == rid)
                return true;

        return false;
    }

    uint32_t AXIMaster::AllocateWriteId() noexcept
    {
        while (IsWActive(nextWriteId))
            nextWriteId++;

        return nextWriteId++;
    }

    uint32_t AXIMaster::AllocateReadId() noexcept
    {
        while (IsRActive(nextReadId))
            nextReadId++;

        return nextReadId++;
    }
}
