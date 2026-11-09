#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage6DWTDirectDBIDDataAndComp)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.dwtEnabled = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);
    driver.Oracle(4).RegisterSubordinate(6);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, 0, 0x10000, false));

    const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(downstream.DoDWT() == 1);
    LUNATIC_CHECK(downstream.ReturnNID() == 4);
    LUNATIC_CHECK(downstream.ReturnTxnID() == 0);
    oracle.Request(driver.driver.now, downstream);

    typename CheckedDriver<Config256>::RSP dbidResponse{};
    dbidResponse.SrcID() = 6;
    dbidResponse.TgtID() = 4;
    dbidResponse.TxnID() = 0;
    dbidResponse.DBID() = 77;
    dbidResponse.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
    oracle.Response(driver.driver.now, dbidResponse);
    driver.Oracle(4).Response(driver.driver.now, dbidResponse);

    constexpr size_t bytes = Config256::dataWidth / 8;
    LineData expected{};
    LineData actual{};
    for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
    {
        typename CheckedDriver<Config256>::DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 6;
        data.HomeNID() = 0;
        data.TxnID() = 77;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = beat * bytes / 16;
        data.CCID() = 0;
        data.BE() = ~uint64_t(0);
        const size_t offset = beat * bytes;
        for (size_t index = 0; index < bytes; ++index)
        {
            const uint8_t value = uint8_t(0x40 + offset + index);
            expected[offset + index] = value;
            data.Data()[index / 8] |= uint64_t(value) << ((index % 8) * 8);
        }
        oracle.WriteData(driver.driver.now, data);
        driver.Oracle(4).WriteData(driver.driver.now, data);
        for (size_t index = 0; index < bytes; ++index)
            actual[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    LUNATIC_CHECK(actual == expected);

    typename CheckedDriver<Config256>::RSP comp{};
    comp.SrcID() = 6;
    comp.TgtID() = 2;
    comp.TxnID() = downstream.TxnID();
    comp.DBID() = 77;
    comp.Resp() = CHI::Eb::Resps::Comp_I;
    comp.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    oracle.Response(driver.driver.now, comp);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(comp));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
}

LUNATIC_TEST(Stage6DirectPersistRoutesOriginalRequester)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.directPersistEnabled = true;
    CheckedDriver<Config256> driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);
    driver.Oracle(4).RegisterSubordinate(6);

    for (uint16_t txnId = 1; txnId <= 2; ++txnId)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, txnId, 0x10000, false);
        request.PGroupID() = 0x20 + txnId;
        driver.PushRequest(request);
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        LUNATIC_CHECK(downstream.ReturnNID() == 4);
        LUNATIC_CHECK(downstream.ReturnTxnID() == 0);
        LUNATIC_CHECK(downstream.PGroupID() == 0x20 + txnId);
        oracle.Request(driver.driver.now, downstream);

        typename CheckedDriver<Config256>::RSP comp{};
        comp.SrcID() = 6;
        comp.TgtID() = 2;
        comp.TxnID() = downstream.TxnID();
        comp.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
        comp.Resp() = CHI::Eb::Resps::Comp_UC;
        oracle.Response(driver.driver.now, comp);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(comp));
        const auto homeComp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(homeComp.TgtID() == 4);
        LUNATIC_CHECK(homeComp.TxnID() == txnId);

        typename CheckedDriver<Config256>::RSP persist{};
        persist.SrcID() = 6;
        persist.TgtID() = 4;
        persist.TxnID() = 0;
        persist.Opcode() = CHI::Eb::Opcodes::RSP::Persist;
        persist.PGroupID() = 0x20 + txnId;
        oracle.Response(driver.driver.now, persist);
        driver.Oracle(4).Response(driver.driver.now, persist);
        driver.Quiesce();
    }
}
