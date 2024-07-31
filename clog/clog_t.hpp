#pragma once

#ifndef __CHI__CLOG_T
#define __CHI__CLOG_T

#include <string>
#include <sstream>
#include <ostream>
#include <istream>
#include <iomanip>
#include <exception>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <initializer_list>
#include <optional>
#include <variant>

#include "clog.hpp"


namespace CLog::CLogT {

    //
    template<class TContext = std::monostate>
    class Parser;

    // CLog.T parsers and executors
    template<class TContext = std::monostate>
    using Executor          = std::function<bool(Parser<TContext>&, TContext&, std::istream&)>;

    template<class TContext>
    class Parser {
    protected:
        TContext                                            context;

        std::unordered_map<std::string, Executor<TContext>> executors;
        size_t                                              executionCounter;

        bool                                                stopOnUnknownToken;
        bool                                                stopOnExecutorFailure;
        bool                                                stopOnSegmentEnd;

        bool                                                segmentMode;
        bool                                                segmentMaskEnable;
        std::string                                         segmentBeginToken;
        std::string                                         segmentEndToken;
        std::unordered_set<std::string>                     segmentMask;

        bool                                                inSegment;
        bool                                                startOfSegment;
        bool                                                endOfSegment;

    public:
        template<class... Targs>
        Parser(Targs... contextConstructorArgs) noexcept;

    public:
        bool                        Parse(std::istream&) noexcept;

    public:
        TContext&                   GetContext() noexcept;
        const TContext&             GetContext() const noexcept;

        bool                        IsStopOnUnknownToken() const noexcept;
        void                        SetStopOnUnknownToken(bool stopOnUnknownToken) noexcept;

        bool                        IsStopOnExecutorFailure() const noexcept;
        void                        SetStopOnExecutorFailure(bool stopOnExecutorFailure) noexcept;

        bool                        IsStopOnSegmentEnd() const noexcept;
        void                        SetStopOnSegmentEnd(bool stopOnSegmentEnd) noexcept;

        size_t                      GetExecutionCounter() const noexcept;
        void                        SetExecutionCounter(size_t executionCounter) noexcept;

        bool                        IsSegmentMode() const noexcept;
        void                        SetSegmentMode(bool segmentMode) noexcept;
        void                        SetSegmentToken(const std::string& segmentBeginToken, const std::string& segmentEndToken) noexcept;
        void                        AddSegmentMask(const std::string& token) noexcept;
        void                        AddSegmentMask(std::initializer_list<std::string> tokens) noexcept;
        void                        EraseSegmentMask(const std::string& token) noexcept;
        void                        EraseSegmentMask(std::initializer_list<std::string> tokens) noexcept;
        void                        ClearSegmentMask() noexcept;

        bool                        IsSegmentMaskEnable() const noexcept;
        void                        SetSegmentMaskEnable(bool enable = true) noexcept;

        bool                        IsInSegment() const noexcept;
        bool                        IsStartOfSegment() const noexcept;
        bool                        IsEndOfSegment() const noexcept;

        void                        SetSegmentBeginToken(const std::string& segmentBeginToken) noexcept;
        std::string                 GetSegmentBeginToken() const noexcept;

        void                        SetSegmentEndToken(const std::string& segmentEndToken) noexcept;
        std::string                 GetSegmentEndToken() const noexcept;

    public:
        void                        RegisterExecutor(const std::string& token, Executor<TContext> executor) noexcept;
        void                        UnregisterExecutor(const std::string& token) noexcept;

    protected:
        virtual std::optional<Executor<TContext>> 
                                    GetExecutor(const std::string& token) noexcept;

    protected:
        virtual void                OnException(const std::string& token, std::exception& exception) noexcept;
        virtual void                OnTokenUnknown(const std::string& unknownToken) noexcept;
        
        virtual void                OnSegmentEnter(const std::string& token) noexcept;
        virtual void                OnSegmentExit(const std::string& token) noexcept;
    };

    /*
    * Spec-defined sentences & tokens for CLog.T
    */
    namespace Sentence {

        /*
        * Terminal Token: Ending a sentence (not necessary).
        */
        //
        namespace TERMINAL {
            static constexpr const char*    Token   = "$";
        }
        //

        /*
        * End Token: Same as Terminal Token
        */
        namespace END {
            static constexpr const char*    Token   = "end";
        }

        /*
        * Comment Token: Comment sentence
        */
        namespace COMMENT {
            static constexpr const char*    Token = "comment";
            //
            namespace Term {
                //
                bool Write(std::ostream& os, const std::string& comment);
                //
                bool Read(std::istream& is, std::string& comment);
            }
        }

