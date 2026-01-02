//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_BASE_B__DENIAL)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_BASE_EB__DENIAL))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_BASE_B__DENIAL
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_BASE_EB__DENIAL
#endif


/*
namespace CHI {
*/
    namespace Xact {

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
            inline constexpr XactDenialEnumBack DENIED_TXNID_IN_USE                 ("XACT_DENIED_TXNID_IN_USE",                0x00000000 |  5);
            inline constexpr XactDenialEnumBack DENIED_TXNID_NOT_EXIST              ("XACT_DENIED_TXNID_NOT_EXIST",             0x00000000 |  6);
            inline constexpr XactDenialEnumBack DENIED_DBID_IN_USE                  ("XACT_DENIED_DBID_IN_USE",                 0x00000000 |  7);
            inline constexpr XactDenialEnumBack DENIED_DBID_NOT_EXIST               ("XACT_DENIED_DBID_NOT_EXIST",              0x00000000 |  8);
            inline constexpr XactDenialEnumBack DENIED_NESTING_SNP                  ("XACT_DENIED_NESTING_SNP",                 0x00000000 |  9);

            inline constexpr XactDenialEnumBack DENIED_OPCODE                       ("XACT_DENIED_OPCODE",                      0x00010000 |  0);
            inline constexpr XactDenialEnumBack DENIED_REQ_OPCODE                   ("XACT_DENIED_REQ_OPCODE",                  0x00010000 |  1);
            inline constexpr XactDenialEnumBack DENIED_RSP_OPCODE                   ("XACT_DENIED_RSP_OPCODE",                  0x00010000 |  2);
            inline constexpr XactDenialEnumBack DENIED_DAT_OPCODE                   ("XACT_DENIED_DAT_OPCODE",                  0x00010000 |  3);
            inline constexpr XactDenialEnumBack DENIED_SNP_OPCODE                   ("XACT_DENIED_SNP_OPCODE",                  0x00010000 |  4);
            inline constexpr XactDenialEnumBack DENIED_OPCODE_NOT_SUPPORTED         ("XACT_DENIED_OPCODE_NOT_SUPPORTED",        0x00010000 | 16);
            inline constexpr XactDenialEnumBack DENIED_XACTION_NOT_SUPPORTED        ("XACT_DENIED_XACTION_NOT_SUPPORTED",       0x00010000 | 17);

            inline constexpr XactDenialEnumBack DENIED_REQ_NOT_TO_HN                ("XACT_DENIED_REQ_NOT_TO_HN",               0x00020000 |  0);
            inline constexpr XactDenialEnumBack DENIED_REQ_NOT_FROM_RN_TO_HN        ("XACT_DENIED_REQ_NOT_FROM_RN_TO_HN",       0x00020000 |  1);
            inline constexpr XactDenialEnumBack DENIED_REQ_NOT_FROM_HN_TO_SN        ("XACT_DENIED_REQ_NOT_FROM_HN_TO_SN",       0x00020000 |  2);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_TO_RN                ("XACT_DENIED_RSP_NOT_TO_RN",               0x00020000 |  3);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_HN_TO_RN        ("XACT_DENIED_RSP_NOT_FROM_HN_TO_RN",       0x00020000 |  4);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_TO_HN                ("XACT_DENIED_RSP_NOT_TO_HN",               0x00020000 |  5);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_RN_TO_HN        ("XACT_DENIED_RSP_NOT_FROM_RN_TO_HN",       0x00020000 |  6);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_SN              ("XACT_DENIED_RSP_NOT_FROM_SN",             0x00020000 |  7);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_SN_TO_HN        ("XACT_DENIED_RSP_NOT_FROM_SN_TO_HN",       0x00020000 |  8);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_SN_TO_RN        ("XACT_DENIED_RSP_NOT_FROM_SN_TO_RN",       0x00020000 |  9);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN  ("XACT_DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN", 0x00020000 | 10);
            inline constexpr XactDenialEnumBack DENIED_RSP_NOT_TO_SN                ("XACT_DENIED_RSP_NOT_TO_SN",               0x00020000 | 11);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_TO_RN                ("XACT_DENIED_DAT_NOT_TO_RN",               0x00020000 | 12);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_FROM_HN_TO_RN        ("XACT_DENIED_DAT_NOT_FROM_HN_TO_RN",       0x00020000 | 13);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_FROM_HN_TO_SN        ("XACT_DENIED_DAT_NOT_FROM_HN_TO_SN",       0x00020000 | 14);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_TO_HN                ("XACT_DENIED_DAT_NOT_TO_HN",               0x00020000 | 15);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_FROM_RN_TO_HN        ("XACT_DENIED_DAT_NOT_FROM_RN_TO_HN",       0x00020000 | 16);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_TO_SN                ("XACT_DENIED_DAT_NOT_TO_SN",               0x00020000 | 17);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_FROM_SN              ("XACT_DENIED_DAT_NOT_FROM_SN",             0x00020000 | 18);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_FROM_SN_TO_HN        ("XACT_DENIED_DAT_NOT_FROM_SN_TO_HN",       0x00020000 | 19);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_TO_HN_OR_RN          ("XACT_DENIED_DAT_NOT_TO_HN_OR_RN",         0x00020000 | 20);
            inline constexpr XactDenialEnumBack DENIED_DAT_NOT_FROM_RN_TO_RN        ("XACT_DENIED_DAT_NOT_FROM_RN_TO_RN",       0x00020000 | 21);
            inline constexpr XactDenialEnumBack DENIED_SNP_NOT_TO_RN                ("XACT_DENIED_SNP_NOT_TO_RN",               0x00020000 | 22);
            inline constexpr XactDenialEnumBack DENIED_SNP_NOT_FROM_HN_TO_RN        ("XACT_DENIED_SNP_NOT_FROM_HN_TO_RN",       0x00020000 | 23);
            inline constexpr XactDenialEnumBack DENIED_RSP_RETRYACK_ROUTE           ("XACT_DENIED_RSP_RETRYACK_ROUTE",          0x00020000 | 24);

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

            inline constexpr XactDenialEnumBack DENIED_RESPSEPDATA_AFTER_COMPDATA   ("XACT_DENIED_RESPSEPDATA_AFTER_COMPDATA",  0x000B0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_RESPSEPDATA_AFTER_COMP       ("XACT_DENIED_RESPSEPDATA_AFTER_COMP",      0x000B0000 |  1);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_RESPSEP           ("XACT_DENIED_COMP_AFTER_RESPSEP",          0x000B0000 |  2);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_DATASEP           ("XACT_DENIED_COMP_AFTER_DATASEP",          0x000B0000 |  3);
            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMPDATA          ("XACT_DENIED_COMP_AFTER_COMPDATA",         0x000B0000 |  4);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_RESPSEP       ("XACT_DENIED_COMPDATA_AFTER_RESPSEP",      0x000B0000 |  5);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_DATASEP       ("XACT_DENIED_COMPDATA_AFTER_DATASEP",      0x000B0000 |  6);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_COMP          ("XACT_DENIED_COMPDATA_AFTER_COMP",         0x000B0000 |  7);
            inline constexpr XactDenialEnumBack DENIED_DATASEPRESP_AFTER_COMP       ("XACT_DENIED_DATASEPRESP_AFTER_COMP",      0x000B0000 |  8);
            inline constexpr XactDenialEnumBack DENIED_DATASEPRESP_AFTER_COMPDATA   ("XACT_DENIED_DATASEPRESP_AFTER_COMPDATA",  0x000B0000 |  9);
            
            inline constexpr XactDenialEnumBack DENIED_DATA_BEFORE_DBIDRESP         ("XACT_DENIED_DATA_BEFORE_DBIDRESP",        0x000B0000 | 10);
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

            inline constexpr XactDenialEnumBack DENIED_COMPACK_BEFORE_COMPDATA_OR_RESPSEPDATA   ("XACT_DENIED_COMPACK_BEFORE_COMPDATA_OR_RESPSEPDATA",  0x000B0000 | 32);

            inline constexpr XactDenialEnumBack DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL          ("XACT_DENIED_NCBWRDATA_AFTER_WRITEDATACANCEL",         0x000B0000 | 33);
            inline constexpr XactDenialEnumBack DENIED_NCBWRDATA_AFTER_NCBWRDATACOMPACK         ("XACT_DENIED_NCBWRDATA_AFTER_NCBWRDATACOMPACK",        0x000B0000 | 34);
            inline constexpr XactDenialEnumBack DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA          ("XACT_DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA",         0x000B0000 | 35);
            inline constexpr XactDenialEnumBack DENIED_WRITEDATACANCEL_AFTER_NCBWRDATACOMPACK   ("XACT_DENIED_WRITEDATACANCEL_AFTER_NCBWRDATACOMPACK",  0x000B0000 | 36);
            inline constexpr XactDenialEnumBack DENIED_NCBWRDATACOMPACK_AFTER_WRITEDATACANCEL   ("XACT_DENIED_NCBWRDATACOMPACK_AFTER_WRITEDATACANCEL",  0x000B0000 | 37);
            inline constexpr XactDenialEnumBack DENIED_NCBWRDATACOMPACK_AFTER_NCBWRDATA         ("XACT_DENIED_NCBWRDATACOMPACK_AFTER_NCBWRDATA",        0x000B0000 | 38);

            inline constexpr XactDenialEnumBack DENIED_NCBWRDATA_AFTER_NCBWRDATA                ("XACT_DENIED_NCBWRDATA_AFTER_NCBWRDATA",               0x000B0000 | 39);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_COMPDATA                  ("XACT_DENIED_COMPDATA_AFTER_COMPDATA",                 0x000B0000 | 40);

            inline constexpr XactDenialEnumBack DENIED_READRECEIPT_AFTER_READRECEIPT            ("XACT_DENIED_READRECEIPT_AFTER_READRECEIPT",           0x000B0000 | 41);

            inline constexpr XactDenialEnumBack DENIED_COMP_AFTER_COMPSTASHDONE                 ("XACT_DENIED_COMP_AFTER_COMPSTASHDONE",                0x000B0000 | 42);
            inline constexpr XactDenialEnumBack DENIED_STASHDONE_AFTER_STASHDONE                ("XACT_DENIED_STASHDONE_AFTER_STASHDONE",               0x000B0000 | 43);
            inline constexpr XactDenialEnumBack DENIED_STASHDONE_AFTER_COMPSTASHDONE            ("XACT_DENIED_STASHDONE_AFTER_COMPSTASHDONE",           0x000B0000 | 44);
            inline constexpr XactDenialEnumBack DENIED_COMPSTASHDONE_AFTER_COMP                 ("XACT_DENIED_COMPSTASHDONE_AFTER_COMP",                0x000B0000 | 45);
            inline constexpr XactDenialEnumBack DENIED_COMPSTASHDONE_AFTER_STASHDONE            ("XACT_DENIED_COMPSTASHDONE_AFTER_STASHDONE",           0x000B0000 | 46);
            inline constexpr XactDenialEnumBack DENIED_COMPSTASHDONE_AFTER_COMPSTASHDONE        ("XACT_DENIED_COMPSTASHDONE_AFTER_COMPSTASHDONE",       0x000B0000 | 47);

            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_DATAID            ("XACT_DENIED_DUPLICATED_DATAID",           0x000B0000 | 48);

            inline constexpr XactDenialEnumBack DENIED_COMPACK_ON_NON_EXPCOMPACK    ("XACT_DENIED_COMPACK_ON_NON_EXPCOMPACK",   0x000B0000 | 49);
            
            inline constexpr XactDenialEnumBack DENIED_DWT_ON_EXPCOMPACK            ("XACT_DENIED_DWT_ON_EXPCOMPACK",           0x000B0000 | 50);
            inline constexpr XactDenialEnumBack DENIED_DWT_WITH_DBIDRESPORD         ("XACT_DENIED_DWT_WITH_DBIDRESPORD",        0x000B0000 | 51);

            inline constexpr XactDenialEnumBack DENIED_READRECEIPT_ON_NO_ORDER      ("XACT_DENIED_READRECEIPT_ON_NO_ORDER",     0x000B0000 | 52);

            inline constexpr XactDenialEnumBack DENIED_SNPRESP_BEFORE_ALL_SNPDVMOP  ("XACT_DENIED_SNPRESP_BEFORE_ALL_SNPDVMOP", 0x000B0000 | 53);
            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_SNPDVMOP          ("XACT_DENIED_DUPLICATED_SNPDVMOP",         0x000B0000 | 54);

            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_SNPRESP                   ("XACT_DENIED_COMPDATA_AFTER_SNPRESP",                  0x000B0000 | 53);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_SNPRESPDATA               ("XACT_DENIED_COMPDATA_AFTER_SNPRESPDATA",              0x000B0000 | 54);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_AFTER_SNPRESPDATAPTL            ("XACT_DENIED_COMPDATA_AFTER_SNPRESPDATAPTL",           0x000B0000 | 55);
            inline constexpr XactDenialEnumBack DENIED_SNPRESP_AFTER_COMPDATA                   ("XACT_DENIED_SNPRESP_AFTER_COMPDATA",                  0x000B0000 | 56);
            inline constexpr XactDenialEnumBack DENIED_SNPRESP_AFTER_SNPRESP                    ("XACT_DENIED_SNPRESP_AFTER_SNPRESP",                   0x000B0000 | 57);
            inline constexpr XactDenialEnumBack DENIED_SNPRESP_AFTER_SNPRESPDATA                ("XACT_DENIED_SNPRESP_AFTER_SNPRESPDATA",               0x000B0000 | 58);
            inline constexpr XactDenialEnumBack DENIED_SNPRESP_AFTER_SNPRESPDATAPTL             ("XACT_DENIED_SNPRESP_AFTER_SNPRESPDATAPTL",            0x000B0000 | 59);
            inline constexpr XactDenialEnumBack DENIED_SNPRESP_AFTER_SNPRESPFWDED               ("XACT_DENIED_SNPRESP_AFTER_SNPRESPFWDED",              0x000B0000 | 60);
            inline constexpr XactDenialEnumBack DENIED_SNPRESP_AFTER_SNPRESPDATAFWDED           ("XACT_DENIED_SNPRESP_AFTER_SNPRESPDATAFWDED",          0x000B0000 | 61);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATA_AFTER_COMPDATA               ("XACT_DENIED_SNPRESPDATA_AFTER_COMPDATA",              0x000B0000 | 62);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATA_AFTER_SNPRESP                ("XACT_DENIED_SNPRESPDATA_AFTER_SNPRESP",               0x000B0000 | 63);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATA_AFTER_SNPRESPDATAPTL         ("XACT_DENIED_SNPRESPDATA_AFTER_SNPRESPDATAPTL",        0x000B0000 | 64);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATA_AFTER_SNPRESPFWDED           ("XACT_DENIED_SNPRESPDATA_AFTER_SNPRESPFWDED",          0x000B0000 | 65);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATA_AFTER_SNPRESPDATAFWDED       ("XACT_DENIED_SNPRESPDATA_AFTER_SNPRESPDATAFWDED",      0x000B0000 | 66);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAPTL_AFTER_COMPDATA            ("XACT_DENIED_SNPRESPDATAPTL_AFTER_COMPDATA",           0x000B0000 | 67);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAPTL_AFTER_SNPRESP             ("XACT_DENIED_SNPRESPDATAPTL_AFTER_SNPRESP",            0x000B0000 | 68);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAPTL_AFTER_SNPRESPDATA         ("XACT_DENIED_SNPRESPDATAPTL_AFTER_SNPRESPDATA",        0x000B0000 | 69);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAPTL_AFTER_SNPRESPFWDED        ("XACT_DENIED_SNPRESPDATAPTL_AFTER_SNPRESPFWDED",       0x000B0000 | 70);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAPTL_AFTER_SNPRESPDATAFWDED    ("XACT_DENIED_SNPRESPDATAPTL_AFTER_SNPRESPDATAFWDED",   0x000B0000 | 71);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPFWDED_AFTER_SNPRESP               ("XACT_DENIED_SNPRESPFWDED_AFTER_SNPRESP",              0x000B0000 | 72);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPFWDED_AFTER_SNPRESPDATA           ("XACT_DENIED_SNPRESPFWDED_AFTER_SNPRESPDATA",          0x000B0000 | 73);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPFWDED_AFTER_SNPRESPDATAPTL        ("XACT_DENIED_SNPRESPFWDED_AFTER_SNPRESPDATAPTL",       0x000B0000 | 74);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPFWDED_AFTER_SNPRESPFWDED          ("XACT_DENIED_SNPRESPFWDED_AFTER_SNPRESPFWDED",         0x000B0000 | 75);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPFWDED_AFTER_SNPRESPDATAFWDED      ("XACT_DENIED_SNPRESPFWDED_AFTER_SNPRESPDATAFWDED",     0x000B0000 | 76);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_AFTER_SNPRESP           ("XACT_DENIED_SNPRESPDATAFWDED_AFTER_SNPRESP",          0x000B0000 | 77);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPDATA       ("XACT_DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPDATA",      0x000B0000 | 78);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPDATAPTL    ("XACT_DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPDATAPTL",   0x000B0000 | 79);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPFWDED      ("XACT_DENIED_SNPRESPDATAFWDED_AFTER_SNPRESPFWDED",     0x000B0000 | 80);

            inline constexpr XactDenialEnumBack DENIED_SNPRESPFWDED_COMPDATA_FWDSTATE_MISMATCH      ("XACT_DENIED_SNPRESPFWDED_COMPDATA_FWDSTATE_MISMATCH",     0x000B0000 | 81);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_COMPDATA_FWDSTATE_MISMATCH  ("XACT_DENIED_SNPRESPDATAFWDED_COMPDATA_FWDSTATE_MISMATCH", 0x000B0000 | 82);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_FWDSTATE_MISMATCH           ("XACT_DENIED_SNPRESPDATAFWDED_FWDSTATE_MISMATCH",          0x000B0000 | 83);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_RESP_MISMATCH               ("XACT_DENIED_SNPRESPDATAFWDED_RESP_MISMATCH",              0x000B0000 | 84);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATA_RESP_MISMATCH                    ("XACT_DENIED_SNPRESPDATA_RESP_MISMATCH",                   0x000B0000 | 85);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAPTL_RESP_MISMATCH                 ("XACT_DENIED_SNPRESPDATAPTL_RESP_MISMATCH",                0x000B0000 | 86);
            inline constexpr XactDenialEnumBack DENIED_COMPDATA_RESP_MISMATCH                       ("XACT_DENIED_COMPDATA_RESP_MISMATCH",                      0x000B0000 | 87);
            inline constexpr XactDenialEnumBack DENIED_DATASEPRESP_RESP_MISMATCH                    ("XACT_DENIED_DATASEPRESP_RESP_MISMATCH",                   0x000B0000 | 88);
            inline constexpr XactDenialEnumBack DENIED_COPYBACKWRDATA_RESP_MISMATCH                 ("XACT_DENIED_COPYBACKWRDATA_RESP_MISMATCH",                0x000B0000 | 89);

            inline constexpr XactDenialEnumBack DENIED_SNPRESPFWDED_INVALID_FWDSTATE_RESP           ("XACT_DENIED_SNPRESPFWDED_INVALID_FWDSTATE_RESP",          0x000B0000 | 90);
            inline constexpr XactDenialEnumBack DENIED_SNPRESPDATAFWDED_INVALID_FWDSTATE_RESP       ("XACT_DENIED_SNPRESPDATAFWDED_INVALID_FWDSTATE_RESP",      0x000B0000 | 91);

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
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_STASHLPID         ("XACT_DENIED_RETRY_DIFF_STASHLPID",        0x000C0000 | 43);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_OPCODE            ("XACT_DENIED_RETRY_DIFF_OPCODE",           0x000C0000 | 44);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SIZE              ("XACT_DENIED_RETRY_DIFF_SIZE",             0x000C0000 | 45);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_ADDR              ("XACT_DENIED_RETRY_DIFF_ADDR",             0x000C0000 | 46);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_NS                ("XACT_DENIED_RETRY_DIFF_NS",               0x000C0000 | 47);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_LIKELYSHARED      ("XACT_DENIED_RETRY_DIFF_LIKELYSHARED",     0x000C0000 | 48);
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_ALLOWRETRY        ("XACT_DENIED_RETRY_DIFF_ALLOWRETRY",       0x000C0000 | 49);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_ORDER             ("XACT_DENIED_RETRY_DIFF_ORDER",            0x000C0000 | 50);
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_PCRDTYPE          ("XACT_DENIED_RETRY_DIFF_PCRDTYPE",         0x000C0000 | 51);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_MEMATTR           ("XACT_DENIED_RETRY_DIFF_MEMATTR",          0x000C0000 | 52);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SNPATTR           ("XACT_DENIED_RETRY_DIFF_SNPATTR",          0x000C0000 | 53);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_DODWT             ("XACT_DENIED_RETRY_DIFF_DODWT",            0x000C0000 | 54);
#endif
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_LPID              ("XACT_DENIED_RETRY_DIFF_LPID",             0x000C0000 | 55);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_PGROUPID          ("XACT_DENIED_RETRY_DIFF_PGROUPID",         0x000C0000 | 56);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_STASHGROUPID      ("XACT_DENIED_RETRY_DIFF_STASHGROUPID",     0x000C0000 | 57);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TAGGROUPID        ("XACT_DENIED_RETRY_DIFF_TAGGROUPID",       0x000C0000 | 58);
#endif
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_EXCL              ("XACT_DENIED_RETRY_DIFF_EXCL",             0x000C0000 | 59);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_SNOOPME           ("XACT_DENIED_RETRY_DIFF_SNOOPME",          0x000C0000 | 60);
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_EXPCOMPACK        ("XACT_DENIED_RETRY_DIFF_EXPCOMPACK",       0x000C0000 | 61);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TAGOP             ("XACT_DENIED_RETRY_DIFF_TAGOP",            0x000C0000 | 62);
#endif
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_TRACETAG          ("XACT_DENIED_RETRY_DIFF_TRACETAG",         0x000C0000 | 63);
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_MPAM              ("XACT_DENIED_RETRY_DIFF_MPAM",             0x000C0000 | 64);
#endif
        //  inline constexpr XactDenialEnumBack DENIED_RETRY_DIFF_RSVDC             ("XACT_DENIED_RETRY_DIFF_RSVDC",            0x000C0000 | 65);

            inline constexpr XactDenialEnumBack DENIED_RETRY_ON_ACTIVE_PROGRESS     ("XACT_DENIED_RETRY_ON_ACTIVE_PROGRESS",    0x000C0000 | 72);

            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_SNPRESP           ("XACT_DENIED_DUPLICATED_SNPRESP",          0x000D0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_SNPRESPDATA       ("XACT_DENIED_DUPLICATED_SNPRESPDATA",      0x000D0000 |  1);
            inline constexpr XactDenialEnumBack DENIED_DUPLICATED_READRECEIPT       ("XACT_DENIED_DUPLICATED_READRECEIPT",      0x000D0000 |  2);

            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_QOS                ("XACT_DENIED_REQ_FIELD_QOS",               0x00100000 |  0);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_TGTID              ("XACT_DENIED_REQ_FIELD_TGTID",             0x00100000 |  1);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_SRCID              ("XACT_DENIED_REQ_FIELD_SRCID",             0x00100000 |  2);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_TXNID              ("XACT_DENIED_REQ_FIELD_TXNID",             0x00100000 |  3);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_OPCODE             ("XACT_DENIED_REQ_FIELD_OPCODE",            0x00100000 |  4);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_ALLOWRETRY         ("XACT_DENIED_REQ_FIELD_ALLOWRETRY",        0x00100000 |  5);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_PCRDTYPE           ("XACT_DENIED_REQ_FIELD_PCRDTYPE",          0x00100000 |  6);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_RSVDC              ("XACT_DENIED_REQ_FIELD_RSVDC",             0x00100000 |  7);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_TAGOP              ("XACT_DENIED_REQ_FIELD_TAGOP",             0x00100000 |  8);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_TRACETAG           ("XACT_DENIED_REQ_FIELD_TRACETAG",          0x00100000 |  9);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_MPAM               ("XACT_DENIED_REQ_FIELD_MPAM",              0x00100000 | 10);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_ADDR               ("XACT_DENIED_REQ_FIELD_ADDR",              0x00100000 | 11);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_NS                 ("XACT_DENIED_REQ_FIELD_NS",                0x00100000 | 12);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_SIZE               ("XACT_DENIED_REQ_FIELD_SIZE",              0x00100000 | 13);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_MEMATTR_ALLOCATE   ("XACT_DENIED_REQ_FIELD_MEMATTR_ALLOCATE",  0x00100000 | 14);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_MEMATTR_CACHEABLE  ("XACT_DENIED_REQ_FIELD_MEMATTR_CACHEABLE", 0x00100000 | 15);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_MEMATTR_DEVICE     ("XACT_DENIED_REQ_FIELD_MEMATTR_DEVICE",    0x00100000 | 16);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_MEMATTR_EWA        ("XACT_DENIED_REQ_FIELD_MEMATTR_EWA",       0x00100000 | 17);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_SNPATTR            ("XACT_DENIED_REQ_FIELD_SNPATTR",           0x00100000 | 18);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_DODWT              ("XACT_DENIED_REQ_FIELD_DODWT",             0x00100000 | 19);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_ORDER              ("XACT_DENIED_REQ_FIELD_ORDER",             0x00100000 | 20);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_LIKELYSHARED       ("XACT_DENIED_REQ_FIELD_LIKELYSHARED",      0x00100000 | 21);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_EXCL               ("XACT_DENIED_REQ_FIELD_EXCL",              0x00100000 | 22);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_SNOOPME            ("XACT_DENIED_REQ_FIELD_SNOOPME",           0x00100000 | 23);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_EXPCOMPACK         ("XACT_DENIED_REQ_FIELD_EXPCOMPACK",        0x00100000 | 24);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_LPID               ("XACT_DENIED_REQ_FIELD_LPID",              0x00100000 | 25);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_TAGGROUPID         ("XACT_DENIED_REQ_FIELD_TAGGROUPID",        0x00100000 | 26);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_STASHGROUPID       ("XACT_DENIED_REQ_FIELD_STASHGROUPID",      0x00100000 | 27);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_PGROUPID           ("XACT_DENIED_REQ_FIELD_PGROUPID",          0x00100000 | 28);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_RETURNNID          ("XACT_DENIED_REQ_FIELD_RETURNNID",         0x00100000 | 29);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_STASHNID           ("XACT_DENIED_REQ_FIELD_STASHNID",          0x00100000 | 30);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_SLCREPHINT         ("XACT_DENIED_REQ_FIELD_SLCREPHINT",        0x00100000 | 31);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_STASHNIDVALID      ("XACT_DENIED_REQ_FIELD_STASHNIDVALID",     0x00100000 | 32);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_ENDIAN             ("XACT_DENIED_REQ_FIELD_ENDIAN",            0x00100000 | 33);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_DEEP               ("XACT_DENIED_REQ_FIELD_DEEP",              0x00100000 | 34);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_RETURNTXNID        ("XACT_DENIED_REQ_FIELD_RETURNTXNID",       0x00100000 | 35);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_STASHLPIDVALID     ("XACT_DENIED_REQ_FIELD_STASHLPIDVALID",    0x00100000 | 36);
            inline constexpr XactDenialEnumBack DENIED_REQ_FIELD_STASHLPID          ("XACT_DENIED_REQ_FIELD_STASHLPID",         0x00100000 | 37);

            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_QOS                ("XACT_DENIED_RSP_FIELD_QOS",               0x00110000 |  0);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_TGTID              ("XACT_DENIED_RSP_FIELD_TGTID",             0x00110000 |  1);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_SRCID              ("XACT_DENIED_RSP_FIELD_SRCID",             0x00110000 |  2);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_TXNID              ("XACT_DENIED_RSP_FIELD_TXNID",             0x00110000 |  3);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_OPCODE             ("XACT_DENIED_RSP_FIELD_OPCODE",            0x00110000 |  4);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_RESPERR            ("XACT_DENIED_RSP_FIELD_RESPERR",           0x00110000 |  5);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_RESP               ("XACT_DENIED_RSP_FIELD_RESP",              0x00110000 |  6);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_CBUSY              ("XACT_DENIED_RSP_FIELD_CBUSY",             0x00110000 |  7);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_DBID               ("XACT_DENIED_RSP_FIELD_DBID",              0x00110000 |  8);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_TAGGROUPID         ("XACT_DENIED_RSP_FIELD_TAGGROUPID",        0x00110000 |  9);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_STASHGROUPID       ("XACT_DENIED_RSP_FIELD_STASHGROUPID",      0x00110000 | 10);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_PGROUPID           ("XACT_DENIED_RSP_FIELD_PGROUPID",          0x00110000 | 11);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_PCRDTYPE           ("XACT_DENIED_RSP_FIELD_PCRDTYPE",          0x00110000 | 12);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_FWDSTATE           ("XACT_DENIED_RSP_FIELD_FWDSTATE",          0x00110000 | 13);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_DATAPULL           ("XACT_DENIED_RSP_FIELD_DATAPULL",          0x00110000 | 14);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_TAGOP              ("XACT_DENIED_RSP_FIELD_TAGOP",             0x00110000 | 15);
            inline constexpr XactDenialEnumBack DENIED_RSP_FIELD_TRACETAG           ("XACT_DENIED_RSP_FIELD_TRACETAG",          0x00110000 | 16);

            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_QOS                ("XACT_DENIED_DAT_FIELD_QOS",               0x00120000 |  0);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_TGTID              ("XACT_DENIED_DAT_FIELD_TGTID",             0x00120000 |  1);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_SRCID              ("XACT_DENIED_DAT_FIELD_SRCID",             0x00120000 |  2);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_TXNID              ("XACT_DENIED_DAT_FIELD_TXNID",             0x00120000 |  3);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_OPCODE             ("XACT_DENIED_DAT_FIELD_OPCODE",            0x00120000 |  4);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_RESPERR            ("XACT_DENIED_DAT_FIELD_RESPERR",           0x00120000 |  5);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_RESP               ("XACT_DENIED_DAT_FIELD_RESP",              0x00120000 |  6);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_CBUSY              ("XACT_DENIED_DAT_FIELD_CBUSY",             0x00120000 |  7);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_DBID               ("XACT_DENIED_DAT_FIELD_DBID",              0x00120000 |  8);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_CCID               ("XACT_DENIED_DAT_FIELD_CCID",              0x00120000 |  9);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_DATAID             ("XACT_DENIED_DAT_FIELD_DATAID",            0x00120000 | 10);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_RSVDC              ("XACT_DENIED_DAT_FIELD_RSVDC",             0x00120000 | 11);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_BE                 ("XACT_DENIED_DAT_FIELD_BE",                0x00120000 | 12);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_DATA               ("XACT_DENIED_DAT_FIELD_DATA",              0x00120000 | 13);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_HOMENID            ("XACT_DENIED_DAT_FIELD_HOMENID",           0x00120000 | 14);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_FWDSTATE           ("XACT_DENIED_DAT_FIELD_FWDSTATE",          0x00120000 | 15);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_DATAPULL           ("XACT_DENIED_DAT_FIELD_DATAPULL",          0x00120000 | 16);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_DATASOURCE         ("XACT_DENIED_DAT_FIELD_DATASOURCE",        0x00120000 | 17);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_TRACETAG           ("XACT_DENIED_DAT_FIELD_TRACETAG",          0x00120000 | 18);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_DATACHECK          ("XACT_DENIED_DAT_FIELD_DATACHECK",         0x00120000 | 19);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_POISON             ("XACT_DENIED_DAT_FIELD_POISON",            0x00120000 | 20);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_TAGOP              ("XACT_DENIED_DAT_FIELD_TAGOP",             0x00120000 | 21);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_TAG                ("XACT_DENIED_DAT_FIELD_TAG",               0x00120000 | 22);
            inline constexpr XactDenialEnumBack DENIED_DAT_FIELD_TU                 ("XACT_DENIED_DAT_FIELD_TU",                0x00120000 | 23);

            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_QOS                ("XACT_DENIED_SNP_FIELD_QOS",               0x00130000 |  0);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_SRCID              ("XACT_DENIED_SNP_FIELD_SRCID",             0x00130000 |  1);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_TXNID              ("XACT_DENIED_SNP_FIELD_TXNID",             0x00130000 |  2);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_OPCODE             ("XACT_DENIED_SNP_FIELD_OPCODE",            0x00130000 |  3);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_ADDR               ("XACT_DENIED_SNP_FIELD_ADDR",              0x00130000 |  4);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_NS                 ("XACT_DENIED_SNP_FIELD_NS",                0x00130000 |  5);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_FWDNID             ("XACT_DENIED_SNP_FIELD_FWDNID",            0x00130000 |  6);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_FWDTXNID           ("XACT_DENIED_SNP_FIELD_FWDTXNID",          0x00130000 |  7);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_STASHLPIDVALID     ("XACT_DENIED_SNP_FIELD_STASHLPIDVALID",    0x00130000 |  8);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_STASHLPID          ("XACT_DENIED_SNP_FIELD_STASHLPID",         0x00130000 |  9);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_VMIDEXT            ("XACT_DENIED_SNP_FIELD_VMIDEXT",           0x00130000 | 10);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_DONOTGOTOSD        ("XACT_DENIED_SNP_FIELD_DONOTGOTOSD",       0x00130000 | 11);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_DONOTDATAPULL      ("XACT_DENIED_SNP_FIELD_DONOTDATAPULL",     0x00130000 | 12);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_RETTOSRC           ("XACT_DENIED_SNP_FIELD_RETTOSRC",          0x00130000 | 13);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_TRACETAG           ("XACT_DENIED_SNP_FIELD_TRACETAG",          0x00130000 | 14);
            inline constexpr XactDenialEnumBack DENIED_SNP_FIELD_MPAM               ("XACT_DENIED_SNP_FIELD_MPAM",              0x00130000 | 15);

            inline constexpr XactDenialEnumBack DENIED_STATE_INITIAL                ("XACT_DENIED_STATE_INITIAL",               0x00200000 |  0);
            inline constexpr XactDenialEnumBack DENIED_STATE_COMP                   ("XACT_DENIED_STATE_COMP",                  0x00200000 |  1);
            inline constexpr XactDenialEnumBack DENIED_STATE_COMPDATA               ("XACT_DENIED_STATE_COMPDATA",              0x00200000 |  2);
            inline constexpr XactDenialEnumBack DENIED_STATE_DATASEPRESP            ("XACT_DENIED_STATE_DATASEPRESP",           0x00200000 |  3);
            inline constexpr XactDenialEnumBack DENIED_STATE_COPYBACKWRDATA         ("XACT_DENIED_STATE_COPYBACKWRDATA",        0x00200000 |  4);
            inline constexpr XactDenialEnumBack DENIED_STATE_RESP_SNPRESP           ("XACT_DENIED_STATE_RESP_SNPRESP",          0x00200000 |  5);
            inline constexpr XactDenialEnumBack DENIED_STATE_RESP_SNPRESPDATA       ("XACT_DENIED_STATE_RESP_SNPRESPDATA",      0x00200000 |  6);
            inline constexpr XactDenialEnumBack DENIED_STATE_RESP_SNPRESPFWDED      ("XACT_DENIED_STATE_RESP_SNPRESPFWDED",     0x00200000 |  7);
            inline constexpr XactDenialEnumBack DENIED_STATE_RESP_SNPRESPDATAFWDED  ("XACT_DENIED_STATE_RESP_SNPRESPDATAFWDED", 0x00200000 |  8);
            inline constexpr XactDenialEnumBack DENIED_STATE_FWD_SNPRESPFWDED       ("XACT_DENIED_STATE_FWD_SNPRESPFWDED",      0x00200000 |  9);
            inline constexpr XactDenialEnumBack DENIED_STATE_FWD_SNPRESPDATAFWDED   ("XACT_DENIED_STATE_FWD_SNPRESPDATAFWDED",  0x00200000 | 10);
            inline constexpr XactDenialEnumBack DENIED_STATE_NONCOPYBACKWRDATA      ("XACT_DENIED_STATE_NONCOPYBACKWRDATA",     0x00200000 | 11);
            inline constexpr XactDenialEnumBack DENIED_STATE_NESTED_TRANSFER        ("XACT_DENIED_STATE_NESTED_TRANSFER",       0x00200000 | 12);
            inline constexpr XactDenialEnumBack DENIED_STATE_MISMATCH_REPEAT        ("XACT_DENIED_STATE_MISMATCH_REPEAT",       0x00200000 | 13);
            inline constexpr XactDenialEnumBack DENIED_STATE_FWD_COMPDATA           ("XACT_DENIED_STATE_FWD_COMPDATA",          0x00200000 | 14);
            inline constexpr XactDenialEnumBack DENIED_STATE_FWD_MISMATCH           ("XACT_DENIED_STATE_FWD_MISMATCH",          0x00200000 | 15);
            inline constexpr XactDenialEnumBack DENIED_STATE_FWD_MISMATCH_REPEAT    ("XACT_DENIED_STATE_FWD_MISMATCH_REPEAT",   0x00200000 | 16);
            inline constexpr XactDenialEnumBack DENIED_STATE_DONOTGOTOSD            ("XACT_DENIED_STATE_DONOTGOTOSD",           0x00200000 | 17);

            inline constexpr XactDenialEnumBack DENIED_UNSUPPORTED_FEATURE          ("XACT_DENIED_UNSUPPORTED_FEATURE",         0xFFFF0000 |  0);
            inline constexpr XactDenialEnumBack DENIED_UNSUPPORTED_FEATURE_OPCODE   ("XACT_DENIED_UNSUPPORTED_FEATURE_OPCODE",  0xFFFF0000 |  1);
        }
    }
/*
}
*/

#endif
