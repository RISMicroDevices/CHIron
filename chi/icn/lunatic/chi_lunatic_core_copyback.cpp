#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {
namespace {

bool IsDirtyCopyback(uint8_t resp)
{
    return resp == CHI::Eb::Resps::CopyBackWrData_UD_PD || resp == CHI::Eb::Resps::CopyBackWrData_SD_PD;
}

ByteEnables FullEnables()
{
    ByteEnables enables{};
    enables.fill(1);
    return enables;
}

}

bool Core::Impl::PrepareHomeCommit(Transaction& transaction)
{
    switch (ClassifyRequest(transaction.request.opcode))
    {
        case RequestClass::ReadOnce:
        case RequestClass::CoherentRead:
        case RequestClass::MakeUnique:
            return CommitReadDirectory(transaction);
        case RequestClass::CopybackWrite:
            return CommitCopyback(transaction);
        case RequestClass::CMO:
            return CommitCMO(transaction);
        default:
            return true;
    }
}

bool Core::Impl::CommitReadDirectory(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    const LineKey line = transaction.request.Line();
    PolicyContext context;
    context.line = line;
    context.requester = transaction.request.source;
    context.homeValid = transaction.homeHit;
    context.homeDirty = flow.homeDirty;
    context.requesterValid = flow.requesterWasValid;
    context.requesterDirty = flow.requesterWasDirty;
    const PolicyDecision decision = ClassifyRequest(transaction.request.opcode) == RequestClass::ReadOnce
        ? policy->OnHomeAllocation(context) : policy->OnRNFill(context);
    flow.allocateHome = decision.allocateHome;
    flow.retainHome = decision.retainHome;
    flow.removeHome = decision.removeHome;

    size_t otherSharers = 0;
    for (NodeId target : snoopFilter.Targets(line))
    {
        if (target != transaction.request.source && IsValid(snoopFilter.State(line, target)))
            ++otherSharers;
    }

    switch (ClassifyRequest(transaction.request.opcode))
    {
        case RequestClass::CoherentRead:
            if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::ReadUnique)
                flow.requesterFinal = flow.dataDirty || flow.passDirty || flow.homeDirty ? CacheState::UD : CacheState::UC;
            else if (flow.passDirty)
                flow.requesterFinal = otherSharers ? CacheState::SD : CacheState::UD;
            else
                flow.requesterFinal = CacheState::SC;
            break;
        case RequestClass::MakeUnique:
            flow.requesterFinal = flow.dataDirty || flow.passDirty || flow.homeDirty ? CacheState::UD : CacheState::UC;
            break;
        case RequestClass::ReadOnce:
            flow.requesterFinal = CacheState::I;
            break;
        default:
            return false;
    }

    if (decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
        return false;

    if (decision.allocateHome || decision.retainHome)
    {
        const std::optional<size_t> currentWay = directory.FindWay(line);
        const size_t way = currentWay ? *currentWay : flow.victimWay;
        const bool requesterDirty = IsDirty(flow.requesterFinal);
        const bool dirty = requesterDirty ? false : flow.homeDirty || (flow.dataDirty && !flow.passDirty);
        InstallHome(line, way, transaction.data, dirty, time);
    }
    else if (decision.removeHome && transaction.homeHit)
    {
        directory.Invalidate(directory.SetIndex(line), *directory.FindWay(line));
    }

    if (ClassifyRequest(transaction.request.opcode) != RequestClass::ReadOnce)
        snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
    return true;
}

bool Core::Impl::CommitCopyback(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    const LineKey line = transaction.request.Line();
    const bool dirty = IsDirtyCopyback(flow.writeResp);

    if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteBackPtl && !transaction.homeHit)
    {
        const LineData base = memory.Read(line);
        for (size_t index = 0; index < LINE_SIZE; ++index)
        {
            if (!transaction.byteEnables[index])
                transaction.data[index] = base[index];
        }
    }

    PolicyContext context;
    context.line = line;
    context.requester = transaction.request.source;
    context.homeValid = transaction.homeHit;
    context.homeDirty = flow.homeDirty;
    context.requesterValid = flow.requesterWasValid;
    context.requesterDirty = dirty;
    context.evictionHasData = true;
    const PolicyDecision decision = policy->OnCopyback(context);
    if (decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
        return false;

    if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::WriteCleanFull)
    {
        const uint8_t state = flow.writeResp & 0b011;
        flow.requesterFinal = state == 0b000 ? CacheState::I : state == 0b001 ? CacheState::SC : CacheState::UC;
    }
    else
        flow.requesterFinal = CacheState::I;

    if (decision.mergeCopyback && (decision.allocateHome || decision.retainHome || transaction.homeHit))
    {
        const std::optional<size_t> currentWay = directory.FindWay(line);
        InstallHome(line, currentWay ? *currentWay : flow.victimWay, transaction.data, dirty, time);
    }
    else
        memory.Write(line, transaction.data, transaction.byteEnables);

    snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
    return true;
}

