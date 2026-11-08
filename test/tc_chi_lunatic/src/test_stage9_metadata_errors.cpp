#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config512>;
    using DAT        = TestDriver::DAT;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;

    enum class MetadataFault : uint8_t {
        None,
        Poison,
        Parity
    };

    struct PacketMetadata {
        LinePoison    poison{};
        LineDataCheck dataCheck{};
    };

    LineData MetadataPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    ByteEnables FullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    Options MetadataOptions(bool poison, bool dataCheck, BackendKind backend = BackendKind::CHI)
    {
        Options options;
        options.backend = backend;
        options.subordinateId = 6;
        options.poisonEnabled = poison;
        options.dataCheckEnabled = dataCheck;
        return options;
    }

    PacketMetadata MakeMetadata(const LineData& data, MetadataFault fault, size_t chunk, size_t byte)
    {
        PacketMetadata metadata;
        Options options;
        options.dataCheckEnabled = true;
        GenerateMetadata(metadata.poison, metadata.dataCheck, data, FullEnables(), options);
        if (fault == MetadataFault::Poison)
            metadata.poison.at(chunk) = 1;
        else if (fault == MetadataFault::Parity)
            metadata.dataCheck.at(byte) ^= 1;
        return metadata;
    }

    DAT MakePacket(NodeId source, TransactionId txnId, uint16_t opcode, const LineData& data,
                   const ByteEnables& enables, uint8_t resp, const PacketMetadata& metadata,
                   uint8_t respErr = CHI::Eb::RespErrs::OK)
    {
        DAT flit{};
        flit.SrcID() = source;
        flit.TgtID() = 2;
        flit.TxnID() = txnId;
        flit.Opcode() = opcode;
        flit.Resp() = resp;
        flit.RespErr() = respErr;
        uint64_t byteEnables = 0;
        uint64_t poison = 0;
        uint64_t dataCheck = 0;
        for (size_t index = 0; index < LINE_SIZE; ++index)
        {
            if (enables[index])
                flit.Data()[index / 8] |= uint64_t(data[index]) << ((index % 8) * 8);
            byteEnables |= uint64_t(bool(enables[index])) << index;
            dataCheck |= uint64_t(metadata.dataCheck[index] & 1) << index;
        }
        for (size_t lane = 0; lane < metadata.poison.size(); ++lane)
            poison |= uint64_t(metadata.poison[lane] & 1) << lane;
        flit.BE() = byteEnables;
        flit.Poison() = poison;
        flit.DataCheck() = dataCheck;
        return flit;
    }

    PacketMetadata PacketMetadataOf(const DAT& flit)
    {
        PacketMetadata metadata;
        for (size_t index = 0; index < LINE_SIZE; ++index)
            metadata.dataCheck[index] = (uint64_t(flit.DataCheck()) >> index) & 1;
        for (size_t lane = 0; lane < metadata.poison.size(); ++lane)
            metadata.poison[lane] = (uint64_t(flit.Poison()) >> lane) & 1;
        return metadata;
    }

    uint8_t ExpectedMetadataError(const Options& options, MetadataFault fault)
    {
        const bool unsupportedPoison = fault == MetadataFault::Poison && !options.poisonEnabled;
        const bool unsupportedParity = fault == MetadataFault::Parity && !options.dataCheckEnabled;
        return unsupportedPoison || unsupportedParity ? uint8_t(CHI::Eb::RespErrs::DERR)
                                                      : uint8_t(CHI::Eb::RespErrs::OK);
    }

    void SendBackendResponse(TestDriver& driver, const REQ& request, uint16_t opcode, DataBufferId dbid = 77)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = dbid;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    REQ NextBackendRequest(TestDriver& driver)
    {
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXREQ()); });
        const auto request = driver.node.PopTXREQ();
        LUNATIC_CHECK(request);
        return *request;
    }

    void CheckBackendWrite(bool poison, bool dataCheck, MetadataFault fault, size_t chunk,
                           bool partial = false, bool copyback = false, size_t faultByte = 0,
                           bool disabledFault = false)
    {
        Options options = MetadataOptions(poison, dataCheck);
        TestDriver driver(options);
        const uint64_t address = 0x80000 + chunk * LINE_SIZE;
        ByteEnables enables = FullEnables();
        if (partial)
        {
            enables.fill(0);
            enables[(chunk + (disabledFault ? 1 : 0)) * 8 + 3] = 1;
        }
        LineData data{};
        const LineData pattern = MetadataPattern(19 + chunk);
        for (size_t index = 0; index < LINE_SIZE; ++index)
            if (enables[index])
                data[index] = pattern[index];
        const size_t parityByte = faultByte ? faultByte : chunk * 8;
        const PacketMetadata metadata = MakeMetadata(data, fault, chunk, parityByte);
        bool faultEnabled = fault != MetadataFault::None;
        if (fault == MetadataFault::Poison)
        {
            faultEnabled = false;
            for (size_t index = chunk * 8; index < chunk * 8 + 8; ++index)
                faultEnabled |= enables[index] != 0;
        }
        else if (fault == MetadataFault::Parity)
            faultEnabled = enables[parityByte] != 0;
        const uint16_t requestOpcode = copyback
            ? (partial ? uint16_t(CHI::Eb::Opcodes::REQ::WriteBackPtl)
                       : uint16_t(CHI::Eb::Opcodes::REQ::WriteCleanFull))
            : (partial ? uint16_t(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl)
                       : uint16_t(CHI::Eb::Opcodes::REQ::WriteNoSnpFull));
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(requestOpcode, 4, 1, address)));
        const auto dbid = driver.NextRSP(copyback ? CHI::Eb::Opcodes::RSP::CompDBIDResp
                                                  : CHI::Eb::Opcodes::RSP::DBIDResp);
        auto write = MakePacket(4, dbid.DBID(),
            copyback ? CHI::Eb::Opcodes::DAT::CopyBackWrData : CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
            data, enables, copyback ? uint8_t(CHI::Eb::Resps::CopyBackWrData_UD_PD) : uint8_t(0), metadata);
        LUNATIC_CHECK(driver.node.PushRXDAT(write));
        driver.Tick();

        const REQ downstream = NextBackendRequest(driver);
        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::DBIDResp);
        const DAT backendData = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(backendData.RespErr() ==
            (faultEnabled ? ExpectedMetadataError(options, fault) : uint8_t(CHI::Eb::RespErrs::OK)));
        for (size_t index = 0; index < LINE_SIZE; ++index)
        {
            const uint8_t actual = uint8_t(backendData.Data()[index / 8] >> ((index % 8) * 8));
            LUNATIC_CHECK(actual == (enables[index] ? data[index] : 0));
            LUNATIC_CHECK(((uint64_t(backendData.BE()) >> index) & 1) == bool(enables[index]));
        }
        const PacketMetadata forwarded = PacketMetadataOf(backendData);
        LUNATIC_CHECK(forwarded.poison == (options.poisonEnabled ? metadata.poison : LinePoison{}));
        if (!options.dataCheckEnabled)
            LUNATIC_CHECK(forwarded.dataCheck == LineDataCheck{});
        else
            for (size_t index = 0; index < LINE_SIZE; ++index)
                if (enables[index])
                    LUNATIC_CHECK(forwarded.dataCheck[index] == metadata.dataCheck[index]);

        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::Comp);
        if (!copyback)
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckBackendRead(bool poison, bool dataCheck, MetadataFault fault,
                          uint8_t originalError = CHI::Eb::RespErrs::OK)
    {
        Options options = MetadataOptions(poison, dataCheck);
        TestDriver driver(options);
        const LineData data = MetadataPattern(73);
        const ByteEnables enables = FullEnables();
        const PacketMetadata metadata = MakeMetadata(data, fault, 5, 43);
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x81000)));
        const REQ downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        DAT read = MakePacket(6, downstream.TxnID(), CHI::Eb::Opcodes::DAT::CompData,
                              data, enables, CHI::Eb::Resps::CompData_I, metadata, originalError);
        read.HomeNID() = 2;
        read.BE() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(read));
        driver.Tick();
        const DAT response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        const uint8_t expected = originalError == CHI::Eb::RespErrs::OK
            ? ExpectedMetadataError(options, fault) : originalError;
        LUNATIC_CHECK(response.RespErr() == expected);
        for (size_t index = 0; index < LINE_SIZE; ++index)
            LUNATIC_CHECK(uint8_t(response.Data()[index / 8] >> ((index % 8) * 8)) == data[index]);
        if (expected == CHI::Eb::RespErrs::OK)
        {
            const PacketMetadata forwarded = PacketMetadataOf(response);
            LUNATIC_CHECK(forwarded.poison == (options.poisonEnabled ? metadata.poison : LinePoison{}));
            LUNATIC_CHECK(forwarded.dataCheck == (options.dataCheckEnabled ? metadata.dataCheck : LineDataCheck{}));
        }
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void SeedDirtyOwner(TestDriver& driver, uint64_t address, const LineData& data)
    {
        const ByteEnables enables = FullEnables();
        const PacketMetadata clean = MakeMetadata(data, MetadataFault::None, 0, 0);
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(6, response.DBID(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            data, enables, CHI::Eb::Resps::CopyBackWrData_UD_PD, clean)));
        driver.Tick();
        driver.Quiesce();

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 91, address, true)));
        const DAT initial = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(initial.RespErr() == CHI::Eb::RespErrs::OK);
        driver.CompAck(6, initial.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(LineKey::FromAddress(address, false), 6) == CacheState::UD);
    }

    void CheckAtomicMetadata(bool backendResult)
    {
        Options options = MetadataOptions(false, false);
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 3, 0x82000);
        request.Size() = 0;
        request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
        request.SnpAttr() = 0;
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        LineData operands{};
        operands[0] = 7;
        ByteEnables operandEnables{};
        operandEnables[0] = 1;
        const auto operandMetadata = MakeMetadata(operands, backendResult ? MetadataFault::None : MetadataFault::Poison, 0, 0);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(4, dbid, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
            operands, operandEnables, 0, operandMetadata)));
        driver.Tick();

        const REQ downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::AtomicSwap);
        SendBackendResponse(driver, downstream, CHI::Eb::Opcodes::RSP::DBIDResp);
        const DAT backendOperand = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(backendOperand.RespErr() == (backendResult ? CHI::Eb::RespErrs::OK : CHI::Eb::RespErrs::DERR));

        LineData original{};
        original[0] = 9;
        const auto resultMetadata = MakeMetadata(original, backendResult ? MetadataFault::Poison : MetadataFault::None, 0, 0);
        DAT result = MakePacket(6, downstream.TxnID(), CHI::Eb::Opcodes::DAT::CompData,
                                original, operandEnables, CHI::Eb::Resps::CompData_I, resultMetadata);
        result.HomeNID() = 2;
        result.DBID() = 77;
        result.BE() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(result));
        driver.Tick();
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::DERR);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9MetadataConversionOptionsAndEveryChunk)
{
    for (size_t profile = 0; profile < 4; ++profile)
    {
        const bool poison = profile & 1;
        const bool dataCheck = profile & 2;
        for (size_t chunk = 0; chunk < LINE_SIZE / 8; ++chunk)
        {
            CheckBackendWrite(poison, dataCheck, MetadataFault::None, chunk);
            CheckBackendWrite(poison, dataCheck, MetadataFault::Poison, chunk);
            CheckBackendWrite(poison, dataCheck, MetadataFault::Parity, chunk);
        }
    }
}

