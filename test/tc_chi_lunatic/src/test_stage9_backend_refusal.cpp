#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData RefusalPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 5);
        return data;
    }

    void SendReadData(Driver<Config256>& driver, const Driver<Config256>::REQ& request, const LineData& line)
    {
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = request.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (request.Addr() >> 4) & 3;
            for (size_t index = 0; index < bytes; ++index)
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

}

LUNATIC_TEST(Stage9CHIBackendReadQueueRefusalRetries)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x62000, false);
    const LineKey b = LineKey::FromAddress(0x62040, false);
    const auto dataA = RefusalPattern(23);
    const auto dataB = RefusalPattern(91);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, a.address)));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXREQ()); });
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2, b.address)));
    for (size_t cycle = 0; cycle < 4; ++cycle)
        driver.Tick();
    LUNATIC_CHECK(driver.node.PeekTXREQ().has_value());

    const auto first = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(first.TxnID() == 0);
    SendReadData(driver, first, dataA);

    const auto second = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(second.TxnID() == 1);
    SendReadData(driver, second, dataB);

    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).TgtID() == 4);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).TgtID() == 5);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