bool Core::Impl::CommitCMO(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    const LineKey line = transaction.request.Line();
    HomeLine* home = directory.Find(line);

    if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanShared)
    {
        flow.requesterFinal = snoopFilter.State(line, transaction.request.source);
        if (home)
        {
            if (flow.snoopDataValid)
                home->data = transaction.data;
            home->dirty = false;
        }
        if (flow.dataDirty || flow.homeDirty)
            memory.Write(line, transaction.data, FullEnables());
        return true;
    }

    flow.requesterFinal = CacheState::I;
    if (transaction.request.opcode == CHI::Eb::Opcodes::REQ::CleanInvalid && (flow.dataDirty || flow.homeDirty))
        memory.Write(line, transaction.data, FullEnables());
    if (home)
        directory.Invalidate(directory.SetIndex(line), *directory.FindWay(line));
    snoopFilter.Track(line, transaction.request.source, CacheState::I, time);
    return true;
}

bool Core::Impl::PrepareVictim(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    const LineKey line = transaction.request.Line();
    if (!flow.victimPending)
    {
        flow.victimWay = directory.ChooseVictim(line);
        const HomeLine& victim = directory.At(directory.SetIndex(line), flow.victimWay);
        if (!victim.valid)
            return true;
        flow.victimPending = true;
        flow.victimLine = victim.key;
        flow.victimData = victim.data;
        flow.victimDirty = victim.dirty;
        flow.victimDataValid = true;
    }

    if (!flow.victimLinked)
    {
        hazards.Add(transaction.handle, flow.victimLine, transaction.actionAge);
        flow.victimLinked = true;
    }
    if (!hazards.IsHead(transaction.handle, flow.victimLine))
        return false;

    PolicyContext context;
    context.line = flow.victimLine;
    context.homeValid = true;
    context.homeDirty = flow.victimDirty;
    context.requesterValid = !snoopFilter.Targets(flow.victimLine).empty();
    const PolicyDecision decision = policy->OnHomeEviction(context);
    if (decision.backInvalidate && context.requesterValid && flow.nextSnoop == 0)
    {
        flow.snoopForVictim = true;
        flow.snoopLine = flow.victimLine;
        transaction.snoopTargets = snoopFilter.Targets(flow.victimLine);
        transaction.pendingSnoops = transaction.snoopTargets;
        transaction.snoopsRequired = true;
        flow.nextSnoop = 0;
        return false;
    }

    if (flow.nextSnoop == 0)
    {
        CompleteVictim(transaction);
        return true;
    }
    return false;
}

void Core::Impl::CompleteVictim(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    if (!flow.victimPending)
        return;
    if (flow.victimDirty)
        memory.Write(flow.victimLine, flow.victimData, FullEnables());
    directory.Invalidate(directory.SetIndex(flow.victimLine), flow.victimWay);
    flow.victimPending = false;
    flow.victimDataValid = false;
    flow.victimDirty = false;
    flow.snoopForVictim = false;
    flow.snoopLine = transaction.request.Line();
    transaction.snoopTargets.clear();
    transaction.pendingSnoops.clear();
    transaction.snoopsRequired = false;
    flow.nextSnoop = 0;
    flow.snoopBeatMasks.clear();
    flow.snoopResponses.clear();
}

void Core::Impl::InstallHome(const LineKey& line, size_t way, const LineData& data, bool dirty, Time age)
{
    HomeLine& home = directory.Install(line, way, age);
    home.data = data;
    home.validBytes = FullEnables();
    home.dirty = dirty;
}

void Core::Impl::EvictRequester(Transaction& transaction)
{
    FlowState& flow = flows.at(transaction.handle);
    snoopFilter.Remove(transaction.request.Line(), transaction.request.source);
    flow.requesterFinal = CacheState::I;
}

}
