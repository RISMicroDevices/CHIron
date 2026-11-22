#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using RSP = TestDriver::RSP;
    using DAT = TestDriver::DAT;

    LineData DataPullPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    ByteEnables FullDataPullEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename Driver>
    auto StashRequest(Driver& driver, uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        if (IsSeparatedStashRequest(opcode))
            request.StashGroupID() = 0x25;
        return request;
    }

    void PushStashRequest(TestDriver& driver, const TestDriver::REQ& request)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
    }

    RSP PopResponse(TestDriver& driver)
    {
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });
        const auto response = driver.node.PopTXRSP();
        LUNATIC_CHECK(response);
        return *response;
    }

    void SendDataPullRSP(TestDriver& driver, const TestDriver::SNP& snoop,
                         DataBufferId dbid, uint8_t dataPull = 1)
    {
        RSP response{};
        response.SrcID() = 5;
        response.TgtID() = 2;
        response.TxnID() = snoop.flit.TxnID();
        response.DBID() = dbid;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = CHI::Eb::Resps::SnpResp_I;
        response.FwdState() = dataPull;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SendDataPullDAT(TestDriver& driver, const TestDriver::SNP& snoop,
                         DataBufferId dbid, const LineData& line)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 5;
            data.TgtID() = 2;
            data.TxnID() = snoop.flit.TxnID();
            data.DBID() = dbid;
            data.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
            data.DataID() = beat * 2;
            data.CCID() = 0;
            data.BE() = ~uint64_t(0);
            data.DataSource() = 1;
            for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
                data.Data()[index / 8] |= uint64_t(line[beat * (Config256::dataWidth / 8) + index]) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    void CheckImpliedData(TestDriver& driver, NodeId target, TransactionId txnId,
                          const LineData& expected, bool separated = false)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                                      : CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == target && data.TxnID() == txnId);
            LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::OK);
            dbid = data.DBID();
            for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
                LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) ==
                    expected[beat * (Config256::dataWidth / 8) + index]);
        }
        driver.CompAck(target, dbid);
    }

    void CheckIndependentResponse(TestDriver& driver, uint16_t opcode)
    {
        if (!IsSeparatedStashRequest(opcode))
        {
            LUNATIC_CHECK(PopResponse(driver).Opcode() == CHI::Eb::Opcodes::RSP::Comp);
            return;
        }
        const auto first = PopResponse(driver);
        if (first.Opcode() == CHI::Eb::Opcodes::RSP::CompStashDone)
            return;
        const auto second = PopResponse(driver);
        LUNATIC_CHECK((first.Opcode() == CHI::Eb::Opcodes::RSP::Comp &&
                       second.Opcode() == CHI::Eb::Opcodes::RSP::StashDone) ||
                      (first.Opcode() == CHI::Eb::Opcodes::RSP::StashDone &&
                       second.Opcode() == CHI::Eb::Opcodes::RSP::Comp));
    }

    void CheckIndependentDataPull(uint16_t opcode)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x7d000, false);
        const LineData expected = DataPullPattern(0x31);
        driver.node.GetMemory().WriteFull(line, expected);
        PushStashRequest(driver, StashRequest(driver, opcode, 10, line.address));
        const auto expectedSnoop = opcode == CHI::Eb::Opcodes::REQ::StashOnceShared ||
            opcode == CHI::Eb::Opcodes::REQ::StashOnceSepShared
            ? CHI::Eb::Opcodes::SNP::SnpStashShared : CHI::Eb::Opcodes::SNP::SnpStashUnique;
        const auto snoop = driver.NextSNP(expectedSnoop);
        LUNATIC_CHECK(snoop.target == 5);
        const DataBufferId dbid = 0x37;
        SendDataPullRSP(driver, snoop, dbid);
        CheckIndependentResponse(driver, opcode);
        CheckImpliedData(driver, 5, dbid, expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckWriteDataPull(uint16_t opcode)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x7d040, false);
        const LineData initial = DataPullPattern(0x51);
        const LineData update = DataPullPattern(0xa7);
        driver.node.GetMemory().WriteFull(line, initial);
        PushStashRequest(driver, StashRequest(driver, opcode, 10, line.address));
        const auto expectedSnoop = opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
            ? CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash : CHI::Eb::Opcodes::SNP::SnpUniqueStash;
        const auto snoop = driver.NextSNP(expectedSnoop);
        LUNATIC_CHECK(snoop.target == 5);
        const DataBufferId dbid = 0x38;
        if (opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash)
            SendDataPullRSP(driver, snoop, dbid);
        else
            SendDataPullDAT(driver, snoop, dbid, initial);

        const auto dbidResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        ByteEnables enables{};
        if (opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash)
            enables.fill(1);
        else
            for (size_t index = 7; index < 23; ++index)
                enables[index] = 1;
        driver.WriteData(4, 10, dbidResponse.DBID(), update, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LineData expected = initial;
        for (size_t index = 0; index < expected.size(); ++index)
            if (enables[index])
                expected[index] = update[index];
        CheckImpliedData(driver, 5, dbid, expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9DataPullIndependentRSPImpliedRead)
{
    CheckIndependentDataPull(CHI::Eb::Opcodes::REQ::StashOnceShared);
    CheckIndependentDataPull(CHI::Eb::Opcodes::REQ::StashOnceSepUnique);
}

LUNATIC_TEST(Stage9DataPullIndependentDataResponseRejected)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::StashOnceUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepShared })
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x7d000, false);
        const LineData expected = DataPullPattern(0x31);
        driver.node.GetMemory().WriteFull(line, expected);
        PushStashRequest(driver, StashRequest(driver, opcode, 10, line.address));
        const auto expectedSnoop = opcode == CHI::Eb::Opcodes::REQ::StashOnceSepShared
            ? CHI::Eb::Opcodes::SNP::SnpStashShared : CHI::Eb::Opcodes::SNP::SnpStashUnique;
        const auto snoop = driver.NextSNP(expectedSnoop);
        SendDataPullDAT(driver, snoop, 0x37, expected);
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
    }
}

