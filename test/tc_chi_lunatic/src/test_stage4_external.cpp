#include "oracle_driver.hpp"

#include <algorithm>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData ExternalData(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 11);
        return data;
    }

    ByteEnables ExternalEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    class BackendModel {
        using Base = Driver<config>;

        std::map<LineKey, LineData> lines;
        std::vector<uint16_t>       opcodes;

        void SendRead(Base& driver, const typename Base::REQ& request)
        {
            constexpr size_t bytes = config::dataWidth / 8;
            LUNATIC_CHECK(CHI::Eb::MemAttrs::ExtractEWA(request.MemAttr()) == 0);
            const LineKey line = LineKey::FromAddress(request.Addr(), request.NS());
            const LineData data = Read(line);
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                typename Base::DAT response{};
                response.SrcID() = 6;
                response.TgtID() = 2;
                response.TxnID() = request.TxnID();
                response.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
                response.Resp() = CHI::Eb::Resps::CompData_I;
                response.DataID() = beat * bytes / 16;
                response.CCID() = (request.Addr() >> 4) & 3;
                for (size_t index = 0; index < bytes; ++index)
                    response.Data()[index / 8] |= uint64_t(data[beat * bytes + index]) << ((index % 8) * 8);
                LUNATIC_CHECK(driver.node.PushRXDAT(response));
            }
            driver.Tick();
        }

        void HandleWrite(Base& driver, const typename Base::REQ& request)
        {
            constexpr size_t bytes = config::dataWidth / 8;
            constexpr uint64_t fullEnables = bytes == 64 ? ~uint64_t(0) : (uint64_t(1) << bytes) - 1;
            typename Base::RSP response{};
            response.SrcID() = 6;
            response.TgtID() = 2;
            response.TxnID() = request.TxnID();
            response.DBID() = 77;
            response.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
            LUNATIC_CHECK(driver.node.PushRXRSP(response));
            driver.Tick();

            const LineKey line = LineKey::FromAddress(request.Addr(), request.NS());
            LineData update = Read(line);
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
                LUNATIC_CHECK(data.TgtID() == 6);
                LUNATIC_CHECK(data.TxnID() == request.TxnID());
                LUNATIC_CHECK(data.DBID() == response.DBID());
                if (request.Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull)
                    LUNATIC_CHECK(uint64_t(data.BE()) == fullEnables);
                const size_t offset = size_t(data.DataID()) * 16;
                for (size_t index = 0; index < bytes; ++index)
                {
                    if ((uint64_t(data.BE()) >> index) & 1)
                        update[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
                }
            }
            lines[line] = update;
            response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
            LUNATIC_CHECK(driver.node.PushRXRSP(response));
            driver.Tick();
        }

        void HandleAtomic(Base& driver, const typename Base::REQ& request)
        {
            constexpr size_t bytes = config::dataWidth / 8;
            const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(request.Opcode());
            const size_t form = request.TxnID() % 3;
            const LineKey line = LineKey::FromAddress(request.Addr(), request.NS());
            const LineData oldLine = Read(line);
            Request atomicRequest{};
            atomicRequest.opcode = request.Opcode();
            atomicRequest.address = request.Addr();
            atomicRequest.size = request.Size();
            atomicRequest.endian = request.Endian();
            AtomicGeometry geometry;
            LUNATIC_CHECK(GetAtomicGeometry(atomicRequest, geometry));

            typename Base::RSP response{};
            response.SrcID() = 6;
            response.TgtID() = 2;
            response.TxnID() = request.TxnID();
            if (store && form == 1)
            {
                response.DBID() = 99;
                response.Resp() = CHI::Eb::Resps::Comp_I;
                response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
                LUNATIC_CHECK(driver.node.PushRXRSP(response));
                driver.Tick();
            }
            response.DBID() = 77;
            response.Opcode() = store && form == 2 ? CHI::Eb::Opcodes::RSP::CompDBIDResp
                : CHI::Eb::Opcodes::RSP::DBIDResp;
            LUNATIC_CHECK(driver.node.PushRXRSP(response));
            driver.Tick();

            auto sendResult = [&] {
                typename Base::DAT result{};
                result.SrcID() = 6;
                result.TgtID() = 2;
                result.HomeNID() = 2;
                result.TxnID() = request.TxnID();
                result.DBID() = 77;
                result.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
                result.Resp() = CHI::Eb::Resps::CompData_I;
                const size_t address = request.Addr() % LINE_SIZE;
                const size_t offset = address / bytes * bytes;
                result.DataID() = offset / 16;
                result.CCID() = (request.Addr() >> 4) & 3;
                for (size_t index = 0; index < geometry.operandBytes; ++index)
                {
                    result.BE() = uint64_t(result.BE()) | (uint64_t(1) << (address - offset + index));
                    result.Data()[(address - offset + index) / 8] |=
                        uint64_t(oldLine[address + index]) << (((address - offset + index) % 8) * 8);
                }
                LUNATIC_CHECK(driver.node.PushRXDAT(result));
                driver.Tick();
            };
            if (!store && request.TxnID() % 2 != 0)
                sendResult();

            Options options;
            options.dataWidth = config::dataWidth;
            const uint64_t expected = AtomicOperandBeatMask(atomicRequest, options);
            uint64_t received = 0;
            LineData operands{};
            while ((received & expected) != expected)
            {
                auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
                LUNATIC_CHECK(data.TgtID() == 6 && data.TxnID() == request.TxnID() && data.DBID() == 77);
                LUNATIC_CHECK(data.CCID() == ((request.Addr() >> 4) & 3));
                const size_t offset = size_t(data.DataID()) * 16;
                LUNATIC_CHECK(offset + bytes <= LINE_SIZE && offset % bytes == 0);
                const size_t beat = offset / bytes;
                const uint64_t bit = uint64_t(1) << beat;
                LUNATIC_CHECK((expected & bit) && !(received & bit));
                for (size_t index = 0; index < bytes; ++index)
                {
                    const size_t lineOffset = offset + index;
                    const bool enabled = lineOffset >= geometry.windowOffset &&
                        lineOffset < geometry.windowOffset + geometry.windowBytes;
                    LUNATIC_CHECK(bool((uint64_t(data.BE()) >> index) & 1) == enabled);
                    if (enabled)
                        operands[lineOffset] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
                    else
                        LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) == 0);
                }
                received |= bit;
            }
            const auto execution = ExecuteAtomic(atomicRequest, oldLine, operands);
            if (execution.write)
                lines[line] = execution.data;
            if (store && form == 0)
            {
                response.Resp() = CHI::Eb::Resps::Comp_I;
                response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
                LUNATIC_CHECK(driver.node.PushRXRSP(response));
                driver.Tick();
            }
            if (!store && request.TxnID() % 2 == 0)
                sendResult();
        }

    public:
        LineData Read(const LineKey& line) const
        {
            const auto it = lines.find(line);
            return it == lines.end() ? LineData{} : it->second;
        }

        void Write(const LineKey& line, const LineData& data) { lines[line] = data; }

        const std::vector<uint16_t>& Opcodes() const { return opcodes; }

        void Step(Base& driver)
        {
            if (!driver.node.PeekTXREQ())
                return;
            auto request = driver.node.PopTXREQ();
            LUNATIC_CHECK(request);
            opcodes.push_back(request->Opcode());
            if (IsAtomicRequest(request->Opcode()))
            {
                LUNATIC_CHECK(request->Order() == 0 && request->SnpAttr() == 0);
                LUNATIC_CHECK(request->SnoopMe() == 0 && request->DoDWT() == 0 && request->ExpCompAck() == 0);
                LUNATIC_CHECK(CHI::Eb::MemAttrs::ExtractEWA(request->MemAttr()) == 0);
                HandleAtomic(driver, *request);
                return;
            }
            if (request->Opcode() == CHI::Eb::Opcodes::REQ::ReadNoSnp)
            {
                SendRead(driver, *request);
                return;
            }
            if (request->Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull ||
                request->Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpPtl)
            {
                HandleWrite(driver, *request);
                return;
            }
            if (request->Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpZero)
            {
                lines[LineKey::FromAddress(request->Addr(), request->NS())] = LineData{};
                typename Base::RSP response{};
                response.SrcID() = 6;
                response.TgtID() = 2;
                response.TxnID() = request->TxnID();
                response.Opcode() = CHI::Eb::Opcodes::RSP::CompDBIDResp;
                LUNATIC_CHECK(driver.node.PushRXRSP(response));
                driver.Tick();
                return;
            }
            if (request->Opcode() == CHI::Eb::Opcodes::REQ::CleanSharedPersist ||
                request->Opcode() == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
            {
                const bool separated = request->Opcode() == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep;
                LUNATIC_CHECK(separated || request->PGroupID() == 0);
                typename Base::RSP response{};
                response.SrcID() = 6;
                response.TgtID() = 2;
                response.TxnID() = request->TxnID();
                response.Opcode() = separated ? CHI::Eb::Opcodes::RSP::CompPersist : CHI::Eb::Opcodes::RSP::Comp;
                if (separated)
                    response.PGroupID() = request->PGroupID();
                LUNATIC_CHECK(driver.node.PushRXRSP(response));
                driver.Tick();
                return;
            }
            if (request->Opcode() == CHI::Eb::Opcodes::REQ::CleanShared ||
                request->Opcode() == CHI::Eb::Opcodes::REQ::CleanInvalid ||
                request->Opcode() == CHI::Eb::Opcodes::REQ::MakeInvalid)
            {
                typename Base::RSP response{};
                response.SrcID() = 6;
                response.TgtID() = 2;
                response.TxnID() = request->TxnID();
                response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
                LUNATIC_CHECK(driver.node.PushRXRSP(response));
                driver.Tick();
                return;
            }
            throw std::runtime_error("unexpected downstream opcode");
        }

        void Run(Base& driver)
        {
            for (size_t cycle = 0; cycle < 1000; ++cycle)
            {
                Step(driver);
                if (driver.node.Drain())
                    return;
                driver.Tick();
            }
            throw std::runtime_error("external backend failed to drain");
        }
    };

    template<typename config>
    typename Driver<config>::DAT ExternalNextDAT(CheckedDriver<config>& driver, BackendModel<config>& backend)
    {
        for (size_t cycle = 0; cycle < 1000; ++cycle)
        {
            backend.Step(driver.driver);
            if (driver.driver.node.PeekTXDAT())
                return driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            driver.driver.Tick();
        }
        throw std::runtime_error("external data timed out");
    }

    template<typename config>
    LineData ExternalReadData(CheckedDriver<config>& driver, BackendModel<config>& backend, NodeId source,
                              TransactionId txnId, uint64_t address, uint16_t opcode)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        auto request = driver.Request(opcode, source, txnId, address, true);
        if (opcode == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
            opcode == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid)
        {
            request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
            request.SnpAttr() = 1;
        }
        driver.PushRequest(request);
        LineData result{};
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            auto data = ExternalNextDAT(driver, backend);
            dbid = data.DBID();
            const size_t offset = size_t(data.DataID()) * 16;
            for (size_t index = 0; index < bytes; ++index)
                result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        driver.CompAck(source, dbid);
        return result;
    }

    template<typename config>
    void ExternalWriteback(CheckedDriver<config>& driver, const LineKey& line, const LineData& data,
                           uint16_t opcode = CHI::Eb::Opcodes::REQ::WriteBackFull)
    {
        driver.PushRequest(driver.Request(opcode, 4, 1, line.address));
        auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 1, dbid.DBID(), data, ExternalEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
    }

    template<typename config>
    typename Driver<config>::RSP ExternalNextRSP(CheckedDriver<config>& driver, BackendModel<config>& backend,
                                                 uint16_t opcode)
    {
        for (size_t cycle = 0; cycle < 1000; ++cycle)
        {
            backend.Step(driver.driver);
            if (driver.driver.node.PeekTXRSP())
            {
                auto response = driver.NextRSP(opcode);
                return response;
            }
            driver.driver.Tick();
        }
        throw std::runtime_error("external response timed out");
    }

}

