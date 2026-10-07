#pragma once

#include <cstdint>
#include <optional>

#include "../cohestra_axi.hpp"
#include "../cohestra_boot.hpp"

#include "../cohestra_packages/avocado/boot/avocado_script_sequencer.hpp"

#include "spdlog/spdlog.h"


namespace Cohestra {

    /*
    *   AvocadoBootSequencer: Boot-Stage adapter replaying the Avocado boot
    *   script (cohestra_packages/avocado/boot/) through one of the harness's AXI
    *   masters (selected by 'boot.avocado.axi_master', default 0).
    *
    *   The actual write -> B-check -> read-back-verify replay is done by the
    *   duck-typed AvocadoScriptSequencer; this adapter only binds it to the
    *   Boot Host contract and the BootSequencer completion/failure model.
    */
    template<BootHostContract Host>
    class AvocadoBootSequencer : public BootSequencer {
    public:
        static const ConfigurationEntryBack<uint64_t> CONFIG_BOOT_AVOCADO_AXI_MASTER;
        static const ConfigurationEntryBack<uint64_t> CONFIG_BOOT_AVOCADO_WARMUP_CYCLES;

    protected:
        Host*                                           host;
        AXIMaster*                                      master      = nullptr;
        std::optional<AvocadoScriptSequencer<AXIMaster>>  core;
        uint64_t                                        warmupTicks = 0;
        uint64_t                                        warmupStart = 0;
        bool                                            warmupArmed = false;
        bool                                            failLogged  = false;
        bool                                            doneLogged  = false;

    public:
        AvocadoBootSequencer(Host* host) noexcept;

    public:
        bool        Initialize(const StringLoaderBase& configSource) noexcept override;

    public:
        void        Tick(uint64_t time) noexcept override;

    public:
        bool        IsBootComplete() const noexcept override;
        bool        HasBootFailed() const noexcept override;
    };

    // Host-generic factory: any harness whose host context satisfies the
    // Boot Host contract can run Avocado, as long as it exposes an AXI
    // master wired to the mesh config port.
    template<BootHostContract Host>
    class AvocadoBootSequencerFactory : public BootSequencerFactory<Host> {
    public:
        AvocadoBootSequencerFactory() noexcept;

    public:
        std::unique_ptr<BootSequencer> Create(Host& host) noexcept override;
    };
}


// Implementation of: class AvocadoBootSequencer
namespace Cohestra {

    template<BootHostContract Host>
    inline const ConfigurationEntryBack<uint64_t> AvocadoBootSequencer<Host>::CONFIG_BOOT_AVOCADO_AXI_MASTER
        ("BOOT_AVOCADO_AXI_MASTER", "avocado.axi_master", "boot", 0, DeserializeUInt64, SerializeUInt64);

    // Mesh power-up warm-up: the mesh's configuration fabric (CCB FAM
    // broadcast over the PUB network) needs on the order of 20k cycles after
    // reset release before the first access may reach the RN-I config port;
    // an earlier access wedges the PUB fabric and the boot deadlocks. The
    // sequencer therefore idles this many cycles (counted from the boot-stage
    // start, which follows the reset phase) before issuing the first write.
    // Set to 0 to disable (only when the reset phase already provides the
    // warm-up, e.g. a long reset_cycles).
    template<BootHostContract Host>
    inline const ConfigurationEntryBack<uint64_t> AvocadoBootSequencer<Host>::CONFIG_BOOT_AVOCADO_WARMUP_CYCLES
        ("BOOT_AVOCADO_WARMUP_CYCLES", "avocado.warmup_cycles", "boot", 25000, DeserializeUInt64, SerializeUInt64);

    template<BootHostContract Host>
    inline AvocadoBootSequencer<Host>::AvocadoBootSequencer(Host* host) noexcept
        : BootSequencer ("Avocado", "Avocado mesh bring-up over AXI (RN-I config port)")
        , host          (host)
    { }