LUNATIC_TEST(Stage9DataPullWriteUniqueStashReturnsUpdatedLine)
{
    CheckWriteDataPull(CHI::Eb::Opcodes::REQ::WriteUniquePtlStash);
    CheckWriteDataPull(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash);
}

LUNATIC_TEST(Stage9DataPullDMTFromSubordinate)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dmtEnabled = true;
    TestDriver driver(options);
    const LineKey line = LineKey::FromAddress(0x7d080, false);
    const LineData expected = DataPullPattern(0x62);
    driver.node.GetMemory().WriteFull(line, expected);
    PushStashRequest(driver, StashRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceShared, 10, line.address));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashShared);
    const DataBufferId dbid = 0x37;
    SendDataPullRSP(driver, snoop, dbid);
    CheckIndependentResponse(driver, CHI::Eb::Opcodes::REQ::StashOnceShared);

    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.ReturnNID() == 5 && downstream.ReturnTxnID() == dbid);
    LUNATIC_CHECK(downstream.Order() == 1);
    RSP receipt{};
    receipt.SrcID() = 6;
    receipt.TgtID() = 2;
    receipt.TxnID() = downstream.TxnID();
    receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
    LUNATIC_CHECK(driver.node.PushRXRSP(receipt));
    driver.Tick();
    driver.CompAck(5, downstream.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9DataPullSeparatedResponse)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.separatedReadResponse = true;
    options.separatedBackendRead = true;
    TestDriver driver(options);
    const LineKey line = LineKey::FromAddress(0x7d0c0, false);
    const LineData expected = DataPullPattern(0x72);
    PushStashRequest(driver, StashRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceShared, 10, line.address));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashShared);
    const DataBufferId dbid = 0x37;
    SendDataPullRSP(driver, snoop, dbid);
    CheckIndependentResponse(driver, CHI::Eb::Opcodes::REQ::StashOnceShared);
    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
    LUNATIC_CHECK(downstream.ReturnNID() == 2 && downstream.ReturnTxnID() == downstream.TxnID());

    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = downstream.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::DataSepResp;
        data.Resp() = CHI::Eb::Resps::DataSepResp_UC;
        data.DataID() = beat * 2;
        data.CCID() = 0;
        for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
            data.Data()[index / 8] |= uint64_t(expected[beat * (Config256::dataWidth / 8) + index]) << ((index % 8) * 8);
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
    }
    driver.Tick();
    const auto response = PopResponse(driver);
    LUNATIC_CHECK(response.Opcode() == CHI::Eb::Opcodes::RSP::RespSepData);
    CheckImpliedData(driver, 5, dbid, expected, true);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9DataPullRejectsReservedEncoding)
{
    TestDriver driver;
    PushStashRequest(driver, StashRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceShared, 10, 0x7d100));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashShared);
    SendDataPullRSP(driver, snoop, 0x37, 2);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    LUNATIC_CHECK(!driver.node.PeekTXDAT());
}

namespace {

    using CheckedDataPullDriver = CheckedDriver<Config256>;
    using CheckedDataPullRSP = CheckedDataPullDriver::RSP;
    using CheckedDataPullDAT = CheckedDataPullDriver::DAT;

