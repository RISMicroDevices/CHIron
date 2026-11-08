#include "common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Options ResponseOptions()
    {
        Options options;
        options.dataWidth = 256;
        options.inputQueueLimit = 16;
        options.outputQueueLimit = 16;
        options.axiOutstandingLimit = 8;
        options.axiIdWidth = 4;
        return options;
    }

    LineData ResponsePattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 19 + index / 5);
        return data;
    }

    AXIOperation ResponseOperation(TransactionHandle transaction, uint64_t address, bool write,
        const LineData& data = {})
    {
        AXIOperation operation;
        operation.transaction = transaction;
        operation.line = LineKey::FromAddress(address, true);
        operation.data = data;
        operation.beatMask = 0b11;
        operation.write = write;
        if (write)
            operation.enables.fill(1);
        return operation;
    }

    AXIReadData ResponseBeat(uint32_t id, const LineData& data, size_t beat,
        AXIResp response = AXIResp::OKAY)
    {
        AXIReadData result;
        result.id = id;
        result.resp = response;
        result.last = beat == 1;
        for (size_t index = 0; index < 32; ++index)
            result.data[index] = data[beat * 32 + index];
        return result;
    }

    void CheckResponseCompletion(const std::optional<AXICompletion>& completion,
        const AXIOperation& operation, AXIResp response = AXIResp::OKAY,
        const char* protocolError = "")
    {
        LUNATIC_CHECK(completion);
        LUNATIC_CHECK(completion->transaction == operation.transaction);
        LUNATIC_CHECK(completion->line == operation.line);
        LUNATIC_CHECK(completion->beatMask == operation.beatMask);
        LUNATIC_CHECK(completion->response == response);
        LUNATIC_CHECK(completion->write == operation.write);
        LUNATIC_CHECK(completion->protocolError == protocolError);
    }

    void CheckWriteBeat(const std::optional<AXIWriteData>& beat, const LineData& data,
        size_t index)
    {
        LUNATIC_CHECK(beat);
        LUNATIC_CHECK(beat->last == (index == 1));
        for (size_t byte = 0; byte < 32; ++byte)
        {
            LUNATIC_CHECK(beat->data[byte] == data[index * 32 + byte]);
            LUNATIC_CHECK(beat->strobes[byte]);
        }
    }

}

LUNATIC_TEST(Stage9AXIInterleavedReadBursts)
{
    AXIBackend backend(ResponseOptions());
    const auto first = ResponseOperation(70, 0x91000, false);
    const auto second = ResponseOperation(71, 0x92000, false);
    const LineData firstData = ResponsePattern(13);
    const LineData secondData = ResponsePattern(139);
    LUNATIC_CHECK(backend.Submit(first));
    LUNATIC_CHECK(backend.Submit(second));
    const auto firstAR = backend.PopAR();
    const auto secondAR = backend.PopAR();
    LUNATIC_CHECK(firstAR && secondAR && firstAR->id != secondAR->id);

    LUNATIC_CHECK(backend.PushR(ResponseBeat(firstAR->id, firstData, 0)));
    LUNATIC_CHECK(backend.PushR(ResponseBeat(secondAR->id, secondData, 0)));
    LUNATIC_CHECK(backend.PushR(ResponseBeat(firstAR->id, firstData, 1)));
    LUNATIC_CHECK(backend.PushR(ResponseBeat(secondAR->id, secondData, 1)));
    LUNATIC_CHECK(!backend.PeekCompletion());
    backend.Tick();

    const auto firstCompletion = backend.PopCompletion();
    CheckResponseCompletion(firstCompletion, first);
    LUNATIC_CHECK(firstCompletion->data == firstData);
    const auto secondCompletion = backend.PopCompletion();
    CheckResponseCompletion(secondCompletion, second);
    LUNATIC_CHECK(secondCompletion->data == secondData);
    LUNATIC_CHECK(!backend.HasInflight());
    LUNATIC_CHECK(!backend.PopError());
}

