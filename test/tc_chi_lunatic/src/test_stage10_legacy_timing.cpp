#include "driver.hpp"

#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    class TimingTrace {
    public:
        std::vector<Time> acceptedRequests;
        std::vector<Time> acceptedData;
        std::vector<Time> emittedResponses;
        std::vector<Time> emittedData;
        std::vector<Time> retired;

        explicit TimingTrace(Driver<Config256>& driver)
        {
            auto events = driver.node.GetEvents();
            events->OnAcceptedRXREQ.Register(
                Gravity::MakeListener<AcceptedRXREQEvent>("stage10-accepted-request", 0,
                    [this](AcceptedRXREQEvent& event) { acceptedRequests.push_back(event.time); }));
            events->OnAcceptedRXDAT.Register(
                Gravity::MakeListener<AcceptedRXDATEvent>("stage10-accepted-data", 0,
                    [this](AcceptedRXDATEvent& event) { acceptedData.push_back(event.time); }));
            events->OnEmittedTXRSP.Register(
                Gravity::MakeListener<EmittedTXRSPEvent>("stage10-emitted-response", 0,
                    [this](EmittedTXRSPEvent& event) { emittedResponses.push_back(event.time); }));
            events->OnEmittedTXDAT.Register(
                Gravity::MakeListener<EmittedTXDATEvent>("stage10-emitted-data", 0,
                    [this](EmittedTXDATEvent& event) { emittedData.push_back(event.time); }));
            events->OnRetired.Register(
                Gravity::MakeListener<RetiredEvent>("stage10-retired", 0,
                    [this](RetiredEvent& event) { retired.push_back(event.time); }));
        }
    };

    ByteEnables FullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

}