    void SendDataPullRSPOracle(CheckedDataPullDriver& driver, const CheckedDataPullDriver::SNP& snoop,
                               DataBufferId dbid, uint8_t resp = CHI::Eb::Resps::SnpResp_I)
    {
        CheckedDataPullRSP response{};
        response.SrcID() = 5;
        response.TgtID() = driver.home;
        response.TxnID() = snoop.flit.TxnID();
        response.DBID() = dbid;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = resp;
        response.DataPull() = 1;
        driver.Oracle(5).SnoopResponse(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

    void SendDataPullDATOracle(CheckedDataPullDriver& driver, const CheckedDataPullDriver::SNP& snoop,
                               DataBufferId dbid, const LineData& line, bool partial)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            CheckedDataPullDAT data{};
            data.SrcID() = 5;
            data.TgtID() = driver.home;
            data.TxnID() = snoop.flit.TxnID();
            data.DBID() = dbid;
            data.Opcode() = partial ? CHI::Eb::Opcodes::DAT::SnpRespDataPtl
                : CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = partial ? CHI::Eb::Resps::SnpRespDataPtl_I_PD
                : CHI::Eb::Resps::SnpRespData_I_PD;
            data.DataID() = beat * 2;
            data.DataSource() = 1;
            for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
                if (!partial || index < Config256::dataWidth / 16)
                {
                    data.Data()[index / 8] |= uint64_t(line[beat * 32 + index]) << ((index % 8) * 8);
                    data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
                }
            driver.Oracle(5).SnoopData(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
    }

    void CheckIndependentResponseOracle(CheckedDataPullDriver& driver, uint16_t opcode)
    {
        if (!IsSeparatedStashRequest(opcode))
        {
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).Opcode() == CHI::Eb::Opcodes::RSP::Comp);
            return;
        }
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const uint16_t firstOpcode = driver.driver.node.PeekTXRSP()->Opcode();
        const auto first = driver.NextRSP(firstOpcode);
        if (first.Opcode() == CHI::Eb::Opcodes::RSP::CompStashDone)
            return;
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const auto second = driver.NextRSP(driver.driver.node.PeekTXRSP()->Opcode());
        LUNATIC_CHECK((first.Opcode() == CHI::Eb::Opcodes::RSP::Comp &&
                       second.Opcode() == CHI::Eb::Opcodes::RSP::StashDone) ||
                      (first.Opcode() == CHI::Eb::Opcodes::RSP::StashDone &&
                       second.Opcode() == CHI::Eb::Opcodes::RSP::Comp));
    }

    void CheckImpliedDataOracle(CheckedDataPullDriver& driver, NodeId target, TransactionId txnId,
                                const LineData& expected, bool separated = false)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                                      : CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == target && data.TxnID() == txnId);
            LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::OK);
            dbid = data.DBID();
            for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
                LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) ==
                    expected[beat * 32 + index]);
        }
        driver.CompAck(target, dbid);
    }

    void CheckIndependentDataPullOracle(uint16_t opcode)
    {
        CheckedDataPullDriver driver;
        const LineKey line = LineKey::FromAddress(0x7d200, false);
        const LineData expected = DataPullPattern(0x31);
        driver.driver.node.GetMemory().WriteFull(line, expected);
        driver.PushRequest(StashRequest(driver, opcode, 10, line.address));
        const auto expectedSnoop = opcode == CHI::Eb::Opcodes::REQ::StashOnceShared ||
            opcode == CHI::Eb::Opcodes::REQ::StashOnceSepShared
            ? CHI::Eb::Opcodes::SNP::SnpStashShared : CHI::Eb::Opcodes::SNP::SnpStashUnique;
        const auto snoop = driver.NextSNP(expectedSnoop);
        LUNATIC_CHECK(snoop.target == 5);
        SendDataPullRSPOracle(driver, snoop, 0x37);
        CheckIndependentResponseOracle(driver, opcode);
        CheckImpliedDataOracle(driver, 5, 0x37, expected);
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage9DataPullCHIronIndependentRSP)
{
    CheckIndependentDataPullOracle(CHI::Eb::Opcodes::REQ::StashOnceShared);
    CheckIndependentDataPullOracle(CHI::Eb::Opcodes::REQ::StashOnceSepUnique);
}

LUNATIC_TEST(Stage9DataPullCHIronUniqueStashPartialData)
{
    CheckedDataPullDriver driver;
    const LineKey line = LineKey::FromAddress(0x7d240, false);
    const LineData initial = DataPullPattern(0x51);
    const LineData update = DataPullPattern(0xa7);
    driver.driver.node.GetMemory().WriteFull(line, initial);
    driver.PushRequest(StashRequest(driver, CHI::Eb::Opcodes::REQ::WriteUniquePtlStash, 10, line.address));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUniqueStash);
    SendDataPullDATOracle(driver, snoop, 0x38, initial, true);

    const auto dbidResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    ByteEnables enables{};
    for (size_t index = 7; index < 23; ++index)
        enables[index] = 1;
    driver.WriteData(4, 10, dbidResponse.DBID(), update, enables,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LineData expected = initial;
    for (size_t index = 0; index < expected.size(); ++index)
        if (enables[index])
            expected[index] = update[index];
    CheckImpliedDataOracle(driver, 5, 0x38, expected);
    driver.Quiesce();
}

