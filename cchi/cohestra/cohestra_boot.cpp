#include "cohestra_boot.hpp"


// Implementation of: class BootSequencer
namespace Cohestra {

    BootSequencer::BootSequencer(const std::string& name,
                                 const std::string& description) noexcept
        : name        (name)
        , description (description)
    { }

    const std::string& BootSequencer::GetName() const noexcept
    {
        return name;
    }

    const std::string& BootSequencer::GetDescription() const noexcept
    {
        return description;
    }
}
