#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <verilated.h>

#include "../cohestra_config.hpp"


namespace Cohestra::Waves {

    enum class WaveformType {
        VCD,
        FST
    };

    std::optional<WaveformType> ParseWaveformType(const std::string& value) noexcept;
    const char*                 WaveformTypeName(WaveformType type) noexcept;
    const char*                 WaveformTypeExtension(WaveformType type) noexcept;

    // The waveform format compiled into this build.
    // Verilator supports only one trace format per verilation; the format is
    // selected at CMake configure time with -DCOHESTRA_V3_WAVEFORM_FORMAT=FST|VCD.
    constexpr WaveformType CompiledWaveformType() noexcept
    {
#ifdef COHESTRA_V3_WAVEFORM_FORMAT_VCD
        return WaveformType::VCD;
#else
        return WaveformType::FST;
#endif
    }

    // Configuration entries, section [verilated] :
    //   waveform.enable     - enable waveform dumping
    //   waveform.path       - output file path, empty for default "cohestra.<ext>"
    //   waveform.type       - vcd / fst, must match the compiled format, empty for the compiled format
    //   waveform.start      - first simulation time to dump (0 = dump from the start)
    extern const ConfigurationEntryBack<bool>           CONFIG_WAVEFORM_ENABLE;
    extern const ConfigurationEntryBack<std::string>    CONFIG_WAVEFORM_PATH;
    extern const ConfigurationEntryBack<std::string>    CONFIG_WAVEFORM_TYPE;
    extern const ConfigurationEntryBack<uint64_t>       CONFIG_WAVEFORM_START;

    bool LoadConfiguration(const StringLoaderBase& loader, Configuration& config) noexcept;


    namespace details {
        struct IWaveFile;
    }


    class Tracer {
    public:
        Tracer() noexcept;
        ~Tracer() noexcept;

        Tracer(const Tracer&) = delete;
        Tracer& operator=(const Tracer&) = delete;

    public:
        // Resolves the configuration entries and validates the requested type
        // against the compiled format. Returns false on configuration errors.
        bool                    Configure(const Configuration& config) noexcept;

        // Whether waveform dumping is enabled and validated by configuration.
        bool                    IsConfigured() const noexcept { return configured; }

        WaveformType            GetType() const noexcept { return type; }
        const std::string&      GetPath() const noexcept { return path; }

    public:
        // Attaches to the verilated context and opens the waveform file.
        // No-op (returns true) when not configured.
        bool                    Open(VerilatedContext& context, int levels = 99) noexcept;

        // Flushes and closes the waveform file. Idempotent.
        void                    Close() noexcept;

        bool                    IsOpen() const noexcept;

    public:
        // Toggles dumping on the open waveform file.
        void                    Start() noexcept;
        void                    Stop() noexcept;

        bool                    IsTracing() const noexcept;

        // Dumps one time point; no-op unless open and tracing.
        void                    Dump(uint64_t time) noexcept;

    private:
        bool                                configured  = false;
        WaveformType                        type        = CompiledWaveformType();
        std::string                         path;
        uint64_t                            startAt     = 0;

        bool                                tracing     = false;

        std::unique_ptr<details::IWaveFile> file;
    };
}
