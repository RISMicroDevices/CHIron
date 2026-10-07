#include "cohestra_clogb.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "simpleini/SimpleIni.h"

#include "../../chi_eb/util/chi_eb_util_flit.hpp"

#include "spdlog/spdlog.h"


namespace Cohestra {

    const ConfigurationEntryBack<bool> CONFIG_CLOGB_ENABLE
        ("CLOGB_ENABLE", "enable", "clogb", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::string> CONFIG_CLOGB_PATH
        ("CLOGB_PATH", "path", "clogb", "avocado.clogb", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> CONFIG_CLOGB_COMPRESS
        ("CLOGB_COMPRESS", "compress", "clogb", true, DeserializeBool, SerializeBool);
}


// Parsing helpers
namespace Cohestra::details {

    inline std::string CloGBLowercase(std::string value) noexcept
    {
        for (char& c : value)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return value;
    }

    inline std::string CloGBTrim(const std::string& value) noexcept
    {
        const char* ws = " \t\r\n";
        size_t first = value.find_first_not_of(ws);
        if (first == std::string::npos)
            return "";
        size_t last = value.find_last_not_of(ws);
        return value.substr(first, last - first + 1);
    }

    // [clogb.topo] node type strings -> CLog::NodeType (case-insensitive)
    inline std::optional<CLog::NodeType> CloGBParseNodeType(const std::string& text) noexcept
    {
        std::string value = CloGBLowercase(CloGBTrim(text));

        if (value == "rnf") return CLog::NodeType::RN_F;
        if (value == "rnd") return CLog::NodeType::RN_D;
        if (value == "rni") return CLog::NodeType::RN_I;
        if (value == "hnf") return CLog::NodeType::HN_F;
        if (value == "hni") return CLog::NodeType::HN_I;
        if (value == "snf") return CLog::NodeType::SN_F;
        if (value == "sni") return CLog::NodeType::SN_I;
        if (value == "mn")  return CLog::NodeType::MN;

        return std::nullopt;
    }

    // Strictly decimal, 16-bit (CLog.B topology/record node IDs are 2B)
    inline std::optional<uint32_t> CloGBParseDecimal(const std::string& text) noexcept
    {
        if (text.empty())
            return std::nullopt;

        uint32_t value = 0;
        for (char c : text)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return std::nullopt;

            value = value * 10 + uint32_t(c - '0');
            if (value > 0xFFFF)
                return std::nullopt;
        }

        return value;
    }
}


// Implementation of: class CLogBWriter
namespace Cohestra {

    CLogBWriter::~CLogBWriter() noexcept
    {
        Close();
    }

    bool CLogBWriter::LoadConfiguration(const StringLoaderBase& loader, const std::string& iniFilePath) noexcept
    {
        if (!LoadConfigurationEntry(loader, &CONFIG_CLOGB_ENABLE, enabled))
            return false;

        if (!LoadConfigurationEntry(loader, &CONFIG_CLOGB_PATH, path))
            return false;

        if (!LoadConfigurationEntry(loader, &CONFIG_CLOGB_COMPRESS, compress))
            return false;

        if (!enabled)
            return true;

        if (iniFilePath.empty())
        {
            spdlog::error("[clogb] enable=1 requires an INI configuration file: "
                "the mandatory [clogb.topo] section cannot be enumerated through "
                "the generic configuration loader.");
            return false;
        }

        return LoadEnumeratedKeys(iniFilePath);
    }

