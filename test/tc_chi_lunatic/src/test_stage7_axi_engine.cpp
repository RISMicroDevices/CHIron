#include "common.hpp"
#include "axi_model.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Options EngineOptions(size_t dataWidth = 256)
    {
        Options options;
        options.dataWidth = dataWidth;
        options.inputQueueLimit = 16;
        options.outputQueueLimit = 16;
        options.axiOutstandingLimit = 8;
        options.axiIdWidth = 4;
        return options;
    }

    uint64_t FullBeatMask(size_t dataWidth)
    {
        const size_t beats = LINE_SIZE / (dataWidth / 8);
        return (uint64_t(1) << beats) - 1;
    }

    LineData Pattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 13 + index / 3);
        return data;
    }

    AXIOperation Operation(TransactionHandle transaction, uint64_t address, bool write,
                           uint64_t beatMask, const LineData& data = {})
    {
        AXIOperation operation;
        operation.transaction = transaction;
        operation.line = LineKey::FromAddress(address, true);
        operation.data = data;
        operation.beatMask = beatMask;
        operation.qos = 11;
        operation.write = write;
        if (write)
            operation.enables.fill(1);
        return operation;
    }

    AXIReadData ReadBeat(uint32_t id, const LineData& data, size_t dataWidth, size_t beat,
                         AXIResp response = AXIResp::OKAY)
    {
        const size_t bytes = dataWidth / 8;
        const size_t beats = LINE_SIZE / bytes;
        AXIReadData result;
        result.id = id;
        result.resp = response;
        result.last = beat + 1 == beats;
        for (size_t index = 0; index < bytes; ++index)
            result.data[index] = data[beat * bytes + index];
        return result;
    }

    void CheckAddress(const AXIAddress& address, const AXIOperation& operation, size_t dataWidth)
    {
        const size_t bytes = dataWidth / 8;
        const size_t first = size_t(std::countr_zero(operation.beatMask));
        LUNATIC_CHECK(address.address == operation.line.address + first * bytes);
        LUNATIC_CHECK(address.len == std::popcount(operation.beatMask) - 1);
        LUNATIC_CHECK(address.size == std::countr_zero(bytes));
        LUNATIC_CHECK(address.burst == AXIBurst::INCR);
        LUNATIC_CHECK(address.cache == (operation.write ? AXIWriteCache(operation.memAttr)
                                                       : AXIReadCache(operation.memAttr)));
        LUNATIC_CHECK(address.qos == operation.qos);
        LUNATIC_CHECK(address.prot == 0b010);
    }

    void CheckCompletion(const AXICompletion& completion, const AXIOperation& operation,
                         AXIResp response = AXIResp::OKAY, const char* protocolError = "")
    {
        LUNATIC_CHECK(completion.transaction == operation.transaction);
        LUNATIC_CHECK(completion.line == operation.line);
        LUNATIC_CHECK(completion.beatMask == operation.beatMask);
        LUNATIC_CHECK(completion.response == response);
        LUNATIC_CHECK(completion.write == operation.write);
        LUNATIC_CHECK(completion.protocolError == protocolError);
    }

    void CheckSameCompletion(const AXICompletion& actual, const AXICompletion& expected)
    {
        LUNATIC_CHECK(actual.transaction == expected.transaction);
        LUNATIC_CHECK(actual.line == expected.line);
        LUNATIC_CHECK(actual.data == expected.data);
        LUNATIC_CHECK(actual.beatMask == expected.beatMask);
        LUNATIC_CHECK(actual.response == expected.response);
        LUNATIC_CHECK(actual.write == expected.write);
        LUNATIC_CHECK(actual.protocolError == expected.protocolError);
    }

    template<typename Predicate>
    void RunModel(AXIBackend& backend, AXIModel& model, Predicate&& predicate, size_t limit = 128)
    {
        for (size_t cycle = 0; cycle < limit; ++cycle)
        {
            if (predicate())
                return;
            model.Tick(backend);
            backend.Tick();
        }
        if (!predicate())
            throw std::runtime_error("AXI engine test condition timed out");
    }

    void CheckLineReadWrite(size_t dataWidth, uint8_t seed)
    {
        AXIBackend backend(EngineOptions(dataWidth));
        AXIModel model(dataWidth / 8);
        const uint64_t address = 0x80000 + dataWidth * 0x40;
        const LineData data = Pattern(seed);
        const auto write = Operation(100 + dataWidth, address, true, FullBeatMask(dataWidth), data);

        LUNATIC_CHECK(backend.Submit(write));
        LUNATIC_CHECK(backend.HasInflight());
        LUNATIC_CHECK(backend.Outstanding(true) == 1);
        RunModel(backend, model, [&] { return static_cast<bool>(backend.PeekCompletion()); });

        LUNATIC_CHECK(model.Errors().empty());
        LUNATIC_CHECK(model.CompletedWrites().size() == 1);
        const auto& completedWrite = model.CompletedWrites().front();
        CheckAddress(completedWrite.address, write, dataWidth);
        LUNATIC_CHECK(completedWrite.response == AXIResp::OKAY);
        LUNATIC_CHECK(completedWrite.beats.size() == LINE_SIZE / (dataWidth / 8));
        const size_t bytes = dataWidth / 8;
        for (size_t beat = 0; beat < completedWrite.beats.size(); ++beat)
        {
            const auto& channelData = completedWrite.beats[beat];
            LUNATIC_CHECK(channelData.last == (beat + 1 == completedWrite.beats.size()));
            for (size_t index = 0; index < bytes; ++index)
            {
                LUNATIC_CHECK(channelData.data[index] == data[beat * bytes + index]);
                LUNATIC_CHECK(channelData.strobes[index] == 1);
            }
            for (size_t index = bytes; index < LINE_SIZE; ++index)
            {
                LUNATIC_CHECK(channelData.data[index] == 0);
                LUNATIC_CHECK(channelData.strobes[index] == 0);
            }
        }
        LUNATIC_CHECK(model.ReadLine(address) == data);
        auto completion = backend.PeekCompletion();
        LUNATIC_CHECK(completion);
        CheckCompletion(*completion, write);
        completion = backend.PopCompletion();
        LUNATIC_CHECK(completion);
        CheckCompletion(*completion, write);
        LUNATIC_CHECK(backend.Outstanding(true) == 0);
        LUNATIC_CHECK(!backend.HasInflight());
        LUNATIC_CHECK(model.Drain());

        const auto read = Operation(200 + dataWidth, address, false, FullBeatMask(dataWidth));
        LUNATIC_CHECK(backend.Submit(read));
        RunModel(backend, model, [&] { return static_cast<bool>(backend.PeekCompletion()); });
        LUNATIC_CHECK(model.Errors().empty());
        LUNATIC_CHECK(model.CompletedReads().size() == 1);
        const auto& completedRead = model.CompletedReads().front();
        CheckAddress(completedRead.address, read, dataWidth);
        LUNATIC_CHECK(completedRead.beats.size() == LINE_SIZE / bytes);
        for (size_t beat = 0; beat < completedRead.beats.size(); ++beat)
        {
            LUNATIC_CHECK(completedRead.beats[beat].id == completedRead.address.id);
            LUNATIC_CHECK(completedRead.beats[beat].last == (beat + 1 == completedRead.beats.size()));
            LUNATIC_CHECK(completedRead.beats[beat].resp == AXIResp::OKAY);
        }
        completion = backend.PopCompletion();
        LUNATIC_CHECK(completion);
        CheckCompletion(*completion, read);
        LUNATIC_CHECK(completion->data == data);
        LUNATIC_CHECK(!backend.PeekCompletion());
        LUNATIC_CHECK(!backend.HasInflight());
        LUNATIC_CHECK(!backend.PopError());
        LUNATIC_CHECK(model.Drain());
    }

    void CheckErrorCompletion(bool write, AXIResp response)
    {
        AXIBackend backend(EngineOptions(512));
        AXIModel model(64);
        if (write)
            model.ScriptWriteResponse(response);
        else
        {
            model.ScriptReadResponse(response);
            model.SetLine(0x8a000, Pattern(73));
        }
        const auto operation = Operation(write ? 501 : 502, 0x8a000, write, 1, Pattern(73));
        LUNATIC_CHECK(backend.Submit(operation));
        RunModel(backend, model, [&] { return static_cast<bool>(backend.PeekCompletion()); });
        const auto completion = backend.PopCompletion();
        LUNATIC_CHECK(completion);
        CheckCompletion(*completion, operation, response);
        if (write)
        {
            LUNATIC_CHECK(model.CompletedWrites().size() == 1);
            LUNATIC_CHECK(model.CompletedWrites().front().response == response);
        }
        else
        {
            LUNATIC_CHECK(model.CompletedReads().size() == 1);
            LUNATIC_CHECK(completion->data == Pattern(73));
        }
        LUNATIC_CHECK(!backend.HasInflight());
        LUNATIC_CHECK(!backend.PopError());
        LUNATIC_CHECK(model.Errors().empty());
    }

}

