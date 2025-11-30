#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__BASE)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__BASE))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__BASE
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__BASE
#endif


/*
namespace CHI {
*/
    namespace Xact {

        class CacheState;
        class CacheResp;

        class CacheState {
        public:
            union {
                struct {
                    bool    UC      : 1;
                    bool    UCE     : 1;
                    bool    UD      : 1;
                    bool    UDP     : 1;
                    bool    SC      : 1;
                    bool    SD      : 1;
                    bool    I       : 1;

                    bool    _Padding0   : 1;
                };

                uint8_t i8;
            };

        public:
            inline constexpr CacheState() noexcept : UC(false), UCE(false), UD(false), UDP(false), SC(false), SD(false), I(false), _Padding0(false) {}
            constexpr CacheState(bool UC, bool UCE, bool UD, bool UDP, bool SC, bool SD, bool I) noexcept;
            CacheState(uint8_t i8) noexcept;
            constexpr explicit CacheState(const CacheResp resp) noexcept;
            constexpr operator bool() const noexcept;
            constexpr bool operator==(const CacheState obj) const noexcept;
            constexpr bool operator!=(const CacheState obj) const noexcept;
            constexpr CacheState operator^(const CacheState obj) const noexcept;
            constexpr CacheState operator&(const CacheState obj) const noexcept;
            constexpr CacheState operator|(const CacheState obj) const noexcept;
            constexpr CacheResp  operator|(const CacheResp obj) const noexcept;
            constexpr CacheState operator~() const noexcept;

            uint8_t operator&(const uint8_t v) const noexcept;
            uint8_t operator|(const uint8_t v) const noexcept;

        public:
            std::string     ToString() const noexcept;

        public:
            static constexpr CacheState FromSnpResp(Resp resp) noexcept;
            static constexpr CacheState FromSnpRespData(Resp resp) noexcept;
            static constexpr CacheState FromSnpRespDataPtl(Resp resp) noexcept;
            static constexpr CacheState FromSnpRespFwded(Resp resp) noexcept;
            static constexpr CacheState FromSnpRespDataFwded(Resp resp) noexcept;
        };

        class CacheResp {
        public:
            union {
                struct {
                    bool    UC      : 1;
                    bool    UCE     : 1;
                    bool    UD      : 1;
                    bool    UDP     : 1;
                    bool    SC      : 1;
                    bool    SD      : 1;
                    bool    I       : 1;

                    // PassDirty extended, should not be used in cache-line state
                    bool    UC_PD   : 1;
                    bool    UCE_PD  : 1;
                    bool    UD_PD   : 1;
                    bool    UDP_PD  : 1;
                    bool    SC_PD   : 1;
                    bool    SD_PD   : 1;
                    bool    I_PD    : 1;

                    //
                    bool    _Padding0   : 1;
                    bool    _Padding1   : 1;
                };

                uint16_t i16;
            };

        public:
            inline constexpr CacheResp() noexcept : UC(false), UCE(false), UD(false), UDP(false), SC(false), SD(false), I(false), UC_PD(false), UCE_PD(false), UD_PD(false), UDP_PD(false), SC_PD(false), SD_PD(false), I_PD(false), _Padding0(false), _Padding1(false) {}
            constexpr CacheResp(bool UC, bool UCE, bool UD, bool UDP, bool SC, bool SD, bool I, bool UC_PD, bool UCE_PD, bool UD_PD, bool UDP_PD, bool SC_PD, bool SD_PD, bool I_PD) noexcept;
            CacheResp(uint16_t i16) noexcept;
            constexpr CacheResp(const CacheState state) noexcept;
            constexpr operator bool() const noexcept;
            constexpr bool operator==(const CacheResp obj) const noexcept;
            constexpr bool operator!=(const CacheResp obj) const noexcept;
            constexpr CacheResp operator^(const CacheResp obj) const noexcept;
            constexpr CacheResp operator&(const CacheResp obj) const noexcept;
            constexpr CacheResp operator|(const CacheResp obj) const noexcept;
            constexpr CacheResp operator~() const noexcept;

            uint16_t operator&(const uint16_t v) const noexcept;
            uint16_t operator|(const uint16_t v) const noexcept;

            constexpr CacheResp PD() const noexcept;
            constexpr CacheResp NonPD() const noexcept;

        public:
            std::string     ToString() const noexcept;

        public:
            static constexpr CacheResp FromCompData(Resp resp) noexcept;
            static constexpr CacheResp FromDataSepResp(Resp resp) noexcept;
            static constexpr CacheResp FromRespSepData(Resp resp) noexcept;
            static constexpr CacheResp FromComp(Resp resp) noexcept;
            static constexpr CacheResp FromCopyBackWrData(Resp resp) noexcept;
            static constexpr CacheResp FromSnpResp(Resp resp) noexcept;
            static constexpr CacheResp FromSnpRespFwded(FwdState fwdState) noexcept;
            static constexpr CacheResp FromSnpRespFwdedFwdState(FwdState fwdState) noexcept;
            static constexpr CacheResp FromSnpRespData(Resp resp) noexcept;
            static constexpr CacheResp FromSnpRespDataFwded(Resp resp) noexcept;
            static constexpr CacheResp FromSnpRespDataFwdedFwdState(FwdState fwdState) noexcept;
            static constexpr CacheResp FromSnpRespDataPtl(Resp resp) noexcept;
        };
    }