LUNATIC_TEST(Stage9MetadataConversionPartialByteEnables)
{
    CheckBackendWrite(false, true, MetadataFault::Poison, 2, true);
    CheckBackendWrite(false, true, MetadataFault::Poison, 3, true, false, 0, true);
    CheckBackendWrite(true, false, MetadataFault::Parity, 2, true, false, 19);
    CheckBackendWrite(true, false, MetadataFault::Parity, 2, true, false, 20);
    CheckBackendWrite(false, false, MetadataFault::None, 7, true);
}

LUNATIC_TEST(Stage9MetadataConversionCopyback)
{
    CheckBackendWrite(false, false, MetadataFault::Poison, 7, false, true);
    CheckBackendWrite(true, true, MetadataFault::Parity, 6, false, true);
}

LUNATIC_TEST(Stage9MetadataConversionBackendRead)
{
    for (size_t profile = 0; profile < 4; ++profile)
    {
        const bool poison = profile & 1;
        const bool dataCheck = profile & 2;
        CheckBackendRead(poison, dataCheck, MetadataFault::None);
        CheckBackendRead(poison, dataCheck, MetadataFault::Poison);
        CheckBackendRead(poison, dataCheck, MetadataFault::Parity);
    }
}

LUNATIC_TEST(Stage9MetadataConversionExistingRespErrControls)
{
    CheckBackendRead(false, false, MetadataFault::Poison, CHI::Eb::RespErrs::DERR);
    CheckBackendRead(false, false, MetadataFault::Parity, CHI::Eb::RespErrs::NDERR);
}