    bool CLogBWriter::LoadEnumeratedKeys(const std::string& iniFilePath) noexcept
    {
        constexpr uint32_t nodeIdLimit = 1u << COHESTRA_CHI_NODEID_WIDTH;

        CSimpleIniA ini;
        ini.SetUnicode();

        if (ini.LoadFile(iniFilePath.c_str()) != SI_OK)
        {
            spdlog::error("CLog.B writer: failed to load configuration file for key enumeration: {}.", iniFilePath);
            return false;
        }

        // [clogb] enumerated keys: rnf.nodeid.<port> / snf.nodeid.<port>
        // (the scalar keys enable/path/compress were parsed by the loader already)
        {
            CSimpleIniA::TNamesDepend keys;

            if (ini.GetAllKeys("clogb", keys))
            {
                std::set<size_t> rnPortsSeen;
                std::set<size_t> snPortsSeen;

                for (const auto& key : keys)
                {
                    std::string name    = key.pItem;
                    std::string lowered = details::CloGBLowercase(name);

                    const bool isRN = lowered.starts_with("rnf.nodeid.");
                    const bool isSN = !isRN && lowered.starts_with("snf.nodeid.");

                    if (!isRN && !isSN)
                        continue;

                    const char* kind = isRN ? "rnf" : "snf";
                    std::string indexText = name.substr(std::string(kind).size() + std::string(".nodeid.").size());

                    std::optional<uint32_t> port = details::CloGBParseDecimal(indexText);
                    if (!port)
                    {
                        spdlog::error("CLog.B writer: [clogb] {}: port index \"{}\" is not a decimal number.",
                            name, indexText);
                        return false;
                    }

                    const char* rawValue = ini.GetValue("clogb", name.c_str());
                    if (!rawValue)
                    {
                        spdlog::error("CLog.B writer: [clogb] {}: missing value.", name);
                        return false;
                    }

                    std::optional<uint32_t> nodeId = details::CloGBParseDecimal(details::CloGBTrim(rawValue));
                    if (!nodeId)
                    {
                        spdlog::error("CLog.B writer: [clogb] {}: node ID \"{}\" is not a decimal number.",
                            name, details::CloGBTrim(rawValue));
                        return false;
                    }

                    if (*nodeId >= nodeIdLimit)
                    {
                        spdlog::error("CLog.B writer: [clogb] {}: node ID {} is not representable in the compiled {}-bit node-ID width.",
                            name, *nodeId, COHESTRA_CHI_NODEID_WIDTH);
                        return false;
                    }

                    auto& nodeIds    = isRN ? rnNodeIds    : snNodeIds;
                    auto& portsSeen  = isRN ? rnPortsSeen  : snPortsSeen;

                    if (!portsSeen.insert(*port).second)
                    {
                        spdlog::error("CLog.B writer: [clogb] {}: duplicate {} node-ID assignment for monitor port {}.",
                            name, kind, *port);
                        return false;
                    }

                    nodeIds[*port] = uint16_t(*nodeId);
                }
            }
        }

        // [clogb.topo]: one key per mesh node, <decimal nid> = <type>
        {
            CSimpleIniA::TNamesDepend topoKeys;

            if (!ini.GetAllKeys("clogb.topo", topoKeys) || topoKeys.empty())
            {
                spdlog::error("CLog.B writer: [clogb] enable=1 requires a non-empty [clogb.topo] section "
                    "(one line per mesh node: <decimal nid> = <rnf|rnd|rni|hnf|hni|snf|sni|mn>).");
                return false;
            }

            std::set<uint16_t> nidsSeen;

            for (const auto& key : topoKeys)
            {
                std::string nidText = key.pItem;

                std::optional<uint32_t> nid = details::CloGBParseDecimal(nidText);
                if (!nid)
                {
                    spdlog::error("CLog.B writer: [clogb.topo] {}: node ID key is not a decimal number.", nidText);
                    return false;
                }

                if (*nid >= nodeIdLimit)
                {
                    spdlog::error("CLog.B writer: [clogb.topo] {}: node ID is not representable in the compiled {}-bit node-ID width.",
                        nidText, COHESTRA_CHI_NODEID_WIDTH);
                    return false;
                }

                const char* rawType = ini.GetValue("clogb.topo", nidText.c_str());
                if (!rawType)
                {
                    spdlog::error("CLog.B writer: [clogb.topo] {}: missing node type.", nidText);
                    return false;
                }

                std::optional<CLog::NodeType> type = details::CloGBParseNodeType(rawType);
                if (!type)
                {
                    spdlog::error("CLog.B writer: [clogb.topo] {}: unknown node type \"{}\" "
                        "(expected rnf/rnd/rni/hnf/hni/snf/sni/mn).",
                        nidText, details::CloGBTrim(rawType));
                    return false;
                }

                if (!nidsSeen.insert(uint16_t(*nid)).second)
                {
                    spdlog::error("CLog.B writer: [clogb.topo] {}: duplicate node ID.", nidText);
                    return false;
                }

                topology.push_back({ uint16_t(*nid), *type });
            }

            std::sort(topology.begin(), topology.end(),
                [] (const TopoNode& a, const TopoNode& b) { return a.id < b.id; });
        }

        // every configured monitor port node must exist in the topology with
        // the matching type
        for (const auto& [port, nid] : rnNodeIds)
        {
            auto it = std::find_if(topology.begin(), topology.end(),
                [nid] (const TopoNode& node) { return node.id == nid; });

            if (it == topology.end() || it->type != CLog::NodeType::RN_F)
            {
                spdlog::error("CLog.B writer: [clogb] rnf.nodeid.{} = {}: node {} is not present in [clogb.topo] typed rnf.",
                    port, nid, nid);
                return false;
            }
        }

        for (const auto& [port, nid] : snNodeIds)
        {
            auto it = std::find_if(topology.begin(), topology.end(),
                [nid] (const TopoNode& node) { return node.id == nid; });

            if (it == topology.end() || it->type != CLog::NodeType::SN_F)
            {
                spdlog::error("CLog.B writer: [clogb] snf.nodeid.{} = {}: node {} is not present in [clogb.topo] typed snf.",
                    port, nid, nid);
                return false;
            }
        }

        if (rnNodeIds.empty() && snNodeIds.empty())
            spdlog::warn("CLog.B writer: enabled but no [clogb] rnf.nodeid.*/snf.nodeid.* keys configured; "
                "only the parameters and topology tags will be written.");

        return true;
    }

