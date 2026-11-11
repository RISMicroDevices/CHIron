#include "driver.hpp"

#include <algorithm>
#include <array>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using MetadataConfig256 = CHI::Eb::FlitConfiguration<7, 48, 0, 0, 256, true, true, false>;

    Options BankingOptions()
    {
        Options options;
        options.policy = PolicyKind::NINE;
        options.slcSets = 1;
        options.slcWays = 1;
        options.snoopFilterSets = 1;
        options.snoopFilterWays = 1;
        options.structure.bankCount = 4;
        return options;
    }

    std::array<LineKey, 4> BankLines(uint64_t base = 0x40000)
    {
        std::array<LineKey, 4> lines{};
        for (size_t index = 0; index < lines.size(); ++index)
            lines[index] = LineKey::FromAddress(base + index * LINE_SIZE, false);
        return lines;
    }

    LineData Pattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 11);
        return data;
    }

    ByteEnables FullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    void AdmitFour(Driver<config>& driver, const std::array<LineKey, 4>& lines, uint16_t opcode,
                   TransactionId firstTxn, NodeId firstSource = 4)
    {
        for (size_t index = 0; index < lines.size(); ++index)
        {
            LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(
                opcode, firstSource + index, firstTxn + index, lines[index].address, false)));
        }
        for (size_t index = 0; index < lines.size(); ++index)
            driver.node.Tick(driver.now);
    }

    std::vector<Time> LookupTimes(const std::array<LineKey, 4>& lines)
    {
        Driver<Config256> driver(BankingOptions());
        std::vector<Time> lookups;
        driver.node.GetEvents()->OnTransactionSchedule.Register(
            Gravity::MakeListener<TransactionScheduleEvent>("banking-lookup", 0,
                [&](TransactionScheduleEvent& event) {
                    if (event.phase != Phase::Lookup)
                        return;
                    LUNATIC_CHECK(event.readyTime == event.time);
                    lookups.push_back(event.time);
                }));

        AdmitFour(driver, lines, CHI::Eb::Opcodes::REQ::ReadNoSnp, 10);
        driver.Until([&] { return lookups.size() == lines.size(); }, 32);
        LUNATIC_CHECK(driver.node.GetErrors().empty());
        LUNATIC_CHECK(driver.node.ValidateInvariants());
        return lookups;
    }

    void Writeback(Driver<Config256>& driver, NodeId source, TransactionId txnId,
                   const LineKey& line, const LineData& data)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(
            driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, source, txnId, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(source, txnId, response.DBID(), data, FullEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
        LUNATIC_CHECK(driver.node.ValidateInvariants());
    }

    void SnoopResponse(Driver<Config256>& driver, NodeId source, TransactionId txnId, uint8_t resp)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = source;
        response.TgtID() = driver.node.GetNodeId();
        response.TxnID() = txnId;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = resp;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    template<typename config>
    void SendBackendReadData(Driver<config>& driver, TransactionId txnId, const LineKey& line,
                             const LineData& data)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename Driver<config>::DAT flit{};
            flit.SrcID() = 6;
            flit.TgtID() = driver.node.GetNodeId();
            flit.HomeNID() = driver.node.GetNodeId();
            flit.TxnID() = txnId;
            flit.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            flit.Resp() = CHI::Eb::Resps::CompData_I;
            flit.DataID() = beat * bytes / 16;
            flit.CCID() = (line.address >> 4) & 3;
            for (size_t index = 0; index < bytes; ++index)
                flit.Data()[index / 8] |= uint64_t(data[beat * bytes + index]) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(flit));
        }
        driver.Tick();
    }

    struct ReadTraceEntry {
        EventKind     kind = EventKind::Error;
        Time          time = 0;
        TransactionId txnId = 0;
        uint16_t      opcode = 0;

        bool operator==(const ReadTraceEntry&) const = default;
    };

    class ReadReplay {
        Driver<Config256>             driver;
        std::vector<ReadTraceEntry>   trace;
        std::array<size_t, 4>         beats{};
        size_t                        retired = 0;

        template<typename Event>
        void Record(const Event& event)
        {
            trace.push_back({ event.kind, event.time, event.txnId, event.opcode });
        }

    public:
        explicit ReadReplay(const Options& options)
            : driver(options)
        {
            auto events = driver.node.GetEvents();
            events->OnAcceptedRXREQ.Register(
                Gravity::MakeListener<AcceptedRXREQEvent>("banking-replay-accepted", 0,
                    [this](AcceptedRXREQEvent& event) { Record(event); }));
            events->OnEmittedTXDAT.Register(
                Gravity::MakeListener<EmittedTXDATEvent>("banking-replay-emitted", 0,
                    [this](EmittedTXDATEvent& event) { Record(event); }));
            events->OnRetired.Register(
                Gravity::MakeListener<RetiredEvent>("banking-replay-retired", 0,
                    [this](RetiredEvent& event) {
                        Record(event);
                        ++retired;
                    }));
        }

        Driver<Config256>& DriverRef() { return driver; }

        std::vector<ReadTraceEntry> Run(const std::array<LineKey, 4>& lines, TransactionId firstTxn)
        {
            LUNATIC_CHECK(driver.node.GetTime() == 0);
            trace.clear();
            beats.fill(0);
            retired = 0;
            AdmitFour(driver, lines, CHI::Eb::Opcodes::REQ::ReadNoSnp, firstTxn);

            for (size_t cycle = 0; cycle < 128 && retired != lines.size(); ++cycle)
            {
                driver.Tick();
                while (const auto data = driver.node.PopTXDAT())
                {
                    LUNATIC_CHECK(data->Opcode() == CHI::Eb::Opcodes::DAT::CompData);
                    LUNATIC_CHECK(data->TxnID() >= firstTxn && data->TxnID() < firstTxn + lines.size());
                    const size_t transaction = data->TxnID() - firstTxn;
                    LUNATIC_CHECK(beats[transaction] < 2);
                    LUNATIC_CHECK(data->DataID() == beats[transaction] * 2);
                    const LineData expected = Pattern(20 + transaction);
                    constexpr size_t bytes = Config256::dataWidth / 8;
                    for (size_t index = 0; index < bytes; ++index)
                    {
                        const uint8_t actual = uint8_t(data->Data()[index / 8] >> ((index % 8) * 8));
                        LUNATIC_CHECK(actual == expected[beats[transaction] * bytes + index]);
                    }
                    ++beats[transaction];
                }
            }
            LUNATIC_CHECK(retired == lines.size());
            LUNATIC_CHECK((beats == std::array<size_t, 4>{ 2, 2, 2, 2 }));
            driver.Quiesce();
            LUNATIC_CHECK(driver.node.GetErrors().empty());
            LUNATIC_CHECK(driver.node.ValidateInvariants());
            return trace;
        }

        void ResetTime()
        {
            driver.now = 0;
        }
    };

}

