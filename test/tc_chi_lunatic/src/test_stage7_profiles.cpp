#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData MetadataPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    Options MetadataOptions()
    {
        Options options;
        options.dataWidth = 512;
        options.poisonEnabled = true;
        options.dataCheckEnabled = true;
        return options;
    }

    LinePoison MetadataPoison(uint8_t seed)
    {
        LinePoison poison{};
        for (size_t lane = 0; lane < poison.size(); ++lane)
            poison[lane] = (seed >> lane) & 1;
        return poison;
    }

    LineDataCheck IncorrectParity(const LineData& data)
    {
        Options options;
        options.dataCheckEnabled = true;
        LinePoison poison{};
        LineDataCheck check{};
        ByteEnables enables{};
        enables.fill(1);
        GenerateMetadata(poison, check, data, enables, options);
        for (size_t index = 0; index < check.size(); index += 3)
            check[index] ^= 1;
        return check;
    }

    Driver<Config512>::DAT MetadataDAT(NodeId source, NodeId target, TransactionId txnId, uint16_t opcode,
                                       const LineData& data, const ByteEnables& enables,
                                       const LinePoison& poison, const LineDataCheck& check, uint8_t resp = 0)
    {
        Driver<Config512>::DAT flit{};
        flit.SrcID() = source;
        flit.TgtID() = target;
        flit.TxnID() = txnId;
        flit.Opcode() = opcode;
        flit.Resp() = resp;
        uint64_t byteEnables = 0;
        uint64_t poisonBits = 0;
        uint64_t checkBits = 0;
        for (size_t index = 0; index < LINE_SIZE; ++index)
        {
            if (enables[index])
                flit.Data()[index / 8] |= uint64_t(data[index]) << ((index % 8) * 8);
            byteEnables |= uint64_t(bool(enables[index])) << index;
            checkBits |= uint64_t(check[index] & 1) << index;
        }
        for (size_t lane = 0; lane < poison.size(); ++lane)
            poisonBits |= uint64_t(poison[lane] & 1) << lane;
        flit.BE() = byteEnables;
        flit.Poison() = poisonBits;
        flit.DataCheck() = checkBits;
        return flit;
    }

    LineImage ReadImage(const Driver<Config512>::DAT& flit)
    {
        LineImage image;
        for (size_t index = 0; index < LINE_SIZE; ++index)
        {
            image.data[index] = uint8_t(flit.Data()[index / 8] >> ((index % 8) * 8));
            image.dataCheck[index] = (uint64_t(flit.DataCheck()) >> index) & 1;
        }
        for (size_t lane = 0; lane < image.poison.size(); ++lane)
            image.poison[lane] = (uint64_t(flit.Poison()) >> lane) & 1;
        return image;
    }

    void PushWrite(Driver<Config512>& driver, uint16_t requestOpcode, TransactionId txnId, uint64_t address,
                   const LineData& data, const ByteEnables& enables, const LinePoison& poison,
                   const LineDataCheck& check, uint16_t dataOpcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
                   uint8_t resp = 0)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(requestOpcode, 4, txnId, address)));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp).DBID();
        auto flit = MetadataDAT(4, 2, dbid, dataOpcode, data, enables, poison, check, resp);
        LUNATIC_CHECK(driver.node.PushRXDAT(flit));
        driver.Tick();
    }

}

LUNATIC_TEST(Stage7ProfileCapabilitiesAndUnsupportedFeatures)
{
    Options options;
    options.poisonEnabled = true;
    CheckThrows([&] { HomeNode<Config256> node(2, options); });
    options = {};
    options.dataCheckEnabled = true;
    CheckThrows([&] { HomeNode<Config256> node(2, options); });
    options = MetadataOptions();
    HomeNode<Config512> capable(2, options);
    LUNATIC_CHECK(capable.GetOptions().poisonEnabled);
    LUNATIC_CHECK(capable.GetOptions().dataCheckEnabled);

    options = {};
    options.mteEnabled = true;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.dvmEnabled = true;
    CheckThrows([&] { ValidateOptions(options); });
}

