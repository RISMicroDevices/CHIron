#pragma once

#ifndef __CCHI__CCHI_XACT_BASE__DENIAL
#define __CCHI__CCHI_XACT_BASE__DENIAL


namespace CCHI {

    class XactDenialEnumBack {
    public:
        const char* name;
        const int   value;

    public:
        inline constexpr XactDenialEnumBack(const char* name, const int value) noexcept
        : name(name), value(value) { }

    public:
        inline constexpr operator int() const noexcept
        { return value; }

        inline constexpr operator const XactDenialEnumBack*() const noexcept
        { return this; }

        inline constexpr bool operator==(const XactDenialEnumBack& obj) const noexcept
        { return value == obj.value; }

        inline constexpr bool operator!=(const XactDenialEnumBack& obj) const noexcept
        { return !(*this == obj); }
    };

    using XactDenialEnum = const XactDenialEnumBack*;

    namespace XactDenial {
        inline constexpr XactDenialEnumBack NOT_INITIALIZED                         ("XACT_NOT_INITIALIZED",                        0xFFFF0000 |  0);

        inline constexpr XactDenialEnumBack ACCEPTED                                ("XACT_ACCEPTED",                               0x00000000 |  0);

        inline constexpr XactDenialEnumBack DENIED_CHANNEL_NOT_EVT                  ("XACT_DENIED_CHANNEL_NOT_EVT",                 0x00000000 | 32);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_NOT_SNP                  ("XACT_DENIED_CHANNEL_NOT_SNP",                 0x00000000 | 33);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_NOT_REQ                  ("XACT_DENIED_CHANNEL_NOT_REQ",                 0x00000000 | 34);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_NOT_DNRSP                ("XACT_DENIED_CHANNEL_NOT_DNRSP",               0x00000000 | 35);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_NOT_UPRSP                ("XACT_DENIED_CHANNEL_NOT_UPRSP",               0x00000000 | 36);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_NOT_DNDAT                ("XACT_DENIED_CHANNEL_NOT_DNDAT",               0x00000000 | 37);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_NOT_UPDAT                ("XACT_DENIED_CHANNEL_NOT_UPDAT",               0x00000000 | 38);

        inline constexpr XactDenialEnumBack DENIED_CHANNEL_EVT                      ("XACT_DENIED_CHANNEL_EVT",                     0x00000000 | 40);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_SNP                      ("XACT_DENIED_CHANNEL_SNP",                     0x00000000 | 41);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_REQ                      ("XACT_DENIED_CHANNEL_REQ",                     0x00000000 | 42);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_DNRSP                    ("XACT_DENIED_CHANNEL_DNRSP",                   0x00000000 | 43);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_UPRSP                    ("XACT_DENIED_CHANNEL_UPRSP",                   0x00000000 | 44);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_DNDAT                    ("XACT_DENIED_CHANNEL_DNDAT",                   0x00000000 | 45);
        inline constexpr XactDenialEnumBack DENIED_CHANNEL_UPDAT                    ("XACT_DENIED_CHANNEL_UPDAT",                   0x00000000 | 46);

        inline constexpr XactDenialEnumBack DENIED_COMPLETED_DNRSP                  ("XACT_DENIED_COMPLETED_DNRSP",                 0x00000000 | 48);
        inline constexpr XactDenialEnumBack DENIED_COMPLETED_UPRSP                  ("XACT_DENIED_COMPLETED_UPRSP",                 0x00000000 | 49);
        inline constexpr XactDenialEnumBack DENIED_COMPLETED_DNDAT                  ("XACT_DENIED_COMPLETED_DNDAT",                 0x00000000 | 50);
        inline constexpr XactDenialEnumBack DENIED_COMPLETED_UPDAT                  ("XACT_DENIED_COMPLETED_UPDAT",                 0x00000000 | 51);

