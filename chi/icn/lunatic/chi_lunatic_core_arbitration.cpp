#include "chi_lunatic_core.hpp"

#include <algorithm>
#include <map>

namespace CHI::Lunatic {

    bool Core::Impl::IsRefillTransaction(const Transaction& transaction) const
    {
        const FlowState& flow = flows.at(transaction.handle);
        return transaction.Complete(COMPLETE_MEMORY) && !flow.directoryCommitted &&
            transaction.pendingSnoops.empty() &&
            (IsReadRequest(transaction.request.opcode) || flow.baselineFetched);
    }

    void Core::Impl::OrderTransactions(std::vector<TransactionHandle>& handles)
    {
        std::sort(handles.begin(), handles.end(), [&](TransactionHandle a, TransactionHandle b) {
            const Transaction* ta = tracker.Find(a);
            const Transaction* tb = tracker.Find(b);
            if (ta->actionAge != tb->actionAge)
                return ta->actionAge < tb->actionAge;
            return a < b;
        });

        if (options.strategy.requestArbitration == RequestArbitrationKind::RefillFirst)
        {
            std::stable_sort(handles.begin(), handles.end(), [&](TransactionHandle a, TransactionHandle b) {
                const Transaction* ta = tracker.Find(a);
                const Transaction* tb = tracker.Find(b);
                const bool refillA = IsRefillTransaction(*ta);
                const bool refillB = IsRefillTransaction(*tb);
                if (refillA != refillB)
                    return refillA;
                if (ta->actionAge != tb->actionAge)
                    return ta->actionAge < tb->actionAge;
                return a < b;
            });
        }
        else if (options.strategy.requestArbitration == RequestArbitrationKind::OldestClassWeighted &&
                 options.structure.classes.enabled)
        {
            struct WorkGroup {
                LineKey                       line;
                std::vector<TransactionHandle> handles;
            };

            std::vector<TransactionHandle> ordered;
            for (size_t first = 0; first < handles.size();)
            {
                const Time age = tracker.Find(handles[first])->actionAge;
                size_t last = first + 1;
                while (last < handles.size() && tracker.Find(handles[last])->actionAge == age)
                    ++last;

                std::vector<WorkGroup> groups;
                std::map<LineKey, size_t> groupByLine;
                for (size_t index = first; index < last; ++index)
                {
                    const TransactionHandle handle = handles[index];
                    const Transaction* transaction = tracker.Find(handle);
                    if (!transaction || !Bank(*transaction).pipeline.Eligible(*transaction, time))
                        continue;
                    const LineKey line = transaction->request.Line();
                    auto [it, inserted] = groupByLine.emplace(line, groups.size());
                    if (inserted)
                        groups.push_back({ line, {} });
                    groups[it->second].handles.push_back(handle);
                }

                while (!groups.empty())
                {
                    std::array<bool, 4> eligible{};
                    for (const WorkGroup& group : groups)
                    {
                        const size_t classId = flows.at(group.handles.front()).assignedClass;
                        if (classId < eligible.size())
                            eligible[classId] = true;
                    }
                    const size_t selectedClass = SelectWeighted(options.structure.classes.requestWeights,
                        eligible, requestClassRound, true);
                    if (selectedClass == eligible.size())
                        break;
                    const auto selected = std::find_if(groups.begin(), groups.end(), [&](const WorkGroup& group) {
                        return flows.at(group.handles.front()).assignedClass == selectedClass;
                    });
                    if (selected == groups.end())
                        break;
                    ordered.insert(ordered.end(), selected->handles.begin(), selected->handles.end());
                    groups.erase(selected);
                }
                for (const WorkGroup& group : groups)
                    ordered.insert(ordered.end(), group.handles.begin(), group.handles.end());
                first = last;
            }
            handles = std::move(ordered);
        }

        if (options.randomizeArbitration && options.strategy.requestArbitration == RequestArbitrationKind::Age &&
            handles.size() > 1)
        {
            struct WorkGroup {
                LineKey                       line;
                std::vector<TransactionHandle> handles;
            };
            std::vector<WorkGroup> groups;
            std::map<LineKey, size_t> groupByLine;
            for (TransactionHandle handle : handles)
            {
                const Transaction* transaction = tracker.Find(handle);
                if (!transaction || !Bank(*transaction).pipeline.Eligible(*transaction, time))
                    continue;
                const LineKey line = transaction->request.Line();
                auto [it, inserted] = groupByLine.emplace(line, groups.size());
                if (inserted)
                    groups.push_back({ line, {} });
                groups[it->second].handles.push_back(handle);
            }
            for (size_t size = groups.size(); size > 1; --size)
                std::swap(groups[size - 1], groups[random.Bounded(size - 1)]);
            handles.clear();
            for (const WorkGroup& group : groups)
                handles.insert(handles.end(), group.handles.begin(), group.handles.end());
        }
    }

}
