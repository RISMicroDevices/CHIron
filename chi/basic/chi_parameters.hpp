#pragma once

#ifndef __CHI__CHI_PARAMETERS
#define __CHI__CHI_PARAMETERS

#include <cstddef>


namespace CHI {

    /*
    Constraint checkers for CHI parameters.
    */

    /*
    * Constraint checker for CHI parameter <NodeID_Width>
    *   -> Legal values of <NodeID_Width> are 7 to 11
    */
    inline static constexpr bool CheckNodeIdWidth(size_t nodeIdWidth) noexcept
    {
        return nodeIdWidth >= 7 && nodeIdWidth <= 11;
    }

    /*
    * Constraint checker for CHI parameter <Req_Addr_Width>
    *   -> Legal values of <Req_Addr_Width> are 44 to 52
    */
    inline static constexpr bool CheckReqAddrWidth(size_t reqAddrWidth) noexcept
    {
        return reqAddrWidth >= 44 && reqAddrWidth <= 52;
    }

    /*
    * Constraint checker for CHI parameter <Data_Width>
    *   -> Legal values of <Data_Width> are 128, 256, and 512
    */
    inline static constexpr bool CheckDataWidth(size_t dataWidth) noexcept
    {
        return dataWidth == 128 || dataWidth == 256 || dataWidth == 512;
    }

    /*
    * Constraint checker for CHI configuration of RSVDC width
    *   -> Legal values are 0 or 4, 8, 12, 16, 24, 32
    */
    inline static constexpr bool CheckRSVDCWidth(size_t rsvdcWidth) noexcept
    {
        return rsvdcWidth == 0 || rsvdcWidth == 4  || rsvdcWidth == 8
                               || rsvdcWidth == 12 || rsvdcWidth == 16
                               || rsvdcWidth == 24 || rsvdcWidth == 32;
    }
}


#endif // __CHI__CHI_PARAMETERS
