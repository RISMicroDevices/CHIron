//*****************************************************************************
// waves_vcs.cpp — implementation of the Cohestra VCS waveform configuration
// & control (see waves_vcs.hpp). Mirrors cohestra_v3/waves.cpp's INI contract
// (same section, same keys, same defaults) with the simulator-side dumping
// substituted for the Verilator trace runtime.
//*****************************************************************************

#include "waves_vcs.hpp"

#include <cctype>

#include "cohestra_vcs_dpi.hpp"

#include "spdlog/spdlog.h"


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

    namespace {

        std::string ToLower(const std::string& value)
        {
            std::string lower;
            lower.reserve(value.size());

            for (char c : value)
                lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

            return lower;
        }

        // The build-default waveform type: fsdb when linked with the Verdi
        // PLI, vcd otherwise.
        constexpr uint32_t DefaultWaveType() noexcept
        {
#if COHESTRA_VCS_HAVE_FSDB
            return VCS::wire::WAVE_FSDB;
#else
            return VCS::wire::WAVE_VCD;
#endif
        }

        const char* WaveTypeName(uint32_t type) noexcept
        {
            switch (type)
            {
                case VCS::wire::WAVE_VCD:  return "vcd";
                case VCS::wire::WAVE_FSDB: return "fsdb";
                default:                   return "unknown";
            }
        }

        const char* WaveTypeExtension(uint32_t type) noexcept
        {
            switch (type)
            {
                case VCS::wire::WAVE_VCD:  return ".vcd";
                case VCS::wire::WAVE_FSDB: return ".fsdb";
                default:                   return "";
            }
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


// Implementation of: class WaveControl
namespace Cohestra::Waves {

    WaveControl::~WaveControl() noexcept
    {
        Close();
    }

    void WaveControl::SetPlusargPaths(std::string vcdPath, std::string fsdbPath) noexcept
    {
        plusargVcd  = std::move(vcdPath);
        plusargFsdb = std::move(fsdbPath);
    }

    bool WaveControl::Configure(const Configuration& config) noexcept
    {
        configured = false;

        bool enable = config.GetOrDefault(&CONFIG_WAVEFORM_ENABLE);

        std::string typeValue = ToLower(config.GetOrDefault(&CONFIG_WAVEFORM_TYPE));

        uint32_t requestedType;
        if (typeValue.empty())
            requestedType = DefaultWaveType();
        else if (typeValue == "vcd")
            requestedType = VCS::wire::WAVE_VCD;
        else if (typeValue == "fsdb")
            requestedType = VCS::wire::WAVE_FSDB;
        else if (typeValue == "fst")
        {
            // FST is a Verilator-only format; the VCS flow dumps VCD or FSDB.
            // A disabled waveform section carrying the stock 'fst' value only
            // warns (it requests nothing); enabling it is a hard error.
            if (enable)
            {
                spdlog::error(
                    "Waveform type 'fst' requested, but FST is Verilator-only; "
                    "the VCS flow supports 'vcd' and 'fsdb'. Set waveform.type accordingly.");
                return false;
            }

            spdlog::warn("waveform.type = fst is not supported by the VCS flow (ignored; waveform.enable = 0).");
            requestedType = DefaultWaveType();
        }
        else
        {
            spdlog::error("Invalid waveform type: {} (expected 'vcd' or 'fsdb').", typeValue);
            return false;
        }

#if !COHESTRA_VCS_HAVE_FSDB
        if (enable && requestedType == VCS::wire::WAVE_FSDB)
        {
            spdlog::error(
                "Waveform type 'fsdb' requested, but this build was not linked with the Verdi PLI "
                "(VERDI_HOME was not set at build.sh time). Rebuild with VERDI_HOME set, or use type = vcd.");
            return false;
        }
#endif

        type = requestedType;

        path = config.GetOrDefault(&CONFIG_WAVEFORM_PATH);
        if (path.empty())
            path = std::string("cohestra") + WaveTypeExtension(type);

        startAt = config.GetOrDefault(&CONFIG_WAVEFORM_START);

        configured = enable;
        return true;
    }

    bool WaveControl::Open() noexcept
    {
        if (opened)
        {
            spdlog::warn("Waveform already open: {}.", path);
            return true;
        }

        uint32_t openType = type;
        std::string openPath = path;

        if (!configured)
        {
            // INI did not enable dumping: fall back to the bridge plusargs.
            if (!plusargFsdb.empty())
            {
#if COHESTRA_VCS_HAVE_FSDB
                openType = VCS::wire::WAVE_FSDB;
                openPath = plusargFsdb;
#else
                spdlog::error(
                    "+cohestra_fsdb={} given, but this build was not linked with the Verdi PLI "
                    "(VERDI_HOME was not set at build.sh time).", plusargFsdb);
                return false;
#endif
            }
            else if (!plusargVcd.empty())
            {
                openType = VCS::wire::WAVE_VCD;
                openPath = plusargVcd;
            }
            else
            {
                spdlog::info("Waveform dumping disabled.");
                return true;
            }

            startAt = 0;
        }

        if (!VCS::OpenWaveform(openType, openPath.c_str(), startAt))
        {
            spdlog::error("Failed to open waveform file (DPI bridge not bound): {}.", openPath);
            return false;
        }

        type = openType;
        path = openPath;
        opened = true;

        spdlog::info("Waveform dumping enabled: type {}, path {}.", WaveTypeName(type), path);
        if (startAt)
            spdlog::info("Waveform dumping starts at time {} ({} ps of simulator time).", startAt, startAt * 500);

        return true;
    }

    void WaveControl::Close() noexcept
    {
        if (!opened)
            return;

        opened = false;
        VCS::CloseWaveform();
        spdlog::info("Waveform file closed: {}.", path);
    }
}
