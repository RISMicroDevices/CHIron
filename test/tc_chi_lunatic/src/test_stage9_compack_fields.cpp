#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void RejectCompAckWithoutExpCompAck(uint16_t opcode, bool afterFinalBeat)
    {
        Driver<Config256> driver;
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(opcode, 4, 2, 0x18100, false)));
        const auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        if (afterFinalBeat)
        {
            const auto second = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(second.DBID() == first.DBID());
        }

        driver.CompAck(4, first.DBID());
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
        LUNATIC_CHECK(driver.node.GetErrors().front().reason.find("unexpected or early CompAck") !=
                      std::string::npos);
        if (!afterFinalBeat)
        {
            const auto second = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(second.DBID() == first.DBID());
        }
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetPerformanceCounters().retiredTransactions == 1);
    }

}

LUNATIC_TEST(Stage9CompAckFieldRejectedWithoutExpCompAckEarly)
{
    RejectCompAckWithoutExpCompAck(CHI::Eb::Opcodes::REQ::ReadNoSnp, false);
}

LUNATIC_TEST(Stage9CompAckFieldRejectedWithoutExpCompAckFinal)
{
    RejectCompAckWithoutExpCompAck(CHI::Eb::Opcodes::REQ::ReadOnce, true);
}
