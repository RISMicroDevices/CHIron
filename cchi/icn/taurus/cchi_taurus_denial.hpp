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
        const bool  isDone;

    public:
        inline constexpr DenialEnumBack(const char* name, const int value, bool isAccepted = false, bool isRejected = false, bool isDone = false) noexcept
        : name(name), value(value), isAccepted(isAccepted), isRejected(isRejected), isDone(isDone) { }

    public:
        inline constexpr bool IsDone() const noexcept
        { return isDone; }

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

        inline constexpr DenialEnumBack DONE                                ("DONE",                            0x00000000 |  0, false, false, true);
        inline constexpr DenialEnumBack ACCEPTED                            ("ACCEPTED",                        0x00000000 |  1, true , false);

        inline constexpr DenialEnumBack REJECTED                            ("REJECTED",                        0x00010000 |  0, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_TXNID_BUSY          ("REJECTED_TAURUS_TXNID_BUSY",      0x00010000 |  1, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_PA_REQ_BUSY         ("REJECTED_TAURUS_PA_REQ_BUSY",     0x00010000 |  2, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_PA_EVT_BUSY         ("REJECTED_TAURUS_PA_EVT_BUSY",     0x00010000 |  3, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_EVICT_MISS          ("REJECTED_TAURUS_EVICT_MISS",      0x00010000 |  4, false, true );

        inline constexpr DenialEnumBack REJECTED_TAURUS_EVT_LIMIT_EXCEEDED  ("REJECTED_TAURUS_EVT_LIMIT_EXCEEDED", 0x00010000 |  5, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_SNP_LIMIT_EXCEEDED  ("REJECTED_TAURUS_SNP_LIMIT_EXCEEDED", 0x00010000 |  6, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_REQ_LIMIT_EXCEEDED  ("REJECTED_TAURUS_REQ_LIMIT_EXCEEDED", 0x00010000 |  7, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED("REJECTED_TAURUS_TOTAL_LIMIT_EXCEEDED", 0x00010000 |  8, false, true );

        inline constexpr DenialEnumBack REJECTED_TAURUS_PREFETCH_LIMIT_EXCEEDED("REJECTED_TAURUS_PREFETCH_LIMIT_EXCEEDED", 0x00010000 |  9, false, true );
        inline constexpr DenialEnumBack REJECTED_TAURUS_CMO_LIMIT_EXCEEDED("REJECTED_TAURUS_CMO_LIMIT_EXCEEDED", 0x00010000 | 10, false, true );

        inline constexpr DenialEnumBack REJECTED_TAURUS_PROTOCOL_DENIED    ("REJECTED_TAURUS_PROTOCOL_DENIED", 0x00010000 | 11, false, true );

        inline constexpr DenialEnumBack REJECTED_TAURUS_EVENT               ("REJECTED_TAURUS_EVENT",           0x00020000 |  0, false, true );
    }
}


#endif // __CCHI__CCHI_ICN_TAURUS__DENIAL
