#include "cohestra_lunatic_mock.hpp"
#include "cohestra_lunatic_axi.hpp"
#include "common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace Cohestra;
using namespace LunaticTest;

namespace {

    constexpr uint64_t MEMORY_BASE = 0x80000000ULL;
    constexpr uint64_t MEMORY_END = MEMORY_BASE + 0x1000;
    constexpr size_t BEAT_BYTES = 32;

    ::CHI::Lunatic::Options MakeOptions()
    {
        ::CHI::Lunatic::Options options;
        options.backend = ::CHI::Lunatic::BackendKind::AXI;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        options.dataWidth = 256;
        options.tagLatency = 1;
        options.dataLatency = 1;
        options.memoryLatency = 1;
        return options;
    }

    LunaticModel::Configuration MakeConfiguration(const ::CHI::Lunatic::Options& options)
    {
        LunaticModel::Configuration configuration;
        configuration.homeNodeId = 0;
        configuration.options = options;
        configuration.rnPorts = { { 0, 1 } };
        return configuration;
    }

    ::CHI::Lunatic::LineData Pattern(uint8_t seed)
    {
        ::CHI::Lunatic::LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 29 + index / 8);
        return data;
    }

    Cohestra::CHI::REQFlit MakeRequest(uint16_t opcode, uint16_t txnId, uint64_t address)
    {
        Cohestra::CHI::REQFlit request{};
        request.SrcID() = 1;
        request.TgtID() = 0;
        request.TxnID() = txnId;
        request.Opcode() = opcode;
        request.Size() = 6;
        request.Addr() = address;
        request.AllowRetry() = 1;
        request.MemAttr() = ::CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    class StandaloneAXIFixture {
    public:
        FakeCHIToRNInterface transport;
        AXIMemory memory;
        LunaticModel model;
        LunaticAXIAdapter adapter;
        uint64_t time = 0;

        explicit StandaloneAXIFixture(::CHI::Lunatic::Options options = MakeOptions(),
            uint32_t readDepth = 0, uint32_t readLatency = 0)
            : memory(256, 0xA71u, MEMORY_BASE, MEMORY_END, readDepth, 0,
                  readLatency, 0, 1, false, false, false)
            , model(transport, MakeConfiguration(options))
            , adapter(model.GetNode(), memory)
        {
        }

        void Tick()
        {
            time += 2;
            memory.TickPreHandshake(time);
            adapter.Tick();
            model.Tick(time);
            memory.TickPostHandshake(time);
        }

        template<typename Predicate>
        void Until(Predicate&& predicate, size_t limit = 256)
        {
            for (size_t cycle = 0; cycle < limit; ++cycle)
            {
                if (predicate())
                    return;
                Tick();
            }

            std::string message = "standalone AXI condition timed out";
            for (const auto& error : model.GetIntegrationErrors())
                message += "; " + error;
            for (const auto& error : model.GetNode().GetErrors())
                message += "; " + error.reason;
            throw std::runtime_error(message);
        }

        Cohestra::CHI::RSPFlit NextRSP()
        {
            Until([&] { return !transport.ports[0].rxrsp.empty(); });
            const auto response = transport.TakeRSP(0);
            LUNATIC_CHECK(response);
            return *response;
        }

        Cohestra::CHI::DATFlit NextDAT()
        {
            Until([&] { return !transport.ports[0].rxdat.empty(); });
            const auto data = transport.TakeDAT(0);
            LUNATIC_CHECK(data);
            return *data;
        }

        void Preload(uint64_t address, const ::CHI::Lunatic::LineData& data)
        {
            for (size_t index = 0; index < data.size(); ++index)
                memory.Access(address + index) = data[index];
        }

        ::CHI::Lunatic::LineData ReadLine(uint64_t address) const
        {
            ::CHI::Lunatic::LineData data{};
            for (size_t index = 0; index < data.size(); ++index)
                data[index] = memory.Access(address + index);
            return data;
        }

        void Quiesce()
        {
            Until([&] { return model.IsDrained(); });
            LUNATIC_CHECK(model.Drain());
            LUNATIC_CHECK(model.IsDrained());
            LUNATIC_CHECK(!model.HasErrors());
            LUNATIC_CHECK(model.Validate());
            LUNATIC_CHECK(adapter.Validate());
            LUNATIC_CHECK(memory.GetProtocolDenialCount() == 0);
            LUNATIC_CHECK(!memory.HasB());
            LUNATIC_CHECK(!memory.HasR());
            LUNATIC_CHECK(!model.GetNode().PeekTXAW());
            LUNATIC_CHECK(!model.GetNode().PeekTXW());
            LUNATIC_CHECK(!model.GetNode().PeekTXAR());
        }
    };

    Cohestra::CHI::DATFlit MakeWriteData(::CHI::Lunatic::DataBufferId dataBufferId,
        size_t beat, const ::CHI::Lunatic::LineData& data,
        const ::CHI::Lunatic::ByteEnables& enables)
    {
        Cohestra::CHI::DATFlit flit{};
        flit.SrcID() = 1;
        flit.TgtID() = 0;
        flit.HomeNID() = 0;
        flit.TxnID() = dataBufferId;
        flit.Opcode() = ::CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        flit.DataID() = beat * 2;
        flit.CCID() = 0;
        for (size_t index = 0; index < BEAT_BYTES; ++index)
        {
            const size_t offset = beat * BEAT_BYTES + index;
            if (enables[offset])
            {
                flit.Data()[index / 8] |= uint64_t(data[offset]) << ((index % 8) * 8);
                flit.BE() = uint64_t(flit.BE()) | (uint64_t(1) << index);
            }
        }
        return flit;
    }

    void CopyBeat(const Cohestra::CHI::DATFlit& flit, size_t beat,
        ::CHI::Lunatic::LineData& destination)
    {
        for (size_t index = 0; index < BEAT_BYTES; ++index)
            destination[beat * BEAT_BYTES + index] =
                uint8_t(flit.Data()[index / 8] >> ((index % 8) * 8));
    }

}