LUNATIC_TEST(Stage7AXIEngineRepeatedPeekAndPopAgreement)
{
    AXIBackend backend(EngineOptions(256));
    const LineData writeData = Pattern(5);
    const LineData readData = Pattern(91);
    const auto write = Operation(1, 0x81000, true, FullBeatMask(256), writeData);
    const auto read = Operation(2, 0x82000, false, FullBeatMask(256));
    LUNATIC_CHECK(backend.Submit(write));
    LUNATIC_CHECK(backend.Submit(read));

    const auto aw = backend.PeekAW();
    const auto ar = backend.PeekAR();
    LUNATIC_CHECK(aw && ar);
    CheckAddress(*aw, write, 256);
    CheckAddress(*ar, read, 256);
    for (size_t iteration = 0; iteration < 12; ++iteration)
    {
        LUNATIC_CHECK(backend.PeekAW() == aw);
        LUNATIC_CHECK(backend.PeekAR() == ar);
        LUNATIC_CHECK(!backend.PeekW());
        LUNATIC_CHECK(!backend.PeekCompletion());
        LUNATIC_CHECK(backend.Outstanding(true) == 1);
        LUNATIC_CHECK(backend.Outstanding(false) == 1);
    }
    LUNATIC_CHECK(backend.PopAW() == aw);
    LUNATIC_CHECK(backend.PopAR() == ar);
    LUNATIC_CHECK(!backend.PeekAW());
    LUNATIC_CHECK(!backend.PeekAR());

    const auto firstW = backend.PeekW();
    LUNATIC_CHECK(firstW && !firstW->last);
    for (size_t iteration = 0; iteration < 12; ++iteration)
    {
        LUNATIC_CHECK(backend.PeekW() == firstW);
        LUNATIC_CHECK(backend.Outstanding(true) == 1);
        LUNATIC_CHECK(backend.HasInflight());
    }
    LUNATIC_CHECK(backend.PopW() == firstW);
    const auto lastW = backend.PopW();
    LUNATIC_CHECK(lastW && lastW->last);
    LUNATIC_CHECK(!backend.PopW());

    LUNATIC_CHECK(backend.PushB({ aw->id, AXIResp::OKAY }));
    LUNATIC_CHECK(backend.PushR(ReadBeat(ar->id, readData, 256, 0)));
    LUNATIC_CHECK(backend.PushR(ReadBeat(ar->id, readData, 256, 1)));
    backend.Tick();

    const auto writeCompletion = backend.PeekCompletion();
    LUNATIC_CHECK(writeCompletion);
    CheckCompletion(*writeCompletion, write);
    for (size_t iteration = 0; iteration < 12; ++iteration)
    {
        const auto repeated = backend.PeekCompletion();
        LUNATIC_CHECK(repeated);
        CheckSameCompletion(*repeated, *writeCompletion);
        LUNATIC_CHECK(backend.Outstanding(true) == 1);
        LUNATIC_CHECK(backend.Outstanding(false) == 1);
    }
    const auto poppedWrite = backend.PopCompletion();
    LUNATIC_CHECK(poppedWrite);
    CheckSameCompletion(*poppedWrite, *writeCompletion);

    const auto readCompletion = backend.PeekCompletion();
    LUNATIC_CHECK(readCompletion);
    CheckCompletion(*readCompletion, read);
    LUNATIC_CHECK(readCompletion->data == readData);
    for (size_t iteration = 0; iteration < 12; ++iteration)
    {
        const auto repeated = backend.PeekCompletion();
        LUNATIC_CHECK(repeated);
        CheckSameCompletion(*repeated, *readCompletion);
    }
    const auto poppedRead = backend.PopCompletion();
    LUNATIC_CHECK(poppedRead);
    CheckSameCompletion(*poppedRead, *readCompletion);
    LUNATIC_CHECK(!backend.PeekCompletion());
    LUNATIC_CHECK(!backend.PopCompletion());
    LUNATIC_CHECK(!backend.HasInflight());
}