LUNATIC_TEST(Stage9AXISerialWriteBurstAssociation)
{
    AXIBackend backend(ResponseOptions());
    const LineData firstData = ResponsePattern(31);
    const LineData secondData = ResponsePattern(167);
    const auto first = ResponseOperation(72, 0x93000, true, firstData);
    const auto second = ResponseOperation(73, 0x94000, true, secondData);
    LUNATIC_CHECK(backend.Submit(first));
    LUNATIC_CHECK(backend.Submit(second));
    const auto firstAW = backend.PopAW();
    const auto secondAW = backend.PopAW();
    LUNATIC_CHECK(firstAW && secondAW && firstAW->id != secondAW->id);

    const auto held = backend.PeekW();
    CheckWriteBeat(held, firstData, 0);
    for (size_t iteration = 0; iteration < 8; ++iteration)
        LUNATIC_CHECK(backend.PeekW() == held);
    CheckWriteBeat(backend.PopW(), firstData, 0);
    CheckWriteBeat(backend.PopW(), firstData, 1);
    CheckWriteBeat(backend.PopW(), secondData, 0);
    CheckWriteBeat(backend.PopW(), secondData, 1);
    LUNATIC_CHECK(!backend.PeekW());

    LUNATIC_CHECK(backend.PushB({ secondAW->id, AXIResp::OKAY }));
    LUNATIC_CHECK(backend.PushB({ firstAW->id, AXIResp::OKAY }));
    backend.Tick();
    CheckResponseCompletion(backend.PopCompletion(), second);
    CheckResponseCompletion(backend.PopCompletion(), first);
    LUNATIC_CHECK(!backend.HasInflight());
    LUNATIC_CHECK(!backend.PopError());
}

LUNATIC_TEST(Stage9AXIMixedReadResponseMatrix)
{
    constexpr std::array<std::array<AXIResp, 2>, 5> responses{ {
        { AXIResp::OKAY, AXIResp::OKAY },
        { AXIResp::SLVERR, AXIResp::OKAY },
        { AXIResp::OKAY, AXIResp::SLVERR },
        { AXIResp::DECERR, AXIResp::OKAY },
        { AXIResp::OKAY, AXIResp::DECERR }
    } };
    for (size_t index = 0; index < responses.size(); ++index)
    {
        AXIBackend backend(ResponseOptions());
        const auto read = ResponseOperation(80 + index, 0x95000 + index * LINE_SIZE, false);
        const LineData data = ResponsePattern(uint8_t(47 + index * 23));
        LUNATIC_CHECK(backend.Submit(read));
        const auto ar = backend.PopAR();
        LUNATIC_CHECK(ar);
        for (size_t beat = 0; beat < 2; ++beat)
            LUNATIC_CHECK(backend.PushR(ResponseBeat(ar->id, data, beat, responses[index][beat])));
        backend.Tick();
        const AXIResp expected = responses[index][0] == AXIResp::OKAY
            ? responses[index][1] : responses[index][0];
        const auto completion = backend.PopCompletion();
        CheckResponseCompletion(completion, read, expected);
        LUNATIC_CHECK(completion->data == data);
        LUNATIC_CHECK(!backend.HasInflight());
        LUNATIC_CHECK(!backend.PopError());
    }
}

