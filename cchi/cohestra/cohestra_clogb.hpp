#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cohestra_config.hpp"
#include "cohestra_chi.hpp"

#include "spdlog/spdlog.h"

#include "../../clog/clog_b/clog_b.hpp"


namespace Cohestra {

    // CLogBWriter: serializes the flits observed by the CHI monitor
    // interface (V3CHIMonitorInterface*, see cohestra_v3/interface_chi.hpp)
    // into a CLog.B binary log (clog/clog_b).
    //
    // One record is written per observed flit: the node ID is the mesh node
    // configured for the monitor port, the channel is the CLog.B channel
    // matching the monitor queue the flit was popped from, and the time is
    // the harness tick count handed to DrainAndWrite. Records are batched
    // into one CHI_RECORDS tag per flush (timeBase = tick of the first
    // record in the batch, per-record timeShift relative to it).
    //
    // Configuration, section [clogb]:
    //   enable          - 0/1, default 0; the writer is fully inert when 0
    //   path            - output file, default "avocado.clogb"
    //   compress        - 0/1, default 1: zlib-compress the record blocks
    //   rnf.nodeid.<p>  - mesh node ID of the RN-F tapped by monitor port <p>
    //   snf.nodeid.<p>  - mesh node ID of the SN-F tapped by SN-side monitor
    //                     port <p> (additive over the base schema; without it
    //                     the SN-side taps are drained and dropped)
    //
    // Topology, section [clogb.topo] (mandatory when enable=1):
    //   <decimal nid> = <type>   type in rnf/rnd/rni/hnf/hni/snf/sni/mn
    //
    // The enumerated keys (rnf.nodeid.* / snf.nodeid.* / [clogb.topo]) need
    // section enumeration, which the StringLoaderBase interface does not
    // expose; they are parsed from the INI file directly, so
    // LoadConfiguration takes the INI file path alongside the loader.
    //
    // Validation at load (any violation fails the configuration load):
    // duplicate topology NIDs, unknown type strings, non-decimal NID keys,
    // NIDs not representable in the compiled node-ID width, and every
    // rnf.nodeid.* / snf.nodeid.* NID must exist in [clogb.topo] with the
    // matching type. At Open(), every configured monitor port must exist on
    // the monitor interface.
    extern const ConfigurationEntryBack<bool>         CONFIG_CLOGB_ENABLE;
    extern const ConfigurationEntryBack<std::string>  CONFIG_CLOGB_PATH;
    extern const ConfigurationEntryBack<bool>         CONFIG_CLOGB_COMPRESS;

    class CLogBWriter {
    public:
        struct TopoNode {
            uint16_t        id;
            CLog::NodeType  type;
        };

        // Records are accumulated into one CHI_RECORDS tag and flushed when
        // this many records are pending (and at Close()).
        static constexpr size_t FLUSH_RECORD_COUNT = 1024;

    protected:
        bool            enabled     = CONFIG_CLOGB_ENABLE.defaultValue;
        std::string     path        = CONFIG_CLOGB_PATH.defaultValue;
        bool            compress    = CONFIG_CLOGB_COMPRESS.defaultValue;

        std::unordered_map<size_t, uint16_t>    rnNodeIds;  // RN monitor port -> RN-F node ID
        std::unordered_map<size_t, uint16_t>    snNodeIds;  // SN monitor port -> SN-F node ID
        std::vector<TopoNode>                   topology;   // sorted by node ID

        bool                        opened          = false;
        std::ofstream               stream;
        CLog::CLogB::Writer         writer;
        CLog::CLogB::TagCHIRecords  pending;
        uint64_t                    pendingTimeBase = 0;

        uint64_t                    countRecords    = 0;
        uint64_t                    countDropped    = 0;

    public:
        CLogBWriter() noexcept = default;
        ~CLogBWriter() noexcept;

        CLogBWriter(const CLogBWriter&) = delete;
        CLogBWriter& operator=(const CLogBWriter&) = delete;

    public:
        // Parses the scalar keys through the loader (standard component
        // pattern) and the enumerated keys ([clogb] *.nodeid.* and
        // [clogb.topo]) from iniFilePath. iniFilePath may be empty only when
        // the writer is disabled.
        bool    LoadConfiguration(const StringLoaderBase& loader, const std::string& iniFilePath) noexcept;

        // Creates the log file and writes the CHI parameters (Issue-E.b,
        // geometry from the compiled COHESTRA_CHI_* configuration) and the
        // configured topology. Validates the configured monitor ports against
        // the interface. No-op (returns true) when disabled.
        bool    Open(const CHI::CHIMonitorInterface* monitorInterface) noexcept;

        // Pops every per-port monitor queue and appends one record per flit.
        // Queues of ports without a configured node ID are drained and
        // dropped (counted, reported at Close()). No-op unless open.
        void    DrainAndWrite(CHI::CHIMonitorInterface* monitorInterface, uint64_t time) noexcept;

        // Flushes the pending records; end-of-stream is the CLog.B EOF
        // marker (TagEOF is a phantom tag, never serialized). Idempotent.
        void    Close() noexcept;

    public:
        bool                IsEnabled() const noexcept        { return enabled; }
        bool                IsOpen() const noexcept           { return opened; }
        const std::string&  GetPath() const noexcept          { return path; }
        uint64_t            GetRecordCount() const noexcept   { return countRecords; }
        uint64_t            GetDroppedCount() const noexcept  { return countDropped; }

    protected:
        template<class T>
        bool    LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

        bool    LoadEnumeratedKeys(const std::string& iniFilePath) noexcept;

        void    DrainPortRN(CHI::CHIMonitorInterface* monitorInterface, size_t port, uint16_t nodeId, uint64_t time) noexcept;
        void    DrainPortSN(CHI::CHIMonitorInterface* monitorInterface, size_t port, uint16_t nodeId, uint64_t time) noexcept;
        void    DrainPortUnconfigured(CHI::CHIMonitorInterface* monitorInterface, size_t port, bool rnSide, bool snSide) noexcept;

        void    AppendREQ(uint16_t nodeId, CLog::Channel channel, const CHI::FiredREQ& fired, uint64_t time) noexcept;
        void    AppendRSP(uint16_t nodeId, CLog::Channel channel, const CHI::FiredRSP& fired, uint64_t time) noexcept;
        void    AppendDAT(uint16_t nodeId, CLog::Channel channel, const CHI::FiredDAT& fired, uint64_t time) noexcept;
        void    AppendSNP(uint16_t nodeId, CLog::Channel channel, const CHI::FiredSNP& fired, uint64_t time) noexcept;

        void    Flush() noexcept;
    };
}


// Inline implementation of: class CLogBWriter
namespace Cohestra {

    template<class T>
    inline bool CLogBWriter::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry);

        if (!rawValue)
        {
            dst = entry->defaultValue;
            return true;
        }

        std::optional<T> deserializedValue = entry->deserializer(*rawValue);

        if (!deserializedValue)
        {
            spdlog::error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }
}
