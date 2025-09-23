#pragma once

#include "base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_STATE_B__CST_BASE)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_STATE_EB__CST_BASE))

#ifdef CHI_ISSUE_B_ENABLE
#	define __CHI__CHI_XACT_STATE_B__CST_BASE
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#	define __CHI__CHI_XACT_STATE_EB__CST_BASE
#endif


/*
namespace CHI {
*/
    namespace Xact {

        struct RetToSrc {
            bool _0 : 1;
            bool _1 : 1;

            inline constexpr RetToSrc() noexcept
                : _0 (false), _1 (false)
			{ }

            inline constexpr RetToSrc(bool _0, bool _1) noexcept
				: _0(_0), _1(_1)
            { }

            inline constexpr RetToSrc operator&(RetToSrc obj) const noexcept
            { return { _0 && obj._0, _1 && obj._1 }; }

            inline constexpr operator bool() const noexcept
            { return _0 || _1; }
        };

        namespace RetToSrcs {
            static constexpr RetToSrc X     = {  true,  true };
            static constexpr RetToSrc A0    = {  true, false };
            static constexpr RetToSrc A1    = { false,  true };
        };

        class CacheStateTransition {
        public:
            enum class Type {
                Dataless = 0,
                Read,
                MakeReadUnique,
                WriteCopyBack,
                WriteNonCopyBack,
                Atomic,
                Snoop,
                SnoopForward
            };

        public:
            Type        type;

            CacheState  initialExpectedState;
            CacheState  initialPermittedState;
            CacheState  finalState;

            CacheResp   respComp;
            CacheResp   respCompData;
            CacheResp   respDataSepResp;
            CacheResp   respCopyBackWrData;
            CacheResp   respSnpResp;
            CacheResp   respSnpRespData;
            CacheResp   respSnpRespDataPtl;

            RetToSrc    retToSrc;
            bool        dataPull;

            bool        noChange;               // keep state unchanged regardless of final resp

        public:
            inline constexpr CacheStateTransition() noexcept
                : type                  (Type::Dataless)
                , initialExpectedState  ()
                , initialPermittedState ()
                , finalState            ()
                , respComp              ()
                , respCompData          ()
                , respDataSepResp       ()
                , respCopyBackWrData    ()
                , respSnpResp           ()
                , respSnpRespData       ()
                , respSnpRespDataPtl    ()
                , retToSrc              (RetToSrcs::X)
                , dataPull              (false)
                , noChange              (false)
            { }

            inline constexpr CacheStateTransition(CacheState initialExpectedState) noexcept
                : type                  (Type::Dataless)
                , initialExpectedState  (initialExpectedState)
                , initialPermittedState ()
                , finalState            ()
                , respComp              ()
                , respCompData          ()
                , respDataSepResp       ()
                , respCopyBackWrData    ()
                , respSnpResp           ()
                , respSnpRespData       ()
                , respSnpRespDataPtl    ()
                , retToSrc              (RetToSrcs::X)
                , dataPull              (false)
                , noChange              (false)
            { }

            inline constexpr CacheStateTransition(CacheState initialExpectedState, CacheState finalState) noexcept
                : type                  (Type::Dataless)
                , initialExpectedState  (initialExpectedState)
                , initialPermittedState ()
                , finalState            (finalState)
                , respComp              ()
                , respCompData          ()
                , respDataSepResp       ()
                , respCopyBackWrData    ()
                , respSnpResp           ()
                , respSnpRespData       ()
                , respSnpRespDataPtl    ()
                , retToSrc              (RetToSrcs::X)
                , dataPull              (false)
                , noChange              (false)
            { }

            inline constexpr CacheStateTransition(CacheState initialExpectedState, CacheState initialPermittedState, CacheState finalState) noexcept
                : type                  (Type::Dataless)
                , initialExpectedState  (initialExpectedState)
                , initialPermittedState (initialPermittedState)
                , finalState            (finalState)
                , respComp              ()
                , respCompData          ()
                , respDataSepResp       ()
                , respCopyBackWrData    ()
                , respSnpResp           ()
                , respSnpRespData       ()
                , respSnpRespDataPtl    ()
                , retToSrc              (RetToSrcs::X)
                , dataPull              (false)
                , noChange              (false)
            { }

            inline constexpr CacheStateTransition(
                Type        type,
                CacheState  initialExpectedState,
                CacheState  initialPermittedState,
                CacheState  finalState,
                CacheResp   respComp,
                CacheResp   respCompData,
                CacheResp   respDataSepResp,
                CacheResp   respCopyBackWrData,
                CacheResp   respSnpResp,
                CacheResp   respSnpRespData,
                CacheResp   respSnpRespDataPtl,
                RetToSrc    retToSrc,
                bool        dataPull,
                bool        noChange
            ) noexcept
                : type                  (type)
                , initialExpectedState  (initialExpectedState)
                , initialPermittedState (initialPermittedState)
                , finalState            (finalState)
                , respComp              (respComp)
                , respCompData          (respCompData)
                , respDataSepResp       (respDataSepResp)
                , respCopyBackWrData    (respCopyBackWrData)
                , respSnpResp           (respSnpResp)
                , respSnpRespData       (respSnpRespData)
                , respSnpRespDataPtl    (respSnpRespDataPtl)
                , retToSrc              (retToSrc)
                , dataPull              (dataPull)
                , noChange              (noChange)
            { }

            inline constexpr CacheStateTransition RetToSrc(RetToSrc retToSrc) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition DataPull(bool dataPull = true) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition NoChange(bool noChange = true) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition Comp(CacheResp respComp) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition CompData(CacheResp respCompData) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition DataSepResp(CacheResp respDataSepResp) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition CopyBackWrData(CacheResp respCopyBackWrData) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition SnpResp(CacheResp respSnpResp) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition SnpRespData(CacheResp respSnpRespData) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }
        
            inline constexpr CacheStateTransition SnpRespDataPtl(CacheResp respSnpRespDataPtl) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition SnpRespFwded(CacheResp respSnpResp, CacheResp respCompData) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition SnpRespDataFwded(CacheResp respSnpRespData, CacheResp respCompData) const noexcept
            { return { type, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition TypeDataless() const noexcept
            { return { Type::Dataless, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition TypeRead() const noexcept
            { return { Type::Read, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition TypeMakeReadUnique() const noexcept
            { return { Type::MakeReadUnique, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }
        
            inline constexpr CacheStateTransition TypeWriteCopyBack() const noexcept
            { return { Type::WriteCopyBack, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition TypeWriteNonCopyBack() const noexcept
            { return { Type::WriteNonCopyBack, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition TypeAtomic() const noexcept
            { return { Type::Atomic, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition TypeSnoop() const noexcept
            { return { Type::Snoop, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheStateTransition TypeSnoopForward() const noexcept
            { return { Type::SnoopForward, initialExpectedState, initialPermittedState, finalState, respComp, respCompData, respDataSepResp, respCopyBackWrData, respSnpResp, respSnpRespData, respSnpRespDataPtl, retToSrc, dataPull, noChange }; }

            inline constexpr CacheState GetPermittedInitials() const noexcept
            { if (type == Type::Dataless || type == Type::Read) { return initialExpectedState | initialPermittedState; } else { return initialExpectedState; } };
        };

        namespace CacheStateTransitions {

            //
            static constexpr CacheState UC      = CacheStates::UC;
            static constexpr CacheState UCE     = CacheStates::UCE;
            static constexpr CacheState UD      = CacheStates::UD;
            static constexpr CacheState UDP     = CacheStates::UDP;
            static constexpr CacheState SC      = CacheStates::SC;
            static constexpr CacheState SD      = CacheStates::SD;
            static constexpr CacheState I       = CacheStates::I;
            //
            static constexpr CacheResp  UC_PD   = CacheResps::UC_PD;
            static constexpr CacheResp  UCE_PD  = CacheResps::UCE_PD;
            static constexpr CacheResp  UD_PD   = CacheResps::UD_PD;
            static constexpr CacheResp  UDP_PD  = CacheResps::UDP_PD;
            static constexpr CacheResp  SC_PD   = CacheResps::SC_PD;
            static constexpr CacheResp  SD_PD   = CacheResps::SD_PD;
            static constexpr CacheResp  I_PD    = CacheResps::I_PD;
        }
    }
/*
}
*/

#endif
