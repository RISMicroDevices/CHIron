#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    typename CheckedDriver<config>::REQ OrderedStashRequest(CheckedDriver<config>& driver, uint16_t opcode,
        TransactionId txnId, uint64_t address, uint8_t order, bool compAck)
    {
        auto request = driver.Request(opcode, 4, txnId, address, compAck);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        request.Order() = order;
        return request;
    }

    template<typename config>
    void RespondOrderedStashSnoop(CheckedDriver<config>& driver, bool full)
    {
        const uint16_t opcode = full ? CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash
            : CHI::Eb::Opcodes::SNP::SnpUniqueStash;
        driver.driver.Until([&] {
            return static_cast<bool>(driver.driver.node.PeekTXSNP() || driver.driver.node.PeekTXRSP());
        });
        LUNATIC_CHECK(driver.driver.node.PeekTXSNP());
        const auto snoop = driver.NextSNP(opcode);
        LUNATIC_CHECK(snoop.target == 5);
        LUNATIC_CHECK(snoop.flit.StashLPIDValid() == 1);
        LUNATIC_CHECK(snoop.flit.StashLPID() == 3);
        driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    }

    LineData OrderedStashData(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 3);
        return data;
    }

    ByteEnables OrderedStashEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    void RunOrderedStashOWO(uint16_t opcode, size_t ackForm)
    {
        Options options;
        options.outputQueueLimit = 1;
        CheckedDriver<config> driver(options);
        const bool full = opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash;
        const LineKey line = LineKey::FromAddress(0x5e000, false);
        const LineData data = OrderedStashData(41);
        driver.PushRequest(OrderedStashRequest(driver, opcode, 1, line.address, 2, true));
        RespondOrderedStashSnoop(driver, full);
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
        if (ackForm == 0)
            driver.CompAck(4, dbid.DBID());
        const uint16_t dataOpcode = ackForm == 1 ? CHI::Eb::Opcodes::DAT::NCBWrDataCompAck
            : CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        driver.WriteData(4, 1, dbid.DBID(), data, OrderedStashEnables(), dataOpcode);
        for (size_t cycle = 0; cycle < 8; ++cycle)
            driver.driver.Tick();
        if (ackForm == 2)
        {
            LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
            LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());
            driver.CompAck(4, dbid.DBID());
        }
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == data);
    }

    template<typename config>
    void RunOrderedStashOWOMatrix()
    {
        for (uint16_t opcode : { CHI::Eb::Opcodes::REQ::WriteUniquePtlStash,
                 CHI::Eb::Opcodes::REQ::WriteUniqueFullStash })
            for (size_t ackForm = 0; ackForm < 3; ++ackForm)
                RunOrderedStashOWO<config>(opcode, ackForm);
    }

    void RunOrderedStashWithoutCompAck(uint16_t opcode, uint8_t order)
    {
        CheckedDriver<Config256> driver;
        const bool full = opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash;
        const LineKey line = LineKey::FromAddress(0x5e100 + order * 0x40, false);
        const LineData data = OrderedStashData(73);
        driver.PushRequest(OrderedStashRequest(driver, opcode, 1, line.address, order, false));
        RespondOrderedStashSnoop(driver, full);
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
        driver.WriteData(4, 1, dbid.DBID(), data, OrderedStashEnables(),
            CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == data);
    }

    void RunOrderedStashSerialization(uint16_t opcode)
    {
        Options options;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        const bool full = opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash;
        const LineKey line = LineKey::FromAddress(0x5e200, false);
        const LineData firstData = OrderedStashData(17);
        const LineData secondData = OrderedStashData(139);
        driver.PushRequest(OrderedStashRequest(driver, opcode, 1, line.address, 2, true));
        RespondOrderedStashSnoop(driver, full);
        const auto first = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
        driver.WriteData(4, 1, first.DBID(), firstData, OrderedStashEnables(),
            CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        driver.PushRequest(OrderedStashRequest(driver, opcode, 2, line.address, 2, true));
        for (size_t cycle = 0; cycle < 8; ++cycle)
            driver.driver.Tick();
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());
        LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());

        driver.CompAck(4, first.DBID());
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
        RespondOrderedStashSnoop(driver, full);
        const auto second = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
        LUNATIC_CHECK(second.TxnID() == 2);
        driver.WriteData(4, 2, second.DBID(), secondData, OrderedStashEnables(),
            CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        driver.CompAck(4, second.DBID());
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 2);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == secondData);
    }

    void RunOrderedStashCancellation(uint16_t opcode)
    {
        CheckedDriver<Config256> driver;
        const bool full = opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash;
        const LineKey line = LineKey::FromAddress(full ? 0x5e340 : 0x5e300, false);
        driver.PushRequest(OrderedStashRequest(driver, opcode, 1, line.address, 2, true));
        RespondOrderedStashSnoop(driver, full);
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd);
        driver.WriteData(4, 1, dbid.DBID(), LineData{}, ByteEnables{},
            CHI::Eb::Opcodes::DAT::WriteDataCancel);
        for (size_t cycle = 0; cycle < 8; ++cycle)
            driver.driver.Tick();
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());
        driver.CompAck(4, dbid.DBID());
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
    }

}

LUNATIC_TEST(Stage9OrderedStashOWO128)
{
    RunOrderedStashOWOMatrix<Config128>();
}

LUNATIC_TEST(Stage9OrderedStashOWO256)
{
    RunOrderedStashOWOMatrix<Config256>();
}

LUNATIC_TEST(Stage9OrderedStashOWO512)
{
    RunOrderedStashOWOMatrix<Config512>();
}

LUNATIC_TEST(Stage9OrderedStashWithoutCompAckAndSerialization)
{
    for (uint16_t opcode : { CHI::Eb::Opcodes::REQ::WriteUniquePtlStash,
             CHI::Eb::Opcodes::REQ::WriteUniqueFullStash })
    {
        RunOrderedStashWithoutCompAck(opcode, 2);
        RunOrderedStashSerialization(opcode);
    }
}

LUNATIC_TEST(Stage9OrderedStashCancellation)
{
    RunOrderedStashCancellation(CHI::Eb::Opcodes::REQ::WriteUniquePtlStash);
}

LUNATIC_TEST(Stage9OrderedStashRejectsReservedControls)
{
    for (uint16_t opcode : { CHI::Eb::Opcodes::REQ::WriteUniquePtlStash,
             CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, CHI::Eb::Opcodes::REQ::StashOnceShared })
    {
        Driver<Config256> driver;
        auto request = driver.Request(opcode, 4, 1, 0x5e400);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        request.Order() = opcode == CHI::Eb::Opcodes::REQ::StashOnceShared ? 2 : 1;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(response.RespErr() == (opcode == CHI::Eb::Opcodes::REQ::StashOnceShared
            ? CHI::Eb::RespErrs::NDERR : CHI::Eb::RespErrs::DERR));
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
}
