#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "cohestra_common.hpp"
#include "cohestra_config.hpp"
#include "cohestra_interface.hpp"

#include "../icn/taurus/cchi_taurus_component_afx.hpp"


namespace Cohestra {

    class Instance;
    class AXIMaster;

    /*
    *   Boot Host contract: the facilities a harness must expose to boot
    *   sequencers. Composed of three providers; a boot sequencer is free to
    *   ignore the parts it does not need.
    *
    *   - Instance:      the CCHI instance (state, configuration).
    *   - CCHI Upstream: the Taurus upstream nodes (CCHI master side).
    *   - AXI Master:    the AXI master models.
    */
    template<class Host>
    concept BootHostInstanceProvider = requires(Host& host) {
        { host.GetInstance() } -> std::convertible_to<Instance*>;
    };

    template<class Host>
    concept BootHostCCHIUpstreamProvider = requires(Host& host, size_t index) {
        { host.GetCCHIUpstreamCount() } -> std::convertible_to<size_t>;
        { host.GetCCHIUpstream(index) } -> std::convertible_to<CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*>;
    };

    template<class Host>
    concept BootHostAXIMasterProvider = requires(Host& host, size_t index) {
        { host.GetAXIMasterCount() } -> std::convertible_to<size_t>;
        { host.GetAXIMaster(index) } -> std::convertible_to<AXIMaster*>;
    };

    template<class Host>
    concept BootHostContract =
            BootHostInstanceProvider<Host>
        &&  BootHostCCHIUpstreamProvider<Host>
        &&  BootHostAXIMasterProvider<Host>;

    /*
    *   BootSequencer: harness-owned driver of the Boot Stage, the run phase
    *   between the reset stage and the test stimulation stage.
    *
    *   The base class is deliberately bus-agnostic: a boot sequencer is free
    *   to sequence through the CCHI master side, AXI masters, CHI interfaces
    *   or anything else the harness exposes. Concrete boot sequencers capture
    *   whatever they need from the typed host context handed to their
    *   factory's Create().
    *
    *   The Boot Stage ends only when the boot sequencer signals completion
    *   through IsBootComplete(). When no boot sequencer is registered, the
    *   Boot Stage is skipped entirely.
    */
    class BootSequencer {
    protected:
        const std::string   name;
        const std::string   description;

    public:
        BootSequencer(const std::string&    name,
                      const std::string&    description) noexcept;

        virtual ~BootSequencer() noexcept = default;

    public:
        virtual bool        Initialize(const StringLoaderBase& configSource) noexcept = 0;

    public:
        virtual void        Tick(uint64_t time) noexcept = 0;

    public:
        // Completion contract for the Boot Stage: the harness keeps ticking
        // the boot sequencer until this reports true. Pure virtual on
        // purpose - signaling the end of the Boot Stage is the sequencer's
        // responsibility.
        virtual bool        IsBootComplete() const noexcept = 0;

    public:
        const std::string&  GetName() const noexcept;
        const std::string&  GetDescription() const noexcept;
    };

    template<BootHostContract Host>
    class BootSequencerFactory {
    protected:
        std::string     name;
        std::string     description;

    public:
        BootSequencerFactory(const std::string& name, const std::string& description) noexcept;
        virtual ~BootSequencerFactory() noexcept = default;

    public:
        virtual std::unique_ptr<BootSequencer> Create(Host& host) noexcept = 0;

    public:
        const std::string& GetName() const noexcept;
        const std::string& GetDescription() const noexcept;
    };

    template<BootHostContract Host>
    class BootSequencerRegistry {
    protected:
        std::unordered_map<std::string, std::shared_ptr<BootSequencerFactory<Host>>>
                        factories;

    public:
        void    Register(std::shared_ptr<BootSequencerFactory<Host>> factory) noexcept;
        void    Unregister(const std::string& name) noexcept;
        void    UnregisterAll() noexcept;

        size_t  Size() const noexcept;
        bool    IsEmpty() const noexcept;

        bool                                        Has(const std::string& name) const noexcept;
        std::shared_ptr<BootSequencerFactory<Host>> Get(const std::string& name) const noexcept;
    };
}


// Implementation of: class BootSequencerFactory
namespace Cohestra {

    template<BootHostContract Host>
    inline BootSequencerFactory<Host>::BootSequencerFactory(const std::string& name, const std::string& description) noexcept
        : name        (name)
        , description (description)
    { }

    template<BootHostContract Host>
    inline const std::string& BootSequencerFactory<Host>::GetName() const noexcept
    {
        return name;
    }

    template<BootHostContract Host>
    inline const std::string& BootSequencerFactory<Host>::GetDescription() const noexcept
    {
        return description;
    }
}

// Implementation of: class BootSequencerRegistry
namespace Cohestra {

    template<BootHostContract Host>
    inline void BootSequencerRegistry<Host>::Register(std::shared_ptr<BootSequencerFactory<Host>> factory) noexcept
    {
        if (!factory)
            return;

        factories[factory->GetName()] = std::move(factory);
    }

    template<BootHostContract Host>
    inline void BootSequencerRegistry<Host>::Unregister(const std::string& name) noexcept
    {
        factories.erase(name);
    }

    template<BootHostContract Host>
    inline void BootSequencerRegistry<Host>::UnregisterAll() noexcept
    {
        factories.clear();
    }

    template<BootHostContract Host>
    inline size_t BootSequencerRegistry<Host>::Size() const noexcept
    {
        return factories.size();
    }

    template<BootHostContract Host>
    inline bool BootSequencerRegistry<Host>::IsEmpty() const noexcept
    {
        return factories.empty();
    }

    template<BootHostContract Host>
    inline bool BootSequencerRegistry<Host>::Has(const std::string& name) const noexcept
    {
        return factories.find(name) != factories.end();
    }

    template<BootHostContract Host>
    inline std::shared_ptr<BootSequencerFactory<Host>> BootSequencerRegistry<Host>::Get(const std::string& name) const noexcept
    {
        auto iter = factories.find(name);

        if (iter != factories.end())
            return iter->second;

        return nullptr;
    }
}