        /*
        * CHI Issue: Specifying the CHI Issue Version.
        */
        //
        namespace CHI_ISSUE {
            static constexpr const char*    Token   = "chi.issue";
            //  -> Legal terms: "B"     => Issue B
            //                  "E.b"   => Issue E.b
            namespace Term {
                static constexpr const char* B      = "B";
                static constexpr const char* Eb     = "E.b";
                //
                bool Write(std::ostream& os, Issue issue);
                //
                bool Read(std::istream& is, Issue& issue);
            }
        }
        //

        /*
        * CHI Parameters: Specifying CHI parameters.
        */
        //
        namespace CHI_WIDTH_NODEID {
            static constexpr const char*    Token   = "chi.width.nodeid";
            //  -> Legal terms: <Integer>
            namespace Term {
                //
                bool Write(std::ostream& os, uint64_t nodeIdWidth);
                //
                bool Read(std::istream& is, uint64_t& nodeIdWidth);
            }
        }

        namespace CHI_WIDTH_ADDR {
            static constexpr const char*    Token   = "chi.width.addr";
            //  -> Legal terms: <Integer>
            namespace Term {
                //
                bool Write(std::ostream& os, uint64_t addrWidth);
                //
                bool Read(std::istream& is, uint64_t& addrWidth);
            }
        }

        namespace CHI_WIDTH_RSVDC_REQ {
            static constexpr const char*    Token   = "chi.width.rsvdc.req";
            //  -> Legal terms: <Integer>
            namespace Term {
                //
                bool Write(std::ostream& os, uint64_t reqRsvdcWidth);
                //
                bool Read(std::istream& is, uint64_t& reqRsvdcWidth);
            }
        }

        namespace CHI_WIDTH_RSVDC_DAT {
            static constexpr const char*    Token   = "chi.width.rsvdc.dat";
            //  -> Legal terms: <Integer>
            namespace Term {
                //
                bool Write(std::ostream& os, uint64_t datRsvdcWidth);
                //
                bool Read(std::istream& is, uint64_t& datRsvdcWidth);
            }
        }

        namespace CHI_WIDTH_DATA {
            static constexpr const char*    Token   = "chi.width.data";
            //  -> Legal terms: <Integer>
            namespace Term {
                //
                bool Write(std::ostream& os, uint64_t dataWidth);
                //
                bool Read(std::istream& is, uint64_t& dataWidth);
            }
        }

        namespace CHI_ENABLE_DATACHECK {
            static constexpr const char*    Token   = "chi.enable.datacheck";
            //  -> Legal terms: 0, 1
            namespace Term {
                //
                bool Write(std::ostream& os, bool dataCheckPresent);
                //
                bool Read(std::istream& is, bool& dataCheckPresent);
            }
        }

        namespace CHI_ENABLE_POISON {
            static constexpr const char*    Token   = "chi.enable.poison";
            //  -> Legal terms: 0, 1
            namespace Term {
                //
                bool Write(std::ostream& os, bool poisonPresent);
                //
                bool Read(std::istream& is, bool& poisonPresent);
            }
        }

        namespace CHI_ENABLE_MPAM {
            static constexpr const char*    Token   = "chi.enable.mpam";
            //  -> Legal terms: 0, 1
            namespace Term {
                //
                bool Write(std::ostream& os, bool mpamPresent);
                //
                bool Read(std::istream& is, bool& mpamPresent);
            }
        }
        //

        /*
        * CHI Topology: Specify CHI node information
        */
        //
        namespace CHI_TOPO {
            static constexpr const char*    Token   = "chi.topo";
            //  -> Legal terms: <NodeID: Integer> <NodeType>
            //          -> NodeType: "RNF"      => RN-F
            //                       "RND"      => RN-D
            //                       "RNI"      => RN-I  
            //                       "HNF"      => HN-F
            //                       "HNI"      => HN-I
            //                       "SNF"      => SN-F
            //                       "SNI"      => SN-I
            //                       "MN"       => MN
            namespace Term {
                static constexpr const char* RNF    = "RNF";
                static constexpr const char* RND    = "RND";
                static constexpr const char* RNI    = "RNI";
                static constexpr const char* HNF    = "HNF";
                static constexpr const char* HNI    = "HNI";
                static constexpr const char* SNF    = "SNF";
                static constexpr const char* SNI    = "SNI";
                static constexpr const char* MN     = "MN";
                //
                bool Write(std::ostream& os, uint64_t nodeId, NodeType nodeType);
                //
                bool Read(std::istream& is, uint64_t& nodeId, NodeType& nodeType);
            }
        }
        //

