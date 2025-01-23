//#pragma once

//#ifndef __CHI__CHI_XACT_BASE
//#define __CHI__CHI_XACT_BASE

#ifndef CHI_XACT_BASE__STANDALONE
#   include "chi_xact_base_header.hpp"                  // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding_header.hpp"  // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding.hpp"         // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_BASE_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_BASE_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_BASE_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_BASE_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {

        class XactScopeEnumBack {
        public:
            const char* name;
            const int   value;

        public:
            inline constexpr XactScopeEnumBack(const char* name, const int value) noexcept
            : name(name), value(value) { }

        public:
            inline constexpr operator int() const noexcept
            { return value; }

            inline constexpr operator const XactScopeEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const XactScopeEnumBack& obj) const noexcept
            { return value == obj.value; }

            inline constexpr bool operator!=(const XactScopeEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using XactScopeEnum = const XactScopeEnumBack*;

        namespace XactScope {
            inline constexpr XactScopeEnumBack  Requester   ("Requester",   1);
            inline constexpr XactScopeEnumBack  Home        ("Home",        2);
            inline constexpr XactScopeEnumBack  Subordinate ("Subordinate", 3);
        }


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
            inline constexpr XactDenialEnumBack NOT_INITIALIZED                     ("XACT_NOT_INITIALIZED",                    0xFFFF0000 |  0);

            inline constexpr XactDenialEnumBack ACCEPTED                            ("XACT_ACCEPTED",                           0x00000000 |  0);
            inline constexpr XactDenialEnumBack DENIED_COMPLETED                    ("XACT_DENIED_COMPLETED",                   0x00000000 |  1);
            inline constexpr XactDenialEnumBack DENIED_SCOPE                        ("XACT_DENIED_SCOPE",                       0x00000000 |  2);
            inline constexpr XactDenialEnumBack DENIED_CHANNEL                      ("XACT_DENIED_CHANNEL",                     0x00000000 |  3);
            inline constexpr XactDenialEnumBack DENIED_COMMUNICATION                ("XACT_DENIED_COMMUNICATION",               0x00000000 |  4);
            inline constexpr XactDenialEnumBack DENIED_TXNID_IN_USE                 ("XACT_DENIED_TXNID_IN_USE",                0x00000000 |  5);
            inline constexpr XactDenialEnumBack DENIED_TXNID_NOT_EXIST              ("XACT_DENIED_TXNID_NOT_EXIST",             0x00000000 |  6);
            inline constexpr XactDenialEnumBack DENIED_DBID_IN_USE                  ("XACT_DENIED_DBID_IN_USE",                 0x00000000 |  7);
            inline constexpr XactDenialEnumBack DENIED_DBID_NOT_EXIST               ("XACT_DENIED_DBID_NOT_EXIST",              0x00000000 |  8);

            inline constexpr XactDenialEnumBack DENIED_OPCODE                       ("XACT_DENIED_OPCODE",                      0x00010000 |  0);

            inline constexpr XactDenialEnumBack DENIED_REQ_NOT_TO_HN                ("XACT_DENIED_REQ_NOT_TO_HN",               0x00020000 |  0);
            inline constexpr XactDenialEnumBack DENIED_REQ_NOT_FROM_RN_TO_HN        ("XACT_DENIED_REQ_NOT_FROM_RN_TO_HN",       0x00020000 |  1);
            inline constexpr XactDenialEnumBack DENIED_REQ_NOT_FROM_SN_TO_HN        ("XACT_DENIED_REQ_NOT_FROM_SN_TO_HN",       0x00020000 |  2);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_TO_RN                ("XACT_DENIED_RSP_NOT_TO_RN",               0x00020000 |  3);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_HN_TO_RN        ("XACT_DENIED_RSP_NOT_FROM_HN_TO_RN",       0x00020000 |  4);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_TO_HN                ("XACT_DENIED_RSP_NOT_TO_HN",               0x00020000 |  5);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_RN_TO_HN        ("XACT_DENIED_RSP_NOT_FROM_RN_TO_HN",       0x00020000 |  6);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_TO_SN                ("XACT_DENIED_RSP_NOT_TO_SN",               0x00020000 |  7);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_TO_RN                ("XACT_DENIED_DAT_NOT_TO_RN",               0x00020000 |  8);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_TO_HN                ("XACT_DENIED_DAT_NOT_TO_HN",               0x00020000 |  9);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_FROM_RN_TO_HN        ("XACT_DENIED_DAT_NOT_FROM_RN_TO_HN",       0x00020000 | 10);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_TO_SN                ("XACT_DENIED_DAT_NOT_TO_SN",               0x00020000 | 11);
            inline constexpr XactDenialEnumBack DENIED_SNP_NOT_TO_RN                ("XACT_DENIED_SNP_NOT_TO_RN",               0x00020000 | 12);
            inline constexpr XactDenialEnumBack DENIED_SNP_NOT_FROM_HN_TO_RN        ("XACT_DENIED_SNP_NOT_FROM_HN_TO_RN",       0x00020000 | 13);

            inline constexpr XactDenialEnumBack DENIED_TXNID_MISMATCH               ("XACT_DENIED_TXNID_MISMATCH",              0x00040000 |  1);
            inline constexpr XactDenialEnumBack DENIED_DBID_MISMATCH                ("XACT_DENIED_DBID_MISMATCH",               0x00040000 |  2);
            inline constexpr XactDenialEnumBack DENIED_TGTID_MISMATCH               ("XACT_DENIED_TGTID_MISMATCH",              0x00040000 |  3);
            inline constexpr XactDenialEnumBack DENIED_SRCID_MISMATCH               ("XACT_DENIED_SRCID_MISMATCH",              0x00040000 |  4);
            inline constexpr XactDenialEnumBack DENIED_FWDNID_MISMATCH              ("XACT_DENIED_FWDNID_MISMATCH",             0x00040000 |  5);
            inline constexpr XactDenialEnumBack DENIED_FWDTXNID_MISMATCH            ("XACT_DENIED_FWDTXNID_MISMATCH",           0x00040000 |  6);
            inline constexpr XactDenialEnumBack DENIED_HOMENID_MISMATCH             ("XACT_DENIED_HOMENID_MISMATCH",            0x00040000 |  7);
            inline constexpr XactDenialEnumBack DENIED_PGROUPID_MISMATCH            ("XACT_DENIED_PGROUPID_MISMATCH",           0x00040000 |  8);

            inline constexpr XactDenialEnumBack DENIED_SECOND_RETRY                 ("XACT_DENIED_SECOND_RETRY",                0x000A0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_SECOND_PCRD                  ("XACT_DENIED_SECOND_PCRD",                 0x000A0000 |  1);
            inline constexpr XactDenialEnumBack DENIED_PCRD_NO_RETRY                ("XACT_DENIED_PCRD_NO_RETRY",               0x000A0000 |  2);
            inline constexpr XactDenialEnumBack DENIED_PCRD_TYPE_MISMATCH           ("XACT_DENIED_PCRD_TYPE_MISMATCH",          0x000A0000 |  3);

            inline constexpr XactDenialEnumBack DENIED_RESPSEP_AFTER_COMPDATA       ("XACT_DENIED_RESPSEP_AFTER_COMPDATA",      0x000B0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_RESPSEP_AFTER_COMP           ("XACT_DENIED_RESPSEP_AFTER_COMP",          0x000B0000 |  1);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_RESPSEP           ("XACT_DENIED_COMP_AFTER_RESPSEP",          0x000B0000 |  2);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_DATASEP           ("XACT_DENIED_COMP_AFTER_DATASEP",          0x000B0000 |  3);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMPDATA          ("XACT_DENIED_COMP_AFTER_COMPDATA",         0x000B0000 |  4);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_RESPSEP       ("XACT_DENIED_COMPDATA_AFTER_RESPSEP",      0x000B0000 |  5);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_DATASEP       ("XACT_DENIED_COMPDATA_AFTER_DATASEP",      0x000B0000 |  6);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_COMP          ("XACT_DENIED_COMPDATA_AFTER_COMP",         0x000B0000 |  7);
            inline constexpr XactDenialEnumBack DENIED_DATASEP_AFTER_COMP           ("XACT_DENIED_DATASEP_AFTER_COMP",          0x000B0000 |  8);
            inline constexpr XactDenialEnumBack DENIED_DATASEP_AFTER_COMPDATA       ("XACT_DENIED_DATASEP_AFTER_COMPDATA",      0x000B0000 |  9);
            
            inline constexpr XactDenialEnumBack DENIED_DATA_BEFORE_DBID             ("XACT_DENIED_DATA_BEFORE_DBID",            0x000B0000 | 10);
            inline constexpr XactDenialEnumBack DENIED_DATA_AFTER_COMP              ("XACT_DENIED_DATA_AFTER_COMP",             0x000B0000 | 11);
            inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_DBID          ("XACT_DENIED_COMPACK_BEFORE_DBID",         0x000B0000 | 12);
            inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_COMP          ("XACT_DENIED_COMPACK_BEFORE_COMP",         0x000B0000 | 13);
            inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_COMPDATA      ("XACT_DENIED_COMPACK_BEFORE_COMPDATA",     0x000B0000 | 14);
            inline constexpr XactDenialEnumBack DENIED_COMPACK_AFTER_DBIDRESP       ("XACT_DENIED_COMPACK_AFTER_DBIDRESP",      0x000B0000 | 15);
            inline constexpr XactDenialEnumBack DENIED_COMPACK_AFTER_COMPACK        ("XACT_DENIED_COMPACK_AFTER_COMPACK",       0x000B0000 | 16);

            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMP              ("XACT_DENIED_COMP_AFTER_COMP",             0x000B0000 | 17);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMPPERSIST       ("XACT_DENIED_COMP_AFTER_COMPPERSIST",      0x000B0000 | 18);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMPDBIDRESP      ("XACT_DENIED_COMP_AFTER_COMPDBIDRESP",     0x000B0000 | 19);
            inline constexpr XactDenialEnumBack DENIED_PERSIST_AFTER_PERSIST        ("XACT_DENIED_COMP_AFTER_PERSIST",          0x000B0000 | 20);
            inline constexpr XactDenialEnumBack DENIED_PERSIST_AFTER_COMPPERSIST    ("XACT_DENIED_COMP_AFTER_COMPPERSIST",      0x000B0000 | 21);
            inline constexpr XactDenialEnumBack DENIED_COMPPERSIST_AFTER_COMP       ("XACT_DENIED_COMPPERSIST_AFTER_COMP",      0x000B0000 | 22);
            inline constexpr XactDenialEnumBack DENIED_COMPPERSIST_AFTER_PERSIST    ("XACT_DENIED_COMPPERSIST_AFTER_PERSIST",   0x000B0000 | 23);
            inline constexpr XactDenialEnumBack DENIED_COMPPERSIST_AFTER_COMPPERSIST("XACT_DENIED_COMPPERSIST_AFTER_COMPPERSIST", 0x000B0000 | 24);
            inline constexpr XactDenialEnumBack DENIED_RESPSEP_AFTER_RESPSEP        ("XACT_DENIED_RESPSEP_AFTER_RESPSEP",       0x000B0000 | 25);
            inline constexpr XactDenialEnumBack DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP  ("XACT_DENIED_COMPDBIDRESP_AFTER_COMPDBIDRESP", 0x000B0000 | 26);
            inline constexpr XactDenialEnumBack DENIED_COMPDBIDRESP_AFTER_COMP      ("XACT_DENIED_COMPDBIDRESP_AFTER_COMP",     0x000B0000 | 27);
            inline constexpr XactDenialEnumBack DENIED_COMPDBIDRESP_AFTER_DBIDRESP  ("XACT_DENIED_COMPDBIDRESP_AFTER_DBIDRESP", 0x000B0000 | 28);
            inline constexpr XactDenialEnumBack DENIED_DBIDRESP_AFTER_DBIDRESP      ("XACT_DENIED_DBIDRESP_AFTER_DBIDRESP",     0x000B0000 | 29);
            inline constexpr XactDenialEnumBack DENIED_DBIDRESP_AFTER_COMPDBIDRESP  ("XACT_DENIED_DBIDRESP_AFTER_COMPDBIDRESP", 0x000B0000 | 30);

            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_DATAID            ("XACT_DENIED_DUPLICATED_DATAID",           0x000B0000 | 31);

            inline constexpr XactDenialEnumBack DENIED_COMPACK_ON_NON_EXPCOMPACK    ("XACT_DENIED_COMPACK_ON_NON_EXPCOMPACK",   0x000B0000 | 32);
            
            inline constexpr XactDenialEnumBack DENIED_DWT_ON_EXPCOMPACK            ("XACT_DENIED_DWT_ON_EXPCOMPACK",           0x000B0000 | 33);
            inline constexpr XactDenialEnumBack DENIED_DWT_WITH_DBIDRESPORD         ("XACT_DENIED_DWT_WITH_DBIDRESPORD",        0x000B0000 | 34);

            inline constexpr XactDenialEnumBack DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL          ("XACT_DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL",         0x000B0000 | 33);
            inline constexpr XactDenialEnumBack DENIED_NCBWRDATA_AFTER_NCBWRDATACOMPACK         ("XACT_DENIED_NCBWRDATA_AFTER_NCBWRDATACOMPACK",        0x000B0000 | 34);
            inline constexpr XactDenialEnumBack DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA          ("XACT_DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA",         0x000B0000 | 35);
            inline constexpr XactDenialEnumBack DENIED_WRITEDATACANCEL_AFTER_NCBWRDATACOMPACK   ("XACT_DENIED_WRITEDATACANCEL_AFTER_NCBWRDATACOMPACK",  0x000B0000 | 36);
            inline constexpr XactDenialEnumBack DENIED_NCBWRDATACOMPACK_AFTER_WRITEDATACANCEL   ("XACT_DENIED_NCBWRDATACOMPACK_AFTER_WRITEDATACANCEL",  0x000B0000 | 37);
            inline constexpr XactDenialEnumBack DENIED_NCBWRDATACOMPACK_AFTER_NCBWRDATA         ("XACT_DENIED_NCBWRDATACOMPACK_AFTER_NCBWRDATA",        0x000B0000 | 38);

            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_XACT_TYPE         ("XACT_DENIED_RETRY_DIFF_XACT_TYPE",        0x000C0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_RETRY_NO_ALLOWRETRY          ("XACT_DENIED_RETRY_NO_ALLOWRETRY",         0x000C0000 |  1);
            inline constexpr XactDenialEnumBack DENIED_NO_MATCHING_RETRY            ("XACT_DENIED_NO_MATCHING_RETRY",           0x000C0000 |  2);
            inline constexpr XactDenialEnumBack DENIED_NO_MATCHING_PCREDIT          ("XACT_DENIED_NO_MATCHING_PCREDIT",         0x000C0000 |  3);
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_QOS               ("XACT_DENIED_RETRY_DIFF_QOS",              0x000C0000 | 31);
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TGTID             ("XACT_DENIED_RETRY_DIFF_TGTID",            0x000C0000 | 32);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SRCID             ("XACT_DENIED_RETRY_DIFF_SRCID",            0x000C0000 | 33);
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TXNID             ("XACT_DENIED_RETRY_DIFF_TXNID",            0x000C0000 | 34);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_RETURNNID         ("XACT_DENIED_RETRY_DIFF_RETURNNID",        0x000C0000 | 35);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_STASHNID          ("XACT_DENIED_RETRY_DIFF_STASHNID",         0x000C0000 | 36);
#ifdef CHI_ISSUE_EB_ENABLE
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SLCREPHINT        ("XACT_DENIED_RETRY_DIFF_SCLREPHINT",       0x000C0000 | 37);
#endif
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_STASHNIDVALID     ("XACT_DENIED_RETRY_DIFF_STASHNIDVALID",    0x000C0000 | 38);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_ENDIAN            ("XACT_DENIED_RETRY_DIFF_ENDIAN",           0x000C0000 | 39);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_DEEP              ("XACT_DENIED_RETRY_DIFF_DEEP",             0x000C0000 | 40);
#endif
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_RETURNTXNID       ("XACT_DENIED_RETRY_DIFF_RETURNTXNID",      0x000C0000 | 41);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_STASHLPIDVALID    ("XACT_DENIED_RETRY_DIFF_STASHLPIDVALID",   0x000C0000 | 42);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_OPCODE            ("XACT_DENIED_RETRY_DIFF_OPCODE",           0x000C0000 | 43);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SIZE              ("XACT_DENIED_RETRY_DIFF_SIZE",             0x000C0000 | 44);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_ADDR              ("XACT_DENIED_RETRY_DIFF_ADDR",             0x000C0000 | 45);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_NS                ("XACT_DENIED_RETRY_DIFF_NS",               0x000C0000 | 46);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_LIKELYSHARED      ("XACT_DENIED_RETRY_DIFF_LIKELYSHARED",     0x000C0000 | 47);
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_ALLOWRETRY        ("XACT_DENIED_RETRY_DIFF_ALLOWRETRY",       0x000C0000 | 48);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_ORDER             ("XACT_DENIED_RETRY_DIFF_ORDER",            0x000C0000 | 49);
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_PCRDTYPE          ("XACT_DENIED_RETRY_DIFF_PCRDTYPE",         0x000C0000 | 50);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_MEMATTR           ("XACT_DENIED_RETRY_DIFF_MEMATTR",          0x000C0000 | 51);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SNPATTR           ("XACT_DENIED_RETRY_DIFF_SNPATTR",          0x000C0000 | 52);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_DODWT             ("XACT_DENIED_RETRY_DIFF_DODWT",            0x000C0000 | 53);
#endif
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_LPID              ("XACT_DENIED_RETRY_DIFF_LPID",             0x000C0000 | 54);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_PGROUPID          ("XACT_DENIED_RETRY_DIFF_PGROUPID",         0x000C0000 | 55);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_STASHGROUPID      ("XACT_DENIED_RETRY_DIFF_STASHGROUPID",     0x000C0000 | 56);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TAGGROUPID        ("XACT_DENIED_RETRY_DIFF_TAGGROUPID",       0x000C0000 | 57);
#endif
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_EXCL              ("XACT_DENIED_RETRY_DIFF_EXCL",             0x000C0000 | 58);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SNOOPME           ("XACT_DENIED_RETRY_DIFF_SNOOPME",          0x000C0000 | 59);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_EXPCOMPACK        ("XACT_DENIED_RETRY_DIFF_EXPCOMPACK",       0x000C0000 | 60);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TAGOP             ("XACT_DENIED_RETRY_DIFF_TAGOP",            0x000C0000 | 61);
#endif
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TRACETAG          ("XACT_DENIED_RETRY_DIFF_TRACETAG",         0x000C0000 | 62);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_MPAM              ("XACT_DENIED_RETRY_DIFF_MPAM",             0x000C0000 | 63);
#endif
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_RSVDC             ("XACT_DENIED_RETRY_DIFF_RSVDC",            0x000C0000 | 64);

            inline constexpr XactDenialEnumBack DENIED_RETRY_ON_ACTIVE_PROGRESS     ("XACT_DENIED_RETRY_ON_ACTIVE_PROGRESS",    0x000C0000 | 72);

            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_SNPRESP           ("XACT_DENIED_DUPLICATED_SNPRESP",          0x000D0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_SNPRESPDATA       ("XACT_DENIED_DUPLICATED_SNPRESPDATA",      0x000D0000 |  1);
            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_READRECEIPT       ("XACT_DENIED_DUPLICATED_READRECEIPT",      0x000D0000 |  2);

            inline constexpr XactDenialEnumBack DENIED_UNSUPPORTED_FEATURE          ("XACT_DENIED_UNSUPPORTED_FEATURE",         0xFFFF0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_UNSUPPORTED_FEATURE_OPCODE   ("XACT_DENIED_UNSUPPORTED_FEATURE_OPCODE",  0xFFFF0000 |  1);
        }


        class NodeTypeEnumBack {
        public:
            const char*         name;
            const XactScopeEnum scope;
            const int           value;

        public:
            inline constexpr NodeTypeEnumBack(const char* name, const XactScopeEnum scope, const int value) noexcept
            : name(name), scope(scope), value(value) { }

        public:
            inline constexpr operator int() const noexcept
            { return value; }

            inline constexpr operator const NodeTypeEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const NodeTypeEnumBack& obj) const noexcept
            { return value == obj.value; }

            inline constexpr bool operator!=(const NodeTypeEnumBack& obj) const noexcept
            { return !(*this == obj); }

            inline constexpr bool IsRequester() const noexcept
            { return scope == XactScope::Requester; }

            inline constexpr bool IsHome() const noexcept
            { return scope == XactScope::Home; }

            inline constexpr bool IsSubordinate() const noexcept
            { return scope == XactScope::Subordinate; }
        };

        using NodeTypeEnum = const NodeTypeEnumBack*;

        namespace NodeType {
            inline constexpr NodeTypeEnumBack   RN_F    ("RN-F", XactScope::Requester   , 0);
            inline constexpr NodeTypeEnumBack   RN_D    ("RN-D", XactScope::Requester   , 1);
            inline constexpr NodeTypeEnumBack   RN_I    ("RN-I", XactScope::Requester   , 2);
            inline constexpr NodeTypeEnumBack   HN_F    ("HN-F", XactScope::Home        , 3);
            inline constexpr NodeTypeEnumBack   HN_I    ("HN-I", XactScope::Home        , 4);
            inline constexpr NodeTypeEnumBack   SN_F    ("SN-F", XactScope::Subordinate , 5);
            inline constexpr NodeTypeEnumBack   SN_I    ("SN-I", XactScope::Subordinate , 6);
            inline constexpr NodeTypeEnumBack   MN      ("MN",   XactScope::Home        , 7);
        }

        class Topology {
        public:
            class Node {
            public:
                NodeTypeEnum    type;
                std::string     name;
                bool            valid;

            public:
                Node() noexcept;
                Node(NodeTypeEnum type, const std::string& name) noexcept;

            public:
                bool            IsValid() const noexcept;
                operator        bool() const noexcept;
            };

        protected:
            std::unordered_map<uint16_t, Node>  nodes;

        public:
            void            SetNode(uint16_t id, NodeTypeEnum type, const std::string& name = "unspecified") noexcept;
            bool            HasNode(uint16_t id) const noexcept;
            Node            GetNode(uint16_t id) const noexcept;
            bool            RemoveNode(uint16_t id) noexcept;

        public:
            bool            IsRequester(uint16_t id) const noexcept;
            bool            IsHome(uint16_t id) const noexcept;
            bool            IsSubordinate(uint16_t id) const noexcept;
        };


        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        class FiredFlit {
        public:
            XactScopeEnum   scope;
            uint64_t        time;
            bool            isTX;

        public:
            FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept;

        public:
            bool            IsTX() const noexcept;
            bool            IsRX() const noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class FiredRequestFlit : public FiredFlit<config, conn> {
        public:
            bool            isREQ;
            union {
                Flits::REQ<config, conn>    req;
                Flits::SNP<config, conn>    snp;
            }               flit;

        public:
            FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::REQ<config, conn>& reqFlit) noexcept;
            FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::SNP<config, conn>& snpFlit) noexcept;

        public:
            bool            IsREQ() const noexcept;
            bool            IsSNP() const noexcept;

            bool            IsTXREQ() const noexcept;
            bool            IsRXREQ() const noexcept;
            bool            IsTXSNP() const noexcept;
            bool            IsRXSNP() const noexcept;

        public:
            bool            IsToRequester(const Topology& topo) const noexcept;
            bool            IsToHome(const Topology& topo) const noexcept;
            bool            IsToSubordinate(const Topology& topo) const noexcept;

        public:
            bool            IsFromRequester(const Topology& topo) const noexcept;
            bool            IsFromHome(const Topology& topo) const noexcept;
            bool            IsFromSubordinate(const Topology& topo) const noexcept;

        public:
            bool            IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Topology& topo) const noexcept;
            bool            IsFromRequesterToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromHomeToRequester(const Topology& topo) const noexcept;
            bool            IsFromHomeToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToRequester(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToHome(const Topology& topo) const noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class FiredResponseFlit : public FiredFlit<config, conn> {
        public:
            bool            isRSP;
            union {
                Flits::RSP<config, conn>    rsp;
                Flits::DAT<config, conn>    dat;
            }               flit;

        public:
            FiredResponseFlit() noexcept;
            FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::RSP<config, conn>& rspFlit) noexcept;
            FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DAT<config, conn>& datFlit) noexcept;
        
        public:
            bool            IsRSP() const noexcept;
            bool            IsDAT() const noexcept;

            bool            IsTXRSP() const noexcept;
            bool            IsRXRSP() const noexcept;
            bool            IsTXDAT() const noexcept;
            bool            IsRXDAT() const noexcept;

        public:
            bool            IsToRequester(const Topology& topo) const noexcept;
            bool            IsToHome(const Topology& topo) const noexcept;
            bool            IsToSubordinate(const Topology& topo) const noexcept;

            bool            IsToRequesterDCT(const Topology& topo) const noexcept;
            bool            IsToRequesterDMT(const Topology& topo) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsToRequesterDWT(const Topology& topo) const noexcept;
            bool            IsToSubordinateDWT(const Topology& topo) const noexcept;
#endif

        public:
            bool            IsFromRequester(const Topology& topo) const noexcept;
            bool            IsFromHome(const Topology& topo) const noexcept;
            bool            IsFromSubordinate(const Topology& topo) const noexcept;

            bool            IsFromRequesterDCT(const Topology& topo) const noexcept;
            bool            IsFromSubordinateDMT(const Topology& topo) const noexcept;
#ifdef CHI_ISSUE_EB_ENABLE
            bool            IsFromRequesterDWT(const Topology& topo) const noexcept;
            bool            IsFromSubordinateDWT(const Topology& topo) const noexcept;
#endif

        public:
            bool            IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept;
            bool            IsFromRequesterToHome(const Topology& topo) const noexcept;
            bool            IsFromRequesterToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromHomeToRequester(const Topology& topo) const noexcept;
            bool            IsFromHomeToSubordinate(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToRequester(const Topology& topo) const noexcept;
            bool            IsFromSubordinateToHome(const Topology& topo) const noexcept;
        };
    }
/*
}
*/


// Implementation of: class Topology::Node
namespace /*CHI::*/Xact {
    /*
    NodeTypeEnum    type;
    std::string     name;
    bool            valid;
    */

    inline Topology::Node::Node() noexcept
        : type      (NodeType::RN_F)
        , name      ("invalid")
        , valid     (false)
    { }

    inline Topology::Node::Node(NodeTypeEnum type, const std::string& name) noexcept
        : type      (type)
        , name      (name)
        , valid     (true)
    { }

    inline bool Topology::Node::IsValid() const noexcept
    {
        return valid;
    }

    inline Topology::Node::operator bool() const noexcept
    {
        return valid;
    }
}


// Implementation of: class Topology
namespace /*CHI::*/Xact {
    /*
    std::unordered_map<uint16_t, Node>  nodes;
    */

    inline void Topology::SetNode(uint16_t id, NodeTypeEnum type, const std::string& name) noexcept
    {
        nodes[id] = Node(type, name);
    }

    inline bool Topology::HasNode(uint16_t id) const noexcept
    {
        return nodes.contains(id);
    }

    inline Topology::Node Topology::GetNode(uint16_t id) const noexcept
    {
        auto iter = nodes.find(id);

        if (iter == nodes.end())
            return Node();

        return iter->second;
    }

    inline bool Topology::RemoveNode(uint16_t id) noexcept
    {
        return nodes.erase(id) != 0;
    }

    inline bool Topology::IsRequester(uint16_t id) const noexcept
    {
        Node node = GetNode(id);
        return node.valid && node.type->IsRequester();
    }

    inline bool Topology::IsHome(uint16_t id) const noexcept
    {
        Node node = GetNode(id);
        return node.valid && node.type->IsHome();
    }

    inline bool Topology::IsSubordinate(uint16_t id) const noexcept
    {
        Node node = GetNode(id);
        return node.valid && node.type->IsSubordinate();
    }
}


// Implementation of: class FiredFlit
namespace /*CHI::*/Xact {
    /*
    XactScopeEnum   scope;
    uint64_t        time;
    bool            isTX;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredFlit<config, conn>::FiredFlit(XactScopeEnum scope, bool isTX, uint64_t time) noexcept
        : scope     (scope)
        , time      (time)
        , isTX      (isTX)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredFlit<config, conn>::IsTX() const noexcept
    {
        return isTX;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredFlit<config, conn>::IsRX() const noexcept
    {
        return !isTX;
    }
}


// Implementation of: class FiredRequestFlit
namespace /*CHI::*/Xact {
    /*
    bool        isREQ;
    union {
        Flits::REQ<config, conn>    req;
        Flits::SNP<config, conn>    snp;
    }           flit;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredRequestFlit<config, conn>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::REQ<config, conn>& reqFlit) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isREQ                     (true)
    { 
        flit.req = reqFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredRequestFlit<config, conn>::FiredRequestFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::SNP<config, conn>& snpFlit) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isREQ                     (false)
    {
        flit.snp = snpFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsREQ() const noexcept
    {
        return isREQ;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsSNP() const noexcept
    {
        return !isREQ;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXREQ() const noexcept
    {
        return this->IsTX() && IsREQ();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXREQ() const noexcept
    {
        return this->IsRX() && IsREQ();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsTXSNP() const noexcept
    {
        return this->IsTX() && IsSNP();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsRXSNP() const noexcept
    {
        return this->IsRX() && IsSNP();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToRequester(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRX();

            case XactScope::Home:
                return this->IsTXREQ() && topo.IsRequester(flit.req.TgtID()) 
                    || this->IsTXSNP();

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToHome(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                // Don't check TgtID of TXREQ, RN E-SAM could remap this on network
                return this->IsTXREQ() /*&& topo.IsHome(flit.req.TgtId())*/;

            case XactScope::Home:
                return this->IsRXREQ();

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsToSubordinate(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return false;

            case XactScope::Home:
                return this->IsTXREQ() && topo.IsSubordinate(flit.req.TgtID());

            case XactScope::Subordinate:
                return this->IsRXREQ();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequester(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsTXREQ();

            case XactScope::Home:
                return this->IsRXREQ();

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHome(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRXSNP();

            case XactScope::Home:
                return this->IsTXSNP();

            case XactScope::Subordinate:
                return this->IsRXREQ();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinate(const Topology& topo) const noexcept
    {
        // Requests never come from Subordnates.
        return false;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        switch (from->value)
        {
            case XactScope::Requester:
                if (!IsFromRequester(topo))
                    return false;

            case XactScope::Home:
                if (!IsFromHome(topo))
                    return false;

            case XactScope::Subordinate:
                if (!IsFromSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        switch (to->value)
        {
            case XactScope::Requester:
                if (!IsToRequester(topo))
                    return false;
            
            case XactScope::Home:
                if (!IsToHome(topo))
                    return false;
            
            case XactScope::Subordinate:
                if (!IsToSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequesterToHome(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToHome(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromRequesterToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHomeToRequester(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromHomeToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinateToRequester(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredRequestFlit<config, conn>::IsFromSubordinateToHome(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToHome(topo);
    }
}

// Implementation of: class FiredResponseFlit
namespace /*CHI::*/Xact {
    /*
    bool        isRSP;
    union {
        Flits::RSP<config, conn>    rsp;
        Flits::DAT<config, conn>    dat;
    }           flit;
    */

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredResponseFlit<config, conn>::FiredResponseFlit() noexcept
        : FiredFlit<config, conn>   (XactScope::Requester, false, 0)
        , isRSP                     (false)
    { }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredResponseFlit<config, conn>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::RSP<config, conn>& rspFlit) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isRSP                     (true)
    {
        flit.rsp = rspFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline FiredResponseFlit<config, conn>::FiredResponseFlit(XactScopeEnum scope, bool isTX, uint64_t time, const Flits::DAT<config, conn>& datFlit) noexcept
        : FiredFlit<config, conn>   (scope, isTX, time)
        , isRSP                     (false)
    {
        flit.dat = datFlit;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRSP() const noexcept
    {
        return isRSP;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsDAT() const noexcept
    {
        return !isRSP;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXRSP() const noexcept
    {
        return this->IsTX() && IsRSP();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXRSP() const noexcept
    {
        return this->IsRX() && IsRSP();
    }
    
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsTXDAT() const noexcept
    {
        return this->IsTX() && IsDAT();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsRXDAT() const noexcept
    {
        return this->IsRX() && IsDAT();
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequester(const Topology& topo) const noexcept
    {
        switch (this->scope->value)
        {
            case XactScope::Requester:
                return this->IsRX()
                    || IsToRequesterDCT(topo);

            case XactScope::Home:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID());

            case XactScope::Subordinate:
                return IsToRequesterDMT(topo)
#ifdef CHI_ISSUE_B_ENABLE
                    ;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                    || IsToRequesterDWT(topo);
#endif

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToHome(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && topo.IsHome(flit.rsp.TgtID()) 
                    || this->IsTXDAT() && topo.IsHome(flit.dat.TgtID());

            case XactScope::Home:
                return this->IsRX();

            case XactScope::Subordinate:
                return this->IsTXRSP() && topo.IsHome(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsHome(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToSubordinate(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
#ifdef CHI_ISSUE_B_ENABLE
                return false;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                return IsToSubordinateDWT(topo);
#endif

            case XactScope::Home:
                return this->IsTXRSP() && topo.IsSubordinate(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsSubordinate(flit.dat.TgtID());

            case XactScope::Subordinate:
                return this->IsRX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDCT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && topo.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDMT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && topo.IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToRequesterDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && topo.IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsToSubordinateDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && topo.IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequester(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTX()
                    || IsFromRequesterDCT(topo);

            case XactScope::Home:
                return this->IsRXRSP() && topo.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            case XactScope::Subordinate:
                return IsFromRequesterDWT(topo);

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHome(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && topo.IsHome(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsHome(flit.dat.SrcID());

            case XactScope::Home:
                return this->IsTX();

            case XactScope::Subordinate:
                return this->IsRXDAT() && topo.IsHome(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinate(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
#ifdef CHI_ISSUE_B_ENABLE
                return false;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
                return IsFromSubordinateDWT(topo);
#endif

            case XactScope::Home:
                return this->IsRXRSP() && topo.IsSubordinate(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsSubordinate(flit.dat.SrcID());

            case XactScope::Subordinate:
                return this->IsTX();

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterDCT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID())
                    || this->IsTXDAT() && topo.IsRequester(flit.dat.TgtID())
                    || this->IsRXRSP() && topo.IsRequester(flit.rsp.SrcID())
                    || this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return false;

            [[unlikely]] default:
                return false;
        }
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateDMT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXDAT() && topo.IsSubordinate(flit.dat.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->isTXDAT() && topo.IsRequester(flit.dat.TgtID());

            [[unlikely]] default:
                return false;   
        }
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsTXDAT() && topo.IsSubordinate(flit.dat.TgtID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsRXDAT() && topo.IsRequester(flit.dat.SrcID());

            [[unlikely]] default:
                return false;
        }
    }
#endif
    
#ifdef CHI_ISSUE_EB_ENABLE
    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateDWT(const Topology& topo) const noexcept
    {
        switch (*this->scope)
        {
            case XactScope::Requester:
                return this->IsRXRSP() && topo.IsSubordinate(flit.rsp.SrcID());

            case XactScope::Home:
                return false;

            case XactScope::Subordinate:
                return this->IsTXRSP() && topo.IsRequester(flit.rsp.TgtID());

            [[unlikely]] default:
                return false;
        }
    }
#endif

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromTo(const Topology& topo, XactScopeEnum from, XactScopeEnum to) const noexcept
    {
        switch (from->value)
        {
            case XactScope::Requester:
                if (!IsFromRequester(topo))
                    return false;

            case XactScope::Home:
                if (!IsFromHome(topo))
                    return false;

            case XactScope::Subordinate:
                if (!IsFromSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        switch (to->value)
        {
            case XactScope::Requester:
                if (!IsToRequester(topo))
                    return false;
            
            case XactScope::Home:
                if (!IsToHome(topo))
                    return false;
            
            case XactScope::Subordinate:
                if (!IsToSubordinate(topo))
                    return false;

            [[unlikely]] default:
                return false;
        }

        return true;
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterToHome(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToHome(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromRequesterToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromRequester(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHomeToRequester(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromHomeToSubordinate(const Topology& topo) const noexcept
    {
        return IsFromHome(topo) && IsToSubordinate(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateToRequester(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToRequester(topo);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline bool FiredResponseFlit<config, conn>::IsFromSubordinateToHome(const Topology& topo) const noexcept
    {
        return IsFromSubordinate(topo) && IsToHome(topo);
    }
}


#endif // __CHI__CHI_XACT_BASE_*

//#endif // __CHI__CHI_XACT_BASE
