#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__BASE
#define __CCHI__CCHI_XACT_STATE__BASE

#include "includes.hpp"


namespace CCHI {
    namespace Xact {

        class CacheState;
        class CacheResp;

        /*
        CacheState: a set over the 4 CCHI cache-line coherency states
        (spec: states.md, "缓存行状态": I, UC, UD, SC), one bit per state.
        */
        class CacheState {
        public:
            union {
                struct {
                    bool    UC          : 1;
                    bool    UD          : 1;
                    bool    SC          : 1;
                    bool    I           : 1;

                    bool    _Padding0   : 1;
                    bool    _Padding1   : 1;
                    bool    _Padding2   : 1;
                    bool    _Padding3   : 1;
                };

                uint8_t i8;
            };

        public:
            inline constexpr CacheState() noexcept : UC(false), UD(false), SC(false), I(false), _Padding0(false), _Padding1(false), _Padding2(false), _Padding3(false) {}
            constexpr CacheState(bool UC, bool UD, bool SC, bool I) noexcept;
            CacheState(uint8_t i8) noexcept;
            constexpr explicit CacheState(const CacheResp resp) noexcept;
            constexpr operator bool() const noexcept;

            // *NOTICE: elementwise comparisons (instead of CHI's union-i8 compare)
            //          keep these operators constant-evaluable for static_asserts.
            constexpr bool operator==(const CacheState obj) const noexcept;
            constexpr bool operator!=(const CacheState obj) const noexcept;
            constexpr CacheState operator^(const CacheState obj) const noexcept;
            constexpr CacheState operator&(const CacheState obj) const noexcept;
            constexpr CacheState operator|(const CacheState obj) const noexcept;
            constexpr CacheResp  operator|(const CacheResp obj) const noexcept;
            constexpr CacheState operator~() const noexcept;

        public:
            std::string     ToString() const noexcept;
        };

        /*
        CacheResp: a set over the 6 encodable CCHI Resp values
        (cchi_protocol_encoding.hpp: Resps::{I, SC, UC, I_PD, SC_PD, UC_PD}),
        one bit per encoding.
        */
        class CacheResp {
        public:
            union {
                struct {
                    bool    I           : 1;
                    bool    SC          : 1;
                    bool    UC          : 1;

                    // PassDirty extended, should not be used in cache-line state
                    bool    I_PD        : 1;
                    bool    SC_PD       : 1;
                    bool    UC_PD       : 1;

                    bool    _Padding0   : 1;
                    bool    _Padding1   : 1;
                };

                uint8_t i8;
            };

        public:
            inline constexpr CacheResp() noexcept : I(false), SC(false), UC(false), I_PD(false), SC_PD(false), UC_PD(false), _Padding0(false), _Padding1(false) {}
            constexpr CacheResp(bool I, bool SC, bool UC, bool I_PD, bool SC_PD, bool UC_PD) noexcept;
            CacheResp(uint8_t i8) noexcept;
            constexpr CacheResp(const CacheState state) noexcept;
            constexpr operator bool() const noexcept;

            // *NOTICE: elementwise comparisons, see CacheState::operator==.
            constexpr bool operator==(const CacheResp obj) const noexcept;
            constexpr bool operator!=(const CacheResp obj) const noexcept;
            constexpr CacheResp operator^(const CacheResp obj) const noexcept;
            constexpr CacheResp operator&(const CacheResp obj) const noexcept;
            constexpr CacheResp operator|(const CacheResp obj) const noexcept;
            constexpr CacheResp operator~() const noexcept;

        public:
            std::string     ToString() const noexcept;

        public:
            // Normalizers: raw flit Resp values (Resps::I ... Resps::UC_PD) -> CacheResp set space.
            // Unencodable/semantically invalid values on a given channel map to CacheResps::None.
            static constexpr CacheResp FromCompData(Resp resp) noexcept;
            static constexpr CacheResp FromComp(Resp resp) noexcept;
            static constexpr CacheResp FromCompCMO(Resp resp) noexcept;
            static constexpr CacheResp FromCompStash(Resp resp) noexcept;
            static constexpr CacheResp FromSnpResp(Resp resp) noexcept;
            static constexpr CacheResp FromSnpRespData(Resp resp) noexcept;
            static constexpr CacheResp FromCopyBackWrData(Resp resp) noexcept;
            static constexpr CacheResp FromNonCopyBackWrData(Resp resp) noexcept;
        };
    }
}


