#pragma once

#include <cstdint>

#include "../cohestra_boot.hpp"


namespace Cohestra {

    /*
    *   DelayBootSequencer: minimal reference boot sequencer.
    *
    *   Uses no host facilities at all; it simply holds the Boot Stage for
    *   'boot.delay.cycles' ticks (one tick per simulation cycle) and then
    *   signals completion. Serves as the end-to-end verification vehicle for
    *   the Boot Stage machinery.
    */
    class DelayBootSequencer : public BootSequencer {
    public:
        static const ConfigurationEntryBack<uint64_t> CONFIG_BOOT_DELAY_CYCLES;

    protected:
        uint64_t    remaining   = 0;

    public:
        DelayBootSequencer() noexcept;

    public:
        bool        Initialize(const StringLoaderBase& configSource) noexcept override;

    public:
        void        Tick(uint64_t time) noexcept override;

    public:
        bool        IsBootComplete() const noexcept override;
    };

    // Host-generic factory: the delay booter touches no host facilities, so
    // it can be instantiated for any harness's host context type that
    // satisfies the Boot Host contract.
    template<BootHostContract Host>
    class DelayBootSequencerFactory : public BootSequencerFactory<Host> {
    public:
        DelayBootSequencerFactory() noexcept;

    public:
        std::unique_ptr<BootSequencer> Create(Host& host) noexcept override;
    };
}


// Implementation of: class DelayBootSequencer
namespace Cohestra {

    inline const ConfigurationEntryBack<uint64_t> DelayBootSequencer::CONFIG_BOOT_DELAY_CYCLES
        ("BOOT_DELAY_CYCLES", "delay.cycles", "boot", 0, DeserializeUInt64, SerializeUInt64);

    inline DelayBootSequencer::DelayBootSequencer() noexcept
        : BootSequencer ("Delay", "Holds the Boot Stage for a fixed number of cycles")
    { }

    inline bool DelayBootSequencer::Initialize(const StringLoaderBase& configSource) noexcept
    {
        std::optional<std::string> rawValue = configSource.GetRawString(&CONFIG_BOOT_DELAY_CYCLES);

        if (!rawValue)
        {
            remaining = CONFIG_BOOT_DELAY_CYCLES.defaultValue;
            return true;
        }

        std::optional<uint64_t> value = CONFIG_BOOT_DELAY_CYCLES.deserializer(*rawValue);

        if (!value)
            return false;

        remaining = *value;
        return true;
    }

    inline void DelayBootSequencer::Tick(uint64_t time) noexcept
    {
        if (remaining)
            --remaining;
    }

    inline bool DelayBootSequencer::IsBootComplete() const noexcept
    {
        return remaining == 0;
    }
}

// Implementation of: class DelayBootSequencerFactory
namespace Cohestra {

    template<BootHostContract Host>
    inline DelayBootSequencerFactory<Host>::DelayBootSequencerFactory() noexcept
        : BootSequencerFactory<Host> ("Delay", "Holds the Boot Stage for a fixed number of cycles")
    { }

    template<BootHostContract Host>
    inline std::unique_ptr<BootSequencer> DelayBootSequencerFactory<Host>::Create(Host& host) noexcept
    {
        return std::make_unique<DelayBootSequencer>();
    }
}
