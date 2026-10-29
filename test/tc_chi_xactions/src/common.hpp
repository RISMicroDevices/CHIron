#pragma once

#ifndef TC_XACT_STATE__STANDALONE
#   define CHI_ISSUE_E
#endif


#ifndef CHI_NODEID_WIDTH
#   define  CHI_NODEID_WIDTH            7
#endif

#ifndef CHI_REQ_ADDR_WIDTH
#   define  CHI_REQ_ADDR_WIDTH          48
#endif

#ifndef CHI_REQ_RSVDC_WIDTH
#   define  CHI_REQ_RSVDC_WIDTH         0
#endif

#ifndef CHI_DAT_RSVDC_WIDTH
#   define  CHI_DAT_RSVDC_WIDTH         0
#endif

#ifndef CHI_DATA_WIDTH
#   define  CHI_DATA_WIDTH              256
#endif

#ifndef CHI_DATACHECK_PRESENT
#   define  CHI_DATACHECK_PRESENT       false
#endif

#ifndef CHI_POISON_PRESENT
#   define  CHI_POISON_PRESENT          false
#endif

#ifndef CHI_MPAM_PRESENT
#   define  CHI_MPAM_PRESENT            false
#endif


#ifdef CHI_ISSUE_E
#   include "../../../chi_eb/xact/chi_eb_xact_base.hpp"         // IWYU pragma: keep
    using namespace CHI::Eb;
    using config = FlitConfiguration<
        CHI_NODEID_WIDTH,
        CHI_REQ_ADDR_WIDTH,
        CHI_REQ_RSVDC_WIDTH,
        CHI_DAT_RSVDC_WIDTH,
        CHI_DATA_WIDTH,
        CHI_DATACHECK_PRESENT,
        CHI_POISON_PRESENT,
        CHI_MPAM_PRESENT
    >;
#endif


#include <string>

// [PASS    ]
// [FAIL    ]
// [ENVERROR]
static const std::string STATE_PASS     = "[\033[92mPASS    \033[0m]";
static const std::string STATE_FAIL     = "[\033[91mFAIL    \033[0m]";
static const std::string STATE_ENVERROR = "[\033[35mENVERROR\033[0m]";
static const std::string STATE_DISABLED = "[\033[90mDISABLED\033[0m]";

// [FAIL #nn]
static const std::string STATE_FAIL_0   = "[\033[91mFAIL #0 \033[0m]";
static const std::string STATE_FAIL_1   = "[\033[91mFAIL #1 \033[0m]";
static const std::string STATE_FAIL_2   = "[\033[91mFAIL #2 \033[0m]";
static const std::string STATE_FAIL_3   = "[\033[91mFAIL #3 \033[0m]";
static const std::string STATE_FAIL_4   = "[\033[91mFAIL #4 \033[0m]";
static const std::string STATE_FAIL_5   = "[\033[91mFAIL #5 \033[0m]";
static const std::string STATE_FAIL_6   = "[\033[91mFAIL #6 \033[0m]";
static const std::string STATE_FAIL_7   = "[\033[91mFAIL #7 \033[0m]";