// Implementation of: class CacheState
namespace CCHI::Xact {

    inline constexpr CacheState::CacheState(bool UC, bool UD, bool SC, bool I) noexcept
        : UC        (UC     )
        , UD        (UD     )
        , SC        (SC     )
        , I         (I      )
        , _Padding0 (false  )
        , _Padding1 (false  )
        , _Padding2 (false  )
        , _Padding3 (false  )
    { }

    inline CacheState::CacheState(uint8_t i8) noexcept
        : i8    (i8     )
    { }

    inline constexpr CacheState::CacheState(const CacheResp resp) noexcept
        : UC        (resp.UC    )
        , UD        (false      )
        , SC        (resp.SC    )
        , I         (resp.I     )
        , _Padding0 (false      )
        , _Padding1 (false      )
        , _Padding2 (false      )
        , _Padding3 (false      )
    { }

    inline constexpr CacheState::operator bool() const noexcept
    {
        return UC || UD || SC || I;
    }

    inline constexpr bool CacheState::operator==(const CacheState obj) const noexcept
    {
        return UC == obj.UC && UD == obj.UD && SC == obj.SC && I == obj.I;
    }

    inline constexpr bool CacheState::operator!=(const CacheState obj) const noexcept
    {
        return !(*this == obj);
    }

    inline constexpr CacheState CacheState::operator^(const CacheState obj) const noexcept
    {
        CacheState r { false, false, false, false };
        //
        r.UC        = UC        != obj.UC;
        r.UD        = UD        != obj.UD;
        r.SC        = SC        != obj.SC;
        r.I         = I         != obj.I;
        //
        return r;
    }

    inline constexpr CacheState CacheState::operator&(const CacheState obj) const noexcept
    {
        CacheState r { false, false, false, false };
        //
        r.UC        = UC        && obj.UC;
        r.UD        = UD        && obj.UD;
        r.SC        = SC        && obj.SC;
        r.I         = I         && obj.I;
        //
        return r;
    }

    inline constexpr CacheState CacheState::operator|(const CacheState obj) const noexcept
    {
        CacheState r { false, false, false, false };
        //
        r.UC        = UC        || obj.UC;
        r.UD        = UD        || obj.UD;
        r.SC        = SC        || obj.SC;
        r.I         = I         || obj.I;
        //
        return r;
    }

    inline constexpr CacheResp CacheState::operator|(const CacheResp obj) const noexcept
    {
        return CacheResp(*this) | obj;
    }

    inline constexpr CacheState CacheState::operator~() const noexcept
    {
        CacheState r { false, false, false, false };
        //
        r.UC        = !UC;
        r.UD        = !UD;
        r.SC        = !SC;
        r.I         = !I;
        //
        return r;
    }

    inline std::string CacheState::ToString() const noexcept
    {
        bool first = true;
        StringAppender strapp;

        if (UC)              { first = false; strapp.Append("UC"); }
        if (UD) { if (first) { first = false; strapp.Append("UD"); } else strapp.Append(", UD"); }
        if (SC) { if (first) { first = false; strapp.Append("SC"); } else strapp.Append(", SC"); }
        if (I ) { if (first) { first = false; strapp.Append("I" ); } else strapp.Append(", I" ); }

        return strapp.ToString();
    }
}

// Implementation of: class CacheResp
namespace CCHI::Xact {

    inline constexpr CacheResp::CacheResp(bool I, bool SC, bool UC, bool I_PD, bool SC_PD, bool UC_PD) noexcept
        : I         (I      )
        , SC        (SC     )
        , UC        (UC     )
        , I_PD      (I_PD   )
        , SC_PD     (SC_PD  )
        , UC_PD     (UC_PD  )
        , _Padding0 (false  )
        , _Padding1 (false  )
    { }