LUNATIC_TEST(Stage7AXIEngine128BitLineReadWrite)
{
    CheckLineReadWrite(128, 7);
}

LUNATIC_TEST(Stage7AXIEngine256BitLineReadWrite)
{
    CheckLineReadWrite(256, 39);
}

LUNATIC_TEST(Stage7AXIEngine512BitLineReadWrite)
{
    CheckLineReadWrite(512, 71);
}

LUNATIC_TEST(Stage7AXIEngineSparseWSTRBAndWLAST)
{
    AXIBackend backend(EngineOptions(256));
    AXIModel model(32);
    const uint64_t address = 0x83000;
    const LineData initial = Pattern(19);
    const LineData update = Pattern(121);
    model.SetLine(address, initial);
    auto write = Operation(3, address, true, 0b10, update);
    for (size_t index = 0; index < LINE_SIZE; ++index)
        write.enables[index] = (index == 32 || index == 33 || index == 47 || index == 63) ? 1 : 0;
    model.Stall(AXIChannel::AW, 2);
    model.Stall(AXIChannel::W, 1);
    model.Stall(AXIChannel::B, 2);

    LUNATIC_CHECK(backend.Submit(write));
    RunModel(backend, model, [&] { return static_cast<bool>(backend.PeekCompletion()); });
    LUNATIC_CHECK(model.CompletedWrites().size() == 1);
    const auto& completed = model.CompletedWrites().front();
    CheckAddress(completed.address, write, 256);
    LUNATIC_CHECK(completed.address.address == address + 32);
    LUNATIC_CHECK(completed.address.len == 0);
    LUNATIC_CHECK(completed.beats.size() == 1);
    LUNATIC_CHECK(completed.beats.front().last);
    for (size_t index = 0; index < 32; ++index)
    {
        LUNATIC_CHECK(completed.beats.front().data[index] == update[32 + index]);
        LUNATIC_CHECK(completed.beats.front().strobes[index] == write.enables[32 + index]);
    }

    LineData expected = initial;
    for (size_t index = 0; index < LINE_SIZE; ++index)
        if (write.enables[index])
            expected[index] = update[index];
    LUNATIC_CHECK(model.ReadLine(address) == expected);
    const auto completion = backend.PopCompletion();
    LUNATIC_CHECK(completion);
    CheckCompletion(*completion, write);
    LUNATIC_CHECK(!backend.HasInflight());
    LUNATIC_CHECK(!backend.PopError());
    LUNATIC_CHECK(model.Errors().empty());
}

