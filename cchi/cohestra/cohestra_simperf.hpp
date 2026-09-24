#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "cohestra_config.hpp"
#include "cohestra_instance.hpp"


namespace Cohestra {

    // SimPerfMeter: wall-clock simulation-throughput meter ("speedometer").
    //
    // Measures simulated cycles per real second (std::chrono::steady_clock)
    // and reports it two ways:
    //
    //  - in-flight: Tick() emits one log line on either of two triggers,
    //    whichever comes first:
    //      * cycle trigger - every 'simperf.interval.cycles' simulated cycles
    //        (default 1,000,000; 0 disables this trigger);
    //      * time trigger - every 'simperf.interval.seconds' wall-clock
    //        seconds (default 600 = 10 minutes; 0 disables this trigger).
    //    Both triggers share one mark state, so the interval ("instantaneous")
    //    rate always measures cycles/elapsed since the previous printed line.
    //    The line carries the interval rate, the cumulative average rate and
    //    the elapsed wall-clock time, e.g.
    //        [perf] cycle 12,000,000 | 213.42 KHz inst, 187.16 KHz avg, elapsed 64.1s
    //    The wall clock is read at most once per WALL_CHECK_QUANTUM_TICKS
    //    ticks, never on the per-tick hot path; the cycle trigger stays exact.
    //    Setting both intervals to 0 suppresses the in-flight display and
    //    keeps only the final summary;
    //
    //  - final: Report() emits a single summary line with the total simulated
    //    cycles, the total wall-clock time and the average rate, e.g.
    //        [perf] finished: 3,000,000 cycles in 14.27s | avg 210.23 KHz
    //
    // Rates are auto-scaled (Hz/KHz/MHz/GHz, two decimals). The harness tells
    // the meter the ticks-per-cycle ratio via SetTicksPerCycle (earth: 1,
    // v3: 2 - half-cycle ticks), so all reported counts/rates are in true
    // cycles regardless of the harness time unit.
    //
    // Start() is called once right before the main simulation loop (after any
    // reset phase), so reset cycles are excluded from the measurement.
    //
    // This meter is independent from the [profiler] bus profiler (cchi/prof/
    // + cohestra_profiler.*): it measures host wall-clock throughput of the
    // simulation itself, not on-bus protocol metrics. It is enabled by
    // default; 'simperf.enable = false' turns it fully off.
    class SimPerfMeter {
    public:
        static const ConfigurationEntryBack<bool>       SIMPERF_ENABLE;
        static const ConfigurationEntryBack<uint64_t>   SIMPERF_INTERVAL_CYCLES;
        static const ConfigurationEntryBack<uint64_t>   SIMPERF_INTERVAL_SECONDS;

    protected:
        // How often Tick() may read the wall clock for the time trigger
        // (ticks between reads); keeps the per-tick hot path clock-free.
        static constexpr uint64_t   WALL_CHECK_QUANTUM_TICKS = 65536;

    protected:
        Instance&                   instance;

        bool                        enabled         = SIMPERF_ENABLE.defaultValue;
        uint64_t                    intervalCycles  = SIMPERF_INTERVAL_CYCLES.defaultValue;
        uint64_t                    intervalSeconds = SIMPERF_INTERVAL_SECONDS.defaultValue;

        unsigned                    ticksPerCycle   = 1;

        bool                        started         = false;
        uint64_t                    baseTime        = 0;    // harness time at Start()
        uint64_t                    lastMarkTime    = 0;    // harness time at last in-flight mark
        uint64_t                    lastWallCheckTime = 0;  // harness time at last wall-clock read

        std::chrono::steady_clock::time_point
                                    startClock;
        std::chrono::steady_clock::time_point
                                    lastMarkClock;

    public:
        explicit                    SimPerfMeter(Instance& instance) noexcept;

    public:
        bool                        LoadConfiguration(const StringLoaderBase& loader) noexcept;

        void                        SetTicksPerCycle(unsigned ticksPerCycle) noexcept;

        bool                        IsEnabled() const noexcept;

    public:
        // Arms the meter at the current harness time; call once right before
        // the main simulation loop.
        void                        Start(uint64_t time) noexcept;

        // Per-loop-iteration hook with the current harness time. Hot path is
        // a single subtraction; a log line is emitted only on interval marks.
        void                        Tick(uint64_t time) noexcept;

        // Final summary line (totals + average rate); call after the loop.
        void                        Report(uint64_t time) const noexcept;

    protected:
        template<class T>
        bool                        LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;

        // Emits one in-flight line at the current harness time and re-bases
        // the mark state; shared by the cycle and time triggers.
        void                        Mark(uint64_t time) noexcept;

    public:
        // "213.42 KHz" / "1.07 MHz" / "998.00 Hz" (auto-scaled, two decimals)
        static std::string          FormatRate(double cyclesPerSecond);

        // "12,000,000" (thousands-separated)
        static std::string          FormatCount(uint64_t value);
    };
}