LUNATIC_TEST(Stage7DVMRequestIsContained)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x73000, false);
    const LineData initial = MetadataPattern(61);
    driver.node.GetMemory().WriteFull(line, initial);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::DVMOp, 4, 44, line.address);
    request.Size() = 3;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(response.TgtID() == 4 && response.TxnID() == 44);
    LUNATIC_CHECK(response.RespErr() != CHI::Eb::RespErrs::OK);
    for (size_t cycle = 0; cycle < 16; ++cycle)
    {
        LUNATIC_CHECK(!driver.node.PeekTXREQ());
        LUNATIC_CHECK(!driver.node.PeekTXSNP());
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
        driver.Tick();
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == initial);
    LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage7MetadataSurvivesMemoryAndPartialWrites)
{
    Driver<Config512> driver(MetadataOptions());
    const LineKey line = LineKey::FromAddress(0x71000, false);
    const LineData initial = MetadataPattern(3);
    const LinePoison initialPoison = MetadataPoison(0x35);
    const LineDataCheck initialCheck = IncorrectParity(initial);
    ByteEnables full{};
    full.fill(1);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 1, line.address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    auto write = MetadataDAT(4, 2, dbid, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
                             initial, full, initialPoison, initialCheck);
    LUNATIC_CHECK(driver.node.PushRXDAT(write));
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 2, line.address)));
    LineImage image = ReadImage(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData));
    LUNATIC_CHECK(image.data == initial);
    LUNATIC_CHECK(image.poison == initialPoison);
    LUNATIC_CHECK(image.dataCheck == initialCheck);

    const LineData update = MetadataPattern(91);
    const LinePoison updatePoison{};
    const LineDataCheck updateCheck = IncorrectParity(update);
    ByteEnables partial{};
    for (size_t index = 3; index < 6; ++index)
        partial[index] = 1;
    for (size_t index = 8; index < 16; ++index)
        partial[index] = 1;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 3, line.address)));
    const DataBufferId partialDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    write = MetadataDAT(4, 2, partialDBID, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
                        update, partial, updatePoison, updateCheck);
    LUNATIC_CHECK(driver.node.PushRXDAT(write));
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();

    LineData expectedData = initial;
    LineDataCheck expectedCheck = initialCheck;
    for (size_t index = 0; index < LINE_SIZE; ++index)
    {
        if (!partial[index])
            continue;
        expectedData[index] = update[index];
        expectedCheck[index] = updateCheck[index];
    }
    LinePoison expectedPoison = initialPoison;
    expectedPoison[1] = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 4, line.address)));
    image = ReadImage(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData));
    LUNATIC_CHECK(image.data == expectedData);
    LUNATIC_CHECK(image.poison == expectedPoison);
    LUNATIC_CHECK(image.dataCheck == expectedCheck);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage7GeneratedZeroDataHasOddParity)
{
    Driver<Config512> driver(MetadataOptions());
    const LineKey line = LineKey::FromAddress(0x72000, false);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpZero, 4, 5, line.address)));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 6, line.address)));
    const LineImage image = ReadImage(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData));
    LUNATIC_CHECK(image.data == LineData{});
    LUNATIC_CHECK(image.poison == LinePoison{});
    for (uint8_t check : image.dataCheck)
        LUNATIC_CHECK(check == 1);
    driver.Quiesce();
}

LUNATIC_TEST(Stage7MetadataSurvivesHomeAndSnoopPaths)
{
    Options options = MetadataOptions();
    options.policy = PolicyKind::NINE;
    Driver<Config512> driver(options);
    const LineKey line = LineKey::FromAddress(0x73000, false);
    const LineData data = MetadataPattern(17);
    const LinePoison poison = MetadataPoison(0x4a);
    const LineDataCheck check = IncorrectParity(data);
    ByteEnables full{};
    full.fill(1);
    PushWrite(driver, CHI::Eb::Opcodes::REQ::WriteBackFull, 7, line.address, data, full, poison, check,
              CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
    const HomeLineInfo home = driver.node.InspectHome(line);
    LUNATIC_CHECK(home.valid && home.data == data);
    LUNATIC_CHECK(home.poison == poison && home.dataCheck == check);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 8, line.address, true)));
    auto read = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(ReadImage(read).poison == poison);
    LUNATIC_CHECK(ReadImage(read).dataCheck == check);
    driver.CompAck(5, read.DBID());
    driver.Quiesce();

    const LineData snooped = MetadataPattern(113);
    const LinePoison snoopPoison = MetadataPoison(0x25);
    const LineDataCheck snoopCheck = IncorrectParity(snooped);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 9, line.address, true)));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    auto response = MetadataDAT(5, 2, snoop.flit.TxnID(), CHI::Eb::Opcodes::DAT::SnpRespData,
                                snooped, full, snoopPoison, snoopCheck, CHI::Eb::Resps::SnpRespData_I_PD);
    LUNATIC_CHECK(driver.node.PushRXDAT(response));
    driver.Tick();
    read = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    const LineImage image = ReadImage(read);
    LUNATIC_CHECK(image.data == snooped && image.poison == snoopPoison && image.dataCheck == snoopCheck);
    driver.CompAck(6, read.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage7MetadataSurvivesDirtyVictim)
{
    Options options = MetadataOptions();
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    Driver<Config512> driver(options);
    const LineKey a = LineKey::FromAddress(0x73500, false);
    const LineKey b = LineKey::FromAddress(0x73540, false);
    const LineData dataA = MetadataPattern(31);
    const LineData dataB = MetadataPattern(79);
    const LinePoison poisonA = MetadataPoison(0x69);
    const LineDataCheck checkA = IncorrectParity(dataA);
    ByteEnables full{};
    full.fill(1);
    PushWrite(driver, CHI::Eb::Opcodes::REQ::WriteBackFull, 20, a.address, dataA, full, poisonA, checkA,
              CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
    PushWrite(driver, CHI::Eb::Opcodes::REQ::WriteBackFull, 21, b.address, dataB, full, LinePoison{},
              IncorrectParity(dataB), CHI::Eb::Opcodes::DAT::CopyBackWrData,
              CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.InspectHome(a).valid);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 22, a.address)));
    const LineImage image = ReadImage(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData));
    LUNATIC_CHECK(image.data == dataA && image.poison == poisonA && image.dataCheck == checkA);
    driver.Quiesce();
}

