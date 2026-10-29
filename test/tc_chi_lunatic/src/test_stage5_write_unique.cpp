#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData WritePattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 13);
        return data;
    }

    ByteEnables FullWriteEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    void RunWriteResponseMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        for (TransactionHandle form = 0; form < 3; ++form)
        {
            const LineKey line = LineKey::FromAddress(0x40000 + form * LINE_SIZE, false);
            const LineData expected = WritePattern(41 + form * 29);
            auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, form + 1, line.address, true);
            driver.PushRequest(request);
            DataBufferId dbid = 0;
            if (form == 0)
            {
                dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
                driver.WriteData(4, form + 1, dbid, expected, FullWriteEnables(),
                    CHI::Eb::Opcodes::DAT::NCBWrDataCompAck);
                LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == form + 1);
            }
            else if (form == 1)
            {
                LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == form + 1);
                dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
            }
            else
                dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp).DBID();
            if (form != 0)
                driver.WriteData(4, form + 1, dbid, expected, FullWriteEnables(),
                    CHI::Eb::Opcodes::DAT::NCBWrDataCompAck);
            driver.Quiesce();
            LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
        }
    }

    template<typename config>
    void RunPartialZeroAndCancel(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        const LineKey line = LineKey::FromAddress(0x41000, false);
        const LineData initial = WritePattern(19);
        const LineData partial = WritePattern(173);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 1, line.address));
        auto writeResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 1, writeResponse.DBID(), initial, FullWriteEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteUniquePtl, 4, 2, line.address));
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 2);
        DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        ByteEnables enables{};
        for (size_t index = 7; index < 23; ++index)
            enables[index] = 1;
        driver.WriteData(4, 2, dbid, partial, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        driver.Quiesce();
        LineData expected = initial;
        for (size_t index = 7; index < 23; ++index)
            expected[index] = partial[index];
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueZero, 4, 3, line.address));
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp).TxnID() == 3);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == LineData{});

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteUniquePtl, 4, 4, line.address));
        dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        typename CheckedDriver<config>::DAT cancel{};
        cancel.SrcID() = 4;
        cancel.TgtID() = 2;
        cancel.TxnID() = dbid;
        cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        driver.Oracle(4).WriteData(driver.driver.now, cancel);
        LUNATIC_CHECK(driver.driver.node.PushRXDAT(cancel));
        driver.driver.Tick();
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 4);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == LineData{});
    }

    template<typename config>
    void RunEvictOrEvict(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        const LineKey withData = LineKey::FromAddress(0x42000, false);
        const LineData dirty = WritePattern(87);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 1, withData.address));
        auto writeResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 1, writeResponse.DBID(), dirty, FullWriteEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(4, 2, withData.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(withData, 4) == CacheState::UD);

        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteEvictOrEvict, 4, 3, withData.address, true);
        driver.PushRequest(request);
        DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp).DBID();
        driver.WriteData(4, 3, dbid, dirty, FullWriteEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(withData, 4) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectHome(withData).data == dirty);

        const LineKey clean = LineKey::FromAddress(0x42100, false);
        driver.driver.node.GetMemory().WriteFull(clean, dirty);
        LUNATIC_CHECK(driver.ReadData(4, 4, clean.address, CHI::Eb::Opcodes::REQ::ReadShared) == dirty);
        request = driver.Request(CHI::Eb::Opcodes::REQ::WriteEvictOrEvict, 4, 5, clean.address, true);
        driver.PushRequest(request);
        if (policy == PolicyKind::Exclusive)
        {
            const DataBufferId cleanDbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp).DBID();
            driver.WriteData(4, 5, cleanDbid, dirty, FullWriteEnables(),
                CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UC);
        }
        else
        {
            auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(comp.Resp() == CHI::Eb::Resps::Comp_I);
            driver.CompAck(4, comp.DBID());
        }
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(clean, 4) == CacheState::I);

        const LineKey empty = LineKey::FromAddress(0x42200, false);
        request = driver.Request(CHI::Eb::Opcodes::REQ::WriteEvictOrEvict, 4, 6, empty.address, true);
        driver.PushRequest(request);
        const auto emptyComp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.CompAck(4, emptyComp.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.InspectHome(empty).valid);
    }

}

LUNATIC_TEST(Stage5WriteUniqueResponseForms128)
{
    RunWriteResponseMatrix<Config128>(PolicyKind::NINE);
    RunWriteResponseMatrix<Config128>(PolicyKind::Exclusive);
    RunWriteResponseMatrix<Config128>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5WriteUniqueResponseForms256)
{
    RunWriteResponseMatrix<Config256>(PolicyKind::NINE);
    RunWriteResponseMatrix<Config256>(PolicyKind::Exclusive);
    RunWriteResponseMatrix<Config256>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5WriteUniqueResponseForms512)
{
    RunWriteResponseMatrix<Config512>(PolicyKind::NINE);
    RunWriteResponseMatrix<Config512>(PolicyKind::Exclusive);
    RunWriteResponseMatrix<Config512>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5WriteUniquePartialZeroAndCancel)
{
    RunPartialZeroAndCancel<Config256>(PolicyKind::NINE);
    RunPartialZeroAndCancel<Config256>(PolicyKind::Exclusive);
    RunPartialZeroAndCancel<Config256>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5WriteEvictOrEvictAlternatives)
{
    RunEvictOrEvict<Config256>(PolicyKind::NINE);
    RunEvictOrEvict<Config256>(PolicyKind::Exclusive);
    RunEvictOrEvict<Config256>(PolicyKind::Inclusive);
}