LUNATIC_TEST(Stage7AXIEngineAWBeforeW)
{
    AXIBackend backend(EngineOptions(256));
    const auto write = Operation(4, 0x84000, true, FullBeatMask(256), Pattern(44));
    LUNATIC_CHECK(backend.Submit(write));
    LUNATIC_CHECK(!backend.PeekW());
    LUNATIC_CHECK(!backend.PopW());
    const auto aw = backend.PopAW();
    LUNATIC_CHECK(aw);
    const auto first = backend.PeekW();
    LUNATIC_CHECK(first && !first->last);
    LUNATIC_CHECK(backend.PopW() == first);
    const auto last = backend.PeekW();
    LUNATIC_CHECK(last && last->last);
    LUNATIC_CHECK(backend.PopW() == last);
    LUNATIC_CHECK(!backend.PeekW());
    LUNATIC_CHECK(backend.Outstanding(true) == 1);
    LUNATIC_CHECK(backend.HasInflight());
}

LUNATIC_TEST(Stage7AXIEngineResponseBackpressure)
{
    Options options = EngineOptions(512);
    options.inputQueueLimit = 1;
    AXIBackend backend(options);
    const auto first = Operation(10, 0x85000, true, 1, Pattern(2));
    const auto second = Operation(11, 0x85100, true, 1, Pattern(3));
    LUNATIC_CHECK(backend.Submit(first));
    LUNATIC_CHECK(backend.Submit(second));
    const auto firstAW = backend.PopAW();
    const auto firstW = backend.PopW();
    const auto secondAW = backend.PopAW();
    const auto secondW = backend.PopW();
    LUNATIC_CHECK(firstAW && firstW && firstW->last);
    LUNATIC_CHECK(secondAW && secondW && secondW->last);
    LUNATIC_CHECK(backend.PushB({ firstAW->id, AXIResp::OKAY }));
    LUNATIC_CHECK(!backend.PushB({ secondAW->id, AXIResp::OKAY }));
    LUNATIC_CHECK(!backend.PeekCompletion());
    backend.Tick();
    LUNATIC_CHECK(backend.PeekCompletion());
    CheckCompletion(*backend.PeekCompletion(), first);
    LUNATIC_CHECK(backend.PushB({ secondAW->id, AXIResp::OKAY }));
    const auto firstCompletion = backend.PopCompletion();
    LUNATIC_CHECK(firstCompletion);
    CheckCompletion(*firstCompletion, first);
    backend.Tick();
    const auto secondCompletion = backend.PopCompletion();
    LUNATIC_CHECK(secondCompletion);
    CheckCompletion(*secondCompletion, second);
    LUNATIC_CHECK(!backend.HasInflight());

    options = EngineOptions(256);
    options.inputQueueLimit = 1;
    AXIBackend readBackend(options);
    const auto read = Operation(12, 0x85200, false, FullBeatMask(256));
    const LineData data = Pattern(88);
    LUNATIC_CHECK(readBackend.Submit(read));
    const auto ar = readBackend.PopAR();
    LUNATIC_CHECK(ar);
    LUNATIC_CHECK(readBackend.PushR(ReadBeat(ar->id, data, 256, 0)));
    LUNATIC_CHECK(!readBackend.PushR(ReadBeat(ar->id, data, 256, 1)));
    readBackend.Tick();
    LUNATIC_CHECK(!readBackend.PeekCompletion());
    LUNATIC_CHECK(readBackend.PushR(ReadBeat(ar->id, data, 256, 1)));
    readBackend.Tick();
    const auto readCompletion = readBackend.PopCompletion();
    LUNATIC_CHECK(readCompletion);
    CheckCompletion(*readCompletion, read);
    LUNATIC_CHECK(readCompletion->data == data);
    LUNATIC_CHECK(!readBackend.HasInflight());
}

