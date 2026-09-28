#include "earth_model.hpp"


// Implementation of: class EarthModel (configuration entries)
namespace Cohestra {

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_NODE_ID
        ("EARTH_NODE_ID", "nodeid", "earth", 0x10, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_PARALLELISM
        ("EARTH_PARALLELISM", "parallelism", "earth", 8, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_INFLIGHT_SNP
        ("EARTH_INFLIGHT_SNP", "inflight.snp", "earth", 4, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_QUEUE_EVT
        ("EARTH_QUEUE_EVT", "queue.evt", "earth", 2, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_QUEUE_REQ
        ("EARTH_QUEUE_REQ", "queue.req", "earth", 2, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_QUEUE_RSP
        ("EARTH_QUEUE_RSP", "queue.rsp", "earth", 8, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_QUEUE_DAT
        ("EARTH_QUEUE_DAT", "queue.dat", "earth", 16, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_LATENCY_RSP
        ("EARTH_LATENCY_RSP", "latency.rsp", "earth", 4, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> EarthModel::CONFIG_EARTH_LATENCY_DAT
        ("EARTH_LATENCY_DAT", "latency.dat", "earth", 4, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint64_t> EarthModel::CONFIG_EARTH_MEMORY_START
        ("EARTH_MEMORY_START", "memory.start", "earth", 0x80000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> EarthModel::CONFIG_EARTH_MEMORY_END
        ("EARTH_MEMORY_END", "memory.end", "earth", 0xA0000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> EarthModel::CONFIG_EARTH_VERBOSE
        ("EARTH_VERBOSE", "verbose", "earth", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> EarthModel::CONFIG_EARTH_VERBOSE_FLIT
        ("EARTH_VERBOSE_FLIT", "verbose.flit", "earth", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> EarthModel::CONFIG_EARTH_VERBOSE_XACT
        ("EARTH_VERBOSE_XACT", "verbose.xact", "earth", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> EarthModel::CONFIG_EARTH_VERBOSE_STATE
        ("EARTH_VERBOSE_STATE", "verbose.state", "earth", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> EarthModel::CONFIG_EARTH_VERBOSE_INTERNAL
        ("EARTH_VERBOSE_INTERNAL", "verbose.internal", "earth", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<bool> EarthModel::CONFIG_EARTH_VERBOSE_BACKPRESSURE
        ("EARTH_VERBOSE_BACKPRESSURE", "verbose.backpressure", "earth", false, DeserializeBool, SerializeBool);
}

// Implementation of: EarthModel text decoders
namespace {

    const char* TextOfREQOpcode(uint32_t opcode) noexcept
    {
        switch (opcode)
        {
        case CCHI::Opcodes::REQ::StashShared:       return "StashShared";
        case CCHI::Opcodes::REQ::StashUnique:       return "StashUnique";
        case CCHI::Opcodes::REQ::ReadNoSnp:         return "ReadNoSnp";
        case CCHI::Opcodes::REQ::ReadOnce:          return "ReadOnce";
        case CCHI::Opcodes::REQ::ReadShared:        return "ReadShared";
        case CCHI::Opcodes::REQ::WriteNoSnpPtl:     return "WriteNoSnpPtl";
        case CCHI::Opcodes::REQ::WriteNoSnpFull:    return "WriteNoSnpFull";
        case CCHI::Opcodes::REQ::WriteUniquePtl:    return "WriteUniquePtl";
        case CCHI::Opcodes::REQ::WriteUniqueFull:   return "WriteUniqueFull";
        case CCHI::Opcodes::REQ::CleanShared:       return "CleanShared";
        case CCHI::Opcodes::REQ::CleanInvalid:      return "CleanInvalid";
        case CCHI::Opcodes::REQ::MakeInvalid:       return "MakeInvalid";
        case CCHI::Opcodes::REQ::ReadUnique:        return "ReadUnique";
        case CCHI::Opcodes::REQ::MakeUnique:        return "MakeUnique";
        case CCHI::Opcodes::REQ::EvictBack:         return "EvictBack";
        case CCHI::Opcodes::REQ::EvictClean:        return "EvictClean";
        case CCHI::Opcodes::REQ::AtomicSwap:        return "AtomicSwap";
        case CCHI::Opcodes::REQ::AtomicCompare:     return "AtomicCompare";
        default:
            if (CCHI::Opcodes::REQ::AtomicStore::Is(opcode))
                return "AtomicStore";

            if (CCHI::Opcodes::REQ::AtomicLoad::Is(opcode))
                return "AtomicLoad";

            return "Reserved";
        }
    }

    const char* TextOfEVTOpcode(uint32_t opcode) noexcept
    {
        switch (opcode)
        {
        case CCHI::Opcodes::EVT::Evict:             return "Evict";
        case CCHI::Opcodes::EVT::WriteBackFull:     return "WriteBackFull";
        default:                                    return "Reserved";
        }
    }

    const char* TextOfSNPOpcode(uint32_t opcode) noexcept
    {
        switch (opcode)
        {
        case CCHI::Opcodes::SNP::SnpMakeInvalid:    return "SnpMakeInvalid";
        case CCHI::Opcodes::SNP::SnpToInvalid:      return "SnpToInvalid";
        case CCHI::Opcodes::SNP::SnpToShared:       return "SnpToShared";
        case CCHI::Opcodes::SNP::SnpToClean:        return "SnpToClean";
        default:                                    return "Reserved";
        }
    }

    const char* TextOfUpRSPOpcode(uint32_t opcode) noexcept
    {
        switch (opcode)
        {
        case CCHI::Opcodes::UpRSP::CompAck:         return "CompAck";
        case CCHI::Opcodes::UpRSP::SnpResp:         return "SnpResp";
        default:                                    return "Reserved";
        }
    }

    const char* TextOfUpDATOpcode(uint32_t opcode) noexcept
    {
        switch (opcode)
        {
        case CCHI::Opcodes::UpDAT::NonCopyBackWrData:   return "NonCopyBackWrData";
        case CCHI::Opcodes::UpDAT::CopyBackWrData:      return "CopyBackWrData";
        case CCHI::Opcodes::UpDAT::SnpRespData:         return "SnpRespData";
        default:                                        return "Reserved";
        }
    }

    const char* TextOfDnRSPOpcode(uint32_t opcode) noexcept
    {
        switch (opcode)
        {
        case CCHI::Opcodes::DnRSP::CompStash:       return "CompStash";
        case CCHI::Opcodes::DnRSP::Comp:            return "Comp";
        case CCHI::Opcodes::DnRSP::DBIDResp:        return "DBIDResp";
        case CCHI::Opcodes::DnRSP::CompDBIDResp:    return "CompDBIDResp";
        case CCHI::Opcodes::DnRSP::CompCMO:         return "CompCMO";
        default:                                    return "Reserved";
        }
    }

    const char* TextOfDnDATOpcode(uint32_t opcode) noexcept
    {
        switch (opcode)
        {
        case CCHI::Opcodes::DnDAT::CompData:        return "CompData";
        default:                                    return "Reserved";
        }
    }

    const char* TextOfResp(uint32_t resp) noexcept
    {
        return CCHI::Resps::ToEnum(static_cast<CCHI::Resp>(resp))->name;
    }

    const char* TextOfLineState(size_t state) noexcept
    {
        switch (state)
        {
        case 0:     return "I";
        case 1:     return "S";
        case 2:     return "U";
        default:    return "?";
        }
    }

    const char* TextOfXactKind(size_t kind) noexcept
    {
        switch (kind)
        {
        case 0:     return "ReadShared";
        case 1:     return "ReadUnique";
        case 2:     return "MakeUnique";
        case 3:     return "Evict";
        case 4:     return "WriteBackFull";
        default:    return "?";
        }
    }

    const char* TextOfXactPhase(size_t phase) noexcept
    {
        switch (phase)
        {
        case 0:     return "Snoop";
        case 1:     return "Respond";
        case 2:     return "WaitCompAck";
        case 3:     return "WaitWrData";
        case 4:     return "WaitPop";
        default:    return "?";
        }
    }
}

// Implementation of: class EarthModel
namespace Cohestra {

    EarthModel::EarthModel(size_t portCount) noexcept
        : portCount           (portCount)
        , nodeID              (CONFIG_EARTH_NODE_ID.defaultValue)
        , parallelism         (CONFIG_EARTH_PARALLELISM.defaultValue)
        , inflightSNP         (CONFIG_EARTH_INFLIGHT_SNP.defaultValue)
        , queueDepthEVT       (CONFIG_EARTH_QUEUE_EVT.defaultValue)
        , queueDepthREQ       (CONFIG_EARTH_QUEUE_REQ.defaultValue)
        , queueDepthRSP       (CONFIG_EARTH_QUEUE_RSP.defaultValue)
        , queueDepthDAT       (CONFIG_EARTH_QUEUE_DAT.defaultValue)
        , latencyRSP          (CONFIG_EARTH_LATENCY_RSP.defaultValue)
        , latencyDAT          (CONFIG_EARTH_LATENCY_DAT.defaultValue)
        , memoryStart         (CONFIG_EARTH_MEMORY_START.defaultValue)
        , memoryEnd           (CONFIG_EARTH_MEMORY_END.defaultValue)
        , verbose             (CONFIG_EARTH_VERBOSE.defaultValue)
        , verboseFlit         (CONFIG_EARTH_VERBOSE_FLIT.defaultValue)
        , verboseXact         (CONFIG_EARTH_VERBOSE_XACT.defaultValue)
        , verboseState        (CONFIG_EARTH_VERBOSE_STATE.defaultValue)
        , verboseInternal     (CONFIG_EARTH_VERBOSE_INTERNAL.defaultValue)
        , verboseBackpressure (CONFIG_EARTH_VERBOSE_BACKPRESSURE.defaultValue)
        , upstreamNodeIDs     ()
        , ports               (portCount)
        , tracker             (CONFIG_EARTH_PARALLELISM.defaultValue)
    { }

    bool EarthModel::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        bool good = LoadConfigurationEntry(loader, &CONFIG_EARTH_NODE_ID,      nodeID)       &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_PARALLELISM,  parallelism)  &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_INFLIGHT_SNP, inflightSNP)  &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_QUEUE_EVT,    queueDepthEVT) &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_QUEUE_REQ,    queueDepthREQ) &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_QUEUE_RSP,    queueDepthRSP) &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_QUEUE_DAT,    queueDepthDAT) &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_LATENCY_RSP,  latencyRSP)   &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_LATENCY_DAT,  latencyDAT)   &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_MEMORY_START, memoryStart)  &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_MEMORY_END,   memoryEnd)    &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_VERBOSE,      verbose)      &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_VERBOSE_FLIT, verboseFlit)  &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_VERBOSE_XACT, verboseXact)  &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_VERBOSE_STATE, verboseState) &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_VERBOSE_INTERNAL, verboseInternal) &&
                    LoadConfigurationEntry(loader, &CONFIG_EARTH_VERBOSE_BACKPRESSURE, verboseBackpressure);

        if (!good)
            return false;

        if (nodeID >= (1u << CommonFlitConfigurationType1::downstreamNodeIdWidth))
        {
            logger->error("EarthModel: nodeid ({}) does not fit the downstream node ID width.",
                nodeID);
            return false;
        }

        if (!parallelism)
        {
            logger->error("EarthModel: parallelism must not be zero.");
            return false;
        }

        if (!inflightSNP)
        {
            logger->error("EarthModel: inflight.snp must not be zero.");
            return false;
        }

        if (!queueDepthEVT || !queueDepthREQ || !queueDepthRSP || !queueDepthDAT)
        {
            logger->error("EarthModel: queue depths must not be zero.");
            return false;
        }

        if (memoryEnd <= memoryStart)
        {
            logger->error("EarthModel: memory.end ({}) must be greater than memory.start ({}).",
                memoryEnd, memoryStart);
            return false;
        }

        // the total switch mutes every verbose category
        if (!verbose)
        {
            verboseFlit         = false;
            verboseXact         = false;
            verboseState        = false;
            verboseInternal     = false;
            verboseBackpressure = false;
        }

        tracker.clear();
        tracker.resize(parallelism);

        logger->info(
            "EarthModel: configured with node ID {}, parallelism {}, snoop inflight {}, "
            "queue depths EVT {}/REQ {}/RSP {}/DAT {}, latencies RSP {}/DAT {}, memory [{}, {}).",
            nodeID, parallelism, inflightSNP,
            queueDepthEVT, queueDepthREQ, queueDepthRSP, queueDepthDAT,
            latencyRSP, latencyDAT, memoryStart, memoryEnd);

        return true;
    }

