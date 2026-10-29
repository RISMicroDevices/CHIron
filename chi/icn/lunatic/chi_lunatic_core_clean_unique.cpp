#include "chi_lunatic_core.hpp"

namespace CHI::Lunatic {

    bool Core::Impl::CommitCleanUnique(Transaction& transaction)
    {
        FlowState& flow = flows.at(transaction.handle);
        const LineKey line = transaction.request.Line();
        const CacheState initial = snoopFilter.State(line, transaction.request.source);
        switch (initial)
        {
            case CacheState::I:   flow.requesterFinal = CacheState::UCE; break;
            case CacheState::UCE: flow.requesterFinal = CacheState::UCE; break;
            case CacheState::UC:
            case CacheState::SC:  flow.requesterFinal = CacheState::UC; break;
            case CacheState::SD:
            case CacheState::UD:  flow.requesterFinal = CacheState::UD; break;
            default: return false;
        }

        if (flow.snoopDataPartial && !transaction.homeHit)
        {
            if (options.backend == BackendKind::CHI)
            {
                if (!FetchBaseline(transaction))
                    return false;
            }
            else
            {
                const LineData baseline = memory.Read(line);
                for (size_t index = 0; index < LINE_SIZE; ++index)
                    if (!flow.snoopValidBytes[index])
                        transaction.data[index] = baseline[index];
            }
        }

        PolicyContext context;
        context.line = line;
        context.requester = transaction.request.source;
        context.homeValid = transaction.homeHit;
        context.homeDirty = flow.homeDirty;
        context.requesterValid = IsValid(flow.requesterFinal);
        context.requesterDirty = IsDirty(flow.requesterFinal);
        const PolicyDecision decision = policy->OnHomeAllocation(context);
        if (decision.allocateHome && !transaction.homeHit && !PrepareVictim(transaction))
            return false;

        if (flow.dataDirty || flow.homeDirty)
        {
            ByteEnables enables{};
            enables.fill(1);
            if (!WriteLine(transaction, line, transaction.data, enables))
                return false;
        }

        if (decision.allocateHome || decision.retainHome)
        {
            const std::optional<size_t> currentWay = directory.FindWay(line);
            const size_t way = currentWay ? *currentWay : flow.victimWay;
            HomeLine& home = InstallHome(line, way, transaction.data, false, time);
            home.validBytes = flow.validBytes;
        }
        else if (decision.removeHome && transaction.homeHit)
        {
            if (const auto way = directory.FindWay(line); way)
                directory.Invalidate(directory.SetIndex(line), *way);
        }
        snoopFilter.Track(line, transaction.request.source, flow.requesterFinal, time);
        return true;
    }

}
