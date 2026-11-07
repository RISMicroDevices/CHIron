#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void CheckWriteDataHomeNID()
    {
        Driver<config> driver;
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 9, 0x6c000);
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        LineData line{};
        for (size_t index = 0; index < line.size(); ++index)
            line[index] = uint8_t(0x51 + index * 3);
        ByteEnables enables{};
        enables.fill(1);
        std::vector<typename Driver<config>::DAT> generated;
        driver.WriteData(4, 9, dbid, line, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
                         0, 0, &generated);

        constexpr size_t bytes = config::dataWidth / 8;
        LUNATIC_CHECK(generated.size() == LINE_SIZE / bytes);
        for (size_t beat = 0; beat < generated.size(); ++beat)
        {
            const auto& packet = generated[beat];
            LUNATIC_CHECK(packet.HomeNID() == 0);
            LUNATIC_CHECK(packet.SrcID() == 4 && packet.TgtID() == 2);
            LUNATIC_CHECK(packet.TxnID() == dbid && packet.DBID() == 0);
            LUNATIC_CHECK(packet.DataID() == beat * bytes / 16 && packet.CCID() == 0);
            for (size_t index = 0; index < bytes; ++index)
            {
                LUNATIC_CHECK((uint64_t(packet.BE()) >> index) & 1);
                const uint8_t actual = uint8_t(packet.Data()[index / 8] >> ((index % 8) * 8));
                LUNATIC_CHECK(actual == line[beat * bytes + index]);
            }
        }
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9DriverWriteDataHomeNID128)
{
    CheckWriteDataHomeNID<Config128>();
}

LUNATIC_TEST(Stage9DriverWriteDataHomeNID256)
{
    CheckWriteDataHomeNID<Config256>();
}

LUNATIC_TEST(Stage9DriverWriteDataHomeNID512)
{
    CheckWriteDataHomeNID<Config512>();
}
