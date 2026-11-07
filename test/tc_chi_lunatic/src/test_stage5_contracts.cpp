#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage5ContractRequestClassification)
{
    namespace REQ = CHI::Eb::Opcodes::REQ;
    LUNATIC_CHECK(ClassifyRequest(REQ::ReadOnceCleanInvalid) == RequestClass::ReadOnce);
    LUNATIC_CHECK(ClassifyRequest(REQ::ReadOnceMakeInvalid) == RequestClass::ReadOnce);
    LUNATIC_CHECK(ClassifyRequest(REQ::ReadClean) == RequestClass::CoherentRead);
    LUNATIC_CHECK(ClassifyRequest(REQ::ReadNotSharedDirty) == RequestClass::CoherentRead);
    LUNATIC_CHECK(ClassifyRequest(REQ::ReadPreferUnique) == RequestClass::CoherentRead);
    LUNATIC_CHECK(ClassifyRequest(REQ::MakeReadUnique) == RequestClass::MakeReadUnique);
    LUNATIC_CHECK(ClassifyRequest(REQ::WriteUniqueFull) == RequestClass::ImmediateWrite);
    LUNATIC_CHECK(ClassifyRequest(REQ::WriteUniquePtlStash) == RequestClass::ImmediateWrite);
    LUNATIC_CHECK(ClassifyRequest(REQ::WriteUniqueZero) == RequestClass::WriteUniqueZero);
    LUNATIC_CHECK(ClassifyRequest(REQ::WriteEvictOrEvict) == RequestClass::WriteEvictOrEvict);
    LUNATIC_CHECK(ClassifyRequest(REQ::CleanUnique) == RequestClass::CleanUnique);
    LUNATIC_CHECK(ClassifyRequest(REQ::CleanSharedPersist) == RequestClass::Persistence);
    LUNATIC_CHECK(ClassifyRequest(REQ::CleanSharedPersistSep) == RequestClass::Persistence);
    LUNATIC_CHECK(ClassifyRequest(REQ::StashOnceUnique) == RequestClass::Stash);
    LUNATIC_CHECK(ClassifyRequest(REQ::StashOnceSepShared) == RequestClass::Stash);
    LUNATIC_CHECK(ClassifyRequest(REQ::AtomicStore::ADD) == RequestClass::Atomic);
    LUNATIC_CHECK(ClassifyRequest(REQ::AtomicLoad::UMIN) == RequestClass::Atomic);
    LUNATIC_CHECK(ClassifyRequest(REQ::AtomicSwap) == RequestClass::Atomic);
    LUNATIC_CHECK(ClassifyRequest(REQ::AtomicCompare) == RequestClass::Atomic);
    LUNATIC_CHECK(IsAtomicRequest(REQ::AtomicCompare));
    LUNATIC_CHECK(IsStashRequest(REQ::StashOnceShared));
    LUNATIC_CHECK(IsStashRequest(REQ::WriteUniqueFullStash));

    Request request;
    request.opcode = REQ::MakeReadUnique;
    LUNATIC_CHECK(RequiresCompAck(request));
    request.opcode = REQ::CleanUnique;
    LUNATIC_CHECK(RequiresCompAck(request));
    request.opcode = REQ::AtomicSwap;
    request.expCompAck = true;
    LUNATIC_CHECK(!RequiresCompAck(request));
}

LUNATIC_TEST(Stage5ContractRequestFieldsRoundTrip)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    HomeNode<Config256> node(2, options);

    HomeNode<Config256>::REQFlit request{};
    request.SrcID() = 4;
    request.TgtID() = 2;
    request.TxnID() = 9;
    request.ReturnNID() = 11;
    request.ReturnTxnID() = 0x1d;
    request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    request.Addr() = 0x12000;
    request.Size() = 6;
    request.AllowRetry() = 1;
    request.SnpAttr() = 0;
    request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    request.PGroupID() = 0x81;
    request.StashGroupID() = 0x81;
    request.StashNIDValid() = 1;
    request.StashLPIDValid() = 1;
    request.StashLPID() = 0x1d;
    request.Endian() = 1;
    request.Deep() = 1;
    request.LPID() = 0x15;
    request.Excl() = 1;
    LUNATIC_CHECK(node.PushRXREQ(request));

    std::optional<HomeNode<Config256>::REQFlit> downstream;
    for (size_t cycle = 1; cycle <= 64 && !downstream; ++cycle)
    {
        node.Tick(cycle);
        downstream = node.PopTXREQ();
    }
    LUNATIC_CHECK(downstream);
    LUNATIC_CHECK(downstream->ReturnNID() == 2);
    LUNATIC_CHECK(downstream->StashNID() == 2);
    LUNATIC_CHECK(downstream->ReturnTxnID() == downstream->TxnID());
    LUNATIC_CHECK(downstream->StashNIDValid() == 0);
    LUNATIC_CHECK(downstream->StashLPIDValid() == 0);
    LUNATIC_CHECK(downstream->StashLPID() == 0);
    LUNATIC_CHECK(downstream->Endian() == 0);
    LUNATIC_CHECK(downstream->Deep() == 0);
    LUNATIC_CHECK(downstream->DoDWT() == 0);
    LUNATIC_CHECK(downstream->LPID() == 0x15);
    LUNATIC_CHECK(downstream->Excl() == 1);
}

LUNATIC_TEST(Stage5ContractRetryExclAndLPIDCompatibility)
{
    Request original;
    original.source = 4;
    original.target = 2;
    original.txnId = 9;
    original.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    original.pGroupId = 0x15;
    original.snoopMe = true;
    Request resend = original;
    resend.txnId = 10;
    resend.allowRetry = false;
    LUNATIC_CHECK(RetryRequestCompatible(original, resend, 2));
    resend.snoopMe = false;
    LUNATIC_CHECK(!RetryRequestCompatible(original, resend, 2));
    resend.snoopMe = true;
    resend.pGroupId = 0x16;
    LUNATIC_CHECK(!RetryRequestCompatible(original, resend, 2));
}

LUNATIC_TEST(Stage5ContractWriteLPIDPropagation)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config256> driver(options);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 10, 0x12100);
    request.LPID() = 0x12;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 10, dbid, LineData{}, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXREQ()); });
    const auto downstream = driver.node.PopTXREQ();
    LUNATIC_CHECK(downstream);
    LUNATIC_CHECK(downstream->Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(downstream->LPID() == 0x12);
    LUNATIC_CHECK(downstream->Excl() == 0);
}
