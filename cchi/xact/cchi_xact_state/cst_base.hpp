#pragma once

#ifndef __CCHI__CCHI_XACT_STATE__CST_BASE
#define __CCHI__CCHI_XACT_STATE__CST_BASE

#include "base.hpp"


namespace CCHI {
    namespace Xact {

        /*
        CacheStateTransition: one authored row of a request/snoop state-transition table.

        For request (EVT/REQ) transactions:
            initial     = Requester cache state when the request is sent
                          (spec: request_types.md "请求发送时允许的状态",
                           structure.md "初始状态" column).
            final       = Requester cache state after the corresponding response
                          (structure.md "最终状态" column).

        For snoop (SNP) transactions (structure.md "Snoop" table, :306-346):
            startState  = Completer cache state when the snoop arrives
                          ("起始状态" column) - drives the G2 nested-transfer tables.
            initial     = Completer cache state at the time the snoop response is sent
                          ("回复前状态" column, may have been changed by nested
                           evictions/writebacks) - drives the G0 response tables.
            final       = Completer cache state after the snoop response
                          ("最终状态" column).

        For CMO transactions the two uses of 'initial' diverge (spec:
        request_types.md CMO send matrix allows all four states at send time,
        while structure.md :185-194 constrains the state at CompCMO):
            initialSend = Requester cache state when the CMO is sent
                          (request_types.md send matrix) - drives the Initials::
                          map-reduce only. Empty means: same as 'initial'.
            initial     = the compliant at-CompCMO states (structure.md table
                          under the CompCMO no-transition note) - keys the G0
                          CompCMO compliance check.
        */
        class CacheStateTransition {
        public:
            enum class Type {
                Read = 0,
                Evict,
                WriteBack,
                Dataless,
                CMO,
                Stash,
                WriteNonCopyBack,
                Snoop
            };

        public:
            Type        type;

            CacheState  startState;
            CacheState  initial;
            CacheState  initialSend;
            CacheState  final;

            CacheResp   respComp;
            CacheResp   respCompData;
            CacheResp   respCompCMO;
            CacheResp   respCompStash;
            CacheResp   respSnpResp;
            CacheResp   respSnpRespData;
            CacheResp   respCopyBackWrData;
            CacheResp   respNonCopyBackWrData;

        public:
            inline constexpr CacheStateTransition() noexcept
                : type                  (Type::Dataless)
                , startState            ()
                , initial               ()
                , initialSend           ()
                , final                 ()
                , respComp              ()
                , respCompData          ()
                , respCompCMO           ()
                , respCompStash         ()
                , respSnpResp           ()
                , respSnpRespData       ()
                , respCopyBackWrData    ()
                , respNonCopyBackWrData ()
            { }

            inline constexpr CacheStateTransition(CacheState initial) noexcept
                : type                  (Type::Dataless)
                , startState            ()
                , initial               (initial)
                , initialSend           ()
                , final                 ()
                , respComp              ()
                , respCompData          ()
                , respCompCMO           ()
                , respCompStash         ()
                , respSnpResp           ()
                , respSnpRespData       ()
                , respCopyBackWrData    ()
                , respNonCopyBackWrData ()
            { }

            inline constexpr CacheStateTransition(CacheState initial, CacheState final) noexcept
                : type                  (Type::Dataless)
                , startState            ()
                , initial               (initial)
                , initialSend           ()
                , final                 (final)
                , respComp              ()
                , respCompData          ()
                , respCompCMO           ()
                , respCompStash         ()
                , respSnpResp           ()
                , respSnpRespData       ()
                , respCopyBackWrData    ()
                , respNonCopyBackWrData ()
            { }

        public:
            inline constexpr CacheStateTransition Start(CacheState startState) const noexcept
            { CacheStateTransition c = *this; c.startState = startState; return c; }

            // send-time initial state set when it differs from 'initial' (CMO only);
            // consumed by the Initials:: map-reduce, never by the G0 tables
            inline constexpr CacheStateTransition SendInitial(CacheState initialSend) const noexcept
            { CacheStateTransition c = *this; c.initialSend = initialSend; return c; }

            inline constexpr CacheStateTransition Comp(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respComp = resp; return c; }

            inline constexpr CacheStateTransition CompData(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respCompData = resp; return c; }

            inline constexpr CacheStateTransition CompCMO(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respCompCMO = resp; return c; }

            inline constexpr CacheStateTransition CompStash(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respCompStash = resp; return c; }

            inline constexpr CacheStateTransition SnpResp(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respSnpResp = resp; return c; }

            inline constexpr CacheStateTransition SnpRespData(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respSnpRespData = resp; return c; }

            inline constexpr CacheStateTransition CopyBackWrData(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respCopyBackWrData = resp; return c; }

            inline constexpr CacheStateTransition NonCopyBackWrData(CacheResp resp) const noexcept
            { CacheStateTransition c = *this; c.respNonCopyBackWrData = resp; return c; }

            inline constexpr CacheStateTransition TypeRead() const noexcept
            { CacheStateTransition c = *this; c.type = Type::Read; return c; }

            inline constexpr CacheStateTransition TypeEvict() const noexcept
            { CacheStateTransition c = *this; c.type = Type::Evict; return c; }

            inline constexpr CacheStateTransition TypeWriteBack() const noexcept
            { CacheStateTransition c = *this; c.type = Type::WriteBack; return c; }

            inline constexpr CacheStateTransition TypeDataless() const noexcept
            { CacheStateTransition c = *this; c.type = Type::Dataless; return c; }

            inline constexpr CacheStateTransition TypeCMO() const noexcept
            { CacheStateTransition c = *this; c.type = Type::CMO; return c; }

            inline constexpr CacheStateTransition TypeStash() const noexcept
            { CacheStateTransition c = *this; c.type = Type::Stash; return c; }

            inline constexpr CacheStateTransition TypeWriteNonCopyBack() const noexcept
            { CacheStateTransition c = *this; c.type = Type::WriteNonCopyBack; return c; }

            inline constexpr CacheStateTransition TypeSnoop() const noexcept
            { CacheStateTransition c = *this; c.type = Type::Snoop; return c; }
        };

        namespace CacheStateTransitions {

            //
            static constexpr CacheState UC      = CacheStates::UC;
            static constexpr CacheState UD      = CacheStates::UD;
            static constexpr CacheState SC      = CacheStates::SC;
            static constexpr CacheState I       = CacheStates::I;
            //
            static constexpr CacheResp  I_PD    = CacheResps::I_PD;
            static constexpr CacheResp  SC_PD   = CacheResps::SC_PD;
            static constexpr CacheResp  UC_PD   = CacheResps::UC_PD;
        }
    }
}


#endif // __CCHI__CCHI_XACT_STATE__CST_BASE