    void EarthModel::SetUpstreamNodeIDs(const std::vector<uint32_t>& nodeIDs) noexcept
    {
        upstreamNodeIDs = nodeIDs;
    }

    void EarthModel::Tick(uint64_t time) noexcept
    {
        this->time = time;

        // CCHI channel priority: EVT > SNP > REQ
        TickEVT();
        TickSNP();
        TickREQ();

        // response emission onto the DnRSP/DnDAT channels
        TickEmit();
    }

    void EarthModel::TickEVT() noexcept
    {
        // highest priority: admit and process pending EVT flits
        for (size_t port = 0; port < portCount; ++port)
        {
            Port& p = ports[port];

            if (p.evtFIFO.empty())
                continue;

            const FlitEVT& flit = p.evtFIFO.front().flit;
            uint64_t     line = flit.Addr >> 6;

            if (flit.Opcode != CCHI::Opcodes::EVT::Evict
             && flit.Opcode != CCHI::Opcodes::EVT::WriteBackFull)
            {
                ReportViolation("EVT with unsupported opcode, dropped", port);
                p.evtFIFO.pop_front();
                continue;
            }

            // one active eviction per line; an eviction may cut ahead of an
            // active REQ transaction on the same line (see class notes)
            if (lineEVT.find(line) != lineEVT.end())
                continue;

            std::optional<size_t> slot = AllocateSlot();

            if (!slot)
                continue;

            CheckAddressWindow(flit.Addr, "EVT", port);

            int dbid = -1;

            if (flit.Opcode == CCHI::Opcodes::EVT::WriteBackFull)
            {
                std::optional<int> id = AllocateID(port);

                if (!id)
                    continue;

                dbid = *id;
            }

            p.evtFIFO.pop_front();

            Xaction& x  = tracker[*slot];
            x           = Xaction{};
            x.active    = true;
            x.kind      = flit.Opcode == CCHI::Opcodes::EVT::Evict ? XactKind::Evict
                                                                   : XactKind::WriteBackFull;
            x.port      = port;
            x.line      = line;
            x.addr      = flit.Addr;
            x.txnID     = static_cast<uint8_t>(flit.TxnID);
            x.srcNodeID = static_cast<uint32_t>(flit.SrcID);
            x.traceTag  = static_cast<uint8_t>(flit.TraceTag);
            x.phase     = XactPhase::Respond;
            x.readyTime = time + latencyRSP;
            x.dbid      = dbid;

            lineEVT[line] = *slot;

            if (verboseXact)
                logger->debug("EarthModel: EVT admitted on slot {} (opcode {}, line {:#x}, port {}, txnID {}) at time {}.",
                    *slot, TextOfEVTOpcode(static_cast<uint32_t>(flit.Opcode)), line << 6, port,
                    static_cast<uint32_t>(flit.TxnID), time);

            if (x.kind == XactKind::Evict)
            {
                // clean eviction: the directory entry is dropped immediately
                DirRemove(port, line);
            }
            else
            {
                // write-back: the directory entry is dropped on data completion
                p.dbids[static_cast<uint8_t>(dbid)] = { *slot, DBIDKind::WriteBackData };
            }

            countEVT++;
        }
    }