LUNATIC_TEST(Stage4ExternalDirtyCleanSharedWritesSubordinate)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x21000, false);
    const LineData initial = ExternalData(3);
    const LineData dirty = ExternalData(91);
    backend.Write(line, initial);
    ExternalWriteback(driver, line, dirty);
    LUNATIC_CHECK(ExternalReadData(driver, backend, 5, 2, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 6, 3, line.address));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
    driver.SnoopData(5, snoop.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_UC_PD);
    ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp);
    backend.Run(driver.driver);
    LUNATIC_CHECK(backend.Read(line) == dirty);
}

LUNATIC_TEST(Stage4ExternalDirtyVictimWritesSubordinate)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey a = LineKey::FromAddress(0x22000, false);
    const LineKey b = LineKey::FromAddress(0x22040, false);
    const LineData dataA = ExternalData(17);
    const LineData dataB = ExternalData(109);
    backend.Write(a, ExternalData(1));
    backend.Write(b, ExternalData(2));

    ExternalWriteback(driver, a, dataA);
    LUNATIC_CHECK(driver.ReadData(5, 4, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 5, b.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(6, 5, dbid.DBID(), dataB, ExternalEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    driver.SnoopData(5, victim.flit.TxnID(), dataA, CHI::Eb::Resps::SnpRespData_I_PD);
    backend.Run(driver.driver);
    LUNATIC_CHECK(backend.Read(a) == dataA);
    LUNATIC_CHECK(driver.driver.node.InspectHome(b).data == dataB);
}

LUNATIC_TEST(Stage4ExternalPartialWritebackUsesSubordinateBaseline)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::Exclusive;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x23000, false);
    const LineData data = ExternalData(29);
    backend.Write(line, ExternalData(4));
    ExternalWriteback(driver, line, data);
    LUNATIC_CHECK(ExternalReadData(driver, backend, 5, 6, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
    backend.Run(driver.driver);

    LineData partial = ExternalData(151);
    ByteEnables enables{};
    for (size_t index = 7; index < 21; ++index)
        enables[index] = 1;
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 5, 7, line.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(5, 7, dbid.DBID(), partial, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    backend.Run(driver.driver);
    LineData merged = data;
    for (size_t index = 7; index < 21; ++index)
        merged[index] = partial[index];
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == merged);
}

LUNATIC_TEST(Stage4ExternalPartialSnoopUsesSubordinateBaseline)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::Exclusive;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x24000, false);
    const LineData data = ExternalData(37);
    backend.Write(line, ExternalData(5));
    ExternalWriteback(driver, line, data);
    LUNATIC_CHECK(ExternalReadData(driver, backend, 5, 8, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
    backend.Run(driver.driver);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 9, line.address, true));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true);
    auto first = ExternalNextDAT(driver, backend);
    auto second = ExternalNextDAT(driver, backend);
    LineData actual{};
    for (size_t index = 0; index < 32; ++index)
    {
        actual[index] = uint8_t(first.Data()[index / 8] >> ((index % 8) * 8));
        actual[32 + index] = uint8_t(second.Data()[index / 8] >> ((index % 8) * 8));
    }
    driver.CompAck(6, first.DBID());
    backend.Run(driver.driver);
    LUNATIC_CHECK(actual == data);
}