        inline constexpr XactDenialEnumBack DENIED_EVT_TXNID_IN_USE                 ("XACT_DENIED_EVT_TXNID_IN_USE",                0x00000000 | 52);
        inline constexpr XactDenialEnumBack DENIED_SNP_TXNID_IN_USE                 ("XACT_DENIED_SNP_TXNID_IN_USE",                0x00000000 | 53);
        inline constexpr XactDenialEnumBack DENIED_REQ_TXNID_IN_USE                 ("XACT_DENIED_REQ_TXNID_IN_USE",                0x00000000 | 54);

        inline constexpr XactDenialEnumBack DENIED_DNRSP_DBID_IN_USE                ("XACT_DENIED_DNRSP_DBID_IN_USE",               0x00000000 | 55);
        inline constexpr XactDenialEnumBack DENIED_DNDAT_DBID_IN_USE                ("XACT_DENIED_DNDAT_DBID_IN_USE",               0x00000000 | 56);

        inline constexpr XactDenialEnumBack DENIED_UPRSP_TXNID_NOT_EXIST            ("XACT_DENIED_UPRSP_TXNID_NOT_EXIST",           0x00000000 | 57);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_TXNID_NOT_EXIST            ("XACT_DENIED_UPDAT_TXNID_NOT_EXIST",           0x00000000 | 58);
        inline constexpr XactDenialEnumBack DENIED_DNRSP_TXNID_NOT_EXIST            ("XACT_DENIED_DNRSP_TXNID_NOT_EXIST",           0x00000000 | 59);
        inline constexpr XactDenialEnumBack DENIED_DNDAT_TXNID_NOT_EXIST            ("XACT_DENIED_DNDAT_TXNID_NOT_EXIST",           0x00000000 | 60);

        inline constexpr XactDenialEnumBack DENIED_EVT_OPCODE                       ("XACT_DENIED_EVT_OPCODE",                      0x00010000 |  0);
        inline constexpr XactDenialEnumBack DENIED_SNP_OPCODE                       ("XACT_DENIED_SNP_OPCODE",                      0x00010000 |  1);
        inline constexpr XactDenialEnumBack DENIED_REQ_OPCODE                       ("XACT_DENIED_REQ_OPCODE",                      0x00010000 |  2);
        inline constexpr XactDenialEnumBack DENIED_DNRSP_OPCODE                     ("XACT_DENIED_DNRSP_OPCODE",                    0x00010000 |  3);
        inline constexpr XactDenialEnumBack DENIED_UPRSP_OPCODE                     ("XACT_DENIED_UPRSP_OPCODE",                    0x00010000 |  4);
        inline constexpr XactDenialEnumBack DENIED_DNDAT_OPCODE                     ("XACT_DENIED_DNDAT_OPCODE",                    0x00010000 |  5);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_OPCODE                     ("XACT_DENIED_UPDAT_OPCODE",                    0x00010000 |  6);
        inline constexpr XactDenialEnumBack DENIED_EVT_OPCODE_NOT_SUPPORTED         ("XACT_DENIED_EVT_OPCODE_NOT_SUPPORTED",        0x00010000 |  7);
        inline constexpr XactDenialEnumBack DENIED_SNP_OPCODE_NOT_SUPPORTED         ("XACT_DENIED_SNP_OPCODE_NOT_SUPPORTED",        0x00010000 |  8);
        inline constexpr XactDenialEnumBack DENIED_REQ_OPCODE_NOT_SUPPORTED         ("XACT_DENIED_REQ_OPCODE_NOT_SUPPORTED",        0x00010000 |  9);
        inline constexpr XactDenialEnumBack DENIED_EVT_OPCODE_NOT_DECODED           ("XACT_DENIED_EVT_OPCODE_NOT_DECODED",          0x00010000 | 10);
        inline constexpr XactDenialEnumBack DENIED_SNP_OPCODE_NOT_DECODED           ("XACT_DENIED_SNP_OPCODE_NOT_DECODED",          0x00010000 | 11);
        inline constexpr XactDenialEnumBack DENIED_REQ_OPCODE_NOT_DECODED           ("XACT_DENIED_REQ_OPCODE_NOT_DECODED",          0x00010000 | 12);

