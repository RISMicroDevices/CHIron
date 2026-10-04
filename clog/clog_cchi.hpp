#pragma once

#ifndef __CHI__CLOG_CCHI
#define __CHI__CLOG_CCHI

#include <cstdint>                          // IWYU pragma: keep
#include <string>

//#define CLOG_STANDALONE

#ifndef CLOG_STANDALONE
#   include "../cchi/basic/cchi_parameters.hpp"
#endif


namespace CLog::CCHI {

    /*
    * Enumeration of CCHI component types
    *   -> values mirror CCHI::ComponentType (cchi/basic/cchi_components.hpp)
    */
    enum class NodeType {
        Type1   = 1,
        Type2   = 2,
        Type3   = 3,
        Type4   = 4,
        Type5   = 5,
        Home    = 6
    };

    std::string NodeTypeToString(NodeType type) noexcept;

    /*
    * Enumeration of CCHI channels
    */
    enum class Channel {
        TXEVT   = 0,
        TXREQ   = 1,
        TXSNP   = 2,
        TXDNRSP = 3,
        TXUPRSP = 4,
        TXDNDAT = 5,
        TXUPDAT = 6,

        RXEVT   = 7,
        RXREQ   = 8,
        RXSNP   = 9,
        RXDNRSP = 10,
        RXUPRSP = 11,
        RXDNDAT = 12,
        RXUPDAT = 13
    };

    std::string ChannelToString(Channel channel) noexcept;

    /*
    * CCHI parameter container
    *   -> mirrors CCHI::FlitConfiguration (cchi/spec/cchi_protocol_flits.hpp)
    */
    class Parameters {
    protected:
        NodeType    componentType;
        size_t      txnIdWidth;
        size_t      dbIdWidth;
        size_t      upstreamNodeIdWidth;
        size_t      downstreamNodeIdWidth;
        size_t      wayIndexWidth;
        size_t      dataWidth;
        size_t      tagAliasWidth;
        bool        uwPersistEnable;
        bool        uwPredictEnable;

    public:
        Parameters() noexcept;

    public:
        void        SetComponentType(NodeType componentType) noexcept;
        bool        SetTxnIdWidth(size_t txnIdWidth) noexcept;
        bool        SetDBIdWidth(size_t dbIdWidth) noexcept;
        bool        SetUpstreamNodeIdWidth(size_t upstreamNodeIdWidth) noexcept;
        bool        SetDownstreamNodeIdWidth(size_t downstreamNodeIdWidth) noexcept;
        bool        SetWayIndexWidth(size_t wayIndexWidth) noexcept;
        bool        SetDataWidth(size_t dataWidth) noexcept;
        bool        SetTagAliasWidth(size_t tagAliasWidth) noexcept;
        void        SetUWPersistEnable(bool uwPersistEnable) noexcept;
        void        SetUWPredictEnable(bool uwPredictEnable) noexcept;

    public:
        NodeType    GetComponentType() const noexcept;
        size_t      GetTxnIdWidth() const noexcept;
        size_t      GetDBIdWidth() const noexcept;
        size_t      GetUpstreamNodeIdWidth() const noexcept;
        size_t      GetDownstreamNodeIdWidth() const noexcept;
        size_t      GetWayIndexWidth() const noexcept;
        size_t      GetDataWidth() const noexcept;
        size_t      GetTagAliasWidth() const noexcept;
        bool        IsUWPersistEnabled() const noexcept;
        bool        IsUWPredictEnabled() const noexcept;
    };
}


// Implementation of: class Parameters
namespace CLog::CCHI {
    /*
    NodeType    componentType;
    size_t      txnIdWidth;
    size_t      dbIdWidth;
    size_t      upstreamNodeIdWidth;
    size_t      downstreamNodeIdWidth;
    size_t      wayIndexWidth;
    size_t      dataWidth;
    size_t      tagAliasWidth;
    bool        uwPersistEnable;
    bool        uwPredictEnable;
    */

    inline Parameters::Parameters() noexcept
        : componentType         (NodeType::Type1)
        , txnIdWidth            (7)
        , dbIdWidth             (8)
        , upstreamNodeIdWidth   (5)
        , downstreamNodeIdWidth (5)
        , wayIndexWidth         (4)
        , dataWidth             (256)
        , tagAliasWidth         (8)
        , uwPersistEnable       (true)
        , uwPredictEnable       (true)
    { }

    inline void Parameters::SetComponentType(NodeType componentType) noexcept
    {
        this->componentType = componentType;
    }

    inline bool Parameters::SetTxnIdWidth(size_t txnIdWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!::CCHI::CheckTxnIDWidth(txnIdWidth))
            return false;
#endif

        this->txnIdWidth = txnIdWidth;