LUNATIC_TEST(Stage9AXIEarlyAndExtraResponses)
{
    for (const bool acceptAddress : { false, true })
    {
        AXIBackend backend(ResponseOptions());
        const auto write = ResponseOperation(90, 0x97000, true, ResponsePattern(61));
        LUNATIC_CHECK(backend.Submit(write));
        const auto aw = acceptAddress ? backend.PopAW() : backend.PeekAW();
        LUNATIC_CHECK(aw);
        LUNATIC_CHECK(backend.PushB({ aw->id, AXIResp::OKAY }));
        backend.Tick();
        CheckResponseCompletion(backend.PeekCompletion(), write, AXIResp::OKAY,
            "AXI write response arrived before request or WLAST acceptance");
        if (!acceptAddress)
            LUNATIC_CHECK(backend.PopAW());
        CheckWriteBeat(backend.PopW(), write.data, 0);
        CheckWriteBeat(backend.PopW(), write.data, 1);
        CheckResponseCompletion(backend.PopCompletion(), write, AXIResp::OKAY,
            "AXI write response arrived before request or WLAST acceptance");
        LUNATIC_CHECK(!backend.PeekCompletion());
        LUNATIC_CHECK(!backend.HasInflight());
        LUNATIC_CHECK(!backend.PopError());
    }

    AXIBackend earlyReadBackend(ResponseOptions());
    const auto earlyRead = ResponseOperation(91, 0x98000, false);
    const LineData earlyData = ResponsePattern(79);
    LUNATIC_CHECK(earlyReadBackend.Submit(earlyRead));
    const auto earlyAR = earlyReadBackend.PeekAR();
    LUNATIC_CHECK(earlyAR);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(earlyReadBackend.PushR(ResponseBeat(earlyAR->id, earlyData, beat)));
    earlyReadBackend.Tick();
    const auto earlyCompletion = earlyReadBackend.PopCompletion();
    CheckResponseCompletion(earlyCompletion, earlyRead, AXIResp::OKAY,
        "AXI read data arrived before AR acceptance");
    LUNATIC_CHECK(earlyCompletion->data == earlyData);
    LUNATIC_CHECK(earlyReadBackend.PopAR());
    LUNATIC_CHECK(!earlyReadBackend.HasInflight());
    LUNATIC_CHECK(!earlyReadBackend.PopError());

    AXIBackend extraBackend(ResponseOptions());
    const auto extraRead = ResponseOperation(92, 0x99000, false);
    const LineData extraData = ResponsePattern(101);
    LUNATIC_CHECK(extraBackend.Submit(extraRead));
    const auto extraAR = extraBackend.PopAR();
    LUNATIC_CHECK(extraAR);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(extraBackend.PushR(ResponseBeat(extraAR->id, extraData, beat)));
    LUNATIC_CHECK(extraBackend.PushR(ResponseBeat(extraAR->id, extraData, 1)));
    extraBackend.Tick();
    const auto extraCompletion = extraBackend.PopCompletion();
    CheckResponseCompletion(extraCompletion, extraRead);
    LUNATIC_CHECK(extraCompletion->data == extraData);
    const auto error = extraBackend.PopError();
    LUNATIC_CHECK(error && *error == "unexpected AXI read response ID");
    LUNATIC_CHECK(!extraBackend.HasInflight());
}

LUNATIC_TEST(Stage9AXIExclusiveResponseNegativeControls)
{
    AXIBackend backend(ResponseOptions());
    const auto write = ResponseOperation(93, 0x9a000, true, ResponsePattern(113));
    const auto read = ResponseOperation(94, 0x9b000, false);
    const LineData readData = ResponsePattern(127);
    LUNATIC_CHECK(backend.Submit(write));
    LUNATIC_CHECK(backend.Submit(read));
    const auto aw = backend.PopAW();
    const auto ar = backend.PopAR();
    LUNATIC_CHECK(aw && ar);
    CheckWriteBeat(backend.PopW(), write.data, 0);
    CheckWriteBeat(backend.PopW(), write.data, 1);
    LUNATIC_CHECK(backend.PushB({ aw->id, AXIResp::EXOKAY }));
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(backend.PushR(ResponseBeat(ar->id, readData, beat, AXIResp::EXOKAY)));
    backend.Tick();

    CheckResponseCompletion(backend.PopCompletion(), write, AXIResp::OKAY,
        "AXI write returned EXOKAY to a non-exclusive request");
    const auto readCompletion = backend.PopCompletion();
    CheckResponseCompletion(readCompletion, read, AXIResp::OKAY,
        "AXI read returned EXOKAY to a non-exclusive request");
    LUNATIC_CHECK(readCompletion->data == readData);
    LUNATIC_CHECK(!backend.HasInflight());
    LUNATIC_CHECK(!backend.PopError());
}