        /*
        * CHI Log: CHI Log entries
        */
        //
        namespace CHI_LOG {
            static constexpr const char*    Token   = "chi.log";
            //  -> Legal terms: <Time: Integer> <NodeID: Integer> <Channel: Channel> <Flit: Hex String>
            //          -> Channel: "TXREQ"     => TXREQ
            //                      "TXRSP"     => TXRSP
            //                      "TXDAT"     => TXDAT
            //                      "TXSNP"     => TXSNP
            //                      "RXREQ"     => RXREQ
            //                      "RXRSP"     => RXRSP
            //                      "RXDAT"     => RXDAT
            //                      "RXSNP"     => RXSNP
            namespace Term {
                static constexpr const char* TXREQ  = "TXREQ";
                static constexpr const char* TXRSP  = "TXRSP";
                static constexpr const char* TXDAT  = "TXDAT";
                static constexpr const char* TXSNP  = "TXSNP";
                static constexpr const char* RXREQ  = "RXREQ";
                static constexpr const char* RXRSP  = "RXRSP";
                static constexpr const char* RXDAT  = "RXDAT";
                static constexpr const char* RXSNP  = "RXSNP";
                //
                bool Write(
                    std::ostream&       os,
                    uint64_t            time,
                    uint64_t            nodeId,
                    Channel             channel,
                    const uint32_t*     flit,
                // --------
                    size_t              flitLength);
                //
                bool Read(
                    std::istream&       is,
                    uint64_t&           time,
                    uint64_t&           nodeId,
                    Channel&            channel,
                    uint32_t*           flit,
                // --------
                    size_t              flitLength,
                // --------
                    bool*               flitOverflow    = nullptr);
            }
        }
        //

        /*
        * Segmental Token: Comment / Ignored Segement
        */
        //
        namespace CLOG_SEGMENT_IGNORE {
            static constexpr const char*    Token   = "$#";
        }
        //

        /*
        * Segmental Token: CHI Parameter Segment
        */
        //
        namespace CLOG_SEGMENT_PARAM_BEGIN {
            static constexpr const char*    Token   = "clog.segment.param.begin";
        }
        //
        namespace CLOG_SEGMENT_PARAM_END {
            static constexpr const char*    Token   = "clog.segment.param.end";
        }
        //

        /*
        * Segmental Token: CHI Topology Information Segement
        */
        //
        namespace CLOG_SEGMENT_TOPO_BEGIN {
            static constexpr const char*    Token   = "clog.segment.topo.begin";
        }
        //
        namespace CLOG_SEGMENT_TOPO_END {
            static constexpr const char*    Token   = "clog.segment.topo.end";
        }
        //
    }
}


// Implementation of: class Parser
namespace CLog::CLogT {
    /*
    TContext                                            context;

    std::unordered_map<std::string, Executor<TContext>> executors;
    size_t                                              executionCounter;

    bool                                                stopOnUnknownToken;
    bool                                                stopOnExecutorFailure;
    bool                                                stopOnSegmentEnd;

    bool                                                segmentMode;
    bool                                                segmentMaskEnable;
    std::string                                         segmentBeginToken;
    std::string                                         segmentEndToken;
    std::unordered_set<std::string>                     segmentExecutorMask;

    bool                                                inSegment;
    bool                                                startOfSegment;
    bool                                                endOfSegment;
    */

    template<class TContext>
    template<class... Targs>
    inline Parser<TContext>::Parser(Targs... contextConstructorArgs) noexcept
        : context               (contextConstructorArgs...)
        , executors             ()
        , executionCounter      (0)
        , stopOnUnknownToken    (false)
        , stopOnExecutorFailure (false)
        , stopOnSegmentEnd      (false)
        , segmentMode           (false)
        , segmentMaskEnable     (false)
        , segmentBeginToken     ()
        , segmentEndToken       ()
        , segmentMask   ()
        , inSegment             (false)
        , startOfSegment        (false)
        , endOfSegment          (false)
    { }

