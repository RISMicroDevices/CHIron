#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>

#include "cohestra_common.hpp"
#include "cohestra_config.hpp"


namespace Cohestra {

    class Stimulator {
    protected:
        const std::string   name;
        const std::string   description;

    protected:
        Instance&           instance;
        const size_t        index;

    public:
        Stimulator(const std::string&   name, 
                   const std::string&   description,
                   Instance&            instance,
                   size_t               index) noexcept;

        virtual ~Stimulator() noexcept = default;

    public:
        virtual bool        Initialize(const StringLoaderBase& configSource) noexcept = 0;

    public:
        virtual void        Tick(uint64_t time) noexcept = 0;

    public:
        // Completion contract for harness completion tracking: a stimulator
        // reports true once its stimulus sequence has run to completion.
        // The default is 'not finished' (e.g. unbounded/background stimulus).
        virtual bool        IsFinished() const noexcept;

    public:
        const std::string&  GetName() const noexcept;
        const std::string&  GetDescription() const noexcept;

    public:
        Instance&           GetOwner() noexcept;
        const Instance&     GetOwner() const noexcept;

        size_t              GetIndex() const noexcept;
    };

    class StimulatorFactory {
    protected:
        std::string     name;
        std::string     description;

    public:
        StimulatorFactory(const std::string& name, const std::string& description) noexcept;
        virtual ~StimulatorFactory() noexcept = default;
        
    public:
        virtual std::unique_ptr<Stimulator> Create(Instance&                    instance,
                                                   size_t                       index,
                                                   const std::vector<uint32_t>& type1Upstreams) noexcept = 0;

    public:
        const std::string& GetName() const noexcept;
        const std::string& GetDescription() const noexcept;
    };

    class StimulatorRegistry {
    protected:
        std::unordered_map<std::string, std::shared_ptr<StimulatorFactory>>
                        factories;

    public:
        void    Register(std::shared_ptr<StimulatorFactory> factory) noexcept;
        void    Unregister(const std::string& name) noexcept;
        void    UnregisterAll() noexcept;

        size_t  Size() const noexcept;
        bool    IsEmpty() const noexcept;

        bool                                Has(const std::string& name) const noexcept;
        std::shared_ptr<StimulatorFactory>  Get(const std::string& name) const noexcept;
    };
}