    void EarthModel::TickSNP() noexcept
    {
        // middle priority: snoop channel progress, which covers consuming
        // snoop responses, write-back data and CompAcks, and issuing snoops

        // consume UpRSP flits, one per port per cycle
        for (size_t port = 0; port < portCount; ++port)
        {
            Port& p = ports[port];

            if (p.uprspFIFO.empty())
                continue;

            FlitUpRSP flit = p.uprspFIFO.front();
            p.uprspFIFO.pop_front();

            if (flit.Opcode == CCHI::Opcodes::UpRSP::SnpResp)
            {
                auto it = p.snoops.find(static_cast<uint8_t>(flit.TxnID));

                if (it == p.snoops.end())
                {
                    ReportViolation("SnpResp with unknown TxnID, dropped", port);
                    continue;
                }

                SnoopEntry& se = it->second;

                if (se.gotData)
                {
                    ReportViolation("SnpResp received after SnpRespData on the same TxnID", port);
                    p.snoops.erase(it);
                    FreeID(port, static_cast<uint8_t>(flit.TxnID));
                    continue;
                }

                se.gotResp = true;

                size_t slot = se.slot;

                Snoop& snoop    = tracker[slot].snoops[se.snoop];
                snoop.done      = true;
                snoop.respNotI  = flit.Resp != CCHI::Resps::I;

                if (verboseXact)
                    logger->debug("EarthModel: SnpResp completed (resp {}, port {}, txnID {}, for slot {}) at time {}.",
                        TextOfResp(static_cast<uint32_t>(flit.Resp)), port, static_cast<uint32_t>(flit.TxnID), slot, time);

                p.snoops.erase(it);
                FreeID(port, static_cast<uint8_t>(flit.TxnID));

                CheckSnoopsDone(slot);
            }
            else if (flit.Opcode == CCHI::Opcodes::UpRSP::CompAck)
            {
                auto it = p.dbids.find(static_cast<uint8_t>(flit.TxnID));

                if (it == p.dbids.end())
                {
                    ReportViolation("CompAck with unknown DBID, dropped", port);
                    continue;
                }

                DBIDEntry& de = it->second;

                if (de.kind != DBIDKind::CompAckWait)
                {
                    ReportViolation("CompAck colliding with a write-back DBID", port);
                    continue;
                }

                Xaction& x = tracker[de.slot];

                if (x.ackReceived)
                    ReportViolation("duplicated CompAck on the same DBID", port);

                x.ackReceived = true;

                if (verboseXact)
                    logger->debug("EarthModel: CompAck received (port {}, DBID {}, for slot {} in phase {}) at time {}.",
                        port, static_cast<uint32_t>(flit.TxnID), de.slot, TextOfXactPhase(static_cast<size_t>(x.phase)), time);

                if (x.phase == XactPhase::WaitCompAck)
                    Retire(de.slot);
            }
            else
                ReportViolation("UpRSP with unsupported opcode, dropped", port);
        }

        // consume UpDAT flits, one per port per cycle
        for (size_t port = 0; port < portCount; ++port)
        {
            Port& p = ports[port];

            if (p.updatFIFO.empty())
                continue;

            FlitUpDAT flit = p.updatFIFO.front();
            p.updatFIFO.pop_front();

            size_t dataID = static_cast<size_t>(flit.DataID);

            if (flit.Opcode == CCHI::Opcodes::UpDAT::SnpRespData)
            {
                auto it = p.snoops.find(static_cast<uint8_t>(flit.TxnID));

                if (it == p.snoops.end())
                {
                    ReportViolation("SnpRespData with unknown TxnID, dropped", port);
                    continue;
                }

                SnoopEntry& se = it->second;

                if (se.gotResp)
                {
                    ReportViolation("SnpRespData received after SnpResp on the same TxnID", port);
                    continue;
                }

                if (se.beatMask & (1u << dataID))
                {
                    ReportViolation("SnpRespData with duplicated DataID, dropped", port);
                    continue;
                }

                if (se.beatMask && se.resp != static_cast<uint8_t>(flit.Resp))
                    ReportViolation("SnpRespData with mismatched Resp across beats", port);

                se.gotData   = true;
                se.resp      = static_cast<uint8_t>(flit.Resp);
                se.beatMask |= (1u << dataID);

                if (CCHI::Resps::ToEnum(static_cast<CCHI::Resp>(flit.Resp))->IsPD())
                    MergeBeat(se.line, dataID, flit.Data, static_cast<uint32_t>(flit.BE));

                if (se.beatMask == ((1u << BEATS_PER_LINE) - 1))
                {
                    size_t slot = se.slot;

                    Snoop& snoop    = tracker[slot].snoops[se.snoop];
                    snoop.done      = true;
                    snoop.respNotI  = se.resp != CCHI::Resps::I;

                    if (verboseXact)
                        logger->debug("EarthModel: SnpRespData completed (resp {}, port {}, txnID {}, for slot {}) at time {}.",
                            TextOfResp(se.resp), port, static_cast<uint32_t>(flit.TxnID), slot, time);

                    p.snoops.erase(it);
                    FreeID(port, static_cast<uint8_t>(flit.TxnID));

                    CheckSnoopsDone(slot);
                }
            }
            else if (flit.Opcode == CCHI::Opcodes::UpDAT::CopyBackWrData)
            {
                auto it = p.dbids.find(static_cast<uint8_t>(flit.TxnID));

                if (it == p.dbids.end())
                {
                    ReportViolation("CopyBackWrData with unknown DBID, dropped", port);
                    continue;
                }

                DBIDEntry& de = it->second;

                if (de.kind != DBIDKind::WriteBackData)
                {
                    ReportViolation("CopyBackWrData colliding with a CompAck DBID", port);
                    continue;
                }

                Xaction& x = tracker[de.slot];

                if (x.wrBeatMask & (1u << dataID))
                {
                    ReportViolation("CopyBackWrData with duplicated DataID, dropped", port);
                    continue;
                }

                x.wrBeatMask |= (1u << dataID);

                MergeBeat(x.line, dataID, flit.Data, static_cast<uint32_t>(flit.BE));

                if (verboseState)
                    logger->debug("EarthModel: CopyBackWrData beat merged (dataID {}, BE {:#x}, port {}, DBID {}, for slot {}) at time {}.",
                        dataID, static_cast<uint32_t>(flit.BE), port, static_cast<uint32_t>(flit.TxnID), de.slot, time);

                if (x.wrBeatMask == ((1u << BEATS_PER_LINE) - 1))
                {
                    // write-back data completed: memory is up to date and the
                    // directory ownership of the line is dropped
                    DirRemove(x.port, x.line);

                    Retire(de.slot);
                }
            }
            else
                ReportViolation("UpDAT with unsupported opcode, dropped", port);
        }

        // issue pending snoops, at most one per target port per cycle
        std::vector<bool> snpIssued(portCount, false);

        for (size_t slotIdx = 0; slotIdx < tracker.size(); ++slotIdx)
        {
            Xaction& x = tracker[slotIdx];

            if (!x.active || x.phase != XactPhase::Snoop)
                continue;

            if (!x.snoopPlanBuilt)
                BuildSnoopPlan(slotIdx);

            if (x.phase != XactPhase::Snoop)
                continue;

            for (size_t si = 0; si < x.snoops.size(); ++si)
            {
                Snoop& snoop = x.snoops[si];

                if (snoop.issued)
                    continue;

                Port& tp = ports[snoop.port];

                if (tp.snoops.size() >= inflightSNP)
                    continue;

                if (snpIssued[snoop.port])
                    continue;

                std::optional<int> id = AllocateID(snoop.port);

                if (!id)
                    continue;

                snoop.txnID  = *id;
                snoop.issued = true;

                uint32_t tgtNodeID = snoop.port < upstreamNodeIDs.size()
                                   ? upstreamNodeIDs[snoop.port] : static_cast<uint32_t>(snoop.port);

                FlitSNP flit;
                flit.TxnID    = static_cast<FlitSNP::txnid_t>(*id);
                flit.SrcID    = static_cast<FlitSNP::srcid_t>(nodeID);
                flit.TgtID    = static_cast<FlitSNP::tgtid_t>(tgtNodeID);
                flit.Opcode   = static_cast<FlitSNP::opcode_t>(snoop.opcode);
                flit.Addr     = static_cast<FlitSNP::addr_t>(x.line << 3);
                flit.NS       = 0;
                flit.TraceTag = 0;

                tp.snoops[static_cast<uint8_t>(*id)] = { slotIdx, si, x.line, 0, false, false, 0 };
                tp.snpQueue.push_back(flit);

                snpIssued[snoop.port] = true;

                if (verboseFlit)
                    logger->debug("EarthModel: SNP issued (opcode {}, line {:#x}, port {}, txnID {}, for slot {}) at time {}.",
                        TextOfSNPOpcode(static_cast<uint32_t>(snoop.opcode)), x.line << 6, snoop.port, *id, slotIdx, time);

                countSNP++;
            }
        }
    }