    bool CLogBWriter::Open(const CHI::CHIMonitorInterface* monitorInterface) noexcept
    {
        if (!enabled)
            return true;

        if (opened)
            return true;

        if (!monitorInterface)
        {
            spdlog::error("CLog.B writer: no CHI monitor interface to drain.");
            return false;
        }

        // every configured monitor port must exist on the interface
        {
            std::set<size_t> ports = monitorInterface->GetPortIndices();

            for (const auto& [port, nid] : rnNodeIds)
            {
                if (!ports.count(port))
                {
                    spdlog::error("CLog.B writer: [clogb] rnf.nodeid.{} = {}: the monitor interface has no port {}.",
                        port, nid, port);
                    return false;
                }
            }

            for (const auto& [port, nid] : snNodeIds)
            {
                if (!ports.count(port))
                {
                    spdlog::error("CLog.B writer: [clogb] snf.nodeid.{} = {}: the monitor interface has no port {}.",
                        port, nid, port);
                    return false;
                }
            }
        }

        stream.open(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            spdlog::error("CLog.B writer: failed to open log file: {}.", path);
            return false;
        }

        // CHI parameters tag: Issue-E.b with the geometry compiled into the
        // interface layer (the COHESTRA_CHI_* defines, cohestra_chi.hpp)
        {
            CLog::CLogB::TagCHIParameters parametersTag;
            CLog::Parameters& parameters = parametersTag.parameters;

            parameters.SetIssue(CLog::Issue::Eb);

            if (!parameters.SetNodeIdWidth(COHESTRA_CHI_NODEID_WIDTH)
                || !parameters.SetReqAddrWidth(COHESTRA_CHI_REQ_ADDR_WIDTH)
                || !parameters.SetReqRSVDCWidth(COHESTRA_CHI_REQ_RSVDC_WIDTH)
                || !parameters.SetDatRSVDCWidth(COHESTRA_CHI_DAT_RSVDC_WIDTH)
                || !parameters.SetDataWidth(COHESTRA_CHI_DATA_WIDTH))
            {
                spdlog::error("CLog.B writer: the compiled COHESTRA_CHI_* flit geometry is rejected by the CLog.B parameter validation.");
                return false;
            }

            parameters.SetDataCheckPresent(COHESTRA_CHI_DATACHECK_PRESENT != 0);
            parameters.SetPoisonPresent(COHESTRA_CHI_POISON_PRESENT != 0);
            parameters.SetMPAMPresent(COHESTRA_CHI_MPAM_PRESENT != 0);

            writer.Next(stream, &parametersTag);
        }

        // topology tag (a single CHI_TOPOS tag is the atomic equivalent of
        // the DPI writer's WriteTopo* -> WriteTopoEnd sequence)
        {
            CLog::CLogB::TagCHITopologies toposTag;
            toposTag.nodes.reserve(topology.size());

            for (const TopoNode& node : topology)
                toposTag.nodes.push_back({ node.type, node.id });

            writer.Next(stream, &toposTag);
        }

        if (!stream)
        {
            spdlog::error("CLog.B writer: failed to write the header tags to {}.", path);
            return false;
        }

        // land the header on disk immediately: a simulator-side $finish (DUT
        // assertion, watchdog) bypasses C++ teardown incl. Close(), so the
        // log must be complete on disk after every write point
        stream.flush();

        opened = true;

        spdlog::info("CLog.B writer opened: {} ({} topology node(s), {} RN port(s), {} SN port(s){}).",
            path, topology.size(), rnNodeIds.size(), snNodeIds.size(),
            compress ? ", compressed" : "");

        return true;
    }

