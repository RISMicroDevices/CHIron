#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void SendSeparatedBackendRead(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                                  const typename CheckedDriver<config>::REQ& request)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT response{};
            response.SrcID() = 6;
            response.TgtID() = 2;
            response.HomeNID() = 2;
            response.TxnID() = request.TxnID();
            response.Opcode() = CHI::Eb::Opcodes::DAT::DataSepResp;
            response.Resp() = CHI::Eb::Resps::DataSepResp_UC;
            response.DataID() = beat * bytes / 16;
            response.CCID() = (request.Addr() >> 4) & 3;
            oracle.Data(driver.driver.now, response);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(response));
        }
        driver.driver.Tick();
    }

}

LUNATIC_TEST(Stage6OrderingRejectsReservedRNOrder)
{
    Driver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false);
    request.Order() = 1;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::DERR);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    driver.Quiesce();
}

LUNATIC_TEST(Stage6OrderingRejectsUnsupportedOrderedOpcode)
{
    Driver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, 0x10000, true);
    request.Order() = 2;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt).RespErr() == CHI::Eb::RespErrs::OK);
    DataBufferId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::NDERR);
        dbid = data.DBID();
    }
    driver.CompAck(4, dbid);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    driver.Quiesce();
}

LUNATIC_TEST(Stage6OrderingReadReceiptReleasesOnPop)
{
    CheckedDriver<Config256> driver;
    for (uint16_t txnId = 1; txnId <= 2; ++txnId)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, txnId,
            0x10000 + txnId * 0x1000, false);
        request.Order() = 2;
        driver.PushRequest(request);
    }

    driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        const auto pending = driver.driver.node.PeekTXRSP();
        LUNATIC_CHECK(pending && pending->TxnID() == 1);
        driver.driver.Tick();
    }
    const auto first = driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);
    LUNATIC_CHECK(first.TxnID() == 1);
    const auto second = driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);
    LUNATIC_CHECK(second.TxnID() == 2);

    for (uint16_t txnId = 1; txnId <= 2; ++txnId)
    {
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    }
    driver.Quiesce();
}

LUNATIC_TEST(Stage6OrderingDBIDRespOrdReleasesOnPop)
{
    CheckedDriver<Config256> driver;
    for (uint16_t txnId = 1; txnId <= 2; ++txnId)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, txnId,
            0x10000 + txnId * 0x1000, false);
        request.Order() = 2;
        driver.PushRequest(request);
    }

    driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        const auto pending = driver.driver.node.PeekTXRSP();
        LUNATIC_CHECK(pending && pending->TxnID() == 1);
        driver.driver.Tick();
    }

    std::vector<uint16_t> dbidTransactions;
    std::vector<uint16_t> compTransactions;
    LineData data{};
    ByteEnables enables{};
    enables.fill(1);
    while (dbidTransactions.size() < 2 || compTransactions.size() < 2)
    {
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const auto opcode = driver.driver.node.PeekTXRSP()->Opcode();
        if (opcode == CHI::Eb::Opcodes::RSP::DBIDRespOrd)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
            dbidTransactions.push_back(response.TxnID());
            driver.WriteData(4, response.TxnID(), response.DBID(), data, enables,
                CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        }
        else
        {
            LUNATIC_CHECK(opcode == CHI::Eb::Opcodes::RSP::Comp);
            compTransactions.push_back(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID());
        }
    }
    LUNATIC_CHECK(dbidTransactions.size() == 2 && dbidTransactions[0] == 1 && dbidTransactions[1] == 2);
    LUNATIC_CHECK(compTransactions.size() == 2);
    driver.Quiesce();
}

LUNATIC_TEST(Stage6OrderingOWOWaitsForCompAck)
{
    CheckedDriver<Config256> driver;
    for (uint16_t txnId = 1; txnId <= 2; ++txnId)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, txnId,
            0x10000 + txnId * 0x1000, true);
        request.Order() = 2;
        driver.PushRequest(request);
    }

    const auto firstDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
    LUNATIC_CHECK(firstDBID.TxnID() == 1);
    LineData data{};
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 1, firstDBID.DBID(), data, enables,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    for (size_t cycle = 0; cycle < 4; ++cycle)
    {
        LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());
        driver.driver.Tick();
    }
    driver.CompAck(4, firstDBID.DBID());

    const auto secondDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
    LUNATIC_CHECK(secondDBID.TxnID() == 2);
    const auto firstComp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(firstComp.TxnID() == 1);
    driver.WriteData(4, 2, secondDBID.DBID(), data, enables,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    driver.CompAck(4, secondDBID.DBID());
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
}

LUNATIC_TEST(Stage6OrderingBackendRequestAcceptedAndLateReceipt)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.separatedReadResponse = true;
    options.separatedBackendRead = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, true);
    request.Order() = 2;
    driver.PushRequest(request);

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, downstream);
    SendSeparatedBackendRead(driver, oracle, downstream);

    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);
    driver.CompAck(4, response.DBID());
    driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);

    typename CheckedDriver<Config256>::RSP receipt{};
    receipt.SrcID() = 6;
    receipt.TgtID() = 2;
    receipt.TxnID() = downstream.TxnID();
    receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
    oracle.Response(driver.driver.now, receipt);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(receipt));
    driver.driver.Tick();
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage6OrderingOWOVisibilityWaitsForCompAck)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::WriteNoSnpFull, CHI::Eb::Opcodes::REQ::WriteUniqueFull })
        for (size_t ackForm = 0; ackForm < 3; ++ackForm)
        {
            CheckedDriver<Config256> driver;
            const uint64_t address = 0x18000;
            const LineKey line = LineKey::FromAddress(address, false);
            auto request = driver.Request(opcode, 4, 1, address, true);
            request.Order() = 2;
            driver.PushRequest(request);
            const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
            LineData data{};
            data.fill(0x5c);
            ByteEnables enables{};
            enables.fill(1);
            if (ackForm == 0)
                driver.CompAck(4, dbid.DBID());
            const uint16_t dataOpcode = ackForm == 1 ? CHI::Eb::Opcodes::DAT::NCBWrDataCompAck
                                                     : CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
            driver.WriteData(4, 1, dbid.DBID(), data, enables, dataOpcode);
            for (size_t cycle = 0; cycle < 8; ++cycle)
                driver.driver.Tick();
            if (ackForm == 2)
            {
                if (opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFull)
                    LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
                else
                    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == LineData{});
                LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());
                driver.CompAck(4, dbid.DBID());
            }

            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
            driver.Quiesce();
            if (opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFull)
                LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == data);
            else
                LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == data);
        }
}

LUNATIC_TEST(Stage6OrderingOWOCancelRetainsCompAckLifetime)
{
    CheckedDriver<Config256> driver;
    const uint64_t address = 0x19000;
    const LineKey line = LineKey::FromAddress(address, false);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 1, address, true);
    request.Order() = 2;
    request.Size() = 4;
    driver.PushRequest(request);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
    driver.WriteData(4, 1, dbid.DBID(), LineData{}, ByteEnables{},
        CHI::Eb::Opcodes::DAT::WriteDataCancel, 0, 1);
    for (size_t cycle = 0; cycle < 8; ++cycle)
        driver.driver.Tick();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == LineData{});
    LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());

    driver.CompAck(4, dbid.DBID());
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == LineData{});
}