LUNATIC_TEST(Stage7AtomicOperandAndResultMetadataStaySeparate)
{
    Driver<Config512> driver(MetadataOptions());
    const LineKey line = LineKey::FromAddress(0x73800, false);
    const LineData initial = MetadataPattern(43);
    const LinePoison initialPoison = MetadataPoison(0x01);
    const LineDataCheck initialCheck = IncorrectParity(initial);
    ByteEnables full{};
    full.fill(1);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 30, line.address)));
    const DataBufferId writeDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    auto write = MetadataDAT(4, 2, writeDBID, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
                             initial, full, initialPoison, initialCheck);
    LUNATIC_CHECK(driver.node.PushRXDAT(write));
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 31, line.address + 8);
    request.Size() = 3;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const DataBufferId atomicDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    LineData operands{};
    ByteEnables operandEnables{};
    for (size_t index = 8; index < 16; ++index)
    {
        operands[index] = uint8_t(0xa0 + index);
        operandEnables[index] = 1;
    }
    const LinePoison operandPoison = MetadataPoison(0x02);
    const LineDataCheck operandCheck = IncorrectParity(operands);
    write = MetadataDAT(4, 2, atomicDBID, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
                        operands, operandEnables, operandPoison, operandCheck);
    LUNATIC_CHECK(driver.node.PushRXDAT(write));
    driver.Tick();
    const LineImage original = ReadImage(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData));
    LUNATIC_CHECK(original.poison == initialPoison);
    LUNATIC_CHECK(original.dataCheck == initialCheck);
    driver.Quiesce();

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 32, line.address)));
    const LineImage updated = ReadImage(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData));
    LinePoison expectedPoison = initialPoison;
    expectedPoison[1] = 1;
    LUNATIC_CHECK(updated.poison == expectedPoison);
    for (size_t index = 0; index < LINE_SIZE; ++index)
    {
        if (operandEnables[index])
        {
            LUNATIC_CHECK(updated.data[index] == operands[index]);
            LUNATIC_CHECK(updated.dataCheck[index] == OddByteParity(operands[index]));
        }
        else
        {
            LUNATIC_CHECK(updated.data[index] == initial[index]);
            LUNATIC_CHECK(updated.dataCheck[index] == initialCheck[index]);
        }
    }
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage7MetadataSurvivesCHIBackend)
{
    Options options = MetadataOptions();
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config512> driver(options);
    const LineKey line = LineKey::FromAddress(0x74000, false);
    const LineData data = MetadataPattern(29);
    const LinePoison poison = MetadataPoison(0x53);
    const LineDataCheck check = IncorrectParity(data);
    ByteEnables full{};
    full.fill(1);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 10, line.address)));
    const auto request = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    auto downstream = MetadataDAT(6, 2, request.TxnID(), CHI::Eb::Opcodes::DAT::CompData,
                                  data, full, poison, check, CHI::Eb::Resps::CompData_I);
    downstream.HomeNID() = 2;
    LUNATIC_CHECK(driver.node.PushRXDAT(downstream));
    driver.Tick();
    const LineImage image = ReadImage(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData));
    LUNATIC_CHECK(image.data == data && image.poison == poison && image.dataCheck == check);
    driver.Quiesce();

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 11, line.address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    auto write = MetadataDAT(4, 2, dbid, CHI::Eb::Opcodes::DAT::NonCopyBackWrData, data, full, poison, check);
    LUNATIC_CHECK(driver.node.PushRXDAT(write));
    driver.Tick();
    const auto backendWrite = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    Driver<Config512>::RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = backendWrite.TxnID();
    response.DBID() = 91;
    response.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    downstream = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(downstream.TxnID() == 91);
    const LineImage written = ReadImage(downstream);
    LUNATIC_CHECK(written.data == data && written.poison == poison && written.dataCheck == check);
    response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
