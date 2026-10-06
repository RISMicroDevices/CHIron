#include "cohestra_watchdog.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>


// Implementation of: ToString helpers
namespace Cohestra {

    const char* ToString(WatchdogXactClass xactClass) noexcept
    {
        switch (xactClass)
        {
        case WatchdogXactClass::REQ:         return "REQ";
        case WatchdogXactClass::EVT:         return "EVT";
        case WatchdogXactClass::CMO:         return "CMO";
        case WatchdogXactClass::PREFETCH:    return "PREFETCH";
        }

        return "UNKNOWN";
    }
}


// Implementation of: class TransactionWatchdog
namespace Cohestra {

    const ConfigurationEntryBack<bool> TransactionWatchdog::WATCHDOG_ENABLE
        ("WATCHDOG_ENABLE", "enable", "watchdog", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> TransactionWatchdog::WATCHDOG_PROGRESS_TIMEOUT
        ("WATCHDOG_PROGRESS_TIMEOUT", "progress.timeout", "watchdog", 200000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<uint64_t> TransactionWatchdog::WATCHDOG_XACT_TIMEOUT
        ("WATCHDOG_XACT_TIMEOUT", "xact.timeout", "watchdog", 100000, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> TransactionWatchdog::WATCHDOG_FAIL_ON_TIMEOUT
        ("WATCHDOG_FAIL_ON_TIMEOUT", "fail.on.timeout", "watchdog", true, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<uint64_t> TransactionWatchdog::WATCHDOG_DUMP_LIMIT
        ("WATCHDOG_DUMP_LIMIT", "dump.limit", "watchdog", 8, DeserializeUInt64, SerializeUInt64);

    const ConfigurationEntryBack<bool> TransactionWatchdog::WATCHDOG_DUMP_FLITS
        ("WATCHDOG_DUMP_FLITS", "dump.flits", "watchdog", true, DeserializeBool, SerializeBool);


    TransactionWatchdog::TransactionWatchdog(Instance& instance) noexcept
        : instance  (instance)
    { }

    TransactionWatchdog::~TransactionWatchdog() noexcept
    {
        Detach();
    }

    bool TransactionWatchdog::LoadConfiguration(const StringLoaderBase& loader) noexcept
    {
        return LoadConfigurationEntry(loader, &WATCHDOG_ENABLE,             enabled)
            && LoadConfigurationEntry(loader, &WATCHDOG_PROGRESS_TIMEOUT,   progressTimeout)
            && LoadConfigurationEntry(loader, &WATCHDOG_XACT_TIMEOUT,       xactTimeout)
            && LoadConfigurationEntry(loader, &WATCHDOG_FAIL_ON_TIMEOUT,    failOnTimeout)
            && LoadConfigurationEntry(loader, &WATCHDOG_DUMP_LIMIT,         dumpLimit)
            && LoadConfigurationEntry(loader, &WATCHDOG_DUMP_FLITS,         dumpFlits);
    }

    void TransactionWatchdog::Attach() noexcept
    {
        if (attached)
            return;

        for (size_t index = 0; index < instance.GetType1UpstreamCount(); ++index)
        {
            auto* upstream = instance.GetType1Upstream(index);

            if (!upstream || !upstream->events)
                continue;

            // a harness may share one event hub between all upstream nodes;
            // register only once per hub
            if (std::find(attachedHubs.begin(), attachedHubs.end(), upstream->events) != attachedHubs.end())
                continue;

            upstream->events->OnAcceptedREQ.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedREQ, this));
            upstream->events->OnAcceptedEVT.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedEVT, this));
            upstream->events->OnAcceptedCMO.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedCMO, this));
            upstream->events->OnAcceptedPrefetch.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedPrefetch, this));
            upstream->events->OnCacheLineGranted.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnCacheLineGranted, this));
            upstream->events->OnAcceptedCompCMO.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedCompCMO, this));
            upstream->events->OnAcceptedDnRSP.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedDnRSP, this));
            upstream->events->OnAcceptedDnDAT.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedDnDAT, this));
            upstream->events->OnAcceptedSNP.Register(
                Gravity::MakeListener(LISTENER_NAME, 0, &TransactionWatchdog::OnAcceptedSNP, this));

            attachedHubs.push_back(upstream->events);
        }

        attached = true;
    }

    void TransactionWatchdog::Detach() noexcept
    {
        for (auto& hub : attachedHubs)
        {
            hub->OnAcceptedREQ.Unregister(LISTENER_NAME);
            hub->OnAcceptedEVT.Unregister(LISTENER_NAME);
            hub->OnAcceptedCMO.Unregister(LISTENER_NAME);
            hub->OnAcceptedPrefetch.Unregister(LISTENER_NAME);
            hub->OnCacheLineGranted.Unregister(LISTENER_NAME);
            hub->OnAcceptedCompCMO.Unregister(LISTENER_NAME);
            hub->OnAcceptedDnRSP.Unregister(LISTENER_NAME);
            hub->OnAcceptedDnDAT.Unregister(LISTENER_NAME);
            hub->OnAcceptedSNP.Unregister(LISTENER_NAME);
        }

        attachedHubs.clear();
        attached = false;
    }

    bool TransactionWatchdog::IsAttached() const noexcept
    {
        return attached;
    }

    bool TransactionWatchdog::IsEnabled() const noexcept
    {
        return enabled;
    }

    void TransactionWatchdog::Birth(WatchdogXactClass xactClass,
                                    std::shared_ptr<CCHI::Xact::Xaction<CommonFlitConfigurationType1>> xaction,
                                    uint64_t txnID,
                                    uint64_t opcode,
                                    uint64_t address,
                                    size_t upstreamIndex) noexcept
    {
        if (!xaction)
            return;

        const void* key = xaction.get();

        // ignore duplicates defensively: the same xaction identity must enter
        // the registry only once
        if (outstandingByKey.find(key) != outstandingByKey.end())
            return;

        // a new outstanding epoch begins: re-base the no-progress timer so a
        // quiet period without outstanding transactions never counts against
        // the first transaction of the epoch
        if (outstanding.empty())
            lastProgress = now;

        OutstandingEntry entry;

        entry.xaction       = std::move(xaction);
        entry.upstreamIndex = upstreamIndex;
        entry.xactClass     = xactClass;
        entry.txnID         = txnID;
        entry.opcode        = opcode;
        entry.address       = address;
        entry.birthTime     = now;

        outstanding.push_back(std::move(entry));

        auto iter = outstanding.end();
        --iter;

        outstandingByKey.emplace(key, iter);

        ++countBirthsByClass[size_t(xactClass)];

        maxOutstanding = std::max<uint64_t>(maxOutstanding, outstanding.size());
    }

    void TransactionWatchdog::Retire(const std::shared_ptr<CCHI::Xact::Xaction<CommonFlitConfigurationType1>>& xaction) noexcept
    {
        // every death event is transaction progress, matched or not
        lastProgress = now;

        if (!xaction)
            return;

        auto iter = outstandingByKey.find(xaction.get());

        // the first matching death retires the entry; later deaths of the
        // same xaction (and deaths of never-registered xactions) are ignored
        if (iter == outstandingByKey.end())
            return;

        ++countDeathsByClass[size_t(iter->second->xactClass)];

        outstanding.erase(iter->second);
        outstandingByKey.erase(iter);

        // the no-progress epoch ended: a later epoch may warn again
        if (outstanding.empty())
            livelockWarned = false;
    }

    void TransactionWatchdog::OnAcceptedREQ(AcceptedREQEvent& event) noexcept
    {
        const auto& flit = event.GetREQFlit();

        Birth(WatchdogXactClass::REQ,
            event.GetXaction(),
            uint64_t(flit.TxnID),
            uint64_t(flit.Opcode),
            uint64_t(flit.Addr),
            instance.GetType1UpstreamIndex(&event.GetUpstream()).value_or(UNKNOWN_UPSTREAM));
    }

    void TransactionWatchdog::OnAcceptedEVT(AcceptedEVTEvent& event) noexcept
    {
        const auto& flit = event.GetEVTFlit();

        Birth(WatchdogXactClass::EVT,
            event.GetXaction(),
            uint64_t(flit.TxnID),
            uint64_t(flit.Opcode),
            uint64_t(flit.Addr),
            instance.GetType1UpstreamIndex(&event.GetUpstream()).value_or(UNKNOWN_UPSTREAM));
    }

    void TransactionWatchdog::OnAcceptedCMO(AcceptedCMOEvent& event) noexcept
    {
        const auto& flit = event.GetREQFlit();

        Birth(WatchdogXactClass::CMO,
            event.GetXaction(),
            uint64_t(flit.TxnID),
            uint64_t(flit.Opcode),
            uint64_t(flit.Addr),
            instance.GetType1UpstreamIndex(&event.GetUpstream()).value_or(UNKNOWN_UPSTREAM));
    }

    void TransactionWatchdog::OnAcceptedPrefetch(AcceptedPrefetchEvent& event) noexcept
    {
        const auto& flit = event.GetREQFlit();

        // Fire-and-forget prefetch (ExpCompStash=0) completes at its request
        // flit: no response/grant ever follows, so there is nothing to watch.
        if (!flit.ExpCompStash)
            return;

        Birth(WatchdogXactClass::PREFETCH,
            event.GetXaction(),
            uint64_t(flit.TxnID),
            uint64_t(flit.Opcode),
            uint64_t(flit.Addr),
            instance.GetType1UpstreamIndex(&event.GetUpstream()).value_or(UNKNOWN_UPSTREAM));
    }

    void TransactionWatchdog::OnCacheLineGranted(GrantedEvent& event) noexcept
    {
        Retire(event.GetXaction());
    }

    void TransactionWatchdog::OnAcceptedCompCMO(AcceptedCompCMOEvent& event) noexcept
    {
        Retire(event.GetXaction());
    }

    void TransactionWatchdog::OnAcceptedDnRSP(AcceptedDnRSPEvent& event) noexcept
    {
        Retire(event.GetXaction());
    }

    void TransactionWatchdog::OnAcceptedDnDAT(AcceptedDnDATEvent& event) noexcept
    {
        Retire(event.GetXaction());
    }

    void TransactionWatchdog::OnAcceptedSNP(AcceptedSNPEvent& event) noexcept
    {
        // DUT-driven snoop activity is transaction progress (the DUT is
        // demonstrably alive), but touches no registry entry
        lastProgress = now;
    }

    void TransactionWatchdog::Tick(uint64_t time) noexcept
    {
        if (!enabled)
            return;

        now = time;

        // once the instance left ALIVE (e.g. by this watchdog's own failure),
        // the watchdog goes inert
        if (!instance.IsAlive())
            return;

        if (outstanding.empty())
            return;

        // livelock tier: the whole system went silent while transactions are
        // outstanding
        if (progressTimeout && now - lastProgress > progressTimeout && !livelockWarned)
        {
            ++countLivelockTriggers;

            instance.GetLogger()->error(
                "Transaction watchdog: no transaction progress for {} time unit(s), {} transaction(s) outstanding.",
                now - lastProgress,
                outstanding.size());

            DumpOutstanding();

            if (failOnTimeout)
            {
                char message[256];

                std::snprintf(message, sizeof(message),
                    "Transaction watchdog: no transaction progress for %llu time unit(s), %zu transaction(s) outstanding.",
                    static_cast<unsigned long long>(now - lastProgress),
                    outstanding.size());

                instance.SetFailed({ message });
                return;
            }

            livelockWarned = true;
        }

        // deadlock tier: the oldest outstanding transaction never received
        // its first response (front of the birth-ordered registry, O(1))
        OutstandingEntry& front = outstanding.front();

        if (xactTimeout && now - front.birthTime > xactTimeout && !front.reported)
        {
            ++countXactTriggers;

            instance.GetLogger()->error(
                "Transaction watchdog: transaction outstanding for {} time unit(s) without first response: upstream {}, class {}, TxnID {:#x}, opcode {:#x}, address {:#x}.",
                now - front.birthTime,
                front.upstreamIndex == UNKNOWN_UPSTREAM ? "unknown" : std::to_string(front.upstreamIndex),
                ToString(front.xactClass),
                front.txnID,
                front.opcode,
                front.address);

            DumpOutstanding();

            if (failOnTimeout)
            {
                char message[256];

                std::snprintf(message, sizeof(message),
                    "Transaction watchdog: transaction outstanding for %llu time unit(s) without first response: upstream %zu, class %s, TxnID 0x%llx, opcode 0x%llx, address 0x%llx.",
                    static_cast<unsigned long long>(now - front.birthTime),
                    front.upstreamIndex,
                    ToString(front.xactClass),
                    static_cast<unsigned long long>(front.txnID),
                    static_cast<unsigned long long>(front.opcode),
                    static_cast<unsigned long long>(front.address));

                instance.SetFailed({ message });
                return;
            }

            front.reported = true;
        }
    }

    void TransactionWatchdog::DumpOutstanding() const noexcept
    {
        auto logger = instance.GetLogger();

        uint64_t shown = 0;

        for (const auto& entry : outstanding)
        {
            if (shown >= dumpLimit)
                break;

            auto hash = GetXactHashTag(entry.xaction);

            logger->error(
                " - [#{}] upstream {}, class {}, TxnID {:#x}, opcode {:#x}, address {:#x}, age {} time unit(s).",
                hash ? std::to_string(*hash) : std::to_string(shown),
                entry.upstreamIndex == UNKNOWN_UPSTREAM ? "unknown" : std::to_string(entry.upstreamIndex),
                ToString(entry.xactClass),
                entry.txnID,
                entry.opcode,
                entry.address,
                now - entry.birthTime);

            if (dumpFlits)
                DumpXactFlits(entry);

            ++shown;
        }

        if (outstanding.size() > shown)
            logger->error(" - ... and {} more outstanding transaction(s).", outstanding.size() - shown);
    }

    void TransactionWatchdog::DumpXactFlits(const OutstandingEntry& entry) const noexcept
    {
        if (!entry.xaction)
            return;

        auto logger = instance.GetLogger();

        if (entry.xaction->GetFirstDenial() == CCHI::XactDenial::ACCEPTED)
        {
            const auto& first = entry.xaction->GetFirst();

            logger->error(
                "     time={} cycle={} [{}]{}",
                first.time,
                first.time / 2,
                GetRequestChannelLabel(first),
                FormatFirstFlit(first));
        }

        const auto& subsequence = entry.xaction->GetSubsequence();

        for (size_t i = 0; i < subsequence.size(); ++i)
        {
            if (entry.xaction->GetSubsequentDenial(i) != CCHI::XactDenial::ACCEPTED)
                continue;

            const auto& flit = subsequence[i];

            logger->error(
                "     time={} cycle={} [{}]{}",
                flit.time,
                flit.time / 2,
                GetResponseChannelLabel(flit),
                FormatResponseFlit(flit));
        }
    }

    const char* TransactionWatchdog::GetRequestChannelLabel(const CCHI::Xact::FiredRequestFlit<Cfg>& flit) noexcept
    {
        if (flit.IsEVT()) return "TX-EVT";
        if (flit.IsREQ()) return "TX-REQ";
        if (flit.IsSNP()) return "RX-SNP";
        return "REQ?";
    }

    const char* TransactionWatchdog::GetResponseChannelLabel(const CCHI::Xact::FiredResponseFlit<Cfg>& flit) noexcept
    {
        if (flit.IsDnRSP()) return "RX-RSP";
        if (flit.IsUpRSP()) return "TX-RSP";
        if (flit.IsDnDAT()) return "RX-DAT";
        if (flit.IsUpDAT()) return "TX-DAT";
        return "RSP?";
    }

    std::string TransactionWatchdog::FormatFirstFlit(const CCHI::Xact::FiredRequestFlit<Cfg>& flit) noexcept
    {
        if (flit.IsEVT()) return CCHIFlitFormat::FormatEVT(flit.flit.evt);
        if (flit.IsSNP()) return CCHIFlitFormat::FormatSNP(flit.flit.snp);
        if (flit.IsREQ()) return CCHIFlitFormat::FormatREQ(flit.flit.req);
        return "";
    }

    std::string TransactionWatchdog::FormatResponseFlit(const CCHI::Xact::FiredResponseFlit<Cfg>& flit) noexcept
    {
        if (flit.IsDnRSP()) return CCHIFlitFormat::FormatDnRSP(flit.flit.dnrsp);
        if (flit.IsUpRSP()) return CCHIFlitFormat::FormatUpRSP(flit.flit.uprsp);
        if (flit.IsDnDAT()) return CCHIFlitFormat::FormatDnDAT(flit.flit.dndat);
        if (flit.IsUpDAT()) return CCHIFlitFormat::FormatUpDAT(flit.flit.updat);
        return "";
    }

    std::optional<uint64_t> TransactionWatchdog::GetXactHashTag(const std::shared_ptr<CCHI::Xact::Xaction<Cfg>>& xaction) noexcept
    {
        if (!xaction)
            return std::nullopt;

        auto existing = xaction->companion.Get(CCHIFlitFormat::XactHashTag);

        if (existing)
            return *existing;

        return std::nullopt;
    }
}


