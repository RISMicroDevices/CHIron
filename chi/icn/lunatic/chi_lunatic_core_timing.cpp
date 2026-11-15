#include "chi_lunatic_core.hpp"

#include <algorithm>
#include <bit>

namespace CHI::Lunatic {

    SchedulePoint OutputPoint(const Request&, bool downstream, size_t)
    {
        return downstream ? SchedulePoint::BackendRequest : SchedulePoint::NormalResponse;
    }

    SchedulePoint OutputPoint(const Response& flit, bool, size_t)
    {
        switch (flit.opcode)
        {
            case CHI::Eb::Opcodes::RSP::RetryAck: return SchedulePoint::RetryAck;
            case CHI::Eb::Opcodes::RSP::PCrdGrant: return SchedulePoint::CreditGrant;
            case CHI::Eb::Opcodes::RSP::DBIDResp:
            case CHI::Eb::Opcodes::RSP::DBIDRespOrd:
            case CHI::Eb::Opcodes::RSP::CompDBIDResp:
                return SchedulePoint::FastResponse;
            default: return SchedulePoint::NormalResponse;
        }
    }

    SchedulePoint OutputPoint(const Data&, bool downstream, size_t)
    {
        return downstream ? SchedulePoint::BackendData : SchedulePoint::DataReturn;
    }

    SchedulePoint OutputPoint(const Snoop&, bool, size_t)
    {
        return SchedulePoint::SnoopIssue;
    }

    Time Core::Impl::ScheduleResult(Transaction& transaction, SchedulePoint point, Time earliest)
    {
        BankState& bank = Bank(transaction);
        FlowState& flow = flows.at(transaction.handle);
        const Time delay = bank.pipeline.OperationDelay(point);
        const auto [it, inserted] = flow.scheduleMarks.emplace(point, bank.pipeline.SaturatingAdd(earliest, delay));
        (void)inserted;
        return it->second;
    }

    bool Core::Impl::ScheduleTransition(Transaction& transaction, SchedulePoint point)
    {
        if (point == SchedulePoint::Automatic)
            return true;
        FlowState& flow = flows.at(transaction.handle);
        if (point == SchedulePoint::DataBeat)
        {
            const size_t beat = flow.backendPhase == BackendPhase::SendData
                ? size_t(std::popcount(flow.backendBeatMask)) : transaction.sentBeats;
            if (flow.dataBeatScheduled == beat)
                return true;
            flow.dataBeatScheduled = beat;
            flow.scheduleMarks.erase(point);
        }
        else if (flow.scheduleMarks.contains(point))
            return true;
        if (Bank(transaction).pipeline.OperationDelay(point) == 0)
        {
            ScheduleResult(transaction, point, time);
            return true;
        }
        transaction.readyTime = std::max(transaction.readyTime, ScheduleResult(transaction, point, time));
        return transaction.readyTime <= time;
    }

    Time Core::Impl::ScheduleOutput(TransactionHandle handle, SchedulePoint point, Time earliest, size_t retry)
    {
        size_t bank = 0;
        if (const Transaction* transaction = tracker.Find(handle))
            bank = BankIndex(transaction->request.Line());
        else if (const auto record = retries.find(retry); record != retries.end())
            bank = record->second.bank;
        VirtualPipeline& pipeline = banks[bank].pipeline;
        Time delay = pipeline.OperationDelay(point);
        delay = pipeline.SaturatingAdd(delay, pipeline.OperationDelay(SchedulePoint::Output));
        return pipeline.SaturatingAdd(earliest, delay);
    }

}