LUNATIC_TEST(Stage7AXIEngineBoundedIDsAndReuseAfterCompletionPop)
{
    Options options = EngineOptions(512);
    options.axiIdWidth = 1;
    options.axiOutstandingLimit = 4;
    AXIBackend backend(options);
    const auto first = Operation(20, 0x86000, true, 1, Pattern(9));
    const auto second = Operation(21, 0x86100, true, 1, Pattern(10));
    const auto third = Operation(22, 0x86200, true, 1, Pattern(11));
    LUNATIC_CHECK(backend.Submit(first));
    LUNATIC_CHECK(backend.Submit(second));
    LUNATIC_CHECK(!backend.Submit(third));
    const auto firstAW = backend.PopAW();
    const auto secondAW = backend.PopAW();
    LUNATIC_CHECK(firstAW && secondAW);
    LUNATIC_CHECK(firstAW->id < 2 && secondAW->id < 2);
    LUNATIC_CHECK(firstAW->id != secondAW->id);
    LUNATIC_CHECK(backend.PopW()->last);
    LUNATIC_CHECK(backend.PopW()->last);
    LUNATIC_CHECK(backend.PushB({ firstAW->id, AXIResp::OKAY }));
    backend.Tick();
    LUNATIC_CHECK(backend.PeekCompletion());
    LUNATIC_CHECK(backend.Outstanding(true) == 2);
    LUNATIC_CHECK(!backend.Submit(third));
    const auto completion = backend.PopCompletion();
    LUNATIC_CHECK(completion);
    CheckCompletion(*completion, first);
    LUNATIC_CHECK(backend.Submit(third));
    const auto thirdAW = backend.PopAW();
    LUNATIC_CHECK(thirdAW && thirdAW->id == firstAW->id);
    LUNATIC_CHECK(backend.PopW()->last);
    LUNATIC_CHECK(backend.PushB({ secondAW->id, AXIResp::OKAY }));
    LUNATIC_CHECK(backend.PushB({ thirdAW->id, AXIResp::OKAY }));
    backend.Tick();
    const auto secondCompletion = backend.PopCompletion();
    const auto thirdCompletion = backend.PopCompletion();
    LUNATIC_CHECK(secondCompletion && thirdCompletion);
    CheckCompletion(*secondCompletion, second);
    CheckCompletion(*thirdCompletion, third);
    LUNATIC_CHECK(!backend.HasInflight());
}

