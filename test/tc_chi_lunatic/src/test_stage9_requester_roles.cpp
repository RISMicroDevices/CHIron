#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;

    LineData RolePattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x6b + index * 17);
        return data;
    }

    ByteEnables RoleEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void SeedDirtyProvider(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, RoleEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage9RequesterRoleMapDefaultsAndOverrides)
{
    Options options;
    LUNATIC_CHECK(GetRequesterRole(options, 4) == RequesterRole::RN_F);
    LUNATIC_CHECK(IsSnoopableRequester(options, 4));

    options.requesterRoles[4] = RequesterRole::RN_D;
    options.requesterRoles[5] = RequesterRole::RN_I;
    options.requesterRoles[6] = RequesterRole::RN_F;
    LUNATIC_CHECK(GetRequesterRole(options, 4) == RequesterRole::RN_D);
    LUNATIC_CHECK(GetRequesterRole(options, 5) == RequesterRole::RN_I);
    LUNATIC_CHECK(GetRequesterRole(options, 6) == RequesterRole::RN_F);
    LUNATIC_CHECK(GetRequesterRole(options, 7) == RequesterRole::RN_F);
    LUNATIC_CHECK(!IsSnoopableRequester(options, 4));
    LUNATIC_CHECK(!IsSnoopableRequester(options, 5));
    LUNATIC_CHECK(IsSnoopableRequester(options, 6));
    LUNATIC_CHECK(IsSnoopableRequester(options, 7));
}

LUNATIC_TEST(Stage9RequesterRolesImpreciseSnoopEligibility)
{
    for (const RequesterRole role : { RequesterRole::RN_D, RequesterRole::RN_I })
    {
        Options options;
        options.preciseSnoopFilter = false;
        options.requesterRoles[8] = role;
        TestDriver driver(options);
        const LineKey line = LineKey::FromAddress(0x76000, false);
        const LineData expected = RolePattern();
        SeedDirtyProvider(driver, line, expected);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 8, 10, line.address));
        const auto writeSnoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(writeSnoop.target == 6);
        driver.SnoopData(6, writeSnoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_I_PD);
        const auto writeResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        LUNATIC_CHECK(writeResponse.TgtID() == 8 && writeResponse.TxnID() == 10);
        driver.WriteData(8, 10, writeResponse.DBID(), expected, RoleEnables(),
            CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 8) == CacheState::I);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 4, 11, line.address, true));
        const auto invalidate = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(invalidate.target == 6);
        driver.SnoopResponse(6, invalidate.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.TgtID() == 4 && comp.TxnID() == 11);
        driver.CompAck(4, comp.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
    }
}

LUNATIC_TEST(Stage9RequesterRolesRejectNonSnoopableStashTarget)
{
    for (const RequesterRole role : { RequesterRole::RN_D, RequesterRole::RN_I })
    {
        Options options;
        options.requesterRoles[8] = role;
        Driver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x76100, false);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, 4, 20, line.address);
        request.ReturnNID() = 8;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.Until([&] { return driver.node.PeekTXRSP() || driver.node.PeekTXSNP(); });
        LUNATIC_CHECK(!driver.node.PeekTXSNP());

        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        driver.WriteData(4, 20, dbid, RolePattern(), RoleEnables(), CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        LUNATIC_CHECK(!driver.node.PeekTXSNP());
        LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
    }
}