        return true;
    }

    inline bool Parameters::SetDBIdWidth(size_t dbIdWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!::CCHI::CheckDBIDWidth(dbIdWidth))
            return false;
#endif

        this->dbIdWidth = dbIdWidth;

        return true;
    }

    inline bool Parameters::SetUpstreamNodeIdWidth(size_t upstreamNodeIdWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!::CCHI::CheckUpstreamNodeIDWidth(upstreamNodeIdWidth))
            return false;
#endif

        this->upstreamNodeIdWidth = upstreamNodeIdWidth;

        return true;
    }

    inline bool Parameters::SetDownstreamNodeIdWidth(size_t downstreamNodeIdWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!::CCHI::CheckDownstreamNodeIDWidth(downstreamNodeIdWidth))
            return false;
#endif

        this->downstreamNodeIdWidth = downstreamNodeIdWidth;

        return true;
    }

    inline bool Parameters::SetWayIndexWidth(size_t wayIndexWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!::CCHI::CheckWayIndexWidth(wayIndexWidth))
            return false;
#endif

        this->wayIndexWidth = wayIndexWidth;

        return true;
    }

    inline bool Parameters::SetDataWidth(size_t dataWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!::CCHI::CheckDataWidth(dataWidth))
            return false;
#endif

        this->dataWidth = dataWidth;

        return true;
    }

    inline bool Parameters::SetTagAliasWidth(size_t tagAliasWidth) noexcept
    {
#ifndef CLOG_STANDALONE
        if (!::CCHI::CheckTagAliasWidth(tagAliasWidth))
            return false;
#endif

        this->tagAliasWidth = tagAliasWidth;

        return true;
    }

    inline void Parameters::SetUWPersistEnable(bool uwPersistEnable) noexcept
    {
        this->uwPersistEnable = uwPersistEnable;
    }

    inline void Parameters::SetUWPredictEnable(bool uwPredictEnable) noexcept
    {
        this->uwPredictEnable = uwPredictEnable;
    }

    inline NodeType Parameters::GetComponentType() const noexcept
    {
        return componentType;
    }

    inline size_t Parameters::GetTxnIdWidth() const noexcept
    {
        return txnIdWidth;
    }

    inline size_t Parameters::GetDBIdWidth() const noexcept
    {
        return dbIdWidth;
    }

    inline size_t Parameters::GetUpstreamNodeIdWidth() const noexcept
    {
        return upstreamNodeIdWidth;
    }

    inline size_t Parameters::GetDownstreamNodeIdWidth() const noexcept
    {
        return downstreamNodeIdWidth;
    }

    inline size_t Parameters::GetWayIndexWidth() const noexcept
    {
        return wayIndexWidth;
    }

    inline size_t Parameters::GetDataWidth() const noexcept
    {
        return dataWidth;
    }

    inline size_t Parameters::GetTagAliasWidth() const noexcept
    {
        return tagAliasWidth;
    }

    inline bool Parameters::IsUWPersistEnabled() const noexcept
    {
        return uwPersistEnable;
    }

    inline bool Parameters::IsUWPredictEnabled() const noexcept
    {
        return uwPredictEnable;
    }
}


// Implementation of: NodeTypeToString
namespace CLog::CCHI {
    inline std::string NodeTypeToString(NodeType type) noexcept
    {
        switch (type)
        {
            case NodeType::Type1:   return "Type1";
            case NodeType::Type2:   return "Type2";
            case NodeType::Type3:   return "Type3";
            case NodeType::Type4:   return "Type4";
            case NodeType::Type5:   return "Type5";
            case NodeType::Home:    return "Home";
            default:                return "";
        }
    }
}


// Implementation of: ChannelToString
namespace CLog::CCHI {
    inline std::string ChannelToString(Channel channel) noexcept
    {
        switch (channel)
        {
            case Channel::TXEVT:    return "TXEVT";
            case Channel::TXREQ:    return "TXREQ";
            case Channel::TXSNP:    return "TXSNP";
            case Channel::TXDNRSP:  return "TXDNRSP";
            case Channel::TXUPRSP:  return "TXUPRSP";
            case Channel::TXDNDAT:  return "TXDNDAT";
            case Channel::TXUPDAT:  return "TXUPDAT";
            case Channel::RXEVT:    return "RXEVT";
            case Channel::RXREQ:    return "RXREQ";
            case Channel::RXSNP:    return "RXSNP";
            case Channel::RXDNRSP:  return "RXDNRSP";
            case Channel::RXUPRSP:  return "RXUPRSP";
            case Channel::RXDNDAT:  return "RXDNDAT";
            case Channel::RXUPDAT:  return "RXUPDAT";
            default:                return "";
        }
    }
}


#endif // __CHI__CLOG_CCHI
