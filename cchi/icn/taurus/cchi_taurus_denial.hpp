#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__DENIAL
#define __CCHI__CCHI_ICN_TAURUS__DENIAL


namespace CCHI::Taurus {

    class DenialEnumBack {
    public:
        const char* name;
        const int   value;
        const bool  isAccepted;
        const bool  isRejected;

    public:
        inline constexpr DenialEnumBack(const char* name, const int value, bool isAccepted = false, bool isRejected = false) noexcept
        : name(name), value(value), isAccepted(isAccepted), isRejected(isRejected) { }

    public:
        inline constexpr bool IsDone() const noexcept
        { return !isAccepted && !isRejected; }

        inline constexpr bool IsAccepted() const noexcept
        { return isAccepted; }

        inline constexpr bool IsRejected() const noexcept
        { return isRejected; }

    public:
        inline constexpr operator int() const noexcept
        { return value; }

        inline constexpr operator const DenialEnumBack*() const noexcept
        { return this; }

        inline constexpr bool operator==(const DenialEnumBack& obj) const noexcept
        { return value == obj.value; }

        inline constexpr bool operator!=(const DenialEnumBack& obj) const noexcept
        { return !(*this == obj); }
    };

    using DenialEnum = const DenialEnumBack*;

    namespace Denial {
        inline constexpr DenialEnumBack NOT_INITIALIZED                     ("NOT_INITIALIZED",                 0xFFFF0000 |  0, false, false);

        inline constexpr DenialEnumBack DONE                                ("DONE",                            0x00000000 |  0, false, false);
        inline constexpr DenialEnumBack ACCEPTED                            ("ACCEPTED",                        0x00000000 |  1, true , false);

        inline constexpr DenialEnumBack REJECTED                            ("REJECTED",                        0x00010000 |  0, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_TXNID_BUSY          ("REJECTED_TAURUS_TXNID_BUSY",      0x00010000 |  1, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_PA_REQ_BUSY         ("REJECTED_TAURUS_PA_REQ_BUSY",     0x00010000 |  2, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_PA_EVT_BUSY         ("REJECTED_TAURUS_PA_EVT_BUSY",     0x00010000 |  3, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_EVICT_MISS          ("REJECTED_TAURUS_EVICT_MISS",      0x00010000 |  4, false, true );
    }
}


#endif // __CCHI__CCHI_ICN_TAURUS__DENIAL