LUNATIC_TEST(Stage9MetadataConversionSnoopData)
{
    Options options = MetadataOptions(false, false, BackendKind::Internal);
    TestDriver driver(options);
    const uint64_t address = 0x83000;
    const LineData data = MetadataPattern(101);
    SeedDirtyOwner(driver, address, data);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 4, address, true)));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 6);
    const auto metadata = MakeMetadata(data, MetadataFault::Parity, 6, 49);
    LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(6, snoop.flit.TxnID(), CHI::Eb::Opcodes::DAT::SnpRespData,
        data, FullEnables(), CHI::Eb::Resps::SnpRespData_I_PD, metadata)));
    driver.Tick();
    const DAT response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::DERR);
    driver.CompAck(4, response.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9MetadataConversionAtomicOperand)
{
    CheckAtomicMetadata(false);
}

LUNATIC_TEST(Stage9MetadataConversionAtomicBackendResult)
{
    CheckAtomicMetadata(true);
}

LUNATIC_TEST(Stage9MetadataAbsentFieldsRemainClean)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    HomeNode<Config256> withoutFields(2, options);
    Driver<Config256> driver(options);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 5, 0x84000)));
    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        Driver<Config256>::DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = downstream.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_I;
        data.DataID() = beat * 2;
        data.Data()[0] = 0x0123456789abcdefULL + beat;
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
    }
    driver.Tick();
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::OK);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}


