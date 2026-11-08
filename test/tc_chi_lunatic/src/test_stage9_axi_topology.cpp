#include "common.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage9AXICachelessTopologyProfile)
{
    Options options;
    options.backend = BackendKind::AXI;
    LUNATIC_CHECK(options.axiCachelessSingleManager);
    ValidateOptions(options);

    options.axiCachelessSingleManager = false;
    CheckThrows([&] { ValidateOptions(options); });

    options.backend = BackendKind::Internal;
    ValidateOptions(options);
    options.backend = BackendKind::CHI;
    ValidateOptions(options);
}