// Implementation of: class TransactionWatchdog (getters and reporting)
namespace Cohestra {

    uint64_t TransactionWatchdog::GetOutstandingCount() const noexcept
    {
        return outstanding.size();
    }

    uint64_t TransactionWatchdog::GetMaxOutstandingCount() const noexcept
    {
        return maxOutstanding;
    }

    uint64_t TransactionWatchdog::GetBirthCount() const noexcept
    {
        uint64_t total = 0;

        for (uint64_t count : countBirthsByClass)
            total += count;

        return total;
    }

    uint64_t TransactionWatchdog::GetBirthCount(WatchdogXactClass xactClass) const noexcept
    {
        return countBirthsByClass[size_t(xactClass)];
    }

    uint64_t TransactionWatchdog::GetDeathCount() const noexcept
    {
        uint64_t total = 0;

        for (uint64_t count : countDeathsByClass)
            total += count;

        return total;
    }

    uint64_t TransactionWatchdog::GetDeathCount(WatchdogXactClass xactClass) const noexcept
    {
        return countDeathsByClass[size_t(xactClass)];
    }

    uint64_t TransactionWatchdog::GetLivelockTriggerCount() const noexcept
    {
        return countLivelockTriggers;
    }

    uint64_t TransactionWatchdog::GetXactTriggerCount() const noexcept
    {
        return countXactTriggers;
    }