    void CLogBWriter::AppendREQ(uint16_t nodeId, CLog::Channel channel, const CHI::FiredREQ& fired, uint64_t time) noexcept
    {
        if (pending.records.empty())
            pendingTimeBase = time;

        auto& record      = pending.records.emplace_back();
        record.timeShift  = uint32_t(time - pendingTimeBase);
        record.nodeId     = nodeId;
        record.channel    = channel;
        record.flitLength = uint8_t((CHI::REQFlit::WIDTH + 7) / 8);
        record.flit       = std::shared_ptr<uint32_t[]>(new uint32_t[(record.flitLength + 3) >> 2]());

        ::CHI::Eb::Flits::SerializeREQ(fired.Flit, record.flit.get(), CHI::REQFlit::WIDTH);

        ++countRecords;
    }

    void CLogBWriter::AppendRSP(uint16_t nodeId, CLog::Channel channel, const CHI::FiredRSP& fired, uint64_t time) noexcept
    {
        if (pending.records.empty())
            pendingTimeBase = time;

        auto& record      = pending.records.emplace_back();
        record.timeShift  = uint32_t(time - pendingTimeBase);
        record.nodeId     = nodeId;
        record.channel    = channel;
        record.flitLength = uint8_t((CHI::RSPFlit::WIDTH + 7) / 8);
        record.flit       = std::shared_ptr<uint32_t[]>(new uint32_t[(record.flitLength + 3) >> 2]());

        ::CHI::Eb::Flits::SerializeRSP(fired.Flit, record.flit.get(), CHI::RSPFlit::WIDTH);

        ++countRecords;
    }

    void CLogBWriter::AppendDAT(uint16_t nodeId, CLog::Channel channel, const CHI::FiredDAT& fired, uint64_t time) noexcept
    {
        if (pending.records.empty())
            pendingTimeBase = time;

        auto& record      = pending.records.emplace_back();
        record.timeShift  = uint32_t(time - pendingTimeBase);
        record.nodeId     = nodeId;
        record.channel    = channel;
        record.flitLength = uint8_t((CHI::DATFlit::WIDTH + 7) / 8);
        record.flit       = std::shared_ptr<uint32_t[]>(new uint32_t[(record.flitLength + 3) >> 2]());

        ::CHI::Eb::Flits::SerializeDAT(fired.Flit, record.flit.get(), CHI::DATFlit::WIDTH);

        ++countRecords;
    }

    void CLogBWriter::AppendSNP(uint16_t nodeId, CLog::Channel channel, const CHI::FiredSNP& fired, uint64_t time) noexcept
    {
        if (pending.records.empty())
            pendingTimeBase = time;

        auto& record      = pending.records.emplace_back();
        record.timeShift  = uint32_t(time - pendingTimeBase);
        record.nodeId     = nodeId;
        record.channel    = channel;
        record.flitLength = uint8_t((CHI::SNPFlit::WIDTH + 7) / 8);
        record.flit       = std::shared_ptr<uint32_t[]>(new uint32_t[(record.flitLength + 3) >> 2]());

        ::CHI::Eb::Flits::SerializeSNP(fired.Flit, record.flit.get(), CHI::SNPFlit::WIDTH);

        ++countRecords;
    }

    void CLogBWriter::DrainPortRN(CHI::CHIMonitorInterface* monitorInterface, size_t port, uint16_t nodeId, uint64_t time) noexcept
    {
        while (auto fired = monitorInterface->PopTXREQ(port))
            AppendREQ(nodeId, CLog::Channel::TXREQ, *fired, time);

        while (auto fired = monitorInterface->PopTXRSP(port))
            AppendRSP(nodeId, CLog::Channel::TXRSP, *fired, time);

        while (auto fired = monitorInterface->PopTXDAT(port))
            AppendDAT(nodeId, CLog::Channel::TXDAT, *fired, time);

        while (auto fired = monitorInterface->PopRXRSP(port))
            AppendRSP(nodeId, CLog::Channel::RXRSP, *fired, time);

        while (auto fired = monitorInterface->PopRXDAT(port))
            AppendDAT(nodeId, CLog::Channel::RXDAT, *fired, time);

        while (auto fired = monitorInterface->PopRXSNP(port))
            AppendSNP(nodeId, CLog::Channel::RXSNP, *fired, time);
    }