LUNATIC_TEST(Stage10LegacyTimingInternalReadTrace)
{
    Driver<Config256> driver;
    TimingTrace trace(driver);
    std::vector<Time> queuedData;

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 31, 0x18000, false)));
    LUNATIC_CHECK(trace.acceptedRequests == std::vector<Time>{ 0 });

    while (driver.now < 18)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
    }
    driver.Tick();
    const auto firstQueued = driver.node.PeekTXDAT();
    LUNATIC_CHECK(firstQueued);
    LUNATIC_CHECK(firstQueued->DataID() == 0);
    queuedData.push_back(driver.now);
    const auto first = driver.node.PopTXDAT();
    LUNATIC_CHECK(first);
    LUNATIC_CHECK(first->Opcode() == CHI::Eb::Opcodes::DAT::CompData);

    driver.Tick();
    const auto secondQueued = driver.node.PeekTXDAT();
    LUNATIC_CHECK(secondQueued);
    LUNATIC_CHECK(secondQueued->DataID() == 2);
    queuedData.push_back(driver.now);
    const auto second = driver.node.PopTXDAT();
    LUNATIC_CHECK(second);
    LUNATIC_CHECK(second->Opcode() == CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(trace.retired.empty());

    driver.Tick();
    LUNATIC_CHECK((queuedData == std::vector<Time>{ 19, 20 }));
    LUNATIC_CHECK((trace.emittedData == std::vector<Time>{ 19, 20 }));
    LUNATIC_CHECK(trace.retired == std::vector<Time>{ 21 });
    LUNATIC_CHECK(trace.emittedResponses.empty());
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10LegacyTimingInternalWriteTrace)
{
    Driver<Config256> driver;
    TimingTrace trace(driver);
    std::vector<Time> queuedResponses;
    const LineKey line = LineKey::FromAddress(0x1c000, false);
    LineData update{};
    for (size_t index = 0; index < update.size(); ++index)
        update[index] = uint8_t(index * 7 + 3);

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 32, line.address, false)));
    LUNATIC_CHECK(trace.acceptedRequests == std::vector<Time>{ 0 });

    while (driver.now < 9)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
    }
    driver.Tick();
    const auto dbidQueued = driver.node.PeekTXRSP();
    LUNATIC_CHECK(dbidQueued);
    LUNATIC_CHECK(dbidQueued->Opcode() == CHI::Eb::Opcodes::RSP::DBIDResp);
    queuedResponses.push_back(driver.now);
    const auto dbid = driver.node.PopTXRSP();
    LUNATIC_CHECK(dbid);
    driver.WriteData(4, 32, dbid->DBID(), update, FullEnables(),
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(!driver.node.PeekTXRSP());

    while (driver.now < 20)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
    }
    driver.Tick();
    const auto compQueued = driver.node.PeekTXRSP();
    LUNATIC_CHECK(compQueued);
    LUNATIC_CHECK(compQueued->Opcode() == CHI::Eb::Opcodes::RSP::Comp);
    queuedResponses.push_back(driver.now);
    const auto comp = driver.node.PopTXRSP();
    LUNATIC_CHECK(comp);
    LUNATIC_CHECK(trace.retired.empty());

    driver.Tick();
    LUNATIC_CHECK((queuedResponses == std::vector<Time>{ 10, 21 }));
    LUNATIC_CHECK((trace.emittedResponses == std::vector<Time>{ 10, 21 }));
    LUNATIC_CHECK((trace.acceptedData == std::vector<Time>{ 10, 10 }));
    LUNATIC_CHECK(trace.retired == std::vector<Time>{ 22 });
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == update);
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10LegacyTimingResponseQueuedBeforeReturnDelay)
{
    Options options;
    options.returnDelay = 6;
    Driver<Config256> driver(options);
    TimingTrace trace(driver);

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpZero, 4, 33, 0x20000, false)));
    while (driver.now < 17)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
    }
    driver.Tick();
    const auto queued = driver.node.PeekTXRSP();
    LUNATIC_CHECK(queued);
    LUNATIC_CHECK(queued->Opcode() == CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(driver.now == 18);
    const auto response = driver.node.PopTXRSP();
    LUNATIC_CHECK(response);
    LUNATIC_CHECK(trace.retired.empty());

    driver.Tick();
    LUNATIC_CHECK(trace.emittedResponses == std::vector<Time>{ 18 });
    LUNATIC_CHECK(trace.retired == std::vector<Time>{ 19 });
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10LegacyTimingContinuousReturnDelay)
{
    Options options;
    options.returnDelay = 4;
    Driver<Config256> driver(options);
    TimingTrace trace(driver);
    std::vector<Time> queuedData;

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 34, 0x24000, false)));
    while (driver.now < 21)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
    }

    driver.Tick();
    LUNATIC_CHECK(driver.node.PeekTXDAT());
    queuedData.push_back(driver.now);
    LUNATIC_CHECK(driver.node.PopTXDAT());
    driver.Tick();
    LUNATIC_CHECK(driver.node.PeekTXDAT());
    queuedData.push_back(driver.now);
    LUNATIC_CHECK(driver.node.PopTXDAT());
    LUNATIC_CHECK(trace.retired.empty());
    driver.Tick();

    LUNATIC_CHECK((queuedData == std::vector<Time>{ 22, 23 }));
    LUNATIC_CHECK((trace.emittedData == std::vector<Time>{ 22, 23 }));
    LUNATIC_CHECK(trace.retired == std::vector<Time>{ 24 });
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10LegacyTimingActionDelayReentry)
{
    Options options;
    options.actionDelay = 3;
    Driver<Config256> driver(options);
    TimingTrace trace(driver);
    std::vector<Time> queuedResponses;
    const LineData update{};

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 35, 0x28000, false)));
    while (driver.now < 12)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
    }

    driver.Tick();
    const auto dbidQueued = driver.node.PeekTXRSP();
    LUNATIC_CHECK(dbidQueued);
    LUNATIC_CHECK(dbidQueued->Opcode() == CHI::Eb::Opcodes::RSP::DBIDResp);
    queuedResponses.push_back(driver.now);
    const auto dbid = driver.node.PopTXRSP();
    LUNATIC_CHECK(dbid);
    driver.WriteData(4, 35, dbid->DBID(), update, FullEnables(),
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);

    while (driver.now < 26)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
    }
    driver.Tick();
    const auto compQueued = driver.node.PeekTXRSP();
    LUNATIC_CHECK(compQueued);
    LUNATIC_CHECK(compQueued->Opcode() == CHI::Eb::Opcodes::RSP::Comp);
    queuedResponses.push_back(driver.now);
    LUNATIC_CHECK(driver.node.PopTXRSP());
    LUNATIC_CHECK(trace.retired.empty());
    driver.Tick();

    LUNATIC_CHECK((queuedResponses == std::vector<Time>{ 13, 27 }));
    LUNATIC_CHECK((trace.emittedResponses == std::vector<Time>{ 13, 27 }));
    LUNATIC_CHECK((trace.acceptedData == std::vector<Time>{ 13, 13 }));
    LUNATIC_CHECK(trace.retired == std::vector<Time>{ 28 });
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}

LUNATIC_TEST(Stage10LegacyTimingActionCommitRetirementBypass)
{
    Options options;
    options.commitDelay = 9;
    Driver<Config256> driver(options);
    TimingTrace trace(driver);
    const LineData update{};

    LUNATIC_CHECK(driver.node.PushRXREQ(
        driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 36, 0x2c000, false)));
    while (driver.now < 9)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
    }

    driver.Tick();
    const auto queued = driver.node.PeekTXRSP();
    LUNATIC_CHECK(queued);
    LUNATIC_CHECK(queued->Opcode() == CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(driver.now == 10);
    const auto response = driver.node.PopTXRSP();
    LUNATIC_CHECK(response);
    driver.WriteData(4, 36, response->DBID(), update, FullEnables(),
        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);

    LUNATIC_CHECK(driver.now == 11);
    LUNATIC_CHECK(trace.retired == std::vector<Time>{ 11 });
    LUNATIC_CHECK(trace.emittedResponses == std::vector<Time>{ 10 });
    LUNATIC_CHECK((trace.acceptedData == std::vector<Time>{ 10, 10 }));
    LUNATIC_CHECK(driver.node.Drain());
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.ValidateInvariants());
}
