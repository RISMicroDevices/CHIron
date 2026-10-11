#include "waves.hpp"

#include <cctype>

#include <verilated_vcd_c.h>
#include <verilated_fst_c.h>

#include "spdlog/spdlog.h"


// Type-erased wrappers over VerilatedVcdC / VerilatedFstC, which share no
// common base exposing open/close/dump.
namespace Cohestra::Waves::details {

    struct IWaveFile {
        virtual ~IWaveFile() noexcept = default;

        virtual void                    Open(const char* path) = 0;
        virtual void                    Close() = 0;
        virtual bool                    IsOpen() const = 0;
        virtual void                    Dump(uint64_t time) = 0;
        virtual VerilatedTraceBaseC*    Base() = 0;
    };

    template<class T>
    struct WaveFile final : IWaveFile {
        T impl;

        void                    Open(const char* path) override { impl.open(path); }
        void                    Close() override { impl.close(); }
        bool                    IsOpen() const override { return impl.isOpen(); }
        void                    Dump(uint64_t time) override { impl.dump(time); }
        VerilatedTraceBaseC*    Base() override { return &impl; }
    };
}


// Implementation of: configuration entries
namespace Cohestra::Waves {

    const ConfigurationEntryBack<bool> CONFIG_WAVEFORM_ENABLE
        ("WAVEFORM_ENABLE", "waveform.enable", "verilated", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::string> CONFIG_WAVEFORM_PATH
        ("WAVEFORM_PATH", "waveform.path", "verilated", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<std::string> CONFIG_WAVEFORM_TYPE
        ("WAVEFORM_TYPE", "waveform.type", "verilated", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<uint64_t> CONFIG_WAVEFORM_START
        ("WAVEFORM_START", "waveform.start", "verilated", 0, DeserializeUInt64, SerializeUInt64);
}


// Implementation of: helpers
namespace Cohestra::Waves {

    std::optional<WaveformType> ParseWaveformType(const std::string& value) noexcept
    {
        std::string lower;
        lower.reserve(value.size());

        for (char c : value)
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

        if (lower == "vcd")
            return { WaveformType::VCD };

        if (lower == "fst")
            return { WaveformType::FST };

        return std::nullopt;
    }

    const char* WaveformTypeName(WaveformType type) noexcept
    {
        switch (type)
        {
            case WaveformType::VCD: return "vcd";
            case WaveformType::FST: return "fst";
            default:                return "unknown";
        }
    }

    const char* WaveformTypeExtension(WaveformType type) noexcept
    {
        switch (type)
        {
            case WaveformType::VCD: return ".vcd";
            case WaveformType::FST: return ".fst";
            default:                return "";
        }
    }

    static const char* WaveformTypeCMakeName(WaveformType type) noexcept
    {
        switch (type)
        {
            case WaveformType::VCD: return "VCD";
            case WaveformType::FST: return "FST";
            default:                return "";
        }
    }
}


// Implementation of: LoadConfiguration
namespace Cohestra::Waves {

    namespace {

        template<class T>
        bool LoadConfigurationEntry(const StringLoaderBase& loader, Configuration& config, ConfigurationEntry<T> entry) noexcept
        {
            std::optional<std::string> rawValue = loader.GetRawString(entry);

            if (!rawValue)
            {
                config.Set(entry, entry->defaultValue);
                return true;
            }

            std::optional<T> value = entry->deserializer(*rawValue);

            if (!value)
            {
                spdlog::error("Failed to parse {} ({}) from configuration value: {}",
                    entry->key, entry->name, *rawValue);

                return false;
            }

            config.Set(entry, *value);
            return true;
        }
    }

    bool LoadConfiguration(const StringLoaderBase& loader, Configuration& config) noexcept
    {
        return LoadConfigurationEntry(loader, config, &CONFIG_WAVEFORM_ENABLE)
            && LoadConfigurationEntry(loader, config, &CONFIG_WAVEFORM_PATH)
            && LoadConfigurationEntry(loader, config, &CONFIG_WAVEFORM_TYPE)
            && LoadConfigurationEntry(loader, config, &CONFIG_WAVEFORM_START);
    }
}


// Implementation of: class Tracer
namespace Cohestra::Waves {

    Tracer::Tracer() noexcept = default;

    Tracer::~Tracer() noexcept
    {
        Close();
    }

    bool Tracer::Configure(const Configuration& config) noexcept
    {
        configured = false;

        bool enable = config.GetOrDefault(&CONFIG_WAVEFORM_ENABLE);

#ifdef COHESTRA_V3_WAVEFORM_FORMAT_OFF
        if (enable)
        {
            spdlog::error(
                "Waveform dumping requested (waveform.enable=1), but this build was compiled "
                "with COHESTRA_V3_WAVEFORM_FORMAT=OFF (no trace runtime). "
                "Rebuild with -DCOHESTRA_V3_WAVEFORM_FORMAT=FST or VCD to dump waves.");
            return false;
        }
#endif

        std::string typeValue = config.GetOrDefault(&CONFIG_WAVEFORM_TYPE);

        WaveformType requestedType;
        if (typeValue.empty())
            requestedType = CompiledWaveformType();
        else if (std::optional<WaveformType> parsed = ParseWaveformType(typeValue))
            requestedType = *parsed;
        else
        {
            spdlog::error("Invalid waveform type: {} (expected 'vcd' or 'fst').", typeValue);
            return false;
        }

        if (requestedType != CompiledWaveformType())
        {
            spdlog::error(
                "Waveform type '{}' requested, but this build was compiled with '{}' support only. "
                "Reconfigure with -DCOHESTRA_V3_WAVEFORM_FORMAT={} to switch.",
                WaveformTypeName(requestedType),
                WaveformTypeName(CompiledWaveformType()),
                WaveformTypeCMakeName(requestedType));

            return false;
        }

        type = requestedType;

        path = config.GetOrDefault(&CONFIG_WAVEFORM_PATH);
        if (path.empty())
            path = std::string("cohestra") + WaveformTypeExtension(type);

        startAt = config.GetOrDefault(&CONFIG_WAVEFORM_START);

        configured = enable;
        return true;
    }

    bool Tracer::Open(VerilatedContext& context, int levels) noexcept
    {
        if (!configured)
            return true;

#ifdef COHESTRA_V3_WAVEFORM_FORMAT_OFF
        // Configure() already rejects waveform.enable=1 on OFF builds, so this
        // is unreachable defense-in-depth. The trace-backend body below must
        // stay out of COMPILATION as well: with no trace runtime linked,
        // instantiating WaveFile<VerilatedFstC> fails at link time.
        (void)context;
        (void)levels;
        spdlog::error("Waveform support is compiled out of this build (COHESTRA_V3_WAVEFORM_FORMAT=OFF).");
        return false;
#else
        if (file)
        {
            spdlog::warn("Waveform tracer already open: {}.", path);
            return true;
        }

        // Instantiate only the backend matching the compiled format: the
        // verilation links the trace runtime of the compiled format only, and
        // Configure() has already validated 'type' against it.
#ifdef COHESTRA_V3_WAVEFORM_FORMAT_VCD
        file = std::make_unique<details::WaveFile<VerilatedVcdC>>();
#else
        file = std::make_unique<details::WaveFile<VerilatedFstC>>();
#endif

        context.trace(file->Base(), levels, 0);
        file->Open(path.c_str());

        if (!file->IsOpen())
        {
            spdlog::error("Failed to open waveform file: {}.", path);
            file.reset();
            return false;
        }

        tracing = (startAt == 0);

        spdlog::info("Waveform dumping enabled: type {}, path {}.", WaveformTypeName(type), path);
        if (startAt)
            spdlog::info("Waveform dumping starts at time {}.", startAt);

        return true;
#endif
    }

    void Tracer::Close() noexcept
    {
        if (!file)
            return;

        tracing = false;

        if (file->IsOpen())
        {
            file->Close();
            spdlog::info("Waveform file closed: {}.", path);
        }

        file.reset();
    }

    bool Tracer::IsOpen() const noexcept
    {
        return file && file->IsOpen();
    }

    void Tracer::Start() noexcept
    {
        if (file && file->IsOpen())
            tracing = true;
    }

    void Tracer::Stop() noexcept
    {
        tracing = false;
    }

    bool Tracer::IsTracing() const noexcept
    {
        return tracing && file && file->IsOpen();
    }

    void Tracer::Dump(uint64_t time) noexcept
    {
        if (!tracing && time >= startAt)
            Start();

        if (IsTracing())
            file->Dump(time);
    }
}