    template<class TContext>
    inline bool Parser<TContext>::Parse(std::istream& is) noexcept
    {
        if (!is)
            return true;

        //
        std::string         term;

        std::ostringstream  sentence;
        Executor<TContext>  executor;
        std::string         executorToken;
        bool                executorAvailable;

        // parse first token
        if (!(is >> term))
            return true;

        if (term.find_first_of('$') == 0)
        {
            term = term.substr(1);

            if (!term.empty())
            {
                if (segmentMode)
                {
                    if (segmentBeginToken.compare(term) == 0)
                    {
                        OnSegmentEnter(term);
                        inSegment = true;

                        auto executor_optional = GetExecutor(term);

                        if (executor_optional)
                        {
                            executor = *executor_optional;
                            executorToken = term;
                            executorAvailable = true;
                        }
                        else
                        {
                            OnTokenUnknown(term);

                            if (stopOnUnknownToken)
                                return false;

                            executorAvailable = false;
                        }
                    }
                    else
                        executorAvailable = false;
                }
                else
                {
                    auto executor_optional = GetExecutor(term);

                    if (executor_optional)
                    {
                        executor = *executor_optional;
                        executorToken = term;
                        executorAvailable = true;
                    }
                    else
                    {
                        OnTokenUnknown(term);

                        if (stopOnUnknownToken)
                            return false;

                        executorAvailable = false;
                    }
                }
            }
            else
                executorAvailable = false;  // implicit end token, both accepted for '$' and '$$'
                                            // while for '$', it wouldn't go to any executor
        }
        else
        {
            // TODO: error info of first token
            //       the first character is required to be '$'
            return false;
        }

        // parse sentences
        while (is >> term)
        {
            if (term.find_first_of('$') == 0)
            {
                // execute previous token
                if (executorAvailable)
                {
                    std::istringstream sentence_terminated(sentence.str());

                    try {
                        if (!executor(*this, context, sentence_terminated) && stopOnExecutorFailure)
                            return false;

                        executionCounter++;

                    } catch (std::exception& e) {
                        OnException(executorToken, e);

                        if (stopOnExecutorFailure)
                            return false;
                    }
                }

                if (endOfSegment && stopOnSegmentEnd)
                    return true;

                startOfSegment = false;
                endOfSegment   = false;

                sentence = std::ostringstream();

                // parse next token
                term = term.substr(1);

                if (!term.empty())
                {
                    if (segmentMode)
                    {
                        if (!inSegment && segmentBeginToken.compare(term) == 0)
                        {
                            OnSegmentEnter(term);
                            inSegment = true;

                            startOfSegment = true;
                        }
                        else if (inSegment && segmentEndToken.compare(term) == 0)
                        {
                            OnSegmentExit(term);
                            inSegment = false;

                            endOfSegment = true;
                        }
                        else if (inSegment)
                            ;
                        else
                        {
                            executorAvailable = false;
                            continue;
                        }
                    }

                    if (segmentMode && inSegment && segmentMaskEnable && !startOfSegment && !endOfSegment)
                    {
                        if (segmentMask.find(term) == segmentMask.end())
                        {
                            executorAvailable = false;
                            continue;
                        }
                    }

                    auto executor_optional = GetExecutor(term);

                    if (executor_optional)
                    {
                        executor = *executor_optional;
                        executorToken = term;
                        executorAvailable = true;
                        continue;
                    }
                    else
                    {
                        OnTokenUnknown(term);

                        if (stopOnUnknownToken)
                            return false;

                        executorAvailable = false;
                        continue;
                    }
                }
                else
                {
                    executorAvailable = false;
                    continue;
                }
            }
            else
            {
                sentence << term << " ";
                continue;
            }
        }

        // execute last token
        if (executorAvailable)
        {
            std::istringstream sentence_terminated(sentence.str());

            try {
                if (!executor(*this, context, sentence_terminated) && stopOnExecutorFailure)
                    return false;

                executionCounter++;
            } catch (std::exception& e) {
                OnException(executorToken, e);

                if (stopOnExecutorFailure)
                    return false;
            }
        }

        return true;
    }

    template<class TContext>
    inline TContext& Parser<TContext>::GetContext() noexcept
    {
        return context;
    }

    template<class TContext>
    inline const TContext& Parser<TContext>::GetContext() const noexcept
    {
        return context;
    }

    template<class TContext>
    inline bool Parser<TContext>::IsStopOnUnknownToken() const noexcept
    {
        return stopOnUnknownToken;
    }

    template<class TContext>
    inline void Parser<TContext>::SetStopOnUnknownToken(bool stopOnUnknownToken) noexcept
    {
        this->stopOnUnknownToken = stopOnUnknownToken;
    }

    template<class TContext>
    inline bool Parser<TContext>::IsStopOnExecutorFailure() const noexcept
    {
        return stopOnExecutorFailure;
    }

    template<class TContext>
    inline void Parser<TContext>::SetStopOnExecutorFailure(bool stopOnExecutorFailure) noexcept
    {
        this->stopOnExecutorFailure = stopOnExecutorFailure;
    }

    template<class TContext>
    inline bool Parser<TContext>::IsStopOnSegmentEnd() const noexcept
    {
        return stopOnSegmentEnd;
    }

    template<class TContext>
    inline void Parser<TContext>::SetStopOnSegmentEnd(bool stopOnSegmentEnd) noexcept
    {
        this->stopOnSegmentEnd = stopOnSegmentEnd;
    }