    void EarthModel::TickREQ() noexcept
    {
        // lowest priority: admit pending REQ flits
        for (size_t port = 0; port < portCount; ++port)
        {
            Port& p = ports[port];

            if (p.reqFIFO.empty())
                continue;

            StampedFlit<FlitREQ>&   head = p.reqFIFO.front();
            const FlitREQ&          flit = head.flit;
            uint64_t                line = flit.Addr >> 6;

            if (flit.Opcode != CCHI::Opcodes::REQ::ReadShared
             && flit.Opcode != CCHI::Opcodes::REQ::ReadUnique
             && flit.Opcode != CCHI::Opcodes::REQ::MakeUnique)
            {
                ReportViolation("REQ with unsupported opcode, dropped", port);
                p.reqFIFO.pop_front();
                continue;
            }

            // same-line requests are serialized in arrival order
            bool lineBusy = lineREQ.find(line) != lineREQ.end()
                         || lineEVT.find(line) != lineEVT.end();

            auto wit = lineWaiters.find(line);

            bool waitersPending = wit != lineWaiters.end() && !wit->second.empty();
            bool headWaiter     = waitersPending
                               && wit->second.front().first == head.stamp
                               && wit->second.front().second == port;

            if (lineBusy || (waitersPending && !headWaiter))
            {
                if (!head.registered)
                {
                    lineWaiters[line].push_back({ head.stamp, port });
                    head.registered = true;

                    if (verboseInternal)
                        logger->debug("EarthModel: REQ waiting registered (opcode {}, line {:#x}, port {}, stamp {}) at time {}.",
                            TextOfREQOpcode(static_cast<uint32_t>(flit.Opcode)), line << 6, port, head.stamp, time);
                }

                continue;
            }

            std::optional<size_t> slot = AllocateSlot();

            if (!slot)
                continue;

            CheckAddressWindow(flit.Addr, "REQ", port);

            p.reqFIFO.pop_front();

            if (headWaiter)
            {
                wit->second.pop_front();

                if (wit->second.empty())
                    lineWaiters.erase(wit);
            }

            Xaction& x    = tracker[*slot];
            x             = Xaction{};
            x.active      = true;
            x.kind        = flit.Opcode == CCHI::Opcodes::REQ::ReadShared ? XactKind::ReadShared
                          : flit.Opcode == CCHI::Opcodes::REQ::ReadUnique ? XactKind::ReadUnique
                                                                          : XactKind::MakeUnique;
            x.port        = port;
            x.line        = line;
            x.addr        = flit.Addr;
            x.txnID       = static_cast<uint8_t>(flit.TxnID);
            x.srcNodeID   = static_cast<uint32_t>(flit.SrcID);
            x.expCompData = static_cast<uint8_t>(flit.ExpCompData) != 0;
            x.traceTag    = static_cast<uint8_t>(flit.TraceTag);
            x.phase       = XactPhase::Snoop;

            lineREQ[line] = *slot;

            if (verboseXact)
                logger->debug("EarthModel: REQ admitted on slot {} (opcode {}, line {:#x}, port {}, txnID {}, ExpCompData {}) at time {}.",
                    *slot, TextOfREQOpcode(static_cast<uint32_t>(flit.Opcode)), line << 6, port,
                    static_cast<uint32_t>(flit.TxnID), static_cast<uint32_t>(flit.ExpCompData), time);

            countREQ++;
        }
    }

