#include "cohestra_simperf.hpp"

#include <cstdio>


// Implementation of: class SimPerfMeter
namespace Cohestra {

    const ConfigurationEntryBack<bool> SimPerfMeter::SIMPERF_ENABLE
        ("SIMPERF_ENABLE", "enable", "simperf", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> SimPerfMeter::SIMPERF_INTERVAL_CYCLES
        ("SIMPERF_INTERVAL_CYCLES", "interval.cycles", "simperf", 1000000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> SimPerfMeter::SIMPERF_INTERVAL_SECONDS
        ("SIMPERF_INTERVAL_SECONDS", "interval.seconds", "simperf", 600, DeserializeUInt64, SerializeUInt64);


    SimPerfMeter::SimPerfMeter(Instance& instance) noexcept
        : instance  (instance)
    { }

    template<class T>
    bool SimPerfMeter::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
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
            instance.GetLogger()->error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }

    bool SimPerfMeter::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        return LoadConfigurationEntry(loader, &SIMPERF_ENABLE,           enabled)
            && LoadConfigurationEntry(loader, &SIMPERF_INTERVAL_CYCLES,  intervalCycles)
            && LoadConfigurationEntry(loader, &SIMPERF_INTERVAL_SECONDS, intervalSeconds);
    }

    void SimPerfMeter::SetTicksPerCycle(unsigned ticksPerCycle) noexcept
    {
        this->ticksPerCycle = ticksPerCycle ? ticksPerCycle : 1;
    }

    bool SimPerfMeter::IsEnabled() const noexcept
    {
        return enabled;
    }

    void SimPerfMeter::Start(uint64_t time) noexcept
    {
        if (!enabled)
            return;

        started             = true;
        baseTime            = time;
        lastMarkTime        = time;
        lastWallCheckTime   = time;
        startClock          = std::chrono::steady_clock::now();
        lastMarkClock       = startClock;
    }

    void SimPerfMeter::Tick(uint64_t time) noexcept
    {
        if (!started)
            return;

        // cycle trigger: exact, clock-free
        if (intervalCycles && time - lastMarkTime >= intervalCycles * ticksPerCycle)
        {
            Mark(time);
            return;
        }

        // time trigger: the wall clock is read at most once per quantum of
        // ticks, never on the per-tick hot path
        if (intervalSeconds && time - lastWallCheckTime >= WALL_CHECK_QUANTUM_TICKS)
        {
            lastWallCheckTime = time;

            if (std::chrono::steady_clock::now() - lastMarkClock >= std::chrono::seconds(intervalSeconds))
                Mark(time);
        }
    }

    void SimPerfMeter::Mark(uint64_t time) noexcept
    {
        auto now = std::chrono::steady_clock::now();

        double dtMark   = std::chrono::duration<double>(now - lastMarkClock).count();
        double dtAll    = std::chrono::duration<double>(now - startClock).count();

        uint64_t cyclesMark = (time - lastMarkTime) / ticksPerCycle;
        uint64_t cyclesAll  = (time - baseTime)     / ticksPerCycle;

        double instCps  = dtMark > 0.0 ? cyclesMark / dtMark : 0.0;
        double avgCps   = dtAll  > 0.0 ? cyclesAll  / dtAll  : 0.0;

        instance.GetLogger()->info("[perf] cycle {} | {} inst, {} avg, elapsed {:.1f}s",
            FormatCount(cyclesAll), FormatRate(instCps), FormatRate(avgCps), dtAll);

        lastMarkTime    = time;
        lastMarkClock   = now;
    }

    void SimPerfMeter::Report(uint64_t time) const noexcept
    {
        if (!started)
            return;

        auto now = std::chrono::steady_clock::now();

        double dtAll    = std::chrono::duration<double>(now - startClock).count();

        uint64_t cyclesAll  = (time - baseTime) / ticksPerCycle;

        double avgCps   = dtAll > 0.0 ? cyclesAll / dtAll : 0.0;

        instance.GetLogger()->info("[perf] finished: {} cycles in {:.2f}s | avg {} ({} cycles/s)",
            FormatCount(cyclesAll), dtAll, FormatRate(avgCps), FormatCount(static_cast<uint64_t>(avgCps)));
    }

    std::string SimPerfMeter::FormatRate(double cyclesPerSecond)
    {
        static const struct { double limit; double scale; const char* unit; } SCALES[] = {
            { 1e9, 1e9, "GHz" },
            { 1e6, 1e6, "MHz" },
            { 1e3, 1e3, "KHz" }
        };

        char buffer[64];

        for (const auto& scale : SCALES)
        {
            if (cyclesPerSecond >= scale.limit)
            {
                std::snprintf(buffer, sizeof(buffer), "%.2f %s", cyclesPerSecond / scale.scale, scale.unit);
                return buffer;
            }
        }

        std::snprintf(buffer, sizeof(buffer), "%.2f Hz", cyclesPerSecond);
        return buffer;
    }

    std::string SimPerfMeter::FormatCount(uint64_t value)
    {
        char digits[32];
        std::snprintf(digits, sizeof(digits), "%llu", static_cast<unsigned long long>(value));

        std::string result(digits);

        for (size_t pos = result.length(); pos > 3; pos -= 3)
            result.insert(pos - 3, 1, ',');

        return result;
    }
}
