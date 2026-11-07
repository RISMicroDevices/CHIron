#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using Config44 = CHI::Eb::FlitConfiguration<7, 44, 0, 0, 256, false, false, false>;
    using Config52 = CHI::Eb::FlitConfiguration<11, 52, 0, 0, 256, false, false, false>;

}

LUNATIC_TEST(Stage9InterfaceRejectsOutOfRangeHomeID)
{
    LUNATIC_CHECK(HomeNode<Config256>(127).GetNodeId() == 127);
    CheckThrows([&] { HomeNode<Config256> node(128); });
    LUNATIC_CHECK(HomeNode<Config52>(2047).GetNodeId() == 2047);
    CheckThrows([&] { HomeNode<Config52> node(2048); });
}

LUNATIC_TEST(Stage9InterfaceRejectsOutOfRangeCHISubordinate)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 127;
    LUNATIC_CHECK(HomeNode<Config256>(2, options).GetOptions().subordinateId == 127);
    options.subordinateId = 128;
    CheckThrows([&] { HomeNode<Config256> node(2, options); });

    options.backend = BackendKind::Internal;
    HomeNode<Config256> internal(2, options);
    LUNATIC_CHECK(internal.GetOptions().subordinateId == 128);
    options.backend = BackendKind::AXI;
    HomeNode<Config256> axi(2, options);
    LUNATIC_CHECK(axi.GetOptions().subordinateId == 128);
}

LUNATIC_TEST(Stage9InterfaceGeometryBoundaries)
{
    HomeNode<Config44> lower(2);
    HomeNode<Config52> upper(2047);
    LUNATIC_CHECK(lower.GetNodeId() == 2);
    LUNATIC_CHECK(upper.GetNodeId() == 2047);

    Driver<Config52> driver({}, 2047);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000)));
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.SrcID() == 2047);
        LUNATIC_CHECK(data.TgtID() == 4);
    }
    driver.Quiesce();
}