LUNATIC_TEST(Stage4ExternalPartialVictimBaselineAndMaskIsolation)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey a = LineKey::FromAddress(0x2a000, false);
    const LineKey b = LineKey::FromAddress(0x2a040, false);
    LineData dataA{};
    ByteEnables enables{};
    for (size_t index = 0; index < dataA.size(); ++index)
    {
        dataA[index] = uint8_t(41 + index * 3);
        if (index % 32 < 16)
            enables[index] = 1;
    }
    const LineData dataB = ExternalData(131);
    LineData expected = dataA;
    for (size_t index = 0; index < expected.size(); ++index)
        if (index % 32 >= 16)
            expected[index] = 0;

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 4, 10, a.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(4, 10, dbid.DBID(), dataA, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    LUNATIC_CHECK(ExternalReadData(driver, backend, 5, 11, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
    backend.Run(driver.driver);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 12, b.address));
    dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(6, 12, dbid.DBID(), dataB, ExternalEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    driver.SnoopData(5, victim.flit.TxnID(), dataA, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true);
    backend.Run(driver.driver);

    LUNATIC_CHECK(backend.Read(a) == expected);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(b, 6) == CacheState::I);
    LUNATIC_CHECK(driver.ReadData(7, 13, b.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataB);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(b, 7) == CacheState::UD);
    LUNATIC_CHECK(driver.driver.node.InspectHome(b).valid);
}

