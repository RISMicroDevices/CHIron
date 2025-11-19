#pragma once

#include "common.hpp"       // IWYU pragma: keep

#include <vector>


void TCPtHRespAtomic(
    size_t*                     totalCount,
    size_t*                     errCountFail,
    size_t*                     errCountEnvError,
    std::vector<std::string>*   errList,
    const Xact::Topology&       topo
) noexcept;
