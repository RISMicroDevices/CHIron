#pragma once

#include <cstdint>
#include <optional>

#include "../cohestra_axi.hpp"
#include "../cohestra_boot.hpp"

#include "../cohestra_packages/cornetto/boot/cornetto_script_sequencer.hpp"

#include "spdlog/spdlog.h"


namespace Cohestra {

    /*
    *   CornettoBootSequencer: Boot-Stage adapter replaying the Cornetto boot
    *   script (cohestra_packages/cornetto/boot/) through one of the harness's AXI
    *   masters (selected by 'boot.cornetto.axi_master', default 0).
    *
    *   The actual write -> B-check -> read-back-verify replay is done by the
    *   duck-typed CornettoScriptSequencer; this adapter only binds it to the
    *   Boot Host contract and the BootSequencer completion/failure model.
    */
    template<BootHostContract Host>
    class CornettoBootSequencer : public BootSequencer {
    public:
        static const ConfigurationEntryBack<uint64_t> CONFIG_BOOT_CORNETTO_AXI_MASTER;
        static const ConfigurationEntryBack<uint64_t> CONFIG_BOOT_CORNETTO_WARMUP_CYCLES;

    protected:
        Host*                                           host;
        AXIMaster*                                      master      = nullptr;
        std::optional<CornettoScriptSequencer<AXIMaster>> core;
        uint64_t                                        warmupTicks = 0;
        uint64_t                                        warmupStart = 0;
        bool                                            warmupArmed = false;
        bool                                            failLogged  = false;
        bool                                            doneLogged  = false;

    public:
        CornettoBootSequencer(Host* host) noexcept;

    public:
        bool        Initialize(const StringLoaderBase& configSource) noexcept override;

    public:
        void        Tick(uint64_t time) noexcept override;

    public:
        bool        IsBootComplete() const noexcept override;
        bool        HasBootFailed() const noexcept override;
    };

    // Host-generic factory: any harness whose host context satisfies the
    // Boot Host contract can run Cornetto, as long as it exposes an AXI
    // master wired to the mesh config port.
    template<BootHostContract Host>
    class CornettoBootSequencerFactory : public BootSequencerFactory<Host> {
    public:
        CornettoBootSequencerFactory() noexcept;

    public:
        std::unique_ptr<BootSequencer> Create(Host& host) noexcept override;
    };
}


// Implementation of: class CornettoBootSequencer
namespace Cohestra {

    template<BootHostContract Host>
    inline const ConfigurationEntryBack<uint64_t> CornettoBootSequencer<Host>::CONFIG_BOOT_CORNETTO_AXI_MASTER
        ("BOOT_CORNETTO_AXI_MASTER", "cornetto.axi_master", "boot", 0, DeserializeUInt64, SerializeUInt64);

    // Mesh power-up warm-up: the mesh's configuration fabric (CCB FAM
    // broadcast over the PUB network) needs on the order of 20k cycles after
    // reset release before the first access may reach the RN-I config port;
    // an earlier access wedges the PUB fabric and the boot deadlocks. The
    // sequencer therefore idles this many cycles (counted from the boot-stage
    // start, which follows the reset phase) before issuing the first write.
    // Set to 0 to disable (only when the reset phase already provides the
    // warm-up, e.g. a long reset_cycles).
    template<BootHostContract Host>
    inline const ConfigurationEntryBack<uint64_t> CornettoBootSequencer<Host>::CONFIG_BOOT_CORNETTO_WARMUP_CYCLES
        ("BOOT_CORNETTO_WARMUP_CYCLES", "cornetto.warmup_cycles", "boot", 25000, DeserializeUInt64, SerializeUInt64);

    template<BootHostContract Host>
    inline CornettoBootSequencer<Host>::CornettoBootSequencer(Host* host) noexcept
        : BootSequencer ("Cornetto", "Cornetto mesh bring-up over AXI (RN-I config port)")
        , host          (host)
    { }