    void CLogBWriter::DrainPortSN(CHI::CHIMonitorInterface* monitorInterface, size_t port, uint16_t nodeId, uint64_t time) noexcept
    {
        // SN-side taps, mapped to the SN node's own channel frame:
        // inbound REQ is the node's RXREQ, outbound RSP/DAT are TX, inbound
        // DAT is RXDAT
        while (auto fired = monitorInterface->PopSN_RXREQ(port))
            AppendREQ(nodeId, CLog::Channel::RXREQ, *fired, time);

        while (auto fired = monitorInterface->PopSN_TXRSP(port))
            AppendRSP(nodeId, CLog::Channel::TXRSP, *fired, time);

        while (auto fired = monitorInterface->PopSN_TXDAT(port))
            AppendDAT(nodeId, CLog::Channel::TXDAT, *fired, time);

        while (auto fired = monitorInterface->PopSN_RXDAT(port))
            AppendDAT(nodeId, CLog::Channel::RXDAT, *fired, time);
    }

    void CLogBWriter::DrainPortUnconfigured(CHI::CHIMonitorInterface* monitorInterface, size_t port, bool rnSide, bool snSide) noexcept
    {
        // drain queues without a configured node ID so they cannot grow
        // unboundedly; the flits cannot be attributed to a mesh node and are
        // dropped (counted, reported at Close())
        if (rnSide)
        {
            while (monitorInterface->PopTXREQ(port))    ++countDropped;
            while (monitorInterface->PopTXRSP(port))    ++countDropped;
            while (monitorInterface->PopTXDAT(port))    ++countDropped;
            while (monitorInterface->PopRXRSP(port))    ++countDropped;
            while (monitorInterface->PopRXDAT(port))    ++countDropped;
            while (monitorInterface->PopRXSNP(port))    ++countDropped;
        }

        if (snSide)
        {
            while (monitorInterface->PopSN_RXREQ(port)) ++countDropped;
            while (monitorInterface->PopSN_TXRSP(port)) ++countDropped;
            while (monitorInterface->PopSN_TXDAT(port)) ++countDropped;
            while (monitorInterface->PopSN_RXDAT(port)) ++countDropped;
        }
    }

    void CLogBWriter::DrainAndWrite(CHI::CHIMonitorInterface* monitorInterface, uint64_t time) noexcept
    {
        if (!opened)
            return;

        // the interface's port indices are an ordered set, so the record
        // order within one tick is deterministic
        for (size_t port : monitorInterface->GetPortIndices())
        {
            auto rn = rnNodeIds.find(port);
            auto sn = snNodeIds.find(port);

            if (rn != rnNodeIds.end())
                DrainPortRN(monitorInterface, port, rn->second, time);
            else
                DrainPortUnconfigured(monitorInterface, port, true, false);

            if (sn != snNodeIds.end())
                DrainPortSN(monitorInterface, port, sn->second, time);
            else
                DrainPortUnconfigured(monitorInterface, port, false, true);
        }

        // flush policy: bulk at FLUSH_RECORD_COUNT, plus any non-empty batch
        // at the end of every drain — a simulator-side $finish bypasses
        // Close(), so at most one tick of records may be lost
        if (!pending.records.empty())
            Flush();
    }

    void CLogBWriter::Flush() noexcept
    {
        if (pending.records.empty())
            return;

        pending.head.timeBase   = pendingTimeBase;
        pending.head.compressed = compress ? 1 : 0;

        writer.Next(stream, &pending);

        pending.records.clear();

        // push to the OS now: a simulator-side $finish bypasses Close()
        stream.flush();
    }

    void CLogBWriter::Close() noexcept
    {
        if (!opened)
            return;

        Flush();

        // CLog.B has no serialized EOF marker: TagEOF is a phantom tag the
        // Reader synthesizes at end-of-stream, so closing the file IS the
        // EOF (writing tag 0x7F would be rejected by Reader::Next).
        stream.flush();
        stream.close();

        opened = false;

        spdlog::info("CLog.B writer closed: {} ({} record(s) written{}).",
            path, countRecords,
            countDropped ? std::string(" WARNING: ") + std::to_string(countDropped) +
                " flit(s) dropped from ports without a configured node ID" : "");

        if (countDropped)
            spdlog::warn("CLog.B writer: {} flit(s) were dropped from monitor ports without a configured node ID.",
                countDropped);
    }
}
