#include "common.hpp"
#include "chi/icn/lunatic/chi_lunatic_pipeline.hpp"
#include "chi/icn/lunatic/chi_lunatic_protocol.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage2PipelineEligibilityAndLatency)
{
    Options                         options;
    options.tagLatency = 2;
    options.dataLatency = 3;
    VirtualPipeline                 pipeline(options);
    Request                         request;
    request.opcode = CHI::Eb::Opcodes::REQ::ReadShared;
    Transaction                     transaction(1, request, 0);

    LUNATIC_CHECK(pipeline.Eligible(transaction, 0));
    LUNATIC_CHECK(pipeline.Advance(transaction, Phase::Lookup, 1) == 3);
    LUNATIC_CHECK(!pipeline.Eligible(transaction, 2));
    LUNATIC_CHECK(pipeline.Eligible(transaction, 3));
    LUNATIC_CHECK(pipeline.Advance(transaction, Phase::Action, 3) == 6);
    LUNATIC_CHECK(pipeline.Advance(transaction, Phase::Commit, 6) == 7);
}

LUNATIC_TEST(Stage2PipelinePortContention)
{
    Options                         options;
    options.tagLatency = 2;
    options.dataLatency = 2;
    VirtualPipeline                 pipeline(options);
    LUNATIC_CHECK(pipeline.ScheduleTag(0) == 2);
    LUNATIC_CHECK(pipeline.ScheduleTag(0) == 3);
    LUNATIC_CHECK(pipeline.ScheduleTag(5) == 7);
    LUNATIC_CHECK(pipeline.ScheduleData(4) == 6);
    LUNATIC_CHECK(pipeline.ScheduleData(4) == 7);
    pipeline.Reset();
    LUNATIC_CHECK(pipeline.ScheduleTag(0) == 2);
}

LUNATIC_TEST(Stage2ProtocolClassification)
{
    namespace REQ = CHI::Eb::Opcodes::REQ;
    LUNATIC_CHECK(ClassifyRequest(REQ::ReadNoSnp) == RequestClass::NonCoherentRead);
    LUNATIC_CHECK(ClassifyRequest(REQ::ReadOnce) == RequestClass::ReadOnce);
    LUNATIC_CHECK(IsCoherentRequest(REQ::ReadUnique));
    LUNATIC_CHECK(IsCopybackRequest(REQ::WriteBackPtl));
    LUNATIC_CHECK(IsCMORequest(REQ::MakeInvalid));
    LUNATIC_CHECK(!IsSupportedRequest(REQ::AtomicSwap));

    Request                         request;
    request.opcode = REQ::ReadShared;
    LUNATIC_CHECK(RequiresCompAck(request));
    request.opcode = REQ::ReadNoSnp;
    LUNATIC_CHECK(!RequiresCompAck(request));
    request.expCompAck = true;
    LUNATIC_CHECK(RequiresCompAck(request));
    request.opcode = REQ::WriteNoSnpZero;
    LUNATIC_CHECK(!RequiresCompAck(request));
}
