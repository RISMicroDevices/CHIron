#pragma once

#ifndef __CCHI__CCHI_PARAMETERS
#define __CCHI__CCHI_PARAMETERS

#include <cstddef>


namespace CCHI {

    /*
    Constraint checkers for CCHI parameters.
    */

    /*
    * Constraint checker for CCHI parameter <TxnID_Width>
    *   -> Legal values of <TxnID_Width> are 4 to 10
    */
    inline static constexpr bool CheckTxnIDWidth(size_t txnIdWidth) noexcept
    {
        return txnIdWidth >= 4 && txnIdWidth <= 10;
    }

    /*
    * Constraint checker for CCHI parameter <DBID_Width>
    *   -> Legal values of <DBID_Width> are 4 to 10
    */
    inline static constexpr bool CheckDBIDWidth(size_t dbIdWidth) noexcept
    {
        return dbIdWidth >= 4 && dbIdWidth <= 10;
    }

    /*
    * Constraint checker for CCHI parameter <UpstreamNodeID_Width>
    *   -> Legal values of <UpstreamNodeID_Width> are 1 to 7
    */
    inline static constexpr bool CheckUpstreamNodeIDWidth(size_t upstreamNodeIDWidth) noexcept
    {
        return upstreamNodeIDWidth >= 1 && upstreamNodeIDWidth <= 7;
    }

    /*
    * Constraint checker for CCHI parameter <DownstreamNodeID_Width>
    *   -> Legal values of <DownstreamNodeID_Width> are 1 to 7
    */
    inline static constexpr bool CheckDownstreamNodeIDWidth(size_t downstreamNodeIDWidth) noexcept
    {
        return downstreamNodeIDWidth >= 1 && downstreamNodeIDWidth <= 7;
    }

    /*
    * Constraint checker for CCHI parameter <WayIndex_Width>
    *   -> Legal values of <WayIndex_Width> are 1 to 5
    */
    inline static constexpr bool CheckWayIndexWidth(size_t wayIndexWidth) noexcept
    {
        return wayIndexWidth >= 1 && wayIndexWidth <= 5;
    }
}



#endif // __CCHI__CCHI_PARAMETERS
