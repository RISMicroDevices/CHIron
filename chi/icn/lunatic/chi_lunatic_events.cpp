#include "chi_lunatic_events.hpp"

namespace CHI::Lunatic {

    void EventHub::Clear() noexcept
    {
        OnAcceptedRXREQ.UnregisterAll();
        OnAcceptedRXRSP.UnregisterAll();
        OnAcceptedRXDAT.UnregisterAll();
        OnEmittedTXREQ.UnregisterAll();
        OnEmittedTXRSP.UnregisterAll();
        OnEmittedTXDAT.UnregisterAll();
        OnEmittedTXSNP.UnregisterAll();
        OnDenied.UnregisterAll();
        OnRetired.UnregisterAll();
        OnReset.UnregisterAll();
        OnError.UnregisterAll();
    }

}