    template<class TContext>
    inline size_t Parser<TContext>::GetExecutionCounter() const noexcept
    {
        return executionCounter;
    }

    template<class TContext>
    inline void Parser<TContext>::SetExecutionCounter(size_t executionCounter) noexcept
    {
        this->executionCounter = executionCounter;
    }

    template<class TContext>
    inline bool Parser<TContext>::IsSegmentMode() const noexcept
    {
        return segmentMode;
    }

    template<class TContext>
    inline void Parser<TContext>::SetSegmentMode(bool segmentMode) noexcept
    {
        this->segmentMode = segmentMode;
    }

    template<class TContext>
    inline void Parser<TContext>::SetSegmentToken(const std::string& segmentBeginToken, const std::string& segmentEndToken) noexcept
    {
        this->segmentBeginToken = segmentBeginToken;
        this->segmentEndToken   = segmentEndToken;
    }

    template<class TContext>
    inline void Parser<TContext>::AddSegmentMask(const std::string& token) noexcept
    {
        this->segmentMask.insert(token);
    }

    template<class TContext>
    inline void Parser<TContext>::AddSegmentMask(std::initializer_list<std::string> tokens) noexcept
    {
        this->segmentMask.insert(tokens);
    }

    template<class TContext>
    inline void Parser<TContext>::EraseSegmentMask(const std::string& token) noexcept
    {
        this->segmentMask.erase(token);
    }

    template<class TContext>
    inline void Parser<TContext>::EraseSegmentMask(std::initializer_list<std::string> tokens) noexcept
    {
        for (const std::string& token : tokens)
            EraseSegmentMask(token);
    }

    template<class TContext>
    inline void Parser<TContext>::ClearSegmentMask() noexcept
    {
        this->segmentMask.clear();
    }

    template<class TContext>
    inline bool Parser<TContext>::IsSegmentMaskEnable() const noexcept
    {
        return segmentMaskEnable;
    }

    template<class TContext>
    inline void Parser<TContext>::SetSegmentMaskEnable(bool segmentMaskEnable) noexcept
    {
        this->segmentMaskEnable = segmentMaskEnable;
    }

    template<class TContext>
    inline bool Parser<TContext>::IsInSegment() const noexcept
    {
       return inSegment;
    }

    template<class TContext>
    inline bool Parser<TContext>::IsEndOfSegment() const noexcept
    {
        return endOfSegment;
    }

    template<class TContext>
    inline void Parser<TContext>::SetSegmentBeginToken(const std::string& segmentBeginToken) noexcept
    {
        this->segmentBeginToken = segmentBeginToken;
    }

    template<class TContext>
    inline std::string Parser<TContext>::GetSegmentBeginToken() const noexcept
    {
        return segmentBeginToken;
    }

    template<class TContext>
    inline void Parser<TContext>::SetSegmentEndToken(const std::string& segmentEndToken) noexcept
    {
        this->segmentEndToken = segmentEndToken;
    }

    template<class TContext>
    inline std::string Parser<TContext>::GetSegmentEndToken() const noexcept
    {
        return segmentEndToken;
    }

    template<class TContext>
    inline void Parser<TContext>::RegisterExecutor(const std::string& token, Executor<TContext> executor) noexcept
    {
        this->executors[token] = executor;
    }

    template<class TContext>
    inline void Parser<TContext>::UnregisterExecutor(const std::string& token) noexcept
    {
        this->executors.erase(token);
    }

    template<class TContext>
    inline std::optional<Executor<TContext>> Parser<TContext>::GetExecutor(const std::string& token) noexcept
    {
        auto iter = this->executors.find(token);

        if (iter == this->executors.end())
            return std::nullopt;

        return { iter->second };
    }

    template<class TContext>
    inline void Parser<TContext>::OnException(const std::string& token, std::exception& exception) noexcept
    { }

    template<class TContext>
    inline void Parser<TContext>::OnTokenUnknown(const std::string& unknownToken) noexcept
    { }

    template<class TContext>
    inline void Parser<TContext>::OnSegmentEnter(const std::string& token) noexcept
    { }

    template<class TContext>
    inline void Parser<TContext>::OnSegmentExit(const std::string& token) noexcept
    { }
}


// Implementations of Sentences
namespace CLog::CLogT {

    namespace shared_details {

        //
        inline bool WriteUInt64(std::ostream& os, uint64_t val)
        {
            os << val;
            return true;
        }
        //
        inline bool ReadUInt64(std::istream& is, uint64_t& val)
        {
            std::string term;
            is >> term;

            try 
            {
                val = std::stoll(term);
            } 
            catch (std::exception const&) 
            {
                return false;
            }

            return true;
        }
        //