namespace {

    void SendReadData(TestDriver& driver, const REQ& request, const LineData& data,
                      MetadataFault fault = MetadataFault::None, size_t chunk = 0)
    {
        const auto metadata = MakeMetadata(data, fault, chunk, chunk * 8);
        DAT response = MakePacket(6, request.TxnID(), CHI::Eb::Opcodes::DAT::CompData,
                                  data, FullEnables(), CHI::Eb::Resps::CompData_I, metadata);
        response.HomeNID() = 2;
        response.BE() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(response));
        driver.Tick();
    }

    DAT CompleteWrite(TestDriver& driver, const REQ& request)
    {
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::DBIDResp);
        const DAT data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        SendBackendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
        return data;
    }

    void QuiesceWithBackend(TestDriver& driver, const LineData& readData)
    {
        for (size_t cycle = 0; cycle < 500; ++cycle)
        {
            if (driver.node.PeekTXREQ())
            {
                const REQ request = NextBackendRequest(driver);
                if (IsReadRequest(request.Opcode()))
                    SendReadData(driver, request, readData);
                else
                    CompleteWrite(driver, request);
                continue;
            }
            if (driver.node.Drain())
                return;
            driver.Tick();
        }
        throw std::runtime_error("Lunatic backend-assisted drain timed out");
    }

    void CheckVictimMetadata()
    {
        Options options = MetadataOptions(false, false);
        options.policy = PolicyKind::Inclusive;
        options.slcSets = 1;
        options.slcWays = 1;
        TestDriver driver(options);
        const uint64_t a = 0x85000;
        const uint64_t b = 0x85040;
        const LineData dataA = MetadataPattern(113);
        const LineData dataB = MetadataPattern(167);

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 10, a)));
        auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(4, response.DBID(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            dataA, FullEnables(), CHI::Eb::Resps::CopyBackWrData_UD_PD,
            MakeMetadata(dataA, MetadataFault::None, 0, 0))));
        driver.Tick();
        driver.Quiesce();

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 11, a, true)));
        const DAT initial = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.CompAck(5, initial.DBID());
        driver.Quiesce();

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 12, b)));
        response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(6, response.DBID(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            dataB, FullEnables(), CHI::Eb::Resps::CopyBackWrData_UD_PD,
            MakeMetadata(dataB, MetadataFault::None, 0, 0))));
        driver.Tick();
        const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 5);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(5, victim.flit.TxnID(), CHI::Eb::Opcodes::DAT::SnpRespData,
            dataA, FullEnables(), CHI::Eb::Resps::SnpRespData_I_PD,
            MakeMetadata(dataA, MetadataFault::Poison, 3, 24))));
        driver.Tick();

        const REQ downstream = NextBackendRequest(driver);
        LUNATIC_CHECK(downstream.Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        LUNATIC_CHECK(CompleteWrite(driver, downstream).RespErr() == CHI::Eb::RespErrs::DERR);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckBaselineMetadata()
    {
        Options options = MetadataOptions(false, false);
        options.policy = PolicyKind::Exclusive;
        TestDriver driver(options);
        const LineKey line = LineKey::FromAddress(0x86000, false);
        const LineData data = MetadataPattern(197);
        ByteEnables enables{};
        LineData partialData{};
        for (size_t index = 0; index < LINE_SIZE; ++index)
            if (index % 32 < 16)
            {
                enables[index] = 1;
                partialData[index] = data[index];
            }
        const auto cleanPartial = MakeMetadata(partialData, MetadataFault::None, 0, 0);

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 4, 20, line.address)));
        auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(4, response.DBID(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            partialData, enables, CHI::Eb::Resps::CopyBackWrData_UD_PD, cleanPartial)));
        driver.Tick();
        REQ downstream = NextBackendRequest(driver);
        LUNATIC_CHECK(downstream.Opcode() == CHI::Eb::Opcodes::REQ::ReadNoSnp);
        SendReadData(driver, downstream, data);
        driver.Quiesce();

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 21, line.address, true)));
        downstream = NextBackendRequest(driver);
        CompleteWrite(driver, downstream);
        const DAT initial = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(initial.RespErr() == CHI::Eb::RespErrs::OK);
        driver.CompAck(5, initial.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(line, 5) == CacheState::UDP);

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 22, line.address, true)));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(snoop.target == 5);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakePacket(5, snoop.flit.TxnID(), CHI::Eb::Opcodes::DAT::SnpRespDataPtl,
            partialData, enables, CHI::Eb::Resps::SnpRespDataPtl_I_PD, cleanPartial)));
        driver.Tick();
        downstream = NextBackendRequest(driver);
        LUNATIC_CHECK(downstream.Opcode() == CHI::Eb::Opcodes::REQ::ReadNoSnp);
        SendReadData(driver, downstream, data, MetadataFault::Poison, 7);
        downstream = NextBackendRequest(driver);
        LUNATIC_CHECK(downstream.Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        CompleteWrite(driver, downstream);

        const DAT read = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(read.RespErr() == CHI::Eb::RespErrs::DERR);
        for (size_t index = 0; index < LINE_SIZE; ++index)
            LUNATIC_CHECK(uint8_t(read.Data()[index / 8] >> ((index % 8) * 8)) == data[index]);
        driver.CompAck(6, read.DBID());
        QuiesceWithBackend(driver, data);
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9MetadataConversionVictim)
{
    CheckVictimMetadata();
}