/*
}
*/


// Implementation of: class CacheState
namespace /*CHI::*/Xact {
    /*
    ...
    */

    inline constexpr CacheState::CacheState(bool UC, bool UCE, bool UD, bool UDP, bool SC, bool SD, bool I) noexcept
        : UC        (UC     )
        , UCE       (UCE    )
        , UD        (UD     )
        , UDP       (UDP    )
        , SC        (SC     )
        , SD        (SD     )
        , I         (I      )
        , _Padding0 (false  )
    { }

    inline CacheState::CacheState(uint8_t i8) noexcept
        : i8    (i8     )
    { }

    inline constexpr CacheState::CacheState(const CacheResp resp) noexcept
        : UC        (resp.UC    )
        , UCE       (resp.UCE   )
        , UD        (resp.UD    )
        , UDP       (resp.UDP   )
        , SC        (resp.SC    )
        , SD        (resp.SD    )
        , I         (resp.I     )
        , _Padding0 (false      )
    { }
 
    inline constexpr CacheState::operator bool() const noexcept
    {
        return UC    || UCE    || UD    || UDP    || SC    || SD    || I;
    }

    inline constexpr bool CacheState::operator==(const CacheState obj) const noexcept
    {
        return i8 == obj.i8;
    }

    inline constexpr bool CacheState::operator!=(const CacheState obj) const noexcept
    {
        return i8 != obj.i8;
    }

    inline constexpr CacheState CacheState::operator^(const CacheState obj) const noexcept
    {
        CacheState r { false, false, false, false, false, false, false };
        //
        r.UC        = UC        != obj.UC;
        r.UCE       = UCE       != obj.UCE;
        r.UD        = UD        != obj.UD;
        r.UDP       = UDP       != obj.UDP;
        r.SC        = SC        != obj.SC;
        r.SD        = SD        != obj.SD;
        r.I         = I         != obj.I;
        //
        return r;
    }

    inline constexpr CacheState CacheState::operator&(const CacheState obj) const noexcept
    {
        CacheState r { false, false, false, false, false, false, false };
        //
        r.UC        = UC        && obj.UC;
        r.UCE       = UCE       && obj.UCE;
        r.UD        = UD        && obj.UD;
        r.UDP       = UDP       && obj.UDP;
        r.SC        = SC        && obj.SC;
        r.SD        = SD        && obj.SD;
        r.I         = I         && obj.I;
        //
        return r;
    }

    inline constexpr CacheState CacheState::operator|(const CacheState obj) const noexcept
    {
        CacheState r { false, false, false, false, false, false, false };
        //
        r.UC        = UC        || obj.UC;
        r.UCE       = UCE       || obj.UCE;
        r.UD        = UD        || obj.UD;
        r.UDP       = UDP       || obj.UDP;
        r.SC        = SC        || obj.SC;
        r.SD        = SD        || obj.SD;
        r.I         = I         || obj.I;
        //
        //
        return r;
    }

    inline constexpr CacheResp CacheState::operator|(const CacheResp obj) const noexcept
    {
        return CacheResp(*this) | obj;
    }

    inline constexpr CacheState CacheState::operator~() const noexcept
    {
        CacheState r { false, false, false, false, false, false, false };
        //
        r.UC        = !UC;
        r.UCE       = !UCE;
        r.UD        = !UD;
        r.UDP       = !UDP;
        r.SC        = !SC;
        r.SD        = !SD;
        r.I         = !I;
        //
        return r;
    }