        //
        inline bool WriteBool(std::ostream& os, bool val)
        {
            os << uint64_t(val);
            return true;
        }
        //
        inline bool ReadBool(std::istream& is, bool& val)
        {
            std::string term;
            is >> term;

            if (term.compare("0") == 0)
                val = 0;
            else if (term.compare("1") == 0)
                val = 1;
            else
                return false;

            return true;
        }
        //

        //
        template<class Tk, class Tv>
        inline bool ReadMapped(const std::unordered_map<Tk, Tv>& map, const Tk& key, Tv& val)
        {
            auto res = map.find(key);

            if (res == map.end())
                return false;

            val = res->second;

            return true;
        }

        //
    }

    /*
    * Tokens in CLog.T format
    */
    namespace Sentence {

        /*
        * CHI Issue: Specifying the CHI Issue Version.
        */
        //
        namespace CHI_ISSUE {
            namespace Term {
                //
                inline bool Write(std::ostream& os, Issue issue)
                {
                    switch (issue)
                    {
                        case Issue::B:  os << B;    break;
                        case Issue::Eb: os << Eb;   break;
                        default:        return false;
                    }

                    return true;
                }
                //
                inline bool Read(std::istream& is, Issue& issue)
                {
                    std::string term;
                    is >> term;

                    static const std::unordered_map<std::string, Issue> map = {
                        {B,     Issue::B},
                        {Eb,    Issue::Eb}
                    };

                    return shared_details::ReadMapped(map, term, issue);
                }
            }
        }
        //

        /*
        * CHI Parameters: Specifying CHI parameters.
        */
        //
        namespace CHI_WIDTH_NODEID {
            namespace Term {
                //
                inline bool Write(std::ostream& os, uint64_t nodeIdWidth)
                {
                    return shared_details::WriteUInt64(os, nodeIdWidth);
                }
                //
                inline bool Read(std::istream& is, uint64_t& nodeIdWidth)
                {
                    return shared_details::ReadUInt64(is, nodeIdWidth);
                }
            }
        }

        namespace CHI_WIDTH_ADDR {
            namespace Term {
                //
                inline bool Write(std::ostream& os, uint64_t addrWidth)
                {
                    return shared_details::WriteUInt64(os, addrWidth);
                }
                //
                inline bool Read(std::istream& is, uint64_t& addrWidth)
                {
                    return shared_details::ReadUInt64(is, addrWidth);
                }
            }
        }

        namespace CHI_WIDTH_RSVDC_REQ {
            namespace Term {
                //
                inline bool Write(std::ostream& os, uint64_t reqRsvdcWidth)
                {
                    return shared_details::WriteUInt64(os, reqRsvdcWidth);
                }
                //
                inline bool Read(std::istream& is, uint64_t& reqRsvdcWidth)
                {
                    return shared_details::ReadUInt64(is, reqRsvdcWidth);
                }
            }
        }

        namespace CHI_WIDTH_RSVDC_DAT {
            namespace Term {
                //
                inline bool Write(std::ostream& os, uint64_t datRsvdcWidth)
                {
                    return shared_details::WriteUInt64(os, datRsvdcWidth);
                }
                //
                inline bool Read(std::istream& is, uint64_t& datRsvdcWidth)
                {
                    return shared_details::ReadUInt64(is, datRsvdcWidth);
                }
            }
        }

        namespace CHI_WIDTH_DATA {
            namespace Term {
                //
                inline bool Write(std::ostream& os, uint64_t dataWidth)
                {
                    return shared_details::WriteUInt64(os, dataWidth);
                }
                //
                inline bool Read(std::istream& is, uint64_t& dataWidth)
                {
                    return shared_details::ReadUInt64(is, dataWidth);
                }
            }
        }

        namespace CHI_ENABLE_DATACHECK {
            namespace Term {
                //
                inline bool Write(std::ostream& os, bool dataCheckPresent)
                {
                    return shared_details::WriteBool(os, dataCheckPresent);
                }
                //
                inline bool Read(std::istream& is, bool& dataCheckPresent)
                {
                    return shared_details::ReadBool(is, dataCheckPresent);
                }
            }
        }

        namespace CHI_ENABLE_POISON {
            namespace Term {
                //
                inline bool Write(std::ostream& os, bool poisonPresent)
                {
                    return shared_details::WriteBool(os, poisonPresent);
                }
                //
                inline bool Read(std::istream& is, bool& poisonPresent)
                {
                    return shared_details::ReadBool(is, poisonPresent);
                }
            }
        }