LUNATIC_TEST(Stage10BankingDirectoryFilterSetMapping)
{
    Options options = BankingOptions();
    options.slcSets = 127;
    options.snoopFilterSets = 127;
    Directory directory(options);
    SnoopFilter snoopFilter(options);
    Directory legacyDirectory(127, 1);
    SnoopFilter legacySnoopFilter(127, 1);

    for (size_t line = 0; line < 1024; ++line)
    {
        const LineKey key = LineKey::FromAddress(line * LINE_SIZE, false);
        LUNATIC_CHECK(directory.SetIndex(key) == (line / 4) % 127);
        LUNATIC_CHECK(snoopFilter.SetIndex(key) == (line / 4) % 127);
        LUNATIC_CHECK(legacyDirectory.SetIndex(key) == line % 127);
        LUNATIC_CHECK(legacySnoopFilter.SetIndex(key) == line % 127);
    }
}

LUNATIC_TEST(Stage10BankingPerBankStorageAndInspect)
{
    const Options options = BankingOptions();
    Driver<Config256> driver(options);
    const auto lines = BankLines();
    BankMap map(options);
    for (size_t index = 0; index < lines.size(); ++index)
        LUNATIC_CHECK(map.Index(lines[index]) == index);

    for (size_t index = 0; index < lines.size(); ++index)
    {
        driver.node.GetMemory().WriteFull(lines[index], Pattern(30 + index));
        Writeback(driver, 4 + index, 10 + index, lines[index], Pattern(30 + index));
        LUNATIC_CHECK(driver.ReadData(4 + index, 20 + index, lines[index].address,
            CHI::Eb::Opcodes::REQ::ReadUnique, true) == Pattern(30 + index));
        driver.Quiesce();
    }

    for (size_t index = 0; index < lines.size(); ++index)
    {
        const auto home = driver.node.InspectHome(lines[index]);
        LUNATIC_CHECK(home.valid);
        LUNATIC_CHECK(home.data == Pattern(30 + index));
        LUNATIC_CHECK(driver.node.InspectRequester(lines[index], 4 + index) == CacheState::UD);
    }
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10BankingVictimDoesNotEvictOtherBanks)
{
    const Options options = BankingOptions();
    Driver<Config256> driver(options);
    const auto lines = BankLines();
    const LineKey replacement = LineKey::FromAddress(lines.front().address + 4 * LINE_SIZE, false);
    BankMap map(options);
    LUNATIC_CHECK(map.Index(replacement) == map.Index(lines.front()));

    for (size_t index = 0; index < lines.size(); ++index)
        Writeback(driver, 4 + index, 40 + index, lines[index], Pattern(40 + index));
    Writeback(driver, 8, 50, replacement, Pattern(50));

    LUNATIC_CHECK(!driver.node.InspectHome(lines.front()).valid);
    LUNATIC_CHECK(driver.node.InspectHome(replacement).valid);
    LUNATIC_CHECK(driver.node.InspectHome(replacement).data == Pattern(50));
    LUNATIC_CHECK(driver.node.GetMemory().Read(lines.front()) == Pattern(40));
    for (size_t index = 1; index < lines.size(); ++index)
    {
        LUNATIC_CHECK(map.Index(lines[index]) != map.Index(replacement));
        LUNATIC_CHECK(driver.node.InspectHome(lines[index]).valid);
        LUNATIC_CHECK(driver.node.InspectHome(lines[index]).data == Pattern(40 + index));
    }
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10BankingDifferentBanksLookupTogether)
{
    LUNATIC_CHECK((LookupTimes(BankLines()) == std::vector<Time>{ 6, 6, 6, 6 }));
}