LUNATIC_TEST(Stage7AXIEngineLegalOutOfOrderResponses)
{
    AXIBackend backend(EngineOptions(256));
    std::array<AXIOperation, 3> reads{
        Operation(30, 0x87000, false, FullBeatMask(256)),
        Operation(31, 0x87100, false, FullBeatMask(256)),
        Operation(32, 0x87200, false, FullBeatMask(256))
    };
    std::array<AXIAddress, 3> addresses{};
    std::array<LineData, 3> data{ Pattern(15), Pattern(47), Pattern(79) };
    for (size_t index = 0; index < reads.size(); ++index)
    {
        LUNATIC_CHECK(backend.Submit(reads[index]));
        const auto address = backend.PopAR();
        LUNATIC_CHECK(address);
        addresses[index] = *address;
        for (size_t earlier = 0; earlier < index; ++earlier)
            LUNATIC_CHECK(address->id != addresses[earlier].id);
    }
    LUNATIC_CHECK(backend.Outstanding(false) == 3);
    constexpr std::array<size_t, 3> order{ 2, 0, 1 };
    for (const size_t index : order)
        for (size_t beat = 0; beat < 2; ++beat)
            LUNATIC_CHECK(backend.PushR(ReadBeat(addresses[index].id, data[index], 256, beat)));
    backend.Tick();
    LUNATIC_CHECK(backend.Outstanding(false) == 3);
    for (const size_t index : order)
    {
        const auto completion = backend.PopCompletion();
        LUNATIC_CHECK(completion);
        CheckCompletion(*completion, reads[index]);
        LUNATIC_CHECK(completion->data == data[index]);
    }
    LUNATIC_CHECK(!backend.HasInflight());
    LUNATIC_CHECK(!backend.PopError());

    const auto first = Operation(33, 0x87300, true, FullBeatMask(256), Pattern(101));
    const auto second = Operation(34, 0x87400, true, FullBeatMask(256), Pattern(103));
    LUNATIC_CHECK(backend.Submit(first));
    LUNATIC_CHECK(backend.Submit(second));
    const auto firstAW = backend.PopAW();
    const auto secondAW = backend.PopAW();
    LUNATIC_CHECK(firstAW && secondAW);
    LUNATIC_CHECK(backend.PopW());
    LUNATIC_CHECK(backend.PopW());
    LUNATIC_CHECK(backend.PopW());
    LUNATIC_CHECK(backend.PopW());
    LUNATIC_CHECK(backend.PushB({ secondAW->id, AXIResp::OKAY }));
    LUNATIC_CHECK(backend.PushB({ firstAW->id, AXIResp::OKAY }));
    backend.Tick();
    const auto secondCompletion = backend.PopCompletion();
    const auto firstCompletion = backend.PopCompletion();
    LUNATIC_CHECK(secondCompletion && firstCompletion);
    CheckCompletion(*secondCompletion, second);
    CheckCompletion(*firstCompletion, first);
    LUNATIC_CHECK(!backend.HasInflight());
}