    void EarthModel::TickEmit() noexcept
    {
        // emit scheduled responses, at most one DnRSP and one DnDAT per port
        // per cycle (channel width)
        for (size_t port = 0; port < portCount; ++port)
        {
            Port& p = ports[port];

            bool emittedRSP = false;
            bool emittedDAT = false;

            for (size_t slotIdx = 0; slotIdx < tracker.size() && !(emittedRSP && emittedDAT); ++slotIdx)
            {
                Xaction& x = tracker[slotIdx];

                if (!x.active || x.port != port
                 || x.phase != XactPhase::Respond
                 || x.readyTime > time)
                    continue;

                switch (x.kind)
                {
                case XactKind::ReadShared:
                case XactKind::ReadUnique:

                    if (x.kind == XactKind::ReadShared || x.expCompData)
                    {
                        // CompData beats, both carrying the same allocated DBID
                        if (emittedDAT)
                            continue;

                        if (x.dbid < 0)
                        {
                            std::optional<int> id = AllocateID(port);

                            if (!id)
                                continue;

                            x.dbid = *id;
                            p.dbids[static_cast<uint8_t>(*id)] = { slotIdx, DBIDKind::CompAckWait };
                        }

                        const std::array<uint64_t, LINE_WORDS>& lineData = memory[x.line];

                        FlitDnDAT flit;
                        flit.TxnID      = static_cast<FlitDnDAT::txnid_t>(x.txnID);
                        flit.SrcID      = static_cast<FlitDnDAT::srcid_t>(nodeID);
                        flit.TgtID      = static_cast<FlitDnDAT::tgtid_t>(x.srcNodeID);
                        flit.DBID       = static_cast<FlitDnDAT::dbid_t>(x.dbid);
                        flit.Opcode     = CCHI::Opcodes::DnDAT::CompData;
                        flit.RespErr    = 0;
                        flit.Resp       = x.kind == XactKind::ReadShared ? CCHI::Resps::SC
                                                                         : CCHI::Resps::UC;
                        flit.DataSource = 0;
                        flit.CBusy      = 0;
                        flit.WayValid   = 0;
                        flit.Way        = 0;
                        flit.DataID     = static_cast<FlitDnDAT::dataid_t>(x.compDataBeats);

                        for (size_t w = 0; w < WORDS_PER_BEAT; ++w)
                            flit.Data[w] = lineData[x.compDataBeats * WORDS_PER_BEAT + w];

                        flit.TraceTag   = static_cast<FlitDnDAT::tracetag_t>(x.traceTag);

                        p.dndatQueue.push_back(flit);

                        if (verboseFlit)
                            logger->debug("EarthModel: {} emitted (dataID {}, resp {}, port {}, txnID {}, DBID {}, for slot {}) at time {}.",
                                TextOfDnDATOpcode(static_cast<uint32_t>(flit.Opcode)),
                                x.compDataBeats, TextOfResp(static_cast<uint32_t>(flit.Resp)), port, x.txnID, x.dbid, slotIdx, time);

                        x.compDataBeats++;
                        emittedDAT = true;

                        // every following CompData beat is scheduled on its own,
                        // so the upstream CompAck may legitimately arrive before
                        // or after any of them
                        if (x.compDataBeats == BEATS_PER_LINE)
                        {
                            DirGrant(slotIdx);

                            if (x.ackReceived)
                                Retire(slotIdx);
                            else
                                x.phase = XactPhase::WaitCompAck;
                        }
                        else
                            x.readyTime = time + latencyDAT;
                    }
                    else
                    {
                        // ReadUnique with data already held upstream: Comp only
                        if (emittedRSP)
                            continue;

                        if (x.dbid < 0)
                        {
                            std::optional<int> id = AllocateID(port);

                            if (!id)
                                continue;

                            x.dbid = *id;
                            p.dbids[static_cast<uint8_t>(*id)] = { slotIdx, DBIDKind::CompAckWait };
                        }

                        FlitDnRSP flit;
                        flit.TxnID    = static_cast<FlitDnRSP::txnid_t>(x.txnID);
                        flit.SrcID    = static_cast<FlitDnRSP::srcid_t>(nodeID);
                        flit.TgtID    = static_cast<FlitDnRSP::tgtid_t>(x.srcNodeID);
                        flit.DBID     = static_cast<FlitDnRSP::dbid_t>(x.dbid);
                        flit.Opcode   = CCHI::Opcodes::DnRSP::Comp;
                        flit.RespErr  = 0;
                        flit.Resp     = CCHI::Resps::UC;
                        flit.CBusy    = 0;
                        flit.WayValid = 0;
                        flit.Way      = 0;
                        flit.TraceTag = static_cast<FlitDnRSP::tracetag_t>(x.traceTag);

                        p.dnrspQueue.push_back(flit);

                        if (verboseFlit)
                            logger->debug("EarthModel: {} emitted ({}, port {}, txnID {}, DBID {}, for slot {}) at time {}.",
                                TextOfDnRSPOpcode(static_cast<uint32_t>(flit.Opcode)),
                                TextOfXactKind(static_cast<size_t>(x.kind)), port, x.txnID, x.dbid, slotIdx, time);

                        DirGrant(slotIdx);

                        emittedRSP = true;

                        if (x.ackReceived)
                            Retire(slotIdx);
                        else
                            x.phase = XactPhase::WaitCompAck;
                    }

                    break;

                case XactKind::MakeUnique:

                    if (emittedRSP)
                        continue;

                    if (x.dbid < 0)
                    {
                        std::optional<int> id = AllocateID(port);

                        if (!id)
                            continue;

                        x.dbid = *id;
                        p.dbids[static_cast<uint8_t>(*id)] = { slotIdx, DBIDKind::CompAckWait };
                    }

                    {
                        FlitDnRSP flit;
                        flit.TxnID    = static_cast<FlitDnRSP::txnid_t>(x.txnID);
                        flit.SrcID    = static_cast<FlitDnRSP::srcid_t>(nodeID);
                        flit.TgtID    = static_cast<FlitDnRSP::tgtid_t>(x.srcNodeID);
                        flit.DBID     = static_cast<FlitDnRSP::dbid_t>(x.dbid);
                        flit.Opcode   = CCHI::Opcodes::DnRSP::Comp;
                        flit.RespErr  = 0;
                        flit.Resp     = CCHI::Resps::UC;
                        flit.CBusy    = 0;
                        flit.WayValid = 0;
                        flit.Way      = 0;
                        flit.TraceTag = static_cast<FlitDnRSP::tracetag_t>(x.traceTag);

                        p.dnrspQueue.push_back(flit);
                    }

                    if (verboseFlit)
                        logger->debug("EarthModel: {} emitted ({}, port {}, txnID {}, DBID {}, for slot {}) at time {}.",
                            TextOfDnRSPOpcode(CCHI::Opcodes::DnRSP::Comp),
                            TextOfXactKind(static_cast<size_t>(x.kind)), port, x.txnID, x.dbid, slotIdx, time);

                    DirGrant(slotIdx);

                    emittedRSP = true;

                    if (x.ackReceived)
                        Retire(slotIdx);
                    else
                        x.phase = XactPhase::WaitCompAck;

                    break;

                case XactKind::Evict:

                    if (emittedRSP)
                        continue;

                    {
                        FlitDnRSP flit;
                        flit.TxnID    = static_cast<FlitDnRSP::txnid_t>(x.txnID);
                        flit.SrcID    = static_cast<FlitDnRSP::srcid_t>(nodeID);
                        flit.TgtID    = static_cast<FlitDnRSP::tgtid_t>(x.srcNodeID);
                        flit.DBID     = 0;
                        flit.Opcode   = CCHI::Opcodes::DnRSP::Comp;
                        flit.RespErr  = 0;
                        flit.Resp     = 0;
                        flit.CBusy    = 0;
                        flit.WayValid = 0;
                        flit.Way      = 0;
                        flit.TraceTag = static_cast<FlitDnRSP::tracetag_t>(x.traceTag);

                        p.dnrspQueue.push_back(flit);
                    }

                    if (verboseFlit)
                        logger->debug("EarthModel: {} emitted ({}, port {}, txnID {}, for slot {}) at time {}.",
                            TextOfDnRSPOpcode(CCHI::Opcodes::DnRSP::Comp),
                            TextOfXactKind(static_cast<size_t>(x.kind)), port, x.txnID, slotIdx, time);

                    emittedRSP = true;

                    // an Evict retires once its Comp is delivered downstream
                    x.phase = XactPhase::WaitPop;

                    break;

                case XactKind::WriteBackFull:

                    if (emittedRSP)
                        continue;

                    {
                        FlitDnRSP flit;
                        flit.TxnID    = static_cast<FlitDnRSP::txnid_t>(x.txnID);
                        flit.SrcID    = static_cast<FlitDnRSP::srcid_t>(nodeID);
                        flit.TgtID    = static_cast<FlitDnRSP::tgtid_t>(x.srcNodeID);
                        flit.DBID     = static_cast<FlitDnRSP::dbid_t>(x.dbid);
                        flit.Opcode   = CCHI::Opcodes::DnRSP::CompDBIDResp;
                        flit.RespErr  = 0;
                        flit.Resp     = 0;
                        flit.CBusy    = 0;
                        flit.WayValid = 0;
                        flit.Way      = 0;
                        flit.TraceTag = static_cast<FlitDnRSP::tracetag_t>(x.traceTag);

                        p.dnrspQueue.push_back(flit);
                    }

                    if (verboseFlit)
                        logger->debug("EarthModel: {} emitted ({}, port {}, txnID {}, DBID {}, for slot {}) at time {}.",
                            TextOfDnRSPOpcode(CCHI::Opcodes::DnRSP::CompDBIDResp),
                            TextOfXactKind(static_cast<size_t>(x.kind)), port, x.txnID, x.dbid, slotIdx, time);

                    emittedRSP = true;

                    x.phase = XactPhase::WaitWrData;

                    break;
                }
            }
        }
    }