LUNATIC_TEST(Stage9CohestraAXIPreloadedReadNoSnp)
{
    StandaloneAXIFixture fixture;
    constexpr uint64_t address = MEMORY_BASE + 0x100;
    constexpr uint16_t txnId = 0x21;
    const auto expected = Pattern(0x31);
    fixture.Preload(address, expected);
    fixture.transport.InjectREQ(0,
        MakeRequest(::CHI::Eb::Opcodes::REQ::ReadNoSnp, txnId, address));

    ::CHI::Lunatic::LineData actual{};
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = fixture.NextDAT();
        LUNATIC_CHECK(data.Opcode() == ::CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.SrcID() == 0);
        LUNATIC_CHECK(data.TgtID() == 1);
        LUNATIC_CHECK(data.TxnID() == txnId);
        LUNATIC_CHECK(data.RespErr() == 0);
        LUNATIC_CHECK(data.DataID() == beat * 2);
        CopyBeat(data, beat, actual);
    }
    LUNATIC_CHECK(actual == expected);
    fixture.Quiesce();

    const auto counters = fixture.model.GetNode().GetPerformanceCounters();
    LUNATIC_CHECK(counters.emittedTXAR == 1);
    LUNATIC_CHECK(counters.acceptedRXR == 2);
    LUNATIC_CHECK(counters.emittedTXAW == 0);
    LUNATIC_CHECK(counters.emittedTXW == 0);
    LUNATIC_CHECK(counters.acceptedRXB == 0);
    LUNATIC_CHECK(fixture.memory.GetBackpressureDenialCount() == 0);
}