LUNATIC_TEST(Stage10BankingSameBankAliasesSerialize)
{
    const auto lines = BankLines();
    const std::array<LineKey, 4> aliases = {
        lines[0],
        LineKey::FromAddress(lines[0].address + 4 * LINE_SIZE, false),
        LineKey::FromAddress(lines[0].address + 8 * LINE_SIZE, false),
        LineKey::FromAddress(lines[0].address + 12 * LINE_SIZE, false)
    };
    LUNATIC_CHECK((LookupTimes(aliases) == std::vector<Time>{ 6, 7, 8, 9 }));
}

LUNATIC_TEST(Stage10BankingSameLineOrderAndCrossBankCompletion)
{
    const Options options = BankingOptions();
    Driver<Config256> driver(options);
    const auto lines = BankLines();
    std::vector<std::pair<Time, TransactionId>> retired;
    driver.node.GetEvents()->OnRetired.Register(
        Gravity::MakeListener<RetiredEvent>("banking-retirement-order", 0,
            [&](RetiredEvent& event) { retired.push_back({ event.time, event.txnId }); }));

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 70, lines[0].address, false)));
    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 5, 71, lines[0].address, false)));
    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 6, 72, lines[1].address, false)));
    for (size_t index = 0; index < 3; ++index)
        driver.node.Tick(0);

    const auto firstDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(firstDBID.TxnID() == 70);
    driver.WriteData(4, 70, firstDBID.DBID(), Pattern(60), FullEnables(),
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);

    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TxnID() == 72);
    }
    driver.Tick();
    LUNATIC_CHECK((retired == std::vector<std::pair<Time, TransactionId>>{ { 20, 72 } }));
    const auto heldComp = driver.node.PeekTXRSP();
    LUNATIC_CHECK(heldComp);
    LUNATIC_CHECK(heldComp->Opcode() == CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(heldComp->TxnID() == 70);

    LUNATIC_CHECK(driver.node.PopTXRSP());
    driver.Tick();
    LUNATIC_CHECK(retired.size() == 2);
    LUNATIC_CHECK((retired.back() == std::pair<Time, TransactionId>{ 21, 70 }));

    const auto secondDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(secondDBID.TxnID() == 71);
    driver.WriteData(5, 71, secondDBID.DBID(), Pattern(61), FullEnables(),
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 71);
    driver.Quiesce();

    LUNATIC_CHECK(retired.size() == 3);
    LUNATIC_CHECK((retired[0] == std::pair<Time, TransactionId>{ 20, 72 }));
    LUNATIC_CHECK((retired[1] == std::pair<Time, TransactionId>{ 21, 70 }));
    LUNATIC_CHECK((retired[2] == std::pair<Time, TransactionId>{ 38, 71 }));
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10BankingGlobalDBIDUniqueness)
{
    const Options options = BankingOptions();
    Driver<Config256> driver(options);
    const auto lines = BankLines();
    AdmitFour(driver, lines, CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 80);

    std::array<DataBufferId, 4> ids{};
    for (size_t index = 0; index < lines.size(); ++index)
    {
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(response.TgtID() == 4 + index);
        LUNATIC_CHECK(response.TxnID() == 80 + index);
        ids[index] = response.DBID();
    }
    auto sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    LUNATIC_CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());

    for (size_t index = 0; index < lines.size(); ++index)
        driver.WriteData(4 + index, 80 + index, ids[index], Pattern(70 + index), FullEnables(),
            CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    for (size_t index = 0; index < lines.size(); ++index)
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 80 + index);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10BankingGlobalSnoopIDUniqueness)
{
    const Options options = BankingOptions();
    Driver<Config256> driver(options);
    const auto lines = BankLines();
    for (size_t index = 0; index < lines.size(); ++index)
    {
        const LineData expected = Pattern(80 + index);
        driver.node.GetMemory().WriteFull(lines[index], expected);
        LUNATIC_CHECK(driver.ReadData(4, 90 + index, lines[index].address,
            CHI::Eb::Opcodes::REQ::ReadUnique, true) == expected);
        driver.Quiesce();
    }

    AdmitFour(driver, lines, CHI::Eb::Opcodes::REQ::ReadUnique, 100, 8);
    std::array<TransactionId, 4> ids{};
    for (size_t index = 0; index < lines.size(); ++index)
    {
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(snoop.target == 4);
        ids[index] = snoop.flit.TxnID();
    }
    auto sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    LUNATIC_CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());

    for (TransactionId id : ids)
        SnoopResponse(driver, 4, id, CHI::Eb::Resps::SnpResp_I);
    for (size_t index = 0; index < lines.size(); ++index)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == 8 + index);
            LUNATIC_CHECK(data.TxnID() == 100 + index);
            dbid = data.DBID();
        }
        driver.CompAck(8 + index, dbid);
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10BankingResetClearsAllBanksAndReplays)
{
    const Options options = BankingOptions();
    const auto lines = BankLines();
    ReadReplay replay(options);
    for (size_t index = 0; index < lines.size(); ++index)
        replay.DriverRef().node.GetMemory().WriteFull(lines[index], Pattern(20 + index));

    const auto baseline = replay.Run(lines, 110);
    for (size_t index = 0; index < lines.size(); ++index)
        Writeback(replay.DriverRef(), 12 + index, 120 + index, lines[index], Pattern(20 + index));
    for (size_t index = 0; index < lines.size(); ++index)
        LUNATIC_CHECK(replay.DriverRef().node.InspectHome(lines[index]).valid);

    replay.DriverRef().node.Reset();
    replay.ResetTime();
    LUNATIC_CHECK(!replay.DriverRef().node.HasInflight());
    for (size_t index = 0; index < lines.size(); ++index)
    {
        LUNATIC_CHECK(!replay.DriverRef().node.InspectHome(lines[index]).valid);
        LUNATIC_CHECK(replay.DriverRef().node.InspectRequester(lines[index], 12 + index) == CacheState::I);
        replay.DriverRef().node.GetMemory().WriteFull(lines[index], Pattern(20 + index));
    }
    const auto repeated = replay.Run(lines, 110);
    LUNATIC_CHECK(repeated == baseline);
}