        namespace CHI_ENABLE_MPAM {
            namespace Term {
                //
                inline bool Write(std::ostream& os, bool mpamPresent)
                {
                    return shared_details::WriteBool(os, mpamPresent);
                }
                //
                inline bool Read(std::istream& is, bool& mpamPresent)
                {
                    return shared_details::ReadBool(is, mpamPresent);
                }
            }
        }
        //

        /*
        * CHI Topology: Specify CHI node information
        */
        //
        namespace CHI_TOPO {
            namespace Term {
                //
                inline bool Write(std::ostream& os, uint64_t nodeId, NodeType nodeType)
                {
                    os << nodeId << " ";

                    switch (nodeType)
                    {
                        case NodeType::RN_F:    os << RNF;  break;
                        case NodeType::RN_D:    os << RND;  break;
                        case NodeType::RN_I:    os << RNI;  break;
                        case NodeType::HN_F:    os << HNF;  break;
                        case NodeType::HN_I:    os << HNI;  break;
                        case NodeType::SN_F:    os << SNF;  break;
                        case NodeType::SN_I:    os << SNI;  break;
                        case NodeType::MN:      os << MN;   break;
                        default:                return false;
                    }

                    return true;
                }
                //
                inline bool Read(std::istream& is, uint64_t& nodeId, NodeType& nodeType)
                {
                    if (!shared_details::ReadUInt64(is, nodeId))
                        return false;

                    std::string term;
                    is >> term;

                    static const std::unordered_map<std::string, NodeType> map = {
                        {RNF,   NodeType::RN_F},
                        {RND,   NodeType::RN_D},
                        {RNI,   NodeType::RN_I},
                        {HNF,   NodeType::HN_F},
                        {HNI,   NodeType::HN_I},
                        {SNF,   NodeType::SN_F},
                        {SNI,   NodeType::SN_I},
                        {MN,    NodeType::MN}
                    };

                    return shared_details::ReadMapped(map, term, nodeType);
                }
            }
        }
        //

        /*
        * CHI Log: CHI Log entries
        */
        //
        namespace CHI_LOG {
            namespace Term {
                //
                inline bool Write(
                    std::ostream&       os,
                    uint64_t            time,
                    uint64_t            nodeId,
                    Channel             channel,
                    const uint32_t*     flit,
                // --------
                    size_t              flitLength)
                {
                    os << time   << " ";
                    os << nodeId << " ";

                    switch (channel)
                    {
                        case Channel::TXREQ:    os << TXREQ;    break;
                        case Channel::TXRSP:    os << TXRSP;    break;
                        case Channel::TXDAT:    os << TXDAT;    break;
                        case Channel::TXSNP:    os << TXSNP;    break;
                        case Channel::RXREQ:    os << RXREQ;    break;
                        case Channel::RXRSP:    os << RXRSP;    break;
                        case Channel::RXDAT:    os << RXDAT;    break;
                        case Channel::RXSNP:    os << RXSNP;    break;
                        default:                return false;
                    }

                    os << " ";

                    std::ostringstream oss;
                    for (size_t i = flitLength - 1; ; i--)
                    {
                        oss << std::hex << std::setw(8) << std::setfill('0') << flit[i];

                        if (i == 0)
                            break;
                    }
                    
                    std::string data = oss.str();

                    // remove all leading zeros
                    data = data.substr(data.find_first_not_of('0'));

                    // compensate a zero if empty (all zero)
                    if (data.empty())
                        data = "0";

                    os << data;

                    return true;
                }
                //
                inline bool Read(
                    std::istream&       is,
                    uint64_t&           time,
                    uint64_t&           nodeId,
                    Channel&            channel,
                    uint32_t*           flit,
                // --------
                    size_t              flitLength,
                // --------
                    bool*               flitOverflow)
                {
                    if (!shared_details::ReadUInt64(is, time))
                        return false;
                    
                    if (!shared_details::ReadUInt64(is, nodeId))
                        return false;

                    std::string term;
                    is >> term;

                    static const std::unordered_map<std::string, Channel> map = {
                        {TXREQ, Channel::TXREQ},
                        {TXRSP, Channel::TXRSP},
                        {TXDAT, Channel::TXDAT},
                        {TXSNP, Channel::TXSNP},
                        {RXREQ, Channel::RXREQ},
                        {RXRSP, Channel::RXRSP},
                        {RXDAT, Channel::RXDAT},
                        {RXSNP, Channel::RXSNP}
                    };

                    if (!shared_details::ReadMapped(map, term, channel))
                        return false;

                    //
                    is >> term;
                    std::string curr;

                    int flit_i = 0;
                    while (term.length() > 8)
                    {
                        curr = term.substr(term.length() - 8);
                        term = term.substr(0, term.length() - 8);

                        try {
                            flit[flit_i++] = std::stoll(curr, nullptr, 16);
                        } catch (std::exception&) {
                            return false;
                        }

                        if (flit_i == flitLength)
                        {
                            if (flitOverflow)
                                *flitOverflow = true;

                            return true;
                        }
                    }

                    try {
                        flit[flit_i++] = std::stoll(term, nullptr, 16);
                    } catch (std::exception&) {
                        return false;
                    }

                    for (; flit_i < flitLength; flit_i++)
                        flit[flit_i] = 0;

                    return true;
                }
            }
        }
        //
    }