LUNATIC_TEST(Stage9CohestraAXIPartialWriteNoSnpPtl)
{
    StandaloneAXIFixture fixture;
    constexpr uint64_t address = MEMORY_BASE + 0x200;
    constexpr uint16_t txnId = 0x32;
    const auto initial = Pattern(0x17);
    const auto update = Pattern(0x93);
    fixture.Preload(address, initial);

    ::CHI::Lunatic::ByteEnables enables{};
    for (size_t index = 3; index < 9; ++index)
        enables[index] = 1;
    enables[0] = 1;
    enables[17] = 1;
    enables[31] = 1;
    enables[32] = 1;
    enables[47] = 1;
    for (size_t index = 56; index < 63; ++index)
        enables[index] = 1;

    fixture.transport.InjectREQ(0,
        MakeRequest(::CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, txnId, address));
    const auto dbidResponse = fixture.NextRSP();
    LUNATIC_CHECK(dbidResponse.Opcode() == ::CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(dbidResponse.TxnID() == txnId);
    const ::CHI::Lunatic::DataBufferId dataBufferId = dbidResponse.DBID();

    for (size_t beat = 0; beat < 2; ++beat)
        fixture.transport.InjectDAT(0, MakeWriteData(dataBufferId, beat, update, enables));

    const auto comp = fixture.NextRSP();
    LUNATIC_CHECK(comp.Opcode() == ::CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.TxnID() == txnId);
    LUNATIC_CHECK(comp.RespErr() == 0);
    fixture.Quiesce();

    auto expected = initial;
    for (size_t index = 0; index < expected.size(); ++index)
        if (enables[index])
            expected[index] = update[index];
    LUNATIC_CHECK(fixture.ReadLine(address) == expected);

    const auto counters = fixture.model.GetNode().GetPerformanceCounters();
    LUNATIC_CHECK(counters.emittedTXAW == 1);
    LUNATIC_CHECK(counters.emittedTXW == 2);
    LUNATIC_CHECK(counters.acceptedRXB == 1);
    LUNATIC_CHECK(counters.emittedTXAR == 0);
    LUNATIC_CHECK(counters.acceptedRXR == 0);
    LUNATIC_CHECK(fixture.memory.GetBackpressureDenialCount() == 0);
}

LUNATIC_TEST(Stage9CohestraAXIBoundedResponseBackpressure)
{
    auto options = MakeOptions();
    options.inputQueueLimit = 1;
    options.outputQueueLimit = 1;
    options.axiOutstandingLimit = 2;
    StandaloneAXIFixture fixture(options, 1, 64);
    fixture.transport.outputLimit = 1;

    constexpr uint64_t addressA = MEMORY_BASE + 0x300;
    constexpr uint64_t addressB = MEMORY_BASE + 0x340;
    constexpr uint16_t txnA = 0x41;
    constexpr uint16_t txnB = 0x42;
    const auto expectedA = Pattern(0x25);
    const auto expectedB = Pattern(0xB7);
    fixture.Preload(addressA, expectedA);
    fixture.Preload(addressB, expectedB);
    fixture.transport.InjectREQ(0,
        MakeRequest(::CHI::Eb::Opcodes::REQ::ReadNoSnp, txnA, addressA));
    fixture.transport.InjectREQ(0,
        MakeRequest(::CHI::Eb::Opcodes::REQ::ReadNoSnp, txnB, addressB));

    fixture.Until([&] {
        return fixture.model.GetNode().GetPerformanceCounters().acceptedRXR == 4;
    });
    LUNATIC_CHECK(fixture.memory.GetBackpressureDenialCount() != 0);
    LUNATIC_CHECK(fixture.transport.ports[0].rxdat.size() == 1);
    const auto held = fixture.transport.ports[0].rxdat.front();

    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        fixture.Tick();
        LUNATIC_CHECK(fixture.transport.ports[0].rxdat.size() == 1);
        const auto& current = fixture.transport.ports[0].rxdat.front();
        LUNATIC_CHECK(current.TxnID() == held.TxnID());
        LUNATIC_CHECK(current.DataID() == held.DataID());
        for (size_t lane = 0; lane < 4; ++lane)
            LUNATIC_CHECK(current.Data()[lane] == held.Data()[lane]);
    }

    std::array<::CHI::Lunatic::LineData, 2> actual{};
    std::array<size_t, 2> beats{};
    for (size_t count = 0; count < 4; ++count)
    {
        const auto data = fixture.NextDAT();
        LUNATIC_CHECK(data.TxnID() == txnA || data.TxnID() == txnB);
        const size_t read = data.TxnID() == txnA ? 0 : 1;
        LUNATIC_CHECK(beats[read] < 2);
        LUNATIC_CHECK(data.DataID() == beats[read] * 2);
        CopyBeat(data, beats[read], actual[read]);
        ++beats[read];
    }
    LUNATIC_CHECK(beats[0] == 2 && beats[1] == 2);
    LUNATIC_CHECK(actual[0] == expectedA);
    LUNATIC_CHECK(actual[1] == expectedB);
    fixture.Quiesce();

    const auto counters = fixture.model.GetNode().GetPerformanceCounters();
    LUNATIC_CHECK(counters.acceptedRXREQ == 2);
    LUNATIC_CHECK(counters.emittedTXAR == 2);
    LUNATIC_CHECK(counters.acceptedRXR == 4);
    LUNATIC_CHECK(counters.emittedTXDAT == 4);
    LUNATIC_CHECK(counters.emittedTXAW == 0);
    LUNATIC_CHECK(counters.emittedTXW == 0);
    LUNATIC_CHECK(counters.acceptedRXB == 0);
    LUNATIC_CHECK(fixture.memory.GetBackpressureDenialCount() != 0);
}
