#define REPORT_FLIT__STANDALONE

#define CHI_ISSUE_EB_ENABLE
#include "report_flit.hpp"
#undef  CHI_ISSUE_EB_ENABLE

int main()
{
    return report_flit_main();
}