    inline uint8_t CacheState::operator&(const uint8_t v) const noexcept
    {
        return (i8 & v) & 0x7F;
    }

    inline uint8_t CacheState::operator|(const uint8_t v) const noexcept
    {
        return (i8 | v) & 0x7F;
    }

    inline std::string CacheState::ToString() const noexcept
    {
        bool first = true;
        StringAppender strapp;

        if (UC )              { first = false; strapp.Append("UC" ); }
        if (UCE) { if (first) { first = false; strapp.Append("UCE"); } else strapp.Append(", UCE"); }
        if (UD ) { if (first) { first = false; strapp.Append("UD" ); } else strapp.Append(", UD" ); }
        if (UDP) { if (first) { first = false; strapp.Append("UDP"); } else strapp.Append(", UDP"); }
        if (SC ) { if (first) { first = false; strapp.Append("SC" ); } else strapp.Append(", SC" ); }
        if (SD ) { if (first) { first = false; strapp.Append("SD" ); } else strapp.Append(", SD" ); }
        if (I  ) { if (first) { first = false; strapp.Append("I"  ); } else strapp.Append(", I"  ); }

        return strapp.ToString();
    }
}

// Implementation of: class CacheResp
namespace /*CHI::*/Xact {
    /*
    ...
    */

    inline constexpr CacheResp::CacheResp(bool UC, bool UCE, bool UD, bool UDP, bool SC, bool SD, bool I, bool UC_PD, bool UCE_PD, bool UD_PD, bool UDP_PD, bool SC_PD, bool SD_PD, bool I_PD) noexcept
        : UC        (UC     )
        , UCE       (UCE    )
        , UD        (UD     )
        , UDP       (UDP    )
        , SC        (SC     )
        , SD        (SD     )
        , I         (I      )
        , UC_PD     (UC_PD  )
        , UCE_PD    (UCE_PD )
        , UD_PD     (UD_PD  )
        , UDP_PD    (UDP_PD )
        , SC_PD     (SC_PD  )
        , SD_PD     (SD_PD  )
        , I_PD      (I_PD   )
        , _Padding0 (false  )
        , _Padding1 (false  )
    { }

    inline CacheResp::CacheResp(uint16_t i16) noexcept
        : i16   (i16    )
    { }

    inline constexpr CacheResp::CacheResp(const CacheState state) noexcept
        : UC        (state.UC   )
        , UCE       (state.UCE  )
        , UD        (state.UD   )
        , UDP       (state.UDP  )
        , SC        (state.SC   )
        , SD        (state.SD   )
        , I         (state.I    )
        , UC_PD     (false      )
        , UCE_PD    (false      )
        , UD_PD     (false      )
        , UDP_PD    (false      )
        , SC_PD     (false      )
        , SD_PD     (false      )
        , I_PD      (false      )
        , _Padding0 (false      )
        , _Padding1 (false      )
    { }

    inline constexpr CacheResp::operator bool() const noexcept
    {
        return UC    || UCE    || UD    || UDP    || SC    || SD    || I    ||
               UC_PD || UCE_PD || UD_PD || UDP_PD || SC_PD || SD_PD || I_PD;
    }

    inline constexpr bool CacheResp::operator==(const CacheResp obj) const noexcept
    {
        return i16 == obj.i16;
    }

    inline constexpr bool CacheResp::operator!=(const CacheResp obj) const noexcept
    {
        return i16 != obj.i16;
    }

    inline constexpr CacheResp CacheResp::operator^(const CacheResp obj) const noexcept
    {
        CacheResp r { false, false, false, false, false, false, false, false, false, false, false, false, false, false };
        //
        r.UC        = UC        != obj.UC;
        r.UCE       = UCE       != obj.UCE;
        r.UD        = UD        != obj.UD;
        r.UDP       = UDP       != obj.UDP;
        r.SC        = SC        != obj.SC;
        r.SD        = SD        != obj.SD;
        r.I         = I         != obj.I;
        //
        r.UC_PD     = UC_PD     != obj.UC_PD;
        r.UCE_PD    = UCE_PD    != obj.UCE_PD;
        r.UD_PD     = UD_PD     != obj.UD_PD;
        r.UDP_PD    = UDP_PD    != obj.UDP_PD;
        r.SC_PD     = SC_PD     != obj.SC_PD;
        r.SD_PD     = SD_PD     != obj.SD_PD;
        r.I_PD      = I_PD      != obj.I_PD;
        //
        return r;
    }

