#include "chi_lunatic_tracker.hpp"

namespace CHI::Lunatic {

namespace {

template<typename Index>
void EraseHandle(Index& index, TransactionHandle handle) noexcept
{
    for (auto it = index.begin(); it != index.end();) {
        if (it->second == handle)
            it = index.erase(it);
        else
            ++it;
    }
}

}

Transaction* Tracker::Allocate(const Request& request, Time actionAge)
{
    if (transactions.size() >= limit || transactions.size() >= size_t(INVALID_TRANSACTION))
        return nullptr;

    const RequestKey requestKey{ request.source, request.txnId };
    if (requestMap.find(requestKey) != requestMap.end())
        return nullptr;

    TransactionHandle handle = INVALID_TRANSACTION;
    for (size_t attempt = 0; attempt < limit; ++attempt) {
        const TransactionHandle candidate = nextHandle;
        nextHandle = (nextHandle + 1) % limit;
        if (candidate != INVALID_TRANSACTION && transactions.find(candidate) == transactions.end()) {
            handle = candidate;
            break;
        }
    }
    if (handle == INVALID_TRANSACTION)
        return nullptr;

    auto transaction = std::make_unique<Transaction>(handle, request, actionAge);
    auto [requestIt, requestInserted] = requestMap.emplace(requestKey, handle);
    if (!requestInserted)
        return nullptr;

    try {
        auto [transactionIt, inserted] = transactions.emplace(handle, std::move(transaction));
        if (!inserted) {
            requestMap.erase(requestIt);
            return nullptr;
        }

        return transactionIt->second.get();
    } catch (...) {
        requestMap.erase(requestIt);
        throw;
    }
}

Transaction* Tracker::Find(TransactionHandle handle) const noexcept
{
    auto it = transactions.find(handle);
    return it == transactions.end() ? nullptr : it->second.get();
}

Transaction* Tracker::FindRequest(NodeId source, TransactionId txnId) const noexcept
{
    auto it = requestMap.find({ source, txnId });
    return it == requestMap.end() ? nullptr : Find(it->second);
}

Transaction* Tracker::FindDBID(NodeId source, DatabaseId dbid) const noexcept
{
    auto it = dbidMap.find({ source, dbid });
    return it == dbidMap.end() ? nullptr : Find(it->second);
}

Transaction* Tracker::FindSnoop(NodeId source, TransactionId txnId) const noexcept
{
    auto it = snoopMap.find({ source, txnId });
    return it == snoopMap.end() ? nullptr : Find(it->second);
}

Transaction* Tracker::FindMemory(NodeId source, TransactionId txnId) const noexcept
{
    auto it = memoryMap.find({ source, txnId });
    return it == memoryMap.end() ? nullptr : Find(it->second);
}

bool Tracker::BindDBID(TransactionHandle handle, DatabaseId dbid)
{
    Transaction* transaction = Find(handle);
    if (!transaction)
        return false;

    const RequestKey key{ transaction->request.source, dbid };
    if (dbidMap.find(key) != dbidMap.end())
        return false;

    for (const auto& [boundKey, boundHandle] : dbidMap) {
        (void)boundKey;
        if (boundHandle == handle)
            return false;
    }

    auto [it, inserted] = dbidMap.emplace(key, handle);
    if (!inserted)
        return false;

    transaction->dbid = dbid;
    return true;
}

bool Tracker::BindSnoop(TransactionHandle handle, NodeId target, TransactionId txnId)
{
    if (!Find(handle))
        return false;
    return snoopMap.emplace(RequestKey{ target, txnId }, handle).second;
}

bool Tracker::BindMemory(TransactionHandle handle, NodeId subordinate, TransactionId txnId)
{
    if (!Find(handle))
        return false;
    return memoryMap.emplace(RequestKey{ subordinate, txnId }, handle).second;
}

bool Tracker::UnbindMemory(NodeId subordinate, TransactionId txnId)
{
    return memoryMap.erase({ subordinate, txnId }) != 0;
}

bool Tracker::Retire(TransactionHandle handle)
{
    auto it = transactions.find(handle);
    if (it == transactions.end())
        return false;

    EraseHandle(requestMap, handle);
    EraseHandle(dbidMap, handle);
    EraseHandle(snoopMap, handle);
    EraseHandle(memoryMap, handle);
    transactions.erase(it);
    return true;
}

std::vector<TransactionHandle> Tracker::Handles() const
{
    std::vector<TransactionHandle> handles;
    handles.reserve(transactions.size());
    for (const auto& [handle, transaction] : transactions) {
        (void)transaction;
        handles.push_back(handle);
    }
    return handles;
}

bool Tracker::Validate() const noexcept
{
    if (transactions.size() > limit)
        return false;

    for (const auto& [handle, transaction] : transactions) {
        if (handle == INVALID_TRANSACTION || !transaction || transaction->handle != handle)
            return false;

        const RequestKey key{ transaction->request.source, transaction->request.txnId };
        auto requestIt = requestMap.find(key);
        if (requestIt == requestMap.end() || requestIt->second != handle)
            return false;

        if (transaction->dbid != 0) {
            auto dbidIt = dbidMap.find({ transaction->request.source, transaction->dbid });
            if (dbidIt == dbidMap.end() || dbidIt->second != handle)
                return false;
        }
    }

    for (const auto& [key, handle] : requestMap) {
        const Transaction* transaction = Find(handle);
        if (!transaction || transaction->request.source != key.source ||
            transaction->request.txnId != key.txnId || FindRequest(key.source, key.txnId) != transaction)
            return false;
    }

    for (const auto& [key, handle] : dbidMap) {
        const Transaction* transaction = Find(handle);
        if (!transaction || transaction->request.source != key.source || transaction->dbid != key.txnId ||
            FindDBID(key.source, key.txnId) != transaction)
            return false;
    }

    for (const auto& [key, handle] : snoopMap) {
        const Transaction* transaction = Find(handle);
        if (!transaction || FindSnoop(key.source, key.txnId) != transaction)
            return false;
    }

    for (const auto& [key, handle] : memoryMap) {
        const Transaction* transaction = Find(handle);
        if (!transaction || FindMemory(key.source, key.txnId) != transaction)
            return false;
    }

    return true;
}

void Tracker::Clear() noexcept
{
    requestMap.clear();
    dbidMap.clear();
    snoopMap.clear();
    memoryMap.clear();
    transactions.clear();
    nextHandle = 0;
}

}