    template<BootHostContract Host>
    inline bool CornettoBootSequencer<Host>::Initialize(const StringLoaderBase& configSource) noexcept
    {
        uint64_t masterIndex = CONFIG_BOOT_CORNETTO_AXI_MASTER.defaultValue;
        uint64_t warmupCycles = CONFIG_BOOT_CORNETTO_WARMUP_CYCLES.defaultValue;

        std::optional<std::string> rawValue = configSource.GetRawString(&CONFIG_BOOT_CORNETTO_AXI_MASTER);

        if (rawValue)
        {
            std::optional<uint64_t> value = CONFIG_BOOT_CORNETTO_AXI_MASTER.deserializer(*rawValue);

            if (!value)
                return false;

            masterIndex = *value;
        }

        rawValue = configSource.GetRawString(&CONFIG_BOOT_CORNETTO_WARMUP_CYCLES);

        if (rawValue)
        {
            std::optional<uint64_t> value = CONFIG_BOOT_CORNETTO_WARMUP_CYCLES.deserializer(*rawValue);

            if (!value)
                return false;

            warmupCycles = *value;
        }

        master = host->GetAXIMaster(static_cast<size_t>(masterIndex));

        if (!master)
        {
            spdlog::error("Cornetto: no AXI master at index {} ({} master(s) exposed by the harness).",
                masterIndex, host->GetAXIMasterCount());
            return false;
        }

        // times are in half-cycle ticks (2 per cycle)
        warmupTicks = warmupCycles * 2;

        core.emplace(*master, CORNETTO_BOOT_SCRIPT, CORNETTO_BOOT_SCRIPT_COUNT);

        spdlog::info("Cornetto: booting through AXI master {} ({} script entries, warm-up {} cycles).",
            masterIndex, core->Total(), warmupCycles);

        return true;
    }

    template<BootHostContract Host>
    inline void CornettoBootSequencer<Host>::Tick(uint64_t time) noexcept
    {
        if (!core)
            return;

        // hold off the first config access until the mesh power-up window has
        // passed (see CONFIG_BOOT_CORNETTO_WARMUP_CYCLES)
        if (warmupTicks)
        {
            if (!warmupArmed)
            {
                warmupArmed = true;
                warmupStart = time;
                spdlog::info("Cornetto: mesh warm-up started ({} cycles).", warmupTicks / 2);
            }

            if (time - warmupStart < warmupTicks)
                return;

            spdlog::info("Cornetto: mesh warm-up complete, starting the boot script.");
            warmupTicks = 0;
        }

        core->Tick();

        if (core->Failed() && !failLogged)
        {
            spdlog::error("Cornetto: boot failed at script entry {}/{} (state {}).",
                core->Progress(), core->Total(), core->StateName());
            failLogged = true;
        }

        if (core->Done() && !doneLogged)
        {
            spdlog::info("Cornetto: boot script completed ({} entries written and verified).",
                core->Total());
            doneLogged = true;
        }
    }

    template<BootHostContract Host>
    inline bool CornettoBootSequencer<Host>::IsBootComplete() const noexcept
    {
        return core && (core->Done() || core->Failed());
    }

    template<BootHostContract Host>
    inline bool CornettoBootSequencer<Host>::HasBootFailed() const noexcept
    {
        return core && core->Failed();
    }
}

// Implementation of: class CornettoBootSequencerFactory
namespace Cohestra {

    template<BootHostContract Host>
    inline CornettoBootSequencerFactory<Host>::CornettoBootSequencerFactory() noexcept
        : BootSequencerFactory<Host> ("Cornetto", "Cornetto mesh bring-up over AXI (RN-I config port)")
    { }

    template<BootHostContract Host>
    inline std::unique_ptr<BootSequencer> CornettoBootSequencerFactory<Host>::Create(Host& host) noexcept
    {
        return std::make_unique<CornettoBootSequencer<Host>>(&host);
    }
}
