#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void SendDMTData(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                     TransactionId originalTxnId, TransactionId downstreamTxnId,
                     const LineData& expected, uint16_t opcode)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        LineData observed{};
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 4;
            data.HomeNID() = 2;
            data.TxnID() = originalTxnId;
            data.DBID() = downstreamTxnId;
            data.Opcode() = opcode;
            data.Resp() = opcode == CHI::Eb::Opcodes::DAT::CompData
                ? CHI::Eb::Resps::CompData_I : CHI::Eb::Resps::DataSepResp_UC;
            data.DataID() = beat * bytes / 16;
            data.CCID() = 0;
            for (size_t index = 0; index < bytes; ++index)
                data.Data()[index / 8] |= uint64_t(expected[beat * bytes + index]) << ((index % 8) * 8);
            oracle.Data(driver.driver.now, data);
            driver.Oracle(4).Data(driver.driver.now, data);
            for (size_t index = 0; index < bytes; ++index)
                observed[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        LUNATIC_CHECK(observed == expected);
    }

    template<typename config>
    void SendBackendReceipt(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                            TransactionId downstreamTxnId)
    {
        typename CheckedDriver<config>::RSP receipt{};
        receipt.SrcID() = 6;
        receipt.TgtID() = 2;
        receipt.TxnID() = downstreamTxnId;
        receipt.Opcode() = CHI::Eb::Opcodes::RSP::ReadReceipt;
        oracle.Response(driver.driver.now, receipt);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(receipt));
        driver.driver.Tick();
    }

}

LUNATIC_TEST(Stage6DMTUnorderedEarlyDeallocation)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dmtEnabled = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);
    LineData expected{};
    for (size_t index = 0; index < expected.size(); ++index)
        expected[index] = uint8_t(0x96 ^ (index * 19));
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false));

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.ReturnNID() == 4);
    LUNATIC_CHECK(downstream.ReturnTxnID() == 1);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, downstream);
    SendBackendReceipt(driver, oracle, downstream.TxnID());
    SendDMTData(driver, oracle, 1, downstream.TxnID(), expected, CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
    LUNATIC_CHECK(!driver.driver.node.InspectHome(LineKey::FromAddress(0x10000, false)).valid);
}

LUNATIC_TEST(Stage6DMTSeparatedCompAckAndLateReceipt)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dmtEnabled = true;
    options.separatedReadResponse = true;
    options.separatedBackendRead = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);
    LineData expected{};
    for (size_t index = 0; index < expected.size(); ++index)
        expected[index] = uint8_t(0x3c + index * 23);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, true);
    request.Order() = 2;
    driver.PushRequest(request);

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
    LUNATIC_CHECK(downstream.ReturnNID() == 4);
    LUNATIC_CHECK(downstream.ReturnTxnID() == 1);
    LUNATIC_CHECK(downstream.Order() == 1);
    oracle.Request(driver.driver.now, downstream);
    SendDMTData(driver, oracle, 1, downstream.TxnID(), expected, CHI::Eb::Opcodes::DAT::DataSepResp);

    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    LUNATIC_CHECK(response.Resp() == 0);
    driver.CompAck(4, response.DBID());
    SendBackendReceipt(driver, oracle, downstream.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
    LUNATIC_CHECK(!driver.driver.node.InspectHome(LineKey::FromAddress(0x10000, false)).valid);
}

LUNATIC_TEST(Stage6DMTOrderedWithoutCompAckUsesHomeFallback)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::ReadNoSnp, CHI::Eb::Opcodes::REQ::ReadOnce })
        for (const uint8_t order : { uint8_t(2), uint8_t(3) })
            for (const bool separated : { false, true })
            {
                Options options;
                options.backend = BackendKind::CHI;
                options.subordinateId = 6;
                options.dmtEnabled = true;
                options.separatedReadResponse = separated;
                options.separatedBackendRead = separated;
                CheckedDriver<Config256> driver(options);
                SNOracle<Config256> oracle(2, 6);
                oracle.RegisterRequester(4);
                if (order == 3 && opcode == CHI::Eb::Opcodes::REQ::ReadOnce)
                    continue;
                LineData expected{};
                for (size_t index = 0; index < expected.size(); ++index)
                    expected[index] = uint8_t(0x31 + index * 17 + order * 7 + (separated ? 3 : 0));
                auto request = driver.Request(opcode, 4, 1, 0x10100, false);
                request.Order() = order;
                if (order == 3)
                    request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
                else if (opcode == CHI::Eb::Opcodes::REQ::ReadOnce)
                    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
                driver.PushRequest(request);

                const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
                LUNATIC_CHECK(downstream.ReturnNID() == 2);
                LUNATIC_CHECK(downstream.ReturnTxnID() == downstream.TxnID());
                LUNATIC_CHECK(downstream.Order() == 1);
                driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);
                oracle.Request(driver.driver.now, downstream);
                SendBackendReceipt(driver, oracle, downstream.TxnID());
                for (size_t beat = 0; beat < 2; ++beat)
                {
                    typename CheckedDriver<Config256>::DAT data{};
                    data.SrcID() = 6;
                    data.TgtID() = 2;
                    data.HomeNID() = 2;
                    data.TxnID() = downstream.TxnID();
                    data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
                    data.Resp() = CHI::Eb::Resps::CompData_I;
                    data.DataID() = beat * 2;
                    for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
                        data.Data()[index / 8] |= uint64_t(expected[beat * (Config256::dataWidth / 8) + index]) << ((index % 8) * 8);
                    oracle.Data(driver.driver.now, data);
                    LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
                }
                driver.driver.Tick();
                LineData observed{};
                for (size_t beat = 0; beat < 2; ++beat)
                {
                    const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
                    LUNATIC_CHECK(data.TgtID() == 4 && data.TxnID() == 1);
                    LUNATIC_CHECK(data.DataID() == beat * 2);
                    for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
                        observed[beat * (Config256::dataWidth / 8) + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
                }
                LUNATIC_CHECK(observed == expected);
                driver.Quiesce();
                LUNATIC_CHECK(!oracle.HasInflight());
            }
}