LUNATIC_TEST(Stage5ExternalReadVariants)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x32000, false);
    const LineData expected = ExternalData(67);
    backend.Write(line, expected);

    struct Variant {
        uint16_t   opcode;
        CacheState final;
    };
    const Variant variants[] = {
        { CHI::Eb::Opcodes::REQ::ReadClean, CacheState::SC },
        { CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, CacheState::SC },
        { CHI::Eb::Opcodes::REQ::ReadPreferUnique, CacheState::SC },
        { CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid, CacheState::I },
        { CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid, CacheState::I },
        { CHI::Eb::Opcodes::REQ::MakeReadUnique, CacheState::UC }
    };

    TransactionId txnId = 20;
    for (const Variant& variant : variants)
    {
        LUNATIC_CHECK(ExternalReadData(driver, backend, 5, txnId++, line.address, variant.opcode) == expected);
        backend.Run(driver.driver);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == variant.final);
    }
}

namespace {

    template<typename config>
    void RunExternalWriteUnique()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        CheckedDriver<config> driver(options);
        BackendModel<config> backend;
        const LineKey line = LineKey::FromAddress(0x43000, false);
        const LineData expected = ExternalData(103);
        backend.Write(line, ExternalData(7));

        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, 1, line.address);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        driver.PushRequest(request);
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        driver.WriteData(4, 1, dbid, expected, ExternalEnables(), CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
        backend.Run(driver.driver);
        LUNATIC_CHECK(backend.Read(line) == expected);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
    }

}