    inline constexpr CacheResp CacheResp::operator&(const CacheResp obj) const noexcept
    {
        CacheResp r { false, false, false, false, false, false, false, false, false, false, false, false, false, false };
        //
        r.UC        = UC        && obj.UC;
        r.UCE       = UCE       && obj.UCE;
        r.UD        = UD        && obj.UD;
        r.UDP       = UDP       && obj.UDP;
        r.SC        = SC        && obj.SC;
        r.SD        = SD        && obj.SD;
        r.I         = I         && obj.I;
        //
        r.UC_PD     = UC_PD     && obj.UC_PD;
        r.UCE_PD    = UCE_PD    && obj.UCE_PD;
        r.UD_PD     = UD_PD     && obj.UD_PD;
        r.UDP_PD    = UDP_PD    && obj.UDP_PD;
        r.SC_PD     = SC_PD     && obj.SC_PD;
        r.SD_PD     = SD_PD     && obj.SD_PD;
        r.I_PD      = I_PD      && obj.I_PD;
        //
        return r;
    }

    inline constexpr CacheResp CacheResp::operator|(const CacheResp obj) const noexcept
    {
        CacheResp r { false, false, false, false, false, false, false, false, false, false, false, false, false, false };
        //
        r.UC        = UC        || obj.UC;
        r.UCE       = UCE       || obj.UCE;
        r.UD        = UD        || obj.UD;
        r.UDP       = UDP       || obj.UDP;
        r.SC        = SC        || obj.SC;
        r.SD        = SD        || obj.SD;
        r.I         = I         || obj.I;
        //
        r.UC_PD     = UC_PD     || obj.UC_PD;
        r.UCE_PD    = UCE_PD    || obj.UCE_PD;
        r.UD_PD     = UD_PD     || obj.UD_PD;
        r.UDP_PD    = UDP_PD    || obj.UDP_PD;
        r.SC_PD     = SC_PD     || obj.SC_PD;
        r.SD_PD     = SD_PD     || obj.SD_PD;
        r.I_PD      = I_PD      || obj.I_PD;
        //
        return r;
    }

    inline constexpr CacheResp CacheResp::operator~() const noexcept
    {
        CacheResp r { false, false, false, false, false, false, false, false, false, false, false, false, false, false };
        //
        r.UC        = !UC;
        r.UCE       = !UCE;
        r.UD        = !UD;
        r.UDP       = !UDP;
        r.SC        = !SC;
        r.SD        = !SD;
        r.I         = !I;
        //
        r.UC_PD     = !UC_PD;
        r.UCE_PD    = !UCE_PD;
        r.UD_PD     = !UD_PD;
        r.UDP_PD    = !UDP_PD;
        r.SC_PD     = !SC_PD;
        r.SD_PD     = !SD_PD;
        r.I_PD      = !I_PD;
        //
        return r;
    }

    inline uint16_t CacheResp::operator&(const uint16_t v) const noexcept
    {
        return (i16 & v) & 0x3FFF;
    }

    inline uint16_t CacheResp::operator|(const uint16_t v) const noexcept
    {
        return (i16 | v) & 0x3FFF;
    }

    inline constexpr CacheResp CacheResp::PD() const noexcept
    {
        return { false, false, false, false, false, false, false,
                 UC, UCE, UD, UDP, SC, SD, I };
    }

    inline constexpr CacheResp CacheResp::NonPD() const noexcept
    {
        return { UC_PD, UCE_PD, UD_PD, UDP_PD, SC_PD, SD_PD, I_PD,
                 false, false, false, false, false, false, false };
    }

