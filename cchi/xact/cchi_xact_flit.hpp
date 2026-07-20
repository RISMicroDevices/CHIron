#pragma once

#ifndef __CCHI__CCHI_XACT_FLIT
#define __CCHI__CCHI_XACT_FLIT

#include <cstdint>

#include "../spec/cchi_protocol_flits.hpp"
#include "cchi_xact_base/cchi_xact_base_denial.hpp"
#include "cchi_xact_base/cchi_xact_base_topology.hpp"

#include "../../common/utility.hpp"


namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    class FiredFlit {
    public:
        XactScopeEnum   scope;
        uint64_t        time;
        bool            isTX;

    public:
        Companion   companion;

    public:
        FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept;

    public:
        bool        IsTX() const noexcept;
        bool        IsRX() const noexcept;
        
    public:
        virtual ChannelTypeEnum GetChannel() const noexcept = 0;
    };

    template<FlitConfigurationConcept config>
    class FiredRequestFlit : public FiredFlit<config> {
    public:
        struct {
            bool isEVT : 1;
            bool isSNP : 1;
            bool isREQ : 1;
        };
        union {
            Flits::EVT<config> evt;
            Flits::SNP<config> snp;
            Flits::REQ<config> req;
        } flit;

    public:
        FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::EVT<config>& evtFlit) noexcept;
        FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::SNP<config>& snpFlit) noexcept;
        FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::REQ<config>& reqFlit) noexcept;
    
    public:
        bool        IsEVT() const noexcept;
        bool        IsSNP() const noexcept;
        bool        IsREQ() const noexcept;

        bool        IsTXEVT() const noexcept;
        bool        IsRXEVT() const noexcept;
        bool        IsTXSNP() const noexcept;
        bool        IsRXSNP() const noexcept;
        bool        IsTXREQ() const noexcept;
        bool        IsRXREQ() const noexcept;

    public:
        virtual ChannelTypeEnum GetChannel() const noexcept override;
        ChannelTypeEnum         GetChannelType() const noexcept;
    };

    template<FlitConfigurationConcept config>
    class FiredResponseFlit : public FiredFlit<config> {
    public:
        struct {
            bool isDnRSP : 1;
            bool isUpRSP : 1;
            bool isDnDAT : 1;
            bool isUpDAT : 1;
        };
        union {
            Flits::DnRSP<config> dnrsp;
            Flits::UpRSP<config> uprsp;
            Flits::DnDAT<config> dndat;
            Flits::UpDAT<config> updat;
        } flit;

    public:
        FiredResponseFlit() noexcept;
        FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DnRSP<config>& dnrspFlit) noexcept;
        FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::UpRSP<config>& uprspFlit) noexcept;
        FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DnDAT<config>& dndatFlit) noexcept;
        FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::UpDAT<config>& updatFlit) noexcept;

    public:
        bool        IsDnRSP() const noexcept;
        bool        IsUpRSP() const noexcept;
        bool        IsDnDAT() const noexcept;
        bool        IsUpDAT() const noexcept;

        bool        IsTXDnRSP() const noexcept;
        bool        IsRXDnRSP() const noexcept;
        bool        IsTXUpRSP() const noexcept;
        bool        IsRXUpRSP() const noexcept;
        bool        IsTXDnDAT() const noexcept;
        bool        IsRXDnDAT() const noexcept;
        bool        IsTXUpDAT() const noexcept;
        bool        IsRXUpDAT() const noexcept;

    public:
        virtual ChannelTypeEnum GetChannel() const noexcept override;
        ChannelTypeEnum         GetChannelType() const noexcept;
    };
}


// Implementation of: class FiredFlit
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline FiredFlit<config>::FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept
        : scope  (scope)
        , time   (time)
        , isTX   (isTX)
    { }

    template<FlitConfigurationConcept config>
    inline bool FiredFlit<config>::IsTX() const noexcept
    {
        return isTX;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredFlit<config>::IsRX() const noexcept
    {
        return !isTX;
    }
}


