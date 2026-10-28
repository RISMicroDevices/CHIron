#include "oracle_driver.hpp"

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

class BackendModel {
    std::map<LineKey, LineData> lines;

    void SendRead(Driver<Config256>& driver, const Driver<Config256>::REQ& request)
    {
        LUNATIC_CHECK(CHI::Eb::MemAttrs::ExtractEWA(request.MemAttr()) == 0);
        const LineKey line = LineKey::FromAddress(request.Addr(), request.NS());
        const LineData data = Read(line);
        for (size_t beat = 0; beat < 2; ++beat)
        {
            Driver<Config256>::DAT response{};
            response.SrcID() = 6;
            response.TgtID() = 2;
            response.TxnID() = request.TxnID();
            response.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            response.Resp() = CHI::Eb::Resps::CompData_I;
            response.DataID() = beat * 2;
            response.CCID() = (request.Addr() >> 4) & 3;
            for (size_t index = 0; index < 32; ++index)
                response.Data()[index / 8] |= uint64_t(data[beat * 32 + index]) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(response));
        }
        driver.Tick();
    }

    void HandleWrite(Driver<Config256>& driver, const Driver<Config256>::REQ& request)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();

        const LineKey line = LineKey::FromAddress(request.Addr(), request.NS());
        LineData update = Read(line);
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(data.TgtID() == 6);
            const size_t offset = size_t(data.DataID()) * 16;
            for (size_t index = 0; index < 32; ++index)
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

public:
    LineData Read(const LineKey& line) const
    {
        const auto it = lines.find(line);
        return it == lines.end() ? LineData{} : it->second;
    }

    void Write(const LineKey& line, const LineData& data) { lines[line] = data; }

    void Step(Driver<Config256>& driver)
    {
        if (!driver.node.PeekTXREQ())
            return;
        auto request = driver.node.PopTXREQ();
        LUNATIC_CHECK(request);
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
            Driver<Config256>::RSP response{};
            response.SrcID() = 6;
            response.TgtID() = 2;
            response.TxnID() = request->TxnID();
            response.Opcode() = CHI::Eb::Opcodes::RSP::CompDBIDResp;
            LUNATIC_CHECK(driver.node.PushRXRSP(response));
            driver.Tick();
            return;
        }
        if (request->Opcode() == CHI::Eb::Opcodes::REQ::CleanShared ||
            request->Opcode() == CHI::Eb::Opcodes::REQ::CleanInvalid ||
            request->Opcode() == CHI::Eb::Opcodes::REQ::MakeInvalid)
        {
            Driver<Config256>::RSP response{};
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

    void Run(Driver<Config256>& driver)
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

Driver<Config256>::DAT ExternalNextDAT(CheckedDriver<Config256>& driver, BackendModel& backend)
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

LineData ExternalReadData(CheckedDriver<Config256>& driver, BackendModel& backend, NodeId source,
                          TransactionId txnId, uint64_t address, uint16_t opcode)
{
    driver.PushRequest(driver.Request(opcode, source, txnId, address, true));
    LineData result{};
    DatabaseId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        auto data = ExternalNextDAT(driver, backend);
        dbid = data.DBID();
        const size_t offset = size_t(data.DataID()) * 16;
        for (size_t index = 0; index < 32; ++index)
            result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    driver.CompAck(source, dbid);
    return result;
}

void ExternalWriteback(CheckedDriver<Config256>& driver, const LineKey& line, const LineData& data,
                       uint16_t opcode = CHI::Eb::Opcodes::REQ::WriteBackFull)
{
    driver.PushRequest(driver.Request(opcode, 4, 1, line.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.WriteData(4, 1, dbid.DBID(), data, ExternalEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
}

void ExternalReceiveUnique(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId)
{
    DatabaseId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_UC);
        dbid = data.DBID();
    }
    driver.CompAck(source, dbid);
}

Driver<Config256>::RSP ExternalNextRSP(CheckedDriver<Config256>& driver, BackendModel& backend, uint16_t opcode)
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
    BackendModel backend;
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
    BackendModel backend;
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
    BackendModel backend;
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
    BackendModel backend;
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
    BackendModel backend;
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