    inline CacheResp::CacheResp(uint8_t i8) noexcept
        : i8    (i8     )
    { }

    inline constexpr CacheResp::CacheResp(const CacheState state) noexcept
        : I         (state.I    )
        , SC        (state.SC   )
        , UC        (state.UC   )
        , I_PD      (false      )
        , SC_PD     (false      )
        , UC_PD     (false      )
        , _Padding0 (false      )
        , _Padding1 (false      )
    { }

    inline constexpr CacheResp::operator bool() const noexcept
    {
        return I || SC || UC || I_PD || SC_PD || UC_PD;
    }

    inline constexpr bool CacheResp::operator==(const CacheResp obj) const noexcept
    {
        return I == obj.I && SC == obj.SC && UC == obj.UC
            && I_PD == obj.I_PD && SC_PD == obj.SC_PD && UC_PD == obj.UC_PD;
    }

    inline constexpr bool CacheResp::operator!=(const CacheResp obj) const noexcept
    {
        return !(*this == obj);
    }

    inline constexpr CacheResp CacheResp::operator^(const CacheResp obj) const noexcept
    {
        CacheResp r { false, false, false, false, false, false };
        //
        r.I         = I         != obj.I;
        r.SC        = SC        != obj.SC;
        r.UC        = UC        != obj.UC;
        //
        r.I_PD      = I_PD      != obj.I_PD;
        r.SC_PD     = SC_PD     != obj.SC_PD;
        r.UC_PD     = UC_PD     != obj.UC_PD;
        //
        return r;
    }

    inline constexpr CacheResp CacheResp::operator&(const CacheResp obj) const noexcept
    {
        CacheResp r { false, false, false, false, false, false };
        //
        r.I         = I         && obj.I;
        r.SC        = SC        && obj.SC;
        r.UC        = UC        && obj.UC;
        //
        r.I_PD      = I_PD      && obj.I_PD;
        r.SC_PD     = SC_PD     && obj.SC_PD;
        r.UC_PD     = UC_PD     && obj.UC_PD;
        //
        return r;
    }

    inline constexpr CacheResp CacheResp::operator|(const CacheResp obj) const noexcept
    {
        CacheResp r { false, false, false, false, false, false };
        //
        r.I         = I         || obj.I;
        r.SC        = SC        || obj.SC;
        r.UC        = UC        || obj.UC;
        //
        r.I_PD      = I_PD      || obj.I_PD;
        r.SC_PD     = SC_PD     || obj.SC_PD;
        r.UC_PD     = UC_PD     || obj.UC_PD;
        //
        return r;
    }

    inline constexpr CacheResp CacheResp::operator~() const noexcept
    {
        CacheResp r { false, false, false, false, false, false };
        //
        r.I         = !I;
        r.SC        = !SC;
        r.UC        = !UC;
        //
        r.I_PD      = !I_PD;
        r.SC_PD     = !SC_PD;
        r.UC_PD     = !UC_PD;
        //
        return r;
    }

    inline std::string CacheResp::ToString() const noexcept
    {
        bool first = true;
        StringAppender strapp;

        if (I       )              { first = false; strapp.Append("I"       ); }
        if (SC      ) { if (first) { first = false; strapp.Append("SC"      ); } else strapp.Append(", SC"      ); }
        if (UC      ) { if (first) { first = false; strapp.Append("UC"      ); } else strapp.Append(", UC"      ); }

        if (I_PD    ) { if (first) { first = false; strapp.Append("I_PD"    ); } else strapp.Append(", I_PD"    ); }
        if (SC_PD   ) { if (first) { first = false; strapp.Append("SC_PD"   ); } else strapp.Append(", SC_PD"   ); }
        if (UC_PD   ) { if (first) { first = false; strapp.Append("UC_PD"   ); } else strapp.Append(", UC_PD"   ); }

        return strapp.ToString();
    }
}


namespace CCHI {
    namespace Xact {

        namespace CacheStates {
            //
            static constexpr CacheState None    = { false, false, false, false };
            static constexpr CacheState All     = { true , true , true , true  };
            //
            static constexpr CacheState UC      = { true , false, false, false };
            static constexpr CacheState UD      = { false, true , false, false };
            static constexpr CacheState SC      = { false, false, true , false };
            static constexpr CacheState I       = { false, false, false, true  };
        }