    bool EarthModel::PushEVT(size_t port, const FlitEVT& flit) noexcept
    {
        if (port >= portCount)
        {
            ReportViolation("EVT pushed to a non-existent port, dropped", port);
            return false;
        }

        Port& p = ports[port];

        // concurrent upstream transactions per port are supported: Taurus nodes
        // drive unique TxnIDs, and responses are told apart by the DBIDs and
        // snoop TxnIDs this model allocates per transaction
        if (p.evtFIFO.size() >= queueDepthEVT)
        {
            ReportBackpressure("EVT denied: channel queue full", port);
            return false;
        }

        p.evtFIFO.push_back({ flit, stampCounter++, false });

        if (verboseFlit)
            logger->debug("EarthModel: EVT accepted (opcode {}, addr {:#x}, port {}, txnID {}) at time {}.",
                TextOfEVTOpcode(static_cast<uint32_t>(flit.Opcode)), static_cast<uint64_t>(flit.Addr), port,
                static_cast<uint32_t>(flit.TxnID), time);

        return true;
    }

    bool EarthModel::PushREQ(size_t port, const FlitREQ& flit) noexcept
    {
        if (port >= portCount)
        {
            ReportViolation("REQ pushed to a non-existent port, dropped", port);
            return false;
        }

        Port& p = ports[port];

        // concurrent upstream transactions per port are supported: Taurus nodes
        // drive unique TxnIDs, and responses are told apart by the DBIDs and
        // snoop TxnIDs this model allocates per transaction
        if (p.reqFIFO.size() >= queueDepthREQ)
        {
            ReportBackpressure("REQ denied: channel queue full", port);
            return false;
        }

        p.reqFIFO.push_back({ flit, stampCounter++, false });

        if (verboseFlit)
            logger->debug("EarthModel: REQ accepted (opcode {}, addr {:#x}, port {}, txnID {}, ExpCompData {}) at time {}.",
                TextOfREQOpcode(static_cast<uint32_t>(flit.Opcode)), static_cast<uint64_t>(flit.Addr), port,
                static_cast<uint32_t>(flit.TxnID), static_cast<uint32_t>(flit.ExpCompData), time);

        return true;
    }

    bool EarthModel::PushUpRSP(size_t port, const FlitUpRSP& flit) noexcept
    {
        if (port >= portCount)
        {
            ReportViolation("UpRSP pushed to a non-existent port, dropped", port);
            return false;
        }

        Port& p = ports[port];

        if (p.uprspFIFO.size() >= queueDepthRSP)
        {
            ReportBackpressure("UpRSP denied: channel queue full", port);
            return false;
        }

        p.uprspFIFO.push_back(flit);

        if (verboseFlit)
            logger->debug("EarthModel: UpRSP accepted (opcode {}, resp {}, port {}, txnID {}) at time {}.",
                TextOfUpRSPOpcode(static_cast<uint32_t>(flit.Opcode)), TextOfResp(static_cast<uint32_t>(flit.Resp)), port,
                static_cast<uint32_t>(flit.TxnID), time);

        return true;
    }

    bool EarthModel::PushUpDAT(size_t port, const FlitUpDAT& flit) noexcept
    {
        if (port >= portCount)
        {
            ReportViolation("UpDAT pushed to a non-existent port, dropped", port);
            return false;
        }

        Port& p = ports[port];

        if (p.updatFIFO.size() >= queueDepthDAT)
        {
            ReportBackpressure("UpDAT denied: channel queue full", port);
            return false;
        }

        p.updatFIFO.push_back(flit);

        if (verboseFlit)
            logger->debug("EarthModel: UpDAT accepted (opcode {}, resp {}, dataID {}, port {}, txnID {}) at time {}.",
                TextOfUpDATOpcode(static_cast<uint32_t>(flit.Opcode)), TextOfResp(static_cast<uint32_t>(flit.Resp)),
                static_cast<uint32_t>(flit.DataID), port, static_cast<uint32_t>(flit.TxnID), time);

        return true;
    }

    bool EarthModel::HasSNP(size_t port) const noexcept
    {
        return port < portCount && !ports[port].snpQueue.empty();
    }

    std::optional<EarthModel::FlitSNP> EarthModel::PeekSNP(size_t port) const noexcept
    {
        if (!HasSNP(port))
            return std::nullopt;

        return { ports[port].snpQueue.front() };
    }

    std::optional<EarthModel::FlitSNP> EarthModel::PopSNP(size_t port) noexcept
    {
        if (!HasSNP(port))
            return std::nullopt;

        FlitSNP flit = ports[port].snpQueue.front();
        ports[port].snpQueue.pop_front();

        return { flit };
    }

    bool EarthModel::HasDnRSP(size_t port) const noexcept
    {
        return port < portCount && !ports[port].dnrspQueue.empty();
    }

    std::optional<EarthModel::FlitDnRSP> EarthModel::PeekDnRSP(size_t port) const noexcept
    {
        if (!HasDnRSP(port))
            return std::nullopt;

        return { ports[port].dnrspQueue.front() };
    }

    std::optional<EarthModel::FlitDnRSP> EarthModel::PopDnRSP(size_t port) noexcept
    {
        if (!HasDnRSP(port))
            return std::nullopt;

        Port& p = ports[port];

        FlitDnRSP flit = p.dnrspQueue.front();
        p.dnrspQueue.pop_front();

        // an Evict transaction retires once its Comp is delivered downstream
        if (flit.Opcode == CCHI::Opcodes::DnRSP::Comp)
        {
            for (size_t slotIdx = 0; slotIdx < tracker.size(); ++slotIdx)
            {
                Xaction& x = tracker[slotIdx];

                if (x.active && x.phase == XactPhase::WaitPop
                 && x.port == port && x.txnID == static_cast<uint8_t>(flit.TxnID))
                {
                    Retire(slotIdx);
                    break;
                }
            }
        }

        return { flit };
    }