    void TransactionWatchdog::Report() const noexcept
    {
        auto logger = instance.GetLogger();

        if (!enabled)
        {
            logger->info("Transaction watchdog report: disabled by configuration.");
            return;
        }

        logger->info("Transaction watchdog report:");
        logger->info(" - births: {} (REQ {}, EVT {}, CMO {}, prefetch {}).",
            GetBirthCount(),
            countBirthsByClass[size_t(WatchdogXactClass::REQ)],
            countBirthsByClass[size_t(WatchdogXactClass::EVT)],
            countBirthsByClass[size_t(WatchdogXactClass::CMO)],
            countBirthsByClass[size_t(WatchdogXactClass::PREFETCH)]);
        logger->info(" - retirements: {} (REQ {}, EVT {}, CMO {}, prefetch {}).",
            GetDeathCount(),
            countDeathsByClass[size_t(WatchdogXactClass::REQ)],
            countDeathsByClass[size_t(WatchdogXactClass::EVT)],
            countDeathsByClass[size_t(WatchdogXactClass::CMO)],
            countDeathsByClass[size_t(WatchdogXactClass::PREFETCH)]);
        logger->info(" - max outstanding: {} ({} still outstanding).",
            maxOutstanding, outstanding.size());

        if (!outstanding.empty() && dumpFlits)
        {
            logger->error(" - still-outstanding transaction flit timeline:");
            DumpOutstanding();
        }

        if (!countLivelockTriggers && !countXactTriggers)
        {
            logger->info(" - triggers: 0.");
            return;
        }

        logger->error(" - triggers: {} livelock (no-progress), {} deadlock (transaction timeout).",
            countLivelockTriggers, countXactTriggers);
    }
}