        namespace CacheResps {
            //
            static constexpr CacheResp  None    = { false, false, false, false, false, false };
            static constexpr CacheResp  All     = { true , true , true , true , true , true  };
            //
            static constexpr CacheResp  I       = { true , false, false, false, false, false };
            static constexpr CacheResp  SC      = { false, true , false, false, false, false };
            static constexpr CacheResp  UC      = { false, false, true , false, false, false };
            //
            static constexpr CacheResp  I_PD    = { false, false, false, true , false, false };
            static constexpr CacheResp  SC_PD   = { false, false, false, false, true , false };
            static constexpr CacheResp  UC_PD   = { false, false, false, false, false, true  };
        }

        //
        inline constexpr CacheResp CacheResp::FromCompData(Resp resp) noexcept
        {
            // D1: incoming CompData PassDirty upgrades to dirty: wire UC_PD (the spec's
            //     unencodable CompData_UD_PD rows) lands on UD in the transition tables.
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                case Resps::UC_PD:      return CacheResps::UC_PD;
                default:                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromComp(Resp resp) noexcept
        {
            // Dataless Comp on DnRSP (Comp_UC for MakeUnique / ReadUnique-dataless,
            // Resp = I assumed for Evict - D12: Comp carries no state on Evict).
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                case Resps::I_PD:       return CacheResps::I_PD;
                case Resps::SC_PD:      return CacheResps::SC_PD;
                case Resps::UC_PD:      return CacheResps::UC_PD;
                default:                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromCompCMO(Resp resp) noexcept
        {
            // CompCMO carries no cache-line state (D8); the full encoding space is
            // mapped so that the check degenerates to a state-membership test.
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                case Resps::I_PD:       return CacheResps::I_PD;
                case Resps::SC_PD:      return CacheResps::SC_PD;
                case Resps::UC_PD:      return CacheResps::UC_PD;
                default:                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromCompStash(Resp resp) noexcept
        {
            // CompStash carries stash feedback, not cache-line state (D9).
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                case Resps::I_PD:       return CacheResps::I_PD;
                case Resps::SC_PD:      return CacheResps::SC_PD;
                case Resps::UC_PD:      return CacheResps::UC_PD;
                default:                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpResp(Resp resp) noexcept
        {
            // SnpResp carries no data and therefore no PassDirty forms.
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                default:                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromSnpRespData(Resp resp) noexcept
        {
            // D3: outgoing SnpRespData *_PD means the dirty data went home and the
            //     final state is the clean variant (I_PD -> I, SC_PD -> SC, UC_PD -> UC).
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                case Resps::I_PD:       return CacheResps::I_PD;
                case Resps::SC_PD:      return CacheResps::SC_PD;
                case Resps::UC_PD:      return CacheResps::UC_PD;
                default:                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromCopyBackWrData(Resp resp) noexcept
        {
            // D2: outgoing writeback CopyBackWrData: wire I_PD (the spec's unencodable
            //     CopyBackWrData_UD_PD row) -> final I.
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                case Resps::I_PD:       return CacheResps::I_PD;
                case Resps::SC_PD:      return CacheResps::SC_PD;
                case Resps::UC_PD:      return CacheResps::UC_PD;
                default:                return CacheResps::None;
            }
        }

        inline constexpr CacheResp CacheResp::FromNonCopyBackWrData(Resp resp) noexcept
        {
            // D10: NonCopyBackWrData carries no state transition (I-only hardcoded
            //      check in CacheStateMap); mapped fully for completeness only.
            switch (resp)
            {
                case Resps::I:          return CacheResps::I;
                case Resps::SC:         return CacheResps::SC;
                case Resps::UC:         return CacheResps::UC;
                case Resps::I_PD:       return CacheResps::I_PD;
                case Resps::SC_PD:      return CacheResps::SC_PD;
                case Resps::UC_PD:      return CacheResps::UC_PD;
                default:                return CacheResps::None;
            }
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__BASE
