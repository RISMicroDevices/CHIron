// *NOTICE: CHI Issue B selected by default.
#if defined(CHI_ISSUE_EB_ENABLE)
#   define CHI_ISSUE_EB_ENABLED
#elif defined(CHI_ISSUE_B_ENABLE)
#   define CHI_ISSUE_B_ENABLED
#else
#   define CHI_ISSUE_B_ENABLE
#   define CHI_ISSUE_B_ENABLED
#endif