    template<BootHostContract Host>
    inline bool AvocadoBootSequencer<Host>::Initialize(const StringLoaderBase& configSource) noexcept
    {
        uint64_t masterIndex = CONFIG_BOOT_AVOCADO_AXI_MASTER.defaultValue;
        uint64_t warmupCycles = CONFIG_BOOT_AVOCADO_WARMUP_CYCLES.defaultValue;

        std::optional<std::string> rawValue = configSource.GetRawString(&CONFIG_BOOT_AVOCADO_AXI_MASTER);

        if (rawValue)
        {
            std::optional<uint64_t> value = CONFIG_BOOT_AVOCADO_AXI_MASTER.deserializer(*rawValue);

            if (!value)
                return false;

            masterIndex = *value;
        }

        rawValue = configSource.GetRawString(&CONFIG_BOOT_AVOCADO_WARMUP_CYCLES);

        if (rawValue)
        {
            std::optional<uint64_t> value = CONFIG_BOOT_AVOCADO_WARMUP_CYCLES.deserializer(*rawValue);

            if (!value)
                return false;

            warmupCycles = *value;
        }

        master = host->GetAXIMaster(static_cast<size_t>(masterIndex));

        if (!master)
        {
            spdlog::error("Avocado: no AXI master at index {} ({} master(s) exposed by the harness).",
                masterIndex, host->GetAXIMasterCount());
            return false;
        }

        // times are in half-cycle ticks (2 per cycle)
        warmupTicks = warmupCycles * 2;

        core.emplace(*master, AVOCADO_BOOT_SCRIPT, AVOCADO_BOOT_SCRIPT_COUNT);

        spdlog::info("Avocado: booting through AXI master {} ({} script entries, warm-up {} cycles).",
            masterIndex, core->Total(), warmupCycles);

        return true;
    }

    template<BootHostContract Host>
    inline void AvocadoBootSequencer<Host>::Tick(uint64_t time) noexcept
    {
        if (!core)
            return;

        // hold off the first config access until the mesh power-up window has
        // passed (see CONFIG_BOOT_AVOCADO_WARMUP_CYCLES)
        if (warmupTicks)
        {
            if (!warmupArmed)
            {
                warmupArmed = true;
                warmupStart = time;
                spdlog::info("Avocado: mesh warm-up started ({} cycles).", warmupTicks / 2);
            }

            if (time - warmupStart < warmupTicks)
                return;

            spdlog::info("Avocado: mesh warm-up complete, starting the boot script.");
            warmupTicks = 0;
        }

        core->Tick();

        if (core->Failed() && !failLogged)
        {
            spdlog::error("Avocado: boot failed at script entry {}/{} (state {}).",
                core->Progress(), core->Total(), core->StateName());
            failLogged = true;
        }

        if (core->Done() && !doneLogged)
        {
            spdlog::info("Avocado: boot script completed ({} entries written and verified).",
                core->Total());
            doneLogged = true;
        }
    }

    template<BootHostContract Host>
    inline bool AvocadoBootSequencer<Host>::IsBootComplete() const noexcept
    {
        return core && (core->Done() || core->Failed());
    }

    template<BootHostContract Host>
    inline bool AvocadoBootSequencer<Host>::HasBootFailed() const noexcept
    {
        return core && core->Failed();
    }
}

// Implementation of: class AvocadoBootSequencerFactory
namespace Cohestra {

    template<BootHostContract Host>
    inline AvocadoBootSequencerFactory<Host>::AvocadoBootSequencerFactory() noexcept
        : BootSequencerFactory<Host> ("Avocado", "Avocado mesh bring-up over AXI (RN-I config port)")
    { }

    template<BootHostContract Host>
    inline std::unique_ptr<BootSequencer> AvocadoBootSequencerFactory<Host>::Create(Host& host) noexcept
    {
        return std::make_unique<AvocadoBootSequencer<Host>>(&host);
    }
}
