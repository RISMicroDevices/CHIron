#include "event_upstream.hpp"

#include <cstdint>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../../util/cchi_util_decoding.hpp"

#include "spdlog/spdlog.h"


// Implementation of: event text decoders
namespace {

    const char* CacheStateName(CCHI::Taurus::CacheStateEnum state) noexcept
    {
        return state ? state->name : "null";
    }

    const char* XactionTypeName(CCHI::Xact::XactionType type) noexcept
    {
        switch (type)
        {
            case CCHI::Xact::XactionType::NonCacheableRead:         return "NonCacheableRead";
            case CCHI::Xact::XactionType::CacheableTransientRead:   return "CacheableTransientRead";
            case CCHI::Xact::XactionType::CacheableAllocatingRead:  return "CacheableAllocatingRead";
            case CCHI::Xact::XactionType::Evict:                    return "Evict";
            case CCHI::Xact::XactionType::WriteBack:                return "WriteBack";
            case CCHI::Xact::XactionType::CacheableDataless:        return "CacheableDataless";
            case CCHI::Xact::XactionType::CMO:                      return "CMO";
            case CCHI::Xact::XactionType::Stash:                    return "Stash";
            case CCHI::Xact::XactionType::NonCacheableWrite:        return "NonCacheableWrite";
            case CCHI::Xact::XactionType::CacheableWrite:           return "CacheableWrite";
            case CCHI::Xact::XactionType::Snoop:                    return "Snoop";
            case CCHI::Xact::XactionType::EvictRemote:              return "EvictRemote";
            default:                                                return "Unknown";
        }
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* EvtOpcodeName(const CCHI::Flits::EVT<config>& flit) noexcept
    {
        return CCHI::EVTOpcodeDecoder<CCHI::Flits::EVT<config>>::INSTANCE
            .Decode(flit.Opcode).GetName("<UnknownEVT>");
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* SnpOpcodeName(const CCHI::Flits::SNP<config>& flit) noexcept
    {
        return CCHI::SNPOpcodeDecoder<CCHI::Flits::SNP<config>>::INSTANCE
            .Decode(flit.Opcode).GetName("<UnknownSNP>");
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* ReqOpcodeName(const CCHI::Flits::REQ<config>& flit) noexcept
    {
        return CCHI::REQOpcodeDecoder<CCHI::Flits::REQ<config>>::INSTANCE
            .Decode(flit.Opcode).GetName("<UnknownREQ>");
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* DnRspOpcodeName(const CCHI::Flits::DnRSP<config>& flit) noexcept
    {
        return CCHI::DnRSPOpcodeDecoder<CCHI::Flits::DnRSP<config>>::INSTANCE
            .Decode(flit.Opcode).GetName("<UnknownDnRSP>");
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* UpRspOpcodeName(const CCHI::Flits::UpRSP<config>& flit) noexcept
    {
        return CCHI::UpRSPOpcodeDecoder<CCHI::Flits::UpRSP<config>>::INSTANCE
            .Decode(flit.Opcode).GetName("<UnknownUpRSP>");
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* DnDatOpcodeName(const CCHI::Flits::DnDAT<config>& flit) noexcept
    {
        return CCHI::DnDATOpcodeDecoder<CCHI::Flits::DnDAT<config>>::INSTANCE
            .Decode(flit.Opcode).GetName("<UnknownDnDAT>");
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* UpDatOpcodeName(const CCHI::Flits::UpDAT<config>& flit) noexcept
    {
        return CCHI::UpDATOpcodeDecoder<CCHI::Flits::UpDAT<config>>::INSTANCE
            .Decode(flit.Opcode).GetName("<UnknownUpDAT>");
    }

    CCHI::Resp RespValue(CCHI::Resp resp) noexcept
    {
        return resp;
    }

    template<class T>
    CCHI::Resp RespValue(T resp) noexcept
    {
        return CCHI::Resp(resp);
    }

    template<class T>
    const char* RespName(T resp) noexcept
    {
        return CCHI::Resps::ToEnum(RespValue(resp))->name;
    }

    template<class T>
    const char* SizeName(T size) noexcept
    {
        CCHI::Size value = size;
        return CCHI::Sizes::ToEnum(value)->name;
    }
}


// Implementation of: event dump helpers
namespace {

    template<class T>
    uint64_t ToU64(T value) noexcept
    {
        return static_cast<uint64_t>(value);
    }

    std::string Hex(uint64_t value)
    {
        std::ostringstream stream;
        stream << "0x" << std::hex << std::uppercase << value;
        return stream.str();
    }

    std::string Hex64(uint64_t value)
    {
        std::ostringstream stream;
        stream << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << value;
        return stream.str();
    }

    std::string DecHex(uint64_t value)
    {
        return std::to_string(value) + " (" + Hex(value) + ")";
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Flits::EVT<config>& flit)
    {
        std::ostringstream stream;
        stream << "EVT flit:"   << "\n"
               << "    TxnID: " << DecHex(ToU64(flit.TxnID)) << "\n"
               << "    SrcID: " << DecHex(ToU64(flit.SrcID)) << "\n"
               << "    TgtID: " << DecHex(ToU64(flit.TgtID)) << "\n"
               << "    Opcode: " << EvtOpcodeName(flit) << " " << DecHex(ToU64(flit.Opcode)) << "\n"
               << "    Addr: " << Hex(ToU64(flit.Addr)) << "\n"
               << "    NS: " << DecHex(ToU64(flit.NS)) << "\n"
               << "    MemAttr: " << DecHex(ToU64(flit.MemAttr)) << "\n"
               << "    WayValid: " << DecHex(ToU64(flit.WayValid)) << "\n"
               << "    Way: " << DecHex(ToU64(flit.Way)) << "\n"
               << "    TraceTag: " << DecHex(ToU64(flit.TraceTag));
        return stream.str();
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Flits::SNP<config>& flit)
    {
        std::ostringstream stream;
        stream << "SNP flit:"   << "\n"
               << "    TxnID: " << DecHex(ToU64(flit.TxnID)) << "\n"
               << "    SrcID: " << DecHex(ToU64(flit.SrcID)) << "\n"
               << "    TgtID: " << DecHex(ToU64(flit.TgtID)) << "\n"
               << "    Opcode: " << SnpOpcodeName(flit) << " " << DecHex(ToU64(flit.Opcode)) << "\n"
               << "    Addr: " << Hex(ToU64(flit.Addr) << 3) << " line-index " << DecHex(ToU64(flit.Addr)) << "\n"
               << "    NS: " << DecHex(ToU64(flit.NS)) << "\n"
               << "    TraceTag: " << DecHex(ToU64(flit.TraceTag));
        return stream.str();
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Flits::REQ<config>& flit)
    {
        std::ostringstream stream;
        stream << "REQ flit:"   << "\n"
               << "    TxnID: " << DecHex(ToU64(flit.TxnID)) << "\n"
               << "    SrcID: " << DecHex(ToU64(flit.SrcID)) << "\n"
               << "    TgtID: " << DecHex(ToU64(flit.TgtID)) << "\n"
               << "    Opcode: " << ReqOpcodeName(flit) << " " << DecHex(ToU64(flit.Opcode)) << "\n"
               << "    Size: " << SizeName(flit.Size) << " " << DecHex(ToU64(flit.Size)) << "\n"
               << "    Addr: " << Hex(ToU64(flit.Addr)) << "\n"
               << "    NS: " << DecHex(ToU64(flit.NS)) << "\n"
               << "    Order: " << DecHex(ToU64(flit.Order)) << "\n"
               << "    MemAttr: " << DecHex(ToU64(flit.MemAttr)) << "\n"
               << "    Excl: " << DecHex(ToU64(flit.Excl)) << "\n"
               << "    ExpCompData: " << DecHex(ToU64(flit.ExpCompData)) << "\n"
               << "    WayValid: " << DecHex(ToU64(flit.WayValid)) << "\n"
               << "    Way: " << DecHex(ToU64(flit.Way)) << "\n"
               << "    TraceTag: " << DecHex(ToU64(flit.TraceTag));
        return stream.str();
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Flits::DnRSP<config>& flit)
    {
        std::ostringstream stream;
        stream << "DnRSP flit:" << "\n"
               << "    TxnID: " << DecHex(ToU64(flit.TxnID)) << "\n"
               << "    SrcID: " << DecHex(ToU64(flit.SrcID)) << "\n"
               << "    TgtID: " << DecHex(ToU64(flit.TgtID)) << "\n"
               << "    DBID: " << DecHex(ToU64(flit.DBID)) << "\n"
               << "    Opcode: " << DnRspOpcodeName(flit) << " " << DecHex(ToU64(flit.Opcode)) << "\n"
               << "    RespErr: " << DecHex(ToU64(flit.RespErr)) << "\n"
               << "    Resp: " << RespName(flit.Resp) << " " << DecHex(ToU64(flit.Resp)) << "\n"
               << "    CBusy: " << DecHex(ToU64(flit.CBusy)) << "\n"
               << "    WayValid: " << DecHex(ToU64(flit.WayValid)) << "\n"
               << "    Way: " << DecHex(ToU64(flit.Way)) << "\n"
               << "    TraceTag: " << DecHex(ToU64(flit.TraceTag));
        return stream.str();
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Flits::UpRSP<config>& flit)
    {
        std::ostringstream stream;
        stream << "UpRSP flit:" << "\n"
               << "    TxnID: " << DecHex(ToU64(flit.TxnID)) << "\n"
               << "    SrcID: " << DecHex(ToU64(flit.SrcID)) << "\n"
               << "    TgtID: " << DecHex(ToU64(flit.TgtID)) << "\n"
               << "    Opcode: " << UpRspOpcodeName(flit) << " " << DecHex(ToU64(flit.Opcode)) << "\n"
               << "    RespErr: " << DecHex(ToU64(flit.RespErr)) << "\n"
               << "    Resp: " << RespName(flit.Resp) << " " << DecHex(ToU64(flit.Resp)) << "\n"
               << "    TraceTag: " << DecHex(ToU64(flit.TraceTag));
        return stream.str();
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Flits::DnDAT<config>& flit)
    {
        std::ostringstream stream;
        stream << "DnDAT flit:" << "\n"
               << "    TxnID: " << DecHex(ToU64(flit.TxnID)) << "\n"
               << "    SrcID: " << DecHex(ToU64(flit.SrcID)) << "\n"
               << "    TgtID: " << DecHex(ToU64(flit.TgtID)) << "\n"
               << "    DBID: " << DecHex(ToU64(flit.DBID)) << "\n"
               << "    Opcode: " << DnDatOpcodeName(flit) << " " << DecHex(ToU64(flit.Opcode)) << "\n"
               << "    RespErr: " << DecHex(ToU64(flit.RespErr)) << "\n"
               << "    Resp: " << RespName(flit.Resp) << " " << DecHex(ToU64(flit.Resp)) << "\n"
               << "    DataSource: " << DecHex(ToU64(flit.DataSource)) << "\n"
               << "    CBusy: " << DecHex(ToU64(flit.CBusy)) << "\n"
               << "    WayValid: " << DecHex(ToU64(flit.WayValid)) << "\n"
               << "    Way: " << DecHex(ToU64(flit.Way)) << "\n"
               << "    DataID: " << DecHex(ToU64(flit.DataID)) << "\n"
               << "    TraceTag: " << DecHex(ToU64(flit.TraceTag)) << "\n"
               << "    Data:";

        for (uint64_t word : flit.Data)
            stream << " " << Hex64(word);

        return stream.str();
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Flits::UpDAT<config>& flit)
    {
        std::ostringstream stream;
        stream << "UpDAT flit:" << "\n"
               << "    TxnID: " << DecHex(ToU64(flit.TxnID)) << "\n"
               << "    SrcID: " << DecHex(ToU64(flit.SrcID)) << "\n"
               << "    TgtID: " << DecHex(ToU64(flit.TgtID)) << "\n"
               << "    Opcode: " << UpDatOpcodeName(flit) << " " << DecHex(ToU64(flit.Opcode)) << "\n"
               << "    RespErr: " << DecHex(ToU64(flit.RespErr)) << "\n"
               << "    Resp: " << RespName(flit.Resp) << " " << DecHex(ToU64(flit.Resp)) << "\n"
               << "    DataID: " << DecHex(ToU64(flit.DataID)) << "\n"
               << "    BE: " << Hex(ToU64(flit.BE)) << "\n"
               << "    TraceTag: " << DecHex(ToU64(flit.TraceTag)) << "\n"
               << "    Data:";

        for (uint64_t word : flit.Data)
            stream << " " << Hex64(word);

        return stream.str();
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Xact::FiredRequestFlit<config>& firedFlit)
    {
        if (firedFlit.IsEVT())
            return DumpFlit(firedFlit.flit.evt);

        if (firedFlit.IsSNP())
            return DumpFlit(firedFlit.flit.snp);

        if (firedFlit.IsREQ())
            return DumpFlit(firedFlit.flit.req);

        return "<UnknownREQFlit>";
    }

    template<CCHI::FlitConfigurationConcept config>
    std::string DumpFlit(const CCHI::Xact::FiredResponseFlit<config>& firedFlit)
    {
        if (firedFlit.IsDnRSP())
            return DumpFlit(firedFlit.flit.dnrsp);

        if (firedFlit.IsUpRSP())
            return DumpFlit(firedFlit.flit.uprsp);

        if (firedFlit.IsDnDAT())
            return DumpFlit(firedFlit.flit.dndat);

        if (firedFlit.IsUpDAT())
            return DumpFlit(firedFlit.flit.updat);

        return "<UnknownRSPFlit>";
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* FiredChannelName(const CCHI::Xact::FiredRequestFlit<config>& firedFlit) noexcept
    {
        if (firedFlit.IsEVT())
            return firedFlit.IsTX() ? "TXEVT" : "RXEVT";

        if (firedFlit.IsSNP())
            return firedFlit.IsTX() ? "TXSNP" : "RXSNP";

        if (firedFlit.IsREQ())
            return firedFlit.IsTX() ? "TXREQ" : "RXREQ";

        return "<Unknown>";
    }

    template<CCHI::FlitConfigurationConcept config>
    const char* FiredChannelName(const CCHI::Xact::FiredResponseFlit<config>& firedFlit) noexcept
    {
        if (firedFlit.IsDnRSP())
            return firedFlit.IsTX() ? "TXDnRSP" : "RXDnRSP";

        if (firedFlit.IsUpRSP())
            return firedFlit.IsTX() ? "TXUpRSP" : "RXUpRSP";

        if (firedFlit.IsDnDAT())
            return firedFlit.IsTX() ? "TXDnDAT" : "RXDnDAT";

        if (firedFlit.IsUpDAT())
            return firedFlit.IsTX() ? "TXUpDAT" : "RXUpDAT";

        return "<Unknown>";
    }

    std::vector<std::string> BuildDeniedMessages(Cohestra::Instance& instance,
        const char* channel,
        const CCHI::Taurus::UpstreamNodeXactDeniedEventBase<Cohestra::CommonFlitConfigurationType1>& event)
    {
        const auto& upstream  = event.GetUpstream();
        const auto& cacheLine = event.GetCacheLine();

        std::ostringstream headline;
        headline << "Taurus CCHI upstream node denied " << channel << " flit: index ";

        if (auto index = instance.GetType1UpstreamIndex(&upstream))
            headline << *index;
        else
            headline << "?";

        headline << ", node ID " << DecHex(ToU64(upstream.nodeID))
                 << ", denial " << (event.GetDenial() ? event.GetDenial()->name : "null");

        std::ostringstream context;
        context << "Time: " << upstream.time
                << ", PA: " << Hex(event.GetPA())
                << ", cache state: " << CacheStateName(upstream.GetState(event.GetPA()))
                << ", in-flight: REQ=" << cacheLine.IsREQInFlight(upstream.glbl)
                << ", SNP=" << cacheLine.IsSNPInFlight(upstream.glbl)
                << ", EVT=" << cacheLine.IsEVTInFlight(upstream.glbl);

        std::ostringstream xaction;
        if (auto xact = event.GetXaction())
            xaction << "Xaction: type " << XactionTypeName(xact->GetType())
                    << ", TxnID " << DecHex(ToU64(xact->GetTxnID()))
                    << ", first denial " << (xact->GetFirstDenial() ? xact->GetFirstDenial()->name : "null");
        else
            xaction << "Xaction: none";

        return { headline.str(), context.str(), xaction.str() };
    }

    template<class EventType>
    std::vector<std::string> BuildCacheStateMapDeniedMessages(Cohestra::Instance& instance,
        const char* channel,
        EventType& event)
    {
        const auto& upstream = event.GetUpstream();

        std::ostringstream headline;
        headline << "Taurus CCHI upstream node cache-state check denied on " << channel << ": index ";

        if (auto index = instance.GetType1UpstreamIndex(&upstream))
            headline << *index;
        else
            headline << "?";

        headline << ", node ID " << DecHex(ToU64(upstream.nodeID))
                 << ", denial " << (event.GetDenial() ? event.GetDenial()->name : "null");

        std::ostringstream context;
        context << "Time: " << upstream.time
                << ", PA: " << Hex(event.GetPA())
                << ", cache state: " << CacheStateName(upstream.GetState(event.GetPA()));

        std::ostringstream xaction;
        if (auto xact = event.GetXaction())
            xaction << "Xaction: type " << XactionTypeName(xact->GetType())
                    << ", TxnID " << DecHex(ToU64(xact->GetTxnID()))
                    << ", first denial " << (xact->GetFirstDenial() ? xact->GetFirstDenial()->name : "null");
        else
            xaction << "Xaction: none";

        return { headline.str(), context.str(), xaction.str() };
    }

    [[maybe_unused]] void FailInstance(Cohestra::Instance& instance, std::vector<std::string>&& messages)
    {
        for (const auto& message : messages)
            spdlog::error("{}", message);

        std::vector<std::string> failMessage = instance.GetFailMessage();
        failMessage.insert(failMessage.end(),
            std::make_move_iterator(messages.begin()),
            std::make_move_iterator(messages.end()));

        instance.SetFailed(failMessage);
    }

    // Downgrade xact denials to warnings (temporary, per user request): log and
    // continue instead of failing the instance. The denial events are
    // observational only and do not alter flit emission or control flow.
    void WarnOnly(std::vector<std::string>&& messages)
    {
        for (const auto& message : messages)
            spdlog::warn("{}", message);
    }
}


// Implementation of: denied event handlers
namespace Cohestra {

    void OnUpstreamXactDeniedEVT(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedEVTEvent<CommonFlitConfigurationType1>& event)
    {
        auto messages = BuildDeniedMessages(instance, "EVT", event);
        messages.push_back(DumpFlit(event.GetEVTFlit()));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamXactDeniedSNP(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedSNPEvent<CommonFlitConfigurationType1>& event)
    {
        auto messages = BuildDeniedMessages(instance, "SNP", event);
        messages.push_back(DumpFlit(event.GetSNPFlit()));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamXactDeniedREQ(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedREQEvent<CommonFlitConfigurationType1>& event)
    {
        auto messages = BuildDeniedMessages(instance, "REQ", event);
        messages.push_back(DumpFlit(event.GetREQFlit()));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamXactDeniedDnRSP(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedDnRSPEvent<CommonFlitConfigurationType1>& event)
    {
        auto messages = BuildDeniedMessages(instance, "DnRSP", event);
        messages.push_back(DumpFlit(event.GetDnRSPFlit()));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamXactDeniedUpRSP(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedUpRSPEvent<CommonFlitConfigurationType1>& event)
    {
        auto messages = BuildDeniedMessages(instance, "UpRSP", event);
        messages.push_back(DumpFlit(event.GetUpRSPFlit()));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamXactDeniedDnDAT(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedDnDATEvent<CommonFlitConfigurationType1>& event)
    {
        auto messages = BuildDeniedMessages(instance, "DnDAT", event);
        messages.push_back(DumpFlit(event.GetDnDATFlit()));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamXactDeniedUpDAT(Instance& instance,
        CCHI::Taurus::UpstreamNodeXactDeniedUpDATEvent<CommonFlitConfigurationType1>& event)
    {
        auto messages = BuildDeniedMessages(instance, "UpDAT", event);
        messages.push_back(DumpFlit(event.GetUpDATFlit()));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamCacheStateMapDeniedRequest(Instance& instance,
        CCHI::Taurus::UpstreamNodeCacheStateMapDeniedRequestEvent<CommonFlitConfigurationType1>& event)
    {
        const auto& flit = event.GetFlit();
        auto messages = BuildCacheStateMapDeniedMessages(instance, FiredChannelName(flit), event);
        messages.push_back(DumpFlit(flit));
        WarnOnly(std::move(messages));
    }

    void OnUpstreamCacheStateMapDeniedResponse(Instance& instance,
        CCHI::Taurus::UpstreamNodeCacheStateMapDeniedResponseEvent<CommonFlitConfigurationType1>& event)
    {
        const auto& flit = event.GetFlit();
        auto messages = BuildCacheStateMapDeniedMessages(instance, FiredChannelName(flit), event);
        messages.push_back(DumpFlit(flit));
        WarnOnly(std::move(messages));
    }

    void RegisterUpstreamXactDeniedEventListeners(Instance& instance,
        CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>::EventHub& events)
    {
        events.OnDeniedEVT.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeXactDeniedEVTEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeXactDeniedEVTEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamXactDeniedEVT(instance, event);
                }));

        events.OnDeniedSNP.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeXactDeniedSNPEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeXactDeniedSNPEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamXactDeniedSNP(instance, event);
                }));

        events.OnDeniedREQ.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeXactDeniedREQEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeXactDeniedREQEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamXactDeniedREQ(instance, event);
                }));

        events.OnDeniedDnRSP.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeXactDeniedDnRSPEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeXactDeniedDnRSPEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamXactDeniedDnRSP(instance, event);
                }));

        events.OnDeniedUpRSP.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeXactDeniedUpRSPEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeXactDeniedUpRSPEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamXactDeniedUpRSP(instance, event);
                }));

        events.OnDeniedDnDAT.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeXactDeniedDnDATEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeXactDeniedDnDATEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamXactDeniedDnDAT(instance, event);
                }));

        events.OnDeniedUpDAT.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeXactDeniedUpDATEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeXactDeniedUpDATEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamXactDeniedUpDAT(instance, event);
                }));

        events.OnCacheStateMapDeniedRequest.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeCacheStateMapDeniedRequestEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeCacheStateMapDeniedRequestEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamCacheStateMapDeniedRequest(instance, event);
                }));

        events.OnCacheStateMapDeniedResponse.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeCacheStateMapDeniedResponseEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeCacheStateMapDeniedResponseEvent<CommonFlitConfigurationType1>& event) {
                    OnUpstreamCacheStateMapDeniedResponse(instance, event);
                }));

        // debug-level trace of snoop-induced cache-state demotions (visible with
        // [log] verbose = 1); used for coherence forensics
        events.OnSNPCacheStatePostDemotion.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeSNPCacheStatePostDemotionEvent<CommonFlitConfigurationType1>>(
                "event", 0,
                [&instance](CCHI::Taurus::UpstreamNodeSNPCacheStatePostDemotionEvent<CommonFlitConfigurationType1>& event) {
                    auto& upstream = event.GetUpstream();

                    std::ostringstream line;

                    if (auto index = instance.GetType1UpstreamIndex(&upstream))
                        line << "upstream " << *index;
                    else
                        line << "upstream ?";

                    line << ", PA " << Hex(event.GetPA())
                         << ", snoop " << SnpOpcodeName(event.GetSNPFlit())
                         << ", state " << CacheStateName(event.GetPrevState())
                         << " -> " << CacheStateName(event.GetNextState())
                         << ", time " << upstream.time;

                    instance.GetLogger()->debug("SNP demotion: {}.", line.str());
                }));

        // debug-level trace of cache-line grant events (visible with
        // [log] verbose = 1); used for coherence forensics
        events.OnCacheLineGranted.Register(
            Gravity::MakeListener<CCHI::Taurus::UpstreamNodeCacheLineGrantedEvent<CommonFlitConfigurationType1>>(
                "event.grant", 0,
                [&instance](CCHI::Taurus::UpstreamNodeCacheLineGrantedEvent<CommonFlitConfigurationType1>& event) {
                    auto& upstream = event.GetUpstream();

                    std::ostringstream line;

                    if (auto index = instance.GetType1UpstreamIndex(&upstream))
                        line << "upstream " << *index;
                    else
                        line << "upstream ?";

                    line << ", PA " << Hex(event.GetPA())
                         << ", time " << upstream.time;

                    instance.GetLogger()->debug("Line grant: {}.", line.str());
                }));
    }
}