LUNATIC_TEST(Stage7AXIEngineResetAndDrain)
{
    AXIBackend backend(EngineOptions(512));
    const auto write = Operation(40, 0x88000, true, 1, Pattern(5));
    const auto read = Operation(41, 0x88100, false, 1);
    LUNATIC_CHECK(backend.Submit(write));
    LUNATIC_CHECK(backend.Submit(read));
    const auto aw = backend.PopAW();
    const auto ar = backend.PopAR();
    LUNATIC_CHECK(aw && ar && backend.PopW());
    LUNATIC_CHECK(backend.PushB({ aw->id, AXIResp::OKAY }));
    const LineData data = Pattern(111);
    LUNATIC_CHECK(backend.PushR(ReadBeat(ar->id, data, 512, 0)));
    backend.Tick();
    LUNATIC_CHECK(backend.HasInflight());
    LUNATIC_CHECK(backend.PopCompletion());
    LUNATIC_CHECK(backend.HasInflight());
    LUNATIC_CHECK(backend.PopCompletion());
    LUNATIC_CHECK(!backend.HasInflight());

    LUNATIC_CHECK(backend.Submit(write));
    LUNATIC_CHECK(backend.Submit(read));
    const auto resetAW = backend.PopAW();
    LUNATIC_CHECK(resetAW && backend.PopW());
    LUNATIC_CHECK(backend.PushB({ resetAW->id, AXIResp::OKAY }));
    LUNATIC_CHECK(backend.PushR({ 15, {}, AXIResp::OKAY, true }));
    backend.Tick();
    LUNATIC_CHECK(backend.PeekCompletion());
    LUNATIC_CHECK(backend.HasInflight());
    backend.Reset();
    LUNATIC_CHECK(!backend.HasInflight());
    LUNATIC_CHECK(backend.Outstanding(true) == 0);
    LUNATIC_CHECK(backend.Outstanding(false) == 0);
    LUNATIC_CHECK(!backend.PeekAW());
    LUNATIC_CHECK(!backend.PeekW());
    LUNATIC_CHECK(!backend.PeekAR());
    LUNATIC_CHECK(!backend.PeekCompletion());
    LUNATIC_CHECK(!backend.PopAW());
    LUNATIC_CHECK(!backend.PopW());
    LUNATIC_CHECK(!backend.PopAR());
    LUNATIC_CHECK(!backend.PopCompletion());
    LUNATIC_CHECK(!backend.PopError());
}

LUNATIC_TEST(Stage7AXIEngineSLVERRAndDECERRComplete)
{
    for (const AXIResp response : { AXIResp::SLVERR, AXIResp::DECERR })
    {
        CheckErrorCompletion(true, response);
        CheckErrorCompletion(false, response);
    }
}

