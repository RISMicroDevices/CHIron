#include "chi_lunatic_pipeline.hpp"

#include <algorithm>
#include <limits>

namespace CHI::Lunatic {

    bool VirtualPipeline::Eligible(const Transaction& transaction, Time time) const noexcept
    {
        return transaction.phase != Phase::Retired && transaction.readyTime <= time;
    }

    Time VirtualPipeline::StageLatency(Phase phase) const noexcept
    {
        switch (phase)
        {
            case Phase::Lookup: return options.tagLatency;
            case Phase::Action: return options.dataLatency;
            case Phase::WaitMemory:
            case Phase::WaitWriteData:
            case Phase::WaitSnoop:
            case Phase::WaitCompAck:
            case Phase::Retired:
                return 0;
            default:
                return 1;
        }
    }

    Time VirtualPipeline::PhaseDelay(Phase phase) const noexcept
    {
        switch (phase)
        {
            case Phase::Receive: return options.receiveDelay;
            case Phase::Decode: return options.decodeDelay;
            case Phase::Admit: return options.admissionDelay;
            case Phase::Hazard: return options.hazardDelay;
            case Phase::Lookup: return options.lookupDelay;
            case Phase::Resolve: return options.resolveDelay;
            case Phase::Action: return options.actionDelay;
            case Phase::Return: return options.returnDelay;
            case Phase::Commit: return options.commitDelay;
            default: return 0;
        }
    }

    bool VirtualPipeline::DelayablePhase(Phase phase) const noexcept
    {
        switch (phase)
        {
            case Phase::Receive:
            case Phase::Decode:
            case Phase::Admit:
            case Phase::Hazard:
            case Phase::Lookup:
            case Phase::Resolve:
            case Phase::Action:
            case Phase::Return:
            case Phase::Commit:
                return true;
            default:
                return false;
        }
    }

    Time VirtualPipeline::Advance(Transaction& transaction, Phase next, Time time) const noexcept
    {
        transaction.phase = next;
        transaction.readyTime = SaturatingAdd(time, StageLatency(next));
        return transaction.readyTime;
    }

    Time VirtualPipeline::ScheduleTag(Time earliest) noexcept
    {
        const Time slot = std::max(earliest, tagPortFree);
        tagPortFree = SaturatingAdd(slot, 1);
        return SaturatingAdd(slot, options.tagLatency);
    }

    Time VirtualPipeline::ScheduleData(Time earliest) noexcept
    {
        const Time slot = std::max(earliest, dataPortFree);
        dataPortFree = SaturatingAdd(slot, 1);
        return SaturatingAdd(slot, options.dataLatency);
    }

    Time VirtualPipeline::SaturatingAdd(Time time, Time delay) const noexcept
    {
        if (delay > std::numeric_limits<Time>::max() - time)
            return std::numeric_limits<Time>::max();
        return time + delay;
    }

    void VirtualPipeline::Reset() noexcept
    {
        tagPortFree = 0;
        dataPortFree = 0;
    }

}
