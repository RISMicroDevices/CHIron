#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void ExerciseEarlyReadAck(uint16_t opcode, bool expCompAck)
    {
        Driver<Config256> driver;
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(opcode, 4, 2, 0x18000, expCompAck)));
        const auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.CompAck(4, first.DBID());
        const auto second = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(second.DBID() == first.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9CompAckAfterFirstReadDataBeat)
{
    ExerciseEarlyReadAck(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, true);
}

LUNATIC_TEST(Stage9CompAckOptionalReadNoSnp)
{
    ExerciseEarlyReadAck(CHI::Eb::Opcodes::REQ::ReadNoSnp, true);
}

LUNATIC_TEST(Stage9CompAckOptionalReadOnce)
{
    ExerciseEarlyReadAck(CHI::Eb::Opcodes::REQ::ReadOnce, true);
}

LUNATIC_TEST(Stage9CompAckOptionalAfterFinalDataBeat)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x18000, true)));
    const auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    const auto second = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(second.DBID() == first.DBID());
    driver.CompAck(4, second.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9CompAckRejectedBeforeAnyDataBeat)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, 4, 2, 0x18000, true)));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXDAT()); });
    const auto dbid = driver.node.PeekTXDAT()->DBID();
    driver.CompAck(4, dbid);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9CompAckDuplicateRejected)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, 4, 2, 0x18000, true)));
    const auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.CompAck(4, first.DBID());
    driver.CompAck(4, first.DBID());
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
}
