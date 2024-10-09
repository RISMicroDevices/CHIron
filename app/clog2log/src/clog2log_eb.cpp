#define CLOG2LOG__STANDALONE

#define CHI_ISSUE_EB_ENABLE
#include "clog2log.hpp"
#undef  CHI_ISSUE_EB_ENABLE

int main(int argc, char* argv[])
{
    return clog2log(argc, argv);
}
