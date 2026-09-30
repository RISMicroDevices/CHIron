#pragma once

#include "../cohestra_instance.hpp"


namespace Cohestra {

    // Handlers of Taurus upstream node transaction denial events.
    // Each handler dumps the denial context (upstream node, PA, cache state,
    // xaction and the denied flit itself), marks the instance FAILED and
    // pushes the messages into Instance::SetFailed.

    void OnUpstreamXactDeniedEVT(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedEVTEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamXactDeniedSNP(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedSNPEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamXactDeniedREQ(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedREQEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamXactDeniedDnRSP(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedDnRSPEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamXactDeniedUpRSP(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedUpRSPEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamXactDeniedDnDAT(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedDnDATEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamXactDeniedUpDAT(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedUpDATEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamCacheStateMapDeniedRequest(Instance& instance,
        CCHI::Taurus::UpstreamNodeCacheStateMapDeniedRequestEvent<CommonFlitConfigurationType1>& event);

    void OnUpstreamCacheStateMapDeniedResponse(Instance& instance,
        CCHI::Taurus::UpstreamNodeCacheStateMapDeniedResponseEvent<CommonFlitConfigurationType1>& event);

    // Registers all handlers above on the event hub of a Taurus upstream node,
    // each listener onto its matching channel EventBus.
    void RegisterUpstreamXactDeniedEventListeners(Instance& instance,
        CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>::EventHub& events);
}