        inline constexpr XactDenialEnumBack DENIED_UPRSP_TXNID_MISMATCHING_DBID     ("XACT_DENIED_UPRSP_TXNID_MISMATCHING_DBID",    0x00040000 |  0);
        inline constexpr XactDenialEnumBack DENIED_UPRSP_TXNID_MISMATCHING_DNRSP    ("XACT_DENIED_UPRSP_TXNID_MISMATCHING_DNRSP",   0x00040000 |  1);
        inline constexpr XactDenialEnumBack DENIED_UPRSP_TXNID_MISMATCHING_DNDAT    ("XACT_DENIED_UPRSP_TXNID_MISMATCHING_DNDAT",   0x00040000 |  2);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_TXNID_MISMATCHING_DBID     ("XACT_DENIED_UPDAT_TXNID_MISMATCHING_DBID",    0x00040000 |  3);
        inline constexpr XactDenialEnumBack DENIED_DNDAT_TXNID_MISMATCHING_REQ      ("XACT_DENIED_DNDAT_TXNID_MISMATCHING_REQ",     0x00040000 |  4);
        inline constexpr XactDenialEnumBack DENIED_DNRSP_TXNID_MISMATCHING_REQ      ("XACT_DENIED_DNRSP_TXNID_MISMATCHING_REQ",     0x00040000 |  5);
        inline constexpr XactDenialEnumBack DENIED_DNRSP_TXNID_MISMATCHING_EVT      ("XACT_DENIED_DNRSP_TXNID_MISMATCHING_EVT",     0x00040000 |  6);
        inline constexpr XactDenialEnumBack DENIED_UPRSP_TXNID_MISMATCHING_SNP      ("XACT_DENIED_UPRSP_TXNID_MISMATCHING_SNP",     0x00040000 |  7);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_TXNID_MISMATCHING_SNP      ("XACT_DENIED_UPDAT_TXNID_MISMATCHING_SNP",     0x00040000 |  8);

        inline constexpr XactDenialEnumBack DENIED_DNRSP_DBID_MISMATCH              ("XACT_DENIED_DNRSP_DBID_MISMATCHING_REQ",      0x00040000 | 12);
        inline constexpr XactDenialEnumBack DENIED_DNDAT_DBID_MISMATCH              ("XACT_DENIED_DNDAT_DBID_MISMATCHING_REQ",      0x00040000 | 13);

        inline constexpr XactDenialEnumBack DENIED_DNRSP_TGTID_MISMATCHING_REQ      ("XACT_DENIED_DNRSP_TGTID_MISMATCHING_REQ",     0x00040000 | 32);
        inline constexpr XactDenialEnumBack DENIED_DNDAT_TGTID_MISMATCHING_REQ      ("XACT_DENIED_DNDAT_TGTID_MISMATCHING_REQ",     0x00040000 | 33);
        inline constexpr XactDenialEnumBack DENIED_DNRSP_TGTID_MISMATCHING_EVT      ("XACT_DENIED_DNRSP_TGTID_MISMATCHING_EVT",     0x00040000 | 34);
        inline constexpr XactDenialEnumBack DENIED_UPRSP_TGTID_MISMATCHING_DNRSP    ("XACT_DENIED_UPRSP_TGTID_MISMATCHING_DNRSP",   0x00040000 | 36);
        inline constexpr XactDenialEnumBack DENIED_UPRSP_TGTID_MISMATCHING_DNDAT    ("XACT_DENIED_UPRSP_TGTID_MISMATCHING_DNDAT",   0x00040000 | 37);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_TGTID_MISMATCHING_DNRSP    ("XACT_DENIED_UPDAT_TGTID_MISMATCHING_DNRSP",   0x00040000 | 38);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_TGTID_MISMATCHING_DNDAT    ("XACT_DENIED_UPDAT_TGTID_MISMATCHING_DNDAT",   0x00040000 | 39);
        inline constexpr XactDenialEnumBack DENIED_UPRSP_TGTID_MISMATCHING_SNP      ("XACT_DENIED_UPRSP_TGTID_MISMATCHING_SNP",     0x00040000 | 40);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_TGTID_MISMATCHING_SNP      ("XACT_DENIED_UPDAT_TGTID_MISMATCHING_SNP",     0x00040000 | 41);

        inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_DBID              ("XACT_DENIED_COMPACK_BEFORE_DBID",             0x000B0000 |  0);
        inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_COMPDATA          ("XACT_DENIED_COMPACK_BEFORE_COMPDATA",         0x000B0000 |  1);
        inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_COMPDATA_OR_COMP  ("XACT_DENIED_COMPACK_BEFORE_COMPDATA_OR_COMP", 0x000B0000 |  2);
        inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_COMP              ("XACT_DENIED_COMPACK_BEFORE_COMP",             0x000B0000 |  3);
        inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMP                  ("XACT_DENIED_COMP_AFTER_COMP",                 0x000B0000 |  4);
        inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMPDATA              ("XACT_DENIED_COMP_AFTER_COMPDATA",             0x000B0000 |  5);
        inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMPDBIDRESP          ("XACT_DENIED_COMP_AFTER_COMPDBIDRESP",         0x000B0000 |  6);
        inline constexpr XactDenialEnumBack DENIED_COMP_ON_EXPCOMPDATA              ("XACT_DENIED_COMP_ON_EXPCOMPDATA",             0x000B0000 |  7);
        inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_COMP              ("XACT_DENIED_COMPDATA_AFTER_COMP",             0x000B0000 |  8);
        inline constexpr XactDenialEnumBack DENIED_DBIDRESP_AFTER_COMPDBIDRESP      ("XACT_DENIED_DBIDRESP_AFTER_COMPDBIDRESP",     0x000B0000 |  9);
        inline constexpr XactDenialEnumBack DENIED_DBIDRESP_AFTER_DBIDRESP          ("XACT_DENIED_DBIDRESP_AFTER_DBIDRESP",         0x000B0000 | 10);
        inline constexpr XactDenialEnumBack DENIED_COMPDBIDRESP_AFTER_COMP          ("XACT_DENIED_COMPDBIDRESP_AFTER_COMP",         0x000B0000 | 11);
        inline constexpr XactDenialEnumBack DENIED_COMPDBIDRESP_AFTER_DBIDRESP      ("XACT_DENIED_COMPDBIDRESP_AFTER_DBIDRESP",     0x000B0000 | 12);
        inline constexpr XactDenialEnumBack DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP  ("XACT_DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP", 0x000B0000 | 13);
        inline constexpr XactDenialEnumBack DENIED_COMPSTASH_NOT_EXPECTED           ("XACT_DENIED_COMPSTASH_NOT_EXPECTED",          0x000B0000 | 14);
        inline constexpr XactDenialEnumBack DENIED_SNPRESP_AFTER_SNPRESPDATA        ("XACT_DENIED_SNPRESP_AFTER_SNPRESPDATA",       0x000B0000 | 15);
        inline constexpr XactDenialEnumBack DENIED_SNPRESPDATA_AFTER_SNPRESP        ("XACT_DENIED_SNPRESPDATA_AFTER_SNPRESP",       0x000B0000 | 16);

        inline constexpr XactDenialEnumBack DENIED_DNDAT_DUPLICATED_DATAID          ("XACT_DENIED_DNDAT_DUPLICATED_DATAID",         0x000B0000 | 78);
        inline constexpr XactDenialEnumBack DENIED_UPDAT_DUPLICATED_DATAID          ("XACT_DENIED_UPDAT_DUPLICATED_DATAID",         0x000B0000 | 79);
        inline constexpr XactDenialEnumBack DENIED_DATA_BEFORE_DBID                 ("XACT_DENIED_DATA_BEFORE_DBID",                0x000B0000 | 80);

        inline constexpr XactDenialEnumBack DENIED_COMPDATA_RESP_MISMATCH           ("XACT_DENIED_COMPDATA_RESP_MISMATCH",          0x000B0000 | 81);
    }
}

#endif // __CCHI__CCHI_XACT_BASE__DENIAL