LUNATIC_TEST(Stage9DataPullCHIronDMT)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dmtEnabled = true;
    CheckedDataPullDriver driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(5);
    driver.Oracle(5).RegisterSubordinate(6);
    const LineKey line = LineKey::FromAddress(0x7d280, false);
    const LineData expected = DataPullPattern(0x62);
    driver.driver.node.GetMemory().WriteFull(line, expected);
    driver.PushRequest(StashRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceShared, 10, line.address));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashShared);
    SendDataPullRSPOracle(driver, snoop, 0x37);
    CheckIndependentResponseOracle(driver, CHI::Eb::Opcodes::REQ::StashOnceShared);

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.ReturnNID() == 5 && downstream.ReturnTxnID() == 0x37);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, downstream);
    CheckedDataPullRSP receipt{};
    receipt.SrcID() = 6;
    receipt.TgtID() = 2;
    receipt.TxnID() = downstream.TxnID();
    receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
    oracle.Response(driver.driver.now, receipt);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(receipt));
    driver.driver.Tick();

    for (size_t beat = 0; beat < 2; ++beat)
    {
        CheckedDataPullDAT data{};
        data.SrcID() = 6;
        data.TgtID() = 5;
        data.HomeNID() = 2;
        data.TxnID() = 0x37;
        data.DBID() = downstream.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_SC;
        data.DataID() = beat * 2;
        data.BE() = ~uint32_t(0);
        for (size_t index = 0; index < 32; ++index)
            data.Data()[index / 8] |= uint64_t(expected[beat * 32 + index]) << ((index % 8) * 8);
        oracle.Data(driver.driver.now, data);
        driver.Oracle(5).Data(driver.driver.now, data);
    }
    driver.CompAck(5, downstream.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage9DataPullCHIronSeparatedResponse)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.separatedReadResponse = true;
    options.separatedBackendRead = true;
    CheckedDataPullDriver driver(options);
    SNOracle<Config256> oracle(2, 6);
    const LineKey line = LineKey::FromAddress(0x7d2c0, false);
    const LineData expected = DataPullPattern(0x72);
    driver.PushRequest(StashRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceShared, 10, line.address));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashShared);
    SendDataPullRSPOracle(driver, snoop, 0x37);
    CheckIndependentResponseOracle(driver, CHI::Eb::Opcodes::REQ::StashOnceShared);

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
    LUNATIC_CHECK(downstream.ReturnNID() == 2 && downstream.ReturnTxnID() == downstream.TxnID());
    oracle.Request(driver.driver.now, downstream);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        CheckedDataPullDAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = downstream.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::DataSepResp;
        data.Resp() = CHI::Eb::Resps::DataSepResp_UC;
        data.DataID() = beat * 2;
        for (size_t index = 0; index < 32; ++index)
            data.Data()[index / 8] |= uint64_t(expected[beat * 32 + index]) << ((index % 8) * 8);
        oracle.Data(driver.driver.now, data);
        LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
    }
    driver.driver.Tick();
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    LUNATIC_CHECK(response.TgtID() == 5 && response.TxnID() == 0x37);
    CheckImpliedDataOracle(driver, 5, 0x37, expected, true);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage9DataPullPreciseResponseMatrix)
{
    const OrdinarySnoopResponse uniqueShared{
        .opcode = CHI::Eb::Opcodes::SNP::SnpStashUnique,
        .resp = CHI::Eb::Resps::SnpResp_SC,
        .dataPull = 1
    };
    LUNATIC_CHECK(IsOrdinarySnoopResponseLegal({ CacheState::SC }, uniqueShared));
    const OrdinarySnoopResponse sharedUnique{
        .opcode = CHI::Eb::Opcodes::SNP::SnpStashShared,
        .resp = CHI::Eb::Resps::SnpResp_UC,
        .dataPull = 1
    };
    LUNATIC_CHECK(IsOrdinarySnoopResponseLegal({ CacheState::UCE }, sharedUnique));
    const OrdinarySnoopResponse sharedIllegal{
        .opcode = CHI::Eb::Opcodes::SNP::SnpStashShared,
        .resp = CHI::Eb::Resps::SnpResp_SC,
        .dataPull = 1
    };
    LUNATIC_CHECK(!IsOrdinarySnoopResponseLegal({ CacheState::SC }, sharedIllegal));
    const OrdinarySnoopResponse cleanData{
        .opcode = CHI::Eb::Opcodes::SNP::SnpUniqueStash,
        .resp = CHI::Eb::Resps::SnpRespData_I,
        .dataPull = 1,
        .withData = true
    };
    LUNATIC_CHECK(IsOrdinarySnoopResponseLegal({ CacheState::UC }, cleanData));
}