LUNATIC_TEST(Stage5ExternalWriteUnique128)
{
    RunExternalWriteUnique<Config128>();
}

LUNATIC_TEST(Stage5ExternalWriteUnique)
{
    RunExternalWriteUnique<Config256>();
}

LUNATIC_TEST(Stage5ExternalWriteUnique512)
{
    RunExternalWriteUnique<Config512>();
}

LUNATIC_TEST(Stage5ExternalWriteUniquePartial)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x43100, false);
    const LineData initial = ExternalData(11);
    const LineData update = ExternalData(171);
    backend.Write(line, initial);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniquePtl, 4, 1, line.address);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    driver.PushRequest(request);
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    ByteEnables enables{};
    for (size_t index = 9; index < 43; ++index)
        enables[index] = 1;
    driver.WriteData(4, 1, dbid, update, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
    backend.Run(driver.driver);
    LineData expected = initial;
    for (size_t index = 9; index < 43; ++index)
        expected[index] = update[index];
    LUNATIC_CHECK(backend.Read(line) == expected);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
}

LUNATIC_TEST(Stage5ExternalWriteUniqueStash)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x43200, false);
    const LineData expected = ExternalData(141);
    backend.Write(line, ExternalData(9));

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, 4, 1, line.address);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    request.ReturnNID() = 5;
    request.StashNIDValid() = 1;
    request.StashLPIDValid() = 1;
    request.StashLPID() = 3;
    driver.PushRequest(request);
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash);
    LUNATIC_CHECK(snoop.target == 5 && snoop.flit.StashLPID() == 3);
    driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    driver.WriteData(4, 1, dbid, expected, ExternalEnables(), CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
    backend.Run(driver.driver);
    LUNATIC_CHECK(backend.Read(line) == expected);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
}

LUNATIC_TEST(Stage5ExternalWriteUniqueZero)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;

    for (TransactionHandle form = 0; form < 3; ++form)
    {
        const LineKey line = LineKey::FromAddress(0x43000 + form * LINE_SIZE, false);
        backend.Write(line, ExternalData(7));
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueZero, 4, form + 1, line.address);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        driver.PushRequest(request);
        if (form == 0)
        {
            LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::DBIDResp).TxnID() == form + 1);
            LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp).TxnID() == form + 1);
        }
        else if (form == 1)
        {
            LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp).TxnID() == form + 1);
            LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::DBIDResp).TxnID() == form + 1);
        }
        else
            LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::CompDBIDResp).TxnID() == form + 1);
        backend.Run(driver.driver);
        LUNATIC_CHECK(backend.Read(line) == LineData{});
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
    }
}

namespace {

    void RunExternalPersistenceFields(uint16_t opcode)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        Driver<Config256> driver(options);
        const bool separated = opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep;
        const uint8_t group = separated ? 0x68 : 0;
        auto request = driver.Request(opcode, 5, 50, 0x4c000);
        request.PGroupID() = group;
        request.Deep() = 1;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));

        auto downstream = driver.NextREQ(opcode);
        LUNATIC_CHECK(downstream.SrcID() == 2 && downstream.TgtID() == 6);
        LUNATIC_CHECK(downstream.PGroupID() == group && downstream.Deep() == 1);
        if (separated)
        {
            LUNATIC_CHECK(downstream.ReturnNID() == 5);
            LUNATIC_CHECK(downstream.ReturnTxnID() == 0);
        }
        LUNATIC_CHECK(!driver.node.PeekTXRSP());

        Driver<Config256>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = downstream.TxnID();
        response.Opcode() = separated ? CHI::Eb::Opcodes::RSP::CompPersist : CHI::Eb::Opcodes::RSP::Comp;
        if (separated)
            response.PGroupID() = group;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
        auto upstream = driver.NextRSP(response.Opcode());
        LUNATIC_CHECK(upstream.TxnID() == 50);
        if (separated)
            LUNATIC_CHECK(upstream.PGroupID() == group);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage5ExternalPersistenceFields)
{
    RunExternalPersistenceFields(CHI::Eb::Opcodes::REQ::CleanSharedPersist);
    RunExternalPersistenceFields(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
}

LUNATIC_TEST(Stage5ExternalPersistenceRejectsWrongGroup)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config256> driver(options);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 5, 51, 0x4c000);
    request.PGroupID() = 0x69;
    request.Deep() = 1;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    Driver<Config256>::RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = downstream.TxnID();
    response.Opcode() = CHI::Eb::Opcodes::RSP::CompPersist;
    response.PGroupID() = 0x6a;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    LUNATIC_CHECK(!driver.node.PeekTXRSP());
}

