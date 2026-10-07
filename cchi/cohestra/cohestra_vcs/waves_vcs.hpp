//*****************************************************************************
// waves_vcs.hpp — waveform configuration & control for the Cohestra VCS
// harness. VCS-side counterpart of cohestra_v3/waves.hpp: it re-reads the
// SAME [verilated] waveform.* INI keys (the section name is kept for INI
// compatibility with the Verilator flow), but the actual dumping is performed
// by the simulator inside the SV bridge ($dumpvars for type=vcd,
// $fsdbDumpvars for type=fsdb), driven from here through the DPI exports.
//
// Differences from the Verilator flow:
//   * waveform.type accepts "vcd" and "fsdb" (empty = the build default:
//     fsdb when built with Verdi PLI support, vcd otherwise).
//   * waveform.type = "fst" is Verilator-only and is REJECTED: a hard
//     configuration error when waveform.enable = 1, a warning otherwise.
//   * There is no per-tick Dump(): the simulator dumps continuously once
//     opened, so Dump() is a no-op kept for call-site parity with the
//     cohestra_v3 executable.
//   * waveform.start is honored with $dumpoff/$dumpon ($fsdbDumpoff/
//     $fsdbDumpon) at start*500ps (one harness time unit = one half cycle).
//
// Plusarg fallback: when the INI does not enable dumping, +cohestra_vcd=<path>
// / +cohestra_fsdb=<path> (passed into CohestraVCS_Main by the bridge) select
// the format and path directly, dumping from time 0.
//*****************************************************************************
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "../cohestra_config.hpp"


namespace Cohestra::Waves {

    // Configuration entries, section [verilated] (identical keys to the
    // Verilator flow — see cohestra_v3/waves.cpp):
    //   waveform.enable     - enable waveform dumping
    //   waveform.path       - output file path, empty for the default
    //                         "cohestra.vcd" / "cohestra.fsdb"
    //   waveform.type       - vcd / fsdb, empty for the build default
    //   waveform.start      - first harness time unit to dump (0 = from start)
    extern const ConfigurationEntryBack<bool>           CONFIG_WAVEFORM_ENABLE;
    extern const ConfigurationEntryBack<std::string>    CONFIG_WAVEFORM_PATH;
    extern const ConfigurationEntryBack<std::string>    CONFIG_WAVEFORM_TYPE;
    extern const ConfigurationEntryBack<uint64_t>       CONFIG_WAVEFORM_START;

    bool LoadConfiguration(const StringLoaderBase& loader, Configuration& config) noexcept;


    class WaveControl {
    public:
        WaveControl() noexcept = default;
        ~WaveControl() noexcept;

        WaveControl(const WaveControl&) = delete;
        WaveControl& operator=(const WaveControl&) = delete;

    public:
        // Plusarg paths from the bridge (+cohestra_vcd / +cohestra_fsdb);
        // used only when the INI does not enable dumping.
        void                    SetPlusargPaths(std::string vcdPath, std::string fsdbPath) noexcept;

        // Resolves the configuration entries and validates the requested
        // type. Returns false on configuration errors (e.g. type=fst with
        // waveform.enable=1, or fsdb without Verdi PLI support).
        bool                    Configure(const Configuration& config) noexcept;

        // Whether waveform dumping is enabled and validated.
        bool                    IsConfigured() const noexcept { return configured; }

        uint32_t                GetType() const noexcept { return type; }   // Cohestra::VCS::wire::WAVE_*
        const std::string&      GetPath() const noexcept { return path; }

    public:
        // Opens the waveform file in the simulator (DPI). Considers the
        // plusarg paths when the INI did not enable dumping. No-op (returns
        // true) when nothing is to be dumped.
        bool                    Open() noexcept;

        // Closes the waveform file. Idempotent.
        void                    Close() noexcept;

        bool                    IsOpen() const noexcept { return opened; }

    public:
        // Simulator-side dumping is continuous; these are call-site-parity
        // no-ops (the Verilator flow toggles/dumps per tick).
        void                    Start() noexcept { }
        void                    Stop() noexcept { }
        bool                    IsTracing() const noexcept { return opened; }
        void                    Dump(uint64_t time) noexcept { (void) time; }

    private:
        bool                                configured  = false;
        uint32_t                            type        = 0;      // wire::WAVE_*
        std::string                         path;
        uint64_t                            startAt     = 0;

        std::string                         plusargVcd;
        std::string                         plusargFsdb;

        bool                                opened      = false;
    };
}
