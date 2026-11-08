#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;
    using REQ = TestDriver::REQ;
    using RSP = TestDriver::RSP;
    using DAT = TestDriver::DAT;

    class RangeAddressMap final : public SystemAddressMap {
    public:
        NodeId Map(uint64_t address, bool) const noexcept override
        {
            return address < 0x40000 ? 6 : 9;
        }
    };

    Options BackendOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.addressMap = std::make_shared<RangeAddressMap>();
        return options;
    }

    RSP MakeResponse(NodeId source, uint16_t opcode, TransactionId txnId,
                     DataBufferId dbid = 0, uint8_t group = 0)
    {
        RSP response{};
        response.SrcID() = source;
        response.TgtID() = 2;
        response.TxnID() = txnId;
        response.DBID() = dbid;
        response.Opcode() = opcode;
        response.Resp() = opcode == CHI::Eb::Opcodes::RSP::Comp ? uint8_t(CHI::Eb::Resps::Comp_I) : uint8_t(0);
        if (group)
            response.PGroupID() = group;
        return response;
    }

    void SendResponse(TestDriver& driver, SNOracle<Config256>& oracle, const RSP& response)
    {
        oracle.Response(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

    void SendReadData(TestDriver& driver, SNOracle<Config256>& oracle, const REQ& downstream)
    {
        const NodeId source = downstream.TgtID();
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = source;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = downstream.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * 2;
            data.CCID() = (downstream.Addr() >> 4) & 3;
            data.BE() = ~uint64_t(0);
            oracle.Data(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        driver.driver.Tick();
    }

    void CompleteRead(TestDriver& driver, SNOracle<Config256>& oracle,
                      TransactionId txnId, uint64_t address, NodeId expected)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, txnId, address, false));
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(downstream.TgtID() == expected);
        oracle.Request(driver.driver.now, downstream);
        SendReadData(driver, oracle, downstream);
        for (size_t beat = 0; beat < 2; ++beat)
            driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    }

    void CompleteWrite(TestDriver& driver, SNOracle<Config256>& oracle,
                       TransactionId txnId, uint64_t address, NodeId expected)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, txnId, address));
        const auto requesterDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        ByteEnables enables{};
        enables.fill(1);
        driver.WriteData(4, txnId, requesterDBID.DBID(), LineData{}, enables,
            CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        LUNATIC_CHECK(downstream.TgtID() == expected);
        oracle.Request(driver.driver.now, downstream);

        SendResponse(driver, oracle,
            MakeResponse(expected, CHI::Eb::Opcodes::RSP::DBIDResp, downstream.TxnID(), 77));
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(data.TgtID() == expected && data.TxnID() == 77);
            oracle.WriteData(driver.driver.now, data);
        }
        SendResponse(driver, oracle,
            MakeResponse(expected, CHI::Eb::Opcodes::RSP::Comp, downstream.TxnID(), 77));
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    }

    void CompleteAtomic(TestDriver& driver, SNOracle<Config256>& oracle,
                        TransactionId txnId, uint64_t address, NodeId expected)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, txnId, address);
        request.Size() = 2;
        driver.PushRequest(request);
        const auto requesterDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);

        LineData operands{};
        operands[0] = 4;
        operands[1] = 3;
        operands[2] = 2;
        operands[3] = 1;
        ByteEnables enables{};
        for (size_t index = 0; index < 4; ++index)
            enables[index] = 1;
        driver.WriteData(4, txnId, requesterDBID.DBID(), operands, enables,
            CHI::Eb::Opcodes::DAT::NonCopyBackWrData, 0, 1);

        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::AtomicStore::ADD);
        LUNATIC_CHECK(downstream.TgtID() == expected);
        oracle.Request(driver.driver.now, downstream);
        SendResponse(driver, oracle,
            MakeResponse(expected, CHI::Eb::Opcodes::RSP::DBIDResp, downstream.TxnID(), 77));
        const auto data = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(data.TgtID() == expected && data.TxnID() == 77);
        oracle.WriteData(driver.driver.now, data);
        SendResponse(driver, oracle,
            MakeResponse(expected, CHI::Eb::Opcodes::RSP::Comp, downstream.TxnID(), 77));
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    }

    void CompletePersistence(TestDriver& driver, SNOracle<Config256>& oracle,
                             TransactionId txnId, uint64_t address, NodeId expected)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, txnId, address);
        request.PGroupID() = 0x68;
        request.Deep() = 1;
        driver.PushRequest(request);
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        LUNATIC_CHECK(downstream.TgtID() == expected && downstream.ReturnNID() == 2);
        oracle.Request(driver.driver.now, downstream);

        SendResponse(driver, oracle,
            MakeResponse(expected, CHI::Eb::Opcodes::RSP::Comp, downstream.TxnID(), 0, 0x68));
        SendResponse(driver, oracle,
            MakeResponse(expected, CHI::Eb::Opcodes::RSP::Persist, 0, 0, 0x68));
        const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.TgtID() == 4 && comp.TxnID() == txnId);
        const auto persist = driver.NextRSP(CHI::Eb::Opcodes::RSP::Persist);
        LUNATIC_CHECK(persist.TgtID() == 4 && persist.TxnID() == 0 && persist.PGroupID() == 0x68);
    }

}

LUNATIC_TEST(Stage9SAMFixedTargetAndRangeBoundaries)
{
    FixedSystemAddressMap fixed(6);
    LUNATIC_CHECK(fixed.Map(0, false) == 6);
    LUNATIC_CHECK(fixed.Map(0x3ffff, true) == 6);
    LUNATIC_CHECK(fixed.Map(0x40000, false) == 6);
    LUNATIC_CHECK(fixed.Map(UINT64_MAX, true) == 6);

    RangeAddressMap range;
    LUNATIC_CHECK(range.Map(0, false) == 6);
    LUNATIC_CHECK(range.Map(0x3ffff, false) == 6);
    LUNATIC_CHECK(range.Map(0x40000, false) == 9);
    LUNATIC_CHECK(range.Map(UINT64_MAX, true) == 9);

    Options options;
    options.subordinateId = 7;
    LUNATIC_CHECK(SelectSubordinate(options, 0x40000, false) == 7);
    options.addressMap = std::make_shared<FixedSystemAddressMap>(6);
    LUNATIC_CHECK(SelectSubordinate(options, 0x40000, false) == 6);
    options.addressMap = std::make_shared<RangeAddressMap>();
    LUNATIC_CHECK(SelectSubordinate(options, 0x3ffff, false) == 6);
    LUNATIC_CHECK(SelectSubordinate(options, 0x40000, false) == 9);
}

LUNATIC_TEST(Stage9SAMRoutesBackendFamilies)
{
    TestDriver driver(BackendOptions());
    SNOracle<Config256> oracleA(2, 6);
    SNOracle<Config256> oracleB(2, 9);

    CompleteRead(driver, oracleA, 1, 0x10000, 6);
    CompleteWrite(driver, oracleB, 2, 0x40000, 9);
    CompleteAtomic(driver, oracleA, 3, 0x10040, 6);
    CompletePersistence(driver, oracleB, 50, 0x40040, 9);

    driver.Quiesce();
    LUNATIC_CHECK(!oracleA.HasInflight());
    LUNATIC_CHECK(!oracleB.HasInflight());
}
