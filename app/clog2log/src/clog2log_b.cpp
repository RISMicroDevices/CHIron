#define CLOG2LOG__STANDALONE

#define CHI_ISSUE_B_ENABLE
#include "clog2log.hpp"
#undef  CHI_ISSUE_B_ENABLE

int main(int argc, char* argv[])
{
    return clog2log(argc, argv);
}
