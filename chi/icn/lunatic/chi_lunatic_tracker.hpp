#pragma once

#include "chi_lunatic_transaction.hpp"

#include <map>
#include <memory>
#include <vector>

namespace CHI::Lunatic {

struct RequestKey {
    NodeId source = 0;
    TransactionId txnId = 0;
    auto operator<=>(const RequestKey&) const = default;
};

class Tracker {
    size_t limit;
    TransactionHandle nextHandle = 0;
    std::map<TransactionHandle, std::unique_ptr<Transaction>> transactions;
    std::map<RequestKey, TransactionHandle> requestMap;
    std::map<RequestKey, TransactionHandle> dbidMap;
    std::map<RequestKey, TransactionHandle> snoopMap;
    std::map<RequestKey, TransactionHandle> memoryMap;

public:
    explicit Tracker(size_t limit) : limit(limit) {}

    Transaction* Allocate(const Request& request, Time actionAge);
    Transaction* Find(TransactionHandle handle) const noexcept;
    Transaction* FindRequest(NodeId source, TransactionId txnId) const noexcept;
    Transaction* FindDBID(NodeId source, DatabaseId dbid) const noexcept;
    Transaction* FindSnoop(NodeId source, TransactionId txnId) const noexcept;
    Transaction* FindMemory(NodeId source, TransactionId txnId) const noexcept;

    bool BindDBID(TransactionHandle handle, DatabaseId dbid);
    bool BindSnoop(TransactionHandle handle, NodeId target, TransactionId txnId);
    bool BindMemory(TransactionHandle handle, NodeId subordinate, TransactionId txnId);
    bool UnbindMemory(NodeId subordinate, TransactionId txnId);
    bool Retire(TransactionHandle handle);

    std::vector<TransactionHandle> Handles() const;
    size_t Size() const noexcept { return transactions.size(); }
    size_t Limit() const noexcept { return limit; }
    bool Empty() const noexcept { return transactions.empty(); }
    bool Validate() const noexcept;
    void Clear() noexcept;
};

}