LUNATIC_TEST(Stage7AXIEngineBadResponseIDs)
{
    AXIBackend backend(EngineOptions(512));
    const auto write = Operation(50, 0x89000, true, 1, Pattern(6));
    const auto read = Operation(51, 0x89100, false, 1);
    LUNATIC_CHECK(backend.Submit(write));
    LUNATIC_CHECK(backend.Submit(read));
    const auto aw = backend.PopAW();
    const auto ar = backend.PopAR();
    LUNATIC_CHECK(aw && ar && backend.PopW());
    LUNATIC_CHECK(backend.PushB({ aw->id + 1, AXIResp::OKAY }));
    LUNATIC_CHECK(backend.PushR({ ar->id + 1, {}, AXIResp::OKAY, true }));
    backend.Tick();
    LUNATIC_CHECK(!backend.PeekCompletion());
    LUNATIC_CHECK(backend.Outstanding(true) == 1);
    LUNATIC_CHECK(backend.Outstanding(false) == 1);
    const auto writeError = backend.PopError();
    const auto readError = backend.PopError();
    LUNATIC_CHECK(writeError && *writeError == "unexpected AXI write response ID");
    LUNATIC_CHECK(readError && *readError == "unexpected AXI read response ID");

    LUNATIC_CHECK(backend.PushB({ aw->id, AXIResp::OKAY }));
    LUNATIC_CHECK(backend.PushR(ReadBeat(ar->id, Pattern(17), 512, 0)));
    backend.Tick();
    const auto writeCompletion = backend.PopCompletion();
    const auto readCompletion = backend.PopCompletion();
    LUNATIC_CHECK(writeCompletion && readCompletion);
    CheckCompletion(*writeCompletion, write);
    CheckCompletion(*readCompletion, read);
    LUNATIC_CHECK(!backend.HasInflight());
    LUNATIC_CHECK(!backend.PopError());
}

LUNATIC_TEST(Stage7AXIEngineEarlyRLAST)
{
    AXIBackend backend(EngineOptions(256));
    const auto read = Operation(60, 0x8a000, false, FullBeatMask(256));
    const LineData data = Pattern(29);
    LUNATIC_CHECK(backend.Submit(read));
    const auto ar = backend.PopAR();
    LUNATIC_CHECK(ar);
    auto response = ReadBeat(ar->id, data, 256, 0);
    response.last = true;
    LUNATIC_CHECK(backend.PushR(response));
    backend.Tick();
    const auto completion = backend.PopCompletion();
    LUNATIC_CHECK(completion);
    CheckCompletion(*completion, read, AXIResp::OKAY, "early AXI RLAST");
    for (size_t index = 0; index < 32; ++index)
        LUNATIC_CHECK(completion->data[index] == data[index]);
    for (size_t index = 32; index < LINE_SIZE; ++index)
        LUNATIC_CHECK(completion->data[index] == 0);
    LUNATIC_CHECK(!backend.HasInflight());
}

LUNATIC_TEST(Stage7AXIEngineMissingRLAST)
{
    AXIBackend backend(EngineOptions(256));
    const auto read = Operation(61, 0x8b000, false, FullBeatMask(256));
    const LineData data = Pattern(61);
    LUNATIC_CHECK(backend.Submit(read));
    const auto ar = backend.PopAR();
    LUNATIC_CHECK(ar);
    auto first = ReadBeat(ar->id, data, 256, 0);
    auto second = ReadBeat(ar->id, data, 256, 1);
    first.last = false;
    second.last = false;
    LUNATIC_CHECK(backend.PushR(first));
    LUNATIC_CHECK(backend.PushR(second));
    backend.Tick();
    const auto completion = backend.PopCompletion();
    LUNATIC_CHECK(completion);
    CheckCompletion(*completion, read, AXIResp::OKAY, "missing AXI RLAST");
    LUNATIC_CHECK(completion->data == data);
    LUNATIC_CHECK(!backend.HasInflight());
}