    bool EarthModel::HasDnDAT(size_t port) const noexcept
    {
        return port < portCount && !ports[port].dndatQueue.empty();
    }

    std::optional<EarthModel::FlitDnDAT> EarthModel::PeekDnDAT(size_t port) const noexcept
    {
        if (!HasDnDAT(port))
            return std::nullopt;

        return { ports[port].dndatQueue.front() };
    }

    std::optional<EarthModel::FlitDnDAT> EarthModel::PopDnDAT(size_t port) noexcept
    {
        if (!HasDnDAT(port))
            return std::nullopt;

        FlitDnDAT flit = ports[port].dndatQueue.front();
        ports[port].dndatQueue.pop_front();

        return { flit };
    }

    size_t EarthModel::GetPortCount() const noexcept
    {
        return portCount;
    }

    uint32_t EarthModel::GetNodeID() const noexcept
    {
        return nodeID;
    }

    bool EarthModel::IsIdle() const noexcept
    {
        for (const Port& p : ports)
        {
            if (!p.evtFIFO.empty()   || !p.reqFIFO.empty()
             || !p.uprspFIFO.empty() || !p.updatFIFO.empty()
             || !p.snpQueue.empty()  || !p.dnrspQueue.empty() || !p.dndatQueue.empty()
             || !p.snoops.empty()    || !p.dbids.empty())
                return false;
        }

        for (const Xaction& x : tracker)
        {
            if (x.active)
                return false;
        }

        return true;
    }

    uint64_t EarthModel::GetBackpressureDenialCount() const noexcept
    {
        return countDeniedBackpressure;
    }

    uint64_t EarthModel::GetProtocolDenialCount() const noexcept
    {
        return countDeniedProtocol;
    }

    uint64_t EarthModel::GetServicedREQCount() const noexcept
    {
        return countREQ;
    }

    uint64_t EarthModel::GetServicedEVTCount() const noexcept
    {
        return countEVT;
    }

    uint64_t EarthModel::GetServicedSNPCount() const noexcept
    {
        return countSNP;
    }

    std::shared_ptr<spdlog::logger> EarthModel::GetLogger() const noexcept
    {
        return logger;
    }

    void EarthModel::SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept
    {
        this->logger = std::move(logger);
    }

    std::optional<int> EarthModel::AllocateID(size_t port) noexcept
    {
        Port& p = ports[port];

        for (size_t i = 0; i < TXNID_SPACE; ++i)
        {
            if (!p.idUsed[i])
            {
                p.idUsed[i] = true;

                if (verboseInternal)
                    logger->debug("EarthModel: ID {} allocated on port {} at time {}.", i, port, time);

                return { static_cast<int>(i) };
            }
        }

        if (verboseInternal)
            logger->debug("EarthModel: no free ID on port {} at time {}.", port, time);

        return std::nullopt;
    }

    void EarthModel::FreeID(size_t port, int id) noexcept
    {
        ports[port].idUsed[static_cast<size_t>(id)] = false;

        if (verboseInternal)
            logger->debug("EarthModel: ID {} freed on port {} at time {}.", id, port, time);
    }

    std::optional<size_t> EarthModel::AllocateSlot() noexcept
    {
        for (size_t i = 0; i < tracker.size(); ++i)
        {
            if (!tracker[i].active)
                return { i };
        }

        return std::nullopt;
    }

    void EarthModel::BuildSnoopPlan(size_t slot) noexcept
    {
        Xaction& x = tracker[slot];

        x.snoopPlanBuilt = true;
        x.snoops.clear();

        DirectoryEntry dir = DirLookup(x.line);

        switch (x.kind)
        {
        case XactKind::ReadShared:

            // a unique owner is downgraded to shared, returning dirty data
            if (dir.state == LineState::U && dir.owner != x.port)
                x.snoops.push_back({ dir.owner, CCHI::Opcodes::SNP::SnpToShared, -1, false, false, false });

            break;

        case XactKind::ReadUnique:
        case XactKind::MakeUnique:

            // all other holders are invalidated; a unique owner may hold dirty
            // data and is snooped with a data-returning opcode
            if (dir.state == LineState::U && dir.owner != x.port)
            {
                x.snoops.push_back({ dir.owner, CCHI::Opcodes::SNP::SnpToInvalid, -1, false, false, false });
            }
            else if (dir.state == LineState::S)
            {
                for (size_t p = 0; p < portCount; ++p)
                {
                    if (p != x.port && (dir.sharers & (uint32_t(1) << p)))
                        x.snoops.push_back({ p, CCHI::Opcodes::SNP::SnpMakeInvalid, -1, false, false, false });
                }
            }

            break;

        default:
            break;
        }

        if (x.snoops.empty())
        {
            x.phase     = XactPhase::Respond;
            x.readyTime = time + (x.kind == XactKind::ReadShared || x.expCompData ? latencyDAT
                                                                                 : latencyRSP);
        }

        if (verboseInternal)
            logger->debug("EarthModel: snoop plan for slot {} (kind {}, line {:#x}, port {}): {} target(s){}, directory state {} sharers {:#x} owner {}.",
                slot, TextOfXactKind(static_cast<size_t>(x.kind)), x.line << 6, x.port, x.snoops.size(),
                [&] {
                    std::string targets;
                    for (const Snoop& snoop : x.snoops)
                    {
                        targets += " (port " + std::to_string(snoop.port)
                                 + ", opcode " + TextOfSNPOpcode(static_cast<uint32_t>(snoop.opcode)) + ")";
                    }
                    return targets;
                }(),
                TextOfLineState(static_cast<size_t>(dir.state)), dir.sharers, dir.owner);
    }

    bool EarthModel::CheckSnoopsDone(size_t slot) noexcept
    {
        Xaction& x = tracker[slot];

        if (!x.active || x.phase != XactPhase::Snoop)
            return false;

        for (const Snoop& snoop : x.snoops)
        {
            if (!snoop.done)
                return false;
        }

        x.phase     = XactPhase::Respond;
        x.readyTime = time + (x.kind == XactKind::ReadShared || x.expCompData ? latencyDAT
                                                                             : latencyRSP);

        return true;
    }

    void EarthModel::Retire(size_t slot) noexcept
    {
        Xaction& x = tracker[slot];

        if (verboseXact)
            logger->debug("EarthModel: slot {} retired (kind {}, line {:#x}, port {}) at time {}.",
                slot, TextOfXactKind(static_cast<size_t>(x.kind)), x.line << 6, x.port, time);

        if (x.dbid >= 0)
        {
            ports[x.port].dbids.erase(static_cast<uint8_t>(x.dbid));
            FreeID(x.port, x.dbid);
            x.dbid = -1;
        }

        if (x.kind == XactKind::Evict || x.kind == XactKind::WriteBackFull)
            lineEVT.erase(x.line);
        else
            lineREQ.erase(x.line);

        x.active = false;
    }