    //
    inline void WriteCHISentence(
        std::ostream&       os,
        const std::string&  token,
        bool                writeEnd = false)
    {
        os << "$" << token << " ";
        if (writeEnd)
            os << "$" << Sentence::END::Token;
        os << std::endl;
    }

    //
    inline void WriteCLogSegmentParamBegin(
        std::ostream&       os,
        bool                writeEnd = false)
    {
        WriteCHISentence(os, Sentence::CLOG_SEGMENT_PARAM_BEGIN::Token, writeEnd);
    }

    inline void WriteCLogSegmentParamEnd(
        std::ostream&       os,
        bool                writeEnd = false)
    {
        WriteCHISentence(os, Sentence::CLOG_SEGMENT_PARAM_END::Token, writeEnd);
    }

    inline void WriteCLogSegmentTopoBegin(
        std::ostream&       os,
        bool                writeEnd = false)
    {
        WriteCHISentence(os, Sentence::CLOG_SEGMENT_TOPO_BEGIN::Token, writeEnd);
    }

    inline void WriteCLogSegmentTopoEnd(
        std::ostream&       os, 
        bool                writeEnd = false)
    {
        WriteCHISentence(os, Sentence::CLOG_SEGMENT_TOPO_END::Token, writeEnd);
    }

    //
    #define WRITE_CHI_SENTENCE(writeEnd, os, sentence, val...) \
        os << "$" << sentence::Token << " "; \
        if (!sentence::Term::Write(os, val)) \
            return false; \
        os << " "; \
        if (writeEnd) \
            os << "$" << Sentence::END::Token; \
        os << std::endl; \
        return true;

    //
    inline bool WriteCHISentenceComment(
        std::ostream&       os,
        const std::string&  comment,
        bool                writeEnd = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::COMMENT, comment);
    }

    inline bool WriteCHISentenceIssue(
        std::ostream&   os,
        Issue           issue,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_ISSUE, issue);
    }

    inline bool WriteCHISentenceNodeIDWidth(
        std::ostream&   os,
        size_t          nodeIdWidth,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_WIDTH_NODEID, nodeIdWidth);
    }

    inline bool WriteCHISentenceAddrWidth(
        std::ostream&   os,
        size_t          addrWidth,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_WIDTH_ADDR, addrWidth);
    }

    inline bool WriteCHISentenceReqRSVDCWidth(
        std::ostream&   os,
        size_t          reqRsvdcWidth,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_WIDTH_RSVDC_REQ, reqRsvdcWidth);
    }

    inline bool WriteCHISentenceDatRSVDCWidth(
        std::ostream&   os,
        size_t          datRsvdcWidth,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_WIDTH_RSVDC_DAT, datRsvdcWidth);
    }

    inline bool WriteCHISentenceDataWidth(
        std::ostream&   os,
        size_t          dataWith,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_WIDTH_DATA, dataWith);
    }

    inline bool WriteCHISentenceDataCheckPresent(
        std::ostream&   os,
        bool            dataCheckPresent,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_ENABLE_DATACHECK, dataCheckPresent);
    }

    inline bool WriteCHISentencePoisonPresent(
        std::ostream&   os,
        bool            poisonPresent,
        bool            writeEnd            = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_ENABLE_POISON, poisonPresent);
    }

    inline bool WriteCHISentenceMPAMPresent(
        std::ostream&   os,
        bool            mpamPresent,
        bool            writeEnd                = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_ENABLE_MPAM, mpamPresent);
    }

    inline bool WriteCHISentenceTopo(
        std::ostream&   os,
        uint64_t        nodeId,
        NodeType        nodeType,
        bool            writeEnd                = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_TOPO, nodeId, nodeType);
    }

    inline bool WriteCHISentenceLog(
        std::ostream&   os,
        uint64_t        time,
        uint64_t        nodeId,
        Channel         channel,
        uint32_t*       flit,
        size_t          flitLength,
        bool            writeEnd                = false)
    {
        WRITE_CHI_SENTENCE(writeEnd, os, Sentence::CHI_LOG, time, nodeId, channel, flit, flitLength);
    }

    #undef WriteCHISentence
    //

}

#endif
