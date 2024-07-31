#pragma once

#ifndef __CHI__CLOG_T_UTIL
#define __CHI__CLOG_T_UTIL

#include "clog_t.hpp"

#include <variant>
#include <istream>
#include <functional>


namespace CLog::CLogT {
    
    template<class TContext = std::monostate>
    class ParametersSerDes {
    private:
        Parameters* params;

    public:
        Parameters*         GetParametersReference() noexcept;
        const Parameters*   GetParametersReference() const noexcept;
        void                SetParametersReference(Parameters* params) noexcept;

    public:
        void                SerializeTo(std::ostream&, bool segmentWrap = false) const;

    public:
        void                RegisterAsDeserializer(Parser<TContext>& parser) noexcept;
        void                Unregister(Parser<TContext>& parser) const noexcept;

        void                ApplySegmentToken(Parser<TContext>& parser) const noexcept;
        void                ApplySegmentMask(Parser<TContext>& parser) const noexcept;

    protected:
        virtual bool        OnCHI_ISSUE             (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_WIDTH_NODEID      (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_WIDTH_ADDR        (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_WIDTH_RSVDC_REQ   (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_WIDTH_RSVDC_DAT   (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_WIDTH_DATA        (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_ENABLE_DATACHECK  (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_ENABLE_POISON     (Parser<TContext>& parser, TContext, std::istream& is);
        virtual bool        OnCHI_ENABLE_MPAM       (Parser<TContext>& parser, TContext, std::istream& is);
    };
}


// Implementation of: class ParametersSerDes 
namespace CLog::CLogT {
    /*
    Parameters* params;
    */

    template<class TContext>
    inline Parameters* ParametersSerDes<TContext>::GetParametersReference() noexcept
    {
        return params;
    }

    template<class TContext>
    inline const Parameters* ParametersSerDes<TContext>::GetParametersReference() const noexcept
    {
        return params;
    }

    template<class TContext>
    inline void ParametersSerDes<TContext>::SetParametersReference(Parameters* params) noexcept
    {
        this->params = params;
    }

    template<class TContext>
    inline void ParametersSerDes<TContext>::SerializeTo(std::ostream& os, bool segmentWrap) const
    {
        if (segmentWrap)
            WriteCLogSegmentParamBegin(os);

        if (params)
        {
            WriteCHISentenceIssue           (os, params->GetIssue());
            WriteCHISentenceNodeIDWidth     (os, params->GetNodeIdWidth());
            WriteCHISentenceAddrWidth       (os, params->GetReqAddrWidth());
            WriteCHISentenceReqRSVDCWidth   (os, params->GetReqRSVDCWidth());
            WriteCHISentenceDatRSVDCWidth   (os, params->GetDatRSVDCWidth());
            WriteCHISentenceDataWidth       (os, params->GetDataWidth());
            WriteCHISentenceDataCheckPresent(os, params->IsDataCheckPresent());
            WriteCHISentencePoisonPresent   (os, params->IsPoisonPresent());
            WriteCHISentenceMPAMPresent     (os, params->IsMPAMPresent());
        }

        if (segmentWrap)
            WriteCLogSegmentParamEnd(os);
    }

    template<class TContext>
    inline void ParametersSerDes<TContext>::RegisterAsDeserializer(Parser<TContext>& parser) noexcept
    {
        parser.RegisterExecutor(Sentence::CHI_ISSUE::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_ISSUE,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_WIDTH_NODEID::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_WIDTH_NODEID,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_WIDTH_ADDR::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_WIDTH_ADDR,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_WIDTH_RSVDC_REQ::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_WIDTH_RSVDC_REQ,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_WIDTH_RSVDC_DAT::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_WIDTH_RSVDC_DAT,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_WIDTH_DATA::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_WIDTH_DATA,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_ENABLE_DATACHECK::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_ENABLE_DATACHECK,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_ENABLE_POISON::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_ENABLE_POISON,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        parser.RegisterExecutor(Sentence::CHI_ENABLE_MPAM::Token,
            std::bind(&ParametersSerDes<TContext>::OnCHI_ENABLE_MPAM,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    template<class TContext>
    inline void ParametersSerDes<TContext>::Unregister(Parser<TContext>& parser) const noexcept
    {
        parser.UnregisterExecutor(Sentence::CHI_ISSUE::Token);
        parser.UnregisterExecutor(Sentence::CHI_WIDTH_NODEID::Token);
        parser.UnregisterExecutor(Sentence::CHI_WIDTH_ADDR::Token);
        parser.UnregisterExecutor(Sentence::CHI_WIDTH_RSVDC_REQ::Token);
        parser.UnregisterExecutor(Sentence::CHI_WIDTH_RSVDC_DAT::Token);
        parser.UnregisterExecutor(Sentence::CHI_WIDTH_DATA::Token);
        parser.UnregisterExecutor(Sentence::CHI_ENABLE_DATACHECK::Token);
        parser.UnregisterExecutor(Sentence::CHI_ENABLE_POISON::Token);
        parser.UnregisterExecutor(Sentence::CHI_ENABLE_MPAM::Token);
    }

    template<class TContext>
    inline void ParametersSerDes<TContext>::ApplySegmentToken(Parser<TContext>& parser) const noexcept
    {
        parser.SetSegmentToken(
            Sentence::CLOG_SEGMENT_PARAM_BEGIN::Token,
            Sentence::CLOG_SEGMENT_PARAM_END::Token);
    }

    template<class TContext>
    inline void ParametersSerDes<TContext>::ApplySegmentMask(Parser<TContext>& parser) const noexcept
    {
        parser.AddSegmentMask({
            Sentence::CHI_ISSUE             ::Token,
            Sentence::CHI_WIDTH_NODEID      ::Token,
            Sentence::CHI_WIDTH_ADDR        ::Token,
            Sentence::CHI_WIDTH_RSVDC_REQ   ::Token,
            Sentence::CHI_WIDTH_RSVDC_DAT   ::Token,
            Sentence::CHI_WIDTH_DATA        ::Token,
            Sentence::CHI_ENABLE_DATACHECK  ::Token,
            Sentence::CHI_ENABLE_POISON     ::Token,
            Sentence::CHI_ENABLE_MPAM       ::Token
        });
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_ISSUE(Parser<TContext>& parser, TContext, std::istream& is)
    {
        Issue issue;
        if (!Sentence::CHI_ISSUE::Term::Read(is, issue))
            return false;

        if (params)
            params->SetIssue(issue);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_WIDTH_NODEID(Parser<TContext>& parser, TContext, std::istream& is)
    {
        size_t nodeIdWidth;
        if (!Sentence::CHI_WIDTH_NODEID::Term::Read(is, nodeIdWidth))
            return false;

        if (params)
            return params->SetNodeIdWidth(nodeIdWidth);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_WIDTH_ADDR(Parser<TContext>& parser, TContext, std::istream& is)
    {
        size_t reqAddrWidth;
        if (!Sentence::CHI_WIDTH_ADDR::Term::Read(is, reqAddrWidth))
            return false;

        if (params)
            return params->SetReqAddrWidth(reqAddrWidth);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_WIDTH_RSVDC_REQ(Parser<TContext>& parser, TContext, std::istream& is)
    {
        size_t reqRsvdcWidth;
        if (!Sentence::CHI_WIDTH_RSVDC_REQ::Term::Read(is, reqRsvdcWidth))
            return false;

        if (params)
            return params->SetReqRSVDCWidth(reqRsvdcWidth);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_WIDTH_RSVDC_DAT(Parser<TContext>& parser, TContext, std::istream& is)
    {
        size_t datRsvdcWidth;
        if (!Sentence::CHI_WIDTH_RSVDC_DAT::Term::Read(is, datRsvdcWidth))
            return false;

        if (params)
            return params->SetDatRSVDCWidth(datRsvdcWidth);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_WIDTH_DATA(Parser<TContext>& parser, TContext, std::istream& is)
    {
        size_t dataWidth;
        if (!Sentence::CHI_WIDTH_DATA::Term::Read(is, dataWidth))
            return false;

        if (params)
            return params->SetDataWidth(dataWidth);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_ENABLE_DATACHECK(Parser<TContext>& parser, TContext, std::istream& is)
    {
        bool dataCheckPresent;
        if (!Sentence::CHI_ENABLE_DATACHECK::Term::Read(is, dataCheckPresent))
            return false;

        if (params)
            params->SetDataCheckPresent(dataCheckPresent);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_ENABLE_POISON(Parser<TContext>& parser, TContext, std::istream& is)
    {
        bool poisonPresent;
        if (!Sentence::CHI_ENABLE_POISON::Term::Read(is, poisonPresent))
            return false;

        if (params)
            params->SetPoisonPresent(poisonPresent);

        return true;
    }

    template<class TContext>
    inline bool ParametersSerDes<TContext>::OnCHI_ENABLE_MPAM(Parser<TContext>& parser, TContext, std::istream& is)
    {
        bool mpamPresent;
        if (!Sentence::CHI_ENABLE_MPAM::Term::Read(is, mpamPresent))
            return false;

        if (params)
            params->SetMPAMPresent(mpamPresent);

        return true;
    }
}

#endif // __CHI__CLOG_T_UTIL