    EarthModel::DirectoryEntry EarthModel::DirLookup(uint64_t line) const noexcept
    {
        auto it = directory.find(line);

        if (it == directory.end())
            return {};

        return it->second;
    }

    void EarthModel::DirRemove(size_t port, uint64_t line) noexcept
    {
        auto it = directory.find(line);

        if (it == directory.end())
        {
            if (verboseState)
                logger->debug("EarthModel: directory drop of line {:#x} for port {} ignored, entry absent.",
                    line << 6, port);
            return;
        }

        DirectoryEntry& dir = it->second;

        if (dir.state == LineState::U && dir.owner == port)
        {
            if (verboseState)
                logger->debug("EarthModel: directory line {:#x}: U(owner {}) -> I at time {}.",
                    line << 6, dir.owner, time);

            directory.erase(it);
        }
        else if (dir.state == LineState::S)
        {
            dir.sharers &= ~(uint32_t(1) << port);

            if (verboseState)
                logger->debug("EarthModel: directory line {:#x}: sharer {} dropped, sharers now {:#x} at time {}.",
                    line << 6, port, dir.sharers, time);

            if (!dir.sharers)
                directory.erase(it);
        }
        else if (verboseState)
            logger->debug("EarthModel: directory drop of line {:#x} for port {} ignored, state {} owner {} sharers {:#x}.",
                line << 6, port, TextOfLineState(static_cast<size_t>(dir.state)), dir.owner, dir.sharers);
    }

    void EarthModel::DirGrant(size_t slot) noexcept
    {
        Xaction& x = tracker[slot];

        DirectoryEntry& dir = directory[x.line];

        if (x.kind == XactKind::ReadShared)
        {
            if (dir.state == LineState::U && dir.owner == x.port)
            {
                // defensive: the requester already owns the line
                if (verboseState)
                    logger->debug("EarthModel: directory line {:#x}: grant skipped, requester {} already owns it.",
                        x.line << 6, x.port);
                return;
            }

            if (dir.state == LineState::U)
            {
                // the previous owner was snooped SnpToShared and stays a
                // sharer, unless it answered I (e.g. it evicted in between)
                bool keepOwner = true;

                for (const Snoop& snoop : x.snoops)
                {
                    if (snoop.port == dir.owner
                     && snoop.opcode == CCHI::Opcodes::SNP::SnpToShared)
                    {
                        keepOwner = snoop.respNotI;
                    }
                }

                uint32_t sharers = uint32_t(1) << x.port;

                if (keepOwner)
                    sharers |= uint32_t(1) << dir.owner;

                if (verboseState)
                    logger->debug("EarthModel: directory line {:#x}: U(owner {}) -> S(sharers {:#x}) at time {}.",
                        x.line << 6, dir.owner, sharers, time);

                dir.state   = LineState::S;
                dir.sharers = sharers;
            }
            else if (dir.state == LineState::S)
            {
                dir.sharers |= uint32_t(1) << x.port;

                if (verboseState)
                    logger->debug("EarthModel: directory line {:#x}: sharer {} added, sharers now {:#x} at time {}.",
                        x.line << 6, x.port, dir.sharers, time);
            }
            else
            {
                dir.state   = LineState::S;
                dir.sharers = uint32_t(1) << x.port;

                if (verboseState)
                    logger->debug("EarthModel: directory line {:#x}: I -> S(sharers {:#x}) at time {}.",
                        x.line << 6, dir.sharers, time);
            }
        }
        else if (x.kind == XactKind::ReadUnique || x.kind == XactKind::MakeUnique)
        {
            if (verboseState)
                logger->debug("EarthModel: directory line {:#x}: (state {}, sharers {:#x}, owner {}) -> U(owner {}) at time {}.",
                    x.line << 6, TextOfLineState(static_cast<size_t>(dir.state)), dir.sharers, dir.owner, x.port, time);

            dir.state   = LineState::U;
            dir.sharers = 0;
            dir.owner   = x.port;
        }
    }

    void EarthModel::MergeBeat(uint64_t line, size_t dataID, const uint64_t* data, uint32_t BE) noexcept
    {
        std::array<uint64_t, LINE_WORDS>& lineData = memory[line];

        // one BE bit per byte within the beat payload
        for (size_t w = 0; w < WORDS_PER_BEAT; ++w)
        {
            for (size_t b = 0; b < sizeof(uint64_t); ++b)
            {
                if (BE & (uint32_t(1) << (w * sizeof(uint64_t) + b)))
                {
                    uint64_t  mask = uint64_t(0xFF) << (b * 8);
                    uint64_t& dst  = lineData[dataID * WORDS_PER_BEAT + w];

                    dst = (dst & ~mask) | (data[w] & mask);
                }
            }
        }
    }

    void EarthModel::ReportBackpressure(const char* what, size_t port) noexcept
    {
        countDeniedBackpressure++;

        if (verboseBackpressure)
            logger->debug("EarthModel: {} at time {} (port {}).", what, time, port);
    }

    void EarthModel::ReportViolation(const char* what, size_t port) noexcept
    {
        countDeniedProtocol++;

        logger->warn("EarthModel: {} at time {} (port {}).", what, time, port);
    }

    bool EarthModel::CheckAddressWindow(uint64_t addr, const char* what, size_t port) noexcept
    {
        if (addr < memoryStart || addr >= memoryEnd)
        {
            logger->warn("EarthModel: {} address {:#x} outside the configured memory window [{:#x}, {:#x}) at time {} (port {}).",
                what, addr, memoryStart, memoryEnd, time, port);

            countDeniedProtocol++;
            return false;
        }

        return true;
    }

    void EarthModel::DumpState() const noexcept
    {
        logger->info("EarthModel state dump at time {}:", time);

        for (size_t slotIdx = 0; slotIdx < tracker.size(); ++slotIdx)
        {
            const Xaction& x = tracker[slotIdx];

            if (!x.active)
                continue;

            logger->info(" - slot {}: {} {} on line {:#x} (port {}), txnID {}, dbid {}, "
                "ack {}, CompData beats {}, snoops {}/{} done, ready time {}",
                slotIdx,
                TextOfXactKind(static_cast<size_t>(x.kind)),
                TextOfXactPhase(static_cast<size_t>(x.phase)),
                x.line << 6, x.port, x.txnID, x.dbid,
                x.ackReceived, x.compDataBeats,
                [&] {
                    size_t done = 0;
                    for (const Snoop& snoop : x.snoops)
                        done += snoop.done;
                    return done;
                }(),
                x.snoops.size(),
                x.readyTime);
        }

        for (size_t port = 0; port < portCount; ++port)
        {
            const Port& p = ports[port];

            logger->info(" - port {}: FIFOs EVT {} / REQ {} / UpRSP {} / UpDAT {}, "
                "queues SNP {} / DnRSP {} / DnDAT {}, snoops outstanding {}, DBIDs outstanding {}",
                port,
                p.evtFIFO.size(), p.reqFIFO.size(), p.uprspFIFO.size(), p.updatFIFO.size(),
                p.snpQueue.size(), p.dnrspQueue.size(), p.dndatQueue.size(),
                p.snoops.size(), p.dbids.size());
        }

        logger->info(" - lines: {} REQ-active, {} EVT-active, {} waiting, directory {} entries, memory {} lines",
            lineREQ.size(), lineEVT.size(), lineWaiters.size(), directory.size(), memory.size());
    }
}
