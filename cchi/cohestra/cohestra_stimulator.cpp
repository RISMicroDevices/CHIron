#include "cohestra_stimulator.hpp"

// Implementation of: class Stimulator
namespace Cohestra {

    Stimulator::Stimulator(const std::string&   name, 
                     const std::string&   description,
                     Instance&            instance,
                     size_t               index) noexcept
        : name        (name)
        , description (description)
        , instance    (instance)
        , index       (index)
    { }

    const std::string& Stimulator::GetName() const noexcept
    {
        return name;
    }

    const std::string& Stimulator::GetDescription() const noexcept
    {
        return description;
    }

    Instance& Stimulator::GetOwner() noexcept
    {
        return instance;
    }

    const Instance& Stimulator::GetOwner() const noexcept
    {
        return instance;
    }

    size_t Stimulator::GetIndex() const noexcept
    {
        return index;
    }

    bool Stimulator::IsFinished() const noexcept
    {
        return false;
    }
}

// Implementation of: class StimulatorFactory
namespace Cohestra {

    StimulatorFactory::StimulatorFactory(const std::string& name, const std::string& description) noexcept
        : name        (name)
        , description (description)
    { }

    const std::string& StimulatorFactory::GetName() const noexcept
    {
        return name;
    }

    const std::string& StimulatorFactory::GetDescription() const noexcept
    {
        return description;
    }
}

// Implementation of: class StimulatorRegistry
namespace Cohestra {

    void StimulatorRegistry::Register(std::shared_ptr<StimulatorFactory> factory) noexcept
    {
        if (!factory)
            return;

        factories[factory->GetName()] = std::move(factory);
    }

    void StimulatorRegistry::Unregister(const std::string& name) noexcept
    {
        factories.erase(name);
    }

    void StimulatorRegistry::UnregisterAll() noexcept
    {
        factories.clear();
    }

    size_t StimulatorRegistry::Size() const noexcept
    {
        return factories.size();
    }

    bool StimulatorRegistry::IsEmpty() const noexcept
    {
        return factories.empty();
    }

    bool StimulatorRegistry::Has(const std::string& name) const noexcept
    {
        return factories.find(name) != factories.end();
    }

    std::shared_ptr<StimulatorFactory> StimulatorRegistry::Get(const std::string& name) const noexcept
    {
        auto iter = factories.find(name);

        if (iter != factories.end())
            return iter->second;

        return nullptr;
    }
}
