#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using DAT = TestDriver::DAT;

    enum class TagMutation {
        Opcode,
        Value,
        Update
    };

    LineData TagPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x61 + index * 5);
        return data;
    }

    ByteEnables FullTagEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void SeedDirtyProvider(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, FullTagEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

    DAT MakeWriteData(uint16_t opcode, DataBufferId dbid, size_t beat, const LineData& line, TagMutation mutation)
    {
        DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = dbid;
        data.Opcode() = opcode;
        data.Resp() = opcode == CHI::Eb::Opcodes::DAT::CopyBackWrData
            ? uint8_t(CHI::Eb::Resps::CopyBackWrData_UD_PD) : uint8_t(0);
        data.DataID() = beat * 2;
        data.CCID() = 0;
        if (opcode != CHI::Eb::Opcodes::DAT::WriteDataCancel)
        {
            data.BE() = ~uint64_t(0);
            for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
                data.Data()[index / 8] |= uint64_t(line[beat * (Config256::dataWidth / 8) + index]) << ((index % 8) * 8);
        }
        if (mutation == TagMutation::Opcode)
            data.TagOp() = CHI::Eb::TagOp::Update;
        else if (mutation == TagMutation::Value)
            data.Tag() = 1;
        else
            data.TU() = 1;
        return data;
    }

    void CheckWriteContainment(uint16_t requestOpcode, uint16_t dataOpcode, TagMutation mutation)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x79000, false);
        const LineData expected = TagPattern();
        auto request = driver.Request(requestOpcode, 4, 10, line.address);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(requestOpcode == CHI::Eb::Opcodes::REQ::WriteBackFull
            ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(!driver.node.PushRXDAT(MakeWriteData(dataOpcode, response.DBID(), 0, expected, mutation)));
        LUNATIC_CHECK(driver.node.GetMemory().Read(line) == LineData{});

        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = MakeWriteData(dataOpcode, response.DBID(), beat, expected, TagMutation::Opcode);
            data.TagOp() = CHI::Eb::TagOp::Invalid;
            data.Tag() = 0;
            data.TU() = 0;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        if (dataOpcode != CHI::Eb::Opcodes::DAT::CopyBackWrData)
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
        LUNATIC_CHECK(driver.ReadData(4, 20, line.address, CHI::Eb::Opcodes::REQ::ReadNoSnp) ==
            (dataOpcode == CHI::Eb::Opcodes::DAT::WriteDataCancel ? LineData{} : expected));
    }

    DAT MakeSnoopData(const TestDriver::SNP& snoop, uint16_t opcode, size_t beat,
                      const LineData& line, TagMutation mutation)
    {
        DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.TxnID() = snoop.flit.TxnID();
        data.Opcode() = opcode;
        data.Resp() = opcode == CHI::Eb::Opcodes::DAT::SnpRespDataPtl
            ? CHI::Eb::Resps::SnpRespDataPtl_I_PD : CHI::Eb::Resps::SnpRespData_I_PD;
        data.DataID() = beat * 2;
        data.CCID() = 0;
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t index = 0; index < bytes; ++index)
        {
            if (opcode == CHI::Eb::Opcodes::DAT::SnpRespData || index < bytes / 2)
            {
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
                data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
            }
        }
        if (mutation == TagMutation::Opcode)
            data.TagOp() = CHI::Eb::TagOp::Update;
        else if (mutation == TagMutation::Value)
            data.Tag() = 1;
        else
            data.TU() = 1;
        return data;
    }

    void CheckSnoopContainment(uint16_t opcode, TagMutation mutation)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x79040, false);
        const LineData expected = TagPattern();
        if (opcode == CHI::Eb::Opcodes::DAT::SnpRespDataPtl)
        {
            LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 6, 90, line.address)));
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
            ByteEnables partial{};
            LineData partialExpected{};
            for (size_t index = 0; index < partial.size(); ++index)
            {
                if (index % 32 < 16)
                {
                    partial[index] = 1;
                    partialExpected[index] = expected[index];
                }
            }
            driver.WriteData(6, 90, response.DBID(), expected, partial, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                CHI::Eb::Resps::CopyBackWrData_UD_PD);
            driver.Quiesce();
            LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == partialExpected);
            LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UDP);
        }
        else
            SeedDirtyProvider(driver, line, expected);

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 10, line.address, true)));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(!driver.node.PushRXDAT(MakeSnoopData(snoop, opcode, 0, expected, mutation)));

        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = MakeSnoopData(snoop, opcode, beat, expected, TagMutation::Opcode);
            data.TagOp() = CHI::Eb::TagOp::Invalid;
            data.Tag() = 0;
            data.TU() = 0;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
            dbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
        driver.CompAck(4, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    }

}

LUNATIC_TEST(Stage9MTEContainmentRejectsWriteTagPayloads)
{
    CheckWriteContainment(CHI::Eb::Opcodes::REQ::WriteNoSnpFull,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData, TagMutation::Opcode);
    CheckWriteContainment(CHI::Eb::Opcodes::REQ::WriteNoSnpFull,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData, TagMutation::Value);
    CheckWriteContainment(CHI::Eb::Opcodes::REQ::WriteNoSnpFull,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData, TagMutation::Update);
    CheckWriteContainment(CHI::Eb::Opcodes::REQ::WriteBackFull,
        CHI::Eb::Opcodes::DAT::CopyBackWrData, TagMutation::Opcode);
    CheckWriteContainment(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl,
        CHI::Eb::Opcodes::DAT::WriteDataCancel, TagMutation::Opcode);
}

LUNATIC_TEST(Stage9MTEContainmentRejectsSnoopTagPayloads)
{
    CheckSnoopContainment(CHI::Eb::Opcodes::DAT::SnpRespData, TagMutation::Opcode);
    CheckSnoopContainment(CHI::Eb::Opcodes::DAT::SnpRespData, TagMutation::Value);
    CheckSnoopContainment(CHI::Eb::Opcodes::DAT::SnpRespData, TagMutation::Update);
    CheckSnoopContainment(CHI::Eb::Opcodes::DAT::SnpRespDataPtl, TagMutation::Opcode);
}
