#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData StashLPIDPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(29 + index * 7);
        return data;
    }

    ByteEnables StashLPIDEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    typename CheckedDriver<config>::REQ StashLPIDRequest(CheckedDriver<config>& driver, uint16_t opcode,
                                                         TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        return request;
    }

    template<typename config>
    void ReceiveStashLPIDRead(CheckedDriver<config>& driver, NodeId source, TransactionId txnId)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        DataBufferId dataBufferId = 0;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source);
            LUNATIC_CHECK(data.TxnID() == txnId);
            dataBufferId = data.DBID();
        }
        driver.CompAck(source, dataBufferId);
    }

    template<typename config>
    void SeedStashLPIDPair(CheckedDriver<config>& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
        const auto write = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, write.DBID(), data, StashLPIDEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(5, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 6, 92, line.address, true));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        LUNATIC_CHECK(snoop.target == 5);
        driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_SC_PD);
        ReceiveStashLPIDRead(driver, 6, 92);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::SC);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SD);
    }

    template<typename config>
    void CompleteStashLPIDWrite(CheckedDriver<config>& driver, TransactionId txnId,
                                const LineData& data, const ByteEnables& enables)
    {
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const uint16_t firstOpcode = driver.driver.node.PeekTXRSP()->Opcode();
        if (firstOpcode == CHI::Eb::Opcodes::RSP::Comp)
        {
            LUNATIC_CHECK(driver.NextRSP(firstOpcode).TxnID() == txnId);
            const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
            driver.WriteData(4, txnId, dbid, data, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        }
        else if (firstOpcode == CHI::Eb::Opcodes::RSP::DBIDResp)
        {
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            driver.WriteData(4, txnId, dbid, data, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == txnId);
        }
        else
        {
            LUNATIC_CHECK(firstOpcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            driver.WriteData(4, txnId, dbid, data, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        }
    }

    void RunStashLPIDIsolation(bool full)
    {
        Options options;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(full ? 0x5a100 : 0x5a000, false);
        const LineData initial = StashLPIDPattern();
        LineData update{};
        for (size_t index = 0; index < update.size(); ++index)
            update[index] = initial[index] ^ 0xa5;
        SeedStashLPIDPair(driver, line, initial);

        const uint16_t opcode = full ? CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
            : CHI::Eb::Opcodes::REQ::WriteUniquePtlStash;
        driver.PushRequest(StashLPIDRequest(driver, opcode, 100, line.address));
        const uint16_t stashOpcode = full ? CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash
            : CHI::Eb::Opcodes::SNP::SnpUniqueStash;
        const auto selected = driver.NextSNP(stashOpcode);
        LUNATIC_CHECK(selected.target == 5);
        LUNATIC_CHECK(selected.flit.StashLPIDValid() == 1);
        LUNATIC_CHECK(selected.flit.StashLPID() == 3);
        const auto ordinary = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(ordinary.target == 6);
        LUNATIC_CHECK(ordinary.flit.StashLPIDValid() == 0);
        LUNATIC_CHECK(ordinary.flit.StashLPID() == 0);

        driver.SnoopResponse(5, selected.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        driver.SnoopData(6, ordinary.flit.TxnID(), initial, CHI::Eb::Resps::SnpRespData_I_PD);
        ByteEnables enables{};
        for (size_t index = 0; index < enables.size(); ++index)
            enables[index] = full || (index >= 11 && index < 47);
        CompleteStashLPIDWrite(driver, 100, update, enables);
        driver.Quiesce();

        LineData expected = initial;
        for (size_t index = 0; index < expected.size(); ++index)
            if (enables[index])
                expected[index] = update[index];
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    }

}

LUNATIC_TEST(Stage9WriteStashLPIDIsolationPtl)
{
    RunStashLPIDIsolation(false);
}

LUNATIC_TEST(Stage9WriteStashLPIDIsolationFull)
{
    RunStashLPIDIsolation(true);
}
