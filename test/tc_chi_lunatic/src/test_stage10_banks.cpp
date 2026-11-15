#include "common.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage10BankMapRejectsInvalidGeometry)
{
    CheckThrows([] { BankMap map(0); });
    CheckThrows([] { BankMap map(3); });
    CheckThrows([] { BankMap map(6); });
    CheckThrows([] { BankMap map(size_t(1) << (sizeof(size_t) * 8 - 2)); });

    BankMap map(4);
    const LineKey line = LineKey::FromAddress(0x1000, false);
    CheckThrows([&] { map.SetIndex(line, 0); });
}

LUNATIC_TEST(Stage10BankMapPreservesLegacySingleBankSets)
{
    for (size_t count : { size_t(1), size_t(2), size_t(4), size_t(8) })
    {
        BankMap map(count);
        LUNATIC_CHECK(map.Count() == count);
        if (count != 1)
            continue;

        for (size_t line = 0; line < 1024; ++line)
        {
            const LineKey key = LineKey::FromAddress(line * LINE_SIZE, false);
            LUNATIC_CHECK(map.Index(key) == 0);
            LUNATIC_CHECK(map.SetIndex(key, 127) == line % 127);
            LUNATIC_CHECK(map.SetIndex(key, 128) == line % 128);
        }
    }
}

LUNATIC_TEST(Stage10BankMapUsesLineOffsetBankBits)
{
    BankMap map(4);
    for (size_t line = 0; line < 1024; ++line)
    {
        const LineKey key = LineKey::FromAddress(line * LINE_SIZE, line % 2 != 0);
        LUNATIC_CHECK(map.Index(key) == line % 4);
        LUNATIC_CHECK(map.SetIndex(key, 128) == (line / 4) % 128);
    }

    LUNATIC_CHECK(map.Index(LineKey::FromAddress(0x0000, false)) == 0);
    LUNATIC_CHECK(map.Index(LineKey::FromAddress(0x0040, false)) == 1);
    LUNATIC_CHECK(map.Index(LineKey::FromAddress(0x0080, false)) == 2);
    LUNATIC_CHECK(map.Index(LineKey::FromAddress(0x00c0, false)) == 3);
    LUNATIC_CHECK(map.Index(LineKey::FromAddress(0x0100, false)) == 0);
    LUNATIC_CHECK(map.SetIndex(LineKey::FromAddress(0x0100, false), 128) == 1);
    LUNATIC_CHECK(map.SetIndex(LineKey::FromAddress(0x0140, false), 128) == 1);
}

LUNATIC_TEST(Stage10BankMapSupportsNonPowerOfTwoSets)
{
    BankMap map(4);
    for (size_t line = 0; line < 1024; ++line)
    {
        const LineKey key = LineKey::FromAddress(line * LINE_SIZE, false);
        LUNATIC_CHECK(map.SetIndex(key, 127) == (line / 4) % 127);
    }
}

LUNATIC_TEST(Stage10BankMapUsesPackagedGeometry)
{
    BankMap type3(MakeType3Options());
    BankMap type7(MakeType7Options());
    LUNATIC_CHECK(type3.Count() == 4);
    LUNATIC_CHECK(type7.Count() == 1);
}