LUNATIC_TEST(Stage5ExternalDirtyPersistenceSpillsFirst)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x4c100, false);
    const LineData data = ExternalData(53);
    ExternalWriteback(driver, line, data);
    LUNATIC_CHECK(ExternalReadData(driver, backend, 6, 2, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 5, 3, line.address);
    request.PGroupID() = 0x6b;
    request.Deep() = 1;
    driver.PushRequest(request);
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
    driver.SnoopData(6, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I_PD);
    auto response = ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(response.TxnID() == 3 && response.PGroupID() == 0x6b);
    backend.Run(driver.driver);
    LUNATIC_CHECK(backend.Read(line) == data);
    LUNATIC_CHECK(backend.Opcodes().size() == 2);
    LUNATIC_CHECK(backend.Opcodes()[0] == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(backend.Opcodes()[1] == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
}

LUNATIC_TEST(Stage5ExternalCleanUniqueWritesBackWithoutForwarding)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    BackendModel<Config256> backend;
    const LineKey line = LineKey::FromAddress(0x4c200, false);
    const LineData data = ExternalData(79);
    ExternalWriteback(driver, line, data);
    LUNATIC_CHECK(ExternalReadData(driver, backend, 6, 2, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 5, 4, line.address, true));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.flit.RetToSrc() == 0);
    driver.SnoopData(6, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I_PD);
    auto comp = ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.TxnID() == 4 && comp.Resp() == CHI::Eb::Resps::Comp_UC);
    driver.CompAck(5, comp.DBID());
    backend.Run(driver.driver);
    LUNATIC_CHECK(backend.Read(line) == data);
    LUNATIC_CHECK(backend.Opcodes().size() == 1);
    LUNATIC_CHECK(backend.Opcodes()[0] == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UCE);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
}

namespace {

    void SetExternalInteger(LineData& data, size_t offset, size_t bytes, uint64_t value)
    {
        for (size_t index = 0; index < bytes; ++index)
            data[offset + index] = uint8_t(value >> (index * 8));
    }

    Request ExternalAtomicRequest(uint16_t opcode, uint8_t size, uint64_t address)
    {
        Request request{};
        request.opcode = opcode;
        request.size = size;
        request.address = address;
        return request;
    }

