#define CLOG2COVERAGE__STANDALONE

#define CHI_ISSUE_EB_ENABLE
#include "clog2coverage.hpp"
#undef  CHI_ISSUE_EB_ENABLE

int main(int argc, char* argv[])
{
    return clog2coverage(argc, argv);
}
