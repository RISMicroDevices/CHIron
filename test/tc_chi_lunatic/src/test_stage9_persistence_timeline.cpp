#include "oracle_driver.hpp"

#include <map>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;
    using REQ = TestDriver::REQ;
    using RSP = TestDriver::RSP;

    LineData TimelinePattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 29 + index / 3);
        return data;
    }

    void SeedDirtyRequester(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        ByteEnables enables{};
        enables.fill(1);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 9, 90, line.address));
        const auto write = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(9, 90, write.DBID(), data, enables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address,
            CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        driver.Quiesce();
    }

    void SendSubordinateResponse(TestDriver& driver, SNOracle<Config256>& oracle,
        const REQ& request, uint16_t opcode)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        if (opcode == CHI::Eb::Opcodes::RSP::CompPersist || opcode == CHI::Eb::Opcodes::RSP::Persist)
            response.PGroupID() = request.PGroupID();
        oracle.Response(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

}

LUNATIC_TEST(Stage9ReferencePersistenceDependencyTimeline)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.subordinatePersistSepEnabled = true;
    TestDriver driver(options);
    SNOracle<Config256> subordinate(2, 6);
    const LineKey line = LineKey::FromAddress(0xb6000, false);
    const LineData initial = TimelinePattern(19);
    const LineData dirty = TimelinePattern(173);
    driver.driver.node.GetMemory().WriteFull(line, initial);
    SeedDirtyRequester(driver, line, dirty);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
    LUNATIC_CHECK(!subordinate.HasInflight());

    auto persist = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 50, line.address);
    persist.PGroupID() = 0x73;
    persist.Deep() = 1;
    driver.PushRequest(persist);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
    LUNATIC_CHECK(snoop.target == 6);
    driver.SnoopData(6, snoop.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_I_PD);

    const auto write = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    subordinate.Request(driver.driver.now, write);
    LUNATIC_CHECK(!driver.driver.node.PeekTXREQ());
    LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());
    SendSubordinateResponse(driver, subordinate, write, CHI::Eb::Opcodes::RSP::DBIDResp);

    LineData backendData = initial;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        subordinate.WriteData(driver.driver.now, data);
        if (data.TgtID() != 6 || data.TxnID() != 77)
            throw std::runtime_error("persistence write-data identifiers: source=" +
                std::to_string(uint16_t(data.SrcID())) + " target=" + std::to_string(uint16_t(data.TgtID())) +
                " txn=" + std::to_string(uint16_t(data.TxnID())) + " dbid=" +
                std::to_string(uint16_t(data.DBID())) + " data-id=" + std::to_string(uint16_t(data.DataID())) +
                " be=" + std::to_string(uint64_t(data.BE())) + " request-txn=" +
                std::to_string(uint16_t(write.TxnID())));
        LUNATIC_CHECK(data.HomeNID() == 0 && data.DataID() == beat * 2);
        LUNATIC_CHECK(uint64_t(data.BE()) == 0xffffffff);
        for (size_t index = 0; index < 32; ++index)
            backendData[beat * 32 + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    LUNATIC_CHECK(backendData == dirty);
    LUNATIC_CHECK(!driver.driver.node.PeekTXREQ());
    LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());

    SendSubordinateResponse(driver, subordinate, write, CHI::Eb::Opcodes::RSP::Comp);
    const auto downstreamPersist = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    subordinate.Request(driver.driver.now, downstreamPersist);
    LUNATIC_CHECK(downstreamPersist.PGroupID() == 0x73);
    LUNATIC_CHECK(downstreamPersist.Deep() == 1);
    LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());

    SendSubordinateResponse(driver, subordinate, downstreamPersist, CHI::Eb::Opcodes::RSP::CompPersist);
    const auto completion = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
    LUNATIC_CHECK(completion.TgtID() == 4 && completion.TxnID() == 50);
    LUNATIC_CHECK(completion.PGroupID() == 0x73);
    LUNATIC_CHECK(completion.RespErr() == CHI::Eb::RespErrs::OK);
    driver.Quiesce();
    LUNATIC_CHECK(!subordinate.HasInflight());
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).valid);
    LUNATIC_CHECK(!driver.driver.node.InspectHome(line).dirty);
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == dirty);
    LUNATIC_CHECK(backendData == dirty);
}