    template<typename config>
    typename Driver<config>::DAT ExternalAtomicPacket(const typename Driver<config>::REQ& request,
                                                       const LineData& operands, size_t dataId)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        const Request geometryRequest = ExternalAtomicRequest(request.Opcode(), request.Size(), request.Addr());
        AtomicGeometry geometry;
        LUNATIC_CHECK(GetAtomicGeometry(geometryRequest, geometry));
        const size_t offset = dataId * 16;
        LUNATIC_CHECK(offset + bytes <= LINE_SIZE && offset % bytes == 0);
        typename Driver<config>::DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = 0;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = dataId;
        data.CCID() = (request.Addr() >> 4) & 3;
        for (size_t index = 0; index < bytes; ++index)
        {
            const size_t lineOffset = offset + index;
            const bool enabled = lineOffset >= geometry.windowOffset &&
                lineOffset < geometry.windowOffset + geometry.windowBytes;
            if (enabled)
            {
                data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
                data.Data()[index / 8] |= uint64_t(operands[lineOffset]) << ((index % 8) * 8);
            }
        }
        return data;
    }

    template<typename config>
    void ExternalAtomicDrive(CheckedDriver<config>& driver, BackendModel<config>& backend,
                             const typename Driver<config>::REQ& request, const LineData& operands,
                             const LineData& oldData, std::optional<NodeId> peer = std::nullopt,
                             bool reverse = false)
    {
        const bool store = CHI::Eb::Opcodes::REQ::AtomicStore::Is(request.Opcode());
        driver.PushRequest(request);
        if (peer)
        {
            auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
            LUNATIC_CHECK(snoop.target == *peer && snoop.flit.RetToSrc() == 0);
            driver.SnoopData(*peer, snoop.flit.TxnID(), oldData, CHI::Eb::Resps::SnpRespData_I_PD);
        }
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const uint16_t firstOpcode = driver.driver.node.PeekTXRSP()->Opcode();
        bool compComplete = !store;
        DataBufferId dbid = 0;
        if (firstOpcode == CHI::Eb::Opcodes::RSP::Comp)
        {
            LUNATIC_CHECK(store);
            LUNATIC_CHECK(driver.NextRSP(firstOpcode).TxnID() == request.TxnID());
            compComplete = true;
            dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        }
        else if (firstOpcode == CHI::Eb::Opcodes::RSP::DBIDResp)
            dbid = driver.NextRSP(firstOpcode).DBID();
        else
        {
            LUNATIC_CHECK(firstOpcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
            dbid = driver.NextRSP(firstOpcode).DBID();
            compComplete = true;
        }

        constexpr size_t bytes = config::dataWidth / 8;
        Options options;
        options.dataWidth = config::dataWidth;
        const Request geometryRequest = ExternalAtomicRequest(request.Opcode(), request.Size(), request.Addr());
        const uint64_t mask = AtomicOperandBeatMask(geometryRequest, options);
        std::vector<size_t> beats;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            if (mask & (uint64_t(1) << beat))
                beats.push_back(beat);
        if (reverse)
            std::reverse(beats.begin(), beats.end());
        for (size_t beat : beats)
        {
            auto data = ExternalAtomicPacket<config>(request, operands, beat * bytes / 16);
            data.TxnID() = dbid;
            driver.Oracle(4).WriteData(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
        if (store && !compComplete)
            LUNATIC_CHECK(ExternalNextRSP(driver, backend, CHI::Eb::Opcodes::RSP::Comp).TxnID() == request.TxnID());

        if (!store)
        {
            auto result = ExternalNextDAT(driver, backend);
            AtomicGeometry geometry;
            LUNATIC_CHECK(GetAtomicGeometry(geometryRequest, geometry));
            const size_t address = request.Addr() % LINE_SIZE;
            const size_t offset = address / bytes * bytes;
            LUNATIC_CHECK(result.TgtID() == request.SrcID() && result.TxnID() == request.TxnID());
            LUNATIC_CHECK(result.DBID() == dbid && result.DataID() == offset / 16);
            LUNATIC_CHECK(result.CCID() == ((request.Addr() >> 4) & 3));
            for (size_t index = 0; index < bytes; ++index)
            {
                const size_t lineOffset = offset + index;
                const bool enabled = lineOffset >= address && lineOffset < address + geometry.operandBytes;
                LUNATIC_CHECK(bool((uint64_t(result.BE()) >> index) & 1) == enabled);
                if (enabled)
                    LUNATIC_CHECK(uint8_t(result.Data()[index / 8] >> ((index % 8) * 8)) == oldData[lineOffset]);
            }
        }
        backend.Run(driver.driver);
        driver.Quiesce();
    }

    template<typename config>
    void RunExternalAtomicMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        CheckedDriver<config> driver(options);
        BackendModel<config> backend;
        for (size_t index = 0; index < 18; ++index)
        {
            const bool compare = index == 17;
            const bool swap = index == 16;
            const bool load = !compare && !swap && index >= 8;
            const uint8_t operation = compare || swap ? 0 : index % 8;
            const uint16_t opcode = compare ? uint16_t(CHI::Eb::Opcodes::REQ::AtomicCompare)
                : swap ? uint16_t(CHI::Eb::Opcodes::REQ::AtomicSwap) : uint16_t((load ? 0x30 : 0x28) | operation);
            const uint8_t size = compare ? 4 : 2;
            const LineKey line = LineKey::FromAddress(0x51000 + index * LINE_SIZE, false);
            const uint64_t address = line.address + (compare ? 0x38 : 0x20);
            auto request = driver.Request(opcode, 4, index + 1, address);
            request.Size() = size;
            LineData initial{};
            LineData operands{};
            initial.fill(0x13);
            SetExternalInteger(initial, address % LINE_SIZE, compare ? 8 : 4, 5);
            SetExternalInteger(operands, address % LINE_SIZE, compare ? 8 : 4, compare ? 5 : 7);
            if (compare)
                SetExternalInteger(operands, (address ^ 8) % LINE_SIZE, 8, 9);
            backend.Write(line, initial);
            const Request executionRequest = ExternalAtomicRequest(opcode, size, address);
            const auto expected = ExecuteAtomic(executionRequest, initial, operands);
            LUNATIC_CHECK(expected.write);
            ExternalAtomicDrive(driver, backend, request, operands, initial);
            LUNATIC_CHECK(backend.Read(line) == expected.data);
            LUNATIC_CHECK(backend.Opcodes().back() == opcode);
        }
    }

    void RunExternalDirtyAtomic(bool mismatch)
    {
        Options options;
        options.policy = PolicyKind::NINE;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        CheckedDriver<Config256> driver(options);
        BackendModel<Config256> backend;
        const LineKey line = LineKey::FromAddress(mismatch ? 0x51100 : 0x51200, false);
        const LineData initial = ExternalData(37);
        ExternalWriteback(driver, line, initial);
        LUNATIC_CHECK(ExternalReadData(driver, backend, 6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == initial);
        const uint64_t address = line.address + 0x20;
        const uint16_t opcode = mismatch ? CHI::Eb::Opcodes::REQ::AtomicCompare : CHI::Eb::Opcodes::REQ::AtomicLoad::ADD;
        auto request = driver.Request(opcode, 4, 100, address);
        request.Size() = mismatch ? 4 : 2;
        LineData operands = initial;
        if (mismatch)
        {
            SetExternalInteger(operands, 0x20, 8, 7);
            SetExternalInteger(operands, 0x28, 8, 9);
            operands[0x20] ^= 1;
        }
        else
            SetExternalInteger(operands, 0x20, 4, 7);
        const Request executionRequest = ExternalAtomicRequest(opcode, request.Size(), address);
        const auto expected = ExecuteAtomic(executionRequest, initial, operands);
        ExternalAtomicDrive(driver, backend, request, operands, initial, 6);
        LUNATIC_CHECK(backend.Read(line) == expected.data);
        LUNATIC_CHECK(backend.Opcodes().back() == opcode);
        LUNATIC_CHECK(backend.Opcodes().size() >= 2);
        LUNATIC_CHECK(backend.Opcodes()[backend.Opcodes().size() - 2] == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    }

}

LUNATIC_TEST(Stage5ExternalAtomicMatrix128)
{
    RunExternalAtomicMatrix<Config128>(PolicyKind::NINE);
    RunExternalAtomicMatrix<Config128>(PolicyKind::Exclusive);
    RunExternalAtomicMatrix<Config128>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5ExternalAtomicMatrix256)
{
    RunExternalAtomicMatrix<Config256>(PolicyKind::NINE);
    RunExternalAtomicMatrix<Config256>(PolicyKind::Exclusive);
    RunExternalAtomicMatrix<Config256>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5ExternalAtomicMatrix512)
{
    RunExternalAtomicMatrix<Config512>(PolicyKind::NINE);
    RunExternalAtomicMatrix<Config512>(PolicyKind::Exclusive);
    RunExternalAtomicMatrix<Config512>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5ExternalAtomicDirtyInterventionAndMismatch)
{
    RunExternalDirtyAtomic(false);
    RunExternalDirtyAtomic(true);
}

LUNATIC_TEST(Stage5ExternalAtomicCompare128ReversedPackets)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config128> driver(options);
    BackendModel<Config128> backend;
    const LineKey line = LineKey::FromAddress(0x51300, false);
    const uint64_t address = line.address + 0x30;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicCompare, 4, 1, address);
    request.Size() = 5;
    LineData initial{};
    LineData operands{};
    for (size_t index = 0; index < 16; ++index)
    {
        initial[0x30 + index] = 5;
        operands[0x30 + index] = 5;
        operands[0x20 + index] = 9;
    }
    backend.Write(line, initial);
    ExternalAtomicDrive(driver, backend, request, operands, initial, std::nullopt, true);
    const Request executionRequest = ExternalAtomicRequest(request.Opcode(), request.Size(), address);
    LUNATIC_CHECK(backend.Read(line) == ExecuteAtomic(executionRequest, initial, operands).data);
}