    inline std::string CacheResp::ToString() const noexcept
    {
        bool first = true;
        StringAppender strapp;

        if (UC      )              { first = false; strapp.Append("UC"      ); }
        if (UCE     ) { if (first) { first = false; strapp.Append("UCE"     ); } else strapp.Append(", UCE"     ); }
        if (UD      ) { if (first) { first = false; strapp.Append("UD"      ); } else strapp.Append(", UD"      ); }
        if (UDP     ) { if (first) { first = false; strapp.Append("UDP"     ); } else strapp.Append(", UDP"     ); }
        if (SC      ) { if (first) { first = false; strapp.Append("SC"      ); } else strapp.Append(", SC"      ); }
        if (SD      ) { if (first) { first = false; strapp.Append("SD"      ); } else strapp.Append(", SD"      ); }
        if (I       ) { if (first) { first = false; strapp.Append("I"       ); } else strapp.Append(", I"       ); }

        if (UC_PD   ) { if (first) { first = false; strapp.Append("UC_PD"   ); } else strapp.Append(", UC_PD"   ); }
        if (UCE_PD  ) { if (first) { first = false; strapp.Append("UCE_PD"  ); } else strapp.Append(", UCE_PD"  ); }
        if (UD_PD   ) { if (first) { first = false; strapp.Append("UD_PD"   ); } else strapp.Append(", UD_PD"   ); }
        if (UDP_PD  ) { if (first) { first = false; strapp.Append("UDP_PD"  ); } else strapp.Append(", UDP_PD"  ); }
        if (SC_PD   ) { if (first) { first = false; strapp.Append("SC_PD"   ); } else strapp.Append(", SC_PD"   ); }
        if (SD_PD   ) { if (first) { first = false; strapp.Append("SD_PD"   ); } else strapp.Append(", SD_PD"   ); }
        if (I_PD    ) { if (first) { first = false; strapp.Append("I_PD"    ); } else strapp.Append(", I_PD"    ); }

        return strapp.ToString();
    }
}


