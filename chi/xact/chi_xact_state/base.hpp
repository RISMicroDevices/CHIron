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
                };

                uint8_t i8;
            };

        public:
            inline constexpr CacheState() noexcept : UC(false), UCE(false), UD(false), UDP(false), SC(false), SD(false), I(false) {}
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
            static constexpr CacheState FromSnpResp(typename Resp::type resp) noexcept;
            static constexpr CacheState FromSnpRespData(typename Resp::type resp) noexcept;
            static constexpr CacheState FromSnpRespDataPtl(typename Resp::type resp) noexcept;
            static constexpr CacheState FromSnpRespFwded(typename Resp::type resp) noexcept;
            static constexpr CacheState FromSnpRespDataFwded(typename Resp::type resp) noexcept;
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
                };

                uint16_t i16;
            };

        public:
            inline constexpr CacheResp() noexcept : UC(false), UCE(false), UD(false), UDP(false), SC(false), SD(false), I(false), UC_PD(false), UCE_PD(false), UD_PD(false), UDP_PD(false), SC_PD(false), SD_PD(false), I_PD(false) {}
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
            static constexpr CacheResp FromCompData(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromDataSepResp(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromRespSepData(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromComp(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromCopyBackWrData(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromSnpResp(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromSnpRespFwded(typename FwdState::type fwdState) noexcept;
            static constexpr CacheResp FromSnpRespFwdedFwdState(typename FwdState::type fwdState) noexcept;
            static constexpr CacheResp FromSnpRespData(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromSnpRespDataFwded(typename Resp::type resp) noexcept;
            static constexpr CacheResp FromSnpRespDataFwdedFwdState(typename FwdState::type fwdState) noexcept;
            static constexpr CacheResp FromSnpRespDataPtl(typename Resp::type resp) noexcept;
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
        : UC    (UC     )
        , UCE   (UCE    )
        , UD    (UD     )
        , UDP   (UDP    )
        , SC    (SC     )
        , SD    (SD     )
        , I     (I      )
    { }

    inline CacheState::CacheState(uint8_t i8) noexcept
        : i8    (i8     )
    { }

    inline constexpr CacheState::CacheState(const CacheResp resp) noexcept
        : UC    (resp.UC    )
        , UCE   (resp.UCE   )
        , UD    (resp.UD    )
        , UDP   (resp.UDP   )
        , SC    (resp.SC    )
        , SD    (resp.SD    )
        , I     (resp.I     )
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
        return (i8 & v) & 0x3FFF;
    }

    inline uint8_t CacheState::operator|(const uint8_t v) const noexcept
    {
        return (i8 | v) & 0x3FFF;
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
        : UC    (UC     )
        , UCE   (UCE    )
        , UD    (UD     )
        , UDP   (UDP    )
        , SC    (SC     )
        , SD    (SD     )
        , I     (I      )
        , UC_PD (UC_PD  )
        , UCE_PD(UCE_PD )
        , UD_PD (UD_PD  )
        , UDP_PD(UDP_PD )
        , SC_PD (SC_PD  )
        , SD_PD (SD_PD  )
        , I_PD  (I_PD   )
    { }

    inline CacheResp::CacheResp(uint16_t i16) noexcept
        : i16   (i16    )
    { }

    inline constexpr CacheResp::CacheResp(const CacheState state) noexcept
        : UC    (state.UC   )
        , UCE   (state.UCE  )
        , UD    (state.UD   )
        , UDP   (state.UDP  )
        , SC    (state.SC   )
        , SD    (state.SD   )
        , I     (state.I    )
        , UC_PD (false      )
        , UCE_PD(false      )
        , UD_PD (false      )
        , UDP_PD(false      )
        , SC_PD (false      )
        , SD_PD (false      )
        , I_PD  (false      )
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
        inline constexpr CacheState CacheState::FromSnpResp(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpResp_I:       return CacheStates::I;
                case Resp::SnpResp_SC:      return CacheStates::SC;
                case Resp::SnpResp_UC:      return CacheStates::UC | CacheStates::UD;
            //  case Resp::SnpResp_UD:      return CacheStates::UC | CacheStates::UD;
                case Resp::SnpResp_SD:      return CacheStates::SD;
                default:                    return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespData(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpRespData_I:       return CacheStates::I;
                case Resp::SnpRespData_UC:      return CacheStates::UC | CacheStates::UD;
            //  case Resp::SnpRespData_UD:      return CacheStates::UC | CacheStates::UD;
                case Resp::SnpRespData_SC:      return CacheStates::SC;
                case Resp::SnpRespData_SD:      return CacheStates::SD;
                case Resp::SnpRespData_I_PD:    return CacheStates::I;
                case Resp::SnpRespData_UC_PD:   return CacheStates::UC | CacheStates::UD;
                case Resp::SnpRespData_SC_PD:   return CacheStates::SC;
                default:                        return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespDataPtl(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpRespDataPtl_I_PD: return CacheStates::I;
                case Resp::SnpRespDataPtl_UD:   return CacheStates::UDP;
                default:                        return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespFwded(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpResp_I_Fwded_I:       return CacheStates::I;
            //  case Resp::SnpResp_I_Fwded_SC:      return CacheStates::I;
            //  case Resp::SnpResp_I_Fwded_UC:      return CacheStates::I;
            //  case Resp::SnpResp_I_Fwded_UD_PD:   return CacheStates::I;
            //  case Resp::SnpResp_I_Fwded_SD_PD:   return CacheStates::I;
                case Resp::SnpResp_SC_Fwded_I:      return CacheStates::SC;
            //  case Resp::SnpResp_SC_Fwded_SC:     return CacheStates::SC;
            //  case Resp::SnpResp_SC_Fwded_SD_PD:  return CacheStates::SC;
                case Resp::SnpResp_UC_Fwded_I:      return CacheStates::UC | CacheStates::UD;
            //  case Resp::SnpResp_UD_Fwded_I:      return CacheStates::UC | CacheStates::UD;
                case Resp::SnpResp_SD_Fwded_I:      return CacheStates::SD;
            //  case Resp::SnpResp_SD_Fwded_SC:     return CacheStates::SD;
                default:                            return CacheStates::None;
            }
        }
        //
        inline constexpr CacheState CacheState::FromSnpRespDataFwded(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpRespData_I_Fwded_SC:      return CacheStates::I;
            //  case Resp::SnpRespData_I_Fwded_SD_PD:   return CacheStates::I;
                case Resp::SnpRespData_SC_Fwded_SC:     return CacheStates::SC;
            //  case Resp::SnpRespData_SC_Fwded_SD_PD:  return CacheStates::SC;
                case Resp::SnpRespData_SD_Fwded_SC:     return CacheStates::SD;
                case Resp::SnpRespData_I_PD_Fwded_I:    return CacheStates::I;
            //  case Resp::SnpRespData_I_PD_Fwded_SC:   return CacheStates::I;
                case Resp::SnpRespData_SC_PD_Fwded_I:   return CacheStates::SC;
            //  case Resp::SnpRespData_SC_PD_Fwded_SC:  return CacheStates::SC;
                default:                                return CacheStates::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromCompData(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::CompData_I:          return CacheResps::I;
                case Resp::CompData_UC:         return CacheResps::UC;
                case Resp::CompData_SC:         return CacheResps::SC;
                case Resp::CompData_UD_PD:      return CacheResps::UD_PD;
                case Resp::CompData_SD_PD:      return CacheResps::SD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromDataSepResp(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::DataSepResp_I:       return CacheResps::I;
                case Resp::DataSepResp_UC:      return CacheResps::UC;
                case Resp::DataSepResp_SC:      return CacheResps::SC;
                case Resp::DataSepResp_UD_PD:   return CacheResps::UD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromRespSepData(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::RespSepData_I:       return CacheResps::I;
                case Resp::RespSepData_UC:      return CacheResps::UC;
                case Resp::RespSepData_SC:      return CacheResps::SC;
                case Resp::RespSepData_UD_PD:   return CacheResps::UD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromComp(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::Comp_I:              return CacheResps::I;
                case Resp::Comp_UC:             return CacheResps::UC;
                case Resp::Comp_SC:             return CacheResps::SC;
                case Resp::Comp_UD_PD:          return CacheResps::UD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromCopyBackWrData(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::CopyBackWrData_I:    return CacheResps::I;
                case Resp::CopyBackWrData_UC:   return CacheResps::UC;
                case Resp::CopyBackWrData_SC:   return CacheResps::SC;
                case Resp::CopyBackWrData_UD_PD:return CacheResps::UD_PD;
                case Resp::CopyBackWrData_SD_PD:return CacheResps::SD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpResp(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpResp_I:           return CacheResps::I;
                case Resp::SnpResp_SC:          return CacheResps::SC;
                case Resp::SnpResp_UC:          return CacheResps::UC | CacheResps::UD;
                case Resp::SnpResp_SD:          return CacheResps::SD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespFwded(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpResp_I_Fwded_I:       return CacheResps::I;
            //  case Resp::SnpResp_I_Fwded_SC:      return CacheResps::I;
            //  case Resp::SnpResp_I_Fwded_UC:      return CacheResps::I;
            //  case Resp::SnpResp_I_Fwded_UD_PD:   return CacheResps::I;
            //  case Resp::SnpResp_I_Fwded_SD_PD:   return CacheResps::I;
                case Resp::SnpResp_SC_Fwded_I:      return CacheResps::SC;
            //  case Resp::SnpResp_SC_Fwded_SC:     return CacheResps::SC;
            //  case Resp::SnpResp_SC_Fwded_SD_PD:  return CacheResps::SC;
                case Resp::SnpResp_UC_Fwded_I:      return CacheResps::UC | CacheResps::UD;
            //  case Resp::SnpResp_UD_Fwded_I:      return CacheResps::UC | CacheResps::UD;
                case Resp::SnpResp_SD_Fwded_I:      return CacheResps::SD;
            //  case Resp::SnpResp_SD_Fwded_SC:     return CacheResps::SD;
                default:                            return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespFwdedFwdState(typename FwdState::type fwdState) noexcept
        {
            switch (fwdState)
            {
                case FwdState::I:               return CacheResps::I;
                case FwdState::SC:              return CacheResps::SC;
                case FwdState::UC:              return CacheResps::UC;
                case FwdState::UD_PD:           return CacheResps::UD_PD;
                case FwdState::SD_PD:           return CacheResps::SD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespData(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpRespData_I:       return CacheResps::I;
                case Resp::SnpRespData_UC:      return CacheResps::UC | CacheResps::UD;
                case Resp::SnpRespData_SC:      return CacheResps::SC;
                case Resp::SnpRespData_SD:      return CacheResps::SD;
                case Resp::SnpRespData_I_PD:    return CacheResps::I_PD;
                case Resp::SnpRespData_UC_PD:   return CacheResps::UC_PD;
                case Resp::SnpRespData_SC_PD:   return CacheResps::SC_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespDataFwded(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpRespData_I_Fwded_SC:      return CacheResps::I;
            //  case Resp::SnpRespData_I_Fwded_SD_PD:   return CacheResps::I;
                case Resp::SnpRespData_SC_Fwded_SC:     return CacheResps::SC;
            //  case Resp::SnpRespData_SC_Fwded_SD_PD:  return CacheResps::SC;
                case Resp::SnpRespData_SD_Fwded_SC:     return CacheResps::SD;
                case Resp::SnpRespData_I_PD_Fwded_I:    return CacheResps::I_PD;
            //  case Resp::SnpRespData_I_PD_Fwded_SC:   return CacheResps::I_PD;
                case Resp::SnpRespData_SC_PD_Fwded_I:   return CacheResps::SC_PD;
            //  case Resp::SnpRespData_SC_PD_Fwded_SC:  return CacheResps::SC_PD;
                default:                                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespDataFwdedFwdState(typename FwdState::type fwdState) noexcept
        {
            switch (fwdState)
            {
                case FwdState::I:               return CacheResps::I;
                case FwdState::SC:              return CacheResps::SC;
                case FwdState::UC:              return CacheResps::UC;
                case FwdState::UD_PD:           return CacheResps::UD_PD;
                case FwdState::SD_PD:           return CacheResps::SD_PD;
                default:                        return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespDataPtl(typename Resp::type resp) noexcept
        {
            switch (resp)
            {
                case Resp::SnpRespDataPtl_I_PD: return CacheResps::I_PD;
                case Resp::SnpRespDataPtl_UD:   return CacheResps::UD;
                default:                        return CacheResps::None;
            }
        }
    }
/*
}
*/


#endif