// Implementation of: class FiredRequestFlit
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline FiredRequestFlit<config>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::REQ<config>& reqFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isEVT             (false)
        , isSNP             (false)
        , isREQ             (true)
    {
        flit.req = reqFlit;
    }

    template<FlitConfigurationConcept config>
    inline FiredRequestFlit<config>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::SNP<config>& snpFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isEVT             (false)
        , isSNP             (true)
        , isREQ             (false)
    {
        flit.snp = snpFlit;
    }

    template<FlitConfigurationConcept config>
    inline FiredRequestFlit<config>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::EVT<config>& evtFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isEVT             (true)
        , isSNP             (false)
        , isREQ             (false)
    {
        flit.evt = evtFlit;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsEVT() const noexcept
    {
        return isEVT;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsSNP() const noexcept
    {
        return isSNP;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsREQ() const noexcept
    {
        return isREQ;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTXEVT() const noexcept
    {
        return IsEVT() && this->IsTX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsRXEVT() const noexcept
    {
        return IsEVT() && this->IsRX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTXSNP() const noexcept
    {
        return IsSNP() && this->IsTX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsRXSNP() const noexcept
    {
        return IsSNP() && this->IsRX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsTXREQ() const noexcept
    {
        return IsREQ() && this->IsTX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredRequestFlit<config>::IsRXREQ() const noexcept
    {
        return IsREQ() && this->IsRX();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredRequestFlit<config>::GetChannel() const noexcept
    {
        return GetChannelType();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredRequestFlit<config>::GetChannelType() const noexcept
    {
        if (IsEVT())
            return ChannelType::EVT;

        if (IsSNP())
            return ChannelType::SNP;

        if (IsREQ())
            return ChannelType::REQ;
           
        return nullptr;
    }
}


// Implementation of: class FiredResponseFlit
namespace CCHI::Xact {

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit() noexcept
        : FiredFlit<config> (nullptr, false, 0)
        , isDnRSP           (false)
        , isUpRSP           (false)
        , isDnDAT           (false)
        , isUpDAT           (false)
    { }

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DnRSP<config>& dnrspFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isDnRSP           (true)
        , isUpRSP           (false)
        , isDnDAT           (false)
        , isUpDAT           (false)
    {
        flit.dnrsp = dnrspFlit;
    }

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::UpRSP<config>& uprspFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isDnRSP           (false)
        , isUpRSP           (true)
        , isDnDAT           (false)
        , isUpDAT           (false)
    {
        flit.uprsp = uprspFlit;
    }

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DnDAT<config>& dndatFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isDnRSP           (false)
        , isUpRSP           (false)
        , isDnDAT           (true)
        , isUpDAT           (false)
    {
        flit.dndat = dndatFlit;
    }

    template<FlitConfigurationConcept config>
    inline FiredResponseFlit<config>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::UpDAT<config>& updatFlit) noexcept
        : FiredFlit<config> (scope, isTX, time)
        , isDnRSP           (false)
        , isUpRSP           (false)
        , isDnDAT           (false)
        , isUpDAT           (true)
    {
        flit.updat = updatFlit;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsDnRSP() const noexcept
    {
        return isDnRSP;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsUpRSP() const noexcept
    {
        return isUpRSP;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsDnDAT() const noexcept
    {
        return isDnDAT;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsUpDAT() const noexcept
    {
        return isUpDAT;
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXDnRSP() const noexcept
    {
        return IsDnRSP() && this->IsTX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXDnRSP() const noexcept
    {
        return IsDnRSP() && this->IsRX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXUpRSP() const noexcept
    {
        return IsUpRSP() && this->IsTX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXUpRSP() const noexcept
    {
        return IsUpRSP() && this->IsRX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXDnDAT() const noexcept
    {
        return IsDnDAT() && this->IsTX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXDnDAT() const noexcept
    {
        return IsDnDAT() && this->IsRX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsTXUpDAT() const noexcept
    {
        return IsUpDAT() && this->IsTX();
    }

    template<FlitConfigurationConcept config>
    inline bool FiredResponseFlit<config>::IsRXUpDAT() const noexcept
    {
        return IsUpDAT() && this->IsRX();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredResponseFlit<config>::GetChannel() const noexcept
    {
        return GetChannelType();
    }

    template<FlitConfigurationConcept config>
    inline ChannelTypeEnum FiredResponseFlit<config>::GetChannelType() const noexcept
    {
        if (IsDnRSP())
            return ChannelType::DnRSP;

        if (IsUpRSP())
            return ChannelType::UpRSP;

        if (IsDnDAT())
            return ChannelType::DnDAT;

        if (IsUpDAT())
            return ChannelType::UpDAT;
           
        return nullptr;
    }
}


#endif // __CCHI__CCHI_XACT_FLIT