LUNATIC_TEST(Stage9MetadataConversionBaseline)
{
    CheckBaselineMetadata();
}


LUNATIC_TEST(Stage9MetadataConversionMultipleParityAndApplicability)
{
    for (size_t chunk = 0; chunk < LINE_SIZE / 8; ++chunk)
    {
        Data flit;
        flit.bytes = MetadataPattern(223);
        flit.poisonPresent = true;
        flit.dataCheckPresent = true;
        Options generate;
        generate.dataCheckEnabled = true;
        GenerateMetadata(flit.poison, flit.dataCheck, flit.bytes, FullEnables(), generate);
        for (size_t byte = chunk * 8 + 1; byte < chunk * 8 + 8; byte += 2)
            flit.dataCheck[byte] ^= 1;

        Options options;
        options.poisonEnabled = true;
        LUNATIC_CHECK(HasUnsupportedMetadataError(flit, FullEnables(), 0, 0, LINE_SIZE, options));
        options.dataCheckEnabled = true;
        LUNATIC_CHECK(!HasUnsupportedMetadataError(flit, FullEnables(), 0, 0, LINE_SIZE, options));
        flit.dataCheckPresent = false;
        options.dataCheckEnabled = false;
        LUNATIC_CHECK(!HasUnsupportedMetadataError(flit, FullEnables(), 0, 0, LINE_SIZE, options));
        flit.dataCheckPresent = true;
        flit.respErr = CHI::Eb::RespErrs::DERR;
        LUNATIC_CHECK(!HasUnsupportedMetadataError(flit, FullEnables(), 0, 0, LINE_SIZE, options));
        flit.respErr = CHI::Eb::RespErrs::NDERR;
        LUNATIC_CHECK(!HasUnsupportedMetadataError(flit, FullEnables(), 0, 0, LINE_SIZE, options));
    }
}
