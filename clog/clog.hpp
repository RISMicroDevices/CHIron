#pragma once

#ifndef __CHI__CLOG
#define __CHI__CLOG

#include <cstdint>                          // IWYU pragma: keep

#include "../chi/basic/chi_parameters.hpp"

namespace CLog {

    /*
    * Enumeration of CHI Issue versions
    */
    enum class Issue {
        B       = 0,
        Eb      = 3
    };

    /*
    * Enumeration of CHI standard node types
    */
    enum class NodeType {
        RN_F    = 1,
        RN_D    = 2,
        RN_I    = 3,
        HN_F    = 5,
        HN_I    = 7,
        SN_F    = 9,
        SN_I    = 11,
        MN      = 12
    };

    /*
    * Enumeration of CHI channels
    */
    enum class Channel {
        TXREQ   = 0,
        TXRSP   = 1,
        TXDAT   = 2,
        TXSNP   = 3,

        RXREQ   = 4,
        RXRSP   = 5,
        RXDAT   = 6,
        RXSNP   = 7,
    };

    /*
    * CHI parameter container
    */
    class Parameters {
    protected:
        size_t      nodeIdWidth;
        size_t      reqAddrWidth;
        size_t      reqRsvdcWidth;
        size_t      datRsvdcWidth;
        size_t      dataWidth;
        bool        dataCheckPresent;
        bool        poisonPresent;
        bool        mpamPresent;

    public:
        Parameters() noexcept;

    public:
        bool        SetNodeIdWidth(size_t nodeIdWidth) noexcept;
        bool        SetReqAddrWidth(size_t reqAddrWidth) noexcept;
        bool        SetReqRSVDCWidth(size_t reqRsvdcWidth) noexcept;
        bool        SetDatRSVDCWidth(size_t datRsvdcWidth) noexcept;
        bool        SetDataWidth(size_t dataWidth) noexcept;
        void        SetDataCheckPresent(bool dataCheckPresent) noexcept;
        void        SetPoisonPresent(bool poisonPresent) noexcept;
        void        SetMPAMPresent(bool mpamPresent) noexcept;

    public:
        size_t      GetNodeIdWidth() const noexcept;
        size_t      GetReqAddrWidth() const noexcept;
        size_t      GetReqRSVDCWidth() const noexcept;
        size_t      GetDatRSVDCWidth() const noexcept;
        size_t      GetDataWidth() const noexcept;
        bool        IsDataCheckPresent() const noexcept;
        bool        IsPoisonPresent() const noexcept;
        bool        IsMPAMPresent() const noexcept;
    };
}


// Implementation of: class Parameters
namespace CLog {
    /*
    size_t      nodeIdWidth;
    size_t      reqAddrWidth;
    size_t      reqRsvdcWidth;
    size_t      datRsvdcWidth;
    size_t      dataWidth;
    bool        dataCheckPresent;
    bool        poisonPresent;
    bool        mpamPresent;
    */

    inline bool Parameters::SetNodeIdWidth(size_t nodeIdWidth) noexcept
    {
        if (!CHI::CheckNodeIdWidth(nodeIdWidth))
            return false;

        this->nodeIdWidth = nodeIdWidth;

        return true;
    }

    inline bool Parameters::SetReqAddrWidth(size_t reqAddrWidth) noexcept
    {
        if (!CHI::CheckReqAddrWidth(reqAddrWidth))
            return false;

        this->reqAddrWidth = reqAddrWidth;

        return true;
    }

    inline bool Parameters::SetReqRSVDCWidth(size_t reqRsvdcWidth) noexcept
    {
        if (!CHI::CheckRSVDCWidth(reqRsvdcWidth))
            return false;

        this->reqRsvdcWidth = reqRsvdcWidth;

        return true;
    }

    inline bool Parameters::SetDatRSVDCWidth(size_t datRsvdcWidth) noexcept
    {
        if (!CHI::CheckRSVDCWidth(datRsvdcWidth))
            return false;

        this->datRsvdcWidth = datRsvdcWidth;

        return true;
    }

    inline bool Parameters::SetDataWidth(size_t dataWidth) noexcept
    {
        if (!CHI::CheckDataWidth(dataWidth))
            return false;

        this->dataWidth = dataWidth;

        return true;
    }

    inline void Parameters::SetDataCheckPresent(bool dataCheckPresent) noexcept
    {
        this->dataCheckPresent = dataCheckPresent;
    }

    inline void Parameters::SetPoisonPresent(bool poisonPresent) noexcept
    {
        this->poisonPresent = poisonPresent;
    }
    
    inline void Parameters::SetMPAMPresent(bool mpamPresent) noexcept
    {
        this->mpamPresent = mpamPresent;
    }

    inline size_t Parameters::GetNodeIdWidth() const noexcept
    {
        return nodeIdWidth;
    }

    inline size_t Parameters::GetReqAddrWidth() const noexcept
    {
        return reqAddrWidth;
    }

    inline size_t Parameters::GetReqRSVDCWidth() const noexcept
    {
        return reqRsvdcWidth;
    }

    inline size_t Parameters::GetDatRSVDCWidth() const noexcept
    {
        return datRsvdcWidth;
    }

    inline size_t Parameters::GetDataWidth() const noexcept
    {
        return dataWidth;
    }

    inline bool Parameters::IsDataCheckPresent() const noexcept
    {
        return dataCheckPresent;
    }

    inline bool Parameters::IsPoisonPresent() const noexcept
    {
        return poisonPresent;
    }

    inline bool Parameters::IsMPAMPresent() const noexcept
    {
        return mpamPresent;
    }
}


#endif // __CHI__CLOG