LUNATIC_TEST(Stage10BankingSingleBankLegacyTiming)
{
    Options options;
    options.structure.bankCount = 1;
    Driver<Config256> driver(options);
    std::vector<Time> retired;
    driver.node.GetEvents()->OnRetired.Register(
        Gravity::MakeListener<RetiredEvent>("banking-legacy-retired", 0,
            [&](RetiredEvent& event) { retired.push_back(event.time); }));

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 130, 0x50000, false)));
    while (driver.now < 18)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
    }
    driver.Tick();
    LUNATIC_CHECK(driver.node.PeekTXDAT());
    LUNATIC_CHECK(driver.node.PopTXDAT());
    driver.Tick();
    LUNATIC_CHECK(driver.node.PeekTXDAT());
    LUNATIC_CHECK(driver.node.PopTXDAT());
    LUNATIC_CHECK(retired.empty());
    driver.Tick();
    LUNATIC_CHECK(retired == std::vector<Time>{ 21 });
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage10BankingType3MetadataSmoke)
{
    Options options = MakeType3Options();
    options.subordinateId = 6;
    Driver<MetadataConfig256> driver(options);
    const auto lines = BankLines(0x60000);
    AdmitFour(driver, lines, CHI::Eb::Opcodes::REQ::ReadNoSnp, 140);

    std::array<TransactionId, 4> downstream{};
    for (size_t index = 0; index < lines.size(); ++index)
    {
        const auto request = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(request.TgtID() == 6);
        downstream[index] = request.TxnID();
    }
    for (size_t index = 0; index < lines.size(); ++index)
        SendBackendReadData(driver, downstream[index], lines[index], Pattern(90 + index));

    for (size_t transaction = 0; transaction < lines.size(); ++transaction)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == 140 + transaction);
            LUNATIC_CHECK(data.DataID() == beat * 2);
            const LineData expected = Pattern(90 + transaction);
            for (size_t index = 0; index < 32; ++index)
            {
                const uint8_t actual = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
                LUNATIC_CHECK(actual == expected[beat * 32 + index]);
            }
        }
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetOptions().profile == ProfileKind::Type3);
    LUNATIC_CHECK(BankMap(driver.node.GetOptions()).Count() == 4);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}