/*
namespace CHI {
*/
    namespace Xact {

        namespace CacheStates {
            //
            static constexpr CacheState None    = { false, false, false, false, false, false, false };
            static constexpr CacheState All     = { true , true , true , true , true , true , true  };
            //
            static constexpr CacheState UC      = { true , false, false, false, false, false, false };
            static constexpr CacheState UCE     = { false, true , false, false, false, false, false };
            static constexpr CacheState UD      = { false, false, true , false, false, false, false };
            static constexpr CacheState UDP     = { false, false, false, true , false, false, false };
            static constexpr CacheState SC      = { false, false, false, false, true , false, false };
            static constexpr CacheState SD      = { false, false, false, false, false, true , false };
            static constexpr CacheState I       = { false, false, false, false, false, false, true  };
        }

        namespace CacheResps {
            //
            static constexpr CacheResp  None    = { false, false, false, false, false, false, false, false, false, false, false, false, false, false };
            static constexpr CacheResp  All     = { true , true , true , true , true , true , true , true , true , true , true , true , true , true  };
            //
            static constexpr CacheResp  UC      = { true , false, false, false, false, false, false, false, false, false, false, false, false, false };
            static constexpr CacheResp  UCE     = { false, true , false, false, false, false, false, false, false, false, false, false, false, false };
            static constexpr CacheResp  UD      = { false, false, true , false, false, false, false, false, false, false, false, false, false, false };
            static constexpr CacheResp  UDP     = { false, false, false, true , false, false, false, false, false, false, false, false, false, false };
            static constexpr CacheResp  SC      = { false, false, false, false, true , false, false, false, false, false, false, false, false, false };
            static constexpr CacheResp  SD      = { false, false, false, false, false, true , false, false, false, false, false, false, false, false };
            static constexpr CacheResp  I       = { false, false, false, false, false, false, true , false, false, false, false, false, false, false  };
            //
            static constexpr CacheResp  UC_PD   = { false, false, false, false, false, false, false, true , false, false, false, false, false, false };
            static constexpr CacheResp  UCE_PD  = { false, false, false, false, false, false, false, false, true , false, false, false, false, false };
            static constexpr CacheResp  UD_PD   = { false, false, false, false, false, false, false, false, false, true , false, false, false, false };
            static constexpr CacheResp  UDP_PD  = { false, false, false, false, false, false, false, false, false, false, true , false, false, false };
            static constexpr CacheResp  SC_PD   = { false, false, false, false, false, false, false, false, false, false, false, true , false, false };
            static constexpr CacheResp  SD_PD   = { false, false, false, false, false, false, false, false, false, false, false, false, true , false };
            static constexpr CacheResp  I_PD    = { false, false, false, false, false, false, false, false, false, false, false, false, false, true  };
        }

        //
        inline constexpr CacheState CacheState::FromSnpResp(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpResp_I:      return CacheStates::I;
                case Resps::SnpResp_SC:     return CacheStates::SC;
                case Resps::SnpResp_UC:     return CacheStates::UC | CacheStates::UD;
            //  case Resps::SnpResp_UD:     return CacheStates::UC | CacheStates::UD;
                case Resps::SnpResp_SD:     return CacheStates::SD;
                default:                    return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespData(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpRespData_I:      return CacheStates::I;
                case Resps::SnpRespData_UC:     return CacheStates::UC | CacheStates::UD;
            //  case Resps::SnpRespData_UD:     return CacheStates::UC | CacheStates::UD;
                case Resps::SnpRespData_SC:     return CacheStates::SC;
                case Resps::SnpRespData_SD:     return CacheStates::SD;
                case Resps::SnpRespData_I_PD:   return CacheStates::I;
                case Resps::SnpRespData_UC_PD:  return CacheStates::UC | CacheStates::UD;
                case Resps::SnpRespData_SC_PD:  return CacheStates::SC;
                default:                        return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespDataPtl(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpRespDataPtl_I_PD:return CacheStates::I;
                case Resps::SnpRespDataPtl_UD:  return CacheStates::UDP;
                default:                        return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespFwded(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpResp_I_Fwded_I:      return CacheStates::I;
            //  case Resps::SnpResp_I_Fwded_SC:     return CacheStates::I;
            //  case Resps::SnpResp_I_Fwded_UC:     return CacheStates::I;
            //  case Resps::SnpResp_I_Fwded_UD_PD:  return CacheStates::I;
            //  case Resps::SnpResp_I_Fwded_SD_PD:  return CacheStates::I;
                case Resps::SnpResp_SC_Fwded_I:     return CacheStates::SC;
            //  case Resps::SnpResp_SC_Fwded_SC:    return CacheStates::SC;
            //  case Resps::SnpResp_SC_Fwded_SD_PD: return CacheStates::SC;
                case Resps::SnpResp_UC_Fwded_I:     return CacheStates::UC | CacheStates::UD;
            //  case Resps::SnpResp_UD_Fwded_I:     return CacheStates::UC | CacheStates::UD;
                case Resps::SnpResp_SD_Fwded_I:     return CacheStates::SD;
            //  case Resps::SnpResp_SD_Fwded_SC:    return CacheStates::SD;
                default:                            return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespDataFwded(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpRespData_I_Fwded_SC:     return CacheStates::I;
            //  case Resps::SnpRespData_I_Fwded_SD_PD:  return CacheStates::I;
                case Resps::SnpRespData_SC_Fwded_SC:    return CacheStates::SC;
            //  case Resps::SnpRespData_SC_Fwded_SD_PD: return CacheStates::SC;
                case Resps::SnpRespData_SD_Fwded_SC:    return CacheStates::SD;
                case Resps::SnpRespData_I_PD_Fwded_I:   return CacheStates::I;
            //  case Resps::SnpRespData_I_PD_Fwded_SC:  return CacheStates::I;
                case Resps::SnpRespData_SC_PD_Fwded_I:  return CacheStates::SC;
            //  case Resps::SnpRespData_SC_PD_Fwded_SC: return CacheStates::SC;
                default:                                return CacheStates::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromCompData(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::CompData_I:         return CacheResps::I;
                case Resps::CompData_UC:        return CacheResps::UC;
                case Resps::CompData_SC:        return CacheResps::SC;
                case Resps::CompData_UD_PD:     return CacheResps::UD_PD;
                case Resps::CompData_SD_PD:     return CacheResps::SD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromDataSepResp(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::DataSepResp_I:      return CacheResps::I;
                case Resps::DataSepResp_UC:     return CacheResps::UC;
                case Resps::DataSepResp_SC:     return CacheResps::SC;
                case Resps::DataSepResp_UD_PD:  return CacheResps::UD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromRespSepData(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::RespSepData_I:      return CacheResps::I;
                case Resps::RespSepData_UC:     return CacheResps::UC;
                case Resps::RespSepData_SC:     return CacheResps::SC;
                case Resps::RespSepData_UD_PD:  return CacheResps::UD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromComp(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::Comp_I:             return CacheResps::I;
                case Resps::Comp_UC:            return CacheResps::UC;
                case Resps::Comp_SC:            return CacheResps::SC;
                case Resps::Comp_UD_PD:         return CacheResps::UD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromCopyBackWrData(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::CopyBackWrData_I:       return CacheResps::I;
                case Resps::CopyBackWrData_UC:      return CacheResps::UC;
                case Resps::CopyBackWrData_SC:      return CacheResps::SC;
                case Resps::CopyBackWrData_UD_PD:   return CacheResps::UD_PD;
                case Resps::CopyBackWrData_SD_PD:   return CacheResps::SD_PD;
                default:                            return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpResp(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpResp_I:          return CacheResps::I;
                case Resps::SnpResp_SC:         return CacheResps::SC;
                case Resps::SnpResp_UC:         return CacheResps::UC | CacheResps::UD;
                case Resps::SnpResp_SD:         return CacheResps::SD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespFwded(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpResp_I_Fwded_I:      return CacheResps::I;
            //  case Resps::SnpResp_I_Fwded_SC:     return CacheResps::I;
            //  case Resps::SnpResp_I_Fwded_UC:     return CacheResps::I;
            //  case Resps::SnpResp_I_Fwded_UD_PD:  return CacheResps::I;
            //  case Resps::SnpResp_I_Fwded_SD_PD:  return CacheResps::I;
                case Resps::SnpResp_SC_Fwded_I:     return CacheResps::SC;
            //  case Resps::SnpResp_SC_Fwded_SC:    return CacheResps::SC;
            //  case Resps::SnpResp_SC_Fwded_SD_PD: return CacheResps::SC;
                case Resps::SnpResp_UC_Fwded_I:     return CacheResps::UC | CacheResps::UD;
            //  case Resps::SnpResp_UD_Fwded_I:     return CacheResps::UC | CacheResps::UD;
                case Resps::SnpResp_SD_Fwded_I:     return CacheResps::SD;
            //  case Resps::SnpResp_SD_Fwded_SC:    return CacheResps::SD;
                default:                            return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespFwdedFwdState(FwdState fwdState) noexcept
        {
            switch (fwdState)
            {
                case FwdStates::I:              return CacheResps::I;
                case FwdStates::SC:             return CacheResps::SC;
                case FwdStates::UC:             return CacheResps::UC;
                case FwdStates::UD_PD:          return CacheResps::UD_PD;
                case FwdStates::SD_PD:          return CacheResps::SD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespData(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpRespData_I:      return CacheResps::I;
                case Resps::SnpRespData_UC:     return CacheResps::UC | CacheResps::UD;
                case Resps::SnpRespData_SC:     return CacheResps::SC;
                case Resps::SnpRespData_SD:     return CacheResps::SD;
                case Resps::SnpRespData_I_PD:   return CacheResps::I_PD;
                case Resps::SnpRespData_UC_PD:  return CacheResps::UC_PD;
                case Resps::SnpRespData_SC_PD:  return CacheResps::SC_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespDataFwded(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpRespData_I_Fwded_SC:     return CacheResps::I;
            //  case Resps::SnpRespData_I_Fwded_SD_PD:  return CacheResps::I;
                case Resps::SnpRespData_SC_Fwded_SC:    return CacheResps::SC;
            //  case Resps::SnpRespData_SC_Fwded_SD_PD: return CacheResps::SC;
                case Resps::SnpRespData_SD_Fwded_SC:    return CacheResps::SD;
                case Resps::SnpRespData_I_PD_Fwded_I:   return CacheResps::I_PD;
            //  case Resps::SnpRespData_I_PD_Fwded_SC:  return CacheResps::I_PD;
                case Resps::SnpRespData_SC_PD_Fwded_I:  return CacheResps::SC_PD;
            //  case Resps::SnpRespData_SC_PD_Fwded_SC: return CacheResps::SC_PD;
                default:                                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespDataFwdedFwdState(FwdState fwdState) noexcept
        {
            switch (fwdState)
            {
                case FwdStates::I:              return CacheResps::I;
                case FwdStates::SC:             return CacheResps::SC;
                case FwdStates::UC:             return CacheResps::UC | CacheResps::UD;
                case FwdStates::UD_PD:          return CacheResps::UD_PD;
                case FwdStates::SD_PD:          return CacheResps::SD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespDataPtl(Resp resp) noexcept
        {
            switch (resp)
            {
                case Resps::SnpRespDataPtl_I_PD:return CacheResps::I_PD;
                case Resps::SnpRespDataPtl_UD:  return CacheResps::UC | CacheResps::UD;
                default:                        return CacheResps::None;
            }
        }
    }
/*
}
*/


#endif
