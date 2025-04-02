#pragma once

#ifndef __CHI__CLOG
#define __CHI__CLOG

#include <cstdint>                          // IWYU pragma: keep
#include <string>

//#define CLOG_STANDALONE

#ifndef CLOG_STANDALONE
#   include "../chi/basic/chi_parameters.hpp"
#endif


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

    std::string NodeTypeToString(NodeType type) noexcept;

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
        Issue       issue;
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
        void        SetIssue(Issue issue) noexcept;
        bool        SetNodeIdWidth(size_t nodeIdWidth) noexcept;
        bool        SetReqAddrWidth(size_t reqAddrWidth) noexcept;
        bool        SetReqRSVDCWidth(size_t reqRsvdcWidth) noexcept;
        bool        SetDatRSVDCWidth(size_t datRsvdcWidth) noexcept;
        bool        SetDataWidth(size_t dataWidth) noexcept;
        void        SetDataCheckPresent(bool dataCheckPresent) noexcept;
        void        SetPoisonPresent(bool poisonPresent) noexcept;
        void        SetMPAMPresent(bool mpamPresent) noexcept;

    public:
        Issue       GetIssue() const noexcept;
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
    Issue       issue;
    size_t      nodeIdWidth;
    size_t      reqAddrWidth;
    size_t      reqRsvdcWidth;
    size_t      datRsvdcWidth;
    size_t      dataWidth;
    bool        dataCheckPresent;
    bool        poisonPresent;
    bool        mpamPresent;
    */

    inline Parameters::Parameters() noexcept
        : issue             (Issue::B)
        , nodeIdWidth       (7)
        , reqAddrWidth      (44)
        , reqRsvdcWidth     (0)
        , datRsvdcWidth     (0)
        , dataWidth         (128)
        , dataCheckPresent  (false)
        , poisonPresent     (false)
        , mpamPresent       (false)
    { }

    inline void Parameters::SetIssue(Issue issue) noexcept
    {
        this->issue = issue;
    }

    inline bool Parameters::SetNodeIdWidth(size_t nodeIdWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!CHI::CheckNodeIdWidth(nodeIdWidth))
            return false;
#endif

        this->nodeIdWidth = nodeIdWidth;

        return true;
    }

    inline bool Parameters::SetReqAddrWidth(size_t reqAddrWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!CHI::CheckReqAddrWidth(reqAddrWidth))
            return false;
#endif

        this->reqAddrWidth = reqAddrWidth;

        return true;
    }

    inline bool Parameters::SetReqRSVDCWidth(size_t reqRsvdcWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!CHI::CheckRSVDCWidth(reqRsvdcWidth))
            return false;
#endif

        this->reqRsvdcWidth = reqRsvdcWidth;

        return true;
    }

    inline bool Parameters::SetDatRSVDCWidth(size_t datRsvdcWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!CHI::CheckRSVDCWidth(datRsvdcWidth))
            return false;
#endif

        this->datRsvdcWidth = datRsvdcWidth;

        return true;
    }

    inline bool Parameters::SetDataWidth(size_t dataWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!CHI::CheckDataWidth(dataWidth))
            return false;
#endif

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

    inline Issue Parameters::GetIssue() const noexcept
    {
        return issue;
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


// Implementation of: NodeTypeToString
namespace CLog {
    inline std::string NodeTypeToString(NodeType type) noexcept
    {
        switch (type)
        {
            case NodeType::RN_F:    return "RN-F";
            case NodeType::RN_D:    return "RN-D";
            case NodeType::RN_I:    return "RN-I";
            case NodeType::HN_F:    return "HN-F";
            case NodeType::HN_I:    return "HN-I";
            case NodeType::SN_F:    return "SN-F";
            case NodeType::SN_I:    return "SN-I";
            case NodeType::MN:      return "MN";
            default:                return "";
        }
    }
}


#endif // __CHI__CLOG
